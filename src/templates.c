/* templates.c — bundled checklists
 */
#include "qc.h"

#define FM(id, when, scope, exp) \
    "---\nid: " id "\napplies_when: " when "\nscope: [" scope "]\nexpires: " exp "\n---\n\n"

static const char *cq_min =
    "---\nid: code-quality\n"
    "applies_when: The change modifies application behavior (not comments/docs only)\n"
    "scope: [src/**]\nexpires: 30d\n---\n\n"
    "- [ ] lint      :: Is the linter clean? @run(ruff check .)\n"
    "- [ ] types     :: Is the type checker clean? @run(mypy src/)\n"
    "- [ ] api-shape :: Which public symbols changed, and which are breaking? @scope(src/**)\n"
    "- [ ] deps      :: Which new dependencies were added, and who maintains them? @scope(pyproject.toml, uv.lock)\n"
    "- [ ] naming    :: Which names would confuse a stranger reading this diff?\n"
    "- [ ] diff-read :: What did you find reading the full diff, top to bottom? @human @scope(src/**)\n";

static const char *cq_full =
    "---\nid: code-quality\n"
    "applies_when: The change modifies application behavior (not comments/docs only)\n"
    "scope: [src/**]\nexpires: 30d\n---\n\n"
    "- [ ] lint      :: Is the linter clean? @run(ruff check .)\n"
    "- [ ] types     :: Is the type checker clean? @run(mypy src/)\n"
    "- [ ] api-shape :: Which public symbols changed, and which are breaking? @scope(src/**)\n"
    "- [ ] deps      :: Which new dependencies were added, and who maintains them? @scope(pyproject.toml, uv.lock)\n"
    "- [ ] naming    :: Which names would confuse a stranger reading this diff?\n"
    "- [ ] errors    :: Which new failure modes did you add, and how are they surfaced?\n"
    "- [ ] tests     :: Which tests cover the behavior you changed?\n"
    "- [ ] diff-read :: What did you find reading the full diff, top to bottom? @human @scope(src/**)\n";

static const char *api_min =
    "---\nid: api-change\n"
    "applies_when: The change touches a public API or contract\n"
    "scope: [src/**]\nexpires: 14d\n---\n\n"
    "- [ ] breaking :: Which responses, fields, or routes become incompatible?\n"
    "- [ ] additive :: What was added, and is it backward compatible?\n"
    "- [ ] tests    :: Which contract tests cover the new surface?\n";

static const char *api_full =
    "---\nid: api-change\n"
    "applies_when: The change touches a public API or contract\n"
    "scope: [src/**]\nexpires: 14d\n---\n\n"
    "- [ ] breaking :: Which responses, fields, or routes become incompatible?\n"
    "- [ ] additive :: What was added, and is it backward compatible?\n"
    "- [ ] docs     :: Where is the change documented for callers?\n"
    "- [ ] tests    :: Which contract tests cover the new surface?\n"
    "- [ ] version  :: Which version bump does this require, and why?\n";

static const char *sec_min =
    "---\nid: security-review\n"
    "applies_when: The change touches auth, secrets, or untrusted input\n"
    "scope: [src/**]\nexpires: 14d\n---\n\n"
    "- [ ] authz-paths :: Which authorization checks did you re-read?\n"
    "- [ ] secrets     :: Which new secret or credential surfaces exist?\n"
    "- [ ] input       :: Which untrusted inputs reach this change?\n";

static const char *sec_full =
    "---\nid: security-review\n"
    "applies_when: The change touches auth, secrets, or untrusted input\n"
    "scope: [src/**]\nexpires: 14d\n---\n\n"
    "- [ ] authz-paths :: Which authorization checks did you re-read?\n"
    "- [ ] secrets     :: Which new secret or credential surfaces exist?\n"
    "- [ ] input       :: Which untrusted inputs reach this change?\n"
    "- [ ] deps-cve    :: Which dependency advisories apply to this bump?\n"
    "- [ ] logging     :: What sensitive data could leak through new logs?\n";

static const char *mig_min =
    "---\nid: data-migration\n"
    "applies_when: The change migrates stored data or schema\n"
    "scope: [src/**]\nexpires: 7d\n---\n\n"
    "- [ ] reversible :: Can this migration be reversed without data loss? @strict\n"
    "- [ ] backfill   :: How is existing data backfilled, and what is the failure mode?\n"
    "- [ ] downtime   :: What is the downtime window and who approved it?\n"
    "- [ ] rollback   :: How did you test the rollback? @strict\n";

static const char *mig_full =
    "---\nid: data-migration\n"
    "applies_when: The change migrates stored data or schema\n"
    "scope: [src/**]\nexpires: 7d\n---\n\n"
    "- [ ] reversible :: Can this migration be reversed without data loss? @strict\n"
    "- [ ] backfill   :: How is existing data backfilled, and what is the failure mode?\n"
    "- [ ] downtime   :: What is the downtime window and who approved it?\n"
    "- [ ] rollback   :: How did you test the rollback? @strict\n"
    "- [ ] expand     :: Did you expand-contract so old binaries keep working?\n";

static const char *dep_min =
    "---\nid: dependency-update\n"
    "applies_when: The change adds or bumps a dependency\n"
    "scope: [src/**]\nexpires: 30d\n---\n\n"
    "- [ ] why    :: Why this version, and what does the changelog say is dangerous?\n"
    "- [ ] compat :: Which of our call sites break?\n"
    "- [ ] lock   :: Does the lockfile change match the intended bump? @scope(uv.lock)\n";

static const char *dep_full =
    "---\nid: dependency-update\n"
    "applies_when: The change adds or bumps a dependency\n"
    "scope: [src/**]\nexpires: 30d\n---\n\n"
    "- [ ] why     :: Why this version, and what does the changelog say is dangerous?\n"
    "- [ ] compat  :: Which of our call sites break?\n"
    "- [ ] license :: What license and who maintains it?\n"
    "- [ ] lock    :: Does the lockfile change match the intended bump? @scope(uv.lock)\n";

static const char *rel_min =
    "---\nid: release\n"
    "applies_when: This change is a release or cut\n"
    "scope: []\nexpires: 7d\n---\n\n"
    "- [ ] notes    :: What does the user-facing changelog say?\n"
    "- [ ] version  :: Which version bump and why?\n"
    "- [ ] rollback :: How do we revert this release?\n";

static const char *rel_full =
    "---\nid: release\n"
    "applies_when: This change is a release or cut\n"
    "scope: []\nexpires: 7d\n---\n\n"
    "- [ ] notes    :: What does the user-facing changelog say?\n"
    "- [ ] version  :: Which version bump and why?\n"
    "- [ ] migrate  :: What must operators do?\n"
    "- [ ] rollback :: How do we revert this release?\n";

static const char *docs_min =
    "---\nid: docs\n"
    "from: builtin/docs@1\n"
    "applies_when: The change is documentation\n"
    "scope: [docs/**, README.md]\nexpires: 60d\n---\n\n"
    "- [ ] accuracy :: Which claims in the doc did you verify against the code?\n"
    "- [ ] stale    :: Which old instructions did you remove?\n";

static const char *docs_full =
    "---\nid: docs\n"
    "from: builtin/docs@1\n"
    "applies_when: The change is documentation\n"
    "scope: [docs/**, README.md]\nexpires: 60d\n---\n\n"
    "- [ ] accuracy :: Which claims in the doc did you verify against the code?\n"
    "- [ ] stale    :: Which old instructions did you remove?\n"
    "- [ ] links    :: Which links did you click, and which failed?\n";

static const char *ai_min =
    "---\nid: ai-generated-code\n"
    "applies_when: An agent authored or substantially edited this change\n"
    "scope: [src/**]\nexpires: 7d\n---\n\n"
    "- [ ] apis-real    :: Against which official docs did you verify each API you used (not memory)?\n"
    "- [ ] no-extra     :: Which files are in the diff that are unrelated to the ask?\n"
    "- [ ] tests-honest :: Which tests were deleted or weakened, and why is that justified?\n"
    "- [ ] no-silence   :: Which lint or type suppressions were added?\n";

static const char *ai_full =
    "---\nid: ai-generated-code\n"
    "applies_when: An agent authored or substantially edited this change\n"
    "scope: [src/**]\nexpires: 7d\n---\n\n"
    "- [ ] apis-real    :: Against which official docs did you verify each API you used (not memory)?\n"
    "- [ ] no-extra     :: Which files are in the diff that are unrelated to the ask?\n"
    "- [ ] tests-honest :: Which tests were deleted or weakened, and why is that justified?\n"
    "- [ ] no-silence   :: Which lint or type suppressions were added?\n"
    "- [ ] scope-tight  :: Which files did you touch that a human would not have needed?\n";

QcTemplate qc_templates[8];
int qc_ntemplates = 8;

// Implements: SW-REQ-003
static void templates_init(void) {
    static int ready;
    if (ready) return;
    qc_templates[0].id = "code-quality";
    qc_templates[0].blurb = "quality questions for application code";
    qc_templates[0].minimal = cq_min;
    qc_templates[0].full = cq_full;
    qc_templates[1].id = "api-change";
    qc_templates[1].blurb = "public surface and compatibility";
    qc_templates[1].minimal = api_min;
    qc_templates[1].full = api_full;
    qc_templates[2].id = "security-review";
    qc_templates[2].blurb = "auth, secrets, untrusted input";
    qc_templates[2].minimal = sec_min;
    qc_templates[2].full = sec_full;
    qc_templates[3].id = "data-migration";
    qc_templates[3].blurb = "reversibility, backfill, rollback";
    qc_templates[3].minimal = mig_min;
    qc_templates[3].full = mig_full;
    qc_templates[4].id = "dependency-update";
    qc_templates[4].blurb = "why this bump, who maintains it";
    qc_templates[4].minimal = dep_min;
    qc_templates[4].full = dep_full;
    qc_templates[5].id = "release";
    qc_templates[5].blurb = "notes, version, rollback";
    qc_templates[5].minimal = rel_min;
    qc_templates[5].full = rel_full;
    qc_templates[6].id = "docs";
    qc_templates[6].blurb = "claims verified against code";
    qc_templates[6].minimal = docs_min;
    qc_templates[6].full = docs_full;
    qc_templates[7].id = "ai-generated-code";
    qc_templates[7].blurb = "agent failure modes: memory APIs, weakened tests";
    qc_templates[7].minimal = ai_min;
    qc_templates[7].full = ai_full;
    ready = 1;
}

// Implements: SW-REQ-003
const QcTemplate * qc_find_template(const char *id) {
    const QcTemplate *t;
    int left;
    templates_init();
    t = qc_templates;
    left = qc_ntemplates;
    while (left) {
        if (t->id && !strcmp(t->id, id)) return t;
        t++;
        left--;
    }
    return NULL;
}

// Implements: SW-REQ-002
static void help_root(void) {
    qc_out(
"qc — a deterministic checklist gate\n"
"\n"
"WHAT\n"
"  Record evidenced answers bound to file content. The gate does not judge quality.\n"
"  It judges whether a decision was recorded — by whom, when, against what bytes.\n"
"\n"
"WHY\n"
"  Agents are non-deterministic. A content-addressed attestation is not.\n"
"\n"
"NEXT\n"
"  New repo:        qc init\n"
"  Adopt a list:    qc template list\n"
"                   qc add <template>\n"
"  Start work:      qc plan\n"
"  Answer:          qc mark <ref> --pass|--fail|--na -m \"…\"\n"
"  Ship:            qc seal --staged && qc verify --staged\n"
"\n"
"PRINCIPLE\n"
"  Evidence must point at something falsifiable. The tool will not tell you the token.\n"
"\n"
"EXIT\n"
"  0 all clear · 1 open items · 2 config/parse error\n"
"\n"
"Topics: init add template plan decide mark baseline seal reset verify compact report\n"
"        qc help <topic>\n"
    );
}

// Implements: SW-REQ-002
static void help_topic(const char *t, const char *what, const char *why, const char *next) {
    qc_out("qc %s\n\nWHAT\n  %s\n\nWHY\n  %s\n\nNEXT\n%s\n", t, what, why, next);
}

// Implements: SYS-REQ-002
// Implements: SW-REQ-002
// Implements: STK-REQ-002
QcHelpRc qc_print_help(const char *topic) {
    if (!topic || !topic[0] || !strcmp(topic, "help")) { help_root(); return QC_OK; }
    if (!strcmp(topic, "init"))
        help_topic("init",
            "Create committed qc/ and ignored .qc/. Vendor this APE as qc/qc. One-line .gitignore.",
            "The dot carries the semantics: dotted is disposable, visible is the review surface.",
            "  qc init\n  qc template list\n");
    else if (!strcmp(topic, "add"))
        help_topic("add",
            "Copy a bundled template into qc/checklists/. Minimal unless --full. Stamps from:.",
            "Checklists compose by copying. Runtime inheritance would hide the truth in review (I7).",
            "  qc template list\n  qc add <template>\n  qc add <template> --full\n  qc plan\n");
    else if (!strcmp(topic, "template"))
        help_topic("template",
            "The gallery. list prints the menu; show prints the minimal file.",
            "The agent's prior for checklist generation: copy the closest, then adapt.",
            "  qc template list\n  qc template show <t>\n  qc add <t>\n");
    else if (!strcmp(topic, "plan"))
        help_topic("plan",
            "The ordered worklist: forced items, questions, previous answers, decision requests.",
            "Questions enter the agent's context BEFORE work. A stop-hook after the fact trains escape.",
            "  qc plan\n  qc mark <ref> --pass -m \"…\"\n");
    else if (!strcmp(topic, "decide"))
        help_topic("decide",
            "Record whether a checklist with applies_when applies. Writes scratch only.",
            "Exclusions must be justified. A n_a on @applies is a diffable Statement of Applicability.",
            "  qc decide <id> --yes -m \"…\"\n  qc decide <id> --no -m \"…\"\n");
    else if (!strcmp(topic, "mark"))
        help_topic("mark",
            "Record an evidenced answer on scratch, bound to the current worktree digest.",
            "Every status requires evidence. The tool never names the string that would pass.",
            "  qc mark <ref> --pass -m \"…\"\n  qc mark <ref> --fail -m \"…\"\n  qc mark <ref> --na -m \"…\"\n");
    else if (!strcmp(topic, "baseline"))
        help_topic("baseline",
            "Grandfather current content at adoption. Writes scratch. Honors expiry.",
            "A new checklist cannot reach trunk until baselined-and-sealed in the introducing PR.",
            "  qc baseline -m \"…\"\n  qc baseline --item <ref> -m \"…\"\n  qc seal --staged\n");
    else if (!strcmp(topic, "seal"))
        help_topic("seal",
            "Promote scratch entries whose digest still matches into this branch's segment.",
            "Unsealed work is invisible to CI. Forgetting seal does not corrupt; it leaves items forced.",
            "  qc seal --staged\n  qc verify --staged\n");
    else if (!strcmp(topic, "reset"))
        help_topic("reset",
            "Wipe scratch, or one scratch entry. Always safe: committed state is untouched.",
            "Scratch is the agent's inner loop. rm -rf .qc/ is the same class of safe.",
            "  qc reset\n  qc reset --item <ref>\n  qc plan\n");
    else if (!strcmp(topic, "verify"))
        help_topic("verify",
            "The gate. loop = scratch+committed. --staged = index, no scratch. --ci = committed, read-only.",
            "Verdict is a pure function of worktree bytes, checklists, state, clock. No inference.",
            "  qc verify\n  qc verify --staged\n  qc verify --ci\n  qc plan\n");
    else if (!strcmp(topic, "compact"))
        help_topic("compact",
            "Trunk-only: fold segments into base, one line per item, drop orphans, commit.",
            "Single-writer by placement. Parallel branches write different segment files (I10).",
            "  git checkout main && qc compact\n");
    else if (!strcmp(topic, "report"))
        help_topic("report",
            "Claims table: item, status, by, evidence, digest. --md for CI summaries.",
            "Reviewers read claims next to the diff. Do not require answers in the PR description.",
            "  qc report\n  qc report --md\n");
    else {
        qc_out("unknown help topic %s.\nNEXT: qc help\n", topic);
        return QC_CONFIG;
    }
    return QC_OK;
}

