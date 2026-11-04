/* states.c — evaluation kernel
 */
#include "qc.h"

static const char *deny_builtin[] = {
    "done", "ok", "okay", "yes", "verified", "checked",
    "looks good", "lgtm", "fine", "n/a", "complete", "passed", NULL
};

static int looks_url(const char *s) {
    return strstr(s, "http://") || strstr(s, "https://");
}

static int has_digit(const char *s) {
    for (; *s; s++) if (isdigit((unsigned char)*s)) return 1;
    return 0;
}

static int has_backtick(const char *s) {
    return strchr(s, '`') != NULL;
}

static int has_repo_path(QcCtx *cx, const char *ev) {
    /* scan whitespace-separated tokens for a path that exists */
    char buf[QC_MAX_EV];
    char *tok, *save = NULL;
    snprintf(buf, sizeof buf, "%s", ev);
    for (tok = strtok_r(buf, " \t,;()[]{}", &save); tok; tok = strtok_r(NULL, " \t,;()[]{}", &save)) {
        char full[QC_MAX_PATH];
        size_t n = strlen(tok);
        if (n && (tok[n-1] == '.' || tok[n-1] == ',')) tok[--n] = 0; //mcdc:ignore:defensive strtok never yields an empty token so n is constant T
        if (!n || !strchr(tok, '/')) {
            /* also accept a bare filename that exists at root */
            qc_join(full, sizeof full, cx->root, tok);
            if (qc_is_file(full) || qc_is_dir(full)) return 1;
            continue;
        }
        qc_join(full, sizeof full, cx->root, tok);
        if (qc_is_file(full) || qc_is_dir(full)) return 1;
    }
    return 0;
}

/* Two-letter words and vowel-less abbreviations that appear as whole words in
 * identifiers: AddCACertificatesFromChainToPool, ConvertHeadersToHttpHeaders. */
static const char *const ident_short_words[] = {
    "to", "is", "of", "at", "by", "go", "on", "or", "in", "up", "no", "do", "be",
    "as", "if", "it", "id", "ok", "vs", "my", "we", "an", "me", "us",
    "http", "https", "grpc", "dns", "ctx", "fmt", "str", "pkg", "cmd", "src", "msg",
    "mgr", "svc", "xml", "sql", "tcp", "tls", "ssl", "jwt", "jwks", "rpc", "cfg",
    "db", "fn", "sdk", "ws", "wss", "css", "html", "js", "ts", "tmp", "dst", "ptr",
    "cnt", "tx", "rx", "kv", "ttl", "crl", "csr", "md", "mtls", "smtp", "ftp", "ssh",
    "gql", "op", "co", "py", "gz", "cpp", "ui", NULL
};

/* One word of an identifier: a listed short word, or 2+ lowercase letters with a
 * vowel, at most 4 consonants in a row and at most 20 letters (from 10 letters on,
 * a quarter of them vowels). Random letters fail these within a few words. */
// Implements: SW-REQ-006
static int ident_word_ok(const char *w, int n) {
    char low[24];
    int i, lower = 0, vowels = 0, run = 0, maxrun = 0;
    if (n > 20) return 0;
    for (i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)w[i]);
    low[n] = 0;
    for (i = 0; ident_short_words[i]; i++)
        if (!strcmp(low, ident_short_words[i])) return 1;
    for (i = 0; i < n; i++) {
        if (islower((unsigned char)w[i])) lower++;
        if (strchr("aeiouy", low[i])) { vowels++; run = 0; }
        else if (++run > maxrun) maxrun = run;
    }
    return lower >= 2 && vowels > 0 && maxrun <= 4 && (n < 10 || vowels * 4 >= n);
}

/* A long CamelCase / camelCase / path-like identifier (a Go test name, a package
 * path) is evidence, not a secret. Every word must pass ident_word_ok, acronyms
 * are at most 8 capitals (an all-caps run before a lowercase run ends at its last
 * capital: APIList = API + List; a trailing s is a plural: IDs), there are at most
 * 2 groups of digits (8 digits in all), and base64 '+' or '=' rules it out. In
 * simulation about 0.02% of random 32-char base62 strings and 0.08% of random
 * 32-letter strings still pass (none at 64), and about 1.3% of 30k real Go
 * function names and import paths are still refused. */
// Implements: SW-REQ-006
static int identifier_run(const char *s, int n) {
    int i = 0, letters = 0, digits = 0, groups = 0;
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        int j, k, w;
        if (c == '+' || c == '=') return 0;
        if (!isalpha(c)) {
            if (isdigit(c)) {
                digits++;
                if (i == 0 || !isdigit((unsigned char)s[i - 1])) groups++;
            }
            i++;
            continue;
        }
        j = i;
        while (j < n && isupper((unsigned char)s[j])) j++;
        k = j;
        while (k < n && islower((unsigned char)s[k])) k++;
        letters += k - i;
        if (k == j + 1 && j - i >= 2 && j - i <= 8 && s[j] == 's') { i = k; continue; } /* plural acronym: IDs, APIs */
        w = (k > j && j - i >= 2) ? j - 1 : (k > j ? i : j); /* where the word starts */
        if (w - i > 8) return 0;                                /* acronym too long */
        if (k > j && !ident_word_ok(s + w, k - w)) return 0;
        i = k;
    }
    return letters > 0 && digits <= 8 && groups <= 2;
}

// Implements: SW-REQ-006
static int benign_run(const char *s, int n) {
    int i, snake = 1, hex = 1, underscore = 0;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '_') underscore = 1;
        if (!(islower(c) || isdigit(c) || c == '_')) snake = 0;
        if (!(isdigit(c) || (c >= 'a' && c <= 'f'))) hex = 0;
    }
    return (snake && underscore) || hex || identifier_run(s, n);
}

/* pre followed by at least min token characters ([A-Za-z0-9_-]). With word_start
 * the prefix must not follow a letter or digit: "sk-" inside "task-42" or
 * "desk-review" is prose, while "_sk-…" (markdown emphasis) is still a key. */
// Implements: SW-REQ-006
static int prefixed_token(const char *ev, const char *pre, int min, int word_start) {
    const char *p = ev;
    size_t pl = strlen(pre);
    while ((p = strstr(p, pre)) != NULL) {
        if (!word_start || p == ev || !isalnum((unsigned char)p[-1])) {
            const char *q = p + pl;
            int k = 0;
            while (isalnum((unsigned char)q[k]) || q[k] == '_' || q[k] == '-') k++;
            if (k >= min) return 1;
        }
        p++;
    }
    return 0;
}

/* Credential families with a fixed prefix, refused before any identifier
 * exemption applies: GitHub, Slack, Stripe, OpenAI/Anthropic keys. */
static const struct { const char *pre; int min, word_start; } token_prefixes[] = {
    { "gho_", 30, 0 }, { "ghu_", 30, 0 }, { "ghs_", 30, 0 }, { "ghr_", 30, 0 },
    { "github_pat_", 30, 0 },
    { "xoxb-", 10, 0 }, { "xoxp-", 10, 0 }, { "xoxa-", 10, 0 }, { "xoxr-", 10, 0 },
    { "xoxs-", 10, 0 }, { "xoxe-", 10, 0 }, { "xapp-", 10, 0 },
    { "sk_live_", 16, 0 }, { "rk_live_", 16, 0 }, { "sk_test_", 16, 0 }, { "rk_test_", 16, 0 },
    { "sk-ant-", 20, 0 }, { "sk-proj-", 20, 0 }, { "sk-svcacct-", 20, 0 }, { "sk-admin-", 20, 0 },
    { "sk-", 20, 1 },
    { NULL, 0, 0 }
};

/* A JWT: eyJ<base64url>.eyJ<base64url>. — header and payload are base64 JSON
 * objects, so both start with eyJ. The '-' in base64url splits the blob scan. */
// Implements: SW-REQ-006
static int jwt_hit(const char *ev) {
    const char *p = ev;
    while ((p = strstr(p, "eyJ")) != NULL) {
        const char *q = p;
        int a = 0, b = 0;
        while (isalnum((unsigned char)*q) || *q == '_' || *q == '-') { q++; a++; }
        if (a >= 10 && *q == '.' && !strncmp(q + 1, "eyJ", 3)) {
            q++;
            while (isalnum((unsigned char)*q) || *q == '_' || *q == '-') { q++; b++; }
            if (b >= 10 && *q == '.') return 1;
        }
        p++;
    }
    return 0;
}

static int secret_hit(const char *ev) {
    /* common credential shapes — refuse, never print the token */
    const char *p;
    int t;
    /* every AKIA, so a short decoy before a real key id does not hide it */
    for (p = strstr(ev, "AKIA"); p; p = strstr(p + 1, "AKIA")) {
        int i, ok = 1;
        for (i = 4; i < 20; i++) { /* stops at the NUL of a short tail */
            char c = p[i];
            if (!(isdigit((unsigned char)c) || (c >= 'A' && c <= 'Z'))) { ok = 0; break; }
        }
        if (ok) return 1;
    }
    if (strstr(ev, "ghp_")) return 1;
    for (t = 0; token_prefixes[t].pre; t++)
        if (prefixed_token(ev, token_prefixes[t].pre, token_prefixes[t].min, token_prefixes[t].word_start)) return 1;
    if (jwt_hit(ev)) return 1;
    if (strstr(ev, "-----BEGIN") && strstr(ev, "PRIVATE KEY")) return 1;
    /* high-entropy blob >= 32. Lowercase snake_case identifiers, CamelCase test
     * names and package paths (identifier_run), and lowercase hex (a commit sha,
     * a sha256) are evidence, not secrets. */
    {
        int run = 0;
        const char *start = ev;
        for (p = ev; ; p++) {
            unsigned char c = (unsigned char)*p;
            if (c && (isalnum(c) || c == '/' || c == '+' || c == '=' || c == '_')) {
                if (!run) start = p;
                run++;
                continue;
            }
            if (run >= 32 && !benign_run(start, run)) return 1;
            run = 0;
            if (!c) break;
        }
    }
    return 0;
}

// Implements: STK-REQ-003
int qc_evidence_ok(QcCtx *cx, const char *ev, char *why, size_t n) {
    char low[QC_MAX_EV];
    int i;
    snprintf(low, sizeof low, "%s", ev ? ev : ""); //mcdc:ignore:defensive production never passes a NULL evidence pointer
    qc_trim(low);
    qc_lower(low);
    for (i = 0; deny_builtin[i]; i++)
        if (!strcmp(low, deny_builtin[i])) {
            snprintf(why, n, "evidence must point at something falsifiable (a path, a number, a command, a link), not restate that checking happened");
            return -1;
        }
    for (i = 0; i < cx->cfg.ndeny; i++) {
        char d[64];
        snprintf(d, sizeof d, "%s", cx->cfg.deny[i]);
        qc_lower(d);
        if (!strcmp(low, d)) {
            snprintf(why, n, "evidence must point at something falsifiable (a path, a number, a command, a link), not restate that checking happened");
            return -1;
        }
    }
    if (secret_hit(ev)) {
        snprintf(why, n, "evidence looks like a secret. The state store becomes committed text — do not paste credentials");
        return -1;
    }
    if ((int)strlen(ev) < 12 || !(has_digit(ev) || looks_url(ev) || has_backtick(ev) || has_repo_path(cx, ev))) {
        snprintf(why, n, "evidence must point at something falsifiable (a path, a number, a command, a link), not restate that checking happened");
        return -1;
    }
    return 0;
}

// Implements: SYS-REQ-006
// Implements: SW-REQ-006
QcRunStatus qc_run_cmd(QcCtx *cx, const char *cmd, int *rc, char *hint, size_t hn) {
    char full[QC_MAX_CMD + 128];
    int st;
    hint[0] = 0;
    snprintf(full, sizeof full, "cd \"%s\" && %s", cx->root, cmd);
    st = system(full);
    if (st == -1) {
        *rc = 127;
        snprintf(hint, hn, "could not spawn the platform shell. NEXT: qc verify after the environment is usable"); //mcdc:ignore:defensive system() returning -1 is not host-testable
        return -1;
    }
#ifdef WIFEXITED
    if (WIFEXITED(st)) *rc = WEXITSTATUS(st);
    else *rc = 1; //mcdc:ignore:defensive signaled wait status is host-and-shell specific
#else
    *rc = st;
#endif
    if (*rc == 127) {
        char bin[64];
        sscanf(cmd, "%63s", bin);
        snprintf(hint, hn, "binary %s not found. Install the command named in @run, then: qc verify", bin);
    }
    return 0;
}

// Implements: SW-REQ-001
QcExpired qc_expired(int expires_sec, time_t now, time_t answered_at) {
    return expires_sec > 0 && (now - answered_at) > expires_sec;
}

static int list_index(QcCtx *cx, const char *id) {
    int i;
    for (i = 0; i < cx->nlists; i++) if (!strcmp(cx->lists[i].id, id)) return i;
    return -1;
}

int qc_topo_order(QcCtx *cx, int *order, int *nord) {
    int indeg[QC_MAX_LISTS];
    int i, j, k, left;
    memset(indeg, 0, sizeof indeg);
    for (i = 0; i < cx->nlists; i++)
        for (j = 0; j < cx->lists[i].nrequires; j++) {
            k = list_index(cx, cx->lists[i].requires[j]);
            if (k >= 0) indeg[i]++;
        }
    *nord = 0;
    left = cx->nlists;
    while (left) {
        int pick = -1;
        for (i = 0; i < cx->nlists; i++) {
            int used = 0;
            for (j = 0; j < *nord; j++) if (order[j] == i) used = 1;
            if (!used && indeg[i] == 0) { pick = i; break; }
        }
        if (pick < 0) { /* cycle already checked; dump remaining in file order */
            for (i = 0; i < cx->nlists; i++) {
                int used = 0;
                for (j = 0; j < *nord; j++) if (order[j] == i) used = 1;
                if (!used) order[(*nord)++] = i;
            }
            break;
        }
        order[(*nord)++] = pick;
        left--;
        for (i = 0; i < cx->nlists; i++)
            for (j = 0; j < cx->lists[i].nrequires; j++)
                if (list_index(cx, cx->lists[i].requires[j]) == pick) indeg[i]--;
    }
    return 0;
}

static int req_blocked(QcCtx *cx, QcEval *evs, int nev, QcChecklist *cl) {
    int r, e;
    for (r = 0; r < cl->nrequires; r++) {
        for (e = 0; e < nev; e++) {
            if (strcmp(evs[e].list->id, cl->requires[r]) != 0) continue;
            if (evs[e].state == IS_CLEAR || evs[e].state == IS_DORMANT) continue;
            return 1;
        }
    }
    return 0;
}

static void free_pairs(QcPair *p, int n) {
    int i;
    if (!p) return;
    for (i = 0; i < n; i++) free(p[i].path); //mcdc:ignore:defensive self-scope returns before pairs are allocated
    free(p);
}

// Implements: SYS-REQ-001
// Implements: SYS-REQ-004
int qc_eval_all(QcCtx *cx, QcEval **out, int *n) {
    int order[QC_MAX_LISTS], nord = 0, i, oi;
    int cap = 0;
    int staged = (cx->mode == MODE_STAGED);
    *out = NULL; *n = 0;
    qc_topo_order(cx, order, &nord);
    for (oi = 0; oi < nord; oi++) {
        QcChecklist *cl = &cx->lists[order[oi]];
        int applies_forced = 0;
        if (cl->has_applies) {
            QcEval ev;
            QcPair *pairs = NULL;
            int np = 0, rc;
            memset(&ev, 0, sizeof ev);
            ev.list = cl;
            ev.item = NULL;
            ev.is_applies = 1;
            snprintf(ev.ref, sizeof ev.ref, "%s/@applies", cl->id);
            snprintf(ev.question, sizeof ev.question, "%s", cl->applies_when);
            ev.expires_sec = cl->expires_sec;
            rc = qc_digest_applies(cx, cl, &pairs, &np, ev.digest, staged);
            if (rc == -1) {
                snprintf(cx->self_scope_ref, sizeof cx->self_scope_ref, "%s", ev.ref);
                free_pairs(pairs, np);
                return -1;
            }
            ev.pairs = pairs; ev.npairs = np;
            ev.match = qc_winner_match(cx->store, cx->nstore, ev.ref, ev.digest);
            ev.echo = qc_latest_any(cx->store, cx->nstore, ev.ref);
            if (!ev.match) ev.state = ev.echo ? IS_FORCED_STALE : IS_FORCED_NEVER;
            else if (ev.match->status == ST_FAIL) ev.state = IS_FAILED;
            else if (qc_expired(ev.expires_sec, cx->now, qc_parse_iso(ev.match->at)))
                ev.state = IS_FORCED_EXPIRED;
            else ev.state = IS_CLEAR;
            if (ev.state != IS_CLEAR && ev.state != IS_FAILED) applies_forced = 1;
            if (ev.match && ev.match->status == ST_NA && ev.state == IS_CLEAR) {
                /* whole checklist suppressed — still record @applies, skip items */
                if (*n == cap) { cap = cap ? cap * 2 : 16; *out = realloc(*out, (size_t)cap * sizeof(QcEval)); } //mcdc:ignore:defensive QC_MAX_LISTS=16 cannot grow the NA-skip cap a second time
                (*out)[(*n)++] = ev;
                continue;
            }
            if (*n == cap) { cap = cap ? cap * 2 : 16; *out = realloc(*out, (size_t)cap * sizeof(QcEval)); }
            (*out)[(*n)++] = ev;
        }
        for (i = 0; i < cl->nitems; i++) {
            QcItem *it = &cl->items[i];
            QcEval ev;
            QcPair *pairs = NULL;
            int np = 0, rc;
            memset(&ev, 0, sizeof ev);
            ev.list = cl; ev.item = it;
            snprintf(ev.ref, sizeof ev.ref, "%s/%s", cl->id, it->id);
            snprintf(ev.question, sizeof ev.question, "%s", it->question);
            ev.expires_sec = it->expires_sec;
            ev.is_run = it->run_cmd[0] != 0;
            ev.is_human = it->human;
            ev.is_strict = it->strict;
            if (applies_forced) {
                ev.state = IS_AWAITING;
                if (*n == cap) { cap = cap ? cap * 2 : 16; *out = realloc(*out, (size_t)cap * sizeof(QcEval)); } //mcdc:ignore:defensive applies is always pushed first so cap is never 0 on the awaiting realloc
                (*out)[(*n)++] = ev;
                continue;
            }
            if (ev.is_run) {
                /* run items ignore scope/expires unless @cache (phase 2) */
                qc_run_cmd(cx, it->run_cmd, &ev.run_rc, ev.run_hint, sizeof ev.run_hint);
                ev.state = (ev.run_rc == 0) ? IS_CLEAR : IS_FAILED;
            } else {
                rc = qc_digest_item(cx, cl, it, &pairs, &np, ev.digest, staged);
                if (rc == -1) {
                    snprintf(cx->self_scope_ref, sizeof cx->self_scope_ref, "%s", ev.ref);
                    free_pairs(pairs, np);
                    return -1;
                }
                ev.pairs = pairs; ev.npairs = np;
                //mcdc:ignore:defensive inner nscope check is redundant with the outer guard
                if (it->nscope || cl->nscope) {
                    //mcdc:ignore:defensive inner nscope is redundant with the outer guard
                    if (np == 0 && (it->nscope || cl->nscope)) ev.state = IS_DORMANT;
                }
                if (ev.state != IS_DORMANT) {
                    ev.match = qc_winner_match(cx->store, cx->nstore, ev.ref, ev.digest);
                    ev.echo = qc_latest_any(cx->store, cx->nstore, ev.ref);
                    if (!ev.match) ev.state = ev.echo ? IS_FORCED_STALE : IS_FORCED_NEVER;
                    else if (ev.match->status == ST_FAIL) ev.state = IS_FAILED;
                    else if (qc_expired(ev.expires_sec, cx->now, qc_parse_iso(ev.match->at)))
                        ev.state = IS_FORCED_EXPIRED;
                    else ev.state = IS_CLEAR;
                }
                if (cx->mode != MODE_CI && ev.digest[0] && ev.npairs) //mcdc:ignore:defensive digest_core always writes 16 hex so digest[0] is constant T
                    qc_write_manifest(cx, ev.digest, ev.pairs, ev.npairs);
            }
            if (*n == cap) { cap = cap ? cap * 2 : 16; *out = realloc(*out, (size_t)cap * sizeof(QcEval)); }
            (*out)[(*n)++] = ev;
        }
    }
    /* requires: mark blocked */
    for (i = 0; i < *n; i++) {
        if ((*out)[i].state == IS_AWAITING || (*out)[i].state == IS_DORMANT) continue;
        if (req_blocked(cx, *out, *n, (*out)[i].list)) {
            if ((*out)[i].state == IS_CLEAR) { /* still clear but deprioritized display */ }
            (*out)[i].blocked_by_requires = 1;
            if ((*out)[i].state != IS_CLEAR && (*out)[i].state != IS_DORMANT) //mcdc:ignore:defensive DORMANT already continued above so != DORMANT is constant T
                (*out)[i].state = IS_BLOCKED;
        }
    }
    return 0;
}

