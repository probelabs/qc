# Surface coverage scorecard

Generated: 2026-08-21T09:37:03Z

## Scope (partial coverage supported)

- partial: **true**
- groups (declared): `[cli digest gate runtime state]`
- group filter: `[runtime]`
- path_prefixes: `[]`

## Summary

| metric | value |
|--------|------|
| surfaces | 1 |
| OK | 1 |
| DEBT | 0 |
| GAP | 0 |
| dual-surface GAP | 0 |
| dual-surface residual-only | 0 |

**Green matrix ≠ zero bugs.** GAP=0 means every in-scope surface has an explicit disposition.

## Gate

```
surface_coverage: OK — 1 surface(s) (ok=1 debt=0 gap=0) scope_partial=true groups=[runtime]
```

## Rows

| status | surface | group | dual | disposition | reason / ref |
|--------|---------|-------|------|-------------|--------------|
| OK | `runtime.ape_no_fetch` | runtime | false | reviewed | Phase 1 is a Cosmopolitan APE (Makefile cosmocc, include/qc.h _COSMO_SOURCE). src/ has no HTTP client or registry fetch. STK-REQ-005/AC-001. / STK-REQ-005/AC-001 |

## How to close GAPs

Edit the surface's declaration under `proof/surfaces/<group>.yaml` and set a
disposition with a reason:

1. `reviewed` + a `reason` (and optional `ref` to an AC/test/requirement).
2. `accepted_risk` + a `reason` for honest, tracked open work (counts as DEBT).
3. `not_applicable` + a `reason` explaining why it does not apply.
4. Re-run: `proof surfaces check`
