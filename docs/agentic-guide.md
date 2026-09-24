# Agentic guide — using qc with Claude Code, Cursor, and similar agents

<!-- Documents: STK-REQ-002, INT-REQ-003, SW-REQ-002, SYS-REQ-002 -->

Agents are non-deterministic. `qc` is not. This guide shows how to wire a coding agent so every change is gated by content-addressed, evidenced checklist answers before CI (or a human) sees a green build.

Short skill card agents can load: [`SKILL.md`](../SKILL.md).  
Worked demo + real transcript: [`examples/agent-demo/`](../examples/agent-demo/) ([`DEMO.md`](../examples/agent-demo/DEMO.md)).

## Why

| Without qc | With qc |
|---|---|
| Agent says “tests pass” in prose | Agent must `qc mark` with a falsifiable path / digit / URL / ``backtick`` |
| CI trusts the PR description | CI runs `qc verify --ci` on committed state only |
| Re-attesting hides drift | Digest binds answers to file bytes; edits re-force items |

The gate does not judge taste. It judges whether a decision was recorded — by whom, when, against what bytes.

## One-time setup (consumer repo)

### 1. Install qc

```bash
curl -fsSL https://raw.githubusercontent.com/probelabs/qc/main/scripts/install.sh | sh
# → ~/.local/bin/qc   (ensure that dir is on PATH)
qc help
```

Or point `PATH` at a local build (`/path/to/qc-repo` containing the APE named `qc`).

### 2. Init in the consumer repo

```bash
cd your-app
git init   # if needed
qc init
# Created qc/ (tool + checklists, committed) and .qc/ (local workspace, ignored)
```

`qc/` is git’s review surface. `.qc/` is disposable scratch.

### 3. Add templates

```bash
qc template list
qc add ai-generated-code   # agent-authored edits
qc add code-quality        # optional; may include @run / @human items
qc add docs                # when README/docs change
```

Adapt checklists after copy (no runtime `extends`). Prefer `ai-generated-code` + `docs` for a first agent loop; add `code-quality` when you have the `@run` tools installed and a human for `@human` items.

## How to wire the agent

Set provenance for every mark/decide:

```bash
export QC_BY=agent
# or pass --by agent on each command
```

### Claude Code — `CLAUDE.md` or project skill

Paste this into `CLAUDE.md` (or a `.claude/` instruction / skill that loads [`SKILL.md`](../SKILL.md)):

```markdown
## qc gate (required)

Load and follow the repo's SKILL.md (qc agent skill) for every coding task.

1. `export QC_BY=agent`
2. Run `qc plan` before editing; treat questions as requirements.
3. After edits: `qc decide …` if needed, then `qc mark <ref> --pass|-fail|--na -m "…"` with falsifiable evidence.
4. Never mark `@human` items — surface them to the user.
5. Never hand-edit `qc/state/` or `.qc/scratch.qcs`.
6. Before finishing: `git add` the ship set, then `qc seal --staged && qc verify --staged` (exit 0).
7. On failure: read the worklist top-to-bottom and fix; do not invent passing tokens.
```

Point a Claude Code project skill at `SKILL.md` when the consumer vendors or submodules qc, or paste the skill card verbatim.

### Cursor — rule or `AGENTS.md`

Add a project rule (e.g. `.cursor/rules/qc.mdc`) or an `AGENTS.md` section:

```markdown
# qc

- Always `export QC_BY=agent` in the shell used for qc.
- Start tasks with `qc plan`. End with `qc seal --staged && qc verify --staged`.
- Evidence must include a real path, digit, URL, or `backticked` span. Applause (`lgtm`, `done`, `ok`) is rejected.
- Do not mark `@human` items; ask the user to run `qc mark … --by <their-name>`.
- Do not edit `qc/state/` or `.qc/` by hand.
- Skill contract: see SKILL.md in the qc repo (or paste it here).
```

### Generic agents

Paste [`SKILL.md`](../SKILL.md) into the system / project instructions. Require `QC_BY=agent`. Treat exit codes as the machine API: `0` clear · `1` open · `2` config/rejection.

## Exact loop

Realistic flow for an agent editing application code (and docs). Commands and evidence shapes match the live demo in [`examples/agent-demo/DEMO.md`](../examples/agent-demo/DEMO.md).

```bash
export QC_BY=agent
export PATH="$HOME/.local/bin:$PATH"   # or path to the APE

qc plan
# → lists FORCED / AWAITING-DECISION items; exit 1 while open

# If a checklist has applies_when:
qc decide ai-generated-code --yes -m "agent edited src/hello.py to add empty-name handling in greet()"
qc decide docs --yes -m "README.md describes this throwaway demo for the agent loop"

# Work… then mark with falsifiable evidence (path / digit / URL / backticks):
qc mark ai-generated-code/apis-real --pass -m "used only stdlib f-strings and def; verified against https://docs.python.org/3/tutorial/controlflow.html in src/hello.py"
qc mark ai-generated-code/no-extra --pass -m "diff touches only src/hello.py; README.md unchanged this step"
qc mark ai-generated-code/tests-honest --pass -m "no tests/ directory exists; nothing deleted or weakened in this change to src/hello.py"
qc mark ai-generated-code/no-silence --pass -m "no noqa, type: ignore, or suppressions added in src/hello.py"
qc mark docs/accuracy --pass -m "README.md claims this is a throwaway demo; matches src/hello.py greet/main present"
qc mark docs/stale --pass -m "README.md is new in this demo; no prior install steps to remove"

qc verify                 # scratch + committed — agent loop; expect exit 0 when clear

git add src/ README.md qc/checklists/ qc/config.json   # ship set (not .qc/)
qc seal --staged          # promote scratch → qc/state/seg-*.qcs
git add qc/state/
qc verify --staged        # predicts CI; expect exit 0
```

### Deliberate failure (bad evidence)

```bash
qc mark ai-generated-code/apis-real --pass -m "lgtm"
# REJECTED — evidence must point at something falsifiable …
# EXIT: 2

qc verify
# FORCED items still open → EXIT: 1
```

Fix by re-marking with real evidence, then seal again.

### Skipping seal

`qc verify` (loop) can be green from scratch while `qc verify --staged` / `--ci` stay red until seal. Forgetting seal does not corrupt state; it leaves items forced where CI looks.

## What agents must NOT do

| Forbidden | Why |
|---|---|
| Mark `@human` items with `QC_BY=agent` | CLI refuses; human approval is provenance |
| Hand-edit `qc/state/*.qcs` or `.qc/scratch.qcs` | Only `qc mark` / `decide` / `baseline` / `seal` / `compact` write |
| Invent applause tokens (`lgtm`, `done`, `ok`) | Rejected at mark time (I5) |
| Skip `qc seal --staged` before claiming CI-ready | Unsealed scratch is invisible to `--staged` / `--ci` |
| Re-attest blindly after a red verify | Read the worklist top-to-bottom and fix the underlying gap |

## CI sketch (GitHub Actions)

```yaml
# .github/workflows/qc.yml
name: qc
on:
  pull_request:
  push:
    branches: [main]

jobs:
  verify:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install qc
        run: curl -fsSL https://raw.githubusercontent.com/probelabs/qc/main/scripts/install.sh | sh
      - name: Gate of record
        run: |
          export PATH="$HOME/.local/bin:$PATH"
          qc verify --ci
```

Use the vendored `./qc/qc` instead of the installer when the consumer commits the APE from `qc init`.

## Failure recovery

1. Run `qc plan` (or `qc verify` without `--quiet`).
2. Read **top to bottom** — first FORCED / FAILED / AWAITING-DECISION wins attention.
3. Decide if still awaiting; mark with better evidence; install `@run` tools if exit 127.
4. Surface `@human` items to a person (`--by <name>`).
5. `qc seal --staged && qc verify --staged` only when the index matches what you intend to ship.
6. `qc reset --item <ref>` wipes one scratch entry if you need a clean re-mark (committed state untouched).

## Copy-paste snippets

### CLAUDE.md block

See [Claude Code](#claude-code--claudemd-or-project-skill) above.

### Cursor / AGENTS.md block

See [Cursor](#cursor--rule-or-agentsmd) above.

### GH Actions step

See [CI sketch](#ci-sketch-github-actions) above.

### Skill contract (minimal)

```text
QC_BY=agent
qc plan → work → qc mark <ref> --pass -m "…" → qc seal --staged && qc verify --staged
Exit: 0 all clear · 1 open · 2 config/rejection
Never: @human as agent, hand-edit state, applause evidence, skip seal
```

## Replay the demo

```bash
# from a clone of probelabs/qc, with qc on PATH:
cd examples/agent-demo
qc verify --ci    # sealed tree → exit 0

# or rebuild from scratch (see DEMO.md)
```

<!-- Documents: STK-REQ-003, SYS-REQ-004, INT-REQ-001 -->
