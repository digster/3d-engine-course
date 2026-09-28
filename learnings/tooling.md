# Tooling — the docs pipeline, builders, sweeps and repairs

Moved verbatim from LEARNINGS.md on 2026-09-27, in the original order; LEARNINGS.md
is now the index. Append new sections at the end, and add each heading there.

---

## A constraint nobody rechecked cost 18% of the docs tree (CSS extraction)

The shared stylesheet and page script were duplicated into all 36 pages — 26.6 KB and 8.0 KB
each, **1.18 MB, 18% of `docs/`** — because the spec said each lesson had to be "fully
self-contained … no external assets". That rule was written to protect a real property: a lesson
must render by double-clicking it, offline, with no server and no build step.

The rule outlived its justification. **`file://` does not block a relative `<link
rel=stylesheet>` or a classic `<script src>`.** The restriction people remember is on
`fetch`/XHR/ES modules, which are a different mechanism. So the no-build-step guarantee never
actually required inlining — the duplication was protecting against something that was not there.

Two lessons, and the second is the sharper one:

- **Test the constraint, don't inherit it.** The premise was checkable in about five minutes with
  a three-file fixture and a headless browser. It had instead been carried, unexamined, through
  36 pages and a purpose-built propagation tool.
- **Check it in the engine that is strictest, not the one you have open.** Lesson pages link
  *upward* (`../shared/course.css`), and WebKit — Safari's engine, the one with the tightest
  `file://` policy — is the one that could plausibly have refused. Verifying in Chromium alone
  would have proved almost nothing about the macOS reader who double-clicks a lesson. All three
  engines pass, including the upward traversal; that is the claim worth having.

What it cost, stated so nobody rediscovers it as a bug: **a lesson file is no longer portable on
its own.** Copied out of the tree it renders unstyled. The `docs/` directory is the unit now.

### The failure mode traded for the old one

Duplication drifts loudly enough to be findable (six versions of the highlighter by Lesson 1.2).
A **wrong relative href does not fail at all** — no error, no console warning, just an unstyled,
inert page that reads as unfinished rather than broken. And it breaks by *moving* a page, not by
editing one, so it arrives in commits that look unrelated. The prefix depends on depth (`shared/…`
at `docs/`, `../shared/…` at `docs/lessons/`), which is why `apply-shared.py` computes it per page
rather than trusting anyone's eye, and why `check-page.js` now asserts the sheet is *in effect*
rather than merely linked.

### Do not ask the CSSOM whether a stylesheet loaded

The obvious probe — `document.styleSheets[…].cssRules.length > 0` — reports a **perfectly good
page as broken** over `file://` in WebKit, which treats every file as its own origin and throws a
`SecurityError` on CSSOM access. The sheet had loaded and applied; only the introspection was
blocked. The check flagged all nine sample pages while `getComputedStyle` showed the shared
`--bg: #fdfdfb` and 22,829 highlighted tokens on the very same pages.

Same shape as the KaTeX trap in this file: **a probe that cannot distinguish "absent" from
"unreadable" is not a check.** Judge by the effect (computed style), and treat a thrown CSSOM read
as *inconclusive* — never as failure.


## Two correct branches can leave a hole between them (docs tooling)

The change that extracted the shared CSS and script into `docs/shared/` converted all 36 pages
that existed **on its branch**. That branch was cut before Lesson 3.6 landed on `main`. So when it
merged, 3.6 was not converted — not because the run missed it, but because the page did not exist
when the run happened, and a merge has nothing to say about a file neither side changed.

Both branches were individually correct and fully verified. The defect lived in the gap, and 3.6
shipped for weeks carrying an 881-line inline copy of a stylesheet that by then had a single source
of truth.

Two properties made it survive review:

- **It is introduced by merging, not by editing.** No diff shows anything wrong, because nothing
  about the page changed. It simply missed a change that happened elsewhere. There is no hunk to
  review.
- **It fails silently.** A missing or wrong shared link throws nothing; the page renders unstyled
  and inert, which reads as a page nobody finished rather than a page that is broken.

The general shape is worth carrying beyond stylesheets: **a branch that adds an item and a branch
that transforms every item are a bad pair**, and no amount of care on either one closes the gap.
Codemods, renames, lint-rule rollouts and dependency bumps all have it. The only reliable defence
is a checker that enumerates the current tree rather than the changed files — `apply-shared.py
--check` does exactly that, which is why it found this in one run — and the discipline is to run it
**after merges**, not only after the edits you remember making.

## Course-infrastructure facts (docs/, 2026-08-26)

### `max-height` is a no-op on anything already shorter than it

The whole listing-fold feature is one rule on `.listing pre`, applied unconditionally to all 798
listings across 50 pages, and it changed only the ~230 that are whole files. The ~560 short
excerpts clear the bar and render pixel-identically. No markup migration, no builder change, no
re-stamping. When a retrofit looks like it needs an attribute on every element, check first whether
the property you want is already inert on the elements you meant to skip.

### The corpus was safe to automate because its shape is bimodal, and that was measured first

Listing lengths: median 9 lines, p75 74, p90 290. The count exceeding a threshold barely moves
between 24 and 60 lines (233 → 209) — the pages are made of short excerpts *and* whole files, with
almost nothing between. That measurement is what justified an automatic threshold instead of a
per-listing opt-in flag. A threshold over a distribution you have not plotted is a guess.

### A clamp that must survive first paint belongs in CSS, not in the page script

`course.js` is a classic `<script>` at end of body, so it runs *after* the browser has painted and
after it has jumped to any `#anchor`. Collapsing the document there would yank a reader who was
already positioned. Clamping in CSS happens at parse time and cannot. The script's whole job is to
*add controls*, and the bottom bar is `position: absolute` while collapsed so even that is free:
removing every injected bar changes document height by exactly 0 px. Measured end to end, the
script now moves Lesson 5.1's height by 33 px — one 13-line listing being released.

### `<details>` hides content with `display: none`, which blinds computed-style probes

It was the semantically obvious choice for a collapsible listing and the wrong engineering one:
`check-page.js` decides `sharedCssApplied` and `wrappedListings` from
`getComputedStyle('.listing pre')`, and a closed `<details>` returns `rgba(0,0,0,0)` and `normal`
for a subtree that is perfectly styled. Clipping with `overflow: hidden` keeps every probe honest —
and keeps the text in the accessibility tree, which is the right trade for a screen reader anyway.

### Decide by line count, not by measuring the box

`textContent.split("\n").length` needs no layout, so it is correct before webfonts settle and
correct for the one listing in the course nested inside a closed `<details>`
(`01-07-vectors-2d.html`), where `scrollHeight` and `clientHeight` both read 0 and every
measurement-based test calls a 400-line file "short".

### A verifier must expand what it is about to measure

`check-page.js` now opens every listing before running the geometry and layout checks. Otherwise a
clamp hides the very lines that cause a horizontal-overflow or wrapped-listing regression, and the
suite passes on a broken page. The fold-specific checks run *first*, against the collapsed state a
reader actually loads — including `clippedWithoutToggle`, which fires en masse if `course.js` fails
to load at all, exactly when every other signal still looks fine.

### Two vocabularies for one concept will get crossed, so make the checker enumerate them

`.badge` takes `mod`; `.tag` takes `modified`. The builders emitted `class="tag mod"` — right word,
right-looking source, no matching rule — so 82 badges across 14 pages rendered neutral grey next to
144 amber ones, for six lessons, without anyone noticing. It was fixed at the emitter rather than
by aliasing `.tag.mod`, because the alias would have blessed both spellings permanently. The guard
is three lines: list the modifiers a class actually defines and report anything else.

### `scroll-behavior: smooth` makes fixed-delay anchor tests lie

`course.css` sets it, and lesson pages are tens of thousands of pixels tall. A Playwright check
that navigates to `#id`, waits 300 ms and measures the heading reads it mid-animation: headings
measured at y=2091 and y=5014 looked exactly like "content grows after the anchor scroll", which is
a real failure mode and cost a diagnosis detour. Use `reducedMotion: 'reduce'`, or poll until
`scrollY` stops moving.

### A class with no rule fails silently, and looks like a styling opinion

`<p class="mono">` was authored from Lesson 3.3 onward for hand-aligned numeric walkthroughs —
columns padded with `&nbsp;`, continuation lines indented under an `=`. No `p.mono` declaration was
ever written. All 16 blocks across two lessons rendered in the body serif at 18px: no alignment, no
monospace, and wide enough to push the page into horizontal scroll at 390px. It survived thirteen
lessons because unstyled prose still *reads* fine — the failure only shows if you know the columns
were meant to line up. The nearest neighbour in the sheet, `figure.dia svg .mono`, is a different
class in a different context and only sets the family, which is exactly the kind of near-miss that
makes a missing rule look deliberate.

### `overflow-x` was a listing rule when it should always have been a `pre` rule

Six bare `<pre>` blocks across three lessons — compiler errors in pitfall callouts, harness
transcripts in `.worked` boxes — sat outside `<figure class="listing">` and so matched no overflow
declaration at all. A 677px error message inside a 302px column spilled visibly and dragged the
document into horizontal scroll. The rule now hangs on `pre`, not on `.listing pre`, because the
"code scrolls, never wraps" bargain is a property of preformatted text and not of the chrome that
happens to be wrapped around it.

### A dedup key that ignores position hides duplicates of the same defect

`check-page.js` dedupes text-on-shape hits by `figure|text|tagName|class`. Lesson 0.6 figure 2 has
*two* labels reading "observe", and had they both collided the checker would have reported one.
When a finding says "a label named X collides", verify how many X there are before assuming the fix
is done — measuring both is what proved only the second one needed moving.

### A gutter is a width budget, and long identifiers overrun it

Lesson 1.2's timeline reserves x=10..100 for row labels. `SDL_EVENT_KEY_DOWN` at 11px is 131 user
units and ran past the axis into the t=0 event spike. Shrinking to `xs` still needs 113; anchoring
right pushes the text to negative x and trips the viewBox-spill check instead. Wrapping onto two
`<text>` elements is the fix that keeps the exact SDL3 constant, which is the whole reason the
label is there.

---

## Course-infrastructure facts (docs/, STATE-block consolidation)

### A rule written before its replacement existed does not retire itself

CLAUDE.md §9 said "end every lesson with a fenced `STATE` block" because, when it was written,
there was nowhere else to put one. `STATE.md` arrived five lessons later (`cba92f1`, with 0.6) and
took over the resume-key job — but §9 was never amended, so **both** ran for another 46 lessons.
By 5.7 the in-page copies were **3.97 MB, 22.2% of `docs/lessons/`**, and 5.7's own block was
331 KB of a 550 KB page (60%) and byte-identical to `STATE.md`'s, 4,433 lines each.

This is the *second* time this exact shape has bitten `docs/` — the shared-CSS extraction was the
first, at 18% duplication. The tell is identical both times: a cost that grows monotonically with
lesson count while nobody re-reads the rule that causes it. **When a new artifact takes over an
old rule's job, amend the rule in the same breath.** Three places still demanded the block (§6
item 13, §9, §11's pre-flight checklist), plus `docs/_template/README.md`'s authoring checklist.

### "Only the newest copy is accurate" means every other copy is a bug

The standing rule was already *"only the newest lesson's STATE block tracks reality"* — an
explicit admission that 51 of 52 pages carried a knowingly-stale snapshot. A duplicate that is
documented as unreliable is not documentation, it is 4 MB of it. Worth asking of any per-page
copy: if it were wrong, would anything catch it? Here nothing would, because nothing read it.

### Invisible duplication still costs at write time

The blocks were inside `<details>`, collapsed, muted — a reader never saw one, so this never
showed up as a rendering complaint. The cost was entirely on the authoring side: every lesson
regenerated 4,400 lines that had to stay consistent with a file that already held them.
**Absence of a reader-facing symptom is not evidence that duplication is free.**

### Deleting a rule means deleting its scaffolding too

The block was gone but four things still referenced it: three `.state` rules in `course.css`, the
template's `SECTION 12 — STATE BLOCK` comment banner, and two checklist lines. One more was
subtler — `course.css`'s rationale comment for `pre { overflow-x: auto; }` cited "`.listing pre`
and `.state pre` restate this", naming a selector that no longer existed. **Grep for the class
name, not just the markup**, and read the comments the grep hits.

### Verify a mass deletion by proving it deleted nothing else

`git diff --numstat` over 53 files gave **54,907 deletions and 0 insertions**. That single number
is a stronger guarantee than reading any diff: a regex that over-matched, ate a `<nav>`, or
re-indented a line would have shown up as insertions. Pair it with a tag-balance parse across
every touched file and `apply-shared.py --check` (which re-verifies all 57 pages' relative
`../shared/` prefixes), and the change is proven without opening a browser — though a real
Chromium pass over HTTP still confirmed CSS applies and each page now ends Further Reading → nav.

---

## Renumbering a live course (roadmap reshape, 2026-09-08)

- **A bare `N.M` in this corpus is almost never a lesson reference.** Renumbering Module 8 → 9
  turned up 58 candidate `8.N` strings; exactly **4** were lesson references. The rest were
  intra-page section headings (`<h3>8.2`, and pages number their own sections 1–15), exercise
  numbers (`Exercise 4.8.3`), and measurements (`8.3 MB`, `8.4 × 10⁻⁸`, `farZ = -8.2`). A blind
  `sed` over `8.N` would have silently corrupted ~54 sites, most of them numeric data inside
  published prose. **Sweep the unambiguous long form (`Module 8`, 177 occurrences, verified total);
  review the short form by hand, every time.**

- **The dangerous replacements are the ones a newline splits.** Three misses survived a 37-rule
  table purely because the text read `Module 7's\n    /// collision lessons`. Re-scan after any
  bulk edit with a `re.S` pattern and a keep-list, rather than trusting the rule table's own
  report — the rules that matched *nothing* are the ones worth reading.

- **Source comments are copied verbatim into published pages, so a fix must be paired.**
  `scratch/build_NN.py` inlines engine source via `@@LISTING:path@@`. `bounds.hpp`'s comment string
  is physically embedded in `docs/lessons/06-08-shadow-mapping.html`. Editing only the header
  leaves the page disagreeing with the repo, and `scratch/` is gitignored so regeneration is not
  guaranteed. **Edit both, always.**

- **Fix a misattribution to its *current* correct value before renumbering, not after.**
  `bounds.hpp` said "Lesson 6.10's frustum culling" when 6.10 was HDR and culling was 6.13.
  Correcting it to 6.13 first let the general 6.13 → 6.16 map carry it home; correcting it
  afterwards would have needed a special case that the map would have fought.

- **Keeping one lesson number fixed can be worth more than a tidy sequence.** 6.9 stayed put
  because 6.8 references it six times in prose plus both nav labels and calls cascades "an
  extension of this file". Inserting before it would have cost 8 edits and a narrative thread;
  inserting after it cost nothing. **Renumber around the references, not through them.**

- **An unchecked hand-maintained page drifts in every direction at once.** `docs/index.html` had
  never been validated and had five simultaneous inconsistencies, two of which contradicted *each
  other* (95 vs 94 lessons) on the same page. `docs/_template/check-curriculum.py` now checks it,
  and CLAUDE.md §11's pre-flight requires it green.

- **A new linter's first run is mostly its own bugs, and that is normal.** The checker's first pass
  reported 27 problems; 16 were entity-encoding false positives (`&mdash;` vs `—`, `&middot;` vs
  `·` — both render identically and pages use them interchangeably). Normalise punctuation entities
  before comparing, and never `&lt;`/`&gt;`, which appear inside code listings. A linter that cries
  wolf gets muted, which is worse than no linter.

- **zsh does not word-split unquoted parameters.** `node check-pages.mjs $PAGES` passed 42 page
  paths as one argument and reported a single bogus FAIL. Use `${(f)PAGES}` (split on newlines).
  This will bite again in any loop over a captured file list.

## Retrofitting a published lesson (2026-09-08)

- **A pinned listing means three copies, not one.** `build_66.py` splices `gltf.hpp` from
  `scratch/l66_gltf.hpp`, not from the live header, so correcting a doc comment meant editing the
  header, the pin, *and* the rendered listing already inside the page. Editing only the header
  leaves the page disagreeing with the repo and nothing reports it; editing only the page means the
  next rebuild silently reverts you. **Check `LISTING_SOURCE` before touching any file a published
  lesson lists.** Diff the pin against that lesson's commit first, so you know you are starting
  from a clean snapshot.

- **The most damaging gaps are the ones that fire no status.** 6.6's importer reports every limit
  it has — `too_many_vertices`, `unsupported_primitive`, an assertable `factor_texture_conflicts`
  count — except `alphaMode`, which is silent. And the reason is worth generalising: **a status
  code needs a concept to compare against.** The engine had no blend state, so the importer could
  not report a conflict with something that did not exist. When a subsystem "forgets" to report a
  case, check whether it has anywhere to put the answer before calling it an oversight.

- **When correcting a published measurement, separate the number from the reading.** 4.8's 87% is
  *sound for what it measured* — geometry, winding, depth and interpolation genuinely agreed. What
  was wrong was the broader inference that the GPU path was correct. Saying "keep the result, here
  is what it cannot support" is both more accurate and more useful than retracting a figure that
  was never false.

- **A harness that builds its own configuration tests that configuration.** 4.8's comparison
  constructed its own `_SRGB` target to isolate the rasterizers from presentation — a reasonable
  instinct that removed the exact surface the bug lived on, and let a real defect survive four
  modules *and* a pixel-by-pixel comparison. When a test constructs its own setup, ask which part
  of the shipped path it just stopped covering.

- **Qualify the recap too.** A reader who skims takes the recap's summary as the finding. A caveat
  buried in §3 does not reach them.

## The figure-order sweep — three lessons, and what it uncovered underneath

- **A checker's first run is worth more than its hundredth.** `figOrder` was added in 6.12 after
  that lesson's own build misnumbered two figures. Sweeping the back catalogue with it found the
  same defect in **three published lessons** — 2.5, 3.10 and 4.1 — which had been shipping a
  numbered cross-reference that landed on the wrong picture. Same pattern as
  `check-curriculum.py`, which found three dead prerequisite links on its first run.

- **Renumber, do not move.** In all three, every figure already sat in the section that discusses
  it and every prose reference was adjacent to its own figure; only the numbers were out of step.
  Moving a figure would have moved it away from the prose that introduces it. The rule that fell
  out: **when placement and numbering disagree, the placement is almost always the deliberate one**,
  because it was chosen while writing the argument and the number was assigned in a draft order.

- **Prove the builder reproduces the page BEFORE changing anything.** Both old builders turned out
  to be unrunnable, and in two different ways that would each have silently corrupted a page:
  - `build_310.py` and `build_41.py` still **stamped a `STATE` block**, retired from lesson pages at
    5.7. Re-running either would have re-added 60% of a file.
  - Both read their listings from `src/`, a directory **Module 5's refactor deleted**. They had been
    unreproducible since 5.1 and nobody had noticed, because nobody had needed to rebuild them.
  Fixed by retiring the STATE stamping and pinning every listing from the commit that shipped each
  lesson — after which both rebuilt **byte-identically**, which is the only thing that makes the
  subsequent diff trustworthy.

- **A rendered page can be more correct than its source.** The 2026-09-08 Module 8→9 renumber and
  4.1's "next" nav link had been applied to the shipped HTML and never to the body fragments or the
  listings. A rebuild would have reverted all four. **If a page is edited by hand, the edit has to
  go back into whatever generates it, the same day** — this is the third time this exact drift has
  been found (6.9's retrofit links, and now twice here).

- **Not every page has a generator.** Lesson 2.5 predates the `build_NN.py` pipeline, which starts
  at 3.7 — so for it the rendered HTML *is* the source and editing it directly is correct. Check
  before assuming a build script exists.

- **Swapping two values needs a sentinel.** Renaming `fig3 ↔ fig4` (files, caption numbers, and
  `aria-labelledby` ids) one at a time clobbers one of them. Every swap here went through a
  temporary name. Related: an assertion of the form "the substitution changed something" is wrong
  for an identity mapping — 1→1 legitimately changes nothing, and asserting otherwise aborted a
  script halfway through a rename.

---

## The builder reproducibility repair (2026-09-12)

Twenty-seven of thirty-eight page generators could no longer produce their own published page.
Eleven crashed; sixteen ran and made something different. All thirty-eight reproduce byte-identically
now, and the standing check is `docs/_template/check-builders.py`.

- **Measure on the axes the failure actually has.** The audit that missed this asked one question —
  "did the page file change?" — of a three-axis situation: *did it exit 0*, *did it write anything*,
  *does what it wrote match*. A crashing builder writes nothing, so eleven crashes scored as eleven
  passes and the problem was filed as two builders. The harness now reports the three separately and
  refuses to infer one from another. Generalised: **when a check collapses several failure modes
  into one observation, it will report the benign one.**

- **The rebuild diff is the oracle.** Every line it prints is a correction that lives on the page and
  not in its source, so repair is not guesswork: derive the pairs from the diff, route each to
  whichever source produced that line, re-run. What made this mechanical was realising the four
  spellings a page line can have — verbatim (body fragment), escaped (pinned listing), re-indented
  (inline SVG), or not a line at all (a figure caption wrapped across Python string literals).

- **A published page is a lossless archive of its own sources.** CLAUDE.md §8's "zero placeholders"
  rule means every listing is embedded whole. So when `scratch/verify_54.cpp` — gitignored, therefore
  no history — turned out to have been rewritten by Lessons 6.4 and 6.7, its 5.4-era text was still
  recoverable *from the page that published it*. `scratch/extract_listing.py` does this and asserts
  the round-trip. Four listings across 5.4, 5.6, 6.1, 6.2, 6.6 and 6.7 were recovered this way.

- **`html.unescape()` is not the inverse of a five-rule escaper.** It decodes the entire HTML5 entity
  table, including legacy entities with no semicolon, so round-tripping through it alters bytes the
  escaper never touched. The inverse of `esc()` is five replacements with `&amp;` undone **last**.
  Caught only because the extractor asserts its own round-trip — without that assertion it would have
  written a quietly corrupted pin.

- **A pin's right commit is decidable, not a judgement call.** The page embeds each listing escaped,
  so "does this page show commit X's version of this file?" is a substring test. `which_commit.py`
  answers it per file, which matters because two pages are pinned to a *later* lesson's commit
  (below) and guessing would have silently rewritten them.

- **The drift class recurses.** A page's source is an SVG; the SVG's source is `figs_NN.py`. Porting
  a diagram-label fix into only the SVG leaves the generator able to revert it — the identical bug
  one layer down. `check-builders.py --figures` is the check for that layer.

- **My own tool shipped the bug it exists to catch.** `port_line.py` skipped TSV lines beginning with
  `#` as comments; two of Lesson 5.1's corrections are CMake comment lines, so they were silently
  dropped while the run still reported success. The fix was to delete the comment convention, which
  had no user and only a failure mode. Same family as `class="tag mod"`: **plausible output with a
  hole in it beats an error message at hiding.**

### Two pedagogical defects found, deliberately NOT fixed

Both are Cause A committed to history, and correcting them changes a published lesson's *content*,
which is a different decision from making its builder reproducible. Recorded here for the author.

- **`05-02-platform-layer.html` shows Lesson 5.3's code.** When 5.3 landed it re-ran `build_52.py` to
  retrofit a `next` nav link; the live reads pulled 5.3's logging in — 156 lines in, 38 out. The page
  shows `ENGINE_LOG_ERROR(...)` where 5.2 wrote `SDL_Log(...)`, `image_report` where 5.2 wrote
  `image_status`, and `#include <engine/core/log.hpp>`, a header 5.3 creates. This violates §8's
  "every listing must compile at this point in the course".

- **`05-04-handles.html` shows Lesson 5.5's code**, from `d599928`, including a doc comment reading
  "Lesson 5.5 replaced this struct's contents" — in the past tense, inside Lesson 5.4.

These two are the *only* pages where the next lesson's commit changed more than the nav links; the
other thirteen were `+2/-2`. That is what makes them findable and bounded.

### One page byte-changed, zero rendered characters

`06-06-gltf.html` had two raw `'` inside a *generated* listing, hand-typed by the 7756e93 retrofit
where the builder's `esc()` emits `&#x27;`. No pin can make `esc()` produce a bare apostrophe, so the
page was normalised instead. **A hand-edit to generated output is a fork; the page and its generator
have to be brought back into agreement in one direction or the other, and the generator wins.**

### Known gap, so it is not rediscovered as a bug

`figs_45/46/48.py` read `.ppm` render captures that later sessions overwrote, so `--figures` reports
those three as DIFF. The published SVGs are correct and the pages rebuild from them; it is the SVGs'
own inputs that are lost. Recovering them means re-rendering Module 4 demos on a Module 6 engine.

---

## A label's colour is either semantic or an identity, and they have different fixes

Thirteen inline `fill="…"` attributes on SVG `<text>` shipped in 06-02 and 06-03. `course.css`'s
`figure.dia svg text { fill: var(--dia-ink); }` is CSS, and CSS always beats a presentation
attribute, so every one of them was silently ignored: the labels rendered in default ink, nothing
errored, and both pages looked finished. `apply-shared.py --check` has linted for this since 1.7 —
these predate the lint being taken seriously, and they sat through fifteen lessons.

The fix is not one fix, because the colour was doing two different jobs:

- **Semantic** (06-02's table: two quantities fell by four, one did not). Use the existing classes —
  `t-ok`, `t-bad`, `t-hi`, `muted`. In this case the verdict row *fifteen lines below in the same
  figure* already said exactly that with `t-bad` and `t-ok`, so the values now match the verdicts
  beneath them and the figure's whole claim reads at a glance.
- **Series identity** (06-03's curve labels: which curve is this?). There is no class for the figure
  palette and there should not be one — those five hexes are not the axis colours, and minting
  `.t-green` invites somebody to colour an axis label with it. The answer is the idiom the same file
  already uses twice for its legends: **a coloured shape beside plain text.** The `fill` attribute
  works perfectly on `rect`, `path` and `line`; the rule only targets `text`. In three of the four
  sites a grey leader dash was already there and only had to stop being grey.

The general shape: when a rule blocks you from colouring a thing, check whether the colour belongs
on the thing at all, or on something next to it that is allowed to carry it.

**And the drift recurses, so fix the generator.** The page's source is an `.svg` in `scratch/`, and
the `.svg`'s source is `figs_NN.py`. Editing the HTML leaves the generator able to revert it on the
next rebuild; `check-builders.py --figures` is the check that closes that loop, and it is the one
that proves a figure fix actually landed.

## A page can look complete and still not add up to the engine

`check-builders.py` proves a page is what its builder says, not that it says enough. On 2026-09-26
a replay of the pages — every whole listing in course order, 5.1's move script replayed — found
23 lessons whose commits changed files their pages never listed whole (6.15 added 156 lines of
IBL to `scene.frag.hlsl` and listed only the skybox shaders), so a student following Modules 5–6
could not reach the engine as shipped. `check-continuity.py` now asks the question on every run.
When a lesson changes a file for a small reason, it still lists it whole — the exceptions are
exactly where this went wrong.

## A tagged excerpt is read as the whole file

6.9 captioned a ten-line excerpt of `shadow.cpp` with the `modified` tag outside Code Listings. A
checker keyed on the tag took it for the file, and so would a student pasting it over 372 lines.
Whole files live only in Complete Code Listings; excerpts elsewhere are captioned
`path — what it is` and never carry a bare `new`/`modified` tag.

## A move script is code: it can break a build, and it can be non-portable

5.1's script promised "your files match the repository's byte for byte" and did not: it never
rewrote the line-1 path comments the page said change, left `pong.cpp` including
`"game/pong.hpp"` (a header it had just moved — a broken build at 5.1), and used `sed -i ''`,
which only BSD sed reads as intended. A published script gets a test that RUNS it
(`test_checkers.py`), on the machine's own sed.

## `GIT_INDEX_FILE` leaks into every child `git`, and `git reset` empties a staged index

Staging one change set in a temporary index (`GIT_INDEX_FILE=… git add`) is a clean way to verify
it without disturbing the real one — but EXPORTED, the variable reached the unit tests' throwaway
repositories, and their `git add` overwrote the temporary index. Set it per command, and have
tests that create repositories strip `GIT_*` from their environment. Separately, `git reset -q`
"to unstage one thing" unstaged 1,394 uncommitted files; it was recoverable only because a
`git write-tree` taken earlier had saved the index as a tree (`git read-tree <tree>` restores it).
Snapshot the index with `git write-tree` before any bulk index operation.

## A harness cannot check a claim about somebody else's engine

Module 8's measurements were rigorous and its comparisons were not: "Bullet and PhysX both ship
the gyroscopic term off" (Bullet has shipped it on since 2.83), "Box2D, Bullet and PhysX all ship
`sqrt(a·b)`" (Bullet multiplies, PhysX averages), "1.0 m/s is the number every engine ships"
(Box2D's; Bullet 0.2, PhysX 2 at defaults), "Box2D lands on four manifold points" (it is 2D: two),
and "the coupling nearly every engine ships is 1 − F(v·h)" (Filament does not couple at all). All
were copied into shipped headers and five lessons' listings. Each was a universal resting on one
example, and 8.3's even carried a ⚠ VERIFY for seven lessons. Cite the header and symbol; never
say "every". The authoring guide's §11 now says so.

## Writing an exercise's solution audits the lesson it belongs to

Phase 1.5 of the post-Module 8 review wrote solution sketches for the 113 exercises in the twenty
lessons that had none, and required every number in a sketch to be computed or read from source.
That requirement turned the job into an audit. Fifteen published statements failed it:

- **Wrong mechanisms, found in library source** (`build/_deps/*-src`): 4.6's "pool being exhausted"
  was a 32 KiB per-push overrun; the partial-bind "rule" from 6.8 (and 6.15's prose) was false —
  SDL resets bindings at the end of a pass, not outside a range; 6.18's hint had Dear ImGui cropping
  in vertex data "instead" of scissoring, when its SDL_GPU backend scissors per draw command.
- **Wrong algebra**: 6.16 §3.2 put OpenGL's near plane "a far distance behind the camera"; it lands
  at nf/(2f − n), half the near distance, in front. 7.4.4's hint looked for the largest gap where
  1 − 2s⁴ is most negative, which is exactly where the gap is zero.
- **Wrong numbers in hints**: 4.5.1's "24 or 20" bytes (28 or 24); 6.7.5's "a tenth of a degree"
  for 16-bit octahedral tangents (0.0037°); 7.4.1's "not 180°" (exactly 180° about ŷ); 4.9.6's
  "small win" from Forsyth (34% fewer invocations); 7.8's "200 Hz buzz" (60 Hz).
- **Wrong direction**: 8.5.5 told students to advance by an *upper* bound — the mistake 8.13 then
  made and measured (377 of 6,049 casts inside the skin).
- **Dangling references**: 4.5.3 cited bandwidth figures 4.1 never gave; 6.14.4 cited §6.4 for a
  claim that lives, unmeasured, in §1.

Three of them live in engine comments too, and wait for the next lesson that lists those files
(STATE.md `decisions: pending-code-corrections`). The general rule: a hint is a claim, and a claim
nobody has had to *use* has not been checked. Where a sketch's number came from a model
(`scratch/vcache_49.py`, `oct_67.py`, `isrot_74.py`, `roundtrip_82.py`), the model first had to
reproduce the lesson's own published figure — 2,400 invocations, −99.998217257, 15.99% — before
anything new it said was used. One sketch (8.7.2) contradicts its hint by code reading alone and
says so; it has not been run.

## A widget that renders dashes is not an error, so it can be dead for ninety lessons

Lesson 0.2's pace planner — a slider that turns hours per week into a finish date — lost its
script in commit 225f7b1, when the shared-script stamper was extended and rewrote the region the
widget's code sat in. The page still rendered: the slider moved, the cards said "—", the table was
empty, and there was no console error because there was no code left to throw one. It was found
during the hours re-estimate, only because the planner's numbers needed changing and there was
nothing to change. The rule that page-specific scripts live outside the SHARED-SCRIPT markers
(CLAUDE.md §7) came later and was never applied backwards. `check-curriculum.py` check 14 now
fails if the planner's data is missing, not only if it is wrong — the check for existence is the
one that would have mattered.

