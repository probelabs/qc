/* util.c — paths, sha256, process helpers
 */
#include "qc.h"

// Implements: SW-REQ-002
void qc_die(int code, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    if (fmt[0] && fmt[strlen(fmt) - 1] != '\n') fputc('\n', stderr);
    exit(code);
}

// Implements: SW-REQ-002
void qc_warn(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fputs("warning: ", stderr);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    if (fmt[0] && fmt[strlen(fmt) - 1] != '\n') fputc('\n', stderr);
}

// Implements: SW-REQ-002
void qc_out(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
}

// Implements: SW-REQ-005
char *qc_strdup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *d = malloc(n + 1);
    //mcdc:ignore:defensive malloc OOM is not host-testable
    if (!d) qc_die(QC_CONFIG, "out of memory\nNEXT: free memory, then retry the last qc command");
    if (s) memcpy(d, s, n);
    d[n] = 0;
    return d;
}

// Implements: SW-REQ-005
void qc_trim(char *s) {
    char *a = s, *b;
    if (!s) return;
    while (*a && isspace((unsigned char)*a)) a++;
    if (a != s) memmove(s, a, strlen(a) + 1);
    b = s + strlen(s);
    while (b > s && isspace((unsigned char)b[-1])) *--b = 0;
}

// Implements: SW-REQ-005
void qc_lower(char *s) {
    for (; s && *s; s++) *s = (char)tolower((unsigned char)*s);
}

// Implements: INT-REQ-004
int qc_startswith(const char *s, const char *pfx) {
    size_t n = strlen(pfx);
    return s && strncmp(s, pfx, n) == 0;
}

int qc_endswith(const char *s, const char *sfx) {
    size_t n = strlen(s), m = strlen(sfx);
    return n >= m && memcmp(s + n - m, sfx, m) == 0;
}

// Implements: INT-REQ-004
void qc_slash(char *p) {
    for (; p && *p; p++) if (*p == '\\') *p = '/';
}

int qc_path_under(const char *path, const char *dir) {
    size_t n = strlen(dir);
    if (strcmp(path, dir) == 0) return 1;
    if (strncmp(path, dir, n) == 0 && (dir[n - 1] == '/' || path[n] == '/')) return 1;
    return 0;
}

// Implements: SW-REQ-005
void qc_join(char *dst, size_t n, const char *a, const char *b) {
    if (!b || !b[0]) { snprintf(dst, n, "%s", a ? a : ""); return; }
    if (!a || !a[0]) { snprintf(dst, n, "%s", b); return; }
    size_t la = strlen(a);
    if (a[la - 1] == '/') snprintf(dst, n, "%s%s", a, b);
    else snprintf(dst, n, "%s/%s", a, b);
}

// Implements: SW-REQ-005
int qc_is_dir(const char *p) {
    struct stat st;
    return p && stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

int qc_is_file(const char *p) {
    struct stat st;
    return p && stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

// Implements: SW-REQ-005
int qc_mkdir_p(const char *path) {
    char buf[QC_MAX_PATH];
    size_t i, n;
    if (!path || !path[0]) return -1;
    snprintf(buf, sizeof buf, "%s", path);
    n = strlen(buf);
    for (i = 1; i < n; i++) {
        if (buf[i] == '/') {
            buf[i] = 0;
            if (!qc_is_dir(buf) && mkdir(buf, 0755) != 0 && errno != EEXIST) return -1;
            buf[i] = '/';
        }
    }
    if (!qc_is_dir(buf) && mkdir(buf, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

// Implements: SW-REQ-005
int qc_write_file(const char *path, const char *data) {
    FILE *f;
    char dir[QC_MAX_PATH];
    char *sl;
    snprintf(dir, sizeof dir, "%s", path);
    sl = strrchr(dir, '/');
    if (sl) { *sl = 0; qc_mkdir_p(dir); }
    f = fopen(path, "wb");
    //mcdc:ignore:defensive popen/fopen failure is not host-testable
    if (!f) return -1;
    if (data && data[0]) fwrite(data, 1, strlen(data), f);
    fclose(f);
    return 0;
}

// Implements: SW-REQ-005
char *qc_read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    char *b;
    long n;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; } //mcdc:ignore:defensive fseek on a regular file is not host-testable
    n = ftell(f);
    if (n < 0) { fclose(f); return NULL; } //mcdc:ignore:defensive ftell on a regular file is not host-testable
    rewind(f);
    b = malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; } //mcdc:ignore:defensive malloc OOM is not host-testable
    if (n && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; } //mcdc:ignore:defensive short fread on a regular file is not host-testable
    b[n] = 0;
    fclose(f);
    if (out_len) *out_len = (size_t)n;
    return b;
}

// Implements: SW-REQ-003
int qc_copy_file(const char *src, const char *dst) {
    FILE *in, *out;
    char buf[8192];
    size_t n;
    char dir[QC_MAX_PATH], *sl;
    snprintf(dir, sizeof dir, "%s", dst);
    sl = strrchr(dir, '/');
    if (sl) { *sl = 0; qc_mkdir_p(dir); }
    in = fopen(src, "rb");
    if (!in) return -1;
    out = fopen(dst, "wb");
    if (!out) { fclose(in); return -1; } //mcdc:ignore:defensive fopen dest failure is not host-testable
    while ((n = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, n, out);
    fclose(in);
    fclose(out);
    chmod(dst, 0755);
    return 0;
}

// Implements: SYS-REQ-001
void qc_now_iso(char *dst, size_t n) {
    time_t t = time(NULL);
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(dst, n, "%Y-%m-%dT%H:%M:%SZ", &tm);
}

// Implements: SYS-REQ-001
time_t qc_parse_iso(const char *s) {
    struct tm tm;
    int Y, M, D, h, m, sec;
    if (!s || sscanf(s, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &sec) < 6) return 0;
    memset(&tm, 0, sizeof tm);
    tm.tm_year = Y - 1900;
    tm.tm_mon = M - 1;
    tm.tm_mday = D;
    tm.tm_hour = h;
    tm.tm_min = m;
    tm.tm_sec = sec;
#if defined(_DEFAULT_SOURCE) || defined(_BSD_SOURCE) || defined(__APPLE__) || defined(__COSMOPOLITAN__)
    return timegm(&tm);
#else
    return timegm(&tm);
#endif
}

char *qc_relpath(const char *root, const char *abs, char *dst, size_t n) {
    size_t r = strlen(root);
    const char *p = abs;
    if (strncmp(abs, root, r) == 0 && (abs[r] == '/' || abs[r] == 0)) {
        p = abs + r;
        if (*p == '/') p++;
    }
    snprintf(dst, n, "%s", p);
    qc_slash(dst);
    return dst;
}

// Implements: SW-REQ-003
int qc_git_toplevel(char *dst, size_t n) {
    FILE *f = popen("git rev-parse --show-toplevel 2>/dev/null", "r");
    //mcdc:ignore:defensive popen/fopen failure is not host-testable
    if (!f) return -1;
    if (!fgets(dst, (int)n, f)) { pclose(f); return -1; }
    pclose(f);
    qc_trim(dst);
    return dst[0] ? 0 : -1;
}

// Implements: SW-REQ-003
int qc_find_root(char *dst, size_t n) {
    char cwd[QC_MAX_PATH], walk[QC_MAX_PATH], cand[QC_MAX_PATH];
    if (!getcwd(cwd, sizeof cwd)) return -1;
    qc_slash(cwd);
    snprintf(walk, sizeof walk, "%s", cwd);
    for (;;) {
        qc_join(cand, sizeof cand, walk, "qc/config.json");
        if (qc_is_file(cand)) { snprintf(dst, n, "%s", walk); return 0; }
        qc_join(cand, sizeof cand, walk, "qc/checklists");
        if (qc_is_dir(cand)) { snprintf(dst, n, "%s", walk); return 0; }
        {
            char *sl = strrchr(walk, '/');
            if (!sl || sl == walk) break; //mcdc:ignore:defensive getcwd paths always contain a slash so !sl is unpairable
            *sl = 0;
        }
    }
    if (qc_git_toplevel(dst, n) == 0) return 0;
    snprintf(dst, n, "%s", cwd);
    return 0;
}

// Implements: SW-REQ-003
int qc_git_ok(const char *root) {
    char cmd[QC_MAX_PATH + 64], buf[32];
    FILE *f;
    snprintf(cmd, sizeof cmd, "git -C \"%s\" rev-parse --is-inside-work-tree 2>/dev/null", root);
    f = popen(cmd, "r");
    //mcdc:ignore:defensive popen failure is not host-testable
    if (!f) return 0;
    if (!fgets(buf, sizeof buf, f)) { pclose(f); return 0; }
    pclose(f);
    return buf[0] == 't';
}

// Implements: SW-REQ-003
int qc_git_cmd(const char *root, char *out, size_t nout, const char *fmt, ...) {
    char cmd[2048], inner[1600];
    va_list ap;
    FILE *f;
    va_start(ap, fmt);
    vsnprintf(inner, sizeof inner, fmt, ap);
    va_end(ap);
    snprintf(cmd, sizeof cmd, "git -C \"%s\" %s 2>/dev/null", root, inner);
    f = popen(cmd, "r");
    //mcdc:ignore:defensive popen/fopen failure is not host-testable
    if (!f) return -1;
    if (out && nout) {
        size_t n = fread(out, 1, nout - 1, f);
        out[n] = 0;
        qc_trim(out);
    } else {
        char dump[256];
        while (fread(dump, 1, sizeof dump, f)) {}
    }
    return pclose(f);
}

// Implements: SW-REQ-007
int qc_git_branch(const char *root, char *dst, size_t n) {
    char buf[256];
    if (qc_git_cmd(root, buf, sizeof buf, "rev-parse --abbrev-ref HEAD") != 0 || !buf[0])
        return -1;
    snprintf(dst, n, "%s", buf);
    return 0;
}

// Implements: SW-REQ-007
int qc_git_head(const char *root, char *dst, size_t n) {
    char buf[80];
    if (qc_git_cmd(root, buf, sizeof buf, "rev-parse HEAD") != 0 || !buf[0])
        return -1;
    snprintf(dst, n, "%s", buf);
    return 0;
}

// Implements: SW-REQ-007
int qc_on_trunk(QcCtx *cx) {
    char br[256];
    if (!qc_git_ok(cx->root)) return 0; /* no git: not trunk */
    if (qc_git_branch(cx->root, br, sizeof br) != 0) return 0;
    if (strcmp(br, "HEAD") == 0) return 0; /* detached */
    if (cx->cfg.trunk[0] && strcmp(cx->cfg.trunk, "auto") != 0)
        return strcmp(br, cx->cfg.trunk) == 0;
    return strcmp(br, "main") == 0 || strcmp(br, "master") == 0;
}

// Implements: SW-REQ-004
void qc_who(char *dst, size_t n, const char *by_flag) {
    const char *e;
    if (by_flag && by_flag[0]) { snprintf(dst, n, "%s", by_flag); qc_norm_by(dst); return; }
    e = getenv("QC_BY");
    if (e && e[0]) { snprintf(dst, n, "%s", e); qc_norm_by(dst); return; }
    e = getenv("USER");
    if (e && e[0]) { snprintf(dst, n, "%s", e); qc_norm_by(dst); return; }
    e = getenv("USERNAME");
    if (e && e[0]) { snprintf(dst, n, "%s", e); qc_norm_by(dst); return; }
    {
        struct passwd *pw = getpwuid(geteuid());
        if (pw && pw->pw_name) { snprintf(dst, n, "%s", pw->pw_name); qc_norm_by(dst); return; } //mcdc:ignore:defensive a non-NULL passwd always has pw_name on this host
    }
    snprintf(dst, n, "unknown"); //mcdc:ignore:defensive getpwuid always yields a name on this host
}

// Implements: SW-REQ-004
void qc_norm_by(char *by) {
    char *s;
    for (s = by; *s; s++) if (isspace((unsigned char)*s)) *s = '-';
}

void qc_norm_evidence(char *ev) {
    char *s, *d;
    for (s = ev; *s; s++) if (*s == '\n' || *s == '\r') *s = (*s == '\r' ? ' ' : ';');
    /* collapse leftover CR leftovers already handled */
    for (s = d = ev; *s; s++) {
        *d++ = *s;
    }
    *d = 0;
    qc_trim(ev);
}

/* CRC-32 ISO 3309 */
// Implements: SYS-REQ-001
uint32_t qc_crc32(const void *data, size_t n) {
    static uint32_t t[256];
    static int init = 0;
    const uint8_t *p = data;
    uint32_t c = 0xffffffffu;
    size_t i;
    if (!init) {
        uint32_t k, b;
        for (k = 0; k < 256; k++) {
            b = k;
            for (i = 0; i < 8; i++) b = (b & 1) ? (0xedb88320u ^ (b >> 1)) : (b >> 1);
            t[k] = b;
        }
        init = 1;
    }
    for (i = 0; i < n; i++) c = t[(c ^ p[i]) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffffu;
}

void qc_crc6(const char *s, char out[7]) {
    uint32_t c = qc_crc32(s, strlen(s));
    snprintf(out, 7, "%06x", (unsigned)(c & 0xffffffu));
}

/* ---- SHA-256 (FIPS 180-4), locale-blind ---- */
// Implements: SYS-REQ-001
static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

void qc_sha256(const void *data, size_t n, uint8_t out[32]) {
    static const uint32_t K[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    uint32_t H[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    uint8_t *msg;
    size_t bitlen = n * 8, pad, tot, i;
    pad = (n % 64 < 56) ? (56 - n % 64) : (120 - n % 64);
    tot = n + pad + 8;
    msg = calloc(1, tot);
    //mcdc:ignore:defensive calloc OOM is not host-testable
    if (!msg) qc_die(QC_CONFIG, "out of memory\nNEXT: free memory, then retry the last qc command");
    memcpy(msg, data, n);
    msg[n] = 0x80;
    for (i = 0; i < 8; i++) msg[tot - 1 - i] = (uint8_t)(bitlen >> (8 * i));
    for (i = 0; i < tot; i += 64) {
        uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2;
        int j;
        for (j = 0; j < 16; j++) {
            w[j] = ((uint32_t)msg[i + 4 * j] << 24) | ((uint32_t)msg[i + 4 * j + 1] << 16) |
                   ((uint32_t)msg[i + 4 * j + 2] << 8) | (uint32_t)msg[i + 4 * j + 3];
        }
        for (j = 16; j < 64; j++) {
            uint32_t s0 = rotr(w[j - 15], 7) ^ rotr(w[j - 15], 18) ^ (w[j - 15] >> 3);
            uint32_t s1 = rotr(w[j - 2], 17) ^ rotr(w[j - 2], 19) ^ (w[j - 2] >> 10);
            w[j] = w[j - 16] + s0 + w[j - 7] + s1;
        }
        a = H[0]; b = H[1]; c = H[2]; d = H[3]; e = H[4]; f = H[5]; g = H[6]; h = H[7];
        for (j = 0; j < 64; j++) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            t1 = h + S1 + ch + K[j] + w[j];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            t2 = S0 + maj;
            h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        H[0] += a; H[1] += b; H[2] += c; H[3] += d;
        H[4] += e; H[5] += f; H[6] += g; H[7] += h;
    }
    free(msg);
    for (i = 0; i < 8; i++) {
        out[4 * i]     = (uint8_t)(H[i] >> 24);
        out[4 * i + 1] = (uint8_t)(H[i] >> 16);
        out[4 * i + 2] = (uint8_t)(H[i] >> 8);
        out[4 * i + 3] = (uint8_t)H[i];
    }
}

// Implements: SYS-REQ-001
void qc_hex32(const uint8_t *b, char hex[65]) {
    static const char *x = "0123456789abcdef";
    int i;
    for (i = 0; i < 32; i++) {
        hex[2 * i]     = x[b[i] >> 4];
        hex[2 * i + 1] = x[b[i] & 15];
    }
    hex[64] = 0;
}

// Implements: SYS-REQ-001
void qc_sha256_hex(const void *data, size_t n, char hex[65]) {
    uint8_t d[32];
    qc_sha256(data, n, d);
    qc_hex32(d, hex);
}

/* normalize: NUL in first 8KiB → binary (raw); else CRLF→LF */
// Implements: INT-REQ-004
static uint8_t *normalize_bytes(const uint8_t *in, size_t n, size_t *outn, int *binary) {
    size_t sniff = n < 8192 ? n : 8192, i, o;
    uint8_t *out;
    *binary = 0;
    for (i = 0; i < sniff; i++) {
        if (in[i] == 0) { *binary = 1; break; }
    }
    if (*binary) {
        out = malloc(n ? n : 1); //mcdc:ignore:defensive binary path requires a NUL so n is never 0
        //mcdc:ignore:defensive malloc OOM is not host-testable
        //mcdc:ignore:defensive malloc OOM is not host-testable
    if (!out) return NULL;
        if (n) memcpy(out, in, n); //mcdc:ignore:defensive binary path requires a NUL so n is never 0
        *outn = n;
        return out;
    }
    out = malloc(n ? n : 1);
    //mcdc:ignore:defensive malloc OOM is not host-testable
    if (!out) return NULL;
    for (i = o = 0; i < n; i++) {
        if (in[i] == '\r' && i + 1 < n && in[i + 1] == '\n') continue;
        out[o++] = in[i];
    }
    *outn = o;
    return out;
}

// Implements: INT-REQ-004
int qc_sha256_bytes_norm(const void *data, size_t n, char hex[65]) {
    size_t m;
    int bin;
    uint8_t *norm = normalize_bytes(data, n, &m, &bin);
    uint8_t d[32];
    if (!norm) return -1; //mcdc:ignore:defensive malloc OOM is not host-testable
    qc_sha256(norm, m, d);
    qc_hex32(d, hex);
    free(norm);
    (void)bin;
    return 0;
}

// Implements: INT-REQ-004
int qc_sha256_file_norm(const char *path, char hex[65], int *binary) {
    size_t n = 0, m;
    int bin;
    char *raw = qc_read_file(path, &n);
    uint8_t *norm;
    uint8_t d[32];
    if (!raw && n == 0) { //mcdc:ignore:defensive qc_read_file leaves n at the caller's 0 on every failure
        /* empty or missing */
        if (!qc_is_file(path)) return -1;
        raw = qc_strdup(""); //mcdc:ignore:defensive existing empty file always yields a buffer
    }
    if (!raw) return -1; //mcdc:ignore:defensive after the empty-file arm a successful open always yields a buffer
    norm = normalize_bytes((uint8_t *)raw, n, &m, &bin);
    free(raw);
    if (!norm) return -1; //mcdc:ignore:defensive malloc OOM is not host-testable
    qc_sha256(norm, m, d);
    qc_hex32(d, hex);
    free(norm);
    if (binary) *binary = bin;
    return 0;
}

// Implements: SYS-REQ-001
int qc_parse_expires(const char *s) {
    int v;
    char u;
    if (!s || !*s) return 0;
    if (sscanf(s, "%d%c", &v, &u) < 1) return 0;
    if (v < 0) return 0;
    if (u == 'd' || u == 'D') return v * 86400;
    if (u == 'h' || u == 'H') return v * 3600;
    if (u == 'w' || u == 'W') return v * 86400 * 7;
    return 0;
}

// Implements: SW-REQ-005
const char *qc_status_str(QcStatus st) {
    switch (st) {
    case ST_PASS: return "pass";
    case ST_FAIL: return "fail";
    case ST_NA: return "n_a";
    case ST_BASELINE: return "baseline";
    default: return "?";
    }
}

// Implements: SW-REQ-005
QcStatus qc_status_parse(const char *s) {
    if (!s) return ST_UNKNOWN;
    if (strcmp(s, "pass") == 0) return ST_PASS;
    if (strcmp(s, "fail") == 0) return ST_FAIL;
    if (strcmp(s, "n_a") == 0) return ST_NA;
    if (strcmp(s, "baseline") == 0) return ST_BASELINE;
    return ST_UNKNOWN;
}

// Implements: SW-REQ-005
int qc_status_rank(QcStatus st) {
    if (st == ST_FAIL) return 3;
    if (st == ST_NA) return 2;
    if (st == ST_PASS || st == ST_BASELINE) return 1;
    return 0;
}

char *qc_json_escape(const char *s, char *dst, size_t n) {
    size_t i = 0;
    if (!s) { if (n) dst[0] = 0; return dst; }
    while (*s && i + 2 < n) {
        unsigned char c = (unsigned char)*s++;
        if (c == '"' || c == '\\') {
            if (i + 3 >= n) break;
            dst[i++] = '\\'; dst[i++] = (char)c;
        } else if (c == '\n') {
            if (i + 3 >= n) break;
            dst[i++] = '\\'; dst[i++] = 'n';
        } else if (c == '\r') {
            if (i + 3 >= n) break;
            dst[i++] = '\\'; dst[i++] = 'r';
        } else if (c < 32) {
            continue;
        } else {
            dst[i++] = (char)c;
        }
    }
    dst[i] = 0;
    return dst;
}
