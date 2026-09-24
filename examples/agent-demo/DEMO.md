# Agent demo transcript (real run)

<!-- Documents: STK-REQ-002, INT-REQ-003, SW-REQ-002 -->

Captured 2026-09-24 (Europe/Istanbul) against qc phase-1 APE from this repo.
Host PATH used `/Users/buger/go/src/qc/qc` (not the vendored `qc/qc` binary — that APE is gitignored here on purpose).

Committed `qc/state/seg-main-28cd64.qcs` matches the **current** tree (including the longer `README.md`). The fail-then-pass transcript below is from the original isolated run; after expanding README we re-decided/re-marked `docs/*` and re-sealed so `qc verify --ci` stays green in-tree.


This tree is the **post-seal** snapshot: `src/hello.py`, checklists, and `qc/state/seg-main-28cd64.qcs`. Replay verify:

```bash
export PATH="/path/to/qc-build:$PATH"   # directory containing the APE named qc
cd examples/agent-demo
qc verify --ci --quiet; echo EXIT:$?
# EXIT:0
```

## Setup (once)

```text
git init -b main
# seed src/hello.py + README.md, commit
qc init
qc add ai-generated-code
qc add docs
# agent edits src/hello.py (empty-name branch in greet)
export QC_BY=agent
```

## Commands and exit codes

| Step | Command (abridged) | Exit |
|---|---|---|
| Plan (open decisions) | `qc plan` | **1** |
| Decide AI checklist | `qc decide ai-generated-code --yes -m "agent edited src/hello.py…"` | **0** |
| Decide docs | `qc decide docs --yes -m "README.md describes…"` | **0** |
| Bad evidence | `qc mark ai-generated-code/apis-real --pass -m "lgtm"` | **2** (REJECTED) |
| Verify still open | `qc verify --quiet` | **1** |
| Honest marks | six `qc mark … --pass -m "…"` with paths/URLs | **0** each |
| Verify scratch | `qc verify --quiet` | **0** |
| Staged before seal | `qc verify --staged --quiet` | **1** |
| Seal | `qc seal --staged` → `seg-main-28cd64.qcs` | **0** |
| Staged after seal | `qc verify --staged --quiet` | **0** |
| CI after commit | `qc verify --ci --quiet` | **0** |

## Excerpt — rejection then repair

```text
$ qc mark ai-generated-code/apis-real --pass -m "lgtm"
REJECTED — evidence must point at something falsifiable (a path, a number, a command, a link), not restate that checking happened.
NEXT: qc mark <ref> --pass -m "…"
EXIT:2

$ qc verify --quiet
FORCED (never)     ai-generated-code/apis-real
…
EXIT:1

$ qc mark ai-generated-code/apis-real --pass -m "used only stdlib f-strings and def; verified against https://docs.python.org/3/tutorial/controlflow.html in src/hello.py"
Recorded pass → scratch · digest @6a3eb112ade9e562 · 1 files
EXIT:0
```

## Excerpt — seal required for staged/CI

```text
$ qc verify --quiet
all clear
EXIT:0

$ git add src/hello.py README.md qc/checklists qc/config.json .gitignore .gitattributes
$ qc verify --staged --quiet
FORCED (never)     ai-generated-code/@applies
…
EXIT:1

$ qc seal --staged
seal: promoted 8 of 8 → qc/state/seg-main-28cd64.qcs
NEXT: qc verify --staged
EXIT:0

$ git add qc/state
$ qc verify --staged --quiet; echo EXIT:$?
EXIT:0

$ git commit -m "demo: seal agent loop attestations"
$ qc verify --ci --quiet; echo EXIT:$?
EXIT:0
```

## Honest evidence used

- `ai-generated-code/apis-real` — URL + `src/hello.py`
- `ai-generated-code/no-extra` — `src/hello.py` / `README.md`
- `ai-generated-code/tests-honest` — no `tests/` path
- `ai-generated-code/no-silence` — no `noqa` / `type: ignore` in `src/hello.py`
- `docs/accuracy` — `README.md` vs `src/hello.py`
- `docs/stale` — new `README.md`

## Rebuild from scratch

```bash
rm -rf /tmp/qc-agent-demo && mkdir -p /tmp/qc-agent-demo && cd /tmp/qc-agent-demo
# copy src/hello.py + README from this example, or recreate
git init -b main && git add src README.md && git commit -m "seed"
export PATH="/path/to/qc:$PATH" QC_BY=agent
qc init && qc add ai-generated-code && qc add docs
# edit, decide, mark, seal, verify — follow docs/agentic-guide.md
```

Do **not** commit the vendored `qc/qc` APE into this example (large binary). Use an installed `qc` or PATH to a build.
