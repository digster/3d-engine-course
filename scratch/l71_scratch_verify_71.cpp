// scratch/verify_71.cpp — every number Lesson 7.1 prints, measured rather than asserted.
//
// Build and run:  sh scratch/build_verify_71.sh
//
// Six sections, and the order is the lesson's:
//
//   A  the convention: what our three angles mean, checked by hand and by machine
//   B  the twenty-four: how wrong is the same triple read the other way?
//   C  gimbal lock as a rank deficiency, with singular values
//   D  the extraction, and what its degenerate branch does and does not lose
//   E  interpolation: the real indictment, with a control that must report zero
//   F  the instrument itself: can the metric measure what E asks it to?
//
// EVERY SECTION CARRIES A CONTROL. Lesson 6.16 found `identical=YES` printed on
// two failed file reads and 6.18 found a `PASS` on `inf > 80.0`; the rule that
// came out of both is that a check whose degenerate case is a pass is not a
// check. So each measurement below is paired with an input that must make it
// FAIL, and the harness reports both.

#include <engine/math/euler.hpp>
#include <engine/math/mat3.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <vector>

using engine::mat3;
using engine::vec3;
using engine::euler_angles;

namespace {

constexpr float k_pi = std::numbers::pi_v<float>;

int checks_run = 0;
int checks_passed = 0;

float deg(float radians) { return radians * 180.0f / k_pi; }
float rad(float degrees) { return degrees * k_pi / 180.0f; }

void check(const char* name, bool ok)
{
    ++checks_run;
    if (ok) { ++checks_passed; }
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
}

/// Largest absolute difference between two matrices, element by element.
float max_element_diff(const mat3& a, const mat3& b)
{
    float worst = 0.0f;
    for (int r = 0; r < 3; ++r)
    {
        for (int c = 0; c < 3; ++c)
        {
            worst = std::max(worst, std::fabs(a.at(r, c) - b.at(r, c)));
        }
    }
    return worst;
}

/// The nine floats of the composite matrix, transcribed from the derivation in
/// Lesson 7.1 §4.4 rather than produced by multiplying. The whole point is that
/// this is a SECOND, independent route to the same matrix — if the derivation on
/// the page is wrong, this disagrees with `rotation_from_euler` and section A
/// fails. Written in row-major order, exactly as the lesson prints it.
mat3 closed_form(euler_angles e)
{
    const float cy = std::cos(e.yaw),   sy = std::sin(e.yaw);
    const float cp = std::cos(e.pitch), sp = std::sin(e.pitch);
    const float cr = std::cos(e.roll),  sr = std::sin(e.roll);

    const float m00 = cy * cr + sy * sp * sr, m01 = sy * sp * cr - cy * sr, m02 = sy * cp;
    const float m10 = cp * sr,                m11 = cp * cr,               m12 = -sp;
    const float m20 = cy * sp * sr - sy * cr, m21 = sy * sr + cy * sp * cr, m22 = cy * cp;

    // Columns, from the rows above. mat3 stores columns (mat3.hpp §3.3).
    return mat3{vec3{m00, m10, m20}, vec3{m01, m11, m21}, vec3{m02, m12, m22}};
}

/// A rotation of `angle` about an axis that has itself been carried by `frame`.
///
/// This is the conjugation `F · R · Fᵀ`: step into the frame, do the elementary
/// turn there, step back out. It is how "rotate about the axis the previous turn
/// just moved" is written without any new machinery, and section A uses it to
/// build the intrinsic sequence the long way as a check on the short way.
mat3 about_moved_axis(const mat3& frame, const mat3& elementary)
{
    return frame * elementary * engine::transpose(frame);
}

/// The twelve axis sequences, as products of elementary rotations.
///
/// A convention is a sequence plus a frame. `intrinsic` multiplies left to right
/// (each turn is about an axis the previous turns moved); `extrinsic` multiplies
/// right to left (every turn is about a fixed world axis). Twelve sequences times
/// two frames is the twenty-four of Lesson 7.1 §4.2.
struct sequence
{
    const char* name;
    int axis[3];     ///< 0 = x, 1 = y, 2 = z
};

const sequence k_sequences[12] = {
    // The six Tait-Bryan orders: three different axes.
    {"xyz", {0, 1, 2}}, {"xzy", {0, 2, 1}}, {"yxz", {1, 0, 2}},
    {"yzx", {1, 2, 0}}, {"zxy", {2, 0, 1}}, {"zyx", {2, 1, 0}},
    // The six proper-Euler orders: the first axis repeats as the third.
    {"xyx", {0, 1, 0}}, {"xzx", {0, 2, 0}}, {"yxy", {1, 0, 1}},
    {"yzy", {1, 2, 1}}, {"zxz", {2, 0, 2}}, {"zyz", {2, 1, 2}},
};

mat3 elementary(int axis, float angle)
{
    if (axis == 0) { return engine::rotation_x(angle); }
    if (axis == 1) { return engine::rotation_y(angle); }
    return engine::rotation_z(angle);
}

mat3 build(const sequence& s, bool intrinsic, float a0, float a1, float a2)
{
    const mat3 r0 = elementary(s.axis[0], a0);
    const mat3 r1 = elementary(s.axis[1], a1);
    const mat3 r2 = elementary(s.axis[2], a2);
    return intrinsic ? r0 * r1 * r2 : r2 * r1 * r0;
}

/// The Jacobian's smallest and largest gain, found WITHOUT using the derivation.
///
/// Sweep a fine grid of unit vectors in rate-space, apply J, and keep the extreme
/// output lengths. Those are the smallest and largest singular values by
/// definition, and finding them this way means section C's closed forms are
/// checked against something that assumes nothing about them.
struct gain_range { float smallest; float largest; };

gain_range brute_force_gain(const mat3& j, int steps)
{
    gain_range g{1e30f, 0.0f};
    for (int i = 0; i <= steps; ++i)
    {
        const float theta = k_pi * static_cast<float>(i) / static_cast<float>(steps);
        for (int k = 0; k < 2 * steps; ++k)
        {
            const float phi = 2.0f * k_pi * static_cast<float>(k)
                            / static_cast<float>(2 * steps);
            const vec3 u{std::sin(theta) * std::cos(phi), std::cos(theta),
                         std::sin(theta) * std::sin(phi)};
            const float len = engine::length(j * u);
            g.smallest = std::min(g.smallest, len);
            g.largest = std::max(g.largest, len);
        }
    }
    return g;
}

/// Total turning along a path of orientations, summed step by step.
///
/// The geodesic — the shortest possible route — is a single call to the metric
/// between the endpoints. Anything longer is detour, and the ratio of the two is
/// the number section E is after.
struct path_stats
{
    float length = 0.0f;      ///< radians of turning actually performed
    float geodesic = 0.0f;    ///< radians of turning strictly required
    float slowest = 1e30f;    ///< smallest per-step turn
    float fastest = 0.0f;     ///< largest per-step turn
};

template <typename Fn>
path_stats measure_path(Fn orientation_at, int steps)
{
    path_stats s;
    mat3 previous = orientation_at(0.0f);
    const mat3 first = previous;
    for (int i = 1; i <= steps; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const mat3 current = orientation_at(t);
        const float step = engine::angle_between_rotations(previous, current);
        s.length += step;
        s.slowest = std::min(s.slowest, step);
        s.fastest = std::max(s.fastest, step);
        previous = current;
    }
    s.geodesic = engine::angle_between_rotations(first, previous);
    return s;
}

euler_angles lerp_angles(euler_angles a, euler_angles b, float t)
{
    return {a.yaw + (b.yaw - a.yaw) * t,
            a.pitch + (b.pitch - a.pitch) * t,
            a.roll + (b.roll - a.roll) * t};
}

euler_angles lerp_angles_shortest(euler_angles a, euler_angles b, float t)
{
    return {a.yaw + engine::shortest_angle_delta(a.yaw, b.yaw) * t,
            a.pitch + engine::shortest_angle_delta(a.pitch, b.pitch) * t,
            a.roll + engine::shortest_angle_delta(a.roll, b.roll) * t};
}

// ---------------------------------------------------------------------------
// A — the convention
// ---------------------------------------------------------------------------

void section_a()
{
    std::printf("\n== A. The convention: intrinsic Y-X-Z, active, right-handed ==\n");

    // A.1  Yaw turns the nose to the LEFT. Our forward is -Z and our right is +X
    //      (Conventions §2), so a positive turn about +Y must send -Z toward -X.
    //      This is the one fact a reader can check without any machinery, so it
    //      is checked first and printed as a vector rather than a verdict.
    const vec3 forward{0.0f, 0.0f, -1.0f};
    const vec3 yawed = engine::rotation_from_euler({rad(90.0f), 0.0f, 0.0f}) * forward;
    std::printf("  yaw +90 sends forward (0,0,-1) to (%+.3f, %+.3f, %+.3f)  [expect (-1,0,0) = left]\n",
                static_cast<double>(yawed.x), static_cast<double>(yawed.y),
                static_cast<double>(yawed.z));
    check("yaw +90 turns the nose left (-X)", yawed.x < -0.999f && std::fabs(yawed.z) < 1e-6f);

    // A.2  Pitch tips the nose UP. Positive rotation about +X turns +Y toward +Z
    //      (right-hand rule), so forward -Z swings toward +Y: up.
    const vec3 pitched = engine::rotation_from_euler({0.0f, rad(90.0f), 0.0f}) * forward;
    std::printf("  pitch +90 sends forward (0,0,-1) to (%+.3f, %+.3f, %+.3f)  [expect (0,1,0) = up]\n",
                static_cast<double>(pitched.x), static_cast<double>(pitched.y),
                static_cast<double>(pitched.z));
    check("pitch +90 tips the nose up (+Y)", pitched.y > 0.999f);

    // A.3  The closed form the lesson derives must equal the product the engine
    //      computes, everywhere. 21^3 = 9,261 triples spanning the full range.
    float worst_closed = 0.0f;
    euler_angles worst_at{};
    for (int i = 0; i < 21; ++i)
    {
        for (int j = 0; j < 21; ++j)
        {
            for (int k = 0; k < 21; ++k)
            {
                const euler_angles e{rad(-180.0f + 18.0f * static_cast<float>(i)),
                                     rad(-90.0f + 9.0f * static_cast<float>(j)),
                                     rad(-180.0f + 18.0f * static_cast<float>(k))};
                const float d = max_element_diff(engine::rotation_from_euler(e), closed_form(e));
                if (d > worst_closed) { worst_closed = d; worst_at = e; }
            }
        }
    }
    std::printf("  derived closed form vs the product, 9,261 triples: worst element diff %.3e\n",
                static_cast<double>(worst_closed));
    std::printf("      (worst at yaw %+.0f pitch %+.0f roll %+.0f deg)\n",
                static_cast<double>(deg(worst_at.yaw)), static_cast<double>(deg(worst_at.pitch)),
                static_cast<double>(deg(worst_at.roll)));
    check("the derivation on the page matches the code", worst_closed < 1e-6f);

    // A.3 CONTROL. Perturb one term of the closed form and the comparison must
    //     notice. Without this, "worst diff 0" is also what a comparison of a
    //     thing with itself prints.
    {
        const euler_angles e{rad(35.0f), rad(20.0f), rad(-50.0f)};
        mat3 nudged = closed_form(e);
        nudged.c1.y += 1e-3f;
        const float d = max_element_diff(engine::rotation_from_euler(e), nudged);
        std::printf("  CONTROL: one element nudged by 1e-3 -> diff %.3e\n", static_cast<double>(d));
        check("CONTROL the comparison can fail", d > 9e-4f);
    }

    // A.4  Intrinsic composition, built the long way. Turn about world +Y; then
    //      about the +X that turn moved; then about the +Z both moved. Each step
    //      is a conjugation, and the result must equal the plain left-to-right
    //      product the engine writes. This is the proof of the sentence
    //      "intrinsic multiplies on the right", not an illustration of it.
    {
        const euler_angles e{rad(37.0f), rad(-24.0f), rad(61.0f)};
        const mat3 f1 = engine::rotation_y(e.yaw);
        const mat3 f2 = about_moved_axis(f1, engine::rotation_x(e.pitch)) * f1;
        const mat3 f3 = about_moved_axis(f2, engine::rotation_z(e.roll)) * f2;
        const float d = max_element_diff(f3, engine::rotation_from_euler(e));
        std::printf("  intrinsic built by conjugation vs Ry*Rx*Rz: worst element diff %.3e\n",
                    static_cast<double>(d));
        check("intrinsic sequences multiply on the right", d < 1e-5f);
    }

    // A.6  THE PAGE'S WORKED EXAMPLE, printed by the binary rather than by hand.
    //      Lesson 7.1 §4.5 pushes (30°, 40°, 50°) through the derivation on paper;
    //      these are the numbers it must land on. CLAUDE.md §10 makes wrong
    //      arithmetic in a worked example a correctness bug, and the cheapest way
    //      to keep that promise is to never transcribe a number by hand.
    {
        const euler_angles e{rad(30.0f), rad(40.0f), rad(50.0f)};
        const mat3 r = engine::rotation_from_euler(e);
        std::printf("  worked example, yaw 30 pitch 40 roll 50, written row by row:\n");
        for (int row = 0; row < 3; ++row)
        {
            std::printf("      [ %+9.6f  %+9.6f  %+9.6f ]\n",
                        static_cast<double>(r.at(row, 0)), static_cast<double>(r.at(row, 1)),
                        static_cast<double>(r.at(row, 2)));
        }
        const mat3 j = engine::euler_rate_jacobian(e);
        std::printf("  its rate Jacobian, columns = the three live axes:\n");
        for (int row = 0; row < 3; ++row)
        {
            std::printf("      [ %+9.6f  %+9.6f  %+9.6f ]\n",
                        static_cast<double>(j.at(row, 0)), static_cast<double>(j.at(row, 1)),
                        static_cast<double>(j.at(row, 2)));
        }
        std::printf("      det J = %+9.6f,  -cos(40) = %+9.6f\n",
                    static_cast<double>(engine::determinant(j)),
                    static_cast<double>(-std::cos(e.pitch)));
    }

    // A.5  Everything it produces is a rotation: orthonormal, determinant +1.
    {
        float worst_orth = 0.0f;
        float worst_det = 0.0f;
        for (int i = 0; i < 21; ++i)
        {
            for (int j = 0; j < 21; ++j)
            {
                const euler_angles e{rad(-180.0f + 18.0f * static_cast<float>(i)),
                                     rad(-90.0f + 9.0f * static_cast<float>(j)), rad(33.0f)};
                const mat3 r = engine::rotation_from_euler(e);
                worst_orth = std::max(worst_orth,
                                      max_element_diff(engine::transpose(r) * r, mat3::identity()));
                worst_det = std::max(worst_det, std::fabs(engine::determinant(r) - 1.0f));
            }
        }
        std::printf("  orthonormality: worst |RtR - I| %.3e, worst |det - 1| %.3e\n",
                    static_cast<double>(worst_orth), static_cast<double>(worst_det));
        check("every output is a rotation", worst_orth < 1e-6f && worst_det < 1e-6f);
    }
}

// ---------------------------------------------------------------------------
// B — the twenty-four conventions
// ---------------------------------------------------------------------------

void section_b()
{
    std::printf("\n== B. The same three numbers, read twenty-four ways ==\n");

    const euler_angles e{rad(30.0f), rad(40.0f), rad(50.0f)};
    const mat3 ours = engine::rotation_from_euler(e);

    std::printf("  triple (30, 40, 50) deg. Disagreement with OUR reading (intrinsic yxz):\n");
    std::printf("      %-6s %10s %10s\n", "order", "intrinsic", "extrinsic");

    int within_one_degree = 0;
    float worst = 0.0f;
    const char* worst_name = "";
    for (const sequence& s : k_sequences)
    {
        float d[2];
        for (int frame = 0; frame < 2; ++frame)
        {
            const mat3 other = build(s, frame == 0, e.yaw, e.pitch, e.roll);
            d[frame] = deg(engine::angle_between_rotations(ours, other));
            if (d[frame] < 1.0f) { ++within_one_degree; }
            if (d[frame] > worst) { worst = d[frame]; worst_name = s.name; }
        }
        std::printf("      %-6s %9.2f° %9.2f°\n", s.name,
                    static_cast<double>(d[0]), static_cast<double>(d[1]));
    }
    std::printf("  worst disagreement %.2f° (%s); readings agreeing with ours to 1°: %d of 24\n",
                static_cast<double>(worst), worst_name, within_one_degree);
    check("exactly one of the 24 readings is ours", within_one_degree == 1);
    check("misreading the convention is not a small error", worst > 45.0f);

    // B.2  The identity that makes it 24 and not 48: an intrinsic sequence is the
    //      REVERSED extrinsic one with the angles reversed too. Ours is intrinsic
    //      y-x-z, so it is extrinsic z-x-y read (roll, pitch, yaw).
    {
        const mat3 extrinsic_zxy = engine::rotation_z(e.roll);
        const mat3 composed = engine::rotation_y(e.yaw) * engine::rotation_x(e.pitch)
                            * extrinsic_zxy;
        const mat3 stepwise = engine::rotation_y(e.yaw)
                            * (engine::rotation_x(e.pitch) * engine::rotation_z(e.roll));
        const float d = max_element_diff(composed, stepwise);
        std::printf("  intrinsic yxz(y,p,r) == extrinsic zxy(r,p,y): worst element diff %.3e\n",
                    static_cast<double>(d));
        check("the intrinsic/extrinsic mirror identity holds", d < 1e-6f);
    }

    // B.3  The practical case: a file written under the aerospace z-y-x reading,
    //      loaded by code that assumes ours. Same bytes, different aeroplane.
    {
        const mat3 aerospace = build(k_sequences[5], true, e.yaw, e.pitch, e.roll);  // zyx
        const float d = deg(engine::angle_between_rotations(ours, aerospace));
        const vec3 forward{0.0f, 0.0f, -1.0f};
        const vec3 a = ours * forward;
        const vec3 b = aerospace * forward;
        std::printf("  aerospace intrinsic zyx read as ours: %.2f° apart\n", static_cast<double>(d));
        std::printf("      nose points (%+.3f,%+.3f,%+.3f) vs (%+.3f,%+.3f,%+.3f)\n",
                    static_cast<double>(a.x), static_cast<double>(a.y), static_cast<double>(a.z),
                    static_cast<double>(b.x), static_cast<double>(b.y), static_cast<double>(b.z));
        check("the zyx/yxz confusion is visible, not subtle", d > 20.0f);
    }

    // B.3 CONTROL. The same comparison against our own convention must report
    //     zero, or the number above is measuring the comparison and not the
    //     conventions.
    {
        const mat3 same = build(k_sequences[2], true, e.yaw, e.pitch, e.roll);  // yxz intrinsic
        const float d = deg(engine::angle_between_rotations(ours, same));
        std::printf("  CONTROL: our own convention, spelled the generic way: %.3e°\n",
                    static_cast<double>(d));
        check("CONTROL the convention comparison reports zero when it should", d < 1e-3f);
    }
}

// ---------------------------------------------------------------------------
// C — gimbal lock, as a rank deficiency
// ---------------------------------------------------------------------------

void section_c()
{
    std::printf("\n== C. Gimbal lock is a rank deficiency, and here is its number ==\n");

    // C.1  det(J) = -cos(pitch), everywhere. The minus sign is the column order,
    //      not a fact about rotation (§4.7).
    {
        float worst = 0.0f;
        for (int i = 0; i <= 360; ++i)
        {
            for (int k = 0; k <= 8; ++k)
            {
                const euler_angles e{rad(static_cast<float>(k) * 40.0f),
                                     rad(-180.0f + static_cast<float>(i)), rad(17.0f)};
                const float d = engine::determinant(engine::euler_rate_jacobian(e));
                worst = std::max(worst, std::fabs(d + std::cos(e.pitch)));
            }
        }
        std::printf("  det(J) + cos(pitch) over 3,249 poses: worst |error| %.3e\n",
                    static_cast<double>(worst));
        check("det(J) = -cos(pitch)", worst < 1e-6f);
    }

    // C.2  The table the lesson prints, and A BUG THIS HARNESS FOUND IN ITSELF.
    //
    //      The closed form derived in §4.8 is sigma_min = sqrt(1 - |sin pitch|),
    //      and it is correct. It is also unusable in `float` near the place it
    //      exists to describe: at pitch 89.99° the true value of |sin| is
    //      1 - 1.5e-8, which is closer to 1.0 than a float can represent, so the
    //      subtraction returns EXACTLY zero and the conditioning number comes out
    //      `inf`. The first draft of this table printed `inf` at 89.99° and the
    //      pose is nowhere near singular — the SAME catastrophic cancellation
    //      against 1.0 that `angle_between_rotations` has in its trace form, met
    //      twice in one lesson.
    //
    //      The algebra removes it. Multiply above and below by (1 + |sin p|):
    //
    //          1 - s  =  (1 - s)(1 + s) / (1 + s)  =  (1 - s²)/(1 + s)  =  c²/(1 + s)
    //
    //      so sigma_min = |cos p| / sqrt(1 + |sin p|), which never subtracts two
    //      nearly-equal numbers and stays accurate all the way in. Both columns
    //      are printed, because the point is the gap between them.
    std::printf("  %8s %10s %11s %11s %10s %12s\n",
                "pitch", "det J", "s_min naive", "s_min stable", "brute", "1/s_min");
    float worst_sv = 0.0f;
    const float pitches[] = {0.0f, 45.0f, 60.0f, 80.0f, 89.0f, 89.9f, 89.99f, 90.0f};
    for (float p : pitches)
    {
        const euler_angles e{rad(41.0f), rad(p), rad(-13.0f)};
        const mat3 j = engine::euler_rate_jacobian(e);
        const float sp = std::fabs(std::sin(e.pitch));
        const float cp = std::fabs(std::cos(e.pitch));
        const float naive = std::sqrt(std::max(0.0f, 1.0f - sp));
        const float stable = cp / std::sqrt(1.0f + sp);
        const gain_range g = brute_force_gain(j, 500);
        if (p < 89.5f) { worst_sv = std::max(worst_sv, std::fabs(g.smallest - stable)); }
        std::printf("  %7.2f° %10.5f %11.7f %11.7f %10.7f %12.1f\n",
                    static_cast<double>(p),
                    static_cast<double>(engine::determinant(j)),
                    static_cast<double>(naive), static_cast<double>(stable),
                    static_cast<double>(g.smallest), static_cast<double>(1.0f / stable));
    }
    std::printf("  stable closed form vs brute-force sweep (pitch <= 89°): worst |error| %.3e\n",
                static_cast<double>(worst_sv));
    check("sigma_min = |cos p| / sqrt(1 + |sin p|), independently confirmed", worst_sv < 2e-3f);

    // C.2b The two spellings, side by side, where it matters. The naive one is
    //      not merely less accurate — it reaches exactly zero while the pose is
    //      still 0.01° away from lock, and zero is the value every guard tests.
    {
        const float sp = std::fabs(std::sin(rad(89.99f)));
        const float naive = std::sqrt(std::max(0.0f, 1.0f - sp));
        const float stable = std::fabs(std::cos(rad(89.99f))) / std::sqrt(1.0f + sp);
        std::printf("  at pitch 89.99°: naive %.9f (1/x = %.1f), stable %.9f (1/x = %.1f)\n",
                    static_cast<double>(naive),
                    static_cast<double>(naive > 0.0f ? 1.0f / naive : 1.0f / 0.0f),
                    static_cast<double>(stable), static_cast<double>(1.0f / stable));
        check("the naive spelling collapses to zero 0.01° early", naive == 0.0f);
        check("the stable spelling does not", stable > 1e-5f);
    }

    // C.3  The shape of J'J, which is where the closed forms come from. Predicted
    //      to be identity with -sin(pitch) in the two corners.
    {
        const euler_angles e{rad(41.0f), rad(63.0f), rad(-13.0f)};
        const mat3 j = engine::euler_rate_jacobian(e);
        const mat3 jtj = engine::transpose(j) * j;
        mat3 predicted = mat3::identity();
        const float sp = std::sin(e.pitch);
        predicted.c2.x = -sp;
        predicted.c0.z = -sp;
        const float d = max_element_diff(jtj, predicted);
        std::printf("  JtJ vs predicted [[1,0,-sp],[0,1,0],[-sp,0,1]]: worst diff %.3e\n",
                    static_cast<double>(d));
        check("JtJ has the predicted form", d < 1e-6f);
    }

    // C.4  The algebraic collapse. At pitch = +90 the matrix depends on yaw and
    //      roll only through their DIFFERENCE, so moving both together is a knob
    //      connected to nothing. At pitch = -90 it is their sum.
    {
        const mat3 a = engine::rotation_from_euler({rad(20.0f), rad(90.0f), rad(-35.0f)});
        const mat3 b = engine::rotation_from_euler({rad(20.0f + 47.0f), rad(90.0f),
                                                    rad(-35.0f + 47.0f)});
        const float d = deg(engine::angle_between_rotations(a, b));
        std::printf("  at pitch +90: yaw and roll both +47° -> %.4f° of actual motion\n",
                    static_cast<double>(d));
        check("at lock, yaw and roll are one knob (difference)", d < 0.01f);

        const mat3 c = engine::rotation_from_euler({rad(20.0f), rad(-90.0f), rad(-35.0f)});
        const mat3 dd = engine::rotation_from_euler({rad(20.0f + 47.0f), rad(-90.0f),
                                                     rad(-35.0f - 47.0f)});
        const float d2 = deg(engine::angle_between_rotations(c, dd));
        std::printf("  at pitch -90: yaw +47°, roll -47°  -> %.4f° of actual motion\n",
                    static_cast<double>(d2));
        check("at lock, yaw and roll are one knob (sum)", d2 < 0.01f);
    }

    // C.4 CONTROL. Away from lock the same joint move must produce real motion,
    //     or the two zeros above are a property of the test and not of the pose.
    {
        const mat3 a = engine::rotation_from_euler({rad(20.0f), rad(0.0f), rad(-35.0f)});
        const mat3 b = engine::rotation_from_euler({rad(67.0f), rad(0.0f), rad(12.0f)});
        const float d = deg(engine::angle_between_rotations(a, b));
        std::printf("  CONTROL: the same joint move at pitch 0 -> %.2f° of motion "
                    "(two orthogonal 47° turns would give %.2f°)\n",
                    static_cast<double>(d), static_cast<double>(47.0f * std::sqrt(2.0f)));
        check("CONTROL away from lock the knobs are independent", d > 60.0f);
    }

    // C.5  What it costs to steer near lock: the rates needed for 1 rad/s of body
    //      turn in the weakest direction are 1 / sigma_min.
    {
        std::printf("  rates needed for 1 rad/s about the weakest axis:\n");
        for (float p : {80.0f, 89.0f, 89.9f, 89.99f})
        {
            const float sp = std::fabs(std::sin(rad(p)));
            const float sigma_min = std::fabs(std::cos(rad(p))) / std::sqrt(1.0f + sp);
            std::printf("      pitch %6.2f°  ->  %9.1f rad/s  (%.0f deg/s)\n",
                        static_cast<double>(p), static_cast<double>(1.0f / sigma_min),
                        static_cast<double>(deg(1.0f / sigma_min)));
        }
    }
}

// ---------------------------------------------------------------------------
// D — the extraction
// ---------------------------------------------------------------------------

void section_d()
{
    std::printf("\n== D. Extraction: what the degenerate branch keeps and what it loses ==\n");

    // D.1  Round trip on the strip where the inverse is a true inverse.
    {
        float worst_matrix = 0.0f;
        float worst_angle = 0.0f;
        for (int i = 0; i < 37; ++i)
        {
            for (int j = 0; j < 35; ++j)
            {
                for (int k = 0; k < 37; ++k)
                {
                    const euler_angles e{rad(-180.0f + 10.0f * static_cast<float>(i)),
                                         rad(-85.0f + 5.0f * static_cast<float>(j)),
                                         rad(-180.0f + 10.0f * static_cast<float>(k))};
                    const mat3 r = engine::rotation_from_euler(e);
                    const engine::euler_extraction x = engine::euler_from_rotation(r);
                    const mat3 back = engine::rotation_from_euler(x.angles);
                    worst_matrix = std::max(worst_matrix, max_element_diff(r, back));
                    worst_angle = std::max(worst_angle,
                                           deg(engine::angle_between_rotations(r, back)));
                }
            }
        }
        std::printf("  47,915 round trips, |pitch| <= 85°: worst element %.3e, worst angle %.2e°\n",
                    static_cast<double>(worst_matrix), static_cast<double>(worst_angle));
        check("the extraction inverts the construction", worst_angle < 0.01f);
    }

    // D.2  At lock. The MATRIX still round-trips; the ANGLES do not. That split is
    //      the whole content of `degenerate`, so both numbers are printed.
    {
        const euler_angles e{rad(20.0f), rad(90.0f), rad(-35.0f)};
        const mat3 r = engine::rotation_from_euler(e);
        const engine::euler_extraction x = engine::euler_from_rotation(r);
        const mat3 back = engine::rotation_from_euler(x.angles);
        std::printf("  in: yaw %+7.2f pitch %+7.2f roll %+7.2f\n",
                    static_cast<double>(deg(e.yaw)), static_cast<double>(deg(e.pitch)),
                    static_cast<double>(deg(e.roll)));
        std::printf("  out: yaw %+7.2f pitch %+7.2f roll %+7.2f  (cos_pitch %.3e, degenerate %s)\n",
                    static_cast<double>(deg(x.angles.yaw)), static_cast<double>(deg(x.angles.pitch)),
                    static_cast<double>(deg(x.angles.roll)),
                    static_cast<double>(x.cos_pitch), x.degenerate ? "YES" : "no");
        std::printf("  matrix recovered to %.3e°, but yaw moved by %.2f° and roll by %.2f°\n",
                    static_cast<double>(deg(engine::angle_between_rotations(r, back))),
                    static_cast<double>(deg(x.angles.yaw) - deg(e.yaw)),
                    static_cast<double>(deg(x.angles.roll) - deg(e.roll)));
        check("the degenerate branch reports itself", x.degenerate);
        check("the degenerate branch still reproduces the rotation",
              deg(engine::angle_between_rotations(r, back)) < 0.01f);
        check("yaw absorbed the whole combination",
              std::fabs(x.angles.roll) < 1e-6f
                  && std::fabs(deg(x.angles.yaw) - (20.0f - (-35.0f))) < 0.01f);
    }

    // D.3  Where the threshold belongs, and the first answer being wrong.
    //
    //      The argument written into `k_euler_lock_epsilon` is that a float's
    //      ~1e-7 of error in a matrix entry reaches the recovered yaw divided by
    //      cos(pitch). Measuring it on a matrix built by `rotation_from_euler`
    //      showed NO amplification at all, at any pitch — and that measurement is
    //      correct and does not refute the argument. Float error is RELATIVE, and
    //      the two entries `atan2` reads are themselves proportional to
    //      cos(pitch), so their error shrinks with them and the ratio survives.
    //
    //      That is a property of a freshly-built matrix and of nothing else. A
    //      rotation that has come down a transform hierarchy, or out of a file,
    //      or through a few hundred compositions carries ABSOLUTE error of about
    //      1e-7 spread over all nine entries — and then the division is real. So
    //      the table has two columns, and only the second one is the case the
    //      threshold exists for. The perturbation is 200 trials of +/-1e-7 per
    //      entry, worst case reported.
    std::printf("  %11s %12s %13s %14s %9s\n",
                "90 - pitch", "cos_pitch", "clean err", "perturbed err", "flagged");
    unsigned seed = 12345u;
    auto noise = [&seed] {
        seed = seed * 1664525u + 1013904223u;
        return (static_cast<float>(seed >> 8) / static_cast<float>(1u << 24)) * 2.0f - 1.0f;
    };
    for (float away : {1.0f, 0.1f, 0.02f, 0.0057f, 0.002f, 0.0005f})
    {
        const euler_angles e{rad(20.0f), rad(90.0f - away), rad(-35.0f)};
        const mat3 r = engine::rotation_from_euler(e);
        const engine::euler_extraction x = engine::euler_from_rotation(r);

        float worst_perturbed = 0.0f;
        int perturbed_usable = 0;
        for (int trial = 0; trial < 200; ++trial)
        {
            mat3 n = r;
            n.c0.x += noise() * 1e-7f; n.c0.y += noise() * 1e-7f; n.c0.z += noise() * 1e-7f;
            n.c1.x += noise() * 1e-7f; n.c1.y += noise() * 1e-7f; n.c1.z += noise() * 1e-7f;
            n.c2.x += noise() * 1e-7f; n.c2.y += noise() * 1e-7f; n.c2.z += noise() * 1e-7f;
            const engine::euler_extraction y = engine::euler_from_rotation(n);
            if (y.degenerate) { continue; }
            ++perturbed_usable;
            worst_perturbed = std::max(worst_perturbed, std::fabs(deg(y.angles.yaw) - 20.0f));
        }

        // A ZERO FROM A SKIPPED LOOP IS NOT A ZERO ERROR. Every trial below the
        // threshold reports `degenerate` and is skipped, which would leave the
        // worst at its initial 0.0 and print the best-looking number in the table
        // at the worst pose in it. Same shape as 6.16's `identical=YES` on two
        // failed reads. Print the count, and print a dash when there is nothing.
        char clean_cell[32];
        char perturbed_cell[32];
        if (x.degenerate) { std::snprintf(clean_cell, sizeof clean_cell, "%13s", "(in yaw)"); }
        else { std::snprintf(clean_cell, sizeof clean_cell, "%12.5f°",
                             static_cast<double>(std::fabs(deg(x.angles.yaw) - 20.0f))); }
        if (perturbed_usable == 0) { std::snprintf(perturbed_cell, sizeof perturbed_cell,
                                                   "%14s", "(all flagged)"); }
        else { std::snprintf(perturbed_cell, sizeof perturbed_cell, "%13.5f°",
                             static_cast<double>(worst_perturbed)); }
        std::printf("  %10.4f° %12.3e %s %s %9s\n",
                    static_cast<double>(away), static_cast<double>(x.cos_pitch),
                    clean_cell, perturbed_cell, x.degenerate ? "YES" : "no");
    }
    std::printf("  the perturbed column is (1.2e-7 rad / cos_pitch) in degrees; at the\n"
                "  threshold cos_pitch = 1e-4 that is 0.07°, which is where it was put.\n");

    // D.4  Outside the strip the inverse chooses. Pitch 100° comes back as a
    //      different triple naming the same rotation — not an error, and worth
    //      demonstrating so nobody files it as one.
    {
        const euler_angles e{rad(10.0f), rad(100.0f), rad(0.0f)};
        const mat3 r = engine::rotation_from_euler(e);
        const engine::euler_extraction x = engine::euler_from_rotation(r);
        std::printf("  pitch 100° comes back as yaw %+.1f pitch %+.1f roll %+.1f — same rotation, %.3e° apart\n",
                    static_cast<double>(deg(x.angles.yaw)), static_cast<double>(deg(x.angles.pitch)),
                    static_cast<double>(deg(x.angles.roll)),
                    static_cast<double>(deg(engine::angle_between_rotations(
                        r, engine::rotation_from_euler(x.angles)))));
        check("a different triple, the same rotation",
              deg(engine::angle_between_rotations(r, engine::rotation_from_euler(x.angles))) < 0.01f
                  && std::fabs(deg(x.angles.pitch) - 100.0f) > 10.0f);
    }
}

// ---------------------------------------------------------------------------
// E — interpolation
// ---------------------------------------------------------------------------

void section_e()
{
    std::printf("\n== E. Interpolation: the indictment ==\n");

    constexpr int k_steps = 2048;

    // E.1  THE CONTROL FIRST, deliberately. Two poses differing only in yaw: the
    //      angle lerp IS the geodesic, so the excess must be zero and the speed
    //      ratio exactly one. An instrument that cannot report "no detour" cannot
    //      be trusted when it reports one.
    {
        const euler_angles a{rad(-40.0f), rad(25.0f), rad(15.0f)};
        const euler_angles b{rad(70.0f), rad(25.0f), rad(15.0f)};
        const path_stats s = measure_path(
            [&](float t) { return engine::rotation_from_euler(lerp_angles(a, b, t)); }, k_steps);
        const float excess = 100.0f * (s.length / s.geodesic - 1.0f);
        std::printf("  CONTROL yaw-only: path %.4f rad, geodesic %.4f rad, excess %+.3f%%, "
                    "speed ratio %.4f\n",
                    static_cast<double>(s.length), static_cast<double>(s.geodesic),
                    static_cast<double>(excess),
                    static_cast<double>(s.fastest / s.slowest));
        check("CONTROL a one-axis lerp is already the geodesic",
              std::fabs(excess) < 0.05f && s.fastest / s.slowest < 1.01f);
    }

    // E.2  A generic pair. Three angles moving at once, nowhere near lock.
    {
        const euler_angles a{rad(-70.0f), rad(-35.0f), rad(20.0f)};
        const euler_angles b{rad(85.0f), rad(55.0f), rad(-60.0f)};
        const path_stats s = measure_path(
            [&](float t) { return engine::rotation_from_euler(lerp_angles(a, b, t)); }, k_steps);
        const float excess = 100.0f * (s.length / s.geodesic - 1.0f);
        std::printf("  generic pair: path %.4f rad (%.2f°), geodesic %.4f rad (%.2f°)\n",
                    static_cast<double>(s.length), static_cast<double>(deg(s.length)),
                    static_cast<double>(s.geodesic), static_cast<double>(deg(s.geodesic)));
        std::printf("      excess turning %+.2f%% (%.2f° of detour), speed varies by %.2fx\n",
                    static_cast<double>(excess),
                    static_cast<double>(deg(s.length - s.geodesic)),
                    static_cast<double>(s.fastest / s.slowest));
        check("an Euler lerp takes the long way", excess > 1.0f);
        check("an Euler lerp does not hold its speed", s.fastest / s.slowest > 1.2f);
    }

    // E.3  Near lock, where it goes from wrong to gross — and THE FIRST DRAFT OF
    //      THIS CHECK MEASURED NOTHING. It swept yaw alone at pitch 88° and
    //      reported a ratio of exactly 1.00x, which is not a failure of the
    //      instrument: a path that turns ONE Euler knob is a rotation about one
    //      fixed axis, and that is a geodesic. Every single-knob Euler path is
    //      already optimal. The detour needs at least two knobs moving, which is
    //      worth knowing before you go looking for it.
    //
    //      So: the same three deltas, run at four heights, with the pitch band
    //      sliding away from 90°. The deltas never change; only how close the
    //      path comes to the singularity does.
    {
        std::printf("  the same three angle deltas, at four distances from lock:\n");
        std::printf("  %26s %10s %10s %10s %8s\n",
                    "pitch band", "path", "geodesic", "excess", "speed");
        float excess_near = 0.0f;
        float excess_far = 0.0f;
        const float bands[4] = {87.0f, 47.0f, 20.0f, 0.0f};
        for (int i = 0; i < 4; ++i)
        {
            const float top = bands[i];
            const euler_angles a{rad(-60.0f), rad(top), rad(40.0f)};
            const euler_angles b{rad(120.0f), rad(top - 57.0f), rad(-70.0f)};
            const path_stats s2 = measure_path(
                [&](float t) { return engine::rotation_from_euler(lerp_angles(a, b, t)); },
                k_steps);
            const float excess = 100.0f * (s2.length / s2.geodesic - 1.0f);
            if (i == 0) { excess_near = excess; }
            if (i == 3) { excess_far = excess; }
            char label[40];
            std::snprintf(label, sizeof label, "%.0f° -> %.0f°%s", static_cast<double>(top),
                          static_cast<double>(top - 57.0f), (i == 0) ? "  (near lock)" : "");
            std::printf("  %26s %9.2f° %9.2f° %9.1f%% %7.2fx\n", label,
                        static_cast<double>(deg(s2.length)),
                        static_cast<double>(deg(s2.geodesic)),
                        static_cast<double>(excess),
                        static_cast<double>(s2.fastest / s2.slowest));
        }
        check("running the same move near lock costs far more turning",
              excess_near > 3.0f * excess_far);
        check("CONTROL the same move away from lock still detours, but modestly",
              excess_far > 5.0f && excess_far < 40.0f);
    }

    // E.4  The wrap bug, which is a fact about the numbers and not about rotation.
    //      Yaw 170° to -170° is a 20° turn across the back; a raw lerp of the
    //      floats performs 340° in the other direction.
    {
        const euler_angles a{rad(170.0f), 0.0f, 0.0f};
        const euler_angles b{rad(-170.0f), 0.0f, 0.0f};
        const path_stats naive = measure_path(
            [&](float t) { return engine::rotation_from_euler(lerp_angles(a, b, t)); }, k_steps);
        const path_stats fixed = measure_path(
            [&](float t) { return engine::rotation_from_euler(lerp_angles_shortest(a, b, t)); },
            k_steps);
        std::printf("  yaw 170° -> -170°: raw lerp turns %.1f°, shortest-delta lerp turns %.1f° "
                    "(the journey is %.1f°)\n",
                    static_cast<double>(deg(naive.length)), static_cast<double>(deg(fixed.length)),
                    static_cast<double>(deg(naive.geodesic)));
        check("the raw lerp goes the long way round", deg(naive.length) > 300.0f);
        check("shortest_angle_delta fixes it", std::fabs(deg(fixed.length) - 20.0f) < 0.5f);
    }
}

// ---------------------------------------------------------------------------
// F — the instrument
// ---------------------------------------------------------------------------

void section_f()
{
    std::printf("\n== F. Can the metric measure what section E asked of it? ==\n");

    // F.1  Against a known answer: a pure turn of theta about one axis must
    //      measure theta. Both forms, so the comparison is like for like.
    std::printf("  %12s %14s %14s %12s %12s\n",
                "true angle", "atan2 form", "trace form", "atan2 err", "trace err");
    float worst_atan2 = 0.0f;
    float worst_trace = 0.0f;
    for (float a : {170.0f, 90.0f, 10.0f, 1.0f, 0.23f, 0.05f, 0.004f})
    {
        const mat3 base = engine::rotation_from_euler({rad(31.0f), rad(-17.0f), rad(52.0f)});
        const mat3 turned = base * engine::rotation_x(rad(a));
        const float m1 = deg(engine::angle_between_rotations(base, turned));
        const float m2 = deg(engine::angle_between_rotations_by_trace(base, turned));
        const float e1 = std::fabs(m1 - a) / a;
        const float e2 = std::fabs(m2 - a) / a;
        worst_atan2 = std::max(worst_atan2, e1);
        worst_trace = std::max(worst_trace, e2);
        std::printf("  %11.4f° %13.6f° %13.6f° %11.2e %11.2e\n",
                    static_cast<double>(a), static_cast<double>(m1), static_cast<double>(m2),
                    static_cast<double>(e1), static_cast<double>(e2));
    }
    std::printf("  worst relative error: atan2 form %.2e, trace form %.2e (%.0fx worse)\n",
                static_cast<double>(worst_atan2), static_cast<double>(worst_trace),
                static_cast<double>(worst_trace / std::max(worst_atan2, 1e-12f)));
    check("the metric is accurate at the step sizes section E uses", worst_atan2 < 1e-3f);
    check("the textbook form is not", worst_trace > 1e-2f);

    // F.2  A path made of 2,048 steps summing to a known total. If the metric
    //      biased small steps, this would not come out.
    {
        const float total = rad(120.0f);
        const mat3 base = engine::rotation_from_euler({rad(31.0f), rad(-17.0f), rad(52.0f)});
        float sum = 0.0f;
        mat3 previous = base;
        for (int i = 1; i <= 2048; ++i)
        {
            const mat3 current = base * engine::rotation_x(total * static_cast<float>(i) / 2048.0f);
            sum += engine::angle_between_rotations(previous, current);
            previous = current;
        }
        std::printf("  2,048 steps summing to a known 120.00°: measured %.4f° (error %.2e)\n",
                    static_cast<double>(deg(sum)),
                    static_cast<double>(std::fabs(deg(sum) - 120.0f)));
        check("summing 2,048 small steps is unbiased", std::fabs(deg(sum) - 120.0f) < 0.05f);
    }
}

}  // namespace

int main()
{
    std::printf("verify_71 — Lesson 7.1, Euler angles and their pathologies\n");
    section_a();
    section_b();
    section_c();
    section_d();
    section_e();
    section_f();
    std::printf("\n%d/%d checks passed\n", checks_passed, checks_run);
    return checks_passed == checks_run ? 0 : 1;
}
