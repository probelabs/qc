---
id: code-quality
applies_when: The change modifies application behavior (not comments/docs only)
scope: [src/**]
expires: 30d
---

- [ ] lint      :: Is the linter clean? @run(ruff check .)
- [ ] types     :: Is the type checker clean? @run(mypy src/)
- [ ] api-shape :: Which public symbols changed, and which are breaking? @scope(src/**)
- [ ] deps      :: Which new dependencies were added, and who maintains them? @scope(pyproject.toml, uv.lock)
- [ ] naming    :: Which names would confuse a stranger reading this diff?
- [ ] diff-read :: What did you find reading the full diff, top to bottom? @human @scope(src/**)
