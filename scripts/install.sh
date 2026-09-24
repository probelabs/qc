#!/bin/sh
# Install qc (Cosmopolitan APE) from the latest GitHub release.
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/probelabs/qc/main/scripts/install.sh | sh
# Optional (env must be visible to sh, not only to curl):
#   curl -fsSL … | QC_INSTALL_DIR=~/bin sh
#   curl -fsSL … | QC_VERSION=v0.1.0 sh   # pin a release tag (default: latest)
#
# Works on Linux, macOS, and Windows Git Bash / MSYS / Cygwin / MinGW.
# APE binaries must be invoked from a shell (e.g. `qc help`), not opened as
# a document from a GUI file manager.

set -eu
# pipefail when the interpreting shell supports it (bash/zsh; ignored on dash)
(set -o pipefail) 2>/dev/null && set -o pipefail

REPO="probelabs/qc"
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

# Cosmopolitan APE is one multi-platform binary; OS/arch only for messaging
# and for choosing the install filename (.exe on Windows-ish hosts).
OS="$(uname -s 2>/dev/null || echo unknown)"
ARCH="$(uname -m 2>/dev/null || echo unknown)"
printf 'qc install: detected %s/%s (APE binary is universal)\n' "$OS" "$ARCH"

# Git Bash / MSYS / Cygwin / MinGW report uname like MINGW64_NT-10.0, MSYS_NT-…, CYGWIN_NT-…
BIN_NAME="qc"
case "$OS" in
  MINGW*|MSYS*|CYGWIN*|mingw*|msys*|cygwin*)
    BIN_NAME="qc.exe"
    ;;
esac

# Prefer the obviously-named Windows asset when installing as .exe; fall back to `qc`.
ASSET="qc"
if [ "$BIN_NAME" = "qc.exe" ]; then
  ASSET="qc.exe"
fi

if [ "$VERSION" = "latest" ]; then
  DOWNLOAD_URL="https://github.com/${REPO}/releases/latest/download/${ASSET}"
  FALLBACK_URL="https://github.com/${REPO}/releases/latest/download/qc"
  LABEL="latest"
else
  # Accept v0.1.0 or 0.1.0
  case "$VERSION" in
    v*) TAG="$VERSION" ;;
    *) TAG="v$VERSION" ;;
  esac
  DOWNLOAD_URL="https://github.com/${REPO}/releases/download/${TAG}/${ASSET}"
  FALLBACK_URL="https://github.com/${REPO}/releases/download/${TAG}/qc"
  LABEL="$TAG"
fi

TMPDIR_INSTALL="$(mktemp -d "${TMPDIR:-/tmp}/qc-install.XXXXXX")"
cleanup() {
  rm -rf "$TMPDIR_INSTALL"
}
trap cleanup EXIT INT HUP TERM

TMP_BIN="${TMPDIR_INSTALL}/${BIN_NAME}"

printf 'qc install: downloading %s → %s\n' "$LABEL" "$DOWNLOAD_URL"
if ! curl -fsSL -o "$TMP_BIN" "$DOWNLOAD_URL"; then
  if [ "$ASSET" != "qc" ]; then
    printf 'qc install: %s not found; falling back to qc asset\n' "$ASSET"
    if ! curl -fsSL -o "$TMP_BIN" "$FALLBACK_URL"; then
      err "download failed: ${DOWNLOAD_URL} and ${FALLBACK_URL}"
    fi
    DOWNLOAD_URL="$FALLBACK_URL"
  else
    err "download failed: ${DOWNLOAD_URL}"
  fi
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
DEST="${INSTALL_DIR}/${BIN_NAME}"
TMP_DEST="${DEST}.new.$$"
cp "$TMP_BIN" "$TMP_DEST"
chmod +x "$TMP_DEST"
mv -f "$TMP_DEST" "$DEST"

printf 'qc install: installed %s (%s bytes)\n' "$DEST" "$SIZE"
printf 'qc install: note — run qc from a shell (APE); do not open the binary in a GUI\n'

# On Apple Silicon, APE may self-extract a user-local loader on first run.
# System-wide `ape install` (sudo) is optional and not required for a smoke test.
case "$OS:$ARCH" in
  Darwin:arm64|Darwin:aarch64)
    printf 'qc install: note — on Apple Silicon, first run may self-extract an APE loader under TMPDIR/HOME (no sudo needed)\n'
    ;;
esac

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
