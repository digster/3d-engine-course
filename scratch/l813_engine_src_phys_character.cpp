// engine/src/phys/character.cpp — a capsule that moves by asking questions.
//
// Lesson 8.13. Read `character.hpp` first; it carries the argument. Everything
// in this file is built on ONE query, `sweep`, which is `cast.hpp` run once per
// candidate obstacle. The rest is policy: what to do with the answer.
//
// The order of `move_character` is the lesson's §13, and every stage is there
// because a measured section needed it:
//
//   1. CARRY     by the ground body's step, by transform            §10
//   2. RECOVER   out of anything that moved into us, by EPA          §11
//   3. SIDEWAYS  along the ground, off walls, up steps               §4 §5 §8
//   4. VERTICAL  land on floor, slide off steep slopes, stop at roofs §5
//   5. GROUND    probe down; snap down if the ground dropped away     §6 §7

#include <engine/phys/character.hpp>

#include <engine/math/bounds.hpp>
#include <engine/phys/collide.hpp>
#include <engine/phys/convex.hpp>
#include <engine/phys/gjk.hpp>

#include <algorithm>   // std::max, std::min
#include <cmath>       // std::cos, std::sin, std::sqrt, std::atan, std::pow

namespace engine::phys
{
namespace
{

constexpr vec3 k_up{0.0f, 1.0f, 0.0f};

/// The most surfaces one pass remembers. `max_slides` is clamped to it.
constexpr int k_max_planes = 8;

/// A displacement shorter than this is no displacement. A tenth of a micron:
/// far below anything a cast resolves, far above a float's floor at 1 m.
constexpr float k_tiny_move2 = 1e-14f;

[[nodiscard]] vec3 horizontal(vec3 v) { return vec3{v.x, 0.0f, v.z}; }

/// A walkable test with a hair of slack, so that a ramp authored at EXACTLY
/// `max_slope` is floor whichever way the float rounded its normal.
[[nodiscard]] float walkable_cos(const character_config& cfg) { return std::cos(cfg.max_slope) - 1e-5f; }

/// **The rest of a move, after a hit**, by `cfg.clip`. `intent` is the whole
/// step's motion, `time_left` the fraction of the step not yet spent, `left` the
/// part of the last attempt that was not travelled.
[[nodiscard]] vec3 next_move(const character_config& cfg, vec3 intent, float time_left, vec3 left,
                             const vec3* planes, int count)
{
    const auto all = std::span<const vec3>{planes, static_cast<std::size_t>(count)};
    switch (cfg.clip)
    {
    case character_config::clip_rule::original:   return clip_to_planes(intent * time_left, all);
    case character_config::clip_rule::remainder:  return clip_to_planes(left, all);
    case character_config::clip_rule::last_plane: return clip_to_planes(left, all.subspan(all.size() - 1));
    }
    return vec3{};
}

/// **Collide and slide**: move along `d`, and whenever something is hit, clip
/// the rest by `cfg.clip` and go on. Used for the carry, which is a plain
/// slide; the sideways pass has its own loop, because it has ground to follow
/// and steps to climb, and so does the vertical one, because it lands.
void slide_move(vec3& position, vec3 d, const character_world& w, const character_config& cfg,
                sweep_filter filter, move_report& r)
{
    vec3 planes[k_max_planes];
    int count = 0;
    const int limit = std::min(cfg.max_slides, k_max_planes);

    float time_left = 1.0f;
    vec3 remaining = d;
    for (int i = 0; i < limit; ++i)
    {
        if (length_squared(remaining) <= k_tiny_move2) { break; }
        const character_hit hit = sweep(position, remaining, w, cfg, filter, &r);
        position += remaining * hit.t;
        if (!hit.hit) { break; }

        time_left *= 1.0f - hit.t;
        const vec3 left = remaining * (1.0f - hit.t);
        planes[count++] = !walkable(hit.normal, cfg) ? wall_normal(hit.normal) : hit.normal;
        remaining = next_move(cfg, d, time_left, left, planes, count);
    }
}

/// Record walkable ground under the character.
void stand_on(character& c, const character_hit& hit)
{
    c.grounded = true;
    c.ground_normal = hit.normal;
    c.ground_point = hit.point;
    c.ground_body = hit.body;
}

/// **The step-up**: up by `step_height`, across, down by as much as it rose.
///
/// Accepted only if it LANDS — on walkable ground, higher than it started and no
/// higher than `step_height` — and made horizontal progress. Otherwise nothing
/// has happened and the caller slides along the wall instead.
///
/// The across move is at least `step_forward_min`: at the riser the capsule's
/// centre can be a whole radius short of the edge, and it must end within
/// `r·sin θ` of it to land with a walkable normal (§8). That is a lurch of up to
/// nine centimetres on the frame a step is taken, and it is the price of a
/// round bottom.
bool try_step_up(character& c, vec3 across, const character_world& w, const character_config& cfg,
                 move_report& r)
{
    const float len = length(across);
    if (len <= 0.0f || cfg.step_height <= 0.0f) { return false; }

    const vec3 start = c.position;
    vec3 p = start;

    // UP, as far as the ceiling allows.
    const character_hit up = sweep(p, k_up * cfg.step_height, w, cfg, sweep_filter::everything, &r);
    const float rise = cfg.step_height * up.t;
    p.y += rise;
    if (rise <= cfg.skin) { return false; }   // a roof right overhead: nothing to step into

    // ACROSS, by the rest of the move or the least that lands walkable.
    const float reach = std::max(len, step_forward_min(cfg.radius + cfg.skin, cfg.max_slope));
    const vec3 dir = across / len;
    const character_hit ahead = sweep(p, dir * reach, w, cfg, sweep_filter::solid, &r);
    const float moved = reach * ahead.t;
    if (moved <= cfg.skin) { return false; }  // still a wall at the top: taller than a step
    p += dir * moved;

    // DOWN, as far as it rose. Anything lower is a drop, not a step.
    const character_hit down = sweep(p, k_up * -rise, w, cfg, sweep_filter::everything, &r);
    if (!down.hit || !walkable(down.normal, cfg)) { return false; }
    p.y -= rise * down.t;

    const float gained = p.y - start.y;
    if (gained <= 0.0f || gained > cfg.step_height) { return false; }

    c.position = p;
    stand_on(c, down);
    r.stepped = true;
    r.step_rise = gained;
    return true;
}

/// **The sideways pass.** Along the ground while grounded; walls clipped by
/// `cfg.clip` with their normals flattened; one step-up attempt at the first
/// wall. Pushable bodies are not in it: the proxy moves them.
void sideways(character& c, vec3 d, bool grounded, const character_world& w, const character_config& cfg,
              move_report& r)
{
    vec3 planes[k_max_planes];
    int count = 0;
    const int limit = std::min(cfg.max_slides, k_max_planes);
    bool tried_step = false;
    const bool follow = grounded && cfg.lay_along_ground;

    vec3 ground_n = c.ground_normal;
    vec3 intent = d;           // the whole step's motion, laid along whatever ground we are on
    float time_left = 1.0f;
    vec3 remaining = d;
    for (int i = 0; i <= limit; ++i)
    {
        if (length_squared(remaining) <= k_tiny_move2) { return; }
        if (i == limit)
        {
            r.blocked = true;
            return;
        }

        const character_hit hit = sweep(c.position, remaining, w, cfg, sweep_filter::solid, &r);
        c.position += remaining * hit.t;
        if (!hit.hit) { return; }
        time_left *= 1.0f - hit.t;
        const vec3 left = remaining * (1.0f - hit.t);

        // Walkable ground rising or falling under a grounded character — the
        // foot of a ramp, the edge of a curb low enough for the round bottom to
        // roll over. Not a wall: lay the intent along it and carry on.
        if (follow && walkable(hit.normal, cfg))
        {
            ground_n = hit.normal;
            intent = along_ground(horizontal(intent), ground_n);
            remaining = count == 0 ? intent * time_left
                                   : along_ground(horizontal(next_move(cfg, intent, time_left, left, planes, count)),
                                                  ground_n);
            continue;
        }

        ++r.slides;

        // A wall while standing: try to step up it, once per move.
        if (grounded && !tried_step && cfg.step_height > 0.0f)
        {
            tried_step = true;
            if (try_step_up(c, horizontal(left), w, cfg, r)) { return; }
        }

        planes[count++] = (cfg.flatten_walls && !walkable(hit.normal, cfg)) ? wall_normal(hit.normal) : hit.normal;
        vec3 next = next_move(cfg, intent, time_left, left, planes, count);

        // Clipping turned the motion; turn its climb with it, so that it still
        // lies on the ground it is crossing rather than lifting off or digging in.
        // A flattened wall normal is horizontal, so this cannot undo the clip.
        if (follow && walkable(ground_n, cfg)) { next = along_ground(horizontal(next), ground_n); }
        remaining = next;
    }
}

/// **The vertical pass.** Down: land on floor, slide off anything steeper.
/// Up: stop at the first thing overhead.
void vertical(character& c, float dy, const character_world& w, const character_config& cfg, move_report& r)
{
    if (dy > 0.0f)
    {
        const character_hit hit = sweep(c.position, k_up * dy, w, cfg, sweep_filter::everything, &r);
        c.position.y += dy * hit.t;
        r.hit_ceiling = hit.hit;
        return;
    }
    if (dy == 0.0f) { return; }

    vec3 planes[k_max_planes];
    int count = 0;
    const int limit = std::min(cfg.max_slides, k_max_planes);

    const vec3 intent = k_up * dy;
    float time_left = 1.0f;
    vec3 remaining = intent;
    for (int i = 0; i < limit; ++i)
    {
        if (length_squared(remaining) <= k_tiny_move2) { return; }
        const character_hit hit = sweep(c.position, remaining, w, cfg, sweep_filter::everything, &r);
        c.position += remaining * hit.t;
        if (!hit.hit) { return; }
        time_left *= 1.0f - hit.t;

        if (walkable(hit.normal, cfg))
        {
            stand_on(c, hit);
            if (cfg.stop_on_ground) { return; }
        }

        // Steep: gravity slides the character down it. Not flattened — sliding
        // DOWN a slope too steep to stand on is exactly what the design wants.
        const vec3 left = remaining * (1.0f - hit.t);
        planes[count++] = hit.normal;
        remaining = next_move(cfg, intent, time_left, left, planes, count);
    }
}

/// **The snap.** Stood last step, not rising, ground gone: look down
/// `snap_distance` for floor, and stand on it if it is there.
///
/// One complication, and it is the round bottom again. A capsule that has just
/// walked off a stair is usually RESTING ON THE EDGE it walked off, with a
/// contact normal too steep to stand on — so the first thing a cast down finds
/// is that edge, not the tread below. It rolls off first: sideways, away from
/// the edge, by exactly what clears it, `(r + skin) − overhang`, which is less
/// than `step_forward_min` whenever the edge was too steep to stand on (§7).
void snap_down(character& c, const character_world& w, const character_config& cfg, move_report& r)
{
    vec3 p = c.position;
    const vec3 down = k_up * -cfg.snap_distance;
    character_hit hit = sweep(p, down, w, cfg, sweep_filter::everything, &r);

    if (hit.hit && !walkable(hit.normal, cfg))
    {
        const vec3 foot = p - k_up * cfg.half_height;        // the bottom sphere's centre
        const vec3 away = horizontal(foot - hit.point);
        const float overhang = length(away);
        const float clear = cfg.radius + cfg.skin - overhang + 0.1f * cfg.skin;
        if (overhang > 1e-6f && clear > 0.0f)
        {
            const vec3 roll = away * (clear / overhang);
            const character_hit side = sweep(p, roll, w, cfg, sweep_filter::solid, &r);
            p += roll * side.t;
            hit = sweep(p, down, w, cfg, sweep_filter::everything, &r);
        }
    }
    if (!hit.hit || !walkable(hit.normal, cfg)) { return; }

    p.y -= cfg.snap_distance * hit.t;
    r.snapped = true;
    r.snap_drop = c.position.y - p.y;
    r.snap_shift = length(horizontal(p - c.position));
    c.position = p;
    stand_on(c, hit);
}

} // namespace

// ---------------------------------------------------------------------------
// Small pieces
// ---------------------------------------------------------------------------

capsule character_capsule(vec3 position, const character_config& cfg)
{
    capsule c;
    c.centre = position;
    c.axis = k_up;
    c.half_height = cfg.half_height;
    c.radius = cfg.radius;
    return c;
}

bool walkable(vec3 surface_normal, const character_config& cfg)
{
    return surface_normal.y >= walkable_cos(cfg);
}

bool pushable(const rigid_body& b, const character_config& cfg)
{
    return b.kind == body_kind::dynamic && b.inv_mass > 0.0f && b.inv_mass * cfg.push_mass_limit >= 1.0f;
}

vec3 slide_along(vec3 v, vec3 n)
{
    const float into = dot(v, n);
    return into < 0.0f ? v - n * into : v;
}

vec3 clip_to_planes(vec3 v, std::span<const vec3> planes)
{
    // "Goes into a plane" with a relative tolerance: the clipped vector lies IN
    // the planes it was clipped against, and rounding leaves it a hair either
    // side. Without the slack a vector clipped exactly onto plane i reads as
    // entering plane i again, and every corner becomes a stop.
    if (planes.empty()) { return v; }
    const float slack = 1e-5f * length(v);
    auto enters = [&](vec3 u, std::size_t k) { return dot(u, planes[k]) < -slack; };

    // One plane at a time.
    for (std::size_t i = 0; i < planes.size(); ++i)
    {
        const vec3 u = slide_along(v, planes[i]);
        bool ok = true;
        for (std::size_t j = 0; j < planes.size() && ok; ++j)
        {
            if (j != i && enters(u, j)) { ok = false; }
        }
        if (ok) { return u; }
    }

    // Two at a time: along the crease both planes contain.
    for (std::size_t i = 0; i < planes.size(); ++i)
    {
        for (std::size_t j = i + 1; j < planes.size(); ++j)
        {
            const vec3 crease = cross(planes[i], planes[j]);
            const float len2 = length_squared(crease);
            if (len2 <= 1e-10f) { continue; }   // parallel planes have no crease
            const vec3 dir = crease * (1.0f / std::sqrt(len2));
            const vec3 u = dir * dot(dir, v);
            bool ok = true;
            for (std::size_t k = 0; k < planes.size() && ok; ++k)
            {
                if (k != i && k != j && enters(u, k)) { ok = false; }
            }
            if (ok) { return u; }
        }
    }

    // Three planes pin it. Stop.
    return vec3{};
}

vec3 wall_normal(vec3 n)
{
    const vec3 f = horizontal(n);
    const float len = length(f);
    if (len < 1e-3f) { return n; }
    return f / len;
}

vec3 along_ground(vec3 d, vec3 ground_normal)
{
    if (ground_normal.y <= 1e-3f) { return d; }
    return d - k_up * (dot(d, ground_normal) / ground_normal.y);
}

// ---------------------------------------------------------------------------
// The query
// ---------------------------------------------------------------------------

character_hit sweep(vec3 position, vec3 d, const character_world& w, const character_config& cfg,
                    sweep_filter filter, move_report* report)
{
    character_hit best;
    if (report != nullptr) { ++report->sweeps; }
    if (length_squared(d) <= 0.0f) { return best; }

    const capsule here = character_capsule(position, cfg);
    const capsule there = character_capsule(position + d, cfg);
    const convex mover = as_convex(here);

    // The swept volume's box: both ends, plus the skin. A cast can only hit
    // what overlaps it, and a box test is a dozen comparisons against a cast's
    // several GJK calls.
    aabb swept = bounds_of(here);
    swept.expand(bounds_of(there));
    swept.grow(cfg.skin);

    cast_config cc = cfg.cast;
    cc.skin = cfg.skin;

    const std::size_t n = std::min(w.bodies.size(), w.shapes.size());
    for (std::size_t i = 0; i < n; ++i)
    {
        const auto index = static_cast<std::uint32_t>(i);
        if (index == w.self) { continue; }
        const rigid_body& b = w.bodies[i];
        if (filter == sweep_filter::solid && pushable(b, cfg)) { continue; }
        const shape& s = w.shapes[i];
        if (!overlaps(swept, bounds_of(s, b.state.position, b.orientation))) { continue; }

        const placed_shape obstacle = place(s, b.state.position, b.orientation);
        const cast_result hit = cast(mover, d, obstacle.view(), cc);
        if (report != nullptr)
        {
            ++report->candidates;
            ++report->casts;
            report->cast_iterations += hit.iterations;
            report->gjk_iterations += hit.gjk_iterations;
        }
        if (!hit.hit()) { continue; }
        if (!best.hit || hit.t < best.t)
        {
            best.hit = true;
            best.t = hit.t;
            best.normal = hit.surface_normal;
            best.point = hit.point;
            best.body = index;
        }
    }
    return best;
}

int recover(character& c, const character_world& w, const character_config& cfg)
{
    int pushes = 0;
    const std::size_t n = std::min(w.bodies.size(), w.shapes.size());

    // A few passes, because pushing out of one thing can push into another; a
    // corner is two walls. Four is plenty for anything that is not a vice.
    for (int pass = 0; pass < 4; ++pass)
    {
        bool moved = false;
        for (std::size_t i = 0; i < n; ++i)
        {
            const auto index = static_cast<std::uint32_t>(i);
            if (index == w.self) { continue; }
            const rigid_body& b = w.bodies[i];
            if (b.kind == body_kind::dynamic) { continue; }   // the solver's: the proxy meets them

            const capsule here = character_capsule(c.position, cfg);
            aabb box = bounds_of(here);
            box.grow(cfg.skin);
            const shape& s = w.shapes[i];
            if (!overlaps(box, bounds_of(s, b.state.position, b.orientation))) { continue; }

            const placed_shape obstacle = place(s, b.state.position, b.orientation);
            const convex me = as_convex(here);
            const gjk_result g = gjk_distance(me, obstacle.view());
            vec3 push{};
            if (g.status == gjk_status::separated)
            {
                // Apart, but nearer than half a skin: casts from here would start
                // inside GJK's contact margin. Back off to a full skin.
                if (g.distance >= 0.5f * cfg.skin) { continue; }
                push = g.direction * -(cfg.skin - g.distance);
            }
            else
            {
                // Overlapping: 8.6's minimum translation, plus the skin. The axis
                // points from us toward it, so we move the other way.
                const separation s2 = collide(me, obstacle.view());
                push = s2.axis * -(s2.depth + cfg.skin);
            }
            c.position += push;
            ++pushes;
            moved = true;
        }
        if (!moved) { break; }
    }
    return pushes;
}

// ---------------------------------------------------------------------------
// Standing on things that move
// ---------------------------------------------------------------------------

body_step step_motion(const rigid_body& b, float h, spin_rule rule)
{
    body_step m;
    if (b.kind == body_kind::fixed)
    {
        m.pivot = b.state.position;
        return m;
    }
    m.translation = b.state.velocity * h;
    m.pivot = b.state.position - m.translation;
    // `advance_orientation(q)` is `Δ·q` for both rules — the linearised one
    // because normalising `(1 + ½hω)·q` is normalising `(1 + ½hω)` when `q` is
    // unit — so the step's rotation is what it does to the identity.
    m.rotation = advance_orientation(quat{}, b.angular_velocity, h, rule);
    return m;
}

vec3 carry_by_transform(vec3 p, const body_step& m)
{
    return m.pivot + m.translation + rotate(m.rotation, p - m.pivot);
}

vec3 carry_by_velocity(vec3 p, const rigid_body& b, float h)
{
    const vec3 pivot = b.state.position - b.state.velocity * h;
    return p + (b.state.velocity + cross(b.angular_velocity, p - pivot)) * h;
}

// ---------------------------------------------------------------------------
// The move
// ---------------------------------------------------------------------------

move_report move_character(character& c, vec3 displacement, float h, const character_world& w,
                           const character_config& cfg)
{
    move_report r;
    const vec3 start = c.position;
    const bool was_grounded = c.grounded;

    // 1. CARRY. The ground moved during the physics step that just ran; move
    //    with it, swept, so that a platform cannot carry us through a wall.
    if (was_grounded && c.ground_body < w.bodies.size() && cfg.carry != character_config::carry_rule::none)
    {
        const rigid_body& g = w.bodies[c.ground_body];
        if (g.kind != body_kind::fixed)
        {
            const body_step m = step_motion(g, h, w.spin);
            const vec3 target = cfg.carry == character_config::carry_rule::transform
                                    ? carry_by_transform(c.position, m)
                                    : carry_by_velocity(c.position, g, h);
            const vec3 before = c.position;
            slide_move(c.position, target - c.position, w, cfg, sweep_filter::solid, r);
            r.carried = c.position - before;
            r.carried_turn = m.rotation;
        }
    }

    // 2. RECOVER from whatever moved into us.
    r.recovered = recover(c, w, cfg);

    // 3. SIDEWAYS.
    c.grounded = false;
    vec3 lateral = horizontal(displacement);
    if (was_grounded && cfg.lay_along_ground && displacement.y <= 0.0f && walkable(c.ground_normal, cfg))
    {
        lateral = along_ground(lateral, c.ground_normal);
    }
    sideways(c, lateral, was_grounded && displacement.y <= 0.0f, w, cfg, r);
    const bool stepped = r.stepped;

    // 4. VERTICAL.
    vertical(c, displacement.y, w, cfg, r);

    // 5. GROUND. A rising character is not standing on anything, whatever is
    //    under it. Otherwise probe a skin's width below the skin.
    if (displacement.y <= 0.0f && !c.grounded)
    {
        const character_hit probe = sweep(c.position, k_up * -(2.0f * cfg.skin), w, cfg,
                                          sweep_filter::everything, &r);
        if (probe.hit && walkable(probe.normal, cfg))
        {
            c.position.y -= 2.0f * cfg.skin * probe.t;
            stand_on(c, probe);
        }
    }
    if (stepped) { c.grounded = true; }

    // ...and SNAP: it stood last step, it is not rising, and the ground has
    // gone. Kept only if it ends on floor.
    if (!c.grounded && was_grounded && displacement.y <= 0.0f && cfg.snap_distance > 0.0f)
    {
        snap_down(c, w, cfg, r);
    }

    r.landed = c.grounded && !was_grounded;
    if (!c.grounded) { c.ground_body = k_no_body; }
    c.velocity = (c.position - start) * (1.0f / h);
    return r;
}

void steer_proxy(rigid_body& proxy, vec3 target, float h)
{
    proxy.state.velocity = (target - proxy.state.position) * (1.0f / h);
    proxy.angular_velocity = vec3{};
}

// ---------------------------------------------------------------------------
// Closed forms
// ---------------------------------------------------------------------------

float free_curb_height(float radius, float max_slope) { return radius * (1.0f - std::cos(max_slope)); }

float edge_overhang(float radius, float max_slope) { return radius * std::sin(max_slope); }

float step_forward_min(float radius, float max_slope) { return radius * (1.0f - std::sin(max_slope)); }

float slope_creep_speed(float g, float h, float slope) { return g * h * std::sin(slope); }

float skip_speed(float g, float h, float slope, float reach) { return (g * h * h + reach) / (h * std::tan(slope)); }

float velocity_carry_growth(float omega, float h)
{
    const double x = static_cast<double>(omega) * static_cast<double>(h);
    if (x <= 0.0) { return 1.0f; }
    const double turn = 2.0 * std::atan(0.5 * x);         // one linearised step
    const double steps = 2.0 * 3.14159265358979323846 / turn;
    return static_cast<float>(std::pow(1.0 + x * x, 0.5 * steps));
}

float jump_speed(float g, float height) { return std::sqrt(2.0f * g * height); }

float stopping_distance(float speed, float friction, float g) { return speed * speed / (2.0f * friction * g); }

} // namespace engine::phys
