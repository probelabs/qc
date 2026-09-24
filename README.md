# qc

Deterministic checklist gate for AI-driven development. Agents are non-deterministic; a content-addressed attestation is not. `qc` records evidenced answers bound to file bytes and refuses to clear until every applicable item has one.

<!-- Documents: STK-REQ-001, SYS-REQ-001, SW-REQ-001 -->

## Install

`qc` is a Cosmopolitan [APE](https://justine.lol/ape.html) universal binary — one release asset runs on Linux, macOS, and Windows from a shell (do not open it from a GUI file manager).

### One-liner

```bash
curl -fsSL https://raw.githubusercontent.com/probelabs/qc/main/scripts/install.sh | sh
```

Installs the latest GitHub release asset `qc` to `~/.local/bin/qc`. No root required. Overrides (pass env to `sh`, not only to `curl`): `curl … | QC_INSTALL_DIR=~/bin sh`, `curl … | QC_VERSION=v0.1.0 sh`. Then:

```bash
qc help
```

If `~/.local/bin` is not on your `PATH`, the installer prints the export to add.

### Build from source

Needs [cosmocc](https://cosmo.zip/pub/cosmocc/cosmocc.zip):

```bash
git clone https://github.com/probelabs/qc.git
cd qc

# Makefile defaults COSMOCC to a local path; override for your machine:
export COSMOCC=/path/to/cosmocc/bin/cosmocc
make                 # writes ./qc (APE). Invoke it from a shell as ./qc
./qc help
```

In a consumer repo, `qc init` vendors that APE as `qc/qc`. Call it as `./qc/qc`, or put `qc/` on your `PATH` so plain `qc` works (the quickstart below assumes that).

<!-- Documents: STK-REQ-005, SYS-REQ-003, SW-REQ-003 -->

## 60-second quickstart

From a git repo (throwaway is fine). Assumes `qc` resolves to the APE (`export PATH="/path/to/qc-build:$PATH"` before init, or `export PATH="$PWD/qc:$PATH"` after).

```bash
qc init
qc template list
qc add docs
qc plan
```

`plan` prints the ordered worklist. A checklist with `applies_when` starts with a decision:

```bash
export QC_BY=agent
qc decide docs --yes -m "README.md documents the public install and usage surface for this change"
qc plan
```

Answer with falsifiable evidence (a real path, digit, URL, or backtick span). Vague ticks are rejected; the tool names the next command, never a passing phrase:

```bash
qc mark docs/accuracy --pass -m "checked README.md section headings against the files present in the worktree"
qc mark docs/stale --pass -m "README.md is new; no prior install steps to retire"
qc verify                 # loop mode: scratch + committed state
```

When the worktree is what you intend to ship:

```bash
git add qc/
qc seal --staged         # promote matching scratch into this branch segment
qc verify --staged       # predicts CI: committed/index state only, no scratch
qc report --md           # claims table for the PR summary
```

Exit codes: `0` all clear · `1` open items · `2` config/parse error.

<!-- Documents: STK-REQ-003, SYS-REQ-004, INT-REQ-001 -->

## Core concepts

### Digest and attestation

An attestation is a claim about content. Its logical key is `(item_ref, digest)`. The digest is a 16-hex SHA-256 prefix over the item identity text plus the sorted, normalized hashes of files in the item's scope. Rewording the question or changing scoped bytes forces the item again. Forcing means staleness, never, or expiry — there is no second mechanism.

<!-- Documents: STK-REQ-001, SYS-REQ-001, SW-REQ-001, INT-REQ-004 -->

### `qc/` vs `.qc/`

| Path | Git | Role |
|---|---|---|
| `qc/` | committed | Vendored APE (`qc/qc`), checklists, config, state segments/base — the review surface |
| `.qc/` | ignored | Scratch attestations, manifests, cache — disposable local workspace |

`rm -rf .qc/` is always safe (same class as `qc reset`). Hand-editing `qc/state/` or `.qc/scratch.qcs` is not; only `qc mark` / `qc decide` / `qc baseline` / `qc seal` / `qc compact` write state.

<!-- Documents: STK-REQ-004, SYS-REQ-005, SW-REQ-005, SW-REQ-007, INT-REQ-002 -->

### Exit codes

| Code | Meaning |
|---|---|
| 0 | All clear |
| 1 | Open items (forced, failed, or awaiting decision) |
| 2 | Config or parse error |

This triple is the machine API. Help and errors name the **next** command; they never name a string that would satisfy the gate (I5).

<!-- Documents: STK-REQ-002, SYS-REQ-002, SW-REQ-002 -->

### Verify modes

| Mode | State sources | Question |
|---|---|---|
| `qc verify` | scratch + committed | Am I ready? (agent loop) |
| `qc verify --staged` | index/committed only | Will this commit pass CI? |
| `qc verify --ci` | committed only, read-only | Gate of record |

Unsealed scratch is invisible to `--staged` and `--ci`. Forgetting `seal` does not corrupt state; it leaves items forced where it matters.

Also: `--only <ref>`, `--quiet`, `--json`.

<!-- Documents: INT-REQ-001, SYS-REQ-001 -->

### Branch state without merge fights

`mark` / `decide` / `baseline` write scratch only. `seal` appends to a branch-owned `qc/state/seg-*.qcs`. `compact` (trunk only) folds segments into `base.qcs`. Parallel branches write different segment files, so merges are both-add — correctness does not depend on merge drivers.

<!-- Documents: STK-REQ-007, SYS-REQ-007, SW-REQ-007, INT-REQ-002 -->

### I5 — every line is a prompt

Help, errors, and worklists name the next `qc …` command. They never print a phrase that would satisfy the evidence checker. Agents copy NEXT lines; applause tokens (`lgtm`, `done`, `ok`) are rejected at mark time.

<!-- Documents: STK-REQ-002, SYS-REQ-002, SW-REQ-002 -->

### `QC_BY=agent`

Set `export QC_BY=agent` (or pass `--by agent`) so attestations record provenance. Agents must not mark `@human` items — the CLI refuses and tells you to surface the item to a person.

<!-- Documents: INT-REQ-003, SW-REQ-002 -->

## Command reference

Topic help: `qc help` then `qc help <topic>`. Every topic ends with NEXT.

### `qc help [topic]`

```bash
qc help
qc help mark
qc help verify
```

Topics: `init` `add` `template` `plan` `decide` `mark` `baseline` `seal` `reset` `verify` `compact` `report`.

### `qc init`

Creates committed `qc/` (vendors this APE as `qc/qc`), ignored `.qc/`, and one-line `.gitignore` / `.gitattributes` updates.

```bash
qc init
# Created qc/ (tool + checklists, committed) and .qc/ (local workspace, ignored)
# NEXT: qc template list
```

### `qc template list` / `qc template show <t>`

Gallery of bundled checklists. `show` prints the minimal file you would copy.

```bash
qc template list
#   docs                 claims verified against code
#   code-quality         quality questions for application code
#   …

qc template show docs
# ---
# id: docs
# applies_when: The change is documentation
# scope: [docs/**, README.md]
# …
```

### `qc add <t> [--full]`

Copies a template into `qc/checklists/`. Minimal by default; `--full` keeps every item.

```bash
qc add docs
# Copied builtin/docs@1 (minimal) → qc/checklists/docs.md

qc add security-review --full
# Copied builtin/security-review@1 (full) → qc/checklists/security-review.md
```

Adapt after copy; there is no runtime `extends` (legibility beats DRY).

### `qc plan [--json]`

Ordered worklist before work: forced items, questions, previous answers, decision requests.

```bash
qc plan
# 1  docs/@applies   FORCED (never)
#    Q: The change is documentation
#    NEXT: qc decide docs --yes|--no -m "…"

qc plan --json | head -c 200
# {"items":[{"ref":"docs/@applies","state":"FORCED (never)",…},…]}
```

### `qc decide <id> --yes|--no -m "…"`

Applicability for checklists with `applies_when`. Writes scratch only. Exclusions are diffable Statements of Applicability.

```bash
export QC_BY=agent
qc decide docs --yes -m "README.md documents the public install and usage surface for this change"
qc decide code-quality --no -m "this change is docs-only README.md; src.rs is a stub not shipped"
```

### `qc mark <ref> --pass|--fail|--na -m "…"`

Evidenced answer bound to the current worktree digest → scratch.

```bash
qc mark docs/accuracy --pass -m "checked README.md section headings against the files present in the worktree"
qc mark docs/stale --fail -m "README.md still mentions cosmocc.zip path that Makefile no longer uses"
qc mark docs/stale --na -m "docs/ is empty; only README.md exists and it is new in this PR"
```

### `qc baseline [-m "…"] [--item <ref>]`

Grandfather current content at adoption. Writes scratch; seal afterward so trunk sees it.

```bash
qc baseline -m "adopting docs checklist on an existing README.md that already matches the code"
qc baseline --item code-quality/naming -m "greenfield src.rs with a single fn main; naming debt accepted at adoption"
qc seal --staged
```

### `qc seal [--staged]`

Promote scratch entries whose digest still matches into this branch's `qc/state/seg-*.qcs`.

```bash
git add qc/ README.md
qc seal --staged
# seal: promoted 4 of 4 → qc/state/seg-main-28cd64.qcs
# NEXT: qc verify --staged
```

Without `--staged`, seal uses the worktree view; `--staged` matches what the index will commit.

### `qc reset [--item <ref>]`

Wipe scratch, or one scratch entry. Committed `qc/state/` is untouched. Same safety class as `rm -rf .qc/`.

```bash
qc reset --item docs/stale
# removed docs/stale from scratch. Committed state is untouched.
qc reset                 # wipe all scratch
qc plan
```

### `qc verify` modes

```bash
qc verify                         # loop: scratch + committed
qc verify --staged                # index/committed only — predicts CI
qc verify --ci                    # committed only, read-only gate of record
qc verify --only docs/accuracy    # single ref
qc verify --quiet                 # exit code only (still prints forced/failed lines)
qc verify --json                  # machine-readable items[]
```

Example `--only` clear:

```bash
qc verify --only docs/accuracy
# CLEAR              docs/accuracy
# all clear
```

### `qc compact` (trunk only)

Fold segments into `base.qcs`, one line per item, drop orphans, commit. Refuses off-trunk so parallel branches cannot rewrite shared base.

```bash
git checkout main
qc compact

# On a feature branch:
qc compact
# REFUSED — compact rewrites shared base.qcs; it runs on trunk only.
# NEXT: git checkout main && qc compact
```

### `qc report [--md]`

Claims table for reviewers / CI summaries.

```bash
qc report --md
# | item | status | by | age | digest | evidence |
# |---|---|---|---|---|---|
# | docs/accuracy | CLEAR | agent | | b1a1bbdb28626343 | checked README.md section headings… |
```

<!-- Documents: STK-REQ-002, INT-REQ-003, SW-REQ-002 -->

## Evidence, `@human`, and `@run`

### Evidence rejection (I5)

Every `pass` / `fail` / `n_a` needs `-m` evidence. Shape check at mark time:

- Rejects empty applause (`done`, `ok`, `lgtm`, … and `deny_evidence`).
- Requires enough substance **and** at least one falsifiable token: an existing repo path, a digit, a URL, or a ``backticked`` span.
- Refuses common secret shapes (AWS keys, `ghp_`, `sk-`, PEM blocks, high-entropy blobs).

```bash
qc mark docs/accuracy --pass -m "lgtm"
# REJECTED — evidence must point at something falsifiable (a path, a number, a command, a link),
# not restate that checking happened.
# NEXT: qc mark <ref> --pass -m "…"
```

Rejection messages state the principle and name the next `qc mark` command. They do not reveal a phrase that would pass.

### `@human` refuse

Items tagged `@human` require a human `--by <name>`. With `QC_BY=agent` the CLI refuses:

```bash
export QC_BY=agent
qc mark code-quality/diff-read --pass -m "read src.rs top to bottom; only fn main present"
# REFUSED — this item is @human. An agent must not attest it.
# Principle: human approval is provenance the PR author cannot forge locally.
# NEXT: surface code-quality/diff-read to the user; they run
#       qc mark code-quality/diff-read --pass -m "…" --by <their-name>
```

### `@run` fail-closed

`@run(cmd)` items are executed by the gate (exit 0 clears). A missing binary is a loud **FAILED** with an install hint — never a skip.

```bash
qc verify --only code-quality/lint
# ruff: No such file or directory
# FAILED             code-quality/lint
#   @run exited 127. binary ruff not found. Install the command named in @run, then: qc verify
#   → The gate does not skip a missing checker. NEXT: install it, then qc verify
```

<!-- Documents: STK-REQ-003, STK-REQ-006, SYS-REQ-004, SYS-REQ-006, SW-REQ-004, SW-REQ-006 -->

## Agent workflow

Point agents at [`SKILL.md`](./SKILL.md). Contract:

1. Set `QC_BY=agent`.
2. Run `qc plan` at task start; treat questions as requirements, not an end exam.
3. Mark at natural boundaries with falsifiable evidence.
4. Never mark `@human` items — surface them to the user. The CLI refuses `by:agent` on `@human`.
5. Never hand-edit `qc/state/` or `.qc/scratch.qcs`.
6. Loop: `qc plan` → work → `qc mark …` → `qc seal --staged && qc verify --staged`.
7. On failure, read the worklist top-to-bottom and fix; do not re-attest blindly.

<!-- Documents: STK-REQ-002, INT-REQ-003, SW-REQ-002 -->

## Bundled templates

| Template | Use when |
|---|---|
| `code-quality` | Application behavior changes |
| `api-change` | Public surface / compatibility |
| `security-review` | Auth, secrets, untrusted input |
| `data-migration` | Stored data or schema (includes `@strict` killers) |
| `dependency-update` | Add or bump a dependency |
| `release` | Release or cut |
| `docs` | Documentation changes |
| `ai-generated-code` | Agent-authored edits (memory APIs, weakened tests, suppressions) |

`qc add` copies the minimal variant unless `--full`. Adapt after copy; there is no runtime `extends` (legibility beats DRY).

## Build and test

```bash
make              # cosmocc → ./qc (APE)
make test         # kernel + shell acceptance + acceptance binary
make cover-host   # host gcc-15 MC/DC path (not the APE; see scripts/host-c-mcdc.sh)
make clean
```

Host coverage needs Homebrew `gcc-15` / `gcov-15` as wired in `scripts/host-c-mcdc.sh`. The APE product binary remains the cosmocc build.

<!-- Documents: SW-REQ-003, SYS-REQ-003 -->

## Spec and skill

- [`SPEC.md`](./SPEC.md) — specification v0.4 (purpose, invariants, digest, state, evidence, CLI, phasing).
- [`SKILL.md`](./SKILL.md) — short agent loop contract.

Phase 1 is what this binary implements. Features marked Phase 2 in the spec (template diff, `qc update`, `qc stats`, `qc diff` / `--carry`, merge-queue mode, …) are not shipped here.

## License

No `LICENSE` file is present in this repository. Treat the code as unlicensed until the maintainers add one — do not assume an SPDX identifier.
