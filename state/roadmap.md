# Roadmap — the 2026-09-08 reshape and what follows

Moved verbatim from STATE.md's `roadmap:` block on 2026-09-27, when that file became
a compact resume key (CLAUDE.md §9). Append here; keep STATE.md's headline in step.

```text
roadmap: RESHAPED 2026-09-08, AFTER TWO EXTERNAL REVIEWS OF THE PUBLISHED OUTLINE.
        9 modules / 95 lessons / 444 h  ->  10 MODULES / 107 LESSONS / ~510 H.
        The course was sitting on the exact ceiling of CLAUDE.md §5's old band
        (75-95), which is why nothing had been added; the band is now 100-115
        lessons / 450-550 h, amended in §5 with a note.
        THE THREE GAPS BOTH REVIEWERS FOUND INDEPENDENTLY, from the index alone:
          MIPMAPS. Not an omission — A PROMISE ON RECORD, made six times: 3.9's
            prose and its Exercise 8.5, 4.7's exercise 4.7.5, and a doc comment
            SHIPPED IN texture.hpp's public sampler struct, all saying
            "Module 6". Module 6 had no such lesson. Now 6.10, and it must
            precede 6.15 because A PREFILTERED ENVIRONMENT MAP IS A MIP CHAIN.
          TRANSPARENCY. Worse than a curriculum gap: gpu_pipeline.hpp still says
            "no blending", and 6.6's glTF importer IGNORES alphaMode, so every
            downloaded asset with foliage or glass renders as opaque cardboard
            TODAY. Now 6.11.
          ANTIALIASING. Genuinely absent. Now 6.14.
        MODULE 6 RENUMBERED 6.10-6.15 -> 6.12,6.13,6.15,6.16,6.17,6.18.
        6.9 (CASCADED SHADOW MAPS) DELIBERATELY DID NOT MOVE: 6.8 names it six
        times in prose plus both nav labels and calls it "an extension of this
        file". Keeping it saved eight edits and a narrative thread — and it is
        why `next:` below is unchanged.
        6.14 "Instancing and a Frame Graph" WAS ALREADY HALF-REDUNDANT: instancing
        is BUILT (instance-rate input, instance_buffer binding,
        draw(pass, instances) — gpu_mesh.cpp:218, 180 mentions in 4.5). Split:
        6.16 = frustum culling FEEDING instanced draws, 6.17 = frame graph alone.
        5.12 ADDED — a checkpoint game against the public API. The boundary was
        declared law in 5.1 and has never been used from outside by anyone but
        the person who drew it; 9.11 would have been its first real test, at
        hour 434 with 10 h of budget to absorb whatever it found.
        PHYSICS EXPANDED, NOT LIMITED (the user's explicit call), so it outgrew
        Module 7 and BECAME MODULE 8; Professional Polish & Capstone -> MODULE 9.
          Module 7 = Rotation, Animation, Audio (8 lessons, 38 h). Audio 7.13 -> 7.8.
          Module 8 = Physics (13 lessons, 68 h): integrators, linear bodies,
            ANGULAR DYNAMICS + INERTIA TENSOR, SAT, GJK, EPA, CONTACT MANIFOLDS
            WITH PERSISTENCE, broadphase, impulse response, SEQUENTIAL IMPULSES
            WITH WARM STARTING + ISLANDS + SLEEPING, JOINTS, RAGDOLLS, and the
            character controller SPLIT OUT as its own lesson (it is not a rigid
            body, and that is the lesson).
          8.10 REDEEMS THE PROMISE AT :2077 — "one impact per step for now
            (Module 8 iterates until the budget is spent)". It took its own module.
          mat3 NEEDS operator+/-, scalar multiply and an outer product before an
            inertia tensor can be written; inverse/transpose/multiply already
            exist. quat.hpp does not exist yet — 7.4 builds it, and every
            angular lesson depends on that having landed.
        THE RENUMBER COST, PAID IN FULL: 177 "Module 8" occurrences across 65
        files, 39 of them PUBLISHED lesson pages. Safe only because the mapping
        was TOTAL — every "Module 8" in the corpus meant the polish module.
        THE BARE `8.N` FORM WAS NOT SWEPT AND MUST NEVER BE: of 58 candidates
        only 4 were lesson references; the rest are intra-page section headings
        (<h3>8.2), exercise numbers (Exercise 4.8.3) and measurements (8.3 MB,
        8.4e-8). A blind sed would have corrupted ~54 sites.
        NEW TOOL: docs/_template/check-curriculum.py. The index had drifted FIVE
        WAYS AT ONCE (95 vs 94 lessons, 436 vs 438 vs 444 hours, Modules 3/4/6
        subtotals wrong, Module 5 badged in-progress with all 11 published) and
        NOTHING HAD EVER CHECKED IT. Now checked: subtotals, headline totals,
        badges, file existence, ordering, nav chains, internal links. It found
        three dead prerequisite links in 6.8 and a stale `next` in 5.1 on its
        first run. §11's pre-flight now requires it green.

        TWO RETROFITS TO PUBLISHED LESSONS, made the same day as the reshape.
          6.6 §10 — THE alphaMode GAP, which was the importer's ONE SILENT ONE.
            gltf_material_desc carries `alpha` (baseColorFactor[3], consumed by
            nothing) and does NOT read alphaMode or alphaCutoff, so MASK and
            BLEND import as fully opaque. Every other limit in that importer
            reports — too_many_vertices and unsupported_primitive fire a status,
            the factor-x-texture conflict logs AND returns an assertable count —
            and this one fires nothing, which breaks §10's own stated rule.
            THE REASON IS NOT AN OVERSIGHT: a status says "the file wants what
            the engine cannot do", and the engine had no blend state to compare
            against (pipeline_desc has carried `no blending` since 4.4). It
            cannot report a conflict with a concept that does not exist yet.
            Named now, fixed in 6.11. Cutout foliage and glass are the common
            cases and both come back as cardboard.
            THREE COPIES MOVED TOGETHER: engine/include/engine/gfx/gltf.hpp, its
            PIN at scratch/l66_gltf.hpp (build_66.py splices the pin, not the
            live header), and the already-rendered listing inside the page.
            Editing only the header would have left the page disagreeing with
            the repo, invisibly.
          4.8 §3 — WHAT THE 87% DID NOT TEST. The figure STANDS for what it
            measured (geometry, winding, depth, perspective-correct interp all
            genuinely agreed, and a convention error would have collapsed it);
            what it cannot support is "the GPU path is correct". The harness
            BUILT ITS OWN _SRGB RENDER TARGET and the shipped demo did not, so
            it compared against a configuration the program never ran — 6.1
            measures the cost at 2.3x too dark at mid grey, 13x in shadow.
            THE TRANSFERABLE LESSON IS NOT ABOUT COLOUR: a harness that
            constructs its own configuration tests THAT configuration. This one
            was written to isolate the rasterizers from presentation, which is a
            reasonable instinct, and the isolation removed the exact surface the
            bug lived on. The recap's "the two renderers agree" now carries the
            qualifier too, because a skimmer reads the recap and stops.


  8.5 — GJK: CONVEX DISTANCE FROM A SUPPORT FUNCTION.  Nine measured sections,
        33 checks, 0 failures. 12 figures. The lesson replaces 8.4's enumeration
        with a search, and the five things worth carrying are all things the
        measurement changed rather than confirmed.

        THE SIMPLEX SOLVER WAS REWRITTEN BECAUSE OF A MEASUREMENT, and this is
          the finding to remember. The FIRST DRAFT SHIPPED ERICSON'S
          `ClosestPtPointTriangle` — the seven-Voronoi-region version every
          implementation copies. Its three region tests are SIGNED AREAS,
          differences of nearly equal products, and on a thin triangle they are
          noise: the region it selects can be the wrong one, and the point it
          then returns is the foot of the perpendicular on the triangle's PLANE,
          which can be arbitrarily NEARER the origin than the triangle is.
          Measured: 3 failures in 200,000 FAT triangles and 1,048 in 200,000
          SLIVERS, worst case 14.5% wrong.
          AND A SLIVER IS NOT AN EDGE CASE IN GJK. The algorithm's whole job is
          to drive the simplex onto the closest feature, so by the last two
          iterations of EVERY query the triangle is as thin as the problem allows
          and the origin is a fraction of a millimetre from it. The failure mode
          lives exactly where the algorithm converges, which is why it survives
          any test suite written against uniform random data — including, for a
          while, this one, whose first thin population was not thin enough and
          did not place the origin near the plane.
          SHIPPED INSTEAD: four candidates (the interior projection, if its
          barycentrics are non-negative, plus three CLAMPED edges, which already
          contain their own endpoints) evaluated and compared. 21.681 ns per
          triangle against a 115 ns query. Refusing to decide costs a few percent
          and buys an answer that cannot be wrong.

        AN INVARIANT YOU CAN CHECK IS WORTH MORE THAN ONE YOU BELIEVE. |v| is
          monotonically non-increasing across GJK iterations BY CONSTRUCTION: the
          new simplex contains the old reduced one, and the nearest point of a
          superset cannot be farther. Two lines test it, and they caught TWO
          UNRELATED FAILURES:
            (a) THE LOOP CYCLING, on ordinary input. Two 3 m boxes 2 mm apart
                reached the 64-iteration cap with the distance ALREADY CORRECT to
                six digits.
            (b) THE SOLVER GOING BACKWARDS — which is how (the sliver bug) was
                found. A sphere against a turned box: 18 iterations converging
                cleanly to 0.0233827 m against a true 0.0233828, then ONE
                reduction returning 0.0674522, three times too far.
          The block RESTORES the last good state rather than merely breaking,
          which is what turns a terminator into a repair.

        AND A PROOF OUTRANKS A DECISION. A single positive dot(v, w) is a
          COMPLETE proof of separation — w minimises dot(x, v) over the whole
          difference set, so if that minimum is positive no point of the set
          reaches the origin's plane. The loop banks the best such bound and
          REFUSES any later containment that contradicts it. Measured on the case
          that motivated it: a sphere and a turned box 55 mm apart, 14 clean
          iterations to 0.0553297 m, and then a tetrahedron whose four face tests
          ALL reported containment — turning a 55 mm gap into a contact. A proof
          does not become false because a later determinant changed sign.

        THE SHARED UP AXIS COLLAPSES THE SEARCH BY A DIMENSION, and this one is
          8.6'S PROBLEM. Two boxes standing on the same floor have equal y half
          extents multiplying the same world direction, and `support_local`'s
          `>= 0` tie-break picks the same sign for both, so EVERY vertex of the
          difference comes out with w.y = (+h) − (+h) − 0 = EXACTLY zero. All
          100,000 simplices lay in that plane. A search confined to a plane cannot
          build a tetrahedron: 0 of 100,000, including all 65,633 pairs
          overlapping by more than 10 cm — against 77,326 once one box is tilted a
          SINGLE DEGREE.
          EVERY DESCRIPTION OF EPA BEGINS "start from the tetrahedron GJK
          terminated with". On the commonest arrangement in a game there is none,
          and 8.6's FIRST job is to build one. This is 8.4 §F's finding from the
          other side: there a shared axis made one cross product degenerate; here
          it removes a dimension.

        THE SEED'S SIGN WAS WORTH 22%. The difference set is centred near
          −delta, so the direction from it TOWARD the origin is +delta. The first
          draft seeded with −delta, which picks the point FARTHEST from the
          origin — legal, and one wasted iteration per query for ever. Mean over
          117,413 separated box pairs: 4.52 → 3.52, and sphere-sphere 2 → 1.
          WARM STARTING, by contrast, is worth 3% (2.219 → 2.152 mean) — and the
          CONTROL is what makes that number readable: a RANDOM seed costs 54%
          more, so the seed matters a great deal and the free heuristic was
          already nearly right. 8.7 will want warm starting for STABILITY (the
          same contact feature frame to frame), not for speed.

        POLYTOPES TERMINATE, CURVES CONVERGE — and the odd one out is the sphere.
          Box 2.68→2.80 and hull 1.57→1.98 across five decades of tolerance, i.e.
          FLAT: finitely many vertices, so the simplex can only improve finitely
          often and the tolerance is never what stopped the search. The SPHERE is
          flat at ONE ITERATION, because a ball's nearest point to an exterior
          point is on the line to its centre and the first support call along
          `delta` lands ON the answer. Only the CAPSULE genuinely converges
          (2.06 → 7.43), because its difference is a parallelogram rounded by a
          ball and the nearest point can sit on the curved part.

        TWO SEPARATE THINGS GIVE OUT AT TWO DIFFERENT GAPS, and conflating them
          would have produced a wrong fix.
            THE PROOF first, at |v| ~ eps*|w|/tol ≈ 4.8e-03 m for 2 m cubes. The
              termination threshold is proportional to |v|² — the gap SQUARED —
              while its rounding error is O(eps*|v|*|w|) — the gap times the
              SHAPE. One falls quadratically, the other linearly. Below the
              crossover the test is noise against noise; the ANSWER is still
              right and still a valid UPPER bound (worst shortfall below truth
              over 36,000 sphere-box pairs: 8.5e-06 m).
            THE ANSWER second, at |v| <= tol*|w| — a CONTACT MARGIN, measured by
              bisection at 2.11 x tolerance x size across FOUR DECADES of cube
              size. Which is the finding: `tolerance` is a RELATIVE quantity and
              "set it to a millimetre" is a category error.

        8.4 §11's PRECISION WALL IS FIXED RATHER THAN REPEATED. Support points
          are taken RELATIVE to each shape's own centre and the single
          world-sized subtraction `b.origin − a.origin` is formed ONCE, outside
          the loop — where it is EXACT by Sterbenz's lemma, which two things
          about to collide always satisfy. Measured on one pair walked from 0 to
          1000 km against an exact double reference taken from the SAME float
          boxes: naive 1.0299e-02 m worst error, relative 1.0698e-07 —
          **96,270x**. The naive error tracks ulp(position) at a fixed fraction;
          the relative error tracks nothing.
          THE TWO ARMS ARE NOT TWO IMPLEMENTATIONS. gjk.cpp is the same code in
          both columns and only the `convex` VIEW differs, which is what makes
          the comparison mean anything — 4.8 §3's lesson about harnesses that
          construct their own configuration, applied on purpose.
          AND THE REFERENCE HAD TO BE FIXED TOO. The first version computed the
          box corners as `centre - half` IN FLOAT, which is exactly what the
          NAIVE arm does, so the reference agreed with the arm it was meant to
          convict and reported the relative form as 96,270x WORSE. Promoting to
          double before subtracting flipped the result. A reference that rounds
          the way one arm rounds is not a reference.

        THE DUPLICATE TEST MUST NOT USE THE USER'S TOLERANCE. It exists to catch
          a support function returning a vertex the simplex already has, which on
          a POLYTOPE is how the search finishes exactly. The first version used
          `cfg.tolerance` (1e-4) and was a quiet disaster on curved shapes: a
          capsule's support point moves continuously, so consecutive iterations
          produce points a fraction of a millimetre apart — "duplicates" by a
          relative 1e-4 measure — and the loop exited before the bounds
          converged. Measured: capsule certificate slack of 2.4% where the
          tolerance promised 0.01%, on results NOT flagged stalled and therefore
          looking trustworthy. `k_duplicate_rel2 = 1e-12f`, at float's own
          resolution.

        THE SENTINEL THAT WAS ALREADY TAKEN. GJK's `axis_index` is -3, not -2,
          because collide.cpp already had `constexpr int k_no_axis = -2` and
          `source_of` mapped it to `none` by FALLING OFF THE END of the function
          rather than through a named case. A sentinel reachable only by
          fallthrough is a sentinel waiting to be reused by mistake. Both are
          named cases now.

        THE FUNCTION POINTER COSTS NOTHING: 1.906 ns through `convex::support`
          against 1.908 ns direct, bit-identical. One target per query is a
          branch the predictor learns on the first call — 6.17's finding at a
          hotter call site. AND GJK IS SLOWER THAN THE SAT ON BOXES AND SHOULD BE:
          8.433 / 49.477 (overlaps / collide) against 73.223 / 114.744
          (gjk_intersects / gjk_distance). What GJK buys is the metres and the
          generality, not the speed, which is why the engine keeps BOTH.

        A HULL IS NOT A shape_kind, and the reason is `inertia_of`. A `shape` is
          a 24-byte value Module 9 will write to a save file and a pointer is not
          a value — but the sharper reason is that inertia_of(shape) would have to
          ANSWER for a hull and cannot: the divergence-theorem sum needs faces and
          a point set has none. A function returning a plausible wrong answer is
          worse than one that does not exist. Named in §14; 8.12's ragdolls use
          capsules for exactly this reason.



  8.6 — EPA: PENETRATION DEPTH.  Nine measured sections, 2,028 checks, 0
        failures. 11 figures. The lesson pays 8.5's IOU, and FOUR OF ITS FIVE
        FINDINGS ARE BUGS THE MEASUREMENTS FOUND — three of them in the seed,
        which is the part every write-up skips.

        THE PLACEHOLDER WAS WORSE THAN ITS OWN COMMENT SAID. 8.5's `collide.cpp`
          described the axis on an overlap as "the last search direction, a
          reasonable guess at a contact normal". `gjk_result::direction` is
          assigned in ONE place and that place is the separated path, so on an
          overlap it was the ZERO VECTOR — 200,000 of 200,000. A comment that
          describes what the code was MEANT to do is worse than no comment,
          because no reader goes looking: a poor normal and a zero normal look
          identical from outside until something divides by one.
          8.5's PAGE STILL PRINTS THE WRONG COMMENT, ON PURPOSE. A page is an
          accurate archive of what shipped, and the correction belongs in 8.6 §1
          where a reader meets it with the measurement attached. Patching the pin
          would erase the mistake instead of teaching it. (This is NOT the
          "port the correction back into the pin" case in CLAUDE.md §11 — that
          rule is about editing a published page's HTML without editing the
          fragment it came from. Nothing in 8.5's HTML was touched.)
          The naive alternative measured for comparison, centre-to-centre, is
          40.15° off on average and implies a push of 1.71x the MTV (max 2,538x).

        THE DEFINITION IS TWO LINES AND 8.5 ALREADY PROVED THE STEP. A and B+t
          overlap iff t = a − b for some pair of points iff t ∈ A ⊖ B, so THE
          TRANSLATIONS THAT KEEP TWO SHAPES OVERLAPPING ARE THE DIFFERENCE SET,
          and the shortest that separates them is the nearest point of its
          BOUNDARY. GJK measured the distance to the SET, which is zero when the
          origin is inside; EPA measures it to the set's BOUNDARY. Same support
          function, same sandwich, OPPOSITE HANDEDNESS — the inner polytope's
          closest face is a LOWER bound that RISES, where 8.5's `|v|` only fell.
          `epa.cpp` therefore carries 8.5's monotonicity check with the
          inequality reversed, and it fires for the same two reasons.

        AND 8.4 WAS THE EXACT ANSWER FOR BOXES ALL ALONG. min over unit n of
          h(n) is attained on a FACE NORMAL of A ⊖ B, and the faces of a
          difference of two boxes have normals ±aᵢ, ±bⱼ, ±(aᵢ × bⱼ) — 8.4's
          fifteen candidate axes in both signs, and nothing else. So the SAT's
          minimum-overlap axis IS the minimum translation, shipped a lesson
          before EPA existed and described only as "the smallest translation
          that would separate them". That makes 8.4 this lesson's reference.
          The contrast with sampling is the point: 4,096 random directions per
          pair over 2,000 pairs never beat EPA (a minimum over a subset can only
          be too large) and never got within 0.03%, while 30 enumerated normals
          land on the answer to six digits. A FALSIFIER CAN CONVICT AND CAN
          NEVER ACQUIT.

        THE SEED IS THE HARD PART, AND THE LITERATURE DOES NOT MENTION IT.
          On 50,000 crates on a floor GJK hands over a 4-point simplex ZERO
          times and a flat TRIANGLE 49,951 times. NOT the flat tetrahedron the
          folklore warns about — `reduce_simplex` discards any vertex the
          closest point does not use and three points in a plane already span
          it, so the redundant fourth is gone before the function returns. That
          is BETTER news: a triangle GJK reduced to necessarily covers the
          origin's projection (checked, 30,000 of 30,000), where a flat
          tetrahedron would have needed a choice with no guarantee. A textbook
          EPA refuses 100% of that row and 1.57% even of free orientations.
          BUG 1 — ONE APEX IS NOT ENOUGH. The origin was already in that plane,
            so a single support call out of it leaves the origin on the
            tetrahedron's BASE: closest face at distance zero, lower bound
            starts and stays there. 30,000 of 30,000, MAXIMUM as well as mean.
          BUG 2 — SIX FACES STITCHED BY HAND ARE NOT A POLYTOPE. A bipyramid is
            convex only when each apex projects INSIDE the triangle, and a
            support point along the plane normal has no reason to: 52.85% are
            not convex. Only 2.3% of queries then answered visibly wrong, which
            is EXACTLY why it survived — nineteen in twenty broken seeds
            repaired themselves on the first expansion. It took a STATUS
            HISTOGRAM over 50,000 pairs, not a failing assertion, to see it.
            Fixed by refusing to hand-build: tetrahedron on the first apex
            (always convex), second apex through the same `expand` the loop
            uses, which is beneath-and-beyond and produces the hull by
            construction. The fix is SMALLER than the code it replaced.
          BUG 3 — THE CONTAINMENT QUESTION WAS ASKED TOO EARLY. GJK can
            terminate on a SEGMENT THROUGH THE ORIGIN on a pair overlapping
            deeply, so the seed puts the origin on an EDGE where the incident
            offsets are zero to within a few ulps. Testing `d < 0` up front
            returned a depth of ZERO for spheres 70 cm inside a crate, 19 in
            20,000 — and scaling the threshold by `cfg.tolerance` made it come
            BACK as the tolerance tightened, because the margin was set by GJK's
            tolerance, not EPA's. THERE IS NO CONSTANT THAT IS RIGHT. So it is
            not asked: the expansion runs either way (beneath-and-beyond does
            not care where the origin is) and `lower <= 0` at the END is the
            answer, where it is a measurement rather than a guess.

        BUG 4 — THE HORIZON, AND IT IS THE ONE TO REMEMBER. A support point that
          lands EXACTLY on several of the polytope's face planes leaves
          `dot(n, w)` and `d` as the same number computed two different ways,
          and the last few bits decide which side of `>` each face falls on.
          Traced on two 1 m cubes face to face with 0.1 mm of overlap: THREE
          faces called visible, scattered, sharing no edge — nine horizon edges
          with nothing to cancel, a fan stitched across three loops, and a depth
          of depth/√3, 42% LOW. A comparison whose two sides are mathematically
          equal has no correct answer in floating point; the code asked it
          anyway. The repair is to ask a question that HAS an answer: flood fill
          the visible set from the CLOSEST face, which is the one face certainly
          visible. In exact arithmetic it changes nothing (the set was already
          connected — Preparata & Shamos §3.4); in float it undoes the scatter.
          MEASURED, AND THIS IS THE THIRD LESSON RUNNING WITH THE SAME MORAL:
          4.17% of crates on a floor tear (worst 61.7% low), 0.01% at free
          orientations — four hundred times rarer — and NEVER on hulls built
          from random points. The tear needs the axis alignment and face-on-face
          contact a real level is made of, which a uniform random fixture
          destroys. 8.4 §F said it about shared up axes; 8.5 §C about slivers.

        EULER'S FORMULA AS A RUNTIME TEST. F = 2V − 4 for a closed triangulated
          surface of genus zero, and `epa_result::manifold()` is that line —
          computable from the RETURNED STRUCT ALONE, so the harness never has to
          see inside EPA to know whether EPA built a polytope. Green at every
          exit over 200,000 queries across two populations, including 1000:1
          aspect-ratio plates whose face normals are rounding error. It also
          SIZES the arrays rather than guessing them, and is why dead faces are
          COMPACTED rather than flagged: faces CREATED over a query (each pass
          deletes k and stitches k+2) far exceed faces held, so flagging turns a
          bound on the polytope into a bound on the total work.

        THE SPHERE HAS SWAPPED PLACES, and the asymmetry is geometric. GJK's
          EASIEST case — exactly ONE iteration, because a ball's nearest point to
          an exterior point is on the line to its centre — is EPA's WORST, 28.64
          rising to the 60-cap, because the nearest BOUNDARY point from inside is
          on a sphere of directions and every triangle is a chord that
          under-reaches. GJK walks TO a curved surface; EPA has to COVER it. Box
          and hull are flat against tolerance for 8.5 §9's reason with the
          inequality reversed. Which is the concrete argument for KEEPING 8.4's
          closed forms: generality is a property of the interface, not an
          instruction to use it everywhere.
          max_iterations = 32 is justified by measurement rather than taste: 48
          removes 98% of the remaining cap hits and buys 1.6 MICRONS, because
          past 32 the error is the flat-triangle discretisation, not the count.

        AND TWO STRUCT DEFINITIONS WERE WORTH 20.5%. 1344.04 -> 1068.14 ns/pair,
          MEASURED BACK TO BACK, from deleting the default member initialisers
          on `face` and `edge`.
          One on ANY member makes the whole type non-trivially-default-
          constructible and that propagates into arrays of it, so `polytope p;`
          — an ordinary local, once per query — memset four kilobytes the seed
          overwrote on the next line, and `expand`'s 3 KB horizon array is
          declared PER PASS. The tell is the array, not the struct. The THIRD
          array was measured and LEFT ALONE (1054.4 against 1053.1, inside the
          noise, because the compiler could see it) and the file says so: a
          change that buys nothing still costs a reader.

        A SWEEP FINDS WHAT A SAMPLE MISSES. §H.2 — two cubes walked face to face
          from 10 cm of overlap down to zero, nine lines of fixture — found TWO
          of the four bugs, because it walked a parameter across decades instead
          of sampling it. Both were invisible to 200,000 random pairs.

        AND THE FIXTURE CAN BE THE NULL RESULT. §H.3's first version used half
          extents of 0.5 and measured the naive and relative support
          formulations as EXACTLY EQUAL at every distance — because 0.5 is a
          multiple of the float grid spacing at every scale below 2²¹, so
          `centre + half` stays exact even a million metres out. Half extents of
          0.37 are a multiple of nothing, and the naive arm reaches a CENTIMETRE
          while the relative one holds at 1.5e−08 m. A null result from a fixture
          that cannot express the effect is not a null result.



  8.10 — SEQUENTIAL IMPULSES: WARM STARTING, ISLANDS, AND SLEEPING. (Its `updated:`
        header, moved here intact by 8.11 so that append-and-merge loses nothing.)
         13, 54 h of 70. PLANNED AT 6 AND SHIPPED AT 6, so no module subtotal
         and no course total moved. Still 107 lessons, ~523 h.
         check-curriculum.py green; check-builders.py green; check-page.js
         `pass: true` at 1280 AND 390. Eleven measured sections, 64 checks.
         8.9's findings are in STATE below (`conventions: contact:`) and in the
         memory file for 2026-09-19; what follows is 8.10's.

         *** THE ENGINE CAN HOLD A STACK UP. *** And the honest bound on that
         sentence is one table: EIGHT SWEEPS HOLD FIVE CRATES AND DO NOT HOLD
         TEN. 2 crates need 2 sweeps, 3 need 2, 5 need 4, 10 need 20, and 15
         need more than 64. A Gauss-Seidel sweep carries information across ONE
         contact, so a chain of n needs of order n sweeps; §13 measured the cost
         as LINEAR in the count, so a tall stack is quadratic and a scene costs
         what its tallest pile costs. An under-swept tower does not sag, it
         LEANS, because the corrections arrive inconsistently and the
         inconsistency has a direction.

         NO NEW TRANSLATION UNIT, NO NEW PUBLIC HEADER. First time in Module 8:
         engine/CMakeLists.txt and engine.hpp are untouched. Ten listings all
         the same (10,231 lines, the largest set in the course) because six are
         files earlier lessons printed and CLAUDE.md §8 says whole.

         THE CONTRACTION IS 0.53 PER SWEEP and it is constant over four decades.
         Measured on ONE crate — a four-point manifold is already a system, and
         you do not need a stack to need a solver, because three quarters of the
         effect of an impulse at a corner of a 0.25 m cube is ROTATION and
         rotation is what the four points share. The control is a sphere: one
         point, nothing to iterate against, 2.5e-13 at one pass and the same at
         sixteen. The two rows below 1e-07 are EXCLUDED from the mean, because
         there the residual has stopped measuring the solver and started
         measuring `float` — including them reports 0.64 where the solver is
         0.53.

         WARM STARTING IS WORTH 89 ITERATIONS AGAINST MORE THAN 400, and at the
         shipped eight the cold five-crate tower does not settle badly, it
         COLLAPSES: 2,020 mm of sink against 40. Match rate 97.62% over 600
         frames. It is FREE (the impulses are already in the manifold) and it is
         an INITIAL GUESS: scaling the inherited impulse by 2 costs more
         iterations than discarding it. AND THE ×5 ROW IS THE WARNING THAT GOES
         WITH EVERY CONVERGENCE NUMBER HERE — 52 iterations, a beautiful
         residual, and a tower moving at 1.54 m/s. THE RESIDUAL MEASURES
         CONSISTENCY, NOT CORRECTNESS. Every quoted residual in this lesson has
         a second measurement beside it saying where the bodies are.

         *** A CACHED NUMBER IS MEANINGLESS WITHOUT THE FRAME IT WAS MEASURED
         IN, AND THAT BUG WAS THREE LESSONS OLD. *** `contact_point::
         tangent_impulse` is two COORDINATES in a basis, and the basis was never
         stored. `tangent_basis` seeds from the normal's SMALLEST component, so
         on a near-vertical contact the choice is |n.x| against |n.z| — two
         numbers around 1e-05 that cross constantly. Measured: 113 of 11,830
         manifold-frames rotate by more than 30 deg, worst 134.6, and last
         frame's friction is applied SIDEWAYS. Fix: `contact_manifold::tangent`,
         two dot products, exact. Drift 380 mm -> 155 at 24 iterations. Nothing
         in the type system was ever going to catch it — both versions are two
         floats called tangent_impulse — and what found it was a PROBE, not a
         test: compute the basis independently in the harness and count.

         THE ARRIVAL DEPTH IS A CEILING, NOT AN EQUALITY, and §6's first draft
         claimed the equality and was refused by up to 81%. The body crosses the
         surface INSIDE a step and is sampled a whole step later, so how much
         was left over is arbitrary: over 200 finely spaced drops per row the
         depth is UNIFORM on [0, v*h] — mean 0.486/0.489/0.505, largest 0.9725,
         smallest 0.002. `arrival_depth` is documented as the ceiling. And it is
         FROZEN: 3.09% across a 64x range of iteration count while the residual
         moves six orders of magnitude. TWO DIFFERENT PROBLEMS.

         BAUMGARTE ADDS ENERGY AND SPLIT IMPULSE DOES NOT, for the same rate.
         tau = -h/ln(1-beta) measured at 333.33 / 166.67 / 83.33 / 33.33 ms
         against 324.93 / 158.19 / 74.69 / 32.63 — every one the prediction
         rounded up to a whole 16.67 ms step, which is the finest a once-a-frame
         measurement can be. Departure from 100 mm deep with gravity off:
         1.14006 m/s and 6.50 J from nowhere, against 0.00000 for split impulse.
         WITH GRAVITY ON IT IS MOSTLY HIDDEN, which is why it survives: below a
         200 mm start the push cannot beat the climb. At 200 mm Baumgarte throws
         the crate 99 mm CLEAR OF THE FLOOR and at 300 mm, 215. Split impulse
         settles at the slop at every depth tried. Cost: one position iteration
         is 46% of a velocity one.
         AND THE DECAY FIXTURE HAD TO HAVE GRAVITY IN IT. Without it the decay
         came out 33% fast at beta=0.05 and exact at 0.40 — an error that grows
         as a parameter shrinks is a SECOND EFFECT (8.9 §8's rule), and the
         second effect was this section's own subject: an unheld crate keeps the
         correction velocity and coasts out at the largest bias it ever saw.

         THE SLOP REFUSED ITS OWN FOLKLORE. Fifth time in Module 8 (8.6 §1,
         8.7 §9, 8.8 §4, 8.9 §1). A SINGLE resting crate is bit-for-bit
         motionless at a slop of ZERO, under either correction, because a
         geometric correction removes 20% of what is left each step and never
         reaches zero — so the contact never separates and there is nothing to
         oscillate. The folklore describes beta = 1, which nobody ships. What
         needs the slop is contacts that COMPETE: a five-crate tower moves
         1801 um p-p at zero slop, 9086 at 0.5 mm, and 0.00 at 2 mm and above.
         What it costs is exactly legible — THE RESTING DEPTH IS THE SLOP.

         ISLANDS: 20 AGAINST 1, ON THE SAME CONTACTS. A body that cannot move is
         NOT A BRIDGE, and it is safe rather than convenient — an impulse
         applied to a fixed body changes nothing any other contact can read.
         Break it and one rolling marble keeps the level awake, which is not a
         slow simulation but NO SLEEPING AT ALL. The whole partition is 2.48 ns
         per body, labelling included, and allocates nothing: the union-find
         uses its OUTPUT ARRAY as the parent array.
         AND THE ENCODING IS `-label - 2`. The obvious `-label - 1` sends label
         0 to -1, which is already the sentinel for "no island", so the FIRST
         island in every scene silently ceased to exist and a tower whose bottom
         crate landed in it fell through the floor while every other tower
         stood. WHAT FOUND IT WAS TWO INSTRUMENTS DISAGREEING: stats.points == 0
         on a frame where deepest() read 57 mm. Keep enough counters that they
         can contradict one another.

         *** THE SLEEP TEST CANNOT RUN BEFORE THE SOLVE, AND THE REASON IS g*h.
         *** A semi-implicit step gives every dynamic body 0.1635 m/s of
         downward velocity before the solver looks — 3.3x the default threshold
         — so a test placed there NEVER FIRES at any setting, and the symptom is
         indistinguishable from thresholds that are merely too tight. Measured
         on one frame of a scene motionless for six seconds: 0 of 100 quiet
         before, 96 of 100 after; fastest 0.1836 before, 0.0629 after. It is
         8.9's restitution artifact wearing a different hat.
         WAKING IS THE OPPOSITE — it must run BEFORE the solve, or an island
         that has just acquired a moving neighbour is skipped on the frame it
         most needs solving. Hence `wake_islands` and `update_sleep` as separate
         functions at opposite ends of `solve`.
         AND SLEEPING IS PER ISLAND BECAUSE A SLEPT BODY IS AN IMMOVABLE BODY.
         Crate thrown at the bottom of a sleeping three-crate tower: island rule
         drops the top crate 0.500 m, per-body rule 0.0000 — it hangs in the
         air. The fair experiment is a COLLISION; a support removed by teleport
         is invisible to both rules, which is what `wake` is for.
         THE DEFAULT THRESHOLD IS NOT GENEROUS: the smallest value at which a
         yard of 100 sleeps at all is 0.02 m/s against a default of 0.05, and
         one notch below that a quarter of the crates never qualify.
         `wake` RESETTING THE CLOCK matters only in the case nobody tests: a
         woken body that is SHOVED reads 1.0027 m either way, and a woken body
         LEFT ALONE sleeps again after 1 frame without the reset and 30 with.

         THE SPLIT OF body_world::step IS BIT-IDENTICAL, 400 of 400 bodies over
         120 steps, worst position difference 0.000e+00 — checked against a
         private copy of the monolithic function rather than asserted. It is
         only possible because semi-implicit Euler's two lines are independent;
         the other two rules run whole in the velocity half and a contact
         impulse arrives one step late for them. `gyroscopic_mode::momentum`
         cannot be cut at all (its orientation advance is in the middle of its
         own derivation), so all of it runs in the velocity half and the
         position half skips that body's orientation.
         SOLVING ONE STEP LATE COSTS g*h^2 PER RESTING STEP: 2.72500 mm measured
         against 2.72500 predicted, ratio 1.0000, and 654 mm in four seconds.
         THE SPLIT COSTS A SECOND mat3_from_quat PER BODY: 14.542 -> 19.938
         ns/body, +37.1%, undoing half of 8.3 §12's largest saving. Sleeping
         pays it back 15.7x on the same yard.

         THE BUDGET IS LINEAR TO THREE FIGURES: 14.958 us fixed (islands, sleep,
         prepare, warm start, write-back) + 12.917 per velocity sweep + 5.903
         per position sweep, over 100 manifolds. Predicts the n=16 row at 221.6
         against a measured 221.500. At 2,001 bodies: 0.134 broad + 1.118 narrow
         + 3.102 solve = 4.354 ms of 16.67 (26%), the solve 71% of it, and
         1.324 ms once the pile is asleep.
         AND THE FIRST VERSION OF THAT TABLE WAS NONSENSE: it changed the
         iteration count on a RUNNING scene, so the zero-iteration row dropped
         the yard through the floor and measured a scene with no contacts left.
         A timing sweep over a parameter that changes the simulation has to
         restore the simulation — AND THE SNAPSHOT HAS TO INCLUDE THE MANIFOLD
         CACHE, because warm starting is precisely the feature that makes a step
         depend on more than its bodies.

         SOLVE ORDER IS WORTH 0.12 OF AN ITERATION, which is less than the
         folklore claims. Bottom-up beats top-down by 1.06x at n=16 and 1.08x at
         n=64 on a ten-crate tower; at §3's contraction that is an eighth of a
         sweep. The as-added column is IDENTICAL to bottom-up — the broadphase's
         cell order happens to run up the tower — and the control (twelve crates
         touching only the floor, no chain) is 0.000e+00 between the two orders.
         This engine does not sort and NAMES that, which is more honest than
         implying an accident is a decision.

  8.11 — CONSTRAINTS AND JOINTS: HINGE AND BALL-SOCKET. (Its `updated:`
        header, moved here intact by 8.12 so that append-and-merge loses nothing.)
         13, 60 h of 70. PLANNED AT 6 AND SHIPPED AT 6 — the fourth lesson
         running to land on its estimate — so no module subtotal and no course
         total moved. Still 107 lessons, ~523 h.
         check-curriculum.py green; check-builders.py green; check-page.js
         `pass: true` at 1280 AND 390. Eleven measured sections, 58 checks, in
         half a second. 8.10's findings are in STATE below (`conventions:
         solver:` and the `8.10 —` notes block after `roadmap:`) and in the
         memory file for 2026-09-19; what follows is 8.11's.

         *** THE ENGINE HAS JOINTS, AND NO SECOND SOLVER. *** A constraint is a
         JACOBIAN ROW — twelve numbers in four vec3 blocks — plus two BOUNDS on
         the accumulated impulse. A rod, a rope, a ball-socket, a hinge, a limit
         and a motor are choices of rows and bounds (and so, analysed, is 8.9's
         friction). Joints ride 8.10's loop unchanged: prepared and warm-started
         once, visited FIRST in every sweep of every island and then the
         contacts, visited in the split-impulse position pass, written back.
         `contact_solver` is now `constraint_solver`; the old name survives as
         an alias so demos/stack and 8.10's harness compile unchanged.

         A CONTACT NORMAL IS A ROW, checked to 1.9e-07 on 200 real contacts, and
         eight sweeps both ways agree to 2.6e-08 m/s. Controls: delete the
         angular halves and the mass is wrong by 3.80x-4.03x (8.10 §2's corner
         predicts 4.000); negate J and the yard falls 784 mm in 0.5 s where the
         derived rows sink 27 mm (the cold-start sink). THE ROW IS 27% FASTER —
         8.75 against 11.95 ns a point per sweep, 108 against 76 bytes — which
         REFUSED the handover's prediction: a row stores M^-1 J^T and applies an
         impulse with four scaled adds, where 8.9 recomputes I^-1 (r x J) with
         two cross products and two mat-vecs on every visit. solve_contacts
         stays hand-written ANYWAY, because the hoist changes every number 8.9
         and 8.10 printed at the rounding level. It is exercise 3.

         A ROPE IS A CONTACT TURNED INSIDE OUT: point_row(-u), bounds [0, inf).
         Swung up with v0^2 = 3.5 g L, it lets go at EXACTLY the height a rod's
         row changes sign from tension to compression, at every rate — and both
         read LOW against the closed form L/2 (17% at 60 Hz, 3.6% at 240, 0.9%
         at 960, first order in h per unit time) because the bob has lost 10% of
         its energy on the way up. REFUSAL #2, and the refusal was §5. The
         parabola after release lands EXACTLY on the lowest point of the circle
         (y = L/2 + 3L/2 - 3L = -L), which nobody planned.

         *** A VELOCITY JOINT DRIFTS AND DISSIPATES, BOTH IN CLOSED FORM. ***
         After the solve the bob moves along a TANGENT, so r(n+1)^2 = r(n)^2 +
         (v h)^2, and the next solve removes only radial velocity, so v r is
         conserved EXACTLY: a rod at 5 m/s tracked for 60 steps to 1.7e-07,
         angular momentum to 2.9e-07, energy (L/r)^2 to 1e-06. For a BODY on a
         pin only the centre's ORBIT is projected, so the energy lost per step
         is rho (w h)^2 with rho = m d^2 / (I_cm + m d^2) — to 0.1% on six
         configurations. `lever_ratio` and `projection_loss_per_step` compute it.
         INSTRUMENT LESSON: the velocity a step ENDS with was solved at the
         radius the step STARTED with; pairing it with the final radius missed
         by exactly one step's drift (0.4%).

         *** BAUMGARTE CUTS BOTH WAYS ON A JOINT. *** Both corrections leave a
         turning joint stretched by delta/beta (12.50 vs 12.22 mm split, 16.55
         vs 16.53 Baumgarte). Split impulse then loses (w h)^2 per step exactly
         as no correction does; BAUMGARTE LOSES NOTHING, because its REAL inward
         bias delta/h, turned by w h into the next step's tangent, is
         v (w h)^2 / 2 — precisely the speed the projection removes. REFUSAL #3:
         the section set out to show Baumgarte adding energy. But a cube pulled
         back into a corner socket from 0.2 m keeps 0.35 J of spin under
         Baumgarte (zero through the centre, zero under split) — the energy goes
         into the joint's FREE degrees of freedom. A 90 deg pendulum at 60 Hz
         keeps 75.4% of its energy per period under split and 92.0% under
         Baumgarte. THE DEFAULT STAYS SPLIT IMPULSE: damping reads as damping,
         invented energy reads as a twitching limb. Per-joint choice is ex. 2.

         THE BLOCK. A socket's three rows are Gauss-Seidel on K = J M^-1 J^T,
         and their contraction IS K's Gauss-Seidel spectral radius — 0.2323
         measured against 0.2325 computed from K alone. The 3x3 block is exact
         (5.9e-07 of the violation, worst of 1,000 random sockets, against up to
         0.925 for rows) AND cheaper (10.8 against 23.1 ns a visit). r = 0
         control: K is diagonal and rows are exact too. Affordable because joint
         rows have NO CLAMP; a block of contact rows is an LCP. On a 12-link
         chain it barely helps (94 against 102 mm worst gap): a chain's problem
         is between its joints, not inside one.

         THE PENDULUM. 1.645967 s against 1.646241 at 2 deg, 13.8% from the
         point-mass formula; error divided by 4.02 / 4.00 / 3.98 per halving of
         h. The reference HAD to be the period at 2 deg — T0 / AGM(1, cos(a/2))
         — or the error refused to converge (+6.6e-05 at 240 Hz). The AGM form
         holds to 2.6e-03 up to 120 deg at 3840 Hz.

         THE HINGE: a 3x3 pin block plus a 2x2 alignment block. A door stays
         within 0.0001 deg of true; on the socket alone it falls 176 deg. The
         perpendicular basis is chosen from the axis IN BODY a's FRAME and the
         cached impulse is a WORLD vector: a world-chosen basis flips 127 of
         7,787 hinge-steps on a sagging bridge, every one exactly 90 deg; the
         body-chosen one, never.

         *** LIMITS ARE SPECULATIVE FOR FREE, AND rho IS A CONVERGENCE RATE. ***
         A hinge always knows its angle, so a limit row exists before its stop
         with target -C/h. Turnstile: reactive overshoot uniform on [0, w h]
         (mean 0.4985 over 200 PHASES — sweeping the speed instead covered 1.36
         phase cycles and read 0.42), speculative exactly 0. On a DOOR the limit
         row, which sees I_cm, converges against the pin at EXACTLY rho per
         sweep: 0.4261 / 0.7481 / 0.9224 at d = W/4, W/2, W, to four figures. Any
         uniform slab hinged at its edge is 3/4. At eight sweeps a door with
         restitution zero bounces off its stop at 7.53% of its arrival speed.

         MOTORS. A saturated clamp is an exact torque: the stall bisects to
         9.8100 N m against m g d = 9.8100. Spin-up t = I w / tau is right to the
         step through the centre and 7.9% SLOW at the edge — REFUSAL #4 — because
         the joint bleeds rho (w h)^2 while the motor works; dw/dt = tau/I -
         (rho h / 2) w^3 predicts 2.8847 s against a measured 2.8833. A
         zero-speed motor is Coulomb friction in the hinge: it stops a door
         within two steps of prediction and then holds it to 1e-43 rad/s.

         ISLANDS. A joint is an edge by `union_edge`, THE SAME FUNCTION contacts
         use. Two chains from one ceiling are 2 islands (16 without joint
         edges). A sleeping chain struck at the bottom: the island rule wakes 8
         of 8, a per-link rule 1 of 8, swinging from a frozen chain.

         WHAT IT LEAVES. A 10-link chain under a 100:1 end weight stretches
         388 mm at eight sweeps (1:1, 1.33 mm; 1000:1 does not settle). EIGHT
         SUB-STEPS OF ONE SWEEP hold it to 20.5 mm — 19x better for the same
         joint visits. Per joint per sweep: socket 13.7 ns, hinge 23.9, hinge +
         limit + motor 60.0, rod 12.5, against 129 per contact MANIFOLD.


  8.12 — RAGDOLLS: JOINTS ON A SKELETON. (Its `updated:` header, moved here
        intact by 8.13 so that append-and-merge loses nothing.)
         2026-09-23 (after Lesson 8.12 — 95 of 107 lessons; MODULE 8 OPEN, 12 of
         13, 66 h of 71. PLANNED AT 5 AND SHIPPED AT 6 — the first lesson
         since 8.7 to miss its estimate, and the index moved with it: M8 70 ->
         71 h, the course ~523 -> ~524 h. Eleven measured sections and TWO
         ENGINE BUGS FIXED WITH REGRESSION EVIDENCE is 8.11's size, not 5 h.
         check-curriculum.py green; check-builders.py 63/63 byte-identical
         (see OPEN DEFECTS for --figures); check-page.js `pass: true` at 1280
         AND 390. Eleven sections, 68 checks, 1.84 s. 8.11's findings are in
         `conventions: joint:` and the `8.11 —` notes block after `roadmap:`;
         what follows is 8.12's.

         *** THE ENGINE HAS RAGDOLLS. *** phys/ragdoll.{hpp,cpp}: a skeleton
         becomes eleven capsules (Dempster's masses, Winter Table 4.1: pelvis
         .142, thorax+abdomen .355, head+neck .081, upper arm .028, forearm+
         hand .022, thigh .100, leg+foot .061 — they sum to 1), 4 hinges and 6
         BALL-SOCKETS WITH A SWING CONE AND A TWIST RANGE. Twelve of 23 joints
         are PASSENGERS riding on the part above at the clip's last local.
         EVERY BODY HAS EXACTLY ONE OWNER: animated = kinematic, STEERED BY
         VELOCITY (never teleported); simulated = dynamic, joints in the solver.

         SWING-TWIST, NOT EULER. q = swing * twist, twist about the bone; the
         swing is 7.4's two mirrors (t, then half-way h). Euler sensitivity
         609.9 at an arm raised forward (1/cos(pitch) = 573 predicted), swing-
         twist 1.55 there; its one singularity is the antipode of the axis the
         swing is measured FROM — so measure from a cone axis tilted into the
         middle of the range, and no cone ever contains it.
         THE SWING ROW IS EXACT: dphi/dt = (w_b - w_a).n, n = a1 x b1 / |..|.
         THE TWIST ROW IS THE HALF-WAY AXIS OVER cos(phi/2):
         (w_b - w_a).(a1 + b1)/(1 + a1.b1), because the swing's own angular
         velocity 2 h x h_dot is perpendicular to a1 + b1. A row about the bone
         is wrong by -(w_perp . a1)/(1 + cos phi) <= |w_perp| tan(phi/2)
         (bound measured to 1.0045). CODMAN'S PARADOX: carried round a loop
         with no spin about itself a limb gains the loop's SOLID ANGLE of
         twist — 90.000 deg for the octant, 48.231/180/360/540 for circles at
         30/60/90/120 — to three decimals. In the solver the bone row does NOT
         walk through its stop (REFUSED): the position pass measures the TRUE
         twist and lags it by alpha'(1 - cos phi) h / beta = 9.55 deg predicted,
         9.08 measured; with no position pass, 179.93 deg.

         TWO SHIPPED BUGS, FIXED. (1) 8.11's solve_joint_positions solved a
         SATISFIED one-sided row against its zero bias, so a two-ended limit's
         far stop cancelled every correction of the near one: a loaded knee
         frozen at -0.1691 deg for ten seconds, cold -70.7 deg; now 0.0000 and
         a steady -1.8915. Fix: satisfied rows take their speculative target
         -C/h in the position pass too (solve_row_position_to). verify_811
         rerun: 58/58, ONE table moved (§H rebound 0.0753 -> 0.0750, overshoot
         0.00318 -> 0.00316 rad). (2) 8.10's islands never let a MOVING
         KINEMATIC body wake anything (kinematic is not a bridge, so it never
         joined the sleeping island): a steered character ran through a crate
         asleep before it arrived (0.09 m/s, 48 contact frames). Fix:
         wake_touched_by_kinematic before wake_islands, gated on the sleep
         thresholds so a stopped lift wakes nothing. verify_810: every
         non-timing line byte-identical.

         THE HANDOFF IS ONE ASSIGNMENT. The chord a steered body carries IS
         semi-implicit Euler's velocity (x_n = x_{n-1} + h v_n): 0.0045 m/s
         from the clip's midpoint velocity, 0.2320 from its end. Rotation:
         the linearised spin inverts to the RODRIGUES vector (2/h) dq.v/dq.w —
         lands to 1.57e-07 rad, the logarithm misses by 9.75e-05 against
         theta^3/12 = 9.76e-05. Momentum kept EXACTLY (204.40 kg m/s =
         80 x 2.555); at rest the character stops dead (10 mm in 0.5 s against
         1.18 m). Each joint starts violated by 1/2 h |wb x (wb x rb) -
         wa x (wa x ra)|, to 0.43%. 7.7'S WALK BENDS THE KNEES FORWARD (48 deg
         past the stop) and twists forearms off their hinges (32 deg): split
         impulse repairs it adding 0.001 J, Baumgarte leaves 0.356 J more.

         THE RETURN: read_pose inverts part_targets to 2.4e-07; realign_model
         (ground plane only; heading is ex. 4) removes a 1.302 m slide; a
         local slerp blend changes no bone by more than 0.06 mm, a model-space
         blend by 208 mm; first frame 58 mm against a 1.648 m snap; the worst
         joint crosses 115.5 deg, where nlerp lags slerp by 1.98 deg.
         THE FIGHT: teleported dynamic bodies carry a 20.6 m/s velocity lie
         and a limb 287 mm into a crate; with the chord added, 159 mm.

         WHAT RAGDOLLS LEAVE. Capsule rho about 0.1 above Dempster's (0.76-0.78
         vs 0.64-0.68). Default exclusion = the 10 jointed pairs (nothing else
         within 4 cm at rest — the handover's forearm/torso overlap REFUSED);
         "two links" excludes 13 more incl. torso/forearm (touch in 9 of 12
         falls) and a forearm RESTS 179.6 mm inside the chest. Eight sweeps:
         92 mm peak gap in a fall; hung from one hand 4.13 mm at 8 sweeps,
         2.52 at 32, 0.001 with EIGHT SUB-STEPS; a chest lying on an arm sinks
         61.4 mm (a 16:1 stack). SLEEP REFUSED: every trip settles to mJ, only
         4 of 12 sleep at 8.10's crate-tuned thresholds (5 crushed, 3 thin
         limbs turning); angular damping 1/s -> 7 of 12. 12.4 us a falling
         ragdoll a step (solve 8.1), 81 per ms.

  CARRIED FROM STATE.md `next:` ON 2026-09-27, VERBATIM. When the post-Module 8
  review inserted 6.17b, 6.18b, 7.7b and 8.14 and re-planned Module 9, `next:`
  moved to 6.17b. The notes below were written for "9.1 — Multithreading",
  which is now 9.3; they apply to it unchanged, and their OPEN DEFECTS list is
  the engine's standing defect list.

  next: 9.1 — Multithreading: Data Hazards and Safety Rules

        MODULE 9 OPENS. Module 8 is complete: 13 lessons, ~72 h. The next
        lesson is the first of Professional Polish & Capstone, and the first in
        the course to run code on more than one thread.

        WHAT 9.1 INHERITS, AND MUST NOT RE-DERIVE:
          - Every system so far is single-threaded and several are already
            partitioned: 8.10's ISLANDS are independent by construction (no
            contact or joint crosses one), 8.8's grid emits pairs per cell, and
            8.13's casts are pure functions of a const world. Those are the
            natural first jobs, and the lesson should say so rather than invent
            a toy.
          - The one shared mutable thing the engine already has is 7.8's audio
            voice table, behind one mutex, touched from SDL's audio thread. 9.1's
            data-hazard vocabulary should be shown on THAT before anything new.
          - A LESSON PAGE ENDS AT FURTHER READING. STATE.md is the sole resume key.
          - `scratch/build_verify_NN.sh` REFUSES an unoptimised libengine.a.
          - Timing a threaded harness adds an instrument problem the course has
            not met yet: the OS scheduler. Minima over repetitions, as since 8.8,
            and say how many cores the machine has.

        OPEN DEFECTS, STILL DELIBERATELY NOT FIXED:
          (2026-09-28: four more, found by 6.17b — see state/decisions.md `found-by-617b`.)
          1. `epa_config::max_iterations = 32` gives a sphere-sphere normal
             4.2602 deg off (8.6's knob). Did not bite in 8.12 or 8.13.
          2. EIGHT SWEEPS DO NOT HOLD A TEN-CRATE TOWER, a chest off an arm, or a
             hanging body; SUB-STEPPING is the measured answer three times.
             Module 9 prices it with contacts in the loop.
          3. NO SPECULATIVE CONTACTS (8.10 §6). The CHARACTER no longer tunnels
             (8.13 §9 casts); every other body still does, at 8.10's odds.
          4. The M^-1 J^T hoist into contact_constraint (8.11 ex. 3).
          5. Joints-before-contacts is argued, not measured (8.11 ex. 4).
          6. SLEEP THRESHOLDS ARE PER-BODY SPEEDS tuned on crates (8.12 ex. 3).
          7. `check-builders.py --figures` fails 511 because build/swarm511.ppm
             (a gitignored render capture) is gone, and the known 45/46/48.
             PLAIN check-builders is green: 511's truncated fig7 SVG was restored
             from its own page on 2026-09-24.
          8. NEW: THE CONTROLLER IS NOT PUSHED BY ANYTHING (one-way coupling; the
             proxy is kinematic) and puts no weight on what it stands on. Both
             are 8.13 ex. 4, with the proxy's contact impulses as the input.
          9. NEW: THE CONTROLLER'S CULL IS LINEAR IN THE SCENE (847 us at
             10,000 bodies). A region query on 8.8's uniform_grid is 8.13 ex. 5,
             and a natural Module 9 profiling case study.
         10. NEW: GJK's certified lower bound is loose by about the obstacle's
             SIZE times its angular error; a 60 m ramp as one box leaves the
             character up to 2.3 mm above its skin. An authoring rule, not a fix.

        CARRY FORWARD from 8.13:
          - EIGHT MEASUREMENTS REFUSED THEIR SECTION'S CLAIM (the 18th to 25th in
            Module 8). Four of them were the engine's OWN first drafts caught by
            their controls: stepping a cast from the upper bound, clipping the
            remainder, a tangent-plane snap, and a recover() that let the solver
            move the character. Keep writing the first draft as a control arm.
          - INSTRUMENT ERRORS THIS TIME: reading a quaternion's angle with
            2 atan2(v, w) across a full turn (q = -1 reads +-360); a "last
            second" window that caught the tail of an approach and read it as
            jitter; an x-reach initialised to 0 when the quantity is negative;
            a GJK re-run WITHOUT the cast's warm start used to explain the
            cast's own stop; and a gap measured with the same loose GJK as the
            thing it was judging (the reference now runs GJK at 1e-7).

  RE-PLANNED 2026-09-27 (post-Module 8 review, at the user's direction):
    INSERTED, as b-lessons in their home modules (planned rows, not written):
      6.17b Local Lights: Point and Spot, and Their Shadows        ~10 h
      6.18b Compute Shaders: GPU Particles                          ~10 h
      7.7b  Animated Characters from glTF: Skins and Clips          ~8 h
      8.14  Scene Queries and a Static Mesh Collider                ~11 h
    MODULE 9, 13 lessons (was 11; nothing in it was published):
      9.1  Hardening the Build: Exceptions Off, Warnings as Errors  (new)
      9.2  A Testing Strategy for Engine Code                       (was 9.9)
      9.3  Multithreading: Data Hazards and Safety Rules            (was 9.1)
      9.4  A Job System                                             (was 9.2)
      9.5  CPU and GPU Profiling: Case Studies                      (was 9.3)
      9.6  Custom Allocators: Arena and Pool                        (was 9.4)
      9.7  Serialization and a Scene Format                         (was 9.5)
      9.8  Hot Reloading                                            (was 9.6)
      9.9  The Editor: Hierarchy and Inspector                      (was 9.7)
      9.10 Transform Gizmos                                         (was 9.8)
      9.11 Packaging and Distribution                   (was 9.10, first half)
      9.12 Documentation: Public API Reference and a Docs Site  (9.10, second)
      9.13 Capstone: A Complete Game on the Public API              (was 9.11)
    WHY THIS ORDER: hardening first because §4's no-exceptions/no-RTTI rule was
    never enforced by a flag and CI v1 stops short of -Werror and shaders;
    testing second so the rest of Module 9 is written against tests; the four
    insertions before all of it because the editor, gizmos and capstone lean
    on them. Totals: 10 modules, 113 lessons, ~772 h (planned rows keep
    placeholder hours; each is measured by estimate-hours.py when it lands).
    Insertion protocol: docs/_template/README.md §17.
    LANDED: 6.17b 2026-09-28 at 12 h; 6.18b 2026-09-29 at 15 h — Module 6 complete
    again, the course at ~779 h. Both came in over the placeholder, 6.18b by half: an
    insertion's plan row should be read as a floor. 7.7b (~8 h) and 8.14 (~11 h) remain.
```
