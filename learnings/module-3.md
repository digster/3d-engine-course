# Module 3 — learnings from its lessons

Moved verbatim from LEARNINGS.md on 2026-09-27, in the original order; LEARNINGS.md
is now the index. Append new sections at the end, and add each heading there.

---

## "Show the failure" only works if the failure is BIG ENOUGH TO SEE (Lesson 3.1)

The cycle scene — three woven panels whose depth order is a loop — was correct geometry from the
first attempt. The harness confirmed A over B over C over A at all three corners. And the demo
looked *fine*: the painter's algorithm and the z-buffer disagreed on **7 pixels**, a smudge you
would never notice.

The construction was right and the *parameters* were wrong. Planks laid end to end along the sides
of a triangle only overlap in a sliver near each shared corner; the wrongness was real and
microscopic. Sweeping the circumradius, plank width and overhang and measuring the disagreement
took ten minutes and moved it from 7 px to **144 px** — a fifth of the covered area, unmissable.

The lesson generalises beyond this demo. Pedagogy §5 says *show the artifact*, and it is easy to
read that as "construct a case where the bug occurs". It is not enough. The artifact has to be
loud, and whether it is loud is a **quantitative** property of your test scene that you should
measure and tune deliberately, exactly as you would tune the scene for a screenshot. A failure
demo that requires the reader to squint has failed.

Corollary: build the measurement *before* the demo. The pixel-difference counter was written to be
a HUD readout and turned out to be the tool that made the scene right.


## A verification harness will tell you your explanation is wrong, if you let it (Lesson 3.1)

The demo prints how many pixels the painter's algorithm and the z-buffer disagree about. On the
milestone scene — where sorting is genuinely correct — it read **29**, not 0. The hypothesis was
easy to reach and easy to believe: silhouette edges, where a front face and a back face share an
edge, tie exactly in depth, and the two algorithms break ties in opposite directions (`<` keeps the
first drawn; painting keeps the last).

Believing it would have been a mistake. The check was four lines: drop screen-space back-facing
triangles and re-measure. Result: `6 of 12 tris kept, 0 px differ`. The explanation was right —
*and now it is verified*, which is a different thing, and it is what let the pitfall entry state it
as fact and hand the fix to Lesson 3.4 by name.

The general rule this codebase keeps re-learning: **a plausible explanation for a measured anomaly
is a hypothesis, and hypotheses are cheap to test when you already have a harness.** The cost of
the check was minutes; the cost of publishing a confident wrong explanation is a reader who cannot
reproduce it.


## Interpolate the quantity the projection already fixed (Lesson 3.1)

The reason a z-buffer stores *device* depth rather than view-space `z` is not a convention or an
efficiency: it is that barycentric interpolation computes the unique **affine** function agreeing
with three corner values, and only one of the two candidates is affine in screen space.

Substituting the projection into a triangle's plane equation makes `w` factor straight out and
leaves `1/w` as a constant plus constants times the screen coordinates. Since
`z_ndc = −A + B·(1/w)`, device depth inherits that affinity exactly. View-space `z = −1/(1/w)` is
the reciprocal of an affine function — a hyperbola — and interpolating it linearly reads **−50.5
where the truth is −1.98** at the screen midpoint of a near-to-far edge.

Two things to carry forward:

- **The bug hides on flat walls.** If a triangle's plane is parallel to the screen, `1/w` is
  constant and both choices agree exactly. Test scenes are made of boxes and floors, which is
  precisely the geometry that cannot reveal the error. When something "works on my test scene",
  ask what family of input the test scene structurally excludes.
- **`1/w` is affine in screen space** is the reusable fact, not the depth conclusion. It is the
  entire tool Lesson 3.2 needs for perspective-correct interpolation of every *other* attribute.


## Precision is a formula, not a vibe (Lesson 3.1)

Z-fighting gets diagnosed by staring at shimmering surfaces and then nudging geometry until it
stops. It does not need to be. One depth code spans

    Δw = Δz · w² · (1/near − 1/far)      with  Δz = 1/(2^bits − 1)

of real distance at distance `w`. Put your numbers in and you get an answer in metres, and you can
compare it against the gap between your surfaces *before* rendering anything.

The formula also ranks the fixes, which staring never does. The bracket is dominated by `1/near`,
so with `near = 1, far = 100`: pulling `near` to 0.1 costs **10.09×** precision everywhere, while
pushing `far` to 1000 costs **0.9%**. The near plane is the expensive knob and the far plane is
nearly free — the opposite of most people's intuition, and it is arithmetic rather than opinion.

The demo's numbers back it: two panels 1 mm apart at ~6.4 units, where one D16 code spans
0.00208 units, means a 0.48-code gap — and 478 of 875 covered pixels (54.6%) show the wrong panel.
At D24 and D32_FLOAT, zero do. The prediction and the pixel count agree.


## Widget SVGs are not `figure.dia` SVGs, and an unstyled `<text>` is black (Lesson 3.1)

The shared stylesheet themes diagram text with `figure.dia svg text { fill: var(--dia-ink) }`.
Interactive widgets live in `.widget`, which that selector does not reach — so an SVG `<text>` in a
widget falls back to the SVG default fill, which is **black**, and disappears against the dark
theme's raised background. It had been that way since Lesson 2.12's widget shipped.

The trap has two halves and both are worth remembering. Adding `fill="var(--dia-ink-soft)"` inline
*works* in a widget (nothing overrides it) but is flagged by `apply-shared.py`'s lint, which is
unanchored — and the lint is right in spirit even where it is wrong in detail, because the fix
belongs in the shared stylesheet. `.widget svg` now carries the same `text` / `.muted` / `.mono` /
`.sm` / `.xs` vocabulary as `figure.dia svg`, so a widget label is written exactly like a diagram
label, and 2.12's widget was repaired by re-stamping.

General rule: when a lint tells you not to do the obvious thing, check whether the obvious thing is
a symptom of a missing shared rule rather than a local mistake.

## Interpolate the quantity that is affine in the space you are walking (Lesson 3.2)

Lessons 3.1 and 3.2 are the same sentence applied twice. Barycentric interpolation promises exactly
one thing: **the unique affine function of the pixel position** that agrees with three corner
values. So the only question worth asking about any quantity is *is this affine in screen space?*

- Device depth: **yes**, because the projection matrix's depth row already made it so. Interpolate
  it directly. Correcting it again is a genuine bug.
- View-space `z`: **no** — it is the reciprocal of an affine function.
- Texture coordinates, colours, normals: **no**, but `a/w` is, and so is `1/w`. Interpolate both
  and divide.

The derivation for the third case is five lines and it never mentions texture coordinates: write
`a = αx_v + βy_v + γz_v + δ`, substitute the projection, divide by `w`, and the constants cancel
out of the conclusion. One derivation therefore covers every attribute a vertex will ever carry —
which is why hardware documentation can describe varying interpolation in a paragraph.

The reusable habit: when an interpolation looks wrong, do not reach for a fudge factor. Ask which
space the interpolator walks in, and what is affine *there*.


## An error that is zero at the corners is a chord under a curve (Lesson 3.2)

Affine texture warping is invisible to every check you would naturally run. Print the corner uvs —
correct. Verify the mesh — correct. Look at the wireframe — correct. The error is **exactly zero at
all three vertices and maximal in the interior**, because that is what a chord does under a curve.

Recognising the shape tells you three things at once: where to look (the middle), why your tests
passed (they sampled the endpoints), and how it will respond to subdivision (quadratically — chord
error over an interval `h` is `O(h²)`).

That last one is worth having as a number rather than a feeling. Measured on this course's floor,
the improvement ratios per doubling run 2.31, 2.42, 2.62, 2.84, 3.10 — climbing toward 4 — and at
2,048 triangles the picture is *still* wrong on 381 pixels. **Convergence is not termination.** Any
time subdivision is proposed as a fix for an interpolation error, that distinction is the whole
argument.


## Ask what your test scene structurally cannot show (Lesson 3.2)

Both of Module 3's interpolation bugs — view-space depth in 3.1, affine attributes in 3.2 — are
*exactly zero* on a triangle whose plane is parallel to the screen, because `w` is then constant
across it and every interpolation scheme agrees. Sprites, UI quads, billboards and the front faces
of axis-aligned boxes are all in that family. A test scene built from them cannot reveal either
bug, no matter how carefully it is inspected.

This reframes "it works on my test scene" from an embarrassment into a question with a findable
answer: **what family of input does my test set structurally exclude?** Here the answer was
"anything steeply angled", and the fix was to put a steeply-angled surface in the demo permanently,
one keypress away. That is cheaper than remembering to test for it.


## A saturating metric hides the thing it is measuring (Lesson 3.2)

The demo counts pixels where two renders disagree. On the tessellation sweep it read 48.5%, 48.2%,
51.2%, 46.7% — flat, and then fell off a cliff to 4.9%. That looks like a threshold effect in the
*error*. It is not: it is the **metric** saturating. Once the uv error exceeds half a checker cell
the pattern is scrambled, and two scrambled two-colour images differ on about half their pixels
however much worse one of them gets. *You cannot be more wrong than a coin flip.*

Measuring the underlying quantity instead — worst uv error, in cells — gave a clean
`4.23 → 1.83 → 0.76 → 0.29 → 0.10 → 0.03`, from which the second-order convergence is obvious.

The general hazard: **measuring a continuous error through a quantised output caps your dynamic
range at the quantisation.** Pixel-difference counts, pass/fail rates and anything else derived
from a thresholded comparison all have this ceiling. Keep one metric on the raw quantity.


## A parameter list that keeps growing is asking to be a state object (Lesson 3.2)

`fill_triangle` collected a `blend_space` in 2.4, was about to collect an interpolation mode and a
shading mode in 3.2, and will want a lighting term in 3.6. Four trailing enums at every call site
is where an API starts to rot — and the fix was not invention but *recognition*: the hardware
already solved this, and calls the answer a **pipeline object**. A GPU does not take render state as
draw-call arguments; it bakes it into an object built once and bound before drawing, because
validating that state per draw would be ruinous.

So `fill_style` is not tidiness, it is adopting a shape that is known to be right and that Module 4
will make literal. Two properties worth copying deliberately: **every field defaults to the correct
value**, so the right call is the short call and each deliberately-broken mode must be named; and
adding a knob costs one field rather than one argument at every call site.


## The fixed-size scratch array that was fine until it wasn't (Lesson 3.2)

`collect_triangles` transformed vertices into `engine::vec3 view_pos[64]`, clamped with
`std::min(mesh.vertices.size(), std::size(view_pos))`. Correct and cheap while the largest mesh was
a twelve-vertex icosahedron. Lesson 3.2's tessellated floor has **289** vertices, and the clamp
would have silently drawn a fraction of it — no crash, no warning, just a floor with a bite out of
it and no obvious cause.

The clamp is the trap. A hard limit that *truncates* rather than failing converts a capacity bug
into a rendering bug, which is much harder to trace. Two honest options: assert, or remove the
limit. We removed it — `std::vector` scratch owned **across frames** by the caller, so `clear()`
keeps the capacity and the steady state allocates nothing.

That ownership detail is the whole trick, and it is the smallest possible preview of Module 9: the
fix for allocation in a hot loop is almost never a faster allocator, it is not allocating.


## A destructive step has a precondition, not a guard (Lesson 3.3)

The perspective divide is `x/w`, and for a vertex behind the eye `w` is negative, so the result is
not merely large — it is **sign-flipped**. A point up and to the right of the camera lands down and
to the left. `x_ndc = -0.43` is that point; it is *also* an ordinary point slightly left of centre,
comfortably in front. The divide collapses the two onto one number and leaves no residue.

So there is no test you can put *after* the divide that recovers the distinction, and no branch you
can put *inside* it that helps: the correct answer for a straddling triangle is not "draw it" or
"drop it" but "draw a different, smaller triangle". That is work, and it has to happen upstream.

**The general form:** when a step destroys information, the check that needs that information cannot
live at or after the step. It has to be a precondition enforced by whoever comes before, and the
step's contract should say so out loud. `screen_from_clip` has no guard on `w`, and its doc comment
says why: a `w` at the eye is not a case to branch on, it is a case that must not arrive.


## Two states that must not be confused should be two types (Lesson 3.3)

`engine::vertex` is a screen-space type: integer pixels, device depth in `[0,1]`, a pre-divided
`inv_w`. Every one of those fields assumes the divide has already happened, which is exactly what
makes it the wrong type to clip with. A single struct with a "have I been divided yet" flag would
compile, and would let a screen-space vertex reach the clipper, where the arithmetic is silently
meaningless.

`clip_vertex` makes that a compile error instead. This is the third time the same move has paid in
this codebase — `point()` vs `direction()` (2.7), `xyz()` vs `perspective_divide()` (2.10), and now
this — and the pattern is worth naming: **when two things have the same shape and different
meanings, spending a type is cheaper than spending a comment.**


## Prove the bound instead of clamping to it (Lesson 3.3)

`clip_polygon_near` writes into a caller-supplied buffer with no capacity check in the loop. That is
defensible only because the bound is a *proof*, written where the reader will meet it: emissions are
(vertices inside) + (crossings), one plane produces at most one crossing in each direction around a
convex polygon, a triangle has three vertices, so the worst case is exactly 4.

Lesson 3.2 learned the other half of this the hard way — a fixed array plus `std::min` turns a
capacity bug into a *rendering* bug, which is far harder to trace. The resolution is not "always
clamp" and not "always assert". It is: make the bound exact and say why, or make exceeding it fail
loudly. What must never happen is a limit that silently truncates.


## The measurement is allowed to prove *you* wrong (Lesson 3.3)

This lesson's first draft asserted that "no amount of subdivision removes the artifact" — a
satisfying line, parallel to 3.2's genuine finding about quadratic convergence. The harness
disagreed immediately: at 8×8 on the demo's floor, dropping straddling triangles and clipping them
produced **bit-identical frames**.

The reason is specific and it is the interesting part. On a *ground plane* the strip that straddles
the near plane is the one under and behind your feet, so once tessellation makes it thin enough it
falls off the bottom of the view and costs nothing. Subdivision does not fix the bug; it moves the
hole somewhere the camera is not looking.

What replaced the false claim is stronger than it was: measured over every camera the demo allows,
2 triangles lose the whole frame, 32 triangles lose the whole frame, 512 triangles still lose 51%,
and it takes **8,192** — four thousand times the geometry — before the hole is finally pushed off
every reachable view. The clipper is thirty lines and exact.

Pedagogy §5 says show the failure. It cuts both ways: build the harness so it can tell you the
failure you are describing is not the one that happens.


## Do not use a broken baseline to test the fix for the breakage (Lesson 3.3)

The obvious winding check is "signed screen area before clipping vs after". It fails, and the
clipper is innocent: the *before* triangle has a vertex behind the eye, so its projection is exactly
the garbage this lesson exists to prevent. Comparing against it measures nothing.

The test that works is **continuity**. Slide a triangle steadily through the plane, clipping at every
step, and assert the sign never changes — it cannot, because the shape does not turn inside out as
it moves. Generalisation: when testing a fix for a degenerate case, the reference has to be drawn
from the *non*-degenerate regime, and a sweep through the boundary is usually how you get one.


## `std::clamp` cannot remove a NaN (Lesson 3.3)

`std::clamp(v, lo, hi)` is `v < lo ? lo : hi < v ? hi : v`. Every comparison against a NaN is false,
so both tests fail and the NaN is handed straight back. Code that clamps "to be safe" before a cast
is therefore not safe at all, and converting a NaN — or any float outside the target's range — to an
integer is **undefined behaviour**, not a large number.

Two consequences worth carrying:

- Write the test in the form that catches NaN: `!(x < limit)` and `!(x > 0)`, never `x >= limit` or
  `x <= 0`. The negated form is true for a NaN; the direct form is false.
- A cast that is undefined for a value the program can reach is a latent bug regardless of which
  lesson first reaches it. Lesson 3.3's deliberately-broken mode is what *found* the reachable NaN
  in `linear_to_srgb_u8`, but Module 6's HDR pipeline and Module 8's physics would both have found
  it eventually, in circumstances far less convenient.


## One constant, two undefined behaviours (Lesson 3.3)

`to_pixel` clamps to ±8,000, and the number is doing two jobs at once. It keeps the float inside
`int`'s range so the conversion is defined — and it keeps `edge_function`'s products inside 32 bits,
because that function multiplies coordinate *differences* and signed overflow is undefined too.
Picking ±100,000 would have fixed the first and quietly created the second.

Worth the habit: when a limit exists to hold off one failure, check what else downstream has a
range, and pick a value that satisfies all of them. Then say so at the constant, because the next
person will otherwise assume the smaller number was arbitrary and raise it.


## Read the sign before you destroy it (Lesson 3.4)

`fill_triangle` had computed the triangle's signed area since Lesson 2.2, and then immediately
swapped two vertices to force it positive — because the top-left rule is stated for a
positively-oriented triangle. That swap *destroys the facing*. Back-face culling is therefore not a
new computation at all; it is one comparison inserted into the single window between the sign being
known and the sign being thrown away.

The generalisable habit: when you find yourself adding a test, look first for a quantity the code
already computes for another reason. Twice now in this module the answer was already on the stack —
`1/w` in Lesson 3.2, the signed area here — and in both cases the "expensive" feature turned out to
cost one line.


## A function signature can make a bug unwritable (Lesson 3.4)

`is_front_facing` takes three **screen-space** vertices. There is no overload that accepts a
view-space position, so the classic culling bug — asking the question before the projection, against
the camera's forward axis — cannot be written by accident against this API. The type says where the
test belongs.

That is a cheaper defence than a comment and a much cheaper one than a code review. It is the same
move as `point()` vs `direction()` (2.7), `xyz()` vs `perspective_divide()` (2.10), and
`clip_vertex` vs `vertex` (3.3): **when two things have the same shape and different meanings,
spend a type.**


## The wrong test is often the right test for a camera you are not using (Lesson 3.4)

`dot(normal, camera_forward)` misjudges 15.46% of triangles at a 55° field of view and 32.43% at
120°. Under an **orthographic** projection it is wrong **0 times out of 200,000** — because
orthographic projection is exactly the statement that every ray to the eye *is* the camera axis.

So it is not a sloppy approximation. It is a correct implementation of a different question, and
that is why it survives in codebases: it is exactly right in the orthographic views a level editor
shows you, and subtly wrong in the wide-FOV gameplay camera nobody is looking at while they write
the culler. When a bug's incidence depends on a *parameter* (here, field of view), find the value at
which it vanishes — it usually explains why the bug exists.


## Folklore deserves a measurement (Lesson 3.4)

"Back-face culling removes half your triangles" is repeated everywhere and is false. Measured over
6,000 random orientations: a cube shows **2 to 6** of its 12 triangles (mean 5.55), an icosahedron
**7 to 10** of its 20 (mean 8.80). Half is a *ceiling*, not a rule, and the geometry says why —
pair the faces whose planes are parallel, and the eye is in front of at most one of each pair, and
in front of *neither* when it lies in the slab between them. Look a cube square in the face and you
see one face, not three.

The time saved is smaller again: 54.8% of triangles removed bought **31.6%** of the frame
(19.95 µs → 13.64 µs), because back faces were precisely the triangles the z-buffer was already
rejecting on their first depth comparison. They were the cheapest pixels in the frame, not the most
expensive.

Both numbers are better teaching than the folklore was, and neither could have been guessed.


## An optimisation that quietly fixes a bug is worth understanding, not glossing (Lesson 3.4)

Culling should be invisible on closed geometry. Measured over 1,008 camera and rotation
combinations, it changes up to **44 pixels** — and **100% of the changed pixels are ones a back face
had been drawn on**. That framing needed no threshold and is the strongest form of the claim: the
only thing culling can touch is a place where you were seeing the inside of a solid.

Two mechanisms put a back face there, both ties. Along a silhouette, a front face and a back face
share an edge in 3-D and therefore have *equal* depth; the test is a strict `<`, so mesh order
decides. (Drawing front faces first drops the worst case from 44 px to 29 px — which is exactly the
number Lesson 3.1 measured and could not explain.) The remaining 29 px are quantisation: the two
faces round to integer pixels independently, so the back face's outline can stick out where the
front face's does not reach. No draw order fixes that; only removing the back face does.

So back-face culling is an optimisation *and*, marginally, a correctness improvement — not because
the z-buffer was broken, but because a tie has to break somewhere and "never show the inside of a
solid" is a better rule than "whichever triangle the index buffer listed first". Lesson 3.3 drew a
firm line between correctness and optimisation; this is the case that shows the line is real but not
always sharp.


## Choose a normaliser that reflects where the error comes from (Lesson 3.4)

Checking `dot(n, a) == det[a,b,c]` numerically "failed" at a relative error of 1.1e-4, and the
identity is exact algebra. The error was in the *test*: I divided by the magnitude of the result,
and the triple product is a difference of large products that cancels almost completely near
degeneracy — so a relative-to-result error is unbounded and meaningless there. Normalising by
`|a||b||c|`, the size of the *terms*, gives 1.06e-6 and a threshold that means something.

The general rule: when a quantity is computed as a difference, its error scales with the
**inputs**, not with the answer. Normalise by what the floats actually were.


## The extension your build system reserves may be someone else's data format (Lesson 3.5)

`*.obj` is MSVC's object-file extension. It is in essentially every C++ project's `.gitignore`,
including this one since Lesson 0.4. It is also Wavefront's model extension.

So `git add assets/cube.obj` silently does nothing. Not an error, not a warning — the file is
simply not staged, the build works perfectly on the machine that has it, and the repository is
broken for everybody else in a way that looks like a missing feature rather than a missing file.

Two habits fall out of it. **Negate deliberately** (`!assets/*.obj`) rather than deleting the
broad rule, because the broad rule is still right for build output. And **check the decision
rather than the intent**: `git check-ignore -v <path>` prints the exact rule that decided, so a
pattern with `!` in the output means "not ignored" and you have proof rather than a belief.
`git status --untracked-files=all` is the other half — plain `git status` collapses a whole
untracked directory to one line and will happily hide that only three of its four files matter.


## A file's idea of a vertex and the hardware's idea of a vertex are different ideas (Lesson 3.5)

This is the whole of what makes writing an OBJ loader worth a lesson rather than an afternoon.
OBJ gives each face corner **three independent indices** — `f 1/3/7` means position 1, texture
coordinate 3, normal 7 — while a vertex buffer has **one** index that selects position, uv and
normal together, because the hardware fetches a vertex as a unit.

So a vertex is not a position. A vertex is the *triple* `(i_v, i_vt, i_vn)`, and a position shared
by two faces that disagree about its normal has to be stored twice. That is why a cube arrives as
**24 vertices, not 8** — every corner is three corners of paper, one per face, and the three
disagree about which way the surface faces. Measured on our `assets/cube.obj`: 8 positions, 4 uvs,
6 normals, 24 corner tokens, 24 distinct triples, zero reuse.

The generalisable part is not about OBJ. It is that **an asset pipeline exists because the shape
data is authored in is not the shape hardware consumes**, and reconciling the two is real work with
a real cost you should be able to quote. Anyone who has wondered why an exporter turns a tidy
model into a much bigger vertex buffer has met this without being told what it was.


## Number parsing is where a loader most easily starts lying (Lesson 3.5)

Three separate traps, all of which fail silently.

**Accept the whole token or reject it.** `strtof("1.0abc", &end)` returns 1.0 and points `end` at
the `a`. If you do not compare `end` against the token's end, a typo in a model file becomes
geometry instead of an error.

**`SDL_strtod` is not `strtod`.** SDL's header documents it as making *fewer* guarantees than the
C runtime's: "the handling of scientific and hexadecimal notation is unspecified". Exporters emit
`1.0e-5` constantly. Checked in `SDL3/SDL_stdinc.h`, not assumed — and it is the sort of thing that
would have looked like a natural choice for a program that already links SDL.

**`strtof` reads the decimal point through `LC_NUMERIC`.** On a machine whose locale writes `1,5`,
a program that has called `setlocale(LC_ALL, "")` parses `"1.5"` as **1** and drops the fraction,
for every number in every asset. We are safe only because neither we nor SDL calls `setlocale`.
`std::from_chars` is the principled fix — locale-independent by definition — but floating-point
`from_chars` was the last piece of C++17 to reach the standard libraries and arrived very late in
libc++, so it needs a `__cpp_lib_to_chars` guard rather than an assumption.


## Topology is a property of the surface, not of the array that encodes it (Lesson 3.5)

A uv seam stores one point of the surface twice, because the two copies need different texture
coordinates and a vertex cannot hold two values. That is correct and unavoidable. It also means
**the vertex array is an encoding, not the set of points** — so every topological question (is it
closed? is the winding consistent? what is V − E + F?) has to be asked of the *welded* mesh.

Ask them of the raw arrays and a perfectly watertight torus reports 48 boundary edges: a
seam-shaped hole in a model with no hole. It is a spectacularly confusing false alarm, because the
model renders perfectly and the number is precise.

The same distinction settles what a round-trip test should compare. Writing a mesh and reading it
back must preserve the *geometry* — the expanded list of triangle corners — not the vertex array's
ordering, which the loader has no way to reproduce and which means nothing.


## "Same point in principle" is not "same number", and welding needs the second (Lesson 3.5)

Generating a torus, the natural way to place the seam column is to compute its angle from its
texture coordinate: the last column has `u = 1`, so its angle is `1.0f * 2π`, and the first has
`u = 0`, so its angle is `0`. Mathematically identical positions. In `float`:

```
sin(0.0f)                    = 0
sin(1.0f * 6.28318530718f)   = 1.74845553e-07
```

A float cannot hold 2π exactly, so the two columns land 1.7 × 10⁻⁷ apart. Nothing renders
differently. But no welder recognises them, so the seam becomes a boundary and every check in the
previous entry reports a hole that is not there.

The fix is to compute the angle from the **wrapped index** (`i % nu`) so the last column literally
reuses the first column's angle — the two positions are then the same *number*, not merely the same
*point*. The general lesson: if two values must compare equal later, arrange for them to be
produced by the same computation, rather than by two computations that agree in exact arithmetic.


## Euler's formula is a statement about spheres (Lesson 3.5)

`V − E + F = 2` is how everyone learns it, and it is the special case. The real statement is
`χ = V − E + F = 2 − 2g`, where `g` counts the holes: a sphere, cube or icosahedron gives 2, a
torus gives **0**, a two-holed pretzel gives −2. χ is invariant under subdivision — cutting a face
in two adds one face and one edge, which cancel in the alternating sum — which is exactly why it is
a fact about the *shape* and not about the mesh, and why it can be computed on the triangulated
form and still be talking about the cube.

The practical consequence is a rule about validators: **χ is a diagnostic, not a validity
condition.** A loader that asserts 2 rejects every handle, link, chain and pair of glasses ever
modelled. The conditions that really are errors for a renderer are different ones — boundary edges,
non-manifold edges, inconsistent winding — and they should be reported as separate counts, because
"this mesh has 4 boundary edges" tells you where to look and "invalid" does not.


## The general test costs the same as the test that only works sometimes (Lesson 3.5)

Lesson 2.12 checked "wound outward" by taking each face's normal and dotting it against the vector
from the centroid. That silently assumes the solid is **star-shaped about its centroid** — that a
ray from the centre hits the surface once. It is true for an icosahedron and false for the first
torus you meet, whose centroid is in the hole.

The assumption-free test is the signed volume by the divergence theorem: sum `dot(a, cross(b, c))`
over every triangle and divide by six. Space outside the solid is swept an even number of times
with opposite signs and cancels; the interior is swept once. **The origin's position is
irrelevant**, which is precisely what makes it general. Positive means wound counter-clockwise seen
from outside — the property back-face culling depends on and cannot check for itself.

It is also barely more code than the wrong test, and it comes with a free numeric check: a unit
cube reads exactly 1.0, and a torus converges to `2π²Rr²` from below with second-order error
(12.3% → 3.2% → 0.8% → 0.09% as the resolution doubles). Verify a geometric predicate against a
closed form whenever one exists — a predicate that returns a *number* can be checked, and one that
returns a *bool* can only be believed.


## Distinguish "malformed" from "silly", and report both (Lesson 3.5)

A loader for real-world data has to answer a policy question before it answers any technical one:
which inputs stop the load, and which are absorbed?

The line that worked: **malformed is fatal, silly is counted.** `f 1/x/2` is not an OBJ file, so
guessing what it meant helps nobody — that stops with a line number. A face whose three corners are
the same vertex is a perfectly good OBJ file describing a triangle with no area, usually left by a
merge operation; real files contain these, so it is dropped and counted and the load succeeds.

Both halves have to appear in the report, which is the part that is easy to skip. A loader that
silently absorbs oddities and a loader that dies on real data are both unusable; what makes one
usable by somebody who is not its author is that the report says *how much* it ignored. `skipped
412 lines` is a very different statement from silence.


## A check-page pass is a floor, not a ceiling — two figures said false things (Lesson 3.5)

`check-page.js` reported `pass: true` on figures that were actively wrong, because it checks
*collisions*, not *claims*.

Figure 5 shaded two overlapping cones from an origin and asserted they cancelled outside the solid;
they were drawn at angles where the nesting was invisible, so the picture demonstrated nothing.
Figure 4 was worse: it drew a concave pentagon, highlighted the fan triangle `(0,2,3)`, and
captioned it "leaves the polygon" — and a point-in-polygon test on that triangle's centroid put it
firmly *inside*. The figure's central claim was false, and it looked plausible.

Both were caught by rendering the figure and reading it against its own caption, which is now the
habit. And the second one produced a better lesson than the wrong one would have: a fan is correct
**iff the anchor corner can see the whole polygon** (star-shaped about it). Convexity is the
*sufficient* condition everyone quotes, and it is sufficient because in a convex polygon every
corner works. So a concave face may fan perfectly from one corner and grow fins from another —
which means the bug depends on where the exporter started listing the face, and that is what makes
it appear in one file and not the next one that looks just like it.

When a diagram makes a geometric claim, **test the claim numerically**, not just the layout.


## A normal is not a direction — it is a relationship (Lesson 3.6)

The single most useful reframing in this lesson, and it generalises far past lighting.

Lesson 2.7 established that points have `w = 1` and translate, directions have `w = 0` and do
not. That makes it look as though "direction" is one kind of thing transformed one way — and a
normal is not that kind of thing. A **tangent** is a direction: it joins two nearby points on the
surface, so it goes wherever `M` sends those points. A **normal** is defined by a *property* —
perpendicular to every tangent — and it is the property, not the arrow, that has to survive.

Write the property down and the answer falls out in three lines. Demand
`dot(M·t, X·n) = 0` whenever `dot(t, n) = 0`; use `a·b = aᵀb` to get `tᵀ Mᵀ X n`; observe that
this reduces to `tᵀn` exactly when `Mᵀ X = I`, so `X = (M⁻¹)ᵀ`.

The transferable habit: **when you do not know how something transforms, ask what defines it and
require that to be preserved.** Tangent vectors, normals, planes, and covectors generally all
fall out of this, and it saves memorising a table of rules that look arbitrary.


## The bug that only appears on objects nobody is looking at (Lesson 3.6)

The inverse transpose is **identical** to the model matrix for a pure rotation (measured:
`max |R − normal_matrix(R)| = 5.96e−08`) and **parallel** to it for a uniform scale (`0.0000°` of
difference). It differs only under a non-uniform scale.

Rotation and uniform scale describe an enormous fraction of a typical scene. In our own demo the
uniformly-scaled icosahedron — the hero, the thing you are actually looking at — shows a
worst-case normal tilt of `0.03°`, i.e. float noise, while the squashed slab reaches **67.99°**
and the plinth **66.46°**. Rendered on a flattened torus, **97.5% of the object's covered pixels
differ**, by up to 135/255 in a channel.

So the failure mode is: *the hero object is perfect and the set dressing is subtly wrong.* Nobody
files that bug. The lesson for testing is to **choose test geometry that violates the assumption
you are least sure about** — and, better, to make the invariant checkable directly: take any
tangent, transform it with `M` and the normal with your candidate matrix, and assert the dot
product is still zero. Two lines, no rendering, no eyes.


## Fake shading gives itself away by what it does NOT do (Lesson 3.6)

`face_shade(base, face_index)` coloured every surface in this course for five lessons, and no
individual frame it produced ever looked wrong. The tell is not a bad colour — it is the
**absence of a relationship**: spin the object and the shading does not move, because
`face % 5` does not care which way the surface is pointing.

This is worth generalising into a debugging instinct. When something looks plausible but you
suspect it is not real, do not stare harder at one frame — **change an input that the correct
implementation must respond to, and check that it responds.** Rotate the object and watch the
shading; move the camera and watch the specular (and watch the diffuse *not* move, which is
equally informative). A still image cannot distinguish a computation from a lookup table; a
derivative can.


## Adding a light required no rasterizer changes, and that was informative (Lesson 3.6)

`fill_style` gained no field. `fill_triangle` gained no branch. Lighting's output is a vertex
colour, and interpolating vertex colours across a triangle has been the fill's job since Lesson
2.4, so a whole new subsystem dropped in with zero changes to the code that draws pixels.

That is not luck — it is the **vertex/fragment split** appearing before it was named. Lighting is
per-vertex work whose result feeds per-pixel work, and the pipeline already had that boundary
implicitly. Module 4 makes it literal with two shaders.

The corollary is the useful part: it also revealed that `fill_style` is the *wrong home* for
shading parameters. Specular colour and shininess belong to the surface, not the fill, and 3.7
will not be able to fit them there. When a new feature slots in with no changes, that is evidence
the boundary is in the right place; when the next one cannot, that is where the next abstraction
goes.


## Verify a figure's claim numerically, not just its layout (Lesson 3.6)

Lesson 3.5 already learned that `check-page.js` cannot see a false claim. Lesson 3.6 produced
three more instances, in one lesson, all of which passed the collision checker:

- **Figure 1** was drawn with the surface at 30° while its labels said 60°, so a reader measuring
  the picture would have got a footprint of 1.15 where the text said 2.00.
- **Figure 2** shaded the *wrong half* of the disc as unlit, drew the terminator along the wrong
  diagonal, and mislabelled one cosine as 0.28 where the geometry gives 0.10.
- **Figure 3** and its interactive widget disagreed with the worked example, because the figure
  squashed one axis and the example squashed two.

The fix that worked was to **compute the figure's coordinates in a script and read the labels off
the computation**, rather than placing them by eye and annotating them from memory. For Figure 2
that meant a five-line program printing each sample point's normal, its dot product with the
light, and the arrow's endpoint. Every number in the final SVG came from that output.

For an interactive widget the same rule applies with more force, because a reader *will* drag it
to the value the prose quotes: the widget, the figure and the worked example must all use the
same inputs. Ours now all use the slab's `(1.8, 0.35, 0.9)`, and dragging the slider to 0.35
reproduces the prose's 137.5° and 90.0° exactly.


## Folklore survives because nobody arranges the case that breaks it (Lesson 3.7)

The first draft of this lesson said, in five places including a figure and its `alt` text, that
Phong's highlight is cut off at grazing angles because **the mirror ray dips below the surface**.
That is the standard explanation. It is also false, and one line of algebra says so: `R` is `l`
mirrored about `n`, so `dot(n, R) == dot(n, l)` exactly — if the light is above the surface then
`R` is above it too, always, without exception.

What is actually happening is sharper and more useful. `cos^p` answers only over the hemisphere
*around R*, and that is not the *visible* hemisphere. With the light `a` degrees off the normal,
the visible directions the lobe fails to cover form a wedge exactly `a` degrees wide. So the
condition is not "grazing" — it is **the light and the eye on the same side of the normal**, which
on a floor means the sun is behind you.

Two things made the error survive as long as it did:

- **The measurements agreed with it.** Every number in the harness — `dot(R,v) <= 0` for 50.4% of
  above-surface pairs, 0 of 30,806 lit pixels highlighted at 35° sun elevation — is correct and is
  *equally* consistent with the wrong explanation. Passing tests confirm the arithmetic, not the
  story you tell about it.
- **The experiment was arranged to succeed.** The first plane render put the sun behind the
  camera, which is the configuration that shows the cut-off. Adding the *other* arrangement — sun
  ahead, the sunset-on-water case — showed Phong highlighting all 30,806 pixels with no cut-off at
  all, and that contrast is what forced the re-derivation.

The habit worth keeping: **when you find yourself repeating a phrase you did not derive, derive
it.** And ship the control that could have embarrassed you; `render_37` §6 now runs both
arrangements on purpose.


## A figure can pass every automated check and still hide the claim (Lesson 3.7)

`check-page.js` reported `pass: true` on a Figure 5 that was useless. It was a polar lobe plot on
a linear radial scale, and the entire point of the figure — that Blinn still returns 0.063 at 90°
where Phong returns nothing — was a dot fifteen pixels from the origin, indistinguishable from the
dot for 0.004. No label overlapped, nothing spilled the viewBox, and the geometry was exactly
right.

The fix was to change what was plotted, not where the labels went: value against angle, on
Cartesian axes, where 0.063 is 6% of the height and plainly visible. The polar view still earns
its place in Figure 1, where the question is the *shape* of the spray rather than the size of the
tail.

Generalisable: a collision checker verifies that a figure is *legible*. Whether it is *informative*
is a question about the mapping from data to ink, and the test is to state the figure's one claim
in a sentence and ask whether a reader could extract it by measuring the picture. Lesson 3.6
learned to compute a figure's coordinates rather than place them by eye; 3.7 adds that computing
them correctly is not sufficient.

Two smaller instances of the same thing in the same lesson, both invisible to the checker:

- The lobe in Figure 1 and in the interactive widget was drawn **below the surface line**, because
  `cos^p` is defined there. The function is; the surface is not. Both are now clipped at the
  horizon.
- The widget's default exponent was `p = 8`, at which *both* models read 0 in the cut-off region —
  so the widget's own caption ("watch Phong go to zero while Blinn does not") was refuted by
  dragging it. The default is now `p = 2`, and the caption explains that the difference between
  the models is a *rough-surface* difference.


## A fast path can be bought out by a feature, and it is worth naming when it happens (Lesson 3.7)

Through 3.6, `collect_triangles` composed `view_from_world * world_from_model` once per object and
sent each vertex straight to view space — one matrix multiply instead of two. That optimisation
was available *because* the shading was view-independent: a directional light is a direction,
Lambert compares two directions, and no world position was ever needed.

A highlight needs `eye - position`, which is a question about places. So the composition comes
apart and every vertex pays a second multiply.

Nothing went wrong here. But the instinct to record is that **an optimisation is usually a
simplifying assumption with a name**, and features cash those assumptions in. Being able to say
which of your fast paths depend on which of your assumptions is most of what performance work
actually is — and it is why "why is this slower than last month?" is so often answered by a
feature that nobody connected to the loop it slowed down.


## Passing tests do not mean the test measured the right thing (Lesson 3.7)

Three of this lesson's checks passed on the first run while measuring something other than what
they claimed:

- **The per-vertex peak on a 48×24 torus.** The claim was "per-vertex evaluation misses the
  highlight". The measurement found 99.6% of the true peak and looked like a refutation. It was
  the wrong measurement: with 1,225 vertices some vertex almost always lands near the peak. The
  real defect is the **chord error** *between* vertices (up to 0.722 of full strength at shininess
  128, on 87.8% of the lit area) and the **flicker** on coarse meshes (`cube.obj`: no highlight at
  all in 157 of 180 frames). Same claim, three different instruments, only two of them sensitive.
- **The grazing comparison on a torus.** A torus presents every incidence angle at once, so
  "lower the sun" changes nothing that was not already happening somewhere on the surface. The
  experiment needs a *plane*, where the incidence angle is the sun's elevation.
- **The `n·l` leak.** The first version parked the eye exactly on the mirror ray and swept the
  light past the terminator, which drives `dot(n,h)` to −1 and returns 0 for the right reason
  rather than the one under test. The geometry that exposes it is a fixed light and eye on
  opposite sides with the *normal* sweeping between them.

The pattern in all three: **the test was sensitive to something adjacent to the claim.** Before
trusting a green check, ask what result would have falsified it — and if you cannot construct one,
the test is decorative.


## A knob that will not extend is usually two knobs (Lesson 3.8)

Lesson 3.6 shipped `shade_mode { palette, flat, smooth }` and could not add `per_pixel` to it.
The instinct is to blame the missing value. The actual problem was that the enum held **two
independent questions** — where the normal comes from, and where the shading equation is
evaluated — and their combinations form a *grid*, which a list cannot represent.

The tell was there in the code and was even written down. 3.6's own doc comment said
"interpolating the *colour* across a triangle and interpolating the *normal* and shading each
pixel are different things". A comment explaining why a type cannot express something is a
comment describing a type that is the wrong shape.

The generalisable version: **when a new case will not fit an enum, check whether the enum is
enumerating one thing.** If two of its values differ in more than one respect, they are a
product and not a sum. Splitting them is nearly always cheaper than it looks — here it also made
three previously-invisible facts checkable, because a grid has cells you can predict and a list
does not.


## Predict, then measure — and write the prediction down where it can be wrong (Lesson 3.8)

`verify_38` §A computes, for each of six cells, whether it *should* differ from per-pixel, and
then measures. Writing the prediction as code rather than as a comment caught a real error within
one run: I had claimed `face × gouraud` was degenerate unconditionally, and the harness found
**761 differing pixels**.

The reasoning that was wrong is worth keeping, because it was nearly right. With a face normal
the normal is constant across the triangle, so the shading is constant, so all three evaluation
points agree. True — while the shading depends only on the normal. Lesson 3.7 added a term that
depends on the *position*, which varies across a face even when the normal does not, and the
argument silently stopped holding one lesson earlier.

Two habits come out of this:

- **A degeneracy is a theorem with hypotheses.** When you assert that two configurations produce
  identical output, list what the output depends on and check each item — rather than checking
  the one that motivated the claim.
- **The corrected rule was shorter than the wrong one.** `if (gouraud) return true;` became a
  single condition covering both cells. A rule with a special case carved into it is often a rule
  stated at the wrong level, and simplifying it is a signal you have found the right one.


## The folklore about shading cost has a precondition nobody states (Lesson 3.8)

"Per-vertex shading is cheaper than per-pixel" is universal, and at the sizes this engine renders
it is **false**. Measured on one mesh with the resolution swept:

| px / triangle | per-pixel ÷ Gouraud |
|---|---|
| 2.8 (320×180) | **0.91×** — cheaper |
| 11.0 | 1.41× |
| 44.1 | 1.84× |
| 396.9 (4K) | 2.15× |

The argument behind the folklore is sound: a mesh has fewer vertices than covered pixels, so
per-vertex is fewer calls. The unstated assumption is that a triangle covers *many* pixels. Our
torus has 2,304 triangles covering 6,346 pixels — 6,912 vertex shading calls against 6,346
fragment ones — and the comparison inverts.

Three things worth keeping:

- **The asymptote is the number to quote** (2.15×), not any single measurement. A ratio taken at
  one resolution is a ratio taken at one point on a curve.
- **Always report the px/triangle ratio with a shading timing.** Without it the number is not
  reproducible and not transferable, which makes it not a measurement.
- Modern content lives near the crossover: a 50k-triangle character on a quarter of a 1080p
  screen averages about ten pixels per triangle. This is why GPUs shade in 2×2 quads and why
  "too many small triangles" is a named performance problem.


## Fixing the wrong axis fixes nothing, however hard you push (Lesson 3.8)

The plan for this lesson predicted that per-pixel shading would take `cube.obj` from 157 blank
frames out of 180 to zero. It took it to **157**. The 12×8 torus went 58 → 0.

Per-pixel shading fixes an *interpolation* error. A cube has six normals, and whether any of them
points near the halfway vector is settled by the geometry before shading begins — so evaluating
the equation ten thousand times instead of twenty-four changes nothing, because all ten thousand
evaluations get the same normal.

This is the same lesson 3.6 learned from the other direction ("a faceted mesh is faceted because
of the split, not the shading model"), and it is the strongest argument for having separated the
two axes at all: **each fixes a class of defect the other cannot touch.** When a fix does not
work, the first question is not "did I implement it correctly" but "is this the axis the defect
lives on".


## A figure can be geometrically perfect and still refute its own caption (Lesson 3.8)

Figure 4 was captioned "continuous but not smooth" and plotted the brightness across six facets
under Gouraud against the true curve. Both were computed correctly. `check-page.js` passed. And
the dashed line hugged the true curve so closely that the figure appeared to show the two were
*the same* — the exact opposite of the point.

The content was in the derivative, not the value. Adding a second panel underneath, plotting the
slope of each, made it immediate: the true slope is a smooth curve, Gouraud's is a staircase that
jumps at every knot. Same data, same claim, and now the reader can see it.

This is the second time in two lessons that a figure passed every automated check while hiding
its claim (3.7's Figure 5 buried a tail on a linear radial scale). The rule that has emerged:
**state the figure's one claim in a sentence, then ask whether a reader could extract that exact
sentence by measuring the picture.** If the claim is about a rate of change, plot the rate of
change.

---

## A test failing is not evidence the code is wrong (Lesson 3.9)

Five of `verify_39`'s checks failed on the first run. **Three were defects in the test.** That
ratio is not unusual and it is worth internalising, because the instinct on a red test is to go
and read the implementation.

The three, and what each one was actually measuring:

| Symptom | What the test was really measuring |
|---|---|
| The 1:1 blit test failed while its **control passed** — which is impossible if the test is sound | This rasterizer samples attributes at **integer** pixel coordinates, so pixel `i`'s sample point is `i`, not `i + 0.5`. Lining that up with a texel centre at `(i+0.5)/N` requires offsetting the quad's uvs by half a texel. |
| Clamp addressing "smeared the wrong texel" | The probe sat at `v = 0.5`, which on an 8-texel image is **halfway between rows 3 and 4**. The sample was a blend of two rows, so comparing it against one texel was meaningless. |
| 141 of 1,225 uvs "differed" after a uniform import step | `load_obj` numbers vertices by **order of first appearance** (Lesson 3.5); `make_torus` numbers them by its construction loop. `uvs[i]` named different vertices in the two arrays. Compared as sorted multisets: worst difference `0.000e+00`. |

The rule: **when a test fails, first check that it is asking the question you think it is
asking.** Two of the three above failed by not holding a second variable still — a test of one
thing has to pin everything else at a value where it does nothing.

A control that *passes* when the real test fails is the loudest possible version of this signal.
It means the two are not measuring what their names say.

## The half-texel question exists at both ends of a pipeline (Lesson 3.9)

A texel is a **sample**, so its value lives at `(i + 0.5)/N`. That is the famous half.

The one nobody mentions: a **framebuffer** has exactly the same question, and this engine
answered it differently. `fill_triangle` steps its edge functions over integers, so a fragment's
attributes are evaluated at the pixel's integer coordinate. Pixel `i`'s sample point is `i`.

So a "1:1" blit is only bit-identical when *both* answers are reconciled — the quad's uvs must
run `0.5/N` to `1 + 0.5/N`, not 0 to 1. Neither convention is wrong; assuming they agree is.

Any time a continuous coordinate is mapped onto a discrete grid, ask where in the cell the value
lives. There will be a half somewhere, and there may be two.

## Nearest-neighbour sampling cannot see a half-texel error (Lesson 3.9)

Measured over 160,801 sample positions: removing the half-texel offset changes **0** samples
under nearest filtering and **160,632** under bilinear.

This is why the bug ships. A texture pipeline can carry it for years while everything looks
crisp and correct, and reveal it the day somebody enables filtering — at which point the symptom
is "filtering makes everything soft and slightly misaligned" and **the filter gets blamed**.

Generalises: a defect that only one of two modes can express will be attributed to whichever
mode was switched on last. When a feature "introduces" a problem, check whether it merely made a
pre-existing one visible.

## Hold the confound constant, or your benchmark measures the wrong thing (Lesson 3.9)

The obvious benchmark for "what does a texture fetch cost" compares a procedural rule against a
texture lookup. It reported **5.04×** for nearest and **6.80×** for bilinear.

Both numbers are nearly meaningless. `shading::uv_checker` returns a packed pixel and encodes
nothing; `shading::textured` decodes texels and **re-encodes** the result, and `linear_to_srgb`
calls `std::pow` — three per pixel. The benchmark was mostly measuring the sRGB transfer
function.

Making all three variants `shading::lit`, so every one pays exactly one encode, gives the honest
numbers: a nearest fetch costs **1.12×** a lit fragment and bilinear **1.41×**, i.e. bilinear is
**1.26×** nearest for four fetches and three lerps instead of one.

Both tables are kept in `verify_39` §I, labelled. **A plausible benchmark that measures the wrong
thing is more dangerous than no benchmark**, because it comes with a number and numbers end
arguments. Before believing a ratio, list what *else* differs between the two things you timed.

## Aliasing is not caused by a large footprint (Lesson 3.9)

The usual explanation — "one pixel covers many texels, so it aliases" — is incomplete, and the
measurement shows why. Four test images, all 64 texels wide, so the footprint at any given screen
row is **identical** for all four: 62.46 texels per pixel two rows below the horizon. The sparkle
under a sub-pixel camera nudge still went 8.0% → 16.2% → 32.6% → **64.8%** as the checker went
from 4 to 32 cells.

What changed was the **contrast inside the footprint**. Aliasing is caused by a pixel covering
many texels *that disagree*, and the sampler having no way to average them. A large footprint over
a smooth image is harmless.

The practical consequence: if shimmer scales with texture *fineness* at a fixed camera distance,
it is aliasing and not a filtering bug — and enabling bilinear will not help.

## Duplicate SVG marker ids are silent, and stop being harmless later (Lesson 3.9)

Every generated figure emitted the same `<defs>` block with the same four marker ids, so a page
with six figures declared each id six times. `url(#e-i)` resolves to the **first** match, so every
figure was quietly using figure 1's markers.

Harmless while all six definitions are byte-identical — which is exactly what makes it a trap. The
day one figure wants a different arrowhead, it silently gets somebody else's, and nothing in
`check-page.js` looks for it. Marker ids are now namespaced per figure (`e-i-f391`); Lessons 3.7
and 3.8 still carry the old pattern.

Worth adding to a page check: `[...document.querySelectorAll('[id]')].map(e => e.id)` and look for
repeats.

## `.t-inv` needs an opposite, and the choice is a computation (Lesson 3.9)

`course.css` provides `.t-inv` — a fixed white text fill — for labels sitting on a saturated
shape, with the right justification: the shape is the same colour in both themes, so its label
must **not** follow the theme's ink.

The first draft of Lesson 3.9's figures used it for numerals on *every* swatch, including two pale
ones (`#e2ded2`, `#c2bdae`), where white on light grey was barely readable. `check-page.js` cannot
see this — it checks label *geometry*, not contrast.

The fix is a page-local `.t-onlight` (fixed dark) and picking between them by **relative
luminance** in the figure generator, not by eye:

```python
lum = 0.2126 * r + 0.7152 * g + 0.0722 * b
return "t-onlight" if lum > 0.45 else "t-inv"
```

Contrast on generated figures is a computation, not a judgement call — and it is one of the
things only a screenshot will catch.

## The clock is coarser than it is expensive, and that inverts the rule (Lesson 3.10)

Two independent properties decide what you may measure, and the intuitive one is not the binding
one. On the reference machine (Apple M4 Pro):

| | |
|---|---|
| `SDL_GetPerformanceFrequency()` | **24 MHz** → a tick every **41.667 ns** |
| one `SDL_GetPerformanceCounter()` call | **5.42 ns** |
| one `scope_timer` (two reads + an add) | **13.46 ns** |

**Reading the clock is roughly eight times faster than the clock changes.** So the limit on what
can be timed is the *tick*, not the overhead — the cheap operation is the one that stops you.

One sRGB encode takes 9.3 ns, which means four and a half of them fit inside a single tick, and
bracketing exactly one returns **0.00 ns or 41.67 ns and never 9.3**. Measured, twelve trials at
N = 1: min 0.00, max 666.67.

The rule that falls out: **never instrument anything shorter than `100 × max(tick, timer)`** — the
duration at which quantisation and overhead are both under 1%. That is 4.17 µs here, and it is why
`profiler` exposes `resolution_ns()` and `overhead_ns()` and calibrates itself at construction
rather than carrying a number in a comment. Do not assume 1 GHz: the frequency is 24 MHz here, 1e7
on Windows, 1e9 on modern Linux.

A corollary worth keeping: **a zone that reads 0.00 is either work you are not doing or work you
cannot measure, and the two look identical in a table.** `zone::build` reads zero for the second
reason and is kept, deliberately, as the rule appearing in the engine's own output.

## Instrumentation makes things slower *and* under-reports, at the same time (Lesson 3.10)

Putting a `scope_timer` around each iteration of a loop measured 9.71 → **18.08 ns/iteration**,
a 1.86× slowdown. That much is expected. The part that is not:

**The reported total is also too small.** The closing clock read happens *inside* the interval it
is trying to close, so it cannot be part of what it measures. The instrument therefore makes the
program run twice as slow while claiming it ran faster than it did — two errors, in opposite
directions, from one decision.

Fine-grained answers come from **subtraction**, not from finer instruments: two variants differing
by exactly one thing, both timed coarsely. The resolution comes from the experiment's design
rather than from the clock's.

## A differential benchmark without a control is a story (Lesson 3.10)

This lesson's first draft reported that a texture fetch costs **6.40×** more in situ than in a
microbenchmark. The number was produced by comparing a loop containing *fetch + encode + two
stores* against a loop containing *fetch*, and attributing the entire difference to the fetch.

With a proper control — the same loop, the same encode, the same two stores, and only the fetch
replaced by a value that varies the same way — the honest figure is **2.27×**:

```
fetch alone, tight loop:          2.26 ns
fetch + encode + 2 stores:       14.60 ns
CONTROL: same, minus the fetch:   9.47 ns
-> the fetch IN SITU costs 5.13 ns, against 2.26 alone (2.27x)
```

Still a striking result, and it has the advantage of being true. **A function's cost is not a
property of the function**: in a tight loop consecutive fetches overlap in the pipeline, and in
situ each one sits in a dependency chain — its uv comes from the perspective divide, its result
feeds the encode — with nothing to overlap with.

The general shape, and this is the second sighting (Lesson 3.9 §5.1 was the first): **when a
measured ratio is much larger than you can explain, the two sides differ in more than one way.**

## Median for a frame, minimum for a kernel — never the mean (Lesson 3.10)

Timing noise is **one-sided**. The machine can always be slower than your code deserves — a
context switch, a core migration, a page fault, a frequency drop — and can never be faster. So the
distribution is a hard floor with a long right tail, and the mean, which assumes symmetry, is
wrong for both cases:

- **A kernel benchmark takes the minimum.** There is one true cost and everything above it is
  interference, so the smallest observation is the closest you got to running uncontended.
- **A frame budget takes the median.** There is *no* single true cost — a frame genuinely varies —
  and the question is what a typical frame costs. One 40 ms hitch moves a mean of 120 frames by a
  third of a millisecond and moves the median by nothing.

Say which one you used. A performance number quoted without it is a rumour.

## The unmeasured remainder is the most important row in a budget (Lesson 3.10)

Six honest bars that sum to 60% of a frame look like a complete picture, right up to the moment
you make the biggest one twice as fast and the frame improves by nine percent. **Display the
remainder.** Ours is 0.2% of the frame — *because it is displayed*, which is the only reason it
stayed small.

Two related rules. Zones must be **disjoint**: two live timers count the same nanoseconds twice,
so nesting is an error rather than a feature (`zones_overlapped()` catches it). And a **sum of
medians is not the median of a sum** — they differ whenever two zones peak on different frames.
Leave the discrepancy visible; deriving one number from the others so the table adds up perfectly
is how a budget stops being a measurement.

## Triangle count is close to irrelevant; pixels per triangle is the axis (Lesson 3.10)

The prediction everyone makes, including the first draft of this lesson: the 2,304-triangle torus
must cost about a thousand times the 2-triangle floor. Measured:

| | triangles | covered px | fill | ns/triangle | ns/px |
|---|---|---|---|---|---|
| floor | 2 | 30,882 | **1,390.14 µs** | 695,069 | 45.01 |
| torus | 2,304 | 2,678 | **171.78 µs** | 74.6 | 64.14 |

**The floor is eight times more expensive with 1,152× fewer triangles.** The only column where the
two objects resemble each other is the last one, and that is what the fill is paid for.

The two phases sit on perpendicular axes, and both were swept to confirm it: 16× the pixels moved
`collect` by 3.7% (noise) and `fill` by 15×; at constant screen coverage, 36 → 36,864 triangles
moved `collect` by 840× and `fill` by 3×. **The two curves cross near one pixel per triangle**,
and that crossing is the only place on either axis where "optimise the fill" stops being right —
with nothing about the renderer changing to get there.

Consequence for how numbers get recorded: **quote ns per covered pixel and ns per triangle, never
ms per frame.** The first two are properties of the fill loop and the vertex stage and transfer
between machines and resolutions; the third is a fact about one scene on one computer, and it is
the only one most people write down. Reference values: **45.47 ns/px**, **23.0 ns/triangle**.

## Cost does not follow attention (Lesson 3.10)

The differential ladder over the fragment loop, in ns per covered pixel:

```
coverage + colour interpolation   2.682
+ perspective divide              2.664   (-0.018 — FREE, below the noise)
+ depth test & write              2.936   (+0.272)
+ sRGB encode (std::pow)          9.024   (+6.088)  <-- the largest single item
```

The perspective divide had a 5,000-word lesson written about it (3.2, which warned it would cost
you) and is free — it hides entirely behind other latency. The depth test has a lesson named after
it and costs a quarter of a nanosecond. `to_encoded` is a one-line call at the end of the fragment
that nobody has thought about since Lesson 2.4, and it is **2.1× everything above it combined**.

## Judge an approximation before rounding, not after (Lesson 3.10)

Four implementations of `linear_to_srgb`, all four passing the sRGB round trip on all 256 stored
values:

| candidate | ns/call | speedup | error (codes, un-rounded) | worst rounded | % differing |
|---|---|---|---|---|---|
| exact `std::pow` | 3.497 | 1.00× | — | 0 | 0.00% |
| **fitted sqrt chain** | **1.856** | **1.88×** | **0.0115** | 1 | 0.60% |
| threshold table + bsearch | 6.919 | 0.51× | exact | 1 | 0.00% |
| uniform input table, 4096 | 0.994 | 3.52× | 0.4022 | 1 | 3.66% |

Two findings worth carrying:

**The exactly-correct table is slower than `pow`.** Tabulating the 255 *output* thresholds and
binary-searching them is exact by construction and a genuinely good idea — and eight dependent L1
loads is a longer latency chain than a modern `powf`. It exists in the harness only because
somebody timed it instead of shipping the argument for it.

**The "worst rounded code" column cannot decide anything.** It saturates: every candidate accurate
to better than half a code reports 1. The column that separates them is the error *before*
rounding — 0.0115 against 0.4022, a factor of 35 — and that gap becomes the entire answer the
moment the target stops being 8-bit: at 16 bits they are **3.0 and 103.4 codes**. An approximation
that is only good enough because the output format is coarse has an expiry date.

## A hypothesis that fails should ship with its result, not be deleted (Lesson 3.10)

The argument for rejecting the fastest sRGB candidate wrote itself: a 4,096-entry table occupies
16 KiB of cache, and a fill loop is already dragging four megabytes of colour and depth buffers
past the same cache, so its microbenchmark advantage should evaporate in situ.

Measured, with a framebuffer-sized stream running alongside each candidate:

```
exact (std::pow)                 3.492     3.576     1.02x
fitted sqrt chain                1.853     1.870     1.01x
threshold table + bsearch        6.959     7.234     1.04x
uniform input table, 4096        1.020     1.003     0.98x
```

**No penalty, for any of them.** This machine's L2 is large enough that 16 KiB alongside four
megabytes of streaming costs nothing measurable. The hypothesis is false here — and it is kept, in
the harness and in the lesson, *with its result*, because shipping the right decision with the
reasoning that failed is exactly how folklore gets manufactured. The next person inherits "tables
blow the cache" as received wisdom with a measurement sitting right there that says otherwise.

The real reason the polynomial ships is the accuracy column above, and the comment in
`colour.cpp` says so.

## The cache cliff is not where the folklore puts it (Lesson 3.10)

A texture-size sweep at constant sample count and constant access pattern — 2²¹ random samples,
so only the working-set size varies:

```
   size        KiB   nearest ns   vs 32x32
  32x32          4         2.25      1.00x
 128x128        64         2.32      1.03x
 512x512      1024         2.29      1.02x
1024x1024     4096         2.34      1.04x
2048x2048    16384         3.88      1.72x
```

Everything up to **four megabytes** is free on this machine. "The texture must fit in L1" is advice
for a different computer, and repeating it here would be quoting somebody else's constants.

Two method notes. Sample **randomly**, not sequentially — a sequential walk is prefetched perfectly
and measures the prefetcher. And hold the sample count fixed, so the only thing varying is where
the data lives.

## Amdahl's law is an arithmetic check on your own work, not a slogan (Lesson 3.10)

`speedup = 1 / ((1 − p) + p/s)`. Use it in both directions, and the second is the one people skip:

- **Before** — as a filter. Measure `p`, compute the ceiling `1/(1 − p)`, and decide whether the
  work is worth starting at all. A phase that is half your frame can never buy more than 2×.
- **After** — as a check on yourself. `p`, `s` and the whole-frame speedup are *not independent*.
  If the frame improved by more than the formula allows, you mismeasured something. Verified in
  `verify_310` §H: ceiling 1.284×, measured 1.287×.

## An engine-wide default is a decision about the repository, not about renderers (Lesson 3.10)

`fill_style::encode` defaults to `exact` even though the demo selects `fast` and `fast` is the
right answer for any real-time renderer. The reason is not technical: every measured claim in
Lessons 3.1–3.9 — "0 of 4096 texels differ", "bit-identical", "17,275 px, 0 differ" — was made
against the exact encode, and a default that silently moved 0.60% of those pixels by one code
would quietly falsify nine lessons' arithmetic.

Same bargain as `vertex::inv_w = 1` (3.2), `lights = nullptr` (3.8) and an unbound `albedo`
(3.9): **a default that changes nothing is what lets a feature be added to a pipeline object
without auditing its call sites.**
