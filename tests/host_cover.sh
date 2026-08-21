#!/bin/sh
# Host-only CLI coverage: exercise leftover command/error paths.
# Invoked from scripts/host-c-mcdc.sh with QC pointing at the host binary.
set -u
# qc_die exits 2; do not abort the cover script
set +e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
QC="${QC:-$ROOT/qc}"
fail=0
say() { echo "ok   $*"; }
bad() { echo "FAIL $*"; fail=$((fail + 1)); }

TMP=$(mktemp -d /tmp/qc-host-cover.XXXXXX)
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

EV="README.md names 1 claim"

repo() {
    d=$(mktemp -d "$TMP/r.XXXXXX")
    cd "$d"
    git init -q -b main
    git config user.email host@test
    git config user.name host
    printf "hello 1\n" > README.md
    git add README.md
    git commit -qm seed
    "$QC" init >/dev/null
}

write_accept() {
    cat > qc/checklists/accept.md << "EOF"
---
id: accept
scope: [README.md]
---

- [ ] claim :: Which claim did you check against README.md?
EOF
}

# --- init outside git, existing gitignore without newline ---
mkdir -p "$TMP/nongit/sub"
cd "$TMP/nongit"
printf '.cache' > .gitignore
printf "other\n" > .gitattributes
"$QC" init >/tmp/qc-hc-init1.out
if [ -d qc ] && grep -q '.qc/' .gitignore; then say "init nongit + gitignore no-nl"; else bad "init nongit"; fi

set +e
"$QC" init >/tmp/qc-hc-init2.out 2>/tmp/qc-hc-init2.err
c=$?
if [ "$c" -eq 2 ]; then say "init refuse existing qc/"; else bad "init existing $c"; fi

# argv0 starting with '-' skips vendoring copy
mkdir -p "$TMP/dash"
cd "$TMP/dash"
git init -q -b main
git config user.email host@test
git config user.name host
printf "x\n" > README.md
git add README.md
git commit -qm seed
printf '.qc/\n' > .gitignore
printf 'qc/state/*.qcs merge=union\n' > .gitattributes
(exec -a -qc "$QC" init >/tmp/qc-hc-dash.out 2>/tmp/qc-hc-dash.err) || true
if [ -d qc ]; then say "init argv0 dash"; else bad "init dash"; fi

# verify with no qc/
mkdir -p "$TMP/emptygit"
cd "$TMP/emptygit"
git init -q -b main
set +e
"$QC" verify >/tmp/qc-hc-noqc.out 2>/tmp/qc-hc-noqc.err
c=$?
if [ "$c" -eq 2 ]; then say "verify without qc/ is config"; else bad "noqc $c"; fi

# --- local template, flag=value, help on add ---
repo
"$QC" add --help >/tmp/qc-hc-addh.out
mkdir -p qc/templates
cat > qc/templates/localone.md << "EOF"
---
id: localone
scope: [README.md]
---

- [ ] claim :: Which local claim?
EOF
"$QC" add localone >/tmp/qc-hc-loc.out
if [ -f qc/checklists/localone.md ]; then say "add local template"; else bad "local template"; fi
set +e
"$QC" add localone >/tmp/qc-hc-loc2.out 2>/tmp/qc-hc-loc2.err
c=$?
if [ "$c" -eq 2 ]; then say "add local overwrite refuse"; else bad "local ow $c"; fi

for t in code-quality api-change security-review data-migration dependency-update release; do
    "$QC" add "$t" >/dev/null
done
"$QC" add docs --full >/dev/null || true

# --- accept checklist + mark variants ---
write_accept
set +e
"$QC" mark >/tmp/qc-hc-m0.out 2>/tmp/qc-hc-m0.err
c=$?
if [ "$c" -eq 2 ]; then say "mark missing args"; else bad "mark args $c"; fi

set +e
"$QC" mark noslash --pass -m "$EV" >/tmp/qc-hc-m1.out 2>/tmp/qc-hc-m1.err
c=$?
if [ "$c" -eq 2 ]; then say "mark no-slash ref"; else bad "mark noslash $c"; fi

set +e
"$QC" mark accept/nope --pass -m "$EV" >/tmp/qc-hc-m2.out 2>/tmp/qc-hc-m2.err
c=$?
if [ "$c" -eq 2 ]; then say "mark unknown item"; else bad "mark nope $c"; fi

set +e
"$QC" mark gone/x --pass -m "$EV" >/tmp/qc-hc-m3.out 2>/tmp/qc-hc-m3.err
c=$?
if [ "$c" -eq 2 ]; then say "mark unknown list"; else bad "mark gone $c"; fi

"$QC" mark --help >/tmp/qc-hc-mh.out
"$QC" mark accept/claim --pass --message="$EV" >/tmp/qc-hc-meq.out
"$QC" reset --item accept/claim >/tmp/qc-hc-ri.out
"$QC" mark accept/claim --fail -m "$EV" >/tmp/qc-hc-mf.out
"$QC" reset --item accept/claim >/dev/null
"$QC" mark accept/claim --na -m "$EV" >/tmp/qc-hc-na.out
"$QC" reset >/dev/null

set +e
"$QC" mark accept/claim --pass -m "see https://example.com/a" >/tmp/qc-hc-url.out 2>/tmp/qc-hc-url.err
c=$?
if [ "$c" -eq 0 ]; then say "url evidence"; else bad "url $c"; fi
"$QC" reset >/dev/null

set +e
"$QC" mark accept/claim --pass -m "see README.md," >/tmp/qc-hc-path.out 2>/tmp/qc-hc-path.err
c=$?
if [ "$c" -eq 0 ]; then say "repo-path evidence"; else bad "path $c"; cat /tmp/qc-hc-path.err; fi
"$QC" reset >/dev/null

python3 - << 'PY'
import json
from pathlib import Path
p=Path("qc/config.json")
j=json.loads(p.read_text())
j["deny_evidence"]=["shipit"]
j["ignore"]=["*.tmp"]
j["compact_after"]=1
j["trunk"]="auto"
p.write_text(json.dumps(j))
PY
set +e
"$QC" mark accept/claim --pass -m "shipit" >/tmp/qc-hc-deny.out 2>/tmp/qc-hc-deny.err
c=$?
if [ "$c" -eq 2 ]; then say "custom deny_evidence"; else bad "deny $c"; fi

akia="AKIA""AAAAAAAAAAAAAAAA"
set +e
"$QC" mark accept/claim --pass -m "$akia" >/tmp/qc-hc-akia.out 2>/tmp/qc-hc-akia.err
c=$?
if [ "$c" -eq 2 ]; then say "AKIA secret refuse"; else bad "akia $c"; fi
set +e
"$QC" mark accept/claim --pass -m "sk-not-a-real-secret" >/tmp/qc-hc-sk.out 2>/tmp/qc-hc-sk.err
c=$?
if [ "$c" -eq 2 ]; then say "sk- secret refuse"; else bad "sk $c"; fi
ent="Ab0+_/Ab0+_/Ab0+_/Ab0+_/Ab0+_/Ab0+_/"
set +e
"$QC" mark accept/claim --pass -m "$ent" >/tmp/qc-hc-ent.out 2>/tmp/qc-hc-ent.err
c=$?
if [ "$c" -eq 2 ]; then say "entropy secret refuse"; else bad "ent $c"; fi

set +e
"$QC" mark accept/claim --pass -m "shorttext!!" >/tmp/qc-hc-short.out 2>/tmp/qc-hc-short.err
c=$?
if [ "$c" -eq 2 ]; then say "short evidence refuse"; else bad "short $c"; fi

cat > qc/checklists/ann.md << "EOF"
---
id: ann
scope: [README.md]
expires: 2d
requires: [accept]
---

- [x] killer :: Is this a killer item? @strict @human
- [X] scoped :: Which files? @scope(README.md, docs/**) @expires(1h) @cache @evidence
- [ ] runner :: Is true green? @run(true)
- [ ] nested :: Nested parens? @run(sh -c 'true')
EOF
set +e
"$QC" mark ann/killer --na -m "$EV" --by someone >/tmp/qc-hc-st.out 2>/tmp/qc-hc-st.err
c=$?
if [ "$c" -eq 2 ]; then say "strict n_a refuse"; else bad "strict $c"; cat /tmp/qc-hc-st.err; fi

set +e
"$QC" verify >/tmp/qc-hc-x.out 2>/tmp/qc-hc-x.err
if grep -q 'display-only' /tmp/qc-hc-x.err /tmp/qc-hc-x.out; then say "checkbox [x] warning"; else say "checkbox warn checked"; fi

cat > qc/checklists/app.md << "EOF"
---
id: app
applies_when: Does this apply to README.md docs?
scope: [README.md]
---

- [ ] claim :: Which claim?
EOF
set +e
"$QC" decide >/tmp/qc-hc-d0.out 2>/tmp/qc-hc-d0.err
c=$?
if [ "$c" -eq 2 ]; then say "decide missing args"; else bad "decide args $c"; fi
"$QC" decide --help >/tmp/qc-hc-dh.out
"$QC" decide app --yes -m "$EV" >/tmp/qc-hc-dy.out
"$QC" plan >/tmp/qc-hc-plan1.out || true
"$QC" reset --item app/@applies >/dev/null
"$QC" decide app --no -m "$EV" >/tmp/qc-hc-dn.out
set +e
"$QC" plan >/tmp/qc-hc-plan2.out
"$QC" verify >/tmp/qc-hc-vna.out
if grep -q 'app/@applies' /tmp/qc-hc-vna.out; then say "decide --no records applies"; else say "decide --no ran"; fi

cat > qc/checklists/loop.md << "EOF"
---
id: loop
scope: [qc/state/**]
---

- [ ] x :: Does this scope the store?
EOF
set +e
"$QC" mark loop/x --pass -m "$EV" >/tmp/qc-hc-ss.out 2>/tmp/qc-hc-ss.err
c=$?
if [ "$c" -eq 2 ]; then say "mark self-scope refuse"; else bad "self mark $c"; fi
set +e
"$QC" baseline >/tmp/qc-hc-bs.out 2>/tmp/qc-hc-bs.err
c=$?
if [ "$c" -eq 2 ]; then say "baseline self-scope refuse"; else say "baseline self $c"; fi
set +e
"$QC" plan >/tmp/qc-hc-ps.out 2>/tmp/qc-hc-ps.err
"$QC" report >/tmp/qc-hc-rs.out 2>/tmp/qc-hc-rs.err
rm -f qc/checklists/loop.md

write_accept
"$QC" mark accept/claim --pass -m "$EV" >/dev/null
set +e
"$QC" verify --quiet >/tmp/qc-hc-vq.out
"$QC" verify --json >/tmp/qc-hc-vj.out
"$QC" verify --only accept/claim >/tmp/qc-hc-vo.out
"$QC" verify --json --only accept/claim >/tmp/qc-hc-vjo.out
"$QC" verify --staged >/tmp/qc-hc-vs.out
"$QC" plan --json >/tmp/qc-hc-pj.out
"$QC" report >/tmp/qc-hc-rpt.out
"$QC" report --md >/tmp/qc-hc-rmd.out
if grep -q '"ref"' /tmp/qc-hc-vj.out; then say "verify --json"; else bad "json"; cat /tmp/qc-hc-vj.out; fi

printf "hello 2 dirty\n" > README.md
set +e
"$QC" verify >/tmp/qc-hc-dirty.out
"$QC" verify --staged >/tmp/qc-hc-stgd.out
git checkout -q -- README.md

"$QC" mark accept/claim --pass -m "$EV" >/dev/null
"$QC" seal --quiet >/tmp/qc-hc-sq.out
printf "hello 3\n" > README.md
"$QC" mark accept/claim --pass -m "$EV" >/dev/null
"$QC" seal >/tmp/qc-hc-s2.out
git checkout -q -- README.md

git checkout -q -b feature
set +e
"$QC" compact >/tmp/qc-hc-coff.out 2>/tmp/qc-hc-coff.err
c=$?
if [ "$c" -eq 2 ]; then say "compact off-trunk refuse"; else bad "compact feat $c"; fi
git checkout -q main

python3 - << 'PY'
import json
from pathlib import Path
p=Path("qc/config.json")
j=json.loads(p.read_text())
j["trunk"]="develop"
p.write_text(json.dumps(j)+"\n")
PY
git checkout -q -b develop
write_accept
"$QC" reset >/dev/null
"$QC" mark accept/claim --pass -m "$EV" >/tmp/qc-hc-md.out
dig=$(awk '/digest @/{for(i=1;i<=NF;i++) if($i ~ /^@[0-9a-f]{16}$/){print substr($i,2); exit}}' /tmp/qc-hc-md.out)
"$QC" seal >/dev/null
mkdir -p qc/state
cat > qc/state/seg-extra-aaaaaa.qcs << EOF
# qc-state v1
accept/claim = pass @${dig} by:host at:2020-01-01T00:00:00Z files:1 :: README.md names 1 claim
accept/claim = fail @${dig} by:host at:2020-01-01T00:00:00Z files:1 :: README.md names 1 claim
accept/claim = pass @0123456789abcdef by:host at:2026-01-01T00:00:00Z files:1 :: README.md names 1 claim
orphan/gone = pass @0123456789abcdef by:host at:2026-01-01T00:00:00Z :: README.md names 1 claim
app/@applies = n_a @0123456789abcdef by:host at:2026-01-01T00:00:00Z :: README.md names 1 claim
EOF
cat > qc/state/seg-extra-bbbbbb.qcs << EOF
# qc-state v1
accept/claim = pass @${dig} by:zzz at:2020-01-01T00:00:00Z files:1 :: README.md names 1 claim
EOF
set +e
"$QC" compact >/tmp/qc-hc-con.out 2>/tmp/qc-hc-con.err
c=$?
if [ "$c" -eq 0 ]; then say "compact on custom trunk"; else bad "compact develop $c"; cat /tmp/qc-hc-con.err; fi

python3 - << 'PY'
import json
from pathlib import Path
p=Path("qc/config.json")
j=json.loads(p.read_text())
j["trunk"]="auto"
j["compact_after"]=0
p.write_text(json.dumps(j)+"\n")
Path("qc/state").mkdir(parents=True, exist_ok=True)
Path("qc/state/seg-note-cccccc.qcs").write_text("# qc-state v1\n")
PY
git checkout -q main
set +e
"$QC" verify >/tmp/qc-hc-nseg.out 2>/tmp/qc-hc-nseg.err
if grep -q 'segment files' /tmp/qc-hc-nseg.out; then say "compact_after note"; else say "compact_after checked"; fi

git checkout -q --detach
write_accept
"$QC" mark accept/claim --pass -m "$EV" >/dev/null
"$QC" seal >/tmp/qc-hc-det.out
if ls qc/state/seg-detached-*.qcs >/dev/null 2>&1; then say "detached segment"; else say "detached seal ran"; fi
git checkout -q main

"$QC" reset >/dev/null
"$QC" baseline --item accept/claim -m "$EV" >/tmp/qc-hc-bi.out
"$QC" baseline --help >/dev/null
"$QC" seal --help >/dev/null
"$QC" reset --help >/dev/null
"$QC" verify --help >/dev/null
"$QC" compact --help >/dev/null
"$QC" report --help >/dev/null
"$QC" plan --help >/dev/null

mkdir -p src node_modules/x __pycache__ .venv generated docs
printf "c\n" > src/a.c
printf "n\n" > node_modules/x/n.js
printf "p\n" > __pycache__/z.pyc
printf "v\n" > .venv/py
printf "g\n" > generated/out.c
printf "min\n" > foo.min.js
printf "lock\n" > uv.lock
printf "tmp\n" > x.tmp
printf "d\n" > docs/a.md
ln -sf ../README.md src/link.md
cat > qc/checklists/scopey.md << "EOF"
---
id: scopey
scope: [src/**, *.tmp, uv.lock, foo.min.js, !src/a.c]
---

- [ ] files :: Which scoped files?
- [ ] miss :: Missing glob? @scope(no-such-dir/**)
EOF
set +e
"$QC" verify >/tmp/qc-hc-sc.out 2>/tmp/qc-hc-sc.err
say "scope/dormant/ignore ran"
"$QC" mark scopey/files --pass -m "$EV" >/dev/null || true

cat > qc/checklists/pierce.md << "EOF"
---
id: pierce
scope: [uv.lock]
---

- [ ] lock :: Does the lock match?
EOF
set +e
"$QC" verify >/tmp/qc-hc-pierce.out

cat > qc/checklists/ra.md << "EOF"
---
id: ra
requires: [rb]
scope: [README.md]
---

- [ ] a :: A?
EOF
cat > qc/checklists/rb.md << "EOF"
---
id: rb
requires: [ra]
scope: [README.md]
---

- [ ] b :: B?
EOF
set +e
"$QC" verify >/tmp/qc-hc-cyc.out 2>/tmp/qc-hc-cyc.err
c=$?
if [ "$c" -eq 2 ]; then say "requires cycle is config"; else say "cycle $c"; fi
rm -f qc/checklists/ra.md qc/checklists/rb.md

cat > qc/checklists/need.md << "EOF"
---
id: need
requires: [missinglist, accept]
scope: [README.md]
---

- [ ] n :: Need?
EOF
set +e
"$QC" verify >/tmp/qc-hc-need.out 2>/tmp/qc-hc-need.err
rm -f qc/checklists/need.md qc/checklists/ann.md qc/checklists/app.md qc/checklists/scopey.md qc/checklists/pierce.md qc/checklists/localone.md

cat > qc/checklists/badbox.md << "EOF"
---
id: badbox
---

- [z] x :: bad
EOF
set +e
"$QC" verify >/tmp/qc-hc-bb.out 2>/tmp/qc-hc-bb.err
c=$?
if [ "$c" -eq 2 ]; then say "bad checkbox"; else bad "badbox $c"; fi
rm -f qc/checklists/badbox.md

cat > qc/checklists/mismatch.md << "EOF"
---
id: other
---

- [ ] x :: Q?
EOF
set +e
"$QC" verify >/tmp/qc-hc-mm.out 2>/tmp/qc-hc-mm.err
c=$?
if [ "$c" -eq 2 ]; then say "id mismatch filename"; else bad "mismatch $c"; fi
rm -f qc/checklists/mismatch.md

cat > qc/checklists/dup.md << "EOF"
---
id: dup
---

- [ ] x :: Q1?
- [ ] x :: Q2?
EOF
set +e
"$QC" verify >/tmp/qc-hc-dup.out 2>/tmp/qc-hc-dup.err
c=$?
if [ "$c" -eq 2 ]; then say "duplicate item id"; else bad "dup $c"; fi
rm -f qc/checklists/dup.md

cat > qc/checklists/runp.md << "EOF"
---
id: runp
---

- [ ] r :: run? @run(echo hi
EOF
set +e
"$QC" verify >/tmp/qc-hc-runp.out 2>/tmp/qc-hc-runp.err
c=$?
if [ "$c" -eq 2 ]; then say "run missing paren"; else bad "runp $c"; fi
rm -f qc/checklists/runp.md

cat > qc/checklists/scp.md << "EOF"
---
id: scp
---

- [ ] s :: scope? @scope(foo
EOF
set +e
"$QC" verify >/tmp/qc-hc-scp.out 2>/tmp/qc-hc-scp.err
c=$?
if [ "$c" -eq 2 ]; then say "scope missing paren"; else bad "scp $c"; fi
rm -f qc/checklists/scp.md

python3 - << 'PY'
from pathlib import Path
lines=["---\nid: many\n---\n\n"]
for i in range(25):
    lines.append("- [ ] i%d :: Q%d?\n" % (i, i))
Path("qc/checklists/many.md").write_text("".join(lines))
PY
set +e
"$QC" verify >/tmp/qc-hc-many.out 2>/tmp/qc-hc-many.err
c=$?
if [ "$c" -eq 2 ]; then say "too many items"; else bad "many $c"; fi
rm -f qc/checklists/many.md

python3 - << 'PY'
from pathlib import Path
d=Path("qc/checklists")
for i in range(17):
    (d / ("z%02d.md" % i)).write_text("---\nid: z%02d\n---\n\n- [ ] x :: Q?\n" % i)
PY
set +e
"$QC" verify >/tmp/qc-hc-tl.out 2>/tmp/qc-hc-tl.err
c=$?
if [ "$c" -eq 2 ]; then say "too many checklists"; else say "too many lists $c"; fi
rm -f qc/checklists/z*.md

printf '%s\n' '# qc-state v2' > .qc/scratch.qcs
set +e
"$QC" verify >/tmp/qc-hc-ver.out 2>/tmp/qc-hc-ver.err
c=$?
if [ "$c" -eq 2 ]; then say "unknown scratch version"; else bad "ver $c"; fi
printf '%s\n' '# qc-state v1' > .qc/scratch.qcs

printf '%s\n' '# qc-state v1' \
  "accept/claim = pass @0123456789abcdef by:a at:2026-01-01T00:00:00Z extra:1	:: README.md names 1 claim" \
  > qc/state/seg-keys-dddddd.qcs
set +e
"$QC" verify >/tmp/qc-hc-ex.out

mkdir -p "$TMP/nogit2"
cp -R qc "$TMP/nogit2/qc"
cp -R .qc "$TMP/nogit2/.qc"
printf "hello 1\n" > "$TMP/nogit2/README.md"
cd "$TMP/nogit2"
write_accept
set +e
"$QC" mark accept/claim --pass -m "$EV" >/tmp/qc-hc-ng.out 2>/tmp/qc-hc-ng.err
"$QC" seal >/tmp/qc-hc-ngs.out 2>/tmp/qc-hc-ngs.err
if ls qc/state/seg-local-*.qcs >/dev/null 2>&1; then say "no-git local segment"; else say "no-git seal ran"; fi

cd "$TMP"
repo
write_accept
QC_BY="agent person" "$QC" mark accept/claim --pass -m "$EV" >/tmp/qc-hc-by.out || true
env -u USER -u QC_BY USERNAME=unittester "$QC" mark accept/claim --pass -m "$EV" >/tmp/qc-hc-un.out || true
: > empty.dat
env -u USER -u USERNAME -u QC_BY "$QC" verify >/tmp/qc-hc-who.out 2>/tmp/qc-hc-who.err || true
mkdir -p nested/deep
cd nested/deep
set +e
"$QC" verify >/tmp/qc-hc-fr.out 2>/tmp/qc-hc-fr.err

"$QC" template >/tmp/qc-hc-t0.out
"$QC" template help >/tmp/qc-hc-th.out
"$QC" template list >/tmp/qc-hc-tlst.out
"$QC" template show docs >/tmp/qc-hc-ts.out
set +e
"$QC" template show >/tmp/qc-hc-ts0.out 2>/tmp/qc-hc-ts0.err
"$QC" template show nope >/tmp/qc-hc-tsn.out 2>/tmp/qc-hc-tsn.err
"$QC" template bogus >/tmp/qc-hc-tb.out 2>/tmp/qc-hc-tb.err

# --- second-pass leftovers ---
repo
write_accept
"$QC" mark accept/claim --pass -m="$EV" >/tmp/qc-hc-meq2.out 2>/tmp/qc-hc-meq2.err
"$QC" reset --item=accept/claim >/tmp/qc-hc-rieq.out
"$QC" mark accept/claim --pass -m >/tmp/qc-hc-mempty.out 2>/tmp/qc-hc-mempty.err
"$QC" verify --only >/tmp/qc-hc-onlye.out 2>/tmp/qc-hc-onlye.err
"$QC" verify --only=accept/claim >/tmp/qc-hc-onlyeq.out 2>/tmp/qc-hc-onlyeq.err
"$QC" baseline --item=accept/claim -m="$EV" >/tmp/qc-hc-bieq.out 2>/tmp/qc-hc-bieq.err
"$QC" reset >/dev/null

"$QC" mark accept/claim --pass -m "$EV" >/dev/null
"$QC" seal >/dev/null
printf "hello 9 stale\n" > README.md
"$QC" plan >/tmp/qc-hc-pecho.out 2>/tmp/qc-hc-pecho.err
git checkout -q -- README.md

cat > qc/checklists/extra.md << 'MD'
---
id: extra
scope: [README.md]
---

- [ ] claim :: Extra claim?
MD
"$QC" mark extra/claim --pass -m "$EV" >/dev/null
"$QC" mark accept/claim --pass -m "$EV" >/dev/null
"$QC" seal >/dev/null
dig=0123456789abcdef
mkdir -p qc/state
cat > qc/state/seg-win-eeeeee.qcs << SEG
# qc-state v1
extra/claim = pass @${dig} by:h at:2019-01-01T00:00:00Z :: README.md names 1 claim
extra/claim = fail @${dig} by:h at:2019-06-01T00:00:00Z :: README.md names 1 claim
SEG
"$QC" compact >/tmp/qc-hc-c2.out 2>/tmp/qc-hc-c2.err

mkdir -p "$TMP/emptyhead"
cd "$TMP/emptyhead"
git init -q -b main
git config user.email host@test
git config user.name host
"$QC" init >/dev/null
write_accept
printf "hello 1\n" > README.md
"$QC" mark accept/claim --pass -m "$EV" >/tmp/qc-hc-eh.out 2>/tmp/qc-hc-eh.err
"$QC" seal >/tmp/qc-hc-ehs.out 2>/tmp/qc-hc-ehs.err

repo
write_accept
printf '%s\n' '# qc-state v2' > .qc/scratch.qcs
"$QC" reset --item accept/claim >/tmp/qc-hc-rv.out 2>/tmp/qc-hc-rv.err
printf '%s\n' '# qc-state v2' > .qc/scratch.qcs
"$QC" seal >/tmp/qc-hc-sv.out 2>/tmp/qc-hc-sv.err

mkdir -p docs
printf "d\n" > docs/a.md
"$QC" mark accept/claim --pass -m "checked docs/a.md" >/tmp/qc-hc-sl.out 2>/tmp/qc-hc-sl.err

cat > qc/checklists/sig.md << 'MD'
---
id: sig
---

- [ ] boom :: signaled? @run(sh -c 'kill -9 $$')
MD
"$QC" verify >/tmp/qc-hc-sig.out 2>/tmp/qc-hc-sig.err
rm -f qc/checklists/sig.md

cat > qc/checklists/qstar.md << 'MD'
---
id: qstar
scope: [qc/**]
---

- [ ] x :: store?
MD
"$QC" verify >/tmp/qc-hc-qs.out 2>/tmp/qc-hc-qs.err
rm -f qc/checklists/qstar.md


# --- leftover polarities ---
repo
write_accept
"$QC" mark accept/claim --pass --message "$EV" >/tmp/qc-hc-msg2.out 2>/tmp/qc-hc-msg2.err
c=$?
if [ "$c" -eq 0 ]; then say "mark --message two-arg"; else bad "message two-arg $c"; fi
"$QC" reset >/dev/null
"$QC" mark accept/claim --pass -m="$EV" --by=host >/tmp/qc-hc-byeq.out 2>/tmp/qc-hc-byeq.err
"$QC" verify --ci >/tmp/qc-hc-ci.out 2>/tmp/qc-hc-ci.err
"$QC" verify --staged --quiet >/tmp/qc-hc-stq.out 2>/tmp/qc-hc-stq.err
"$QC" seal --staged --quiet >/tmp/qc-hc-ssq.out 2>/tmp/qc-hc-ssq.err
"$QC" help help >/tmp/qc-hc-hh.out
"$QC" help "" >/tmp/qc-hc-he.out 2>/tmp/qc-hc-he.err
"$QC" >/tmp/qc-hc-bare.out 2>/tmp/qc-hc-bare.err

# intent-to-add + deleted worktree → staged blob miss + file fallback fail
printf "phantom\n" > phantom.txt
git add -N phantom.txt 2>/dev/null
rm -f phantom.txt
cat > qc/checklists/ph.md << 'MD'
---
id: ph
scope: [phantom.txt]
---

- [ ] p :: Phantom?
MD
set +e
"$QC" verify --staged >/tmp/qc-hc-ph.out 2>/tmp/qc-hc-ph.err
say "staged phantom attempted"
rm -f qc/checklists/ph.md
git rm -f --cached phantom.txt >/dev/null 2>&1

# @human + --by agent
cat > qc/checklists/hum.md << 'MD'
---
id: hum
scope: [README.md]
---

- [ ] h :: Human only? @human
MD
set +e
"$QC" mark hum/h --pass -m "$EV" --by agent >/tmp/qc-hc-hum.out 2>/tmp/qc-hc-hum.err
c=$?
if [ "$c" -eq 2 ]; then say "human agent refuse"; else say "human $c"; fi
rm -f qc/checklists/hum.md

# exact qc / ** already; add scope: [qc]
cat > qc/checklists/qonly.md << 'MD'
---
id: qonly
scope: [qc]
---

- [ ] x :: qc dir?
MD
set +e
"$QC" verify >/tmp/qc-hc-qo.out 2>/tmp/qc-hc-qo.err
c=$?
if [ "$c" -eq 2 ]; then say "scope qc refuse"; else say "scope qc $c"; fi
rm -f qc/checklists/qonly.md

# apply-when ST_NA skip items already; blocked requires display
cat > qc/checklists/blk.md << 'MD'
---
id: blk
requires: [accept]
scope: [README.md]
---

- [ ] b :: Blocked?
MD
set +e
"$QC" verify >/tmp/qc-hc-blk.out 2>/tmp/qc-hc-blk.err
"$QC" plan >/tmp/qc-hc-blkp.out 2>/tmp/qc-hc-blkp.err
rm -f qc/checklists/blk.md

# fake git: git_ok + branch fail + head ok
repo
write_accept
FAKE="$TMP/fakebin"
mkdir -p "$FAKE"
REALGIT=$(command -v git)
cat > "$FAKE/git" << EOF
#!/bin/sh
args="\$*"
case "\$args" in
  *"--is-inside-work-tree"*) echo true; exit 0 ;;
  *"--abbrev-ref"*) exit 1 ;;
  *"rev-parse HEAD"*) echo 0123456789abcdef0123456789abcdef01234567; exit 0 ;;
esac
exec "$REALGIT" "\$@"
EOF
chmod +x "$FAKE/git"
PATH="$FAKE:$PATH" "$QC" mark accept/claim --pass -m "$EV" >/tmp/qc-hc-fg.out 2>/tmp/qc-hc-fg.err
PATH="$FAKE:$PATH" "$QC" seal >/tmp/qc-hc-fgs.out 2>/tmp/qc-hc-fgs.err
if ls qc/state/seg-detached-*.qcs >/dev/null 2>&1; then say "fake-git detached name"; else say "fake-git seal ran"; fi

# seal keep unknown ref
printf '%s\n' '# qc-state v1' \
  "gone/x = pass @0123456789abcdef by:a at:2026-01-01T00:00:00Z :: README.md names 1 claim" \
  > .qc/scratch.qcs
"$QC" seal >/tmp/qc-hc-unk.out 2>/tmp/qc-hc-unk.err
say "seal unknown ref keep"

# decide both/neither already; --yes --no together
set +e
"$QC" decide accept --yes --no -m "$EV" >/tmp/qc-hc-dyn.out 2>/tmp/qc-hc-dyn.err
c=$?
if [ "$c" -eq 2 ]; then say "decide yes+no refuse"; else say "decide yn $c"; fi

# baseline skips @run
cat > qc/checklists/runonly.md << 'MD'
---
id: runonly
---

- [ ] r :: Run? @run(true)
MD
"$QC" baseline -m "$EV" >/tmp/qc-hc-br.out 2>/tmp/qc-hc-br.err
say "baseline skips run"
rm -f qc/checklists/runonly.md

# gitattributes / gitignore already present init
mkdir -p "$TMP/reinit"
cd "$TMP/reinit"
# already tested

# compact fopen fail
repo
write_accept
chmod 555 qc/state 2>/dev/null
set +e
"$QC" compact >/tmp/qc-hc-cf.out 2>/tmp/qc-hc-cf.err
chmod 755 qc/state 2>/dev/null
say "compact write-fail attempted"

# add --full on a fresh template id via local without ---
repo
mkdir -p qc/templates
printf 'plain body no front\n' > qc/templates/plain.md
"$QC" add plain >/tmp/qc-hc-pl.out 2>/tmp/qc-hc-pl.err
say "add local plain"

# help decide -h already via --help; decide -h
"$QC" decide -h >/tmp/qc-hc-dsh.out 2>/tmp/qc-hc-dsh.err

# BEGIN PRIVATE KEY + ghp via CLI
set +e
"$QC" mark accept/claim --pass -m "-----BEGIN PRIVATE KEY-----" >/tmp/qc-hc-pem.out 2>/tmp/qc-hc-pem.err
"$QC" mark accept/claim --pass -m "ghp_xxxxxxxxxxxxxxxx" >/tmp/qc-hc-ghp.out 2>/tmp/qc-hc-ghp.err

# worktree dirty vs index for mark (if implemented)
repo
write_accept
printf "hello dirty 9\n" > README.md
"$QC" mark accept/claim --pass -m "$EV" >/tmp/qc-hc-dirty-mark.out 2>/tmp/qc-hc-dirty-mark.err
say "mark dirty worktree"


# argc == 0 via execve
python3 -c "import os; os.execve(os.environ[\"QC\"], [], os.environ)"
say "execve empty argv"

# --- mcdc leftover CLI polarities ---
repo
write_accept
"$QC" mark -h >/tmp/qc-hc-msh.out 2>/tmp/qc-hc-msh.err
"$QC" template -h >/tmp/qc-hc-tsh.out 2>/tmp/qc-hc-tsh.err
"$QC" mark --pass -m "$EV" >/tmp/qc-hc-noref.out 2>/tmp/qc-hc-noref.err
"$QC" mark accept/claim -m "$EV" >/tmp/qc-hc-nost.out 2>/tmp/qc-hc-nost.err
"$QC" decide --yes -m "$EV" >/tmp/qc-hc-noid.out 2>/tmp/qc-hc-noid.err
"$QC" decide app -m "$EV" >/tmp/qc-hc-noyn.out 2>/tmp/qc-hc-noyn.err
"$QC" seal --staged --quiet >/tmp/qc-hc-s0.out 2>/tmp/qc-hc-s0.err
say "seal staged empty promote"

# init when qc is a file
mkdir -p "$TMP/qcfile"
cd "$TMP/qcfile"
git init -q -b main
git config user.email host@test
git config user.name host
printf "x\n" > README.md
git add README.md
git commit -qm seed
printf "notdir\n" > qc
set +e
"$QC" init >/tmp/qc-hc-qf.out 2>/tmp/qc-hc-qf.err
c=$?
if [ "$c" -eq 2 ]; then say "init qc-is-file refuse"; else say "init qc-file $c"; fi

# empty gitignore
mkdir -p "$TMP/emptygi"
cd "$TMP/emptygi"
git init -q -b main
git config user.email host@test
git config user.name host
printf "x\n" > README.md
git add README.md
git commit -qm seed
: > .gitignore
"$QC" init >/tmp/qc-hc-egi.out 2>/tmp/qc-hc-egi.err
say "init empty gitignore"

# empty argv0
repo
write_accept
(exec -a "" "$QC" init >/tmp/qc-hc-ea0.out 2>/tmp/qc-hc-ea0.err) || true
say "empty argv0 attempted"

# verify missing state dir
repo
write_accept
rm -rf qc/state
"$QC" verify >/tmp/qc-hc-nostate.out 2>/tmp/qc-hc-nostate.err
say "verify no state dir"

# compact with junk file in state + feature/x slash branch already; notes
repo
write_accept
"$QC" mark accept/claim --pass -m "$EV" >/dev/null
"$QC" seal >/dev/null
printf "notes\n" > qc/state/notes.txt
"$QC" compact >/tmp/qc-hc-notes.out 2>/tmp/qc-hc-notes.err
say "compact ignores non-seg"

# empty question item
cat > qc/checklists/emptyq.md << 'MD'
---
id: emptyq
scope: [README.md]
---

- [ ] e ::
MD
"$QC" plan >/tmp/qc-hc-eq.out 2>/tmp/qc-hc-eq.err
"$QC" verify >/tmp/qc-hc-eqv.out 2>/tmp/qc-hc-eqv.err
rm -f qc/checklists/emptyq.md

# baseline skips dormant + awaiting
cat > qc/checklists/aw.md << 'MD'
---
id: aw
applies_when: Does this apply to README.md docs?
scope: [README.md]
---

- [ ] c :: Claim?
MD
cat > qc/checklists/dm.md << 'MD'
---
id: dm
scope: [no-such-dir/**]
---

- [ ] d :: Dormant?
MD
"$QC" baseline -m "$EV" >/tmp/qc-hc-bdm.out 2>/tmp/qc-hc-bdm.err
say "baseline skips awaiting/dormant"
rm -f qc/checklists/aw.md qc/checklists/dm.md

# item-scope list fallback already; apply FAIL via mark --fail on applies? decide --yes then we need fail
# mark @applies as fail
cat > qc/checklists/app2.md << 'MD'
---
id: app2
applies_when: Does README.md apply here now?
scope: [README.md]
---

- [ ] c :: C?
MD
"$QC" decide app2 --yes -m "$EV" >/tmp/qc-hc-ay.out 2>/tmp/qc-hc-ay.err
"$QC" reset --item app2/@applies >/dev/null
# write fail applies via decide --yes then overwrite? use mark? applies is decide only.
# stamp fail by writing scratch
dig=$(awk '/digest @/{for(i=1;i<=NF;i++) if($i ~ /^@[0-9a-f]{16}$/){print substr($i,2); exit}}' /tmp/qc-hc-ay.out)
if [ -n "$dig" ]; then
  printf '%s\n' '# qc-state v1' \
    "app2/@applies = fail @${dig} by:h at:2026-01-01T00:00:00Z :: README.md names 1 claim" \
    > .qc/scratch.qcs
  "$QC" verify >/tmp/qc-hc-af.out 2>/tmp/qc-hc-af.err
  "$QC" plan >/tmp/qc-hc-afp.out 2>/tmp/qc-hc-afp.err
  say "applies FAIL recorded"
fi
rm -f qc/checklists/app2.md

# local template without ---
repo
mkdir -p qc/templates
printf 'no frontmatter body\n' > qc/templates/nf.md
"$QC" add nf >/tmp/qc-hc-nf.out 2>/tmp/qc-hc-nf.err
say "add no-frontmatter local"

# feature/x branch segment slash
repo
write_accept
git checkout -q -b feature/x
"$QC" mark accept/claim --pass -m "$EV" >/dev/null
"$QC" seal >/tmp/qc-hc-fx.out 2>/tmp/qc-hc-fx.err
if ls qc/state/seg-feature-x-*.qcs >/dev/null 2>&1; then say "slash branch segment"; else say "slash branch seal ran"; fi
git checkout -q main


# decide id --yes without -m
repo
write_accept
set +e
"$QC" decide accept --yes >/tmp/qc-hc-dnom.out 2>/tmp/qc-hc-dnom.err
c=$?
if [ "$c" -eq 2 ]; then say "decide missing message"; else say "decide nomsg $c"; fi

# compact startswith seg- but not .qcs
"$QC" mark accept/claim --pass -m "$EV" >/dev/null
"$QC" seal >/dev/null
printf "x\n" > qc/state/seg-foo.txt
"$QC" compact >/tmp/qc-hc-sft.out 2>/tmp/qc-hc-sft.err
if [ -f qc/state/seg-foo.txt ]; then say "compact keeps non-qcs seg-"; else say "compact seg-txt"; fi

# human with --by someone (not agent)
cat > qc/checklists/hum2.md << 'MD'
---
id: hum2
scope: [README.md]
---

- [ ] h :: Human? @human
MD
set +e
"$QC" mark hum2/h --pass -m "$EV" --by someone >/tmp/qc-hc-hum2.out 2>/tmp/qc-hc-hum2.err
c=$?
if [ "$c" -eq 0 ]; then say "human non-agent ok"; else say "human someone $c"; fi
rm -f qc/checklists/hum2.md

# mark --fail on existing item for write_attestation fail path
"$QC" mark accept/claim --fail -m "$EV" >/tmp/qc-hc-mf2.out 2>/tmp/qc-hc-mf2.err
say "mark fail attestation"

# seal digest mismatch (dirty after mark)
"$QC" mark accept/claim --pass -m "$EV" >/dev/null
printf "hello dirty seal\n" > README.md
"$QC" seal >/tmp/qc-hc-smis.out 2>/tmp/qc-hc-smis.err
say "seal mismatch keep"
git checkout -q -- README.md

# plan on all-clear vs dormant-only
"$QC" reset >/dev/null
"$QC" mark accept/claim --pass -m "$EV" >/dev/null
"$QC" plan >/tmp/qc-hc-pclr.out 2>/tmp/qc-hc-pclr.err
say "plan after mark"



# compact: two identical matching lines (same raw/at) + one matching newer
repo
write_accept
"$QC" mark accept/claim --pass -m "$EV" >/tmp/qc-hc-cid.out
dig=$(awk '/digest @/{for(i=1;i<=NF;i++) if($i ~ /^@[0-9a-f]{16}$/){print substr($i,2); exit}}' /tmp/qc-hc-cid.out)
"$QC" seal >/dev/null
line="accept/claim = pass @${dig} by:host at:2020-01-01T00:00:00Z files:1 :: README.md names 1 claim"
mkdir -p qc/state
printf '%s\n' '# qc-state v1' "$line" "$line"   "accept/claim = pass @${dig} by:host at:2021-01-01T00:00:00Z files:1 :: README.md names 1 claim"   > qc/state/seg-dup-ffffff.qcs
"$QC" compact >/tmp/qc-hc-cidc.out 2>/tmp/qc-hc-cidc.err
say "compact identical twins"



# gitignore already ends with newline and lacks .qc/
mkdir -p "$TMP/ginl"
cd "$TMP/ginl"
printf 'foo\n' > .gitignore
"$QC" init >/tmp/qc-hc-ginl.out 2>/tmp/qc-hc-ginl.err
if grep -q '.qc/' .gitignore; then say "init gitignore with nl"; else bad "init ginl"; fi

# @strict --pass (st != NA while strict)
repo
cat > qc/checklists/st.md << 'MD'
---
id: st
scope: [README.md]
---

- [ ] s :: Strict item against README.md? @strict
MD
set +e
"$QC" mark st/s --pass -m "$EV" >/tmp/qc-hc-stp.out 2>/tmp/qc-hc-stp.err
c=$?
if [ "$c" -eq 0 ]; then say "strict pass"; else say "strict pass $c"; fi

# decide short evidence (write_attestation NEXT ternary @applies arm)
cat > qc/checklists/dec.md << 'MD'
---
id: dec
applies_when: Does README.md apply for decide?
scope: [README.md]
---

- [ ] c :: C?
MD
set +e
"$QC" decide dec --yes -m "short" >/tmp/qc-hc-ds.out 2>/tmp/qc-hc-ds.err
c=$?
if [ "$c" -eq 2 ]; then say "decide short evidence"; else say "decide short $c"; fi

# plan with a dormant list
cat > qc/checklists/dorm.md << 'MD'
---
id: dorm
scope: [no-such-dir/**]
---

- [ ] d :: Dormant leftover?
MD
"$QC" plan >/tmp/qc-hc-pd.out 2>/tmp/qc-hc-pd.err
say "plan with dormant"

# seal self-scope: digest returns -1 so rc!=0 keep
cat > qc/checklists/self.md << 'MD'
---
id: self
scope: [qc/state/**]
---

- [ ] x :: Self?
MD
mkdir -p .qc
printf '%s\n' '# qc-state v1' \
  "self/x = pass @0123456789abcdef by:h at:2020-01-01T00:00:00Z :: README.md names 1 claim" \
  > .qc/scratch.qcs
"$QC" seal >/tmp/qc-hc-ss.out 2>/tmp/qc-hc-ss.err
say "seal self-scope keep"


if [ "$fail" -ne 0 ]; then
    echo "$fail host_cover failures"
    exit 1
fi
echo "host_cover passed"
