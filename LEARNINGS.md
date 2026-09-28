# Learnings

Hard-won facts about this codebase and its dependencies. Read this before writing lessons or
code. Every entry here exists because getting it wrong costs real debugging time — or, worse,
ships a confidently wrong statement to a student who will type it in.

**This file is the index; the entries live in [`learnings/`](learnings/).** It was split
on 2026-09-27, when it had reached 569 KB — far past what "read it before writing" can
mean. Every section moved there verbatim; nothing was rewritten. Read the hazards below
before any lesson, then the full entries for whatever the lesson touches (search the
archive for the heading).

**Adding a learning:** append the section to `learnings/module-N.md` for the module being
written (or `learnings/tooling.md` for the docs pipeline), and add its heading to the
list at the bottom of this file. If it is a hazard that has now bitten twice, add a line
to the list just below.

---

## The hazards that recur — read these first

**SDL and the GPU**
- **Every SDL_GPU convention is verified, never assumed** — winding is per-pipeline, not
  SDL-wide, and the NDC-parity decision is the highest-leverage choice in the course.
  *foundations: "SDL_GPU conventions — verified, not assumed", "The NDC-parity decision".*
- **Internet snippets are SDL2** — `SDL_CreateWindow` has no x/y, `SDL_Init` returns `bool`,
  `keysym` is gone. Check the header at the pinned tag. *foundations: "SDL3 API signatures…".*
- **SDL's asserts key off `__OPTIMIZE__`, not `NDEBUG`** — `-O2` alone silences `SDL_assert`;
  gate your own macros on `SDL_ASSERT_LEVEL`. *tooling: "Course-infrastructure facts (docs/, 2026-08-26)".*
- **shadercross has no releases; pin a commit** — and a build without DXC cannot read HLSL.
  *foundations: "SDL_shadercross has no releases — pin a commit".*

**Numerics**
- **NaN passes every guard you did not write for it** — `std::clamp` keeps it, it never wins a
  maximum, and "finite" is not "right". *module-3: "`std::clamp` cannot remove a NaN"; module-7:
  "A NaN never wins a maximum" and the entry on "finite".*
- **Cancellation against 1.0** — `sqrt(1 - sin*sin)` and its relatives are exact on paper and
  lose every digit that mattered; `acos` of a unit-vector dot product cannot resolve an angle
  under 0.036° in float — use the chord. *module-7: "Cancellation against 1.0…"; module-8:
  "`acos` has a resolution floor, and it is 0.036 degrees".*
- **Symmetric or hand-picked test data agrees with the code by construction** — convention bugs
  hide behind it. *module-8: "Convention bugs hide behind symmetric test data",
  "Hand-picked test data agrees with the code by construction".*

**Measurement**
- **Timing loops measure the compiler** unless the whole result is consumed and the input
  varies — otherwise it deletes or hoists the work; an accumulator that is overwritten measures
  nothing; alternate the arms or you time the machine.
  *module-7: "A timing loop measures the compiler unless you stop it twice"; module-8:
  "Alternate the arms, or you are timing the machine".*
- **A control must fail when the thing under test is broken** — one that fires on the healthy
  case, or whose degenerate case is a pass, is not a control. *module-6, module-7, module-8.*
- **Write the prediction down first; the measurement may refuse it** — sections written to
  confirm a claim have disproved it again and again, most of all in Module 8. *module-3: "Predict,
  then measure — and write the prediction down where it can be wrong", "The measurement is allowed
  to prove *you* wrong".*
- **A harness cannot check a claim about somebody else's engine** — cite the header or source
  line, never "every engine does X". *tooling.*

**Bookkeeping and the pages**
- **One fact in N places will be wrong in one** — and the check you add watches the wrong list
  until it is anchored. *module-7: "The same fact in three places…"; module-8: "…and the fourth
  place is the one nobody can see"; module-7: "A check that greps its own corpus…".*
- **A page can look complete and still not reproduce the engine** — a tagged excerpt reads as
  the whole file; `check-continuity.py` exists because of it. *tooling: "A page can look complete…",
  "A tagged excerpt is read as the whole file".*
- **Generators rot silently** — a crashing builder writes nothing, and "the file did not change"
  is not evidence; a zero-byte generated file is a silent regression. *tooling: "The builder
  reproducibility repair"; module-8: "A zero-byte generated file is a silent regression".*

**Figures and the browser**
- **CSS beats SVG presentation attributes** — `svg text { fill }` overrides `fill="…"`; theme
  text and strokes with classes. *foundations: "Verifying a lesson page"; module-8: "Theme SVG
  strokes with a class, never a literal colour".*
- **`getBBox()` is local space** — use `getBoundingClientRect()` for spill and collision checks.
  *foundations: "Verifying a lesson page".*
- **Figure numbers and filenames follow page order**, and a moved label lands on the next
  obstacle. *module-6, module-7.*
- **Check at 390 px, not only 1280** — a figure defect can be invisible at the width it was
  authored; `scroll-behavior: smooth` makes fixed-delay anchor tests lie. *module-7, module-8,
  tooling.*

**Shell and git**
- **zsh does not word-split an unquoted `$var`** — `set -- $spec` gets one argument. *module-8.*
- **`GIT_INDEX_FILE` leaks into every child `git`, and `git reset` empties a staged index** —
  set it per command, never export it; `git write-tree` before anything risky. *tooling.*
- **A move script is code** — `sed -i ''` only means "no backup" to BSD sed; a published script
  gets a test that runs it with the machine's own tools. *tooling.*

---

## Every entry, by archive

Headings only, in their original order. Each archive keeps its sections' `###`
sub-headings, which are not repeated here.

### [`learnings/foundations.md`](learnings/foundations.md) — SDL3, SDL_GPU, the conventions, and verifying a page (8)

- SDL_GPU conventions — verified, not assumed
- The NDC-parity decision (highest-leverage choice in the course)
- World space is right-handed; clip space is left-handed
- SDL_shadercross has no releases — pin a commit (corrected in Lesson 4.3)
- SDL3 API signatures that differ from SDL2 muscle memory
- Authoring conventions worth remembering
- Verifying a lesson page — what actually catches things
- Testing engine code that talks to SDL

### [`learnings/module-2.md`](learnings/module-2.md) — learnings from its lessons (25)

- Sub-pixel errors have a damage profile you cannot sample (Lesson 2.4)
- Do not oversell a real principle with a fake symptom (Lesson 2.4)
- Name a type after what it is, not what it is shaped like (Lesson 2.4)
- What correct colour actually costs in a software rasterizer (Lesson 2.4)
- A diagram can pass every automated check and still argue the wrong thing (Lesson 2.5)
- Let the type carry the convention (Lesson 2.5)
- Verify a continuous claim with a discrete measurement — and know the bias (Lesson 2.5)
- A zero-argument function cannot be overloaded (Lesson 2.6)
- Transcribe formulas in the notation they were derived in (Lesson 2.6)
- Two lessons running, the same class of diagram defect
- When machinery looks incomplete, check whether it is missing an *input* (Lesson 2.7)
- A magic literal at a call site is a bug waiting for a hurried reader (Lesson 2.7)
- Show a difference against a fixed reference, or you have shown nothing (Lesson 2.7)
- The nastiest transform bugs are invisible in the degenerate cases (Lesson 2.8)
- Prove "rigid" with a measurement, not a picture (Lesson 2.8)
- Name for the code you are going to write, not the code you have (Lesson 2.8)
- Introduce math where it is first needed, not where a plan filed it (Lesson 2.9)
- A projection can make a rigid thing look sheared — verify frames numerically (Lesson 2.9)
- A matrix can't divide, so perspective defers the divide — that's why w exists (Lesson 2.10)
- Perspective depth is 1/z-nonlinear, and the near plane is the master precision knob (Lesson 2.10)
- The clean way to A/B two rendering modes is one matrix, one path (Lesson 2.10)
- A convention that lives in twelve places will eventually be wrong in one (Lesson 2.11)
- Design your types to shadow the API you will port to (Lesson 2.11)
- Hand-authored geometry is DATA — validate it, don't eyeball it (Lesson 2.12)
- The saving from indexed geometry is WORK, not bytes (Lesson 2.12)

### [`learnings/module-3.md`](learnings/module-3.md) — learnings from its lessons (66)

- "Show the failure" only works if the failure is BIG ENOUGH TO SEE (Lesson 3.1)
- A verification harness will tell you your explanation is wrong, if you let it (Lesson 3.1)
- Interpolate the quantity the projection already fixed (Lesson 3.1)
- Precision is a formula, not a vibe (Lesson 3.1)
- Widget SVGs are not `figure.dia` SVGs, and an unstyled `<text>` is black (Lesson 3.1)
- Interpolate the quantity that is affine in the space you are walking (Lesson 3.2)
- An error that is zero at the corners is a chord under a curve (Lesson 3.2)
- Ask what your test scene structurally cannot show (Lesson 3.2)
- A saturating metric hides the thing it is measuring (Lesson 3.2)
- A parameter list that keeps growing is asking to be a state object (Lesson 3.2)
- The fixed-size scratch array that was fine until it wasn't (Lesson 3.2)
- A destructive step has a precondition, not a guard (Lesson 3.3)
- Two states that must not be confused should be two types (Lesson 3.3)
- Prove the bound instead of clamping to it (Lesson 3.3)
- The measurement is allowed to prove *you* wrong (Lesson 3.3)
- Do not use a broken baseline to test the fix for the breakage (Lesson 3.3)
- `std::clamp` cannot remove a NaN (Lesson 3.3)
- One constant, two undefined behaviours (Lesson 3.3)
- Read the sign before you destroy it (Lesson 3.4)
- A function signature can make a bug unwritable (Lesson 3.4)
- The wrong test is often the right test for a camera you are not using (Lesson 3.4)
- Folklore deserves a measurement (Lesson 3.4)
- An optimisation that quietly fixes a bug is worth understanding, not glossing (Lesson 3.4)
- Choose a normaliser that reflects where the error comes from (Lesson 3.4)
- The extension your build system reserves may be someone else's data format (Lesson 3.5)
- A file's idea of a vertex and the hardware's idea of a vertex are different ideas (Lesson 3.5)
- Number parsing is where a loader most easily starts lying (Lesson 3.5)
- Topology is a property of the surface, not of the array that encodes it (Lesson 3.5)
- "Same point in principle" is not "same number", and welding needs the second (Lesson 3.5)
- Euler's formula is a statement about spheres (Lesson 3.5)
- The general test costs the same as the test that only works sometimes (Lesson 3.5)
- Distinguish "malformed" from "silly", and report both (Lesson 3.5)
- A check-page pass is a floor, not a ceiling — two figures said false things (Lesson 3.5)
- A normal is not a direction — it is a relationship (Lesson 3.6)
- The bug that only appears on objects nobody is looking at (Lesson 3.6)
- Fake shading gives itself away by what it does NOT do (Lesson 3.6)
- Adding a light required no rasterizer changes, and that was informative (Lesson 3.6)
- Verify a figure's claim numerically, not just its layout (Lesson 3.6)
- Folklore survives because nobody arranges the case that breaks it (Lesson 3.7)
- A figure can pass every automated check and still hide the claim (Lesson 3.7)
- A fast path can be bought out by a feature, and it is worth naming when it happens (Lesson 3.7)
- Passing tests do not mean the test measured the right thing (Lesson 3.7)
- A knob that will not extend is usually two knobs (Lesson 3.8)
- Predict, then measure — and write the prediction down where it can be wrong (Lesson 3.8)
- The folklore about shading cost has a precondition nobody states (Lesson 3.8)
- Fixing the wrong axis fixes nothing, however hard you push (Lesson 3.8)
- A figure can be geometrically perfect and still refute its own caption (Lesson 3.8)
- A test failing is not evidence the code is wrong (Lesson 3.9)
- The half-texel question exists at both ends of a pipeline (Lesson 3.9)
- Nearest-neighbour sampling cannot see a half-texel error (Lesson 3.9)
- Hold the confound constant, or your benchmark measures the wrong thing (Lesson 3.9)
- Aliasing is not caused by a large footprint (Lesson 3.9)
- Duplicate SVG marker ids are silent, and stop being harmless later (Lesson 3.9)
- `.t-inv` needs an opposite, and the choice is a computation (Lesson 3.9)
- The clock is coarser than it is expensive, and that inverts the rule (Lesson 3.10)
- Instrumentation makes things slower *and* under-reports, at the same time (Lesson 3.10)
- A differential benchmark without a control is a story (Lesson 3.10)
- Median for a frame, minimum for a kernel — never the mean (Lesson 3.10)
- The unmeasured remainder is the most important row in a budget (Lesson 3.10)
- Triangle count is close to irrelevant; pixels per triangle is the axis (Lesson 3.10)
- Cost does not follow attention (Lesson 3.10)
- Judge an approximation before rounding, not after (Lesson 3.10)
- A hypothesis that fails should ship with its result, not be deleted (Lesson 3.10)
- The cache cliff is not where the folklore puts it (Lesson 3.10)
- Amdahl's law is an arithmetic check on your own work, not a slogan (Lesson 3.10)
- An engine-wide default is a decision about the repository, not about renderers (Lesson 3.10)

### [`learnings/module-4.md`](learnings/module-4.md) — learnings from its lessons (18)

- I fell into 3.10's own pitfall within a day (Lesson 4.1)
- Helper lanes are not waste, they are what derivatives cost (Lesson 4.1)
- Divergence costs what your DATA decides, not what your code says (Lesson 4.1)
- The reason state lives in a pipeline object is not the one everybody gives (Lesson 4.1)
- SDL_GPU facts verified at release-3.4.12 (Lesson 4.2)
- The cost of a GPU sync is the overlap you gave up, not the wait (Lesson 4.2)
- A bandwidth figure above the bus is a broken benchmark, twice over (Lesson 4.2)
- A vsync measurement is only comparable on the same display (Lesson 4.2)
- Shader facts verified at SDL 3.4.12 + shadercross 3.0.0 (Lesson 4.3)
- Two CMake facts that cost time (Lesson 4.3)
- Pipeline facts, verified at SDL 3.4.12 (Lesson 4.4)
- Our rasterizer and the hardware compute the same triangle (Lesson 4.4)
- Reversing two vertices deletes a triangle (Lesson 4.4)
- Vertex-layout facts, verified at SDL 3.4.12 (Lesson 4.5)
- Uniform-data facts, verified at SDL 3.4.12 (Lesson 4.6)
- Depth and texture facts, verified at SDL 3.4.12 (Lesson 4.7)
- Scene-porting facts, verified at SDL 3.4.12 (Lesson 4.8)
- Frame-debugging facts, verified at SDL 3.4.12 (Lesson 4.9)

### [`learnings/module-5.md`](learnings/module-5.md) — learnings from its lessons (23)

- Refactoring facts, learned the hard way (Lesson 5.1)
- SDL3 main-callback facts, verified at SDL 3.4.12 (Lesson 5.2)
- Course-infrastructure facts (docs/, Lesson 5.2)
- SDL3 logging and assertion facts, verified at SDL 3.4.12 (Lesson 5.3)
- Course-infrastructure facts (docs/, Lesson 5.3)
- C++ and container facts (Lesson 5.4)
- Asset-system facts (Lesson 5.5)
- Measurement facts (Lesson 5.6)
- ECS storage and measurement facts (Lesson 5.7)
- Course-infrastructure facts (docs/, Lesson 5.7)
- ECS runtime facts (Lesson 5.8)
- Course-infrastructure facts (docs/, Lesson 5.8)
- Transform hierarchy facts (Lesson 5.9)
- Measurement and tooling facts (Lesson 5.9)
- Input mapping facts (Lesson 5.10)
- Debug drawing and tooling UI facts (Lesson 5.11)
- Course-infrastructure facts (docs/, Lesson 5.11)
- A document nothing compiles is a document nothing can keep correct
- An incomplete dependency and a cheap one look identical on a stopwatch
- An assertion is developer-facing control flow, and `--shot` has no developer
- "Scale" means two things in one header, and it will bite three times
- The inverse of a product reverses, and the wrong order is not visibly wrong
- Writing a lesson out of order needs two trees, and the delta is the finding

### [`learnings/module-6.md`](learnings/module-6.md) — learnings from its lessons (32)

- Colour pipeline facts (Lesson 6.1)
- Course-infrastructure facts (docs/, Lesson 6.1)
- Radiometry and BRDF facts (Lesson 6.2)
- Course-infrastructure facts (docs/, Lesson 6.2)
- Microfacet facts (Lesson 6.3)
- Course-infrastructure facts (docs/, Lesson 6.3)
- Lesson 6.4 — Cook–Torrance, assembled
- Lesson 6.5 — A material system
- Lesson 6.6 — glTF 2.0 loading
- Lesson 6.7 — Normal mapping and the TBN derivation
- Lesson 6.9 — Cascaded shadow maps
- Lesson 6.10 — Mipmaps, LOD, and anisotropic filtering
- Lesson 6.11 — transparency
- Lesson 6.12 — HDR and tonemapping
- From Lesson 6.13 (bloom and the post-processing stack)
- From Lesson 6.14 (antialiasing)
- Lesson 6.15 — environment lighting (2026-09-12)
- A sweep along an axis the effect cannot depend on looks exactly like a measurement
- Verify against facts, and then check the facts are sufficient
- Rule out the obvious cause before believing it
- A limit that has never been reached cannot tell you it is wrong
- Two C++ traps this codebase had avoided by luck
- A test that can pass without testing anything is worse than no test
- A measurement that only works once is worse than one that never works
- Before building the thing, check that the reason applies here
- Does the denominator move when the code does?
- A check whose degenerate case is a pass is not a check
- Two bugs in two files can be the same function
- Make a bug impossible rather than catchable
- Return the quantity a caller would need to judge you
- A tool that relocates a claim has not verified it
- Figure numbers follow page order, and a moved label lands on the next obstacle

### [`learnings/module-7.md`](learnings/module-7.md) — learnings from its lessons (46)

- Cancellation against 1.0: the same bug three times in one lesson
- A one-knob Euler interpolation is a geodesic, so the obvious test measures nothing
- `grep` a name, get somebody else's Euler
- A headless run can write the right file and then crash
- A figure defect can be invisible at the width you authored it
- A path in a doc comment is indistinguishable from a path in an `#include`
- Build the golden into `build/demos/`, not `build/`
- A timing loop measures the compiler unless you stop it twice
- Not every catastrophic cancellation is a bug — measure what reaches the output
- Print the ratio, not the verdict
- The figure sampler keeps the brightest pixel, so a thin feature over a bright surface vanishes
- Figure filenames follow page order, and nothing checks it
- A wide equation is scrollable, not clipped — so nothing reports it
- A preview page that does not load the stylesheet lies about everything
- Never put a side effect in a C macro's argument
- Choose the quantity your check looks at before you choose the threshold
- A serial dependency chain can eat an entire optimisation
- The figure palette must contain the demo's own colours
- Nothing checks a documentation page's own table of contents
- The page quotes the program, so narrow the program
- A failure mode does not carry up a dimension — it is a hypothesis about the new one
- `acos` of a near-unit dot product is a blind instrument
- A timed loop that overwrites its accumulator measures nothing
- A narrower type finds existing bugs, not only future ones
- A render figure on the page's own panel loses the program's colours
- A dial drawn in a fixed world plane is an ellipse
- A numerical routine has a scale, and its name does not say so
- A NaN never wins a maximum
- "Finite" is not "right"
- `T · R · S` per node is a restriction, not a representation
- A figure's number is the page's, not the file's
- A flag reaches the translation units it is on, and no further
- Ask what a control would do if the thing under test were completely broken
- An instrument that needs an axis can be fooled by an orientation
- A prediction that survives two different deformations is about the mechanism
- Check a chain against code that shares nothing with it
- The figure quantiser's background test is per-channel
- A benchmark with two variables in it has none — and fixing one confound proves nothing
- `std::fmod` is not constant time
- Fit a lossy transformation with its consumer's exact reconstruction
- An instrument that cannot tell a defect from the feature reports every correct case as broken
- Say which degrees. A doc comment is not exempt
- A control that fires on the healthy case is not a control
- Real-time code you cannot call from a test is code you cannot debug
- A check that greps its own corpus can be broken by writing about it
- The same fact in three places, and one of them will be missed

### [`learnings/module-8.md`](learnings/module-8.md) — learnings from its lessons (62)

- …and the fourth place is the one nobody can see
- Order of accuracy is the `h → 0` question; a game asks the `t → ∞` one
- A search with a fixed budget returns an answer even when the answer is "none"
- A microbenchmark can measure the optimiser's mood
- check-page.js walks a path's stroke, so a text-on-shape hit is real
- A figure can be broken by the renderer rather than by the data
- `figs_NN.py` can end up mixing `\uXXXX` escapes with literal Unicode
- The second copy of a number is the one a reader actually sees
- Hand-picked test data agrees with the code by construction
- A control can have a bug and not fail — it produces a third number
- Alternate the arms, or you are timing the machine
- Print the discrete prediction, not only the continuous one
- Coincident curves look like one curve, and the proof looks like the bug
- `check-page.js` at 390 catches what 1280 cannot
- A derivation says *what* to compute, not *how* — third time
- Convention bugs hide behind symmetric test data
- Validation catches the impossible, not the merely wrong
- Measure in the frame the equations are written in
- A benchmark that copies state measures the copy
- The largest optimisation is usually not an algorithm
- The failure mode lives where the algorithm converges
- An invariant you can check is worth more than one you believe
- A proof outranks a decision
- A reference that rounds the way one arm rounds is not a reference
- A threshold's units decide which constant it may share
- A comparison whose two sides are mathematically equal has no correct answer
- Count how your code finishes, not only what it returns
- Ask the question late, where it is a measurement
- A sweep finds what a sample misses
- A default member initialiser can be a memset
- Theme SVG strokes with a class, never a literal colour
- A control can convict the code of the test's own mistake
- An instrument can be a tautology
- `acos` has a resolution floor, and it is 0.036 degrees
- A knob written for a real hazard can measure zero
- A monotone cell map is all a range-walk grid needs
- An A/B comparison must differ in exactly one thing — including where the code lives
- When the clock is noisier than the difference, find a quantity that needs no clock
- A cached quantity must carry the frame it was measured in
- Keep enough counters that they can contradict one another
- A timing sweep over a parameter that changes the simulation must restore the simulation
- A quantity read at the end of a step may belong to its start
- Sample the phase, not the parameter
- A count can be the wrong instrument where a consequence is the right one
- SVG collapses whitespace, so columns need coordinates
- A demo can hide the very thing it exists to show
- A satisfied one-sided row must keep its speculative target in the position pass
- A kinematic body is not a bridge, so it must wake things explicitly
- Drive kinematic bodies by velocity, and invert the integrator you actually have
- A twist limit's row is the half-way axis over cos(φ/2), not the bone
- Measure where an effect persists, not its deepest instant
- Instruments that measured their own floor this time
- zsh does not word-split `set -- $spec`
- A conservative step must be taken from a lower bound
- Clip the original motion, not what was left
- A skin thinner than the query's own margin is worse than none
- A certificate's looseness can scale with the object, not the tolerance
- A velocity-gated rule cannot see a teleported body
- "Helpful" code can create a second owner
- A measurement window can read an approach as jitter
- A quaternion's angle wraps at a full turn
- A zero-byte generated file is a silent regression

### [`learnings/tooling.md`](learnings/tooling.md) — the docs pipeline, builders, sweeps and repairs (14)

- A constraint nobody rechecked cost 18% of the docs tree (CSS extraction)
- Two correct branches can leave a hole between them (docs tooling)
- Course-infrastructure facts (docs/, 2026-08-26)
- Course-infrastructure facts (docs/, STATE-block consolidation)
- Renumbering a live course (roadmap reshape, 2026-09-08)
- Retrofitting a published lesson (2026-09-08)
- The figure-order sweep — three lessons, and what it uncovered underneath
- The builder reproducibility repair (2026-09-12)
- A label's colour is either semantic or an identity, and they have different fixes
- A page can look complete and still not add up to the engine
- A tagged excerpt is read as the whole file
- A move script is code: it can break a build, and it can be non-portable
- `GIT_INDEX_FILE` leaks into every child `git`, and `git reset` empties a staged index
- A harness cannot check a claim about somebody else's engine
