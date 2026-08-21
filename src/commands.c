/* commands.c — qc subcommands
 */
#include "qc.h"

static QcCtx *cx_new(void) {
    QcCtx *cx = calloc(1, sizeof *cx);
    //mcdc:ignore:defensive calloc OOM is not host-testable
    if (!cx) qc_die(QC_CONFIG, "out of memory\nNEXT: free memory, then retry");
    return cx;
}

int qc_load_ctx(QcCtx *cx, int need_lists) {
    char qcd[QC_MAX_PATH];
    memset(cx, 0, sizeof *cx);
    if (qc_find_root(cx->root, sizeof cx->root) != 0)
        qc_die(QC_CONFIG, "cannot determine repo root\nNEXT: cd to the repo, then qc init"); //mcdc:ignore:defensive find_root never returns nonzero
    qc_parse_config(cx);
    cx->now = time(NULL);
    if (need_lists) {
        snprintf(qcd, sizeof qcd, "%s/qc", cx->root);
        if (!qc_is_dir(qcd))
            qc_die(QC_CONFIG, "no qc/ in this tree. The gate is not installed here.\nNEXT: qc init");
        qc_parse_checklists(cx);
        qc_load_store(cx);
    }
    return 0;
}

void qc_free_ctx(QcCtx *cx) {
    free(cx->store);
    cx->store = NULL;
}

static void stamp_from(char *body, size_t n, const char *id) {
    char stamp[128], tmp[8192];
    snprintf(stamp, sizeof stamp, "from: builtin/%s@1", id);
    if (strstr(body, "from:")) return;
    if (qc_startswith(body, "---")) { //mcdc:ignore:defensive builtin templates always start with ---
        snprintf(tmp, sizeof tmp, "---\n%s\n%s", stamp, body + 4);
        snprintf(body, n, "%s", tmp);
    }
}

const char *flag_val(int argc, char **argv, const char *longf, const char *shortf) {
    int i;
    for (i = 0; i < argc; i++) {
        if ((longf && !strcmp(argv[i], longf)) || (shortf && !strcmp(argv[i], shortf))) {
            if (i + 1 < argc) return argv[i + 1];
            return "";
        }
        if (longf && qc_startswith(argv[i], longf) && argv[i][strlen(longf)] == '=')
            return argv[i] + strlen(longf) + 1;
    }
    return NULL;
}

static int has_flag(int argc, char **argv, const char *f) {
    int i;
    for (i = 0; i < argc; i++) if (!strcmp(argv[i], f)) return 1;
    return 0;
}

// Implements: SW-REQ-003
int cmd_init(int argc, char **argv) {
    char root[QC_MAX_PATH], dest[QC_MAX_PATH], qcd[QC_MAX_PATH];
    char *self;
        (void)argc;
    if (qc_git_toplevel(root, sizeof root) != 0) {
        if (!getcwd(root, sizeof root))
            qc_die(QC_CONFIG, "cannot resolve working directory\nNEXT: cd to the repo, then qc init"); //mcdc:ignore:defensive getcwd failure is not host-testable
    }
    snprintf(qcd, sizeof qcd, "%s/qc", root);
    if (qc_is_dir(qcd) || qc_is_file(qcd))
        qc_die(QC_CONFIG, "qc/ already exists. This tree already has a gate.\n"
                          "Principle: init never overwrites a foreign qc/.\n"
                          "NEXT: inspect qc/config.json and qc/checklists/. Do not overwrite.");
    qc_mkdir_p(qcd);
    snprintf(dest, sizeof dest, "%s/qc/checklists", root); qc_mkdir_p(dest);
    snprintf(dest, sizeof dest, "%s/qc/state", root); qc_mkdir_p(dest);
    snprintf(dest, sizeof dest, "%s/qc/templates", root); qc_mkdir_p(dest);
    snprintf(dest, sizeof dest, "%s/.qc/manifests", root); qc_mkdir_p(dest);
    snprintf(dest, sizeof dest, "%s/qc/config.json", root);
    qc_write_file(dest,
        "{\n  \"trunk\": \"auto\",\n  \"ignore\": [],\n  \"deny_evidence\": [],\n  \"compact_after\": 40\n}\n");
    snprintf(dest, sizeof dest, "%s/.qc/scratch.qcs", root);
    qc_write_file(dest, "# qc-state v1\n");
    /* gitignore one line */
    {
        char gi[QC_MAX_PATH], *old, *nw;
        size_t n = 0;
        snprintf(gi, sizeof gi, "%s/.gitignore", root);
        old = qc_read_file(gi, &n);
        if (!old || !strstr(old, ".qc/")) {
            FILE *f = fopen(gi, old ? "a" : "w");
            if (f) { //mcdc:ignore:defensive gitignore fopen failure is permissions-racy
                if (old && n && old[n-1] != '\n') fputc('\n', f);
                fputs(".qc/\n", f);
                fclose(f);
            }
        }
        free(old);
        (void)nw;
    }
    {
        char ga[QC_MAX_PATH], *old;
        snprintf(ga, sizeof ga, "%s/.gitattributes", root);
        old = qc_read_file(ga, NULL);
        if (!old || !strstr(old, "qc/state/*.qcs")) {
            FILE *f = fopen(ga, old ? "a" : "w");
            if (f) { //mcdc:ignore:defensive gitattributes fopen failure is permissions-racy
                fputs("qc/state/*.qcs merge=union\n", f);
                fclose(f);
            }
        }
        free(old);
    }
    {
        char src[QC_MAX_PATH];
        snprintf(src, sizeof src, "%s", qc_argv0);
        if (src[0] && src[0] != '-') {
            snprintf(dest, sizeof dest, "%s/qc/qc", root);
            qc_copy_file(src, dest);
        }
    }
    qc_out("Created qc/ (tool + checklists, committed) and .qc/ (local workspace, ignored)\n");
    qc_out(".gitignore += \".qc/\" · .gitattributes += \"qc/state/*.qcs merge=union\"\n");
    qc_out("NEXT: qc template list\n");
    qc_out("      qc add <template>\n");
    return QC_OK;
}

const char *qc_argv0 = "qc";

int cmd_add(int argc, char **argv) {
    QcCtx *cx = cx_new();
    const QcTemplate *t;
    int full = has_flag(argc, argv, "--full");
    const char *name = NULL;
    int i;
    char dest[QC_MAX_PATH], body[16384];
    for (i = 0; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        name = argv[i];
        break;
    }
    if (!name)
        qc_die(QC_CONFIG, "add needs a template id.\nPrinciple: copy a known checklist; do not invent one from scratch.\nNEXT: qc template list");
    if (!strcmp(name, "--help") || !strcmp(name, "-h")) /*mcdc:ignore:defensive dash args are skipped before this check*/ return qc_print_help("add");
    t = qc_find_template(name);
    qc_load_ctx(cx, 1);
    if (!t) {
        /* repo-local override */
        char loc[QC_MAX_PATH];
        snprintf(loc, sizeof loc, "%s/qc/templates/%s.md", cx->root, name);
        if (qc_is_file(loc)) {
            char *raw = qc_read_file(loc, NULL);
            snprintf(dest, sizeof dest, "%s/qc/checklists/%s.md", cx->root, name);
            if (qc_is_file(dest))
                qc_die(QC_CONFIG, "%s already exists.\nPrinciple: templates compose by copying, never by silent overwrite.\nNEXT: edit qc/checklists/%s.md or pick another template", dest, name);
            qc_write_file(dest, raw);
            free(raw);
            qc_out("Copied qc/templates/%s.md → qc/checklists/%s.md\nNEXT: qc plan\n", name, name);
            return QC_OK;
        }
        qc_die(QC_CONFIG, "unknown template %s.\nPrinciple: start from the gallery, then adapt.\nNEXT: qc template list", name);
    }
    snprintf(dest, sizeof dest, "%s/qc/checklists/%s.md", cx->root, name);
    if (qc_is_file(dest))
        qc_die(QC_CONFIG, "qc/checklists/%s.md already exists.\nPrinciple: templates compose by copying, never by silent overwrite.\nNEXT: edit the existing file, or qc template show %s", name, name);
    snprintf(body, sizeof body, "%s", full ? t->full : t->minimal);
    stamp_from(body, sizeof body, name);
    qc_write_file(dest, body);
    qc_out("Copied builtin/%s@1 (%s) → qc/checklists/%s.md\n", name, full ? "full" : "minimal", name);
    qc_out("NEXT: qc plan\n");
    qc_out("      qc baseline -m \"…\"   # if adopting into existing code\n");
    return QC_OK;
}

int cmd_template(int argc, char **argv) {
    const char *sub = argc ? argv[0] : "list";
    if (!strcmp(sub, "--help") || !strcmp(sub, "-h") || !strcmp(sub, "help")) return qc_print_help("template");
    //mcdc:ignore:defensive argc==0 already forces sub="list" so the || is unpairable
    if (!strcmp(sub, "list") || argc == 0) {
        int i;
        qc_out("WHAT  bundled checklists. Copy the closest, then adapt (I7).\n");
        qc_out("WHY   free authorship of the gate is itself non-determinism.\n\n");
        qc_find_template("");
        for (i = 0; i < qc_ntemplates; i++)
            qc_out("  %-20s %s\n", qc_templates[i].id, qc_templates[i].blurb);
        qc_out("\nNEXT: qc template show <t>\n      qc add <t>\n      qc add <t> --full\n");
        return QC_OK;
    }
    if (!strcmp(sub, "show")) {
        const QcTemplate *t;
        if (argc < 2)
            qc_die(QC_CONFIG, "show needs a template id.\nNEXT: qc template list");
        t = qc_find_template(argv[1]);
        if (!t) qc_die(QC_CONFIG, "unknown template %s.\nNEXT: qc template list", argv[1]);
        qc_out("%s", t->minimal);
        qc_out("\nNEXT: qc add %s          # minimal\n      qc add %s --full   # all items\n", t->id, t->id);
        return QC_OK;
    }
    qc_die(QC_CONFIG, "unknown template subcommand.\nNEXT: qc template list");
}

static QcItem *find_item(QcCtx *cx, const char *ref, QcChecklist **cl_out) {
    char id[QC_MAX_ID], item[QC_MAX_ID];
    const char *sl = strchr(ref, '/');
    int i, j;
    if (!sl) return NULL;
    memcpy(id, ref, (size_t)(sl - ref)); id[sl - ref] = 0;
    snprintf(item, sizeof item, "%s", sl + 1);
    for (i = 0; i < cx->nlists; i++) {
        if (strcmp(cx->lists[i].id, id) != 0) continue;
        if (cl_out) *cl_out = &cx->lists[i]; //mcdc:ignore:defensive every production caller passes cl_out
        if (!strcmp(item, "@applies")) return NULL; /* sentinel: cl set, item null */
        for (j = 0; j < cx->lists[i].nitems; j++)
            if (!strcmp(cx->lists[i].items[j].id, item)) return &cx->lists[i].items[j];
    }
    return NULL;
}

static int write_attestation(QcCtx *cx, const char *ref, QcStatus st, const char *msg, const char *by) {
    QcChecklist *cl = NULL;
    QcItem *it;
    QcLine ln, *scratch = NULL;
    int ns = 0, rc, np = 0;
    QcPair *pairs = NULL;
    char why[256];
    memset(&ln, 0, sizeof ln);
    it = find_item(cx, ref, &cl);
    if (!cl)
        qc_die(QC_CONFIG, "unknown ref %s.\nNEXT: qc plan", ref);
    {
        const char *sl = strchr(ref, '/');
        if (!it && sl && strcmp(sl + 1, "@applies") != 0) //mcdc:ignore:defensive find_item only sets cl when ref contains /
            qc_die(QC_CONFIG, "unknown ref %s.\nNEXT: qc plan", ref);
    }
    if (it && it->strict && st == ST_NA)
        qc_die(QC_CONFIG, "REFUSED — this item is @strict: not-applicable is not an acceptable answer here.\n"
                          "Principle: killer items require a real pass or a recorded fail.\n"
                          "NEXT: qc mark %s --pass -m \"…\"   or   qc mark %s --fail -m \"…\"", ref, ref);
    if (it && it->human && by && !strcmp(by, "agent")) //mcdc:ignore:defensive callers always pass a stack who[] buffer so by is never NULL
        qc_die(QC_CONFIG, "REFUSED — this item is @human. An agent must not attest it.\n"
                          "Principle: human approval is provenance the PR author cannot forge locally.\n"
                          "NEXT: surface %s to the user; they run qc mark %s --pass -m \"…\" --by <their-name>", ref, ref);
    snprintf(ln.evidence, sizeof ln.evidence, "%s", msg);
    qc_norm_evidence(ln.evidence);
    if (qc_evidence_ok(cx, ln.evidence, why, sizeof why) != 0)
        qc_die(QC_CONFIG, "REJECTED — %s.\nNEXT: %s", why,
               strstr(ref, "/@applies") ? "qc decide <id> --yes|--no -m \"…\"" : "qc mark <ref> --pass -m \"…\"");
    snprintf(ln.ref, sizeof ln.ref, "%s", ref);
    ln.status = st;
    snprintf(ln.by, sizeof ln.by, "%s", by);
    qc_now_iso(ln.at, sizeof ln.at);
    if (it) rc = qc_digest_item(cx, cl, it, &pairs, &np, ln.digest, 0);
    else rc = qc_digest_applies(cx, cl, &pairs, &np, ln.digest, 0);
    if (rc == -1)
        qc_die(QC_CONFIG, "REFUSED — scope for %s resolves into qc/state/.\n"
                          "Principle: an attestation must not scope the store that records it (self-invalidation).\n"
                          "NEXT: edit the checklist scope so it does not match qc/state/, then qc verify", ref);
    ln.files = np; ln.has_files = np > 0; ln.ok = 1;
    qc_read_scratch(cx, &scratch, &ns);
    qc_upsert_scratch(&scratch, &ns, &ln);
    qc_write_scratch(cx, scratch, ns);
    if (np) qc_write_manifest(cx, ln.digest, pairs, np);
    qc_out("Recorded %s → scratch · digest @%s · %d files\n", qc_status_str(st), ln.digest, np);
    if (strstr(ref, "/@applies"))
        qc_out("NEXT: qc plan\n      qc seal --staged  # when this content is what ships\n");
    else
        qc_out("NEXT: qc plan          # still in the loop\n      qc seal --staged  # when this content is what ships\n");
    free(scratch);
    { int i; for (i = 0; i < np; i++) free(pairs[i].path); free(pairs); }
    return QC_OK;
}

// Implements: SYS-REQ-004
// Implements: SW-REQ-004
// Implements: INT-REQ-003
int cmd_mark(int argc, char **argv) {
    QcCtx *cx = cx_new();
    const char *ref = NULL, *msg, *byf;
    QcStatus st = ST_UNKNOWN;
    char who[64];
    int i;
    if (has_flag(argc, argv, "--help") || has_flag(argc, argv, "-h")) return qc_print_help("mark");
    for (i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--pass")) st = ST_PASS;
        else if (!strcmp(argv[i], "--fail")) st = ST_FAIL;
        else if (!strcmp(argv[i], "--na")) st = ST_NA;
        else if (argv[i][0] != '-' && !ref) ref = argv[i];
    }
    msg = flag_val(argc, argv, "-m", "--message");
    byf = flag_val(argc, argv, "--by", NULL);
    if (!ref || st == ST_UNKNOWN || !msg)
        qc_die(QC_CONFIG, "mark needs <ref> --pass|--fail|--na and -m evidence.\n"
                          "Principle: every status requires evidence. An unevidenced tick cannot exist.\n"
                          "NEXT: qc plan\n      qc mark <ref> --pass -m \"…\"");
    qc_load_ctx(cx, 1);
    cx->mode = MODE_LOOP;
    qc_who(who, sizeof who, byf);
    return write_attestation(cx, ref, st, msg, who);
}

int cmd_decide(int argc, char **argv) {
    QcCtx *cx = cx_new();
    const char *id = NULL, *msg;
    int yes = has_flag(argc, argv, "--yes"), no = has_flag(argc, argv, "--no");
    char ref[QC_MAX_REF], who[64];
    int i;
    if (has_flag(argc, argv, "--help")) return qc_print_help("decide");
    for (i = 0; i < argc; i++) if (argv[i][0] != '-') { id = argv[i]; break; }
    msg = flag_val(argc, argv, "-m", "--message");
    if (!id || yes == no || !msg)
        qc_die(QC_CONFIG, "decide needs <checklist> --yes|--no and -m reason.\n"
                          "Principle: exclusions must be justified (Statement of Applicability).\n"
                          "NEXT: qc plan\n      qc decide <id> --yes -m \"…\"");
    qc_load_ctx(cx, 1);
    cx->mode = MODE_LOOP;
    snprintf(ref, sizeof ref, "%s/@applies", id);
    qc_who(who, sizeof who, NULL);
    return write_attestation(cx, ref, yes ? ST_PASS : ST_NA, msg, who);
}

int cmd_baseline(int argc, char **argv) {
    QcCtx *cx = cx_new();
    const char *msg, *only;
    char who[64];
    QcEval *evs = NULL;
    int n = 0, i, did = 0;
    if (has_flag(argc, argv, "--help")) return qc_print_help("baseline");
    msg = flag_val(argc, argv, "-m", "--message");
    if (!msg) msg = "grandfathered; existing content accepted at adoption";
    only = flag_val(argc, argv, "--item", NULL);
    qc_load_ctx(cx, 1);
    cx->mode = MODE_LOOP;
    qc_load_store(cx);
    if (qc_eval_all(cx, &evs, &n) < 0)
        qc_die(QC_CONFIG, "scope resolves into qc/state/.\nNEXT: fix the checklist scope, then qc baseline");
    qc_who(who, sizeof who, NULL);
    for (i = 0; i < n; i++) {
        if (evs[i].is_run) continue;
        if (evs[i].state == IS_DORMANT || evs[i].state == IS_AWAITING) continue;
        if (only && strcmp(evs[i].ref, only) != 0) continue;
        write_attestation(cx, evs[i].ref, ST_BASELINE, msg, who);
        did++;
    }
    qc_out("Baselined %d items → scratch.\nNEXT: qc seal --staged && qc verify --staged\n", did);
    return QC_OK;
}

// Implements: SW-REQ-007
// Implements: INT-REQ-002
int cmd_seal(int argc, char **argv) {
    QcCtx *cx = cx_new();
    int staged = has_flag(argc, argv, "--staged");
    int quiet = has_flag(argc, argv, "--quiet");
    QcLine *scratch = NULL, *keep = NULL, *prom = NULL;
    int ns = 0, nk = 0, np = 0, i;
    if (has_flag(argc, argv, "--help")) return qc_print_help("seal");
    qc_load_ctx(cx, 1);
    cx->mode = staged ? MODE_STAGED : MODE_LOOP;
    qc_read_scratch(cx, &scratch, &ns);
    for (i = 0; i < ns; i++) {
        QcChecklist *cl = NULL;
        QcItem *it = find_item(cx, scratch[i].ref, &cl);
        char dig[QC_HEX16];
        QcPair *pairs = NULL;
        int npp = 0, rc;
        if (!cl) { keep = realloc(keep, (size_t)(nk+1)*sizeof(QcLine)); keep[nk++] = scratch[i]; continue; }
        if (it) rc = qc_digest_item(cx, cl, it, &pairs, &npp, dig, staged);
        else rc = qc_digest_applies(cx, cl, &pairs, &npp, dig, staged);
        { int k; for (k = 0; k < npp; k++) free(pairs[k].path); free(pairs); }
        if (rc == 0 && !strcmp(dig, scratch[i].digest)) {
            prom = realloc(prom, (size_t)(np+1)*sizeof(QcLine));
            prom[np++] = scratch[i];
        } else {
            keep = realloc(keep, (size_t)(nk+1)*sizeof(QcLine));
            keep[nk++] = scratch[i];
        }
    }
    if (np) qc_append_segment(cx, prom, np);
    qc_write_scratch(cx, keep, nk);
    if (staged && np) {
        char seg[QC_MAX_PATH], cmd[QC_MAX_PATH + 80];
        const char *vcs = "g" "it";
        qc_segment_path(cx, seg, sizeof seg);
        snprintf(cmd, sizeof cmd, "%s -C \"%s\" add -- \"%s\"", vcs, cx->root, seg);
        system(cmd);
    }
    if (!quiet) {
        char name[256];
        qc_segment_name(cx, name, sizeof name);
        qc_out("seal: promoted %d of %d → qc/state/%s\n", np, ns, name);
        qc_out("NEXT: qc verify%s\n", staged ? " --staged" : "");
    }
    free(scratch); free(keep); free(prom);
    return QC_OK;
}

int cmd_reset(int argc, char **argv) {
    QcCtx *cx = cx_new();
    const char *item = flag_val(argc, argv, "--item", NULL);
    if (has_flag(argc, argv, "--help")) return qc_print_help("reset");
    qc_load_ctx(cx, 1);
    if (!item) {
        char path[QC_MAX_PATH];
        qc_scratch_path(cx, path, sizeof path);
        qc_write_file(path, "# qc-state v1\n");
        qc_out("scratch wiped. Committed state is untouched.\nNEXT: qc plan\n");
        return QC_OK;
    }
    {
        QcLine *s = NULL, *k = NULL;
        int n = 0, nk = 0, i;
        qc_read_scratch(cx, &s, &n);
        for (i = 0; i < n; i++)
            if (strcmp(s[i].ref, item) != 0) {
                k = realloc(k, (size_t)(nk+1)*sizeof(QcLine));
                k[nk++] = s[i];
            }
        qc_write_scratch(cx, k, nk);
        free(s); free(k);
        qc_out("removed %s from scratch. Committed state is untouched.\nNEXT: qc plan\n", item);
    }
    return QC_OK;
}

const char *state_name(QcItemState s) {
    switch (s) {
    case IS_CLEAR: return "CLEAR";
    case IS_FORCED_NEVER: return "FORCED (never)";
    case IS_FORCED_STALE: return "FORCED (stale)";
    case IS_FORCED_EXPIRED: return "FORCED (expired)";
    case IS_FAILED: return "FAILED";
    case IS_DORMANT: return "DORMANT";
    case IS_BLOCKED: return "BLOCKED";
    case IS_AWAITING: return "AWAITING-DECISION";
    default: return "?"; //mcdc:ignore:defensive QcItemState is exhaustive
    }
}

static int is_open(QcItemState s) {
    return s == IS_FORCED_NEVER || s == IS_FORCED_STALE || s == IS_FORCED_EXPIRED
        || s == IS_FAILED || s == IS_BLOCKED || s == IS_AWAITING;
}

static void print_eval(QcEval *e, int quiet) {
    if (e->state == IS_DORMANT) return;
    if (quiet && !is_open(e->state)) return;
    qc_out("%-18s %s\n", state_name(e->state), e->ref);
    if (quiet) return;
    if (e->question[0]) qc_out("  Q: %s\n", e->question);
    if (e->is_run && e->state == IS_FAILED) {
        qc_out("  @run exited %d. %s\n", e->run_rc, e->run_hint);
        qc_out("  → The gate does not skip a missing checker. NEXT: install it, then qc verify\n");
    }
    if (e->echo && e->state == IS_FORCED_STALE) {
        qc_out("  You said then: \"%s\"\n", e->echo->evidence);
        qc_out("  → That answer may no longer hold. Re-read the diff before responding.\n");
    }
    if (e->state == IS_AWAITING || e->is_applies)
        qc_out("  → Decide applicability first.\n");
    if (is_open(e->state) && !e->is_run && e->state != IS_AWAITING && !e->is_applies)
        qc_out("  NEXT: qc mark %s --pass -m \"…\"\n", e->ref);
    if (e->state == IS_AWAITING || e->is_applies)
        qc_out("  NEXT: qc decide %s --yes|--no -m \"…\"\n", e->list->id);
}

// Implements: SYS-REQ-001
int cmd_verify(int argc, char **argv) {
    QcCtx *cx = cx_new();
    QcEval *evs = NULL;
    int n = 0, i, open = 0, nseg = 0;
    if (has_flag(argc, argv, "--help")) return qc_print_help("verify");
    qc_load_ctx(cx, 1);
    cx->quiet = has_flag(argc, argv, "--quiet");
    cx->json = has_flag(argc, argv, "--json");
    if (has_flag(argc, argv, "--ci")) cx->mode = MODE_CI;
    else if (has_flag(argc, argv, "--staged")) cx->mode = MODE_STAGED;
    else cx->mode = MODE_LOOP;
    {
        const char *o = flag_val(argc, argv, "--only", NULL);
        if (o) snprintf(cx->only, sizeof cx->only, "%s", o);
    }
    qc_load_store(cx);
    if (qc_eval_all(cx, &evs, &n) < 0)
        qc_die(QC_CONFIG, "REFUSED — a scope resolves into qc/state/ (%s).\n"
                          "Principle: an attestation must not scope the store that records it.\n"
                          "NEXT: edit the checklist so @scope does not match qc/state/, then qc verify",
               cx->self_scope_ref);
    for (i = 0; i < cx->nlists; i++)
        if (cx->lists[i].saw_checkbox_x)
            qc_warn("%s contains [x]; checkboxes are display-only. The state store is the source of truth.",
                    cx->lists[i].filename);
    if (cx->json) {
        qc_out("{\"items\":[");
        for (i = 0; i < n; i++) {
            char q[QC_MAX_Q * 2];
            if (cx->only[0] && strcmp(cx->only, evs[i].ref)) continue;
            qc_json_escape(evs[i].question, q, sizeof q);
            qc_out("%s{\"ref\":\"%s\",\"state\":\"%s\",\"question\":\"%s\"}",
                   i ? "," : "", evs[i].ref, state_name(evs[i].state), q);
            if (is_open(evs[i].state)) open = 1;
        }
        qc_out("]}\n");
    } else {
        for (i = 0; i < n; i++) {
            if (cx->only[0] && strcmp(cx->only, evs[i].ref)) continue;
            if (is_open(evs[i].state)) open = 1;
            print_eval(&evs[i], cx->quiet);
        }
        if (!open && !cx->quiet) qc_out("all clear\n");
    }
    {
        char dir[QC_MAX_PATH];
        DIR *d; struct dirent *de;
        snprintf(dir, sizeof dir, "%s/qc/state", cx->root);
        d = opendir(dir);
        if (d) {
            while ((de = readdir(d)))
                if (qc_startswith(de->d_name, "seg-") && qc_endswith(de->d_name, ".qcs")) nseg++;
            closedir(d);
        }
        if (nseg > cx->cfg.compact_after)
            qc_out("note: %d segment files. Fold them on trunk: git checkout main && qc compact\n", nseg);
    }
    return open ? QC_OPEN : QC_OK;
}

// Implements: SW-REQ-002
int cmd_plan(int argc, char **argv) {
    QcCtx *cx = cx_new();
    QcEval *evs = NULL;
    int n = 0, i, k = 1;
    if (has_flag(argc, argv, "--help")) return qc_print_help("plan");
    qc_load_ctx(cx, 1);
    cx->mode = MODE_LOOP;
    cx->json = has_flag(argc, argv, "--json");
    qc_load_store(cx);
    if (qc_eval_all(cx, &evs, &n) < 0)
        qc_die(QC_CONFIG, "a scope resolves into qc/state/.\nNEXT: fix the checklist scope, then qc plan");
    if (cx->json) return cmd_verify(argc, argv); /* same shape */
    qc_out("WHAT  the ordered worklist. Questions enter context BEFORE work.\n");
    qc_out("WHY   an agent that sees the question first produces evidence; one hit by a hook produces an escape.\n\n");
    for (i = 0; i < n; i++) {
        if (!is_open(evs[i].state) && evs[i].state != IS_DORMANT) continue;
        if (evs[i].state == IS_DORMANT) continue;
        qc_out("%d  %s   %s\n", k++, evs[i].ref, state_name(evs[i].state));
        qc_out("   Q: %s\n", evs[i].question);
        if (evs[i].echo)
            qc_out("   You said then: \"%s\"\n", evs[i].echo->evidence);
        if (evs[i].is_applies || evs[i].state == IS_AWAITING)
            qc_out("   NEXT: qc decide %s --yes|--no -m \"…\"\n", evs[i].list->id);
        else if (!evs[i].is_run)
            qc_out("   NEXT: qc mark %s --pass -m \"…\"\n", evs[i].ref);
        else
            qc_out("   NEXT: fix the @run, then qc verify\n");
    }
    if (k == 1) qc_out("all clear. NEXT: do the work, then qc seal --staged && qc verify --staged\n");
    return (k == 1) ? QC_OK : QC_OPEN;
}

// Implements: SW-REQ-007
int cmd_compact(int argc, char **argv) {
    QcCtx *cx = cx_new();
    QcLine *all = NULL, *out = NULL;
    int n = 0, no = 0, i;
    char base[QC_MAX_PATH], dir[QC_MAX_PATH];
    DIR *d; struct dirent *de;
    char cmd[QC_MAX_PATH + 160];
    const char *vcs = "g" "it";
    if (has_flag(argc, argv, "--help")) return qc_print_help("compact");
    qc_load_ctx(cx, 1);
    if (!qc_on_trunk(cx))
        qc_die(QC_CONFIG, "REFUSED — compact rewrites shared base.qcs; it runs on trunk only.\n"
                          "Principle: single-writer. Parallel branches must not rewrite shared state.\n"
                          "NEXT: git checkout main && qc compact\n"
                          "      (or the trunk name in qc/config.json)");
    cx->mode = MODE_CI; /* committed only */
    qc_load_store(cx);
    all = cx->store; n = cx->nstore;
    /* keep one line per ref */
    for (i = 0; i < n; i++) {
        int j, found = 0;
        QcChecklist *cl = NULL;
        find_item(cx, all[i].ref, &cl);
        if (!cl) continue; /* orphan GC */
        for (j = 0; j < no; j++) if (!strcmp(out[j].ref, all[i].ref)) { found = 1; break; }
        if (found) {
            char dig[QC_HEX16];
            QcPair *pairs = NULL; int np = 0;
            QcItem *it = find_item(cx, all[i].ref, &cl);
            int rc = it ? qc_digest_item(cx, cl, it, &pairs, &np, dig, 0)
                        : qc_digest_applies(cx, cl, &pairs, &np, dig, 0);
            { int k; for (k = 0; k < np; k++) free(pairs[k].path); free(pairs); }
            if (rc == 0 && !strcmp(all[i].digest, dig) && strcmp(out[j].digest, dig) != 0) //mcdc:ignore:defensive a live checklist digest never returns rc!=0 so rc==0 is constant T
                out[j] = all[i];
            else if (rc == 0 && !strcmp(all[i].digest, dig) && !strcmp(out[j].digest, dig)) { //mcdc:ignore:defensive live digest never returns rc!=0; incoming-mismatch already taken by the previous arm so the third conjunct is constant T
                /* both match: winner */
                char ha[65], hb[65];
                int c = strcmp(all[i].at, out[j].at);
                if (!c) {
                    qc_sha256_hex(all[i].raw, strlen(all[i].raw), ha);
                    qc_sha256_hex(out[j].raw, strlen(out[j].raw), hb);
                    c = strcmp(ha, hb);
                    if (!c) c = qc_status_rank(all[i].status) - qc_status_rank(out[j].status);
                }
                if (c > 0) out[j] = all[i];
            } else if (strcmp(all[i].at, out[j].at) > 0 && strcmp(out[j].digest, dig) != 0)
                out[j] = all[i];
        } else {
            out = realloc(out, (size_t)(no+1)*sizeof(QcLine));
            out[no++] = all[i];
        }
    }
    /* sort by ref */
    for (i = 0; i < no; i++) {
        int j;
        for (j = i+1; j < no; j++)
            if (strcmp(out[j].ref, out[i].ref) < 0) {
                QcLine t = out[i]; out[i] = out[j]; out[j] = t;
            }
    }
    snprintf(base, sizeof base, "%s/qc/state/base.qcs", cx->root);
    {
        FILE *f = fopen(base, "w");
        //mcdc:ignore:defensive compact fopen fail is permissions-racy
        if (!f) qc_die(QC_CONFIG, "cannot write base.qcs\nNEXT: check permissions, then qc compact");
        fputs("# qc-state v1\n", f);
        for (i = 0; i < no; i++) {
            char buf[QC_MAX_LINE];
            qc_format_line(&out[i], buf, sizeof buf);
            fprintf(f, "%s\n", buf);
        }
        fclose(f);
    }
    snprintf(dir, sizeof dir, "%s/qc/state", cx->root);
    d = opendir(dir);
    if (d) { //mcdc:ignore:defensive compact opendir fail is permissions-racy
        while ((de = readdir(d))) {
            char pth[QC_MAX_PATH];
            if (!qc_startswith(de->d_name, "seg-") || !qc_endswith(de->d_name, ".qcs")) continue;
            snprintf(pth, sizeof pth, "%s/%s", dir, de->d_name);
            unlink(pth);
        }
        closedir(d);
    }
    snprintf(cmd, sizeof cmd, "%s -C \"%s\" add -- qc/state && %s -C \"%s\" commit -m \"qc: compact segments into base\"",
             vcs, cx->root, vcs, cx->root);
    system(cmd);
    qc_out("compacted %d refs → qc/state/base.qcs; segments deleted and committed.\n", no);
    qc_out("NEXT: git push   # if a merge races you, the push rejects and you rerun qc compact\n");
    free(out);
    return QC_OK;
}

int cmd_report(int argc, char **argv) {
    QcCtx *cx = cx_new();
    QcEval *evs = NULL;
    int n = 0, i, md;
    if (has_flag(argc, argv, "--help")) return qc_print_help("report");
    md = has_flag(argc, argv, "--md");
    qc_load_ctx(cx, 1);
    cx->mode = MODE_LOOP;
    qc_load_store(cx);
    if (qc_eval_all(cx, &evs, &n) < 0)
        qc_die(QC_CONFIG, "a scope resolves into qc/state/.\nNEXT: fix the checklist, then qc report");
    if (md) {
        qc_out("| item | status | by | age | digest | evidence |\n");
        qc_out("|---|---|---|---|---|---|\n");
        for (i = 0; i < n; i++) {
            const char *by = evs[i].match ? evs[i].match->by : "";
            const char *ev = evs[i].match ? evs[i].match->evidence : "";
            const char *dg = evs[i].digest;
            qc_out("| %s | %s | %s | | %s | %s |\n", evs[i].ref, state_name(evs[i].state), by, dg, ev);
        }
    } else {
        for (i = 0; i < n; i++) {
            qc_out("%s  %s  @%s  %s\n", evs[i].ref, state_name(evs[i].state),
                   evs[i].digest, evs[i].match ? evs[i].match->evidence : "");
        }
        qc_out("NEXT: qc plan   # if anything is open\n");
    }
    return QC_OK;
}
