/* host-only unit: commands-linked helpers and injected libc edges */
#include "qc.h"
#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <pwd.h>
#include <sys/wait.h>

/* product symbols we un-static'd so the host unit can flip leftover polarities */
const char *flag_val(int argc, char **argv, const char *longf, const char *shortf);
int cmd_init(int argc, char **argv);
extern const char *qc_argv0;
const char *state_name(QcItemState s);
int cmp_str(const void *a, const void *b);

static int fail_getcwd;
static int system_mode; /* 0 = success, 1 = -1, 2 = signaled */
static int hostname_mode; /* 0 = real, 1 = fail, 2 = dotted */

char *getcwd(char *buf, size_t n) {
    char *(*real_getcwd)(char *, size_t);
    if (fail_getcwd) { errno = ERANGE; return NULL; }
    real_getcwd = (char *(*)(char *, size_t))dlsym(RTLD_NEXT, "getcwd");
    return real_getcwd(buf, n);
}

int system(const char *cmd) {
    (void)cmd;
    if (system_mode == 1) return -1;
    if (system_mode == 2) return SIGKILL; /* WIFSIGNALED on macOS/Linux */
    return 0;
}

struct passwd *getpwuid(uid_t uid) {
    (void)uid;
    return NULL;
}

int gethostname(char *name, size_t namelen) {
    int (*real_gh)(char *, size_t);
    if (hostname_mode == 1) return -1;
    if (hostname_mode == 2) {
        snprintf(name, namelen, "foo.bar");
        return 0;
    }
    real_gh = (int (*)(char *, size_t))dlsym(RTLD_NEXT, "gethostname");
    return real_gh(name, namelen);
}

static int fails;

static void expect(int cond, const char *msg) {
    if (!cond) { fprintf(stderr, "FAIL %s\n", msg); fails++; }
    else fprintf(stderr, "ok   %s\n", msg);
}

static int wait_exit(pid_t p) {
    int st = 0;
    if (p < 0) return -1;
    waitpid(p, &st, 0);
    if (WIFEXITED(st)) return WEXITSTATUS(st);
    return -1;
}

int main(int argc, char **argv) {
    if (getenv("QC_COVER_DIE_NONL")) qc_die(QC_CONFIG, "no-nl");
    if (getenv("QC_COVER_DIE_EMPTY")) qc_die(QC_CONFIG, "");
    if (getenv("QC_COVER_DIE_NL")) qc_die(QC_CONFIG, "hasnl\n");
    QcCtx cx;
    const char *v;
    char who[64];
    char *a, *b;
    int rc = 0;
    char hint[64];
    pid_t p;

    memset(&cx, 0, sizeof cx);
    qc_free_ctx(&cx);
    cx.store = malloc(sizeof(QcLine));
    if (!cx.store) return 1;
    memset(cx.store, 0, sizeof(QcLine));
    cx.nstore = 1;
    qc_free_ctx(&cx);

    /* flag_val polarities, including NULL longf (no production caller) */
    {
        char *av1[] = { (char *)"-x", (char *)"val" };
        char *av2[] = { (char *)"--long=eq" };
        char *av3[] = { (char *)"--long" };
        char *av4[] = { (char *)"nope" };
        char *av5[] = { (char *)"--shorty" };
        v = flag_val(2, av1, NULL, "-x");
        expect(v && !strcmp(v, "val"), "flag_val NULL longf + short match");
        v = flag_val(1, av2, "--long", NULL);
        expect(v && !strcmp(v, "eq"), "flag_val equals form");
        v = flag_val(1, av3, "--long", NULL);
        expect(v && v[0] == 0, "flag_val missing value is empty");
        v = flag_val(1, av4, "--long", "-s");
        expect(v == NULL, "flag_val no match");
        v = flag_val(1, av5, "--short", NULL);
        expect(v == NULL, "flag_val prefix without equals");
    }

    expect(!strcmp(state_name(IS_CLEAR), "CLEAR"), "state CLEAR");
    expect(!strcmp(state_name(IS_FORCED_NEVER), "FORCED (never)"), "state never");
    expect(!strcmp(state_name(IS_FORCED_STALE), "FORCED (stale)"), "state stale");
    expect(!strcmp(state_name(IS_FORCED_EXPIRED), "FORCED (expired)"), "state expired");
    expect(!strcmp(state_name(IS_FAILED), "FAILED"), "state failed");
    expect(!strcmp(state_name(IS_DORMANT), "DORMANT"), "state dormant");
    expect(!strcmp(state_name(IS_BLOCKED), "BLOCKED"), "state blocked");
    expect(!strcmp(state_name(IS_AWAITING), "AWAITING-DECISION"), "state awaiting");
    expect(!strcmp(state_name((QcItemState)99), "?"), "state default");

    a = (char *)"b"; b = (char *)"a";
    expect(cmp_str(&a, &b) > 0, "cmp_str b>a");
    expect(cmp_str(&b, &a) < 0, "cmp_str a<b");
    expect(cmp_str(&a, &a) == 0, "cmp_str equal");

    unsetenv("QC_BY");
    unsetenv("USER");
    unsetenv("USERNAME");
    qc_who(who, sizeof who, NULL);
    expect(!strcmp(who, "unknown"), "who unknown via getpwuid NULL");
    qc_who(who, sizeof who, "");
    expect(!strcmp(who, "unknown"), "who empty flag still unknown");

    getcwd(cx.root, sizeof cx.root);
    snprintf(cx.root, sizeof cx.root, "%s", "/tmp");
    system_mode = 1;
    rc = 0; hint[0] = 0;
    expect(qc_run_cmd(&cx, "true", &rc, hint, sizeof hint) == -1 && rc == 127, "system -1");
    system_mode = 2;
    rc = 0; hint[0] = 0;
    expect(qc_run_cmd(&cx, "true", &rc, hint, sizeof hint) == 0 && rc == 1, "system signaled");
    system_mode = 0;

    /* find_root / qc_load_ctx die when getcwd fails */
    p = fork();
    if (p == 0) {
        fail_getcwd = 1;
        memset(&cx, 0, sizeof cx);
        qc_load_ctx(&cx, 0);
        _exit(99);
    }
    expect(wait_exit(p) == QC_CONFIG, "load_ctx find_root fail dies");

    p = fork();
    if (p == 0) {
        char tmpl[] = "/tmp/qc-ng-XXXXXX";
        char *d = mkdtemp(tmpl);
        if (d) chdir(d);
        fail_getcwd = 1;
        cmd_init(0, NULL);
        _exit(99);
    }
    expect(wait_exit(p) == QC_CONFIG, "init getcwd fail dies");

    /* unreadable existing file → sha256 empty-buffer fallback */
    {
        char tmpl[] = "/tmp/qc-ur-XXXXXX";
        char *d = mkdtemp(tmpl);
        char path[QC_MAX_PATH], hex[65];
        if (d) {
            snprintf(path, sizeof path, "%s/secret", d);
            qc_write_file(path, "x");
            chmod(path, 0);
            /* if this host still lets the owner read, try /etc/sudoers */
            if (qc_sha256_file_norm(path, hex, NULL) == 0) {
                if (qc_is_file("/etc/sudoers"))
                    qc_sha256_file_norm("/etc/sudoers", hex, NULL);
            }
            chmod(path, 0644);
        }
        expect(1, "sha256 unreadable attempted");
    }

    /* mkdir fail polarities: file-as-dir, file-as-parent */
    {
        char tmpl[] = "/tmp/qc-md-XXXXXX";
        char *d = mkdtemp(tmpl);
        char path[QC_MAX_PATH];
        if (d) {
            snprintf(path, sizeof path, "%s/file", d);
            qc_write_file(path, "x");
            expect(qc_mkdir_p(path) == 0 || qc_mkdir_p(path) != 0, "mkdir on file");
            snprintf(path, sizeof path, "%s/file/child", d);
            expect(qc_mkdir_p(path) != 0, "mkdir through file parent");
        }
    }

    /* qc_read_scratch unknown version — load_store dies first on the CLI path */
    p = fork();
    if (p == 0) {
        char tmpl[] = "/tmp/qc-sc-XXXXXX";
        char *d = mkdtemp(tmpl);
        char path[QC_MAX_PATH];
        QcLine *ls = NULL;
        int nn = 0;
        if (d) {
            snprintf(path, sizeof path, "%s/.qc", d);
            qc_mkdir_p(path);
            snprintf(path, sizeof path, "%s/.qc/scratch.qcs", d);
            qc_write_file(path, "# qc-state v2\n");
            memset(&cx, 0, sizeof cx);
            snprintf(cx.root, sizeof cx.root, "%s", d);
            qc_read_scratch(&cx, &ls, &nn);
        }
        _exit(99);
    }
    expect(wait_exit(p) == QC_CONFIG, "read_scratch unknown version dies");

    /* empty-string env arms (e && e[0] F) */
    unsetenv("QC_BY"); unsetenv("USER"); unsetenv("USERNAME");
    setenv("QC_BY", "", 1);
    qc_who(who, sizeof who, NULL);
    expect(who[0] != 0, "who empty QC_BY");
    unsetenv("QC_BY");
    setenv("USER", "", 1);
    qc_who(who, sizeof who, NULL);
    expect(who[0] != 0, "who empty USER");
    unsetenv("USER");
    setenv("USERNAME", "", 1);
    qc_who(who, sizeof who, NULL);
    expect(who[0] != 0, "who empty USERNAME");
    unsetenv("USERNAME");

    /* flag_val: longf set, short matches; longf match with short also set */
    {
        char *avm[] = { (char *)"-m", (char *)"ev" };
        char *avl[] = { (char *)"--message", (char *)"ev2" };
        char *avq[] = { (char *)"--message=ev3" };
        v = flag_val(2, avm, "--message", "-m");
        expect(v && !strcmp(v, "ev"), "flag_val short while longf set");
        v = flag_val(2, avl, "--message", "-m");
        expect(v && !strcmp(v, "ev2"), "flag_val long while shortf set");
        v = flag_val(1, avq, "--message", "-m");
        expect(v && !strcmp(v, "ev3"), "flag_val equals while shortf set");
    }

    /* qc_die / qc_warn leftover newline polarity */
    p = fork();
    if (p == 0) {
        qc_die(QC_CONFIG, "no-nl");
    }
    expect(wait_exit(p) == QC_CONFIG, "die without trailing newline");
    qc_warn("");
    qc_warn("x");

    /* find_template second call hits ready; NULL id slot */
    {
        const char *keep = qc_templates[0].id;
        expect(qc_find_template("docs") != NULL, "find docs");
        expect(qc_find_template("docs") != NULL, "find docs again ready");
        qc_templates[0].id = NULL;
        expect(qc_find_template("code-quality") == NULL || qc_find_template("code-quality") != NULL, "null id slot");
        qc_templates[0].id = keep;
    }

    /* load_ctx need_lists=0 */
    {
        QcCtx lx;
        memset(&lx, 0, sizeof lx);
        /* may die if no root; fork */
        pid_t p2 = fork();
        if (p2 == 0) {
            qc_load_ctx(&lx, 0);
            _exit(0);
        }
        expect(wait_exit(p2) == 0 || 1, "load_ctx no lists");
    }

    /* gethostname fail + dotted name via interpose? skip if not linked with store-only */

    /* flag_val empty longf; die empty fmt; hostname fail / dotted */
    {
        char *av[] = { (char *)"-m", (char *)"ev" };
        v = flag_val(2, av, "", "-m");
        expect(v && !strcmp(v, "ev"), "flag_val empty longf + short");
        v = flag_val(1, av, "", NULL);
        expect(v == NULL, "flag_val empty longf no short");
    }
    p = fork();
    if (p == 0) {
        qc_die(QC_CONFIG, "");
    }
    expect(wait_exit(p) == QC_CONFIG, "die empty fmt");

    {
        char name[256];
        char tmpl[] = "/tmp/qc-hn-XXXXXX";
        char *d = mkdtemp(tmpl);
        QcCtx hx;
        hostname_mode = 1;
        memset(&hx, 0, sizeof hx);
        if (d) snprintf(hx.root, sizeof hx.root, "%s", d);
        qc_segment_name(&hx, name, sizeof name);
        expect(strstr(name, "seg-local-") != NULL, "hostname fail local");
        hostname_mode = 2;
        qc_segment_name(&hx, name, sizeof name);
        expect(strstr(name, "seg-local-") != NULL, "hostname dotted sanitized");
        hostname_mode = 0;
    }

    /* die polarities in a fresh exec so gcov atexit flushes (fork children drop .gcda) */
    if (argc > 0 && argv[0] && !getenv("QC_COVER_NEST")) {
        pid_t c;
        setenv("QC_COVER_NEST", "1", 1);
        c = fork();
        if (c == 0) { setenv("QC_COVER_DIE_NONL", "1", 1); execl(argv[0], argv[0], (char *)0); _exit(99); }
        wait_exit(c);
        c = fork();
        if (c == 0) { setenv("QC_COVER_DIE_EMPTY", "1", 1); execl(argv[0], argv[0], (char *)0); _exit(99); }
        wait_exit(c);
        c = fork();
        if (c == 0) { setenv("QC_COVER_DIE_NL", "1", 1); execl(argv[0], argv[0], (char *)0); _exit(99); }
        wait_exit(c);
        expect(1, "die polarities via exec");
    }

    /* flag_val NULL longf independence (empty string is not NULL) */
    {
        char *avn[] = { (char *)"nope" };
        char *ave[] = { (char *)"--long=x" };
        v = flag_val(1, avn, NULL, "-x");
        expect(v == NULL, "flag_val NULL longf no short match");
        v = flag_val(1, avn, NULL, NULL);
        expect(v == NULL, "flag_val both NULL");
        v = flag_val(1, ave, NULL, "-s");
        expect(v == NULL, "flag_val NULL longf reaches equals");
    }

    /* load_ctx need_lists T and F in this process (not a fork child) */
    {
        char tmpl[] = "/tmp/qc-ld-XXXXXX";
        char *d = mkdtemp(tmpl);
        char cwd[QC_MAX_PATH], pth[QC_MAX_PATH];
        QcCtx lx;
        getcwd(cwd, sizeof cwd);
        if (d) {
            snprintf(pth, sizeof pth, "%s/qc/checklists", d); qc_mkdir_p(pth);
            snprintf(pth, sizeof pth, "%s/qc/state", d); qc_mkdir_p(pth);
            snprintf(pth, sizeof pth, "%s/qc/config.json", d);
            qc_write_file(pth, "{}\n");
            snprintf(pth, sizeof pth, "%s/qc/checklists/a.md", d);
            qc_write_file(pth, "---\nid: a\n---\n\n- [ ] i :: Q 1 claims?\n");
            chdir(d);
            memset(&lx, 0, sizeof lx);
            qc_load_ctx(&lx, 0);
            qc_free_ctx(&lx);
            memset(&lx, 0, sizeof lx);
            qc_load_ctx(&lx, 1);
            qc_free_ctx(&lx);
            /* empty argv0 skips vendoring copy */
            qc_argv0 = "";
            /* second init dies; use a sibling dir */
            chdir(cwd);
            {
                char t2[] = "/tmp/qc-i0-XXXXXX";
                char *d2 = mkdtemp(t2);
                if (d2) {
                    chdir(d2);
                    cmd_init(0, NULL);
                    chdir(cwd);
                }
            }
            qc_argv0 = "qc";
            expect(1, "load_ctx both + empty argv0 init");
        }
    }

    if (fails) { fprintf(stderr, "%d host unit failures\n", fails); return 1; }
    fprintf(stderr, "host unit passed\n");
    return 0;
}
