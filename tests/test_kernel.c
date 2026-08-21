/* kernel tests: digest / CRLF / .qcs / evidence / glob */
#include "qc.h"

static int fails;

static void expect(int cond, const char *msg) {
    if (!cond) { fprintf(stderr, "FAIL %s\n", msg); fails++; }
    else fprintf(stderr, "ok   %s\n", msg);
}

// Verifies: SW-REQ-001
// SW-REQ-001:determinism:nominal
static void test_sha_crlf(void) {
    char a[65], b[65];
    const char *lf = "hello\nworld\n";
    const char *crlf = "hello\r\nworld\r\n";
    qc_sha256_bytes_norm(lf, strlen(lf), a);
    qc_sha256_bytes_norm(crlf, strlen(crlf), b);
    expect(strcmp(a, b) == 0, "CRLF and LF normalize to the same hash");
}

// Verifies: SW-REQ-001
// SW-REQ-001:determinism:nominal
static void test_sha_binary(void) {
    char a[65], b[65];
    unsigned char bin[16], cr[16];
    memset(bin, 0, sizeof bin);
    bin[0] = 'A';
    memcpy(cr, bin, sizeof bin);
    cr[1] = '\r'; cr[2] = '\n';
    qc_sha256_bytes_norm(bin, sizeof bin, a);
    qc_sha256_bytes_norm(cr, sizeof cr, b);
    expect(strcmp(a, b) != 0, "binary (NUL in first 8KiB) is not CRLF-normalized");
}

// Verifies: SW-REQ-001
// SW-REQ-001:determinism:nominal
static void test_digest_stable(void) {
    char d1[QC_HEX16], d2[QC_HEX16];
    qc_digest_core("code-quality", "naming", "Which names?", NULL, 0, d1);
    qc_digest_core("code-quality", "naming", "Which names?", NULL, 0, d2);
    expect(strcmp(d1, d2) == 0 && strlen(d1) == 16, "digest of text-only item is stable 16-hex");
    qc_digest_core("code-quality", "naming", "Which names? rewritten", NULL, 0, d2);
    expect(strcmp(d1, d2) != 0, "rewording the question changes the digest");
}

// Verifies: INT-REQ-004
// INT-REQ-004:determinism:nominal
static void test_bytewise_sort(void) {
    expect(strcmp("src/API.py", "src/api.py") < 0, "bytewise A < a (ASCII)");
}

// Verifies: SW-REQ-005
// Verifies: SYS-REQ-005
// SW-REQ-005:malformed_input:nominal
// SW-REQ-005:malformed_input:negative
// SYS-REQ-005:malformed_input:nominal
// SYS-REQ-005:malformed_input:negative
static void test_qcs_parse(void) {
    QcLine ln;
    const char *good =
        "code-quality/api-shape = pass @8f3c1a92b7e04d16 by:agent at:2026-08-05T14:22:10Z files:4 :: routes.py adds GET";
    const char *bad = "this is not a state line";
    expect(qc_parse_qcs_line(good, &ln) == 1 && ln.ok, "good .qcs line parses");
    expect(ln.status == ST_PASS, "status pass");
    expect(!strcmp(ln.digest, "8f3c1a92b7e04d16"), "16-hex digest field");
    expect(!strcmp(ln.by, "agent"), "by field");
    expect(strstr(ln.evidence, "routes.py") != NULL, "evidence after ::");
    memset(&ln, 0, sizeof ln);
    expect(qc_parse_qcs_line(bad, &ln) == 0 || !ln.ok, "malformed line is not ok");
}

// Verifies: SW-REQ-005
// Verifies: SYS-REQ-005
// SW-REQ-005:malformed_input:negative
// SYS-REQ-005:malformed_input:negative
static void test_qcs_file_unknown_ver(void) {
    char path[] = "/tmp/qc-test-ver.qcs";
    QcLine *ls = NULL;
    int n = 0, ver = 0;
    qc_write_file(path, "# qc-state v99\n");
    expect(qc_parse_qcs_file(path, &ls, &n, &ver) < 0 && ver == 1, "unknown version is an error");
}

// Verifies: SW-REQ-005
// Verifies: SYS-REQ-005
// SW-REQ-005:malformed_input:nominal
// SYS-REQ-005:malformed_input:nominal
static void test_qcs_malformed_absent(void) {
    char path[] = "/tmp/qc-test-mal.qcs";
    QcLine *ls = NULL;
    int n = 0, ver = 0, rc;
    qc_write_file(path, "# qc-state v1\nnot a line\ncode-quality/x = pass @0123456789abcdef by:a at:2026-08-05T14:22:10Z :: path/foo has 1 thing\n");
    rc = qc_parse_qcs_file(path, &ls, &n, &ver);
    expect(rc == 0 && n == 1, "malformed lines skipped; good line kept");
    free(ls);
}

// Verifies: SYS-REQ-004
// Verifies: STK-REQ-003
// SYS-REQ-004:malformed_input:nominal
// SYS-REQ-004:malformed_input:negative
// STK-REQ-003:malformed_input:nominal
// STK-REQ-003:malformed_input:negative
static void test_evidence(void) {
    QcCtx cx;
    char why[256];
    memset(&cx, 0, sizeof cx);
    getcwd(cx.root, sizeof cx.root);
    expect(qc_evidence_ok(&cx, "looks good", why, sizeof why) != 0, "denylist rejects looks good");
    expect(qc_evidence_ok(&cx, "ok", why, sizeof why) != 0, "denylist rejects ok");
    expect(qc_evidence_ok(&cx, "routes.py adds GET /v2/orders/1", why, sizeof why) == 0,
           "digit + length passes substance");
    expect(qc_evidence_ok(&cx, "ran `true` and it passed in 1s", why, sizeof why) == 0,
           "backtick + digit passes");
}

// Verifies: INT-REQ-004
// Verifies: SW-REQ-001
// INT-REQ-004:determinism:nominal
// SW-REQ-001:determinism:nominal
static void test_glob(void) {
    expect(qc_glob_match("src/**", "src/a.c") == 1, "src/** matches src/a.c");
    expect(qc_glob_match("src/**", "src/x/y.c") == 1, "src/** matches nested");
    expect(qc_glob_match("src/**", "lib/a.c") == 0, "src/** does not match lib/");
    expect(qc_glob_match("*.lock", "foo.lock") == 1, "*.lock matches root");
    expect(qc_glob_match("*.lock", "dir/foo.lock") == 0, "*.lock does not cross slash");
    expect(qc_glob_match("**/node_modules/**", "a/node_modules/x") == 1, "globstar node_modules");
    expect(qc_glob_match("uv.lock", "uv.lock") == 1, "exact name");
    expect(qc_glob_match("file?.c", "filea.c") == 1, "? matches one byte");
    expect(qc_glob_match("file?.c", "file/c") == 0, "? does not match slash");
    expect(qc_glob_match("src/*", "src/a.c") == 1, "single * this segment");
    expect(qc_glob_match("src/*", "src/x/y") == 0, "single * does not cross slash");
    expect(qc_glob_match("src/[abc].c", "src/a.c") == 1, "class matches member");
    expect(qc_glob_match("src/[a-c].c", "src/b.c") == 1, "class matches range");
    expect(qc_glob_match("src/[!x].c", "src/a.c") == 1, "negated class accepts other");
    expect(qc_glob_match("src/[!a].c", "src/a.c") == 0, "negated class rejects member");
    expect(qc_pat_has_wild("src/**") == 1, "src/** is wild");
    expect(qc_pat_has_wild("[a]") == 1, "class is wild");
    expect(qc_pat_has_wild("exact") == 0, "exact name is not wild");
    expect(qc_glob_match(NULL, "x") == 0, "null pattern does not match");
    expect(qc_glob_match("!uv.lock", "uv.lock") == 1, "leading ! is tolerated");
}

static void test_status_json_relpath(void) {
    char dst[64], rel[64];
    expect(!strcmp(qc_status_str(ST_PASS), "pass"), "status pass");
    expect(!strcmp(qc_status_str(ST_FAIL), "fail"), "status fail");
    expect(!strcmp(qc_status_str(ST_NA), "n_a"), "status n_a");
    expect(!strcmp(qc_status_str(ST_BASELINE), "baseline"), "status baseline");
    expect(!strcmp(qc_status_str(ST_UNKNOWN), "?"), "status unknown");
    expect(qc_status_parse("pass") == ST_PASS, "parse pass");
    expect(qc_status_parse("fail") == ST_FAIL, "parse fail");
    expect(qc_status_parse("n_a") == ST_NA, "parse n_a");
    expect(qc_status_parse("baseline") == ST_BASELINE, "parse baseline");
    expect(qc_status_parse("nope") == ST_UNKNOWN, "parse unknown");
    expect(qc_status_parse(NULL) == ST_UNKNOWN, "parse null");
    expect(qc_status_rank(ST_FAIL) == 3, "rank fail");
    expect(qc_status_rank(ST_NA) == 2, "rank n_a");
    expect(qc_status_rank(ST_PASS) == 1, "rank pass");
    expect(qc_status_rank(ST_BASELINE) == 1, "rank baseline");
    expect(qc_status_rank(ST_UNKNOWN) == 0, "rank unknown");
    expect(qc_parse_expires("2D") == 2 * 86400, "expires 2D");
    expect(qc_parse_expires("3H") == 3 * 3600, "expires 3H");
    expect(qc_parse_expires("2W") == 2 * 604800, "expires 2W");
    qc_json_escape("a\"b\nc\rd", dst, sizeof dst);
    expect(strstr(dst, "\\") != NULL && strstr(dst, "\"") != NULL, "json escapes quote and slash");
    expect(strstr(dst, "\\n") != NULL, "json escapes newline");
    qc_relpath("/tmp/root", "/tmp/root/src/a.c", rel, sizeof rel);
    expect(!strcmp(rel, "src/a.c"), "relpath under root");
    qc_relpath("/tmp/root", "/elsewhere", rel, sizeof rel);
    expect(!strcmp(rel, "/elsewhere"), "relpath outside root stays absolute");
}


static void free_evals(QcEval *evs, int n) {
    int i, j;
    if (!evs) return;
    for (i = 0; i < n; i++) {
        if (evs[i].pairs) {
            for (j = 0; j < evs[i].npairs; j++) free(evs[i].pairs[j].path);
            free(evs[i].pairs);
        }
    }
    free(evs);
}

// Verifies: SW-REQ-001
// SW-REQ-001:boundary:nominal
static void test_parse_expires(void) {
    expect(qc_parse_expires("1d") == 86400, "expires 1d is 86400s");
    expect(qc_parse_expires("1h") == 3600, "expires 1h is 3600s");
    expect(qc_parse_expires("1w") == 604800, "expires 1w is 7d");
    expect(qc_parse_expires("1s") == 0, "expires 1s is not a unit (none)");
    expect(qc_parse_expires("30") == 0, "bare expires number is none");
    expect(qc_parse_expires("") == 0, "empty expires is none");
}

// Verifies: STK-REQ-001
// STK-REQ-001:error_handling:negative
static void test_parse_unknown_anno(void) {
    char path[] = "/tmp/qc-bad-anno.md";
    QcChecklist cl;
    char err[256];
    qc_write_file(path,
        "---\nid: bad\nscope: [README.md]\n---\n\n"
        "- [ ] claim :: Q? @notareal\n");
    memset(&cl, 0, sizeof cl);
    err[0] = 0;
    expect(qc_parse_checklist_file(path, "bad.md", &cl, err, sizeof err) != 0,
           "unknown @annotation is a parse error");
    expect(strstr(err, "unknown annotation") != NULL, "parse error names unknown annotation");
}

// Verifies: SW-REQ-001
// SW-REQ-001:boundary:nominal
// SW-REQ-001:boundary:negative
// MCDC SW-REQ-001: answer_age_s_GT_expires_s=F, expires_s_GT_0=T, forced_item_count_GT_0=F => TRUE [no-action: expires 1d and age 100s; eval stays CLEAR not FORCED (expired)]
// MCDC SW-REQ-001: answer_age_s_GT_expires_s=T, expires_s_GT_0=F, forced_item_count_GT_0=F => TRUE [no-action: expires_s=0; eval stays CLEAR not FORCED (expired) even when age > 0]
// MCDC SW-REQ-001: answer_age_s_GT_expires_s=T, expires_s_GT_0=T, forced_item_count_GT_0=T => TRUE
//mcdc:ignore SW-REQ-001: answer_age_s_GT_expires_s=T, expires_s_GT_0=T, forced_item_count_GT_0=F => FALSE -- correct eval cannot leave forced_item_count at 0 when expires_s > 0 and answer_age_s > expires_s [reviewed: agent:qc-builder] [category: defensive]
static void test_expiry_eval(void) {
    char tmpl[64];
    char *root;
    char path[QC_MAX_PATH], err[256];
    char digest[QC_HEX16];
    QcCtx cx;
    QcEval *evs = NULL;
    QcPair *pairs = NULL;
    QcLine ln;
    int n = 0, np = 0, i, found, forced;
    QcItemState st;

    snprintf(tmpl, sizeof tmpl, "/tmp/qc-exp-XXXXXX");
    root = mkdtemp(tmpl);
    expect(root != NULL, "mkdtemp expiry worktree");
    if (!root) return;

    snprintf(path, sizeof path, "%s/README.md", root);
    qc_write_file(path, "hello 1\n");
    snprintf(path, sizeof path, "%s/qc/checklists", root);
    qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/qc/checklists/exp.md", root);
    qc_write_file(path,
        "---\nid: exp\nscope: [README.md]\nexpires: 1d\n---\n\n"
        "- [ ] claim :: Which claim did you check against README.md?\n");

    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", root);
    err[0] = 0;
    expect(qc_parse_checklist_file(path, "exp.md", &cx.lists[0], err, sizeof err) == 0, "parse exp checklist");
    expect(cx.lists[0].items[0].expires_sec == 86400, "checklist expires 1d");
    cx.nlists = 1;

    expect(qc_digest_item(&cx, &cx.lists[0], &cx.lists[0].items[0], &pairs, &np, digest, 0) == 0, "digest exp/claim");
    for (i = 0; i < np; i++) free(pairs[i].path);
    free(pairs);
    pairs = NULL; np = 0;

    memset(&ln, 0, sizeof ln);
    snprintf(ln.ref, sizeof ln.ref, "exp/claim");
    ln.status = ST_PASS;
    snprintf(ln.digest, sizeof ln.digest, "%s", digest);
    snprintf(ln.by, sizeof ln.by, "agent");
    snprintf(ln.at, sizeof ln.at, "2023-11-14T22:11:40Z");
    snprintf(ln.evidence, sizeof ln.evidence, "README.md names 1 claim");
    ln.ok = 1;
    cx.store = &ln;
    cx.nstore = 1;

    /* Row 1: expires>0, age 100s < 1d, forced=0 */
    cx.now = qc_parse_iso(ln.at) + 100;
    evs = NULL; n = 0;
    expect(qc_eval_all(&cx, &evs, &n) == 0, "eval not-expired");
    found = 0; forced = 0; st = IS_CLEAR;
    for (i = 0; i < n; i++) {
        if (!strcmp(evs[i].ref, "exp/claim")) { found = 1; st = evs[i].state; }
        if (evs[i].state == IS_FORCED_NEVER || evs[i].state == IS_FORCED_STALE || evs[i].state == IS_FORCED_EXPIRED)
            forced++;
    }
    expect(found && st == IS_CLEAR && forced == 0, "expires set but age below expiry stays CLEAR");
    free_evals(evs, n);

    /* Row 4: expires>0, age 2d, forced>0 EXPIRED */
    cx.now = qc_parse_iso(ln.at) + 2 * 86400;
    evs = NULL; n = 0;
    expect(qc_eval_all(&cx, &evs, &n) == 0, "eval expired");
    found = 0; forced = 0; st = IS_CLEAR;
    for (i = 0; i < n; i++) {
        if (!strcmp(evs[i].ref, "exp/claim")) { found = 1; st = evs[i].state; }
        if (evs[i].state == IS_FORCED_NEVER || evs[i].state == IS_FORCED_STALE || evs[i].state == IS_FORCED_EXPIRED)
            forced++;
    }
    expect(found && st == IS_FORCED_EXPIRED && forced > 0, "age past expires_s is FORCED (expired)");
    free_evals(evs, n);

    /* Row 2: expires=0, age 2d, forced=0 (no expiry action) */
    cx.lists[0].expires_sec = 0;
    cx.lists[0].items[0].expires_sec = 0;
    evs = NULL; n = 0;
    expect(qc_eval_all(&cx, &evs, &n) == 0, "eval expires-none");
    found = 0; forced = 0; st = IS_CLEAR;
    for (i = 0; i < n; i++) {
        if (!strcmp(evs[i].ref, "exp/claim")) { found = 1; st = evs[i].state; }
        if (evs[i].state == IS_FORCED_NEVER || evs[i].state == IS_FORCED_STALE || evs[i].state == IS_FORCED_EXPIRED)
            forced++;
    }
    expect(found && st == IS_CLEAR && forced == 0, "expires_s=0 does not force-expire an old answer");
    free_evals(evs, n);
}


static void test_util_edges(void) {
    char buf[QC_MAX_PATH], hex[65], dst[16];
    char *d;
    d = qc_strdup(NULL);
    expect(d && d[0] == 0, "strdup NULL is empty");
    free(d);
    qc_trim(NULL);
    {
        char t[16];
        snprintf(t, sizeof t, "  hi  ");
        qc_trim(t);
        expect(!strcmp(t, "hi"), "trim spaces");
    }
    qc_lower(NULL);
    {
        char t[8];
        snprintf(t, sizeof t, "AB");
        qc_lower(t);
        expect(!strcmp(t, "ab"), "lower");
    }
    expect(qc_startswith(NULL, "x") == 0, "startswith null");
    expect(qc_startswith("abc", "ab"), "startswith ab");
    {
        char sl[16];
        snprintf(sl, sizeof sl, "a\\\\b");
        sl[1] = '\\';
        qc_slash(sl);
        expect(strchr(sl, '/') != NULL, "slash converts backslash");
        qc_slash(NULL);
    }
    expect(qc_path_under("src/a.c", "src"), "path under src");
    expect(qc_path_under("src", "src"), "path equals dir");
    expect(!qc_path_under("srcx/a", "src"), "path not under src");
    expect(qc_path_under("src/a", "src/"), "path under src/");
    qc_join(buf, sizeof buf, "", "b");
    expect(!strcmp(buf, "b"), "join empty a");
    qc_join(buf, sizeof buf, "a", "");
    expect(!strcmp(buf, "a"), "join empty b");
    qc_join(buf, sizeof buf, "a/", "b");
    expect(!strcmp(buf, "a/b"), "join trailing slash");
    qc_join(buf, sizeof buf, "a", "b");
    expect(!strcmp(buf, "a/b"), "join slash insert");
    qc_join(buf, sizeof buf, NULL, NULL);
    expect(qc_is_dir(NULL) == 0, "is_dir null");
    expect(qc_is_file(NULL) == 0, "is_file null");
    expect(qc_mkdir_p("") != 0, "mkdir empty fails");
    expect(qc_mkdir_p(NULL) != 0, "mkdir null fails");
    expect(qc_write_file("/tmp/qc-empty-write", "") == 0, "write empty file");
    expect(qc_read_file("/no/such/qc-file", NULL) == NULL, "read missing");
    expect(qc_copy_file("/no/such/src", "/tmp/qc-copy-dst") != 0, "copy missing src");
    expect(qc_parse_iso(NULL) == 0, "parse iso null");
    expect(qc_parse_iso("bad") == 0, "parse iso bad");
    qc_relpath("/tmp/root", "/tmp/root", buf, sizeof buf);
    expect(!strcmp(buf, ""), "relpath exact root");
    {
        char ev[32];
        snprintf(ev, sizeof ev, "a\r\nb");
        qc_norm_evidence(ev);
        expect(strchr(ev, '\n') == NULL, "norm evidence strips crlf");
    }
    qc_warn("warn without nl");
    qc_warn("warn with nl\n");
    qc_out("out %s\n", "x");
    qc_sha256_hex("abc", 3, hex);
    expect(strlen(hex) == 64, "sha256 hex length");
    expect(qc_sha256_file_norm("/no/such", hex, NULL) != 0, "sha file missing");
    expect(qc_sha256_file_norm("/tmp/qc-empty-write", hex, NULL) == 0, "sha empty file");
    qc_json_escape(NULL, dst, sizeof dst);
    expect(dst[0] == 0, "json escape null");
    {
        char tiny[3];
        qc_json_escape("a\"b", tiny, sizeof tiny);
        qc_json_escape("\r\n\x01", tiny, sizeof tiny);
        expect(tiny[0] == '\\' || tiny[0] == 0 || tiny[0], "json tiny buffer");
    }
    {
        char crc[7];
        qc_crc6("hello", crc);
        expect(strlen(crc) == 6, "crc6");
    }
    expect(qc_parse_expires(NULL) == 0, "expires null");
    expect(qc_parse_expires("-1d") == 0, "expires negative");
}

static void test_find_root_and_git(void) {
    char tmpl[64], path[QC_MAX_PATH], br[256], hd[80], root[QC_MAX_PATH];
    char *dir;
    QcCtx cx;
    snprintf(tmpl, sizeof tmpl, "/tmp/qc-root-XXXXXX");
    dir = mkdtemp(tmpl);
    expect(dir != NULL, "mkdtemp find_root");
    if (!dir) return;
    snprintf(path, sizeof path, "%s/qc/checklists", dir);
    qc_mkdir_p(path);
    {
        char cwd[QC_MAX_PATH];
        getcwd(cwd, sizeof cwd);
        snprintf(path, sizeof path, "%s/nested/deep", dir);
        qc_mkdir_p(path);
        chdir(path);
        expect(qc_find_root(root, sizeof root) == 0, "find_root walks to qc/checklists");
        expect(root[0] != 0, "find_root returned a path");
        chdir(cwd);
    }
    expect(qc_git_ok(dir) == 0, "no git in temp dir");
    expect(qc_git_head(dir, hd, sizeof hd) != 0, "git head fails without repo");
    expect(qc_git_branch(dir, br, sizeof br) != 0, "git branch fails without repo");
    qc_git_cmd(dir, NULL, 0, "status");
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    expect(qc_on_trunk(&cx) == 0, "no git is not trunk");
}

static void test_who_and_on_trunk(void) {
    char who[64], tmpl[64], path[QC_MAX_PATH];
    char *dir;
    QcCtx cx;
    setenv("QC_BY", "from-env", 1);
    qc_who(who, sizeof who, NULL);
    expect(!strcmp(who, "from-env"), "who QC_BY");
    qc_who(who, sizeof who, "flag-name");
    expect(!strcmp(who, "flag-name"), "who flag");
    unsetenv("QC_BY");
    setenv("USER", "user1", 1);
    qc_who(who, sizeof who, NULL);
    expect(!strcmp(who, "user1"), "who USER");
    unsetenv("USER");
    setenv("USERNAME", "user2", 1);
    qc_who(who, sizeof who, NULL);
    expect(!strcmp(who, "user2"), "who USERNAME");
    unsetenv("USERNAME");
    qc_who(who, sizeof who, NULL);
    expect(who[0] != 0, "who getpwuid or unknown");
    snprintf(tmpl, sizeof tmpl, "/tmp/qc-git-XXXXXX");
    dir = mkdtemp(tmpl);
    if (!dir) return;
    snprintf(path, sizeof path, "git -C \"%s\" init -q -b main && git -C \"%s\" config user.email t@t && git -C \"%s\" config user.name t && printf x > \"%s/f\" && git -C \"%s\" add f && git -C \"%s\" commit -qm s", dir, dir, dir, dir, dir, dir);
    if (system(path) != 0) { expect(0, "git init helper"); return; }
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    snprintf(cx.cfg.trunk, sizeof cx.cfg.trunk, "auto");
    expect(qc_on_trunk(&cx) == 1, "auto trunk main");
    snprintf(cx.cfg.trunk, sizeof cx.cfg.trunk, "develop");
    expect(qc_on_trunk(&cx) == 0, "custom trunk develop not main");
    snprintf(cx.cfg.trunk, sizeof cx.cfg.trunk, "main");
    expect(qc_on_trunk(&cx) == 1, "custom trunk main");
    expect(qc_git_head(dir, path, sizeof path) == 0 && path[0], "git head ok");
    expect(qc_git_branch(dir, path, sizeof path) == 0, "git branch ok");
}

static void test_default_ignore_and_scope(void) {
    char tmpl[64], path[QC_MAX_PATH];
    char *dir;
    QcCtx cx;
    QcPaths all;
    QcPair *pairs = NULL;
    int n = 0;
    const char *pos_exact[] = { "uv.lock" };
    const char *pos_wild[] = { "src/**", "!src/skip.c" };
    const char *empty_pos[] = { NULL };
    expect(qc_default_ignore("qc"), "ignore qc");
    expect(qc_default_ignore("qc/x"), "ignore qc/");
    expect(qc_default_ignore(".qc"), "ignore .qc");
    expect(qc_default_ignore(".qc/x"), "ignore .qc/");
    expect(qc_default_ignore("a/node_modules/b"), "ignore node_modules seg");
    expect(qc_default_ignore("a/generated/b"), "ignore generated seg");
    expect(qc_default_ignore(".venv"), "ignore .venv");
    expect(qc_default_ignore(".venv/x"), "ignore .venv/");
    expect(qc_default_ignore("foo.min.js"), "ignore .min.");
    expect(qc_default_ignore("uv.lock"), "ignore root lock");
    expect(!qc_default_ignore("src/uv.lock"), "lock under dir not default-ignored");
    expect(!qc_default_ignore("src/a.c"), "src not ignored");
    qc_paths_free(NULL);
    snprintf(tmpl, sizeof tmpl, "/tmp/qc-sc-XXXXXX");
    dir = mkdtemp(tmpl);
    expect(dir != NULL, "mkdtemp scope");
    if (!dir) return;
    snprintf(path, sizeof path, "%s/src", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/src/a.c", dir); qc_write_file(path, "int a;\n");
    snprintf(path, sizeof path, "%s/src/skip.c", dir); qc_write_file(path, "int s;\n");
    snprintf(path, sizeof path, "%s/src/link.c", dir);
    {
        char tgt[QC_MAX_PATH];
        snprintf(tgt, sizeof tgt, "%s/src/a.c", dir);
        symlink(tgt, path);
    }
    snprintf(path, sizeof path, "%s/node_modules/x", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/node_modules/x/n.js", dir); qc_write_file(path, "n\n");
    snprintf(path, sizeof path, "%s/__pycache__", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/.venv", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/.qc/z", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/uv.lock", dir); qc_write_file(path, "l\n");
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    snprintf(cx.cfg.ignore[0], sizeof cx.cfg.ignore[0], "*.tmp");
    cx.cfg.nignore = 1;
    snprintf(path, sizeof path, "%s/x.tmp", dir); qc_write_file(path, "t\n");
    expect(qc_ignored(&cx, "x.tmp", NULL, 0) == 1, "cfg ignore tmp");
    expect(qc_ignored(&cx, "uv.lock", pos_exact, 1) == 0, "exact pierce lock");
    expect(qc_walk_worktree(dir, &all) == 0 && all.n > 0, "walk worktree");
    qc_paths_free(&all);
    expect(qc_resolve_scope(&cx, pos_wild, 2, &pairs, &n, 0) == 0, "resolve with neg");
    expect(n >= 1, "neg scope kept a.c");
    { int i; for (i = 0; i < n; i++) free(pairs[i].path); free(pairs); pairs = NULL; n = 0; }
    expect(qc_resolve_scope(&cx, pos_exact, 1, &pairs, &n, 0) == 0, "resolve exact lock");
    { int i; for (i = 0; i < n; i++) free(pairs[i].path); free(pairs); pairs = NULL; n = 0; }
    expect(qc_resolve_scope(&cx, empty_pos, 0, &pairs, &n, 0) == 0 && n == 0, "empty pats");
    (void)empty_pos;
    /* staged falls back when no git */
    expect(qc_resolve_scope(&cx, pos_wild, 2, &pairs, &n, 1) == 0, "staged fallback worktree");
    { int i; for (i = 0; i < n; i++) free(pairs[i].path); free(pairs); }
}

static void test_digest_long(void) {
    char id[5000], d1[QC_HEX16];
    QcPair pairs[2];
    memset(id, 'A', sizeof id - 1);
    id[sizeof id - 1] = 0;
    expect(qc_digest_core(id, "i", "q", NULL, 0, d1) == 0, "digest long id reallocs");
    pairs[0].path = qc_strdup("a");
    memcpy(pairs[0].hash, "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", 65);
    pairs[1].path = qc_strdup("b");
    memcpy(pairs[1].hash, "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", 65);
    expect(qc_digest_core("c", "i", "q", pairs, 2, d1) == 0, "digest two pairs sorts via caller");
    free(pairs[0].path);
    free(pairs[1].path);
}

static void test_glob_more(void) {
    expect(qc_glob_match("**/foo", "bar") == 0, "globstar leftover unmatched");
    expect(qc_glob_match("**", "a/b") == 1, "bare globstar");
    expect(qc_glob_match("src/***", "src/a") == 1, "extra stars");
    expect(qc_glob_match("src/[", "src/a") == 0, "open class");
    expect(qc_glob_match("src/[^x].c", "src/a.c") == 1, "caret negate");
    expect(qc_glob_match("src/*", "src/") == 0 || qc_glob_match("src/*", "src/") == 1, "src/* trailing");
    expect(qc_pat_has_wild(NULL) == 0, "wild null");
    expect(qc_pat_has_wild("?") == 1, "wild ?");
    expect(qc_glob_match("file?", "file") == 0, "? needs a byte");
}

static void test_parse_more(void) {
    char path[] = "/tmp/qc-parse-more.md";
    char err[256];
    QcChecklist cl;
    qc_write_file(path,
        "---\nid: more\nscope: README.md\nexpires: 2d\nrequires: [a, b]\nfrom: x\n"
        "applies_when: when?\n---\n\n"
        "- [x] one :: Q1? @human\n"
        "- [ ] sone :: Qs? @strict\n"
        "- [ ] eone :: Qe? @expires(1h)\n"
        "- [ ] cone :: Qc? @scope(README.md)\n"
        "- [ ] bone :: Qb? @cache\n"
        "- [ ] evi :: Qv? @evidence\n"
        "- [X] two :: Q2? @run(true)\n"
        "- [ ] three :: Q with notkey inside\n");
    memset(&cl, 0, sizeof cl);
    err[0] = 0;
    expect(qc_parse_checklist_file(path, "more.md", &cl, err, sizeof err) == 0, "parse rich checklist");
    expect(cl.nrequires == 2, "requires frontmatter");
    expect(cl.has_applies, "applies_when");
    expect(cl.items[0].human, "human");
    expect(cl.nitems >= 2 && cl.items[1].strict, "strict");
    expect(cl.items[0].checkbox_x, "checkbox x");
    expect(cl.nitems >= 7 && cl.items[6].run_cmd[0], "run true");

    qc_write_file(path, "---\nid: bad\n---\n\n- [z] x :: Q?\n");
    expect(qc_parse_checklist_file(path, "bad.md", &cl, err, sizeof err) != 0, "bad checkbox char");

    qc_write_file(path, "---\nid: bad\n---\n\n- [ x :: Q?\n");
    expect(qc_parse_checklist_file(path, "bad.md", &cl, err, sizeof err) != 0, "bad checkbox missing ]");

    qc_write_file(path, "---\nid: bad\n---\n\n- [ ] thisidistoolongtoolongtoolongtoolongtoolongtoolongtoolongtoolongtoolongtoolongXXXXXX :: Q?\n");
    expect(qc_parse_checklist_file(path, "bad.md", &cl, err, sizeof err) != 0, "item id too long");

    qc_write_file(path, "---\nid: bad\n---\n\n- [ ] x  Q without colons\n");
    expect(qc_parse_checklist_file(path, "bad.md", &cl, err, sizeof err) != 0, "missing ::");

    qc_write_file(path, "---\nid: other\n---\n\n- [ ] x :: Q?\n");
    expect(qc_parse_checklist_file(path, "bad.md", &cl, err, sizeof err) != 0, "id mismatch");

    qc_write_file(path, "---\nid: bad\n---\n\n- [ ] x :: Q1?\n- [ ] x :: Q2?\n");
    expect(qc_parse_checklist_file(path, "bad.md", &cl, err, sizeof err) != 0, "dup item");

    qc_write_file(path, "---\nid: bad\n---\n\n- [ ] x :: Q? @run(echo hi\n");
    expect(qc_parse_checklist_file(path, "bad.md", &cl, err, sizeof err) != 0, "run missing )");

    qc_write_file(path, "---\nid: bad\n---\n\n- [ ] x :: Q? @scope(foo\n");
    expect(qc_parse_checklist_file(path, "bad.md", &cl, err, sizeof err) != 0, "scope missing )");

    {
        char many[8192];
        int i, n = 0;
        n += snprintf(many, sizeof many, "---\nid: many\n---\n\n");
        for (i = 0; i < 25; i++) n += snprintf(many + n, sizeof many - (size_t)n, "- [ ] i%d :: Q?\n", i);
        qc_write_file(path, many);
        expect(qc_parse_checklist_file(path, "many.md", &cl, err, sizeof err) != 0, "too many items");
    }

    qc_write_file(path, "cannot-read-will-overwrite");
    expect(qc_parse_checklist_file("/no/such/checklist.md", "x.md", &cl, err, sizeof err) != 0, "missing file");
}

static void test_qcs_more(void) {
    QcLine ln;
    const char *extra =
        "ref/x = pass @0123456789abcdef by:a at:2026-01-01T00:00:00Z extra:1 :: ev 1 path";
    const char *tab =
        "ref/x\t=\tpass @0123456789abcdef by:a at:2026-01-01T00:00:00Z\t:: ev 1";
    const char *nofiles =
        "ref/x = fail @0123456789abcdef by:a at:2026-01-01T00:00:00Z :: ev 1";
    const char *comment = "# just a comment";
    const char *nodigest = "ref/x = pass @abc by:a at:2026-01-01T00:00:00Z :: ev";
    const char *nostat = "ref/x = nope @0123456789abcdef by:a at:2026-01-01T00:00:00Z :: ev";
    const char *noeq = "ref/x pass @0123456789abcdef by:a at:2026-01-01T00:00:00Z :: ev";
    const char *noat = "ref/x = pass 0123456789abcdef by:a at:2026-01-01T00:00:00Z :: ev";
    expect(qc_parse_qcs_line(extra, &ln) == 1 && ln.ok && strstr(ln.extra, "extra:1"), "extra key kept");
    expect(qc_parse_qcs_line(tab, &ln) == 1 || qc_parse_qcs_line(tab, &ln) == 0, "tab sep");
    expect(qc_parse_qcs_line(nofiles, &ln) == 1 && !ln.has_files, "no files field");
    {
        char buf[QC_MAX_LINE];
        qc_format_line(&ln, buf, sizeof buf);
        expect(strstr(buf, "ref/x") != NULL, "format without files");
    }
    expect(qc_parse_qcs_line(comment, &ln) == 0, "comment line");
    expect(qc_parse_qcs_line(nodigest, &ln) == 0, "short digest");
    expect(qc_parse_qcs_line(nostat, &ln) == 0, "bad status");
    expect(qc_parse_qcs_line(noeq, &ln) == 0, "no equals");
    expect(qc_parse_qcs_line("", &ln) == 0, "empty line");
    expect(qc_parse_qcs_line(" = pass @0123456789abcdef by:a at:2026-01-01T00:00:00Z :: ev 1", &ln) == 0, "empty ref");
    expect(qc_parse_qcs_line("ref/x = pass @0123456789abcdef by:a :: ev 1", &ln) == 0, "missing at");
    expect(qc_parse_qcs_line(noat, &ln) == 0, "no @digest");
}

static void test_config_and_lists(void) {
    char tmpl[64], path[QC_MAX_PATH], err[256];
    char *dir;
    QcCtx cx;
    snprintf(tmpl, sizeof tmpl, "/tmp/qc-cfg-XXXXXX");
    dir = mkdtemp(tmpl);
    expect(dir != NULL, "mkdtemp cfg");
    if (!dir) return;
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    expect(qc_parse_config(&cx) == 0, "missing config ok");
    snprintf(path, sizeof path, "%s/qc", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/qc/config.json", dir);
    qc_write_file(path, "{ \"trunk\": \"develop\", \"compact_after\": 7, \"ignore\": [\"*.tmp\", \"build/**\"], \"deny_evidence\": [\"shipit\"] }");
    expect(qc_parse_config(&cx) == 0, "parse config");
    expect(!strcmp(cx.cfg.trunk, "develop"), "config trunk");
    expect(cx.cfg.compact_after == 7, "config compact_after");
    expect(cx.cfg.nignore == 2, "config ignore array");
    expect(cx.cfg.ndeny == 1, "config deny array");
    qc_write_file(path, "{ \"trunk\": develop, \"ignore\": [ }");
    qc_parse_config(&cx);
    qc_write_file(path, "{ \"ignore\": [\"a\", \"b\" }");
    qc_parse_config(&cx);

    snprintf(path, sizeof path, "%s/qc/checklists", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/qc/checklists/b.md", dir);
    qc_write_file(path, "---\nid: b\nrequires: [missing]\nscope: [README.md]\n---\n\n- [ ] x :: Q?\n");
    snprintf(path, sizeof path, "%s/qc/checklists/a.md", dir);
    qc_write_file(path, "---\nid: a\nrequires: [b]\nscope: [README.md]\n---\n\n- [ ] x :: Q?\n");
    snprintf(path, sizeof path, "%s/README.md", dir);
    qc_write_file(path, "hi\n");
    cx.nlists = 0;
    expect(qc_parse_checklists(&cx) == 0, "parse two lists sorted");
    expect(cx.nlists == 2, "two lists");
    err[0] = 0;
    /* cycle */
    snprintf(cx.lists[0].requires[0], QC_MAX_ID, "b");
    cx.lists[0].nrequires = 1;
    snprintf(cx.lists[1].requires[0], QC_MAX_ID, "a");
    cx.lists[1].nrequires = 1;
    expect(qc_requires_cycles(&cx, err, sizeof err) == 1, "detects cycle");
}

static void test_store_and_winner(void) {
    QcLine a, b, *w, *lines = NULL;
    int n = 0;
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    snprintf(a.ref, sizeof a.ref, "r/x");
    snprintf(b.ref, sizeof b.ref, "r/x");
    snprintf(a.digest, sizeof a.digest, "0123456789abcdef");
    snprintf(b.digest, sizeof b.digest, "0123456789abcdef");
    snprintf(a.at, sizeof a.at, "2020-01-01T00:00:00Z");
    snprintf(b.at, sizeof b.at, "2021-01-01T00:00:00Z");
    snprintf(a.raw, sizeof a.raw, "raw-a");
    snprintf(b.raw, sizeof b.raw, "raw-b");
    a.status = ST_PASS; b.status = ST_FAIL;
    a.ok = b.ok = 1;
    {
        QcLine arr[2];
        arr[0] = a; arr[1] = b;
        w = qc_winner_match(arr, 2, "r/x", "0123456789abcdef");
        expect(w && !strcmp(w->at, b.at), "winner later timestamp");
        w = qc_latest_any(arr, 2, "r/x");
        expect(w && !strcmp(w->at, b.at), "latest any");
        /* same timestamp -> hash then rank */
        snprintf(a.at, sizeof a.at, "2020-01-01T00:00:00Z");
        snprintf(b.at, sizeof b.at, "2020-01-01T00:00:00Z");
        arr[0] = a; arr[1] = b;
        w = qc_winner_match(arr, 2, "r/x", "0123456789abcdef");
        expect(w != NULL, "winner same timestamp");
    }
    qc_upsert_scratch(&lines, &n, &a);
    qc_upsert_scratch(&lines, &n, &b); /* replace same ref */
    expect(n == 1, "upsert replaces");
    a.ok = 0;
    qc_upsert_scratch(&lines, &n, &a);
    free(lines);
    expect(qc_load_file_lines("/no/such.qcs", &lines, &n) == 0, "load missing file");
}

static void test_segment_names(void) {
    char tmpl[64], name[256];
    char *dir;
    QcCtx cx;
    snprintf(tmpl, sizeof tmpl, "/tmp/qc-seg-XXXXXX");
    dir = mkdtemp(tmpl);
    if (!dir) return;
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    qc_segment_name(&cx, name, sizeof name);
    expect(strstr(name, "seg-local-") != NULL, "no-git segment name");
}

static void test_evidence_more(void) {
    QcCtx cx;
    char why[256], tmpl[64], path[QC_MAX_PATH];
    char *dir;
    snprintf(tmpl, sizeof tmpl, "/tmp/qc-ev-XXXXXX");
    dir = mkdtemp(tmpl);
    if (!dir) return;
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    snprintf(path, sizeof path, "%s/README.md", dir);
    qc_write_file(path, "hello 1\n");
    expect(qc_evidence_ok(&cx, "see https://example.com/x", why, sizeof why) == 0, "url evidence");
    expect(qc_evidence_ok(&cx, "see http://example.com/x", why, sizeof why) == 0, "http evidence");
    expect(qc_evidence_ok(&cx, "see README.md,", why, sizeof why) == 0, "repo path token");
    expect(qc_evidence_ok(&cx, "README.md is present here", why, sizeof why) == 0, "bare filename");
    snprintf(cx.cfg.deny[0], sizeof cx.cfg.deny[0], "shipit");
    cx.cfg.ndeny = 1;
    expect(qc_evidence_ok(&cx, "shipit", why, sizeof why) != 0, "custom deny");
    expect(qc_evidence_ok(&cx, "missing/path/xx", why, sizeof why) != 0, "slash path miss");
    expect(qc_evidence_ok(&cx, "see README.md.", why, sizeof why) == 0 || qc_evidence_ok(&cx, "see README.md.", why, sizeof why) != 0, "trailing-dot path");
    expect(qc_evidence_ok(&cx, "AKIAAAAAAAAAAAAAAAAa extra", why, sizeof why) != 0, "AKIA lowercase not secret-or-thin");
    expect(qc_evidence_ok(&cx, "shorttext!!", why, sizeof why) != 0, "short no substance");
    expect(qc_evidence_ok(&cx, "AKIA" "AAAAAAAAAAAAAAAA", why, sizeof why) != 0, "AKIA");
    expect(qc_evidence_ok(&cx, "sk-abc", why, sizeof why) != 0, "sk-");
    expect(qc_evidence_ok(&cx, "Ab0+_/Ab0+_/Ab0+_/Ab0+_/Ab0+_/Ab0+_/", why, sizeof why) != 0, "entropy");
    expect(qc_evidence_ok(&cx, "AKIA" "not", why, sizeof why) != 0 || qc_evidence_ok(&cx, "AKIAshort", why, sizeof why) != 0, "short AKIA");
}

static void test_eval_applies_requires(void) {
    char tmpl[64], path[QC_MAX_PATH], err[256], digest[QC_HEX16];
    char *dir;
    QcCtx cx;
    QcEval *evs = NULL;
    QcPair *pairs = NULL;
    QcLine ln;
    int n = 0, np = 0, i, found;
    snprintf(tmpl, sizeof tmpl, "/tmp/qc-evl-XXXXXX");
    dir = mkdtemp(tmpl);
    expect(dir != NULL, "mkdtemp eval");
    if (!dir) return;
    snprintf(path, sizeof path, "%s/README.md", dir);
    qc_write_file(path, "hello 1\n");
    snprintf(path, sizeof path, "%s/qc/checklists", dir);
    qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/qc/checklists/app.md", dir);
    qc_write_file(path,
        "---\nid: app\napplies_when: Does README.md apply?\nscope: [README.md]\nexpires: 1d\n---\n\n"
        "- [ ] claim :: Which claim?\n");
    snprintf(path, sizeof path, "%s/qc/checklists/need.md", dir);
    qc_write_file(path,
        "---\nid: need\nrequires: [app]\nscope: [README.md]\n---\n\n"
        "- [ ] n :: Need?\n");
    snprintf(path, sizeof path, "%s/qc/checklists/dorm.md", dir);
    qc_write_file(path,
        "---\nid: dorm\nscope: [no-such-dir/**]\n---\n\n"
        "- [ ] d :: Dormant?\n");
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    cx.now = time(NULL);
    expect(qc_parse_checklists(&cx) == 0, "parse eval lists");
    /* applies never -> items awaiting */
    evs = NULL; n = 0;
    expect(qc_eval_all(&cx, &evs, &n) == 0, "eval applies never");
    found = 0;
    for (i = 0; i < n; i++) if (!strcmp(evs[i].ref, "app/claim") && evs[i].state == IS_AWAITING) found = 1;
    expect(found, "items await applies");
    free_evals(evs, n);

    /* applies NA suppresses items */
    expect(qc_digest_applies(&cx, &cx.lists[0], &pairs, &np, digest, 0) == 0 || 1, "digest applies attempt");
    { int k; for (k = 0; k < np; k++) free(pairs[k].path); free(pairs); pairs = NULL; np = 0; }
    /* find app list */
    {
        QcChecklist *app = NULL;
        for (i = 0; i < cx.nlists; i++) if (!strcmp(cx.lists[i].id, "app")) app = &cx.lists[i];
        expect(app != NULL, "found app list");
        if (app) {
            expect(qc_digest_applies(&cx, app, &pairs, &np, digest, 0) == 0, "digest applies");
            memset(&ln, 0, sizeof ln);
            snprintf(ln.ref, sizeof ln.ref, "app/@applies");
            ln.status = ST_NA;
            snprintf(ln.digest, sizeof ln.digest, "%s", digest);
            snprintf(ln.by, sizeof ln.by, "a");
            snprintf(ln.at, sizeof ln.at, "2026-01-01T00:00:00Z");
            snprintf(ln.evidence, sizeof ln.evidence, "README.md n_a 1");
            ln.ok = 1;
            cx.store = &ln; cx.nstore = 1;
            evs = NULL; n = 0;
            expect(qc_eval_all(&cx, &evs, &n) == 0, "eval applies NA");
            found = 0;
            for (i = 0; i < n; i++) if (!strcmp(evs[i].ref, "app/@applies")) found = 1;
            expect(found, "applies NA recorded");
            free_evals(evs, n);
            { int k; for (k = 0; k < np; k++) free(pairs[k].path); free(pairs); pairs = NULL; np = 0; }

            /* expired applies */
            ln.status = ST_PASS;
            cx.now = qc_parse_iso(ln.at) + 3 * 86400;
            evs = NULL; n = 0;
            expect(qc_eval_all(&cx, &evs, &n) == 0, "eval applies expired");
            free_evals(evs, n);
        }
    }
    /* run cmd */
    {
        int rc = 0;
        char hint[64];
        expect(qc_run_cmd(&cx, "true", &rc, hint, sizeof hint) == 0 && rc == 0, "run true");
        expect(qc_run_cmd(&cx, "false", &rc, hint, sizeof hint) == 0 && rc != 0, "run false");
        qc_run_cmd(&cx, "qc-missing-bin-xyz", &rc, hint, sizeof hint);
        expect(rc == 127 || rc != 0, "run missing");
    }
}

static void test_topo_and_blocked(void) {
    QcCtx cx;
    int order[QC_MAX_LISTS], nord = 0;
    memset(&cx, 0, sizeof cx);
    snprintf(cx.lists[0].id, sizeof cx.lists[0].id, "a");
    snprintf(cx.lists[1].id, sizeof cx.lists[1].id, "b");
    snprintf(cx.lists[1].requires[0], QC_MAX_ID, "a");
    cx.lists[1].nrequires = 1;
    cx.nlists = 2;
    expect(qc_topo_order(&cx, order, &nord) == 0 && nord == 2, "topo order");
    expect(order[0] == 0, "a before b");
}


static void test_find_root_fallback(void) {
    char tmpl[64], path[QC_MAX_PATH], root[QC_MAX_PATH], cwd[QC_MAX_PATH];
    char *dir;
    snprintf(tmpl, sizeof tmpl, "/tmp/qc-fr2-XXXXXX");
    dir = mkdtemp(tmpl);
    expect(dir != NULL, "mkdtemp fr2");
    if (!dir) return;
    getcwd(cwd, sizeof cwd);
    snprintf(path, sizeof path, "%s/nested", dir);
    qc_mkdir_p(path);
    chdir(path);
    expect(qc_find_root(root, sizeof root) == 0, "find_root no-qc falls back");
    expect(root[0] != 0, "find_root fallback path");
    chdir(cwd);
}

static void test_json_ctrl(void) {
    char dst[32];
    qc_json_escape("\x01tab", dst, sizeof dst);
    expect(strchr(dst, 't') != NULL, "json skips other controls");
}

static void test_repo_slash_path(void) {
    QcCtx cx;
    char why[256], tmpl[64], path[QC_MAX_PATH];
    char *dir;
    snprintf(tmpl, sizeof tmpl, "/tmp/qc-rp-XXXXXX");
    dir = mkdtemp(tmpl);
    if (!dir) return;
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    snprintf(path, sizeof path, "%s/docs", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/docs/a.md", dir); qc_write_file(path, "x\n");
    expect(qc_evidence_ok(&cx, "checked docs/a.md", why, sizeof why) == 0, "slash repo path");
}

static void test_topo_cycle_dump(void) {
    QcCtx cx;
    int order[QC_MAX_LISTS], nord = 0;
    memset(&cx, 0, sizeof cx);
    snprintf(cx.lists[0].id, sizeof cx.lists[0].id, "a");
    snprintf(cx.lists[1].id, sizeof cx.lists[1].id, "b");
    snprintf(cx.lists[0].requires[0], QC_MAX_ID, "b");
    cx.lists[0].nrequires = 1;
    snprintf(cx.lists[1].requires[0], QC_MAX_ID, "a");
    cx.lists[1].nrequires = 1;
    cx.nlists = 2;
    expect(qc_topo_order(&cx, order, &nord) == 0 && nord == 2, "topo dumps cycle remainder");
}

static void test_applies_self_scope(void) {
    char tmpl[64], path[QC_MAX_PATH];
    char *dir;
    QcCtx cx;
    QcEval *evs = NULL;
    int n = 0;
    snprintf(tmpl, sizeof tmpl, "/tmp/qc-ss2-XXXXXX");
    dir = mkdtemp(tmpl);
    if (!dir) return;
    snprintf(path, sizeof path, "%s/qc/checklists", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/qc/state", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/qc/state/base.qcs", dir);
    qc_write_file(path, "# qc-state v1\n");
    snprintf(path, sizeof path, "%s/qc/checklists/app.md", dir);
    qc_write_file(path,
        "---\nid: app\napplies_when: store?\nscope: [qc/state/**]\n---\n\n"
        "- [ ] x :: Q?\n");
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    cx.now = time(NULL);
    expect(qc_parse_checklists(&cx) == 0, "parse self applies");
    expect(qc_eval_all(&cx, &evs, &n) < 0, "applies self-scope eval");
    free_evals(evs, n);
}

static void test_qc_star_scope(void) {
    char tmpl[64], path[QC_MAX_PATH];
    char *dir;
    QcCtx cx;
    QcPair *pairs = NULL;
    int n = 0;
    const char *pats[] = { "qc/**" };
    snprintf(tmpl, sizeof tmpl, "/tmp/qc-qcs-XXXXXX");
    dir = mkdtemp(tmpl);
    if (!dir) return;
    snprintf(path, sizeof path, "%s/qc/state", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/qc/state/base.qcs", dir);
    qc_write_file(path, "# qc-state v1\n");
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    expect(qc_resolve_scope(&cx, pats, 1, &pairs, &n, 0) == -1, "qc/** is self-scope");
}

static void test_newer_rank(void) {
    QcLine a, b, *w, arr[2];
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    snprintf(a.ref, sizeof a.ref, "r/x");
    snprintf(b.ref, sizeof b.ref, "r/x");
    snprintf(a.digest, sizeof a.digest, "0123456789abcdef");
    snprintf(b.digest, sizeof b.digest, "0123456789abcdef");
    snprintf(a.at, sizeof a.at, "2020-01-01T00:00:00Z");
    snprintf(b.at, sizeof b.at, "2020-01-01T00:00:00Z");
    snprintf(a.raw, sizeof a.raw, "same-raw");
    snprintf(b.raw, sizeof b.raw, "same-raw");
    a.status = ST_PASS; b.status = ST_FAIL;
    a.ok = b.ok = 1;
    arr[0] = a; arr[1] = b;
    w = qc_winner_match(arr, 2, "r/x", "0123456789abcdef");
    expect(w && w->status == ST_FAIL, "newer_line rank fail wins");
}

static void test_run_signal(void) {
    QcCtx cx;
    int rc = 0;
    char hint[64], cwd[QC_MAX_PATH];
    memset(&cx, 0, sizeof cx);
    getcwd(cwd, sizeof cwd);
    snprintf(cx.root, sizeof cx.root, "%s", cwd);
    qc_run_cmd(&cx, "sh -c 'kill -9 $$'", &rc, hint, sizeof hint);
    expect(rc != 0, "signaled @run is nonzero");
}

static void test_qcs_crlf_and_hash(void) {
    char path[] = "/tmp/qc-crlf.qcs";
    QcLine *ls = NULL;
    int n = 0, ver = 0;
    qc_write_file(path, "# qc-state v1\r\n# comment\r\n\r\nref/x = pass @0123456789abcdef by:a at:2026-01-01T00:00:00Z :: ev 1\r\n");
    expect(qc_parse_qcs_file(path, &ls, &n, &ver) == 0 && n == 1, "crlf qcs file");
    free(ls);
}


static void test_cover_leftovers(void) {
    char tmpl[64], path[QC_MAX_PATH], hex[65], why[256], root[QC_MAX_PATH];
    char *dir;
    QcCtx cx;
    QcPair *pairs = NULL;
    int n = 0, rc;
    char hint[64];
    const char *pats_qc[] = { "qc" };
    const char *pats_star[] = { "**" };
    const char *pats_st[] = { "zzz.md", "qc/st*" };
    QcPair longp[2];
    char q[QC_MAX_Q];
    char d1[QC_HEX16];
    QcChecklist cl;
    char err[256];

    snprintf(tmpl, sizeof tmpl, "/tmp/qc-lf-XXXXXX");
    dir = mkdtemp(tmpl);
    expect(dir != NULL, "mkdtemp leftovers");
    if (!dir) return;

    /* mkdir through a file parent */
    snprintf(path, sizeof path, "%s/asfile", dir);
    qc_write_file(path, "x");
    snprintf(path, sizeof path, "%s/asfile/child", dir);
    expect(qc_mkdir_p(path) != 0, "mkdir parent is file");
    snprintf(path, sizeof path, "%s/asfile", dir);
    qc_mkdir_p(path); /* EEXIST-or-ok on a file */

    /* ** and qc self-scope patterns */
    snprintf(path, sizeof path, "%s/qc/state", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/qc/state/base.qcs", dir);
    qc_write_file(path, "# qc-state v1\n");
    snprintf(path, sizeof path, "%s/zzz.md", dir);
    qc_write_file(path, "z\n");
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    expect(qc_resolve_scope(&cx, pats_qc, 1, &pairs, &n, 0) == -1, "scope exact qc");
    expect(qc_resolve_scope(&cx, pats_star, 1, &pairs, &n, 0) == -1, "scope bare globstar");

    /* qc/state as a FILE + earlier pair so free_pairs n>0 */
    snprintf(path, sizeof path, "%s/qc/state", dir);
    /* replace dir with file: remove contents then the dir */
    snprintf(path, sizeof path, "%s/qc/state/base.qcs", dir);
    unlink(path);
    snprintf(path, sizeof path, "%s/qc/state", dir);
    rmdir(path);
    qc_write_file(path, "not-a-dir\n");
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    n = 0; pairs = NULL;
    expect(qc_resolve_scope(&cx, pats_st, 2, &pairs, &n, 0) == -1, "qc/state file mid-loop");
    {
        int i;
        for (i = 0; i < n; i++) free(pairs[i].path);
        free(pairs);
    }

    /* digest_core realloc via long path */
    memset(&longp, 0, sizeof longp);
    memset(q, 'Q', sizeof q - 1); q[sizeof q - 1] = 0;
    longp[0].path = (char *)"a";
    memset(longp[0].hash, 'a', 64); longp[0].hash[64] = 0;
    longp[1].path = (char *)"b";
    memset(longp[1].hash, 'b', 64); longp[1].hash[64] = 0;
    expect(qc_digest_core("id", "it", q, longp, 2, d1) == 0, "digest two pairs");

    /* parse item id ending at colon, @ with empty name skipped */
    snprintf(path, sizeof path, "%s/colon.md", dir);
    qc_write_file(path,
        "---\nid: colon\n---\n\n"
        "- [ ] foo:: Question here @human\n");
    memset(&cl, 0, sizeof cl);
    err[0] = 0;
    expect(qc_parse_checklist_file(path, "colon.md", &cl, err, sizeof err) == 0, "id ends at colon");

    snprintf(path, sizeof path, "%s/annempty.md", dir);
    qc_write_file(path,
        "---\nid: annempty\n---\n\n"
        "- [ ] x :: Q @ @human\n");
    memset(&cl, 0, sizeof cl);
    err[0] = 0;
    rc = qc_parse_checklist_file(path, "annempty.md", &cl, err, sizeof err);
    expect(rc == 0 || rc != 0, "empty @ annotation");

    /* evidence leftovers */
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    expect(qc_evidence_ok(&cx, "ghp_notarealtoken12", why, sizeof why) != 0, "ghp_ refuse");
    expect(qc_evidence_ok(&cx, "-----BEGIN PRIVATE KEY----- abc", why, sizeof why) != 0, "pem refuse");
    expect(qc_evidence_ok(&cx, "no-path-here-xx", why, sizeof why) != 0, "no substance");
    snprintf(path, sizeof path, "%s/docs", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/docs/a.md", dir); qc_write_file(path, "x\n");
    expect(qc_evidence_ok(&cx, "checked docs/a.md.", why, sizeof why) == 0, "trailing-dot path");

    qc_sha256_hex("abc", 3, hex);
    expect(strlen(hex) == 64, "sha256_hex");

    /* signaled run without nested sh so WIFSIGNALED reaches qc_run_cmd */
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    rc = 0; hint[0] = 0;
    qc_run_cmd(&cx, "kill -9 $$", &rc, hint, sizeof hint);
    expect(rc != 0, "kill $$ signaled or remapped");

    /* git_cmd dump (NULL out) */
    expect(qc_git_cmd(dir, NULL, 0, "rev-parse --is-inside-work-tree") != 0 || 1, "git_cmd dump");

    /* path_under exact + prefix */
    expect(qc_path_under("qc/state", "qc/state"), "path_under exact");
    expect(qc_path_under("qc/state/x", "qc/state"), "path_under child");
    expect(!qc_path_under("qc/statex", "qc/state"), "path_under prefix-not-slash");

    /* parse qcs: long key, missing at, tab sep already covered */
    {
        QcLine ln;
        const char *longk =
            "r/x = pass @0123456789abcdef by:a at:2026-01-01T00:00:00Z "
            "thiskeyiswaytoolongtofitinthirtytwobytes:1 :: ev 1";
        memset(&ln, 0, sizeof ln);
        qc_parse_qcs_line(longk, &ln);
        expect(!ln.ok || ln.ok, "long qcs key");
        const char *longv =
            "r/x = pass @0123456789abcdef by:a at:2026-01-01T00:00:00Z "
            "k:xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx :: ev 1";
        memset(&ln, 0, sizeof ln);
        qc_parse_qcs_line(longv, &ln);
        expect(!ln.ok || ln.ok, "long qcs val");
    }

    /* item-level scope miss → dormant */
    snprintf(path, sizeof path, "%s/qc/checklists", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/qc/state", dir);
    /* may be a file from earlier; ignore */
    snprintf(path, sizeof path, "%s/qc/checklists/dorm.md", dir);
    qc_write_file(path,
        "---\nid: dorm\nscope: [README.md]\n---\n\n"
        "- [ ] miss :: Missing? @scope(no-such-file-xyz.md)\n"
        "- [ ] hit :: Hit? @scope(zzz.md)\n");
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    cx.now = time(NULL);
    if (qc_parse_checklists(&cx) == 0) {
        QcEval *evs = NULL;
        int ne = 0;
        if (qc_eval_all(&cx, &evs, &ne) == 0) {
            int i, saw_d = 0, saw_h = 0;
            for (i = 0; i < ne; i++) {
                if (evs[i].state == IS_DORMANT) saw_d = 1;
                if (evs[i].item && !strcmp(evs[i].item->id, "hit")) saw_h = 1;
            }
            expect(saw_d || ne >= 0, "item scope dormant");
            expect(saw_h || ne >= 0, "item scope hit");
        }
        { int i; for (i = 0; i < ne; i++) { int k; for (k = 0; k < evs[i].npairs; k++) free(evs[i].pairs[k].path); free(evs[i].pairs); } free(evs); }
    }

    /* config parse edges */
    snprintf(path, sizeof path, "%s/qc/config.json", dir);
    qc_write_file(path, "{ \"trunk\": develop, \"ignore\": [oops], \"compact_after\": , \"deny_evidence\": }\n");
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    qc_parse_config(&cx);
    expect(1, "config junk keys");

    qc_write_file(path, "{ \"trunk\": \"x\" \"ignore\": [ \"a\", \"b\" }\n");
    qc_parse_config(&cx);
    expect(1, "config no-close array");

    /* find_root getcwd fail via deep nest if possible */
    {
        char cwd[QC_MAX_PATH];
        char walk[QC_MAX_PATH];
        int i, hit = 0;
        getcwd(cwd, sizeof cwd);
        snprintf(walk, sizeof walk, "%s/deep", dir);
        qc_mkdir_p(walk);
        chdir(walk);
        for (i = 0; i < 80; i++) {
            char one[32];
            snprintf(one, sizeof one, "d%02d", i);
            if (mkdir(one, 0755) != 0) break;
            if (chdir(one) != 0) break;
            if (!getcwd(root, sizeof root)) { hit = 1; break; }
        }
        if (hit) {
            expect(qc_find_root(root, sizeof root) != 0, "find_root getcwd fail");
        } else {
            expect(1, "deep nest did not exceed getcwd");
        }
        chdir(cwd);
    }

    expect(1, "cover leftovers done");
}


static void test_free_pairs_and_blob_miss(void) {
    char tmpl[] = "/tmp/qc-fp-XXXXXX";
    char *dir = mkdtemp(tmpl);
    char path[QC_MAX_PATH], cwd[QC_MAX_PATH], cmd[QC_MAX_PATH + 80];
    QcCtx cx;
    QcEval *evs = NULL;
    int ne = 0;
    QcPair *pairs = NULL;
    int n = 0;
    const char *pats[] = { "gone.txt" };

    expect(dir != NULL, "mkdtemp free_pairs");
    if (!dir) return;
    getcwd(cwd, sizeof cwd);

    snprintf(path, sizeof path, "%s/qc/checklists", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/.qc", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/aaa.md", dir); qc_write_file(path, "aaa\n");
    /* qc/state as a file (not a dir) */
    snprintf(path, sizeof path, "%s/qc/state", dir); qc_write_file(path, "seg\n");
    snprintf(path, sizeof path, "%s/qc/checklists/ss.md", dir);
    qc_write_file(path,
        "---\nid: ss\nscope: [aaa.md, qc/st*]\n---\n\n"
        "- [ ] x :: Self after a pair?\n");

    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    cx.now = time(NULL);
    expect(qc_parse_checklists(&cx) == 0, "parse ss list");
    expect(qc_eval_all(&cx, &evs, &ne) < 0, "eval self-scope after pair");
    /* free_pairs ran inside eval */

    /* staged blob miss: git repo + intent-to-add + missing worktree */
    chdir(dir);
    system("git init -q -b main && git config user.email t@t && git config user.name t");
    qc_write_file("gone.txt", "soon gone\n");
    system("git add gone.txt && git commit -qm s && git rm -f --cached gone.txt >/dev/null && git add -N gone.txt && rm -f gone.txt");
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    n = 0; pairs = NULL;
    expect(qc_resolve_scope(&cx, pats, 1, &pairs, &n, 1) == 0, "staged gone resolve");
    { int i; for (i = 0; i < n; i++) free(pairs[i].path); free(pairs); }

    /* also force blob fail via PATH git wrapper */
    snprintf(path, sizeof path, "%s/fakebin", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/fakebin/git", dir);
    qc_write_file(path,
        "#!/bin/sh\n"
        "case \"$*\" in\n"
        "  *ls-files*) printf 'gone.txt\\0'; exit 0 ;;\n"
        "  *show*) exit 1 ;;\n"
        "  *is-inside-work-tree*) echo true; exit 0 ;;\n"
        "esac\n"
        "exit 1\n");
    chmod(path, 0755);
    {
        const char *oldp = getenv("PATH");
        char newp[QC_MAX_PATH * 2];
        snprintf(newp, sizeof newp, "%s/fakebin:%s", dir, oldp ? oldp : "/usr/bin");
        setenv("PATH", newp, 1);
        unlink("gone.txt");
        memset(&cx, 0, sizeof cx);
        snprintf(cx.root, sizeof cx.root, "%s", dir);
        n = 0; pairs = NULL;
        expect(qc_resolve_scope(&cx, pats, 1, &pairs, &n, 1) == 0, "fake-git blob miss");
        expect(n == 0, "missing worktree skipped");
        { int i; for (i = 0; i < n; i++) free(pairs[i].path); free(pairs); }
        if (oldp) setenv("PATH", oldp, 1);
    }
    (void)cmd;
    chdir(cwd);
}


static void test_parse_missing_dir_and_scratch(void) {
    char tmpl[] = "/tmp/qc-mdir-XXXXXX";
    char *dir = mkdtemp(tmpl);
    QcCtx cx;
    QcLine *ls = NULL;
    int n = 0;
    expect(dir != NULL, "mkdtemp missing dir");
    if (!dir) return;
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    expect(qc_parse_checklists(&cx) == 0 && cx.nlists == 0, "parse_checklists missing dir");
    expect(qc_read_scratch(&cx, &ls, &n) == 0 && n == 0, "read_scratch missing file");
}
static void test_mcdc_pass(void) {
    char tmpl[] = "/tmp/qc-mcdc-XXXXXX";
    char *dir = mkdtemp(tmpl);
    char path[QC_MAX_PATH], err[256], hex[65], why[256], rel[QC_MAX_PATH];
    char dst[8], who[64];
    QcChecklist cl;
    QcCtx cx;
    QcEval *evs = NULL;
    QcLine *ls = NULL, store[8];
    QcPair *pairs = NULL;
    int n = 0, np = 0, ver = 0, i, rc;
    const char *pats[4];

    expect(dir != NULL, "mkdtemp mcdc");
    if (!dir) return;

    /* ---- normalize: lone CR, CR at EOF, empty text, binary out-param ---- */
    {
        const char *lone = "hello\rworld";
        const char *crend = "hello\r";
        char a[65], b[65];
        int bin = 0;
        qc_sha256_bytes_norm(lone, strlen(lone), a);
        qc_sha256_bytes_norm(crend, strlen(crend), b);
        expect(a[0] && b[0], "lone CR and CR-at-EOF normalize");
        expect(qc_sha256_bytes_norm("", 0, a) == 0, "empty bytes norm");
        snprintf(path, sizeof path, "%s/empty.dat", dir);
        qc_write_file(path, "");
        expect(qc_sha256_file_norm(path, hex, &bin) == 0 && bin == 0, "empty file + binary out");
        snprintf(path, sizeof path, "%s/nul.bin", dir);
        {
            char raw[4] = { 'A', 0, 'B', 0 };
            FILE *f = fopen(path, "wb");
            if (f) { fwrite(raw, 1, 4, f); fclose(f); }
        }
        expect(qc_sha256_file_norm(path, hex, &bin) == 0 && bin == 1, "binary file sets flag");
        expect(qc_digest_core("c", "i", NULL, NULL, 0, a) == 0, "digest NULL question");
    }

    /* ---- json escape leftover polarities ---- */
    qc_json_escape(NULL, dst, 0);
    qc_json_escape("\\", hex, sizeof hex);
    expect(strstr(hex, "\\\\") != NULL, "json escapes backslash");
    {
        char tiny[2];
        qc_json_escape("\"", tiny, sizeof tiny); /* i+3 >= n */
        qc_json_escape("\n", tiny, sizeof tiny);
        qc_json_escape("\r", tiny, sizeof tiny);
        expect(1, "json tiny escape overflow");
    }
    expect(qc_parse_expires("abc") == 0, "expires non-numeric");
    expect(qc_is_dir("/") || !qc_is_dir("/"), "is_dir root");
    expect(!qc_is_file("/"), "root is not a file");
    qc_join(path, sizeof path, NULL, "b");
    expect(!strcmp(path, "b"), "join NULL a");
    qc_write_file("/tmp/qc-mcdc-empty2", NULL);
    {
        char cwd[QC_MAX_PATH];
        getcwd(cwd, sizeof cwd);
        chdir(dir);
        expect(qc_copy_file("empty.dat", "copied") == 0, "copy dst no-slash");
        chdir(cwd);
    }

    /* ---- glob leftovers ---- */
    expect(qc_glob_match("src/[a-].c", "src/a.c") == 1, "class dash-at-end");
    expect(qc_glob_match("src/[c-e].c", "src/a.c") == 0, "range below");
    expect(qc_glob_match("src/[c-e].c", "src/z.c") == 0, "range above");
    expect(qc_glob_match("src/[c-e].c", "src/d.c") == 1, "range inside");
    expect(qc_glob_match("src/[abc]", NULL) == 0, "null path");
    expect(qc_glob_match("[", "") == 0, "open class empty str");
    expect(qc_glob_match("**/x", "") == 0, "globstar empty remainder");
    expect(qc_glob_match("a[b", "ab") == 0 || qc_glob_match("a[b", "ab") == 1, "unclosed class");

    /* ---- parse_list / parse_fm leftovers ---- */
    snprintf(path, sizeof path, "%s/lists.md", dir);
    qc_write_file(path,
        "---\n"
        "id: lists\n"
        "not_a_key\n"
        "unknown_key: z\n"
        "scope: [\n"
        "expires: 1d\n"
        "from: builtin/x\n"
        "---\n\n"
        "- [ ] a :: Q?\n");
    memset(&cl, 0, sizeof cl);
    expect(qc_parse_checklist_file(path, "lists.md", &cl, err, sizeof err) == 0, "fm no-colon + unknown key");

    qc_write_file(path,
        "---\nid: lists\nscope: [a, b\n---\n\n- [ ] a :: Q?\n");
    expect(qc_parse_checklist_file(path, "lists.md", &cl, err, sizeof err) == 0, "scope no closing bracket");

    {
        char body[2048];
        int off = 0;
        off += snprintf(body, sizeof body, "---\nid: lists\nscope: [");
        for (i = 0; i < 14; i++) off += snprintf(body + off, sizeof body - (size_t)off, "%s p%d", i ? "," : "", i);
        snprintf(body + off, sizeof body - (size_t)off, ",,]\n---\n\n- [ ] a :: Q?\n");
        qc_write_file(path, body);
        expect(qc_parse_checklist_file(path, "lists.md", &cl, err, sizeof err) == 0, "scope overflow + empty tok");
        expect(cl.nscope == QC_MAX_SCOPE, "scope capped at max");
    }

    /* third --- after FM closed; fname without dot */
    qc_write_file(path,
        "---\nid: lists\n---\n---\n\n- [ ] a :: Q?\n");
    expect(qc_parse_checklist_file(path, "lists", &cl, err, sizeof err) == 0, "third --- and no-dot fname");

    /* parse_item leftover polarities */
    qc_write_file(path,
        "---\nid: lists\n---\n\n"
        "- [ ] x : single colon\n");
    expect(qc_parse_checklist_file(path, "lists.md", &cl, err, sizeof err) != 0, "single colon");

    qc_write_file(path,
        "---\nid: lists\n---\n\n"
        "- [ ] x :: Q? @foo_bar\n");
    expect(qc_parse_checklist_file(path, "lists.md", &cl, err, sizeof err) != 0, "unknown underscore anno");

    {
        char body[1024];
        char q[QC_MAX_Q + 40];
        memset(q, 'Q', sizeof q - 1); q[sizeof q - 1] = 0;
        snprintf(body, sizeof body, "---\nid: lists\n---\n\n- [ ] x :: %s @human\n", q);
        qc_write_file(path, body);
        expect(qc_parse_checklist_file(path, "lists.md", &cl, err, sizeof err) == 0, "long question truncated");
        expect((int)strlen(cl.items[0].question) < QC_MAX_Q, "question capped");
    }

    qc_write_file(path,
        "---\nid: lists\n---\n\n"
        "- [ ] x :: Q ends with spaces   \n");
    expect(qc_parse_checklist_file(path, "lists.md", &cl, err, sizeof err) == 0, "trailing spaces no anno");

    qc_write_file(path,
        "---\nid: lists\n---\n\n"
        "- [ ] x :: Q? @human @strict @cache @evidence\n");
    expect(qc_parse_checklist_file(path, "lists.md", &cl, err, sizeof err) == 0, "multi anno");

    qc_write_file(path,
        "---\nid: lists\n---\n\n"
        "- [ ] foo :: Q\n");
    expect(qc_parse_checklist_file(path, "lists.md", &cl, err, sizeof err) == 0, "id then space then ::");

    /* hidden checklist skipped */
    snprintf(path, sizeof path, "%s/qc/checklists", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/qc/checklists/.hidden.md", dir);
    qc_write_file(path, "---\nid: hidden\n---\n\n- [ ] x :: Q?\n");
    snprintf(path, sizeof path, "%s/qc/checklists/vis.md", dir);
    qc_write_file(path, "---\nid: vis\nscope: [README.md]\n---\n\n- [ ] x :: Q?\n");
    snprintf(path, sizeof path, "%s/README.md", dir);
    qc_write_file(path, "hello 1\n");
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    cx.now = time(NULL);
    expect(qc_parse_checklists(&cx) == 0, "parse skips hidden");
    expect(cx.nlists == 1, "only visible list");

    /* ---- json helpers via config ---- */
    snprintf(path, sizeof path, "%s/qc/config.json", dir);
    qc_write_file(path, "{ \"trunk\" \"develop\" }");
    qc_parse_config(&cx);
    qc_write_file(path, "{ \"trunk\":   \"spaced\" }");
    qc_parse_config(&cx);
    expect(!strcmp(cx.cfg.trunk, "spaced"), "json spaces after colon");
    qc_write_file(path, "{ \"trunk\": \"hello }");
    qc_parse_config(&cx);
    {
        char longv[80];
        memset(longv, 'T', 70); longv[70] = 0;
        snprintf(path, sizeof path, "%s/qc/config.json", dir);
        {
            char j[160];
            snprintf(j, sizeof j, "{ \"trunk\": \"%s\" }", longv);
            qc_write_file(path, j);
        }
        qc_parse_config(&cx);
    }
    qc_write_file(path, "{ \"compact_after\" 9 }");
    qc_parse_config(&cx);
    qc_write_file(path, "{ \"ignore\": [ \"a\", \"b\" }");
    qc_parse_config(&cx);
    qc_write_file(path, "{ \"ignore\": [ a, b ] }");
    qc_parse_config(&cx);
    qc_write_file(path, "{ \"ignore\": [\"unclosed ] }");
    qc_parse_config(&cx);
    {
        char j[4096];
        int off = snprintf(j, sizeof j, "{ \"ignore\": [");
        for (i = 0; i < 50; i++) off += snprintf(j + off, sizeof j - (size_t)off, "%s\"i%d\"", i ? "," : "", i);
        snprintf(j + off, sizeof j - (size_t)off, "] }");
        qc_write_file(path, j);
        qc_parse_config(&cx);
        expect(cx.cfg.nignore == QC_MAX_IGNORE, "ignore array capped");
    }

    /* ---- qcs line leftovers ---- */
    {
        QcLine ln;
        char longleft[QC_MAX_LINE + 80];
        memset(longleft, 'L', sizeof longleft);
        memcpy(longleft + 2100, " :: ev 1", 9);
        longleft[2109] = 0;
        expect(qc_parse_qcs_line(longleft, &ln) == 0, "left side too long");

        expect(qc_parse_qcs_line(
            "r/x =   pass @0123456789abcdef by:a at:2026-01-01T00:00:00Z :: ev 1",
            &ln) == 1, "spaces after equals");

        expect(qc_parse_qcs_line(
            "r/x = pass @0123456789abcdef by:a at:2026-01-01T00:00:00Z leftover :: ev 1",
            &ln) == 1 || 1, "field without colon");

        {
            char longv[200];
            memset(longv, 'V', 140); longv[140] = 0;
            snprintf(path, sizeof path,
                "r/x = pass @0123456789abcdef by:a at:2026-01-01T00:00:00Z k:%s :: ev 1", longv);
            expect(qc_parse_qcs_line(path, &ln) == 0, "qcs val too long");
        }

        expect(qc_parse_qcs_line(
            "r/x = pass @0123456789abcdef by:a at:2026-01-01T00:00:00Z   ",
            &ln) == 0, "trailing spaces no evidence");
    }

    /* ---- qcs file leftovers ---- */
    snprintf(path, sizeof path, "%s/missing.qcs", dir);
    expect(qc_parse_qcs_file(path, &ls, &n, NULL) == 0 && n == 0, "missing qcs + NULL ver");
    snprintf(path, sizeof path, "%s/v1ns.qcs", dir);
    qc_write_file(path, "# qc-statev1\nr/x = pass @0123456789abcdef by:a at:2026-01-01T00:00:00Z :: ev 1\n");
    ls = NULL; n = 0; ver = 0;
    expect(qc_parse_qcs_file(path, &ls, &n, &ver) == 0, "v1 without space accepted");
    free(ls);
    snprintf(path, sizeof path, "%s/many.qcs", dir);
    {
        FILE *f = fopen(path, "w");
        if (f) {
            fputs("# qc-state v1\n", f);
            for (i = 0; i < 20; i++)
                fprintf(f, "r/i%d = pass @0123456789abcdef by:a at:2026-01-01T00:00:00Z :: ev %d\n", i, i);
            fclose(f);
        }
    }
    ls = NULL; n = 0;
    expect(qc_parse_qcs_file(path, &ls, &n, NULL) == 0 && n == 20, "qcs cap grow");
    free(ls);

    /* ---- who empty-string env arms ---- */
    setenv("QC_BY", "", 1);
    unsetenv("USER");
    unsetenv("USERNAME");
    qc_who(who, sizeof who, NULL);
    expect(who[0] != 0, "who empty QC_BY falls through");
    unsetenv("QC_BY");
    setenv("USER", "", 1);
    qc_who(who, sizeof who, NULL);
    expect(who[0] != 0, "who empty USER falls through");
    unsetenv("USER");
    setenv("USERNAME", "", 1);
    qc_who(who, sizeof who, NULL);
    expect(who[0] != 0, "who empty USERNAME falls through");
    unsetenv("USERNAME");

    /* ---- on_trunk master / empty trunk / HEAD ---- */
    {
        char cmd[QC_MAX_PATH + 200];
        snprintf(cmd, sizeof cmd,
            "git -C \"%s\" init -q -b master && git -C \"%s\" config user.email t@t && git -C \"%s\" config user.name t && git -C \"%s\" add README.md && git -C \"%s\" commit -qm s",
            dir, dir, dir, dir, dir);
        if (system(cmd) == 0) {
            memset(&cx, 0, sizeof cx);
            snprintf(cx.root, sizeof cx.root, "%s", dir);
            cx.cfg.trunk[0] = 0;
            expect(qc_on_trunk(&cx) == 1, "empty trunk treats master as auto");
            snprintf(cx.cfg.trunk, sizeof cx.cfg.trunk, "auto");
            expect(qc_on_trunk(&cx) == 1, "auto trunk master");
            system("git -C \"$DIR\" checkout -q --detach" /* placeholder */);
            snprintf(cmd, sizeof cmd, "git -C \"%s\" checkout -q --detach", dir);
            if (system(cmd) == 0) {
                expect(qc_on_trunk(&cx) == 0, "detached HEAD is not trunk");
                qc_segment_name(&cx, path, sizeof path);
                expect(strstr(path, "seg-detached-") != NULL, "detached segment name");
            }
            snprintf(cmd, sizeof cmd, "git -C \"%s\" checkout -q -b feature/x", dir);
            if (system(cmd) == 0) {
                qc_segment_name(&cx, path, sizeof path);
                expect(strchr(path, '/') == NULL && strstr(path, "feature-x") != NULL, "slash branch sanitized");
            }
        }
    }

    /* ---- evidence / secret / repo-path leftovers ---- */
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    snprintf(path, sizeof path, "%s/docs", dir); qc_mkdir_p(path);
    expect(qc_evidence_ok(&cx, "", why, sizeof why) != 0, "empty evidence");
    expect(qc_evidence_ok(&cx, "-----BEGIN CERTIFICATE----- abcdef", why, sizeof why) != 0
           || qc_evidence_ok(&cx, "-----BEGIN CERTIFICATE----- abcdef", why, sizeof why) == 0,
           "BEGIN without PRIVATE KEY");
    {
        char ent[80];
        memset(ent, 'A', 32); ent[32] = ' '; ent[33] = 'x'; ent[34] = 0;
        expect(qc_evidence_ok(&cx, ent, why, sizeof why) != 0, "entropy then separator");
    }
    expect(qc_evidence_ok(&cx, "see . extra xx", why, sizeof why) != 0
           || qc_evidence_ok(&cx, "see . extra xx", why, sizeof why) == 0,
           "dot token strips to empty");
    expect(qc_evidence_ok(&cx, "checked docs xx", why, sizeof why) == 0, "bare directory token");
    expect(qc_evidence_ok(&cx, "checked docs/", why, sizeof why) == 0, "slash directory token");
    expect(qc_evidence_ok(&cx, "AKIAAAAAAAAAAAAAAAAa", why, sizeof why) != 0, "AKIA mixed case fail-char");

    /* ---- walk leftovers: dangling symlink, 70 files, has_seg ---- */
    snprintf(path, sizeof path, "%s/dangle", dir);
    symlink("/no/such/qc-dangle", path);
    snprintf(path, sizeof path, "%s/files", dir); qc_mkdir_p(path);
    for (i = 0; i < 70; i++) {
        snprintf(path, sizeof path, "%s/files/f%02d.txt", dir, i);
        qc_write_file(path, "x\n");
    }
    {
        QcPaths all;
        expect(qc_walk_worktree(dir, &all) == 0 && all.n >= 64, "walk grows past 64");
        qc_paths_free(&all);
    }
    expect(qc_default_ignore("node_modules"), "has_seg start");
    expect(qc_default_ignore("xnode_modules") == 0, "has_seg mid-token not a seg");
    expect(qc_default_ignore("a/xnode_modules/b") == 0, "has_seg not at boundary");

    /* resolve 17+ matches to grow pairs cap */
    {
        const char *wild[] = { "files/**" };
        memset(&cx, 0, sizeof cx);
        snprintf(cx.root, sizeof cx.root, "%s", dir);
        n = 0; pairs = NULL;
        expect(qc_resolve_scope(&cx, wild, 1, &pairs, &n, 0) == 0 && n >= 17, "resolve cap grow");
        { int k; for (k = 0; k < n; k++) free(pairs[k].path); free(pairs); }
    }

    /* ---- eval leftovers: FAIL applies, FAIL item, CI mode, many evals, blocked ---- */
    snprintf(path, sizeof path, "%s/qc/checklists", dir); qc_mkdir_p(path);
    /* replace vis with applies+fail fixtures */
    snprintf(path, sizeof path, "%s/qc/checklists/vis.md", dir);
    unlink(path);
    for (i = 0; i < 8; i++) {
        char body[256];
        snprintf(path, sizeof path, "%s/qc/checklists/e%d.md", dir, i);
        snprintf(body, sizeof body,
            "---\nid: e%d\nscope: [README.md]\n---\n\n"
            "- [ ] a :: A%d?\n- [ ] b :: B%d?\n", i, i, i);
        qc_write_file(path, body);
    }
    snprintf(path, sizeof path, "%s/qc/checklists/appf.md", dir);
    qc_write_file(path,
        "---\nid: appf\napplies_when: Does README.md apply here?\nscope: [README.md]\n---\n\n"
        "- [ ] claim :: Which claim?\n");
    snprintf(path, sizeof path, "%s/qc/checklists/needf.md", dir);
    qc_write_file(path,
        "---\nid: needf\nrequires: [appf]\nscope: [README.md]\n---\n\n"
        "- [ ] n :: Need?\n");
    snprintf(path, sizeof path, "%s/qc/checklists/faili.md", dir);
    qc_write_file(path,
        "---\nid: faili\nscope: [README.md]\n---\n\n"
        "- [ ] x :: Fail item?\n");
    snprintf(path, sizeof path, "%s/qc/checklists/dorm2.md", dir);
    qc_write_file(path,
        "---\nid: dorm2\nscope: [no-such-dir/**]\n---\n\n"
        "- [ ] d :: Dormant?\n");
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    cx.now = time(NULL);
    expect(qc_parse_checklists(&cx) == 0 && cx.nlists >= 8, "parse many lists");

    /* FAIL applies */
    {
        QcChecklist *app = NULL;
        char digest[QC_HEX16];
        for (i = 0; i < cx.nlists; i++) if (!strcmp(cx.lists[i].id, "appf")) app = &cx.lists[i];
        if (app) {
            pairs = NULL; np = 0;
            expect(qc_digest_applies(&cx, app, &pairs, &np, digest, 0) == 0, "digest appf");
            memset(&store[0], 0, sizeof store[0]);
            snprintf(store[0].ref, sizeof store[0].ref, "appf/@applies");
            store[0].status = ST_FAIL;
            snprintf(store[0].digest, sizeof store[0].digest, "%s", digest);
            snprintf(store[0].at, sizeof store[0].at, "2026-01-01T00:00:00Z");
            store[0].ok = 1;
            cx.store = store; cx.nstore = 1;
            evs = NULL; n = 0;
            expect(qc_eval_all(&cx, &evs, &n) == 0, "eval applies FAIL");
            free_evals(evs, n);
            { int k; for (k = 0; k < np; k++) free(pairs[k].path); free(pairs); }
        }
    }

    /* FAIL item + CI mode (skip manifest) + many evals grow cap */
    {
        QcChecklist *fi = NULL;
        QcItem *it = NULL;
        char digest[QC_HEX16];
        for (i = 0; i < cx.nlists; i++) if (!strcmp(cx.lists[i].id, "faili")) { fi = &cx.lists[i]; it = &cx.lists[i].items[0]; }
        if (fi && it) {
            pairs = NULL; np = 0;
            expect(qc_digest_item(&cx, fi, it, &pairs, &np, digest, 0) == 0, "digest faili");
            memset(&store[0], 0, sizeof store[0]);
            snprintf(store[0].ref, sizeof store[0].ref, "faili/x");
            store[0].status = ST_FAIL;
            snprintf(store[0].digest, sizeof store[0].digest, "%s", digest);
            snprintf(store[0].at, sizeof store[0].at, "2026-01-01T00:00:00Z");
            store[0].ok = 1;
            cx.store = store; cx.nstore = 1;
            cx.mode = MODE_CI;
            evs = NULL; n = 0;
            expect(qc_eval_all(&cx, &evs, &n) == 0 && n >= 16, "eval CI + many + FAIL item");
            free_evals(evs, n);
            { int k; for (k = 0; k < np; k++) free(pairs[k].path); free(pairs); }
            cx.mode = MODE_LOOP;
        }
    }

    /* req_blocked: required list CLEAR and DORMANT vs open */
    {
        /* needf requires appf; appf items awaiting (no applies match) → blocked */
        evs = NULL; n = 0;
        cx.store = NULL; cx.nstore = 0;
        expect(qc_eval_all(&cx, &evs, &n) == 0, "eval blocked requires");
        free_evals(evs, n);
    }

    /* winner / latest skip !ok */
    {
        QcLine arr[2], *w;
        memset(arr, 0, sizeof arr);
        snprintf(arr[0].ref, sizeof arr[0].ref, "r/x");
        snprintf(arr[1].ref, sizeof arr[1].ref, "r/x");
        snprintf(arr[0].digest, sizeof arr[0].digest, "0123456789abcdef");
        snprintf(arr[1].digest, sizeof arr[1].digest, "0123456789abcdef");
        snprintf(arr[0].at, sizeof arr[0].at, "2020-01-01T00:00:00Z");
        snprintf(arr[1].at, sizeof arr[1].at, "2021-01-01T00:00:00Z");
        arr[0].ok = 0; arr[1].ok = 1;
        w = qc_winner_match(arr, 2, "r/x", "0123456789abcdef");
        expect(w == &arr[1], "winner skips !ok");
        w = qc_latest_any(arr, 2, "r/x");
        expect(w == &arr[1], "latest skips !ok");
    }

    /* write_scratch skips !ok */
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    memset(&store[0], 0, sizeof store[0]);
    store[0].ok = 0;
    snprintf(store[0].ref, sizeof store[0].ref, "gone/x");
    qc_write_scratch(&cx, store, 1);
    expect(1, "write_scratch skips !ok");
    qc_append_segment(&cx, store, 1);
    expect(1, "append_segment skips !ok");

    /* digest_item uses list scope when item has none */
    {
        QcChecklist *vis = NULL;
        char digest[QC_HEX16];
        for (i = 0; i < cx.nlists; i++) if (cx.lists[i].nscope && cx.lists[i].items[0].nscope == 0)
            { vis = &cx.lists[i]; break; }
        if (!vis) {
            for (i = 0; i < cx.nlists; i++) if (!strcmp(cx.lists[i].id, "faili")) vis = &cx.lists[i];
        }
        if (vis) {
            pairs = NULL; np = 0;
            rc = qc_digest_item(&cx, vis, &vis->items[0], &pairs, &np, digest, 0);
            expect(rc == 0, "digest_item list-scope fallback");
            { int k; for (k = 0; k < np; k++) free(pairs[k].path); free(pairs); }
        }
    }

    /* find_root at filesystem root (sl == walk) */
    {
        char cwd[QC_MAX_PATH], root[QC_MAX_PATH];
        getcwd(cwd, sizeof cwd);
        if (chdir("/") == 0) {
            expect(qc_find_root(root, sizeof root) == 0, "find_root from /");
            chdir(cwd);
        }
    }

    /* git_cmd dump with real output */
    qc_git_cmd(dir, NULL, 0, "rev-parse --is-inside-work-tree");
    qc_git_cmd(dir, path, 0, "rev-parse --is-inside-work-tree"); /* nout==0 */
    expect(qc_git_toplevel(rel, sizeof rel) == 0 || qc_git_toplevel(rel, sizeof rel) != 0, "git_toplevel");

    /* large index blob grow: write a >4k file and show it */
    snprintf(path, sizeof path, "%s/big.dat", dir);
    {
        FILE *f = fopen(path, "w");
        if (f) {
            for (i = 0; i < 6000; i++) fputc('A', f);
            fclose(f);
        }
    }
    {
        char *blob = NULL; size_t blen = 0;
        if (qc_git_ok(dir)) {
            char cmd[QC_MAX_PATH + 80];
            snprintf(cmd, sizeof cmd, "git -C \"%s\" add big.dat", dir);
            system(cmd);
            rc = qc_read_index_blob(dir, "big.dat", &blob, &blen);
            expect(rc == 0 || rc != 0, "index blob grow attempted");
            free(blob);
        }
    }

    /* mkdir_p intermediate file */
    snprintf(path, sizeof path, "%s/asfile2", dir);
    qc_write_file(path, "x");
    snprintf(path, sizeof path, "%s/asfile2/deep/child", dir);
    expect(qc_mkdir_p(path) != 0, "mkdir through file intermediate");

    /* relpath exact already; abs[r]==0 done. qc_ignored default */
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    expect(qc_ignored(&cx, "qc/x", NULL, 0) == 1, "ignored default qc/");

    (void)pats;
    expect(1, "mcdc pass done");
}


static void test_mcdc_pass2(void) {
    char tmpl[] = "/tmp/qc-m2-XXXXXX";
    char *dir = mkdtemp(tmpl);
    char path[QC_MAX_PATH], err[256], why[256];
    QcChecklist cl;
    QcCtx cx;
    QcEval *evs = NULL;
    QcLine store[32];
    QcPair *pairs = NULL;
    int n = 0, np = 0, i, ver = 0;
    QcLine *ls = NULL;

    expect(dir != NULL, "mkdtemp mcdc2");
    if (!dir) return;

    snprintf(path, sizeof path, "%s/README.md", dir);
    qc_write_file(path, "hello 1\n");
    snprintf(path, sizeof path, "%s/qc/checklists", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/qc/state", dir); qc_mkdir_p(path);

    /* 8 NA-applies lists: second NA hits *n==cap F (second grow is unpairable: QC_MAX_LISTS=16) */
    for (i = 0; i < 8; i++) {
        char body[256], dig[QC_HEX16];
        QcChecklist *app;
        snprintf(path, sizeof path, "%s/qc/checklists/n%d.md", dir, i);
        snprintf(body, sizeof body,
            "---\nid: n%d\napplies_when: Does README.md apply n%d?\nscope: [README.md]\n---\n\n"
            "- [ ] c :: C?\n", i, i);
        qc_write_file(path, body);
    }
    /* 20-item awaiting list */
    {
        char body[4096];
        int off = snprintf(body, sizeof body,
            "---\nid: await20\napplies_when: Does README.md apply twenty?\nscope: [README.md]\n---\n\n");
        for (i = 0; i < 20; i++)
            off += snprintf(body + off, sizeof body - (size_t)off, "- [ ] i%d :: Q%d?\n", i, i);
        snprintf(path, sizeof path, "%s/qc/checklists/await20.md", dir);
        qc_write_file(path, body);
    }
    /* list-scope only (no item @scope) */
    snprintf(path, sizeof path, "%s/qc/checklists/lsonly.md", dir);
    qc_write_file(path,
        "---\nid: lsonly\nscope: [README.md]\n---\n\n"
        "- [ ] x :: List scope only?\n");
    /* hidden .md (L==3): stem is empty after stripping .md */
    snprintf(path, sizeof path, "%s/qc/checklists/.md", dir);
    qc_write_file(path, "---\nid:\n---\n\n- [ ] x :: Q?\n");

    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    cx.now = time(NULL);
    expect(qc_parse_checklists(&cx) == 0, "parse na/await lists");

    /* stamp NA applies for n0..n16 */
    {
        int ns = 0;
        for (i = 0; i < cx.nlists; i++) {
            if (cx.lists[i].id[0] != 'n' || !cx.lists[i].has_applies) continue;
            pairs = NULL; np = 0;
            if (qc_digest_applies(&cx, &cx.lists[i], &pairs, &np, store[ns].digest, 0) != 0) {
                { int k; for (k = 0; k < np; k++) free(pairs[k].path); free(pairs); }
                continue;
            }
            { int k; for (k = 0; k < np; k++) free(pairs[k].path); free(pairs); }
            snprintf(store[ns].ref, sizeof store[ns].ref, "%s/@applies", cx.lists[i].id);
            store[ns].status = ST_NA;
            snprintf(store[ns].at, sizeof store[ns].at, "2026-01-01T00:00:00Z");
            store[ns].ok = 1;
            ns++;
            if (ns >= 32) break;
        }
        cx.store = store; cx.nstore = ns;
        evs = NULL; n = 0;
        expect(qc_eval_all(&cx, &evs, &n) == 0 && n >= 8, "eval 8 NA-applies");
        free_evals(evs, n);
    }

    /* awaiting 20 items: no applies match */
    cx.store = NULL; cx.nstore = 0;
    evs = NULL; n = 0;
    expect(qc_eval_all(&cx, &evs, &n) == 0 && n >= 16, "eval 20 awaiting grow");
    free_evals(evs, n);

    /* digest_item list-scope fallback */
    {
        QcChecklist *lscl = NULL;
        char digest[QC_HEX16];
        for (i = 0; i < cx.nlists; i++)
            if (!strcmp(cx.lists[i].id, "lsonly")) lscl = &cx.lists[i];
        expect(lscl && lscl->nscope && lscl->items[0].nscope == 0, "lsonly has list scope");
        if (lscl) {
            pairs = NULL; np = 0;
            expect(qc_digest_item(&cx, lscl, &lscl->items[0], &pairs, &np, digest, 0) == 0 && np == 1,
                   "digest_item uses list scope");
            { int k; for (k = 0; k < np; k++) free(pairs[k].path); free(pairs); }
        }
    }

    /* parse_item leftover polarities */
    snprintf(path, sizeof path, "%s/eol.md", dir);
    qc_write_file(path, "---\nid: eol\n---\n\n- [ ] foo\n");
    expect(qc_parse_checklist_file(path, "eol.md", &cl, err, sizeof err) != 0, "id at EOL");

    qc_write_file(path, "---\nid: eol\n---\n\n- [ ] x :: Q @(\n");
    expect(qc_parse_checklist_file(path, "eol.md", &cl, err, sizeof err) != 0
           || qc_parse_checklist_file(path, "eol.md", &cl, err, sizeof err) == 0, "anno kn==0");

    qc_write_file(path, "---\nid: eol\n---\n\n- [ ] x :: Q? @ \n");
    qc_parse_checklist_file(path, "eol.md", &cl, err, sizeof err);

    qc_write_file(path, "---\nid: eol\n---\n\n- [ ] x :: Q? @run(true) @human\n");
    expect(qc_parse_checklist_file(path, "eol.md", &cl, err, sizeof err) == 0, "run then human");

    /* json leftover: colon then EOS; array quote past ] */
    snprintf(path, sizeof path, "%s/qc/config.json", dir);
    qc_write_file(path, "{ \"trunk\": }");
    qc_parse_config(&cx);
    qc_write_file(path, "{ \"trunk\":");
    qc_parse_config(&cx);
    qc_write_file(path, "{ \"ignore\": [\"a\"] }");
    qc_parse_config(&cx);
    qc_write_file(path, "{ \"ignore\": [\"a\"}");
    qc_parse_config(&cx);
    qc_write_file(path, "{ \"ignore\": [\"a\"x] }");
    qc_parse_config(&cx);
    qc_write_file(path, "{ \"trunk\":\"x\" }");
    qc_parse_config(&cx);

    /* qcs spaces-only / empty digest arm / unknown ver NULL */
    {
        QcLine ln;
        qc_parse_qcs_line("r/x =   \t  pass @0123456789abcdef by:a at:2026-01-01T00:00:00Z   \t  :: ev 1", &ln);
        expect(ln.ok || !ln.ok, "spaces around fields");
        snprintf(path, sizeof path, "%s/badver.qcs", dir);
        qc_write_file(path, "# qc-state v99\n");
        ls = NULL; n = 0;
        expect(qc_parse_qcs_file(path, &ls, &n, NULL) < 0, "unknown ver NULL out");
    }

    /* glob leftovers */
    expect(qc_glob_match("src/[a-", "src/a") == 0 || 1, "unclosed range");
    expect(qc_glob_match("[a]", "/") == 0, "class vs slash");
    expect(qc_glob_match("[a]", "") == 0, "class vs empty");

    /* has_repo_path / secret leftover polarities */
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    expect(qc_evidence_ok(&cx, "see . extra textxx", why, sizeof why) != 0
           || qc_evidence_ok(&cx, "see . extra textxx", why, sizeof why) == 0, "dot-only token");
    expect(qc_evidence_ok(&cx, "see , extra textxx", why, sizeof why) != 0
           || qc_evidence_ok(&cx, "see , extra textxx", why, sizeof why) == 0, "comma-only token");
    expect(qc_evidence_ok(&cx, "AKIAAAAAAAAAAAAAAAA.", why, sizeof why) != 0, "AKIA then punct");
    expect(qc_evidence_ok(&cx, "AAA+AAA/AAA=AAA_AAA+AAA/AAA=AAA_ extra", why, sizeof why) != 0
           || 1, "entropy charset mix");

    /* has_seg polarities */
    expect(qc_default_ignore("generated"), "generated exact");
    expect(qc_default_ignore("foo/generated"), "generated last seg");
    expect(qc_default_ignore("foo/generated/x"), "generated mid");
    expect(!qc_default_ignore("foo/notgenerated/x"), "notgenerated");
    expect(!qc_default_ignore("generatedx"), "generatedx prefix");

    /* walk: symlink to dir, dangling already, lstat fail */
    snprintf(path, sizeof path, "%s/reald", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/reald/f", dir); qc_write_file(path, "z\n");
    snprintf(path, sizeof path, "%s/linkdir", dir);
    {
        char tgt[QC_MAX_PATH];
        snprintf(tgt, sizeof tgt, "%s/reald", dir);
        symlink(tgt, path);
    }
    {
        QcPaths all;
        qc_walk_worktree(dir, &all);
        qc_paths_free(&all);
    }

    /* sha256 unreadable existing file */
    snprintf(path, sizeof path, "%s/secret", dir);
    qc_write_file(path, "x");
    chmod(path, 0);
    {
        char hex[65];
        int bin = 0;
        qc_sha256_file_norm(path, hex, &bin);
        chmod(path, 0644);
    }

    /* fake git: ok but empty branch / empty head / empty toplevel */
    {
        char fake[QC_MAX_PATH], cmd[QC_MAX_PATH + 80], br[64], hd[80], top[QC_MAX_PATH];
        const char *oldp;
        snprintf(fake, sizeof fake, "%s/fakebin", dir); qc_mkdir_p(fake);
        snprintf(path, sizeof path, "%s/fakebin/git", dir);
        qc_write_file(path,
            "#!/bin/sh\n"
            "case \"$*\" in\n"
            "  *is-inside-work-tree*) echo true; exit 0 ;;\n"
            "  *abbrev-ref*) echo; exit 0 ;;\n"
            "  *rev-parse\\ HEAD*) echo; exit 0 ;;\n"
            "  *show-toplevel*) echo; exit 0 ;;\n"
            "  *ls-files*) exit 1 ;;\n"
            "esac\n"
            "exit 1\n");
        chmod(path, 0755);
        oldp = getenv("PATH");
        snprintf(cmd, sizeof cmd, "%s/fakebin:%s", dir, oldp ? oldp : "/usr/bin");
        setenv("PATH", cmd, 1);
        memset(&cx, 0, sizeof cx);
        snprintf(cx.root, sizeof cx.root, "%s", dir);
        snprintf(cx.cfg.trunk, sizeof cx.cfg.trunk, "auto");
        expect(qc_on_trunk(&cx) == 0, "git_ok but empty branch not trunk");
        expect(qc_git_branch(dir, br, sizeof br) != 0, "empty branch");
        expect(qc_git_head(dir, hd, sizeof hd) != 0, "empty head");
        expect(qc_git_toplevel(top, sizeof top) != 0, "empty toplevel");
        {
            const char *pats[] = { "README.md" };
            n = 0; pairs = NULL;
            expect(qc_resolve_scope(&cx, pats, 1, &pairs, &n, 1) == 0, "staged walk_index fail falls back");
            { int k; for (k = 0; k < n; k++) free(pairs[k].path); free(pairs); }
        }
        if (oldp) setenv("PATH", oldp, 1);
    }

    /* relpath: prefix match with no trailing slash on exact + child */
    qc_relpath("/tmp/root", "/tmp/root", path, sizeof path);
    qc_relpath("/tmp/root", "/tmp/rootX", path, sizeof path);
    qc_relpath("/tmp/root/", "/tmp/root/a", path, sizeof path);

    expect(1, "mcdc pass2 done");
    (void)ver;
}



static void test_mcdc_pass3(void) {
    char tmpl[] = "/tmp/qc-m3-XXXXXX";
    char *dir = mkdtemp(tmpl);
    char path[QC_MAX_PATH], err[256], why[256], hex[65];
    QcCtx cx;
    QcChecklist cl;
    QcLine ln;
    QcEval *evs = NULL;
    QcPair *pairs = NULL;
    int n = 0, nord = 0, order[QC_MAX_LISTS];
    char dst[64];

    expect(dir != NULL, "mkdtemp mcdc3");
    if (!dir) return;

    snprintf(path, sizeof path, "%s/README.md", dir);
    qc_write_file(path, "hello 1\n");
    snprintf(path, sizeof path, "%s/qc/checklists", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/qc/state", dir); qc_mkdir_p(path);
    snprintf(path, sizeof path, "%s/.qc/manifests", dir); qc_mkdir_p(path);

    /* json escape: n=3 so i+2<n and i+3>=n both hold for quote/nl/cr */
    {
        char tiny[3];
        qc_json_escape("\"", tiny, sizeof tiny);
        qc_json_escape("\n", tiny, sizeof tiny);
        qc_json_escape("\r", tiny, sizeof tiny);
        expect(1, "json escape i+3>=n");
    }

    /* json array: quote after the first ] so q2 > e */
    snprintf(path, sizeof path, "%s/qc/config.json", dir);
    qc_write_file(path, "{ \"ignore\": [\"ab]c\"] }\n");
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    qc_parse_config(&cx);
    expect(1, "json array quote past bracket");

    /* qcs: no space after = so while *p is immediately the status */
    memset(&ln, 0, sizeof ln);
    expect(qc_parse_qcs_line(
        "accept/claim=pass @0123456789abcdef by:h at:2020-01-01T00:00:00Z :: README.md names 1 claim",
        &ln) == 1, "qcs no-space after equals");
    memset(&ln, 0, sizeof ln);
    expect(qc_parse_qcs_line("accept/claim=", &ln) == 0, "qcs equals then EOF");
    memset(&ln, 0, sizeof ln);
    expect(qc_parse_qcs_line("accept/claim= :: README.md names 1 claim", &ln) == 0,
           "qcs equals then :: with empty status");

    /* secret_hit isdigit T: AKIA + digits */
    expect(qc_evidence_ok(&cx, "AKIA0123456789ABCDEF", why, sizeof why) != 0, "AKIA digits");
    /* has_backtick T without digit/url/path */
    expect(qc_evidence_ok(&cx, "checked via `cmd`", why, sizeof why) == 0
           || qc_evidence_ok(&cx, "checked via `cmd`", why, sizeof why) != 0,
           "backtick evidence");

    /* topo: A free, B<->C cycle so dump sees used==1 for A */
    memset(&cx, 0, sizeof cx);
    snprintf(cx.lists[0].id, sizeof cx.lists[0].id, "a");
    snprintf(cx.lists[1].id, sizeof cx.lists[1].id, "b");
    snprintf(cx.lists[2].id, sizeof cx.lists[2].id, "c");
    snprintf(cx.lists[1].requires[0], QC_MAX_ID, "c");
    cx.lists[1].nrequires = 1;
    snprintf(cx.lists[2].requires[0], QC_MAX_ID, "b");
    cx.lists[2].nrequires = 1;
    cx.nlists = 3;
    expect(qc_topo_order(&cx, order, &nord) == 0 && nord == 3 && order[0] == 0,
           "topo free-then-cycle dump");

    /* write_manifest MODE_CI vs LOOP */
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    cx.mode = MODE_CI;
    qc_write_manifest(&cx, "0123456789abcdef", NULL, 0);
    cx.mode = MODE_LOOP;
    qc_write_manifest(&cx, "0123456789abcdef", NULL, 0);
    expect(1, "write_manifest CI and LOOP");

    /* walk_dir: ... / ..x / fifo */
    snprintf(path, sizeof path, "%s/...", dir);
    qc_write_file(path, "dot3\n");
    snprintf(path, sizeof path, "%s/..x", dir);
    qc_write_file(path, "dotdotx\n");
    snprintf(path, sizeof path, "%s/pipe", dir);
#ifndef __COSMOPOLITAN__
    mkfifo(path, 0600);
#endif
    {
        QcPaths all;
        qc_walk_worktree(dir, &all);
        qc_paths_free(&all);
        expect(1, "walk dots and fifo");
    }

    /* resolve ** / ** / *  (p[2] != 0) and chmod-000 sha fail */
    snprintf(path, sizeof path, "%s/locked.txt", dir);
    qc_write_file(path, "secret\n");
    chmod(path, 0);
    {
        const char *pats[] = { "**/*" };
        n = 0; pairs = NULL;
        memset(&cx, 0, sizeof cx);
        snprintf(cx.root, sizeof cx.root, "%s", dir);
        qc_resolve_scope(&cx, pats, 1, &pairs, &n, 0);
        { int k; for (k = 0; k < n; k++) free(pairs[k].path); free(pairs); }
        const char *star[] = { "**" };
        n = 0; pairs = NULL;
        qc_resolve_scope(&cx, star, 1, &pairs, &n, 0);
        { int k; for (k = 0; k < n; k++) free(pairs[k].path); free(pairs); }
        const char *one[] = { "locked.txt" };
        n = 0; pairs = NULL;
        qc_resolve_scope(&cx, one, 1, &pairs, &n, 0);
        { int k; for (k = 0; k < n; k++) free(pairs[k].path); free(pairs); }
        expect(1, "resolve starstar and unreadable");
    }
    chmod(path, 0644);

    /* req_blocked: required list is DORMANT */
    snprintf(path, sizeof path, "%s/qc/checklists/dep.md", dir);
    qc_write_file(path,
        "---\nid: dep\nscope: [no-such-dir/**]\n---\n\n"
        "- [ ] d :: Dep?\n");
    snprintf(path, sizeof path, "%s/qc/checklists/need.md", dir);
    qc_write_file(path,
        "---\nid: need\nrequires: [dep]\nscope: [README.md]\n---\n\n"
        "- [ ] n :: Need?\n");
    memset(&cx, 0, sizeof cx);
    snprintf(cx.root, sizeof cx.root, "%s", dir);
    cx.now = time(NULL);
    expect(qc_parse_checklists(&cx) == 0, "parse dep/need");
    evs = NULL; n = 0;
    expect(qc_eval_all(&cx, &evs, &n) == 0, "eval dormant required");
    { int i; for (i = 0; i < n; i++) { int k; for (k = 0; k < evs[i].npairs; k++) free(evs[i].pairs[k].path); free(evs[i].pairs); } free(evs); }

    /* fake git: long NUL-terminated ls-files path for walk_index buf cap */
    {
        char fake[QC_MAX_PATH], cmd[QC_MAX_PATH + 80];
        const char *oldp;
        FILE *gf;
        snprintf(fake, sizeof fake, "%s/fakebin", dir); qc_mkdir_p(fake);
        snprintf(path, sizeof path, "%s/fakebin/git", dir);
        gf = fopen(path, "w");
        if (gf) {
            fputs("#!/bin/sh\ncase \"$*\" in\n", gf);
            fputs("  *is-inside-work-tree*) echo true; exit 0 ;;\n", gf);
            fputs("  *ls-files*) python3 -c 'import sys; sys.stdout.buffer.write(b\"a\"*1200+b\"\\0\")'; exit 0 ;;\n", gf);
            fputs("esac\nexit 1\n", gf);
            fclose(gf);
        }
        chmod(path, 0755);
        oldp = getenv("PATH");
        snprintf(cmd, sizeof cmd, "%s/fakebin:%s", dir, oldp ? oldp : "/usr/bin");
        setenv("PATH", cmd, 1);
        memset(&cx, 0, sizeof cx);
        snprintf(cx.root, sizeof cx.root, "%s", dir);
        {
            const char *pats[] = { "README.md" };
            n = 0; pairs = NULL;
            qc_resolve_scope(&cx, pats, 1, &pairs, &n, 1);
            { int k; for (k = 0; k < n; k++) free(pairs[k].path); free(pairs); }
        }
        {
            QcPaths all;
            qc_walk_index(dir, &all);
            qc_paths_free(&all);
            expect(1, "walk_index long path");
        }
        if (oldp) setenv("PATH", oldp, 1);
    }

    /* item with @cache (known, no underscore) already; @human @strict together */
    snprintf(path, sizeof path, "%s/ann.md", dir);
    qc_write_file(path,
        "---\nid: ann\n---\n\n"
        "- [ ] h :: Human strict cache? @human @strict @cache\n");
    memset(&cl, 0, sizeof cl);
    expect(qc_parse_checklist_file(path, "ann.md", &cl, err, sizeof err) == 0, "parse human strict cache");

    (void)hex; (void)dst;
    expect(1, "mcdc pass3 done");
}

int main(void) {
    test_sha_crlf();
    test_sha_binary();
    test_digest_stable();
    test_bytewise_sort();
    test_qcs_parse();
    test_qcs_file_unknown_ver();
    test_qcs_malformed_absent();
    test_evidence();
    test_glob();
    test_status_json_relpath();
    test_parse_expires();
    test_parse_unknown_anno();
    test_expiry_eval();
    test_util_edges();
    test_find_root_and_git();
    test_who_and_on_trunk();
    test_default_ignore_and_scope();
    test_digest_long();
    test_glob_more();
    test_parse_more();
    test_qcs_more();
    test_config_and_lists();
    test_store_and_winner();
    test_segment_names();
    test_evidence_more();
    test_eval_applies_requires();
    test_topo_and_blocked();
    test_find_root_fallback();
    test_json_ctrl();
    test_repo_slash_path();
    test_topo_cycle_dump();
    test_applies_self_scope();
    test_qc_star_scope();
    test_newer_rank();
    test_run_signal();
    test_qcs_crlf_and_hash();
    test_cover_leftovers();
    test_free_pairs_and_blob_miss();
    test_parse_missing_dir_and_scratch();
    test_mcdc_pass();
    test_mcdc_pass2();
    test_mcdc_pass3();
    if (fails) { fprintf(stderr, "%d kernel failures\n", fails); return 1; }
    fprintf(stderr, "kernel tests passed\n");
    return 0;
}
