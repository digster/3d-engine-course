// engine/include/engine/gfx/cubemap.hpp — the sky, and the two integrals it feeds.
//
// Lesson 6.15. Every light in this engine so far has been a DIRECTION and a
// COLOUR, and `lighting::ambient` has been the apology for everything else: one
// constant radiance arriving equally from every direction, standing in for the
// sky, the floor, the walls and the bounce off all of them. Lesson 6.2 promoted
// it from fudge to model by showing the pi cancels exactly — `albedo * ambient`
// IS the right answer for a uniform environment — and then named what stays
// wrong with it, which is not the arithmetic but the assumption. Real bounced
// light is not uniform. It comes mostly from above.
//
// AND IT HAS NO SPECULAR HALF AT ALL. That is the more serious of the two gaps
// and the easier to see: `scene.frag.hlsl` has been carrying the sentence "a
// mirror in a bright uniform room renders black except where the key light
// reaches it" since Lesson 6.4, because `base * ambient` goes through no BRDF and
// a metal has no `base` to multiply. Chrome in this engine is currently lit by
// one lamp and nothing else.
//
// AN ENVIRONMENT MAP FIXES BOTH BY STORING RADIANCE AS A FUNCTION OF DIRECTION.
// `L_i(d)` — how much light arrives from this way — sampled on the six faces of a
// cube. It is the same data twice over, which is the honest part of the
// technique: the sky you SEE behind the objects and the sky that LIGHTS them are
// one texture, so a reflection cannot disagree with its surroundings.
//
// ---------------------------------------------------------------------------
// THE TWO INTEGRALS, AND ONLY ONE OF THEM IS EXACT
// ---------------------------------------------------------------------------
//
// What we want, per shaded point, is the reflected radiance
//
//     L_o(v) = integral over the hemisphere of  f_r(l, v) * L_i(l) * (n.l) dw
//
// evaluated at every pixel, over an environment with thousands of texels in it.
// That is not a per-frame budget; it is a per-frame *impossibility*. The whole
// technique is two ways of moving the integral offline.
//
//   THE DIFFUSE HALF FACTORS, EXACTLY. Lambert's BRDF is `albedo/pi`, a
//   CONSTANT — it does not depend on `l` at all — so it comes straight out of
//   the integral:
//
//       L_o = (albedo/pi) * integral L_i(l) (n.l) dw  =  (albedo/pi) * E(n)
//
//   and `E(n)`, the irradiance, depends only on the NORMAL. Precompute it into a
//   small cube map (`irradiance_map`) and the run-time cost is one texture
//   fetch. There is no approximation anywhere in that sentence: the only error
//   is the discretisation, which `verify_615` §C measures converging as 1/n^2
//   and which reaches 1.1e-5 relative at 64x64 faces. **Set the environment to a
//   uniform L and this reproduces Lesson 6.2's `albedo * ambient` to six
//   decimals** — the old term is a special case of the new one, not a casualty
//   of it.
//
//   THE SPECULAR HALF DOES NOT FACTOR, AND WE PRETEND IT DOES. `f_r` depends on
//   `l` and `v` together, so nothing comes out of the integral. The SPLIT-SUM
//   APPROXIMATION (Karis 2013) splits it anyway:
//
//       integral f_r L_i (n.l) dw  ~=  [ prefiltered L_i ] * [ integral f_r (n.l) dw ]
//
//   The left bracket is the environment blurred by the lobe's width and stored as
//   a mip chain (`prefilter_environment`); the right is a 2-D function of
//   `(n.v, roughness)` alone, tabulated once for all materials and all
//   environments (`integrate_brdf`). Two lookups, one multiply.
//
//   **The price is measured rather than waved at, and it has a SHAPE.**
//   `verify_615` §E runs the true integral by brute force at a million samples
//   and compares against the same cube the engine reads. On a smooth sky the
//   split sum is within 0.08% at roughness 0.10, never worse than 5.8% at
//   near-normal incidence, and reaches 29.0% dark at roughness 1 seen edge-on.
//   With a sun disc in the sky it reaches 73.0%.
//
//   THAT ASYMMETRY IS NOT NOISE, IT IS THE SECOND APPROXIMATION. Prefiltering
//   has to assume `n = v = r`, because a prefiltered value is indexed by ONE
//   direction while the true integral depends on two. At a grazing view the
//   reflection direction points across the horizon, so the prefilter's lobe
//   averages in ground the real surface never sees — and what it discards is
//   the stretched, comet-shaped highlight a rough surface shows a grazing
//   light. The sun case adds a third cause: the prefilter weights radiance by
//   `n.l` alone where the true integral weights it by the whole BRDF, and those
//   diverge most where the environment is nearly a delta function. **The error
//   of this technique depends on the environment, not only on the material.**
//
// ---------------------------------------------------------------------------
// WHY A CUBE AND NOT A LATITUDE-LONGITUDE IMAGE
// ---------------------------------------------------------------------------
//
// A lat-long (equirectangular) map is one rectangle and is much easier to author
// and to look at. It is also spectacularly non-uniform: every texel in its top
// row covers the same pole, so a 2:1 image spends a whole scanline on a single
// point and a quarter of its texels on the two polar caps. A cube's worst
// non-uniformity is the corner texel, which subtends 1/(3*sqrt(3)) = 0.1925 of
// the centre one — a factor of **5.196**, against a lat-long's unbounded one —
// and hardware samples a cube map natively, with seamless filtering across face
// edges, which is not something a lat-long lookup gets for free.
//
// **That 5.196 is not trivia; it is the correctness bug this file exists to
// avoid.** Averaging cube-map texels with equal weights treats the corners as if
// they were as big as the centres, and `verify_615` §B measures the standing
// error at 1.01% — standing, because refining the cube does not reduce it. Every
// integral in this file weights by `cube_texel_solid_angle`.

#pragma once

#include <engine/gfx/colour.hpp>
#include <engine/gfx/hdr.hpp>
#include <engine/gfx/microfacet.hpp>
#include <engine/gfx/texture.hpp>
#include <engine/math/vec2.hpp>
#include <engine/math/vec3.hpp>

#include <vector>

namespace engine {

// ---------------------------------------------------------------------------
// Faces
// ---------------------------------------------------------------------------

/// The six faces, **in SDL's order** — which is also D3D's, Vulkan's and Metal's.
///
/// The numbering is not ours to choose: `SDL_GPUCubeMapFace` is
/// `POSITIVEX, NEGATIVEX, POSITIVEY, NEGATIVEY, POSITIVEZ, NEGATIVEZ`, and the
/// enum below is `static_cast`-compatible with it so that a face index computed
/// on the CPU can be handed straight to `SDL_GPUTextureRegion::layer`. That is
/// the same deliberate correspondence Lesson 3.9's `filter` and `address_mode`
/// have with `SDL_GPUFilter` and `SDL_GPUSamplerAddressMode`, and `verify_615`
/// asserts it the same way — so if SDL ever reorders, a test fails instead of
/// six faces quietly swapping places.
enum class cube_face : int
{
    pos_x = 0,
    neg_x = 1,
    pos_y = 2,
    neg_y = 3,
    pos_z = 4,
    neg_z = 5
};

inline constexpr int k_cube_faces = 6;

[[nodiscard]] const char* name_of(cube_face f);

/// Where a direction lands: which face, and where on it.
struct cube_texel
{
    cube_face face = cube_face::pos_x;
    float u = 0.0f;   ///< [0,1] across the face, left to right
    float v = 0.0f;   ///< [0,1] down the face — **v grows downward**, see below
};

/// Direction -> face and uv. The lookup a cube map *is*.
///
/// **The algorithm in one sentence**: the largest component of `d` picks the
/// face, and dividing the other two by its magnitude projects the direction onto
/// that face's plane at distance 1. Everything else is a sign convention.
///
/// And the sign convention is the part that costs people a day, so here it is in
/// full, exactly as the D3D and OpenGL specifications give it (`sc` runs along
/// the face's u axis, `tc` along its v axis, `ma` is the major axis):
///
///     face   sc    tc    ma
///     +X     -z    -y     x
///     -X     +z    -y     x
///     +Y     +x    +z     y
///     -Y     +x    -z     y
///     +Z     +x    -y     z
///     -Z     -x    -y     z
///
/// then `u = 0.5*(sc/|ma| + 1)` and `v = 0.5*(tc/|ma| + 1)`.
///
/// **`tc = -y` on four of the six faces is where `v` grows DOWNWARD comes from**,
/// and it is the same top-left texel origin every other image in this engine has
/// used since Lesson 3.9 (`texel_origin::top_left`). It is not an accident that
/// they agree; it is the same inherited convention.
///
/// **THE HANDEDNESS WARNING, WHICH IS THE REAL ONE.** This table was written for
/// a LEFT-handed coordinate system, and conventions §2 pins this engine's world
/// as RIGHT-handed, Y-up, -Z forward. Look along +X in a right-handed Y-up world
/// and your right hand points toward +Z; the table puts the +X face's `u` axis
/// along -Z. So **a cube map assembled by a right-handed camera and sampled by
/// this table comes out mirrored** — left-right flipped, which on a sky is
/// almost invisible and on anything with text in it is unmistakable. The fix is
/// not to "correct" the table, which the hardware implements and we cannot
/// change; it is to generate each face with the axes this table names.
/// `make_sky_environment` does exactly that, by construction, because it
/// evaluates a function of the direction `cube_to_direction` hands it.
[[nodiscard]] cube_texel direction_to_cube(vec3 d);

/// Face and uv -> direction. The exact inverse of `direction_to_cube`, to within
/// float rounding (`verify_615` §A measures the round trip at 8.6e-8 worst case
/// over 1,734 directions).
///
/// The result is **normalised**, which is worth being deliberate about: the
/// un-normalised face point `(sc, tc, +-1)` is what the projection produces, and
/// it is what you want for the *ray*, but every integral in this file weights by
/// `cos(theta) = dot(n, d)` and a non-unit `d` silently scales that cosine by up
/// to sqrt(3).
[[nodiscard]] vec3 cube_to_direction(cube_face f, float u, float v);

/// The solid angle, in steradians, of texel `(i, j)` on a `size x size` face.
///
/// **This is the weight every integral in this file needs, and the reason is
/// geometric rather than numerical.** A cube face is a FLAT square wrapped onto a
/// sphere, so its texels do not subtend equal solid angles. A texel at face
/// coordinates `(x, y)` (both in [-1,1]) sits at distance `r = sqrt(1+x^2+y^2)`
/// from the centre, and its own plane is tilted away from the line of sight by
/// `cos(theta) = 1/r`, so
///
///     dw = dA * cos(theta) / r^2 = dA / r^3
///
/// At the centre `r = 1`; at a corner `r = sqrt(3)` and `r^3 = 3*sqrt(3) =
/// 5.196`. **The centre texel is worth 5.196 corner texels** and an unweighted
/// average is therefore biased, by a measured 1.01% on the cosine integral — a
/// bias that does NOT shrink as the cube is refined, which is what makes it a
/// bug rather than an error.
///
/// The implementation uses the exact closed form rather than `dA/r^3`, because a
/// texel is a finite patch and the approximation is only good in its middle:
/// Lambert's formula for the solid angle of an axis-aligned rectangle,
///
///     f(x, y) = atan2(x*y, sqrt(x^2 + y^2 + 1))
///     w = f(x1,y1) - f(x0,y1) - f(x1,y0) + f(x0,y0)
///
/// which is a difference of corner terms in exactly the way a 2-D integral's
/// antiderivative is. The check that it is right is that six faces' worth sums
/// to `4*pi` — `verify_615` §B measures the residual at 7e-16 relative, which is
/// double-precision round-off and not error.
[[nodiscard]] float cube_texel_solid_angle(int size, int i, int j);

/// The **Hammersley** point set — a low-discrepancy sequence in the unit square.
///
/// Lesson 6.15's sampler, and the reason it is not `rand()` is a convergence
/// rate. Monte Carlo with independent random samples converges as `1/sqrt(N)`;
/// a low-discrepancy sequence, which fills the square evenly rather than
/// randomly, converges close to `1/N` on the smooth integrands here. That is the
/// difference between 64 samples and 4,096 for the same prefilter quality, and
/// prefiltering is `6 * size^2 * levels` integrals.
///
/// `x` is just `(i + 0.5)/n`. `y` is the **radical inverse in base 2**: write `i`
/// in binary and reflect it about the binary point, so 1 -> 0.1b = 0.5,
/// 2 -> 0.01b = 0.25, 3 -> 0.11b = 0.75. Consecutive points therefore keep
/// landing in the largest remaining gap, which is the whole idea. The
/// implementation does the reflection with five shift-and-mask steps rather than
/// a loop — the standard bit-twiddle, spelled out and explained in the .cpp.
[[nodiscard]] vec2 hammersley(int i, int n);

// ---------------------------------------------------------------------------
// The map
// ---------------------------------------------------------------------------

/// Six square faces of **linear radiance**, optionally with a mip chain.
///
/// **The faces are `hdr_buffer`s and that is not a detail.** An environment holds
/// radiance, and radiance has no upper bound: the sky is around 1, and a sun disc
/// in the same image is five thousand times brighter. Lesson 6.12 built
/// `hdr_buffer` for exactly this reason on the framebuffer side, and an
/// environment map is the other place in the engine where a [0,1] container would
/// destroy the data before any of the arithmetic ran. Storing a sky in
/// `engine::texture` would clip the sun to white and the prefilter would then be
/// integrating a lie.
///
/// **The mip chain is indexed by ROUGHNESS, not by footprint.** That is the one
/// sentence separating this class from Lesson 6.10's `mip_chain`, and it is the
/// reason both exist. 6.10's levels answer "how much of this texture does one
/// pixel cover"; these answer "how wide is the lobe reflecting it". The mechanism
/// — a pyramid of progressively blurrier copies, filtered between — is identical,
/// which is the third time in this module that the answer to an undersampling
/// problem has been *prefilter the thing being undersampled* (6.10's mips, 6.14's
/// NDF, now this).
class cube_map
{
public:
    cube_map() = default;

    /// `size x size` texels per face, `levels` mip levels, all black.
    ///
    /// Level `k` has `max(1, size >> k)` texels per side, exactly as Lesson
    /// 6.10's chain does — the same halving, stopping at 1x1.
    cube_map(int size, int levels);

    [[nodiscard]] bool valid() const { return size_ > 0 && !faces_.empty(); }
    [[nodiscard]] int size() const { return size_; }
    [[nodiscard]] int levels() const { return levels_; }

    /// Texels per side at `level` — `max(1, size >> level)`.
    [[nodiscard]] int size_at(int level) const;

    [[nodiscard]] hdr_buffer& face(cube_face f, int level = 0);
    [[nodiscard]] const hdr_buffer& face(cube_face f, int level = 0) const;

    /// Bilinear lookup **within one face**, at one level.
    ///
    /// **It does not filter across face edges**, and that is a deliberate,
    /// documented limit rather than an oversight. Hardware cube samplers do
    /// filter across edges (the feature is called seamless cube filtering and has
    /// been mandatory since D3D10); doing it in software means finding the
    /// neighbouring face, rotating its uv into this face's frame, and handling
    /// the three-way ambiguity at a corner where only three texels exist for a
    /// 2x2 fetch. The visible cost of not doing it is a hairline seam on the
    /// blurriest mip levels, where one texel is a large angle — `verify_615` §C
    /// measures the worst seam discontinuity so the number is known rather than
    /// guessed. The GPU path has no such limit, because it is the GPU's sampler.
    [[nodiscard]] linear_rgb sample(vec3 d, int level = 0) const;

    /// Trilinear: bilinear within the two bracketing levels, then lerp.
    ///
    /// `level` is a CONTINUOUS mip level, the same quantity `mip_level_for`
    /// returns in Lesson 6.10 and the same one `prefilter_level_for` returns
    /// here. Values outside `[0, levels-1]` are clamped, which is the behaviour
    /// `sampler::mip` documents for the 2-D case.
    [[nodiscard]] linear_rgb sample_level(vec3 d, float level) const;

    /// Total texels across all six faces and all levels — for reporting a memory
    /// figure that is a measurement rather than an estimate.
    [[nodiscard]] std::size_t texel_count() const;

private:
    std::vector<hdr_buffer> faces_;   ///< 6 per level, level-major
    int size_ = 0;
    int levels_ = 0;
};

// ---------------------------------------------------------------------------
// Environments to integrate
// ---------------------------------------------------------------------------

/// A constant radiance in every direction — **the environment Lesson 6.2 was
/// already assuming**.
///
/// This exists to be a test, not a scene. Feed it to `irradiance_map` and every
/// texel of the result must come back at `pi * radiance`, because the integral of
/// `cos` over the hemisphere is exactly `pi`; shade with it and the answer must
/// equal `albedo * radiance`, which is the expression `light.hpp` has used since
/// Lesson 3.6. A new subsystem that reproduces the old one on the old one's own
/// input is a subsystem you can trust with a new input.
[[nodiscard]] cube_map make_uniform_environment(int size, linear_rgb radiance);

/// The parameters of `make_sky_environment`.
///
/// A procedural sky rather than a loaded `.hdr` file, for three reasons worth
/// stating: it is reproducible bit-for-bit on every machine, so a measurement in
/// this lesson is a measurement in yours; it can be evaluated analytically at any
/// direction, so a cube map of it can be checked against the function it came
/// from; and it ships as nine floats instead of a twelve-megabyte asset.
///
/// It is **not** a physically-based sky model — Preetham and Hosek-Wilkie are, and
/// §11 points at both. This is a horizon gradient, a ground, and a disc, chosen
/// so that the numbers a real sky produces (a sun four orders of magnitude above
/// the sky around it) are present to be integrated.
struct sky_settings
{
    linear_rgb zenith{0.35f, 0.45f, 0.90f};    ///< straight up
    linear_rgb horizon{0.60f, 0.70f, 0.90f};   ///< at the horizon, fading to zenith
    linear_rgb ground{0.05f, 0.045f, 0.04f};   ///< straight down

    /// The ground AT the horizon, faded to over the lower hemisphere.
    ///
    /// **Not decoration: a constant lower hemisphere is the one part of a
    /// naive sky model that is obviously wrong**, and it is wrong in a way that
    /// matters here rather than merely looking flat. Real ground is lit by the
    /// sky above it, so it is brightest where it faces the most sky — near the
    /// horizon — and darkest straight down. A constant makes every downward
    /// normal in the scene receive identical fill, which is precisely the defect
    /// `lighting::ambient` had and which this whole lesson exists to remove.
    ///
    /// It also decides whether the demo shows anything. The orbit camera sits
    /// 26 degrees above the scene with a 50-degree field of view, so the horizon
    /// falls about one degree above the top of the frame and the ENTIRE
    /// background is this hemisphere. With a constant it is a flat rectangle;
    /// with the fade it is a gradient the reflections can be checked against.
    linear_rgb ground_horizon{0.16f, 0.14f, 0.12f};

    vec3 sun_direction{0.3f, 0.6f, -0.5f};     ///< normalised on use
    linear_rgb sun_radiance{6000.0f, 5600.0f, 5000.0f};

    /// Angular radius of the disc, in degrees. The real sun's is **0.266**, and
    /// the default here is deliberately a little larger so a 128x128 face has
    /// more than one texel of it to hold — see `verify_615` §D, which measures
    /// what a sun smaller than a texel does to the prefilter.
    float sun_angular_radius_deg = 1.0f;
};

/// Evaluate the sky analytically, in a given direction. No texture involved.
[[nodiscard]] linear_rgb sky_radiance(const sky_settings& s, vec3 d);

/// Bake `sky_radiance` into a cube map by evaluating it at every texel centre.
[[nodiscard]] cube_map make_sky_environment(int size, const sky_settings& s);

// ---------------------------------------------------------------------------
// Integral 1 — irradiance (the exact one)
// ---------------------------------------------------------------------------

/// Convolve `env` with the cosine lobe: `E(n) = integral L_i(l) (n.l) dw`.
///
/// The result is a cube map indexed by the **surface normal**, holding
/// irradiance. Divide by pi and multiply by albedo to get outgoing radiance —
/// which is to say, `lambert_brdf(albedo) * irradiance`, the function `light.hpp`
/// has had since Lesson 6.2 and which has been fed a constant until now.
///
/// **Small is correct here, not merely cheap.** Irradiance is the environment
/// convolved with a cosine, and a cosine is about as low-pass as a filter gets:
/// the result has no detail finer than roughly 60 degrees, so 32x32 faces
/// (or even 16x16) are not a compromise — they are the Nyquist rate for what is
/// left after the convolution. Storing it at the source resolution would be
/// storing sixty-four times the texels to represent the same function.
///
/// **The cost is quadratic in the wrong way, and that is why this is offline.**
/// Every output texel integrates over every input texel, so this is
/// `6*out^2 * 6*in^2` samples: at out=32 and in=128 that is 6.0e8 cosine
/// evaluations. `verify_615` §E times it. Real engines do this on the GPU, or in
/// spherical harmonics, and §11 points at both — nine SH coefficients reproduce
/// an irradiance map to within a couple of percent and are a genuinely better
/// representation, which is the exercise at the end of the lesson.
///
/// @param out_size  faces of the RESULT. 32 is plenty; see above.
[[nodiscard]] cube_map irradiance_map(const cube_map& env, int out_size);

// ---------------------------------------------------------------------------
// Integral 2 — the split sum (the approximate one)
// ---------------------------------------------------------------------------

/// The roughness level `k` of a prefiltered chain stands for.
///
/// Linear in roughness across the chain: level 0 is a mirror, the last level is
/// `roughness = 1`. **This is a convention the prefilter and the lookup have to
/// agree on**, so it lives in one function that both call rather than as the
/// expression `k / (levels - 1)` written twice.
[[nodiscard]] float roughness_for_level(int level, int levels);

/// The mip level a given roughness should read — **derived, not chosen**.
///
/// Lesson 6.15's small piece of new mathematics, and it is 6.14's
/// `ggx_lobe_half_angle` cashing a cheque. The question "which level" is really
/// "which level's texels are about as wide as this lobe", and both sides of that
/// can be measured in steradians:
///
///     lobe:            ggx_lobe_solid_angle(alpha)
///     texel at level k: w0 * 4^k       (each level halves the side, so
///                                       quadruples the solid angle)
///
/// Set them equal and take logs:
///
///     k = 0.5 * log2( ggx_lobe_solid_angle(alpha) / w0 )
///
/// with `w0` the centre texel's solid angle at level 0.
///
/// **AND HERE IS THE HONEST PART, WHICH IS 6.14'S DISCIPLINE APPLIED TO THIS
/// LESSON.** The chain is built with roughness spread LINEARLY across the levels
/// (`roughness_for_level`), because that is what everybody ships and what the
/// GPU's `sample_level` interpolates between. The formula above says what the
/// lobe actually wants. They are different functions, and `verify_615` §G
/// measures the gap: **at worst 0.77 of a level, at roughness 0.45**, with the
/// linear mapping over-blurring a near-mirror (0.70 levels of blur at roughness
/// 0.10, where the lobe wants none) and under-blurring everything from 0.3 to
/// 0.9. The folklore is not arbitrary — it is a good fit to the derived answer,
/// and now we know by how much and in which direction. For comparison,
/// `sqrt(roughness)` — also seen in the wild — is off by 2.21 levels.
///
/// So this function is offered as the DERIVED answer and the engine ships the
/// FITTED one, with the boundary marked, exactly as 6.14 marked the boundary
/// between its closed-form lobe width and Tokuyoshi & Kaplanyan's fitted
/// constants. Use it to decide whether your chain is deep enough: if the derived
/// level for your roughest material exceeds `levels - 1`, it is not.
///
/// @param base_size  texels per side at level 0.
[[nodiscard]] float prefilter_level_for(float roughness, int base_size, int levels);

/// Blur `env` by the GGX lobe, once per level, into a chain indexed by roughness.
///
/// **Level 0 is a copy, not a blur**, because roughness 0 is a mirror and a
/// mirror reflects the environment unchanged. Every level after it importance-
/// samples the GGX distribution around the reflection direction and averages the
/// radiance it finds, weighted by `n.l`.
///
/// **THE ASSUMPTION THAT MAKES THIS POSSIBLE, AND WHAT IT COSTS.** A prefiltered
/// value is indexed by ONE direction, but the true integral depends on both the
/// normal and the view. The only way out is to assume they coincide:
///
///     n = v = r
///
/// which is exactly true when you look straight down at a surface and
/// increasingly wrong as you look along it. What it discards is the *stretched*
/// highlight — the comet-shaped smear a rough surface shows a grazing light —
/// leaving a round one. It is the approximation you can see, where the split sum
/// itself is the one you can measure. Lesson 6.15 §5.3 shows both.
///
/// @param levels  how deep. 6 levels off a 128 base bottoms out at 4x4, which is
///                enough for roughness 1 — see `prefilter_level_for`.
/// @param samples per output texel. 64 with `hammersley` is the usual figure and
///                is what `verify_615` §H shows converged; random sampling needs
///                roughly 16x more for the same error.
[[nodiscard]] cube_map prefilter_environment(const cube_map& env, int levels, int samples = 64);

/// The second bracket of the split sum: a **scale and bias on F0**.
///
/// The trick that makes this a 2-D table rather than a 3-D one is Schlick's
/// Fresnel being LINEAR in F0:
///
///     F = F0 + (1 - F0) * (1 - v.h)^5
///
/// so the integral `integral f_r (n.l) dw` splits into a part multiplying F0 and
/// a part that does not, and NEITHER depends on F0. One table serves every
/// material in the scene — gold, chrome, plastic, the lot — and the shader does
///
///     specular = prefiltered * (F0 * scale + bias)
///
/// with `scale` and `bias` read from `(n.v, roughness)`.
///
/// @param samples  importance samples. 1,024 is converged to better than 1e-4;
///                 `verify_615` §H shows the convergence.
/// @return `{scale, bias}`, both in [0,1] and both dimensionless.
struct brdf_terms
{
    float scale = 0.0f;   ///< multiplies F0
    float bias = 0.0f;    ///< added to it
};

[[nodiscard]] brdf_terms integrate_brdf(float n_dot_v, float roughness, int samples = 1024);

/// Tabulate `integrate_brdf` into a `size x size` image, u = n.v, v = roughness.
///
/// **`texel_space::linear`, and the reason is the same one Lesson 6.7 gave for
/// normal maps**: these are not colours, they are the coefficients of an
/// integral, and putting them through an sRGB transfer function would be a
/// category error that happens to look plausible. 6.7 added `texel_space` to this
/// engine for precisely this class of texture and this is its third user.
///
/// **The v axis runs bottom-up in ROUGHNESS but the texture's v runs top-down**
/// (`texel_origin::top_left`, Lesson 3.9), so row 0 is roughness 0. Say it once,
/// here, rather than discovering it as a table that is upside down — which is an
/// error that produces a picture that is wrong everywhere and obviously wrong
/// nowhere.
[[nodiscard]] texture make_brdf_lut(int size, int samples = 1024);

// ---------------------------------------------------------------------------
// Putting it together, on the CPU
// ---------------------------------------------------------------------------

/// Everything a shading point needs from the environment, precomputed.
///
/// Held together in one struct because they are only ever correct TOGETHER: the
/// prefiltered chain and the LUT are two halves of one approximation, and the
/// irradiance map has to have been baked from the same environment or the diffuse
/// and specular halves will disagree about which way the sun is.
struct environment
{
    cube_map radiance;     ///< the sky itself — what the skybox draws
    cube_map irradiance;   ///< integral 1, indexed by normal
    cube_map prefiltered;  ///< integral 2's first bracket, indexed by roughness
    texture brdf_lut;      ///< integral 2's second bracket

    [[nodiscard]] bool valid() const
    {
        return radiance.valid() && irradiance.valid() && prefiltered.valid()
            && brdf_lut.width() > 0;
    }
};

/// Bake all three from one environment, with the sizes this lesson argues for.
[[nodiscard]] environment bake_environment(const cube_map& radiance,
                                           int irradiance_size = 32,
                                           int prefilter_levels = 6,
                                           int lut_size = 64);

/// The image-based ambient term: what `base * ambient` becomes.
///
/// **This is the function that closes the sentence `scene.frag.hlsl` has carried
/// since Lesson 6.4** — "a mirror in a bright uniform room renders black except
/// where the key light reaches it" — and it closes it by giving the ambient half
/// a specular lobe for the first time in the course.
///
/// Both halves are here because both are ambient: the diffuse half is the
/// irradiance map through Lambert's BRDF, the specular half is the split sum.
/// They are added, not blended, and the metallic workflow divides the albedo
/// between them exactly as `f0_of` and `diffuse_albedo_of` already do for the
/// direct light — which is the check that this is the same shading model and not
/// a second one bolted on.
///
/// @param n  the (unit) surface normal, world space.
/// @param v  the (unit) direction toward the eye, world space.
[[nodiscard]] linear_rgb image_based_light(const environment& env,
                                           microsurface surface,
                                           linear_rgb albedo,
                                           vec3 n, vec3 v);

} // namespace engine
