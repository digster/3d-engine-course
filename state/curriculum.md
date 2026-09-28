# Curriculum — counts, reshapes and their reasons

Moved verbatim from STATE.md's `curriculum:` block on 2026-09-27, when that file became
a compact resume key (CLAUDE.md §9). Append here; keep STATE.md's headline in step.

```text
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
        1-F(v.h) is glTF's reference coupling (Filament does not couple at all) and is EXACT at normal incidence
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

  environment: THE AMBIENT TERM IS NO LONGER A CONSTANT. Built in 6.15, in
        engine/gfx/cubemap.{hpp,cpp}. `lighting::ambient` STILL EXISTS and is
        still the fallback; an `environment` REPLACES it when one is supplied.
        THE FOUR OBJECTS, and they are only correct TOGETHER (one struct):
          radiance     the sky itself, six hdr_buffer faces. The skybox draws it.
          irradiance   integral 1, indexed by NORMAL. 32^2 is plenty.
          prefiltered  integral 2's first bracket, a mip chain indexed by
                       ROUGHNESS (not by footprint — that is 6.10's chain).
          brdf_lut     integral 2's second bracket. 64^2, texel_space::linear,
                       and it depends on NEITHER the environment NOR the
                       material, because Schlick is LINEAR in F0.
        ONLY THE DIFFUSE HALF IS EXACT, and this is the sentence to carry:
        Lambert's BRDF does not depend on l, so it leaves the integral entirely.
        Second-order convergence measured (/4.08, /3.92, /3.98 per doubling) to
        1.1e-5 at 64^2, and `image_based_light` reproduces 6.2's
        `albedo * ambient` END TO END to 1.291e-05 on a uniform sky. THE OLD
        TERM IS A SPECIAL CASE, NOT A CASUALTY — and that is the regression test
        that licenses trusting the rest.
        THE SPLIT SUM IS AN INTEGRAL OF A PRODUCT REPLACED BY A PRODUCT OF
        INTEGRALS, and its error HAS A SHAPE (measured against brute force at
        1e6 samples, against the SAME cube the engine reads):
          roughness 0.10           0.08% — essentially exact
          n.v 0.9, any roughness   <= 5.8% dark
          n.v 0.4, roughness 1     29.0% dark
          with a sun disc          73.0% dark
        The four-fold normal/grazing asymmetry IS `n = v = r`: a prefiltered
        value is indexed by ONE direction while the true integral needs two, so
        the chain bakes its lobe around the REFLECTION, which at a grazing view
        points across the horizon and averages in ground the surface never sees.
        The sun case is a THIRD mechanism — the prefilter weights by n.l where
        the true integral weights by the whole BRDF. **THE ERROR OF THIS
        TECHNIQUE DEPENDS ON THE ENVIRONMENT, NOT ONLY ON THE MATERIAL**, which
        is not in the paper, and is why production keeps the sun as a separate
        analytic light. Our key light already is one.
        THE MEASUREMENT IS DECOMPOSED, not quoted: brute force runs twice, once
        against the analytic sky and once against the BAKED cube, so the bake's
        error (buyable with texels) is separated from the approximation's (not).
        On a smooth sky the bake column is 1.0000 throughout.
  cube-faces: THE TABLE IS WRITTEN ONCE, AS DATA, and both directions read it —
        so a typo cannot appear in only one of them. sc/tc/ma with signs, exactly
        the D3D and OpenGL spec's. `engine::cube_face` is enumerator-for-
        enumerator SDL_GPUCubeMapFace and verify_615 ASSERTS it, the same
        discipline 3.9's `filter` gets, so the face index goes straight into
        SDL_GPUTextureRegion::layer. The major axis sign is the LOW BIT of the
        face index, which that ordering buys for free.
        THE HANDEDNESS MIRROR IS REAL AND MEASURED, not assumed: the table was
        written for a LEFT-handed system and conventions §2 pins this world
        right-handed. Looking along +X, the world's "right" is +Z and the cube's
        u axis is -Z — dot -1.000. **Faces baked with a right-handed camera come
        out left-right flipped.** The fix is NOT to correct the table (hardware
        implements it) but to generate faces with the axes it names;
        `make_sky_environment` does that by construction.
  solid-angle: A CUBE TEXEL'S SOLID ANGLE IS NOT UNIFORM AND EVERY INTEGRAL MUST
        WEIGHT BY IT. dw = dA cos(theta)/r^2 = dA/r^3, because the tilt cosine
        is itself 1/r. Centre r=1, corner r^3 = 3*sqrt(3) = 5.196, so THE CENTRE
        TEXEL IS WORTH 5.196 CORNER TEXELS. Measured 0.19321 at 512, converging
        to the derived 1/(3*sqrt(3)) = 0.19245.
        WHAT IGNORING IT COSTS IS 1.01%, AND IT IS A BIAS: 1.010005 at 16^2 and
        1.010066 at 64^2. Sixteen times the texels and the error does not move.
        A discretisation error is a number you can buy down with memory; a bias
        is one that is still there when the memory is spent. That distinction is
        why `cube_texel_solid_angle` is a named public function rather than an
        expression inside one loop.
        THE IMPLEMENTATION USES LAMBERT'S CLOSED FORM, not dA/r^3, because a
        texel at 8^2 is 22.5 degrees across and the small-texel approximation is
        not good there. f(x,y) = atan2(xy, sqrt(x^2+y^2+1)) differenced at four
        corners. The check that it is right: six faces sum to 4*pi.
  prefilter-level: THE ROUGHNESS -> MIP MAPPING IS DERIVED, NOT CHOSEN, and this
        is 6.14's `ggx_lobe_half_angle` cashing a cheque. Turn the lobe into a
        SOLID ANGLE (2*pi*(1-cos t), the spherical cap) because a half-angle is
        not commensurable with a texel and a steradian is; a level-k texel is
        w0*4^k; set equal and k = 0.5*log2(lobe/w0).
        w0 IS THE CENTRE TEXEL, and the choice must be stated: the corner would
        shift every answer by log4(5.196) = 1.19 levels, MORE than the entire
        disagreement being measured.
        AND THE FOLKLORE IS NOT ARBITRARY. `roughness * (levels-1)` is within
        0.770 of a level everywhere (worst at roughness 0.45) and exact at both
        ends; sqrt(roughness) is off by 2.214. But now the error has a
        DIRECTION: at roughness 0.10 the lobe wants level 0 and the map reads
        0.700, so a near-mirror is OVER-blurred; from 0.3 to 0.9 it UNDER-blurs.
        THE ENGINE SHIPS THE FITTED ONE AND ALSO THE DERIVED ONE, boundary
        marked — 6.14's discipline. The lookup MUST use the linear map because
        the chain was BUILT that way; `prefilter_level_for`'s job is to tell you
        whether your chain is deep enough, not to index it.
        A HARD LIMIT HIDES IN THE SAME TABLE: at roughness 0.10 the lobe is
        1.3e-4 sr and a level-0 texel on a 128 face is 2.44e-4 sr — THE LOBE IS
        SMALLER THAN ONE TEXEL. Below roughness ~0.11 the base resolution, not
        the prefilter, is the limit. A crisper mirror wants a bigger cube, not a
        deeper chain.
  two-fitted-steps: 6.15 HAS EXACTLY TWO FITTED STEPS AND BOTH ARE MARKED IN THE
        SOURCE, which is 6.14's habit applied again. (1) The linear
        roughness-to-level map, above. (2) The prefilter weighting radiance by
        n.l — importance sampling already accounts for D and the Jacobian, and
        G and Fresnel belong to the OTHER bracket, so putting them here would
        double-count; n.l is what is left, and Karis (2013) documents it as
        looking better rather than as following from anything.
  half-float: binary16 IN hdr.{hpp,cpp}, AND IT DID NOT EXIST BEFORE 6.15 FOR A
        REASON WORTH KNOWING: everything HDR so far was PRODUCED on the GPU, so
        no half ever crossed the CPU boundary. An environment map is the first
        HDR data computed on the CPU that must be uploaded.
        1 sign, 5 exponent (bias 15), 10 mantissa. Worst relative error 4.85e-4
        across 1e-4..6e4, which is 2^-11 — half an ulp, the best a correctly
        rounding conversion can do, and the number that proves it rounds to
        nearest EVEN rather than truncating (truncation biases every value down
        by half a step, a systematic darkening).
        THREE NON-COMMON CASES, each with a visible artefact if skipped:
        OVERFLOW CLAMPS to 65504 (an inf poisons a whole mip level with NaN);
        NaN STAYS NaN (hiding it hides the bug that made it); SUBNORMALS below
        2^-14 (rounding them to zero quantises the shadows, invisible until you
        tonemap at low exposure).
  cube-gpu: SDL_GPU_TEXTURETYPE_CUBE, layer_count_or_depth EXACTLY 6, and the
        six layers ARE the six faces — so the upload loop is 6.9's cascade-array
        code with a bound of 6. Format is ASKED FOR with the TYPE in the query
        (a device can accept a format as 2D and refuse it as a cube), and an
        unsupported one FAILS rather than falling back: unlike a sample count
        there is no sensible cheaper answer.
        THE CHAIN IS UPLOADED, NOT GENERATED — the opposite of
        create_sampled(mips=true) two functions up, and the difference is the
        whole lesson. SDL_GenerateMipmapsForGPUTexture box-filters, which is
        right when levels mean "this texture, smaller"; these mean "this
        environment, blurred by a GGX lobe of roughness r".
        SIX FRAGMENT SAMPLER SLOTS NOW (was three). 6.8's rule holds and is why
        they travel together: a partial SDL_BindGPUFragmentSamplers REPLACES the
        range it names, so slot 3 would be unbound the moment slot 0 changed.
        CORRECTED 2026-09-27: FALSE. A bind writes only the slots it names (SDL
        release-3.4.12, all three backends); bindings are cleared at the END OF A
        PASS, which is the rule that actually holds. See decisions:
        pending-code-corrections.
        THE FOURTH IDENTITY-ELEMENT FALLBACK: white for a multiply (3.9),
        lavender for a basis change (6.7), 1.0 for a depth comparison (6.8), and
        **BLACK for the ADDITION the ambient term performs**. Bound whether or
        not ibl_intensity is zero, because HLSL does not elide a resource
        because a branch did not reach it.
  ibl-uniforms: scene_light_uniforms 176 -> 192 BYTES, and the free ride ended.
        `ibl_intensity` lands in 6.8's pad2 and costs ZERO (third in a row, after
        6.7 into 6.4's padding and 6.14 into 6.11's); `ibl_max_level` costs a
        whole register. Named rather than absorbed, because three free lessons
        could reasonably set an expectation. Affordable by 6.8's argument: this
        is a PER-FRAME push, so 16 bytes a frame against 16 bytes per draw.
        IT CANNOT BE A SHADER CONSTANT: the chain's depth is chosen at run time
        by bake_environment, so a hard-coded 5 in HLSL would be a number that
        must agree with a number in C++, in another language, with nothing
        checking — the exact failure mode gpu_uniform.hpp's static_asserts exist
        to prevent.
        RENAMING pad2 CAUGHT A REAL COUPLING: gpu_shadow.cpp was zeroing it. The
        compiler found it, which is the argument for filling padding with NAMED
        fields — `pad2 = 0` and `ibl_intensity = 0` are the same store, but only
        one is a statement about the environment.
  ibl-subtraction: fill_style::env IS THE SIXTH NULLABLE-POINTER BARGAIN (after
        inv_w=1 in 3.2, lights in 3.8, albedo in 3.9, shadows in 6.8, cascades
        in 6.9) and it is what keeps the golden byte-identical.
        WHEN IT IS SET, THE CONSTANT AMBIENT MUST BE SUBTRACTED, because shade()
        has already added it. `ambient_only()` exists to be subtracted and lives
        three lines below the line it mirrors — THE SUBTRACTION IS ONLY HONEST
        IF IT IS CHARACTER-FOR-CHARACTER THE ADDITION. Forget it and every
        surface is filled twice, which reads as "IBL is too bright" and is
        "fixed" by tuning env_intensity to ~0.6 — a magic number that goes wrong
        the first time anyone changes lighting::ambient.
  broadphase: *** A BROADPHASE MAY REPORT PAIRS THAT DO NOT TOUCH; IT MAY NOT
        OMIT ONE THAT DOES. *** 8.8, engine/include/engine/phys/broadphase.hpp.
        THAT ASYMMETRY IS THE WHOLE DESIGN. A false positive costs one
        narrow-phase call — 432 ns and a CORRECT answer, so it appears in a
        profile and never in a bug report. A false negative has no stage
        downstream that could notice: the narrow phase is never called, no
        manifold is generated, and the symptom reaches the player as "sometimes
        things fall through the floor" with no line of code to blame. So every
        approximation in the file rounds OUTWARD: cell ranges round out, a proxy
        the grid cannot place is kept rather than dropped, and a candidate is
        rejected only when a cheap EXACT test says so.
        AND THE CONTRACT IS CHECKABLE, which 8.5 and 8.6 were not.
        `brute_force_pairs` answers the same question exactly for any input, so
        correctness is set equality against a function that cannot be wrong —
        12 frames of 900 tumbling proxies, 5,230 true pairs, 0 missed, 12/12
        equal. The demo runs it live every frame behind [B].
        `a < b` IS A CONTRACT, NOT TIDINESS. pair_key is order-independent and
        the manifold is not; generate (7,3) one frame and (3,7) the next and the
        normal reverses, the reference face moves, and every warm start is lost.
        Sorted once, at `emit`, the only place pairs are made.
        THE GRID IS HASHED, NOT ARRAYED, for memory and for the absence of an
        EDGE. A dense array needs the world's extent (10^9 cells for a km at a
        metre) and has a boundary that must be clamped, wrapped or rejected —
        two of which lose pairs. A collision costs a comparison and never an
        answer, because every entry carries its cell coordinate.
        AND REBUILT, NOT UPDATED. Removal is the expensive operation in every
        open scheme, most proxies move most frames, and a rebuild is two linear
        passes over contiguous memory. 8.7's manifold_cache made the same
        argument one lesson earlier: a structure thrown away every frame never
        rots. Zero allocations over 2,000 rebuilds, checked with a counting
        operator new.
        THE STORAGE IS A COUNTING SORT, NOT A VECTOR PER BUCKET. Count, prefix
        sum, scatter — one contiguous array with every bucket's members
        adjacent, and no per-cell allocation anywhere. The repeated triple loop
        that looks wasteful is cheaper than one pass that allocates.
        `owner_cell` IS EXACT. The minimum corner of the overlap of two cell
        ranges lies in both, so both proxies are there (at least once), and there
        is one minimum corner (at most once). Three max calls, no memory, against
        45.2% of the whole broadphase for a std::set of pair keys.
        THE THREE REJECTION TESTS ARE ORDERED BY COST: same cell? (a hash
        collision), owner cell? (a duplicate), boxes overlap? (six comparisons to
        avoid 432 ns). Each counted separately, so the demo can show WHY a
        candidate died.
        `max_cells_per_proxy` IS A DIAGNOSIS, NOT A CURE. An oversized proxy is
        tested against EVERYTHING — the most conservative thing available — so
        the guard changes cost and not answers (pair sets on and off are equal).
        Linear in big*n: free at one floor, most of the broadphase at 64.
        AND `margin` DEFAULTS TO ZERO AND IS 8.9's. It is measured (pairs go as
        ((E+4m)/E)^3, within 12%) and monotone (no margin ever loses a pair), and
        the decision about how far to speculate belongs to the solver that acts
        on it.

  manifold: *** A CONTACT IS A SET OF POINTS SHARING ONE NORMAL, AND THE NUMBER
        IS FOUR. *** 8.7, engine/include/engine/phys/manifold.hpp.
        FOUR IS FORCED. Four independent non-negative impulses place the
        resultant anywhere in a planar convex hull — which is what a support
        polygon IS — and a fifth is a linear combination as far as the three
        equations (force, two torque components) are concerned, so it makes the
        solver's answer NON-UNIQUE rather than better. Same fact as a
        four-legged table wobbling. `k_max_manifold_points = 4`.
        THE NORMAL POINTS FROM `a` TOWARD `b`, unchanged since 8.4, and it is
        the REFERENCE FACE's own normal rather than EPA's. Exact where the
        reference face is the one the exact MTV came from; snapped by at most
        acos(face_cos) where it is not. A normal that is piecewise constant does
        not shake a stack and one that is merely accurate does.
        A CONTACT POINT IS THE MIDPOINT of the two surface points, and either is
        recoverable exactly: A's is `position + normal*depth/2`, B's is
        `position - normal*depth/2`, WHICHEVER SHAPE SUPPLIED THE REFERENCE.
        Measuring the midpoint itself against a surface is what 8.7 §7's first
        instrument did, and it reported 0.63 m of "error" that was a fact about
        the convention.
        AN EDGE CONTACT HAS NO REFERENCE PLANE and therefore no such recovery.
        Its point is between the two surfaces and is only NEAR them at the
        depths a solver maintains. Named as a limitation in §12.
        IDENTITY, NOT POSITION. `contact_id` is six named fields and every one
        is a discrete index into geometry that does not move: a face's own id
        (0-5 for a box, a 16-bit FNV hash of the coplanar vertex set for a
        hull), a vertex's own index on its shape (`obb::corners`' order, which
        8.4's doc comment promised by name a lesson early). Nothing in it is
        computed from a position. `flipped` is part of the identity because the
        reference can move to the other shape.
        `pair_key(a, b)` IS ORDER-INDEPENDENT AND THE MANIFOLD IS NOT. Generate
        with the same shape as `a` every frame; 8.8's broadphase adopts "lower
        index first" for exactly this. Swapping the arguments reverses the
        normal, moves the reference, and costs every warm start on the pair —
        §9's control measures 0 of 4 ids surviving.
        A WINDING IS LOAD-BEARING. `support_face` returns every polygon
        counter-clockwise about its OWN outward normal (conventions.html §7),
        because the clipper builds each side plane as `cross(edge, normal)` and
        that points outward only if the winding is CCW. Reverse it and every
        plane faces inward: 0 of 4 incident vertices accepted, silently, on a
        contact that is plainly there.
        `face_cos` IS A COSINE AND BOUNDS THE NORMAL ERROR AT ITS OWN ARC
        COSINE, exactly. 0.999 by default = 2.5626 deg, measured 2.5611. Choose
        the largest normal error a solver can live with and take its cosine.
        `k_face_gather_sin` IS A SINE (0.05 = 2.87 deg) AND THERE IS NO VALUE
        THAT IS RIGHT, because a `hull` is a point cloud and the question is
        topological. Too tight and a tessellated cylinder's side contact is a
        bare edge; too loose and three flat faces merge into one bulged polygon.
        The only real fix is a face list on the collider.
        NOTHING CONSUMES THE IMPULSES YET. `normal_impulse` and
        `tangent_impulse` are declared, carried by `carry_impulses`, and written
        by nobody until 8.9/8.10. They are declared HERE because the whole point
        of the id is to make them survive.
        AND THERE IS NO SPECULATIVE MARGIN. A pair a micron short of touching
        produces `status == none`. `keep_slop` is the hook and 8.9 owns the
        decision.
```
