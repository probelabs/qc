#!/bin/sh
# Acceptance harness: exercise the built ./qc Cosmopolitan APE end-to-end
# in temporary git repos. This is stakeholder-level evidence, not a kernel unit test.
#
# STK-REQ-001:AC-001:acceptance
# STK-REQ-001:AC-002:acceptance
# STK-REQ-001:AC-003:acceptance
# STK-REQ-002:AC-001:acceptance
# STK-REQ-002:AC-002:acceptance
# STK-REQ-003:AC-001:acceptance
# STK-REQ-003:AC-002:acceptance
# STK-REQ-003:AC-003:acceptance
# STK-REQ-003:AC-004:acceptance
# STK-REQ-004:AC-001:acceptance
# STK-REQ-004:AC-002:acceptance
# STK-REQ-004:AC-003:acceptance
# STK-REQ-005:AC-001:acceptance
# STK-REQ-006:AC-001:acceptance
# STK-REQ-006:AC-002:acceptance
# STK-REQ-007:AC-001:acceptance
# STK-REQ-007:AC-002:acceptance
# STK-REQ-007:AC-003:acceptance

set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
QC="${QC:-$ROOT/qc}"
ONLY="${1:-}"
fail=0
ran=0

say() { echo "ok   $*"; }
bad() { echo "FAIL $*"; fail=$((fail + 1)); }

need_qc() {
    if [ ! -x "$QC" ]; then
        echo "missing built APE at $QC" >&2
        exit 2
    fi
}

new_repo() {
    REPO=$(mktemp -d /tmp/qc-ac.XXXXXX)
    cd "$REPO"
    git init -q -b main
    git config user.email ac@test
    git config user.name ac
    printf "hello 1\n" > README.md
    git add README.md
    git commit -qm seed
    "$QC" init >/dev/null
}

write_attest() {
    cat > qc/checklists/accept.md << "EOF"
---
id: accept
scope: [README.md]
---

- [ ] claim :: Which claim did you check against README.md?
EOF
}

write_mixed() {
    cat > qc/checklists/accept.md << "EOF"
---
id: accept
scope: [README.md]
---

- [ ] claim :: Which claim did you check against README.md?
- [ ] runok :: Is true green? @run(true)
- [ ] missing :: Is the missing checker green? @run(qc-missing-bin-xyz)
- [ ] human :: Did a person read this? @human
EOF
}

write_runs() {
    cat > qc/checklists/accept.md << "EOF"
---
id: accept
scope: [README.md]
---

- [ ] runok :: Is true green? @run(true)
- [ ] missing :: Is the missing checker green? @run(qc-missing-bin-xyz)
EOF
}

EVIDENCE="README.md names 1 claim"

open_refs() {
    awk '
        /FORCED|FAILED|BLOCKED|AWAITING/ {
            for (i = 1; i <= NF; i++) {
                if ($i ~ /\/[A-Za-z@]/) print $i
            }
        }
    ' | sort -u
}

should_run() {
    case_id=$1
    if [ -z "$ONLY" ] || [ "$ONLY" = "$case_id" ]; then
        return 0
    fi
    return 1
}

run_case() {
    case_id=$1
    shift
    if ! should_run "$case_id"; then
        return 0
    fi
    ran=$((ran + 1))
    echo "-- $case_id"
    if "$@"; then
        :
    else
        bad "$case_id case function returned nonzero"
    fi
}

# STK-REQ-001:AC-001:acceptance
# Verifies: SYS-REQ-001
# Verifies: SW-REQ-001
# Verifies: SYS-REQ-006
# Verifies: SW-REQ-006
# Verifies: INT-REQ-001
# STK-REQ-001:nominal:nominal
# STK-REQ-001:boundary:nominal
# SYS-REQ-001:boundary:nominal
# SYS-REQ-001:nominal:nominal
# SYS-REQ-001:determinism:nominal
# SW-REQ-001:boundary:nominal
# SW-REQ-001:determinism:nominal
# MCDC SYS-REQ-001: forced_item_count_EQ_0=F, verdict_EQ_0=F => TRUE [no-action: unanswered verify exits 1 not 0; accept/claim stays FORCED]
# MCDC SYS-REQ-001: forced_item_count_EQ_0=T, verdict_EQ_0=T => TRUE
# MCDC SYS-REQ-006: run_rc_EQ_127=F, verdict_NE_0=F => TRUE [no-action: empty init has no @run item; verify exits 0]
# MCDC SW-REQ-006: forced_item_count_GT_0=F, run_rc_EQ_127=F, verdict_NE_0=F => TRUE [no-action: empty init has no @run item; verify exits 0]
#mcdc:ignore SYS-REQ-001: forced_item_count_EQ_0=F, verdict_EQ_0=T => FALSE -- correct verify cannot exit 0 while forced items remain [reviewed: agent:qc-builder] [category: defensive]
# MCDC INT-REQ-001: ci_mode_EQ_2=F, forced_item_count_EQ_0=F, scratch_counted=F, verdict_EQ_0=F => TRUE
# MCDC INT-REQ-001: ci_mode_EQ_2=F, forced_item_count_EQ_0=T, scratch_counted=F, verdict_EQ_0=T => TRUE
# MCDC INT-REQ-001: ci_mode_EQ_2=T, forced_item_count_EQ_0=T, scratch_counted=F, verdict_EQ_0=T => TRUE
#mcdc:ignore INT-REQ-001: ci_mode_EQ_2=F, forced_item_count_EQ_0=F, scratch_counted=F, verdict_EQ_0=T => FALSE -- correct verify cannot exit 0 while forced items remain [reviewed: agent:qc-builder] [category: defensive]
#mcdc:ignore INT-REQ-001: ci_mode_EQ_2=T, forced_item_count_EQ_0=T, scratch_counted=T, verdict_EQ_0=T => FALSE -- verify --ci never loads scratch so scratch_counted cannot be true in CI [reviewed: agent:qc-builder] [category: defensive]
case_stk001_ac001() {
    new_repo
    set +e
    "$QC" verify >/tmp/qc-ac-v0.out 2>/tmp/qc-ac-v0.err
    code=$?
    set -e
    if [ "$code" -eq 0 ]; then say "STK-REQ-001:AC-001 empty init verify=0"; else bad "empty init verify=$code"; fi
    set +e
    "$QC" verify --ci >/tmp/qc-ac-v0ci.out 2>/tmp/qc-ac-v0ci.err
    code=$?
    set -e
    if [ "$code" -eq 0 ]; then say "STK-REQ-001:AC-001 empty init verify --ci=0"; else bad "empty init ci verify=$code"; fi

    write_attest
    set +e
    "$QC" verify >/tmp/qc-ac-v1.out 2>/tmp/qc-ac-v1.err
    code=$?
    set -e
    if [ "$code" -eq 1 ] && grep -q "accept/claim" /tmp/qc-ac-v1.out; then
        say "STK-REQ-001:AC-001 unanswered applicable item verify=1"
    else
        bad "unanswered verify=$code"
        cat /tmp/qc-ac-v1.out /tmp/qc-ac-v1.err
    fi

    "$QC" mark accept/claim --pass -m "$EVIDENCE" >/tmp/qc-ac-m.out
    "$QC" seal >/tmp/qc-ac-s.out
    set +e
    "$QC" verify >/tmp/qc-ac-v2.out 2>/tmp/qc-ac-v2.err
    code=$?
    set -e
    if [ "$code" -eq 0 ]; then say "STK-REQ-001:AC-001 matching answer verify=0"; else bad "matching verify=$code"; cat /tmp/qc-ac-v2.out; fi

    printf "hello 2 changed\n" > README.md
    set +e
    "$QC" verify >/tmp/qc-ac-v3.out 2>/tmp/qc-ac-v3.err
    code=$?
    set -e
    if [ "$code" -eq 1 ] && grep -q "FORCED" /tmp/qc-ac-v3.out; then
        say "STK-REQ-001:AC-001 stale content verify=1"
    else
        bad "stale verify=$code"
        cat /tmp/qc-ac-v3.out
    fi
}

# STK-REQ-001:AC-002:acceptance
# Verifies: SYS-REQ-001
# STK-REQ-001:determinism:nominal
# SYS-REQ-001:determinism:nominal
case_stk001_ac002() {
    new_repo
    write_mixed
    set +e
    "$QC" plan >/tmp/qc-ac-plan.out 2>/tmp/qc-ac-plan.err
    pcode=$?
    "$QC" verify >/tmp/qc-ac-ver.out 2>/tmp/qc-ac-ver.err
    vcode=$?
    set -e
    open_refs </tmp/qc-ac-plan.out >/tmp/qc-ac-plan.refs
    open_refs </tmp/qc-ac-ver.out >/tmp/qc-ac-ver.refs
    if [ "$pcode" -eq 1 ] && [ "$vcode" -eq 1 ] && cmp -s /tmp/qc-ac-plan.refs /tmp/qc-ac-ver.refs; then
        say "STK-REQ-001:AC-002 plan and verify share the open set"
    else
        bad "plan/verify open-set mismatch (plan=$pcode verify=$vcode)"
        echo plan:; cat /tmp/qc-ac-plan.out
        echo verify:; cat /tmp/qc-ac-ver.out
        echo "plan refs:"; cat /tmp/qc-ac-plan.refs
        echo "ver refs:"; cat /tmp/qc-ac-ver.refs
    fi

    new_repo
    write_attest
    "$QC" mark accept/claim --pass -m "$EVIDENCE" >/dev/null
    "$QC" seal >/dev/null
    set +e
    "$QC" plan >/tmp/qc-ac-plan2.out 2>/tmp/qc-ac-plan2.err
    pcode=$?
    "$QC" verify >/tmp/qc-ac-ver2.out 2>/tmp/qc-ac-ver2.err
    vcode=$?
    set -e
    if [ "$pcode" -eq 0 ] && [ "$vcode" -eq 0 ] && grep -q "all clear" /tmp/qc-ac-plan2.out; then
        say "STK-REQ-001:AC-002 clear tree: plan and verify both exit 0"
    else
        bad "clear tree plan=$pcode verify=$vcode"
        cat /tmp/qc-ac-plan2.out /tmp/qc-ac-ver2.out
    fi
}

# STK-REQ-001:AC-003:acceptance
# Verifies: SW-REQ-001
# Verifies: INT-REQ-004
# STK-REQ-001:nominal:nominal
# SW-REQ-001:determinism:nominal
# INT-REQ-004:determinism:nominal
# INT-REQ-004:integration:integration
case_stk001_ac003() {
    new_repo
    write_attest
    "$QC" mark accept/claim --pass -m "$EVIDENCE" >/tmp/qc-ac-md.out
    dig=$(awk "/digest @/{for(i=1;i<=NF;i++) if(\$i ~ /^@[0-9a-f]{16}$/){print substr(\$i,2); exit}}" /tmp/qc-ac-md.out)
    if [ "${#dig}" -eq 16 ]; then say "STK-REQ-001:AC-003 printed digest is 16-hex"; else bad "printed digest"; cat /tmp/qc-ac-md.out; fi
    line=$(grep "^accept/claim" .qc/scratch.qcs || true)
    case "$line" in
        *"@$dig "*) say "STK-REQ-001:AC-003 stored line digest equals printed digest" ;;
        *) bad "scratch line digest mismatch" ;;
    esac
    "$QC" seal >/dev/null
    set +e
    "$QC" verify >/tmp/qc-ac-eq.out 2>/tmp/qc-ac-eq.err
    code=$?
    set -e
    if [ "$code" -eq 0 ]; then say "STK-REQ-001:AC-003 verify accepts matching digest"; else bad "matching digest verify=$code"; fi

    printf "%s\n" "---" "id: accept" "scope: [README.md]" "---" "" \
        "- [ ] claim :: Which claim did you rewrite against README.md?" \
        > qc/checklists/accept.md
    set +e
    "$QC" verify >/tmp/qc-ac-qw.out 2>/tmp/qc-ac-qw.err
    code=$?
    set -e
    if [ "$code" -eq 1 ] && grep -q "FORCED" /tmp/qc-ac-qw.out; then
        say "STK-REQ-001:AC-003 rewritten question is not a matching answer"
    else
        bad "question rewrite verify=$code"
        cat /tmp/qc-ac-qw.out
    fi
}

# STK-REQ-002:AC-001:acceptance
# Verifies: SYS-REQ-002
# Verifies: SW-REQ-002
# STK-REQ-002:error_handling:negative
# SYS-REQ-002:error_handling:negative
# SW-REQ-002:error_handling:negative
case_stk002_ac001() {
    new_repo
    write_attest
    set +e
    "$QC" mark accept/claim --pass -m "looks good" >/tmp/qc-ac-fail.out 2>/tmp/qc-ac-fail.err
    code=$?
    set -e
    cat /tmp/qc-ac-fail.out /tmp/qc-ac-fail.err >/tmp/qc-ac-fail.all
    if [ "$code" -eq 2 ] && grep -q "NEXT:" /tmp/qc-ac-fail.all && grep -q "qc mark" /tmp/qc-ac-fail.all; then
        say "STK-REQ-002:AC-001 failing mark names the next command"
    else
        bad "failing mark code=$code missing NEXT"
        cat /tmp/qc-ac-fail.all
    fi
    if grep -F "$EVIDENCE" /tmp/qc-ac-fail.all >/dev/null; then
        bad "failing mark printed a passing evidence string"
    else
        say "STK-REQ-002:AC-001 failing mark does not print passing evidence"
    fi
}

# STK-REQ-002:AC-002:acceptance
# Verifies: SYS-REQ-002
# Verifies: SW-REQ-002
# STK-REQ-002:nominal:nominal
# STK-REQ-002:error_handling:nominal
# SYS-REQ-002:error_handling:nominal
# SYS-REQ-002:nominal:nominal
# MCDC SYS-REQ-002: next_named=T, token_leaked=F => TRUE
#mcdc:ignore SYS-REQ-002: next_named=F, token_leaked=F => FALSE -- help and worklist always name the next qc command [reviewed: agent:qc-builder] [category: defensive]
#mcdc:ignore SYS-REQ-002: next_named=T, token_leaked=T => FALSE -- printer never emits a passing evidence token [reviewed: agent:qc-builder] [category: defensive]
# SW-REQ-002:error_handling:nominal
case_stk002_ac002() {
    new_repo
    write_attest
    "$QC" help >/tmp/qc-ac-help.out
    set +e
    "$QC" mark accept/claim --pass -m "lgtm" >/tmp/qc-ac-err.out 2>/tmp/qc-ac-err.err
    "$QC" plan >/tmp/qc-ac-wl.out 2>/tmp/qc-ac-wl.err
    set -e
    cat /tmp/qc-ac-err.out /tmp/qc-ac-err.err >/tmp/qc-ac-err.all
    ok=1
    if grep -q "NEXT" /tmp/qc-ac-help.out && grep -q "qc init" /tmp/qc-ac-help.out && grep -q "qc plan" /tmp/qc-ac-help.out; then
        :
    else
        ok=0
        bad "help missing NEXT qc commands"
    fi
    if grep -q "NEXT:" /tmp/qc-ac-err.all && grep -q "qc mark" /tmp/qc-ac-err.all; then
        :
    else
        ok=0
        bad "error missing NEXT qc command"
    fi
    if grep -q "NEXT: qc mark accept/claim" /tmp/qc-ac-wl.out; then
        :
    else
        ok=0
        bad "worklist missing NEXT qc mark"
    fi
    if grep -F "$EVIDENCE" /tmp/qc-ac-help.out /tmp/qc-ac-err.all /tmp/qc-ac-wl.out >/dev/null; then
        ok=0
        bad "help/error/worklist printed a passing evidence string"
    fi
    if [ "$ok" -eq 1 ]; then
        say "STK-REQ-002:AC-002 help, error, and worklist name next qc commands"
    fi
}

# STK-REQ-003:AC-001:acceptance
# Verifies: SYS-REQ-004
# Verifies: SW-REQ-004
# STK-REQ-003:nominal:nominal
# STK-REQ-003:error_handling:nominal
# SYS-REQ-004:nominal:nominal
# MCDC SYS-REQ-004: omitted_applicable_EQ_0=T => TRUE
#mcdc:ignore SYS-REQ-004: omitted_applicable_EQ_0=F => FALSE -- correct eval cannot skip an applicable @run or @attest item [reviewed: agent:qc-builder] [category: defensive]
# SYS-REQ-004:error_handling:nominal
# SW-REQ-004:error_handling:nominal
# SW-REQ-004:error_handling:negative
# SYS-REQ-004:error_handling:negative
case_stk003_ac001() {
    new_repo
    write_mixed
    set +e
    "$QC" verify >/tmp/qc-ac-run.out 2>/tmp/qc-ac-run.err
    set -e
    if grep -q "CLEAR              accept/runok" /tmp/qc-ac-run.out \
        && grep -q "FORCED (never)     accept/claim" /tmp/qc-ac-run.out \
        && grep -q "FAILED             accept/missing" /tmp/qc-ac-run.out; then
        say "STK-REQ-003:AC-001 @run executed; applicable @attest still forced"
    else
        bad "run/attest split"
        cat /tmp/qc-ac-run.out
    fi
    "$QC" mark accept/claim --pass -m "$EVIDENCE" >/dev/null
    set +e
    "$QC" verify >/tmp/qc-ac-run2.out 2>/tmp/qc-ac-run2.err
    set -e
    if grep -q "CLEAR              accept/claim" /tmp/qc-ac-run2.out \
        && grep -q "CLEAR              accept/runok" /tmp/qc-ac-run2.out; then
        say "STK-REQ-003:AC-001 attested item is evidenced after mark"
    else
        bad "attest after mark"
        cat /tmp/qc-ac-run2.out
    fi
}

# STK-REQ-003:AC-002:acceptance
# Verifies: INT-REQ-003
# STK-REQ-003:error_handling:negative
# INT-REQ-003:access_denied:nominal
# INT-REQ-003:integration:integration
case_stk003_ac002() {
    new_repo
    write_mixed
    printf "%s\n" "# qc-state v1" > .qc/scratch.qcs
    set +e
    QC_BY=agent "$QC" mark accept/human --pass -m "$EVIDENCE" >/tmp/qc-ac-h1.out 2>/tmp/qc-ac-h1.err
    c1=$?
    "$QC" mark accept/human --pass -m "$EVIDENCE" --by agent >/tmp/qc-ac-h2.out 2>/tmp/qc-ac-h2.err
    c2=$?
    set -e
    if [ "$c1" -eq 2 ] && [ "$c2" -eq 2 ]; then
        say "STK-REQ-003:AC-002 agent mark of @human exits 2"
    else
        bad "human refuse codes $c1 $c2"
    fi
    if grep -q "accept/human" .qc/scratch.qcs; then
        bad "human refuse wrote a scratch line"
        cat .qc/scratch.qcs
    else
        say "STK-REQ-003:AC-002 agent mark of @human writes no scratch line"
    fi
}

# STK-REQ-003:AC-003:acceptance
# Verifies: SYS-REQ-004
# STK-REQ-003:malformed_input:negative
# SYS-REQ-004:malformed_input:negative
case_stk003_ac003() {
    new_repo
    write_attest
    printf "%s\n" "# qc-state v1" > .qc/scratch.qcs
    set +e
    "$QC" mark accept/claim --pass -m "lgtm" >/tmp/qc-ac-lgtm.out 2>/tmp/qc-ac-lgtm.err
    c1=$?
    "$QC" mark accept/claim --pass -m "done" >/tmp/qc-ac-done.out 2>/tmp/qc-ac-done.err
    c2=$?
    set -e
    if [ "$c1" -eq 2 ] && [ "$c2" -eq 2 ]; then
        say "STK-REQ-003:AC-003 lgtm and done exit 2"
    else
        bad "deny codes lgtm=$c1 done=$c2"
    fi
    if grep -q "accept/claim" .qc/scratch.qcs; then
        bad "denied evidence wrote a scratch line"
        cat .qc/scratch.qcs
    else
        say "STK-REQ-003:AC-003 denied evidence writes no scratch line"
    fi
}

# STK-REQ-003:AC-004:acceptance
# Verifies: SYS-REQ-004
# STK-REQ-003:malformed_input:negative
# SYS-REQ-004:malformed_input:negative
case_stk003_ac004() {
    new_repo
    write_attest
    prefix_g=ghp
    secret1="${prefix_g}_notarealtoken0000"
    prefix_b=BEGIN
    secret2="-----${prefix_b} PRIVATE KEY-----"
    set +e
    "$QC" mark accept/claim --pass -m "$secret1" >/tmp/qc-ac-s1.out 2>/tmp/qc-ac-s1.err
    c1=$?
    "$QC" mark accept/claim --pass -m "$secret2" >/tmp/qc-ac-s2.out 2>/tmp/qc-ac-s2.err
    c2=$?
    set -e
    cat /tmp/qc-ac-s1.out /tmp/qc-ac-s1.err >/tmp/qc-ac-s1.all
    cat /tmp/qc-ac-s2.out /tmp/qc-ac-s2.err >/tmp/qc-ac-s2.all
    if [ "$c1" -eq 2 ] && [ "$c2" -eq 2 ]; then
        say "STK-REQ-003:AC-004 secret-shaped evidence exits 2"
    else
        bad "secret codes $c1 $c2"
    fi
    if grep -F -- "$secret1" /tmp/qc-ac-s1.all >/dev/null || grep -F -- "$secret2" /tmp/qc-ac-s2.all >/dev/null; then
        bad "qc echoed a secret from rejected evidence"
    else
        say "STK-REQ-003:AC-004 rejected secret is not echoed"
    fi
}

# STK-REQ-004:AC-001:acceptance
# Verifies: SYS-REQ-005
# Verifies: SW-REQ-005
# STK-REQ-004:nominal:nominal
# SYS-REQ-005:malformed_input:nominal
# SYS-REQ-005:nominal:nominal
# SW-REQ-005:nominal:nominal
# MCDC SYS-REQ-005: line_parseable=T, stored_ok=T => TRUE
# MCDC SYS-REQ-005: line_parseable=F, stored_ok=F => TRUE [no-action: malformed line is ignored and not stored as ok]
#mcdc:ignore SYS-REQ-005: line_parseable=F, stored_ok=T => FALSE -- a stored ok line always parses as one .qcs line [reviewed: agent:qc-builder] [category: defensive]
# SW-REQ-005:malformed_input:nominal
case_stk004_ac001() {
    new_repo
    write_attest
    git add qc .gitignore .gitattributes
    git commit -qm "install gate"
    "$QC" mark accept/claim --pass -m "$EVIDENCE" >/dev/null
    "$QC" seal --staged >/tmp/qc-ac-seal.out
    if git diff --cached -- qc/state | grep -q "^+accept/claim = pass @"; then
        added=$(git diff --cached -- qc/state | grep -c "^+accept/claim = pass @" || true)
        files=$(git diff --cached --name-only -- qc/state | grep -c "seg-.*[.]qcs" || true)
        if [ "$added" -eq 1 ] && [ "$files" -eq 1 ]; then
            say "STK-REQ-004:AC-001 seal stages one added .qcs line"
        else
            bad "staged $files files / $added lines"
            git diff --cached -- qc/state
        fi
    else
        bad "no added .qcs line in staged diff"
        git diff --cached -- qc/state
        cat /tmp/qc-ac-seal.out
    fi
}

# STK-REQ-004:AC-002:acceptance
# Verifies: SW-REQ-007
# Verifies: INT-REQ-002
# STK-REQ-004:nominal:nominal
# SW-REQ-007:concurrent:nominal
# INT-REQ-002:concurrent:nominal
# INT-REQ-002:integration:integration
case_stk004_ac002() {
    new_repo
    write_attest
    mkdir -p qc/state
    printf "%s\n" "# qc-state v1" > qc/state/base.qcs
    before=$(cksum qc/state/base.qcs)
    find qc/state -type f | sort > /tmp/qc-ac-st-before
    "$QC" mark accept/claim --pass -m "$EVIDENCE" >/dev/null
    after=$(cksum qc/state/base.qcs)
    find qc/state -type f | sort > /tmp/qc-ac-st-after
    if [ "$before" = "$after" ] && cmp -s /tmp/qc-ac-st-before /tmp/qc-ac-st-after; then
        say "STK-REQ-004:AC-002 mark leaves qc/state unchanged"
    else
        bad "mark touched qc/state"
        cat /tmp/qc-ac-st-before /tmp/qc-ac-st-after
    fi
    if [ -f .qc/scratch.qcs ] && grep -q "accept/claim" .qc/scratch.qcs; then
        say "STK-REQ-004:AC-002 mark writes scratch"
    else
        bad "mark did not write scratch"
    fi
    if find .qc/manifests -name "*.json" 2>/dev/null | grep -q .; then say "STK-REQ-004:AC-002 mark may write digest json under .qc/manifests"; fi
}

# STK-REQ-004:AC-003:acceptance
# Verifies: INT-REQ-001
# Verifies: SYS-REQ-001
# INT-REQ-001:error_handling:nominal
# INT-REQ-001:error_handling:negative
# INT-REQ-001:integration:integration
# MCDC INT-REQ-001: ci_mode_EQ_2=F, forced_item_count_EQ_0=T, scratch_counted=T, verdict_EQ_0=T => TRUE
case_stk004_ac003() {
    new_repo
    write_attest
    "$QC" mark accept/claim --pass -m "$EVIDENCE" >/dev/null
    set +e
    "$QC" verify >/tmp/qc-ac-loop.out 2>/tmp/qc-ac-loop.err
    lcode=$?
    set -e
    if [ "$lcode" -eq 0 ]; then
        say "STK-REQ-004:AC-003 loop mode reads scratch and exits 0"
    else
        bad "loop verify after mark=$lcode (scratch should clear the gate)"
        cat /tmp/qc-ac-loop.out /tmp/qc-ac-loop.err
    fi
    find .qc -type f | sort > /tmp/qc-ac-dot-before
    cksum .qc/scratch.qcs > /tmp/qc-ac-sc-before
    set +e
    "$QC" verify --ci >/tmp/qc-ac-ci.out 2>/tmp/qc-ac-ci.err
    code=$?
    set -e
    find .qc -type f | sort > /tmp/qc-ac-dot-after
    cksum .qc/scratch.qcs > /tmp/qc-ac-sc-after
    if [ "$code" -eq 1 ] && grep -q "FORCED (never)     accept/claim" /tmp/qc-ac-ci.out; then
        say "STK-REQ-004:AC-003 ci mode does not read scratch"
    else
        bad "ci mode treated scratch as visible (exit $code)"
        cat /tmp/qc-ac-ci.out
    fi
    if cmp -s /tmp/qc-ac-dot-before /tmp/qc-ac-dot-after && cmp -s /tmp/qc-ac-sc-before /tmp/qc-ac-sc-after; then
        say "STK-REQ-004:AC-003 ci mode writes no file under .qc"
    else
        bad "ci mode wrote under .qc"
        diff /tmp/qc-ac-dot-before /tmp/qc-ac-dot-after || true
    fi
}

# STK-REQ-005:AC-001:acceptance
# Verifies: SYS-REQ-003
# Verifies: SW-REQ-003
# STK-REQ-005:nominal:nominal
# STK-REQ-005:determinism:nominal
# SYS-REQ-003:determinism:nominal
# SYS-REQ-003:nominal:nominal
# SW-REQ-003:nominal:nominal
# MCDC SYS-REQ-003: artifact_is_ape=T, runtime_fetch=F => TRUE
#mcdc:ignore SYS-REQ-003: artifact_is_ape=F, runtime_fetch=F => FALSE -- shipped artifact is the committed APE [reviewed: agent:qc-builder] [category: defensive]
#mcdc:ignore SYS-REQ-003: artifact_is_ape=T, runtime_fetch=T => FALSE -- APE does not fetch gate code at run time [reviewed: agent:qc-builder] [category: defensive]
# SW-REQ-003:determinism:nominal
case_stk005_ac001() {
    new_repo
    write_attest
    git add qc .gitignore .gitattributes
    git commit -qm gatebin
    if [ ! -x qc/qc ]; then bad missing_vendored_qc; return 0; fi
    hex=$(od -An -tx1 -N 6 qc/qc | tr -d " \n")
    if [ "$hex" = "4d5a71467044" ]; then say "STK-REQ-005:AC-001 vendored qc/qc is APE"; else bad bad_magic; fi
    if grep -qiE "python3" "$ROOT/Makefile"; then bad makefile_runtime; else say "STK-REQ-005:AC-001 Makefile has no python runtime"; fi
    set +e
    ./qc/qc verify --ci >/tmp/qc-ac-ape.out 2>/tmp/qc-ac-ape.err
    code=$?
    set -e
    if [ "$code" -eq 1 ] && grep -q accept/claim /tmp/qc-ac-ape.out; then say "STK-REQ-005:AC-001 vendored binary runs verify ci"; else bad ape_ci; cat /tmp/qc-ac-ape.out /tmp/qc-ac-ape.err; fi
}


# STK-REQ-006:AC-001:acceptance
# Verifies: SYS-REQ-006
# Verifies: SW-REQ-006
# STK-REQ-006:error_handling:nominal
# STK-REQ-006:error_handling:negative
# STK-REQ-006:empty_input:nominal
# STK-REQ-006:boundary:nominal
# SYS-REQ-006:empty_input:nominal
# SYS-REQ-006:boundary:nominal
# SYS-REQ-006:error_handling:nominal
# SYS-REQ-006:error_handling:negative
# SW-REQ-006:empty_input:nominal
# SW-REQ-006:error_handling:nominal
# SW-REQ-006:error_handling:negative
case_stk006_ac001() {
    new_repo
    write_runs
    set +e
    "$QC" verify >/tmp/qc-ac-miss.out 2>/tmp/qc-ac-miss.err
    code=$?
    set -e
    if [ "$code" -eq 1 ] && grep -q "FAILED             accept/missing" /tmp/qc-ac-miss.out && grep -qi install /tmp/qc-ac-miss.out && ! grep -q "CLEAR              accept/missing" /tmp/qc-ac-miss.out; then
        say "STK-REQ-006:AC-001 missing @run is FAILED with install text"
    else
        bad missing_run_not_loud; cat /tmp/qc-ac-miss.out /tmp/qc-ac-miss.err
    fi
}

# STK-REQ-006:AC-002:acceptance
# Verifies: SYS-REQ-006
# Verifies: SW-REQ-006
# STK-REQ-006:error_handling:nominal
# STK-REQ-006:error_handling:negative
# STK-REQ-006:empty_input:nominal
# SYS-REQ-006:empty_input:nominal
# SYS-REQ-006:boundary:nominal
# SYS-REQ-006:error_handling:nominal
# SYS-REQ-006:error_handling:negative
# SW-REQ-006:empty_input:nominal
# SW-REQ-006:error_handling:nominal
# SW-REQ-006:error_handling:negative
# MCDC SYS-REQ-006: run_rc_EQ_127=T, verdict_NE_0=T => TRUE
# MCDC SW-REQ-006: forced_item_count_GT_0=T, run_rc_EQ_127=T, verdict_NE_0=T => TRUE
#mcdc:ignore SYS-REQ-006: run_rc_EQ_127=T, verdict_NE_0=F => FALSE -- a missing @run binary cannot yield verify exit 0 [reviewed: agent:qc-builder] [category: defensive]
#mcdc:ignore SW-REQ-006: forced_item_count_GT_0=F, run_rc_EQ_127=T, verdict_NE_0=F => FALSE -- run_rc 127 cannot leave forced_item_count at 0 and verdict at 0 [reviewed: agent:qc-builder] [category: defensive]
#mcdc:ignore SW-REQ-006: forced_item_count_GT_0=F, run_rc_EQ_127=T, verdict_NE_0=T => FALSE -- run_rc 127 increments the forced set [reviewed: agent:qc-builder] [category: defensive]
#mcdc:ignore SW-REQ-006: forced_item_count_GT_0=T, run_rc_EQ_127=T, verdict_NE_0=F => FALSE -- run_rc 127 cannot yield verdict 0 [reviewed: agent:qc-builder] [category: defensive]
case_stk006_ac002() {
    new_repo
    write_runs
    set +e
    "$QC" verify >/tmp/qc-ac-127.out 2>/tmp/qc-ac-127.err
    set -e
    if grep -q "@run exited 127" /tmp/qc-ac-127.out && grep -qi install /tmp/qc-ac-127.out && grep -q "FAILED             accept/missing" /tmp/qc-ac-127.out && ! grep -q "CLEAR              accept/missing" /tmp/qc-ac-127.out; then
        say "STK-REQ-006:AC-002 missing binary is FAILED with run_rc 127"
    else
        bad missing_not_127; cat /tmp/qc-ac-127.out
    fi
}

# STK-REQ-007:AC-001:acceptance
# Verifies: SYS-REQ-007
# Verifies: SW-REQ-007
# Verifies: INT-REQ-002
# STK-REQ-007:nominal:nominal
# STK-REQ-007:concurrent:nominal
# SYS-REQ-007:concurrent:nominal
# SYS-REQ-007:nominal:nominal
# SW-REQ-007:nominal:nominal
# MCDC SYS-REQ-007: parallel_seals=T, segment_collision=F => TRUE
# MCDC SYS-REQ-007: parallel_seals=F, segment_collision=T => TRUE [no-action: a single-branch seal does not write a second colliding path]
#mcdc:ignore SYS-REQ-007: parallel_seals=T, segment_collision=T => FALSE -- parallel seals write distinct segment files [reviewed: agent:qc-builder] [category: defensive]
# SW-REQ-007:concurrent:nominal
# INT-REQ-002:concurrent:nominal
# INT-REQ-002:integration:integration
case_stk007_ac001() {
    new_repo
    write_attest
    git add qc .gitignore .gitattributes
    git commit -qm gate
    git checkout -qb feat-a
    "$QC" mark accept/claim --pass -m "$EVIDENCE" >/dev/null
    "$QC" seal --staged >/dev/null
    git add qc/state
    git commit -qm feat-a-seal
    git checkout -q main
    git checkout -qb feat-b
    printf "hello from b 3\n" > README.md
    git add README.md
    git commit -qm b-content
    "$QC" mark accept/claim --pass -m "$EVIDENCE" >/dev/null
    "$QC" seal --staged >/dev/null
    git add qc/state README.md
    git commit -qm feat-b-seal
    git checkout -q main
    git merge --no-edit feat-a >/tmp/qc-ac-ma.out
    git merge --no-edit feat-b >/tmp/qc-ac-mb.out
    segs=$(git ls-files "qc/state/seg-*.qcs" | wc -l | tr -d " ")
    if [ "$segs" -eq 2 ] && git ls-files | grep -q qc/state/seg-feat-a- && git ls-files | grep -q qc/state/seg-feat-b-; then
        say "STK-REQ-007:AC-001 two sealed branches merge as two segment files"
    else
        bad merge_segs; git ls-files qc/state; cat /tmp/qc-ac-ma.out /tmp/qc-ac-mb.out
    fi
}

# STK-REQ-007:AC-002:acceptance
# Verifies: SW-REQ-007
# Verifies: INT-REQ-002
# STK-REQ-007:nominal:nominal
# SW-REQ-007:concurrent:nominal
# INT-REQ-002:concurrent:nominal
# INT-REQ-002:integration:integration
case_stk007_ac002() {
    new_repo
    write_attest
    printf "%s\n" "# qc-state v1" > qc/state/base.qcs
    before=$(cksum qc/state/base.qcs)
    "$QC" mark accept/claim --pass -m "$EVIDENCE" >/dev/null
    "$QC" seal >/tmp/qc-ac-seg.out
    after=$(cksum qc/state/base.qcs)
    segs=$(find qc/state -name "seg-*.qcs" | wc -l | tr -d " ")
    if [ "$before" = "$after" ] && [ "$segs" -eq 1 ] && grep -q accept/claim qc/state/seg-*.qcs; then
        say "STK-REQ-007:AC-002 seal appends a per-branch segment and leaves base.qcs"
    else
        bad seal_shared; ls -la qc/state; cat /tmp/qc-ac-seg.out
    fi
}

# STK-REQ-007:AC-003:acceptance
# Verifies: SW-REQ-001
# STK-REQ-007:nominal:nominal
# SW-REQ-001:determinism:nominal
case_stk007_ac003() {
    new_repo
    write_attest
    "$QC" mark accept/claim --pass -m "$EVIDENCE" >/dev/null
    printf "hello 2 changed\n" > README.md
    "$QC" seal >/tmp/qc-ac-mm.out
    if grep -q "promoted 0 of 1" /tmp/qc-ac-mm.out && grep -q accept/claim .qc/scratch.qcs; then
        say "STK-REQ-007:AC-003 digest mismatch leaves the line on scratch"
    else
        bad mismatch_promoted; cat /tmp/qc-ac-mm.out .qc/scratch.qcs
    fi
    git checkout -- README.md
    "$QC" seal >/tmp/qc-ac-ok.out
    if grep -q "promoted 1 of 1" /tmp/qc-ac-ok.out && ! grep -q accept/claim .qc/scratch.qcs; then
        say "STK-REQ-007:AC-003 matching digest is promoted off scratch"
    else
        bad match_not_promoted; cat /tmp/qc-ac-ok.out .qc/scratch.qcs
    fi
}


# SW-REQ-001:expiry
# Verifies: SW-REQ-001
# SW-REQ-001:boundary:nominal
# SW-REQ-001:boundary:negative
# MCDC SW-REQ-001: answer_age_s_GT_expires_s=F, expires_s_GT_0=T, forced_item_count_GT_0=F => TRUE [no-action: expires 7d just sealed; verify stays 0 not FORCED (expired)]
# MCDC SW-REQ-001: answer_age_s_GT_expires_s=T, expires_s_GT_0=T, forced_item_count_GT_0=T => TRUE
case_sw001_expiry() {
    new_repo
    cat > qc/checklists/accept.md << "EOF"
---
id: accept
scope: [README.md]
expires: 7d
---

- [ ] claim :: Which claim did you check against README.md?
EOF
    "$QC" mark accept/claim --pass -m "$EVIDENCE" >/dev/null
    "$QC" seal >/tmp/qc-ac-exp-seal.out
    set +e
    "$QC" verify >/tmp/qc-ac-exp-fresh.out 2>/tmp/qc-ac-exp-fresh.err
    code=$?
    set -e
    if [ "$code" -eq 0 ]; then
        say "SW-REQ-001:expiry fresh 7d answer verify=0"
    else
        bad "fresh expiry verify=$code"
        cat /tmp/qc-ac-exp-fresh.out /tmp/qc-ac-exp-fresh.err
    fi
    # Backdate the sealed attestation so answer_age_s > 7d.
    find qc/state .qc -name "*.qcs" 2>/dev/null | while read -r f; do
        if grep -q "accept/claim" "$f" 2>/dev/null; then
            sed -i.bak 's/at:[0-9TZ:-]*/at:2020-01-01T00:00:00Z/' "$f"
        fi
    done
    set +e
    "$QC" verify >/tmp/qc-ac-exp-old.out 2>/tmp/qc-ac-exp-old.err
    code=$?
    set -e
    if [ "$code" -eq 1 ] && grep -q "FORCED (expired)" /tmp/qc-ac-exp-old.out; then
        say "SW-REQ-001:expiry backdated 7d answer is FORCED (expired)"
    else
        bad "expired verify=$code"
        cat /tmp/qc-ac-exp-old.out /tmp/qc-ac-exp-old.err
    fi
}

# STK-REQ-001:cfg-error
# Verifies: STK-REQ-001
# STK-REQ-001:error_handling:negative
case_stk001_cfg_error() {
    new_repo
    cat > qc/checklists/broken.md << "EOF"
---
id: broken
scope: [README.md]
---

- [ ] claim :: Q? @notareal
EOF
    set +e
    "$QC" verify >/tmp/qc-ac-cfg.out 2>/tmp/qc-ac-cfg.err
    code=$?
    set -e
    if [ "$code" -eq 2 ] && grep -qi "parse error\|unknown annotation\|config" /tmp/qc-ac-cfg.err /tmp/qc-ac-cfg.out; then
        say "STK-REQ-001:cfg-error unknown @annotation verify=2"
    else
        bad "config error verify=$code (want 2)"
        cat /tmp/qc-ac-cfg.out /tmp/qc-ac-cfg.err
    fi
}

need_qc
run_case STK-REQ-001:AC-001 case_stk001_ac001
run_case STK-REQ-001:AC-002 case_stk001_ac002
run_case STK-REQ-001:AC-003 case_stk001_ac003
run_case STK-REQ-002:AC-001 case_stk002_ac001
run_case STK-REQ-002:AC-002 case_stk002_ac002
run_case STK-REQ-003:AC-001 case_stk003_ac001
run_case STK-REQ-003:AC-002 case_stk003_ac002
run_case STK-REQ-003:AC-003 case_stk003_ac003
run_case STK-REQ-003:AC-004 case_stk003_ac004
run_case STK-REQ-004:AC-001 case_stk004_ac001
run_case STK-REQ-004:AC-002 case_stk004_ac002
run_case STK-REQ-004:AC-003 case_stk004_ac003
run_case STK-REQ-005:AC-001 case_stk005_ac001
run_case STK-REQ-006:AC-001 case_stk006_ac001
run_case STK-REQ-006:AC-002 case_stk006_ac002
run_case STK-REQ-007:AC-001 case_stk007_ac001
run_case STK-REQ-007:AC-002 case_stk007_ac002
run_case STK-REQ-007:AC-003 case_stk007_ac003
run_case SW-REQ-001:expiry case_sw001_expiry
run_case STK-REQ-001:cfg-error case_stk001_cfg_error
if [ -n "$ONLY" ] && [ "$ran" -eq 0 ]; then echo unknown_case >&2; exit 2; fi
if [ "$fail" -ne 0 ]; then echo "$fail acceptance failures"; exit 1; fi
echo "acceptance tests passed ($ran cases)"
