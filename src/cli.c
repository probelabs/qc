/* cli.c — dispatcher and agent-doctrine help
 */
#include "qc.h"

extern const char *qc_argv0;

// Implements: SW-REQ-002
int cmd_help(int argc, char **argv) {
    return qc_print_help(argc ? argv[0] : NULL);
}

// Implements: SW-REQ-002
int main(int argc, char **argv) {
    const char *cmd;
    qc_argv0 = argc ? argv[0] : "qc"; //mcdc:ignore:defensive POSIX execve rejects an empty argv so argc==0 is not constructible
    if (argc < 2 || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))
        return qc_print_help(NULL);
    if (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-V")) {
        qc_out("qc phase-1 / SPEC.md v0.4 / Cosmopolitan APE\nNEXT: qc help\n");
        return QC_OK;
    }
    {
        /* --checklist <path> works on every command: pull it out before dispatch. */
        int i, j;
        for (i = 2; i < argc; i++) {
            const char *v = NULL;
            int take = 0;
            if (!strcmp(argv[i], "--checklist")) {
                if (i + 1 >= argc) {
                    qc_out("--checklist needs a path.\nNEXT: write the checklist to a file, then qc plan --checklist <path>\n");
                    return QC_CONFIG;
                }
                v = argv[i + 1]; take = 2;
            } else if (!strncmp(argv[i], "--checklist=", 12)) {
                v = argv[i] + 12; take = 1;
            }
            if (!take) continue;
            qc_note_checklist_arg(v);
            for (j = i; j + take < argc; j++) argv[j] = argv[j + take];
            argc -= take;
            argv[argc] = NULL;
            i--;
        }
    }
    cmd = argv[1];
    argc -= 2; argv += 2;
    if (!strcmp(cmd, "help")) return cmd_help(argc, argv);
    if (!strcmp(cmd, "init")) return cmd_init(argc, argv);
    if (!strcmp(cmd, "add")) return cmd_add(argc, argv);
    if (!strcmp(cmd, "template")) return cmd_template(argc, argv);
    if (!strcmp(cmd, "plan")) return cmd_plan(argc, argv);
    if (!strcmp(cmd, "decide")) return cmd_decide(argc, argv);
    if (!strcmp(cmd, "mark")) return cmd_mark(argc, argv);
    if (!strcmp(cmd, "baseline")) return cmd_baseline(argc, argv);
    if (!strcmp(cmd, "seal")) return cmd_seal(argc, argv);
    if (!strcmp(cmd, "reset")) return cmd_reset(argc, argv);
    if (!strcmp(cmd, "verify")) return cmd_verify(argc, argv);
    if (!strcmp(cmd, "compact")) return cmd_compact(argc, argv);
    if (!strcmp(cmd, "report")) return cmd_report(argc, argv);
    qc_out("unknown command %s.\nPrinciple: the CLI is a prompt. Do not guess.\nNEXT: qc help\n", cmd);
    return QC_CONFIG;
}
