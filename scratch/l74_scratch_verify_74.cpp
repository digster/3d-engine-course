// scratch/verify_74.cpp — every number Lesson 7.4 prints, measured rather than
// asserted.
//
// Build and run:  sh scratch/build_verify_74.sh
//
// Eight sections, and the order is the lesson's:
//
//   A  the multiplication table, FORCED rather than postulated
//   B  the product: the dot and the cross hiding inside it
//   C  two reflections make a rotation, and the sandwich falls out
//   D  the half-angle and the double cover
//   E  extraction: four candidates, and why there is no bad case
//   F  what it costs — compose, apply, convert, drift
//   G  against the engine's existing rotation machinery
//   H  what a quaternion CANNOT store, and what that costs the engine
//
// EVERY SECTION CARRIES A CONTROL, for the reason 7.1 states and 7.2 and 7.3
// repeat: a check whose degenerate case is a pass is not a check. The sharpest
// ones here are in C and D. C's claim is that `q v conj(q)` is the rotation, so
// its control is `conj(q) v q`, which must REVERSE every composition rather
// than merely differ. D's claim is that the double cover is harmless, so its
// control is `angle_between` with the absolute value removed, which must report
// two identical poses as a full turn apart.
//
// TIMING RULES, inherited from 7.2 and 7.3 and obeyed here without exception:
//   - consume the WHOLE result
//   - VARY the input with the repetition counter
//   - BEST OF THREE, never the mean
//   - ONE WARM-UP before the timed runs
//   - divide by the operation count and ask whether the answer is physically
//     possible before believing it
//
// OUTPUT WIDTH IS A CONSTRAINT, not a preference. The lesson quotes this
// program's transcript in <pre> blocks that scroll and never wrap, and the fold
// is at about 66 characters. 7.3 lost fourteen NUMBERS past that edge before
// anybody noticed, including an exactly-zero claim. Nothing below exceeds 66.

#include <engine/math/axis_angle.hpp>
#include <engine/math/euler.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/rotation.hpp>
#include <engine/math/vec3.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>

using engine::axis_angle;
using engine::mat3;
using engine::quat;
using engine::vec3;

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

float max_component_diff(quat a, quat b)
{
    return std::max({std::fabs(a.w - b.w), std::fabs(a.v.x - b.v.x),
                     std::fabs(a.v.y - b.v.y), std::fabs(a.v.z - b.v.z)});
}

/// How far `m` is from being a rotation: max |MᵀM − I| over the nine entries.
///
/// 7.2's function, kept in the harness for the reason that lesson gives: it is
/// half of `is_rotation`, which is an exercise, and writing it into `math/`
/// would solve a problem the student was handed. Section F needs it to make the
/// drift comparison FAIR — a quat drifts in one number and a mat3 in nine, so
/// the two have to be asked the same question rather than each asked its own.
float orthonormality_defect(const mat3& m)
{
    return max_element_diff(engine::transpose(m) * m, mat3::identity());
}

/// A deterministic pseudo-random stream. Same generator as 7.1, 7.2 and 7.3, so
/// that a measurement here and a measurement there are drawn from the same
/// sequence and any difference between them is the code and not the inputs.
struct rng
{
    std::uint32_t s = 0x13579bdfu;

    std::uint32_t next()
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }

    float uniform(float lo, float hi)
    {
        const float u = static_cast<float>(next() >> 8) / 16777216.0f;
        return lo + (hi - lo) * u;
    }

    /// A unit vector, uniform on the sphere. Rejection rather than the usual
    /// spherical-coordinate pair, because that one clusters at the poles and
    /// this file's whole subject is whether a representation has a bad
    /// direction.
    vec3 unit_vector()
    {
        for (;;)
        {
            const vec3 p{uniform(-1.0f, 1.0f), uniform(-1.0f, 1.0f),
                         uniform(-1.0f, 1.0f)};
            const float len_sq = engine::length_squared(p);
            if (len_sq > 1e-4f && len_sq <= 1.0f) { return p / std::sqrt(len_sq); }
        }
    }

    quat unit_quat() { return engine::quat_from_axis_angle(unit_vector(),
                                                           uniform(-k_pi, k_pi)); }
};

/// Best-of-three timing of `fn`, in nanoseconds per operation, with a warm-up.
template <typename Fn>
double time_ns(Fn&& fn, long long ops, double& sink)
{
    sink += fn();

    double best = 1e300;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        const auto t0 = std::chrono::steady_clock::now();
        sink += fn();
        const auto t1 = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        best = std::min(best, ns / static_cast<double>(ops));
    }
    return best;
}

double sink = 0.0;

// ===========================================================================
// A — the multiplication table, FORCED
// ===========================================================================
//
// The lesson does not postulate i, j, k and a table. It makes ONE geometric
// demand — space has no preferred direction, so EVERY unit imaginary must be a
// half-turn and square to -1 — and derives the rest. This section is that
// argument in code.

void section_a()
{
    std::printf("\nA. THE TABLE, FORCED\n");

    const quat i = quat::i();
    const quat j = quat::j();
    const quat k = quat::k();

    // A.1 — ISOTROPY. Every unit imaginary squares to -1, not just the three
    // basis ones. This is the single input; everything below is a consequence.
    rng r;
    float worst = 0.0f;
    for (int n = 0; n < 20000; ++n)
    {
        const quat u = quat::pure(r.unit_vector());
        const quat s = u * u;
        worst = std::max({worst, std::fabs(s.w + 1.0f),
                          engine::length(s.v)});
    }
    std::printf("   worst |u^2 + 1| over 20000 axes:  %.3e\n",
                static_cast<double>(worst));
    check("A.1  every unit imaginary is a half-turn", worst < 1e-6f);

    // A.2 — and that FORCES anticommutativity. (i+j)/sqrt2 is a unit
    // imaginary, so its square is -1; expand and the cross terms must vanish.
    const quat ij = i * j;
    const quat ji = j * i;
    std::printf("   i*j = (%.1f, %.1f, %.1f, %.1f)\n",
                static_cast<double>(ij.w), static_cast<double>(ij.v.x),
                static_cast<double>(ij.v.y), static_cast<double>(ij.v.z));
    std::printf("   j*i = (%.1f, %.1f, %.1f, %.1f)\n",
                static_cast<double>(ji.w), static_cast<double>(ji.v.x),
                static_cast<double>(ji.v.y), static_cast<double>(ji.v.z));
    check("A.2  ij = k and ji = -k", ij == k && ji == -k);

    // A.3 — the rest of the table, every entry, from associativity alone.
    check("A.3  jk = i, ki = j, ik = -j, kj = -i",
          (j * k) == i && (k * i) == j && (i * k) == -j && (k * j) == -i);

    // A.4 — Hamilton's bridge formula, which he cut into the stone of Broom
    // Bridge in 1843. It is a CONSEQUENCE here, not a definition.
    const quat ijk = i * j * k;
    std::printf("   i*j*k = (%.1f, %.1f, %.1f, %.1f)\n",
                static_cast<double>(ijk.w), static_cast<double>(ijk.v.x),
                static_cast<double>(ijk.v.y), static_cast<double>(ijk.v.z));
    check("A.4  i^2 = j^2 = k^2 = ijk = -1",
          (i * i) == -quat::identity() && (j * j) == -quat::identity() &&
          (k * k) == -quat::identity() && ijk == -quat::identity());

    // A.5 — THE CONTROL. If the table were commutative the algebra would be
    // describing something other than rotations of space, so the check that
    // matters is that it is NOT. A commuting product would pass A.1 and A.4.
    check("A.5  CONTROL: the product does not commute", (i * j) != (j * i));

    // A.6 — associativity, which the derivation of A.3 leans on at every step
    // and which nothing above actually verified.
    rng r2;
    float worst_assoc = 0.0f;
    for (int n = 0; n < 20000; ++n)
    {
        const quat a = r2.unit_quat();
        const quat b = r2.unit_quat();
        const quat c = r2.unit_quat();
        worst_assoc = std::max(worst_assoc,
                               max_component_diff((a * b) * c, a * (b * c)));
    }
    std::printf("   worst |(ab)c - a(bc)| over 20000:  %.3e\n",
                static_cast<double>(worst_assoc));
    check("A.6  the product is associative", worst_assoc < 1e-6f);
}

// ===========================================================================
// B — the product, and the two vector operations inside it
// ===========================================================================

void section_b()
{
    std::printf("\nB. THE PRODUCT\n");

    // B.1 — the product of two PURE quaternions is -(dot) + (cross). This is
    // the whole content of the multiplication table, said once in vector
    // language, and it is where the dot and cross products came from
    // historically.
    rng r;
    float worst = 0.0f;
    for (int n = 0; n < 20000; ++n)
    {
        const vec3 a{r.uniform(-2.0f, 2.0f), r.uniform(-2.0f, 2.0f),
                     r.uniform(-2.0f, 2.0f)};
        const vec3 b{r.uniform(-2.0f, 2.0f), r.uniform(-2.0f, 2.0f),
                     r.uniform(-2.0f, 2.0f)};
        const quat p = quat::pure(a) * quat::pure(b);
        worst = std::max(worst, std::fabs(p.w + engine::dot(a, b)));
        worst = std::max(worst, engine::length(p.v - engine::cross(a, b)));
    }
    std::printf("   worst |v1 v2 - (-dot + cross)|:    %.3e\n",
                static_cast<double>(worst));
    check("B.1  v1 v2 = -(v1.v2) + (v1 x v2)", worst < 1e-5f);

    // B.2 — THE WORKED EXAMPLE the lesson pushes through by hand.
    //   q = (2, 1, -1, 3)   p = (-1, 2, 0, 1)
    // scalar: 2*-1 - (1*2 + -1*0 + 3*1) = -2 - 5 = -7
    // vector: 2*(2,0,1) + -1*(1,-1,3) + (1,-1,3)x(2,0,1)
    //       = (4,0,2) + (-1,1,-3) + (-1,5,2) = (2,6,1)
    const quat q{2.0f, {1.0f, -1.0f, 3.0f}};
    const quat p{-1.0f, {2.0f, 0.0f, 1.0f}};
    const quat qp = q * p;
    const quat pq = p * q;
    std::printf("   q*p = (%.0f, %.0f, %.0f, %.0f)\n",
                static_cast<double>(qp.w), static_cast<double>(qp.v.x),
                static_cast<double>(qp.v.y), static_cast<double>(qp.v.z));
    std::printf("   p*q = (%.0f, %.0f, %.0f, %.0f)\n",
                static_cast<double>(pq.w), static_cast<double>(pq.v.x),
                static_cast<double>(pq.v.y), static_cast<double>(pq.v.z));
    check("B.2  q*p = (-7, 2, 6, 1) by hand",
          qp == (quat{-7.0f, {2.0f, 6.0f, 1.0f}}));

    // B.3 — the difference between the two orders is EXACTLY 2 (v1 x v2), so
    // the scalar parts agree and only the vector parts differ.
    const quat diff = qp - pq;
    const vec3 twice_cross = engine::cross(q.v, p.v) * 2.0f;
    std::printf("   q*p - p*q = (%.0f, %.0f, %.0f, %.0f)\n",
                static_cast<double>(diff.w), static_cast<double>(diff.v.x),
                static_cast<double>(diff.v.y), static_cast<double>(diff.v.z));
    std::printf("   2 (v1 x v2) = (%.0f, %.0f, %.0f)\n",
                static_cast<double>(twice_cross.x),
                static_cast<double>(twice_cross.y),
                static_cast<double>(twice_cross.z));
    check("B.3  qp - pq = (0, 2 v1 x v2)",
          diff.w == 0.0f && diff.v == twice_cross);

    // B.4 — |qp| = |q||p|, the multiplicative norm. This is what makes unit
    // quaternions closed under multiplication, which is what makes composing
    // two rotations give a rotation.
    rng r2;
    float worst_norm = 0.0f;
    for (int n = 0; n < 20000; ++n)
    {
        const quat a{r2.uniform(-2.0f, 2.0f), {r2.uniform(-2.0f, 2.0f),
                     r2.uniform(-2.0f, 2.0f), r2.uniform(-2.0f, 2.0f)}};
        const quat b{r2.uniform(-2.0f, 2.0f), {r2.uniform(-2.0f, 2.0f),
                     r2.uniform(-2.0f, 2.0f), r2.uniform(-2.0f, 2.0f)}};
        const float lhs = engine::length(a * b);
        const float rhs = engine::length(a) * engine::length(b);
        worst_norm = std::max(worst_norm,
                              std::fabs(lhs - rhs) / std::max(rhs, 1e-6f));
    }
    std::printf("   worst relative |qp|-|q||p|:        %.3e\n",
                static_cast<double>(worst_norm));
    check("B.4  the norm is multiplicative", worst_norm < 1e-5f);

    // B.5 — CONTROL. B.3 says the ONLY thing that changes under a swap is the
    // cross term, so a pair whose vector parts are parallel must commute
    // exactly. If that failed, B.3 would be an accident of one example.
    const quat a{1.5f, {2.0f, -1.0f, 0.5f}};
    const quat b{-0.25f, {-4.0f, 2.0f, -1.0f}};   // v_b = -2 * v_a
    check("B.5  CONTROL: parallel vector parts commute",
          (a * b) == (b * a));
}

// ===========================================================================
// C — two reflections make a rotation, and the sandwich falls out
// ===========================================================================

void section_c()
{
    std::printf("\nC. REFLECTIONS AND THE SANDWICH\n");

    // C.1 — n v n IS vec3::reflect, the function this engine has had since
    // Lesson 1.8. Two routes to one answer; the engine's is the control.
    rng r;
    float worst = 0.0f;
    for (int n = 0; n < 20000; ++n)
    {
        const vec3 normal = r.unit_vector();
        const vec3 v{r.uniform(-3.0f, 3.0f), r.uniform(-3.0f, 3.0f),
                     r.uniform(-3.0f, 3.0f)};
        worst = std::max(worst, engine::length(
            engine::reflect_in_plane(v, normal) - engine::reflect(v, normal)));
    }
    std::printf("   worst |n v n - reflect(v, n)|:     %.3e\n",
                static_cast<double>(worst));
    check("C.1  n v n is the Householder reflection", worst < 1e-5f);

    // C.2 — two reflections in mirrors PHI apart give a rotation of 2 PHI
    // about the line where the planes meet. The half-angle, in space.
    const vec3 n0{1.0f, 0.0f, 0.0f};
    const float phi = rad(30.0f);
    const vec3 n1{std::cos(phi), std::sin(phi), 0.0f};
    const quat rotor = engine::rotor_from_mirrors(n0, n1);

    // AND HERE IS THE DOUBLE COVER, ARRIVING UNINVITED. `n1 n0` comes out with
    // a NEGATIVE real part — it is -(cos phi + sin phi n) and not the other
    // one — so `axis_angle_from_quat`, which reports an angle in [0, 2pi),
    // calls this 300 degrees about +z. That is the same rotation as 60 degrees
    // about +z and this is not a bug in either: the derivation genuinely
    // produces a sign nobody chose, which is exactly what the double cover is.
    // Negating puts it on the branch the rest of the file uses.
    const quat canonical = (rotor.w < 0.0f) ? -rotor : rotor;
    const auto raw = engine::axis_angle_from_quat(rotor);
    const auto got = engine::axis_angle_from_quat(canonical);
    std::printf("   mirrors %5.1f deg apart -> turn of %6.2f deg\n",
                static_cast<double>(deg(phi)),
                static_cast<double>(deg(got.value.angle)));
    std::printf("   the same rotor read unnegated:    %6.2f deg\n",
                static_cast<double>(deg(raw.value.angle)));
    std::printf("   rotor = (%.6f, %.6f, %.6f, %.6f)\n",
                static_cast<double>(rotor.w), static_cast<double>(rotor.v.x),
                static_cast<double>(rotor.v.y), static_cast<double>(rotor.v.z));
    check("C.2  30 deg of mirror makes 60 deg of turn",
          std::fabs(deg(got.value.angle) - 60.0f) < 1e-3f &&
          std::fabs(deg(raw.value.angle) - 300.0f) < 1e-3f &&
          max_element_diff(engine::mat3_from_quat(rotor),
                           engine::mat3_from_quat(canonical)) == 0.0f);

    // C.3 — the rotor's own half-angle: |w| is cos(phi), |v| is sin(phi).
    std::printf("   |w| = %.6f, cos(phi) = %.6f\n",
                static_cast<double>(std::fabs(rotor.w)),
                static_cast<double>(std::cos(phi)));
    check("C.3  |w| = cos(phi), the mirror separation",
          std::fabs(std::fabs(rotor.w) - std::cos(phi)) < 1e-6f);

    // C.4 — the sandwich reproduces Lesson 7.2's Rodrigues matrix, over a
    // sweep of axes and angles. This is the load-bearing agreement of the
    // whole lesson: two entirely different derivations, one answer.
    rng r2;
    float worst_rod = 0.0f;
    for (int n = 0; n < 20000; ++n)
    {
        const vec3 axis = r2.unit_vector();
        const float angle = r2.uniform(-k_pi, k_pi);
        const mat3 from_q = engine::mat3_from_quat(
            engine::quat_from_axis_angle(axis, angle));
        const mat3 from_r = engine::rotation_from_axis_angle({axis, angle});
        worst_rod = std::max(worst_rod, max_element_diff(from_q, from_r));
    }
    std::printf("   worst |sandwich - Rodrigues|:      %.3e\n",
                static_cast<double>(worst_rod));
    check("C.4  the sandwich is Rodrigues' formula", worst_rod < 1e-5f);

    // C.5 — composition is a homomorphism: M(q p) = M(q) M(p), same order.
    rng r3;
    float worst_hom = 0.0f;
    for (int n = 0; n < 20000; ++n)
    {
        const quat a = r3.unit_quat();
        const quat b = r3.unit_quat();
        worst_hom = std::max(worst_hom, max_element_diff(
            engine::mat3_from_quat(a * b),
            engine::mat3_from_quat(a) * engine::mat3_from_quat(b)));
    }
    std::printf("   worst |M(qp) - M(q)M(p)|:          %.3e\n",
                static_cast<double>(worst_hom));
    check("C.5  M(q p) = M(q) M(p), not reversed", worst_hom < 1e-5f);

    // C.6 — THE CONTROL, and it is the sharpest one in the file. The other
    // sandwich, conj(q) v q, is also a rotation and also lands the basis
    // vectors somewhere sensible. What it gets wrong is the ORDER: it is an
    // ANTI-homomorphism, so every composition comes out backwards. A test that
    // only looked at one rotation would pass on both.
    rng r4;
    float worst_reversed = 0.0f;   // must stay TINY: W is an anti-homomorphism
    float worst_forward = 0.0f;    // must grow LARGE: it is not a homomorphism
    for (int n = 0; n < 2000; ++n)
    {
        const quat a = r4.unit_quat();
        const quat b = r4.unit_quat();
        // The wrong sandwich, built column by column.
        auto wrong = [](quat q) {
            auto f = [q](vec3 e) {
                return (engine::conjugate(q) * quat::pure(e) * q).v;
            };
            return mat3{f({1.0f, 0.0f, 0.0f}), f({0.0f, 1.0f, 0.0f}),
                        f({0.0f, 0.0f, 1.0f})};
        };
        worst_reversed = std::max(worst_reversed,
                                  max_element_diff(wrong(a * b),
                                                   wrong(b) * wrong(a)));
        worst_forward = std::max(worst_forward,
                                 max_element_diff(wrong(a * b),
                                                  wrong(a) * wrong(b)));
    }
    std::printf("   conj(q) v q:  |W(qp) - W(p)W(q)| = %.3e\n",
                static_cast<double>(worst_reversed));
    std::printf("                 |W(qp) - W(q)W(p)| = %.3e\n",
                static_cast<double>(worst_forward));
    check("C.6  CONTROL: the other sandwich reverses order",
          worst_reversed < 1e-5f && worst_forward > 0.5f);
}

// ===========================================================================
// D — the half-angle and the double cover
// ===========================================================================

void section_d()
{
    std::printf("\nD. HALF ANGLES AND THE DOUBLE COVER\n");

    // D.1 — q and -q are the same rotation, EXACTLY, because the sandwich is
    // quadratic in q: every term carries one factor from each side.
    rng r;
    float worst = 0.0f;
    for (int n = 0; n < 20000; ++n)
    {
        const quat q = r.unit_quat();
        worst = std::max(worst, max_element_diff(engine::mat3_from_quat(q),
                                                 engine::mat3_from_quat(-q)));
    }
    std::printf("   worst |M(q) - M(-q)| over 20000:   %.3e\n",
                static_cast<double>(worst));
    check("D.1  q and -q are the same rotation, bitwise", worst == 0.0f);

    // D.2 — so `angle_between` MUST take the absolute value of the dot
    // product. Without it, two identical poses read as a full turn apart.
    const quat q = engine::quat_from_axis_angle(
        engine::normalised(vec3{1.0f, 2.0f, 3.0f}), rad(101.0f));
    const float with_abs = engine::angle_between(q, -q);
    const float d = q.w * (-q).w + engine::dot(q.v, (-q).v);
    const float without = 2.0f * std::acos(std::clamp(d, -1.0f, 1.0f));
    std::printf("   angle_between(q, -q):  %8.4f deg\n",
                static_cast<double>(deg(with_abs)));
    std::printf("   with the fabs removed: %8.4f deg\n",
                static_cast<double>(deg(without)));
    check("D.2  CONTROL: dropping the fabs reports 360 deg",
          deg(with_abs) < 1e-3f && std::fabs(deg(without) - 360.0f) < 1e-3f);

    // D.3 — the 720 degree walk. Turn steadily about one axis and watch the
    // QUATERNION: it returns to -1 after a full turn of the object and to +1
    // only after two. The object comes home; its quaternion does not.
    const vec3 axis = engine::normalised(vec3{0.0f, 1.0f, 0.0f});
    std::printf("   turn      w        |v|     pose vs start\n");
    for (int i = 0; i <= 8; ++i)
    {
        const float turn = rad(90.0f * static_cast<float>(i));
        const quat qi = engine::quat_from_axis_angle(axis, turn);
        const float pose = deg(engine::angle_between_rotations(
            mat3::identity(), engine::mat3_from_quat(qi)));
        std::printf("   %4.0f  %+8.5f  %7.5f  %9.4f deg\n",
                    static_cast<double>(deg(turn)),
                    static_cast<double>(qi.w),
                    static_cast<double>(engine::length(qi.v)),
                    static_cast<double>(pose));
    }
    const quat at360 = engine::quat_from_axis_angle(axis, rad(360.0f));
    const quat at720 = engine::quat_from_axis_angle(axis, rad(720.0f));
    check("D.3  360 deg gives w = -1, 720 deg gives w = +1",
          std::fabs(at360.w + 1.0f) < 1e-5f &&
          std::fabs(at720.w - 1.0f) < 1e-5f);

    // D.4 — the three basis imaginaries are HALF turns, not quarter turns.
    // This is the smallest example of the half-angle there is, and it catches
    // everyone who arrives from complex numbers expecting `i` to be a quarter.
    const auto ex = engine::axis_angle_from_quat(quat::i());
    std::printf("   quat::i() is a turn of %.1f deg\n",
                static_cast<double>(deg(ex.value.angle)));
    check("D.4  quat::i() is a half-turn, not a quarter",
          std::fabs(deg(ex.value.angle) - 180.0f) < 1e-3f);

    // D.5 — and therefore the QUARTER turn about x is not `i` but
    // cos(45) + sin(45) i, whose components are the ones people recognise.
    const quat quarter = engine::quat_x(rad(90.0f));
    std::printf("   quarter turn about x = (%.6f, %.6f, 0, 0)\n",
                static_cast<double>(quarter.w),
                static_cast<double>(quarter.v.x));
    check("D.5  a quarter turn has w = cos(45 deg)",
          std::fabs(quarter.w - std::sqrt(0.5f)) < 1e-6f);
}

// ===========================================================================
// E — extraction: four candidates, and why there is no bad case
// ===========================================================================

/// The naive route every first implementation uses: w from the trace, the rest
/// from the antisymmetric part over 4w. Kept HERE and not in the engine, for
/// the reason `angle_between_rotations_by_trace` gives: it is the right formula
/// to have in your head and the wrong one to call, and the comparison is only
/// worth anything against the real thing.
quat quat_from_rotation_naive(const mat3& m)
{
    const float trace = m.at(0, 0) + m.at(1, 1) + m.at(2, 2);
    const float w = std::sqrt(std::max(1.0f + trace, 0.0f)) * 0.5f;
    if (w <= 0.0f) { return quat::identity(); }
    const float s = 0.25f / w;
    return {w, {(m.at(2, 1) - m.at(1, 2)) * s,
                (m.at(0, 2) - m.at(2, 0)) * s,
                (m.at(1, 0) - m.at(0, 1)) * s}};
}

void section_e()
{
    std::printf("\nE. EXTRACTION\n");

    // E.1 — the four candidates sum to 4, always, which is the whole argument
    // for why the pivot is never small: the largest of four numbers summing to
    // 4 is at least 1.
    rng r;
    float worst_sum = 0.0f;
    float smallest_pivot = 1e30f;
    int histogram[4] = {0, 0, 0, 0};
    for (int n = 0; n < 20000; ++n)
    {
        const mat3 m = engine::mat3_from_quat(r.unit_quat());
        const float r00 = m.at(0, 0);
        const float r11 = m.at(1, 1);
        const float r22 = m.at(2, 2);
        const float cand[4] = {1.0f + r00 + r11 + r22, 1.0f + r00 - r11 - r22,
                               1.0f - r00 + r11 - r22, 1.0f - r00 - r11 + r22};
        float total = 0.0f;
        int pivot = 0;
        for (int i = 0; i < 4; ++i)
        {
            total += cand[i];
            if (cand[i] > cand[pivot]) { pivot = i; }
        }
        ++histogram[pivot];
        worst_sum = std::max(worst_sum, std::fabs(total - 4.0f));
        smallest_pivot = std::min(smallest_pivot, cand[pivot]);
    }
    std::printf("   worst |sum of candidates - 4|:     %.3e\n",
                static_cast<double>(worst_sum));
    std::printf("   smallest pivot seen:              %.6f\n",
                static_cast<double>(smallest_pivot));
    std::printf("   pivot chosen: w %d  x %d  y %d  z %d\n",
                histogram[0], histogram[1], histogram[2], histogram[3]);
    check("E.1  the four candidates sum to 4, pivot >= 1",
          worst_sum < 1e-5f && smallest_pivot >= 0.999f);

    // E.2 — the round trip, over the whole range of angles, both routes.
    std::printf("   angle    Shepperd      naive\n");
    const float probes[] = {0.001f, 1.0f, 45.0f, 90.0f, 120.0f, 170.0f,
                            179.0f, 179.99f, 180.0f};
    float worst_ours = 0.0f;
    float worst_naive = 0.0f;
    for (float a : probes)
    {
        const vec3 axis = engine::normalised(vec3{0.3f, -0.6f, 0.74f});
        const mat3 m = engine::rotation_from_axis_angle({axis, rad(a)});
        const float ours = deg(engine::angle_between_rotations(
            m, engine::mat3_from_quat(engine::quat_from_rotation(m))));
        const float naive = deg(engine::angle_between_rotations(
            m, engine::mat3_from_quat(quat_from_rotation_naive(m))));
        std::printf("   %7.2f  %.3e  %.3e\n", static_cast<double>(a),
                    static_cast<double>(ours), static_cast<double>(naive));
        worst_ours = std::max(worst_ours, ours);
        worst_naive = std::max(worst_naive, naive);
    }
    check("E.2  Shepperd round-trips everywhere, naive does not",
          worst_ours < 1e-3f && worst_naive > 1.0f);

    // E.3 — the round trip over a full random sweep, which is the number the
    // lesson quotes.
    rng r2;
    float worst_trip = 0.0f;
    for (int n = 0; n < 20000; ++n)
    {
        const mat3 m = engine::mat3_from_quat(r2.unit_quat());
        worst_trip = std::max(worst_trip, deg(engine::angle_between_rotations(
            m, engine::mat3_from_quat(engine::quat_from_rotation(m)))));
    }
    std::printf("   worst round trip over 20000:      %.3e deg\n",
                static_cast<double>(worst_trip));
    check("E.3  matrix -> quat -> matrix is exact", worst_trip < 1e-2f);

    // E.4 — CONTROL. E.3 would pass trivially if `quat_from_rotation` just
    // happened to return something whose matrix is the identity for an
    // identity input and nobody checked the rest. So: feed it a matrix built
    // from a KNOWN quaternion and demand the quaternion back, up to sign.
    rng r3;
    float worst_q = 0.0f;
    for (int n = 0; n < 20000; ++n)
    {
        const quat q = r3.unit_quat();
        const quat back = engine::quat_from_rotation(engine::mat3_from_quat(q));
        const float same = max_component_diff(q, back);
        const float flipped = max_component_diff(q, -back);
        worst_q = std::max(worst_q, std::min(same, flipped));
    }
    std::printf("   worst |q - +/-extract(M(q))|:      %.3e\n",
                static_cast<double>(worst_q));
    check("E.4  CONTROL: the components come back, up to sign",
          worst_q < 1e-4f);
}

// ===========================================================================
// F — what it costs
// ===========================================================================

void section_f()
{
    std::printf("\nF. COST\n");

    constexpr int kN = 4096;
    static quat qs[kN];
    static mat3 ms[kN];
    static vec3 vs[kN];
    rng r;
    for (int i = 0; i < kN; ++i)
    {
        qs[i] = r.unit_quat();
        ms[i] = engine::mat3_from_quat(qs[i]);
        vs[i] = r.unit_vector();
    }

    // Inner loop over the array, outer loop over repetitions — 7.3's shape,
    // and the two rules it enforces: the input index moves with the repetition
    // counter so nothing can be hoisted, and EVERY component of the result is
    // accumulated so nothing can be dead-coded. The first draft of F.1 wrote
    // `acc = qs[i] * qs[j]` and measured 0.000 ns, because only the last
    // iteration's product is needed and the compiler knows it.
    //
    // ONE HONEST BIAS, stated rather than hidden: accumulating the whole result
    // costs 4 adds for a quaternion and 9 for a matrix, so the matrix is
    // carrying five extra adds it would not pay in a real caller. That
    // overstates the quaternion's win by roughly the cost of five adds, which
    // §10.1 subtracts out by comparing against the apply loop below, where the
    // accumulation is 3 adds on both sides.
    constexpr int kReps = 4000;

    // F.1 — composing.
    const double t_qq = time_ns([&] {
        double acc = 0.0;
        for (int rep = 0; rep < kReps; ++rep)
        {
            for (int k = 0; k < kN; ++k)
            {
                const quat p = qs[k] * qs[(k + rep) & (kN - 1)];
                acc += static_cast<double>(p.w) + static_cast<double>(p.v.x) +
                       static_cast<double>(p.v.y) + static_cast<double>(p.v.z);
            }
        }
        return acc;
    }, static_cast<long long>(kReps) * kN, sink);

    const double t_mm = time_ns([&] {
        double acc = 0.0;
        for (int rep = 0; rep < kReps; ++rep)
        {
            for (int k = 0; k < kN; ++k)
            {
                const mat3 m = ms[k] * ms[(k + rep) & (kN - 1)];
                acc += static_cast<double>(m.c0.x + m.c0.y + m.c0.z) +
                       static_cast<double>(m.c1.x + m.c1.y + m.c1.z) +
                       static_cast<double>(m.c2.x + m.c2.y + m.c2.z);
            }
        }
        return acc;
    }, static_cast<long long>(kReps) * kN, sink);

    std::printf("   compose  quat*quat  %6.3f ns  (16 mul, 12 add)\n", t_qq);
    std::printf("   compose  mat3*mat3  %6.3f ns  (27 mul, 18 add)\n", t_mm);
    std::printf("   ratio               %6.2fx\n", t_mm / t_qq);
    check("F.1  composing is cheaper as a quaternion", t_qq < t_mm);

    // F.2 — applying. This is the one quaternions LOSE, and the accumulation
    // is three adds on both sides, so this row carries no bias at all.
    const double t_qv = time_ns([&] {
        double acc = 0.0;
        for (int rep = 0; rep < kReps; ++rep)
        {
            for (int k = 0; k < kN; ++k)
            {
                const vec3 o = engine::rotate(qs[k], vs[(k + rep) & (kN - 1)]);
                acc += static_cast<double>(o.x + o.y + o.z);
            }
        }
        return acc;
    }, static_cast<long long>(kReps) * kN, sink);

    const double t_mv = time_ns([&] {
        double acc = 0.0;
        for (int rep = 0; rep < kReps; ++rep)
        {
            for (int k = 0; k < kN; ++k)
            {
                const vec3 o = ms[k] * vs[(k + rep) & (kN - 1)];
                acc += static_cast<double>(o.x + o.y + o.z);
            }
        }
        return acc;
    }, static_cast<long long>(kReps) * kN, sink);

    std::printf("   apply    quat*vec3  %6.3f ns  (18 mul, 12 add)\n", t_qv);
    std::printf("   apply    mat3*vec3  %6.3f ns  ( 9 mul,  6 add)\n", t_mv);
    std::printf("   ratio               %6.2fx  (quaternion slower)\n",
                t_qv / t_mv);
    check("F.2  applying is cheaper as a matrix", t_mv < t_qv);

    // F.3 — the conversion, and therefore the crossover: how many vectors must
    // one quaternion rotate before building the matrix first pays?
    const double t_conv = time_ns([&] {
        double acc = 0.0;
        for (int rep = 0; rep < kReps; ++rep)
        {
            for (int k = 0; k < kN; ++k)
            {
                const mat3 m = engine::mat3_from_quat(qs[(k + rep) & (kN - 1)]);
                acc += static_cast<double>(m.c0.x + m.c0.y + m.c0.z) +
                       static_cast<double>(m.c1.x + m.c1.y + m.c1.z) +
                       static_cast<double>(m.c2.x + m.c2.y + m.c2.z);
            }
        }
        return acc;
    }, static_cast<long long>(kReps) * kN, sink);

    std::printf("   convert  mat3_from_quat  %6.3f ns\n", t_conv);
    const double crossover = t_conv / (t_qv - t_mv);
    std::printf("   crossover: %.2f vectors per rotation\n", crossover);
    check("F.3  the crossover is a small number of vectors",
          crossover > 0.5 && crossover < 20.0);

    // F.4 — DRIFT, and the answer is not the one the folklore gives.
    //
    // Both are asked the same question, `max |MtM - I|`, because a quaternion
    // drifts in one number and a mat3 in nine and "how big is the norm error"
    // is not comparable with "how non-orthonormal is the matrix". 7.3 ran this
    // in the plane and the complex number won by 6.15x. One dimension up it
    // LOSES, and the reason is worth more than the result: an unrepaired
    // quaternion's norm error compounds as a POWER, |q|^n, and the sandwich
    // then squares it, so the whole answer is set by how far the STEP's norm
    // happens to sit from 1. That is a property of one input, not of the
    // representation, which is why three steps are measured and not one.
    constexpr int kSteps = 1000000;

    // Gram-Schmidt: the mat3 repair, written here rather than in `math/`
    // because it is half of `orthonormalise`, which nothing in the engine has
    // needed yet. It is the thing a quaternion's one divide is competing with.
    auto gram_schmidt = [](const mat3& m) {
        const vec3 c0 = engine::normalised(m.c0);
        const vec3 c1 = engine::normalised(m.c1 - c0 * engine::dot(c0, m.c1));
        return mat3{c0, c1, engine::cross(c0, c1)};
    };

    std::printf("   step    quat raw   mat3 raw   quat tidied\n");
    float worst_tidied = 0.0f;
    for (float step_deg : {0.37f, 1.00f, 2.50f})
    {
        const quat step_q = engine::quat_from_axis_angle(
            engine::normalised(vec3{0.31f, 0.55f, -0.77f}), rad(step_deg));
        const mat3 step_m = engine::mat3_from_quat(step_q);

        quat walk_q = quat::identity();
        quat walk_t = quat::identity();
        mat3 walk_m = mat3::identity();
        for (int n = 0; n < kSteps; ++n)
        {
            walk_q = walk_q * step_q;
            walk_t = engine::renormalised_fast(walk_t * step_q);
            walk_m = walk_m * step_m;
        }
        const float d_q = orthonormality_defect(engine::mat3_from_quat(walk_q));
        const float d_t = orthonormality_defect(engine::mat3_from_quat(walk_t));
        const float d_m = orthonormality_defect(walk_m);
        worst_tidied = std::max(worst_tidied, d_t);
        std::printf("   %5.2f   %.3e  %.3e  %.3e\n",
                    static_cast<double>(step_deg), static_cast<double>(d_q),
                    static_cast<double>(d_m), static_cast<double>(d_t));
    }
    std::printf("   (tidied = renormalised_fast every step, no sqrt)\n");
    check("F.4  a tidied quaternion beats both raw walks",
          worst_tidied < 1e-5f);

    // F.4b — and THIS is the advantage, priced. Repairing a quaternion is one
    // multiply-add per component; repairing a matrix is Gram-Schmidt.
    const double t_fix_q = time_ns([&] {
        double acc = 0.0;
        for (int rep = 0; rep < kReps; ++rep)
        {
            for (int k = 0; k < kN; ++k)
            {
                const quat f = engine::renormalised_fast(qs[(k + rep) & (kN - 1)]);
                acc += static_cast<double>(f.w) + static_cast<double>(f.v.x) +
                       static_cast<double>(f.v.y) + static_cast<double>(f.v.z);
            }
        }
        return acc;
    }, static_cast<long long>(kReps) * kN, sink);

    const double t_fix_m = time_ns([&] {
        double acc = 0.0;
        for (int rep = 0; rep < kReps; ++rep)
        {
            for (int k = 0; k < kN; ++k)
            {
                const mat3 f = gram_schmidt(ms[(k + rep) & (kN - 1)]);
                acc += static_cast<double>(f.c0.x + f.c0.y + f.c0.z) +
                       static_cast<double>(f.c1.x + f.c1.y + f.c1.z) +
                       static_cast<double>(f.c2.x + f.c2.y + f.c2.z);
            }
        }
        return acc;
    }, static_cast<long long>(kReps) * kN, sink);

    std::printf("   repair   renormalised_fast  %6.3f ns\n", t_fix_q);
    std::printf("   repair   Gram-Schmidt       %6.3f ns\n", t_fix_m);
    std::printf("   ratio                       %6.2fx\n", t_fix_m / t_fix_q);
    check("F.4b repairing a quaternion is much cheaper",
          t_fix_q * 3.0 < t_fix_m);

    // F.5 — renormalised_fast, and 7.3's warning does not carry up. It gets
    // SHARPER, and in a way that indicts a different habit.
    //
    // In the plane the function at |z| = 2 returned -z: a perfect modulus
    // wearing a 180 degree error, so 7.3 said to test the ROTATION and not the
    // modulus. Here it returns -q, and -q IS q as a rotation. The worst input
    // in two dimensions is a perfect input in three — exactly unit, exactly the
    // right turn, both bit for bit.
    //
    // So the failure is NOT MONOTONIC in |q|, and a test that sampled the two
    // obvious points, 1.0 and 2.0, would certify the function completely. The
    // damage is in the middle, and `rotate` is where it lands: that function's
    // two-cross form is only equal to `q v conj(q)` on the unit sphere, so a
    // norm error becomes a POSE error rather than a scale error.
    std::printf("   |q|    norm after fix   pose error\n");
    const quat truth = engine::quat_from_axis_angle(
        vec3{0.0f, 0.0f, 1.0f}, rad(50.0f));
    float worst_mid = 0.0f;
    float at_one = 0.0f;
    float at_two = 0.0f;
    for (float s_in : {1.0f, 1.05f, 1.25f, 1.5f, 1.75f, 2.0f})
    {
        const quat fixed = engine::renormalised_fast(truth * s_in);
        const float norm_err = std::fabs(engine::length(fixed) - 1.0f);
        const float turn = deg(engine::angle_between_rotations(
            engine::mat3_from_quat(truth), engine::mat3_from_quat(fixed)));
        std::printf("   %5.3f    %.3e      %8.4f deg\n",
                    static_cast<double>(s_in), static_cast<double>(norm_err),
                    static_cast<double>(turn));
        if (s_in == 1.0f) { at_one = turn; }
        else if (s_in == 2.0f) { at_two = turn; }
        else { worst_mid = std::max(worst_mid, turn); }
    }
    check("F.5  exact at |q| = 1 AND 2, wrong in between",
          at_one < 1e-3f && at_two < 1e-3f && worst_mid > 10.0f);

    // F.6 — the corrected claim about `rotate`, stated as a measurement. The
    // literal three-quaternion sandwich DOES scale by |q|^2; the two-cross form
    // this engine ships does not, and the gap is what F.5 fell into.
    std::printf("   |q|    literal/|q|^2 err   rotate() err\n");
    bool literal_scales = true;
    bool fast_does_not = false;
    for (float s_in : {1.0f, 1.10f, 1.50f})
    {
        const quat q = truth * s_in;
        const vec3 probe{0.3f, -0.7f, 0.5f};
        const vec3 want = engine::rotate(truth, probe);
        const vec3 literal =
            (q * quat::pure(probe) * engine::conjugate(q)).v / (s_in * s_in);
        const vec3 fast = engine::rotate(q, probe);
        const float e_lit = engine::length(literal - want);
        const float e_fast = engine::length(fast - want);
        std::printf("   %5.3f    %.3e           %.3e\n",
                    static_cast<double>(s_in), static_cast<double>(e_lit),
                    static_cast<double>(e_fast));
        if (e_lit > 1e-5f) { literal_scales = false; }
        if (s_in > 1.4f && e_fast > 0.1f) { fast_does_not = true; }
    }
    check("F.6  CONTROL: the literal form scales, rotate() does not",
          literal_scales && fast_does_not);
}

// ===========================================================================
// G — against the engine's existing rotation machinery
// ===========================================================================

void section_g()
{
    std::printf("\nG. AGAINST WHAT IS ALREADY HERE\n");

    // G.1 — quat_from_euler agrees with rotation_from_euler. This is the check
    // that the two products describe the same composition, on the engine's own
    // convention (intrinsic y-x-z, active, right-handed).
    rng r;
    float worst = 0.0f;
    for (int n = 0; n < 20000; ++n)
    {
        const engine::euler_angles e{r.uniform(-k_pi, k_pi),
                                     r.uniform(-k_pi * 0.5f, k_pi * 0.5f),
                                     r.uniform(-k_pi, k_pi)};
        worst = std::max(worst, max_element_diff(
            engine::mat3_from_quat(engine::quat_from_euler(e)),
            engine::rotation_from_euler(e)));
    }
    std::printf("   worst |M(quat_euler) - R_euler|:   %.3e\n",
                static_cast<double>(worst));
    check("G.1  quat_from_euler matches rotation_from_euler",
          worst < 1e-5f);

    // G.2 — quat_x/y/z match rotation_x/y/z, which is what lets a call site
    // move over by changing four characters.
    rng r2;
    float worst_axis = 0.0f;
    for (int n = 0; n < 20000; ++n)
    {
        const float a = r2.uniform(-k_pi, k_pi);
        worst_axis = std::max({worst_axis,
            max_element_diff(engine::mat3_from_quat(engine::quat_x(a)),
                             engine::rotation_x(a)),
            max_element_diff(engine::mat3_from_quat(engine::quat_y(a)),
                             engine::rotation_y(a)),
            max_element_diff(engine::mat3_from_quat(engine::quat_z(a)),
                             engine::rotation_z(a))});
    }
    std::printf("   worst |M(quat_k) - rotation_k|:    %.3e\n",
                static_cast<double>(worst_axis));
    check("G.2  quat_x/y/z match rotation_x/y/z", worst_axis < 1e-6f);

    // G.3 — axis_angle_from_quat agrees with axis_angle_from_rotation, INCLUDING
    // near a half-turn where the matrix route has to change strategy and the
    // quaternion route does not.
    //
    // AND THE FIRST DRAFT OF THIS CHECK ASKED THE WRONG QUESTION. It compared
    // the two routes to EACH OTHER, found 0.028 degrees of disagreement at a
    // turn of 1 degree, and failed — which says only that they differ, not
    // which one is wrong. Both are measured against the axis the matrix was
    // BUILT from instead, and the answer is the interesting one: below a few
    // degrees the matrix route is the one losing the axis, for the reason 7.2
    // derived at length (it divides by 2 sin theta, formed as a difference of
    // near-equal entries), while the quaternion route reads |v| = sin(theta/2)
    // off components that ARE of that size and never subtracts anything.
    //
    // AND THE MATRIX HAS TO CARRY NOISE, or the comparison says nothing. 7.2's
    // section 8.4 made the same point: a matrix straight out of
    // `rotation_from_axis_angle` is as good as a float can be, both routes
    // recover the axis exactly, and every row reads 0.000e+00. The question is
    // what happens to an axis read off a matrix that has been THROUGH
    // something, so 1e-7 of absolute error goes into every entry first — the
    // same magnitude 7.2 used, so the two lessons' numbers are comparable.
    std::printf("   turn      matrix err    quat err\n");
    float worst_quat_axis = 0.0f;
    float worst_mat_axis = 0.0f;
    rng rn;
    for (float a : {0.05f, 1.0f, 45.0f, 119.0f, 121.0f, 170.0f, 179.9f})
    {
        const vec3 axis = engine::normalised(vec3{0.3f, -0.6f, 0.74f});
        mat3 m = engine::rotation_from_axis_angle({axis, rad(a)});
        vec3* cols[3] = {&m.c0, &m.c1, &m.c2};
        for (int c = 0; c < 3; ++c)
        {
            cols[c]->x += rn.uniform(-1e-7f, 1e-7f);
            cols[c]->y += rn.uniform(-1e-7f, 1e-7f);
            cols[c]->z += rn.uniform(-1e-7f, 1e-7f);
        }
        const auto from_m = engine::axis_angle_from_rotation(m);
        const auto from_q = engine::axis_angle_from_quat(
            engine::quat_from_rotation(m));
        // MEASURED AS A CHORD, NOT AS AN ACOS, and the first draft of this
        // check learned why the hard way: it printed 0.000e+00 in every row.
        // An axis error of 5.7e-05 rad puts the dot product at 1 - 1.6e-09,
        // which rounds to exactly 1.0f, and `acos` of exactly 1 is exactly 0.
        // That is Lesson 7.1 section 6.4's finding arriving from a third
        // direction: a quantity obtained by subtracting near-equal numbers at 1
        // is gone before the inverse trig function sees it. The chord |a - b|
        // is of the SIZE of the answer and loses nothing, and
        // `2 asin(chord/2)` turns it back into an angle exactly.
        auto err = [axis](vec3 got) {
            const vec3 aligned = (engine::dot(axis, got) < 0.0f) ? -got : got;
            const float chord = engine::length(axis - aligned);
            return deg(2.0f * std::asin(std::min(chord * 0.5f, 1.0f)));
        };
        const float em = err(from_m.value.axis);
        const float eq = err(from_q.value.axis);
        std::printf("   %7.2f   %.3e   %.3e\n", static_cast<double>(a),
                    static_cast<double>(em), static_cast<double>(eq));
        worst_mat_axis = std::max(worst_mat_axis, em);
        worst_quat_axis = std::max(worst_quat_axis, eq);
    }
    std::printf("   worst: matrix %.3e, quat %.3e\n",
                static_cast<double>(worst_mat_axis),
                static_cast<double>(worst_quat_axis));
    std::printf("   ratio at 0.05 deg: %.4f\n",
                static_cast<double>(worst_quat_axis / worst_mat_axis));
    //
    // AND THE MEASUREMENT REFUSED THE CLAIM THIS CHECK WAS WRITTEN TO MAKE.
    // The prediction was that the quaternion route would find the better axis
    // at small turns, because `|v| = sin(theta/2)` is a quantity OF THE SIZE of
    // the answer while the matrix route divides by a difference of near-equal
    // entries. The two come out equal — 3.281e-03 against 3.279e-03 at a turn
    // of 0.05 degrees, and within a factor of two everywhere else.
    //
    // The reason is worth more than the prediction was. `quat_from_rotation`'s
    // input IS a matrix: at a small turn it pivots on w, and the three vector
    // components then come off exactly the same antisymmetric differences the
    // matrix route uses. THE INFORMATION WAS ALREADY GONE BEFORE EITHER ROUTE
    // RAN. A quaternion's better conditioning is a property of HOLDING one, not
    // of extracting one, and Lesson 7.2's hole at theta = 0 is a hole in the
    // matrix that no extraction can patch.
    //
    // So what the four-candidate pivot actually buys is not accuracy. It is the
    // absence of a decision: no crossover to derive, no threshold to tune, and
    // no failure at exactly pi — which the NAIVE quaternion route does have,
    // and E.2 measures at 180 degrees of error.
    check("G.3  the two extractions are equally conditioned",
          worst_quat_axis < worst_mat_axis * 1.5f &&
          worst_mat_axis < worst_quat_axis * 1.5f);

    // G.4 — storage, in bytes. Not measured so much as stated, but stated by
    // the compiler rather than by the author.
    std::printf("   sizeof: quat %zu  mat3 %zu  axis_angle %zu\n",
                sizeof(quat), sizeof(mat3), sizeof(axis_angle));
    check("G.4  a quat is 16 bytes and a mat3 is 36",
          sizeof(quat) == 16 && sizeof(mat3) == 36);

    // G.5 — CONTROL. G.1 and G.2 would both pass if `mat3_from_quat` and
    // `rotation_*` were secretly the same function. They are not: perturb the
    // quaternion and the agreement must break by a predictable amount.
    const quat q = engine::quat_y(rad(40.0f));
    const quat nudged{q.w, q.v + vec3{0.0f, 0.001f, 0.0f}};
    const float moved = deg(engine::angle_between_rotations(
        engine::mat3_from_quat(q),
        engine::mat3_from_quat(engine::normalised(nudged))));
    std::printf("   nudging v.y by 0.001 moves the pose %.4f deg\n",
                static_cast<double>(moved));
    check("G.5  CONTROL: the agreement is not vacuous",
          moved > 0.05f && moved < 0.5f);
}

// ===========================================================================
// H — what a quaternion cannot store
// ===========================================================================
//
// Lesson 7.4 section 11. This is the section that decides when `transform`
// changes, and it exists because the change is NOT a one-line edit: the field
// called `rotation` has been holding things that are not rotations, and the
// narrower type is what finds that out.

void section_h()
{
    std::printf("\nH. WHAT A QUATERNION CANNOT HOLD\n");

    // H.1 — a mat3 can carry a scale. Round-tripping it through a quaternion
    // silently discards it, and the result is a perfectly valid rotation that
    // is simply not the transform you had.
    //
    // AND IT IS WORSE THAN "DROPPED", which is what the first draft of this
    // check assumed and what the measurement then refused. `quat_from_rotation`
    // handed 2R does not return R with the 2 discarded: the four candidates it
    // pivots on are built from the diagonal, a uniform scale of s moves them to
    // `1 + s*(...)`, and they no longer sum to 4. What comes back is not a unit
    // quaternion and its matrix is not a rotation either — determinant 1.65,
    // which is neither 8 nor 1. The failure has no clean interpretation, which
    // is precisely why it must not be reachable by accident.
    const mat3 with_scale = engine::rotation_y(rad(35.0f)) *
                            engine::scale(2.0f, 2.0f, 2.0f);
    const quat scaled_q = engine::quat_from_rotation(with_scale);
    const mat3 back = engine::mat3_from_quat(scaled_q);
    std::printf("   det before %.4f, det after %.4f\n",
                static_cast<double>(engine::determinant(with_scale)),
                static_cast<double>(engine::determinant(back)));
    std::printf("   |q| of the extraction: %.6f\n",
                static_cast<double>(engine::length(scaled_q)));
    std::printf("   pose error vs the true rotation: %.4f deg\n",
                static_cast<double>(deg(engine::angle_between_rotations(
                    engine::rotation_y(rad(35.0f)), back))));
    check("H.1  a scaled rotation extracts to neither answer",
          std::fabs(engine::determinant(with_scale) - 8.0f) < 1e-3f &&
          std::fabs(engine::determinant(back) - 1.0f) > 0.1f &&
          std::fabs(engine::length(scaled_q) - 1.0f) > 0.05f);

    // H.2 — and a NON-uniform one is worse: what comes back is not the
    // rotation part of the original at all, because the extraction's four
    // candidates are computed from a diagonal that the scale has moved.
    const mat3 squashed = engine::rotation_y(rad(35.0f)) *
                          engine::scale(1.0f, 0.4f, 1.0f);
    const mat3 squash_back = engine::mat3_from_quat(
        engine::quat_from_rotation(squashed));
    const float turned = deg(engine::angle_between_rotations(
        engine::rotation_y(rad(35.0f)), squash_back));
    std::printf("   non-uniform scale: pose moves %.4f deg\n",
                static_cast<double>(turned));
    check("H.2  a non-uniform scale also ROTATES the result",
          turned > 0.5f);

    // H.3 — the honest repair, and it is six lines: take the scale off as the
    // column lengths first, then extract. Exact for uniform and non-uniform
    // scale alike; a shear is still beyond it, and nothing here pretends
    // otherwise.
    auto decompose = [](const mat3& m, vec3& out_scale) {
        out_scale = {engine::length(m.c0), engine::length(m.c1),
                     engine::length(m.c2)};
        return engine::quat_from_rotation(mat3{m.c0 / out_scale.x,
                                               m.c1 / out_scale.y,
                                               m.c2 / out_scale.z});
    };
    vec3 recovered{};
    const quat pure_rotation = decompose(squashed, recovered);
    const float repaired = deg(engine::angle_between_rotations(
        engine::rotation_y(rad(35.0f)),
        engine::mat3_from_quat(pure_rotation)));
    std::printf("   after decomposing: scale (%.3f, %.3f, %.3f)\n",
                static_cast<double>(recovered.x),
                static_cast<double>(recovered.y),
                static_cast<double>(recovered.z));
    std::printf("   pose error now %.3e deg\n",
                static_cast<double>(repaired));
    check("H.3  taking the scale off first recovers the rotation",
          repaired < 1e-2f &&
          std::fabs(recovered.y - 0.4f) < 1e-4f);

    // H.4 — CONTROL, and the limit stated honestly: a SHEAR survives neither
    // route. Column lengths cannot see it, because a shear does not change
    // them; it changes the angles between them.
    const mat3 shear{{1.0f, 0.0f, 0.0f}, {0.30f, 0.954f, 0.0f},
                     {0.0f, 0.0f, 1.0f}};
    vec3 shear_scale{};
    const quat shear_q = decompose(shear, shear_scale);
    const float shear_err = max_element_diff(
        shear, engine::mat3_from_quat(shear_q));
    std::printf("   shear: column lengths (%.3f, %.3f, %.3f)\n",
                static_cast<double>(shear_scale.x),
                static_cast<double>(shear_scale.y),
                static_cast<double>(shear_scale.z));
    std::printf("   worst entry error after repair: %.4f\n",
                static_cast<double>(shear_err));
    check("H.4  CONTROL: a shear defeats the repair too",
          shear_err > 0.05f);
}

} // namespace

int main()
{
    std::printf("verify_74 — Lesson 7.4, Quaternions, Derived\n");

    section_a();
    section_b();
    section_c();
    section_d();
    section_e();
    section_f();
    section_g();
    section_h();

    std::printf("\n%d / %d checks passed\n", checks_passed, checks_run);
    std::printf("(sink %.3f)\n", sink);
    return checks_passed == checks_run ? 0 : 1;
}
