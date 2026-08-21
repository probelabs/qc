/* store.c — scratch and segments
 */
#include "qc.h"

int qc_scratch_path(QcCtx *cx, char *dst, size_t n) {
    return snprintf(dst, n, "%s/.qc/scratch.qcs", cx->root);
}

int qc_segment_name(QcCtx *cx, char *dst, size_t n) {
    char br[256], crc[8], host[64];
    char san[256];
    size_t i;
    if (!qc_git_ok(cx->root)) {
        if (gethostname(host, sizeof host) != 0) snprintf(host, sizeof host, "local");
        host[6] = 0;
        for (i = 0; host[i]; i++)
            if (!isalnum((unsigned char)host[i])) host[i] = 'x';
        return snprintf(dst, n, "seg-local-%s.qcs", host);
    }
    if (qc_git_branch(cx->root, br, sizeof br) != 0) {
        char sha[80];
        if (qc_git_head(cx->root, sha, sizeof sha) == 0) {
            sha[8] = 0;
            return snprintf(dst, n, "seg-detached-%s.qcs", sha); //mcdc:ignore:defensive git_ok-but-branch-fail with a live HEAD is not constructible
        }
        return snprintf(dst, n, "seg-local-nogit.qcs");
    }
    if (!strcmp(br, "HEAD")) {
        char sha[80];
        qc_git_head(cx->root, sha, sizeof sha);
        sha[8] = 0;
        return snprintf(dst, n, "seg-detached-%s.qcs", sha);
    }
    snprintf(san, sizeof san, "%s", br);
    for (i = 0; san[i]; i++) if (san[i] == '/') san[i] = '-';
    qc_crc6(br, crc);
    return snprintf(dst, n, "seg-%s-%s.qcs", san, crc);
}

int qc_segment_path(QcCtx *cx, char *dst, size_t n) {
    char name[256];
    qc_segment_name(cx, name, sizeof name);
    return snprintf(dst, n, "%s/qc/state/%s", cx->root, name);
}

int qc_load_file_lines(const char *path, QcLine **acc, int *n) {
    QcLine *add = NULL;
    int na = 0, ver = 0, i;
    if (!qc_is_file(path)) return 0;
    //mcdc:ignore:defensive parse<0 only happens with unknown_ver set, so && ver is unpairable
    if (qc_parse_qcs_file(path, &add, &na, &ver) < 0 && ver)
        qc_die(QC_CONFIG, "unknown # qc-state version in %s\nNEXT: use a v1 state file, or qc reset if this is scratch", path);
    if (!add) return 0;
    *acc = realloc(*acc, (size_t)(*n + na) * sizeof(QcLine));
    //mcdc:ignore:defensive realloc OOM is not host-testable
    if (!*acc) qc_die(QC_CONFIG, "out of memory\nNEXT: free memory, then retry");
    for (i = 0; i < na; i++) (*acc)[(*n)++] = add[i];
    free(add);
    return 0;
}

// Implements: INT-REQ-001
int qc_load_store(QcCtx *cx) {
    char path[QC_MAX_PATH], dir[QC_MAX_PATH];
    DIR *d;
    struct dirent *de;
    cx->store = NULL;
    cx->nstore = 0;
    if (cx->mode == MODE_LOOP) {
        qc_scratch_path(cx, path, sizeof path);
        qc_load_file_lines(path, &cx->store, &cx->nstore);
    }
    /* committed base + segments for all modes */
    snprintf(path, sizeof path, "%s/qc/state/base.qcs", cx->root);
    qc_load_file_lines(path, &cx->store, &cx->nstore);
    snprintf(dir, sizeof dir, "%s/qc/state", cx->root);
    d = opendir(dir);
    if (d) {
        while ((de = readdir(d))) {
            if (!qc_startswith(de->d_name, "seg-") || !qc_endswith(de->d_name, ".qcs")) continue;
            snprintf(path, sizeof path, "%s/%s", dir, de->d_name);
            qc_load_file_lines(path, &cx->store, &cx->nstore);
        }
        closedir(d);
    }
    return 0;
}

int qc_read_scratch(QcCtx *cx, QcLine **out, int *n) {
    char path[QC_MAX_PATH];
    int ver = 0;
    qc_scratch_path(cx, path, sizeof path);
    *out = NULL; *n = 0;
    if (!qc_is_file(path)) return 0;
    //mcdc:ignore:defensive parse<0 only happens with unknown_ver set, so && ver is unpairable
    if (qc_parse_qcs_file(path, out, n, &ver) < 0 && ver)
        qc_die(QC_CONFIG, "unknown # qc-state version in scratch\nNEXT: qc reset");
    return 0;
}

void qc_upsert_scratch(QcLine **lines, int *n, const QcLine *one) {
    int i;
    for (i = 0; i < *n; i++) {
        if (!strcmp((*lines)[i].ref, one->ref)) { (*lines)[i] = *one; return; }
    }
    *lines = realloc(*lines, (size_t)(*n + 1) * sizeof(QcLine));
    //mcdc:ignore:defensive realloc OOM is not host-testable
    if (!*lines) qc_die(QC_CONFIG, "out of memory\nNEXT: free memory, then retry");
    (*lines)[(*n)++] = *one;
}

static void write_qcs(const char *path, QcLine *lines, int n) {
    FILE *f;
    int i;
    char dir[QC_MAX_PATH], *sl;
    snprintf(dir, sizeof dir, "%s", path);
    sl = strrchr(dir, '/');
    if (sl) { *sl = 0; qc_mkdir_p(dir); } //mcdc:ignore:defensive scratch/segment paths always contain a slash
    f = fopen(path, "w");
    //mcdc:ignore:defensive scratch fopen failure is permissions-racy
    if (!f) qc_die(QC_CONFIG, "cannot write %s\nNEXT: check permissions, then retry", path);
    fputs("# qc-state v1\n", f);
    for (i = 0; i < n; i++) {
        char buf[QC_MAX_LINE];
        if (!lines[i].ok) continue;
        qc_format_line(&lines[i], buf, sizeof buf);
        fprintf(f, "%s\n", buf);
    }
    fclose(f);
}

// Implements: SW-REQ-007
// Implements: INT-REQ-002
int qc_write_scratch(QcCtx *cx, QcLine *lines, int n) {
    char path[QC_MAX_PATH];
    qc_scratch_path(cx, path, sizeof path);
    {
        char d[QC_MAX_PATH];
        snprintf(d, sizeof d, "%s/.qc", cx->root);
        qc_mkdir_p(d);
    }
    write_qcs(path, lines, n);
    return 0;
}

// Implements: SW-REQ-007
int qc_append_segment(QcCtx *cx, QcLine *lines, int n) {
    char path[QC_MAX_PATH];
    FILE *f;
    int i, exists;
    qc_segment_path(cx, path, sizeof path);
    {
        char d[QC_MAX_PATH];
        snprintf(d, sizeof d, "%s/qc/state", cx->root);
        qc_mkdir_p(d);
    }
    exists = qc_is_file(path);
    f = fopen(path, exists ? "a" : "w");
    //mcdc:ignore:defensive segment fopen failure is permissions-racy
    if (!f) qc_die(QC_CONFIG, "cannot write segment %s\nNEXT: check permissions, then qc seal", path);
    if (!exists) fputs("# qc-state v1\n", f);
    for (i = 0; i < n; i++) {
        char buf[QC_MAX_LINE];
        if (!lines[i].ok) continue;
        qc_format_line(&lines[i], buf, sizeof buf);
        fprintf(f, "%s\n", buf);
    }
    fclose(f);
    return 0;
}

static int newer_line(const QcLine *a, const QcLine *b) {
    int c = strcmp(a->at, b->at);
    if (c) return c;
    {
        char ha[65], hb[65];
        qc_sha256_hex(a->raw, strlen(a->raw), ha);
        qc_sha256_hex(b->raw, strlen(b->raw), hb);
        c = strcmp(ha, hb);
        if (c) return c;
    }
    return qc_status_rank(a->status) - qc_status_rank(b->status);
}

// Implements: SYS-REQ-001
QcLine * qc_winner_match(QcLine *lines, int n, const char *ref, const char *digest) {
    QcLine *w = NULL;
    int i;
    for (i = 0; i < n; i++) {
        if (!lines[i].ok) continue;
        if (strcmp(lines[i].ref, ref) != 0) continue;
        if (strcmp(lines[i].digest, digest) != 0) continue;
        if (!w || newer_line(&lines[i], w) > 0) w = &lines[i];
    }
    return w;
}

QcLine *qc_latest_any(QcLine *lines, int n, const char *ref) {
    QcLine *w = NULL;
    int i;
    for (i = 0; i < n; i++) {
        if (!lines[i].ok) continue;
        if (strcmp(lines[i].ref, ref) != 0) continue;
        if (!w || strcmp(lines[i].at, w->at) > 0) w = &lines[i];
    }
    return w;
}
