// engine/include/engine/gfx/debug_lines.hpp — what to draw, said by anybody.
//
// Lesson 5.11. This file is the queue half of the debug-draw rework, and the
// single most important thing about it is what it does NOT include.
//
// ---------------------------------------------------------------------------
// WHAT WAS WRONG WITH THE OLD ONE
// ---------------------------------------------------------------------------
//
// gfx/debug_draw.hpp has existed since Module 3 and it draws IMMEDIATELY:
//
//     line3(fb, a, b, colour, pr);
//
// Every call needs a `framebuffer` and a `projector`, so the only code that can
// draw a debug line is code that already holds the renderer's own state — which
// in practice means the renderer, and nothing else. That is three separate
// problems wearing one coat:
//
//   1 THE CALLER MUST BE THE RENDERER. A collision system that wants to draw a
//     contact normal, an ECS pass that wants to draw the parent link, a loader
//     that wants to mark a bad vertex — none of them has a framebuffer, and none
//     of them should. Handing one down through five layers so the sixth can draw
//     a line is how a debug feature becomes an architecture.
//   2 IT ONLY WORKS ON ONE SURFACE. `framebuffer` is the software renderer's
//     target. On `surface::gpu` there is no framebuffer at all, so every one of
//     those calls is unavailable on the path Module 4 spent nine lessons
//     building.
//   3 A LINE LIVES FOR EXACTLY ONE FRAME. A collision normal exists for one
//     step — 16 ms at 60 Hz — and a human needs something on the order of 250 ms
//     to see that anything happened. **An event that lasts one frame is
//     invisible to the person the debug drawing is for**, which is the entire
//     reason a debug-draw system has lifetimes and the reason it is worth a
//     rework rather than a comment.
//
// One change fixes all three: separate SAYING what to draw from DOING it. This
// file is the saying.
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE INCLUDES, AND WHY THE LIST IS THE POINT
// ---------------------------------------------------------------------------
//
// Math, a colour, and the standard library. No framebuffer, no projector, no
// depth buffer, no viewport, no GPU device — and no `mesh`, which is why
// wire_mesh() below takes two spans rather than the type that owns them.
//
// That list is a promise a physics system can rely on: queueing a debug line
// costs you nothing you did not already have. Put one renderer type in here and
// the promise is gone, because C++ include dependencies are transitive and a
// header is a contract about what you are forced to compile against.
//
// The DOING lives in gfx/debug_draw.hpp, which may include whatever it likes
// because it is the renderer's business. Two files, one direction, and the
// direction is the design.

#pragma once

#include <engine/gfx/colour.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/vec3.hpp>

#include <SDL3/SDL_stdinc.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace engine {

// ---------------------------------------------------------------------------
// The axis colours, in one place at last
// ---------------------------------------------------------------------------
//
// x/y/z = red/green/blue is a course-wide convention (see the Conventions page)
// and until this lesson it was a convention held up by three literals inside
// draw_axes3() and a matching set in every diagram. Now there is one home, and
// draw_axes3() reads from it. The VALUES are unchanged, deliberately: this is a
// consolidation, not a re-colouring, and every existing picture must be provably
// identical afterwards.

inline constexpr Uint32 k_axis_x_colour = pack_argb(236, 92, 92);
inline constexpr Uint32 k_axis_y_colour = pack_argb(122, 196, 152);
inline constexpr Uint32 k_axis_z_colour = pack_argb(126, 162, 236);

/// How long a queued line lives. Zero means "this frame only", and it is the
/// default everywhere because it is what a per-frame visualisation wants: the
/// hierarchy is re-queued every frame from the data, so a stale copy would be
/// worse than no copy.
inline constexpr float k_debug_this_frame = 0.0f;

/// The default cap on a queue, in lines. See `debug_lines` for why there is one.
inline constexpr std::size_t k_default_debug_line_capacity = 4096;

/// One queued line segment, in **world space**.
///
/// World space and not view space, because the whole point is that the queuer
/// does not know where the camera is — and often runs before the camera has been
/// resolved for this frame. The view and projection are applied at the flush, by
/// whoever is drawing, which is also the only code that knows which camera it is
/// drawing for. A split-screen game flushes the same queue twice.
struct debug_line
{
    vec3 a{};
    vec3 b{};

    /// Packed ARGB, the same encoding as everything else that reaches a pixel
    /// (Lesson 1.6). Per-line rather than per-flush: a debug drawing's colours
    /// are its labels, and a system that draws three kinds of thing wants three
    /// colours without three queues.
    Uint32 colour = 0xFFFFFFFFu;

    /// Seconds of life left. **Zero means this is the last frame it is drawn**,
    /// which is what makes the default a single-frame line without any special
    /// case; see `debug_lines::advance`.
    float remaining = k_debug_this_frame;
};

/// A bounded queue of world-space debug lines with lifetimes.
///
/// **It is a value, not a global.** A program holds one; an editor holds one per
/// viewport; a test holds one and never draws it at all. This is the fourth
/// time this course has made that choice for the same reason (`asset_store` in
/// 5.5, `registry` in 5.8, `action_map` in 5.10): the engine does not know how
/// many of a thing a game needs, so it declines to decide. A singleton debug
/// drawer is the single most common shape in hobby engines and it is what makes
/// the editor's second viewport a rewrite.
///
/// **Usage**, once per frame:
///
///     for (each entity with a parent) { dbg_.line(child, parent, colour); }
///     ...
///     engine::draw_debug_lines(fb(), view, pr, dbg_);   // the flush
///     dbg_.advance(time().dt());                        // END of frame
///
/// The order matters and is the most likely thing to get wrong: **advance() runs
/// at the END of the frame, after the flush.** Run it first and every line
/// queued this frame with the default lifetime is deleted before it is drawn —
/// a debug system that draws nothing, which reads as "my code did not run".
class debug_lines
{
public:
    /// @param capacity the hard cap, in lines. Storage is reserved up front, so
    ///                 a frame that queues its usual amount performs no
    ///                 allocation at all.
    explicit debug_lines(std::size_t capacity = k_default_debug_line_capacity);

    // ---- Queueing ----------------------------------------------------------

    /// One segment, world space to world space.
    void line(vec3 a, vec3 b, Uint32 colour, float seconds = k_debug_this_frame);

    /// From a point, along a direction, for its own length. The shape a normal,
    /// a velocity or a force wants — none of which knows where it ends.
    void ray(vec3 origin, vec3 direction, Uint32 colour,
             float seconds = k_debug_this_frame);

    /// A three-arrow triad for a transform, in the course's axis colours.
    ///
    /// Takes the full matrix rather than a position, because the whole value of
    /// a triad is that it shows ORIENTATION and SCALE. The arrows are the
    /// matrix's first three columns, which is Lesson 2.5's picture drawn — and
    /// the reason the basis vectors are sent as directions (w = 0) and the
    /// origin as a position (w = 1).
    void axes(const mat4& world_from_local, float length = 1.0f,
              float seconds = k_debug_this_frame);

    /// An axis-aligned box: 12 edges from a centre and a half-extent.
    void box(vec3 centre, vec3 half_extent, Uint32 colour,
             float seconds = k_debug_this_frame);

    /// An oriented box: the same 12 edges, through a matrix. Module 8's
    /// collision lessons draw a great many of these.
    void box(const mat4& world_from_local, vec3 half_extent, Uint32 colour,
             float seconds = k_debug_this_frame);

    /// Three great circles — xy, yz, zx — which is the cheapest drawing that
    /// reads as a sphere from any angle. `segments` is per circle.
    void sphere(vec3 centre, float radius, Uint32 colour,
                float seconds = k_debug_this_frame, int segments = 24);

    /// Every triangle's three edges, transformed by `world_from_local`.
    ///
    /// **Takes the two arrays, not the `mesh` that owns them**, and that is the
    /// include list at the top of this file being defended: `mesh` lives in
    /// gfx/mesh.hpp, which pulls in core/pool.hpp and the handle system, none of
    /// which a physics system should have to compile to draw a box. A caller
    /// with a mesh writes `q.wire_mesh(m.vertices, m.indices, world, colour)`.
    ///
    /// Shared edges are queued TWICE, exactly as draw_mesh() has drawn them
    /// twice since Lesson 2.8 — 60 lines for the icosahedron's 30 unique edges.
    /// Honest waste, named rather than hidden; deduplicating means building an
    /// edge list, which is real work for a debug view nobody profiles.
    void wire_mesh(std::span<const vec3> vertices,
                   std::span<const std::uint16_t> indices,
                   const mat4& world_from_local, Uint32 colour,
                   float seconds = k_debug_this_frame);

    // ---- The frame ---------------------------------------------------------

    /// Age every line by `dt` and drop the ones whose time is up. **Call this at
    /// the END of the frame, after the queue has been drawn.**
    ///
    /// THE EXPIRY RULE IS DELIBERATELY NOT `remaining -= dt; if (remaining <= 0)
    /// drop;`, and the difference only shows up in the case that matters most for
    /// reproducibility. A line queued with the default lifetime has
    /// `remaining == 0`, and under that rule a frame with `dt == 0` — a paused
    /// clock, a single-frame `--shot`, a step-through in the debugger — would
    /// never drop it. The queue would grow forever in exactly the situations you
    /// use a debug view in.
    ///
    /// So the test comes FIRST: a line whose clock has already reached zero has
    /// had its last frame, whatever `dt` is. The visible consequence is that a
    /// line asked to live two seconds lives two seconds plus one frame, which is
    /// the right way round to be wrong.
    void advance(float dt);

    /// Forget everything, including the drop counter. For a scene change.
    void clear();

    // ---- Reading -----------------------------------------------------------

    /// Every live line. What a backend iterates; also what a test asserts on,
    /// which is why the queue is inspectable without drawing anything.
    [[nodiscard]] std::span<const debug_line> lines() const { return lines_; }

    [[nodiscard]] std::size_t size() const { return lines_.size(); }
    [[nodiscard]] std::size_t capacity() const { return capacity_; }
    [[nodiscard]] bool full() const { return lines_.size() >= capacity_; }

    /// How many lines have been refused since the last clear().
    ///
    /// **A bound with no counter is a bug that looks like a rendering
    /// artifact.** Silently dropping the 4,097th line means a debug view that is
    /// missing exactly the thing you are hunting, with no way to tell that from
    /// the thing not existing. The HUD prints this; so does verify_511.
    [[nodiscard]] std::size_t dropped() const { return dropped_; }

private:
    /// The one place a line is added, so the bound is checked once.
    void push(const debug_line& l);

    std::vector<debug_line> lines_;
    std::size_t capacity_ = k_default_debug_line_capacity;
    std::size_t dropped_ = 0;
};

}   // namespace engine
