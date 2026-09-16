// engine/src/phys/rigid_body.cpp — mass, forces, and the walk over the table.
//
// Lesson 8.2. Nothing in this file is a template, because nothing in it needs to
// be: 8.1's general stepper is a template so that a one-line acceleration
// function can inline into the step, and a body's acceleration is not a function
// at all — it is a field divided by a mass. The whole per-body update is eleven
// multiplies and two adds, and `step` below is the only loop-resident code here.
//
// The FORCE ACCUMULATOR is the design, and it shows up as an absence: there is
// no list of force generators, no visitor, no registry, no interface to
// implement. Anything in the program that wants to push a body calls `add_force`
// and the vector adds. That works because F = m*a is linear in F, and it is why
// gravity is handled below by the same two lines a thruster would use rather
// than by a special case in the integrator.

#include <engine/phys/rigid_body.hpp>

#include <engine/core/log.hpp>

#include <cmath>

namespace engine::phys
{

const char* name_of(body_kind kind)
{
    switch (kind)
    {
    case body_kind::dynamic:   return "dynamic";
    case body_kind::kinematic: return "kinematic";
    case body_kind::fixed:     return "fixed";
    }
    // Not an assert, for 5.3's reason: a naming function feeds logs and debug
    // UI, and a log line is the last thing that should be able to stop a
    // program.
    return "?";
}

// ---------------------------------------------------------------------------
// Mass
// ---------------------------------------------------------------------------

float mass_of(const rigid_body& b)
{
    // HUGE_VALF rather than a large float, because the answer really is
    // infinity and a caller comparing against it deserves the comparison to
    // work. Note what this means for arithmetic: `mass_of(floor) -
    // mass_of(wall)` is `inf - inf`, which is NaN, and that is precisely the
    // trap the header's `inv_mass` note is about. Subtracting two inverse
    // masses gives 0, which is the right answer.
    return (b.inv_mass > 0.0f) ? (1.0f / b.inv_mass) : HUGE_VALF;
}

bool set_mass(rigid_body& b, float kilograms)
{
    if (!(kilograms > 0.0f))
    {
        // Catches 0, negatives AND NaN — `!(x > 0)` is true for NaN where
        // `x <= 0` is false, which is the one place in this file where the
        // negated comparison is load-bearing rather than a style choice.
        // `log_core`, and that is a decision rather than a default. 5.3's test
        // for a new category is "would somebody want to turn exactly this off",
        // and 7.8 — which added the last one — predicted in log.hpp that the
        // subsystems arriving in Modules 8 and 9 would not qualify. This is the
        // first test of that prediction and it holds: the only thing physics has
        // to say here is that a caller passed something that is not a mass,
        // which is a programmer error, and a programmer error is the last line
        // anybody wants a switch for.
        ENGINE_LOG_WARN(log_core, "set_mass: %f kg is not a mass; body unchanged",
                        static_cast<double>(kilograms));
        return false;
    }

    // An infinite mass is a legal way to ask for immovable, and `1/inf` is
    // exactly 0, so no branch is needed for it.
    b.inv_mass = 1.0f / kilograms;
    return true;
}

// ---------------------------------------------------------------------------
// Applying things
// ---------------------------------------------------------------------------

void add_force(rigid_body& b, vec3 newtons)
{
    b.force += newtons;
}

void add_impulse(rigid_body& b, vec3 newton_seconds)
{
    // dv = J/m, and no `h` anywhere. That absence is the whole point — see the
    // header's note on the jump that is twice as high at 30 Hz.
    //
    // An immovable body has inv_mass 0, so this is a multiply by zero and the
    // impulse is absorbed. That is the physically correct answer (the floor
    // really does not move when you hit it) and it needs no test.
    b.state.velocity += newton_seconds * b.inv_mass;
}

void add_acceleration(rigid_body& b, vec3 metres_per_second_squared)
{
    if (b.inv_mass <= 0.0f)
    {
        // An immovable body cannot be accelerated by anything, and `m*a` with
        // an infinite mass is an infinite force — which would poison the
        // accumulator with an infinity that survives the later multiply by zero
        // as a NaN. Handled here rather than trusted to the arithmetic.
        return;
    }

    // m*a, so that a field enters the SAME accumulator a push does and the two
    // sum correctly. The round trip through the mass is what makes this line
    // right rather than pointless: writing the velocity directly would move
    // kinematic bodies and would not compose with anything.
    b.force += metres_per_second_squared * (1.0f / b.inv_mass);
}

void clear_force(rigid_body& b)
{
    b.force = vec3{};
}

// ---------------------------------------------------------------------------
// Terminal speeds
// ---------------------------------------------------------------------------

float terminal_speed_damped(float g, float k)
{
    if (!(k > 0.0f)) { return HUGE_VALF; }   // no damping, no terminal speed
    return g / k;
}

float terminal_speed_dragged(float g, float mass, float b_coefficient)
{
    if (!(b_coefficient > 0.0f)) { return HUGE_VALF; }
    return (mass * g) / b_coefficient;
}

// ---------------------------------------------------------------------------
// Units
// ---------------------------------------------------------------------------

float free_fall_time(float distance, float g)
{
    if (!(g > 0.0f) || distance <= 0.0f) { return 0.0f; }
    return std::sqrt(2.0f * distance / g);
}

float time_scale_for_length_scale(float length_scale)
{
    if (!(length_scale > 0.0f)) { return 0.0f; }
    return std::sqrt(length_scale);
}

// ---------------------------------------------------------------------------
// Frames
// ---------------------------------------------------------------------------

frame_report inspect_frame(const mat4& world_from_parent, vec3 gravity)
{
    frame_report out;

    const mat3 linear = linear_of(world_from_parent);

    // A gravity vector is a DIRECTION, not a position — Lesson 2.7's `w`, and
    // the reason this uses the 3x3 linear part rather than the full 4x4. A
    // translation must not change an acceleration; if it did, moving a platform
    // sideways would make everything on it fall harder.
    out.gravity_in_world = linear * gravity;

    const float g_len = length(gravity);
    const float w_len = length(out.gravity_in_world);
    out.gain = (g_len > 0.0f) ? (w_len / g_len) : 1.0f;

    if (g_len > 0.0f && w_len > 0.0f)
    {
        // The angle between where gravity was aimed and where it landed. Clamped
        // before the acos because a dot of two normalised floats can land a few
        // ulps outside [-1, 1] and `std::acos(1.0000001f)` is NaN — the same
        // guard `quat_slerp` needs for the same reason (7.5 §4).
        float c = dot(gravity, out.gravity_in_world) / (g_len * w_len);
        c = (c > 1.0f) ? 1.0f : ((c < -1.0f) ? -1.0f : c);
        out.tilt_degrees = std::acos(c) * (180.0f / 3.14159265358979323846f);
    }

    // The scale and the shear come from 7.5's decomposition, unchanged and
    // uncalled-for until now: `out_of_square` is the worst |cosine| between two
    // recovered axes, which is exactly "does this frame preserve angles".
    const transform_extraction ex = transform_from_affine(world_from_parent);
    out.scale = ex.value.scale;
    out.out_of_square = ex.out_of_square;

    const float sx = std::fabs(out.scale.x);
    const float sy = std::fabs(out.scale.y);
    const float sz = std::fabs(out.scale.z);
    const float biggest = (sx > sy) ? ((sx > sz) ? sx : sz) : ((sy > sz) ? sy : sz);
    const float smallest = (sx < sy) ? ((sx < sz) ? sx : sz) : ((sy < sz) ? sy : sz);

    // A relative test, not an absolute one: a frame scaled uniformly by 1000 is
    // still uniform, and `biggest - smallest` would call it wildly non-uniform.
    out.uniform = (biggest <= 0.0f) || ((biggest - smallest) / biggest <= 1e-4f);

    // "Inertial" here means the narrow thing physics needs: a frame that is a
    // rotation and a translation, so that it preserves lengths and angles and a
    // body integrated in it feels exactly what a body integrated in world space
    // feels. `gain` within a tenth of a percent of 1 covers the uniform-scale
    // case; `out_of_square` covers shear; `uniform` covers the rest.
    //
    // A ROTATING frame passes all three tests and is still not inertial, and
    // this function cannot see that, because a matrix has no time derivative.
    // §8.5 measures what that case costs and the header says what it is: the
    // fictitious forces are real and nothing here supplies them.
    out.inertial = out.uniform
                && std::fabs(out.gain - 1.0f) <= 1e-3f
                && out.out_of_square <= k_transform_square_tolerance;

    return out;
}

transform place_in_parent(const rigid_body& b, const mat4& world_from_parent,
                          const transform& authored)
{
    // The inverse of a general affine matrix, which this genuinely needs: the
    // parent may be scaled, and `rigid_inverse` (2.8) assumes it is not.
    //
    // Route: decompose to a `transform`, then use `local_from_parent`, which
    // 7.6 wrote and which inverts by construction rather than by cofactors —
    // transpose the rotation, reciprocate the scale, negate the translation.
    // Forty operations saved is not the reason; the reason is that a
    // decomposition REPORTS what it could not represent, and a determinant does
    // not.
    const transform_extraction ex = transform_from_affine(world_from_parent);
    const mat4 parent_from_world = local_from_parent(ex.value);

    transform out = authored;
    // A position, so the full affine matrix and the translation with it — the
    // opposite of `inspect_frame` above, and the same `w` deciding both.
    // `point()` and not `to_vec4(p, 1)` — 2.7's rule, and it is exactly the
    // distinction this function is about: `inspect_frame` above uses the linear
    // part because an acceleration is a DIRECTION, and this line uses the full
    // matrix because a position is a POINT. Same matrix, one `w`, two meanings.
    out.position = xyz(parent_from_world * point(b.state.position));
    return out;
}

// ---------------------------------------------------------------------------
// The world
// ---------------------------------------------------------------------------

body_id body_world::add(const rigid_body& body)
{
    return bodies_.insert(body);
}

bool body_world::remove(body_id id)
{
    return bodies_.remove(id);
}

rigid_body* body_world::get(body_id id) { return bodies_.get(id); }
const rigid_body* body_world::get(body_id id) const { return bodies_.get(id); }

std::span<rigid_body> body_world::bodies() { return bodies_.items(); }
std::span<const rigid_body> body_world::bodies() const { return bodies_.items(); }

std::size_t body_world::size() const { return bodies_.size(); }
void body_world::clear() { bodies_.clear(); }

void body_world::set_gravity(vec3 g) { gravity_ = g; }
vec3 body_world::gravity() const { return gravity_; }

void body_world::set_integrator(integrator rule) { rule_ = rule; }
integrator body_world::integrator_rule() const { return rule_; }

const step_report& body_world::report() const { return report_; }

const step_report& body_world::step(float h)
{
    report_ = step_report{};

    for (rigid_body& b : bodies_.items())
    {
        ++report_.bodies;

        switch (b.kind)
        {
        case body_kind::fixed:
            // Skipped entirely. Not "integrated with zero acceleration" — a
            // fixed body does not even pay the loads, and 8.6's broadphase
            // relies on these never moving to keep a structure it never
            // rebuilds. Its force accumulator is still cleared, so that a
            // system pushing on the floor does not leak a growing vector.
            ++report_.fixed;
            b.force = vec3{};
            continue;

        case body_kind::kinematic:
        {
            // Travels at whatever velocity gameplay set, and nothing else
            // touches it: no gravity, no forces, no damping. The position
            // update is the one line of the integrator that applies, written
            // out rather than routed through `integrate` so that it is obvious
            // that the velocity is NOT being changed.
            ++report_.kinematic;
            b.state.position += b.state.velocity * h;
            b.force = vec3{};
            break;
        }

        case body_kind::dynamic:
        {
            ++report_.dynamic;

            // (1) and (2) TOGETHER, and the order of the two terms is a
            // decision this lesson makes twice and gets wrong the first time.
            //
            // The obvious arrangement puts gravity into the accumulator as a
            // force of `m*g`, alongside everything else, and divides the sum by
            // the mass at the end — which is the honest statement of the
            // physics (weight IS a force) and reads beautifully. It costs a
            // FLOAT DIVIDE PER BODY PER STEP, because the weight needs `m` and
            // the body stores `1/m`, and §10 measures that at 15% of the whole
            // update — more than 8.1's entire integrator step.
            //
            // So gravity is added AFTER the division, as the acceleration it
            // already is. The mass was always going to cancel; this line simply
            // declines to compute it and then undo it. Two things survive
            // unchanged: `gravity_scale` still works, because it multiplies an
            // acceleration exactly as happily as it multiplied a force, and the
            // result is now exact by construction rather than by luck — §3
            // found that 16% of masses do not survive the round trip
            // `(g/inv_mass)*inv_mass` bit for bit.
            //
            // The one thing given up is real and is named in §9: `force` no
            // longer contains the body's weight, so a debug overlay drawing
            // force arrows draws every push EXCEPT the one that is always
            // there. That is a tooling problem with a one-line answer
            // (`mass_of(b) * world.gravity()`), and it is the right side of the
            // trade. Box2D makes the same one, in the same place.
            const vec3 acceleration = (b.inv_mass > 0.0f)
                                    ? (b.force * b.inv_mass
                                       + gravity_ * b.gravity_scale)
                                    : vec3{};

            // (3) Advance, by 8.1's rule and 8.1's code. The constant-
            // acceleration overload, because the force was collected before the
            // step and is held fixed across it — which is what "the force
            // applied during this step" means.
            integrate(b.state, acceleration, h, rule_);

            // ...then damping, exactly, as `v *= exp(-k*h)`. AFTER the step and
            // not folded into the acceleration, because `a = -k*v` is not a
            // force derived from a potential and feeding it through a symplectic
            // rule gets you plain explicit Euler's stability limit on it
            // (8.1 §8). A body with `damping` 0 pays a branch and no exp.
            if (b.damping > 0.0f) { apply_drag(b.state, b.damping, h); }

            // (4) ...and only now is the accumulator cleared, so that a debug
            // overlay, an assertion or 8.10's solver can still read what was
            // applied during the step that just ran.
            b.force = vec3{};

            // A `sqrt` PER BODY, and it was measured before being kept. The
            // obvious saving is to compare squared speeds and take one root at
            // the end; §10.2 timed both and found 1.036, 0.968 and 0.969 over
            // three runs — a tie, and once a loss. `sqrtss` is a pipelined
            // single instruction on every target this engine builds for, and
            // the version that reads as what it means wins by default when the
            // measurement is a wash.
            const float speed = length(b.state.velocity);
            if (speed > report_.max_speed) { report_.max_speed = speed; }

            // Momentum excludes immovable bodies: theirs is either zero or
            // infinite and neither belongs in a sum that 8.9 will compare
            // across a collision.
            report_.momentum += b.state.velocity * (1.0f / b.inv_mass);
            break;
        }
        }
    }

    // Computed from the final speed rather than accumulated per body, because
    // the two agree for the semi-implicit rule — its position line moves at
    // exactly the end-of-step velocity — and this way it is one multiply rather
    // than a length per body.
    report_.max_travel = report_.max_speed * h;
    return report_;
}

// ---------------------------------------------------------------------------
// Conveniences
// ---------------------------------------------------------------------------

rigid_body make_dynamic(vec3 position, float mass)
{
    rigid_body b;
    b.state.position = position;
    b.kind = body_kind::dynamic;
    set_mass(b, mass);
    return b;
}

rigid_body make_fixed(vec3 position)
{
    rigid_body b;
    b.state.position = position;
    b.inv_mass = 0.0f;
    b.kind = body_kind::fixed;
    return b;
}

rigid_body make_kinematic(vec3 position, vec3 velocity)
{
    rigid_body b;
    b.state.position = position;
    b.state.velocity = velocity;
    b.inv_mass = 0.0f;
    b.kind = body_kind::kinematic;
    return b;
}

} // namespace engine::phys
