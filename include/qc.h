/* qc — deterministic checklist gate (SPEC.md v0.4 Phase 1)
 * Cosmopolitan APE. One binary. No runtime fetch.
 */
#ifndef QC_H
#define QC_H

#define _COSMO_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QC_OK      0
#define QC_OPEN    1
#define QC_CONFIG  2

#define QC_MAX_ID      80
#define QC_MAX_REF     160
#define QC_MAX_Q       300
#define QC_MAX_CMD     300
#define QC_MAX_SCOPE   12
#define QC_MAX_PAT     96
#define QC_MAX_REQ     12
#define QC_MAX_ITEMS   24
#define QC_MAX_LISTS   16
#define QC_MAX_EV      1024
#define QC_MAX_LINE    2048
#define QC_MAX_PATH    1024
#define QC_MAX_FIELD   256
#define QC_MAX_IGNORE  48
#define QC_MAX_DENY    24
#define QC_HEX16       17

/* ---------- types ---------- */

typedef struct {
    char   id[QC_MAX_ID];
    char   question[QC_MAX_Q];
    char   run_cmd[QC_MAX_CMD];
    char   scope[QC_MAX_SCOPE][QC_MAX_PAT];
    int    nscope;
    int    expires_sec;     /* 0 = none */
    int    human;
    int    strict;
    int    checkbox_x;      /* display-only [x] */
} QcItem;

typedef struct {
    char   id[QC_MAX_ID];
    char   applies_when[QC_MAX_Q];
    char   scope[QC_MAX_SCOPE][QC_MAX_PAT];
    int    nscope;
    int    expires_sec;
    char   requires[QC_MAX_REQ][QC_MAX_ID];
    int    nrequires;
    char   from[QC_MAX_FIELD];
    char   path[QC_MAX_PATH];
    char   filename[QC_MAX_ID];
    QcItem items[QC_MAX_ITEMS];
    int    nitems;
    int    has_applies;
    int    saw_checkbox_x;
} QcChecklist;

typedef enum {
    ST_PASS = 0,
    ST_FAIL,
    ST_NA,
    ST_BASELINE,
    ST_UNKNOWN
} QcStatus;

typedef struct {
    char     ref[QC_MAX_REF];
    QcStatus status;
    char     digest[QC_HEX16];
    char     by[64];
    char     at[32];
    int      files;
    int      has_files;
    char     evidence[QC_MAX_EV];
    char     extra[QC_MAX_FIELD];  /* unknown key:value tokens, space-prefixed */
    char     raw[QC_MAX_LINE];
    int      ok;                   /* 0 = malformed / absent */
} QcLine;

typedef struct {
    char ignore[QC_MAX_IGNORE][QC_MAX_PAT];
    int  nignore;
    char deny[QC_MAX_DENY][64];
    int  ndeny;
    char trunk[64];
    int  compact_after;
} QcConfig;

typedef struct {
    char *path;
    char  hash[65];
} QcPair;

typedef enum {
    MODE_LOOP = 0,
    MODE_STAGED,
    MODE_CI
} QcMode;

typedef enum {
    IS_CLEAR = 0,
    IS_FORCED_NEVER,
    IS_FORCED_STALE,
    IS_FORCED_EXPIRED,
    IS_FAILED,
    IS_DORMANT,
    IS_BLOCKED,
    IS_AWAITING
} QcItemState;

typedef struct {
    QcChecklist *list;
    QcItem      *item;          /* NULL for @applies */
    char         ref[QC_MAX_REF];
    char         question[QC_MAX_Q];
    char         digest[QC_HEX16];
    int          npairs;
    QcPair      *pairs;
    int          expires_sec;
    int          is_run;
    int          is_applies;
    int          is_human;
    int          is_strict;
    QcItemState  state;
    QcLine      *match;         /* winning matching attestation */
    QcLine      *echo;          /* most recent any-digest, for I5 echo */
    int          run_rc;
    char         run_hint[256];
    int          blocked_by_requires;
} QcEval;

typedef struct {
    char        root[QC_MAX_PATH];
    QcConfig    cfg;
    QcChecklist lists[QC_MAX_LISTS];
    int         nlists;
    QcLine     *store;          /* all loaded lines from visible tiers */
    int         nstore;
    QcMode      mode;
    int         quiet;
    int         json;
    char        only[QC_MAX_REF];
    time_t      now;
    int         self_scope;     /* set if a scope hit qc/state/ */
    char        self_scope_ref[QC_MAX_REF];
} QcCtx;

/* ---------- util (util.c) ---------- */

#if defined(__GNUC__) || defined(__clang__)
#define QC_NORETURN __attribute__((noreturn))
#else
#define QC_NORETURN
#endif
void        qc_die(int code, const char *fmt, ...) QC_NORETURN;
void        qc_warn(const char *fmt, ...);
void        qc_out(const char *fmt, ...);
char       *qc_strdup(const char *s);
void        qc_trim(char *s);
void        qc_lower(char *s);
int         qc_startswith(const char *s, const char *pfx);
int         qc_endswith(const char *s, const char *sfx);
void        qc_slash(char *p);                    /* \\ → / */
int         qc_path_under(const char *path, const char *dir);
void        qc_join(char *dst, size_t n, const char *a, const char *b);
int         qc_is_dir(const char *p);
int         qc_is_file(const char *p);
int         qc_mkdir_p(const char *path);
int         qc_write_file(const char *path, const char *data);
char       *qc_read_file(const char *path, size_t *out_len); /* malloc, +NUL */
int         qc_copy_file(const char *src, const char *dst);
void        qc_now_iso(char *dst, size_t n);      /* UTC Z */
time_t      qc_parse_iso(const char *s);
char       *qc_relpath(const char *root, const char *abs, char *dst, size_t n);
int         qc_find_root(char *dst, size_t n);    /* 0 ok */
int         qc_git_ok(const char *root);
int         qc_git_cmd(const char *root, char *out, size_t nout, const char *fmt, ...);
int         qc_git_branch(const char *root, char *dst, size_t n);
int         qc_git_head(const char *root, char *dst, size_t n);
int         qc_git_toplevel(char *dst, size_t n);
int         qc_on_trunk(QcCtx *cx);
void        qc_who(char *dst, size_t n, const char *by_flag);
uint32_t    qc_crc32(const void *data, size_t n);
void        qc_crc6(const char *s, char out[7]);
void        qc_sha256(const void *data, size_t n, uint8_t out[32]);
void        qc_sha256_hex(const void *data, size_t n, char hex[65]);
int         qc_sha256_file_norm(const char *path, char hex[65], int *binary);
int         qc_sha256_bytes_norm(const void *data, size_t n, char hex[65]);
void        qc_hex32(const uint8_t *b, char hex[65]);
int         qc_parse_expires(const char *s);      /* seconds, 0 on fail */
const char *qc_status_str(QcStatus st);
QcStatus    qc_status_parse(const char *s);
int         qc_status_rank(QcStatus st);          /* fail=3 n_a=2 pass/base=1 */
char       *qc_json_escape(const char *s, char *dst, size_t n);

/* ---------- globmatch ---------- */
int qc_glob_match(const char *pat, const char *path);
int qc_pat_has_wild(const char *pat);

/* ---------- digest ---------- */
typedef struct {
    char  **paths;
    int     n, cap;
} QcPaths;

void qc_paths_free(QcPaths *p);
int  qc_walk_worktree(const char *root, QcPaths *out);
int  qc_walk_index(const char *root, QcPaths *out); /* --staged names */
int  qc_read_index_blob(const char *root, const char *rel, char **data, size_t *len);

int  qc_default_ignore(const char *rel);
int  qc_ignored(QcCtx *cx, const char *rel, const char *const *pos, int npos);

/* resolve_scope: 0 ok, -1 self-scope (caller must exit 2), -2 io */
int  qc_resolve_scope(QcCtx *cx, const char *const *pats, int npats,
                      QcPair **out, int *nout, int staged);
int  qc_digest_core(const char *checklist_id, const char *item_id,
                    const char *question, const QcPair *pairs, int npairs,
                    char out[QC_HEX16]);
int  qc_digest_item(QcCtx *cx, QcChecklist *cl, QcItem *it,
                    QcPair **pairs, int *npairs, char out[QC_HEX16], int staged);
int  qc_digest_applies(QcCtx *cx, QcChecklist *cl,
                       QcPair **pairs, int *npairs, char out[QC_HEX16], int staged);
void qc_write_manifest(QcCtx *cx, const char *digest, const QcPair *pairs, int n);

/* ---------- parser ---------- */
int  qc_parse_checklists(QcCtx *cx);              /* load qc/checklists/*.md */
int  qc_parse_checklist_file(const char *path, const char *fname, QcChecklist *cl, char *err, size_t errn);
int  qc_parse_config(QcCtx *cx);
int  qc_parse_qcs_line(const char *line, QcLine *out);
int  qc_parse_qcs_file(const char *path, QcLine **out, int *n, int *unknown_ver);
void qc_format_line(const QcLine *ln, char *dst, size_t n);
int  qc_requires_cycles(QcCtx *cx, char *err, size_t n);

/* ---------- store ---------- */
int  qc_load_store(QcCtx *cx);                    /* visible tiers for mode */
int  qc_load_file_lines(const char *path, QcLine **acc, int *n);
int  qc_scratch_path(QcCtx *cx, char *dst, size_t n);
int  qc_segment_name(QcCtx *cx, char *dst, size_t n);  /* basename */
int  qc_segment_path(QcCtx *cx, char *dst, size_t n);
int  qc_write_scratch(QcCtx *cx, QcLine *lines, int n);
int  qc_append_segment(QcCtx *cx, QcLine *lines, int n);
int  qc_read_scratch(QcCtx *cx, QcLine **out, int *n);
void qc_upsert_scratch(QcLine **lines, int *n, const QcLine *one);
QcLine *qc_winner_match(QcLine *lines, int n, const char *ref, const char *digest);
QcLine *qc_latest_any(QcLine *lines, int n, const char *ref);

/* ---------- states / verify kernel ---------- */
typedef int QcExpired;    /* 1 iff expires_s > 0 and answer_age_s > expires_s */
typedef int QcRunStatus;  /* 0 ran; -1 spawn failure. *rc is the child status */

QcExpired   qc_expired(int expires_sec, time_t now, time_t answered_at);
int  qc_eval_all(QcCtx *cx, QcEval **out, int *n);
int  qc_topo_order(QcCtx *cx, int *order, int *nord);
QcRunStatus qc_run_cmd(QcCtx *cx, const char *cmd, int *rc, char *hint, size_t hn);

/* ---------- evidence ---------- */
int  qc_evidence_ok(QcCtx *cx, const char *ev, char *why, size_t n); /* 0 ok */
void qc_norm_evidence(char *ev);
void qc_norm_by(char *by);

/* ---------- templates / commands ---------- */
typedef struct {
    const char *id;
    const char *blurb;
    const char *minimal;
    const char *full;
} QcTemplate;

extern QcTemplate qc_templates[];
extern int qc_ntemplates;
const QcTemplate *qc_find_template(const char *id);

int cmd_init(int argc, char **argv);
int cmd_add(int argc, char **argv);
int cmd_template(int argc, char **argv);
int cmd_plan(int argc, char **argv);
int cmd_decide(int argc, char **argv);
int cmd_mark(int argc, char **argv);
int cmd_baseline(int argc, char **argv);
int cmd_seal(int argc, char **argv);
int cmd_reset(int argc, char **argv);
int cmd_verify(int argc, char **argv);
int cmd_compact(int argc, char **argv);
int cmd_report(int argc, char **argv);
int cmd_help(int argc, char **argv);

int  qc_load_ctx(QcCtx *cx, int need_lists);
void qc_free_ctx(QcCtx *cx);
extern const char *qc_argv0;
typedef int QcHelpRc; /* help printer exit: 0 ok, 2 unknown topic */
QcHelpRc qc_print_help(const char *topic);

#ifdef __cplusplus
}
#endif
#endif /* QC_H */
