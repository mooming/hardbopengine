#!/usr/bin/env python3
"""Merge independent judgement-review passes into one deduplicated finding set.

Two passes that read the same files under the same twelve rules are not a superset
of one another: a pass sliced per header/source pair sees the class as an API and
misses what a pass sliced by line budget sees, and the other way round. The union
is therefore the honest result, and a finding cited by both passes is the strongest
signal this pipeline produces, so it is marked rather than merely counted twice.

Rule names are free text from a model, so they are folded onto the rule number they
refer to before deduplication; "log before failing" and "log-before-failing" must not
survive as two findings. Rule text that maps to nothing is kept and reported as
"unmapped:" rather than dropped, because an unmapped name is either a finding worth
reading or a hole in this table, and both deserve to be seen.

Track names come from the filename prefix before its digits, so batch07.json belongs
to track "batch" and pack03.json to track "pack". A file whose name has no digits is
skipped, which is what keeps merged.json from merging itself on the next run.

Usage: review_merge.py <dir>   (dir holds the pass JSON files; merged.json is written there)
Prints markdown tables: per group, by rule, by file, and the confidence histogram.
"""

import glob
import json
import os
import sys
from collections import Counter, defaultdict

RULES = [
    ("log", "rule 1 log-before-failing"),
    ("nodiscard", "rule 2 nodiscard missing"),
    ("explicit", "rule 3 explicit ctor missing"),
    ("default", "rule 4 = default for trivial member"),
    ("parameter", "rule 5 parameter naming"),
    ("pointer", "rule 6 pointer not validated"),
    ("dereference", "rule 6 pointer not validated"),
    ("static", "rule 7 static_assert missing"),
    ("magic", "rule 8 magic number"),
    ("constant", "rule 8 magic number"),
    ("final", "rule 9 class not final"),
    ("inherit", "rule 9 class not final"),
    ("noexcept", "rule 10 noexcept questionable"),
    ("prefix", "rule 11 naming"),
    ("hungarian", "rule 11 naming"),
    ("pascalcase", "rule 11 naming"),
    ("camelcase", "rule 11 naming"),
    ("unit_test", "rule 12 unit-test guard placement"),
]

CONFIDENCE = {"high": 0, "medium": 1, "low": 2}


def rule_id(finding):
    text = (finding.get("rule") or "").lower()
    for needle, canonical in RULES:
        if needle in text:
            return canonical
    return "unmapped: " + text


def confidence_of(finding):
    value = (finding.get("confidence") or "").lower()
    return value if value in CONFIDENCE else "absent"


def track_of(name):
    stem = os.path.splitext(name)[0]
    digits = "".join(c for c in stem if c.isdigit())
    if not digits:
        return None
    return stem.rstrip(digits).rstrip("-_") or stem


def load(directory):
    findings = []
    groups = {}
    for path in sorted(glob.glob(os.path.join(directory, "*.json"))):
        name = os.path.basename(path)
        if name in ("args.json", "merged.json"):
            continue
        track = track_of(name)
        if track is None:
            continue
        with open(path) as handle:
            data = json.load(handle)
        items = data.get("findings") or []
        groups[(track, str(data.get("label", name)))] = (len(items), data.get("filesRead"))
        for item in items:
            item["_track"] = track
            findings.append(item)
    return findings, groups


def merge(findings):
    buckets = defaultdict(list)
    for item in findings:
        buckets[(item["file"], int(item["line"]), rule_id(item))].append(item)
    merged = []
    for (path, line, rule), hits in buckets.items():
        best = min(hits, key=lambda h: (CONFIDENCE.get(confidence_of(h), 3), len(h.get("why") or "")))
        merged.append({
            "file": path,
            "line": line,
            "rule": rule,
            "evidence": best.get("evidence"),
            "why": best.get("why"),
            "fix": best.get("fix"),
            "confidence": confidence_of(best),
            "tracks": sorted({h["_track"] for h in hits}),
            "corroborated": len({h["_track"] for h in hits}) > 1,
        })
    merged.sort(key=lambda m: (CONFIDENCE.get(m["confidence"], 3), m["file"], m["line"]))
    return merged


def main():
    if len(sys.argv) != 2:
        sys.stderr.write(__doc__)
        return 2
    directory = sys.argv[1]
    if not os.path.isdir(directory):
        sys.stderr.write("review_merge.py: %s is not a directory\n" % directory)
        return 3
    findings, groups = load(directory)
    if not findings:
        sys.stderr.write("review_merge.py: no findings found in %s. An empty review directory is not a "
                         "clean module: check the passes ran and named their files with a number.\n" % directory)
        return 3
    merged = merge(findings)
    with open(os.path.join(directory, "merged.json"), "w") as handle:
        json.dump(merged, handle, indent=1)

    print("input findings %d -> unique %d (corroborated by more than one track: %d)"
          % (len(findings), len(merged), sum(1 for m in merged if m["corroborated"])))
    print("\n### Per group\n\n| Track | Group | Findings | Files read |")
    print("|---|---|---|---|")
    for (track, label), (count, read) in sorted(groups.items()):
        print("| %s | %s | %d | %s |" % (track, label, count, read))

    print("\n### By rule\n\n| Rule | Findings | Corroborated |")
    print("|---|---|---|")
    by_rule = defaultdict(lambda: [0, 0])
    for m in merged:
        by_rule[m["rule"]][0] += 1
        by_rule[m["rule"]][1] += 1 if m["corroborated"] else 0
    for rule, (count, both) in sorted(by_rule.items(), key=lambda kv: -kv[1][0]):
        print("| %s | %d | %d |" % (rule, count, both))

    print("\n### By file\n\n| File | Findings | High | Corroborated |")
    print("|---|---|---|---|")
    by_file = defaultdict(lambda: [0, 0, 0])
    for m in merged:
        row = by_file[m["file"].split("/")[-1]]
        row[0] += 1
        row[1] += 1 if m["confidence"] == "high" else 0
        row[2] += 1 if m["corroborated"] else 0
    for name, (count, high, both) in sorted(by_file.items(), key=lambda kv: (-kv[1][0], kv[0])):
        print("| %s | %d | %d | %d |" % (name, count, high, both))

    print("\n### Confidence\n")
    print("| " + " | ".join(["high", "medium", "low", "field omitted"]) + " |")
    counts = Counter(m["confidence"] for m in merged)
    print("| " + " | ".join(str(counts.get(k, 0)) for k in ("high", "medium", "low", "absent")) + " |")
    unmapped = sorted({m["rule"] for m in merged if m["rule"].startswith("unmapped:")})
    if unmapped:
        print("\nRule names this table does not fold: " + ", ".join(unmapped))
    return 0


if __name__ == "__main__":
    sys.exit(main())
