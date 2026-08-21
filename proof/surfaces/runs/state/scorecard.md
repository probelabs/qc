# Surface coverage scorecard

Generated: 2026-08-21T09:37:02Z

## Scope (partial coverage supported)

- partial: **true**
- groups (declared): `[cli digest gate runtime state]`
- group filter: `[state]`
- path_prefixes: `[]`

## Summary

| metric | value |
|--------|------|
| surfaces | 4 |
| OK | 4 |
| DEBT | 0 |
| GAP | 0 |
| dual-surface GAP | 0 |
| dual-surface residual-only | 0 |

**Green matrix ≠ zero bugs.** GAP=0 means every in-scope surface has an explicit disposition.

## Gate

```
surface_coverage: OK — 4 surface(s) (ok=4 debt=0 gap=0) scope_partial=true groups=[state]
```

## Rows

| status | surface | group | dual | disposition | reason / ref |
|--------|---------|-------|------|-------------|--------------|
| OK | `state.ci_no_scratch` | state | true | reviewed | AC-DUAL-EQ: qc_load_store reads scratch only in MODE_LOOP; MODE_CI skips scratch and qc_write_manifest returns. / STK-REQ-004/AC-003 |
| OK | `state.scratch_write` | state | true | reviewed | AC-ENFORCE: write_attestation calls qc_write_scratch only; seal is the sole path to qc_append_segment. / STK-REQ-004/AC-002 |
| OK | `state.seal_digest_eq` | state | true | reviewed | AC-DUAL-EQ: cmd_seal promotes iff computed digest ≡ line digest; mismatch stays on scratch. / STK-REQ-007/AC-003 |
| OK | `state.segment_new_file` | state | true | reviewed | AC-ENFORCE: qc_append_segment writes qc/state/seg-*.qcs per branch; two seals are two files. / STK-REQ-007/AC-002 |

## How to close GAPs

Edit the surface's declaration under `proof/surfaces/<group>.yaml` and set a
disposition with a reason:

1. `reviewed` + a `reason` (and optional `ref` to an AC/test/requirement).
2. `accepted_risk` + a `reason` for honest, tracked open work (counts as DEBT).
3. `not_applicable` + a `reason` explaining why it does not apply.
4. Re-run: `proof surfaces check`
