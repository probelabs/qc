# Surface coverage scorecard

Generated: 2026-08-21T09:37:02Z

## Scope (partial coverage supported)

- partial: **true**
- groups (declared): `[cli digest gate runtime state]`
- group filter: `[gate]`
- path_prefixes: `[]`

## Summary

| metric | value |
|--------|------|
| surfaces | 5 |
| OK | 5 |
| DEBT | 0 |
| GAP | 0 |
| dual-surface GAP | 0 |
| dual-surface residual-only | 0 |

**Green matrix ≠ zero bugs.** GAP=0 means every in-scope surface has an explicit disposition.

## Gate

```
surface_coverage: OK — 5 surface(s) (ok=5 debt=0 gap=0) scope_partial=true groups=[gate]
```

## Rows

| status | surface | group | dual | disposition | reason / ref |
|--------|---------|-------|------|-------------|--------------|
| OK | `gate.deny_evidence` | gate | true | reviewed | AC-ENFORCE: qc_evidence_ok rejects deny_builtin tokens such as lgtm/done before any scratch write. / STK-REQ-003/AC-003 |
| OK | `gate.evidence_no_secret` | gate | true | reviewed | AC-ENFORCE: secret_hit rejects ghp_/BEGIN PRIVATE KEY; die message does not print the token. / STK-REQ-003/AC-004 |
| OK | `gate.human_refuse` | gate | true | reviewed | AC-ENFORCE: write_attestation dies before scratch write when item.human and by==agent. / STK-REQ-003/AC-002 |
| OK | `gate.run_missing_fail` | gate | true | reviewed | AC-EMPTY fail-closed: qc_run_cmd sets run_rc=127 and an install hint; print_eval does not skip. / STK-REQ-006/AC-002 |
| OK | `gate.verdict` | gate | true | reviewed | AC-DUAL-EQ: plan forced set ≡ verify exit. cmd_verify and qc_eval_all share the same eval; exit 0 iff no open items. / STK-REQ-001/AC-002 |

## How to close GAPs

Edit the surface's declaration under `proof/surfaces/<group>.yaml` and set a
disposition with a reason:

1. `reviewed` + a `reason` (and optional `ref` to an AC/test/requirement).
2. `accepted_risk` + a `reason` for honest, tracked open work (counts as DEBT).
3. `not_applicable` + a `reason` explaining why it does not apply.
4. Re-run: `proof surfaces check`
