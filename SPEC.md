# qc — a deterministic checklist gate for AI-driven development

<!-- Documents: STK-REQ-001, STK-REQ-002, STK-REQ-003, STK-REQ-004, STK-REQ-005, STK-REQ-006, STK-REQ-007, SYS-REQ-001, SYS-REQ-002, SYS-REQ-003, SYS-REQ-004, SYS-REQ-005, SYS-REQ-006, SYS-REQ-007, SW-REQ-001, SW-REQ-002, SW-REQ-003, SW-REQ-004, SW-REQ-005, SW-REQ-006, SW-REQ-007, INT-REQ-001, INT-REQ-002, INT-REQ-003, INT-REQ-004 -->

## Specification v0.4 — 2026-08-06

v0.4 splits the tool's footprint into a committed, visible qc/ (tool, checklists, state — the review surface) and a gitignored, dotted .qc/ (scratch, manifests, cache — the disposable local workspace), making ephemerality structural: one .gitignore line, rm -rf .qc/ always safe (D19). Also adds the state-self-scope guard (edge 15). v0.3 replaced JSONL state with the human-readable .qcs line grammar (D15) and added §15 Distribution and updates (D16–D18) plus invariants I11–I12. v0.2 replaced the single committed ledger with the three-tier store — scratch → per-branch segments → trunk-compacted base (D11–D14). The logical read model (content-addressed attestations) is unchanged across all versions; only physical placement and syntax have moved.

## 1. Purpose and thesis

LLM agents are non-deterministic. qc constrains them with a deterministic gate built on one honest premise:

The gate never judges whether work is good. It judges whether a decision about the work was recorded — by whom, when, against what content, and with what evidence.

Everything that can be checked by a machine is checked by a machine (@run items). Everything that requires judgment is phrased as a question and must be answered — with evidence — by an agent or a human (@attest items). The gate verifies that every applicable question has a fresh, evidenced answer bound to the current content of the files it concerns. That verification is a pure function of the working tree and the state store, so it is exactly as deterministic as sha256.

The system is simultaneously:

A quality mechanism — questions enter the agent's context before work starts, shaping the work.

An audit trail — every answer is a diffable, blameable line in git.

An automation backlog — every @attest item is a candidate to graduate into an @run item. A question the agent answers "pass" forty times in a row is a lint rule nobody has written yet.

## 2. Design invariants

These are the non-negotiables. Every feature in this spec must be checkable against them; any future feature that violates one is wrong.

I1 — The gate is dumb. verify performs no inference, no heuristics, no network. Its verdict is a deterministic function of (worktree bytes, checklist files, state store, clock). Two machines with the same inputs produce the same verdict.

I2 — Attestations are claims about content. The logical key of an attestation is (item_id, digest-of-content), regardless of where the line physically lives. Branches, rebases, and merges are not special cases; they are content changing or not changing.

I3 — Forcing = staleness. An item demands attention exactly when the content its last answer was bound to has changed, when it has never been answered, or when its answer has aged out. There is no second "forcing" mechanism.

I4 — Every recordable status requires evidence. pass, fail, and n_a all carry a mandatory evidence string that must survive shape validation. An unevidenced tick cannot exist.

I5 — Every string the tool prints is a prompt. Output is written to provoke thought in the next context window (agent or human), not to report status. Stale messages echo the previous answer and name the changed files. The tool never reveals what string would satisfy the gate.

I6 — Provenance, not authorization. Anyone with write access can forge a state line. qc provides audit (git blame, CI report tables); enforcement of human approval lives in branch protection and required CI checks, outside the PR's control. The spec never pretends otherwise.

I7 — Legibility beats DRY. A checklist file must be readable as the complete truth of what it enforces. No runtime inheritance, no extends, no conditional item visibility. Templates compose by copying.

I8 — Fail open on applicability, fail loud on execution. Missing git, shallow clones, detached HEAD → more items apply, never fewer. A missing @run binary → a loud blocking failure with an install hint, never a silent skip.

I9 — Zero runtime dependencies. One Python file, stdlib only, Python ≥ 3.9, vendored into the repo. Identical behavior on Linux, macOS, Windows.

I10 — Zero conflict surface on hot paths, by construction. Parallel branches never contend on a shared mutable file: writes go to a gitignored scratch, promotion appends to a branch-owned segment file, and only a single-writer trunk job rewrites shared state. Merge behavior must never depend on merge drivers, .gitattributes, or forge server settings.

I11 — The verdict kernel admits exactly one runtime dependency, and it must be pinnable, auditable as plain text, and locale-blind. Currently that is CPython ≥ 3.9. No registry fetch, no compiled blob, no shell-dialect dependence anywhere in the verdict path — and exactly one implementation, ever: two gates that can disagree is zero gates.

I12 — Updates to the gate apply only as reviewable commits. Registries and installers provide discovery and transport; git provides trust and application. Nothing in the repo's hooks or CI may fetch gate code at run time.

## 3. Vocabulary

Term Meaning Checklist A markdown file in qc/checklists/ with frontmatter and items. Item One question inside a checklist. Kind is run (has @run) or attest (does not). Item ref <checklist-id>/<item-id>, e.g. code-quality/api-shape. Globally unique. Scope Glob set naming the files an item's answer depends on. Digest SHA-256 over (item identity text + sorted per-file content hashes of its scope). §5. Attestation One state line: an evidenced answer to an item, bound to a digest. Scratch .qc/scratch.qcs — gitignored working attestations; the agent's inner loop; resettable. Segment qc/state/seg-*.qcs — committed, branch-owned append file created by qc seal. Base qc/state/base.qcs — committed snapshot produced by trunk-only qc compact. Seal Promoting matching scratch entries into this branch's segment at commit time. Forced The state of an item that requires a response before the gate passes. Dormant An item whose scope matches zero files. Silent. Baseline A special attestation status used to grandfather existing content at adoption. Template A checklist file that lives in a template source and gets copied into a repo.

## 4. File formats

### 4.1 Repository layout — the committed/disposable twin

qc/                        # COMMITTED — the visible, reviewable half   qc.py                    # the vendored gate (the whole tool)   bin/qc  bin/qc.cmd       # 2-line shims → python3 qc/qc.py "$@"   config.json              # policy knobs; see 4.5   checklists/*.md          # definitions   templates/*.md           # optional repo-local template source   state/     base.qcs               # compacted snapshot     seg-*.qcs              # per-branch append segments  .qc/                       # GITIGNORED — the disposable, local half   scratch.qcs              # working attestations (resettable)   manifests/               # per-digest file hash lists   cache.json               # @run result cache  .gitignore                 # one line: .qc/ .gitattributes             # qc/state/*.qcs merge=union  (convenience only; §4.4) 

The dot carries the semantics: dotted = disposable, visible = versioned. This buys three structural properties that the old single-directory layout could only promise by discipline:

Ephemerality by construction. .gitignore is one line, forever. Any future local artifact (the phase-2 mtime cache, run logs) lands under .qc/ and is ignored automatically — a structural guarantee replacing a "remember to update the ignore list" discipline, in the same spirit as I10's "by construction."

rm -rf .qc/ is always safe. Worst case: unsealed answers gone (which is what qc reset is), stale messages lose file names (spec already degrades gracefully), @run caches rebuild. Nothing precious can ever live there. Conversely, everything under qc/ belongs to git — one directory you may always delete, one you never hand-mangle.

The review surface is visible. Checklists and state lines are the project's quality record — the original thesis is that the checklist drives quality — and things meant to be read in PRs shouldn't hide in a dotfolder that file trees sort away and signal as "infrastructure, skip."

Precedent, and it's exact: Gradle commits visible gradle/ (the wrapper we keep citing) and ignores dotted .gradle/ (its cache); Terraform versions your *.tf and ignores .terraform/. The committed-visible / ignored-dotted twin is an established pattern, not an invention. init fails loudly if a foreign qc/ already exists in the repo root.

CI invokes python3 qc/qc.py verify --ci — no install step, version pinned per repo by construction.

### 4.2 Checklist grammar

file        := frontmatter item* frontmatter := "---" NL (kvline NL)* "---" NL kvline      := key ":" SP (scalar | "[" scalar ("," SP scalar)* "]") item        := "- [" (" "|"x") "] " item-id SP* "::" SP* question annotation* item-id     := [a-z0-9] [a-z0-9-]* annotation  := SP "@" key ( "(" value ")" )? 

Frontmatter keys: id, applies_when, scope (list), expires, requires (list of checklist ids), from (template provenance stamp). Values are single-line; no quoting, no nesting. This deliberately tiny YAML subset is defined by what the parser accepts — the parser is the format's authority, not the YAML spec.

Item annotations:

Annotation Meaning Notes @run(cmd) Machine-checked. Gate executes cmd via the platform shell; exit 0 = clear. Must be the last annotation; swallows to the final ) on the line. Run items ignore @scope/@expires unless @cache is present. @scope(g1, g2, !g3) Files this answer depends on. ! negates. Overrides the checklist-level scope default entirely (no merging). @expires(30d) Answer max age. Nd/Nh/Nw. Overrides checklist-level expires. @human Answer must carry by: human. Audit-trail semantics; see §16. @strict n_a is not an acceptable status. For killer items. Use ~3 times per repo, not 30. @cache(24h) Cache a slow @run result. Cache key = (ref, digest of @scope if any, command). Phase 2. @evidence(paths|command|link) Declares required evidence shape. Phase 2.

Parser rules that prevent silent damage:

An @word token whose key is unknown is a parse error (exit 2), never question text. @scoep(...) must fail loudly.

A literal @ inside the question (a@b.com) is fine — only SP @knownkey parses as an annotation.

The checkbox is display-only. [x] and [ ] parse identically; the state store is the sole source of truth. qc verify warns once per file containing [x] so drift doesn't accumulate.

Duplicate checklist ids across files, duplicate item ids within a file, and requires cycles are exit-2 config errors. id defaults from the filename; if both present they must match.

### 4.3 A complete example

--- id: code-quality applies_when: The change modifies application behavior (not comments/docs only) scope: [src/**] expires: 30d requires: [tests-exist] ---  - [ ] lint      :: Is the linter clean?                                  @run(ruff check .) - [ ] types     :: Is the type checker clean?                            @run(mypy src/) - [ ] api-shape :: Which public symbols changed, and which are breaking? @scope(src/api/**) @expires(7d) - [ ] deps      :: Which new dependencies were added, and who maintains them? @scope(pyproject.toml, uv.lock) - [ ] naming    :: Which names would confuse a stranger reading this diff? - [ ] diff-read :: What did you find reading the full diff, top to bottom? @human @scope(src/**) 

Note the challenge–response phrasing: items are questions whose honest answers are evidence. "Verified" is a visibly absurd reply to "which public symbols changed?"

### 4.4 The state store

#### 4.4.1 The .qcs line format

One record per line, human-readable, zero escaping. Identical grammar in scratch, segments, and base:

# qc-state v1 code-quality/api-shape = pass @8f3c1a92b7e04d16 by:agent at:2026-08-05T14:22:10Z files:4 :: routes.py adds GET /v2/orders/{id}/events; no existing route or schema field removed code-quality/diff-read = pass @1f0e88b2aa93c407 by:human at:2026-08-05T16:40:02Z :: read full diff; auth paths untouched; two TODOs noted in src/api/hooks.py tests-exist/@applies   = n_a  @77d0c1e4b2a9f831 by:agent at:2026-08-04T09:12:44Z :: docs-only change; no behavior modified 

line     := ref WS+ "=" WS+ status WS+ "@" hex16 field* WS+ "::" WS+ evidence-to-EOL ref      := checklist-id "/" (item-id | "@applies")        # charset [a-z0-9-/@] status   := "pass" | "fail" | "n_a" | "baseline" field    := WS+ key ":" value        # split on FIRST colon; value is whitespace-free and may contain colons (at:2026-…T14:22:10Z) comment  := "#" …                    # first line of every file: "# qc-state v1" 

Rules that make it unambiguous and safe:

Evidence swallows everything after the first standalone :: token, to end of line. Nothing before it can contain that token by construction (refs, statuses, hex, whitespace-free values), so the first-match split is exact — and evidence itself may freely contain ::, quotes, backticks, pipes, anything an honest technical answer contains. Zero escaping, ever.

Known fields: by, at, files. Unknown key:value tokens are preserved verbatim through seal and compact rewrites — the forward-compatibility JSONL gave us, kept.

Normalization at mark time keeps records single-line: evidence newlines collapse to ; , whitespace in by becomes -.

A malformed line warns on stderr and is treated as absent — the affected item simply goes forced. A friendly format invites hand-editing, so hand-editing must only ever fail safe: a typo re-asks a question; it never bricks verify with exit 2. (An unknown # qc-state version is exit 2 — that's config, not data.)

An unknown status word is a malformed line (absent), not a parse error.

Why this and not JSONL: v0.2 made the segment diff a review surface — the reviewer reads the author's claims as added lines in the PR diff, next to the code. You can't render a diff, so the committed lines' readability is a functional requirement, and JSONL buries evidence in \" noise. Why not markdown tables (the other "readable" candidate): cells must escape |, and evidence is encouraged to contain backticked commands (ruff check | grep E501) — the escaping lands exactly on the most technical, most valuable evidence. The rendered-table experience lives in qc report --md, where it belongs.

Why one-record-one-line is load-bearing: union-merge of a shared same-branch segment can only duplicate or reorder whole records, never interleave two into garbage; blame and diff operate at claim granularity (one line added = one claim by one author); and the store is unix-native — grep api-shape qc/state/*.qcs returns complete records, awk -F' :: ' '{print $2}' extracts evidence. When scratch or a segment lands in an agent's context window, it reads as dense natural language rather than spending a third of its tokens on JSON syntax — the state store is itself a prompt (I5).

A line is ~200 bytes; base is O(items) — a 50-item repo carries ≈ 10 KB forever.

#### 4.4.2 Tiers and the single write path

Single write path. mark, decide, and baseline write only to scratch. seal is the only writer of segments. compact is the only writer of base. Nothing else writes committed state; nothing writes it implicitly.

Tier Path Git Written by Purpose Scratch .qc/scratch.qcs ignored mark/decide/baseline The messy inner loop: attest, revise, thrash, abandon. qc reset wipes it — always safe; committed state is untouched. Segments qc/state/seg-*.qcs committed qc seal This branch's promoted answers. Append-only per branch. Base qc/state/base.qcs committed qc compact Compacted snapshot: ≤ 1 line per item, sorted by ref.

Seal. qc seal [--staged] computes each item's digest from the worktree (or the git index with --staged), promotes every scratch entry whose digest matches into this branch's segment, removes the promoted lines from scratch, and (with --staged) stages the segment file. Scratch entries that no longer match stay behind — they describe content that isn't shipping. Segment naming is deterministic, needing no local bookkeeping: seg-<sanitized-branch>-<crc6-of-raw-branch-name>.qcs (slashes → -; the crc6 disambiguates names that sanitize identically); detached HEAD → seg-detached-<sha8>; no git at all → seg-local-<host6>. Same branch ⇒ same file, on purpose (see collisions below).

Read model. A verify run unions its visible sources (which sources depends on mode — §6.3), then resolves per item: candidates are entries whose digest equals the item's current digest; the winner is the max by (at, sha256-of-line); on an identical timestamp only, status severity breaks the tie — fail > n_a > pass/baseline. Clocks skew; safety shouldn't. If no candidate matches, the item is forced, and the most recent entry for that item across all sources — matching or not — supplies the "You said then: …" echo (I5).

Compact. qc compact runs on trunk only. For every item ref present in base + segments: if an entry matches the item's current trunk digest, keep the winning one; otherwise keep the single most recent entry, whose only job is preserving the echo for whoever un-stales the item. Items no longer present in any checklist are dropped (orphan GC — this replaces v0.1's prune). Write base sorted by ref, delete all segment files, commit. Single-writer by policy: run it from a scheduled CI job or by hand; if a concurrent merge lands first, the push is rejected and the job simply reruns. verify prints a nudge when segment count exceeds config.compact_after (default 40).

Why the worktree cannot bloat. Base is O(items); between compactions the worktree adds one small file per merged branch. The multi-megabyte file is structurally impossible in the worktree. Git history still grows with attestation churn — the same class of growth as any committed metadata, collapsed per-PR by squash merges, and it is precisely what makes deletion safe: git history is the archive. Every compacted-away line remains recoverable via git log -p; compaction loses nothing forensically.

Why parallel PRs cannot conflict. Ten branches seal into ten differently-named files. Any merge between them is a pair of file additions, which git — including forge server-side merge buttons — auto-merges unconditionally. No merge driver, no .gitattributes, no forge setting is load-bearing (I10). The one residual collision: two people pushing to the same branch append to the same segment → an end-of-file both-add conflict during ordinary pull. init writes qc/state/*.qcs merge=union as a local convenience for exactly this; correctness never depends on it, and duplicates it may admit are handled by the read-model resolution above.

What replaced the merge conflict. When PR #1 merges and PR #2 rebases, PR #2 gets no state-file conflict — but content inside hot items' scopes changed, so verify reports FORCED (stale) with the previous answer echoed. The same underlying reality that produced conflict markers in a snapshot design now surfaces as a named, answerable question in the tool's own language — which is exactly the re-review signal wanted, instead of friction.

### 4.5 Config, manifests, cache — and the self-scope guard

qc/config.json (all keys optional):

{"trunk": "auto", "ignore": ["custom/extra/**"], "deny_evidence": ["approved"], "compact_after": 40} 

ignore extends the built-in default ignore set: qc/**, .qc/**, .git/**, **/node_modules/**, **/dist/**, **/build/**, **/__pycache__/**, .venv/**, .tox/**, .mypy_cache/**, .ruff_cache/**, .pytest_cache/**, **/generated/**, **/*.min.*, *.lock, package-lock.json, yarn.lock, pnpm-lock.yaml, poetry.lock, uv.lock, Cargo.lock.

Ignore-piercing rule: a default/config ignore does not remove a file if one of the item's positive scope patterns contains no wildcard and names it exactly. So @scope(uv.lock, pyproject.toml) sees the lockfile even though uv.lock is globally ignored — the deps item wants exactly what everyone else wants excluded. Piercing may also name checklist files exactly (@scope(qc/checklists/security-review.md)), enabling meta-attestation over policy changes (§16).

The self-scope guard: any scope pattern that resolves into qc/state/ — pierced or otherwise — is an exit-2 config error. An attestation whose scope included the state store would go stale the moment sealing wrote it: a self-invalidation loop, permanently forced. The guard makes the loop unconstructable (edge 15).

.qc/manifests/<digest>.json — the per-file (path, hash) list behind a digest. Written best-effort by mark and non---ci verify. Purely a UX asset: it lets stale messages name the changed files. Absent (fresh clone, CI) → messages degrade to "files under src/api/** changed"; the verdict is identical. Message quality is local; correctness is committed.

## 5. The digest — exact algorithm

normalize(bytes):     if b"\x00" in first 8 KiB: return bytes            # binary     return bytes.replace(b"\r\n", b"\n")               # CRLF → LF (I9: Windows parity)  resolve_scope(patterns):     walk worktree (or git index for --staged)     keep paths matching ≥1 positive pattern (globstar ** supported)     drop paths matching any !negation in the item's scope     drop paths matching ignore set, EXCEPT ignore-piercing rule (§4.5)     reject any pattern resolving into qc/state/ (self-scope guard, §4.5) → exit 2     paths are POSIX-style, repo-relative, bytewise-sorted  digest(item):     core  = item.checklist_id + "/" + item.id + "\n" + item.question_text + "\n"     pairs = [(p, sha256(normalize(read(p))).hex) for p in resolve_scope(item.scope)]     return sha256(core + Σ (p + "\x00" + h + "\n")).hexdigest()[:16] 

Consequences, all intended:

Item text is part of its own scope. Rewording a question orphans old answers — they were answering a different question. Changing only @expires or @scope values does not fold into core; a scope change alters the digest anyway (different file set), and an expiry change applies naturally to the existing answer's timestamp.

A scope-less attest item digests over its text alone: forced once, then re-forced only by expiry or rewording. It is never dormant.

Deletions and renames invalidate for free (the pair list changes).

The 16-hex prefix is both stored and displayed. Verification is exact-match against a freshly computed digest, so 64 bits is far beyond accidental-collision range; and since anyone with write access could always write a whole line (I6), truncation concedes nothing adversarially.

bytewise-sorted is load-bearing: locale-collated ordering (what shell sort does by default) would silently fork digests across machines — the exact bug class this tool exists to abolish (I11).

The glob matcher is a custom ~40-line globstar→regex translator (stdlib fnmatch lacks **). Its behavior is the documented behavior.

## 6. State model

### 6.1 Item states (computed fresh by every verify)

State Condition Gate CLEAR run item: exit 0 · attest item: resolved attestation is pass/n_a/baseline, digest matches, within expiry passes FORCED (never) no attestation exists for this item in any visible source blocks FORCED (stale) attestations exist for this item, none for the current digest blocks FORCED (expired) matching attestation older than the effective expires blocks FAILED run item exited non-zero (incl. binary missing → loud, with install hint) · attest item's resolved status is fail blocks DORMANT scope patterns match zero files silent BLOCKED a checklist in requires has blocking items shown, deprioritized; mark on it warns but is allowed

A recorded fail is a first-class, legitimate state: it blocks the gate and leaves reviewable reasoning, which beats the item quietly not existing.

### 6.2 Checklist applicability

If a checklist has applies_when, an implicit pseudo-item <id>/@applies exists, digested over core = id + applies_when text plus the checklist's default scope pairs, honoring the checklist's default expires. It uses the identical machinery as any item — no special mechanism (I3).

@applies forced → the worklist shows a decision request; the checklist's own items are held in AWAITING-DECISION.

qc decide <id> --no -m "reason" records n_a on the pseudo-item → the entire checklist is suppressed until that digest changes or the decision expires. The reason is a diffable state line anyone can contest in review (the ISO-27001 Statement-of-Applicability move: exclusions must be justified).

qc decide <id> --yes -m "reason" records pass → items proceed under their own scopes.

A checklist without applies_when needs no decision; its items simply force by scope. Needing applies_when at all is a mild smell — try to express it as paths first.

### 6.3 Which sources a verify reads

The three modes form a strict hierarchy, so "green locally" can be made to mean exactly "green in CI":

Mode Sources Question it answers verify (loop) scratch + worktree base/segments "Am I ready?" — unsealed answers count, so the agent iterates without committing. verify --staged staged base/segments only "Will this commit pass CI?" — scratch is deliberately excluded; the pre-commit hook runs seal --staged first, so anything real has been promoted. Predicts the CI verdict exactly. verify --ci committed base/segments only The gate of record. Read-only: no manifest writes, no cache writes, scratch ignored even if present.

Unsealed work is therefore invisible to CI, which makes sealing self-enforcing: forgetting it doesn't corrupt anything, it just leaves the items forced where it matters.

### 6.4 Baselines (adoption)

qc baseline -m "adopted at v2.3; existing code grandfathered" writes a status: baseline attestation to scratch for every currently non-dormant item (including @applies and @strict items — it is an explicit human command) at the current digest; the introducing commit then seals them like any other answer. From then on only genuine change forces. --item <ref> baselines one item, for checklists added later.

Baselines honor expiry. Grandfathering excuses old content, not periodic re-checks; a 30-day dependency audit fires 30 days after adoption, exactly as it should. Content-bound items with no expires stay grandfathered until their files actually change.

Self-enforcing adoption: a PR introducing a new checklist fails its own verify --ci (everything FORCED (never)) until baselined-and-sealed in that same PR. Unbaselined checklists cannot reach trunk.

## 7. Evidence rules

Origin differs by kind: @run evidence is captured (command, exit code, last 10 output lines, duration — stored in the run report, not the state store); attest evidence is authored.

Authored evidence must pass shape validation at mark time:

Denylist — reject if the entire trimmed, lowercased string ∈ {done, ok, okay, yes, verified, checked, looks good, lgtm, fine, n/a, complete, passed} ∪ config.deny_evidence.

Substance — ≥ 12 characters AND contains ≥ 1 falsifiable token: a path that exists in the repo, a digit, a URL, or a backticked span.

Declared shape (phase 2) — @evidence(paths) requires ≥1 existing path; (command) requires a backticked span; (link) requires a URL.

Validation stops at shape, deliberately. Judging content teaches agents to produce the minimum passing string — that trains gaming, not thinking. Rejection messages state the principle ("evidence must point at something falsifiable"), never the specific token that would pass (I5).

Evidence is normalized to a single line at mark time (§4.4.1) and may contain any characters — the .qcs format never requires escaping, so backticked commands, pipes, and quotes pass through verbatim.

Secret scan: mark refuses evidence matching common credential shapes (AKIA[0-9A-Z]{16}, ghp_…, sk-…, -----BEGIN … PRIVATE KEY, ≥ 32-char high-entropy blobs). Evidence eventually becomes committed text; a pasted token is forever.

Statuses: --pass, --fail, --na — all with mandatory -m. @strict refuses --na. by is taken from --by, else $QC_BY, else the OS user. @human items refuse by=agent — a convention with teeth only via blame + CI (§16), and the spec says so.

## 8. CLI reference

Command Purpose Phase qc init Create the committed qc/ and ignored .qc/ trees, vendor qc.py, shims, config, the one-line .gitignore entry and the convenience .gitattributes line. Fails loudly if a foreign qc/ exists. 1 qc add <template> [--full] [--set k=v] Copy a template in, stamp from:, substitute {{vars}} at copy time. Minimal variant by default. 1 qc template list | show <t> The menu; primary agent context for checklist generation. 1 qc template diff Local edits since from:@N + upstream @N→@M changes. Prints both; never auto-merges. 2 qc plan [--json] The ordered worklist: forced items, questions, changed files, previous answers, decision requests. 1 qc decide <checklist> --yes|--no -m "…" Record applicability (→ scratch). 1 qc mark <ref> --pass|--fail|--na -m "…" [--by X] Record an answer (→ scratch). Digests the worktree; warns if worktree ≠ index for in-scope files. 1 qc baseline [-m "…"] [--item ref] Adoption ratchet (→ scratch). 1 qc seal [--staged] Promote matching scratch entries into this branch's segment; with --staged, match against the index and stage the segment. 1 qc reset [--item ref] Wipe scratch (or one entry). Always safe: committed state untouched. 1 qc verify [--staged] [--ci] [--only ref] [--quiet] [--json] The gate. Source hierarchy per §6.3. 1 qc compact Trunk-only: fold segments into base, ≤1 line per item, orphan GC, delete segments, commit. 1 qc report [--md] Claims table: item, status, by, age, evidence, digest. --md for $GITHUB_STEP_SUMMARY / PR comment. 1 qc update [--check] Compare vendored version against the release manifest; without --check, fetch the pinned release, sha256-verify, rewrite qc/qc.py, and print the diff. Never commits (I12). 2 qc diff <ref> Real diff of an item's scope since its last answer (uses the HEAD sha recorded at mark time when the tree was clean). 2 qc mark <ref> --carry Re-attest with prior evidence + "carried" note. Only offered when qc diff can show the diff — carry without a visible diff is a rubber stamp. 2 qc stats Force/pass streaks, stale frequency per item → "candidate for @run", "scope too broad/narrow". 2

Exit codes: 0 all clear · 1 open items (forced / failed / awaiting-decision) · 2 config or parse error. This triple is the entire machine API.

Worklist ordering: requires topology first, then checklist file order, then item order. All open items batch into one list per run — never four separate failures.

## 9. Local workflow — the agentic loop

The design mistake to avoid: treating the gate as an exam at the end. The right shape is a brief at the start:

task start      → qc plan                       # questions enter the agent's context BEFORE work   … agent works with the questions in mind … edit boundary   → qc mark <ref> --pass -m "…"   # answers land in scratch as areas complete abandon a path  → qc reset                      # scratch is disposable by design pre-commit hook → qc seal --staged --quiet && qc verify --staged --quiet stop hook       → qc verify --quiet             # loop mode; final tripwire only 

An agent that saw "which public symbols changed, and which are breaking?" before touching src/api/ produces a real answer. One hit by a stop hook after the fact produces an escape attempt. Accordingly, hooks fire at boundaries (commit, push, task end) — never mid-task, which trains reflexive answering.

SKILL.md (~80 lines; see §15.1 for why it stays that small) teaches the agent: run qc plan at task start and treat the questions as requirements; set QC_BY=agent; mark at natural boundaries with real evidence; never touch qc/state/ or .qc/scratch.qcs by hand — only mark/seal write; everything under .qc/ is disposable, everything under qc/ is git's; never mark @human items — surface them to the user instead; on gate failure, read the worklist top-to-bottom and fix rather than re-attest; when creating a checklist, start from qc template list and adapt the closest match. For Claude Code, a stop-hook wrapper maps qc's exit 1 → hook exit 2 so the worklist text feeds back into the agent.

Sample verify output — every string is a prompt (I5):

FORCED (stale)  code-quality/api-shape   Q: Which public symbols changed, and which are breaking?   Changed since your last answer (2h ago): src/api/routes.py, src/api/schema.py   You said then: "added optional `cursor` param only — additive"   → That answer may no longer hold. Re-read the diff before responding. 

Echoing the previous answer is the highest-leverage line: it reframes the task from "fill a blank" to "defend or revise a claim you made."

## 10. CI/CD — two gates, a compactor, and an enforcement reality

The gotcha that burns everyone first: GitHub's pull_request event checks out a synthetic merge commit, not your branch tip. Base moved since attestation → merged content ≠ attested content → red in CI, green locally → rage-uninstall. Except the disagreement is meaningful — it fires exactly when base changed files inside an item's scope, i.e. a semantic conflict worth re-reviewing. So both behaviors are right, at different gates:

Gate Checks out Verifies Answers PR check (required status) github.event.pull_request.head.sha what the author attested "did the author answer for their content?" — predictable, zero phantom failures Merge queue (strict, phase 2) the merged result the combination "does it still hold after concurrent changes?" — upstream churn nags exactly once, at merge

The merge-queue gate fully replaces the earlier @sticky idea for rebase-heavy branches.

- uses: actions/checkout@v4   with: { ref: ${{ github.event.pull_request.head.sha }} } - run: python3 qc/qc.py verify --ci - if: always()   run: python3 qc/qc.py report --md >> "$GITHUB_STEP_SUMMARY" 

Compaction job — scheduled (weekly cron and/or workflow_dispatch), on trunk: python3 qc/qc.py compact, then commit and push (directly or via an auto-PR if trunk is protected). If a merge races the push, the push is rejected and the next run succeeds; single-writer is a policy of where it runs, not a lock.

CI mode is strictly read-only. The report --md table (also posted as a sticky PR comment) is where mandatory evidence compounds: the reviewer's entry point becomes the author's claims, reviewed like code — and since v0.2, the PR's own diff shows them too, as added segment lines, now readable at a glance in the .qcs format. "Additive only" sitting beside a diff that deletes a field is a five-second catch. Do not require answers in the PR description — hand-maintained text rots; generate the table.

Nested/complex flows: requires chains order the CI verdict identically to local (code-quality → tests-exist → …), and one repo can gate different areas with different checklists purely via scopes — the "complex flows" of the original idea reduce to composition of these primitives, which is the point.

## 11. Templates

Copy-paste, permanently — no extends, ever. Rationale (I7): these files are audited in PRs; a checklist resolving three presets can't be read as truth, while a copied-and-edited one shows its edits in the diff. Helm vs. cookiecutter; cookiecutter is right here. cruft's existence proves the failure mode of copying (drift) and its fix (provenance stamp + template diff), which we adopt wholesale.

A template is a checklist file — same grammar, different directory; promoting a good checklist to a template is cp. Resolution order: bundled → qc/templates/ → git URL (org-shared). --set table=orders substitutes {{table}} at copy time, writing literal strings — nothing resolves at runtime; the file on disk stays the truth.

The gallery is the agent's prior: qc template list output is what lands in context, turning checklist generation from free authorship into "copy the closest, adapt" — constraining the non-determinism of the checklist-generation step itself.

Bundled set: code-quality · api-change · security-review · data-migration (killer items: reversibility @strict, backfill, downtime, rollback tested) · dependency-update · release · docs · ai-generated-code. The last is the differentiator, targeting agentic failure modes: APIs verified against real docs (not memory), no unrelated files touched, no tests weakened or deleted to pass, no suppressions (# type: ignore, eslint-disable) added to silence checkers.

qc add copies the minimal variant unless --full: mandatory evidence makes every item expensive (by design — it's the anti-rubber-stamp pressure), so the path of least resistance must be a short checklist you'd actually defend.

## 12. Git interactions

The logical model is content-addressed (I2); the physical store travels with the branch. Together:

Operation State files Verdict behavior Why it's right checkout / switch that branch's base + segments come along its own attestations apply store is versioned with the code it attests new branch off green base inherits base's state unchanged everything carries identical content, identical digests rebase onto moved trunk segment files are both-add → no conflict items whose scope absorbed upstream changes go FORCED (stale) with the previous answer echoed the friction converts into a named re-review question squash / fast-forward merge segment file content survives squash carries identical content merge with resolved conflicts no state-file conflict conflicted source files = new digests → stale a hand-resolved conflict is precisely when prior review stops being valid ten parallel PRs ten distinct segment files, all additions zero contention, including forge merge buttons I10 same branch, two pushers shared segment → EOF both-add on pull merge=union convenience or trivial manual resolve; read model dedupes correctness never depends on the driver revert to an attested state pre-compaction: old entry still present → resurrects · post-compaction: gone → re-answer acceptable; compaction traded resurrection for boundedness history still holds the original for audit stash / worktree / detached HEAD only content + visible files consulted; each git worktree has its own .qc/ scratch works; detached HEAD gets its own segment name git state is irrelevant to the verdict

Accepted loss (since v0.2): two branches with identical content but no shared state files no longer share attestations — the snapshot travels with ancestry, not with content alone. Rare, and the cost is one re-attestation.

Git is optional in the core. verify and digesting are pure filesystem. Git is required only for: --staged (reads index blobs), branch-derived segment naming (with a non-git fallback), qc diff/--carry, and blame-based audit. No merge-base, no diff-vs-trunk anywhere in the verdict path — "since last answer" replaced "since branch point" as the baseline, deliberately: a diff-window baseline lets three-commit-old unreviewed changes silently age out of the window; a last-answer baseline cannot lose work — debt persists until answered, across commits, branches, weeks.

## 13. Edge-case ledger

The unifying diagnosis: an attestation is a claim about content, and there are three "contents" in play — working tree, index/commit, CI checkout. Most edge cases are gaps between them.

# Case Resolution Phase 1 Attest, then git add -p / fix a typo, commit seal --staged promotes nothing (digest mismatch); verify --staged forces, naming the file 1 2 Formatter sweep invalidates 40 attestations qc diff shows the real diff → --carry with "carried: formatting only". Carry is gated on the diff being showable 2 3 Lockfiles/generated churn default ignores + ! negations + ignore-piercing for exact names 1 4 CRLF: Windows dev, Linux CI normalize before hashing; NUL-sniff for binary. Non-negotiable 1 5 New checklist lands in trunk, every open PR forced impossible: the introducing PR fails its own gate until baselined and sealed (§6.4) 1 6 50k-file scope on every verify hash cache keyed (path, size, mtime), rehash on mismatch — the git-index trick 2 7 Duplicate entries for one (item, digest) across scratch/segments/base read-model resolution: max (at, line-hash); identical-timestamp ties prefer fail 1 8 Secret pasted into evidence mark-time scan §7 1 9 @run binary missing locally loud FAILED + install hint; never skip (I8) 1 10 Checklist item deleted / reworded orphaned lines dropped at compaction; reworded = new digest → forces (§5) 1 11 Marked but never sealed, pushed anyway CI sees committed state only → forced; sealing is self-enforcing (§6.3) 1 12 Two compaction runs race second push rejected → rerun; single-writer is placement policy, not a lock 1 13 Broad scope (src/**) forces every single run thrash → rubber-stamping. Mitigations: narrow item scopes; gate at boundaries not turns; qc stats flags "forced 40×, passed 40×" as an unwritten lint rule 1/2 14 Hand-edited state line is malformed warn on stderr, treat as absent → item forced. Hand-editing fails safe; it never bricks verify (§4.4.1) 1 15 A scope pattern resolves into qc/state/ exit-2 config error: the attestation would go stale the moment sealing wrote it — a self-invalidation loop, made unconstructable (§4.5). Checklist files remain pierceable for meta-attestation (§16) 1

## 14. Phasing and architecture

Phase 1 (the real core, ~700–900 lines): grammar + parsers (checklist + .qcs), digest, three-tier state store with read-model resolution, seal/reset/compact, manifests, init/add/template list|show/plan/decide/mark/baseline/verify(--staged,--ci)/report, evidence layers 1–2, secret scan, CRLF, ignore rules + piercing + self-scope guard, requires-topology, head-SHA CI recipe + compaction job, SKILL.md, bundled templates, release shims (npx/uvx qc-gate init|update) + sha256 release manifest.

Phase 2 (bolts on without rework): template diff + --set, qc diff + --carry, qc update --check, @cache, mtime hash cache, @evidence shapes, qc stats, merge-queue mode, git-URL template sources, scheduled update workflow / Renovate preset.

Why phase 2 is additive, structurally: manifests already persist per-file hashes (the mtime cache sits beside them under .qc/); the .qcs line tolerates unknown fields (carry annotations slot in); @evidence tightens an existing validator; merge-queue mode is plain verify on a different checkout; stats reads data verify already computes. No schema breaks anywhere on the path — this is the "simple UX, proper architecture" contract.

Single file is a distribution choice, not a structure choice: internally sectioned (globmatch / digest / parser / store / states / commands), tested by a tests/ directory in the tool's own repo (not vendored), including a cross-platform CRLF/paths matrix and a parallel-branch merge matrix in its CI. Of the ~850 lines, roughly 300 are the deterministic kernel and 150+ are message strings — the prompts. Those are not overhead to minimize; they are the product (I5).

## 15. Distribution and updates

### 15.1 Packaging: the skill

"Big" measures the wrong axis. What matters is not how many lines the tool has but how many must live in the agent's context — and a skill folder separates the two cleanly:

qc-skill/   SKILL.md          ~80 lines   ← the ONLY context-resident part   scripts/qc.py     ~850 lines  ← executed, never read into context   templates/*.md    8 files     ← loaded on demand via `qc template show` 

The agent runs python3 scripts/qc.py init; it never reads the source. This is the same pattern as Anthropic's document skills — a slim SKILL.md fronting multi-hundred-line scripts. In absolute terms the vendored qc/qc.py is ~32 KB of stdlib-only, human-auditable text with zero install step and zero dependency tree.

SKILL.md stays at ~80 lines because I5 was secretly a distribution decision: since every string the tool prints is written as a prompt, the runtime output is the documentation, delivered just-in-time at exactly the moment it's relevant — which beats front-loaded instructions in a context window every time. The skill therefore teaches only the loop shape (§9), not version-specific behavior — so an old skill drives a new kernel correctly, and skill updates and kernel updates ride separate, independent cadences.

A kernel/toolkit split (vendor only the ~300-line verify kernel; keep mark/seal/plan/templates in the skill) exists as a clean seam but is deliberately unexercised: it would manufacture a version-skew axis that the single vendored file makes structurally impossible. Rule of thumb: single file until ~1,500 lines or a second language; until then, keep the internal module boundaries honest so the split stays a one-day refactor. The file size is the complexity budget made physical — any feature that can't live in the single file is a feature to interrogate before accommodating.

### 15.2 Channels: registries as transport

npx qc-gate init        # or: uvx qc-gate init · or: curl … | sh npx qc-gate update      # fetch pinned release, sha256-verify, rewrite qc/qc.py, print the diff 

The installer is a thin shim that exits the trust path the moment it finishes — after init, nothing about the repo, its hooks, or its CI touches a registry ever again (I12). All channels (npm, PyPI, curl) converge on the byte-identical vendored artifact, verified against a sha256 release manifest. This is the Gradle-wrapper pattern: commit the wrapper, run the wrapper, zero install, pinned by construction — except our wrapper is readable, which is why compiled binaries are also out: an 850-line script that gates you is auditable in review; an opaque per-platform blob is not.

### 15.3 Updates are pull requests

A package manager bundles four properties: discovery, delivery, application, rollback. For a gate — code that executes in every CI run with the repo checked out — automatic application is disqualified: every major registry incident (chalk/debug, Shai-Hulud) was an update-channel attack, and "users get patches automatically" and "users get compromised automatically" are the same mechanism with different payloads. The other three properties come back stronger:

Property How qc gets it Discovery qc update --check; a shipped scheduled workflow (weekly: run the shim, open a PR) and/or a Renovate custom-manager preset watching the release feed Delivery shim fetch, sha256-verified against the release manifest Application a pull request whose diff is the actual changed lines of the gate — the diff is the changelog, reviewed like any code. Contrast pinned npm, where the reviewable diff is a version number and the rest is faith in the registry Rollback git revert — offline, forever, no registry required to still serve an old version

Honest residuals: fleet drift (200 repos on a dozen versions — but pinned npm has identical per-repo-PR cardinality; only banned unpinned @latest avoids it; the open update-PR list is the drift dashboard, and state never crosses repos so drift is structurally harmless). Slower panic-fix propagation than npm audit fix culture — offset by vendoring's superpower: a repo can hot-patch its own gate in-tree today, visible in review, and reconcile with upstream later. No lockfile integrity metadata — replaced by the sha-verified manifest at fetch time and git history afterward. And one category vanishes outright: npm audit exists because of transitive dependencies; a stdlib-only single file has none — the entire audit surface is one file a human can read.

### 15.4 Runtime: why exactly one dependency, and why this one

Three alternatives lost on the same axis — each trades a small visible cost for an invisible expansion of the verdict's dependency surface (I11):

Node/npx as runtime: puts registry availability and registry compromise inside the gate's trust path, and reintroduces version skew (global 1.4 vs repo 1.2 vs CI resolution) — verdict divergence, which for a determinism tool is death (I1).

Compiled binary: breaks everything-is-reviewable-text.

Bash + system tools: not zero dependencies but the largest unpinned dependency surface available — GNU vs BSD forks of sed/grep/date/hashing, macOS's frozen bash 3.2 (no globstar, so src/** is inexpressible), and the killer: sort's locale collation would order src/API.py vs src/api.py differently on en_US.UTF-8 vs C, silently forking digests for identical bytes — the exact bug class this tool exists to abolish. Google's shell style guide draws the rewrite line at ~100 lines; the kernel is 8× past it, and an honest port lands at 1,500–2,500 lines needing a GNU/BSD matrix just to trust verify.

Shell still lives everywhere it belongs — the extension surface, not the kernel: @run items are exactly where grep and friends get reused, the bin/ shims and hooks are sh, and the .qcs data plane is unix-native by design. And one hybrid is banned explicitly: a "lite" POSIX verify for pythonless environments — two implementations of the digest will disagree within a month (I11).

The honest near-tie: a dependency-free single-file Node kernel is equally achievable, and would win if the audience were overwhelmingly JS-first — the criterion is which interpreter is reliably present where agents run, an audience fact rather than an architecture question. Python wins today on polyglot repos (a Go service shouldn't need Node for its gate). Whichever is chosen: one implementation, forever.

## 16. Security and trust model

Plainly: committed state is forgeable by anyone with write access, and no local mechanism fixes that. qc provides provenance; authorization comes from things a PR cannot edit:

Branch protection: required review + the qc workflow as a required status check, both configured outside the repo's PR-editable surface.

@human audit = git blame on the segment line while the PR is under review — authored by someone other than the PR author, in a commit the reviewer can see. That is the only moment authorship matters; after compaction the base lines belong to the compaction commit, and the originals live in history (git log -p). Locally, @human is a speed bump and a prompt; in CI + branch protection, it's real.

Checklists execute shell (@run) — checklist files are code. They review like code, and CI runs them with CI's permissions. This is stated, not hidden.

Policy files in-repo are editable by the PR they gate; treat qc/ paths as CODEOWNERS-protected if that matters — or gate the policy with qc itself: an @human item whose scope names the checklist files exactly (ignore-piercing, §4.5) forces a human attestation whenever the checklists change. The state store itself can never be scoped (edge 15), so the guard cannot loop.

Gate updates arrive only as PRs whose diff is the gate's own source (I12, §15.3); the release manifest's sha256 covers transport, git history covers everything after.

## 17. Risks (consolidated, honest)

Rot — the primary risk, not technical: checklists grow, nobody prunes, 70 of 80 items get rubber-stamped. Antidotes: mandatory evidence makes items expensive; minimal-by-default templates; the stats ratchet (attest→run conversion rate as the health metric — if it's zero for months, the checklist isn't earning its place). Thrash — broad scopes force constantly (edge 13). Gaming — evidence shape is checkable, thought is not; the design accepts this and optimizes for making lazy answers visible in review rather than impossible. Forgery — §16. Scope-authoring errors — too narrow never fires, too broad always fires; stats outliers surface both. History growth — attestation churn accretes in git history by design (it is the archive); squash merges bound it per PR, and it is the price of forensic recoverability. Update lag — a fleet on reviewed updates patches slower than one on @latest; that is the accepted cost of I12, softened by in-tree hot-patching.

## 18. Decision log — supersessions from the design discussion

# Earlier position Final position Why D1 Run state local-only, per-branch runs/<branch>.json → D11 superseded twice; see D11 D2 extends: "in week two" Copy-only templates, extends never legibility of the effective checklist in PR review (I7) D3 force_when: glob override Eliminated; forcing is staleness one mechanism (I3); force_when duplicated @scope D4 Applicability via git merge-base changed-file set @applies pseudo-item over default scope, since-last-answer removed git from the verdict path entirely; same machinery as items D5 @human @sticky for rebase churn Merge-queue strict gate nags once at merge instead of every rebase; no new annotation D6 Single committed ledger, "sorted" → then append-only for blame → D11 growth objection; blame need survives via segments + history (§16) D7 Ledger conflict "last-write-wins" → (at, line-hash) order, ties prefer fail Retained as the read-model resolution across all tiers correct logical rule regardless of physical placement D8 (unspecified) Baselines honor expiry grandfathering excuses old content, not periodic checks D9 (unspecified) File checkboxes display-only state store is single source of truth; editor ticks must not drift D10 "~250 lines" ~700–900, single file, phased edge cases 1/3/4/5/7/9 are correctness, not gold-plating D11 One committed append-only ledger.jsonl (v0.1) Three tiers: gitignored scratch → sealed per-branch segments → trunk-compacted base unbounded growth + PR review noise; git history is the archive, so worktree deletion loses nothing; scratch gives the agent a resettable inner loop D12 Monolithic committed state.jsonl snapshot (considered between v0.1 and v0.2) Rejected hot items (broad scopes) are re-attested by every PR → every PR pair conflicts on those lines, and the resolution carries zero information (post-merge content is stale anyway) D13 merge=union + custom drivers as load-bearing merge machinery Demoted to optional local convenience for same-branch EOF collisions I10: correctness must not depend on gitattributes or forge merge settings; both-add segment files need no driver at all D14 qc prune (ledger GC with age windows and multi-branch caveats) Folded into qc compact (orphan GC + ≤1 line per item) compaction already walks every entry; a second GC concept earns nothing D15 JSONL state lines (v0.1–v0.2) .qcs line grammar: ref = status @digest key:value… :: evidence, 16-hex digests, one record per line, malformed lines fail safe as absent v0.2 made segment diffs a review surface, so committed lines must read as claims; evidence needs zero escaping (backticked commands, pipes); markdown tables rejected — pipe-escaping lands exactly on the most technical evidence; one-record-one-line preserves union-safety, blame granularity, and grep-ability D16 (question) publish as an npm-executed package Registries are channels, never runtimes; installer shims exit the trust path after init/update registry availability would gate merges; the update channel is the attack channel; unpinned resolution breaks I1 D17 (question) bash kernel reusing system tools Kernel keeps exactly one pinnable dependency (I11); shell lives at the edges (@run, shims, hooks) and in the greppable .qcs data plane coreutils are the largest unpinned dependency surface: GNU/BSD forks, macOS bash 3.2, locale-collated sort silently forking digests D18 (question) package-manager update flow Discovery + transport automated (shim, scheduled PR workflow / Renovate preset, sha manifest); application is always a PR; rollback is git revert the diff is the changelog; zero-effort unpinned freshness is the one banned mode; open update-PRs are the fleet-drift dashboard D19 Single .qc/ mixing committed and ignored content; multi-entry .gitignore Visible/dotted twin: qc/ = committed review surface (tool, checklists, state), .qc/ = disposable local workspace (scratch, manifests, cache); .gitignore is one line the dot carries the semantics; ephemerality by construction instead of ignore-list discipline; rm -rf .qc/ always safe; matches gradle/↔.gradle/ and *.tf↔.terraform/; surfaced the self-scope guard (edge 15) and policy self-gating (§16.4)

## 19. Behavioral examples

Digests appear as 8-hex in CLI transcripts for brevity; raw .qcs lines show the stored 16. Every transcript below was checked against §§4–7.

E1 — Adoption in an existing repo

$ qc init Created qc/ (tool + checklists, committed) and .qc/ (local workspace, ignored) .gitignore += ".qc/" · .gitattributes += "qc/state/*.qcs merge=union"  $ qc add code-quality Copied builtin/code-quality@3 (minimal, 6 items) → qc/checklists/code-quality.md  $ qc verify FORCED (never)  code-quality/@applies   — and 5 more items never answered exit 1  $ qc baseline -m "adopted 2026-08-06 at v2.3; existing code grandfathered" Baselined 6 items → scratch.  $ git add -A            # qc/ tree; pre-commit hook fires: seal --staged: promoted 6 entries → qc/state/seg-main-a41f09.qcs verify --staged: all clear · 2 run · 3 attest fresh · 1 dormant (deps: no lockfile present) exit 0 

The commit that introduces the checklist carries its own baseline — trunk CI would fail the PR otherwise (§6.4, edge 5).

E2 — Agent loop with evidence rejection

$ qc plan 1  code-quality/api-shape   FORCED (stale)    Q: Which public symbols changed, and which are breaking?    Changed since your last answer (3d ago): src/api/routes.py    You said then: "no public surface touched; internal refactor of pagination"  $ QC_BY=agent qc mark code-quality/api-shape --pass -m "checked, looks fine" REJECTED — evidence must point at something falsifiable (a path, a number, a command, a link), not restate that checking happened. exit 2  $ QC_BY=agent qc mark code-quality/api-shape --pass \     -m "routes.py adds GET /v2/orders/{id}/events; no existing route or schema field removed; openapi diff attached in PR" Recorded pass → scratch · digest @9c41d7aa · 4 files 

The rejection names the principle, never the passing token (I5, §7). The answer sits in scratch — revisable, resettable — until the commit seals it.

E3 — The typo after the mark

$ qc mark code-quality/naming --pass -m "renamed `proc2` → `settle_invoices` in src/billing/run.py; rest read clean" Recorded pass → scratch · digest @1f0e88b2  $ sed -i 's/settle_invoices/settle_invoice/' src/billing/run.py   # one-char fix $ git add -A && git commit -m "billing rename" pre-commit → seal --staged: promoted 0 of 1 (naming: scratch digest no longer matches staged content)            → verify --staged FORCED (stale)  code-quality/naming   Changed since your answer (1m ago): src/billing/run.py   You said then: "renamed `proc2` → `settle_invoices` …" exit 1  — commit aborted 

The index — what ships — is what gets sealed and verified, never the stale memory of the tree (edge 1). Re-mark against the fixed content → hook promotes and passes.

E4 — Ten parallel PRs, one hot item

# PR #1 merges to main. Its answers ride in qc/state/seg-pr1-billing-c07d2e.qcs.  # PR #2, branched last week, rebases onto the new main: $ git rebase origin/main Successfully rebased.            # segment files are both-add → zero conflicts (I10)  $ qc verify FORCED (stale)  code-quality/diff-read   Q: What did you find reading the full diff, top to bottom?   Changed since your last answer: src/billing/run.py (arrived via merge of PR #1)   You said then: "auth paths untouched; two TODOs noted in src/api/hooks.py"   → That answer may no longer hold. Re-read the diff before responding. exit 1 

The snapshot design produced a merge conflict here whose resolution carried zero information; the segment design produces a named question instead. Weeks later, the scheduled job runs qc compact on main: 12 segments fold into base.qcs (≤ 1 line per item, ~10 KB), the segment files are deleted, and every folded line remains in git log -p.

E5 — n_a with justification; @strict refusing it

$ qc mark code-quality/api-shape --na -m "change touches only src/api/internal/ helpers; no route or schema module in diff" Recorded n_a → scratch · clears the gate once sealed · blameable in review  $ qc mark data-migration/reversible --na -m "small table" REFUSED — this item is @strict: not-applicable is not an acceptable answer here. Answer the question or record a fail with your reasoning. exit 2 

E6 — CI on a pull request, and the diff as review surface

verify --ci  (checkout: head.sha; committed state only — scratch does not exist here) FORCED (never)  security-review/authz-paths        ← checklist added in this PR, never baselined FORCED (never)  code-quality/naming                ← marked locally but never sealed/committed (edge 11) FAILED          code-quality/types                 ← mypy: 2 errors (last 10 lines follow) PENDING @human  code-quality/diff-read exit 1 

The PR's own file diff already shows the author's claims, one line per claim (§4.4.1):

+code-quality/api-shape = pass @9c41d7aa41f0b2c3 by:agent at:2026-08-06T10:12:41Z files:4 :: routes.py adds GET /v2/orders/{id}/events; no existing route or schema field removed +code-quality/diff-read = pass @1f0e88b2aa93c407 by:mira at:2026-08-06T11:03:29Z :: read full diff; auth paths untouched; two TODOs noted in src/api/hooks.py 

And report --md posts the same claims as a sticky comment table (item, status, by, age, evidence). "Additive only" sitting beside a code diff that deletes a field is a five-second catch. Approving the PR (branch protection) plus a diff-read line whose blame is a human other than the author is the enforced human gate (§16).

## 20. Open questions

Monorepo multi-root (qc/ per package vs. one root with prefixed scopes) — leaning one root, prefixed scopes, revisit on real friction. Template upstream pinning for git-URL sources (tag vs. sha) — sha, probably. Whether qc stats should ever block (a hard cap on attest items per checklist) — currently no: pressure via visibility, not authority. Whether compaction should optionally preserve N historical entries per item for richer echoes — currently no: one is enough, history has the rest. Whether the release manifest should add a minisign/sigstore signature alongside sha256 — leaning yes, eventually, once there's a release process worth signing.

End of specification v0.4.
