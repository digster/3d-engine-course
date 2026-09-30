// engine/src/gfx/particles.cpp — the particle system's specification, executable.
//
// Lesson 6.18b. Every function below has a twin in `particles_step.comp.hlsl`,
// and the two are written in the SAME ORDER OF OPERATIONS on purpose. A
// floating-point expression is not a mathematical one: `a + b * c` and
// `(a + b) * c`-shaped rearrangements round differently, and a compiler is only
// forbidden from rearranging what the source did not already arrange. Keeping
// the two texts parallel is what makes the harness's comparison mean "the GPU
// did what the CPU did" rather than "two similar programs gave similar numbers".

#include <engine/gfx/particles.hpp>

#include <cmath>

namespace engine {

namespace {

constexpr float k_two_pi = 6.28318530717958647692f;

} // namespace

// ===========================================================================
//  The emission clock
// ===========================================================================

emission_window emission_clock::advance(float rate, float h, Uint32 capacity)
{
    emission_window w{};
    w.first = next_serial_;
    w.inv_rate = (rate > 0.0f) ? 1.0f / rate : 0.0f;

    // THE FRACTION BEFORE THE STEP is what places this step's births in time
    // (`birth_age`), so it is captured before the clock moves.
    w.carry = static_cast<float>(carry_);

    if (rate <= 0.0f || h <= 0.0f)
    {
        return w;
    }

    carry_ += static_cast<double>(rate) * static_cast<double>(h);
    const double whole = std::floor(carry_);
    carry_ -= whole;

    // AT MOST ONE POOL PER STEP. More births than slots in one step would give
    // two serials the same slot in the same dispatch — two threads, one record,
    // no order between them. The excess is dropped, not queued: an emitter asked
    // for more than its pool can hold has been configured wrongly, and a queue
    // would hide that.
    const double cap = static_cast<double>(capacity);
    w.count = static_cast<Uint32>((whole < cap) ? whole : cap);

    // Unsigned wrap is DEFINED behaviour in C++ and in HLSL, and `slot_of`
    // relies on it being continuous — see the header.
    next_serial_ += w.count;
    return w;
}

// ===========================================================================
//  The uniform block
// ===========================================================================

step_uniforms make_step_uniforms(const emitter_settings& e, const emission_window& w,
                                 float h, Uint32 capacity)
{
    step_uniforms u{};
    u.origin = e.origin;
    u.h = h;

    const vec3 n = normalised_or(e.axis, vec3{0.0f, 1.0f, 0.0f});
    u.axis = n;

    // 1 − cos θ, computed ONCE, here. It is the height of the cone's cap on the
    // unit sphere, and §5 derives why a direction uniform over the cap is one
    // whose cosine is uniform over [cos θ, 1] — which is 1 − u·(1 − cos θ).
    u.one_minus_cos_cone = 1.0f - std::cos(e.cone);

    // THE TANGENT FRAME, branch-free: Duff, Burgess, Christensen, Hery, Kensler,
    // Liani and Villemin, "Building an Orthonormal Basis, Revisited" (JCGT 6(1),
    // 2017). The obvious construction — cross the axis with world up — fails
    // when the axis IS up, which for a fountain is the only axis anybody uses.
    const float sign = std::copysign(1.0f, n.z);
    const float a = -1.0f / (sign + n.z);
    const float b = n.x * n.y * a;
    u.tangent = vec3{1.0f + sign * n.x * n.x * a, sign * b, -sign * n.x};
    u.bitangent = vec3{b, sign + n.y * n.y * a, -n.y};

    u.speed_min = e.speed_min;
    u.speed_span = e.speed_max - e.speed_min;

    u.gravity = e.gravity;
    u.inv_drag_h = 1.0f / (1.0f + e.drag * h);

    u.life_min = e.life_min;
    u.life_span = e.life_max - e.life_min;
    u.ground = e.ground;
    u.restitution = e.restitution;

    u.keep = 1.0f - e.friction;
    u.drag = e.drag;
    u.carry = w.carry;
    u.inv_rate = w.inv_rate;

    u.first = w.first;
    u.count = w.count;
    u.mask = capacity - 1u;
    u.seed = e.seed;

    u.capacity = capacity;
    return u;
}

// ===========================================================================
//  One thread
// ===========================================================================

particle spawn_particle(Uint32 serial, float age, const step_uniforms& u)
{
    particle_rng rng(serial, u.seed);

    // FOUR DRAWS, IN THIS ORDER, and the order is part of the specification: the
    // kernel draws them in the same sequence, so the k-th number means the same
    // thing on both processors.
    const float u_cos = rng.next();
    const float u_phi = rng.next();
    const float u_speed = rng.next();
    const float u_life = rng.next();

    // THE DIRECTION, uniform over the cap. `h` is the distance down from the pole:
    // 1 − cos θ for this sample, drawn uniformly in [0, 1 − cos θ_max).
    //
    // sin θ is then sqrt(h (2 − h)) — NOT sqrt(1 − cos²θ). The two are equal on
    // paper, and the second subtracts two numbers near 1 for exactly the samples
    // near the axis, where most of a narrow cone's samples are (LEARNINGS:
    // "cancellation against 1.0"). Written this way no subtraction ever meets 1.
    const float h = u_cos * u.one_minus_cos_cone;
    const float cos_t = 1.0f - h;
    const float sin_t = std::sqrt(h * (2.0f - h));
    const float phi = k_two_pi * u_phi;
    const float cx = sin_t * std::cos(phi);
    const float cy = sin_t * std::sin(phi);

    const vec3 dir = u.tangent * cx + u.bitangent * cy + u.axis * cos_t;
    const float speed = u.speed_min + u_speed * u.speed_span;

    particle p{};
    p.position = u.origin;
    p.velocity = dir * speed;
    p.life = u.life_min + u_life * u.life_span;
    p.serial = serial;

    // BORN IN THE PAST: advance by its own age in one step of that length, with
    // the implicit drag factor for THAT step — 1 / (1 + k·age), which is the one
    // division left in the kernel, because age varies per particle.
    integrate_particle(p, u, age, 1.0f / (1.0f + u.drag * age));
    p.age = age;

    // `previous` is the EMITTER, not the pre-aged position: at the start of this
    // step the particle was not yet born, and the nearest honest answer to
    // "where was it?" is where it came from. Interpolating from the old tenant
    // of the slot instead would draw a streak across the screen for one frame.
    p.previous = u.origin;
    return p;
}

void integrate_particle(particle& p, const step_uniforms& u, float dt, float inv_drag)
{
    // Velocity FIRST, then position with the new velocity — the order that
    // makes this semi-implicit (Lesson 1.3). The drag is implicit too: solving
    // v' = v + (g − k v') dt for v' gives (v + g dt) / (1 + k dt).
    p.velocity = (p.velocity + u.gravity * dt) * inv_drag;
    p.position = p.position + p.velocity * dt;

    // THE GROUND. A reflection of the NORMAL component only, and only when the
    // particle is moving down — a particle already on its way back up after a
    // bounce is below the plane for at most one step and must not be bounced a
    // second time, which would send it back down through the floor.
    if (p.position.y < u.ground && p.velocity.y < 0.0f)
    {
        // Put it back on the surface rather than leaving it below. A step that
        // overshoots by 3 cm is a step whose sparks sit 3 cm under the floor for
        // a frame, and depth testing makes that visible as a flicker.
        p.position.y = u.ground;
        p.velocity.y = -p.velocity.y * u.restitution;
        p.velocity.x = p.velocity.x * u.keep;
        p.velocity.z = p.velocity.z * u.keep;
    }
}

void step_slot(particle& p, Uint32 slot, const step_uniforms& u)
{
    // WHICH BIRTH, IF ANY, LANDS IN THIS SLOT? The window names serials first ..
    // first + count − 1, and serial s lives in slot s & mask. So this slot is
    // named iff its distance from `first`'s slot, going forward round the ring,
    // is below `count`. Unsigned subtraction wraps, which is exactly "going
    // forward round the ring".
    const Uint32 d = (slot - u.first) & u.mask;
    if (d < u.count)
    {
        p = spawn_particle(u.first + d, birth_age(d, u.carry, u.inv_rate, u.h), u);
        return;
    }

    // Dead particles stay dead until the ring comes round to them. They are not
    // moved, so their bytes are exactly the bytes they died with — which the
    // harness relies on when it compares whole buffers.
    if (!is_alive(p)) { return; }

    p.previous = p.position;
    integrate_particle(p, u, u.h, u.inv_drag_h);
    p.age = p.age + u.h;
}

// ===========================================================================
//  The CPU particle system
// ===========================================================================

Uint32 particle_capacity_for(Uint32 n)
{
    Uint32 c = 64u;
    while (c < n && c < 0x80000000u) { c <<= 1u; }
    return c;
}

particle_pool::particle_pool(Uint32 capacity)
    : particles_(particle_capacity_for(capacity))
{
}

step_uniforms particle_pool::step(const emitter_settings& e, float h)
{
    const Uint32 cap = capacity();
    const emission_window w = clock_.advance(e.rate, h, cap);
    const step_uniforms u = make_step_uniforms(e, w, h, cap);

    // The loop the GPU does not have. One iteration here is one THREAD there,
    // and nothing in the body reads another slot — which is the entire reason
    // the kernel can run them all at once.
    for (Uint32 i = 0; i < cap; ++i)
    {
        step_slot(particles_[i], i, u);
    }
    return u;
}

Uint32 particle_pool::alive_count() const
{
    Uint32 n = 0;
    for (const particle& p : particles_) { n += is_alive(p) ? 1u : 0u; }
    return n;
}

void particle_pool::reset()
{
    for (particle& p : particles_) { p = particle{}; }
    clock_.reset();
}

} // namespace engine
