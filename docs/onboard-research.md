# qc onboard research

Source: SPEC.md v0.4. No product code yet. This map is spec-derived.

## Stakeholders

| Persona | Need |
|---|---|
| AI coding agent | Deterministic gate + prompt-shaped CLI. Every string names the next command. |
| Human reviewer | Diffable, blameable answers next to the code. @human items they must answer. |
| CI operator | verify --ci is read-only, pinned APE, no registry fetch. |
| Repo maintainer | One Cosmopolitan binary. Compact on trunk. Updates only as PRs. |

## Components

| Component | Owns |
|---|---|
| cli | plan, mark, decide, help, errors. I5: every string is a prompt. |
| gate | verify / plan verdict. Pure function of worktree, checklists, state, clock. |
| state | scratch (.qc/), segments, base. Three-tier write path. |
| checklist | Markdown items, @run / @attest, scope, expires. |
| digest | Content-addressed (item text + scoped file hashes). |
| runtime | One Cosmopolitan APE. No Python. No run-time fetch. |

## Boundaries (future INT-REQs)

1. Agent inner loop (scratch) vs what CI sees (committed segments + base).
2. mark/decide/baseline write scratch only; seal writes segments; compact writes base.
3. @run (machine) vs @attest (judgment + evidence).
4. @human answers must not be by:agent.
5. Host OS vs APE: one binary, locale-blind digest.

## Not in this repo yet

No src/, no tests, no hooks. Traces wait until IMPLEMENT.
