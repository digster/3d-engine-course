# Authoring & Visual Style Guide

How to write a lesson for this course. `lesson-template.html` in this directory is the
canonical skeleton — copy it, fill it in, never restructure it.

Binding spec: [CLAUDE.md](../../CLAUDE.md). Where this guide and the master prompt disagree,
the master prompt wins.

---

## 1. The hard constraints

1. **One lesson = one self-contained `.html` file.** No external assets, no build step, no
   server. It must render by double-clicking it on a machine with no network.
2. **The `<style>` block and the trailing `<script>` block are identical in every lesson.** Both
   are duplicated, not linked, because of constraint 1. `lesson-template.html` is the source of
   truth — change it there first, then run `apply-shared.py` (§14). Never edit one lesson's copy
   in isolation: that is precisely how the highlighter ended up with three different keyword
   lists across six lessons.
3. **The only permitted remote is the KaTeX CDN**, and the page must survive its absence
   (see §5).
4. **All 13 sections of the master prompt's §6, in order.** Not a menu.
5. **Zero placeholders in code listings.** No `// ...`, no "rest of the file as before". A file
   that changed appears whole.

## 2. File naming

```
docs/lessons/NN-MM-slug.html     e.g. 02-07-perspective-projection.html
```

Zero-padded so they sort lexically. Inserted lessons take a `b` suffix — `02-04b-slug.html` —
and **approved lessons are never renumbered**.

## 3. Filling in the template

Search the template for these placeholders and replace every one:

| Placeholder | Becomes |
|---|---|
| `N.M`, `Lesson Title` | The lesson id and title |
| `Module N — Module Name` | The eyebrow |
| `PREV.html`, `NEXT.html` | Real filenames — **in both the top and bottom nav** |
| `N.M-1`, `N.M+1` | Real neighbouring lesson ids and titles |

Then delete any example content that survives (the dot-product worked example, the
`src/gfx/example.*` listings, the specimen pitfalls). They are demonstrations of the house
style, not content.

**Keep the four `SHARED-CSS` / `SHARED-SCRIPT` marker comments.** The template carries both
pairs precisely so a copy of it is already opted into propagation (§14). Delete them and your
new lesson silently stops receiving shared CSS and script updates — `apply-shared.py` will
report it as `skip`, which is easy to read past.

### The `<title>`

`N.M — Lesson Title · Build a Professional 3D Game Engine`. Keep the suffix; browser tabs are
narrow and the lesson id is what identifies it.

## 4. Section-by-section notes

**The Problem.** Never open with the solution. Open with the gap — something the student cannot
currently do — and where a concept exists to prevent an artifact, **show the artifact first**
(pedagogy §5). Z-fighting, swimming textures, gimbal lock: name the symptom precisely, then the
cause, then the fix.

**Building Intuition.** Pictures before symbols. No formalism at all in this section. If you
cannot draw it, you do not yet understand it well enough to write the section.

**The Theory.** Derive; never decree. Every formula is preceded by its intuition and followed by
a `.worked` numeric example. If a derivation is genuinely out of scope, say so explicitly, give
the intuition anyway, and name a reference that finishes the job.

**Implementation.** Short snippets interleaved with prose. This section is for *understanding*;
the Complete Code Listings section is for *copying*. Justify each design decision as it is made,
not in a retrospective summary.

**Exercises.** Two to five, ordered warm-up → open. Hints in one `<details>`, solutions in
another. Full solutions when short; a solution *sketch* — the shape of a good answer and the
tradeoff to weigh — when the exercise is open-ended.

## 5. Math

Wrap display math in `.eq`, and **always** provide the `.eq-plain` twin plus a prose statement:

```html
<div class="eq">
  <span class="katex-src">\[ \mathbf{a} \cdot \mathbf{b} = \|\mathbf{a}\|\,\|\mathbf{b}\|\cos\theta \]</span>
  <span class="eq-plain">dot(a, b) = length(a) * length(b) * cos(theta)</span>
</div>
```

**An equation must never be the only carrier of a fact.** If KaTeX's CDN is unreachable the raw
TeX stays on screen, and the `.eq-plain` twin plus the surrounding prose must carry the meaning
alone. Test this by loading the page offline.

KaTeX's auto-render ignores `pre` and `code` by default, so listings containing `\[` or `$` are
safe from being eaten.

**Verify every worked example's arithmetic before publishing.** §10 makes wrong arithmetic a
correctness bug, not a typo.

## 6. Code listings

```html
<figure class="listing">
  <figcaption>
    <span class="path">src/gfx/raster.cpp</span>
    <span class="tag modified">modified</span>       <!-- or: tag new -->
    <span class="lang" data-lang="cpp">C++</span>
  </figcaption>
  <pre><code class="lang-cpp">// ... source ...</code></pre>
</figure>
```

- Language classes the highlighter knows: `lang-cpp`, `lang-hlsl`, `lang-c`, `lang-bash`,
  `lang-sh`, `lang-cmake`. Anything else renders unhighlighted, which is fine.
- **Escape `<`, `>`, `&` as `&lt;`, `&gt;`, `&amp;` in the source.** The highlighter reads
  `textContent`, so entities round-trip back to real characters when copied. `#include <vector>`
  is written `#include &lt;vector&gt;` and copies out correctly.
- Add `class="shell"` to the figure for terminal transcripts.
- **Code never wraps.** It scrolls horizontally. Wrapped code lies about its structure.
- Every listing must compile at its point in the course.

The highlighter is ~60 lines of embedded vanilla JS rather than highlight.js from a CDN, because
colour is not worth a dependency that can be absent. It reads `textContent` and rebuilds escaped
HTML, so the worst it can do is mis-colour a token — it can never corrupt a listing or execute
page content.

It lives in the shared `SHARED-SCRIPT` region (§14), so **add a keyword by editing the template
and re-stamping**, never by patching the lesson you happen to be writing.

Two things to know if you touch its word lists:

- `CPP_KEYWORDS` is consulted **before** `CPP_TYPES`, so a word in both renders as a keyword.
  Fundamental types (`bool`, `char`, `int`, `Uint32`, …) belong in `CPP_TYPES` **only**.
- The shell rule treats `::` as a Windows-batch comment **only at the start of a line**, because
  `::` is also a CMake target separator — an unanchored rule turns
  `cmake --build . --target SDL3::SDL3 --config Release` into a comment from `::` onward.

## 7. Diagrams

Inline SVG, authored by hand. Nothing else.

```html
<figure class="dia bleed">
  <svg viewBox="0 0 640 300" role="img" aria-labelledby="figN-t figN-d">
    <title id="figN-t">Short title</title>
    <desc id="figN-d">What a reader who cannot see it needs to know.</desc>
    ...
  </svg>
  <figcaption><span class="fignum">Figure N.</span> Caption.</figcaption>
</figure>
```

### Rules

- **Every diagram has a caption and is referenced from the body text** ("as Figure 3 shows").
  A diagram the prose never points at is decoration — cut it or reference it.
- **Never hard-code colours.** Use the classes below so diagrams survive dark mode.
- `viewBox` + no `width`/`height` — the CSS makes it fluid.
- Always include `<title>` and `<desc>`.
- Place each diagram at its exact point of use. Never dump them at the end of a section.

### Class vocabulary

| Class | Use |
|---|---|
| `ax-x` / `ax-y` / `ax-z` | Axis **strokes** — red / green / blue |
| `lbl-x` / `lbl-y` / `lbl-z` | Axis **label fills** |
| `ink` / `ink-soft` | Neutral strokes, primary / secondary |
| `grid` | Graph paper |
| `fill-soft` | Neutral shape fill |
| `hi` | Accent stroke for the thing under discussion |
| `mono` | Monospace text (numbers, code) |
| `sm` / `xs` | 11px / 9.5px text |
| `muted` | Secondary text fill |

Arrowheads need a `<marker>` per colour — markers do not inherit the referencing element's
stroke. Give the marker's `<path>` an explicit `fill="var(--axis-x)"`; CSS variables work inside
markers, so dark mode still tracks.

### Required genres

Where relevant: coordinate-space chains **annotated with actual numbers**; before/after
transformation pictures; view frustums; memory layouts (vertex interleaving, ECS storage,
allocator arenas); pipeline and architecture flowcharts; light/BRDF geometry.

**Minimums:** any spatial or mathematical concept gets at least one diagram. Core geometry and
lighting lessons typically want three to six.

## 8. Colour conventions — never violate

| Meaning | Token |
|---|---|
| x axis | `--axis-x` **red** |
| y axis | `--axis-y` **green** |
| z axis | `--axis-z` **blue** |

Course-wide, every diagram, no exceptions. Declared once on
[`conventions.html`](../conventions.html). A reader must be able to identify an axis by colour
alone, in any lesson, without a legend.

## 9. Callouts

| Class | For |
|---|---|
| `note` | An aside worth knowing |
| `warn` | The failure mode; a trap |
| `verify` | **`⚠ VERIFY:`** — an unconfirmed API detail (see §11) |
| `pitfall` | A mistake students reliably make |
| `ok` | A checkpoint; a confirmed-good result |
| `cpp` | A **C++ feature taught in place**, the first time it appears |

Use `cpp` for RAII, ownership, move semantics, references vs pointers, templates,
`const`-correctness, headers and translation units — each with a short dedicated explainer at
the point of use, never a forward reference to a chapter the student has not read.

## 10. Voice

Warm, precise, rigorous. Never condescending.

- **"It just works this way" is banned.** So is hand-waving.
- Anticipate confusion and meet it head-on: *"you might be wondering why we need this fourth
  coordinate — hold that thought, because in two paragraphs it will feel inevitable."*
- Bugs and artifacts are teachers, not embarrassments.
- **When you simplify, say so:** *"this is the ninety-percent picture; the missing ten percent
  is X, and we cover it in lesson Y."*
- **Length serves depth.** 3,000–7,000 words excluding code is typical; foundational lessons
  (projection, PBR, quaternions, ECS design) may run well past that. Never pad, and never
  truncate to save space. Depth is the product.
- The student should never type a line they could not explain.

## 11. Accuracy

**SDL3 correctness is paramount.** SDL3 signatures only — never SDL2 idioms. Never invent a
function signature, struct field, enum name, or toolchain flag.

Where you are not certain, flag it rather than guessing:

```html
<div class="callout verify">
  <span class="label">⚠ Verify</span>
  <p>⚠ VERIFY: field order of <code>SDL_GPUTextureCreateInfo</code> against
     <code>SDL3/SDL_gpu.h</code>. The conceptually correct usage is …</p>
</div>
```

**An honest flag beats a confident fabrication** — the student is about to type this in.

The fastest way to settle a question is the header itself, which leads the wiki:

```sh
curl -sL https://raw.githubusercontent.com/libsdl-org/SDL/main/include/SDL3/SDL_gpu.h \
  | grep -n -A 12 "typedef enum SDL_GPUFrontFace"
```

Record anything surprising in [LEARNINGS.md](../../LEARNINGS.md) so it is settled once.

## 12. Pre-flight checklist

Run silently before emitting any lesson (master prompt §11):

- [ ] Every objective actually covered?
- [ ] Every formula preceded by intuition and followed by a numeric example — **arithmetic verified**?
- [ ] Diagrams present, labelled, captioned, and **referenced from the text**?
- [ ] Implementation narrated incrementally **and** full listings complete with **zero placeholders**?
- [ ] Manifest table, per-platform build/run commands, and expected result present?
- [ ] Pitfalls (symptom → cause → fix) and 2–5 exercises included?
- [ ] Conventions consistent with `conventions.html`?
- [ ] Prev/index/next correct in **both** navs, and the STATE block updated?
- [ ] Does the length serve depth — nothing padded, nothing truncated?
- [ ] Every code listing compiles at this point in the course?

## 13. Verifying a page

```sh
# From docs/ — a plain static server is enough; there is nothing to build.
python3 -m http.server 8000
```

Check: light **and** dark; the page offline (kill the network — equations must stay legible);
code blocks scroll rather than wrap; the reading measure stays ~70–80 characters; and every
inter-page link resolves.

**Use a real browser (Playwright/Chromium), not an embedded preview pane.** The preview pane
reports computed styles that the page cannot actually produce, so a broken theme or a dead
highlighter can look fine there. Serve over HTTP and drive Chromium.

A good highlighter check is a *round-trip* rather than an eyeball: for every
`.listing pre code`, the live `textContent` after highlighting must equal the `textContent` of
the same element parsed with `DOMParser` (which never runs scripts). Any difference means the
tokeniser corrupted a listing.

## 14. The shared CSS and script regions

`apply-shared.py` propagates two blocks from `lesson-template.html` into every page that opts in
with a marker pair:

```
<!-- SHARED-CSS:BEGIN -->      …stylesheet…      <!-- SHARED-CSS:END -->
<!-- SHARED-SCRIPT:BEGIN -->   …page script…     <!-- SHARED-SCRIPT:END -->
```

```sh
python3 docs/_template/apply-shared.py           # stamp every page
python3 docs/_template/apply-shared.py --check   # verify only; exit 1 on drift
```

**This is not a build step.** Readers never run it; the published files are complete and work by
double-clicking. It is an authoring-time tool, like a formatter. Run it after editing the
template, and run `--check` before committing.

The canonical copy of each region is read from between that same marker pair **in the template
itself**, so the template carries every marker a page does. That is what makes "copy the
template to start a lesson" produce a page that is already opted in — see §3.

The regions are independent — a page may opt into either, both, or neither. `index.html` and
`math-toolbox.html` carry the script region even though they have no listings today, so the
highlighter is simply dormant until they gain one.

**Page-specific JavaScript goes *outside* the markers.** Lesson 1.2's key-state widget sits in
its own `<script>` earlier in the body and is never touched by the stamp.

`--check` also lints for `fill="…"` on an SVG `<text>`: the shared stylesheet's
`figure.dia svg text { fill: … }` always beats a presentation attribute, so an inline fill is
silently ignored. Use the classes in §7 instead.
