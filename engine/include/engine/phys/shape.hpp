// engine/include/engine/phys/shape.hpp — what a body is SHAPED like.
//
// Lesson 8.4. Eighty-six lessons in, no object in this engine has a size that
// the simulation knows about. A `rigid_body` has a mass, an inertia tensor, a
// position and an orientation, and every one of those describes how it MOVES.
// Nothing describes how much room it takes up — which is why, if you drop two
// of 8.2's crates on the same spot, they arrive at the same spot and stay
// there, occupying the same cubic metre of the world, forever.
//
// The gap is visible in the constructor. `make_box(position, mass, half)` takes
// half extents, uses them to build an inertia tensor, and **throws them away**.
// The body then knows exactly how hard it is to spin a 0.5 x 1.0 x 1.5 metre box
// and has no idea that it is 0.5 x 1.0 x 1.5 metres. This file is the missing
// half, and the split is deliberate:
//
//     a SHAPE is geometry            — where the surface is
//     a BODY is dynamics             — how the thing responds to a push
//     a COLLIDER is the pair of them — which Lesson 8.6 introduces, because a
//                                      pair is only useful once something walks
//                                      a list of them looking for candidates
//
// So there is no `shape` member on `rigid_body` yet, and that is not an
// oversight to be tidied later. Collision detection is a query over *geometry*:
// every function in `collide.hpp` takes shapes and placements and returns an
// answer, and none of them has any business knowing what the geometry weighs or
// which way it is moving. Keeping the dependency pointed that way is what lets
// 8.5's GJK, 8.6's broadphase and 8.7's manifold cache each take the piece they
// need without dragging the dynamics along.
//
// ---- WHY THESE THREE PRIMITIVES -------------------------------------------
//
// A sphere, an axis-aligned box and an oriented box. They are not an arbitrary
// starter set; they are the three points on a single curve, and the curve is
// "how much does this shape know about which way round it is?".
//
//   SPHERE  knows nothing, and does not need to. A rotation does not change it,
//           so its test is four floats and one comparison and stays correct
//           however wildly the body tumbles. It is the cheapest true answer in
//           collision detection and the reason every broadphase starts with one.
//
//   AABB    knows the world's axes and nothing else. Its test is three interval
//           comparisons, its storage is two corners, and it is what a spatial
//           grid, a BVH and a frustum culler all actually index. Its weakness is
//           the same as its strength: **it cannot turn**. Rotate the object
//           inside it and the box has to be rebuilt, larger.
//
//   OBB     an AABB that brought its own axes. Six floats plus an orientation,
//           exactly as tight as the object under any rotation, and the price is
//           that its overlap test is no longer three comparisons — it is the
//           Separating Axis Theorem, which is the subject of `collide.hpp` and
//           the mathematical heart of this lesson.
//
// Everything past this file is a generalisation of the OBB case: 8.5's GJK
// answers the same question for any convex shape, and 8.6 puts the spheres and
// the AABBs back in front of it as a filter.
//
// ---- WHAT A SHAPE IS NOT ---------------------------------------------------
//
// It is not a mesh. The visual model and the collision shape are separate
// assets in every engine that ships, and the reason is not laziness: a barrel
// drawn with 4,000 triangles collides as a cylinder because a cylinder is 6
// floats and a convex primitive, and 4,000 triangles are neither. A player
// cannot see the difference and the solver can.
//
// It is also not a transform. A shape is defined in its own BODY AXES, centred
// on the centre of mass, and it is placed in the world by a position and an
// orientation supplied at the call site. `world_obb` below is that placement,
// and it is a function rather than stored state because a shape outlives any
// particular place it happens to be.

#pragma once

#include <engine/math/bounds.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/inertia.hpp>

#include <cstdint>
#include <span>

namespace engine::phys
{

// ---------------------------------------------------------------------------
// The shape itself
// ---------------------------------------------------------------------------

/// Which of the primitives a `shape` is.
///
/// A plain tag rather than a class hierarchy, and that is the engine-core rule
/// from 5.2 rather than a stylistic preference: no RTTI and no virtual dispatch
/// in the simulation, because a collision test is called tens of thousands of
/// times a frame and a vtable lookup per call buys nothing here. There are two
/// shapes; there will be three or four; a `switch` is the right tool for that
/// and stays the right tool at the scale this engine works at.
enum class shape_kind : std::uint8_t
{
    sphere,    ///< A ball. Rotation-invariant, four floats.
    box,       ///< A rectangular box, centred, in body axes. Six floats.
    capsule,   ///< A segment along body +y, inflated by a radius. Two floats.
};

/// `"sphere"`, `"box"`, `"capsule"`. For logs and debug UI.
[[nodiscard]] const char* name_of(shape_kind kind);

/// A collision primitive **in body axes, centred on the centre of mass**.
///
/// Twenty-four bytes, laid out as a tag plus the union of what the primitives
/// need — written as plain members rather than as a `union`, because a `union`
/// here would save twelve bytes and cost every reader a moment of doubt about
/// which member is live. If a primitive with a large payload ever arrives, that
/// trade changes; today it does not. Lesson 8.5's `hull` is exactly such a
/// primitive, and note where it went: it is NOT a `shape_kind`, because it is a
/// span of somebody else's vertices and a `shape` is a value you can copy into a
/// save file. See `hull` below.
///
/// **Centred is a requirement, not a convention.** Every tensor in `inertia.hpp`
/// is taken about the centre of mass, `rigid_body::state.position` *is* the
/// centre of mass, and a torque is `r × F` with `r` measured from it. A shape
/// whose geometric centre is somewhere else would put all three of those
/// quietly out of step. When a real asset needs an off-centre collider — and
/// they do — the offset belongs in the collider that pairs shape with body
/// (Lesson 8.6), where it can be applied once, in one place, visibly.
struct shape
{
    shape_kind kind = shape_kind::sphere;

    /// Metres. Meaningful when `kind == sphere`.
    float radius = 0.5f;

    /// Metres, half the full width on each body axis. Meaningful when
    /// `kind == box` — so a unit cube is `{0.5f, 0.5f, 0.5f}`, not `{1, 1, 1}`.
    ///
    /// Half extents rather than a full size for the same reason a box is stored
    /// as centre-and-extent rather than as two corners in the physics code: the
    /// projection formula in `collide.hpp` is a sum of half extents times
    /// absolute dot products, and every line of the SAT wants the half. Storing
    /// the full width would put a `* 0.5f` on fifteen consecutive lines of the
    /// hottest loop in the module.
    vec3 half_extents{0.5f, 0.5f, 0.5f};

    /// Metres, half the length of the **segment** — not of the whole capsule.
    /// Meaningful when `kind == capsule`, where the total height along body `y`
    /// is `2*half_height + 2*radius`.
    ///
    /// Lesson 8.5. The half again, and for the same reason as `half_extents`:
    /// a capsule's support function is `sign(d.y) * half_height * y + radius *
    /// normalise(d)`, and the half is what appears in it. A capsule with
    /// `half_height == 0` is a sphere, exactly — `volume_of`, `inertia_of` and
    /// `support` all reduce to the sphere case with no special-casing, which is
    /// the first hint that these two primitives are the same family.
    float half_height = 0.5f;
};

/// A ball of radius `r`.
[[nodiscard]] shape sphere_shape(float radius);

/// A box with the given half extents — half, not full. See `shape::half_extents`.
[[nodiscard]] shape box_shape(vec3 half_extents);

/// A cube of side `side`. A convenience, because `box_shape({s/2, s/2, s/2})` is
/// where the factor of two gets dropped.
[[nodiscard]] shape cube_shape(float side);

/// A capsule along body **+y**: a segment of half-length `half_height`, inflated
/// by `radius`. Total height `2*(half_height + radius)`.
///
/// Lesson 8.5. **+y and not an arbitrary axis**, because conventions.html §2
/// makes `y` up and a capsule's overwhelming job in this engine is to stand
/// upright under a character controller (8.13). An arbitrary axis is a rotation,
/// and a shape already has one at placement time.
[[nodiscard]] shape capsule_shape(float radius, float half_height);

/// Cubic metres.
[[nodiscard]] float volume_of(const shape& s);

/// The radius of the smallest sphere centred on the shape's centre that contains
/// it — `radius` for a sphere, the half-diagonal for a box.
///
/// **This is the one measurement of a shape that a rotation cannot change**,
/// which is what makes it the right thing to hand a broadphase. A body that is
/// tumbling has a different AABB every frame and the same bounding radius
/// forever.
[[nodiscard]] float bounding_radius(const shape& s);

/// The inertia tensor of this shape, at uniform density, about its centre, in
/// body axes.
///
/// **This is the dependency the file header promised: a shape knows what it
/// weighs.** It runs one way and must keep running one way — `inertia.hpp` was
/// written in 8.3 knowing nothing about collision geometry and must go on
/// knowing nothing about it, because half the bodies in a real scene get their
/// tensor from an artist's number or from a compound assembly rather than from a
/// collision primitive.
///
/// What this function buys is that `make_box`'s two arguments stop being two
/// independent facts that a caller has to keep in agreement. Before it, a box
/// body's tensor and a box shape's extents were set separately, and setting one
/// without the other gave you a crate that collides like a crate and spins like
/// something else — a bug with no symptom until something lands on a corner.
[[nodiscard]] mat3 inertia_of(const shape& s, float mass);

// ---------------------------------------------------------------------------
// Placement: a shape, somewhere, facing some way
// ---------------------------------------------------------------------------

/// An **oriented** bounding box: a centre, three unit axes, three half extents.
///
/// THE AXES ARE STORED AS A `mat3` AND NOT AS A `quat`, and the reason is a
/// measurement rather than a preference. Every line of the SAT is a dot product
/// between one box's axis and the other's; a quaternion cannot supply those
/// without building the matrix first, and 8.3 §12 found the engine building the
/// same `mat3_from_quat` twice in one step because two reasonable functions each
/// needed it. An `obb` is a box that has already paid that cost, once, at the
/// point where it was placed into the world — which is exactly where a
/// broadphase wants it paid, since a box is placed once per frame and tested
/// against every candidate neighbour.
///
/// The columns are the axes, matching `mat3`'s storage and Lesson 2.5's reading
/// of a matrix as "where the basis vectors land": column `i` is the world
/// direction of the box's own `i`-th axis. `axis(i)` says so out loud.
struct obb
{
    vec3 centre{};                      ///< World space, metres.
    mat3 axes = mat3::identity();       ///< Columns are the unit body axes in world space.
    vec3 half_extents{0.5f, 0.5f, 0.5f};///< Metres, along the corresponding axis.

    /// The world-space direction of body axis `i`, for `i` in 0..2.
    [[nodiscard]] vec3 axis(int i) const
    {
        return (i == 0) ? axes.c0 : (i == 1) ? axes.c1 : axes.c2;
    }

    /// Half extent along body axis `i`, for `i` in 0..2.
    [[nodiscard]] float half(int i) const
    {
        return (i == 0) ? half_extents.x : (i == 1) ? half_extents.y : half_extents.z;
    }

    /// The eight corners, written into `out`.
    ///
    /// **Same index order as `aabb::corners`** — x varies fastest, then y, then
    /// z, with bit `i` set meaning the `+` side — so a test can name a corner by
    /// index and mean the same one whichever box type it is holding. That
    /// agreement is worth more than it looks: 8.7's manifolds identify contact
    /// features by index, and two conventions for "corner 3" would be a bug that
    /// only appears when a box changes type.
    void corners(vec3 out[8]) const;
};

/// Place a box shape in the world. Asserts in debug that `s` is a box.
[[nodiscard]] obb world_obb(const shape& s, vec3 centre, quat orientation);

/// Place a sphere shape in the world. Asserts in debug that `s` is a sphere.
[[nodiscard]] engine::sphere world_sphere(const shape& s, vec3 centre);

/// The axis-aligned box that contains this oriented one.
///
/// **This is the projection formula from `collide.hpp` §6, run three times**,
/// and seeing that is worth more than the function is. The half extent of the
/// enclosing AABB along world axis `i` is the OBB's projected radius onto that
/// axis, which is `Σⱼ hⱼ |dot(uⱼ, eᵢ)|` — and `dot(uⱼ, eᵢ)` is just the `i`-th
/// component of the `j`-th column, so the whole thing is the element-wise
/// absolute value of the axis matrix applied to the half extents. Three lines,
/// no corners enumerated, no branches.
///
/// It is loose, necessarily: a box rotated 45° about one axis has an enclosing
/// AABB up to √2 wider, and up to √3 under a general rotation. That looseness is
/// the price of axis alignment and is why 8.6's broadphase uses this for
/// candidate pairs and then asks `collide` for the real answer.
[[nodiscard]] aabb bounds_of(const obb& box);

/// The axis-aligned box that contains this shape, placed here, facing this way.
///
/// The sphere case ignores the orientation, exactly, and that is the whole
/// argument for spheres in one line of code.
[[nodiscard]] aabb bounds_of(const shape& s, vec3 centre, quat orientation);

/// The AABB read as an OBB with identity axes.
///
/// A conversion rather than a separate code path, and it is how §5's claim that
/// "an AABB test is the SAT with three axes" is checked rather than asserted:
/// the harness runs the specialised `collide(aabb, aabb)` and the general
/// `collide(obb, obb)` on the same pairs and requires them to agree.
[[nodiscard]] obb as_obb(const aabb& box);

/// The farthest point of the box in direction `d`. `d` need not be unit length.
///
/// **The support function** — the one interface every convex shape in this
/// engine will eventually be asked for, because Lesson 8.5's GJK is written
/// entirely in terms of it and needs nothing else. For a box it is three sign
/// tests: go to the `+` end of each axis when the direction leans that way and
/// the `−` end when it leans the other, which is the same "choose `sᵢ` to
/// maximise" argument that produces the projected-radius formula.
///
/// A degenerate `d` of exactly zero returns the `+++` corner rather than
/// asserting. There is no meaningful "farthest point in no direction", and every
/// caller that could produce a zero direction is already guarding it for its own
/// reasons; returning a real corner keeps the function total.
[[nodiscard]] vec3 support(const obb& box, vec3 d);

/// The farthest point of the sphere in direction `d`. Zero `d` returns the centre.
[[nodiscard]] vec3 support(const engine::sphere& s, vec3 d);

// ---------------------------------------------------------------------------
// Lesson 8.5: two more convex shapes, and support taken RELATIVE to a centre
// ---------------------------------------------------------------------------

/// A capsule placed in the world: a segment, inflated by a radius.
///
/// **A capsule is a Minkowski sum, and its support function says so out loud.**
/// The set is `segment ⊕ ball`, and the support function of a Minkowski sum is
/// the *sum of the support functions* — `support_{X⊕Y}(d) = support_X(d) +
/// support_Y(d)` — because maximising `dot(x + y, d)` over independent `x` and
/// `y` maximises each term separately. So:
///
///     support(capsule, d) = ±half_height·axis   +   radius·normalise(d)
///                           \_____ segment ____/     \______ ball ______/
///
/// Two terms, one per operand, and neither of them knows the other exists. That
/// identity is the reason 8.5 can hand GJK a capsule without GJK learning what a
/// capsule is, and it is the same identity that makes the Minkowski *difference*
/// in `gjk.hpp` computable at all.
struct capsule
{
    vec3 centre{};                  ///< World space, the midpoint of the segment.
    vec3 axis{0.0f, 1.0f, 0.0f};    ///< Unit, world space. Body `+y`, rotated.
    float half_height = 0.5f;       ///< Half the SEGMENT, not half the capsule.
    float radius = 0.5f;            ///< Metres.

    /// The two segment endpoints — the capsule's "spine".
    [[nodiscard]] vec3 end(int sign) const
    {
        return centre + axis * (sign >= 0 ? half_height : -half_height);
    }
};

/// A convex point set placed in the world — **the shape that has no formula**.
///
/// The other primitives in this file are described by numbers: a radius, three
/// half extents. This one is described by its vertices, which is what an artist's
/// convex collider actually is, and it is the reason GJK exists. There is no SAT
/// axis list for an arbitrary hull — 8.4's fifteen candidates came from *knowing*
/// the shape was a box — and there is no closed form for the distance between
/// two of them. There is only a support function, and that turns out to be
/// enough.
///
/// **`points` is a non-owning span, and the vertices are in BODY axes relative
/// to `centre`.** A hull's geometry is an asset: it is loaded once, shared by
/// every instance, and outlives any particular placement, exactly like a mesh.
/// That is also why a hull is not a `shape_kind` — a `shape` is a 24-byte value
/// that Module 9 will write to a save file, and a pointer is not a value.
///
/// **It need not actually be a hull.** `support` takes the maximum over the
/// points, so passing a point cloud gives you the support function of its convex
/// hull for free, and passing interior points costs time and changes nothing.
/// The engine does not compute hulls — 8.5 §13 says plainly that a hull builder
/// is out of scope and names the algorithm.
struct hull
{
    vec3 centre{};                       ///< World space.
    mat3 axes = mat3::identity();        ///< Columns are the unit body axes in world space.
    std::span<const vec3> points{};      ///< Body axes, relative to `centre`.
};

/// Place a capsule shape in the world. Asserts in debug that `s` is a capsule.
[[nodiscard]] capsule world_capsule(const shape& s, vec3 centre, quat orientation);

/// Place a point set in the world. `points` must outlive the returned `hull`.
[[nodiscard]] hull world_hull(std::span<const vec3> points, vec3 centre, quat orientation);

/// The farthest point of the capsule in direction `d`.
[[nodiscard]] vec3 support(const capsule& c, vec3 d);

/// The farthest point of the hull in direction `d`. Linear in the vertex count.
///
/// **Brute force, and that is a decision rather than a placeholder.** The
/// alternative is hill climbing over an adjacency list, which turns O(n) into
/// roughly O(√n) and needs a data structure this engine does not have. 8.5 §12
/// measures the crossover: at the vertex counts a hand-authored collider
/// actually has, the linear scan wins, because it is a contiguous sweep with no
/// pointer chasing and the branch predictor sees one loop.
[[nodiscard]] vec3 support(const hull& h, vec3 d);

// ---- support, relative to the shape's own centre ---------------------------
//
// THE SAME FUNCTIONS, MINUS THE CENTRE, AND THE DIFFERENCE IS NUMERICAL RATHER
// THAN STYLISTIC. 8.4 §11 measured a 1 mm gap between two boxes decaying to
// EXACTLY ZERO at 100 km from the origin, and found the cause upstream of the
// algorithm: `b.centre - a.centre` subtracts two world-sized floats to produce a
// crate-sized one, and the information is gone before the test begins.
//
// GJK would inherit that intact, and worse: it forms `support_A(d) -
// support_B(-d)` on EVERY iteration, so the cancellation is not paid once but
// once per step. These overloads are the fix. Each returns the support point as
// an OFFSET FROM ITS OWN CENTRE — for a box that is `axes * s` with the centre
// never entering the arithmetic at all — and `gjk.hpp` forms the one large
// subtraction `b_origin - a_origin` exactly once, outside the loop.
//
// 8.5 §11 measures what that buys, and the honest answer has two halves: the
// algorithm stops adding error of its own, and the error already present in the
// input positions stays. A relative formulation cannot recover information that
// `float` positions never held.

/// The farthest point of the box in direction `d`, **relative to `box.centre`**.
[[nodiscard]] vec3 support_local(const obb& box, vec3 d);

/// The farthest point of the sphere in direction `d`, relative to its centre.
[[nodiscard]] vec3 support_local(const engine::sphere& s, vec3 d);

/// The farthest point of the capsule in direction `d`, relative to its centre.
[[nodiscard]] vec3 support_local(const capsule& c, vec3 d);

/// The farthest point of the hull in direction `d`, relative to its centre.
[[nodiscard]] vec3 support_local(const hull& h, vec3 d);

/// The axis-aligned box that contains this capsule.
///
/// A capsule is a Minkowski sum, so its bounds are one too: the AABB of the
/// segment, grown by `radius` on every side. No case analysis, no corners.
[[nodiscard]] aabb bounds_of(const capsule& c);

/// The axis-aligned box that contains this hull. Linear in the vertex count.
[[nodiscard]] aabb bounds_of(const hull& h);

} // namespace engine::phys
