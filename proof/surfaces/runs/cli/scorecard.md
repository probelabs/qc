# Surface coverage scorecard

Generated: 2026-08-21T09:37:02Z

## Scope (partial coverage supported)

- partial: **true**
- groups (declared): `[cli digest gate runtime state]`
- group filter: `[cli]`
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
surface_coverage: OK — 1 surface(s) (ok=1 debt=0 gap=0) scope_partial=true groups=[cli]
```

## Rows

| status | surface | group | dual | disposition | reason / ref |
|--------|---------|-------|------|-------------|--------------|
| OK | `cli.prompt_next` | cli | false | reviewed | AC-ENFORCE: help/errors print NEXT: qc … and do not print a string that would satisfy qc_evidence_ok. / STK-REQ-002/AC-002 |

## How to close GAPs

Edit the surface's declaration under `proof/surfaces/<group>.yaml` and set a
disposition with a reason:

1. `reviewed` + a `reason` (and optional `ref` to an AC/test/requirement).
2. `accepted_risk` + a `reason` for honest, tracked open work (counts as DEBT).
3. `not_applicable` + a `reason` explaining why it does not apply.
4. Re-run: `proof surfaces check`
