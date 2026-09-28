# The `updated:` log — what each lesson changed, newest first

Moved verbatim from STATE.md's `updated:` block on 2026-09-27, when that file became
a compact resume key (CLAUDE.md §9). Append here; keep STATE.md's headline in step.

```text
updated: 2026-09-28 (after Lesson 6.17b — 97 of 113 lessons published; the FIRST of the
         four lessons inserted by the post-Module 8 review. Written after 8.13 and placed
         between 6.17 and 6.18 by the authoring guide's §17 protocol, which it is the first
         use of. 22 files changed, none created; 40 checks green at -O0 and -O2, and on the
         student's post-6.17b tree (replayed from pins: 287 TUs, 0 warnings).
         ADDITIVE, PROVED: 26 harnesses (6.1-6.18, 7.1-7.8) rerun before/after; only timing
         lines, timing-derived ratios, two log timestamps and one heap pointer differ.
         Golden E917C06C byte-identical. Later pins carried: l618 gpu_texture x2, l75 and
         l84 gltf_view, l84 shadow.hpp — 6.18, 7.5 and 8.4 rebuilt, each changing by exactly
         6.17b's lines (6.18 also by its prev link). 6.17's next link and Recap bridge
         re-pointed to 6.17b. Hours 10 -> 12 (estimate-hours.py: 16,449 words, 1,194 code +
         1,030 comment lines, 5 exercises); M6 ~155 -> ~157 h, course ~772 -> ~774 h; 0.2's
         planner moved with it. Carried the two gpu_scene.cpp partial-bind comment
         corrections. Found four defects outside its scope and fixed none (decisions:
         found-by-617b).)

updated: 2026-09-24 (after Lesson 8.13 — 96 of 107 lessons; *** MODULE 8 COMPLETE ***,
         13 of 13, 72 h. PLANNED AT 5 AND SHIPPED AT 6, like 8.12, and the
         index moved with it: M8 71 -> 72 h, the course ~524 -> ~525 h.
         check-curriculum.py green; check-builders.py green (63/63 plus 813,
         and 511 REPAIRED: its gitignored scratch/l511_fig7.svg had been
         truncated to 0 bytes after 8.12's run and was restored from the
         published page, which archives it); check-page.js `pass: true` at
         1280 AND 390 for 8.13, 8.12, the index, conventions and the toolbox.
         Eleven measured sections, 90 checks, 2.49 s. 8.12's `updated:` text
         is in the `8.12 —` notes block after `roadmap:`; what follows is
         8.13's.

         *** THE ENGINE HAS A CHARACTER CONTROLLER, AND A SHAPE CAST. ***
         phys/cast.{hpp,cpp}: `cast(mover, d, obstacle, cfg)` — conservative
         advancement. phys/character.{hpp,cpp}: `move_character` in five
         stages (carry, recover, sideways, vertical, ground+snap), a
         `character_config` with the knobs each section measures, the proxy
         (`steer_proxy`), and eight closed forms. convex.hpp gains
         `placed_shape` + `place` (the dispatch five scenes had copied — the
         debt paid; the five copies left as they shipped).

         WHY NOT A RIGID BODY, MEASURED (§1): a locked-rotation capsule on 8.10's
         solver at mu 0.6 HANGS on a wall mid-jump at any stick speed above
         g h / mu (0.2725 predicted, 0.2757 bisected); slides 2.91 m after the
         stick is released (the discrete sum from release: 2.9098, to four
         decimals); climbs a 20 cm ledge only at 8 m/s; passes a 10 cm wall 50%
         of the time at 42 m/s. A designer's 1.20 m jump rises 1.1599 m
         (v0^2/2g - v0 h/2 = 1.1596: 8.1's integrator).

         A CAST IS NEWTON'S METHOD ON A CONVEX DISTANCE, AND IT IS SAFE BECAUSE
         EACH STEP STOPS AT A SEPARATING PLANE — so it MUST step from GJK's
         certified LOWER bound (certify().lower). Stepped from gjk_result::
         distance (an upper bound) 377 of 6,049 hits finished inside the skin,
         up to 0.17 mm (REFUSAL: "Newton on a convex function cannot
         overshoot" was true only of the exact distance). 9,996 random casts:
         0 missed, 0 late, 2.75 Newton steps mean, 7 worst; 67 hits stop early
         by up to 1.81 mm where GJK STALLS at a centimetre (8.5 F.5). Worked
         example t = 0.55, errors 0.55 / 0.025 / 4.6e-4 / 1.9e-7, ratio -> 0.9.
         A flat face is ONE step (1.8's swept test was always the first Newton
         step). ONE CAST PER OBSTACLE: stepping on a union went through a wall
         11,194 of 20,000 times. Sphere tracing (gap/|d|) takes 237 steps at
         1 deg where the cast takes 2. THE SKIN (REFUSAL): with no skin a cast
         lands ON the contact, GJK calls it intersecting, and it falls back —
         134 mm short on average, 2.3 m worst; a 0.1 mm skin is WORSE than none
         (7.3% of slides start inside vs 2.1%); >= 1 mm, none. Default 1 cm.

         COLLIDE AND SLIDE (§4): CLIP THE ORIGINAL MOTION, scaled to the time
         left, against every plane (Quake 1's SV_FlyMove). The first draft
         clipped the REMAINDER against every plane and walked back and forth
         in a 120 deg corner at 575 mm/s (REFUSAL: the plane list alone is not
         the fix); last-plane-only 572 mm/s there and burns all 4 slides in a
         60 deg corner. `min_approach` 1e-4: at 0, a character running along a
         wall moves at 0.0000 m/s.

         SLOPES (§5): walkable = n.y >= cos(max_slope), 45 deg. along_ground
         t = d - y (d.n)/n.y keeps the horizontal speed exactly (3.0000 m/s
         on 10-40 deg ramps). Gravity slid along the ground instead of stopped
         creeps at g h sin a (0.081746 vs 0.081750 on 30 deg). Flattening a
         wall normal matters only IN THE AIR (REFUSAL): on the ground the
         re-lay along the floor already discards the climb; hopping into 50 deg,
         0.638 m unflattened vs 0.257. A steep slope is slid down at g t sin a
         with no force anywhere (4.3897 vs 4.3895).

         THE ROUND BOTTOM (§6-§8), R = r + skin = 0.31: free curb R(1 - cos t)
         = 90.80 mm (91.83 bisected); overhang R sin t = 219.20 (219.79 placed,
         215.0 walking at 5 mm/step); least step-up forward R(1 - sin t) =
         90.80 (bisected 46.15/84.67/90.89 at three risers vs 46.32/84.94/
         90.80). TALLEST LEDGE = step_height + free curb = 0.39080 (0.39092
         bisected; REFUSAL: not step_height). GJK's normal at a centimetre is
         good to ~0.2 deg (0.189 worst), which is the 1 mm on the curb. AND
         THE CERTIFICATE'S LOOSENESS SCALES WITH THE OBSTACLE'S SIZE, not the
         tolerance (REFUSAL): standing 10.00-10.07 mm off a 6 m ramp,
         10.02-12.32 mm off a 60 m one, at GJK tol 1e-4 and 1e-7 alike.

         SNAPPING (§7): moved flat, a character skips down a 30 deg slope above
         (g h^2 + 2 skin)/(h tan a) = 2.3617 (onset bisected 2.3561 on an 8 m
         ramp, 2.2537 on a 60 m one — the looser certificate eats the probe's
         reach); laid along it, never. A CREST (REFUSAL: "launches like a
         projectile at a jog"): the round bottom rolls over it while v h <=
         sqrt(R^2 - (R - reach)^2) = 0.1165 m, v < 6.99 m/s; above, it flies
         0.833/1.133/1.617 s vs 2v tan a/g 0.883/1.177/1.648. Stairs down: the
         snap ROLLS OFF the edge by R - overhang, then casts down — 0 frames
         airborne, 9 snaps, worst extra forward 77.7 mm (the draft's tangent-
         plane slide lurched 175 mm); no snap: 54 frames. Snap 0.35 m: a 0.30
         drop is snapped, 0.50 falls 16 frames.

         TUNNELLING (§9): rigid capsule through a 10 cm wall with probability
         max(0, 1 - (w/2 + r)/(v h)), 200 phases per speed: 15.5/30/50/65/79%
         vs 16/30/50/65/79. Controller at a 1 cm wall at 10..10,000 m/s: never,
         stops 10.004-10.051 mm from the face.

         CARRY (§10): by the platform's step as a TRANSFORM, read back with the
         integrator (Delta q = advance_orientation(identity, w, h, rule) =
         normalise(1, h w/2) linearised): radius 1.50000 after a revolution,
         0.000 deg lag, facing turns with it. By VELOCITY: |r|^2 *= 1 + (wh)^2
         a step, (1+(wh)^2)^(N/2) ~ e^(pi w h) a revolution — 1.05375 vs
         1.05375 at 1 rad/s, 1.11065 vs 1.11035 at 2 (189 whole steps). A lift
         that only translates: both rules exact (< 10 um in 2 s).

         THE PROXY (§11): kinematic capsule, STEERED by the chord. Steered, a
         20 kg crate moves at 2.0000 m/s WITH velocity 2.0000, overlap 5.00 mm
         (the slop), coasts 0.3562 m = discrete v^2/2mu g 0.3233 + ONE STEP at
         v (the proxy follows a step late) = 0.3566. TELEPORTED (REFUSAL: "the
         crate stops dead"): the crate had fallen asleep and a zero-velocity
         kinematic never wakes it — WALKED THROUGH (8.12's bug class, by a lie
         about velocity); kept awake, it moves at 1.9941 m/s with velocity 0,
         the character sunk slop + v h/beta - v h = 138.33 mm (138.20); on
         release pushed out of the excess ~ v h/beta (0.1649). Baumgarte gives
         the velocity back (coast 0.3565), same 138 mm sunk. 100 kg crate: a
         wall. Standing on a 20 kg crate: grounded on it, one skin above its
         top, carrying none of the weight. A 200 kg boulder at 5 m/s stops dead
         (0.0688 m/s) against a standing character: ONE-WAY COUPLING. The draft
         recover() also pushed out of heavy DYNAMIC bodies and the boulder
         carried the character 2.94 m (observed on the draft): two owners; now
         recover answers only to fixed and kinematic bodies.

         THE BILL (§12): 0.963 us a move on a flat floor (2 casts, 15 GJK
         iterations), 3.27 on stairs, 6.85 pressed into a corner; the AABB cull
         is linear — 847 us at 10,000 boxes, 10.00x the 1,000-box cost for the
         same 2.05 casts. A region query on 8.8's grid is exercise 5.

         EIGHT REFUSALS (the 18th to 25th in Module 8): no skin is merely
         inconvenient; Newton cannot overshoot; the plane list fixes corners;
         flattening matters on the ground; a tighter GJK tolerance buys
         precision; a crest launches at a jog; step_height is the tallest
         ledge; a teleported proxy's crate merely stops dead.
```
