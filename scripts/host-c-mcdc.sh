#!/bin/sh
# Host-gcc coverage path for qc.
# Product APE stays cosmocc (`make`). cosmocc cannot link -lgcov, so this
# script builds the same src/*.c with Homebrew gcc-15, runs host tests, and
# exports honest gcov condition JSON. Do not treat this binary as the APE.
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

HOSTCC="${HOSTCC:-/opt/homebrew/bin/gcc-15}"
GCOV="${GCOV:-/opt/homebrew/bin/gcov-15}"
GCOVR="${GCOVR:-gcovr}"

OUT="$ROOT/.proof/native"
OBJ="$OUT/obj"
GCOVDIR="$OUT/gcov"
RAW="$GCOVDIR/raw"
BIN_QC="$OUT/qc"
BIN_KERNEL="$OUT/test_kernel"

if [ ! -x "$HOSTCC" ]; then
    echo "host-c-mcdc: HOSTCC not executable: $HOSTCC" >&2
    exit 2
fi
if [ ! -x "$GCOV" ]; then
    echo "host-c-mcdc: GCOV not executable: $GCOV" >&2
    exit 2
fi

CFLAGS="-I${ROOT}/include -O0 -g -fprofile-arcs -ftest-coverage -fcondition-coverage -fprofile-abs-path"
LDFLAGS="-lgcov"
if [ -z "${SDKROOT:-}" ] && command -v xcrun >/dev/null 2>&1; then
    SDKROOT=$(xcrun --show-sdk-path 2>/dev/null || true)
fi
if [ -n "${SDKROOT:-}" ]; then
    CFLAGS="$CFLAGS -isysroot $SDKROOT"
    LDFLAGS="$LDFLAGS -Wl,-syslibroot,$SDKROOT"
fi

SRCS="util.c globmatch.c digest.c parser.c store.c states.c templates.c commands.c cli.c"

rm -rf "$OUT"
mkdir -p "$OBJ" "$RAW"

echo "== host compile (gcc-15, all src/*.c) =="
for s in $SRCS; do
    "$HOSTCC" $CFLAGS -c "$ROOT/src/$s" -o "$OBJ/${s%.c}.o"
done

"$HOSTCC" $CFLAGS -o "$BIN_QC" \
    "$OBJ/util.o" "$OBJ/globmatch.o" "$OBJ/digest.o" "$OBJ/parser.o" \
    "$OBJ/store.o" "$OBJ/states.o" "$OBJ/templates.o" "$OBJ/commands.o" \
    "$OBJ/cli.o" $LDFLAGS

"$HOSTCC" $CFLAGS -c "$ROOT/tests/test_kernel.c" -o "$OBJ/test_kernel.o"
"$HOSTCC" $CFLAGS -o "$BIN_KERNEL" \
    "$OBJ/test_kernel.o" \
    "$OBJ/util.o" "$OBJ/globmatch.o" "$OBJ/digest.o" \
    "$OBJ/parser.o" "$OBJ/store.o" "$OBJ/states.o" \
    $LDFLAGS

echo "== host kernel =="
"$BIN_KERNEL"

echo "== host unit (commands-linked helpers) =="
"$HOSTCC" $CFLAGS -c "$ROOT/tests/test_host_unit.c" -o "$OBJ/test_host_unit.o"
"$HOSTCC" $CFLAGS -o "$OUT/test_host_unit" \
    "$OBJ/test_host_unit.o" \
    "$OBJ/util.o" "$OBJ/globmatch.o" "$OBJ/digest.o" \
    "$OBJ/parser.o" "$OBJ/store.o" "$OBJ/states.o" \
    "$OBJ/templates.o" "$OBJ/commands.o" $LDFLAGS
"$OUT/test_host_unit"

echo "== host qc smoke (cli / templates / help) =="
"$BIN_QC" --help >/dev/null
"$BIN_QC" -h >/dev/null
"$BIN_QC" --version >/dev/null
"$BIN_QC" -V >/dev/null
"$BIN_QC" help >/dev/null
for topic in init add template plan decide mark baseline seal reset verify compact report help; do
    "$BIN_QC" help "$topic" >/dev/null
done
set +e
"$BIN_QC" help not-a-topic >/dev/null
"$BIN_QC" nosuch >/dev/null
"$BIN_QC" template show >/dev/null
"$BIN_QC" template bogus >/dev/null
set -e
"$BIN_QC" template list >/dev/null
"$BIN_QC" template show docs >/dev/null

echo "== host extras (commands not covered by APE-only cases) =="
EXTRA=$(mktemp -d /tmp/qc-host-extra.XXXXXX)
(
    cd "$EXTRA"
    git init -q -b main
    git config user.email host@test
    git config user.name host
    printf "hello 1\n" > README.md
    git add README.md
    git commit -qm seed
    "$BIN_QC" init >/dev/null
    "$BIN_QC" template list >/dev/null
    "$BIN_QC" template show docs >/dev/null
    "$BIN_QC" add docs >/dev/null
    set +e
    "$BIN_QC" add docs >/dev/null
    "$BIN_QC" add not-a-template >/dev/null
    "$BIN_QC" add >/dev/null
    set -e
    "$BIN_QC" add docs --full >/dev/null || true
    # docs already exists; copy a second template
    "$BIN_QC" add ai-generated-code --full >/dev/null
    "$BIN_QC" decide docs --yes -m "README.md is a document we claim 1 thing about" >/dev/null || true
    "$BIN_QC" baseline -m "grandfathered existing README.md at adoption 1" >/dev/null || true
    "$BIN_QC" report >/dev/null || true
    "$BIN_QC" report --md >/dev/null || true
    "$BIN_QC" plan --json >/dev/null || true
    "$BIN_QC" verify --quiet >/dev/null || true
    "$BIN_QC" reset --item docs/accuracy >/dev/null || true
    "$BIN_QC" reset >/dev/null || true
    "$BIN_QC" mark docs/accuracy --fail -m "README.md claim 1 is unchecked" >/dev/null || true
    "$BIN_QC" mark docs/accuracy --na -m "README.md is not a code claim 1" >/dev/null || true
    "$BIN_QC" compact >/dev/null || true
    for t in init add template plan decide mark baseline seal reset verify compact report; do
        "$BIN_QC" "$t" --help >/dev/null || true
    done
)
rm -rf "$EXTRA"

echo "== host cover extras =="
QC="$BIN_QC" sh "$ROOT/tests/host_cover.sh"

echo "== host integration (run.sh against host qc) =="
QC="$BIN_QC" sh "$ROOT/tests/run.sh"

echo "== host acceptance (APE-magic case skipped) =="
# STK-REQ-005:AC-001 asserts Cosmopolitan APE magic on the vendored qc/qc.
# The host binary is not the APE; that case stays on `make test`.
for case in \
    STK-REQ-001:AC-001 \
    STK-REQ-001:AC-002 \
    STK-REQ-001:AC-003 \
    STK-REQ-002:AC-001 \
    STK-REQ-002:AC-002 \
    STK-REQ-003:AC-001 \
    STK-REQ-003:AC-002 \
    STK-REQ-003:AC-003 \
    STK-REQ-003:AC-004 \
    STK-REQ-004:AC-001 \
    STK-REQ-004:AC-002 \
    STK-REQ-004:AC-003 \
    STK-REQ-006:AC-001 \
    STK-REQ-006:AC-002 \
    STK-REQ-007:AC-001 \
    STK-REQ-007:AC-002 \
    STK-REQ-007:AC-003 \
    SW-REQ-001:expiry \
    STK-REQ-001:cfg-error
do
    echo "-- host $case"
    QC="$BIN_QC" sh "$ROOT/tests/acceptance.sh" "$case"
done

echo "== gcov-15 --conditions --json-format =="
(
    cd "$OBJ"
    for s in $SRCS; do
        "$GCOV" --conditions --json-format -o "$OBJ" "$ROOT/src/$s" >/dev/null
    done
)
# gcov writes <file>.gcov.json.gz next to the notes / cwd
find "$OBJ" "$ROOT" -maxdepth 2 \( -name '*.gcov.json' -o -name '*.gcov.json.gz' \) -print 2>/dev/null \
    | while read -r f; do
        mv "$f" "$RAW/" 2>/dev/null || true
    done
# also collect from ROOT if gcov dropped them beside sources
find "$ROOT/src" -maxdepth 1 \( -name '*.gcov.json' -o -name '*.gcov.json.gz' \) -print 2>/dev/null \
    | while read -r f; do
        mv "$f" "$RAW/" 2>/dev/null || true
    done

echo "== gcovr merge =="
"$GCOVR" \
    --root "$ROOT" \
    --filter "$ROOT/src/" \
    --exclude "$ROOT/tests/" \
    --gcov-executable "$GCOV" \
    --object-directory "$OBJ" \
    --json-pretty \
    --json "$GCOVDIR/merged.gcov.json" \
    --txt "$GCOVDIR/summary.txt" \
    --print-summary

echo "host-c-mcdc: wrote $GCOVDIR/merged.gcov.json"
