#!/bin/sh
# Install qc (Cosmopolitan APE) from the latest GitHub release.
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/probelabs/qc/main/scripts/install.sh | sh
# Optional (env must be visible to sh, not only to curl):
#   curl -fsSL … | QC_INSTALL_DIR=~/bin sh
#   curl -fsSL … | QC_VERSION=v0.1.0 sh   # pin a release tag (default: latest)
#
# APE binaries must be invoked from a shell (e.g. `qc help`), not opened as
# a document from a GUI file manager.

set -eu
# pipefail when the interpreting shell supports it (bash/zsh; ignored on dash)
(set -o pipefail) 2>/dev/null && set -o pipefail

REPO="probelabs/qc"
ASSET="qc"
DEFAULT_DIR="${HOME}/.local/bin"
INSTALL_DIR="${QC_INSTALL_DIR:-$DEFAULT_DIR}"
VERSION="${QC_VERSION:-latest}"

err() {
  printf 'qc install: %s\n' "$*" >&2
  exit 1
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || err "required command not found: $1"
}

need_cmd curl
need_cmd mkdir
need_cmd chmod
need_cmd mktemp
need_cmd uname
need_cmd cp
need_cmd mv
need_cmd rm
need_cmd wc
need_cmd tr

# Cosmopolitan APE is one multi-platform binary; OS/arch only for messaging.
OS="$(uname -s 2>/dev/null || echo unknown)"
ARCH="$(uname -m 2>/dev/null || echo unknown)"
printf 'qc install: detected %s/%s (APE binary is universal)\n' "$OS" "$ARCH"

if [ "$VERSION" = "latest" ]; then
  DOWNLOAD_URL="https://github.com/${REPO}/releases/latest/download/${ASSET}"
  LABEL="latest"
else
  # Accept v0.1.0 or 0.1.0
  case "$VERSION" in
    v*) TAG="$VERSION" ;;
    *) TAG="v$VERSION" ;;
  esac
  DOWNLOAD_URL="https://github.com/${REPO}/releases/download/${TAG}/${ASSET}"
  LABEL="$TAG"
fi

TMPDIR_INSTALL="$(mktemp -d "${TMPDIR:-/tmp}/qc-install.XXXXXX")"
cleanup() {
  rm -rf "$TMPDIR_INSTALL"
}
trap cleanup EXIT INT HUP TERM

TMP_BIN="${TMPDIR_INSTALL}/${ASSET}"

printf 'qc install: downloading %s → %s\n' "$LABEL" "$DOWNLOAD_URL"
if ! curl -fsSL -o "$TMP_BIN" "$DOWNLOAD_URL"; then
  err "download failed: ${DOWNLOAD_URL}"
fi

if [ ! -s "$TMP_BIN" ]; then
  err "download produced an empty file: ${DOWNLOAD_URL}"
fi

# Reject obvious HTML error pages / empty stubs from misconfigured releases.
SIZE="$(wc -c < "$TMP_BIN" | tr -d ' ')"
if [ "$SIZE" -lt 10000 ]; then
  err "downloaded file looks too small (${SIZE} bytes); release asset missing?"
fi

mkdir -p "$INSTALL_DIR"
DEST="${INSTALL_DIR}/qc"
TMP_DEST="${DEST}.new.$$"
cp "$TMP_BIN" "$TMP_DEST"
chmod +x "$TMP_DEST"
mv -f "$TMP_DEST" "$DEST"

printf 'qc install: installed %s (%s bytes)\n' "$DEST" "$SIZE"
printf 'qc install: note — run qc from a shell (APE); do not open the binary in a GUI\n'

case ":${PATH}:" in
  *":${INSTALL_DIR}:"*) ;;
  *)
    printf '\nqc install: %s is not on your PATH.\n' "$INSTALL_DIR"
    printf 'Add it for this session:\n'
    printf '  export PATH="%s:$PATH"\n' "$INSTALL_DIR"
    printf 'Or add that line to your shell profile (~/.bashrc, ~/.zshrc, …).\n\n'
    ;;
esac

printf 'Try: %s help\n' "$DEST"
