// engine/src/phys/shape.cpp — the primitives, and where they sit in the world.

#include <engine/phys/shape.hpp>

#include <engine/core/assert.hpp>

#include <cmath>

namespace engine::phys
{

// ---------------------------------------------------------------------------
// The shape itself
// ---------------------------------------------------------------------------

const char* name_of(shape_kind kind)
{
    switch (kind)
    {
        case shape_kind::sphere:  return "sphere";
        case shape_kind::box:     return "box";
        case shape_kind::capsule: return "capsule";
    }
    return "?";
}

shape sphere_shape(float radius)
{
    shape s;
    s.kind = shape_kind::sphere;
    s.radius = radius;
    // The box members stay at their defaults rather than being zeroed. A shape
    // is read through its `kind` and a zero there would look meaningful to a
    // debugger; the default at least reads as "not this one".
    return s;
}

shape box_shape(vec3 half_extents)
{
    shape s;
    s.kind = shape_kind::box;
    s.half_extents = half_extents;
    return s;
}

shape cube_shape(float side)
{
    return box_shape(vec3{side * 0.5f, side * 0.5f, side * 0.5f});
}

shape capsule_shape(float radius, float half_height)
{
    shape s;
    s.kind = shape_kind::capsule;
    s.radius = radius;
    s.half_height = half_height;
    return s;
}

float volume_of(const shape& s)
{
    switch (s.kind)
    {
        case shape_kind::sphere:
            return (4.0f / 3.0f) * 3.14159265358979f * s.radius * s.radius * s.radius;
        case shape_kind::box:
            // Half extents, so each side is doubled: 2h_x * 2h_y * 2h_z.
            return 8.0f * s.half_extents.x * s.half_extents.y * s.half_extents.z;
        case shape_kind::capsule:
            // Cylinder plus a whole sphere, because the two hemispherical caps
            // together ARE one. `half_height == 0` therefore returns the sphere
            // volume exactly, with no branch — the reduction the header claims.
            return 3.14159265358979f * s.radius * s.radius * (2.0f * s.half_height)
                 + (4.0f / 3.0f) * 3.14159265358979f * s.radius * s.radius * s.radius;
    }
    return 0.0f;
}

float bounding_radius(const shape& s)
{
    switch (s.kind)
    {
        case shape_kind::sphere:  return s.radius;
        case shape_kind::box:     return length(s.half_extents);   // the half-diagonal
        case shape_kind::capsule: return s.half_height + s.radius; // along the spine
    }
    return 0.0f;
}

mat3 inertia_of(const shape& s, float mass)
{
    // The dispatch the header argues for: one place where a primitive's geometry
    // is turned into what it weighs, so that a body's tensor and a body's
    // collider cannot describe two different objects.
    switch (s.kind)
    {
        case shape_kind::sphere: return inertia_solid_sphere(mass, s.radius);
        case shape_kind::box:    return inertia_solid_box(mass, s.half_extents);
        case shape_kind::capsule:
            // `inertia_capsule` has been here since 8.3, taking the FULL
            // cylinder height while a `shape` stores the half. The conversion is
            // one multiply and it lives here, at the single point where the two
            // conventions meet, rather than at every call site.
            return inertia_capsule(mass, s.radius, 2.0f * s.half_height);
    }
    return mat3{vec3{}, vec3{}, vec3{}};
}

// ---------------------------------------------------------------------------
// Placement
// ---------------------------------------------------------------------------

void obb::corners(vec3 out[8]) const
{
    // Same bit order as `aabb::corners`: bit 0 is x, bit 1 is y, bit 2 is z, and
    // a set bit means the `+` side. The difference is that "the + side" is along
    // the box's OWN axis rather than the world's, which is the whole of what an
    // OBB is.
    for (int i = 0; i < 8; ++i)
    {
        const float sx = (i & 1) ? half_extents.x : -half_extents.x;
        const float sy = (i & 2) ? half_extents.y : -half_extents.y;
        const float sz = (i & 4) ? half_extents.z : -half_extents.z;
        out[i] = centre + axes.c0 * sx + axes.c1 * sy + axes.c2 * sz;
    }
}

obb world_obb(const shape& s, vec3 centre, quat orientation)
{
    ENGINE_ASSERT(s.kind == shape_kind::box);

    obb box;
    box.centre = centre;
    // ONE quaternion-to-matrix conversion, here, at placement time — 8.3 §12's
    // finding turned into an API shape. Everything downstream dots against these
    // columns and none of it needs to know a quaternion was involved.
    box.axes = mat3_from_quat(orientation);
    box.half_extents = s.half_extents;
    return box;
}

engine::sphere world_sphere(const shape& s, vec3 centre)
{
    ENGINE_ASSERT(s.kind == shape_kind::sphere);
    return engine::sphere{centre, s.radius};
}

aabb bounds_of(const obb& box)
{
    // The projected-radius formula run three times, once per world axis. The
    // component of the enclosing box's half extent along world axis `i` is
    //
    //     Σⱼ hⱼ |uⱼ · eᵢ|
    //
    // and `uⱼ · eᵢ` is simply component `i` of column `j`, so this is |axes|
    // applied to the half extents with every sign stripped.
    const vec3& h = box.half_extents;
    const vec3 r{
        std::fabs(box.axes.c0.x) * h.x + std::fabs(box.axes.c1.x) * h.y +
            std::fabs(box.axes.c2.x) * h.z,
        std::fabs(box.axes.c0.y) * h.x + std::fabs(box.axes.c1.y) * h.y +
            std::fabs(box.axes.c2.y) * h.z,
        std::fabs(box.axes.c0.z) * h.x + std::fabs(box.axes.c1.z) * h.y +
            std::fabs(box.axes.c2.z) * h.z,
    };

    aabb out;
    out.min = box.centre - r;
    out.max = box.centre + r;
    return out;
}

aabb bounds_of(const shape& s, vec3 centre, quat orientation)
{
    if (s.kind == shape_kind::sphere)
    {
        // The orientation is ignored, exactly — not approximately, not "usually
        // safely". This one line is the entire practical argument for spheres in
        // a broadphase: a tumbling body's sphere bounds are a translation of the
        // same sphere forever, while its box bounds are rebuilt every frame.
        const vec3 r{s.radius, s.radius, s.radius};
        aabb out;
        out.min = centre - r;
        out.max = centre + r;
        return out;
    }
    if (s.kind == shape_kind::capsule)
    {
        return bounds_of(world_capsule(s, centre, orientation));
    }
    return bounds_of(world_obb(s, centre, orientation));
}

obb as_obb(const aabb& box)
{
    obb out;
    out.centre = box.centre();
    out.axes = mat3::identity();
    out.half_extents = box.extent() * 0.5f;
    return out;
}

// ---------------------------------------------------------------------------
// Lesson 8.5: the capsule and the hull, placed
// ---------------------------------------------------------------------------

capsule world_capsule(const shape& s, vec3 centre, quat orientation)
{
    ENGINE_ASSERT(s.kind == shape_kind::capsule);

    capsule c;
    c.centre = centre;
    // The spine is body +y, so its world direction is the SECOND column of the
    // rotation matrix — 2.5's reading of a matrix as "where the basis vectors
    // land", used to skip building the other two columns' worth of meaning.
    c.axis = rotate(orientation, vec3{0.0f, 1.0f, 0.0f});
    c.half_height = s.half_height;
    c.radius = s.radius;
    return c;
}

hull world_hull(std::span<const vec3> points, vec3 centre, quat orientation)
{
    hull h;
    h.centre = centre;
    h.axes = mat3_from_quat(orientation);
    h.points = points;
    return h;
}

vec3 support(const capsule& c, vec3 d)
{
    return c.centre + support_local(c, d);
}

vec3 support(const hull& h, vec3 d)
{
    return h.centre + support_local(h, d);
}

aabb bounds_of(const capsule& c)
{
    // The Minkowski sum again: bounds of the segment, grown by the radius. Two
    // endpoints, a min and a max, and one uniform inflation.
    const vec3 p0 = c.end(-1);
    const vec3 p1 = c.end(+1);
    const vec3 r{c.radius, c.radius, c.radius};

    aabb out;
    out.min = vec3{std::fmin(p0.x, p1.x), std::fmin(p0.y, p1.y), std::fmin(p0.z, p1.z)} - r;
    out.max = vec3{std::fmax(p0.x, p1.x), std::fmax(p0.y, p1.y), std::fmax(p0.z, p1.z)} + r;
    return out;
}

aabb bounds_of(const hull& h)
{
    aabb out;
    if (h.points.empty()) { return out; }   // the default aabb is EMPTY, per 6.8

    for (const vec3& p : h.points)
    {
        const vec3 w = h.centre + h.axes * p;
        out.min = vec3{std::fmin(out.min.x, w.x), std::fmin(out.min.y, w.y),
                       std::fmin(out.min.z, w.z)};
        out.max = vec3{std::fmax(out.max.x, w.x), std::fmax(out.max.y, w.y),
                       std::fmax(out.max.z, w.z)};
    }
    return out;
}

// ---------------------------------------------------------------------------
// Support
// ---------------------------------------------------------------------------

vec3 support_local(const obb& box, vec3 d)
{
    // Three sign tests. A corner maximises `dot(p, d)` when each of its three
    // choices independently maximises its own term, because the terms do not
    // interact — the same separability that makes `closest_point` a clamp.
    //
    // `>= 0` rather than `> 0` so that a zero component picks the `+` side
    // deterministically; a tie between two corners is a tie in the value, so
    // either is correct and only reproducibility is at stake.
    //
    // 8.5: the centre never enters. The result is at most the half-diagonal from
    // the origin whatever the box's world position, so the arithmetic is done on
    // crate-sized numbers and stays exact where 8.4 §11's version could not.
    const float sx = (dot(box.axes.c0, d) >= 0.0f) ? box.half_extents.x : -box.half_extents.x;
    const float sy = (dot(box.axes.c1, d) >= 0.0f) ? box.half_extents.y : -box.half_extents.y;
    const float sz = (dot(box.axes.c2, d) >= 0.0f) ? box.half_extents.z : -box.half_extents.z;
    return box.axes.c0 * sx + box.axes.c1 * sy + box.axes.c2 * sz;
}

vec3 support_local(const engine::sphere& s, vec3 d)
{
    const float len2 = length_squared(d);
    if (len2 <= 0.0f) { return vec3{}; }
    return d * (s.radius / std::sqrt(len2));
}

vec3 support_local(const capsule& c, vec3 d)
{
    // The Minkowski sum, term by term, exactly as the header writes it.
    //
    //   the SEGMENT's support:  whichever end leans into `d`.
    //   the BALL's support:     `radius * normalise(d)`.
    //
    // Neither term consults the other, because `max(x + y)` over independent
    // choices IS `max(x) + max(y)`. Set `half_height` to zero and the first term
    // vanishes and this is `support_local(sphere)`; set `radius` to zero and it
    // is a segment. One function, three shapes.
    const vec3 spine = c.axis * ((dot(c.axis, d) >= 0.0f) ? c.half_height : -c.half_height);

    const float len2 = length_squared(d);
    if (len2 <= 0.0f) { return spine; }
    return spine + d * (c.radius / std::sqrt(len2));
}

vec3 support_local(const hull& h, vec3 d)
{
    if (h.points.empty()) { return vec3{}; }

    // ONE change of basis, not one per vertex. `dot(axes * p, d) == dot(p,
    // transpose(axes) * d)` because the axes are orthonormal and a rotation is
    // its own inverse transposed (2.5). So carry the DIRECTION into body axes
    // once, scan, and carry the single winner back out — n dot products instead
    // of n matrix-vector products.
    const vec3 dl = transpose(h.axes) * d;

    const vec3* best = &h.points[0];
    float best_dot = dot(*best, dl);
    for (const vec3& p : h.points)
    {
        const float v = dot(p, dl);
        if (v > best_dot) { best_dot = v; best = &p; }
    }
    return h.axes * *best;
}

vec3 support(const obb& box, vec3 d)
{
    return box.centre + support_local(box, d);
}

vec3 support(const engine::sphere& s, vec3 d)
{
    return s.centre + support_local(s, d);
}

} // namespace engine::phys
