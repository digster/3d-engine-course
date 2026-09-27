#!/usr/bin/env python3
"""Decide, per listed file, WHICH commit's version a shipped page actually published.

A lesson page embeds every listing in full, HTML-escaped by the builder's esc().
So the question "does this page show the code as it stood at commit X?" is not a
judgement call - it is a substring test.  Escape commit X's blob the way the
builder would and look for it in the page.

This matters because several pages were silently rewritten by the NEXT lesson's
commit: build_NN.py read its listings live, the next lesson's session re-ran it to
retrofit a nav link, and the newer code went out under the older lesson's number.
Pinning such a page to its own shipping commit therefore does NOT reproduce it,
and guessing which commit to pin instead is how a repair goes subtly wrong.

    python3 scratch/which_commit.py 52 5175d70 ea7a05f
"""
import re
import subprocess
import sys


def esc(text):
    """The escaping every builder applies on the way into a <pre><code> block."""
    return (text.replace("&", "&amp;")
                .replace("<", "&lt;")
                .replace(">", "&gt;")
                .replace('"', "&quot;")
                .replace("'", "&#x27;"))


def main():
    n, commits = sys.argv[1], sys.argv[2:]
    src = open(f"scratch/build_{n}.py", encoding="utf-8").read()
    page_path = re.search(r'^OUT\s*=\s*["\']([^"\']+)["\']', src, re.M).group(1)
    page = open(page_path, encoding="utf-8").read()
    meta = re.search(r"LISTING_META\s*=\s*\{(.*?)\n\}", src, re.S).group(1)
    paths = re.findall(r'["\']([^"\']+)["\']\s*:', meta)

    width = max(len(p) for p in paths)
    print(f"{'path':<{width}}  " + "  ".join(f"{c:<9}" for c in commits))
    verdict = {}
    for path in paths:
        marks = []
        for commit in commits:
            blob = subprocess.run(["git", "show", f"{commit}:{path}"],
                                  capture_output=True, check=False)
            if blob.returncode != 0:
                marks.append("absent")
                continue
            body = blob.stdout.decode("utf-8")
            marks.append("MATCH" if esc(body) in page else "-")
        print(f"{path:<{width}}  " + "  ".join(f"{m:<9}" for m in marks))
        hits = [c for c, m in zip(commits, marks) if m == "MATCH"]
        verdict[path] = hits

    print()
    unmatched = [p for p, h in verdict.items() if not h]
    if unmatched:
        print(f"!! {len(unmatched)} listing(s) match NO candidate commit "
              f"- the page shows something else again:")
        for p in unmatched:
            print(f"     {p}")
    else:
        print("every listing matches at least one candidate")


if __name__ == "__main__":
    main()
