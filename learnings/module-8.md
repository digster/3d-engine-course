# Module 8 — learnings from its lessons

Moved verbatim from LEARNINGS.md on 2026-09-27, in the original order; LEARNINGS.md
is now the index. Append new sections at the end, and add each heading there.

---

## …and the fourth place is the one nobody can see

Lesson 8.1 found the sequel to the entry above, one lesson after it was written. `docs/index.html`
states the course's size in four places: the hero stats, the curriculum prose sentence, the module
subtotals — and `<meta name="description">` in the `<head>`. Checks 1 and 2 verified the first three,
because those are what a human looking at the rendered page can see. The fourth said
**"A 94-lesson course"** for eight days and nine published lessons after the reshape to 107.

It is the one number on the page that is not *on* the page: it is what a search engine indexes, what
a link preview shows, and what anybody sharing the course sees before they open it. Nothing renders
it, so nothing about reading the page could ever have caught it.

`check_meta_description` is now check 10, and it was proved by editing the number back to 94 and
watching it fail — the only way to learn anything from a check that stays quiet. The rule 7.8 wrote
("assume there is a fourth place") was right, and the correction to it is sharper: **enumerate the
places by grepping for the value, not by remembering where you put it.**

## Order of accuracy is the `h → 0` question; a game asks the `t → ∞` one

The standard yardstick for an integrator — halve the step, see how much the error at a fixed
simulated time shrinks — reports explicit Euler and semi-implicit Euler as *comparable*, and on a
harmonic oscillator it reports semi-implicit Euler as **second order** (measured slope 2.002 against
explicit's 1.089). Neither number can see that one of them multiplies its error by 1.9 every second
while the other adds a constant.

Two separate traps, both worth carrying:

- **The question is wrong for the application.** Order describes behaviour as `h → 0` at fixed `t`.
  A game fixes `h` at 1/60 forever and runs for an hour. Measuring error against *time* at fixed `h`
  shows the difference immediately: 2.883e-3, 5.766e-3, 1.442e-2, 2.884e-2 in exact proportion to
  `t` for one rule, and 0.388 → 4.8e5 for the other.
- **A good number can be an accident of the test problem.** Semi-implicit Euler is a first-order
  method. It reads second order *here* because its amplitude error on this particular system is
  exactly zero — which is the property being investigated — leaving only a frequency shift, which is
  `O(h²)`. Reporting that as "second order" without the qualifier would be a confidently wrong
  statement that generalises to nothing.

The instrument that *could* see it was the determinant of the update matrix: `1 + h²ω²` against
exactly `1`. When a metric says two things are equivalent and you can see that they are not, the
metric is answering a different question — find out which one.

## A search with a fixed budget returns an answer even when the answer is "none"

Bisecting for the largest stable step size works beautifully for semi-implicit Euler (measured
0.3182939 against a predicted `2/ω = 0.3183099`). Run the identical bisection on explicit Euler,
which has **no** stable step size, and it returns `3.424e-3` — a number that looks exactly like a
stability limit and is nothing of the kind. What it found was the largest step at which the energy
had not yet doubled *within the 20,000 steps the test happened to run*. Run 200,000 and the "limit"
moves; the number is a property of the test's patience.

The same shape appears wherever a bounded search is asked an unbounded question. State the budget in
the output, and make the honest case visible: `max_stable_step()` returns **0.0** for explicit Euler
rather than a small positive number, because a caller who writes `h = 0.5f * max_stable_step(...)`
then gets a simulation that does not move — which is a far better bug report than one that quietly
explodes twenty seconds into a playtest.

## A microbenchmark can measure the optimiser's mood

Timing three integrator variants with the rule passed as a **runtime** `enum` made explicit Euler 15%
faster than semi-implicit in one run and 30% slower in the next, because whether the compiler hoists
a `switch` out of the loop (loop unswitching) is not stable across builds or data. The measurement
was real; it just was not a measurement of the code.

Making the rule a template non-type parameter — `timed.template operator()<R>()`, one monomorphic
function per variant — dropped the run-to-run spread to 0.3%, which is what let the actual finding
stand: the two Euler rules cost the same (0.994 ns against 0.963 ns per body per step), so choosing
the correct one is free.

Related, and the opposite of the intuition: the **non-template** overload that lives in
`integrate.cpp` measured **slower** than the header template (1.222 ns against 0.963), because it is
a real call into `libengine.a` and cannot inline across the static archive. "Just put it in the .cpp"
is not always tidying up.

## check-page.js walks a path's stroke, so a text-on-shape hit is real

When `svgTextOnShape` fires it is not a bounding-box false positive — the check samples points along
the actual stroke of each path and the perimeter of each hollow rect. Lesson 8.1's first build
produced five hits, and all five were labels genuinely lying on the curve they named.

The fix that works is the rule `figs_78.py`'s docstring already stated: **legends and annotations go
outside the plot.** Nudging a label a few pixels inside a panel that contains a long curve just moves
it onto a different part of the same curve. Moving it under the frame, or into the panel's strapline,
resolves it permanently and reads better.

## A figure can be broken by the renderer rather than by the data

Lesson 8.1's demo screenshot came out as a *dashed* spiral, which looked like a bug in the RLE
encoder. It was not: a scanline through the grid had nine red cells, contiguous in pairs, exactly as
it should. The gaps were browser rounding — a 1-unit `<rect>` in a 900-unit `viewBox` is about 0.7
device pixels once the figure scales down to a phone, and single-cell runs along a thin arc simply
vanish.

Resampling at `cell=3, px=1.5` gives the same size on the page with cells large enough to survive,
and halves the file. The general rule is the one the SVG-text learnings already state in another
form: **check what the renderer did before concluding something is wrong with the data.**

## `figs_NN.py` can end up mixing `\uXXXX` escapes with literal Unicode

Successive patch scripts wrote both forms into `scratch/figs_81.py`: some label strings contain the
six characters `×` and others contain a literal `×`. A `str.replace` written against one form
fails **silently** against the other, and three label fixes were lost that way — noticed only because
`check-page.js` kept reporting the same collisions after they had supposedly been fixed.

Two defences, both cheap: match on a distinctive ASCII substring rather than on a line containing the
character, and **assert the replacement count** in every patch script. A `replace` that matches
nothing is the most common silent failure in this repository's authoring pipeline.

## The second copy of a number is the one a reader actually sees

Twenty minutes after check 10 landed, the same fault line produced a second instance. A lesson's hour
estimate is written twice: in the index row's `<td class="hrs">` and in the page's own `<dt>Time</dt>`
block. They are edited at *different moments* — the page when the lesson is drafted and the estimate
is a guess, the row when it lands and the real cost is known. Lesson 8.1 shipped at 6 h, the index row
and both module subtotals and the course total all moved together, and the page went on saying 5.

Checks 1 and 2 could not see it, because they only ever read the index, where all four numbers agreed
with each other perfectly. **Self-consistency is not correctness when the fact has a copy elsewhere.**

`check_page_hours` is check 11, and on its first run it found two *pre-existing* drifts: Lessons 3.2
and 3.3 said "3–4 hours" against index rows of 5. It is deliberately a **range** test rather than an
equality, and that is a fact about the corpus rather than a softening — pages spell that field at
least four ways (`≈ 4 hours`, `≈ 4–5 hours`, `&asymp; 4&ndash;5 hours`, and one with a parenthetical
after it), so the honest question is whether the index's single number falls inside what the page
claims. An equality test would have flagged fourteen pages that are wrong about nothing, and a check
with fourteen false positives is a check nobody runs.

## Hand-picked test data agrees with the code by construction

Lesson 8.2 checked whether `(g / inv_mass) * inv_mass` recovers `g` in `float`, using seven masses:
0.001, 0.1, 1, 7, 100, 1000 and 1,000,000 kg. All seven were exact. That looked like a proof and was
nothing of the kind — those are the masses *a person picks*, and a person picks powers of ten.

A log-uniform sweep of a million masses over the same range found **15.9937% that are not exact**,
worst error one ulp. The first failing mass is not exotic either: **1.000145 kg**.

The general shape is worth keeping: when a check passes on every value you thought of, the next move
is not to conclude the property holds. It is to try values nobody would think of, in bulk, and to
report the *rate* rather than a verdict. The follow-up measurement matters too — through the force
route two such bodies differ by 1 ulp after one step, 1 ulp after sixty and are **identical again**
after six hundred. The error wanders rather than accumulating, which is invisible in a fall and fatal
to a replay, a lockstep session or a golden test.

## A control can have a bug and not fail — it produces a third number

Lesson 8.2 §7 compares a jump applied as an impulse against the same jump as a one-step force, and
controls it by applying the force for however many whole steps fit in a fixed *duration* — which
should recover the impulse behaviour, because that is what an impulse is.

The first version held it for `round(rate / 60)` steps. At 144 Hz that is two steps = 13.9 ms, not
16.7, so the control delivered a different impulse and reported a value matching neither arm. It did
not fail; it invited an explanation. A control that disagrees with both arms is not evidence of
anything, and the reflex when one appears should be to audit the control before the finding.

## Alternate the arms, or you are timing the machine

The first draft of Lesson 8.2 §11 timed four variants of a loop one after another and reported the
*same* arrangement at **2.625, 1.758, 1.476 and 1.459 ns/body** on four consecutive runs of one
binary. The machine was still settling from a cold start, and whichever arm ran first always lost —
which would have produced a confident, reproducible, entirely fabricated finding about whichever
design happened to be listed second.

`engine/core/bench.hpp`'s `bench_compare` (Lesson 5.6) exists for exactly this: one rep of each arm,
alternately, so both see the same thermal state within microseconds. The **ratio of medians** then
repeats to three decimal places across separate runs, and that ratio is the thing to quote.

Two follow-ons. `bench_result::spread()` over 2000 reps is dominated by a handful of scheduler
outliers and reads as 1–4 on a machine behaving perfectly — so print `min` beside `median` and do not
treat a large spread as a reason to distrust a stable ratio. And `bench_ab::agree` is only meaningful
when the two arms are two spellings of the *same* arithmetic; printing "agree no" for two arms that
answer different questions looks like a failure and is a category error.

## Print the discrete prediction, not only the continuous one

Velocity damping's terminal speed is `g/k` — 19.6200 m/s at the values Lesson 8.2 uses — and the
simulation settles at **19.5383**. A 0.42% gap, far too consistent to be noise.

There is no bug. `g/k` is the fixed point of the *continuous* system and we run a loop that applies
gravity and then damps, whose fixed point is `g·h·e^(−kh) / (1 − e^(−kh))` = **19.5384**. It tends to
`g/k` as `h → 0`, which is the sense in which the textbook formula is right.

A harness that printed only the continuous form would have sent somebody looking for a bug in
`apply_drag`. Whenever a closed form and a discrete loop differ by more than the effect being
measured, print both and name which is which.

## Coincident curves look like one curve, and the proof looks like the bug

Two of the three resistance models in `demos/bodies` make three bodies of very different mass follow
the *same* trajectory to the pixel. Drawn as three solid curves on top of one another, the picture
that proves they agree is indistinguishable from a picture of a renderer that lost two of them.

The fix is interleaved dashes: each body draws one run in three, so a single line cycling amber,
green, blue is three trajectories in exact agreement, and three separate dashed lines are three that
parted company. `demos/bodies/main.cpp` does it in pixels and `scratch/figs_82.py`'s `opoly` does it
in SVG with the same phases. Whenever a result *is* that several things agree, the drawing has to
make the agreement visible rather than rely on the reader's trust.

## `check-page.js` at 390 catches what 1280 cannot

Lesson 8.2's manifest table has one path — `engine/include/engine/phys/rigid_body.hpp` — that is two
characters longer than 8.1's. Inside `<code>` it cannot wrap, so at phone width the table forced the
whole **page** to scroll horizontally (`pageScrollsX: true`), which is completely invisible at
desktop width and which no amount of reading the HTML would reveal.

`.tbl-scroll` around the table fixes it. Two rules follow: run `check-page.js` at **both** widths
before shipping a page, and wrap any table whose first column can hold a long unbreakable token.

## A derivation says *what* to compute, not *how* — third time

`|r|²·1 − r⊗r` is what the vector triple product hands you for a point mass's inertia tensor, and it
is the form every textbook prints. Its diagonal is `|r|² − x²`: a sum of three squares with one of
them subtracted straight off again. For a sample at `r = (1000, 0.001, 0)` the true `Ixx/m` is
`1e−6`, and a `float` computes `1000000 − 1000000` and returns **exactly zero** — the `1e−6` was
never *in* the sum, because the ulp at 1e6 is 0.0625.

`−m·[r]ₓ[r]ₓ` — the cross-product matrix squared and negated — is the same algebra, builds the
diagonal from `y² + z²` directly, never forms the cancelling sum, and is nine multiplies instead of
two matrix products. `inertia_of_point` ships that; the derived form appears nowhere in the engine.

The consequence of getting it wrong is *behavioural*, not numerical: a zero principal moment makes
the tensor singular, `inverse_inertia` refuses it, and every plank, rail, sword and lamp post in the
level silently will not rotate about its own length — while everything else about the body works.

This is the third instance of the same class in the course: Lesson 6.16's `perspective()` formed
`A + 1` with `A = −1.003009` and amplified one ulp by 332×, and Lesson 8.2's mass round trip failed
for 16% of masses. **Whenever a formula subtracts two things that are nearly equal, look for the
algebraically identical version that does not.**

## Convention bugs hide behind symmetric test data

`Rᵀ I R` instead of `R I Rᵀ` is the easiest inertia-tensor mistake there is, and nothing structural
catches it: the result is symmetric, has the same trace, the same determinant, the same principal
moments, satisfies the triangle inequality, and `inspect_inertia` reports it `usable`. It is a
perfectly good tensor — the basis change in the other direction — and is out by 1.569413 on a body
whose moments run 1.67 to 4.33.

Worse, the obvious behavioural test does not catch it either. Rotate the body 90° about `z` and
check that the world `Ixx` equals the body `Iyy`: **both sandwiches pass**, because a right angle is
its own inverse up to a sign on a diagonal tensor. Lesson 8.3 §6 uses 0.7π about `(1, 2, 3)`
normalised for exactly this reason.

Test transforms with a *generic* rotation, never an axis-aligned right angle. The same warning
applies to winding order, to matrix/vector multiplication order, and to anything whose bug is a
transposition.

## Validation catches the impossible, not the merely wrong

`inspect_inertia` tests symmetry, positive moments, the triangle inequality and usability — and a
compound body assembled *without* the parallel-axis shift passes every one of them while being
**15.3× too small**. It has to: the wrong answer is a genuine inertia tensor, of a body whose parts
are all piled on top of each other at the balance point. That body could exist. It is just not the
one you meant.

The same shape of failure is waiting in contact generation, where a perfectly well-formed contact can
be between the wrong pair of features. A validity check is a lower bound on correctness and is worth
having; it is not a correctness test, and treating it as one is how a 15× error ships.

## Measure in the frame the equations are written in

Euler's equations are **body-frame** equations. `rigid_body` stores `angular_velocity` in world
space (deliberately — 8.9's solver needs one common frame). For a body spinning about its own `y`
axis, the two perturbation components are carried around `y` at the spin rate, so a world-space
`ω.x` oscillates at 10 Hz while its envelope grows.

Lesson 8.3's first measurement of the intermediate-axis growth rate sampled that world-space
component at threshold crossings and fitted **3.8498** against a predicted 4.8038 — a 20% miss that
looked like a failed prediction and was a failed frame. Everything in §10 now measures
`transpose(mat3_from_quat(q)) * ω`.

**The tell was that the spurious oscillation was at the spin rate.** A perturbation has no reason to
know about the spin; when a measured signal carries a frequency that belongs to something else in the
system, suspect the frame before the physics.

## A benchmark that copies state measures the copy

Lesson 8.3 §12's first draft wrote each arm as `body_world w = seed; … w.step(h)` so that both arms
would start from identical state. That is a reasonable instinct and it put a **557 KB copy of the
body pool inside the timed region**: every arm reported 34–58 ns/body against 8.2's 1–2, and every
ratio came out near 1.000 — *including the ratio the comparison was supposed to produce*.

The control is what should have caught it immediately. Two identical arms at ratio 1.000 is correct;
two identical arms at 1.000 **while every real comparison is also 1.000** is the signature of a
benchmark measuring something both arms have in common. Build the state once, outside the timed
region, and let the arms diverge — for a comparison of *work*, the state only has to be
representative, not identical.

## The largest optimisation is usually not an algorithm

The same section then found the real cost: `mat3_from_quat` was being called **twice from the same
quaternion, twenty lines apart**, once inside `world_inverse_inertia` and once inside
`angular_momentum`, in two functions that were each perfectly reasonable on their own. Hoisting it
and storing the forward inertia tensor beside its inverse took the step from 33.793 to
**18.778 ns/body** with no change to a single result.

Before reaching for a better algorithm, itemise what the loop actually calls and look for the same
pure function invoked twice on the same argument. Composable helpers make this *easy* to do by
accident, and a profiler shows it as time in the helper rather than as a duplicate.

## The failure mode lives where the algorithm converges

Lesson 8.5's first draft shipped Ericson's `ClosestPtPointTriangle` — the seven-Voronoi-region
solver every GJK implementation copies. It passed 200,000 uniformly random triangles. It failed
**1,048 times in 200,000 slivers**, worst case 14.5% wrong, returning a point that was *not on the
triangle at all*: its three region tests are signed areas, differences of nearly equal products, and
when they go to noise the version it selects returns the foot of the perpendicular on the triangle's
plane instead.

The general lesson is not about triangles. **GJK's whole job is to drive the simplex onto the
closest feature**, so by the last two iterations of every query the triangle is as thin as the
problem allows and the origin is a fraction of a millimetre from it. The degenerate case is not a
corner the algorithm occasionally visits — it is where the algorithm *spends its time*, which is why
a uniform-random fixture measures nothing about it.

Before trusting a subroutine inside an iterative solver, ask what the solver's own convergence does
to that subroutine's inputs, and build the fixture from that rather than from a distribution.

## An invariant you can check is worth more than one you believe

`|v|` is monotonically non-increasing across GJK iterations *by construction*: each new simplex
contains the last, and the nearest point of a superset cannot be farther. Two lines test it. They
caught two unrelated failures — a loop cycling on ordinary input, and the sliver bug above, which
showed up as eighteen iterations converging cleanly to 0.0233827 m and then one reduction returning
**0.0674522**.

The block *restores the last good state* rather than merely breaking, which is what turns a
terminator into a repair. When a derivation hands you a monotonicity property, spend the comparison:
it costs nothing and it is the only thing in the loop that can notice a subroutine lying.

## A proof outranks a decision

The same loop banks the best lower bound it has proven — a single positive `dot(v, w)` is a complete
proof of separation, because `w` minimises `dot(x, v)` over the whole set — and then *refuses* any
later containment test that contradicts it. Measured on the case that motivated it: a sphere and a
turned box 55 mm apart, fourteen clean iterations, and then a tetrahedron whose four face tests all
reported the origin inside, turning a 55 mm gap into a contact.

When two pieces of evidence disagree, they are rarely equally trustworthy. Prefer the one that is a
theorem about a maximum over the one that is the sign of a determinant on a degenerate shape.

## A reference that rounds the way one arm rounds is not a reference

Lesson 8.5 §H compares two formulations of the same GJK — one taking support points relative to each
shape's centre, one in world space — walked away from the origin. Its first "exact" reference built
the box corners as `centre - half` **in float**, which is precisely what the world-space arm does.
The reference therefore agreed with the arm it was meant to convict, and reported the *relative*
form as 96,270× worse. Promoting to double before subtracting flipped the result.

A reference must be exact arithmetic on the *inputs*, not exact arithmetic on one arm's
intermediates. This is 4.8 §3's finding — a harness that constructs its own configuration tests that
configuration — arriving one level down, in the reference rather than in the fixture.

## A threshold's units decide which constant it may share

Two constants in `gjk.cpp` look interchangeable and are not. The termination test uses the caller's
`tolerance`, which is dimensionless and relative; the duplicate-vertex test must not. Using
`tolerance` there was a quiet disaster on curved shapes: a capsule's support point moves
continuously with the direction, so consecutive iterations produce points a fraction of a millimetre
apart — "duplicates" by a 1e-4 relative measure — and the loop exited before the bounds converged,
reporting **2.4% certificate slack where the tolerance promised 0.01%**, on results not flagged as
stalled and therefore looking trustworthy.

A duplicate test is asking "is this the same vertex?", which is a question about float resolution
(1e-12 relative), not about how accurate the caller wants the answer. Before reusing a tunable, ask
what question the new test is asking and whether it has the same units.

## A comparison whose two sides are mathematically equal has no correct answer

`epa.cpp`'s visibility test asks whether a new support point is beyond a face's plane:
`dot(f.n, w) > f.d`. On two cubes meeting face to face, the support point lands *exactly* on four of
the polytope's face planes, so both sides are the same number computed two different ways and the
last few bits decide the verdict. Measured: three faces classified visible, scattered around the
polytope and sharing no edge, nine horizon edges with nothing to cancel, and a surface that was no
longer a polytope — reporting a depth 42% low.

The arithmetic was not wrong; the *question* was. The repair is to ask one whose answer is stable:
flood fill the visible set from the closest face, which is the one face the caller already proved
visible. In exact arithmetic that changes nothing, because the visible set of a convex polytope from
an exterior point is connected; in float it projects a scattered classification back onto the answer
exact arithmetic would have given.

Whenever a predicate's two sides can be equal by construction — coplanar faces, collinear points,
equal projections — the tie-break is arbitrary and the code must not depend on which way it falls.

## Count how your code finishes, not only what it returns

`seed_polytope`'s first version stitched six faces from a triangle to two apexes. A bipyramid is
convex only when each apex projects *inside* the triangle, and **52.85%** of seeds did not qualify —
but only **2.3%** of queries answered visibly wrong, because the first expansion usually deleted the
offending face and repaired the surface by accident. Nothing asserted, nothing crashed, and the
answers were mostly right.

What made it visible was a histogram of `epa_status` over 50,000 pairs: 1,172 `stalled` where there
should have been none. A four-byte status field on a result struct is worth more than it looks,
because the interesting failures are the ones that still produce plausible output.

## Ask the question late, where it is a measurement

Twice in one lesson a check placed at the *start* of `epa_penetration` had no correct threshold. "Is
the origin inside the seed polytope?" was tested against zero (wrong: GJK can terminate on a segment
through the origin on a deep overlap, so the origin sits on an edge where the offsets are zero to
within a few ulps — a depth of zero for spheres 70 cm inside a crate), and then against
`cfg.tolerance` (worse: the margin that put the origin outside was set by *GJK's* tolerance, so
tightening EPA's made the failure come back).

There was no constant that worked, because the question was being asked before the information
existed. The expansion does not care where the origin is, so it now runs either way and the answer
is read off the lower bound at the end — where it is a measurement rather than a guess. When a
guard needs a magic number, check whether it is a guard that could have been a conclusion.

## A sweep finds what a sample misses

Nine lines walking two cubes face to face from 10 cm of overlap down to zero found two of Lesson
8.6's four bugs. Two hundred thousand random overlapping pairs found neither, and both had been
green through several full harness runs. A random population samples the *middle* of a parameter's
range; a sweep visits its decades, and bugs live where a quantity becomes small compared with
something else.

Pair it with the converse, which the same lesson also produced: a fixture can be a null result. The
first version of §H.3 gave two support formulations half extents of **0.5**, which is a multiple of
the float grid spacing at every scale below 2²¹, so `centre + half` stayed exact a million metres
from the origin and the two arms agreed to the bit. Half extents of 0.37 are a multiple of nothing,
and the naive arm degrades to a centimetre. A null result from a fixture that cannot express the
effect is not a null result.

## A default member initialiser can be a memset

`struct face { int v[3] = {0,0,0}; vec3 n{}; float d = 0.0f; bool alive = false; };` cost
**20.5% of an EPA query** — 1344.04 ns to 1068.14 ns, measured back to back, when the four
initialisers were deleted. A
default member initialiser on any member makes the whole type non-trivially-default-constructible,
and that property propagates into arrays of it, so declaring `polytope p;` wrote four kilobytes of
zeroes that the next line overwrote. `expand`'s 3 KB horizon array is declared *per pass*, so a
ten-expansion query paid it ten times.

The tell is the array, not the struct: initialisers are excellent on a type callers construct by
hand and wrong on a hundred slots of scratch space, every one written before it is read. And measure
rather than reason — the third array in the same file was 2.3 KB with the same problem, and removing
it measured 1054.4 against 1053.1, inside the noise, because the compiler could already see through
it. A change that buys nothing still costs a reader.

## Theme SVG strokes with a class, never a literal colour

Three origin markers in Lesson 8.6's figures were drawn with `cline(..., "#e6ecf8", ...)` — a
near-white that is perfectly visible against the dark screenshots the same figures embed, and
invisible against the light theme's page background. `check-page.js` passed: nothing spilled, nothing
overlapped, and the element was genuinely there. Only a rendered screenshot in the light theme showed
that the reader could not see it.

`course.css` themes `.ink`, `.ink-soft` and `.grid` for exactly this. This is the same rule as
"CSS beats SVG presentation attributes" for `<text>`, one step further out: a stroke colour is a
theme decision, and a figure that hardcodes one has decided for both themes at once.

## A control can convict the code of the test's own mistake

Lesson 8.7's persistence section needed a control: a matcher that matches *everything* would look
perfect on a resting crate, so the harness had to show it refusing something. The control teleported
the crate two metres sideways between two frames and expected zero id matches. It got **four out of
four**, and the manifold was right.

A `contact_id` names *features* — the floor's top face, the crate's bottom face, corner 3 — and the
same corner really is on the same face two metres away. Ids cannot detect a teleport and should not
be asked to: a body that was *moved* rather than simulated is a discontinuity, and invalidating its
cached manifolds belongs to the cache. The control that works changes the feature instead — roll the
crate onto its side, 0 of 4.

The tell was that the "failure" was too clean. Four of four is not what a broken matcher looks like;
it is what a matcher looks like when it is answering a different question from the one asked. Before
believing a control's verdict, ask what the instrument is *entitled* to see.

## An instrument can be a tautology

The same lesson's first attempt at measuring what a parallelism tolerance costs checked that each
contact's two claimed surface points lie on their shapes, and reported **exactly zero error across
four decades of the tolerance**. The check was true by construction: one of those points is the
incident vertex and the other is its shadow on the reference plane, so the clipper cannot produce a
pair that fails it, whatever the tolerance is set to.

If a measurement cannot come out wrong, it is not a measurement. What the tolerance actually decides
is the contact *normal*, and against 8.4's exact minimum translation the worst error turns out to be
`acos(face_cos)` exactly — 44.8° at 0.5, 2.56° at the default — which is a genuine result and makes
the tolerance a knob rather than a magic number.

## `acos` has a resolution floor, and it is 0.036 degrees

`acos(dot(a, b))` is the obvious way to measure the angle between two unit vectors and it cannot
resolve a small one. For a small angle `t`, `dot` is `1 − t²/2`, so all the information about `t`
lives in the last bits of a number near 1; two unit `float` vectors give a dot product good to about
1e−7, which recovers `t` only to `sqrt(2e−7)` radians — **0.036°**. Lesson 8.7 reported a
bit-exact normal as "0.0198° off" for a whole draft, and the number was the instrument.

The chord is the angle to first order and loses nothing: `2·asin(|a − b|/2)` is exact at every angle,
and near zero it is a subtraction of two nearly equal vectors, which carries full relative precision
by Sterbenz's lemma. After the change the same measurement reads 0.0000.

## A knob written for a real hazard can measure zero

Every 2D physics engine biases the choice of reference face, and the reasoning is sound: two equally
parallel faces make the choice a coin toss, a bare `>` decides it on rounding, and a flip renames
every contact on the pair. Lesson 8.7 shipped that knob in draft and measured it at **zero flips
over 208,000 pairs** — freely rotated boxes, crates stacked on crates, and hexagonal prisms whose
face normals come out of Newell's method rather than out of a rotation matrix.

The reason is better than the knob. On a face contact both cosines are exactly `1.0f`, because each
face really *is* perpendicular to the contact normal, so the tie is **exact** — and an exact tie is
resolved deterministically by the comparison. A quantity that merely *ought* to be equal is a coin
toss; one that *is* equal is not. The hazard belongs to the quantity, not to the comparison.

The knob came out. And a measurement of zero needs a control more than any other kind, because "it
never happened" and "I was not measuring anything" produce the same number: the control forces a
flip the only way it can, by swapping the two arguments, and measures that 0 of 4 warm starts
survive one.

## A monotone cell map is all a range-walk grid needs

Every tutorial on spatial grids warns that the cell index must use `std::floor` and never a cast to
`int`, because a cast truncates toward zero, merging cells −1 and 0 into one double-width cell that
straddles the origin. The warning is repeated so often that Lesson 8.8's harness was written to
*demonstrate* the resulting lost pairs. It measured **zero**, on 3,000 proxies centred on the origin.

The reason is one line of order theory. A grid that inserts each proxy into every cell of its
**range** finds a pair when the two ranges intersect, and a cell map is a function from the reals to
the integers. If `max(a₀, b₀) ≤ min(a₁, b₁)` and `c` is non-decreasing, then
`c(max(a₀,b₀)) ≤ c(min(a₁,b₁))`, so the mapped ranges still meet. **Monotonicity is the only
property the range walk uses** — not equal cell widths, not contiguity, not a particular origin.
Truncation is monotone. So would a map that made every third cell twice as wide.

What truncation actually costs is occupancy: eight cells become one, a cell's pair loop is quadratic
in what is inside it, and the neighbourhood around the origin runs **4.0×** the comparisons. That is
a footnote, not a bug — and worth knowing precisely, because the mistake sitting next to it in the
same code *is* fatal and has no famous name: inserting each proxy into the single cell containing
its **centre** loses **66.7%** of the pairs there are, while looking nearly right, because the third
it finds are the ones that happen to sit well inside one cell and are therefore the ones you notice
while debugging it.

Use `floor` anyway — it is the same instruction count, and a cell map that behaves identically
everywhere is one less thing that is only true away from the origin. But do not spend a weekend
hunting the famous bug when the expensive one is beside it.

## An A/B comparison must differ in exactly one thing — including where the code lives

Lesson 8.8 §7 timed the engine's spatial hash (three multiplies, two exclusive-ors) against a
stronger mixing function (the same, plus a rotate and a finaliser) and measured the **more expensive
one as three times faster**. That is arithmetically impossible, and the impossibility is the useful
part: a result that contradicts the instruction count is the instrument talking, not the code.

One arm crossed a translation-unit boundary. `hash_cell` was defined in `broadphase.cpp`, so a
caller in another TU paid a real function call for it, while the comparison function was a static in
the harness and inlined away. Re-measured with a `noinline` wrapper so that the call is the only
difference: **0.115 ns inlined, 0.692 ns behind a call — 6.0× for three multiplies.** `cell_of` and
`hash_cell` moved into the header because of it: inside `build` it never mattered, since that is the
same translation unit, and these two are the engine's introspection hooks, so everybody *else* is
exactly who calls them.

Lesson 4.8 §3 paid for the same lesson in a different currency — a harness that built its own sRGB
render target and therefore tested a configuration the shipped program never ran. Before believing
the difference you were looking for, ask what *else* differs between the arms.

## When the clock is noisier than the difference, find a quantity that needs no clock

A uniform grid's cell size has a real optimum, and Lesson 8.8's first attempt to find it swept
thirteen sizes across three decades and timed each build. Around the minimum the measurements were
worthless: `(max − min) / median` over forty-one runs reached **1.18**, meaning the slowest run took
more than twice as long as the fastest, and the "fastest cell size" moved between 2.5 m and 4 m from
run to run. The work being timed is a single build of a few hundred microseconds, which is short
enough that the operating system's interruptions dominate it.

The fix was not more repetitions. It was to notice that `broadphase_stats::entries` and
`bucket_tests` are **deterministic** — they do not vary by one count between runs — and that the
cost is `a·entries + b·bucket_tests` for two constants that can be solved *exactly* from two rows of
the sweep, each chosen because one term dominates it. The constants came out at **8.09 and 5.45 ns**
on a sparse scene and **8.11 and 5.44** on one four times denser, because they are properties of the
machine rather than of the scene. Minimising the model then picks a cell size with no timer involved
at all, and it distinguishes sizes the clock could not.

Two corollaries worth keeping. **Report a minimum rather than a median** when timing sub-millisecond
work: it is the standard robust estimator for "how fast can this code go", and it is the
conservative direction whenever a smaller number makes your argument harder. And **instrument the
counts, not only the time** — the counters that made this possible exist because a grid is the
system in this engine where intuition is least reliable, and the difference between a good cell size
and a bad one is completely invisible in the output.

## A cached quantity must carry the frame it was measured in

`contact_point::tangent_impulse` is two floats, named for what they are ("the accumulated
tangential impulses in the batch's tangent basis"), and 8.7 shipped them, 8.9 wrote to them and
8.10 was the first lesson to read them back a frame later. They are **coordinates**, and the basis
was never stored anywhere.

`tangent_basis` rebuilds that basis from the normal every frame by seeding its cross product from
whichever axis the normal is *least* aligned with — which on a near-vertical contact is a
comparison between `|n.x|` and `|n.z|`, two numbers that are both about 10⁻⁵ on a settled crate
and wander around each other. When they cross, the basis rotates by ninety degrees and last
frame's friction is applied sideways. Measured over twenty seconds of a ten-crate tower:
**113 of 11,830 manifold-frames rotate by more than 30°, worst case 134.6**, and storing the basis
takes the tower's sideways drift from 380 mm to 155.

Three things make this a class rather than an incident.

**Nothing in the type system was ever going to catch it.** Both the broken version and the fixed
one are two `float`s called `tangent_impulse`. They compile, they look right in a debugger, and
the units are correct — newton-seconds either way.

**The symptom names no subsystem.** A tower that leans is a friction bug, a solver bug, a manifold
bug, or six lessons of geometry. Nothing about it points at a cache.

**What found it was a probe, not a test.** Having decided the drift was suspicious, the harness
computed `tangent_basis` *independently* each frame and counted how often it moved. A test asserts
something you already suspect; a probe measures something you do not yet have a hypothesis about,
and it is the cheaper instrument when the symptom is vague.

The general rule: whenever you cache a number that lives in a frame — a basis, a local space, a
parent transform, a texture's UV convention — cache the frame with it, or convert to a
frame-independent representation before storing. The fix here was two dot products and it is
exact.

## Keep enough counters that they can contradict one another

Lesson 8.10's union–find encoded a root's island label as `−label − 1` so it could not be mistaken
for a parent index — and `−label − 1` sends label 0 to `−1`, which was already the array's
sentinel for "this body is in no island". The first island in every scene silently ceased to
exist. Its contacts were never grouped and never solved, and a tower whose bottom crate happened
to land in it fell through the floor while every other tower in the same scene stood.

A crate falling through a floor has a hundred plausible causes and this engine has five lessons of
collision geometry among them. What actually found the bug in minutes was a single line of debug
output in which **`stats.points` read 0 on a frame where `deepest()` read 57 mm**. Neither
instrument was wrong about what it measured, and they could not both be measuring the same
manifold — so the fault was between them, in the grouping, and there was nowhere else to look.

Two instruments that can disagree are worth more than either one twice as precise. Budget for
counters that are *redundant on a healthy run*: `manifolds` against `solved_manifolds`, `points`
against `warm_points`, an island's `body_count` against the number of dynamic bodies. Every one of
those is a tautology when the code is right, which is exactly why a violation localises a fault
instead of merely reporting one.

## A timing sweep over a parameter that changes the simulation must restore the simulation

Lesson 8.10 §13 swept the solver's iteration count to read off a cost model, and its first version
reported **fewer iterations as faster** — 14.96 µs at zero against 118 at eight. The zero-iteration
rows had dropped the whole scene through the floor, so what they timed was a frame with no
contacts left in it.

The fix is to restore a snapshot before every timed row, which is obvious once stated. The part
that is not obvious is **what belongs in the snapshot**. The first version restored only the
bodies, and a warm-starting sweep then came out non-monotone because each trial inherited the
impulses the *previous* trial had written — so a sweep over the iteration count was also an
uncontrolled sweep over the quality of the initial guess. Warm starting is precisely the feature
that makes a physics step depend on more than its bodies, and a fixture for measuring it had
better know that.

The general form: before timing a parameter sweep, ask what state the system carries between
steps that the parameter can influence. Caches, accumulators, adaptive structures and RNG streams
all qualify, and all of them look like implementation details right up until they are the
measurement.

## A quantity read at the end of a step may belong to its start

Lesson 8.11 §5 checked a spinning rod's kinetic energy against `(L/r)²` — angular momentum is
conserved, so the speed should fall exactly as the radius grows — and missed by **0.4%**, which is
enormous for a check that should agree to the rounding. The velocity a step *ends* with was solved
by the constraint at the radius the step *started* with, one drift earlier; the position update
then moved the bob. Pairing the end-of-step velocity with the end-of-step radius mixed two instants,
and the error was exactly one step's worth of drift. Paired with the start-of-step radius it agreed
to **1 × 10⁻⁶**.

The same session produced a second instance of the same mistake in a different costume: a
pendulum's period at a 2° amplitude compared against the **small-swing limit**, which is a
different instant of a different sort — the period of a swing that does not exist. The "error"
refused to shrink with the step (+6.6 × 10⁻⁵ at 240 Hz), and against the period at 2° it fell by
exactly four per halving of `h`.

Two rules come out of it. In a stepped simulation, write down *when* each quantity you compare was
computed, not just what it is — velocities after the solve, positions after the move, and an
energy that adds them is an energy of no instant at all. And **an error that will not shrink with
`h` is a reference that is wrong**, far more often than it is a simulation that is.

## Sample the phase, not the parameter

Lesson 8.11 §10 measured a hinge limit's overshoot — which should be uniform on `[0, ω·h]`, like
8.10's arrival depth — by sweeping the arrival speed finely from 2.0 to 2.2 rad/s, and read a mean
of **0.42** instead of 0.50. The speeds were uniform; the *phase* at which the arm crossed its stop
within a step was not, because that sweep moved the phase through 1.36 cycles rather than a whole
number of them. Holding the speed and stepping the starting angle back through exactly one step's
travel gave 200 evenly spaced phases and a mean of **0.4985**.

When a measured quantity depends on where an event falls inside a discrete step, the variable to
sweep uniformly is that position, not whatever physical parameter happens to move it.

## A count can be the wrong instrument where a consequence is the right one

Lesson 8.11 §3's control for a sign-flipped Jacobian first counted how many flipped contact rows
applied an impulse, expecting zero on a resting yard. It read 111 of 200, then 90 of 200, and could
not be made to mean anything: in a stack, gravity gives *both* crates of a crate-on-crate contact
the same `g·h`, so only the floor contacts are clearly approaching and the rest sit either side of
zero on noise. Running the flipped rows as the yard's only contact solver for half a second said
it in one number — **784 mm** through the floor, against 27 for the rows as derived.

If a control's count depends on a detail you did not intend to test, measure what the defect
*does* instead of how often it fires.

## SVG collapses whitespace, so columns need coordinates

Lesson 8.11's figure 2 wrote each row of a cost table as one monospace string padded with spaces
to line up the columns. SVG collapses runs of whitespace in `<text>` exactly as HTML does, and the
columns came out ragged. `check-page.js` cannot see it, because nothing overlaps and nothing
spills. Emit one `<text>` per column at a computed `x`, with `text-anchor="end"` for numbers.

## A demo can hide the very thing it exists to show

Lesson 8.11's joints demo drove two arms into their hinge limits with motors, to show a door
bouncing off its stop at 7.5% of its arrival speed. The motor, still pushing into the stop,
swallowed a 7.5% rebound within a few frames, and the scene showed two arms sitting still. Kicking
the arms and letting them *coast* into the stop made the rebound a visible drift of a fraction of a
radian. Before a demo scene ships, run it headless at the moment the effect should be visible and
look at the frame.

## A satisfied one-sided row must keep its speculative target in the position pass

Lesson 8.11's `solve_joint_positions` solved every non-motor row against `row.bias`. For a one-sided
row that is *not* violated — a hinge stop the joint is nowhere near — the bias is zero, and solving
against zero turns the row into a hard "no pseudo-velocity toward me" constraint. With speculation
on, a two-ended limit always has both rows, so the far stop cancelled every correction of the near
one exactly: a loaded knee sat frozen at −0.1691° for ten seconds (to four decimals), and with warm
starting off the stop gave way to −70.7°. Baumgarte never had it, which is why "split impulse is
too weak for a loaded stop" would have been exactly the wrong diagnosis.

A one-sided row in any pass of the solver needs the same speculative rule in both: `−C/h` when
`C > 0`. And a violation that does not change in the fourth decimal over seconds is not a slow
correction; it is no correction.

## A kinematic body is not a bridge, so it must wake things explicitly

8.10's waking rule — an island is asleep only if every body in it is, and a moving body joins the
island it hits — works for dynamic bodies only, because `build_islands` unions an edge only when
both ends are dynamic. A moving kinematic body (a lift, a platform, a steered ragdoll) therefore
never woke anything, and passed straight through a crate that had fallen asleep before it arrived:
48 frames of contact, crate unmoved; with sleeping off, 4.78 m/s. Wake the dynamic side of every
contact or joint with a kinematic body moving faster than the sleep thresholds — and gate it on the
thresholds, or nothing on a stopped platform can ever sleep.

## Drive kinematic bodies by velocity, and invert the integrator you actually have

Teleporting a body onto an animated pose leaves its velocity as a lie (20.6 m/s of accumulated
gravity in 8.12 §12), which every contact then sees and every handoff inherits. Steer instead:
set the velocity that the next position update will integrate exactly onto the target. For
position that is the chord `(x_target − x)/h`; for orientation it depends on the spin rule —
the logarithm for an exact exponential step, and for the engine's linearised
`normalise(q + ½hωq)` the Rodrigues vector `(2/h)·Δq.v/Δq.w`. The logarithm under a linearised
integrator misses by `θ³/12` per step (9.75e-05 against 9.76e-05 predicted).

## A twist limit's row is the half-way axis over cos(φ/2), not the bone

With a swing–twist split `r = swing·twist`, the relative spin is the swing's own angular velocity
plus the twist rate along the bone, and the swing's angular velocity (`2h × ḣ`, from the swing as
two mirrors) is perpendicular to `a₁ + b₁`. So the twist rate is `Ω·(a₁ + b₁)/(1 + a₁·b₁)`. A row
about the bone is wrong by up to `|Ω⊥|·tan(φ/2)`, which integrated round a loop is the loop's solid
angle (Codman's paradox). In a solver it does not walk through the stop — the position pass reads
the true angle — but it lags past it by the drift over β for as long as the limb circles.

## Measure where an effect persists, not its deepest instant

8.12 §6 compared two self-collision rules by the deepest overlap reached in twelve falls, and both
read a tenth of a metre, because without speculative contacts a fast limb arrives up to `v·h` deep
in one step whatever the rule. The rules differed only in whether the overlap *stayed*: frames
spent more than 2 cm deep, and the depth at rest. A transient maximum is often the arrival artefact
of the step, not the behaviour under test.

## Instruments that measured their own floor this time

Four in one lesson. `acos` of a float dot product near 1 bottoms out near 7e-4 rad (8.7's floor)
and hid a 1.6e-7 landing error — use `atan2(|q.v|, |q.w|)` of the difference rotation. nlerp and
slerp were compared at the blend's midpoint, where they agree by symmetry (the lag peaks near the
quarter-points). An undamped hanging body was sampled mid-swing, so the "joint gap" measured the
swing's phase. And total kinetic energy, dominated by a walk's travel, swamped what the joints did
until it was taken relative to the centre of mass.

## zsh does not word-split `set -- $spec`

A loop of `for spec in "1 0.6 run" ...; do set -- $spec; demo --scene $1 --t $2 --shot $3; done`
passed each spec as ONE argument under zsh, `--t` swallowed `--shot`, and the demo opened a real
window and never quit. Write shot commands out longhand (or use `${=spec}`).

## A conservative step must be taken from a lower bound

Lesson 8.13's cast is Newton's method on a convex distance, and the argument that it cannot
overshoot — a convex function lies above its tangent — is true of the *exact* distance. The first
draft stepped from `gjk_result::distance`, which is the gap between two witness points and so an
**upper** bound, and 377 of 6,049 random hits finished inside the skin, by up to 0.17 mm: the step
overshot by exactly GJK's slack. Stepping from `certify(...).lower`, the slab between the two
supporting planes, finished inside none. The slab argument is also the stronger proof: under a
translation the slab narrows at exactly `d·n` for *any* `n`, so the step is safe even when GJK's
direction is slightly wrong. When a proof depends on a quantity being exact, check which side of
exact the code's number errs on.

## Clip the original motion, not what was left

Quake's plane-list rule (`clip_to_planes`) is well known; what it is *fed* is the half people get
wrong. 8.13's first draft clipped the remainder after the last hit against every plane, and in a
120° corner the remainder — already bent by the first wall — projected onto the second wall alone
points back out of the corner, passes the "goes into none of the others" check, and the character
walked back and forth at 575 mm/s. Quake 1's `SV_FlyMove` clips the ORIGINAL velocity against every
plane; both single-plane candidates then fail, the crease is vertical, and the character stops.
Keep the rejected rule as a knob and measure it, or nobody will believe the difference matters.

## A skin thinner than the query's own margin is worse than none

With no skin a cast lands exactly on the contact — against a flat face, in one Newton step — and
GJK reports that as intersecting, so the cast must fall back to the last iterate it could measure:
134 mm short on average, 2.3 m at worst. A 0.1 mm skin finishes reliably and finishes *inside*
GJK's contact margin, so 7.3% of the next queries start inside (against 2.1% with no skin). A
millimetre or more, and none do. A tolerance-sized clearance has to clear the *other* algorithm's
tolerance, not just be positive.

## A certificate's looseness can scale with the object, not the tolerance

GJK's certified lower bound is the slab width along its direction; when the direction is off by a
small angle, the far face of the slab is set by the obstacle's farthest corner, and the bound is
short by roughly the obstacle's size times the angle. 8.13 §6 measured a character standing
10.00–10.07 mm off a 6 m ramp and 10.02–12.32 mm off a 60 m one, identically at GJK tolerances of
1e-4 and 1e-7 — GJK stops on float rounding before its tolerance matters. The fix is authoring
(sensible collision pieces), and the lesson is the 8.4/8.5 one again: find which *scale* is spoiling
the answer before tuning a threshold.

## A velocity-gated rule cannot see a teleported body

8.12 fixed islands so that a *moving* kinematic body wakes what it touches, gated on the sleep
thresholds. 8.13 teleported a kinematic proxy — position written, velocity left at zero — and the
character walked straight through a crate that had fallen asleep, because the wake rule reads the
velocity and the velocity said "not moving". The same lie makes the position pass the only thing
that moves an awake crate, so the proxy sinks `v·h/β + slop − v·h` (138 mm) into it. Steer
kinematic bodies by the chord, always; every rule downstream reads velocities.

## "Helpful" code can create a second owner

8.13's first `recover` pushed the character out of any overlapping body it could not push — heavy
dynamic ones included — and a 200 kg boulder carried the character 2.94 m. It looked like a
feature (heavy things shove you) until it was named: the solver was moving a position the
controller owns. `recover` now answers only to fixed and kinematic bodies, and knockback is an
explicit read of the proxy's contact impulses. When two systems both move one quantity, one of them
is wrong even if the result looks plausible.

## A measurement window can read an approach as jitter

§C measured a character pressed into a corner by the travel in "the last second" of three, and one
rule read 85 mm/s of "jitter" — which a frame-by-frame probe showed was the tail of a slow slide
into the corner, settled at frame 128 of 180. Measure a steady state after it has settled (the
window moved to 4–5 s), and look at the trace once before trusting a summary of it.

## A quaternion's angle wraps at a full turn

`2·atan2(q.v.y, q.w)` is ±360° for `q = −1`, which is where a quaternion lands after one full turn
(7.4's double cover). §I's facing check failed at exactly one revolution until the comparison used
`remainder(angle − expected, 2π)`. Compare angles modulo a turn, never by subtraction.

## A zero-byte generated file is a silent regression

`scratch/l511_fig7.svg` (gitignored) was truncated to zero bytes at the end of the 8.12 session —
most likely a figure script run in the working tree that opened its output and then failed — and
the published page stayed correct, so nothing looked wrong until `check-builders.py` reported 511
as DIFF. The fix came from the page itself, which archives its figures; the lesson is to run the
builder check at the START of a session too, and to treat a "known failure" note as a claim to
re-verify, because the note had attributed this to `--figures` only.
