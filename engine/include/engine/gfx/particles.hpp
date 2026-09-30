// engine/include/engine/gfx/particles.hpp — a particle system, specified once for two processors.
//
// Lesson 6.18b. Everything in this file runs on the CPU, and none of it is the
// particle system the engine draws: that one runs in `particles_step.comp.hlsl`,
// on the GPU, and never comes back. This file is the SPECIFICATION the shader is
// held to — the record, the random numbers, the emission clock and the step —
// written in C++ so that a harness can run the same frames on both processors
// and compare every particle.
//
// It is also the "before" of the lesson. `particle_pool` is a complete CPU
// particle system, and §1 measures what it costs to run one and ship its state
// across the bus every frame. That cost is the whole argument for compute.
//
// ---------------------------------------------------------------------------
// THREE RULES THE GPU VERSION IS HELD TO, AND WHY EACH IS HERE
// ---------------------------------------------------------------------------
//
//   1. THE RECORD IS THREE 16-BYTE ROWS. `particle` is read by C++ and by three
//      shaders compiled through two toolchains, and a `float3` followed by a
//      `float3` is laid out 12 bytes apart by HLSL's rules and 16 apart by the
//      SPIR-V rules glslang applies (§4 of the lesson measures both). A row of
//      `float3 + one scalar` is 16 bytes under every rule there is, so the
//      disagreement has nothing to act on.
//   2. A RANDOM NUMBER IS A HASH OF A NAME, NOT THE NEXT VALUE OF A SEQUENCE.
//      Sixty-five thousand threads cannot share one generator's state, so each
//      particle derives its randomness from its own serial number. The hash is
//      integer arithmetic, which the two processors perform identically — the
//      one part of this file whose GPU output can be compared BIT FOR BIT.
//   3. THE STEP READS THE SAME UNIFORM BLOCK THE SHADER READS. `step_uniforms`
//      is built once per step, on the CPU, and both `step_slot` below and the
//      kernel consume it. Every derived number — 1/(1 + k h), 1 − cos θ, the
//      emission window — is therefore computed in ONE place, and a disagreement
//      between the processors can only come from the arithmetic that follows.
//
// ---------------------------------------------------------------------------
// WHAT A PARTICLE IS, AND WHAT IT IS NOT
// ---------------------------------------------------------------------------
//
// A point with a velocity, an age and a life span. It has no orientation, no
// mass, no size (the size is derived from age when it is drawn) and no identity
// beyond its serial number. It collides with exactly one thing — a horizontal
// ground plane — and with nothing else, least of all with another particle.
// That is the ninety-percent picture of a spark or a droplet, and the lesson's
// §14 names what the other ten per cent would cost.

#pragma once

#include <engine/math/vec3.hpp>

#include <SDL3/SDL.h>

#include <cstddef>
#include <span>
#include <vector>

namespace engine {

// ===========================================================================
//  The record
// ===========================================================================

/// One particle, exactly as the GPU stores it: 48 bytes, three 16-byte rows.
///
///     row 0   position.xyz   age      bytes  0..15
///     row 1   velocity.xyz   life     bytes 16..31
///     row 2   previous.xyz   serial   bytes 32..47
///
/// `previous` is the position at the START of the last fixed step, so the draw
/// can place a particle between two steps with Lesson 1.4's `alpha` rather than
/// at whichever step happened to be last. That costs a row — sixteen bytes a
/// particle, a third of the record — and it is what the course's fixed-step rule
/// (CLAUDE.md §4) costs a particle system.
///
/// `serial` is the particle's name: the index of its birth since the emitter
/// started. The draw hashes it for per-particle variation (size, tint), which is
/// why that variation needs no storage of its own.
///
/// **A zeroed record is a dead particle** (age 0 is not less than life 0), so a
/// buffer created and cleared to zero is a pool with nothing in it — no
/// initialisation pass needed, the same identity-element idea as 6.17b's empty
/// light list.
struct particle
{
    vec3 position{};
    float age = 0.0f;

    vec3 velocity{};
    float life = 0.0f;

    vec3 previous{};
    Uint32 serial = 0;
};

static_assert(sizeof(particle) == 48, "must match `Particle` in the particle shaders");
static_assert(offsetof(particle, age) == 12, "row 0 is position.xyz + age");
static_assert(offsetof(particle, velocity) == 16, "row 1 starts at byte 16");
static_assert(offsetof(particle, life) == 28, "row 1 is velocity.xyz + life");
static_assert(offsetof(particle, previous) == 32, "row 2 starts at byte 32");
static_assert(offsetof(particle, serial) == 44, "row 2 is previous.xyz + serial");

/// Alive means younger than its life span. Written once, used by the CPU pool,
/// the compaction kernel's C++ twin and every harness count.
[[nodiscard]] constexpr bool is_alive(const particle& p) { return p.age < p.life; }

// ===========================================================================
//  Randomness
// ===========================================================================

/// PCG's output permutation as a stateless hash of 32 bits to 32 bits.
///
/// From Jarzynski and Olano, "Hash Functions for GPU Rendering" (JCGT 9(3),
/// 2020), whose measurements make it "a good default choice" for a one-input
/// hash on both quality and speed: one multiply-add to advance an LCG state,
/// then PCG's "random xorshift, multiply, xorshift" output function to destroy
/// the LCG's low-bit patterns. The constants are from the paper's code (Nathan
/// Reed's 2021 post on the paper prints them); `particles_step.comp.hlsl`
/// carries the same four lines, and verify_618b §C checks that a million
/// outputs agree bit for bit.
[[nodiscard]] constexpr Uint32 pcg_hash(Uint32 input)
{
    const Uint32 state = input * 747796405u + 2891336453u;
    const Uint32 word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

/// 32 random bits to a float in [0, 1), EXACTLY.
///
/// The top 24 bits are an integer below 2^24, which a float represents without
/// rounding, and multiplying by 2^−24 only changes the exponent. So this
/// conversion has no rounding anywhere, and the CPU and the GPU produce the
/// same float from the same bits — which is not true of the tempting
/// `float(h) / 4294967295.0f`, whose conversion rounds 32 bits to 24 and whose
/// division may be approximated by a GPU compiler.
[[nodiscard]] constexpr float unit_float(Uint32 bits)
{
    return static_cast<float>(bits >> 8u) * (1.0f / 16777216.0f);
}

/// A particle's private stream of random numbers: hash its name once, then hash
/// the hash. Each `next()` is one more application of `pcg_hash`, so the k-th
/// number a particle draws depends on its serial, the emitter's seed and k —
/// and on nothing any other thread did.
struct particle_rng
{
    Uint32 state = 0;

    constexpr particle_rng(Uint32 serial, Uint32 seed) : state(pcg_hash(serial ^ seed)) {}

    [[nodiscard]] constexpr float next()
    {
        state = pcg_hash(state);
        return unit_float(state);
    }
};

// ===========================================================================
//  The emitter
// ===========================================================================

/// What an emitter does, in the units an artist would type.
///
/// Defaults describe the demo's fountain: a 20° cone pointing up, sparks leaving
/// at 4 to 7 m/s, living 1.5 to 3 seconds, twenty thousand a second, bouncing
/// off the ground at y = 0.
struct emitter_settings
{
    vec3 origin{0.0f, 0.05f, 0.0f};
    vec3 axis{0.0f, 1.0f, 0.0f};       ///< the cone's axis; normalised by `make_step_uniforms`
    float cone = 0.35f;                ///< half-angle, radians (about 20°)

    float speed_min = 4.0f;            ///< m/s
    float speed_max = 7.0f;
    float life_min = 1.5f;             ///< seconds
    float life_max = 3.0f;
    float rate = 20000.0f;             ///< particles per second

    vec3 gravity{0.0f, -9.81f, 0.0f};  ///< m/s², y up (conventions §1)
    float drag = 0.4f;                 ///< linear drag, per second: dv/dt = −k v
    float ground = 0.0f;               ///< the plane y = ground
    float restitution = 0.45f;         ///< fraction of normal speed kept at a bounce
    float friction = 0.25f;            ///< fraction of tangential speed LOST at a bounce

    Uint32 seed = 0x2545F491u;         ///< which fountain: same seed, same sparks
};

/// Which serial numbers are born in one step, and how early in it.
///
/// Births are spread uniformly through the step, the way a real emitter spreads
/// them through time: the k-th of `count` is born `birth_age(k)` seconds before
/// the step ends. §6 of the lesson shows the picture without this — every
/// particle of a step born at the same instant, so a fountain comes out in
/// shells one step apart.
struct emission_window
{
    Uint32 first = 0;          ///< serial of the first particle born this step
    Uint32 count = 0;          ///< how many are born this step
    float carry = 0.0f;        ///< the clock's fraction BEFORE this step, in [0, 1)
    float inv_rate = 0.0f;     ///< seconds between births
};

/// The emission clock: turns a rate and a step into whole births, and keeps the
/// fraction.
///
/// **The fraction is the whole of its job.** 20,000 a second at 60 Hz is
/// 333.33 a step, and rounding each step to 333 loses twenty particles a second
/// — every second, forever, as a rate that is silently 0.1% low. The clock keeps
/// the third and pays it out every third step.
///
/// A double, and not for the reason it looks like. The accumulator never holds
/// more than one step's births plus a fraction — `advance` subtracts the whole
/// part every time — so a float would barely drift: over an hour at 20,000 a
/// second the two differ by ONE birth (verify_618b §D). What bounds the clock's
/// accuracy is `h` itself: 1/60 is not a float, the nearest one is 0.0166666675,
/// and at that step the fountain emits 20,000.001 a second in either type. The
/// double costs eight bytes per emitter and removes the question.
class emission_clock
{
public:
    /// Advance by one step of `h` seconds at `rate` births per second, emitting
    /// at most `capacity` (a step cannot usefully refill the pool more than once).
    [[nodiscard]] emission_window advance(float rate, float h, Uint32 capacity);

    /// The next serial to be born. Wraps at 2^32, which at 20,000 a second takes
    /// 59.6 hours; `slot_of` is written so that the wrap is harmless.
    [[nodiscard]] Uint32 next_serial() const { return next_serial_; }

    void reset() { next_serial_ = 0; carry_ = 0.0; }

private:
    Uint32 next_serial_ = 0;
    double carry_ = 0.0;
};

/// Where serial `s` lives in a pool of `mask + 1` slots: `s & mask`.
///
/// **A power of two, and the reason is the wrap.** With N slots and serials
/// counting past 2^32, `s % N` is continuous across the wrap only when N divides
/// 2^32 — that is, when N is a power of two. Any other N puts two consecutive
/// serials in non-consecutive slots at the wrap, and the ring's invariant ("the
/// oldest particle is the next to be replaced") breaks once every 59 hours.
[[nodiscard]] constexpr Uint32 slot_of(Uint32 serial, Uint32 mask) { return serial & mask; }

/// Seconds the k-th birth of a window has already lived when its step ends.
///
/// The clock crosses an integer at every birth. It held `carry` when the step
/// began, so birth k (counting from 0) happens when it reaches k + 1, which is
/// (k + 1 − carry) / rate seconds into the step — and the particle is h minus
/// that old at the step's end. `inv_rate` rather than a division, so both
/// processors multiply by the same float.
[[nodiscard]] inline float birth_age(Uint32 k, float carry, float inv_rate, float h)
{
    return h - (static_cast<float>(k) + (1.0f - carry)) * inv_rate;
}

// ===========================================================================
//  The step, as one uniform block
// ===========================================================================

/// The compute kernel's uniform block, byte for byte. **Must match
/// `cbuffer Step` in `particles_step.comp.hlsl`**; nine 16-byte rows.
///
/// Built by `make_step_uniforms` and consumed unchanged by BOTH `step_slot`
/// (below) and the shader. That is the rule this struct exists to enforce: the
/// CPU reference does not recompute 1/(1 + k h), 1 − cos θ or the cone's
/// tangent frame for itself, it reads them out of the same bytes the GPU reads.
/// A number that is the same for every particle is computed once, here, and
/// never sixty-five thousand times in a kernel.
struct step_uniforms
{
    // row 0
    vec3 origin{};
    float h = 0.0f;                    ///< the step, seconds
    // row 1
    vec3 axis{};                       ///< unit
    float one_minus_cos_cone = 0.0f;   ///< the cap's height: uniform directions need it
    // row 2
    vec3 tangent{};                    ///< unit, perpendicular to `axis`
    float speed_min = 0.0f;
    // row 3
    vec3 bitangent{};                  ///< axis × tangent's partner: (tangent, bitangent, axis) is orthonormal
    float speed_span = 0.0f;           ///< max − min
    // row 4
    vec3 gravity{};
    float inv_drag_h = 1.0f;           ///< 1 / (1 + k h): the implicit drag factor for one step
    // row 5
    float life_min = 0.0f;
    float life_span = 0.0f;
    float ground = 0.0f;
    float restitution = 0.0f;
    // row 6
    float keep = 1.0f;                 ///< 1 − friction: tangential speed kept at a bounce
    float drag = 0.0f;                 ///< k, for a birth's pre-age step of its own length
    float carry = 0.0f;                ///< the emission window's clock fraction
    float inv_rate = 0.0f;
    // row 7
    Uint32 first = 0;                  ///< the emission window
    Uint32 count = 0;
    Uint32 mask = 0;                   ///< capacity − 1
    Uint32 seed = 0;
    // row 8
    Uint32 capacity = 0;
    Uint32 pad0 = 0;
    Uint32 pad1 = 0;
    Uint32 pad2 = 0;
};

static_assert(sizeof(step_uniforms) == 144, "must match cbuffer Step in particles_step.comp.hlsl");
static_assert(offsetof(step_uniforms, tangent) == 32, "a vec3 starts every row");
static_assert(offsetof(step_uniforms, first) == 112, "the integers are row 7");

/// Fold an emitter, a window and a step size into the block both processors read.
///
/// @param capacity the pool's size; **must be a power of two** (see `slot_of`).
[[nodiscard]] step_uniforms make_step_uniforms(const emitter_settings& e,
                                               const emission_window& w,
                                               float h, Uint32 capacity);

// ===========================================================================
//  What one thread does
// ===========================================================================

/// A new particle, born `age` seconds ago, named `serial`.
///
/// The direction is uniform over the cone's cap — equal AREA, which is uniform
/// in cos θ and not in θ (§5 of the lesson) — the speed and the life uniform in
/// their ranges, and then the particle is advanced by its own age in one step,
/// so it is where it would be had it really been born that long ago.
[[nodiscard]] particle spawn_particle(Uint32 serial, float age, const step_uniforms& u);

/// Advance one live particle by one step of `dt` seconds.
///
/// Semi-implicit Euler (Lesson 1.3's "other order", derived properly in 8.1):
/// velocity first, then position with the NEW velocity. Linear drag is applied
/// IMPLICITLY — v' = (v + g dt) / (1 + k dt) — which is stable for any step,
/// where the explicit v' = v + (g − k v) dt reverses the velocity once k dt
/// exceeds 1 and explodes past 2 (§7 measures both). The ground reflects the
/// normal velocity with `restitution` and scales the tangential by `keep`.
///
/// @param inv_drag 1 / (1 + drag * dt), passed in rather than divided here so
///        that a full step reads it from the uniform block like the shader does.
void integrate_particle(particle& p, const step_uniforms& u, float dt, float inv_drag);

/// Exactly what thread `slot` of the step kernel does: respawn the slot if the
/// window names it, otherwise advance whatever lives there (if anything does).
///
/// The kernel's `main` is this function, transliterated. Read them side by side.
void step_slot(particle& p, Uint32 slot, const step_uniforms& u);

// ===========================================================================
//  The CPU particle system
// ===========================================================================

/// A complete particle system on the CPU: a pool, an emitter's clock, a step.
///
/// Two jobs. It is the REFERENCE the GPU is compared against — every harness in
/// the lesson runs one of these beside the kernel — and it is the BEFORE: the
/// thing a program would use without compute, whose cost per step and per byte
/// uploaded §1 measures.
class particle_pool
{
public:
    /// @param capacity rounded UP to a power of two, and at least 64 (one
    ///        thread group, so the kernel's grid is never smaller than a group).
    explicit particle_pool(Uint32 capacity);

    /// One fixed step: advance the clock, then run `step_slot` over every slot.
    /// Returns the uniform block the step used, which is what a GPU twin of this
    /// pool must be handed to take the same step.
    step_uniforms step(const emitter_settings& e, float h);

    /// Every slot, dead ones included, in slot order.
    [[nodiscard]] std::span<const particle> particles() const { return particles_; }
    [[nodiscard]] std::span<particle> particles() { return particles_; }

    [[nodiscard]] Uint32 capacity() const { return static_cast<Uint32>(particles_.size()); }
    [[nodiscard]] Uint32 alive_count() const;

    /// The serial the next birth will get; also the number born so far, mod 2^32.
    [[nodiscard]] Uint32 born() const { return clock_.next_serial(); }

    /// Kill everything and rewind the clock.
    void reset();

private:
    std::vector<particle> particles_;
    emission_clock clock_;
};

/// The smallest power of two that is at least `n` and at least 64.
[[nodiscard]] Uint32 particle_capacity_for(Uint32 n);

} // namespace engine
