// engine/include/engine/phys/convex.hpp — a convex shape, reduced to one question.
//
// Lesson 8.5. 8.4's Separating Axis Theorem needed to KNOW its shapes. The
// fifteen candidate axes were three face normals from each box and nine cross
// products of their edges, and every one of those fifteen is a fact about boxes:
// a box has three distinct face normals, a box has three distinct edge
// directions. Hand that algorithm a cylinder and there is no list to write down,
// because a cylinder has infinitely many face normals around its rim. Hand it an
// artist's twenty-vertex convex collider and the list is 20 + 20 face normals
// and several hundred edge pairs, which is no longer a test but a survey.
//
// So the SAT stops at boxes. This file is the interface that does not.
//
// ---- THE ONE QUESTION ------------------------------------------------------
//
// A convex set is completely determined by its SUPPORT FUNCTION:
//
//     support_X(d) = the point of X that is farthest in direction d
//
// Not "described by", not "approximated by" — DETERMINED. A convex set is the
// intersection of all the half-spaces that contain it, `support` names the
// boundary of each one, and knowing every half-space is knowing the set. Two
// convex shapes with the same support function are the same shape.
//
// That is a remarkably small interface for something that strong. It is one
// function, it returns one point, and every shape in `shape.hpp` implements it
// in four lines or fewer. 8.5 §4 works through why a single point is enough.
//
// ---- HOW THE DISPATCH IS DONE, AND WHY NOT A VIRTUAL -----------------------
//
// `convex` is a FUNCTION POINTER PLUS A CONTEXT POINTER. Not a base class with a
// virtual `support`, because 5.2 forbids RTTI and virtuals in the engine core;
// not a template, because GJK would then have to live in a header and be
// instantiated once per pair of shape types, which is ten instantiations for
// four shapes and grows as the square.
//
// 6.17's frame graph made the same choice and made it with a number, and 8.5 §12
// takes the measurement again for this call site, where it is hotter: an
// indirect call per support query, two support queries per GJK iteration, and
// several iterations per pair. The number is in the lesson; the short version is
// that the branch predictor sees the same target on every call within a query
// and the indirection costs less than the `switch` it replaces.
//
// ---- IT IS A VIEW, AND VIEWS DANGLE ----------------------------------------
//
// `convex` stores a `const void*` to a shape it does not own. That is the same
// contract `std::span` has and the same one 5.1 fixed for the whole engine:
// non-owning access is a view, and a view must not outlive what it views. The
// failure mode is a temporary —
//
//     const convex c = as_convex(world_obb(s, p, q));   // does not compile
//
// — and the rvalue overloads below are deleted so that it does not compile
// rather than reading freed stack on the next line. 8.5 §13 lists the four
// mistakes this file turns into compile errors.

#pragma once

#include <engine/math/vec3.hpp>
#include <engine/phys/shape.hpp>

namespace engine::phys
{

/// A convex shape, as the only thing an algorithm needs to know about it.
///
/// **`support` returns a point RELATIVE TO `origin`, not a world point**, and
/// that split is numerical rather than cosmetic. 8.4 §11 measured a 1 mm gap
/// between two boxes decaying to exactly zero at 100 km from the world origin,
/// because `b.centre − a.centre` subtracts two world-sized floats. GJK forms
/// that same difference on every iteration, so it would pay the cancellation
/// once per step rather than once per query. Splitting the shape into "where it
/// is" and "how far its surface reaches from there" lets `gjk.hpp` do the large
/// subtraction exactly once, outside the loop, on crate-sized numbers
/// thereafter.
struct convex
{
    /// The farthest point of the shape in direction `d`, relative to `origin`.
    /// `d` need not be unit length. A zero `d` must return a real point.
    using support_fn = vec3 (*)(const void* data, vec3 d);

    /// **The whole feature the shape presents in direction `d`**, relative to
    /// `origin` — Lesson 8.7's `support_face`.
    ///
    /// A SECOND FUNCTION POINTER AND NOT A VIRTUAL, for 5.2's reason and for one
    /// more that is specific to a pair of them: a `convex` is passed BY VALUE, so
    /// at a call site that built it from a concrete shape the compiler can see
    /// both targets and devirtualise them. A base class with two virtuals would
    /// be one pointer to a table the optimiser has to chase.
    ///
    /// It may be null, and a null is not a failure — it is a shape that has no
    /// face query, for which `face_of` below falls back to a single support
    /// point. That fallback is CORRECT and not a stub: a one-point manifold is
    /// exactly right for a ball, and it is what a caller gets for any shape
    /// whose adapter has not been taught faces yet.
    using face_fn = contact_face (*)(const void* data, vec3 d);

    support_fn support = nullptr;
    face_fn face = nullptr;
    const void* data = nullptr;

    /// Where the shape's local frame sits in the world. Any point inside the
    /// shape will do; every adapter below uses the geometric centre, because a
    /// centre keeps `support`'s output as small as the shape.
    vec3 origin{};

    /// The farthest point of the shape in direction `d`, **in world space**.
    ///
    /// Provided for callers outside GJK — a debug draw, a ray cast. GJK itself
    /// never calls this, on purpose: adding `origin` back is exactly the
    /// cancellation the split exists to avoid.
    [[nodiscard]] vec3 world_support(vec3 d) const { return origin + support(data, d); }

    /// The feature in direction `d`, relative to `origin`, or a single support
    /// point when this shape has no `face` query.
    ///
    /// **The fallback fabricates nothing.** It returns the one point `support`
    /// gives, with that point's own direction as the normal and an id of zero —
    /// which is the truth about a shape whose features are unknown, and produces
    /// a one-point manifold rather than a wrong four-point one.
    [[nodiscard]] contact_face face_of(vec3 d) const
    {
        if (face) { return face(data, d); }
        contact_face f;
        f.count = 1;
        f.v[0] = support(data, d);
        f.normal = normalised_or(d, vec3{0.0f, 1.0f, 0.0f});
        f.id[0] = 0;
        f.feature = 0;
        return f;
    }
};

// ---------------------------------------------------------------------------
// Adapters
// ---------------------------------------------------------------------------
//
// Each is a captureless lambda converted to a plain function pointer by the
// leading `+`. A lambda with no captures has no state, so the language lets it
// decay to `vec3(*)(const void*, vec3)` — which is how a modern C++ codebase
// writes a C-style thunk without writing a C-style thunk. There is no
// allocation, no `std::function`, and nothing to indirect through but the call
// itself.
//
// Lesson 8.7 gives each adapter a SECOND thunk, for `support_face`. Note that
// the two are independent: a shape may have a support function and no face
// query, and `face_of` then answers with the support point. That is not a
// degraded mode — it is the honest answer for a ball, which has no flat feature
// to return, and it is what makes the face query addable one shape at a time.
//
// The `= delete` rvalue overloads are the whole safety story. They cost nothing
// at runtime and they turn the one mistake this interface invites into a
// compiler diagnostic that names the line.

/// View a placed box as a convex shape.
[[nodiscard]] inline convex as_convex(const obb& box)
{
    return convex{
        +[](const void* data, vec3 d) { return support_local(*static_cast<const obb*>(data), d); },
        +[](const void* data, vec3 d) { return support_face(*static_cast<const obb*>(data), d); },
        &box,
        box.centre,
    };
}

/// View a placed sphere as a convex shape.
[[nodiscard]] inline convex as_convex(const engine::sphere& s)
{
    return convex{
        +[](const void* data, vec3 d)
        { return support_local(*static_cast<const engine::sphere*>(data), d); },
        +[](const void* data, vec3 d)
        { return support_face(*static_cast<const engine::sphere*>(data), d); },
        &s,
        s.centre,
    };
}

/// View a placed capsule as a convex shape.
[[nodiscard]] inline convex as_convex(const capsule& c)
{
    return convex{
        +[](const void* data, vec3 d)
        { return support_local(*static_cast<const capsule*>(data), d); },
        +[](const void* data, vec3 d)
        { return support_face(*static_cast<const capsule*>(data), d); },
        &c,
        c.centre,
    };
}

/// View a placed convex point set as a convex shape.
[[nodiscard]] inline convex as_convex(const hull& h)
{
    return convex{
        +[](const void* data, vec3 d) { return support_local(*static_cast<const hull*>(data), d); },
        +[](const void* data, vec3 d) { return support_face(*static_cast<const hull*>(data), d); },
        &h,
        h.centre,
    };
}

// A `convex` built from a temporary would hold a pointer into a dead stack
// frame, and the symptom would be a distance that is usually right — because the
// memory is usually still intact — and occasionally absurd. Deleted, so the
// mistake is a compile error at the call site instead.
convex as_convex(obb&&) = delete;
convex as_convex(engine::sphere&&) = delete;
convex as_convex(capsule&&) = delete;
convex as_convex(hull&&) = delete;

// ---------------------------------------------------------------------------
// Lesson 8.13: a `shape` placed in the world, by value
// ---------------------------------------------------------------------------
//
// A DEBT PAID. Every scene since 8.10 has needed to turn "this body's `shape` at
// this body's pose" into a `convex`, and `as_convex` needs an LVALUE of the
// placed primitive to point at — so each of them wrote the same small struct
// holding a sphere, a box and a capsule by value, plus a `switch` to fill the
// right one. 8.10's `stack` demo, 8.11's `joints`, 8.12's `ragdoll`, and 8.11's
// and 8.12's harnesses: five copies, each one named as a debt by the lesson that
// wrote it.
//
// 8.13's character controller is the first code INSIDE the engine that needs
// it, and a sixth copy in `character.cpp` would have been the first one a
// student could not see was a copy. So it lives here now. The five that shipped
// are left as they shipped — their pages print them — and new code uses this.

/// A primitive placed in the world and held **by value**, so that `view()` has
/// something to point at.
///
/// Three members rather than a `union` for `shape`'s reason: a union would save
/// sixty bytes and cost every reader a moment of doubt about which member is
/// live. `kind` says which one is.
struct placed_shape
{
    shape_kind kind = shape_kind::sphere;
    engine::sphere s{};
    obb b{};
    capsule c{};

    /// A `convex` view of the live member. **It points INTO this object**, so
    /// the `placed_shape` must outlive it — and the deleted rvalue overload
    /// below turns the one-liner that would not, `place(...).view()`, into a
    /// compile error, for the same reason the `as_convex` overloads above are
    /// deleted.
    [[nodiscard]] convex view() const&
    {
        switch (kind)
        {
        case shape_kind::sphere:  return as_convex(s);
        case shape_kind::box:     return as_convex(b);
        case shape_kind::capsule: return as_convex(c);
        }
        return as_convex(s);
    }

    convex view() const&& = delete;
};

/// Place a body-space `shape` at a pose. `centre` is the body's centre of mass,
/// which `shape` is centred on by definition (see `shape`).
[[nodiscard]] inline placed_shape place(const shape& s, vec3 centre, quat orientation)
{
    placed_shape p;
    p.kind = s.kind;
    switch (s.kind)
    {
    case shape_kind::sphere:  p.s = world_sphere(s, centre); break;
    case shape_kind::box:     p.b = world_obb(s, centre, orientation); break;
    case shape_kind::capsule: p.c = world_capsule(s, centre, orientation); break;
    }
    return p;
}

} // namespace engine::phys
