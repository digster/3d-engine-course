// engine/include/engine/phys/manifold.hpp — one contact is not a contact.
//
// Lesson 8.7. 8.6 finished the narrow phase's arithmetic: `epa_penetration`
// returns how deep two convex shapes overlap, which way to push them apart, and
// a pair of witness points on the two surfaces. Everything a solver needs, and
// a box resting on a floor still falls over.
//
// ---- WHY ONE POINT CANNOT HOLD A BOX UP ------------------------------------
//
// A contact force acts at a point and along a normal, so the torque it exerts
// about the body's centre of mass is `r × F` with `r` from the centre to that
// point. Put the point anywhere but directly under the centre and the torque is
// non-zero, so the box starts to turn. As it turns, the contact point moves, and
// the new torque is generally larger. There is no equilibrium unless the single
// point happens to sit exactly beneath the centre of mass, which is a set of
// measure zero.
//
// With FOUR points the resultant force can act anywhere in their convex hull —
// that hull is what statics calls the SUPPORT POLYGON — because the four normal
// impulses are independent non-negative numbers and their weighted average is
// any point of the hull. A box rests when its centre of mass projects inside its
// support polygon, and tips when it does not. That is the whole of why a table
// has more than one leg, and 8.7 §1 measures it on this engine's own
// `rigid_body`, with the same crate, the same gravity and the same impulses in
// both arms: a four-point contact settles at 1.16 mm of penetration and
// 5.2 deg/s of residual rotation, and the same contact reduced to its deepest
// single point sinks **21.27 mm** and rocks at **27.99 deg/s**, permanently.
// §1 also bisects the moment a support polygon can carry and lands on
// 47.9 N m against a predicted `m·g·(w/2)` of 49.1.
//
// **So a manifold is not an optimisation.** A single-point contact does not
// approximate a resting box badly; it describes a different physical situation —
// a box balanced on a pin.
//
// ---- AND WHY THE SAME FOUR, NEXT FRAME -------------------------------------
//
// The second half of this lesson is the word PERSISTENCE, and the reason for it
// is 8.10's solver rather than 8.7's geometry. A sequential-impulse solver
// converges over many iterations, and sixty times a second it gets to run only a
// handful of them. Warm starting — beginning each contact from the impulse that
// held it last frame — is what turns "not converged yet" into "converged four
// frames ago and still is", and it is why a stack settles instead of sinking.
//
// That requires knowing which of this frame's contacts IS which of last frame's,
// and the tempting answer is wrong: you cannot match them by position. Both
// bodies moved, the positions are floats computed by a different route through
// the same code, and 8.6 §7 already paid for the general version of this lesson
// — a comparison whose two sides are mathematically equal has no correct answer
// in floating point. So contacts are matched by IDENTITY. Every point this file
// produces carries a `contact_id` that names the features that made it: this
// corner of that face, that edge crossing this side plane. Two frames apart, the
// same geometric contact carries the same id bit for bit, and a contact that
// genuinely moved to a new feature carries a different one, which is exactly
// what you want it to do.
//
// ---- THE THING TO KNOW BEFORE READING THE CODE -----------------------------
//
// **The contact normal is discontinuous, and that is a property of a minimum,
// not a bug in EPA.** 8.6 §13 named it and 8.7 opens with it. Slide a box along
// a wall toward the corner: the minimum translation is out through the wall
// until the box passes the corner, at which point it is out through the end, and
// the normal swings ninety degrees between two consecutive frames while the
// depth stays continuous. Every implementation of penetration depth has this,
// because `min` of a set of smooth functions is not smooth where the argmin
// changes.
//
// Nothing here removes that. What this file does is make the contact set stable
// WITHIN a feature pair, so that the swing is a single discrete event affecting
// a handful of ids rather than four points re-derived from scratch every frame.
//
// ---- WHAT THIS FILE NEEDED THAT 8.6 DID NOT HAVE ---------------------------
//
// A face. 8.5's whole claim was that a support function determines a convex set,
// and it does — but `support(d)` returns ONE point even when the argmax is a
// whole square, and which corner comes back is decided by a tie-break. So the
// interface widened by one query, in `shape.hpp`: `support_face` returns the
// entire feature. It did NOT need EPA's winning triangle, which is the thing
// this lesson was expected to want: a face of `A ⊖ B` is a face of A against a
// vertex of B (or the other way, or an edge against an edge), so the features
// are recoverable from it — but its three vertices are support points, with the
// tie-break problem intact, and the triangle is a piece of the true face rather
// than the whole of it. Asking each shape directly is both simpler and exact.
// `epa.hpp` is unchanged by this lesson.

#pragma once

#include <engine/math/vec3.hpp>
#include <engine/phys/convex.hpp>
#include <engine/phys/epa.hpp>
#include <engine/phys/shape.hpp>

#include <cstdint>
#include <vector>

namespace engine::phys
{

// ---------------------------------------------------------------------------
// Capacity
// ---------------------------------------------------------------------------

/// The most contact points a manifold holds. **Four.**
///
/// Not a budget and not a guess. Four is the number of independent non-negative
/// normal impulses needed to place the resultant anywhere in a convex support
/// polygon in three dimensions, and adding a fifth adds nothing a solver can
/// use: the fifth impulse is a linear combination of the others, which makes the
/// contact system rank-deficient and the solver's answer non-unique. A physical
/// four-legged table wobbles for the same reason a five-point manifold does.
///
/// Every engine this course draws on lands on four — Box2D, Bullet's persistent
/// manifold, Havok — and 8.7 §8 measures what the fifth point would buy on this
/// engine's own geometry. The four kept points span **96.3% of the area of the
/// best four there are**, brute-forced over every four-subset, and lose
/// **exactly zero** depth.
inline constexpr int k_max_manifold_points = 4;

/// Working room for the clipper before reduction.
///
/// Clipping a polygon of `m` vertices against a convex polygon of `n` side
/// planes can produce at most `m + n` vertices — each plane adds at most two and
/// removes at least one — so two faces of `k_max_face_vertices` need twice that.
inline constexpr int k_max_clip_points = 2 * k_max_face_vertices;

// ---------------------------------------------------------------------------
// Identity
// ---------------------------------------------------------------------------

/// **What made a contact point.** The half of a `contact_id` that says how to
/// read the other fields.
enum class contact_feature : std::uint8_t
{
    /// A vertex of the incident face, lying inside the reference face. The
    /// commonest contact in a game: a crate's corner on a floor.
    incident_vertex,

    /// An edge of the incident face, cut by a side plane of the reference face.
    /// What you get when two faces overlap only partially.
    crossing,

    /// A vertex of the reference face, lying inside the incident face. The
    /// mirror of `incident_vertex`, and it happens whenever the reference face
    /// is the smaller of the two.
    reference_vertex,

    /// The closest points of two non-parallel edges. Exactly one contact point,
    /// and correctly so: two crossed sticks touch at one place.
    edge_pair,

    /// A single point on a curved surface. A ball on a floor; a capsule's
    /// shoulder against a wall. **One point is the truth here**, not a shortfall.
    point,
};

/// `"incident vertex"`, `"crossing"`, `"reference vertex"`, `"edge pair"`,
/// `"point"`.
[[nodiscard]] const char* name_of(contact_feature feature);

/// **The identity of a contact point, stable across frames.**
///
/// Six bytes of named fields rather than a packed integer. Box2D famously folds
/// the equivalent into four, and it is right to: it stores one per contact point
/// in a structure it copies constantly. Here the id sits inside a 48-byte
/// `contact_point` that already carries three impulses and a position, so the
/// two bytes buy nothing and cost every reader a shift and a mask. **Pack when
/// the packing pays; name the fields when it does not.**
///
/// WHAT MAKES AN ID STABLE. Every field below is a discrete index into geometry
/// that does not move: a face's own identity (0-5 for a box, a hash of the
/// coplanar vertex set for a hull) and a vertex's own index on its shape (0-7 in
/// `obb::corners`' order). None of them is computed from a position, so none of
/// them can change because a body moved by a micron. They change when the
/// CONTACT changes, which is the definition of what an id is for.
struct contact_id
{
    /// The reference shape's face feature, from `contact_face::feature`.
    std::uint16_t reference_face = 0;

    /// The incident shape's face feature.
    std::uint16_t incident_face = 0;

    /// The reference shape's vertex id — the start of the cutting edge for a
    /// `crossing`, the corner itself for a `reference_vertex`. `0xff` when the
    /// reference contributed only its face.
    std::uint8_t reference_index = 0xff;

    /// The incident shape's vertex id — the corner itself for an
    /// `incident_vertex`, the start of the cut edge for a `crossing`. `0xff`
    /// when the incident shape contributed only its face.
    std::uint8_t incident_index = 0xff;

    /// How to read the two index fields.
    contact_feature kind = contact_feature::point;

    /// **The reference face was on `b`, not `a`.**
    ///
    /// This bit is load-bearing and is the one people leave out. Which shape
    /// supplies the reference face is decided by a `>` between two nearly equal
    /// cosines, so it can flip between frames on a face-to-face contact — and
    /// when it flips, every field above swaps meaning. Without the bit, the
    /// swapped id would sometimes MATCH an unrelated contact from the frame
    /// before and a warm start would be applied to the wrong point. With it, the
    /// flip simply looks like four new contacts, which is the honest answer.
    /// 8.7 §9 measures the flip rate at **zero, over 208,000 pairs** — see
    /// `contact_manifold::reference_on_b` for why.
    std::uint8_t flipped = 0;

    [[nodiscard]] friend bool operator==(const contact_id& x, const contact_id& y)
    {
        return x.reference_face == y.reference_face && x.incident_face == y.incident_face
               && x.reference_index == y.reference_index && x.incident_index == y.incident_index
               && x.kind == y.kind && x.flipped == y.flipped;
    }
};

// ---------------------------------------------------------------------------
// The manifold
// ---------------------------------------------------------------------------

/// One point of contact between two shapes.
struct contact_point
{
    /// World space, **midway between the two surfaces**.
    ///
    /// The contact is really a PAIR of points, one on each shape, separated by
    /// `depth` along the manifold normal. A solver needs a single lever arm per
    /// body, and using the midpoint splits the error evenly rather than biasing
    /// every torque toward one of them. Bullet keeps both points and Box2D keeps
    /// the incident one; the difference is `depth/2` in the lever arm, which at
    /// the depths a solver is meant to allow is under a millimetre. Recover
    /// either surface point as `position ± normal * depth * 0.5f`.
    vec3 position{};

    /// Metres of overlap along the manifold normal at this point, never
    /// negative. **Points differ**: a tilted box on a floor is deeper at one
    /// corner than another, and that difference is precisely what a four-point
    /// manifold knows and a single averaged contact does not.
    float depth = 0.0f;

    /// Which features made this point. See `contact_id`.
    contact_id id{};

    /// **The impulse the solver applied here last frame**, in newton-seconds.
    ///
    /// Written by 8.10 and carried across frames by `carry_impulses`. Nothing in
    /// 8.7 sets it to anything but zero, and it is declared here rather than
    /// later because the whole point of the id above is to make this field
    /// survive — a manifold with no slot for it is a manifold no solver can warm
    /// start, and retrofitting it would mean rewriting the matching.
    float normal_impulse = 0.0f;

    /// The two friction impulses, in the solver's tangent basis. 8.9.
    float tangent_impulse[2] = {0.0f, 0.0f};

    /// **This point was matched to one from the previous frame** and inherited
    /// its impulses. Instrumentation that turns "persistence works" into a
    /// number: on a settled stack it should be true for essentially every point,
    /// and a run where it is not is a run where warm starting is doing nothing.
    bool warm = false;
};

/// How a manifold was generated, which is also how much to trust it.
enum class manifold_status : std::uint8_t
{
    /// The shapes are apart. `count` is zero.
    none,

    /// Two faces were clipped against each other. One to four points.
    face,

    /// Two edges crossed. Exactly one point.
    edge,

    /// At least one surface was curved where they touch. Exactly one point.
    point,

    /// A face pair was found but clipping produced nothing — the faces do not
    /// overlap in projection, which a convex overlap should make impossible.
    /// Falls back to the least separated clipped point and says so. 8.7 §7
    /// measures the rate at **one pair in 189,282** and the cause is worth
    /// knowing.
    clip_empty,
};

/// `"none"`, `"face"`, `"edge"`, `"point"`, `"clip empty"`.
[[nodiscard]] const char* name_of(manifold_status status);

/// **A set of contact points that share one normal.**
struct contact_manifold
{
    /// Unit, **pointing from `a` toward `b`** — `collide.hpp`'s convention,
    /// unchanged since 8.4.
    ///
    /// It is the REFERENCE FACE's own normal, not EPA's. On a face contact the
    /// two agree to EPA's tolerance and the face normal is the exact one: it is
    /// a cross product of two edges of a real shape, where EPA's is the normal
    /// of a triangle approximating a face of the difference set. 8.7 §6
    /// measures both halves. Where the reference face is the one the exact MTV
    /// came from — 36,113 of 39,403 face contacts — the manifold normal is
    /// **bit-exact**, against EPA's worst of 0.0163°; where it is not, the normal
    /// is deliberately snapped to the reference face, by at most
    /// `acos(face_cos)`.
    vec3 normal{};

    /// The contact points. Only the first `count` are meaningful.
    ///
    /// **THE ARRAY IS `k_max_clip_points` LONG AND `count` NEVER EXCEEDS FOUR**
    /// unless `manifold_config::reduce` is off. The extra slots are not a
    /// hedge: §8's whole argument is a comparison between the four kept points
    /// and the polygon the entire clipped set spans, and a manifold that can
    /// only hold four cannot be its own control. They cost 480 bytes on a struct
    /// that is built on the stack, once per pair, and never copied per point.
    contact_point points[k_max_clip_points];

    int count = 0;

    manifold_status status = manifold_status::none;

    /// The reference face came from `b`. See `contact_id::flipped`.
    ///
    /// **THIS FIELD IS WHERE A KNOB USED TO BE.** A draft of this header carried
    /// a `reference_bias` — "prefer `a` unless `b`'s face is this much more
    /// parallel" — on the reasoning every 2D engine gives: two equally parallel
    /// faces make the choice a coin toss, a bare `>` decides it on rounding, and
    /// a flip renames all four contacts at once, because `contact_id::flipped`
    /// is part of the identity.
    ///
    /// 8.7 §9 measured it at **zero flips in 208,000 pairs** across freely
    /// rotated boxes, crates stacked on crates, and hexagonal prisms whose face
    /// normals come out of Newell's method rather than out of a rotation matrix.
    /// The reason is worth more than the knob was: on a face contact BOTH
    /// cosines are exactly `1.0f`, because each face really is perpendicular to
    /// the contact normal — so the tie is EXACT, and an exact tie is resolved
    /// deterministically. A quantity that merely *ought* to be equal would be a
    /// coin toss; one that *is* equal is not. The knob came out, under 8.6 §12's
    /// rule that a change buying nothing still costs a reader — and §9's control
    /// forces a flip by swapping the two arguments, to show what one would cost:
    /// **every warm start on the pair.**
    bool reference_on_b = false;

    /// A face had more vertices than `k_max_face_vertices` and was decimated.
    bool truncated = false;

    /// How many points the clipper produced before reduction to four.
    /// Instrumentation: 8.7 §8's histogram, and the argument for the reduction.
    int clipped = 0;

    /// How many points `carry_impulses` matched to the previous frame.
    int warm_points = 0;

    /// The deepest point's depth, or zero for an empty manifold.
    [[nodiscard]] float deepest() const;

    /// The area of the support polygon the points span, in square metres.
    ///
    /// **The number that says whether this manifold can hold a body up**, and it
    /// is why the field exists rather than the count: four points in a line have
    /// a count of four and an area of zero, and they resist rotation about that
    /// line no better than one point does. 8.7 §1 uses it as the instrument.
    [[nodiscard]] float support_area() const;
};

/// Knobs. The defaults are what 8.7 measured.
struct manifold_config
{
    /// **How parallel a feature must be to the contact normal to be used as a
    /// face**, as a cosine. The default is 0.999, about 2.56 degrees.
    ///
    /// Too loose and two boxes meeting edge-on get a four-point manifold whose
    /// outer points are centimetres off the surface. Too tight and a crate
    /// resting on a floor whose contact normal EPA's own tolerance has nudged a
    /// hundredth of a degree off drops to an edge contact and loses half its
    /// points. 8.7 §6 sweeps it and measures both: the mean contact count falls
    /// from 2.38 to 2.21 as it tightens, and **the worst normal error is
    /// `acos(face_cos)` exactly** — 44.8° at 0.5, 2.56° at the default. Which is
    /// what makes this a knob rather than a magic number: choose the largest
    /// normal error a solver can live with, and take its cosine.
    float face_cos = 0.999f;

    /// Keep clipped points that are separated by up to this much, relative to
    /// the shapes' size. **Zero keeps only points that actually penetrate.**
    ///
    /// A solver wants a small positive value here — a point that is a tenth of a
    /// millimetre off the surface this frame is a contact next frame, and having
    /// it already in the manifold with its impulse warm is what stops a stack
    /// breathing. 8.9 turns it up; 8.7 leaves it at zero so that every number
    /// this lesson prints is a real overlap.
    float keep_slop = 0.0f;

    /// Reduce to at most `k_max_manifold_points`. Off only for measurement.
    ///
    /// With it off, the first `max_points_unreduced` clipped points are kept in
    /// clip order, which is the naive scheme 8.7 §8 measures the reduction
    /// against.
    bool reduce = true;

    /// How many points to keep when `reduce` is off. Clamped to
    /// `k_max_clip_points`, and it exists so that §7 can look at the WHOLE
    /// clipped set — a manifold that holds four cannot be its own control.
    int max_points_unreduced = k_max_manifold_points;
};

// ---------------------------------------------------------------------------
// Generation
// ---------------------------------------------------------------------------

/// **Build a contact manifold for two overlapping shapes along a known normal.**
///
/// `normal` points from `a` toward `b` and `depth` is the penetration along it —
/// `epa_result::normal` and `epa_result::depth`, though the function neither
/// knows nor cares where they came from. That independence is deliberate and it
/// is what makes 8.7's harness possible: a manifold can be generated along a
/// normal chosen by hand, on shapes whose true contact is known exactly, without
/// EPA in the loop to blame.
///
/// The algorithm is four steps, and §7 walks each of them:
///
///   1. ask each shape for the feature it presents along the normal;
///   2. pick the more face-like of the two as the REFERENCE, the other as the
///      INCIDENT;
///   3. clip the incident feature against the reference face's side planes —
///      Sutherland and Hodgman, which 3.3 already taught for the near plane;
///   4. keep the points that are actually penetrating, and reduce to four.
[[nodiscard]] contact_manifold build_manifold(const convex& a, const convex& b, vec3 normal,
                                              float depth, const manifold_config& cfg = {});

/// GJK, then EPA, then `build_manifold`. The whole narrow phase in one call.
///
/// Returns a manifold with `status == none` and no points when the shapes are
/// apart. **No speculative margin**: a pair that is a micron short of touching
/// produces nothing, which is the honest answer to "are they in contact" and the
/// wrong answer for a solver that would rather see the contact one frame early.
/// 8.9 adds the margin, in the one place that should own it.
[[nodiscard]] contact_manifold collide_manifold(const convex& a, const convex& b,
                                                const manifold_config& cfg = {});

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

/// **Carry last frame's impulses into this frame's manifold, matched on ids.**
///
/// For each point of `fresh`, look for a point of `previous` with an equal
/// `contact_id`; on a match, copy `normal_impulse` and `tangent_impulse` and set
/// `warm`. Returns how many matched, which also lands in
/// `contact_manifold::warm_points`.
///
/// **It is O(16) and it is a linear scan on purpose.** Four against four, in two
/// arrays that are already in cache because the caller just wrote one of them. A
/// map keyed on the id would be correct, slower, and would allocate.
///
/// THE ORDER OF THE TWO ARGUMENTS IS THE ORDER OF THE TWO SHAPES. A manifold
/// generated for `(a, b)` and one generated for `(b, a)` describe the same
/// contact with opposite normals and mirrored ids, and matching one against the
/// other silently produces zero matches — a stack that mysteriously will not
/// settle. `pair_key` below exists to make the canonical order easy to hold.
int carry_impulses(contact_manifold& fresh, const contact_manifold& previous);

/// A key for a pair of bodies, **independent of the order given**.
///
/// The smaller index goes in the high half, so `pair_key(3, 7) == pair_key(7,
/// 3)`. That is what a broadphase needs, because it has no reason to report a
/// pair the same way round twice — and it is also the trap: the KEY is
/// order-independent and the MANIFOLD is not. Generate with the same shape as
/// `a` every frame. 8.8's broadphase adopts the convention "lower index first"
/// for exactly this reason.
[[nodiscard]] std::uint64_t pair_key(std::uint32_t a, std::uint32_t b);

/// **Last frame's manifolds, one per pair.**
///
/// The storage half of persistence. `carry_impulses` knows how to match two
/// manifolds; something has to remember which manifold belonged to which pair,
/// across a frame in which the set of pairs changed completely.
///
/// Open addressing with linear probing, and NO TOMBSTONES: `end_frame` rebuilds
/// the table from the entries that were touched. That is O(n) per frame, which
/// is what walking the pairs costs anyway, and it buys a table that never rots —
/// a deleted-marker scheme degrades until a rehash, and a physics cache deletes
/// on most frames, because pairs are born and die as things move.
///
/// **The frame discipline is three calls**, and skipping the last one is a leak
/// that presents as a slow frame every few seconds:
///
///     cache.begin_frame();
///     for (pair : pairs) {
///         manifold m = collide_manifold(a, b);
///         if (const contact_manifold* old = cache.find(key)) { carry_impulses(m, *old); }
///         solve(m);
///         cache.store(key, m);
///     }
///     cache.end_frame();
class manifold_cache
{
public:
    /// Begin a frame. Everything stored before this call is now stale and will
    /// be dropped by `end_frame` unless it is stored again.
    void begin_frame();

    /// The manifold stored for this pair, or null. Valid until the next `store`.
    [[nodiscard]] const contact_manifold* find(std::uint64_t key) const;

    /// Store this frame's manifold for a pair, replacing any previous one and
    /// marking the entry fresh.
    void store(std::uint64_t key, const contact_manifold& m);

    /// Drop every pair not stored since `begin_frame`. Returns how many went.
    int end_frame();

    /// How many pairs are stored.
    [[nodiscard]] int size() const { return count_; }

    /// How many slots the table has. A power of two, or zero.
    [[nodiscard]] int capacity() const { return static_cast<int>(slots_.size()); }

    /// Forget everything. For a level change, or a teleport.
    void clear();

private:
    struct slot
    {
        std::uint64_t key = 0;
        contact_manifold value{};
        std::uint32_t stamp = 0;
        bool used = false;
    };

    void rehash(std::size_t wanted);

    std::vector<slot> slots_;
    int count_ = 0;
    std::uint32_t frame_ = 1;
};

} // namespace engine::phys
