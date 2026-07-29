#!/usr/bin/env python3
"""Audit VO coverage: list tutorial guide strings that have no matching voice clip.

Run after editing tutorial dialogue. Lines it flags either need a new entry in
Tools/vo_manifest.json (then generate_vo.py + --emit-cpp) or an updated "match"
field if only the in-game wording changed. UI labels / debug strings in the
output can be ignored.
"""

import json, io, re, glob

m = json.load(io.open("Tools/vo_manifest.json", encoding="utf-8"))
matches = {l.get("match", l["text"]) for l in m["lines"] if l.get("match", l["text"])}

pat = re.compile(r'TEXT\("((?:[^"\\]|\\.)+)"\)')
missing = {}
for f in glob.glob("Source/ChessKids/Private/*Tutorial*.cpp"):
    src = io.open(f, encoding="utf-8").read()
    for mt in pat.finditer(src):
        s = mt.group(1)
        if "%" in s or len(s) < 15:
            continue
        if "/Game/" in s or "/Engine/" in s or "Material" in s or "Cube" in s:
            continue
        cs = s.replace('\\"', '"')
        if cs not in matches:
            missing.setdefault(f.replace("\\", "/").split("/")[-1], []).append(cs)
total = 0
for f, lines in sorted(missing.items()):
    print("==", f, f"({len(lines)})")
    for s in lines:
        print("  ", repr(s))
        total += 1
print("TOTAL MISSING:", total)
