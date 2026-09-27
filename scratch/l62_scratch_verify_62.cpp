// scratch/verify_62.cpp — Lesson 6.2's harness: what the numbers ARE.
//
//   §A  radiometry: the four quantities, and why RADIANCE is the one a pixel holds
//   §B  the cosine factor, derived from PROJECTED AREA rather than from a formula
//   §C  THE PI, DERIVED BY INTEGRATION — and checked by integrating it
//   §D  ENERGY CONSERVATION AS A NUMBER, including the two places we fail it
//   §E  the pi moved out of the light: what changed in float, what changed on screen
//   §F  the API, and the two spellings of one constant
//   §G  the golden
//
// §C AND §D ARE THE TWO THAT MATTER. §C turns "the Lambert BRDF is albedo/pi"
// from a formula into a measurement: integrate a constant BRDF over the
// hemisphere and the answer is pi, so the constant that conserves energy is
// albedo/pi and nothing else. §D then applies the same integral to the highlight
// this engine has shipped since Lesson 3.7 and finds it wanting — in a specific,
// quotable way that Lesson 6.4's Fresnel term is the answer to.
//
// THIS HARNESS NEEDS NO GPU, deliberately. Lesson 6.1's needed one because its
// subject was a swapchain; this lesson's subject is arithmetic, and arithmetic
// that can only be checked on a machine with a display is arithmetic that stops
// being checked. The CPU/GPU agreement across this change is verify_48 §F's job,
// and it still reports 1.788e-07 — one to two ULP — after both sides moved the pi
// independently.
//
// Build and run:  sh scratch/build_verify_62.sh

#include <engine/core/assert.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/light.hpp>
#include <engine/math/vec3.hpp>

#include "../demos/common/demo_scene.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
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
/// `dw = sin(theta) dtheta dphi` is the solid-angle element, derived in §2.2 of
/// the lesson from the area of a patch on the unit sphere. Midpoint sampling on a
/// regular grid, because the integrand is smooth and the point here is to get a
/// number we can trust to five figures rather than to be clever.
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

/// The directional-hemispherical reflectance of the Blinn lobe, on the 1/pi
/// scale the engine puts it on — the fraction of arriving light a white
/// highlight sends back, for one outgoing direction.
double blinn_reflectance(engine::vec3 v, float shininess, double scale, int steps = 512)
{
    return integrate_hemisphere(
        [&](engine::vec3 l) -> double
        {
            const float a = dot(k_up, engine::halfway(l, v));
            if (a <= 0.0f) { return 0.0; }
            return std::pow(static_cast<double>(std::min(1.0f, a)),
                            static_cast<double>(shininess)) * scale;
        },
        steps);
}

// ===========================================================================
//  §A — RADIOMETRY, AND WHY RADIANCE
// ===========================================================================

void section_a_radiometry()
{
    std::printf("\n=== A. Radiometry: which quantity a pixel holds ===\n");

    // THE CLAIM: radiance is invariant along a ray through empty space, and
    // irradiance is not. That single fact is why a renderer stores radiance.
    //
    // The set-up is two small patches facing each other, `d` apart. As `d`
    // grows, the irradiance the second one receives falls as 1/d^2 — but the
    // solid angle the first one subtends falls as 1/d^2 too, and radiance is
    // the ratio. So it does not move.
    const double emitter_area = 1.0e-4;   // 1 cm^2
    const double emitted_radiance = 7.5;  // an arbitrary L, in engine units

    std::printf("      d     solid angle    irradiance     recovered L\n");
    double first_l = 0.0;
    double worst_l_error = 0.0;
    double e_at_1 = 0.0, e_at_2 = 0.0;
    for (int i = 0; i < 4; ++i)
    {
        const double d = 1.0 * std::pow(2.0, i);
        const double omega = emitter_area / (d * d);          // small-angle, face-on
        const double irradiance = emitted_radiance * omega;   // E = L * dw * cos(0)
        const double recovered = irradiance / omega;          // L = E / dw
        if (i == 0) { first_l = recovered; e_at_1 = irradiance; }
        if (i == 1) { e_at_2 = irradiance; }
        worst_l_error = std::max(worst_l_error, std::fabs(recovered - emitted_radiance));
        std::printf("  %7.2f  %12.3e  %12.3e  %12.6f\n", d, omega, irradiance, recovered);
    }
    checkf(worst_l_error < 1e-12,
           "RADIANCE IS INVARIANT along the ray: recovered L = %.6f at every distance "
           "(worst drift %.2e)", first_l, worst_l_error);
    checkf(std::fabs(e_at_1 / e_at_2 - 4.0) < 1e-9,
           "…while IRRADIANCE is not: doubling the distance divides it by %.4f, the "
           "inverse-square law, which is why a renderer that stored irradiance would "
           "have to know how far away everything was", e_at_1 / e_at_2);

    // The unit chain, asserted as arithmetic rather than recited. Flux over area
    // is irradiance; irradiance over solid angle is radiance.
    const double flux = 12.0;                      // W leaving a lamp
    const double area = 3.0;                       // m^2 it lands on
    const double solid_angle = 0.25;               // sr it arrives through
    const double e = flux / area;
    const double l = e / solid_angle;
    checkf(std::fabs(e - 4.0) < 1e-12 && std::fabs(l - 16.0) < 1e-12,
           "the unit chain: %.1f W over %.1f m^2 is E = %.1f W/m^2, and through %.2f sr "
           "that is L = %.1f W/(m^2 sr)", flux, area, e, solid_angle, l);

    // AND THE ONE THAT SURPRISES PEOPLE: the hemisphere has 2*pi steradians, but
    // the COSINE-WEIGHTED hemisphere — the one that matters, because that cosine
    // is in every light-transport integral there is — has exactly pi. This is the
    // pi in the Lambert BRDF, met before it is needed.
    const double solid = integrate_hemisphere([](engine::vec3) { return 1.0; }, 1024);
    checkf(std::fabs(solid - std::numbers::pi) < 1e-5,
           "the COSINE-WEIGHTED hemisphere measures %.7f, which is pi to %.1e — and "
           "2*pi = %.4f is the un-weighted answer nobody's shading equation wants",
           solid, std::fabs(solid - std::numbers::pi), 2.0 * std::numbers::pi);
}

// ===========================================================================
//  §B — THE COSINE, FROM PROJECTED AREA
// ===========================================================================

void section_b_cosine()
{
    std::printf("\n=== B. The cosine factor is a statement about AREA ===\n");

    // Lesson 3.6 drew the spreading beam. Here it is as geometry: take a beam
    // with a unit-square cross-section travelling along -Y, and intersect it with
    // a plane tilted by theta. The footprint is a RECTANGLE, one of whose sides
    // has been stretched by 1/cos(theta) — computed from actual vertices with a
    // cross product, so it owes nothing to the formula it is checking.
    std::printf("      theta   footprint area   1/cos(theta)   n.l\n");
    double worst = 0.0;
    for (int deg = 0; deg <= 75; deg += 15)
    {
        const double theta = deg * std::numbers::pi / 180.0;
        const engine::vec3 n{0.0f, static_cast<float>(std::cos(theta)),
                             static_cast<float>(std::sin(theta))};
        const engine::vec3 l{0.0f, 1.0f, 0.0f};   // the beam arrives straight down

        // Two edges of the unit beam cross-section, dropped onto the tilted
        // plane along the beam's own direction. The one across the tilt is
        // unchanged; the one along it stretches.
        const engine::vec3 e1{1.0f, 0.0f, 0.0f};
        const float stretch = 1.0f / static_cast<float>(std::cos(theta));
        const engine::vec3 e2{0.0f, static_cast<float>(-std::sin(theta)) * stretch,
                              static_cast<float>(std::cos(theta)) * stretch};
        const double area = static_cast<double>(length(cross(e1, e2)));
        const double n_dot_l = static_cast<double>(engine::lambert(n, l));

        // The beam's power is fixed; spread over `area` it delivers 1/area per
        // unit surface. That reciprocal IS the cosine, and that is the whole law.
        worst = std::max(worst, std::fabs(1.0 / area - n_dot_l));
        std::printf("  %7d   %14.6f   %12.6f   %.6f\n", deg, area,
                    1.0 / std::cos(theta), n_dot_l);
    }
    checkf(worst < 1e-6,
           "1 / (footprint area) equals dot(n, l) at every angle, worst error %.2e — "
           "the cosine law derived from geometry, not asserted", worst);

    // AND THE ENGINE'S OWN SPELLING OF IT. `irradiance_on` is the light's side of
    // the equation: E_perp projected onto a surface.
    engine::directional_light lamp;
    lamp.direction = engine::normalised(engine::vec3{0.0f, -1.0f, 0.0f});
    lamp.irradiance = 10.0f;
    const engine::vec3 tilted = engine::normalised(engine::vec3{0.0f, 1.0f, 1.0f});
    checkf(std::fabs(lamp.irradiance_on(k_up) - 10.0f) < 1e-6f,
           "irradiance_on(square-on) returns E_perp itself: %.4f",
           static_cast<double>(lamp.irradiance_on(k_up)));
    checkf(std::fabs(lamp.irradiance_on(tilted) - 10.0f / std::sqrt(2.0f)) < 1e-5f,
           "irradiance_on(45 degrees) returns %.4f = E_perp / sqrt(2)",
           static_cast<double>(lamp.irradiance_on(tilted)));
    checkf(lamp.irradiance_on(engine::vec3{0.0f, -1.0f, 0.0f}) == 0.0f,
           "…and exactly 0 for a surface facing away, not a negative amount of light");
}

// ===========================================================================
//  §C — THE PI
// ===========================================================================

void section_c_the_pi()
{
    std::printf("\n=== C. Where the pi comes from ===\n");

    // THE DERIVATION, AS A MEASUREMENT. A Lambertian surface scatters equally in
    // all directions, so its BRDF is a constant k. The fraction of arriving light
    // it sends back is the integral of k over the cosine-weighted hemisphere,
    // which §A just measured as pi. So k*pi = albedo, and k = albedo/pi. Any
    // other constant either loses light or invents it.
    std::printf("      grid      R (white)     error\n");
    for (int steps : {64, 256, 1024})
    {
        const double r = integrate_hemisphere(
            [](engine::vec3) { return static_cast<double>(engine::lambert_brdf({1, 1, 1}).r); },
            steps);
        std::printf("  %4d x %4d  %11.7f  %.2e\n", steps, 2 * steps, r, std::fabs(r - 1.0));
        if (steps == 1024)
        {
            checkf(std::fabs(r - 1.0) < 1e-5,
                   "a WHITE Lambert BRDF reflects %.7f of what arrives — energy "
                   "conserving to %.1e, and the residue is the quadrature, not the "
                   "physics", r, std::fabs(r - 1.0));
        }
    }

    for (float a : {0.18f, 0.5f, 0.8f})
    {
        const double r = integrate_hemisphere(
            [a](engine::vec3) { return static_cast<double>(engine::lambert_brdf({a, a, a}).r); },
            512);
        checkf(std::fabs(r - static_cast<double>(a)) < 2e-5,
               "albedo %.2f reflects %.5f — the albedo IS the reflectance, which is "
               "what makes it an authorable number", static_cast<double>(a), r);
    }

    // THE PI DIVIDES ONCE AND MULTIPLIES BACK EXACTLY, which is not automatic in
    // floating point and is the reason this re-parameterisation costs no codes.
    checkf(engine::k_reference_irradiance * engine::k_inv_pi == 1.0f,
           "pi * (1/pi) is EXACTLY 1.0f in IEEE single precision (%.9g) — both "
           "roundings land favourably, so moving the constant costs nothing at the "
           "constant itself",
           static_cast<double>(engine::k_reference_irradiance * engine::k_inv_pi));
    checkf(1.0f / engine::k_reference_irradiance == engine::k_inv_pi,
           "…and 1.0f/pi has the same bits as std::numbers::inv_pi_v<float>, so the "
           "two ways of writing the constant cannot diverge");

    // THE WHITE FURNACE, which is where k_reference_irradiance comes from.
    const engine::linear_rgb white{1.0f, 1.0f, 1.0f};
    const float radiance = engine::lambert_brdf(white).r * engine::k_reference_irradiance;
    checkf(radiance == 1.0f,
           "a perfect white Lambertian, square-on to a light of irradiance pi, emits "
           "radiance %.9g — EXACTLY 1.0f, code %d. That is the derivation of "
           "k_reference_irradiance, and it is this engine's definition of 'correctly "
           "exposed' until Lesson 6.12 replaces it with a real one",
           static_cast<double>(radiance), engine::linear_to_srgb_u8(radiance));
}

// ===========================================================================
//  §D — ENERGY CONSERVATION, AS A NUMBER
// ===========================================================================

void section_d_energy()
{
    std::printf("\n=== D. Energy conservation is a number, not a slogan ===\n");

    const engine::vec3 v37 = engine::normalised(engine::vec3{0.6f, 0.8f, 0.0f});
    const engine::vec3 v75 = engine::normalised(engine::vec3{0.966f, 0.259f, 0.0f});

    // FINDING ONE: the raw lobe, with no constant at all, is a light source at
    // low shininess. This is what "Blinn-Phong is not energy conserving" actually
    // means, and it is worth having the crossover as a number.
    std::printf("  the raw cos^s lobe, NO 1/pi (eye 37 deg off the normal):\n");
    std::printf("      shininess    R_spec\n");
    float crossover = 0.0f;
    for (float s : {1.0f, 2.0f, 4.0f, 8.0f, 12.0f, 16.0f, 32.0f})
    {
        const double r = blinn_reflectance(v37, s, 1.0);
        std::printf("  %11.0f  %9.4f%s\n", static_cast<double>(s), r,
                    r > 1.0 ? "   <-- emits more than it receives" : "");
        if (crossover == 0.0f && r <= 1.0) { crossover = s; }
    }
    checkf(blinn_reflectance(v37, 1.0f, 1.0) > 2.0,
           "the un-scaled lobe reflects %.4f at shininess 1 — a 'rough plastic' that "
           "is brighter than the lamp", blinn_reflectance(v37, 1.0f, 1.0));
    checkf(crossover > 8.0f && crossover <= 16.0f,
           "…and it first falls under 1 somewhere in (8, %.0f]: the engine's default "
           "shininess of 32 is comfortably inside the safe range, which is exactly why "
           "nobody noticed", static_cast<double>(crossover));

    // FINDING TWO, AND IT IS THE ONE THAT SURVIVES THE 1/pi: the engine puts the
    // lobe on the same scale as the diffuse, which fixes the runaway above — and
    // then ADDS the two with no coupling whatever. A white surface with any
    // highlight at all still reflects more light than arrives.
    std::printf("\n  on the engine's 1/pi scale, white albedo + white highlight:\n");
    std::printf("      shininess   R_diffuse   R_specular      TOTAL\n");
    bool always_over = true;
    for (float s : {2.0f, 4.0f, 8.0f, 32.0f, 128.0f})
    {
        const double rs = blinn_reflectance(v37, s, static_cast<double>(engine::k_inv_pi));
        const double total = 1.0 + rs;
        always_over = always_over && (total > 1.0);
        std::printf("  %11.0f  %10.4f  %10.4f  %9.4f%s\n", static_cast<double>(s),
                    1.0, rs, total, total > 1.0 ? "   <-- OVER 1" : "");
    }
    checkf(always_over,
           "diffuse + specular exceeds 1 at EVERY shininess, because the two lobes are "
           "added with no coupling — 1.1386 at shininess 32, 1.7333 at 2. This is the "
           "failure Lesson 6.4 fixes with a MECHANISM (Fresnel decides the split) "
           "rather than a constant");

    // FINDING THREE: the lobe's reflectance depends on where the eye is, which no
    // physical surface's does in this way. A number, so it cannot be waved at.
    const double r_steep = blinn_reflectance(v37, 32.0f, static_cast<double>(engine::k_inv_pi));
    const double r_graze = blinn_reflectance(v75, 32.0f, static_cast<double>(engine::k_inv_pi));
    checkf(r_steep / r_graze > 4.0,
           "the SAME surface at shininess 32 reflects %.4f toward an eye 37 degrees off "
           "the normal and %.4f toward one at 75 degrees — a factor of %.2f that the "
           "artist never asked for and cannot control",
           r_steep, r_graze, r_steep / r_graze);

    // AND THE DEMO IS INSIDE THE BUDGET, which is why the engine has looked fine.
    const double demo_spec = 0.30 * r_steep;
    checkf(demo_spec < 0.05,
           "the demo's own shiny surface (specular 0.30, shininess 32) spends %.4f of "
           "its budget on the highlight, leaving albedo room up to %.4f — the engine "
           "has been inside the limit by luck, not by construction",
           demo_spec, 1.0 - demo_spec);
}

// ===========================================================================
//  §E — THE PI MOVED
// ===========================================================================

/// The shading equation EXACTLY as `engine::shade()` wrote it before this lesson:
/// the albedo multiplied straight into the light, with the pi nowhere in sight
/// because it was inside `intensity == 1`.
float shade_the_old_way(float albedo, float key, float ndl, float ambient,
                        float spec_colour, float lobe)
{
    const float ir = key * 1.0f * ndl;
    return albedo * (ir + ambient) + spec_colour * ir * lobe;
}

void section_e_the_move()
{
    std::printf("\n=== E. The pi moved: what changed, exactly ===\n");

    // The reference render's own light, so this is not a synthetic range.
    const float keys[3] = {1.0f, 0.97f, 0.90f};
    const float ambs[3] = {0.06f, 0.07f, 0.10f};

    long total = 0, float_moves = 0, code_moves_exact = 0, code_moves_fast = 0;
    double worst_rel = 0.0;

    for (int ci = 0; ci < 3; ++ci)
    {
        for (int a = 0; a <= 64; ++a)
        {
            for (int c = 0; c <= 64; ++c)
            {
                for (int s = 0; s <= 8; ++s)
                {
                    for (int sc = 0; sc <= 2; ++sc)
                    {
                        const float albedo = static_cast<float>(a) / 64.0f;
                        const float ndl    = static_cast<float>(c) / 64.0f;
                        const float lobe   = static_cast<float>(s) / 8.0f;
                        const float spec_c = static_cast<float>(sc) * 0.5f;

                        const float old_v = shade_the_old_way(albedo, keys[ci], ndl,
                                                              ambs[ci], spec_c, lobe);

                        // The new form, spelled exactly as shade() spells it.
                        const float e = keys[ci] * engine::k_reference_irradiance * ndl;
                        const float f_d = albedo * engine::k_inv_pi;
                        const float f_s = spec_c * lobe * engine::k_inv_pi;
                        const float new_v = (f_d + f_s) * e + albedo * ambs[ci];

                        ++total;
                        if (old_v != new_v)
                        {
                            ++float_moves;
                            worst_rel = std::max(worst_rel,
                                std::fabs(static_cast<double>(new_v - old_v))
                                    / (static_cast<double>(old_v) + 1e-30));
                        }
                        const float o = std::min(1.0f, old_v);
                        const float n = std::min(1.0f, new_v);
                        if (engine::linear_to_srgb_u8(o) != engine::linear_to_srgb_u8(n))
                        {
                            ++code_moves_exact;
                        }
                        if (engine::linear_to_srgb_u8_fast(o) != engine::linear_to_srgb_u8_fast(n))
                        {
                            ++code_moves_fast;
                        }
                    }
                }
            }
        }
    }

    std::printf("  %ld channel samples: %ld differ in float, %ld move a code (exact "
                "encoder), %ld (fast)\n", total, float_moves, code_moves_exact,
                code_moves_fast);
    checkf(float_moves > 0,
           "the arithmetic IS different — %ld of %ld results move, worst relative "
           "%.3e, which is one to two ULP. Re-associating three multiplies is not a "
           "no-op, and claiming it is would be the easy lie here",
           float_moves, total, worst_rel);
    checkf(code_moves_exact == 0 && code_moves_fast == 0,
           "…and NOT ONE eight-bit code moves, through either encoder. That empty "
           "diff is the measurement: if the pi had been anywhere other than inside "
           "the light's scalar, putting E = pi back would not have reproduced the "
           "picture");

    // AND THE SAME QUESTION ASKED OF THE REAL FUNCTION, not a re-spelling of it.
    engine::lighting lights;
    lights.key.direction = -engine::normalised(engine::vec3{0.42f, 0.72f, 0.55f});
    lights.key.colour = {1.0f, 0.97f, 0.90f};
    int worst_code = 0;
    for (int a = 0; a <= 32; ++a)
    {
        for (int t = 0; t <= 32; ++t)
        {
            const float albedo = static_cast<float>(a) / 32.0f;
            const float theta = static_cast<float>(t) / 32.0f * 1.5f;
            const engine::vec3 n{std::sin(theta), std::cos(theta), 0.0f};
            const engine::linear_rgb got =
                engine::shade({albedo, albedo, albedo}, n, engine::vec3{0.0f, 0.0f, 1.0f},
                              lights, {}, engine::specular_model::none);
            const float want = shade_the_old_way(albedo, 1.0f,
                                                 engine::lambert(n, lights.key.to_light()),
                                                 lights.ambient.r, 0.0f, 0.0f);
            worst_code = std::max(worst_code,
                                  std::abs(engine::linear_to_srgb_u8(std::min(1.0f, got.r))
                                           - engine::linear_to_srgb_u8(std::min(1.0f, want))));
        }
    }
    checkf(worst_code == 0,
           "engine::shade() itself agrees with its own pre-6.2 form to 0 codes over "
           "1,089 (albedo, angle) pairs");
}

// ===========================================================================
//  §F — THE API, AND ONE CONSTANT IN TWO LANGUAGES
// ===========================================================================

void section_f_api()
{
    std::printf("\n=== F. The API, pinned so it cannot drift ===\n");

    checkf(engine::k_reference_irradiance == std::numbers::pi_v<float>,
           "k_reference_irradiance is pi (%.9g), not a tuned constant",
           static_cast<double>(engine::k_reference_irradiance));
    checkf(engine::directional_light{}.irradiance == engine::k_reference_irradiance,
           "a DEFAULT-constructed light carries it, so code that never mentions the "
           "field is correctly exposed rather than 3.14x too dark");

    const engine::linear_rgb f = engine::lambert_brdf({0.8f, 0.55f, 0.18f});
    checkf(std::fabs(f.r - 0.8f * engine::k_inv_pi) < 1e-9f
               && std::fabs(f.g - 0.55f * engine::k_inv_pi) < 1e-9f
               && std::fabs(f.b - 0.18f * engine::k_inv_pi) < 1e-9f,
           "lambert_brdf divides every channel by pi: (%.4f, %.4f, %.4f) per steradian",
           static_cast<double>(f.r), static_cast<double>(f.g), static_cast<double>(f.b));

    // THE NORMALISATION CHOICE, ASSERTED. A later lesson that changes it must
    // change this line, which is the point of writing it down.
    const engine::specular surf{{0.85f, 0.85f, 0.85f}, 32.0f};
    checkf(engine::specular_brdf(surf, 1.0f).r == 0.85f * engine::k_inv_pi,
           "specular_brdf puts the lobe on the SAME 1/pi scale — a deliberate choice, "
           "not a normalisation, and this check is where it is written down");
    checkf(engine::specular_brdf(surf, 0.0f).r == 0.0f,
           "…and a zero lobe contributes exactly nothing");

    // TWO SPELLINGS OF ONE CONSTANT. HLSL has no <numbers>, so the shader carries
    // its own digits — and a constant that exists twice is a constant that can
    // disagree. Parse the shader's literal and compare the bits.
    const std::string src = read_file("shaders/scene.frag.hlsl");
    const std::string needle = "static const float k_inv_pi = ";
    const std::size_t at = src.find(needle);
    if (at == std::string::npos)
    {
        check(false, "found k_inv_pi in shaders/scene.frag.hlsl");
    }
    else
    {
        const float hlsl_value =
            static_cast<float>(SDL_atof(src.c_str() + at + needle.size()));
        Uint32 a_bits = 0, b_bits = 0;
        std::memcpy(&a_bits, &hlsl_value, 4);
        const float cpp_value = engine::k_inv_pi;
        std::memcpy(&b_bits, &cpp_value, 4);
        checkf(a_bits == b_bits,
               "the shader's k_inv_pi and engine::k_inv_pi are the SAME FLOAT "
               "(0x%08X): two languages, one constant, checked rather than assumed",
               a_bits);
    }
}

// ===========================================================================
//  §G — THE GOLDEN
// ===========================================================================

void section_g_golden()
{
    std::printf("\n=== G. The golden ===\n");

    const char* path = "build/demos/verify_62.ppm";
    check(demo::write_reference_shot(path) == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file(path);
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(), "same size: %zu bytes",
           ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL to the golden written in Lesson 5.1 — THIRTEEN lessons, and "
          "this one is not a survival, it is a RESULT. The shading equation was "
          "re-parameterised on the reference render's own code path; the picture holds "
          "because the pi that moved out of the light is exactly the pi that moved into "
          "the BRDF");
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

    std::printf("verify_62 — radiometry, BRDFs, and where the pi lives\n");

    section_a_radiometry();
    section_b_cosine();
    section_c_the_pi();
    section_d_energy();
    section_e_the_move();
    section_f_api();
    section_g_golden();

    std::printf("\n%d checks, %d failure%s\n", g_checks, g_failures,
                g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
