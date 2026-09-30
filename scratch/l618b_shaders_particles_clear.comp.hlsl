// shaders/particles_clear.comp.hlsl — reset the draw's arguments before they are counted.
//
// Lesson 6.18b. One thread, four stores. This buffer is an
// `SDL_GPUIndirectDrawCommand` — { num_vertices, num_instances, first_vertex,
// first_instance } — and the compaction kernel ADDS to its second word, once per
// surviving particle. Something must set that word to zero first, and it cannot
// be the compaction kernel itself: which of its sixty-five thousand threads
// would do it, and how would the others know it had happened? Only the end of a
// compute pass orders one set of writes before another (SDL_gpu.h,
// SDL_BeginGPUComputePass), so the reset is its own kernel in its own pass.
//
// Six vertices: two triangles per billboard, generated from SV_VertexID in
// particle.vert.hlsl with no vertex buffer at all. first_vertex and
// first_instance are ZERO, and they must be: SDL_gpu.h warns that the
// built-in vertex and instance IDs are not portable otherwise.

RWStructuredBuffer<uint> args : register(u0, space1);

[numthreads(1, 1, 1)]
void main()
{
    args[0] = 6u;   // num_vertices
    args[1] = 0u;   // num_instances — the count, about to be accumulated
    args[2] = 0u;   // first_vertex
    args[3] = 0u;   // first_instance
}
