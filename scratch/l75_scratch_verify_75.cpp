// scratch/verify_75.cpp — every number Lesson 7.5 prints, measured rather than
// asserted.
//
// Build and run:  sh scratch/build_verify_75.sh
//
// Nine sections, and the order is the lesson's:
//
//   A  the three steps are the SAME three steps, in a third notation
//   B  constant angular speed, and one fixed axis
//   C  the path: slerp's geodesic, and nlerp's identical one
//   D  the schedule: sec^2(Omega/2), and the cap the double cover puts on it
//   E  the shortest-arc sign, and what half of all pairs do without it
//   F  the midpoint is exact, and the instrument that says otherwise
//   G  the textbook trig form, and where it is worse than ours
//   H  what it costs
//   I  the storage swap: decomposition, round trip, and the sheared node
//
// EVERY SECTION CARRIES A CONTROL, for the reason 7.1 states and 7.2, 7.3 and
// 7.4 repeat: a check whose degenerate case is a pass is not a check. The
// sharpest ones here are in E and F. E's claim is that one comparison buys the
// short way, so its control is the same function with that comparison deleted,
// which must travel MORE than 180 degrees rather than merely differ. F's claim
// is that nlerp and slerp agree exactly at t = 0.5, so its control is the same
// comparison made with `acos`, which must report a gap that is not there.
//
// TIMING RULES, inherited from 7.2, 7.3 and 7.4 and obeyed here without
// exception:
//   - consume the WHOLE result, and never overwrite the accumulator
//   - VARY the input with the repetition counter
//   - BEST OF THREE, never the mean
//   - ONE WARM-UP before the timed runs
//   - divide by the operation count and ask whether the answer is physically
//     possible before believing it
//
// OUTPUT WIDTH IS A CONSTRAINT, not a preference. The lesson quotes this
// program's transcript in <pre> blocks that scroll and never wrap, and the fold
// is at about 66 characters. Nothing below exceeds 66.

#include <engine/math/complex.hpp>
#include <engine/math/euler.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/rotation.hpp>
#include <engine/math/transform.hpp>
#include <engine/math/vec3.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>

using engine::complex;
using engine::mat3;
using engine::mat4;
using engine::quat;
using engine::transform;
using engine::vec3;

namespace {

constexpr float k_pi = std::numbers::pi_v<float>;

int checks_run = 0;
int checks_passed = 0;
double sink = 0.0;

float deg(float radians) { return radians * 180.0f / k_pi; }
float rad(float degrees) { return degrees * k_pi / 180.0f; }

void check(const char* name, bool ok)
{
    ++checks_run;
    if (ok) { ++checks_passed; }
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
}

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

/// The four-component chord between two quaternions, taking the nearer of the
/// two representatives.
///
/// **THE INSTRUMENT, and section F is about why it is this one and not `acos`.**
/// A chord is linear in the separation; `acos` of a dot product near 1 is not,
/// and its noise floor at `float` is about 3.5e-4 radians.
float chord(quat a, quat b)
{
    const quat n = engine::nearest(a, b);
    const float dw = a.w - n.w;
    const vec3 dv = a.v - n.v;
    return std::sqrt(dw * dw + engine::dot(dv, dv));
}

/// A deterministic spread of unit quaternions. Same generator as 7.4's, so the
/// two harnesses sample the same orientations and their numbers can be compared.
quat sample_quat(std::uint32_t i)
{
    std::uint32_t s = i * 2654435761u + 1013904223u;
    auto next = [&s]() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return static_cast<float>(s & 0xFFFFFFu) / static_cast<float>(0xFFFFFF);
    };
    const float u1 = next();
    const float u2 = next();
    const float u3 = next();
    // Shoemake's uniform sampling of the rotation group. Uniform on the sphere
    // matters here: section D's claim is about the WORST case over all pairs,
    // and a generator that clusters would understate it.
    const float r1 = std::sqrt(1.0f - u1);
    const float r2 = std::sqrt(u1);
    return {r1 * std::sin(2.0f * k_pi * u2),
            {r1 * std::cos(2.0f * k_pi * u2),
             r2 * std::sin(2.0f * k_pi * u3),
             r2 * std::cos(2.0f * k_pi * u3)}};
}

/// How far `q` has turned from `a`, in [0, 2pi) — distance travelled, not
/// separation. `angle_between` takes an absolute value and caps at pi, which is
/// the wrong instrument for "did this go the long way round".
float travelled(quat a, quat q)
{
    return engine::axis_angle_from_quat(engine::conjugate(a) * q).value.angle;
}

/// Total turning along a path, at a fixed step count. 7.1's instrument, reused
/// unchanged so that this lesson's percentages are comparable with that one's.
template <typename Fn>
float path_length(Fn pose_at, int steps)
{
    float total = 0.0f;
    quat previous = pose_at(0.0f);
    for (int i = 1; i <= steps; ++i)
    {
        const quat current = pose_at(static_cast<float>(i) / static_cast<float>(steps));
        total += engine::angle_between(previous, current);
        previous = current;
    }
    return total;
}

// ===========================================================================
// A — the three steps are the SAME three steps
// ===========================================================================
void section_a()
{
    std::printf("\nA. slerp(a,b,t) = a (a^-1 b)^t, for the third time\n");

    const quat a = engine::quat_from_euler({rad(-70.0f), rad(-35.0f), rad(20.0f)});
    const quat b = engine::quat_from_euler({rad(85.0f), rad(55.0f), rad(-60.0f)});

    check("A.1  t = 0 returns a exactly",
          chord(a, engine::quat_slerp(a, b, 0.0f)) == 0.0f);
    check("A.2  t = 1 returns b (as a rotation)",
          engine::angle_between(b, engine::quat_slerp(a, b, 1.0f)) < 1e-6f);

    // A.3 — the formula, spelled out, against the function.
    float worst_form = 0.0f;
    for (int i = 0; i <= 200; ++i)
    {
        const float t = static_cast<float>(i) / 200.0f;
        const quat spelled = a * engine::quat_pow_unit(
            engine::conjugate(a) * engine::nearest(a, b), t);
        worst_form = std::max(worst_form, chord(spelled, engine::quat_slerp(a, b, t)));
    }
    std::printf("   worst chord, formula vs function: %.3e\n",
                static_cast<double>(worst_form));
    check("A.3  the function IS the formula", worst_form == 0.0f);

    // A.4 — against Lesson 7.2's `rotation_slerp`, which works on mat3 in a
    // completely different notation and has never heard of a quaternion.
    const mat3 ma = engine::mat3_from_quat(a);
    const mat3 mb = engine::mat3_from_quat(b);
    float worst_mat = 0.0f;
    for (int i = 0; i <= 200; ++i)
    {
        const float t = static_cast<float>(i) / 200.0f;
        worst_mat = std::max(worst_mat,
                             max_element_diff(engine::rotation_slerp(ma, mb, t),
                                              engine::mat3_from_quat(
                                                  engine::quat_slerp(a, b, t))));
    }
    std::printf("   worst entry, 7.2's mat3 slerp vs ours: %.3e\n",
                static_cast<double>(worst_mat));
    check("A.4  agrees with Lesson 7.2's rotation_slerp", worst_mat < 2e-6f);

    // A.5 — against Lesson 7.3's `complex_slerp`, on a rotation confined to one
    // plane, where the two are describing literally the same journey.
    float worst_plane = 0.0f;
    for (int i = 0; i <= 200; ++i)
    {
        const float t = static_cast<float>(i) / 200.0f;
        const quat qz = engine::quat_slerp(engine::quat_z(rad(20.0f)),
                                           engine::quat_z(rad(140.0f)), t);
        const complex cz = engine::complex_slerp(engine::complex_from_angle(rad(20.0f)),
                                                 engine::complex_from_angle(rad(140.0f)), t);
        const float from_quat = travelled(engine::quat_z(rad(20.0f)), qz);
        const float from_complex = engine::angle_from_complex(
            engine::conjugate(engine::complex_from_angle(rad(20.0f))) * cz);
        worst_plane = std::max(worst_plane, std::fabs(from_quat - from_complex));
    }
    std::printf("   worst angle, 7.3's complex slerp vs ours: %.3e rad\n",
                static_cast<double>(worst_plane));
    check("A.5  agrees with Lesson 7.3's complex_slerp", worst_plane < 1e-5f);

    // A.6 — CONTROL. The thing a matrix CANNOT do, which is why the storage had
    // to change: average two rotation matrices entrywise and the answer is not a
    // rotation. Measured as a determinant, which is 1 for every rotation.
    const mat3 mixed{(ma.c0 + mb.c0) * 0.5f, (ma.c1 + mb.c1) * 0.5f,
                     (ma.c2 + mb.c2) * 0.5f};
    const float det = engine::determinant(mixed);
    std::printf("   CONTROL det of the entrywise average: %.5f\n",
                static_cast<double>(det));
    check("A.6  CONTROL: averaging two matrices is not a rotation",
          std::fabs(det - 1.0f) > 0.1f);
}

// ===========================================================================
// B — constant angular speed, one fixed axis
// ===========================================================================
void section_b()
{
    std::printf("\nB. constant speed, and one line that never moves\n");

    const quat a = engine::quat_from_euler({rad(-70.0f), rad(-35.0f), rad(20.0f)});
    const quat b = engine::quat_from_euler({rad(85.0f), rad(55.0f), rad(-60.0f)});
    const float arc = engine::angle_between(a, b);
    std::printf("   the arc: %.4f deg of rotation\n", static_cast<double>(deg(arc)));

    float worst_speed = 0.0f;
    for (int i = 0; i <= 1000; ++i)
    {
        const float t = static_cast<float>(i) / 1000.0f;
        const float turned = travelled(a, engine::quat_slerp(a, b, t));
        worst_speed = std::max(worst_speed, std::fabs(turned - t * arc));
    }
    std::printf("   worst |turned - t*arc| over 1001 steps: %.3e rad\n",
                static_cast<double>(worst_speed));
    check("B.1  slerp covers t*arc, linearly in t", worst_speed < 1e-5f);

    // B.2 — the axis is the same line at every t. That is not an extra property:
    // `quat_pow_unit` scales an angle and leaves `v`'s direction alone, so the
    // difference quaternion's axis cannot move. Measured anyway.
    const vec3 axis0 = engine::axis_angle_from_quat(
        engine::conjugate(a) * engine::quat_slerp(a, b, 0.5f)).value.axis;
    float worst_axis = 0.0f;
    float worst_axis_early = 0.0f;
    float worst_cost = 0.0f;
    for (int i = 1; i <= 1000; ++i)
    {
        const float t = static_cast<float>(i) / 1000.0f;
        const quat diff = engine::conjugate(a) * engine::quat_slerp(a, b, t);
        const vec3 ax = engine::axis_angle_from_quat(diff).value.axis;
        // THE CHORD, NOT `acos` OF THE DOT PRODUCT. 7.4 §G.3's finding: an axis
        // error of 5.7e-05 rad puts the dot at 1 - 1.6e-09, which rounds to
        // exactly 1.0f, and acos of exactly 1 is exactly 0 — a blind instrument
        // that prints a perfect score in every row.
        const float err = engine::length(ax - axis0);
        worst_axis = std::max(worst_axis, err);
        if (t >= 0.05f) { worst_axis_early = std::max(worst_axis_early, err); }
        // WHAT THE ERROR COSTS, which is 7.2 §8's formula and is the reason the
        // first column below is allowed to be worse than the second. An axis
        // error of phi on a turn of theta moves the pose by 2 sin(theta/2) phi,
        // which goes to ZERO exactly where the axis becomes unrecoverable.
        worst_cost = std::max(worst_cost,
                              2.0f * std::sin(t * arc * 0.5f) * err);
    }
    std::printf("   worst axis chord, whole blend:  %.3e\n",
                static_cast<double>(worst_axis));
    std::printf("   worst axis chord, t >= 0.05:    %.3e\n",
                static_cast<double>(worst_axis_early));
    std::printf("   worst 2 sin(theta/2) * error:   %.3e rad of pose\n",
                static_cast<double>(worst_cost));
    check("B.2  one axis, unmoved, once the turn exists",
          worst_axis_early < 1e-5f);
    check("B.3  and the wobble near t=0 costs no orientation",
          worst_cost < 1e-6f);

    // B.3 — CONTROL. nlerp is on the same path and is NOT linear in t.
    float worst_nlerp = 0.0f;
    for (int i = 0; i <= 1000; ++i)
    {
        const float t = static_cast<float>(i) / 1000.0f;
        const float turned = travelled(a, engine::quat_nlerp(a, b, t));
        worst_nlerp = std::max(worst_nlerp, std::fabs(turned - t * arc));
    }
    std::printf("   CONTROL worst nlerp deviation: %.4f deg\n",
                static_cast<double>(deg(worst_nlerp)));
    check("B.4  CONTROL: nlerp is not linear in t", deg(worst_nlerp) > 0.5f);
}

// ===========================================================================
// C — the path
// ===========================================================================
void section_c()
{
    std::printf("\nC. the path: two functions, one curve\n");

    const engine::euler_angles ea{rad(-70.0f), rad(-35.0f), rad(20.0f)};
    const engine::euler_angles eb{rad(85.0f), rad(55.0f), rad(-60.0f)};
    const quat a = engine::quat_from_euler(ea);
    const quat b = engine::quat_from_euler(eb);
    const float geodesic = engine::angle_between(a, b);

    const float slerp_path = path_length(
        [&](float t) { return engine::quat_slerp(a, b, t); }, 4096);
    const float nlerp_path = path_length(
        [&](float t) { return engine::quat_nlerp(a, b, t); }, 4096);
    const float euler_path = path_length(
        [&](float t) {
            return engine::quat_from_euler({ea.yaw + (eb.yaw - ea.yaw) * t,
                                            ea.pitch + (eb.pitch - ea.pitch) * t,
                                            ea.roll + (eb.roll - ea.roll) * t});
        }, 4096);

    auto excess = [geodesic](float path) { return 100.0f * (path / geodesic - 1.0f); };
    std::printf("   geodesic  %8.4f deg\n", static_cast<double>(deg(geodesic)));
    std::printf("   slerp     %8.4f deg   excess %+7.2f%%\n",
                static_cast<double>(deg(slerp_path)),
                static_cast<double>(excess(slerp_path)));
    std::printf("   nlerp     %8.4f deg   excess %+7.2f%%\n",
                static_cast<double>(deg(nlerp_path)),
                static_cast<double>(excess(nlerp_path)));
    std::printf("   euler     %8.4f deg   excess %+7.2f%%\n",
                static_cast<double>(deg(euler_path)),
                static_cast<double>(excess(euler_path)));

    check("C.1  slerp wastes no turning", std::fabs(excess(slerp_path)) < 0.05f);
    check("C.2  nlerp wastes no turning EITHER", std::fabs(excess(nlerp_path)) < 0.05f);
    check("C.3  CONTROL: Euler lerp does", excess(euler_path) > 1.0f);

    // C.4 — WHY nlerp cannot leave the curve, measured rather than argued. Every
    // point it returns is a unit multiple of (1-t)a + t*b, so it lies in the
    // 2-plane spanned by a and b. Project each sample onto that plane's
    // complement and the residual is the departure.
    const quat bb = engine::nearest(a, b);
    const float ab = a.w * bb.w + engine::dot(a.v, bb.v);
    float worst_off_plane = 0.0f;
    for (int i = 0; i <= 500; ++i)
    {
        const float t = static_cast<float>(i) / 500.0f;
        const quat p = engine::quat_nlerp(a, bb, t);
        // Gram-Schmidt the plane, then measure what is left of p outside it.
        const float pa = p.w * a.w + engine::dot(p.v, a.v);
        const float pb = p.w * bb.w + engine::dot(p.v, bb.v);
        const float denom = 1.0f - ab * ab;
        const float ca = (pa - ab * pb) / denom;
        const float cb = (pb - ab * pa) / denom;
        const quat in_plane{ca * a.w + cb * bb.w, a.v * ca + bb.v * cb};
        worst_off_plane = std::max(worst_off_plane,
                                   std::sqrt(engine::length_squared(
                                       quat{p.w - in_plane.w, p.v - in_plane.v})));
    }
    std::printf("   worst departure from the a-b plane: %.3e\n",
                static_cast<double>(worst_off_plane));
    check("C.4  nlerp cannot leave the great circle", worst_off_plane < 1e-5f);
}

// ===========================================================================
// D — the schedule, and the cap
// ===========================================================================
void section_d()
{
    std::printf("\nD. the schedule: sec^2(Omega/2), and the cap on it\n");

    // A single axis, so that the arc is exactly the number in the first column.
    const vec3 axis = engine::normalised(vec3{0.31f, 0.86f, -0.41f});
    auto pair_at = [&](float theta) {
        return std::pair<quat, quat>{quat::identity(),
                                     engine::quat_from_axis_angle(axis, theta)};
    };

    std::printf("   rot arc   sphere   worst gap    at t   sec^2(O/2)\n");
    float worst_overall = 0.0f;
    for (const float theta_deg : {30.0f, 60.0f, 90.0f, 120.0f, 150.0f, 180.0f})
    {
        const auto [a, b] = pair_at(rad(theta_deg));
        float gap = 0.0f;
        float at = 0.0f;
        for (int i = 0; i <= 2000; ++i)
        {
            const float t = static_cast<float>(i) / 2000.0f;
            const float g = engine::angle_between(engine::quat_slerp(a, b, t),
                                                  engine::quat_nlerp(a, b, t));
            if (g > gap) { gap = g; at = t; }
        }
        worst_overall = std::max(worst_overall, gap);
        const float half_sphere = rad(theta_deg) * 0.25f;
        const float ratio = 1.0f / (std::cos(half_sphere) * std::cos(half_sphere));
        std::printf("   %6.1f   %6.1f   %8.4f   %.3f   %10.4f\n",
                    static_cast<double>(theta_deg),
                    static_cast<double>(theta_deg * 0.5f),
                    static_cast<double>(deg(gap)), static_cast<double>(at),
                    static_cast<double>(ratio));
    }
    check("D.1  the gap grows with the arc", worst_overall > rad(8.0f));

    // D.2 — THE CAP, and it is exact. `nearest` forces the four-component dot
    // product non-negative, so the SPHERE arc is at most 90 degrees, so
    // sec^2(Omega/2) is at most sec^2(45) = 2 exactly. In the plane, where there
    // is no double cover to exploit, Omega can reach 180 and the same ratio
    // reaches 13131 (Lesson 7.3 §10.3).
    float worst_sphere = 0.0f;
    float worst_gap = 0.0f;
    for (std::uint32_t i = 0; i < 20000u; ++i)
    {
        const quat a = sample_quat(i * 2u);
        const quat b = sample_quat(i * 2u + 1u);
        const quat n = engine::nearest(a, b);
        const float d = a.w * n.w + engine::dot(a.v, n.v);
        worst_sphere = std::max(worst_sphere, std::acos(std::clamp(d, -1.0f, 1.0f)));
        for (const float t : {0.229f, 0.5f, 0.771f})
        {
            worst_gap = std::max(worst_gap,
                                 engine::angle_between(engine::quat_slerp(a, b, t),
                                                       engine::quat_nlerp(a, b, t)));
        }
    }
    std::printf("   over 20,000 random pairs:\n");
    std::printf("     largest sphere arc  %.4f deg  (bound: 90)\n",
                static_cast<double>(deg(worst_sphere)));
    std::printf("     largest nlerp gap   %.4f deg  (bound: 8.1491)\n",
                static_cast<double>(deg(worst_gap)));
    check("D.2  the sphere arc never exceeds 90 deg",
          deg(worst_sphere) <= 90.0001f);
    check("D.3  the nlerp gap never exceeds 8.15 deg",
          deg(worst_gap) <= 8.1492f);

    // D.4 — CONTROL. Delete the sign choice and the bound goes with it.
    float worst_raw = 0.0f;
    for (std::uint32_t i = 0; i < 20000u; ++i)
    {
        const quat a = sample_quat(i * 2u);
        const quat b = sample_quat(i * 2u + 1u);
        const float d = a.w * b.w + engine::dot(a.v, b.v);
        worst_raw = std::max(worst_raw, std::acos(std::clamp(d, -1.0f, 1.0f)));
    }
    std::printf("   CONTROL without nearest: largest arc %.4f deg\n",
                static_cast<double>(deg(worst_raw)));
    check("D.4  CONTROL: without the sign, the arc exceeds 90",
          deg(worst_raw) > 150.0f);
}

// ===========================================================================
// E — the sign
// ===========================================================================
void section_e()
{
    std::printf("\nE. one comparison, and what half of all pairs do without it\n");

    int needed_flip = 0;
    float worst_short = 0.0f;
    float worst_long = 0.0f;
    for (std::uint32_t i = 0; i < 20000u; ++i)
    {
        const quat a = sample_quat(i * 2u);
        const quat b = sample_quat(i * 2u + 1u);
        if (a.w * b.w + engine::dot(a.v, b.v) < 0.0f) { ++needed_flip; }

        worst_short = std::max(worst_short, travelled(a, engine::quat_slerp(a, b, 1.0f)));

        // The same function with the one comparison removed.
        const quat raw = a * engine::quat_pow_unit(engine::conjugate(a) * b, 1.0f);
        worst_long = std::max(worst_long, travelled(a, raw));
    }
    std::printf("   pairs needing the flip: %d of 20000 (%.1f%%)\n",
                needed_flip, 100.0 * needed_flip / 20000.0);
    std::printf("   longest journey WITH nearest:    %.4f deg\n",
                static_cast<double>(deg(worst_short)));
    std::printf("   longest journey WITHOUT it:      %.4f deg\n",
                static_cast<double>(deg(worst_long)));
    check("E.1  about half of random pairs need the flip",
          needed_flip > 9000 && needed_flip < 11000);
    check("E.2  with nearest, no journey exceeds 180 deg",
          deg(worst_short) <= 180.001f);
    check("E.3  CONTROL: without it, journeys reach 360 deg",
          deg(worst_long) > 350.0f);

    // E.4 — the failure in the form a reader will actually meet it. Two poses one
    // degree apart, signed the other way by an exporter that had no reason not to.
    const quat k0 = engine::quat_from_euler({rad(12.0f), rad(-4.0f), rad(31.0f)});
    const quat k1 = -engine::quat_from_euler({rad(13.0f), rad(-4.0f), rad(31.0f)});
    std::printf("   two keyframes 1 deg apart, opposite signs:\n");
    std::printf("     separation (angle_between):  %.4f deg\n",
                static_cast<double>(deg(engine::angle_between(k0, k1))));
    std::printf("     slerp travels:               %.4f deg\n",
                static_cast<double>(deg(travelled(k0, engine::quat_slerp(k0, k1, 1.0f)))));
    const quat raw_end = k0 * engine::quat_pow_unit(engine::conjugate(k0) * k1, 1.0f);
    std::printf("     CONTROL without nearest:     %.4f deg\n",
                static_cast<double>(deg(travelled(k0, raw_end))));
    check("E.4  CONTROL: one missing sign is a 359 deg spin",
          deg(travelled(k0, raw_end)) > 355.0f);

    // E.5 — nlerp's version of the same failure is worse, because the chord
    // between antipodal representatives passes through the origin.
    float smallest_chord = 1.0f;
    for (int i = 0; i <= 500; ++i)
    {
        const float t = static_cast<float>(i) / 500.0f;
        const quat p{k0.w + (k1.w - k0.w) * t, k0.v + (k1.v - k0.v) * t};
        smallest_chord = std::min(smallest_chord, engine::length(p));
    }
    std::printf("     raw nlerp chord passes within %.5f of zero\n",
                static_cast<double>(smallest_chord));
    check("E.5  CONTROL: the raw chord nearly hits the origin",
          smallest_chord < 0.02f);
}

// ===========================================================================
// F — the midpoint, and a blind instrument
// ===========================================================================
void section_f()
{
    std::printf("\nF. the midpoint is exact, and one instrument says it is not\n");

    const vec3 axis = engine::normalised(vec3{0.31f, 0.86f, -0.41f});
    const quat a = quat::identity();
    const quat b = engine::quat_from_axis_angle(axis, rad(150.0f));

    const quat s_mid = engine::quat_slerp(a, b, 0.5f);
    const quat n_mid = engine::quat_nlerp(a, b, 0.5f);

    const float by_chord = chord(s_mid, n_mid);
    std::printf("   chord between the two midpoints: %.3e\n",
                static_cast<double>(by_chord));
    check("F.1  the midpoints agree, by chord", by_chord < 1e-6f);

    // ---- THE INSTRUMENT, and why Lesson 7.5 rewrote it ---------------------
    //
    // `angle_between` was `2 acos|a.b|` when 7.4 shipped it, which is the formula
    // every reference gives. It is a BLIND INSTRUMENT near zero, for the reason
    // `axis_angle_from_quat` states eleven lines above it and then does not
    // follow: the cosine is flat at 1, so a small separation lands within a few
    // ulps of a dot product of 1 and most of the angle is gone before `acos` is
    // called. The floor is about 2*sqrt(2*eps) = 0.056 deg, and either side of
    // it the reading is EXACTLY zero or a quantised step, never the answer.
    //
    // A path integral is where that stops being cosmetic, because it asks the
    // question thousands of times with a small answer each time and the error is
    // BIASED rather than random. Below: slerp's own geodesic, summed with both
    // metrics, against the separation of its endpoints. One of them fails slerp
    // for not walking the path it is walking.
    const float acos_path = [&] {
        float total = 0.0f;
        quat previous = engine::quat_slerp(a, b, 0.0f);
        for (int i = 1; i <= 4096; ++i)
        {
            const quat current = engine::quat_slerp(a, b, static_cast<float>(i) / 4096.0f);
            total += engine::angle_between_by_cosine(previous, current);
            previous = current;
        }
        return total;
    }();
    const float atan_path = path_length(
        [&](float t) { return engine::quat_slerp(a, b, t); }, 4096);
    const float straight = engine::angle_between(a, b);
    std::printf("   endpoints are %.4f deg apart\n", static_cast<double>(deg(straight)));
    std::printf("   slerp path by 7.4's acos form: %.4f deg  (%+.2f%%)\n",
                static_cast<double>(deg(acos_path)),
                static_cast<double>(100.0f * (acos_path / straight - 1.0f)));
    std::printf("   slerp path by the atan2 form:  %.4f deg  (%+.2f%%)\n",
                static_cast<double>(deg(atan_path)),
                static_cast<double>(100.0f * (atan_path / straight - 1.0f)));
    std::printf("   acos noise floor, 2*sqrt(2*eps): %.4f deg per step\n",
                static_cast<double>(deg(2.0f * std::sqrt(2.0f * 1.1920929e-7f))));
    check("F.2  the atan2 form sums the geodesic to itself",
          std::fabs(atan_path / straight - 1.0f) < 5e-4f);
    check("F.3  CONTROL: the acos form is short by more than 5%",
          acos_path / straight < 0.95f);

    // F.4 — WHY the midpoint is exact, which is a symmetry and not a coincidence:
    // the chord's midpoint is equidistant from both endpoints, and so is the
    // arc's, and on a sphere there is only one such point on the short arc.
    // Checked at seven arcs, because a claim true at one angle is an anecdote.
    float worst_mid = 0.0f;
    for (const float theta : {10.0f, 45.0f, 90.0f, 120.0f, 150.0f, 175.0f, 179.0f})
    {
        const quat q1 = engine::quat_from_axis_angle(axis, rad(theta));
        worst_mid = std::max(worst_mid, chord(engine::quat_slerp(a, q1, 0.5f),
                                              engine::quat_nlerp(a, q1, 0.5f)));
    }
    std::printf("   worst midpoint chord over 7 arcs: %.3e\n",
                static_cast<double>(worst_mid));
    check("F.4  exact at every arc, not just this one", worst_mid < 1e-6f);

    // F.5 — CONTROL, and it is the one that matters for a test suite. A test that
    // samples t = 0, 0.5 and 1 certifies nlerp as slerp, because those are
    // precisely the three values at which they agree.
    const float at_half = deg(engine::angle_between(engine::quat_slerp(a, b, 0.5f),
                                                    engine::quat_nlerp(a, b, 0.5f)));
    const float at_quarter = deg(engine::angle_between(engine::quat_slerp(a, b, 0.229f),
                                                       engine::quat_nlerp(a, b, 0.229f)));
    (void)0;
    std::printf("   CONTROL gap at t=0.5   %.4f deg\n", static_cast<double>(at_half));
    std::printf("   CONTROL gap at t=0.229 %.4f deg\n", static_cast<double>(at_quarter));
    check("F.5  CONTROL: the obvious sample points hide it",
          at_quarter > 1.0f && at_half < 0.001f);
}

// ===========================================================================
// G — the textbook form
// ===========================================================================

/// The form every reference prints. Written here rather than in the engine,
/// because §G is the argument for NOT shipping it.
quat textbook_slerp(quat a, quat b, float t)
{
    const quat n = engine::nearest(a, b);
    const float d = std::clamp(a.w * n.w + engine::dot(a.v, n.v), -1.0f, 1.0f);
    const float omega = std::acos(d);
    const float s = std::sin(omega);
    const float wa = std::sin((1.0f - t) * omega) / s;
    const float wb = std::sin(t * omega) / s;
    return {a.w * wa + n.w * wb, a.v * wa + n.v * wb};
}

void section_g()
{
    std::printf("\nG. the textbook form, and where it stops working\n");

    const vec3 axis = engine::normalised(vec3{0.31f, 0.86f, -0.41f});
    const quat a = quat::identity();
    const quat b = engine::quat_from_axis_angle(axis, rad(120.0f));

    float worst = 0.0f;
    for (int i = 0; i <= 500; ++i)
    {
        const float t = static_cast<float>(i) / 500.0f;
        worst = std::max(worst, chord(textbook_slerp(a, b, t),
                                      engine::quat_slerp(a, b, t)));
    }
    std::printf("   worst chord, textbook vs ours, 120 deg arc: %.3e\n",
                static_cast<double>(worst));
    check("G.1  the two forms are the same function", worst < 1e-6f);

    // G.2 — AND THEN THEY ARE NOT. The textbook form divides by sin(Omega),
    // which goes to zero exactly where adjacent animation keyframes live. Ours
    // divides by sin(theta/2) too — and the thing it divides IS sin(t*theta/2),
    // so the ratio tends to t and the expression is continuous. The textbook
    // form's numerator and denominator both vanish and the cancellation is not
    // arranged.
    // **AND THE FIRST DRAFT OF THIS TABLE LIED, which is the fifth blind
    // instrument Module 7 has found and the first one in the harness rather than
    // in the engine.** It accumulated the worst error with `std::max`, and
    // `std::max(x, NaN)` returns `x` — every NaN comparison is false, so a NaN
    // can never win a maximum. The textbook column printed a clean 0.0000e+00 in
    // four rows where the function was returning NaN in all of them. A NaN has to
    // be COUNTED, never maximised.
    std::printf("   arc (deg)     textbook       ours err\n");
    int textbook_nan = 0;
    float worst_small_text = 0.0f;
    float worst_small_ours = 0.0f;
    for (const float theta : {1.0f, 1e-1f, 1e-2f, 1e-4f, 0.0f})
    {
        const quat q1 = engine::quat_from_axis_angle(axis, rad(theta));
        float e_text = 0.0f;
        float e_ours = 0.0f;
        int nan_here = 0;
        for (int i = 0; i <= 100; ++i)
        {
            const float t = static_cast<float>(i) / 100.0f;
            // The exact answer at this arc: a turn of t*theta about the axis.
            const quat exact = engine::quat_from_axis_angle(axis, rad(theta) * t);
            const float et = chord(exact, textbook_slerp(a, q1, t));
            const float eo = chord(exact, engine::quat_slerp(a, q1, t));
            if (!(et == et)) { ++nan_here; } else { e_text = std::max(e_text, et); }
            if (!(eo == eo)) { ++nan_here; } else { e_ours = std::max(e_ours, eo); }
        }
        textbook_nan += nan_here;
        if (nan_here != 0)
        {
            std::printf("   %9.4f     %3d NaN of 101   %.4e\n",
                        static_cast<double>(theta), nan_here,
                        static_cast<double>(e_ours));
        }
        else
        {
            std::printf("   %9.4f     %.4e     %.4e\n", static_cast<double>(theta),
                        static_cast<double>(e_text), static_cast<double>(e_ours));
        }
        worst_small_text = std::max(worst_small_text, e_text);
        worst_small_ours = std::max(worst_small_ours, e_ours);
    }

    // Where the cliff is, and it is predictable to three figures. `acos` returns
    // exactly 0 once the dot product ROUNDS to 1. The ulp just below 1 is eps/2,
    // so round-to-nearest takes anything within eps/4 of 1 — which means
    // 1 - cos(Omega) < eps/4, i.e. Omega^2/2 < eps/4, i.e. the SPHERE arc below
    // sqrt(eps/2). The rotation arc is twice that.
    float cliff = 0.0f;
    for (float theta = 0.2f; theta > 1e-4f; theta *= 0.995f)
    {
        const quat q1 = engine::quat_from_axis_angle(axis, rad(theta));
        const quat got = textbook_slerp(a, q1, 0.5f);
        if (!(got.w == got.w)) { cliff = theta; break; }
    }
    std::printf("   textbook form goes NaN below an arc of %.4f deg\n",
                static_cast<double>(cliff));
    std::printf("   predicted: 2*sqrt(eps/2) = %.4f deg\n",
                static_cast<double>(deg(2.0f * std::sqrt(1.1920929e-7f * 0.5f))));
    check("G.2  ours is exact down to a zero arc", worst_small_ours < 1e-6f);
    check("G.3  CONTROL: the textbook form returns NaN there",
          textbook_nan > 100);
    check("G.4  and the cliff is where sqrt(eps/2) predicts",
          cliff > 0.0f
          && std::fabs(cliff - deg(2.0f * std::sqrt(1.1920929e-7f * 0.5f))) < 0.002f);
}

// ===========================================================================
// H — what it costs
// ===========================================================================
template <typename Fn>
double best_ns(Fn fn, int reps)
{
    double best = 1e30;
    fn();                                     // one warm-up, never timed
    for (int run = 0; run < 3; ++run)
    {
        const auto t0 = std::chrono::steady_clock::now();
        fn();
        const auto t1 = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        best = std::min(best, ns / static_cast<double>(reps));
    }
    return best;
}

void section_h()
{
    std::printf("\nH. what it costs\n");

    constexpr int k_n = 512;
    constexpr int k_reps = 20000;
    static quat qa[k_n];
    static quat qb[k_n];
    static mat3 ma[k_n];
    static mat3 mb[k_n];
    for (int i = 0; i < k_n; ++i)
    {
        qa[i] = sample_quat(static_cast<std::uint32_t>(i) * 2u);
        qb[i] = sample_quat(static_cast<std::uint32_t>(i) * 2u + 1u);
        ma[i] = engine::mat3_from_quat(qa[i]);
        mb[i] = engine::mat3_from_quat(qb[i]);
    }

    // EVERY LOOP ACCUMULATES rather than assigns. 7.4 §F.1's first draft wrote
    // `acc = ...` and reported 0.000 ns, because only the last iteration was
    // needed. Exactly zero is at least loud; a plausible small number is not.
    const double ns_slerp = best_ns([&] {
        double acc = 0.0;
        for (int r = 0; r < k_reps; ++r)
        {
            const int i = r & (k_n - 1);
            const float t = static_cast<float>(r & 63) / 63.0f;
            const quat q = engine::quat_slerp(qa[i], qb[i], t);
            acc += static_cast<double>(q.w + q.v.x + q.v.y + q.v.z);
        }
        sink += acc;
    }, k_reps);

    const double ns_nlerp = best_ns([&] {
        double acc = 0.0;
        for (int r = 0; r < k_reps; ++r)
        {
            const int i = r & (k_n - 1);
            const float t = static_cast<float>(r & 63) / 63.0f;
            const quat q = engine::quat_nlerp(qa[i], qb[i], t);
            acc += static_cast<double>(q.w + q.v.x + q.v.y + q.v.z);
        }
        sink += acc;
    }, k_reps);

    const double ns_mat = best_ns([&] {
        double acc = 0.0;
        for (int r = 0; r < k_reps; ++r)
        {
            const int i = r & (k_n - 1);
            const float t = static_cast<float>(r & 63) / 63.0f;
            const mat3 m = engine::rotation_slerp(ma[i], mb[i], t);
            acc += static_cast<double>(m.c0.x + m.c1.y + m.c2.z);
        }
        sink += acc;
    }, k_reps);

    std::printf("   quat_slerp        %7.3f ns\n", ns_slerp);
    std::printf("   quat_nlerp        %7.3f ns   %.2fx cheaper\n",
                ns_nlerp, ns_slerp / ns_nlerp);
    std::printf("   7.2's mat3 slerp  %7.3f ns   %.2fx dearer\n",
                ns_mat, ns_mat / ns_slerp);
    check("H.1  nlerp is cheaper than slerp", ns_nlerp < ns_slerp);
    check("H.2  the mat3 route is dearer than both", ns_mat > ns_slerp);

    // H.3 — THE NUMBER TO DESIGN WITH. Below what arc is nlerp within half a
    // degree of slerp? That is the threshold an animation system tests against,
    // and it is a property of the arc rather than a preference.
    const vec3 axis = engine::normalised(vec3{0.31f, 0.86f, -0.41f});
    float budget_arc = 0.0f;
    for (float theta = 1.0f; theta <= 180.0f; theta += 0.25f)
    {
        const quat b = engine::quat_from_axis_angle(axis, rad(theta));
        float gap = 0.0f;
        for (int i = 0; i <= 200; ++i)
        {
            const float t = static_cast<float>(i) / 200.0f;
            gap = std::max(gap, engine::angle_between(
                engine::quat_slerp(quat::identity(), b, t),
                engine::quat_nlerp(quat::identity(), b, t)));
        }
        if (deg(gap) <= 0.5f) { budget_arc = theta; }
    }
    std::printf("   nlerp stays within 0.5 deg up to an arc of %.2f deg\n",
                static_cast<double>(budget_arc));
    check("H.3  the 0.5 deg budget holds past 60 deg of arc", budget_arc > 60.0f);
}

// ===========================================================================
// I — the storage swap
// ===========================================================================
void section_i()
{
    std::printf("\nI. the swap: decomposition, round trip, and one sheared node\n");

    // I.1 — the round trip every call site in the engine now performs. Build a
    // rotation as a matrix, narrow it to a quaternion, widen it again.
    float worst_trip = 0.0f;
    for (std::uint32_t i = 0; i < 20000u; ++i)
    {
        const mat3 m = engine::mat3_from_quat(sample_quat(i));
        worst_trip = std::max(worst_trip,
                              max_element_diff(m, engine::mat3_from_quat(
                                  engine::quat_from_rotation(m))));
    }
    std::printf("   worst entry, mat3 -> quat -> mat3, 20,000 poses: %.3e\n",
                static_cast<double>(worst_trip));
    check("I.1  the round trip is lossless to float resolution",
          worst_trip < 1e-6f);

    // I.2 — the same question in the units that decide whether a picture moves.
    // A vertex 3 units from the origin, in a 960-pixel frame about 12 units wide.
    const float px = worst_trip * 3.0f * (960.0f / 12.0f);
    std::printf("   that is %.5f pixels at 3 units out, 960 wide\n",
                static_cast<double>(px));
    check("I.2  and therefore cannot move a pixel", px < 0.01f);

    // I.3 — `transform_from_affine` against the construction it inverts.
    struct probe { const char* name; vec3 scale; };
    const probe probes[] = {{"uniform 1", {1.0f, 1.0f, 1.0f}},
                            {"uniform 2", {2.0f, 2.0f, 2.0f}},
                            {"non-uniform", {2.0f, 0.5f, 1.3f}},
                            {"mirrored x", {-1.0f, 1.0f, 1.0f}},
                            {"flat z", {1.0f, 1.0f, 0.0f}}};
    float worst_rebuild = 0.0f;
    std::printf("   probe          worst entry   out of square  mirrored\n");
    for (const probe& p : probes)
    {
        transform t;
        t.position = {1.5f, -2.0f, 0.25f};
        t.rotation = engine::quat_from_euler({rad(37.0f), rad(-22.0f), rad(64.0f)});
        t.scale = p.scale;

        const mat4 built = engine::parent_from_local(t);
        const engine::transform_extraction got = engine::transform_from_affine(built);
        const mat4 rebuilt = engine::parent_from_local(got.value);

        float worst = 0.0f;
        for (int r = 0; r < 4; ++r)
        {
            for (int c = 0; c < 4; ++c)
            {
                worst = std::max(worst, std::fabs(built.at(r, c) - rebuilt.at(r, c)));
            }
        }
        worst_rebuild = std::max(worst_rebuild, worst);
        std::printf("   %-12s   %.3e     %.3e      %s\n", p.name,
                    static_cast<double>(worst),
                    static_cast<double>(got.out_of_square),
                    got.mirrored ? "yes" : "no");
    }
    check("I.3  every TRS matrix comes apart exactly", worst_rebuild < 1e-5f);

    // I.4 — the case that is outside the struct. A non-uniformly scaled parent
    // with a rotated child, which is a scene anybody can author by accident.
    transform parent;
    parent.scale = {2.0f, 0.5f, 1.0f};
    transform child;
    child.rotation = engine::quat_y(rad(40.0f));
    const mat4 world = engine::parent_from_local(parent) * engine::parent_from_local(child);
    const engine::transform_extraction sheared = engine::transform_from_affine(world);
    const mat4 approx = engine::parent_from_local(sheared.value);
    float worst_shear = 0.0f;
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            worst_shear = std::max(worst_shear, std::fabs(world.at(r, c) - approx.at(r, c)));
        }
    }
    std::printf("   sheared node: out of square %.4f, worst entry %.4f\n",
                static_cast<double>(sheared.out_of_square),
                static_cast<double>(worst_shear));
    check("I.4  shear is DETECTED",
          sheared.out_of_square > engine::k_transform_square_tolerance);
    check("I.5  CONTROL: and it is genuinely not representable", worst_shear > 0.05f);

    // I.6 — `collector`'s boom, which was exactly this shape and had been since
    // Lesson 5.12. The old code put S^-1 * Rz(-bank) into a field called
    // `rotation`; the repair is two nodes, and the two must agree to the bit.
    const vec3 rover_size{1.24f, 0.62f, 1.86f};
    const float bank = rad(22.0f);
    const mat3 unscale{{1.0f / rover_size.x, 0.0f, 0.0f},
                       {0.0f, 1.0f / rover_size.y, 0.0f},
                       {0.0f, 0.0f, 1.0f / rover_size.z}};
    const mat3 one_node = unscale * engine::rotation_z(-bank);

    transform node_scale;
    node_scale.scale = {1.0f / rover_size.x, 1.0f / rover_size.y, 1.0f / rover_size.z};
    transform node_roll;
    node_roll.rotation = engine::quat_z(-bank);
    const mat4 chain = engine::parent_from_local(node_scale)
                     * engine::parent_from_local(node_roll);
    const float chain_err = max_element_diff(one_node, engine::linear_of(chain));
    std::printf("   boom: one sheared node vs two clean ones: %.3e\n",
                static_cast<double>(chain_err));
    check("I.6  the two-node chain reproduces the old basis", chain_err < 1e-6f);

    // I.7 — CONTROL. What the one-node version becomes once the field is a quat:
    // the product is not a rotation, so extracting one is an answer to a
    // different question.
    const engine::transform_extraction naive =
        engine::transform_from_affine(engine::affine(one_node, vec3{}));
    const float naive_err = max_element_diff(
        one_node, engine::mat3_from_quat(naive.value.rotation));
    std::printf("   CONTROL one node through a quat: out of square %.4f,\n",
                static_cast<double>(naive.out_of_square));
    std::printf("     worst entry error %.4f\n", static_cast<double>(naive_err));
    check("I.7  CONTROL: one node cannot hold it", naive_err > 0.05f);
}

}   // namespace

int main()
{
    std::printf("verify_75 - Lesson 7.5, Slerp and the Storage Swap\n");

    section_a();
    section_b();
    section_c();
    section_d();
    section_e();
    section_f();
    section_g();
    section_h();
    section_i();

    std::printf("\n%d / %d checks passed\n", checks_passed, checks_run);
    std::printf("(sink %.3f)\n", sink);
    return checks_passed == checks_run ? 0 : 1;
}
