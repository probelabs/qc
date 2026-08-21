# Acceptance-criteria sweep (ac-sweep) — product playbook

**Audience:** agents and humans running ReqProof on any product.  
**Role:** `proof role show ac-sweep --format agent`  
**Twin of:** hazard-sweep (breadth + workers + stamp + gate) — different property.

This file is **EI-harness product text**: keep language short and operational.

---

## Problem

| We have | Gap |
|---------|-----|
| hazard-sweep + `hazard_review` | Worst-case obligations get stamped |
| MC/DC + obligation evidence | Package multi-conds and fail-open classes |
| `acceptance_criteria_witnessed` | **Forces acceptance tests only if STK ACs exist** |
| Thin STK ACs (often 1 slogan each) | Dual-surface bugs (enforce vs project identity) never enter the gate |

**MC/DC green ≠ acceptance done.** Acceptance criteria are stakeholder-visible outcomes. They drive **acceptance tests** (`// STK:AC-id:acceptance`), not unit MC/DC.

---

## Mission

**ac-sweep** walks **every in-scope STK requirement**, builds a small **surface map** from code, writes or improves **testable ACs**, stamps **`acceptance_review`**, and updates a **deterministic checklist**. The existing check **`acceptance_criteria_witnessed`** then forces real acceptance tests (or honest `witness_deferred` debt).

You are the **ORCHESTRATOR**. Workers do one STK each. Cap **≤5 concurrent** workers.

---

## When to run (stage order)

```text
1. Spec hierarchy exists (STK → SYS → SW/INT)
2. Code grounded enough to map surfaces (implements / read code)
3. hazard-sweep (or hazard_review stamps) preferred first — not required
4. >>> ac-sweep <<<
5. Agents write // STK:AC:acceptance tests (or witness_deferred)
6. proof audit --check acceptance_criteria_witnessed
```

Do **not** run ac-sweep as the first step on an empty spec tree.  
Do **not** wait until MC/DC is 100% — AC debt is independent.

---

## Stamp: `acceptance_review` (mirrors `hazard_review`)

Store on the **STK requirement YAML** (top-level; allowed via model extras today).

```yaml
acceptance_review:
  reviewed_at: "2026-08-05T12:00:00Z"    # RFC3339 UTC
  reviewed_by: "agent:ac-sweep"          # actor
  disposition: reviewed                  # reviewed | not_applicable | deferred
  templates_applied:                     # optional
    - AC-EMPTY
    - AC-DUAL-EQ
  notes: "Short why; surface map summary."
  checklist_version: 1                   # optional
```

| disposition | Meaning |
|-------------|---------|
| `reviewed` | AC set reviewed; testable ACs present or intentional empty with notes |
| `not_applicable` | STK has no testable acceptance surface (rare; needs notes) |
| `deferred` | Analysis started; not finished (still **not-done** on checklist until reviewed) |

**Absence of the block** = **not-done** (pending AC analysis). Same idea as missing `hazard_review` = pending hazard analysis.

**Target engine work (residual):** first-class model field + `proof req edit --acceptance-reviewed` + `acceptance_review_current` lint, symmetric to hazard. Until then: stamp YAML + checklist file; do not claim silent pass.

---

## Deterministic checklist

**Path:** `proof/ac-sweep/checklist.yaml` (project artifact).

Every STK-REQ id **must** appear. No silent omit.

```yaml
version: 1
updated_at: "2026-08-05T12:00:00Z"
updated_by: agent:ac-sweep
items:
  - id: STK-REQ-…
    status: done          # done | not_done | deferred
    acceptance_review: true   # block present with disposition reviewed
    ac_count: 2
    notes: "…"
```

**Done** = **AC analysis complete** (not “tests green”). Only when:

1. Checklist row `status: done`, and  
2. STK has `acceptance_review.disposition: reviewed`, and  
3. Every AC that should be tested has `verification_method: test`, and  
4. Dual-surface values for this STK have AC-DUAL-EQ (or explicit not_applicable notes).

**Not required for Done:** `// STK:AC:acceptance` witnesses or `witness_deferred`.  
Pure unwitnessed ACs after you **log** `acceptance_criteria_witnessed` are **allowed honest debt**. The force check owns that debt; ac-sweep does not block Done on it.

Orchestrator rebuilds the checklist from disk at start and end of the sweep.

---

## AC templates (use these)

Write **observable** text (fixture, actor, action, assert). Prefer `verification_method: test`.

| ID | When | Shape |
|----|------|--------|
| **AC-ENFORCE** | Authz/authn decision | Under secure config, unauthorized actor **cannot** succeed on named action |
| **AC-EMPTY** | Missing/unverifiable claim or input | Behavior is fail-closed **or** specified empty — same on all surfaces |
| **AC-NEG** | Hostile input | Forged/malformed input does not elevate privilege |
| **AC-DUAL-EQ** | Value used for enforce **and** projected (admin/API/audit/log) | Project(V) equals Enforce(V) (or documented projection) under same inputs |
| **AC-OWN** | Projected identity/secrets | Durable export remains correct after temporary auth objects are gone (own storage) |
| **AC-REG** | Representation of V can change | AC-DUAL-EQ still holds after internal refactors |

**Dual-surface rule:** if a security value has both **enforce** and **project** sites in code, **AC-DUAL-EQ is mandatory** (or explicit not_applicable with notes).

---

## Surface map (per STK)

Workers fill this (short):

```text
Value V:
  Produce:  <file:symbol>
  Enforce:  <file:symbol>
  Project:  <file:symbol> | none
  Persist:  <file:symbol> | none
```

Sources: STK story + `derived_reqs` SYS/SW/INT + production code under those components.

---

## Procedure (orchestrator)

### 0. Bootstrap

```bash
proof --help
test -f proof.yaml
proof role show ac-sweep --format agent
ls specs/stakeholder/requirements/*.req.yaml
```

If proof is missing, **STOP**.

### 1. DISCOVER checklist

List all STK ids → write/update `proof/ac-sweep/checklist.yaml` with `not_done` unless already stamped reviewed.

### 2. FAN-OUT workers (≤5 concurrent)

One worker per STK (batch if more than 5). Paste **Worker prompt** below.

### 3. FILTER (orchestrator)

| Gate | Action |
|------|--------|
| **A** | Drop ACs that restate SYS shalls with no observable assert |
| **B** | Ensure `derived_reqs` exist and satisfy the STK |
| **C** | Dual-surface values without AC-DUAL-EQ → reject worker report until fixed or not_applicable notes |
| **D** | Cap new ACs per STK (default ≤5); prefer quality |

### 4. STAMP

Workers (or orchestrator) set `acceptance_review` on STK YAML + update checklist row to `done`.

### 5. FORCE CHECK

```bash
proof audit --check acceptance_criteria_witnessed --fail-level warn
```

Expect: more ACs may increase **unwitnessed** count. That is **honesty**, not regression. Do **not** fake green with unit-test annotations.

### 6. REPORT

Print: checklist table; new ACs; dual-surface hits; witness audit summary; remaining debt.

### 7. VALIDATOR (required after each major phase)

After role landing, after stamps, after dogfood: run the **Validator prompt**. Fix gaps. Re-run the force check.

**Exit ritual (every phase):**  
`It's completed. Run the validator.`

---

## Exit condition

ac-sweep is **done** when:

1. Every STK is on the checklist as `done`, `deferred`, or documented `not_applicable`.  
2. Every `done` STK meets **Done** = analysis-complete (stamp reviewed + testable ACs + dual-surface handled).  
3. `proof audit --check acceptance_criteria_witnessed` has been run and logged (pass, warn debt, or error — all OK if not silent).  
4. Validator report is pass or residual list only.

**Not required for ac-sweep exit:** all acceptance tests green, or every AC already `witness_deferred`.  
Unwitnessed testable ACs after a logged force-check run are **honest debt**, not a failed sweep.

---

## Copy-paste prompts

### Worker prompt

```text
You are an ac-sweep WORKER for ONE stakeholder requirement: <STK-ID>.

Rules:
- Write every observable AC this STK needs. Do not invent slogan ACs. Do not
  pad. An empty set is allowed only if the STK truly has no testable surface —
  it is not a goal. There is no AC quota.
- verification_method: test for any AC that can be tested.
- Prefer proof req edit --add-acceptance-criterion "…" --verification-method test
- Link derived_reqs to existing SYS children of this STK when possible.
- If a security value is both enforced and projected in code, add AC-DUAL-EQ
  (or not_applicable with notes).
- Do NOT write fake // STK:AC:acceptance unit tests.
- Do NOT file KnownIssues unless the user asked.

Steps:
1. proof req show <STK-ID>
2. Read derived SYS/SW/INT and the production code they name.
3. Build surface map (Produce / Enforce / Project / Persist).
4. Improve or add ACs using templates AC-ENFORCE, AC-EMPTY, AC-NEG,
   AC-DUAL-EQ, AC-OWN as needed. Observable text only.
5. Stamp acceptance_review on the STK YAML (disposition: reviewed).
6. Update proof/ac-sweep/checklist.yaml row for this STK to done.
7. Return JSON:
   {stk_id, surface_map, acs_added: [], acs_edited: [], dual_surface: bool,
    stamp: true, residual_notes: ""}

Read only your STK tree + code. Do not rewrite other workers' STKs.
```

### Validator / skeptic prompt

```text
DEFAULT TO FINDING GAPS. You validate an ac-sweep package.

Check:
1. docs/ac-sweep-playbook.md (or proof/docs/…) exists and has mission, stage,
   stamp, checklist, templates, prompts, exit, concurrency.
2. proof role show ac-sweep --format agent loads; ≤5 workers; names
   acceptance_criteria_witnessed.
3. proof/ac-sweep/checklist.yaml lists EVERY STK-REQ; no silent omit.
4. Each status:done STK is analysis-complete (stamp reviewed + testable ACs +
   dual-surface); pure unwitnessed ACs OK if force check was logged.
5. New ACs are testable/observable, not pure SYS restate.
6. proof audit --check acceptance_criteria_witnessed was run; log exists;
   no claim of green via unit tests for acceptance.
7. Dual-surface gaps either have AC-DUAL-EQ or explicit notes.

Classify: pass | fail
On fail: list concrete file fixes only.
Write report to the path the orchestrator names.
```

### Orchestrator one-liner

```text
Wear ac-sweep. Cap 5 workers. Checklist every STK. Stamp acceptance_review.
Then: It's completed. Run the validator.
Then: proof audit --check acceptance_criteria_witnessed
```

---

## Help text drafts (product)

### `proof help ac-sweep` (concept)

```text
ac-sweep
========
Walk every stakeholder requirement. Write testable acceptance criteria.
Stamp acceptance_review. Update proof/ac-sweep/checklist.yaml.

Run:  proof role show ac-sweep --format agent
Force tests:  proof audit --check acceptance_criteria_witnessed

Missing stamp = pending AC analysis (like missing hazard_review).
Unit tests of child SW-REQs do not witness STK ACs.
Checklist Done = analysis complete, not witness green. Pure unwitnessed
ACs (no witness_deferred yet) are allowed honest debt after you log the force check.
```

### Error / finding language (for future `acceptance_review_current`)

```text
STK-REQ-… has no acceptance_review stamp — AC analysis still pending.
Run: proof role show ac-sweep --format agent
```

```text
STK-REQ-… acceptance_review is deferred — finish AC analysis or set disposition reviewed.
```

```text
Acceptance criterion AC-… is testable but has no // STK:AC:acceptance witness.
Write an acceptance test or set witness_deferred with reason.
```

---

## Force check (existing product)

```bash
proof audit --check acceptance_criteria_witnessed
proof help acceptance_criteria_witnessed
```

Annotation form:

```text
// STK-REQ-…:AC-001:acceptance
```

---

## Concurrency

**Max 5 workers in flight** (all stages). Same as hazard-sweep.

---

## Dogfood scope (this repo)

- All 4 STK-REQs on checklist.  
- Improve ACs (observable + dual-surface where justified).  
- At most one new dual-surface AC exemplar if surface map supports it.  
- Do not invent a full OIDC feature STK unless justified by existing derived tree.  
- Leave acceptance test debt honest under `acceptance_criteria_witnessed`.

---

## Residual engine work

| Item | Status |
|------|--------|
| First-class `AcceptanceReview` in pkg/model | Preferred later |
| `proof req edit --acceptance-reviewed` | Preferred later |
| `acceptance_review_current` lint | Preferred later |
| Project stamp + checklist + role | **Ship now** |

---

## Related

- `proof role show hazard-sweep --format agent`  
- `proof help acceptance_criteria_witnessed`  
- `proof help hazard-review` (stamp analogy)

## Related product plan

- `docs/surface-coverage-corpus-plan.md` (or product `proof/docs/…`) — whole-corpus / package surface matrix (beyond STK-only ac-sweep).
- `docs/surface-coverage.md` — shipped CLI and matrix semantics.
- `proof help surface-coverage` · `proof help ac-sweep`
