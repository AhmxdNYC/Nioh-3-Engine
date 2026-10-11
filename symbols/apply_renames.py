"""Apply *-renames.tsv files IN PLACE to symbols.tsv (merge_incoming.py keeps existing names, so renames need this).

Usage: py -I apply_renames.py incoming/<x>-renames.tsv [more ...]
Each renames row overwrites the non-empty columns of the row with the same RVA; the old name is kept in the
description as '(was <old>)'. A row whose kind is DELETE removes the row with that RVA (bogus rows, e.g. a sample
address inside a function; put the real name on the function start in another row). Applied files are moved to
incoming/merged/<stamp>_renames_<name>.tsv.
"""
import datetime
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def main(paths):
    path = os.path.join(HERE, "symbols.tsv")
    lines = open(path, encoding="utf-8").read().split("\n")
    pre = [l for l in lines if l.startswith("#")]
    body = [l for l in lines if l and not l.startswith("#")]
    cols, data = body[0].split("\t"), [l.split("\t") for l in body[1:]]
    index = {r[0].lower(): r for r in data}
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    for p in paths:
        new = {}
        for l in open(p, encoding="utf-8"):
            if not l.strip() or l.startswith("#") or l.startswith("rva\t"):
                continue
            q = l.rstrip("\n").split("\t")
            new[q[0].lower()] = q
        done = 0
        for k, q in new.items():
            r = index.get(k)
            if len(q) > 1 and q[1] == "DELETE":
                if r is not None and r[1] != "struct_field":
                    data.remove(r)
                    del index[k]
                    done += 1
                continue
            if r is None:  # not in the DB yet: add it as a new row
                r = (q + [''] * len(cols))[: len(cols)]
                data.append(r)
                index[k] = r
                done += 1
                continue
            old = r[2]
            for i in range(1, min(len(q), len(r))):
                if q[i]:
                    r[i] = q[i]
            if old != r[2] and old not in r[4]:
                r[4] += " (was " + old + ")"
            done += 1
        print(f"{os.path.basename(p)}: renamed {done} of {len(new)} (missing RVAs: {len(new) - done})")
        dst = os.path.join(HERE, "incoming", "merged", f"{stamp}_renames_{os.path.basename(p)}")
        shutil.move(p, dst)
    open(path, "w", encoding="utf-8", newline="\n").write("\n".join(pre + ["\t".join(cols)] + ["\t".join(r) for r in data]) + "\n")


if __name__ == "__main__":
    main(sys.argv[1:])
