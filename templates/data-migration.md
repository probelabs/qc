---
id: data-migration
applies_when: The change migrates stored data or schema
scope: [src/**]
expires: 7d
---

- [ ] reversible :: Can this migration be reversed without data loss? @strict
- [ ] backfill   :: How is existing data backfilled, and what is the failure mode?
- [ ] downtime   :: What is the downtime window and who approved it?
- [ ] rollback   :: How did you test the rollback? @strict
