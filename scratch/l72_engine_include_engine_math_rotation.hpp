// engine/include/engine/math/rotation.hpp — what is true of a rotation whatever
// you store it in.
//
// This engine is about to accumulate three more ways to write down an
// orientation — axis-angle (7.2), a complex number's 3-D descendant (7.3), a
// quaternion (7.4) — on top of the `mat3` it has used since Lesson 2.5 and the
// `euler_angles` Lesson 7.1 added. Four representations of one thing is not
// indulgence; each is the right answer to a different question, and choosing
// between them is a real engineering skill. But it does mean the engine needs a
// place for the facts that belong to *rotation itself* rather than to any one
// spelling of it.
//
// This is that place, and today it holds exactly one idea: **the distance
// between two orientations**. Ask "how far apart are these two poses?" and there
// is one answer that does not depend on how either was written down — the angle
// of the single turn that carries one onto the other. Euler's rotation theorem
// (Lesson 7.2 §4.1) is what guarantees such a turn always exists.
//
// WHY IT IS ITS OWN FILE, and why it was not one until now. This function was
// written in Lesson 7.1 and lived in `math/euler.hpp`, whose own doc comment
// said, in as many words, that it did not belong there and that 7.2 should move
// it. That was the right call twice over: a `rotation.hpp` containing one
// function is a cupboard built for a thing you own one of, and a header only
// earns its existence once a second inhabitant justifies the shelf. 7.2 brings
// axis-angle, at which point the θ below stops being an implementation detail
// inside a measurement and becomes half of a representation — so the metric is
// now shared by two files and belongs above both of them.
//
// `math/euler.hpp` includes this header, so nothing that used to call
// `angle_between_rotations` has to change. That is deliberate. A refactor that
// makes its callers edit is a refactor that will not be done.

#pragma once

#include <engine/math/mat3.hpp>

#include <algorithm>
#include <cmath>

namespace engine {

// ---- The metric on rotations ---------------------------------------------------

/// The angle, in radians, of the shortest rotation carrying `a` onto `b`.
///
/// This is the distance function on rotations — the length of the geodesic
/// between two orientations — and it is what lets a sentence like "that
/// interpolation wanders off course" be replaced by a number of degrees. Every
/// measurement in Lessons 7.1 and 7.2 is taken with it.
///
/// **The derivation, and then the reason the code does not follow it.** Any
/// rotation is a turn of some angle θ about some axis — that is Euler's rotation
/// theorem, proved in Lesson 7.2 §4.1. In a basis whose first axis IS that axis,
/// the matrix is block-diagonal with a 2x2 rotation in the corner, so its trace
/// is 1 + 2cos(θ); trace survives a change of basis, so that holds in every
/// basis. With R = Aᵀ·B — undo `a`, then do `b` — the textbook formula falls
/// straight out:
///
///     θ = acos( (tr(R) − 1) / 2 )
///
/// **It is correct and it cannot measure a small angle.** Near θ = 0 the trace is
/// 3 − θ², so a turn of 0.004 rad moves the trace by 1.6e-5 — an eighth of a
/// `float`'s resolution at 3.0. The information is gone before `acos` is called,
/// and `acos` then amplifies whatever is left by its infinite slope at 1. The
/// measured relative error at 0.004 rad is **1.00** — it returns zero — and a
/// path length is a sum of several thousand such steps, so that error is the
/// answer.
///
/// So we take the sine as well. The antisymmetric part of a rotation is
/// R − Rᵀ = 2·sin(θ)·[n]ₓ, whose three distinct entries are a vector of length
/// 2|sin(θ)| — computed as differences of matrix entries, which stay proportional
/// to θ instead of hiding inside a 3. Feed both to `atan2` and the result is
/// accurate at both ends, where each half alone is accurate at one:
///
///     θ = atan2( |R − Rᵀ| / 2 , (tr(R) − 1) / 2 )
///
/// `atan2`'s first argument is a magnitude, so the answer is in [0, π] — which is
/// what "the SHORTEST rotation" means, and why no sign is returned. Lesson 7.1
/// §6.4 measures both forms against a known answer; this one is exact to 1e-6
/// across the range.
///
/// **That vector is not only a magnitude, and 7.2 is where that stops being
/// wasted.** `skew` below is `2·sin(θ)·n` — its *direction* is the axis of the
/// turn, and this function has been normalising it away and returning a length
/// since the day it was written. `axis_angle_from_rotation` in
/// `math/axis_angle.hpp` is the same four lines with the division not thrown out.
[[nodiscard]] inline float angle_between_rotations(const mat3& a, const mat3& b)
{
    const mat3 r = transpose(a) * b;

    // Written-notation (r21 − r12, r02 − r20, r10 − r01). Reading those out of
    // column storage is exactly the trap mat3.hpp §3.3 warns about, so they are
    // spelled with `at(row, col)` rather than with member names: six index pairs
    // a reader can check against the formula above, and no mental transpose.
    const vec3 skew{r.at(2, 1) - r.at(1, 2),
                    r.at(0, 2) - r.at(2, 0),
                    r.at(1, 0) - r.at(0, 1)};
    const float trace = r.at(0, 0) + r.at(1, 1) + r.at(2, 2);

    return std::atan2(length(skew) * 0.5f, (trace - 1.0f) * 0.5f);
}

/// The same distance, computed the way every reference states it.
///
/// Kept **only** so that Lesson 7.1 §6.4 and Lesson 7.2 §7.3 can measure the two
/// against each other, and kept in the engine rather than in the harness so that
/// the comparison is against the real thing. It is the right formula to have in
/// your head and the wrong one to call: see `angle_between_rotations` above for
/// the measurement. Nothing in the engine calls this.
[[nodiscard]] inline float angle_between_rotations_by_trace(const mat3& a, const mat3& b)
{
    const mat3 r = transpose(a) * b;
    const float trace = r.at(0, 0) + r.at(1, 1) + r.at(2, 2);
    return std::acos(std::clamp((trace - 1.0f) * 0.5f, -1.0f, 1.0f));
}

} // namespace engine
