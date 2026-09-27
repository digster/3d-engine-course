#!/usr/bin/env python3
"""Assemble docs/lessons/08-08-broadphase.html.

Same pipeline as build_71.py through build_87.py. No STATE block: STATE.md is
the sole resume key and lesson pages end at Further Reading (CLAUDE.md §9).

Prose from scratch/l88_body_{a..f}.html, figures from scratch/figs_88.py, and
every code listing PINNED at writing time — see LISTING_SOURCE.

WHY THE PINS MATTER PARTICULARLY HERE. Nine listings, and four of them are files
that 8.9 onward will keep editing. `collide.hpp` gains a test whenever a shape
pair is added; both CMakeLists gain a line per lesson; and `broadphase.hpp`
itself carries a `margin` that defaults to zero and that 8.9 exists to turn on,
plus a `keep_slop`-shaped hole in the stats that a speculative contact will want.
A builder that opened a repository path would render this page's listings as they
stand THEN rather than as they stood when the prose was written about them, and
§8.2's claim that `hash_cell` is three multiplies and two exclusive-ors is a
claim about a specific function.

FOUR LISTINGS ARE FILES EARLIER PAGES ALSO PRINTED, and they disagree with those
pages on purpose. `collide.hpp` in 8.4-8.6 has no `overlaps(aabb, aabb)`;
`engine.hpp` and both CMakeLists are shorter by a line each. Each page is an
archive of its own era and the disagreement is the edit being recorded.

AND THREE FILES CHANGED THIS LESSON ARE DELIBERATELY NOT LISTED. shape.hpp,
rigid_body.hpp and rigid_body.cpp each carried a doc comment reading "8.6's
broadphase", written when the broadphase was expected to land in 8.6. All four
occurrences now read "8.8's". The three files are 2,458 lines between them, the
change is one word in each, and §14 carries a callout saying so rather than
passing over it. That is the only unlisted change in this course and it is named
in the page, not only here.

EVERY NUMBER IN THE PROSE IS A MEASUREMENT FROM THIS MACHINE, and
scratch/verify_88.log is the canonical run for all of them BUT TWO. The two are
§7's hash timings — 0.115 ns inlined against 0.692 behind a call — which were
measured after `hash_out_of_line` was added to the harness, so the log predates
the instrument that produces them. Every other transcript in the page is from
that one run, and each <pre> block is internally from a single run; none of them
mixes. Re-running the harness gives the same conclusions and different timings
(the ratio moves between about 360x and 410x, the call overhead between 5.7x and
6.0x), which is why the DETERMINISTIC counts — entries, bucket tests, pair
counts, the 3271 — are what the arguments rest on. The timings that do appear are
themselves minima over many repetitions; see the harness's own header for why the
median was the wrong estimator for a build that takes a few hundred
microseconds.

WHEN COPYING THIS FILE, EMPTY LISTING_SOURCE AS WELL AS FIGURES — build_66.py
inherited 6.5's three pins verbatim and they sat inert for a whole lesson. Here
the pins are generated from LISTING_META by `_pin`, so a path added to
LISTING_META without a pin file beside it fails loudly at build time.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/08-08-broadphase.html"

FIGURES = {
    "FIG_WALL": ("l88_fig1.svg", "1",
        "<strong>The quadratic does not arrive later; it is already here.</strong> Frame cost "
        "against scene size, log-log, for three strategies. The red line is every pair handed to "
        "the narrow phase at its measured <strong>432.4&nbsp;ns</strong> &mdash; it crosses one "
        "60&nbsp;Hz frame at <strong>n&nbsp;=&nbsp;278</strong>, which is a small pile of crates. "
        "The blue line substitutes the six-comparison AABB test at <strong>1.190&nbsp;ns</strong>, "
        "<strong>363&times;</strong> cheaper, and moves the wall to n&nbsp;=&nbsp;5,293 without "
        "removing it: both are the same quadratic with different constants, straight lines of slope "
        "2 with no flat part anywhere. Only the green line changes shape, and it is the only one "
        "that is not testing every pair. The table is the measured comparison from &sect;2, with "
        "the two pair sets checked <em>equal</em> at every scene size &mdash; the speedup is "
        "<strong>17.4&times;</strong> at 512 proxies and <strong>137.7&times;</strong> at 8,192."),

    "FIG_CONTRACT": ("l88_fig3.svg", "2",
        "<strong>The two errors a broadphase can make are not symmetric, and that asymmetry is the "
        "entire design.</strong> The broadphase's output must contain every pair that really "
        "touches and may contain as many others as it likes. A false positive costs exactly one "
        "narrow-phase call &mdash; 432&nbsp;ns, and the answer that comes back is correct, so it "
        "appears in a profile and never in a bug report. A false negative has no stage downstream "
        "that could notice it: the narrow phase is never called, no manifold is generated, the "
        "solver has nothing to solve, and the symptom reaches the player as &ldquo;sometimes things "
        "fall through the floor&rdquo; with no line of code to blame. So every approximation in "
        "this lesson rounds outward. The table is &sect;3's check: twelve independent placements of "
        "900 tumbling proxies, 5,230 genuinely overlapping pairs, and the grid's output compared "
        "against <code>brute_force_pairs</code> as a set."),

    "FIG_STAGES": ("l88_fig2.svg", "3",
        "<strong>The broadphase does not make the narrow phase faster. It makes there be two "
        "thousand times less of it.</strong> One frame of 2,000 bodies, measured. The grid reduces "
        "1,999,000 pairs to <strong>983</strong> candidates for <strong>0.091&nbsp;ms</strong>, "
        "which is <strong>17.7%</strong> of the two stages together; the narrow phase then spends "
        "0.426&nbsp;ms on the survivors. The dashed path is what is avoided: the identical narrow "
        "phase on all 1,999,000 pairs, an implied <strong>844&nbsp;ms</strong> &mdash; fifty "
        "frames' budget, for one frame. Note the shape of the trade, because almost every real "
        "optimisation has it: the expensive stage is not improved at all, and the instinct on "
        "reading &ldquo;the narrow phase is 82% of collision detection&rdquo; is to go and optimise "
        "the narrow phase. The 82% is fine. It is the 18% that bought it."),

    "FIG_CELLMAP": ("l88_fig4.svg", "4",
        "<strong>A proxy is a box, so it occupies a RANGE of cells &mdash; and the famous bug on "
        "this page is not the one that loses pairs.</strong> Left: one proxy spanning cells "
        "x&nbsp;&isin;&nbsp;[1,&nbsp;3] and y&nbsp;&isin;&nbsp;[1,&nbsp;2], six insertions for one "
        "object. Right: the cell numbering under <code>floor</code> and under a cast to "
        "<code>int</code>, which truncates toward zero and merges cells &minus;1 and 0 into one "
        "double-width cell straddling the origin. Every tutorial warns about that cast, and "
        "measured here it loses <strong>zero pairs</strong> &mdash; because a range-walk grid needs "
        "only that its cell map be MONOTONE, and if two intervals overlap then so do their images "
        "under a non-decreasing map. What truncation costs is occupancy: eight cells become one, a "
        "cell's pair loop is quadratic in what is inside it, and the neighbourhood runs "
        "<strong>4.0&times;</strong> the comparisons. The mistake that really does lose pairs is "
        "the innocent one in the last row &mdash; insert each proxy into the one cell containing "
        "its centre, and <strong>two thirds of the pairs there are</strong> disappear."),

    "FIG_OWNER": ("l88_fig5.svg", "5",
        "<strong>Two proxies whose cell ranges overlap meet in every shared cell, and exactly one "
        "of those cells owns the pair.</strong> Here the ranges share a 2&times;2 block, so a naive "
        "walk reports the pair four times &mdash; four narrow-phase calls, four manifolds, and once "
        "8.10's solver arrives, the same contact solved four times in one iteration, which is a "
        "stiffer contact than the one you asked for. The overlap of two axis-aligned cell ranges is "
        "itself an axis-aligned range, and a range has exactly one minimum corner; both proxies are "
        "present there by construction, because it lies inside both their ranges. So the rule is "
        "EXACT rather than heuristic, and it costs three <code>max</code> calls and no memory. The "
        "tables are what that is worth: a <code>std::set</code> of pair keys costs "
        "<strong>45%</strong> of the entire broadphase and a sort-and-unique 5%, and the closed "
        "form for the duplicate count agrees with the grid's own counter at <strong>3271 and "
        "3271</strong>."),

    "FIG_CELLSIZE": ("l88_fig6.svg", "6",
        "<strong>Both wings of the cost curve are cubic, which is why the floor is flat and why the "
        "vague rule of thumb survives.</strong> Entries fall as the cell grows (blue) and pair "
        "tests rise (red), and the green curve is the two-term model <code>a&middot;entries + "
        "b&middot;tests</code> whose constants were solved exactly from two rows &mdash; one "
        "dominated by each term. They come out at <strong>8.09 and 5.45&nbsp;ns</strong> here and "
        "<strong>8.11 and 5.44</strong> on a scene four times denser, because they are properties "
        "of the machine rather than of the crowd. Both counts are DETERMINISTIC, which matters: "
        "around the minimum the timing's spread reaches 1.18, meaning the slowest of forty-one runs "
        "took more than twice as long as the fastest, so the clock cannot distinguish 2.5&nbsp;m "
        "from 4&nbsp;m and the model can. Minimising it puts the optimum at <strong>2.13&times;</strong> "
        "the mean longest side on this scene and 1.32&times; on the dense one &mdash; and since "
        "both wings are cubic, the optimum moves as the SIXTH ROOT of the density, which is a "
        "26% move for a fourfold change."),

    "FIG_HASHING": ("l88_fig7.svg", "7",
        "<strong>The table is sized against the scene rather than against the world, and the "
        "textbook hash is measurably mediocre on exactly the input a physics scene produces.</strong> "
        "Left: a dense array of cells needs the world's extent and allocates for cells that are "
        "empty &mdash; a 1&nbsp;km world at a 1&nbsp;m cell is 10⁹ cells &mdash; and it has an "
        "EDGE, which has to be clamped, wrapped or rejected, and two of those three lose pairs. "
        "Right: hashing removes both problems, at the price of collisions, which cost a comparison "
        "and never an answer because every entry carries its cell coordinate. The reference in the "
        "table is neither hash: it is balls in bins, what a good hash <em>should</em> score. On "
        "random cells the textbook hash sits on the prediction &mdash; that is the control &mdash; "
        "and on a lattice of crates it is <strong>25.7%</strong> above it while a stronger mix is "
        "at 3.4%. It stays anyway, and the load-factor table is why the fix is not a bigger table: "
        "the gap to chance WIDENS as the table grows, 7% then 26% then 56% then 160%."),

    "FIG_MARGIN": ("l88_fig8.svg", "8",
        "<strong>A margin costs the cube of a Minkowski sum, not the ratio of two volumes.</strong> "
        "Two boxes overlap exactly when the difference of their centres lies inside a box of extent "
        "<code>e_a&nbsp;+&nbsp;e_b</code> &mdash; the same Minkowski object 8.5 and 8.6 spent two "
        "lessons inside, seen from the other direction. Fattening each box by <em>m</em> adds "
        "<code>2m</code> to its own extent and therefore <code>4m</code> to the sum, so the pair "
        "count goes as <code>((E&nbsp;+&nbsp;4m)/E)&sup3;</code>. Measured within 12% of that at "
        "every margin from a centimetre to half a metre. Read the last column too, because 8.9 will "
        "rely on it: at every margin the reported set CONTAINS the unfattened one, so a margin "
        "never loses a pair. And read the second: half a metre on 1.5&nbsp;m crates costs "
        "<strong>4.5&times; the pairs</strong>, each of which becomes a 432&nbsp;ns narrow-phase "
        "call. A margin is not free insurance."),

    "FIG_TEAPOT": ("l88_fig9.svg", "9",
        "<strong>A uniform grid's fatal input is not exotic. It is the floor.</strong> A "
        "200&nbsp;&times;&nbsp;0.2&nbsp;&times;&nbsp;200&nbsp;m plate at a 1&nbsp;m cell occupies "
        "201&nbsp;&times;&nbsp;2&nbsp;&times;&nbsp;201 = <strong>80,802 cells</strong>, against "
        "30,225 entries for the two thousand crates it is under &mdash; so one proxy wants nearly "
        "three times the insertions of the entire rest of the scene, and it is alone in almost "
        "every one of them. Adding one object multiplies the work by 3.7, and the cliff is cubic in "
        "the reciprocal of the cell size, which is why the table quadruples and quadruples again. A "
        "uniform grid has no structural answer to this, because every cell being the same size is "
        "the definition of one. What it has is a GUARD: past <code>max_cells_per_proxy</code> a "
        "proxy stops being gridded and is tested against everything, which keeps the contract "
        "exactly &mdash; the pair sets with the guard on and off are equal &mdash; and turns a "
        "cubic explosion into a linear cost with a counter attached."),

    "FIG_BUDGET": ("l88_fig10.svg", "10",
        "<strong>Below about a hundred bodies this entire stage is a net loss, and almost nobody "
        "measures that.</strong> An AABB overlap test is six comparisons on data already in "
        "registers; a grid build touches every proxy twice, chases a hash table, writes an entry "
        "array and walks it again. The crossover lands on 96 or 128 depending on the run, because "
        "the two are within twenty per cent of each other either side of it &mdash; read it as "
        "&ldquo;about a hundred&rdquo; rather than as a threshold. Above it the curves separate and "
        "never come back. Right: having paid for it, the broadphase is <strong>17.7%</strong> of "
        "collision detection on 2,000 bodies rather than the 99.95% the alternative spends on pairs "
        "that were never going to touch &mdash; and it allocates nothing across two thousand "
        "rebuilds, which is checked with a counting <code>operator new</code> rather than asserted, "
        "because an allocation per frame hides from a timing measurement and shows up later in "
        "somebody else's profile."),
}

LISTING_META = {
    "engine/include/engine/phys/broadphase.hpp": ("new", "new"),
    "engine/src/phys/broadphase.cpp":            ("new", "new"),
    "engine/include/engine/phys/collide.hpp":    ("modified", "modified"),
    "engine/include/engine/engine.hpp":          ("modified", "modified"),
    "engine/CMakeLists.txt":                     ("modified", "modified"),
    "demos/CMakeLists.txt":                      ("modified", "modified"),
    "demos/broadphase/main.cpp":                 ("new", "new"),
    "scratch/verify_88.cpp":                     ("new", "new"),
    "scratch/build_verify_88.sh":                ("new", "new"),
}

LISTING_LANG = {
    "scratch/build_verify_88.sh": ("bash", "shell"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
    "demos/CMakeLists.txt": ("cmake", "CMake"),
}


def _pin(path):
    return "scratch/l88_" + path.replace("/", "_")


LISTING_SOURCE = {path: _pin(path) for path in LISTING_META}


def esc(text):
    return (text.replace("&", "&amp;")
                .replace("<", "&lt;")
                .replace(">", "&gt;")
                .replace('"', "&quot;")
                .replace("'", "&#x27;"))


def listing(path):
    with open(LISTING_SOURCE.get(path, path)) as fh:
        body = fh.read()
    tag, word = LISTING_META[path]
    lang, label = LISTING_LANG.get(path, ("cpp", "C++"))
    return (
        '  <figure class="listing">\n'
        '    <figcaption>\n'
        f'      <span class="path">{path}</span>\n'
        f'      <span class="tag {tag}">{word}</span>\n'
        f'      <span class="lang" data-lang="{lang}">{label}</span>\n'
        '    </figcaption>\n'
        f'    <pre><code class="lang-{lang}">{esc(body)}</code></pre>\n'
        '  </figure>\n'
    )


def figure(key):
    name, num, caption = FIGURES[key]
    with open(f"scratch/{name}") as fh:
        svg = fh.read().rstrip()
    svg = "\n".join("    " + ln if ln.strip() else ln for ln in svg.split("\n"))
    return (
        '  <figure class="dia bleed">\n'
        f'{svg}\n'
        '    <figcaption>\n'
        f'      <span class="fignum">Figure {num}.</span>\n'
        f'      {caption}\n'
        '    </figcaption>\n'
        '  </figure>\n'
    )


HEAD = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>8.8 — Broadphase: A Uniform Grid · Build a Professional 3D Game Engine</title>
<meta name="description" content="Four lessons of narrow phase have built something precise, and this one is about the fact that you cannot afford to run it. Lesson 8.7 measured the whole chain at 395 nanoseconds for one pair, and this lesson re-measures it on 8.7's own fixture at 432. A scene of n bodies has n(n-1)/2 pairs, so a sixteen-millisecond frame is gone at n equals 278 - not at ten thousand bodies, not at some large number, but at two hundred and seventy-eight crates, which is a small pile. A broadphase is the stage that asks a cheap conservative question of every pair and hands the narrow phase only the survivors, and it gets to be cheap because it is allowed to be wrong in exactly one direction: it may report pairs that do not touch, and it may not omit one that does. A false positive costs one narrow-phase call and a correct answer; a false negative has no stage downstream that could notice, and reaches the player as things falling through the floor. Every approximation in the lesson rounds outward because of that. The structure is a hashed uniform grid, rebuilt from scratch every frame in four linear passes, and the reason it is hashed rather than an array is memory and the absence of a boundary: a kilometre of world at a metre of cell is a billion cells, almost all of them empty, and an array has an edge that has to be clamped, wrapped or rejected, two of which lose pairs. The two things that are actually hard turn out not to be the hashing. The first is duplicates: two proxies sharing four cells meet four times, and the usual fixes - a set of pair keys, a sort and unique - cost forty-five per cent and five per cent of what the entire broadphase costs. There is an exact rule instead, the owner cell, which is the minimum corner of the overlap of two cell ranges: both proxies are present there by construction, there is only one minimum corner, so the pair is reported exactly once for three max calls and no memory. The number of duplicates it removes is computable in closed form, and the closed form agrees with the grid's own counter at 3271 against 3271. The second hard thing is the teapot in the stadium: one ground plate at a one-metre cell wants 80,802 cells to itself, nearly three times what two thousand crates want, and it is alone in almost every one of them. A uniform grid has no structural answer to that, because every cell being the same size is the definition of one; what it has is a guard that stops gridding such a proxy and tests it against everything, which keeps the contract exactly and converts a cubic explosion into a linear cost with a counter attached. Along the way three claims that everybody repeats get measured rather than repeated. The famous floor-versus-cast bug loses zero pairs here, because a range-walk grid needs only that its cell map be monotone, and truncation is monotone; what it costs is occupancy, four times the comparisons in one neighbourhood, which is a footnote rather than a bug. The mistake that really loses pairs is the innocent-looking one beside it - insert each proxy into the single cell containing its centre - and it loses two thirds of the pairs there are. The rule of thumb for cell size is replaced by a two-term cost model fitted to the grid's own deterministic counters, whose constants come out the same on scenes of different density because they are properties of the machine, and whose minimum moves as the sixth root of the crowd, which is why one formula that never looks at density can serve. And the textbook spatial hash really is twenty-six per cent worse than chance on a lattice of crates, which is what a physics scene looks like the moment it settles - and it stays, because the same section measures what that is worth: nine per cent of the inner loop, against a replacement that costs twice as much per call. Ships broadphase.hpp with a proxy, a pair, a config, a stats struct and the grid; a brute-force oracle that is also the baseline and also the right answer below about a hundred bodies; a demo that checks the contract live against n(n-1)/2 every frame; ten measured sections and 51 checks, every one with a control, including two that disproved the claim their section was written to make; and an honest crossover that says when not to ship any of it.">

<!-- SHARED-CSS:BEGIN -->
<!-- The shared course stylesheet, linked rather than inlined. Single source of
     truth: docs/shared/course.css. Edit that file; this page carries no copy.
     Still no build step - the link resolves straight off the filesystem, so this
     page opens by double-clicking, offline. -->
<link rel="stylesheet" href="../shared/course.css">
<!-- SHARED-CSS:END -->

<!-- KaTeX (optional). If unreachable the raw TeX remains readable, and every
     equation is also stated in prose + .eq-plain, so nothing is lost. -->
<link rel="stylesheet"
      href="https://cdn.jsdelivr.net/npm/katex@0.16.11/dist/katex.min.css"
      integrity="sha384-nB0miv6/jRmo5UMMR1wu3Gz6NLsoTkbqJghGIsx//Rlm+ZU03BU6SQNC66uf4l5+"
      crossorigin="anonymous">
</head>
<body>

<header class="masthead">
  <div class="masthead-inner">
    <a class="course" href="../index.html">Build a Professional 3D Game Engine</a>
    <span class="spacer"></span>
    <a href="../index.html">Contents</a>
    <a href="../conventions.html">Conventions</a>
    <a href="../math-toolbox.html">Math Toolbox</a>
    <button class="theme-toggle" id="theme-toggle" type="button" aria-label="Toggle colour theme">Theme</button>
  </div>
</header>

<div class="wrap">

"""

TAIL = """
  <nav class="lesson-nav" aria-label="Lesson navigation (bottom)">
    <a class="prev-l" href="08-07-contact-manifolds.html">
      <span class="dir">← Previous</span>
      <span class="ttl">8.7 — Contact Manifolds and Persistence</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="08-09-impulse-response.html">
      <span class="dir">Next →</span>
      <span class="ttl">8.9 — Impulse Response: Restitution and Friction</span>
    </a>
  </nav>

</div>

<!-- SHARED-SCRIPT:BEGIN -->
<!-- The shared page script (theme toggle, TOC scrollspy, syntax highlighter),
     linked rather than inlined. Single source of truth: docs/shared/course.js.
     A plain classic script at end of body, so it runs exactly where the inline
     copy used to: after the DOM is parsed, before KaTeX's deferred render. -->
<script src="../shared/course.js"></script>
<script defer
        src="https://cdn.jsdelivr.net/npm/katex@0.16.11/dist/katex.min.js"
        integrity="sha384-7zkQWkzuo3B5mTepMUcHkMB5jZaolc2xDwL6VFqjFALcbeS9Ggm/Yr2r3Dy4lfFg"
        crossorigin="anonymous"></script>
<script defer
        src="https://cdn.jsdelivr.net/npm/katex@0.16.11/dist/contrib/auto-render.min.js"
        integrity="sha384-43gviWU0YVjaDtb/GhzOouOXtZMP/7XUzwPTstBeZFe/+rCMvRwr4yROQP43s0Xk"
        crossorigin="anonymous"
        onload="renderMathInElement(document.body, {
          delimiters: [
            {left: '\\\\[', right: '\\\\]', display: true},
            {left: '\\\\(', right: '\\\\)', display: false}
          ],
          throwOnError: false
        });"></script>
<!-- SHARED-SCRIPT:END -->
</body>
</html>
"""




def main():
    parts = []
    for name in ("a", "b", "c", "d", "e", "f"):
        with open(f"scratch/l88_body_{name}.html") as fh:
            parts.append(fh.read())

    page = HEAD + "\n".join(parts) + TAIL

    for key in FIGURES:
        page = page.replace(f"@@{key}@@", figure(key))

    page = re.sub(r"@@LISTING:([^@]+)@@", lambda m: listing(m.group(1)), page)

    left = re.findall(r"@@[A-Z0-9_:./-]+@@", page)
    if left:
        raise SystemExit(f"unsubstituted placeholders: {sorted(set(left))}")

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as fh:
        fh.write(page)
    # BYTES, not characters — build_74.py's note applies. The page is UTF-8 and
    # full of × − ° §, so `len(page)` is several thousand short of `wc -c`.
    print(f"wrote {OUT}  ({len(page.encode('utf-8')):,} bytes, "
          f"{page.count(chr(10)):,} lines)")


if __name__ == "__main__":
    main()
