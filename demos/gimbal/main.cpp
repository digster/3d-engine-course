// demos/gimbal/main.cpp — three rings, three knobs, and one of them dying.
//
// Lesson 7.1. Every treatment of gimbal lock shows you the rings. Very few show
// you the NUMBER, and the number is the part you can act on: at pitch = ±90° the
// matrix that turns knob rates into angular velocity loses a dimension, and
// `determinant` of it — a function this engine has had since Lesson 2.6 — tells
// you exactly how close you are. This program puts the picture and the number on
// screen at the same time, so that neither has to be taken on trust.
//
// LESSON 7.2 ADDED THE OTHER HALF, and it is the same rig answering a different
// question. Euler's rotation theorem says the pose the three knobs just built is
// ALSO a single turn about a single line; [A] poses the craft that way instead
// and shows the two agree to six decimals, and the teal line through the
// fuselage is that axis — the one direction the whole rotation leaves alone.
// [B] then runs the blend 7.1 indicted: two ghost craft travel from the same
// start to the same end, one interpolating three angles and one turning steadily
// about that single axis. **Watch what stays still.** The slerp ghost rotates
// rigidly about one fixed teal line for the entire blend; the Euler ghost does
// not, and the difference between the two trails is the 25.24° of detour §9
// measures.
//
// LESSON 7.4 ADDED TWO MORE, and both are about things the first three
// representations could not show you because they could not show you an
// *algebra*. [C] performs the same two turns in the two orders and draws both
// answers: at 90 degrees each they end up **120 degrees apart**, which is the
// famous book-flip made into a number you can read off the panel. [D] turns the
// craft steadily about one axis for two full revolutions while a dial beside it
// shows the quaternion doing the same journey at **half the rate** — so the
// craft comes home at 360 degrees and its quaternion does not. That is the
// double cover, and it is the same picture Lesson 7.3's plane demo drew for the
// rotor, one dimension up.
//
// LESSON 7.5 ADDED [S], WHICH IS A DIAL WITH TWO ROWS OF TICKS ON IT. Both rows
// mark the same eleven values of `t` on the same arc: slerp outward, nlerp
// inward. Slerp's are evenly spaced and nlerp's bunch at both ends, which is the
// whole of the difference between them made visible rather than described — the
// PATH is identical and only the SCHEDULE is wrong. A third aircraft, in
// magenta, goes the other way round the same circle: that is the same pair of
// orientations blended without the one comparison `nearest` makes, 210 degrees
// instead of 150. Watch which of the three is the bug you would actually ship.
//
//     cmake --build build --target gimbal
//     ./build/demos/gimbal                                play with it
//     ./build/demos/gimbal --pose 40 89 -25               start at a pose
//     ./build/demos/gimbal --shot scratch/l71_shot.ppm    headless, deterministic
//     ./build/demos/gimbal --blend 0.5 --shot out.ppm     the blend, frozen at t
//     ./build/demos/gimbal --commute 90 --shot out.ppm    both orders, frozen
//     ./build/demos/gimbal --cover 405 --shot out.ppm     past the first return
//     ./build/demos/gimbal --schedule 0.23 --shot out.ppm  slerp vs nlerp, frozen
//
// WHAT TO DO WITH IT, in the order that makes the point:
//
//   1. Hold [E] — the JOINT knob, which drives yaw and roll at the same rate.
//      At pitch 0 the craft spins briskly. Press [Up] to climb toward pitch 90
//      and hold [E] again: the rings still turn, the readout still says the
//      knobs are moving, and THE CRAFT STOPS. That is the degree of freedom
//      dying, and it dies gradually rather than at a cliff.
//   2. Watch `|det J|` while you do it. It is 1 when the three knobs are
//      independent and 0 at lock, and it is `cos(pitch)` — which is a fact you
//      can check against the pitch readout beside it.
//   3. Press [K] to snap to pitch 90 and look at the axles. The red axle
//      (pitch) is still its own; the blue axle (roll) has swung round until it
//      lies along the green one (yaw). Two rings, one axis.
//
// THE RULES ARE 5.1'S. Nothing here includes an engine internal, and where the
// public API cannot do something this file does it in the open with a comment
// saying so. There is one such place — §RING below — and it is deliberate.

#include <engine/core/actions.hpp>
#include <engine/ecs/camera.hpp>
#include <engine/ecs/hierarchy.hpp>
#include <engine/ecs/registry.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/debug_draw.hpp>
#include <engine/gfx/debug_lines.hpp>
#include <engine/gfx/depth_buffer.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/material.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/projector.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/gfx/renderable.hpp>
#include <engine/gfx/scene.hpp>
#include <engine/gfx/soft_renderer.hpp>
#include <engine/gfx/viewport.hpp>
#include <engine/math/axis_angle.hpp>
#include <engine/math/euler.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/rotation.hpp>
#include <engine/math/transform.hpp>
#include <engine/platform/app.hpp>
#include <engine/ui/debug_ui.hpp>

#include <utility>

#include <imgui.h>

#include <engine/platform/main.hpp>

#include <cmath>
#include <cstdio>
#include <numbers>
#include <vector>

namespace {

using engine::ecs::entity;
using engine::euler_angles;
using engine::mat3;
using engine::mat4;
using engine::quat;
using engine::vec3;

constexpr int k_width = 960;
constexpr int k_height = 540;

constexpr float k_pi = std::numbers::pi_v<float>;
constexpr float k_deg = 180.0f / k_pi;
constexpr float k_rad = k_pi / 180.0f;

/// How fast a held key turns a knob, in degrees per second. Slow enough that you
/// can stop on a chosen pitch, fast enough that a full sweep is not a chore.
constexpr float k_knob_rate = 45.0f;

constexpr Uint32 k_background = engine::pack_argb(16, 18, 24);
constexpr Uint32 k_ring_yaw   = engine::k_axis_y_colour;   ///< green: turns about +Y
constexpr Uint32 k_ring_pitch = engine::k_axis_x_colour;   ///< red:   turns about +X
constexpr Uint32 k_ring_roll  = engine::k_axis_z_colour;   ///< blue:  turns about +Z
constexpr Uint32 k_nose       = engine::pack_argb(248, 214, 120);
constexpr Uint32 k_trail      = engine::pack_argb(120, 132, 168);
// DELIBERATELY NOT RED. The first draft drew this arrow in red and it was
// indistinguishable from the pitch ring it crosses — and worse, red MEANS the x
// axis course-wide (Conventions §10), so a red arrow that is not an x axis is a
// convention violation as well as a legibility bug. Magenta is used nowhere else
// in the engine's debug palette.
constexpr Uint32 k_dead       = engine::pack_argb(214, 118, 226);

// LESSON 7.2's THREE. The same constraint applies as to `k_dead` above: red,
// green and blue MEAN x, y and z course-wide (Conventions §10) and the three
// rings already use them, so nothing new may be any of those.
//
// Teal for the rotation axis, which is the one line a rotation leaves alone. It
// is the only cool colour on screen that is not the blue roll ring, and the two
// are never drawn together — [B] hides the rings, and outside a blend the axis
// passes through the fuselage rather than round it.
constexpr Uint32 k_axis         = engine::pack_argb(96, 222, 208);
// The two blend ghosts, deliberately the SAME two colours Lesson 7.1's figure 7
// used for the same two paths: amber is the Euler lerp, pale blue the geodesic.
// A reader who has seen the figure should not have to relearn the code.
constexpr Uint32 k_ghost_euler  = engine::pack_argb(236, 168, 86);
constexpr Uint32 k_ghost_slerp  = engine::pack_argb(126, 188, 248);

// LESSON 7.4. [C] REUSES THE TWO GHOST COLOURS rather than inventing two more,
// and the reuse is a claim: amber and pale blue have meant "two routes to the
// same place, compared" since 7.1's figure 7, and two composition orders are
// exactly that. The difference is that in [B] one of them was wrong and in [C]
// neither is — they are two different rotations, both correct, and that is the
// lesson.
//
// [D] needs one new colour and takes it from Lesson 7.3's plane demo, where
// gold meant the ROTOR: the half-angle object a rotation is built out of. It
// means the same thing here, because it IS the same thing with one more
// imaginary unit. The dial's rim is the trail grey, so that the hand is the
// only bright thing on it.
constexpr Uint32 k_rotor        = engine::pack_argb(248, 214, 120);

/// Where the [D] dial sits, in world space, and how big it is.
///
/// Off to the reader's left and slightly below the craft, in the x-y plane so
/// that it faces the camera squarely and its angles can be read with a
/// protractor held against the screen — the same property Lesson 7.3's figures
/// had for free and that every other figure in Module 7 has had to give up.
constexpr vec3 k_dial_centre{-3.9f, -0.6f, 0.0f};
constexpr float k_dial_radius = 1.45f;

/// Where the eye sits during [D], named once so that the dial can face it.
///
/// A dial drawn in a FIXED world plane is an ellipse from any camera that is
/// not square on to it, and the first version of this mode drew exactly that —
/// a circle whose angles could not be read off the screen, which defeats the
/// purpose of putting a protractor-readable picture next to an object whose
/// rotation you cannot measure by eye. The dial is built in the plane facing
/// this point instead, so it is a true circle and 45 degrees on it is 45
/// degrees on the page. Lesson 7.3's figures had that property for free,
/// because they were in the plane.
constexpr vec3 k_cover_eye{3.10f, 2.70f, 6.95f};

/// The axis [D] turns about. Deliberately NOT a coordinate axis: a craft
/// spinning about +Y looks the same at 0 and 360 degrees for a reason that has
/// nothing to do with quaternions (it is a symmetry of the picture), and the
/// mode's claim would be unfalsifiable. Tilted, every pose in the lap is
/// visibly distinct and "it is home again" is something you can see.
const vec3 k_cover_axis = engine::normalised(vec3{0.32f, 0.86f, 0.40f});

// LESSON 7.5 — [S]: the schedule, and the sign.
//
// **150 DEGREES, AND THE NUMBER IS CHOSEN RATHER THAN PICKED.** The gap between
// nlerp and slerp grows with the arc, and the shortest-arc rule caps a rotation
// arc at 180 — so 150 is near the worst case this mode can honestly show while
// leaving 30 degrees of headroom to make clear that the cap is a cap and not the
// end of the axis. The long way round is then 210, which is visibly the long way
// rather than ambiguously so.
constexpr float k_sched_arc = 150.0f * k_rad;

/// The axis both interpolations turn about — **pointed at the camera**.
///
/// This is the same drawing rule Lesson 7.4's dial had to learn the hard way. A
/// circle drawn in a fixed world plane is seen obliquely and renders as an
/// ELLIPSE, and an ellipse's arc lengths are not proportional to its angles — so
/// the eleven ticks that are this mode's entire argument would be compressed at
/// the ends by perspective and not by nlerp, and no reader could tell which. Put
/// the rotation axis along the view direction and the nose's path is a circle on
/// screen, where equal angles really are equal distances and the spacing can be
/// checked against a ruler.
///
/// The value is `k_sched_eye` normalised, and the two must move together.
const vec3 k_sched_axis = engine::normalised(vec3{2.55f, 2.10f, 3.45f});

/// How many evenly spaced ticks are laid along the arc.
///
/// **THE TICKS ARE THE MODE.** Two aircraft 4.5 degrees apart is a difference you
/// have to be told about; eleven marks at equal `t`, bunched at the ends under
/// one rule and evenly spread under the other, is a difference you can see from
/// across the room. It is the same trick a speedometer uses.
constexpr int k_sched_ticks = 11;

/// The radius the nose direction is plotted at.
///
/// The craft is 2.5 units across, so anything under about 2 puts the arc inside
/// the aircraft and the whole picture becomes a thicket — which is what the first
/// version of this mode looked like, and it read as a bug rather than as a
/// framing mistake.
constexpr float k_sched_reach = 2.45f;

/// How long a tick is, in world units.
///
/// **BOTH ROWS TOUCH THE SAME CIRCLE**, one pointing out and one pointing in, and
/// that is a claim rather than a layout. Slerp and nlerp trace the *same path*;
/// drawing the second row on a smaller circle would say they do not, which is the
/// single most common misconception about nlerp and the one this mode exists to
/// kill. One road, two rulers.
constexpr float k_sched_tick = 0.32f;

/// The craft's nose in its own frame. Lesson 2.9's convention: a camera and a
/// craft both look down their own −z.
constexpr vec3 k_nose_dir{0.0f, 0.0f, -1.0f};

/// Where [S] puts the eye. `k_sched_axis` is this, normalised; see there.
constexpr vec3 k_sched_eye{2.55f, 2.10f, 3.45f};

/// The third craft: the same two poses, blended the way round that `nearest`
/// exists to avoid. Magenta, because `k_dead` already means "this is the failure"
/// in this program and it is not drawn in any mode [S] can be in.
constexpr Uint32 k_ghost_long = k_dead;

// ---------------------------------------------------------------------------
// §RING — the one thing the public API cannot do, done here in the open
// ---------------------------------------------------------------------------
//
// `debug_lines` can draw a line, a ray, a box, a sphere and a wire mesh (5.11).
// It cannot draw a CIRCLE in an arbitrary plane, which is what a gimbal ring is.
// `sphere()` comes close — it is three great circles — but its circles are
// axis-aligned in world space and a gimbal ring is not: the whole point is that
// the ring's plane has been carried by every rotation outside it.
//
// So this file draws its own, out of `line()` calls, and the finding is recorded
// rather than quietly worked around (5.12's rule 2). It is a real gap and a small
// one: an `ellipse(centre, u, v, colour)` taking two in-plane vectors would cover
// rings, orbits, cones and the FOV arcs a camera gizmo wants. Filed for 9.7,
// where the editor's gizmos need exactly this.

/// A circle of `segments` chords, centred at the origin of `frame`, spanned by
/// the two in-plane directions `u` and `v` expressed in `frame`.
///
/// A gimbal ring's plane always CONTAINS its own pivot axis — that is what makes
/// it a ring on bearings rather than a turntable, and it is why the ring visibly
/// tips when its knob turns. So `u` is always the pivot axis and `v` is the next
/// axis down the chain.
void ring(engine::debug_lines& out, const mat3& frame, vec3 u, vec3 v, float radius,
          Uint32 colour, int segments = 64)
{
    vec3 previous = frame * (u * radius);
    for (int i = 1; i <= segments; ++i)
    {
        const float a = 2.0f * k_pi * static_cast<float>(i) / static_cast<float>(segments);
        const vec3 current = frame * ((u * std::cos(a) + v * std::sin(a)) * radius);
        out.line(previous, current, colour);
        previous = current;
    }

    // The axle: the pivot diameter, drawn slightly proud of the ring so that two
    // axles lining up at lock is unmissable. This is the line to watch.
    const vec3 axle = frame * (u * (radius * 1.18f));
    out.line(-axle, axle, colour);
}

// ---------------------------------------------------------------------------
// The measurements, computed from the public API and from nothing else
// ---------------------------------------------------------------------------

/// Everything the readout shows, derived from one pose.
struct conditioning
{
    float det_j = 1.0f;       ///< determinant of the rate Jacobian = -cos(pitch)
    float sigma_min = 1.0f;   ///< weakest gain: body rad/s per unit of knob rad/s
    float cost = 1.0f;        ///< knob rates needed for 1 rad/s in the weak direction
    vec3 weak_knobs{};        ///< the knob combination that does least
    vec3 dead_axis{};         ///< the world axis becoming unreachable, at lock
};

conditioning measure(euler_angles e)
{
    conditioning c;
    const mat3 j = engine::euler_rate_jacobian(e);
    c.det_j = engine::determinant(j);

    // sigma_min = |cos p| / sqrt(1 + |sin p|), and NOT the algebraically equal
    // sqrt(1 - |sin p|). The second subtracts two nearly-equal numbers and hits
    // exactly zero at pitch 89.99°, which is a hundredth of a degree of working
    // range thrown away for nothing — and worse, it throws it away by reporting
    // the value every guard downstream tests for. Lesson 7.1 §6.2 measures both.
    const float sp = std::fabs(std::sin(e.pitch));
    const float cp = std::fabs(std::cos(e.pitch));
    c.sigma_min = cp / std::sqrt(1.0f + sp);
    c.cost = (c.sigma_min > 0.0f) ? 1.0f / c.sigma_min : 0.0f;

    // The weakest knob combination. At pitch > 0 the yaw and roll axles converge,
    // so turning both the same way does least; below zero they diverge and it is
    // turning them opposite ways. Normalised, so the HUD reads as a direction.
    const float same_way = (std::sin(e.pitch) >= 0.0f) ? 1.0f : -1.0f;
    c.weak_knobs = engine::normalised(vec3{1.0f, 0.0f, same_way});

    // Where that combination points once J has acted on it. At lock it is the
    // zero vector — nothing at all — which is exactly the claim; away from lock
    // it is a real axis and the arrow on screen is a live one.
    c.dead_axis = j * c.weak_knobs;
    return c;
}

// ---------------------------------------------------------------------------
// Lesson 7.2 — the single turn, and the two ways to travel it
// ---------------------------------------------------------------------------

/// The two poses the blend runs between, which are **Lesson 7.1's generic pair**,
/// unchanged, so that the numbers on screen are the numbers on the page.
constexpr euler_angles k_blend_from{-70.0f * k_rad, -35.0f * k_rad, 20.0f * k_rad};
constexpr euler_angles k_blend_to{85.0f * k_rad, 55.0f * k_rad, -60.0f * k_rad};

/// Total turning performed along a path, and the minimum it could have been.
///
/// `angle_between_rotations` is the metric (`math/rotation.hpp`), so this is a
/// sum of geodesic steps — the length of the route actually driven. The geodesic
/// between the endpoints is one call to the same function, and the ratio is the
/// detour. Computed ONCE when a blend starts, at a fixed step count, rather than
/// accumulated per frame: a per-frame sum measures the frame rate as much as the
/// path, and two runs of the same demo would print different numbers.
struct blend_cost
{
    float euler_path = 0.0f;
    float slerp_path = 0.0f;
    float geodesic = 0.0f;
};

template <typename Fn>
float path_length(Fn orientation_at, int steps)
{
    float total = 0.0f;
    mat3 previous = orientation_at(0.0f);
    for (int i = 1; i <= steps; ++i)
    {
        const mat3 current = orientation_at(static_cast<float>(i) / static_cast<float>(steps));
        total += engine::angle_between_rotations(previous, current);
        previous = current;
    }
    return total;
}

euler_angles lerp_angles(euler_angles a, euler_angles b, float t)
{
    return {a.yaw + (b.yaw - a.yaw) * t,
            a.pitch + (b.pitch - a.pitch) * t,
            a.roll + (b.roll - a.roll) * t};
}

blend_cost measure_blend()
{
    const mat3 from = engine::rotation_from_euler(k_blend_from);
    const mat3 to = engine::rotation_from_euler(k_blend_to);
    blend_cost c;
    c.euler_path = path_length(
        [&](float t) { return engine::rotation_from_euler(lerp_angles(k_blend_from, k_blend_to, t)); },
        2048);
    c.slerp_path = path_length(
        [&](float t) { return engine::rotation_slerp(from, to, t); }, 2048);
    c.geodesic = engine::angle_between_rotations(from, to);
    return c;
}

// ---------------------------------------------------------------------------
// LESSON 7.4 — the two things an algebra can show that a matrix cannot
// ---------------------------------------------------------------------------

/// The two orders of the same pair of turns, and how far apart they land.
///
/// **Both turns are of the same size `phi`**, one about +Y and one about +X, so
/// there is exactly one number to move and the whole family is one slider. That
/// is not a simplification: two equal perpendicular turns are the case everyone
/// already knows by heart, because it is the book you turn twice on a desk.
struct commutation
{
    quat first{};     ///< yaw after pitch:  quat_y(phi) * quat_x(phi)
    quat second{};    ///< pitch after yaw:  quat_x(phi) * quat_y(phi)
    float gap = 0.0f;         ///< radians between the two, from the quaternions
    float gap_matrix = 0.0f;  ///< the same, from the two matrices — a control
    float closed_form = 0.0f; ///< 2 acos|c^4 + 2 c^2 s^2 - s^4|, derived
};

/// **Derived and then checked, on screen, every frame.** With
/// `c = cos(phi/2)` and `s = sin(phi/2)`,
///
///     quat_y(phi) quat_x(phi) = (c^2,  cs x + cs y - s^2 z)
///     quat_x(phi) quat_y(phi) = (c^2,  cs x + cs y + s^2 z)
///
/// — identical but for the sign of the z component, which is the cross product
/// and nothing else. Their four-component dot product is therefore
/// `c^4 + 2 c^2 s^2 - s^4`, and the angle between them is twice its arccosine.
/// At `phi = 90` that is `2 acos(1/2) = 120` degrees exactly.
[[nodiscard]] commutation measure_commutation(float phi)
{
    commutation out;
    out.first = engine::quat_y(phi) * engine::quat_x(phi);
    out.second = engine::quat_x(phi) * engine::quat_y(phi);
    out.gap = engine::angle_between(out.first, out.second);

    // THE CONTROL, and it is free. The same question asked of the two matrices,
    // through machinery written in Lesson 7.1 that has never heard of a
    // quaternion. If these two columns ever disagreed, one of the two products
    // would be wrong and the picture would be a confident lie.
    out.gap_matrix = engine::angle_between_rotations(
        engine::mat3_from_quat(out.first), engine::mat3_from_quat(out.second));

    const float c = std::cos(phi * 0.5f);
    const float sn = std::sin(phi * 0.5f);
    const float d = c * c * c * c + 2.0f * c * c * sn * sn - sn * sn * sn * sn;
    out.closed_form = 2.0f * std::acos(std::clamp(std::fabs(d), 0.0f, 1.0f));
    return out;
}

// ---------------------------------------------------------------------------
// The app
// ---------------------------------------------------------------------------

class gimbal_app final : public engine::app
{
public:
    [[nodiscard]] engine::app_config configure(int argc, char* argv[]) override
    {
        for (int i = 1; i < argc; ++i)
        {
            if (SDL_strcmp(argv[i], "--shot") == 0 && i + 1 < argc) { shot_path_ = argv[++i]; }
            else if (SDL_strcmp(argv[i], "--pose") == 0 && i + 3 < argc)
            {
                pose_.yaw = static_cast<float>(SDL_atof(argv[++i])) * k_rad;
                pose_.pitch = static_cast<float>(SDL_atof(argv[++i])) * k_rad;
                pose_.roll = static_cast<float>(SDL_atof(argv[++i])) * k_rad;
            }
            else if (SDL_strcmp(argv[i], "--no-rings") == 0) { rings_ = false; }
            else if (SDL_strcmp(argv[i], "--axis-angle") == 0) { single_turn_ = true; }
            else if (SDL_strcmp(argv[i], "--blend") == 0 && i + 1 < argc)
            {
                // FROZEN at the given t rather than started, so that a `--shot`
                // of a blend is reproducible. A running blend advances in
                // `on_fixed_step`, which a headless run never calls.
                blending_ = true;
                rings_ = false;
                blend_t_ = std::clamp(static_cast<float>(SDL_atof(argv[++i])), 0.0f, 1.0f);
                cost_ = measure_blend();
                // AND THE TRAILS ARE WALKED IN, not left empty. A headless run
                // never calls `on_fixed_step`, so a frozen blend would otherwise
                // draw two aircraft and no history — a picture of a moment rather
                // than of a journey, and the journey is the entire point. Same
                // step count either way, so the still and the live view agree.
                for (int k = 0; k <= 600; ++k)
                {
                    const float t = blend_t_ * static_cast<float>(k) / 600.0f;
                    record_nose(ghost_euler_, euler_at(t));
                    record_nose(ghost_slerp_, slerp_at(t));
                }
            }
            // LESSON 7.4. Both frozen at a given angle rather than started, for
            // the reason `--blend` gives: `on_fixed_step` never runs headless.
            //
            // ONE NAMED LOCAL PER ARGUMENT, and it is not a style preference.
            // `SDL_clamp` is a MACRO that expands its first argument three
            // times, so `SDL_clamp(SDL_atof(argv[++i]), ...)` advances `i`
            // three times and swallows the next two arguments — which is how
            // Lesson 7.3's plane demo spent an afternoon opening a window
            // during a headless run. `std::clamp` is a function and evaluates
            // once; the rule below survives either.
            else if (SDL_strcmp(argv[i], "--commute") == 0 && i + 1 < argc)
            {
                const float degrees = static_cast<float>(SDL_atof(argv[++i]));
                commuting_ = true;
                rings_ = false;
                commute_phi_ = std::clamp(degrees, 0.0f, 180.0f) * k_rad;
            }
            else if (SDL_strcmp(argv[i], "--cover") == 0 && i + 1 < argc)
            {
                const float degrees = static_cast<float>(SDL_atof(argv[++i]));
                covering_ = true;
                rings_ = false;
                cover_turn_ = std::clamp(degrees, 0.0f, 720.0f) * k_rad;
            }
            // LESSON 7.5. Frozen at `t`, and the trails walked in, for exactly
            // the reasons `--blend` gives above: `on_fixed_step` never runs in a
            // headless capture, so a still would otherwise show three aircraft
            // and no journey.
            else if (SDL_strcmp(argv[i], "--schedule") == 0 && i + 1 < argc)
            {
                const double given = SDL_atof(argv[++i]);
                scheduling_ = true;
                rings_ = false;
                schedule_t_ = std::clamp(static_cast<float>(given), 0.0f, 1.0f);
                for (int k = 0; k <= 600; ++k)
                {
                    const float t = schedule_t_ * static_cast<float>(k) / 600.0f;
                    record_nose(ghost_nlerp_, engine::quat_nlerp(sched_from(), sched_to(), t));
                    record_nose(ghost_sched_, engine::quat_slerp(sched_from(), sched_to(), t));
                    record_nose(ghost_long_, sched_long_at(t));
                }
            }
        }

        return {.title = "gimbal — watch a degree of freedom die",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    [[nodiscard]] bool on_start() override
    {
        declare_actions();

        mesh_box_ = meshes_.insert(
            engine::with_normals(engine::cube_mesh(), engine::normal_style::flat));

        lights_.key.direction = engine::normalised(vec3{-0.42f, -0.66f, -0.62f});
        lights_.key.colour = {1.0f, 0.97f, 0.92f};
        lights_.key.irradiance = engine::k_reference_irradiance;
        lights_.ambient = {0.13f, 0.14f, 0.19f};

        build_craft();
        build_camera();
        (void)tree_.rebuild_and_resolve(world_);

        (void)ui_.start(window(), renderer());
        return true;
    }

    void on_input() override
    {
        ui_.begin_frame();
        gate_.update(in(), ui_.wants_keyboard(), ui_.wants_mouse());
        actions_.update(gate_);

        if (actions_.pressed(a_quit_))  { request_quit(); }
        if (actions_.pressed(a_reset_)) { pose_ = {}; trail_.clear(); }
        if (actions_.pressed(a_lock_))
        {
            // Snap to the singularity rather than creeping up on it, because the
            // interesting thing about lock is the CONFIGURATION and it is
            // fiddly to hit by hand. Yaw and roll are kept, so the craft's
            // orientation changes as little as the pose allows.
            pose_.pitch = (pose_.pitch >= 0.0f) ? 90.0f * k_rad : -90.0f * k_rad;
            trail_.clear();
        }
        if (actions_.pressed(a_rings_)) { rings_ = !rings_; }
        if (actions_.pressed(a_trail_)) { trail_on_ = !trail_on_; trail_.clear(); }
        if (actions_.pressed(a_single_)) { single_turn_ = !single_turn_; }
        if (actions_.pressed(a_blend_))
        {
            blending_ = !blending_;
            blend_t_ = 0.0f;
            blend_phase_ = 0.0f;
            ghost_euler_.clear();
            ghost_slerp_.clear();
            // The rings belong to the three-knob story and only clutter this one.
            if (blending_) { cost_ = measure_blend(); rings_ = false; commuting_ = false;
                             covering_ = false; scheduling_ = false; }
            reframe();
        }

        // LESSON 7.4. THE THREE COMPARISON MODES ARE MUTUALLY EXCLUSIVE and the
        // exclusion is written out three times rather than factored into a
        // `mode` enum. That is a deliberate refusal: an enum here would be the
        // right shape for a program with five modes and the wrong one for a
        // teaching demo where each branch's condition is part of what the
        // reader is being shown. 9.x's editor is where a mode stack earns its
        // keep; three booleans and a rule are honest at this size.
        if (actions_.pressed(a_commute_))
        {
            commuting_ = !commuting_;
            commute_phase_ = 0.0f;
            if (commuting_) { blending_ = false; covering_ = false; scheduling_ = false;
                              rings_ = false; }
            reframe();
        }
        if (actions_.pressed(a_cover_))
        {
            covering_ = !covering_;
            cover_phase_ = 0.0f;
            cover_turn_ = 0.0f;
            if (covering_) { blending_ = false; commuting_ = false; scheduling_ = false;
                             rings_ = false; }
            reframe();
        }

        // LESSON 7.5. The fourth comparison mode, and the note above about not
        // factoring these into an enum now has one more repetition arguing
        // against it. It is still the right call at four and it would not be at
        // six; the line is somewhere around five, and saying so is more useful
        // than pretending there is a rule.
        if (actions_.pressed(a_sched_))
        {
            scheduling_ = !scheduling_;
            schedule_phase_ = 0.0f;
            schedule_t_ = 0.0f;
            ghost_sched_.clear();
            ghost_nlerp_.clear();
            ghost_long_.clear();
            if (scheduling_) { blending_ = false; commuting_ = false; covering_ = false;
                               rings_ = false; }
            reframe();
        }
    }

    /// Put the eye where the current mode wants it.
    ///
    /// Extracted in 7.4 because there are now four framings and three places
    /// that change mode, and the first draft of [D] forgot one of them — the
    /// dial was simply off the left edge, which looks exactly like a mode that
    /// does not work.
    void reframe()
    {
        if (engine::transform* eye = world_.get<engine::transform>(camera_))
        {
            *eye = camera_placement();
        }
    }

    void on_fixed_step(float h) override
    {
        if (shot_path_ != nullptr) { return; }
        if (blending_) { drive_blend(h); return; }
        if (commuting_) { drive_commute(h); return; }
        if (covering_) { drive_cover(h); return; }
        if (scheduling_) { drive_schedule(h); return; }
        drive_knobs(h);
    }

    void on_frame(float alpha) override
    {
        // Not interpolated, for the same reason `collector` gives: the knobs move
        // at 45°/s and the fixed step is 60 Hz, so a frame is at most 0.75° stale.
        (void)alpha;

        fb().clear(k_background);
        depth_.clear();

        // ---- Pose the craft. ONE LINE, and it is the whole lesson ----------
        //
        // `rotation_from_euler` is the entire interface between three numbers a
        // human can type and the matrix the renderer multiplies by. Everything
        // else in this file is a way of looking at what that line just did.
        if (engine::transform* body = world_.get<engine::transform>(craft_))
        {
            // LESSON 7.2's ONE LINE, next to Lesson 7.1's. The two build the same
            // matrix by completely different routes — three elementary turns
            // composed, against one turn about one axis — and the panel prints
            // the angle between them, which is how you know rather than hope.
            // During a blend the craft is the slerp ghost's solid twin.
            //
            // AND LESSON 7.4 ADDS THE THIRD ROUTE TO THE SAME MATRIX. [D] poses
            // the craft from a QUATERNION — `mat3_from_quat(quat_from_axis_angle(...))`
            // — so the solid aircraft on screen during the double-cover mode is
            // drawn by this lesson's code and not by 7.1's or 7.2's. Three
            // representations, one renderer, and the renderer cannot tell.
            // **LESSON 7.5: ALL FIVE ROUTES NOW END IN THE SAME FOUR FLOATS.**
            // `transform::rotation` is a `quat`, so the two that were built as
            // matrices are narrowed here rather than handed over whole — which is
            // not a loss, because `quat_from_rotation` has no bad case (7.4 §9)
            // and because `parent_from_local` was going to build a matrix again
            // anyway. The renderer still cannot tell which route drew it.
            body->rotation = scheduling_ ? schedule_pose()
                           : blending_ ? engine::quat_from_rotation(slerp_pose())
                           : covering_ ? cover_quat()
                           : single_turn_ ? engine::quat_from_axis_angle(single_turn().value)
                                          : engine::quat_from_euler(pose_);
        }
        (void)tree_.rebuild_and_resolve(world_);

        const engine::ecs::world_transform* cam_placement =
            world_.get<engine::ecs::world_transform>(camera_);
        const engine::ecs::camera* cam = world_.get<engine::ecs::camera>(camera_);
        if (cam_placement == nullptr || cam == nullptr) { return; }

        const mat4 view = engine::ecs::view_from_camera(*cam_placement);
        const vec3 eye = engine::ecs::eye_of(*cam_placement);
        const engine::projector proj{
            engine::ecs::projection_of(*cam, static_cast<float>(k_width)
                                                 / static_cast<float>(k_height)),
            engine::viewport{0.0f, 0.0f, static_cast<float>(k_width),
                             static_cast<float>(k_height), 0.0f, 1.0f},
            engine::near_mode::clip};

        // NOTHING SOLID DURING A BLEND, and it is not a performance choice. The
        // blend's content is two nose TRAILS a pixel wide, and a solid aircraft
        // sitting between them and the camera hides the part of each trail that
        // passes behind it — including, at t = 1, most of the gap that is the
        // whole measurement. Two wireframes and two trails on an empty ground is
        // also simply the better picture: there is nothing in it that is not the
        // comparison.
        objects_.clear();
        collect_ = (blending_ || commuting_ || scheduling_) ? engine::renderable_report{}
                             : engine::collect_renderables(world_, meshes_, objects_);

        const engine::render_options opts{.cull = engine::cull_choice::back,
                                          .normals = engine::normal_source::vertex,
                                          .shading = engine::shade_eval::gouraud};
        engine::collect_triangles(triangles_, scratch_, objects_, meshes_, {view, eye}, proj,
                                  lights_, opts);

        const engine::fill_style style{.shade = engine::shading::vertex_colour,
                                       .cull = engine::cull_mode::back,
                                       .eye = eye};
        engine::draw_triangles(fb(), &depth_, triangles_, false, style);

        debug_.clear();
        if (blending_) { queue_blend(); }
        else if (commuting_) { queue_commute(); }
        else if (covering_) { queue_cover(); }
        else if (scheduling_) { queue_schedule(); }
        else
        {
            if (rings_) { queue_rings(); }
            queue_nose_and_axis();
            queue_axis(single_turn(), 2.9f);
            if (trail_on_) { queue_trail(); }
        }
        debug_drawn_ = engine::draw_debug_lines(fb(), view, proj, debug_);
        debug_.advance(time().dt());

        if (shot_path_ != nullptr) { write_shot(); }
    }

    void on_overlay() override
    {
        // GUARDED, because a `--shot` run has no window and therefore no ImGui
        // context, and `ImGui::Begin` on no context is a segfault rather than a
        // no-op. `debug_ui`'s own calls are all safe when it never started (5.11
        // made them so on purpose); a panel built by hand between them is not,
        // and the first headless run of this demo found that out.
        if (ui_.running()) { build_panel(); }
        ui_.render();
    }

    void on_event(const SDL_Event& event) override { (void)ui_.handle_event(event); }

    void on_stop() override { ui_.stop(); }

private:
    // ---- Setup -------------------------------------------------------------

    void declare_actions()
    {
        a_yaw_   = actions_.declare("yaw");
        a_pitch_ = actions_.declare("pitch");
        a_roll_  = actions_.declare("roll");
        a_joint_ = actions_.declare("joint");
        a_lock_  = actions_.declare("snap_to_lock");
        a_reset_ = actions_.declare("reset");
        a_rings_ = actions_.declare("rings");
        a_trail_ = actions_.declare("trail");
        a_single_ = actions_.declare("single_turn");
        a_blend_ = actions_.declare("blend");
        a_commute_ = actions_.declare("commute");
        a_cover_ = actions_.declare("double_cover");
        a_sched_ = actions_.declare("schedule");
        a_quit_  = actions_.declare("quit");

        (void)actions_.bind_key(a_yaw_, SDL_SCANCODE_LEFT, +1.0f);
        (void)actions_.bind_key(a_yaw_, SDL_SCANCODE_RIGHT, -1.0f);
        (void)actions_.bind_key(a_pitch_, SDL_SCANCODE_UP, +1.0f);
        (void)actions_.bind_key(a_pitch_, SDL_SCANCODE_DOWN, -1.0f);
        (void)actions_.bind_key(a_roll_, SDL_SCANCODE_Q, +1.0f);
        (void)actions_.bind_key(a_roll_, SDL_SCANCODE_W, -1.0f);
        (void)actions_.bind_key(a_joint_, SDL_SCANCODE_E);
        (void)actions_.bind_key(a_lock_, SDL_SCANCODE_K);
        (void)actions_.bind_key(a_reset_, SDL_SCANCODE_R);
        (void)actions_.bind_key(a_rings_, SDL_SCANCODE_G);
        (void)actions_.bind_key(a_trail_, SDL_SCANCODE_T);
        (void)actions_.bind_key(a_single_, SDL_SCANCODE_A);
        (void)actions_.bind_key(a_blend_, SDL_SCANCODE_B);
        (void)actions_.bind_key(a_commute_, SDL_SCANCODE_C);
        (void)actions_.bind_key(a_cover_, SDL_SCANCODE_D);
        (void)actions_.bind_key(a_sched_, SDL_SCANCODE_S);
        (void)actions_.bind_key(a_quit_, SDL_SCANCODE_ESCAPE);
    }

    /// A craft made of three boxes, parented to one body.
    ///
    /// Three parts rather than one, because a single box has a four-fold symmetry
    /// about each axis and you genuinely cannot see a 90° roll on it. A fuselage,
    /// a wing and a fin break every symmetry there is, and the orientation reads
    /// at a glance — which is the only thing this demo has to get right.
    void build_craft()
    {
        craft_ = world_.create();
        engine::ecs::add_hierarchy_components(world_, craft_, engine::transform{});

        // Sizes are FULL EXTENTS, because `cube_mesh()` spans +/-0.5 and a
        // transform's scale therefore multiplies the whole width. Lesson 5.12
        // got this wrong three times in one file by reaching for the half-extent
        // that everything else in a game has in its hand.
        add_part({0.0f, 0.0f, 0.0f}, {0.46f, 0.46f, 2.30f}, engine::pack_argb(206, 210, 220));
        add_part({0.0f, 0.0f, 0.22f}, {2.50f, 0.14f, 0.62f}, engine::pack_argb(120, 158, 226));
        add_part({0.0f, 0.50f, 0.92f}, {0.11f, 0.82f, 0.50f}, engine::pack_argb(226, 132, 110));
    }

    void add_part(vec3 offset, vec3 size, Uint32 tint)
    {
        const entity e = world_.create();
        engine::ecs::add_hierarchy_components(
            world_, e, engine::transform{.position = offset, .scale = size});
        world_.add<engine::renderable>(
            e, engine::renderable{.mesh = mesh_box_,
                                  .mat = {.tint = tint, .surface = {.roughness = 0.55f}},
                                  .closed = true});
        (void)engine::ecs::set_parent(world_, e, craft_);
        tree_.mark_topology_changed();
    }

    void build_camera()
    {
        camera_ = world_.create();
        engine::ecs::add_hierarchy_components(world_, camera_, camera_placement());
        world_.add<engine::ecs::camera>(camera_, engine::ecs::camera{.fovy = 42.0f * k_rad});
    }

    /// Where the eye sits, which depends on what is being looked at.
    ///
    /// The knob view has to frame the outermost ring at radius 3.05; the blend
    /// view hides the rings and the widest thing in it is a nose trail at 2.05.
    /// Keeping the far distance for both would waste a third of the frame on
    /// background — and the blend's whole content is the GAP between two trails,
    /// which is the first thing a too-small picture loses.
    ///
    /// LESSON 7.4 ADDED A THIRD AND A FOURTH, and the fourth is the only one
    /// that is not centred on the origin. [D] draws a dial at x = −3.9 with a
    /// radius of 1.25, so the content spans roughly −5.2 to +3.0 and a camera
    /// aimed at the origin puts a third of the frame on empty space to the
    /// right while clipping the thing the mode is about. The target moves left
    /// by half the dial's offset, which is the smallest change that frames
    /// both. [C] shares the blend's view: the same two ghosts at the same size.
    [[nodiscard]] engine::transform camera_placement() const
    {
        if (covering_)
        {
            return engine::ecs::look_along(k_cover_eye, {-1.75f, -0.25f, 0.0f},
                                           {0.0f, 1.0f, 0.0f});
        }
        if (commuting_)
        {
            // Aimed ABOVE the origin, because [C]'s content is not centred on
            // it: both journeys leave the same nose and both climb, so the
            // quadrilateral sits in the upper half of the sphere and a camera
            // on the origin spends the bottom third of the frame on nothing.
            return engine::ecs::look_along({3.20f, 2.05f, 4.95f}, {0.0f, 0.62f, 0.0f},
                                           {0.0f, 1.0f, 0.0f});
        }
        if (scheduling_)
        {
            // Pulled back and raised, because [S] draws a 210-degree arc and two
            // extra aircraft on it. The first framing reused [B]'s eye and the
            // long-way ghost spent half the cycle outside the frame, which looks
            // exactly like a ghost that has stopped being drawn.
            return engine::ecs::look_along(k_sched_eye, {0.0f, 0.0f, 0.0f},
                                           {0.0f, 1.0f, 0.0f});
        }
        const vec3 eye = blending_ ? vec3{3.55f, 2.45f, 4.70f} : vec3{5.0f, 3.4f, 6.6f};
        return engine::ecs::look_along(eye, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    }

    // ---- Simulation --------------------------------------------------------

    void drive_knobs(float h)
    {
        const float step = k_knob_rate * k_rad * h;

        // THE JOINT KNOB, which is the whole demonstration. It drives yaw and roll
        // at the same rate in the direction the pitch makes weakest — the two
        // axles converge above the horizon and diverge below it, so the sign has
        // to follow the pitch or the effect reverses and looks like a bug.
        if (actions_.held(a_joint_))
        {
            const float same_way = (std::sin(pose_.pitch) >= 0.0f) ? 1.0f : -1.0f;
            pose_.yaw += step;
            pose_.roll += step * same_way;
        }

        pose_.yaw += actions_.value(a_yaw_) * step;
        pose_.roll += actions_.value(a_roll_) * step;

        // Pitch is NOT wrapped and is clamped just past vertical instead, because
        // this demo exists to sit at 90° and look at it. Everything else is
        // wrapped, which is `wrap_angle`'s whole job (§5.1) and is why the yaw
        // readout never grows without bound during a long joint-knob run.
        pose_.pitch = std::clamp(pose_.pitch + actions_.value(a_pitch_) * step,
                                 -95.0f * k_rad, 95.0f * k_rad);
        pose_.yaw = engine::wrap_angle(pose_.yaw);
        pose_.roll = engine::wrap_angle(pose_.roll);

        // The trail records where the NOSE has been, in world space, which turns
        // "the craft stopped responding" from a thing you have to notice into a
        // thing that leaves no mark.
        if (trail_on_)
        {
            const vec3 nose = engine::rotation_from_euler(pose_) * vec3{0.0f, 0.0f, -1.0f};
            if (trail_.empty() || engine::distance(trail_.back(), nose) > 0.01f)
            {
                trail_.push_back(nose);
                if (trail_.size() > 480) { trail_.erase(trail_.begin()); }
            }
        }
    }

    /// Advance the blend, wrap it, and record where each ghost's nose has been.
    ///
    /// The last quarter of the cycle holds at t = 1 rather than snapping straight
    /// back to t = 0, so that the finished pair is on screen long enough to look
    /// at. Both ghosts reach it; only one of them took the short way.
    void drive_blend(float h)
    {
        constexpr float k_cycle = 5.0f;      ///< seconds, including the hold
        constexpr float k_travel = 4.0f;     ///< of which this much is moving

        blend_phase_ += h;
        if (blend_phase_ >= k_cycle)
        {
            blend_phase_ = 0.0f;
            ghost_euler_.clear();
            ghost_slerp_.clear();
        }
        blend_t_ = std::clamp(blend_phase_ / k_travel, 0.0f, 1.0f);

        record_nose(ghost_euler_, euler_pose());
        record_nose(ghost_slerp_, slerp_pose());
    }

    /// Lesson 7.5's overload. A quaternion rotates a vector directly (the
    /// sandwich), so no matrix is built for a single nose — one vector is far
    /// below 7.4 §10.3's eight-vector crossover.
    static void record_nose(std::vector<vec3>& trail, quat q)
    {
        record_nose_at(trail, q * vec3{0.0f, 0.0f, -1.0f});
    }

    static void record_nose(std::vector<vec3>& trail, const mat3& r)
    {
        record_nose_at(trail, r * vec3{0.0f, 0.0f, -1.0f});
    }

    static void record_nose_at(std::vector<vec3>& trail, vec3 nose)
    {
        if (trail.empty() || engine::distance(trail.back(), nose) > 0.006f)
        {
            trail.push_back(nose);
            if (trail.size() > 900) { trail.erase(trail.begin()); }
        }
    }

    /// The Euler lerp at `t`. Three angles, interpolated independently.
    [[nodiscard]] static mat3 euler_at(float t)
    {
        return engine::rotation_from_euler(lerp_angles(k_blend_from, k_blend_to, t));
    }

    /// The geodesic at `t`. One axis, one angle, scaled.
    [[nodiscard]] static mat3 slerp_at(float t)
    {
        return engine::rotation_slerp(engine::rotation_from_euler(k_blend_from),
                                      engine::rotation_from_euler(k_blend_to), t);
    }

    [[nodiscard]] mat3 euler_pose() const { return euler_at(blend_t_); }
    [[nodiscard]] mat3 slerp_pose() const { return slerp_at(blend_t_); }

    /// The single turn the current pose IS — Euler's rotation theorem, applied.
    [[nodiscard]] engine::axis_angle_extraction single_turn() const
    {
        return engine::axis_angle_from_rotation(engine::rotation_from_euler(pose_));
    }

    /// The single turn that separates the blend's two endpoints.
    ///
    /// **Constant for the whole blend**, which is the entire visual argument of
    /// [B]: the slerp ghost turns about this one line from start to finish, and
    /// the line never moves. Nothing the Euler ghost does can be described that
    /// way at any instant, let alone throughout.
    [[nodiscard]] engine::axis_angle_extraction blend_turn() const
    {
        return engine::axis_angle_from_rotation(
            engine::transpose(engine::rotation_from_euler(k_blend_from))
            * engine::rotation_from_euler(k_blend_to));
    }

    // ---- The debug layer ---------------------------------------------------

    /// The three rings, each in the frame its own knob acts in.
    ///
    /// Read the three lines below and the convention is right there: frame 1 is
    /// the yaw alone, frame 2 is the yaw carrying the pitch, frame 3 is both
    /// carrying the roll. That is what "intrinsic" means, drawn.
    void queue_rings()
    {
        const mat3 f1 = engine::rotation_y(pose_.yaw);
        const mat3 f2 = f1 * engine::rotation_x(pose_.pitch);
        const mat3 f3 = f2 * engine::rotation_z(pose_.roll);

        // Each ring's plane contains its own pivot axis (the first argument) and
        // the next axis down the chain (the second). At pitch 0 the three planes
        // are mutually perpendicular — the picture everybody draws. Turn the
        // pitch to 90 and the blue axle swings onto the green one.
        ring(debug_, f1, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 3.05f, k_ring_yaw);
        ring(debug_, f2, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 2.65f, k_ring_pitch);
        ring(debug_, f3, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, 2.25f, k_ring_roll);
    }

    /// The craft's nose, and the direction that is dying.
    void queue_nose_and_axis()
    {
        const mat3 r = engine::rotation_from_euler(pose_);
        debug_.ray({0.0f, 0.0f, 0.0f}, r * vec3{0.0f, 0.0f, -1.0f} * 2.05f, k_nose);

        // The axis the weakest knob combination produces, drawn at its ACTUAL
        // length rather than normalised. That is the honest picture: away from
        // lock it is a full-length arrow, and as pitch approaches 90° it shrinks
        // to nothing in your hand. A normalised arrow would still point somewhere
        // at the exact moment the claim is that it points nowhere.
        const conditioning c = measure(pose_);
        debug_.ray({0.0f, 0.0f, 0.0f}, c.dead_axis * 2.6f, k_dead);
    }

    /// The line the rotation leaves alone, drawn through the craft.
    ///
    /// Both ways from the origin, because an axis is a LINE and not a direction:
    /// (n, θ) and (−n, −θ) are the same turn, and at θ = 180° the sign is not
    /// determined at all (§8.6). Drawing only the +n half would be asserting a
    /// choice the mathematics does not make.
    ///
    /// It is drawn at a length that scales with the ANGLE, from nothing at the
    /// identity to full length at a half-turn. That is not decoration: near the
    /// identity the axis is genuinely undetermined (the routine says so, with
    /// `axis_route::no_axis`), and a full-length line pointing confidently at the
    /// placeholder +X would be the picture lying. Same discipline as the magenta
    /// arrow above, which is drawn at the true length of the turn it names.
    void queue_axis(const engine::axis_angle_extraction& turn, float base)
    {
        const float reach = base * turn.value.angle / k_pi;
        debug_.line(turn.value.axis * -reach, turn.value.axis * reach, k_axis);
    }

    /// One ghost craft, in wire, at the given orientation.
    ///
    /// Four lines — fuselage, wing, fin — chosen to match the three solid parts
    /// `build_craft` makes, so that the wire ghost and the solid craft read as
    /// the same object. A ghost drawn as a single nose ray would show the
    /// direction and hide the roll, and roll is half of what an Euler lerp gets
    /// wrong.
    /// **Drawn three times, offset**, which is how a line gets a width here.
    ///
    /// `debug_lines` emits one-pixel lines and has no stroke width, deliberately
    /// (5.11). Both craft and both trails are therefore one pixel in the same two
    /// colours, and the first version of this panel was a tangle in which you
    /// could not tell an aircraft from its own history. Dashing the trails was
    /// tried first and did not survive the figure's 3x downsample — the sampler
    /// takes each block's brightest pixel, which fills a two-pixel gap straight
    /// back in. Three copies offset by 25 thousandths along two world axes is
    /// about three pixels at this camera, in every orientation, and downsampling
    /// cannot thin it.
    /// **`scale` was added in 7.4 and defaults to the size [B] has always
    /// used**, so no existing call site changed. [C] draws its second ghost at
    /// 0.72, because two wireframe aircraft sharing an origin at 120 degrees to
    /// each other interpenetrate into a thicket in which neither is an
    /// aeroplane. Nesting them is Lesson 7.3's figure 2 solving the same
    /// problem with the same tool: when two things must be compared and cannot
    /// both be drawn full size, change one scale and SAY SO rather than let the
    /// reader assume they are at the same one.
    void queue_ghost(const mat3& r, Uint32 colour, float scale = 1.0f)
    {
        const vec3 spread[3] = {{0.0f, 0.0f, 0.0f}, {0.025f, 0.0f, 0.0f}, {0.0f, 0.025f, 0.0f}};
        const mat3 g{r.c0 * scale, r.c1 * scale, r.c2 * scale};
        for (const vec3& d : spread)
        {
            debug_.line(g * vec3{0.0f, 0.0f, 1.15f} + d, g * vec3{0.0f, 0.0f, -1.15f} + d, colour);
            debug_.line(g * vec3{-1.25f, 0.0f, 0.22f} + d, g * vec3{1.25f, 0.0f, 0.22f} + d, colour);
            debug_.line(g * vec3{0.0f, 0.09f, 0.92f} + d, g * vec3{0.0f, 0.91f, 0.92f} + d, colour);
            debug_.line(g * vec3{0.0f, 0.91f, 0.92f} + d, g * vec3{0.0f, 0.09f, 1.15f} + d, colour);
        }
    }

    /// Both blend paths at once, with their trails and the axis one of them uses.
    void queue_blend()
    {
        // The axis is the blend's, not the pose's, and it is expressed in the
        // START frame — which is where the turn happens, since slerp is
        // `A · R(n, tθ)`. Carrying it into world space is one multiply and it is
        // the difference between a line that sits still and one that does not.
        const engine::axis_angle_extraction turn = blend_turn();
        const mat3 from = engine::rotation_from_euler(k_blend_from);
        const vec3 world_axis = from * turn.value.axis;
        debug_.line(world_axis * -3.0f, world_axis * 3.0f, k_axis);

        queue_ghost(euler_pose(), k_ghost_euler);
        queue_ghost(slerp_pose(), k_ghost_slerp);

        queue_path(ghost_euler_, k_ghost_euler);
        queue_path(ghost_slerp_, k_ghost_slerp);
    }

    // ---- Lesson 7.4 --------------------------------------------------------

    /// Sweep the shared turn size from 0 to 180 degrees and back.
    ///
    /// Back as well as out, because the interesting part of this mode is not
    /// the 120 degrees at the far end — it is watching the gap OPEN from
    /// nothing, which tells you that non-commutativity is not a threshold
    /// effect waiting at large angles. It is present at every angle but zero,
    /// and at small angles it is proportional to `phi^2`: the two ghosts part
    /// company slowly at first and then decisively, which is the shape of the
    /// cross product that causes it.
    void drive_commute(float h)
    {
        constexpr float k_cycle = 8.0f;
        commute_phase_ = std::fmod(commute_phase_ + h, k_cycle);
        const float u = commute_phase_ / k_cycle;
        const float triangle = (u < 0.5f) ? (u * 2.0f) : (2.0f - u * 2.0f);
        commute_phi_ = triangle * k_pi;
    }

    /// Turn the craft steadily through two full revolutions, then start again.
    ///
    /// TWO, not one, and the second one is the whole mode. A single revolution
    /// shows a craft returning to where it started, which is not news. The
    /// second shows that its QUATERNION did not — it was at `-1` when the craft
    /// was home — and that it takes a second lap to bring both back together.
    void drive_cover(float h)
    {
        constexpr float k_cycle = 12.0f;
        cover_phase_ = std::fmod(cover_phase_ + h, k_cycle);
        cover_turn_ = 4.0f * k_pi * cover_phase_ / k_cycle;
    }

    /// The craft's orientation in [D], as a quaternion.
    ///
    /// **Not wrapped to one revolution**, and that is the point of keeping the
    /// raw angle rather than a matrix: `quat_from_axis_angle` at 400 degrees
    /// and at 40 degrees give the same ROTATION and opposite quaternions, and
    /// this mode exists to show the difference between those two sentences.
    [[nodiscard]] quat cover_quat() const
    {
        return engine::quat_from_axis_angle(k_cover_axis, cover_turn_);
    }

    // ---- Lesson 7.5 --------------------------------------------------------

    /// The blend's two endpoints. Held as functions rather than as constants
    /// because `quat_from_axis_angle` calls `sin` and `cos` and is therefore not
    /// `constexpr`; both are cheap and neither is on a hot path.
    [[nodiscard]] static quat sched_from() { return quat::identity(); }
    [[nodiscard]] static quat sched_to()
    {
        return engine::quat_from_axis_angle(k_sched_axis, k_sched_arc);
    }

    /// The same two poses, blended **the wrong way round the sphere**.
    ///
    /// **This is `quat_slerp` with `nearest` deleted, and nothing else.** The
    /// endpoint handed in is `−b`, which is the same orientation as `b` (the
    /// double cover — 7.4 §8, measured bitwise), so the destination is identical
    /// and only the route differs: 210 degrees the other way instead of 150.
    ///
    /// It is drawn because the failure is not subtle and is not rare. Half of all
    /// pairs of quaternions naming two given orientations are signed this way, an
    /// exporter has no reason to make them consistent, and the symptom is a
    /// character spinning most of the way round between two keyframes that are
    /// nearly identical.
    [[nodiscard]] static quat sched_long_at(float t)
    {
        const quat a = sched_from();
        const quat b = -sched_to();
        return a * engine::quat_pow_unit(engine::conjugate(a) * b, t);
    }

    [[nodiscard]] quat schedule_pose() const
    {
        return engine::quat_slerp(sched_from(), sched_to(), schedule_t_);
    }

    /// Sweep `t` out and hold, exactly as [B] does, so the arrival is on screen.
    void drive_schedule(float h)
    {
        constexpr float k_cycle = 6.0f;      ///< seconds, including the hold
        constexpr float k_travel = 4.6f;     ///< of which this much is moving

        schedule_phase_ += h;
        if (schedule_phase_ >= k_cycle)
        {
            schedule_phase_ = 0.0f;
            ghost_sched_.clear();
            ghost_nlerp_.clear();
            ghost_long_.clear();
        }
        schedule_t_ = std::clamp(schedule_phase_ / k_travel, 0.0f, 1.0f);

        record_nose(ghost_sched_, schedule_pose());
        record_nose(ghost_nlerp_, engine::quat_nlerp(sched_from(), sched_to(), schedule_t_));
        record_nose(ghost_long_, sched_long_at(schedule_t_));
    }

    /// Three craft strung along one arc, and the eleven ticks that are the
    /// argument.
    ///
    /// **THE DRAWING IS THE CLAIM, so it is worth saying what each mark means.**
    ///
    ///   THE ARC is where the craft's nose direction goes. Slerp and nlerp trace
    ///   the SAME curve — that is the half of nlerp that is exactly right, proved
    ///   in Lesson 7.3 §10.3 by an argument that never mentions dimension: the
    ///   chord between two points of a sphere lies in the plane they span with
    ///   the centre, and normalising moves a point along its own radius, which
    ///   cannot leave that plane. So one curve is drawn, not two.
    ///
    ///   THE TICKS are eleven equally spaced values of `t`. The outer row is
    ///   slerp and it is evenly spaced, because the angle covered is `t·Ω` and
    ///   nothing else in the expression depends on `t`. The inner row is nlerp
    ///   and it bunches at both ends. That is the whole of what nlerp gets wrong,
    ///   and it is a picture rather than a paragraph.
    ///
    ///   THE MAGENTA ARC is the same two orientations blended the other way
    ///   round: 210 degrees instead of 150, for want of one comparison.
    void queue_schedule()
    {
        const quat a = sched_from();
        const quat b = sched_to();

        debug_.line(k_sched_axis * -2.9f, k_sched_axis * 2.9f, k_axis);

        // The two arcs, sampled. 48 segments is enough that the polyline reads as
        // a curve at this radius and few enough that the debug budget is untouched.
        constexpr int k_arc_steps = 48;
        vec3 prev_short = nose_at(engine::quat_slerp(a, b, 0.0f));
        vec3 prev_long = nose_at(sched_long_at(0.0f));
        for (int i = 1; i <= k_arc_steps; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(k_arc_steps);
            const vec3 s_now = nose_at(engine::quat_slerp(a, b, t));
            const vec3 l_now = nose_at(sched_long_at(t));
            debug_.line(prev_short, s_now, k_trail);
            debug_.line(prev_long, l_now, k_ghost_long);
            prev_short = s_now;
            prev_long = l_now;
        }

        // The ticks are radial stubs rather than dots: a dot at this distance is
        // one pixel, and eleven pixels is not a measurement anybody can read.
        for (int i = 0; i < k_sched_ticks; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(k_sched_ticks - 1);
            const vec3 s_dir = engine::quat_slerp(a, b, t) * k_nose_dir;
            const vec3 n_dir = engine::quat_nlerp(a, b, t) * k_nose_dir;
            debug_.line(s_dir * k_sched_reach, s_dir * (k_sched_reach + k_sched_tick),
                        k_ghost_slerp);
            debug_.line(n_dir * (k_sched_reach - k_sched_tick), n_dir * k_sched_reach,
                        k_ghost_euler);
        }

        // The three aircraft, each at its own place on its own arc. Drawn small,
        // because at this radius a full-size one is a third of the frame.
        const quat now_slerp = schedule_pose();
        const quat now_nlerp = engine::quat_nlerp(a, b, schedule_t_);
        const quat now_long = sched_long_at(schedule_t_);
        // THE SAME GLYPH [C] USES, and for the same reason it was invented: the
        // blue and amber poses are at most 8 degrees apart (§7.6), which at this
        // radius is a third of a unit, and two wireframe aircraft that close
        // interpenetrate into a thicket in which neither is an aeroplane. A wing
        // bar and a fin say everything the mode needs — where the pose is, and
        // which way up it is, which is the part a bare dot on an arc cannot show.
        pose_glyph(now_long, k_ghost_long);
        pose_glyph(now_nlerp, k_ghost_euler);
        pose_glyph(now_slerp, k_ghost_slerp);

        // THE GAP, drawn as the one line joining the two — gold, which means "the
        // nose" everywhere else in this program and means "the measurement" here.
        //
        // **IT IS A SHORT LINE AND THAT IS THE ANSWER, not a framing failure.**
        // The worst this gap can ever be is 8.15 degrees (§7.6), which at this
        // radius is a third of a unit. Two aircraft that close cannot be told
        // apart by eye, which is exactly why the ticks carry the argument and the
        // aircraft only say which way each pose faces.
        debug_.line(nose_at(now_slerp), nose_at(now_nlerp), k_nose);
    }

    /// Where a pose puts the craft's nose, at the radius this mode draws on.
    [[nodiscard]] static vec3 nose_at(quat q) { return (q * k_nose_dir) * k_sched_reach; }

    /// A pose, marked on the arc: a wing bar and a fin at its nose.
    void pose_glyph(quat q, Uint32 colour)
    {
        const vec3 at = nose_at(q);
        const vec3 wing = (q * vec3{1.0f, 0.0f, 0.0f}) * 0.42f;
        const vec3 fin = (q * vec3{0.0f, 1.0f, 0.0f}) * 0.34f;
        debug_.line(at - wing, at + wing, colour);
        debug_.line(at, at + fin, colour);
    }

    /// How far `q` has turned from `a`, in `[0, 2pi)` — distance travelled, not
    /// separation. See the receipt in `write_shot` for why the difference matters.
    [[nodiscard]] static float travelled(quat a, quat q)
    {
        return engine::axis_angle_from_quat(engine::conjugate(a) * q).value.angle;
    }


    /// Both orders, drawn, with the axes each of them turns about.
    void queue_commute()
    {
        const commutation c = measure_commutation(commute_phi_);

        // The two fixed axes, at a length that grows with the turn, so that at
        // phi = 0 there is nothing on screen claiming a rotation is happening.
        const float reach = 1.7f * commute_phi_ / k_pi;
        debug_.line({0.0f, -reach, 0.0f}, {0.0f, reach, 0.0f}, k_ring_yaw);
        debug_.line({-reach, 0.0f, 0.0f}, {reach, 0.0f, 0.0f}, k_ring_pitch);

        // ---- THE TWO JOURNEYS, and this is the drawing that earns the mode --
        //
        // The first draft drew only the two DESTINATIONS, which made an honest
        // picture of a fact nobody disputes — two orientations, an arc between
        // them, a number. What it could not show is WHY they differ, and the
        // why is visible for free: draw each order as the two arcs it actually
        // is, and the four arcs form a quadrilateral that FAILS TO CLOSE. The
        // gap at the far corner is the whole of non-commutativity, and it is
        // the same picture as walking a mile south, a mile east, a mile north
        // and a mile west on a sphere and not arriving home.
        //
        // Both journeys start at the same nose and use the same two turns. Only
        // the order differs.
        auto journey = [&](bool pitch_first, Uint32 colour) {
            const vec3 nose{0.0f, 0.0f, -1.0f};
            vec3 previous = nose * 2.05f;
            for (int leg = 0; leg < 2; ++leg)
            {
                const bool doing_pitch = (leg == 0) == pitch_first;
                const quat done = (leg == 0) ? quat::identity()
                                  : (pitch_first ? engine::quat_x(commute_phi_)
                                                 : engine::quat_y(commute_phi_));
                for (int i = 1; i <= 40; ++i)
                {
                    const float a = commute_phi_ * static_cast<float>(i) / 40.0f;
                    const quat step = doing_pitch ? engine::quat_x(a) : engine::quat_y(a);
                    const vec3 here = engine::rotate(step * done, nose) * 2.05f;
                    debug_.line(previous, here, colour);
                    previous = here;
                }
            }
        };
        journey(true, k_ghost_euler);    // pitch, then yaw
        journey(false, k_ghost_slerp);   // yaw, then pitch

        // THE TWO ARRIVALS, as a nose ray and a wing bar each — and NOT as the
        // wireframe aircraft [B] uses. Two craft at a common origin 120 degrees
        // apart interpenetrate into a thicket in which neither is an aeroplane,
        // and the figure this render becomes is 3:1 downsampled, where a
        // thicket is a smudge. Four lines each say everything the mode needs:
        // where the nose points, and which way up it is. The wing bar is what
        // stops this being a picture about directions — two poses can share a
        // nose and differ by a roll, and only the bar shows that.
        for (const auto& [pose, colour] :
             {std::pair{c.first, k_ghost_euler}, std::pair{c.second, k_ghost_slerp}})
        {
            const mat3 m = engine::mat3_from_quat(pose);
            const vec3 nose = m * vec3{0.0f, 0.0f, -1.0f};
            debug_.ray({0.0f, 0.0f, 0.0f}, nose * 2.05f, colour);
            const vec3 wing = m * vec3{1.0f, 0.0f, 0.0f} * 0.42f;
            const vec3 fin = m * vec3{0.0f, 1.0f, 0.0f} * 0.34f;
            debug_.line(nose * 2.05f - wing, nose * 2.05f + wing, colour);
            debug_.line(nose * 2.05f, nose * 2.05f + fin, colour);
        }

        // THE GAP ITSELF: the short arc joining the two journeys' ends. Walked
        // with `rotation_slerp` — 7.2's function, doing the one job it is for —
        // because the straight chord between two noses cuts through the sphere
        // and reads as a shorter journey than it is.
        const mat3 a = engine::mat3_from_quat(c.first);
        const mat3 b = engine::mat3_from_quat(c.second);
        vec3 previous = a * vec3{0.0f, 0.0f, -1.0f} * 2.05f;
        for (int i = 1; i <= 48; ++i)
        {
            const float t = static_cast<float>(i) / 48.0f;
            const vec3 here =
                engine::rotation_slerp(a, b, t) * vec3{0.0f, 0.0f, -1.0f} * 2.05f;
            debug_.line(previous, here, k_dead);
            previous = here;
        }
    }

    /// The craft's turn and its quaternion's, side by side, at a ratio of two.
    ///
    /// The dial is Lesson 7.3's figure 6 with one more imaginary unit: a unit
    /// circle, a teal hand at the angle the OBJECT has turned through, and a
    /// gold hand at the angle its rotor carries. Gold moves at exactly half the
    /// rate of teal for as long as you watch, and that is not a tuning of the
    /// demo — it is `cos(theta/2)`.
    void queue_cover()
    {
        // The axis, full length: unlike every other axis in this file it is
        // fixed and known, so there is nothing to be honest about by shrinking.
        debug_.line(k_cover_axis * -3.0f, k_cover_axis * 3.0f, k_axis);

        // THE DIAL'S OWN BASIS, built to face the camera. `u` is the +w
        // direction and `v` is +|v|, so a hand at angle `a` sits at
        // `(cos a, sin a)` exactly as it would on paper.
        // `u` RIGHT, `v` UP, `toward` into the screen — and the order of the two
        // cross products is the whole of it. The first draft wrote
        // `cross(up, toward)` and `cross(toward, u)`, which is a basis with u
        // pointing left and v pointing down, so every angle on the dial came
        // out mirrored AND upside down: at 405 degrees the teal hand sat at
        // 135 instead of 45. Both hands were wrong by the same transformation,
        // which is exactly the kind of error a picture cannot show you —
        // the dial looked entirely plausible. The receipt is what caught it.
        const vec3 toward = engine::normalised(k_dial_centre - k_cover_eye);
        const vec3 u = engine::normalised(engine::cross(toward, vec3{0.0f, 1.0f, 0.0f}));
        const vec3 v = engine::cross(u, toward);
        auto on_dial = [&](float radians, float scale) {
            return k_dial_centre + (u * std::cos(radians) + v * std::sin(radians))
                                       * (k_dial_radius * scale);
        };

        // The rim.
        vec3 previous = on_dial(0.0f, 1.0f);
        for (int i = 1; i <= 96; ++i)
        {
            const vec3 here = on_dial(2.0f * k_pi * static_cast<float>(i) / 96.0f, 1.0f);
            debug_.line(previous, here, k_trail);
            previous = here;
        }

        // The two hands. Teal is the object's own turn; gold is the half-angle
        // the quaternion actually stores, read straight off its components
        // rather than recomputed — `(w, v . n)` IS the point on the circle,
        // which is the fact the dial exists to make visible.
        const quat q = cover_quat();
        auto hand = [&](float radians, float scale, Uint32 colour) {
            const vec3 tip = on_dial(radians, scale);
            debug_.line(k_dial_centre, tip, colour);
            // A crossbar at the tip, so a hand is distinguishable from the two
            // radial ticks below it at a glance and after a 3:1 downsample.
            const vec3 across = engine::normalised(engine::cross(toward, tip - k_dial_centre));
            debug_.line(tip - across * 0.09f, tip + across * 0.09f, colour);
        };
        hand(cover_turn_, 1.0f, k_axis);
        hand(std::atan2(engine::dot(q.v, k_cover_axis), q.w), 0.70f, k_rotor);

        // THE TWO MARKS THAT MAKE THE POINT READABLE: `w = +1` at the right,
        // where both hands start, and `w = -1` at the left. When the gold hand
        // reaches the left mark the craft is exactly home and its quaternion is
        // as far from home as it can get — which is the whole mode, in two
        // ticks and a pair of hands.
        for (float where : {0.0f, k_pi})
        {
            debug_.line(on_dial(where, 1.0f), on_dial(where, 1.20f), k_rotor);
        }
    }

    /// Where a nose has been, at one pixel — thinner than the craft above, on
    /// purpose, so that the two read as "now" and "was".
    void queue_path(const std::vector<vec3>& trail, Uint32 colour)
    {
        for (std::size_t i = 1; i < trail.size(); ++i)
        {
            debug_.line(trail[i - 1] * 2.05f, trail[i] * 2.05f, colour);
        }
    }

    void queue_trail()
    {
        for (std::size_t i = 1; i < trail_.size(); ++i)
        {
            debug_.line(trail_[i - 1] * 2.05f, trail_[i] * 2.05f, k_trail);
        }
    }

    // ---- The readout -------------------------------------------------------

    void build_panel()
    {
        const conditioning c = measure(pose_);
        const engine::euler_extraction back =
            engine::euler_from_rotation(engine::rotation_from_euler(pose_));

        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("gimbal"))
        {
            float yaw_deg = pose_.yaw * k_deg;
            float pitch_deg = pose_.pitch * k_deg;
            float roll_deg = pose_.roll * k_deg;
            if (ImGui::SliderFloat("yaw", &yaw_deg, -180.0f, 180.0f, "%.2f deg"))
            {
                pose_.yaw = yaw_deg * k_rad;
            }
            if (ImGui::SliderFloat("pitch", &pitch_deg, -95.0f, 95.0f, "%.2f deg"))
            {
                pose_.pitch = pitch_deg * k_rad;
            }
            if (ImGui::SliderFloat("roll", &roll_deg, -180.0f, 180.0f, "%.2f deg"))
            {
                pose_.roll = roll_deg * k_rad;
            }

            ImGui::Separator();
            ImGui::Text("|det J|   %.6f      (= |cos pitch| = %.6f)",
                        static_cast<double>(std::fabs(c.det_j)),
                        static_cast<double>(std::fabs(std::cos(pose_.pitch))));
            ImGui::Text("sigma_min %.6f      weakest gain, body rad/s per knob rad/s",
                        static_cast<double>(c.sigma_min));
            if (c.cost > 0.0f)
            {
                ImGui::Text("to turn at 1 rad/s about the weak axis: %.1f rad/s of knob",
                            static_cast<double>(c.cost));
            }
            else
            {
                ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.45f, 1.0f),
                                   "LOCKED: no knob rate produces that turn at all");
            }
            ImGui::Text("weak combination: yaw %+.2f, pitch %+.2f, roll %+.2f",
                        static_cast<double>(c.weak_knobs.x),
                        static_cast<double>(c.weak_knobs.y),
                        static_cast<double>(c.weak_knobs.z));
            ImGui::Text("it produces %.4f rad/s of body turn",
                        static_cast<double>(engine::length(c.dead_axis)));

            ImGui::Separator();
            ImGui::Text("extracted: yaw %+.2f  pitch %+.2f  roll %+.2f",
                        static_cast<double>(back.angles.yaw * k_deg),
                        static_cast<double>(back.angles.pitch * k_deg),
                        static_cast<double>(back.angles.roll * k_deg));
            ImGui::Text("cos_pitch %.3e   %s", static_cast<double>(back.cos_pitch),
                        back.degenerate ? "DEGENERATE - roll folded into yaw" : "separable");

            // ---- Lesson 7.2 ------------------------------------------------
            ImGui::Separator();
            const engine::axis_angle_extraction turn = single_turn();
            const char* route = (turn.route == engine::axis_route::no_axis)  ? "no_axis"
                              : (turn.route == engine::axis_route::general)  ? "general"
                                                                            : "reversal";
            ImGui::Text("single turn: %.4f deg about (%+.4f, %+.4f, %+.4f)",
                        static_cast<double>(turn.value.angle * k_deg),
                        static_cast<double>(turn.value.axis.x),
                        static_cast<double>(turn.value.axis.y),
                        static_cast<double>(turn.value.axis.z));
            ImGui::Text("sin(angle) %.3e   route %s", static_cast<double>(turn.sin_angle), route);

            // THE AGREEMENT, PRINTED. Two entirely different constructions of the
            // same orientation — three composed elementary turns, and one
            // Rodrigues turn about a recovered axis — with the metric between
            // them. This is Euler's rotation theorem as a live number rather than
            // a claim, and it is never worse than a few millionths of a degree.
            const float disagreement = engine::angle_between_rotations(
                engine::rotation_from_euler(pose_),
                engine::rotation_from_axis_angle(turn.value));
            ImGui::Text("Euler product vs Rodrigues: %.6f deg apart   %s",
                        static_cast<double>(disagreement * k_deg),
                        single_turn_ ? "[A] posing by AXIS-ANGLE" : "[A] posing by EULER");

            if (blending_)
            {
                ImGui::Separator();
                const float euler_excess =
                    100.0f * (cost_.euler_path / cost_.geodesic - 1.0f);
                const float slerp_excess =
                    100.0f * (cost_.slerp_path / cost_.geodesic - 1.0f);
                ImGui::Text("blend t = %.3f", static_cast<double>(blend_t_));
                ImGui::Text("geodesic         %8.2f deg",
                            static_cast<double>(cost_.geodesic * k_deg));
                ImGui::TextColored(ImVec4(0.93f, 0.66f, 0.34f, 1.0f),
                                   "Euler lerp path  %8.2f deg   %+.2f%%",
                                   static_cast<double>(cost_.euler_path * k_deg),
                                   static_cast<double>(euler_excess));
                ImGui::TextColored(ImVec4(0.49f, 0.74f, 0.97f, 1.0f),
                                   "slerp path       %8.2f deg   %+.2f%%",
                                   static_cast<double>(cost_.slerp_path * k_deg),
                                   static_cast<double>(slerp_excess));
                ImGui::TextUnformatted("the teal line is the slerp's axis, and it never moves");
            }

            // ---- Lesson 7.4 ------------------------------------------------
            if (commuting_)
            {
                ImGui::Separator();
                const commutation cm = measure_commutation(commute_phi_);
                ImGui::Text("both turns: %.2f deg", static_cast<double>(commute_phi_ * k_deg));
                ImGui::TextColored(ImVec4(0.93f, 0.66f, 0.34f, 1.0f),
                                   "amber  quat_y * quat_x   (pitch, then yaw)");
                ImGui::TextColored(ImVec4(0.49f, 0.74f, 0.97f, 1.0f),
                                   "blue   quat_x * quat_y   (yaw, then pitch)");
                ImGui::Text("they are %.4f deg apart",
                            static_cast<double>(cm.gap * k_deg));
                // THE TWO CONTROLS, SIDE BY SIDE WITH THE ANSWER. One is the
                // same question asked of the matrices, which shares no code
                // with the quaternion path; the other is the closed form
                // derived above `measure_commutation`. Three routes agreeing
                // is a fact; one route reporting is a hope.
                ImGui::Text("  from the matrices: %.4f deg",
                            static_cast<double>(cm.gap_matrix * k_deg));
                ImGui::Text("  2 acos|c4+2c2s2-s4|: %.4f deg",
                            static_cast<double>(cm.closed_form * k_deg));
                ImGui::TextUnformatted("at 90 deg each, the two orders are 120 deg apart");
            }

            if (covering_)
            {
                ImGui::Separator();
                const quat q = cover_quat();
                const float pose_angle = engine::angle_between_rotations(
                    mat3::identity(), engine::mat3_from_quat(q));
                ImGui::Text("turned %.1f deg about (%+.3f, %+.3f, %+.3f)",
                            static_cast<double>(cover_turn_ * k_deg),
                            static_cast<double>(k_cover_axis.x),
                            static_cast<double>(k_cover_axis.y),
                            static_cast<double>(k_cover_axis.z));
                ImGui::TextColored(ImVec4(0.97f, 0.84f, 0.47f, 1.0f),
                                   "q = (%+.5f, %+.5f, %+.5f, %+.5f)",
                                   static_cast<double>(q.w),
                                   static_cast<double>(q.v.x),
                                   static_cast<double>(q.v.y),
                                   static_cast<double>(q.v.z));
                ImGui::Text("w = cos(half) = %+.5f      |v| = %.5f",
                            static_cast<double>(q.w),
                            static_cast<double>(engine::length(q.v)));
                // THE POSE, MEASURED FROM THE MATRIX. It comes back to 0 at
                // 360 while `w` is at -1, and that pair of numbers on one line
                // is the whole mode.
                ImGui::Text("the craft is %.3f deg from where it started",
                            static_cast<double>(pose_angle * k_deg));
                ImGui::TextUnformatted("gold hand is the rotor: it turns at half the rate");
            }

            if (scheduling_)
            {
                ImGui::Separator();
                const quat a = sched_from();
                const quat b = sched_to();
                const quat s_sl = engine::quat_slerp(a, b, schedule_t_);
                const quat s_nl = engine::quat_nlerp(a, b, schedule_t_);
                const float arc = engine::angle_between(a, b);
                ImGui::Text("arc %.2f deg   sphere arc %.2f deg   t %.3f",
                            static_cast<double>(arc * k_deg),
                            static_cast<double>(arc * k_deg * 0.5f),
                            static_cast<double>(schedule_t_));
                ImGui::Text("slerp has turned %.4f deg   (= t x arc, exactly)",
                            static_cast<double>(travelled(a, s_sl) * k_deg));
                ImGui::Text("nlerp has turned %.4f deg",
                            static_cast<double>(travelled(a, s_nl) * k_deg));
                ImGui::TextColored(ImVec4(0.97f, 0.84f, 0.47f, 1.0f),
                                   "the gold line is %.4f deg long",
                                   static_cast<double>(
                                       engine::angle_between(s_sl, s_nl) * k_deg));
                ImGui::Text("worst speed ratio sec2(sphere arc / 2) = %.4f",
                            static_cast<double>(
                                1.0f / (std::cos(arc * 0.25f) * std::cos(arc * 0.25f))));
                ImGui::TextUnformatted("long way round (magenta): 210 deg, for want of one sign");
            }

            ImGui::Separator();
            ImGui::TextUnformatted("arrows yaw/pitch  Q/W roll  [E] joint knob");
            ImGui::TextUnformatted("[K] snap to lock  [T] nose trail  [G] rings  [R] reset");
            ImGui::TextUnformatted("[A] pose by axis-angle  [B] blend: Euler lerp vs slerp");
            ImGui::TextUnformatted("[C] both orders of two turns  [D] the double cover");
            ImGui::TextUnformatted("[S] slerp vs nlerp, and the long way round");
        }
        ImGui::End();
    }

    void write_shot()
    {
        const conditioning c = measure(pose_);
        // The shot's receipt. Two runs with the same `--pose` print the same
        // numbers or the picture is not reproducible, whatever it looks like.
        const engine::axis_angle_extraction turn = single_turn();
        std::printf("gimbal: pose %.2f %.2f %.2f deg, |det J| %.6f, sigma_min %.6f, "
                    "%zu objects, %zu triangles, %d debug lines\n",
                    static_cast<double>(pose_.yaw * k_deg),
                    static_cast<double>(pose_.pitch * k_deg),
                    static_cast<double>(pose_.roll * k_deg),
                    static_cast<double>(std::fabs(c.det_j)), static_cast<double>(c.sigma_min),
                    collect_.drawn, triangles_.size(), debug_drawn_);
        // The 7.2 half of the receipt. The last field is the one that matters:
        // two runs agreeing on a picture prove nothing if the picture was built
        // from a rotation that disagrees with itself.
        std::printf("gimbal: single turn %.4f deg about (%+.5f, %+.5f, %+.5f), route %d, "
                    "blend t %.3f, Euler-vs-Rodrigues %.6f deg\n",
                    static_cast<double>(turn.value.angle * k_deg),
                    static_cast<double>(turn.value.axis.x),
                    static_cast<double>(turn.value.axis.y),
                    static_cast<double>(turn.value.axis.z),
                    static_cast<int>(turn.route), static_cast<double>(blend_t_),
                    static_cast<double>(k_deg * engine::angle_between_rotations(
                        engine::rotation_from_euler(pose_),
                        engine::rotation_from_axis_angle(turn.value))));
        // The 7.4 half. Printed unconditionally rather than only in the two new
        // modes, because a receipt that changes shape between runs cannot be
        // diffed — and diffing two receipts is how this demo's figures are
        // checked against the harness.
        const commutation cm = measure_commutation(commute_phi_);
        const quat cq = cover_quat();
        //
        // FOUR LINES, NOT TWO, AND THE WIDTH IS THE REASON. The lesson quotes
        // this receipt inside a <pre> block, which scrolls and never wraps, and
        // the fold is at about 66 characters. The first version printed the
        // commute row at 80 characters, so the two CONTROL numbers — the matrix
        // metric and the closed form, which are the entire reason that row is
        // worth quoting — fell off the right-hand edge of the page with no
        // visible scrollbar on macOS. Lesson 7.3 lost fourteen numbers that
        // way before anybody noticed. Narrow the program, not the prose: this
        // also reads in an 80-column terminal, which the 88-character cover row
        // did not.
        std::printf("gimbal: commute %.2f deg -> gap %.4f deg\n",
                    static_cast<double>(commute_phi_ * k_deg),
                    static_cast<double>(cm.gap * k_deg));
        std::printf("gimbal:   matrix %.4f, closed form %.4f\n",
                    static_cast<double>(cm.gap_matrix * k_deg),
                    static_cast<double>(cm.closed_form * k_deg));
        std::printf("gimbal: cover %.2f deg -> pose %.4f deg\n",
                    static_cast<double>(cover_turn_ * k_deg),
                    static_cast<double>(k_deg * engine::angle_between_rotations(
                        mat3::identity(), engine::mat3_from_quat(cq))));
        std::printf("gimbal:   q (%+.5f, %+.5f, %+.5f, %+.5f)\n",
                    static_cast<double>(cq.w), static_cast<double>(cq.v.x),
                    static_cast<double>(cq.v.y), static_cast<double>(cq.v.z));

        // The 7.5 half, and the same width rule: nothing past 66 characters.
        const quat sa = sched_from();
        const quat sb = sched_to();
        const quat s_sl = engine::quat_slerp(sa, sb, schedule_t_);
        const quat s_nl = engine::quat_nlerp(sa, sb, schedule_t_);
        const quat s_lg = sched_long_at(schedule_t_);
        std::printf("gimbal: sched t %.3f  arc %.2f deg\n",
                    static_cast<double>(schedule_t_),
                    static_cast<double>(k_deg * engine::angle_between(sa, sb)));
        // TRAVELLED, NOT SEPARATION, for the first three — and the two are not
        // the same instrument. `angle_between` takes an absolute value and
        // therefore caps at 180, which is right for "how far apart are these two
        // poses" and wrong for "how far has this one turned": the long-way ghost
        // passes 180 at t = 0.857 and a capped reading would show it coming BACK.
        // `axis_angle_from_quat` returns [0, 2pi) precisely so this question has
        // an answer (7.4 §9.2), which is the double cover being useful.
        std::printf("gimbal:   slerp %.4f, nlerp %.4f deg turned\n",
                    static_cast<double>(k_deg * travelled(sa, s_sl)),
                    static_cast<double>(k_deg * travelled(sa, s_nl)));
        std::printf("gimbal:   nlerp lag %.4f deg\n",
                    static_cast<double>(k_deg * engine::angle_between(s_sl, s_nl)));
        std::printf("gimbal:   long way %.4f deg turned\n",
                    static_cast<double>(k_deg * travelled(sa, s_lg)));
        request_quit(engine::save_ppm(fb(), shot_path_));
    }

    // ---- State -------------------------------------------------------------

    engine::ecs::registry world_;
    engine::ecs::hierarchy tree_;
    engine::mesh_pool meshes_;
    engine::mesh_handle mesh_box_{};
    entity craft_{};
    entity camera_{};

    euler_angles pose_{};
    std::vector<vec3> trail_;
    bool trail_on_ = false;
    bool rings_ = true;
    const char* shot_path_ = nullptr;

    // Lesson 7.2.
    bool single_turn_ = false;        ///< [A]: pose by Rodrigues instead of Euler
    bool blending_ = false;           ///< [B]: the two-path comparison is running
    float blend_t_ = 0.0f;            ///< where along it, in [0, 1]
    float blend_phase_ = 0.0f;        ///< seconds into the cycle, including the hold
    blend_cost cost_{};               ///< measured once per blend, never per frame
    std::vector<vec3> ghost_euler_;
    std::vector<vec3> ghost_slerp_;

    // Lesson 7.4.
    bool commuting_ = false;          ///< [C]: the same two turns, both orders
    float commute_phi_ = 0.0f;        ///< the shared turn size, radians
    float commute_phase_ = 0.0f;      ///< seconds into the sweep
    bool covering_ = false;           ///< [D]: two revolutions and one dial
    float cover_turn_ = 0.0f;         ///< how far the craft has turned, radians
    float cover_phase_ = 0.0f;        ///< seconds into the cycle

    // Lesson 7.5.
    bool scheduling_ = false;         ///< [S]: slerp, nlerp and the long way round
    float schedule_t_ = 0.0f;         ///< where along the blend, in [0, 1]
    float schedule_phase_ = 0.0f;     ///< seconds into the cycle, including the hold
    std::vector<vec3> ghost_sched_;   ///< the slerp nose's history
    std::vector<vec3> ghost_nlerp_;   ///< the nlerp nose's — the SAME curve
    std::vector<vec3> ghost_long_;    ///< the one that went the other way

    engine::action_map actions_;
    engine::masked_input<engine::input> gate_;
    engine::action_id a_yaw_{}, a_pitch_{}, a_roll_{}, a_joint_{};
    engine::action_id a_lock_{}, a_reset_{}, a_rings_{}, a_trail_{}, a_quit_{};
    engine::action_id a_single_{}, a_blend_{};
    engine::action_id a_commute_{}, a_cover_{}, a_sched_{};

    engine::depth_buffer depth_{k_width, k_height};
    engine::lighting lights_;
    std::vector<engine::scene_object> objects_;
    std::vector<engine::raster_triangle> triangles_;
    engine::projection_scratch scratch_;
    engine::renderable_report collect_{};
    engine::debug_lines debug_{8192};
    int debug_drawn_ = 0;
    engine::debug_ui ui_;
};

}  // namespace

ENGINE_MAIN(gimbal_app)
