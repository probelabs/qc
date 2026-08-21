# qc — agent skill

Run `qc plan` at task start. Treat the questions as requirements, not an exam at the end.

- Set `QC_BY=agent`.
- Mark at natural boundaries with real evidence. Evidence must point at something falsifiable. The tool will not name the passing token.
- Never mark `@human` items — surface them to the user.
- Never edit `qc/state/` or `.qc/scratch.qcs` by hand. Only `qc mark` / `qc seal` write.
- Everything under `.qc/` is disposable. Everything under `qc/` is git's.
- On gate failure, read the worklist top-to-bottom and fix, rather than re-attest.
- New checklist: `qc template list`, then `qc add <closest>` and adapt.

Loop: `qc plan` → work → `qc mark <ref> --pass -m "…"` → `qc seal --staged && qc verify --staged`.

Exit: 0 all clear · 1 open items · 2 config/parse error.
