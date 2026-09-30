// shaders/particles_probe.comp.hlsl — Lesson 6.18b's instrument.
//
// Not part of the particle system. Like 4.6's uniform_probe and 4.7's
// depth_probe, it exists so that claims the lesson makes about the GPU are
// MEASURED on the GPU rather than asserted about it. One kernel, seven modes,
// chosen by a uniform:
//
//   0  LAYOUT      write known values into a Particle and into the tempting
//                  float3-float3-float struct; the harness reads the bytes back
//                  and finds where each value landed.
//   1  HASH        words[i] = pcg_hash(base + i): compared bit for bit with C++.
//   2  RACE        words[0] = words[0] + 1 from every thread — the lost update.
//   3  ATOMIC      the same count through InterlockedAdd.
//   4  TAIL        every thread of the grid writes, with no bounds check; the
//                  harness sizes the buffer with a guard zone and counts the
//                  writes that land past `count`.
//   5  UNIT        the bits of unit_float(pcg_hash(base + i)): exact on both sides?
//   6  SINE        the bits of frac(sin(base + i) * 43758.5453): the folklore hash,
//                  compared with the same expression evaluated by C++.

struct Particle
{
    float3 position;
    float age;
    float3 velocity;
    float life;
    float3 previous;
    uint serial;
};

// The struct §4 warns about: two float3s in a row. Under HLSL's own packing
// `velocity` is at byte 12; under the SPIR-V rules glslang uses it is at 16.
struct Naive
{
    float3 position;
    float3 velocity;
    float age;
};

cbuffer Probe : register(b0, space2)
{
    uint mode;
    uint count;
    uint base;
    uint pad0;
};

RWStructuredBuffer<uint> words : register(u0, space1);
RWStructuredBuffer<Particle> rows : register(u1, space1);
RWStructuredBuffer<Naive> naive : register(u2, space1);

uint pcg_hash(uint input)
{
    const uint state = input * 747796405u + 2891336453u;
    const uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float unit_float(uint bits)
{
    return float(bits >> 8u) * (1.0f / 16777216.0f);
}

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    const uint i = id.x;

    if (mode == 4u)
    {
        // THE TAIL, deliberately unguarded: this is the line every other kernel
        // in the engine prefixes with `if (i >= count) return;`.
        words[i] = 0xC0FFEEu;
        return;
    }

    if (i >= count)
    {
        return;
    }

    if (mode == 0u)
    {
        Particle p;
        p.position = float3(1.0f, 2.0f, 3.0f);
        p.age = 4.0f;
        p.velocity = float3(5.0f, 6.0f, 7.0f);
        p.life = 8.0f;
        p.previous = float3(9.0f, 10.0f, 11.0f);
        p.serial = 12u;
        rows[i] = p;

        Naive n;
        n.position = float3(1.0f, 2.0f, 3.0f);
        n.velocity = float3(4.0f, 5.0f, 6.0f);
        n.age = 7.0f;
        naive[i] = n;
    }
    else if (mode == 1u)
    {
        words[i] = pcg_hash(base + i);
    }
    else if (mode == 2u)
    {
        // A read, an add and a write — three steps another thread can land between.
        words[0] = words[0] + 1u;
    }
    else if (mode == 3u)
    {
        uint before;
        InterlockedAdd(words[0], 1u, before);
    }
    else if (mode == 5u)
    {
        words[i] = asuint(unit_float(pcg_hash(base + i)));
    }
    else if (mode == 6u)
    {
        words[i] = asuint(frac(sin(float(base + i)) * 43758.5453f));
    }
}
