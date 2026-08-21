# Surface coverage scorecard

Generated: 2026-08-21T09:34:20Z

## Scope (partial coverage supported)

- partial: **false**
- groups (declared): `[]`
- group filter: `[]`
- path_prefixes: `[]`

## Summary

| metric | value |
|--------|------|
| surfaces | 0 |
| OK | 0 |
| DEBT | 0 |
| GAP | 0 |
| dual-surface GAP | 0 |
| dual-surface residual-only | 0 |

**Green matrix ≠ zero bugs.** GAP=0 means every in-scope surface has an explicit disposition.

## Rows

| status | surface | group | dual | disposition | reason / ref |
|--------|---------|-------|------|-------------|--------------|

## How to close GAPs

Edit the surface's declaration under `proof/surfaces/<group>.yaml` and set a
disposition with a reason:

1. `reviewed` + a `reason` (and optional `ref` to an AC/test/requirement).
2. `accepted_risk` + a `reason` for honest, tracked open work (counts as DEBT).
3. `not_applicable` + a `reason` explaining why it does not apply.
4. Re-run: `proof surfaces check`
