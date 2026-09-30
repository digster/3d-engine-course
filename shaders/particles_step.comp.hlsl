// shaders/particles_step.comp.hlsl — one fixed step of the particle system.
//
// Lesson 6.18b. One THREAD per slot of the pool, and every thread does exactly
// what `engine::step_slot` in engine/src/gfx/particles.cpp does — the two are
// written in the same order of operations, and the lesson's harness compares
// their output particle by particle. Read them side by side.
//
// Nothing here reads a slot other than its own, which is the property that lets
// sixty-five thousand of them run at once with no synchronisation at all. The
// kernel that DOES need synchronisation — counting the survivors — is a
// different kernel, in a different compute pass, for a reason SDL_gpu.h states
// in capitals (particles_compact.comp.hlsl).

// ---------------------------------------------------------------------------
// The record: THREE 16-BYTE ROWS, and nothing else is safe.
//
// `engine::particle` is the C++ half and it static_asserts every offset. The
// rows exist because a float3 followed by a float3 is 12 bytes apart under
// HLSL's structured-buffer rules and 16 apart under the SPIR-V rules glslang
// applies — measured in §4 of the lesson — and a float3 followed by ONE SCALAR
// is 16 bytes under both.
// ---------------------------------------------------------------------------
struct Particle
{
    float3 position;
    float age;
    float3 velocity;
    float life;
    float3 previous;
    uint serial;
};

// ---------------------------------------------------------------------------
// The uniform block — engine::step_uniforms, nine rows. b0 in SPACE2: a compute
// shader's uniforms live where a fragment shader keeps its textures
// (SDL_gpu.h, SDL_CreateGPUComputePipeline).
// ---------------------------------------------------------------------------
cbuffer Step : register(b0, space2)
{
    float3 origin;
    float h;
    float3 axis;
    float one_minus_cos_cone;
    float3 tangent;
    float speed_min;
    float3 bitangent;
    float speed_span;
    float3 gravity;
    float inv_drag_h;
    float life_min;
    float life_span;
    float ground;
    float restitution;
    float keep;
    float drag;
    float carry;
    float inv_rate;
    uint first;
    uint count;
    uint mask;
    uint seed;
    uint capacity;
    uint pad0;
    uint pad1;
    uint pad2;
};

// Read AND written by this kernel, so it is a read-write storage buffer: u0 in
// space1, and bound by SDL_BeginGPUComputePass rather than by a bind call.
RWStructuredBuffer<Particle> particles : register(u0, space1);

// ---------------------------------------------------------------------------
// Randomness — engine::pcg_hash and engine::unit_float, bit for bit.
// Jarzynski and Olano, "Hash Functions for GPU Rendering", JCGT 9(3), 2020.
// ---------------------------------------------------------------------------
uint pcg_hash(uint input)
{
    const uint state = input * 747796405u + 2891336453u;
    const uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float unit_float(uint bits)
{
    // 24 bits, then a power of two: no rounding anywhere, on either processor.
    return float(bits >> 8u) * (1.0f / 16777216.0f);
}

// ---------------------------------------------------------------------------
// engine::integrate_particle
// ---------------------------------------------------------------------------
void integrate(inout Particle p, float dt, float inv_drag)
{
    p.velocity = (p.velocity + gravity * dt) * inv_drag;
    p.position = p.position + p.velocity * dt;

    if (p.position.y < ground && p.velocity.y < 0.0f)
    {
        p.position.y = ground;
        p.velocity.y = -p.velocity.y * restitution;
        p.velocity.x = p.velocity.x * keep;
        p.velocity.z = p.velocity.z * keep;
    }
}

// ---------------------------------------------------------------------------
// engine::spawn_particle
// ---------------------------------------------------------------------------
Particle spawn(uint serial, float age)
{
    uint state = pcg_hash(serial ^ seed);

    // Four draws, in the C++ order.
    state = pcg_hash(state);
    const float u_cos = unit_float(state);
    state = pcg_hash(state);
    const float u_phi = unit_float(state);
    state = pcg_hash(state);
    const float u_speed = unit_float(state);
    state = pcg_hash(state);
    const float u_life = unit_float(state);

    // Uniform over the cap; sin from h (2 - h), never from 1 - cos^2.
    const float hh = u_cos * one_minus_cos_cone;
    const float cos_t = 1.0f - hh;
    const float sin_t = sqrt(hh * (2.0f - hh));
    const float phi = 6.28318530717958647692f * u_phi;
    const float cx = sin_t * cos(phi);
    const float cy = sin_t * sin(phi);

    const float3 dir = tangent * cx + bitangent * cy + axis * cos_t;
    const float speed = speed_min + u_speed * speed_span;

    Particle p;
    p.position = origin;
    p.velocity = dir * speed;
    p.life = life_min + u_life * life_span;
    p.serial = serial;
    p.age = 0.0f;
    p.previous = origin;

    integrate(p, age, 1.0f / (1.0f + drag * age));
    p.age = age;
    p.previous = origin;
    return p;
}

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    const uint slot = id.x;

    // THE TAIL. The grid is sized in whole groups of 64, and a pool of any size
    // that is not a multiple of 64 leaves threads with no slot. Without this line
    // they read and write past the end of the buffer — §3 of the lesson measures
    // where those writes land. (Every pool this engine creates is a power of two
    // of at least 64, so today the tail is empty; the check costs one compare and
    // it is the one line nobody may delete.)
    if (slot >= capacity)
    {
        return;
    }

    Particle p = particles[slot];

    const uint d = (slot - first) & mask;
    if (d < count)
    {
        const float age = h - (float(d) + (1.0f - carry)) * inv_rate;
        particles[slot] = spawn(first + d, age);
        return;
    }

    if (!(p.age < p.life))
    {
        return;   // dead, and left byte-for-byte as it died
    }

    p.previous = p.position;
    integrate(p, h, inv_drag_h);
    p.age = p.age + h;
    particles[slot] = p;
}
