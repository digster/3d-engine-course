// scratch/verify_64.cpp — Lesson 6.4's harness: Fresnel, and the assembly.
//
//   §A  THE DOUBLING — a facet tilted by theta swings the ray by 2 theta
//   §B  THE JACOBIAN, measured against a direct change of variables
//   §C  Schlick against the EXACT Fresnel equations, and the error stated
//   §D  F0 and the index of refraction are the same number twice
//   §E  ENERGY — 6.2's integrator, unchanged, over the FULL coupled BRDF
//   §F  the white furnace, extended from 6.2 Exercise 13.5 to the specular
//   §G  the metallic workflow, and the legacy round trip
//   §H  the golden, re-baselined
//
// §B IS THE ONE THAT MATTERS, because the 4 in `4 (n.l)(n.v)` is the single most
// quoted and least explained line in real-time graphics. Lesson 6.3 deferred it
// explicitly. It is derived in the lesson from spherical coordinates on the fixed
// direction, and it is checked here against finite differences on the sphere —
// which is the difference between a derivation and an assertion.
//
// §E REUSES LESSON 6.2's HEMISPHERE INTEGRATOR CHARACTER FOR CHARACTER, for the
// third lesson running. A test rewritten for each new BRDF is not a test of the
// new BRDF; it is a test of the new test.
//
// Build and run:  sh scratch/build_verify_64.sh

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
    char line[768];
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

constexpr engine::vec3 k_up{0.0f, 1.0f, 0.0f};

engine::vec3 sph(double theta, double phi)
{
    return {static_cast<float>(std::sin(theta) * std::cos(phi)),
            static_cast<float>(std::cos(theta)),
            static_cast<float>(std::sin(theta) * std::sin(phi))};
}

/// Integrate `f(l)` times the cosine over the upper hemisphere about +Y.
///
/// **Lesson 6.2 §C's integrator, character for character**, and 6.3 §F's before
/// that. Reusing the identical instrument across BRDFs is what makes the results
/// comparable; rewriting it per subject is how two models come to be measured on
/// two different scales without anyone noticing.
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
            total += f(sph(theta, phi)) * c * s * d_theta * d_phi;
        }
    }
    return total;
}

/// **The exact Fresnel equations**, unpolarised, for a dielectric in air.
///
/// This is the thing Schlick approximates, written out so §C compares against
/// physics rather than against another approximation. Two polarisations, averaged
/// — which is what "unpolarised" means, and the averaging is the step most
/// summaries skip.
double fresnel_exact(double cos_i, double ior)
{
    const double sin_i2 = 1.0 - cos_i * cos_i;
    const double sin_t2 = sin_i2 / (ior * ior);           // Snell's law, squared
    if (sin_t2 >= 1.0) { return 1.0; }                    // total internal reflection
    const double cos_t = std::sqrt(1.0 - sin_t2);

    const double rs = (cos_i - ior * cos_t) / (cos_i + ior * cos_t);
    const double rp = (ior * cos_i - cos_t) / (ior * cos_i + cos_t);
    return 0.5 * (rs * rs + rp * rp);                     // amplitudes -> powers
}

/// The full BRDF as a scalar, for the integrator. Grey albedo, grey F0.
double brdf_scalar(engine::vec3 l, engine::vec3 v, double albedo,
                   engine::microsurface surface,
                   engine::diffuse_coupling coupling)
{
    const float nl = engine::dot(k_up, l);
    const float nv = engine::dot(k_up, v);
    if (nl <= 0.0f || nv <= 0.0f) { return 0.0; }
    const engine::vec3 h = engine::normalised(l + v);
    const float nh = std::max(0.0f, engine::dot(k_up, h));
    const float vh = std::max(0.0f, engine::dot(v, h));

    const engine::linear_rgb a{static_cast<float>(albedo), static_cast<float>(albedo),
                               static_cast<float>(albedo)};
    const engine::linear_rgb f0 = engine::f0_of(surface, a);
    const engine::linear_rgb f_s =
        engine::cook_torrance_specular(surface, f0, nl, nv, nh, vh);
    const engine::linear_rgb kd =
        engine::diffuse_transmission(coupling, f0, nl, nv, vh);
    const engine::linear_rgb f_d =
        engine::lambert_brdf(engine::diffuse_albedo_of(surface, a));
    return static_cast<double>(f_s.r + kd.r * f_d.r);
}

// ===========================================================================
//  §A — THE DOUBLING
// ===========================================================================

void section_a_doubling()
{
    std::printf("\n=== A. The doubling, which is where the 4 comes from ===\n");
    std::printf("  Reflect a FIXED direction v about a facet normal h tilted by\n");
    std::printf("  theta_h. The reflected ray comes off at 2 theta_h, exactly.\n\n");
    std::printf("   theta_h    theta_out    ratio      residual\n");

    const engine::vec3 v = sph(0.0, 0.0);
    double worst = 0.0;
    for (double th : {0.05, 0.2, 0.4, 0.7, 1.0, 1.4})
    {
        const engine::vec3 h = sph(th, 0.0);
        const engine::vec3 l = h * (2.0f * engine::dot(v, h)) - v;   // mirror_direction
        const double out = std::acos(std::clamp(
            static_cast<double>(engine::dot(v, l)), -1.0, 1.0));
        const double residual = std::fabs(out - 2.0 * th);
        worst = std::max(worst, residual);
        std::printf("  %8.3f  %11.5f  %8.4f  %11.2e\n", th, out, out / th, residual);
    }
    checkf(worst < 1e-6,
           "the reflected ray turns at EXACTLY twice the facet's tilt (worst residual "
           "%.1e over six angles) — this is one of the two factors of 2 in the "
           "denominator's 4, and it needs no approximation", worst);

    // The second factor, and it arrives from the same place: the SOLID angle
    // stretches by 2*sin(2 theta)/sin(theta), and the double-angle identity turns
    // that into 4 cos(theta). Checked as an identity rather than assumed.
    double worst_id = 0.0;
    for (double th = 0.01; th < 1.5; th += 0.01)
    {
        const double ratio = 2.0 * std::sin(2.0 * th) / std::sin(th);
        worst_id = std::max(worst_id, std::fabs(ratio - 4.0 * std::cos(th)));
    }
    checkf(worst_id < 1e-12,
           "2 sin(2t)/sin(t) IS 4 cos(t) — worst residual %.1e over 150 angles. The "
           "double-angle identity supplies the 4 AND the cosine at once, which is why "
           "the denominator carries a (v.h) it never spells out", worst_id);
}

// ===========================================================================
//  §B — THE JACOBIAN
// ===========================================================================

void section_b_jacobian()
{
    std::printf("\n=== B. The Jacobian, measured ===\n");
    std::printf("  Hold v fixed, vary l, watch where h goes. A patch of solid angle\n");
    std::printf("  dw_l maps to dw_h, and the claim is dw_h/dw_l = 1/(4 v.h).\n");
    std::printf("  Measured by finite differences on the sphere — NOT by re-deriving\n");
    std::printf("  the formula a second way, which would only check the algebra.\n\n");
    std::printf("   theta_v   theta_l      v.h      measured    1/(4 v.h)    rel err\n");

    const double eps = 1e-4;
    double worst = 0.0;
    for (double tv : {0.0, 0.5, 1.0, 1.2})
    {
        for (double tl : {0.2, 0.7, 1.1, 1.4})
        {
            const engine::vec3 v = sph(tv, 0.0);
            const engine::vec3 l0 = sph(tl, 1.1);

            auto half = [&](engine::vec3 l) { return engine::normalised(l + v); };

            // An orthonormal tangent frame at l0. Stepping `eps` along a tangent
            // and renormalising is a rotation by `eps` to first order, so the
            // source patch has area eps^2 exactly.
            const engine::vec3 t1 =
                engine::normalised(engine::cross(l0, engine::vec3{0.0f, 0.0f, 1.0f}));
            const engine::vec3 t2 = engine::cross(l0, t1);

            const engine::vec3 h0 = half(l0);
            const engine::vec3 da =
                half(engine::normalised(l0 + t1 * static_cast<float>(eps))) - h0;
            const engine::vec3 db =
                half(engine::normalised(l0 + t2 * static_cast<float>(eps))) - h0;

            const double area_h =
                static_cast<double>(engine::length(engine::cross(da, db)));
            const double measured = area_h / (eps * eps);
            const double v_dot_h = static_cast<double>(engine::dot(v, h0));
            const double predicted = 1.0 / (4.0 * v_dot_h);
            const double rel = std::fabs(measured - predicted) / predicted;
            worst = std::max(worst, rel);

            std::printf("  %8.3f  %8.3f  %8.5f  %10.6f  %11.6f  %10.2e\n",
                        tv, tl, v_dot_h, measured, predicted, rel);
        }
    }
    checkf(worst < 2e-3,
           "the half-vector map stretches solid angle by EXACTLY 1/(4 v.h) — worst "
           "relative error %.2e over sixteen configurations, and the residue is the "
           "finite difference, not the formula", worst);

    // THE CANCELLATION, which is why the denominator has no (v.h) in it. The
    // Jacobian contributes 4(v.h); the facets' projected area toward the light
    // contributes (l.h); and l.h == v.h because h bisects them. Stating it as an
    // identity is the whole argument.
    double worst_bisect = 0.0;
    for (double tv : {0.1, 0.6, 1.1})
    {
        for (double tl : {0.3, 0.9, 1.3})
        {
            const engine::vec3 v = sph(tv, 0.0);
            const engine::vec3 l = sph(tl, 2.0);
            const engine::vec3 h = engine::normalised(l + v);
            worst_bisect = std::max(worst_bisect,
                std::fabs(static_cast<double>(engine::dot(v, h) - engine::dot(l, h))));
        }
    }
    checkf(worst_bisect < 1e-6,
           "l.h == v.h always (worst %.1e) — h BISECTS them, which is the whole "
           "reason the (v.h) from the Jacobian cancels against the facets' projected "
           "area and leaves a denominator that looks unmotivated", worst_bisect);
}

// ===========================================================================
//  §C — SCHLICK AGAINST THE EXACT EQUATIONS
// ===========================================================================

void section_c_schlick()
{
    std::printf("\n=== C. Schlick against the exact Fresnel equations ===\n");
    std::printf("  Not 'a good approximation' — a number.\n\n");
    std::printf("    ior      F0      worst |err|  at cos   worst rel err   at cos\n");

    for (double ior : {1.33, 1.4, 1.5, 1.8, 2.42})
    {
        const float f0 = engine::f0_from_ior(static_cast<float>(ior));
        double wa = 0.0, ca = 0.0, wr = 0.0, cr = 0.0;
        for (int i = 0; i <= 20000; ++i)
        {
            const double c = static_cast<double>(i) / 20000.0;
            const double e = fresnel_exact(c, ior);
            const double s = static_cast<double>(
                engine::fresnel_schlick(static_cast<float>(c), f0));
            const double a = std::fabs(e - s);
            if (a > wa) { wa = a; ca = c; }
            if (e > 1e-6 && a / e > wr) { wr = a / e; cr = c; }
        }
        std::printf("  %6.2f  %8.5f  %11.5f  %7.4f  %13.1f%%  %7.4f\n",
                    ior, static_cast<double>(f0), wa, ca, wr * 100.0, cr);
        if (ior == 1.5)
        {
            checkf(wa < 0.04,
                   "glass (ior 1.5): worst ABSOLUTE error %.4f, out at cos %.3f — near "
                   "grazing, where the answer is heading for 1 anyway", wa, ca);
            checkf(wr > 0.15 && wr < 0.30,
                   "…but the worst RELATIVE error is %.1f%%, at cos %.3f (about 55 "
                   "degrees) — in the MIDDLE of the range, where surfaces are usually "
                   "seen. The folklore says Schlick is very accurate; it is very cheap, "
                   "and accurate where the value is large", wr * 100.0, cr);
        }
    }

    // THE TWO ENDS ARE EXACT BY CONSTRUCTION, and that is the shape argument the
    // lesson makes: every interface reflects everything at grazing incidence.
    const float f0_glass = engine::f0_from_ior(1.5f);
    checkf(engine::fresnel_schlick(1.0f, f0_glass) == f0_glass,
           "at normal incidence Schlick returns F0 EXACTLY (%.6f) — by construction, "
           "not by fit", static_cast<double>(f0_glass));
    checkf(engine::fresnel_schlick(0.0f, f0_glass) == 1.0f,
           "at grazing incidence it returns EXACTLY 1, for every F0 — which is not an "
           "approximation but the physics: look along any surface and it is a mirror");

    // The absolute error is what a picture sees, and 0.036 of a term that is
    // itself 0.04 sounds alarming until it is put beside the light it multiplies.
    const double at60_exact = fresnel_exact(0.5, 1.5);
    const double at60_schlick = static_cast<double>(engine::fresnel_schlick(0.5f, f0_glass));
    checkf(std::fabs(at60_exact - 0.0892) < 0.001 && std::fabs(at60_schlick - 0.0700) < 0.001,
           "at 60 degrees the exact answer is %.4f and Schlick says %.4f — 21%% low, "
           "and a 0.019 error in a reflectance is invisible. THAT is the honest defence "
           "of Schlick, not a tight fit", at60_exact, at60_schlick);
}

// ===========================================================================
//  §D — F0 AND THE INDEX OF REFRACTION
// ===========================================================================

void section_d_f0()
{
    std::printf("\n=== D. F0 and the IOR are one number written twice ===\n");
    std::printf("   material            ior       F0      back to ior    residual\n");

    struct row { const char* name; float ior; };
    const row mats[] = {
        {"water",           1.333f},
        {"skin",            1.400f},
        {"plastic / glass", 1.500f},
        {"sapphire",        1.770f},
        {"diamond",         2.420f},
    };
    double worst = 0.0;
    for (const row& m : mats)
    {
        const float f0 = engine::f0_from_ior(m.ior);
        const float back = engine::ior_from_f0(f0);
        const double residual = std::fabs(static_cast<double>(back - m.ior));
        worst = std::max(worst, residual);
        std::printf("   %-16s  %6.3f  %8.5f  %11.5f  %10.1e\n",
                    m.name, static_cast<double>(m.ior), static_cast<double>(f0),
                    static_cast<double>(back), residual);
    }
    checkf(worst < 1e-4,
           "F0 round-trips through the index of refraction (worst residual %.1e) — so "
           "the material parameter is not a taste, it is a measurement someone can "
           "look up", worst);

    checkf(std::fabs(engine::f0_from_ior(1.5f) - 0.04f) < 5e-4,
           "glass at ior 1.5 gives F0 = %.5f, which is where the 0.04 every renderer "
           "hard-codes actually comes from",
           static_cast<double>(engine::f0_from_ior(1.5f)));

    // THE DIAGNOSTIC, and it is the reason `ior_from_f0` exists at all: run the
    // engine's OWN shipped specular colours back through it.
    std::printf("\n   what this engine shipped, read back as an index of refraction:\n");
    std::printf("   authored specular::colour     implied ior\n");
    for (float c : {0.30f, 0.45f, 0.55f, 0.60f, 0.85f})
    {
        std::printf("   %-27.2f  %11.2f\n",
                    static_cast<double>(c), static_cast<double>(engine::ior_from_f0(c)));
    }
    checkf(engine::ior_from_f0(0.85f) > 20.0f,
           "the demo's 0.85 implies an index of refraction of %.1f, where diamond is "
           "2.42 and the highest known solid is about 4. It was never a material — it "
           "was Lesson 6.3's 17x normalisation error wearing a parameter's clothes",
           static_cast<double>(engine::ior_from_f0(0.85f)));
}

// ===========================================================================
//  §E — ENERGY, WITH THE COUPLING IN
// ===========================================================================

void section_e_energy()
{
    std::printf("\n=== E. Energy: the door 6.2 opened, and whether it shuts ===\n");

    // The integrator still reads 1 for a white Lambert BRDF — the instrument is
    // unchanged, which is the only thing that makes the comparisons below mean
    // anything.
    const double lambert_check = integrate_hemisphere(
        [](engine::vec3) { return static_cast<double>(engine::lambert_brdf({1, 1, 1}).r); },
        512);
    checkf(std::fabs(lambert_check - 1.0) < 1e-5,
           "the integrator is unchanged: a white Lambert BRDF still reads %.7f",
           lambert_check);

    std::printf("\n  R(v) for a WHITE surface, F0 = 0.04. 'uncoupled' adds the lobes\n");
    std::printf("  the way Lesson 3.7 did; 'coupled' is what 6.4 ships.\n\n");
    std::printf("   roughness  theta_v   uncoupled   coupled\n");

    double worst_coupled = 0.0, worst_uncoupled = 0.0;
    for (double r : {0.10, 0.30, 0.50, 0.70, 1.00})
    {
        for (double tv : {0.02, 0.65, 1.31})
        {
            const engine::vec3 v = sph(tv, 0.0);
            const engine::microsurface surf{.roughness = static_cast<float>(r)};

            const double un = integrate_hemisphere([&](engine::vec3 l) {
                // The uncoupled form: the same two lobes, added, with no Fresnel
                // told about the other. This is what 1.1386 was.
                const float nl = engine::dot(k_up, l);
                const float nv = engine::dot(k_up, v);
                if (nl <= 0.0f || nv <= 0.0f) { return 0.0; }
                const engine::vec3 h = engine::normalised(l + v);
                const engine::linear_rgb white{1.0f, 1.0f, 1.0f};
                const engine::linear_rgb f0 = engine::f0_of(surf, white);
                const engine::linear_rgb f_s = engine::cook_torrance_specular(
                    surf, f0, nl, nv, std::max(0.0f, engine::dot(k_up, h)),
                    std::max(0.0f, engine::dot(v, h)));
                return static_cast<double>(f_s.r + engine::lambert_brdf(white).r);
            }, 400);

            const double co = integrate_hemisphere([&](engine::vec3 l) {
                return brdf_scalar(l, v, 1.0, surf,
                                   engine::diffuse_coupling::two_crossing);
            }, 400);

            worst_coupled = std::max(worst_coupled, co);
            worst_uncoupled = std::max(worst_uncoupled, un);
            std::printf("  %9.2f  %7.2f  %10.4f  %8.4f%s\n",
                        r, tv * 180.0 / std::numbers::pi, un, co,
                        (un > 1.0) ? "   <- over" : "");
        }
    }

    checkf(worst_uncoupled > 1.0,
           "WITHOUT the coupling the surface still emits more light than arrives — "
           "worst %.4f. Normalising D (Lesson 6.3) did not fix this and 6.3 said so: "
           "the lobes were independent, so the same photon was counted twice",
           worst_uncoupled);
    checkf(worst_coupled <= 1.0,
           "WITH it, never above 1 — worst %.4f over the sweep. F is what bounces off "
           "the interface, so 1 - F is all that is left to go in and scatter back out. "
           "That sentence is the lesson", worst_coupled);

    // THE FULL SWEEP, which is where the claim has to hold if it holds anywhere.
    std::printf("\n  the full sweep — worst R(v) over roughness x view angle:\n");
    std::printf("   coupling                        worst R   at r    at theta_v\n");
    struct crow { engine::diffuse_coupling c; const char* name; };
    const crow forms[] = {
        {engine::diffuse_coupling::two_crossing, "(1-F(n.l))(1-F(n.v))  [ours]"},
        {engine::diffuse_coupling::half_vector,  "1-F(v.h)              [glTF]"},
    };
    double two_cross_worst = 0.0, half_worst = 0.0;
    for (const crow& f : forms)
    {
        double worst = 0.0, ar = 0.0, at = 0.0;
        for (int ri = 0; ri <= 12; ++ri)
        {
            const double r = 0.05 + 0.079 * ri;
            for (int ti = 0; ti <= 10; ++ti)
            {
                const double tv = 0.02 + (1.53 / 10.0) * ti;
                const engine::vec3 v = sph(tv, 0.0);
                const engine::microsurface surf{.roughness = static_cast<float>(r)};
                const double t = integrate_hemisphere([&](engine::vec3 l) {
                    return brdf_scalar(l, v, 1.0, surf, f.c);
                }, 260);
                if (t > worst) { worst = t; ar = r; at = tv; }
            }
        }
        std::printf("   %-30s %8.4f  %6.2f  %10.1f\n",
                    f.name, worst, ar, at * 180.0 / std::numbers::pi);
        if (f.c == engine::diffuse_coupling::two_crossing) { two_cross_worst = worst; }
        else { half_worst = worst; }
    }

    checkf(two_cross_worst <= 1.0,
           "the two-crossing form never exceeds 1 anywhere in the sweep (worst %.4f) — "
           "which is what makes it the default", two_cross_worst);
    checkf(half_worst > 1.0,
           "THE COMMON FORM DOES: 1 - F(v.h) reaches %.4f. It is not a typo in anyone's "
           "engine — it is the fraction of light that got IN, and it says nothing about "
           "the light that fails to get OUT. A diffuse ray leaving toward a grazing eye "
           "meets the interface at a grazing angle, where a quarter of it reflects back "
           "inside, and that form lets all of it out", half_worst);

    // RECIPROCITY, which is what disqualified the obvious repair. Adding an exit
    // factor to the half-vector form kills the excess (measured at 0.9628) and is
    // NOT symmetric in l and v, so it is not a BRDF at all.
    const engine::vec3 a = sph(0.4, 0.3), b = sph(1.2, 2.1);
    const engine::microsurface surf{.roughness = 0.4f};
    double worst_recip = 0.0;
    for (auto c : {engine::diffuse_coupling::two_crossing,
                   engine::diffuse_coupling::half_vector})
    {
        worst_recip = std::max(worst_recip,
            std::fabs(brdf_scalar(a, b, 0.6, surf, c) - brdf_scalar(b, a, 0.6, surf, c)));
    }
    checkf(worst_recip < 1e-7,
           "both coupling forms are RECIPROCAL — f(l,v) == f(v,l) to %.1e. That is not "
           "decoration: it is what a BRDF is, and it is what ruled out the obvious "
           "repair of bolting an exit factor onto the half-vector form", worst_recip);
}

// ===========================================================================
//  §F — THE WHITE FURNACE
// ===========================================================================

void section_f_furnace()
{
    std::printf("\n=== F. The white furnace, extended past 6.2 Exercise 13.5 ===\n");
    std::printf("  A surface that absorbs nothing, lit by radiance 1 from every\n");
    std::printf("  direction, must render at exactly 1 and VANISH into the\n");
    std::printf("  background. 6.2 passed this for the diffuse and predicted the\n");
    std::printf("  specular would fail it. Here is by how much.\n\n");
    std::printf("   roughness   R (white, F0=0.04)   shortfall\n");

    for (double r : {0.05, 0.20, 0.50, 0.80, 1.00})
    {
        const engine::vec3 v = sph(0.3, 0.0);
        const engine::microsurface surf{.roughness = static_cast<float>(r)};
        const double rr = integrate_hemisphere([&](engine::vec3 l) {
            return brdf_scalar(l, v, 1.0, surf, engine::diffuse_coupling::two_crossing);
        }, 500);
        std::printf("  %10.2f  %19.4f  %10.1f%%\n", r, rr, (1.0 - rr) * 100.0);
    }

    // A METAL is where the loss stops being academic, because a metal has no
    // diffuse lobe to carry the light the specular drops.
    std::printf("\n   a METAL (no diffuse lobe at all, F0 = 0.95):\n");
    std::printf("   roughness   R      where the missing light went\n");
    double rough_metal = 1.0;
    for (double r : {0.10, 0.30, 0.60, 1.00})
    {
        const engine::vec3 v = sph(0.3, 0.0);
        const engine::microsurface surf{.roughness = static_cast<float>(r),
                                        .metallic = 1.0f};
        const engine::linear_rgb white{0.95f, 0.95f, 0.95f};
        const double rr = integrate_hemisphere([&](engine::vec3 l) {
            const float nl = engine::dot(k_up, l);
            const float nv = engine::dot(k_up, v);
            if (nl <= 0.0f || nv <= 0.0f) { return 0.0; }
            const engine::vec3 h = engine::normalised(l + v);
            const engine::linear_rgb f0 = engine::f0_of(surf, white);
            const engine::linear_rgb f_s = engine::cook_torrance_specular(
                surf, f0, nl, nv, std::max(0.0f, engine::dot(k_up, h)),
                std::max(0.0f, engine::dot(v, h)));
            return static_cast<double>(f_s.r);
        }, 500);
        if (r >= 1.0) { rough_metal = rr; }
        std::printf("  %10.2f  %6.4f   %s\n", r, rr,
                    (r >= 0.6) ? "a second facet, on the way out" : "almost nowhere");
    }

    checkf(rough_metal < 0.4,
           "a fully rough white metal returns only %.4f of what arrives. THAT IS NOT "
           "ROUNDING AND IT IS NOT A BUG: single-scattering microfacet theory lets a "
           "facet bounce light once and then forgets it, and the missing light hit a "
           "second facet on the way out. Lesson 6.3 measured the same 69%% loss with F "
           "forced to 1; Fresnel did not change it, because it is G's doing, not F's",
           rough_metal);

    checkf(rough_metal > 0.25,
           "…and it is a shortfall, never an excess (%.4f) — the model loses light, "
           "which is the safe direction to be wrong in. Multiple-scattering "
           "compensation is the standard fix and it is named in the lesson rather than "
           "built", rough_metal);
}

// ===========================================================================
//  §G — THE METALLIC WORKFLOW, AND THE LEGACY ROUND TRIP
// ===========================================================================

void section_g_metallic()
{
    std::printf("\n=== G. Metals, dielectrics, and reading the old values across ===\n");

    const engine::linear_rgb gold{1.00f, 0.71f, 0.29f};

    const engine::microsurface plastic{.roughness = 0.4f, .metallic = 0.0f};
    const engine::linear_rgb pf0 = engine::f0_of(plastic, gold);
    checkf(pf0.r == pf0.g && pf0.g == pf0.b && std::fabs(pf0.r - 0.04f) < 1e-6,
           "a DIELECTRIC's F0 is grey (%.4f, %.4f, %.4f) whatever its albedo — its "
           "index of refraction barely varies across the visible spectrum, so the "
           "colour you see comes from what got INSIDE",
           static_cast<double>(pf0.r), static_cast<double>(pf0.g),
           static_cast<double>(pf0.b));

    const engine::microsurface metal{.roughness = 0.4f, .metallic = 1.0f};
    const engine::linear_rgb mf0 = engine::f0_of(metal, gold);
    checkf(mf0.r == gold.r && mf0.g == gold.g && mf0.b == gold.b,
           "a METAL's F0 IS its albedo (%.2f, %.2f, %.2f) — the colour has nowhere "
           "else to live, because nothing that crosses the interface comes back out",
           static_cast<double>(mf0.r), static_cast<double>(mf0.g),
           static_cast<double>(mf0.b));

    const engine::linear_rgb md = engine::diffuse_albedo_of(metal, gold);
    checkf(md.r == 0.0f && md.g == 0.0f && md.b == 0.0f,
           "…and a metal's DIFFUSE albedo is exactly zero. Both facts are the same "
           "fact: a conductor absorbs what gets in within a few atomic layers. The "
           "metallic workflow is not a checkbox, it is a consequence");

    // THE LEGACY ROUND TRIP. Lesson 6.3 built the mapping; 6.4 uses it to keep
    // the old lobes runnable off the new parameters, so the claim that the old
    // parameters were the new ones badly spelled is checkable.
    std::printf("\n   roughness -> legacy shininess -> roughness\n");
    std::printf("   roughness   shininess    back      residual\n");
    double worst = 0.0;
    for (float r : {0.20f, 0.35f, 0.49f, 0.65f, 0.85f})
    {
        const float s = engine::legacy_shininess_of({.roughness = r});
        const float back = engine::roughness_from_alpha(engine::alpha_from_blinn_exponent(s));
        const double residual = std::fabs(static_cast<double>(back - r));
        worst = std::max(worst, residual);
        std::printf("  %10.3f  %10.2f  %8.4f  %11.1e\n",
                    static_cast<double>(r), static_cast<double>(s),
                    static_cast<double>(back), residual);
    }
    checkf(worst < 1e-5,
           "the round trip closes to %.1e — roughness 0.49 really is shininess 32, "
           "which is exactly where the engine's default came from and where it went",
           worst);

    // The demo authors 0.49, and the exact translation of shininess 32 is
    // 0.4925 — so this comes back as 32.69 rather than 32.00. The gap is the
    // ROUNDING IN THE AUTHORED VALUE, not in the mapping, and the tolerance says
    // so instead of being widened until the number fits. The exact round trip is
    // the check above, which closes to zero.
    const float s_from_authored = engine::legacy_shininess_of({.roughness = 0.49f});
    const float s_from_exact = engine::legacy_shininess_of(
        {.roughness = engine::roughness_from_alpha(engine::alpha_from_blinn_exponent(32.0f))});
    checkf(std::fabs(s_from_exact - 32.0f) < 0.01f,
           "the EXACT translation of shininess 32 round-trips to %.4f", 
           static_cast<double>(s_from_exact));
    checkf(std::fabs(s_from_authored - 32.0f) < 1.0f,
           "and the value the demos actually author, 0.49, comes back as %.2f — the "
           "0.69 is the rounding in 0.49 (the exact roughness is 0.4925), not slack in "
           "the mapping. A parameter you can round to two decimals and stay inside 2%% "
           "of where you were is a parameter with a scale",
           static_cast<double>(s_from_authored));
}

// ===========================================================================
//  §H — THE GOLDEN
// ===========================================================================

void section_h_golden()
{
    std::printf("\n=== H. The golden, re-baselined ===\n");

    const int rc = demo::write_reference_shot("scratch/verify64.ppm");
    check(rc == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file("scratch/verify64.ppm");
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(),
           "same size: %zu bytes — and it GREW, from 1,209,616. Lesson 6.4 added an "
           "eighth frame, because replacing the whole shading model moved 2,400 of "
           "403,200 pixels and that was the INSTRUMENT's fault, not the change's: "
           "three of seven frames exercised the shading equation at all",
           ours.size());
    check(ours == golden,
          "byte-identical to the re-baselined golden. THE OLD HASH WAS 905BF27E AND "
          "STOOD FOR FOURTEEN LESSONS; the new one is E917C06C over eight frames. It "
          "did not drift — the model was replaced on purpose, and the old picture was "
          "not merely worse, it was differently parameterised");
}

}   // namespace

int main(int argc, char* argv[])
{
    (void)argc; (void)argv;
    if (!SDL_Init(0)) { SDL_Log("SDL_Init failed: %s", SDL_GetError()); return 1; }

    std::printf("=== Lesson 6.4 — Cook-Torrance, derived ==========================\n");

    section_a_doubling();
    section_b_jacobian();
    section_c_schlick();
    section_d_f0();
    section_e_energy();
    section_f_furnace();
    section_g_metallic();
    section_h_golden();

    std::printf("\n%d checks, %d failure%s\n", g_checks, g_failures,
                g_failures == 1 ? "" : "s");
    SDL_Quit();
    return g_failures == 0 ? 0 : 1;
}
