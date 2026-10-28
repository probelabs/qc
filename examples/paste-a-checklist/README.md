# Paste a checklist into your agent

Try `qc` with Claude Code (or any coding agent) in two minutes. You paste a markdown checklist. The agent saves it to a file, runs real commands, marks each item with the output as evidence, and ends with a `qc report --md` table.

```bash
COPY=1 ./setup.sh     # fresh repo + the message on your clipboard
```

Run the `START` line it prints (Claude Code in the fresh repo with `qc` on PATH), then paste.

## What you paste

[`prompt.txt`](./prompt.txt), then [`checklist.md`](./checklist.md) under it. `setup.sh` writes both to `paste.txt`.

> Using the QC CLI, follow this checklist until it is done. If this checklist is only in the message, save it to a file first, then use that path. When qc verify is clear, show me the qc report --md output.

The checklist checks `same_reservation` in `seed/src/gate.py`: a retry within 24 hours returns the same reservation. The items:

| item | what the agent has to do |
|---|---|
| `promise` | quote the return line of `src/gate.py`. Parent of the next three, so it is marked last of them |
| `row-10`, `row-25` | run `python3 src/gate.py` and copy the matching output row |
| `boundary` | add a test for a retry at exactly 24 hours, run the tests, cite the count |
| `suite` | nothing: it is `@run`, so `qc` runs the tests itself |
| `env` | `python3 --version` |
| `bytes` | `git status --short` |

The prose at the top of the checklist tells the agent to go one item at a time and `sleep` between items, so people can follow along. `qc` only parses the `- [ ]` lines. Delete the prose for full speed.

## What you get

```
| item | status | by | age | digest | evidence | parent | asked for |
|---|---|---|---|---|---|---|---|
| reservation-release/promise | CLEAR | agent | … | 2bf544b4… | src/gate.py line 9: return is_retry and age_hours <= 24 | | the line number and text of the return statement in src/gate.py |
| ↳ reservation-release/row-10 | CLEAR | agent | … | b11d0e9c… | python3 src/gate.py printed: retry=True age=10 same=True expected=True | promise | the age=10 output row, copied verbatim |
| ↳ reservation-release/boundary | CLEAR | agent | … | 7df1f0c3… | Added test_retry_at_exactly_24_hours_is_same …; 'Ran 4 tests' then OK | promise | the new test name and the unittest summary line with its count |
| reservation-release/suite | CLEAR | qc | | | qc ran `python3 -m unittest discover -s tests -q`: exit 0 | | |
| …

7 of 7 items attested to the current bytes.
```

(From a real headless `claude -p` run, trimmed.)

Things worth trying while it runs:

- `qc mark … -m "tests pass"` exits 2. Evidence must name something falsifiable.
- Marking `promise` before its children exits 2, and the error lists the open children.
- Edit `src/gate.py` after the marks. `qc verify` exits 1 and the items scoped to it are open again.

## Where the checklist can live

- **Committed:** `qc/checklists/<id>.md`. Plain `qc plan` picks it up.
- **Pasted in chat:** the agent must save it first. `qc help` tells it to, because `qc` cannot read the model's memory.
- **One-off file:** `$TMPDIR/<id>.md` with `qc plan --checklist <path>` (or `QC_CHECKLIST=<path>`) on every command. The filename stem must equal the frontmatter `id`.

State always lands in the repo's `.qc/` and `qc/state/`. The checklist file is only the definition.
