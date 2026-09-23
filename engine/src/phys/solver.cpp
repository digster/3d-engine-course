// engine/src/phys/solver.cpp — one contact resolved, and then all of them.
//
// Lessons 8.9 and 8.10. Read `solver.hpp` first; it carries the argument. The
// 8.9 half is mostly one loop over one array, and the interesting part is how
// few lines the physics turns out to be once the effective mass is a scalar:
// the normal solve is three lines and a `max`, and friction is the same three
// lines twice with a clip on the end.
//
// Two things in there are easy to get subtly wrong and both are commented at
// the point they happen: the impulse is applied with OPPOSITE signs to the two
// bodies (Newton's third law, and getting it wrong makes contacts suck), and the
// accumulated impulse is what gets clamped, not the increment.
//
// The 8.10 half adds four things and the third one is the surprise:
//
//   * the POSITION solve, which is the velocity solve run a second time against
//     a shadow velocity that only ever moves positions — twenty-four bytes per
//     body and the difference between a crate that rises out of the floor and
//     stops and one that rises out of the floor and leaves;
//   * ISLANDS, by a union-find that uses its own output array as its scratch,
//     with one rule that is the whole of it: a body that cannot move is not a
//     bridge, or the floor welds the level into a single island;
//   * SLEEPING, which is decided per island and BEFORE anything is prepared,
//     because deciding afterwards means doing all the work and discarding it;
//   * and the loop itself, which is thirty lines and whose only subtlety is
//     what is inside it and what is outside — warm starting once per step, all
//     the velocity iterations before any position iteration.
//
// Lesson 8.11 added joints to the loop and changed nothing else in it. Every
// stage that visits a manifold now visits the island's joints first, through
// `constraint.hpp`'s four functions; `build_islands` unions joint edges by the
// same rule as contact edges; and the class was renamed `constraint_solver`,
// with `contact_solver` kept as an alias. The contact half of this file is
// byte for byte what 8.10 shipped.

#include <engine/phys/solver.hpp>

#include <algorithm>   // std::clamp, std::max, std::min
#include <cmath>       // std::sqrt, std::atan, std::abs, std::pow, std::log
#include <limits>      // std::numeric_limits

namespace engine::phys
{
namespace
{

/// Radians to degrees. Local rather than shared: `math/rotation.hpp` has the
/// pair, and pulling a rotation header into the solver for one constant would
/// be a dependency for a multiply.
constexpr float k_rad_to_deg = 57.295779513082320876f;

/// The velocity of `b`'s material point relative to `a`'s, given lever arms that
/// have already been computed.
///
/// The hot inner quantity: `solve_contacts` evaluates it three times per point
/// per iteration, and 8.10 will evaluate it a great many more times than that.
/// It takes the lever arms rather than a world point because the constraint
/// already holds them and `p - position` would be two subtractions to recover
/// what is sitting in the struct.
[[nodiscard]] vec3 relative_at(const rigid_body& a, const rigid_body& b, vec3 r_a, vec3 r_b)
{
    return (b.state.velocity + cross(b.angular_velocity, r_b))
           - (a.state.velocity + cross(a.angular_velocity, r_a));
}

/// Apply `+impulse` to `b` and `-impulse` to `a`, at the contact point.
///
/// **THE OPPOSITE SIGNS ARE NEWTON'S THIRD LAW** and they are the reason a
/// contact conserves momentum exactly, for free, with no correction step: the
/// two linear changes are `+J*inv_m_b` and `-J*inv_m_a`, so the total change in
/// `m*v` summed over the pair is `J - J = 0` in exact arithmetic and to the last
/// bit in floating point, because the same `J` is used twice. §5 measures it.
///
/// An immovable body has `inv_mass == 0` and an all-zero inverse tensor, so both
/// of its lines are multiplications by zero. No branch, and 8.2 §3 chose the
/// reciprocal storage precisely so that this would be true.
void apply_impulse_pair(rigid_body& a, rigid_body& b, const contact_batch& batch,
                        const contact_constraint& c, vec3 impulse)
{
    a.state.velocity   = a.state.velocity - impulse * batch.inv_mass_a;
    a.angular_velocity = a.angular_velocity - batch.inv_inertia_a * cross(c.r_a, impulse);

    b.state.velocity   = b.state.velocity + impulse * batch.inv_mass_b;
    b.angular_velocity = b.angular_velocity + batch.inv_inertia_b * cross(c.r_b, impulse);
}

/// The velocity that this step's external acceleration added to one body.
///
/// Zero for anything that is not dynamic, and scaled by the body's own
/// `gravity_scale`, because `body_world::step` scales gravity that way and a
/// correction that did not would be correcting for a force that was never
/// applied.
[[nodiscard]] vec3 step_bias_of(const rigid_body& body, vec3 bias)
{
    return body.kind == body_kind::dynamic ? bias * body.gravity_scale : vec3{};
}

} // namespace

// ---------------------------------------------------------------------------
// Materials
// ---------------------------------------------------------------------------

const char* name_of(combine_rule rule)
{
    switch (rule)
    {
    case combine_rule::geometric_mean: return "geometric mean";
    case combine_rule::minimum:        return "minimum";
    case combine_rule::maximum:        return "maximum";
    case combine_rule::average:        return "average";
    }
    return "?";
}

float combine(float a, float b, combine_rule rule)
{
    switch (rule)
    {
    // `std::max(0.0f, ...)` guards a negative coefficient handed in by a
    // designer's slider: `sqrt` of a negative is a NaN, and a NaN in a friction
    // coefficient propagates into a velocity and then into a position, where it
    // presents as a body that has vanished rather than as a bad material.
    case combine_rule::geometric_mean: return std::sqrt(std::max(0.0f, a) * std::max(0.0f, b));
    case combine_rule::minimum:        return std::min(a, b);
    case combine_rule::maximum:        return std::max(a, b);
    case combine_rule::average:        return 0.5f * (a + b);
    }
    return 0.0f;
}

contact_material combine_material(contact_material a, contact_material b,
                                  combine_rule restitution_rule, combine_rule friction_rule)
{
    return {combine(a.restitution, b.restitution, restitution_rule),
            combine(a.friction, b.friction, friction_rule)};
}

const char* name_of(friction_model model)
{
    switch (model)
    {
    case friction_model::none: return "none";
    case friction_model::box:  return "box";
    case friction_model::cone: return "cone";
    }
    return "?";
}

const char* name_of(position_correction correction)
{
    switch (correction)
    {
    case position_correction::none:          return "none";
    case position_correction::baumgarte:     return "Baumgarte";
    case position_correction::split_impulse: return "split impulse";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

void tangent_basis(vec3 n, vec3& t1, vec3& t2)
{
    // Seed with whichever axis `n` is LEAST aligned with. The cross product of
    // two nearly parallel vectors is nearly zero and normalising it amplifies
    // whatever rounding error it contains, so the whole trick is never to take
    // one. Picking the smallest component of `n` guarantees the seed axis is at
    // least 54.7 degrees away — the worst case being a normal pointing along a
    // cube's diagonal, where all three components are equal — so the cross
    // product never has length below sin(54.7 deg) = 0.816.
    const vec3 seed = (std::abs(n.x) <= std::abs(n.y) && std::abs(n.x) <= std::abs(n.z))
                          ? vec3{1.0f, 0.0f, 0.0f}
                          : (std::abs(n.y) <= std::abs(n.z) ? vec3{0.0f, 1.0f, 0.0f}
                                                            : vec3{0.0f, 0.0f, 1.0f});

    t1 = normalised(cross(n, seed));
    // No normalisation needed on the second: `n` is unit, `t1` is unit, and they
    // are perpendicular, so their cross product is unit already. Normalising it
    // anyway would cost a square root to fix an error that is not there — and
    // §7 measures the orthonormality that results over a million normals.
    t2 = cross(n, t1);
}

vec3 contact_velocity(const rigid_body& a, const rigid_body& b, vec3 world_point)
{
    return point_velocity(b, world_point) - point_velocity(a, world_point);
}

float effective_mass(float inv_mass_a, const mat3& inv_inertia_a, vec3 r_a,
                     float inv_mass_b, const mat3& inv_inertia_b, vec3 r_b,
                     vec3 direction)
{
    // The two angular terms, in the rearranged form the header derives:
    // `w . (inv_I * w)` with `w = r x dir`. Manifestly non-negative for a
    // positive semi-definite tensor, which is what makes the division below
    // safe to do without a guard on the sign.
    const vec3 wa = cross(r_a, direction);
    const vec3 wb = cross(r_b, direction);

    const float k = inv_mass_a + inv_mass_b
                    + dot(wa, inv_inertia_a * wa)
                    + dot(wb, inv_inertia_b * wb);

    // `k` is zero exactly when both bodies are immovable, because every term is
    // non-negative and the two inverse masses are the only ones that can be
    // positive on their own. Returning zero rather than an infinity means the
    // impulse computed from it is zero, which is the right answer: no impulse
    // can change the relative velocity of two static bodies.
    return k > 0.0f ? 1.0f / k : 0.0f;
}

float effective_mass(const rigid_body& a, const rigid_body& b, vec3 r_a, vec3 r_b,
                     vec3 direction)
{
    return effective_mass(a.inv_mass, world_inv_inertia(a), r_a,
                          b.inv_mass, world_inv_inertia(b), r_b, direction);
}

// ---------------------------------------------------------------------------
// Prepare
// ---------------------------------------------------------------------------

contact_batch prepare_contacts(const rigid_body& a, const rigid_body& b,
                               const contact_manifold& m, const contact_material& material,
                               const solver_config& cfg)
{
    contact_batch batch;
    batch.normal = m.normal;
    batch.material = material;
    batch.count = std::min(m.count, k_max_clip_points);

    // The hoist. Two basis changes for the whole manifold instead of two per
    // effective-mass call — see `contact_batch`, and §13 for what it is worth.
    batch.inv_mass_a = a.inv_mass;
    batch.inv_mass_b = b.inv_mass;
    batch.inv_inertia_a = world_inv_inertia(a);
    batch.inv_inertia_b = world_inv_inertia(b);

    tangent_basis(batch.normal, batch.tangent[0], batch.tangent[1]);

    // ---- restitution, decided once for the manifold -----------------------
    //
    // A manifold has ONE restitution, not one per point, and it is decided by
    // the deepest-approaching point rather than by each point's own approach
    // speed. Per-point restitution sounds more careful and is worse: the four
    // corners of a crate landing flat approach at slightly different speeds, so
    // a per-point threshold makes two corners bouncy and two dead on the frame
    // where the speed straddles it, and the crate takes off sideways.
    float worst_approach = 0.0f;
    for (int i = 0; i < batch.count; ++i)
    {
        const vec3 p = m.points[i].position;
        const vec3 r_a = p - a.state.position;
        const vec3 r_b = p - b.state.position;
        const float vn = dot(relative_at(a, b, r_a, r_b), batch.normal);
        worst_approach = std::min(worst_approach, vn);
    }

    // The step's own contribution to the RELATIVE normal velocity. Note the
    // subtraction: if both bodies are dynamic and unscaled, gravity gave each of
    // them the same velocity and the relative velocity it produced is exactly
    // zero — so two crates colliding in mid-air need no correction at all, and
    // this expression says so without a special case. §6's control is precisely
    // that pair.
    const float bias_n = dot(step_bias_of(b, cfg.restitution_bias)
                                 - step_bias_of(a, cfg.restitution_bias),
                             batch.normal);

    // The speed the contact really ARRIVED with, as opposed to the speed it has
    // by the time the solver is looking at it.
    const float arrival = worst_approach - bias_n;

    batch.restitution_used = (arrival < -cfg.restitution_threshold) ? material.restitution : 0.0f;

    // ---- per point --------------------------------------------------------
    for (int i = 0; i < batch.count; ++i)
    {
        const contact_point& cp = m.points[i];
        contact_constraint& c = batch.points[i];

        c.point_index = i;
        c.r_a = cp.position - a.state.position;
        c.r_b = cp.position - b.state.position;

        // Lesson 8.10. The same overlap, signed the way a position solve wants
        // it: negative is penetration. `prepare_bias` turns it into a velocity
        // and needs the step length to do so, which is why that is a separate
        // call — see its header comment.
        c.separation = -cp.depth;
        c.bias = 0.0f;
        c.pseudo_impulse = 0.0f;

        c.normal_mass = effective_mass(batch.inv_mass_a, batch.inv_inertia_a, c.r_a,
                                       batch.inv_mass_b, batch.inv_inertia_b, c.r_b,
                                       batch.normal);
        for (int t = 0; t < 2; ++t)
        {
            c.tangent_mass[t] = effective_mass(batch.inv_mass_a, batch.inv_inertia_a, c.r_a,
                                               batch.inv_mass_b, batch.inv_inertia_b, c.r_b,
                                               batch.tangent[t]);
        }

        c.initial_normal_velocity = dot(relative_at(a, b, c.r_a, c.r_b), batch.normal);

        // The target. Zero is "stop approaching"; a negative multiple of the
        // arrival speed is "come back off at `e` times the speed you hit with".
        // Restitution is applied to the CORRECTED arrival speed for the reason
        // `solver_config::restitution_bias` gives, and `min(0, ...)` keeps a
        // point that was already separating from being told to approach.
        const float point_arrival = c.initial_normal_velocity - bias_n;
        c.target_normal_velocity =
            batch.restitution_used > 0.0f ? -batch.restitution_used * std::min(0.0f, point_arrival)
                                          : 0.0f;

        if (cfg.warm_start)
        {
            c.normal_impulse = cp.normal_impulse;

            // *** THE TANGENT IMPULSES ARE COORDINATES, AND THEY ARRIVE IN
            // *** SOMEBODY ELSE'S FRAME. ***  Lesson 8.10.
            //
            // `tangent_basis` branches on the smallest component of the normal,
            // so on a near-vertical contact the choice is decided by whether
            // |n.x| or |n.z| is smaller — two numbers that are both around
            // 1e-5 on a settled crate and cross each other constantly. When
            // they cross, the basis rotates by 90 degrees, and last frame's
            // friction, applied as though nothing had happened, acts SIDEWAYS.
            // Measured at 2.1% of manifold-frames on a ten-crate tower, worst
            // case 148 degrees, and the tower fell over in eight seconds.
            //
            // The fix is two dot products: rebuild the world-space impulse in
            // the frame it was measured in, then re-measure it in this one. It
            // is exact, and it is the reason `contact_manifold` now carries the
            // basis at all.
            if (length_squared(m.tangent[0]) > 0.5f)
            {
                const vec3 world = m.tangent[0] * cp.tangent_impulse[0]
                                   + m.tangent[1] * cp.tangent_impulse[1];
                c.tangent_impulse[0] = dot(world, batch.tangent[0]);
                c.tangent_impulse[1] = dot(world, batch.tangent[1]);
            }
            else
            {
                // No basis stored: the manifold has never been through a
                // solver, so the impulses are zero and the frame is moot.
                c.tangent_impulse[0] = cp.tangent_impulse[0];
                c.tangent_impulse[1] = cp.tangent_impulse[1];
            }
        }
    }

    return batch;
}

// ---------------------------------------------------------------------------
// Solve
// ---------------------------------------------------------------------------

namespace
{

/// One point's normal solve. Returns the impulse magnitude actually applied,
/// which may be negative when an accumulated impulse is being unwound.
float solve_one_normal(rigid_body& a, rigid_body& b, contact_batch& batch, contact_constraint& c,
                       float bias)
{
    const float vn = dot(relative_at(a, b, c.r_a, c.r_b), batch.normal);

    // The whole of the normal solve. `normal_mass` is `1/k` and `k` is the
    // change in `vn` per unit impulse, so this is a division dressed as a
    // multiply: the impulse that moves `vn` from where it is to where it should
    // be.
    //
    // Lesson 8.10 added `bias`, and it is zero unless the correction is
    // Baumgarte. It is a velocity the contact was never asked for, added to the
    // target so that a penetrating contact pushes apart rather than merely
    // stopping — and the solver cannot tell it from a real one, which is
    // exactly the complaint against it. See `position_correction`.
    float delta = (c.target_normal_velocity + bias - vn) * c.normal_mass;

    // *** CLAMP THE ACCUMULATED IMPULSE, NOT THE INCREMENT. ***
    //
    // A contact may push and may not pull, so the TOTAL must be non-negative —
    // but an individual increment is allowed to be negative, because a later
    // point may have overshot and this point is entitled to take some of its own
    // impulse back. Clamping `delta` instead of the total is the classic bug: it
    // makes every increment a push, the accumulator ratchets upward, and a stack
    // of boxes slowly launches itself. On the single pass this lesson makes, the
    // two are identical whenever the accumulator starts at zero; with warm
    // starting or with 8.10's iterations they are not, and this file is written
    // for the version that is coming.
    const float total = std::max(0.0f, c.normal_impulse + delta);
    delta = total - c.normal_impulse;
    c.normal_impulse = total;

    apply_impulse_pair(a, b, batch, c, batch.normal * delta);
    return delta;
}

/// One point's friction solve. Returns the magnitude of the tangential impulse
/// actually applied.
float solve_one_friction(rigid_body& a, rigid_body& b, contact_batch& batch,
                         contact_constraint& c, friction_model model)
{
    if (model == friction_model::none || batch.material.friction <= 0.0f)
    {
        return 0.0f;
    }

    // Coulomb's radius is `mu` times THIS point's accumulated normal impulse.
    // Not the manifold's total, and not last frame's: a corner that is barely
    // touching gets barely any friction, which is what stops a tilting crate
    // from gripping on the corner that has just left the floor.
    const float limit = batch.material.friction * c.normal_impulse;

    const vec3 u = relative_at(a, b, c.r_a, c.r_b);

    // Both tangents are solved from the SAME relative velocity, sampled once,
    // rather than one after the other. Solving them sequentially would make the
    // pair of impulses depend on the order of the two axes — and the axes came
    // out of `tangent_basis`, which chose them from the normal's smallest
    // component, so the physics would depend on which way the floor faces.
    float total[2];
    float delta[2];
    for (int t = 0; t < 2; ++t)
    {
        delta[t] = -dot(u, batch.tangent[t]) * c.tangent_mass[t];
        total[t] = c.tangent_impulse[t] + delta[t];
    }

    c.sliding = false;
    if (model == friction_model::cone)
    {
        const float magnitude = std::sqrt(total[0] * total[0] + total[1] * total[1]);
        if (magnitude > limit)
        {
            // Scale both components by one factor, which is the projection onto
            // the disc. Note that this is the projection in the IMPULSE metric
            // and not in the velocity one — the two tangent effective masses
            // differ, so the mathematically exact projection would be onto an
            // ellipse. Every engine does it this way; the error only appears on
            // a contact whose lever arm is strongly anisotropic, and it makes
            // the friction slightly wrong in direction rather than wrong in
            // magnitude. §7 measures it.
            const float scale = magnitude > 0.0f ? limit / magnitude : 0.0f;
            total[0] *= scale;
            total[1] *= scale;
            c.sliding = true;
        }
    }
    else   // friction_model::box
    {
        for (int t = 0; t < 2; ++t)
        {
            const float clipped = std::clamp(total[t], -limit, limit);
            if (clipped != total[t]) { c.sliding = true; }
            total[t] = clipped;
        }
    }

    vec3 impulse{};
    for (int t = 0; t < 2; ++t)
    {
        impulse = impulse + batch.tangent[t] * (total[t] - c.tangent_impulse[t]);
        c.tangent_impulse[t] = total[t];
    }

    apply_impulse_pair(a, b, batch, c, impulse);
    return length(impulse);
}

} // namespace

void warm_start_contacts(rigid_body& a, rigid_body& b, contact_batch& batch)
{
    for (int i = 0; i < batch.count; ++i)
    {
        contact_constraint& c = batch.points[i];
        const vec3 impulse = batch.normal * c.normal_impulse
                             + batch.tangent[0] * c.tangent_impulse[0]
                             + batch.tangent[1] * c.tangent_impulse[1];
        apply_impulse_pair(a, b, batch, c, impulse);
    }
}

solve_report solve_contacts(rigid_body& a, rigid_body& b, contact_batch& batch,
                            const solver_config& cfg)
{
    solve_report report;

    // Two separate loops, normals first. Friction's clip needs a normal impulse
    // to clip against, and interleaving them per point would give point 0's
    // friction a radius while point 3's normal had not been solved yet — which
    // is not wrong so much as arbitrary, and arbitrary is what makes a crate
    // land differently depending on which corner the clipper emitted first.
    // Lesson 8.10. The Baumgarte bias is the ONLY correction that touches a
    // real velocity, so it is switched on here and nowhere else: under
    // `split_impulse` the same number drives `solve_positions` instead, and
    // under `none` `prepare_bias` never wrote it.
    const bool use_bias = cfg.correction == position_correction::baumgarte;

    if (cfg.normal_before_friction)
    {
        for (int i = 0; i < batch.count; ++i)
        {
            contact_constraint& c = batch.points[i];
            report.normal_impulse += solve_one_normal(a, b, batch, c, use_bias ? c.bias : 0.0f);
        }
        for (int i = 0; i < batch.count; ++i)
        {
            report.friction_impulse += solve_one_friction(a, b, batch, batch.points[i], cfg.friction);
        }
    }
    else
    {
        // The wrong order, kept because 8.9 §10 measures it. With no warm start
        // the cone radius is zero here and friction does nothing at all on the
        // frame of first contact.
        for (int i = 0; i < batch.count; ++i)
        {
            report.friction_impulse += solve_one_friction(a, b, batch, batch.points[i], cfg.friction);
        }
        for (int i = 0; i < batch.count; ++i)
        {
            contact_constraint& c = batch.points[i];
            report.normal_impulse += solve_one_normal(a, b, batch, c, use_bias ? c.bias : 0.0f);
        }
    }

    for (int i = 0; i < batch.count; ++i)
    {
        const contact_constraint& c = batch.points[i];
        if (c.normal_impulse > 0.0f) { ++report.pushing_points; }
        if (c.sliding)               { ++report.sliding_points; }

        // The residual, measured AFTER every point has been solved, which is the
        // only measurement that means anything: a point's own solve leaves its
        // velocity exactly on target, and then the next point's impulse moves it
        // again. On one point this is zero to float precision; on four it is
        // not, and the size of it is 8.10's reason for existing.
        const float vn = dot(relative_at(a, b, c.r_a, c.r_b), batch.normal);
        const float residual = std::abs(vn - c.target_normal_velocity);

        // A point whose accumulated impulse is zero is not being violated when
        // its velocity is above target — it is simply separating, which is
        // allowed. Counting that as a residual would report a large error on
        // every contact that is coming apart.
        if (c.normal_impulse > 0.0f || vn < c.target_normal_velocity)
        {
            report.max_residual = std::max(report.max_residual, residual);
        }
    }

    return report;
}

void write_back(const contact_batch& batch, contact_manifold& m)
{
    // The frame the tangent impulses below are expressed in. Lesson 8.10, and
    // it is written whether or not any point pushed, because next frame's
    // `prepare_contacts` needs to know which way "tangent 0" pointed even when
    // the answer was zero.
    m.tangent[0] = batch.tangent[0];
    m.tangent[1] = batch.tangent[1];

    for (int i = 0; i < batch.count; ++i)
    {
        const contact_constraint& c = batch.points[i];
        if (c.point_index < 0 || c.point_index >= m.count) { continue; }

        contact_point& cp = m.points[c.point_index];
        cp.normal_impulse = c.normal_impulse;
        cp.tangent_impulse[0] = c.tangent_impulse[0];
        cp.tangent_impulse[1] = c.tangent_impulse[1];
    }
}

solve_report resolve_contact(rigid_body& a, rigid_body& b, contact_manifold& m,
                             const contact_material& material, const solver_config& cfg)
{
    contact_batch batch = prepare_contacts(a, b, m, material, cfg);
    if (cfg.warm_start) { warm_start_contacts(a, b, batch); }
    const solve_report report = solve_contacts(a, b, batch, cfg);
    write_back(batch, m);
    return report;
}

// ---------------------------------------------------------------------------
// The position solve — Lesson 8.10
// ---------------------------------------------------------------------------

void prepare_bias(contact_batch& batch, float h, const solver_config& cfg)
{
    // `h <= 0` is a caller bug rather than a mode, but dividing by it here
    // would put an infinity into a velocity and then a NaN into a position,
    // where it presents as a body that has vanished rather than as a bad step.
    if (cfg.correction == position_correction::none || !(h > 0.0f))
    {
        for (int i = 0; i < batch.count; ++i) { batch.points[i].bias = 0.0f; }
        return;
    }

    const float inv_h = 1.0f / h;
    for (int i = 0; i < batch.count; ++i)
    {
        contact_constraint& c = batch.points[i];

        // The slop comes off FIRST and the result is clamped at zero, so a
        // contact resting at exactly the slop asks for nothing at all. That
        // subtraction is the difference between a stack that sits still and a
        // stack that breathes at 60 Hz: drive the overlap to zero and the
        // contact separates, gravity puts it back next frame, and the cycle has
        // no fixed point. 8.10 §8 measures a five-crate tower breathing
        // 1.80 mm peak to peak at zero slop and 0.00 at 2 mm — and measures ONE
        // crate sitting perfectly still at zero slop, which is the folklore's
        // usual example and is the one case where the knob buys nothing.
        const float excess = std::max(0.0f, -c.separation - cfg.penetration_slop);

        // A velocity, not a displacement: `excess` metres removed over `h`
        // seconds is `excess/h` metres per second, and `beta` of it is what we
        // ask for this step. The clamp is what turns a body spawned a metre
        // inside a wall into a push rather than a launch — see
        // `solver_config::max_correction_speed`.
        c.bias = std::min(cfg.baumgarte * excess * inv_h, cfg.max_correction_speed);
    }
}

float solve_positions(contact_batch& batch, pseudo_velocity& pa, pseudo_velocity& pb,
                      const solver_config& cfg)
{
    (void)cfg;   // the bias was baked by `prepare_bias`; nothing else is read

    float worst = 0.0f;
    for (int i = 0; i < batch.count; ++i)
    {
        contact_constraint& c = batch.points[i];
        worst = std::max(worst, -c.separation);

        // The relative PSEUDO velocity at this point. Structurally identical to
        // `relative_at`, and pointedly not calling it: the quantity it reads
        // lives in the shadow array rather than in the bodies, and a solve that
        // could accidentally read the real velocity here is a solve that would
        // silently become Baumgarte.
        const vec3 u = (pb.linear + cross(pb.angular, c.r_b))
                       - (pa.linear + cross(pa.angular, c.r_a));
        const float vn = dot(u, batch.normal);

        // Same three lines as the velocity solve, with the bias as the target
        // and `pseudo_impulse` as the accumulator. Every point is solved, not
        // just the penetrating ones: a point resting at the slop has a target
        // of zero, and solving it is what stops a NEIGHBOURING point's
        // correction from levering this corner further into the floor.
        float delta = (c.bias - vn) * c.normal_mass;

        // Clamp the accumulated pseudo-impulse, never the increment — 8.9's
        // rule, and it applies here for the identical reason: a position
        // correction may push and may not pull, but an individual increment is
        // entitled to be negative when a later point has overshot.
        const float total = std::max(0.0f, c.pseudo_impulse + delta);
        delta = total - c.pseudo_impulse;
        c.pseudo_impulse = total;

        const vec3 impulse = batch.normal * delta;
        pa.linear  = pa.linear  - impulse * batch.inv_mass_a;
        pa.angular = pa.angular - batch.inv_inertia_a * cross(c.r_a, impulse);
        pb.linear  = pb.linear  + impulse * batch.inv_mass_b;
        pb.angular = pb.angular + batch.inv_inertia_b * cross(c.r_b, impulse);
    }
    return worst;
}

void apply_pseudo_velocity(rigid_body& b, pseudo_velocity& p, float h, spin_rule spin)
{
    if (b.kind == body_kind::dynamic)
    {
        b.state.position = b.state.position + p.linear * h;

        // The rotation half, and leaving it out is a specific, recognisable
        // bug rather than a small error: a crate resting on one corner is
        // corrected at that corner, the correction is off-centre, and what the
        // contact actually wants is for the crate to LEVEL. Translate only and
        // it climbs out of the floor sideways, sliding along a surface it is
        // not sliding on.
        b.orientation = advance_orientation(b.orientation, p.angular, h, spin);
    }

    // Cleared unconditionally, including for the fixed and kinematic bodies
    // whose entries were written into and then ignored. The array is scratch
    // for one step and the next step's `begin` must not inherit it.
    p = pseudo_velocity{};
}

// ---------------------------------------------------------------------------
// Islands — Lesson 8.10
// ---------------------------------------------------------------------------

namespace
{

/// Union–find `find` with **path halving**: every second link on the way up is
/// re-pointed at its grandparent.
///
/// Halving rather than full compression because it needs no second pass and no
/// recursion, and it gives the same near-constant amortised behaviour. The
/// array it walks is `island_of` itself — see `build_islands`.
int find_root(std::vector<int>& parent, int x)
{
    while (parent[x] != x)
    {
        parent[x] = parent[parent[x]];
        x = parent[x];
    }
    return x;
}

/// Join the islands of two bodies, if the edge between them is an edge.
///
/// Lesson 8.11 lifted this out of the contact loop so that joints could use it
/// too, unchanged — which is the point: a joint is an edge of the island graph
/// by EXACTLY the rule a contact is, and writing the rule twice is how the two
/// would one day disagree.
void union_edge(std::vector<int>& island_of, int n, std::uint32_t body_a, std::uint32_t body_b)
{
    const int a = static_cast<int>(body_a);
    const int b = static_cast<int>(body_b);
    if (a < 0 || a >= n || b < 0 || b >= n) { return; }

    // BOTH ends must be able to move. An impulse applied to a fixed or
    // kinematic body changes nothing any other constraint can read — its
    // velocity is never written — so it propagates nothing and joins nothing.
    // The floor is in every contact in the scene and in no island; the ceiling
    // a lamp hangs from is in every lamp's joint and in no island either.
    if (island_of[static_cast<std::size_t>(a)] < 0) { return; }
    if (island_of[static_cast<std::size_t>(b)] < 0) { return; }

    const int ra = find_root(island_of, a);
    const int rb = find_root(island_of, b);
    if (ra == rb) { return; }

    // Union by INDEX rather than by size: the smaller index wins, always.
    // It is one comparison instead of a second array, and it costs a
    // theoretically worse tree — which path halving flattens anyway, and
    // which 8.10 §9 prices, labelling included, at 2.5 ns per body.
    if (ra < rb) { island_of[static_cast<std::size_t>(rb)] = ra; }
    else         { island_of[static_cast<std::size_t>(ra)] = rb; }
}

} // namespace

int build_islands(std::span<const rigid_body> bodies, std::span<const contact_pair> contacts,
                  std::vector<int>& island_of)
{
    return build_islands(bodies, contacts, std::span<const joint_pair>{}, island_of);
}

int build_islands(std::span<const rigid_body> bodies, std::span<const contact_pair> contacts,
                  std::span<const joint_pair> joints, std::vector<int>& island_of)
{
    const int n = static_cast<int>(bodies.size());

    // *** THE OUTPUT ARRAY IS THE SCRATCH. ***
    //
    // A union–find wants a parent array of `n` ints and the answer is an island
    // label array of `n` ints, and they are never both live: the parents are
    // finished with the moment the labels start. So `island_of` is the parent
    // array for the first half of this function and the answer for the second,
    // and the whole thing allocates nothing on a scene whose size has not
    // changed. `constraint_solver` calls this every step.
    island_of.assign(static_cast<std::size_t>(n), -1);
    for (int i = 0; i < n; ++i)
    {
        // A body that cannot move is not a node of this graph at all: -1 now,
        // -1 at the end. See the header — a fixed body is not a bridge, and
        // that one rule is the difference between twenty islands and one.
        if (bodies[static_cast<std::size_t>(i)].kind == body_kind::dynamic)
        {
            island_of[static_cast<std::size_t>(i)] = i;
        }
    }

    // ---- union over the contact edges, then the joint edges ----------------
    //
    // Lesson 8.11 added the second loop. A joint joins two bodies that need
    // not be touching — the links of a chain, a door and its frame — and
    // leaving it out of the graph is the bug §12 measures: the chain goes to
    // sleep one link at a time, and a sleeping link is an immovable body to its
    // neighbour.
    for (const contact_pair& p : contacts) { union_edge(island_of, n, p.body_a, p.body_b); }
    for (const joint_pair& p : joints)     { union_edge(island_of, n, p.body_a, p.body_b); }

    // ---- turn roots into dense labels, in place ---------------------------
    //
    // Four passes and no second array. A root is a self-parent; give each one a
    // label encoded as a NEGATIVE number so that it cannot be mistaken for a
    // parent index; then every non-root reads its root's slot and copies the
    // encoding; then the whole array is decoded at once.
    //
    // *** THE ENCODING IS `-label - 2` AND THE 2 IS NOT A TYPO. *** The obvious
    // `-label - 1` sends label 0 to -1, which is already this array's sentinel
    // for "no island" — so the FIRST island in every scene silently ceased to
    // exist, its contacts were never grouped and never solved, and a tower
    // whose bottom crate happened to land in it fell through the floor while
    // every other tower stood. It cost an hour, and what found it was not the
    // falling crate but `stats.points == 0` on a frame where the manifold was
    // plainly there: an instrument that disagreed with another instrument.
    for (int i = 0; i < n; ++i)
    {
        const std::size_t u = static_cast<std::size_t>(i);
        if (island_of[u] >= 0) { island_of[u] = find_root(island_of, i); }
    }

    int count = 0;
    for (int i = 0; i < n; ++i)
    {
        const std::size_t u = static_cast<std::size_t>(i);
        if (island_of[u] == i) { island_of[u] = -count - 2; ++count; }
    }

    for (int i = 0; i < n; ++i)
    {
        const std::size_t u = static_cast<std::size_t>(i);
        if (island_of[u] >= 0)
        {
            island_of[u] = island_of[static_cast<std::size_t>(island_of[u])];
        }
    }

    for (int i = 0; i < n; ++i)
    {
        const std::size_t u = static_cast<std::size_t>(i);
        if (island_of[u] <= -2) { island_of[u] = -island_of[u] - 2; }
    }

    return count;
}

// ---------------------------------------------------------------------------
// Sleeping — Lesson 8.10
// ---------------------------------------------------------------------------

int wake_islands(std::span<rigid_body> bodies, std::span<const std::uint32_t> island_bodies,
                 std::span<island> islands)
{
    int sleeping_islands = 0;

    for (island& isl : islands)
    {
        // An island is asleep only if EVERY body in it already is. One awake
        // member and the whole island is awake — which IS the waking rule, and
        // it needs no code of its own: a moving body that collides with a
        // sleeping pile is joined to it by `build_islands`, so the pile's
        // island now contains something awake and every one of its bodies is
        // cleared two lines below.
        bool asleep = isl.body_count > 0;
        for (int k = 0; asleep && k < isl.body_count; ++k)
        {
            if (!bodies[island_bodies[static_cast<std::size_t>(isl.first_body + k)]].sleeping)
            {
                asleep = false;
            }
        }

        isl.sleeping = asleep;
        if (asleep)
        {
            ++sleeping_islands;
            continue;
        }

        for (int k = 0; k < isl.body_count; ++k)
        {
            rigid_body& b = bodies[island_bodies[static_cast<std::size_t>(isl.first_body + k)]];
            if (b.sleeping) { wake(b); }
        }
    }

    return sleeping_islands;
}

int update_sleep(std::span<rigid_body> bodies, std::span<const std::uint32_t> island_bodies,
                 std::span<island> islands, float h, const sleep_config& cfg)
{
    int sleeping_islands = 0;

    for (island& isl : islands)
    {
        // An island already asleep stays asleep and its timers stand still —
        // `wake_islands` is the only thing that takes it out, and it did not.
        // Except when sleeping has been switched off underneath it, which has
        // to wake the world or the switch is one-way.
        if (isl.sleeping)
        {
            if (!cfg.enabled)
            {
                isl.sleeping = false;
                for (int k = 0; k < isl.body_count; ++k)
                {
                    wake(bodies[island_bodies[static_cast<std::size_t>(isl.first_body + k)]]);
                }
            }
            else
            {
                ++sleeping_islands;
                continue;
            }
        }

        // ---- is every body in it quiet, this instant? ---------------------
        //
        // One veto is enough, which is the whole of the all-or-nothing rule: a
        // single moving crate at the bottom of a pile keeps the pile awake, and
        // that is not a compromise — it is the correct answer, because the pile
        // is about to move.
        bool quiet = cfg.enabled;
        for (int k = 0; quiet && k < isl.body_count; ++k)
        {
            const rigid_body& b = bodies[island_bodies[static_cast<std::size_t>(isl.first_body + k)]];
            if (!b.allow_sleep
                || !is_sleep_candidate(b, cfg.linear_threshold, cfg.angular_threshold))
            {
                quiet = false;
            }
        }

        // ---- advance or reset every timer, and ask whether all are ripe ----
        bool ripe = quiet;
        for (int k = 0; k < isl.body_count; ++k)
        {
            rigid_body& b = bodies[island_bodies[static_cast<std::size_t>(isl.first_body + k)]];
            if (quiet)
            {
                b.sleep_timer += h;
                if (b.sleep_timer < cfg.time_to_sleep) { ripe = false; }
            }
            else
            {
                b.sleep_timer = 0.0f;
            }
        }

        isl.sleeping = ripe;

        for (int k = 0; k < isl.body_count; ++k)
        {
            rigid_body& b = bodies[island_bodies[static_cast<std::size_t>(isl.first_body + k)]];
            b.sleeping = ripe;
            if (ripe)
            {
                // *** ASLEEP IS EXACTLY AT REST, AND THE ZEROING IS THE
                // *** DIFFERENCE BETWEEN THE TWO.
                //
                // A body frozen at 4 cm/s resumes at 4 cm/s when something
                // wakes it half a second later, in a direction that no longer
                // means anything — and until then its momentum keeps appearing
                // in `step_report`, so the scene's conservation laws report a
                // drift that nothing is causing. Two lines, and they make
                // "asleep" a state rather than a pause.
                b.state.velocity = vec3{};
                b.angular_velocity = vec3{};
            }
        }

        // Waking is NOT here. It happened in `wake_islands`, before the solve,
        // because an island that has just acquired a moving neighbour has to be
        // solved on the frame it acquires it and not the frame after. What this
        // function cannot see at all is a change nobody collided about — a
        // teleport, a velocity assignment, a force applied to a sleeping body
        // that is therefore never integrated. Those are what `wake` is for.

        if (ripe) { ++sleeping_islands; }
    }

    return sleeping_islands;
}

// ---------------------------------------------------------------------------
// The solver — Lessons 8.10 and 8.11
// ---------------------------------------------------------------------------

void constraint_solver::begin(std::span<rigid_body> bodies)
{
    bodies_ = bodies;

    // `clear` and not `= {}`: a vector cleared keeps its capacity, so a scene
    // of a settled size allocates on its first step and never again. 8.8's grid
    // makes the same contract in the same words, and `constraint_solver::clear`
    // is the way to actually give the memory back.
    pairs_.clear();
    batches_.clear();
    joints_.clear();
    joint_batches_.clear();
    stats_ = solver_stats{};
}

void constraint_solver::add(std::uint32_t body_a, std::uint32_t body_b, contact_manifold& m,
                            const contact_material& material)
{
    // An empty manifold is not a contact. Dropping it here rather than in the
    // solve loop keeps it out of the contact graph too, which matters: a pair
    // that the broadphase reported and the narrow phase rejected must not join
    // two islands, or a crate would keep the crate it is about to miss awake.
    if (m.count <= 0) { return; }

    pairs_.push_back(contact_pair{body_a, body_b, &m, material});
}

void constraint_solver::add(std::uint32_t body_a, std::uint32_t body_b, joint& j)
{
    // No equivalent of the empty-manifold test: a joint always constrains, and
    // it is an edge of the island graph whether or not it is under load. A
    // chain hanging perfectly still is still one chain.
    joints_.push_back(joint_pair{body_a, body_b, &j});
}

const solver_stats& constraint_solver::solve(float h, const solver_config& cfg,
                                             const sleep_config& sleep)
{
    stats_ = solver_stats{};
    stats_.manifolds = static_cast<int>(pairs_.size());
    stats_.joints = static_cast<int>(joints_.size());

    const std::size_t body_count = bodies_.size();

    // ---- 1. the constraint graph ---------------------------------------------
    //
    // Contacts and joints are both edges, by one rule. See `build_islands`.
    stats_.islands = build_islands(bodies_, pairs_, joints_, island_of_);

    // Group the bodies by island: count, prefix-sum, scatter. A counting sort,
    // exactly as 8.8's grid sorts proxies into cells, and for the same payoff —
    // every island's members end up adjacent in one contiguous array, so the
    // sleep pass and (in Module 9) a job system can each take a range.
    islands_.assign(static_cast<std::size_t>(stats_.islands), island{});
    for (std::size_t i = 0; i < body_count; ++i)
    {
        const int isl = island_of_[i];
        if (isl >= 0) { ++islands_[static_cast<std::size_t>(isl)].body_count; }
    }
    int running = 0;
    for (island& isl : islands_)
    {
        isl.first_body = running;
        running += isl.body_count;
        isl.body_count = 0;   // reused as a fill cursor, restored by the scatter
    }
    island_bodies_.assign(static_cast<std::size_t>(running), 0u);
    for (std::size_t i = 0; i < body_count; ++i)
    {
        const int isl = island_of_[i];
        if (isl < 0) { continue; }
        island& it = islands_[static_cast<std::size_t>(isl)];
        island_bodies_[static_cast<std::size_t>(it.first_body + it.body_count)] =
            static_cast<std::uint32_t>(i);
        ++it.body_count;
    }

    // The same sort over the contacts, and then over the joints. An edge's
    // island is whichever of its two bodies has one; an edge between two
    // immovable bodies has none and is dropped, which is a level-design bug
    // reported by its absence.
    const auto edge_island = [this](std::uint32_t a, std::uint32_t b) {
        const int ia = island_of_[a];
        return ia >= 0 ? ia : island_of_[b];
    };

    for (island& isl : islands_) { isl.contact_count = 0; }
    for (const contact_pair& p : pairs_)
    {
        const int isl = edge_island(p.body_a, p.body_b);
        if (isl >= 0) { ++islands_[static_cast<std::size_t>(isl)].contact_count; }
    }
    running = 0;
    for (island& isl : islands_)
    {
        isl.first_contact = running;
        running += isl.contact_count;
        isl.contact_count = 0;
    }
    island_contacts_.assign(static_cast<std::size_t>(running), 0);
    for (std::size_t ci = 0; ci < pairs_.size(); ++ci)
    {
        const int isl = edge_island(pairs_[ci].body_a, pairs_[ci].body_b);
        if (isl < 0) { continue; }
        island& it = islands_[static_cast<std::size_t>(isl)];
        island_contacts_[static_cast<std::size_t>(it.first_contact + it.contact_count)] =
            static_cast<int>(ci);
        ++it.contact_count;
    }

    // Lesson 8.11. The joints, sorted into the same islands the same way.
    for (island& isl : islands_) { isl.joint_count = 0; }
    for (const joint_pair& p : joints_)
    {
        const int isl = edge_island(p.body_a, p.body_b);
        if (isl >= 0) { ++islands_[static_cast<std::size_t>(isl)].joint_count; }
    }
    running = 0;
    for (island& isl : islands_)
    {
        isl.first_joint = running;
        running += isl.joint_count;
        isl.joint_count = 0;
    }
    island_joints_.assign(static_cast<std::size_t>(running), 0);
    for (std::size_t ji = 0; ji < joints_.size(); ++ji)
    {
        const int isl = edge_island(joints_[ji].body_a, joints_[ji].body_b);
        if (isl < 0) { continue; }
        island& it = islands_[static_cast<std::size_t>(isl)];
        island_joints_[static_cast<std::size_t>(it.first_joint + it.joint_count)] =
            static_cast<int>(ji);
        ++it.joint_count;
    }

    // ---- 2. which islands are asleep, and wake the ones that are not --------
    //
    // BEFORE anything is prepared, because that is where the saving is:
    // deciding afterwards would mean preparing, warm-starting and iterating
    // every contact of a settled scene and then throwing the answer away.
    // Deciding here means a sleeping island costs one pass over its bodies.
    //
    // Note what this is NOT: it is not the sleep TEST. It only reads flags the
    // previous step set, and the test itself runs at the very end of this
    // function — see there for the reason, which is a number.
    wake_islands(bodies_, island_bodies_, islands_);

    // ---- 3. prepare, and warm start ONCE ------------------------------------
    batches_.assign(pairs_.size(), contact_batch{});
    joint_batches_.assign(joints_.size(), joint_batch{});
    pseudo_.assign(body_count, pseudo_velocity{});

    for (const island& isl : islands_)
    {
        if (isl.sleeping) { continue; }

        // Joints first, here as in the sweeps. The warm starts are all applied
        // before the first sweep either way, so the order in THIS loop changes
        // only which impulse is added to a velocity first — i.e. nothing but
        // the last bit of the sum.
        for (int k = 0; k < isl.joint_count; ++k)
        {
            const int ji = island_joints_[static_cast<std::size_t>(isl.first_joint + k)];
            const joint_pair& p = joints_[static_cast<std::size_t>(ji)];
            rigid_body& a = bodies_[p.body_a];
            rigid_body& b = bodies_[p.body_b];

            joint_batch& batch = joint_batches_[static_cast<std::size_t>(ji)];
            batch = prepare_joint(a, b, *p.joint_ptr, h, cfg.joints);
            if (cfg.joints.warm_start) { warm_start_joint(a, b, batch); }

            stats_.joint_linear_error =
                std::max(stats_.joint_linear_error, length(batch.point_error));
            for (int i = 0; i < 2; ++i)
            {
                stats_.joint_angular_error =
                    std::max(stats_.joint_angular_error, std::abs(batch.angular_error[i]));
            }
            for (int r = 0; r < batch.row_count; ++r)
            {
                const jacobian_row& row = batch.rows[r];
                const float violation = row.lower == 0.0f ? std::max(0.0f, -row.error)
                                        : row.role == static_cast<std::uint8_t>(row_role::motor)
                                            ? 0.0f
                                            : std::abs(row.error);
                const bool angular = length_squared(row.linear_a) == 0.0f
                                     && length_squared(row.linear_b) == 0.0f;
                if (angular) { stats_.joint_angular_error = std::max(stats_.joint_angular_error, violation); }
                else         { stats_.joint_linear_error = std::max(stats_.joint_linear_error, violation); }
            }
            ++stats_.solved_joints;
        }

        for (int k = 0; k < isl.contact_count; ++k)
        {
            const int ci = island_contacts_[static_cast<std::size_t>(isl.first_contact + k)];
            const contact_pair& p = pairs_[static_cast<std::size_t>(ci)];
            rigid_body& a = bodies_[p.body_a];
            rigid_body& b = bodies_[p.body_b];

            contact_batch& batch = batches_[static_cast<std::size_t>(ci)];
            batch = prepare_contacts(a, b, *p.manifold, p.material, cfg);
            prepare_bias(batch, h, cfg);

            // *** ONCE PER STEP, OUTSIDE THE ITERATION LOOP. *** 8.9's scar:
            // inside the loop it is applied once per iteration, and with
            // sixteen iterations a crate slid 810 mm down a slope it should
            // have gripped — which presented as friction failing, because the
            // spurious normal impulses had launched it off the surface.
            if (cfg.warm_start) { warm_start_contacts(a, b, batch); }

            stats_.points += batch.count;
            stats_.warm_points += p.manifold->warm_points;
            ++stats_.solved_manifolds;
        }
    }

    // ---- 4. the velocity iterations ------------------------------------------
    //
    // Gauss–Seidel: every sweep re-solves each constraint against the
    // velocities the others have since produced, so the corrections compose
    // instead of competing. Within an island the JOINTS go first and the
    // contacts second, which is Box2D's order and is argued rather than
    // measured: a contact is the constraint that must never be violated
    // visibly (a limb through a floor is worse than a limb a millimetre off
    // its socket), so it gets the last word in each sweep. 8.11 leaves the
    // measurement as an exercise, and says so.
    //
    // The order within the contacts is the order the caller added the
    // manifolds, grouped by island — and it MATTERS, because a Gauss–Seidel
    // sweep propagates information in the direction it walks. 8.10 §5
    // measures it and finds LESS than the folklore claims: bottom-up beats
    // top-down by 1.08x in residual on a ten-crate tower, which at §3's
    // measured contraction is worth 0.12 of an iteration. This engine
    // therefore does not sort, and says so rather than implying a decision.
    const bool joint_bias = cfg.correction == position_correction::baumgarte;
    for (int iteration = 0; iteration < cfg.velocity_iterations; ++iteration)
    {
        stats_.max_residual = 0.0f;
        stats_.joint_residual = 0.0f;
        stats_.normal_impulse = 0.0f;
        stats_.friction_impulse = 0.0f;

        for (const island& isl : islands_)
        {
            if (isl.sleeping) { continue; }

            for (int k = 0; k < isl.joint_count; ++k)
            {
                const int ji = island_joints_[static_cast<std::size_t>(isl.first_joint + k)];
                const joint_pair& p = joints_[static_cast<std::size_t>(ji)];
                const joint_report r = solve_joint(bodies_[p.body_a], bodies_[p.body_b],
                                                   joint_batches_[static_cast<std::size_t>(ji)],
                                                   joint_bias);
                stats_.joint_residual = std::max(stats_.joint_residual, r.max_residual);
            }

            for (int k = 0; k < isl.contact_count; ++k)
            {
                const int ci = island_contacts_[static_cast<std::size_t>(isl.first_contact + k)];
                const contact_pair& p = pairs_[static_cast<std::size_t>(ci)];
                const solve_report r = solve_contacts(bodies_[p.body_a], bodies_[p.body_b],
                                                      batches_[static_cast<std::size_t>(ci)], cfg);
                stats_.max_residual = std::max(stats_.max_residual, r.max_residual);
                stats_.normal_impulse += r.normal_impulse;
                stats_.friction_impulse += r.friction_impulse;
            }
        }
        ++stats_.velocity_iterations;
    }

    // ---- 5. the position iterations, in the shadow ----------------------------
    //
    // AFTER every velocity iteration and not interleaved with them. The
    // position pass reads separations that the velocity pass is about to make
    // stale, so interleaving spends corrections on a geometry that is still
    // changing; §8 measures 2.3x the residual for no improvement in depth.
    if (cfg.correction == position_correction::split_impulse)
    {
        for (int iteration = 0; iteration < cfg.position_iterations; ++iteration)
        {
            stats_.max_penetration = 0.0f;
            for (const island& isl : islands_)
            {
                if (isl.sleeping) { continue; }

                for (int k = 0; k < isl.joint_count; ++k)
                {
                    const int ji = island_joints_[static_cast<std::size_t>(isl.first_joint + k)];
                    const joint_pair& p = joints_[static_cast<std::size_t>(ji)];
                    (void)solve_joint_positions(joint_batches_[static_cast<std::size_t>(ji)],
                                                pseudo_[p.body_a], pseudo_[p.body_b]);
                }

                for (int k = 0; k < isl.contact_count; ++k)
                {
                    const int ci =
                        island_contacts_[static_cast<std::size_t>(isl.first_contact + k)];
                    const contact_pair& p = pairs_[static_cast<std::size_t>(ci)];
                    const float depth = solve_positions(batches_[static_cast<std::size_t>(ci)],
                                                        pseudo_[p.body_a], pseudo_[p.body_b], cfg);
                    stats_.max_penetration = std::max(stats_.max_penetration, depth);
                }
            }
            ++stats_.position_iterations;
        }

        for (const island& isl : islands_)
        {
            if (isl.sleeping) { continue; }
            for (int k = 0; k < isl.body_count; ++k)
            {
                const std::uint32_t bi =
                    island_bodies_[static_cast<std::size_t>(isl.first_body + k)];
                apply_pseudo_velocity(bodies_[bi], pseudo_[bi], h);
            }
        }
    }
    else
    {
        for (const island& isl : islands_)
        {
            if (isl.sleeping) { continue; }
            for (int k = 0; k < isl.contact_count; ++k)
            {
                const int ci = island_contacts_[static_cast<std::size_t>(isl.first_contact + k)];
                const contact_batch& batch = batches_[static_cast<std::size_t>(ci)];
                for (int pt = 0; pt < batch.count; ++pt)
                {
                    stats_.max_penetration =
                        std::max(stats_.max_penetration, -batch.points[pt].separation);
                }
            }
        }
    }

    // ---- 6. hand the impulses to next frame ------------------------------------
    //
    // The write-back IS the warm start. Everything this solve learned about how
    // hard each point had to push lands in the manifold, 8.7's `carry_impulses`
    // matches it onto next frame's points by feature id, and next frame's
    // `prepare_contacts` reads it back as an initial guess. Three lessons of
    // machinery, and this is the line that closes the loop.
    //
    // A joint needs none of the matching: it is the same joint next step, by
    // construction, so its impulses go straight back into it.
    for (const island& isl : islands_)
    {
        if (isl.sleeping) { continue; }
        for (int k = 0; k < isl.joint_count; ++k)
        {
            const int ji = island_joints_[static_cast<std::size_t>(isl.first_joint + k)];
            write_back(joint_batches_[static_cast<std::size_t>(ji)],
                       *joints_[static_cast<std::size_t>(ji)].joint_ptr);
        }
        for (int k = 0; k < isl.contact_count; ++k)
        {
            const int ci = island_contacts_[static_cast<std::size_t>(isl.first_contact + k)];
            const contact_pair& p = pairs_[static_cast<std::size_t>(ci)];
            write_back(batches_[static_cast<std::size_t>(ci)], *p.manifold);
        }
    }

    // ---- 7. and only NOW ask whether anything can sleep -----------------------
    //
    // *** THE SLEEP TEST CANNOT RUN BEFORE THE SOLVE, AND THE REASON IS ONE
    // *** NUMBER: g*h.
    //
    // A semi-implicit step gives every dynamic body `gravity * h` of downward
    // velocity before the solver looks — 16.35 cm/s at 60 Hz — so a crate that
    // has been motionless on a floor for a minute is travelling at 0.1635 m/s
    // when a pre-solve test reads it. That is three times
    // `sleep_config::linear_threshold` and above any threshold a designer would
    // accept, so a scene with the test in the wrong place NEVER SLEEPS, at any
    // setting, and looks exactly like a scene whose thresholds are too tight.
    // 8.10 §10 reads a yard that has been motionless for six seconds and
    // finds 0 of 100 crates quiet before the solve and 96 of 100 after it,
    // with the fastest body at 0.1836 m/s before and 0.0629 after.
    stats_.sleeping_islands = update_sleep(bodies_, island_bodies_, islands_, h, sleep);
    for (const island& isl : islands_)
    {
        if (isl.sleeping) { stats_.sleeping_bodies += isl.body_count; }
    }

    return stats_;
}

std::span<const island> constraint_solver::islands() const
{
    return {islands_.data(), islands_.size()};
}

std::span<const std::uint32_t> constraint_solver::island_bodies() const
{
    return {island_bodies_.data(), island_bodies_.size()};
}

std::span<const int> constraint_solver::island_of() const
{
    return {island_of_.data(), island_of_.size()};
}

const solver_stats& constraint_solver::stats() const { return stats_; }

std::span<const joint_batch> constraint_solver::joint_batches() const
{
    return {joint_batches_.data(), joint_batches_.size()};
}

void constraint_solver::clear()
{
    bodies_ = {};
    pairs_ = {};
    batches_ = {};
    joints_ = {};
    joint_batches_ = {};
    pseudo_ = {};
    island_of_ = {};
    islands_ = {};
    island_bodies_ = {};
    island_contacts_ = {};
    island_joints_ = {};
    stats_ = solver_stats{};
}

// ---------------------------------------------------------------------------
// Closed forms
// ---------------------------------------------------------------------------

float bounce_height(float drop_height, float restitution, int bounces)
{
    return drop_height * std::pow(restitution, 2.0f * static_cast<float>(bounces));
}

float terminal_bounce_speed(float restitution, float gravity_step)
{
    if (restitution >= 1.0f) { return std::numeric_limits<float>::infinity(); }
    return restitution * gravity_step / (1.0f - restitution);
}

float critical_slope_degrees(float friction)
{
    return std::atan(friction) * k_rad_to_deg;
}

float rolling_speed_fraction(float inertia_coefficient)
{
    return 1.0f / (1.0f + inertia_coefficient);
}

float rolling_time(float initial_speed, float friction, float gravity, float inertia_coefficient)
{
    if (friction <= 0.0f || inertia_coefficient <= 0.0f)
    {
        return std::numeric_limits<float>::infinity();
    }
    // Sliding decelerates the centre at `mu*g` and spins the body up at
    // `mu*g/(c*r)` in contact-point terms; the gap `v - omega*r` therefore closes
    // at `mu*g*(1 + 1/c)`, and the time is the initial gap over that rate. The
    // radius cancels, which is why it is not a parameter.
    return initial_speed / (friction * gravity * (1.0f + 1.0f / inertia_coefficient));
}

float penalty_stiffness(float mass, float gravity, float penetration)
{
    return penetration > 0.0f ? mass * gravity / penetration
                              : std::numeric_limits<float>::infinity();
}

float penalty_omega(float gravity, float penetration)
{
    return penetration > 0.0f ? std::sqrt(gravity / penetration)
                              : std::numeric_limits<float>::infinity();
}

float penalty_impact_stiffness(float mass, float speed, float penetration)
{
    // Energy in, energy stored: (1/2)m*v^2 = (1/2)k*x^2. The mass does NOT
    // cancel here, which is the difference between this and the static formula
    // — and it is why a penalty floor that holds a pebble beautifully swallows
    // a crate.
    return penetration > 0.0f ? mass * speed * speed / (penetration * penetration)
                              : std::numeric_limits<float>::infinity();
}

float arrival_depth(float approach_speed, float h)
{
    // One step of travel, and the sign is taken off deliberately: the caller
    // has an approach speed that may be signed either way depending on which
    // body the normal points from, and the answer is a depth.
    return std::abs(approach_speed) * h;
}

float baumgarte_time_constant(float beta, float h)
{
    // `(1 - beta)^n` is `exp(n * ln(1 - beta))`, so the depth falls by `1/e`
    // after `-1/ln(1 - beta)` steps, which is this many seconds.
    if (!(beta > 0.0f)) { return std::numeric_limits<float>::infinity(); }
    if (beta >= 1.0f)   { return 0.0f; }
    return -h / std::log(1.0f - beta);
}

} // namespace engine::phys
