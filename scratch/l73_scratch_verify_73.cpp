// scratch/verify_73.cpp — every number Lesson 7.3 prints, measured rather than asserted.
//
// Build and run:  sh scratch/build_verify_73.sh
//
// Eight sections, and the order is the lesson's:
//
//   A  the quarter turn, and the multiplication table it forces
//   B  moduli multiply, arguments add — the geometry hiding in the algebra
//   C  the mat2 correspondence, both ways, including what it does to a shear
//   D  composing without trigonometry: what it costs and what it drifts
//   E  reflections, rotors, and where theta/2 actually comes from
//   F  interpolation: the right path at the wrong speed, with a closed form
//   G  the incremental circle — one multiply per point instead of two transcendentals
//   H  what the plane cannot tell us, stated precisely enough to be a prediction
//
// EVERY SECTION CARRIES A CONTROL, for the reason 7.1 states and 7.2 repeats: a
// check whose degenerate case is a pass is not a check. The sharpest ones here
// are in E and F. E's claim is that two reflections make a rotation of TWICE the
// mirror angle, so the control is a run with the mirrors in the other order,
// which must give the opposite rotation and not the same one. F's claim is that
// nlerp travels the correct path, so the control is the 7.1 measurement that
// says what an INCORRECT path looks like on the same instrument.
//
// TIMING RULES, inherited from 7.2 and obeyed here without exception:
//   - consume the WHOLE result (7.2 timed a 3x3 product at 0.41 ns by reading
//     one element of nine and letting the compiler compute only that one)
//   - VARY the input with the repetition counter, or the compiler computes the
//     answers once and replays them
//   - BEST OF THREE, never the mean: a timing is a lower bound contaminated by
//     interruptions that can only slow it down
//   - divide by the operation count and ask whether the answer is physically
//     possible before believing it

#include <engine/math/complex.hpp>
#include <engine/math/mat2.hpp>
#include <engine/math/vec2.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>

using engine::complex;
using engine::mat2;
using engine::vec2;

namespace {

constexpr float k_pi = std::numbers::pi_v<float>;
constexpr double k_pi_d = std::numbers::pi_v<double>;

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

/// Smallest signed difference between two angles, in radians.
///
/// Needed all over this file because "did these two routes produce the same
/// rotation?" is a question about angles on a circle, where 359.9999 and
/// -0.0001 are the same answer and a plain subtraction reports 360 degrees of
/// disagreement. Local to the harness rather than in the engine: the engine's
/// `angle_between` already does this for `complex`, and this is for the cases
/// where the comparison is between two raw floats.
float angle_delta(float a, float b)
{
    float d = a - b;
    while (d > k_pi)  { d -= 2.0f * k_pi; }
    while (d < -k_pi) { d += 2.0f * k_pi; }
    return d;
}

float max_element_diff(const mat2& a, const mat2& b)
{
    float worst = 0.0f;
    for (int r = 0; r < 2; ++r)
    {
        for (int c = 0; c < 2; ++c)
        {
            worst = std::max(worst, std::fabs(a.at(r, c) - b.at(r, c)));
        }
    }
    return worst;
}

/// How far `m` is from being a rotation: max |MᵀM − I| over the four entries.
///
/// The `mat2` twin of 7.2's `orthonormality_defect`, and kept in the harness for
/// the same reason that one is: it is half of `is_rotation`, which is an
/// exercise, and writing it into `math/` would solve a problem the student was
/// handed. Section D needs it to make the drift comparison fair — a `complex`
/// drifts in one number and a `mat2` drifts in four, so the two have to be asked
/// the same question rather than each asked its own.
float orthonormality_defect(const mat2& m)
{
    const mat2 should_be_identity = transpose(m) * m;
    return max_element_diff(should_be_identity, mat2::identity());
}

/// A deterministic pseudo-random stream. Same generator as 7.1 and 7.2 use, so
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

    /// Uniform in [lo, hi).
    float uniform(float lo, float hi)
    {
        const float u = static_cast<float>(next() >> 8) / 16777216.0f;
        return lo + (hi - lo) * u;
    }
};

/// Best-of-three timing of `fn`, in nanoseconds per operation.
///
/// `fn` returns a value that the caller folds into a printed sink, which is what
/// stops the optimiser deleting the whole loop. Three runs and the minimum, for
/// 7.2's reason: the first draft of that lesson's harness reported the same
/// spelling at 4.20 ns and 9.87 ns in two runs of the mean.
/// ONE WARM-UP RUN BEFORE THE TIMED ONES, and it is not superstition. Without
/// it the first measurement of a freshly-built binary reads 15-30% slow — cold
/// instruction cache, cold branch predictors, first-touch page faults on the
/// input arrays — and since `time_ns` takes the MINIMUM, one cold attempt out of
/// three does not hurt but one cold attempt out of one would. Measured across
/// five runs of this harness: without the warm-up the first run reported
/// `complex * complex` at 0.820 ns and the next four at 0.610-0.628; with it,
/// every run lands in that band. The warm-up's result still goes into the sink
/// so the compiler cannot treat it as dead.
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

// ===========================================================================
// A — the quarter turn, and the multiplication table it forces
// ===========================================================================
//
// The lesson does not define i and then discover geometry. It defines the
// quarter turn and then discovers i. This section is that argument in code: the
// only input is "turning (1,0) by a quarter circle lands on (0,1)", and
// everything else is a consequence.

void section_a()
{
    std::printf("\nA. THE QUARTER TURN\n");

    // A.1 — the single observation the whole algebra is built on.
    // Turn (1,0) a quarter circle: it lands on (0,1). Turn that again: (-1,0).
    // So "quarter turn, twice" is exactly "multiply by -1".
    const complex q = engine::complex_from_angle(0.5f * k_pi);
    const vec2 once = q * vec2{1.0f, 0.0f};
    const vec2 twice = q * (q * vec2{1.0f, 0.0f});
    std::printf("   quarter turn of (1,0):        (%.7f, %.7f)\n",
                static_cast<double>(once.x), static_cast<double>(once.y));
    std::printf("   and again:                    (%.7f, %.7f)\n",
                static_cast<double>(twice.x), static_cast<double>(twice.y));
    check("A.1  a quarter turn twice is multiplication by -1",
          std::fabs(twice.x + 1.0f) < 1e-6f && std::fabs(twice.y) < 1e-6f);

    // A.2 — so i, DEFINED as that quarter turn, squares to -1. Exactly, in
    // float, because the arithmetic is 0*0 - 1*1 and 0*1 + 1*0.
    const complex i = complex::i();
    const complex i_squared = i * i;
    std::printf("   i * i =                       %+.1f %+.1fi\n",
                static_cast<double>(i_squared.re), static_cast<double>(i_squared.im));
    check("A.2  i * i == -1 exactly",
          i_squared == complex{-1.0f, 0.0f});

    // A.3 — the rest of the table. Nothing is postulated here: each product is
    // computed by the one rule (multiply out, substitute i² = -1) and checked
    // against what the geometry says the answer has to be.
    check("A.3  1 * i == i",            (complex{1.0f, 0.0f} * i) == i);
    check("A.4  i * i * i == -i",       (i * i * i) == complex{0.0f, -1.0f});
    check("A.5  i * i * i * i == 1",    (i * i * i * i) == complex::identity());

    // A.6 — CONTROL. If the multiplication above were secretly componentwise —
    // the thing a reader might assume from the addition — then i*i would be
    // (0*0, 1*1) = (0, 1) = i, and every check above would still pass on a type
    // that does not rotate anything. So: a product whose answer distinguishes
    // the two.
    const complex a{2.0f, 3.0f};
    const complex b{4.0f, 5.0f};
    const complex ab = a * b;   // (8 - 15) + (10 + 12)i = -7 + 22i
    const complex componentwise{a.re * b.re, a.im * b.im};   // (8, 15) — wrong
    std::printf("   (2+3i)(4+5i) = %+.1f %+.1fi   (componentwise: %+.1f %+.1fi)\n",
                static_cast<double>(ab.re), static_cast<double>(ab.im),
                static_cast<double>(componentwise.re), static_cast<double>(componentwise.im));
    check("A.6  CONTROL: the product is not componentwise",
          ab == complex{-7.0f, 22.0f} && ab != componentwise);
}

// ===========================================================================
// B — moduli multiply, arguments add
// ===========================================================================

void section_b()
{
    std::printf("\nB. WHAT THE PRODUCT DOES GEOMETRICALLY\n");

    // B.1 — the worked example the lesson pushes through by hand, checked to
    // the digit. (3+4i)(1+2i) = -5+10i is exact in float: every intermediate is
    // a small integer.
    const complex z{3.0f, 4.0f};
    const complex w{1.0f, 2.0f};
    const complex zw = z * w;
    std::printf("   (3+4i)(1+2i) =                %+.1f %+.1fi\n",
                static_cast<double>(zw.re), static_cast<double>(zw.im));
    std::printf("     |z| %.7f   |w| %.7f   |z||w| %.7f\n",
                static_cast<double>(engine::length(z)),
                static_cast<double>(engine::length(w)),
                static_cast<double>(engine::length(z) * engine::length(w)));
    std::printf("                                     |zw| %.7f\n",
                static_cast<double>(engine::length(zw)));
    std::printf("     arg z %.5f   arg w %.5f   sum %.5f deg\n",
                static_cast<double>(deg(engine::angle_from_complex(z))),
                static_cast<double>(deg(engine::angle_from_complex(w))),
                static_cast<double>(deg(engine::angle_from_complex(z)
                                        + engine::angle_from_complex(w))));
    std::printf("                                    arg zw %.5f deg\n",
                static_cast<double>(deg(engine::angle_from_complex(zw))));
    check("B.1  the worked example is exact",
          zw == complex{-5.0f, 10.0f});

    // B.2 — the two claims, swept. 4,000 random pairs, moduli up to 8 so the
    // measurement is not secretly about unit numbers.
    rng r;
    float worst_modulus = 0.0f;
    float worst_argument = 0.0f;
    for (int k = 0; k < 4000; ++k)
    {
        const complex p{r.uniform(-8.0f, 8.0f), r.uniform(-8.0f, 8.0f)};
        const complex q{r.uniform(-8.0f, 8.0f), r.uniform(-8.0f, 8.0f)};
        const complex pq = p * q;

        const float predicted_modulus = engine::length(p) * engine::length(q);
        const float relative = std::fabs(engine::length(pq) - predicted_modulus)
                               / std::max(predicted_modulus, 1e-20f);
        worst_modulus = std::max(worst_modulus, relative);

        const float predicted_argument = engine::angle_from_complex(p)
                                         + engine::angle_from_complex(q);
        worst_argument = std::max(worst_argument,
                                  std::fabs(angle_delta(engine::angle_from_complex(pq),
                                                        predicted_argument)));
    }
    // NARROW ON PURPOSE. Every transcript in this file is quoted verbatim in the
    // lesson, whose output blocks scroll rather than wrap, so anything past
    // column 65 is invisible until the reader thinks to drag sideways. It also
    // makes the harness readable in an 80-column terminal, which is the reason
    // that matters to anyone who is not writing the page.
    std::printf("   4,000 random pairs:  worst |zw| rel. error  %.3e\n",
                static_cast<double>(worst_modulus));
    std::printf("                        worst arg error        %.3e deg\n",
                static_cast<double>(deg(worst_argument)));
    check("B.2  moduli multiply (relative error < 1e-6)", worst_modulus < 1e-6f);
    check("B.3  arguments add (error < 1e-4 deg)", deg(worst_argument) < 1e-4f);

    // B.4 — CONTROL for B.2/B.3. The same sweep asking whether the moduli ADD,
    // which they emphatically do not. Without this, a bug that made `length`
    // return a constant would pass B.2 silently.
    r = rng{};
    float worst_wrong = 0.0f;
    for (int k = 0; k < 4000; ++k)
    {
        const complex p{r.uniform(-8.0f, 8.0f), r.uniform(-8.0f, 8.0f)};
        const complex q{r.uniform(-8.0f, 8.0f), r.uniform(-8.0f, 8.0f)};
        const float sum = engine::length(p) + engine::length(q);
        worst_wrong = std::max(worst_wrong,
                               std::fabs(engine::length(p * q) - sum) / std::max(sum, 1e-20f));
    }
    std::printf("   CONTROL: if moduli ADDED, worst relative error would be %.3f\n",
                static_cast<double>(worst_wrong));
    check("B.4  CONTROL: moduli do not add", worst_wrong > 0.5f);

    // B.5 — a unit complex number is a rotation: it preserves lengths and the
    // angles between vectors. This is the claim that the type deserves its
    // second reading at all.
    r = rng{};
    float worst_length_change = 0.0f;
    float worst_angle_change = 0.0f;
    for (int k = 0; k < 4000; ++k)
    {
        const complex u = engine::complex_from_angle(r.uniform(-k_pi, k_pi));
        const vec2 p{r.uniform(-6.0f, 6.0f), r.uniform(-6.0f, 6.0f)};
        const vec2 q{r.uniform(-6.0f, 6.0f), r.uniform(-6.0f, 6.0f)};
        const vec2 up = u * p;
        const vec2 uq = u * q;

        worst_length_change = std::max(worst_length_change,
                                       std::fabs(engine::length(up) - engine::length(p)));
        const float before = std::atan2(p.x * q.y - p.y * q.x, engine::dot(p, q));
        const float after = std::atan2(up.x * uq.y - up.y * uq.x, engine::dot(up, uq));
        worst_angle_change = std::max(worst_angle_change,
                                      std::fabs(angle_delta(after, before)));
    }
    std::printf("   unit z preserves length to %.3e, angle to %.3e deg\n",
                static_cast<double>(worst_length_change),
                static_cast<double>(deg(worst_angle_change)));
    check("B.5  a unit complex number is a rotation",
          worst_length_change < 1e-5f && deg(worst_angle_change) < 1e-3f);
}

// ===========================================================================
// C — the mat2 correspondence
// ===========================================================================

void section_c()
{
    std::printf("\nC. COMPLEX NUMBERS AND 2x2 MATRICES ARE THE SAME ALGEBRA\n");

    // C.1 — the matrix of "multiply by z" is [[a, -b], [b, a]], and for a unit z
    // that is character-for-character `mat2`'s `rotation`. Exact, because both
    // sides call the same `std::cos` and `std::sin` on the same argument.
    float worst = 0.0f;
    for (int k = 0; k <= 720; ++k)
    {
        const float theta = rad(static_cast<float>(k) * 0.5f - 180.0f);
        worst = std::max(worst,
                         max_element_diff(engine::mat2_from_complex(
                                              engine::complex_from_angle(theta)),
                                          engine::rotation(theta)));
    }
    std::printf("   mat2_from_complex(e^{i t}) vs rotation(t), 721 angles:\n");
    std::printf("     worst element diff  %.3e\n", static_cast<double>(worst));
    check("C.1  the two spellings of a plane rotation agree exactly", worst == 0.0f);

    // C.2 — the correspondence respects multiplication, which is what makes it
    // an ISOMORPHISM rather than a coincidence of shape. M(z)·M(w) = M(zw).
    rng r;
    float worst_product = 0.0f;
    for (int k = 0; k < 2000; ++k)
    {
        const complex z{r.uniform(-4.0f, 4.0f), r.uniform(-4.0f, 4.0f)};
        const complex w{r.uniform(-4.0f, 4.0f), r.uniform(-4.0f, 4.0f)};
        worst_product = std::max(worst_product,
                                 max_element_diff(engine::mat2_from_complex(z)
                                                      * engine::mat2_from_complex(w),
                                                  engine::mat2_from_complex(z * w)));
    }
    std::printf("   M(z)M(w) vs M(zw), 2,000 pairs:  worst diff  %.3e\n",
                static_cast<double>(worst_product));
    check("C.2  the correspondence respects multiplication", worst_product < 1e-5f);

    // C.3 — round-tripping, and what happens to a matrix that is NOT of the
    // form. A shear is a perfectly good `mat2` and is not any complex number;
    // `complex_from_mat2` reads the first column and silently discards the
    // second, so the failure has to be on record rather than discovered.
    const mat2 shear = engine::shear(0.7f, 0.0f);
    const complex claimed = engine::complex_from_mat2(shear);
    const mat2 rebuilt = engine::mat2_from_complex(claimed);
    std::printf("   a shear k=0.7 read as a complex:  %+.4f %+.4fi  (%.2f deg)\n",
                static_cast<double>(claimed.re), static_cast<double>(claimed.im),
                static_cast<double>(deg(engine::angle_from_complex(claimed))));
    std::printf("   rebuilding it loses  %.4f  in the worst entry\n",
                static_cast<double>(max_element_diff(shear, rebuilt)));
    check("C.3  a shear does not survive the round trip, and by 0.7",
          std::fabs(max_element_diff(shear, rebuilt) - 0.7f) < 1e-6f);

    // C.4 — CONTROL: a rotation DOES survive it, exactly. Without this pair,
    // C.3's number could be reporting a broken round trip rather than a
    // matrix outside the family.
    float worst_round_trip = 0.0f;
    for (int k = 0; k <= 720; ++k)
    {
        const float theta = rad(static_cast<float>(k) * 0.5f - 180.0f);
        const mat2 m = engine::rotation(theta);
        worst_round_trip = std::max(worst_round_trip,
                                    max_element_diff(m, engine::mat2_from_complex(
                                                            engine::complex_from_mat2(m))));
    }
    std::printf("   CONTROL: a rotation round-trips to %.3e\n",
                static_cast<double>(worst_round_trip));
    check("C.4  CONTROL: a rotation round-trips exactly", worst_round_trip == 0.0f);

    // C.5 — the storage claim, which is the least interesting and the most
    // often decisive.
    std::printf("   sizeof(complex) = %zu bytes,  sizeof(mat2) = %zu bytes\n",
                sizeof(complex), sizeof(mat2));
    check("C.5  a complex is half the size of a mat2",
          sizeof(complex) * 2 == sizeof(mat2));
}

// ===========================================================================
// D — composing without trigonometry: cost, and drift
// ===========================================================================

void section_d()
{
    std::printf("\nD. COMPOSITION: WHAT IT COSTS AND WHAT IT DRIFTS\n");

    constexpr int k_inputs = 256;
    constexpr long long k_reps = 40000;

    // Two arrays of the SAME rotations in the two spellings, so the comparison
    // is between representations and not between input sets.
    rng r;
    complex cz[k_inputs];
    mat2 cm[k_inputs];
    float ca[k_inputs];
    for (int k = 0; k < k_inputs; ++k)
    {
        ca[k] = r.uniform(-k_pi, k_pi);
        cz[k] = engine::complex_from_angle(ca[k]);
        cm[k] = engine::rotation(ca[k]);
    }

    double sink = 0.0;

    // D.1 — complex product. The whole result is consumed and the input varies
    // with the repetition counter, per the rules at the top of this file.
    const double ns_complex = time_ns([&]() {
        double acc = 0.0;
        for (long long rep = 0; rep < k_reps; ++rep)
        {
            for (int k = 0; k < k_inputs; ++k)
            {
                const complex p = cz[k];
                const complex q = cz[(k + static_cast<int>(rep)) & (k_inputs - 1)];
                const complex prod = p * q;
                acc += static_cast<double>(prod.re) + static_cast<double>(prod.im);
            }
        }
        return acc;
    }, k_reps * k_inputs, sink);

    // D.2 — mat2 product. All four entries consumed.
    const double ns_mat2 = time_ns([&]() {
        double acc = 0.0;
        for (long long rep = 0; rep < k_reps; ++rep)
        {
            for (int k = 0; k < k_inputs; ++k)
            {
                const mat2 p = cm[k];
                const mat2 q = cm[(k + static_cast<int>(rep)) & (k_inputs - 1)];
                const mat2 prod = p * q;
                acc += static_cast<double>(prod.c0.x) + static_cast<double>(prod.c0.y)
                       + static_cast<double>(prod.c1.x) + static_cast<double>(prod.c1.y);
            }
        }
        return acc;
    }, k_reps * k_inputs, sink);

    // D.3 — the third route, and the one people reach for first: keep the angle,
    // add the angles, build the rotation when you need it. Adding is free; the
    // cost is that you cannot USE the result without trig, so the fair unit of
    // work is "compose and be ready to apply".
    const double ns_angle = time_ns([&]() {
        double acc = 0.0;
        for (long long rep = 0; rep < k_reps; ++rep)
        {
            for (int k = 0; k < k_inputs; ++k)
            {
                const float sum = ca[k] + ca[(k + static_cast<int>(rep)) & (k_inputs - 1)];
                const complex built = engine::complex_from_angle(sum);
                acc += static_cast<double>(built.re) + static_cast<double>(built.im);
            }
        }
        return acc;
    }, k_reps * k_inputs, sink);

    std::printf("   compose, %lld operations each, best of three:\n",
                k_reps * k_inputs);
    std::printf("     complex * complex        %6.3f ns\n", ns_complex);
    std::printf("     mat2 * mat2              %6.3f ns   (%.2fx)\n",
                ns_mat2, ns_mat2 / ns_complex);
    std::printf("     add angles, then trig    %6.3f ns   (%.2fx)\n",
                ns_angle, ns_angle / ns_complex);
    check("D.1  the complex product is the cheapest of the three",
          ns_complex < ns_mat2 && ns_complex < ns_angle);

    // D.4 — applying to a vector, which is where the saving ISN'T. Same shape
    // of loop; the honest half of the comparison.
    vec2 pts[k_inputs];
    for (int k = 0; k < k_inputs; ++k)
    {
        pts[k] = vec2{r.uniform(-3.0f, 3.0f), r.uniform(-3.0f, 3.0f)};
    }

    const double ns_apply_complex = time_ns([&]() {
        double acc = 0.0;
        for (long long rep = 0; rep < k_reps; ++rep)
        {
            for (int k = 0; k < k_inputs; ++k)
            {
                const vec2 out = cz[k] * pts[(k + static_cast<int>(rep)) & (k_inputs - 1)];
                acc += static_cast<double>(out.x) + static_cast<double>(out.y);
            }
        }
        return acc;
    }, k_reps * k_inputs, sink);

    const double ns_apply_mat2 = time_ns([&]() {
        double acc = 0.0;
        for (long long rep = 0; rep < k_reps; ++rep)
        {
            for (int k = 0; k < k_inputs; ++k)
            {
                const vec2 out = cm[k] * pts[(k + static_cast<int>(rep)) & (k_inputs - 1)];
                acc += static_cast<double>(out.x) + static_cast<double>(out.y);
            }
        }
        return acc;
    }, k_reps * k_inputs, sink);

    std::printf("   apply to a point:\n");
    std::printf("     complex * vec2           %6.3f ns\n", ns_apply_complex);
    std::printf("     mat2 * vec2              %6.3f ns   (ratio %.2f)\n",
                ns_apply_mat2, ns_apply_mat2 / ns_apply_complex);
    const double apply_ratio = ns_apply_mat2 / ns_apply_complex;
    check("D.2  applying costs the same either way (within 15%)",
          apply_ratio > 0.85 && apply_ratio < 1.15);

    // D.5 — drift. Compose a million rotations and ask each representation how
    // far from being a rotation it has become. The two are asked the SAME
    // question (max |MᵀM − I|) rather than each asked its own, because "how big
    // is the modulus error" and "how non-orthonormal is the matrix" are not
    // comparable numbers.
    constexpr int k_steps = 1000000;
    r = rng{};
    complex acc_z = complex::identity();
    mat2 acc_m = mat2::identity();
    for (int k = 0; k < k_steps; ++k)
    {
        const float theta = r.uniform(-k_pi, k_pi);
        acc_z = acc_z * engine::complex_from_angle(theta);
        acc_m = acc_m * engine::rotation(theta);
    }
    const float defect_z = orthonormality_defect(engine::mat2_from_complex(acc_z));
    const float defect_m = orthonormality_defect(acc_m);
    std::printf("   after %d composed rotations:\n", k_steps);
    std::printf("     complex:  |z|-1 %+.3e,  as a defect %.3e\n",
                static_cast<double>(engine::length(acc_z) - 1.0f),
                static_cast<double>(defect_z));
    std::printf("     mat2:     defect %.3e   (ratio %.2fx)\n",
                static_cast<double>(defect_m),
                static_cast<double>(defect_m / std::max(defect_z, 1e-30f)));
    check("D.3  both drift, and the matrix drifts further",
          defect_m > defect_z);

    // D.6 — and what it costs to fix. `renormalised_fast` has no sqrt and no
    // divide; Gram-Schmidt on a 2x2 has both.
    const complex fixed = engine::renormalised_fast(acc_z);
    std::printf("   renormalised_fast leaves |z| - 1 = %+.3e\n",
                static_cast<double>(engine::length(fixed) - 1.0f));
    check("D.4  the cheap renormalise recovers unit length",
          std::fabs(engine::length(fixed) - 1.0f) < 1e-6f);

    // D.7 — the cheap renormalise, characterised, and the first draft of this
    // block got it WRONG in a way worth keeping. It is a Taylor series about
    // |z|² = 1, so the residual goes as the square of the drift it is handed and
    // it must fall apart far from unit length. The obvious way to show that is to
    // watch the modulus — and the modulus says it works perfectly at |z| = 2,
    // because `(3 − 4)/2 = −0.5` scales a length-2 number to length 1 exactly.
    //
    // It also turns it round by 180°. **A rotation is not its modulus**, and a
    // test that only looked at the length would have certified a function that
    // reverses the object. So both columns are printed, and the check is on the
    // one that matters.
    const complex truth = engine::complex_from_angle(0.7f);
    std::printf("   renormalised_fast, by modulus and by rotation:\n");
    std::printf("     |z|-1 in    after\n");
    for (const float eps : {1e-6f, 1e-4f, 1e-3f, 1e-2f, 1e-1f, 1.0f})
    {
        const complex drifted = truth * (1.0f + eps);
        const complex fixed = engine::renormalised_fast(drifted);
        const float before = std::fabs(engine::length(drifted) - 1.0f);
        const float after = std::fabs(engine::length(fixed) - 1.0f);
        const float turned = std::fabs(deg(engine::angle_between(truth, fixed)));
        std::printf("     %.1e  %.3e   rotation moved %8.3f deg  (%s)\n",
                    static_cast<double>(before), static_cast<double>(after),
                    static_cast<double>(turned),
                    (turned < 1e-3f) ? "ok" : "BROKEN");
    }
    const complex far_off = truth * 2.0f;
    const complex far_fixed = engine::renormalised_fast(far_off);
    check("D.5  at |z| = 2 modulus passes, rotation is 180 deg out",
          std::fabs(engine::length(far_fixed) - 1.0f) < 1e-6f
              && std::fabs(std::fabs(deg(engine::angle_between(truth, far_fixed))) - 180.0f)
                     < 1e-2f);

    std::printf("   (timing sink, ignore: %.3e)\n", sink);
}

// ===========================================================================
// E — reflections, rotors, and where theta/2 comes from
// ===========================================================================

void section_e()
{
    std::printf("\nE. TWO MIRRORS MAKE A TURN OF TWICE THE ANGLE BETWEEN THEM\n");

    // E.1 — two routes to one reflection. The complex route is m² v̄; the vector
    // route is `vec2`'s `reflect`, which takes the mirror's NORMAL, so the
    // mirror direction has to be turned a quarter circle first. Two routes to
    // one answer is a check; one route is a hope.
    rng r;
    float worst_reflect = 0.0f;
    for (int k = 0; k < 4000; ++k)
    {
        const float alpha = r.uniform(-k_pi, k_pi);
        const complex m = engine::complex_from_angle(alpha);
        const vec2 v{r.uniform(-5.0f, 5.0f), r.uniform(-5.0f, 5.0f)};

        const vec2 by_complex = engine::reflect_in_line(v, m);
        const vec2 by_vector = engine::reflect(v, engine::perpendicular(
                                                      engine::vec2_from_complex(m)));
        worst_reflect = std::max(worst_reflect, engine::distance(by_complex, by_vector));
    }
    std::printf("   m^2 conj(v) vs reflect(v, perpendicular(m)), 4,000 cases:\n");
    std::printf("     worst  %.3e\n", static_cast<double>(worst_reflect));
    check("E.1  the two spellings of a mirror agree", worst_reflect < 1e-5f);

    // E.2 — the worked example the lesson walks by hand. Mirrors at 20 and 50
    // degrees; start at angle 0. First mirror sends 0 to 40, second sends 40 to
    // 60. Net: a 60 degree rotation, which is twice the 30 degrees between the
    // mirrors.
    const complex m0 = engine::complex_from_angle(rad(20.0f));
    const complex m1 = engine::complex_from_angle(rad(50.0f));
    const vec2 start{1.0f, 0.0f};
    const vec2 after_first = engine::reflect_in_line(start, m0);
    const vec2 after_second = engine::reflect_in_line(after_first, m1);
    std::printf("   0.000 deg  -[mirror 20]->  %.3f deg  -[mirror 50]->  %.3f deg\n",
                static_cast<double>(deg(std::atan2(after_first.y, after_first.x))),
                static_cast<double>(deg(std::atan2(after_second.y, after_second.x))));
    check("E.2  the worked example lands on 60 degrees",
          std::fabs(deg(std::atan2(after_second.y, after_second.x)) - 60.0f) < 1e-3f);

    // E.3 — the rotor. `rotor_from_mirrors` returns m1·conj(m0), which is the
    // rotation from one mirror to the other — HALF the rotation the pair
    // performs. This is the θ/2 that Lesson 7.4's quaternions are built from,
    // and here it is in the plane, measurable.
    const complex rotor = engine::rotor_from_mirrors(m0, m1);
    std::printf("   rotor %.3f deg;  the rotation it generates %.3f deg\n",
                static_cast<double>(deg(engine::angle_from_complex(rotor))),
                static_cast<double>(deg(engine::angle_from_complex(rotor * rotor))));
    check("E.3  the rotor carries half the angle",
          std::fabs(deg(engine::angle_from_complex(rotor)) - 30.0f) < 1e-3f
              && std::fabs(deg(engine::angle_from_complex(rotor * rotor)) - 60.0f) < 1e-3f);

    // E.4 — swept. For any two mirrors, reflecting in one then the other is a
    // rotation by twice the angle between them, and applying the rotor twice is
    // that rotation.
    r = rng{};
    float worst_double = 0.0f;
    float worst_apply = 0.0f;
    for (int k = 0; k < 4000; ++k)
    {
        const float alpha = r.uniform(-k_pi, k_pi);
        const float beta = r.uniform(-k_pi, k_pi);
        const complex a = engine::complex_from_angle(alpha);
        const complex b = engine::complex_from_angle(beta);
        const vec2 v{r.uniform(-5.0f, 5.0f), r.uniform(-5.0f, 5.0f)};

        const vec2 twice_reflected = engine::reflect_in_line(engine::reflect_in_line(v, a), b);
        const vec2 by_rotation = engine::complex_from_angle(2.0f * (beta - alpha)) * v;
        worst_double = std::max(worst_double, engine::distance(twice_reflected, by_rotation));

        const vec2 by_rotor = engine::apply_rotor(engine::rotor_from_mirrors(a, b), v);
        worst_apply = std::max(worst_apply, engine::distance(twice_reflected, by_rotor));
    }
    std::printf("   4,000 mirror pairs:  vs rotation by 2(b-a)  %.3e\n",
                static_cast<double>(worst_double));
    std::printf("                        vs apply_rotor       %.3e\n",
                static_cast<double>(worst_apply));
    check("E.4  two mirrors phi apart rotate by 2 phi", worst_double < 1e-4f);
    check("E.5  apply_rotor is that rotation", worst_apply < 1e-4f);

    // E.6 — CONTROL, and the sharpest one in this file. The claim is about the
    // ORDER of the mirrors. Reflect in m1 then m0 and you get the INVERSE
    // rotation, not the same one — so the two answers must sum to zero and must
    // not be equal. If E.4 were passing because both sides were secretly the
    // identity, this would pass too and it would be the only warning.
    const vec2 other_order = engine::reflect_in_line(engine::reflect_in_line(start, m1), m0);
    const float forward_angle = deg(std::atan2(after_second.y, after_second.x));
    const float reverse_angle = deg(std::atan2(other_order.y, other_order.x));
    std::printf("   CONTROL: the other order gives %.3f deg, not %.3f deg\n",
                static_cast<double>(reverse_angle), static_cast<double>(forward_angle));
    check("E.6  CONTROL: reflections do not commute; order reverses",
          std::fabs(forward_angle + reverse_angle) < 1e-3f
              && std::fabs(forward_angle - reverse_angle) > 1.0f);

    // E.7 — the double cover, visible in the plane. R and -R are different
    // rotors (different pairs of mirrors) and generate the same rotation,
    // exactly, because (-R)² = R² is the same four products of the same floats.
    const complex negated = -rotor;
    std::printf("   rotors %.3f deg and %.3f deg generate the SAME rotation\n",
                static_cast<double>(deg(engine::angle_from_complex(rotor))),
                static_cast<double>(deg(engine::angle_from_complex(negated))));
    check("E.7  two rotors per rotation, and they agree exactly",
          (negated * negated) == (rotor * rotor) && negated != rotor);
}

// ===========================================================================
// F — interpolation: the right path at the wrong speed
// ===========================================================================

void section_f()
{
    std::printf("\nF. SLERP, NLERP, AND THE DIFFERENCE BETWEEN A PATH AND A SCHEDULE\n");

    // F.1 — slerp moves at constant angular speed, by construction. Measured
    // rather than argued: walk t from 0 to 1 and check every step covers the
    // same angle.
    const complex a = engine::complex_from_angle(rad(-70.0f));
    const complex b = engine::complex_from_angle(rad(75.0f));
    const float span = engine::angle_between(a, b);

    constexpr int k_steps = 2000;
    float slerp_min = 1e30f;
    float slerp_max = 0.0f;
    float slerp_total = 0.0f;
    complex previous = a;
    for (int k = 1; k <= k_steps; ++k)
    {
        const float t = static_cast<float>(k) / static_cast<float>(k_steps);
        const complex now = engine::complex_slerp(a, b, t);
        const float step = std::fabs(engine::angle_between(previous, now));
        slerp_min = std::min(slerp_min, step);
        slerp_max = std::max(slerp_max, step);
        slerp_total += step;
        previous = now;
    }
    std::printf("   arc %.3f deg, %d steps:\n", static_cast<double>(deg(span)), k_steps);
    std::printf("     slerp  path %.4f deg   speed %.6f   excess  +0.00%%\n",
                static_cast<double>(deg(slerp_total)),
                static_cast<double>(slerp_max / slerp_min));
    check("F.1  slerp travels exactly the arc",
          std::fabs(deg(slerp_total) - std::fabs(deg(span))) < 1e-2f);
    check("F.2  slerp's speed is constant to within 1e-3",
          slerp_max / slerp_min - 1.0f < 1e-3f);

    // F.2 — nlerp. Same endpoints, same instrument. The claim is that the PATH
    // is identical and the SCHEDULE is not.
    float nlerp_min = 1e30f;
    float nlerp_max = 0.0f;
    float nlerp_total = 0.0f;
    previous = a;
    for (int k = 1; k <= k_steps; ++k)
    {
        const float t = static_cast<float>(k) / static_cast<float>(k_steps);
        const complex now = engine::complex_nlerp(a, b, t);
        const float step = std::fabs(engine::angle_between(previous, now));
        nlerp_min = std::min(nlerp_min, step);
        nlerp_max = std::max(nlerp_max, step);
        nlerp_total += step;
        previous = now;
    }
    // In DEGREES on both sides. The first draft compared a radian total against a
    // degree span and reported "-98.25% excess turning", which is not a thing a
    // path can do: a negative excess means the journey was shorter than the
    // shortest journey. A percentage that cannot physically be negative is worth
    // an assert, and F.3 below is one.
    const float excess = 100.0f * (deg(nlerp_total) - std::fabs(deg(span)))
                         / std::fabs(deg(span));
    std::printf("     nlerp  path %.4f deg   speed %.6f   excess %+.2f%%\n",
                static_cast<double>(deg(nlerp_total)),
                static_cast<double>(nlerp_max / nlerp_min),
                static_cast<double>(excess));
    check("F.3  nlerp wastes no turning (excess below 0.01%)",
          std::fabs(excess) < 0.01f);
    check("F.4  nlerp's speed is NOT constant", nlerp_max / nlerp_min > 1.05f);

    // F.3 — the closed form. Derived in the lesson: for an arc of Omega, nlerp's
    // angular speed is 2 tan(Omega/2) at the midpoint and sin(Omega) at the
    // ends, so the ratio is exactly sec²(Omega/2). Measured against a sweep at
    // seven arcs, because a formula checked at one point is a coincidence.
    std::printf("   nlerp speed ratio vs sec^2(Omega/2):\n");
    std::printf("      Omega      measured        sec^2   relative\n");
    float worst_ratio_error = 0.0f;
    for (const float omega_deg : {10.0f, 30.0f, 60.0f, 90.0f, 120.0f, 150.0f, 179.0f})
    {
        const float omega = rad(omega_deg);
        const complex p = engine::complex_from_angle(-0.5f * omega);
        const complex q = engine::complex_from_angle(0.5f * omega);

        float lo = 1e30f;
        float hi = 0.0f;
        complex prev = p;
        for (int k = 1; k <= 20000; ++k)
        {
            const float t = static_cast<float>(k) / 20000.0f;
            const complex now = engine::complex_nlerp(p, q, t);
            const float step = std::fabs(engine::angle_between(prev, now));
            lo = std::min(lo, step);
            hi = std::max(hi, step);
            prev = now;
        }
        const double measured = static_cast<double>(hi / lo);
        const double c = std::cos(static_cast<double>(omega) * 0.5);
        const double closed = 1.0 / (c * c);
        const double rel = std::fabs(measured - closed) / closed;
        worst_ratio_error = std::max(worst_ratio_error, static_cast<float>(rel));
        std::printf("     %6.1f deg  %11.3f  %11.3f   %.2e\n",
                    static_cast<double>(omega_deg), measured, closed, rel);
    }
    check("F.5  the sec^2(Omega/2) closed form holds across the range",
          worst_ratio_error < 0.02f);

    // F.4 — how far apart the two actually get, which is the number an animation
    // programmer cares about: at the same t, how wrong is the cheap one?
    std::printf("   worst orientation gap between nlerp and slerp at the same t:\n");
    for (const float omega_deg : {10.0f, 30.0f, 60.0f, 90.0f, 120.0f, 150.0f, 179.0f})
    {
        const float omega = rad(omega_deg);
        const complex p = engine::complex_from_angle(-0.5f * omega);
        const complex q = engine::complex_from_angle(0.5f * omega);
        float gap = 0.0f;
        for (int k = 0; k <= 20000; ++k)
        {
            const float t = static_cast<float>(k) / 20000.0f;
            gap = std::max(gap, std::fabs(deg(engine::angle_between(
                                     engine::complex_slerp(p, q, t),
                                     engine::complex_nlerp(p, q, t)))));
        }
        std::printf("     Omega %6.1f deg   gap %8.4f deg   (%.2f%% of the arc)\n",
                    static_cast<double>(omega_deg), static_cast<double>(gap),
                    static_cast<double>(100.0f * gap / omega_deg));
    }

    // F.5 — CONTROL, and it is 7.1's. In the plane, lerping the single angle IS
    // the geodesic — which is exactly why 7.1's first attempt at measuring
    // Euler-lerp waste reported nothing and had to be redone in three angles.
    // So the control has to come from outside: the instrument that reports
    // 0.00% here must report something else on a path that really is wasteful.
    // A lerp through the ORIGIN-CROSSING chord (un-normalised, then normalised
    // only at the end) is such a path.
    float angle_lerp_total = 0.0f;
    previous = a;
    for (int k = 1; k <= k_steps; ++k)
    {
        const float t = static_cast<float>(k) / static_cast<float>(k_steps);
        const complex now = engine::complex_from_angle(
            engine::angle_from_complex(a) + t * span);
        angle_lerp_total += std::fabs(engine::angle_between(previous, now));
        previous = now;
    }
    std::printf("   CONTROL: the single angle lerped travels %.4f deg\n",
                static_cast<double>(deg(angle_lerp_total)));
    check("F.6  CONTROL: one angle lerped is already a geodesic",
          std::fabs(deg(angle_lerp_total) - std::fabs(deg(span))) < 1e-2f);

    // F.6 — the wasteful control the plane can still provide: a path that visits
    // the wrong side of the circle. Going the long way round is 360 - Omega
    // degrees of turning for the same endpoints, and the instrument had better
    // say so, or its 0.00% above means nothing.
    float long_way_total = 0.0f;
    previous = a;
    for (int k = 1; k <= k_steps; ++k)
    {
        const float t = static_cast<float>(k) / static_cast<float>(k_steps);
        const float long_span = (span > 0.0f) ? span - 2.0f * k_pi : span + 2.0f * k_pi;
        const complex now = engine::complex_from_angle(
            engine::angle_from_complex(a) + t * long_span);
        long_way_total += std::fabs(engine::angle_between(previous, now));
        previous = now;
    }
    const float long_excess = 100.0f * (deg(long_way_total) - std::fabs(deg(span)))
                              / std::fabs(deg(span));
    std::printf("   CONTROL: the long way round travels %.4f deg (%+.1f%%)\n",
                static_cast<double>(deg(long_way_total)), static_cast<double>(long_excess));
    // Checked against the exact answer rather than a guessed threshold. The long
    // way round an arc of Omega is 360 - Omega degrees, so this control knows
    // what it should read and not merely that it should read something large.
    // (The first draft asked for "more than 50% excess" and got 48.3%, which is
    // the right answer failing an arbitrary bar.)
    check("F.7  CONTROL: the long way round is exactly 360 - Omega",
          std::fabs(deg(long_way_total) - (360.0f - std::fabs(deg(span)))) < 1e-2f);
    check("F.8  CONTROL: no path can have negative excess", excess > -1e-3f);
}

// ===========================================================================
// G — the incremental circle
// ===========================================================================

void section_g()
{
    std::printf("\nG. GENERATING A CIRCLE: TWO TRANSCENDENTALS, OR ONE MULTIPLY\n");

    // The engine draws circles in `debug_lines::sphere` with a `cos` and a `sin`
    // per endpoint — three great circles times `segments` points times two
    // transcendentals each. The alternative is one complex multiply per point.
    // This section measures both halves of that trade: what it saves, and what
    // it costs in accuracy, because the recurrence accumulates error and the
    // trig does not.
    constexpr int k_points = 4096;
    constexpr long long k_reps = 400;
    double sink = 0.0;

    const double ns_trig = time_ns([&]() {
        double acc = 0.0;
        for (long long rep = 0; rep < k_reps; ++rep)
        {
            const float phase = static_cast<float>(rep) * 1e-4f;
            for (int k = 0; k < k_points; ++k)
            {
                const float t = phase + 2.0f * k_pi * static_cast<float>(k)
                                            / static_cast<float>(k_points);
                acc += static_cast<double>(std::cos(t)) + static_cast<double>(std::sin(t));
            }
        }
        return acc;
    }, k_reps * k_points, sink);

    const double ns_step = time_ns([&]() {
        double acc = 0.0;
        for (long long rep = 0; rep < k_reps; ++rep)
        {
            const float phase = static_cast<float>(rep) * 1e-4f;
            const complex step = engine::complex_from_angle(
                2.0f * k_pi / static_cast<float>(k_points));
            complex z = engine::complex_from_angle(phase);
            for (int k = 0; k < k_points; ++k)
            {
                acc += static_cast<double>(z.re) + static_cast<double>(z.im);
                z = z * step;
            }
        }
        return acc;
    }, k_reps * k_points, sink);

    // G.1b — the same recurrence, four circles interleaved. THIS IS THE ROW THAT
    // EXPLAINS THE OTHER TWO. `z = z * step` is a serial dependency chain: each
    // multiply must wait for the previous one to retire, so the loop measures
    // LATENCY. The trig loop has no such chain — every point is independent —
    // so it measures THROUGHPUT and the machine overlaps as many as it likes.
    // Running four independent chains at once gives the recurrence the same
    // freedom, and the difference between this row and the one above is the
    // dependency chain, priced.
    const double ns_step4 = time_ns([&]() {
        double acc = 0.0;
        for (long long rep = 0; rep < k_reps; ++rep)
        {
            const float phase = static_cast<float>(rep) * 1e-4f;
            const complex step = engine::complex_from_angle(
                2.0f * k_pi / static_cast<float>(k_points));
            complex z0 = engine::complex_from_angle(phase);
            complex z1 = engine::complex_from_angle(phase + 1.0f);
            complex z2 = engine::complex_from_angle(phase + 2.0f);
            complex z3 = engine::complex_from_angle(phase + 3.0f);
            for (int k = 0; k < k_points; k += 4)
            {
                acc += static_cast<double>(z0.re) + static_cast<double>(z0.im)
                       + static_cast<double>(z1.re) + static_cast<double>(z1.im)
                       + static_cast<double>(z2.re) + static_cast<double>(z2.im)
                       + static_cast<double>(z3.re) + static_cast<double>(z3.im);
                z0 = z0 * step;
                z1 = z1 * step;
                z2 = z2 * step;
                z3 = z3 * step;
            }
        }
        return acc;
    }, k_reps * k_points, sink);

    std::printf("   %d points per circle, %lld circles, best of three:\n",
                k_points, k_reps);
    std::printf("     cos + sin per point      %6.3f ns   independent\n", ns_trig);
    std::printf("     one complex multiply     %6.3f ns   (%.2fx)   serial chain\n",
                ns_step, ns_trig / ns_step);
    std::printf("     four chains interleaved  %6.3f ns   (%.2fx)   chain broken\n",
                ns_step4, ns_trig / ns_step4);
    std::printf("     (D.1's INDEPENDENT complex product: 4-6x a cos+sin pair)\n");
    check("G.1  the recurrence is faster than two transcendentals",
          ns_step < ns_trig);
    check("G.2  most of its advantage is eaten by the chain",
          ns_step4 < ns_step);

    // G.2 — and what it costs. The recurrence multiplies rounding error by
    // itself `n` times; the closed form does not. Measured at four lengths,
    // with and without the cheap renormalise, against `double` truth.
    std::printf("   vs a double-precision walk:  angle error (deg)     |z| - 1\n");
    std::printf("        points       plain    tidied      plain    tidied\n");
    for (const int n : {64, 4096, 262144, 16777216})
    {
        const float d_theta = static_cast<float>(2.0 * k_pi_d / static_cast<double>(n));
        const complex step = engine::complex_from_angle(d_theta);

        complex plain = complex::identity();
        complex tidied = complex::identity();
        float worst_plain = 0.0f;
        float worst_tidied = 0.0f;
        float worst_modulus = 0.0f;
        float worst_modulus_tidied = 0.0f;
        for (int k = 1; k <= n; ++k)
        {
            plain = plain * step;
            tidied = engine::renormalised_fast(tidied * step);

            // Sample rather than compare every point: 16.7 million atan2 calls
            // is a minute of wall clock for a number that does not change.
            if ((k & 1023) == 0 || k == n)
            {
                const double truth = 2.0 * k_pi_d * static_cast<double>(k)
                                     / static_cast<double>(n);
                const float truth_f = static_cast<float>(
                    truth - 2.0 * k_pi_d * std::floor(truth / (2.0 * k_pi_d) + 0.5));
                worst_plain = std::max(worst_plain,
                                       std::fabs(angle_delta(
                                           engine::angle_from_complex(plain), truth_f)));
                worst_tidied = std::max(worst_tidied,
                                        std::fabs(angle_delta(
                                            engine::angle_from_complex(tidied), truth_f)));
                worst_modulus = std::max(worst_modulus,
                                         std::fabs(engine::length(plain) - 1.0f));
                worst_modulus_tidied = std::max(worst_modulus_tidied,
                                                std::fabs(engine::length(tidied) - 1.0f));
            }
        }
        // NARROW ON PURPOSE. This table is quoted verbatim in the lesson, whose
        // output blocks scroll rather than wrap (Conventions §10), so a line
        // wider than the column silently loses its right-hand end — and the
        // right-hand end here is the |z|-1 pair, which is the finding. 72
        // columns fits.
        std::printf("   %10d  %9.2e %9.2e  %9.2e %9.2e\n",
                    n, static_cast<double>(deg(worst_plain)),
                    static_cast<double>(deg(worst_tidied)),
                    static_cast<double>(worst_modulus),
                    static_cast<double>(worst_modulus_tidied));
    }

    // G.3 — the decision this supports, stated as a threshold rather than a
    // preference. A debug sphere is 3 x 48 points; the error there is the
    // question, not the speed.
    constexpr int k_debug_segments = 48;
    const complex step = engine::complex_from_angle(2.0f * k_pi
                                                    / static_cast<float>(k_debug_segments));
    complex z = complex::identity();
    float worst_debug = 0.0f;
    for (int k = 1; k <= k_debug_segments; ++k)
    {
        z = z * step;
        const double truth = 2.0 * k_pi_d * static_cast<double>(k)
                             / static_cast<double>(k_debug_segments);
        const float truth_f = static_cast<float>(
            truth - 2.0 * k_pi_d * std::floor(truth / (2.0 * k_pi_d) + 0.5));
        worst_debug = std::max(worst_debug,
                               std::fabs(angle_delta(engine::angle_from_complex(z), truth_f)));
    }
    std::printf("   at %d segments (the debug sphere) the error is %.3e deg\n",
                k_debug_segments, static_cast<double>(deg(worst_debug)));
    check("G.3  at debug-draw sizes the error is under 1e-4 deg",
          deg(worst_debug) < 1e-4f);

    // G.4 — CONTROL. The recurrence must actually be accumulating; if `step`
    // were the identity this whole section would report zeroes and pass.
    check("G.4  CONTROL: the recurrence moves",
          std::fabs(deg(engine::angle_from_complex(step)) - 7.5f) < 1e-3f);

    std::printf("   (timing sink, ignore: %.3e)\n", sink);
}

// ===========================================================================
// H — what the plane cannot tell us
// ===========================================================================

void section_h()
{
    std::printf("\nH. WHERE THE PLANE RUNS OUT\n");

    // H.1 — the sandwich does nothing here. z v conj(z) = |z|² v, exactly,
    // because the multiplication commutes and conj(z)·z is a real number. Every
    // quaternion text writes rotation as q v q⁻¹; try that in the plane and you
    // get the identity. The reason is commutativity and nothing else — which is
    // a prediction about Lesson 7.4, not a curiosity.
    rng r;
    float worst_sandwich = 0.0f;
    for (int k = 0; k < 4000; ++k)
    {
        const complex z = engine::complex_from_angle(r.uniform(-k_pi, k_pi));
        const vec2 v{r.uniform(-5.0f, 5.0f), r.uniform(-5.0f, 5.0f)};
        const complex sandwiched = z * engine::complex_from_vec2(v) * engine::conjugate(z);
        worst_sandwich = std::max(worst_sandwich,
                                  engine::distance(engine::vec2_from_complex(sandwiched), v));
    }
    std::printf("   z v conj(z) vs v, 4,000 cases:  worst deviation %.3e\n",
                static_cast<double>(worst_sandwich));
    check("H.1  the two-sided form is the identity in the plane",
          worst_sandwich < 1e-5f);

    // H.2 — CONTROL, and the one that makes H.1 mean something. The two-sided
    // form with the SAME factor on both sides — the rotor form — is emphatically
    // not the identity: it is the double-angle rotation of section E.
    float worst_rotor_effect = 0.0f;
    r = rng{};
    for (int k = 0; k < 4000; ++k)
    {
        const float theta = r.uniform(-k_pi, k_pi);
        const complex z = engine::complex_from_angle(theta);
        const vec2 v{r.uniform(-5.0f, 5.0f), r.uniform(-5.0f, 5.0f)};
        const vec2 out = engine::apply_rotor(z, v);
        const float turned = std::fabs(angle_delta(std::atan2(out.y, out.x),
                                                   std::atan2(v.y, v.x)));
        worst_rotor_effect = std::max(worst_rotor_effect,
                                      std::fabs(turned - std::fabs(angle_delta(
                                                             2.0f * theta, 0.0f))));
    }
    std::printf("   CONTROL: z v z turns by 2 theta, to %.3e deg\n",
                static_cast<double>(deg(worst_rotor_effect)));
    check("H.2  CONTROL: the same factor twice rotates by 2 theta",
          deg(worst_rotor_effect) < 1e-3f);

    // H.3 — the algebra that is forced. Suppose a three-dimensional number
    // system spanned by 1, i, j with i² = j² = -1 and an associative product.
    // Write ij = a + bi + cj. Multiply on the left by i:
    //
    //     i(ij) = (ii)j = -j                             [associativity]
    //     i(a + bi + cj) = -b + ca + (a + cb)i + c² j    [expanding]
    //
    // Matching the j coefficient gives c² = -1, with c real. There is no such c,
    // so ij cannot lie in the span of 1, i and j: a FOURTH basis element is
    // forced, and it is not a design choice.
    //
    // This is a symbolic argument and cannot be "measured", so what the harness
    // does instead is the honest thing — it searches. If some (a, b, c) did
    // work, a fine enough sweep would find a residual heading for zero; the
    // residual instead bottoms out at exactly 1, which is |c² + 1| at c = 0.
    float best_residual = 1e30f;
    float best_c = 0.0f;
    for (int ic = -20000; ic <= 20000; ++ic)
    {
        const float c = static_cast<float>(ic) * 0.001f;
        const float residual = std::fabs(c * c + 1.0f);
        if (residual < best_residual) { best_residual = residual; best_c = c; }
    }
    std::printf("   searching c in [-20, 20] for c^2 = -1:\n");
    std::printf("     best residual %.4f at c = %.3f\n",
                static_cast<double>(best_residual), static_cast<double>(best_c));
    check("H.3  no real c has c^2 = -1: the 4th dimension is forced",
          best_residual >= 1.0f - 1e-6f);

    // H.4 — the prediction that Lesson 7.4 will have to keep. Stated as numbers
    // so it can be checked rather than remembered.
    std::printf("   PREDICTION for 7.4:  4 floats, cos(t/2) + sin(t/2)n\n");
    std::printf("                        the product must NOT commute\n");
    std::printf("                        the sandwich must be q v conj(q)\n");
}

} // namespace

int main()
{
    std::printf("verify_73 — Lesson 7.3, complex numbers rotate the plane\n");
    std::printf("=========================================================\n");

    section_a();
    section_b();
    section_c();
    section_d();
    section_e();
    section_f();
    section_g();
    section_h();

    std::printf("\n%d/%d checks passed\n", checks_passed, checks_run);
    return (checks_passed == checks_run) ? 0 : 1;
}
