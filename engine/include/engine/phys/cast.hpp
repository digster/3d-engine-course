// engine/include/engine/phys/cast.hpp — how far can this shape move before it
// touches that one?
//
// Lesson 8.13. Every query in Module 8 so far has been asked of an INSTANT: are
// these two shapes touching now (8.4), how far apart are they now (8.5), how deep
// are they now (8.6). A physics step asks its questions at instants, and 8.10 §6
// measured what that costs — a body arriving at a wall is sampled up to one
// step's travel deep, uniformly, and a thin enough wall is not sampled at all.
//
// A CAST asks about an INTERVAL. The mover travels along `displacement`; the
// answer is the fraction `t` of that trip it can make before it comes within
// `skin` of the obstacle, and the surface it would meet there. Nothing is ever
// sampled at a step boundary, so nothing is ever skipped: the swept test of
// Lesson 1.8, where a ball's straight path across one frame was checked against
// a paddle's face, generalised to any pair of convex shapes.
//
// ---- THE ALGORITHM IS NEWTON'S METHOD, AND IT CANNOT OVERSHOOT -------------------
//
// Let `f(t)` be the distance between the mover, displaced by `t·d`, and the
// obstacle. 8.5's GJK computes `f` at any `t` and hands back the direction `n`
// from the mover's closest point to the obstacle's. The gap closes at the rate
// the motion points along that direction:
//
//     f'(t) = −dot(d, n)
//
// so one Newton step toward the level `skin` is
//
//     t  ←  t + (f(t) − skin) / dot(d, n)
//
// which is CONSERVATIVE ADVANCEMENT, the name the collision literature gives it.
// It would be a guess for most functions. Here it is a proof, and there are two
// ways to see why — one says how fast it converges, the other why it is safe:
//
//   * `f` IS CONVEX in `t`: it is the distance from a point moving along a line
//     (−t·d) to a fixed convex set (the Minkowski difference of 8.5), and the
//     distance to a convex set is a convex function. A convex function lies
//     above every one of its tangent lines, and a Newton step lands exactly
//     where the tangent reaches the target — so the function has not reached
//     it yet. The steps rise monotonically toward the contact and converge
//     QUADRATICALLY once close. 8.13 §3 works a case through by hand.
//
//   * GJK'S DIRECTION IS A SEPARATING PLANE. The slab between the two supporting
//     planes normal to `n` is empty, its width is a LOWER bound on the gap, and
//     under a translation it narrows at exactly `dot(d, n)` per unit `t`, for
//     ANY `n`. Stepping to where the slab is `skin` wide therefore cannot bring
//     the shapes closer than `skin` — even if `n` is not quite the true normal.
//     That is 8.4's separating axis, turned into a time, and it is why `cast`
//     steps from GJK's certified lower bound (`certify`) and not from its
//     `distance`: `distance` is the gap between two witness points, an UPPER
//     bound, and stepping from it finished inside the skin in 377 of 6,049
//     random hits, by up to 0.17 mm. From the lower bound: none.
//
// Two special cases are worth seeing in that light:
//
//   * AGAINST A FLAT FACE `f` is a straight line, the tangent IS the function,
//     and the first step lands exactly. That is 1.8's swept test: one division,
//     the gap over the closing speed. It was always the first Newton step.
//   * AGAINST ANYTHING CURVED the steps converge QUADRATICALLY — the error
//     roughly squares each time — which is why a cast against a capsule or a
//     sphere finishes in three or four GJK calls rather than dozens.
//
// ---- WHAT IT REQUIRES, AND THE MISTAKE THAT BREAKS IT ---------------------------
//
// ONE convex obstacle per cast. A separating plane separates ONE pair, and the
// distance to a UNION of two obstacles is the minimum of two convex functions,
// which is not convex: a Newton step taken on the nearer one's tangent can fly
// straight through the farther one. 8.13 §3 measures it — through a wall in
// 11,194 of 20,000 grazing passes. The fix is the obvious one and it is the
// only correct one: cast against each obstacle separately and keep the smallest
// `t`, which is what `character.cpp` does, and which went through nothing.
//
// And a TRANSLATION. A rotating mover sweeps a volume that is not the Minkowski
// sum of anything, `f` loses its convexity, and conservative advancement then
// needs a bound on the rotational speed to stay conservative. A character does
// not turn its collision capsule, so this file does not pay for that.

#pragma once

#include <engine/math/vec3.hpp>
#include <engine/phys/convex.hpp>
#include <engine/phys/gjk.hpp>

#include <cstdint>

namespace engine::phys
{

/// How a cast finished.
enum class cast_status : std::uint8_t
{
    /// Nothing in the way: the whole displacement is free and `t` is one.
    ///
    /// Includes the case where the mover is already within `skin` of the obstacle
    /// but moving AWAY from it or ALONG it — a character sliding along a wall at
    /// its skin distance is not hitting the wall. See `cast_config::min_approach`.
    clear,

    /// The mover can travel `t` of the way and is then `skin` from the obstacle.
    /// `surface_normal` and `point` describe where.
    hit,

    /// **The two shapes already overlap at `t = 0`.** A cast has no answer for
    /// this — there is no amount of travel that brings them into contact, because
    /// they are past it — and `t` is zero. The overlap is 8.6's question: ask EPA
    /// for the minimum translation out. `character.cpp`'s `recover` does.
    started_inside,

    /// Ran out of iterations. `t` is still SAFE — every iterate of a conservative
    /// advancement is short of the contact — but it is not the contact. Counted
    /// by the harness, and 8.13 §3 has never seen one at the default.
    iteration_limit,
};

/// `"clear"`, `"hit"`, `"started inside"`, `"iteration limit"`.
[[nodiscard]] const char* name_of(cast_status status);

/// Knobs. The defaults are what 8.13 §3 measured.
struct cast_config
{
    /// **Stop this far short of contact**, in metres. Zero means "to touching".
    ///
    /// A cast that stops exactly at contact leaves the two shapes TOUCHING, and
    /// the next query from there is a query about touching shapes — which GJK
    /// answers as `intersecting` whenever the gap is inside its own contact
    /// margin, with no separating direction to report. A controller that casts
    /// every frame would then spend every other frame in EPA. The skin keeps
    /// every query a query about two shapes that are APART, where GJK's answer
    /// is a proven distance with a direction attached.
    ///
    /// **And with no skin the cast often cannot finish at all.** Against a flat
    /// face Newton lands on the contact in ONE step — exactly on it — and GJK
    /// reports that as `intersecting`, so the cast has to fall back to the step
    /// before: the start. 8.13 §3 measured casts with no skin stopping 134 mm
    /// short on average and 2.3 m short at worst. A skin THINNER than GJK's
    /// margin is worse than none (7.3% of slides then start inside, against
    /// 2.1%); a millimetre or more, and none do.
    float skin = 0.0f;

    /// Stop iterating when the gap is within `tolerance · |displacement|` of
    /// `skin`. **Relative to the length of the motion**, for 8.5 §11's reason: an
    /// absolute tolerance quits early on a long cast and spins on a short one,
    /// and the quantity this function returns is a FRACTION of the motion. At
    /// the default, a 5 cm step is resolved to 5 µm.
    float tolerance = 1e-4f;

    /// **A motion whose closing rate is below this fraction of its length is not
    /// approaching.** `dot(d̂, n) ≤ min_approach` counts as sliding along.
    ///
    /// Without it, a character sliding along a wall at exactly its skin distance
    /// reads a closing rate of a few parts in 10⁸ from rounding, is already
    /// within tolerance of the skin, and is reported as HITTING a wall it is
    /// running PARALLEL to. 8.13 §4 measured it: at `min_approach = 0` a
    /// character running along a wall at 3 m/s moves at 0.0000 m/s. The slab
    /// argument above bounds what the threshold can cost: the gap can close by at
    /// most `min_approach · |d|` more — 5 µm on a 5 cm step at the default.
    float min_approach = 1e-4f;

    /// Give up after this many Newton steps and report `iteration_limit`.
    /// Measured in 8.13 §3: the worst of ten thousand random casts took 7, and
    /// the mean was 2.75.
    int max_iterations = 32;

    /// Handed to every GJK call. `initial_direction` is overwritten with the
    /// previous iterate's direction, which is the warm start 8.5 §G measured.
    /// A tighter `tolerance` does NOT make a cast at a centimetre more precise:
    /// GJK stalls on float rounding before it gets there (8.13 §6).
    gjk_config gjk{};
};

/// What a cast found.
struct cast_result
{
    cast_status status = cast_status::clear;

    /// **The fraction of the displacement that is free**, in `[0, 1]`. The mover
    /// may be placed at `origin + t·displacement` and is then at least `skin`
    /// from the obstacle.
    float t = 1.0f;

    /// The gap at `t`, in metres: GJK's CERTIFIED LOWER BOUND, the number the
    /// step was taken from. `skin` to within the tolerance when `hit` — and the
    /// true gap may be larger by however loose that bound was, which at a
    /// centimetre between metre-sized shapes is up to 2 mm when GJK stalls (8.5
    /// §F.5; 8.13 §3 saw it in 67 hits of 6,049). Short of the skin, never past.
    float distance = 0.0f;

    /// **Unit, pointing from the OBSTACLE toward the MOVER** — the normal of the
    /// surface the mover meets, as the mover sees it. Meaningful when `hit`.
    ///
    /// Note which way this points, because it is the OPPOSITE of the engine's
    /// pair convention (conventions.html §9e: an axis points from the first
    /// argument toward the second). Every consumer of a cast wants the surface's
    /// outward normal — to slide along it, to test whether it can be stood on,
    /// to push away from it — and naming it for what it is beats making every
    /// caller write a minus sign it can forget. The field is called
    /// `surface_normal` and not `normal` so that the difference is visible at the
    /// point of use.
    vec3 surface_normal{};

    /// The point on the obstacle closest to the mover at `t`, in world space.
    vec3 point{};

    /// Newton steps taken. One against a flat face that is hit head on.
    int iterations = 0;

    /// GJK iterations summed over every step. The cost that scales with shape.
    int gjk_iterations = 0;

    [[nodiscard]] bool hit() const { return status == cast_status::hit || status == cast_status::iteration_limit; }
};

/// **Sweep `mover` along `displacement` and report where it first comes within
/// `cfg.skin` of `obstacle`.**
///
/// Translation only; one convex obstacle; see the file comment for why both
/// restrictions are load-bearing. The mover's `origin` is where the motion
/// starts. Neither shape is modified.
[[nodiscard]] cast_result cast(const convex& mover, vec3 displacement, const convex& obstacle,
                               const cast_config& cfg = {});

} // namespace engine::phys
