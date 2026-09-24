# examples/agent-demo

Tiny consumer-shaped tree that exercised the qc agent loop end-to-end.

<!-- Documents: STK-REQ-002, INT-REQ-003, SW-REQ-002 -->

| Path | Role |
|---|---|
| `src/hello.py` | App under `ai-generated-code` scope |
| `qc/checklists/` | `ai-generated-code` + `docs` (minimal templates) |
| `qc/state/seg-main-28cd64.qcs` | Sealed attestations from the recorded run |
| `DEMO.md` | **Actual** commands + exit codes (fail then pass) |

Full agent wiring guide: [`docs/agentic-guide.md`](../../docs/agentic-guide.md).

```bash
export PATH="/path/to/qc-build:$PATH"   # APE named qc
cd examples/agent-demo
qc verify --ci --quiet; echo EXIT:$?   # expect 0
python3 src/hello.py                   # hello, world
```

The vendored APE (`qc/qc`) is gitignored here — use an installed binary or PATH.
