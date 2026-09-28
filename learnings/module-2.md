# Module 2 — learnings from its lessons

Moved verbatim from LEARNINGS.md on 2026-09-27, in the original order; LEARNINGS.md
is now the index. Append new sections at the end, and add each heading there.

---

## Sub-pixel errors have a damage profile you cannot sample (Lesson 2.4)

The top-left rule's `-1` bias, left in the accumulators when they are divided into barycentric
weights, does **not** distort the attribute field. It *translates* it, rigidly, by

```
displacement = 1 / ‖e‖   pixels, perpendicular to the edge opposite that weight's vertex
```

The area cancels — which is the whole result. A triangle with a 100-pixel edge is off by 1/100 of
a pixel; a triangle with a 4-pixel edge is off by a quarter of one. **The bug therefore lives in
small triangles**, i.e. dense meshes, which is exactly the geometry nobody inspects individually.

The part worth internalising is what happened when we tried to demonstrate it. On a *smooth*
attribute the shift changes a channel by a fraction of one level out of 256 — undetectable. On a
*quantised* attribute (texel index, stripe, checker cell) it flips whole pixels. Sweeping the
stripe frequency over one fixed triangle with one fixed 0.088-pixel error:

| bands | 2.00 | 2.50 | 3.00 | 3.50 | 4.00 | 5.00 |
|---|---|---|---|---|---|---|
| wrong pixels (of 52) | 4 | 0 | 0 | **15** | 4 | 0 |

Nothing about the error changed across that row. Only where the thresholds happened to fall did.
So "it looked fine when I tried it" is a sample of one from a distribution containing both 0 and
15, and the discipline is to **derive the magnitude of a sub-pixel error rather than look for it**
— `1/‖e‖` is computable in your head, and the fix is one exact integer subtraction, so there is no
decision left to make once you know the number.

This also settled how to build the demo: the band count is swept live with `[` and `]` precisely
so the count jumps around. A panel showing one impressive fixed number would have taught that the
bug is visible, which is the opposite of true.

## Do not oversell a real principle with a fake symptom (Lesson 2.4)

I expected to find that stepping barycentric weights as floats drifts visibly, and to use that as
the argument for integer accumulators. Measured on a hostile 4000×900 triangle:

| method | worst error | in 8-bit colour levels |
|---|---|---|
| integer accumulator, then divide | **0** (bit-exact) | 0 |
| float weight, 4,000 adds along one row | 4.94 × 10⁻⁶ | 0.0013 |
| float weight, never reset across 900 rows | 4.26 × 10⁻⁵ | 0.011 |

One eight-hundredth of a colour level. The scary version of the claim is simply false at this
scale, and the honest argument is narrower and still sufficient: **integers are exact,
reproducible, and cost the same, so take the exact option and stop having to reason about its
error.** Where float accumulation genuinely bites is where comparisons are against tiny
differences — z-buffer tests in Module 3, which this table now feeds into.

Lesson learned about the course itself: an overstated principle backed by a symptom the student
cannot reproduce teaches them to distrust the principle. Measure first, then decide how strong a
claim the measurement supports.

## Name a type after what it is, not what it is shaped like (Lesson 2.4)

`linear_rgb` and the rasterizer-private `rgb3` have identical layout — three floats — and reusing
one for both was tempting. It would have been a lie that compiles: under `blend_space::encoded`
the numbers are stored 0–255 channel values, and a variable named `linear_rgb` holding those is
precisely the confusion Lesson 1.6 exists to prevent. Two structs, one distinction, zero runtime
cost.

The same instinct produced `struct vertex`. Bundling a position with the attributes that corner
carries is not tidiness — it makes "swap the coordinates, forget the colours" *unwritable*, and
that bug has no geometric symptom at all: right shape, right place, shading rotated by one corner,
and only for one winding.

## What correct colour actually costs in a software rasterizer (Lesson 2.4)

Measured on a 20,760-pixel triangle, 400 iterations, release build:

| fill | per triangle | per pixel | relative |
|---|---|---|---|
| flat `fill_triangle` | 20.2 µs | 0.97 ns | 1.0× |
| shaded, encoded blend (wrong) | 57.9 µs | 2.8 ns | 2.9× |
| shaded, linear blend (correct) | 232.0 µs | 11.2 ns | **11.5×** |

Nearly all of the gap is `std::pow` inside `linear_to_srgb`, three times per pixel. Note the
asymmetry that causes it: **decode has 256 possible inputs and fits in a table; encode takes a
continuous float and does not.** A 4096-entry encode table is under 0.4 stored levels of error
everywhere — the bound comes from the curve's steepest slope, `12.92 × 255 ≈ 3295` levels per unit
of light near black, so one table step moves the output ~0.8 levels and nearest-entry rounding
halves it. Left undone deliberately: 232 µs inside a 16.6 ms budget is not a problem we have, and
the entire cost disappears in Module 4 where the GPU encodes sRGB on write for free.

Hoisting matters more than micro-optimisation here. Decoding the three corner colours *once per
triangle* rather than per pixel, and taking one reciprocal instead of 20,760 divisions, are what
make the correct path affordable at all.

## A diagram can pass every automated check and still argue the wrong thing (Lesson 2.5)

The basis-transform demo draws the image of the integer lattice under the current matrix. The first
version skipped `i == 0` in the loop, reasoning that the axes were drawn separately — which left
the images of the lines `x = 0` and `y = 0` missing. The cell containing the origin therefore had
no left or bottom edge, appeared to be twice its true size, and the unit square drawn inside it
looked as though it did not line up with the grid at all.

Nothing failed. No check fired, no pixel was out of place, and the picture looked plausible. It
simply undermined the one claim the figure exists to make — *the transformed unit square is one
cell of the transformed grid*. It was found by rendering the view offscreen and looking at it.

That is now two lessons running where `check-page.js` returned `pass: true` over a defective
diagram (2.4's Figure 4 had iso-lines sprawling outside their triangle — itself a repeat of a
defect 2.3's demo had to fix). The script catches labels that collide, spill or sit on strokes. It
cannot evaluate whether the picture makes the argument. **Budget a pass where you screenshot every
figure and ask what a reader would conclude from it**, and treat a repeat of a previously-fixed
defect as a signal that the check belongs in the tooling or the authoring notes, not in memory.

## Let the type carry the convention (Lesson 2.5)

`mat2` stores two `vec2` columns rather than a `float[4]`. The payoff is larger than it looks:

- **Column-major storage stops being a convention to enforce** and becomes a consequence of naming
  the right things. Two adjacent `vec2`s are four adjacent floats in column order, which is what
  SDL_GPU and HLSL want — so there is no transpose at the API boundary and no opportunity to apply
  one twice or not at all.
- **The arithmetic can be written as its own derivation.** `operator*(mat2, vec2)` is `c0*x + c1*y`;
  `operator*(mat2, mat2)` is `{a * b.c0, a * b.c1}`. Compare with the row-times-column form, which
  is four lines of indices and offers four chances to transpose something silently. Both compile to
  the same code; only one can be verified by reading it.

The general principle: when a convention is causing bugs, look for a type whose shape makes the
convention automatic, rather than for a comment reminding people about it.

## Verify a continuous claim with a discrete measurement — and know the bias (Lesson 2.5)

The determinant claims to be an area factor, and we own a rasterizer, so the claim is testable:
transform a square, fill it with `fill_triangle`, count lit pixels, compare with
`side² × |det|`. At 140 px: identity, scale and shear exact; rotation −0.26%; `rotation · scale`
−0.06%.

The residual is not an error in the determinant. It is the fill rule counting pixel *centres*, so
it scales with the **perimeter** while the total scales with the **area** — meaning the relative
error falls as roughly `1/side`, and the demo at 44 px sees around 1%. Axis-aligned squares are
exact at any size, because the top-left rule makes shared boundaries come out right.

Worth generalising: a discrete measurement of a continuous quantity is always biased. Know which
way and how fast the bias vanishes before you use the measurement as evidence, or you will
eventually mistake a sampling artifact for a bug in the mathematics — or, worse, tune the
mathematics until the artifact goes away.

## A zero-argument function cannot be overloaded (Lesson 2.6)

Adding `mat3` broke exactly one thing in `mat2`'s published API: the free function

```cpp
[[nodiscard]] constexpr mat2 identity() { return {}; }
```

A `mat3` version would take the same arguments — none — and differ only in return type, and C++
does not overload on return type. Not "should not": cannot, because at the point of decision the
compiler may have nothing to tell it which was wanted (`auto x = identity();`).

Everything else in the file survived, and the pattern of what survived is the useful part.
`transpose`, `inverse` and `determinant` overload on the parameter type. `rotation` versus
`rotation_x/y/z` differ by name. `scale(sx, sy)` versus `scale(sx, sy, sz)` differ by arity. The
only casualty was the function with **nothing at the call site to disambiguate it**.

Fix: a static member, `mat2::identity()`, which names the type at the call site and scales to as
many matrix types as we like. The general rule worth carrying: *if a zero-argument function will
ever need a per-type version, give it a type scope or a distinct name now, while that is still
free.* The uniform `scale(float)` overload went at the same time — unused, and it would have become
a trap the moment somebody wanted a uniform 3-D scale.

## Transcribe formulas in the notation they were derived in (Lesson 2.6)

The first `mat3::inverse` was written directly in terms of `c0.x`, `c1.y` and so on, transcribing
the adjugate straight into column-stored members. Two of its nine cofactors used the wrong
component. It compiled, it looked entirely plausible, and it was wrong.

The rewrite names the elements in **written** notation first:

```cpp
const float m00 = m.c0.x, m01 = m.c1.x, m02 = m.c2.x;
const float m10 = m.c0.y, m11 = m.c1.y, m12 = m.c2.y;
const float m20 = m.c0.z, m21 = m.c1.z, m22 = m.c2.z;
```

Nine extra lines, and now every subsequent expression can be compared against any textbook
derivation without translating between row and column indexing in your head. That fixes the
*class* of error rather than the instance.

The check that caught it is worth stating too: `M * inverse(M) == I` over 300 assorted matrices is
not something you can pass by accident. When a formula is too long to verify by reading, verify it
by its defining property instead.

## Two lessons running, the same class of diagram defect

2.4's Figure 4 had iso-lines sprawling outside their triangle. 2.5's lattice was missing its centre
lines. 2.6's Figure 1 had the first leg of a vector walk drawn exactly along the x axis in a dim
dashed grey, where it was simply invisible — so the "a vector is a recipe" picture did not visibly
build the vector.

`check-page.js` returned `pass: true` for all three. It checks that labels do not collide, spill or
sit on strokes; it has no way to evaluate whether the picture makes its argument. The screenshot
pass is now a fixed part of the workflow rather than something to remember, and the specific
recurring failure is worth naming: **a diagram element drawn collinear with, or underneath,
something else is invisible even when it is geometrically correct.** Draw it brighter, thicker, or
offset — or accept that it is not communicating.

## When machinery looks incomplete, check whether it is missing an *input* (Lesson 2.7)

Lesson 2.6 built a 4×4, put a translation in its fourth column, and demonstrated that it moved a
point by exactly `(0,0,0)`. The natural conclusion is that the matrix code is missing something.

It was not. `operator*(mat4, vec4)` in Lesson 2.7 is **byte for byte** the function 2.6 wrote:

```cpp
return m.c0 * v.x + m.c1 * v.y + m.c2 * v.z + m.c3 * v.w;
```

What was missing was a reason for `v.w` to be anything in particular. Supplying that reason — 1 for
a position, 0 for a direction — made the whole thing work with no change to the arithmetic at all.

This is a recurring shape and worth recognising: **the code you are staring at is correct, and the
defect is in what the caller is saying about the data.** It is unusually hard to debug because
reading the implementation more carefully cannot help. The tell is that the implementation is
simple and obviously right, and the behaviour is still wrong.

## A magic literal at a call site is a bug waiting for a hurried reader (Lesson 2.7)

`to_vec4(n, 0.0f)` and `direction(n)` compile to identical code. They are not equally good.

The first is a magic number, and magic numbers get changed by whoever is trying to make something
compile — flipping `0.0f` to `1.0f` looks like a harmless adjustment. The second states an
intention, and changing `direction(n)` to `point(n)` is visibly a claim about what the vector *is*.

The bug that distinction prevents is the worst-shaped one in this module: a direction transformed as
a position has the translation added to it, so **the error equals the translation** — measured at
10.77, then 107.70, then 1077.03 as the object moves ×1, ×10, ×100 from the origin. It is invisible
in a test scene at the origin and ruinous in a real level, which is exactly backwards from how you
would like a bug to behave.

Cheapest test for it: a unit direction through a rotation must come back **unit length**. If it
comes back with the magnitude of your translation, that is this bug.

## Show a difference against a fixed reference, or you have shown nothing (Lesson 2.7)

Figure 1 of Lesson 2.7 drew a room twice — before and after being moved — with a lamp inside it and
an arrow. The lamp was supposed to move and the arrow was supposed not to. Both rooms were drawn
identically, so *relative to the room* nothing had changed and the reader had to take the labels'
word for it. `check-page.js` was green.

The fix was to draw an identical ruler under both copies and a dashed line from the lamp down to it:
now the lamp is visibly above tick 3 and then above tick 5, while the arrow is visibly the same
arrow. The claim became checkable by looking.

Generalising, and this is now three lessons of diagram defects in a row: **a before/after figure
needs something in it that provably did not change.** Without a fixed reference, "this moved and
that did not" is a caption rather than a picture.


## The nastiest transform bugs are invisible in the degenerate cases (Lesson 2.8)

The wrong model-matrix order `T·S·R` shears a non-uniformly-scaled object as it turns. But it is
*bit-for-bit correct* at θ = 0° and θ = 90° — the two orientations anybody types while testing — and
it is *identical to the correct order* for any uniformly-scaled object or any object with no
rotation. Verified: sweeping the uniformly-scaled demo post through 360° a degree at a time, the
worst difference across all sixteen matrix elements against `T·R·S` is `0.000e+00`.

A real scene is mostly uniform scales and mostly axis-aligned props, so a codebase with the order
wrong renders almost everything perfectly and gets caught only by the one rotating,
non-uniformly-scaled object added weeks later — by which point the wrong order is buried in a working
renderer and the new asset is the obvious suspect.

The lesson for authoring and for engine code alike: **a transform that is invisible in the
degenerate cases and wrong everywhere else cannot be tested away by trying the easy cases.** The
defence is to derive the thing once and write it down where it cannot be re-derived wrongly
(`parent_from_local()` builds `T·R·S` and nothing else). This failure profile recurs — normal
transforms under non-uniform scale (3.6), shadow bias — so it is worth recognising by shape now.


## Prove "rigid" with a measurement, not a picture (Lesson 2.8)

"Is the object deformed?" looks like a question you answer by eye, but the projection can lie about
it — an oblique or perspective view distorts shapes on purpose, so a rigid object can *look* sheared
and a sheared one can look fine. The reliable test is numeric and reads straight off the matrix:
transform the model's `x̂` and `ŷ` **as directions** (`w = 0`, so translation cannot touch them) and
check their dot product is zero and their lengths equal the intended scale. If so the object is rigid
whatever it looks like. The demo's HUD does exactly this and prints `DEFORMED` only when the measured
corner leaves 90°.

Two traps embedded in that check, both real: send the axes as `point()` instead of `direction()` and
you fold the object's *position* into what you think is its shape; and `acos` needs its argument
clamped to `[−1, 1]` first, because `dot/(|a||b|)` rounds just past 1.0 at an exact axis alignment —
which the demo hits every few seconds — and `acos(1.0000001)` is `NaN`, not 0.


## Name for the code you are going to write, not the code you have (Lesson 2.8)

`parent_from_local()`, not `world_from_local()`, even though in Module 2 the parent *is* the world.
Module 5 adds a transform hierarchy where "parent" becomes another object, and at that point the
function does not change by a character — only the meaning of the word widens. The alternative name
would have forced either a rename touching every call site or a name that lies. This is *not*
speculative generality (no parent pointer, no hierarchy machinery ships today); it is only declining
to hard-code an assumption already known to be temporary, which costs nothing. The `a_from_b`
convention is the same instinct applied to composition: the name is chosen so a wrong product is a
spelling mistake.


## Introduce math where it is first needed, not where a plan filed it (Lesson 2.9)

`vec3.hpp` shipped from Lesson 2.5 with a comment promising the cross product would arrive in Lesson
3.4, when back-face culling needed a surface normal. But Lesson 2.9's `look_at` needs a vector
perpendicular to two others — the camera's `right` axis, from the look direction and an up hint —
and there is *no honest way* to build an orthonormal basis without it. The options were: hand-roll
the specific computation inline without naming it (dishonest — it *is* the cross product, and
pretending otherwise breaks "the student should never type a line they couldn't explain"), or
introduce the cross product here. We introduced it here, and revised the deferral comment.

The general rule this reinforces: **a "just-in-time math" plan is a guess, and the first lesson that
actually needs a tool wins over the lesson that expected to introduce it.** The course's spiral is
undamaged — 2.9 introduces the cross product for a camera basis, 3.4 deepens it for a triangle
normal and its tie to signed area. Introduced where used, deepened where it recurs. The cost of
getting the plan "wrong" was one revised comment; honoring the plan would have cost a decreed
formula in the middle of a derive-everything course.


## A projection can make a rigid thing look sheared — verify frames numerically (Lesson 2.9)

When the demo's camera orbits, the whole scene turns. Is it turning *rigidly*, or is the view
matrix quietly shearing it? You cannot tell by eye, because the orthographic projection already
distorts on purpose (and perspective, in 2.10, will distort more). The check is the same shape as
Lesson 2.8's deform test, moved up a space: a correct view matrix is built from an **orthonormal**
basis, so its three axis rows must stay mutually perpendicular unit vectors at every camera angle.
The demo prints those rows and they read as a clean tripod throughout; the harness asserts
`V · world_from_camera == I` (the definition of "inverse") to `1e−4` over many random cameras.

The companion trap: a **left-handed** basis passes the orthonormality check and still mirrors the
world. `cross` anticommutes, so a single swapped argument order (`cross(backward, up)` instead of
`cross(up, backward)`) negates `right`, and the scene renders mirrored — invisible until text or
winding reveals it. The cheap guard is a handedness assertion: for a right-handed frame,
`cross(right, up)` must equal `backward` (i.e. `x × y = z`), not `−backward`.


## A matrix can't divide, so perspective defers the divide — that's why w exists (Lesson 2.10)

The single most clarifying fact about the projection pipeline: a matrix is a *linear* map, and
`x' = x/z` is not linear, so **no matrix can do perspective by itself.** What the projection matrix
does instead is copy `−z` (the depth) into `w` via its bottom row `(0,0,−1,0)`, and a *separate*
step — the perspective divide, `v/v.w` — does the actual shrink afterwards. Every piece of
"projection jargon" falls out of that one deferral: `w` stops being 1 (this is the third case
Lesson 2.7 flagged); "clip space" and "NDC" are just the before and after of the divide; and the
reason clipping (Lesson 3.3) happens in clip space is that it is the last place an edge is still
straight, before the non-linear divide bends it. If you remember one sentence about perspective,
remember "the matrix can't divide, so it stashes the depth in `w` for later."

Corollary that keeps the code honest: `xyz()` (drop `w`) and `perspective_divide()` (divide by `w`)
are kept as **two separate named functions**, never one accessor with a mode. Before a projection is
in the chain `w` is always 1 and the two agree; the moment one appears they diverge, and a silent
wrong choice is a bug that looks like a tuning problem. Two names make the call site declare intent.


## Perspective depth is 1/z-nonlinear, and the near plane is the master precision knob (Lesson 2.10)

Because the projection divides by `−z`, depth does **not** map linearly into `[0,1]`. With
`near = 1, far = 100`, a point at `z = −2` — one unit past the near plane — is already at
`z_ndc = 0.5`: half the entire depth buffer's range is spent in the first 1% of the frustum, and the
remaining 99 units share the other half. Consequences worth having as instinct: depth precision is
lavish up close and starving far away; **z-fighting is a distant-geometry problem**; and the single
highest-leverage fix is to **push the near plane out** (moving `near` from `0.01` to `0.5` buys back
orders of magnitude of far precision), which costs nothing. This is why "set near as large as your
scene tolerates" is standard advice, and it is the setup for reversed-Z (Exercise 2.10.5) and the
z-buffer (Lesson 3.1). Made it a number in a harness so it is an instinct, not a warning.


## The clean way to A/B two rendering modes is one matrix, one path (Lesson 2.10)

The demo's `[P]` perspective/orthographic toggle changes **nothing** in the draw code — it selects
which matrix `project()` receives. Both projections run the identical `clip = proj * v` →
`perspective_divide` → viewport path; the orthographic matrix simply keeps `w = 1`, so the same
divide harmlessly divides by one. This makes the comparison *honest*: the only variable between the
two pictures is whether the matrix wrote depth into `w`, so anything that differs on screen is
genuinely perspective and not an incidental difference in how the two modes are drawn. General rule
for "show the artifact" toggles: route both sides through one code path and vary a single input, so
the toggle isolates exactly the thing under study — the same discipline as 2.4's fill-rule coverage
counter and 2.8's `[O]` order toggle.


## A convention that lives in twelve places will eventually be wrong in one (Lesson 2.11)

The `+y`-up-to-`+y`-down flip appeared as a bare minus sign in Lesson 2.5's `to_screen`, again in
2.6's cube demo, again in 2.8, again in 2.9, and finally inside 2.10's `project` — five copies, each
with a comment gesturing at what it was for, none deriving it. Every copy was correct, and that is
precisely the danger: nothing was broken, so nothing forced the question, and the sixth copy would
have been written by someone who had only ever seen the fifth.

Lesson 2.11 moved it into one function, `viewport::to_screen`, written as `(1 - t_y)` rather than
`-ndc.y` so the code *states* that it is reversing a fraction instead of just looking negative.
**The general rule: when the same non-obvious sign or constant appears in a third place, it is telling
you a transform is missing a name.** Give it a type and one home. The tell is not that the copies are
wrong — it is that each one needs a comment to justify itself.

Corollary for refactors of this kind: **prove it moved nothing.** The harness swept an NDC grid
through both the old constants and the new `viewport` and reported a worst difference of
`0.000e+00`. For a "give this a name" refactor that is the whole acceptance test — if the picture
changes, the refactor is a rewrite wearing a refactor's clothes.


## Design your types to shadow the API you will port to (Lesson 2.11)

`engine::viewport` has the same six fields, with the same names and meanings, as SDL's
`SDL_GPUViewport` (`x, y, w, h, min_depth, max_depth`) — verified by reading the fetched
`SDL3/SDL_gpu.h`, not assumed. That is deliberate: in Module 4 the conversion is a field-by-field
copy rather than a translation, and any convention mismatch (origin top-left vs bottom-left, depth
range) is forced into the open *now*, while the software rasterizer is the only consumer and a
mistake costs minutes.

This is the same bet already made twice: `mat4` is column-major because that is what HLSL constant
buffers want (2.6), and the projection targets SDL_GPU's `[0,1]` NDC depth rather than OpenGL's
`[-1,1]` (2.10, the NDC-parity decision). The pattern is worth naming — **match the destination
early, port cheaply later** — with the caveat that "mirrors the C struct" is a claim about a header
that can change, so it belongs behind a `⚠ VERIFY` and a real grep of the header, never memory.


## Hand-authored geometry is DATA — validate it, don't eyeball it (Lesson 2.12)

The icosahedron ships as sixty hand-typed index numbers. Exactly one of them being wrong produces a
missing face or a doubled edge that is easy to miss in a spinning wireframe and impossible to
diagnose by staring. But mesh data is *checkable*, cheaply and mechanically:

- **Euler's formula** `V − E + F = 2` for a closed surface. Breaks if you merged or duplicated an edge.
- **Manifold:** every *undirected* edge belongs to exactly two faces — one means a hole, three a pinch.
- **Consistent winding:** every *directed* edge appears exactly once, because adjacent faces traverse
  their shared edge in opposite directions. This is strictly stronger than the manifold check.
- **Outward winding:** each face's `cross(b−a, c−a)` dotted against the face centre (relative to the
  mesh centroid) must be positive.

The last one matters most for a reason that is easy to miss: **a wireframe does not care about
winding, so a winding error is completely invisible until Lesson 3.4 turns on back-face culling and
random triangles vanish.** Authoring the data correctly *and verifying it* while the check is free
means the mesh never needs re-authoring. The general rule: when a convention has no consumer yet,
that is the cheapest possible moment to get it right and prove it — not the moment to defer it.

This is the same "check where the input can actually be wrong" judgement as `put_pixel` bounds-checking
while `at(row, col)` does not (Lesson 2.5). Code you wrote is wrong in ways review catches; *data* is
wrong in ways only a validator catches.


## The saving from indexed geometry is WORK, not bytes (Lesson 2.12)

The obvious pitch for a vertex array plus an index array is memory: the icosahedron stores 12
positions instead of 60. True, and much the less interesting half. The real win is that the render
loop transforms **each vertex once** — 12 matrix multiplies per frame instead of 60 — because the
indices are consulted *after* the transform, not before. Order the two loops the other way (walk
triangles, transform each corner) and you keep the memory saving while throwing away the compute
saving entirely.

That is exactly why GPUs carry a post-transform vertex cache, and why the loop structure in
`draw_mesh` — transform all vertices, *then* walk indices — is the structure Module 4 hands to the
hardware. Worth internalising as a general shape: **an indirection is only a saving if the expensive
work happens on the small side of it.**
