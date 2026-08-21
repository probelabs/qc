# Surface coverage scorecard

Generated: 2026-08-21T09:36:51Z

## Scope (partial coverage supported)

- partial: **false**
- groups (declared): `[cli digest gate runtime state]`
- group filter: `[]`
- path_prefixes: `[]`

## Summary

| metric | value |
|--------|------|
| surfaces | 12 |
| OK | 12 |
| DEBT | 0 |
| GAP | 0 |
| dual-surface GAP | 0 |
| dual-surface residual-only | 0 |

**Green matrix ≠ zero bugs.** GAP=0 means every in-scope surface has an explicit disposition.

## Gate

```
surface_coverage: OK — 12 surface(s) (ok=12 debt=0 gap=0) scope_partial=false groups=[]
```

## Rows

| status | surface | group | dual | disposition | reason / ref |
|--------|---------|-------|------|-------------|--------------|
| OK | `cli.prompt_next` | cli | true | reviewed | AC-ENFORCE: help/errors print NEXT: qc … and do not print a string that would satisfy qc_evidence_ok. / STK-REQ-002/AC-002 |
| OK | `digest.content_bind` | digest | true | reviewed | AC-DUAL-EQ: qc_digest_core produces the 16-hex value stored/printed on the line; qc_winner_match requires equality. / STK-REQ-001/AC-003 |
| OK | `gate.deny_evidence` | gate | true | reviewed | AC-ENFORCE: qc_evidence_ok rejects deny_builtin tokens such as lgtm/done before any scratch write. / STK-REQ-003/AC-003 |
| OK | `gate.evidence_no_secret` | gate | true | reviewed | AC-ENFORCE: secret_hit rejects ghp_/BEGIN PRIVATE KEY; die message does not print the token. / STK-REQ-003/AC-004 |
| OK | `gate.human_refuse` | gate | true | reviewed | AC-ENFORCE: write_attestation dies before scratch write when item.human and by==agent. / STK-REQ-003/AC-002 |
| OK | `gate.run_missing_fail` | gate | true | reviewed | AC-EMPTY fail-closed: qc_run_cmd sets run_rc=127 and an install hint; print_eval does not skip. / STK-REQ-006/AC-002 |
| OK | `gate.verdict` | gate | true | reviewed | AC-DUAL-EQ: plan forced set ≡ verify exit. cmd_verify and qc_eval_all share the same eval; exit 0 iff no open items. / STK-REQ-001/AC-002 |
| OK | `runtime.ape_no_fetch` | runtime | false | reviewed | Phase 1 is a Cosmopolitan APE (Makefile cosmocc, include/qc.h _COSMO_SOURCE). src/ has no HTTP client or registry fetch. STK-REQ-005/AC-001. / STK-REQ-005/AC-001 |
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
