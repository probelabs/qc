/* globmatch.c — locale-blind path glob
 *
 * **  zero or more path segments (slash after ** is optional)
 * *   any run of bytes except /
 * ?   one byte except /
 */
#include "qc.h"

// Implements: INT-REQ-004
int qc_pat_has_wild(const char *pat) {
    for (; pat && *pat; pat++) {
        if (*pat == '*' || *pat == '?' || *pat == '[') return 1;
    }
    return 0;
}

// Implements: INT-REQ-004
static int match_class(const char **ppat, unsigned char ch) {
    const char *p = *ppat;
    int neg = 0, ok = 0;
    if (*p == '!' || *p == '^') { neg = 1; p++; }
    while (*p && *p != ']') {
        if (p[1] == '-' && p[2] && p[2] != ']') {
            unsigned char a = (unsigned char)p[0], b = (unsigned char)p[2];
            if (ch >= a && ch <= b) ok = 1;
            p += 3;
        } else {
            if ((unsigned char)*p == ch) ok = 1;
            p++;
        }
    }
    if (*p == ']') p++;
    *ppat = p;
    return neg ? !ok : ok;
}

// Implements: INT-REQ-004
static int match(const char *pat, const char *str);

/* ** matches zero or more segments. A following slash is optional. */
static int match_globstar(const char *pat, const char *str) {
    while (*pat == '*') pat++; /* extra * */
    if (*pat == '/') pat++;
    if (*pat == 0) return 1; /* trailing ** matches the rest */
    /* try this position and every segment boundary */
    for (;;) {
        if (match(pat, str)) return 1;
        if (*str == 0) return 0;
        /* advance to next '/' then one past it; also try mid-segment? no */
        while (*str && *str != '/') str++;
        if (*str == '/') str++;
        else return 0;
    }
}

// Implements: INT-REQ-004
static int match(const char *pat, const char *str) {
    while (*pat) {
        if (pat[0] == '*' && pat[1] == '*') {
            return match_globstar(pat + 2, str);
        }
        if (*pat == '*') {
            pat++;
            if (*pat == 0) {
                /* trailing * — rest of this segment only (no slash) */
                for (; *str; str++) if (*str == '/') return 0;
                return 1;
            }
            /* consume 0+ non-slash */
            for (;;) {
                if (match(pat, str)) return 1;
                if (*str == 0 || *str == '/') return 0;
                str++;
            }
        }
        if (*pat == '?') {
            if (*str == 0 || *str == '/') return 0;
            pat++; str++;
            continue;
        }
        if (*pat == '[') {
            const char *p = pat + 1;
            if (*str == 0 || *str == '/') return 0;
            if (!match_class(&p, (unsigned char)*str)) return 0;
            pat = p;
            str++;
            continue;
        }
        if (*str == 0 || *pat != *str) return 0;
        pat++; str++;
    }
    return *str == 0;
}

// Implements: INT-REQ-004
int qc_glob_match(const char *pat, const char *path) {
    if (!pat || !path) return 0;
    if (pat[0] == '!') pat++; /* caller usually strips; tolerate */
    return match(pat, path);
}
