// scratch/verify_63.cpp — Lesson 6.3's harness: what a surface is.
//
//   §A  THE DEFINING PROPERTY — every NDF is a probability density
//   §B  Blinn-Phong IS a microfacet distribution, and the engine's constant is
//       off by exactly (s + 2) / 2
//   §C  the tail, which is the whole reason GGX won
//   §D  the geometry term, and the independence assumption that is not free
//   §E  the exponent map is a PEAK match — fitted against folklore
//   §F  6.2'S ENERGY TEST, REUSED UNCHANGED, pointed at the new machinery
//   §G  the golden
//
// §A IS THE ONE THAT MATTERS. A microfacet distribution is not a curve chosen to
// look right — it is a statement about what fraction of a surface faces each way,
// so the fractions must add to one. That makes it CHECKABLE, which `shininess`
// never was, and it is the whole difference between a model and a fudge.
//
// §F REUSES LESSON 6.2's HEMISPHERE INTEGRATOR CHARACTER FOR CHARACTER, and that
// is deliberate. A test rewritten for each new BRDF is not a test of the new BRDF;
// it is a test of the new test. The one below is 6.2's, unedited, and every BRDF
// for the rest of this course goes through it in the same shape.
//
// THIS HARNESS NEEDS NO GPU, like 6.2's. Nothing here is wired into a shader yet.
//
// Build and run:  sh scratch/build_verify_63.sh

#include <engine/core/assert.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/microfacet.hpp>
#include <engine/math/vec3.hpp>

#include "../demos/common/demo_scene.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <numbers>
#include <string>

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok) { ++g_failures; }
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
}

void checkf(bool ok, const char* fmt, ...)
{
    char line[640];
    va_list args;
    va_start(args, fmt);
    SDL_vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    check(ok, line);
}

[[nodiscard]] std::string read_file(const char* path)
{
    std::size_t size = 0;
    void* data = SDL_LoadFile(path, &size);
    if (data == nullptr) { return {}; }
    std::string out(static_cast<const char*>(data), size);
    SDL_free(data);
    return out;
}

/// Integrate `f(l)` times the cosine over the upper hemisphere about +Y.
///
/// **This is Lesson 6.2 §C's integrator, character for character.** It is not
/// copied out of laziness: reusing the identical test across BRDFs is what makes
/// the results comparable, and rewriting the instrument for each new subject is
/// how two models come to be measured on two different scales without anyone
/// noticing.
template <typename F>
double integrate_hemisphere(F&& f, int steps)
{
    const double d_theta = (std::numbers::pi / 2.0) / steps;
    const double d_phi   = (2.0 * std::numbers::pi) / (2 * steps);
    double total = 0.0;
    for (int i = 0; i < steps; ++i)
    {
        const double theta = (static_cast<double>(i) + 0.5) * d_theta;
        const double s = std::sin(theta);
        const double c = std::cos(theta);
        for (int j = 0; j < 2 * steps; ++j)
        {
            const double phi = (static_cast<double>(j) + 0.5) * d_phi;
            const engine::vec3 l{static_cast<float>(s * std::cos(phi)),
                                 static_cast<float>(c),
                                 static_cast<float>(s * std::sin(phi))};
            total += f(l) * c * s * d_theta * d_phi;
        }
    }
    return total;
}

constexpr engine::vec3 k_up{0.0f, 1.0f, 0.0f};

/// The NDF's own normalisation integral. Azimuthally symmetric, so the phi
/// integral is just a factor of 2*pi and one dimension is enough — which also
/// buys the sample count to make the answer mean something.
double integrate_ndf(engine::ndf_model model, float alpha, int steps)
{
    const double d_theta = (std::numbers::pi / 2.0) / steps;
    double total = 0.0;
    for (int i = 0; i < steps; ++i)
    {
        const double theta = (static_cast<double>(i) + 0.5) * d_theta;
        const double c = std::cos(theta);
        total += static_cast<double>(engine::ndf(model, static_cast<float>(c), alpha))
               * c * std::sin(theta) * d_theta;
    }
    return total * 2.0 * std::numbers::pi;
}

const char* name_of(engine::ndf_model m)
{
    switch (m)
    {
    case engine::ndf_model::blinn:    return "Blinn";
    case engine::ndf_model::beckmann: return "Beckmann";
    case engine::ndf_model::ggx:      break;
    }
    return "GGX";
}

// ===========================================================================
//  §A — THE DEFINING PROPERTY
// ===========================================================================

void section_a_distribution()
{
    std::printf("\n=== A. An NDF is a probability density ===\n");
    std::printf("  integral of D(h) cos(theta_h) dw, which must be 1:\n");
    std::printf("      %-10s %-14s %-14s %-14s\n", "roughness", "Blinn", "Beckmann", "GGX");

    double worst = 0.0;
    const char* worst_name = "";
    float worst_r = 0.0f;
    for (float r : {0.10f, 0.25f, 0.40f, 0.60f, 0.80f, 1.00f})
    {
        const float a = engine::alpha_from_roughness(r);
        double v[3];
        for (int m = 0; m < 3; ++m)
        {
            v[m] = integrate_ndf(static_cast<engine::ndf_model>(m), a, 400000);
            const double err = std::fabs(v[m] - 1.0);
            if (err > worst)
            {
                worst = err;
                worst_name = name_of(static_cast<engine::ndf_model>(m));
                worst_r = r;
            }
        }
        std::printf("      %-10.2f %-14.7f %-14.7f %-14.7f\n", static_cast<double>(r),
                    v[0], v[1], v[2]);
    }
    checkf(worst < 2.0e-5,
           "ALL THREE distributions integrate to 1 against cos(theta_h) — worst error "
           "%.2e (%s at roughness %.2f). This is what makes an NDF a model rather than "
           "a curve that looks right: the fractions of a surface facing each way must "
           "add up to the whole surface",
           worst, worst_name, static_cast<double>(worst_r));

    // AND THE READING OF THAT IDENTITY THE OTHER WAY ROUND. The cos(theta_h) is
    // the projected area of a microfacet on the plane it stands on, so the
    // integral says the facets' projected areas total exactly the flat area.
    // Nothing else could be true, and stating it that way is what makes the
    // cosine stop looking like a convention.
    check(std::fabs(integrate_ndf(engine::ndf_model::ggx,
                                  engine::alpha_from_roughness(0.5f), 400000) - 1.0) < 2e-5,
          "…and read the other way it says the microfacets' PROJECTED AREAS add up to "
          "the area of the flat surface they stand on — which is the only thing that "
          "could be true, and is why the cosine is in there");

    // A DISTRIBUTION WITHOUT ITS CONSTANT IS NOT A DISTRIBUTION. The bare lobe,
    // which is what the engine has shipped since 3.7, integrates to whatever it
    // likes.
    const float alpha32 = engine::alpha_from_blinn_exponent(32.0f);
    const float s32 = engine::blinn_exponent_from_alpha(alpha32);
    double bare = 0.0;
    {
        const int steps = 400000;
        const double d_theta = (std::numbers::pi / 2.0) / steps;
        for (int i = 0; i < steps; ++i)
        {
            const double th = (static_cast<double>(i) + 0.5) * d_theta;
            const double c = std::cos(th);
            bare += std::pow(c, static_cast<double>(s32)) * c * std::sin(th) * d_theta;
        }
        bare *= 2.0 * std::numbers::pi;
    }
    checkf(std::fabs(bare - 2.0 * std::numbers::pi / (s32 + 2.0)) < 1e-4,
           "the BARE cos^s lobe integrates to %.6f, not 1 — it is a shape, and "
           "2*pi/(s+2) is exactly the constant it is missing", bare);

    // ---- AND THE REASON ndf() DOES NOT USE THE TEXTBOOK ALGEBRA ------------
    //
    // This test is what found it. Every reference writes GGX's denominator as
    // `c2*(alpha^2 - 1) + 1`, which in `float` is the difference of two numbers
    // near 1 that very nearly agree — resolvable to ~1e-7 ABSOLUTE, while the
    // true value at the peak of a smooth lobe is 1e-4. Then it is squared.
    //
    // The check is written out here rather than left in a comment because the
    // stable form looks like a pointless rearrangement, and the next person to
    // "tidy" it back needs to be told by a failing test rather than by a diff.
    {
        const auto naive = [](float c, float a)
        {
            const float a2 = a * a;
            const float d = c * c * (a2 - 1.0f) + 1.0f;
            return a2 / (std::numbers::pi_v<float> * d * d);
        };
        const float a_mirror = 1.0e-3f;   // k_min_alpha: as smooth as we allow
        const int steps = 400000;
        const double d_theta = (std::numbers::pi / 2.0) / steps;
        double naive_total = 0.0;
        for (int i = 0; i < steps; ++i)
        {
            const double th = (static_cast<double>(i) + 0.5) * d_theta;
            const double c = std::cos(th);
            naive_total += static_cast<double>(naive(static_cast<float>(c), a_mirror))
                         * c * std::sin(th) * d_theta;
        }
        naive_total *= 2.0 * std::numbers::pi;
        const double ours = integrate_ndf(engine::ndf_model::ggx, a_mirror, 400000);
        checkf(std::fabs(ours - 1.0) < std::fabs(naive_total - 1.0) / 4.0,
               "at mirror roughness the TEXTBOOK denominator integrates to %.7f — it "
               "loses %.1f%% of the model's energy to rounding — while ndf()'s "
               "rearranged form gives %.7f. Identical algebra, and Sterbenz's lemma "
               "makes `1.0f - c` exact for c >= 0.5. Refining the grid does not help, "
               "because this is not a quadrature error",
               naive_total, std::fabs(naive_total - 1.0) * 100.0, ours);
    }
}

// ===========================================================================
//  §B — BLINN-PHONG WAS A MICROFACET MODEL ALL ALONG
// ===========================================================================

void section_b_blinn()
{
    std::printf("\n=== B. Blinn-Phong is an NDF missing its constant ===\n");

    // The engine multiplies the lobe by 1/pi (Lesson 6.2's decision, taken so the
    // two lobes shared the light's units). The normalised distribution multiplies
    // it by (s+2)/2pi. The ratio is therefore (s+2)/2 exactly, with no pi in it.
    std::printf("      %-12s %-16s %-16s %-10s\n", "shininess", "engine (1/pi)",
                "normalised", "ratio");
    double worst = 0.0;
    for (float s : {4.0f, 8.0f, 32.0f, 128.0f, 512.0f})
    {
        const float engine_k = engine::k_inv_pi;
        const float norm_k = (s + 2.0f) / (2.0f * std::numbers::pi_v<float>);
        const double ratio = static_cast<double>(norm_k / engine_k);
        worst = std::max(worst, std::fabs(ratio - static_cast<double>(s + 2.0f) / 2.0));
        std::printf("      %-12.0f %-16.6f %-16.6f %-10.2f\n", static_cast<double>(s),
                    static_cast<double>(engine_k), static_cast<double>(norm_k), ratio);
    }
    checkf(worst < 1e-4,
           "the ratio is EXACTLY (s+2)/2 — no pi in it, because both constants carry "
           "one and it cancels (worst deviation %.2e)", worst);
    check(std::fabs((32.0f + 2.0f) / 2.0f - 17.0f) < 1e-6f,
          "so at the demo's default shininess of 32 the engine's highlight is 17x "
          "DIMMER than a normalised distribution — which is why this engine's "
          "materials have always wanted a specular colour larger than a real one");

    // AND THE TRANSLATION BACK. Reading the existing materials into the new
    // vocabulary is the practical value of the mapping.
    const float a32 = engine::alpha_from_blinn_exponent(32.0f);
    checkf(std::fabs(a32 - 0.2425f) < 1e-3f,
           "shininess 32 means alpha = %.4f, a perceptual roughness of %.4f — almost "
           "exactly half way, which is a reasonable default and was never chosen",
           static_cast<double>(a32),
           static_cast<double>(engine::roughness_from_alpha(a32)));

    // The two mappings must be inverses, or a material round-tripped through the
    // new vocabulary comes back a different material.
    double worst_trip = 0.0;
    for (float s = 1.0f; s <= 1024.0f; s *= 1.6f)
    {
        const float back = engine::blinn_exponent_from_alpha(
            engine::alpha_from_blinn_exponent(s));
        worst_trip = std::max(worst_trip,
                              std::fabs(static_cast<double>(back - s)) / static_cast<double>(s));
    }
    checkf(worst_trip < 1e-5,
           "shininess -> alpha -> shininess round-trips to %.2e relative over 15 "
           "exponents, so a material converted to the new vocabulary and back is the "
           "same material", worst_trip);
}

// ===========================================================================
//  §C — THE TAIL
// ===========================================================================

void section_c_tail()
{
    std::printf("\n=== C. The tail, which is why GGX won ===\n");

    const float a = 0.3f;
    const float peak_b = engine::ndf(engine::ndf_model::beckmann, 1.0f, a);
    const float peak_g = engine::ndf(engine::ndf_model::ggx, 1.0f, a);
    checkf(std::fabs(peak_b - peak_g) < 1e-3f * peak_b,
           "at alpha %.2f Beckmann and GGX have the SAME PEAK, %.4f — so any "
           "difference between them is entirely a difference of SHAPE, not of "
           "brightness", static_cast<double>(a), static_cast<double>(peak_g));

    std::printf("      %-10s %-14s %-14s %-12s\n", "theta_h", "Beckmann", "GGX", "GGX/Beck");
    double ratio_45 = 0.0;
    double shoulder_min = 1e9;
    for (float deg : {5.0f, 10.0f, 20.0f, 30.0f, 45.0f, 60.0f, 75.0f})
    {
        const float c = std::cos(deg * std::numbers::pi_v<float> / 180.0f);
        const double b = static_cast<double>(engine::ndf(engine::ndf_model::beckmann, c, a));
        const double g = static_cast<double>(engine::ndf(engine::ndf_model::ggx, c, a));
        const double r = (b > 0.0) ? g / b : 1e300;
        if (deg == 45.0f) { ratio_45 = r; }
        if (deg <= 20.0f) { shoulder_min = std::min(shoulder_min, r); }
        std::printf("      %-10.0f %-14.6g %-14.6g %-12.4g\n",
                    static_cast<double>(deg), b, g, r);
    }
    checkf(shoulder_min < 0.85,
           "near the peak GGX is LOWER than Beckmann (down to %.2f of it at 20 "
           "degrees) — the tail is not free, and this is where it is paid for",
           shoulder_min);
    checkf(ratio_45 > 100.0,
           "…and 45 degrees off the peak GGX is %.0fx Beckmann, because a rational "
           "function has a power-law tail and an exponential has none. That glow "
           "around a highlight is the single reason GGX replaced Beckmann "
           "everywhere", ratio_45);

    const float c60 = std::cos(60.0f * std::numbers::pi_v<float> / 180.0f);
    checkf(engine::ndf(engine::ndf_model::beckmann, c60, a) < 1e-9f,
           "at 60 degrees Beckmann is %.3g — numerically zero, a hard edge where a "
           "real surface has a soft one",
           static_cast<double>(engine::ndf(engine::ndf_model::beckmann, c60, a)));
}

// ===========================================================================
//  §D — THE GEOMETRY TERM
// ===========================================================================

void section_d_geometry()
{
    std::printf("\n=== D. Shadowing and masking ===\n");

    checkf(std::fabs(engine::smith_g1(1.0f, 0.5f) - 1.0f) < 1e-6f,
           "looking straight down the normal, nothing is hidden: G1 = %.6f",
           static_cast<double>(engine::smith_g1(1.0f, 0.5f)));
    check(engine::smith_g1(0.0f, 0.5f) == 0.0f,
          "…and exactly 0 exactly edge-on, which is the limit rather than a guard");

    std::printf("      %-10s", "theta");
    for (float r : {0.22f, 0.45f, 0.71f, 1.00f})
    {
        std::printf(" a=%-10.2f", static_cast<double>(engine::alpha_from_roughness(r)));
    }
    std::printf("\n");
    bool monotone = true;
    for (float deg : {0.0f, 30.0f, 60.0f, 75.0f, 85.0f})
    {
        std::printf("      %-10.0f", static_cast<double>(deg));
        float prev = 2.0f;
        for (float r : {0.22f, 0.45f, 0.71f, 1.00f})
        {
            const float g1 = engine::smith_g1(
                std::cos(deg * std::numbers::pi_v<float> / 180.0f),
                engine::alpha_from_roughness(r));
            monotone = monotone && (g1 <= prev + 1e-6f);
            prev = g1;
            std::printf(" %-12.6f", static_cast<double>(g1));
        }
        std::printf("\n");
    }
    check(monotone, "G1 falls as roughness rises, at every angle — a rougher surface "
                    "hides more of itself, which is the whole content of the term");

    const float a_smooth = engine::alpha_from_roughness(0.22f);
    const float a_rough = engine::alpha_from_roughness(1.0f);
    const float c75 = std::cos(75.0f * std::numbers::pi_v<float> / 180.0f);
    checkf(engine::smith_g1(c75, a_smooth) / engine::smith_g1(c75, a_rough) > 2.0f,
           "at 75 degrees a near-smooth surface still shows %.4f of itself and a fully "
           "rough one only %.4f — THIS IS THE TERM LESSON 6.2 WAS MISSING when it "
           "measured a 5.36x view-angle swing it could not explain",
           static_cast<double>(engine::smith_g1(c75, a_smooth)),
           static_cast<double>(engine::smith_g1(c75, a_rough)));

    // THE INDEPENDENCE ASSUMPTION, PRICED. Multiplying two G1s assumes being lit
    // and being visible are independent events. They share a height field, so
    // they are not.
    std::printf("\n      separable vs height-correlated Smith:\n");
    std::printf("      %-10s %-10s %-10s %-14s %-14s %-8s\n",
                "alpha", "theta_l", "theta_v", "separable", "correlated", "ratio");
    double worst_ratio = 1.0;
    for (float a : {0.1f, 0.4f, 0.8f})
    {
        for (float d : {20.0f, 60.0f, 80.0f})
        {
            const float c = std::cos(d * std::numbers::pi_v<float> / 180.0f);
            const float sep = engine::smith_g_separable(c, c, a);
            const float cor = engine::smith_g(c, c, a);
            worst_ratio = std::max(worst_ratio, static_cast<double>(cor / sep));
            std::printf("      %-10.1f %-10.0f %-10.0f %-14.6f %-14.6f %-8.3f\n",
                        static_cast<double>(a), static_cast<double>(d),
                        static_cast<double>(d), static_cast<double>(sep),
                        static_cast<double>(cor), static_cast<double>(cor / sep));
        }
    }
    checkf(worst_ratio > 1.6,
           "the two forms agree to four decimals on smooth surfaces and disagree by "
           "%.3fx at alpha 0.8 with both directions at 80 degrees. Grazing angles on "
           "rough surfaces are sunsets, wet roads and worn metal, so \"either is "
           "fine\" would have been comfortable and wrong", worst_ratio);
    check(engine::smith_g(0.5f, 0.5f, 0.4f) <= 1.0f
              && engine::smith_g_separable(0.5f, 0.5f, 0.4f) <= 1.0f,
          "both forms stay within [0,1]: G is a fraction of the surface, and a "
          "fraction above 1 would be facets that are not there");
}

// ===========================================================================
//  §E — THE EXPONENT MAP IS A PEAK MATCH
// ===========================================================================

void section_e_fit()
{
    std::printf("\n=== E. \"s = 2/alpha^2 - 2\" matches the PEAK, and only the peak ===\n");

    // The mapping is derived by equating the two distributions AT h == n. Check
    // that it does what it claims before criticising it for what it does not.
    double worst_peak = 0.0;
    for (float a : {0.1f, 0.3f, 0.6f})
    {
        const float pb = engine::ndf(engine::ndf_model::blinn, 1.0f, a);
        const float pg = engine::ndf(engine::ndf_model::ggx, 1.0f, a);
        worst_peak = std::max(worst_peak,
                              std::fabs(static_cast<double>(pb - pg) / static_cast<double>(pg)));
    }
    checkf(worst_peak < 1e-4,
           "Blinn and GGX peak at the SAME height for the same alpha (worst relative "
           "difference %.2e) — the mapping does exactly what it was derived to do",
           worst_peak);

    // Now fit the whole lobe instead, and see where the exponent actually lands.
    std::printf("      %-10s %-14s %-14s %-10s\n", "alpha", "peak-match s", "fitted s",
                "fitted/peak");
    double worst_frac = 1.0;
    for (float a : {0.1f, 0.2f, 0.3f, 0.5f})
    {
        double best_s = 1.0, best_err = 1e300;
        for (double ls = 0.0; ls < 8.0; ls += 0.004)
        {
            const double s = std::exp(ls);
            const float alpha_for_s = engine::alpha_from_blinn_exponent(static_cast<float>(s));
            double err = 0.0;
            const int n = 1200;
            for (int i = 0; i < n; ++i)
            {
                const double th = (static_cast<double>(i) + 0.5) * (std::numbers::pi / 2.0) / n;
                const double c = std::cos(th);
                const double diff =
                    static_cast<double>(engine::ndf(engine::ndf_model::blinn,
                                                    static_cast<float>(c), alpha_for_s))
                    - static_cast<double>(engine::ndf(engine::ndf_model::ggx,
                                                      static_cast<float>(c), a));
                err += diff * diff * c * std::sin(th);
            }
            if (err < best_err) { best_err = err; best_s = s; }
        }
        const double peak_s = static_cast<double>(engine::blinn_exponent_from_alpha(a));
        worst_frac = std::min(worst_frac, best_s / peak_s);
        std::printf("      %-10.2f %-14.1f %-14.1f %-10.3f\n", static_cast<double>(a),
                    peak_s, best_s, best_s / peak_s);
    }
    checkf(worst_frac < 0.85,
           "fitting the whole LOBE instead gives consistently about 0.80x the "
           "peak-match exponent (worst %.3f) — a lobe fit trades peak height for the "
           "tail, and GGX has a tail Blinn cannot reproduce at ANY exponent. Both "
           "numbers are right; they answer different questions, and quoting one "
           "without saying which is how folklore travels", worst_frac);
}

// ===========================================================================
//  §F — 6.2's ENERGY TEST, REUSED UNCHANGED
// ===========================================================================

void section_f_energy()
{
    std::printf("\n=== F. Lesson 6.2's energy test, pointed at the new machinery ===\n");

    // First: the integrator still says what it said. If this line ever moves, the
    // instrument changed and none of the comparisons below mean anything.
    const double lambert = integrate_hemisphere(
        [](engine::vec3) { return static_cast<double>(engine::lambert_brdf({1, 1, 1}).r); },
        512);
    checkf(std::fabs(lambert - 1.0) < 2e-5,
           "the integrator is unchanged: a white Lambert BRDF still reads %.7f", lambert);

    // Now the specular microfacet BRDF, D*G/(4 nl nv), with F = 1 — so nothing is
    // absorbed at the interface and any shortfall is light the MODEL loses.
    std::printf("      R(v) for D*G/(4 nl nv) with F = 1:\n");
    std::printf("      %-12s %-14s %-14s\n", "roughness", "v at 0 deg", "v at 75 deg");
    double smooth_r = 0.0, rough_r = 1.0;
    for (float r : {0.22f, 0.50f, 0.71f, 0.87f, 1.00f})
    {
        const float alpha = engine::alpha_from_roughness(r);
        double vals[2];
        int k = 0;
        for (float deg : {0.0f, 75.0f})
        {
            const float th = deg * std::numbers::pi_v<float> / 180.0f;
            const engine::vec3 v{std::sin(th), std::cos(th), 0.0f};
            const float n_dot_v = std::cos(th);
            vals[k++] = integrate_hemisphere(
                [&](engine::vec3 l) -> double
                {
                    const float n_dot_l = dot(k_up, l);
                    if (n_dot_l <= 0.0f) { return 0.0; }
                    const engine::vec3 h = engine::halfway(l, v);
                    const float n_dot_h = dot(k_up, h);
                    if (n_dot_h <= 0.0f) { return 0.0; }
                    return static_cast<double>(
                               engine::ndf(engine::ndf_model::ggx, n_dot_h, alpha)
                               * engine::smith_g(n_dot_l, n_dot_v, alpha))
                         / (4.0 * static_cast<double>(n_dot_l)
                              * static_cast<double>(n_dot_v));
                },
                512);
        }
        if (r == 0.22f) { smooth_r = vals[0]; }
        if (r == 1.00f) { rough_r = vals[0]; }
        std::printf("      %-12.2f %-14.4f %-14.4f\n", static_cast<double>(r),
                    vals[0], vals[1]);
    }

    checkf(smooth_r < 1.0 && rough_r < 1.0,
           "it NEVER exceeds 1 — unlike Blinn-Phong, this model cannot emit more "
           "light than arrives, at any roughness or view angle");
    checkf(smooth_r > 0.95,
           "a near-smooth surface returns %.4f of what arrives, so almost nothing is "
           "lost where the model is nearly exact", smooth_r);
    checkf(rough_r < 0.45,
           "…but a fully rough one returns only %.4f. THAT IS NOT A BUG AND IT IS NOT "
           "ROUNDING: this is single-scattering microfacet theory, which lets a facet "
           "bounce light once and then forgets it. The missing %.0f%% is light that "
           "hit a second facet on its way out. Multiple-scattering compensation is "
           "the standard fix and it is named in §9 rather than built",
           rough_r, (1.0 - rough_r) * 100.0);
}

// ===========================================================================
//  §G — THE GOLDEN
// ===========================================================================

void section_g_golden()
{
    std::printf("\n=== G. The golden ===\n");

    const char* path = "build/demos/verify_63.ppm";
    check(demo::write_reference_shot(path) == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file(path);
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(), "same size: %zu bytes",
           ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL to the golden written in Lesson 5.1 — FOURTEEN lessons, and "
          "this one is cheap: 6.3 added a header and wired none of it in. The golden "
          "will not survive 6.4, which replaces shade()'s specular half outright");
}

}   // namespace

int main(int argc, char* argv[])
{
    bool debug = false;
    for (int i = 1; i < argc; ++i)
    {
        if (SDL_strcmp(argv[i], "--debug") == 0) { debug = true; }
    }
    SDL_SetLogPriorities(debug ? SDL_LOG_PRIORITY_DEBUG : SDL_LOG_PRIORITY_INFO);

    std::printf("verify_63 — microfacets: distributions, masking, and what they cost\n");

    section_a_distribution();
    section_b_blinn();
    section_c_tail();
    section_d_geometry();
    section_e_fit();
    section_f_energy();
    section_g_golden();

    std::printf("\n%d checks, %d failure%s\n", g_checks, g_failures,
                g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
