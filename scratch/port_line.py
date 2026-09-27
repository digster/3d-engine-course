#!/usr/bin/env python3
"""Port a correction from a shipped page back into the source that produced it.

The rebuild diff is the oracle: each line it reports is a fix that was applied to
the page on disk and never to the fragment, pin or builder it came from, so the
next rebuild reverts it.  This routes one such fix to the right source.

Routing matters because a page line has two possible origins with two different
spellings:

  * a body fragment (scratch/lNN_body_*.html) contributes its text VERBATIM;
  * a pinned code listing goes through the builder's esc(), so `'` on the page is
    `&#x27;` and `<` is `&lt;`.

So we look for the literal text first and for the HTML-unescaped text second.
Nothing is written unless a match is found in exactly the files we expect; a
substitution that matches nowhere is an error, not a no-op, because a silent
no-op is indistinguishable from a repair that worked.

    python3 scratch/port_line.py 38 --old '<old page text>' --new '<new page text>'
    python3 scratch/port_line.py 38 --pairs pairs.tsv     # one old<TAB>new per line
"""
import argparse
import glob
import os
import sys


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


def candidates(n):
    """Every source a builder assembles a page out of - and their own sources.

    THE .svg FILES AND figs_NN.py BOTH BELONG HERE, and leaving either out is the
    same bug one layer down. A diagram label lives in the rendered `lNN_figK.svg`
    AND in the generator that computes it, so porting a correction into only the
    SVG leaves figs_NN.py able to revert it the next time the figures are
    regenerated. (Binary artefacts are excluded; SVG is text and is precisely
    where diagram labels live.)
    """
    files = (sorted(glob.glob(f"scratch/l{n}_*"))
             + [f"scratch/build_{n}.py", f"scratch/figs_{n}.py"])
    return [f for f in files
            if os.path.exists(f) and not f.endswith((".png", ".ppm", ".jpg"))]


def fragments(old, new, min_len=10):
    """Candidate differing-middles of two strings, widest context first.

    A page line and the code that GENERATES it share no whole line: figs_NN.py
    holds `label(24, 322, "stay at four past Module 7.", ...)` while the page
    holds `<text ...>stay at four past Module 7.</text>`. What they share is the
    text itself, so strip the common prefix and suffix to find what changed.

    Context is the safety - the bare difference is `7` -> `8`, which would match
    a hundred harmless places, while `stay at four past Module 7.` matches one.
    But too MUCH context reaches back into the markup the generator does not
    contain, so several widths are offered and the caller takes the first that
    lands. Fragments too short to be trusted are never offered at all.
    """
    p = 0
    while p < min(len(old), len(new)) and old[p] == new[p]:
        p += 1
    s = 0
    while (s < min(len(old), len(new)) - p
           and old[len(old) - 1 - s] == new[len(new) - 1 - s]):
        s += 1

    # EITHER SIDE must be allowed to reach ZERO, and both cases are real:
    #
    #   trail = 0  - the common suffix of an SVG label is `.</text>`, pure markup
    #                that figs_NN.py does not contain, so any variant keeping part
    #                of it can never match however much LEADING context it has.
    #   lead  = 0  - a builder wraps a long figure caption across several Python
    #                string literals, and the changed character can land as the
    #                FIRST character of one of them (6.6's "Module " / "8&#8217;s"
    #                is exactly this), so any variant with leading context spans a
    #                source line break and cannot match either.
    #
    # Widest first; the caller takes the first that lands.
    widths = [(60, 60), (60, 0), (40, 40), (40, 0), (24, 24), (24, 0),
              (12, 12), (12, 0), (0, 60), (0, 40), (0, 24), (0, 12)]
    out = []
    for lead, trail in widths:
        lo = max(0, p - lead)
        hi = len(old) - s + min(trail, s)
        frag_old = old[lo:hi]
        frag_new = new[lo:len(new) - s + min(trail, s)]
        if len(frag_old.strip()) >= min_len and (frag_old, frag_new) not in out:
            out.append((frag_old, frag_new))
    return out


def apply_pair(n, old, new, quiet=False):
    """Port one correction, trying progressively looser forms until one lands.

    The forms exist because a page line can come from four kinds of source:

      literal    - a body fragment, or the builder's own HEAD/TAIL: verbatim.
      unescaped  - a pinned code listing, which the builder puts through esc().
      stripped   - an inline SVG, which the builder re-indents on the way in.
      fragment   - a figure GENERATOR, which holds the text but not the line.

    Looser forms are tried ONLY when the tighter ones matched nothing, so a
    correction is never applied twice through two different spellings.
    """
    hits = 0
    touched = set()
    forms = [("literal", old, new)]
    if unesc(old) != old:
        forms.append(("unescaped", unesc(old), unesc(new)))
    if old.strip() != old:
        forms.append(("stripped", old.strip(), new.strip()))

    for form_name, o, w in forms:
        for path in candidates(n):
            try:
                text = open(path, encoding="utf-8").read()
            except (UnicodeDecodeError, IsADirectoryError):
                continue
            if o not in text:
                continue
            count = text.count(o)
            open(path, "w", encoding="utf-8").write(text.replace(o, w))
            hits += count
            touched.add(path)
            if not quiet:
                print(f"  {path}: {count} x ({form_name})")
        if hits:
            break

    # Whether or not a line-shaped form landed, the generator behind a figure
    # still needs the correction - the SVG and figs_NN.py both hold it, and
    # fixing only the SVG leaves the generator able to revert it.
    # build_NN.py IS included here. Figure captions live in its FIGURES dict as
    # Python string literals wrapped across several source lines, so a caption
    # correction exists as a whole line in no source at all and only the fragment
    # form can reach it. Files the literal pass already fixed are skipped, so a
    # correction is never applied twice through two spellings.
    for path in candidates(n):
        if not path.endswith(".py") or path in touched:
            continue
        try:
            text = open(path, encoding="utf-8").read()
        except UnicodeDecodeError:
            continue
        for frag_old, frag_new in fragments(old.strip(), new.strip()):
            if frag_old not in text:
                continue
            count = text.count(frag_old)
            open(path, "w", encoding="utf-8").write(text.replace(frag_old, frag_new))
            hits += count
            if not quiet:
                print(f"  {path}: {count} x (fragment, ctx={len(frag_old)})")
            break   # widest fragment that matched wins; do not apply a narrower one too

    if hits == 0:
        print(f"  !! NO MATCH for: {old[:90]}", file=sys.stderr)
    return hits


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("n")
    ap.add_argument("--old")
    ap.add_argument("--new")
    ap.add_argument("--pairs", help="TSV of old<TAB>new, one correction per line")
    args = ap.parse_args()

    pairs = []
    if args.pairs:
        for line in open(args.pairs, encoding="utf-8"):
            line = line.rstrip("\n")
            # No comment convention here, deliberately. The file is machine-written
            # by autoport.py and a correction's text may legitimately BEGIN with a
            # '#' - two of Lesson 5.1's are CMake comment lines - so treating '#'
            # as a comment silently dropped real work while still reporting success.
            if not line:
                continue
            old, _, new = line.partition("\t")
            pairs.append((old, new))
    else:
        pairs.append((args.old, args.new))

    total = sum(apply_pair(args.n, old, new) for old, new in pairs)
    print(f"  = {total} substitution(s) across {len(pairs)} correction(s)")
    return 0 if total else 1


if __name__ == "__main__":
    sys.exit(main())
