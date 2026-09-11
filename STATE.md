# STATE — resume key

This file mirrors the `STATE` block from the master prompt (CLAUDE.md §9). It is the
single source of truth for "where the course is". Update it at the end of every lesson.
To resume: read CLAUDE.md (the binding spec), then this file, then continue from `next`.

```STATE
course: Build a Professional 3D Game Engine (SDL3 + C++20)
version: 1.0
updated: 2026-09-11 (after Lesson 6.12 — 68 of 107 lessons)

conventions:
  ecs-storage: THE ECS IS A SPARSE SET, DECIDED IN 5.7 BY MEASUREMENT, NOT TASTE.
        One dense array per component TYPE plus a sparse map entity -> dense index.
        NOT an archetype. The argument is NOT "sparse sets are faster" — on the
        query the archetype wins — it is that THE MIGRATION ONLY RUNS ONE WAY:
        a sparse set can be given an archetype's query later, one GROUP at a time
        (measured 0.99x of a real archetype on the archetype's own best case),
        and an archetype can never be given O(1) structural change, because
        moving between archetypes IS what an archetype is.
        THE FOUR RULES THAT FOLLOW, and 5.8 must honour all four:
          1 COMPONENTS STAY SMALL AND SINGLE-PURPOSE. The whole model is that a
            system pays only for the pools it names; a fat component drags unread
            bytes exactly as 5.6's 96-byte scene_object did.
          2 ONE SHARED ID SPACE. pool<T>'s three arrays are already a sparse set
            (slots_=sparse, items_=data, owners_=dense) but each pool MINTS ITS
            OWN keys, so a mesh handle means nothing to a texture pool. The
            entity id must be minted once and handed to every pool.
          3 A VIEW LEADS WITH THE SMALLEST POOL. One comparison in the view's
            constructor, worth 1.73x -> 1.46x at one-in-four selectivity, and
            worth more the rarer the component. Never walk the big pool and skip.
          4 NO PAGED SPARSE ARRAYS YET, and the reason is 5.4: the free list
            reuses the most recently freed slot, so the id space's high-water
            mark is peak LIVE entities, not entities ever created. 10 types x
            10k peak = 400 KB. Revisit at 32 types x 1e5 (12.8 MB, 3x L2).
  ecs-runtime: BUILT IN 5.8, IN engine/include/engine/ecs/, HEADER-ONLY, 4 FILES.
        entity.hpp   the id + entity_allocator. pool.hpp    pool_base + pool<T>.
        registry.hpp the world + component_id_of<T>.  view.hpp    the query.
        No CMake change: header-only, so engine/CMakeLists.txt is untouched and the
        public header count goes 47 -> 51.
        THE ID IS ITS OWN TYPE, NOT handle<entity_tag>, and the reason is meaning
        rather than bits: it REUSES core/handle.hpp's constants (20/12, generation
        0 reserved, bump on removal) so the budget has one home, but
        handle<mesh_data> names an item IN one container and an entity names a row
        ACROSS every pool there is. Aliasing them would make
        pool<T>::get(handle<T>) and a component lookup the same spelling for
        opposite things — an odd way to spend the phantom parameter that exists to
        keep ids from mixing.
        THE ALLOCATOR SLOT IS 8 BYTES FOR 13 BITS and that is deliberate:
        {uint32 generation; bool live;} pads to 8, packing would buy 0.4 KB per
        thousand entities and cost every reader a shift and a mask, and at 10k peak
        entities the 80 KB is nothing beside the 400 KB of COMPONENT sparse arrays
        the same 10k imply across ten types. Fields are private; the change is local.
        dense_ STORES THE FULL ENTITY WORD, NOT A BARE INDEX. contains() is three
        tests and the third — dense_[at] == e — is what rejects a stale or recycled
        id. Store an index there and that test CANNOT BE WRITTEN, and 5.4's aliasing
        failure walks back in.
        THE SPARSE PATCH IS ONE LINE AND ITS ABSENCE IS SILENT:
        sparse_[dense_[at].index()] = at; after the swap. The entity swapped into
        the hole did not ask to move. Without it the symptom is one entity reading
        another's data, an arbitrary number of frames later, in a different system.
        The last-element case needs no special handling BECAUSE OF THE ORDER: when
        at == last the "moved" entity is the erased one, and the final
        sparse_[e] = k_none overwrites the redundant patch either way.
        TYPE ERASURE WITHOUT RTTI: component_id_of<T>() is a monotonic counter
        behind a function-local static (one object per instantiation across every
        TU, thread-safe to initialise, NOT thread-safe to increment — touch each
        type once before the job system starts). The id indexes
        vector<unique_ptr<pool_base>> and static_cast recovers the type, SAFE
        BECAUSE THE ID IS WHAT CREATED THE POOL — justified by construction, not
        checked at use. Ids are GLOBAL to the program, not per registry: a registry
        using only the tenth type ever registered allocates 11 slots, 10 of them
        null. 80 bytes once, in exchange for an array index instead of a hash.
        EVERY pool_base VIRTUAL IS COLD, and that is a constraint from 5.6's
        1.5-1.7x measurement rather than an accident: erase (once per pool per
        entity destruction), clear (teardown), size, entities (once per VIEW, never
        per entity). A view holds concrete pool<T>* and calls nothing through the
        base; pool<T> is `final` so even the cold calls devirtualise.
        entities() RETURNING span<const entity> FOR EVERY T IS WHAT MAKES RULE 3
        FIVE LINES. A pack of pool<Ts>* becomes an ordinary array of spans and the
        smallest is chosen by a `for` loop — no dispatch, no metaprogramming.
        THE LEAD POOL EARNS ITS KEEP TWICE: fewer candidates (rule 3), and its own
        component needs NO sparse read because the dense position IS the loop
        counter. `I == lead_` compares a compile-time constant against a value
        fixed for the whole loop.
        THE ONE ITERATION RULE: DO NOT ADD OR ERASE A COMPONENT THE VIEW NAMES
        WHILE WALKING IT. The walk is over the lead pool's dense array BY POSITION
        and both insert and erase move it — same hazard as mutating a vector inside
        a range-for, and no more forgivable. Debug catches it via the assertion in
        view::fetch (dense position no longer matches the entity). Safe patterns:
        collect-then-act (lifetime_system), or a deferred command list (Module 9).
        registry::add TO A DEAD ENTITY IS REFUSED, not filed — a row under a dead id
        is invisible to every query and never erased, i.e. a leak with no symptom.
        registry::has DELIBERATELY DOES NOT CHECK LIVENESS: a stale id fails
        pool::contains anyway, so alive() would be a second slower route on the
        hottest path.
        EVERY READ PATH USES storage_if<T>(), NOT storage<T>(). Asking whether an
        entity has a component must not allocate a pool as a side effect — that
        makes a query mutating and a const registry impossible. A view naming an
        unused type is EMPTY, which is the right answer without a special case.
        NO GROUPS YET, and the hole is named rather than hidden: 5.7 measured a
        group at 0.99x and THAT is why the sparse set was chosen (the migration
        only runs one way), but building one now is optimising before there is a
        profile. pool::components() documents that its dense order is nobody's
        business PRECISELY so a future group may sort it. Also absent, each with a
        reason: exclusion queries (Ex 10.2, ~15 lines), const views, signals,
        thread safety (Module 9, all containers at once).
        TWO CONTAINERS CALLED pool, ON PURPOSE. engine::pool<T> mints its own keys;
        engine::ecs::pool<T> is keyed by an id it did not mint. Same three arrays,
        opposite jobs. The namespace keeps them apart; `using namespace engine;`
        plus `using namespace engine::ecs;` makes bare `pool` ambiguous, which is
        the good kind of breakage. The engine never writes `using namespace`.
  hdr: LESSON 6.12. THE ENGINE HAD NO HDR BUG — IT WAS AVOIDING THE QUESTION BY
        CONSTRUCTION, and the two choices that held the lid on are both on record:
        `k_reference_irradiance` = pi (6.2, so a white surface renders at exactly
        1.0) and the demo's roughness of 0.49 (peak 0.8676, just under). Measured
        at the light's MIRROR direction — not a sweep, see below — the same
        equation gives 1.74 / 11.59 / 177.03 / 2824 at roughness .35/.20/.10/.05
        and 55,917 for a polished metal. ABOVE 1.0 THE CLAMP IS TOTAL: 1.0 and
        55,917 are the same code.
        EXPOSURE = 1/(1.2 * 2^EV100). The 2^EV is the DEFINITION of a stop; the
        1.2 is 78/(q*100) with q=0.65 from ISO 12232, a CAMERA CALIBRATION quoted
        rather than derived (CLAUDE.md §3.2's rule, applied). The engine's old
        behaviour is EV -0.263, which makes it A POINT ON THE NEW SCALE rather
        than a special case beside it — that reframing is what turns a
        replacement into a generalisation, and it is worth reaching for whenever
        a lesson "replaces" something.
        AN OPERATOR IS LEGAL IF: monotonic (or a highlight comes out darker than
        its own edge), near the identity at 0 (or the dark end — where the encode
        spends its codes — is wrong), bounded by 1 (or something downstream
        clamps and you have a curve AND a clamp). Everything else is taste.
        REINHARD DERIVED IN ONE LINE: divide by something ~1 for small x and ~x
        for large x; the simplest is 1+x. WHITE POINT DERIVED from f(W)=1:
        x(1+x/W^2)/(1+x), and W->inf recovers the plain operator. ACES IS A FIT,
        five constants, no derivation — and its toe costs 13 CODES at x=0.02,
        which is a real artistic choice made on your behalf.
        THE 224. Plain Reinhard never reaches 1, and code 255 needs an input of
        224 — NOT 768, which is what you get by inverting 254.5/255 in the wrong
        space. THIS LESSON'S OWN AUTHOR MADE THAT MISTAKE IN A DOC COMMENT and
        the harness caught it. Keeping codes and light apart is Module 6's
        recurring subject and it does not stop being hard because you are the one
        writing about it.
        PER-CHANNEL vs LUMINANCE-ONLY IS A REAL CHOICE, like blend_space in 2.4.
        Per-channel desaturates toward white (red:blue 20 -> 3.11), which is what
        film does and is the default. Luminance-only preserves hue EXACTLY (20.00)
        and then leaves a channel above 1 for the encode to clip — a pure blue of
        (0,0,8) has luminance 0.5776, sails through, and arrives at 5.07, because
        blue's luminance weight is 0.0722. It preserves the hue right up to the
        point where it does not.
        THE ORDER IS EXPOSURE -> CURVE -> ENCODE and each wrong order has its own
        signature, both measured: exposure last turns code 203 into 128 AND
        destroys the bright end's variation (everything above ~4 was already in
        one place); curve after the encode maps mid grey to 0.4244 instead of
        0.3333 and crushes the shadows first.
        THE RESOLVE IS A SECOND PASS, NOT MORE FRAGMENT SHADER, and the three
        reasons each become a real limitation within two lessons: an exposure
        derived from the frame cannot be known while the frame is being drawn;
        bloom (6.13) operates on the PRE-curve image; and blending would
        composite tonemapped values, where f(a) over f(b) != f(a over b).
        ONE TRIANGLE, NOT TWO, and 4.1 is why: fragments shade in aligned 2x2
        quads, so along a shared diagonal every quad straddles both triangles and
        is issued twice. NO VERTEX BUFFER AT ALL — the corners come from
        SV_VertexID; `SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0)` is the whole draw.
        COLOR_TARGET *AND* SAMPLER on the HDR texture. Omitting the second gives a
        texture that renders perfectly and reads as undefined, with no error on a
        release device (6.10: SDL's GPU validation lives inside
        `if (device->debug_mode)`).
        METERING IS LOG-SPACE. One pixel in 10,000 at 5000 moves the ARITHMETIC
        mean to 0.55 (of which 0.50 is that one pixel); the log-average is 0.05016.
        11x apart on the same image, which is the difference between an exposure
        that tracks the scene and one that flickers.
        AND 6.11's _SRGB-vs-UNORM RULE DOES NOT TRANSFER TO A FLOAT TARGET —
        there is no transfer function in the ROP to be right or wrong about,
        because nothing in a float pipeline stores codes. `encode_output` has
        always meant "must THIS shader apply the transfer function?", so on a
        float target the answer is permanently 0: same number, THIRD reason.
        Compositing there is 8.1x cheaper (9.25 ns vs 75.12) because 6.11's round
        trip was never a cost of blending — it was a cost of storing something
        other than light.
        A SHARP LOBE CAN MISS THE SCREEN ENTIRELY. At roughness 0.15 the demo's
        flat-faced scene peaked at 0.22 and reported ZERO pixels over the lid,
        which is how a renderer with a real clipping problem looks fine. The demo
        adds a TORUS — curved in both directions, so some point satisfies the
        mirror condition from every viewpoint. Related diagnostic: if SHARPER
        surfaces report LOWER peaks, the measurement is missing the lobe, not the
        physics being surprising.
  transparency: LESSON 6.11. ALPHA IS COVERAGE — THE FRACTION OF THE PIXEL'S
        AREA A SURFACE OCCUPIES — NOT OPACITY, AND NEVER A QUANTITY OF LIGHT.
        `colour.hpp` has said so since 1.6 ("carry it separately if you need it");
        6.11 is what carrying it separately turned out to mean. Every result in
        the lesson falls out of that one reframing.
        THE THREE MODES LAND ON OPPOSITE SIDES OF 6.5'S LINE, and the pipeline
        count is the proof rather than the illustration:
          MASK  is A NUMBER IN A BUFFER — `material_uniforms::alpha_cutoff` and a
                `clip` in the shader. ZERO new pipelines. Its only price is that
                THE DEPTH WRITE MOVES AFTER THE FRAGMENT, because the fragment is
                what decides whether there IS a fragment — the same dependency
                that costs a GPU its early-Z for any shader containing `discard`.
          BLEND is PIPELINE STATE — factors, op, and DEPTH WRITES OFF. SIX new
                pipelines, because it is a SECOND AXIS and 3 x 3 = 9. A second
                axis MULTIPLIES the count; that is why real engines hash state
                into a pipeline cache instead of enumerating. Ours enumerates
                because nine is countable (0.05 ms warm).
        THE TWO INTEGERS, A THIRD TIME. Half-coverage white over black is half
        the LIGHT = code 188; lerping the STORED BYTES gives code 128 and emits
        0.2159 — 42.9% of the answer. EXACT AT a=0 AND a=1 and worst in the
        middle (0.2898 of full white at a=0.55), which is precisely why the bug
        survives review: every fade starts and finishes correctly.
        AND THE HARDWARE MAKES THE SAME MISTAKE, MEASURED. The same draw into an
        _SRGB target and a UNORM one with the shader encoding: 188 against 127,
        42.2% — the SAME shortfall 6.10 found in a naive mip chain, because it is
        the same arithmetic. Blending happens in the ROP, AFTER the shader
        returns, so THE SHADER CANNOT FIX IT; only the target's format can.
        `scene.frag.hlsl` predicted this in a 6.1 comment ("correct for opaque
        geometry and wrong the moment anything blends") and 6.11 discharged it.
        PREMULTIPLIED ALPHA IS FOR FILTERING, NOT FOR COMPOSITING. The saved
        multiply is incidental; the property is that `over` composes
        ASSOCIATIVELY in that form, so an AVERAGE of premultiplied fragments is a
        valid composite. Straight-alpha filtering across a cutout edge halves the
        leaf's green (0.2636 against 0.5271) because the transparent texels are
        BLACK — what an image editor leaves in a cleared region — and that deficit
        IS the dark halo. `alpha_storage` therefore lives on `texture` beside
        `texel_space`: a property of the DATA, read by `fetch` and `build_mips`,
        not remembered by call sites (6.7's rule, second application).
        AVERAGING AND THRESHOLDING DO NOT COMMUTE, which is why alpha-tested
        foliage dissolves with distance: an ordinary chain passes 0.2500 at level
        6 where the source passed 0.3635, 31.2% of the leaves gone, with NOTHING
        in the chain wrong. Castano's rescale (bisection on one scale factor per
        level — coverage is a STEP function of the scale, so no derivative, so
        not Newton) holds it to 0.0115 down to 8x8 and then STOPS: a 4x4 level has
        sixteen texels, so its coverage can only be k/16. Measure where a
        technique stops rather than widening a tolerance until the test passes.
        DRAW ORDER: BACK TO FRONT, AXIAL, STABLE. Axial (`dot(p-eye, forward)`) is
        6.9's cascade distinction arriving a second time and worth 16.6% at a
        frame corner. Stable because two objects at equal depth have NO correct
        order, and an unstable sort turns a tie into a flicker. MASKED GEOMETRY IS
        NOT SORTED — `stable_partition` on `mode != blend`, NOT `mode == opaque`,
        which is the line most likely to be written backwards; reading
        `alpha_mode` as a scale from solid to see-through puts every leaf in the
        scene into a per-frame sort in exchange for nothing.
        THE CEILING, NAMED RATHER THAN GLOSSED: two INTERSECTING blended quads
        need both orders at once. Not a better sort — OIT, which is a different
        data structure and is not built.
  colour-pipeline: TWO CONVERSIONS, AT THE EDGES — NOT ONE PER OPERATION, AND NOT
        "CAREFULLY". 6.1 turned 1.6's rule into a pipeline and found the missing
        edge. THIS SUPERSEDES NOTHING IN conventions:colour, which stands entire;
        it pays the first two clauses of that block's "NOT YET LINEAR" note.
        THE TWO INTEGERS: code 128 emits 0.2159 of white (not half); half the
        light is code 188. Sixty codes apart, and every mistake in this subject is
        a variation on that gap.
        THE ENCODING IS A BUDGET, NOT A CRT ARTEFACT — and the CRT story, while
        true, is useless because CRTs are gone and the encoding is not. The eye
        judges RATIOS, so equal code steps should be equal ratios. MEASURED on 256
        codes: evenly spaced in LIGHT puts 26 in the darkest tenth and 128 in the
        brightest half where nobody can tell two apart; sRGB puts 90 in the darks
        and 68 in the bright half. 8 bits of linear light needs ~12 to look as
        smooth. Frame it as perceptual compression and it stops being history.
        THE TOE IS DERIVED, NOT DECREED. d/dx of x^(1/2.4) is (1/2.4)x^-0.583,
        which goes to INFINITY at zero — unbounded gain at black, so sensor noise
        becomes banding, the inverse is unstable, and an 8-bit boundary is
        arbitrarily sensitive. Hence 12.92*x below 0.0031308. MEASURED back out of
        the shipped function as 12.9200. The four constants are not independent:
        0.04045 = 12.92 * 0.0031308, and 1.055/0.055 make the pieces MEET (step at
        the join measured at 8.02e-05, a fiftieth of a code).
        pow(x, 1/2.2) IS A DIFFERENT CURVE: worst disagreement 8 CODES near linear
        0.0010, down in the toe where the eye has the most codes to notice with.
        Fine as a stylistic brightness knob; NEVER as the output transfer
        function, because that one must agree to the code with every hardware
        sampler in the pipeline.
        THE AUDIT IS THE METHOD, and it is the part to copy: grep every conversion
        (15 sites outside colour.{hpp,cpp}) and give each a JOB — input edge,
        output edge, or per-operation. One row was left over.
        THE HOLE: THE GPU PATH HAD AN INPUT EDGE AND NO OUTPUT EDGE. SDL claims a
        window with SWAPCHAINCOMPOSITION_SDR, whose header says "pixel values are
        in sRGB ENCODING", and scene.frag.hlsl has returned linear LIGHT since 4.8.
        MEASURED, on real downloaded pixels: linear 0.5 -> code 128, correct 188.
        Ramp (linear / raw / correct): 0.02/5/39, 0.05/13/63, 0.10/26/89,
        0.20/51/124, 0.35/89/160, 0.50/128/188, 0.65/166/211, 0.80/204/231,
        0.95/242/249.
        THE ERROR IS A RATIO, NOT AN OFFSET: 13x at linear 0.02, 1.1x at 0.95.
        THAT SHAPE IS WHY FOUR MODULES DID NOT FIND IT — the bright half is nearly
        right and the dark half reads as a deliberate grade. An error shaped like
        this hides wherever there is least contrast to spare.
        AND IT IS WHY 4.8's COMPARISON MISSED IT: verify_48 rendered into
        R8G8B8A8_UNORM_SRGB, so inside the test the encode happened and the two
        renderers genuinely agreed (87% byte-identical). The measurement was sound
        and measured a configuration THE SHIPPED PROGRAM DOES NOT USE. A TEST THAT
        CONSTRUCTS ITS OWN ENVIRONMENT TESTS THE ENVIRONMENT IT CONSTRUCTED —
        which is why the answer is now a FIELD on gpu_report rather than a literal
        in two places.
        THE FIX, AND THE FOUR HABITS IN IT: ask (WindowSupportsGPUSwapchainComposition)
        then set then READ THE FORMAT BACK; store the answer as a field, because a
        comment would be true on one machine; promote a predicate the moment a
        second caller needs it; and a TWO-PARAMETER SETTER MUST PASS THROUGH THE
        PARAMETER IT IS NOT CHANGING — set_present_mode passed a literal SDR and
        would have silently undone the fix on the first vsync toggle.
        THE SHADER FALLBACK IS THE SECOND-BEST ANSWER, and the reason is the
        POSITION OF THE BLEND STAGE: hardware blending happens AFTER the fragment
        shader, so a shader that encodes hands the blender codes to interpolate —
        correct for opaque geometry, wrong the moment anything is transparent.
        Prefer the swapchain; keep the fallback; read the field to know which.
        THREE IMPLEMENTATIONS OF ONE CURVE AGREE EXACTLY on nine flat samples —
        hardware write, our HLSL, engine::linear_to_srgb_u8, worst disagreement 0
        codes. This does NOT contradict 4.8's one-code floor: these are flat values
        chosen away from code boundaries; 4.8's came off interpolated textured
        geometry. A tighter result on an easier test is not a better result.
        THE SOFTWARE PATH NEEDED NOTHING. Its output edge (to_encoded in
        raster.cpp) has been right since 1.6, which is why the golden survived —
        see the prediction note under decisions.
        STILL OWED, NAMED RATHER THAN DISCOVERED: headroom above 1.0 + a float
        target + tonemapping (the HDR lesson); the software renderer's three
        remaining per-operation conversions (clip.cpp, light.hpp, soft_renderer.cpp
        — all CORRECT, just not a pipeline); and colour SPACES as opposed to
        transfer functions (primaries — this course stays in sRGB primaries).
  debug-draw: SAYING WHAT TO DRAW AND DOING IT ARE TWO FILES, AND THE INCLUDE
        LISTS ARE THE INTERFACE. 5.11 reworked a header that had been wrong since
        Module 3 WITHOUT DELETING ANY OF IT.
        WHAT WAS WRONG WITH line3(fb, a, b, colour, pr): two of six parameters are
        renderer state, and that ONE fact is three problems.
          1 THE CALLER MUST BE THE RENDERER. A collision system, an ECS pass and a
            loader all have something worth drawing and none of them has a
            framebuffer — and handing one down means every caller of every caller
            has one too.
          2 ONE SURFACE ONLY. framebuffer is the software target; surface::gpu has
            none, so every such call is unavailable on Module 4's whole path.
          3 A LINE LIVES ONE FRAME = 16.7 ms at 60 Hz, against the ~250 ms a person
            needs. SO A PER-FRAME DRAWER CAN SHOW STATE AND NEVER EVENTS, and it is
            usually events you are hunting. This is the argument for lifetimes and
            it is the one people dismiss.
        THE FIX REMOVES AN ARGUMENT AND MAKES NOTHING FASTER. debug_lines holds
        WORLD-SPACE segments — world, not view, because the queuer does not know
        where the camera is and often runs before it is resolved; a split-screen
        game flushes the same queue twice.
        TWO FILES BECAUSE THE INCLUDE LISTS MUST DIFFER, and this is the whole of
        the physical design. debug_lines.hpp includes colour, mat4, vec3, span,
        vector — and NOTHING THAT CAN DRAW. debug_draw.hpp includes that plus the
        framebuffer, depth buffer, mesh, projector and viewport. Include
        dependencies are TRANSITIVE, so putting the queue in the drawing header
        would make a physics TU compile the renderer to draw one box.
        HENCE wire_mesh() TAKES TWO SPANS, NOT `mesh` — mesh.hpp drags in
        core/pool.hpp and the handle system. TAKE THE DATA, NOT THE TYPE.
        THE OLD FUNCTIONS BECAME THE BACKEND. draw_debug_lines() is a four-line
        loop over line3_world(), which has clipped correctly since 3.3. Nothing was
        rewritten; a queue was put IN FRONT.
        line3/line3_world NOW RETURN bool = "survived the near plane", NOT
        [[nodiscard]] because six lessons of call sites correctly ignore it. It
        changes no pixel — both new `return false` paths were already `return` and
        already drew nothing — and it makes "queued 152, drawn 151" a real claim
        instead of the queue size printed twice. A NUMBER IN A HUD IS A CHECKABLE
        CLAIM; one the code cannot back is worse than no number.
        EXPIRY TESTS BEFORE IT SUBTRACTS, and the honest statement is not "the
        obvious rule is broken" but "whether it is broken depends on a comparison
        operator and on dt". Measured in float at 60 Hz on a 0.5 s line:
          test-first (shipped)          31 advances; drops a 0 s line at dt == 0
          subtract-then-test, <=        30 advances; drops a 0 s line at dt == 0
          subtract-then-test, <         30 advances; KEEPS IT FOREVER at dt == 0
        dt == 0 is a paused clock, a single-frame --shot, a breakpoint — the
        moments you are looking hardest. Testing first REMOVES the dependency
        rather than getting it right. Cost: a 2 s line lives 2 s + 1 frame, which
        is the right way round to be wrong for a thing whose job is to be seen.
        ORDER: QUEUE -> FLUSH -> ADVANCE, and advance() is the LAST thing in the
        frame and OUTSIDE every branch. Age first and every default-lifetime line
        dies before it is drawn — presenting as a debug system that draws nothing,
        which reads as "my code did not run". Age inside the has-a-camera branch
        and the queue grows without limit on the frames that draw nothing.
        BOUNDED (4096) WITH A DROP COUNTER, because a bound with no counter is a
        bug that presents as a rendering artifact: the missing line looks exactly
        like a thing that does not exist. One private push() checks it, so a
        12-edge box with 2 free slots is 2 kept and 10 dropped, and a primitive
        added next month cannot forget to ask.
        NO DEPTH TEST AND NO BATCHING, both named rather than discovered. A flag no
        backend honours is speculative; and the queue is what makes batching
        POSSIBLE LATER, because you cannot batch calls that already happened.
        THE AXIS COLOURS NOW HAVE ONE HOME (k_axis_{x,y,z}_colour), values
        unchanged from draw_axes3's three literals — a consolidation, not a
        re-colouring, so every earlier picture is provably identical.
  debug-ui: DEAR IMGUI, TAKEN PUBLICLY AND CONTAINED. engine/ui/debug_ui.{hpp,cpp},
        v1.92.9b pinned via FetchContent.
        WHY NOT HAND-ROLL: the test is not "is it hard" (the rasterizer was hard and
        we wrote it) but IS THE HARD PART THE SUBJECT. A debug UI is four subjects —
        text rasterization and shaping, layout, input routing with focus, and widget
        state across frames — and none is graphics or architecture. 41,385 lines of
        core + 1,232 of backends against 182 lines of ours.
        IMMEDIATE MODE IS WHY IT SUITS A DEBUG TOOL SPECIFICALLY: there is no widget
        object, so THERE IS NOWHERE FOR STALENESS TO LIVE. A retained-mode panel must
        be kept in sync with the world and the bug is always a number that is no
        longer true. Checkbox takes the ADDRESS of the program's bool, so the panel
        is a window onto the state rather than a copy of it.
        PUBLIC WHERE stb_image IS PRIVATE, and the reason is not laziness: with stb
        we wrapped a CONCEPT (decode bytes into pixels), one function and one type.
        ImGui's value IS ITS VOCABULARY — four hundred widget calls — and a wrapper
        around that is a re-spelling with no content that must be re-spelt for every
        widget forever. Cost stated out loud: the engine's public API now carries a
        second third-party vocabulary. What makes it acceptable is CONTAINMENT —
        only TOOLING code may speak it, and tooling can be rewritten without the
        game changing by a pixel. debug_ui.hpp itself does NOT include <imgui.h>;
        owning the lifecycle and speaking the widget language are different jobs.
        IMGUI SHIPS SOURCES, NOT A BUILD. No CMakeLists.txt, so
        FetchContent_MakeAvailable only POPULATES and the target is ours to declare —
        which is how you find out exactly which files you compile. imgui_demo.cpp
        (11,299 lines) is included on purpose: ShowDemoWindow() is the fastest widget
        reference there is and its source is the documentation. NOT
        engine_set_warnings(imgui) — third-party code keeps its own flags (5.1).
        BACKEND: SDL_Renderer FIRST. ecs_swarm is on surface::renderer, and there
        on_overlay() already sits after the blit and before the present. On
        surface::gpu the PROGRAM owns device, command buffer and render pass, so the
        UI must be handed all three. imgui_impl_sdlgpu3 needs TWO calls, not one —
        PrepareDrawData before the pass, RenderDrawData inside it — which is 4.2's
        model. Declared out of scope with the real signatures and an exercise.
        NewFrame ORDER IS FIXED: renderer, then platform, then core, because the
        core derives from what the two backends just filled in.
        begin_frame() GOES FIRST IN on_input(), NOT beside the panels where it looks
        like it belongs. The capture flags are computed INSIDE NewFrame, and the mask
        reads them two lines later; put NewFrame in on_overlay and every read answers
        about the PREVIOUS frame — one frame of leakage, every time, invisible unless
        you look. 5.11 ADDS NO HOOK: 5.10's on_input already runs exactly once per
        frame before the simulation, which is the property this needs.
        HEADLESS IS A CONFIGURATION, NOT A FAILURE. start() returns false with no
        window or no renderer, logs at INFO (an error line in every --shot run trains
        the reader to ignore error lines — the same argument engine_set_warnings
        makes one layer up), and every other call is a safe no-op INCLUDING
        wants_keyboard(), so the mask blocks nothing and the program behaves exactly
        as it did before this lesson. That is what keeps 5.1's golden byte-identical.
        THE ONE SINGLETON IN THIS ENGINE, AND NOT BY CHOICE. asset_store (5.5),
        registry (5.8), action_map (5.10) and debug_lines (5.11) are all VALUES.
        debug_ui cannot be: ImGui keeps its context in a library global that every
        ImGui:: call reads. start() REFUSES a second instance and says so — an
        enforced limit you can read beats an undocumented one you discover.
        NO imgui.ini: ImGui persists window positions beside the WORKING DIRECTORY
        by default, so a tool would behave differently depending on where it was
        launched from — 3.5's reproducibility problem, in a UI.
  input-mask: TWO CONSUMERS OF ONE KEYBOARD, ARBITRATED ON LEVELS, NEVER BY ROUTING
        EVENTS. engine::masked_input<Source>, in core/actions.hpp.
        NEVER WITHHOLD AN EVENT. engine::input tracks LEVELS, and A LEVEL IS ONLY
        EVER CORRECTED BY THE EVENT THAT CONTRADICTS IT — so a key-UP routed to the
        UI and not passed on leaves that key held DOWN FOREVER, and no later event
        fixes it because the key is not released twice. Both consumers see every
        event; the arbitration happens one layer later.
        AND STILL CALL update(). Skipping it while the UI has focus freezes every
        level: hold [Left], click a text field, and the camera yaws forever.
        Updating THROUGH the mask reports masked keys as UP, which fires the RELEASE
        edge — not damage limitation but the behaviour you want, because focusing a
        text field genuinely should let go of the movement keys.
        5.10's CONCEPT PAID FOR ITSELF ONE LESSON LATER. masked_input is a different
        TYPE satisfying input_snapshot, so action_map::update took it with NOT ONE
        CHARACTER CHANGED. Against `const input&` the only options were an `if`
        inside the map (the mapper learning what a UI is) or a copy of input with
        fields cleared (a second source of truth about the keyboard).
        YOU CANNOT MASK A DELTA BY MASKING ONE OF ITS ENDPOINTS. A key is a level, so
        reporting it up is a complete lie. The cursor is not: action_map DERIVES a
        delta by differencing two frames. Cursor 100 -> 160 over three blocked
        frames, then on to 170:
          report 0 while blocked      release frame sees +170  (the whole screen)
          freeze the last position    release frame sees  +70  (THE ONE THAT SHIPS)
          virtual cursor (shipped)    release frame sees  +10  (one frame's motion)
        THE VIRTUAL CURSOR: reported = real - offset, and offset += (real -
        real_previous) on every blocked frame. Reported stops dead while blocked
        (delta exactly 0) and the offset stops GROWING the instant the block lifts,
        so the next difference is one frame's movement rather than the excursion.
        The cursor stays permanently 60 px behind and is permanently right about how
        far it moved, which is the only question anyone asked it.
        THE WHEEL NEEDS NONE OF THIS because input publishes it as a PER-FRAME DELTA
        already — zeroing a delta is exact, not a lie about a level. The machinery is
        needed only where the CONSUMER derives the delta.
  actions: AN ACTION IS A NAME; A BINDING MAPS A SIGNAL ONTO IT; A FRAME PUBLISHES
        THE VALUE. 5.10, engine/core/actions.hpp + actions.cpp (53 -> 54 public
        headers; FIRST new SOURCE file since 5.5, so engine/CMakeLists.txt changed).
        WHY, AND NONE OF THE FOUR IS ABOUT SPEED: a hard-coded scancode cannot be
        rebound, is about one device, cannot be recorded (a replay wants the
        INTENTION — the binding may have changed since), and SAYS THE WRONG THING
        (key_pressed(SDL_SCANCODE_SPACE) in a jump handler is a sentence about
        hardware in a file about jumping).
        THIS LESSON MEASURES NOTHING AND SAYS SO. The alternatives differ in what
        they can EXPRESS and all are nanoseconds. Producing a benchmark anyway
        would attach a number to a decision the number did not make, and the next
        reader would think the number was the reason. A design lesson says it is one.
        ONE MECHANISM FOR BUTTONS AND AXES: EVERY BINDING CONTRIBUTES A SIGNED
        FLOAT AND AN ACTION'S VALUE IS THE SUM. jump <- Space(+1); steer <- A(-1)
        and D(+1); look_x <- mouse dx * 0.5. Holding BOTH halves of an axis gives
        exactly 0 — and a design with separate button/axis kinds would have needed
        a documented rule for that case. There is none, because -1 + 1 = 0.
        NOT CLAMPED, deliberately: two keys on one button action sum to 2, which
        held() does not care about, and a mouse flick SHOULD be big. Clamping would
        need to know which kind of action it was looking at, which is the thing this
        design exists to avoid knowing.
        held() = |value| >= 0.5. The threshold is for sources that are NOT keys —
        an analog stick must be PUSHED, not brushed. fabs, so a two-way axis is
        "held" either way: legal, and rarely the question you meant.
        EDGES COME FROM THE ACTION'S LEVEL, NEVER FROM A BINDING'S. Bind jump to
        Space AND the left mouse button, then: Space down (ONE press), mouse down
        while Space held (NO second press — the action was already active), Space up
        while mouse held (NO release — still active), mouse up (ONE release).
        Binding-derived edges give two of each and A DOUBLE JUMP. THE BUG IS
        INVISIBLE WITH ONE BINDING PER ACTION, which is what a first implementation
        has and what a first test writes; it ships as "sometimes it jumps twice" the
        week somebody binds a controller. verify_510 §C binds two on purpose.
        A FIXED STEP NEEDS A SECOND KIND OF EDGE, and this is 1.4's trap closed
        rather than documented. on_fixed_step runs 0..N times, so pressed() read
        there FAILS BOTH WAYS: a two-step frame acts twice, a zero-step frame loses
        the press entirely. Neither is fixable by the caller without cross-frame
        state, so the map keeps it: every rising edge queues one press,
        consume_pressed() pops one. QUEUE IS 4 DEEP AND OVERFLOW IS DROPPED — a
        DECISION, not a bug: an unbounded queue replays a burst of jumps after the
        player stopped asking, which feels worse than losing them. A fighting game
        raises the number and calls it design.
        THE RULE FOR CALLERS: pressed()/released() in on_input, on_frame, on_event
        (each runs once per frame). In on_fixed_step use consume_pressed() for edges
        and held()/value() for LEVELS, which are frame-coherent and safe.
        find() DOES NOT DECLARE. A lookup that created things would turn a typo in a
        config file into a brand-new action with no bindings and no reader — a
        perfectly functioning thing that does nothing. declare() is idempotent, so
        two subsystems may both declare "quit" without coordinating.
        reset() CLEARS LEVELS, EDGES, THE QUEUE **AND THE MOUSE ORIGIN**. For focus
        loss and scene changes; without the last part the first frame back delivers
        the whole distance the cursor travelled while you were away.
        NOT A SINGLETON — the third application of 5.5's and 5.8's rule. A two-player
        local game needs TWO maps with different bindings, and a per-entity game puts
        one in a component. THE ENGINE DOES NOT KNOW HOW MANY OF A THING A GAME
        NEEDS, SO IT DECLINES TO DECIDE. What it DOES provide is the moment at which
        updating one is correct — see app-hooks.
        NO GAMEPAD, AND THE REASON IS STATED IN THE LESSON RATHER THAN BURIED: it
        could not be RUN on this machine, and untested device code in an engine is a
        liability that looks like support. §7 lists the five steps to add one and
        marks the SDL3 signatures as needing verification against SDL_gamepad.h
        (SDL3 renamed the whole SDL_GameController* family; axes are signed 16-bit
        and need normalising). The property the gamepad claim RESTS on — one action,
        several bindings, more than one device — IS demonstrated, with a keyboard
        and a mouse.
        THE HARD PART OF ADDING ONE IS DEVICE DISCONNECTION, and the right answer is
        in the SNAPSHOT, not the map: a disconnected pad answers false, the sum
        drops to zero, the level goes false, and the release edge falls out of the
        existing machinery. IF YOU FIND YOURSELF SPECIAL-CASING action_map, THE
        ABSTRACTION BOUNDARY IS IN THE WRONG PLACE.
  app-hooks: SIX HOOKS, AND 5.10 ADDED THE SIXTH BECAUSE NONE OF THE FIVE FIT.
        configure / on_start / on_event / **on_input** / on_fixed_step / on_frame /
        on_overlay / on_stop.
        on_input RUNS ONCE PER FRAME, AFTER INPUT IS PUBLISHED AND BEFORE ANY STEP.
        That is the only moment at which updating an action map is correct, and
        nothing else in the frame is it: on_event runs several times a frame or
        none; on_fixed_step runs 0..N times AND is where the answers get read; and
        on_frame is THE NEAR MISS — right frequency, wrong place, because it runs
        AFTER the steps, so every step would read LAST frame's actions. Invisible in
        a demo, a real 16 ms of input delay in a game.
        A HOOK THAT SERVES ONE CALLER IS A SMELL, so: on_input is also where a
        network snapshot is read, a debug scrubber sampled, or a replay's recorded
        intentions latched. It was missing before 5.10 happened to need it.
        FRAME-SCOPED EDGES ARE VALID IN on_input AND NOT IN on_fixed_step.
  concepts: DEPEND ON A SHAPE, NOT A TYPE — first use of a C++20 concept in the
        course, 5.10, and it was forced by a test that could not otherwise exist.
        input::update() samples SDL's LIVE keyboard state, so a harness that wants a
        key held would have to persuade SDL that a key is held. action_map::update
        is therefore templated on `input_snapshot`, which names the six questions it
        actually asks (key_down, mouse_down, mouse_x/y, wheel_x/y). The harness
        writes a struct with six functions and the shipping logic runs unchanged.
        A CONCEPT BEATS AN ABSTRACT BASE HERE ON THREE COUNTS: it does not change
        input.hpp (not one character of 1.2's header moved); it costs no virtual
        call on a per-binding-per-frame loop; and it is OPEN — engine::input
        satisfies it without knowing it exists, where a base class is only
        implemented by types whose author said so.
        A static_assert BESIDE THE CONCEPT KEEPS THE CLAIM HONEST: rename one of the
        six accessors in input.hpp and the engine fails to build with a message
        naming the requirement, rather than failing deep inside a template.
  hierarchy: THE MATHS WAS FREE; THE ORDER WAS THE LESSON. 5.9, in
        engine/include/engine/ecs/hierarchy.hpp + camera.hpp, both header-only —
        public headers 51 -> 53, no CMake change anywhere.
        world_from_local(child) = world_from_local(parent) * parent_from_local(child),
        and NOTHING in math/transform.hpp changed by a character: 2.8 named the
        function parent_from_local FOR THIS DAY, and said so in a comment at the
        time. Two components: `parent` {entity} and `world_transform` {mat4}.
        NO CHILD LIST, deliberately — children are derivable and a stored list is a
        second source of truth (the same argument registry::destroy makes about a
        component mask). The bill is visible in destroy_subtree, which sorts the
        world's links once, O(n log n), rather than maintaining an index forever.
        AN ENTITY WITH NO `parent` IS A ROOT, so 5.8's flat world is the SPECIAL
        CASE and nothing had to be migrated.
        THE ORDER IS THE PROBLEM AND IT IS REAL, NOT THEORETICAL. A parent must be
        resolved before its children; 5.7 established a pool's dense order is
        insertion order, disturbed by every swap-and-pop. verify_59 §C churns a
        48-entity 4-level tree and finds TWELVE sitting ahead of their own parent.
        A dense-order walk composes those against LAST FRAME'S matrix — wrong in a
        way nothing reports.
        DEPTH BUCKETING IS A TOPOLOGICAL SORT, and that is the whole idea: a
        parent's depth is always exactly one less than its child's, so a counting
        sort by depth is O(n) and puts every parent first. WITHIN A LEVEL THE ORDER
        DOES NOT MATTER AT ALL, which is why a level is a parallel_for Module 9
        will not have to design. It arrived with the choice of order.
        MEASURED, THREE CANDIDATES, -O2 -DNDEBUG, M4 Pro, medians of 25:
          DEPTH IS THE AXIS (n = 100,000 in every row, only the shape moves):
            depth      1     2     4     8    16    32
            recurse  3.34  6.55 10.08 11.81 13.29 13.55   <- depth-DEPENDENT
            levels   3.39  4.42  4.83  4.78  5.04  4.94   <- very nearly NOT
            ratio    1.01  0.67  0.48  0.40  0.38  0.36
          THE DEPTH-1 ROW IS THE CONTROL: no hierarchy, both arms identical work,
          1.01x. Without it nothing below is worth reading. And it SATURATES
          around depth 8.
          SIZE + ROW ORDER, depth 16, level-order relative to recursion:
            n =            100   1,000  10,000  100,000
            index, fresh   0.88   0.85   0.66    0.39
            packed, fresh  0.87   0.85   0.66    0.39
            index, SCRAMBLED 0.87 0.81   0.84    0.57
            packed, SCRAMBLED 0.87 0.81  0.65    0.38
          PACKING ONLY PAYS ONCE THE ROW ORDER HAS DECAYED: in creation order the
          rows are already nearly in level order and the permutation achieves
          nothing. Scrambling costs the index arm 5.00 -> 7.63 (+52%) and the
          packed arm 4.81 -> 4.91 (+2%).
          MAINTAINING IT (ns/entity, depth 8): reindex 3.98/4.16/8.98, permute
          7.73/7.94/7.96, one resolve 4.66/4.78/4.94 at n = 1e3/1e4/1e5. So
          A REBUILD IS ABOUT ONE RESOLVE AND A PERMUTATION ABOUT 1.6.
          THE SHIPPED RESOLVER vs THE PROBE: 1.22x / 1.32x / 1.40x. THE ECS COSTS
          22-40% ON THIS PASS — three sparse lookups per entity instead of three
          array reads — and that number is published rather than hidden.
        DECIDED: LEVEL ORDER THROUGH AN INDEX, NO PERMUTATION. It takes all of the
        depth-independence, needs no row movement, and is the same SHAPE as the
        packed version so a world that outgrows it adds a permutation to rebuild()
        without touching the loop. Wrong when: >1e4 entities, decayed rows, stable
        shape (a streaming world, a crowd).
        REBUILD IS SPLIT FROM RESOLVE AND THAT IS WORTH MORE THAN THE ORDER.
        rebuild() reads the parent links and produces the level order — needed only
        when the SHAPE changes. resolve() runs every frame. A game re-parents
        rarely and moves constantly. mark_topology_changed() is A FLAG THE CALLER
        SETS, because 5.8 ships no signals; resolve() asserts order_.size() ==
        transform pool size, which is the strongest check available without a
        version counter AND CANNOT SEE an add plus a remove between two resolves.
        Said out loud rather than papered over; Module 9 revisits it with the editor.
        THE RESOLVE LOOP HAS NO recursion, NO stack, NO visited set and NO "has my
        parent been done yet" test, because THE ORDER ALREADY GUARANTEES what those
        would check. Three "is this a root?" cases — no parent component, dead
        parent, parent with no world_transform — collapse to one line, which is what
        makes the orphan policy cheap as well as defensible.
        TWO POLICIES, BOTH STATED:
          ORPHAN (parent died) -> BECOMES A ROOT, keeps its local transform, and is
            COUNTED in hierarchy_report::orphans. Destroying the subtree is a policy
            a GAME may want and a transform system must not impose; destroy_subtree()
            is the explicit tool.
          CYCLE -> BROKEN, COUNTED, LOGGED. A wrong picture is recoverable; A HANG IS
            NOT. Two defences because `parent` is a PUBLIC component: set_parent()
            refuses to create one (walking the whole chain, not one link), and
            rebuild() marks k_in_progress and breaks the chain if one exists anyway.
        NOT SHIPPED, WITH THE NUMBER: DIRTY FLAGS. moved -> reached -> ratio at
        1e5/depth 8: 0.1%->0.5%->0.08x, 1%->4.8%->0.15x, 5%->20.7%->0.40x,
        10%->36.4%->0.65x, 25%->66.3%->1.04x, 50%->87.6%->1.19x, 100%->100%->1.29x.
        THE AMPLIFICATION IS THE PART NOBODY QUOTES — every descendant of a moved
        entity has to move too. CROSSOVER AT ~25% MOVED, and past it the
        "optimisation" is SLOWER. This engine's demo animates 100% of its entities
        every step, so shipping it would have cost 1.29x. A measurement lesson has
        to be willing to conclude NO; 5.9 concludes no twice.
  camera: A CAMERA IS AN ENTITY, NOT A KIND OF THING. 5.9, ecs/camera.hpp.
        Four components: transform (where), world_transform (…resolved, so it can
        be PARENTED), camera {fovy, near_plane, far_plane}, active_camera (a TAG).
        THREE CONSEQUENCES, NONE OF WHICH NEEDED DESIGNING: it can be parented
        (attach it to a car and it rides, resolved by the same pass); "which camera
        is active" is a COMPONENT rather than a pointer, so a destroyed camera
        leaves no dangle; and it is findable by view<camera, active_camera>().
        active_camera IS AN EMPTY STRUCT AND THAT IS THE FEATURE — a component with
        no data is a tag and its presence is the information. Costs 1 byte/row
        because C++ has no zero-sized objects; an engine with hundreds of tags would
        specialise the pool.
        ASPECT IS DELIBERATELY NOT A FIELD. It belongs to the SURFACE, which the
        user can resize; storing it means every camera in a scene file carries a
        number that was true on the machine that saved it. projection_of(c, aspect).
        THE VIEW MATRIX IS rigid_inverse OF THE PLACEMENT, and 2.9 already derived
        it — for M = affine(R, t) with R orthonormal, M^-1 = affine(R^T, -R^T t).
        Extracted into math/mat4.hpp alongside is_rigid(). STILL NO GENERAL 4x4
        INVERSE, for 2.9's reason: it would answer a question we never ask.
        THE IDENTITY, CHECKED BIT FOR BIT (verify_59 §D, worst element 0.000e+00):
          rigid_inverse(parent_from_local(look_along(e,t,u))) == look_at(e,t,u)
        look_along answers "where must the camera BE"; look_at answers "what matrix
        takes the world into its view". Inverses, and now both are spelled out.
        is_rigid ACCEPTS A REFLECTION (det -1) on purpose — a mirrored camera is
        legitimate and the inverse is still correct. The check exists to catch SCALE,
        and a scaled camera is a category mistake (it changes fov by changing units),
        so it is an ASSERTION rather than an error.
  measurement: A/B TIMING HAS FOUR RULES AND engine/core/bench.hpp IS THEM.
        1 ALTERNATE THE ARMS — one rep of A, one of B, microseconds apart, never
          in blocks. 3.10 published a 10% "improvement" that was session drift.
        2 REPORT THE MEDIAN AND THE SPREAD. A hiccup moves a mean, not a median;
          a median alone hides a bimodal result. If one arm's range straddles the
          other's median the honest word is "tie".
        3 STOP THE OPTIMISER DELETING THE WORK. bench_keep() stores through an
          `inline volatile double`, which the standard requires to happen.
        4 CHECK BOTH ARMS COMPUTED THE SAME ANSWER. bench_ab carries `agree`
          (bit-identical) AND `max_rel_diff`, because an arm that VISITS THE SAME
          ELEMENTS IN A DIFFERENT ORDER sums them in a different order and FP
          addition is not associative. Exact is the right default; >1e-9 is not
          rounding, it is different work.
        QUOTE RATIOS WITHIN A PAIRING, NEVER ABSOLUTES ACROSS PAIRINGS. At large N
        the two arms evict each other, so one arm's own median moved 0.98 -> 1.83
        ns/item depending on who it was measured against. Alternation protects
        against thermal drift and INTRODUCES cache interference; both arms in a
        pairing paid the same interference.
        THREE WAYS A MICROBENCHMARK LIES, all three hit in 5.6:
          - the timer is coarser than the work (24 MHz => 41.67 ns per tick; a
            pass over 4 objects read 0 or 1 ticks). Tell: a round number, often 0.
          - the compiler deleted the loop (a pure function of nothing, repeated).
            Tell: A NUMBER THAT IS NOT PHYSICALLY POSSIBLE — 0.166 ns for four
            matrix builds is 2.3 cycles for 9 multiplies and 16 stores.
          - the accumulator IS the bottleneck (`hits += 1.0` is a ~4-cycle serial
            chain). Tell: two arms that should differ, agreeing exactly.
        SANITY-CHECK EVERY PUBLISHED NUMBER AGAINST THE HARDWARE. ns -> cycles ->
        instructions, and instructions are countable. No tool finds this one.
        AN EFFECT THAT SURVIVES THE REMOVAL OF ITS EXPLANATION HAD A DIFFERENT
        EXPLANATION. Find the knob that turns off the hypothesised mechanism
        (-fno-vectorize; a scene small enough for L1; a monomorphic version of a
        polymorphic call) and see whether the effect goes with it.
        RELEASE BUILDS ONLY for any timing claim. A debug build measures the
        optimiser's absence.
  assets: AN ASSET IS FOUND BY NAME, LOADED ONCE, AND EXPLICITLY UNLOADED.
        A NAME IS NOT A PATH. "torus.obj" is stable, recorded in a scene file, and
        the key the store caches on; /Users/…/assets/torus.obj is where that name
        resolved TODAY. Store the first, compute the second.
        engine::search_path is an ORDERED LIST OF ROOTS, and order IS the feature:
        prepend_root shadows a shipped asset without moving or deleting anything —
        mods, localisation packs, live editing out of the source tree, and a test
        fixture standing in for a real asset are all the same mechanism.
        search_path::beside_executable(subdir) is THE ONLY CALLER OF
        SDL_GetBasePath() IN THE ENGINE (was 2 before 5.5, was 3 counting the
        demo's own path assembly). Assets, shaders and Module 7's sounds each get
        their own root from one rule.
        resolve() REFUSES a name that is absolute, empty, or contains "..", before
        any root is tried — SYNTACTICALLY, not by canonicalising, because
        canonicalisation needs the filesystem and grows bypasses. It also requires
        SDL_PATHTYPE_FILE: a directory of the right name is not a hit.
        NO REFERENCE COUNTING ON HANDLES, and the argument is from 5.4's
        properties rather than from taste: a refcount needs a copy constructor, a
        destructor and a pointer to the store, which costs 4 bytes/trivially
        copyable/memcpy-able-into-a-component/serializable-without-fixups — every
        property that made a handle worth having. A refcounted handle is a
        shared_ptr with extra steps.
        SO: EXPLICIT UNLOAD, safe to get wrong because 5.4 made staleness
        detectable. Forget to unload -> a LEAK, which live_count() finds. Unload
        too early -> a NULL and collect_stats::unresolved. Neither is a crash.
        DERIVED ASSETS ARE THE ONE PLACE A LIFETIME RULE IS UNAVOIDABLE: a derived
        asset is OWNED BY ITS SOURCE and unloading a source cascades TRANSITIVELY.
        It has no name, because nobody asked for it by one. Deriving from a stale
        source is REFUSED (an orphan has no name to find it by and no source to
        free it with). A cascade that stops after one hop is a leak.
        IMPORT SETTINGS ARE PART OF AN ASSET'S IDENTITY. The same file imported two
        ways is two meshes with two vertex arrays, so the key is name + settings
        (engine::mesh_import). THE DEFAULT CONFIGURATION MUST SERIALISE TO NOTHING:
        asset_key("torus.obj", {}) == "torus.obj", and only a non-default import
        earns a "|flip=0" suffix — otherwise generated content (bare name) and
        loaded content (decorated name) live in two key spaces wearing one name.
        THE NAME MAP IS A HINT; THE POOL IS THE TRUTH. Every cache probe nests a
        pool::contains() inside the map lookup, so an asset freed by handle leaves
        no live-looking entry. A pointer-keyed cache could never ask.
        LOAD FAILURE IS THE ORDINARY CASE: null handle + a report, logged ONCE at
        the point that knows why (resolved_path::refused exists so the caller can
        tell "already reported" from "mine to report"). No exception, no assertion,
        and NO FALLBACK ASSET — substituting a default cube removes the game's
        ability to notice.
        NOT A SINGLETON, and the reason is concrete rather than stylistic: sandbox
        holds THREE asset_stores in one program. platform and app deliberately do
        not own one either — that would be a singleton with better manners.
  handles: A REFERENCE INTO THE ENGINE IS AN INDEX PLUS A GENERATION, NEVER A
        POINTER. engine::handle<T> is ONE 32-BIT WORD split 20 index / 12
        generation (the same split EnTT uses for entt::entity), packed
        generation-high. T is a PHANTOM parameter — nothing of it is stored,
        sizeof is 4 for every T, T may be incomplete — so handle<mesh_data> and
        handle<texture> are DIFFERENT TYPES and a mix-up is a compile error.
        GENERATION 0 IS RESERVED, which makes the null handle all-bits-zero for
        free: a value-initialised member, a default-constructed vector element and
        a memset struct are all null with no code.
        THE THREE FAILURES, and this is the derivation, not a list:
          dangling    the object was freed
          aliasing    …and something took its place (ABA). A BARE INDEX MAKES
                      THIS WORSE — it converts a probable crash into a guaranteed
                      silent wrong answer, which is why the generation exists.
          relocation  nothing was freed; the container moved. An index survives
                      this and a pointer cannot.
        THE GENERATION IS BUMPED ON REMOVAL, never on insertion. A freed slot then
        already carries a value no outstanding handle has, so "is this slot
        occupied" needs no separate flag and no window exists in which a stale
        handle matches.
        THE BUDGET IS MONITORED, NOT ASSUMED. 4,095 usable generations means a
        slot's generation REPEATS after 4,095 frees of that slot — 68 s at 60 Hz,
        4.7 days at one level load per 100 s. Unreachable for assets, worth
        thinking about for entities. pool::generation_wraps() counts it and the
        pool logs a warning; if it ever leaves zero the two fixes are a FIFO free
        list (spreads reuse across all slots, multiplying time-to-wrap by the slot
        count) or 64 bits split 32/32.
        THE ONE RULE FOR USERS: RESOLVE LATE, USE IMMEDIATELY, NEVER STORE. get()
        returns a T* valid until the next insert or remove and not one instruction
        longer. Storing one re-creates the bug the design abolished, wearing
        modern clothes.
        RESOLVE AT THE BOUNDARY, ONCE PER OBJECT — not once per use and never once
        per vertex. Measured: 0.98 ns vs 0.85 ns for a raw dereference, so +0.13 ns
        per resolution; per object per frame that is 0.003% of a 60 Hz budget on a
        4,096-object scene, and per vertex it would be a different conversation.
        A HANDLE IS HALF A REFERENCE; THE POOL IS THE OTHER HALF. Every function
        that resolves one takes the pool as a parameter (collect_triangles went
        8 -> 9 params, and Lesson 5.1's reduction is not being walked back — this
        is the cost of the design, made visible rather than hidden in a global).
        A GLOBAL WAS REFUSED and there is a concrete reason: sandbox has TWO
        mesh_pools in one frame by the end of 5.4.
  logging: TWO AXES, NEVER ONE. A CATEGORY says WHO is speaking (a noun, a part of
        the program); a PRIORITY says HOW MUCH IT MATTERS. Collapsing them — a
        `LOG_ERROR` *category* — destroys the mechanism, because you can then never
        say "errors from the GPU but not from assets".
        FIVE CATEGORIES, based at SDL_LOG_CATEGORY_CUSTOM (never at a literal —
        SDL reserves everything above that enumerator for applications, and a
        number would collide the week SDL adds one):
          log_core  log_platform  log_gfx  log_gpu  log_asset
        A static_assert ties the name table to the enum, so adding one without a
        name does not compile. log_gfx currently has ZERO users: the CPU
        rasterizer never logs. Recorded rather than hidden; Module 6 fills it.
        SIX LEVELS, and the meanings are PROMISES that 92 call sites obey:
          error     the operation did not happen, and the caller is being told so
          warn      something went wrong and WE RECOVERED  <- the whole distinction
          info      a fact worth having on a bug report
          debug     what a subsystem did, once per operation
          trace     per-frame or per-item detail
          critical  the program cannot continue
        THE MECHANICAL TEST between error and warn: DID THE OPERATION HAPPEN?
        WHO LOGS A FAILURE: the DEEPEST point that knows WHY. Callers propagate
        SILENTLY and add only the CONSEQUENCE — the thing they know that the
        callee does not. Chosen over "the caller decides the level" because the
        cost of THIS rule is visible (a recovering caller gets an error line it
        did not deserve, fixable by demoting that call to debug) and the cost of
        the other is invisible (detail lost at every layer boundary).
        THE PAYOFF IS FREE AND THAT IS THE ARGUMENT AGAINST A WRAPPER. SDL's
        documented default table is `app=info,assert=warn,test=verbose,*=error`,
        so every category SDL does not know about — i.e. every one of ours —
        defaults to ERROR. Moving the engine off SDL_LOG_CATEGORY_APPLICATION
        makes it quiet with no configuration and no filtering code. A hand-rolled
        logger would have had to reimplement that and would have got a different
        default.
        DEMOS KEEP SDL_Log ON PURPOSE (122 of them). A demo IS the application;
        APPLICATION/info is exactly what its output is, and it is on by default
        because the person who ran the program asked for it.
        `--log SPEC` works on EVERY program: platform::start reads it first thing,
        and app_runner::init fills in argv when the app left it null. Grammar
        borrowed from SDL's own SDL_LOGGING hint so two things a person might type
        do not need two mental models. `*` means OUR categories only — silencing
        SDL's diagnostics is not ours to do on the user's behalf.
        THE PARSER VALIDATES EVERYTHING BEFORE APPLYING ANYTHING. A rejected spec
        is a NO-OP. Parsing straight into SDL_SetLogPriority would leave the good
        first entry applied after a bad third one, and the user would have a
        configuration they did not ask for and cannot see.
        THE FILE SINK CHAINS rather than replaces: SDL_GetLogOutputFunction before
        SDL_SetLogOutputFunction, and call the previous one. "Also log to a file"
        must not silently cost you your console. SDL holds a mutex across the
        hook, so it is thread-safe for free. SDL FILTERS BEFORE THE HOOK, so a
        message below its level never reaches the sink — the file and the level
        are independent controls.
  assertions: AN ASSERTION IS NOT AN ERROR, and one question separates them:
        COULD A CORRECT PROGRAM, ON A WORKING MACHINE, ENCOUNTER THIS?
          yes -> an ERROR. The world did it to you. Return it; it ships.
          no  -> an ASSERTION. Your own code is wrong. Stop; it may be compiled out.
        THREE MACROS:
          ENGINE_ASSERT(c)  debug only. The default. NO SIDE EFFECTS in c.
          ENGINE_CHECK(c)   every build. Only where continuing is worse than stopping.
          ENGINE_VERIFY(e)  the expression ALWAYS runs; the check is debug only.
        SDL DECIDES ITS LEVEL FROM __OPTIMIZE__, NOT NDEBUG (SDL_assert.h). So -O2
        ALONE takes you to SDL_ASSERT_LEVEL 1 — SDL_assert disabled,
        SDL_assert_release live — with no -DNDEBUG anywhere. OUR log floor keys off
        NDEBUG. TWO GATES, TWO SWITCHES: `-O2` gives dead assertions + live trace
        logging; `-O0 -DNDEBUG` gives the exact inverse. Neither is what you guess.
        SDL_disabled_assert WRAPS THE CONDITION IN sizeof — compiled, never
        evaluated. Good (the condition cannot rot, and `assert(fp = fopen(...))`
        still does not open the file) and a trap (this is why ENGINE_VERIFY exists).
        SDL_enabled_assert IS A `while (!(condition))` LOOP, so SDL_ASSERTION_RETRY
        genuinely RE-TESTS: fix state in a debugger, continue, proceed as though the
        bug had not happened. ALWAYS_IGNORE LATCHES in a static per expansion site.
        ASSERTIONS ARE TESTABLE: SDL_SetAssertionHandler returning IGNORE, plus
        SDL_GetAssertionReport()'s linked list (condition text, file, line,
        trigger_count). verify_53 §F proves an assertion fires on the input that
        should fire it — the thing everyone assumes is impossible.
  errors: THE ENGINE HAD ALREADY CONVERGED ON THE ANSWER TWICE. obj_report (3.5)
        and gpu_report (4.2) are both: a STATUS enum naming the failure precisely,
        the FACTS you ask for next, and a `bool ok()`. Written a module apart by
        nobody trying to match the other. 5.3 NAMES that shape rather than
        inventing a fourth, and converts the odd one out (image_status ->
        image_report).
        THE FOUR RULES:
          1. status + report + ok() when a caller might BRANCH on the failure, or
             when there are diagnostics worth having on SUCCESS too.
          2. A BARE `bool` IS HONEST WHEN THERE IS NOTHING MORE TO SAY
             (platform::set_vsync: one failure mode, one sane response).
          3. PER-SUBSYSTEM status enums, NOT one engine-wide one. A global enum
             becomes a junk drawer where bad_format means six things. The SHAPE is
             unified; the vocabularies are not, and should not be.
          4. "returns false and has ALREADY logged" is retired as an unwritten
             contract — it is now the documented who-logs rule above.
        A REPORT DESCRIBES WHAT IT REFUSED: image_report fills in width/height
        BEFORE the size check, so `too_large` says how large. A report that
        describes the thing it rejected is a diagnosis; one that does not is a
        complaint.
        std::expected IS C++23 AND WE ARE NOT ADOPTING IT YET. Reason recorded:
        the engine's loaders return FACTS, not just values, and they return them
        whether or not they worked — expected has nowhere to put obj_report's
        twelve statistics. Exercise 9.5 builds one anyway. ADOPT A NEW ABSTRACTION
        WHEN THE OLD ONE HAS FAILED YOU, NOT WHEN THE NEW ONE IS ELEGANT.
  program-entry: A PROGRAM PICKS ONE OF TWO ARRANGEMENTS AND THE ENGINE SUPPORTS
        BOTH. Library: construct engine::platform, write your own main() and your
        own while(plat.running()). Framework: derive from engine::app, override
        the hooks, and write ENGINE_MAIN(YourApp) — no main(), no loop.
        engine::app IS IMPLEMENTED ON engine::platform, NEVER BESIDE IT. Anything
        app can do must be reachable from the library path, or the library path
        has quietly become second-class. verify_52 §E renders six frames down each
        path and compares the framebuffers byte for byte.
        <engine/platform/main.hpp> GOES IN EXACTLY ONE .cpp PER PROGRAM, AND THAT
        FILE MUST NOT DEFINE main(). It defines SDL_MAIN_USE_CALLBACKS and
        includes <SDL3/SDL_main.h>, which emits a NON-INLINE definition of
        SDL_main() plus the platform entry point. Two inclusions = duplicate
        symbol _main. That is the ODR, not an SDL quirk — and it is why the entry
        point cannot live in libengine.a: an entry point is not a library's to own.
        It is DELIBERATELY ABSENT from engine.hpp, the umbrella: "include
        everything, it is harmless" must not be a way to acquire a main().
        THE FOUR SDL3 SIGNATURES, verified against SDL 3.4.12:
          SDL_AppResult SDL_AppInit   (void **appstate, int argc, char *argv[]);
          SDL_AppResult SDL_AppIterate(void *appstate);
          SDL_AppResult SDL_AppEvent  (void *appstate, SDL_Event *event);
          void          SDL_AppQuit   (void *appstate, SDL_AppResult result);
        Returns: SDL_APP_CONTINUE / SDL_APP_SUCCESS / SDL_APP_FAILURE.
  surfaces: THE SURFACE IS CHOSEN BEFORE ANYTHING EXISTS, in app_config, because
        by the time you could regret it the window already belongs to somebody.
          surface::renderer  SDL_Renderer + streaming texture (Module 1's path)
          surface::gpu       a window claimed by NOBODY, for gpu_device::create
          surface::headless  no window, and NO SDL_INIT_VIDEO — runs with no display
        This is Lesson 4.2's "a window is claimed by an SDL_GPU device or driven by
        an SDL_Renderer, never both" turned from a comment into an enum.
        HEADLESS DOES NOT INITIALISE VIDEO, and that is the point rather than an
        optimisation: SDL_Init(SDL_INIT_VIDEO) FAILS on a build server. The
        pre-5.2 `sandbox --shot` called it and never used it, so the
        characterization test built specifically for automation could not run
        anywhere automatic. verify_52 §B pins it (SDL_WasInit == 0).
  edges-and-levels: EDGES BELONG TO THE FRAME, LEVELS BELONG TO THE STEP.
        on_fixed_step runs 0..N times per frame (measured: 0,1,2,1,2 across five
        frames within 1.6 ms of each other at 60 Hz), so an edge query inside it
        fires TWICE on a two-step frame. key_down() in a step is safe — input is
        frame-coherent since Lesson 1.2, so every step in one frame sees the same
        snapshot. key_pressed() is not. Discrete presses go in on_event, where
        event.key.repeat is also available.
  resource-names: NAME EVERY GPU RESOURCE AT CREATION, through
        SDL_PROP_GPU_{BUFFER,TEXTURE,TRANSFERBUFFER}_CREATE_NAME_STRING — never
        through SDL_SetGPU{Buffer,Texture}Name. SDL's own docs on the setter:
        "You should use SDL_PROP_GPU_BUFFER_CREATE_NAME_STRING with
        SDL_CreateGPUBuffer instead of this function to avoid thread safety
        issues", and "This function is not thread safe".
        LESSON 4.3's COMMENT ABOUT THIS WAS WRONG AND IS NOW CORRECTED IN PLACE.
        It claimed the creation property existed for shaders "because a shader is
        immutable the moment it exists", inferring a reason from the asymmetry
        with the buffer/texture setters. There was no asymmetry to explain: the
        property is the recommended path for all three and the setters are the
        older API. A CONFIDENT EXPLANATION OF WHY SOMEBODY ELSE'S API IS SHAPED
        THE WAY IT IS, IS A HYPOTHESIS — and the cheapest place to test it is the
        documentation of the function you are already calling.
        NEITHER API HAS A GETTER. A name is write-only from the program's side, so
        it cannot be asserted on in a harness; only a capture shows it.
        engine::create_named_{buffer,texture,transfer_buffer} in gfx/gpu_debug.hpp
        are the wrappers. A failed property allocation costs the NAME and never
        the RESOURCE — a debugging aid must not be able to break what it aids.
        Cost: 0.0011 -> 0.0018 ms per buffer, paid once, at load.
  debug-groups: A DEBUG GROUP IS A C++ SCOPE, engine::debug_group, and it is
        NEITHER COPYABLE NOR MOVABLE. That is SDL's Metal rule expressed in the
        type system: "On some backends (e.g. Metal), pushing a debug group during
        a render/blit/compute pass will create a group that is scoped to the
        native pass rather than the command buffer. For best results, if you push
        a debug group during a pass, always pop it in the same pass."
        The engine pushes FOUR a frame (upload / clear / blit / scene) and opens
        the scene's group OUTSIDE its pass, closing after it — legal everywhere.
        ON D3D12 ALL THREE CALLS NEED WinPixEventRuntime.dll in PATH or beside the
        executable, and without it they are INERT, not an error: a capture with no
        tree and no diagnostic.
        Measured at ~183 ns per push+label+pop, so four a frame is 0.73 us =
        0.004% of a 16.7 ms frame. THEY SHIP ON.
  measure-the-noise-floor: BEFORE COMPARING TWO TIMINGS, MEASURE THE SPREAD OF THE
        IDENTICAL WORKLOAD RUN SEVERAL TIMES, and require any claimed effect to
        beat it by a factor of TWO. verify_49 §C runs the same frame five times
        and takes the range of the medians (2.46 us here).
        THE FIRST VERSION SKIPPED THIS AND REPORTED A NEGATIVE COST FOR ADDING
        WORK (-0.0003 ms for adding a debug group), which is the tell that a
        measurement has nothing to say. TWO runs was also not enough — a single
        difference is itself a sample of a noisy quantity, and the two-run version
        let -541.7 ns through the guard.
        WHEN AN EFFECT IS BELOW THE FLOOR, SCALE THE WORKLOAD UNTIL IT CLEARS IT
        AND DIVIDE: 1 and 8 groups are unmeasurable, 32 and 128 give 183.6 and
        182.0 ns, and THE AGREEMENT BETWEEN TWO INDEPENDENT ESTIMATES IS THE
        EVIDENCE. Same trick 3.10 used on a profiler zone reading 0.00 us.
  frame-log: THE ENGINE CAN PRINT ITS OWN COMMAND STREAM — engine::frame_log,
        armed by [P] for ONE frame, or by `engine --trace` which prints one frame
        and exits (the headless/CI mode). 33 events, 4 groups, 3 draws, 560
        uniform bytes for 4.8's scene.
        RECORDED FROM THE STATEMENTS THAT ISSUE THE CALLS, never from a parallel
        description of what render() is believed to do. Instrumentation that can
        drift from the code it describes is worse than none, BECAUSE IT IS
        BELIEVED. verify_49 §D asserts the log's draws / uniform bytes / triangles
        against draw_stats — two counters incremented from the same statements,
        which is a test A CAPTURE CANNOT GIVE YOU (a capture is a picture of what
        happened and has no independent account of what should have).
        Fixed-size name buffer (48 B) and fixed event capacity, because the log
        runs inside the frame it measures and an allocation per event would make
        the measured thing differ from the shipped thing. SDL_strlcpy, which
        always null-terminates; strncpy does not.
        WHAT IT CANNOT DO, and why the tools exist: resource CONTENTS at an event,
        replay-to-a-draw, live pipeline state, shader disassembly, per-draw GPU
        timing, per-fragment stepping. All of those need the frame REPLAYED, which
        needs the driver.
  frame-debuggers: RENDERDOC DOES NOT SUPPORT METAL. Its front page: "available
        for Vulkan, D3D11, D3D12, OpenGL, and OpenGL ES development on Windows,
        Linux, Android, and Nintendo Switch". SDL_GPU picks Metal on macOS, so a
        Mac reader uses XCODE'S METAL DEBUGGER — Debug > Debug Executable…, set
        "GPU Frame Capture" to "Metal" in the scheme's Options, run, click the
        Metal icon. No Xcode PROJECT is needed. SDL_gpu.h has a "Debugging"
        section that says all of this and is the first place to look.
        PIX is D3D12-only and is the better GPU PROFILER; RenderDoc is the better
        STATE INSPECTOR. Module 9 returns to that split.
        HOW A CAPTURE WORKS, and it explains every limit: the tool records the
        full CONTENTS of every resource at frame start plus the ordered call list,
        then REPLAYS. That is why it can show any resource at any event, why
        captures are huge, and why the tool must sit at the driver level.
        FOUR QUESTIONS TO ASK ONE: is my draw there at all (missing = CPU-side;
        present but invisible = culled/clipped/depth-rejected); is the right thing
        bound; are the bytes what I think; where did the geometry go. PREDICT
        BEFORE LOOKING — browsing without a prediction generates the feeling of
        investigating and no information.
  vertex-reuse: THE TRUE VERTEX-SHADER INVOCATION COUNT IS NOT MEASURABLE ON THIS
        MACHINE and 4.5's promise that "4.9's RenderDoc capture is where the real
        number finally shows up" IS NOT KEPT. Said plainly in the lesson rather
        than fabricated. Read it from: RenderDoc's Pipeline State statistics or
        VK_QUERY_TYPE_PIPELINE_STATISTICS / VERTEX_SHADER_INVOCATIONS; D3D12's
        D3D12_QUERY_TYPE_PIPELINE_STATISTICS -> VSInvocations; Xcode's Metal
        Debugger per-pipeline counters. Exercise 4.9.5.
        A MODEL INSTEAD, with assumptions stated: a FIFO post-transform cache
        simulated over torus.obj's own index order. best 1225, worst 6912, and the
        curve is a STAIRCASE — 6 through 32 all give 2400, 48 gives 2306, 50+
        gives 1225 — because make_torus emits ring by ring, so reuse happens at
        two distances (a few positions, and ~2*24) and nothing in between.
        THE FINDING IS BIGGER THAN THE QUESTION: REUSE IS A PROPERTY OF THE INDEX
        ORDER. Control: the same 2,304 triangles shuffled go from 2,400 to 6,784
        invocations at cache 32, 2.83x worse, with nothing about the geometry
        changed. This is why real pipelines run an index optimiser (Forsyth,
        meshoptimizer) as a build step.
        The model assumes FIFO (may be LRU), whole vertices (may be fixed-size
        outputs), strictly in-order indices (hardware batches and may reorder),
        and one cache (usually several).
  validation-cost: 1.17x TO RECORD a 3-draw frame (0.0267 vs 0.0228 ms), which
        pays off the promise main.cpp has carried since 4.2. MEASURED ON THE
        RECORDING, not the whole frame, because validation runs on the CPU as each
        call is recorded and a whole-frame number would dilute it with GPU time.
        DO NOT READ 1.17x AS "VALIDATION IS CHEAP": the cost is PER API CALL, so
        it scales with how chatty the frame is. The useful form is "about X per
        call, and I know how many calls I make" — and the frame log counts them.
        Keep it ON while developing; 4.2's argument stands.
  draw-rates: FOUR RATES OF CHANGE, and data is grouped by RATE rather than by
        subject. per FRAME — camera_uniforms (64 B, vertex slot 0) and
        scene_light_uniforms (64 B, fragment slot 0), pushed once before the pass.
        per DRAW — object_uniforms (112 B, vertex slot 1) and material_uniforms
        (32 B, fragment slot 1), pushed BETWEEN draws; SDL licenses this in one
        sentence ("Subsequent draw calls in this command buffer will use this
        uniform data"). per INSTANCE — a vertex buffer at INSTANCE rate (4.5).
        per VERTEX — a vertex buffer at VERTEX rate (4.5).
        The camera and the lamp share a push because both change once a frame,
        not because they are related. Grouping by subject is how you end up
        re-pushing something 1,225 times.
        Measured: a three-object scene moves 128 + 3*144 = 560 bytes of uniform
        traffic a frame, and would move the same 560 if each object had a million
        triangles. PER-DRAW OVERHEAD SCALES WITH OBJECT COUNT, NOT OBJECT SIZE.
  one-draw-per-object: A MATERIAL AND A CULL MODE CANNOT RIDE ON A TRIANGLE. 3.8
        flagged raster_triangle::surface in writing as a cheat that would not
        survive Module 4; this is where the bill arrives. A GPU draw is a LAUNCH,
        not a loop — thousands of fragments enter at once with no moment between
        triangle 7 and triangle 8 — so anything constant across a draw is fixed
        BEFORE it: as pipeline state (cull, fill, depth op, blend, the shaders) or
        as a uniform push (the numbers). Four objects is four draws.
        surface_style is the enum for the part that cannot be a number: solid
        (cull back), two_sided (cull none), wireframe (fill LINE). Three values,
        three pipeline objects at startup. THREE AND NOT MORE because every extra
        axis MULTIPLIES: 2 cull x 2 fill x 2 depth is 8.
        render() DOES NOT SORT. Sorting has several right answers that conflict
        (by pipeline, front-to-back for early-z, back-to-front for transparency,
        by distance for LOD) and the choice belongs to whoever knows what the
        frame is for. It COUNTS instead: pipeline_binds against
        ideal_pipeline_binds. Measured 3 vs 2 on a three-object, two-style scene.
  normals-are-data: A VERTEX SHADER SEES ONE VERTEX, so it cannot compute a face
        normal. collect_triangles could and did — cross(b-a, c-a) inside the
        per-triangle loop, whenever normal_at() returned zero. Three of the four
        built-in meshes carry no normals (cube, quad, icosahedron) and neither
        does 3.2's ground plane, so the fallback had to move OUT of the renderer
        and INTO the geometry. engine::with_normals(mesh, normal_style) is that
        step, and it is where every real engine puts it (aiProcess_GenNormals).
        FLAT FORCES UNSHARED VERTICES — one normal per face, and a vertex holds
        one — so a cube's 8 positions become 36 and an icosahedron's 12 become 60,
        with an index buffer of 0,1,2,3,... and no sharing left to express. That
        is 4.5's `expanded` form arriving for a reason other than a comparison.
        SMOOTH keeps the buffers' shape and averages, WEIGHTED BY AREA FOR FREE:
        |cross(b-a, c-a)| IS twice the triangle's area, so accumulating the
        UN-NORMALISED cross products and normalising once at the end weights each
        face by its area at no cost. Normalising per face first is more work AND
        throws the weighting away. Verified on a cube corner: the averaged normal
        dotted with (-1,-1,-1)/sqrt3 is 1.000000.
        GEOMETRY THAT ALREADY HAS NORMALS IS RETURNED UNCHANGED, whatever style is
        asked for. A file's normals are authorship — 3.5's loader rule, one level
        up. torus.obj's pass through bit for bit (verified).
        AND A RUNTIME TOGGLE BECAME A BUILD-TIME DECISION: 3.8's [Q] flat/smooth
        key is now a property of the vertex buffer, and the two options no longer
        share one. First time in this course that moving to the GPU took something
        away.
  matrix-3x3-uniform: A 3x3 CROSSES A CONSTANT BUFFER AS THREE EXPLICIT COLUMNS,
        never as float3x3. Both occupy 48 bytes (each column padded to a 16-byte
        register), so nothing is saved either way — what the matrix type ADDS is a
        dependence on the compiler's matrix-packing default (column_major for DXC;
        row_major if any #pragma pack_matrix upstream says so), with no diagnostic
        when it is not what you assumed. A transposed normal matrix does not crash:
        it lights every non-symmetric object from a slightly wrong direction.
        The shader rebuilds M*v by its definition — c0*v.x + c1*v.y + c2*v.z —
        which is 2.5's "a matrix is where the basis vectors land" as three
        multiply-adds, and cannot be transposed by a flag.
        MEASURED ANYWAY (verify_48 §C, shaders/matrix_probe.frag.hlsl): on this
        toolchain float3x3 WOULD have worked; DXC packs column-major and the
        matrix-typed reading agrees exactly. Still not what we ship. A default
        that happens to be right on the machine you tested is the most expensive
        kind of correctness there is.
  invisible-on-boxes: THE NAIVE NORMAL MATRIX IS INVISIBLE ON AXIS-ALIGNED
        GEOMETRY BY CONSTRUCTION, not merely usually. With linear part R*S and S
        diagonal, naive = R*S and correct = (R*S)^-T = R*S^-1. Feed an axis e_x:
        naive gives s_x*(R e_x), correct gives (1/s_x)*(R e_x) — PARALLEL, and the
        fragment's normalize() throws the length away. A box's model-space normals
        ARE its own axes, so the difference is exactly zero.
        Measured: 0 px change on two non-uniformly scaled boxes. Squash the
        ICOSAHEDRON to (1.7, 0.5, 1.0), whose twenty face normals are not axes, and
        2,160 px change by up to 143 codes. Worked example: n = (0.7071, 0.7071, 0)
        under S = diag(1.7, 0.5, 1.0) gives (0.9594, 0.2822, 0) naive and
        (0.2822, 0.9594, 0) correct — 57.2 degrees apart.
        THE GENERAL RULE IS ABOUT TESTING, NOT NORMALS: a test scene made of crates
        would have reported a clean pass while the renderer was broken. CHOOSE TEST
        GEOMETRY THAT IS ABLE TO FAIL. Same discipline as 3.1's cyclic planks and
        4.5's deliberately wrong vertex pitch.
  cpu-gpu-agreement: WHAT TWO RASTERIZERS CAN AND CANNOT AGREE ON, measured on the
        same scene, same camera, same light and literally the same mesh_data at
        320x180 (verify_48 §D–F).
        THE SHADING EQUATION: engine::shade() against scene.frag.hlsl over 4,096
        fragments x 3 specular models — worst disagreement 1.192e-07, which is
        ONE FLOAT ULP. Two implementations written three modules apart, in two
        languages, on two processors, agreeing to the last bit a float has.
        COVERAGE: 42 px only the CPU drew, 60 px only the GPU did — a one-pixel
        sliver on every silhouette. Same fill rule (centre-in, top-left tie-break)
        at different sub-pixel resolutions: we round corners to whole pixels, the
        hardware snaps to a fixed sub-pixel grid.
        COLOUR: of 2,729 shared pixels, 86.99% byte-identical, 7.88% one code,
        2.35% two, and 2.46% badly wrong — all of the last on silhouettes, where a
        large number is one pixel of coverage difference wearing a disguise.
        THE FLOOR IS ONE CODE, AND IT IS WORST IN THE DARKS. Our to_encoded()'s
        powf and the hardware's _SRGB write are two approximations of a CURVE, not
        two readings of a table. They agree exactly above linear 0.006; below it
        the curve's slope (12.92 near zero) makes a linear value cross a code
        boundary twelve times faster, and the 14/15 boundary lands at 0.004580 for
        us and between 0.0045148 and 0.0045186 for this hardware. A claim of
        bit-identity across this boundary would be a claim about the hardware.
        NEVER REPORT A PERCENTAGE WITHOUT A MAGNITUDE. The untextured floor reports
        100% of 39,202 pixels differing — all by one code, in one channel, because
        it is ONE flat colour that lands in the gap. That finding is worth nothing.
        AND A TEXTURE MAKES IT MUCH WORSE, honestly: the same quad textured is
        66.76% exact with 14.38% differing by >16 codes, because under minification
        the sample position decides the answer and neither renderer is wrong. The
        UNTEXTURED CONTROL (max one code) is what makes that a finding.
  program-modes: THE DEFAULT INVERTED, as 4.2 said it would. `engine` = 4.8's GPU
        scene; `engine --software` = the CPU renderer with its HUD and all five
        demos on [Tab]; `engine --probe` = 4.2–4.7's instrument; `engine --gpu` is
        an alias for --probe, kept because six lessons tell you to type it.
        THE SOFTWARE PATH IS THE REFERENCE AND IS NOT GOING AWAY. Every measured
        claim in Modules 2 and 3 was made against it; a port whose reference has
        been deleted is a port nobody can check. [V] runs both at once, split down
        the window.
        NO ON-SCREEN TEXT IN THE GPU PATH, and it is a real loss rather than an
        oversight. SDL_RenderDebugText needs an SDL_Renderer and 4.2 established a
        window is claimed by one or the other, never both. Text needs a font atlas
        and a shader (stb_truetype, Module 6). Numbers go to the log; Exercise
        4.8.5 is the way back, via an alpha-blended overlay that is also this
        engine's first post-processing pass.
  depth-attachment: DEPTH IS AN ATTACHMENT PLUS THREE PIPELINE FIELDS, and both are
        required. The pass takes SDL_GPUDepthStencilTargetInfo* as a SEPARATE
        parameter (may be NULL); the pipeline carries enable_depth_test,
        enable_depth_write and compare_op, which we have been zero-filling since
        4.4. 3.1's depth_buffer.hpp modelled exactly this split two modules early
        and said the port would be a rename. IT WAS.
        COMPAREOP_LESS, because SDL_GPU's NDC is 0 at near and 1 at far, so
        smaller is nearer — the same reason 3.1's software test was `<`.
        CLEAR TO 1.0, THE FAR VALUE. Clear to 0 and every fragment fails: a black
        screen with no error. Under reversed-Z it is 0 and the op is GREATER; the
        clear value and the compare op ALWAYS change together.
        STOREOP_DONT_CARE unless a later pass reads it. On tile-based hardware
        (every phone, Apple silicon) that skips writing the buffer back to memory.
        THE ATTACHMENT MUST MATCH THE COLOUR TARGET'S SIZE, AND THE WINDOW IS
        RESIZABLE. Recreate on change. Never reproduces on the machine where the
        code was written, because nobody resizes the window there.
  depth-precision: PRECISION FALLS OFF AS THE SQUARE OF DISTANCE, and this is the
        one genuinely new piece of theory in Module 4.
        z_ndc = (f/(f-n))(1 - n/d)   —   dz/dd = (f/(f-n)) * n/d^2
        SO 70% OF OUR RANGE IS SPENT IN THE FIRST METRE (n=0.3, f=100: z_ndc(1) =
        0.7021). The smallest separation N evenly spaced codes can resolve is
        Δd = (f-n)d^2/(f n N).
        MEASURED AND IT MATCHES TO A FEW PER CENT over two orders of magnitude.
        D16 worst case: 0.050 mm at 1 m (predicted 0.051), 4.8 at 10 (5.07), 31.1
        at 25 (31.69), 125.7 at 50 (126.8), 412.4 at 90 (410.8). D32_FLOAT
        ordinary: 0.008 / 0.036 / 0.206 / 0.967 / 2.2 mm.
        SO TWO WALLS 41 cm APART AT 90 m SHARE A D16 CODE. That is z-fighting with
        a number on it.
        FIXES IN ORDER OF VALUE: push the NEAR plane out (it is in the
        denominator, so 0.3 -> 1.0 is 3x everywhere and most scenes have nothing
        within a metre); reversed-Z with a float format; a wider format; moving
        the far plane in helps least.
  reversed-z: 180x ON A FLOAT FORMAT, NOTHING ON A UNORM ONE, and knowing which
        follows from knowing where a float keeps its precision — near ZERO. The
        ordinary mapping puts the FAR plane at z=1, the coarse end, which is
        exactly where 1/d^2 has already thrown resolution away: the two effects
        COMPOUND. Reversing puts far at 0 so they nearly cancel.
        THREE CHANGES: one subtraction in the vertex stage, COMPAREOP_GREATER,
        clear to 0. MEASURED: D32_FLOAT at 90 m goes 2.2 mm -> 0.012 mm (180x);
        D16_UNORM 412.4 -> 412.4, unchanged, because evenly spaced codes do not
        care which end is which. A large effect where theory predicts one and NO
        effect where it predicts none is what makes the measurement believable.
        NOT ADOPTED YET, deliberately: it touches the projection, the compare op,
        the clear, and every later pass that reads depth. Module 6, with the
        shadow maps in view. Exercise 4.7.4.
  depth-formats: ASK, DO NOT ASSUME. SDL guarantees exactly ONE depth format,
        D16_UNORM. MEASURED ON THIS MACHINE: D16 yes, D24_UNORM **NO**, D32_FLOAT
        yes, D24_UNORM_S8_UINT no, D32_FLOAT_S8_UINT yes. D24 is the format a
        desktop renderer would have hard-coded.
        engine::supported_depth_format takes a preference list and falls back to
        the guaranteed one. Same discipline 4.2 established for the swapchain
        format and present modes.
  samplers: A SAMPLER IS AN OBJECT, WHICH IS THE WHOLE DIFFERENCE FROM 3.9. There
        the filter and address mode were ARGUMENTS to sample(); here they are
        baked into SDL_GPUSampler. Fourth time this module has made the move —
        pipeline (4.4), vertex layout (4.5), uniforms (4.6), sampler (4.7) — and
        always for the same reason: a decision the hardware specialises for cannot
        be a parameter of the inner loop.
        TEXTURE AND SAMPLER ARE SEPARATE OBJECTS BOUND AS A PAIR
        (SDL_GPUTextureSamplerBinding, t0 with s0, space2). Better than OpenGL's
        fusion and better than ours: one image read three ways in a frame is three
        samplers, and one sampler serves every texture in a material system.
        THE PORT IS TWO CASTS, AND THERE IS A TEST BEHIND IT. 3.9 defined
        engine::filter and engine::address_mode to match SDL enumerator for
        enumerator; verify_42 §G has asserted it every run since; verify_47 §A
        adds the address modes. So static_cast is a rename with a regression test,
        not a coincidence being relied on.
        WHAT THE OBJECT HAS THAT THE CALL HAD NO ROOM FOR: min_filter and
        mag_filter SEPARATELY (the CPU path never minified), mipmap_mode + min/max
        lod + lod bias, THREE address modes for three axes, anisotropy, and
        enable_compare — which makes the sampler do the depth comparison itself
        and return filtered occlusion. That last one is how PCF shadows are nearly
        free (Module 6).
  textures-gpu: _SRGB IS ONE ENUM AND IT DECIDES WHETHER THE LIGHTING IS CORRECT.
        3.9's argument: an albedo is a reflectance, a reflectance multiplies a
        quantity of light, both sides must be linear. 3.9 measured the cost of
        skipping it (blending in encoded space gives 0.2139 where 0.5 is right —
        43% of the light). On the GPU the sampler decodes per read, FREE, and
        BEFORE the filter, which is the ordering software cannot easily achieve.
        MEASURED: file byte 222 comes back 186 under _SRGB; sRGB-decoding 222/255
        gives 0.7305 = 186.3. Exact.
        _SRGB FOR COLOURS, _UNORM FOR NUMBERS. Normal maps, roughness, masks are
        _UNORM — decoding them corrupts data that was never encoded. One of the
        commonest material-system mistakes.
        ORIENTATION IS NOW TESTED, NOT REASONED. assets/uv_grid.png carries a
        different colour in each corner; sampled through a _UNORM texture the four
        corners match the file BYTE FOR BYTE, so decoder + upload + sampler +
        readback all agree about which way v runs. 3.9's import-time uv flip
        stands.
        pixels_per_row IS PIXELS, NOT BYTES — third appearance of this bug in the
        course (1.5's framebuffer pitch, 4.5's vertex pitch, now the texture
        upload). Treat any field named for a row with suspicion.
        DEPTH TARGETS ASK FOR DEPTH_STENCIL_TARGET AND NOTHING ELSE. Adding
        SAMPLER would let a later pass read it (Module 6's shadow maps) and can
        force the driver into a layout that is slower for the usage you do have.
  third-party: THE TEST IS WHETHER THE HARD PART IS THE SUBJECT. We wrote
        parse_obj because OBJ's difficulty is exactly this course's subject — the
        mismatch between how a file describes a vertex and how hardware fetches
        one. We do NOT write a PNG decoder: baseline PNG is an afternoon, but PNG
        in the wild is DEFLATE + five filter modes + Adam7 + 16-bit + palettes +
        tRNS + colour profiles, and a decoder that handles only your test files
        fails on a USER's asset. There is nothing about engines in the fifth
        filter mode.
        stb_image PINNED AT A COMMIT SHA — no tags exist, same situation 4.3 hit
        with shadercross.
        A DEPENDENCY REACHES AS FAR AS ITS TYPES APPEAR IN HEADERS. stb's reach is
        src/gfx/image.cpp: STB_IMAGE_IMPLEMENTATION is defined there and nowhere
        else, image.hpp mentions no third-party type, and replacing it is one file.
        STBI_NO_STDIO so the decode takes bytes SDL_LoadFile already read — one
        notion of where files live, one error style.
        SUPPRESS WARNINGS AT THE BOUNDARY, NEVER BY EDITING THE DEPENDENCY. An
        edited dependency is one you can no longer update.
        ALWAYS ASK FOR 4 CHANNELS. Costs a byte per pixel on opaque images and
        means nothing downstream branches on what shape a file was.
        5.11 AMENDED "A DEPENDENCY REACHES AS FAR AS ITS TYPES APPEAR IN HEADERS":
        Dear ImGui is the FIRST one taken PUBLICLY, on purpose. The distinction is
        CONCEPT vs VOCABULARY — stb wraps to one function and one type; ImGui's
        value is four hundred widget calls, and a wrapper around those is a
        re-spelling with no content that must be re-spelt forever. So demos include
        <imgui.h> and engine/CMakeLists.txt links imgui PUBLIC, and the containment
        is a RULE ABOUT WHICH CODE MAY SPEAK IT (tooling only) rather than a link
        flag. See conventions:debug-ui. The rest of this block stands unchanged —
        stb is still PRIVATE and still reaches exactly one translation unit.
  uniform-data: THERE IS NO UNIFORM BUFFER OBJECT IN SDL_GPU. Look for
        SDL_GPU_BUFFERUSAGE_UNIFORM in SDL_gpu.h: it is not there. The six bits
        are VERTEX, INDEX, INDIRECT, GRAPHICS_STORAGE_READ, COMPUTE_STORAGE_READ,
        COMPUTE_STORAGE_WRITE. Instead you PUSH bytes onto the COMMAND BUFFER —
        SDL_PushGPU{Vertex,Fragment}UniformData(cb, slot, data, bytes) — and every
        draw recorded after that point reads them.
        CONSEQUENCES, ALL SIMPLIFICATIONS: no lifetime (no create/release, no
        move-only wrapper — the only GPU thing in this engine that needed none);
        no cycling hazard (the bytes are copied at the call, into a command buffer
        that is not executing); ordering is the only rule. MEASURED: push 201,
        draw, push 77, draw — the second draw reads 77, nothing rebound.
        THE THIRD RATE. Per-vertex is 8,575 writes a frame here, per-instance 7,
        per-frame 1. The first two are FETCHES (the hardware indexes an array with
        a counter it already keeps); the third is not an array at all, which is
        why it is not a buffer.
  uniform-packing: THE RULE IS HLSL'S, NOT std140 — AND SDL'S HEADER SAYS std140.
        SDL: "The data being pushed must respect std140 layout conventions... vec3
        and vec4 fields are 16-byte aligned." What our toolchain actually emits is
        HLSL constant-buffer packing: fields in order, packed tightly, except that
        A VECTOR MAY NOT STRADDLE A 16-BYTE REGISTER BOUNDARY — if it would, it
        moves to the next one. A scalar is never moved.
        MEASURED, from the Offset decorations in our own compiled SPIR-V:
        float4x4 at 0, float at 64, float3 at 68 (std140 would say 80), float at
        80, float2 at 84, float3 at 96 (92 would straddle).
        FOLLOW SDL'S ADVICE ANYWAY: std140 is a SUPERSET, so a layout satisfying
        it also satisfies HLSL packing, and the question of which rule applies
        stops mattering — including under a GLSL front end later.
        THE HABIT: PAIR EVERY float3 WITH A float. The two fill a register
        exactly, so nothing can straddle and both rules agree. light_uniforms is
        float3/float/float3/float = 32 bytes, two registers, no padding.
        THE FAILURE CORRUPTS THE TAIL. A naive C++ struct put the last float3 at
        92; the shader reads 96; it arrived as (242, 243, 0) where (241, 242, 243)
        was written — shifted one float, zero on the end, EVERY EARLIER FIELD
        FINE. Note this is the mirror image of 4.5's pitch bug, where vertex 0 was
        always right and it got worse further in. Both look fine at the start.
        packed_offset() in gpu_uniform.hpp IS the rule, constexpr, and every block
        static_asserts its offsets against it — a rule you can execute cannot
        drift from the code it describes, and a comment cannot fail.
  matrix-upload: THE MATRIX CROSSES UNTOUCHED, AND 2.6's CLAIM IS NOW CHECKED.
        mat4 stores four columns contiguously; pushed as a cbuffer float4x4, the
        element we wrote at (row, col) arrives as m[row][col] — ALL SIXTEEN
        verified individually by a probe shader that reports one element per
        pixel. memcpy is the entire conversion, no transpose anywhere.
        DO NOT BELIEVE THE INTERMEDIATE. The compiled SPIR-V says
        "OpMemberDecorate %Camera 0 RowMajor", which looks exactly like the
        transpose that is demonstrably not happening. It is an artefact of how DXC
        maps HLSL packing onto SPIR-V's naming. AN INTERMEDIATE REPRESENTATION IS
        ALLOWED TO DESCRIBE YOUR DATA IN ITS OWN VOCABULARY — measure the endpoint.
        `mul(M, v)` is column-vector convention (2.5), and float4(world, 1.0f):
        w = 0 there makes it a DIRECTION, so the fourth column — the camera's
        translation — is multiplied by zero and the scene spins about a point the
        camera never leaves. Distinctive enough to diagnose by sight.
        projection * view, IN THAT ORDER, because A*B applies B first.
  uniform-space: A WRONG REGISTER SPACE IS CAUGHT BY THE BUILD, NOT THE
        REFLECTION — which REVISES 4.3's guess that it would be silent. Asked of
        each tool with the fragment cbuffer moved to space0:
          glslc HLSL -> SPIR-V        accepted, does not care
          the JSON reflection         BYTE-IDENTICAL to the correct shader
          spirv-dis | grep DescriptorSet   0 instead of 3 — visible
          shadercross SPIR-V -> MSL   REFUSED: "Descriptor set index for graphics
                                      uniform buffer must be 1 or 3!"
        So 4.3's "compile offline" argument pays off from a new direction: it
        moved a would-be black screen into a build error on your own machine.
        THE REFLECTION CANNOT SEE IT, so 4.5's cross-check is no help here.
        ⚠ VERIFY: a Vulkan build consumes SPIR-V directly, so nothing translates
        and nothing refuses. Untested — no Vulkan device on this machine.
        verify_46 §D reads the DescriptorSet decorations out of the .spv IN THE
        HARNESS (a five-word header, then (wordcount<<16)|opcode; OpDecorate is
        71, the DescriptorSet decoration is 34) — turning 4.3's advice from
        something a person must remember into something that runs.
  push-cost: A PUSH IS A COPY, SO IT IS SMALL DATA. Best of seven runs of 256
        pushes: 108 bytes 0.015 us (7.1 GB/s), 4 KB 0.058 us (70.3), 16 KB 0.259
        us (63.4). Read as ~14 ns of call overhead plus a memcpy at ~65 GB/s,
        which is what "copied into the command buffer" predicts.
        THE FIRST VERSION OF THIS MEASUREMENT WAS WRONG AND SAID SO: scaled rep
        counts, one run each, and 16 KB came out at 43 GB/s against 4 KB at 3.8 —
        an 11x difference in the throughput of a memcpy, which cannot be true.
        Fixed with a fixed rep count and best-of-seven (the minimum, because every
        source of error adds time). 4.4's rule caught it: check the number CAN be
        true.
        AND THERE IS AN UNDOCUMENTED CEILING THAT CRASHES SILENTLY. Repeating a
        LARGE push into one command buffer kills the process with NO message — no
        SDL error, no validation output. Measured in an isolated program: 16,000
        pushes of 4 KB (62 MB) fine; 64 KB pushes die between 24 and 32 of them,
        AND NOT AT THE SAME COUNT TWICE. Non-determinism at a resource boundary is
        the signature of a pool being exhausted rather than a limit enforced. KEPT
        OUT OF THE HARNESS per 4.4's rule: a test that destabilises the process is
        not a test.
        FOR BULK DATA USE A STORAGE BUFFER: GRAPHICS_STORAGE_READ, declared as a
        StructuredBuffer in space0/space2, BOUND rather than pushed, so the bytes
        move once instead of once per draw. Module 6.
  uniform-probe: READING A UNIFORM BACK IS NOT A THING AN API OFFERS, so ask the
        shader and let it answer in the only currency it has — the colour of a
        pixel. shaders/uniform_probe.{vert,frag}.hlsl: pixel (x, y) reports one
        field, encoded value/255 into a _UNORM target so a value of n returns as
        the byte n.
        SV_Position IS THE PIXEL CENTRE, so the first pixel is (0.5, 0.5):
        TRUNCATE, do not round. Rounding shifts the whole probe by one and
        produces a table that looks plausible and is wrong in every entry.
        THE PROBE HAS NO VERTEX BUFFER — SV_VertexID generates the three corners
        (-1,-1), (3,-1), (-1,3), a triangle twice the target's size in each
        direction. Partly convenience, mostly hygiene: an instrument with a vertex
        layout might be measuring one by accident (4.5).
        ONE triangle, not two: no shared edge means no seam and no pixel
        rasterised twice, which on a probe would mean a value written twice.
  vertex-fetch: A LAYOUT IS THREE NUMBERS AND ONE FORMULA — address = base +
        i*pitch + offset — evaluated by fixed-function hardware with no way to
        know whether the numbers are right. Any pitch produces addresses, any
        addresses produce bytes, any bytes produce floats. THE PICTURE IS THE
        ONLY PLACE A MISTAKE BECOMES VISIBLE.
        SIZEOF FOR THE PITCH, OFFSETOF FOR EVERY OFFSET, NEVER A LITERAL.
        gpu_mesh::describe and main.cpp's describe_instances are the only two
        places in the engine that name a layout.
        A WRONG PITCH SHATTERS, A WRONG OFFSET DEFORMS, and the difference is
        diagnostic. Pitch error is i*(error), so it ACCUMULATES — 4 bytes at
        vertex 1, 4,896 by vertex 1,224, and EXACTLY ZERO AT VERTEX 0, which is
        how the bug survives a three-vertex test. Measured: pitch 32 -> 3,696 px
        in an 88x52 box; 28 -> 5,076 px in 88x84; 36 -> 5,027 px in 88x84 —
        coverage goes UP, and too-long and too-short look the SAME, so do not
        read the direction out of the picture. A wrong OFFSET reads the wrong
        bytes of the RIGHT vertex: position pointed at the normal collapses the
        mesh onto a unit sphere (3,126 px, 62x64).
  vertex-format: A FORMAT ANSWERS TWO QUESTIONS AND THE ANSWERS DIFFER.
        engine::size_of is bytes IN THE BUFFER; engine::shader_type_of is the
        type IN THE SHADER. UBYTE4_NORM is 4 bytes and a float4 — the divide by
        255 is done by the fetch unit, free. Also SHORT2_NORM -> float2 (/32767),
        HALF4 -> float4. The plain integer forms (UBYTE4, SHORT4) do NOT convert.
        CASHED IT: 4.4's gpu_vertex went 28 -> 16 bytes (-43%) with
        triangle.vert.hlsl UNCHANGED. Same 47,124 px covered; 22,494 of them
        differ by AT MOST 1 code of 255 — below the precision of an 8-bit target.
        UBYTE4 vs UBYTE4_NORM is SIX CHARACTERS and the same four bytes: the
        first delivers 0..255 into a float (white), the second 0..1.
  layout-check: THE REFLECTION JSON IS THE NOTARY, AND IT CATCHES 3 OF 5.
        shadercross has emitted an `inputs` array (name/type/location) since 4.3
        and we read only the four counts. parse_shader_inputs reads the rest;
        pipeline_desc::check_layout compares. SETTLES EXERCISE 4.4.4.
        BOUND THE SCAN TO THE ARRAY — the same file has an `outputs` array with
        byte-identical keys, so an unbounded scan for "location" walks out of one
        and into the other. Absent `inputs` is an ANSWER (true); malformed is an
        ERROR (false), because half a description would let the check give a
        clean bill of health to a contract it never read.
        CATCHES: a too-short pitch (attribute end > pitch), a location nothing
        declares, a location nothing supplies, duplicate locations, a base-type
        mismatch. CANNOT CATCH: a too-long pitch, swapped offsets. Saying so is
        what makes it worth reading — a check that implies total coverage is
        worse than none.
        AND SDL CATCHES ONE OF SIX. Measured: pitch 4 short -> CREATED, 4 long ->
        CREATED, offsets swapped -> CREATED, an attribute the shader never
        declares -> CREATED. Only "a shader input nothing supplies" is REFUSED,
        with an excellent message ("Vertex attribute input_uv(2) is missing from
        the vertex descriptor") — and it is the one that would have been obvious
        anyway. Same temperament as 4.3's shader creation: names checked,
        numbers not.
        WIDENING IS A NOTE, NOT A PROBLEM. FLOAT3 into a float4 is legal and the
        hardware fills the rest. MEASURED ON METAL: w = 1 — the FLOAT3 draw
        covered 9,944 px, EXACTLY equal to a FLOAT4 draw at scale 1.0 (the
        missing component IS the scale, so coverage reads the answer off the
        screen). Agrees with Vulkan/D3D12's (0,0,0,1). ⚠ VERIFY elsewhere.
  interleave: INTERLEAVED BY DEFAULT, HYBRID IN PRODUCTION, AND BOTH NUMBERS ARE
        MEASURED on our 1,225-vertex torus with 64-byte lines. Fetching ONE
        vertex: 1 line interleaved (32 B is half a line exactly, so it never
        straddles), up to 5 separate. Sweeping every POSITION: 613 lines
        interleaved against 230 separate — 2.7x THE OTHER WAY, because an
        interleaved line carries 12 useful bytes in 32.
        SO IT IS A TRADE, NOT A RULE. The production layout is position in its
        own buffer and everything else interleaved, which makes a depth-only pass
        fast without slowing the main pass. NOT BUILT — there is no depth pass to
        justify it until Module 6 — and it costs no new concepts when it comes,
        because a "separate" layout is just more vertex_buffer slots.
        PAYS OFF 3.2's PROMISE: mesh.hpp said parallel arrays were right for a
        CPU loop and that Module 4 would revisit with the diagram the decision
        deserves. gpu_mesh::interleave is the revisit; it converts once, at load.
  index-buffers: 16-BIT, AND THAT IS WHERE k_max_mesh_vertices COMES FROM.
        SDL_GPUIndexElementSize has exactly two values; 16 bits names 65,536,
        which mesh.hpp has declared since 3.5 with a justification and which
        gpu_mesh::create is the first line to DEPEND on.
        ON OUR TORUS: 1,225 vertices + 6,912 indices = 53,024 bytes against 6,912
        expanded vertices = 221,184. 4.17x. The index buffer COSTS 13,824 and
        SAVES 181,984, because an index is 2 bytes and a vertex is 32.
        THE PICTURES ARE IDENTICAL — 0 differing pixels, max channel delta 0,
        proven by drawing both through one pipeline into one target. A test that
        can only ever return zero is worth writing: a nonzero result has exactly
        one explanation.
        INVOCATIONS ARE A RANGE, NOT A FIGURE: indexed 1,225..6,912 depending on
        the post-transform cache; expanded 6,912 exactly, no reuse possible. This
        is what vertex-cache optimisation (Forsyth) optimises. 4.9's RenderDoc
        capture is where the real number appears.
        1,152 POSITIONS IN THE FILE, 1,225 VERTICES AFTER LOADING — the uv seam
        splits every vertex where u wraps 1 -> 0. 3.5 §3's argument, on the mesh
        that was built to contain it.
        THE ELEMENT SIZE IS PASSED AT BIND TIME, not baked into the pipeline and
        not carried by the buffer. 16-bit indices bound as _32BIT read pairs as
        single enormous values: glass shards, no error.
  instancing: ONE ENUM VALUE. input_rate = _INSTANCE instead of _VERTEX; same
        buffer type, same usage bit, same SDL_BindGPUVertexBuffers, same
        attributes at ordinary locations. THE SHADER CANNOT TELL —
        mesh.vert.hlsl declares six inputs in one struct and nothing marks 3/4/5
        as per-instance. Every tool built for layouts (formats, check_layout, the
        reflection) therefore works on instance data unchanged.
        LOCATIONS ARE NUMBERED ACROSS THE PIPELINE, NOT PER BUFFER. Slot 1's are
        3, 4, 5 because slot 0 used 0, 1, 2.
        instance_step_rate IS RESERVED AND MUST BE 0 — not exposed by
        instance_buffer(), because a setter for a field with one legal value is
        an invitation to a bug.
        THE DEFAULT IS THE TRAP AGAIN: every enum in the description has its
        first enumerator at 0, so a zero-initialised description means _VERTEX.
        Identical in shape to 4.4's cull_mode finding.
        THE NUMBER: 28 bytes/instance x 7 = 196 bytes rewritten per frame against
        53,024 bytes of geometry uploaded once — 0.370%. That ratio IS the
        argument for instancing.
        A REAL ENGINE SENDS A MATRIX HERE. We send a cos/sin pair for one axis,
        because a matrix has to come from a uniform buffer and that is 4.6.
  stream-buffers: TWO KINDS OF BUFFER, AND THE DIFFERENCE IS HOW OFTEN THEY ARE
        WRITTEN, NOT WHAT THEY HOLD. gpu_buffer: written once at load, staging
        created and released per upload, cycle = FALSE (no reader to race, and
        cycling would allocate a second copy of 39 KB of torus).
        gpu_stream_buffer NEW: written every frame, staging KEPT for the object's
        lifetime, cycle = TRUE on BOTH the map and the upload.
        GETTING IT BACKWARDS COSTS MEMORY IN ONE DIRECTION AND CORRECTNESS IN THE
        OTHER — and the correctness bug appears only when the GPU falls behind,
        i.e. on somebody else's slower machine. Same hazard gpu_present_target
        faced in 4.2, now on the geometry side.
  gpu-viewport: SDL_SetGPUViewport IS RENDER-PASS STATE, NOT PIPELINE STATE. It
        survives a pipeline change, so a second draw in the same pass inherits
        it — put it back if that draw wants the whole target. Used because
        mesh.vert.hlsl bakes 16:9 into its projection and the window is
        resizable: the mesh pass is restricted to the same letterboxed rect the
        blit uses. This is 2.11's viewport transform handed to hardware as a
        struct, min_depth/max_depth 0..1 per conventions §4.
  no-uniforms-yet: mesh.vert.hlsl's camera is a pile of `static const` floats and
        the projection is LESSON 2.10's MATRIX WRITTEN OUT AS ARITHMETIC, line
        for line: clip.x = (t/a)*view.x, clip.y = t*view.y, clip.z =
        R*(view.z + n), clip.w = -view.z, with t = 1/tan(fovY/2) and R = f/(n-f).
        Nothing is simplified away; only the DELIVERY is deferred to 4.6.
        Consequences to undo next lesson: the camera cannot move, the
        per-instance rotation is a cos/sin pair rather than a matrix, and the
        aspect ratio is a constant that the viewport has to make true.
        SETTLED IN 4.6: all seven constants and the eleven lines of arithmetic are
        gone, replaced by one cbuffer and one mul(). The viewport call stayed but
        its JOB CHANGED — from making a compile-time aspect ratio true to sharing
        a rectangle with the blitted software picture. A workaround that becomes a
        decision is a sign the design moved the right way.
  pipelines: EVERY PIECE OF RENDER STATE, IN ONE IMMUTABLE OBJECT — 9 top-level
        fields, 53 expanded, which is exactly the count 4.1 predicted from
        fill_style. It IS fill_style's argument list, hoisted out of the call and
        frozen; that is why 3.8's per-triangle material rebind is a thing a GPU
        cannot do.
        SET THE RASTERIZER STATE EXPLICITLY. Every enum in it has its first
        enumerator at 0, so a zero-initialised state means "CCW front, cull
        nothing" — a forgotten cull_mode is NOT an error, it is no culling.
        Course default: FILLMODE_FILL, CULLMODE_BACK, FRONTFACE_CCW.
        THE COLOUR TARGET FORMAT IS NOT A CHOICE — it must equal the format of the
        texture rendered into. pipeline_desc reads it from the device's report
        (4.2 logged it for this), and colour_target_format() overrides for
        offscreen targets, which Module 6 will live on.
        CREATION VALIDATES ALMOST NOTHING. Measured: a colour format the target
        does not have -> CREATED (and the frame drew); an attribute the shader
        never declared -> CREATED; only "no vertex layout while the shader has
        inputs" is REFUSED, with an excellent message. A SHADER IN THE WRONG SLOT
        IS WORSE THAN AN ERROR: vertex-in-fragment is refused but leaves Metal's
        compiler service broken so the NEXT creation crashes, and
        fragment-in-vertex SEGFAULTS immediately. Both reproduced 3x in an
        isolated one-trial program. The type system cannot help — both are
        SDL_GPUShader*.
  pipeline-compile: 4.3's PREDICTION IS CONFIRMED — the compile is at PIPELINE
        creation, not shader creation. SDL_CreateGPUShader is ~0.031 ms in every
        run and configuration; pipeline creation is never that cheap.
        HOW MUCH MORE DEPENDS ON THE DRIVER'S CACHE, WHICH IS ON DISK AND OUTLIVES
        THE PROCESS. Measured on one machine: ~32 ms for the FIRST pipeline in a
        process (one-time driver/compiler setup), ~2.4 ms per NEW state
        permutation (the compile, ~80x a shader), 0.01-0.6 ms for one compiled
        before — including in a previous RUN.
        TWO WRONG CONCLUSIONS WERE DRAWN FROM THIS CALL BEFORE IT WAS MEASURED
        PROPERLY, and both are kept in the lesson: (1) timing one pipeline then
        the same description again and calling the difference the compile;
        (2) comparing a release run with a debug run — 0.5 vs 33.5 ms — and
        calling the difference the VALIDATION LAYER, at 66x. Both were the disk
        cache. Measured with a run-unique blend permutation in both configs,
        validation costs almost nothing: 31.8 vs 34.5 ms for a first pipeline.
        WHAT CAUGHT IT: running the engine again the next day and seeing 0.077 ms
        where the log had said 42. A number that moves 500x between runs of an
        unchanged binary is a property of what happened before the call.
        CONSEQUENCE FOR ENGINES: build every pipeline at load time (2.4 ms is 15%
        of a 60 Hz frame), and remember that STATE PERMUTATIONS ARE COMPILATIONS —
        a material system with five booleans is 32 pipelines and ~80 ms of
        startup. This is what "shader compilation stutter" means in patch notes.
  create-info-lifetime: SDL_GPUGraphicsPipelineCreateInfo HOLDS POINTERS — the
        vertex buffer descriptions, the attributes, the colour target
        descriptions. A function that fills one in and RETURNS IT returns a struct
        pointing at its own dead stack frame, and the compiler says nothing. Hence
        pipeline_desc is a CLASS that owns the arrays, and info() returns a CONST
        REFERENCE. Same shape as a dangling string_view or span; same fix, which
        is to give the view and the viewed one lifetime (3.5's mesh_data/mesh).
  vertex-layout: THE VERTEX IS DECLARED TWICE, IN TWO LANGUAGES, AND NOTHING
        CHECKS THEY AGREE. HLSL says `float3 position : TEXCOORD0; float4 colour :
        TEXCOORD1`; C++ says pitch 28, FLOAT3 at 0, FLOAT4 at 12. Use sizeof and
        offsetof, never literals. A wrong pitch walks the buffer at the wrong rate
        and draws a smear; a wrong offset feeds colour into position. 4.3's
        reflection JSON already holds the shader's half (`inputs`), and checking
        one against the other is exercise 4.4.4 and the first piece of a material
        system.
        SUPERSEDED IN SCOPE BY 4.5: the exercise was done. See `vertex-fetch`,
        `vertex-format` and `layout-check` above, which measure what this entry
        only asserted.
  draw-calls: A DRAW CALL SAYS ALMOST NOTHING — SDL_DrawGPUPrimitives(pass, 3, 1,
        0, 0) is "three vertices, one instance, from the start". No geometry, no
        shader, no target: all of it was BOUND beforehand. Even the MEANING of
        three vertices is pipeline state — measured from one buffer:
        TRIANGLELIST 20,808 px, LINESTRIP 410, POINTLIST 0 (the last one flagged
        ⚠ VERIFY: probably Metal needing a written point size; measurement real,
        explanation a hypothesis).
        BIND ORDER: pipeline, then vertex buffers, then draw. The pipeline
        describes what a vertex IS.
  gpu-vs-ours: THE TWO RASTERIZERS COMPUTE THE SAME TRIANGLE. Identical geometry
        through engine::fill_triangle and through the hardware, compared per
        pixel: both 20,808; GPU-only 0; OURS-ONLY 102 (0.49%); DISAGREEMENTS
        STRICTLY INSIDE THE TRIANGLE: **ZERO**. Our coverage is a strict superset,
        and every extra pixel is on one edge, one per row.
        THE DIFFERENCE IS A FILL RULE, not a bug: hardware uses top-left, our 2.2
        edge test uses >= 0 and includes every boundary pixel. On a lone triangle
        that is 102 px; on two triangles sharing an edge it is a seam or a double
        draw. Sub-pixel precision differs too (our vertices are integers; hardware
        rasterizes at ~1/256 px), so exercise 4.4.2 cannot reach zero.
  shaders: ONE SOURCE, THREE BINARIES, AND SPIR-V IN THE MIDDLE. HLSL is the
        course's shader language; SDL_shadercross is the sanctioned tool. The
        pipeline has exactly TWO stages and only the first is a compiler:
          HLSL --DXC (or glslc)--> SPIR-V --SPIRV-Cross--> MSL / DXIL / JSON
        EVERYTHING RIGHT OF SPIR-V IS A TRANSLATION FROM IT, so a missing front
        end costs the WHOLE toolchain, not one backend. Measured, one shader:
        HLSL->SPIR-V 12.3 ms, SPIR-V->MSL 1.5, SPIR-V->JSON 1.5; four shaders,
        all hops, 61.2 ms.
        OFFLINE, NOT AT RUNTIME — and not for speed. 61 ms at every launch would
        be invisible. The reasons are that runtime translation ships the compiler
        (SPIRV-Cross alone is 6.7 MB here, DXC is an LLVM fork) and moves a
        failure that would happen on YOUR machine to the user's.
  shadercross-version: header says 3.0.0, package metadata says 3.0.0, and
        UPSTREAM HAS NO TAGS AND NO RELEASES (checked 2026-08-15). So the
        "pin an exact tag, never a branch" rule CMakeLists.txt applies to SDL
        CANNOT be applied here — pin a COMMIT SHA. (This supersedes the earlier
        "3.0.0-preview" note in LEARNINGS.md, which was imprecise.)
        A BUILD WITHOUT DXC CANNOT READ HLSL AT ALL — not "cannot emit DXIL".
        The version number does not reveal this; only running the tool does,
        which is why cmake/Shaders.cmake PROBES at configure time. Clone with
        --recursive or you get exactly that half-equipped build.
  shader-spaces: THE REGISTER SPACE IS FIXED BY SDL, PER STAGE, and is not a
        choice. vertex: t/s in space0, cbuffer in space1. fragment: t/s in
        space2, cbuffer in space3. Verified on our own output with
        `spirv-dis x.spv | grep DescriptorSet` — space1 -> set 1, space2 -> set 2,
        space3 -> set 3, exactly as SDL_gpu.h's SPIR-V rules require.
        A WRONG SPACE IS SILENT: valid HLSL, compiles, translates, loads, runs,
        and reads whatever is bound at the slot named. Check with the
        disassembler when you write the declaration, not from the picture later.
  shader-semantics: EVERY non-system-value semantic is TEXCOORDn, numbered from
        0, no gaps — SDL_gpu.h says it assumes this. SV_Position (clip space, the
        vec4 from 2.10) and SV_Target0 (the pass's first colour target) are
        system values and mean something; the TEXCOORD names do not.
  shader-counts: SDL_GPUShaderCreateInfo WANTS FOUR COUNTS AND VALIDATES NONE OF
        THEM. Measured on textured.frag (really 1 sampler, 1 uniform buffer):
        correct -> created, too low -> created, too high -> created, 99 of each
        -> CREATED. SDL's own FAQ names wrong counts as the commonest cause of a
        shader that does not work, and nothing in the creation path catches it.
        THEREFORE NEVER TYPE THEM. shadercross emits a JSON reflection with
        exactly those four keys; read it, and REFUSE TO LOAD if it is missing
        rather than defaulting to zero — a default of zero silently builds the
        exact bug the file exists to prevent.
  shader-entrypoint: THE ENTRY POINT IS A PROPERTY OF THE FORMAT, NOT THE SHADER.
        Our HLSL declares `main`; SPIRV-Cross renames it to `main0` in MSL
        because `main` is reserved there. SPIR-V keeps `main`. Measured:
        "main0" -> created, "main" -> REFUSED, nonsense -> REFUSED. This one SDL
        does check, at creation, with a message ("Creating MTLFunction failed").
        Note the temperament: it checks the NAME and not the NUMBERS.
  shader-compile-timing: SDL_CreateGPUShader IS NOT WHERE THE COMPILE HAPPENS.
        Measured 0.008 ms per shader COLD (identical to the ninth run, so not a
        cache); all four in 0.028 ms against 61.2 ms of build-time compilation.
        Eight microseconds cannot be MSL -> machine code.
        PREDICTION FOR 4.4, WRITTEN DOWN BEFORE IT IS CHECKED: the compile happens
        at PIPELINE creation, because only there does the driver know the target
        formats, depth/blend state and vertex layout the code must be specialised
        to. That is 4.1's measured reason pipeline objects exist and what
        SDL_gpu.h means by "precalculated rendering state".
  build-probes: WHEN A TOOL'S BEHAVIOUR DEPENDS ON HOW IT WAS BUILT, RUN IT.
        cmake/Shaders.cmake runs shadercross once at configure time on a real
        shader and reads the exit code, then PRINTS which of three routes it got.
        A version number cannot answer the question and a silent fallback is
        discovered months later on a build machine.
        add_custom_command(OUTPUT) DECLARES HOW TO MAKE A FILE; it does not build
        one. With no target depending on the output, the rule never runs and the
        build SUCCEEDS with an empty directory. Wrap outputs in a custom target
        and add_dependencies() it — and note CMake target names cannot contain a
        dot, so `triangle.vert` needs string(REPLACE "." "_" ...).
  gpu-async: A CALL RECORDS WORK; IT DOES NOT PERFORM IT. This is the whole of
        Module 4's difficulty and every new object exists to manage it. MEASURED:
        one command buffer of 48 blits of 2048^2 costs the CPU 0.1879 ms to
        acquire + record + submit, and the work takes 4.7815 ms — 25.45x. One
        SDL_BlitGPUTexture is RECORDED in 0.0038 ms.
        THE FOUR NEW OBJECTS ARE ONE IDEA. device = the connection to another
        processor; command buffer = a message to it; transfer buffer = shared
        ground because its memory is not ours; fence = how we learn it finished.
        A software rasterizer needed none of them because it was one machine.
        FIVE OF THE NINE ARE RENAMES: framebuffer->swapchain texture,
        texture+sampler->SDL_GPUTexture+SDL_GPUSampler, depth_buffer->depth-stencil
        target, fill_style->SDL_GPUGraphicsPipeline, the fill loop->render pass +
        draw. Checked, not asserted: verify_42 §G tests enumerator parity for
        engine::filter, engine::address_mode and engine::cull_mode against SDL's.
        KEEP THAT TEST — if SDL inserts an enumerator it must fail there, with a
        line number, not later as a wrong picture.
  gpu-fence: A FENCE IS ONE BIT FOR ONE SUBMISSION: not yet, or done. It does not
        say how far along, does not order anything, and does not apply to the
        device. READ BEFORE THE FENCE AND THE DATA IS WRONG — measured 64/64 with
        the transfer buffer poisoned to 0xAB first, 0/64 after the wait.
        THE COST OF A SYNC IS THE OVERLAP YOU GAVE UP, NOT THE DURATION OF THE
        WAIT. Two regimes, both measured:
          GPU-BOUND: 32 submissions waiting on every fence 9.438 ms, waiting only
          on the last 5.904 ms -> 1.60x. The pipelined figure equals the GPU work
          per submission exactly (0.184 ms), i.e. the GPU never idles.
          DISPLAY-BOUND: a full sync every frame costs NOTHING MEASURABLE. Fence
          0.761 ms, acquire falls 16.002 -> 15.272, frame unchanged at 16.667 ms.
        THAT SECOND RESULT IS THE DANGEROUS ONE — it is why the bug ships.
  gpu-cycle: `cycle = true` MEANS "IF THIS IS BUSY, GIVE ME A FRESH ONE". Resources
        are ring buffers wearing the disguise of one object. Pass false on a
        per-frame write and the corruption appears ONLY when the GPU falls behind:
        a flickering band of the previous frame, on someone else's machine.
        NEVER cycle a resource you intend to update only partially — cycling makes
        the WHOLE resource undefined.
  gpu-clear: THE CLEAR COLOUR IS ALWAYS LINEAR LIGHT; THE FORMAT DECIDES WHAT IS
        STORED. Measured by clearing a 1x1 target and downloading the byte:
          _UNORM      : byte = round(255*v) EXACTLY, at all five test values.
          _UNORM_SRGB : byte = the sRGB encode, matching OUR engine::linear_to_srgb_u8
                        to the code. 0.5 -> 128 and 0.5 -> 188 respectively.
        Out of range is CLAMPED, not wrapped (-0.5 -> 0, 1.5 -> 255). The encode
        applies to RGB ONLY — measured: clear (.5,.5,.5,.5) into _UNORM_SRGB gives
        R=188, A=128. Alpha is coverage, not colour.
        NEVER PRE-ENCODE A CLEAR COLOUR. Double encode = washed-out darks (128 ->
        188 -> 226), which is Module 6's artifact arriving early.
  gpu-present: A SWAPCHAIN TEXTURE IS BORROWED, NOT OWNED. Ask each frame, give it
        back by submitting; write-only, cannot be sampled or read back; may be
        NULL (minimised) and that is not an error. THE ACQUIRE IS WHERE A VSYNCED
        FRAME WAITS — 16.002 of 16.667 ms measured. Not the submit; SDL_GPU has no
        present call at all, presentation is implied by having acquired.
        A WINDOW IS POINTS, A SWAPCHAIN IS PIXELS. They agree only without
        SDL_WINDOW_HIGH_PIXEL_DENSITY. Use the sizes the acquire fills in.
  gpu-memory: A TEXTURE CANNOT BE memcpy'd INTO — it is tiled (2x2 quads adjacent),
        possibly swizzled, possibly compressed, and the scheme is vendor-specific.
        Hence the transfer buffer: a plainly-laid-out region both processors can
        see. THREE COPIES per frame, in TWO time frames: memcpy (now, CPU,
        0.0028 ms at 320x180), upload (recorded), blit (recorded).
        SDL_GPUTextureTransferInfo::pixels_per_row IS PIXELS, NOT A PITCH. Give it
        width*4 and the image shears — Lesson 1.5's bug, four modules later.
        SDL_PIXELFORMAT_ARGB8888 -> B8G8R8A8_UNORM (enum 12), ASKED via
        SDL_GetGPUTextureFormatFromPixelFormat because the answer is
        endianness-dependent. Round trip up-and-back is BIT-IDENTICAL.
  gpu-flight: throughput = 1/max(C,G) for F>=2; latency ~ F*max(C,G). GOING FROM 1
        TO 2 BUYS THROUGHPUT; 2 TO 3 BUYS ONLY LATENCY, which is why SDL's default
        is 2. SDL_SetGPUAllowedFramesInFlight "will stall and flush the command
        queue" — once, at a settings change, never per frame.
  measurement (extends 3.10 §7e): CHECK THAT THE NUMBER CAN BE TRUE. The first
        bandwidth figure in 4.2 was 763 GB/s on a machine whose bus is 273. TWO
        causes, found in order: (1) ELISION — blitting src->dst N times is N copies
        of one answer; fixed by ping-ponging so blit i+1 depends on blit i.
        (2) LOSSLESS RENDER-TARGET COMPRESSION — both textures were cleared FLAT,
        which compresses to nearly nothing, so the copy was real and the bytes were
        not; fixed with xorshift noise. Flat vs noise: 1.04x at 512^2 rising to
        2.87x at 4096^2, and noise converges on 277.6 GB/s against a published 273.
        A MEASUREMENT THAT LANDS ON AN INDEPENDENTLY KNOWN NUMBER IS THE STRONGEST
        EVIDENCE A BENCHMARK CAN OFFER.
        NEW AXIS FOR THE SAME OLD RULE: the same binary measured 120 fps and then
        60 fps minutes apart because the window opened on a DIFFERENT DISPLAY, and
        produced a briefly exciting false conclusion that fencing halves the frame
        rate. Table 3's numbers were retaken as three binaries run ALTERNATELY,
        twice.
  world: right-handed, Y-up, -Z forward
  clip: left-handed, +Y up, z in [0,1] (SDL_GPU-fixed; projection absorbs the flip)
  sw-rasterizer: targets SDL_GPU's exact NDC (Module 4 port = API change, not maths change)
  matrices: column vectors, v' = M*v, COLUMN-MAJOR storage.
        A matrix IS the set of answers to "where do the basis vectors land":
        c0 = image of (1,0,..), c1 = image of (0,1,..), and so on. Everything else
        is a consequence, not a rule to memorise.
        mat2 / mat3 / mat4 ALL HAVE THE SAME SHAPE: N columns of vecN, and
        identity() as a STATIC MEMBER on each (a free identity() became impossible
        the moment mat3 existed — no arguments, so it could differ only by return
        type, which C++ cannot overload on).
        WRITTEN IN ROWS, STORED IN COLUMNS. The first two floats in memory are
        the LEFT COLUMN read downwards, NOT the top row. Getting this backwards
        transposes the matrix, which turns a rotation into the opposite rotation
        AND a shear into the wrong axis — if both break at once it is ONE layout
        bug, not two.
        A*B means B FIRST. Forced by (A*B)v == A(Bv), not a convention to look
        up. Row-vector codebases (v' = v*M) read the other way; mixing the two
        gives code that is transposed AND backwards.
  depth: DEVICE DEPTH, in [0,1], 0 = NEAR, 1 = FAR. Smaller is nearer.
        CLEAR TO 1.0. Clearing to 0 claims the whole screen is already covered by
        something touching the lens, so EVERY fragment fails and the screen is
        empty — a total failure that reads like a broken transform.
        COMPARE WITH < (strictly). On a tie the pixel already there keeps it, so
        coplanar geometry has ONE STABLE winner (the first drawn) instead of
        flickering. Matches SDL_GPU_COMPAREOP_LESS. Note the consequence: at a
        SILHOUETTE, a back face and a front face tie exactly along their shared
        edge and the back face wins — a one-pixel artifact that 3.4's culling
        removes (measured: 29 px on the milestone scene, 0 with back faces culled).
        STORE DEVICE DEPTH, NEVER VIEW-SPACE z. Device depth is EXACTLY AFFINE in
        screen space (1/w is affine in screen space; z_ndc = -A + B*(1/w)), so
        barycentric interpolation of it is exact rather than approximate. View z
        is the RECIPROCAL of an affine function — a hyperbola — and interpolating
        it linearly reads -50.5 where the truth is -1.98 (near=1, far=100, screen
        midpoint). THE BUG HIDES on surfaces parallel to the screen, where 1/w is
        constant and both choices agree — i.e. on exactly the boxes-and-floors
        geometry test scenes are made of.
        PRECISION: dw = dz * w^2 * (1/near - 1/far). Quadratic in distance; the
        bracket is dominated by 1/near. near 1 -> 0.1 costs 10.09x EVERYWHERE;
        far 100 -> 1000 costs 0.9%. The near plane is the expensive one.
        THE TEST IS NOT PART OF THE BUFFER. depth_buffer stores and clears; the
        rasterizer compares. Same split as SDL_GPUDepthStencilState's three
        independent knobs (compare_op / enable_depth_test / enable_depth_write).
  clipping: NEAR PLANE ONLY, IN CLIP SPACE, BEFORE THE DIVIDE. Inside is
        z_clip >= 0 — NOT w >= 0, which is the plane through the EYE, `near` units
        closer, and which admits a point 0.0001 units in front of the camera that
        divides to x = 691714 px on a 320-px-wide buffer (measured).
        WHY BEFORE THE DIVIDE: the divide is the ONE information-destroying step in
        the pipeline. x/w for a point behind the eye and x/w for an ordinary point
        slightly left of centre are the SAME NUMBER. Nothing downstream can tell
        them apart, so nothing downstream can clip.
        WHY z_clip = 0 IS THE NEAR PLANE: 2.10's depth row is z_clip = A*z_v + B
        with A = f/(n-f), B = f*n/(n-f); setting it to zero cancels f and (n-f) and
        leaves z_v = -n exactly. The plane was ARRANGED to be a coordinate plane,
        which is what makes the test one comparison. Equivalent w form: w >= near.
        CROSSING PARAMETER t = da/(da - db), and it equals (z_a + n)/(z_a - z_b) —
        the far/(near-far) factor is common to both distances and cancels, so t does
        not depend on the far plane, the fov or the aspect at all. If yours changes
        when you change `far`, you have a bug.
        EVERY COMPONENT lerps with that one t, INCLUDING w. Interpolating x,y,z and
        forgetting w gives a vertex in the right place with the wrong depth scale —
        right shape, wrong size, and only on clipped geometry.
        THE OTHER FIVE PLANES ARE AN OPTIMISATION, not correctness: the rasterizer's
        bbox clamp already draws off-screen triangles correctly, just wastefully.
        Say which is which; it is the whole reason this lesson exists and 3.4's
        culling does not.
  culling: FROM THE SIGN OF THE SCREEN-SPACE SIGNED AREA, in the rasterizer, at
        the front of fill_triangle. FRONT-FACING IS edge_function < 0 in framebuffer
        coordinates — the convention is CCW-in-NDC (+y up), the viewport flips y, and
        a reflection reverses signed area. Measured: a CCW-in-NDC triangle through the
        real viewport gives -5184. DO NOT discover this sign by trying both; a wrong
        guess looks plausible (you see the inside of everything) and leaves you with
        two unexplained flips the day a mirrored transform arrives.
        NEVER dot(normal, camera_forward). That asks about the camera's AXIS; the
        question is about the RAY FROM THE EYE. Measured wrong on 15.46% of triangles
        at 55° fovy, 32.43% at 120°, and 0% under an ORTHOGRAPHIC projection — which
        is exactly why it survives: it is the ray test, for a camera we are not using.
        The correct view-space form is dot(n, centroid) < 0.
        WHY THE SCREEN TEST IS THE VIEW TEST: dot(n,a) = det[a,b,c], and
        2*area_ndc = kx*ky*(-det)/(wa*wb*wc). Clipping guarantees every w >= near > 0,
        so the divisor is positive and the sign is the determinant's alone. Culling
        without clipping is not merely unsafe, it is WRONG — a negative w flips the
        winding of a triangle that never moved.
        PRECONDITION: CLOSED geometry. "Facing away" implies "hidden" only for a
        surface enclosing a volume. Ground planes, quads and billboards need
        cull_mode::none; it is a property of the OBJECT, not the renderer, which is
        why it belongs on a material (Module 6) and why two cull modes = two batches.
        NOT "half the triangles" — that is a CEILING, not a rule. Measured: a cube
        shows 2..6 of its 12 (mean 5.55), an icosahedron 7..10 of 20 (mean 8.80).
        The eye can lie BETWEEN a parallel face pair's planes, in front of neither.
        And 54.8% of triangles removed bought 31.6% of the time (19.95 -> 13.64 us):
        back faces were the ones the z-buffer was already rejecting on their first
        depth test. Culling is worth MORE the more expensive the fragment work is.
  interpolation: BARYCENTRIC INTERPOLATION PROMISES ONE THING — the unique AFFINE
        function of the PIXEL POSITION agreeing with three corner values. So it is
        correct for a quantity affine in screen space and wrong for one that is not.
        Two answers, and the asymmetry is load-bearing:
          DEPTH        interpolated DIRECTLY. Already affine (3.1). Correcting it
                       twice is a real bug: self-consistent, so it reads as a depth
                       PRECISION problem and sends you to the near plane.
          EVERYTHING   perspective-corrected: interpolate a/w and 1/w, divide.
          ELSE         a is linear over the SURFACE; substituting the projection
                       gives a/w = (affine in x_n,y_n) + delta*(1/w), affine by 3.1.
                       One derivation covers uv, colour, normals — the constants
                       cancel and are never computed.
        vertex::inv_w stores 1/w PRE-DIVIDED (the loop wants 1/w, and the divide-back
        becomes a multiply). DEFAULTS TO 1 = the orthographic case, where the
        correction is the identity — so every 2-D fill written before 3.2 goes
        through the new path and comes out BIT-IDENTICAL (verified, 17275 px, 0 differ).
        COST: one divide per pixel, shared by every attribute, so it amortises the
        moment a fragment carries more than one thing.
        THE BLIND SPOT: a triangle whose plane is PARALLEL TO THE SCREEN has constant
        w, so affine and correct agree EXACTLY. Sprites, UI quads, billboards and the
        front faces of axis-aligned boxes are all in that family — which is why BOTH
        of Module 3's interpolation bugs survive any test scene made of them. The
        question to ask is not "what is different about the game" but "WHAT FAMILY OF
        INPUT DOES MY TEST SCENE STRUCTURALLY EXCLUDE?"
        THE SHAPE TO RECOGNISE: an error that is EXACTLY ZERO AT THE CORNERS and
        maximal in the middle is a chord drawn under a curve. That is why checking
        vertex values — the first thing anyone does — never finds it. Chord error is
        O(h^2), so subdividing converges QUADRATICALLY and never terminates: measured
        ratios 2.31, 2.42, 2.62, 2.84, 3.10 (-> 4), and 381 px still wrong at 2048 tris.
  homogeneous: w SAYS WHAT KIND OF THING THIS IS.
        w = 1  a POSITION  -> the translation column is added in full
        w = 0  a DIRECTION -> the translation column is multiplied by 0
        NOT a flag and NOT arbitrary: w is the MULTIPLIER ON THE TRANSLATION
        COLUMN, so 1 is the value that applies t exactly once. MEASURED: w of
        0 / 0.5 / 1 / 2 applies none / half / one / two of the offset. This is
        also WHY no purely linear map could translate — it had no input component
        that was always the same number.
        DIRECTIONS MUST CARRY 0. Sent as a position, a direction has the
        translation added, so THE ERROR EQUALS THE TRANSLATION and scales with
        distance from the origin: measured 10.77 / 107.70 / 1077.03 at x1 / x10 /
        x100. Invisible at the origin, ruinous far away — the worst possible
        detection profile. CHEAPEST TEST: a unit direction through a rotation must
        come back UNIT LENGTH (1.00, not 10.82).
        THE BOTTOM ROW (0,0,0,1) IS LOAD-BEARING. w_out = 0x+0y+0z+1w = w, so a
        position stays a position EXACTLY. The product of two matrices with that
        bottom row has it too, so the guarantee survives ANY depth of composition.
        VERIFIED: after 40 affine compositions the bottom row is EXACTLY
        (0,0,0,1) and w returns exactly 1.0 / 0.0. Such a matrix is AFFINE.
        THE PAYOFF: affine(A,ta) * affine(B,tb) == affine(A*B, A*tb + ta),
        verified. Exercise 2.5.3's hand-carried bookkeeping is ABSORBED into
        ordinary matrix multiply. Rotation about any pivot is now ONE matrix:
        T(c) * R * T(-c) — pivot fixed, distances preserved, both verified.
        "HOMOGENEOUS" = the representation is defined only up to overall scale,
        because you DIVIDE BY w to read a position out: (2,4,6,2) is the same
        point as (1,2,3,1). With w = 1 the divide is invisible, which is why
        xyz() DROPS w rather than dividing — deliberately, so 2.10 introduces the
        perspective divide under its own name instead of it turning out to have
        been hiding inside an accessor.
        A direction has w = 0 so its position is undefined — a point at infinity.
        W CAN BE NEITHER 0 NOR 1. Bottom row (0,0,-1,0) gives w_out = -z = the
        distance in front of the camera, and x/w then SHRINKS WITH DISTANCE:
        measured 1.0, 0.5, 0.2, 0.1 at z = -1, -2, -5, -10. THAT IS PERSPECTIVE.
        2.10 derives it from similar triangles — do not derive it early. Its two
        failure cases (z = 0 -> w = 0, undefined; z > 0 -> w negative and geometry
        from BEHIND the camera appears mirrored on screen) are why CLIPPING exists
        and are Lesson 3.3's. Clipping is not an optimisation.
  spaces: A COORDINATE IS THREE FLOATS AND A ROOM. vec3 is identical bytes in
        model or world space; nothing in the type or the arithmetic distinguishes
        them, and w CANNOT carry it (space does not change how a vector multiplies,
        only what the answer means). So the defence is NAMING, not the type system.
        MODEL space = the mesh's own room (k_cube_v: half a unit from the object's
        OWN centre). WORLD space = the one shared frame everything agrees on.
        NAME MATRICES a_from_b: produces space a FROM space b. Then a product's
        adjacent labels must match — view_from_world * world_from_model — and a
        wrong-ordered composition is a SPELLING mistake, visible before running.
        Values too: v_model before the multiply, v_world after. (2.8 §3.1)
  model-matrix: M = T * R * S. SCALE FIRST, ROTATE, TRANSLATE LAST — and the order
        is DERIVED, not conventional (2.8 §3.2): scale is along the object's own
        axes (so must act while coords are still the object's); rotation is about
        the object's own origin (so must act before it is moved off the origin);
        translation is a statement about the world (so it goes last). Written order
        is the REVERSE of what happens, because A*B applies B first.
        THE COLUMNS ARE THE OBJECT'S FRAME: c0/c1/c2 = the object's own x/y/z axis
        in world space, each times its size along that axis; c3 = position. Read a
        placed object straight off the matrix, no multiply. (2.8 §3.5, verified.)
        TWO WRONG ORDERS, TWO FAILURES: T*S*R applies the scale after rotation
        (along WORLD axes) so a non-uniform object SHEARS as it turns — a mesh
        right angle opening to 157.99deg for a 1.8:0.35 slab at 45deg, |x axis|
        sweeping 0.35..1.8 over a turn. R*T*S translates before rotating so the
        object ORBITS the world origin instead of spinning (== Exercise 2.5.3's
        "rotate about a point", correct for a moon/hinge). BOTH equal T*R*S when
        the scale is UNIFORM or the rotation is IDENTITY — verified bit-for-bit
        (worst diff 0.000e+00 over 360deg) — which is why the bug hides in most of
        a scene and only the one rotating non-uniform object catches it.
  transform: struct { vec3 position; mat3 rotation; vec3 scale{1,1,1}; }. The
        AUTHORING interface — position/rotation/scale in the order a human thinks,
        not the order applied. parent_from_local(t) is the ONLY place that knows
        the T*R*S order. Named parent_from_local not world_from_local because
        Module 5's hierarchy widens "parent" without changing a character. scale
        defaults to (1,1,1) not (0,0,0): the do-nothing scale is one. Rotation is a
        mat3 FOR NOW — 7.1 replaces it with a quaternion, touching one line.
        Rebuilt from a scalar angle every frame, NEVER accumulated into (a running
        matrix * small-delta drifts out of being a rotation — the SAME shear, by a
        different route).
  view-matrix: view_from_world = inverse(world_from_camera). A CAMERA IS AN
        OBJECT WITH A TRANSFORM; looking through it is UNDOING that transform, so
        moving the camera one way moves the world the other — exactly, not as a
        mnemonic (slide eye +2 in x -> a fixed world point's view x drops 2,
        verified). No general 4x4 inverse: a camera has no scale, so its placement
        is RIGID and inverse(affine(R, eye)) = affine(transpose(R), -transpose(R)*
        eye) — an orthonormal rotation's inverse is its transpose. Transposing puts
        the camera's right/up/backward axes into the view matrix's ROWS (fastest
        way to read a camera off its matrix). Last column of each row is -axis·eye.
        BASIS from eye/target/up_hint (right-handed, -Z-forward): backward =
        normalise(eye - target) [+z, because we look down -z]; right = normalise(
        cross(up_hint, backward)); up = cross(backward, right) [already unit].
        VERIFIED: eye -> origin, target -> (0,0,-d); V * world_from_camera == I;
        worked point (0,3,0) with eye (0,3,8) -> view (0, 1.94, -7.76).
        SINGULARITY: look straight up/down => look dir parallel to up_hint =>
        cross is zero => right undefined. Demo CLAMPS elevation to ~+-83 deg. The
        real fix (orientation with no preferred up axis) is the quaternion, 7.1 —
        this is its motivation, met early.
  projection: perspective(fovy, aspect, near, far) -> CLIP space. PERSPECTIVE IS
        ONE DIVIDE BY DEPTH, from similar triangles: x' = f*x/(-z), y' = f*y/(-z),
        so distant things shrink. A matrix is LINEAR and CANNOT DIVIDE, so P writes
        -z into w (the -1 in the bottom row) and a SEPARATE step divides by w. THAT
        IS WHERE w STOPS BEING 1 (2.7's third case, finally cashed). The divide is
        the PERSPECTIVE_DIVIDE, and it is why clip space (pre-divide) and NDC
        (post-divide) are distinct spaces. Matrix (column-major), written as rows:
          | f/aspect  0    0    0 |   f = cot(fovy/2)
          |    0      f    0    0 |   A = -far/(far-near)
          |    0      0    A    B |   B = -far*near/(far-near)
          |    0      0   -1    0 |   bottom row copies -z into w
        DEPTH maps near->0, far->1 (SDL_GPU range, NOT OpenGL's [-1,1]) and is
        1/z-NONLINEAR: near=1,far=100 puts z=-2 already at z_ndc=0.5. Precision is
        lavish near, starved far; PUSH THE NEAR PLANE OUT to fix z-fighting (3.1).
        The HANDEDNESS FLIP (right-handed view -> left-handed clip, conventions §5)
        happens INSIDE this matrix; +y stays up (the +y-down framebuffer flip is
        the VIEWPORT's job, 2.11). VERIFIED: (2,1,-10) -> clip (1.949,1.732,9.091,
        w=10) -> ndc (0.195,0.173,0.909); near->0, far->1 exactly.
  viewport: NDC -> framebuffer pixels + depth, the LAST hop of the chain (2.11).
        THREE INDEPENDENT AFFINE MAPS (a scale and an offset each; no division,
        nothing coupled):
          t = (ndc + 1)/2                      remap [-1,1] -> [0,1]
          screen.x = vx + t_x * w
          screen.y = vy + (1 - t_y) * h        <-- THE FLIP, and only y flips
          screen.z = min_depth + ndc.z*(max_depth - min_depth)
        THE Y-FLIP IS THE POINT: NDC's +y is UP, the framebuffer counts rows DOWN
        from the top (row 0 = top), so NDC's top edge (y=+1) must land on the
        SMALLEST screen y. Drop the (1 - t) and the scene renders UPSIDE DOWN — the
        classic beginner bug. This is the same lone minus sign that has drifted
        through to_screen since 2.5's basis demo and through project() in 2.10; as
        of 2.11 it lives in EXACTLY ONE function, viewport::to_screen, and nowhere
        else. src/gfx/viewport.hpp (header-only, NEW) mirrors SDL_GPUViewport
        FIELD-FOR-FIELD (x, y, w, h, min_depth, max_depth — VERIFIED against
        SDL3/SDL_gpu.h; x/y are the LEFT/TOP offset), so Module 4 fills SDL's struct
        by copying ours.
        min/max depth are usually [0,1] but narrowing is a real trick: render a HUD
        or gizmo at [0, 0.1] over a world at [0.1, 1] and it always wins the depth
        test, no extra pass (Exercise 2.11.4).
        PIXEL CENTRES: NDC +-1 maps to viewport EDGES, not pixel centres; column i's
        centre is ndc.x = (2i+1)/w - 1. The rasterizer samples centres at (x+.5,y+.5)
        (2.2) and that half-pixel is what keeps the two conventions consistent.
        VERIFIED: the new viewport reproduces 2.10's ad-hoc constants EXACTLY (worst
        diff 0.000e+00 over an NDC grid) — 2.11 moved no pixels, it named a transform.
  meshes: INDEXED GEOMETRY (2.12). A mesh is a VERTEX ARRAY (positions, each stored
        once) plus an INDEX ARRAY of uint16 taken in TRIPLES, one triple per
        TRIANGLE — never an edge list, because triangles are what gets filled (3.x),
        culled (3.4) and uploaded (Module 4). src/gfx/mesh.hpp (header-only, NEW):
        struct mesh { span<const vec3> vertices; span<const uint16_t> indices; }
        — two std::spans, so a mesh is NON-OWNING, four words, and cannot outlive
        its data (fine for inline constexpr arrays; Module 5's asset system is what
        happens when meshes are loaded at runtime). Data arrays are INLINE constexpr
        (ODR: a plain constexpr array in a header is one object PER TU).
        WHY INDEXED: the icosahedron's 12 vertices serve 20 faces, so unshared would
        store 60 positions and do 60 matrix multiplies/frame instead of 12. The
        saving is WORK, not just bytes; it is why GPUs have a post-transform cache.
        WIREFRAME FROM TRIANGLES draws each shared edge TWICE (60 draws for 30
        edges, 2x). Named, not hidden; it evaporates once triangles are filled.
        ALL FACES WOUND CCW FROM OUTSIDE, authored that way from the start even
        though nothing consumes winding until 3.4, so meshes never need re-authoring.
        VALIDATE, NEVER TRUST mesh data: Euler V-E+F=2; every UNDIRECTED edge in
        exactly 2 faces (manifold); every DIRECTED edge exactly once (consistent
        winding); each face normal cross(b-a,c-a) pointing away from the centre
        (outward). VERIFIED on the shipped data: icosahedron 12/30/20 -> Euler 2,
        all edges 2-shared, all 20 faces outward, degree uniformly 5, radius exactly
        1.000000, all 30 edges 1.051462. Cube 8/18/12 -> Euler 2 (18 edges, not 12,
        because triangulating each square face adds a diagonal).
        THE ICOSAHEDRON AND WHY phi IS FORCED: three mutually perpendicular
        rectangles of width 2 and height 2h have 12 corners = cyclic permutations of
        (0,+-1,+-h). Demanding ALL 30 EDGES EQUAL gives 2h^2-2h+2 = 4, i.e.
        h^2 = h+1 — the golden ratio's defining equation, so h = phi = 1.6180340.
        Normalising by sqrt(1+phi^2) = 1.9021131 puts every vertex on the UNIT
        SPHERE (so size is the transform's job, not the data's) and makes every edge
        2/1.9021131 = 1.0514622. k_icos_a = 0.5257311, k_icos_b = 0.8506508.
  cross-product: cross(a,b) = (a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y -
        a.y*b.x). Perpendicular to both; |a x b| = |a||b|sin(theta) = the
        PARALLELOGRAM AREA (the sin sibling of dot's cos). Zero for parallel
        inputs (no area, no unique perpendicular). Right-handed: x cross y = z,
        which IS our handedness. ANTICOMMUTES: cross(a,b) = -cross(b,a), so a
        swapped argument order flips an axis (left-handed frame -> mirrored /
        inside-out). INTRODUCED IN 2.9 (camera's right axis); 3.4 revisits it for
        a triangle normal and connects it to signed area + determinant. This
        REVISES vec3.hpp's old "deferred to 3.4" comment — it now lives in 2.9.
  winding: CCW = front, cull back (per-pipeline state; set explicitly every time)
  units: 1 unit = 1 metre; radians internally; linear colour in the renderer
  axis colours: x/y/z = red/green/blue (every diagram, no exceptions)
  sdl3: FetchContent, pinned GIT_TAG release-3.4.12 (main is 3.5.0 but unreleased);
        target SDL3::SDL3; SDL_TEST_LIBRARY OFF
  sdl3-api: bool SDL_Init / bool SDL_PollEvent; SDL_CreateWindow(title,w,h,flags) — no x/y;
            SDL_CreateRenderer(window,name); event.key.key (NOT SDL2's keysym.sym);
            #include <SDL3/SDL_main.h> separately; classic main + our own loop
  sdl3-events: SDL_Event is a tagged union — type@0, timestamp@8 identical in every
            variant; sizeof 128 (explicit MSVC/GCC ABI padding). Types grouped by range:
            0x100 quit · 0x2xx window · 0x3xx keyboard · 0x4xx mouse · 0x6xx joystick ·
            0x650 gamepad · 0x8000 user. SDL turns SIGINT/SIGTERM into SDL_EVENT_QUIT.
  sdl3-input: const bool *SDL_GetKeyboardState(int*) — bool in SDL3, NOT SDL2's Uint8;
            indexed by SDL_Scancode; SDL_SCANCODE_COUNT = 512 (A=4, W=26, SPACE=44).
            SDL_GetMouseState returns SDL_MouseButtonFlags + writes float x,y;
            SDL_BUTTON_MASK(n) = 1u<<(n-1), buttons 1..5.
            event.key.{scancode,key,down,repeat} — down/repeat are bool in SDL3.
            Wheel is event-only (no pollable state): event.wheel.{x,y} float, and
            direction == SDL_MOUSEWHEEL_FLIPPED means negate (macOS natural scrolling).
            SDL_PollEvent pumps (-> SDL_WaitEventTimeoutNS(e,0)), so the state arrays are
            only as fresh as the last drain. Focus loss -> SDL_SetKeyboardFocus(NULL) ->
            SDL_ResetKeyboard() sends key-UP EVENTS, so drain-then-sample is what stops
            keys sticking after alt-tab.
  sdl3-time: Uint64 SDL_GetTicks() ms / SDL_GetTicksNS() ns since SDL_Init. Both are
            MONOTONIC — not stated in the header, traced through SDL_GetPerformanceCounter
            to CLOCK_MONOTONIC_RAW / mach_absolute_time / QueryPerformanceCounter.
            SDL_Delay/SDL_DelayNS wait AT LEAST the requested time (measured: Delay(10)
            ~= 11.8 ms); SDL_DelayPrecise busy-waits to get closer.
            SDL_NS_PER_SECOND etc. in SDL_timer.h. SDL_SetRenderVSync(renderer, n) with
            SDL_RENDERER_VSYNC_DISABLED = 0 — can fail per backend, check the return.
            SDL_RenderDebugText/Format = built-in 8x8 ASCII bitmap font in the current
            draw colour; SDL_SetRenderScale also scales its coordinates. Real text = M6.
  time-model: absolute time = Uint64 ns; NEVER float seconds — ulp(86400.0f) = 7.8 ms, so
            86400.0f + 1/500 == 86400.0f and time FREEZES after ~24 h at 500 fps (compiled
            and verified). Only the small delta becomes float, and the ns->s division runs
            in double before narrowing. Milliseconds are too coarse to measure a frame
            (+-20% at 300 fps; truncates to 0 above 1000 fps).
            dt() clamped to 0.25 s, raw_dt() unclamped, was_clamped() reports the lie;
            fps() smoothed over 0.5 s = DISPLAY ONLY.
            dt-scaling is EXACT for constant velocity (v factors out of the sum) and only
            first-order for anything accelerating: explicit Euler error = 0.5*g*T*h,
            proportional to the step, so frame rate is an input to the physics. That is
            the whole argument for 1.4's fixed timestep.
  input-model: poll levels, DERIVE edges (pressed = cur && !prev, released = !cur && prev);
            COPY SDL's array into std::array, never alias the pointer (aliasing makes
            every edge x && !x = false); scancodes for positions, keycodes for symbols
  frame-order: THE loop, settled as of 1.4 and not changing again:
            drain events -> clk.tick() -> in.update() -> stepper.begin_frame(clk.dt())
            -> while (stepper.next_step()) { previous = current; simulate(current, h); }
            -> alpha = stepper.alpha() -> render(lerp(previous, current, alpha))
            Drain first because SDL_PollEvent pumps (that is what refreshes the state
            arrays); tick each subsystem exactly once so the whole frame sees one snapshot.
  fixed-step: h = 1/60 s default. INVARIANT: after the step loop 0 <= accumulator < h, so
            alpha in [0,1) and a lerp can never extrapolate. The accumulator may be float
            (it is BOUNDED — 1.3's Uint64 rule is about unbounded quantities).
            `previous = current` goes INSIDE the step loop (a frame may run 0..N steps;
            hoisting it out only breaks below the sim rate, i.e. never on the dev machine).
            simulate() receives h, never clk.dt() — the separation as a signature.
            Interpolation renders at exactly T - h: a CONSTANT lag replaces one swinging
            0..h. Smoothness is consistency, not immediacy.
            Spiral of death when cost-per-step / h > 1. Two guards: clock's 0.25 s dt clamp
            (bounds a frame to 15 steps at 60 Hz) + fixed_step's per-frame cap, which
            DRAINS the excess (not just returns — else alpha > 1) and REPORTS the dropped
            time. Past the cap the sim falls behind permanently: slow motion, a real loss.
            Determinism = same binary + same machine + same inputs. NOT cross-platform
            (FMA contraction, x87, libm, vectorisation).
            NEVER interpolate across a teleport — snap previous = current instead. That is
            why the 1.4 demo's box bounces rather than wrapping.
  sdl3-pixels: SDL_CreateTexture(renderer, format, access, w, h) with
            SDL_TEXTUREACCESS_STREAMING for a per-frame buffer.
            SDL_LockTexture(tex, NULL, &pixels, &pitch) = WRITE-ONLY, previous contents
            UNDEFINED — SDL's docs say to keep the master copy app-side, which the
            framebuffer is. The returned pitch MAY EXCEED width*4 (driver row padding),
            so copy ROW BY ROW or the image shears on other people's machines.
            SDL_UpdateTexture is documented as slow / for static textures.
            SDL_RenderTexture(r, tex, NULL, NULL): NULL dst = entire render target, so a
            small framebuffer scales to the window and resize needs NO code.
            SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST) for crisp upscaling
            (SDL_SCALEMODE_PIXELART also exists, 3.4+).
            FORMAT NAMES: "8888" = packed into a native-endian integer, MSB first
            (ARGB8888 = 0xAARRGGBB). "32" = byte order in memory. On little-endian they
            are REVERSED: RGBA32 == ABGR8888 and BGRA32 == ARGB8888 (header-verified).
            We store Uint32 and build with shifts only, so ARGB8888 matches everywhere
            and endianness never enters the engine until an image loader (Module 6).
            Symptom: red/blue swapped with green fine = channel order, never gamma.
  framebuffer: row-major, index = y*width + x. Right = +1, down = +width.
            An x past width is NOT an error — it lands on the next row (candy-striping);
            a stray y leaves the buffer entirely (UB, and the crash is the lucky case).
            put_pixel is bounds-checked; row(y) is the documented fast path (row index
            still clamped). fill_rect CLIPS ONCE then std::fill_n per row. No clear() in
            the demo because the gradient covers every pixel — a real optimisation with a
            real trap attached.
            MEASURED (M4 Pro, median): put_pixel vs row pointer = 5.1x (-O0), 14.8x (-O2);
            rows-outer vs columns-outer = 10.9x @320x180, 32.7x @720p, 48.8x @4K
            (64-byte line = 16 pixels). Rows outer, columns inner, always.
            BENCHMARK TRAP: measuring both loop orders THROUGH put_pixel gave 1.00x —
            its overhead swamped the effect. Make the paths differ ONLY in what is under
            test, and measure at -O2.
  colour: stored channel values are sRGB-ENCODED, not light. VERIFIED BOTH IN PYTHON
            AND IN THE C++: 128 emits 21.6% of white's light; half the light is stored
            as 188; fades 75%->225, 50%->188, 25%->137, 10%->89 (naive 64 for 25% emits
            only 5.1%). Red+green at t=0.5: naive (128,128,0) dark olive vs linear
            (188,188,0) bright yellow. Encode/decode round trip is LOSSLESS for all 256.
            Code budget: evenly spaced light puts 26 of 256 codes in the darkest 10%,
            sRGB puts 90; evenly spaced wastes 128 on the brightest half, sRGB 68.
            Use the EXACT piecewise transform (0.04045 / 12.92 / 0.055 / 2.4), never
            pow(x,2.2). Tabulate DECODE only (256 inputs); encode's input is continuous.
            ALPHA IS COVERAGE, NOT LIGHT — never transfer-function it. mix_linear
            converts three channels and leaves the fourth.
            SAFE on stored values: copy, compare, pick. WRONG: mix, fade, average,
            downscale/mipmap, add light, anti-alias edges.
            NOT YET LINEAR: we convert per-operation (slow + lossy). A real pipeline
            decodes once in, encodes once out, and needs a float/half framebuffer,
            headroom above 1.0, and tonemapping = Module 6.
            Fingerprints: red/blue swapped + green fine = channel order, NOT gamma;
            muddy fades / early-dying fades / darkening mipmaps / fringed text = gamma;
            washed-out milky = the conversion applied twice or backwards.
  vec2: an ARROW — direction and length, NO position. Components are its SHADOWS on
            the axes, which is WHY dot(a,b) = ax*bx + ay*by. Derivation needs only
            "shadows add" + "x_hat . b = |b|cos(alpha) = bx" — NO law of cosines
            (the course assumes no trig beyond basics).
            VERIFIED: (3,4).(4,3) = 24 both ways; |a|=|b|=5, cos=0.96, theta=16.2602 deg,
            shadow 4.8, 4.8*5 = 24. Signs: (6,8)->50 front, (-4,3)->0 perpendicular,
            (-3,-4)->-25 opposite.
            dot(v,v) == length_squared(v). PREFER length_squared for comparisons (sqrt is
            monotonic): square the constant, never root the variable.
            NORMALISE the direction THEN scale — normalised(input)*speed*dt, never
            normalised(input*speed*dt). |(1,-1)| = 1.41421 so a raw diagonal step is
            127.27922 vs 90.00000: Exercise 1.2.3's 41.4%, now closed.
            normalised({0,0}) MUST NOT be 0/0 = NaN; ours returns (0,0), with
            normalised_or(v, fallback) where a direction must exist.
            perpendicular({x,y}) = {-y,x}; dot(v, perpendicular(v)) is EXACTLY 0.
            sizeof(vec2) == 8 -> PASS BY VALUE, never const&.
            HEADER-ONLY on purpose (small/hot/stable, must inline) — and a header-only
            addition needs NO CMakeLists change. Not a general licence.
            Does NOT generalise to 3-D: perpendicular() (a whole plane of them in 3-D);
            the cross product is the 3-D-only operation Module 2 adds.
  collision: a discrete overlap test answers about an INSTANT; collision is a fact about an
            INTERVAL. Overlap window along an axis = size_a + size_b (Minkowski sum), so a
            naive test is guaranteed only while |v_axis|*h < size_a + size_b. Ours: 8 px window,
            ball capped 260 px/s -> safe to 480 px/s at 60 Hz (bug UNREACHABLE and still there),
            240 at 30 Hz (intermittent), 80 at 10 Hz (fails on the opening serve).
            SWEPT FIX: t = (face - lead_from)/(lead_to - lead_from); require the edge BEGAN near
            and ENDED far (also makes the divisor non-zero by construction); interpolate the
            other axis AT t, not at the endpoint; then spend the remaining (1-t) of the step.
            VERIFIED: ball at x=16, -105 px/s, h=0.1, face x=14 -> t=0.190476, speed 105->112,
            vel (109.5525,-23.2861), final x=22.8685; naive test on the same step says NO HIT.
            One impact per step for now — Module 8 iterates until the budget is spent.
  reflect: reflect(v,n) = v - 2*dot(v,n)*n, derived as "subtract the shadow twice". n MUST be
            unit (project_onto's /|n|^2 is what is missing); a length-k normal scales the
            correction by k^2 and the ball silently gains/loses energy. Assert |reflect|==|v|.
            VERIFIED (3,4) off (0,-1) -> (3,-4); (0,5) off a 45-deg wall -> (5,0).
            A bounce needs BOTH velocity turned AND position mirrored back inside: velocity-only
            leaves the ball outside for a step, position-only sticks it to the wall.
            Framebuffer normals point INTO the court: ceiling (0,+1), floor (0,-1) — +y is down.
  determinism: same binary + machine + seed + inputs + STEP SIZE. Verified bit-identical over
            20000 steps; the same seed at 60 vs 120 Hz diverges within 2 simulated seconds,
            because h is an input too. PRNG seed lives IN the state (not SDL_rand's hidden
            global) or the sim is not a function of its inputs. xorshift32: seed 0 is a fixed
            point — guard it; take the top 24 bits for an exact float in [0,1).
  engine/game: src/game/ is the first NOT-engine directory. Test: could a different game use
            this unchanged? game -> engine only, never back. Enforced by discipline today, by
            the compiler in Module 5. pong.hpp FORWARD-DECLARES engine::framebuffer rather than
            including it (include what you use, forward declare what you mention).
  lines: endpoint-INCLUSIVE at BOTH ends (so a rectangle's corners close). Bresenham,
            integer, all 8 octants. Ties break toward NE (E >= dx, not >).
            Derivation: e = y_true - y_plotted; e += m each step; e >= 1/2 -> step minor,
            e -= 1. Scale by 2*dx to clear denominators (comparisons survive multiplication
            by a positive constant): E += 2dy, test E >= dx, E -= 2dx. All integers, E=0.
            TERMINATION PROOF: with dx=|Dx|>=0, dy=-|Dy|<=0, both tests failing needs
            dx < 2*err < dy <= 0 <= dx, i.e. dx < dx. So one always fires.
            TIE THEOREM: with p = major/gcd(major,minor), an exact tie exists IFF p is even
            — and EXACTLY those lines are asymmetric under endpoint swap (verified over
            23103 lines, 7692 asymmetric, zero disagreements). Slope 1/2 ties constantly;
            3/5 and 45 degrees never do. This is WHY 2.2 needs a fill rule.
            BENCHMARK (M4 Pro, clang 21, -O2, ns/px, stepping only): Bresenham compact 1.32,
            Bresenham major-axis 0.74, DDA lround 0.60, DDA trunc 0.65. DDA IS FASTER —
            the folklore is inverted. Cost is the two data-dependent BRANCHES, not floats
            (swapping lround for truncation changes nothing). We ship Bresenham for
            EXACTNESS (integers are bit-identical everywhere; 1.8 showed floats are not)
            and because its error term IS 2.2's edge function. Lines are not the hot path.
  triangles: edge_function(a,b,p) = (bx-ax)(py-ay) - (by-ay)(px-ax) = z of the 2-D
            cross product = dot(P-A, perpendicular(B-A)). SIGN = which side (0 = exactly
            on the line); MAGNITUDE = 2 * area of triangle ABP.
            The three edge functions SUM to the total area, for points INSIDE AND OUTSIDE
            — verified; a superb debug assertion, and 2.3's barycentric weights unnormalised.
            AFFINE in the pixel, so stepping is constant: dE/dx = ay-by, dE/dy = bx-ax.
            fill = bbox (clipped) + incremental stepping + top-left rule. MEASURED 75x
            faster than direct full-buffer evaluation (5763 ms -> 77 ms), and pixel-identical
            to it over 144 triangles incl. many straddling the buffer edge.
            Inner loop writes through fb.row(y) — safe because the bbox was clipped first.
            edge_function OVERFLOWS int32 past ~+-16000 coords. Signed overflow is UB, so
            this is documented in the header; Module 3's clipping keeps us inside it.
  winding-screen: "CCW = front" is an NDC statement. In the FRAMEBUFFER (+y down) the
            viewport flip reverses it: screen-CCW has NEGATIVE signed area. MEASURED:
            (5,0),(0,10),(10,10) -> -100; reversed -> +100. fill_triangle accepts EITHER
            winding (measures area once, swaps two vertices if negative) so nothing
            vanishes; culling by sign is Lesson 3.4's job, in its own space.
            Zero area = collinear = draws nothing. That check is LOAD-BEARING: the fill
            rule's proof needs a non-degenerate edge.
  assets: GEOMETRY FROM DISK IS DATA, AND DATA GETS CHECKED. Settled in 3.5.
            OBJ INDICES ARE 1-BASED (slot = index - 1) and may be NEGATIVE, meaning
            relative to the count SEEN SO FAR (slot = n + index, no extra -1).
            Index 0 is illegal, which makes it a free "absent" sentinel.
            A VERTEX IS THE TRIPLE (i_v, i_vt, i_vn). Corners share a vertex only when
            they agree about EVERYTHING. Numbered by first appearance, so a load is
            reproducible and diffable.
            ATTRIBUTE ARRAYS ARE INDEX-PARALLEL OR EMPTY. Empty means "has none"; an
            array of zeroes would be the different and more confusing claim that every
            pixel samples one texel.
            FAN TRIANGULATION, (0, k-1, k) — same fan as 3.3's clipper and the same
            condition, stated precisely: correct IFF corner 0 SEES the whole polygon
            (star-shaped about it). Convexity is the sufficient version, sufficient
            because then EVERY corner works. So a concave face may fan fine from one
            corner and grow fins from another — the bug depends on where the exporter
            started listing, which is what makes it intermittent across files.
            uint16 INDICES, ceiling 65536 = SDL_GPU_INDEXELEMENTSIZE_16BIT. Exceeding
            it is an ERROR; a silent wrap builds triangles from unrelated corners.
            NORMALS AND UVS STORED AS WRITTEN — a loader is not a place where data may
            differ from its source. THE uv ORIGIN IS SETTLED (3.9): exporters write v
            bottom-up, SDL_GPU samples top-down, and the flip is v -> 1 - v applied at
            the IMPORT step (flip_uv_v), not in the parser and not in the sampler. The
            parser's rule above is unchanged; a loader must not alter its input, but a
            pipeline may. Applied to EVERY mesh a load produces, including in-memory
            ones, or the 3.5 round trip starts measuring the import.
            MALFORMED IS FATAL, SILLY IS COUNTED. `f 1/x/2` stops the load with a line
            number; a zero-area face is dropped and reported. Both in the report.
            TOPOLOGY IS MEASURED ON THE WELDED MESH. A uv seam legitimately stores one
            point twice; ask the raw arrays and a watertight model reports a
            seam-shaped hole. Welding is by EXACT bits (with -0.0 normalised) because
            the duplicates came from one computation — a tolerance is Module 5's.
            THE SEAM ONLY WELDS IF YOU ARRANGE IT: computing the wrap angle from
            u = 1.0 gives sin(1.0f*tau) = 1.748e-7 instead of 0, so the two copies
            differ in the last bit and 48 boundary edges appear in a closed torus.
            Compute it from `i % nu`. (Measured, verify_35 §E.)
            EULER IS A DIAGNOSTIC, NOT A VALIDITY CONDITION. V - E + F = 2 - 2g; the
            torus gives 0. Asserting 2 rejects every handle and hole ever modelled.
            "WOUND OUTWARD" = SIGNED VOLUME > 0, by the divergence theorem — assumption
            free, unlike 2.12's centroid test which needs a star-shaped solid and fails
            on the first torus. Accumulate in double: the terms nearly cancel.
  lighting: IN WORLD SPACE, PER VERTEX, IN LINEAR LIGHT. Settled in 3.6.
            LAMBERT IS A FOOTPRINT, NOT A FORMULA. A beam of fixed cross-section on a
            surface tilted by theta covers 1/cos(theta) more area, so power per unit
            area falls by cos(theta). Measured: at 60 deg the footprint is exactly
            2.00 and the brightness exactly 0.50.
            l POINTS TOWARD THE LIGHT. A directional light stores the direction light
            TRAVELS (midday sun = (0,-1,0)); to_light() is the negation and exists so
            the negation has a name. Getting it wrong lights the scene from precisely
            the wrong side and NOTHING LOOKS BROKEN.
            THE CLAMP IS LOAD-BEARING. Past the terminator the dot goes negative and
            SUBTRACTS light: measured -0.740 in red for albedo 0.8. It hides on the
            unlit side (already black) and shows as a hard black rim eating into the
            LIT side near the terminator.
            NORMALS GO THROUGH THE INVERSE TRANSPOSE, never the model matrix. Derived
            from the only thing that defines a normal: perpendicular to every tangent,
            so dot(M*t, X*n) = 0 forces M^T X = I, X = (M^-1)^T.
            AND IT HIDES. Rotation: X == R exactly (measured max diff 5.96e-08).
            Uniform scale: X = (1/s)R, same direction, 0.0000 deg apart. Only a
            NON-UNIFORM scale differs — our slab (1.8,0.35,0.9) tilts a normal by up
            to 67.99 deg, the plinth (1.2,0.25,1.2) by 66.46, the icosahedron by 0.03.
            Rendered on a squashed torus: 97.5% of covered pixels differ, worst
            channel delta 135/255. THE HERO OBJECT LOOKS PERFECT, which is why it ships.
            NORMALISE AT THE POINT OF USE. The inverse transpose does not preserve
            length (the worked example produces one of length 2.16), and neither does
            interpolation. An un-normalised normal scales brightness by its length.
            NO NORMALS -> FACE NORMAL, cross(b-a, c-a), through the SAME normal matrix
            because a face normal is a normal. Zero is the sentinel (normal_at), and it
            survives a matrix multiply as zero.
            LAMBERT IS VIEW-INDEPENDENT. Moving the camera must not change the shading
            (verified: the brightest lit pixel is identical from two camera angles);
            moving the light changed 6,104 px. 3.7's specular is the first view-dependent
            term.
            WHERE THE NORMAL COMES FROM is a SEPARATE axis from WHERE THE EQUATION IS
            EVALUATED. 3.6 does per-vertex evaluation with either normal source; 3.8
            compares flat/Gouraud/per-pixel properly. And cube.obj cannot tell the two
            sources apart — 0 px — because 3.5's split already gave each of its 24
            vertices its own face's normal. A faceted mesh is faceted because of the
            SPLIT, not the shading model. (torus.obj: 5,576 px.)
  specular: THE FIRST VIEW-DEPENDENT TERM IN THE COURSE. Settled in 3.7.
            EVERYTHING BEFORE IT could be evaluated without knowing where the viewer
            was standing, and 3.6 MEASURED that: one fixed point shades 0.83408 from
            two different eyes, the same float. A highlight cannot, and the cost is
            structural, not just arithmetic (see the world_pos note below).
            MIRROR DIRECTION R = 2(n.l)n - l, and it IS -reflect(l,n) exactly (worst
            |sum| over 20000 random pairs: 0.000000). The sign differs because a
            velocity points INTO a surface and a light direction points OUT of it.
            Both names ship; picking the wrong one puts the highlight on the far side.
            R IS UNIT (R.R = 4c^2 - 4c^2 + 1 = 1, so no renormalise) and makes the
            SAME ANGLE with n that l does (n.R = 2c - c = c). Consequence used below:
            R CAN NEVER BE BELOW A SURFACE THE LIGHT IS ABOVE.
            HALFWAY h = normalise(l+v) IS NOT AN APPROXIMATION. Demanding
            mirror(h,l) == v gives 2(h.l)h = l+v, so h must lie along l+v and being
            unit fixes it. Verified: worst |mirror(h,l) - v| = 6e-6 over 20000 pairs.
            THAT is why dot(n,h) asks about the SURFACE ("how much of it faces this
            way") rather than about a ray — the microfacet question, and Module 6's.
            Zero when v == -l, so the term dies rather than peaking.
            beta = alpha/2 EXACTLY (worst 0.000018 deg over a 33x33 sweep), because R
            is fixed by the light while h bisects. Hence q ~ 4p, from
            cos^k x ~ exp(-k x^2/2). FITTED: 4.38x at p=4 -> 4.01x at p=128. It is a
            NEAR-PEAK match and gets WORSE for broad lobes — on the torus, matched
            exponents differ on 85.5% of the object at p=2 and 3.3% at p=64.
            ANY COMPARISON MUST CONVERT THE EXPONENT FIRST or it measures lobe width.
            THE CUT-OFF, AND WHAT IT IS NOT. Phong returns 0 when |a+b| >= 90 deg,
            i.e. when the light and eye are on the SAME SIDE of the normal (on a
            floor: the sun BEHIND you). NOT "the mirror ray dips below the surface" —
            it never does, see above; that folklore was in this lesson's first draft
            and had to be re-derived. What happens is that cos^p only answers over the
            hemisphere AROUND R, which is not the VISIBLE hemisphere, and the visible
            wedge it misses is exactly as wide as the light's angle from n. Measured:
            dot(R,v) <= 0 for 50.4% of above-surface pairs, dot(n,h) <= 0 for 0%.
            Rendered on a plane, sun behind: at 35 deg elevation Phong highlights
            0 of 30806 lit px and Blinn all 30806; at 60 deg, 6168 vs 30806 with
            24638 px Phong paints black that Blinn does not (worst delta 27/255).
            With the sun AHEAD (opposite sides) there is NO cut-off at all and both
            cover the floor — that control is in render_37 on purpose.
            BOTH TERMS CARRY n.l. The cosine law is about ARRIVAL and says nothing
            about what the surface does with the light afterwards. Classic Phong omits
            it: measured 0.608 of full strength on unlit geometry, over 400 of 701
            unlit-but-visible normals. Including it also makes a separate "zero the
            specular past the terminator" guard unnecessary.
            AMBIENT GETS NO SPECULAR — it has no direction, so no mirror direction.
            SPECULAR COLOUR IS NOT THE ALBEDO. A dielectric mirrors off a clear outer
            layer without tinting (white highlight on a coloured body = plastic); a
            metal has no such layer and tints what it reflects. Module 6 = F0.
            NOT ENERGY CONSERVING, and said so with numbers: integral cos^p dw =
            2pi/(p+1), and with the outgoing cosine 2pi/(p+2) (both confirmed by
            quadrature). p 32 -> 64 halves the reflected light (x0.515) while the peak
            stays at exactly 1.0 — backwards, since a smoother surface should
            CONCENTRATE the same light. (m+8)/(8pi) is the usual normalisation:
            0.4775 at m=4, 10.5042 at m=256, so it needs HDR and waits for Module 6.
            THE HIGHLIGHT'S POSITION IS PREDICTABLE on a plane: p = e - R*(e_y/R_y),
            because h == n exactly when v == R and R is the same everywhere on the
            plane. Verified against a 1601^2 argmax, worst error 0.0107 (grid step
            0.02). The peak is often BEHIND the camera, which is why the sun-behind
            case shows only the far tail of the lobe.
            THE CLAMP TO 1 IS REAL: dot of two unit vectors can round above 1 and pow
            amplifies it (measured peak 1.00001). std::min(1.0f, .) makes the
            advertised [0,1] range a fact. GPUs spell it saturate.
            PER-VERTEX IS THE WRONG PLACE, MEASURED THREE WAYS (this is 3.8's case):
              (a) CHORD ERROR. Interpolating the answer vs evaluating at the
                  interpolated normal, over every triangle of the 48x24 torus: worst
                  0.311 at shininess 32, 0.507 at 64, 0.722 at 128 — and 87.8% of the
                  lit area differs at 128. THE ERROR GROWS WITH SHININESS, because a
                  tighter highlight is a sharper feature and a chord approximates a
                  sharp feature badly. Same shape as 2.4's bias and 3.2's affine uv.
              (b) THE PEAK IS MISSED on coarse meshes: an 8x6 torus finds 0.0% of the
                  true peak, 12x8 finds 8.5%, 24x12 73.6%, 48x24 99.6%. 96x48 also
                  finds 99.6% — QUADRUPLING THE VERTICES IMPROVED NOTHING, because
                  whether a vertex lands near the peak is luck.
              (c) SO IT FLICKERS. Over 180 frames of a spin, the brightest pixel is no
                  brighter than the diffuse-only render in 157 of 180 frames for
                  cube.obj, 107 for the icosahedron, 58 for a 12x8 torus, 0 for 48x24.
                  The CUBE is worst, and the reason is the lesson: on a flat face the
                  peak sits in the MIDDLE and per-vertex only samples the corners.
  shading: TWO INDEPENDENT AXES, and 3.8 exists because 3.6 shipped them as one enum.
        WHERE THE NORMAL COMES FROM (face vs vertex) is a property of the MESH and
        its vertex splits. WHERE THE EQUATION IS EVALUATED (flat / Gouraud /
        per-pixel) is a property of the PIPELINE. Six cells, not six pictures.
        WITH A FACE NORMAL ALL THREE EVALUATION POINTS AGREE EXACTLY — 0 px, not
        "close" — FOR EXACTLY AS LONG AS THE SHADING IS VIEW-INDEPENDENT. A face
        normal makes the normal constant across the triangle; the albedo already
        was; and under diffuse-only nothing else enters. A SPECULAR term breaks it
        because to_eye varies across a face even when the normal does not: measured
        1231 px (flat) and 761 px (Gouraud) against per-pixel with Blinn p=32.
        (The first draft of is_degenerate() said face x gouraud was ALWAYS
        degenerate. verify_38 §A found the 761 px. The corrected rule is SHORTER.)
        GOURAUD IS A NAME FOR WHAT 2.4 + 3.6 ALREADY BUILT — interpolation plus lit
        vertex colours. No new machinery, only a name.
        PHONG SHADING != THE PHONG REFLECTION MODEL. Same 1975 paper, two things, one
        on each axis. This engine now runs PHONG SHADING with the BLINN-PHONG
        REFLECTION MODEL, which is the standard modern pair.
        THE INTERPOLATED NORMAL IS SHORT: |lerp of two unit normals theta apart| =
        cos(theta/2) — 3.4% at 30 deg, 13.4% at 60. Worst on the shipped torus
        0.99051 (0.95%). ZERO AT THE CORNERS, MAXIMAL IN THE MIDDLE: the chord
        signature again (2.4, 3.2, 3.7). This engine never pays it because 3.6 made
        shade() normalise its own argument — a debt booked in 2.4's header and paid
        two lessons before anyone could incur it.
        MACH BANDS: Gouraud's ramp is C0 but not C1, and lateral inhibition turns
        each slope break into an apparent stripe that IS NOT IN THE BUFFER. More
        colour precision does nothing. Tessellation converges O(h^2) and never
        terminates. Per-pixel removes them because a curve has no kinks.
        PER-PIXEL CANNOT INVENT NORMALS A MESH DOES NOT HAVE. Measured over a
        180-frame spin, frames with no highlight at all: torus 12x8 goes 58 -> 0,
        but cube.obj stays at 157 -> 157 and the icosahedron 50 -> 51. Six normals
        is six normals. THE TWO AXES FIX DIFFERENT THINGS and neither substitutes.
        3.6's result survives: on cube.obj face vs vertex normals is still 0 px
        (welded torus: 4122).
        COST — AND THE FOLKLORE IS WRONG BELOW ~3 PIXELS PER TRIANGLE. Same mesh,
        resolution swept, per-pixel / Gouraud: 0.91x at 320x180 (2.8 px/tri), 1.41x
        at 640x360, 1.84x at 720p, 2.12x at 1440p, 2.15x at 4K. The asymptote 2.15x
        is the real steady-state price; below the crossover there are MORE VERTICES
        THAN COVERED PIXELS and per-vertex is the expensive one. Always report the
        px/triangle ratio with a shading timing or the number is unusable.
  varyings: vertex = position + VARYINGS as of 3.8. `normal` and `world` are inputs
        to a calculation that has not happened yet, carried to the fragment.
        vertex::colour CHANGES MEANING with the pipeline — a lit result under
        vertex_colour, the ALBEDO under lit. That is what a varying is.
        ANYTHING THE FRAGMENT READS MUST BE INTERPOLATED BY EVERY STAGE BETWEEN,
        and the clipper is the one that gets forgotten because it usually does
        nothing. Forget it and shading breaks only on triangles crossing the near
        plane — i.e. only when you walk into something.
        COST: vertex 28 -> 52 bytes, six more floats interpolated per pixel.
  fill-rule: top-left. For a triangle oriented to POSITIVE area:
            top edge = (dy == 0 && dx > 0); left edge = (dy < 0). Bias -1 on the others,
            folded into the loop's starting value, so it costs NOTHING per pixel.
            WHY IT WORKS WITHOUT COORDINATION: two triangles share an edge by traversing
            it OPPOSITELY, so any rule phrased on edge direction answers oppositely.
            VERIFIED: quad + 12-triangle fan, 0 px drawn twice, 0 interior gaps; with the
            rule off the same quad double-draws its whole seam.
            COVERAGE BECOMES HALF-OPEN — a lone 37x37 quad loses exactly 73 px (bottom row
            + right column - shared corner = 37+37-1). Nothing else dropped, nothing added.
            That is the [start, end) trade: half-open tiles, closed cannot.
            Double-draws only hit pixels whose centres are EXACTLY on the seam, so an
            axis-aligned or 45-degree edge fails TOTALLY (40 px on a 40 px seam) while a
            rotated one loses 2-3 stray pixels. Common geometry is the catastrophic case.
  barycentric: w_i = area of the sub-triangle OPPOSITE v_i, over the total.
            w0 uses the edge v1->v2. THE PAIRING IS THE BUG: a rotated pairing still
            sums to 1 and still looks plausible — VERIFIED, it reconstructs (5.2,5.8)
            instead of (5,5) for the standard example. ALWAYS assert RECONSTRUCTION
            (w0*v0 + w1*v1 + w2*v2 == P), never just the sum: the sum passes for all
            three wrong rotations, reconstruction for exactly one.
            e0+e1+e2 == area EXACTLY in integers, for EVERY P in the plane (inside or
            outside) — the P terms cancel symbolically. Free assertion; leave it in.
            Geometry: 1 at its own vertex, 0 on the opposite edge, 1/3 at the centroid,
            NEGATIVE outside. "All three >= 0" IS 2.2's inside test divided by a
            positive constant — verified identical over 5041 points.
            Constant weight = a line PARALLEL to the opposite edge, evenly spaced,
            because that edge is a fixed base so equal area means equal height.
            MEASURED: w0 varies by EXACTLY 0 along such a line.
            Interpolation with these weights is the UNIQUE affine function matching the
            three corners (3 coefficients, 3 independent conditions) — not merely a
            reasonable blend. Affine in SCREEN space, which stops being surface-correct
            under perspective: that is Lesson 3.2's 1/w trick, and the artifact is
            swimming textures.
            PRECISION (measured over 32761 points): worst |sum-1| = 2.4e-7 — one
            rounding, NOT accumulation, because only the final division is inexact.
            But the sum is bitwise 1.0f only ~85% of the time. NEVER compare weights
            for equality; test the INTEGER edge values, where "on the edge" is == 0.
            USE UNBIASED edge values for interpolation — the top-left rule's -1 bias is
            for coverage only and shifts weights by a fraction of a pixel. 2.4 must
            carry both sets.
            Degenerate (collinear) triangle -> all zeros, the one case where the weights
            do not sum to 1. Documented in the header; a NaN here would spread silently.
  interpolation: a(P) = w0*a0 + w1*a1 + w2*a2. The UNIQUE affine function through the
            three corners — 3 coefficients, 3 conditions, one solution — so there is
            nothing to tune and no better scheme to find. Works for ANY payload you can
            scale and add; the rasterizer never learns what it carries, which is also why
            it returns an interpolated NORMAL that is no longer unit length (renormalise
            per pixel, 3.6) and a DEPTH that is not perspective-correct (3.2).
            VERIFIED: an attribute that is itself affine in position (3x - 2y + 7)
            reproduces itself over 3721 points, worst error 3.05e-5 on values up to 120.
            UNBIAS BEFORE INTERPOLATING — the single silent trap of 2.4. The top-left
            rule's -1 is a COVERAGE decision. Left in the accumulators when you divide it
            (a) breaks sum-to-one by (b0+b1+b2)/2A, and EXACTLY one or two of the three
            biases is -1, never zero of them and never all three — because the three
            directed edges' dy sum to 0 and cannot all be 0, so at least one is negative
            (a left edge, bias 0) and at least one positive (bias -1). Brute-forced over
            all 495,648 non-degenerate triangles in a 9x9 grid: 247,824 sum to -1,
            247,824 to -2, none to anything else.
            (b) TRANSLATES the whole attribute field — rigidly, not tilted — by
                displacement = 1 / |edge opposite that weight's vertex|, in PIXELS,
                perpendicular to that edge. The area CANCELS. Derived from
                |grad w0| = |e| / 2A; both verified numerically.
            So the bug lives in SMALL triangles, i.e. dense meshes. On a smooth attribute
            it is a fraction of one colour level and invisible. On a QUANTISED one (texel
            index, stripe, checker cell) a sub-pixel shift at a threshold flips WHOLE
            pixels: measured 0 to 15 wrong out of 52 on ONE triangle with ONE fixed
            0.088 px error, depending only on where the thresholds happened to fall.
            THEREFORE: derive the magnitude of a sub-pixel error, never look for it.
            "It looked fine when I tried it" is a sample of one from a distribution
            containing both 0 and 15.
            The fix is one exact integer subtraction: the accumulator holds E + bias and
            bias is a known constant, so E = accumulator - bias, with no drift to unwind.
            Coverage keeps using the BIASED value in the same loop, one line above.
  colour-interpolation: DECODE TO LINEAR, INTERPOLATE, ENCODE ONCE. A stored channel is
            a CODE for a quantity of light, not the quantity. MEASURED on an R/G/B
            triangle: centre pixel (156,156,156) in linear light vs (85,85,85) on stored
            values — 0.3325 vs 0.0908 of white's light, a factor of 3.66, so the naive
            blend emits 27% of what it claims. R/G edge midpoint 188 vs 128, matching
            Lesson 1.6 exactly (same arithmetic, arriving through a triangle).
            At w=(0.4,0.3,0.3): (170,149,149) correct vs (102,77,77) naive.
            HONEST EXCEPTION: an artist's UI-gradient swatches may genuinely mean the
            encoded blend. Our vertex colours become LIGHTING RESULTS in 3.6, and a
            quantity of light is averaged as light. Ask what the number measures.
  stepping-precision: integer accumulators are BIT-EXACT against direct evaluation
            (worst difference 0 over 4000 steps). Float-stepped weights drift only
            4.94e-6 over 4000 adds = 0.0013 of an 8-bit colour level = 0.020 texels of a
            4096 texture; carried across 900 rows without a reset, 4.26e-5 = 0.011 levels.
            SO DO NOT OVERSELL IT: the case for integers is exactness, reproducibility
            and zero cost — NOT a visible artifact. Overselling a real principle with a
            fake symptom teaches students to distrust the principle. Where it WILL matter
            is z-buffer comparisons against tiny differences (3.1).
  linear-algebra: LINEAR means T(a+b)=T(a)+T(b) AND T(ca)=cT(a). Consequence:
        T(v) = x*T(i) + y*T(j), so TWO ARROWS DETERMINE EVERYTHING — not
        approximately, exactly. Grid picture: straight stays straight, parallel
        stays parallel and evenly spaced, and THE ORIGIN NEVER MOVES (put c=0 in
        the scaling rule). That last one is a one-line proof that NO 2x2 CAN
        TRANSLATE. Left open ON PURPOSE — 2.7 earns the fourth component from it,
        and the payoff dies if the gap is closed early.
        ROTATION IS DERIVED, never looked up: i -> (cos,sin) because that is what
        sin/cos MEAN; j is a quarter turn ahead so j -> (-sin,cos). The minus sign
        lands on c1.x, the top-RIGHT element as written.
        R(a)*R(b) == R(a+b) VERIFIED over 1716 pairs (worst 2.98e-7) — and
        multiplying it out DERIVES the angle-addition formulas. Spot-checked:
        top-left of R(.6)*R(.9) == cos(1.5) == 0.070737.
        DETERMINANT = signed area factor = edge_function with its first point at
        the ORIGIN = the 2-D cross product. VERIFIED identical over 28,561 integer
        matrices, 0 mismatches. det>0 orientation kept; det<0 FLIPPED, so a
        negative determinant turns every front face into a back face (3.4) —
        verified: the standard triangle's doubled signed area goes +60 -> -60
        under scale(-1,1), and a det of 1.0200 takes +60 -> +61.2, ratio exactly
        1.0200. det==0 folds the plane onto a LINE: no inverse, and information is
        genuinely gone (verified — (1,0) and (-1,1) both map to (2,1), and all
        1681 sampled inputs land on the single line x = 2y).
        det IS MULTIPLICATIVE. Shear has det 1 — it slants without changing area.
        MEASURED against our own rasterizer (140-px square, fill + count pixels):
        identity/scale/shear EXACT, rotation -0.26%, rot*scale -0.06%. The
        residual is the fill rule counting pixel CENTRES, so it scales with
        PERIMETER and falls as ~1/side (demo at 44 px sees ~1%). Axis-aligned is
        exact at any size — the top-left rule paying off somewhere unexpected.
        ORDER MATTERS: R*S and S*R map (1,0) to (0,2) vs (0,1) — but BOTH have
        det 2. Same area factor, different shape; the determinant is a summary and
        summaries lose information. Uniform scale DOES commute with rotation.
        Composition is ALWAYS associative even though it never commutes.
        transpose == inverse ONLY for a rotation (orthonormal). VERIFIED false for
        scale(2,0.5). Using transpose as a cheap inverse is the worst kind of wrong
        — it looks almost right. 3.6 meets this properly with normals.
        --- 3-D (Lesson 2.6) ---
        EVERY ARGUMENT ABOVE SURVIVES UNCHANGED, because none of them counted the
        axes. Three basis vectors, three columns; the proofs are identical.
        ROTATION NOW NEEDS AN AXIS. Each axis rotation is the 2-D rotation acting
        in the plane of the OTHER TWO, in the order given by the cycle
        x -> y -> z -> x, with its own column left alone.
        Ry's MINUS SIGN IS BELOW THE DIAGONAL, mirrored relative to Rx and Rz,
        because the cycle wraps (z -> x) while the matrix lists x's row above z's.
        THE #1 SIGN ERROR IN GRAPHICS — derive from the cycle, never recall the
        shape. Test: rotation_y(t) * (0,0,1) == (sin t, 0, cos t).
        VERIFIED: rotation_z's top-left 2x2 == mat2's rotation over 121 angles, 0
        mismatches; each rotation fixes its own axis, has det 1, preserves length,
        and transpose == inverse.
        ROTATIONS ABOUT DIFFERENT AXES DO NOT COMMUTE — new in 3-D; in 2-D any two
        rotations always did. Same-axis rotations still add. SHARPEST DEMO: pick a
        point ON one of the axes so one rotation provably does nothing.
        (1,0,0) with Rx(0.6), Ry(0.8): x-first -> (0.6967, 0.0000, -0.7174);
        y-first -> (0.6967, 0.4050, -0.5921). Seed of gimbal lock (Module 7).
        det becomes a VOLUME factor; its sign is HANDEDNESS, so det<0 renders a
        model inside-out once 3.4's culling exists. MEASURED by counting lattice
        points inside the transformed unit cube (120 steps/unit, inside iff
        inverse(M)*p in [0,1)^3): identity 0.00%, rotation_y(0.7) -0.01%,
        scale(1.6,.8,1.3) 0.00%, Rx*Ry*scale -0.00%.
        THE 4x4's FOURTH COLUMN IS INERT while w = 0. A correct translation written
        into c3 moves a point by EXACTLY (0,0,0) — measured, not approximated.
        DIAGNOSIS (2.7's opening): a matrix can only scale each column by a
        component of the input and add them up, so adding a CONSTANT needs a
        component that is always the same number. DO NOT PRE-EMPT THIS.
  shaders: HLSL -> SDL_shadercross (3.0.0-preview) -> SPIR-V/DXIL/MSL  [Module 4+]
  cpp: C++20, no exceptions/RTTI in core, snake_case, private members trailing _,
       .hpp + #pragma once, [[nodiscard]], -Wall -Wextra // /W4, all warnings fixed
  build: CMake >= 3.24, out-of-source (build/), 64-bit; two phases (configure, build);
         Debug build = -DCMAKE_BUILD_TYPE=Debug (adds -g); sources listed explicitly
         (never file(GLOB)); target_include_directories(engine PRIVATE src)
  state-block: THERE IS ONE STATE BLOCK AND IT IS THIS FILE. Lesson pages END AT
         FURTHER READING — no <details class="state">, no per-page copy. CLAUDE.md
         §9 originally said "end every lesson with a STATE block" because STATE.md
         DID NOT EXIST YET: 0.1 shipped 2026-07-16 with an in-page block, STATE.md
         arrived at 0.6 (cba92f1, 2026-07-17) and took over the resume-key job, and
         the per-lesson rule was never retired. Both then ran for 46 more lessons.
         BY 5.7 THE COST WAS 3.97 MB = 22.2% OF docs/lessons/ — worse than the 18%
         CSS duplication that forced the docs-tooling change — and 5.7's own block
         was 331 KB of a 550 KB page (60%) and BYTE-IDENTICAL to this file's block,
         4,433 lines each. Stripped 2026-09-04: 53 files, 54,907 lines, PURE
         DELETION (0 lines added).
         WHY NOTHING WAS LOST: the standing rule was already "only the newest
         lesson's STATE block tracks reality", so 51 of 52 pages carried a snapshot
         that was STALE BY DESIGN. The reader-facing half of the job is §6's
         Recap & Next, which all 52 lessons have.
         THE GENERAL LESSON, and it is the second time this exact shape has bitten:
         A RULE WRITTEN BEFORE ITS REPLACEMENT EXISTED DOES NOT RETIRE ITSELF. When
         a new artifact takes over an old rule's job, AMEND THE RULE IN THE SAME
         BREATH — §6 item 13, §9 and §11 all still demanded the block. Amended now.
         Also removed: the three dead `.state` rules in course.css, the template's
         SECTION 12 banner, and two checklist lines that asked for the block.
  docs-tooling: the shared CSS and page script are LINKED, not duplicated — ONE copy each,
         at docs/shared/course.css and docs/shared/course.js. Edit those files
         directly; every page picks the change up immediately. (Until the CSS
         extraction they were duplicated into all 36 pages = 18% of docs/; that is
         how the script drifted into 6 versions before 1.2. Cause removed.)
         Still no build step: relative <link>/<script src> resolve off the
         filesystem, so docs/index.html opens by double-clicking, offline —
         verified in Chromium, Firefox AND WebKit including ../shared/ from
         docs/lessons/. Cost: a lesson file is NOT portable alone; the docs/ tree is.
         Two marker regions remain — <!-- SHARED-CSS:BEGIN/END --> and
         <!-- SHARED-SCRIPT:BEGIN/END --> — but they now hold the LINK TAGS, and
         docs/_template/apply-shared.py computes each page's relative prefix
         ("" at docs/, "../" at docs/lessons/) and verifies it. A wrong prefix is
         SILENT: unstyled inert page, no error. It breaks by MOVING a page, not
         editing one, so run the tool after adding/moving pages.
         The KaTeX loader stays INLINE inside SHARED-SCRIPT (SRI + inline onload
         cannot move into course.js; and below the END marker it was never
         propagated — 6 lessons shipped with no maths renderer).
         Page-specific JS/CSS goes OUTSIDE the markers (1.2's key-state widget;
         index.html + math-toolbox.html's own <style>).
         Highlighter word lists: kw is checked before ty, so fundamental types
         (bool/char/int/Uint32/...) belong in CPP_TYPES only. Shell `::` comments
         are anchored to line start (SDL3::SDL3 must not read as a comment).
         `apply-shared.py --check` exits 1 on drift, 2 if a shared file is missing
         or empty, and also lints inline fill= on SVG <text>. Run before committing.
  docs-verify: serve docs/ over HTTP and drive REAL Chromium (Playwright). The
         preview pane reports impossible computed styles — it will show a dead
         highlighter or broken theme toggle as fine. Strongest highlighter check:
         live textContent after highlighting == DOMParser parse of the same file.
         Use getBoundingClientRect(), NOT getBBox(), for spill/collision checks —
         getBBox is in LOCAL coords, so anything inside a <g transform> is compared
         against the wrong origin (three false positives in 1.8).
         Verify a renderer by its POSITIVE signal: .katex count == .eq count, and
         exactly two script[src*=katex] tags. "No console errors" passed for six
         lessons while KaTeX was entirely absent.
         Listings are SPLICED from the real files (@@LISTING:path@@ + a small script),
         never retyped — makes drift from the compiled source impossible.
  katex-trap: the KaTeX loader used to sit just BELOW <!-- SHARED-SCRIPT:END --> in the
         template, so apply-shared.py never propagated it and every lesson shipped
         without a maths renderer. Invisible because raw TeX is also the documented
         CDN-unreachable fallback, and 1.1-1.7 have zero .eq blocks so nothing failed.
         FIXED in 1.8: moved inside the region, duplicate standalone copies removed
         from conventions.html and math-toolbox.html. ANYTHING every page needs goes
         BETWEEN the markers.

  textures: A TEXEL IS A SAMPLE, NOT A SQUARE — its value lives at (i + 0.5)/N. Settled
        in 3.9, and every rule below follows from that one sentence.
        ORIGIN TOP-LEFT, v DOWNWARDS. Not a preference: SDL3/SDL_gpu.h, "Coordinate
        System" — "Texture Coordinates: The top-left corner has an x,y coordinate of
        (0, 0) and extends to the bottom-right corner at (1.0, 1.0). +Y is down."
        Agrees with the framebuffer (1.5) and the viewport flip (2.11).
        TEXEL SPACE = u*N. Integers are texel BOUNDARIES, i+0.5 is the CENTRE. Two
        questions, two answers: "which texel contains this point" is floor(u*N) and the
        half plays NO part; "which two centres bracket it" needs u*N - 0.5.
        THE HALF-TEXEL ERROR IS INVISIBLE TO NEAREST. 0 of 160801 samples change without
        the offset; 160632 do under bilinear. So a pipeline carries it for years looking
        sharp and reveals it the day filtering is switched on — and the FILTER gets
        blamed. MEASURED: bilinear at a texel centre is that texel EXACTLY (worst error
        0.000000000; 0.625 without). The shift is exactly -0.5000 texels.
        1:1 IS THE IDENTITY and it is the strongest single check: 0 of 4096 texels differ
        under BOTH filters, control 1779. Rests on the sRGB u8 round trip being exact
        (measured: 0 of 256 values change).
        THE HALF TEXEL EXISTS AT BOTH ENDS. This rasterizer samples attributes at INTEGER
        pixel coordinates, so pixel i's sample point is i while a texel's value is at
        (i+0.5)/N — a 1:1 quad's uvs run 0.5/N .. 1 + 0.5/N. Same family as 2.11's
        pixel-centre question; a blit is exact only when both ends agree.
        STORAGE: Uint32 ARGB8888, sRGB-ENCODED, row 0 at the TOP. Byte-identical to a
        framebuffer pixel, so a 1:1 draw is a COPY rather than a conversion.
        DECODE INSIDE THE SAMPLER, BEFORE THE FILTER. sample() returns linear_rgb, so the
        order cannot be got wrong at a call site. Filtering encoded bytes: black+white
        gives 0.2139 where 0.5 is wanted — 42.8% of the light; worst pair over all 65536
        is (0,255), off by 0.286. THIS IS WHAT *_SRGB TEXTURE FORMATS DO, in hardware.
        FILTER: nearest | linear, matching SDL_GPUFilter's order. ONE field where SDL has
        three (min/mag/mipmap) — without mipmaps there is nothing different for a
        minification filter to do, and that ABSENCE is the aliasing below.
        ADDRESSING: repeat | mirrored_repeat | clamp_to_edge, PER AXIS, matching
        SDL_GPUSamplerAddressMode's order. Per axis because a road strip repeats along its
        length and clamps across its width.
        `i % n` IS NOT REPEAT — C++ truncates toward zero, so -1 % 4 = -1. Fix:
        m = i % n; if (m < 0) m += n. Adding n ONCE suffices. Getting it wrong paints a
        one-texel stripe at exactly the boundary tiling was meant to cross.
        MIRRORING IS ONE IDENTITY: index -1-k maps where index k does. That FORCES the
        doubled edge texel rather than making it a choice. Period 2n.
        THE FOUR TEXEL INDICES ARE ADDRESSED INDEPENDENTLY, after the neighbours are
        chosen. Wrap the COORDINATE first and a repeating texture blends the last texel
        with itself — a hairline seam at every tile boundary, in the FILTERED MODE ONLY.
        MEASURED seamless: 0.0000000 across both u = 0 and u = 1.
        BILINEAR = TWO LERPS ALONG u, ONE ALONG v. The four weights are (1-tu)(1-tv),
        tu(1-tv), (1-tu)tv, tu*tv. ORDER OF THE AXES DOES NOT MATTER (5.96e-08 over
        200000 samples) and THE WEIGHTS SUM TO 1, so it is an average and cannot brighten
        or darken — filtering a CONSTANT image returns the constant exactly (0.000e+00
        over 20000 samples), which is the test a busy image hides.
        THIRD APPEARANCE of "weighted average, weights sum to one" (barycentric 2.3,
        Gouraud 2.4, this). ONE REAL DIFFERENCE: barycentric weights are AFFINE; bilinear's
        contain tu*tv and are not, so bilinear is only PIECEWISE smooth — the derivative
        jumps at every texel boundary, which is the diamond quilt, and what bicubic fixes.
        TEXTURE x LIGHT IS A CONSEQUENCE, NOT A CONVENTION. An albedo is a REFLECTANCE and
        a reflectance multiplies a quantity of light, so both must be linear. MEASURED per
        pixel over 32301 covered pixels: lit == albedo * light, 0 off by more than 0.02,
        worst 0.0076 = two units of 8-bit quantisation.
        MINIFICATION IS NOT SOLVED. Bilinear reads FOUR texels however many the pixel
        covers. Traced footprint on the floor: 0.60 texels/px at the bottom of the frame,
        62.46 two rows below the horizon; the curves cross at row ~103 and the sampler
        sees 6.4% of the footprint at the horizon.
        ALIASING IS NOT CAUSED BY A BIG FOOTPRINT, it is caused by a big footprint
        CONTAINING DISAGREEMENT. All four test images are 64 texels wide so the footprint
        is identical; sub-pixel-nudge sparkle still runs 8.0% / 16.2% / 32.6% / 64.8% for
        4 / 8 / 16 / 32 cells. Mipmaps are Module 6.

  measurement: THREE INSTRUMENTS, THREE QUESTIONS (3.10). BUDGET = which phase costs
        what (coarse, always on, in the engine). DIFFERENTIAL = what one line costs
        (offline, by SUBTRACTION, with a control). COUNTER = how much work there is
        (free, exactly reproducible, and the only one that EXPLAINS the other two).
        THE CLOCK HAS TWO PROPERTIES AND THE SLOWER ONE IS NOT THE ONE YOU GUESS.
        Measured (M4 Pro): SDL_GetPerformanceFrequency = 24 MHz, so the counter TICKS
        every 41.667 ns, while one READ costs 5.42 ns and a whole scope_timer 13.46.
        Reading the clock is ~8x faster than the clock changes. So one encode (9.3 ns)
        cannot be timed at all: 4.5 fit in a tick, and a single measurement reports
        0.00 or 41.67 and never 9.3.
        TIME A BATCH AND DIVIDE. N=1 spans 0..16 ticks; N=1000 spans ~230 and twelve
        trials agree to 0.12 ns. THE RULE: never instrument anything shorter than
        100 * max(tick, timer) = 4.17 us here. profiler exposes resolution_ns() and
        overhead_ns() so the number is DERIVED on the machine being measured.
        A ZONE THAT READS ZERO is either work you are not doing or work you cannot
        measure, and the two look identical. `build` reads 0.00 for the second reason
        and is KEPT, as the rule appearing in the engine's own output.
        OBSERVER EFFECT, MEASURED: a scope_timer per iteration takes 9.71 -> 18.08
        ns/iter (1.86x) AND UNDER-REPORTS, because the closing clock read is inside the
        interval it closes. Both errors at once, in opposite directions. The fix is not
        a finer clock, it is SUBTRACTION.
        STATISTICS: timing noise is ONE-SIDED (the machine can be slower than your code,
        never faster), so the mean is wrong for both cases. MINIMUM for a kernel (one
        true cost, everything above is interference); MEDIAN for a frame (no single true
        cost; one 40 ms hitch moves a mean of 120 by 0.3 ms and the median by nothing).
        SAY WHICH ONE YOU USED — a number quoted without it is a rumour.
        BENCHMARK HYGIENE: volatile sink or the compiler deletes the work; -O2 always
        (1.5 measured 5.1x at -O0 vs 14.8x at -O2, so a debug profile ranks a DIFFERENT
        PROGRAM); both variants in the SAME RUN so thermal/scheduling state cannot drift.
        A DIFFERENTIAL BENCHMARK NEEDS A CONTROL. 3.10 first reported a texture fetch at
        6.40x its microbenchmark cost — produced by adding an encode and two stores
        alongside the fetch and attributing all three to it. With the control: 2.27x.
        WHEN A MEASURED RATIO IS BIGGER THAN YOU CAN EXPLAIN, the two sides differ in
        more than one way. (3.9 §5.1 was the first sighting; this is the second.)
  frame-budget: SIX DISJOINT ZONES THAT COVER THE FRAME — build / collect / sort / fill
        / overlay / present, in the order they happen. DISJOINT IS ARITHMETIC, not
        tidiness: two live timers count the same nanoseconds twice (zones_overlapped()).
        THE UNMEASURED REMAINDER IS THE MOST IMPORTANT ROW, and the one everybody omits:
        six honest bars summing to 60% of a frame look complete until you double the
        biggest and gain 9%. Ours is 0.2% BECAUSE IT IS DISPLAYED. Drawn to the END of
        the stacked bar in a grey that deliberately does not look like a phase.
        A SUM OF MEDIANS IS NOT THE MEDIAN OF A SUM; the disagreement is left visible
        rather than derived away.
        FRAME STARTS BEFORE THE EVENT DRAIN (draining is work) and ENDS BEFORE
        SDL_RenderPresent (which blocks on vsync). The HUD prints engine AND wall time,
        because WITH VSYNC ON AN FPS COUNTER CANNOT TELL YOU THAT YOU MADE ANYTHING
        FASTER.
        NOT EVERY PHASE IS A LEXICAL SCOPE — the HUD uses an explicit now_ticks()/add()
        pair, which is why profiler::add is public. A zone is a CATEGORY of work, not a
        point in time; `overlay` accumulates from two places in the frame.
        INSTRUMENTATION SHAPES THE CODE IT MEASURES. sort_back_to_front left
        draw_triangles so the sort could be timed apart from the fill; the harness stages
        all triangles before filling so collect and fill cannot interleave. Say so.
        MEASURED, 320x180: build 0.00, collect 53.08, sort 0.00, fill 1547.29 (96.1%),
        overlay 4.08, present 3.25, other 3.17, FRAME 1610.88 us (621 fps).
        MEASURED, 1280x720: fill 23243.92 (99.2%), collect 55.04, FRAME 23423.62 (43 fps).
  px-per-triangle: THE AXIS. Work happens at two frequencies — per-VERTEX (triangles)
        and per-PIXEL (covered pixels) — and their ratio is one number.
        THE PREDICTION EVERYONE GETS WRONG: the 2-triangle floor costs 1390.14 us and
        the 2304-triangle torus 171.78. THE FLOOR IS 8x MORE EXPENSIVE WITH 1152x FEWER
        TRIANGLES. ns/tri 695069 vs 74.6; ns/px 45.01 vs 64.14 (1.42x, and that residue
        is real — tiny triangles pay fill_triangle's setup against ~1.2 px).
        TWO SLOPES, MEASURED. 16x the pixels: collect 53.08 -> 55.04 (noise), fill
        1.55 -> 23.24 ms. Same screen size, 36 -> 36864 triangles: collect 1.15 ->
        967.62 us (840x), fill only 338 -> 1070. THE TWO LINES CROSS near 1 px/triangle,
        and "optimise the fill" is right on one side and wrong on the other with NOTHING
        about the renderer changed.
        QUOTE ns/PX AND ns/TRIANGLE, NEVER ms/frame: 45.47 ns/covered px, 23.0 ns/tri.
        Total milliseconds is a fact about one scene at one resolution on one machine.
  amdahl: speedup = 1/((1-p) + p/s); s -> infinity gives 1/(1-p) and no more. USE IT
        TWICE — BEFORE as a filter (compute the ceiling, decide whether to start), AFTER
        as a check ON YOURSELF (p, s and the whole-frame speedup are NOT independent, so
        a frame that beat the formula means you mismeasured). VERIFIED: ceiling 1.284x,
        measured 1.287x. Our fill is p = 0.96, ceiling 25x.
  fill-anatomy: ns PER COVERED PIXEL, 2 triangles over 272223 px at 960x540:
        coverage+colour 2.682 · +perspective divide 2.664 (-0.018, FREE) · +depth test
        2.936 (+0.272) · +sRGB encode 9.024 (+6.088) · textured nearest 18.186 ·
        bilinear 25.076 · lit 33.048 · lit+textured 46.456 · lit+textured FAST 35.212.
        THE ENCODE ALONE IS 2.1x COVERAGE + DIVIDE + DEPTH COMBINED. 3.2 warned its
        per-pixel divide was "the honest cost of this lesson"; the measurement says the
        warning was unnecessary — it hides entirely behind other latency.
        COST DOES NOT DISTRIBUTE ITSELF ACCORDING TO HOW MUCH YOU THOUGHT ABOUT
        SOMETHING: to_encoded has been an unremarked one-liner since 2.4.
  srgb-encode: FOUR CANDIDATES (ns/call, speedup, UN-ROUNDED error in 8-bit codes,
        % differing, round-trip failures of 256):
          exact std::pow             3.497  1.00x       —     0.00%  0/256
          FITTED SQRT CHAIN          1.856  1.88x  0.0115     0.60%  0/256  <- SHIPS
          threshold table + bsearch  6.919  0.51x   exact     0.00%  0/256
          uniform input table 4096   0.994  3.52x  0.4022     3.66%  0/256
        THE EXACT TABLE IS SLOWER THAN pow — 8 dependent L1 loads is a longer latency
        chain than a modern powf. A beautiful idea, timed and rejected.
        JUDGE THE ERROR BEFORE ROUNDING. "Worst code" SATURATES: everything under half a
        code reports 1, so it cannot separate 0.0115 from 0.4022, which is the whole
        question. At a 16-bit target those become 3.0 and 103.4 — and Module 6 stops
        being 8-bit, so only one of the two survives it.
        THE CACHE ARGUMENT AGAINST THE TABLE IS FALSE ON THIS MACHINE, MEASURED: with a
        framebuffer-sized stream alongside, all four read 0.98-1.04x. KEPT WITH ITS
        RESULT rather than dropped — shipping the right decision with the reasoning that
        failed is how folklore is manufactured.
        THE FIT: toe kept EXACT (12.92*x below 0.0031308 — one multiply, and every basis
        function has infinite slope at 0 where the truth has 12.92, so fitting the toe
        costs 3 codes instead of 0.0115). Above it a*sqrt(x) + b*x^1/4 + c*x^1/8 + d*x,
        three CHAINED sqrts (latency chain of ~3, not a throughput win of 3).
        Coefficients FITTED in scratch/fit_srgb.py — least squares reweighted toward
        minimax, constrained to sum to 1 so white is exact. Worst |err| 0.0000451; the
        error OSCILLATES about zero (a wobble, not a bias).
        THE NaN GUARD AND CLAMP ARE REPEATED DELIBERATELY: they are not part of the
        approximation, they are what "encode a float into a byte" means. sqrt of a
        negative is a NaN too (3.3's undefined cast).
        encode_mode {exact, fast} IS THE FIRST KNOB IN THIS ENGINE THAT IS NEITHER RIGHT
        NOR WRONG — blend_space::encoded / interpolation::affine / draw_line_naive are
        kept so a MISTAKE can be summoned; this is a defensible speed/accuracy point.
        fill_style::encode DEFAULTS TO exact, a decision about the COURSE not about
        renderers: every measured claim in 3.1-3.9 was made against it, and a default
        moving 0.60% of those pixels by one code would falsify nine lessons. The DEMO
        selects fast. (Same bargain as inv_w=1 in 3.2, lights=nullptr in 3.8, an unbound
        albedo in 3.9.)
        RESULT: fill 1.30x, FRAME 1.29x (1.29x/1.28x at 720p), costing 2657 of 473199
        shaded px differing by exactly 1 code (0.56%).
  cache: MEASURE THE CLIFF, DO NOT ASSERT IT. 2^21 random samples, count and pattern
        held fixed: 32x32 through 1024x1024 all read 2.25-2.38 ns/fetch and only
        2048x2048 (16 MiB) jumps to 3.88 (1.72x). THE CLIFF IS NOT WHERE THE FOLKLORE
        PUTS IT — this machine's L2 makes a few MiB free, so "the texture must fit in
        L1" is advice for a different computer. RANDOM, NOT SEQUENTIAL: a sequential
        walk is prefetched perfectly and measures the prefetcher.
        A FUNCTION'S COST IS NOT A PROPERTY OF THE FUNCTION. The same 64x64 nearest
        fetch costs 2.26 ns in a tight loop and 5.13 ns in situ (2.27x), isolated with a
        control that keeps the encode and both stores and removes only the fetch. In a
        tight loop consecutive fetches overlap; in situ each sits in a dependency chain
        (uv from the divide, result into the encode) with nothing to overlap with.

  gpu-model: A GPU IS NOT A FAST CPU. Per lane it is SLOWER — lower clock, shorter
        pipeline, no branch predictor worth the name, far less cache. It finishes
        first for three structural reasons, all measurable on a CPU:
        (1) WIDE, NOT FAST. A dependent chain runs at the LATENCY of its operation
        however many units are idle: measured 3.124 ns/step for one sqrt chain,
        0.443 for eight interleaved (7.05x), 0.112 for 32 (27.88x). Same
        arithmetic in every row; only the amount of INDEPENDENT work changed.
        That is the whole architecture, and it is why a shader using fewer
        registers runs faster — more groups resident, more latency hidden.
        OCCUPANCY IS THAT TABLE.
        NAME THE SECOND MECHANISM: past ~8 chains the compiler also vectorises, so
        the last rows combine interleaving WITH SIMD. Those are the two things a
        GPU combines, but they are two, and saying so is the difference between a
        demonstration and a conjuring trick.
        (2) LOCKSTEP. Lanes are grouped (32 = a warp/wavefront; 32 or 64 on AMD)
        and share one program counter. One decoder and one scheduler for 32 lanes
        is what buys the width.
        (3) 2x2 QUADS. See below.
  divergence: A WARP RUNS BOTH SIDES OF A BRANCH ITS LANES DISAGREE ABOUT, with the
        inactive lanes MASKED. Measured against a warp that never diverges:
        1.00x at 1024 px of coherence, 1.03x at 64, 1.52x at 16, 1.93x at 8 and
        below. THE STEP IS EXACTLY AT THE WARP WIDTH, which is what makes the model
        right rather than merely plausible.
        REPORT `vs coherent`, NOT `vs CPU`. The CPU comparison is two different
        loops and carries the emulation's own overhead; the first draft published
        5.0x that way, and the controlled figure is 1.9x.
        COHERENCE IS A PROPERTY OF YOUR DATA, NOT YOUR CODE. The same shader is
        fast or slow depending on how the branch is arranged across the SCREEN —
        which is why "sort by material" and "use separate pipelines" are real
        advice, and why a branch benchmarked on a test scene can cost 30% in a
        real one.
  quads: FRAGMENTS ARE SHADED IN ALIGNED 2x2 BLOCKS, ALWAYS. Lanes the triangle
        misses are HELPER LANES: they run the fragment shader and their results are
        discarded.
        NOT WASTE TO BE ENGINEERED AWAY — it is where ddx/ddy come from. ddx is
        lane 1 minus lane 0, ddy is lane 2 minus lane 0, so a lane needs its
        neighbours to have run the same code. EVERY AUTOMATIC MIP SELECTION ON
        EARTH IS PAID FOR BY HELPER LANES (and 3.9's minification gap is what they
        buy). Consequence: a derivative inside a divergent branch is undefined,
        because the neighbouring lane has no value at that point in the program.
        ALIGNED TO EVEN COORDINATES IN THE RENDER TARGET, not to the triangle — so
        a triangle cannot arrange to be cheap by being positioned well, and the
        waste depends on its SIZE, not its placement.
        LANE EFFICIENCY ~ A/(A + cP): covered lanes live in the AREA, helpers along
        the PERIMETER. MEASURED over 32 rotations, circumradius -> efficiency:
        64 px 96.3% · 32 px 93.0% · 16 px 86.7% · 8 px 78.5% · 4 px 67.8% ·
        2 px 41.5% · 1 px 25.0%. The r/(r+4) model is optimistic by a few points
        throughout but gets the shape right.
        OUR NUMBERS UNDERSTATE IT: vertex::x/y are INTEGERS, so a sub-pixel
        triangle rounds away and draws nothing. Real hardware rasterizes at ~1/256
        px, so it draws them, at efficiencies below 25%.
        WHY 2x2 AND NOT WIDER, measured at r = 8: 2x2 75.5% · 4x4 48.7% ·
        8x8 30.4% · 16x16 8.9%. 2x2 is the SMALLEST block that can produce a
        screen-space derivative (one neighbour in x, one in y) and every wider one
        wastes more — forced from both ends. A 32-lane warp is EIGHT QUADS, possibly
        from eight different triangles: the grouping for SCHEDULING and the grouping
        for DERIVATIVES are different things, and only the second is 2x2.
        THE COST, MEASURED on the capstone scene: 87.4% efficient, and the quad
        traversal is 1.04x - 1.38x slower than scanline as tessellation rises from
        66 to 36,866 triangles. 1/efficiency PREDICTS every row to within 0.09x;
        where measurement exceeds prediction the gap is the quad walk's own
        overhead (four coverage tests and a mask per block), not the shading.
        THIS IS WHAT "SMALL TRIANGLES ARE EXPENSIVE" ACTUALLY MEANS, and it is a
        statement about SIZE IN PIXELS, not about count (3.10's floor-vs-torus).
  traversal: fill_style::traverse {scanline, quad, quad_debug}, defaulting to
        SCANLINE — what a CPU rasterizer should do, and what every measurement
        before 4.1 was taken against. `quad` is an INSTRUMENT, not a feature: it is
        here to be measured, not used.
        IT MUST CHANGE NOTHING IT WRITES. Verified bit-identical in BOTH colour and
        depth over 2,306 triangles at 640x360 — 0 of 230,400 px, 0 of 230,400
        depths — while shading 21,005 lanes whose results went nowhere. The line
        that guarantees it: THE DEPTH TEST IS FOR COVERED LANES ONLY, because a
        helper lane is not on the surface.
        ONE EARLY-OUT, the one hardware has: a quad with no covered lane is never
        issued. Without it the cost would be the bounding box, not the triangle.
        THE FRAGMENT IS NOW A FUNCTION (a lambda from three barycentric weights to
        a colour, with pipeline state captured) — WHICH IS A FRAGMENT SHADER, and
        has been since 3.6. The only thing separating it from one is that the
        CALLER CANNOT SUPPLY IT. It must now be TOTAL, because it runs for lanes
        outside the triangle: negative barycentrics (2.3 called that useful), a
        w_recip that can be enormous or negative, a uv anywhere at all. Nothing
        reads out of bounds — wrap_texel folds any index (3.9), linear_to_srgb_u8
        refuses a NaN (3.3) — and both guards were written for other reasons.
        quad_stats is an OUT-PARAMETER, not a fill_style field: a pipeline object
        describes how to draw, and this describes what happened. It ACCUMULATES,
        because a per-triangle lane efficiency is not a number anybody wants.
  pipeline-object: RENDER STATE IS AN IMMUTABLE OBJECT, and we have had one since
        3.2 without the name. fill_style has 10 top-level fields;
        SDL_GPUGraphicsPipelineCreateInfo has 9 (53 once its nested state structs
        are expanded). THEY ARE THE SAME OBJECT — deliberately, since 3.2/3.4/3.9
        mirrored SDL's field names and enumerator orders.
        THE FOLK EXPLANATION IS WRONG, AND IT WAS MEASURED. "State is baked in so
        the inner loop need not branch on it" — our loop branches per pixel on
        draw-constant state, and hoisting that branch is worth 0.93x, i.e. nothing.
        A perfectly predicted branch is free.
        THE REAL REASON is that the driver must VALIDATE the combination and
        COMPILE a shader specialised to it — milliseconds, ruinous per draw and
        free once. SDL's own header calls pipelines "precalculated rendering
        state", under things "created once and used over and over".
        OUR fill_style ALREADY HAS 96 COMBINATIONS (4 shading x 2 encode x 2
        interp x 2 blend space x 3 traversal), every one decided at runtime, per
        pixel. A GPU compiles the one you asked for. THAT IS WHAT A SHADER IS.
  pipeline-stages: ELEVEN, AND THE STUDENT HAS WRITTEN ALL ELEVEN.
        vertex shader -> collect_triangles (2.8-2.10) · primitive assembly ->
        index triples (2.12) · clipping -> clip_polygon_near (3.3) · perspective
        divide -> /w (2.10) · viewport -> viewport::to_screen (2.11) · face culling
        -> is_front_facing (3.4) · rasterization -> edge functions + quads (2.2,
        4.1) · interpolation -> barycentric x 1/w (2.4, 3.2) · depth test ->
        depth_buffer (3.1) · fragment shader -> the fragment lambda (3.6-3.9) ·
        blend/write -> row[x] = colour (1.5).
        MODULE 4 DOES NOT TEACH A PIPELINE. It hands yours to hardware, and the
        port is a rename because of the NDC-parity decision taken in Module 2.
  measurement: A REFACTOR'S PERFORMANCE CLAIM NEEDS THE SAME CONTROL AS A
        FEATURE'S. Extracting the fragment into a lambda appeared to gain 10%
        (46.46 -> 40.46 ns/px) against 3.10's published figure. Two binaries
        differing ONLY by the extraction, run alternately in one session: minimums
        say 2.5% faster, medians say 1% slower — i.e. NO MEASURABLE DIFFERENCE. The
        10% was session drift, visible in the data as both columns climbing
        monotonically as the machine warmed.
        THIS BROKE 3.10's OWN PITFALL ("never compare two numbers taken hours
        apart") WITHIN A DAY, on the code that lesson was written about. 3.10's
        published numbers STAND; nothing needed restating.

curriculum: 107 lessons, ~510 h, 10 modules   (reshaped 2026-09-08 — see `roadmap:`)
  M0:6  M1:8  M2:12  M3:10  M4:9  M5:12  M6:18  M7:8  M8:13  M9:11
  (M8 IS PHYSICS, NEW. M9 is the old M8, Professional Polish & Capstone.
   M5:10 in the previous line of this file was ALREADY STALE — 5.10 split
   into 5.10+5.11 and the count was never bumped, which is the same class of
   drift check-curriculum.py now catches in docs/index.html.)

  the-boundary: THE ENGINE IS A STATIC LIBRARY WITH AN OUTSIDE, and the outside is
        enforced by the INCLUDE PATH, not by the style guide.
          target_include_directories(engine PUBLIC include PRIVATE src)
        engine/include/ contains exactly one directory, `engine`, so from outside
        the only spellings that resolve are <engine/gfx/raster.hpp> and siblings.
        `#include "gfx/raster.hpp"` — the spelling every file used through Module 4
        — DOES NOT COMPILE from a demo. Verified: 'gfx/raster.hpp' file not found.
        DEMOS AND THE CAPSTONE MAY USE ONLY PUBLIC HEADERS. This is law from here
        to the end of the course (CLAUDE.md §8).
        `engine::engine` is the ALIAS to link; a name with :: in it cannot be
        mistaken for a file, so a typo fails at configure time instead of becoming
        `-lengine` at link time.
        stb_image is PRIVATE and STOPS AT THE BOUNDARY — one TU (image.cpp)
        includes it, image.hpp exposes engine::image, and a demo reaching for
        stb_image.h is refused. SDL3::SDL3 is PUBLIC and that is an ADMISSION, not
        a choice: framebuffer.hpp says Uint32, gpu_device.hpp takes SDL_Window*.
        An API that names another library's types has adopted its vocabulary.
        add_subdirectory(engine) BEFORE add_subdirectory(demos): swap them and the
        configure fails, which is a build system enforcing an architecture rule.
  engine-vs-demo: THE RULE THAT DECIDES EVERY FILE. It belongs to the ENGINE if a
        different game, one we have not written, would want it. It belongs to a
        DEMO if it exists to show, teach or drive this particular program.
        Applied: collect_triangles/draw_triangles/scene_object/debug draw ->
        engine. build_scene, orbit_camera, the floor, the textures, pong ->
        demos/common. next_cull()/next_eval()/is_degenerate/HUD labels -> the
        sandbox, BECAUSE THEY ARE ABOUT A KEYBOARD.
        demos/common/ is a static library so several programs can share content
        instead of copying it. THE RULE THAT KEEPS IT HONEST: nothing in it may be
        needed by a shipped game; the moment something is, it is an engine feature
        nobody has named yet.
  public-api-design: FOUR RULES, each with a receipt in 5.1.
        (a) the CALLER's vocabulary, not the implementation's;
        (b) state that always travels together travels as ONE THING — fill_style
            (3.2), projector (3.3), render_options (5.1), three for three;
        (c) EVERY DEFAULT IS THE CORRECT ANSWER — render_options{} cannot produce
            a wrong picture; reaching a wrong answer costs a line that says its
            name;
        (d) INSTRUMENTATION IS OPTIONAL AND SAYS SO IN THE TYPE — clip_stats& was
            required, so 7 throwaway `clip_stats ignored;` variables existed.
        collect_triangles: 15 parameters -> 8, at 8 call sites. The old signature
        was not merely ugly, IT WAS A TAX ON EVER IMPROVING THE THING IT BELONGED
        TO: a ninth knob meant editing eight calls forty lines apart.
  physical-design: A HEADER IS A PROMISE ABOUT REBUILD TIME as much as about
        behaviour. Measured on this tree, best of three, incremental:
          demos/sandbox/main.cpp      0.38 s
          engine/src/gfx/raster.cpp   0.42 s   (one object file, then relink)
          .../gfx/gpu_scene.hpp       0.81 s
          .../gfx/raster.hpp          0.97 s   (everyone who includes it)
        2.3x, on ~22k lines. THE RATIO IS WHAT TRAVELS, not the seconds.
        A HEADER MUST COMPILE ALONE — checked, 37/37 public headers do. The check
        is a four-line loop and belongs in CI.
        A TYPE TWO COMPONENTS SHARE IS A HEADER, not a section of whichever one
        defined it first — which is the entire reason projector.hpp exists
        (soft_renderer and debug_draw both need it).
  characterization-test: BEFORE A REFACTOR, BUILD THE THING THAT COULD PROVE IT
        WRONG. `sandbox --shot PATH` renders SEVEN pinned frames (fixed t, camera,
        light, every mode) to one binary PPM and exits: 1,209,600 bytes, hash
        905BF27E. It lives in demos/common so a harness can call it too.
        THE SEVENTH FRAME EXISTS BECAUSE THE FIRST SIX ALL REPORTED straddle = 0 —
        the near-plane clipper was never called. A characterization test that does
        not reach a branch cannot pin it; READ YOUR INSTRUMENT'S OWN COUNTERS AND
        ASK WHAT THEY MISSED. Frame 6 stands the camera on the floor: 32 in, 8
        straddling, 36 out.
        LESSON 6.4 ADDED AN EIGHTH FRAME, and the reason is the SAME MISTAKE
        CAUGHT TWICE. 6.4 replaced the whole shading model and the shot moved by
        0.60%, because shading::textured is UNLIT and frame 1 is in shadow — three
        of seven frames reached the shading equation at all. The rule generalises
        past clipping: A CHARACTERIZATION TEST THAT DOES NOT REACH A BRANCH CANNOT
        PIN IT, so read what its counters DO NOT say. And the timing rule that is
        the transferable half: THE MOMENT TO EXTEND A CHARACTERIZATION TEST IS THE
        MOMENT IT BREAKS FOR ANOTHER REASON — it is free at a re-baseline and costs
        a re-baseline at every other lesson. New golden E917C06C, 8 frames,
        1,382,416 bytes.
        MOVE WITHOUT CHANGING, THEN CHANGE WITHOUT MOVING, verifying separately.
        Both passes byte-identical; verify_50 (which LINKS) also byte-identical.
  radiometry: LESSON 6.2 GAVE THE SHADING EQUATION UNITS, AND THE UNITS ARE THE
        CONVENTION — every later BRDF must be stated in them.
          L_o = (f_diffuse + f_specular) * E_perp * cos(theta) + albedo * L_ambient
        RADIANCE, W/(m^2 sr), IS WHAT A PIXEL HOLDS. Not irradiance: doubling the
        distance divides irradiance by 4 AND the subtended solid angle by 4, so
        their ratio is invariant along a ray. That cancellation is the entire
        reason a framebuffer needs no distance term. Fails in participating media,
        which this course does not reach.
        A BRDF IS A RATIO WITH UNITS OF INVERSE STERADIAN. Radiance out over
        irradiance in. It may exceed 1 without inventing energy — a surface
        reflecting 100% into a 20-degree cone is 2.7210 sr^-1 against Lambert's
        0.3183, and a mirror's is unbounded. The quantity capped at 1 is the
        INTEGRAL, R(v) = integral of f_r cos(theta) dw.
        THE PI IS THE AREA OF A DISC, and it is DERIVED, never quoted: every patch
        of the hemisphere projects down as its own size times cos(theta), those
        shadows tile the unit disc exactly once, so the cosine-weighted hemisphere
        measures pi. A constant BRDF k therefore returns k*pi; demanding that
        equal the albedo forces k = albedo/pi and nothing else.
        THE COSINE IS ON THE LIGHT'S SIDE. `albedo * n_dot_l` written as one
        expression fuses two independent facts, and the fusion is exactly why the
        pi had nowhere to live. directional_light::irradiance_on() is where the
        cosine now lives, and it is where a point light's 1/d^2 will go.
        EXPOSURE: k_reference_irradiance = pi is THIS ENGINE'S EXPOSURE, named and
        derived (a white Lambertian square-on to it renders at exactly 1.0f, and
        pi * inv_pi IS exactly 1.0f in IEEE single). It is a CHOICE, not a law —
        6.12's tonemapper replaces it, at which point lights get authored in lux
        and this becomes a default.
        THE AMBIENT TERM'S PI CANCELS. Uniform hemispherical radiance L_a through
        albedo/pi integrates to albedo * L_a exactly. So `albedo * ambient`, used
        since 3.6 because it looked right, IS right for a uniform environment —
        the one term needing no constant is the one nobody put a constant in. What
        is wrong with it is the ASSUMPTION (real bounced light is not uniform),
        which is 6.15's subject, and that is now a precise statement rather than
        the word "fudge".
        BLINN-PHONG IS NOT ENERGY CONSERVING, AND NOT FOR THE EXPECTED REASON.
        The 1/pi tames the raw lobe (2.6650 at shininess 1 without it; first under
        1 between shininess 8 and 12). What survives is that DIFFUSE AND SPECULAR
        ARE ADDED WITH NO COUPLING: white albedo + white highlight reflects 1.1386
        at shininess 32, 1.7333 at 2. specular_brdf's /pi is therefore NOT called a
        normalisation anywhere, and verify_62 §F asserts the choice so it cannot
        drift. 6.4's Fresnel is the fix, and it is a MECHANISM, not a constant.
        NOT DONE, NAMED: no specular normalisation, no reciprocity (Blinn's term
        happens to be reciprocal, Phong's is not), no real photometric unit, no
        transmission (BTDF / subsurface).
  microfacets: LESSON 6.3 REPLACED shininess WITH A STATISTICAL CLAIM, and the
        claim is testable, which is the whole difference.
        A SURFACE IS A LANDSCAPE OF PERFECT MIRRORS, far smaller than a pixel and
        far larger than a wavelength. A facet reflects l to v only if its own
        normal IS h — so 3.7's halfway vector stops being a way of putting it. The
        highlight's shape is a HISTOGRAM OF SLOPES and roughness is its width.
        THE NDF IDENTITY IS THE CONVENTION EVERY LATER DISTRIBUTION MUST MEET:
            integral over hemisphere of D(h) * cos(theta_h) dw = 1
        Read backwards: the facets' PROJECTED AREAS total the flat area they stand
        on. verify_63 §A is the test, and it is reused unchanged for every new D.
        THE DIAGNOSTIC TABLE, for when it fails: 2.0 means the cos(theta_h) weight
        is missing; 0.5 means the sin(theta) in dw is; 2*pi means phi was forgotten.
        alpha = roughness^2 IS A CONVENTION (Disney's remap), not physics — an
        imported roughness from a renderer that squares differently WILL NOT MATCH.
        Convert at the import edge and write down which you store: 6.1's
        two-conversions-at-the-edges discipline, applied to a second quantity.
        BLINN-PHONG IS AN NDF MISSING ITS CONSTANT. (s+2)/2pi normalises it; the
        engine ships 1/pi, off by (s+2)/2 = 17x at shininess 32. NOT FIXED IN 6.3:
        the constant is wrong but the MATERIALS were authored against it, so
        specular::colour absorbed it, and correcting one without the other blows
        the highlights out. 6.4 replaces both at once. DONE — and 6.4 DELETED the
        struct rather than fixing the constant, so every call site had to be
        revisited. The legacy 1/pi is still deliberately wrong and still reachable
        via specular_model::{phong,blinn}, which now read the SAME microsurface
        through 6.3's own mapping — the demonstration that the old parameters were
        the new ones badly spelled.
        s = 2/alpha^2 - 2 MATCHES THE PEAK AND ONLY THE PEAK (to one float ULP). A
        lobe fit lands 0.80x lower. Both are right; quote the criterion.
        G IS A CONSEQUENCE, NOT A CORRECTION — once you have said "landscape" you
        have said "some of it is hidden". Height-correlated Smith is the DEFAULT
        because the separable form's independence assumption is false by 1.715x at
        alpha 0.8 and 80 degrees, MEASURED. "Either is fine" was not true.
        SINGLE SCATTERING LOSES 69% AT FULL ROUGHNESS (0.3069 with F=1), because a
        facet bounces light once and the model forgets it. Rough metal renders
        dark, proportionally, so turning the light up cannot fix it. Named, not
        built — Kulla-Conty is the standard compensation and needs 6.4's assembled
        BRDF to attach to.
        NUMERICAL: THE TEXTBOOK GGX DENOMINATOR IS WRONG IN float. Written
        c2*(a2-1)+1 it is a catastrophic cancellation and loses 1.1% of the model's
        energy at mirror roughness. ndf() computes (1-c)*(1+c) + a2*c2 instead —
        identical algebra, and Sterbenz's lemma makes 1.0f - c EXACT for c >= 0.5,
        which is the whole peak region. 450x better at alpha 0.01. verify_63 §A
        asserts the comparison so the "tidy" refactor back fails loudly.
        THE DIAGNOSTIC THAT FOUND IT, and it generalises: QUADRATURE ERROR SHRINKS
        WITH THE GRID, ARITHMETIC ERROR DOES NOT. Two runs — refine the grid, then
        run the same formula in double on the same grid — locate any such bug.

  fresnel-and-assembly: LESSON 6.4 ADDED THE THIRD TERM AND ASSEMBLED THE BRDF.
        The convention every later reflectance term is stated in:
            f_r = D G F / (4 (n.l)(n.v))  +  kd * albedo/pi
        THE COUPLING IS THE LESSON, and it is one sentence: F IS WHAT BOUNCES OFF,
        SO 1-F IS WHAT GOES IN — and only what goes in can scatter back out. Before
        it the two lobes were independent and their sum was unbounded (6.2's
        1.1386). The fault was never a constant; IT WAS THE PLUS SIGN.
        THE DENOMINATOR IS DERIVED, NOT QUOTED, and 6.3 deferred it on purpose.
        Spherical coordinates on the FIXED direction: a facet tilted by theta_h
        turns the ray by 2*theta_h, so dw_out/dw_h = 2 sin(2t)/sin(t) = 4 cos(t)
        = 4(v.h). Two factors of two — one from dtheta_out = 2 dtheta_h, one from
        the double-angle identity, which hands over the cosine as change. Measured
        against finite differences on the sphere: worst 1.42e-03 (verify_64 §B).
        THE (v.h) CANCELS against the facets' projected area toward the light,
        which is (l.h) and equal to it BECAUSE h BISECTS. That cancellation is
        exactly why the finished formula looks unmotivated — the term that would
        explain the 4 is not in it. (n.v) is radiance's per-PROJECTED-area
        definition; (n.l) is the BRDF's own definition per unit irradiance.
        F0 = ((1-n)/(1+n))^2, SQUARED because Fresnel gives an AMPLITUDE ratio and
        reflectance is a ratio of powers. Glass at n=1.5 gives exactly 0.04, which
        is where every renderer's hard-coded constant comes from. Dielectrics live
        in [0.02, 0.08]; only gemstones climb (diamond 0.1724).
        RUN IT BACKWARDS AND IT IS AN AUDIT. ior_from_f0(0.85) = 24.6, where
        diamond is 2.42 — so the engine's shipped specular colours were never
        materials. A PARAMETER THAT ROUND-TRIPS INTO A PHYSICAL QUANTITY CAN BE
        AUDITED; ONE THAT CANNOT, CANNOT. Second time in three lessons this found
        something (6.3's 17x was the first).
        SCHLICK IS EXACT AT BOTH ENDS BY CONSTRUCTION and fitted in between. Worst
        ABSOLUTE error 0.0357 (glass, 85 deg); worst RELATIVE error 23.2% AT 55
        DEGREES, in the middle of the range where surfaces are seen. At 60 deg the
        exact answer is 0.0892 and Schlick says 0.0700 — 21% low. Ships anyway, and
        the defence is NOT that the fit is tight: 23% of 0.04 is 0.019 of a
        reflectance, which is invisible. The absolute error DOUBLES for diamond, so
        the defence weakens as F0 rises. cos_theta here is v.h, NOT n.v — the
        MICROFACET is the mirror, so it is the facet's own normal light bounces off.
        METALS: F0 = lerp(0.04, albedo, metallic), diffuse = albedo*(1-metallic).
        A CONSEQUENCE, not a checkbox: a conductor absorbs what crosses its
        interface within a few atomic layers, so there is no diffuse lobe and the
        colour has nowhere to live but F0. One albedo field serves both materials
        and `metallic` says which question it is answering. metallic is a FLOAT
        because a texture that says "painted here, bare metal there" must filter,
        and a filtered switch is a float.
        THE COUPLING CHOICE IS MEASURED, NOT ASSUMED, and this is 6.4's finding:
            no coupling            worst R(v) = 1.4300
            1 - F(v.h)   [glTF]    worst R(v) = 1.3395   <- STILL EMITS LIGHT
            (1-F(n.l))(1-F(n.v))   worst R(v) = 0.9255   <- ships, the default
        1-F(v.h) is what nearly every engine ships and is EXACT at normal incidence
        (furnace 0.9999). It accounts for the light that got IN and says nothing
        about the light that fails to get OUT: a diffuse ray leaving toward a
        grazing eye meets the interface at a grazing angle, where a quarter of it
        reflects back inside. Bolting on an exit factor FIXES THE ENERGY (0.9628)
        AND FAILS RECIPROCITY (0.2623 vs 0.2932) — which is what selects the
        symmetric two-crossing form. RECIPROCITY IS NOT DECORATION; IT IS WHAT A
        BRDF IS, and it is the test that ruled out the obvious repair.
        THE HONEST COST: two-crossing is ~8.5% DARKER at normal incidence than the
        half-vector form. That light is the portion reflecting back INSIDE at the
        exit boundary, which the model forgets — same class as 6.3's missing 69%.
        Both want Kulla-Conty multiple-scattering compensation, still not built.
        half_vector IS KEPT, named and measured, exactly as 6.3 kept
        smith_g_separable — and 6.6 (glTF) may need it for spec compliance.
        ⚠ VERIFY the glTF Appendix B claim against the Khronos spec before 6.6.

  tangent-space: LESSON 6.7. FOUR CONVENTIONS STACKED ON ONE IMAGE, and three of
        the four have a plausible-looking wrong answer — which is why they are on
        the Conventions page (§7l) and not in a comment.
          colour space  texel_space::linear. The byte is value/255, no curve. Wrong
                        -> EVERY surface tilted 38.8 deg, one direction. Reads as
                        "this map was authored too strong", and the usual fix makes
                        the picture less wrong without making it right.
          encoding      stored = (c+1)/2, so flat is (128,128,255). THAT IS WHY
                        EVERY NORMAL MAP IS LAVENDER — arithmetic, not a
                        convention somebody chose.
          green's sense +v, which is DOWN the image (our origin is upper-left,
                        §7k; glTF agrees). Wrong -> BUMPS READ AS DENTS. An asset
                        problem, undetectable from the image.
          handedness    tangent.w = +/-1, B = w*cross(N,T). Wrong -> one half of
                        every symmetric model lit as the MIRROR IMAGE of the other,
                        because an artist unwraps one arm and reflects it.
        THE DERIVATION IS TWO EQUATIONS, NOT A FORMULA. A triangle's edge is ONE
        WALK described twice — in metres (e) and in texture units (du,dv) — so
        e1 = du1*T + dv1*B and e2 = du2*T + dv2*B, two unknowns, invert the 2x2.
        det is TWICE THE SIGNED UV AREA (2.4's quantity, different axes), so
        det == 0 is a REAL CASE — an untextured face, a collapsed unwrap — and the
        face contributes nothing rather than an infinity.
        A TANGENT TAKES THE MODEL MATRIX; A NORMAL TAKES THE INVERSE TRANSPOSE.
        3.6's distinction on its other side: a normal is defined by being
        PERPENDICULAR (the property a non-uniform scale destroys), a tangent lies
        IN the surface and is therefore A DIFFERENCE OF POSITIONS. Wrong -> 36.9
        deg of SKEW on a (2,1,1) scale — and BOTH ANSWERS STAY IN THE PLANE, so it
        reads as an asset authored at the wrong angle; under a UNIFORM scale they
        agree to 8.4e-08, so it looks perfect on everything nobody stretched.
        THE ROUND TRIP IS THE TEST: a flat map must return the geometric normal,
        and a wrong colour space, decode range, orthonormality, handedness,
        multiply order or missing Gram-Schmidt each break it. ONE ASSERTION, SIX
        BUGS.
        AND ITS FLOOR IS NOT ZERO. 0.5 IS NOT AN 8-BIT CODE: 128/255*2-1 = 1/255,
        so the flattest STORABLE map tilts by 0.318 deg — every flat normal map in
        existence. verify_67 §D asserts the measurement EQUALS the predicted floor
        (5.5460e-03, to 7 digits), which is strictly stronger than the "< 1e-6" it
        first asserted and which the renderer correctly failed.
        THE ENCODING SETS THE TOLERANCE: a half-code error is 0.5/255 stored,
        DOUBLED to 1/255 by the [-1,1] decode, so three channels is sqrt(3)/255 =
        6.79e-03. The first draft forgot the doubling and produced a bound BELOW
        the true floor — a tolerance that is WRONG rather than merely loose fails a
        correct implementation, which is the more expensive mistake.
        WHERE THE COLOUR SPACE LIVES IS SETTLED BY THE HARDWARE, not by taste. In
        SDL_GPU the decode is declared by the texture's FORMAT (_UNORM vs
        _UNORM_SRGB) and performed by the sampler, so one image cannot be sRGB in
        one binding and linear in another. It goes on the TEXTURE. And note the
        finding: THE GPU HAS HAD create_sampled(..., srgb) SINCE 4.7 AND THE
        SOFTWARE RENDERER NEVER HAD THE CONCEPT — the same shape of gap 6.6 found
        between load_image and sample.
        A VERTEX LAYOUT IS PER-PIPELINE STATE. gpu_vertex_pnu went 32 -> 48 bytes
        and `describe` gained `with_tangent`, because locations are numbered ACROSS
        THE WHOLE PIPELINE and a fourth mesh attribute takes location 3 from 4.6's
        instancing. A shader that reads no tangent should not declare one: it is a
        fetch paid for nothing. The BUFFER carries it either way — the pitch is
        sizeof(gpu_vertex_pnu) regardless — so opting out pays the memory and not
        the bandwidth.

  interchange: LESSON 6.6 IMPORTS glTF 2.0, AND THE HEADLINE IS HOW LITTLE THERE
        WAS TO DO. The audit belongs on the Conventions page (§7k) because the NEXT
        format will need it too; a convention mismatch does not throw, it produces
        a PLAUSIBLE PICTURE, and a plausible picture survives review.
        FOUR CONVENTIONS, THREE AGREE:
          handedness   glTF right-handed +Y up == ours (§2). No basis change.
          winding      counter-clockwise front == ours (§7). No index reversal.
          uv origin    glTF (0,0) UPPER left == ours (3.9). NO FLIP — and THIS IS
                       THE TRAP, because OBJ's is the LOWER left, so
                       mesh_import::flip_uv_v defaults to TRUE and is correct for
                       every mesh this engine has loaded and WRONG for every one it
                       loads next. THE FLIP BELONGS TO THE FORMAT, NOT THE CALLER,
                       which is why load_model takes no import settings at all.
          asset facing glTF front faces +Z, our camera looks down -Z. DIFFERS AND
                       IS NOT A CONVERSION: an authoring fact, fixed with a yaw.
                       Negating coordinates would also MIRROR, reversing winding —
                       two wrongs, the second hiding the first.
        gltf.cpp CONTAINS ZERO CONVERSION CODE and verify_66 §B asserts it
        BIT-EXACTLY (vertex 6 is (+0.5,+0.5,+0.5); all 8 match k_cube_vertices to
        the last bit, no tolerance, because every coordinate is exactly
        representable). Also column-major matrices: cgltf_node_transform_world's
        16 floats ARE mat4's storage, so it is a copy and not a transpose.
        WRITE IT OR TAKE IT — the test is NOT "is it hard?" (the rasterizer was
        hard and we wrote it) but WHETHER THE HARD PART IS THE SUBJECT. For OBJ the
        hard part was unifying v/vt/vn triples, which IS the index problem; for
        glTF it is a strided, typed, optionally-sparse view into somebody else's
        bytes — 5 component types x normalized x interleaving x sparse ~= 40 legal
        encodings of the same 8 positions, and not one is about graphics.
        cgltf over tinygltf: one C header, no deps, and tinygltf would pull a
        SECOND copy of stb_image at a different pin.
        THE PARSER RETURNS DESCRIPTIONS, NOT HANDLES. gltf_material_desc holds a
        std::string base_colour_uri and an int material index. Handing parse_gltf
        an asset_store& would be fewer types and would cost three things: the
        parser could not be tested without a filesystem (3.5's parse_obj/load_obj
        split, and §8 used it to test a dozen malformed inputs from string
        literals); Module 9's offline cooker could not use it, since a handle is
        meaningless outside its pool and cannot be serialised; and "is this the
        same image?" would have a second answer that eventually disagrees.
        BASE COLOUR: THE FACTOR IS LINEAR, THE TEXTURE IS sRGB. The single most
        mishandled number in a glTF importer. gltf_material_desc::base_colour is a
        linear_rgb with NO to_linear anywhere near it; material::tint IS encoded
        (6.5's input-edge rule), so the one conversion in the importer runs
        linear -> encoded, in load_model. Get it backwards and gold's 0.71 becomes
        0.4624 and every asset is darkened by the gamma curve — verify_66 §D
        asserts the 0.71 and PRINTS the 0.4624 beside it.
        NO REMAP ON THE SURFACE. metallicFactor and roughnessFactor go straight
        into microsurface, because glTF's alpha = roughness^2 IS 6.3's
        alpha_from_roughness and its dielectric IOR of 1.5 IS k_dielectric_f0 =
        0.04. Gold's baseColorFactor (1.00,0.71,0.29) comes out of f0_of verbatim
        while diffuse_albedo_of returns exactly black. THAT is what it feels like
        when the parameters were derived rather than tuned.
        THE ⚠ VERIFY FROM 6.4 IS DISCHARGED. Khronos Appendix B gives
        dielectric_brdf = mix(diffuse, specular, F), so the claim was TRUE IN
        SUBSTANCE and IMPRECISE: the SAME F scales the specular UP and
        cook_torrance_specular already carries it. Six terms compared, FIVE
        IDENTICAL. The metallic blend is not even a difference — the spec lerps two
        whole BRDFs, we lerp the F0, and those commute EXACTLY because Schlick is
        AFFINE in f0: F(f0) = f0(1-w) + w. Measured 1.19e-07 over 4,851 points.
        Only the diffuse coupling differs: 1 - F(v.h) against
        (1-F(n.l))(1-F(n.v)). Priced at BOTH ends, because one number tells you
        nothing: 1.036x at normal incidence (invisible), 5.62x at 88 degrees
        (unmissable) — which is exactly why it hid for two lessons.
        AND THE SPEC ANSWERS ITS OWN QUESTION. §3.9.6: BRDF implementations MAY
        vary. Appendix B: a physically accurate BRDF MUST be positive, reciprocal
        and energy conserving. two_crossing is 0.9255 and the spec's own sample
        form is 1.3395, so WE SHIP OURS AND ARE CONFORMANT. half_vector stays
        selectable and measured (6.3's two-G-forms precedent). AND NOTE WHY IT
        CANNOT BE A MATERIAL FIELD: a coupling is a SHADER BRANCH, which is
        pipeline state by 6.5's own rule.
        THE ONE CONFORMANCE GAP, REPORTED NOT HIDDEN. glTF §5.19.4: a base colour
        factor is a linear MULTIPLIER on its texture. Ours REPLACES (3.9's rule,
        because both are the albedo and a surface has one). They agree exactly when
        the factor is white, which is the common case; the gap is only the
        COMBINATION. Counted as model_load::factor_texture_conflicts and logged at
        THE ONLY POINT THAT KNOWS BOTH HALVES — the parser sees the factor and the
        URI but not whether the image resolved, the renderer sees a bound texture
        but has lost the factor. A count as well as a log line, because a log line
        scrolls past and a number can be asserted.
        NAMED AND NOT FIXED: metallicRoughnessTexture and normalTexture are
        COUNTED (wants_*) and not loaded, for one concrete reason — they are LINEAR
        DATA IN AN IMAGE and `texture` stores sRGB-encoded texels because sample()
        decodes on read. A roughness map through an sRGB decode is wrong by the
        gamma curve everywhere except 0 and 1. That is 6.7's problem, because
        normal maps need the same thing. The FACTORS import completely.
        THE uint16 CEILING IS REPORTED, NEVER TRUNCATED. k_max_primitive_vertices
        = 65536. A narrowing cast RENDERS — a spray of triangles between the wrong
        corners — with nothing saying so. skipped_too_large +
        max_primitive_vertices is what a caller can act on. Widening costs bytes on
        every mesh; splitting costs code; both are exercises, neither is a thing to
        guess at inside a loader.
        A DERIVED ASSET IS A DEPENDENCY EDGE, NOT A REFCOUNT. A texture is MADE
        FROM an image, so load_texture goes through load_image (one decode however
        many materials name it) and derive_texture records the edge. NOBODY OUTSIDE
        THE STORE CAN HOLD ONE, which is what makes the cascade safe where
        refcounting would cost every property 5.4 bought. unload_image releases 2.
        THE CASCADE MUST ERASE THE NAME ENTRY TOO, or find_texture keeps handing
        out a recycled slot — which does NOT crash (5.4's generation catches it),
        so the symptom is an object silently losing its texture N frames later.
        DERIVED NAMES ARE WHAT MAKE THE MODEL CACHE WORK without a second map:
        "shapes.glb#2" for primitive 2, "shapes.glb:gold" for a material
        (namespaced — two models may both call one "Metal"), and "uv_grid.png"
        BARE for the image, because that one is a real file shared across models.

  material: LESSON 6.5 GAVE THE SURFACE ONE HOME, and the rule that decides
        membership is the HARDWARE'S, not taste:
            CAN THIS BE A NUMBER IN A BUFFER?
        Yes -> per-draw data, pushed as a uniform; two objects differing only in
        it draw back to back. No -> PIPELINE STATE, baked into a pipeline object;
        two objects differing in it need two pipelines and a sort (4.8's receipt:
        three pipelines and pipeline_binds vs ideal_pipeline_binds).
        engine::material = {tint, albedo_map, samp, surface}. cull_mode,
        surface_style and specular_model STAY OUT. verify_65 §A asserts the rule
        as is_trivially_copyable — the machine's way of saying "can be a uniform".
        HANDLE TO STORE, POINTER TO USE, RESOLVE ONCE PER DRAW. bind_albedo() is
        the named step. A handle names a SLOT not an address (measured: 64
        insertions moved the pool 0x928c03500 -> 0x928c16000, handle unaffected);
        a STALE one resolves to "no image" rather than freed memory, which is the
        case a pointer cannot express. Resolving per PIXEL is what 5.4 priced and
        is exactly where it is unaffordable.
        DERIVE, DO NOT STORE. material::textured() reads albedo_map.valid(). The
        old pair — a `textured` float beside a separately-chosen pointer — could
        say "textured but no image" (debug magenta) or "image but not textured"
        (silently flat). A BUG YOU CANNOT EXPRESS BEATS ONE YOU DETECT. uniforms_of()
        is the single packing site; it derives the flag and decodes the tint.
        THE POOL RULE IS ABOUT SHARING, NOT SIZE. scene_object holds its material
        BY VALUE (it has one); ecs_swarm's 96 drones share 6 materials by handle —
        3456 B -> 600 B, 5.8x, and more importantly one write instead of a loop.
        "Handles for big things" gets this backwards: a material is small and the
        handle is still right.
        COST, MEASURED: scene_object 96 -> 112 bytes, and verify_56 caught it. The
        16-byte `sampler` is the whole growth — four enums stored per object,
        identical everywhere. Interning it is named as a debt (Exercise 15.4).
        A FACT IS NOT A DECISION. `closed` is a property of the MESH (validate()
        counts it from the edges, as mesh_report::closed()); the cull decision is
        the caller's, because a closed mesh may still be drawn two-sided. cull_of()
        is the one place the rule lives. 3.4's comment predicting `closed` would
        move onto the material was WRONG TWICE — a material is not pipeline state,
        and `closed` is not cull mode. A COMMENT PREDICTING A FUTURE DESIGN CANNOT
        BE TESTED, which is why it repeated for three modules.
        WHEN A DEMO INVENTS ONE OF YOUR TYPES, THE TYPE IS MISSING. ecs_swarm —
        restricted to the public API since 5.1 — declared `struct material` itself
        in 5.7, with a comment naming the reason. A consumer that cannot reach
        inside the library is a direct measurement of what the API lacks.

completed:
  - 0.1  What a Game Engine Actually Is
  - 0.2  How This Course Works
  - 0.3  Setting Up Your Toolchain
  - 0.4  CMake From Zero
  - 0.5  Your First Window
  - 0.6  Reading Headers & the Debugger
  ===> MODULE 0 COMPLETE <===
  - 1.1  Events, Properly
  - 1.2  Input: State vs Events
  - 1.3  Frames, Delta Time, and Why Naive Loops Lie
  - 1.4  The Fixed Timestep with Interpolation, Derived
  - 1.5  The Framebuffer: Your First Owned Pixels
  - 1.6  Colour, and an Honest Teaser of sRGB
  - 1.7  2D Vectors, Geometrically
  - 1.8  Checkpoint: Pong
  ===> MODULE 1 COMPLETE <===
  - 2.1  Lines: DDA, then Bresenham
  - 2.2  The Triangle: Edge Functions
  - 2.3  Barycentric Coordinates from Signed Areas
  - 2.4  Interpolating Attributes Across a Triangle
  - 2.5  Matrices as Basis Transforms
  - 2.6  Building mat4 by Hand
  - 2.7  Homogeneous Coordinates and What w Really Means
  - 2.8  The Space Chain: Model to World
  - 2.9  The View Matrix: Deriving Look-At
  - 2.10 Perspective from Similar Triangles
  - 2.11 The Viewport Transform
  - 2.12 MILESTONE: A Spinning Wireframe Mesh  (**Module 2 complete**)
  ===> MODULE 2 COMPLETE <===
  - 3.1  The Painter's Problem and the Z-Buffer
  - 3.2  Perspective-Correct Interpolation
  - 3.3  Near-Plane Clipping
  - 3.4  Back-Face Culling
  - 3.5  A Hand-Rolled OBJ Loader
  - 3.6  Normals and Lambert's Cosine Law
  - 3.7  Specular and Blinn-Phong
  - 3.8  Flat, Gouraud and Per-Pixel Shading
  - 3.9  Texture Mapping and Bilinear Filtering
  - 3.10 Profiling, and the Module 3 Capstone
  ===> MODULE 3 COMPLETE — Stage A (the CPU software rasterizer) is DONE <===
  - 4.1  How GPUs Actually Work
  - 4.2  The SDL_GPU Mental Model
  - 4.3  The Shader Toolchain
  - 4.4  The First Triangle
  - 4.5  Vertex Buffers and Layouts
  - 4.6  Uniform Data and the Matrix Upload
  - 4.7  Textures, Samplers, and Depth
  - 4.8  Porting the Module-3 Scene
  - 4.9  Debugging a Frame with RenderDoc
  ===> MODULE 4 COMPLETE — Stage B (the SDL_GPU renderer) draws the scene <===
  - 5.1  The Refactor: Engine, Demos, and the Public API
  - 5.2  The Platform and Application Layer
  - 5.3  Logging, Assertions, and Errors Without Exceptions
  - 5.4  Handles: Generational Indices
  - 5.5  The Asset System v1
  - 5.6  Data-Oriented Design: Why Scene Trees Creak
  - 5.7  An ECS from Scratch: Storage Design
  - 5.8  The ECS Runtime
  - 5.9  Transform Hierarchy and the Camera System
  - 5.10 Input Mapping: Actions, Not Keycodes
        (5.10 was SPLIT from the curriculum's "Input Mapping, ImGui, and Debug
         Draw": three engine subsystems, each with its own design argument, and
         §3.8 forbids truncating. ImGui + debug draw became 5.11. Module 5 is now
         11 lessons, inside its stated 9-11, and the course total is 95.)
  - 5.11 Dear ImGui and the Debug Draw System
  ===> MODULE 5 COMPLETE <===
  - 6.1  Linear and sRGB: The Gamma Lesson
  - 6.2  Radiometry-Lite: What a BRDF Is
  - 6.3  Microfacet Theory
  - 6.4  Cook–Torrance PBR, Derived
  - 6.5  A Material System
  - 6.6  glTF 2.0 Loading
  - 6.7  Normal Mapping and the TBN Derivation
  - 6.8  Shadow Mapping: Bias, Acne, and PCF
  - 6.9  Cascaded Shadow Maps
  - 6.10 Mipmaps, LOD, and Anisotropic Filtering
  - 6.11 Transparency: Alpha Modes, Blending, and Draw Order
  - 6.12 HDR and Tonemapping
        (6.9 and 6.10 were missing from this list until 6.11 added them — the
         list had not been appended to since 6.8 while `capabilities:` below was
         kept current, which is the append-and-merge rule being half-followed.
         Both are published and both are in the index; the omission was here.)

capabilities:
  - 6.12 AN HDR PIPELINE IN BOTH RENDERERS, AND THE LIGHT-UNITS DEBT PAID.
    68 -> 70 public headers, 39 -> 41 sources, 15 -> 17 shaders. Golden
    byte-identical at E917C06C for the TWENTY-FIRST lesson.
    THE ENGINE WAS NOT GETTING HDR WRONG — IT WAS AVOIDING THE QUESTION BY
    CONSTRUCTION, and saying so is the lesson's opening. TWO choices held the lid
    on: `k_reference_irradiance` is pi so a white surface renders at exactly 1.0
    (6.2 §5.2 derived it), and the demo's roughness is 0.49, which peaks at
    0.8676 — JUST under. Change the second and the SAME shading equation returns
    11.59 at roughness 0.20, 2824 at 0.05 and 55,917 for a polished metal
    (15.8 stops), all stored as code 255. Above 1.0 the clamp is TOTAL, not lossy
    at the margin.
    THE GOLDEN HELD FOR A STRUCTURAL REASON, WHICH IS NEW AND STRONGER THAN 6.10's
    AND 6.11's OPT-IN DEFAULTS: the fixture renders into an 8-bit `framebuffer`
    and everything 6.12 built operates on a DIFFERENT BUFFER TYPE. Not "the
    tonemapper is optional" but "the tonemapper is a stage over a target the
    fixture does not have". Decide that question BEFORE writing code — 6.10's
    habit, now three lessons old and never once wrong.
    WHAT IS BUILT: `hdr_buffer` (CPU, 3x memory) and R16G16B16A16_FLOAT (GPU, 2x —
    the difference is entirely the half float); exposure on the photographic EV
    scale; four curves; `measure()` with BOTH means; a full-screen resolve pass on
    both sides; `fill_style::hdr` as the sixth nullable target pointer.
    NOT BUILT, NAMED: auto-exposure (Ex 8.3 — the log-average is already
    measured), HDR DISPLAY output via SDL_GPUSwapchainComposition (Ex 8.5 — a
    different subject: colour volume and metadata, not range), and a desaturation
    step for luminance-only tonemapping.

  - 6.11 TRANSPARENCY IN BOTH RENDERERS, AND THE LAST SILENT GAP IN THE glTF
    IMPORTER CLOSED. 66 -> 68 public headers, 37 -> 39 sources. Golden
    byte-identical at E917C06C for the TWENTIETH lesson — and this one survived a
    REFACTOR rather than an addition.
    NOT A MISSING FEATURE BUT A CORRECTNESS GAP IN SHIPPED CODE. Three facts were
    already true: `pipeline_desc` had said "no blending" since 4.4; 6.6's importer
    read `baseColorFactor[3]` and ignored `alphaMode` entirely; and that was the
    one gap in that importer which FIRED NO STATUS. The reason was structural — a
    status says "the file wants what the engine cannot do" and there was no blend
    state to be the other operand — so the gap could not be REPORTED, only
    REMOVED. Every downloaded asset with foliage or glass had been rendering as
    opaque cardboard, silently, for five lessons.
    THE GOLDEN SURVIVED A REFACTOR, WHICH IS NEW. `sample` and `sample_mipped`
    were both rewritten as WRAPPERS over four-channel versions, and `average_2x2`
    grew a weight per texel. It came through bit for bit for two reasons, both
    deliberate: the colour channels are combined by the IDENTICAL expressions in
    the IDENTICAL ORDER (float addition is not associative), and the default
    weights are exactly 1.0 with a denominator of exactly 4.0, so `sum * (1/4)`
    is bit-for-bit `sum * 0.25f`. "Equivalent in effect" and "identical to the
    last bit" are different claims and only the second one keeps a golden.
    WHAT IS BUILT: alpha masking and alpha blending on BOTH renderers;
    premultiplied alpha as a property of image DATA; a coverage-preserving mip
    chain; a per-frame back-to-front sort; blend state on the GPU pipeline; and
    `alphaMode`/`alphaCutoff` read at last.
    NOT BUILT, NAMED: order-independent transparency. Two INTERSECTING blended
    quads have no correct per-object order — each is in front along part of the
    overlap — and that is a ceiling rather than a missing sort. Also absent:
    alpha in the SHADOW pass (a leaf casts a rectangular shadow; Ex 8.4), and
    two-pass draw for double-sided blended geometry.

  - 6.10 MIPMAPPING IN BOTH RENDERERS, AND A DEBT OF SIX PROMISES CLEARED.
    65 -> 66 public headers, 36 -> 37 sources. Golden byte-identical at E917C06C
    for the NINETEENTH lesson — AND THIS ONE WAS NOT AUTOMATIC.
    THE GOLDEN DECISION, MADE FIRST AND ON PURPOSE: the reference scene SAMPLES
    TEXTURES, so a chain built by default would have moved it. Two honest
    options — re-baseline and say so, or make the chain opt-in. OPT-IN WINS ON
    ITS OWN MERITS: 33% memory, a 1x1 fallback has nothing to average, a UI atlas
    is never minified. `texture` untouched, `mip_chain` a separate type,
    `texture_binding::mips` nullable. Decide this BEFORE writing code, not after
    the diff.
    THE DEBT: mipmaps were promised BY NAME SIX TIMES — 3.9's prose, 3.9's
    Exercises 8.4 and 8.5, 4.7's exercise 4.7.5, 4.7's shipped
    `ti.num_levels = 1; // no mipmaps yet — Module 6`, and a doc comment IN
    texture.hpp's PUBLIC SAMPLER STRUCT. Two external reviewers found the gap
    from the outline alone.
    gfx/mipmap.{hpp,cpp}  NEW. `uv_footprint`, `uv_gradients`, `mip_chain`,
                `build_mips`, `mip_level_for`, `sample_mipped`.
                k_max_mip_levels = 17.
    THE LEVEL IS log2 OF THE FOOTPRINT, and the logarithm is a CONSEQUENCE of the
                pyramid halving rather than a tuning choice — which is why the
                formula has no constants. 3.9's own measured 62.46 texels/pixel
                lands at level 5.965, and the FRACTION IS REAL: no level has
                texels exactly 62.46 across, which is what trilinear blends.
                rho is the LONGER axis, and that one word is the whole of why
                isotropic filtering over-blurs.
    THE ANALYTIC GRADIENT IS THE CPU-SPECIFIC PART AND THE BEST THING HERE.
                A GPU shades in 2x2 quads SO THAT a neighbour exists, which is
                why ddx is a subtraction and why it cannot be called from
                divergent flow. A SCANLINE RASTERIZER HAS NO NEIGHBOUR — the row
                above is discarded, the pixel to the right has not happened. So
                the derivative comes from the TRIANGLE:
                  u = U/W, U and W BOTH AFFINE in screen space (the normalised
                  barycentrics are), so the quotient rule gives
                  du/dx = (dU/dx - u*dW/dx) / W
                df_i/dx is step_x_i * inv_area — the edge steps the fill ALREADY
                walks over the area it ALREADY reciprocated. dU/dx and dW/dx are
                CONSTANT over the triangle, so everything but two multiplies and
                a subtract HOISTS. It is EXACT, not an approximation, and §D
                proves it against a CENTRAL DIFFERENCE of the real interpolation
                on a triangle with genuinely different w per vertex: 5.79e-06,
                which is the finite difference's own truncation error.
                (3.9's Exercise 8.4 asked for exactly this and called it
                "Module 6's job properly".)
    THE LINEAR-LIGHT BUG, QUANTIFIED RATHER THAN NAMED. Averaging is linear, sRGB
                is not, and the curve is convex so the error is always DARKER.
                One 2x2 of black and white should be HALF THE LIGHT = 6.1's code
                188. Naive byte average = code 127 = 0.2122 of white, so it
                DELIVERS 42.2% OF THE LIGHT IT SHOULD — 57.8% too dark AT LEVEL 1
                ALONE — and it COMPOUNDS down the chain. Symptom: a surface that
                dims as it RECEDES, usually diagnosed as "the lighting falls off
                too fast", which sends you to the wrong file.
                `build_mips` reads texel_space FROM THE DATA (6.7's field paying
                for itself again): srgb decodes, linear averages bytes. ALPHA IS
                AVERAGED AS BYTES ON BOTH PATHS — coverage, never encoded.
                THE GPU GETS THIS FREE because the texture has an _SRGB FORMAT,
                so the blit chain decodes and encodes in hardware. The clearest
                case in the course of a format flag doing real work.
    TRILINEAR IS 6.9'S CASCADE SEAM, one lesson later and one dimension down:
                two defensible representations meeting at a boundary. Measured
                step across a level boundary: NEAREST 0.3470, TRILINEAR 0.0124,
                28x smaller.
    ANISOTROPY IS A REFUSAL TO CHOOSE. At 16:1, the long axis picks level 5 and
                blurs the short one SIXTEENFOLD (the smeared distant ground of a
                game with aniso off); the short axis picks level 1 and the long
                one aliases. Take the SHORT axis's level and several taps ALONG
                the long one. ON A SQUARE FOOTPRINT IT COSTS NOTHING — verified
                identical to six decimals, which matters because the failure
                would be invisible and expensive.
    texture.hpp `sampler` gains mip_filter, mip_bias, max_anisotropy — the two
                fields 3.9 collapsed into one and SAID SO. That comment is now
                discharged.
    TWO SDL FIELDS THAT SILENTLY DISABLE THE WHOLE FEATURE, both the same shape
                (a default that is correct at one level and wrong at nine):
                  max_lod = 0 CLAMPS THE ENTIRE CHAIN AWAY. No error, no warning
                    — clamping to level 0 is a legal request. You build the
                    chain, set mipmap_mode, see no change, go looking in the
                    generator. Now 1000.0f.
                  max_anisotropy IS IGNORED unless enable_anisotropy is true.
                create_comparison KEEPS max_lod = 0 and aniso off, deliberately:
                a shadow map has ONE LEVEL (6.9 added LAYERS, not levels).
    ⚠ VERIFY DISCHARGED against the SDL3 tree:
                SDL_GenerateMipmapsForGPUTexture(cb, texture) returns void, takes
                exactly two args, and MUST NOT BE CALLED INSIDE A PASS.
                SDL_gpu.c validates three things — no pass in progress,
                num_levels > 1, and usage carrying SAMPLER|COLOR_TARGET — AND ALL
                THREE LIVE INSIDE `if (COMMAND_BUFFER_DEVICE->debug_mode)`. On a
                release device the requirement is UNCHECKED and the result is
                undefined rather than diagnosed. (4.7's exercise claimed the
                COLOR_TARGET requirement; it was right, and this is the line.)
    THE COST, SAID WITH NUMBERS: 33% memory always (1/4+1/16+... = 1/3, measured
                65,536 -> 87,381 texels). At ONE TEXEL PER PIXEL the mipped fetch
                is IDENTICAL to the plain one, because level 0 is the original —
                so a surface never undersampled pays only memory. A +2 mip_bias
                moves the same fetch 0.4488 -> 0.4872, which is what over-eager
                LOD costs and why the bias is exposed rather than hidden.
                On the demo floor, 13.1% of the frame changes when mips come on —
                the floor, and nothing else.
  - 6.9 CASCADED SHADOW MAPS IN BOTH RENDERERS, AND AN AUDIT 6.8 PASSED.
    63 -> 65 public headers, 35 -> 36 sources. Golden byte-identical at E917C06C
    for the EIGHTEENTH lesson: cascades are an opt-in path and the reference
    scene binds no map.
    THE LESSON'S CLAIM: A CASCADE IS A FIT, NOT A FEATURE. Each cascade is an
    ordinary 6.8 `shadow_map` with a different camera. Nothing about the
    rasterisation, the depth compare, the PCF kernel OR THE BIAS changed.
    THE AUDIT, AND WHY IT MATTERS: 6.8 derived bias from `world_per_texel`.
    Cascades change that by 7.3x. If the derivation was real, nothing should
    need to move — AND NOTHING DID, because `light_camera` owns both terms and
    `visibility()` already read them from there.
    THE RESULT NOBODY ARRANGED: in DEVICE depth the bias is CONSTANT across
    cascades 1-3 at 2.031e-03. wpt = 2r/res and range ~ 2r, SO THE RADIUS
    CANCELS: bias ~ reach*tan(theta)/resolution = 2.0716e-03 predicted, 2%
    agreement. CASCADE 0 IS THE EXCEPTION AT 64% because its range is set by the
    CASTERS (31.9 m) not its own sphere (19.9 m) — an exception PREDICTED BY the
    mechanism, which is worth more than a rule without one.
    gfx/cascade.{hpp,cpp}  NEW. `camera_frustum`, `frustum_slice`,
                `practical_split`, `slice_corners`, `fit_directional_slice`,
                `cascade_settings`, `cascade_choice`, `cascaded_shadow_map`.
                k_max_cascades = 4.
    THE SPHERE, NOT THE CORNERS, and it is the whole anti-shimmer argument: a
                corner-fitted box changes side by 30.2% under yaw (measured
                24.6 -> 35.3 m), and wpt is the side over the resolution, so
                every texel resizes while the camera merely turned. A sphere has
                no orientation: measured spread 9.93e-08. IT COSTS 29% OF THE
                RESOLUTION (0.01508 tight vs 0.01945 sphere) and the lesson says
                so.
    SNAPPING, AND THE BUG THE HARNESS CAUGHT: round the box centre to a whole
                texel IN LIGHT SPACE. 0.499 texels of drift -> 7.63e-06.
                THE BASIS MUST BE ANCHORED AT THE WORLD ORIGIN. Built at the
                slice centre, `basis * point(centre)` is (0,0,0) BY CONSTRUCTION
                and snapping silently does nothing — §E reported an identical
                0.499 with snap on and off, which is a far better error message
                than a slightly crawly picture. `floor`, never a cast: a cast
                truncates toward zero and puts a discontinuity at the origin.
    DEPTH RANGE COMES FROM THE CASTERS, not the slice. An occluder between the
                light and the slice is OUTSIDE it and must still be drawn, or
                shadows blink out at the screen edge (reads as a culling bug,
                is not one). Measured: a 50 m caster stretches range 26.1 ->
                62.3 m and leaves wpt untouched at 0.024999.
    shadow.hpp  `render(objects, meshes, light_camera, bounds, stats)` — the fit
                is the one thing a cascade has to replace. The old signature
                delegates.
    gpu_texture `create_depth_array`. SDL_GPU_TEXTURETYPE_2D_ARRAY +
                layer_count_or_depth. THE SHADOW MAP IS NOW ALWAYS AN ARRAY,
                EVEN AT ONE LAYER, so 6.8's single map is the degenerate case of
                6.9's and the shader has ONE code path — Texture2D and
                Texture2DArray are different binding types, so carrying both
                means two shaders.
    ⚠ VERIFY DISCHARGED against SDL3/SDL_gpu.h: the per-pass layer field is
                `SDL_GPUDepthStencilTargetInfo::layer`, a Uint8, declared after
                `mip_level`. A pass targets ONE layer, so N cascades are N
                passes — which they were anyway, each having its own camera.
    gpu_uniform `cascade_uniforms`, 336 bytes, FRAGMENT SLOT 2. float4 not
                float[4]: HLSL gives each scalar-array element its own 16-byte
                register, so float[4] costs 64 bytes to carry 16.
    view_forward IS NOT PADDING. Splits are AXIAL depth; a fragment knows a
                position. length(eye-world) is RADIAL and differs by
                1/cos(off-axis) — ~22% at the corner of a 60 deg frame — so
                selecting on it bends the seam into a curve following the frame
                edge. dot(world-eye, forward) is the number the splits were
                computed in.
    gpu_scene   THE CASCADE BLOCK IS PUSHED EVEN WHEN THERE ARE NO SHADOWS. A
                cbuffer the shader declares and nobody fills reads as WHATEVER
                WAS LAST IN THAT SLOT, not as zero. The fallback is an identity:
                one cascade, splits at 1e30, 6.8's matrix in every slot. A
                uniform slot has no null.
    raster      `fill_style::cascades` (takes priority over `shadows`) plus
                `view_eye`/`view_forward`. Two pointers, not a variant: they are
                two answers to one question and only the cascaded one needs an
                axis.
    THE HONEST MEASUREMENT, and it is the one to remember: ON THIS COURSE'S OWN
                DEMO SCENE CASCADES LOSE. shapes.glb is 2.4 m across with the
                camera 9.8 m back, so 6.8's scene fit is ALREADY tight — cascade
                0 comes out at 0.02169 against the single map's 0.0207, slightly
                WORSE, because there is no far field to over-serve and the
                sphere charges its 29% anyway. §C measures a 2.8x WIN on a 40 m
                scene. CASCADES PAY WHEN THE SCENE IS MUCH LARGER THAN WHAT YOU
                CAN USEFULLY SEE; they are a fix for a measured problem, not an
                upgrade.
  - 6.8 SHADOWS IN BOTH RENDERERS, FROM A BIAS THAT IS DERIVED RATHER THAN TUNED.
    60 -> 63 public headers, 33 -> 35 sources, 14 -> 16 shaders. Golden
    byte-identical at E917C06C for the SEVENTEENTH lesson, because the reference
    scene binds no map and `shade()`'s new `visibility` defaults to 1.
    THE LESSON'S CLAIM, and it is the one to remember: SHADOW ACNE IS NOT A
    MYSTERY, IT IS A SAMPLING ERROR WITH A COMPUTABLE MAGNITUDE. The map stores
    ONE depth per texel and the fragment is somewhere else inside it, so half of
    every texel's footprint is downhill of its own sample — predicted 50%,
    measured 50.1% on an unoccluded plane. The magnitude is
    `reach * world_per_texel * tan(theta) / depth_range`, and the measured worst
    error reaches 92% OF THAT BOUND. A bound reached is a derivation.
    mat4.hpp  `orthographic(l, r, b, t, near, far)`, derived from an interval
                remap. Its bottom row is (0,0,0,1), so W IS EXACTLY 1 — spent
                three times: depth is affine so precision is uniform (perspective's
                near/far ratio measured at >100x), the near plane may be NEGATIVE
                so the light's eye sits at the scene centre, and a fragment's
                light-space position is recoverable from 3.7's interpolated world
                position for ZERO NEW VARYINGS.
    gfx/bounds.hpp  NEW, header-only. `aabb` at its FOURTH call site (gltf_view,
                mesh_report, the gltf importer, now the shadow fit).
                INSIDE-OUT DEFAULT (+1e30 / -1e30) is the identity element for
                `expand`, which removes the first-vertex branch from every caller.
                `transformed(box, m)` is the box around the transformed BOX.
                6.16's frustum culling is the next caller.
  gfx/shadow.{hpp,cpp}  NEW. `light_camera` (view, clip_from_view,
                clip_from_world, viewport, world_per_texel, depth_range),
                `fit_directional`, `shadow_bias` (none / constant / slope_scaled /
                normal_offset), `shadow_settings`, `shadow_stats`,
                `slope_from_cosine`, `pcf_reach_texels`, `slope_scaled_bias`,
                `quantisation_bias`, `shadow_map` (create / render / visibility /
                bounds_of).
                THE DEPTH PASS IS `collect_triangles` + `draw_triangles` WITH A
                DIFFERENT CAMERA. Not one line of the rasterizer changed.
    gfx/gpu_shadow.{hpp,cpp}  NEW. `gpu_shadow_map`: a depth-only pipeline
                (num_color_targets = 0), a SAMPLED depth texture, a comparison
                sampler, its own render pass, and `fill_uniforms` — ONE function
                so the CPU and GPU cannot disagree about what a bias means.
    light.hpp  `shade(..., float visibility = 1.0f)`. It multiplies E, beside the
                cosine — a shadow is a fact about whether light ARRIVES — and
                NEVER the ambient term, which is exactly what a shadowed surface
                is left with.
    raster.{hpp,cpp}  `fill_style::shadows` (nullable, non-owning) and
                `fill_style::depth_only`. `shadow_map` is FORWARD-DECLARED in
                raster.hpp because shadow.hpp includes it — a shadow pass is a
                rasterizer pass.
    gpu_texture.{hpp,cpp}  `create_depth(..., bool sampled)`,
                `gpu_sampler::create_comparison`, `supported_shadow_format`
                (asks for DEPTH_STENCIL_TARGET | SAMPLER in ONE query, because
                that is the texture actually created).
    gpu_scene.{hpp,cpp}  A THIRD sampler slot and a 1x1 sampled depth texture
                cleared to 1.0 — the third identity-element fallback in this
                class (white for a multiply, lavender for a basis change, 1.0 for
                a depth comparison). It cannot be UPLOADED: the only way to write
                a depth texture is a render pass that clears it and draws nothing.
    gpu_uniform.hpp  `scene_light_uniforms` 64 -> 176 bytes (a float4x4 and
                eleven floats). Acceptable because it is a PER-FRAME push;
                6.7 made a point of a zero-byte flag on the PER-DRAW block, and
                the two are billed at different rates.
    shaders/  `shadow.vert.hlsl` (four lines, ONE attribute out of four) and
                `shadow.frag.hlsl` (`void main() {}` — SDL does not document a
                NULL fragment_shader; ⚠ VERIFY against SDL_gpu.h, and it is also
                where an alpha-tested caster's `discard` would go).
                `scene.frag.hlsl` gains `shadow_visibility` at t2/s2.
    demos/  `gltf_view` gains a GROUND PLANE (a shadow needs a receiver, and acne
                is a pattern ACROSS a lit surface) plus --shadow/--bias/--pcf/
                --shadow-cull/--no-ground-cast; `sandbox` renders the GPU depth
                pass and gains keys [1] [2] [3].
    THE FINDING THIS LESSON MADE BY RENDERING: A 3x3 KERNEL REACHES 2.12 TEXEL
    DIAGONALS, NOT 0.71. A bias sized for one tap leaves two thirds of the error
    uncovered, and the acne returns AT THE MOMENT PCF IS SWITCHED ON, which makes
    it look like a filtering bug. 79.6% acne on a controlled plane; 10,348 stray
    pixels in a real render, against 78 one tap earlier.
    AND THE PREDICTION THE PLAN GOT WRONG, corrected in the lesson rather than
    quietly: the non-linear depth distribution is a PERSPECTIVE problem. An
    orthographic light spreads depth evenly, so the quantisation term is a
    CONSTANT across the whole map. It returns the day the light is a spot light.
  - 6.7 PER-PIXEL NORMALS IN BOTH RENDERERS, AND THE GOLDEN STILL DID NOT MOVE.
    NO NEW FILES — 60 public headers, 33 sources, unchanged. The widest diff since
    6.4, and byte-identical at E917C06C for the SIXTEENTH lesson, because no
    material in the reference scene has a normal map. A NEW CAPABILITY IS A PATH.
    texture.hpp/.cpp  `texel_space{srgb, linear}` on the TEXTURE (not the
                sampler), `texture::space()`, `to_texture(src, space)`,
                `make_normal_bumps(size, cells, strength)` — an ANALYTIC height
                field, so verify_67 §E compares against a closed form rather than
                a picture. ONE BRANCH, in `fetch`, the one place every read
                already goes through.
    mesh.hpp/.cpp  `mesh::tangents` (span<const vec4>), `tangent_at`,
                `mesh_data::tangents`, `with_tangents(m)` — the derivation. Plus a
                file-local `any_perpendicular` for the degenerate fallback.
    material.hpp  `normal_map` (a second texture_handle), `normal_mapped()`
                (DERIVED), `bind_normal_map()`. ONE sampler for both maps, named
                as a compromise: 6.5's interning debt is what fixes it properly.
    raster.hpp/.cpp  `vertex::tangent` (vec4), `fill_style::normal_map`, and the
                fragment — Gram-Schmidt, decode, basis change.
    clip.hpp/.cpp  `clip_vertex::tangent` + one lerp line. INCLUDING `w`, and the
                mirroring-seam degeneracy is named rather than clamped away.
    soft_renderer  `projection_scratch::world_tangent`; the tangent transformed by
                `linear_of(world_from_model)` and NOT the normal matrix; both
                aggregate initialisations converted to DESIGNATED form, which is
                what this lesson's own breakage argued for.
    gpu_mesh    `gpu_vertex_pnu` 32 -> 48 bytes (the static_assert caught it), and
                `describe(desc, slot, with_tangent = true)` — a vertex layout is
                PER-PIPELINE state.
    gpu_scene   `draw_item::normal_map`, a 1x1 flat-normal fallback (128,128,255)
                created with srgb = FALSE, and both slots bound in ONE
                SDL_BindGPUFragmentSamplers call.
    gpu_uniform `material_uniforms::normal_mapped` — spent 6.4's `pad0`, so the
                block is STILL exactly 32 bytes and no binding code moved.
    gltf.hpp/.cpp  TANGENT read (VEC4, the w is handedness), `normal_uri`,
                `normal_scale` (read, NOT applied — Exercise 3), `with_tangents`
                generation after normal generation, `with_tangents`/
                `generated_tangents` counters.
    asset_store `load_texture(name, space)` and `find_texture(name, space)`;
                `texture_key()` — the space is part of the asset's IDENTITY (5.5's
                rule, second type), and THE DEFAULT SERIALISES TO NOTHING.
                `model_load::normal_maps_loaded`.
    shaders     scene.vert (tangent in/out, carried by `world_from_model`),
                scene.frag (t1/s1, the TBN, `normal_mapped` lerp), mesh.vert
                (unchanged at 3/4/5 BECAUSE `describe` can opt out).

  - 6.6 THE ENGINE READS glTF 2.0, AND THE GOLDEN STILL DID NOT MOVE.
    ONE NEW HEADER + ONE NEW SOURCE: 59 -> 60 public headers, 32 -> 33 sources,
    and the first CMake dependency change since 5.11. Golden byte-identical at
    E917C06C for the FIFTEENTH lesson — adding a format adds a PATH.
    gltf.hpp    NEW. gltf_status (9 values), gltf_material_desc (name,
                base_colour LINEAR, alpha, microsurface, base_colour_uri, sampler,
                wants_metallic_roughness_texture, wants_normal_texture,
                double_sided), gltf_primitive (mesh_data + mat4 world_from_local +
                int material + node_name), gltf_scene_data, gltf_report (18
                fields incl. three skip counters), k_max_primitive_vertices,
                parse_gltf(span,base_dir,out) / load_gltf(path,out),
                gltf_default_material().
    gltf.cpp    NEW, and the ONLY translation unit that has ever seen cgltf.
                CGLTF_IMPLEMENTATION defined here and nowhere else. Compiled
                clean at -Wall -Wextra on the first try, which stb did not.
    texture.hpp/.cpp + to_texture(const image_data&) — TWELVE LINES THAT SHOULD
                HAVE EXISTED SINCE 5.3. The engine could decode a PNG (5.3) and
                could sample a texture (3.9) and NOTHING JOINED THEM, because
                every CPU texture was generated and every loaded image went
                straight to the GPU. Two complete halves, no middle, and no test
                could see the gap because no path crossed it. It is a CHANNEL
                SHUFFLE (RGBA bytes -> ARGB8888 words), written through pack_argb
                so the memcpy version is not expressible.
    asset_store + load_texture / insert_texture / find_texture / unload_texture;
                insert_material / find_material / unload_material; load_model;
                derived_count(image_handle); textures()/materials(),
                texture_at()/material_at(); texture_pool + material_pool as
                members; texture_by_key_ + material_by_key_; texture_derivations_
                (a SECOND edge list, not a tagged one — an untagged
                {uint32,uint32} would resolve a texture's bits against the mesh
                pool, which is 5.4's aliasing failure in a new costume);
                counters_.models_loaded (separate from files_read, because one
                model file reads MANY files and a counter measuring two things
                measures neither).
    microfacet.hpp  the ⚠ VERIFY from 6.4 DISCHARGED with the citation. No code.
    demos/gltf_view NEW. The asset system's acceptance test, as hello_cube was
                the library boundary's. Links engine::engine directly, not
                demo_common. --model, --shot, --pose.
    assets/     cube.gltf + cube.bin (text + EXTERNAL buffer + EXTERNAL image URI)
                and shapes.glb (BINARY container, 4 primitives, 3-deep node tree,
                3 materials + the spec's default). GENERATED by
                scratch/make_gltf_assets.py, so the course still ships no
                third-party geometry (3.5's rule) — and the cube's positions are
                transcribed from k_cube_vertices, which is what makes verify_66
                §A a real round trip rather than a tautology.
    CMakeLists  cgltf v1.15 via FetchContent, pinned to a TAG (stb had to be a
                commit; cgltf has releases). PRIVATE include dir on the engine
                target, exactly as stb: no demo can include <cgltf.h>, so
                swapping to tinygltf is a one-file change.

  - 6.5 THE ENGINE HAS A MATERIAL, AND THE GOLDEN DID NOT MOVE.
    TWO NEW HEADERS, header-only: 57 -> 59 public headers, 32 sources, no CMake
    change. A REFACTOR, so the whole claim is byte-identical output.
    material.hpp  NEW. struct material {Uint32 tint, texture_handle albedo_map,
                sampler samp, microsurface surface} + textured() (DERIVED),
                material_handle, material_pool, bind_albedo(), cull_of().
    cull.hpp    NEW. cull_mode, moved out of raster.hpp because two components
                now share it (5.1's projector.hpp rule, second application).
    texture.hpp + texture_handle, texture_pool. NOT in asset_store: that is about
                FILES, and 6.6 is the lesson that gets to decide.
    scene.hpp   scene_object {tint, surface} -> {material mat}; `closed` STAYS,
                with its 3.4 comment corrected.
    gpu_uniform.hpp + uniforms_of() — the one place a material becomes GPU
                numbers, replacing hand-assembly at three sites.
    raster.hpp  cull_mode removed; includes cull.hpp.
    ecs_swarm   ITS OWN `struct material` DELETED — the demo invented the engine's
                type in 5.7 and now stops needing to. Component holds a
                material_handle; nine materials built up front, 96 drones sharing
                six of them.
    demo_scene  texture_set became a texture_pool; the reference shot builds a
                material and resolves it, so the handle path is ON the covered
                path rather than beside it (6.4's eighth-frame discipline).
    VERIFIED IN THREE STAGES, SEPARATELY (5.1's rule), golden E917C06C at each:
      (1) the move — tint/surface into mat, nothing else
      (2) texture handles — texture_set to a pool, the shot resolving
      (3) the GPU packing + the swarm's handle component
    The swarm's own shot was captured before and after FROM CLEAN BUILDS on both
    sides and is byte-identical too.
    TRAP, and it cost ten minutes: capturing the "before" meant git stash +
    incremental rebuild, and scene_object had CHANGED SIZE — so some TUs had the
    old layout and some the new. The symptom was not a crash: it was a shot with
    the wrong scenes (frame 1 rendering `solids` instead of `cycle`, in=20 where
    44 was right, one frame's name printing `?`). PLAUSIBLE GARBAGE. AFTER A
    LAYOUT CHANGE, AN INCREMENTAL BUILD IS NOT EVIDENCE — same failure class as a
    uniform block disagreeing with its shader, on the CPU side.
  - 6.4 THE ENGINE HAS A PHYSICALLY-BASED BRDF, LIVE IN BOTH RENDERERS.
    NO NEW FILES: 57 public headers and 32 sources, unchanged, no CMake change —
    and the widest diff since the 5.1 refactor. `engine::specular` was DELETED.
    microfacet.hpp  + k_dielectric_f0 (0.04), f0_from_ior(), ior_from_f0(),
                fresnel_schlick() (scalar and linear_rgb), diffuse_coupling
                {two_crossing, half_vector}, diffuse_transmission(),
                struct microsurface {roughness, metallic, f0}, f0_of(),
                diffuse_albedo_of(), cook_torrance_specular().
    light.hpp   specular_model gains cook_torrance AND IT IS THE DEFAULT; struct
                specular REMOVED; specular_brdf() now takes a linear_rgb
                reflectance; cook_torrance_brdf() added; legacy_shininess_of()
                lets phong/blinn run off a microsurface; shade() and
                shade_encoded() take a microsurface and an ndf_model.
    gpu_uniform.hpp  material_uniforms {albedo, roughness, metallic, f0, textured,
                pad0} — STILL EXACTLY 32 BYTES, two registers, so not one line of
                binding code moved. Every offset static_assert'd, which is what
                made a change this size safe in one commit.
    scene.frag.hlsl  ndf_ggx(), smith_g(), fresnel_schlick() and the two-lobe
                assembly, carrying 6.3's rearranged GGX denominator because A GPU
                IS NO LESS IEEE-754 THAN A CPU.
    THE FIRST TIME shade() AND THE SHADER HAD TO MOVE IN THE SAME COMMIT. Until
    now one always followed the other by a lesson. verify_48 §F is what made it
    survivable — a fourth row, Cook-Torrance, agreeing to 2.384e-07 (mean 7.8e-08)
    over 4096 fragments. A DRIFTING SHADER DOES NOT FAIL LOUDLY; IT RENDERS
    SOMETHING PLAUSIBLE.
    MATERIALS RE-AUTHORED IN THE SAME COMMIT, because the constant and the
    parameters were wrong in compensating directions (6.3's 17x against an 0.85
    that should have been 0.04). shininess 32 -> roughness 0.49; 48 -> 0.45;
    64 -> 0.42; 80 -> 0.40; 96 -> 0.38; 24 -> 0.53. THE TEAL SLAB AND THE SWARM'S
    SUN BECAME metallic = 1 — both had been faking a metal since 3.7 by typing a
    tinted highlight colour beside a matching tint, two numbers kept in step by
    hand. They can no longer disagree with themselves.
    THE SANDBOX: [E] cycles ROUGHNESS, not shininess — {0.05, 0.12, 0.22, 0.35,
    0.49, 0.65, 0.82, 1.00}, spaced so the steps LOOK even, which is what alpha =
    r^2 buys. [H] cycles none / Phong / Blinn / COOK-TORRANCE.
    THE GOLDEN BROKE, ON PURPOSE, AFTER FOURTEEN LESSONS: 905BF27E -> E917C06C,
    7 frames -> 8, 1,209,616 -> 1,382,416 bytes. Of the shared frames 2,400 of
    403,200 pixels moved (0.60%), ALL DARKER, none brighter — worst per-channel
    R 38, G 93, B 78. "All darker" is the check that matters: an energy-conserving
    model replacing one that emitted light cannot brighten anything.
    AND 0.60% WAS THE INSTRUMENT'S FAULT. Only frames 0, 2, 3 moved. Frames 4, 5
    and 6 bind a texture, and shading::textured is UNLIT BY CONSTRUCTION; frame 1's
    planks face away from the light and encode to albedo*ambient exactly, a term
    6.4 does not touch. THREE OF SEVEN FRAMES EXERCISED THE SHADING EQUATION, and
    the torus — chosen in 3.8 BECAUSE it shows highlights — was drawn unlit.
    So frame 7 was added: the model scene with shading::lit and the texture bound
    as the ALBEDO (per-pixel normals + sampled albedo + the new BRDF), hashing
    3F9DD2CF against frame 5's 81727C17. See characterization-test.
  - 6.3 THE ENGINE HAS A STORY ABOUT WHAT A SURFACE IS, AND IT CAN BE CHECKED.
    ONE NEW HEADER, HEADER-ONLY: 56 -> 57 public headers, 32 sources unchanged, no
    CMake change. NOTHING IS WIRED IN — shade() and scene.frag.hlsl are untouched,
    and the only edit outside the new file is a pointer comment in light.hpp.
    microfacet.hpp  k_min_alpha (1e-3), ndf_model{blinn,beckmann,ggx},
                alpha_from_roughness (= r*r, Disney's remap, a CONVENTION),
                roughness_from_alpha, blinn_exponent_from_alpha (a PEAK match) and
                its inverse, ndf(), smith_g1(), smith_g_separable(), smith_g()
                (height-correlated, the default).
    THE DEFINING IDENTITY, and the first equation in the shading arc with a
    right-hand side a model can be held to:
        integral over hemisphere of D(h) * cos(theta_h) dw = 1
    Read backwards it says the microfacets' PROJECTED AREAS add up to the flat area
    they stand on, which is why the cosine is in it and is not a convention.
    MEASURED: worst error 3.74e-07 over three models and six roughnesses at 400k
    samples. Blinn-Phong SATISFIES IT given (s+2)/2pi — it was a microfacet
    distribution all along — and the engine ships 1/pi, wrong by exactly (s+2)/2,
    = 17x at the default shininess of 32. GGX vs Beckmann: same peak, 456x the tail
    at 45 degrees. Height-correlated vs separable Smith: 1.715x at grazing on rough.
    Single scattering with F=1 returns 0.3069 at full roughness — LOSES 69%.
  - 6.2 THE SHADING EQUATION HAS UNITS, AND THEY ARE SEPARABLE.
    NO NEW HEADERS, NO NEW SOURCES — 56 public headers and 32 sources, unchanged
    for a SECOND lesson. 6.2 renamed things; it did not add a subsystem.
    THE EQUATION, and every later lesson refines it rather than replacing it:
        L_o = (f_diffuse + f_specular) * E_perp * cos(theta) + albedo * L_ambient
    light.hpp   k_inv_pi (std::numbers::inv_pi_v<float>) and
                k_reference_irradiance (= pi). directional_light::intensity
                RENAMED to ::irradiance, defaulting to k_reference_irradiance, plus
                irradiance_on(normal). lambert_brdf(albedo) = albedo/pi and
                specular_brdf(surface, lobe) = colour*lobe/pi, both sr^-1.
                shade()'s local `spec` renamed `lobe` — a lobe is a shape, a BRDF
                is a shape with units, and the function now contains both.
    scene.frag.hlsl  the same three factors, with its OWN k_inv_pi (HLSL has no
                <numbers>), written to more digits than a float holds so the
                compiler rounds once. verify_62 §F PARSES THE SHADER and compares
                bit patterns: both 0x3EA2F983.
    gpu_uniform.hpp  DOCUMENTATION ONLY — zero bytes, zero offsets moved. The
                field was always the PRODUCT colour*scalar, so giving one factor a
                unit could not reach the GPU. A boundary that carries results
                rather than inputs is one the far side cannot be wrong about.
    demos       five call sites, each the compile error the rename existed for.
    MEASURED: over 342,225 channel samples the re-parameterisation moves 154,240
    float results by 1-2 ULP (worst relative 2.465e-07) and ZERO 8-bit codes,
    through BOTH encoders. The golden is byte-identical for the THIRTEENTH lesson
    and here that is the RESULT, not a survival — see the next: block.
  - 6.1 THE ENGINE'S OUTPUT STAGE IS CORRECT ON BOTH SURFACES.
    NO NEW HEADERS, NO NEW SOURCES — 56 public headers and 32 sources, unchanged.
    This lesson CLOSED A GAP rather than adding a subsystem.
    gpu_device.{hpp,cpp}  asks for SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR;
                          gpu_report gains `composition` and
                          `output_encodes_in_hardware`; name_of(composition);
                          is_srgb_format PROMOTED from a file-local helper in
                          gpu_present.cpp to the public header (two callers now).
    gpu_present.cpp       uses the promoted predicate instead of its own copy.
    gpu_uniform.hpp       scene_light_uniforms::pad2 -> encode_output. SAME 64
                          BYTES, SAME OFFSETS: HLSL packing had already reserved
                          the slot, so the flag cost nothing (4.6).
    scene.frag.hlsl       applies the EXACT piecewise sRGB curve when
                          encode_output > 0.5, and returns light otherwise.
    demos/sandbox         fills the flag from gpu.report(), never a constant.
    docs/shared/course.css + docs/_template/check-page.js gained a THIRD listing
                          tag, `unchanged`, for a page that reproduces a file it
                          did not edit. The CSS rule and the checker's allowlist
                          are two places that must agree; add the CSS first.
    MEASURED ON THIS MACHINE: the swapchain was B8G8R8A8_UNORM and is now
    B8G8R8A8_UNORM_SRGB. Linear 0.5 was stored as 128 where 188 was meant.
  - 5.11 THE ENGINE CAN DRAW WHAT IT IS THINKING, AND BE ASKED QUESTIONS.
    TWO SUBSYSTEMS, 54 -> 56 public headers, 30 -> 32 engine sources, and the first
    third-party library in the PUBLIC link line.
    engine/gfx/debug_lines.{hpp,cpp}  the QUEUE. engine/ui/debug_ui.{hpp,cpp}  ImGui.
    engine/gfx/debug_draw.{hpp,cpp}   gains draw_debug_lines(); line3/line3_world
                                      now RETURN bool (survived the near plane).
    engine/core/actions.hpp           gains masked_input<Source>.
    API (queue): debug_line{a,b,colour,remaining} / debug_lines{line, ray, axes,
    box (AABB and OBB), sphere, wire_mesh, advance, clear, lines, size, capacity,
    full, dropped} + k_axis_{x,y,z}_colour, k_debug_this_frame,
    k_default_debug_line_capacity (4096).
    API (ui): debug_ui{start, stop, running, handle_event, begin_frame, render,
    wants_keyboard, wants_mouse, wants_text, version}.
    API (mask): masked_input{update(source, block_keyboard, block_mouse), the six
    input_snapshot accessors, blocking_keyboard, blocking_mouse, cursor_offset_x/y}.
    demos/ecs_swarm DRAWS ITS OWN HIERARCHY: 152 lines = 96 ring + 32 moons + 24
    waypoints, and the hierarchy report independently says 2 roots (sun + camera),
    so 154 = 152 + 2 is TWO SUBSYSTEMS COUNTING THE SAME STRUCTURE TWO WAYS.
    Its five SDL_RenderDebugTextFormat lines at hand-placed y = 6/20/34/48/62 are
    GONE, replaced by two ImGui panels with tables, a slider, buttons that call the
    same functions the keys call, and a TEXT FIELD that is the input seam's demo.
    Three new actions: [F1] panels, [L] links, [G] triads. The world is otherwise
    unchanged (154/906/10/129/3776), because this lesson added tooling only.
    demos/ecs_swarm/main.cpp 933 -> 1,341 lines.
  - 5.10 THE ENGINE CAN BE TOLD WHAT THE PLAYER MEANT.
    engine/core/actions.hpp + engine/src/core/actions.cpp (53 -> 54 public headers;
    FIRST new source file since 5.5, so engine/CMakeLists.txt gained a line), plus
    the on_input() hook in platform/app.{hpp,cpp}.
    API: action_id / input_source / mouse_axis / binding / input_snapshot (a C++20
    concept) / action_map{declare, find, name_of, action_count, bind_key,
    bind_mouse_button, bind_mouse_axis, clear_bindings, bindings, binding_count,
    update<Source>, reset, value, held, pressed, released, consume_pressed,
    pending_presses, clear_pending}.
    demos/ecs_swarm HAS NO SCANCODES IN ITS GAMEPLAY CODE. 13 actions, 19 bindings.
    The picture is unchanged (154 entities, 906 components, 10 pools, 129 drawn,
    3,776 triangles) because this lesson changed how input ARRIVES and nothing about
    what is drawn. New: arrow keys drive camera_yaw/camera_pitch as key-pair axes,
    [ and ] and the wheel drive camera_zoom (a digital pair AND a continuous source
    on ONE action), and [K] rebinds spawn from Space to Enter AT RUNTIME.
    THE HUD READS THE BINDING TABLE rather than a hard-coded string, so it cannot go
    stale — press [K] and it says so. That is the first dividend an action layer
    pays: THE PROGRAM CAN DESCRIBE ITS OWN CONTROLS, which a switch never could.
    THE DEMO DOES ONE OF EACH KIND OF EDGE ON PURPOSE: the UI toggles read pressed()
    in on_input (once per frame), and spawn reads consume_pressed() in
    on_fixed_step (0..N times per frame).
    scratch/verify_510.cpp — 62 checks in seven sections: A declaring, B the value
    rule, C EDGES FROM THE LEVEL, D THE FIXED-STEP TRAP, E rebinding + reset,
    F continuous sources, G the golden. §C and §D are the only two that could catch
    a plausible wrong implementation; the rest check things that are hard to get
    wrong.
    NUMBERS WORTH KEEPING: six presses with no step queues 4 and drains to exactly 4
    (the cap, working). A two-step frame consumes ONE. A zero-step frame does not
    lose the press. The first frame after start-up or reset reports a mouse delta of
    ZERO, because there is no previous position — reporting the absolute position
    would fling the camera on frame one.
    GOLDEN BYTE-IDENTICAL, TENTH LESSON. verify_45..59 all still green.
  - 5.9 THE ENGINE HAS A TRANSFORM HIERARCHY AND A CAMERA THAT IS AN ENTITY.
    engine/include/engine/ecs/{hierarchy,camera}.hpp, header-only, 51 -> 53 public
    headers, no CMake change. Plus rigid_inverse() and is_rigid() in
    math/mat4.hpp — 2.9's derivation extracted so a camera can be an object.
    API: parent / world_transform components; set_parent (refuses cycles,
    parent_status{ok,dead_child,dead_parent,cycle}); is_ancestor_of;
    destroy_subtree; add_hierarchy_components; hierarchy{rebuild, resolve,
    rebuild_and_resolve, mark_topology_changed, topology_changed, order, levels,
    level(i), depth_of, last_report} and hierarchy_report{entities, roots, levels,
    orphans, cycles}. Camera: camera{fovy,near_plane,far_plane}, active_camera
    (tag), look_along, view_from_camera, eye_of, projection_of,
    find_active_camera, set_active_camera.
    demos/ecs_swarm IS NOW THREE LEVELS DEEP: sun -> ring member -> moon, plus
    waypoints and a camera entity. 154 entities, 906 components, 10 pools, 129
    drawn, 3,776 triangles, 2 roots (sun + free camera), 3 levels, 0 orphans, 0
    cycles. Checkable by hand: 1 + 96 + 32 + 24 + 1 = 154, and 1 + 96 + 32 = 129
    carry a geometry. New keys: [F] drift the sun and EVERYTHING FOLLOWS with no
    code that says so; [C] parent the camera to a ring member and it rides;
    [H] detach the outer half, which then orbits the WORLD origin; [X] now uses
    destroy_subtree so a planet takes its moons.
    THE DEMO'S MOONS ARE A TEACHING POINT ON PURPOSE: authored at scale 0.8 under
    a planet of scale 0.16, so they come out 0.128 in world units and their local
    orbit radius of 3.6 becomes 0.58. SCALE COMPOSES DOWN THE CHAIN, which is why
    a "1-metre" prop under a scaled parent is not 1 metre.
    THE RENDER SEAM MOVED ONE LEVEL DEEPER AND IS STILL A SEAM. render_system now
    reads a world_transform (a composed mat4) and must get it into a scene_object,
    which holds a `transform`. The conversion is EXACT and is a trick rather than a
    design: transform::rotation is a general mat3 with no orthonormality
    requirement, so {position = translation_of(m), rotation = linear_of(m),
    scale = 1} reproduces ANY affine matrix bit for bit (verified). Module 6 gives
    the renderer a matrix directly and those four lines go.
    scratch/hier_probe.hpp — FOUR arms over three plain arrays (recursive/CSR,
    level-index, level-packed, dirty), no ECS in the way, for 5.7's reason: an
    experiment built on the container measures the container. bench_59.cpp is five
    measurements; verify_59.cpp is 73 checks in seven sections.
    THE TIMER LIED FIRST. The initial run reported 10.417 ns/entity for two
    different arms at n = 100 — 25 ticks of a 41.67 ns counter, below the 100-tick
    floor. Fixed by scaling the workload to 200,000 composes and dividing, which
    is 4.9's rule applied.
    EVERY ARM REPORTS `agree`, WHICH 5.6 SAYS SHOULD BE IMPOSSIBLE, and the
    explanation has a knob: each addend is a float accumulated into a DOUBLE, and a
    running sum of 1e5 values of magnitude ~1 needs at most 17 + 24 = 41 bits, so
    NOTHING IS EVER ROUNDED and an exact sum is order-independent. Swap the
    accumulator to float and the agreement vanishes (49928.1484 vs 49928.2734).
    verify_59 §F asserts BOTH outcomes.
    GOLDEN BYTE-IDENTICAL, NINTH LESSON, 1,209,616 bytes. 5.9 added two headers and
    two functions and touched no line the reference scene executes.
    verify_45..58 all still green.
  - 5.8 THE ENGINE HAS AN ECS, AND IT IS ABOUT 400 LINES OF LOGIC.
    engine/include/engine/ecs/{entity,pool,registry,view}.hpp, header-only, no
    CMake change. API: create / destroy / alive / add<T> / remove<T> / has<T> /
    get<T> / storage<T> / storage_if<T> / view<Ts...> / clear, plus diagnostics
    (size, pool_count, component_count, entities().slot_count/free_count/
    generation_wraps).
    WHAT IT IS FOR AT THIS SCALE IS COMPOSITION, NOT SPEED, AND THE LESSON SAYS SO
    IN ITS FIRST PARAGRAPH. 5.6 measured that below 1,000 objects every layout is
    within 1%; the demo has 121 entities. The argument is that scene_object has
    been accreting fields since 3.1 (tint, then closed, then a whole specular) and
    a struct is a promise that every instance has every field — so the floor pays
    for a shininess it never uses and an invisible mover is a null handle plus a
    branch in the renderer.
    demos/ecs_swarm — 121 entities, SIX component types, four systems, one new
    target in demos/CMakeLists.txt. At rest: 121 entities, 468 components, 5 pools
    (nothing has a `lifetime` until [Space] — an unused type has NO pool), 97 drawn,
    3,136 triangles, render view leads with pool 1 and 97 candidates. The 24
    WAYPOINTS are the point: placement + orbit and no geometry, so they move every
    step and are invisible because the render query names a component they lack.
    Nobody wrote an `if`. Keys: [Space] 32 sparks (structural churn), [M] strip
    material from every third ring member — 32 vanish AND THE HUD's lead pool moves
    1 -> 2 as the material pool becomes smallest, [O] freeze half by removing orbit
    (they keep spinning: different component, different system), [X] destroy every
    fourth — slots holds, free climbs, and the next [Space] reuses those slots.
    The demo keeps a vector<entity> of destroyed ids ON PURPOSE and skips them with
    alive(); a vector of pointers there would be a vector of landmines.
    THE RENDER SEAM IS NAMED, NOT HIDDEN. collect_triangles still takes
    span<const scene_object>, so render_system walks a view and FILLS one — 6 fields
    per visible entity, ~8 KB at 97 objects, nothing now and not nothing at 1e4.
    Module 6 deletes it. Nothing renders through the ECS this lesson, which is why
    the golden CAN be byte-identical and why that fact is worth checking.
    scratch/verify_58.cpp — 101 checks (99 in a debug build; 2 skip because an
    assertion fires before the release-path refusal can be observed, and the harness
    SAYS SO rather than silently passing), seven sections: A ids, B pool, C
    registry, D views, E churn, F memory, G golden.
    THE NUMBERS THIS LESSON MEASURED:
      §D  pools 60/30/12: view<pos,vel> leads with pool 1 (30 candidates); naming
          them in the other order picks the SAME pool — the decision is by size, not
          argument order; view<pos,vel,label> leads with label, 12 candidates
          instead of 60, i.e. 6 rejections instead of 54 for the same 6 answers.
      §E  200 frames: 518 created, 301 destroyed, 1,061 components added, 131
          removed, 217 live. ID SPACE ENDS AT 220 SLOTS FOR 518 ENTITIES CREATED —
          rule 4's premise MEASURED, and why paged sparse arrays can wait. 0
          generation wraps.
      §A  4,096 create/destroy pairs on ONE slot => slot_count 1, generation_wraps
          exactly 1. The 12-bit budget is monitored, not assumed.
      §F  10,000 entities, 2 types: 78 KB of sparse index. The label pool's sparse
          array is 9,991 entries for 1,000 components — 90% of it means "absent".
          Destroy 9/10 and the id space does NOT shrink; the next 9,000 creates
          reuse those slots rather than growing it.
      §G  golden byte-identical, 1,209,616 bytes — EIGHTH lesson.
    verify_45..57 all still green.
  - DESIGN 5.7: THE ECS STORAGE QUESTION IS ANSWERED, WITH EVIDENCE ATTACHED, AND
    NO ENGINE CODE CHANGED. scratch/ecs_probe.hpp simulates BOTH candidate designs
    (archetype = K parallel dense arrays on one shared index; sparse set = one
    dense walk + K-1 redirects) well enough to time them, WITHOUT building either:
    no entity manager, no registry, no type erasure, no views, no scheduler —
    because the two designs differ in exactly two operations and everything else
    is identical machinery. bench_57.cpp runs four measurements on bench.hpp;
    verify_57.cpp is 54 checks; measure_57.py builds twice (-O2 and -fno-vectorize)
    and prints six tables.
    THE NUMBERS, -O2 -DNDEBUG, Apple M4 Pro (64 KB L1d, 4 MB L2), medians of 25:
      QUERY, K=4, sparse relative to archetype, at n = 4/100/1k/10k/100k
        cheap body, aligned      0.81 0.91 1.06 1.26 1.31
        cheap body, scrambled    0.84 0.98 1.16 1.92 2.40
        real body, aligned       0.97 1.05 1.05 1.02 0.96
        real body, scrambled     0.98 1.05 1.05 1.17 1.34
        CONTROL: K=1 is 0.99-1.01x at every size — both designs do the same walk,
        so anything else would be the harness measuring itself.
      SCALAR (-fno-vectorize), real body, scrambled: 1.00 1.00 1.01 1.21 1.39.
        1.00x to a THOUSAND entities on the WORST-case world. The redirect is
        LATENCY IN THE SHADOW OF WORK, and it costs nothing until the working set
        leaves cache AND the orders have diverged. Either alone is free.
        The scalar build also kills the n=4 anomalies (archetype cheap K=3 went
        2.58 -> 0.53 ns): those were vectorisation, and a cost non-monotonic in
        the work is a compiler, not a cache.
      STRUCTURAL CHANGE, ns per add-or-remove, 1% of the world per frame:
        4 comps   archetype 15.4 15.3 12.3 15.0 13.1 | sparse 9.6 9.4 4.5 5.3 4.25
        12 comps  archetype 29.7 31.4 40.9 39.0 58.5 | sparse 10.1 10.4 4.6 4.4 4.23
        THE FINDING IS WHICH ROW MOVED. Eight components the operation never reads
        take the archetype 13.1 -> 58.5 and the sparse set 4.25 -> 4.23.
        The cost model is NOT bytes moved (that predicts 2.3x, measured 4.5x) but
        INDEPENDENT MEMORY STREAMS TOUCHED: two per column, per move, because a
        column is a separate allocation.
      FRAGMENTATION (archetype's own downside, measured not assumed): 1.12x WORST
        at 512 chunks of 19 entities. Far less than the folklore. Iteration only —
        query-matching, per-archetype column lookup and allocator pressure are NOT
        measured and the case against archetypes must not lean on this.
      SELECTIVITY (1 in 4 matches), real body, relative to archetype, at 100k:
        lead small pool 1.46 | scrambled 1.94 | lead big pool 1.73 | GROUPED 0.99
      MEMORY: 4 types = 25% overhead; 32 types at 1e5 entities = 12.8 MB of sparse
        index, ~80% of it the value meaning "does not have this component".
  - PERF 5.6: THE ENGINE CAN TIME TWO THINGS FAIRLY. engine/core/bench.hpp —
    header-only, no CMake change, 46 -> 47 public headers. bench_result {median,
    min, max, samples, items, spread()}, bench_run(items, reps, body) with a
    discarded warm-up, bench_compare(items, reps, a, b) ALTERNATING one rep each,
    bench_ab {a, b, agree, max_rel_diff, ratio()}, and bench_keep(v) storing
    through an inline volatile. It exists because 5.3, 5.4 and 5.5 each hand-rolled
    the same twenty lines differently — 5.5's own "three copies means the idea has
    no name", applied to the course's own tooling.
    IT IS NOT A PROFILER. core/profile.hpp (3.10) instruments a running frame with
    named zones; this takes code OUT of the program and answers "which is faster".
  - PERF 5.6: THE ECS IS NOW EARNED RATHER THAN ASSUMED. Six layouts x two
    workloads x five scene sizes, on the engine's own transform, in
    scratch/scene_layouts.hpp (shared by bench_56.cpp which times them and
    verify_56.cpp which proves they compute the same scene).
    THE HEADLINE NUMBERS, -O2 -DNDEBUG, Apple Silicon, medians of 25:
      below 1,000 objects EVERY layout is within 13% of flat except virtual;
      the KNEE is between 1,000 and 10,000 = where 96 B/object leaves L2;
      ptr_ordered 1.00x -> 1.98x at 100k, ptr_shuffled 1.13x -> 2.38x;
      tree_fresh 1.04x -> 1.68x, tree_shuffled 1.03x -> 4.86x -> 6.47x;
      virtual 1.49-1.70x, FLAT ACROSS N;
      soa 0.64-0.68x on the full transform, 0.19x on the cull.
  - DOC 5.6: docs/lessons/05-06-data-oriented-design.html. 6 figures, 6 pitfalls,
    5 exercises, and NO ENGINE SOURCE CHANGED — the manifest is one new header and
    four scratch files. Golden byte-identical for the SIXTH lesson, which this
    time is the consequence of correctly deciding to change nothing.
  - ASSET 5.5: THE ENGINE CAN FIND, SHARE AND FREE THE THINGS IT DRAWS.
    engine/asset/ is a NEW DIRECTORY (not under gfx/ — an asset system loads
    meshes, images and Module 7's sounds, and gfx/ would make the audio loader's
    home a joke). Two headers, two sources; 44 -> 46 public headers.
    search_path: ordered roots, resolve() -> resolved_path {path, root_index,
    bytes, refused}, prepend_root/add_root, beside_executable().
    asset_store: load_mesh/load_image (cache probe -> resolve -> load -> import ->
    insert), find_mesh/find_image, insert_mesh/insert_image (generated content,
    REPLACES a name it already holds), derive_mesh (lifetime bound to the source),
    unload_mesh/unload_image/unload_all, meshes()/images()/mesh_at()/image_at(),
    name_of() (O(n), diagnostic — ONE map, not two, echoing 5.4's dense sentinel),
    and asset_counters {files_read, cache_hits, loads_failed, inserted, derived,
    unloaded, bytes_read}.
    image_handle / image_pool are TWO LINES in gfx/image.hpp, and pool<T> needed
    no change at all — the second use is where an abstraction is decided.
  - ASSET 5.5: THE CACHE IS WORTH 15,000x, MEASURED. torus.obj cold 3.749 ms, hit
    0.00025 ms. The stage breakdown is the finding and it is the opposite of the
    intuition: resolve 0.0022, read 200 KB 0.0186, PARSE 3.6903, import 0.0034,
    VALIDATE 2.4873. The disk is a rounding error; the cost is the two steps that
    look at every vertex. That is why Module 9's cooked assets are a real
    optimisation and a faster file format is not.
    A SESSION (five models cycled twice, as [L] does): 8 acquires, 4 file reads,
    4 hits, 205.7 KB. "Loaded once" is now a number.
  - DEMO 5.5: [Bksp] UNLOADS THE MODEL OUT FROM UNDER A LIVE SCENE. The object
    vanishes, everything else keeps drawing, and the HUD reads
    "handle 4:1 is stale, unresolved = 1". The handle is DELIBERATELY LEFT IN
    PLACE rather than nulled — build_scene keeps writing it in every frame, which
    is the whole demonstration. unload_model REFUSES the generated torus: it is
    the round-trip control, and naming an asset is how you protect it.
    load_model no longer frees (a load is not a free), so [L] round the five
    models reads five files ONCE. That broke a verify_54 check written one lesson
    earlier — correctly — and the check now asserts the new contract with the
    reason next to it.
  - DEMO 5.5: THE 4.8 MESH CACHE'S LIFETIME HOLE IS CLOSED. Its private second
    pool became store.derive_mesh(source, with_normals(...)), and its eviction is
    now one line — `if (store.meshes().contains(slots_[i].cpu))` — because an
    entry whose derived asset is gone is a miss. A cache keyed on a pointer could
    never have been told; a cache keyed on a handle finds out by asking the
    question it was already asking.
    demo::mesh_library -> demo::scene_assets: the store is the engine's, and what
    is left is the only part that was ever a program's business — WHICH HANDLES IT
    REMEMBERS. Ask by name once, at load; refer by handle for ever after.
  - ARCH 5.4: NOTHING IN THE ENGINE HOLDS A BORROWED POINTER TO GEOMETRY ANY MORE.
    engine/core/handle.hpp (handle<T>, the 20/12 constants, make_handle) and
    engine/core/pool.hpp (pool<T>) are HEADER-ONLY — no new .cpp, no CMake change,
    44 public headers now — COUNTED, not incremented: 5.3's STATE said 43 and
    `find engine/include -name '*.hpp' | wc -l` says 42 before this lesson, so
    that number was off by one and is corrected here.
    pool<T> is three vectors: slots_ SPARSE and STABLE
    (generation + dense index, indexed by the handle), items_ DENSE and MOBILE
    (live objects only, packed, reallocates freely), owners_ mapping dense back to
    slot. slot::dense == 0xFFFFFFFF IS the occupancy flag — no bool, nothing to
    keep in sync. insert/remove/get/contains are all O(1); removal is SWAP AND
    PATCH (move the last item into the hole, then patch ITS slot), which means a
    live object's address changes while its handle keeps working — the only real
    proof that a handle is not a pointer with extra steps.
  - GFX 5.4: scene_object::geometry IS A mesh_handle. 160 -> 96 BYTES (40%
    smaller, 2.50 -> 1.50 cache lines per object), because a `mesh` view is four
    spans = 64 bytes and a handle is 4. mesh.hpp adds mesh_handle, mesh_pool and
    to_mesh_data(); collect_triangles takes a const mesh_pool& and RESOLVES ONCE
    PER OBJECT at the top of the loop, skipping and COUNTING what does not resolve
    (collect_stats::unresolved — a field that could not previously exist, because
    a dangling span is not detectable).
  - DEMO 5.4: SIX OWNING GEOMETRY MEMBERS IN demos/ BECAME ZERO. demo::mesh_library
    (a pool + three built-in handles + build()) replaces floor_geometry's three
    vectors, model_state::data, model_state::generated and hello_cube's cube_ —
    including its comment "the data must outlive every frame that uses it", which
    was a promise enforced by nothing. build_floor and load_model now REMOVE then
    INSERT, so the handle changes and a stale one fails loudly; load_model builds
    into a local and only hands the pool the result at the END, so a failed load
    leaves the previous model on screen.
  - DEMO 5.4: THE 4.8 MESH CACHE KEY WENT FROM FIVE TESTS TO TWO. It was
    `key == m.vertices.data() && style && cpu.vertices.size() >= … &&
    source_vertices == … && source_indices == …`; it is now
    `source == handle && style == style`. Four of the five existed only because a
    std::vector rebuilt in place keeps its address — an address is a location, not
    an identity. The upload log now prints [handle N:G]. The cache also holds a
    SECOND mesh_pool for the geometry it derives via with_normals, which is why a
    global pool was never on the table.
  - DIAG 5.3: THE ENGINE CAN BE TURNED DOWN. 197 SDL_Log calls were one category
    at one level; the engine's 76 are now 92 ENGINE_LOG_* calls across five
    categories and six levels (error 54, info 25, warn 6, debug 4, critical 2,
    trace 1). `./build/demos/pong` prints ONE line — its own — and
    `--log platform=info` brings the engine back. Zero configuration was needed
    for the default, because SDL's own table already says `*=error`.
    engine/core/log.hpp + log.cpp (categories, six macros with a COMPILE-TIME
    floor, a two-pass `--log` parser, a chaining file sink) and
    engine/core/assert.hpp (three macros). 43 public headers now.
    MEASURED, in bytes of emitted code: at -O2 -DNDEBUG a disabled
    ENGINE_LOG_TRACE is 4 B — identical to an empty function — and the object file
    does not reference SDL_LogTrace at all.
  - ERR 5.3: load_image RETURNS A REPORT. image_report (status, width, height,
    source_channels, bytes, file_bytes, ok()) is the third instance of the shape
    obj_report and gpu_report had already converged on. Exactly ONE call site
    needed changing, which is Lesson 5.1's boundary paying out again.
  - ARCH 5.2: A PROGRAM NO LONGER HAS TO SAY HOW TO START. engine::platform owns
    SDL_Init, the window, the renderer, the streaming texture and the mirror
    teardown; engine::app adds the four SDL3 callbacks on top of it. 40 public
    headers now (37 + platform/{platform,app,main}.hpp), 26 private sources.
    THE MEASUREMENT: 48 lifecycle SDL calls across the three demos became 3 — and
    all three survivors are SDL_PollEvent, in sandbox, on purpose. hello_cube went
    160 -> 96 code lines and 17 -> 0 lifecycle calls. The layer that did it is 428
    lines of code across five files; a poor trade on line count alone, and stated
    as such in the lesson.
  - DEMO 5.2: PONG IS A PROGRAM. ./build/demos/pong, 87 lines of code, five
    overrides, zero SDL lifecycle calls. It was a branch of sandbox's five-way
    [Tab] switch for four modules for exactly one reason — a demo needs a loop and
    there was one loop in the repository. sandbox is down to four screens
    (scene / basis / triangles / lines).
  - HEADLESS 5.2: `--shot` RUNS WITHOUT A DISPLAY. Both sandbox and hello_cube
    choose surface::headless from the same flag that turns the shot on, in
    configure(), before SDL exists. engine::save_ppm (gfx/image.hpp) is the shared
    writer, promoted out of two demos that had each hand-rolled it.
  - ARCH 5.1: THE ENGINE HAS AN OUTSIDE. engine/ is a STATIC LIBRARY
    (engine::engine) with 37 public headers under engine/include/engine/ and 24
    private sources under engine/src/. libengine.a is 1,201 KB. demos/ holds
    three targets that link it: demo_common (shared content), sandbox (Lessons
    1.8-4.9, renamed from `engine`), and hello_cube (160 lines, public headers
    only, the acceptance test — it also takes --shot for CI).
    FOUR NEW PUBLIC HEADERS, lifted out of main.cpp (1,325 lines):
      gfx/projector.hpp      near_mode, projector, screen_point, to_clip,
                             to_pixel, screen_from_clip. Its own header because
                             soft_renderer AND debug_draw both need it.
      gfx/scene.hpp          trs_order, model_matrix, scene_object — the type
                             BOTH renderers consume (4.8's split view, made
                             structural). verify_50 §D checks that the CPU
                             pipeline and a gpu_draw_item build the SAME model
                             matrix from one scene_object.
      gfx/soft_renderer.hpp  raster_triangle, projection_scratch, the stats,
                             camera_view, render_options, collect_stats,
                             collect_triangles, sort_back_to_front,
                             draw_triangles. + soft_renderer.cpp.
      gfx/debug_draw.hpp     line3, line3_world, draw_mesh, draw_axes3,
                             show_depth, count_differences, brightest_channel.
                             Grouped by PURPOSE, not shape. 5.10 grows it into a
                             real debug-draw system. + debug_draw.cpp.
      engine.hpp             the umbrella. Shipped, documented, and used by
                             nothing we ship: 262 ms / 82,507 preprocessed lines
                             vs 215 ms / 72,942 for one header. Only 22% worse
                             BECAUSE SDL ALREADY DOMINATES (73k lines arrive
                             before we contribute anything).
    demos/common/demo_scene.{hpp,cpp} — 1,100 lines of CONTENT: spin, scene_kind,
    build_scene, the floor, model loading, texture_set, orbit_camera, the two
    viewports, the projection constants, and write_reference_shot().
    src/game/pong.* -> demos/common/. IT IS A GAME, AND IT WAS IN src/.
    cmake/EngineHelpers.cmake NEW — engine_set_warnings / engine_use_assets /
    engine_use_shaders, one rule one place. Shaders.cmake reshaped:
    add_hlsl_shader(name stage) now records a GLOBAL PROPERTY instead of taking a
    target, because shaders belong to the repository rather than to any one of
    three executables.
    main.cpp: 7,789 -> 5,721 lines (27%); its -O2 compile 1.30 -> 0.82 s (37%).
  - gfx 4.9: THE ENGINE IS DEBUGGABLE. One new file pair, six modified.
    src/gfx/gpu_debug.hpp/.cpp NEW — scoped_properties (RAII for an
    SDL_PropertiesID), create_named_{buffer,texture,transfer_buffer},
    debug_group (an immovable RAII scope), and frame_log with gpu_event /
    gpu_event_kind and an indented tree printer.
    src/gfx/gpu_buffer, gpu_texture, gpu_present — every resource now named at
    CREATION, and the four TRANSFER BUFFERS named for the first time ever.
    src/gfx/gpu_shader.cpp — the wrong explanation quoted and corrected.
    src/gfx/gpu_scene.{hpp,cpp} — render() takes an optional frame_log* and
    records from the statements that issue the calls.
    src/main.cpp — four debug groups per frame, [P] to dump one frame, and
    `--trace` to dump one frame headlessly and exit.
    THE FRAME, printed: 33 events, 4 groups, 1 pass, 3 draws, 560 uniform bytes,
    cross-checked against draw_stats and AGREEING.
  - gfx 4.8: THE ENGINE DRAWS A SCENE, not a thing. Two new files, three new
    shaders, five modified.
    src/gfx/gpu_scene.hpp/.cpp NEW — surface_style (solid / two_sided /
    wireframe), gpu_draw_item (a borrowed gpu_mesh*, world_from_model,
    normal_from_model, material_uniforms, a texture, a style), draw_stats, and
    gpu_scene_renderer, which owns three pipelines, the depth target and a 1x1
    white texture and turns a list of items into command-buffer calls.
    shaders/scene.vert.hlsl NEW — collect_triangles' per-vertex loop, as a vertex
    stage. Outputs world position (a highlight is view-dependent), the normal, and
    the uv.
    shaders/scene.frag.hlsl NEW — engine::shade(), in HLSL, line for line.
    shaders/matrix_probe.frag.hlsl NEW — the instrument: one 48-byte block
    declared twice, as float3x3 at b0 and as three float4 at b1.
    src/gfx/mesh.hpp/.cpp — normal_style and with_normals(), the import step that
    generates flat or area-weighted smooth normals for geometry carrying none.
    src/gfx/gpu_uniform.hpp — object_uniforms (112 B), scene_light_uniforms
    (64 B), material_uniforms (32 B), each with packed_offset asserts.
    src/gfx/gpu_present.hpp/.cpp — blit_region(), a blit into a rectangle the
    caller chooses, for the split view.
    src/main.cpp — run_gpu_scene(), a mesh cache keyed by pointer (deliberately
    the worst possible asset system), scene_controls, and the inverted flag.
    THE DEMO now runs Module 3's own build_scene / build_floor / load_model
    verbatim — not one of them knows a GPU exists — through both renderers at
    once, on [V].
    KEYS: [V] view, [C] scene, [L] model, [J] normal matrix, [W] wireframe,
    [U] cull, [Z] depth, [O] sort, [H]/[E] specular, [M]/[F] texture, arrows /
    [-][=] / [A][D] camera, lamp.
  - gfx 4.7: THE ENGINE CAN HIDE SURFACES AND PAINT THEM. Four new files, one new
    asset, three new probe shaders, four modified.
    src/gfx/image.hpp/.cpp NEW — image_data (always RGBA8, whatever the file held)
    and load_image, with stb_image confined to the .cpp. The first third-party
    code here that is not SDL, with the "why we don't hand-roll this" paragraph
    CLAUDE.md §4 requires.
    src/gfx/gpu_texture.hpp/.cpp NEW — gpu_texture with two creation paths
    (create_sampled, which is 4.2's transfer chain with a new destination, and
    create_depth, which uploads nothing), gpu_sampler, supported_depth_format and
    depth_bits.
    shaders/depth_probe.{vert,frag}.hlsl and texture_probe.frag.hlsl NEW — the
    instruments. The vertex stage is SHARED by both fragment stages because both
    want the same full-target surface; it writes clip position directly from
    2.10's terms so the experiment measures the depth BUFFER and nothing else.
    assets/uv_grid.png NEW — 256x256, a distinct colour per corner so orientation
    is readable by a program, gridlines, an arrow, and a checkerboard for the
    filter comparison. Generated by scratch/make_uv_grid.py.
    shaders/mesh.frag.hlsl — samples the albedo; 4.5's diagnostic grid survives as
    a uniform-driven toggle rather than being deleted.
    src/gfx/gpu_uniform.hpp — light_uniforms gains uv_scale and grid_mix, grouped
    by RATE OF CHANGE (both per-frame) rather than by subject.
    src/gfx/gpu_device.cpp — name_of gains the depth formats, growing the table
    that 4.2 started rather than beginning a second one.
  - demo 4.7: `engine --gpu` is textured and depth-tested. [C] the depth test
    (off is what every lesson before this looked like), [T] texture or grid, [F]
    linear/nearest — a different sampler OBJECT, not a different argument — and
    [R] the uv scale, which is where address_mode::repeat starts to mean
    something. The depth target is recreated when the swapchain resizes.
  - gfx 4.6: THE CAMERA CAN MOVE. One new header, two new shaders, two rewritten.
    src/gfx/gpu_uniform.hpp NEW — camera_uniforms (a bare mat4, 64 B) and
    light_uniforms (float3/float/float3/float, 32 B, two registers exactly), plus
    packed_offset(), which is HLSL's packing rule as a constexpr function so that
    every block can static_assert its offsets against THE RULE rather than against
    numbers somebody worked out once. NO WRAPPER CLASS ANYWHERE IN THIS FILE —
    there is no object to own.
    shaders/uniform_probe.{vert,frag}.hlsl NEW — the instrument: a full-target
    triangle from SV_VertexID with no vertex layout, and a fragment stage that
    reports one uniform field per pixel. Every measurement in the lesson came
    through it.
    shaders/mesh.vert.hlsl — seven `static const` camera constants and eleven
    lines of hand-written projection deleted; one cbuffer in space1 and one mul().
    shaders/mesh.frag.hlsl — a lighting block in space3, and the ambient term is
    now TINTED by a sky colour rather than grey: one multiply, visibly better, and
    a one-sample approximation of the hemisphere that Module 6 replaces properly.
  - demo 4.6: `engine --gpu` grows a camera you can fly. probe_view holds the
    orbit_camera main.cpp has had since 2.9 (finally reachable from the GPU path),
    a lamp on one angle, and the fovy/near/far the Module 3 scene uses so the two
    pictures are comparable. Arrows or WASD orbit, [Z]/[X] dolly, [0] resets, [L]
    sets the lamp orbiting. Held keys x dt, not per frame (1.3); elevation clamped
    to 1.5 rad, JUST UNDER pi/2, because look_at's cross(up, backward) degenerates
    there (2.9).
    AND IT EXPOSES THE MISSING DEPTH TEST. 4.5's fixed camera hid it by arranging
    the scene so nothing overlapped; orbit now and the later instance wins
    regardless of distance. A limitation you have designed around stops being
    visible and starts being load-bearing. 4.7 fixes it.
  - gfx 4.5: THE ENGINE CAN DRAW A REAL MESH, MANY TIMES. Two new files, two new
    shaders, four modified.
    src/gfx/gpu_mesh.hpp/.cpp NEW — gpu_vertex_pnu (32 B, position+normal+uv,
    half a cache line exactly); interleave() and expand(), the second existing to
    be MEASURED rather than used; gpu_mesh owning a vertex buffer and a 16-bit
    index buffer, knowing which of the two draw calls applies, and enforcing
    k_max_mesh_vertices; describe(), the ONE place the vertex layout is named,
    written entirely in sizeof and offsetof.
    src/gfx/gpu_buffer.hpp/.cpp — adds gpu_stream_buffer (persistent staging,
    cycle = true on both hops) beside the one-shot gpu_buffer.
    src/gfx/gpu_pipeline.hpp/.cpp — instance_buffer(); check_layout() returning a
    layout_report (checked/missing/extra/type_mismatch/overrun/duplicate, plus
    `widening` which is deliberately NOT a problem); size_of() and
    shader_type_of(), the two different questions a format answers.
    src/gfx/gpu_shader.hpp/.cpp — shader_input/shader_inputs and
    parse_shader_inputs(), bounded to the `inputs` array. A malformed array is
    logged and cleared rather than fatal: a shader still draws without the check,
    and what is lost is only the ability to CHECK, which must not be lost
    silently.
    shaders/mesh.{vert,frag}.hlsl NEW — six inputs across two buffers, a frozen
    camera, 2.10's projection as arithmetic, 3.6's Lambert, and a uv grid whose
    only job is to make attribute 2 visible (nothing samples a texture until 4.7,
    so a scrambled uv would otherwise look exactly like a correct one).
  - demo 4.5: `engine --gpu` loads assets/torus.obj, uploads it BOTH ways, builds
    THREE mesh pipelines differing only in pitch (32/28/36), and draws seven
    instances. [6] mesh, [7] indexed/expanded (no visible change — that is the
    point), [8] the pitch, [9] 1/4/7 instances. The startup log prints the byte
    counts, the invocation range and the layout report, so the lesson's claims
    are visible without the harness. 4.4's gpu_vertex shrank 28 -> 16 B.
  - gfx 4.4: THE ENGINE CAN DRAW. Four new files and the first hardware-computed
    pixel in the course.
    src/gfx/gpu_pipeline.hpp/.cpp NEW — `pipeline_desc`, which owns the arrays the
    create-info POINTS AT (see create-info-lifetime) and pre-fills the course's
    conventions; `gpu_pipeline`, move-only, which TIMES ITS OWN CREATION because
    4.3 published a prediction about that call. colour_target_format() for
    offscreen targets — Module 6 needs it, 4.4's harness needed it first.
    src/gfx/gpu_buffer.hpp/.cpp NEW — create + one-shot upload through a staging
    buffer, released while the copy that reads it is still only RECORDED (safe by
    documentation: SDL frees "as soon as it is safe to do so"). cycle = false
    here, deliberately, against gpu_present_target's true — the hazard does not
    exist for geometry written once.
  - demo 4.4: `engine --gpu` draws a gradient triangle in a SECOND render pass,
    LOADOP_LOAD over the blitted software picture, so both rasterizers' output
    shares one window — the comparison Module 2 has been heading towards. [5]
    toggles it. Vertices are in CLIP SPACE because triangle.vert applies no
    matrix; 4.6 gives it a uniform buffer and it starts moving.
  - gfx 4.3: THE ENGINE CAN LOAD A SHADER. Two new files, four new HLSL sources,
    one new CMake module, and no drawing whatsoever.
    shaders/triangle.{vert,frag}.hlsl NEW — the pair 4.4 will draw with; no
    resources at all, so every count is zero.
    shaders/textured.{vert,frag}.hlsl NEW — one uniform buffer (space1) in the
    vertex stage; texture + sampler (space2) and uniform buffer (space3) in the
    fragment stage. They exist so the register rules appear in real code and the
    counts are not all zero. `mul(clip_from_model, float4(pos, 1))` is our
    column-vector convention from 2.5, and HLSL packs cbuffer matrices
    column-major by default, so mat4's bytes cross untouched.
    cmake/Shaders.cmake NEW — the first file in cmake/, which ARCHITECTURE has
    listed as planned since Module 0. find_program for shadercross and glslc, a
    loader-path fix for installs missing an rpath, a CONFIGURE-TIME CAPABILITY
    PROBE, and add_hlsl_shader(target name stage) producing .spv/.msl/(.dxil)/.json.
    src/gfx/gpu_shader.hpp/.cpp NEW — `shader_stage` (parity-checked against
    SDL_GPUShaderStage), `shader_resources` (the four counts and NOTHING ELSE:
    the type's edge is drawn at "what must be discovered"), `shader_target`
    {format, extension, entrypoint}, `choose_shader_target(granted)`,
    `shader_path()`, `parse_shader_reflection()` and the move-only `gpu_shader`.
    THE JSON SCANNER IS ~30 LINES AND THAT IS DELIBERATE: this file is a build
    artefact we generated seconds ago, not a trust boundary — the opposite
    decision to 3.5's parse_obj, and for the opposite reason. It still REFUSES
    (missing key, non-numeric, empty, and the "num_samplers" substring trap).
    src/gfx/gpu_device.hpp/.cpp MODIFIED — create(nullptr, debug) now gives a
    device with NO WINDOW. Not a degenerate case: SDL_gpu.h says offscreen
    rendering with no window is supported, and it is what a shader harness wants.
    The swapchain fields of the report stay at their defaults.
  - demo 4.3: `engine --gpu` loads all four shaders, logs the format chosen, the
    entry point, the byte count and the four resource counts per shader, and
    draws ONE SMALL SQUARE PER SHADER at the top left — green for loaded, red for
    not. The graph is still the HUD; text needs a font and a shader, and we have
    only just produced the shaders. NOTHING IS DRAWN WITH THEM. A shader that
    exists and a shader that draws are two different achievements.
  - gfx 4.2: THE ENGINE CAN TALK TO A GPU. Four new files, no shaders anywhere.
    src/gfx/gpu_device.hpp/.cpp NEW — `gpu_status` {ok, no_device,
    window_not_claimed}, `gpu_report` (driver, shader formats asked AND granted,
    swapchain format, present-mode support, frames in flight — every field a query,
    none an assumption), `gpu_device` owning BOTH the device and the window claim
    because their orders are opposite (claim after create, release before destroy).
    Move-only; two-phase create() returning the report, since a constructor cannot
    fail without exceptions. handle() is public ON PURPOSE: the wrapper exists for
    LIFETIME, not concealment — wrapping ninety SDL functions would hide the API
    this course is about.
    src/gfx/gpu_present.hpp/.cpp NEW — `blit_rect`, `fit_centred` (letterboxing by
    integer cross-multiplication, no divide-by-zero, tested in verify_42 §H), and
    `gpu_present_target` owning the device texture + its staging buffer.
    upload() holds BOTH TIME FRAMES IN ONE FUNCTION: memcpy happens now, the copy
    pass happens later. blit_onto() is outside any pass, because a blit IS a pass.
    THE FORMAT IS DERIVED, NOT PICKED: byte layout from SDL, sRGB-ness matched to
    the swapchain so a decode on read cancels an encode on write.
  - demo 4.2: `engine --gpu` is a SECOND PROGRAM in the same binary. Forced, not
    chosen: a window is owned by an SDL_GPU device OR an SDL_Renderer, and every
    HUD in Modules 1-3 is SDL_RenderDebugText. Deleting the five demos [Tab] cycles to
    make room was not a trade worth making, so the flag is parsed at the top of
    main and the branch is taken before SDL_CreateRenderer is ever reached.
    INVERTS AT 4.8, when the GPU path becomes the default.
    Draws the Module 3 rasterizer's picture — checkerboard, spinning
    vertex-coloured triangle — carried to the display by SDL_GPU, plus a live
    stacked graph of draw / record / acquire / fence per frame built entirely from
    framebuffer::fill_rect. THE GRAPH IS THE HUD, because text needs a font and a
    shader and both are later. [1] filter, [2] present mode, [3] frames in flight,
    [4] fence every frame.
  - gfx 4.1: THE RASTERIZER CAN IMITATE THE HARDWARE, AND COUNT WHAT THAT COSTS.
    src/gfx/raster.hpp — `traversal {scanline, quad, quad_debug}` and `quad_stats`
    {quads, shaded, covered, helpers, off_target, efficiency()}; fill_triangle
    gains a `quad_stats*` out-parameter.
    src/gfx/raster.cpp — the fragment body and the depth test EXTRACTED into
    lambdas shared by both traversals (one rule, one place), plus a 2x2 quad walk
    that shades helper lanes and discards them. Bit-identical output in colour AND
    depth, verified.
    NO NEW FILES. Everything is a change to machinery that already existed, which
    is itself the argument Module 4 opens with.
  - demo 4.1: [5] cycles the traversal, `quad_debug` writes helper lanes in debug
    magenta (the engine's third use of that convention — checker_at 3.2, unbound
    sampler 3.9, helper lanes now), and the budget panel gains a lane-efficiency
    row that appears ONLY under a quad traversal — under scanline every lane is
    covered by construction, and "100% efficient" would be announcing that the
    feature is off, dressed up as a result.

  - core 3.10: THE ENGINE CAN MEASURE ITSELF. src/core/profile.hpp/.cpp NEW — `zone`
    (build/collect/sort/fill/overlay/present + a `count` sentinel), `profiler` (a ring
    of 120 frames, medians via std::nth_element, other_ns(), zones_overlapped(),
    resolution_ns(), overhead_ns(), to_ns()), and `scope_timer` (RAII, ALL FOUR
    copy/move operations DELETED so a double-count is unwritable — the same move
    `vertex` made in 2.4).
    IN core/, NOT gfx/, and that is the first placement in this codebase decided by
    Module 5's argument rather than by convenience: measuring time is not a graphics
    concern, and the physics step and the asset loader will each want a zone without
    including a rasterizer to get one.
    THE PROFILER CALIBRATES ITSELF AT CONSTRUCTION — 100000 empty scope_timers — and
    main() logs the result, so the shortest instrumentable interval is a MEASURED
    property of the machine rather than a number in a comment.
    std::nth_element, not sort: O(n), and the ordering it does not produce is
    information we would discard. COPIES the ring first — sorting it in place would
    destroy the history that makes it one.
    to_ns MULTIPLIES BEFORE DIVIDING. `ticks / freq * 1e9` truncates to whole seconds
    first, so every interval under a second reports zero: a profiler in which every
    zone is free, which compiles and runs.
  - gfx 3.10: THE FIRST OPTIMISATION IN THE COURSE WITH A MEASURED BEFORE.
    src/gfx/colour.hpp/.cpp — linear_to_srgb_fast (toe exact, then a four-term fit in
    nested square roots), linear_to_srgb_u8_fast, and `encode_mode {exact, fast}`;
    to_encoded now takes a mode, with ONE branch for three channels because the mode is
    constant for a whole draw.
    src/gfx/raster.hpp/.cpp — fill_style::encode threaded to three call sites
    (shading::textured, shading::lit, and pixel_from, which gained a parameter).
    NOTHING ELSE IN THE RASTERIZER CHANGED: not the interpolation, not the depth test,
    not the sampler. The hotspot was the last line of the fragment, and it had been an
    unremarked one-liner since 2.4.
  - demo 3.10: six zones instrumented; sort_back_to_front EXTRACTED from draw_triangles
    so the painter's sort is timed apart from the fill (one rule, one place, two callers
    — 3.4's is_front_facing discipline); a stacked budget panel on [3] with the
    unmeasured remainder drawn to the end of the bar; engine time and wall time side by
    side; and [4] toggling the encode with a live count of the pixels that differ.
    THE PANEL COVERS THE RENDER, deliberately: that is what a profiler HUD is, which is
    also why its own cost is charged to `overlay` rather than being quietly free.
  - gfx 3.9: COLOUR FROM DATA. The last source of fragment colour that was not a rule.
    src/gfx/texture.hpp/.cpp NEW — `texture` (owning, ARGB8888, sRGB-ENCODED, ROW 0 AT
    THE TOP), `filter` / `address_mode` / `texel_origin`, `sampler`, `texture_binding`
    (= SDL_GPUTextureSamplerBinding's pair, and a pair for the same reason: the same
    image is read two ways in one frame and the same rules apply to a hundred images),
    `wrap_texel`, and sample / sample_nearest / sample_bilinear RETURNING linear_rgb.
    A TEXEL IS A SAMPLE, NOT A SQUARE, and every other rule here follows from it:
    texel space is u*N, integers are BOUNDARIES, i+0.5 is the CENTRE. Nearest asks
    floor(u*N) and the half plays no part; bilinear needs u*N - 0.5.
    THE HALF-TEXEL ERROR IS INVISIBLE TO NEAREST — 0 of 160801 samples change, against
    160632 under bilinear. That is why it ships, and why the filter gets blamed.
    MEASURED: bilinear at a texel centre is that texel EXACTLY (worst error
    0.000000000; 0.625 without the offset). The shift is -0.5000 texels, bisected on a
    step image. A 1:1 draw is BIT-IDENTICAL: 0 of 4096 texels differ under BOTH filters,
    control 1779. Rests on the sRGB u8 round trip being exact — measured, 0 of 256.
    ONE `texel_filter` WHERE SDL HAS THREE (min/mag/mipmap), and the gap is NAMED: with
    no mipmaps there is nothing different for a minification filter to do, and that
    absence IS the aliasing.
    src/gfx/mesh.hpp/.cpp — flip_uv_v(mesh_data&). One subtraction per uv; touches NO
    indices, because a flip moves where a corner samples FROM, not which corners exist.
    src/gfx/raster.hpp/.cpp — `shading::textured`, `fill_style::albedo`, four lines in
    the fill loop, and the albedo source under `lit`. THE UVs ARE OBTAINED BY ARITHMETIC
    IDENTICAL to uv_checker's — 2.4's "the rasterizer never learns what it carries",
    collecting for the last time in Module 3. Nothing about interpolation, clipping,
    perspective correction or the depth test changed.
    src/main.cpp — albedo_source {rule, checker, uv_grid, fine}, texture_set, [M] image,
    [S] filter, [R] address, [1] texel origin, [2] uv flip on import; texel_wrong and
    filter_wrong.
    THE FLOOR IS LIT. Since 3.6 it was the one surface light did not touch, because a
    procedural rule computes a colour and has no ALBEDO for light to multiply. Module 3's
    last structural gap in the fragment stage, closed.
  - gfx 3.6: LIGHT. face_shade's five-step ramp indexed by TRIANGLE NUMBER is retired to
    a key; surfaces now respond to which way they face.
    src/gfx/light.hpp NEW (header-only) — directional_light {direction, colour,
    intensity} where DIRECTION IS THE WAY LIGHT TRAVELS and to_light() is the
    negation, named so it cannot be skipped; `lighting` adding an ambient constant
    that the header itself labels a fudge; lambert(n, l) = max(0, dot) with the clamp
    justified rather than tidied; shade()/shade_encoded() doing every multiply in
    LINEAR light and normalising the normal at the point of use.
    src/math/mat4.hpp — linear_of() and normal_matrix() = transpose(inverse(3x3)),
    with the derivation in the header: a normal is defined by being perpendicular to
    every tangent, so demanding dot(M*t, X*n) = 0 forces X = (M^-1)^T. Needed no new
    machinery — mat3 has had inverse() and transpose() since 2.5.
    src/main.cpp — shading happens PER VERTEX, in WORLD space, in collect_triangles;
    scratch gains world_normal[] and vertex_colour[]. shade_mode {palette, flat,
    smooth} on [G]; correct_normals on [J] with the naive M kept as the wrong thing
    behind a key (8th time, and the FIRST where the wrong thing was the default);
    [A]/[D] swing the light; normal_stats {shaded, fell_back, max_tilt} and a
    normal_wrong pixel count guarded on max_tilt > 0 — no non-uniform scale, nothing
    to compare, which is itself the lesson.
    THE RASTERIZER DID NOT CHANGE. fill_style gained no field and fill_triangle
    gained no branch: lighting produces a vertex colour, and interpolating vertex
    colours is 2.4's job. That is the vertex/fragment split arriving unbidden.
  - gfx 3.5: GEOMETRY FROM DISK. Four files, and the lesson is none of them being
    the parser.
    THE INDEX PROBLEM IS THE CONTENT. OBJ gives every face corner three INDEPENDENT
    indices (`f 1/1/1`); a vertex buffer has ONE index that selects the whole
    vertex. So a vertex IS the triple (i_v, i_vt, i_vn), and a position shared by
    faces that disagree about uv or normal must be stored twice. Measured on
    assets/cube.obj: 8 positions + 4 uvs + 6 normals -> 24 vertices, 16 splits, 0
    reused corners (every one of the 24 corner tokens is a distinct triple). That is
    also why a cube in any engine's vertex buffer has 24 vertices and not 8.
    Numbered by ORDER OF FIRST APPEARANCE, which is deterministic and independent of
    unordered_map's iteration order — so two loads of a file are bit-identical and
    can be diffed.
    src/gfx/mesh.hpp — mesh gains `normals` (nothing reads them until 3.6; they are
    loaded because the file has them and because a normal PARTICIPATES IN DECIDING
    WHAT A VERTEX IS). New owning `mesh_data` {vertices, uvs, normals, indices} with
    .view() -> mesh: the owner/view pair (string/string_view, vector/span), and the
    answer to the ownership strain 3.2's floor_geometry admitted. New `mesh_report`
    + validate(). k_max_mesh_vertices = 65536 — not arbitrary, it is
    SDL_GPU_INDEXELEMENTSIZE_16BIT (verified in SDL_gpu.h). Three factory functions
    switched to designated initialisers (four members now; -Wmissing-field-
    initializers was right to complain).
    src/gfx/mesh.cpp NEW — validate() and make_torus(). Validation runs ONCE at a
    trust boundary, so it reaches for std::map while the loader's hot de-dup loop
    reaches for unordered_map: match the container to how hot the loop actually is.
    src/gfx/obj.hpp/.cpp NEW — obj_status (enum + line number, NOT an exception and
    NOT a general Result<T,E>: that would be inventing a language feature for a
    problem we have once), obj_report (counts, not a bool — split_vertices is the
    index problem measured on this file), parse_obj(string_view) separate from
    load_obj(path) so every awkward case is testable from a string literal,
    save_obj() which COMPACTS each attribute stream as a real exporter does, and
    asset_path() over SDL_GetBasePath.
    src/main.cpp — scene_kind::model + [L] cycling torus/cube/twisted/quirks/
    generated, a live round-trip pixel comparison, and scene_object::closed now
    computed from validate() instead of typed by hand.
    CMakeLists.txt — POST_BUILD copy of assets/ to $<TARGET_FILE_DIR:engine>, a
    generator expression because multi-config generators put the binary in Debug/.
    .gitignore — `*.obj` is MSVC's object extension AND Wavefront's model
    extension; without `!assets/*.obj` every model silently fails to be added.
    assets/ NEW — cube.obj (20 readable lines, quads, the worked example),
    twisted.obj (one face reversed: 3.4's debt made visible), quirks.obj (CRLF,
    negative indices, mixed corner formats, an n-gon, a degenerate face, unknown
    keywords), torus.obj (2,304 tris, written by save_obj from make_torus).
  - gfx 3.4: BACK-FACE CULLING. raster.hpp gains constexpr is_front_facing(a,b,c)
    (= edge_function < 0; a NAMED rule because it now has two readers — the
    rasterizer that acts on it and the demo that counts it, the same argument
    is_top_left got in 2.4), enum class cull_mode {none, front, back} mirroring
    SDL_GPUCullMode's order exactly (verified against SDL_gpu.h), and
    fill_style::cull defaulting to NONE — the ONE field in that struct whose default
    is SAFE rather than CORRECT, because there is no universally correct cull mode.
    raster.cpp: one branch, placed AFTER the area is known and BEFORE the swap that
    reorients to positive area — the swap destroys the sign, so there is exactly one
    window and this is it. Takes SCREEN-SPACE vertices, so the view-space bug cannot
    be written by accident.
    main.cpp: [U] cycles cull_choice {none, back, front, back_by_forward}; the last
    is the classic bug (dot(n, camera_forward), culled in collect_triangles in VIEW
    space, which is exactly where it lives in real codebases) and maps to
    cull_mode::none at the rasterizer. raster_triangle carries front_by_forward so
    the two tests can be compared on identical geometry every frame. cull_stats
    {submitted, front, drawn, disagree}. scene_object gains `closed` — the
    precondition, declared by the geometry, with the HUD warning rather than
    switching modes per object (two cull modes would mean two batches, which is the
    honest lesson and Module 6's job). Second render with cull=none counts the pixels
    culling changed: 100% of them are pixels a BACK FACE had been drawn on.
  - gfx 3.3: NEAR-PLANE CLIPPING. src/gfx/clip.hpp/.cpp (NEW) — struct clip_vertex
    {vec4 position; vec2 uv; Uint32 colour;} (a CLIP-SPACE type, deliberately
    distinct from engine::vertex which is screen-space, so handing the wrong one to
    the clipper is a compile error rather than nonsense); constexpr near_distance(p)
    = p.z; constexpr near_crossing(da, db) = da/(da-db); lerp() (all four position
    components + uv + colour IN LINEAR LIGHT); clip_segment_near(a, b) in place
    (the 1-D case, for the world grid / axes / wireframe); clip_polygon_near(span,
    span) — Sutherland-Hodgman as TWO questions, not four cases, returning 0/3/4
    vertices; k_clip_max_vertices = 4 (an EXACT bound, proved in the header, not a
    margin — six planes would need 9).
    main.cpp: project() SPLIT into to_clip() and screen_from_clip(); screen_point
    LOSES its `visible` flag (the pipeline now has a plan instead of a question);
    new `struct projector {mat4 proj; viewport vp; near_mode near;}` replaces the
    loose (proj, vp) pair everywhere — adding a knob made every call site SHORTER;
    collect_triangles stops at clip space per vertex and moves the divide INSIDE
    the triangle loop, after the clipper, then fans (0, k-1, k); clip_stats reports
    input / in_front / straddling / behind / output as properties of the GEOMETRY,
    not the mode, so the three modes are comparable. [K] cycles clip / drop / none.
    Floor scene dolly min 4 -> 1 so the camera can actually walk past the floor's
    near edge (z=+6, and the eye at radius 7 is at z~6.97).
    NaN guards, all three genuine hardenings: to_pixel() (float->int is UB out of
    range; clamp to +-8000, which also keeps edge_function's products inside int32),
    checker_at() (magenta on a non-finite uv), linear_to_srgb_u8() (`!(linear > 0)`
    — std::clamp CANNOT remove a NaN, since every comparison with one is false).
  - verified C++20 toolchain (MSVC / GCC / Clang), 64-bit
  - portable CMake build, now six translation units; FetchContent SDL3 (release-3.4.12)
  - engine app: 1280x720 window, complete switch-based event dispatch (quit,
    window-close, window-resize), clean shutdown, startup version log
  - input subsystem (src/core/input): keyboard levels + edges addressed by scancode,
    mouse buttons (levels + edges) and cursor position, wheel accumulation with
    flipped-scroll correction; one frame-coherent snapshot published per frame
  - clock subsystem (src/core/clock): monotonic ns timing, clamped dt + raw dt +
    was_clamped(), elapsed(), frame_count(), smoothed fps() for display
  - fixed_step subsystem (src/core/fixed_step): accumulator, alpha, per-frame step cap
    with drop reporting, runtime set_rate
  - THE loop: fixed-timestep simulation with render interpolation, spiral-guarded
  - demo: variable-dt vs fixed-raw vs fixed-interpolated boxes + a bouncing ball whose
    apexes are now identical at 2000 / 240 / 60 / 20 fps; sim-rate keys [1-4], vsync
    toggle, throttle; on-screen readout via SDL_RenderDebugTextFormat
  - framebuffer subsystem (src/gfx/framebuffer): 320x180 ARGB8888 CPU buffer,
    clear / put_pixel / pixel_at / fill_rect / row(), pack_argb
  - presentation: streaming texture, locked + row-wise upload honouring the driver pitch,
    NEAREST 4x upscale; render resolution independent of window size
  - demo now drawn ENTIRELY into our own pixels: gradient background (two addressing
    paths, timed on screen via [G]), 1.4's three timing boxes and ball, and a pixel trail
    with one dot per simulation step
  - colour subsystem (src/gfx/colour): pack/unpack, exact sRGB transfer functions with a
    256-entry decode LUT, mix_encoded vs mix_linear
  - demo: a comparison board — black-white and red-green ramps mixed both ways, drawn
    TOUCHING so the seam shows the error — plus the bouncing ball, whose trail fade rule
    switches with [M]
  - maths: src/math/vec2.hpp (header-only) — arithmetic, length/length_squared,
    normalised(+_or), dot, perpendicular, project_onto, reflect, lerp, distance
  - gfx 2.12: src/gfx/mesh.hpp (header-only, NEW) — struct mesh (two spans) +
    triangle_count(), plus cube_mesh() (8 verts / 12 tris / 36 idx) and
    icosahedron_mesh() (12 / 20 / 60). main.cpp: draw_cube -> draw_mesh(fb, mesh, m,
    proj, point_w) which transforms each vertex ONCE into view space then walks
    indices 3 at a time drawing each triangle's 3 edges (bounds-checked: its input is
    DATA, which can be wrong in ways code cannot — the same put_pixel-vs-at(row,col)
    distinction from 2.5). New demo-local `struct scene_object { transform xform;
    mesh geometry; const char* name; }` — deliberately NOT bolted onto
    engine::transform (a transform is a placement, not a thing; Module 5's ECS
    attaches mesh and transform as separate components for the same reason).
    Scene is now icosahedron (hero, uniform, spinning) + slab (non-uniform,
    spinning — keeps 2.8's [O] teaching) + plinth (non-uniform, still). The HUD
    probe is now VERTEX 0 OF THE SELECTED MESH and the panel shows verts/tris/idx.
    Dolly min raised 3.0 -> 4.0 (VERIFIED: at 4 the whole scene stays inside the
    viewport at every orbit angle and elevation; the ground grid still runs
    off-screen, which is what a floor should do).
    Verified default frame: icosahedron vertex 0 model(-0.53,+0.85,0) ->
    world(-0.77,+1.29,+0.37) -> view(-0.77,+0.52,-6.42) ->
    clip(-0.83,+1.00,+6.14,w=6.42) -> ndc(-0.130,+0.156,+0.956) -> scr(80.9,82.5,
    d=0.956). 132 wireframe line draws/frame across the 3 objects.
  - gfx 2.11: src/gfx/viewport.hpp (header-only, NEW) — struct viewport {x,y,w,h,
    min_depth,max_depth} mirroring SDL_GPUViewport, with constexpr to_screen(ndc)
    doing the three affine maps and the y flip. main.cpp: k_scene_viewport
    {6, 41.625, 172, 96.75, 0, 1} (centre 92,90; 16:9 to match the projection's
    aspect) REPLACES the loose k_vp_centre/half_w/half_h; project() ends by calling
    to_screen; the HUD's probe now runs the FULL chain model->world->view->clip->
    ndc->SCREEN (the 2.9 R/U/B camera-axis rows were dropped to make room — the
    complete chain is the module's payoff). Depth output computed but unused until
    3.1's z-buffer. Verified default probe ndc(-0.119,0.300,0.959) ->
    screen(81.78, 75.47, d=0.959), inside the viewport, and y ABOVE centre 90
    because ndc.y > 0 — the flip visible in the numbers.
  - maths 2.10: src/math/vec4.hpp gains perspective_divide(v) = v/v.w — a SEPARATE
    named function from xyz() (which still drops w), so drop-vs-divide can never be
    confused. No w guard (behind-camera is clipped upstream, 3.3). src/math/mat4.hpp
    gains perspective(fovy, aspect, near, far); needs <cmath> (added) for std::tan.
    main.cpp: the projection pipeline — project(v_view, proj) does clip = proj*
    point(v) then perspective_divide then viewport (k_vp_centre/half_w/half_h, a
    16:9 rect; -ndc.y is the +y-up->+y-down flip). line3/line3_world/draw_cube/
    draw_axes3/draw_world all take a const mat4& proj now. [P] toggles
    scene_perspective vs scene_orthographic (a demo-local ortho matrix, w=1, kept
    local until 2.11 owns viewport/ortho). Depth cue window retuned to view-z
    [-13,-2]. HUD carries the probe the WHOLE chain model->world->view->clip->ndc
    and shows w=-z_view on the clip line. Verified: default-frame probe
    world(-0.76,1.65,-0.25)->view(-0.76,1.07,-6.87)->clip(-0.82,2.06,6.59,w=6.87)
    ->ndc(-0.119,0.300,0.959); scene fits panel & stays in front (min w 1.74 > near
    0.3) across the whole dolly range.
  - maths 2.9: src/math/vec3.hpp gains cross(a,b) (constexpr; the deferral comment
    is REVISED — cross now belongs to 2.9, not 3.4). src/math/mat4.hpp gains
    look_at(eye, target, up_hint) = the view matrix, built as the inverse of a
    rigid camera placement (transpose the orthonormal basis, -transpose(R)*eye in
    the last column). NO general 4x4 inverse added — mat4 still has none. main.cpp:
    an orbit_camera (target/radius/azimuth/elevation) drives look_at; the scene is
    drawn through view_from_model = view_from_world * world_from_model; to_screen3
    becomes a PLAIN orthographic projection of VIEW space (the 2.8 oblique hack is
    gone — a real camera supplies the 3-D now). Arrows orbit, [-]/[=] dolly (does
    nothing under ortho, on purpose — 2.10 makes it matter). HUD carries one vertex
    model -> world -> view, named in every space, and shows the camera axes read
    from the view matrix's rows. Depth-brightness cue remapped to view-space z
    (window -9..-5, the scene's measured range at r=7).
  - maths 2.8: src/math/transform.hpp (header-only, NEW) — struct transform
    { position; rotation(mat3); scale{1,1,1}; } and parent_from_local(t) building
    M = T*R*S as three scaled rotation columns fed to affine() (9 muls, not 27, and
    it SAYS "column k = axis k times size k" instead of leaving it to be derived).
    NO local_from_parent yet — cheap for this shape (S^-1 * R^T * T^-1) but left for
    2.9's view matrix to derive rather than find written. First "scene": one mesh,
    three transforms, a visible world (ground grid + origin triad). main.cpp:
    to_screen3 gains an OBLIQUE z term (cabinet projection, k_scene_zx/zy) so the
    ground plane no longer collapses to a line — a stopgap, real perspective is 2.10.
    model_matrix(t, order) builds the two WRONG orders on [O] (fifth kept-broken
    demo). Verified: worked vertex (0.5,0.5,-0.5) -> (2.5,1.25,-5.0); T*R*S keeps
    corner 90.000 and |x|=sx at every angle; T*S*R reaches 157.99deg; uniform/still
    controls bit-identical across 360deg.
  - maths 2.7: vec4 gains point(v) [w=1] and direction(v) [w=0] as NAMED
    CONSTRUCTORS — prefer them over to_vec4 everywhere; a literal 1.0f is a magic
    number and magic numbers get changed by whoever is making something compile.
    mat4 gains translation(t), affine(linear, t), translation_of(m).
    ***operator*(mat4, vec4) IS UNCHANGED FROM 2.6, BYTE FOR BYTE.*** The whole
    lesson was a change of MEANING, not machinery — the fix for "my 4x4 does not
    translate" was in what the CALLER said about the data, which is why the code
    you stare at was correct the whole time.
  - maths: src/math/vec3.hpp, vec4.hpp, mat3.hpp, mat4.hpp (header-only, NEW in
    2.6). vec3 = vec2 with a z; every 1.7 idea carries over untouched. NO cross
    product and NO perpendicular() — in 3-D there is a whole PLANE of
    perpendiculars, so the 2-D function has no honest generalisation, and getting
    a specific one needs a second vector, which is exactly what cross takes and
    exactly why 3.4 introduces it when a triangle supplies one.
    vec4 exists so a 4x4 has something to have columns of; its fourth component is
    called w and is NOT yet given a meaning. to_vec4(v, w) is an EXPLICIT named
    function, never an implicit conversion, so w is always something somebody
    chose. xyz(v) DROPS w rather than dividing by it (the divide is 2.10's).
    mat3: three vec3 columns; apply, compose, rotation_x/y/z, scale(sx,sy,sz),
    determinant (cofactor expansion along the top row), transpose, inverse.
    mat4: four vec4 columns, to_mat4(mat3) (VERIFIED faithful AND
    composition-preserving), apply, compose. NO 4x4 determinant or inverse — 2.9
    needs one only for a structured form whose inverse is far cheaper to write
    directly.
  - maths: src/math/mat2.hpp (header-only, NEW in 2.5) — two vec2 COLUMNS, so
    column-major layout is a CONSEQUENCE of naming the right things rather than a
    convention to enforce. Verified: raw floats come out 2,0,1,3 and sizeof is
    exactly 4 floats with no padding. operator*(mat2,vec2) = c0*x + c1*y and
    operator*(mat2,mat2) = {a*b.c0, a*b.c1} — both are their derivations
    transcribed, two lines total, with nothing in them to get backwards.
    at(row,col) bridges written notation and column storage (ROW first,
    deliberately). identity / rotation / scale / shear / determinant / transpose /
    inverse; inverse returns ZEROS when det==0, never NaN — same discipline as
    normalised() and barycentric_at.
  - game: src/game/pong.{hpp,cpp} — a COMPLETE, WINNABLE Pong. Swept collision with a
    runtime toggle back to the naive test so tunnelling can be watched happening;
    angle-from-hit-position paddles; a beatable AI (0.82 speed, chase-only-when-incoming,
    3 px deadzone); deterministic in-state xorshift32; 3x5 bitmap digits for the score;
    teleport-aware interpolation. Verified: perfect tracker beats the AI 11-4, longest
    rally 61 hits, ball reaches the 260 px/s cap, never leaves the court in 300k steps.
  - main.cpp is now a HOST only: window, framebuffer, loop, input->intent, upload, HUD.
    The rules of Pong are not in it and could not be.
  - RASTERIZER (src/gfx/raster) — framebuffer sets ONE pixel; raster decides WHICH pixels
    a SHAPE is made of. draw_line = integer Bresenham, all 8 octants, endpoint-inclusive,
    VERIFIED pixel-identical to a reflect-in/out first-octant midpoint reference over
    1600 lines. draw_line_dda and draw_line_naive kept so the argument can be reproduced.
  - raster: edge_function (constexpr), fill_triangle (bbox + incremental + top-left rule,
    either winding, degenerate- and offscreen-safe), draw_triangle (wireframe),
    struct barycentric + barycentric_at (one reciprocal, three multiplies)
  - raster 2.4: is_top_left is now PUBLIC (interpolation has to UNDO the bias, so the
    rule producing it must be inspectable — and the demo must ask the engine's question,
    not a re-typed copy of it). struct vertex {x, y, colour} — bundling position with
    attributes is CORRECTNESS, not tidiness: reorientation does std::swap(v1,v2) and the
    colour moves with the position it belongs to. enum class blend_space {linear,
    encoded}, defaulting to linear. fill_triangle OVERLOAD taking three vertices =
    Gouraud shading, unbiased weights, linear light. VERIFIED: identical coverage to the
    flat fill over 400 random triangles (0 mismatches), and identical output for both
    windings over 200 (0 differing pixels).
  - raster internals: fill_setup + prepare_fill — bbox, biases, steps and starting values
    extracted so two fills (and Module 3's more) share ONE copy of the subtle part.
    ORIENTATION IS DELIBERATELY LEFT TO THE CALLER: only the caller knows what a vertex
    carries. Private rgb3 + corner_in/pixel_from: NOT linear_rgb, because under
    blend_space::encoded the numbers are 0..255 stored values and a type called
    linear_rgb holding those is a lie that compiles.
  - colour 2.4: struct linear_rgb + to_linear / to_encoded — the type that keeps light
    arithmetic out of stored values. No alpha in it, deliberately (coverage is not light).
    Values above 1.0 permitted; to_encoded clamps only because 8 bits has nowhere to put
    the excess (Module 6 HDR depends on that not being an error).
  - COST, measured on a 20,760-px triangle x400, release: flat fill 20.2 us, encoded
    blend 57.9 us, linear blend 232.0 us = 11.2 ns/px = 11.5x the flat fill. Nearly all
    of it is pow() in linear_to_srgb; decode is a 256-entry table, encode has no obvious
    one. Corner colours decoded ONCE per triangle and 1/area hoisted — without those it
    is worse for no gain. A 4096-entry encode LUT is under 0.4 levels of error everywhere
    (slope 12.92*255 = 3295 levels per unit of light near black) — Exercise 2.4.3.
    NOT done on purpose: 232 us of a 16,600 us budget is not a problem we have, and the
    whole cost vanishes in Module 4 where the GPU encodes sRGB on write for free.
  - demo: [Tab] now CYCLES THREE demos. Triangles (2.2): a rotating triangle in filled /
    wireframe / HALF-PLANE view (colour by how many of the three edge tests a pixel passes
    — Figure 1 rendered live from the shipped code), plus a coverage COUNTER proving the
    fill rule: an axis-aligned square split by its diagonal, green = drawn once, red =
    twice, [R] toggles the rule (0 px -> 40 px).
    Three demos in one binary is now visibly too many: that IS the Module 5 argument.
    Triangle views extended for 2.3: [4] w0 as a ramp INCLUDING the negative region
    outside in red, [5] the iso-line grid (three families of parallel lines). Both
    carry a mouse probe that draws the three sub-triangles — using the same pairing
    barycentric_at uses, so the picture and the maths cannot drift — and prints the
    three weights with their sum.
  - demo: rotating 32-spoke fan (crosses every octant; steep=coral, shallow=green) +
    an 8x magnified pixel inspector that reads back the REAL routine's output, algorithm
    switchable [1][2][3], live pixel count and per-fan timing. Naive lights 1483 px where
    DDA/Bresenham light 2081. Pong preserved on [Tab].
  - demo 2.4: [6] GOURAUD — R/G/B corners, [M] switches blend space, and the centre pixel
    is READ BACK out of the framebuffer and printed (156 vs 85), so the HUD reports what
    was drawn rather than what we believe was drawn. [7] UV CHECKER — the same loop
    carrying (u,v) instead, written out longhand in main.cpp so it can be compared line
    for line against the engine's, and so the by-hand attribute swap on reorientation is
    visible. Right panel is view-dependent: 2.2's coverage counter for [1]-[5], 2.4's
    BIAS MAGNIFIER for [6]/[7] — one 11-px triangle drawn twice at 5x, unbiased vs biased,
    disagreeing cells ringed and counted, band count swept with [ and ] so the count
    visibly jumps between 0 and 15 for one unchanging error. The sweep IS the lesson;
    a fixed impressive number would have taught the wrong thing.
  - demo 2.7: the cube view becomes a SCENE — TWO cubes built from the same
    rotation and offset composed in OPPOSITE ORDERS. translation(-p)*R spins in
    place; R*translation(p) orbits the origin. That is Exercise 2.5.3 answered on
    screen, and Lesson 2.5's "order matters" with translation finally in play. A
    faint cross marks the world origin so the difference reads in a still frame.
    [W] sets corner w to 0 -> BOTH CUBES COLLAPSE ONTO THE ORIGIN, one exactly on
    top of the other. That is Lesson 2.6 reproduced with one keystroke.
    [N] sets the axis-arrow w to 1 -> the direction arrows get TRANSLATED and skew
    off, while the cubes stay perfectly correct. THAT ASYMMETRY IS THE POINT: it
    is why the normal-as-a-position bug survives code review.
    World centres are read back out of the transformed result, so the HUD reports
    what was drawn rather than what we believe was drawn.
  - demo 2.6: [Tab] now cycles FIVE demos — cube (2.6) / basis (2.5) / triangles /
    lines / Pong. main.cpp is past 1,600 lines. This is no longer merely awkward
    and IT IS THE MODULE 5 ARGUMENT — do not fix it early, but do point at it (the
    lesson does, explicitly).
    Cube view: orthographic wireframe (literally drop z), depth-brightness CUE only
    (no z-buffer, no lighting), the three mat3 columns drawn as red/green/blue
    arrows, cube CENTRED on the origin so rotation spins it in place rather than
    orbiting. [Z] cycles rotation_x / _y / _z / Rx*Ry / Ry*Rx — the last two share
    both ingredients and differ ONLY in order. [,] [.] adjust, [0] reset,
    [Space] spin.
    [T] writes (1.2,0,0) into the 4x4's c3 and the cube MOVES BY 0.00 px. The
    displacement is MEASURED (mat4 result minus mat3 result) so the HUD cannot lie.
    That readout is the whole lesson, and it should START WORKING in 2.7 with no
    change to mat4.hpp — only to what w the caller passes.
    The wireframe is a genuinely ambiguous NECKER CUBE: orthographic projection
    discards the information that would settle it, and perspective (2.10) is what
    puts it back. Worth saying that perspective is not cosmetic.
  - demo 2.5: [Tab] now cycles FOUR demos — basis (2.5) / triangles (2.2-2.4) /
    lines (2.1) / Pong (1.8). Four in one binary is well past awkward; that IS the
    Module 5 argument and it is now loud.
    Basis view: the image of the integer lattice (the i==0 lines are drawn too and
    BRIGHTER — omitting them was a real bug, see below), the transformed unit
    square, the two basis vectors as red/green arrows, and an asymmetric F glyph.
    [Z] cycles identity / rotation / scale / shear / R*S / S*R; [,] [.] adjust the
    one parameter; [0] resets; [Space] animates. The two composition modes GHOST
    THE OTHER ORDER as a gold outline, so non-commutativity is watched rather than
    described. The readout prints the matrix BOTH as written and as stored, the
    determinant, and the unit square's area MEASURED by rasterising into a scratch
    buffer and counting pixels.
  - known-and-deliberate: NO TRANSLATION — every linear map fixes the origin, so no
    2x2 can express it. Left broken ON PURPOSE; 2.7 earns the fourth component from
    exactly this gap, and Exercise 2.5.3 (rotate about a point, the painful way) is
    the setup. No mat3/mat4 yet (2.6). rotation() is not constexpr because std::cos
    is not until C++26.
  - known-and-deliberate: no perspective correction — everything is affine in SCREEN
    space, exact for a flat triangle and wrong the moment depth varies (3.2, and the
    artifact is swimming textures); no depth buffer (3.1);
    the encode pow is NOT tabulated (Ex 2.4.3), number written down instead;
    the demo's uv loop is a SECOND copy of the fill loop — deliberate pressure, resolved
    in stages: Module 3 grows `vertex` as attributes earn their place, Module 4 hands it
    to GPU varyings. Two copies is not yet evidence; four would be;
    no line clipping — put_pixel discards out-of-range writes, so an
    off-screen line still costs a full walk (Exercise 2.1.4; Module 3 makes clipping
    mandatory for CORRECTNESS, not speed). No anti-aliasing (Ex 2.1.5, Module 6).
    Two demos in one executable is deliberately awkward — it is the argument for Module 5's
    demos/ split, accumulating where it can be felt;
    explicit Euler still gains energy — but identically everywhere,
    at a rate set by h, which is ours to choose (Module 8 fixes the integrator);
    colour converts per-operation rather than at the pipeline edges (Module 6);
    the debug-text overlay is the only thing on screen SDL still draws;
    ONE impact resolved per simulation step (Module 8 iterates until the budget is spent);
    the naive DDA line routine was RETIRED with 1.7's demo — Lesson 2.1 derives line
    drawing properly and puts it in gfx/ (nothing draws lines at the moment)
  - depth_buffer subsystem (src/gfx/depth_buffer): width x height floats, clear to
    far, clamped row() fast path, depth_format {f32, unorm24, unorm16} mirroring
    SDL_GPU_TEXTUREFORMAT_D32_FLOAT / D24_UNORM / D16_UNORM, quantise() applied
    BEFORE the compare (the order the hardware uses)
  - fill_triangle takes a NULLABLE depth_buffer*, mirroring
    SDL_BeginGPURenderPass's depth_stencil_target_info ("may be NULL"). Depth is
    interpolated with the SAME unbiased weights as colour; the colour blend runs
    AFTER the test, so a losing pixel is never shaded (early-Z in miniature)
  - vertex carries z (device depth), so fill_triangle's reorientation swap carries
    it automatically — the 2.4 argument for bundling attributes, collected again
  - mesh: quad_mesh() added (4 verts / 2 tris, z=0 plane, CCW from +z)
  - demo: filled, depth-tested 3-D. [F] wireframe / painter's / z-buffer / depth view,
    [C] four scenes (solids, CYCLE, intersecting, near-coplanar), [B] depth format.
    Every frame runs BOTH hidden-surface algorithms on identical geometry and counts
    disagreeing pixels — the lesson's headline number, measured live
  - PERSPECTIVE-CORRECT INTERPOLATION of every vertex attribute; the affine failure
    kept behind interpolation::affine and summonable on [I]
  - vertex now carries x, y, z, inv_w, u, v, colour; mesh carries an optional uvs span
    (empty = this geometry has none) plus uv_at()
  - fill_style: render state as an OBJECT (interpolation + shading + blend_space),
    every field defaulting to the correct value. The shape of a GPU pipeline object;
    adding a knob in 3.6 costs one field, not one parameter at every call site
  - shading::uv_checker — a procedural debug pattern, explicitly a placeholder for a
    fragment shader, which 3.6 will strain and Module 4 replaces
  - the VIEWPORT IS NOW A PARAMETER, threaded through project/line3/draw_world/
    draw_mesh/collect_triangles. The floor scene uses the whole 320x180 framebuffer
    (which is already 16:9, so no new projection); everything else keeps the inset rect
  - demo: a checkered ground plane to the horizon, [I] interpolation, [T] tessellation
    (1..16, rebuilt only on change), and the affine-vs-correct disagreement measured live
  - projection_scratch: reusable per-vertex buffers owned ACROSS frames, replacing the
    fixed 64-vertex stack arrays that the 289-vertex floor would have silently truncated
  - skills: reading SDL headers as source of truth; debugging with lldb/gdb/VS

decisions:
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

files:
  /: CLAUDE.md, README.md, ARCHITECTURE.md, LEARNINGS.md, PROMPT.md, LICENSE,
     .gitignore, CMakeLists.txt, STATE.md
  cmake/: EngineHelpers.cmake, Shaders.cmake
  shaders/: triangle.vert.hlsl, triangle.frag.hlsl,
            textured.vert.hlsl, textured.frag.hlsl,
            mesh.vert.hlsl, mesh.frag.hlsl,
            uniform_probe.vert.hlsl, uniform_probe.frag.hlsl,
            depth_probe.vert.hlsl, depth_probe.frag.hlsl,
            texture_probe.frag.hlsl,
            scene.vert.hlsl, scene.frag.hlsl,
            shadow.vert.hlsl, shadow.frag.hlsl                            [6.8]
            fullscreen.vert.hlsl, tonemap.frag.hlsl                       [6.12]
            matrix_probe.frag.hlsl
  engine/: CMakeLists.txt
  engine/include/engine/: engine.hpp                      (the umbrella)
  engine/include/engine/asset/: search_path.hpp, asset_store.hpp        [5.5]
  engine/include/engine/core/: assert.hpp, bench.hpp, clock.hpp, fixed_step.hpp,
            handle.hpp, input.hpp, log.hpp, pool.hpp, profile.hpp,
            actions.hpp                                                     [5.10]
  engine/include/engine/ecs/: entity.hpp, pool.hpp, registry.hpp, view.hpp   [5.8]
            hierarchy.hpp, camera.hpp                                       [5.9]
            (header-only — NO entry in engine/CMakeLists.txt. NOTE: ecs/pool.hpp's
             engine::ecs::pool<T> is a DIFFERENT container from core/pool.hpp's
             engine::pool<T>; see conventions:ecs-runtime.)
  engine/include/engine/gfx/: clip.hpp, colour.hpp, debug_draw.hpp,
            bounds.hpp                                                       [6.8]
            cascade.hpp                                                      [6.9]
            mipmap.hpp                                                      [6.10]
            blend.hpp                                                       [6.11]
            draw_order.hpp                                                  [6.11]
            hdr.hpp                                                         [6.12]
            gpu_post.hpp                                                    [6.12]
            debug_lines.hpp                                                 [5.11]
            depth_buffer.hpp, framebuffer.hpp, gpu_buffer.hpp, gpu_debug.hpp,
            gpu_device.hpp, gpu_mesh.hpp, gpu_pipeline.hpp, gpu_present.hpp,
            gpu_scene.hpp, gpu_shader.hpp,
            gpu_shadow.hpp                                                   [6.8]
            gpu_texture.hpp, gpu_uniform.hpp,
            cull.hpp                                                         [6.5]
            gltf.hpp                                                         [6.6]
            image.hpp, light.hpp,
            material.hpp                                                     [6.5]
            mesh.hpp,
            microfacet.hpp                                              [6.3, 6.4]
            obj.hpp, projector.hpp, raster.hpp,
            scene.hpp,
            shadow.hpp                                                       [6.8]
            soft_renderer.hpp, texture.hpp, viewport.hpp
  engine/include/engine/math/: mat2.hpp, mat3.hpp, mat4.hpp, transform.hpp,
            vec2.hpp, vec3.hpp, vec4.hpp
  engine/include/engine/platform/: platform.hpp, app.hpp,
            main.hpp   (NOT in engine.hpp — it defines the entry point)
  engine/include/engine/ui/: debug_ui.hpp                                   [5.11]
            (a new directory, same argument asset/ made in 5.5: tooling UI is not a
             graphics subsystem. Does NOT include <imgui.h> — see debug-ui.)
  engine/src/asset/: search_path.cpp, asset_store.cpp                   [5.5]
  engine/src/core/: actions.cpp [5.10], clock.cpp, fixed_step.cpp, input.cpp,
            log.cpp, profile.cpp
  engine/src/gfx/: cascade.cpp [6.9], mipmap.cpp [6.10],
            blend.cpp [6.11], draw_order.cpp [6.11],
            hdr.cpp [6.12], gpu_post.cpp [6.12], clip.cpp, colour.cpp,
            debug_draw.cpp,
            debug_lines.cpp [5.11],
            depth_buffer.cpp,
            framebuffer.cpp, gpu_buffer.cpp, gpu_debug.cpp, gpu_device.cpp,
            gpu_mesh.cpp, gpu_pipeline.cpp, gpu_present.cpp, gpu_scene.cpp,
            gpu_shader.cpp,
            gpu_shadow.cpp                                                   [6.8]
            gpu_texture.cpp,
            gltf.cpp                    [6.6 — THE ONLY TU THAT SEES cgltf]
            image.cpp, mesh.cpp, obj.cpp,
            raster.cpp,
            shadow.cpp                                                       [6.8]
            soft_renderer.cpp, texture.cpp
  engine/src/platform/: platform.cpp, app.cpp
  engine/src/ui/: debug_ui.cpp   [5.11 — THE ONLY engine TU that includes <imgui.h>]
  demos/: CMakeLists.txt
  demos/common/: demo_scene.hpp, demo_scene.cpp, pong.hpp, pong.cpp
  demos/sandbox/: main.cpp
  demos/hello_cube/: main.cpp
  demos/pong/: main.cpp
  demos/ecs_swarm/: main.cpp                                              [5.8]
  demos/gltf_view/: main.cpp                                              [6.6]
  assets/: cube.obj, twisted.obj, quirks.obj, torus.obj, uv_grid.png,
           cube.gltf, cube.bin, shapes.glb                              [6.6]
           (GENERATED by scratch/make_gltf_assets.py — the course ships no
            third-party geometry, 3.5's rule, and the cube's positions are
            transcribed from k_cube_vertices so verify_66 §A is a real
            round trip. `.bin` is excepted in .gitignore for the same
            reason `.obj` is: the failure would be silent.)
  docs/: index.html, conventions.html, math-toolbox.html, cpp-style.html
  docs/lessons/: 00-01-what-is-an-engine.html, 00-02-how-this-course-works.html,
                 00-03-toolchain.html, 00-04-cmake-from-zero.html,
                 00-05-first-window.html, 00-06-headers-and-debugger.html,
                 01-01-events-properly.html, 01-02-input-state-vs-events.html,
                 01-03-delta-time.html, 01-04-fixed-timestep.html,
                 01-05-framebuffer.html, 01-06-colour.html,
                 01-07-vectors-2d.html, 01-08-pong.html,
                 02-01-lines.html, 02-02-triangle-edge-functions.html,
                 02-03-barycentric.html, 02-04-attribute-interpolation.html,
                 02-05-matrices.html, 02-06-mat4.html,
                 02-07-homogeneous.html, 02-08-space-chain.html,
                 02-09-view-matrix.html, 02-10-perspective.html,
                 02-11-viewport.html, 02-12-wireframe-mesh.html,
                 03-01-z-buffer.html, 03-02-perspective-correct.html,
                 03-03-near-plane-clipping.html, 03-04-back-face-culling.html,
                 03-05-obj-loader.html, 03-06-normals-and-lambert.html,
                 03-07-specular-blinn-phong.html, 03-08-shading-models.html,
                 03-09-textures.html, 03-10-profiling-capstone.html,
                 04-01-how-gpus-work.html, 04-02-sdl-gpu-model.html,
                 04-03-shader-toolchain.html, 04-04-first-triangle.html,
                 04-05-vertex-buffers.html, 04-06-uniforms.html,
                 04-07-textures-and-depth.html,
                 04-08-porting-the-scene.html,
                 04-09-renderdoc.html,
                 05-01-the-refactor.html,
                 05-02-platform-layer.html,
                 05-03-logging-and-errors.html,
                 05-04-handles.html, 05-05-asset-system.html,
                 05-06-data-oriented-design.html,
                 05-07-ecs-storage.html,
                 05-08-ecs-runtime.html,
                 05-09-transform-hierarchy.html,
                 05-10-input-mapping.html,
                 05-11-imgui-debug-draw.html,
                 06-01-linear-and-srgb.html,
                 06-02-what-a-brdf-is.html,
                 06-03-microfacet-theory.html,
                 06-04-cook-torrance.html,
                 06-05-material-system.html,
                 06-06-gltf.html,
                 06-07-normal-mapping.html,
                 06-08-shadow-mapping.html
  docs/shared/: course.css, course.js      (THE stylesheet + page script; one copy each)
  docs/_template/: lesson-template.html, README.md, apply-shared.py, check-page.js
  scratch/ (5.7, not shipped with the engine): ecs_probe.hpp, bench_57.cpp,
           verify_57.cpp, measure_57.py, build_bench_57.sh, build_verify_57.sh,
           figs_57.py, build_57.py, l57_body_{a,b}.html, l57_fig{1..6}.svg
  scratch/ (5.8, not shipped with the engine): verify_58.cpp, build_verify_58.sh,
           figs_58.py, build_58.py, l58_body_{a,b}.html, l58_fig{1..6}.svg,
           l58_ecs_swarm.cpp  (A PINNED LISTING — see the note in build_58.py:
           5.9 rewrote demos/ecs_swarm/main.cpp, and re-running build_58.py to
           repoint one nav link spliced 5.9's demo into 5.8's page. The snapshot is
           byte-identical to that file at commit dfbdb0f. ANY FILE A LATER LESSON
           MODIFIES MUST BE PINNED THE SAME WAY.)
  scratch/ (5.10, not shipped with the engine): verify_510.cpp,
           build_verify_510.sh, figs_510.py, build_510.py,
           l510_body_{a,b,c}.html, l510_fig{1..5}.svg,
           l510_ecs_swarm.cpp AND l510_actions.hpp — TWO PINNED LISTINGS, both
           byte-identical to commit c697e14. The demo was expected. actions.hpp was
           NOT, and that is the lesson: it was 5.10's OWN NEW HEADER, so it read as
           finished, and 5.11 added masked_input to it. Rebuilding 5.10 spliced a
           class that mentions Lesson 5.11 into Lesson 5.10's listings (+173 lines,
           caught by `git diff` on the page).
           THE RULE IS NOT "PIN THE DEMO" — IT IS: PIN EVERY FILE THE PAGE LISTS
           THAT A LATER LESSON TOUCHES, and the cheap way to find out which is to
           re-run the builder and diff the page BEFORE shipping.
  scratch/ (5.9, not shipped with the engine): hier_probe.hpp, bench_59.cpp,
           bench_59.log, verify_59.cpp, build_bench_59.sh, build_verify_59.sh,
           figs_59.py, build_59.py, l59_body_{a,b,c}.html, l59_fig{1..6}.svg,
           l59_ecs_swarm.cpp  (A PINNED LISTING, byte-identical to that file at
           commit c1c5ea7 — 5.10 rewrote the demo, so build_59.py must not read the
           live one. SAME TRAP AS build_58.py; see its comment.)
           (NOTE: build_57.py was amended in 5.8 — it no longer stamps a STATE
            block, and it now byte-reproduces the shipped 05-07 page. Any future
            build_NN.py copied from it inherits the correct form.)
  scratch/ (6.1, not shipped with the engine): verify_61.cpp, build_verify_61.sh,
           figs_61.py, build_61.py, l61_body_{a,b,c}.html, l61_fig{1..6}.svg,
           probe_61.cpp (a THROWAWAY that established the facts before a line of
           the lesson was written: it printed the default swapchain format, asked
           which compositions this window supports, and tabulated what raw linear
           values look like when read as codes. Kept because "write the probe
           first" is the habit, not the file.)
           PINNED BY 6.2, BEFORE A LINE OF 6.2 WAS WRITTEN — and this is the first
           time the rule was applied on time rather than after a diff caught it.
           ALL SIX repository listings frozen at 373dd4b and verified byte-identical:
           l61_gpu_device.{hpp,cpp}, l61_gpu_uniform.hpp, l61_scene.frag.hlsl,
           l61_colour.{hpp,cpp}. Two were certain to be touched (gpu_uniform.hpp,
           scene.frag.hlsl); the other four were pinned anyway, because the file
           that bites you is the one you were sure was finished. Re-running
           build_61.py then produced a diff of exactly the four nav lines that were
           MEANT to change, which is what "pinned correctly" looks like.
  scratch/ (6.2, not shipped with the engine): verify_62.cpp, build_verify_62.sh,
           figs_62.py, build_62.py, l62_body_{a,b,c}.html, l62_fig{1..6}.svg,
           probe_62.cpp and probe_62b.cpp (THROWAWAYS, kept: the first established
           that pi*inv_pi is exactly 1.0f and that no 8-bit code moves; the second
           was written BECAUSE THE FIRST OVERTURNED A GUESS — I expected the raw
           Blinn lobe to break energy conservation at the shininess values the
           engine uses, and it does not, so 62b went looking for the failure that
           does survive and found it in the un-coupled SUM. Write the probe first,
           and write a second one when it tells you something you did not expect.)
           figview/ — one throwaway HTML page per figure, because scrolling a
           220 KB page to look at figure 5 lands unpredictably (5.11's note).
           PINNED BY 6.3, before a line of it was written. All four listings frozen:
           l62_light.hpp, l62_scene.frag.hlsl, l62_gpu_uniform.hpp and
           l62_verify_62.cpp. The three TRACKED files were verified against commit
           3507837 with `git show ... | diff - <pin>`; verify_62.cpp is GITIGNORED,
           so its pin is only a working-tree copy taken before the first edit —
           weaker provenance, worth knowing, and the reason to take it early.
           Rebuilding then gave a diff of exactly the two nav lines meant to move.
  scratch/ (6.3, not shipped with the engine): verify_63.cpp, build_verify_63.sh,
           figs_63.py, build_63.py, l63_body_{a,b,c}.html, l63_fig{1..6}.svg,
           probe_63.cpp and probe_63b.cpp (THROWAWAYS, kept. The first asked six
           questions; the second existed because the first's answers raised two
           DESIGN questions the lesson could not settle by taste — is the
           height-correlated Smith form worth shipping beside the separable one
           (yes: 1.715x), and what does the specular BRDF's own energy budget look
           like (0.3069 at full roughness). Same habit as 6.2: write another probe
           the moment one tells you something you did not expect.)
           figview/ — one throwaway page per figure; g1..g6.html this lesson.
           PINNED BY 6.4, before a line of it was written: l63_microfacet.hpp
           (verified against commit 8735ba5 with `git show ... | diff - <pin>`) and
           l63_verify_63.cpp, a working-tree copy taken FIRST because it is
           gitignored and that provenance cannot be recovered later. Rebuilding gave a diff of exactly the two nav
           lines meant to move — THIRD LESSON RUNNING, so it is the standard now.
  scratch/ (6.4, not shipped with the engine): verify_64.cpp, build_verify_64.sh,
           figs_64.py, build_64.py, l64_body_{a,b,c}.html, l64_fig{1..6}.svg,
           l64_golden_905BF27E.ppm (THE RETIRED GOLDEN, kept as history — the
           picture the engine made for fourteen lessons), worked_64.cpp (a
           throwaway that checks every worked example the page prints against the
           engine itself; CLAUDE.md §10 requires the arithmetic verified, and
           doing it by hand twice is not verification),
           probe_64.cpp, probe_64b.cpp and probe_64c.cpp (THROWAWAYS, kept, and the
           chain is the point. 64 measured the Jacobian, Schlick and the energy,
           and OVERTURNED THE PLAN: I expected Fresnel coupling to close the energy
           door outright, and it leaves 1.2031 at grazing. 64b asked whether that
           was the instrument (refine the grid: it CONVERGES, so no) and where it
           sat (the specular takes 0.40, the diffuse gives up 0.05). 64c tested the
           resulting DIAGNOSIS — that the missing factor is the light which fails
           to get OUT — by predicting an exit factor would kill it. It did, and
           then failed reciprocity, which is what chose the shipped form. THREE
           PROBES, EACH BECAUSE THE LAST ONE DISAGREED WITH ME.)
           check-pages.mjs is shared, not per-lesson; shot_figs.mjs likewise.
           PINNED BY 6.5, before a line of it was written: l64_microfacet.hpp,
           l64_light.hpp and l64_scene.frag.hlsl, all verified against commit
           e394559 with `git show ... | diff - <pin>`; l64_verify_64.cpp was taken
           during 6.4 itself, since it is gitignored.
           AND THE REBUILD CAUGHT SOMETHING THE RULE WAS NOT AIMED AT: three stale
           "Lesson 6.7" references (glTF is 6.6) that had been fixed in the SOURCES
           after the last build and never rebuilt into the page. The generalisation
           is wider than pinning — ANY GENERATED ARTIFACT NEEDS A
           REGENERATE-AND-DIFF AFTER THE LAST EDIT TO ITS INPUTS, not only when you
           remember to pin.
  scratch/ (6.12, not shipped with the engine): verify_612.cpp,
           build_verify_612.sh, figs_612.py, build_612.py,
           l612_body_{a,b,c}.html, l612_fig{1..6}.svg,
           probe_612.cpp (a THROWAWAY, and it earned its keep twice: it
           established the peaks the lesson opens with, and its FIRST version was
           WRONG in an instructive way — it swept the eye through a plane that did
           not contain the mirror direction, so SHARPER surfaces reported LOWER
           peaks. A monotonic relationship coming out backwards is the diagnostic
           that a measurement is missing the thing it measures).
           PINNED BY 6.12, before a line of it was written: l611_blend.hpp,
           l611_blend.cpp, l611_draw_order.hpp, l611_draw_order.cpp against
           commit adec68d, plus l611_verify_611.cpp as a working-tree copy. THE
           REBUILD DIFF WAS EXACTLY THE TWO NAV LINES, eleventh lesson running —
           and the prediction was RIGHT: 6.12 did move blend.hpp (a doc note about
           where 6.11's rule applies), so an unpinned build_611.py would have
           spliced it into 6.11's page.
           NEW TOOL, and it found three SHIPPED defects on its first run:
           `figOrder` in docs/_template/check-page.js, which verifies that figure
           captions read 1, 2, 3… in DOM order. 6.12's own first build had the
           resolve diagram at 4 and the per-channel one at 5 while the body showed
           them the other way round — the SECOND time this drift has shipped
           (6.10 rendered fig4.png captioned "Figure 5") — and it happens because
           the NUMBERS live in build_NN.py and the ORDER lives in the body
           fragments. Sweeping every lesson page then found 02-05-matrices (3/4
           swapped), 03-10-profiling-capstone (4,5,6 as 6,4,5) and
           04-01-how-gpus-work (1,2,3,4 as 4,1,3,2). NOT FIXED IN THIS SESSION —
           out of scope, and the fix must edit the BODY FRAGMENTS and the prose's
           numbered references, not the rendered pages. Flagged as a task.
           (build_612.py PINS NOTHING YET and lists SEVEN files whole. 6.13 is
            bloom and the post-processing STACK, which is the second user of
            gpu_post.{hpp,cpp} — and that header says outright it is "deliberately
            not a stack" and that 6.13 asks the ownership question. Those two are
            near certain to move and hdr.{hpp,cpp} are likely (a bright-pass
            threshold and a downsample chain are both HDR operations wanting a
            home). fullscreen.vert.hlsl should NOT move, which makes it the
            control.)
  scratch/ (6.11, not shipped with the engine): verify_611.cpp,
           build_verify_611.sh, figs_611.py, build_611.py,
           l611_body_{a,b,c}.html, l611_fig{1..6}.svg, shot_figs.mjs (NEW, and
           SHARED: screenshots every `figure.dia` of a served page at 1280, for
           the visual pass check-page.js cannot do — 6.10 shipped two invisible
           markers for three build cycles because a line that was never drawn
           cannot overlap anything).
           PINNED BY 6.11, before a line of it was written: l610_mipmap.hpp,
           l610_mipmap.cpp against commit cf2d208, plus l610_verify_610.cpp as a
           working-tree copy. The prediction was RIGHT this time — 6.11 grew
           `average_2x2` a weight per texel and added `mip_options`, so an
           unpinned build_610.py would have spliced 6.11's code into 6.10's page.
           THE REBUILD DIFF WAS EXACTLY THE TWO NAV LINES, tenth lesson running.
           TWO FIGURES WERE REDRAWN AFTER check-page.js REJECTED THEM: figures 3
           and 5 had annotations placed in what looked like empty regions of a
           plot, and six of them landed on a polyline. Both now put their legend
           OUTSIDE the plot box. "Empty" judged by eye on a diagram whose curves
           cross most of the box is not a measurement.
           (build_611.py PINS NOTHING YET and lists FIVE files whole — all four
            new engine files plus the harness. 6.12 is HDR and tonemapping, which
            changes what a colour target IS, and `blend_over` clamps into [0,1]
            through `to_encoded` — so blend.{hpp,cpp} are near certain to move.
            Pin all four before writing a line of 6.12, and take verify_611.cpp
            early since it is gitignored.)
  scratch/ (6.8, not shipped with the engine): verify_68.cpp, build_verify_68.sh,
           figs_68.py, build_68.py, l68_body_{a,b,c}.html, l68_fig{1..7}.svg.
           SEVEN figures, not six, because §3.5 wants the ARTEFACT shown before
           the derivation and acne deserves its own picture. Figure 4 is a mock
           whose fringes are COMPUTED: acne on a ground plane is the level sets of
           a projective coordinate, which is why it swirls rather than striping,
           and drawing them from that formula is the difference between a diagram
           and a doodle.
           NO PROBE, the fourth lesson running. verify_68 needs a headless
           gpu_device (SDL_Init(VIDEO) first — without it the device silently does
           not come back and §G skips rather than fails, which is the most
           misleading kind of silence a harness can produce).
           PINNED BY 6.8, before a line of it was written: l67_texture.hpp,
           l67_mesh.cpp, l67_material.hpp and l67_scene.frag.hlsl against commit
           b5cbda8, plus l67_verify_67.cpp as a working-tree copy. THE REBUILD
           DIFF WAS EXACTLY THE TWO NAV LINES, seventh lesson running.
           AND ONE OF THE PREDICTIONS WAS WRONG, which is worth recording:
           material.hpp did NOT need a shadow-casting flag (whether an object
           casts is a property of the DRAW LIST, not of the surface — the demo
           expresses it with a subspan), and texel_space did NOT need a third
           value (a CPU shadow map is a depth_buffer of floats, not a texture of
           Uint32, so the question never arises). Only scene.frag.hlsl was
           certain, and it was.
           (build_68.py PINS NOTHING YET and lists EIGHT files whole. 6.9 is
            cascades and will edit shadow.{hpp,cpp}, gpu_shadow.{hpp,cpp},
            scene.frag.hlsl and probably bounds.hpp. Pin first, and take
            verify_68.cpp early.)
  scratch/ (6.7, not shipped with the engine): verify_67.cpp, build_verify_67.sh,
           figs_67.py, build_67.py, l67_body_{a,b,c}.html, l67_fig{1..6}.svg.
           NO PROBE, the third lesson running — every measurement had an answer
           derivable on paper first, so they went into verify_67 as assertions
           rather than into a probe as questions. NO NEW ASSET EITHER: the normal
           map is GENERATED by make_normal_bumps and inserted through
           insert_texture, which is 5.5's "an asset system that can only load is
           missing half its job" applied to the newest asset type.
           PINNED BY 6.7, before a line of it was written: l66_gltf.hpp,
           l66_gltf.cpp, l66_asset_store.hpp and l66_asset_store.cpp, all verified
           against commit 2c787a2 with `git show ... | diff - <pin>`;
           l66_verify_66.cpp and l66_make_gltf_assets.py taken as working-tree
           copies since both are gitignored. THE REBUILD DIFF WAS EXACTLY THE TWO
           NAV LINES, sixth lesson running.
           AND THE PINNING CAUGHT SOMETHING ELSE: build_66.py had inherited 6.5's
           THREE PINS verbatim when it was copied from build_65.py, and they sat
           inert for a whole lesson because none of those paths appeared in its
           LISTING_META. Harmless — and it made the discipline LOOK satisfied.
           WHEN COPYING build_NN.py, EMPTY LISTING_SOURCE AS WELL AS FIGURES.
           (build_67.py PINS NOTHING YET and lists texture.hpp, mesh.cpp,
            material.hpp, scene.frag.hlsl and verify_67.cpp WHOLE. LESSON 6.8 IS
            SHADOW MAPPING: scene.frag.hlsl is certain (the depth comparison is a
            fragment), material.hpp is likely (a shadow-casting flag is per-object
            state and 6.5's rule has to sort it), and texture.hpp is plausible — a
            depth map is neither colour nor ordinary data, so texel_space may need
            a third value. Pin first, and take verify_67.cpp early.)
  scratch/ (6.6, not shipped with the engine): verify_66.cpp, build_verify_66.sh,
           figs_66.py, build_66.py, l66_body_{a,b,c}.html, l66_fig{1..6}.svg,
           make_gltf_assets.py (NOT a throwaway — it is how assets/cube.gltf and
           assets/shapes.glb are produced, and re-running it must reproduce them
           byte-for-byte; it is gitignored with the rest of scratch/, which is a
           debt worth naming, because the assets it generates ARE committed).
           NO PROBE THIS LESSON, the second after 6.5. The measurements that
           mattered (the f0-lerp identity, the coupling ratio, the metal peak
           radiance) all had answers derivable on paper first, so they went
           straight into verify_66 §E as assertions rather than into a probe as
           questions. That is the right shape when you can predict the answer —
           6.2's rule was to write a probe the moment the first one disagrees with
           you, and none did.
           PINNED BY 6.6, before a line of it was written: l65_material.hpp and
           l65_scene.hpp, both verified against commit ebb3199 with
           `git show ... | diff - <pin>`; l65_verify_65.cpp taken as a working-tree
           copy since it is gitignored. THE REBUILD DIFF WAS EXACTLY THE TWO NAV
           LINES, fifth lesson running.
           (build_66.py PINS NOTHING YET and lists gltf.hpp, gltf.cpp,
            asset_store.hpp and asset_store.cpp WHOLE. LESSON 6.7 IS NORMAL
            MAPPING and needs the thing 6.6 deferred — a texture that knows whether
            it holds colour or linear data — so it will edit ALL FOUR: the
            wants_normal_texture flag becomes a real load, and load_texture grows a
            colour-space argument. Pin before writing a line of 6.7, and take
            verify_66.cpp AND make_gltf_assets.py early since both are gitignored.)
  scratch/ (6.5, not shipped with the engine): verify_65.cpp, build_verify_65.sh,
           figs_65.py, build_65.py, l65_body_{a,b,c}.html, l65_fig{1..5}.svg,
           check-tags.py (SHARED, and new: see below),
           shot_65_*.ppm and swarm_65_*.ppm (the three-stage and clean-build
           comparisons; throwaways, kept as the evidence for the refactor claim).
           NO PROBE THIS LESSON — the first since 6.1. A refactor's question is
           "did the output move?", which the golden answers directly; there was no
           measurement whose answer could surprise. The one number that DID need
           taking was the header compile times, and taking it overturned the
           argument it was meant to support (see conventions:material).
           (build_65.py PINS NOTHING YET and lists material.hpp and scene.hpp
            WHOLE. LESSON 6.6 LOADS glTF and will most likely grow material.hpp —
            a material from a file needs a name, a cache and an identity that
            survives a reload. Pin before writing a line of 6.6, and take
            verify_65.cpp's copy EARLY since it is gitignored.)
  scratch/ (5.11, not shipped with the engine): verify_511.cpp,
           build_verify_511.sh, figs_511.py, build_511.py,
           l511_body_{a,b,c}.html, l511_fig{1..7}.svg
           (figs_511.py adds peak_sample() — a MAX downsampler, because 5.7's
            box_sample AVERAGES and a 1-pixel debug line inside a 3x3 block
            contributes one ninth of its brightness, so 152 crisp lines average
            into a haze. It also floors dim cells to true black, because
            figs_45.quantise() calls a cell background only when ALL THREE channels
            are under 14 and this demo's background is (12, 14, 20) — blue is 20,
            so every empty cell snapped to a tint and the panel came out solid
            slate with the lines invisible inside it.
            build_511.py PINS NOTHING YET. It lists BOTH CMakeLists.txt files, the
            umbrella header and actions.hpp, and Module 6 will edit at least the
            first three — so it will need pins, and the warning in it says so.)
  memory/: 2026-07-16.md … 2026-08-25.md, 2026-08-25-b.md, 2026-08-25-c.md,
           2026-08-26.md, 2026-08-26-b.md, 2026-08-29.md, 2026-09-02.md, 2026-09-02-b.md,
           2026-09-02-c.md, 2026-09-04.md, 2026-09-05.md, 2026-09-06.md, 2026-09-07.md,
           2026-09-08.md
           (ONE FILE PER DATE. 6.2, 6.3 and 6.4 all landed on 2026-09-06 and all
            three are sections of that one file — never a -b suffix for a same-day
            session. 6.7 and 6.8 both landed on 2026-09-08 and share that file the
            same way.)
  (retired: src/ — the whole directory. hello.cpp.)


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


next: 6.13 — Bloom and the Post-Processing Stack
      (planned filename: docs/lessons/06-13-bloom-post-stack.html — 6.12's TWO
      next links point at the index and BOTH need repointing;
      scratch/l612_body_a.html holds the top one and build_612.py's TAIL the
      bottom.
      PIN FIRST. build_612.py's LISTING_SOURCE is EMPTY and correctly so — every
      file it lists whole, 6.12 created. It lists SEVEN: hdr.hpp, hdr.cpp,
      gpu_post.hpp, gpu_post.cpp, fullscreen.vert.hlsl, tonemap.frag.hlsl and
      verify_612.cpp. The command:
        for f in engine/include/engine/gfx/hdr.hpp \
                 engine/src/gfx/hdr.cpp \
                 engine/include/engine/gfx/gpu_post.hpp \
                 engine/src/gfx/gpu_post.cpp \
                 shaders/fullscreen.vert.hlsl \
                 shaders/tonemap.frag.hlsl; do
          git show <6.12 commit>:$f > scratch/l612_$(basename $f)
        done
        cp scratch/verify_612.cpp scratch/l612_verify_612.cpp   # gitignored
      Then re-run build_612.py and `git diff` the page: ELEVEN lessons running,
      the diff has been exactly the nav lines meant to move.

      WHY gpu_post.{hpp,cpp} ARE CERTAIN TO MOVE. `gpu_post.hpp` says in its own
      words that it is "deliberately not a post-processing stack", that it is one
      pass with one input, and that 6.13 is where "how do several of these
      compose, and who owns the intermediate targets?" gets asked properly. That
      is a promise on record, in a shipped public header, and 6.13 is where it
      comes due — the same shape as the mipmap debt 6.10 paid and the alphaMode
      gap 6.11 closed.

      WHAT 6.13 OWES, beyond the obvious:
        1 THE ORDERING ARGUMENT, WHICH IS THE WHOLE REASON BLOOM IS NEXT. A bloom
          operates on the PRE-TONEMAP image, because it is looking for exactly
          the values above 1 that 6.12 finally lets the engine keep. So the stack
          is not "a list of passes" — it is a list with a constraint, and the
          constraint comes from physics rather than from taste. Say why a bloom
          applied after the curve looks wrong (everything bright is already at
          the same value, so the bloom has nothing to select).
        2 WHO OWNS THE INTERMEDIATE TARGETS. A bloom is a downsample chain and a
          blur, so it needs several targets at several sizes, reused across
          frames. That is the question 6.12 declined to answer with one user, and
          it is the SAME question 6.17's frame graph answers at a larger scale —
          so 6.13 should build the small honest version and name the larger one
          rather than pre-empting it.
        3 THE PIPELINE COUNT, AGAIN. 6.11 made it nine; 6.12 added a resolve
          pipeline against a second colour format. Every post pass is another
          one, and this is where the enumerate-versus-hash argument stops being a
          curiosity. 6.12 §4.6 set that up deliberately.
        4 THE GOLDEN. Same structural answer as 6.12's should hold — a bloom is a
          stage over the HDR target and the fixture has no HDR target — but
          CHECK IT EARLY rather than assuming, because a stack that generalises
          the resolve could plausibly reach into the LDR path.
      CARRY FORWARD: 6.12's habit of opening by measuring what the engine was
      ALREADY doing wrong (or already avoiding) before proposing anything, and
      6.11's of naming a technique's ceiling with a number rather than a hedge.

      (RESOLVED 2026-09-11, in a follow-up session.) `check-page.js`'s new
      `figOrder` check found out-of-order figure numbers in THREE published
      lessons — 02-05-matrices, 03-10-profiling-capstone, 04-01-how-gpus-work —
      and all three are fixed by RENUMBERING, not by moving figures: in every one
      the figure already sat in the section that discusses it, so only the numbers
      were out of step. Whole site now green, 73 pages x 2 widths.
      WHAT THAT SWEEP UNCOVERED IS BIGGER THAN THE FIGURES, and it is recorded
      here because it will bite again:
        1 build_310.py AND build_41.py STILL STAMPED A `STATE` BLOCK, retired
          from lesson pages at 5.7. Re-running either would have re-added 60% of
          a file. Amended, as build_57.py was in 5.8.
        2 BOTH READ THEIR LISTINGS FROM `src/`, the directory Module 5's refactor
          DELETED. They had been unreproducible since 5.1 and nobody had noticed,
          because nobody had needed to rebuild them. Now pinned from the commits
          that shipped each lesson (26cd723, b9bedf0) and byte-identical again.
        3 THE RENDERED PAGES WERE MORE CORRECT THAN THEIR SOURCES: the 2026-09-08
          Module 8->9 renumber (3 sites) and 4.1's `next` nav link had been
          applied to the shipped HTML and never to the body fragments. A rebuild
          would have reverted all four. THIRD time this drift has been found.
        4 LESSON 2.5 HAS NO GENERATOR — the build_NN.py pipeline starts at 3.7,
          so for that page the rendered HTML IS the source. Do not assume a
          builder exists before looking.
      THE ORDER THAT MADE THIS SAFE: prove the builder reproduces the shipped
      page BYTE-IDENTICALLY first, then make the intended change, then diff. Both
      final diffs were exactly caption numbers and prose references.
```
