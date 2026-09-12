// engine/src/gfx/cubemap.cpp — Lesson 6.15.
//
// The file is in four parts, and they get progressively less exact: the face
// geometry (exact), the irradiance convolution (exact for Lambert, up to
// discretisation), the prefilter (two stacked approximations), and the BRDF
// table (exact given the same importance sampler, so its only error is
// convergence). The header argues each of those claims; this file implements
// them and comments only where the code is not the obvious transcription.

#include <engine/gfx/cubemap.hpp>

#include <engine/core/assert.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace engine {
namespace {

constexpr float k_pi = std::numbers::pi_v<float>;

/// The face table from the header, as data.
///
/// Each row names which COMPONENT of the direction supplies `sc`, `tc` and `ma`,
/// and the sign each carries. Written as indices rather than as a switch over six
/// cases because the two directions — direction-to-face and face-to-direction —
/// are then provably inverse: one reads the table forwards and the other reads it
/// backwards, so a typo cannot appear in only one of them.
struct face_rule
{
    int sc_index, tc_index, ma_index;
    float sc_sign, tc_sign;
};

constexpr face_rule k_rules[k_cube_faces] = {
    { 2, 1, 0, -1.0f, -1.0f },   // +X : sc = -z, tc = -y
    { 2, 1, 0, +1.0f, -1.0f },   // -X : sc = +z, tc = -y
    { 0, 2, 1, +1.0f, +1.0f },   // +Y : sc = +x, tc = +z
    { 0, 2, 1, +1.0f, -1.0f },   // -Y : sc = +x, tc = -z
    { 0, 1, 2, +1.0f, -1.0f },   // +Z : sc = +x, tc = -y
    { 0, 1, 2, -1.0f, -1.0f },   // -Z : sc = -x, tc = -y
};

[[nodiscard]] constexpr float component(vec3 v, int i)
{
    return (i == 0) ? v.x : ((i == 1) ? v.y : v.z);
}

/// Lambert's formula for the solid angle of an axis-aligned rectangle on the
/// plane `z = 1`, measured from the origin, with one corner at the origin's
/// projection. The full rectangle is the usual four-corner difference.
///
/// In DOUBLE, deliberately. The four terms nearly cancel for a small texel — at
/// 128x128 each is around 1e-4 and their difference around 2e-4 — and doing that
/// subtraction in float costs about four significant digits on a quantity every
/// integral in this file then multiplies by. The result is returned as a float
/// because that is what the callers want; the arithmetic is not.
[[nodiscard]] double solid_angle_corner(double x, double y)
{
    return std::atan2(x * y, std::sqrt(x * x + y * y + 1.0));
}

/// A tangent frame around `n`. Any frame will do — the integrals below are
/// isotropic about the normal, so only the fact that it is orthonormal matters.
void build_frame(vec3 n, vec3& t, vec3& b)
{
    // The guard is the standard one and it is not optional: `cross(up, n)` is the
    // zero vector when they are parallel, and normalising that gives NaN for
    // exactly the normals a ground plane and a ceiling have — which is to say,
    // the two most common normals in any scene.
    const vec3 up = (std::fabs(n.y) < 0.999f) ? vec3{0.0f, 1.0f, 0.0f}
                                              : vec3{1.0f, 0.0f, 0.0f};
    t = normalised(cross(up, n));
    b = cross(n, t);
}

} // namespace

// ---------------------------------------------------------------------------
// Faces
// ---------------------------------------------------------------------------

const char* name_of(cube_face f)
{
    switch (f)
    {
        case cube_face::pos_x: return "+X";
        case cube_face::neg_x: return "-X";
        case cube_face::pos_y: return "+Y";
        case cube_face::neg_y: return "-Y";
        case cube_face::pos_z: return "+Z";
        case cube_face::neg_z: return "-Z";
    }
    return "?";
}

cube_texel direction_to_cube(vec3 d)
{
    const float ax = std::fabs(d.x);
    const float ay = std::fabs(d.y);
    const float az = std::fabs(d.z);

    // THE LARGEST COMPONENT PICKS THE FACE, and the tie-breaks matter more than
    // they look. `>=` rather than `>` means a direction exactly on an edge — the
    // 45-degree diagonal, which is not a rare input when you are walking a grid —
    // resolves to a definite face instead of falling through to the last branch.
    // Either face is correct there (the uv comes out on the shared edge), but
    // *some* face has to be chosen deterministically or the round trip stops
    // being a round trip.
    int index;
    if (ax >= ay && ax >= az)      { index = (d.x > 0.0f) ? 0 : 1; }
    else if (ay >= az)             { index = (d.y > 0.0f) ? 2 : 3; }
    else                           { index = (d.z > 0.0f) ? 4 : 5; }

    const face_rule& r = k_rules[index];
    const float ma = std::fabs(component(d, r.ma_index));

    // A zero-length direction has no face. Returning the +X face's centre is a
    // defined answer for an undefined question, which is what the engine core
    // does everywhere in place of an exception (conventions §4).
    if (ma <= 0.0f) { return {cube_face::pos_x, 0.5f, 0.5f}; }

    const float sc = r.sc_sign * component(d, r.sc_index);
    const float tc = r.tc_sign * component(d, r.tc_index);

    return {static_cast<cube_face>(index),
            0.5f * (sc / ma + 1.0f),
            0.5f * (tc / ma + 1.0f)};
}

vec3 cube_to_direction(cube_face f, float u, float v)
{
    const int index = static_cast<int>(f);
    ENGINE_ASSERT(index >= 0 && index < k_cube_faces);
    const face_rule& r = k_rules[index];

    const float sc = 2.0f * u - 1.0f;
    const float tc = 2.0f * v - 1.0f;

    // The major axis is +1 on even face indices and -1 on odd ones, which is
    // exactly what SDL's enum ordering (+X, -X, +Y, -Y, +Z, -Z) buys us: the
    // sign is the low bit. That is a small thing to lean on, so it is asserted
    // rather than assumed — `verify_615` §A checks the enum against SDL's.
    float c[3] = {0.0f, 0.0f, 0.0f};
    c[r.ma_index] = (index & 1) ? -1.0f : 1.0f;
    c[r.sc_index] = r.sc_sign * sc;
    c[r.tc_index] = r.tc_sign * tc;

    return normalised(vec3{c[0], c[1], c[2]});
}

float cube_texel_solid_angle(int size, int i, int j)
{
    if (size <= 0) { return 0.0f; }

    const double step = 2.0 / size;
    const double x0 = -1.0 + i * step;
    const double y0 = -1.0 + j * step;
    const double x1 = x0 + step;
    const double y1 = y0 + step;

    const double w = solid_angle_corner(x1, y1) - solid_angle_corner(x0, y1)
                   - solid_angle_corner(x1, y0) + solid_angle_corner(x0, y0);
    return static_cast<float>(w);
}

vec2 hammersley(int i, int n)
{
    if (n <= 0) { return {0.0f, 0.0f}; }

    // THE RADICAL INVERSE, by bit reversal. Five shift-and-mask steps reverse a
    // 32-bit word: the first swaps the two halves, the second swaps the two
    // quarters within each half, and so on down to adjacent bits. Reversing a
    // binary fraction about the point is what puts sample 1 at 0.5, sample 2 at
    // 0.25, sample 3 at 0.75 — each new point in the largest remaining gap.
    auto bits = static_cast<std::uint32_t>(i);
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

    // 2^-32, written as a literal rather than as `1.0 / 4294967296.0` so that it
    // is exactly representable and the compiler cannot be tempted to do anything
    // clever with the division.
    const float y = static_cast<float>(bits) * 2.3283064365386963e-10f;
    return {(static_cast<float>(i) + 0.5f) / static_cast<float>(n), y};
}

// ---------------------------------------------------------------------------
// cube_map
// ---------------------------------------------------------------------------

cube_map::cube_map(int size, int levels)
{
    if (size <= 0 || levels <= 0) { return; }

    size_ = size;
    levels_ = levels;
    faces_.reserve(static_cast<std::size_t>(k_cube_faces) * levels);

    // LEVEL-MAJOR, so that the six faces of one level are contiguous. Every loop
    // in this file walks a level at a time (a prefilter pass, an upload, a
    // report), and none walks a face through its levels.
    for (int level = 0; level < levels; ++level)
    {
        const int n = size_at(level);
        for (int f = 0; f < k_cube_faces; ++f) { faces_.emplace_back(n, n); }
    }
}

int cube_map::size_at(int level) const
{
    const int n = size_ >> std::clamp(level, 0, 30);
    return std::max(1, n);
}

hdr_buffer& cube_map::face(cube_face f, int level)
{
    const int l = std::clamp(level, 0, levels_ - 1);
    return faces_[static_cast<std::size_t>(l) * k_cube_faces + static_cast<int>(f)];
}

const hdr_buffer& cube_map::face(cube_face f, int level) const
{
    const int l = std::clamp(level, 0, levels_ - 1);
    return faces_[static_cast<std::size_t>(l) * k_cube_faces + static_cast<int>(f)];
}

std::size_t cube_map::texel_count() const
{
    std::size_t total = 0;
    for (int level = 0; level < levels_; ++level)
    {
        const auto n = static_cast<std::size_t>(size_at(level));
        total += n * n * k_cube_faces;
    }
    return total;
}

linear_rgb cube_map::sample(vec3 d, int level) const
{
    if (!valid()) { return {}; }

    const int l = std::clamp(level, 0, levels_ - 1);
    const cube_texel t = direction_to_cube(d);
    const hdr_buffer& img = face(t.face, l);
    const int n = img.width();

    // Texel centres sit at (i + 0.5)/n, so the continuous coordinate of the
    // texel *grid* is uv*n - 0.5 — the same half-texel shift Lesson 3.9 derived
    // for bilinear sampling, and the same one whose absence shifts an image by
    // half a texel and makes a checkerboard look subtly wrong.
    const float fx = t.u * n - 0.5f;
    const float fy = t.v * n - 0.5f;

    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const float tx = fx - x0;
    const float ty = fy - y0;

    // CLAMP, NOT WRAP, and not a jump to the neighbouring face either. The header
    // documents this limit; the clamp is what makes it a seam rather than a
    // wrap-around to the far side of the sky.
    auto at = [&](int x, int y) {
        return img.pixel_at(std::clamp(x, 0, n - 1), std::clamp(y, 0, n - 1));
    };

    const linear_rgb c00 = at(x0, y0), c10 = at(x0 + 1, y0);
    const linear_rgb c01 = at(x0, y0 + 1), c11 = at(x0 + 1, y0 + 1);

    const float w00 = (1.0f - tx) * (1.0f - ty);
    const float w10 = tx * (1.0f - ty);
    const float w01 = (1.0f - tx) * ty;
    const float w11 = tx * ty;

    return {c00.r * w00 + c10.r * w10 + c01.r * w01 + c11.r * w11,
            c00.g * w00 + c10.g * w10 + c01.g * w01 + c11.g * w11,
            c00.b * w00 + c10.b * w10 + c01.b * w01 + c11.b * w11};
}

linear_rgb cube_map::sample_level(vec3 d, float level) const
{
    if (!valid()) { return {}; }

    const float clamped = std::clamp(level, 0.0f, static_cast<float>(levels_ - 1));
    const int lo = static_cast<int>(std::floor(clamped));
    const int hi = std::min(lo + 1, levels_ - 1);
    const float t = clamped - lo;

    if (lo == hi || t <= 0.0f) { return sample(d, lo); }

    const linear_rgb a = sample(d, lo);
    const linear_rgb b = sample(d, hi);
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

// ---------------------------------------------------------------------------
// Environments
// ---------------------------------------------------------------------------

cube_map make_uniform_environment(int size, linear_rgb radiance)
{
    cube_map env(size, 1);
    if (!env.valid()) { return env; }

    for (int f = 0; f < k_cube_faces; ++f)
    {
        env.face(static_cast<cube_face>(f), 0).clear(radiance);
    }
    return env;
}

linear_rgb sky_radiance(const sky_settings& s, vec3 d)
{
    const vec3 dir = normalised(d);

    if (dir.y < 0.0f)
    {
        // THE GROUND FADES TOWARD THE HORIZON, linearly in the elevation's sine
        // just as the sky does — one expression, two hemispheres, and no
        // trigonometry in either. `t = 0` at the horizon and 1 straight down.
        const float t = std::clamp(-dir.y, 0.0f, 1.0f);
        return {s.ground_horizon.r + (s.ground.r - s.ground_horizon.r) * t,
                s.ground_horizon.g + (s.ground.g - s.ground_horizon.g) * t,
                s.ground_horizon.b + (s.ground.b - s.ground_horizon.b) * t};
    }

    // A LINEAR blend from horizon to zenith in the elevation's SINE, which is
    // `dir.y` and needs no trigonometry. It is not a physical sky — §11 names two
    // that are — but it has the property that matters for integrating: it is
    // smooth, so the convolution below has something continuous to work on and
    // the discretisation error is the only error.
    const float t = std::clamp(dir.y, 0.0f, 1.0f);
    linear_rgb c{s.horizon.r + (s.zenith.r - s.horizon.r) * t,
                 s.horizon.g + (s.zenith.g - s.horizon.g) * t,
                 s.horizon.b + (s.zenith.b - s.horizon.b) * t};

    // THE DISC, AS A HARD EDGE. A real sun has a limb-darkened profile and an
    // atmosphere smearing it; this one is a step function, which is the harder
    // case for everything downstream and therefore the more useful one to test
    // with. A step in the environment is what makes the prefilter's sample count
    // visible: too few samples and the blurred sun becomes a ring of dots.
    const vec3 sun = normalised(s.sun_direction);
    const float cos_limit = std::cos(s.sun_angular_radius_deg * k_pi / 180.0f);
    if (dot(dir, sun) >= cos_limit)
    {
        c.r += s.sun_radiance.r;
        c.g += s.sun_radiance.g;
        c.b += s.sun_radiance.b;
    }
    return c;
}

cube_map make_sky_environment(int size, const sky_settings& s)
{
    cube_map env(size, 1);
    if (!env.valid()) { return env; }

    for (int f = 0; f < k_cube_faces; ++f)
    {
        hdr_buffer& img = env.face(static_cast<cube_face>(f), 0);
        for (int y = 0; y < size; ++y)
        {
            linear_rgb* row = img.row(y);
            for (int x = 0; x < size; ++x)
            {
                // EVALUATED AT THE TEXEL CENTRE, through `cube_to_direction` —
                // which is what makes the handedness question the header raises
                // a non-issue for this generator. The face is filled with
                // whatever direction the lookup will later produce for it, so
                // the two agree by construction rather than by agreement.
                const vec3 d = cube_to_direction(static_cast<cube_face>(f),
                                                 (x + 0.5f) / size,
                                                 (y + 0.5f) / size);
                row[x] = sky_radiance(s, d);
            }
        }
    }
    return env;
}

// ---------------------------------------------------------------------------
// Integral 1 — irradiance
// ---------------------------------------------------------------------------

cube_map irradiance_map(const cube_map& env, int out_size)
{
    cube_map out(out_size, 1);
    if (!out.valid() || !env.valid()) { return out; }

    const int in_size = env.size();

    // PRECOMPUTE THE INPUT ONCE. Every output texel integrates over every input
    // texel, so anything that depends only on the input — its direction and its
    // solid angle — is hoisted out of the inner loop. At 32 out and 128 in this
    // turns 6.0e8 calls to `cube_to_direction` into 98,304 of them, and the
    // convolution goes from minutes to a couple of seconds.
    struct sample_point { vec3 dir; linear_rgb radiance; float weight; };
    std::vector<sample_point> points;
    points.reserve(static_cast<std::size_t>(k_cube_faces) * in_size * in_size);

    for (int f = 0; f < k_cube_faces; ++f)
    {
        const hdr_buffer& img = env.face(static_cast<cube_face>(f), 0);
        for (int y = 0; y < in_size; ++y)
        {
            const linear_rgb* row = img.row(y);
            for (int x = 0; x < in_size; ++x)
            {
                points.push_back({cube_to_direction(static_cast<cube_face>(f),
                                                    (x + 0.5f) / in_size,
                                                    (y + 0.5f) / in_size),
                                  row[x],
                                  // THE WEIGHT THE WHOLE FILE EXISTS FOR. Drop
                                  // it and the corners count 5.196x too much.
                                  cube_texel_solid_angle(in_size, x, y)});
            }
        }
    }

    for (int f = 0; f < k_cube_faces; ++f)
    {
        hdr_buffer& img = out.face(static_cast<cube_face>(f), 0);
        for (int y = 0; y < out_size; ++y)
        {
            linear_rgb* row = img.row(y);
            for (int x = 0; x < out_size; ++x)
            {
                const vec3 n = cube_to_direction(static_cast<cube_face>(f),
                                                 (x + 0.5f) / out_size,
                                                 (y + 0.5f) / out_size);

                // Accumulate in DOUBLE. A sun disc contributes single samples
                // around 6,000 into a sum whose other terms are around 1e-4, and
                // float addition loses the small ones entirely once the running
                // total is large enough — the classic catastrophic-accumulation
                // failure, which here would show as an irradiance map that is
                // correct near the sun and quietly too dark away from it.
                double r = 0.0, g = 0.0, b = 0.0;
                for (const sample_point& p : points)
                {
                    const float cosine = dot(n, p.dir);
                    if (cosine <= 0.0f) { continue; }
                    const double w = static_cast<double>(cosine) * p.weight;
                    r += p.radiance.r * w;
                    g += p.radiance.g * w;
                    b += p.radiance.b * w;
                }
                row[x] = {static_cast<float>(r), static_cast<float>(g),
                          static_cast<float>(b)};
            }
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Integral 2 — the split sum
// ---------------------------------------------------------------------------

float roughness_for_level(int level, int levels)
{
    if (levels <= 1) { return 0.0f; }
    return std::clamp(static_cast<float>(level) / static_cast<float>(levels - 1),
                      0.0f, 1.0f);
}

float prefilter_level_for(float roughness, int base_size, int levels)
{
    if (levels <= 1 || base_size <= 0) { return 0.0f; }

    const float alpha = alpha_from_roughness(roughness);
    const float lobe = ggx_lobe_solid_angle(alpha);

    // The level-0 texel to compare against is the CENTRE one, which is the
    // largest a level-0 texel gets. Using the corner instead would shift every
    // answer by log4(5.196) = 1.19 levels, which is more than the whole
    // disagreement with the linear mapping — so the choice has to be stated. The
    // centre is right because the lobe is being compared against the resolution
    // the map achieves where it is best, and a level that cannot resolve the
    // lobe even at the face centre certainly cannot at the corner.
    const float w0 = cube_texel_solid_angle(base_size, base_size / 2, base_size / 2);
    if (w0 <= 0.0f || lobe <= 0.0f) { return 0.0f; }

    // Each level quadruples a texel's solid angle, so the level whose texel
    // matches the lobe is log base 4 of their ratio — and log4(x) is
    // 0.5 * log2(x).
    const float k = 0.5f * std::log2(lobe / w0);
    return std::clamp(k, 0.0f, static_cast<float>(levels - 1));
}

cube_map prefilter_environment(const cube_map& env, int levels, int samples)
{
    cube_map out(env.size(), levels);
    if (!out.valid() || !env.valid()) { return out; }

    for (int level = 0; level < levels; ++level)
    {
        const int n = out.size_at(level);
        const float roughness = roughness_for_level(level, levels);
        const float alpha = alpha_from_roughness(roughness);

        for (int f = 0; f < k_cube_faces; ++f)
        {
            hdr_buffer& img = out.face(static_cast<cube_face>(f), level);
            for (int y = 0; y < n; ++y)
            {
                linear_rgb* row = img.row(y);
                for (int x = 0; x < n; ++x)
                {
                    // THE ASSUMPTION, IN ONE LINE: the texel's direction serves
                    // as the normal, the view and the reflection all at once.
                    // Everything the prefilter cannot do follows from this line.
                    const vec3 r = cube_to_direction(static_cast<cube_face>(f),
                                                     (x + 0.5f) / n,
                                                     (y + 0.5f) / n);

                    if (level == 0)
                    {
                        // ROUGHNESS 0 IS A MIRROR, so level 0 is a copy and not
                        // a one-sample integral. Running the sampler here would
                        // be harmless in principle (`alpha` is clamped to
                        // k_min_alpha, so every sample lands on `r`) and wasteful
                        // in practice — and it would blur the sun disc by one
                        // bilinear tap for no reason.
                        row[x] = env.sample(r, 0);
                        continue;
                    }

                    vec3 t, b;
                    build_frame(r, t, b);

                    double sr = 0.0, sg = 0.0, sb = 0.0, sw = 0.0;
                    for (int s = 0; s < samples; ++s)
                    {
                        const vec2 xi = hammersley(s, samples);
                        const vec3 h_t = ggx_importance_sample(xi.x, xi.y, alpha);
                        const vec3 h = t * h_t.x + b * h_t.y + r * h_t.z;

                        // `reflect(v, n)` mirrors `v` about the plane with normal
                        // `n`, so reflecting -r about h gives the light direction
                        // that a microfacet facing h would send toward r.
                        const vec3 l = reflect(-r, h);

                        const float n_dot_l = dot(r, l);
                        if (n_dot_l <= 0.0f) { continue; }

                        // WEIGHTED BY n.l, WHICH IS KARIS'S CHOICE AND NOT A
                        // DERIVATION. Importance sampling GGX already accounts
                        // for D and the Jacobian, so the estimator's remaining
                        // weight ought to include G and the Fresnel — but those
                        // belong to the OTHER bracket of the split sum, and
                        // putting them here would double-count. `n.l` is what is
                        // left, and it is documented in the original 2013 course
                        // notes as producing a visibly better result than
                        // uniform weighting rather than as a consequence of
                        // anything. It is the second fitted step in this lesson,
                        // and §5.4 marks it as such.
                        const linear_rgb c = env.sample(l, 0);
                        sr += static_cast<double>(c.r) * n_dot_l;
                        sg += static_cast<double>(c.g) * n_dot_l;
                        sb += static_cast<double>(c.b) * n_dot_l;
                        sw += n_dot_l;
                    }

                    if (sw > 0.0)
                    {
                        row[x] = {static_cast<float>(sr / sw),
                                  static_cast<float>(sg / sw),
                                  static_cast<float>(sb / sw)};
                    }
                    else
                    {
                        // Every sample landed below the horizon, which can happen
                        // at high roughness when the frame is unlucky. Falling
                        // back to the unfiltered value is better than black: it
                        // is wrong by the blur, not by the whole radiance.
                        row[x] = env.sample(r, 0);
                    }
                }
            }
        }
    }
    return out;
}

brdf_terms integrate_brdf(float n_dot_v, float roughness, int samples)
{
    const float ndv = std::clamp(n_dot_v, 1.0e-4f, 1.0f);
    const float alpha = alpha_from_roughness(roughness);

    // The view vector in tangent space, with the normal along +Z. Any azimuth
    // will do — the integrand is isotropic about the normal — so it is placed in
    // the x-z plane, which makes the dot products below one term shorter.
    const vec3 v{std::sqrt(1.0f - ndv * ndv), 0.0f, ndv};

    // The normal is (0, 0, 1) and is therefore never written down: every `n.x`
    // below is just the vector's own z component. That is the one real economy
    // of working in tangent space, and it is worth naming rather than leaving
    // the reader to wonder where the normal went.

    double scale = 0.0, bias = 0.0;

    for (int s = 0; s < samples; ++s)
    {
        const vec2 xi = hammersley(s, samples);
        const vec3 h = ggx_importance_sample(xi.x, xi.y, alpha);
        const vec3 l = reflect(-v, h);

        const float n_dot_l = l.z;
        if (n_dot_l <= 0.0f) { continue; }

        const float n_dot_h = std::max(0.0f, h.z);
        const float v_dot_h = std::max(0.0f, dot(v, h));

        // THE ESTIMATOR, AND WHY D IS ABSENT. Sampling from D's own distribution
        // means the D in the numerator and the D in the pdf cancel exactly; what
        // survives is the geometry term, the Jacobian of the half-vector-to-light
        // change of variables, and the cosines. The algebra is in §5.5 and its
        // punchline is this single expression:
        //
        //     weight = G * (v.h) / ((n.h) * (n.v))
        //
        // If you have ever wondered why the reference implementations of this
        // function contain no NDF at all, that is the answer.
        const float g = smith_g(n_dot_l, ndv, alpha);
        const float weight = g * v_dot_h / (std::max(1.0e-6f, n_dot_h) * ndv);

        // SCHLICK'S LINEARITY IN F0 IS WHAT MAKES THE TABLE TWO-DIMENSIONAL.
        //     F = F0 + (1 - F0) * fc,   fc = (1 - v.h)^5
        //       = F0 * (1 - fc) + fc
        // so the integral is F0 * integral(weight * (1 - fc)) + integral(weight * fc),
        // and neither integral mentions F0. One table, every material.
        const float fc = std::pow(1.0f - v_dot_h, 5.0f);
        scale += static_cast<double>(weight) * (1.0f - fc);
        bias += static_cast<double>(weight) * fc;
    }

    return {static_cast<float>(scale / samples), static_cast<float>(bias / samples)};
}

texture make_brdf_lut(int size, int samples)
{
    // `texel_space::linear` — see the header. These are integral coefficients,
    // not colours, and an sRGB decode on read would corrupt every one of them.
    texture lut(size, size, 0xFF000000u, texel_space::linear);
    if (size <= 0) { return lut; }

    for (int y = 0; y < size; ++y)
    {
        const float roughness = (y + 0.5f) / size;
        for (int x = 0; x < size; ++x)
        {
            const float ndv = (x + 0.5f) / size;
            const brdf_terms t = integrate_brdf(ndv, roughness, samples);

            // 8 BITS PER CHANNEL, AND THIS IS THE ONE PLACE IN THE FILE WHERE
            // THE STORAGE IS A REAL COMPROMISE. `scale` and `bias` are both in
            // [0,1], so a byte quantises them to 1/255 = 0.39%, which lands on
            // the specular term as a multiplicative error of the same size.
            // Shipping engines store this table as RG16F for that reason, and
            // this engine has no 16-bit-per-channel CPU texture type — Lesson
            // 6.12's `hdr_buffer` is float and has no alpha, `texture` is RGBA8.
            // `verify_615` §H measures what the quantisation costs against the
            // un-quantised `integrate_brdf`, so the number is known rather than
            // assumed, and the exercise at the end of the lesson is to fix it.
            const auto to_byte = [](float f) {
                return static_cast<Uint8>(std::clamp(f, 0.0f, 1.0f) * 255.0f + 0.5f);
            };

            // `pack_argb` AND NOT A HAND-ROLLED SHIFT, and this cost an hour.
            // The first version of this line wrote `(bias << 8) | scale`,
            // reasoning about an RGBA word — but `engine::texture` stores
            // **ARGB**, so red is bits 16-23 and `scale` went into BLUE. The
            // symptom was not a colour shift, because nothing here is a colour:
            // `bias` landed in green and was read correctly, `scale` read back
            // as zero, and IBL's specular term therefore vanished at normal
            // incidence and survived at grazing — which looks exactly like
            // "the environment is too dim", not like a channel-order bug.
            // Lesson 6.15 §9 has the full symptom.
            //
            // The lesson generalises: a packing that is spelled out at the call
            // site is a packing that can disagree with the one the engine uses.
            // `colour.hpp` has had the right answer since Lesson 1.6.
            lut.set_texel(x, y, pack_argb(to_byte(t.scale), to_byte(t.bias), 0, 255));
        }
    }
    return lut;
}

// ---------------------------------------------------------------------------
// Putting it together
// ---------------------------------------------------------------------------

environment bake_environment(const cube_map& radiance, int irradiance_size,
                             int prefilter_levels, int lut_size)
{
    environment env;
    if (!radiance.valid()) { return env; }

    // The radiance map is COPIED rather than referenced, and the reason is the
    // struct's whole contract: an `environment` is only correct if all four parts
    // came from the same sky. Holding a pointer to someone else's cube map would
    // let the sky change under the three things baked from it.
    env.radiance = radiance;
    env.irradiance = irradiance_map(radiance, irradiance_size);
    env.prefiltered = prefilter_environment(radiance, prefilter_levels);
    env.brdf_lut = make_brdf_lut(lut_size);
    return env;
}

linear_rgb image_based_light(const environment& env, microsurface surface,
                             linear_rgb albedo, vec3 n, vec3 v)
{
    if (!env.valid()) { return {}; }

    const vec3 nn = normalised(n);
    const vec3 vv = normalised(v);
    const float n_dot_v = std::max(0.0f, dot(nn, vv));

    const linear_rgb f0 = f0_of(surface, albedo);
    const linear_rgb kd_albedo = diffuse_albedo_of(surface, albedo);

    // ---- The diffuse half: exact for Lambert -------------------------------
    //
    // `lambert_brdf` is `albedo / pi`, and the irradiance map already holds the
    // cosine-weighted integral, so this product IS the reflected radiance. With
    // a uniform environment the pi cancels against the pi in `E = pi * L` and
    // the answer is `albedo * L` — Lesson 6.2's identity, arriving as a
    // consequence rather than as a special case in the code.
    const linear_rgb e = env.irradiance.sample(nn, 0);
    linear_rgb out{kd_albedo.r * e.r / k_pi,
                   kd_albedo.g * e.g / k_pi,
                   kd_albedo.b * e.b / k_pi};

    // ---- The specular half: the split sum -----------------------------------
    //
    // The reflection of the VIEW about the normal, which is where the assumption
    // n = v = r is cashed: the prefilter baked its levels around this direction,
    // so this is the only direction it can be asked about.
    const vec3 r = reflect(-vv, nn);

    // THE FITTED MAPPING, NOT THE DERIVED ONE. `prefilter_level_for` computes
    // what the lobe actually wants; the chain was BUILT with roughness spread
    // linearly across the levels, so the lookup has to match the chain. Mixing
    // them would read level 4.2 out of a chain whose level 4.2 means something
    // else. §5.6 measures the gap between the two and this line is where the
    // choice is made — visibly, rather than inside an expression.
    const float level = std::clamp(surface.roughness, 0.0f, 1.0f)
                      * static_cast<float>(env.prefiltered.levels() - 1);
    const linear_rgb pre = env.prefiltered.sample_level(r, level);

    // The LUT: u is n.v, v is roughness, and both are read at texel centres
    // through the ordinary bilinear sampler so that the table is interpolated
    // rather than stepped.
    sampler samp;
    samp.texel_filter = filter::linear;
    samp.address_u = address_mode::clamp_to_edge;
    samp.address_v = address_mode::clamp_to_edge;
    const linear_rgb lut = sample(env.brdf_lut, samp,
                                  n_dot_v,
                                  std::clamp(surface.roughness, 0.0f, 1.0f));
    const float scale = lut.r;
    const float bias = lut.g;

    out.r += pre.r * (f0.r * scale + bias);
    out.g += pre.g * (f0.g * scale + bias);
    out.b += pre.b * (f0.b * scale + bias);
    return out;
}

} // namespace engine
