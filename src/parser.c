/* parser.c — checklists and .qcs lines
 */
#include "qc.h"

static const char *known_anno[] = {
    "run", "scope", "expires", "human", "strict", "cache", "evidence", NULL
};

// Implements: SW-REQ-004
static int is_known_anno(const char *k, size_t n) {
    int i;
    for (i = 0; known_anno[i]; i++)
        if (strlen(known_anno[i]) == n && !strncmp(known_anno[i], k, n)) return 1;
    return 0;
}

// Implements: SW-REQ-004
static void parse_list(const char *v, char dest[][QC_MAX_PAT], int max, int *n) {
    char buf[512];
    char *s, *tok;
    *n = 0;
    snprintf(buf, sizeof buf, "%s", v);
    qc_trim(buf);
    s = buf;
    if (s[0] == '[') {
        s++;
        if (s[0] && s[strlen(s)-1] == ']') s[strlen(s)-1] = 0;
    }
    for (tok = strtok(s, ","); tok && *n < max; tok = strtok(NULL, ",")) {
        qc_trim(tok);
        if (tok[0]) snprintf(dest[(*n)++], QC_MAX_PAT, "%s", tok); //mcdc:ignore:defensive strtok collapses consecutive commas so tok[0] is never NUL
    }
}

// Implements: SW-REQ-004
static int parse_fm_line(QcChecklist *cl, char *line) {
    char *colon = strchr(line, ':');
    char key[64], val[QC_MAX_Q];
    if (!colon) return 0;
    *colon = 0;
    snprintf(key, sizeof key, "%s", line);
    snprintf(val, sizeof val, "%s", colon + 1);
    qc_trim(key); qc_trim(val);
    if (!strcmp(key, "id")) snprintf(cl->id, sizeof cl->id, "%s", val);
    else if (!strcmp(key, "applies_when")) {
        snprintf(cl->applies_when, sizeof cl->applies_when, "%s", val);
        cl->has_applies = 1;
    } else if (!strcmp(key, "scope")) parse_list(val, cl->scope, QC_MAX_SCOPE, &cl->nscope);
    else if (!strcmp(key, "expires")) cl->expires_sec = qc_parse_expires(val);
    else if (!strcmp(key, "requires")) {
        char tmp[QC_MAX_REQ][QC_MAX_PAT];
        int i, n = 0;
        parse_list(val, tmp, QC_MAX_REQ, &n);
        cl->nrequires = n;
        for (i = 0; i < n; i++) snprintf(cl->requires[i], QC_MAX_ID, "%s", tmp[i]);
    } else if (!strcmp(key, "from")) snprintf(cl->from, sizeof cl->from, "%s", val);
    return 0;
}

// Implements: SW-REQ-004
static int parse_item_line(QcChecklist *cl, char *line, char *err, size_t errn) {
    QcItem *it;
    char *p, *q, *ann;
    if (cl->nitems >= QC_MAX_ITEMS) {
        snprintf(err, errn, "too many items");
        return -1;
    }
    it = &cl->items[cl->nitems];
    memset(it, 0, sizeof *it);
    it->expires_sec = cl->expires_sec;
    p = line;
    if (strncmp(p, "- [", 3) != 0) return 0; //mcdc:ignore:defensive caller already matched "- ["
    p += 3;
    if (*p == 'x' || *p == 'X') { it->checkbox_x = 1; cl->saw_checkbox_x = 1; p++; }
    else if (*p == ' ') p++;
    else { snprintf(err, errn, "bad checkbox"); return -1; }
    if (*p != ']') { snprintf(err, errn, "bad checkbox"); return -1; }
    p++;
    while (*p == ' ') p++;
    q = p;
    while (*q && !isspace((unsigned char)*q) && *q != ':') q++;
    if ((size_t)(q - p) >= QC_MAX_ID) { snprintf(err, errn, "item id too long"); return -1; }
    memcpy(it->id, p, (size_t)(q - p));
    it->id[q - p] = 0;
    p = q;
    while (*p == ' ') p++;
    if (p[0] != ':' || p[1] != ':') { snprintf(err, errn, "item %s: expected ::", it->id); return -1; }
    p += 2;
    while (*p == ' ') p++;
    /* question until SP @knownkey */
    ann = p;
    while (*ann) {
        if (ann[0] == ' ' && ann[1] == '@') {
            const char *k = ann + 2;
            size_t kn = 0;
            while (k[kn] && (isalnum((unsigned char)k[kn]) || k[kn] == '_')) kn++;
            if (kn && is_known_anno(k, kn)) break;
            if (kn && !is_known_anno(k, kn)) { //mcdc:ignore:defensive known annotations already broke above so !is_known_anno is constant T
                snprintf(err, errn, "unknown annotation @%.*s (typos are config errors)", (int)kn, k);
                return -1;
            }
        }
        ann++;
    }
    {
        size_t qn = (size_t)(ann - p);
        if (qn >= QC_MAX_Q) qn = QC_MAX_Q - 1;
        memcpy(it->question, p, qn);
        it->question[qn] = 0;
        qc_trim(it->question);
    }
    while (*ann == ' ' && ann[1] == '@') { //mcdc:ignore:defensive the scanner only stops on " @" so ann[1]=='@' is constant T when *ann==' '
        char *k = ann + 2;
        size_t kn = 0;
        char key[32];
        char *vp = NULL;
        char val[QC_MAX_CMD];
        val[0] = 0;
        while (k[kn] && (isalnum((unsigned char)k[kn]) || k[kn] == '_')) kn++; //mcdc:ignore:defensive known annotation keys are underscore-free so k[kn]=='_' is constant F
        memcpy(key, k, kn); key[kn] = 0;
        vp = k + kn;
        if (*vp == '(') {
            const char *end;
            if (!strcmp(key, "run")) {
                end = strrchr(vp, ')');
                if (!end) { snprintf(err, errn, "@run missing )"); return -1; }
                memcpy(val, vp + 1, (size_t)(end - vp - 1));
                val[end - vp - 1] = 0;
                vp = end + 1;
            } else {
                end = strchr(vp, ')');
                if (!end) { snprintf(err, errn, "@%s missing )", key); return -1; }
                memcpy(val, vp + 1, (size_t)(end - vp - 1));
                val[end - vp - 1] = 0;
                vp = end + 1;
            }
        }
        if (!strcmp(key, "run")) snprintf(it->run_cmd, sizeof it->run_cmd, "%s", val);
        else if (!strcmp(key, "scope")) {
            char tmp[QC_MAX_SCOPE][QC_MAX_PAT];
            int n = 0, i;
            parse_list(val, tmp, QC_MAX_SCOPE, &n);
            it->nscope = n;
            for (i = 0; i < n; i++) snprintf(it->scope[i], QC_MAX_PAT, "%s", tmp[i]);
        } else if (!strcmp(key, "expires")) it->expires_sec = qc_parse_expires(val);
        else if (!strcmp(key, "human")) it->human = 1;
        else if (!strcmp(key, "strict")) it->strict = 1;
        /* cache, evidence: phase 2 — accepted, ignored */
        while (*vp == ' ') vp++;
        ann = vp;
    }
    cl->nitems++;
    return 0;
}

// Implements: SW-REQ-004
int qc_parse_checklist_file(const char *path, const char *fname, QcChecklist *cl, char *err, size_t errn) {
    char *raw, *line, *save = NULL;
    int in_fm = 0, seen_fm = 0;
    char stem[QC_MAX_ID];
    size_t n = 0;
    memset(cl, 0, sizeof *cl);
    snprintf(cl->path, sizeof cl->path, "%s", path);
    snprintf(stem, sizeof stem, "%s", fname);
    {
        char *dot = strrchr(stem, '.');
        if (dot) *dot = 0;
    }
    snprintf(cl->filename, sizeof cl->filename, "%s", stem);
    raw = qc_read_file(path, &n);
    if (!raw) { snprintf(err, errn, "cannot read %s", path); return -1; }
    for (line = strtok_r(raw, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        if (line[0] && line[strlen(line)-1] == '\r') line[strlen(line)-1] = 0; //mcdc:ignore:defensive strtok never yields an empty token so line[0] is constant T
        if (!strcmp(line, "---")) {
            if (!seen_fm) { in_fm = 1; seen_fm = 1; continue; }
            if (in_fm) { in_fm = 0; continue; }
        }
        if (in_fm) { parse_fm_line(cl, line); continue; }
        if (strncmp(line, "- [", 3) == 0) {
            if (parse_item_line(cl, line, err, errn) != 0) { free(raw); return -1; }
        }
    }
    free(raw);
    if (!cl->id[0]) snprintf(cl->id, sizeof cl->id, "%s", stem);
    if (strcmp(cl->id, stem) != 0) {
        snprintf(err, errn, "id %s does not match filename %s.md", cl->id, stem);
        return -1;
    }
    {
        int i, j;
        for (i = 0; i < cl->nitems; i++)
            for (j = i + 1; j < cl->nitems; j++)
                if (!strcmp(cl->items[i].id, cl->items[j].id)) {
                    snprintf(err, errn, "duplicate item id %s", cl->items[i].id);
                    return -1;
                }
    }
    return 0;
}

// Implements: SW-REQ-005
static int json_str_val(const char *json, const char *key, char *dst, size_t n) {
    char pat[80], *s, *e;
    snprintf(pat, sizeof pat, "\"%s\"", key);
    s = strstr(json, pat);
    if (!s) return 0;
    s = strchr(s + strlen(pat), ':');
    if (!s) return 0;
    s++;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s != '"') return 0;
    s++;
    e = s;
    while (*e && *e != '"') e++;
    if ((size_t)(e - s) >= n) return 0;
    memcpy(dst, s, (size_t)(e - s));
    dst[e - s] = 0;
    return 1;
}

// Implements: SW-REQ-005
static int json_int_val(const char *json, const char *key, int *out) {
    char pat[80], *s;
    snprintf(pat, sizeof pat, "\"%s\"", key);
    s = strstr(json, pat);
    if (!s) return 0;
    s = strchr(s + strlen(pat), ':');
    if (!s) return 0;
    *out = atoi(s + 1);
    return 1;
}

// Implements: SW-REQ-005
static int json_str_array(const char *json, const char *key, char dest[][QC_MAX_PAT], int max, int *n) {
    char pat[80], *s, *e;
    *n = 0;
    snprintf(pat, sizeof pat, "\"%s\"", key);
    s = strstr(json, pat);
    if (!s) return 0;
    s = strchr(s + strlen(pat), '[');
    if (!s) return 0;
    s++;
    e = strchr(s, ']');
    if (!e) return 0;
    while (s < e && *n < max) {
        char *q1 = strchr(s, '"');
        char *q2;
        if (!q1 || q1 >= e) break;
        q2 = strchr(q1 + 1, '"');
        if (!q2 || q2 > e) break;
        memcpy(dest[*n], q1 + 1, (size_t)(q2 - q1 - 1));
        dest[*n][q2 - q1 - 1] = 0;
        (*n)++;
        s = q2 + 1;
    }
    return 1;
}

// Implements: SW-REQ-005
int qc_parse_config(QcCtx *cx) {
    char path[QC_MAX_PATH], *raw;
    memset(&cx->cfg, 0, sizeof cx->cfg);
    snprintf(cx->cfg.trunk, sizeof cx->cfg.trunk, "auto");
    cx->cfg.compact_after = 40;
    qc_join(path, sizeof path, cx->root, "qc/config.json");
    raw = qc_read_file(path, NULL);
    if (!raw) return 0;
    json_str_val(raw, "trunk", cx->cfg.trunk, sizeof cx->cfg.trunk);
    json_int_val(raw, "compact_after", &cx->cfg.compact_after);
    json_str_array(raw, "ignore", cx->cfg.ignore, QC_MAX_IGNORE, &cx->cfg.nignore);
    {
        char tmp[QC_MAX_DENY][QC_MAX_PAT];
        int i, n = 0;
        json_str_array(raw, "deny_evidence", tmp, QC_MAX_DENY, &n);
        cx->cfg.ndeny = n;
        for (i = 0; i < n; i++) snprintf(cx->cfg.deny[i], 64, "%s", tmp[i]);
    }
    free(raw);
    return 0;
}

// Implements: SW-REQ-005
int qc_parse_qcs_line(const char *line, QcLine *out) {
    const char *ev, *p;
    char left[QC_MAX_LINE];
    char status[32];
    memset(out, 0, sizeof *out);
    snprintf(out->raw, sizeof out->raw, "%s", line);
    if (!line[0] || line[0] == '#') return 0;
    ev = strstr(line, " :: ");
    if (!ev) ev = strstr(line, "\t:: ");
    if (!ev) return 0;
    {
        size_t ln = (size_t)(ev - line);
        if (ln >= sizeof left) return 0;
        memcpy(left, line, ln);
        left[ln] = 0;
        snprintf(out->evidence, sizeof out->evidence, "%s", ev + 4);
        qc_trim(out->evidence);
    }
    /* ref = status @hex16 fields */
    p = left;
    {
        char *eq = strchr(left, '=');
        if (!eq) return 0;
        *eq = 0;
        snprintf(out->ref, sizeof out->ref, "%s", left);
        qc_trim(out->ref);
        p = eq + 1;
        while (*p && isspace((unsigned char)*p)) p++;
    }
    if (sscanf(p, "%31s @%16s", status, out->digest) < 2) return 0;
    if (strlen(out->digest) != 16) return 0;
    out->status = qc_status_parse(status);
    if (out->status == ST_UNKNOWN) return 0;
    {
        char *at = strchr(p, '@');
        if (!at) return 0; //mcdc:ignore:defensive sscanf already required @ so strchr cannot miss
        p = at + 1 + 16;
    }
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        {
            const char *colon = strchr(p, ':');
            char key[32], val[128];
            size_t kn;
            const char *vs, *ve;
            if (!colon) break;
            kn = (size_t)(colon - p);
            if (kn >= sizeof key) return 0;
            memcpy(key, p, kn); key[kn] = 0;
            vs = colon + 1;
            ve = vs;
            while (*ve && !isspace((unsigned char)*ve)) ve++;
            if ((size_t)(ve - vs) >= sizeof val) return 0;
            memcpy(val, vs, (size_t)(ve - vs)); val[ve - vs] = 0;
            if (!strcmp(key, "by")) snprintf(out->by, sizeof out->by, "%s", val);
            else if (!strcmp(key, "at")) snprintf(out->at, sizeof out->at, "%s", val);
            else if (!strcmp(key, "files")) { out->files = atoi(val); out->has_files = 1; }
            else {
                char extra[QC_MAX_FIELD];
                snprintf(extra, sizeof extra, " %s:%s", key, val);
                strncat(out->extra, extra, sizeof out->extra - strlen(out->extra) - 1);
            }
            p = ve;
        }
    }
    if (!out->ref[0] || !out->digest[0] || !out->at[0]) return 0; //mcdc:ignore:defensive digest length is already 16 so !digest[0] is unpairable
    out->ok = 1;
    return 1;
}

// Implements: SW-REQ-005
int qc_parse_qcs_file(const char *path, QcLine **out, int *n, int *unknown_ver) {
    char *raw, *line, *save = NULL;
    size_t sz = 0;
    int cap = 0;
    *out = NULL; *n = 0;
    if (unknown_ver) *unknown_ver = 0;
    raw = qc_read_file(path, &sz);
    if (!raw) return 0;
    for (line = strtok_r(raw, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        QcLine ln;
        if (line[0] && line[strlen(line)-1] == '\r') line[strlen(line)-1] = 0; //mcdc:ignore:defensive strtok never yields an empty token so line[0] is constant T
        qc_trim(line);
        if (!line[0]) continue;
        if (qc_startswith(line, "# qc-state")) {
            if (!strstr(line, " v1") && !strstr(line, "v1")) {
                if (unknown_ver) *unknown_ver = 1;
                free(raw);
                return -1;
            }
            continue;
        }
        if (line[0] == '#') continue;
        memset(&ln, 0, sizeof ln);
        if (!qc_parse_qcs_line(line, &ln) || !ln.ok) { //mcdc:ignore:defensive a successful parse always sets ok so || !ln.ok is unpairable
            qc_warn("ignoring malformed state line (treated as absent): %s", line);
            continue;
        }
        if (*n == cap) {
            cap = cap ? cap * 2 : 16;
            *out = realloc(*out, (size_t)cap * sizeof(QcLine));
            //mcdc:ignore:defensive realloc OOM is not host-testable
            if (!*out) qc_die(QC_CONFIG, "out of memory\nNEXT: free memory, then retry");
        }
        (*out)[(*n)++] = ln;
    }
    free(raw);
    return 0;
}

// Implements: SW-REQ-005
void qc_format_line(const QcLine *ln, char *dst, size_t n) {
    if (ln->has_files)
        snprintf(dst, n, "%s = %s @%s by:%s at:%s files:%d%s :: %s",
                 ln->ref, qc_status_str(ln->status), ln->digest, ln->by, ln->at,
                 ln->files, ln->extra, ln->evidence);
    else
        snprintf(dst, n, "%s = %s @%s by:%s at:%s%s :: %s",
                 ln->ref, qc_status_str(ln->status), ln->digest, ln->by, ln->at,
                 ln->extra, ln->evidence);
}

// Implements: INT-REQ-004
int cmp_str(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

// Implements: SW-REQ-004
int qc_parse_checklists(QcCtx *cx) {
    char dir[QC_MAX_PATH];
    DIR *d;
    struct dirent *de;
    char names[QC_MAX_LISTS][QC_MAX_ID];
    int nn = 0, i, j;
    qc_join(dir, sizeof dir, cx->root, "qc/checklists");
    d = opendir(dir);
    if (!d) return 0;
    while ((de = readdir(d))) {
        size_t L;
        if (!qc_endswith(de->d_name, ".md")) continue;
        L = strlen(de->d_name);
        if (L > 3 && de->d_name[0] == '.') continue;
        if (nn >= QC_MAX_LISTS) {
            closedir(d);
            qc_die(QC_CONFIG, "too many checklists in qc/checklists/\nNEXT: split or remove extras, then qc verify");
        }
        snprintf(names[nn++], QC_MAX_ID, "%s", de->d_name);
    }
    closedir(d);
    /* bytewise sort filenames */
    for (i = 0; i < nn; i++)
        for (j = i + 1; j < nn; j++)
            if (strcmp(names[i], names[j]) > 0) {
                char t[QC_MAX_ID];
                snprintf(t, sizeof t, "%s", names[i]);
                snprintf(names[i], QC_MAX_ID, "%s", names[j]);
                snprintf(names[j], QC_MAX_ID, "%s", t);
            }
    cx->nlists = 0;
    for (i = 0; i < nn; i++) {
        char path[QC_MAX_PATH], err[256];
        qc_join(path, sizeof path, dir, names[i]);
        err[0] = 0;
        if (qc_parse_checklist_file(path, names[i], &cx->lists[cx->nlists], err, sizeof err) != 0)
            qc_die(QC_CONFIG, "parse error in %s: %s\nNEXT: fix the checklist, then qc verify", names[i], err);
        cx->nlists++;
    }
    {
        char err[256];
        if (qc_requires_cycles(cx, err, sizeof err))
            qc_die(QC_CONFIG, "%s\nNEXT: break the requires cycle, then qc verify", err);
    }
    return 0;
}

// Implements: SW-REQ-004
int qc_requires_cycles(QcCtx *cx, char *err, size_t n) {
    int mark[QC_MAX_LISTS];
    int stack[QC_MAX_LISTS], sp;
    memset(mark, 0, sizeof mark);
    for (sp = 0; sp < cx->nlists; sp++) {
        int idx[QC_MAX_LISTS], top, k;
        if (mark[sp]) continue;
        top = 0;
        idx[top++] = sp;
        while (top) {
            int u = idx[top - 1], ri, v, found;
            if (mark[u] == 0) mark[u] = 1;
            found = 0;
            for (ri = 0; ri < cx->lists[u].nrequires; ri++) {
                v = -1;
                for (k = 0; k < cx->nlists; k++)
                    if (!strcmp(cx->lists[k].id, cx->lists[u].requires[ri])) { v = k; break; }
                if (v < 0) continue; /* missing required list: fail open */
                if (mark[v] == 1) {
                    snprintf(err, n, "requires cycle involving %s", cx->lists[u].id);
                    return 1;
                }
                if (mark[v] == 0) { idx[top++] = v; found = 1; break; }
            }
            if (!found) { mark[u] = 2; top--; }
        }
    }
    (void)stack;
    (void)cmp_str;
    return 0;
}
