#!/bin/sh
# integration: evidence reject, self-scope guard, init+verify
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
QC="${QC:-$ROOT/qc}"
fail=0
say() { echo "ok   $*"; }
bad() { echo "FAIL $*"; fail=$((fail+1)); }

TMP=$(mktemp -d /tmp/qc-int.XXXXXX)
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

# --- init + verify on empty tree ---
mkdir -p "$TMP/repo"
cd "$TMP/repo"
git init -q
"$QC" init >/tmp/qc-init.out
code=$?
if [ "$code" -eq 0 ] && [ -d qc ] && [ -d .qc ]; then say "init creates qc/ and .qc/"; else bad "init"; fi
"$QC" verify >/tmp/qc-ver.out
code=$?
if [ "$code" -eq 0 ]; then say "verify on empty inited tree is 0"; else bad "verify empty exit $code"; fi

# --- evidence reject ---
"$QC" add docs >/tmp/qc-add.out || true
# docs has attest items; mark with denylist token
set +e
"$QC" mark docs/accuracy --pass -m "looks good" >/tmp/qc-mark.out 2>/tmp/qc-mark.err
code=$?
set -e
if [ "$code" -eq 2 ]; then
    if grep -q "falsifiable" /tmp/qc-mark.err /tmp/qc-mark.out 2>/dev/null; then
        say "evidence 'looks good' rejected with principle"
    else
        bad "reject message missing principle"
        cat /tmp/qc-mark.err /tmp/qc-mark.out
    fi
    if grep -qi "digit\|backtick\|http\|token that would" /tmp/qc-mark.err /tmp/qc-mark.out 2>/dev/null; then
        # mentioning the category is the principle; mentioning a passing string would fail I5.
        # we only fail if it prints an example passing evidence.
        :
    fi
else
    bad "looks good should exit 2, got $code"
    cat /tmp/qc-mark.err /tmp/qc-mark.out
fi

# --- self-scope guard ---
mkdir -p "$TMP/repo/qc/checklists" "$TMP/repo/qc/state"
printf '%s\n' '---' 'id: bad-scope' 'scope: [qc/state/**]' '---' '' \
  '- [ ] loop :: Does this scope the store? @scope(qc/state/**)' \
  > "$TMP/repo/qc/checklists/bad-scope.md"
# also drop a dummy state file so resolve can see it
printf '%s\n' '# qc-state v1' > "$TMP/repo/qc/state/base.qcs"
set +e
"$QC" verify >/tmp/qc-self.out 2>/tmp/qc-self.err
code=$?
set -e
if [ "$code" -eq 2 ]; then say "self-scope guard exits 2"; else bad "self-scope exit $code"; cat /tmp/qc-self.err /tmp/qc-self.out; fi

# --- help is doctrine ---
"$QC" help >/tmp/qc-help.out
if grep -q "NEXT" /tmp/qc-help.out && grep -q "WHAT" /tmp/qc-help.out; then
    say "help is agent doctrine"
else
    bad "help missing WHAT/NEXT"
fi

if grep -q "qc cannot read" /tmp/qc-help.out && grep -q -- "--checklist <path>" /tmp/qc-help.out; then
    say "help says to write a pasted checklist to a file"
else
    bad "help missing pasted-checklist doctrine"
fi

# --- nested checklist outside the repo via --checklist ---
mkdir -p "$TMP/nest/src" "$TMP/pasted"
cd "$TMP/nest"
git init -q
echo 'x = 1' > src/a.py
"$QC" init >/dev/null
CL="$TMP/pasted/nested.md"
printf '%s\n' '---' 'id: nested' 'scope: [src/**]' '---' \
  '- [ ] top :: Does the parent hold? @evidence(src/a.py)' \
  '  - [ ] kid :: Does the child hold? @evidence(src/a.py line 1)' > "$CL"
set +e
"$QC" plan --checklist "$TMP/pasted/missing.md" >/dev/null 2>&1
c_missing=$?
"$QC" plan --checklist "$CL" >/tmp/qc-nest-plan.out 2>&1
c_plan=$?
"$QC" mark nested/top --pass -m "src/a.py line 1" --checklist "$CL" >/dev/null 2>&1
c_parent=$?
"$QC" mark nested/kid --pass -m "src/a.py line 1 sets x = 1" --checklist "$CL" >/dev/null 2>&1
c_kid=$?
QC_CHECKLIST="$CL" "$QC" mark nested/top --pass -m "src/a.py line 1" >/dev/null 2>&1
c_top=$?
"$QC" verify --checklist "$CL" >/dev/null 2>&1
c_clear=$?
echo 'x = 2' > src/a.py
"$QC" verify --checklist "$CL" >/dev/null 2>&1
c_stale=$?
"$QC" report --md --checklist "$CL" >/tmp/qc-nest-report.out 2>&1
set -e
if [ "$c_missing" -eq 2 ]; then say "--checklist missing file exits 2"; else bad "--checklist missing exit $c_missing"; fi
if [ "$c_plan" -eq 1 ] && grep -q "Evidence -m must name: src/a.py line 1" /tmp/qc-nest-plan.out; then
    say "plan shows the @evidence hint"; else bad "plan hint (exit $c_plan)"; cat /tmp/qc-nest-plan.out; fi
if [ "$c_parent" -eq 2 ] && [ "$c_kid" -eq 0 ] && [ "$c_top" -eq 0 ]; then
    say "parent pass refused until the child is clear"; else bad "parent order $c_parent $c_kid $c_top"; fi
if [ "$c_clear" -eq 0 ] && [ "$c_stale" -eq 1 ]; then say "nested verify clears, then reopens on edit"; else bad "nested verify $c_clear $c_stale"; fi
if grep -q "| parent | asked for |" /tmp/qc-nest-report.out && grep -q "| top | src/a.py line 1 |" /tmp/qc-nest-report.out; then
    say "report --md shows parent and evidence hint"; else bad "report columns"; cat /tmp/qc-nest-report.out; fi

# --- several annotations on one item all parse ---
printf '%s\n' '---' 'id: multi' 'scope: [src/**]' '---' \
  '- [ ] both :: Two annotations? @scope(src/**) @evidence(src/a.py line 1)' \
  '- [ ] ran :: Machine checked? @scope(src/**) @run(true)' > "$TMP/pasted/multi.md"
set +e
"$QC" plan --checklist "$TMP/pasted/multi.md" >/tmp/qc-multi.out 2>&1
set -e
if grep -q "Evidence -m must name: src/a.py line 1" /tmp/qc-multi.out && ! grep -q "multi/ran" /tmp/qc-multi.out; then
    say "@scope followed by @evidence or @run both parse"; else bad "multi annotations"; cat /tmp/qc-multi.out; fi

# --- long snake_case names and commit shas are evidence, not secrets ---
set +e
"$QC" mark nested/kid --pass -m "test_retry_at_exactly_24_hours_is_same in src/a.py" --checklist "$CL" >/dev/null 2>&1
c_snake=$?
"$QC" mark nested/kid --pass -m "commit 0123456789abcdef0123456789abcdef01234567 src/a.py" --checklist "$CL" >/dev/null 2>&1
c_sha=$?
"$QC" mark nested/kid --pass -m "key QWERTYuiopASDFGHjklZXCVBNm1234567890 src/a.py" --checklist "$CL" >/dev/null 2>&1
c_tok=$?
set -e
if [ "$c_snake" -eq 0 ] && [ "$c_sha" -eq 0 ] && [ "$c_tok" -eq 2 ]; then
    say "secret filter spares identifiers and shas, refuses mixed-case blobs"; else bad "secret filter $c_snake $c_sha $c_tok"; fi

if [ "$fail" -ne 0 ]; then echo "$fail integration failures"; exit 1; fi
echo "integration tests passed"
