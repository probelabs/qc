# qc

Deterministic checklist gate for AI-driven development. Agents are non-deterministic; a content-addressed attestation is not. `qc` records evidenced answers bound to file bytes and refuses to clear until every applicable item has one.

<!-- Documents: STK-REQ-001, SYS-REQ-001, SW-REQ-001 -->

## Install

`qc` is a Cosmopolitan [APE](https://justine.lol/ape.html) universal binary. There are no release downloads yet — build from source with [cosmocc](https://cosmo.zip/pub/cosmocc/cosmocc.zip).

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
qc decide docs --yes -m "README.md is documentation updated in this change"
qc plan
```

Answer with falsifiable evidence (a real path, digit, URL, or backtick span). Vague ticks are rejected; the tool names the next command, never a passing phrase:

```bash
qc mark docs/accuracy --pass -m "checked README.md install section against Makefile COSMOCC and ./qc help"
qc mark docs/stale --pass -m "no stale install steps removed; README.md is new in this repo"
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

## Command map

| Command | Purpose |
|---|---|
| `qc help [topic]` | Topic help; every topic ends with NEXT |
| `qc init` | Create `qc/` + `.qc/`, vendor APE, one-line `.gitignore` |
| `qc template list\|show <t>` | Bundled checklist gallery |
| `qc add <t> [--full]` | Copy template into `qc/checklists/` (minimal unless `--full`) |
| `qc plan [--json]` | Ordered worklist before work |
| `qc decide <id> --yes\|--no -m "…"` | Applicability for `applies_when` checklists |
| `qc mark <ref> --pass\|--fail\|--na -m "…"` | Evidenced answer → scratch |
| `qc baseline [-m "…"] [--item <ref>]` | Grandfather current content at adoption |
| `qc seal [--staged]` | Promote matching scratch into this branch segment |
| `qc reset [--item <ref>]` | Wipe scratch (committed state untouched) |
| `qc verify [--staged\|--ci] [--only] [--quiet] [--json]` | The gate |
| `qc compact` | Trunk-only: fold segments into base, drop orphans, commit |
| `qc report [--md]` | Claims table for reviewers / CI summaries |

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

## Evidence rules

Every `pass` / `fail` / `n_a` needs `-m` evidence. Shape check at mark time:

- Rejects empty applause (`done`, `ok`, `lgtm`, … and `deny_evidence`).
- Requires enough substance **and** at least one falsifiable token: an existing repo path, a digit, a URL, or a ``backticked`` span.
- Refuses common secret shapes (AWS keys, `ghp_`, `sk-`, PEM blocks, high-entropy blobs).

Rejection messages state the principle and name the next `qc mark` command. They do not reveal a phrase that would pass.

`@run(cmd)` items are executed by the gate (exit 0 clears). A missing `@run` binary is a loud **FAILED** with an install hint — never a skip.

<!-- Documents: STK-REQ-003, STK-REQ-006, SYS-REQ-004, SYS-REQ-006, SW-REQ-004, SW-REQ-006 -->

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
