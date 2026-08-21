/* acceptance driver: runs tests/acceptance.sh against the built ./qc APE */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run_case(const char *id)
{
    char cmd[512];
    const char *sh = getenv("QC_ACCEPT_SH");
    int rc;
    if (!sh || !sh[0]) sh = "tests/acceptance.sh";
    snprintf(cmd, sizeof cmd, "sh \"%s\" %s", sh, id);
    rc = system(cmd);
    if (rc == 0) return 0;
    fprintf(stderr, "FAIL acceptance case %s (status %d)\n", id, rc);
    return 1;
}

// Verifies: SYS-REQ-001
// Verifies: SW-REQ-001
// Verifies: SYS-REQ-006
// Verifies: SW-REQ-006
// Verifies: INT-REQ-001
// STK-REQ-001:AC-001:acceptance
// STK-REQ-001:nominal:nominal
// STK-REQ-001:boundary:nominal
// SYS-REQ-001:boundary:nominal
// SYS-REQ-001:nominal:nominal
// SYS-REQ-001:determinism:nominal
// SW-REQ-001:boundary:nominal
// SW-REQ-001:determinism:nominal
// MCDC SYS-REQ-001: forced_item_count_EQ_0=F, verdict_EQ_0=F => TRUE [no-action: unanswered verify exits 1 not 0; accept/claim stays FORCED]
// MCDC SYS-REQ-001: forced_item_count_EQ_0=T, verdict_EQ_0=T => TRUE
// MCDC SYS-REQ-006: run_rc_EQ_127=F, verdict_NE_0=F => TRUE [no-action: empty init has no @run item; verify exits 0]
// MCDC SW-REQ-006: forced_item_count_GT_0=F, run_rc_EQ_127=F, verdict_NE_0=F => TRUE [no-action: empty init has no @run item; verify exits 0]
//mcdc:ignore SYS-REQ-001: forced_item_count_EQ_0=F, verdict_EQ_0=T => FALSE -- correct verify cannot exit 0 while forced items remain [reviewed: agent:qc-builder] [category: defensive]
// MCDC INT-REQ-001: ci_mode_EQ_2=F, forced_item_count_EQ_0=F, scratch_counted=F, verdict_EQ_0=F => TRUE
// MCDC INT-REQ-001: ci_mode_EQ_2=F, forced_item_count_EQ_0=T, scratch_counted=F, verdict_EQ_0=T => TRUE
// MCDC INT-REQ-001: ci_mode_EQ_2=T, forced_item_count_EQ_0=T, scratch_counted=F, verdict_EQ_0=T => TRUE
//mcdc:ignore INT-REQ-001: ci_mode_EQ_2=F, forced_item_count_EQ_0=F, scratch_counted=F, verdict_EQ_0=T => FALSE -- correct verify cannot exit 0 while forced items remain [reviewed: agent:qc-builder] [category: defensive]
//mcdc:ignore INT-REQ-001: ci_mode_EQ_2=T, forced_item_count_EQ_0=T, scratch_counted=T, verdict_EQ_0=T => FALSE -- verify --ci never loads scratch so scratch_counted cannot be true in CI [reviewed: agent:qc-builder] [category: defensive]
static int test_stk001_ac001(void) {
 return run_case("STK-REQ-001:AC-001"); }

// Verifies: SYS-REQ-001
// STK-REQ-001:AC-002:acceptance
// STK-REQ-001:determinism:nominal
// SYS-REQ-001:determinism:nominal
static int test_stk001_ac002(void) {
 return run_case("STK-REQ-001:AC-002"); }

// Verifies: SW-REQ-001
// Verifies: INT-REQ-004
// STK-REQ-001:AC-003:acceptance
// STK-REQ-001:nominal:nominal
// SW-REQ-001:determinism:nominal
// INT-REQ-004:determinism:nominal
// INT-REQ-004:integration:integration
static int test_stk001_ac003(void) {
 return run_case("STK-REQ-001:AC-003"); }

// Verifies: SYS-REQ-002
// Verifies: SW-REQ-002
// STK-REQ-002:AC-001:acceptance
// STK-REQ-002:error_handling:negative
// SYS-REQ-002:error_handling:negative
// SW-REQ-002:error_handling:negative
static int test_stk002_ac001(void) {
 return run_case("STK-REQ-002:AC-001"); }

// Verifies: SYS-REQ-002
// Verifies: SW-REQ-002
// STK-REQ-002:AC-002:acceptance
// STK-REQ-002:nominal:nominal
// STK-REQ-002:error_handling:nominal
// SYS-REQ-002:error_handling:nominal
// SYS-REQ-002:nominal:nominal
// MCDC SYS-REQ-002: next_named=T, token_leaked=F => TRUE
//mcdc:ignore SYS-REQ-002: next_named=F, token_leaked=F => FALSE -- help and worklist always name the next qc command [reviewed: agent:qc-builder] [category: defensive]
//mcdc:ignore SYS-REQ-002: next_named=T, token_leaked=T => FALSE -- printer never emits a passing evidence token [reviewed: agent:qc-builder] [category: defensive]
// SW-REQ-002:error_handling:nominal
static int test_stk002_ac002(void) {
 return run_case("STK-REQ-002:AC-002"); }

// Verifies: SYS-REQ-004
// Verifies: SW-REQ-004
// STK-REQ-003:AC-001:acceptance
// STK-REQ-003:nominal:nominal
// STK-REQ-003:error_handling:nominal
// SYS-REQ-004:nominal:nominal
// MCDC SYS-REQ-004: omitted_applicable_EQ_0=T => TRUE
//mcdc:ignore SYS-REQ-004: omitted_applicable_EQ_0=F => FALSE -- correct eval cannot skip an applicable @run or @attest item [reviewed: agent:qc-builder] [category: defensive]
// SYS-REQ-004:error_handling:nominal
// SW-REQ-004:error_handling:nominal
// SW-REQ-004:error_handling:negative
// SYS-REQ-004:error_handling:negative
static int test_stk003_ac001(void) {
 return run_case("STK-REQ-003:AC-001"); }

// Verifies: INT-REQ-003
// STK-REQ-003:AC-002:acceptance
// STK-REQ-003:error_handling:negative
// INT-REQ-003:access_denied:nominal
// INT-REQ-003:integration:integration
static int test_stk003_ac002(void) {
 return run_case("STK-REQ-003:AC-002"); }

// Verifies: SYS-REQ-004
// STK-REQ-003:AC-003:acceptance
// STK-REQ-003:malformed_input:negative
// SYS-REQ-004:malformed_input:negative
static int test_stk003_ac003(void) {
 return run_case("STK-REQ-003:AC-003"); }

// Verifies: SYS-REQ-004
// STK-REQ-003:AC-004:acceptance
// STK-REQ-003:malformed_input:negative
// SYS-REQ-004:malformed_input:negative
static int test_stk003_ac004(void) {
 return run_case("STK-REQ-003:AC-004"); }

// Verifies: SYS-REQ-005
// Verifies: SW-REQ-005
// STK-REQ-004:AC-001:acceptance
// STK-REQ-004:nominal:nominal
// SYS-REQ-005:malformed_input:nominal
// SYS-REQ-005:nominal:nominal
// SW-REQ-005:nominal:nominal
// MCDC SYS-REQ-005: line_parseable=T, stored_ok=T => TRUE
// MCDC SYS-REQ-005: line_parseable=F, stored_ok=F => TRUE [no-action: malformed line is ignored and not stored as ok]
//mcdc:ignore SYS-REQ-005: line_parseable=F, stored_ok=T => FALSE -- a stored ok line always parses as one .qcs line [reviewed: agent:qc-builder] [category: defensive]
// SW-REQ-005:malformed_input:nominal
static int test_stk004_ac001(void) {
 return run_case("STK-REQ-004:AC-001"); }

// Verifies: SW-REQ-007
// Verifies: INT-REQ-002
// STK-REQ-004:AC-002:acceptance
// STK-REQ-004:nominal:nominal
// SW-REQ-007:concurrent:nominal
// INT-REQ-002:concurrent:nominal
// INT-REQ-002:integration:integration
static int test_stk004_ac002(void) {
 return run_case("STK-REQ-004:AC-002"); }

// Verifies: INT-REQ-001
// Verifies: SYS-REQ-001
// STK-REQ-004:AC-003:acceptance
// INT-REQ-001:error_handling:nominal
// INT-REQ-001:error_handling:negative
// INT-REQ-001:integration:integration
// MCDC INT-REQ-001: ci_mode_EQ_2=F, forced_item_count_EQ_0=T, scratch_counted=T, verdict_EQ_0=T => TRUE
static int test_stk004_ac003(void) {
 return run_case("STK-REQ-004:AC-003"); }

// Verifies: SYS-REQ-003
// Verifies: SW-REQ-003
// STK-REQ-005:AC-001:acceptance
// STK-REQ-005:nominal:nominal
// STK-REQ-005:determinism:nominal
// SYS-REQ-003:determinism:nominal
// SYS-REQ-003:nominal:nominal
// SW-REQ-003:nominal:nominal
// MCDC SYS-REQ-003: artifact_is_ape=T, runtime_fetch=F => TRUE
//mcdc:ignore SYS-REQ-003: artifact_is_ape=F, runtime_fetch=F => FALSE -- shipped artifact is the committed APE [reviewed: agent:qc-builder] [category: defensive]
//mcdc:ignore SYS-REQ-003: artifact_is_ape=T, runtime_fetch=T => FALSE -- APE does not fetch gate code at run time [reviewed: agent:qc-builder] [category: defensive]
// SW-REQ-003:determinism:nominal
static int test_stk005_ac001(void) {
 return run_case("STK-REQ-005:AC-001"); }

// Verifies: SYS-REQ-006
// Verifies: SW-REQ-006
// STK-REQ-006:AC-001:acceptance
// STK-REQ-006:error_handling:nominal
// STK-REQ-006:error_handling:negative
// STK-REQ-006:empty_input:nominal
// STK-REQ-006:boundary:nominal
// SYS-REQ-006:empty_input:nominal
// SYS-REQ-006:boundary:nominal
// SYS-REQ-006:error_handling:nominal
// SYS-REQ-006:error_handling:negative
// SW-REQ-006:empty_input:nominal
// SW-REQ-006:error_handling:nominal
// SW-REQ-006:error_handling:negative
static int test_stk006_ac001(void) {
 return run_case("STK-REQ-006:AC-001"); }

// Verifies: SYS-REQ-006
// Verifies: SW-REQ-006
// STK-REQ-006:AC-002:acceptance
// STK-REQ-006:error_handling:nominal
// STK-REQ-006:error_handling:negative
// STK-REQ-006:empty_input:nominal
// SYS-REQ-006:empty_input:nominal
// SYS-REQ-006:boundary:nominal
// SYS-REQ-006:error_handling:nominal
// SYS-REQ-006:error_handling:negative
// SW-REQ-006:empty_input:nominal
// SW-REQ-006:error_handling:nominal
// SW-REQ-006:error_handling:negative
// MCDC SYS-REQ-006: run_rc_EQ_127=T, verdict_NE_0=T => TRUE
// MCDC SW-REQ-006: forced_item_count_GT_0=T, run_rc_EQ_127=T, verdict_NE_0=T => TRUE
//mcdc:ignore SYS-REQ-006: run_rc_EQ_127=T, verdict_NE_0=F => FALSE -- a missing @run binary cannot yield verify exit 0 [reviewed: agent:qc-builder] [category: defensive]
//mcdc:ignore SW-REQ-006: forced_item_count_GT_0=F, run_rc_EQ_127=T, verdict_NE_0=F => FALSE -- run_rc 127 cannot leave forced_item_count at 0 and verdict at 0 [reviewed: agent:qc-builder] [category: defensive]
//mcdc:ignore SW-REQ-006: forced_item_count_GT_0=F, run_rc_EQ_127=T, verdict_NE_0=T => FALSE -- run_rc 127 increments the forced set [reviewed: agent:qc-builder] [category: defensive]
//mcdc:ignore SW-REQ-006: forced_item_count_GT_0=T, run_rc_EQ_127=T, verdict_NE_0=F => FALSE -- run_rc 127 cannot yield verdict 0 [reviewed: agent:qc-builder] [category: defensive]
static int test_stk006_ac002(void) {
 return run_case("STK-REQ-006:AC-002"); }

// Verifies: SYS-REQ-007
// Verifies: SW-REQ-007
// Verifies: INT-REQ-002
// STK-REQ-007:AC-001:acceptance
// STK-REQ-007:nominal:nominal
// STK-REQ-007:concurrent:nominal
// SYS-REQ-007:concurrent:nominal
// SYS-REQ-007:nominal:nominal
// SW-REQ-007:nominal:nominal
// MCDC SYS-REQ-007: parallel_seals=T, segment_collision=F => TRUE
// MCDC SYS-REQ-007: parallel_seals=F, segment_collision=T => TRUE [no-action: a single-branch seal does not write a second colliding path]
//mcdc:ignore SYS-REQ-007: parallel_seals=T, segment_collision=T => FALSE -- parallel seals write distinct segment files [reviewed: agent:qc-builder] [category: defensive]
// SW-REQ-007:concurrent:nominal
// INT-REQ-002:concurrent:nominal
// INT-REQ-002:integration:integration
static int test_stk007_ac001(void) {
 return run_case("STK-REQ-007:AC-001"); }

// Verifies: SW-REQ-007
// Verifies: INT-REQ-002
// STK-REQ-007:AC-002:acceptance
// STK-REQ-007:nominal:nominal
// SW-REQ-007:concurrent:nominal
// INT-REQ-002:concurrent:nominal
// INT-REQ-002:integration:integration
static int test_stk007_ac002(void) {
 return run_case("STK-REQ-007:AC-002"); }

// Verifies: SW-REQ-001
// STK-REQ-007:AC-003:acceptance
// STK-REQ-007:nominal:nominal
// SW-REQ-001:determinism:nominal
static int test_stk007_ac003(void) {
 return run_case("STK-REQ-007:AC-003"); }


// Verifies: SW-REQ-001
// SW-REQ-001:boundary:nominal
// SW-REQ-001:boundary:negative
// MCDC SW-REQ-001: answer_age_s_GT_expires_s=F, expires_s_GT_0=T, forced_item_count_GT_0=F => TRUE [no-action: expires 7d just sealed; verify stays 0 not FORCED (expired)]
// MCDC SW-REQ-001: answer_age_s_GT_expires_s=T, expires_s_GT_0=T, forced_item_count_GT_0=T => TRUE
static int test_sw001_expiry(void) {
 return run_case("SW-REQ-001:expiry"); }

// Verifies: STK-REQ-001
// STK-REQ-001:error_handling:negative
static int test_stk001_cfg_error(void) {
 return run_case("STK-REQ-001:cfg-error"); }

int main(void)
{
    int fails = 0;
    fails += test_stk001_ac001();
    fails += test_stk001_ac002();
    fails += test_stk001_ac003();
    fails += test_stk002_ac001();
    fails += test_stk002_ac002();
    fails += test_stk003_ac001();
    fails += test_stk003_ac002();
    fails += test_stk003_ac003();
    fails += test_stk003_ac004();
    fails += test_stk004_ac001();
    fails += test_stk004_ac002();
    fails += test_stk004_ac003();
    fails += test_stk005_ac001();
    fails += test_stk006_ac001();
    fails += test_stk006_ac002();
    fails += test_stk007_ac001();
    fails += test_stk007_ac002();
    fails += test_stk007_ac003();
    fails += test_sw001_expiry();
    fails += test_stk001_cfg_error();
    if (fails) {
        fprintf(stderr, "%d acceptance failures\n", fails);
        return 1;
    }
    fprintf(stderr, "acceptance tests passed\n");
    return 0;
}
