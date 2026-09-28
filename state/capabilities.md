# Capabilities — what the engine can do, lesson by lesson

Moved verbatim from STATE.md's `capabilities:` block on 2026-09-27, when that file became
a compact resume key (CLAUDE.md §9). Append here; keep STATE.md's headline in step.

```text
capabilities:
  - 8.13 THE ENGINE HAS A CHARACTER CONTROLLER, AND A SHAPE CAST ANY GAMEPLAY
    CODE CAN USE. MODULE 8 IS COMPLETE.
    phys/cast.hpp (new): cast_status {clear, hit, started_inside,
    iteration_limit} + name_of; cast_config {skin, tolerance, min_approach,
    max_iterations, gjk}; cast_result {status, t, distance (the lower
    bound), surface_normal, point, iterations, gjk_iterations, hit()}; cast.
    phys/character.hpp (new): k_no_body; character_config (radius,
    half_height, skin, max_slope, step_height, snap_distance, max_slides,
    push_mass_limit, clip_rule, flatten_walls, lay_along_ground,
    stop_on_ground, carry_rule, cast); character {position, velocity,
    grounded, ground_normal, ground_point, ground_body}; character_world;
    sweep_filter {everything, solid}; character_hit; move_report; and
    character_capsule, walkable, pushable, sweep, slide_along,
    clip_to_planes, wall_normal, along_ground, recover, body_step,
    step_motion, carry_by_transform, carry_by_velocity, move_character,
    steer_proxy; closed forms free_curb_height, edge_overhang,
    step_forward_min, slope_creep_speed, skip_speed(g, h, a, reach),
    velocity_carry_growth, jump_speed, stopping_distance.
    phys/convex.hpp: placed_shape (by value, view() const& with the && overload
    deleted) and place(shape, centre, orientation).
    WHAT IT CANNOT DO: be pushed by anything (one-way coupling; knockback is
    ex. 4), put its weight on what it stands on (ex. 4), cull candidates
    faster than linearly (ex. 5), turn its capsule (the cast is translation
    only), or keep its feet within 0.1 mm on very large collision pieces (the
    certificate's looseness scales with the obstacle, §6).
  - 8.12 THE ENGINE HAS RAGDOLLS, AND HANDS A CHARACTER TO THE SOLVER AND BACK.
    phys/ragdoll.hpp (new): ragdoll_link {root, hinge, cone_twist};
    ragdoll_part_desc; ragdoll_exclusion {jointed, overlapping, two_links};
    ragdoll_desc; ragdoll_part; ragdoll_mode; ragdoll; ragdoll_report;
    build_ragdoll, part_targets, spawn, steering_angular_velocity, steer,
    simulate, animate, add_joints, exclude_pairs, read_pose, realign_model,
    worst_joint_error.
    phys/constraint.hpp: swing_twist + split_swing_twist, swing_angle,
    joint_cone, make_cone_twist, joint::cone / swing_impulse, row_role::swing,
    joint_batch::swing, solve_row_position_to; ball-socket swing and twist
    rows; the position-pass FIX.
    phys/solver.hpp: wake_touched_by_kinematic (the kinematic-wake FIX),
    solver_stats::kinematic_wakes.
    WHAT IT CANNOT DO: get a character up convincingly (no get-up clip;
    ex. 4 heading), put most settled ragdolls to sleep (thresholds tuned on
    crates; ex. 3), hold a chest off an arm at 8 sweeps (61 mm), elliptical
    cones (ex. 2), or active/powered ragdolls (ex. 5).
  - 8.11 THE ENGINE HAS JOINTS, IN THE SAME LOOP AS CONTACTS.
    phys/constraint.hpp (new): `jacobian_row` + point_row / angular_row /
    prepare_row / row_velocity / solve_row / solve_row_position; `joint_kind`
    {distance, ball_socket, hinge}; `joint_limit`, `joint_motor`, `joint`
    with make_ball_socket / make_hinge / make_rod / make_rope; `hinge_angle`;
    `joint_error` + `measure_joint`; `joint_config` (block_solve,
    speculative_limits, warm_start, baumgarte, max_correction_speed);
    `joint_batch` (3x3 point block, 2x2 hinge block, up to 8 scalar rows);
    prepare_joint / warm_start_joint / solve_joint / solve_joint_positions /
    write_back(joint_batch, joint&); `collision_filter`; and the closed forms
    pendulum_period, pendulum_period_at (by the AGM), constraint_drift_per_step,
    lever_ratio, projection_loss_per_step, motor_spin_up_time.
    `pseudo_velocity` moved here from solver.hpp.
    phys/solver.hpp: `constraint_solver` (alias `contact_solver`) with
    add(a, b, joint&) and joint_batches(); `joint_pair`; island
    first_joint/joint_count; build_islands(bodies, contacts, joints, out);
    solver_config::joints; solver_stats joints / solved_joints /
    joint_residual / joint_linear_error / joint_angular_error.
    A door hangs within 0.0001 deg of true on its hinge; a 1 m box swings at
    1.645967 s against 1.646241; a twelve-plank bridge carries crates.
    WHAT IT CANNOT DO: hold a heavy weight on a light chain (388 mm of stretch
    at 100:1 and eight sweeps), stop a door dead at its limit at eight sweeps
    (7.5% rebound, rho = 3/4 per sweep), or keep a fast swing's energy under
    split impulse (rho (w h)^2 per step). No swing or twist limits: 8.12
    added them (and fixed this lesson's position pass — see 8.12).
  - 8.10 THE ENGINE CAN HOLD A STACK UP, PARTITION A LEVEL, AND STOP SIMULATING
    THE PARTS OF IT THAT ARE NOT DOING ANYTHING.
    phys/solver.hpp gains: `position_correction` (none / baumgarte /
    split_impulse, default the third) + `pseudo_velocity` + `prepare_bias` +
    `solve_positions` + `apply_pseudo_velocity` + `contact_pair` + `island` +
    `build_islands` + `sleep_config` + `wake_islands` + `update_sleep` +
    `solver_stats` + `contact_solver` (begin/add/solve/islands/island_bodies/
    island_of/stats/clear) + `arrival_depth` + `baumgarte_time_constant`.
    `solver_config` gains velocity_iterations (8), position_iterations (3),
    correction, baumgarte (0.2), penetration_slop (5 mm),
    max_correction_speed (3 m/s), and warm_start FLIPPED TO TRUE.
    `contact_constraint` gains separation, bias, pseudo_impulse.
    phys/rigid_body.hpp gains: sleeping / sleep_timer / allow_sleep on the body,
    `wake`, `is_sleep_candidate`, step_report::sleeping, and the two halves of
    `step`.
    phys/manifold.hpp gains ONE FIELD: `contact_manifold::tangent[2]`, the basis
    the cached friction impulses were measured in. See `solver:` in conventions.
    A five-crate tower settles at the slop and sleeps in 2.8 s; a yard of twenty
    costs 3 us to solve instead of 131 once it has; 2,001 bodies fit in 4.35 ms
    of a 16.67 ms frame awake and 1.32 asleep.
    WHAT IT CANNOT DO: hold a TEN-crate tower at the shipped eight iterations
    (it leans and falls; twenty sweeps hold it, fifteen crates need more than
    64), or converge quickly at a large mass ratio (1000:1 squashes the light
    crate 283x further at the same count). Both are the same limitation — a
    Gauss-Seidel sweep carries information across ONE contact — and both want a
    Jacobian rather than a bigger number. 8.11.
  - 8.9 THE ENGINE CAN RESOLVE A CONTACT, which is the first thing in phys/
    that changes the world rather than describing it. A crate dropped on a floor
    lands on it; a ball bounces to e^(2n)*h0 and stops; a slab holds a slope up
    to atan(mu) to 0.43 deg and slides past it; a sphere that lands sliding
    rolls away at 5/7 of the speed it arrived with, whatever mu is. Coulomb
    friction with an isotropic cone clip, restitution with the g*h contamination
    removed exactly, and a prepared-constraint shape that hoists the per-body
    basis change out of the inner loop at 139 ns per manifold.
    WHAT IT CANNOT DO: hold a crate up with ONE pass (it sinks 275 mm in 15 s),
    or repair an overlap that already exists (the converged solve freezes it at
    the arrival depth). 8.10.
  - 8.8 THE ENGINE CAN FIND CANDIDATE PAIRS IN A SCENE OF THOUSANDS OF BODIES
    WITHOUT TESTING EVERY PAIR, and can prove it has not missed any.
    phys/broadphase.hpp: `proxy` (aabb + uint32 index — six floats and a number,
    and NOTHING ELSE, because the stage whose value is being cheap must not be
    able to look inside anything) + `broadphase_pair` (a < b ALWAYS, which is
    what lets 8.7's pair_key index the manifold_cache) + `broadphase_config`
    (cell_size, margin, max_cells_per_proxy) + `broadphase_stats` (proxies,
    cell_size, entries, buckets, occupied, largest_bucket, colliding_buckets,
    bucket_tests, owner_rejects, cell_rejects, box_rejects, pairs, oversized,
    oversized_pairs) + `brute_force_pairs` (oracle AND baseline AND the right
    answer below ~100) + `auto_cell_size` (1.5x the mean longest side) +
    `uniform_grid` (build/pairs/stats/clear + static cell_of and hash_cell,
    both INLINE in the header — §7 measured 6.0x for the call) + `owner_cell`.
    phys/collide.hpp GAINS: `overlaps(aabb, aabb)` — six comparisons, inline,
    the SAT with twelve of its fifteen axes deleted by symmetry.
    NOT a shape, not a body, not a pointer to either, and no dependency on
    anything in phys/ but `collide.hpp` for those six comparisons. It is the
    first file in phys/ that depends on no shape at all.
  - 8.5 THE ENGINE CAN MEASURE THE DISTANCE BETWEEN ANY TWO CONVEX SHAPES and
    hand back the two surface points that realise it. Box, sphere, capsule,
    arbitrary point set, in any of the ten pairings, through one function that
    names none of them.
    phys/convex.hpp: `convex` (support_fn + const void* data + vec3 origin, 24
    bytes) + world_support + as_convex() for obb/sphere/capsule/hull + FOUR
    DELETED RVALUE OVERLOADS. `support` returns a point RELATIVE TO `origin`,
    which is the whole numerical argument of the file: see §12's 96,270x.
    phys/gjk.hpp: gjk_vertex (w + pa + pb, the two halves carried so the witness
    points come out of the same barycentric weights) + `simplex` (4 vertices,
    Caratheodory) + gjk_status{separated, intersecting, iteration_limit} +
    gjk_result (+ `stalled`, + the TERMINAL SIMPLEX for 8.6) + gjk_config
    (tolerance RELATIVE, max_iterations, initial_direction) + gjk_distance +
    gjk_intersects + gjk_bounds/certify + cso_support + reduce_simplex (the last
    two public because the demo single-steps the loop and §C measures the solver
    directly).
    phys/shape.hpp GAINS: shape_kind::capsule + `shape::half_height` (24 bytes
    now) + capsule_shape + `capsule` (world placement) + `hull` (world placement
    + std::span of BODY-AXES points) + world_capsule + world_hull +
    support/support_local for all four + bounds_of for capsule and hull.
    phys/collide.hpp GAINS: axis_source::witness (index -3) + collide(convex,
    convex) — the facade, returning a `separation` so a caller can swap the
    specialised test for the general one without touching anything downstream.
  - 8.4 THE ENGINE KNOWS HOW BIG THINGS ARE, and can answer "are these two
    touching, and if so how do I fix it?" for every pair of {sphere, aabb, obb}.
    phys/shape.hpp: shape_kind{sphere,box} + `shape` (20 bytes, BODY axes,
    CENTRED on the centre of mass) + sphere_shape/box_shape/cube_shape +
    volume_of + bounding_radius (the one measurement a rotation cannot change,
    which is what a broadphase wants) + inertia_of(shape, mass) — the ONE arrow
    into inertia.hpp, and it runs one way + `obb` (centre, mat3 axes as COLUMNS,
    half extents, axis(i)/half(i)/corners()) + world_obb/world_sphere +
    bounds_of(obb) and bounds_of(shape, centre, quat) + as_obb + support() for
    box and sphere — added a lesson early because 8.5's GJK is written entirely
    in terms of it.
    phys/collide.hpp: `separation` (unit axis FROM a TOWARD b, signed depth,
    axis_index, axes_tested, hit() DERIVED from the sign) + axis_source +
    source_of + flip + `interval` + interval_overlap + projected_radius +
    project + gap_on_axis + k_parallel_sin2 + collide() for sphere/sphere,
    aabb/aabb, aabb/sphere, obb/sphere, obb/obb (both argument orders where they
    differ) + overlaps() (the absR formulation, boolean only, no square roots) +
    closest_point and distance_squared_to for aabb and obb.
    THE TWO FORMULATIONS ARE A DELIBERATE PAIR, not duplication: collide()
    normalises because the DEPTH must be comparable across candidates, overlaps()
    does not because the boolean is scale-invariant. Measured 79.258 ns vs
    10.458 ns on crowded pairs — 7.58x, same answers on 200,000 pairs, and the
    ratio is entirely square roots. A broadphase confirmation pass wants
    overlaps(); a contact generator wants collide().
    STILL MISSING, all of it named on the page: a DISTANCE (a gap is on one axis
    and is a lower bound — 8.5's GJK), CONTACT POINTS (8.7), anything better than
    quadratic (8.6), and any awareness of MOTION (8.6's continuous half).
  - 8.3 THE ENGINE CAN TURN. A `rigid_body` carries an `orientation` (unit
    quat), an `angular_velocity` (WORLD space, rad/s), a `torque` accumulator
    (N.m about the CENTRE OF MASS, world axes), an `inv_inertia_local` AND an
    `inertia_local` (both BODY axes, about the centre of mass — the redundancy
    is §12's, measured), an `angular_damping` and a `gyroscopic_mode`.
    WHAT IS NEW, in phys/inertia.hpp: point_mass + inertia_of_point (the
    WELL-CONDITIONED form, see below) + inertia_of_points + inertia_solid_box /
    _solid_sphere / _hollow_sphere / _solid_cylinder / _thin_rod / _capsule +
    shift_inertia / unshift_inertia (parallel axis, BOTH directions) +
    rotate_inertia + world_inertia + world_inverse_inertia + inverse_inertia
    (RELATIVE singularity test, not det == 0) + inertia_part /
    inertia_assembly_result / inertia_assembly + inertia_report /
    inspect_inertia (asymmetry, trace, diagonal, off_diagonal, positive,
    TRIANGLE INEQUALITY, usable).
    ...in phys/integrate.hpp: spin_rule (linearised | exponential) +
    advance_orientation + k_spin_epsilon + spin_inflation + spin_angle_error.
    ...in phys/rigid_body.hpp: gyroscopic_mode (off | explicit_term |
    implicit_term | momentum) + add_torque + add_force_at + add_impulse_at +
    add_angular_impulse + clear_torque + set_inertia + inertia_of +
    world_inv_inertia + gyroscopic_step + angular_momentum + kinetic_energy +
    point_velocity + world_point_of + make_box + make_sphere; step_report gains
    angular_momentum, max_spin and max_unit_error; body_world gains
    set_spin_rule / spin().
    ...in math/mat3.hpp: operator+ / - (binary and unary) / *(scalar, both
    orders) / += / -= / *= + outer + skew + trace + diagonal + asymmetry.
    THE DECISIONS, each with a number behind it:
      1. THE TENSOR IS IN BODY AXES AND EVERYTHING ELSE IS IN WORLD AXES, and
         that asymmetry is FORCED: in world space a tensor changes the instant
         the body turns. world_inv_inertia is the bridge, and it is a SANDWICH
         (R I R^T) because a quantity that eats a vector and produces one must
         be converted on BOTH sides.
      2. R^T I R IS THE PLAUSIBLE WRONG ONE. Symmetric, same trace, same
         determinant, same principal moments, `usable` — and out by 1.569413.
         A test built on a 90-degree turn passes with the transpose in EITHER
         place, because a right angle is its own inverse on a diagonal tensor.
         Convention bugs hide behind symmetric test data.
      3. STORE BOTH TENSORS. 36 bytes of redundancy with an invariant nothing
         can enforce (set_inertia is the only writer), bought because the
         forward tensor is needed by the gyroscopic term AND by
         step_report::angular_momentum — which is computed whether or not
         anybody reads it, and was 40% of the whole step at 13.418 ns.
      4. THE ORIENTATION UPDATE IS SEMI-IMPLICIT ALWAYS, whatever `rule_` says.
         Velocity Verlet's second half would need the tensor rebuilt at the new
         orientation and a second renormalisation, to correct by less than the
         renormalisation's own error.
      5. GYROSCOPIC DEFAULTS TO `off`, which is PhysX's default (Bullet's, since 2.83, is the implicit form;
         flag spellings verified against both headers 2026-09-26). So out of the box this engine's bodies
         do NOT tumble, and the most interesting thing in the lesson is
         something you have to ask for. Said out loud in §13.
    THE INSTRUMENT THAT MEASURES SOMEBODY ELSE: step_report::max_unit_error
    reads | |q| - 1 | as the step FINDS each body, before touching anything.
    Since step renormalises on the way out, anything it finds was written from
    outside — an animation blend, a lerp, a network packet, a hand-authored
    quat — all of which render as a subtle shear that is impossible to grep for.
    NO NEW LOG CATEGORY, and that is the SECOND test of 7.8's prediction: phys/
    still has one thing to say (a caller passed a tensor no shape could have)
    and it is a programmer error. set_inertia's refusal goes to log_core beside
    set_mass's. Six categories, unchanged since 7.8.

  - 8.2 THE ENGINE HAS BODIES, and can be told what is pushing on what.
    `engine::phys::body_world` is a `pool<rigid_body>` and about eight lines of
    policy: gravity, a choice of integrator, and a walk that turns a force
    accumulator into a step. A `rigid_body` HAS 8.1's `motion` rather than being
    one — which is what lets §3's harness step a bare `motion` beside a body and
    compare them bit for bit — plus a force accumulator, an inverse mass, a
    velocity-space damping coefficient, a gravity_scale and a body_kind.
    WHAT IS NEW: rigid_body + body_kind (dynamic/kinematic/FIXED — `static` is a
    keyword and cannot be an enumerator, so the doc comment carries the other
    word for searchers) + body_id + mass_of/set_mass + add_force/add_impulse/
    add_acceleration/clear_force + terminal_speed_damped/terminal_speed_dragged
    + free_fall_time/time_scale_for_length_scale + frame_report/inspect_frame +
    place_in_parent + step_report + body_world + make_dynamic/make_fixed/
    make_kinematic + k_gravity/k_gravity_down.
    THE THREE DECISIONS, each with a number behind it:
      1. AN ACCUMULATOR, NOT A SETTER, because F = ma is linear in F. Four
         systems then need no interface, no registry and no visitor. The failure
         mode is silent: four `=` instead of four `+=` and gravity, wind and drag
         are simply gone.
      2. INVERSE MASS, and the divide is the LEAST of the three reasons.
         Immovable is the common case in a real level and is an exact zero;
         mass_of(floor) - mass_of(wall) is NaN where inv_a - inv_b is 0; and
         8.9's effective mass is LITERALLY 1/(w_a + w_b), so the storage was
         chosen to match a hot loop that does not exist yet.
      3. AN IMPULSE HAS NO h IN IT. Same 70 kg jump: as a one-step force it is
         3.8528 m at 30 Hz and 0.1672 m at 144 — 23.0x — and as an impulse it
         spans 6.16%, whose residue is 8.1's v0*h/2 and is predicted to four
         decimals. Every contact response from 8.9 is an impulse for this reason.
    THE NUMBERS, MEASURED, NINE SECTIONS AND THIRTEEN CONTROLS:
      a is proportional to 1/m      m*a = 50.0000 N at 1, 10 and 1000 kg
      forces, 24 orderings, spread  3.7000e-03 = THE SMALLEST OF THE FOUR
      CONTROL, exact powers of two  0.0000e+00 across all 24
      (g/inv_mass)*inv_mass sweep   159,937 of 1,000,001 inexact = 15.9937%
      ...worst, and the first bad m 1.00 ulp; 1.000145 kg
      force route, 1 vs 1.000145 kg 1 ulp at step 1 and 60, ZERO at 600
      acceleration route, same pair  0 at every step count
      damping terminal, 3 masses    19.5383 m/s, all of them
      ...g/k says 19.6200; DISCRETE  19.5384  (g h e^-kh / (1 - e^-kh))
      drag-force terminal, 3 masses 1.9607 / 19.6026 / 1960.1298 m/s
      ...and their time constants   m/b = 0.2 s, 2 s, 200 s
      jump as impulse, 4 rates      0.9275 .. 0.9846 m   (6.16%)
      jump as one-step force        0.1672 .. 3.8528 m   (23.0x)
      1e6 kg DYNAMIC vs 10 g pebble -4.986749 m BOTH
      fixed body, 10 s + 10 kN      exactly 0.0
      fall time ratio vs 1/sqrt(s)  2.0000 1.4142 1.0000 0.7071 0.5000
      g scaled by 1/s, simulated    36 steps at EVERY scale
      uniform parent scale 2        gain 2.0000, and the fall ratio is 2.0000
      scale(2,1,1) with gravity //y gain 1.0000 tilt 0.000 — THE TRAP
      ...same frame, body turned 45 gain 1.5811, tilt 63.435 deg, square 0.6000
      spinning parent, no forces    3.7963 m off a straight line after 1 s
      body_world::step              2.706 ns/body (damped 4.222, fixed 0.712)
    THE FRAME RULE IS ABSOLUTE AND THE ABSENCE IS THE DESIGN: a rigid_body's
    position and velocity are WORLD SPACE and body_world has no way to express
    anything else — no parent, no hierarchy, no transform. `place_in_parent` is
    the only bridge and it converts rather than integrating; `inspect_frame` is
    called by NOTHING and exists so the rule is a measurable claim.
    `frame_report::out_of_square` is 7.5's number, built to report what a quat
    could not hold and uncalled for three lessons — it is the exact diagnostic a
    physics frame needs, because both questions are "does this preserve angles".
    THE DEMO IS demos/bodies, and it is built around the one claim a picture
    settles faster than a table: MASS IS INVISIBLE UNTIL SOMETHING RESISTS.
    Three masses 100x apart, identical launch, three resistance modes — and two
    of the three pictures are identical.
  - 8.1 THE ENGINE CAN ADVANCE A STATE THROUGH TIME, and can say whether the step
    size you chose is one that works. `engine::phys` is one header and one source
    file: three rules over a `motion` of two vec3s (explicit Euler, semi-implicit
    Euler, velocity Verlet), exact linear drag, a frame-rate-independent damping
    factor, and four diagnostics — spring_energy, shadow_energy, area_factor and
    max_stable_step.
    THE STEPPER IS A TEMPLATE over the acceleration callable, and the measurement
    is the argument: std::function costs 1.591 ns against 0.963, +65% on top of
    the step itself. The non-template constant-acceleration overload — the gravity
    path, the one that looks simpler — is 1.222 ns, SLOWER than the template,
    because it is a real call into libengine.a that cannot inline across the
    archive.
    WHAT IS MISSING, and 8.1 §11 lists it rather than implying it: no mass and no
    forces (8.2), no rotation (8.3, and it needs 7.4's quat), no collision and no
    constraints (8.4-8.10), no adaptive step EVER (1.4's argument: a variable step
    makes replays and lockstep impossible), no RK4 (four force evaluations, and
    its determinant is 1 - u^3/72 — backward Euler's disease in fifth-order
    clothing), and no velocity-dependent force in the symplectic path.
    THE DEMO IS demos/integrate, and it draws PHASE SPACE rather than a scene,
    because all three rules look like a spring for the first few seconds. It runs
    1.4's `fixed_step` NESTED INSIDE the application's own — the app's rate is a
    platform decision and the simulation's rate is the variable under study — and
    it carries a 20-line Cohen-Sutherland clip, because engine::draw_line
    deliberately does not clip (2.1 says so in a comment) and a diverging red
    spiral would otherwise be drawn across the energy chart next door. That
    exercise from 2.1 came due seventy lessons later for a reason nobody
    predicted.
  - 7.8 THE ENGINE CAN HEAR. `engine::audio` is three headers and about a
    thousand lines: a WAV loader with format conversion and linear resampling, a
    float mixer with a voice table behind generational handles, and a spatial
    model that turns a listener and an emitter into two gains.
    THE ASSET / PLAYHEAD SPLIT FOR THE THIRD TIME, and the third instance is what
    makes it feel inevitable rather than clever: a skeleton is shared and a pose
    is not (7.6), a clip is shared and its cursor is not (7.7), a sound is shared
    and its playback position is not. Here it buys something the other two did
    not — the voice table is the ONLY shared mutable state in the subsystem, so
    the only thing a mutex has to guard.
    WHAT IS NEW: audio::sound + wav_report + load_wav + make_tone + make_noise +
    apply_fade + measure; audio::listener + listener_from + falloff (4 laws) +
    emitter + stereo_gain + spatial_result + attenuation + pan_of +
    pan_constant_power + pan_linear + spatialise + amplitude_to_db +
    db_to_amplitude; audio::mixer + mixer_config + voice_params + mixer_report +
    voice_id + open/open_offline/close/play/play_spatial/set_gain/set_gain_pan/
    set_pitch/playing/stop/stop_sound/stop_all/update/report/reset_report/
    mix_into. Plus `log_audio`, the sixth log category since 5.3 and the first
    added since, with the argument for it written into log.hpp.
    THE NUMBERS, MEASURED, NINE SECTIONS AND TWELVE CONTROLS:
      rms of N uncorrelated voices   0.577*sqrt(N)  exactly, N = 1..64
      peak of the same mix           7.68x at N=16  (sqrt(N) says 4, N says 16)
      peak vs observation window     6.98x @ 10 ms -> 10.46x @ 8 s
      CONTROL, N copies of one voice peak = N exactly
      linear pan law, worst dip      -3.01 dB at centre (constant power: 0.000)
      1/d vs 1/d^2 per doubling      -6.02 dB vs -12.04 dB
      bare inverse at max_distance   -33.98 dB, then cut: a step, i.e. a click
      gain as a step vs the waveform 0.100745 vs 0.013088   7.70x
      gain as a ramp vs the waveform 0.013072               1.00x
      of the injected error, at f0   1.2%   (the other 98.8% is the click)
      a sound starting on a peak     0.2500 step, 38.2x; 5 ms fade -> 1.00x
      linear resampler 44.1 -> 48    62.40 dB SNR (SDL 85.05, no resample 91.67)
      ...and by frequency            89.00 dB @ 200 Hz, 20.43 dB @ 10 kHz
      64 voices, 512-frame buffer    29.761 us of 10,667 us   0.28%
      per voice per frame            0.91 ns
      set_gain (lock + 2 stores)     9.0 ns
      one log line, format only      82.0 ns; + write + flush 1,021.6 ns
    THE OFFLINE PATH IS THE DESIGN, NOT A TEST HOOK. `mix_into` is public and
    `open_offline` builds the voice table with no device, so the real-time path
    is a pure function from a table to an array of floats. Every number above
    was produced on a machine that was never asked whether it had speakers, and
    demos/audio's `--shot` uses the same entry point, so a screenshot is silent
    and deterministic.
    AND THAT LEFT EXACTLY ONE THING UNTESTED, WHICH FOUND A BUG.
    scratch/devcheck_78.cpp opens a real device for 700 ms. Its first run
    reported 28 of 58 buffers "starved" on a mix at 0.2% load: the counter
    incremented when SDL_GetAudioStreamQueued() was 0 at callback entry, which in
    SDL3's PULL model is the steady state rather than a failure. Renamed to
    `queue_empty`; the question it was meant to answer is now `late`, which
    compares the mixer's own time against its own deadline and needs nothing from
    the driver. A healthy run prints `late 0` — a check whose degenerate case is
    a FAILURE.
    WHAT IS STILL NOT: NO STREAMING (a 4-minute track is 44 MB resident; Module
    9). NO COMPRESSED FORMATS (WAV only; Ogg/Opus are the stb_image argument
    again). NO EFFECTS, no filters, no buses, no reverb. NO VOICE STEALING — a
    play past max_voices is refused and counted, and the 65th voice is 0.6 dB.
    NO FRONT/BACK and no elevation: two speakers cannot express it, and HRTFs are
    where the next ten percent begins. NO DOPPLER (set_pitch exists; it is an
    exercise). NO LOCK-FREE COMMAND QUEUE — one mutex on the audio thread, with
    the priority-inversion compromise stated in mixer.cpp rather than hidden, and
    Module 9's job system named as where it goes.
    ONE UNDEFINED BEHAVIOUR, DELIBERATELY: a voice holds a non-owning
    `const sound*`, so a sound destroyed while a voice reads it is UB rather than
    a counter. `stop_sound()` before releasing one. It is a raw pointer on
    purpose — a shared_ptr would put a refcount decrement, and therefore possibly
    a free, on the audio thread.

  - 7.7 THE ENGINE CAN PLAY RECORDED MOTION. `engine::anim::clip` is a function
    from time to pose: per-channel keyframe tracks, cursor-based sampling with a
    binary-search fallback on any jump, correct looping, cross-fading in POSE
    space, load-time sign canonicalisation and greedy keyframe reduction.
    THE SEAM 7.6 DREW HELD. `build_pose` in demos/rig went from two sliders to
    two sample() calls and a blend_poses(), and NOTHING between it and the screen
    changed — because everything downstream takes std::span<const transform>.
    What the seam could NOT hide is [M], the matrix-blend path, and it was never
    going to: that is a different operation on a different type, and a span
    cannot conceal a change in what is being averaged. An abstraction that holds
    is one that is honest about its edges.
    WHAT IS MISSING, and 7.7 §10 lists it rather than implying it: nothing LOADS
    a clip (the demo and the harness both bake); root motion is measured and not
    applied; there is no STEP and no CUBICSPLINE; no additive layer (Exercise 4
    derives it — it is quat_pow_unit, which 7.5 already wrote); no blend tree or
    state machine (gameplay, and the right side of the boundary); no component
    quantisation.
  - 7.6 THE ENGINE CAN DEFORM A SURFACE, which is categorically not what 5.9's
    hierarchy does: that PLACES objects, and this moves parts of ONE continuous
    mesh by different amounts. `engine::anim` holds a skeleton (joints,
    parent-before-child, bind pose), the inverse binds, a pose -> palette
    pipeline, and linear blend skinning for points and directions.
    WHAT IS MISSING IS THE POSE. Everything takes
    std::span<const transform> and does not care where the transforms came
    from; in 7.6 they came from two sliders. 7.7 replaces the sliders with a
    clip, and that is the whole of what is left to make a character animate.
  - 7.5 THE ENGINE CAN BLEND TWO ORIENTATIONS, AND IT STORES THEM AS FOUR
    FLOATS. That second clause is the one Lessons 7.6 and 7.7 are built on: a
    skeleton is a hundred of these per character per frame.
    NO NEW HEADER (still 83 public, umbrella lists 82, one documented
    exception), so the configure-time lint had nothing to say — the first
    Module 7 lesson where it did not fire. NO NEW DEMO TARGET; `gimbal` gained
    [S] and is now the instrument for the whole rotation arc (7.1 rings +
    det J, 7.2 single turn + blend, 7.4 commutation + double cover, 7.5
    schedule + the long way round). 42 checks green, TWELVE of them controls.
    WHAT IS NEW IN math/quat.hpp: nearest + quat_pow_unit + quat_slerp +
    quat_nlerp + angle_between_by_cosine, and angle_between REWRITTEN with
    atan2 (see `quat-metric` under `conventions:`).
    WHAT IS NEW IN math/transform.hpp: transform::rotation is a `quat`;
    transform_extraction + transform_from_affine + k_transform_square_tolerance.
    WHAT IS NEW IN gfx/renderable.hpp: renderable_report::skewed.
    WHAT MOVED: ~25 call sites across ten files. ecs/camera.hpp (look_along),
    gfx/scene.hpp (model_matrix), gfx/renderable.cpp, demos/common/demo_scene.cpp,
    demos/hello_cube, demos/gltf_view (transform_of loses its body to the
    engine), demos/ecs_swarm (its local copy deleted), demos/collector (the boom
    becomes TWO entities; the carousel and spinner gain renormalised_fast),
    demos/gimbal ([S]).
    WHAT IS STILL NOT: scene_object STILL HOLDS A transform RATHER THAN A mat4,
    so two functions now take a matrix apart only to hand the pieces to
    parent_from_local, which puts it back together. That round trip IS the
    argument for the change, and it is a published struct with a dozen callers,
    so 7.5 NAMES IT AND DOES NOT MAKE IT — same discipline as 5.12's
    view<const T>. NO transform_slerp: nothing has asked for one; 7.7 will, and
    will also have to decide whether scale lerps or interpolates geometrically.
    NO ANIMATION: 7.6 is the skeleton, 7.7 the sampling and blending.
  - 7.4 THE ENGINE CAN WRITE DOWN A ROTATION OF SPACE IN FOUR FLOATS.
    82 -> 83 public headers (math/quat.hpp; the umbrella lists 82, one
    documented exception). Header-only again: the harness links nothing, which
    for this lesson is half the argument — the whole representation is four
    floats and a multiplication rule. 42 checks green, TEN of them controls.
    No new demo target; `gimbal` gained two modes and is now the instrument for
    the whole of Module 7's rotation arc (7.1 rings + det J, 7.2 single turn +
    blend, 7.4 commutation + double cover).
    WHAT IS NEW: quat + quat::i/j/k + quat::pure + operator*(quat,quat) +
    operator*(quat,vec3) + conjugate + length_squared + length + normalised +
    normalised_or + renormalised_fast + inverse + reflect_in_plane +
    rotor_from_mirrors + rotate + quat_from_axis_angle (two overloads) +
    quat_x/y/z + quat_from_euler + axis_angle_from_quat + angle_between +
    mat3_from_quat + quat_from_rotation.
    WHAT IS STILL NOT: NO STORAGE CHANGE. transform::rotation is a mat3 until
    7.5, and the reason is measured rather than scheduled — see
    `quat-swap-deferred` under `decisions:`. NO SLERP: 7.5 builds it, and it
    needs the shortest-arc sign choice the double cover forces, which has
    enough content to be a section rather than a line. `angle_between` IS here,
    because it is a metric and not an interpolation.
    THE COSTS, MEASURED, AND ONE OF THEM IS A LOSS:
      compose   quat 1.666 ns vs mat3 2.612 ns    1.57x CHEAPER
      apply     quat 1.679 ns vs mat3 1.070 ns    1.57x DEARER
      convert   mat3_from_quat 4.619 ns
      crossover 7.58 VECTORS PER ROTATION  <- the number to design with
      repair    renormalised_fast 1.059 vs Gram-Schmidt 3.437   3.25x cheaper
      storage   16 bytes vs 36
    "Quaternions are faster than matrices" is TRUE of storing and composing and
    FALSE of the thing a frame spends its time on. Rotate a point with the
    quaternion; rotate a mesh by building the matrix.

  - 7.3 THE ENGINE CAN COMPOSE A ROTATION BY MULTIPLYING, AND INTERPOLATE ONE.
    81 -> 82 public headers (math/complex.hpp; the umbrella lists 81, one
    documented exception). Header-only again: the harness links nothing, and
    for this lesson the empty link line is HALF THE ARGUMENT. 43 checks green,
    TWELVE of them controls. One new demo target, `plane` — the smallest in the
    repository, smaller than pong, and the first 2-D one since 5.2.
    WHAT IS NEW: complex + complex::i + operator*(complex,complex) +
    operator*(complex,vec2) + conjugate + length_squared + length + normalised +
    normalised_or + renormalised_fast + inverse + complex_from_angle +
    angle_from_complex + angle_between + complex_pow_unit + reflect_in_line +
    rotor_from_mirrors + apply_rotor + mat2_from_complex + complex_from_mat2 +
    vec2_from_complex + complex_from_vec2 + complex_slerp + complex_nlerp.
    WHAT IS STILL NOT: no storage change, and 7.3 does not even argue for one —
    a complex rotates the PLANE and nothing in this scene graph is 2-D.
    transform::rotation is a mat3 until 7.4.
    THE LAYERING IS DELIBERATELY BROKEN. rotation.hpp sits above euler.hpp and
    axis_angle.hpp because all three are about rotations of SPACE. complex.hpp
    includes mat2.hpp and vec2.hpp and nothing in that layer, and its
    `angle_between` is NOT an overload of `angle_between_rotations` because the
    plane's answer is SIGNED and space's cannot be. Written up in
    ARCHITECTURE.md so it is not "fixed" later.
    ITS REAL ROLE IS SEQUENCING: 7.4's quat.hpp is written as a DIFF against
    this file, function for function — conjugate, length_squared, normalised,
    inverse, operator*, renormalised_fast, complex_slerp each get a twin with
    the same body, one more imaginary unit, and one loss (commutativity).
  - 7.2 THE ENGINE CAN NAME THE SINGLE TURN A ROTATION IS, AND TRAVEL IT.
    79 -> 81 public headers (math/rotation.hpp and math/axis_angle.hpp; the
    umbrella lists 80, one documented exception). Header-only again: the harness
    links nothing. 42 checks green, ELEVEN of them controls.
    WHAT IS NEW: axis_angle + rotate_about_axis + rotation_from_axis_angle +
    axis_route + axis_angle_extraction + axis_angle_from_rotation +
    k_axis_angle_reversal_angle + k_axis_angle_identity_angle +
    rotation_from_rotation_vector + rotation_vector_from_rotation +
    rotation_slerp. Plus demos/gimbal's [A] and [B] modes and --blend/--axis-angle.
    WHAT MOVED: angle_between_rotations and its by-trace twin left
    math/euler.hpp for math/rotation.hpp, as euler.hpp's own doc comment asked
    in 7.1. euler.hpp INCLUDES rotation.hpp, so NOT ONE CALL SITE CHANGED — a
    refactor that makes its callers edit is one that keeps being postponed.
    WHAT IS STILL NOT: no storage change. transform::rotation is a mat3 until
    7.4, and 7.2 strengthens rather than weakens that: axis-angle has no usable
    composition formula at all, so it is a worse storage format than the matrix
    it would replace.
    THE ENGINE CAN NOW BLEND TWO ORIENTATIONS CORRECTLY, which it could not
    before at any price: rotation_slerp, 0.00% excess turning at every pose.
  - 7.1 THE ENGINE CAN BE TOLD AN ORIENTATION IN THREE NUMBERS. 78 -> 79 public
    headers (the 79th is math/euler.hpp; the umbrella lists 78, one documented
    exception), sources and shaders unchanged — the whole lesson is header-only
    and links nothing. 34 checks green, six of them CONTROLS.
    WHAT IS NEW: euler_angles + rotation_from_euler + euler_extraction +
    euler_from_rotation + k_euler_lock_epsilon + euler_rate_jacobian +
    wrap_angle + shortest_angle_delta + angle_between_rotations +
    angle_between_rotations_by_trace (kept, called by nothing, for §F's
    comparison). Plus demos/gimbal.
    WHAT IS NOT: no storage change. transform::rotation is a mat3 until 7.4.
    THE ONE PUBLIC-API GAP THE DEMO FOUND: debug_lines has no ellipse/circle in
    an arbitrary plane. `sphere()` is three great circles but they are
    axis-aligned in WORLD space, and a gimbal ring's plane has been carried by
    every rotation outside it. Demo has a 15-line private ring(); an
    `ellipse(centre, u, v, colour)` would cover rings, orbits, cones and camera
    FOV arcs. FILED FOR 9.7, where the editor's gizmos need exactly this.
    5.12's CONFIGURE-TIME UMBRELLA LINT FIRED FOR REAL, on the first lesson
    after it existed, on a header written ten minutes earlier. That is the
    evidence 5.12 could not produce for itself.
  - 6.18 THE ENGINE CAN SAY SOMETHING. 75 -> 78 public headers, 47 -> 50 sources,
    24 -> 26 shaders. 98 checks green, 0 failures (CPU sections run without a
    GPU; §I and §J need one, driver `metal`). Counts MEASURED against commit
    422d414, per 6.15's rule: re-measure, do not increment.
    WHAT IS NEW: glyph + kern_pair + font_atlas + font_bake_options +
    font_status + bake_font + load_font + next_codepoint + glyph_quad +
    text_layout_options + text_metrics + layout_text + measure_text
    (font.{hpp,cpp}, the ONE translation unit containing stb_truetype);
    overlay_vertex + overlay_batch + overlay_blend + composite_overlay +
    apply_stem_darkening (overlay.{hpp,cpp}, which mentions NEITHER renderer);
    gpu_overlay + overlay_viewport_uniforms + overlay_shading_uniforms
    (gpu_overlay.{hpp,cpp}); overlay.vert.hlsl + overlay.frag.hlsl;
    gpu_texture::create_coverage (R8_UNORM, never _SRGB);
    assets/fonts/Karla-Regular.ttf (16,848 B, SIL OFL 1.1, provenance and
    licence in assets/fonts/OFL.txt).
    - THE THREE-FILE SPLIT IS THE LOAD-BEARING DECISION. `font.hpp` and
      `overlay.hpp` name no SDL_GPU type, so `hello_cube` — no GPU device —
      gets a HUD in twenty lines, and §J can render one `overlay_batch` through
      BOTH renderers and compare every channel. 68 of 16,384 pixels differ, by
      at most ONE sRGB code. The claim is deliberately weaker than 6.17's
      bit-equality, because the two sides are not the same code: the CPU
      re-encodes with `engine::linear_to_srgb` and the GPU's ROP uses
      fixed-function hardware of unspecified intermediate precision. The
      instrument reports the DISTRIBUTION, not a yes/no. Control: 934 differing.
    - 6.11's 43% ARRIVED AS AN ARTEFACT WITH A NAME. Ink mass (total light added,
      in linear light) over "Handgloves 0123" at 16 px: encoded blending gives
      62.2% of correct on black, 137.8% on white. SAME ERROR, OPPOSITE
      DIRECTIONS — which is why the bug survives review. The error is worst at
      LOW coverage (10.3% at a tenth), and a 16 px glyph is mostly edge, so it
      presents as STEM WEIGHT rather than as a colour shift.
    - TWO BUGS IN TWO FILES ARE THE SAME FUNCTION. An `_SRGB` coverage atlas
      gives 62.1% — indistinguishable from encoded blending on a dark ground,
      because both are `srgb_to_linear` applied to the coverage at different
      points. The separating test is in the lesson: invert the contrast. A
      blend-space bug is directional (137.8%); a coverage bug is not (62.1%).
    - THE UNORM FALLBACK IS `blend_over_encoded` IN SILICON, measured: the same
      draw at a plain UNORM target with `encode = 1` differs from the CPU's
      deliberately-wrong function on **0 pixels** and from the right one on 933.
      6.1 predicted this in a comment; it is now an equality.
    - COMPOSITE AFTER THE TONEMAP. ACES(1.0) = 0.803797 = sRGB code 232, and it
      moves with exposure (165 at x0.25, 252 at x4). A UI colour is
      display-referred. Diegetic UI is the deliberate exception.
    - SNAP THE POSITION, NEVER THE ADVANCE. Worst error 0.4765 px, bounded at
      0.5 forever, against 5.26 px worst and 4.32 px still out at the end of a
      45-glyph line. The pen is never rounded; the quad is.
    - KERNING LIVES IN GPOS. The shipped font has NO legacy `kern` table. 186
      non-zero pairs of 9,025 (2.06%), stored sparse: 2,232 B against 36,100.
    - A "16 PX" FONT HAS A 13.6869 PX EM. `ScaleForPixelHeight` maps
      ascent−descent, and Karla's is 1.169 em. Its `line_gap` is ZERO.
    - DIGITS ARE NOT TABULAR (4.530 px for '1', 8.418 for '8'), so "11.1 ms" and
      "88.8 ms" differ by 11.66 px and a readout breathes.
      `text_layout_options::tabular_digits` fixes it exactly.
    - ONE PIPELINE, ONE TEXTURE, ONE DRAW CALL for text AND panels, bought with a
      2x2 fully-covered block in the atlas. A steady-state frame of text performs
      ZERO heap allocations (200 rebuilds, global `operator new` replaced).
    - FIRST EXTERNAL USE OF 6.17'S API, and the branch it named as untested —
      `keep` on an IMPORTED resource at version 0 — works and now has four
      checks. And the honest half: `discard_write` on a blended target compiles
      happily and derives DONT_CARE. **A frame graph relocates a dataflow claim;
      it does not verify it.**
  - 6.17b THE ENGINE HAS LAMPS, AND EACH ONE CAN CAST A SHADOW.
    light.hpp: local_light {kind point|spot, position, direction (of TRAVEL),
    colour, intensity = irradiance at 1 m (default k_reference_irradiance, so a
    default lamp at 1 m IS the sun), range (<= 0 infinite), inner/outer half-angles
    (glTF's 0 and pi/4), casts_shadow}; inverse_square (Frostbite's 1 cm floor),
    range_window ((1 - (d/r)^4)^2, Karis/Frostbite), cone_terms_of + spot_cone
    (glTF's scale/offset, squared), sample_local_light, shade_local;
    surface_brdf lifted out of shade() statement for statement.
    shadow.hpp/.cpp: light_camera gains perspective/near/far/eye/axis;
    perspective_depth, perspective_slope (sin a cos p / cos t), the 2^-22
    rounding floor, fit_spot (80-degree cap), fit_cube_face (a MIRROR from
    cube_face_axes), local_shadow_settings (512 spot, 256 cube face, radius 0),
    local_shadow_set (built ON shadow_map); visibility's perspective branch.
    cubemap: cube_axes + cube_face_axes. clip/soft_renderer: GUARD-BAND
    CLIPPING at +-7936 px, only under near_mode::clip and only outside the band;
    clip_stats::guard_clipped. raster: fill_style::local_lights (span) +
    local_shadows; the loop after shade().
    GPU: gpu_local_light (11 float4, 176 B, matrix as ROWS), local_light_count in
    6.15's pad at offset 180, create_depth_cube_array (asks SupportsFormat
    first), gpu_local_shadows (budgets 4 spots / 2 points, jobs, render and
    render_into), a depth-only pipeline shared with the sun, scene_local_lights
    as a trailing render()/render_batched() argument, renderer-set count, three
    identity fallbacks, an 8-slot sampler bind, the storage bind at slot 0
    (HLSL t8, space2), gpu_event_kind::bind_storage. scene.frag.hlsl: the lamp
    loop, spot and cube lookups with reach (r+1)sqrt2.
    demo: gltf_view --lights [--lamp-shadows 0] (CPU renderer).
  - 6.17 THE FRAME IS A DECLARATION, AND FOUR FACTS STOPPED BEING MAINTAINED.
    74 -> 75 public headers, 46 -> 47 sources, 24 shaders (unchanged — this
    lesson added none). 40 checks green, 0 failures (7 CPU-only, 33 needing a
    GPU). Counts MEASURED against commit 1c83edd, per 6.15's rule: re-measure,
    do not increment.
    WHAT IS NEW: frame_graph + fg_texture + fg_texture_desc + fg_init + fg_use +
    fg_pass_context + fg_execute_fn + dump_frame_graph (frame_graph.{hpp,cpp});
    gpu_shadow_map::render_into; bloom_stage + gpu_bloom::record_stage, with
    gpu_bloom::render rewritten to go through it.
    WHAT IS MEASURED RATHER THAN CLAIMED: the compiled schedule against the
    hand-written order, name for name, 14 of 14; every derived load and store op
    against the hand-written one, 8 of 8, including 4.7's DONT_CARE and 6.8's
    STORE; eleven bloom passes culled by a dependency rather than a flag; five
    silent mistakes turned into named errors; 8.62 us and ZERO allocations per
    compile (global operator new replaced to count); and the memory claim, which
    came out at ZERO saved of a possible 14.3% for two separate reasons — see
    frame-graph-memory.
    THE GOLDEN IS BYTE-IDENTICAL AT E917C06C FOR THE TWENTY-SIXTH LESSON, AND IT
    IS A NULL INSTRUMENT FOR THE THIRD LESSON RUNNING. Confirmed STRUCTURALLY
    before running it: write_reference_shot renders through soft_renderer.cpp and
    raster.cpp, neither of which includes frame_graph.*, gpu_shadow.* or
    gpu_post.*. The real instrument was BUILT — the same frame assembled by hand
    and compiled from a declaration, 0 of 262,144 channels differing — WITH A
    CONTROL that reports 196,608 differing on a bloom-less frame, which is 6.16's
    finding about golden_615 applied rather than quoted.
    THE CLEAN-TREE BUILD WAS RUN and is green with no warnings at -Wall -Wextra.
  - 6.16 THE ENGINE DECIDES WHAT NOT TO DRAW, AND DRAWS THE REST IN FEWER CALLS.
    72 -> 74 public headers, 44 -> 46 sources, 23 -> 24 shaders, 9 -> 10 scene
    pipelines. 60 checks green, 0 failures (47 CPU-only, 13 needing a GPU).
    WHAT IS NEW: frustum_of / classify / intersects / cull_visible over an aabb
    or a sphere (frustum.{hpp,cpp}); sphere + bounds_of + bounding_sphere on
    bounds.hpp, redeeming the promise 6.8 wrote into its own header; gpu_instance
    + describe_instances + batch_instances (instancing.{hpp,cpp});
    scene_instanced.vert.hlsl; a tenth pipeline and
    gpu_scene_renderer::render_batched, sharing render()'s prologue through the
    new private begin_frame.
    WHAT IS MEASURED RATHER THAN CLAIMED: the far plane's 1.4e-3 displacement
    traced OUT of the culler and INTO perspective() (332x amplification); 0.69%
    false positives against ZERO false rejects; 2.41x worst-case box inflation;
    the sphere pre-test LOSING in both regimes; a break-even cull rate predicted
    at 0.2446% and then bracketed; 12 objects making 12 batches.
    THE GOLDEN IS BYTE-IDENTICAL AT E917C06C FOR THE TWENTY-FIFTH LESSON, AND
    THAT IS A NULL RESULT, NOT A PASS. Nothing in the reference shot is ever
    outside the frustum (14 objects across 8 frames, 0 culled), so the golden
    cannot see this lesson at all. Checked, and then the instrument checked: the
    same code culls 1 when an object is moved 200 units off screen. Part 2 needed
    a correctness instrument BUILT — render vs render_batched, 0 of 76,800
    channels differing, with a lit-pixel count guarding the null.
    THE CLEAN-TREE BUILD WAS RUN, which is 6.15's finding applied: a build only
    ever run incrementally cannot tell you it is wrong. `rm -rf build-clean &&
    cmake -S . -B build-clean && cmake --build build-clean` succeeds with the
    24th shader from scratch.
  - 6.15 AN ENVIRONMENT LIGHTS THE SCENE AND IS THE SKY BEHIND IT, ON BOTH
    RENDERERS. 71 -> 72 public headers, 43 -> 44 sources, 21 -> 23 shaders.
    Golden byte-identical at E917C06C for the TWENTY-FOURTH lesson, and the
    argument was checked FIRST and found structural SIX times over (twice
    6.14's three): the fixture's `lighting` has no environment field; shade()
    never calls image_based_light; cube_map/environment/the BRDF table are types
    the fixture never constructs; microfacet.hpp's two new functions are called
    by neither shade() nor cook_torrance_specular; the half-float pair has no
    CPU-raster caller; and every shader and GPU change is gated behind
    ibl_intensity, which defaults to 0.
    (THE COUNTS ABOVE ARE MEASURED, and they correct an off-by-one that has been
     carried for at least two lessons: `ls engine/include/engine/*/*.hpp
     engine/include/engine/*.hpp | wc -l` gives 71 at commit 9830dd3 where
     6.14's entry says 72, and 21 shaders where it says 20. Future lessons:
     re-measure, do not increment.)
    See conventions:environment, cube-faces, solid-angle, prefilter-level,
    two-fitted-steps, half-float, cube-gpu, ibl-uniforms and ibl-subtraction
    above for the substance.
    THE CAPABILITIES: a `cube_map` of six hdr_buffer faces with the D3D face
    table asserted against SDL's enum; an IRRADIANCE convolution (exact for
    Lambert); a GGX-PREFILTERED chain and a BRDF TABLE (the split sum); a
    from-scratch binary16 codec; `create_cube` on the device; a skybox with no
    geometry on both renderers; and `--env` in gltf_view.
    WHAT IS DELIBERATELY MISSING, so it is not rediscovered as a bug: ONE probe,
    infinitely far away (no parallax, no local reflections); NO multi-scattering
    compensation, so the BRDF table sums to 0.3276 at roughness 1 and 67% of the
    energy is lost — which is NOT the split sum's error but the single-scattering
    Smith G, and 6.3's probe measured the same thing at 0.3069 from the other
    direction; and the prefilter runs on the CPU in ~5 s, which is a loading
    screen you would not ship. All three are exercises 11.3-11.5.
    THE DEMO: `--env` changes 100% of the frame. The orbit camera sits 26
    degrees above the scene with a 50-degree fov, so the horizon falls ~1 degree
    ABOVE the top of frame and the whole background is the LOWER hemisphere —
    which is why `sky_settings` gained `ground_horizon` and the ground now fades
    toward the horizon (physically motivated: real ground is brightest where it
    faces the most sky, and a constant lower hemisphere is the one part of a
    naive sky model that is obviously wrong).
    `--env-size 16` starves the source cube and the failure is instructive:
    rough materials are unchanged (a cosine convolution has nothing finer than
    60 degrees to lose) while a polished one shows facets.
  - 6.14 ANTIALIASING, BOTH KINDS, AND THE HONEST ACCOUNT OF WHAT EACH REACHES.
    71 -> 72 public headers, 42 -> 43 sources, 20 shaders (none added). Golden
    byte-identical at E917C06C for the TWENTY-THIRD lesson.
    See conventions:antialias, antialias-thesis and msaa above for the substance.
    The three capabilities: SUPERSAMPLING on the CPU (render at N times the linear
    resolution, box-filter down IN LINEAR LIGHT), NDF FILTERING in both renderers
    (widen alpha to cover the pixel's normal variation), and MSAA on the GPU
    (sample counts on targets and pipelines, plus the resolve).
    THE RESOLVE IS 6.1's RULE FOR THE FOURTH TIME, after 6.10's mip chains and
    6.11's compositing: a half-covered edge is code 188 in linear light and code
    127 in bytes, and the wrong one delivers 21.2% of the light instead of 50%.
    The symptom is a THIN DARK OUTLINE on every silhouette, usually misdiagnosed
    as edges being composited twice. `encoded_average` keeps the bug selectable.
    IT WILL KEEP ARRIVING, because every new way of AVERAGING is a new chance to
    average the wrong quantity.
    THE DEMO: 8,599 of 388,800 bytes change between 1x and 4x — 2.2% of the image,
    and that 2.2% is what everybody notices. Distinct red values 166 -> 184.
    `--aa-encoded` differs from the correct resolve by up to 30 codes at edges.
    `--spec-aa` changes 3.6% of the frame with a peak difference of 185 codes, and
    the still frame looks DIMMER at the highlight — that is the energy being
    spread, and the payoff is only visible in motion.
  - 6.13 A BLOOM IN BOTH RENDERERS, AND THE TWO QUESTIONS 6.12 DEFERRED, ANSWERED.
    70 -> 71 public headers, 41 -> 42 sources, 17 -> 20 shaders. Golden
    byte-identical at E917C06C for the TWENTY-SECOND lesson.
    THE FRAMING, WHICH THE HARNESS DECIDED: 6.12 stopped bright values being
    destroyed by the FRAGMENT SHADER; it did not make them VISIBLE, and the lid
    only MOVED. Bisected through the engine's own `to_encoded`, the smallest
    linear value that still resolves to code 255 is 0.9955 (clamp), 3.9556
    (reinhard_white W=4), 6.3774 (ACES) and 223.4789 (reinhard) — so 6.12's
    polished metal STILL spends 13.10 stops on one code under the best operator
    the engine has. ACES's 6.3774 was checked independently against the positive
    root of 0.090828x^2 - 0.557371x - 0.139376 = 0 (= 6.3773), which is what makes
    it a measurement rather than a restatement.
    AND NOTE WHICH OPERATOR HAS THE HIGHEST LID: reinhard, at 223.5, the one
    everybody agrees looks worst — because it spends its codes creeping toward a
    white it never reaches. TONEMAPPING IS NOT THE REMOVAL OF CLIPPING, IT IS THE
    CHOICE OF WHERE TO CLIP. No curve can do better: an 8-bit sRGB display spans
    code 1 (linear 0.00030353) to code 255, which is 11.69 STOPS TOTAL, fixed by
    the hardware.
    SO THE FIX IS A CHANGE OF VARIABLE, and it is a PREDICTION WITH AN EXPONENT
    rather than a metaphor. A real lens scatters, so a point brighter than white
    lands as a spike with wide skirts, and the skirts are BELOW the lid even when
    the source is far above it. If the tail falls as r^-2 then the radius at which
    the glow crosses any fixed visibility bar goes as sqrt(L) and the AREA goes as
    L. MEASURED over three decades against a fixed bar (the linear value ACES +
    sRGB turns into exactly code 128, = 0.14927): area/L = 1.150, 1.238, 1.140 —
    linear to within 8.6%, radius/sqrt(L) to within 4%. The same three values
    through a clamp are three identical white dots.
    WHERE IT STOPS, MEASURED NOT HEDGED: at 55,917 the law wants a radius of ~146
    texels and the buffer is 128 across, so it saturates at 16,384. Not the law
    failing — the law running out of room, which is also why exposure has to do
    some of this work and bloom cannot do all of it.
  - 6.13 THE PYRAMID IS NOT A CHEAP APPROXIMATION — IT IS A BETTER SHAPE, and
    that is the claim the usual cost argument crowds out. Adding every level on
    the way up makes the composite kernel a SUM OF GAUSSIANS WHOSE WIDTHS DOUBLE,
    and a geometric sum of Gaussians has an approximately power-law envelope.
    Measured on a delta through the real chain: log-log slope -1.86, -2.01, -2.19
    over successive octaves — an INVERSE-SQUARE tail, which is roughly what
    measured glare in a real eye does (Vos & van den Berg's 1/theta^2).
    THE FAIR COMPARISON IS THE OCTAVE RATIO, because it is normalisation-
    independent: over r = 32 to 64 the pyramid falls 6.0x and a sigma-16 Gaussian
    falls 403x. A factor of 67 in TAIL SHAPE that no choice of scale can move.
    exp(-r^2) does not have a dimmer tail than the pyramid; it does not have one.
    AND THE POWER LAW ENDS RATHER THAN DECAYING: past the pyramid's reach there is
    no level left to contribute, so the slope steepens to -2.59 at r = 64. Six
    levels from half resolution reach 64 half-res texels = 128 full pixels.
    `k_max_bloom_levels` is 8 for exactly this reason.
    COST: the whole pyramid is 0.3330 of one full-res target — THE SAME ONE THIRD
    as a mip chain (6.10), because 1/4 + 1/16 + ... = 1/3 — which is 1.32 MB
    against the HDR target's 3.96 MB at 960x540. Fragments: 129,600 bright +
    43,020 down + 172,500 up = 345,120, which is 0.666 OF ONE FULL-SCREEN PASS.
    One separable sigma-64 Gaussian of comparable reach would be 385 taps per axis
    and 399 MILLION texel fetches for the same frame.
  - 6.13 BOTH FILTERS ARE DERIVED, NOT CHOSEN, and each identity buys something
    specific. ONE BILINEAR TAP AT THE CORNER SHARED BY FOUR TEXELS RETURNS THEIR
    UNWEIGHTED MEAN (both fractional weights are exactly 1/2, so all four products
    are 1/4) — checked on real numbers: 1, 2, 4, 8 -> 3.750000. So the downsample
    is ONE fetch, not four, performed by filtering hardware that was going to run
    anyway. THAT is why `gpu_bloom`'s sampler is LINEAR where the 6.12 resolve's
    is deliberately NEAREST: here the filter mode is carrying ARITHMETIC rather
    than smoothing, and setting it to nearest makes every stage a silent point
    decimation that still LOOKS blurry because five more levels follow.
    A BOX CONVOLVED WITH A BOX IS A TENT: [1 1] * [1 1] = [1 2 1]. So a bilinear
    magnification ALREADY IS a tent filter and the 3x3 kernel is that vector's
    outer product over 16, summing to exactly 1 — asserted before anything else
    in §C, because every energy claim in the lesson rests on it.
    ADDRESSED BY SV_POSITION, NOT BY THE INTERPOLATED uv, in all three shaders.
    `uv * source_size` equals `2x + 1` only when the source is EXACTLY twice the
    destination, and integer halving breaks that at every odd level (135 halves to
    67; 67 doubles to 134). The explicit form is what makes the GPU agree with the
    CPU to the last BIT rather than to the last EVEN DIMENSION.
    THE TWO MAPPINGS MUST BE INVERSES: `2*dst + 1` going down, `(dst + 0.5) * 0.5`
    coming back up. They are why the pyramid does not drift, and a missing
    half-texel shift compounds across six levels into a bloom that slides
    diagonally away from what produced it.
  - 6.13 THE SOFT KNEE HAS EXACTLY ONE FORM, and the artefact it fixes is in the
    DERIVATIVE. A hard threshold's slope jumps 0 -> 1 at T, so a pixel drifting
    0.999 -> 1.001 goes from contributing nothing to contributing its full excess,
    and the set of pixels at exactly T is a CONTOUR that moves with the camera —
    a crawling edge along every gradient that crosses the threshold, which reads
    as a sampling bug rather than a thresholding one.
    THREE CONDITIONS, THREE COEFFICIENTS: value 0 and slope 0 at T-k, slope 1 at
    T+k. One quadratic fits: f(x) = (x - T + k)^2 / 4k. Checked: f(T-k) = 0,
    f'(T-k) = 0.0000, f(T) = 0.125 = k/4, f(T+k) = 0.5 = k, f'(T+k) = 1.0002.
    A PIXEL EXACTLY AT T CONTRIBUTES k/4, NOT NOTHING — that is the whole
    difference between a soft cut and a hard one.
    SELECT ON LUMINANCE, SCALE THE COLOUR. Thresholding per channel subtracts a
    constant from each, which moves a saturated colour TOWARD WHITE — the filter
    would desaturate the very thing it selects, and a fire would bloom pale.
    Scaling by a scalar is a move along the ray from black, so hue survives.
  - 6.13 FIREFLIES, AND THE TRADE THE ONE-TAP DOWNSAMPLE FORCES. One pixel of
    6.12's polished metal (55,917) dropped into an otherwise ordinary 256x256
    field over the threshold — 0.00153% of the frame — contributes 62.8% OF THE
    FINISHED BLOOM'S ENERGY (82,922 of 132,074). It is sub-pixel, so it appears
    and vanishes as the camera moves by a pixel, and nearly two thirds of the glow
    blinks with it.
    A clamp at 100 removes 99.3% of that and leaves the honest bloom within 1.19%.
    Unusually cheap BECAUSE the firefly is so far out of family that a ceiling
    sixty times above every legitimate value still catches it.
    AND THE CLAMP RUNS AFTER THE 2x2 AVERAGE, NOT BY CHOICE: the average happens
    INSIDE the bilinear fetch, so by the time the shader sees a number the four
    source texels no longer exist separately. Clamping the TEXELS would leave
    0.30% instead of 1.19% — a factor of FOUR, measured by pre-clamping the source
    buffer. THAT GAP IS THE PRICE OF THE FREE BOX FILTER, and it is the real
    reason shipping engines use Karis's 13-tap kernel: not because it is wider,
    but because it is THE ONLY SHAPE THAT CAN SEE WHAT IT IS AVERAGING.
  - 6.13 THE ORDERING IS PHYSICS, NOT POLICY, IN TWO PLACES.
    (1) THE COMPOSITE GOES BEFORE THE CURVE. Scattering happens in the LENS,
        before the sensor responds, so a bloom is light and is photographed by the
        same curve as the light that did not scatter. The arithmetic agrees more
        bluntly: the curve's output is already in [0,1], so anything added lands
        above 1 and the encode clips it. MEASURED on one image: 324 of 4,096
        pixels clip composited after, 32 composited before — TEN TIMES as many.
        Every glow grows a flat white core that grows with the source, which is
        exactly the artefact bloom exists to remove, reintroduced one pass later.
    (2) THE THRESHOLD IS IN EXPOSURE-CORRECTED LIGHT. Its job is to separate "will
        look bright on the display" from "will not", which is a question about the
        FINISHED IMAGE, and exposure is what decides it. In scene-referred units
        instead, a dim room at a high exposure looks bright and blooms nothing:
        the bug reads as "the bloom turns itself off indoors".
        SO THE BRIGHT PASS APPLIES THE EXPOSURE AND THE RESOLVE MUST NOT AGAIN.
        Giving the two different values changes 4,064 of 4,096 pixels.
    WHICH PRODUCES AN ARCHITECTURAL FACT: EXPOSURE IS NOT A STAGE IN THE STACK, IT
    IS A PROPERTY OF THE FRAME THAT SEVERAL STAGES MUST AGREE ABOUT.
    `gpu_post_stack::resolve_into` hands both halves the same struct field for
    precisely this reason.
  - 6.13 THE OWNERSHIP ANSWER, WHICH IS THE NARROW ONE ON PURPOSE.
    `gpu_bloom` OWNS THE PYRAMID, because a pass that knows its own intermediates
    is the only thing that can know their lifetimes: nothing outside it needs
    level 3, nothing outside it knows how many levels there are, and every
    intermediate's life begins and ends inside one `render` call.
    `gpu_post_stack` OWNS WHAT CROSSES BETWEEN STAGES — the float scene target the
    scene pass writes and both later stages read, plus the 1x1 black stand-in.
    ELEVEN RENDER PASSES FOR SIX LEVELS, and `2n-1` is a FLOOR rather than an
    implementation detail: a pass writes ONE set of colour targets, and no pass
    may read the texture it writes (a read-after-write with no ordering available
    inside a pass — which is what forces the ping-pong every post chain has, and
    which the bloom escapes only because every stage reads one level and writes a
    different one). At eleven, the bloom issues more render passes than the whole
    rest of this engine's frame. THAT is the number 6.17's frame graph now has to
    justify itself against.
    WHAT THE TYPE CANNOT DO, STATED IN ITS OWN HEADER: you cannot insert a stage
    without editing it. No list, no registry, no `add_stage`, no declared
    read/write set. At TWO stages that is right — a registry with two entries is
    an architecture pretending to be a feature, which is 6.5's argument and the
    one 6.12 declined to make with one stage. It stops being right at four or
    five, when intermediates outlive the stage that produced them and two stages
    want same-sized targets at different times.
    "NO BLOOM" HAS TO BE SPELLED AS A 1x1 BLACK TEXTURE. SDL_GPU has no way to
    UNBIND a sampler, and a pipeline whose shader declares `t1` must have
    something bound on every draw. The alternatives are a second tonemap pipeline
    compiled without the fetch (doubling this pass's pipeline count to save one
    texel) or a shader branch (a divergent fetch across a wavefront to save a
    multiply). One texel wins.
    THE PIPELINE COUNT IS NOW THIRTEEN: 9 scene (6.11's 3x3) + 1 shadow + 1
    resolve (6.12) + 3 bloom. Bright and downsample differ in NOTHING but their
    fragment shader, which is exactly the situation that makes a hash-keyed
    pipeline cache pay. Still enumerated, because thirteen is countable and
    `create_ms()` can then say what each one cost — but note where the pressure
    comes from: not the number of effects, but that every effect MULTIPLIES
    against every existing axis.
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
  - 5.12 THE ENGINE HAS A GAME BUILT ON IT, BY SOMEBODY STANDING OUTSIDE.
    56 -> 57 public headers (57th is gfx/renderable.hpp), 32 -> 33 engine sources,
    and a sixth demo. NOT A RENDERING CHANGE: the golden is byte-identical and
    the lesson says so structurally rather than hopefully.
    engine/gfx/renderable.{hpp,cpp}   the COMPONENT the engine's renderer reads,
                                      and collect_renderables().
    engine/engine.hpp                 40 -> 55 listed headers. FIRST CHANGE SINCE
                                      5.11, and the first time anything compiled it.
    engine/CMakeLists.txt             the umbrella LINT, at configure time.
    demos/collector/main.cpp          the game. 1,205 lines, 23 engine includes,
                                      30 entities, 6 game components, 5 systems.
    API: renderable{mesh, tint, surface, closed} (6.5 folds the middle two into
    `material mat`), renderable_report{drawn, unresolved, missing_mesh},
    collect_renderables(registry&, const mesh_pool&, vector<scene_object>&).
    SEVEN FINDINGS, TWO FIXED — see conventions:checkpoint-findings.
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
```
