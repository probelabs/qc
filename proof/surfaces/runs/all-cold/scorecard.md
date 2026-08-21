# Surface coverage scorecard

Generated: 2026-08-21T09:35:54Z

## Scope (partial coverage supported)

- partial: **false**
- groups (declared): `[cli digest gate runtime state]`
- group filter: `[]`
- path_prefixes: `[]`

## Summary

| metric | value |
|--------|------|
| surfaces | 12 |
| OK | 0 |
| DEBT | 0 |
| GAP | 12 |
| dual-surface GAP | 11 |
| dual-surface residual-only | 0 |

**Green matrix ≠ zero bugs.** GAP=0 means every in-scope surface has an explicit disposition.

## Rows

| status | surface | group | dual | disposition | reason / ref |
|--------|---------|-------|------|-------------|--------------|
| GAP | `cli.prompt_next` | cli | true | pending |  |
| GAP | `digest.content_bind` | digest | true | pending |  |
| GAP | `gate.deny_evidence` | gate | true | pending |  |
| GAP | `gate.evidence_no_secret` | gate | true | pending |  |
| GAP | `gate.human_refuse` | gate | true | pending |  |
| GAP | `gate.run_missing_fail` | gate | true | pending |  |
| GAP | `gate.verdict` | gate | true | pending |  |
| GAP | `runtime.ape_no_fetch` | runtime | false | pending |  |
| GAP | `state.ci_no_scratch` | state | true | pending |  |
| GAP | `state.scratch_write` | state | true | pending |  |
| GAP | `state.seal_digest_eq` | state | true | pending |  |
| GAP | `state.segment_new_file` | state | true | pending |  |

## How to close GAPs

Edit the surface's declaration under `proof/surfaces/<group>.yaml` and set a
disposition with a reason:

1. `reviewed` + a `reason` (and optional `ref` to an AC/test/requirement).
2. `accepted_risk` + a `reason` for honest, tracked open work (counts as DEBT).
3. `not_applicable` + a `reason` explaining why it does not apply.
4. Re-run: `proof surfaces check`
