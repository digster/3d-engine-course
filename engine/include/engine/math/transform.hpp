// engine/include/engine/math/transform.hpp — where a thing is, which way it faces, and how big it is.
//
// Lessons 2.5-2.7 built the machinery: mat3 for the linear part, mat4 for the
// affine one, and `w` to say which of a position and a direction you meant.
// Nothing in any of those files knows what a scene is. This one does.
//
// A `transform` is the answer to three questions an artist and a designer ask
// about every object — how big, which way, where — and `parent_from_local()`
// turns those three answers into the one matrix that carries the object's
// vertices out of MODEL space and into WORLD space.
//
// The name of that function is the point of Lesson 2.8, and it is not decorative:
//
//     parent_from_local   reads right to left, like the matrices it builds.
//                         Feed it something in LOCAL space, get something in
//                         PARENT space. Compose two and the inner labels have to
//                         match, or you have written a bug:
//
//                             view_from_model = view_from_world * world_from_model
//                                                     ^^^^^        ^^^^^
//                                                     these must agree
//
// Nearly every transform bug in graphics is a coordinate used in the space it is
// not in. The maths cannot catch it — a vec3 is three floats whatever room it
// lives in — so the naming has to. Lesson 2.8 §2.
//
// Why "parent" and not "world": in Module 2 every object stands on its own, so
// its parent IS the world and `parent_from_local` returns a world-from-model
// matrix. Module 5 adds a transform HIERARCHY, where an object's transform is
// relative to another object rather than to the world. When that happens this
// function does not change by a single character — only the meaning of "parent"
// widens. Naming it `world_from_local` today would have been a lie we had to go
// back and correct.

#pragma once

#include <engine/math/mat4.hpp>
#include <engine/math/quat.hpp>

#include <cmath>

namespace engine {

/// Position, orientation and size — an object's place in the world.
///
/// This is the seed of Module 5's transform component, and the shape is not
/// arbitrary: it is what essentially every engine exposes, because it is what a
/// human can actually author. Nobody types sixteen floats to place a crate.
///
/// **LESSON 7.5 MADE THE SWAP THIS COMMENT HAS PROMISED SINCE MODULE 2.**
/// `rotation` was a `mat3` for five modules and is now a `quat`. The promise
/// moved three times — 7.1, then 7.4, then here — so it is worth writing down
/// what was finally traded, in both directions.
///
/// WHAT THE ENGINE GAINS. Four floats instead of nine, and composition in 16
/// multiplies instead of 27 (Lesson 7.4 §10 measures both, and honestly: a
/// quaternion is 1.57× *dearer* to apply to a vector, which is why
/// `parent_from_local` below converts once per object and then multiplies
/// vertices by a matrix). But the row that decided it is neither of those:
///
///   **A `mat3` cannot be interpolated and a `quat` can.** Average two rotation
///   matrices entrywise and the result is not a rotation — it is a shear that
///   shrinks. There is no way to blend two poses stored as matrices without
///   converting to something else first, and *blending two poses* is what an
///   animation system does for a living. `quat_slerp` is three lines
///   (`math/quat.hpp`), it walks the shortest arc at constant speed, and Lessons
///   7.6 and 7.7 are built on it. The storage had to change before the animation
///   could exist.
///
/// WHAT IT COST, and it was not the line count. **The field is called `rotation`
/// and one caller in the engine had never put a rotation in it.**
/// `gfx/renderable.cpp` built a `transform` from an ECS `world_transform` by
/// assigning `linear_of(w.matrix)` here and `{1,1,1}` to `scale`, and its comment
/// said — correctly — that the recomposition "reproduces the original affine
/// matrix to the bit". It reproduced it because **a `mat3` will hold anything**,
/// including the scale that came down the hierarchy from a parent. A `quat` will
/// not, so narrowing the type turned that line into a compile error, which is the
/// type doing exactly the job it is here to do. The repair is a decomposition
/// rather than a rename — take the scale off as the three column lengths, then
/// extract — and Lesson 7.4 §11 measured what the missing decomposition would have
/// cost: a uniform scale of 2 extracts to something whose determinant is 1.65
/// rather than to either 8 or 1, and a non-uniform one also *rotates* the result
/// by 1.21°. `collect_renderables` now does the decomposition, reports what it
/// cannot represent, and says so in its own header.
///
/// **A narrower type does not only prevent future mistakes. It finds existing
/// ones** — this one had been shipping since Lesson 5.11.
///
/// **And note what Module 7 deliberately did NOT put here**: `euler_angles`.
/// Three angles are the smallest possible storage and they are still the wrong
/// one — they cannot be composed without converting to a matrix, they cannot be
/// interpolated without leaving the geodesic (measured in 7.1: 14% of extra
/// turning on a generic pair, 209% near lock), and they have a singularity. Euler
/// angles are an INTERFACE, for a human and for somebody else's file format, and
/// `math/euler.hpp` is where they live. Axis-angle is a *reading* — the answer to
/// "what single turn is this pose?" — and lives in `math/axis_angle.hpp`. A
/// matrix is what the renderer multiplies by. A quaternion is what the engine
/// stores.
struct transform
{
    /// Where the object's origin sits, in parent space. A POSITION (Lesson 2.7).
    vec3 position{0.0f, 0.0f, 0.0f};

    /// Which way the object faces. Identity means "aligned with the parent's axes".
    ///
    /// **A unit quaternion**, and the unit part is a precondition rather than an
    /// invariant this struct enforces — `quat` is four public floats and anyone
    /// may write to them. Code that steps this field incrementally (integrating an
    /// angular velocity, spinning a carousel by `rate * h` every frame) should run
    /// `renormalised_fast` on the result, which is one subtract and five multiplies
    /// and is the whole of what keeping a rotation a rotation costs here. The
    /// `mat3` this replaced needed Gram-Schmidt for the same job.
    quat rotation = quat::identity();

    /// How big, along the object's OWN axes — which is why scale is applied
    /// first and why `(1,1,1)` rather than `(0,0,0)` is the do-nothing value.
    vec3 scale{1.0f, 1.0f, 1.0f};
};

/// The matrix that carries the object's vertices from local space to parent space.
///
/// This is `T * R * S` — **scale first, then rotate, then translate** — and the
/// order is derived rather than decreed (Lesson 2.8 §3.2). In one line: scale is
/// defined along the object's own axes, so it has to act while the coordinates
/// are still the object's own; rotation is about the object's own origin, so it
/// has to act while the object is still at the origin; translation puts the
/// finished object where it belongs, so it goes last. Every other order breaks
/// one of those three sentences, and two of the breakages are visible on screen
/// in this lesson's demo.
///
/// **Read the returned matrix column by column and it spells out the object's
/// frame**, which is the single most useful thing to know when a scene has gone
/// wrong:
///
///     column 0 = the object's x axis in parent space, times its x size
///     column 1 = the object's y axis in parent space, times its y size
///     column 2 = the object's z axis in parent space, times its z size
///     column 3 = the object's origin in parent space  (= `position`)
///
/// That is not a coincidence to be memorised. Lesson 2.5 established that a
/// matrix's columns are where the basis vectors land, and the basis vectors of
/// model space ARE the object's own axes. The scale multiplies each column
/// because `S` sends `(1,0,0)` to `(sx,0,0)` before `R` ever sees it.
[[nodiscard]] inline mat4 parent_from_local(const transform& t)
{
    // Written as three scaled columns rather than `t.rotation * scale(...)`,
    // which produces exactly the same nine floats. Two reasons, in order of
    // importance:
    //
    //   1. It SAYS the thing. The line below is the doc comment above, in code:
    //      column k is axis k scaled by size k. Composing two matrices and
    //      trusting the result to have that property is a fact you have to
    //      remember; writing it is a fact you can read.
    //   2. It is nine multiplies instead of twenty-seven, because a diagonal
    //      matrix's product is not a general product. Small, and not the reason —
    //      but there is no cost to taking it.
    //
    // LESSON 7.5 ADDED THE FIRST LINE AND NOTHING ELSE, which is the whole
    // argument for having a named type here rather than passing three loose
    // variables around: the storage changed under every caller in the engine and
    // the arithmetic below did not move a character.
    //
    // **That line is also engine policy arriving as one statement.** Lesson 7.4
    // §10.3 measured the crossover — a quaternion is 1.57× dearer than a matrix
    // at rotating a vector, converting costs 4.6 ns, so building the matrix first
    // pays for itself after about eight vectors. A mesh has thousands. So the
    // engine stores quaternions, converts exactly once per object per frame,
    // right here, and multiplies every vertex by a matrix. "Quaternions are
    // faster than matrices" is true of storing and composing and false of the
    // thing a frame actually spends its time on.
    const mat3 basis = mat3_from_quat(t.rotation);

    const mat3 axes{basis.c0 * t.scale.x,
                    basis.c1 * t.scale.y,
                    basis.c2 * t.scale.z};

    return affine(axes, t.position);
}

/// What `transform_from_affine` recovered, and what it could not.
///
/// The same shape as `axis_angle_extraction` (Lesson 7.2) and `euler_extraction`
/// (7.1), and for the same reason: a recovery that can lose information should
/// hand back the information about what it lost, in the same breath, rather than
/// leave the caller to find out from the picture.
struct transform_extraction
{
    /// The recovered placement. Always finite and always usable.
    transform value;

    /// **The largest |cosine| between two of the recovered basis directions.**
    ///
    /// Zero for any matrix that really is (rotate, then scale along the object's
    /// own axes); positive when the matrix has SHEAR in it, which no `transform`
    /// can hold. It is a cosine, so it reads as an angle: 1e-3 is 0.057° out of
    /// square, and float error through five or six hierarchy multiplications
    /// lands three orders below that.
    float out_of_square = 0.0f;

    /// The matrix had **negative determinant** — it mirrors as well as rotates.
    ///
    /// Recorded rather than fixed. The sign is folded into `value.scale.x`, so the
    /// recomposition is exact; the flag is here because "this object is
    /// inside-out" is usually a content bug and is always worth being able to see.
    bool mirrored = false;
};

/// Take an affine matrix apart into the three fields a `transform` holds.
///
/// **LESSON 7.5, AND THE ENGINE ONLY DISCOVERED IT NEEDED THIS WHEN A TYPE GOT
/// NARROWER.** Three places in this repository were doing the job already —
/// `gfx/renderable.cpp`, `demos/ecs_swarm` and `demos/gltf_view` — and two of
/// them were doing it by putting the matrix's whole linear part into a field
/// called `rotation` and writing `1` into `scale`. That reproduced any affine
/// matrix to the bit, **because a `mat3` will hold anything**. A `quat` will not,
/// so those two stopped compiling, and the third — which had been doing it
/// properly all along — turned out to be the function all three wanted.
///
/// THREE STEPS, AND THE SECOND IS THE ONE NOBODY WRITES DOWN.
///
///   1. **The scale is the column lengths.** `parent_from_local` builds the
///      matrix by scaling each basis column, and scaling a unit vector by `s`
///      makes its length `s`. Reading the lengths back is therefore the inverse
///      of the construction, not an estimate of it.
///
///   2. **A negative determinant is a mirror, and no quaternion is one.** Every
///      rotation has determinant +1; an object scaled by `(−1, 1, 1)` has
///      determinant −1 and is a reflection, which is not in the rotation group at
///      all. Column lengths are non-negative by construction, so the sign has to
///      be handed back deliberately — and it goes on `x` because it has to go
///      somewhere: any single negated column with its negated scale rebuilds the
///      same matrix. Skip this step and a mirrored object silently becomes an
///      unmirrored one that is *rotated*, which is the kind of bug that gets
///      blamed on the artist.
///
///   3. **Divide the lengths out and extract.** `quat_from_rotation` has no bad
///      case anywhere (Lesson 7.4 §9: four candidates that sum to exactly 4, so
///      the pivot is never below 1), which is what makes this safe to run on
///      every visible object every frame without a threshold to worry about.
///
/// **WHAT IT CANNOT DO, reported rather than hidden.** A parent with non-uniform
/// scale and a rotated child produce a world matrix with SHEAR, and shear is not
/// (rotation × axis-aligned scale) under any decomposition — so no assignment to
/// a `transform` can be right. Column lengths cannot even see it, because a shear
/// changes the ANGLES between the columns and not their lengths. So the angles
/// are measured and returned in `out_of_square`, and the caller decides whether
/// to care. Lesson 7.4 §11.4 measured the residual: a 0.30 shear leaves a worst
/// entry error of 0.14 after the best repair either route can make.
///
/// **A ZERO-LENGTH COLUMN IS REBUILT FROM THE OTHER TWO, and the obvious
/// alternative is finite and wrong.** A degenerate object — scale 0 on an axis,
/// which is how a sheet of paper or a collapsed node is authored — has no
/// direction to read off that column. The version this engine inherited from
/// `demos/gltf_view` substituted the PARENT's axis and said, correctly, that this
/// "keeps the matrix finite instead of producing NaNs". It does. It also produces
/// a basis whose three columns are not perpendicular, and `quat_from_rotation` of
/// a non-orthonormal basis is wrong in **every** column, not just the bad one —
/// measured at 0.399 of absolute entry error on a flattened object whose other
/// two axes were perfectly recoverable. The zero column is the one that does not
/// matter (its scale multiplies it away); the two good ones are the ones that got
/// broken.
///
/// A cross product of the surviving pair is the right answer and costs six
/// multiplies: it is perpendicular to both by construction, unit because they
/// are, and correctly handed because the order is cyclic. With two columns gone
/// there is a one-parameter family of correct answers and any of them rebuilds
/// the object exactly, so one is chosen; with three gone, the object is a point
/// and the identity is as good as anything.
[[nodiscard]] inline transform_extraction transform_from_affine(const mat4& m)
{
    const mat3 linear = linear_of(m);

    transform_extraction out;
    out.value.position = translation_of(m);
    out.value.scale = {length(linear.c0), length(linear.c1), length(linear.c2)};

    out.mirrored = determinant(linear) < 0.0f;
    if (out.mirrored) { out.value.scale.x = -out.value.scale.x; }

    const bool live_x = out.value.scale.x != 0.0f;
    const bool live_y = out.value.scale.y != 0.0f;
    const bool live_z = out.value.scale.z != 0.0f;

    vec3 bx = live_x ? linear.c0 * (1.0f / out.value.scale.x) : vec3{1.0f, 0.0f, 0.0f};
    vec3 by = live_y ? linear.c1 * (1.0f / out.value.scale.y) : vec3{0.0f, 1.0f, 0.0f};
    vec3 bz = live_z ? linear.c2 * (1.0f / out.value.scale.z) : vec3{0.0f, 0.0f, 1.0f};

    const int live = static_cast<int>(live_x) + static_cast<int>(live_y)
                   + static_cast<int>(live_z);
    if (live == 2)
    {
        // Cyclic, so the handedness is right without a special case: x = y × z,
        // y = z × x, z = x × y.
        if (!live_x) { bx = cross(by, bz); }
        else if (!live_y) { by = cross(bz, bx); }
        else { bz = cross(bx, by); }
    }
    else if (live == 1)
    {
        // One direction survives and the other two are free. Take any vector not
        // parallel to it — the least-aligned coordinate axis, which cannot be
        // within 54° of a unit vector — and build a frame from there.
        const vec3 known = live_x ? bx : (live_y ? by : bz);
        const vec3 seed = (std::fabs(known.x) < std::fabs(known.y))
                              ? ((std::fabs(known.x) < std::fabs(known.z))
                                     ? vec3{1.0f, 0.0f, 0.0f} : vec3{0.0f, 0.0f, 1.0f})
                              : ((std::fabs(known.y) < std::fabs(known.z))
                                     ? vec3{0.0f, 1.0f, 0.0f} : vec3{0.0f, 0.0f, 1.0f});
        const vec3 other = normalised(cross(known, seed));
        if (live_x) { by = other; bz = cross(bx, by); }
        else if (live_y) { bz = other; bx = cross(by, bz); }
        else { bx = other; by = cross(bz, bx); }
    }
    else if (live == 0)
    {
        bx = {1.0f, 0.0f, 0.0f};
        by = {0.0f, 1.0f, 0.0f};
        bz = {0.0f, 0.0f, 1.0f};
    }

    const float xy = std::fabs(dot(bx, by));
    const float xz = std::fabs(dot(bx, bz));
    const float yz = std::fabs(dot(by, bz));
    out.out_of_square = (xy > xz) ? ((xy > yz) ? xy : yz) : ((xz > yz) ? xz : yz);

    out.value.rotation = quat_from_rotation(mat3{bx, by, bz});
    return out;
}

/// How far out of square a decomposition may be before a caller should complain.
///
/// A cosine, so it reads as an angle: 0.057°. Chosen to sit three orders above
/// the float error a deep hierarchy accumulates and three orders below the
/// smallest shear anybody authors by accident.
inline constexpr float k_transform_square_tolerance = 1e-3f;

/// The matrix that carries a point from PARENT space back into this object's
/// local space — the exact inverse of `parent_from_local`.
///
/// **LESSON 7.6 FINALLY WROTE THE FUNCTION LESSON 2.8 DECLINED TO.** The comment
/// that stood here for five modules said it was "left out because nothing needs
/// it yet, and because the first thing that will need it deserves to derive it
/// rather than to find it already written". That was the right call and this is
/// the caller it was waiting for: a skeleton's INVERSE BIND MATRICES are nothing
/// but this function composed down the joint chain (`anim/skeleton.hpp`).
///
/// **NO GENERAL 4x4 INVERSION HAPPENS HERE, and that is the point.** Inverting a
/// product reverses it, so
///
///     (T · R · S)⁻¹  =  S⁻¹ · R⁻¹ · T⁻¹  =  S⁻¹ · Rᵀ · T⁻¹
///
/// and every one of those three is free. A rotation's inverse is its transpose
/// (Lesson 2.6 — its columns are orthonormal, so RᵀR = I by inspection). A
/// scale's inverse is the reciprocals. A translation's inverse is the negation.
/// The general route — cofactors, a determinant and a divide — costs about forty
/// operations, can be numerically poor, and would be answering a question this
/// struct already knows the answer to. **A type that knows how it was built knows
/// how to undo itself.**
///
/// Read the result the way `parent_from_local`'s doc comment reads its own: the
/// linear part is Rᵀ with row `k` divided by `scale[k]`, so it *unrotates* and
/// then *unscales*, in that order, and the translation is whatever it takes to
/// send `position` back to the origin.
///
/// **A ZERO SCALE HAS NO INVERSE, AND THIS RETURNS SOMETHING ANYWAY — read this
/// paragraph before you rely on it.** A scale of 0 on an axis collapses a
/// dimension, and no matrix un-collapses one; the information is gone. The
/// reciprocal is taken as 0 rather than as an infinity, which makes the result
/// the PSEUDO-inverse: it projects onto the surviving axes instead of inverting,
/// so `local_from_parent(t) * parent_from_local(t)` is a projection and **not**
/// the identity. That is a real answer to a different question, and Lesson 7.5's
/// rule applies in full — *finite is not right*. The value is finite so that a
/// degenerate joint cannot spray NaNs across a whole skeleton; the caller is
/// expected to have been told, and `anim::validate` is what tells it
/// (`skeleton_report::singular_binds`).
[[nodiscard]] inline mat4 local_from_parent(const transform& t)
{
    // Rᵀ. `transpose` rather than `mat3_from_quat(conjugate(q))`, which produces
    // the same nine floats: the transpose is nine moves and the conjugate route
    // is a negate plus the twelve multiplies of a rebuild. Both are correct and
    // one of them says "this is an inverse because the matrix is orthonormal",
    // which is the fact worth reading off the line.
    const mat3 unrotate = transpose(mat3_from_quat(t.rotation));

    // The reciprocals, with 0 standing in where there is nothing to invert.
    const float rx = (t.scale.x != 0.0f) ? 1.0f / t.scale.x : 0.0f;
    const float ry = (t.scale.y != 0.0f) ? 1.0f / t.scale.y : 0.0f;
    const float rz = (t.scale.z != 0.0f) ? 1.0f / t.scale.z : 0.0f;

    // S⁻¹ · Rᵀ scales ROW k by 1/scale[k] — and a row in column-major storage is
    // one component taken from each column, which is why this reads as three
    // component-wise multiplies rather than as a matrix product. Nine multiplies
    // instead of twenty-seven, for the same reason `parent_from_local` builds its
    // columns by hand instead of composing with `scale(...)`.
    const mat3 linear{{unrotate.c0.x * rx, unrotate.c0.y * ry, unrotate.c0.z * rz},
                      {unrotate.c1.x * rx, unrotate.c1.y * ry, unrotate.c1.z * rz},
                      {unrotate.c2.x * rx, unrotate.c2.y * ry, unrotate.c2.z * rz}};

    // …and the offset that sends `position` to the origin. Not `-position`: the
    // linear part acts on the difference, so the translation this matrix applies
    // is the one measured AFTER unrotating and unscaling.
    return affine(linear, -(linear * t.position));
}

// ---------------------------------------------------------------------------
// Lesson 7.7 — being BETWEEN two placements
// ---------------------------------------------------------------------------
//
// Everything above this line answers "where is it". An animation asks a harder
// question — "where is it a third of the way from here to there" — and the
// answer is not one rule applied three times. A `transform` holds three fields
// that live in three different kinds of space, and each one needs its own
// argument for what "between" means.
//
// *** THE NAME IS DELIBERATE, AND IT CORRECTS AN EXERCISE. *** Lesson 7.5 §12
// asked the reader to write `transform_slerp`, and that name is wrong: exactly
// one of the three fields slerps. Calling the whole function after the rule that
// governs a third of it is the kind of name that makes a caller believe the
// position is travelling on an arc. `transform_blend` claims nothing, and the
// two paragraphs below are what it actually does.
//
//   POSITION — LERP, and there is no case to answer. Positions live in an affine
//   space: the only structure a point has is the straight line to another point,
//   and a straight line at constant speed is the unique path that invents no
//   information. Anything curved would be a claim about a trajectory that the two
//   endpoints do not contain.
//
//   ROTATION — NLERP, and this one is a judgement with a number behind it.
//   Lesson 7.5 measured the whole tradeoff: nlerp walks the same great circle as
//   slerp (excess turning 0.00%) on a schedule that is wrong by `sec^2(W/2)`,
//   which is 0.13 degrees at a 30-degree arc, 4.07 at 90 and 26.34 at 150 — and
//   it is 3.42x cheaper. THE ARCS THIS FUNCTION ACTUALLY SEES ARE SMALL: two
//   adjacent keyframes of a 30 Hz clip are a few degrees apart even on a limb
//   moving fast, which is three orders inside the threshold. The glTF 2.0
//   specification says slerp **SHOULD** be used for rotations and then says, in
//   the same appendix, that "implementations **MAY** approximate these equations
//   to reach application-specific accuracy and/or performance targets" — and
//   notes that "when `a` is close to zero, spherical linear interpolation turns
//   into regular linear interpolation", which is the same observation from the
//   other end. `transform_blend_slerp` is one line below for the caller whose
//   arcs are NOT small, and Lesson 7.7 §7 is about how to know which you are.
//
//   SCALE — LERP, and this is the decision Lesson 7.5 left open, so it gets a
//   paragraph rather than a clause. The alternative is GEOMETRIC interpolation,
//   `a*(b/a)^t`, which is the one that gives equal RATIOS in equal times where
//   the lerp gives equal DIFFERENCES. On a growth from 1 to 8 they disagree
//   loudly: the lerp's midpoint is 4.5 and the geometric one's is
//   `sqrt(8) = 2.828`, and only the second of those looks like steady growth,
//   because scale composes by multiplication all the way down a hierarchy and
//   because perceived size is closer to logarithmic than to linear.
//
//   THREE THINGS SETTLE IT FOR THE LERP ANYWAY, in increasing order of how hard
//   they are to argue with.
//
//     1. THE GAP IS INVISIBLE OVER THE RANGE CONTENT USES. The two rules differ
//        at the midpoint by exactly the arithmetic-mean-minus-geometric-mean gap,
//        `(a+b)/2 - sqrt(ab)`. Squash and stretch lives between about 0.8 and
//        1.25, where that is 0.62%; at a full doubling it is 6.07%; it only
//        reaches the 59% of the 1-to-8 example at ratios no rig animates.
//     2. A SCALE OF ZERO IS LEGAL, COMMON, AND HAS NO LOGARITHM. Scaling an
//        object to 0 is how animators make it vanish, and mirrored rigs carry
//        negative scale on one side. `ln(0)` and `ln(-1)` end the geometric rule
//        outright — not with a wrong answer but with no answer — and a rule that
//        needs a special case for its most common extreme value is not the rule.
//     3. THE FILE FORMAT SAYS LINEAR. glTF 2.0's `LINEAR` mode is defined as
//        `v_t = (1 - t) * v_k + t * v_{k+1}`, componentwise, for every animated
//        property that is not a rotation. An importer that log-lerps scale plays
//        the file differently from every other viewer on earth, which is a
//        correctness bug wearing a mathematician's coat.
//
//   AND THE REAL ANSWER TO SOMEONE WHO WANTS GEOMETRIC GROWTH IS NOT A RUNTIME
//   RULE. It is another keyframe. Splitting a 1-to-8 growth into six segments of
//   ratio `sqrt(2)` drops the per-segment gap from 59% to 1.5%, and the animator
//   already has the tool for it. A policy the content pipeline can express does
//   not belong in the sampler.
//
// **`t` IS NOT CLAMPED**, for the reason `lerp` and `quat_slerp` are not: the
// engine's convention since Lesson 1.6 is that the caller owns its parameter.
// Values outside [0, 1] extrapolate, which an ease-out overshoot genuinely wants
// and an accident never does silently.

/// The placement `t` of the way from `a` to `b`: lerp, nlerp, lerp.
///
/// The default blend for poses, keyframes and cross-fades. See the block comment
/// above for why each field gets the rule it gets; the short version is that
/// positions have only straight lines, rotations have a sphere, and scale is
/// linear because zero is a legal scale and the file format says so.
[[nodiscard]] inline transform transform_blend(const transform& a, const transform& b, float t)
{
    return {.position = lerp(a.position, b.position, t),
            .rotation = quat_nlerp(a.rotation, b.rotation, t),
            .scale    = lerp(a.scale, b.scale, t)};
}

/// The same blend with the rotation on an exact constant-speed schedule.
///
/// Identical in every respect but one: `quat_slerp` instead of `quat_nlerp`.
/// Reach for it when the two orientations are far apart — a cross-fade between
/// two unrelated poses, a 90-degree turn held on two keys, a camera orbit
/// interpolated over a whole second — and the timing of the middle of the move
/// is something a viewer can see. Lesson 7.5's table is the whole decision
/// procedure: under about 30 degrees of arc the two are within 0.13 degrees of
/// each other, over about 90 the difference is a visible lurch, and
/// `pose_blend_report::worst_arc` (in `anim/clip.hpp`) is how you find out which
/// regime your content is actually in rather than guessing.
[[nodiscard]] inline transform transform_blend_slerp(const transform& a, const transform& b,
                                                     float t)
{
    return {.position = lerp(a.position, b.position, t),
            .rotation = quat_slerp(a.rotation, b.rotation, t),
            .scale    = lerp(a.scale, b.scale, t)};
}


} // namespace engine
