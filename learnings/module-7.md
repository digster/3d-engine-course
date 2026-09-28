# Module 7 — learnings from its lessons

Moved verbatim from LEARNINGS.md on 2026-09-27, in the original order; LEARNINGS.md
is now the index. Append new sections at the end, and add each heading there.

---

## Cancellation against 1.0: the same bug three times in one lesson

Lesson 7.1 shipped three formulas that are algebraically exact and numerically useless, and it took
meeting the third one to see they were the same bug:

| Formula | Job | What it does instead |
|---|---|---|
| `sqrt(1 - sin*sin)` | `cos(pitch)` in `euler_from_rotation` | returns **exactly 0** at pitch 89.99°, so the gimbal-lock flag fires 0.01° early |
| `sqrt(1 - fabs(sin))` | smallest singular value of the rate Jacobian | same, and the reciprocal prints `inf` at a pose that is fine |
| `acos((trace(R) - 1) / 2)` | angle between two rotations | **1.00 relative error** at 0.004°, i.e. returns zero — and NaN on identical inputs, unclamped |

The mechanism is one sentence: **a `float` cannot hold a change of 1.6e-5 at 3.0, or of 1.5e-8 at
1.0**, so a quantity computed by subtracting two nearly-equal numbers near 1 is gone before the
`sqrt` or the `acos` sees it. Then the outer function amplifies whatever noise is left, because
`acos` and `asin` both have infinite slope at ±1.

The cure is always the same shape: **get the small quantity from something that is itself small.**

- `cos(pitch)` = `hypot(r02, r22)`, because those two entries *are* `sin(yaw)·cos(pitch)` and
  `cos(yaw)·cos(pitch)` — their length is what you want, and no subtraction happens. The pairing is
  worth remembering on its own: **the conditioning of the yaw extraction is the length of the vector
  whose angle the yaw extraction takes.**
- `sqrt(1 - s)` = `|c| / sqrt(1 + s)`, from `1 - s = (1 - s²)/(1 + s) = c²/(1 + s)`.
- The rotation angle comes from `atan2(|R - Rᵀ|/2, (tr R - 1)/2)`: the antisymmetric part's entries
  are *differences of matrix entries*, so they stay proportional to θ instead of hiding inside a 3.

**The diagnostic to carry:** if a formula's job is to report something near zero, look at what it
computes just before it returns and ask whether *that* is ever small. If it is a difference of two
things near 1, the formula cannot do its job and no amount of `double` will save you at the limit —
it only moves the cliff.

**And watch the drift before the cliff.** The naive singular value reads 0.0012449 against a true
0.0012340 at pitch 89.9° — 0.9% wrong while still entirely plausible. The cliff is what you notice;
the drift is what ships.

## A one-knob Euler interpolation is a geodesic, so the obvious test measures nothing

The first version of 7.1's interpolation check swept the yaw alone at pitch 88° — as close to the
singularity as it could get — and correctly reported **0.00% excess turning**. Turning one Euler
angle and leaving the others fixed is a steady rotation about one fixed axis, which is the
definition of a geodesic, whatever the pitch. The pathology needs **at least two** angles moving.

Two things follow. Aim a demonstration at two or three knobs, and control it by running *the same
deltas* at four distances from lock (7.1 measured 209.5%, 62.5%, 22.5%, 26.7%) rather than by
changing the deltas, or the comparison is measuring the move rather than the pose.

## `grep` a name, get somebody else's Euler

`grep -rn euler demos/common` returns two hits and neither is Lesson 7.1: they are the **Euler
characteristic**, V − E + F, printed by Lesson 3.5's mesh validator. Two unrelated things named
after the same man, in the one directory a structural argument was about. A dependency claim made
by grepping a *name* rather than an *include path* would have invented a dependency that is not
there. Grep the include, then confirm it by what the translation unit actually compiles.

## A headless run can write the right file and then crash

`demos/gimbal --shot` exited 139 (SIGSEGV) *after* printing its receipt and writing a correct PPM.
Cause: a `--shot` run has no window, so ImGui has no context, and `ImGui::Begin` on no context
segfaults. `debug_ui`'s own entry points are all safe when it never started — 5.11 made them so
deliberately — but a panel built by hand *between* those calls is not covered by that guard.

The general point is about evidence: **every artifact on disk being correct is not evidence that
the program succeeded.** Check exit status separately, in scripts and by eye. This is the same
three-independent-axes rule `check-builders.py` learned on 2026-09-12 (exit status, output
existence, output equality), met from the other side.

## A figure defect can be invisible at the width you authored it

Figure 1 of 7.1 had two labels sitting on the dashed ground square. `check-page.js` reported them at
1280 and **not** at 390, because SVG labels scale with the viewport and the collision only opened at
the wider one. Run the checker at both widths every time; "it looked fine" is a statement about one
viewport.

The fix is also worth the note: the labels were correct and the *shape* was too big. Shrinking the
ground square inside every label's radius fixed four placements at once, where moving four labels
would have been four chances to create a new overlap.

## A path in a doc comment is indistinguishable from a path in an `#include`

Lesson 7.1 landed the rule *"grep the include path, not the name"*, after `grep -rn euler` found
the Euler characteristic. Lesson 7.2 found the next rung: `grep -rln "math/euler.hpp"` reports
`math/transform.hpp` and `math/rotation.hpp` as includers, and **neither one includes it**. Both
mention the path inside a doc comment — this codebase's comments are dense enough that half the
hits for any header path are prose.

Two fixes, and take the second:

1. Match the directive: `grep -rln "^#include <engine/math/euler\.hpp>"`.
2. **Walk the graph transitively** from the actual roots. Fifteen lines of Python, and it also
   catches a header reached through two others, which a one-level grep cannot see at all — the
   more dangerous of the two errors, because it produces a *false negative* on a real dependency.

The golden's structural argument is now computed rather than asserted: from the six engine headers
`demos/common/demo_scene.cpp` includes, the reachable math headers are exactly `mat3`, `mat4`,
`transform`, `vec2`, `vec3`, `vec4`. Anything else in `math/` cannot move the reference render, by
construction.

## Build the golden into `build/demos/`, not `build/`

`golden_72` reported `identical=NO` **with the correct byte count**, because the binary sat one
directory higher than the other harnesses, the asset search path failed to find `torus.obj`, and
two of the eight shots drew nothing at all. The output was the right size and the wrong picture, so
a size check passes and only the byte comparison catches it.

**The working directory is part of the instrument.** A golden that fails because an asset did not
load looks exactly like a regression, and the log line that explains it (`mesh 'torus.obj' is in
none of the 1 root(s)`) scrolls past above the verdict. Read the whole output, not the last line.

## A timing loop measures the compiler unless you stop it twice

One lesson, two independent eliminations, both producing *plausible* numbers:

- **Partial dead-code elimination.** Reading `.c1.y` of a returned `mat3` let the compiler compute
  one element of nine. A 3×3 product "timed" at **0.41 ns** — about one cycle for 27 multiplies and
  18 adds. Cure: consume the whole result (sum all nine entries).
- **Loop-invariant hoisting.** A fixed input array meant every repetition asked the identical
  question, so the compiler computed 256 answers once and replayed them 4,000 times. A `volatile`
  sink stops the work being *deleted*; it does not stop it being *cached*. Cure: index the inputs
  with the repetition counter so the million calls are a million different questions.

The sanity test is free: **divide the reported time by the operation count and ask whether the
answer is physically possible.** And take the *minimum* of several passes, never the mean — a
timing is a lower bound contaminated by interruptions that can only slow it down. Before that
change the same spelling measured 4.20 ns and 9.87 ns in two runs.

## Not every catastrophic cancellation is a bug — measure what reaches the output

Lesson 7.1's rule: *if a formula's job is to report something near zero, look at what it computes
just before it returns and ask whether that is ever small.* Lesson 7.2 found the other half.

The coefficient `(1 − cos θ)/θ²` has exactly 7.1's shape and is destroyed exactly as predicted —
relative error **1.000** at θ ≤ 1e-4, returning zero where the answer is one half. The matrix it
builds is wrong by **one float ULP**, at every angle, because the term it scales shrinks as `θ²`
precisely as fast as the coefficient's error grows.

The pattern is identical in the bug case and the benign case. Only the measurement separates them,
so **track the error to the number the caller actually receives**. (Keep the well-conditioned
spelling anyway: "the error cancels downstream" is a property of today's call sites, not of the
function.)

## Print the ratio, not the verdict

Comparing two algorithms across nine probes with a "winner" column produced the summary *"the
crossover is between 120° and 110°"*, which is not an interval. The two were **tied** across a wide
band and the winner was flipping on noise; a ratio column says that immediately and a verdict
column cannot say it at all.

Related: **the engine cannot make this measurement about itself.** It runs one of the two routes
per call, chosen by a threshold, so the harness has to carry its own copy of both. You cannot find
where two curves cross by plotting one of them — and duplicating engine code in a harness, normally
a smell, is here the only way the question can be asked.

## The figure sampler keeps the brightest pixel, so a thin feature over a bright surface vanishes

`peak_sample` takes each 3×3 block's brightest pixel — right for thin bright lines on a dark ground
(which is what it was written for) and wrong when a pale one-pixel trail crosses a white fuselage:
the fuselage wins every block and the trail disappears from the figure while being perfectly
visible in the PPM.

Two consequences worth keeping:

- **Do not draw anything solid in a panel whose subject is thin lines.** Removing the opaque
  aircraft from the blend figure fixed it, and made the better picture — nothing in it is not the
  comparison.
- **Dashing does not survive the 3× downsample.** The sampler fills a two-pixel gap straight back
  in. *Width* survives where dash does not: draw the line three times at small offsets.

## Figure filenames follow page order, and nothing checks it

`check-page.js` verifies geometry, `check-builders.py` verifies reproduction, `check-curriculum.py`
verifies links — and **none of them can catch a figure numbered wrong**, because every figure still
renders and the page is still valid. Lesson 7.2's figures 8 and 9 were written in the opposite
order to the page and had to be swapped by hand. Check the placeholder order against the generator's
filename list before building.

## A wide equation is scrollable, not clipped — so nothing reports it

`.eq` is `overflow-x: auto`. A rendered KaTeX display that is too wide gets its own scrollbar, so
`check-page.js` passes, `pageScrollsX` is false, and the reader sees a formula that stops
mid-symbol with **no visible affordance** — macOS hides overlay scrollbars until you scroll. Two of
Lesson 7.3's equations were over-wide at 1280 and only a screenshot found them.

`scratch/tools/eqfit.py` measures `scrollWidth` against `clientWidth` on every `.eq`. The fix is to
split the equation with `\begin{aligned}`, not to widen anything. **At 1280 nothing should
overflow; at 390 the whole corpus does** (7.2 has 15, 7.3 has 12) and that is the accepted mobile
behaviour, matching tables and listings.

## A preview page that does not load the stylesheet lies about everything

`scratch/_preview73.html` linked `docs/shared/course.css` from inside `scratch/`, which resolves to
`scratch/docs/shared/course.css` and 404s. Every figure in Lesson 7.3 was reviewed **unstyled** —
16px serif instead of 9.5px — and `check-page.js` duly reported 37 text overlaps that did not
exist, which sent a fixing pass after imaginary bugs.

It says so in its own output: **`sharedCssLoaded: False`**. Read that line before reading the
findings. From `scratch/`, the link is `../docs/shared/course.css`.

## Never put a side effect in a C macro's argument

`SDL_clamp(x, a, b)` expands to `(((x) < (a)) ? (a) : (((x) > (b)) ? (b) : (x)))` — **three
evaluations of `x`**. Written as `SDL_clamp(SDL_atof(argv[++i]), 0.0f, 1.0f)` in the plane demo's
argument parser it advanced `i` three times, swallowed `--shot` and its path, and a headless run
opened a real window and hung forever. It had also been running in the wrong mode for two captures
before anyone noticed.

`std::clamp` is a function and evaluates once. One named local per argument, always — and note
that `demos/gimbal` used `std::clamp` and never had this bug.

## Choose the quantity your check looks at before you choose the threshold

`renormalised_fast(z)` is `z * (3 − |z|²)/2`. At `|z| = 2` the factor is `−0.5`, which produces a
result of modulus **exactly 1.000000** — a perfect score on the obvious test — while turning the
rotation **180° the wrong way**. A unit test on `|z|` certifies a function that reverses the object.

A rotation is not its modulus. This is not a tolerance that was too loose; it is the wrong
*observable*. Related: a percentage that cannot physically be negative deserves an assertion saying
so — comparing radians against degrees reported "−98.25% excess turning", a journey shorter than
the shortest journey, and a four-character check would have caught it before the figure was drawn.

## A serial dependency chain can eat an entire optimisation

Replacing `cos`/`sin` per point with one complex multiply per point should be ~6× by operation
count. Measured: **1.21×**. `z = z * step` makes each multiply wait for the previous one to retire,
so the loop measures **latency**, while the trig loop computes every point from its own index and
measures **throughput**. Four interleaved chains: **3.52×**.

Counting operations cannot see this and neither can reading the code. Two corollaries: inherited
performance advice is a hypothesis (a `float` `sin`+`cos` pair is ~1.85 ns of throughput on an M4,
not the hundred cycles the folklore assumes), and a timing harness needs a **warm-up pass** — the
first measurement of a freshly built binary reads 15–30% slow from cold caches and first-touch page
faults.

## The figure palette must contain the demo's own colours

`rle_rects` snaps every sampled pixel to the nearest palette entry. A colour the palette does not
contain comes back as whichever entry is nearest — Lesson 7.3's first render turned the demo's light
blue mirror grey and its amber ticks khaki, so the figure disagreed with its own caption.
Transcribe the constants from the demo rather than approximating them.

And the demo's **grid** must sit below `figs_511.peak_sample`'s floor of luminance 30,000. Graph
paper at `(38, 42, 52)` clears it, survives the 3:1 downsample, and buries the construction inside
a strong grid. `demos/plane` draws its grid at `(24, 26, 32)`: still legible on a screen, gone from
a capture.

## Nothing checks a documentation page's own table of contents

`docs/conventions.html` has listed §8b since Lesson 7.1 and never listed §8c, because 7.2 added the
section and not the link. `check-curriculum.py` verifies that every href **resolves** — it has no
idea that a heading exists with no entry pointing at it. Found by hand, eleven days later.

## The page quotes the program, so narrow the program

Transcript `<pre>` blocks scroll and never wrap, and the fold at 1280px is about **66 characters**.
Fourteen of Lesson 7.3's quoted lines lost a *number* past it — including `C.1`'s
`worst element diff 0.000e+00`, which is the exactly-zero claim the section is about.

Fix it in the harness's `printf` widths, not in the prose: the program is then also readable in an
80-column terminal, which is the reason that matters to anyone not writing the page.

Two tools now enforce it. `scratch/tools/prefit.py` reports transcript blocks wider than their box.
`scratch/tools/retranscribe.py` rewrites each quoted block from `verify_NN.log`, **matching on the
stable `[PASS] X.N` check ids** rather than on a printed line — because narrowing the `printf`s left
fourteen blocks quoting output the program no longer produced, and **stale is worse than wide**.

Related: `pre class="output"` has no rule anywhere in `course.css`. Lesson 7.3 invented it and was
the only page using it; it rendered identically to a plain `<pre>`. `check-page.js`'s
`unknownTagClasses` check only covers `.listing figcaption .tag`, so nothing caught the dead class.

## A failure mode does not carry up a dimension — it is a hypothesis about the new one

Lesson 7.3 found `renormalised_fast` at `|z| = 2` returning `−z`: modulus *exactly* 1.000000, and
the object turned **180° the wrong way**. Its conclusion — *test the rotation, never the modulus* —
was right and is still right.

The same function, one dimension up, is **exact at `|q| = 1` and exact again at `|q| = 2`**, because
it returns `−q` there and `−q` *is* `q` as a rotation. The damage is in the middle: 34.7° of pose
error at `|q| = 1.5`, 49.9° at 1.75. **The failure is not monotonic**, so a test sampling the two
obvious points certifies a broken function completely.

One level up from 7.3's lesson: that one said *choose the observable*. This one says *choose the
sample points* — and says that a failure mode inherited from a simpler case is something to
**re-measure**, not something to carry.

## `acos` of a near-unit dot product is a blind instrument

Third time in this course, and the third disguise. Lesson 7.1 §6.4 found
`acos((tr R − 1)/2)` returning **exactly zero** for a 0.004° turn. Lesson 7.2 found the axis
extraction dividing by a difference of near-equal entries. Lesson 7.4 found a *test* doing it:
comparing two recovered axes with `acos(|a·b|)` printed `0.000e+00` in **every row**, which reads
as a pass and is a broken measurement.

An axis error of `5.7×10⁻⁵` rad puts the dot product at `1 − 1.6×10⁻⁹`, which rounds to exactly
`1.0f`. Measure the **chord** `|a − b|`, which is of the size of the answer, and turn it back into
an angle with `2 asin(chord/2)`.

**The general shape:** a quantity obtained by subtracting near-equal numbers at 1 is gone before the
inverse trig function sees it, and `acos`/`asin` then amplify what is left, having infinite slope at
±1. It applies to instruments as much as to engine code, and an instrument that fails this way
reports success.

## A timed loop that overwrites its accumulator measures nothing

`acc = qs[i] * qs[j]` inside a loop reported **0.000 ns**: only the last iteration's product is
needed and the compiler knows it. Third distinct way to lose a timing here, after Lesson 7.2's
"read one element of nine" and "hoist the whole loop out".

Exactly zero is at least **loud**. The dangerous version of this mistake is the one that leaves a
plausible small number behind, which is why the rule is *accumulate every component of every
result*, not *use the result somewhere*.

And state the bias that rule creates: accumulating a `quat` is 4 adds and a `mat3` is 9, so a
compose comparison carries five extra adds on the matrix side. Pair it with a row where both sides
accumulate the same number of floats, and use that row as the calibration.

## A narrower type finds existing bugs, not only future ones

`transform::rotation` is a `mat3`. `gfx/renderable.cpp` assigns `linear_of(w.matrix)` to it — the
upper-left 3×3 of an ECS world matrix, which carries the **scale** that came down the hierarchy —
and writes `1` into the field named `scale`. Its comment says the recomposition reproduces the
original affine matrix *to the bit*, and it does, **because a `mat3` will hold anything**.

That line has been correct, tested and shipping since Lesson 5.11. Narrowing the field to a
quaternion turns it into a compile error.

The tell was in the comment all along: a "to the bit" guarantee on a conversion that *narrows* is a
guarantee that nothing was narrowed. Read exactness claims on lossy conversions as symptoms.

## A render figure on the page's own panel loses the program's colours

Every screenshot figure in `docs/` sat on `.fill-soft`, which is `--dia-fill` — near-**white** in
light mode. That is fine for a lit solid and destroys a thin bright line, and the demos draw gold
and teal on near-black because that is where those colours read. Lesson 7.4's double-cover figure
had its gold dial hand — *the entire content of the figure* — at about **1.8:1** against the panel.

The fix is a CSS class, `.fill-shot`, with **one** value rather than a light and a dark: it is a
photograph of a program, and the program's background does not change when the reader flips themes.

The alternative — darkening the demo's palette to suit the page — was rejected, and the reason is
7.3's "transcribe the constants" rule one level up: **the figure must not disagree with the
program**, in either direction.

## A dial drawn in a fixed world plane is an ellipse

And an ellipse cannot be read with a protractor held against the screen, which is the whole point of
putting one next to an object whose rotation you cannot measure by eye. Build it in the plane facing
the camera.

Then get the basis handedness right, because this one is invisible: `cross(up, toward)` points
**left**, so the first version had every angle mirrored *and* upside down — 135° where the answer is
45°. The picture looked entirely plausible. **The printed receipt caught it, not the eye**, which is
the argument for a demo printing its own numbers even when it draws them.

## A numerical routine has a scale, and its name does not say so

Lesson 7.4 wrote `angle_between(quat, quat)` as `2·acos|a·b|` — the formula every reference gives,
correct, and fine for the question it was written for: *how far apart are these two poses?*

Lesson 7.5 asked the same function a different question: *how far apart are these two poses that are
one four-thousandth of an arc apart?* — four thousand times, and summed the answers. The cosine is
**quadratic** at its maximum where the sine is linear, so every step landed inside the `float` ulp
below 1, every reading was quantised **downward**, and the sum came out at **−41.10%** of the
straight-line distance between the endpoints. The metric failed slerp for not walking the path it
was walking.

The function did not change. **The question did**, and nothing in the name, the signature or the doc
comment marked the difference. The repair is the form `axis_angle_from_quat` — eleven lines above it
in the same file — already used *and explained*: `2·atan2(|v|, |w|)`.

Re-derive a routine's conditioning at every scale you reuse it at. Fourth appearance of this trap in
Module 7 and the first one inside the engine rather than in a harness.

## A NaN never wins a maximum

`std::max(x, NaN)` returns `x`. Every comparison against a NaN is false, so `a < b ? b : a` hands
back `a`. A table of worst-case errors accumulated with `std::max` therefore printed a clean
`0.0000e+00` for a function that was returning NaN in **404 of 505 samples** — and the one row it
printed correctly made the broken function look *better* than the working one.

**Count non-finite results; never let one into a maximum.** The same applies to `std::min`, to a
running sum (which at least goes NaN and announces itself), and to any comparison-based reduction.

## "Finite" is not "right"

`gltf_view`'s decomposition replaced a zero-length basis column with the *parent's* axis, and its
comment said — correctly — that this "keeps the matrix finite instead of producing NaNs". It does.
It also leaves a basis whose columns are not perpendicular, and `quat_from_rotation` of a
non-orthonormal basis is wrong in **every** column, not just the bad one: 0.399 of absolute entry
error on a flattened object whose other two axes were perfectly recoverable.

A guard that returns a plausible wrong answer is harder to find than the NaN it prevented, because a
NaN spreads and announces itself and a wrong basis just looks like the artist scaled something
oddly. When you write a guard, say what the *right* answer is at that input — here, the cross
product of the two surviving columns, six multiplies.

## `T · R · S` per node is a restriction, not a representation

A `transform` composes `translate · rotate · scale` with the scale **innermost**, so a single node
can express "rotate then scale" and cannot express "scale then rotate". Those are different matrices
whenever the scale is non-uniform, and the difference is a **shear**.

`demos/collector`'s camera boom needed exactly the second one — `S⁻¹ · Rz(−bank)`, because the
inverse of a product reverses its order — and stored it in a field called `rotation`, which a `mat3`
accepted without complaint from Lesson 5.12 until the field became a `quat`.

**The fix for a matrix outside the set is another node, not a wider field.** Put the unscale in one
entity and the unroll in its child; the hierarchy multiplies them parent-first, which is the order
the derivation asked for, and the result is identical to 0.000e+00. Real engines split nodes for
exactly this reason, and this is it.

## A figure's number is the page's, not the file's

Two of Lesson 7.5's figures were drafted in the opposite order to the page, so their *filenames* had
to be swapped to match the numbers a reader sees. One cross-reference written **inside** a figure
still pointed at the old number, and nothing in the pipeline can catch that: every figure still
renders, the reference still reads as a sentence, and it points at the wrong picture.

`check-page.js` catches spill, overlap and text-on-shape. It cannot read. Grep the figure sources
for `[Ff]igure \d` whenever a number moves.

## A flag reaches the translation units it is on, and no further

`cmake -S . -B build` leaves **`CMAKE_BUILD_TYPE` empty**, which means `libengine.a` is compiled
with no `-O` flag at all. A harness's own `-O2` covers `verify_NN.cpp` and nothing it links, and an
unoptimised static library links exactly as quietly as an optimised one.

Lesson 7.6 was the first Module 7 harness to link the library — 7.1 through 7.5 test header-only
code — and its first timings were **153.90 ns/vertex** where the truth is 8.51, and **26.32 µs** per
matrix palette where the truth is 0.99. 18.1× and 26.6×, on the same machine in the same minute.

ARCHITECTURE.md §6's rule ("`-O2`, never a debug build") was written when a harness *was* the whole
program, and had never reached what one links. The tell was physical implausibility: 153.9 ns is
about five hundred cycles to do four multiply-adds.

**Any harness that times code living in the library must configure a build type**, and the harness
should say which archive it linked and at what type rather than leaving it to be remembered:

```sh
cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release
cmake --build build-rel --target engine
```

## Ask what a control would do if the thing under test were completely broken

Lesson 7.6's §G control skinned normals as *points* — the translation column wrongly applied — and
reported **0.020°** of tilt, which reads as "the two rules basically agree". It ran at the **bind
pose**, where every palette matrix is the identity, so its translation column is zero and the wrong
rule gives the right answer. The check was passing because its fixture could not fail. Posed, the
same control reports **156.108°**.

That is the second instance in two lessons of the same shape — 7.5's §G table printed a clean
`0.0000e+00` for a function returning `NaN`, because `std::max(x, NaN)` returns `x`. The two
together sharpen into one question to ask of every control: **what would this fixture do if the
thing being tested were completely broken?** If the answer is "pass", the fixture is the bug.

## An instrument that needs an axis can be fooled by an orientation

"How much did this ring shrink" took three attempts, and the first two both measured the radius
*perpendicular to the chain* — which sounds obviously right, since a ring on a bent limb is not
perpendicular to anything else.

Version one had no segment through the tip ring (it is weighted entirely to the last joint) and fell
back to world `+y`; on a limb bent 90° that ring is edge-on, and it read **0.241 instead of 0.380**
— a pinch reported where there are not two joints to pinch between. Version two clamped to the last
real segment and left a smaller copy of the same error: the tip ring is rigidly rotated one joint
angle further than the segment below it, so its plane is tilted against the measuring axis, and at
120° it read 0.364.

The fix is to stop needing an axis. A rigid rotation does not change the distance between two points,
so a distance from the ring's **own centroid** is invariant under every rotation the skeleton can
apply. **Both wrong versions reported a smaller number**, which is the direction the measurement was
hunting in — so both of them looked like a discovery.

## A prediction that survives two different deformations is about the mechanism

`r cos(δ/2)` matches a *bend* to six figures and a *twist* to six figures, on different geometry
(a twist collapses a ring uniformly; a bend flattens it into an ellipse whose minor axis is the same
cosine). One formula that is right about both is a claim about the **blend**, which is what it said
it was. One that is only right about the case it was derived on is a curve fit.

Corollary for the instrument: **measure the minimum radius on a ring, not the mean.** The minimum is
the minor axis in both cases, so one number reads the same law in both modes; the mean would be a
statement about the shape of an ellipse.

## Check a chain against code that shares nothing with it

Lesson 7.6 builds a skeleton's inverse bind matrices by composing exact per-node inverses, and the
claim is that this equals inverting the composed matrix. Checking it against a rearrangement of the
same per-node inverses would prove nothing at all, so the harness carries a **general 4×4
Gauss-Jordan inverse with partial pivoting, written from scratch in `double`**, forty lines, sharing
no line with the engine. Worst entry difference at depth 8: `4.768e−07`.

The fixture matters as much as the control. A joint chain with identity rotations would have tested
the easy half of `local_from_parent` — its inverse is a pure negation and never exercises the
transpose — so the chain has an awkward rotation on every joint and a non-uniform scale on one.

## The figure quantiser's background test is per-channel

`figs_45.py`'s `quantise` emits **nothing** for a pixel — treats it as background — only when red,
green **and** blue are each below 14. A demo clear colour of `(12, 13, 17)` is darker than the
house `(16, 18, 24)` on two channels and fails on the third, so the background goes through the
quantiser like any other colour and comes back at `(79, 84, 98)`: a mid-grey slab with the subject
barely visible on it. `(11, 12, 13)` is background; `(12, 13, 17)` is not.

A second, unrelated artifact in the same pipeline: `rle_rects` butts its rectangles edge to edge,
and at browser scale that shared edge is antialiased from both sides, so a solid shaded surface
picks up a hairline of the panel behind it every row or two. It reads as quantisation banding and is
a rasterisation seam. Wrap the panel in `<g shape-rendering="crispEdges">` **in the lesson's own
`figs_NN.py`**, never in `rle_rects`, which every render figure since Lesson 4.5 depends on.

## A benchmark with two variables in it has none — and fixing one confound proves nothing

Lesson 7.7 tried to measure what a clip's loop wrap costs a sampling cursor. The first attempt
varied the wrap period by changing **the driver's step size**, so each lookup on the "more wraps"
row also crossed ten key intervals instead of one — two variables, one number, and it confidently
reported 43% for the wrong cause. The fix is to hold everything but the one thing: advance
**exactly one key interval per lookup** and let only the track length differ.

The second half is the part worth remembering. A separate timing in the same lesson reported a
301-key clip as **faster to sample than a 31-key one**, which contradicts arithmetic. The first
cause found was a playback clock that climbed to 666 seconds (see below). Fixing it left the
inversion in place, because there was a *second*, independent cause: the benchmark was the first
timing in the process and a laptop's first hundred milliseconds run about **30% slower** than
everything after them. `best_of_three` cannot see past that — all three attempts are inside the
ramp — so the harness now spins for 150 ms before it measures anything.

Two independent reasons for one wrong number is the normal case, not the unlucky one. And neither
was found by suspecting the instrument: both were found by **believing arithmetic over a
measurement**.

## `std::fmod` is not constant time

Its cost grows with the **quotient**, not with the operands' magnitude alone. Measured on this
machine, with nothing else in the loop:

| duration | clock bounded | clock at 6,666 s |
|---|---|---|
| 1.0 s  | 3.798 ns | 7.726 ns |
| 10.0 s | 1.603 ns | 5.901 ns |

So a one-second animation played by a clock that just accumulates `dt` pays a growing tax on every
sample, forever, and a program left running overnight pays a large one. **Wrap the playhead every
step** rather than wrapping at the point of use. It also means a clip's *duration* affects its
sampling cost, which is not a relationship anyone would think to look for.

## Fit a lossy transformation with its consumer's exact reconstruction

Keyframe reduction removes keys whose value the sampler can rebuild within a tolerance. Its whole
job is to replace many small steps with one large one — which **manufactures** exactly the long
arcs on which `quat_nlerp` and `quat_slerp` disagree. Fit against slerp, play back with nlerp, and
the error exceeds the budget you asked for by more the *better* the reduction worked: measured at
**4.5×** on a 120° constant-rate turn with a half-degree tolerance, and nothing anywhere reports it.
The reducer believes it succeeded and the sampler believes it is doing its job.

The rule generalises well past quaternions. Whatever you throw data away against, the thing that
reads it back is the thing you owe the tolerance to.

## An instrument that cannot tell a defect from the feature reports every correct case as broken

`clip_report` measures a loop's seam by sampling at `t = 0` and `t = duration` and comparing the
poses. The first version included every joint and reported a **1.200-unit** seam on a walk cycle
that is perfect — because the root carries the character forward, so it is *supposed* to end the
cycle a stride from where it started. A walk with zero there moonwalks on the spot.

The fix is not a threshold; it is splitting the measurement. `root_travel` for joints with no
parent, `loop_gap_position` for everything else, and two facts where there was one confused one.
Before adding a tolerance to silence an instrument, check whether it is measuring two things.

## Say which degrees. A doc comment is not exempt

`quat_nlerp`'s shipped doc comment carried an error table — *0.13° at 30°, 4.07° at 90°* — and then
argued from it that "under about 30° of arc … is where a 30 Hz animation clip's adjacent keyframes
live". Both halves are true and they are in **different units**: the table is in *sphere* arcs and
sphere degrees, the sentence is thinking in rotation angles, and the two differ by the factor of two
the half-angle puts everywhere. Read as written, the sentence claims a clip's keys are 60° of
rotation apart, which is 1,800°/s and nothing a limb does.

A quantity with a factor of two in it needs its unit stated every single time it appears, including
in prose, including in a comment, including when the surrounding paragraph "obviously" means the
other one.

## A control that fires on the healthy case is not a control

Lesson 7.8's mixer shipped a dropout counter that incremented when the audio device's queue was
empty as the callback began. The reasoning is the obvious one — an empty queue means the device has
already consumed everything we gave it — and it is wrong for SDL3's pull model, where the callback
is invoked *because* the device wants more. On a mix using **0.2% of its budget**, with nothing
audibly or measurably wrong, it reported **28 of 58 buffers starved**.

The danger is not that it was wrong; it is that it was wrong in the direction of *always firing*. A
counter that reads non-zero on every healthy run is a counter nobody looks at, and on the day a real
dropout happens it has nothing new to say. The replacement is `late`, which compares the mixer's own
elapsed time against the mixer's own deadline: it needs nothing from the driver, it is true on every
platform, and a healthy run prints zero.

Two rules come out of it. **Prefer a counter computed entirely from quantities you own** — anything
you have to ask another layer for encodes that layer's model, and you may have the model wrong. And
**check a new instrument against the healthy case before you trust it on the sick one**: 7.6's rule
was to ask what a control would say if the thing were completely broken, and this is the other half
of the same question.

## Real-time code you cannot call from a test is code you cannot debug

An audio callback is the worst place in a program to learn anything. You cannot breakpoint it
without changing the result — the device keeps consuming and the thing you were investigating is
replaced by the sound of having stopped. You cannot print from it, because that is a syscall. And
its output is a pressure wave.

Making `mixer::mix_into()` **public**, and adding an `open_offline()` that builds the voice table
with no device at all, cost one extra method. What it bought was every number in Lesson 7.8: with no
device, the real-time path is a pure function from a voice table to an array of floats, and an array
of floats can be diffed, plotted, subtracted and asserted on. The whole nine-section harness runs on
a machine with no sound card, and the demo's `--shot` uses the same entry point, so a screenshot is
silent and deterministic.

The generalisation is not about audio. Any subsystem that runs on a clock you do not control — a
job-system worker, a network tick, a physics step driven by a fixed accumulator — should expose the
thing it *computes* separately from the thing that *schedules* it.

## A check that greps its own corpus can be broken by writing about it

`check-curriculum.py`'s STATE-manifest check found its block with
`text.find("docs/lessons/:")`. Lesson 7.8 added a note to `STATE.md`'s `completed:` roll that
*mentioned* that key in prose — a thousand lines above the real manifest — and the check began
parsing a paragraph instead of a list, reporting all 82 published pages missing.

Loud, and still the wrong failure for the wrong reason. It is now anchored on `"\n  docs/lessons/:"`,
the indented key rather than the bare string. Any check that searches the corpus it lives in has this
shape, and the fix is always the same: **match on the structure, not on the word**.

## The same fact in three places, and one of them will be missed

`STATE.md` records each lesson in three separate places: the `completed:` roll, a `capabilities:`
entry, and the `files:` manifest. Lesson 7.7 landed in two of them. Its page shipped, its capability
notes were written, its filename went into the manifest — and the `completed:` list never gained a
line, so that roll and the module marker under it still said "6 of 8" while the index, the navigation
chain and the file manifest all said otherwise.

Nothing caught it for a whole lesson, because the existing check watched a list of **filenames** and
could not see a missing lesson **number**. A new conversation resuming from `STATE.md` — which is
the file's entire job — would have been told to write 7.7 again.

`check_state_completed` is now check 9. The transferable part is the diagnosis rather than the fix:
when one fact is recorded in *n* places, the probability that all *n* are updated is not high, and
"be careful" has never once been the answer in this repository. Count the places, then write the
check that compares them.
