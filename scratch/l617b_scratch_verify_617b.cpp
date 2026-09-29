// scratch/verify_617b.cpp — every number Lesson 6.17b quotes, produced here.
//
//   sh scratch/build_verify_617b.sh                                # as configured
//   ENGINE_CFLAGS="-O2 -DNDEBUG" sh scratch/build_verify_617b.sh   # release, for §J's times
//
// §A  the inverse square: a sphere's area, then measured three ways
// §B  the range window: three published falloffs, compared at the edge
// §C  the spot cone: interpolated in cosine, and what that does in angle
// §D  one BRDF, two kinds of light — and the golden, which cannot see any of this
// §E  the cube face cameras: the mirror, measured, with look_at as the control
// §F  the perspective bias, against ray-cast ground truth
// §G  both renderers, one lit ground, pixel for pixel
// §H  additive: with no lamps nothing that existed can move
// §I  the frame graph: seven passes declared, then a frame that declares none
// §J  the budget, and what a lamp's shadow costs
// §K  the sun's reach: §G's finding in the sun's own lookup (the fix after 6.17b)
// §L  one sun map and no cascade block: the same picture as with one (the second fix)
//
// §F AND §G ARE THE TWO THAT MATTER. §F is the lesson's derivation made to face
// a ground truth that shares no code with it: a ray from each ground point to the
// lamp, tested against the caster's box analytically. §G is the port's claim: the
// same lamps through `light.hpp`'s maths and `scene.frag.hlsl`'s, compared in
// linear light, with a control that proves the comparison can see a 5% change.
#include "../demos/common/demo_scene.hpp"

#include <engine/gfx/cubemap.hpp>
#include <engine/gfx/frame_graph.hpp>
#include <engine/gfx/gpu_post.hpp>
#include <engine/gfx/gpu_scene.hpp>
#include <engine/gfx/gpu_shader.hpp>
#include <engine/gfx/gpu_shadow.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/shadow.hpp>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

using engine::cube_face;
using engine::light_camera;
using engine::linear_rgb;
using engine::local_light;
using engine::local_light_kind;
using engine::mat4;
using engine::vec3;
using engine::vec4;

namespace {

int g_checks = 0;
int g_failures = 0;

void checkf(bool ok, const char* fmt, ...)
{
    ++g_checks;
    if (!ok) { ++g_failures; }
    std::printf("  [%s] ", ok ? "PASS" : "FAIL");
    std::va_list args;
    va_start(args, fmt);
    std::vprintf(fmt, args);
    va_end(args);
    std::printf("\n");
}

void section(const char* title)
{
    std::printf("\n===========================================================================\n");
    std::printf("  §%s\n", title);
    std::printf("===========================================================================\n");
}

constexpr float k_pi = 3.14159265358979f;

/// A deterministic stream of numbers in [0, 1) — the same every run, so every
/// count this file prints is reproducible to the unit.
struct lcg
{
    Uint32 s = 0x6117bu;
    float next()
    {
        s = s * 1664525u + 1013904223u;
        return static_cast<float>(s >> 8) / 16777216.0f;
    }
};

/// Side of every image this file renders or dumps, in pixels.
constexpr int k_size = 256;

/// Write an RGB float image as a PPM, tonemapped by a plain clamp — for looking
/// at, never for measuring. Only when VERIFY617B_DUMP is set in the environment.
void dump_ppm(const char* path, const std::vector<float>& rgb)
{
    if (std::getenv("VERIFY617B_DUMP") == nullptr || rgb.empty()) { return; }
    std::FILE* f = std::fopen(path, "wb");
    if (f == nullptr) { return; }
    std::fprintf(f, "P6\n%d %d\n255\n", k_size, k_size);
    for (float v : rgb)
    {
        const float c = std::clamp(v, 0.0f, 1.0f);
        const unsigned char b = static_cast<unsigned char>(
            std::lround(255.0 * (c <= 0.0031308f ? 12.92 * c : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055)));
        std::fputc(b, f);
    }
    std::fclose(f);
}

struct compare_result
{
    int lit = 0;         ///< pixels where either side is above 1e-4
    int agree = 0;       ///< within tolerance, relative to the larger
    int differ = 0;
    double worst = 0.0;       ///< worst relative difference, no absolute floor
};

// ---------------------------------------------------------------------------
//  Shared scene pieces
// ---------------------------------------------------------------------------

/// A horizontal ground square at y = 0, normal +y, `half` metres from centre to
/// edge. Built ALREADY LYING DOWN rather than as `quad_mesh` plus a rotation,
/// because the rotation's type is one of the things a later lesson changes
/// (`transform::rotation` becomes a quaternion in 7.5) — this file has to
/// compile at 6.17b's point in the course and at HEAD, and geometry that needs
/// no rotation is geometry both can place.
engine::mesh_data ground_data(float half)
{
    engine::mesh_data g;
    g.vertices = {{-half, 0.0f, -half}, {-half, 0.0f, half}, {half, 0.0f, half}, {half, 0.0f, -half}};
    g.normals = {{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    g.uvs = {{0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}};
    // Counter-clockwise seen from ABOVE (+y), the course's front face.
    g.indices = {0, 1, 2, 0, 2, 3};
    return g;
}

/// World-from-model for a translation and a per-axis scale, written as columns.
mat4 placed(vec3 p, vec3 s)
{
    return mat4{{s.x, 0.0f, 0.0f, 0.0f},
                {0.0f, s.y, 0.0f, 0.0f},
                {0.0f, 0.0f, s.z, 0.0f},
                {p.x, p.y, p.z, 1.0f}};
}

/// The scene §F-§I share: an 8 m ground, a 0.6 m box floating over it, and two
/// lamps both casting shadows of the box onto the ground.
constexpr float k_ground_half = 4.0f;
constexpr float k_box_size = 0.6f;
const vec3 k_box_centre{0.3f, 0.6f, 0.2f};

/// A THIN SIGN standing on the floor, 4 mm thick — the contact case.
///
/// TWO PREDICTIONS WERE REFUSED BEFORE THIS ONE, and both are kept here because
/// together they are the physics of peter-panning. The first stood a solid
/// 0.5 m crate here: along the ray, the floor just behind a solid crate lies
/// half a metre beyond the crate's lit face, and no bias of a few centimetres
/// reaches it. The second stood a 3 cm wall: the floor behind its base is
/// within the bias of the lit face only for (0.71 x 6.0 cm) - 3.0 cm = 1.3 cm,
/// within the judge's two-texel angular neighbourhood of the wall's bottom
/// edge, which the map rightly cannot resolve. Peter-panning is a failure of THIN casters; a 4 mm sign
/// leaves a predicted band of about 3.9 cm.
const vec3 k_wall_centre{0.9f, 0.3f, 0.9f};
const vec3 k_wall_size{0.004f, 0.6f, 0.8f};   // thin in x, facing the spot

local_light scene_point()
{
    local_light l;
    l.kind = local_light_kind::point;
    l.position = {1.2f, 1.5f, 0.8f};
    l.colour = {1.0f, 0.78f, 0.52f};
    // pi at 1.5 m: a white surface right below it renders at exactly 1.0.
    l.intensity = engine::k_reference_irradiance * 1.5f * 1.5f;
    l.range = 6.0f;
    l.casts_shadow = true;
    return l;
}

local_light scene_spot()
{
    local_light l;
    l.kind = local_light_kind::spot;
    l.position = {-2.0f, 2.5f, -0.5f};
    const vec3 aim{0.3f, 0.0f, 0.2f};
    l.direction = engine::normalised(aim - l.position);
    l.colour = {0.55f, 0.75f, 1.0f};
    const float reach = engine::length(aim - l.position);
    l.intensity = engine::k_reference_irradiance * reach * reach;
    l.inner_angle = 0.25f;
    l.outer_angle = 0.40f;
    l.range = 8.0f;
    l.casts_shadow = true;
    return l;
}

/// Does the segment from `p` to `lamp` pass through the box? Slab test: the
/// ground truth §F measures the shadow maps against, sharing no code with them.
bool box_blocks(vec3 p, vec3 lamp, vec3 centre, vec3 size)
{
    const vec3 h = size * 0.5f;
    const float o[3] = {p.x, p.y, p.z};
    const float d[3] = {lamp.x - p.x, lamp.y - p.y, lamp.z - p.z};
    const float lo[3] = {centre.x - h.x, centre.y - h.y, centre.z - h.z};
    const float hi[3] = {centre.x + h.x, centre.y + h.y, centre.z + h.z};
    float t0 = 0.0f;
    float t1 = 1.0f;
    for (int a = 0; a < 3; ++a)
    {
        if (std::fabs(d[a]) < 1e-12f)
        {
            if (o[a] < lo[a] || o[a] > hi[a]) { return false; }
            continue;
        }
        float ta = (lo[a] - o[a]) / d[a];
        float tb = (hi[a] - o[a]) / d[a];
        if (ta > tb) { std::swap(ta, tb); }
        t0 = std::max(t0, ta);
        t1 = std::min(t1, tb);
        if (t0 > t1) { return false; }
    }
    return true;
}

// ===========================================================================
//  §A — the inverse square
// ===========================================================================

void section_a()
{
    section("A  THE INVERSE SQUARE — a sphere's area, then measured three ways");

    local_light lamp;
    lamp.position = {0.0f, 0.0f, 0.0f};
    lamp.intensity = engine::k_reference_irradiance;
    lamp.range = 0.0f;   // infinite: no window, so this section sees 1/d^2 alone

    // ---- 1. What a white surface facing the lamp renders at ----------------
    std::printf("  a lamp of intensity pi, a white Lambertian surface facing it:\n");
    std::printf("  %8s %12s %12s %12s\n", "d (m)", "E (W/m^2)", "rendered", "1/d^2");
    float at[3] = {0.0f, 0.0f, 0.0f};
    int k = 0;
    for (float d : {1.0f, 2.0f, 4.0f})
    {
        const vec3 p{0.0f, -d, 0.0f};
        const engine::local_light_sample s = engine::sample_local_light(lamp, p);
        const float e = lamp.intensity * s.attenuation;
        // `specular_model::none` is the pure Lambert path — 3.6's picture — so
        // the rendered value is (albedo / pi) * E and nothing else.
        const linear_rgb c = engine::shade_local({1.0f, 1.0f, 1.0f}, p, {0.0f, 1.0f, 0.0f},
                                                 {0.0f, 1.0f, 0.0f}, lamp, {},
                                                 engine::specular_model::none);
        std::printf("  %8.1f %12.6f %12.6f %12.6f\n", static_cast<double>(d),
                    static_cast<double>(e), static_cast<double>(c.r),
                    static_cast<double>(1.0f / (d * d)));
        at[k++] = c.r;
    }
    checkf(std::fabs(at[0] - 1.0f) < 1e-6f,
           "at 1 m the lamp IS the sun: a white surface renders %.7f (6.2's pi, cashed)",
           static_cast<double>(at[0]));
    checkf(std::fabs(at[1] / at[0] - 0.25f) < 1e-6f && std::fabs(at[2] / at[0] - 0.0625f) < 1e-6f,
           "doubling the distance quarters the light: %.6f and %.6f of the 1 m value",
           static_cast<double>(at[1] / at[0]), static_cast<double>(at[2] / at[0]));

    // ---- 2. The derivation, as a measurement --------------------------------
    //
    // The whole law is "the same power crosses every sphere". Integrate the
    // irradiance over spheres of three radii: if attenuation is 1/d^2, every
    // one of them collects 4 * pi * I, independent of the radius.
    std::printf("\n  flux through a sphere around the lamp (should be 4*pi*I = %.5f):\n",
                static_cast<double>(4.0f * k_pi * lamp.intensity));
    const int nt = 400;
    const int np = 800;
    double worst_rel = 0.0;
    for (float r : {0.5f, 2.0f, 8.0f})
    {
        double flux = 0.0;
        const double dt = k_pi / nt;
        const double dp = 2.0 * k_pi / np;
        for (int i = 0; i < nt; ++i)
        {
            const double th = (i + 0.5) * dt;
            for (int j = 0; j < np; ++j)
            {
                const double ph = (j + 0.5) * dp;
                const vec3 p{static_cast<float>(r * std::sin(th) * std::cos(ph)),
                             static_cast<float>(r * std::cos(th)),
                             static_cast<float>(r * std::sin(th) * std::sin(ph))};
                const engine::local_light_sample s = engine::sample_local_light(lamp, p);
                // The surface faces the lamp (n = -p/r), so n.l = 1 exactly.
                const double e = static_cast<double>(lamp.intensity * s.attenuation);
                flux += e * r * r * std::sin(th) * dt * dp;
            }
        }
        const double want = 4.0 * k_pi * static_cast<double>(lamp.intensity);
        const double rel = std::fabs(flux - want) / want;
        worst_rel = std::max(worst_rel, rel);
        std::printf("    r = %4.1f m   flux %.5f   relative error %.2e\n",
                    static_cast<double>(r), flux, rel);
    }
    checkf(worst_rel < 1e-4, "every sphere collects the same power, to %.1e — the law, measured",
           worst_rel);

    // ---- 3. The singularity, and the 1 cm floor ------------------------------
    const float a5 = engine::inverse_square(0.005f * 0.005f);
    const float a20 = engine::inverse_square(0.02f * 0.02f);
    checkf(std::fabs(a5 - 10000.0f) < 1e-1f && std::fabs(a20 - 2500.0f) < 1e-1f,
           "inside 1 cm the curve stops rising: %.0f at 5 mm (unclamped: 40000), %.0f at 2 cm",
           static_cast<double>(a5), static_cast<double>(a20));

    // ---- 4. A far point light IS a directional light ------------------------
    //
    // Put the lamp D metres straight up with intensity E * D^2, and look at the
    // irradiance over a 1 m square patch beneath it. Straight below it is E;
    // at the patch corner, r = 0.7071 m off axis, the distance grows and the
    // cosine shrinks, so the irradiance is E * (D/d)^3.
    std::printf("\n  a point light D metres above a 1 m patch vs the sun (E = pi):\n");
    std::printf("  %8s %16s %16s\n", "D (m)", "worst deviation", "(D/d)^3 predicts");
    double dev10 = 0.0;
    for (float big_d : {2.0f, 10.0f, 100.0f})
    {
        local_light far;
        far.position = {0.0f, big_d, 0.0f};
        far.intensity = engine::k_reference_irradiance * big_d * big_d;
        far.range = 0.0f;
        double worst = 0.0;
        for (int i = 0; i <= 10; ++i)
        {
            for (int j = 0; j <= 10; ++j)
            {
                const vec3 p{-0.5f + 0.1f * i, 0.0f, -0.5f + 0.1f * j};
                const engine::local_light_sample s = engine::sample_local_light(far, p);
                const double e = static_cast<double>(far.intensity * s.attenuation)
                               * std::max(0.0, static_cast<double>(s.to_light.y));
                worst = std::max(worst, std::fabs(e / engine::k_reference_irradiance - 1.0));
            }
        }
        const double dd = std::sqrt(big_d * big_d + 0.5);
        const double pred = 1.0 - std::pow(big_d / dd, 3.0);
        std::printf("  %8.0f %15.4f%% %15.4f%%\n", static_cast<double>(big_d), 100.0 * worst,
                    100.0 * pred);
        if (big_d == 10.0f) { dev10 = worst; }
    }
    checkf(dev10 < 0.0076 && dev10 > 0.0074,
           "at 10 m the lamp is a sun to %.3f%% across the patch — (D/d)^3, as derived",
           100.0 * dev10);
}

// ===========================================================================
//  §B — the range window
// ===========================================================================

void section_b()
{
    section("B  THE RANGE WINDOW — three published falloffs, compared at the edge");

    const float r = 10.0f;
    std::printf("  window value at a fraction x of the range (r = 10 m):\n");
    std::printf("  %6s %16s %14s\n", "x", "Karis/Frostbite", "glTF recipe");
    for (float x : {0.25f, 1.0f / 3.0f, 0.5f, 0.7f, 0.9f, 0.99f, 1.0f})
    {
        const float d = x * r;
        const float ours = engine::range_window(d * d, r);
        const float gltf = std::clamp(1.0f - x * x * x * x, 0.0f, 1.0f);
        std::printf("  %6.3f %16.6f %14.6f\n", static_cast<double>(x),
                    static_cast<double>(ours), static_cast<double>(gltf));
    }
    const float w_half = engine::range_window(25.0f, r);
    const float w_edge = engine::range_window(100.0f, r);
    checkf(std::fabs(w_half - 0.87890625f) < 1e-6f && w_edge == 0.0f,
           "the engine's window is (1 - x^4)^2: %.6f at half the range, exactly 0 at it",
           static_cast<double>(w_half));

    // ---- The edge, as a slope ------------------------------------------------
    //
    // E(d) = I * w(d) / d^2. Just inside the range, how steeply is it falling?
    // For the glTF recipe the window's own slope (-4/r) survives and E drops
    // TWICE as steeply as the inverse square does there; one step later it is
    // flat at zero. A slope that jumps is a crease, and a crease is a ring.
    const double h = 1e-4;
    auto e_ours = [&](double d) {
        return static_cast<double>(engine::range_window(static_cast<float>(d * d), r))
             / (d * d);
    };
    auto e_gltf = [&](double d) {
        const double x = d / r;
        return std::clamp(1.0 - x * x * x * x, 0.0, 1.0) / (d * d);
    };
    const double slope_ours = (e_ours(r - h) - e_ours(r - 2 * h)) / h;
    const double slope_gltf = (e_gltf(r - h) - e_gltf(r - 2 * h)) / h;
    const double slope_inv = -2.0 / (r * r * r);
    std::printf("\n  dE/dd just inside the range, per unit intensity (r = 10 m):\n");
    std::printf("    inverse square alone     %+.6f   (-2/r^3)\n", slope_inv);
    std::printf("    glTF recipe              %+.6f   (-4/r^3: twice as steep, then 0)\n",
                slope_gltf);
    std::printf("    Karis / Frostbite        %+.6f   (0: no crease)\n", slope_ours);
    checkf(std::fabs(slope_gltf / slope_inv - 2.0) < 0.01 && std::fabs(slope_ours) < 1e-5,
           "glTF's edge falls %.3fx as steeply as the inverse square; the squared window's %.1e",
           slope_gltf / slope_inv, slope_ours);

    // ---- Unreal's +1, which removes the singularity by changing the curve ----
    std::printf("\n  Karis 2013 eq. 9's 1/(d^2 + 1), as a fraction of the true 1/d^2:\n");
    bool floor_exact = true;
    for (float d : {0.5f, 1.0f, 2.0f, 5.0f, 10.0f})
    {
        const float ue = 1.0f / (d * d + 1.0f);
        const float ours = engine::inverse_square(d * d);
        std::printf("    d = %5.1f m   +1 form %.4f of true   1 cm floor %.4f of true\n",
                    static_cast<double>(d), static_cast<double>(ue * d * d),
                    static_cast<double>(ours * d * d));
        if (std::fabs(ours * d * d - 1.0f) > 1e-6f) { floor_exact = false; }
    }
    checkf(floor_exact,
           "the 1 cm floor changes nothing beyond 1 cm; the +1 halves the light at 1 m");
}

// ===========================================================================
//  §C — the spot cone
// ===========================================================================

void section_c()
{
    section("C  THE SPOT CONE — interpolated in cosine, and what that does in angle");

    local_light spot;
    spot.kind = local_light_kind::spot;
    spot.inner_angle = 0.30f;
    spot.outer_angle = 0.45f;
    const engine::cone_terms t = engine::cone_terms_of(spot);

    std::printf("  inner 0.30 rad (%.2f deg), outer 0.45 rad (%.2f deg); scale %.4f offset %.4f\n",
                static_cast<double>(0.30f * 180.0f / k_pi),
                static_cast<double>(0.45f * 180.0f / k_pi), static_cast<double>(t.scale),
                static_cast<double>(t.offset));
    std::printf("  %10s %10s %18s\n", "angle", "factor", "(linear in angle)^2");
    for (float a : {0.0f, 0.20f, 0.30f, 0.3375f, 0.375f, 0.4125f, 0.45f, 0.50f})
    {
        const float f = engine::spot_cone(std::cos(a), t);
        const float u = std::clamp((0.45f - a) / 0.15f, 0.0f, 1.0f);
        std::printf("  %10.4f %10.5f %18.5f\n", static_cast<double>(a),
                    static_cast<double>(f), static_cast<double>(u * u));
    }
    const float at_axis = engine::spot_cone(1.0f, t);
    const float at_inner = engine::spot_cone(std::cos(0.30f), t);
    const float at_outer = engine::spot_cone(std::cos(0.45f), t);
    const float at_mid = engine::spot_cone(std::cos(0.375f), t);
    checkf(at_axis == 1.0f && std::fabs(at_inner - 1.0f) < 1e-4f && at_outer < 1e-4f,
           "1 on the axis, %.5f at the inner angle, %.5f at the outer",
           static_cast<double>(at_inner), static_cast<double>(at_outer));
    checkf(std::fabs(at_mid - 0.300f) < 0.002f,
           "halfway in ANGLE the cone gives %.4f, not 0.25 — cosine is not linear in angle",
           static_cast<double>(at_mid));

    // ---- How much of the sphere a cone is ------------------------------------
    std::printf("\n  solid angle of a cone of half-angle theta, 2*pi*(1 - cos theta):\n");
    for (float deg : {15.0f, 30.0f, 45.0f, 90.0f})
    {
        const float th = deg * k_pi / 180.0f;
        const float omega = 2.0f * k_pi * (1.0f - std::cos(th));
        std::printf("    %5.1f deg   %.4f sr   %.2f%% of the sphere\n",
                    static_cast<double>(deg), static_cast<double>(omega),
                    static_cast<double>(100.0f * omega / (4.0f * k_pi)));
    }
    const float omega45 = 2.0f * k_pi * (1.0f - std::cos(0.25f * k_pi));
    checkf(std::fabs(omega45 - 1.8403f) < 1e-3f,
           "glTF's default 45-degree cone is %.4f sr: a mask over 14.64%% of the sphere",
           static_cast<double>(omega45));

    // A spot is a masked point light: on the axis they agree to the bit.
    local_light point = spot;
    point.kind = local_light_kind::point;
    spot.position = point.position = {0.0f, 3.0f, 0.0f};
    spot.direction = {0.0f, -1.0f, 0.0f};
    const float as = engine::sample_local_light(spot, {0.0f, 0.0f, 0.0f}).attenuation;
    const float ap = engine::sample_local_light(point, {0.0f, 0.0f, 0.0f}).attenuation;
    checkf(as == ap, "on its axis a spot light is its point light exactly (%.6f == %.6f)",
           static_cast<double>(as), static_cast<double>(ap));
}

// ===========================================================================
//  §D — one BRDF, two kinds of light
// ===========================================================================

std::string slurp(const char* path)
{
    std::ifstream f(path, std::ios::binary);
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}

void section_d()
{
    section("D  ONE BRDF, TWO KINDS OF LIGHT — and the golden, which cannot see this");

    // A lamp placed a kilometre along the sun's reverse direction, with intensity
    // E * D^2, delivers the sun's irradiance from the sun's direction to a point
    // at the origin. If `shade_local` and `shade` share one BRDF, they agree to
    // float rounding, whatever the surface.
    engine::lighting sun;
    sun.key.direction = engine::normalised(vec3{-0.3f, -1.0f, 0.2f});
    sun.key.colour = {1.0f, 0.9f, 0.8f};
    sun.key.irradiance = engine::k_reference_irradiance;
    sun.ambient = {0.0f, 0.0f, 0.0f};

    const float big_d = 1000.0f;
    local_light lamp;
    lamp.position = sun.key.direction * (-big_d);
    lamp.colour = sun.key.colour;
    lamp.intensity = sun.key.irradiance * big_d * big_d;
    lamp.range = 0.0f;

    const engine::microsurface surfaces[3] = {
        {.roughness = 0.4f, .metallic = 0.0f, .f0 = 0.04f},
        {.roughness = 0.2f, .metallic = 1.0f, .f0 = 0.04f},
        {.roughness = 0.9f, .metallic = 0.0f, .f0 = 0.04f}};
    const engine::specular_model models[2] = {engine::specular_model::cook_torrance,
                                              engine::specular_model::blinn};
    lcg rng;
    double worst = 0.0;
    int evaluated = 0;
    for (int k = 0; k < 400; ++k)
    {
        const vec3 n = engine::normalised(vec3{rng.next() - 0.5f, rng.next() + 0.2f,
                                               rng.next() - 0.5f});
        const vec3 v = engine::normalised(vec3{rng.next() - 0.5f, rng.next() + 0.1f,
                                               rng.next() - 0.5f});
        for (const engine::microsurface& s : surfaces)
        {
            for (engine::specular_model m : models)
            {
                const linear_rgb albedo{0.7f, 0.5f, 0.3f};
                const linear_rgb a = engine::shade(albedo, n, v, sun, s, m);
                const linear_rgb b = engine::shade_local(albedo, {0.0f, 0.0f, 0.0f}, n, v,
                                                         lamp, s, m);
                const float big = std::max({a.r, a.g, a.b});
                if (big < 1e-4f) { continue; }
                const double rel = std::max({std::fabs(a.r - b.r), std::fabs(a.g - b.g),
                                             std::fabs(a.b - b.b)}) / big;
                worst = std::max(worst, rel);
                ++evaluated;
            }
        }
    }
    // Not bit-identical, and the reason is worth knowing: the lamp's direction is
    // `normalise(position - p)` with coordinates of a thousand metres, so it
    // differs from the sun's in the last bits — and a GGX lobe at roughness 0.2
    // (alpha 0.04) turns a 1e-7 change in n.h into a relative change hundreds of
    // times larger. The bound is therefore on the ROUNDING, not on equality.
    checkf(evaluated > 1000 && worst < 1e-4,
           "a lamp 1 km out and the sun agree to %.2e over %d lit evaluations (one BRDF)",
           worst, evaluated);

    // ---- The golden ---------------------------------------------------------
    //
    // A NULL INSTRUMENT, and the argument is structural: the reference fixture
    // builds `fill_style` with an empty `local_lights`, so the new loop runs zero
    // times; the only other thing 6.17b changed on that path is `shade()`'s BRDF
    // moving into `surface_brdf`, statement for statement. Run anyway, because
    // "the refactor cannot have moved a bit" is exactly the sentence a test is
    // for — with a control, so an empty read cannot print IDENTICAL.
    const int rc = demo::write_reference_shot("scratch/golden617b.ppm");
    const std::string ours = slurp("scratch/golden617b.ppm");
    const std::string golden = slurp("scratch/shot_52.ppm");
    std::size_t differing = (ours.size() == golden.size()) ? 0u : ours.size() + golden.size();
    if (differing == 0)
    {
        for (std::size_t i = 0; i < ours.size(); ++i) { differing += (ours[i] != golden[i]) ? 1u : 0u; }
    }
    checkf(rc == 0 && !ours.empty() && differing == 0,
           "the reference render is BYTE-IDENTICAL (%zu bytes, %zu differing) — golden E917C06C",
           ours.size(), differing);
    std::string flipped = golden;
    if (!flipped.empty()) { flipped[flipped.size() / 2] ^= 0x01; }
    std::size_t control = 0;
    for (std::size_t i = 0; i < flipped.size() && i < golden.size(); ++i)
    {
        control += (flipped[i] != golden[i]) ? 1u : 0u;
    }
    checkf(control == 1, "control: one flipped bit reports %zu differing byte", control);
}

// ===========================================================================
//  §E — the cube face cameras
// ===========================================================================

float det3(const mat4& m)
{
    return m.at(0, 0) * (m.at(1, 1) * m.at(2, 2) - m.at(1, 2) * m.at(2, 1))
         - m.at(0, 1) * (m.at(1, 0) * m.at(2, 2) - m.at(1, 2) * m.at(2, 0))
         + m.at(0, 2) * (m.at(1, 0) * m.at(2, 1) - m.at(1, 1) * m.at(2, 0));
}

/// Project `q` through `clip_from_world` and return the TEXTURE uv (v down)
/// and the device depth: the mapping `scene.frag.hlsl` and `viewport::to_screen`
/// both use.
vec3 uv_of(const mat4& clip_from_world, vec3 q)
{
    const vec4 c = clip_from_world * engine::point(q);
    const vec3 ndc = engine::perspective_divide(c);
    return {ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f, ndc.z};
}

void section_e()
{
    section("E  THE CUBE FACE CAMERAS — the mirror, measured, with look_at as the control");

    local_light bulb;
    bulb.position = {0.3f, 1.2f, -0.7f};
    bulb.range = 5.0f;
    const int res = 256;

    std::printf("  %5s %10s %13s %14s %13s\n", "face", "det(ours)", "det(look_at)",
                "worst uv err", "look_at err");
    float worst = 0.0f;
    float worst_control = 1.0f;
    float worst_depth = 0.0f;
    bool dets_ok = true;
    for (int f = 0; f < engine::k_cube_faces; ++f)
    {
        const cube_face face = static_cast<cube_face>(f);
        const light_camera cam = engine::fit_cube_face(bulb, face, res);
        const engine::cube_axes ax = engine::cube_face_axes(face);

        // THE CONTROL: what you would write first. look_at along the major
        // axis with the face's image-up as the up hint, then a 90-degree
        // perspective with the same planes.
        const mat4 lk = engine::look_at(bulb.position, bulb.position + ax.major, -ax.v);
        const mat4 control = engine::perspective(0.5f * k_pi, 1.0f, cam.near_plane,
                                                 cam.far_plane) * lk;

        float face_worst = 0.0f;
        float face_control = 0.0f;
        for (int i = 0; i < 33; ++i)
        {
            for (int j = 0; j < 33; ++j)
            {
                const float u = (i + 0.5f) / 33.0f;
                const float v = (j + 0.5f) / 33.0f;
                const vec3 dir = engine::cube_to_direction(face, u, v);
                const vec3 q = bulb.position + dir * 2.5f;
                const vec3 got = uv_of(cam.clip_from_world, q);
                face_worst = std::max({face_worst, std::fabs(got.x - u), std::fabs(got.y - v)});
                const vec3 bad = uv_of(control, q);
                face_control = std::max({face_control, std::fabs(bad.x - u),
                                         std::fabs(bad.y - v)});

                // Depth: the face's w IS the major-axis distance, so the stored
                // depth is predictable from the direction alone.
                const float axial = std::max({std::fabs(dir.x), std::fabs(dir.y),
                                              std::fabs(dir.z)}) * 2.5f;
                worst_depth = std::max(worst_depth,
                                       std::fabs(got.z - engine::perspective_depth(
                                                             axial, cam.near_plane,
                                                             cam.far_plane)));
            }
        }
        const float d_ours = det3(cam.view_from_world);
        const float d_lk = det3(lk);
        if (std::fabs(d_ours + 1.0f) > 1e-5f || std::fabs(d_lk - 1.0f) > 1e-5f) { dets_ok = false; }
        std::printf("  %5s %10.4f %13.4f %14.2e %13.4f\n", engine::name_of(face),
                    static_cast<double>(d_ours), static_cast<double>(d_lk),
                    static_cast<double>(face_worst), static_cast<double>(face_control));
        worst = std::max(worst, face_worst);
        worst_control = std::min(worst_control, face_control);
    }
    checkf(dets_ok, "every face's view is a MIRROR (det -1); look_at's is a rotation (det +1)");
    checkf(worst < 2e-6f,
           "1,089 directions per face land on the texel direction_to_cube reads, to %.1e",
           static_cast<double>(worst));
    checkf(worst_control > 0.9f,
           "control: look_at's faces are mirrored — every face misses by up to %.3f of the face",
           static_cast<double>(worst_control));
    checkf(worst_depth < 2e-6f,
           "the stored depth is perspective_depth(major axis) to %.1e — no matrix needed to read it",
           static_cast<double>(worst_depth));

    // ---- Coverage: every direction belongs to exactly the face that picks it -
    lcg rng;
    int outside = 0;
    const int n = 20000;
    for (int k = 0; k < n; ++k)
    {
        const vec3 d{rng.next() * 2.0f - 1.0f, rng.next() * 2.0f - 1.0f, rng.next() * 2.0f - 1.0f};
        if (engine::dot(d, d) < 1e-6f) { continue; }
        const cube_face f = engine::direction_to_cube(d).face;
        const light_camera cam = engine::fit_cube_face(bulb, f, res);
        const vec4 c = cam.clip_from_world
                     * engine::point(bulb.position + engine::normalised(d) * 3.0f);
        const vec3 ndc = engine::perspective_divide(c);
        if (std::fabs(ndc.x) > 1.0f + 1e-5f || std::fabs(ndc.y) > 1.0f + 1e-5f
            || ndc.z < 0.0f || ndc.z > 1.0f)
        {
            ++outside;
        }
    }
    checkf(outside == 0,
           "%d random directions: %d fall outside the frustum of the face that owns them",
           n, outside);
}

// ===========================================================================
//  §F — the perspective bias, against ray-cast ground truth
// ===========================================================================

struct cpu_scene
{
    engine::mesh_pool pool;
    std::vector<engine::scene_object> objects;
};

cpu_scene make_cpu_scene(bool with_box)
{
    cpu_scene s;
    engine::scene_object ground{};
    ground.geometry = s.pool.insert(ground_data(k_ground_half));
    ground.name = "ground";
    ground.closed = false;
    s.objects.push_back(ground);
    if (with_box)
    {
        engine::scene_object box{};
        box.geometry = s.pool.insert(engine::with_normals(engine::cube_mesh(),
                                                          engine::normal_style::flat));
        box.name = "box";
        box.xform.position = k_box_centre;
        box.xform.scale = {k_box_size, k_box_size, k_box_size};
        s.objects.push_back(box);

        engine::scene_object wall = box;
        wall.name = "wall";
        wall.xform.position = k_wall_centre;
        wall.xform.scale = k_wall_size;
        s.objects.push_back(wall);
    }
    return s;
}

struct shadow_errors
{
    int judged = 0;   ///< unambiguous ground points inside the lamp's reach
    int acne = 0;     ///< truth lit, map says shadowed
    int leak = 0;     ///< truth shadowed, map says lit (peter-panning)
};

/// The ray-cast truth: is `p` hidden from the lamp by either box?
bool occluded(vec3 p, vec3 lamp)
{
    return box_blocks(p, lamp, k_box_centre, vec3{k_box_size, k_box_size, k_box_size})
        || box_blocks(p, lamp, k_wall_centre, k_wall_size);
}

/// Walk a 600x600 grid (1.33 cm) over the ground, and for every point the lamp
/// reaches and whose truth is UNAMBIGUOUS, compare the map's answer with the ray's.
///
/// "Unambiguous" is measured IN THE LIGHT'S ANGLES, because that is where a
/// shadow map's error lives: the map resolves DIRECTIONS from the lamp, one
/// texel apart, and a caster's silhouette in it can be a whole texel from
/// where the ray says it is (half from the CPU rasterizer snapping vertices to
/// whole pixels, half from the nearest sample). So the ray from the lamp to the
/// point is tilted by +-2 texel angles in two perpendicular directions — at the
/// point's own distance — and the truth must survive all four tilts.
///
/// FOUR VERSIONS OF THIS JUDGE WERE WRONG FIRST, each caught by probing a point
/// rather than excusing it. A fixed 6 cm on the floor (a texel's shadow at
/// grazing incidence is wider); 1.5 texels on the floor; 2 texels on the floor
/// — which failed on a point whose ray clears the box's top edge by a fifth of
/// a texel and then travels another three metres, so that no floor position
/// within half a metre of it is anywhere near the box's shadow: the ambiguity
/// was never on the floor, it was in the angle; and 2 texels of angle sampled
/// only at its ends, which stepped over the half-texel-wide sign.
constexpr float k_judge_texels = 2.0f;

template <typename Visibility>
shadow_errors judge(const local_light& lamp, float texel_angle, Visibility vis_of)
{
    shadow_errors e;
    constexpr int k_grid = 600;
    const float step = 2.0f * k_ground_half / static_cast<float>(k_grid);
    for (int i = 0; i < k_grid; ++i)
    {
        for (int j = 0; j < k_grid; ++j)
        {
            const vec3 p{-k_ground_half + (i + 0.5f) * step, 0.0f,
                         -k_ground_half + (j + 0.5f) * step};
            const engine::local_light_sample s = engine::sample_local_light(lamp, p);
            if (s.attenuation <= 0.0f || s.to_light.y <= 0.02f) { continue; }

            const bool truth = occluded(p, lamp.position);

            // Two directions perpendicular to the ray, and the point re-placed
            // along each tilted ray at its own distance from the lamp.
            const vec3 ray = -s.to_light;
            const vec3 helper = (std::fabs(ray.y) < 0.9f) ? vec3{0.0f, 1.0f, 0.0f}
                                                          : vec3{1.0f, 0.0f, 0.0f};
            const vec3 t1 = engine::normalised(engine::cross(ray, helper));
            const vec3 t2 = engine::cross(ray, t1);
            const float dist = std::sqrt(s.distance_sq);
            bool ambiguous = false;
            // DENSELY, every quarter texel out to two: a caster thinner than a
            // texel (the 4 mm sign is half of one) slips between tilts of +-2,
            // which is the very aliasing the map itself suffers from — the
            // fourth version of this judge was caught by exactly that.
            for (int k = 1; k <= static_cast<int>(4.0f * k_judge_texels) && !ambiguous; ++k)
            {
                const float tilt = 0.25f * static_cast<float>(k) * texel_angle;
                for (const vec3 off : {t1 * tilt, t1 * (-tilt), t2 * tilt, t2 * (-tilt)})
                {
                    const vec3 q = lamp.position + engine::normalised(ray + off) * dist;
                    if (occluded(q, lamp.position) != truth) { ambiguous = true; }
                }
            }
            if (ambiguous) { continue; }

            ++e.judged;
            const float v = vis_of(p, s.to_light.y);
            if (!truth && v < 0.5f) { ++e.acne; }
            if (truth && v > 0.5f) { ++e.leak; }
        }
    }
    return e;
}

void section_f()
{
    section("F  THE PERSPECTIVE BIAS — derived, and judged against ray-cast truth");

    const cpu_scene scene = make_cpu_scene(true);
    const engine::aabb bounds = engine::shadow_map::bounds_of(scene.objects, scene.pool);
    const local_light spot = scene_spot();
    const int res = 512;
    const light_camera cam = engine::fit_spot(spot, res);

    std::printf("  spot: near %.2f m, far %.1f m, %d texels, %.3f mm per texel at 1 m\n",
                static_cast<double>(cam.near_plane), static_cast<double>(cam.far_plane), res,
                static_cast<double>(1000.0f * cam.world_per_texel));

    // ---- The bias the derivation asks for, at a 45-degree surface -----------
    std::printf("\n  what one tap needs on a 45-degree surface, and what 6.8's formula gives:\n");
    std::printf("  %6s %10s %10s %14s %14s %9s\n", "d (m)", "texel mm", "bias mm",
                "derived dev", "6.8 as-is dev", "ratio");
    const float reach = engine::pcf_reach_texels(0);
    for (float d : {0.5f, 1.0f, 2.0f, 4.0f, 8.0f})
    {
        const float texel = cam.world_per_texel * d;
        const float metres = reach * texel * 1.0f;
        const float derived = engine::perspective_depth(d, cam.near_plane, cam.far_plane)
                            - engine::perspective_depth(d - metres, cam.near_plane, cam.far_plane);
        const float naive = engine::slope_scaled_bias(cam.world_per_texel, 1.0f,
                                                      cam.far_plane - cam.near_plane, reach);
        std::printf("  %6.1f %10.3f %10.3f %14.3e %14.3e %9.3f\n", static_cast<double>(d),
                    static_cast<double>(1000.0f * texel), static_cast<double>(1000.0f * metres),
                    static_cast<double>(derived), static_cast<double>(naive),
                    static_cast<double>(naive / derived));
    }
    // THE CROSSOVER CARRIES A HIDDEN METRE. The ratio is d * (1 m) / (near *
    // far): 6.8's formula reads `world_per_texel` as THE footprint, and under
    // perspective it is the footprint at one metre, so the two agree exactly
    // where the fragment's footprint is the one-metre one scaled by n*f/(1 m).
    std::printf("  (6.8's formula is right at one distance only, d = near * far / 1 m = %.2f m;\n"
                "   inside it too small, outside it too large, in proportion to the distance)\n",
                static_cast<double>(cam.near_plane * cam.far_plane));

    // ---- The maps, four ways -------------------------------------------------
    //
    // WRITTEN BEFORE THE RUN (the third time — two refused predictions are
    // recorded at `k_wall_centre`): 6.8's formula is 1.2x to 20x too LARGE at
    // every distance here, so it should show no acne and should LEAK a band of
    // floor about 3.9 cm wide behind the thin sign's base.
    //
    // The SAME depth map every time — the render only reads the camera's two
    // matrices — and four ways of asking it. The "as-is" camera is the spot's
    // own fit with the perspective flag cleared: exactly what reusing 6.8's
    // `visibility` on a spot light's map would do.
    struct policy
    {
        const char* name;
        engine::shadow_bias bias;
        bool perspective;
    };
    const policy policies[] = {
        {"no bias (the acne)", engine::shadow_bias::none, true},
        {"6.8's formula, as-is", engine::shadow_bias::slope_scaled, false},
        {"derived, per fragment", engine::shadow_bias::slope_scaled, true},
        {"normal offset", engine::shadow_bias::normal_offset, true},
    };

    shadow_errors results[4];
    std::printf("\n  %-24s %8s %8s %8s\n", "spot light, 512 map", "judged", "acne", "leak");
    for (int k = 0; k < 4; ++k)
    {
        engine::shadow_map m;
        if (!m.create(res)) { checkf(false, "shadow_map::create"); return; }
        engine::shadow_settings set;
        set.bias = policies[k].bias;
        set.pcf_radius = 0;
        m.settings() = set;
        light_camera c = cam;
        c.perspective = policies[k].perspective;
        m.render(scene.objects, scene.pool, c, bounds);

        results[k] = judge(spot, cam.world_per_texel, [&](vec3 p, float n_dot_l) {
            return m.visibility(p, vec3{0.0f, 1.0f, 0.0f}, n_dot_l);
        });
        std::printf("  %-24s %8d %8d %8d\n", policies[k].name, results[k].judged,
                    results[k].acne, results[k].leak);
    }
    checkf(results[0].acne > results[0].judged / 4,
           "with no bias, %d of %d judged points are acne — the failure is real and large",
           results[0].acne, results[0].judged);
    checkf(results[1].acne == 0 && results[1].leak > results[2].leak,
           "6.8's formula, unmodified: %d acne and %d leaking behind the sign's base (derived: %d)",
           results[1].acne, results[1].leak, results[2].leak);
    checkf(results[2].acne == 0 && results[2].leak == 0,
           "derived per fragment: %d acne, %d leaking, of %d judged", results[2].acne,
           results[2].leak, results[2].judged);

    // ---- The same derivation through a CUBE --------------------------------
    engine::local_shadow_set set;
    set.settings().point_resolution = 256;
    const local_light bulb = scene_point();
    const std::vector<local_light> lamps{bulb};
    set.render(scene.objects, scene.pool, lamps);
    const float cube_texel = 2.0f / static_cast<float>(set.settings().point_resolution);
    const shadow_errors cube = judge(bulb, cube_texel, [&](vec3 p, float n_dot_l) {
        return set.visibility(0, p, vec3{0.0f, 1.0f, 0.0f}, n_dot_l);
    });
    std::printf("  %-24s %8d %8d %8d\n", "point light, 6 x 256", cube.judged, cube.acne, cube.leak);
    checkf(cube.acne == 0 && cube.leak == 0,
           "the point light's six faces, one bias rule: %d acne, %d leaking, of %d judged",
           cube.acne, cube.leak, cube.judged);

    // ---- The vertex snap: gltf_view's floor under its bulb ------------------
    //
    // A 7.2 m floor 1.2 m below a point light, and NO casters: every shadowed
    // point is acne. `slope_scale = 0.5` turns the snap-aware reach (r + 1)
    // sqrt(2) back into 6.8's (r + 1/2) sqrt(2) exactly, which is the control:
    // the reach this lesson started with, on the scene that exposed it.
    {
        engine::mesh_pool fpool;
        engine::mesh_data f;
        const vec3 c{0.98f, -1.3f, -0.04f};
        const float h = 3.6f;
        f.vertices = {{c.x - h, c.y, c.z - h}, {c.x - h, c.y, c.z + h},
                      {c.x + h, c.y, c.z + h}, {c.x + h, c.y, c.z - h}};
        f.normals = {{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
        f.indices = {0, 1, 2, 0, 2, 3};
        engine::scene_object floor{};
        floor.geometry = fpool.insert(f);
        floor.name = "floor";
        floor.closed = false;
        const std::vector<engine::scene_object> fobjs{floor};

        local_light over;
        over.position = {2.3f, -0.1f, 0.8f};
        over.range = 6.0f;
        over.casts_shadow = true;
        const std::vector<local_light> one{over};

        int acne[2] = {0, 0};
        int judged = 0;
        for (int pass = 0; pass < 2; ++pass)
        {
            engine::local_shadow_set fs;
            fs.settings().bias.slope_scale = (pass == 0) ? 0.5f : 1.0f;
            fs.render(fobjs, fpool, one);
            judged = 0;
            for (int i = 0; i < 360; ++i)
            {
                for (int j = 0; j < 360; ++j)
                {
                    const vec3 p{c.x - h + (i + 0.5f) * (2.0f * h / 360.0f), c.y,
                                 c.z - h + (j + 0.5f) * (2.0f * h / 360.0f)};
                    const engine::local_light_sample ls = engine::sample_local_light(over, p);
                    if (ls.attenuation <= 0.0f) { continue; }
                    ++judged;
                    if (fs.visibility(0, p, vec3{0.0f, 1.0f, 0.0f}, ls.to_light.y) < 0.5f)
                    {
                        ++acne[pass];
                    }
                }
            }
        }
        if (std::getenv("VERIFY617B_DUMP") != nullptr)
        {
            // Top-down visibility of the bare floor, both reaches: white lit,
            // black shadowed — every black pixel is acne, since nothing casts.
            for (int pass = 0; pass < 2; ++pass)
            {
                engine::local_shadow_set fs;
                fs.settings().bias.slope_scale = (pass == 0) ? 0.5f : 1.0f;
                fs.render(fobjs, fpool, one);
                std::vector<float> img(static_cast<std::size_t>(k_size) * k_size * 3u, 0.0f);
                for (int j = 0; j < k_size; ++j)
                {
                    for (int i = 0; i < k_size; ++i)
                    {
                        const vec3 p{c.x - h + (i + 0.5f) * (2.0f * h / k_size), c.y,
                                     c.z - h + (j + 0.5f) * (2.0f * h / k_size)};
                        const engine::local_light_sample ls = engine::sample_local_light(over, p);
                        const float v = (ls.attenuation <= 0.0f) ? 0.2f
                            : fs.visibility(0, p, vec3{0.0f, 1.0f, 0.0f}, ls.to_light.y);
                        const std::size_t o = (static_cast<std::size_t>(j) * k_size + i) * 3u;
                        img[o] = img[o + 1] = img[o + 2] = v;
                    }
                }
                dump_ppm(pass == 0 ? "build/demos/verify617b_floor_68.ppm"
                                   : "build/demos/verify617b_floor_snap.ppm", img);
            }
        }
        std::printf("  %-24s %8d %8d %8s\n", "floor, 6.8's reach", judged, acne[0], "-");
        std::printf("  %-24s %8d %8d %8s\n", "floor, + half-texel snap", judged, acne[1], "-");
        checkf(acne[0] > 100 && acne[1] == 0,
               "a bare floor under a bulb: %d points of acne with 6.8's reach, %d with the snap's half texel",
               acne[0], acne[1]);
    }

    set.settings().bias.bias = engine::shadow_bias::none;
    set.render(scene.objects, scene.pool, lamps);
    const shadow_errors cube_none = judge(bulb, cube_texel, [&](vec3 p, float n_dot_l) {
        return set.visibility(0, p, vec3{0.0f, 1.0f, 0.0f}, n_dot_l);
    });
    std::printf("  %-24s %8d %8d %8d\n", "  ...with no bias", cube_none.judged, cube_none.acne,
                cube_none.leak);
    checkf(cube_none.acne > cube_none.judged / 4,
           "control: the same cube with no bias shows %d acne — the judge can see it",
           cube_none.acne);
}

} // namespace

// ===========================================================================
//  Device-side plumbing — verify_617's shape
// ===========================================================================

namespace {

struct target
{
    engine::gpu_device* dev = nullptr;
    SDL_GPUTexture* colour = nullptr;
    SDL_GPUTransferBuffer* readback = nullptr;
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_INVALID;
    int w = 0;
    int h = 0;

    [[nodiscard]] Uint32 pixel_bytes() const
    {
        return (format == SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT) ? 16u : 8u;
    }

    bool create(engine::gpu_device& d, int width, int height, SDL_GPUTextureFormat fmt)
    {
        dev = &d;
        w = width;
        h = height;
        format = fmt;

        SDL_GPUTextureCreateInfo ti{};
        ti.type = SDL_GPU_TEXTURETYPE_2D;
        ti.format = fmt;
        ti.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        ti.width = static_cast<Uint32>(width);
        ti.height = static_cast<Uint32>(height);
        ti.layer_count_or_depth = 1;
        ti.num_levels = 1;
        ti.sample_count = SDL_GPU_SAMPLECOUNT_1;
        colour = SDL_CreateGPUTexture(d.handle(), &ti);
        if (colour == nullptr) { return false; }

        SDL_GPUTransferBufferCreateInfo tb{};
        tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        tb.size = static_cast<Uint32>(width * height) * pixel_bytes();
        readback = SDL_CreateGPUTransferBuffer(d.handle(), &tb);
        return readback != nullptr;
    }

    void destroy()
    {
        if (dev == nullptr) { return; }
        if (readback != nullptr) { SDL_ReleaseGPUTransferBuffer(dev->handle(), readback); }
        if (colour != nullptr) { SDL_ReleaseGPUTexture(dev->handle(), colour); }
        readback = nullptr;
        colour = nullptr;
    }
};

/// binary16 -> float, for a device that cannot render to RGBA32F.
float half_to_float(Uint16 h)
{
    const Uint32 sign = static_cast<Uint32>(h & 0x8000u) << 16;
    const Uint32 exp = (h >> 10) & 0x1Fu;
    const Uint32 man = h & 0x3FFu;
    float f;
    if (exp == 0) { f = std::ldexp(static_cast<float>(man), -24); }
    else if (exp == 31) { f = man ? NAN : INFINITY; }
    else { f = std::ldexp(static_cast<float>(man | 0x400u), static_cast<int>(exp) - 25); }
    Uint32 bits;
    std::memcpy(&bits, &f, 4);
    bits |= sign;
    std::memcpy(&f, &bits, 4);
    return f;
}

/// Record a download of the target onto `cb`, submit it, wait, and return RGB
/// floats per pixel, top row first.
std::vector<float> download(target& t, SDL_GPUCommandBuffer* cb)
{
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cb);
    SDL_GPUTextureRegion src{};
    src.texture = t.colour;
    src.w = static_cast<Uint32>(t.w);
    src.h = static_cast<Uint32>(t.h);
    src.d = 1;
    SDL_GPUTextureTransferInfo dst{};
    dst.transfer_buffer = t.readback;
    dst.pixels_per_row = static_cast<Uint32>(t.w);
    dst.rows_per_layer = static_cast<Uint32>(t.h);
    SDL_DownloadFromGPUTexture(copy, &src, &dst);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cb);
    if (fence == nullptr) { return {}; }
    SDL_WaitForGPUFences(t.dev->handle(), true, &fence, 1);
    SDL_ReleaseGPUFence(t.dev->handle(), fence);

    const std::size_t n = static_cast<std::size_t>(t.w) * static_cast<std::size_t>(t.h);
    std::vector<float> rgb(n * 3u);
    const Uint8* mapped = static_cast<const Uint8*>(
        SDL_MapGPUTransferBuffer(t.dev->handle(), t.readback, false));
    if (mapped == nullptr) { return {}; }
    for (std::size_t i = 0; i < n; ++i)
    {
        for (std::size_t c = 0; c < 3u; ++c)
        {
            if (t.pixel_bytes() == 16u)
            {
                float f;
                std::memcpy(&f, mapped + i * 16u + c * 4u, 4);
                rgb[i * 3u + c] = f;
            }
            else
            {
                Uint16 hbits;
                std::memcpy(&hbits, mapped + i * 8u + c * 2u, 2);
                rgb[i * 3u + c] = half_to_float(hbits);
            }
        }
    }
    SDL_UnmapGPUTransferBuffer(t.dev->handle(), t.readback);
    return rgb;
}

/// Everything the GPU sections need, created once.
struct rig
{
    engine::gpu_device gpu;
    engine::gpu_mesh ground;
    engine::gpu_mesh box;
    engine::gpu_scene_renderer scene;
    engine::gpu_local_shadows shadows;
    engine::gpu_sampler samp;
    engine::gpu_stream_buffer lights;
    target image;
    SDL_GPUTextureFormat depth_format = SDL_GPU_TEXTUREFORMAT_INVALID;
    engine::local_shadow_settings settings{};
    bool ok = false;
};

bool build_rig(rig& r)
{
    if (!r.gpu.create(nullptr, false).ok()) { return false; }

    engine::gpu_shader scene_v, scene_f, shadow_v, shadow_f;
    const bool shaders =
        scene_v.load(r.gpu, "scene.vert", engine::shader_stage::vertex)
        && scene_f.load(r.gpu, "scene.frag", engine::shader_stage::fragment)
        && shadow_v.load(r.gpu, "shadow.vert", engine::shader_stage::vertex)
        && shadow_f.load(r.gpu, "shadow.frag", engine::shader_stage::fragment);
    if (!shaders) { std::printf("  (shaders did not load)\n"); return false; }

    static constexpr SDL_GPUTextureFormat k_depth_candidates[] = {
        SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        SDL_GPU_TEXTUREFORMAT_D24_UNORM,
        SDL_GPU_TEXTUREFORMAT_D16_UNORM};
    r.depth_format = engine::supported_shadow_format(
        r.gpu, k_depth_candidates, static_cast<int>(std::size(k_depth_candidates)));
    if (r.depth_format == SDL_GPU_TEXTUREFORMAT_INVALID) { return false; }

    // FULL FLOAT WHERE THE DEVICE ALLOWS, so the comparison in §G is limited by
    // the maths and not by a 10-bit mantissa. Half float is the fallback.
    SDL_GPUTextureFormat colour = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    if (!SDL_GPUTextureSupportsFormat(r.gpu.handle(), colour, SDL_GPU_TEXTURETYPE_2D,
                                      SDL_GPU_TEXTUREUSAGE_COLOR_TARGET))
    {
        colour = engine::k_hdr_format;
    }
    if (!r.scene.create(r.gpu, scene_v.handle(), scene_f.handle(), r.depth_format, colour))
    {
        std::printf("  (scene renderer did not create)\n");
        return false;
    }
    if (!r.shadows.create(r.gpu, shadow_v.handle(), shadow_f.handle(), r.depth_format,
                          r.settings, 4, 2))
    {
        std::printf("  (gpu_local_shadows did not create)\n");
        return false;
    }
    if (!r.samp.create(r.gpu)) { return false; }
    if (!r.lights.create(r.gpu, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                         static_cast<Uint32>(8 * sizeof(engine::gpu_local_light)),
                         "verify617b lights"))
    {
        return false;
    }
    if (!r.image.create(r.gpu, k_size, k_size, colour)) { return false; }

    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
    if (cb == nullptr) { return false; }
    const engine::mesh_data g = ground_data(k_ground_half);
    const engine::mesh_data b = engine::with_normals(engine::cube_mesh(),
                                                     engine::normal_style::flat);
    const bool mesh_ok =
        r.ground.create(r.gpu, cb, g.view(), engine::index_mode::indexed, "ground")
        && r.box.create(r.gpu, cb, b.view(), engine::index_mode::indexed, "box");
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cb);
    if (fence != nullptr)
    {
        SDL_WaitForGPUFences(r.gpu.handle(), true, &fence, 1);
        SDL_ReleaseGPUFence(r.gpu.handle(), fence);
    }
    if (!mesh_ok) { return false; }

    std::printf("\n  device: %s; colour %s; depth %s\n",
                SDL_GetGPUDeviceDriver(r.gpu.handle()),
                (colour == SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT) ? "RGBA32F" : "RGBA16F",
                engine::name_of(r.depth_format));
    r.ok = true;
    return true;
}

/// The camera: straight down from 10 m, orthographic over the whole ground, so
/// pixel (i, j) sees exactly one known ground point.
const vec3 k_eye{0.0f, 10.0f, 0.0f};

mat4 top_view()
{
    return engine::look_at(k_eye, vec3{0.0f, 0.0f, 0.0f}, vec3{0.0f, 0.0f, -1.0f});
}

mat4 top_clip()
{
    return engine::orthographic(-k_ground_half, k_ground_half, -k_ground_half, k_ground_half,
                                1.0f, 20.0f) * top_view();
}

/// The world point pixel (i, j)'s centre sees — by inverting the view, so the
/// answer does not depend on remembering which way `look_at` puts +x.
vec3 world_of_pixel(int i, int j)
{
    const float nx = (i + 0.5f) / k_size * 2.0f - 1.0f;
    const float ny = 1.0f - (j + 0.5f) / k_size * 2.0f;
    const vec4 w = engine::rigid_inverse(top_view())
                 * engine::point(vec3{nx * k_ground_half, ny * k_ground_half, -10.0f});
    return {w.x, w.y, w.z};
}

const vec3 k_albedo{0.8f, 0.8f, 0.8f};
const engine::microsurface k_surface{.roughness = 0.5f, .metallic = 0.0f, .f0 = 0.04f};

engine::gpu_draw_item ground_item(const rig& r)
{
    engine::gpu_draw_item item{};
    item.mesh = &r.ground;
    item.world_from_model = mat4::identity();
    item.normal_from_model = engine::mat3::identity();
    item.material.albedo = k_albedo;
    item.material.roughness = k_surface.roughness;
    item.material.metallic = k_surface.metallic;
    item.material.f0 = k_surface.f0;
    item.material.textured = 0.0f;
    item.material.normal_mapped = 0.0f;
    item.material.alpha = 1.0f;
    item.material.alpha_cutoff = 0.0f;
    item.style = engine::surface_style::two_sided;
    return item;
}

engine::gpu_draw_item box_item(const rig& r, vec3 centre = k_box_centre,
                                vec3 size = vec3{k_box_size, k_box_size, k_box_size})
{
    engine::gpu_draw_item item = ground_item(r);
    item.mesh = &r.box;
    item.world_from_model = placed(centre, size);
    item.style = engine::surface_style::solid;
    return item;
}

/// The light block: no sun unless asked, no ambient, Cook-Torrance, no encode
/// — so the picture is the lamps and nothing else.
engine::scene_light_uniforms lamps_only_light(vec3 key = {0.0f, 0.0f, 0.0f})
{
    engine::scene_light_uniforms light{};
    light.to_light = engine::normalised(vec3{0.3f, 1.0f, 0.2f});
    light.key = key;
    light.ambient = vec3{0.0f, 0.0f, 0.0f};
    light.eye_world = k_eye;
    light.spec_model = 3.0f;
    light.encode_output = 0.0f;
    light.shadow_strength = 0.0f;
    return light;
}

/// Record the lamps' shadow passes and the scene pass the hand-written way,
/// and read the image back. `use_locals` false passes a null
/// `scene_local_lights` — the call every program before 6.17b makes.
std::vector<float> render_frame(rig& r, const std::vector<local_light>& lamps, bool use_locals,
                                vec3 key = {0.0f, 0.0f, 0.0f})
{
    std::vector<engine::gpu_local_light> records(lamps.size() + 1u);
    const int n = r.shadows.prepare(lamps, records);

    const engine::gpu_draw_item items[3] = {ground_item(r), box_item(r),
                                            box_item(r, k_wall_centre, k_wall_size)};

    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
    if (cb == nullptr) { return {}; }
    if (n > 0
        && !r.lights.write(cb, records.data(),
                           static_cast<Uint32>(n) * static_cast<Uint32>(sizeof(records[0]))))
    {
        return {};
    }
    r.shadows.render(cb, items, 3);   // the job list decides which passes exist

    if (!r.scene.ensure_depth(r.gpu, k_size, k_size)) { return {}; }
    SDL_GPUColorTargetInfo ct{};
    ct.texture = r.image.colour;
    ct.load_op = SDL_GPU_LOADOP_CLEAR;
    ct.store_op = SDL_GPU_STOREOP_STORE;
    ct.clear_color = SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f};
    const SDL_GPUDepthStencilTargetInfo dsi = r.scene.depth_target_info();
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cb, &ct, 1, &dsi);
    if (pass == nullptr) { return {}; }

    engine::gpu_scene_renderer::scene_local_lights locals;
    locals.lights = r.lights.handle();
    locals.count = n;
    locals.spot_shadows = r.shadows.spot_texture();
    locals.point_shadows = r.shadows.point_texture();
    locals.shadow_sampler = r.shadows.sampler();

    const engine::camera_uniforms camera{top_clip()};
    // The ground only: the box is drawn into the maps and not into the image,
    // so every pixel of the picture is a ground point §G can compute by hand.
    (void)r.scene.render(cb, pass, items, 1, camera, lamps_only_light(key), r.samp.handle(),
                         nullptr, nullptr, nullptr, nullptr, nullptr,
                         use_locals ? &locals : nullptr);
    SDL_EndGPURenderPass(pass);
    return download(r.image, cb);
}

/// The same frame on the CPU, one ground point per pixel, through `shade_local`
/// and a `local_shadow_set` rendered from the same box.
std::vector<float> cpu_frame(const std::vector<local_light>& lamps,
                             const engine::local_shadow_settings& settings,
                             std::vector<float>* visibility_out = nullptr)
{
    const cpu_scene scene = make_cpu_scene(true);
    engine::local_shadow_set set;
    set.settings() = settings;
    set.render(scene.objects, scene.pool, lamps);

    const std::size_t n = static_cast<std::size_t>(k_size) * static_cast<std::size_t>(k_size);
    std::vector<float> rgb(n * 3u, 0.0f);
    if (visibility_out != nullptr) { visibility_out->assign(n * lamps.size(), 1.0f); }
    const vec3 up{0.0f, 1.0f, 0.0f};
    for (int j = 0; j < k_size; ++j)
    {
        for (int i = 0; i < k_size; ++i)
        {
            const vec3 p = world_of_pixel(i, j);
            const std::size_t px = static_cast<std::size_t>(j) * k_size + static_cast<std::size_t>(i);
            linear_rgb sum{0.0f, 0.0f, 0.0f};
            for (std::size_t k = 0; k < lamps.size(); ++k)
            {
                const engine::local_light_sample s = engine::sample_local_light(lamps[k], p);
                float vis = 1.0f;
                if (lamps[k].casts_shadow && s.attenuation > 0.0f)
                {
                    vis = set.visibility(k, p, up, engine::dot(up, s.to_light));
                }
                if (visibility_out != nullptr) { (*visibility_out)[px * lamps.size() + k] = vis; }
                const linear_rgb c = engine::shade_local({k_albedo.x, k_albedo.y, k_albedo.z}, p,
                                                         up, k_eye - p, lamps[k], k_surface,
                                                         engine::specular_model::cook_torrance,
                                                         engine::ndf_model::ggx, vis);
                sum = {sum.r + c.r, sum.g + c.g, sum.b + c.b};
            }
            rgb[px * 3u] = sum.r;
            rgb[px * 3u + 1u] = sum.g;
            rgb[px * 3u + 2u] = sum.b;
        }
    }
    return rgb;
}

/// An ABSOLUTE floor under the relative tolerance: 1e-5 of linear light, a
/// thirtieth of the smallest non-zero sRGB code (code 1 is 3.0e-4 linear). At a
/// spot cone's rim the whole pixel can be 1e-4, and there a relative tolerance
/// measures float rounding in the cone's squared ramp rather than the renderer.
constexpr double k_abs_floor = 1e-5;

/// Per pixel: the largest channel difference over the largest channel value —
/// or 0 when the difference is below the absolute floor.
double pixel_rel(const std::vector<float>& a, const std::vector<float>& b, std::size_t i,
                 double* big_out)
{
    double big = 0.0;
    double diff = 0.0;
    for (std::size_t k = 0; k < 3u; ++k)
    {
        const double x = a[i * 3u + k];
        const double y = b[i * 3u + k];
        big = std::max({big, std::fabs(x), std::fabs(y)});
        diff = std::max(diff, std::fabs(x - y));
    }
    *big_out = big;
    if (diff <= k_abs_floor) { return 0.0; }
    return (big > 0.0) ? diff / big : 0.0;
}

compare_result compare(const std::vector<float>& a, const std::vector<float>& b, double tol)
{
    compare_result c;
    const std::size_t n = std::min(a.size(), b.size()) / 3u;
    for (std::size_t i = 0; i < n; ++i)
    {
        double big = 0.0;
        const double rel = pixel_rel(a, b, i, &big);
        if (big < 1e-4) { continue; }
        ++c.lit;
        double raw = 0.0;
        for (std::size_t k = 0; k < 3u; ++k)
        {
            raw = std::max(raw, std::fabs(static_cast<double>(a[i * 3u + k] - b[i * 3u + k])));
        }
        c.worst = std::max(c.worst, raw / big);
        if (rel <= tol) { ++c.agree; } else { ++c.differ; }
    }
    return c;
}

// ===========================================================================
//  §G — both renderers
// ===========================================================================

void section_g(rig& r)
{
    section("G  BOTH RENDERERS — one lit ground, the CPU's maths against the GPU's");

    const double tol = (r.image.pixel_bytes() == 16u) ? 2e-4 : 2e-3;

    // ---- 1. Without shadows: the lighting maths alone -----------------------
    std::vector<local_light> lamps{scene_point(), scene_spot()};
    for (local_light& l : lamps) { l.casts_shadow = false; }
    const std::vector<float> gpu_flat = render_frame(r, lamps, true);
    const std::vector<float> cpu_flat = cpu_frame(lamps, r.settings);
    const compare_result flat = compare(gpu_flat, cpu_flat, tol);
    checkf(flat.lit > 20000 && flat.differ == 0,
           "no shadows: %d lit pixels, %d agree to %.0e relative, worst %.2e",
           flat.lit, flat.agree, tol, flat.worst);

    // ---- 2. With shadows ------------------------------------------------------
    for (local_light& l : lamps) { l.casts_shadow = true; }
    const std::vector<float> gpu = render_frame(r, lamps, true);
    std::vector<float> vis;
    const std::vector<float> cpu = cpu_frame(lamps, r.settings, &vis);
    const compare_result shaded = compare(gpu, cpu, tol);
    dump_ppm("build/demos/verify617b_gpu.ppm", gpu);
    dump_ppm("build/demos/verify617b_cpu.ppm", cpu);
    if (std::getenv("VERIFY617B_DUMP") != nullptr)
    {
        // The disagreement mask: 1.0 where the two renderers differ, else 0.
        std::vector<float> mask(gpu.size(), 0.0f);
        for (std::size_t i = 0; i < gpu.size() / 3u; ++i)
        {
            double big = 0.0;
            const double rel = pixel_rel(gpu, cpu, i, &big);
            if (big >= 1e-4 && rel > tol) { mask[i * 3u] = 1.0f; }
        }
        dump_ppm("build/demos/verify617b_mask.ppm", mask);

        // The GPU frame with NO bias: the acne, for the figure.
        const engine::shadow_bias kept = r.shadows.settings().bias.bias;
        r.shadows.settings().bias.bias = engine::shadow_bias::none;
        dump_ppm("build/demos/verify617b_gpu_nobias.ppm", render_frame(r, lamps, true));
        r.shadows.settings().bias.bias = kept;
    }

    // Where do they disagree? A pixel is on a SHADOW EDGE when some lamp's CPU
    // visibility is fractional, or changes, within `radius` pixels of it. There a
    // nearest-texel lookup (CPU) and a bilinear comparison (GPU: hardware 2x2
    // PCF) are entitled to different answers — 6.8 §6 — and the GPU's footprint
    // is one TEXEL wider than the CPU's. On this floor one cube texel covers one
    // to two screen pixels, so the honest band is two pixels; both are reported.
    const std::size_t nl = lamps.size();
    int shadowed = 0;
    for (float v : vis) { shadowed += (v < 1.0f) ? 1 : 0; }
    auto edge_at = [&](int i, int j, int radius) {
        const std::size_t p = static_cast<std::size_t>(j) * k_size + static_cast<std::size_t>(i);
        for (std::size_t k = 0; k < nl; ++k)
        {
            const float v = vis[p * nl + k];
            if (v > 0.0f && v < 1.0f) { return true; }
            for (int dj = -radius; dj <= radius; ++dj)
            {
                for (int di = -radius; di <= radius; ++di)
                {
                    const int qi = i + di;
                    const int qj = j + dj;
                    if (qi < 0 || qj < 0 || qi >= k_size || qj >= k_size) { continue; }
                    const std::size_t q = static_cast<std::size_t>(qj) * k_size
                                        + static_cast<std::size_t>(qi);
                    if (vis[q * nl + k] != v) { return true; }
                }
            }
        }
        return false;
    };
    // THE DISTRIBUTION, not a verdict: every disagreeing pixel, binned by the
    // smallest band radius that contains it. Two effects set the width, and
    // both are known before the measurement: the GPU's filter reaches one texel
    // further than the CPU's, and the two maps sample depth half a texel apart
    // (the CPU at integer texel coordinates, the GPU at texel centres — 6.8 §4.4
    // shows both give the same offset DISTRIBUTION, not the same texel). On
    // this floor a cube texel spans up to ~2.6 screen pixels at grazing angles,
    // so 1.5 texels is about 4 pixels.
    int by_radius[7] = {0, 0, 0, 0, 0, 0, 0};   // [1..5] within r px, [6] beyond 5
    for (int j = 0; j < k_size; ++j)
    {
        for (int i = 0; i < k_size; ++i)
        {
            const std::size_t p = static_cast<std::size_t>(j) * k_size + static_cast<std::size_t>(i);
            double big = 0.0;
            const double rel = pixel_rel(gpu, cpu, p, &big);
            if (big < 1e-4 || rel <= tol) { continue; }
            int bin = 6;
            for (int radius = 1; radius <= 5; ++radius)
            {
                if (edge_at(i, j, radius)) { bin = radius; break; }
            }
            ++by_radius[bin];
        }
    }
    int edge_band = 0;
    for (int j = 0; j < k_size; ++j)
    {
        for (int i = 0; i < k_size; ++i) { edge_band += edge_at(i, j, 4) ? 1 : 0; }
    }
    std::printf("  with shadows: %d lit pixels, %d agree, %d differ; %d shadowed lamp-pixels\n",
                shaded.lit, shaded.agree, shaded.differ, shadowed);
    std::printf("  disagreeing pixels by distance to the nearest CPU shadow edge:\n");
    std::printf("    <=1 px %d   <=2 px %d   <=3 px %d   <=4 px %d   <=5 px %d   beyond %d\n",
                by_radius[1], by_radius[2], by_radius[3], by_radius[4], by_radius[5], by_radius[6]);
    const int beyond4 = by_radius[5] + by_radius[6];
    checkf(shadowed > 2000, "the box casts real shadows: %d lamp-pixels are occluded", shadowed);
    checkf(beyond4 == 0,
           "every one of the %d disagreeing pixels is within 4 px (~1.5 texels) of a shadow edge",
           shaded.differ);
    checkf(shaded.differ < edge_band / 4,
           "and they are a small part of that band: %d of %d pixels", shaded.differ, edge_band);

    // ---- 3. The control ---------------------------------------------------------
    std::vector<local_light> nudged = lamps;
    nudged[1].outer_angle *= 1.05f;
    const std::vector<float> gpu_nudged = render_frame(r, nudged, true);
    const compare_result ctl = compare(gpu_nudged, cpu, tol);
    checkf(ctl.differ > 1000,
           "control: widening the spot's cone by 5%% changes %d pixels — the instrument can see it",
           ctl.differ);
}

// ===========================================================================
//  §H — additive
// ===========================================================================

int channels_differing(const std::vector<float>& x, const std::vector<float>& y)
{
    if (x.empty() || x.size() != y.size()) { return -1; }
    int d = 0;
    for (std::size_t i = 0; i < x.size(); ++i) { d += (x[i] != y[i]) ? 1 : 0; }
    return d;
}

void section_h(rig& r)
{
    section("H  ADDITIVE — with no lamps, nothing that existed can move");

    const vec3 sun{1.2f, 1.1f, 1.0f};
    const std::vector<local_light> none;
    const std::vector<float> a = render_frame(r, none, false, sun);   // null locals
    const std::vector<float> b = render_frame(r, none, true, sun);    // a list of zero

    // One lamp whose range is 1 cm and which sits a kilometre away: the loop
    // RUNS, and every attenuation is exactly zero.
    local_light far;
    far.position = {1000.0f, 1000.0f, 1000.0f};
    far.range = 0.01f;
    const std::vector<float> c = render_frame(r, std::vector<local_light>{far}, true, sun);

    int lit = 0;
    for (float v : a) { lit += (v > 1e-4f) ? 1 : 0; }
    checkf(lit > 50000 && channels_differing(a, b) == 0,
           "null locals vs an empty list: %d of %zu channels differ (%d lit)",
           channels_differing(a, b), a.size(), lit);
    checkf(channels_differing(a, c) == 0,
           "a lamp that reaches nothing: %d of %zu channels differ — the loop added exact zeros",
           channels_differing(a, c), a.size());

    const std::vector<float> d = render_frame(r, std::vector<local_light>{scene_point()}, true, sun);
    checkf(channels_differing(a, d) > 1000, "control: a real lamp changes %d channels",
           channels_differing(a, d));

    std::printf("  (the CPU golden is §D's; the older harnesses — 6.11-6.18, 7.1-7.8 — are rerun\n"
                "   before and after by scratch/_base617b/run_all.sh, outside this file)\n");
}

// ===========================================================================
//  §I — the frame graph
// ===========================================================================

struct local_ctx
{
    const rig* r = nullptr;
    int job = 0;
    const engine::gpu_draw_item* items = nullptr;
    int count = 0;
};

void exec_local(const engine::fg_pass_context& c, void* user)
{
    const local_ctx& s = *static_cast<const local_ctx*>(user);
    s.r->shadows.render_into(c.cb, c.pass, s.job, s.items, s.count);
}

struct scene_ctx
{
    const rig* r = nullptr;
    const engine::gpu_draw_item* items = nullptr;
    engine::fg_texture spots{};
    engine::fg_texture points{};
    int count = 0;
};

void exec_scene(const engine::fg_pass_context& c, void* user)
{
    const scene_ctx& s = *static_cast<const scene_ctx*>(user);
    engine::gpu_scene_renderer::scene_local_lights locals;
    locals.lights = s.r->lights.handle();
    locals.count = s.count;
    // THE MAPS ARRIVE AS HANDLES — whatever version the pass declared it
    // samples. In the cached frame that is version 0: last frame's contents.
    locals.spot_shadows = c.texture(s.spots);
    locals.point_shadows = c.texture(s.points);
    locals.shadow_sampler = s.r->shadows.sampler();
    const engine::camera_uniforms camera{top_clip()};
    (void)s.r->scene.render(c.cb, c.pass, s.items, 1, camera, lamps_only_light(),
                            s.r->samp.handle(), nullptr, nullptr, nullptr, nullptr, nullptr,
                            &locals);
}

/// Declare and run one frame. `declare_shadows` false is the CACHED frame: the
/// maps are imported and sampled at version 0 — whatever the last frame left.
std::vector<float> graph_frame(rig& r, const std::vector<local_light>& lamps,
                               bool declare_shadows, int shadow_casters,
                               engine::frame_graph& fg, int* live = nullptr,
                               bool print_ops = false)
{
    std::vector<engine::gpu_local_light> records(lamps.size() + 1u);
    const int n = r.shadows.prepare(lamps, records);
    static engine::gpu_draw_item items[3];
    items[0] = ground_item(r);
    items[1] = box_item(r);
    items[2] = box_item(r, k_wall_centre, k_wall_size);

    fg.reset();
    engine::fg_texture_desc spot_desc{};
    spot_desc.width = static_cast<Uint32>(r.settings.spot_resolution);
    spot_desc.height = spot_desc.width;
    spot_desc.format = r.depth_format;
    spot_desc.layers = static_cast<Uint32>(r.shadows.max_spots());
    spot_desc.depth = true;
    engine::fg_texture_desc point_desc = spot_desc;
    point_desc.width = static_cast<Uint32>(r.settings.point_resolution);
    point_desc.height = point_desc.width;
    point_desc.layers = static_cast<Uint32>(6 * r.shadows.max_points());
    engine::fg_texture_desc image_desc{};
    image_desc.width = k_size;
    image_desc.height = k_size;
    image_desc.format = r.image.format;
    engine::fg_texture_desc depth_desc{};
    depth_desc.width = k_size;
    depth_desc.height = k_size;
    depth_desc.format = r.depth_format;
    depth_desc.depth = true;
    depth_desc.sampled = false;

    engine::fg_texture spots = fg.import("spot shadows", r.shadows.spot_texture(), spot_desc);
    engine::fg_texture points = fg.import("point shadows", r.shadows.point_texture(), point_desc);
    const engine::fg_texture image = fg.import("image", r.image.colour, image_desc);
    const engine::fg_texture depth = fg.create("scene depth", depth_desc);

    static local_ctx ctx[engine::k_max_fg_passes];
    static char names[engine::k_max_fg_passes][32];
    if (declare_shadows)
    {
        for (int j = 0; j < r.shadows.job_count(); ++j)
        {
            const engine::local_shadow_job& job = r.shadows.job(j);
            ctx[j] = local_ctx{&r, j, items, shadow_casters};
            if (job.kind == local_light_kind::point)
            {
                std::snprintf(names[j], sizeof(names[j]), "point %d face %s", job.light,
                              engine::name_of(static_cast<cube_face>(job.face)));
            }
            else
            {
                std::snprintf(names[j], sizeof(names[j]), "spot %d", job.light);
            }
            const int p = fg.add_pass(names[j], &exec_local, &ctx[j]);
            if (job.kind == local_light_kind::spot) { spots = fg.clear_depth(p, spots, 1.0f, job.layer); }
            else { points = fg.clear_depth(p, points, 1.0f, job.layer); }
        }
    }

    static scene_ctx sctx;
    sctx = scene_ctx{&r, items, spots, points, n};
    const int scene = fg.add_pass("scene", &exec_scene, &sctx);
    fg.sample(scene, spots);
    fg.sample(scene, points);
    (void)fg.clear(scene, image, SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f});
    (void)fg.clear_depth(scene, depth, 1.0f);

    if (!fg.compile(r.gpu)) { return {}; }
    if (live != nullptr) { *live = fg.live_pass_count(); }
    if (print_ops)
    {
        for (int s = 0; s < fg.live_pass_count(); ++s)
        {
            const int p = fg.scheduled(s);
            for (int a = 0; a < fg.access_count(p); ++a)
            {
                const engine::frame_graph::access_report rep = fg.access_at(p, a);
                if (rep.use == engine::fg_use::sample) { continue; }
                std::printf("    %2d %-18s %-14s layer %2u  %-9s %s\n", s, fg.pass_name(p),
                            fg.resource_name(rep.resource), rep.layer,
                            (rep.load_op == SDL_GPU_LOADOP_CLEAR)  ? "CLEAR"
                            : (rep.load_op == SDL_GPU_LOADOP_LOAD) ? "LOAD" : "DONT_CARE",
                            (rep.store_op == SDL_GPU_STOREOP_STORE) ? "STORE" : "DONT_CARE");
            }
        }
    }

    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
    if (cb == nullptr) { return {}; }
    if (n > 0
        && !r.lights.write(cb, records.data(),
                           static_cast<Uint32>(n) * static_cast<Uint32>(sizeof(records[0]))))
    {
        return {};
    }
    fg.execute(cb);
    return download(r.image, cb);
}

void section_i(rig& r)
{
    section("I  THE FRAME GRAPH — seven passes declared, then a frame that declares none");

    const std::vector<local_light> lamps{scene_point(), scene_spot()};
    const std::vector<float> by_hand = render_frame(r, lamps, true);

    engine::frame_graph fg;
    int live = 0;
    std::printf("  the derived ops (every write the graph schedules):\n");
    const std::vector<float> declared = graph_frame(r, lamps, true, 3, fg, &live, true);
    checkf(!declared.empty() && live == 8,
           "%d passes live: 6 cube faces, 1 spot, the scene", live);

    const int differing = channels_differing(by_hand, declared);
    checkf(differing == 0, "declared frame vs hand-recorded frame: %d of %zu channels differ",
           differing, declared.size());

    // ---- The cache ------------------------------------------------------------
    int live_cached = 0;
    const std::vector<float> cached = graph_frame(r, lamps, false, 3, fg, &live_cached);
    const int cache_diff = channels_differing(declared, cached);
    checkf(live_cached == 1 && cache_diff == 0,
           "a frame that declares NO shadow passes samples version 0 of the imports: %d live "
           "pass, %d channels differ", live_cached, cache_diff);

    // CONTROL: overwrite the maps with passes that clear and draw nothing, then
    // run the cached frame again. The shadows must be gone.
    (void)graph_frame(r, lamps, true, 0, fg);
    const std::vector<float> emptied = graph_frame(r, lamps, false, 3, fg);
    const int ctl = channels_differing(declared, emptied);
    checkf(ctl > 1000, "control: clear the maps, rerun the cached frame — %d channels change", ctl);
}

// ===========================================================================
//  §J — the budget and the cost
// ===========================================================================

void section_j(rig& r)
{
    section("J  THE BUDGET, AND WHAT A LAMP'S SHADOW COSTS");

    // Three shadowed points into a budget of two cubes.
    std::vector<local_light> three{scene_point(), scene_point(), scene_point()};
    three[1].position.x += 1.0f;
    three[2].position.z -= 1.0f;
    std::vector<engine::gpu_local_light> rec(3);
    const int n = r.shadows.prepare(three, rec);
    checkf(n == 3 && r.shadows.dropped() == 1 && rec[2].cone_shadow.y < 0.0f
               && r.shadows.job_count() == 12,
           "3 shadowed points, 2 cubes: %d dropped, its slot %.0f (drawn unshadowed), %d passes",
           r.shadows.dropped(), static_cast<double>(rec[2].cone_shadow.y), r.shadows.job_count());

    const int spot_texels = r.settings.spot_resolution * r.settings.spot_resolution;
    const int point_texels = 6 * r.settings.point_resolution * r.settings.point_resolution;
    std::printf("  texels per shadowed lamp: spot %d, point %d (%.2fx)\n", spot_texels,
                point_texels, static_cast<double>(point_texels) / spot_texels);
    std::printf("  GPU memory at 4 B/texel: spot array %d x %d^2 = %.2f MiB; cube array %d x 6 x %d^2 = %.2f MiB\n",
                r.shadows.max_spots(), r.settings.spot_resolution,
                r.shadows.max_spots() * spot_texels * 4.0 / (1024.0 * 1024.0),
                r.shadows.max_points(), r.settings.point_resolution,
                r.shadows.max_points() * point_texels * 4.0 / (1024.0 * 1024.0));
    checkf(point_texels == 393216 && spot_texels == 262144,
           "a 256 cube is 1.5x the texels of a 512 spot map, and six passes to one");

    // CPU cost of the maps: timing lines, meaningful only at -O2.
    const cpu_scene scene = make_cpu_scene(true);
    engine::local_shadow_set set;
    engine::shadow_stats spot_stats;
    engine::shadow_stats point_stats;
    for (int rep = 0; rep < 3; ++rep)
    {
        set.render(scene.objects, scene.pool, std::vector<local_light>{scene_spot()}, &spot_stats);
        set.render(scene.objects, scene.pool, std::vector<local_light>{scene_point()}, &point_stats);
    }
    std::printf("  TIMING  CPU shadow render: spot %.3f ms, point %.3f ms (last of 3)\n",
                spot_stats.render_ms, point_stats.render_ms);
}

} // namespace

// ===========================================================================
//  §K — THE SUN'S REACH: §G's finding, turned on the light that had it first
// ===========================================================================
//
// §G found that a GPU lookup through a LINEAR comparison sampler needs a bias
// sized for (r + 1)·√2 texels, because one tap blends a 2x2 block, where the
// CPU's nearest-texel lookup needs (r + ½)·√2. The lamps were built with the
// right one. The SUN's lookup (6.8, cascaded in 6.9) goes through the same kind
// of sampler and was fed `pcf_reach_texels` — the CPU's number.
//
// The instrument that cannot be argued with: a bare ground, nothing else in the
// map. Nothing can shadow a plane under a directional light, so every pixel of
// the shadowed frame that is darker than the unshadowed one is acne. Three sun
// elevations, because the error is the reach times tan(theta).

struct sun_rig
{
    engine::gpu_shadow_map map;
    engine::gpu_shader vs;
    engine::gpu_shader fs;
    static constexpr int k_res = 512;
};

/// One cascade that is the whole map — the block a caller with a single map
/// must push, since the shader reads the texel size and depth range from it.
engine::cascade_uniforms one_cascade(const engine::light_camera& cam)
{
    engine::cascade_uniforms c{};
    for (mat4& m : c.light_clip_from_world) { m = cam.clip_from_world; }
    c.splits = engine::vec4{1e30f, 1e30f, 1e30f, 1e30f};
    c.world_per_texel = engine::vec4{cam.world_per_texel, cam.world_per_texel,
                                     cam.world_per_texel, cam.world_per_texel};
    c.depth_range = engine::vec4{cam.depth_range, cam.depth_range, cam.depth_range,
                                 cam.depth_range};
    c.cascade_count = 1.0f;
    c.view_forward = engine::vec4{0.0f, -1.0f, 0.0f, 0.0f};
    return c;
}

std::vector<float> sun_frame(rig& r, sun_rig& s, const engine::light_camera& cam,
                             const engine::scene_light_uniforms& light,
                             const engine::cascade_uniforms* casc, bool with_box)
{
    const engine::gpu_draw_item items[2] = {ground_item(r), box_item(r)};
    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
    if (cb == nullptr) { return {}; }
    s.map.render(cb, items, with_box ? 2 : 1, cam);

    if (!r.scene.ensure_depth(r.gpu, k_size, k_size)) { return {}; }
    SDL_GPUColorTargetInfo ct{};
    ct.texture = r.image.colour;
    ct.load_op = SDL_GPU_LOADOP_CLEAR;
    ct.store_op = SDL_GPU_STOREOP_STORE;
    ct.clear_color = SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f};
    const SDL_GPUDepthStencilTargetInfo dsi = r.scene.depth_target_info();
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cb, &ct, 1, &dsi);
    if (pass == nullptr) { return {}; }
    const engine::camera_uniforms camera{top_clip()};
    (void)r.scene.render(cb, pass, items, 1, camera, light, r.samp.handle(), nullptr,
                         s.map.texture(), s.map.sampler(), casc);
    SDL_EndGPURenderPass(pass);
    return download(r.image, cb);
}

/// Pixels darker than the reference by more than a part in a thousand.
int darker(const std::vector<float>& a, const std::vector<float>& ref)
{
    int n = 0;
    for (std::size_t i = 0; i + 2 < a.size() && i + 2 < ref.size(); i += 4)
    {
        const float la = a[i] + a[i + 1] + a[i + 2];
        const float lr = ref[i] + ref[i + 1] + ref[i + 2];
        if (lr > 1e-4f && la < lr * 0.999f) { ++n; }
    }
    return n;
}

void section_k(rig& r)
{
    section("K  THE SUN'S REACH — the GPU shortfall §G found, in the sun's own lookup");

    sun_rig s;
    const bool ok = s.vs.load(r.gpu, "shadow.vert", engine::shader_stage::vertex)
                 && s.fs.load(r.gpu, "shadow.frag", engine::shader_stage::fragment)
                 && s.map.create(r.gpu, s.vs.handle(), s.fs.handle(), sun_rig::k_res,
                                 r.depth_format);
    if (!ok) { std::printf("  (sun shadow map did not create)\n"); return; }

    engine::aabb box;
    box.expand(vec3{-k_ground_half, -0.1f, -k_ground_half});
    box.expand(vec3{k_ground_half, 1.2f, k_ground_half});

    std::printf("  a bare ground under the sun, a 512 map, nothing to cast; pixels darker "
                "than the unshadowed frame, of %d:\n", k_size * k_size);
    std::printf("  %4s %8s %8s %16s %16s %12s\n", "pcf", "theta", "tan", "(r + 1/2) sqrt2",
                "(r + 1) sqrt2", "as filled");

    // Four sun elevations, both kernels. The arms override `shadow_reach` and
    // nothing else; "as filled" is whatever `fill_uniforms` chose.
    int worst_old_one_tap = 1 << 30;
    int worst_new = 0;
    int worst_filled = 0;
    int worst_old_nine_tap = 0;
    bool filled_is_gpu_reach = true;
    engine::shadow_settings set;   // the defaults: slope-scaled, 3x3 PCF
    for (const int radius : {0, 1})
    {
        for (const float deg : {20.0f, 45.0f, 63.4f, 75.0f})
        {
            set.pcf_radius = radius;
            const float th = deg * 0.017453293f;
            const vec3 to_sun = engine::normalised(vec3{std::sin(th) * 0.866f, std::cos(th),
                                                        std::sin(th) * 0.5f});
            engine::directional_light sun;
            sun.direction = -to_sun;
            const engine::light_camera cam = engine::fit_directional(sun, box, sun_rig::k_res);

            engine::scene_light_uniforms light = lamps_only_light(vec3{3.0f, 3.0f, 3.0f});
            light.to_light = to_sun;
            engine::gpu_shadow_map::fill_uniforms(light, cam, set, sun_rig::k_res);
            filled_is_gpu_reach = filled_is_gpu_reach
                && light.shadow_reach == engine::gpu_pcf_reach_texels(radius);
            const engine::cascade_uniforms casc = one_cascade(cam);

            engine::scene_light_uniforms off = light;
            off.shadow_strength = 0.0f;
            const std::vector<float> ref = sun_frame(r, s, cam, off, &casc, false);

            engine::scene_light_uniforms a = light;
            a.shadow_reach = engine::pcf_reach_texels(radius);
            const int n_old = darker(sun_frame(r, s, cam, a, &casc, false), ref);
            engine::scene_light_uniforms b = light;
            b.shadow_reach = (static_cast<float>(radius) + 1.0f) * 1.41421356f;
            const int n_new = darker(sun_frame(r, s, cam, b, &casc, false), ref);
            const int n_filled = darker(sun_frame(r, s, cam, light, &casc, false), ref);
            std::printf("  %4d %7.1f° %8.3f %16d %16d %12d\n", radius, static_cast<double>(deg),
                        static_cast<double>(std::tan(th)), n_old, n_new, n_filled);

            if (radius == 0) { worst_old_one_tap = std::min(worst_old_one_tap, n_old); }
            else { worst_old_nine_tap = std::max(worst_old_nine_tap, n_old); }
            worst_new = std::max(worst_new, n_new);
            worst_filled = std::max(worst_filled, n_filled);
        }
    }

    checkf(worst_old_one_tap > k_size * k_size / 10,
           "the CPU's reach through the GPU's sampler, one tap: at least %d of %d pixels "
           "of a bare ground are acne at every elevation — the shortfall is real",
           worst_old_one_tap, k_size * k_size);
    checkf(worst_new == 0,
           "(r + 1) sqrt2, the reach of a 2x2 comparison: 0 acne at every elevation "
           "and both kernels (worst %d)", worst_new);
    checkf(worst_old_nine_tap == 0,
           "and at 3x3 PCF, the default, the old reach showed nothing either (%d) — "
           "which is how it survived from 6.8 to here", worst_old_nine_tap);
    checkf(filled_is_gpu_reach && worst_filled == 0,
           "fill_uniforms now writes gpu_pcf_reach_texels: 0 acne as filled (worst %d)",
           worst_filled);

    s.map.destroy();
    s.vs.destroy();
    s.fs.destroy();
}

// ===========================================================================
//  §L — ONE SUN MAP AND NO CASCADES (the second fix after 6.17b)
// ===========================================================================
//
// Since 6.9 the sun's lookup reads its texel size and depth range from the
// CASCADE block. A caller with one map and no cascades — `gpu_shadow_map` +
// `fill_uniforms` + `render(..., shadow, sampler)`, which is how demos/sandbox
// draws — gets whatever `gpu_scene_renderer` pushes in their place. §K's rig
// found that the box's shadow vanished there. The test: the same frame, with a
// one-cascade block and without one, must be the same picture.

int differing_channels(const std::vector<float>& a, const std::vector<float>& b)
{
    int n = 0;
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i)
    {
        if (std::fabs(a[i] - b[i]) > 1e-6f) { ++n; }
    }
    return n + static_cast<int>(a.size() > b.size() ? a.size() - b.size() : b.size() - a.size());
}

void section_l(rig& r)
{
    section("L  ONE SUN MAP, NO CASCADES — what gpu_scene pushes in their place");

    sun_rig s;
    const bool ok = s.vs.load(r.gpu, "shadow.vert", engine::shader_stage::vertex)
                 && s.fs.load(r.gpu, "shadow.frag", engine::shader_stage::fragment)
                 && s.map.create(r.gpu, s.vs.handle(), s.fs.handle(), sun_rig::k_res,
                                 r.depth_format);
    if (!ok) { std::printf("  (sun shadow map did not create)\n"); return; }

    engine::aabb box;
    box.expand(vec3{-k_ground_half, -0.1f, -k_ground_half});
    box.expand(vec3{k_ground_half, 1.2f, k_ground_half});

    std::printf("  the box's shadow on the ground, 512 map, default settings:\n");
    std::printf("  %8s %18s %18s %16s\n", "theta", "one-cascade block", "no cascade block",
                "channels differ");
    int least_with = 1 << 30;
    int worst_gap = 0;
    int worst_diff = 0;
    for (const float deg : {20.0f, 45.0f, 63.4f})
    {
        const float th = deg * 0.017453293f;
        const vec3 to_sun = engine::normalised(vec3{std::sin(th) * 0.866f, std::cos(th),
                                                    std::sin(th) * 0.5f});
        engine::directional_light sun;
        sun.direction = -to_sun;
        const engine::light_camera cam = engine::fit_directional(sun, box, sun_rig::k_res);
        engine::scene_light_uniforms light = lamps_only_light(vec3{3.0f, 3.0f, 3.0f});
        light.to_light = to_sun;
        engine::gpu_shadow_map::fill_uniforms(light, cam, engine::shadow_settings{},
                                              sun_rig::k_res);
        const engine::cascade_uniforms casc = one_cascade(cam);

        engine::scene_light_uniforms off = light;
        off.shadow_strength = 0.0f;
        const std::vector<float> ref = sun_frame(r, s, cam, off, &casc, true);
        const std::vector<float> with_block = sun_frame(r, s, cam, light, &casc, true);
        const std::vector<float> without = sun_frame(r, s, cam, light, nullptr, true);
        const int n_with = darker(with_block, ref);
        const int n_without = darker(without, ref);
        const int diff = differing_channels(with_block, without);
        std::printf("  %7.1f° %18d %18d %16d\n", static_cast<double>(deg), n_with, n_without, diff);
        least_with = std::min(least_with, n_with);
        worst_gap = std::max(worst_gap, std::abs(n_with - n_without));
        worst_diff = std::max(worst_diff, diff);
    }

    checkf(least_with > 300,
           "with a one-cascade block the box casts a shadow at every elevation (at least %d px) "
           "— the control: this rig can see a shadow", least_with);
    checkf(worst_gap == 0 && worst_diff == 0,
           "with NO cascade block, the call demos/sandbox makes, the picture is the same: "
           "shadow counts differ by %d px, %d channels differ", worst_gap, worst_diff);

    s.map.destroy();
    s.vs.destroy();
    s.fs.destroy();
}

int main()
{
    std::printf("verify_617b — Lesson 6.17b: local lights, point and spot, and their shadows\n");

    section_a();
    section_b();
    section_c();
    section_d();
    section_e();
    section_f();

    rig r;
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::printf("\n  ---- SDL_Init(VIDEO): %s\n", SDL_GetError());
    }
    if (!build_rig(r))
    {
        std::printf("\n  (no GPU device — §G to §L skipped)\n");
    }
    else
    {
        section_g(r);
        section_h(r);
        section_i(r);
        section_j(r);
        section_k(r);
        section_l(r);
        r.image.destroy();
    }
    SDL_Quit();

    std::printf("\n===========================================================================\n");
    std::printf("  %d checks, %d failure(s)\n", g_checks, g_failures);
    std::printf("===========================================================================\n");
    return g_failures == 0 ? 0 : 1;
}
