---
id: reservation-release
scope: [src/**]
---

# Release check: reservation retry

Promise: a retry within 24 hours returns the same reservation. Anything else creates a new one.

How to work this checklist (people are watching, so go slowly):

1. Do one item at a time, in the order `qc plan` gives, children before their parent.
2. Before each item, run `echo "== <item id>: <question> ==" && sleep 3` so the item is on screen before you work on it.
3. Run the command the item needs and show its output.
4. Mark the item with that output as evidence, then run `sleep 2` before the next item.
5. After the last mark, run `qc verify`, then `qc report --md`, and show the report.

- [ ] promise :: Does src/gate.py still encode the 24-hour promise? @evidence(the line number and text of the return statement in src/gate.py)
  - [ ] row-10 :: Run `python3 src/gate.py`. Is a retry at 10 hours the same reservation? @evidence(the age=10 output row, copied verbatim)
  - [ ] row-25 :: In the same output, is a retry at 25 hours a new reservation? @evidence(the age=25 output row, copied verbatim)
  - [ ] boundary :: Is a retry at exactly 24 hours the same reservation? Add a test for it to tests/test_gate.py and run the tests. @scope(src/**, tests/**) @evidence(the new test name and the unittest summary line with its count)
- [ ] suite :: Does the whole test suite pass? @scope(src/**, tests/**) @run(python3 -m unittest discover -s tests -q)
- [ ] env :: Which interpreter produced this evidence? @evidence(the `python3 --version` output)
- [ ] bytes :: Which files did this work change? @scope(src/**, tests/**) @evidence(the `git status --short` output, one entry per file)
