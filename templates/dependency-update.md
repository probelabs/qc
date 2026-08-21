---
id: dependency-update
applies_when: The change adds or bumps a dependency
scope: [src/**]
expires: 30d
---

- [ ] why    :: Why this version, and what does the changelog say is dangerous?
- [ ] compat :: Which of our call sites break?
- [ ] lock   :: Does the lockfile change match the intended bump? @scope(uv.lock)
