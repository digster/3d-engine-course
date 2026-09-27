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

// ---------------------------------------------------------------------------
// Support faces — Lesson 8.7
// ---------------------------------------------------------------------------

contact_face support_face(const obb& box, vec3 d)
{
    contact_face f;

    // WHICH FACE IS AN ARGMAX OVER THREE NUMBERS, and that is a strictly
    // better-behaved question than "is this vertex on the supporting plane".
    // `support_local` answers three independent sign tests because a corner's
    // three choices do not interact; a FACE is the single axis the direction
    // leans into hardest, so the three terms have to be compared against each
    // other and only the winner survives.
    int best = 0;
    float best_abs = -1.0f;
    float sign = 1.0f;
    for (int i = 0; i < 3; ++i)
    {
        const float p = dot(box.axis(i), d);
        const float a = std::fabs(p);
        if (a > best_abs)
        {
            best_abs = a;
            best = i;
            sign = (p >= 0.0f) ? 1.0f : -1.0f;
        }
    }

    // `>` rather than `>=` in the loop, so a direction exactly between two faces
    // keeps the LOWER axis index. Deterministic, and that is all that is at
    // stake: the two faces are equally good answers to the query, and 8.7 §4
    // measures what the arbitrariness costs a manifold (nothing, because the
    // clipper rejects whatever is not actually penetrating) against what it
    // costs a single witness point (everything).
    const int j = (best + 1) % 3;
    const int k = (best + 2) % 3;

    const vec3 n = box.axis(best) * sign;
    const vec3 centre = n * box.half(best);
    const vec3 uj = box.axis(j) * box.half(j);
    const vec3 uk = box.axis(k) * box.half(k);

    // WINDING. `(best, j, k)` is a cyclic permutation of a right-handed frame,
    // so `axis(j) x axis(k) = axis(best)`. Walking `-e0-e1 -> +e0-e1 -> +e0+e1
    // -> -e0+e1` therefore turns counter-clockwise about `+axis(best)`: the
    // first two edges are `2*e0` then `2*e1`, whose cross product is
    // `4*hj*hk*axis(best)`. On the NEGATIVE face the outward normal is
    // `-axis(best)`, so the two in-plane vectors swap and the same walk turns
    // counter-clockwise about the normal that is actually outward.
    const vec3 e0 = (sign >= 0.0f) ? uj : uk;
    const vec3 e1 = (sign >= 0.0f) ? uk : uj;

    f.normal = n;
    f.count = 4;
    f.v[0] = centre - e0 - e1;
    f.v[1] = centre + e0 - e1;
    f.v[2] = centre + e0 + e1;
    f.v[3] = centre - e0 + e1;
    f.feature = static_cast<std::uint16_t>(2 * best + (sign < 0.0f ? 1 : 0));

    // THE CORNER INDICES, IN `obb::corners`' ORDER — bit `i` set means the `+`
    // side of axis `i`. The face's own axis contributes a fixed bit; the other
    // two vary with the walk above, and the `(sign >= 0)` swap has to be undone
    // here or the ids would follow the winding rather than the geometry.
    const int bit_i = (sign >= 0.0f) ? (1 << best) : 0;
    const int bit_j = 1 << ((sign >= 0.0f) ? j : k);
    const int bit_k = 1 << ((sign >= 0.0f) ? k : j);
    f.id[0] = static_cast<std::uint8_t>(bit_i);
    f.id[1] = static_cast<std::uint8_t>(bit_i | bit_j);
    f.id[2] = static_cast<std::uint8_t>(bit_i | bit_j | bit_k);
    f.id[3] = static_cast<std::uint8_t>(bit_i | bit_k);
    return f;
}

contact_face support_face(const engine::sphere& s, vec3 d)
{
    contact_face f;
    f.normal = normalised_or(d, vec3{0.0f, 1.0f, 0.0f});
    f.count = 1;
    f.v[0] = f.normal * s.radius;
    f.id[0] = 0;
    f.feature = 0;
    return f;
}

contact_face support_face(const capsule& c, vec3 d)
{
    contact_face f;

    const vec3 dn = normalised_or(d, vec3{0.0f, 1.0f, 0.0f});
    const float along = dot(c.axis, dn);
    const vec3 perp = dn - c.axis * along;
    const float p2 = length_squared(perp);

    // A CAPSULE HAS EXACTLY ONE FLAT FEATURE and it is one-dimensional: the line
    // where the cylindrical side meets the supporting plane, which exists only
    // when the query direction is perpendicular to the spine. Everything else —
    // the two caps, and every shoulder between them — is spherical, and a sphere
    // has no flat feature at all.
    //
    // So the tolerance is on `along`, and it is the sine of the angle off
    // perpendicular. Inside it the answer is the segment; outside it, one point.
    // Widen it and a capsule leaning against a wall reports a two-point contact
    // whose second point is centimetres off the surface; narrow it and an
    // upright capsule standing against a wall flickers between one point and two
    // as it settles.
    if (along * along <= k_face_gather_sin * k_face_gather_sin && p2 > 0.0f)
    {
        const vec3 radial = perp * (c.radius / std::sqrt(p2));
        f.normal = perp * (1.0f / std::sqrt(p2));
        f.count = 2;
        f.v[0] = c.axis * (-c.half_height) + radial;
        f.v[1] = c.axis * (c.half_height) + radial;
        f.id[0] = 1;
        f.id[1] = 0;
        f.feature = 2;
        return f;
    }

    const bool positive = (along >= 0.0f);
    f.normal = dn;
    f.count = 1;
    f.v[0] = c.axis * (positive ? c.half_height : -c.half_height) + dn * c.radius;
    f.id[0] = static_cast<std::uint8_t>(positive ? 0 : 1);
    f.feature = static_cast<std::uint16_t>(positive ? 0 : 1);
    return f;
}

namespace
{

/// FNV-1a, folded to sixteen bits. The identity of a hull face.
///
/// A hull has no face list — it is a point cloud, by `hull`'s own design — so
/// the only thing that names a face is the SET of vertices on its supporting
/// plane. Those indices arrive in ascending order because the gather scans in
/// index order, so the hash is a function of the face and not of the query
/// direction, which is the property persistence needs.
///
/// **Sixteen bits means collisions**, and the consequence is bounded and worth
/// stating: two different faces of the same hull that hash alike would let a
/// warm-start impulse from one land on the other for a single frame, after which
/// the solver corrects it. A hull would need on the order of three hundred faces
/// before the birthday bound makes one likely, and a collision costs one frame
/// of a slightly wrong initial guess.
std::uint16_t hash_face(const int* indices, int count)
{
    std::uint32_t h = 2166136261u;
    for (int i = 0; i < count; ++i)
    {
        const std::uint32_t x = static_cast<std::uint32_t>(indices[i]);
        for (int byte = 0; byte < 4; ++byte)
        {
            h ^= (x >> (8 * byte)) & 0xffu;
            h *= 16777619u;
        }
    }
    return static_cast<std::uint16_t>((h >> 16) ^ (h & 0xffffu));
}

} // namespace

contact_face support_face(const hull& h, vec3 d, float gather_sin)
{
    contact_face f;
    if (h.points.empty()) { return f; }

    // ONE change of basis, as `support_local(hull)` does and for the same
    // reason: the axes are orthonormal, so carrying the direction into body
    // space costs one matrix-vector product and saves n of them.
    const vec3 dl = normalised_or(transpose(h.axes) * d, vec3{0.0f, 1.0f, 0.0f});

    const int n = static_cast<int>(h.points.size());
    float best = -3.4028235e38f;
    float extent2 = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float v = dot(h.points[static_cast<std::size_t>(i)], dl);
        if (v > best) { best = v; }
        const float m2 = length_squared(h.points[static_cast<std::size_t>(i)]);
        if (m2 > extent2) { extent2 = m2; }
    }

    // THE GATHER, AND WHY ITS WIDTH IS AN ANGLE RATHER THAN A DISTANCE. A vertex
    // `w` metres across a face sits `w * sin(theta)` below the supporting plane
    // when the query direction is `theta` off the face normal. So the tolerance
    // that admits a whole face is `sin(theta) * extent`, and `extent` is the
    // shape's own size. A constant in metres here would gather half the hull on
    // a doorknob and one vertex on a car.
    const float eps = gather_sin * std::sqrt(extent2);

    int gathered[k_max_gathered_vertices];
    int count = 0;
    int on_plane = 0;
    for (int i = 0; i < n; ++i)
    {
        if (dot(h.points[static_cast<std::size_t>(i)], dl) >= best - eps)
        {
            ++on_plane;
            if (count < k_max_gathered_vertices) { gathered[count++] = i; }
        }
    }
    // COUNTING PAST THE CAPACITY IS THE POINT. Setting `truncated` on
    // `count == capacity` would also fire on a face with exactly 32 vertices and
    // nothing lost, and a flag that cries wolf is a flag a measurement cannot
    // use. The scan runs to the end and compares.
    if (on_plane > count) { f.truncated = true; }

    f.normal = h.axes * dl;
    f.feature = hash_face(gathered, count);

    if (count <= 2)
    {
        f.count = count;
        for (int i = 0; i < count; ++i)
        {
            f.v[i] = h.axes * h.points[static_cast<std::size_t>(gathered[i])];
            f.id[i] = static_cast<std::uint8_t>(gathered[i] & 0xff);
        }
        return f;
    }

    // ---- wind them counter-clockwise about the normal ----------------------
    //
    // The gathered vertices are in index order, which is the order the artist's
    // exporter happened to write them in and has nothing to do with the face.
    // A polygon whose vertices are not in order around its own boundary is not a
    // polygon: its edges cross, `cross(edge, normal)` points in scattered
    // directions, and 8.7's clipper rejects everything.
    //
    // So: build a frame in the face's plane and sort by angle. `u` points at the
    // LOWEST-INDEXED gathered vertex rather than at an arbitrary one, which is
    // what makes local index 0 the same vertex on every frame — and local
    // indices are half of every contact id.
    vec3 centroid{};
    for (int i = 0; i < count; ++i)
    {
        centroid += h.points[static_cast<std::size_t>(gathered[i])];
    }
    centroid = centroid * (1.0f / static_cast<float>(count));

    const vec3 first = h.points[static_cast<std::size_t>(gathered[0])] - centroid;
    vec3 u = first - dl * dot(first, dl);
    if (length_squared(u) <= 1e-20f)
    {
        // The lowest-indexed vertex sits on the centroid, which means the face is
        // degenerate. Any in-plane direction will do; take the one the standard
        // trick produces from the smallest component of `dl`.
        const vec3 helper = (std::fabs(dl.x) < 0.9f) ? vec3{1.0f, 0.0f, 0.0f} : vec3{0.0f, 1.0f, 0.0f};
        u = helper - dl * dot(helper, dl);
    }
    u = normalised_or(u, vec3{1.0f, 0.0f, 0.0f});
    const vec3 w = cross(dl, u);   // (u, w, dl) is right-handed: u x w = dl.

    float angle[k_max_gathered_vertices];
    for (int i = 0; i < count; ++i)
    {
        const vec3 r = h.points[static_cast<std::size_t>(gathered[i])] - centroid;
        angle[i] = std::atan2(dot(r, w), dot(r, u));
    }

    // Insertion sort. The gathered set is a FACE — three to six vertices on any
    // collider a person would author — so this is the fastest sort there is at
    // this size, and it is stable, which matters because two coincident vertices
    // must not swap places between frames.
    for (int i = 1; i < count; ++i)
    {
        const float a = angle[i];
        const int g = gathered[i];
        int j = i - 1;
        while (j >= 0 && angle[j] > a)
        {
            angle[j + 1] = angle[j];
            gathered[j + 1] = gathered[j];
            --j;
        }
        angle[j + 1] = a;
        gathered[j + 1] = g;
    }

    // ---- decimate, AFTER the sort -----------------------------------------
    //
    // A face with more vertices than we represent is a cylinder or a sphere that
    // an artist tessellated, and the honest thing is to inscribe a polygon in it
    // rather than to refuse. `k*n/8` keeps local index 0 — the lowest-indexed
    // vertex, the anchor the ids depend on — and spreads the rest evenly, so the
    // kept polygon is inscribed in the true face and every contact point it
    // produces is still ON the surface. 8.7 §5 measures the cost on a 24-gon:
    // the rim is under-reached by `r(1 - cos(pi/8))`, predicted at 0.03806 m for
    // a radius of 0.5 and measured at 0.03806.
    if (count > k_max_face_vertices)
    {
        for (int i = 0; i < k_max_face_vertices; ++i)
        {
            gathered[i] = gathered[(i * count) / k_max_face_vertices];
        }
        count = k_max_face_vertices;
        f.truncated = true;
    }

    f.count = count;
    for (int i = 0; i < count; ++i)
    {
        f.v[i] = h.axes * h.points[static_cast<std::size_t>(gathered[i])];
        f.id[i] = static_cast<std::uint8_t>(gathered[i] & 0xff);
    }

    // ---- the normal is the FACE's, not the query's -------------------------
    //
    // Up to here `f.normal` has been the query direction, which is what a
    // one-vertex or two-vertex answer has to use because a point and a segment
    // have no plane of their own. A polygon does, and it is a different vector:
    // the gather admits a face up to `k_face_gather_sin` off the query, so the
    // two can differ by nearly three degrees, and 8.7 clips against the
    // REFERENCE FACE'S PLANE. Using the query direction there would tilt every
    // side plane by that angle and mis-measure every contact depth.
    //
    // NEWELL'S METHOD rather than one cross product of two edges, because the
    // gathered set is only approximately planar — it is defined by a tolerance,
    // after all — and a cross product of the first two edges reads whatever
    // noise those particular three vertices carry. Newell's sum is
    // `Σ vᵢ × vᵢ₊₁`, which for a planar polygon is exactly twice its area times
    // its normal, and for a nearly planar one is the area-weighted average of
    // every triple. It is also the same formula a mesh importer uses to give a
    // quad a normal, for the same reason.
    vec3 newell{};
    for (int i = 0; i < count; ++i)
    {
        newell += cross(f.v[i], f.v[(i + 1) % count]);
    }
    if (length_squared(newell) > 0.0f)
    {
        const vec3 unit = normalised(newell);
        f.normal = (dot(unit, f.normal) >= 0.0f) ? unit : -unit;
    }
    return f;
}

} // namespace engine::phys
