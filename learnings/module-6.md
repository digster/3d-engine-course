# Module 6 — learnings from its lessons

Moved verbatim from LEARNINGS.md on 2026-09-27, in the original order; LEARNINGS.md
is now the index. Append new sections at the end, and add each heading there.

---

## Colour pipeline facts (Lesson 6.1)

### The two integers

Code 128 emits **0.2159** of white's light, not half. Half the light is code **188**. Sixty codes
apart, and every mistake in this subject is a variation on that gap. If a 50% grey looks too dark
in a renderer, this is the first thing to check and it takes one line of arithmetic.

### The encoding is a budget, not a CRT artefact

The CRT story is true and useless — CRTs are gone and the encoding is not. Derive it from
perception instead: the eye judges **ratios**, so equal code steps should be equal ratios. Counted
over 256 codes:

| codes falling in… | evenly spaced in light | spaced by sRGB |
|---|---|---|
| the darkest tenth of the range | 26 | **90** |
| the brightest half of the range | 128 | **68** |

The even scheme spends half its budget where the eye can barely tell two neighbours apart. 8 bits
of *linear* light needs about **12 bits** to look as smooth as sRGB does in 8. Framed as
perceptual compression, the encoding stops being history and starts being a design.

### The toe is derived, not decreed

The derivative of x^(1/2.4) is (1/2.4)·x^(−0.583), which goes to **infinity** at zero. A pure
power curve therefore has unbounded gain at black: sensor noise becomes visible banding, the
inverse is numerically unstable, and an 8-bit code boundary is arbitrarily sensitive. Replacing
the bottom with a line of slope 12.92 fixes all three.

The four constants are not independent — 0.04045 = 12.92 × 0.0031308, and 1.055/0.055 are chosen
so the pieces **meet** (measured step at the join: 8.02 × 10⁻⁵, a fiftieth of a code).

`pow(x, 1/2.2)` is a **different curve**: worst disagreement **8 codes**, near linear 0.0010 —
down in the toe, exactly where the eye has the most codes to notice with. Fine as a stylistic
brightness knob; never as the output transfer function, which has to agree to the code with every
hardware sampler in the pipeline.

### Convert twice, at the edges — and the difference is structural

Per-operation conversion (which is what Lesson 1.6 shipped, and said so) is not merely slower. It
makes every *new* operation a chance to forget, and it quantises to 8 bits after every step.
Converting at the edges makes the middle a region where ordinary arithmetic is valid **because
there is nothing else there** — a property of the pipeline's shape rather than of anyone's care.

### The audit is the method

Not "be careful about colour spaces". Grep every conversion, list them, and give each one a job:
**input edge, output edge, or per-operation.** Fifteen sites in this engine outside
`colour.{hpp,cpp}`, and one row left over — the GPU path had an input edge and **no output edge at
all**.

Do this to a codebase you did not write and the interesting outcome is not "found a bug"; it is
discovering how many pipelines have *three* conversions and stay correct by cancellation. Those
break when somebody adds a post-processing pass.

### SDL claims windows with an sRGB-**encoded** swapchain

Straight from `SDL_gpu.h`, and it had been on disk since Lesson 0.4:

> `SDR: B8G8R8A8 or R8G8B8A8 swapchain. Pixel values are in sRGB encoding.`
> `SDR_LINEAR: B8G8R8A8_SRGB … accessed in shaders in "linear sRGB"`

and `SDL_ClaimWindowForGPUDevice` creates the swapchain with **SDR**. A fragment shader that
returns light into that is writing a linear value into a slot that means a code. Measured on this
machine, that is what the engine did for four modules.

### An error that is a *ratio* hides

Linear 0.5 stored as 128 where 188 was meant is 60 codes. But expressed as emitted light the error
is **13× at linear 0.02 and 1.1× at 0.95** — savage in shadow, absent near white. The bright half
of every image looked nearly right and the dark half read as a deliberate moody grade.

**An error shaped like a ratio hides wherever there is least contrast to spare.** Worth carrying
as a general diagnostic instinct: when a defect survived a long time, ask what shape it had.

### A test that constructs its own environment tests the environment it constructed

Lesson 4.8 compared the two renderers pixel by pixel and reported **87% byte-identical**. It was a
sound measurement of a configuration the shipped program does not use: the harness created its
render target as `R8G8B8A8_UNORM_SRGB` while the program renders to the swapchain, which was
`UNORM`. One enum, and it was the entire subject.

The fix is not "be more careful" — it is to make the production value a **readable field**
(`gpu_report::output_encodes_in_hardware`) so a harness can assert on it instead of writing its own
literal.

### Ask, set, then read it back

`SDL_WindowSupportsGPUSwapchainComposition` → `SDL_SetGPUSwapchainParameters` →
`SDL_GetGPUSwapchainTextureFormat`. The third call is the one people skip, and it is the only one
that reports what actually happened. SDL guarantees only `SDR`; everything else is a request.

### A two-parameter setter must pass through the parameter it is not changing

`SDL_SetGPUSwapchainParameters` sets composition **and** present mode together.
`set_present_mode()` passed a literal `SDL_GPU_SWAPCHAINCOMPOSITION_SDR` — correct when written,
because there had only ever been one composition — and would have silently undone this lesson's
fix the first time anybody toggled vsync. **This lesson's own bug, latent three functions away, by
a different route.**

### Encoding in the fragment shader is the second-best answer, and the reason is the blend stage

Hardware blending happens **after** the fragment shader. A shader that encodes hands the blend
unit *codes* to interpolate — which is the whole mistake, moved one stage later. Correct for
opaque geometry, wrong the moment anything is transparent. Prefer an `_SRGB` target; keep the
shader path as a fallback; read a field to know which one you are on.

### `pad2` was never wasted space

HLSL packs a `float3` and a `float` into one 16-byte register, so `scene_light_uniforms` already
had an addressable float sitting at offset 44 doing nothing. Renaming it `encode_output` cost
**zero bytes and zero repacking** — the struct is still 64 bytes and Lesson 4.6's `static_assert`s
did not move. Padding created by an alignment rule is an *unused field*, and it is free to use.

### A prediction that fails for a reason you can state is worth more than one that succeeds

Module 5's plan said the golden was "probably over" — surely a lesson that settles colour changes
the reference render. It did not, and the reason **is** the result: the software renderer had been
correct since Lesson 1.6, and the entire defect was on the GPU path's output stage, which the
golden never touches.

---

## Course-infrastructure facts (docs/, Lesson 6.1)

### Inline KaTeX fails the page check, every time

`check-page.js` asserts `katexRendered === eqBlocks`, and `eqBlocks` counts `<div class="eq">`. A
single inline `\(…\)` anywhere makes the two disagree — this page had seven and reported
`katexRendered: 10, eqBlocks: 3`. Write inline maths as HTML (`<em>x</em><sup>1/2.4</sup>`) and
keep KaTeX for display blocks only. Already recorded in the pipeline notes; recorded again because
it was still made.

### A new listing tag needs the CSS rule *and* the checker's allowlist

6.1 added a third state, `unchanged`, for a page that reproduces a file it did not edit (labelling
that "modified" is a small lie in a caption). `check-page.js` keeps its own `TAG_MODIFIERS` list —
deliberately, since the check exists to catch a class that matches no CSS rule. So both files have
to be edited, and **the CSS rule goes first**.

### When a label cannot find clear space among the data, stop looking among the data

Figure 1's curve label was placed twice inside the plot and sat on something both times — first
the polyline it named, then a dashed leader. The interior of a plot is mostly lines. Moving it to
the surrounding text column and naming the curve in words solved it immediately.

### Write the throwaway probe before the lesson

`scratch/probe_61.cpp` — forty lines, no test framework — established every fact the lesson rests
on before a word was written: the default swapchain format, which compositions this window
supports, whether the request succeeds, and what raw linear values look like when read as codes.
Reasoning found the bug; the probe is what made it a claim.

---

## Radiometry and BRDF facts (Lesson 6.2)

### The π in `albedo/π` is the area of a disc, and it has to be derived every time

The cosine-weighted hemisphere measures **π**, not 2π, and that single number is why π appears in
shading constants at all. The geometric reading is the one that sticks: every patch of the
hemisphere, projected straight down, casts a shadow of its own size times cos θ, and those shadows
tile the unit disc exactly once. So a constant BRDF *k* returns *k·π* of everything arriving, and
demanding that equal the albedo forces `k = albedo/π`.

Two diagnostics fall straight out, and they are worth memorising because they identify the bug
from the value alone:

| Your hemisphere integral of a white Lambert BRDF returns | What is missing |
|---|---|
| 1.0 | nothing — correct |
| 2.0 | the **cos θ** weight (you integrated 1/π over 2π sr) |
| 0.5 | the **sin θ** in `dω = sin θ dθ dφ` |
| 6.28 | the **π** in the BRDF |

And: **convergence is the diagnostic**. Quadrature error shrinks as the grid refines (1.0001004 at
64×128 → 1.0000004 at 1024×2048); a wrong constant does not shrink at all.

### A BRDF above 1 invents no energy — its unit is per steradian

The confusion is real and it is settled by the unit. Radiance (W/m²·sr) over irradiance (W/m²)
leaves **sr⁻¹**. A surface reflecting 100% of arriving light into a cone of half-angle α spreads it
over a projected solid angle of `π sin²α`, so its BRDF is the reciprocal: **0.3183 sr⁻¹** at
α = 90° (Lambert), **2.7210** at α = 20°, unbounded for a mirror. The quantity actually capped at 1
is the *integral*. Confusing the function with its integral is the whole of the mistake.

### A renderer with no units has a constant hiding somewhere — find it by asking one question

**What value of the light makes a perfect white surface render at exactly full scale?** If the
answer is 1, the diffuse BRDF has no π and the π is inside the light. If it is π, the BRDF has it.
Anything else means a third constant nobody has mentioned.

This works because it is a *measurement* rather than a reading of the code — two minutes with a
white quad settles what an hour of grepping might not. In this engine the answer was 1, and four
lines of algebra then said exactly what the field had been: `intensity = E_perp / π`, since Lesson
3.6.

### Rename the field when the meaning changes, even though the type has not

Moving the π into the BRDF makes every existing `intensity = 1.0f` wrong by 3.14 — and there is no
compiler diagnostic for "same type, new meaning". Renaming to `irradiance` turns each call site
into an error a human must answer. The engine has now made this bargain three times (3.1's `z`
ahead of `colour`, 3.7's defaultless `to_eye`, 6.2's rename) and it has never been the wrong call.

**And set the default to the meaningful value, not to 1.** A default of 1.0 compiles everywhere,
mentions nothing, and darkens every defaulted scene by π — which is the exact silent failure the
rename existed to prevent, walked back in through the constructor.

### Re-associating float multiplies is not a no-op, and 8-bit output absorbs it

`albedo * (key * 1.0f * ndl)` against `(albedo * inv_pi) * (key * pi * ndl)` moved **154,240 of
342,225** sampled results, worst relative error 2.465e−07 — one to two ULP. Not one 8-bit code
moved, through either encoder. Report both halves: "byte-identical" alone reads as "you changed
nothing", and "the arithmetic changed" alone reads as "the picture moved".

A related fact worth having: `std::numbers::pi_v<float> * std::numbers::inv_pi_v<float>` is
**exactly 1.0f**, and `1.0f/pi_v<float>` has the same bits as `inv_pi_v<float>`. Neither is
guaranteed by anything; both were checked rather than assumed.

### An empty diff can be evidence — but only next to a measurement that something changed

The claim was "the π is hiding in the light". That predicts that taking it out and putting the
light at π reproduces the image *exactly*. 1,209,616 bytes of agreement is the negative control
passing. It is evidence **because** the float sweep independently shows the arithmetic differed; a
change that did nothing at all would prove nothing at all.

### A constant that exists in two languages will eventually disagree

HLSL has no `<numbers>`, so `scene.frag.hlsl` carries its own 1/π. Write more digits than a float
can hold so the compiler rounds once — then *assert it*: `verify_62` §F reads the shader source,
parses the literal and compares bit patterns against `engine::k_inv_pi` (both `0x3EA2F983`). Three
lines against a class of bug otherwise found months later in a one-code pixel diff.

### "Not energy conserving" was true for a different reason than expected

The guess was that the un-normalised Blinn-Phong lobe would exceed unit reflectance at the
shininess values the engine actually uses. **It does not** — on the 1/π scale it reaches only
0.1386 at shininess 32. The failure that survives is structural: diffuse and specular are *added
with no coupling*, so a white surface with a white highlight reflects **1.1386** of what arrives,
and 1.7333 at shininess 2. The same light is counted once as having bounced off and once as having
gone in.

This reframes what Cook–Torrance's Fresnel term is *for*: not a better lobe shape, but the
mechanism that makes the two terms dependent — `kD = 1 − F`. **Measure before writing the
paragraph that explains the failure**; the probe that overturned this took ten minutes and the
wrong version would have shipped as a confident sentence.

### Give a lobe and a BRDF different names in a function that holds both

`specular_term()` returns a bare `cos^s` — a *shape*. `specular_brdf()` returns that shape with
units. `shade()` contains both, so the local variable was renamed `spec` → `lobe`. Dividing the
lobe by π is also documented as explicitly **not** a normalisation, because it normalises nothing;
calling it one would stop the next reader from asking the question that found 1.1386.

### A harness that builds its own environment tests the environment it built — second occurrence

`verify_48` §F filled the GPU light uniform from the lamp's *colour alone*, dropping the scalar.
Silently correct for four modules because the scalar was 1; the moment it became π, worst
|CPU − GPU| went from 1.2e−07 to **5.3e−01**. The shipped renderer never had the bug —
`gpu_scene.cpp` has always multiplied. **The instrument was wrong, and only changing a value the
instrument assumed constant could expose it.**

Lesson 6.1 recorded the same failure with a different constant (a harness building an `_SRGB`
target while the program used `UNORM`). Two occurrences make it a rule: when a harness constructs
inputs the shipped path also constructs, one of them will drift, and the drift is invisible while
the value is a default.

**The ratio names the bug.** Divide the two disagreeing values; if you get π, look for a missing
multiply by the light's scalar, not for a shading error.

---

## Course-infrastructure facts (docs/, Lesson 6.2)

### Pinning a published page's listings before editing — the rule applied on time

`build_61.py` was pinned to commit `373dd4b` **before a line of 6.2 was written**, all six
repository listings, each verified byte-identical with `git show <commit>:<path> | diff - <pin>`.
Re-running the builder then produced a diff of exactly the four nav lines that were *meant* to
change. That is what a correct pin looks like, and it is the first time in this pipeline the rule
was applied ahead of the damage rather than after a `git diff` caught it.

Pin **all** the listings, not only the ones you expect to touch: 5.10 needed two pins and only
predicted one.

### A wrong relative href in a prereq link fails silently — check every link, mechanically

Two of this lesson's four prereq links were wrong (`03-06-lighting.html` for
`03-06-normals-and-lambert.html`; `03-07-specular.html` for `03-07-specular-blinn-phong.html`).
Nothing throws and nothing looks broken; the link is simply dead. `check-page.js` does not cover
this. Twelve lines of Python that resolve every `href`/`src` against the filesystem do:

```python
for href in re.findall(r'href="([^"]+)"', html):
    if href.startswith(("http", "#", "mailto:")): continue
    if not os.path.exists(os.path.join(os.path.dirname(page), href.split("#")[0])):
        print("BROKEN", href)
```

Guessing a filename from a lesson *title* is the specific trap — the titles and the slugs diverge.

### A label on a shape the eye reads as background

`check-page.js` flagged figure 3's `n` label as sitting on an `ink-soft` line. It was true and
invisible: at θ = 0 the surface normal points straight back up the beam, so the label and the
centre beam arrow wanted the same pixels. Removing the arrow fixed it. **The geometry check sees
what the eye slides over**, which is exactly why it exists — a drawing can be wrong and still look
fine.

### Write the second probe when the first overturns a guess

`probe_62.cpp` answered its four questions and turned one over: the raw Blinn lobe does *not*
exceed unity where the engine actually runs. `probe_62b.cpp` existed only because of that, and it
found the failure that does survive. The habit is not "write a probe"; it is **write another one
the moment a probe disagrees with you**, because the paragraph you were about to write is now
wrong and you do not yet know what replaces it.

---

## Microfacet facts (Lesson 6.3)

### A distribution must integrate to 1 — and that is the only reason any of this is checkable

```
∫ D(h) · cos θₕ · dω  =  1
```

Read it backwards for the statement worth keeping: **the microfacets' projected areas add up to
the area of the flat surface they stand on.** Nothing else could be true, so the `cos θₕ` is not a
convention — it is the same projected-area factor as everywhere else in Module 6.

`shininess` could not be wrong; *D* can be, in a way a machine detects. Every distribution added
from here goes through the same test, unchanged. The diagnostic table, when it fails:

| Your NDF integral returns | What is missing |
|---|---|
| 2.0 | the `cos θₕ` weight |
| 0.5 | the `sin θ` in `dω = sin θ dθ dφ` |
| 2π | the φ integral |

Establish the integrator first on something with a known answer — integrate 1 and check for 2π,
then cos θ and check for π. Two lines, and they separate the instrument from the subject.

### Blinn-Phong was always a microfacet distribution; it lacked only its constant

The bare `cos^s` lobe integrates to `2π/(s+2)` — 0.1848 at *s* = 32. Give it `(s+2)/2π` and it
satisfies the identity exactly, at every exponent. Lesson 3.7 called it "the first, crudest
microfacet model" and was being literal.

This engine multiplies by `1/π`, so it is off by `(s+2)/2` — **exactly 17× at the default
shininess of 32**, and the ratio has no π in it because both constants carry one.

**And nothing ever looked wrong, which is the transferable part.** `specular::colour` absorbed it:
an artist turns the specular up until the highlight looks right and lands on seventeen times a
plausible reflectance. **An error a parameter can absorb is invisible until someone tries to
author against physical values** — which is exactly what Lesson 6.2 did, and why it found 1.1386.

Corollary for the fix: do **not** correct the constant alone. The materials were authored against
the wrong one. Change both in the same commit or every highlight blows out.

### GGX beats Beckmann on the tail, and only on the tail

Same peak for the same α (3.5368 at α = 0.3), so every difference is shape. GGX is *lower* near
the peak — 0.71× at 20° — and **456× higher at 45°**. It must trade: both integrate to 1.

The mechanism is in the formulas. Beckmann's exponential dies faster than any power and is
numerically zero (1.9e−13) by 60°, a hard edge where a real surface glows; GGX's rational
denominator has a power-law tail that never quite stops.

### "Matches" needs a criterion — the folklore mapping matches only the peak

`s = 2/α² − 2` comes from equating the two distributions **at h = n**, and it does that to one
float ULP. Fit the whole *lobe* instead and the exponent lands consistently **0.80×** lower (0.806
at α = 0.1, 0.759 at 0.5), because a lobe fit trades peak height for a tail Blinn cannot reproduce
at any exponent.

Both numbers are right. **When you meet a conversion formula, derive it far enough to see what it
optimised** — two minutes of algebra showed a widely-quoted mapping answers a narrower question
than the one people use it for.

### The textbook GGX denominator is wrong in `float`, and the normalisation test is what finds it

Written as every reference prints it:

```
d = cos²θₕ · (α² − 1) + 1
```

this is the difference of two numbers of size 1 that nearly agree — catastrophic cancellation. A
`float` resolves that to ~1e−7 absolute, while the true value of `d` at the peak is α². At
α = 0.01 that is 1e−4, so a thousandth of it is noise before the term is *squared*.

The identical expression, rearranged, does not cancel:

```
d = (1 − c)(1 + c) + α²c²
```

**Sterbenz's lemma** is why: for `c ≥ 0.5`, `1.0f - c` is computed *exactly*, with no rounding, in
any IEEE format — and `c ≥ 0.5` covers the whole peak region. Measured, integrated against the
identity:

| α | textbook | rearranged | double (truth) |
|---|---|---|---|
| 0.001 | 0.9891063 | 1.0023035 | 1.0000013 |
| 0.010 | 0.9998169 | 1.0000004 | 1.0000000 |
| 0.050 | 1.0000006 | 1.0000000 | 1.0000000 |

**The textbook form loses 1.1% of the model's energy to rounding at mirror roughness.** The bug is
bounded by the data — it lives entirely below α ≈ 0.05 — which is why it survives in production
and shows up on chrome and still water rather than everywhere.

`verify_63` §A asserts the comparison inline, computing the textbook form and requiring ours to
beat it by 4×, because the rearrangement looks like a pointless shuffle and the next person to
tidy it back should be told by a failing test.

### Quadrature error shrinks when the grid refines. Arithmetic error does not.

This is the whole diagnostic, and it took two runs. Refining 5× moved 0.9998169 to 0.9998154 —
nothing. The identical formula in `double` on the identical grid gave 1.0000000. Diagnosis
complete: the error is in `float`, and in the formula rather than the integrator.

**Do not widen the tolerance.** The value of a test with a known answer is entirely in believing
the known answer; a tolerance widened to admit a failure is a test deleted slowly. 1.8e−4 looked
small and was a real 1.1% energy loss hiding at the smooth end.

### Shadowing and masking are a consequence, not a correction

Once you have said "landscape", you have said "some of it is hidden". Smith's *G* derives from the
same slope distribution that produced *D*, which is what makes it a consequence rather than a
second model.

It is also **the term Lesson 6.2 was missing** when it measured a 5.36× view-angle swing it could
not explain: at 75° a near-smooth surface still shows 0.9920 of itself and a fully rough one
0.4112. The effect was always real; it lacked a reason.

**The separable form's independence assumption is false, and measurably so.** Multiplying two
`G₁`s assumes being lit and being visible are unrelated events; they share a height field.
Height-correlated against separable: identical to four decimals on smooth surfaces, and **1.715×
apart at α = 0.8 with both directions at 80°**. Grazing angles on rough surfaces are sunsets, wet
roads and worn metal — "either is fine" would have been comfortable and wrong.

### Single-scattering microfacet theory loses 69% at full roughness

With *F* = 1 — a surface that absorbs nothing — R(v) for `D·G/(4 n·l n·v)` runs 0.9976 at
roughness 0.22 down to **0.3069 at 1.00**. It never *exceeds* 1, which is what the machinery
bought; but *G* removes the light that hits a hidden facet and never asks where it went. It hit
another facet and carried on.

The symptom is specific: **rough metal renders too dark, and it is a fraction rather than an
offset**, so raising the light scales the problem instead of fixing it. Kulla–Conty's
multiple-scattering compensation is the standard fix and needs an assembled BRDF to attach to.

### `α = roughness²` is a convention and a real interoperability trap

Disney's remap, shipped by UE4 and Filament, chosen because almost all the visible change lives in
α's bottom fifth — the same argument 6.1 made about sRGB spending codes where the eye is. Not
physics. A roughness copied from a tool that squares differently will not match, so convert at the
import edge and write down which convention you store.

---

## Course-infrastructure facts (docs/, Lesson 6.3)

### A figure whose numbers contradict its own caption

The first draft of the microfacet landscape counted **32 of 74** facets as facing **h** — 43% of
the surface, which says "most of it reflects at you" and is the opposite of the model being drawn.
The threshold was a guess. Tightening it to a ~4° cone gave 4 of 48, which is what a glossy surface
actually looks like.

**No geometry check catches this**: nothing overlaps, nothing spills, and the picture is perfectly
well-formed. The only test is reading the caption against the drawing.

### Draw the negative case too

The same figure only became legible when the *non*-participating facets got their normals drawn
faintly. A picture of the four that count is a picture of four sticks; a picture of four heavy
normals among forty-four faint ones is the model. **The contrast is the message, so the thing that
does not qualify has to be visible.**

### An arrow that ends in mid-air reads as a drawing error

Lesson 6.3's masking figure first drew its light rays as a floating row above the surface, pointing
the wrong way because the direction vector's sign was never checked against the screen's y-down
convention. Terminating each ray *on the facet it strikes* fixed both problems at once — and the
sign error was invisible until the arrows had somewhere to land.

### Pick plot limits from the data, not from the first curve you tried

The NDF figure clipped its sharpest curve at a hand-chosen ceiling, which renders as a flat top
and reads as a bug. With cosine weighting the peak is `1/(πα²)`, so the ceiling is computable —
and once it was, the honest picture (peaks differing 16×, areas all exactly 1) turned out to be a
better illustration than the clipped one.

---

## Lesson 6.4 — Cook–Torrance, assembled

### Write a second probe the moment the first disagrees with you — then a third

Three probes, and each existed because the previous one overturned something. `probe_64` was
supposed to confirm that Fresnel coupling closes the energy door; it left **1.2031** at grazing.
`probe_64b` asked whether that was the instrument (refine the grid — it *converges*, so no) and
where it sat (specular takes 0.40, diffuse gives up 0.05). `probe_64c` tested the resulting
*diagnosis* — that the missing factor is the light which fails to get **out** — by predicting an
exit factor would remove it. It did.

The chain is the technique. **A probe that confirms your plan has told you nothing you did not
already believe; a probe that contradicts it has told you where the lesson actually is.**

### A fix that passes the test it was written for can fail a test you forgot

The exit-factor repair killed the over-unity exactly as predicted — and it is **not reciprocal**
(0.262327 one way, 0.293236 the other), which means it is not a BRDF. The prediction was right and
the patch was still wrong.

Keep the *defining properties* of the thing you are building in a list, and check a candidate
against all of them, not only against the symptom that prompted it. For a BRDF that list is:
non-negative, reciprocal, and energy-conserving. The repair scored two out of three.

### An error a parameter can absorb is invisible — so give every parameter an inverse

Lesson 6.3 found a 17× normalisation error hiding inside `specular::colour`. 6.4 found the other
half of the same story by writing `ior_from_f0` — three lines whose only purpose is to run a
material backwards and ask what it claims about the world. The demo's authored 0.85 comes back as
an **index of refraction of 24.6**, where diamond is 2.42.

**A parameter that round-trips into a physical quantity can be audited automatically; one that
cannot, cannot.** That is a design argument for physical parameterisations that has nothing to do
with realism — it is about testability.

### When the meaning of a value changes, delete the type

`specular::colour = 0.85f` and `f0 = 0.04f` are both perfectly good floats. There is no diagnostic
anywhere in C++ for "same type, new meaning", so a struct that kept its name would have let all
forty-four call sites keep compiling while rendering twenty-one times too bright.

Deleting `specular` outright turned every one of them into a compile error. That is the third time
this course has made the trade (3.7's `to_eye` with no default, 6.2's `intensity` → `irradiance`),
and it is worth stating as a rule: **a rename or a deletion is the only refactoring tool that
reaches every caller.** Doc comments do not.

### Corrections that compensate must land in one commit

The specular constant was 17× too small and the authored specular colours were ~21× too large.
Either fix alone makes the picture *worse* than leaving both wrong — highlights blow out, or
everything goes black. There was no tidy sequence of small commits here, and pretending otherwise
would have meant shipping a broken intermediate state.

**When two errors are compensating, the unit of work is both of them.** Say so in the commit
message, because a reviewer looking at half of it will be right to object.

### The moment to extend a characterization test is the moment it breaks

Replacing the entire shading model moved **0.60%** of the reference render. Not because the change
was small — because `shading::textured` is unlit by construction and one scene faces away from the
light, so **three of seven frames exercised the shading equation at all**, and the torus chosen in
3.8 *because* it shows highlights was drawn unlit.

This is Lesson 5.1's finding a second time (there it was the near-plane clipper: *a
characterization test that does not reach a branch cannot pin it*). The new half is about timing:
an eighth frame costs **nothing** at a lesson that re-baselines the golden anyway, and costs a
whole re-baseline at every other lesson. So the moment a golden breaks for a good reason is the
cheapest moment it will ever be to improve it.

### Re-baseline honestly: record the direction, not just the hash

`905BF27E → E917C06C` says nothing. What made the new baseline believable was the *direction*:
2,400 pixels changed and **every one got darker**, none brighter — which is exactly what an
energy-conserving model replacing one that emitted light must do. A single brighter pixel would
have been a question to answer before shipping.

Record: old hash beside new, the per-channel magnitude, and a claim about the sign that the data
can contradict.

### HLSL cbuffer members share one global namespace

Adding `pad0` to a second cbuffer is a *redefinition*, not a local name — and the error, "nameless
block contains a member that already has a name at global scope", does not obviously say so. Name
padding per buffer.

### A truncated bar axis lies about the data it is there to report

The energy figure first drew its bars from 0.6, which made 0.71 and 1.24 look like a five-fold
difference in a chart whose entire subject is *magnitudes*. Zero-based, with the unity line drawn
in, is both honest and still perfectly legible — the 1.0 crossing is what the eye needs, not the
bar heights.

### Put the label where the geometry cannot reach, and compute where that is

Three separate label placements failed `check-page.js` in this lesson, and the instructive one was
the arc label: I placed "2θ" at its arc's *midpoint*, which by construction **is** the h ray. The
obvious choice was the one guaranteed to collide.

Two habits came out of it. Give a label an explicit angle rather than deriving it from the shape it
annotates. And when a figure needs four labels and four strokes in one quadrant, the fix is usually
to draw fewer things — the second ray pair in the Jacobian figure was cut, and the figure got
better.

### Geometry checks cannot see clutter

`check-page.js` passed a version of Figure 1 that a reader would have struggled with: two panels,
each with three rays crossing, labels technically clear of every stroke. The rewrite abandoned the
ray diagram entirely for a **budget bar**, because the claim being made was an accounting claim.
Look at the rendered figure and ask what it is *for*; no geometric check will ask that for you.

---

## Lesson 6.5 — A material system

### A demo that has to invent one of your types is measuring a hole in your API

`ecs_swarm` declared `struct material { Uint32 tint; microsurface surface; }` in Lesson 5.7, with a
comment naming exactly why. It is restricted to the engine's public headers, so it could not reach
inside to work around the gap — which is what makes it evidence rather than an opinion.

**When a consumer that cannot see your internals builds one of your types, the type is missing.**
Not "would be nice"; missing. Two engine structs had been *saying* the same thing in comments for
three modules and nobody acted; the demo's forty lines of workaround were the thing that finally
counted as proof.

### The membership rule for a container type should come from the machine, not from taste

"What belongs in a material?" has no good answer in English — everything surface-shaped sounds
right. It has a sharp answer in hardware: **can this be a number in a buffer?** Per-draw data can be
a uniform; pipeline state cannot, and two objects differing in it need two pipelines and a sort.

The test is even mechanically checkable — `static_assert(std::is_trivially_copyable_v<material>)` is
the compiler agreeing that the type can be pushed. **When you can find a rule the machine already
enforces, use that one**; it survives arguments that a style guide does not.

### Store a handle, use a pointer, and name the step between

Both of these are true and they pull in opposite directions: a stored reference must survive a
reallocation and be checkable, and an inner loop cannot afford a lookup per pixel. The resolution is
not to pick one — it is a named function that runs **once per draw**.

The measurement that makes it concrete: 64 insertions moved a pool from `0x928c03500` to
`0x928c16000`. Every pointer taken before that is now wrong, and nothing about it says so. The
handle names a slot and does not care.

And the case a pointer cannot express at all: a **stale** handle — well-formed, naming something
gone — resolves to "no image" and the surface falls back to its tint. Undefined behaviour became a
defined fallback, which is worth more than the four bytes it saved.

### An argument that is only true of *your* caller is not an argument about the type

The comment defending the old raw pointer said: *"`textures` lives for the whole program, so there
is nothing here that can dangle."* That was **true**. It was also a claim about one demo's lifetime
discipline, unavailable to anyone storing a material in a file, a component, or a scene.

When you find yourself justifying a type's design with facts about its current callers, you have
found a constraint that will break the first time the type is used somewhere else.

### Derive it, and the bug becomes unrepresentable

A `textured` flag stored beside a separately-chosen texture pointer can say "textured but no image"
(debug magenta) and "image but not textured" (silently flat). Neither fails where the mistake is.
Computing the flag from the handle removes both states from the program's vocabulary.

**A bug you cannot express beats a bug you detect.** — and note the trap on the way: replacing the
hand-assembly with the derived version compiled cleanly and would have silently lost every GPU
texture, because the *other* opinion (a keypress and a scene test) had never been written onto the
material. Deriving a value only helps if the field you derive from is the one that knows.

### A pool is for sharing, not for size

Ninety-six drones over six tints: 3,456 bytes of copies against 600 of handles. But the byte count
is the weaker half. With copies, "make the drones rougher" is a loop over the registry that has to
find every entity carrying that appearance; with handles it is one write.

The folk rule — "use handles for big things" — gets this case backwards. A material is small and the
handle is still right, because ninety-six things point at six. Meanwhile `scene_object` holds its
material *by value*, because it has exactly one. **Ask whether anything shares it, not how big it
is.**

### A comment predicting a future design cannot be tested

`scene_object::closed` carried a comment from 3.4 saying it would move onto the material in Module
6, "because cull mode is pipeline state and pipeline state is what a material *is*". It repeated
through three modules and was wrong twice over — a material is explicitly *not* pipeline state, and
`closed` is not cull mode anyway but a fact about the mesh.

This is not an argument against writing such comments; 3.7's identical prediction about the material
was exactly right and is why the lesson happened. It is an argument for **re-reading them when you
arrive, as claims to check rather than instructions to follow.**

### The measurement you take to strengthen an argument can be the only false part of it

Splitting `cull_mode` into its own header is justified by one rule: a type two components share gets
a header. That was enough. I reached for a second, quantitative justification anyway — compile time,
citing 5.1's measured 0.97 s for `raster.hpp`.

Re-measured on the machine actually building: `raster.hpp` 0.258 s, and `material.hpp` **already**
0.261 s via `texture.hpp`. The marginal saving is zero. Worse, 5.1's own note says in capitals
**"THE RATIO IS WHAT TRAVELS, NOT THE SECONDS"** — I read past the caveat to the number I wanted.

Two rules out of one mistake. **A recorded caveat does not protect you if the figure beside it is
more useful.** And: when a design rule stands on its own, adding a number to it is not free — it is
a new claim, with its own chance of being wrong.

### After a struct's layout changes, an incremental build is not evidence

Capturing a "before" render meant `git stash`, rebuild, shoot. The rebuild was incremental and
`scene_object` had changed size, so some translation units had the old layout and some the new.

The symptom was not a crash. It was a reference shot with the *wrong scenes in it* — frame 1
rendering `solids` instead of `cycle`, a triangle count of 20 where 44 was right, one frame's name
printing as `?`. **Plausible garbage**, and it took a minute to tell apart from a real regression.
`cmake --build build --target clean` fixed it. This is the same failure class as a uniform block
disagreeing with its shader, arriving on the CPU side.

### Two unclosed `<figure>` tags, and every automated check passed

`check-page.js` verifies highlighter round-trips, KaTeX, SVG geometry, shared assets, badge classes
and listing folds — and passed a page whose figures were nested inside an unclosed
`figure.listing`, silently inheriting `display: flex` on their captions. Two captions rendered as
forty-pixel columns of single stacked characters.

No geometric check sees that: the SVG is fine, the labels are clear, the links resolve. It was found
by **looking at the rendered figure**, which check-page.js's own header says is the thing it cannot
do for you. `scratch/check-tags.py` now balances the container tags, which is the cheap half — but
the expensive half stays: *look at the picture*.

---

## Lesson 6.6 — glTF 2.0 loading

### The write-it-or-take-it test is not "is it hard?" — it is "is the hard part the subject?"

This course has hand-rolled the maths library, the rasterizer, the OBJ parser and the ECS, and taken
`stb_image`, Dear ImGui and now `cgltf`. Difficulty does not separate those lists: the rasterizer
was harder than a PNG decoder.

What separates them is whether the difficult core belongs to the discipline you came to learn. For
OBJ the hard part was unifying `v/vt/vn` triples into vertices, which **is** the index problem. For
glTF the hard part is that an accessor may be any of five component types, normalized or not,
tightly packed or interleaved at a stride, dense or sparse — roughly forty legal encodings of the
same eight positions, every one of which some exporter emits, and **not one of them about
graphics**. `cgltf_accessor_read_float` collapses the whole matrix in one call.

Stated as a question you can ask of a library you have never seen: *if I write this myself, what
will I have learned when it works?*

### Two conventions agreeing is worth asserting, not just noting

Three of the four convention checks between glTF and this engine came out "do nothing" — handedness,
winding, and texture origin. The temptation is to write a sentence in the lesson and move on.

Assert them instead. `verify_66` §B checks that vertex 6 of the loaded cube is exactly
`(+0.5, +0.5, +0.5)` and that all eight positions match `k_cube_vertices` **bit-exactly** — no
tolerance, because every coordinate in the file is exactly representable, so a difference would be a
real one. The reason it is worth a test rather than a sentence: **a cube survives the wrong answer
looking like a cube.** Negate z and it is still a cube, wound inside out. Nobody reviewing a
screenshot catches that.

### The convention that agrees can be more dangerous than the one that differs

glTF and this engine both put texture coordinate `(0,0)` at the *upper* left. OBJ puts it at the
lower left, which is why `mesh_import::flip_uv_v` defaults to `true` — correct for every mesh this
engine had ever loaded.

Apply that default to a glTF and every asset arrives upside down, for everyone who left it alone.
**The flip belongs to the FORMAT, not to the caller's preference**, so `asset_store::load_model`
takes no import settings at all and its header says why in as many words. A parameter that is right
for one format and silently wrong for the next is worse than no parameter.

And the one that *does* differ turned out not to need code at all: glTF says an asset's front faces
+Z and our camera looks down −Z, which is an **authoring fact**, fixed with a yaw. Negating
coordinates in the loader would also mirror the geometry and reverse every triangle — two wrongs,
the second hiding the first.

### An unverified claim in a comment survives exactly as long as nobody needs it

Lesson 6.4 wrote that `1 − F(v·h)` is "what the glTF 2.0 reference BRDF uses" and marked it
`⚠ VERIFY`. Two lessons passed. Checking it took twenty minutes and found the claim **true in
substance and imprecise in a way that changed the design**: the spec writes
`mix(diffuse, specular, F)`, so the same Fresnel that scales the diffuse down scales the specular
*up*, and our specular already carried it. Reading "glTF uses `1 − F(v·h)`" as a statement about the
diffuse alone misses half the sentence.

The habit worth keeping is not "always verify immediately" — 6.4 was right to defer, and said so
with a marker and a lesson number. It is that **the marker has to name the lesson that discharges
it**, or it becomes furniture.

### Reduce two models to named terms, then price the survivor at BOTH ends

Comparing BRDFs by rendering them is useless: two that differ by 4% look identical and two that
differ by 5× look like a lighting change. Write both as a product of named terms, line them up, and
find the terms that are not the same expression. Against Khronos Appendix B, six terms line up and
**five are identical**.

Then price the survivor at both ends of its range, because one number is not a measurement. Our
diffuse coupling against the spec's is **1.036×** at normal incidence and **5.62×** at 88°.
Reporting only the first says "the models agree"; reporting only the second says "they are
unrelated". Together they say **where to look** — the difference lives at silhouettes and vanishes
in the middle of every surface, which is exactly why it went unnoticed for two lessons.

### Two things that look like different models can be the same expression

The glTF spec blends two complete BRDFs by `metallic`; this engine blends the F0 and evaluates one.
They commute **exactly**, because Schlick is affine in F0: `F(f0) = f0(1 − w) + w` is a straight
line, so lerping the inputs and lerping the outputs agree. Measured at 1.19 × 10⁻⁷ over 4,851
points — float rounding and nothing else.

Worth looking for whenever two implementations of "the same thing" appear to disagree structurally.
**Check whether the operation you are interpolating through is linear** before concluding one of
them is an approximation of the other.

### A gap between two working halves is invisible to every test, because no path crosses it

The engine has been able to decode a PNG since Lesson 5.3 and to sample a texture since Lesson 3.9,
and **nothing has ever joined them**. Every CPU texture was generated (`make_checker`,
`make_uv_grid`) and every loaded image went straight to the GPU as bytes. Two complete, tested,
well-documented halves with no middle.

No test could have caught it: there was no wrong behaviour, only absent behaviour, and coverage
measures the code that exists. It surfaced the moment a glTF material named an image file and the
software renderer had to sample it — twelve lines, `to_texture`.

The generalisation: **look for pairs of subsystems that ought to compose and have never been asked
to.** They are where the next feature will discover a hole.

### A conformance gap belongs at the point that knows both halves

glTF multiplies a base colour factor by its texture; this engine's albedo image replaces the tint.
They agree exactly when the factor is white, so the gap is only the *combination*.

The parser sees the factor and the URI but not whether the image resolved. The renderer sees a bound
texture but has long since lost the factor. Only `load_model` holds both, so that is where the
warning lives — and it is a **counter** as well as a log line, because a log line scrolls past and a
number can be asserted. `verify_66` asserts `factor_texture_conflicts == 0` on an asset built so it
should be zero, which tests the *detector* as much as the asset.

### Report a format limit; never truncate to meet it

`mesh_data::indices` is `uint16_t`, so a primitive can name 65,536 vertices. Real glTF assets exceed
that routinely. The lazy version is `static_cast<uint16_t>(index)` and let it wrap — and that
version **renders**, as a spray of triangles connecting the wrong corners, looking like a corrupt
file rather than a loader limit, with nothing anywhere saying what happened.

Widening the index type costs bytes on every mesh in the engine; splitting the primitive costs code.
Both are real answers and neither is a thing to guess at inside a loader. What the loader owes the
caller is `skipped_too_large = 1, max 197,346` — a number they can act on.

### An assertion in a test can be wrong about the code's *correct* behaviour

`verify_66` §A asserted the loaded cube had 8 vertices, matching `cube_mesh()`. It got 36, and the
loader was right: the test asset shipped no normals, so flat normals were generated, and flat
shading forces one vertex per face-corner. Lesson 3.5's index problem, arriving in a second format.

The fix was not to relax the assertion — that is a test deleted slowly (6.3's rule). It was to make
the **assets say which case they test**: `cube.gltf` now ships smooth normals so its round trip stays
index-for-index *and* proves the loader honours authored normals, and `shapes.glb` ships none so the
generation path and its 4.5× vertex-count cost have an asset of their own. One failing check turned
one test into two better ones.

### A physically correct render can be a bad picture, and saying so is the lesson

`shapes.glb`'s metals render nearly black, or clipped to white. That is not a bug: a conductor has no
diffuse lobe, so a metal with nothing to reflect but one directional light has exactly two states.
Measured, the bright one is **64× displayable white** at roughness 0.25.

The temptation is to fudge the asset until the screenshot looks good. The better move is to keep the
values defensible, *measure* the clipping, and let the number be the argument for the two lessons
that fix it — image-based lighting gives the metal something to reflect, and a tonemapper gives the
highlight somewhere to go. A demo whose flaw has a lesson number attached is teaching.

### Check forward lesson references against the index, not against memory

Lesson 6.4 shipped three stale "Lesson 6.7" references for glTF, which is 6.6. Lesson 6.6 then wrote
"6.9's image-based lighting" and "6.11's frustum culling" — both wrong, because IBL is **6.12** and
frustum culling is **6.13**, and it also got the next lesson's *title* wrong ("TBN Basis" against the
index's "TBN Derivation").

Twice in three lessons, from the same cause: writing a forward reference from memory of the plan
rather than from `docs/index.html`, which is the plan. **The index is the authority.** Grep the
lesson body for `6\.[0-9]` before building the page; it takes ten seconds and it has now caught six
errors.

---

## Lesson 6.7 — Normal mapping and the TBN derivation

### Three of the hardest bugs here share one shape: they produce a plausible picture

A normal map read as sRGB tilts every surface by 38.8°, which reads as "this map was authored too
strong". A dropped handedness lights one half of a symmetric model as the mirror image of the other,
which reads as a modelling error. A tangent transformed by the normal matrix skews the frame within
the plane, which reads as an asset authored at the wrong angle.

None of them looks like the thing it is, and each has a *plausible wrong fix* that makes the picture
less wrong without making it right. **That is the argument for a type or an assertion rather than a
comment**: a `vec4` that cannot lose its sign, an enum on the texture that no caller can forget, and
a round trip whose floor is predicted from the encoding.

### Look for pairs of subsystems that ought to compose and have never been asked to

The GPU path has had a colour-space flag since Lesson 4.7 — `create_sampled(…, srgb)` picks
`_UNORM_SRGB` or `_UNORM`. The software renderer had **no counterpart at all**: `sample` decoded
unconditionally, and was right every time, because every texture it had ever been given was an
albedo.

Two halves of one idea, one complete and one absent, and no test could see it because there was no
wrong behaviour — only absent behaviour, and coverage measures the code that exists. This is the
second time in two lessons (6.6 found the same shape between `load_image` and `sample`), which makes
it a habit worth having rather than a coincidence.

### Derive a test's tolerance from the encoding; never choose it

`verify_67` §D asserted an exact round trip through a flat normal map and measured 5.55e−03. The
renderer was right and the assertion was wrong: **0.5 is not an 8-bit code**, so 128 decodes to
1/255 rather than 0, and every flat normal map in existence tilts its surface by 0.318°.

The wrong repair is to loosen the tolerance until it passes — 6.3's rule, a test deleted slowly. The
right one is to *predict the floor from the encoding* and assert the measurement equals it, which is
strictly stronger: it now also catches an implementation that is somehow **better** than the
quantisation floor, which would mean the encoding is not what we think it is.

§E made the same mistake in the other direction. Its first bound was `1/510` — which forgot that the
`[-1,1]` decode **doubles** a half-code error — and produced a bound *below* the true floor. **A
tolerance that is wrong rather than merely loose fails a correct implementation**, which is the more
expensive of the two mistakes.

### A parameter whose effect changes when you change an unrelated parameter accuses the wrong subsystem

`make_normal_bumps` first took `strength` as the height field's amplitude. `A·cos(ku)·cos(kv)` has a
peak gradient of `A·k`, and `k` grows with the cell count — so an amplitude of 1 at six cells is a
slope of 37.7, a surface tilted 88° everywhere. Every normal points sideways, `n·l` collapses, and
the render goes dark.

The symptom pointed straight at the renderer. It took a minute to realise the *shading was correct*
and the input was absurd. Dividing the amplitude by `k` makes `strength` mean the maximum **slope**,
so 1.0 is a 45° tilt at any cell count.

The general form: a parameter you cannot reason about in isolation produces failures that accuse
whatever consumes it.

### Assert the numbers you also put in a diagram

`gpu_vertex_pnu` grew from 32 bytes to 48, and the build stopped at Lesson 4.5's
`static_assert(sizeof(gpu_vertex_pnu) == 32)` before a pixel was drawn. That assertion exists
because 4.5 drew a memory-layout figure, and the alternative to asserting the number is a lesson page
that says 32 bytes forever while the code says 48.

Two more caught the same growth: `verify_54`'s `sizeof(mesh)` and `verify_56`'s
`sizeof(scene_object)`, the latter for the **second** lesson running. A struct quietly crossing a
cache line is 5.6's whole subject, and it is only visible if somebody wrote the number down.

### A shared vertex layout is an interface, and widening it renumbers every consumer

Adding a fourth mesh attribute made `gpu_mesh::describe` emit location 3 — which Lesson 4.6's
instancing pipeline already used for per-instance placement. Vertex attribute locations are numbered
**across the whole pipeline**, not per buffer.

4.5's `check_layout` caught it in `verify_46`: it compares declared attributes against the shader's
*reflected* inputs and reports `extra` and `duplicate`. Without it the symptom would have been
`placement` silently fetching a tangent's bytes — seven instances at nonsense positions, in a demo
three modules old, with nothing pointing at the lesson that broke it.

Two fixes were available and the smaller one was also the better design. Renumbering every consumer
teaches "a layout is an interface" and is real churn; making the attribute **optional** teaches that
**a vertex layout is per-pipeline state**, which is the more useful fact — a shader that never reads
an attribute should not declare one, because it is a fetch paid for nothing.

### Adding a field to a struct is a good reason to convert its initialisers

Two positional aggregate initialisations in `soft_renderer.cpp` broke when `vertex` grew a
`tangent` between `normal` and `world`. That was a *compile error* — a `vec3` where a `vec4` was
expected — which is the good outcome.

The version worth designing against is the one that **compiles**: two same-typed fields swapping
places, silently. `mesh.hpp` made exactly this argument in Lesson 3.5 and this file never took it.
The moment a struct grows is the moment to convert.

### Gram-Schmidt twice is not redundancy

The tangent frame is orthogonalised at the vertex *and* again at the fragment, and both are
necessary. The vertex pass makes the frame orthonormal **at the vertices**; interpolation across the
triangle destroys both orthogonality and unit length again, exactly as it does for the normal (3.8
measured that). The per-vertex pass makes the *inputs* sane; the per-fragment pass makes the *frame*
sane.

Worth stating because "we already normalised that" is a very natural objection and it is wrong for a
reason that is easy to say once and hard to reconstruct.

### An asset's defect can be invisible until a feature makes it visible

`assets/cube.gltf` carries one uv per box corner, which Lesson 6.6 chose deliberately so its
round-trip test could compare index-for-index against `cube_mesh()`. The consequence — four of six
faces have **zero uv area**, and therefore no tangent frame — was not detectable by anything until
normal mapping arrived and made those faces band instead of dimple.

Nothing was wrong with the asset, the test, or the loader. A property that had no observable
consequence acquired one. Worth remembering when a new feature makes old content look broken: check
whether the content was always like that.

### A regular sampling grid aliases against a regular texel grid

`verify_68` §C measures shadow acne by walking a grid of points across a plane and counting how many
report themselves shadowed. The first version used a plain 96×96 grid and reported **3.3%** where the
true figure is **50.1%**, and a worst-case depth error at 22% of a bound that is in fact tight.

Nothing was wrong with the renderer. The grid's spacing came out at 4.8 shadow texels, so every
sample landed at nearly the same position *inside* its texel: the measurement covered a sliver of the
sub-texel space instead of all of it. The fix is a **Weyl sequence** — step the sub-texel offset by
the golden ratio's fractional part, the number hardest to approximate by a rational and therefore the
one that never falls back into step with the grid.

This is not a testing footnote. It is the same aliasing the shadow map itself suffers from, it is why
a shadow's edge crawls when the light moves, and it is the reason cascaded shadow maps snap their box
to texel boundaries. **When a measurement disagrees with your eyes, suspect both.**

### An assertion stricter than its own message fails correct code

`verify_48` carried `sizeof(scene_light_uniforms) == 64 && sizeof(material_uniforms) == 32` under the
message *"the two fragment blocks fill whole registers"*. Lesson 6.8 grew the light block to 176
bytes — still a whole number of 16-byte registers, so the property the message names was never
violated — and the harness failed.

The invariant is `% 16 == 0`. Pinning an exact size under a message about packing meant a legitimate
change failed a test that was not about it, and the diagnosis cost more than the fix. Write the
assertion the message claims; if an exact size is also worth pinning, pin it *separately*, with its
own message saying which lesson set it and why.

### Where your rasterizer samples decides `round` versus `floor`

This engine's `fill_triangle` evaluates every attribute — depth included — at **integer** pixel
coordinates, not at pixel centres. So the depth stored "for texel *i*" is the surface's depth at
exactly *x* = *i*, and a shadow lookup must take `round(x)` to find the nearest stored sample.

Hardware samples at pixel *centres*, and `SampleCmp` selects the texel *containing* the uv — a
different spelling of the same [−0.5, +0.5) offset. Write `floor` on the CPU side to "match the GPU"
and every offset is biased half a texel in one direction: the sampling reach doubles, the derived
bias is half what is needed, and acne returns on one side of every slope while the other side looks
perfect.

The general form: **a lookup and the pass that wrote the thing being looked up must agree about where
a sample sits**, and that agreement is a property of the rasterizer, not of the API.

### A bound that is never reached is not a derivation

Lesson 6.8 derives shadow bias as `reach × world_per_texel × tan θ ÷ depth_range` and then measures
the worst actual disagreement: 1.931e−3 against a bound of 2.101e−3, **92% of it**.

That last number is the point of the test. A bound you cannot exceed is easy to write and tells you
nothing; a bound that is *reached* means the derivation describes what actually happens. Had the
measurement come out at 22% — as it did, from the aliasing above — the bound would have been
describing something else, and the bias built on it would have been a superstition that happened to
work.

Applies to any derived tolerance: **measure how close the worst real case gets to it, and treat a
large gap as a bug in the derivation rather than as safety margin.**

### Anything derived from one sample's footprint must be re-derived when the footprint widens

The bias above was derived for a single lookup, where the fragment sits somewhere inside its own
texel — half a texel diagonal, `√2/2 = 0.707` texels of reach. A 3×3 PCF kernel reads a texel one
step out in each direction, so its furthest tap is **2.121** texels away: three times as far.

A bias sized for one tap therefore covers a third of what a 3×3 needs, and the acne walks back in **at
the exact moment PCF is switched on** — which makes it look like a filtering bug rather than a bias
one. Measured: 78 stray pixels at radius 0, **10,348** at radius 1, 20,265 at radius 2; after the
correction, 174 and 224, which is the legitimate soft edge.

Found by rendering, not by reasoning. The general rule is worth carrying: a formula whose inputs
include "how far apart are the two things I am comparing" has a hidden dependency on every filter,
kernel or footprint downstream of it.

## Lesson 6.9 — Cascaded shadow maps

- **A derived formula proves itself when you change what it was derived from.** 6.8's bias was
  `reach·world_per_texel·tanθ/depth_range`. Cascades change `world_per_texel` by 7.3×, and not one
  line had to move — because both terms live in `light_camera` and `visibility()` already read them
  from there. That is the difference between a formula and a tuned constant with a good story: the
  constant needs a new value per cascade, the formula does not. **Design the audit into the next
  lesson, not into the one making the claim.**

- **The strongest result was the one nobody arranged.** In device depth the bias is *constant*
  across cascades 1–3 (2.031e-03), because a sphere-fitted cascade has `wpt = 2r/res` and
  `range ≈ 2r`, so the radius cancels and `bias ≈ reach·tanθ/resolution` — 2% from the prediction.
  And cascade 0 disobeys **exactly** where the derivation says it should, its range being set by the
  casters rather than its sphere. An exception you can predict from the mechanism is worth more than
  a rule with no exceptions.

- **Anchor a quantisation basis somewhere other than the thing you are quantising.** Texel snapping
  did nothing at all, silently, because the light basis was built with
  `look_at(centre, centre + fwd, up)` — making `basis * point(centre)` exactly `(0,0,0)`, so there
  was nothing left to round. The harness caught it as "§E reports an identical 0.499 texels with
  snapping on and off", which is a far better error message than a picture that looks slightly
  crawly. **A no-op that compiles is why the measurement has to be numeric.**

- **Measure the case where the feature loses, and put it in the lesson.** On this course's own demo
  scene (2.4 m across, camera 9.8 m back) cascades come out *worse* than 6.8's single map — 0.02169
  against 0.0207 — because there is no far field to over-serve and the sphere fit charges 29%
  regardless. The 2.8× win only appears on a 40 m scene. Shipping both numbers is what stops a
  reader cargo-culting the technique.

- **A uniform slot has no null.** A `cbuffer` the shader declares and nobody pushes reads as
  whatever was last in that slot — not as zero. The cascade block is therefore pushed on every
  frame, with a deliberate identity fallback, even when shadows are off. The nullable-pointer
  bargain the rest of the engine makes does not survive the crossing to GPU state.

- **Radial distance is not axial depth, and the difference has a shape.** Splits are computed in
  view-space depth; a fragment knows a position. `length(eye − world)` overstates depth by
  1/cos(off-axis) — ~22% at the corner of a 60° frame — so selecting a cascade on it bends the seam
  into a curve that follows the frame edge. Sixteen bytes of `view_forward` keeps selection and
  fitting talking about the same quantity.

- **Making one case the degenerate case of another beats maintaining two.** `Texture2D` and
  `Texture2DArray` are different HLSL binding types, so supporting both means two shaders. Making
  the shadow map *always* an array — one layer for 6.8's single map — left exactly one code path.

- **The rebuild-and-diff discipline caught a real regression this session.** Retrofit edits made to
  a *rendered page* were silently reverted by the first rebuild, because the source fragment in
  `scratch/` still held the old text. Pins must be taken from the state you actually intend to
  ship, not from the lesson's original commit — the renumber had moved on since. See
  `state-md-is-append-and-merge` and the pipeline notes.

## Lesson 6.10 — Mipmaps, LOD, and anisotropic filtering

- **Decide the golden question before writing code, not after the diff.** Unlike every lesson since
  6.4, this one's reference render was *not* automatically safe: the fixture samples textures, so a
  mip chain built by default would have moved it. Two honest options existed — re-baseline and say
  so, or make the chain opt-in — and opt-in won on its own merits (33% memory, a 1×1 fallback has
  nothing to average). Had I written the code first, the choice would have been made by whichever
  was easier to retrofit.

- **A deferral repeated in a shipped public header is a debt with the student's name on it.**
  Mipmaps were promised six times: prose, two exercises, a `num_levels = 1` comment, and a doc
  comment in `texture.hpp`'s sampler struct. Two external reviewers found the gap **from the
  published outline alone**, without the code. Keep a list of what you are deferring, and check it
  against what you have shipped.

- **`rule()` takes a class, not a colour — and the failure is invisible.** Passing a hex string
  gives `class="#e05c5c"`, which matches no CSS rule, so the line renders as nothing at all. No
  error, no warning, and `check-page.js` cannot see a line that is not drawn. Two dashed markers
  were missing from a figure for three build cycles. **Only the visual pass catches this**; added a
  `cline()` helper so the mistake is not available.

- **Figure numbers must follow page order, and nothing checks it.** The numbers live in
  `build_NN.py`'s `FIGURES` dict, the order lives in the body fragments, and they drifted the
  moment a figure was authored before the section that shows it — producing `fig4.png` captioned
  "Figure 5". Read the captions in order, every time; it is a five-second check that no tool does.

- **`svgSpill` can be vertical.** A label past the bottom of the viewBox reports as `overPx` just
  like one past the right edge, and I spent a cycle shortening text that was already narrow enough.
  Check the y coordinate against `H` before shortening the string.

- **At 390 px the SVG scales but the text does not.** Font size is CSS px, not SVG units, so a long
  string that fits comfortably at 1280 px overflows at mobile width regardless of the viewBox.
  Split long captions into two lines rather than widening the canvas.

- **On a log axis, adjacent values collide however you draw them.** 62.46 and 64 are three pixels
  apart at this width, so a vertical marker at the measurement will always land on the "64" tick. A
  short leader out to clear space says the same thing and collides with nothing.

- **Two SDL sampler fields silently disable mipmapping, and they are the same shape:** a default
  that is correct for a one-level texture and wrong for a nine-level one. `max_lod = 0` clamps the
  entire chain away, and `max_anisotropy` is ignored without `enable_anisotropy`. Neither errors,
  because both are legal requests. When a feature "does nothing", suspect a clamp before a bug.

- **SDL's GPU validation is conditional on debug mode.** All three checks on
  `SDL_GenerateMipmapsForGPUTexture` — no pass in progress, `num_levels > 1`, usage carrying
  `SAMPLER|COLOR_TARGET` — live inside `if (device->debug_mode)`. On a release device the
  requirement is unchecked and the result undefined. Related: SDL's asserts key on `__OPTIMIZE__`
  rather than `NDEBUG`, so `-O2` alone switches them off.

## Lesson 6.11 — transparency

- **An explicit `destroy()` just before a scope ends is almost always a bug.** `verify_611` §H
  segfaulted on exit with every check passing. The device was declared first — so C++ was already
  going to destroy it *last*, which is exactly the order the shaders, meshes and samplers need.
  Calling `gpu.destroy()` at the end of the function ran the device's teardown in the *middle* of
  that sequence, and the shader destructors then released handles against freed memory. Deleting
  the cleanup fixed it. RAII's ordering guarantee is the feature; hand-written teardown is how you
  opt out of it without meaning to.

- **A refactor can keep a golden, but only if the float operations keep their ORDER.** `sample`
  and `sample_mipped` were both rewritten as wrappers over four-channel versions, and
  `average_2x2` grew a per-texel weight — and the reference render stayed byte-identical, because
  the colour channels are combined by the identical expressions in the identical sequence and the
  default weights are exactly `1.0f` over exactly `4.0f`, so `sum * (1/4)` is bit-for-bit
  `sum * 0.25f`. "Equivalent in effect" and "identical to the last bit" are different claims, and
  only the second one survives a byte comparison. Plan for the second when a golden is in play.

- **Do not test a symmetric-looking property at its symmetric point.** The check that `over` is
  not commutative was written at `a = 0.5`, where `src*a + dst*(1-a)` weights both operands
  equally and the two orderings coincide — so the test failed while the code was right. The most
  obvious value to pick was the one value at which the property is invisible.

- **`enable_blend` is the third member of a family.** Fill in every field of
  `SDL_GPUColorTargetBlendState` and leave the enable false, and the state is perfect, the picture
  is unchanged, and nothing errors. Same shape as 6.10's `max_lod = 0` and `max_anisotropy`
  without `enable_anisotropy`. Also note that the zero-initialised alpha factors are
  `SDL_GPU_BLENDFACTOR_INVALID`, which is not a synonym for "the same as the colour ones".

- **A harness section that creates a GPU device should not be followed by one that does not.**
  `verify_611`'s golden ran after the GPU section and inherited its initialised video subsystem and
  destroyed device. Moving the golden ahead of it costs nothing and removes a whole class of
  "the reference render moved" that has nothing to do with the reference render.

- **"Empty space" in a plot is not a measurement.** Two figures placed their annotations in
  regions that looked clear; `check-page.js` reported six texts sitting on a polyline. On a
  diagram whose curves cross most of the box there is very little genuinely empty interior — put
  the legend outside the plot and stop guessing.

- **A published assertion can legitimately go red.** `verify_67` asserted
  `sizeof(material_uniforms) == 32`, which was true of 6.7 and stopped being true when 6.11 spent a
  third register. The fix is to amend the assertion to what it was ever *about* (the flag is
  derived) and record the supersession in a comment — not to leave a harness failing, and not to
  pretend the earlier lesson was wrong.

## Lesson 6.12 — HDR and tonemapping

- **A monotonic relationship coming out backwards means the measurement is missing its target.**
  `probe_612.cpp`'s first version swept the eye through a plane and reported that *sharper*
  surfaces had *lower* specular peaks — roughness 0.05 peaking below roughness 0.35. The physics
  was not surprising; the sweep never passed through the mirror direction, and a sharp lobe is a
  few degrees wide. Evaluate at `reflect(−to_light, n)`. The same trap bites the demo: a
  flat-faced scene at roughness 0.15 reports **zero** pixels over the lid, which is how a renderer
  with a real clipping problem looks completely fine.

- **Keeping codes and light apart does not get easier because you are the one writing about it.**
  A doc comment in `hdr.hpp` claimed Reinhard needs an input of ~768 to reach code 255. The real
  answer is **224**: the 254.5/255 threshold is an *encoded* value, and inverting the sRGB
  transform first gives a linear 0.995545. I inverted in the wrong space — Module 6's recurring
  mistake, committed in the module about it, and caught only because the harness measured the
  number instead of repeating it. **Assert quantities you state in prose.**

- **"Decide the golden question before writing code" is now three for three**, and 6.12 produced
  the strongest form of the answer yet. 6.10 and 6.11 made their features opt-in *by default*;
  6.12 did not need to, because tonemapping is a stage over a **different buffer type** and the
  reference fixture has no such buffer. A structural reason beats a flag: nobody can turn it off by
  accident.

- **Hoisting can be free, and worth proving separately.** Moving a hundred lines of shading out of
  `fragment` into `shade_lit` was pure code motion, and running the previous lesson's harness
  immediately afterwards — before adding anything — is what made the later HDR work safe to do.
  Verify the no-op step on its own; a refactor and a feature landing together have no control.

- **Two files that must agree, with nothing checking them, will drift.** Figure numbers live in
  `build_NN.py` and figure order lives in the body fragments. 6.10 shipped `fig4.png` captioned
  "Figure 5"; 6.12's first build did the same thing with two figures. Added `figOrder` to
  `check-page.js`, which then found the identical defect in **three already-published lessons**
  (2.5, 3.10, 4.1). The lesson generalises past figures: when a fact is stated in two places, either
  derive one from the other or check them, because discipline is not a mechanism.

- **An explicit `destroy()` before a scope ends is still a bug.** 6.11's segfault taught this and
  6.12's harness was written with the fix already in place — declare the device first so it is
  destroyed last, and delete the hand-written teardown. Worth recording that the *second*
  application of a lesson is where it pays.

## From Lesson 6.13 (bloom and the post-processing stack)

- **A `POST_BUILD` command runs only when its own target is rebuilt — so it cannot be trusted to
  deploy a dependency.** `engine_use_shaders()` copied compiled shaders beside each executable with
  `add_custom_command(TARGET x POST_BUILD ...)`. Edit only a shader and the shader target
  recompiles, but no executable has any reason to relink, so the copy never fires and every program
  keeps the shader it was last linked beside. **The build reports success the whole time**, and the
  new HLSL really is compiled and sitting in `build/shaders/`. This had been live since Module 4 and
  hid because nobody had ever edited a shader without also touching C++ in the same build.
  The fix is the standard shape: a command whose `OUTPUT` is a stamp file and whose `DEPENDS` are
  the compiled shader **files**, wrapped in a target the executable depends on — so the copy runs
  *before* the executable is considered built. Note that the `add_dependencies` call already present
  ordered the **compile** and never the **deploy**; having one is not evidence of the other.

- **A GPU result that is constant across a parameter sweep means the parameter is not reaching the
  code.** That is what exposed the above: the bloom composited as exactly zero at intensities from
  0.05 to 50. Reading the bloom target back showed it held exactly the derived value, which
  relocated the fault from the chain to the composite — and the decisive diagnostic was that the
  *deployed* `.msl` was a different size from the canonical one. **When a GPU answer disagrees with
  everything else, check that the binary on disk is the one you think you compiled.**

- **`check-page.js`'s spill check tests text only.** A legend *box* ran 6 px off its viewBox and the
  page passed. Shapes drawn from computed coordinates can leave the frame exactly as labels can, and
  nothing is watching.

- **The automated page checks are necessary and not sufficient — look at every figure.** The chain
  diagram passed every check while its labels were crowded, its arrows did not meet its boxes, and
  one label sat on a corner. Crowding, dead space and arrows that fail to connect are invisible to a
  geometric test and obvious in a screenshot.

- **A caption that names another figure by number is a reference the `figOrder` check cannot see.**
  Renumbering left a caption referring to "figure 2's tail" *while being figure 2*. `figOrder`
  compares a figure's number to its position and is blind to prose. Grep for `figure [0-9]` after
  any renumber.

- **The index's hours cell must be an integer.** `check-curriculum.py`'s `ROW_RE` ends
  `[0-9]+\s*h`, so `4.5 h` makes the whole row invisible: the page reads as an orphan and the module
  comes up one lesson short. Also note nothing checks the index's hours against the lesson header's
  own estimate — align them by hand.

- **A null result on a test that cannot vary is not evidence.** The firefly probe compared a bright
  pixel at `x` and at `x+1`, which fall in the *same* 2×2 block, so the box downsample returned
  identical output and the test reported a confident 0.000%. Before believing a null, check the
  measurement is capable of a non-null.

- **Measure a tail in the units the data is stored in.** The same probe sampled radii 1 and 2 of a
  half-resolution buffer, both of which land on the delta's core rather than its tail, and reported
  a slope that was an artefact of measuring the spike.

- **A free hardware optimisation can foreclose an option you will want later.** One bilinear tap
  *is* a 2×2 box average — but the averaging happens inside the fetch, so the four texels are gone
  before the shader sees anything. Per-texel firefly weighting therefore becomes impossible, at a
  measured cost of 4× (1.19% drift against 0.30%). That, not kernel width, is the real reason
  shipping engines use Karis's 13-tap downsample.

- **A tuple whose elements are both strings will not tell you when you swap their meanings.**
  `LISTING_META`'s entries are `(status, status_word)` and `listing()` emits
  `<span class="tag {tag}">{word}</span>`. Putting the *language* in the second slot produced a
  correctly-classed pill reading "cpp", beside a `.lang` span already reading "C++" — so the caption
  said path / language / language instead of path / status / language. It ran from **6.9 to 6.12**
  before anyone looked at a pill, because nothing type-checks a pair of strings and
  `check-page.js`'s `unknownTagClasses` validates only the **class**, which was right the whole
  time. When a check validates one half of a pair, the other half is unguarded.

- **Scope a repeated defect by sweeping, not by extrapolating from where you noticed it.** This was
  filed as "two lessons" because that is where it was spotted. A grep over every page found **four**
  builders and 18 entries — and also 16 *deliberate* pills ("excerpt", "the type", "the resolve")
  that are hand-written in body fragments and must not be touched. The counts reconciling exactly
  (3+3+5+7 = 18 generated; the rest authored) is what made it safe to change one set and leave the
  other.

---

## From Lesson 6.14 (antialiasing)

- **A constant that refuses to vary is a derivation you have not done yet.** The probe bisected the
  GGX lobe's half-width and the ratio to α came out at 0.6436 at *every* roughness. Empirical
  constants do not do that. Ten minutes of algebra turned it into
  `sin t = α√((√2−1)/(1−α²))` — exact rather than a small-angle fit, and it showed that the familiar
  "the lobe is about α wide" overstates by 55%.

- **Check that a measurement can produce a non-null result before believing a null one.** This bit
  twice in one lesson, both times as a statistic blind to its own subject:
  - the supersampling test took the **mean over the whole image** and found it already correct at
    1×, because errors of opposite sign cancel across pixels whose phases differ. Aliasing is
    per-pixel, so the statistic has to be.
  - the specular-AA test averaged each method over 64 sub-pixel phases and compared the means — but
    **averaging over phases *is* antialiasing**, so it flattered the single sample to a 4.6%
    "error" at roughness 0.05, where the honest per-phase figure is 230%.

- **"Unchanged" is not "reproduced" when the thing under test can fail without writing.** An audit
  of every page builder reported 4 failures; the real number was 27, because eleven of them *crash*
  before writing and the check only compared the file before and after. **Check the exit status as
  well as the diff.** (This is the same shape as the two items above: a check that cannot observe
  the failure mode it was written for.)

- **A note in LEARNINGS is not a fix.** 6.13 discovered that `check-page.js`'s spill test only
  examined `<text>`, so a legend *box* ran off a viewBox and passed. That was written down here —
  and the very next lesson shipped the identical defect in its own figure. Writing the check took
  five minutes and it caught the live defect within a minute of existing. **When a note describes a
  gap in a tool, the note is the interim measure; closing the gap is the fix.**

- **Prefer making an error impossible to detecting it.** Figure 7's boxes were hand-placed at
  arithmetic offsets and the last one overflowed. Computing the layout from the canvas width removed
  the failure mode rather than catching it.

- **A design is only tested when something it was not written for arrives.** 6.13's ownership rule —
  the stack owns what crosses between stages, a pass owns its own intermediates — placed MSAA's
  multisample target without amendment, and that is worth more evidence than the argument that
  produced it.

- **Check the capability, do not assume it.** `SDL_GPUTextureSupportsSampleCount` exists because
  MSAA support is per *format*: on this machine 8× works on neither the float nor the 8-bit target,
  while 2× and 4× work on both. Falling back with a warning is better behaviour for a quality
  setting than failing to start.

- **A free hardware optimisation can be the thing that makes a feature impossible.** MSAA's
  affordability comes from shading once per primitive per pixel — which is exactly why no sample
  count reaches shading aliasing. The same shape as 6.13's one-tap downsample foreclosing per-texel
  firefly weighting. **Ask what an optimisation removes access to, not just what it saves.**

## Lesson 6.15 — environment lighting (2026-09-12)

### A channel-order bug whose symptom is a plausible picture

`make_brdf_lut` packed its two coefficients with a hand-written shift — `(bias << 8) | scale` —
reasoning about an RGBA word. **`engine::texture` stores ARGB**, so red is bits 16–23 and `scale`
went into blue.

What made it expensive is that green is bits 8–15 in *both* layouts, so `bias` read back correctly.
The shader computes `F0 * scale + bias`; with `scale` reading zero, the specular term became `bias`
alone — ~0 head-on and ~0.078 at grazing. So IBL was not *broken*, it was **dim in a way that
invited tuning**. The same mistake then appeared a second time in the harness's ARGB→RGBA unpack.

**A packing spelled out at a call site is a packing that can disagree with the engine's.**
`colour.hpp::pack_argb` has had the right answer since Lesson 1.6. The five-second diagnostic:
`scale` at (n·v = 0.9, roughness = 0.1) must be exactly 1.000.

### 6.14's rule about null results has a mirror

6.14: *check a measurement CAN produce a non-null result before believing a null one.*
6.15: **check a non-null result has CONVERGED before believing it.**

The first draft of the split-sum measurement reported 20% error at roughness 0.25. It was Monte
Carlo noise: 8,192 importance samples against a 6000:1 sun disc disagree with 1,000,000 by a factor
of **1.98**, and the under-sampled answer is a number of the right order with nothing wrong-looking
about it. With the sun removed the same two counts agree to 0.040% — **the sample count was never
the problem, the dynamic range was.** Both rules are one rule: establish what your instrument can
see before you read it.

### A null measurement from an environment that cannot exhibit the defect

The seam test read 0.0000% twice, for two different reasons.

1. The probe directions never crossed a face boundary. *Fix: assert the crossing first* — the
   harness now reports "31 of 31 probe pairs land on different faces" as its own check.
2. They did cross, and it was **still** zero, because `sky_radiance` depends only on `d.y`: along a
   vertical face edge the sky is literally constant, so the two clamped edge texels hold the same
   value. Measured on an environment that varies with *azimuth*, the seam is **12.09%** at 16×16.

The general form is worth more than the seam: **a null result from a fixture that cannot show the
defect is not evidence the defect is absent** — it is a measurement of the fixture.

### A latent build bug that only a NEW shader could find

`engine_use_shaders` made the copy-stamp depend on shader *files* produced in the top-level
directory, while the stamp target lives in `demos/`. The Makefile generator needs a target-level
edge too, so adding `skybox.vert` gave

    No rule to make target `shaders/skybox.frag.json', needed by `demos/sandbox_shaders.stamp'

**Nobody hit it for fourteen lessons because once a shader has been built once, the file exists —
and `make` will happily depend on an existing file it has no rule for.** Every shader added before
this one worked from its second build onward. A genuinely clean tree would have failed on all
twenty-three at once. Fixed with one `add_dependencies(${target}_shaders ${shader_targets})`.

### check-page.js gained a rect test, and it caught two of this lesson's own figures

The text-on-shape check selected `line, polyline, path` — so an **annotation box laid across a
label** was invisible to every check on the page. 6.15's figures 1, 5 and 6 all shipped first drafts
with exactly that, and only a screenshot caught the first one.

Two design points in the new check:

- **It tests "crosses", not "contains".** A legend box is *supposed* to have text inside it.
  Sampling the four edges gives that distinction for free.
- **It is restricted to `rect[fill="none"]`, and the restriction is measured.** Over every rect it
  fires 26 times across 12 published pages, and most are filled cells with a label deliberately
  annotating them — a pixel grid (2.1), a memory layout (1.2, 5.7), an NDC corner (4.4). That is the
  diagram working. Hollow boxes only: **9 hits across 4 pages**, and those four look real.

### Known finding, not fixed here

The narrowed check reports hollow-box/label crossings in four published pages:
`00-04-cmake-from-zero` (fig 2, ×2), `00-05-first-window` (fig 2), `01-02-input-state-vs-events`
(fig 3, ×2) and `02-01-lines` (figs 3 and 6, ×4). Left alone deliberately — fixing them changes
published visuals and rebuilds four pages whose reproducibility was only just stabilised, which is
the same call made on 2026-09-12 for the two pedagogical defects above.

---

## A sweep along an axis the effect cannot depend on looks exactly like a measurement

*Lesson 6.16.* The first version of the culling benchmark swept the **object count** — 4, 16, 64,
256, 1024, 4096 — and produced a table, a trend and a verdict column reading `CULL WINS` six times.
It contained no information. Culling is O(n) and the work it skips is O(n), so their ratio is
independent of n and no amount of sweeping n can cross it.

The crossover was in the **cull rate**, and once that was named it could be *predicted* rather than
hunted: culling costs `c` per object always and saves `w` per object rejected, so break-even is
`c/w`. Measured `c = 58.3 ns` and `w = 23,821 ns`, predicted **0.2446%**, then found it bracketed
between the 0% and 0.4% samples.

**Before sweeping a parameter, ask what would have to be true for the answer to depend on it.**
This is the third member of a family this codebase keeps rediscovering:

| Lesson | Rule |
|---|---|
| 6.14 | Check a measurement *can* produce a non-null result before believing a null one |
| 6.15 | Check a non-null result has *converged* before believing it |
| 6.16 | Check the *axis* can show the effect before sweeping it |

Underneath all three: **establish what your instrument can see before you read it.**

## Verify against facts, and then check the facts are sufficient

The frustum extraction is checkable without a reference implementation, because a frustum has
properties that follow from what it *is*: the eye is the apex of the pyramid, so its distance to all
four side planes is exactly zero. Four independent zeros, and they came out as `0.000000`.

**And they were not enough.** A sign error that *reverses* a plane's normal leaves the apex on the
plane — zero is zero either way — so all four checks pass while the culler rejects the world. That
is exactly the bug the first draft made. The checks that catch it are `d(eye, near) == −near` and
"opposite planes are not exact negations of one another".

*A test that would pass on the bug you actually made is not a test.*

## Rule out the obvious cause before believing it

The far plane came out 1.4 × 10⁻³ from where the arithmetic says it should be. The obvious diagnosis
was catastrophic cancellation in `row3 − row2`, and the rows really do agree to three digits — case
apparently closed.

Redoing the same subtraction in `double`, from the same `float` matrix, reproduced the `float`
answer to seven digits. That *rules the subtraction out*. The error is upstream in `perspective()`,
where forming `A + 1` with `A = −1.003009` discards eight bits and amplifies A's last ulp by **332×**.

A plausible mechanism that is present is not the same as the mechanism responsible. The cheap
discriminating experiment — redo the suspect step in higher precision — took four lines.

## A limit that has never been reached cannot tell you it is wrong

`pipeline_desc` held eight vertex attributes and, past the eighth, did `return *this` — dropping
them **silently**. Nothing in Modules 4 or 5, or eight lessons of Module 6, had ever declared a
ninth, so the cap had never fired. Lesson 6.16's instanced pipeline wants eleven.

What caught it was Lesson 4.5's `check_layout`, which compares the declared attributes against the
shader's *reflected* inputs — an independent reading of what the pipeline actually declares. This is
the sibling of 6.15's finding that **a build only ever run incrementally cannot tell you it is
wrong**; that one is now acted on too, by running a clean-tree build before shipping.

## Two C++ traps this codebase had avoided by luck

- **`aabb::expand({1, 2, 3})` is ambiguous.** Brace elision makes the braced list a candidate for
  both `expand(vec3)` and `expand(const aabb&)` — an `aabb` whose first member is initialised from
  three floats. Every existing call site happened to pass a named variable. Write `expand(vec3{…})`.
- **`std::vector<int> v(std::size_t(n));` declares a function.** The most vexing parse:
  `std::size_t(n)` reads as a parameter named `n`. The error surfaces forty lines later at the first
  use, as an impossible conversion. Use `static_cast<std::size_t>(n)`.

## A test that can pass without testing anything is worse than no test

`golden_615.cpp` compared two files as strings and printed `identical=YES` when **both** reads
failed — two empty strings are equal. It was run from the correct directory every time, so it never
fired. `golden_616.cpp` reports both sizes and a differing-byte count, and fails loudly on an empty
read.

## A measurement that only works once is worse than one that never works

The frame graph's pool reported how many bytes descriptor-keyed reuse saved. The first
implementation summed a texture's bytes each time one was **created** — correct on the first frame,
and a reported saving of 100% on every frame after it, because the pool is then warm and creates
nothing.

The first run looks right, which is precisely what makes it dangerous: a measurement that is wrong
from the start gets investigated. This one only fails on the second call, in a number nobody
re-reads. It was caught because §G of `verify_617` happened to run after three earlier sections had
already warmed the pool, and printed an implausible `saved 1223296 B`.

Count over the **slots the frame used**, not over the work this call happened to do. The general
form: if a statistic is derived from a side effect (an allocation, a cache miss, a file write),
it measures the side effect's *novelty*, not the quantity you meant.

## Before building the thing, check that the reason applies here

Frame graphs are sold on memory aliasing. Measured on this engine's own fourteen-pass frame, the
saving is **zero** — and for two reasons that had to be separated, because they have different
futures:

- SDL_GPU 3.4.12 has no placed resource, no heap and no aliasing flag, so the strongest reuse
  available is handing back a whole texture whose descriptor matches *exactly*. That limit goes away
  if SDL_GPU ever grows placed resources.
- The frame is a **chain**: peak live bytes are 85.7% of the sum, because `hdr` spans the whole
  schedule and all six pyramid levels are alive at the turn. That limit goes away the moment a
  second post effect lands.

Collapsing the two into "aliasing does not help here" would have been wrong in both directions. The
graph was still worth building, for reasons that had to be found rather than assumed — the order,
the load ops, the store ops and the pass culling all stopped being facts a human maintains.

This is now the fifth member of a family: check a measurement **can** produce a non-null result
(6.14), check a non-null result has **converged** (6.15), check the swept axis **can show** the
effect (6.16), check a comparison **can report a difference** (6.17 §I) — and check that the
**reason you are building this is true here**.

## Does the denominator move when the code does?

The obvious way to judge the glyph packer was "what fraction of the atlas is glyphs": 7,145 texels
over 128 × 128 = **43.6%**. That number is not about the packer. The denominator is a power of two
decided *before* the packer runs, by the observation that 7,145 will not fit in 64 × 64; the
numerator is the total area of the glyphs plus their padding, a property of the font at that size.
A perfect packer and a hopeless one both score 43.6%.

The honest quantity is glyph area over the rows the packer **actually touched** — 7,145 over
128 × 67 = **83.3%** — and now both halves move when the algorithm does. Put every glyph on its own
shelf and the denominator explodes.

Sixth member of the instrument family: can this measurement produce a non-null result (6.14); has a
non-null result converged (6.15); can the swept axis show the effect (6.16); can this comparison
report a difference (6.17 §I); is the reason I am building this true here (6.17 §G) — and now,
**does my denominator move when the code does?**

## A check whose degenerate case is a pass is not a check

`check(efficiency > 80.0, ...)` shipped in a first draft against a stale library where the packer's
`used_height` was still zero. The division produced `inf`, and `inf > 80.0` is **true**. The harness
printed `shelf efficiency = inf%` and reported a PASS.

This is the second time in three lessons. Lesson 6.16 found `golden_615.cpp` printing
`identical=YES` when **both** of its file reads failed, because two empty strings compare equal.
Both times the only tell was an implausible printed **number**, not the verdict.

Two habits follow. Assert the *inputs* are plausible before deriving anything from them —
`used_height > 0 && used_height <= height` costs one line and cannot pass on a degenerate state.
And print the inputs beside the verdict, always: a harness that prints only PASS/FAIL has thrown
away the only evidence that would have caught either of these.

## Two bugs in two files can be the same function

Compositing a glyph by lerping sRGB codes gives 62.2% of the correct ink on a dark background.
Uploading the coverage atlas as an `_SRGB` texture — a *texture format* mistake, in a different
file, made by different code on a different day — gives **62.1%**. They agree to four significant
figures, and it is not a coincidence:

    lerping codes:   code = 255a, so light = srgb_to_linear(a)
    _SRGB atlas:     a' = srgb_to_linear(a), and the linear blend emits a'

The same function, applied at two points in the pipeline. So the obvious measurement cannot tell
them apart, and "I checked the blend space and it was fine" is not evidence that the atlas is.

When two independent explanations predict the same measurement, do not pick one — **find the
measurement that separates them**. Here it took one line: render the text black-on-white as well.
A blend-space bug is directional and *fattens* dark-on-light (137.8%); a coverage bug corrupts the
input to the lerp and thins it either way (62.1%).

## Make a bug impossible rather than catchable

Lesson 6.15 shipped an ARGB/RGBA byte-order bug in a BRDF lookup table, which survived review
because green occupies bits 8–15 in both layouts, so half the data read correctly and the result
was merely *dim*. Lesson 6.18 had the identical hazard: `SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM`
hands the shader four bytes in **memory order**, so a packed ARGB `Uint32` arrives as (B, G, R, A)
on a little-endian machine.

The fix was not a test. `overlay_vertex` names its four colour bytes `r, g, b, a`, and
`set_colour(Uint32 argb)` is the single place the packed convention is unpacked. There is nothing
left to get wrong, so there is nothing to check.

Prefer this whenever the type system can carry it. A test proves the bug is absent today; a type
proves it is unrepresentable.

## Return the quantity a caller would need to judge you

`shelf_pack` knew exactly how many rows it had used and threw the number away, so the honest
occupancy figure could not be computed from outside — which is *why* the misleading one got
written. Four lines of plumbing (`used_height`, out-parameter, field, log line) turned a
measurement that meant nothing into one that means something.

The general form: if a routine computes a quantity a caller would need in order to judge the
routine, returning it is not instrumentation, it is part of the interface.

## A tool that relocates a claim has not verified it

The frame graph derives four facts — the order, the load op, the producer's store op, the lifetime
— from one word per pass. Lesson 6.18 was its first user from outside, and declared the overlay
`keep`, which is correct: an alpha blend reads its destination.

Declare the same pass `discard_write` instead and the graph compiles it happily, derives
`DONT_CARE`, and erases the entire frame under the text. The graph cannot know the claim is false.

What it *does* buy is that the claim moved: a load op buried in a struct three files away became a
sentence about arithmetic at the call site, which a pass author can answer without knowing what a
render pass is. That is a real improvement and it is a different one from correctness. Say which
you have; a tool oversold is a tool trusted in the one case it does not cover.

## Figure numbers follow page order, and a moved label lands on the next obstacle

Two authoring lessons from 6.18's diagrams, both caught by `check-page.js` and neither by reading:

- The figures were numbered as they were **written** — the architecture diagram was figure 7 and
  appeared first — and the `figOrder` check reported all seven. A reader counts figures as they
  meet them.
- An annotation moved off a dashed leader landed on the x-axis tick row; moved off that, it landed
  on the plotted curve. Three placements before it reached empty space. `check-page.js` has a
  separate check for text-on-text and for text-on-shape, which is the only reason each move was
  caught rather than one hiding behind the other. Budget for the iterations.

## A rule inherited from a comment outlives the lesson that wrote it — the partial-bind "rule"

6.8's `gpu_scene.cpp` said a partial `SDL_BindGPUFragmentSamplers` "REPLACES the range it names",
so slot 2 "would be unbound the moment slot 0 changed". 6.15 repeated it as "the rule Lesson 6.8
discovered", STATE carried it, and Exercise 6.8.4 was built on it ("the obvious optimisation is a
bug"). SDL `release-3.4.12` disagrees: all three backends store bindings per slot and write only
`firstSlot + i`. What does reset every binding is **the end of a render pass** (`SDL_zeroa` on
each backend's arrays), with the debug layer asserting `Missing fragment sampler binding!` at draw
time — which is probably the real failure the comment was generalising from. Binding all slots in
one call remains correct; only the reason was wrong. Found by writing the exercise's solution
against the library's source (`build/_deps/sdl3-src`), not against the comment.


## Lesson 6.17b — local lights and their shadows (2026-09-28)

Written after 8.13 and inserted between 6.17 and 6.18. Every entry below was found by a
measurement that disagreed with a claim, and several by an instrument that was itself wrong first.

- **The inverse square, the window and the cone, as the engine defines them.** `intensity` is the
  irradiance at one metre, so a lamp of intensity π at 1 m renders white at exactly 1.0 — the sun,
  to the bit. Flux through spheres of 0.5, 2 and 8 m: 39.47852 each against 4π² = 39.47842
  (midpoint-rule error, identical at every radius because the d² cancels). Frostbite's 1 cm floor,
  not Karis's 1/(d²+1), which is 0.5 of the true value at 1 m. The squared window
  (1−x⁴)² is 0.879 at half the range, NOT "0.99" — a header comment said 0.99 and the harness
  refused it before it shipped. glTF's unsquared recipe meets the range at −4/r³, twice the inverse
  square's slope, then 0: a crease. The cone ramps in cosine: 0.2999 at the angular midpoint.

- **A formula derived for one camera carries that camera in its assumptions.** 6.8's slope-scaled
  bias used tan θ and a single `depth_range` — both exact for an orthographic map and both wrong
  under perspective. The device-depth conversion must go through the curve
  (depth(w) − depth(w − b)); 6.8's constant is right only at w = n·f/1 m (0.40 m for 0.05/8) and
  20× too large at 8 m. And the slope of AXIAL depth is sin α cos φ / cos θ (α normal-vs-axis,
  φ ray-vs-axis, θ normal-vs-ray), of which tan θ is the φ = 0 case. It was found as 19 CPU/GPU
  disagreements far from any shadow edge, on ground the spot lit obliquely; the corrected slope
  cleared 16.

- **A derived quantity that is exactly zero exposes the rounding under it.** The floor under a
  point light's −Y face has constant axial depth, so the derived slope is exactly 0 — correctly —
  and the stored and recomputed depths then disagree by one ULP about half the time: 90 points of
  acne from nothing. A 2⁻²² floor (four float spacings near 1) fixed it; "add an epsilon" would
  have been the same fix without the reason for its size.

- **One number, two reasons: say both, share neither.** The GPU needs a reach of (r+1)√2 because a
  LINEAR comparison sampler reads four texels (11,098 off-edge disagreements → 0). The CPU needs
  (r+½)√2 + ½√2 = the same number, because the rasterizer snaps vertices to whole pixels and slides
  a guard-clipped plane by up to half a texel (measured 0.36 of a texel's depth step; 713 → 0 on a
  bare floor, 1,895 → 0 in gltf_view). Each comment names its own reason; a shared constant would
  be wrong for one side the day either filter changes. The SUN's GPU path still uses the CPU reach
  through a linear sampler — a quarter short at r = 1, unmeasured, recorded in STATE decisions
  `found-by-617b`.

- **Finite is not small: a clamp that moves geometry.** Near clipping bounds x/w; it does not keep
  it small. A 5 cm lamp near plane over an 8 m floor projects clipped corners ~40,000 px out, and
  `to_pixel`'s ±8,000 clamp (written for `near_mode::none`) MOVED them — the map read 0.950 where
  the floor was 0.987, acne on 88% of it, which no bias could touch. Guard-band clipping at ±7,936
  px, only when a polygon leaves the band, left the golden byte-identical: no frame in Modules 3–6
  had ever reached it. A latent bug is invisible until some caller's parameters leave the range
  every earlier caller happened to stay in.

- **A cube face's camera is a mirror.** The face table (u, v grows DOWN, major) is left-handed, so
  the camera with rows (u, −v, −major) has determinant −1. `look_at` can only build rotations; with
  any up vector it derives right = −u and every face is mirrored (0.970 of a face at worst) — which
  renders a plausible picture with shadows on the wrong side. Test a face camera by pushing
  directions through it and through `direction_to_cube` and requiring the same texel (1.8e-7).
  The mirror also reverses winding: a shadow pass that culls must flip it for cube faces.

- **Clamp, don't reject, at a face you chose.** 6.8 rejects outside [−1, 1] as "lit"; for a cube
  face chosen by `direction_to_cube` the direction belongs to the face even when rounding puts it a
  hair past −1 on the 45° seams — 54 lit points through a crate's shadow until the rule changed.

- **Peter-panning needs a thin caster, and the prediction was refused twice.** 6.8's constant is
  too large far from the lamp, so it "should" detach shadows. A half-metre crate: no leak (the
  floor behind lies half a metre beyond the lit face). A 3 cm wall: no leak (1.3 cm of overlap,
  inside the judge's ambiguous band). A 4 mm sign: 50 leaks. The over-bias is only visible through
  casters thinner than it — leaves, cloth, fences — so test biases with one.

- **The judge is an instrument, and it was wrong four times.** A ray-cast ground truth that shares
  no code with the engine still has to decide which points are too close to a shadow edge to
  judge. 6 cm on the floor, 1.5 texels on the floor, 2 texels on the floor (failed on a ray that
  grazed the box by a fifth of a texel three metres up), 2 texels of angle tested only at the ends
  (stepped over the half-texel sign). The shipped judge tilts the ray by every quarter texel out to
  two, in the LIGHT's angles — the coordinates the map actually resolves. Each wrong judge was
  caught by probing one flagged point until it explained itself, not by fixing the engine until
  the count went to zero.

- **A light list sized by the scene is a storage buffer, and its count is the binder's.** SDL numbers
  resources per kind on the C++ side (storage slot 0) while HLSL puts sampled textures, storage
  textures and storage buffers in one `t` sequence in space2 (so t8 after eight samplers; MSL
  `[[buffer(3)]]`). A declared buffer must be bound even when the loop runs zero times ("Missing
  fragment storage buffer binding"), so the renderer owns a one-record zero buffer; and the renderer,
  not the caller, writes the count, so length and contents cannot disagree. Every member a
  `float4`, matrices as rows.

- **"The platform tolerates it" is a claim, and the validation layer is the instrument.** 6.17b's
  gpu_scene.cpp said Metal tolerated 6.8's 2D fallback in a `Texture2DArray` slot, because it
  rendered correctly. `MTL_DEBUG_LAYER=1` reported it on every draw that used it ("incorrect type
  of texture (MTLTextureType2D) … expect MTLTextureType2DArray"). Correct output is not the same as
  a correct binding; run the GPU harnesses under the validation layer before writing either. The
  same run found a sibling: the frame graph pools a one-layer depth texture as plain 2D.

- **A difference in a figure can be real — check before captioning either way.** The demo's spot
  pool came out bluer with shadows on than off. It looked like the quantiser flipping a borderline
  colour; the renders said otherwise (R and G down four codes, B unchanged): the bulb's shadow of
  the slab falls across the spot's pool and removes its warm light. The first draft of the caption
  said "the pools are identical".
