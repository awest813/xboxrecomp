"""Insert cooperative single-CPU safe points without altering guest operations.

Also usable on existing generated chunks, avoiding another disassembly/lift
when only scheduler instrumentation changes. Reapplying is idempotent.
"""
import argparse
from pathlib import Path
import re

HINT = "    RECOMP_PREEMPT(); /* guest scheduler */"


def instrument(code):
    code = "\n".join(line for line in code.splitlines() if line != HINT)
    lines = code.splitlines()
    # Preserve line numbers while excluding commented-out dead branches.
    stripped = re.sub(r"/\*.*?\*/", lambda m: "\n" * m[0].count("\n"), code, flags=re.S)
    clean = stripped.splitlines()
    labels = {}
    for i, line in enumerate(clean):
        m = re.match(r"^(loc_[0-9A-Fa-f]+):", line)
        if m:
            labels[m[1]] = i
    backedges = set()
    for i, line in enumerate(clean):
        for target in re.findall(r"\bgoto (loc_[0-9A-Fa-f]+);", line):
            if target in labels and labels[target] <= i:
                backedges.add(labels[target])
    result = []
    for i, line in enumerate(lines):
        result.append(line)
        entry = line == "{" and i and re.match(r"^void \w+\(void\)$", lines[i-1])
        if entry or i in backedges:
            result.append(HINT)
    return "\n".join(result) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("gen_dir", type=Path)
    args = parser.parse_args()
    changed = points = 0
    for path in sorted(args.gen_dir.glob("recomp_[0-9][0-9][0-9][0-9].c")):
        old = path.read_text()
        new = instrument(old)
        points += new.count(HINT)
        if new != old:
            path.write_text(new)
            changed += 1
    print(f"Scheduler safe points: {points}; updated chunks: {changed}")


if __name__ == "__main__":
    main()
