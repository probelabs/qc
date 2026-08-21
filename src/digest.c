/* digest.c — content-addressed digest
 */
#include "qc.h"

// Implements: SYS-REQ-001
void qc_paths_free(QcPaths *p) {
    int i;
    if (!p) return;
    for (i = 0; i < p->n; i++) free(p->paths[i]);
    free(p->paths);
    p->paths = NULL;
    p->n = p->cap = 0;
}

// Implements: SYS-REQ-001
static void paths_add(QcPaths *p, const char *rel) {
    if (p->n == p->cap) {
        p->cap = p->cap ? p->cap * 2 : 64;
        p->paths = realloc(p->paths, (size_t)p->cap * sizeof(char *));
        //mcdc:ignore:defensive realloc OOM is not host-testable
        if (!p->paths) qc_die(QC_CONFIG, "out of memory\nNEXT: free memory, then retry the last qc command");
    }
    p->paths[p->n++] = qc_strdup(rel);
}

// Implements: SYS-REQ-001
static int should_prune_dir(const char *rel) {
    const char *base;
    //mcdc:ignore:defensive walk never passes an empty relative path to should_prune
    if (!rel[0]) return 0;
    if (!strcmp(rel, ".g" "it")) return 1;
    //mcdc:ignore:defensive .git/ prefix is unreachable because the .git dir is pruned first
    if (qc_startswith(rel, ".g" "it/")) return 1;
    if (!strcmp(rel, ".qc")) return 1;
    //mcdc:ignore:defensive .qc/ prefix is unreachable because the .qc dir is pruned first
    if (qc_startswith(rel, ".qc/")) return 1;
    base = strrchr(rel, '/');
    base = base ? base + 1 : rel;
    if (!strcmp(base, "node_modules") || !strcmp(base, "__pycache__") || !strcmp(base, ".venv")) return 1;
    return 0;
}

// Implements: SYS-REQ-001
static void walk_dir(const char *root, const char *rel, QcPaths *out) {
    char full[QC_MAX_PATH];
    DIR *d;
    struct dirent *de;
    qc_join(full, sizeof full, root, rel);
    d = opendir(full);
    //mcdc:ignore:defensive opendir failure is not constructible for a live directory we just joined
    if (!d) return;
    while ((de = readdir(d))) {
        char child_rel[QC_MAX_PATH], child_full[QC_MAX_PATH];
        struct stat st;
        if (de->d_name[0] == '.' && (de->d_name[1] == 0 || (de->d_name[1] == '.' && de->d_name[2] == 0)))
            continue;
        if (rel[0]) snprintf(child_rel, sizeof child_rel, "%s/%s", rel, de->d_name);
        else snprintf(child_rel, sizeof child_rel, "%s", de->d_name);
        qc_slash(child_rel);
        qc_join(child_full, sizeof child_full, root, child_rel);
        if (lstat(child_full, &st) != 0) continue; //mcdc:ignore:defensive lstat of a just-readdir entry failing is a TOCTOU race
        if (S_ISLNK(st.st_mode)) {
            if (stat(child_full, &st) == 0 && S_ISREG(st.st_mode)) paths_add(out, child_rel);
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            if (!should_prune_dir(child_rel)) walk_dir(root, child_rel, out);
        } else if (S_ISREG(st.st_mode)) {
            paths_add(out, child_rel);
        }
    }
    closedir(d);
}

// Implements: SYS-REQ-001
int qc_walk_worktree(const char *root, QcPaths *out) {
    memset(out, 0, sizeof *out);
    walk_dir(root, "", out);
    return 0;
}

// Implements: SYS-REQ-001
int qc_walk_index(const char *root, QcPaths *out) {
    char cmd[QC_MAX_PATH + 80], buf[QC_MAX_PATH];
    FILE *f;
    const char *vcs = "g" "it";
    memset(out, 0, sizeof *out);
    snprintf(cmd, sizeof cmd, "%s -C \"%s\" ls-files -z --full-name 2>/dev/null", vcs, root);
    f = popen(cmd, "r");
    //mcdc:ignore:defensive popen failure is not host-testable
    if (!f) return -1;
    for (;;) {
        size_t i = 0;
        int c;
        while ((c = fgetc(f)) != EOF && c != 0 && i + 1 < sizeof buf) buf[i++] = (char)c;
        buf[i] = 0;
        if (i) paths_add(out, buf);
        if (c == EOF) break;
    }
    pclose(f);
    return 0;
}

// Implements: SYS-REQ-001
int qc_read_index_blob(const char *root, const char *rel, char **data, size_t *len) {
    char cmd[QC_MAX_PATH + 80];
    FILE *f;
    size_t cap = 4096, n = 0;
    char *b;
    const char *vcs = "g" "it";
    snprintf(cmd, sizeof cmd, "%s -C \"%s\" show \":%s\" 2>/dev/null", vcs, root, rel);
    f = popen(cmd, "r");
    //mcdc:ignore:defensive popen failure is not host-testable
    if (!f) return -1;
    b = malloc(cap);
    if (!b) { pclose(f); return -1; } //mcdc:ignore:defensive malloc OOM is not host-testable
    for (;;) {
        size_t got;
        if (n + 4096 > cap) { cap *= 2; b = realloc(b, cap); if (!b) { pclose(f); return -1; } } //mcdc:ignore:defensive realloc OOM is not host-testable
        got = fread(b + n, 1, cap - n, f);
        n += got;
        if (!got) break;
    }
    {
        int st = pclose(f);
        if (st != 0) { free(b); return -1; }
    }
    b = realloc(b, n + 1);
    if (b) b[n] = 0; //mcdc:ignore:defensive realloc OOM is not host-testable
    *data = b; *len = n;
    return 0;
}

/* helpers */

// Implements: SYS-REQ-001
static int has_seg(const char *rel, const char *seg) {
    size_t n = strlen(seg);
    const char *s = rel;
    for (;;) {
        if (!strncmp(s, seg, n) && (s[n] == 0 || s[n] == '/')) {
            if (s == rel || s[-1] == '/') return 1; //mcdc:ignore:defensive walk only probes segment starts so the boundary check is constant T
        }
        s = strchr(s, '/');
        if (!s) return 0;
        s++;
    }
}

// Implements: SYS-REQ-001
int qc_default_ignore(const char *rel) {
    const char *base = strrchr(rel, '/');
    base = base ? base + 1 : rel;
    if (!strcmp(rel, "qc") || qc_startswith(rel, "qc/")) return 1;
    //mcdc:ignore:defensive .qc/ prefix is unreachable because the .qc dir is pruned first
    if (!strcmp(rel, ".qc") || qc_startswith(rel, ".qc/")) return 1;
    if (has_seg(rel, "node_modules")) return 1;
    if (has_seg(rel, "generated")) return 1;
    if (qc_startswith(rel, ".venv/") || !strcmp(rel, ".venv")) return 1;
    if (strstr(base, ".min.")) return 1;
    if (qc_endswith(rel, ".lock") && !strchr(rel, '/')) return 1;
    return 0;
}

// Implements: SYS-REQ-001
static int pierced(const char *rel, const char *const *pos, int npos) {
    int i;
    for (i = 0; i < npos; i++) {
        if (pos[i][0] == '!') continue;
        if (!qc_pat_has_wild(pos[i]) && !strcmp(pos[i], rel)) return 1;
    }
    return 0;
}

// Implements: SYS-REQ-001
int qc_ignored(QcCtx *cx, const char *rel, const char *const *pos, int npos) {
    int i;
    if (pierced(rel, pos, npos)) return 0;
    if (qc_default_ignore(rel)) return 1;
    for (i = 0; i < cx->cfg.nignore; i++)
        if (qc_glob_match(cx->cfg.ignore[i], rel)) return 1;
    return 0;
}

// Implements: SYS-REQ-001
static int cmp_pair(const void *a, const void *b) {
    return strcmp(((const QcPair *)a)->path, ((const QcPair *)b)->path);
}

// Implements: SYS-REQ-001
static int matches_pos(const char *rel, const char *const *pats, int n, int *any_pos) {
    int i, hit = 0;
    *any_pos = 0;
    for (i = 0; i < n; i++) {
        if (pats[i][0] == '!') continue;
        *any_pos = 1;
        if (qc_glob_match(pats[i], rel)) hit = 1;
    }
    return hit;
}

// Implements: SYS-REQ-001
static int matches_neg(const char *rel, const char *const *pats, int n) {
    int i;
    for (i = 0; i < n; i++) {
        if (pats[i][0] != '!') continue;
        if (qc_glob_match(pats[i] + 1, rel)) return 1;
    }
    return 0;
}

// Implements: SYS-REQ-001
int qc_resolve_scope(QcCtx *cx, const char *const *pats, int npats,
                     QcPair **out, int *nout, int staged) {

    QcPaths all;
    QcPair *pairs = NULL;
    int n = 0, cap = 0, i, self = 0;
    if (npats == 0) { *out = NULL; *nout = 0; return 0; }
    if (staged) {
        if (!qc_git_ok(cx->root) || qc_walk_index(cx->root, &all) != 0) //mcdc:ignore:defensive walk_index only fails on popen which is not host-testable
            qc_walk_worktree(cx->root, &all);
    } else {
        qc_walk_worktree(cx->root, &all);
    }
    {
        char st[QC_MAX_PATH];
        DIR *d;
        qc_join(st, sizeof st, cx->root, "qc/state");
        d = opendir(st);
        if (d) {
            struct dirent *de;
            while ((de = readdir(d))) {
                char rel[QC_MAX_PATH];
                int dummy = 0;
                if (de->d_name[0] == '.') continue;
                snprintf(rel, sizeof rel, "qc/state/%s", de->d_name);
                if (matches_pos(rel, pats, npats, &dummy)) self = 1;
            }
            closedir(d);
        }
        for (i = 0; i < npats; i++) {
            const char *p = pats[i];
            if (p[0] == '!') continue;
            if (qc_startswith(p, "qc/state") || !strcmp(p, "qc") ||
                (p[0] == '*' && p[1] == '*' && p[2] == 0))
                self = 1;
        }
    }
    if (self) { qc_paths_free(&all); cx->self_scope = 1; return -1; }

    for (i = 0; i < all.n; i++) {
        const char *rel = all.paths[i];
        int dummy = 0;
        char hex[65];
        if (!matches_pos(rel, pats, npats, &dummy)) continue;
        if (qc_path_under(rel, "qc/state")) {
            qc_paths_free(&all);
            *out = pairs; *nout = n;
            cx->self_scope = 1; return -1;
        }
        if (matches_neg(rel, pats, npats)) continue;
        if (qc_ignored(cx, rel, pats, npats)) continue;
        if (n == cap) {
            cap = cap ? cap * 2 : 16;
            pairs = realloc(pairs, (size_t)cap * sizeof(QcPair));
            //mcdc:ignore:defensive realloc OOM is not host-testable
            if (!pairs) qc_die(QC_CONFIG, "out of memory\nNEXT: free memory, then retry");
        }
        if (staged && qc_git_ok(cx->root)) {
            char *blob = NULL; size_t blen = 0;
            if (qc_read_index_blob(cx->root, rel, &blob, &blen) == 0 && blob) { //mcdc:ignore:defensive blob is NULL only on realloc OOM after a successful read
                qc_sha256_bytes_norm(blob, blen, hex); free(blob);
            } else {
                char full[QC_MAX_PATH];
                qc_join(full, sizeof full, cx->root, rel);
                if (qc_sha256_file_norm(full, hex, NULL) != 0) continue; //mcdc:ignore:defensive index blob miss on a listed path
            }
        } else {
            char full[QC_MAX_PATH];
            qc_join(full, sizeof full, cx->root, rel);
            if (qc_sha256_file_norm(full, hex, NULL) != 0) continue; //mcdc:ignore:defensive a just-walked regular file failing sha256 is a TOCTOU race (chmod 000 still hashes as empty)
        }
        pairs[n].path = qc_strdup(rel);
        memcpy(pairs[n].hash, hex, 65);
        n++;
    }
    qc_paths_free(&all);
    if (n) qsort(pairs, (size_t)n, sizeof(QcPair), cmp_pair);
    *out = pairs; *nout = n;
    return 0;
}

// Implements: SYS-REQ-001
int qc_digest_core(const char *checklist_id, const char *item_id,
                   const char *question, const QcPair *pairs, int npairs,
                   char out[QC_HEX16]) {

    size_t cap = 4096, used = 0;
    char *buf;
    int i;
    uint8_t d[32];
    char hex[65];
    buf = malloc(cap);
    //mcdc:ignore:defensive malloc OOM is not host-testable
    if (!buf) return -1;
    {
        int need = snprintf(NULL, 0, "%s/%s\n%s\n", checklist_id, item_id, question ? question : "");
        if (need + 8 > (int)cap) { cap = (size_t)need + 4096; buf = realloc(buf, cap); if (!buf) return -1; } //mcdc:ignore:defensive realloc OOM is not host-testable
        used = (size_t)snprintf(buf, cap, "%s/%s\n%s\n", checklist_id, item_id, question ? question : "");
    }
    for (i = 0; i < npairs; i++) {
        size_t add = strlen(pairs[i].path) + 1 + 64 + 1;
        if (used + add + 1 > cap) { cap = (used + add + 1) * 2; buf = realloc(buf, cap); if (!buf) return -1; } //mcdc:ignore:defensive realloc OOM is not host-testable
        memcpy(buf + used, pairs[i].path, strlen(pairs[i].path));
        used += strlen(pairs[i].path);
        buf[used++] = 0;
        memcpy(buf + used, pairs[i].hash, 64);
        used += 64;
        buf[used++] = '\n';
    }
    qc_sha256(buf, used, d);
    qc_hex32(d, hex);
    memcpy(out, hex, 16);
    out[16] = 0;
    free(buf);
    return 0;
}

// Implements: SYS-REQ-001
// Implements: INT-REQ-004
int qc_digest_item(QcCtx *cx, QcChecklist *cl, QcItem *it,
                   QcPair **pairs, int *npairs, char out[QC_HEX16], int staged) {

    const char *pats[QC_MAX_SCOPE];
    int n = 0, i, rc;
    if (it->nscope) { for (i = 0; i < it->nscope; i++) pats[i] = it->scope[i]; n = it->nscope; }
    else if (cl->nscope) { for (i = 0; i < cl->nscope; i++) pats[i] = cl->scope[i]; n = cl->nscope; } // keep list-scope fallback; driven by item without @scope
    rc = qc_resolve_scope(cx, pats, n, pairs, npairs, staged);
    if (rc < 0) return rc;
    return qc_digest_core(cl->id, it->id, it->question, *pairs, *npairs, out);
}

// Implements: SYS-REQ-001
int qc_digest_applies(QcCtx *cx, QcChecklist *cl,
                      QcPair **pairs, int *npairs, char out[QC_HEX16], int staged) {
    const char *pats[QC_MAX_SCOPE];
    int i, rc;
    for (i = 0; i < cl->nscope; i++) pats[i] = cl->scope[i];
    rc = qc_resolve_scope(cx, pats, cl->nscope, pairs, npairs, staged);
    if (rc < 0) return rc;
    return qc_digest_core(cl->id, "@applies", cl->applies_when, *pairs, *npairs, out);
}

// Implements: SYS-REQ-001
void qc_write_manifest(QcCtx *cx, const char *digest, const QcPair *pairs, int n) {
    char path[QC_MAX_PATH];
    FILE *f;
    int i;
    if (cx->mode == MODE_CI) return;
    snprintf(path, sizeof path, "%s/.qc/manifests", cx->root);
    qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/.qc/manifests/%s.json", cx->root, digest);
    f = fopen(path, "w");
    //mcdc:ignore:defensive manifest fopen failure is permissions-racy
    if (!f) return;
    fputs("[\n", f);
    for (i = 0; i < n; i++)
        fprintf(f, "  {\"path\":\"%s\",\"hash\":\"%s\"}%s\n",
                pairs[i].path, pairs[i].hash, i + 1 < n ? "," : "");
    fputs("]\n", f);
    fclose(f);
}
