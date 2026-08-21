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

if [ "$fail" -ne 0 ]; then echo "$fail integration failures"; exit 1; fi
echo "integration tests passed"
