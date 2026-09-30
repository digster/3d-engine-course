// shaders/particle.vert.hlsl — a billboard per living particle, and no vertex buffer.
//
// Lesson 6.18b. This vertex shader has no inputs. The draw is issued with six
// vertices per instance and one instance per LIVING particle — a count the
// compaction kernel wrote into the indirect-argument buffer, so the CPU that
// issues the draw never learns it. Each invocation therefore finds its data the
// way a compute thread does: by index, from storage buffers.
//
//     SV_InstanceID  ->  alive[instance]  ->  the particle's slot  ->  particles[slot]
//     SV_VertexID    ->  which corner of the quad
//
// Vertex-stage storage buffers are READ-ONLY in SDL_GPU (a graphics stage cannot
// write storage), and they sit after the sampled and storage textures in
// (t[n], space0) — this shader has neither, so the buffers are t0 and t1.

struct Particle
{
    float3 position;
    float age;
    float3 velocity;
    float life;
    float3 previous;
    uint serial;
};

StructuredBuffer<Particle> particles : register(t0, space0);
StructuredBuffer<uint> alive : register(t1, space0);

// engine::particle_draw_uniforms — five rows.
cbuffer Draw : register(b0, space1)
{
    float4x4 clip_from_world;
    float3 right;       // the camera's right, in world space, unit
    float size;         // a new particle's radius, metres
    float3 up;          // the camera's up, in world space, unit
    float alpha;        // Lesson 1.4's interpolation factor, [0, 1]
    float intensity;    // HDR brightness of a newborn spark
    uint seed;
    float pad0;
    float pad1;
};

struct Output
{
    float2 corner : TEXCOORD0;   // (-1..1, -1..1) across the quad
    float3 colour : TEXCOORD1;   // linear HDR radiance
    float4 position : SV_Position;
};

uint pcg_hash(uint input)
{
    const uint state = input * 747796405u + 2891336453u;
    const uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

Output main(uint vertex : SV_VertexID, uint instance : SV_InstanceID)
{
    const uint slot = alive[instance];
    const Particle p = particles[slot];

    // Two counter-clockwise triangles in the camera's (right, up) plane, so the
    // quad faces the camera by construction and survives the course's default
    // back-face cull (conventions §7). Six entries rather than an index buffer:
    // the table is the index buffer.
    const float2 corners[6] = {
        float2(-1.0f, -1.0f), float2(1.0f, -1.0f), float2(1.0f, 1.0f),
        float2(-1.0f, -1.0f), float2(1.0f, 1.0f),  float2(-1.0f, 1.0f)
    };
    const float2 c = corners[vertex];

    // WHERE, between the last two steps: Lesson 1.4's alpha, applied per
    // particle. `previous` is the position at the start of the last step (or the
    // emitter, for one born during it), so this never interpolates across two
    // different particles that happened to share a slot.
    const float3 centre = lerp(p.previous, p.position, alpha);

    // HOW OLD, as a fraction of its life — everything the particle LOOKS like is
    // derived from this and from its name, so none of it needs storage.
    const float t = saturate(p.age / p.life);
    const uint h = pcg_hash(p.serial ^ seed ^ 0x68E31DA4u);
    const float vary = 0.6f + 0.8f * float(h >> 8u) * (1.0f / 16777216.0f);

    // Sparks shrink and cool as they age: white-yellow at birth, deep orange at
    // death, and dimming on a square so the end is a fade rather than a pop.
    const float radius = size * vary * (1.0f - 0.6f * t);
    const float3 hot = float3(1.0f, 0.78f, 0.45f);
    const float3 cold = float3(0.9f, 0.22f, 0.04f);
    const float fade = (1.0f - t) * (1.0f - t);

    Output o;
    o.corner = c;
    o.colour = lerp(hot, cold, t) * (intensity * fade * vary);

    const float3 world = centre + (right * c.x + up * c.y) * radius;
    o.position = mul(clip_from_world, float4(world, 1.0f));
    return o;
}
