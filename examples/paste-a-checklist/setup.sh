#!/bin/bash
# Make a fresh repo to try "paste a checklist into your agent".
#
#   ./setup.sh [dir]     default: a new temp directory
#   COPY=1 ./setup.sh    also put prompt + checklist on the clipboard
#
# <dir>/repo is the git repo the agent works in. <dir>/paste.txt is the
# message to paste: prompt.txt, a blank line, then checklist.md.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
DIR="${1:-}"
if [[ -z "$DIR" ]]; then
  DIR="$(mktemp -d "${TMPDIR:-/tmp}/qc-paste.XXXXXX")"
fi
mkdir -p "$DIR"
if [[ -n "$(ls -A "$DIR")" ]]; then
  echo "setup.sh: $DIR is not empty; give a new directory" >&2
  exit 2
fi

# qc from QC_BIN, else PATH, else a build in this checkout.
if [[ -z "${QC_BIN:-}" ]]; then
  if command -v qc >/dev/null 2>&1; then QC_BIN="$(command -v qc)"
  elif [[ -x "$HERE/../../qc" ]]; then QC_BIN="$HERE/../../qc"
  fi
fi
[[ -n "${QC_BIN:-}" && -x "$QC_BIN" ]] || {
  echo "qc not found. Install it (see the README) or set QC_BIN=/path/to/qc" >&2
  exit 127
}
QC_DIR="$(cd "$(dirname "$QC_BIN")" && pwd)"
"$QC_BIN" help | grep -q -- "--checklist" || {
  echo "this qc predates --checklist and nested items; build from main or update" >&2
  exit 2
}

REPO="$DIR/repo"
mkdir -p "$REPO"
# Seed without bytecode, so running python does not show up in git status.
(cd "$HERE/seed" && find . -name __pycache__ -prune -o -type f -print) | while IFS= read -r f; do
  mkdir -p "$REPO/$(dirname "$f")"
  cp "$HERE/seed/$f" "$REPO/$f"
done
cd "$REPO"
git init -q -b main
git config user.email "demo@example.com"
git config user.name "qc example"
PATH="$QC_DIR:$PATH" qc init >/dev/null
git add -A
git commit -q -m "seed reservation retry with qc"

{ cat "$HERE/prompt.txt"; echo; cat "$HERE/checklist.md"; } > "$DIR/paste.txt"
if [[ "${COPY:-}" == "1" ]]; then
  if command -v pbcopy >/dev/null; then pbcopy < "$DIR/paste.txt"; echo "COPIED paste.txt to the clipboard"
  elif command -v xclip >/dev/null; then xclip -selection clipboard < "$DIR/paste.txt"; echo "COPIED paste.txt to the clipboard"
  else echo "no pbcopy/xclip; open $DIR/paste.txt and copy it"; fi
fi

echo "LIVE_DIR $DIR"
echo "PASTE    $DIR/paste.txt"
echo "START    cd $REPO && QC_BY=agent PATH=$QC_DIR:\$PATH claude"
