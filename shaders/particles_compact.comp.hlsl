// shaders/particles_compact.comp.hlsl — list the living, and count them, on the GPU.
//
// Lesson 6.18b. The step kernel leaves a pool in which some slots hold a live
// particle and the rest hold a dead one, in no useful order. The draw wants only
// the living, and it wants to know how many there are — a number that exists
// nowhere but in this buffer, and that the CPU never sees. So each thread whose
// slot is alive takes the next free place in a list and writes its slot there,
// and the list's length becomes the draw's instance count.
//
// TAKING THE NEXT FREE PLACE IS THE WHOLE PROBLEM. `args[1] = args[1] + 1` is a
// read, an add and a write, and two threads that read the same value both write
// the same result: one increment is lost, and two particles land in one place.
// §9 of the lesson runs exactly that line and measures how many are lost.
// InterlockedAdd makes the three steps one indivisible operation and returns the
// value it replaced, which is this thread's place in the list.
//
// The ORDER of the list is therefore whatever order the hardware happened to
// serve the atomics in, and it changes from frame to frame. The draw does not
// care — its blend is additive, and addition does not depend on order — except
// in the last bit of a half-float, which §9 also measures.

struct Particle
{
    float3 position;
    float age;
    float3 velocity;
    float life;
    float3 previous;
    uint serial;
};

// READ-ONLY this time: the compaction only looks. t0 in space0, bound with
// SDL_BindGPUComputeStorageBuffers, and the buffer needs the
// COMPUTE_STORAGE_READ usage bit as well as the _WRITE one the step kernel uses.
StructuredBuffer<Particle> particles : register(t0, space0);

// Read-write, bound at SDL_BeginGPUComputePass in THIS order: slot 0, slot 1.
RWStructuredBuffer<uint> args : register(u0, space1);
RWStructuredBuffer<uint> alive : register(u1, space1);

cbuffer Compact : register(b0, space2)
{
    uint capacity;
    uint pad0;
    uint pad1;
    uint pad2;
};

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    const uint slot = id.x;
    if (slot >= capacity)
    {
        return;
    }

    const Particle p = particles[slot];
    if (!(p.age < p.life))
    {
        return;
    }

    uint place;
    InterlockedAdd(args[1], 1u, place);
    alive[place] = slot;
}
