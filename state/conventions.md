# Conventions — the full text of every key

Moved verbatim from STATE.md's `conventions:` block on 2026-09-27, when that file became
a compact resume key (CLAUDE.md §9). Append here; keep STATE.md's headline in step.

```text
conventions:
  quat: w FIRST, w = cos(theta/2), SANDWICH q v conj(q), q*p MEANS "DO p THEN q".
        7.4, engine/include/engine/math/quat.hpp + docs/conventions.html §8e.
        `struct quat { float w; vec3 v; }` -> in memory w, x, y, z. MOST GPU
        PACKING AND A GREAT DEAL OF PUBLISHED CODE USE x, y, z, w. The two are
        incompatible and a blind memcpy across the boundary presents as a 180
        degree error about a diagonal axis. Ours follows the derivation
        (q = w + v) so that operator* reads as its own formula.
        THE TABLE IS FORCED BY ISOTROPY, not postulated. One demand — space has
        no preferred direction, so EVERY unit imaginary squares to -1, not just
        the three basis ones — gives ij + ji = 0 in three lines:
        (i+j)/sqrt2 is unit, so (i+j)^2 = 2u^2 = -2, and expanding gives
        -2 + (ij + ji). Associativity then gives all eleven remaining entries
        and ijk = (ij)k = k^2 = -1 falls out as a CONSEQUENCE. Measured: worst
        |u^2 + 1| over 20,000 axes 2.980e-07; associativity 1.788e-07.
        ANTICOMMUTATIVITY IS WHAT ISOTROPY COSTS. A commuting alternative would
        be describing a space that is not ours.
        THE DOT AND CROSS PRODUCTS ARE INSIDE THE PRODUCT.
        v1 v2 = -(v1.v2) + (v1 x v2); Gibbs and Heaviside cut them out of
        exactly this in the 1880s. Swap the operands and ONLY the cross term
        changes sign: qp - pq = (0, 2 v1 x v2). So "quaternions do not commute",
        "the cross product is antisymmetric" and "rotations of space do not
        commute" are ONE fact said three ways.
        MEASURED, TWO PERPENDICULAR QUARTER-TURNS IN THE TWO ORDERS ARE 120.0000
        DEGREES APART — derived in closed form as 2 acos|c^4 + 2c^2s^2 - s^4|,
        which at phi = 90 is 2 acos(1/2), and measured three ways that share no
        code (quaternion metric, matrix metric, closed form).
        THE HALF-ANGLE AND THE SANDWICH COME OUT OF ONE CALCULATION, and so
        does the double cover. Reflection in the plane perpendicular to unit n
        is `n v n`, which expands to v - 2(n.v)n — `vec3::reflect`, written in
        Lesson 1.8 for a bouncing ball (agreement 1.046e-06). Do it twice and
        move the brackets: n1(n0 v n0)n1 = (n1n0) v (n0n1), and n0n1 IS
        conj(n1n0) because conjugation reverses a product and negates a pure
        quaternion. So the sandwich was NOT CHOSEN. Then q = n1n0 =
        -(cos phi + sin phi n_hat), and NOTHING IN THE DERIVATION DETERMINES
        THAT SIGN — the double cover, arriving before it is named.
  clip: A CLIP IS A FUNCTION FROM TIME TO POSE, AND IT HAS NO STATE. 7.7,
        engine/include/engine/anim/clip.hpp + docs/conventions.html §8h.
        PER-CHANNEL TRACKS, EACH WITH ITS OWN ABSOLUTE TIMES — glTF's layout and
        everyone else's. `clip.tracks[j].{position,rotation,scale}`, parallel to
        skeleton::joints BY INDEX, and a clip may be SHORTER than the skeleton
        (an upper-body clip is exactly that). An EMPTY channel means the joint's
        bind value and is EXACT, not an approximation: sample an all-empty clip
        and it reproduces rest_pose bit for bit (measured 0.000e+00).
        *** THE LAYOUT IS NOT THE SAVING. *** A naive per-channel bake is 30%
        BIGGER than a pose array — 37,076 bytes against 28,520 — because a pose
        spends 40 B per joint-frame and three channels spend 16+20+16 = 52, each
        key carrying its own four-byte time. What the layout BUYS is that
        independent times let a channel be reduced or deleted without dragging
        its neighbours: 53 of 69 channels on a 23-joint walk carry NOTHING (one
        position, on the root; no scale anywhere), and the clip lands at 3,716 B,
        7.7x smaller than the pose array. 3.60 MB -> 0.48 MB at production scale.
        THE MUTABLE HALF LIVES IN THE CALLER. `std::vector<track_cursor>`, 12 B
        per joint, exactly as compose_pose's scratch arrays do. A clip is an
        ASSET — one copy, shared — so a playhead or a cursor inside it would mean
        two characters could not play the same walk out of phase.
        THREE INTERPOLATION RULES, ONE PER FIELD, in math/transform.hpp as
        `transform_blend` (nlerp) and `transform_blend_slerp`. NOT named
        transform_slerp — 7.5 §12's exercise asked for that and the name is a lie,
        since one field of three slerps. Position lerps. Rotation NLERPS: two
        adjacent keys of a 30 Hz clip are 24 deg apart even at 720 deg/s and nlerp
        lags slerp by 0.016921 deg, three orders inside 7.5's 73.50 deg threshold,
        and glTF Appendix C explicitly permits the approximation. SCALE LERPS —
        the decision 7.5 left open — because (a) the arithmetic-minus-geometric
        mean gap is 0.62% over the range content uses, (b) ln(0) is -inf and a
        scale of ZERO is how an animator hides a thing (geometric TELEPORTS
        rather than shrinks) while ln(-1) is NaN for a mirrored rig, and (c)
        glTF's LINEAR mode is componentwise lerp so an importer that log-lerps
        plays the file differently from every other viewer. THE ANSWER TO ANYONE
        WANTING GEOMETRIC GROWTH IS ANOTHER KEYFRAME: 1->8 in six keys cuts the
        59.10% gap to 1.51%, and a policy the content pipeline can express does
        not belong in the sampler.
        DURATION IS AUTHORED, NOT DERIVED. "The largest key time" puts a 30 Hz
        two-second loop's last key at 1.9667 and restarts the cycle one frame
        early: 5.8215 deg of seam, once per cycle, on an animation correct at
        every other instant. Bake `frames + 1` samples over `frames` intervals.
        ROOT MOTION IS NOT A SEAM, and the first instrument could not tell — it
        reported 1.200 units on a perfect walk. clip_report splits `root_travel`
        (parent == k_no_parent) from `loop_gap_position` (everything else).
        NOTHING APPLIES the travel to a world transform yet; 7.7 §10 names it.
        wrap_time, NOT std::fmod: fmod takes the sign of the DIVIDEND, so
        fmod(-0.1, 1.0) is -0.1. And AND std::fmod IS NOT CONSTANT TIME — its
        cost grows with the quotient, 1.603 ns on a bounded clock against 5.901
        after the clock has run to 6,666 s. WRAP THE PLAYHEAD EVERY STEP.
  contact: *** THE ARITHMETIC IS IN VELOCITY, AND A COLLISION IS AN EVENT
        RATHER THAN AN INTERVAL. *** 8.9, engine/include/engine/phys/solver.hpp.
        n points from a toward b (collide.hpp's, unchanged since 8.4), so
        u = (v_b + w_b x r_b) - (v_a + w_a x r_a) is NEGATIVE along n when
        approaching. The impulse is +J on b and -J on a: same value twice, which
        is why momentum is conserved to the bit and therefore why momentum is
        USELESS as evidence. ENERGY is the check.
        THE EFFECTIVE MASS IS A SCALAR AND THAT IS THE WHOLE TRICK:
        k = 1/m_a + 1/m_b + dot(w_a, inv_I_a w_a) + dot(w_b, inv_I_b w_b) with
        w = r x dir. Store 1/k. The rearrangement from the derivation's
        dot(dir, cross(inv_I*(r x dir), r)) is by the scalar triple product and
        it PROVES the division is safe.
        THE ORDER WITHIN A STEP IS LAW: integrate velocities, THEN solve
        contacts, THEN integrate positions. Solving last costs g*h^2 per step
        forever, whatever the solver does.
        THE ORDER WITHIN A SOLVE IS ALSO LAW: all normals, then all frictions.
        Coulomb's radius is mu times THIS solve's normal impulse, so friction
        first means a cone of radius zero and no friction at all.
        CLAMP THE ACCUMULATED IMPULSE, NEVER THE INCREMENT. Identical at one
        pass; a stack that launches itself at sixteen.
        WARM STARTING IS ONCE PER STEP (`warm_start_contacts`), never inside the
        iteration loop.
        MATERIALS ARE A PAIR PROPERTY passed to `prepare_contacts`, NOT a field
        on rigid_body — 8.8's handover predicted otherwise and the reason it was
        wrong is the content. Both combine defaults are `minimum`, which is NOT
        what Box2D, Bullet or PhysX ship; see the header block for the five
        published surface pairs that decided it.
  solver: *** THE LOOP IS SEVEN STAGES AND THREE OF THE ORDERINGS ARE LAW. ***
        8.10, engine/include/engine/phys/solver.hpp, `contact_solver::solve`.
          1 build the islands and group them (counting sort, 8.8's shape)
          2 WAKE any island not entirely asleep        <- before the solve
          3 prepare + prepare_bias + warm_start_contacts, ONCE per manifold
          4 velocity_iterations x solve_contacts
          5 position_iterations x solve_positions, then apply_pseudo_velocity
          6 write_back
          7 update_sleep                                <- after the solve
        WARM STARTING ONCE PER STEP (8.9's scar: inside the loop it slid a crate
        810 mm down a slope). ALL VELOCITY ITERATIONS BEFORE ANY POSITION ONE
        (the position pass reads separations the velocity pass will make stale).
        WAKING BEFORE, THE SLEEP TEST AFTER — two different things that read
        like one, and the reason is g*h: before the solve every resting body in
        the scene is moving at 0.1635 m/s, so a pre-solve test never fires.
        A FIXED BODY IS NOT A BRIDGE. `build_islands` unions only when BOTH ends
        are dynamic. 20 islands against 1 on a yard of twenty towers, and one
        island means no sleeping at all. Kinematic bodies are not bridges
        either. A body with no contacts is its own island of one.
        THE ISLAND LABEL ENCODING IS `-label - 2` AND THE 2 IS NOT A TYPO.
        `island_of` is its own union-find parent array (no allocation), so a
        root's label is written back negative to keep it distinguishable from a
        parent index — and `-label - 1` sends label 0 to the -1 sentinel.
        SLEEPING IS ALL OF AN ISLAND OR NONE OF IT, and a slept body has its
        velocities ZEROED so that "asleep" is a state rather than a pause. A
        body slept on its own is an immovable body and hangs in the air.
        SPLIT IMPULSE IS THE DEFAULT CORRECTION. Baumgarte's bias is a real
        velocity the body keeps; a pseudo velocity is thrown away at the end of
        the step. Same bias formula, same time constant, 46% of a velocity
        iteration, 1.14006 m/s of departure against 0.00000.
        THE BIAS IS NOT COMPUTED BY prepare_contacts, because that function does
        not know `h` and should not: `prepare_bias(batch, h, cfg)` is separate
        so the dependency is named rather than hidden in a config struct.
        A CACHED TANGENT IMPULSE TRAVELS WITH ITS BASIS. `contact_manifold::
        tangent[2]`, written by write_back, carried by carry_impulses, rotated
        by prepare_contacts. `tangent_basis` re-chooses the frame every step
        from the normal's smallest component and flips by 90 deg on 0.96% of
        manifold-frames.
        DEFAULTS, ALL MEASURED: 8 velocity iterations (holds 5 crates, not 10),
        3 position iterations, beta 0.2 (tau 74.7 ms), slop 5 mm (the resting
        depth IS the slop), max_correction_speed 3 m/s, warm_start TRUE,
        sleep 0.05 m/s / 0.10 rad/s / 0.5 s (and 0.05 is only 2.5x the floor).
        AND THE STEP IS TWO HALVES WITH A GAP: integrate_velocities, contacts,
        integrate_positions. Solving after the position half costs g*h^2 per
        resting step forever — 2.725 mm at 60 Hz, 654 mm in four seconds. The
        split is bit-identical to the monolithic `step` (400 of 400 bodies over
        120 steps) and is only separable because the integrator is
        semi-implicit; it costs a second mat3_from_quat per body, +37.1%.

  joint: *** A CONSTRAINT IS A ROW AND TWO BOUNDS, AND C >= 0 IS THE SATISFIED
        SIDE. *** 8.11, engine/include/engine/phys/constraint.hpp.
        J = [linear_a | angular_a | linear_b | angular_b] and J.V = dC/dt. The
        impulse is J^T lambda (virtual work: an ideal constraint does no work on
        any V with J.V = 0) and the effective mass is 1/(J M^-1 J^T).
        point_row(d, r_a, r_b) = [-d, -(r_a x d), d, r_b x d] — a contact normal,
        a rod, each axis of a socket. angular_row(axis) = [0, -axis, 0, axis].
        `a`'s halves carry the minus because a's point velocity is SUBTRACTED.
        BOUNDS ON THE ACCUMULATED IMPULSE: contact [0, inf); rod (-inf, inf);
        rope = point_row(-u) with [0, inf); a limit's two ends =
        angular_row(+/-axis) with [0, inf); motor [-tau h, tau h] with its
        speed as `target`.
        ONE-SIDED ROWS ARE SPECULATIVE by default: C > 0 gives target -C/h,
        C <= 0 gives target 0 plus a bias. `target` (real) and `bias` (-beta C/h,
        correction only) are SEPARATE fields; the velocity pass adds bias only
        under Baumgarte, the position pass uses it only under split impulse.
        JOINTS ARE AUTHORED BODY-LOCAL: anchor_a/b, axis_a/b, and rest =
        conj(q_a) q_b at creation. Hinge angle = 2 atan2(q.v . axis_a, q.w) of
        q = conj(q_a) q_b conj(rest), moved to w >= 0 first (double cover).
        WARM-START STATE LIVES IN FRAMES THAT CANNOT ROTATE AWAY: point_impulse
        and angular_impulse are WORLD vectors; limit and motor scalars are along
        an axis fixed in body a; the hinge perpendiculars are chosen from the
        axis in a's own frame (8.10 §4's bug, avoided twice).
        BLOCKS: the point constraint is one 3x3 K^-1, a hinge's alignment one
        2x2; limits and motor are scalar rows. Within a joint the solve order is
        motor, limits, alignment, pin — what may give way first, the pin last.
        Joints before contacts in every sweep (argued, not measured: ex. 4).
        THE CORRECTION IS THE SOLVER'S (split impulse), see the header for the
        measured tradeoff against Baumgarte.
        A JOINT DOES NOT KNOW ITS BODIES: indices go to constraint_solver::add
        every step, like manifolds. Jointed pairs are kept out of the narrow
        phase BY THE CALLER, with collision_filter, unless collide_connected.
        rho = m d^2 / (I_cm + m d^2) (`lever_ratio`) sets a body pendulum's
        period, the energy a velocity joint loses per step (rho (w h)^2), and
        the per-sweep convergence of any angular row against its pin.

  swing_twist: *** q = swing * twist, TWIST FIRST, ABOUT THE BONE. *** 8.12,
        engine/include/engine/phys/constraint.hpp `split_swing_twist`.
        The twist keeps q.v's component along the axis, renormalised; the
        swing is q * conj(twist) and is the SMALLEST rotation carrying the axis
        where q does = rotor_from_mirrors(t, h), h the half-way vector. Both
        returned with w >= 0. One singularity: a swing of exactly 180 deg,
        returned as twist = identity.
        A CONE-TWIST JOINT (ball_socket + `joint_cone` + `joint_limit`):
        axis_a = the CONE's axis in a, axis_b = the TWIST axis (bone) in b,
        rest = conj(offset) conj(q_a) q_b with offset the swing from cone axis
        to bone at authoring, so conj(q_a) q_b conj(rest) splits into the
        joint's real swing and twist at every pose and `hinge_angle` IS the
        twist. Swing = atan2(|a1 x b1|, a1.b1) (`swing_angle`).
        ROWS: swing = angular_row(-n), n = a1 x b1 normalised, left out when
        sin(swing) < 1e-4 (on the axis; a limb crossing a cone from dead
        centre in one step needs > swing/h). Twist = angular_row(+/-(a1 + b1)
        /(1 + a1.b1)), left out within 2.6 deg of a 180 swing. NEVER ABOUT THE
        BONE (Codman). The hinge's limit rows are the zero-swing case, so 8.11
        was right for hinges. Row role `swing`, warm-start `joint::swing_impulse`
        (a scalar along n, which is fixed by the bones and cannot jump).
        k_max_joint_rows stays 8 (cone-twist needs 6).
        POSITION PASS (8.12 fix): a one-sided row with C > 0 is solved against
        its SPECULATIVE TARGET -C/h, never its zero bias.

  ragdoll: *** EVERY BODY HAS EXACTLY ONE OWNER. *** 8.12,
        engine/include/engine/phys/ragdoll.hpp. Animated = KINEMATIC (inv mass
        and inertia zeroed) and STEERED: v = (x_target - x)/h, w = Rodrigues
        (2/h) dq.v/dq.w under spin_rule::linearised (the log under
        exponential) — the inverse of the integrator, never a teleport.
        Simulated = dynamic; `simulate` restores mass/inertia and KEEPS the
        velocities; `animate` zeroes them again and CLEARS every joint's cached
        impulse. Joints are added only while simulated (`add_joints`).
        DESCRIPTIONS ARE MODEL SPACE AT THE BIND POSE; parts in parent-first
        order; the part member is `bone` (not `joint`: two kinds of joint).
        joint_from_body (offset, rotation) is the only link between skeleton
        and body. Capsules along body +y. Unit scale on every joint required.
        BODIES ARE A CONTIGUOUS RUN of the world's dense array from
        `first_body` (spawn produces that; body_world::remove breaks it —
        Module 9's handles lift it). Exclusion default `overlapping` (jointed +
        overlap at rest within 1 cm; audited by ragdoll_report). Return:
        realign_model (x, z only) -> read_pose -> animate -> steer toward
        transform_blend_slerp(start, clip, w), local space.
        Passengers ride at `frozen_local` (the clip's pose at `simulate`).

  cast: *** A CAST IS NEWTON'S METHOD ON A CONVEX DISTANCE, STEPPED FROM GJK'S
        LOWER BOUND. *** 8.13, engine/include/engine/phys/cast.hpp.
        t <- t + (lower - skin)/(d . n), n = gjk_result::direction (mover ->
        obstacle), lower = certify(at, obstacle, g).lower. SAFE for any n (the
        slab normal to n narrows at exactly d.n per unit t); CONVERGES
        quadratically because f is convex. Exits: d.n <= min_approach |d|
        (1e-4) -> clear (the slab never narrows); lower - skin <= tolerance |d|
        (1e-4, RELATIVE) -> hit; GJK intersecting at step 0 -> started_inside
        (EPA's question), later -> hit at the last measured iterate. 32
        steps max (worst seen 7). cast_result::surface_normal points OBSTACLE
        -> MOVER — the named exception to 9e's a -> b. ONE CONVEX OBSTACLE PER
        CAST. skin >= 1 mm (0 and 0.1 mm both measured worse).
  character: *** A CHARACTER IS A QUESTION, NOT A BODY. *** 8.13,
        engine/include/engine/phys/character.hpp. Capsule along world +y,
        position = capsule CENTRE, no mass, radius 0.3, half_height 0.6, skin
        0.01 (the mesh is drawn a skin lower). walkable = n.y >= cos(max_slope)
        - 1e-5, 45 deg. move_character ORDER: 1 carry (ground body's step as a
        TRANSFORM, swept, walls flattened) 2 recover (EPA out of FIXED and
        KINEMATIC only; back to a skin from < skin/2) 3 sideways (x,z; laid
        along walkable ground while grounded and not rising; walls clipped by
        clip_rule::original with flattened normals; ONE step-up try at the
        first wall; pushables ignored) 4 vertical (up: first thing is a
        ceiling; down: land on walkable, slide along steep, stop_on_ground)
        5 ground (probe 2 skins down, not while rising) + snap (grounded last
        step, not rising, snap_distance 0.35, roll off an edge by R - overhang
        first). step_height 0.3 (tallest ledge = 0.39: step + curb);
        step-up across >= R(1 - sin t). push_mass_limit 40 kg: sideways moves
        and recover ignore dynamic bodies at or below it; vertical and probe
        include them (you stand on crates). character::velocity = ACHIEVED
        displacement / h. RUN AFTER THE PHYSICS STEP: steer_proxy(proxy,
        ch.position, h) -> world step -> move_character. character_world is
        the parallel bodies[i]/shapes[i] arrays (a precondition, like the
        ragdoll's contiguous run) plus `self` (the proxy) and the spin rule.
        Knobs that exist to be measured: clip_rule {original, remainder,
        last_plane}, flatten_walls, lay_along_ground, stop_on_ground,
        carry_rule {transform, velocity, none}.
  integrator: *** SEMI-IMPLICIT (SYMPLECTIC) EULER IS THE DEFAULT, AND EXPLICIT
        EULER IS NEVER CORRECT. *** 8.1, engine/include/engine/phys/integrate.hpp.
        `position += velocity * h` BEFORE the velocity update is explicit Euler
        and is UNCONDITIONALLY UNSTABLE on any position-dependent force — not
        inaccurate, not needs-a-smaller-step: wrong at every h there is. The two
        lines in the other order preserve phase-space area EXACTLY and cost the
        same (0.994 ns vs 0.963 ns per body per step, a 3% gap against a 3%
        run-to-run spread). THE WHOLE ARGUMENT IS ONE DETERMINANT: for a linear
        restoring force a step is a 2x2 matrix, det(explicit) = 1 + h^2 w^2 > 1
        always, det(semi-implicit) = 1 exactly, det(backward) = 1/(1 + h^2 w^2).
        Measured at 1.010966377 against a predicted 1.010966182.
        AREA IS ENERGY. An orbit encloses 2*pi*E/w, so a determinant applied N
        times IS the energy ratio: (1.010966229)^3600 = 1.1270e17, which is what
        the harness measures. A 1 m spring reaches 3.36e8 m in a simulated
        minute; the first ten seconds look normal.
        ORDER OF ACCURACY CANNOT SEE ANY OF IT. Order is the h -> 0 question at
        fixed t; a game asks the t -> infinity question at fixed h. Both Euler
        rules read respectably on the first (explicit 1.089, semi-implicit 2.002
        — and that 2 is a FACT ABOUT THE OSCILLATOR, not a promotion, because
        the amplitude error is exactly zero and only a frequency shift is left).
        WHAT IS CONSERVED IS NOT ENERGY. Semi-implicit Euler holds
        (v^2 + w^2 x^2 - h w^2 x v)/2 — the SHADOW energy — to 4.6e-6 over 3,600
        float steps, while the true energy wobbles by exactly h*w peak to peak
        (predicted 0.104720, measured 0.104723). The orbit is a level set of the
        shadow quantity, which is why the wobble never becomes a trend.
        STABILITY IS THAT SAME STATEMENT. The shadow form is positive-definite —
        an ellipse rather than a hyperbola — iff h*w < 2, which IS the stability
        limit. Bisected at 0.3182939 against 2/w = 0.3183099. BOUNDED IS NOT
        RIGHT: at h*w = 1.95 the orbit is stable and swings x40. Budget h*w <=
        0.4, which at 60 Hz admits w <= 24 rad/s — a spring of 3.8 Hz.
        AND THAT IS WHY CONTACTS ARE NOT SPRINGS. A contact stiff enough not to
        let a box sink is hundreds of Hz and would need thousands of steps a
        second. 8.9/8.10 use impulses, and this is the number that says so.
        GRAVITY FORGIVES EVERYTHING: under constant acceleration all three rules
        agree on velocity to the last bit and err in position by exactly
        -/+ 0.5*a*h*t, LINEARLY (8.2 cm after 1 s at 60 Hz), with velocity
        Verlet exact. Your first falling-cube demo is fine. That is why the bug
        ships.
        DRAG IS NOT A POSITION FORCE, so the reordering buys nothing: use
        apply_drag (exact, std::exp) and damping_factor (pow(r, h)) instead.
        `v *= 0.99f` per step keeps 74% of the velocity per second at 30 Hz and
        24% at 144 Hz — same constant, four different games.
        max_stable_step(explicit_euler, w) RETURNS 0, deliberately: there is no
        stable step, so a caller who scales it gets a simulation that does not
        move rather than one that explodes in a playtest.
  audio: *** ENERGY ADDS, AMPLITUDES DO NOT. *** 7.8,
        engine/include/engine/audio/ + docs/conventions.html §8i +
        math-toolbox.html §8d. Almost every rule below is that sentence wearing
        a different hat, and the factor of two that keeps appearing in the
        exponents is always the same factor of two.
        ONE RUNTIME FORMAT: interleaved f32 in [-1,1] at the DEVICE's rate,
        converted once at load. Float for headroom (a sum of things in [-1,1] is
        not), interleaved because that is what the device takes, one format
        because five means five inner loops and four tested by nobody. Costs
        2.00x the file's memory for 16-bit content — the streaming debt, named
        for Module 9.
        A FRAME IS ONE SAMPLE PER CHANNEL. Durations, buffer sizes and cursors
        are all in FRAMES. Confusing the two is SILENT IN MONO and the first
        stereo file plays at double speed with the channels scrambled.
        SIGNED INTEGERS DIVIDE BY 2^(bits-1), never by 2^(bits-1) - 1. 16-bit
        divides by 32768, so -32768 -> exactly -1.0. Round trip measured at
        0.000e+00.
        A LISTENER IS A TRANSFORM, and `forward` is where -z_hat lands (§2's
        world space, applied to something that is NOT drawing). Getting the minus
        wrong is INAUDIBLE with two speakers until somebody adds a filter for
        sounds behind the head. The listener is usually the camera and MUST NOT
        BE ASSUMED to be: a third-person game hears from the character.
        pan = dot(dir_to_source, listener.right), which is already the sine of
        the angle off the median plane — no atan2, no special cases. Elevation
        folds to the centre FOR FREE, which is where two speakers can put it;
        front and back give the same answer, which is the limit of a stereo pair
        rather than a bug.
        CONSTANT-POWER PANNING: L = cos(theta), R = sin(theta),
        theta = (pan+1)*pi/4. CENTRE IS 0.7071, NOT 0.5, because two speakers
        playing one signal are correlated and what decides loudness is L^2+R^2.
        The linear law puts 0.5 there = -3.01 dB, a hole a swept sound falls
        into. Measured worst deviation over a sweep: 3.010 dB vs 0.000 dB.
        AMPLITUDE FALLS AS 1/d, NOT 1/d^2. Intensity falls as 1/d^2 — that part
        is right — but a SAMPLE IS A PRESSURE and pressure is sqrt(intensity).
        -6.02 dB per doubling; the plausible mistake gives -12.04 and sounds like
        every source is at the bottom of a well.
        A DISTANCE CURVE MUST REACH ZERO AT max_distance, not be cut off there.
        The bare inverse law is still at -33.98 dB at 50 m with a 1 m reference,
        and a step to silence is a click. falloff::inverse_ranged subtracts the
        value at max and rescales; it costs 8.7 dB of extra steepness at 32 m,
        which is the price of a curve that ends.
        dB IS 20*log10(amplitude). Twenty, not ten, for the same reason as the
        row above. Halving an amplitude is -6.02 dB.
        ANY PER-FRAME GAIN CHANGE IS RAMPED ACROSS THE BUFFER, NEVER ASSIGNED.
        The gain is sampled at 60 Hz and reconstructed at 48 kHz; a zero-order
        hold injects 0.100745 in one sample, 7.70x the waveform's own largest
        step, of which 98.8% is BROADBAND. 7.7's reconstruction rule in a
        subsystem that had never heard of animation.
        THE YARDSTICK FOR ANY INJECTED STEP IS THE SIGNAL'S OWN SLOPE,
        A*2*pi*f/f_s. A fade does not make a start-of-sound step SMALL, it
        REMOVES it: 0.2500 (38.2x) becomes 0.0065443 (1.00x), which IS the
        slope.
        THE AUDIO THREAD DOES NOT ALLOCATE, LOG, THROW, OR WAIT ON ANYTHING
        UNBOUNDED. It increments counters; the main thread reads them. 64 voices
        are 29.76 us of a 10,667 us deadline, so the mixing is never what makes
        you late — one log line is 1.02 us ON AVERAGE and is a syscall.
        A SOUND MUST OUTLIVE EVERY VOICE READING IT. stop_sound() before
        releasing one. The only UB in the subsystem, and deliberate: a shared_ptr
        would put a possible free on the audio thread.
  blend-space: *** A CROSS-FADE IS NOT 7.6's WEIGHTED SUM, AND THE DIFFERENCE IS
        WHICH OBJECT IS BEING AVERAGED. *** 7.7 §7.
        BLEND THE POSES: one transform_blend per joint, in LOCAL space, before
        anything is composed. quat_nlerp normalises, so a blended rotation is
        still a rotation and a blended transform is still a placement — every
        bone length preserved to 2.980e-07 at every angle.
        BLEND THE COMPOSED MATRICES: identical inputs, identical weight, ONE STEP
        LATER — and it is linear blend skinning one level up. A matrix lerp
        averages the joint POSITIONS, so bone k becomes the average of two unit
        vectors an angle apart and shortens by cos of half it. Bone k's direction
        is joint k-1's own axis, which has ACCUMULATED every rotation above it,
        so the chain is sum cos((k-1)*delta/2) — matched to 4.768e-07 over a
        sweep. 0.4434 units of five at 20 deg per joint; the demo measures
        5.00000 against 4.65020 on a real cross-fade. THE ERROR GROWS DOWN THE
        CHAIN, so the hips are untouched and the HANDS AND FEET are ruined, which
        is where props attach and where a player looks.
        pose_blend_report::worst_arc is the LOCAL arc, not the accumulated one:
        a chain differing by 20 deg at every joint and 100 at the tip reports 20,
        correctly, because the per-joint arc is what sets nlerp's error.
        Computed as the ARGMIN of |scalar| (one dot each, which nearest needs
        anyway) with angle_between run ONCE on the winner.
  skinning: A SKINNING MATRIX IS `model_from_model`, AND EVERYTHING FOLLOWS
        FROM THAT. 7.6, engine/include/engine/anim/skeleton.hpp +
        docs/conventions.html §8g.
        skin_j = model_from_joint_j(POSED) * joint_from_model_j(BIND). The
        inner labels agree, so the product is legal (2.8's rule) — and the two
        `joint`s are THE SAME JOINT AT TWO DIFFERENT TIMES, which is the one
        piece of bookkeeping no naming convention carries for you and the piece
        every confused explanation of skinning has dropped.
        THE OUTER LABELS ALSO AGREE, and that is rarely said out loud. Domain
        and codomain are both MODEL space, which is (a) why four of them may be
        ADDED — you may add linear maps only when they share both — and (b) why
        the sum is not a rotation, because model-to-model maps are closed under
        addition and rotations are not. One reading of the labels gives both the
        licence and the artifact.
        *** AT THE BIND POSE EVERY SKINNING MATRIX IS EXACTLY I. *** The two
        factors are then inverses of each other, so skinning a mesh at its bind
        pose must reproduce it vertex for vertex. THAT IS THE CHEAPEST COMPLETE
        TEST OF A RIG THAT EXISTS: no reference image, no artist, no eye, and it
        catches a transposed matrix, a chain composed in the wrong order, an
        inverse-bind array baked from a different skeleton, a wrong parent index
        and a skeleton edited after baking. skeleton_report::worst_bind_residual
        is that number — 4.768e-07 over six joints, 6.676e-06 at depth 32.
        Control: nudge one bind by 0.01 AFTER baking (a rig edited without a
        re-export) and it goes to 1.056e-02, four orders louder.
        NO MATRIX IS INVERTED TO BUILD THE INVERSE BINDS. (AB)^-1 = B^-1 A^-1,
        so the inverse chain composes with THE SAME FLAT LOOP as the forward
        one, multiplying the parent on the RIGHT instead of the left:
          model_from_joint[j] = model_from_joint[p] * parent_from_local(j)
          joint_from_model[j] = local_from_parent(j) * joint_from_model[p]
        Both walk index 0 upward. The only inverse anywhere is
        local_from_parent on one node — a transpose, three reciprocals and a
        negated offset. Checked against a GAUSS-JORDAN INVERSE WRITTEN FROM
        SCRATCH IN THE HARNESS, sharing no code with the engine: 4.768e-07 at
        depth 8.
        THE ORDERING IS A PRECONDITION, NOT AN ALGORITHM: joints[i].parent < i,
        always. 5.9's hierarchy could not demand that (pool order is insertion
        order disturbed by swap-and-pop) and paid three passes plus a dirty flag
        EVERY FRAME. A skeleton is AUTHORED and SMALL, so the order is a
        requirement on the data, checked once, and compose_pose is one flat loop
        with no sorting. The cost lands on the IMPORTER — glTF's skins.joints is
        an arbitrary permutation — as one topological sort at load time.
        The failure it prevents is not a broken picture: a child whose parent
        comes later composes against LAST FRAME's matrix, which reads as a
        one-frame lag that grows with depth and only appears when the character
        moves fast. skeleton_report::out_of_order counts it.
        YOU ATTACH PROPS WITH model_from_joint, NEVER WITH THE PALETTE. A sword
        in a hand socket is a rigid object with its own vertices in its own
        space — 5.9's case. Multiplying it by a SKINNING matrix asks "where did
        this model-space point of the CHARACTER move to", which has nothing to
        say about the sword. compose_pose and build_palette are kept apart for
        exactly this reason.
        WEIGHTS MUST SUM TO 1, AND IT IS NOT TIDINESS. A palette matrix is
        AFFINE, so its translation is blended too: sum w = s scales every vertex
        to s times its distance from the MODEL ORIGIN. 0.9 gives a character 10%
        smaller and sunk into the floor, which reads as a scale bug. Measured on
        208 vertices: gap from the prediction 0.9*v is 5.218e-07, worst move
        0.5012 = 0.1 * sqrt(0.35^2 + 5^2). A vertex weighted to NOTHING lands on
        the model origin and draws a spike; normalise_weights repairs it at
        import, after validate has counted it.
        FOUR INFLUENCES IS A HARDWARE NUMBER: the width of a GPU vector
        register, so four indices are one attribute and four weights one float4,
        and the blend is four multiply-adds with no loop and no branch. A ZERO
        WEIGHT MUST STILL CARRY A VALID JOINT INDEX — the index is read before
        the weight is applied, so a slot padded with 0xFFFF is an out-of-bounds
        read on the hottest path. skin_report::padded_indices counts it
        SEPARATELY from out_of_range, because only the second changes the
        picture and only the first is a latent crash.
        NORMALS USE THE LINEAR PART, NOT THE INVERSE TRANSPOSE, and the
        justification is NOT that the error is small. Exact for a rotation
        (3.817e-07 over 4,000) and for a uniform scale (0.000e+00); 22.6 deg of
        normal at a 1.5:1 stretch and 36.9 at 2:1, costing 0.083 and 0.134 of
        Lambert brightness. Non-uniform scale on a SKINNING JOINT is a rig
        defect rather than a renderer's problem to absorb;
        skeleton_report::nonuniform_binds counts the authored half.
        ONLY POSITIONS AND NORMALS CHANGE. uvs, tangents and indices are copied
        once and never again — which is the observation the GPU path is built
        on: 4,096 bytes of palette against 116,160 bytes of deformed vertices,
        28.4x on this fixture and growing with the mesh.
  candy-wrapper: THE COLLAPSE IS cos(theta/2), DERIVED FROM AN ISOCELES
        TRIANGLE AND NOTHING ELSE. 7.6, docs/conventions.html §8g.
        Two joints theta apart send a vertex to two points, both |v| from the
        axis; the half-and-half blend takes their midpoint, which lies on the
        bisector, and the bisector of an isoceles triangle meets the base at a
        RIGHT ANGLE. So |v'| = |v| cos(theta/2). NOTHING IN THAT DERIVATION
        MENTIONS SKINNING, A JOINT OR A MATRIX — it is the same half-angle 7.3
        found between two mirrors and 7.4 found inside a quaternion, arrived at
        for a THIRD independent reason. Worst gap over a 0-180 sweep 8.742e-08.
        At 180 deg the radius is not small, it is EXACTLY ZERO: the average of
        two antipodal points is the origin. Control: weight 1.0 on one joint
        gives 1.000000 at the same twist, so the artifact belongs to the BLEND.
        THE FIX IS IN THE FORMULA. The collapse depends on the angle between
        ADJACENT joints, so n segments give cos(theta/2n): 0.000, 0.707, 0.866,
        0.924 at 180 deg. THAT IS WHAT A FOREARM TWIST BONE IS — a direct attack
        on a half-angle — and the first one or two joints do nearly all the
        work.
        A BEND OBEYS THE SAME COSINE ON AN ELLIPSE. A twist rotates about the
        limb's OWN axis so the whole ring is in the collapsing plane; a bend
        rotates about an axis ACROSS it and a rotation fixes its own axis, so
        the ring FLATTENS: semi-minor r cos(theta/2), semi-major r untouched.
        Measured at 120 deg: 0.500000 / 0.500000 / 1.000000. That is why a
        twisted forearm loses its cross-section and a bent elbow reads as a
        crease — one cosine, two geometries. MEASURE THE MINIMUM RADIUS ON A
        RING, NOT THE MEAN: the minimum is the minor axis in both cases, so one
        number reads the same law in both modes.
        LBS IS NOT "nlerp WITHOUT THE NORMALISE" — WRONG IN BOTH DIRECTIONS.
        WORSE: nlerp's schedule error is governed by the arc between two
        QUATERNIONS (half the rotation angle, then doubled), LBS's by the whole
        angle, and schedule error grows much faster than linearly — about 4x at
        every arc: 10.95 deg of pose against 2.23 at a 120 deg twist, 76.63
        against 8.00 at 179. BETTER: a rotation matrix is UNIQUE, so there is no
        double cover, no long way round and no `nearest` to forget — 7.5's 359
        deg bug cannot be written here (excess over the chord 1.192e-07 over
        20,000 pairs). At t = 0.5 the schedule error vanishes by symmetry for
        both and only the radius is left: 0.500000, which is the cosine.
  slerp: THE FORMULA IS a (a^-1 b)^t AND IT IS NOT ABOUT QUATERNIONS. 7.5,
        engine/include/engine/math/quat.hpp + docs/conventions.html §8f.
        It is the definition of a geodesic on any group with an inverse, a
        product and a real power — which is why 7.2's rotation_slerp (mat3),
        7.3's complex_slerp (plane) and 7.5's quat_slerp are the SAME THREE
        STEPS and agree to float resolution on one journey while sharing no
        code: 4.172e-07 of a matrix entry, 2.384e-07 rad.
        EVERY BLEND CALLS nearest(a, b) FIRST. q and -q are the same
        orientation and OPPOSITE POINTS, so every pair has two arcs summing to
        2pi and 49.6% of uniformly sampled pairs name the long one (measured,
        9923 of 20000). Two keyframes 1 deg apart signed oppositely become a
        359 DEGREE SPIN. nlerp's version is worse: the raw chord passes within
        0.00436 of the ORIGIN.
        NLERP'S PATH IS EXACT AND ITS SCHEDULE IS NOT, by exactly
        sec^2(Omega/2) — 7.3 derived it in the plane and nothing in that
        argument mentions dimension. Excess turning 0.00%; departure from the
        a-b plane 1.605e-07.
        *** THE DOUBLE COVER BOUNDS THE ERROR, AND THE BOUND IS EXACT. ***
        nearest forces the four-component dot non-negative, so the SPHERE arc
        Omega <= 90 deg, so sec^2(Omega/2) <= sec^2(45) = 2 EXACTLY and the
        pose gap <= 8.1491 deg. In the plane, with no double cover to exploit,
        the same function reaches 13,131x and 26.34 deg. Measured over 20,000
        pairs: largest arc 89.9969, largest gap 8.1345; control without
        nearest, 176.3020. THE COST AND THE GUARANTEE ARE THE SAME FACT.
        THRESHOLD: nlerp within 0.5 deg up to an arc of 73.50 deg. Under that,
        use it (3.42x cheaper: 4.763 ns against 16.288; 7.2's mat3 slerp is
        45.752). Over it, or for any pose a designer chose, use slerp.
        DO NOT SHIP THE TEXTBOOK TRIG FORM. It divides by sin(Omega) and Omega
        comes from an acos that returns exactly 0 below an arc of
        2*sqrt(eps/2) = 0.0280 deg — measured 0.0279. NaN, and a NaN pose
        propagates down a whole skeleton. Ours divides by sin(theta/2) and the
        thing it divides IS sin(t theta/2), so the ratio tends to t: continuous
        through the identity, no epsilon, no branch, no "lerp if small".
        A TEST MUST NOT SAMPLE ONLY t = 0, 0.5, 1. Those are EXACTLY the three
        values at which nlerp and slerp are the same function (the midpoint is
        exact by symmetry, measured 6.664e-08 over seven arcs). Sample 0.229
        and 0.771.
  quat-metric: 2 atan2(|v|, |w|), NEVER 2 acos|a.b|. 7.5 REWROTE 7.4's
        angle_between, and the old form is kept as angle_between_by_cosine so
        the two can be measured rather than argued about — exactly the
        arrangement angle_between_rotations_by_trace has had since 7.1.
        THE COSINE IS QUADRATIC AT 1 AND THE SINE IS LINEAR, so the acos form
        has a noise floor of 2*sqrt(2*eps) = 0.0560 deg AND A BIASED ERROR.
        A path integral over 4,096 steps of a 150 deg arc summed slerp's own
        geodesic to 88.3437 deg — -41.10% — i.e. the metric failed slerp for
        not walking the path it was walking. The atan2 form gives -0.00%, and
        the Euler control then lands on +14.10%, which is Lesson 7.1's
        published +14% arrived at from a different representation.
        THE GENERAL RULE, which is not about acos: 7.4 wrote the metric for a
        SEPARATION (large answer) and 7.5 asked it for a STEP (tiny answer).
        The function did not change; the question did, and nothing in the name,
        the type or the doc comment marks the difference. Re-derive a numerical
        routine's conditioning at every scale you reuse it at.
        Cost: ~12 multiplies against 4. Right side of the trade for a metric.
  transform-storage: transform::rotation IS A quat, SINCE 7.5. Five modules as
        a mat3, promised away in Module 2, moved to 7.1, then to 7.4, then done
        here. parent_from_local gained ONE LINE (mat3_from_quat) and the
        arithmetic below it did not move a character — which is the argument
        for a named type rather than three loose variables.
        THE CONVERSION IS ENGINE POLICY, not a detail: 7.4 §10.3 measured the
        crossover at ~8 vectors, a mesh has thousands, so the engine STORES
        quaternions and converts ONCE PER OBJECT PER FRAME, right there, then
        multiplies vertices by a matrix.
        A STEPPED rotation gets renormalised_fast; a REBUILT one gets nothing.
        The distinction is whether the field is an ACCUMULATOR — whether the
        previous frame's answer is an input. collector's carousel and spinner
        are the first two in the engine's history that are.
        transform_from_affine (math/transform.hpp) IS THE DECOMPOSITION, and it
        exists because THREE call sites were doing it and TWO were doing it
        wrong. Returns transform_extraction {value, out_of_square, mirrored} —
        the same shape as axis_angle_extraction (7.2) and euler_extraction
        (7.1), for the same reason.
        NEGATIVE DETERMINANT IS A MIRROR AND MUST BE HANDED BACK DELIBERATELY;
        column lengths are non-negative, so skipping it turns a mirrored object
        into an unmirrored ROTATED one.
        A ZERO COLUMN IS REBUILT AS A CROSS PRODUCT OF THE SURVIVING PAIR, not
        as the parent's axis. gltf_view's inherited version did the latter and
        its comment said, correctly, that it "keeps the matrix finite instead of
        producing NaNs" — and a non-orthonormal basis makes quat_from_rotation
        wrong in EVERY column, measured 0.399 on a flattened object whose other
        two axes were perfectly recoverable. FINITE IS NOT RIGHT.
        SHEAR CANNOT BE HELD AT ALL and is COUNTED, in
        renderable_report::skewed. A non-uniformly scaled parent with a rotated
        child produces it; out_of_square 0.5941, residual 0.3922 after the best
        repair either route can make.
        T*R*S PER NODE IS A RESTRICTION, NOT A REPRESENTATION. The fix for a
        matrix outside it is usually ANOTHER NODE, not a wider field — see
        `boom-two-nodes` under `decisions:`.
  quat-cover: q AND -q ARE THE SAME ROTATION, BITWISE. 7.4. The sandwich is
        QUADRATIC in q, so a global sign cannot survive it: measured
        |M(q) - M(-q)| = 0.000e+00 over 20,000 rotations, not a tolerance.
        THE FABS IS NOT OPTIONAL. `angle_between(quat, quat)` takes |dot|;
        without it two identical poses read as 360.0000 degrees apart, which is
        the single most common quaternion bug in shipped code (a character
        spinning all the way round between adjacent keyframes).
        A 360 DEGREE TURN SENDS q TO -1 AND THE POSE TO 0.0000 DEG, and it takes
        720 to bring both home. The gimbal demo's [D] mode draws exactly that:
        a dial plotting (w, v.n_hat) = (cos(theta/2), sin(theta/2)) turning at
        half the craft's rate.
        axis_angle_from_quat RETURNS [0, 2pi), NOT [0, pi], and that is
        information rather than inconsistency: a quaternion distinguishes
        "350 deg about n" from "10 deg about -n", which a matrix cannot, and
        that is what an animation system needs to take the long way on purpose.
  quat-extract: FOUR CANDIDATES THAT SUM TO 4, SO THERE IS NO BAD CASE. 7.4.
        4w^2 = 1 + r00 + r11 + r22, 4x^2 = 1 + r00 - r11 - r22, and the other
        two by symmetry; every off-diagonal cancels, so the four sum to exactly
        4 (measured 4.768e-07) and THE LARGEST IS ALWAYS AT LEAST 1 (smallest
        pivot seen in 20,000: 1.038415). Divisor >= 2, always.
        COMPARE 7.2, whose three candidates summed to 1, guaranteed only 1/3,
        and needed a crossover DERIVED at tan(theta/2) = sqrt3 -> 120 deg. The
        fourth component is what removes the threshold.
        THE NAIVE TRACE ROUTE IS INDISTINGUISHABLE UP TO ~120 DEG — which is
        exactly where a hand-written test suite lives — and then dies: 1.800e+02
        degrees of error at a turn of 179.99, because it divides by 4w and
        w = cos(theta/2). At exactly 180 it returns the identity.
        BUT THE AXIS CONDITIONING IS NOT BETTER, and §G.3 was written to claim
        it was. On a matrix carrying 1e-7 of error the two routes agree to
        within 0.07% at 0.05 degrees. quat_from_rotation's INPUT IS A MATRIX: at
        a small turn it pivots on w and the vector components come off the same
        antisymmetric differences the matrix route uses. THE INFORMATION WAS
        ALREADY GONE. A quaternion's better conditioning is a property of
        HOLDING one, not of extracting one.
  euler: INTRINSIC Y-X-Z, ACTIVE, RIGHT-HANDED, RADIANS — and the whole point
        is that this line exists. 7.1, engine/include/engine/math/euler.hpp
        + docs/conventions.html §8b. `rotation_from_euler({yaw, pitch, roll})`
        == `rotation_y(yaw) * rotation_x(pitch) * rotation_z(roll)`.
        THE COUNT IS TWENTY-FOUR: twelve axis orders (3 x 2 x 2 — consecutive
        axes must differ) times two frames, intrinsic and extrinsic. Shoemake,
        Graphics Gems IV, encodes exactly these in four bits. MEASURED: the
        triple (30, 40, 50) read under all 24 lands between 16.03° and 83.48°
        from ours, and EXACTLY ONE is zero. The dangerous one is not the worst —
        it is `yzx` extrinsic at 16°, which reads as a tuning problem.
        THE AXIS ORDER IS A CHOICE OF WHERE TO PUT THE HOLE, not whether to have
        one. For a three-distinct-axis order the singularity sits on the MIDDLE
        angle at ±90°, so y-x-z puts it at pitch ±90° — nose vertical, which
        every first-person camera already clamps away from for non-mathematical
        reasons. x-y-z would put it on the heading. The repeated-axis families
        (zxz etc.) put theirs at middle angle 0°, which is the rest pose.
        INTRINSIC == EXTRINSIC REVERSED, ANGLES REVERSED TOO: ours is also
        extrinsic z-x-y read (roll, pitch, yaw). Proof is one line — intrinsic
        composition multiplies on the RIGHT because (F R Fᵀ) F = F R, the
        step-in and step-out cancelling. Checked: worst element diff 0.000e+00,
        because it is literally the same sequence of float operations.
        NOTHING IN THE ENGINE STORES ONE. `transform::rotation` is still a
        `mat3` and becomes a quaternion at 7.4. Euler angles are an INTERFACE.
  euler-lock: GIMBAL LOCK IS A RANK DEFICIENCY YOU CAN PRINT, AND IT NEEDED NO
        NEW API. 7.1. `euler_rate_jacobian(e)` has the three knob axes as its
        columns (world +Y; Ry(yaw)·x̂; Ry(yaw)Rx(pitch)·ẑ) and
        `determinant` has been in mat3.hpp since 2.6:
          det J = -cos(pitch)      (yaw cancels — it must, since turning the
                                    whole craft cannot change its controls)
          JᵀJ  = I with -sin(pitch) in two corners  <- THE WHOLE PATHOLOGY
          sigma = { sqrt(1+|sin p|), 1, sqrt(1-|sin p|) }
        Read JᵀJ and it says: THE YAW AXIS AND THE ROLL AXIS ARE NOT
        PERPENDICULAR, AND HOW FAR FROM PERPENDICULAR IS EXACTLY THE PITCH.
        MEASURED, closed form vs a brute-force sweep of a million unit rate
        vectors per pose: worst |error| 1.788e-07. At 0.01° from vertical the
        weakest gain is 1.235e-4, so 8,100 rad/s of knob buys 1 rad/s of craft —
        464,000 deg/s. That is what "the control loop saturates and lurches"
        means in numbers.
        ALGEBRAIC TWIN, same fact by another route: at pitch +90 the composite
        matrix contains yaw and roll ONLY as (yaw - roll), so an entire diagonal
        of the (yaw, roll) square is one orientation. At -90 it is the sum.
        Measured: both knobs +47° at pitch 90 gives 0.0000° of motion; CONTROL
        at pitch 0 gives 65.51°, against 47*sqrt(2) = 66.47 for two orthogonal
        turns. The control's threshold was WRONG at 80 on the first run and the
        control is what found that out.
  cancellation-against-one: THE SAME NUMERICAL BUG, THREE TIMES IN ONE LESSON,
        AND THE THIRD IS WHAT MADE IT VISIBLE. 7.1.
          sqrt(1 - sin*sin)   for cos(pitch)  -> exactly 0 at pitch 89.99°
          sqrt(1 - |sin p|)   for sigma_min   -> exactly 0, prints inf
          acos((tr R - 1)/2)  for the angle   -> exactly 0 at 0.004°, rel err 1.00
        A float cannot hold a change of 1.5e-8 at 1.0 or 1.6e-5 at 3.0, so a
        quantity obtained by subtracting two nearly-equal numbers near 1 is gone
        before the sqrt or the acos sees it — and acos/asin then amplify the
        remains, having infinite slope at ±1.
        THE CURE IS ONE SHAPE: get the small quantity from something that IS
        small. cos_pitch = hypot(r02, r22), because those entries ARE
        sin(yaw)cos(pitch) and cos(yaw)cos(pitch) — so THE CONDITIONING OF THE
        YAW EXTRACTION IS THE LENGTH OF THE VECTOR WHOSE ANGLE IT TAKES.
        sigma_min = |cos| / sqrt(1 + |sin|), from 1-s = c²/(1+s). The angle from
        atan2(|R - Rᵀ|/2, (tr R - 1)/2), whose first argument is built from
        DIFFERENCES of entries and stays proportional to theta.
        AND WATCH THE DRIFT BEFORE THE CLIFF: the naive sigma reads 0.0012449
        against a true 0.0012340 at 89.9° — 0.9% wrong and entirely plausible.
        The cliff is what you notice; the drift is what ships.
        DIAGNOSTIC TO CARRY: if a formula's job is to report something near
        zero, look at what it computes just before it returns and ask whether
        THAT is ever small.
  euler-extraction: THE EXTRACTION RETURNS ITS OWN CONFIDENCE. 7.1,
        `euler_extraction { angles, cos_pitch, degenerate }`. Entry (1,2) of the
        composite is -sin(pitch) ALONE — the middle axis of any Euler sequence
        is the one no other rotation conjugates, so it always leaves exactly one
        clean entry; change the order and it moves, it does not disappear.
        AT LOCK THE MATRIX SURVIVES AND THE SPLIT DOES NOT. In: (20, 90, -35).
        Out: (55, 90, 0) — 55 is 20-(-35), the difference and the whole of the
        recoverable information. Round-tripped the matrix is back to 6.5e-6°.
        `degenerate` says so rather than leaving the caller to find out. This is
        6.18's `used_height` rule again: IF A ROUTINE COMPUTES A QUANTITY A
        CALLER WOULD NEED TO JUDGE IT, RETURN IT.
        THE THRESHOLD'S FIRST JUSTIFICATION WAS WRONG AND MEASURING FOUND IT.
        The claim was "float error / cos(pitch) reaches the recovered yaw"; on a
        freshly-built matrix there is NO amplification at any pitch, because
        FLOAT ERROR IS RELATIVE and the two entries atan2 reads are themselves
        proportional to cos(pitch). The amplification is real only for a matrix
        carrying ABSOLUTE error — one that came down a hierarchy, or out of a
        file. Measured with ±1e-7 per entry: 0.00041° at cos_pitch 1.7e-2,
        0.02029° at 3.5e-4, i.e. 1.2e-7 rad / cos_pitch. k_euler_lock_epsilon =
        1e-4 bounds it at 0.07° and corresponds to 0.0057° from vertical.
        NOT A TRUE INVERSE, and cannot be: asin's range is ±90°, so pitch 100°
        comes back as (-170, 80, 180) — a different triple, the same rotation to
        1.5e-5°. DO NOT ROUND-TRIP THROUGH EULER ANGLES IN A LOOP.
  axis-angle: ONE CONVENTION, TWO HONEST AMBIGUITIES. 7.2,
        engine/include/engine/math/axis_angle.hpp + docs/conventions.html §8c.
        CANONICAL: axis unit, angle in [0, pi]. The range is not a clamp — it
        arrives free, because atan2(magnitude, trace) lands there — and it buys
        the property THE ANGLE IS THE DISTANCE, which is the instrument 7.1 had
        to build separately.
        THE AXIS IS NOT NORMALISED FOR YOU, deliberately: a non-unit axis does
        not give a slightly wrong rotation, it gives a SHEAR (in-plane scales by
        |n|, along-axis by |n|^2). A caller holding a non-unit direction is
        holding a ROTATION VECTOR; rotation_from_rotation_vector takes exactly
        that and has no requirement to violate. The API's shape says so.
        THE TWO AMBIGUITIES ARE REAL AND NEITHER IS NUMERICAL. theta = 0: every
        axis is correct, which is the same sentence as none is determined; we
        return placeholder +X and say so via axis_route::no_axis. theta = pi:
        (n, pi) and (-n, pi) are THE SAME ROTATION, measured 1.0e-05° apart, and
        no implementation can prefer one. NEVER COMPARE TWO RECOVERED AXES FOR
        EQUALITY; compare rotations. Control: at 179° the same pair is 2.000°
        apart, so the ambiguity exists at exactly one point and nowhere near it.
  plane: COMPLEX NUMBERS ARE THE PLANE'S ROTATIONS, AND THE ROTOR IS NOT ONE.
        7.3, engine/include/engine/math/complex.hpp + docs/conventions.html §8d.
        CANONICAL: unit complex, ANTICLOCKWISE for positive theta in a y-up
        plane — the same numbers mat2::rotation has had since 2.5, because §5
        PROVES the two types are the same object (worst element diff 0.000e+00
        over 721 angles; M(z)M(w) = M(zw) to 1.9e-06 over 2,000 pairs).
        In framebuffer coordinates, where +y is DOWN, the identical numbers turn
        clockwise on screen. Flip once in the view mapping, never at a call site.
        THE ANGLE IS SIGNED, (-pi, pi], and this is a DELIBERATE DIFFERENCE from
        §8c's unsigned [0, pi]. The plane is oriented so one number can say which
        way; space is not, because negating axis and angle together gives the
        same rotation. THAT IS WHY `angle_between(complex, complex)` IS NOT AN
        OVERLOAD of `angle_between_rotations(mat3, mat3)`.
        A ROTOR IS NOT A ROTATION — IT IS A SQUARE ROOT OF ONE. This is the one
        that causes bugs. rotor_from_mirrors(m0, m1) = m1 * conj(m0) carries
        HALF the angle of the rotation R^2 it generates; apply it once and
        everything turns half as far as intended. Use apply_rotor, whose name is
        the warning. Same bug as `q v` instead of `q v conj(q)` one dimension up.
        TWO ROTORS PER ROTATION, EXACTLY: (-R)^2 = R^2, bit for bit, the same
        products of the same floats. A rotation remembers the ANGLE BETWEEN its
        mirrors and not WHICH mirrors. That is the double cover, and it is a
        plane fact before it is a quaternion fact.
  plane-halfangle: WHERE THE theta/2 IN EVERY QUATERNION COMES FROM, AND IT IS
        VISIBLE WITH A PROTRACTOR. 7.3 §7, and it collects a debt 7.2 left open
        (§8.3's 2 sin(theta/2) and §8.4's tan(theta/2) = sqrt(3) were both
        half-angle quantities arrived at for unrelated reasons).
        Reflect in the line along unit m:  v -> m^2 conj(v)  — rotate the mirror
        onto the real axis, conjugate, rotate back. NOTE THE m^2: the mirror's
        own angle enters DOUBLED, and nothing was halved on purpose.
        Compose two:  v -> (m1 conj(m0))^2 v.  So mirrors phi apart generate a
        rotation of 2 phi, and the object built from them carries phi.
        MEASURED: 4,000 mirror pairs, 3.844e-06 against rotation by 2(beta-alpha)
        and 1.450e-06 against apply_rotor. Worked example 20°/50° -> probe walks
        0° -> 40° -> 60°.
        THE CONTROL IS THE SHARP ONE: mirrors in the other order give -60°, not
        60°. REFLECTIONS DO NOT COMMUTE EVEN THOUGH C DOES, because a reflection
        is a multiplication AND a conjugation and the bar falls on a different
        factor. The non-commutativity 7.4 has to live with is ALREADY HERE, in
        the plane, in the operation rotations are made of.
  plane-schedule: A BLEND CAN TAKE THE RIGHT PATH ON THE WRONG SCHEDULE, AND
        THAT IS A THIRD CATEGORY 7.1 AND 7.2 DID NOT HAVE. 7.3 §10.
        nlerp CANNOT leave the geodesic — the chord stays in the plane the
        endpoints span, and normalising changes a modulus and never an argument
        — so its excess turning is 0.00%, the same number 7.2's geodesic gives.
        What it gets wrong is WHEN:
          fastest/slowest = 2 tan(Omega/2) / sin(Omega) = sec^2(Omega/2)
        Derived from phi(t) = arctan(u tan(Omega/2)) with u = 2t-1 (the chord is
        a VERTICAL LINE when the endpoints are put symmetric about the real
        axis, which is what makes the algebra one line). MEASURED at seven arcs,
        worst relative 2.3e-03, including 13134.350 vs 13131.480 at 179°.
        THE NUMBER TO DESIGN WITH is the gap in degrees at the same t:
        0.005° at 10°, 0.13° at 30°, 4.07° at 90°, 26.34° at 150°, 76.65° at
        179°. Peak at t = 0.2386 for a 90° arc. That turns "slerp or nlerp?"
        into A THRESHOLD ON THE ARC rather than a preference.
        SLERP IS THE SAME THREE LINES IN EVERY REPRESENTATION:
        slerp(a,b,t) = a (a^-1 b)^t. 7.2's rotation_slerp on mat3 is this in
        another notation, and 7.5's quaternion slerp will be this verbatim.
        THE CONTROL 7.1 ALREADY WARNED ABOUT: in the plane a rotation has ONE
        parameter, so lerping the angle IS the geodesic and the instrument reads
        0.00% for a trivial reason. The wasteful control is the LONG WAY ROUND,
        checked against the exact 360 - Omega rather than a guessed threshold.
  axis-angle-amplify: ONE FACTOR SETTLES THE WHOLE EXTRACTION, AND IT IS THE
        SINGLE MOST USEFUL FACT IN THE FILE. 7.2 §8.3.
          orientation error  =  2 sin(theta/2)  x  axis error
        From cos(Theta/2) = cos^2(theta/2) + sin^2(theta/2) cos(phi). ZERO at
        theta = 0 and TWO at theta = pi. So THE TWO HOLES ARE NOT EQUALLY
        DANGEROUS: the axis becomes unrecoverable at the identity and it costs
        nothing, because whatever nonsense comes back is multiplied by ~0; it
        becomes hard to find at a half-turn, which is exactly where the
        multiplier peaks. A library giving both ends the same epsilon and the
        same fallback has misunderstood its own problem.
        MEASURED against a brute-force sweep, worst relative error 2.3e-05, and
        1.41420 at 90° which is sqrt(2). THE STRONGEST FORM: the placeholder
        axis at theta = 1e-6 is 78.30° wrong and costs 7.280e-05° of
        orientation; the law predicts 7.830e-05°. Harmless is arithmetic, not
        hope.
        COMPOSED WITH THE SKEW ROUTE'S OWN CONDITIONING eps/(2 sin theta), the
        round-trip orientation error is eps/(2 cos(theta/2)) — finite at 0,
        unbounded at pi. One expression, both ends.
  axis-angle-routes: A CROSSOVER IS A BAND, NOT A POINT, AND MEASURING SAID SO
        BETTER THAN THE DERIVATION DID. 7.2 §8.4.
        Skew route divides by 2 sin(theta); symmetric route by (1 - cos theta)
        with the pivot >= 1/sqrt(3). Setting the errors equal gives
        sqrt(3) sin theta = 1 - cos theta, i.e. tan(theta/2) = sqrt(3), i.e.
        k_axis_angle_reversal_angle = 2pi/3 = 120°.
        MEASURED on matrices carrying 1e-7 ABSOLUTE error, 4,000 axes/probe:
        the two run WITHIN 25% OF EACH OTHER FROM ~95° TO ~140° and the winner
        FLIPS from probe to probe. Outside it is decisive: skew 6.8x better at
        30°, symmetric 569x better at 179.9°. 120° is in the middle of a band
        where the choice does not matter — the best place for a threshold, not a
        compromise between two bad options.
        FIRST DRAFT PRINTED A WINNER COLUMN and summarised "crossover between
        120° and 110°", which is not an interval. A ratio says "they are tied";
        a winner cannot. PRINT THE RATIO, NOT THE VERDICT.
        CHEAPEST CHECK THAT A THRESHOLD IS NOT ARBITRARY: at the switch the two
        routes must AGREE. Measured 1.354e-05° apart over 20,000 axes.
        THE HARNESS HAS ITS OWN COPY OF BOTH ROUTES, and must: the engine runs
        one per call, so THE ENGINE CANNOT MAKE THIS MEASUREMENT ABOUT ITSELF.
        You cannot find where two curves cross by plotting one of them.
        k_axis_angle_identity_angle = 1e-5, set where the axis stops being a
        usable DIRECTION (measured 0.5150° error there, scaling exactly as
        1/theta) rather than where it stops being computable.
  axis-angle-slerp: THE GEODESIC, AND 7.1'S INDICTMENT ANSWERED ON 7.1'S OWN
        PAIRS. 7.2 §9. slerp(A,B,t) = A * R(n, t*theta) with (n,theta) the
        axis-angle of A^T B. Exact at both ends by construction; the geodesic
        between, because turning steadily about one fixed axis is what "straight
        line" means here. TAKES THE SHORT WAY FOR FREE (angle already in
        [0, pi]) where 7.1 needed shortest_angle_delta per angle, three times:
        yaw 170 -> -170 is 340.0° raw and 20.0° slerped.
        MEASURED, same instrument, same pairs 7.1 published:
          generic pair   Euler 204.29° / 179.05° = +14.10%, speed 1.556x
                         slerp 179.05° / 179.05° = -0.00%,  speed 1.000x
          four bands     Euler +209.5 / +62.5 / +22.5 / +26.7 %
                         slerp   -0.00 / +0.00 / +0.00 / -0.00 %
        THE EULER COLUMN IS THE CONTROL and reproduces 7.1 to the digit. A claim
        of zero excess is worth nothing unless the instrument producing it can
        still produce 14.10% for the thing that genuinely detours.
        THE SLERP COLUMN CANNOT VARY, and that is the word: a geodesic performs
        the turning required and there is NO OTHER NUMBER it could report. The
        four rows check the code, not the claim.
        THE BILL, and it is 7.4's reason to exist: 31.46 ns/call, of which 82%
        is the two trig-bearing stages. AND AXIS-ANGLE DOES NOT COMPOSE AT ALL —
        no usable closed form for "do this then that", so every composition goes
        out to a matrix and back. Fine interface, fine interpolator, bad storage.
  eulers-theorem: DET(R - I) = 0, AND THE ONLY LINE THAT MENTIONS THE DIMENSION
        IS (-1)^3. 7.2 §4. det(R-I) = det(R^T)det(R-I) = det(I - R^T)
        = det(I - R) = (-1)^3 det(R-I), so x = -x, so it is zero, so R - I
        collapses a direction, so an axis exists. IN 4-D THE SAME LINE READS
        (-1)^4 = +1 and proves NOTHING — and the theorem is genuinely FALSE
        there: a double rotation (35°, 50°) fixes nothing, measured
        det(R - I) = 0.25840, matching the eigenvalue prediction
        (2-2cos a)(2-2cos b) to five decimals. The harness carries its own 4x4
        determinant because mat3 cannot hold a disproof of a claim about mat3.
        THE HONEST GENERAL STATEMENT: rotation happens in a PLANE. In n
        dimensions it decomposes into floor(n/2) perpendicular planes; 3-D is
        where that is ONE plane AND a plane has a unique normal. Two
        coincidences, both needed, both gone in 4-D.
  euler-interp: A ONE-KNOB EULER LERP IS ALREADY A GEODESIC, WHICH IS WHY THE
        OBVIOUS TEST MEASURES NOTHING. 7.1 §7. The first draft swept the yaw
        alone at pitch 88° and correctly reported 0.00% excess: turning one
        angle is a steady rotation about one fixed axis, at any pitch.
        WITH TWO OR MORE MOVING: generic pair, 204.29° of turning performed for
        a 179.05° journey — 25.24° of detour, +14.10%, and the RATE varies by
        1.56x, which is the part a player sees. The same three deltas at four
        pitch bands: +209.5% (87->30, near lock), +62.5%, +22.5%, +26.7%. The
        control is the LAST row, not a different move — change the pose, never
        the deltas, or the comparison measures the move.
        NOT MONOTONE AT THE BOTTOM, and that is real: the endpoints differ
        between bands so the geodesic differs too, and excess is a ratio. The
        claim the table supports is "near lock is catastrophically worse".
        THE WRAP BUG IS SEPARATE AND HAS A SEPARATE FIX. Yaw 170 -> -170 is a
        20° turn; a raw lerp performs 340° backwards. `shortest_angle_delta`
        fixes THAT and does nothing about the detour above. std::remainder, not
        std::fmod — remainder rounds to nearest and lands in [-pi, pi].
  rotation-metric: THE TEXTBOOK FORMULA CANNOT MEASURE THE THING EVERY CLAIM IN
        MODULE 7 IS MEASURED WITH. 7.1, `angle_between_rotations`. Both
        spellings ship; the acos one is called by nothing and exists so §F can
        compare against the real thing rather than a copy of it. Measured
        relative error at a true 0.004°: atan2 form 1.24e-04, acos form 1.00
        (it returns zero). A 2,048-step path across 120° takes 0.059° steps.
        Unbiased over a long sum: 2,048 steps of a known 120.00° measure
        120.0003°.
  era-split: LESSON 5.12 WAS WRITTEN ELEVEN LESSONS LATE, AND THE FIX IS TWO TREES.
        It closes Module 5 and was authored after 6.18, so CLAUDE.md §8 ("every
        listing compiles at its point in the course") and the repository state
        disagreed. Resolution: the lesson was WRITTEN, COMPILED AND RUN in a
        checkout of 9be6c96 at scratch/era511 (gitignored), and scratch/port_512.py
        applies the deltas to produce the files that live in the repository.
        THE DELTA IS THE MEASUREMENT: 26 real source lines out of 1,485, all of
        them in TWO named places — 6.2's intensity -> irradiance, and 6.5's
        tint + specular -> material. Everything structural (ECS, hierarchy,
        camera, action map, asset store, debug lines, debug UI, app layer, the
        whole software renderer call) ported UNCHANGED.
        THE TWO BUILDS AGREE EXACTLY ON GAMEPLAY and not at all on pixels:
        `2/12 orbs, 25 objects, 380 triangles, 889 debug lines` identical;
        91.9% OF PIXELS DIFFER, max channel delta 128. That split is the whole
        story of a public API — the structural surface survived, the SEMANTICS
        (6.1's linear/sRGB, 6.2's units, 6.5's material) did not. Module 6 changed
        what "lit" MEANS, not what "draw this" means.
        THE PAGE'S LISTINGS ARE PINNED TO THE ERA COPIES (scratch/l512_*), not to
        repository paths. Reading them live would publish Module 6 spellings
        inside a Module 5 lesson — README §15's Cause A, arriving by a route that
        pinning-at-the-next-lesson cannot catch because the drift is already
        there on day one.
        verify_512.cpp IS DELIBERATELY ERA-NEUTRAL: it names no field Module 6
        moves, so ONE TEXT compiles and passes 27/27 against both engines. That
        is not tidiness — it is what lets its claims be checked against both.
  umbrella-lint: A HEADER NOBODY COMPILES CANNOT BE KEPT CORRECT BY BEING USED.
        5.12, engine/engine.hpp + engine/CMakeLists.txt. The file calls itself
        "the whole public API, in one include" and "the fastest way to see whether
        something is public". IT LISTED 40 OF THE 55 HEADERS IT COULD HAVE —
        missing the ENTIRE ECS, the asset store, handles, pools, the logger, the
        assertions and the action map, i.e. very nearly everything Module 5 built.
        THE MECHANISM, WHICH MATTERS MORE THAN THE BUG: `grep -rl engine/engine.hpp`
        over demos/ and engine/ returns the file ITSELF and nothing else. It had
        ZERO consumers, so it was never compiled, so nothing could ever fail.
        Its history is three commits — 5.1 created it, 5.2 remembered platform/,
        5.11 remembered debug_lines — and seven lessons in between did not. That
        is a rule kept by MEMORY inside the repository whose 5.1 insisted the
        boundary be "enforced by the include path, not the style guide".
        NOW CHECKED AT CONFIGURE TIME: file(GLOB_RECURSE) + file(STRINGS) +
        FATAL_ERROR, with platform/main.hpp as the ONE documented exception (an
        umbrella must not be a way to acquire a main()). A GLOB USED AS A LINT IS
        NOT THE ANTI-PATTERN — the usual objection is about globbing SOURCES,
        where a stale glob drops a translation unit; a stale lint can only fail to
        notice a header added since the last configure. PROVEN TO FAIL: deleting
        one #include makes the configure abort naming it.
        THE MATCH IS ANCHORED TO A WHOLE #include LINE, and the first draft was
        not — an unanchored grep passed on headers only MENTIONED IN A COMMENT,
        including platform/main.hpp, which the file names in prose precisely to
        say it is absent. A check that reads its own excuse as compliance is
        worse than no check.
        AND THE COST MEASUREMENT CAME OUT BACKWARDS. One TU, best of five:
          nothing                     0.01 s
          umbrella as shipped (40)    0.28 s   <- CHEAPER than explicit
          the game's own 23 includes  0.37 s
          umbrella completed (55)     0.39 s
        The broken umbrella was cheap BECAUSE it was incomplete: the fourteen it
        omitted are the templated ones (registry, view, pool, asset_store,
        actions). Completing it costs +39% against the broken version and +5.4%
        against including what you use, so THE SINGLE-FILE NUMBER IS NOT THE
        REASON TO AVOID AN UMBRELLA. The reason is 5.1's incremental rebuild,
        which no single-file benchmark can see. Both numbers are in the header,
        because a reader who finds only one draws the wrong conclusion either way.
  renderable: THE ENGINE DEFINES A COMPONENT WHEN, AND ONLY WHEN, AN ENGINE
        SYSTEM READS IT. 5.12, engine/gfx/renderable.{hpp,cpp}. Keeps the promise
        gfx/scene.hpp has carried since 5.1 — "Module 5's ECS replaces the struct
        with components" — which Module 5 had not kept and 5.12 is its last chance
        to. The rule is not a preference: ecs/hierarchy.hpp already defines
        `parent` and `world_transform` because hierarchy::resolve CANNOT BE
        COMPILED against "whatever you happen to call parent". So `renderable` is
        the engine's; `rover`, `collectible`, `spinner`, `bobber`, `carousel` and
        `pillar` are the game's and nothing under engine/ will ever look at them.
        HOISTED AT TWO CALLERS, AGAINST OUR OWN THREE-CALLER RULE, and the reason
        is narrow: what is duplicated is not an IDIOM, it is a known-broken
        CONVERSION. ecs_swarm's four lines put a mat4's whole linear part into a
        field named `rotation`, and its own comment predicted "Module 6 gives the
        renderer a matrix directly and this function loses its last four lines".
        MODULE 6 HAS BEEN WRITTEN AND scene_object STILL HOLDS A transform
        (scene.hpp:104), so the prediction did not come true and the trick is
        still load-bearing. A stable idiom twice is a coincidence; a workaround
        twice is two places to fix, one of them inside a demo nobody greps.
        IN gfx/ AND NOT ecs/, because the arrow points at the more general: a
        registry has no opinion about meshes, and physics, serialization and a
        future editor all want entities and will never want a mesh_handle.
        THE REGISTRY IS FORWARD-DECLARED, not included — `namespace engine::ecs
        { class registry; }` — so a TU that merely STORES a renderable compiles
        none of the ECS. Same trick gpu_scene.hpp uses for instance_batch.
        NO `visible` FLAG, DELIBERATELY: 5.8 spent a demo arguing that an entity
        is invisible because it LACKS a component. remove<renderable>(e) costs
        one structural change (~4 ns, 5.7) and makes the entity genuinely cheaper
        rather than merely skipped. NO `name` either — one caller has asked.
        THE CONVERSION IS EXACT AND IT IS CHECKED. Setting scale to 1 and letting
        `rotation` absorb the whole linear part performs NO ARITHMETIC: nine
        numbers copied, three copied, and parent_from_local's multiply is by I.
        verify_512 §B: max element difference EXACTLY 0, with a control that
        reports 1.0 on a 1.0 nudge.
        THE SIGNATURE TAKES A MUTABLE registry AND READS NOTHING, on purpose.
        registry::view() has no const overload and ecs::view hands out Ts&, so
        there is no way to spell "I will walk this and change nothing". Reported,
        not fixed (it changes two published headers); Exercise 4. A comment nobody
        reads is a worse bug report than a parameter everybody trips over.
  checkpoint-findings: SEVEN, FROM ONE GAME, AT HOUR 62 INSTEAD OF HOUR 434.
        5.12. FIXED: the umbrella (above) and the ECS->renderer bridge (above) —
        and BOTH were promises already on record, which is the pattern worth
        remembering: the cheapest findings are the ones the codebase already
        told you about in a comment nobody re-read.
        REPORTED AND LEFT: (a) view() has no const overload; (b) NO PUBLIC PATH
        FROM AN ENTITY TO THE GPU RENDERER — gpu_scene_renderer consumes
        gpu_draw_item, the ECS produces components, and sandbox bridges it in
        ~700 lines PRIVATELY inside its own main.cpp, which is exactly why nobody
        noticed: the one program that ever crossed it predates the boundary.
        So every demo written from OUTSIDE since 5.1 renders on the CPU, and
        three lessons after moving to the GPU the real renderer is the one a game
        cannot reach (Module 9's facade); (c) no collision — the arena wall is a
        six-line position CLAMP and the eight pillars are scenery you drive
        through, with their OBBs drawn from the world matrix a solver would use,
        so THE DATA IS PRESENT AND THE SOLVER IS NOT (Module 8); (d) no text —
        the HUD is SDL_RenderDebugTextFormat, which exists only on
        surface::renderer, so `--shot` produces a picture of the game with NO
        SCORE ON IT and the GPU path cannot show a number either (Module 6);
        (e) no audio, so the only rewarding act in the game is silent (7.8);
        (f) a program cannot NAME its own log category — log_category_count is
        the extension point and works, but name_of() returns "?" so --log cannot
        address it; which is why the --shot summary is a printf and not a log.
        A log is a DIAGNOSTIC; a tool's result is OUTPUT, and a tool whose result
        only appears with the right --log spec is a tool with a trapdoor.
  unit-cube-trap: "SCALE" MEANS TWO DIFFERENT THINGS IN ONE HEADER, AND IT BIT
        THREE TIMES IN ONE FILE. 5.12. cube_mesh() and quad_mesh() span +/-0.5,
        so `scale` is the FULL SIZE; icosahedron_mesh()'s vertices are at
        distance 1.0, so `scale` is a RADIUS. At scale 1 the ball is 2x the
        diameter of the cube (verify_512 §D pins all three as facts).
        THE TRAP IS NOT THE ASYMMETRY. It is that a HALF-EXTENT is what the rest
        of the program has in its hand — a collision test wants one,
        debug_lines::box takes one — so the value you reach for is wrong by two
        at exactly the moment you reach for it. The three: the floor came out a
        QUARTER of its area with the pillars floating beside it (which reads as a
        camera bug for ten minutes); boxes were half-buried because `half.y` is
        the centre height and the scale is twice that; and the debug OBBs were
        EXACTLY 2x too big because box(world_from_local, half) takes the extent in
        the MATRIX'S OWN SPACE and the matrix already carries the scale — which
        looks like a deliberate collision margin. A plausible margin that is
        exactly a factor of two is never a margin.
        ANSWERED WITH ONE FUNCTION, box_at(centre, half, rotation=I), whose
        entire content is the doubling.
  boom: A PARENTED CAMERA IS NOT A FOLLOW CAMERA, AND THE ALGEBRA HAS ONE TRAP.
        5.12. 5.9 made the camera an entity, so parenting it to the player gives a
        rigid follow for free — and it inherits the WHOLE basis, including 22
        degrees of cosmetic roll (read as the WORLD tilting, because the camera is
        the viewer's inner ear) and the rover's NON-UNIFORM SCALE.
        The fix is one more link, a boom, and:
          L = (H * Rz(bank) * S)^-1 * H = S^-1 * Rz(-bank)
        THE INVERSE OF A PRODUCT REVERSES ITS FACTORS, so the unscale comes
        FIRST. Writing it the way the English reads — "undo the roll, then undo
        the scale" — gives Rz(-bank) * S^-1, a different matrix because a rotation
        and a non-uniform scale do not commute. On the x axis the two agree to
        FOUR DECIMAL PLACES and on y they differ by 36%, which is what makes it
        expensive: close enough to look like a tuning problem.
        AND THE SYMPTOM WAS NOT A TILTED HORIZON. view_from_camera asserts
        is_rigid (5.9), the assertion fired correctly on frame 1, and the HEADLESS
        --shot RUN HUNG FOR EVER WITH NO OUTPUT. SDL_assert expands to a `while`
        loop so RETRY re-tests (5.3 made a point of this); with no display for a
        dialog and no terminal to prompt at, the default answer keeps arriving and
        the condition cannot change. AN ASSERTION IS DEVELOPER-FACING CONTROL
        FLOW, and `--shot` is the one configuration with no developer in front of
        it. Diagnosed with `sample <pid>`: the assert's own function at the top of
        1,538 samples. Not changed here; Exercise 5 installs a handler that
        aborts when SDL_WasInit(SDL_INIT_VIDEO) is false.
        verify_512 §E builds all three chains — right, swapped, and no boom —
        and the third is the CONTROL: without it, "the swapped one is not rigid"
        would be equally consistent with the boom doing nothing at all.
  golden-ordinal: 5.12 DECLINES TO NUMBER ITS BYTE-IDENTICAL GOLDEN, and this is
        deliberate. Every lesson since 5.1 numbers the run ("the eleventh", "the
        twelfth"), and 6.1 — ALREADY PUBLISHED — claims the twelfth. Inserting
        5.12 before it would either collide or force an off-by-one correction
        through EIGHTEEN published pages, for a count no reader can check. So the
        page and golden_512.cpp say "byte-identical, as it has been since 5.1"
        and claim no position. DO NOT "FIX" THIS LATER.
  frame-graph: THE FRAME IS DECLARED, NOT ASSEMBLED. Built in 6.17,
        engine/include/engine/gfx/frame_graph.{hpp,cpp}.
        THE WHOLE THING RESTS ON ONE MOVE: a resource is NOT a texture, it is a
        VARIABLE WITH VALUES OVER TIME. `hdr@1` is what the scene pass produced,
        `hdr@2` is what the skybox produced from it, and each version has EXACTLY
        ONE PRODUCER. That is single static assignment applied to render targets,
        and it is the reason "the bloom reads the HDR target" stops being an
        ambiguous sentence.
        ONE STATEMENT, FOUR DERIVED FACTS. A pass says what it does with what was
        already in a target — `discard_write` (I cover every texel),
        `clear` (give me a known value), `keep` (my output DEPENDS on the old
        contents) — and from it fall: the ORDERING edge, the LOAD op, the
        PRODUCER's STORE op, and the LIFETIME. The third is the one that
        surprises: it is a consequence for a DIFFERENT PASS than the one that
        spoke.
          STORE iff some live pass consumes this version, or the resource is
          imported. DONT_CARE otherwise.
        That single rule reproduces 4.7's DONT_CARE on the scene depth and 6.8's
        STORE on the shadow map — two decisions argued out in comments nine
        lessons apart. verify_617 §D checks eight attachments and all eight
        match what the engine chose by hand.
        THE LOAD OP IS THE ONE THAT STINGS. 6.13 wrote a paragraph warning that
        the upsample's LOAD is "load-bearing" and that getting it wrong "looks
        like a tuning problem rather than a load op". Under the graph it is not a
        decision; it follows from `keep`. NOTE THE HONEST LIMIT: the decision was
        MOVED, not abolished — `discard_write` is still a promise that the pass
        covers every texel, and that promise is the one thing the graph cannot
        check, because it would have to know what the shader covers.
        CULLING IS BACKWARD REACHABILITY FROM IMPORTED WRITES. An import is the
        only value observable after the frame ends, so a pass survives iff it
        transitively feeds one. No present() call, no side-effect flag. Measured:
        stop sampling the bloom's level 0 and ELEVEN PASSES DISAPPEAR, replacing
        gpu_post.cpp's `if (!s.enabled) { return; }` with a consequence. Nothing
        in frame_graph.cpp knows what a bloom is.
        DETERMINISM IS CORRECTNESS, NOT TIDINESS. Kahn's algorithm picks any
        ready pass; ties are broken by DECLARATION INDEX, deliberately, with the
        slowest possible scan. A scheduler that wanders produces a golden that
        fails one run in five, and by the time anyone sees the pattern the
        scheduler is the last thing suspected.
        THE GRAPH ORDERS PASSES, NOT DRAWS, and that boundary is what keeps
        6.11's back-to-front tail and 6.16's batching structurally out of reach.
        The general rule: ordering that comes from DATA is safe to hand over;
        ordering that comes from SEMANTICS must live inside an atom the scheduler
        cannot open. Here the atom is the render pass.
  frame-graph-memory: THE USUAL PITCH DOES NOT APPLY HERE AND THE TWO REASONS
        MUST BE TOLD APART. 6.17 §7. Measured on the engine's own 14-pass frame:
          no sharing at all        1,223,296 B   100.0%
          true memory aliasing     1,048,576 B    85.7%   <- the literature
          descriptor-keyed reuse   1,223,296 B   100.0%   <- what we can express
        SO THE PRIZE IS 174,720 B (14.3%) AND WE COLLECT NONE OF IT.
        (1) THE API. SDL_GPU 3.4.12 has NO placed resource, NO heap, NO aliasing
            flag — checked against SDL_gpu.h, not assumed. The strongest reuse
            available is handing back a WHOLE texture whose descriptor matches
            EXACTLY (width, height, format, samples, layers, depth, sampled).
            `shadow` is 256x256 sampled depth and dies at pass 1; `bloom 0` is
            128x128 RGBA16F and is born at pass 2. 262,144 dead bytes could hold
            131,072 live ones and there is no call that asks.
        (2) THE SHAPE. OUR FRAME IS A CHAIN. `hdr` spans the whole schedule
            because the resolve samples it last, and all six pyramid levels are
            live at the turn between the down chain and the up chain — which is
            what an additive pyramid MEANS. A chain has nothing to alias.
        AT 1920x1080 WITH A 2048 SHADOW MAP: 45.00 MB unshared, 39.73 MB with
        perfect aliasing, 5.27 MB unreachable — which, worked out, is EXACTLY the
        bloom pyramid, which would fit inside the shadow map's memory after the
        shadow map dies.
        THE POOL IS BUILT ANYWAY AND SHOWN WORKING: a graph shaped like a TREE
        (two half-res effects used one after the other) puts both scratch targets
        on one slot and returns 131,072 B. That is the shape a post stack takes
        once SSAO, reflections and depth of field arrive.
        THE GENERAL PROBLEM HAS A NAME AND WE ARE NOT SOLVING IT: interval graph
        colouring, which for intervals on a line is optimal by a greedy sweep in
        first-use order (what the pool does). What makes real allocators hard is
        that TRUE aliasing gives the intervals different SIZES, which is bin
        packing with lifetimes.
  frame-graph-cost: FIXED CAPACITIES, AND THE REASON IS MEASURED. 6.17 §8.
        8.62 us per declare+compile in the steady state (15.83 us on the first,
        which creates 9 textures), and ZERO HEAP ALLOCATIONS across 200 cycles —
        measured by REPLACING THE GLOBAL operator new in verify_617, not by
        asserting it. 0.05% of a 16.67 ms budget, ~1/300th of what 6.16's culling
        test costs on 45,512 boxes.
        THAT IS THE ARGUMENT AGAINST std::function, made with a number rather
        than with taste: it type-erases, so any callable bigger than its small
        buffer heap-allocates, and the lambdas you would write here (capturing a
        renderer, a settings block, a draw list) do not fit. One allocation per
        pass per frame, invisible. A function pointer plus a void* is two words.
        THE COST OF THAT CHOICE IS REAL AND IS STATED: the context structs must
        OUTLIVE THE GRAPH, because it points at them. verify_617 keeps them in
        one `frame_decl` beside the graph.
        COMPLEXITY IS O(P^2 x A^2) because producer_of is a linear scan called
        from inside two loops. FINE at 14 passes, not at 200; the fix is an index
        from (resource, version) to producer, and it is Exercise 3.
  frame-graph-limits: THE NINETY-PERCENT PICTURE, STATED. NO SUBRESOURCE
        VERSIONS — a version covers the whole texture, so four cascade passes
        writing four LAYERS serialise into sm@1->sm@2->sm@3->sm@4 even though
        they are independent. It costs NOTHING today (a command buffer submits in
        order anyway) and would cost something on an API with parallel queues.
        NO ASYNC COMPUTE / QUEUES / SPLIT BARRIERS — SDL_GPU presents one
        timeline to this API. NO BUFFERS, only textures, because every cross-pass
        resource in this engine is one. ONE COMMAND BUFFER.
  pass-recording-split: A FUNCTION THAT NEVER SEES A LOAD OP CANNOT GET ONE
        WRONG. 6.17 cut two functions in half and the SHAPE of both cuts is the
        same: the half that draws loses everything about the TARGET.
          gpu_shadow_map::render_into(cb, pass, items, count, cam, log) — and the
            `layer` parameter did NOT come with it, because which cascade a pass
            writes is part of the WRITE DECLARATION, the only place that can also
            know whether the previous cascade must survive.
          gpu_bloom::record_stage(cb, pass, bloom_stage, source, sw, sh, s, exp)
            — no destination at all. bloom_stage is {bright, down, up}. The
            SOURCE dimensions are parameters rather than read off the texture,
            because the bright pass's texel_size is the SCENE's while its target
            is half that, and confusing them gives a glow of the wrong radius and
            no error.
        AND THE OLD PATH NOW GOES THROUGH THE NEW ONE. gpu_bloom::render was
        REWRITTEN to call record_stage eleven times. Not tidiness: §I compares
        the two chains bit for bit, and two copies of the same draw code would
        make that comparison pass while proving nothing. Fourth time this engine
        has paid for the rule (5.1's four harnesses, 6.15's two ARGB spellings,
        6.16's begin_frame, this).
  destroy-completeness: A destroy() THAT HAS FALLEN BEHIND ITS MEMBERS IS
        INVISIBLE. Found in 6.16: gpu_scene_renderer::destroy() had been missing
        6.15's three members (black_cube_, black_lut_, cube_sampler_) since they
        were added, and nothing noticed because a leaked GPU resource at shutdown
        looks exactly like a clean shutdown. THE WAY IT WAS FOUND IS THE
        TRANSFERABLE PART: add a member, then READ THE DESTROY LIST against the
        member list. Do that every time a class gains a resource.
  brace-elision-ambiguity: `aabb::expand({1, 2, 3})` IS AMBIGUOUS. Brace elision
        makes the braced list a candidate for BOTH the vec3 and the const aabb&
        overloads, so the call does not compile — and it had never come up
        because every call site in the engine happened to pass a named variable.
        Write `expand(vec3{...})`. The general shape: an overload set where one
        type is constructible from a prefix of another's initialiser is ambiguous
        under brace elision, and only aggregate-initialised call sites reveal it.
  frustum: SIX PLANES, FROM THE ROWS OF clip_from_world, NEVER FROM A CAMERA.
        Built in 6.16, engine/include/engine/gfx/frustum.{hpp,cpp}.
        THE DERIVATION IS ONE OBSERVATION: a clip coordinate IS a signed distance.
        `x >= -w` is `x + w >= 0` is `(row0 + row3) . p >= 0`, and that is a plane
        in (a,b,c,d) form. No trigonometry, and — the actual reason — NO SECOND
        COPY OF THE TRUTH. A geometric construction from eye/fov/aspect silently
        disagrees with the matrix the renderer uses the moment anyone builds an
        off-centre (VR), sheared (mirror), oblique (water clip) or reversed-Z
        projection, and the symptom is objects vanishing at the frame edge, which
        reads as a RENDERER bug.
          left  = row0 + row3      right = row3 - row0
          bottom= row1 + row3      top   = row3 - row1
          NEAR  = row2 ALONE       far   = row3 - row2
        THE SIGN GOES ON THE SECOND ROW. `row0 - row3` instead of `row3 - row0` is
        the same plane reversed; the culler then rejects the whole scene and the
        TELL is that opposite planes report EXACTLY negated distances. verify_616
        §A asserts on that directly, because it is the one symptom that names the
        cause.
        THE NEAR PLANE IS THE CONVENTION. SDL_GPU clips 0 <= z <= w, so near is
        row2 with nothing added. Every OpenGL-derived article (and the 2001 Gribb
        & Hartmann note everything descends from) writes row2 + row3, which under
        our range puts the near plane roughly a FAR distance behind the camera —
        so nothing within the visible range is ever culled and the symptom is
        "culling doesn't seem to help much", which nobody debugs.
        NORMALISED AT EXTRACTION, always, though the box test does not need it
        (sign survives any positive scale). Two things do: the sphere test
        compares a distance against a RADIUS, and every printed distance is
        meaningless in unknown units. Six square roots per camera per frame.
        VERIFIED AGAINST FACTS, NOT A REFERENCE. The eye is the APEX, so its
        distance to all four side planes is exactly 0.000000, and to near is
        -near. AND FOUR ZEROS ARE NOT ENOUGH: a reversed normal leaves the apex
        ON the plane, so all four still pass. That is the bug the first draft
        made. Checks 2 (near = -near) and 4 (opposite planes not negated) exist
        for exactly that.
  frustum-precision: THE FAR PLANE IS 1.4e-3 OUT OF PLACE AND IT IS NOT THE
        EXTRACTION'S FAULT. 6.16 §4, and the method matters more than the
        finding. The obvious diagnosis is cancellation in `row3 - row2` (three
        digits of agreement wiped out, visible in the rows). REDOING THE SAME
        SUBTRACTION IN DOUBLE, FROM THE SAME FLOAT MATRIX, REPRODUCES THE FLOAT
        ANSWER TO SEVEN DIGITS — which rules the subtraction out.
        The error is upstream, in perspective(): A = far/(near-far) = -1.003009,
        and the far plane solves to -B/(A+1). `A + 1` is a cancellation of two
        nearly-equal numbers IN THE MATRIX BUILDER, dropping ~8 bits and
        amplifying A's last ulp by 332x. The float matrix's far plane sits at
        -99.998217; in double it is exactly -100.
        NOT WORTH FIXING (1.8 mm at 100 m, in the depth range 4.7 measured as
        worthless anyway) and worth KNOWING, so a real precision problem later is
        not blamed on the culler. It is also a second independent argument for
        reversed-Z: A + 1 does not cancel when near and far swap roles.
        The near plane, being row2 ALONE with no subtraction, is accurate to
        3.0e-7 — 4557x better, measured.
  culling-cost: THE MEASUREMENT THAT MATTERS IS NOT THE CULL RATE. A cull rate
        measures the SCENE, not the technique: point the camera at a wall and
        cull 99%, at the sky and cull nothing.
        A SWEEP OVER OBJECT COUNT FOUND NOTHING, and the null result is the
        lesson: the test is O(n), the work it skips is O(n), so their ratio
        CANNOT depend on n. Six confident data points along an axis the effect
        does not depend on look exactly like a measurement. BEFORE SWEEPING A
        PARAMETER, ASK WHAT WOULD HAVE TO BE TRUE FOR THE ANSWER TO DEPEND ON IT.
        THE CROSSOVER IS IN THE CULL RATE, and it is predictable: culling costs c
        per object ALWAYS and saves w per object REJECTED, so break-even is
        r = c/w. Measured: c = 58.3 ns (the culler on a scene that rejects
        nothing), w = 23,821 ns (collect_triangles per object). Predicted 0.2446%
        — ONE OBJECT IN 409 — then bracketed between the 0% and 0.4% samples. At
        0% culled the loss is 53.7 ns/object, i.e. exactly c.
        THE SAME CULLER IS A CLEAR WIN ON ONE RENDERER AND A WASH ON ANOTHER.
        Replace collect_triangles with a GPU submission costing a few hundred ns
        of CPU and the break-even rate climbs into the tens of percent.
        COST PER OUTCOME IS ASYMMETRIC AND BACKWARDS FROM MOST OPTIMISATIONS:
        6.0 plane evaluations per SURVIVOR (all six), 1.95 per REJECT (early
        exit). A scene that culls heavily is cheaper per object to cull.
  bounding-volumes: TWO SHAPES, OPPOSITE PROPERTIES, AND 6.8 AND 6.16 CHOOSE
        DIFFERENTLY ON PURPOSE.
        6.8's shadow_map::bounds_of walks EVERY VERTEX for a tight world box,
        because shadow texel density is inversely proportional to the box's side.
        6.16 must NOT: walking every vertex to decide whether to skip a draw is
        the very work being avoided, done eagerly. It caches an OBJECT-SPACE box
        and runs `transformed()`, which returns the box around the transformed
        BOX. SAME TWO FUNCTIONS, OPPOSITE CHOICES, BOTH CORRECT.
        THE PRICE IS MEASURED: 10 of the demo's 12 objects inflate by EXACTLY
        1.0000 (their transforms are axis-aligned), the two rotated meshes by
        2.41x and 1.90x, summed scene volume 1.40x. Worst case for a cube is
        3*sqrt(3) = 5.196.
        THE POSITIVE VERTEX: per axis take max where the normal's component is
        positive, min where negative. `n . c` is a sum of three INDEPENDENT terms,
        so maximising the sum means maximising each — three compares, no search,
        no loop over eight corners. Testing the NEGATIVE vertex too gives the
        third state, `inside`, which is what lets a hierarchy accept a subtree in
        one test. We have no hierarchy; the counter is there anyway, because
        counting is how you find out whether one would pay: 25,859 of 45,512 kept
        boxes were wholly inside.
        THE SPHERE PRE-TEST DOES NOT PAY, measured in BOTH regimes: 1.99x slower
        on a mostly-visible scene (nothing rejected, pure added work) and STILL
        1.61x slower on a mostly-hidden one (bounding_sphere is 2.72x the box's
        volume, so boxes the box test rejects immediately survive the sphere and
        get tested twice). THE PRINCIPLE IS NOT "PRE-TESTS ARE BAD": a filter
        cascade pays only when the cheap filter is MUCH cheaper AND NEARLY AS
        SELECTIVE. This one is half the price and substantially less selective.
        The flag stays, with its number in the comment, because the answer
        changes for an expensive precise test (OBB, convex hull).
  conservatism: CONSERVATIVE IN TWO DIRECTIONS, AND ONLY ONE IS ALLOWED TO BE
        WRONG. A box that straddles is KEPT (the price of bounding anything).
        AND — the corner case — A BOX CAN PASS ALL SIX HALF-SPACE TESTS WHILE
        BEING ENTIRELY OUTSIDE, by poking past each plane with a DIFFERENT
        corner. Measured over 200,000 random boxes: 316 false positives, 0.69% of
        those KEPT. Not rare enough to forget, not common enough to pay for a
        better test (the fixes roughly double the cost to recover 2/3 of 1%).
        THE ASYMMETRY IS THE ENTIRE LICENCE: keeping something invisible costs a
        draw call; rejecting something visible is a hole in the picture. ZERO
        visible boxes were rejected, and that is the only correctness figure in
        the section — everything else is performance.
        KNOW WHICH WAY YOUR INSTRUMENT LEANS. The ground truth samples a lattice,
        so it can MISS a thin sliver of overlap and under-report visibility —
        which makes the quoted false-positive rate an UPPER bound.
  instancing-engine: BUILT IN 6.16, engine/include/engine/gfx/instancing.{hpp,cpp}.
        The mechanism has existed since 4.5 (instance-rate input, instance_buffer,
        draw(pass, n)) and was fed by nothing for eleven lessons.
        THE BYTES DO NOT MOVE; THE CALLS DO. gpu_instance is 112 bytes, EXACTLY
        sizeof(object_uniforms), and a static_assert keeps it so. Instancing a
        hundred objects removes a hundred pushes, a hundred binds and 99 draw
        calls — and not one byte of per-object data. "INSTANCING SAVES BANDWIDTH"
        IS A COMMON AND WRONG SUMMARY; it saves SUBMISSION, a CPU cost.
        MEASURED on 16 cubes: uniform road 3152 B over 16 draws; instanced 640 B
        + 1792 instance B = 2432 B over ONE draw. The whole 720-byte saving is
        15 x 48, i.e. fifteen MATERIALS not pushed. The 112-byte block changed
        road.
        A 4x4 IS FOUR ATTRIBUTES. A vertex attribute is at most four components in
        every backend, so a matrix is four consecutive float4 the shader
        reassembles — and HLSL's float4x4(a,b,c,d) takes ROWS while engine::mat4
        stores COLUMNS, so scene_instanced.vert TRANSPOSES, deliberately and with
        the reason written down. Quietly reordering the attributes until the
        picture looks right also works and leaves the next reader unable to tell a
        convention from a coincidence.
        NO INSTANCED FRAGMENT SHADER. scene.frag.hlsl is bound unchanged, because
        instancing is a vertex-stage question — so PBR, normal maps, cascades,
        alpha modes and IBL all arrive unported. A pipeline is the PAIR, so it is
        still a new pipeline (the tenth).
  batch-key: FIVE THINGS, NOT ONE, AND THAT IS WHY INSTANCING IS A CONTENT
        DECISION. One instanced draw has one pipeline, one set of bound textures
        and one set of pushed uniforms, so everything that is not per-instance
        vertex data is shared: (mesh, albedo, normal map, surface style, blend
        style, MATERIAL). The material is 32 bytes of floats two objects do not
        match on by accident.
        SPELLED OUT, NOT HASHED, because a batch that did not form is a QUESTION —
        "which of these five did they differ on?" — and a 64-bit hash cannot
        answer it. batch_report counts splits per field, in the order a content
        author can act on (mesh = modelling, texture = atlasing, material =
        parameters).
        THIS ENGINE'S OWN DEMO SCENE MAKES 12 BATCHES FROM 12 OBJECTS, every split
        on the mesh, a saving of exactly ZERO. That measurement is in the lesson
        instead of a claim that instancing helps.
        BLENDED DRAWS ARE NEVER MERGED, and it is correctness, not laziness: 6.11
        established `over` is not commutative, so the back-to-front order is part
        of the picture and an instanced draw cannot express "these, in this order,
        interleaved with those". They come back as single-instance batches in the
        caller's order, checked rather than assumed.
        THE COMPARATOR SORTS ON POINTERS, so the grouping is non-deterministic
        across runs. Harmless for opaque geometry (3.1: the z-buffer does not care
        what order draws arrive in) and unacceptable for anything order-dependent,
        which is exactly why blended draws never reach it.
        batch_instances TOUCHES NO DEVICE STATE — it compares pointers, enums and
        material bytes, skips a NULL mesh and does not ask whether one is valid.
        render_batched asks that. The dividend is that batching runs headless.
  attribute-cap: A LIMIT THAT HAS NEVER BEEN REACHED CANNOT TELL YOU IT IS WRONG.
        pipeline_desc held eight vertex attributes and, past the eighth,
        `return *this` — DROPPED SILENTLY. Nothing in Modules 4 or 5 or eight
        lessons of Module 6 had ever declared a ninth. 6.16's instanced pipeline
        wants ELEVEN (4 from gpu_mesh::describe + 7 from describe_instances) and
        lost three with no diagnostic.
        WHAT CAUGHT IT was 4.5's check_layout, comparing declared attributes
        against the shader's REFLECTED inputs and reporting `missing` — an
        independent reading of what the pipeline actually declares. That is the
        whole argument for that function, arriving eleven lessons later.
        Cap now 16; attribute() ASSERTS then refuses (the refusal stays in release
        — a missing attribute is a wrong picture, a buffer overrun is worse).
  frame-setup: THE PER-FRAME PROLOGUE IS A FUNCTION, NOT A PATTERN. 6.16 split
        gpu_scene_renderer::begin_frame out of render() when render_batched
        arrived needing every one of those ninety lines and none of the loop.
        Copying them would have made two copies of one rule — the failure this
        engine has paid for twice (5.1's four hand-transcribed harnesses, 6.15's
        two spellings of an ARGB packing).
        THE SPLIT IS AT "DOES THIS CHANGE WITHIN A FRAME?", which is 4.6's
        draw-rates line: data grouped by RATE of change, not by what it describes.
        ADDING A MEMBER FOUND THREE THAT WERE MISSING. destroy() had never been
        taught about black_cube_, black_lut_ or cube_sampler_ (all 6.15). NOT a
        leak — RAII members, and every create_* destroys first — but after
        destroy() the object reported valid() == false while holding three live
        GPU objects, so "destroyed" and "empty" had stopped meaning the same
        thing. FOUND BY ADDING A MEMBER AND READING THE LIST, which is the only
        way a teardown function is ever checked. An argument for few members, and
        for Module 9's arena.
  forward-declare-to-break-a-cycle: instancing.hpp includes gpu_scene.hpp (a batch
        key is made of gpu_draw_item's fields), so gpu_scene.hpp CANNOT include
        instancing.hpp — #pragma once would resolve the cycle by giving whichever
        file was reached second a half-defined view of the other. gpu_scene.hpp
        forward-declares `struct instance_batch;` and render_batched takes a
        POINTER AND A COUNT rather than a std::span (which needs a complete type).
        The constraint landed on a spelling render() had chosen in 4.8 anyway.
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
  antialias: LESSON 6.14. TWO PROBLEMS SHARE THE NAME AND THE POPULAR CURE FIXES
        ONE. Geometric aliasing undersamples COVERAGE (a step function: energy at
        every frequency, so NO sample rate resolves it). Shading aliasing
        undersamples the LIGHTING (a lobe narrower than the pixel). MSAA
        multisamples coverage and shades ONCE per primitive per pixel — the
        asymmetry that makes 4x cost ~1.3x — so on an INTERIOR pixel its answer IS
        the single-sample answer, measured 300x from true supersampling at
        roughness 0.05. YOU CANNOT BUY SHADING SAMPLES WITH A COVERAGE FEATURE.
        THE GGX LOBE HAS A CLOSED FORM, and it is now in microfacet.hpp because it
        is a property of the DISTRIBUTION rather than of any technique consulting
        it (6.15's prefiltered environment maps want the same number):
            sin t = alpha * sqrt((sqrt2 - 1) / (1 - alpha^2))
        Exact at every roughness, checked against a bisection of the real NDF to
        1.6e-8 across 0.02..0.90. THE COEFFICIENT IS sqrt(sqrt2 - 1) = 0.6436, so
        the folklore "the lobe is about alpha wide" OVERSTATES IT BY 55%.
        THE CROSSOVER IS COMPUTABLE: compare the lobe to the normal's variation
        across a pixel ((pi/2)/R on a closed surface R pixels in radius). On a
        40 px sphere it is ROUGHNESS 0.2468 — the demo's 0.49 safely above, and
        every polished metal 6.12 introduced (0.20, 0.10, 0.05) below.
        `aa_settings::factor` DEFAULTS TO 1 and `specular` to false, the sixth
        time this course has made that call. All three techniques are STRUCTURALLY
        outside the fixture: supersampling is a resolve between two `framebuffer`s
        it does not allocate, specular AA is a function producing a ROUGHNESS that
        the CALLER applies (so `shade()` and `fill_style` never change — both take
        `microsurface` BY VALUE, which is what made this free), and MSAA is a GPU
        property in a CPU-only fixture. DECIDED BEFORE ANY CODE, as 6.12 and 6.13
        both were, and this time the reason was checked rather than assumed
        because STATE warned the usual argument might not transfer.
  antialias-thesis: LESSON 6.14, AND IT IS THE RESULT THAT REFRAMES THE SUBJECT.
        Filtering the NDF improves per-pixel ACCURACY against brute-force ground
        truth by only 2.4x at roughness 0.05 — and at 0.30 and 0.20 it makes the
        error THREE TIMES WORSE — while improving STABILITY (the swing as the
        camera pans by under one pixel) from 3996.5x to 2.0x: A FACTOR OF 2034.
        ANTIALIASING DOES NOT MAKE A PIXEL CORRECT, IT MAKES IT STABLE. Obvious in
        hindsight: the artefact was never "this pixel has the wrong value" — a
        still frame with a slightly wrong specular pixel looks fine and nobody
        files a bug — it was "this pixel changes violently when nothing in the
        scene did". Aliasing is not an error in MAGNITUDE, it is an error
        DISCONTINUOUS IN THE PARAMETERS, which converts smooth camera motion into
        flashing.
        SO THE QUESTION TO ASK OF AN AA TECHNIQUE IS NEVER "how accurate is it"
        but "WHAT FREQUENCIES DOES IT REMOVE, AND CAN THE GRID CARRY WHAT IS
        LEFT". Both halves of 6.14 answer it identically: supersampling prefilters
        coverage by integrating over the pixel's area, NDF filtering prefilters
        the lighting by widening the lobe. Both discard real information on
        purpose. AND IT IS 6.13's TRADE IN ANOTHER CURRENCY: that lesson had more
        RANGE than the display could carry and spent AREA; this one has more
        DETAIL than the grid can carry and spends SHARPNESS.
        WHAT IS DERIVED AND WHAT IS FITTED, kept separate: the convolution
        argument (a pixel integrates over a footprint; convolving distributions
        ADDS VARIANCES, so alpha'^2 = alpha^2 + 2 sigma^2) is real. The factor of
        2, sigma^2 = 1/(2pi) and kappa = 0.18 are Tokuyoshi & Kaplanyan's FITS.
        And "alpha behaves as a standard deviation" is an approximation doing real
        work, because GGX'S VARIANCE IS INFINITE — the same heavy tail 6.13
        exploited to make bloom look like glare. Marked, then measured.
        `dot(d, d)` AND NOT `length(d)`: the quantity that adds is the VARIANCE.
  msaa: LESSON 6.14, AND EVERY FIELD WAS CHECKED AGAINST SDL_gpu.h RATHER THAN
        ASSUMED. A MULTISAMPLE TEXTURE CANNOT BE SAMPLED AT ALL, so an MSAA frame
        needs TWO colour targets where a plain one needs one:
        `gpu_post_stack::scene_target()` is what the scene pass renders into and
        `resolved_target()` is what everything downstream reads. Getting them
        backwards is a validation error on debug and undefined on release, which
        is why `create_colour_target` SILENTLY DROPS the SAMPLER usage above 1x.
        SUPPORT IS PER FORMAT AND IS ASKED: `SDL_GPUTextureSupportsSampleCount`.
        On the author's machine 2x and 4x work on both R16G16B16A16_FLOAT and
        R8G8B8A8_UNORM and 8x WORKS ON NEITHER. An unsupported request falls back
        to 1x with a warning rather than failing — refusing to start because 8x is
        unavailable is worse behaviour than running at 4x and saying so.
        STOREOP_RESOLVE, NOT RESOLVE_AND_STORE: the header says the first lets the
        driver DISCARD the multisample memory and is "the most performant method",
        and nothing here reads per-sample data. `scene_target_info()` exists so a
        caller cannot assemble that struct and forget `resolve_texture` — the
        failure is a frame that renders perfectly into a texture nobody reads.
        EVERY ATTACHMENT AND EVERY PIPELINE SHARES ONE SAMPLE COUNT. A 4x colour
        target beside a 1x depth target is a pass that CANNOT BE BEGUN, and the
        count is now part of the depth target's IDENTITY in `ensure_depth` —
        without that, toggling MSAA reuses a same-sized 1x buffer and the pass
        fails several frames after the setting changed.
        `SDL_GPUMultisampleState`'s other two fields are LEFT ALONE: the header
        says `sample_mask` is "Reserved for future use. Must be set to 0" and
        `enable_mask` must be false.
        THE PIPELINE COUNT GOES 13 -> 22, because MSAA does not ADD a pipeline, it
        DOUBLES the scene set — sample count is another axis of baked-in state.
        6.13 said the enumerate-versus-hash argument would stop being a curiosity;
        this is where it stops.
        AND 6.13's OWNERSHIP RULE SURVIVED ITS FIRST REAL TEST WITHOUT AMENDMENT:
        the RESOLVED target crosses between stages so the stack owns it; the
        multisample target is an intermediate of the scene pass that nothing
        downstream sees. Only the allocation changed. A design tested by a case it
        was not written for, and it held.
  bloom: LESSON 6.13. THE THRESHOLD IS IN EXPOSURE-CORRECTED LIGHT AND THE
        COMPOSITE IS BEFORE THE CURVE — both are physics rather than taste, and
        both are measured (324 vs 32 clipped pixels; 4,064 of 4,096 changed by an
        exposure mismatch). `bloom_settings::enabled` DEFAULTS TO FALSE, the
        fourth time this course has made that call (6.7's texel_space, 6.10's
        opt-in chain, 6.11's mip_options, this): a new capability is a new path,
        so every prior measurement and the golden survive it.
        `intensity` IS A TUNED SCALAR, NOT A PERCENTAGE, and the doc comment says
        so because the alternative is a reader discovering that 0.04 is not four
        percent. The upsample adds every level at full weight, so a uniform field
        comes out at exactly `levels x bright` — 4.0, 8.0, 12.0 measured at 2, 4
        and 6 levels, which is also the harness's analytic check.
        THE PYRAMID IS ALLOCATED EVEN WHEN THE BLOOM IS OFF, deliberately:
        toggling mid-frame would otherwise stall on six texture creations, which
        is exactly when a user is A/B-ing the effect. 1.32 MB at 960x540.
        LOADOP_LOAD ON THE UPSAMPLE PASSES AND ONLY THOSE. Additive blending makes
        the destination an OPERAND; DONT_CARE there turns the pyramid from a sum
        over every level into just the widest one, and the symptom is a bloom
        that is too soft and too dim — a tuning symptom for an addressing bug.
  shader-deploy: LESSON 6.13, AND IT IS A BUILD BUG THAT HID FOR A WHOLE LESSON.
        `engine_use_shaders()` copied compiled shaders beside each executable with
        `add_custom_command(TARGET x POST_BUILD ...)`. A POST_BUILD COMMAND RUNS
        ONLY WHEN ITS TARGET IS REBUILT — so editing ONLY a shader recompiles the
        shader target, gives no demo any reason to relink, and the copy NEVER
        FIRES. The build reports success, the new HLSL is genuinely compiled and
        sitting in build/shaders/, and every program keeps the shader it was last
        linked beside. 6.13's harness measured a bloom composite the deployed
        shader did not contain, and the only visible symptom was that the GPU
        answer was constant across a parameter sweep.
        FIXED with the standard CMake shape: a command whose OUTPUT is a stamp
        file and whose DEPENDS are the compiled shader FILES (recorded in a new
        global property ENGINE_SHADER_OUTPUTS), wrapped in a target the executable
        depends on — so the copy runs BEFORE the executable is considered built
        rather than after. The `add_dependencies` line that was already there only
        ever ordered the COMPILE, never the DEPLOY.
        THE DIAGNOSTIC THAT FOUND IT: build/demos/shaders/tonemap.frag.msl was a
        DIFFERENT SIZE from build/shaders/tonemap.frag.msl. When a GPU result
        disagrees with everything else, check that the binary on disk is the one
        you think you compiled.
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
```
