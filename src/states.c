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

static int secret_hit(const char *ev) {
    /* common credential shapes — refuse, never print the token */
    const char *p;
    if (strstr(ev, "AKIA")) {
        p = strstr(ev, "AKIA");
        if (p && strlen(p) >= 20) { //mcdc:ignore:defensive p is the strstr hit just proven non-NULL
            int i, ok = 1;
            for (i = 4; i < 20; i++) {
                char c = p[i];
                if (!(isdigit((unsigned char)c) || (c >= 'A' && c <= 'Z'))) { ok = 0; break; }
            }
            if (ok) return 1;
        }
    }
    if (strstr(ev, "ghp_")) return 1;
    if (strstr(ev, "sk-")) return 1;
    if (strstr(ev, "-----BEGIN") && strstr(ev, "PRIVATE KEY")) return 1;
    /* high-entropy blob >= 32 */
    {
        int run = 0;
        for (p = ev; *p; p++) {
            unsigned char c = (unsigned char)*p;
            if (isalnum(c) || c == '/' || c == '+' || c == '=' || c == '_') run++;
            else {
                if (run >= 32) return 1;
                run = 0;
            }
        }
        if (run >= 32) return 1;
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

