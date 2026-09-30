# Decisions — standing policies and why

Moved verbatim from STATE.md's `decisions:` block on 2026-09-27, when that file became
a compact resume key (CLAUDE.md §9). Append here; keep STATE.md's headline in step.

```text
decisions:
  pending-code-corrections: THREE ENGINE COMMENTS ARE WRONG (TWO IN gpu_scene.cpp,
        ONE IN frustum.cpp), AND THE NEXT LESSON THAT LISTS EACH FILE WHOLE CORRECTS IT.
        (Added 2026-09-27, while writing Exercise 6.8.4's solution.) Both six-slot
        SDL_BindGPUFragmentSamplers calls in gpu_scene.cpp — in `render` and in the
        instanced path — carry a comment saying a partial bind "REPLACES the range
        it names", leaving slots outside it unbound. SDL release-3.4.12 says
        otherwise: METAL_/VULKAN_/D3D12_BindFragmentSamplers each write only
        firstSlot + i, and what DOES reset every binding is EndRenderPass (all
        three backends SDL_zeroa their binding arrays; the debug layer asserts
        "Missing fragment sampler binding!" at draw time). Binding all six
        together stays correct and costs the same call, so no behaviour changes —
        only the reason. The prose was corrected in place (6.8's pitfall row and
        Exercise 6.8.4, 6.15's paragraph); the CODE comments follow
        shipped-lesson-fixes, because changing HEAD without a page that lists the
        file would break check-continuity R1. 6.17b (local lights) will touch
        gpu_scene.cpp and is the natural carrier.
        THE THIRD (added the same day, from Exercise 6.16.2): engine/src/gfx/
        frustum.cpp's near-plane comment says copying OpenGL's row2 + row3 "lands
        the near plane at the FAR plane's distance behind the camera, so nothing
        within far units of the eye is ever culled". The algebra says the plane
        lands at nf/(2f - n) = 0.150, half the near distance, IN FRONT; everything
        behind is still culled and the picture does not change. The code (row2
        alone) is right; only the comment's account of the wrong version is not.
        Prose corrected in place (6.16 §3.2's callout and its pitfall entry).
        UPDATED 2026-09-28 (6.17b): BOTH gpu_scene.cpp COMMENTS CORRECTED. 6.17b
        lists gpu_scene.cpp whole and rewrote the bind as bind_fragment_samplers
        (eight slots now), whose comment tells the corrected story; the instanced
        path calls the same function and says so. Only frustum.cpp's remains.
  found-by-77b: A REAL EXPORTER AND THE SPECIFICATION REFUTED THREE 7.6 CHECKS, ONE 6.6 SENTENCE AND TWO OF 7.7's FOUR PREDICTIONS; ALL CORRECTED IN PLACE, WITH MARKED NOTES ON THE EARLIER PAGES.
        (2026-09-30.) Found by importing a Blender-authored character and by reading the glTF
        2.0 specification, and fixed the same session:
        1. 7.6 skeleton.hpp/.cpp: `out_of_order` "composes against last frame's value — a
           one-frame lag" — compose_pose's guard makes the child a ROOT on every frame
           (verify_77b §C). Comments corrected in the 7.7b listings; 7.6's page §4.3 and its
           pitfalls row carry "Corrected after Lesson 7.7b" notes.
        2. 7.6 skeleton.cpp: `nonuniform_binds` tested exact equality; Blender's decomposition
           noise (spread 8.94e-06) tripped 12 of 20 joints. Now a 1e-4 ratio
           (k_uniform_scale_tolerance, skeleton.hpp); a 1.5:1 stretch still counts (§I).
        3. 7.6 skin.hpp: k_weight_tolerance 0.02 argued that byte weights "can miss by up to
           4/255 before anyone has done anything wrong"; the spec says normalised byte/short
           sums MUST be 255/65535. Now 1e-5 (above float noise 8.57e-08, below 1/65535).
        4. 6.6 gltf.hpp + page §4 + conventions §6-interop: "a model dropped into the world
           unrotated presents its BACK" — it FACES a camera looking down -Z (nose z = +0.121,
           §J); forward differs, not front. Corrected in gltf.hpp's comment and the living
           Conventions page; 6.6's page carries a callout.
        5. 7.7 exercise 5's hints: (2) "nodes that are not joints ... must be dropped" (they
           must be carried: §B moves a metre) and (4) "a 180 deg error about a diagonal axis"
           plus "expect sign_flips to be non-zero" (111-180 deg, not one rotation; 0 flips on
           Blender's file). 7.7's page carries a correction paragraph under the exercise.
        Following 8ff0388's precedent (a marked note on the earlier page, the story told in the
        later one) — a departure from `shipped-lesson-fixes`' letter, flagged to the user.
        ALSO FOUND, NOT FIXED: verify_85 and verify_86 do not compile at HEAD (8.7 added
        `face_fn` to the convex struct; their aggregate initialisers were never updated). And
        a tooling note: make_t77b.py `carry` is not idempotent — a second run refuses on its
        first file and writes nothing, which is the safe failure, but run it once.
  found-by-618b: 6.18b CORRECTED ONE SHIPPED CLAIM (blend_add "commutative and associative")
        IN FOUR PLACES, GAVE MODULE 6 THE PROJECT TREE IT NEVER HAD, AND FOUND THE 5.12 LINT
        WAS NEVER CARRIED INTO MODULE 6's PINS. (2026-09-29.)
        1. blend_add's doc comment (gpu_pipeline.hpp, since 6.13) called the add
           order-independent because addition is "commutative and associative", and 6.13's
           page said a UNORM target "stops being associative in practice" — implying a float
           one is. Floating-point addition does not associate. verify_618b §K drew the same
           additive sparks in two orders into the half-float HDR target: ~7,000-9,000 of
           172,800 channels differ, by up to 8-17 ULPs, total light equal within 1e-4. The
           no-sort argument needs only commutativity and stands; a golden image of additive
           geometry needs a tolerance. CORRECTED in HEAD and in the 6.13, 6.14 and 6.16 pins
           (every page that lists the file whole; HEAD was byte-equal to 6.16's pin), with a
           "Corrected after Lesson 6.18b" callout on 6.13, whose bullet list is reworded.
           This follows commit 8ff0388's precedent (the pins carry a correction, the page
           says so), NOT shipped-lesson-fixes' "earlier pins are not edited": that policy is
           about CODE a later lesson fixes, where the buggy listing is history; a false
           sentence in a comment is not history worth keeping. Say so if it should be.
        2. MODULE 6 NEVER HAD A PROJECT TREE (§8: "every module ends with a full annotated
           project tree"); 6.18 ended the module before the review and did not carry one.
           6.18b, now the last lesson, carries it on its Recap. 8.13's tree gains 6.18b's
           files. Module 7 has none either: 7.8 is its last lesson and 7.7b is not, so the
           gap stays 7.8's (recorded in next:).
        3. THE 5.12 UMBRELLA LINT (engine.hpp lists every public header, alphabetically,
           exact lines) was never carried into Module 6's pins: at 6.18 engine.hpp had no
           context for a unified-diff hunk. make_t618b.py's insert_includes() places each
           include alphabetically with a line-accounting proof instead. Not fixed in the
           earlier pins — they build, and the lint runs on HEAD.
        4. A stray `\d` in check-builders.py's docstring raised a SyntaxWarning on 3.12;
           escaped.
  found-by-617b: 6.17b FOUND FOUR DEFECTS OUTSIDE ITS SCOPE, AND FIXING THEM FOUND THREE
        MORE; ALL SEVEN WERE FIXED THE SAME DAY, EACH IN ITS OWN COMMIT, TEST FIRST.
        (2026-09-28. The lesson itself fixed none, because an insertion must not move
        another lesson's numbers; the user then asked for the four as separate commits.)
        1. THE SUN'S GPU SHADOW UNDER-REACHED. scene.frag.hlsl's cascade lookup
           sizes its bias with pcf_reach_texels(r) = (r+1/2)sqrt2 — the CPU's
           nearest-texel reach — through a LINEAR comparison sampler, which reads
           four texels: (r+1)sqrt2 is reachable. At the default r = 1 that is 2.12
           where 2.83 is needed, a quarter short. 6.17b found exactly this for lamps
           (11,098 GPU/CPU disagreements off any edge -> 0). Unmeasured for the sun;
           fixing it moves 6.8/6.9's GPU numbers, so it belongs to a lesson that
           re-measures them (Module 9's testing or profiling pass). Page: 6.17b §9.1.
           FIXED 2026-09-28 (own commit), and the measurement refused half the claim.
           verify_617b §K (new): a bare ground under the sun, nothing in the map, every
           pixel darker than the unshadowed frame counted as acne, at 20/45/63.4/75°.
           ONE TAP: 17,821-23,756 of 65,536 pixels (27-36%) with the old reach, 0 with
           (r+1)sqrt2. 3x3 (THE DEFAULT): 0 with EITHER — "a quarter short" predicted a
           symptom the default does not have, which is how it survived. Fix:
           gpu_pcf_reach_texels() in gpu_shadow.hpp, written by fill_uniforms; verify_68
           §G's check updated. All 26 Module 6–7 harnesses: no number moved but timings.
           6.17b's §9.1, manifest and pitfall retell it; 44 checks now.
        2. 6.8's FALLBACK far_depth_ IS A 2D TEXTURE IN A Texture2DArray SLOT
           (gpu_scene.cpp create_depth, slot t2 declared Texture2DArray since 6.9).
           Renders correctly on Metal here; ⚠ VERIFY under MTL_DEBUG_LAYER=1 and the
           Vulkan validation layers. One-line fix (create_depth_array(..., 1 layer));
           posed as Exercise 6.17b.5.
           FIXED 2026-09-28 (own commit). Reproduced first: under MTL_DEBUG_LAYER=1
           Metal reports "incorrect type of texture (MTLTextureType2D) bound at Texture
           binding at index 2 (expect MTLTextureType2DArray) for shadow_map[0]" —
           twelve times in verify_617b. far_depth_ is now create_depth_array(..., 1
           layer); zero reports, 40/40, and all 26 Module 6–7 harnesses unchanged but
           for timing. gpu_scene.cpp's 6.17b comment ("Metal tolerates the mismatch")
           was wrong and is corrected; 6.17b's page tells the story in §10.8 and
           Exercise 5 is now the wide-spot case (item 6).
        5. FOUND WHILE FIXING 2, OPEN: the frame graph creates a pooled depth texture
           with layers == 1 as a plain 2D texture (frame_graph.cpp,
           `r.desc.layers > 1 ? create_depth_array : create_depth`), so a sun shadow
           map POOLED by the graph hits the same mismatch — verify_617's pool frame
           reports it twice under validation. Its home is frame_graph.cpp, last listed
           by 6.18; not in the four fixes asked for.
           FIXED 2026-09-28 (own commit). The files are in fact last listed by 6.17,
           not 6.18 — check-continuity said so. fg_texture_desc gains `array` (+
           is_array()): a one-layer depth array. AND a second fault found beside it:
           pool reuse compared a slot's size/format/samples with the request's and
           took layers, depth and sampled FROM THE REQUEST, so they always matched;
           the pool now records each slot's creation descriptor (pool_desc_, public
           pool_desc()) and judges reuse against it. A colour array request is a
           named error. verify_617 (now tracked): its pooled shadow desc sets array;
           a new test shows a 2D and a one-layer array of the same size on separate
           slots, each created as asked. Validation reports 2 -> 0. All 26 harnesses:
           no number moved but the new checks. 6.17's page carries the corrected
           listings and a "Corrected after Lesson 6.17b" callout in §7.3.
        6. OPEN: a spot wider than k_max_spot_shadow_angle (80°). The GPU calls the ring
           beyond the map lit; the CPU clamps to the edge texel — and shadow.cpp's
           comment says that clamp only touches points the cone has zeroed, true only
           up to 80°. Now Exercise 6.17b.5.
           FIXED 2026-09-28 (own commit), properly rather than quickly: a shadowed
           spot wider than k_max_spot_shadow_angle is shadowed through a CUBE, from the
           point budget, with its cone still masking — shadows_through_cube() in
           shadow.hpp, asked by local_shadow_set and gpu_local_shadows::prepare; the
           record's shadow_normal.y = 1 selects the shader's cube lookup (by the
           SHADOW's kind, not the light's). verify_617b §M (new): before, 1 map, 304
           judged points leaking behind the box, 69 CPU/GPU disagreements > 2 px from
           an edge; after, 6 maps and 6 passes, 0/0 of 163,128 judged, 0 far. All
           26 Module 6–7 harnesses unmoved; Metal validation clean. Exercise 6.17b.5
           is now "skip the faces a wide cone cannot reach" (saves at most 1 of 6).
        7. FOUND WHILE FIXING 1, OPEN — AND THE LARGEST: A SINGLE SUN MAP WITH NO CASCADE
           BLOCK DRAWS NO SHADOW AT ALL. The sun's lookup reads its texel size and depth
           range from the CASCADE block (6.9); when render() gets no cascades,
           gpu_scene_renderer pushes a fallback with world_per_texel = depth_range =
           1.0, so the slope-scaled bias is ~reach*tan(theta) in DEVICE units — larger
           than the whole depth range. Measured with the §K rig: a box's shadow at 45°
           is 792 px with a one-cascade block and 0 px without. demos/sandbox draws
           exactly this way (gpu_shadow_map + fill_uniforms, no cascades), so its GPU
           shadows have been invisible since 6.9; no harness renders that path.
           Likely fix: the fallback copies light.shadow_texel/shadow_depth_range, which
           fill_uniforms already writes. Home: gpu_scene.cpp (last listed by 6.17b).
           FIXED 2026-09-28 (own commit), exactly so. verify_617b §L (new): the same
           frame with a one-cascade block and with none — before: 528/792/1,240 px of
           box shadow vs 0/0/0 at 20/45/63.4°; after: identical, 0 channels differ.
           All 26 Module 6–7 harnesses: no number moved but timings. 6.17b §9.1 tells
           it; 46 checks now. demos/sandbox --gpu itself was not looked at (interactive
           only); §L makes its exact call.
        3. 6.8's HARNESS DOES NOT BUILD AT HEAD: scratch/verify_68.cpp fails in
           engine/math/quat.hpp (a mat3 assigned to a quat), and it, 6.9's and 6.16's
           include <engine/gfx/bounds.hpp>, which 8.4 moved to engine/math/.
           scratch/_base617b/run_shim.sh forwards the include (6.9 and 6.16 then
           build and pass); 6.8's still fails. Harnesses are not covered by any
           checker — check-builders covers pages only.
           FIXED 2026-09-28 (own commit): the include moved to <engine/math/bounds.hpp>
           in all three, and 6.8's plane now sets its rotation with
           quat_from_rotation(mat3{...}) — the matrix it always had, converted. 6.8:
           53/53; 6.9 and 6.16 give the same verdicts as their shimmed runs. The
           repaired harnesses are TRACKED (their pins stay the 6.8/6.9/6.16-era
           listings); the shim is deleted.
        4. 6.5's HARNESS REPORTS 2 FAILURES AT HEAD, before and after 6.17b alike:
           "the whole material is 64 bytes" and "material_uniforms is still exactly
           32 bytes" — sizes later lessons grew on purpose; the checks were never
           updated. Same class as 3: harness rot nothing measures.
           FIXED 2026-09-28 (own commit): the material check is now "within one
           64-byte cache line" (the claim was always "small enough to copy"; 6.7,
           6.10 and 6.11 grew it to exactly 64), and the uniform check is 48, the
           number gpu_uniform.hpp itself static_asserts since 6.11. 27/27. The
           repaired harness is tracked; its pin stays as 6.5 shipped it.
  shipped-lesson-fixes: *** A LATER LESSON THAT FIXES AN EARLIER LESSON'S ENGINE
        CODE SHIPS THE FIX IN ITS OWN LISTINGS AND TELLS THE STORY; THE EARLIER
        PAGE STAYS AN ARCHIVE OF WHAT SHIPPED. *** 8.10's tangent basis set the
        precedent; 8.12 applied it twice (8.11's position pass, 8.10's
        kinematic wake). The earlier lesson's pins are NOT edited — its page
        shows the buggy code with the comment that was right about intent.
        THE EARLIER HARNESS IS RERUN AGAINST THE FIXED ENGINE and the new
        lesson reports exactly what moved (verify_811: one table, 0.0753 ->
        0.0750; verify_810: timings only). If an earlier page's prose quoted a
        moved number, say so on the new page; here 8.11 says "7.5%", still true.
  quat-swap-deferred: THE FIELD IS CALLED `rotation` AND ONE CALLER DOES NOT PUT
        A ROTATION IN IT. 7.4. transform.hpp has promised since Module 2 that
        `transform::rotation` becomes a quaternion and that "the swap touches
        one line of parent_from_local() and nothing else". That sentence was
        written in Module 2 and was wrong by Module 5.
        engine/src/gfx/renderable.cpp:54 builds a `transform` out of an ECS
        `world_transform` with `.rotation = linear_of(w.matrix)` and
        `.scale = {1,1,1}`, and its comment says — CORRECTLY, today — that the
        recomposition reproduces the original affine matrix TO THE BIT. It does,
        because a mat3 will hold anything, INCLUDING the scale that came down
        the hierarchy from a parent. A quaternion will not.
        MEASURED (§H), and the truth is worse than "the scale is dropped":
          uniform scale 2   -> |q| 1.3337, det 1.6531, pose 28.1519 deg wrong
                               (NEITHER 8 NOR 1 — no clean interpretation)
          non-uniform 0.4y  -> the pose itself MOVES by 1.2089 deg
          decompose first   -> column lengths off, then extract: 0.000e+00
          shear 0.30        -> column lengths all 1. INVISIBLE to both routes.
        So the repair is a DECOMPOSITION, not a rename, and the swap is ~25 call
        sites across ten files with that decision inside one of them. 7.5 makes
        it, with quat_slerp in hand so the moved call sites get something back.
        THE TRANSFERABLE POINT: a narrower type does not only prevent future
        mistakes, IT FINDS EXISTING ONES. That assignment has been correct,
        tested and shipping since 5.11, and the "to the bit" guarantee in its
        comment — which reads like a strength — was the symptom.
  fill-shot: A RENDER FIGURE KEEPS THE PROGRAM'S OWN BACKGROUND. 7.4 added one
        rule to docs/shared/course.css. Every screenshot figure before it sat on
        `.fill-soft`, which is `--dia-fill` and near-WHITE in light mode. Fine
        for a lit solid; catastrophic for a thin bright line, and Module 7's
        demos draw gold and teal on near-black because that is where those
        colours read. 7.4's double-cover figure had its GOLD DIAL HAND — the
        entire content of the figure — at about 1.8:1 against the pale panel.
        ONE VALUE, NOT TWO, deliberately: it is a photograph of a program, and
        the program's background does not change when the reader flips themes.
        THE ALTERNATIVE WAS CONSIDERED AND REJECTED: darkening the demo's gold
        to suit the page would have made the figure disagree with the program,
        which is the failure 7.3's "transcribe the constants" rule exists to
        prevent, one level up.
        The two 7.3 render figures were NOT retrofitted — out of scope for a
        lesson with no other business in that page. Candidate for 9.10.
  builder-byte-count: 46 OF 47 BUILDERS PRINT A CHARACTER COUNT AND CALL IT
        BYTES. `print(f"... ({len(page):,} bytes")` on a UTF-8 page full of
        × − ° ₁ is short by 2,405 on 7.4's page. Harmless (nothing consumes it)
        and wrong, which wastes twenty minutes the first time somebody diffs it
        against `wc -c`. build_74.py prints `len(page.encode('utf-8'))`. The
        other forty-six are a one-line sweep for 9.10 rather than something a
        lesson with no business in them should touch.
  - 5.7 CHOSE THE SPARSE SET, AND THE ARGUMENT IS NOT THE QUERY RATIO. Four
    reasons in weight order:
      1 THE MIGRATION ONLY RUNS ONE WAY. A group (EnTT's term) sorts two pools
        into a common dense order so index i means the same entity in both; the
        query then reads NO sparse entry and is walking a byte-identical archetype
        chunk. Measured 0.99-1.01x at every size, on the archetype's BEST case.
        The reverse does not exist at any price.
      2 THE COST ACCEPTED IS BOUNDED; THE COST AVOIDED IS NOT. Archetype's query
        win is <=1.94x, only above 1e4, only when selective, only on ungrouped
        arms. Its structural-change loss is 13.8x and GROWS WITH EVERY COMPONENT
        TYPE THE COURSE ADDS — Module 6 materials, Module 7 skeletal state,
        Module 8 rigid bodies and colliders. This entity passes 12 components in Modules 7–8.
      3 NOTHING AT THIS SCALE IS ON THE TABLE. Below 1,000 entities every arm is
        within 16% and most within 5%. The demos have four objects.
      4 IT IS A QUARTER OF THE CODE (20 lines vs 100 in the probe) AND THIS IS A
        COURSE. Admissible only as a tiebreaker, AFTER 1-3 decided it.
    WHEN IT IS THE WRONG ANSWER, in one sentence: a world past ~1e4 entities whose
    COMPONENT SETS ARE STABLE and whose SYSTEMS ARE NARROW (little arithmetic per
    entity, so nothing hides the redirect) — a streaming open world, a crowd, a
    million-agent boid sim. That is Unity DOTS's shape. Even then the fix is to
    group two or three pools, not to rewrite the storage.
    THE STRONGEST UNMADE ARGUMENT FOR THE OTHER SIDE, stated because it is real:
    BATCHED structural change. Moving a thousand entities between two archetypes
    at once should cost far less than a thousand single moves (the destination
    appends become one memcpy), and DOTS defers changes to a command buffer for
    exactly this. 5.7 measures the operation one at a time and says so; that
    experiment is exercise 10.5, and it is what would change the answer.
  - 5.7 REFUSED TO LET pool<T> DECIDE IT, and the refusal is the method. 5.4's
    pool IS a sparse set (slots_/items_/owners_ = sparse/data/dense, plus
    generations) and noticing that is worth real credit — but adopting sparse
    sets BECAUSE we own one is choosing an architecture by an accident of what
    meshes needed in 5.4. Convenience is admissible as a tiebreaker and
    inadmissible as evidence. What pool<T> actually lacks is the thing 5.8 must
    add: ONE shared id space, minted once, instead of each pool minting its own.
  - 5.7 GAVE THE ARCHETYPE THREE CONCESSIONS AND SAYS SO, so every archetype
    number is an UPPER BOUND on a real one: statically typed columns (no
    per-archetype column lookup, no type-erased stride the compiler cannot see),
    a query that genuinely skips non-matching chunks, and a fragmentation case
    that is measured rather than assumed to be a disaster.
  - 5.7 CHANGED NO ENGINE CODE, ON PURPOSE. Not one file under engine/ or demos/,
    no CMake change, golden byte-identical for the SEVENTH lesson. The output of
    a design lesson is a decision; one that quietly rewrote the renderer on the
    way past would be a different lesson.
  - 5.6 CHANGED NOTHING IN THE RENDERER, ON PURPOSE, and the data is the reason:
    the scene is FOUR objects, 384 bytes, every layout within noise, and the whole
    per-object transform work for a frame is ~6 ns. A measurement lesson has to be
    willing to conclude "no".
  - 5.6 JUSTIFIES EXACTLY THREE THINGS ABOUT THE ECS, each with a number:
      storage must be DENSE AND COMPACTABLE — not because contiguous is virtuous
        (ptr_ordered is 1.00x to 10k) but because a long-lived scene DECAYS into
        disorder, and the difference between an array you can compact and a tree
        you cannot is 6.47x at the scale the decay happens. 5.4's swap-and-patch
        is justified here, AFTER the spend, which is the honest order to admit.
      a system must DECLARE WHICH COMPONENTS IT TOUCHES — 60 of 96 bytes buys
        nothing, 12 of 96 buys 5x.
      iteration MUST NOT BE VIRTUAL — a flat 1.5-1.7x at every N, so it is the one
        cost that applies to the four-object scene too.
    AND IT EXPLICITLY DOES NOT SETTLE ARCHETYPE vs SPARSE SET. Both are dense,
    contiguous, and allow a subset. That is 5.7's question and must not inherit
    5.6's numbers.
  - 5.6's THREE UNEXPECTED RESULTS, all of which survived a correction:
      SoA's full-transform win is VECTORISATION, NOT CACHE. Identical at 384 B and
        9.6 MB, and 1.00x at every size under -fno-vectorize. The cull win is the
        cache in isolation: 0.97x below the knee, 0.37x above it, scalar.
        THE RULE IS NOT "USE SoA": split the data a loop does not read away from
        the data it does, and the prize is the fraction you leave behind.
      A POLYMORPHIC VIRTUAL CALL COSTS THE SAME AS A MONOMORPHIC ONE (within 4% at
        every N), so it is not misprediction; being flat across N it is not the
        vtable load; what is left is THE INLINING IT PREVENTS.
      THE FRAGMENTATION THE BENCHMARK ARRANGED IS NOT CONTROLLABLE AT ALL. 24-byte
        spacers between 96-byte objects go in a different allocator size class and
        separated nothing (497 of 499 objects exactly sizeof(scene_object) apart);
        matching the sizes moved ptr_ordered@100k from 1.09x to 1.98x and LOOKED
        like the fix. It is not. The SAME layout code gives 0 adjacent in the -O2
        benchmark at every n, 99,411/99,999 in the -fno-vectorize one, 0 in a
        standalone probe at n=100k and 485/499 at n=500. ADJACENCY IS A PROPERTY OF
        THE PROCESS, NOT OF THE LAYOUT — macOS interleaves same-size blocks or not
        depending on that size class's magazine state, which depends on everything
        allocated earlier.
        RESOLUTION IS A PROTOCOL, NOT A FIX: bench_56 REPORTS ITS OWN ALLOCATION
        LAYOUT at every n (an `alloc` row), measure_56.py flags any build whose
        objects came out contiguous, and verify_56 asserts only what the code
        controls (three spacers per object) and PRINTS the adjacency. A NUMBER
        WHOSE PROVENANCE YOU CANNOT STATE IS A NUMBER YOU SHOULD NOT PUBLISH.
        CONSEQUENCE FOR THE TABLES: the -O2 table is sound throughout (0 adjacent);
        from the -fno-vectorize table ONLY the soa/flat rows are quoted, because
        neither touches the heap per object.
  - 5.6 FIXED A CONFOUND THAT HALVED A HEADLINE: the accumulator read 5 of a
    matrix's 16 entries, so every INLINED arm could skip work the virtual arm had
    to do. virtual read 3.4x; reading all 16 it reads 1.7x. A benchmark's dead-code
    elimination is not symmetric across arms that differ in inlinability.
  - 5.6 REPLACED AN ASSERTION THAT WAS ASSERTING AN ACCIDENT. verify_56 §D claimed
    "at least one reordering arm is not bit-identical" and failed: at n=2000 all
    three landed on the same bits. Whether a reordering differs is a property of
    the NUMBERS. Replaced with a four-line demonstration that (a+b)-a != (a-a)+b
    at 1e16, which is true regardless of the data.
  - 5.5 REFUSED REFERENCE COUNTING, from 5.4's properties rather than from taste.
    See conventions:assets. The general form of the question is never "should this
    be refcounted?" but "what does this reference have to be able to do, and does
    a count take that away?" — gpu_texture's RAII wrapper is a count of one and is
    exactly right, because it is already an object with a lifetime.
  - 5.5 KEPT engine::asset_path RATHER THAN DELETING IT, and reimplemented it over
    search_path. Five verification harnesses from Modules 3-4 call it and they are
    correct; breaking five working tests to remove two lines is a bad trade. What
    matters is ONE IMPLEMENTATION, not one spelling. New code takes an asset_store.
    Its behaviour improved on the way: it now returns where the file ACTUALLY IS,
    searching every root, rather than where it would be under the first.
  - 5.5's KEY-SPACE BUG, found by verify_55 §D one line after the check that
    stored the asset: insert_mesh("cube") stored "cube" while find_mesh("cube")
    looked up "cube|flip=1". Two key spaces wearing one name, silent, no crash and
    no log. FIX AND GENERAL RULE: THE DEFAULT CONFIGURATION MUST SERIALISE TO
    NOTHING — same shape as 5.4 reserving generation 0 so a zeroed handle is null
    for free.
  - 5.5's DOUBLE-LOG BUG, found on the first smoke run: a refused name produced
    TWO error lines, search_path's (which says why) and asset_store's (which says
    less) — the exact double-reporting 5.3 forbade, reintroduced by the lesson
    after it. resolved_path::refused is the fix. A rule you wrote down is not a
    rule you will follow; it is a rule you will NOTICE BREAKING.
  - 5.5 COLLECTS THEN RECURSES in release_mesh. Walking mesh_derivations_ while
    the recursion erases from it is correct for every mesh with ONE dependent,
    which is every mesh the demo has. The worst kind of bug: correct in every test
    you would think to write.
  - 5.5's HONEST LIMITS, both recorded in the header: derivations are MESH -> MESH
    only (a texture and its mipmaps need the edge to carry kinds — Exercise 9.5),
    and unloading is IMMEDIATE rather than deferred to a frame boundary, so it is
    safe between frames and Exercise 9.4 shows exactly what happens during one
    (collect_triangles resolves once per object and holds the pointer for the
    object's duration — the handle was fine, the resolved pointer was not).
  - 5.5's COUNTERS GO TO THE LOG, NOT THE HUD, and it is a layout fact rather than
    a design one: the software HUD's rows are full at 14 px spacing and a number
    squeezed into a row another lesson owns is a number nobody reads. Printed on
    every acquire and every unload, which is when they change.
  - 5.4 CONVERTED ONE SUBSYSTEM, MESHES, and deliberately left the rest. 5.3's
    residue (61 bool-returning functions, three status enums) is STILL open and
    was not folded in: handles are a big enough idea on their own and the ECS
    (5.7-5.8) depends on getting them right rather than soon. Textures, shaders
    and GPU resources convert in 5.5 with the asset system.
  - 5.4 CHOSE DENSE STORAGE OVER A SLOT-INDEXED ARRAY, at a cost of ~30 lines. A
    slot-indexed array is sparse: iterating walks holes and touches cache lines
    holding nothing, and the waste grows with everything ever destroyed. items()
    returns a contiguous span of exactly the live objects, which is what 5.6
    measures and what 5.8's ECS needs. Order of items() is DELIBERATELY
    UNSPECIFIED — it is a function of your removal history.
  - 5.4 CHOSE LIFO FOR THE FREE LIST, knowing the cost. The warmest slot is reused
    first (cheapest insert), and the price is that hot allocate/free pairs hammer
    one slot's generation, which is the only thing that makes wrap reachable.
    FIFO is the documented fix and is Exercise 9.3.
  - 5.4 REBUILDS REPLACE RATHER THAN OVERWRITE. build_floor/load_model could have
    resolved the handle and assigned over the mesh_data in place, keeping the
    handle valid and the change invisible. Invisible is exactly wrong when
    something downstream caches per mesh: a new mesh gets a new handle, and the
    sandbox cache is the code that benefits.
  - 5.4's HONEST LIMIT, exhibited rather than hidden: verify_54 §D recycles ONE
    slot 4,095 times and shows the ORIGINAL handle resolving to the new occupant.
    12 bits buys the failure down; it does not eliminate it. Stated in the lesson,
    in the header, and counted at runtime.
  - 5.4 LEFT A LIFETIME HOLE FOR 5.5 ON PURPOSE. scene_mesh_cache now holds two
    pools — loaded geometry and geometry DERIVED from it by with_normals — with no
    rule connecting their lifetimes. Free the source and the derived mesh lives on
    keyed by a handle that no longer resolves. Not a bug today because nothing
    frees; exactly the bug 5.5 must be designed against.
  - 5.4 NAMED THE TYPE `handle`, KNOWING IT SHADOWS. A class with a member
    function handle() — gpu_device, gpu_texture, gpu_mesh all have one — cannot
    write handle<T> unqualified: "explicit qualification required to use member
    'handle' from dependent base class". Verified in four lines. The aliases
    (mesh_handle) exist partly for this, and it is Pitfall 2.
  - THE LOGGING LAYER IS 200 LINES BECAUSE SDL'S IS GOOD. Same policy as 5.2's
    platform decision, applied a second time and paying out harder: SDL already
    has priorities, categories, a per-category table, an env override and an
    output hook. What it CANNOT supply is names for our subsystems and a
    compile-time floor, and that is the entire content of log.hpp/log.cpp.
    THE DECIDING EVIDENCE WAS READING src/SDL_log.c: the default table is
    `app=info,assert=warn,test=verbose,*=error`, so custom categories are silent
    by default for free. A wrapper would have had to reimplement that and would
    have got a different answer.
  - `if constexpr`, NOT `#if`, FOR THE COMPILE-TIME FLOOR. `#define X(...) ((void)0)`
    in release means the ARGUMENTS ARE NEVER COMPILED, so a trace call naming a
    renamed variable keeps building in release and breaks in debug — found by the
    person least able to explain it. `if constexpr` with a literal condition
    DISCARDS the statement (nothing in the object file) while still parsing and
    type-checking it. Deletion plus type-checking; measured both ways in §5.
  - ENGINE_VERIFY WAS WRONG THE FIRST TIME, AND MEASUREMENT FOUND IT. v1 was
    `#ifdef NDEBUG ((void)(expr)) #else SDL_assert(expr)`. Reads correctly; both
    branches evaluate. But SDL gates on __OPTIMIZE__, so at `-O2` with no
    -DNDEBUG we take the #else and SDL_assert is at level 1 =
    `(void)sizeof(condition)` — an UNEVALUATED CONTEXT. The macro whose entire
    purpose is guaranteed evaluation silently stopped evaluating, in exactly one
    configuration. MEASURED AT 4 BYTES — a bare `ret`. Fix: evaluate into a named
    bool ALWAYS, assert on the value under `if constexpr (SDL_ASSERT_LEVEL >= 2)`.
    16 bytes now. GENERAL LESSON: WHEN YOU GATE YOUR MACHINERY ON A BUILD FLAG,
    FIND OUT WHAT YOUR DEPENDENCIES GATE THEIRS ON.
  - THE 76-SITE CONVERSION WAS DONE BY A WRITTEN RULE, NOT BY HAND.
    scratch/convert_logs_53.py: category from the file's directory (that is what a
    category IS), level from an ORDERED pattern list where "— continuing" beats
    "failed" — which is the error/warn rule in code. It PRINTS its classification
    for every site so review is possible; four were wrong and were corrected by
    hand. 76 judgement calls made at 76 different moments is how a level system
    loses its meaning before it has one.
  - `--trace` RAISES gpu=info, IN ONE LINE, and without it the flag would have
    silently done nothing — worse than no flag, because it looks like the feature
    is broken rather than the logging. Any diagnostic switch whose output lives on
    a category must raise that category.
  - MEASURE IN FOUR CONFIGURATIONS, NOT TWO. -O0 / -O0 -DNDEBUG / -O2 /
    -O2 -DNDEBUG, per-function code size read out of the object file plus the
    undefined-symbol list. Columns 2 and 3 are exact inverses and neither is
    guessable. The undefined-symbol list is the stronger claim: at -O2 -DNDEBUG
    the object does not reference SDL_LogTrace, so the calls did not become cheap,
    they stopped existing.
  - THE HARNESS REPORTS WITH std::printf, NOT WITH ENGINE_LOG_*. A harness that
    reports through the machinery it is testing cannot report that the machinery
    is broken. verify_53 also RUNS IN THREE CONFIGURATIONS and adapts its
    assertions (60 checks at -O0, 59 at -O2) rather than assuming one.
  - THE PLATFORM LAYER ADMITS SDL RATHER THAN HIDING IT, and the argument is
    written into platform.hpp's header comment so nobody re-opens it by accident.
    window() returns SDL_Window*; handle() takes SDL_Event. A WRAPPER THAT HIDES A
    LIBRARY YOU HAVE NO INTENTION OF REPLACING IS COST WITH NO BENEFIT — the
    swap-ability is a benefit we would never collect, paid for with a parallel
    vocabulary (engine::key, engine::event, a window-flags bitfield) that must be
    invented, documented, kept in sync and taxed on every new SDL feature.
    WHAT IT OWNS INSTEAD IS LIFECYCLE AND ORDER. 5.1 flagged SDL-in-public-headers
    as residue; 5.2's conclusion is that it is not residue, it is the decision.
    (The counter-case is named too: a console SDK under NDA, a second backend that
    already exists, or a public API shipped to customers. None apply.)
  - SANDBOX KEEPS ITS OWN main() AND ITS OWN LOOP, DELIBERATELY. It adopts
    engine::platform (31 lifecycle calls -> 3) and refuses engine::app. Three
    reasons, all still true: it runs THREE incompatible loops chosen at runtime
    (run_gpu_scene / run_gpu_probe / software), so one app subclass would mean
    every hook opening with a three-way branch; it is THE INSTRUMENT every measured
    claim in Modules 2-4 was taken with, and an instrument wants its control flow
    visible; and SOMEBODY HAS TO PROVE THE LIBRARY PATH STILL WORKS, or platform
    has no independent users and drifts into being an implementation detail of app.
    THE RULE: use the framework when your program wants A loop; write your own when
    it wants THIS loop. A framework you built is still a framework.
  - OWNERSHIP IS PUBLISHED TO SDL *BEFORE* ANYTHING CAN FAIL. app_runner::init does
    release() -> *appstate = self -> then configure/start/on_start, any of which may
    return SDL_APP_FAILURE. Publishing only on success looks safer and is worse:
    SDL calls SDL_AppQuit "in all cases, even if SDL_AppInit requests termination
    at startup", so a null appstate would have to mean both "never built" and
    "already cleaned up". ONE OWNER, ONE TEARDOWN PATH, and the window in which the
    raw pointer is unowned contains NO BRANCHES.
    on_stop() runs whenever on_start() RAN, whatever it returned.
  - iterate() CHECKS running() TWICE AND THAT IS NOT REDUNDANT. Top: decline to
    START a frame after a quit. Bottom: refuse to ABANDON one — every --shot
    depends on draw-then-write-then-exit. THE TOP CHECK WAS FOUND BY verify_52 §D,
    NOT BY DESIGN: SDL's own loop reads its result atom before calling iterate, so
    the gap could never appear in a running program. A CALLBACK SHOULD BE TOTAL —
    safe to call at any moment, including one SDL would not have chosen — and that
    is exactly what lets a test drive the four functions with no entry point.
  - THE MACRO CONTAINS NO LOGIC. ENGINE_MAIN is four one-line forwards into
    app_runner's four static functions. A macro is code the debugger struggles to
    step through and the compiler reports odd line numbers for, so the only thing
    worth putting in one is the part that cannot be a function: the four fixed C
    symbol names SDL looks up. Consequence: verify_52 calls app_runner::init /
    iterate / event / quit DIRECTLY, no SDL_MAIN_USE_CALLBACKS, no window.
  - app_config's `extra_subsystems` IS "EXTRA", NOT "SUBSYSTEMS". The first draft
    was `SDL_InitFlags subsystems = SDL_INIT_VIDEO`, which would have forced the
    class to SUBTRACT a flag the caller had explicitly set whenever the surface was
    headless — a class quietly overruling its own configuration. Video belongs to
    the surface; the field is for what else (Module 7's audio).
  - PRESENTATION IS TWO CALLS: blit_framebuffer() then present(). Lesson 3.10
    needed the split so the frame budget excludes the vsync block; it turns out to
    be exactly the gap a HUD wants (on_overlay). One seam, two independent reasons.
  - PPM, NOT PNG, FOR EVERY SHOT — now engine::save_ppm in gfx/image.hpp. A test
    artifact should be trivially comparable: ASCII header + raw RGB, no
    compression, no filters, no timestamp, so `cmp` is a valid renderer test. A PNG
    of the same picture can differ in bytes for encoder reasons, which removes the
    cheapest check exactly when it is needed. Writes one buffered ROW per
    SDL_WriteIO (180 crossings at 320x180, not 57,600).
  - MEASUREMENT: measure_52.py STRIPS COMMENTS BEFORE COUNTING API CALLS. Its first
    run reported four remaining lifecycle calls in sandbox and one of them was the
    sentence explaining that SDL_Init is no longer called there. A MEASUREMENT THAT
    COUNTS ITS OWN FOOTNOTES IS NOT A MEASUREMENT.
  - THE ORDERING CONTRACT SURVIVED THE INVERSION; ITS ENFORCER CHANGED. Lesson
    1.2's "drain, then update()" still holds under the callbacks because
    SDL_IterateMainCallbacks() calls SDL_PumpEvents(), then
    SDL_DispatchMainCallbackEvents(), then the iterate callback — three lines of
    src/main/SDL_main_callbacks.c, READ rather than assumed. WHEN A GUARANTEE MOVES
    OUT OF YOUR CODE, GO AND READ THE CODE THAT NOW PROVIDES IT.
  - platform IS NON-COPYABLE **AND** NON-MOVABLE, which is stronger than the rest
    of the engine (gpu_device and framebuffer are movable). It owns PROCESS-WIDE
    state — there is one SDL library — so a second one is a bug and a moved-from
    one is a bug found later. Deleting the move makes both compile-time.
  - THE STRONGEST ARGUMENT WAS A RECEIPT, NOT AN OPINION: verify_46 through 49
    each TRANSCRIBED build_scene by hand because it sat in an anonymous namespace.
    Four copies, none compared to the original by anything. That opened the lesson
    and verify_50 closes it — it LINKS demo_common and reproduces the sandbox's
    picture to the byte (1,209,616 compared, 0 differ). IF A REFACTOR DOES NOT
    MAKE THE HARNESSES SIMPLER IT WAS DECORATION; build_verify_*.sh went from
    eighteen hand-listed sources to one archive, for all five older harnesses.
  - THE EXECUTABLE IS NOW `sandbox`, NOT `engine`. The name belongs to the library
    — to the thing it links — which is the confusion the old name was papering
    over. build/demos/sandbox. Six lessons' run commands change, and the lesson
    says so in a warn callout rather than letting readers discover it.
  - A NAME COLLISION THE SPLIT CREATED: `enum class demo` in the sandbox's
    anonymous namespace HIDES `namespace demo`, so demo::build_scene would not
    compile. The enum was renamed to `screen`. NAMES ONLY COLLIDE ONCE THEY CAN
    SEE EACH OTHER — four modules of private names had never had to be distinct
    from anything.
  - TWO OF verify_50's PROBES WERE WORTHLESS AND BOTH ARE IN THE LESSON.
    correct_normal_matrix=false reported 0 px on the stock scene — 4.8's theorem
    (a box's normals ARE its axes; a diagonal scale sends an axis to a multiple of
    itself; normalize() discards the length), so it needed a SQUASHED ICOSAHEDRON:
    then 454 px. normals=face reported 0 px because the stock meshes carry NO
    VERTEX NORMALS, so both settings fall back to the face normal —
    fell_back = 132 says so. A MEASUREMENT TAKEN ON CONTENT THAT CANNOT EXPRESS
    THE EFFECT IS NOT A MEASUREMENT.
  - A CHECK THAT ASSERTS A KNOWN DEFECT IS NOT A MISTAKE. cull_choice is applied
    in TWO PLACES — collect_triangles reads only back_by_forward, draw_triangles
    applies the rest via fill_style — and verify_50 pins that (`cull = back, in
    collect only -> 0 px, expected 0`). It stops one known defect becoming two,
    and tells whoever fixes it which line to change. Repair: 6.5's material
    system, where cull mode is pipeline state because pipeline state IS what a
    material is.
  - §7 NAMES FOUR THINGS STILL ON THE WRONG SIDE OF THE LINE: cull_choice applied
    twice (-> 6.5); SDL in the public API (-> 5.2); render_options shipping the
    demos' teaching switches, trs_order::tsr and correct_normal_matrix=false,
    which no shipping engine would export (-> exercise 5.1.4, honestly never);
    and the sandbox still at 5,721 lines (-> 5.2, which supplies something to
    split it INTO). A REFACTOR LESSON THAT ENDS "AND NOW IT IS CLEAN" IS LYING.
  - ONE DEVIATION FROM THE ZERO-PLACEHOLDER RULE, STATED IN THE PAGE: the 57 moved
    files are given as a MOVE TABLE plus the exact reproducible command, not as 57
    full listings (>1 MB of near-identical text that would bury the 15 files that
    actually changed). Every genuinely changed file appears whole.
  - A COMMENT THIS CODEBASE HAD CARRIED SINCE 4.3 WAS WRONG, and the correction is
    written to quote the wrong version first. It explained the buffer/texture vs
    shader naming asymmetry by inferring a reason (immutability) instead of
    reading SDL_SetGPUBufferName's own docs, which say to prefer the property. It
    survived four lessons, a full published listing and several readings BECAUSE
    NOTHING DEPENDS ON A COMMENT BEING RIGHT — no compiler, no test, no reviewer.
    Standing rule: be most suspicious of comments that explain WHY SOMEBODY ELSE'S
    API IS SHAPED AS IT IS. Comments about your own code are checked constantly by
    people reading the code beside them; comments about a dependency's design are
    checked by nobody.
  - THE LESSON IS NAMED FOR A TOOL THE AUTHORING MACHINE CANNOT RUN, and §5 says
    so in a callout rather than pretending. The RenderDoc walkthrough is assembled
    from RenderDoc's own Quick Start (panel names quoted); the Xcode walkthrough
    from SDL_gpu.h. Marked ⚠ VERIFY on menu paths. Master prompt §10: an honest
    flag beats a confident fabrication.
  - THE FRAME LOG IS NOT A REPLACEMENT FOR A CAPTURE AND THE LESSON SAYS WHERE IT
    STOPS. Six things it does that a capture cannot (every backend, CI, a
    cross-check against a second counter) and six it cannot do that a capture can,
    all six of which need REPLAY. Writing the small version is justified by the
    macOS reader, not offered as an alternative to the tool.
  - MEASURING INSTRUMENTATION TOOK THREE ATTEMPTS AND ALL THREE ARE IN THE LESSON.
    (1) No noise floor: reported a NEGATIVE cost for adding work. (2) A two-run
    floor: still let -541.7 ns through, because one difference is itself a noisy
    sample. (3) A five-run spread plus a 2x threshold, and then scaling the group
    count until the effect cleared: 183.6 and 182.0 ns from two independent
    estimates. The convergence is the evidence, not either number.
  - THE 4.5 DEBT IS DECLARED UNPAYABLE HERE rather than fudged, and the
    replacement is better than the original would have been: modelling the vertex
    cache turned up that REUSE IS A PROPERTY OF THE INDEX ORDER (2.83x from
    shuffling alone), which is a transferable finding, where the true invocation
    count would have been one number about one GPU.
  - `--trace` EXISTS BECAUSE THE AUTHOR NEEDED IT. A keypress-armed log cannot be
    tested headlessly, and the flag that made it testable turned out to be the
    genuinely useful artifact: a deterministic frame dump that runs in CI and can
    be diffed between commits.
  - THE ENGINE'S FOUR DEBUG GROUPS SHIP ON IN EVERY BUILD, decided by measurement
    (0.004% of a frame) rather than by argument. The frame log does NOT ship on;
    it is armed for one frame, which is how a capture works and for the same
    reason.
  - THE PORT IS AN API CHANGE AND NOT A MATHS CHANGE, and that is the whole
    report card for Modules 2 and 3. Of fifteen pipeline stages: TWO are literally
    the same C++ function called from both sides (parent_from_local,
    normal_matrix — neither knows a GPU exists); EIGHT became fixed-function
    hardware; ONE was translated line for line; TWO were folded into one CPU
    multiply per frame; and TWO could not cross at all (the cull mode and the
    material). Counted, not asserted.
  - collect_triangles / draw_triangles ARE STILL HERE AND ARE NOT DEPRECATED.
    They are the REFERENCE. This project temporarily has two complete renderers
    for one scene, which almost no graphics course has, and the whole audit above
    is only possible because of it. Module 5 keeps them.
  - THE HUD WAS GIVEN UP, and it is named rather than glossed. For eight lessons
    the HUD was how this course showed its working. SDL_RenderDebugText needs a
    renderer; a GPU font needs stb_truetype and a shader; inventing three lessons
    of Module 6 here to print eleven numbers would be the tail wagging the dog.
    The split view [V] is a better instrument than the HUD was, and the log still
    carries every number. Exercise 4.8.5 is the honest way back.
  - THE ONE DRAW PER OBJECT IS NOT A REGRESSION AND MUST NOT BE APOLOGISED FOR.
    3.8 predicted it in the field's own doc comment. §2.1 of the lesson explains
    WHY in terms of the machine (a draw is a launch, not a loop) rather than in
    terms of API limitations, because "the GPU is less flexible" is the wrong
    intuition and leads people to look for a way around it.
  - THE FLOOR'S GENERATED NORMAL POINTS DOWN, and Module 3 never noticed. The
    ground plane's triangles are wound for a viewer underneath it, so
    with_normals() — which takes winding at face value, correctly — produces
    (0, -1, 0) and the floor receives only ambient. Nothing in Module 3 saw it
    because the floor scene there was never LIT: it drew a procedural checker,
    unshaded. A CONVENTION THAT NOTHING CONSUMED WAS NEVER ACTUALLY BEING CHECKED.
    Kept as-is in the harness (the measurement is unaffected — both renderers use
    the same geometry) and written up as a pitfall.
  - THE MESH CACHE IS DELIBERATELY THE WORST POSSIBLE ASSET SYSTEM. Keyed by the
    address of the first vertex, holds eight, never evicts, and needs two extra
    fields to notice that a std::vector rebuilt in place keeps its address and
    changes its contents. Fifth time the pressure has been named (3.2, 3.5, 3.9,
    4.5, here) and the FIRST time it actually hurts. Module 5's handles are the
    answer; letting the pain accumulate is what makes it land.
  - verify_48 CANNOT LINK TO THE DEMO'S SCENE and has to transcribe it by hand —
    build_scene, scene_object, collect_triangles and draw_triangles all live in
    main.cpp's anonymous namespace. Fourth harness in a row to want a piece of the
    demo it cannot have, and the first where the duplication can silently DRIFT.
    Strongest argument the course has produced for Module 5's engine/demo split.
  - THREE HARNESS BUGS, ALL KEPT IN THE LESSON. (1) The software reference had no
    near clipping and skipped every triangle crossing the near plane — the floor's
    near edge is behind the camera, so the GPU "covered" 35,532 pixels the CPU did
    not and the port looked catastrophic. Fixed by calling clip_polygon_near, i.e.
    3.3's code doing 3.3's job: A REFERENCE THAT HAS BEEN SIMPLIFIED IS NOT A
    REFERENCE. (2) The sRGB sweep ran without a depth attachment while its
    pipelines declared one — undefined, and it produced an entirely plausible
    table. (3) The first sweep probed fourteen points across the whole range, none
    near the boundary, and "refuted" a hypothesis that was merely unmeasured. A
    SWEEP IS ONLY EVIDENCE WHERE IT HAS SAMPLES.
  - THE FIRST HYPOTHESIS ABOUT THE ONE-CODE FLOOR WAS WRONG, and the ten minutes
    that refuted it are in the lesson. "Every pixel differs" looked like a
    systematic encode difference; the coarse sweep showed the encoders agreeing
    everywhere; the float-target probe showed the GPU fragment bit-identical to
    shade(); only a fine sweep across 0.0044–0.0050 found the actual gap. MEASURE
    THE THING YOU ARE ABOUT TO BLAME.
  - A NEW SHADER PAIR RATHER THAN EXTENDING mesh.{vert,frag}. The 4.6 and 4.7
    sessions each broke verify_45 by editing a shared shader; scene.* is separate,
    so mesh.* is untouched and verify_45/46/47 all still pass (checked, per the
    standing rule).
  - THE DIFFERENCE IMAGE'S RAMP STARTS VISIBLE (90 + 22*d rather than 24*d). A
    one-code difference scaled linearly to 255 is a pixel nobody can see, and the
    image exists to be looked at. A figure has a job.
  - CHANGING mesh.frag.hlsl BROKE verify_45 FOR THE SECOND TIME, and the standing
    rule caught it. 4.6 gave that shader a uniform block; 4.7 gave it a SAMPLER,
    and a draw with nothing bound to a sampler slot draws nothing. Fixed the same
    way: give the older harness the identity of the new feature — a 1x1 WHITE
    texture, which multiplies the tint by one — so every pixel count in Lesson 4.5
    is still the number that program prints and all six of its figure SVGs
    regenerate byte for byte. RUN EVERY PREVIOUS HARNESS AFTER TOUCHING A SHARED
    SHADER; twice now it has been the shader, not the C++.
  - THE DEPTH COMPARISON FIGURE IS DRAWN UNTEXTURED, and that is a figure decision
    rather than an engine one. A high-frequency checkerboard on both panels makes
    the eye hunt for the difference, and the difference is the whole point. One
    variable at a time, in a figure as much as in a measurement.
  - THE PROBE SHADERS SHARE ONE VERTEX STAGE. depth_probe.frag and
    texture_probe.frag both take its uv output; the depth one declares and ignores
    it, because a fragment stage must declare what the vertex stage sends. A
    second copy of the vertex shader would be a second place to get the corners
    wrong.
  - REVERSED-Z IS MEASURED AND NOT ADOPTED. It touches the projection, the compare
    op, the clear value and every pass that later reads depth — a change to make
    once, deliberately, with Module 6's shadow maps in view. Measuring it now and
    deferring it is better than either adopting it silently or not knowing.
  - THE SCENE FIGURE USES A LOWER CAMERA THAN THE DEMO'S DEFAULT (elevation 0.10
    against 0.42). 4.6's default looks down on the ring and the tori barely
    overlap — which is how 4.5 arranged the scene so the missing depth test would
    not show. A figure about the depth test needs overlap.
  - name_of(SDL_GPUTextureFormat) WAS EXTENDED, NOT DUPLICATED. Writing a second
    one in gpu_texture.cpp produced a duplicate-symbol link error, which was the
    right answer arriving as a build failure: 4.2 already owns that table and a
    lesson that starts printing something adds the row.
  - THE PROBE SHADERS SHIP; THE DELIBERATELY-BROKEN ONE DOES NOT. A space0 variant
    was written to measure "what does a wrong space do", and shadercross REFUSES
    to translate it, so it cannot live in the build. Deleted; the finding is
    recorded, verify_46 §D prints the exact reproduction commands, and the harness
    instead checks the SHIPPED shaders' descriptor sets on every run. A file in
    the repo that does not build is a liability (CLAUDE.md §8 wants whole files).
  - packed_offset IS A constexpr FUNCTION RATHER THAN A COMMENT, and that is the
    general principle: when a rule is simple enough to write as code, writing it
    as code is better than writing it as prose, because a comment describing a
    layout cannot fail. The prose then explains WHY instead of restating WHAT.
  - THE PUSH HAPPENS BEFORE SDL_BeginGPURenderPass, though it is legal inside one.
    A push applies to the COMMAND BUFFER, and putting it outside the pass makes
    that visible rather than implied.
  - THE ASPECT RATIO IS COMPUTED FROM THE LETTERBOX RECT rather than being a
    constant. That is the whole difference 4.6 makes to 4.5's viewport call, and
    it is why the viewport stops being a workaround.
  - NO gpu_uniform WRAPPER CLASS, deliberately, against the pattern every other
    GPU resource in this engine follows. There is no handle, no lifetime and no
    hazard; inventing a class would be ceremony around two function calls.
  - CHANGING mesh.{vert,frag}.hlsl BROKE verify_45, AND FIXING IT PROPERLY MEANT
    GIVING IT THE OLD CAMERA. 4.5's harness draws with those shaders; once they
    wanted a uniform block it got a zero matrix and drew nothing. It now pushes
    EXACTLY the camera the deleted `static const` floats encoded — eye (0,2.3,5.0)
    at (0,-0.8,0), 55 deg, 16:9 — so every pixel count in Lesson 4.5 is still the
    number the program prints (3,696 / 5,076 / 5,027 / 3,126 / 1,990 / 9,944 /
    32,006, all bounding boxes identical, all six figure SVGs regenerate byte for
    byte). A harness that stops reproducing its own lesson's figures has quietly
    become a different experiment. CHECK THE PREVIOUS LESSON'S HARNESS whenever a
    shared shader or header changes.
  - THE HARNESS DOES NOT REPRODUCE THE PUSH-CEILING CRASH. 4.4 settled the policy
    when vertex/fragment shader swapping segfaulted: a test that destabilises the
    process is not a test. Measured in an isolated program, reported in prose,
    exercise 4.6.5 hands it to the student with the same warning.
  - INDEX BUFFERS MOVED FROM 4.6 INTO 4.5, and docs/index.html's 4.6 was retitled
    "Uniform Data and the Matrix Upload". Porting a real mesh without indices
    would have meant deliberately uploading 5x the data and undoing it a lesson
    later; 4.6 has a full lesson in uniforms alone (push vs buffers, alignment,
    the column-major payoff). NO PUBLISHED LESSON WAS RENUMBERED.
  - THE HARNESS RENDERS TO A 16:9 OFFSCREEN TARGET (512x288), not a square, and
    the demo SETS A VIEWPORT, because mesh.vert.hlsl's aspect ratio is a compile-
    time constant. Two ways of making the same assumption true.
  - THE `expanded` MODE EXISTS TO BE MEASURED, NOT USED. It is not dead code and
    it is not a fallback: [7] toggles it live, the harness proves the pictures are
    identical, and the byte counts are the argument for the indexed path. There
    IS one real use — per-face data cannot live on a shared vertex, which is
    exactly what 3.8's flat shading hit on the CPU.
  - THE uv GRID IN mesh.frag.hlsl IS A DEBUGGING DEVICE WEARING A STYLE. Attribute
    2 is fetched and interpolated and nothing samples a texture until 4.7, so
    without it a scrambled uv is invisible. `groove`, not `line`, because `line`
    is a reserved word in HLSL and the parse error points at the semicolon.
  - THREE PIPELINES AT STARTUP RATHER THAN ONE REBUILT ON A KEYPRESS. 4.4
    measured a new state permutation at ~2.4 ms, which is 15% of a 60 Hz frame;
    building all three at load is the same advice the lesson gives.
  - check_layout DOES NOT GATE PIPELINE CREATION. It logs and returns a tally, and
    the caller decides. Every disagreement it finds is a bug, but several of them
    draw a picture, and a lesson about layouts wants those pictures.
  - THE FIRST BOUNDING-BOX ASSERTION IN THE HARNESS CHECKS THAT NOTHING IS
    CLIPPED BY THE FRAME, because "does the scene fit" is a question with a
    numeric answer and screen capture is unavailable on this machine (4.4's
    finding, still true).
  - sample() RETURNS linear_rgb, NOT Uint32 (3.9). A texture is an albedo, an albedo is a
    REFLECTANCE, and a reflectance multiplies a quantity of light — so both sides have to
    be linear. Putting the decode inside the sampler also puts it BEFORE the filter with
    no way to get the order wrong at a call site. MEASURED: black+white blended correctly
    is 0.5 linear (stored 188); blended as bytes then decoded is 0.2139 (stored 128) —
    42.8% of the light. Worst pair over all 65536: (0,255), off by 0.286 of full range.
    THIS IS WHY *_SRGB TEXTURE FORMATS EXIST — hardware decodes in the sampler, for free.
  - THE uv FLIP IS AT IMPORT, NOT IN THE PARSER OR THE SAMPLER (3.9). Resolves the tension
    with 3.5's "a loader stores what the file says" by noticing the parser and the IMPORT
    are different steps. A loader must not alter its input; a pipeline may. Not the
    sampler either: the sampler is SDL_GPU's, and a sampler that "helpfully" flipped would
    be correct here and wrong in Module 4 — the worst place to hide a convention.
    THE FLIP COMMUTES WITH WRAPPING (worst 6.5e-06 over 100000 coords), which is what lets
    it be applied once and forgotten. flip_uv_v is its own inverse, exactly.
  - APPLIED TO EVERY BRANCH OF load_model, INCLUDING `generated`. An import applied to
    everything cannot break 3.5's round trip; an import applied to some things silently
    can. m.data is re-copied from m.generated each load, so nothing accumulates.
  - uv_checker WAS KEPT, and 3.2 predicted it would be deleted. A procedural rule is not a
    worse texture, it is a different thing: no memory, no sampler, and EXACT at any
    magnification because there is no finite grid to run out of. Both one keypress apart
    is the cleanest demonstration of what an image buys and what it costs.
  - `textured` WITH NOTHING BOUND DRAWS MAGENTA while `lit` with nothing bound falls back
    to vertex colours. Deliberately asymmetric: a lit fill with no texture is a legitimate
    configuration; a textured fill with no texture is a mistake with no second reading.
    Magenta beats black — black reads as "unlit" and sends you to the wrong file.
  - "TEXTURED AND LIT" IS NOT AN ENUM VALUE, it is `lit` PLUS A BINDING — so `shading` no
    longer describes a fragment on its own. 3.8 split an enum when one held two questions;
    this is the same pressure and another enum will NOT do, because the combinations are a
    PROGRAM rather than a grid. FIFTH pull toward Module 4's programmable fragment stage
    (3.4 cull modes, 3.6 "fill_style is the wrong home", 3.7 specular, 3.8 per-triangle
    material, now this).
  - raster.hpp NOW INCLUDES texture.hpp AS WELL AS light.hpp. The rasterizer owns coverage,
    interpolation, depth, lighting and texturing — five jobs a real pipeline splits into
    "fixed function" and "whatever the shader says". Shipped deliberately, tracked, paid
    in Module 4.
  - THE HALF-TEXEL TOGGLE IS THE 9th KEEP-THE-WRONG-THING BARGAIN (draw_line_naive 2.1,
    pong swept_collision 1.8, blend_space::encoded 2.4, the w toggles 2.7, trs_order 2.8,
    interpolation::affine 3.2, near_mode 3.3, cull_choice::back_by_forward 3.4, Phong 3.7).
    The most INVISIBLE of the nine: nearest cannot see it at all.
  - THE SAMPLER COMPARISONS NEED NO SECOND collect_triangles, and that is worth naming: a
    sampler is PIPELINE STATE, so changing it changes no geometry. Same fact that lets a
    GPU swap a sampler without re-running the vertex stage.
  - NO IMAGE DECODER (3.9). Decoding PNG is a COMPRESSION problem, not a graphics one, and
    teaches nothing about rendering; stb_image is the approved answer and arrives with the
    asset pipeline. Everything here takes an array of texels and does not care where it
    came from — which is exactly why generating one in memory costs the lesson nothing.
  - THE OBVIOUS BENCHMARK MEASURED THE WRONG THING, and both halves are kept in verify_39
    §I so the trap is visible rather than quietly avoided. Unlit rule-vs-textured reads
    5.04x / 6.80x — but uv_checker encodes NOTHING and textured re-encodes through
    std::pow, three per pixel. Hold the encode constant (all three `lit`) and the real
    numbers are 1.12x for a nearest fetch and 1.41x for bilinear, i.e. bilinear is 1.26x
    nearest. NEVER QUOTE A FETCH COST WITHOUT SAYING WHAT ELSE DIFFERED BETWEEN THE RUNS.
  - THREE OF verify_39's FIRST FIVE FAILURES WERE THE TEST, NOT THE CODE.
    (a) The 1:1 test failed while its CONTROL passed — because this rasterizer samples
        attributes at INTEGER pixel coordinates, so a 1:1 quad needs its uvs offset by half
        a texel (0.5/N .. 1 + 0.5/N). THE HALF-TEXEL QUESTION EXISTS AT BOTH ENDS of the
        pipeline; 2.11 answered one end and 3.9 answers the other.
    (b) Probing clamp at v = 0.5 on an 8-texel image straddles two rows, so the sample was
        a BLEND and comparing it against one texel was meaningless. A test of one thing has
        to hold everything else where it does nothing.
    (c) Comparing torus.obj's uvs against make_torus()'s INDEX BY INDEX reported 141 of
        1225 differing — it was measuring the VERTEX NUMBERING, because load_obj numbers by
        first appearance (3.5) and make_torus by its construction loop. As sorted multisets:
        worst 0.000e+00.
    RULE: when a test fails, check it is asking the question you think it is asking before
    you go looking at the code.
  - A FIGURE MEASURED ON THE WRONG SIGNAL. The half-texel shift first read -0.69 texels
    because it fitted a "step" on the uv grid, which has darkened lines every N/8. Rebuilt
    on a ONE-ROW step image it reads -0.5000 exactly. A measurement needs a signal whose
    shape you can state in one sentence.
  - SVG MARKER IDS ARE NOW NAMESPACED PER FIGURE (e-i-f391, …). Six figures on one page
    declaring the same four ids is invalid HTML, and url(#e-i) resolves to the FIRST match
    — so every figure quietly used figure 1's markers. Byte-identical definitions made that
    harmless and would have made it baffling the first time one figure wanted a different
    arrowhead. 3.7 and 3.8 still carry the old pattern.
  - SWATCH NUMERALS PICK THEIR FILL BY RELATIVE LUMINANCE, not by eye: `.t-inv` (fixed
    white) on dark swatches, a page-local `.t-onlight` (fixed dark) on pale ones. Both are
    theme-INDEPENDENT for course.css's stated reason — the shape underneath is the same
    colour in both themes, so the label must not follow the theme's ink.
  - input lives in src/core/, not src/platform/ — there is no platform layer until
    Module 5, and input is a state cache rather than a device driver. Revisit at the
    Module 5 refactor. Recorded in ARCHITECTURE.md §2.1.
  - src/game/ created in 1.8, three modules before the Module 5 refactor needs it. The
    boundary costs nothing now and decides how hard that refactor is. ARCHITECTURE.md §2.1.1.
  - fill_triangle does NOT cull by winding, deliberately. Module 2 rasterises in
    framebuffer space where the sign is flipped relative to the NDC convention, and a
    fill that silently dropped "backwards" triangles would be indistinguishable from a
    bug. Culling is Lesson 3.4's, made in NDC where "CCW = front" actually means something.
  - the fill rule's -1 bias is folded into the loop's starting value rather than tested
    per pixel, so correctness here costs zero instructions in the inner loop.
  - src/gfx/raster.hpp forward-declares engine::framebuffer rather than including it —
    the physical-design habit from 1.8, now applied by default in gfx/.
  - draw_line stays Bresenham despite MEASURING SLOWER than DDA. Reasons recorded above
    and in the lesson; revisit with evidence, not deference.
  - struct vertex bundles position WITH attributes so that reorientation cannot leave
    them behind. Chosen for correctness, not tidiness: swapping loose coordinates and
    forgetting loose colours produces a triangle of exactly the right shape, in the right
    place, shaded one corner out of step — and it fires for only ONE winding, so a
    spinning triangle looks right half the time and a static test scene may never show it.
  - prepare_fill does NOT orient the triangle, by design. Orientation moves vertices and
    a vertex carries attributes, so only the caller can do it. Everything AFTER
    orientation is mechanical and identical for every fill — that is what gets shared.
    Rule applied: not "never repeat yourself" but NEVER REPEAT SOMETHING SUBTLE.
  - is_top_left promoted from raster.cpp's anonymous namespace to the header. The bias is
    no longer an internal coverage detail once interpolation must undo it; and a rule you
    cannot inspect is a rule you cannot check (the magnifier would otherwise be comparing
    the demo against itself). Same discipline as 2.1's pixel inspector.
  - blend_space::encoded SHIPS, defaulting off. Third time this bargain has been made
    (draw_line_naive 2.1, pong swept_collision 1.8): a failure you can summon with one
    keystroke teaches more than a paragraph describing it. The WRONG option must be
    asked for by name; the right one is what you get by not thinking.
  - rgb3 is a separate type from linear_rgb despite identical layout. Name a type after
    what it IS, not what it is shaped like — reusing linear_rgb for encoded 0..255 values
    would be exactly the confusion Lesson 1.6 exists to prevent.
  - the encode pow is NOT replaced with a LUT yet. Measured (11.2 ns/px), bounded
    (< 0.4 levels for 4096 entries), written down, and deferred — optimising a 232 us
    cost inside a 16.6 ms budget would be optimising by reflex, which is precisely what
    Module 3's profiling lesson teaches against.
  - point()/direction() named constructors instead of SEPARATE TYPES. The
    type-safe design (position and direction as distinct types) does eliminate the
    bug at compile time and some engines do it. Rejected because it roughly doubles
    the maths library's surface — every operation must state which combinations it
    accepts, and some answers are fiddly (position - position = direction;
    position + position is meaningless) — and because the distinction collapses at
    the GPU boundary anyway, where a shader sees four floats and no types. Named
    constructors buy most of the safety for a twentieth of the machinery.
    Exercise 2.7.5 argues the other side honestly; it is a real trade, not a
    settled question.
  - affine(linear, t) provided ALONGSIDE translation(t) * to_mat4(linear). Same
    matrix; the first says WHAT, the second says HOW, and writing the product out
    is one more chance to get the order backwards.
  - xyz() still DROPS w rather than dividing, deliberately, with a comment saying
    exactly when that stops being correct. The perspective divide must appear in
    2.10 under its own name, not turn out to have been hiding inside an accessor.
  - the demo keeps BOTH ways of getting w wrong on keys ([W] and [N]). Fourth time
    this bargain has been made (draw_line_naive 2.1, pong swept_collision 1.8,
    blend_space::encoded 2.4).
  - identity() moved to a STATIC MEMBER on every matrix type. FORCED: a free
    identity() takes no arguments, so mat2's and mat3's could differ only by return
    type. Everything else survived — transpose/inverse/determinant overload on the
    parameter, rotation vs rotation_x/y/z differ by name, scale differs by arity.
    The function with NOTHING to disambiguate it was the one that broke. General
    rule: a zero-argument function cannot be overloaded at all, so make it a static
    member or give it a distinct name while that is still free.
  - mat2's uniform scale(float) REMOVED. Unused, and it would have become a trap
    the moment someone wanted a uniform 3-D scale: scale(2.0f) silently meaning
    "the 2-D one". Component counts are explicit now.
  - mat3::inverse names its elements in WRITTEN notation (m00..m22) BEFORE doing
    anything. The first draft transcribed the adjugate straight into column members
    and had two cofactors using the wrong component — plausible-looking and wrong.
    The rewrite fixes the CLASS of error, not the instance. M*inverse(M)==I over
    300 matrices is what caught it.
  - the demo cube is CENTRED on the origin. Rotation is always about the origin, so
    a corner-at-origin cube would orbit rather than spin. That is the "choose
    coordinates where the pivot is already the origin" workaround — what asset
    pipelines really do, and the cheap half of Exercise 2.5.3.
  - mat2 stores two vec2 COLUMNS rather than float[4]. The columns are the images
    of the basis vectors — the whole lesson — so the type makes the idea structural
    and gets column-major layout for free rather than by decree.
  - no constructors on the maths types, deliberately: default member initialisers
    give a safe default AND keep the struct an aggregate, preserving brace init,
    constexpr, and a layout guaranteed to match what it looks like — which matters
    the moment Module 4 uploads one as raw bytes.
  - at(row,col) takes ROW first even though the lookup goes to the column first. It
    exists so code can be read against a written derivation without transposing in
    your head. No bounds check: indices are literals at every call site, a DIFFERENT
    trade from put_pixel whose indices come from arithmetic that can genuinely go
    out of range. The rule is not "always check" — it is "check where the input can
    actually be wrong".
  - the demo's y-flip lives in ONE function (to_screen). mat2 is +y up (maths
    convention, rotation is CCW); the framebuffer is +y down. One minus sign at the
    boundary — NOT negations sprinkled through drawing code, and NOT baked into the
    matrices, which would leave "which way does rotation() turn?" permanently
    ambiguous. 2.11 names that boundary.
  - the demo glyph is an F, not a blob. A symmetric shape cannot show a reflection,
    and the reflection case is the entire point of the determinant's sign.
  - DEPTH BUFFER IS ITS OWN TYPE, not a field of framebuffer. Three reasons, and the
    third settles it: (a) 2-D demos, Pong, the HUD and Module 6's post-process
    intermediates are colour-only and would pay 8 MB each at 1080p for nothing;
    (b) different formats and unrelated clear values; (c) SDL_BeginGPURenderPass
    takes colour targets as an ARRAY and the depth-stencil target as a SEPARATE,
    NULLABLE parameter. Modelling the hardware's split now makes Module 4 a rename.
  - NO depth_buffer::test_and_set(). Tempting — it would name the algorithm — and
    wrong: the comparison is PIPELINE state, not storage. SDL_GPUDepthStencilState
    has compare_op + enable_depth_test + enable_depth_write as three separate knobs
    because different passes set them differently (shadow pass writes depth and no
    colour; transparent pass tests depth and does not write it). Baking
    "less-than, always write" into the buffer would make those unexpressible.
  - fill_triangle takes depth_buffer* rather than gaining a second overload. The
    fill's set-up is subtle (bias, six steps, three starting values) and raster.cpp
    already argues that duplicating something subtle is how one bias ends up wrong
    in one of three places. A nullable NON-OWNING pointer is not a violation of the
    no-raw-owning-pointers rule: that rule is about ownership, and what a reference
    cannot express here is OPTIONALITY.
  - z inserted BEFORE colour in `vertex`, which breaks every brace-init call site —
    deliberately. Uint32 -> float is narrowing in list-initialisation, so the old
    3-argument form is a COMPILE ERROR rather than a colour landing silently in the
    depth field. Aggregate initialisation earning its keep; a constructor taking
    (int,int,Uint32) would have compiled and produced garbage.
  - quantisation is a property of the BUFFER (its format), not of the rasterizer or
    the demo. Storage stays float and we round to the format's grid on write, so the
    BEHAVIOUR (z-fighting) is exact while the memory saving is the part not modelled.
    Said so in the lesson. 2^24-1 and 2^16-1 are exactly representable in float, so
    round(z*codes)/codes lands on a real grid point.
  - the ground grid is drawn FIRST and never depth-tested. Not laziness — it is the
    painter's algorithm surviving as a legitimate special case for a background,
    which is exactly what a skybox is (Module 6). Depth-tested lines are Ex 3.1.2.
  - the demo renders BOTH algorithms every frame into a scratch framebuffer and
    counts differing pixels. A second full pass at 320x180 costs microseconds and
    turns "the painter's algorithm is wrong" from a claim into a live number. Fifth
    time this bargain has been made (draw_line_naive 2.1, pong swept_collision 1.8,
    blend_space::encoded 2.4, the w toggles 2.7, trs_order 2.8).
  - the CYCLE scene's planks are built from make_plank(a, b, width, tilt, OVERHANG).
    The overhang lengthens each quad WITHOUT moving its plane, so the depths at the
    two named corners are still exactly +-tilt. It exists because end-to-end planks
    overlap in a sliver, and a sliver is not a demonstration: swept (R, width),
    the disagreement went 7 px -> 144 px. Measured, not eyeballed.
  - inv_w STORED PRE-DIVIDED rather than w. The inner loop interpolates 1/w, so
    storing w would mean a divide per vertex AND the same divide per pixel; and the
    divide-back becomes a multiply by a reciprocal already computed.
  - AFFINE MODE IS NOT A SECOND CODE PATH. It is the same arithmetic with every 1/w
    forced to 1 — which is what affine interpolation IS: a perspective renderer doing
    orthographic interpolation. One loop, and it says the thing. Costs a redundant
    divide in a mode that exists only to be shown failing; worth it.
  - fill_style replaces the growing tail of trailing enums. Not only tidiness: it is
    the pipeline-object shape (SDL_GPUGraphicsPipelineCreateInfo), and it previews
    Module 4 §2's "why state lives in pipeline objects".
  - shading enum lives in the RASTERIZER and is admitted to be a placeholder for a
    fragment shader. Introduce the crude thing, let it strain (3.6), let the strain
    motivate the right thing — the same arc as main.cpp -> Module 5's demos/ split.
  - mesh uvs are a PARALLEL ARRAY, not interleaved. Parallel lets the cube and
    icosahedron carry none and store none; interleaving is what the GPU wants and what
    Module 4 revisits with the memory-layout diagram the decision deserves.
  - the FLOOR gets the whole framebuffer as its viewport, and that is measured rather
    than chosen: scratch/fit_floor.cpp sweeps every extent worth having and the near
    edge ALWAYS projects outside the inset rect. Not a tuning failure — a surface you
    stand on fills the bottom of your view, which is what makes it a good subject.
  - the uvs on the floor come from the WORLD POSITION, not the grid index, so the
    checker is bit-identical at every tessellation and [T] changes only the
    interpolation error. Without that you are comparing two different pictures.
  - std::floor, not a cast to int, in checker_at. A cast truncates toward zero, so
    -0.5 and +0.5 share cell 0 and the pattern grows a doubled cell at the origin —
    which the floor's uvs (-3..+3) would have shown.
  - the naive collision test is KEPT in the shipped code behind state::swept_collision
    rather than deleted, so the failure can be reproduced on demand (pedagogy §5:
    show the artifact). It is dead weight only if you think a bug you can summon is
    worth less than one you can only describe.
  - CLIPPING GETS ITS OWN VERTEX TYPE. engine::vertex is a SCREEN-space type —
    integer pixels, device depth, a pre-divided inv_w — and every one of those
    fields assumes the divide has happened, which is exactly what makes it the
    wrong type to clip with. One flexible struct with a "have I been divided yet"
    flag would COMPILE and silently produce nonsense. Two types make that call fail
    to compile. Same argument as point()/direction() in 2.7 and drop-vs-divide in
    2.10: when two states must not be confused, make them two things.
  - clip_polygon_near TAKES AND RETURNS SPANS, and the output bound is a PROOF
    written in the header, not a margin: emissions = (vertices inside) + (crossings),
    one plane gives at most one crossing each way around a convex polygon, so 2+2=4.
    No capacity check in the loop. This is 3.2's lesson applied — a limit that
    silently truncates turns a capacity bug into a rendering bug, so the right move
    is to make the bound provable rather than to clamp.
  - SUTHERLAND-HODGMAN WRITTEN AS TWO QUESTIONS, NOT FOUR CASES. "Did the edge
    cross?" (emit the crossing) and "is `cur` inside?" (emit cur). The four-row
    table is real but it is a table, and four branches that are really two is an
    invitation to typo one of them.
  - THE SORT KEY IS COMPUTED BEFORE CLIPPING and shared by every piece. The pieces
    are the same surface; a sort must not be able to tell them apart, or one half of
    a wall sorts in front of the other.
  - THE DIVIDE MOVED INSIDE THE TRIANGLE LOOP, so a shared vertex is divided once
    per triangle using it (60 instead of 12 on the icosahedron). Paid knowingly: the
    two-pass alternative (divide the unclipped, re-divide the clipped) is more code,
    more state, and wrong in ways that are easy to miss. Real hardware pays it too —
    vertex shader, clip, THEN divide, in that order.
  - `projector` GATHERS proj + viewport + near policy. Same argument fill_style made
    in 3.2, one level up. Note the direction of the win: adding this lesson's knob
    made every call site SHORTER, because it replaced two loose parameters with one.
  - THE ARROWHEAD IN draw_axes3 STILL DROPS RATHER THAN CLIPS, and says so. It is
    screen-space DECORATION built by rotating the projected shaft; if either end is
    behind the eye the rotation has nothing meaningful to act on. A decoration with
    no defined position is dropped; GEOMETRY is clipped. Worth stating explicitly so
    it does not read as an oversight.
  - THREE near modes, not a bool. There are two DIFFERENT wrong answers here (drop
    the triangle; divide anyway) and they fail in visibly different ways. A bool
    would only let one of them be seen. Sixth time this bargain has been made
    (draw_line_naive 2.1, pong swept_collision 1.8, blend_space::encoded 2.4, the w
    toggles 2.7, trs_order 2.8, interpolation::affine 3.2).
  - THE NaN GUARDS ARE ENGINE HARDENING, NOT DEMO SCAFFOLDING. float->int is
    UNDEFINED out of range, and `none` mode reaches it. But the NaN is not really
    the demo's: Module 6 pushes an HDR pipeline through linear_to_srgb_u8 and
    Module 8's physics will produce the occasional NaN the way all physics does. A
    cast that is undefined for a value the program can reach is a latent bug
    whichever lesson first reaches it. Note the SHAPE of the test — `!(x < limit)`
    and `!(linear > 0)`, because every comparison with a NaN is false and
    std::clamp therefore hands one straight back.
  - to_pixel CLAMPS TO +-8000 and the constant does two jobs: it keeps the float
    inside int's range, AND it keeps edge_function's products (which multiply
    coordinate DIFFERENCES) inside int32, where signed overflow is also undefined.
  - CULLING READS A SIGN THE RASTERIZER WAS ALREADY COMPUTING, and then throwing
    away. fill_triangle has measured edge_function since 2.2 and immediately
    reoriented to positive area so the top-left rule has meaning — which destroys
    the facing. One window, one branch. No normal, no dot product, no camera: the
    projection already folded the camera into the sign (see the determinant identity
    in the conventions block).
  - is_front_facing IS A NAMED FUNCTION taking SCREEN-SPACE vertices, for two
    reasons. The rule has two readers (rasterizer + HUD counter) and duplicating it
    is how they drift apart — 2.4's is_top_left argument exactly. And the signature
    makes the view-space bug UNWRITABLE: there is no overload that accepts a
    view-space position, so you cannot ask the question in the wrong space by
    accident.
  - THE DEMO COUNTS IN THE CALLER, NOT VIA A RETURN VALUE. fill_triangle could
    report whether it culled, but making every fill return a bool puts a value at
    100% of call sites that 99% ignore. Counting in draw_triangles costs one extra
    edge_function per triangle and keeps the RULE in one place. Instrumentation may
    duplicate the question; it must never duplicate the answer.
  - cull_mode DEFAULTS TO none, breaking 3.2's rule that every fill_style field
    defaults to the correct value. Deliberate: there IS no universally correct cull
    mode, so the default is the SAFE one. SDL_GPU makes the same call (a
    zero-initialised SDL_GPURasterizerState is CULLMODE_NONE).
  - `closed` IS A BOOL ON scene_object AND THE DEMO ONLY WARNS. Restraint on
    purpose: the whole scene is one batch with one fill_style, and per-object cull
    modes would mean splitting the draw. That friction is the lesson — it is the
    first time this engine has wanted two pipeline states in one frame, and the
    answer is a material system (Module 6), not another parameter.
  - THE WRONG TEST IS CULLED IN collect_triangles, IN VIEW SPACE — not because that
    was convenient, but because that is exactly where the bug lives in codebases that
    have it. It looks like a sensible early-out. Seventh keep-the-wrong-thing bargain
    (draw_line_naive 2.1, pong swept_collision 1.8, blend_space::encoded 2.4, the w
    toggles 2.7, trs_order 2.8, interpolation::affine 3.2, near_mode 3.3).

  - to_eye HAS NO DEFAULT on shade(), so every call site written before 3.7 is a
    COMPILE ERROR rather than a silent highlight computed against a viewer who is
    not there. Same bargain 3.1 made inserting z ahead of colour in `vertex`: when
    a change must be noticed, make the compiler notice it. The SURFACE parameters
    do default, because there the safe answer and the correct answer coincide —
    a black highlight reproduces 3.6 BIT FOR BIT (0 of 100000 random configs differ).
  - `specular` IS A MATERIAL AND IS DELIBERATELY NOT CALLED ONE. A material is also
    the albedo, the cull mode, the blend mode, the textures and eventually the
    shader; inventing four fifths of that here would be guessing. This is the
    FOURTH pull in the same direction (3.2's shading enum, 3.4's `closed`, 3.6's
    "fill_style is the wrong home", now this) and Module 6 answers it with
    arguments rather than by accretion.
  - specular_model IS A PARAMETER, NOT A FIELD OF `specular`. Which approximation
    you evaluate is PIPELINE state — a real engine bakes it into a shader at
    compile time and a scene does not mix the two. It is a runtime knob here for
    exactly one reason: so both can be rendered and the difference counted.
  - THE COMPARISON CONVERTS THE EXPONENT (matched_shininess, 4x). Comparing the two
    models at one exponent mostly measures that one lobe is wider, which is true,
    is 3.4's own point, and swamps the effect under test. A comparison is only
    worth running once you have controlled for what you already know differs.
  - THE COMPOSED view_from_model IS GONE, and that is worth naming rather than
    absorbing. It existed BECAUSE the shading was view-independent; a highlight
    needs the world POSITION, so the two hops come apart and every vertex pays a
    second matrix multiply. A fast path was not lost to carelessness, it was BOUGHT
    OUT by a feature. (The model-space dodge — move the eye into model space once
    per object — is Exercise 3.7.4, and it is a win for one light and a loss for
    many, which is why it fell out of fashion.)
  - FLAT SHADING SAMPLES THE CENTROID. Flat used to need no position at all; with a
    view-dependent term it does, because the specular varies across a face even
    when the normal does not. The centre is the only point that privileges no corner.
  - PHONG IS KEPT BEHIND [H] — the EIGHTH keep-the-wrong-thing bargain
    (draw_line_naive 2.1, pong swept_collision 1.8, blend_space::encoded 2.4, the w
    toggles 2.7, trs_order 2.8, interpolation::affine 3.2, near_mode 3.3,
    cull_choice::back_by_forward 3.4). Note this one is not simply "wrong": it is a
    real historical model with a real, provable failure, which is a better example.
  - THE HUD READS THE BRIGHTEST CHANNEL IN THE VIEWPORT. One integer, and it tracks
    the highlight without needing to know WHERE the highlight went — which is the
    thing under investigation. The cheapest honest instrument in the demo.
  - THE MISSING n.l IS AN EXERCISE, NOT A KEY. Three lighting toggles ([G], [J],
    [H]) plus an exponent ([E]) is already at the limit of what one HUD line can
    say, and the artifact is fully characterised numerically in verify_37 §E.
    Restraint, on the same grounds 3.4 used for per-object cull modes.

  - THE `shading` ENUM GREW A `lit` VALUE AND raster.hpp NOW INCLUDES light.hpp.
    A real layering violation, shipped deliberately. The rasterizer's job is
    coverage and interpolation; what a covered pixel LOOKS like is somebody else's.
    The clean fix is to make the fragment calculation a PARAMETER — which is what a
    fragment shader IS, and building it here means inventing Module 4's answer three
    lessons early. Fixed-function hardware made the same trade and lost the same
    way; the whole programmable-shader era is this debt being paid. 2.4's own header
    predicted it ("a placeholder for a fragment shader").
  - vertex GAINED TWO VARYINGS AND STOPPED BEING JUST A POSITION. `normal` and
    `world` are INPUTS to a calculation that has not happened yet, as against every
    earlier field which was geometry or an answer. Cost: 28 -> 52 bytes and six more
    interpolated floats per pixel, paid in bandwidth to be saved in accuracy — the
    same trade a GPU makes, which is why minimising varyings is a real optimisation
    in Module 4.
  - vertex::colour NOW MEANS DIFFERENT THINGS IN DIFFERENT PIPELINES: a lit result
    under vertex_colour, the ALBEDO under lit. Not a wart — deciding what a varying
    means is the vertex stage's job, and this is the first varying in this engine
    whose meaning depends on what it is bound to.
  - THE CLIPPER CARRIES THEM TOO, and this is the one that gets forgotten because
    the clipper usually does nothing. Miss it and shading breaks ONLY on triangles
    crossing the near plane — i.e. only when the player walks into something, which
    is the same detection profile as 2.7's w bug. Exercise 3.8.2 makes it visible.
  - THE NORMAL IS NOT RENORMALISED IN THE CLIPPER. The fill interpolates it again
    on the way to the pixel and shortens it again, so normalising there fixes
    nothing the fragment's own normalise would not. Two sqrt for one result.
  - `lit` FORCES blend_space::linear AND IGNORES THE FIELD. There is no coherent
    reading of "blend in encoded space" for a value about to be multiplied by a
    quantity of light; honouring it would produce nonsense rather than a different
    picture. Ignoring a knob is better than obeying it into meaninglessness — and
    it is said in the code rather than left to be discovered.
  - A NULL `lights` FALLS BACK TO THE UNLIT PATH rather than being a precondition.
    A pipeline set to `lit` with no light is a configuration mistake; drawing an
    unlit surface is diagnosable, and dereferencing null per fragment is not.
  - THE MATERIAL RIDES ON raster_triangle AND IS REBOUND PER TRIANGLE. A cheat a
    GPU cannot make: material parameters are pipeline state, so a real renderer
    BATCHES BY MATERIAL and a scene with three materials is three draws. Taken
    because splitting the single pass into per-object draws would break the
    painter's-algorithm comparison running since 3.1 (that one must sort ACROSS
    objects). Third time this pressure has appeared from a new direction — 3.4's
    cull modes, 3.7's specular, now this.
  - TWO KEYS FOR TWO AXES, [G] and [Q], and the HUD prints them on TWO LINES. A
    single line reading "[G] smooth" was precisely the conflation the lesson undoes;
    the layout is part of the argument.
  - is_degenerate() WAS WRONG THE FIRST TIME and the corrected version is SHORTER.
    It claimed face x gouraud was degenerate unconditionally. verify_38 §A measured
    761 px with the highlight on. A rule with an exception carved into it is often a
    rule stated at the wrong level.
  - THE FILL IS TIMED, and only the fill — not the projection, the clip or the HUD,
    because those do not change with the evaluation point. Smoothed like clock::fps()
    for the same reason: one frame on a desktop OS is mostly noise.
```
