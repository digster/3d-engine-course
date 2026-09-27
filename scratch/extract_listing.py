#!/usr/bin/env python3
"""Recover a listing's lesson-era text FROM the shipped page that published it.

Last resort, and sometimes the only resort.  `scratch/` is gitignored, so a
listing that points into it - a verify_NN.cpp harness, a one-off generator -
has no history at all.  When later lessons edit that file, its lesson-era text
survives in exactly one place: the rendered page, which embeds every listing
whole and escaped because CLAUDE.md §8 forbids placeholders.  That makes a
published page a lossless archive of its own sources.

Extraction is the inverse of the builder's esc(), applied to the <pre><code>
block whose figcaption names the path.  The result is then checked by
re-escaping and searching the page, so a mis-parse cannot pass silently.

    python3 scratch/extract_listing.py 54 scratch/verify_54.cpp \
        --out scratch/l54_scratch_verify_54.cpp
"""
import argparse
import re
import sys


def esc(text):
    return (text.replace("&", "&amp;")
                .replace("<", "&lt;")
                .replace(">", "&gt;")
                .replace('"', "&quot;")
                .replace("'", "&#x27;"))


def unesc(text):
    """The EXACT inverse of esc(), which is not what html.unescape() is.

    esc() maps five characters. html.unescape() decodes the whole HTML5 entity
    table - including sequences esc() never emits, and legacy entities that need
    no semicolon - so round-tripping through it can alter bytes the escaper never
    touched. Lesson 6.6's gltf.hpp is a live example; the round-trip assertion
    below is what caught it.

    `&amp;` is undone LAST, mirroring esc() doing it first: otherwise a source
    that literally contained `&lt;` would come back as `<`.
    """
    return (text.replace("&#x27;", "'")
                .replace("&quot;", '"')
                .replace("&gt;", ">")
                .replace("&lt;", "<")
                .replace("&amp;", "&"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("n")
    ap.add_argument("path", help="the listed path, exactly as the figcaption spells it")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    src = open(f"scratch/build_{args.n}.py", encoding="utf-8").read()
    page_path = re.search(r'^OUT\s*=\s*["\']([^"\']+)["\']', src, re.M).group(1)
    page = open(page_path, encoding="utf-8").read()

    # The listing figure: a figcaption naming the path, then the <pre><code>.
    # Non-greedy up to the first </code>, anchored on this path's caption, so a
    # page listing several files cannot hand back the wrong one.
    pattern = (r'<span class="path">' + re.escape(args.path) + r'</span>.*?'
               r'<pre><code[^>]*>(.*?)</code></pre>')
    matches = re.findall(pattern, page, re.S)
    if not matches:
        sys.exit(f"!! no listing for {args.path} in {page_path}")
    if len(matches) > 1:
        sys.exit(f"!! {len(matches)} listings for {args.path}; disambiguate by hand")

    body = unesc(matches[0])
    # Round-trip: re-escaping must reproduce what the page holds, or the parse
    # took the wrong bytes and the pin would be quietly wrong.
    if esc(body) != matches[0]:
        sys.exit("!! round-trip failed - the extraction is not faithful")

    with open(args.out, "w", encoding="utf-8") as fh:
        fh.write(body)
    print(f"recovered {len(body):,} bytes of {args.path} from {page_path} -> {args.out}")


if __name__ == "__main__":
    main()
