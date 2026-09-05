// engine/src/gfx/debug_lines.cpp — the queue.
//
// Every function here does one of two things: turn a shape into line segments,
// or manage the lifetime of segments already queued. Nothing in this file knows
// what a pixel is, and that is checkable — the include list has no renderer
// header in it.

#include <engine/gfx/debug_lines.hpp>

#include <cmath>

namespace engine {

debug_lines::debug_lines(std::size_t capacity)
    : capacity_(capacity)
{
    // Reserved once, up front. A debug system that reallocates mid-frame is a
    // debug system that shows up in the profile of the thing you are profiling,
    // which is a particularly annoying way to be wrong.
    lines_.reserve(capacity_);
}

void debug_lines::push(const debug_line& l)
{
    // THE ONE PLACE THE BOUND IS CHECKED. Every queueing function below funnels
    // through here, so "what happens when the queue is full" has exactly one
    // answer and one line of code — and adding a primitive next month cannot
    // forget to ask.
    if (lines_.size() >= capacity_)
    {
        ++dropped_;
        return;
    }
    lines_.push_back(l);
}

void debug_lines::line(vec3 a, vec3 b, Uint32 colour, float seconds)
{
    push({a, b, colour, seconds});
}

void debug_lines::ray(vec3 origin, vec3 direction, Uint32 colour, float seconds)
{
    push({origin, origin + direction, colour, seconds});
}

void debug_lines::axes(const mat4& world_from_local, float length, float seconds)
{
    // The origin is a POSITION (w = 1) and the basis vectors are DIRECTIONS
    // (w = 0) — Lesson 2.7's distinction, and the reason a triad drawn with the
    // wrong w moves the arrows instead of rotating them. What comes back is the
    // matrix's first three columns, scaled: Lesson 2.5's "where the basis
    // vectors land", drawn.
    const vec3 origin = xyz(world_from_local * point(vec3{}));
    const vec3 x = xyz(world_from_local * direction(vec3{1.0f, 0.0f, 0.0f}));
    const vec3 y = xyz(world_from_local * direction(vec3{0.0f, 1.0f, 0.0f}));
    const vec3 z = xyz(world_from_local * direction(vec3{0.0f, 0.0f, 1.0f}));

    push({origin, origin + x * length, k_axis_x_colour, seconds});
    push({origin, origin + y * length, k_axis_y_colour, seconds});
    push({origin, origin + z * length, k_axis_z_colour, seconds});
}

namespace {

/// The twelve edges of a unit box, as index pairs into the eight corners.
///
/// A table rather than three nested loops, because the loop version is the one
/// people get wrong: it is easy to write something that emits each edge twice or
/// misses the four verticals. Eight corners are indexed by bit — bit 0 is x,
/// bit 1 is y, bit 2 is z — so two corners share an edge exactly when their
/// indices differ in one bit, which is what this table says.
constexpr int k_box_edges[12][2] = {
    {0, 1}, {2, 3}, {4, 5}, {6, 7},   // along x  (bit 0 differs)
    {0, 2}, {1, 3}, {4, 6}, {5, 7},   // along y  (bit 1 differs)
    {0, 4}, {1, 5}, {2, 6}, {3, 7},   // along z  (bit 2 differs)
};

}   // namespace

void debug_lines::box(vec3 centre, vec3 half_extent, Uint32 colour, float seconds)
{
    vec3 corner[8];
    for (int i = 0; i < 8; ++i)
    {
        corner[i] = {centre.x + ((i & 1) ? half_extent.x : -half_extent.x),
                     centre.y + ((i & 2) ? half_extent.y : -half_extent.y),
                     centre.z + ((i & 4) ? half_extent.z : -half_extent.z)};
    }
    for (const auto& e : k_box_edges)
    {
        push({corner[e[0]], corner[e[1]], colour, seconds});
    }
}

void debug_lines::box(const mat4& world_from_local, vec3 half_extent,
                      Uint32 colour, float seconds)
{
    // The corners are built in LOCAL space and transformed, which is what makes
    // this an oriented box rather than the axis-aligned bounds of one. Transform
    // the axis-aligned box's corners instead and you get a box that is correct;
    // transform its CENTRE and EXTENT and you get the classic bug — a box that
    // stays axis-aligned while the thing inside it rotates.
    vec3 corner[8];
    for (int i = 0; i < 8; ++i)
    {
        const vec3 local{(i & 1) ? half_extent.x : -half_extent.x,
                         (i & 2) ? half_extent.y : -half_extent.y,
                         (i & 4) ? half_extent.z : -half_extent.z};
        corner[i] = xyz(world_from_local * point(local));
    }
    for (const auto& e : k_box_edges)
    {
        push({corner[e[0]], corner[e[1]], colour, seconds});
    }
}

void debug_lines::sphere(vec3 centre, float radius, Uint32 colour,
                         float seconds, int segments)
{
    if (segments < 3) { segments = 3; }

    // THREE GREAT CIRCLES, NOT A TESSELLATED BALL. A sphere drawn as latitude
    // and longitude bands is hundreds of lines and reads as a blob; three
    // circles through the centre read as a sphere from every angle and cost
    // 3 x segments. This is the standard debug-draw sphere for that reason, and
    // it is worth noticing that the choice is about LEGIBILITY rather than cost.
    const float step = 6.28318530718f / static_cast<float>(segments);
    for (int i = 0; i < segments; ++i)
    {
        const float t0 = static_cast<float>(i) * step;
        const float t1 = static_cast<float>(i + 1) * step;
        const float c0 = std::cos(t0) * radius;
        const float s0 = std::sin(t0) * radius;
        const float c1 = std::cos(t1) * radius;
        const float s1 = std::sin(t1) * radius;

        push({{centre.x + c0, centre.y + s0, centre.z},
              {centre.x + c1, centre.y + s1, centre.z}, colour, seconds});   // xy
        push({{centre.x, centre.y + c0, centre.z + s0},
              {centre.x, centre.y + c1, centre.z + s1}, colour, seconds});   // yz
        push({{centre.x + s0, centre.y, centre.z + c0},
              {centre.x + s1, centre.y, centre.z + c1}, colour, seconds});   // zx
    }
}

void debug_lines::wire_mesh(std::span<const vec3> vertices,
                            std::span<const std::uint16_t> indices,
                            const mat4& world_from_local, Uint32 colour,
                            float seconds)
{
    for (std::size_t t = 0; t + 2 < indices.size(); t += 3)
    {
        const std::size_t tri[3] = {indices[t], indices[t + 1], indices[t + 2]};
        for (int e = 0; e < 3; ++e)
        {
            const std::size_t ia = tri[e];
            const std::size_t ib = tri[(e + 1) % 3];

            // An index past the end of the vertex array is a malformed mesh, and
            // a debug drawer is exactly the code most likely to be handed one —
            // it is what you reach for when the mesh looks wrong. Skip, do not
            // fault: the reason you are here is to SEE the problem.
            if (ia >= vertices.size() || ib >= vertices.size()) { continue; }

            push({xyz(world_from_local * point(vertices[ia])),
                  xyz(world_from_local * point(vertices[ib])), colour, seconds});
        }
    }
}

void debug_lines::advance(float dt)
{
    // A compacting sweep rather than erase-in-a-loop: one pass, no repeated
    // shifting, and the surviving order is unchanged. `out` never runs ahead of
    // `i`, so the copy is always backwards into space already read.
    std::size_t out = 0;
    for (std::size_t i = 0; i < lines_.size(); ++i)
    {
        debug_line entry = lines_[i];

        // THE TEST COMES BEFORE THE SUBTRACTION, and the header says why: a line
        // whose clock has already reached zero has had its last frame no matter
        // what dt is, including dt == 0. Subtract first and a paused clock keeps
        // single-frame lines forever.
        if (entry.remaining <= 0.0f) { continue; }

        entry.remaining -= dt;
        lines_[out++] = entry;
    }
    lines_.resize(out);
}

void debug_lines::clear()
{
    lines_.clear();
    dropped_ = 0;
}

}   // namespace engine
