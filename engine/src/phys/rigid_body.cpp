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

const char* name_of(gyroscopic_mode mode)
{
    switch (mode)
    {
    case gyroscopic_mode::off:           return "off";
    case gyroscopic_mode::explicit_term: return "explicit";
    case gyroscopic_mode::implicit_term: return "implicit";
    case gyroscopic_mode::momentum:      return "momentum";
    }
    return "?";
}

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

// ---- Torque (Lesson 8.3) ---------------------------------------------------

void add_torque(rigid_body& b, vec3 newton_metres)
{
    b.torque += newton_metres;
}

void add_force_at(rigid_body& b, vec3 newtons, vec3 world_point)
{
    // Both accumulators, one call. The lever arm is measured from the centre of
    // mass, which for this engine is the body's position — see the header note
    // on `add_force_at` for why there is no separate centre-of-mass offset.
    b.force += newtons;
    b.torque += cross(world_point - b.state.position, newtons);
}

void add_impulse_at(rigid_body& b, vec3 newton_seconds, vec3 world_point)
{
    b.state.velocity += newton_seconds * b.inv_mass;

    const vec3 arm = world_point - b.state.position;
    b.angular_velocity += world_inv_inertia(b) * cross(arm, newton_seconds);
}

void add_angular_impulse(rigid_body& b, vec3 newton_metre_seconds)
{
    b.angular_velocity += world_inv_inertia(b) * newton_metre_seconds;
}

void clear_torque(rigid_body& b)
{
    b.torque = vec3{};
}

// ---- Inertia (Lesson 8.3) --------------------------------------------------

bool set_inertia(rigid_body& b, const mat3& body_inertia)
{
    const inertia_report r = inspect_inertia(body_inertia);
    if (!r.usable)
    {
        // The same shape of refusal as `set_mass`, and it goes to `log_core` for
        // the same reason: this is a programmer error, not a condition anybody
        // would want a per-subsystem switch for. 7.8's rule for a log category
        // is "would somebody want to turn exactly this off", and nobody wants to
        // turn off the line that tells them their crate cannot exist.
        ENGINE_LOG_WARN(log_core,
                        "set_inertia: rejected tensor (diag %f %f %f, asym %f, "
                        "positive %d, triangle %d); body unchanged",
                        static_cast<double>(r.diagonal.x),
                        static_cast<double>(r.diagonal.y),
                        static_cast<double>(r.diagonal.z),
                        static_cast<double>(r.asymmetry),
                        r.positive ? 1 : 0, r.triangle ? 1 : 0);
        return false;
    }

    // BOTH, always, and this is the only function that writes either. See the
    // note on `rigid_body::inertia_local`.
    b.inv_inertia_local = inverse_inertia(body_inertia);
    b.inertia_local = body_inertia;
    return true;
}

mat3 inertia_of(const rigid_body& b)
{
    return b.inertia_local;
}

mat3 world_inv_inertia(const rigid_body& b)
{
    return world_inverse_inertia(b.orientation, b.inv_inertia_local);
}

vec3 gyroscopic_step(const rigid_body& b, float h, gyroscopic_mode mode)
{
    // `momentum` is not a correction applied to an angular velocity — it is a
    // different choice of state variable, and it needs the END-OF-STEP
    // orientation, which this function does not have. `step` implements it
    // directly; here it is a no-op so that a caller cycling through the modes
    // does not get a wrong answer instead of an unavailable one.
    if (mode == gyroscopic_mode::off || mode == gyroscopic_mode::momentum)
    {
        return b.angular_velocity;
    }

    const mat3 body_to_world = mat3_from_quat(b.orientation);
    const mat3& i_body = b.inertia_local;

    if (mode == gyroscopic_mode::explicit_term)
    {
        // The obvious one: evaluate the term at the state you already have and
        // add it. Done in world space, where `omega` already lives.
        const mat3 i_world = body_to_world * i_body * transpose(body_to_world);
        const mat3 inv_i_world =
            body_to_world * b.inv_inertia_local * transpose(body_to_world);

        const vec3 term = cross(b.angular_velocity, i_world * b.angular_velocity);
        return b.angular_velocity - (inv_i_world * term) * h;
    }

    // ---- the implicit one, in the BODY frame ------------------------------
    //
    // Work in body axes, because that is the one frame in which `I` is a
    // constant and can be differentiated through without a product rule.
    const vec3 w0 = transpose(body_to_world) * b.angular_velocity;
    const vec3 l0 = i_body * w0;

    // Write the step as a root-finding problem in the UNKNOWN end-of-step
    // angular velocity `w`:
    //
    //     f(w) = I*(w - w0) + h * (w x (I w)) = 0
    //
    // At `w = w0` the first term vanishes and the residual is just the term
    // itself, scaled by the step. That residual is already O(h), which is why
    // ONE Newton iteration is enough — a second would correct an O(h^2) error
    // in a scheme that is O(h) anyway.
    const vec3 residual = cross(w0, l0) * h;

    // The derivative of `w x (I w)` with respect to `w`, applied to a
    // perturbation `d`, is `d x (I w) + w x (I d)` — which as matrices is
    // `[w]x I - [I w]x`. Both cross-product matrices, and the reason `skew`
    // exists in mat3.hpp.
    const mat3 jacobian = i_body + (skew(w0) * i_body - skew(l0)) * h;

    const vec3 correction = inverse(jacobian) * residual;
    return body_to_world * (w0 - correction);
}

vec3 angular_momentum(const rigid_body& b)
{
    // `R * (I_body * (R^T * omega))` rather than `(R I_body R^T) * omega`.
    //
    // The two are the same number and not the same amount of work: the sandwich
    // builds a whole matrix — two 3x3 products, 90 flops — and then throws it
    // away after one use, where this route is three matrix-VECTOR products at 15
    // flops each. Build the operator when you will apply it many times; apply it
    // directly when you will not.
    const mat3 body_to_world = mat3_from_quat(b.orientation);
    const vec3 omega_body = transpose(body_to_world) * b.angular_velocity;
    return body_to_world * (b.inertia_local * omega_body);
}

float kinetic_energy(const rigid_body& b)
{
    const float linear = (b.inv_mass > 0.0f)
                       ? (0.5f * length_squared(b.state.velocity) / b.inv_mass)
                       : 0.0f;

    // The rotational half is a quadratic FORM — omega . (I omega) — not a
    // product of two scalars, which is the whole difference between a mass and
    // a tensor written out in one line of arithmetic.
    const vec3 l = angular_momentum(b);
    return linear + 0.5f * dot(b.angular_velocity, l);
}

vec3 point_velocity(const rigid_body& b, vec3 world_point)
{
    return b.state.velocity + cross(b.angular_velocity, world_point - b.state.position);
}

vec3 world_point_of(const rigid_body& b, vec3 local_point)
{
    return b.state.position + rotate(b.orientation, local_point);
}

// ---------------------------------------------------------------------------
// Sleeping — Lesson 8.10
// ---------------------------------------------------------------------------

void wake(rigid_body& b)
{
    b.sleeping = false;

    // Both lines, always. Clearing the flag without resetting the clock is the
    // bug that makes a shoved crate travel ten centimetres and stop dead: the
    // body wakes with four tenths of a second already banked and falls asleep
    // again one tenth later, long before it has finished doing what it was
    // pushed to do. 8.10 §10 measures that at 0.104 m of travel against 1.181.
    b.sleep_timer = 0.0f;
}

bool is_sleep_candidate(const rigid_body& b, float linear_threshold, float angular_threshold)
{
    // A fixed body is quiet by definition, and saying so here rather than at
    // every call site is what lets `update_sleep` ask the question of a whole
    // island without a special case for the floor in it.
    if (b.kind == body_kind::fixed) { return true; }

    // A kinematic body NEVER sleeps, whatever its velocity. A stopped lift is
    // about to move — that is what kinematic means, that gameplay owns the
    // velocity — and a lift that slept would stop carrying the crates standing
    // on it, because their island would sleep with it.
    if (b.kind == body_kind::kinematic) { return false; }

    // Squared comparisons: two lengths per body per step, over a scene of
    // thousands, to answer a question whose answer is a bool. The thresholds
    // are squared here rather than by the caller so that the units of the
    // arguments are the units a designer types.
    return length_squared(b.state.velocity) < linear_threshold * linear_threshold
           && length_squared(b.angular_velocity) < angular_threshold * angular_threshold;
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

void body_world::set_spin_rule(spin_rule rule) { spin_ = rule; }
spin_rule body_world::spin() const { return spin_; }

const step_report& body_world::report() const { return report_; }

const step_report& body_world::step(float h)
{
    // Lesson 8.10 split this function in two, and `step` is now literally the
    // composition. Keeping it is not politeness to old callers: most of the
    // engine has no contacts to insert, and a caller that writes the two lines
    // below by hand has written a bug waiting for the day somebody reorders
    // them. The halves are public so that a contact solve can go BETWEEN them,
    // and for no other reason — see `integrate_velocities`.
    integrate_velocities(h);
    return integrate_positions(h);
}

void body_world::integrate_velocities(float h)
{
    report_ = step_report{};

    for (rigid_body& b : bodies_.items())
    {
        ++report_.bodies;

        // Lesson 8.3's instrument, read BEFORE the step touches anything: how
        // far from the unit sphere did this body's quaternion ARRIVE? The step
        // renormalises on the way out, so anything found here was written by
        // somebody else — an animation blend, a lerp, a network packet, a
        // hand-authored quaternion. See `step_report::max_unit_error`.
        const float unit_error = std::fabs(length(b.orientation) - 1.0f);
        if (unit_error > report_.max_unit_error) { report_.max_unit_error = unit_error; }

        switch (b.kind)
        {
        case body_kind::fixed:
            // Skipped entirely. Not "integrated with zero acceleration" — a
            // fixed body does not even pay the loads, and 8.8's broadphase
            // relies on these never moving to keep a structure it never
            // rebuilds. Its force accumulator is cleared by the other half, so
            // that a system pushing on the floor does not leak a growing
            // vector.
            ++report_.fixed;
            continue;

        case body_kind::kinematic:
            // Travels at whatever velocity gameplay set, and nothing else
            // touches it: no gravity, no forces, no damping. There is
            // therefore nothing at all for the VELOCITY half to do to it, and
            // saying so in one line is clearer than the old version's
            // carefully written-out position update was.
            ++report_.kinematic;
            continue;

        case body_kind::dynamic:
        {
            ++report_.dynamic;

            // ---- Lesson 8.10: asleep is OUT OF THE SIMULATION -------------
            //
            // Not "integrated with gravity disabled" and not "integrated and
            // then snapped back". A sleeping body is skipped by both halves and
            // its contacts are skipped by `contact_solver`, which is the whole
            // of the saving: a settled scene costs its broadphase and nothing
            // else. `update_sleep` zeroed the velocities on the way in, so the
            // momentum this body contributes to the report is exactly zero and
            // omitting it changes no number.
            //
            // The accumulators are still cleared by the position half. A
            // sleeping body that a gameplay system keeps pushing on must WAKE,
            // and a force silently accumulating on it for ten seconds would
            // make that wake-up a launch.
            if (b.sleeping)
            {
                ++report_.sleeping;
                continue;
            }

            // (1) and (2) TOGETHER, and the order of the two terms is a
            // decision this lesson makes twice and gets wrong the first time.
            //
            // The obvious arrangement puts gravity into the accumulator as a
            // force of `m*g`, alongside everything else, and divides the sum by
            // the mass at the end — which is the honest statement of the
            // physics (weight IS a force) and reads beautifully. It costs a
            // FLOAT DIVIDE PER BODY PER STEP, because the weight needs `m` and
            // the body stores `1/m`, and 8.2 §10 measures that at 15% of the
            // whole update — more than 8.1's entire integrator step.
            //
            // So gravity is added AFTER the division, as the acceleration it
            // already is. The mass was always going to cancel; this line simply
            // declines to compute it and then undo it. Two things survive
            // unchanged: `gravity_scale` still works, because it multiplies an
            // acceleration exactly as happily as it multiplied a force, and the
            // result is now exact by construction rather than by luck — 8.2 §3
            // found that 16% of masses do not survive the round trip
            // `(g/inv_mass)*inv_mass` bit for bit.
            //
            // The one thing given up is real and is named in 8.2 §9: `force` no
            // longer contains the body's weight, so a debug overlay drawing
            // force arrows draws every push EXCEPT the one that is always
            // there. That is a tooling problem with a one-line answer
            // (`mass_of(b) * world.gravity()`), and it is the right side of the
            // trade. Box2D makes the same one, in the same place.
            const vec3 acceleration = (b.inv_mass > 0.0f)
                                    ? (b.force * b.inv_mass
                                       + gravity_ * b.gravity_scale)
                                    : vec3{};

            // (3) Advance the VELOCITY, by 8.1's rule. The constant-
            // acceleration overload, because the force was collected before the
            // step and is held fixed across it — which is what "the force
            // applied during this step" means.
            //
            // *** AND THIS IS WHERE THE SPLIT IS EITHER FREE OR IMPOSSIBLE. ***
            // Semi-implicit Euler is two independent lines, so taking the first
            // one here and leaving the second for `integrate_positions` is an
            // identity: the arithmetic, its order and its rounding are all
            // unchanged, which 8.10 §11 checks bit for bit against the
            // monolithic version this replaced. The other two rules are not
            // separable — explicit Euler's position line reads the velocity
            // from BEFORE this update, and velocity Verlet's needs the
            // acceleration too — so they run whole, here, and the position half
            // leaves their positions alone. A contact impulse applied in the
            // gap then arrives one step late for them. That is not a bug in the
            // split; it is 8.1's argument arriving again in a new place.
            if (rule_ == integrator::semi_implicit_euler)
            {
                b.state.velocity += acceleration * h;
            }
            else
            {
                integrate(b.state, acceleration, h, rule_);
            }

            // ---- the angular half's VELOCITY line, Lesson 8.3 -------------
            //
            // Structurally the mirror of the line above — a resistance, an
            // accumulator, a step — with the difference that is the whole of
            // 8.3: the resistance is a TENSOR and it lives in body axes, so it
            // has to be carried into world space before it can act on a
            // world-space torque. That is the sandwich `R * I^-1 * R^T`, and
            // 8.3 §6 is about why it is a sandwich rather than a product.
            const mat3 body_to_world = mat3_from_quat(b.orientation);
            const mat3 inv_i_world =
                body_to_world * b.inv_inertia_local * transpose(body_to_world);

            if (b.gyroscopic == gyroscopic_mode::momentum)
            {
                // ---- the momentum formulation -------------------------------
                //
                // Integrate `L`, derive `omega`. Nothing below ever writes `L`
                // except the torque line, so a torque-free body's angular
                // momentum is conserved BIT FOR BIT rather than to a tolerance.
                //
                // Note what has disappeared: there is no gyroscopic term here,
                // no Jacobian and no Newton iteration. The term was never
                // physics — it was the price of differentiating `I(t)*omega`
                // while insisting that `omega` be the state.
                //
                // *** IT IS THE ONE BLOCK THE 8.10 SPLIT CANNOT CUT. *** Its
                // orientation advance sits in the MIDDLE of its own derivation
                // — the midpoint estimate needs a half-advanced orientation and
                // the final `omega` needs the fully advanced one — so the whole
                // of it runs here, in the velocity half, and
                // `integrate_positions` skips the orientation of a body in this
                // mode. The consequence is named in the header and is real: a
                // contact impulse applied in the gap changes this body's
                // angular velocity for NEXT step's rotation, not this one. It
                // is one frame of lag on spin, on a mode that is off by
                // default, and the alternative was to make the accurate tumble
                // inaccurate for every body that never touches anything.
                vec3 l_world = body_to_world
                             * (b.inertia_local
                                * (transpose(body_to_world) * b.angular_velocity));

                l_world += b.torque * h;

                if (b.angular_damping > 0.0f)
                {
                    l_world *= std::exp(-b.angular_damping * h);
                }

                // ---- and now the MIDPOINT, which is the whole accuracy
                // ---- of this mode.
                //
                // Advancing the orientation by the angular velocity at the
                // START of the step is first order, and 8.3 §9 measured what
                // that costs on a tumbling box. `L` is conserved by
                // construction whatever this line does, so `L` cannot tell you
                // the body is wrong — the SECOND conserved quantity can. At
                // 60 Hz the kinetic energy, which a torque-free body also holds
                // exactly, drifted by **97.1%**, and `|omega|` ranged over
                // 4.0559 to 8.0350 where 8.3 §8 derives a bound of 4.0524 to
                // 4.4880. The momentum was beautifully conserved and the body
                // was doing the wrong thing with it.
                //
                // The fix is to take a half step first, read the angular
                // velocity THERE, and use that for the whole step. One extra
                // rotation matrix and one extra orientation advance took the
                // energy drift from 9.714e-01 to **1.198e-04** at the same
                // 60 Hz — a factor of 8,100 — and put `|omega|` on
                // 4.0525..4.4879, inside the bound.
                //
                // This works here and would not work on `angular_velocity`,
                // because `L` is FIXED across the whole step: the only thing
                // the half step is estimating is where the body will be
                // pointing, and that is a question a half step answers well.
                const vec3 omega_start = body_to_world
                                       * (b.inv_inertia_local
                                          * (transpose(body_to_world) * l_world));

                const quat half = advance_orientation(b.orientation, omega_start,
                                                      0.5f * h, spin_);
                const mat3 half_to_world = mat3_from_quat(half);
                const vec3 omega_mid = half_to_world
                                     * (b.inv_inertia_local
                                        * (transpose(half_to_world) * l_world));

                b.orientation = advance_orientation(b.orientation, omega_mid, h, spin_);

                // ...and re-derive the angular velocity at the orientation the
                // body actually ended up in. THIS is where the tumble comes
                // from: `L` did not move, `I_world` did, so `omega` must.
                const mat3 new_to_world = mat3_from_quat(b.orientation);
                b.angular_velocity = new_to_world
                                   * (b.inv_inertia_local
                                      * (transpose(new_to_world) * l_world));
            }
            else
            {
                // The gyroscopic term first, on its own, and the applied torque
                // after it. The order is Bullet's and it is the order the
                // derivation wants: the implicit solve asks "where does this
                // body's OWN momentum carry its angular velocity in `h`
                // seconds", which is a question about the body as it is now,
                // before anything external has been added to it.
                if (b.gyroscopic != gyroscopic_mode::off)
                {
                    b.angular_velocity = gyroscopic_step(b, h, b.gyroscopic);
                }

                b.angular_velocity += (inv_i_world * b.torque) * h;

                if (b.angular_damping > 0.0f)
                {
                    b.angular_velocity *= std::exp(-b.angular_damping * h);
                }
            }
            break;
        }
        }
    }
}

const step_report& body_world::integrate_positions(float h)
{
    for (rigid_body& b : bodies_.items())
    {
        switch (b.kind)
        {
        case body_kind::fixed:
            // Nothing moves, but the accumulators are cleared, for the reason
            // the velocity half gives.
            b.force = vec3{};
            b.torque = vec3{};
            continue;

        case body_kind::kinematic:
        {
            // The position update is the one line of the integrator that
            // applies, written out rather than routed through `integrate` so
            // that it is obvious that the velocity is NOT being changed.
            b.state.position += b.state.velocity * h;

            // Lesson 8.3: it TURNS as well as travels. A rotating platform, a
            // swinging door and a fan blade are all this line.
            b.orientation = advance_orientation(b.orientation, b.angular_velocity, h, spin_);

            b.force = vec3{};
            b.torque = vec3{};
            continue;
        }

        case body_kind::dynamic:
        {
            if (b.sleeping)
            {
                // Cleared even though nothing was integrated. See the velocity
                // half: a force accumulating on a sleeping body for ten seconds
                // makes its eventual wake-up a launch.
                b.force = vec3{};
                b.torque = vec3{};
                continue;
            }

            // (4) The position line, at the velocity the step ENDED with —
            // which, after Lesson 8.10, means the velocity a contact solve
            // running in the gap approved, rather than the one gravity asked
            // for. That substitution is the entire reason this function exists.
            //
            // Under the two non-separable rules the velocity half already did
            // this line; see `integrate_velocities`.
            if (rule_ == integrator::semi_implicit_euler)
            {
                b.state.position += b.state.velocity * h;
            }

            // ...then damping, exactly, as `v *= exp(-k*h)`. AFTER the position
            // line and not folded into the acceleration, because `a = -k*v` is
            // not a force derived from a potential and feeding it through a
            // symplectic rule gets you plain explicit Euler's stability limit
            // on it (8.1 §8). A body with `damping` 0 pays a branch and no exp.
            //
            // It stays on this side of the split, after the move rather than
            // before it, because that is where it was: `step` has to remain the
            // function 8.2 shipped, bit for bit, and 8.10 §11 checks that it is.
            if (b.damping > 0.0f) { apply_drag(b.state, b.damping, h); }

            // ---- THE SECOND BASIS CHANGE, AND IT IS THE PRICE OF THE SPLIT --
            //
            // 8.3 §12's largest single saving was hoisting `mat3_from_quat` so
            // that the angular half built the rotation once and used it twice —
            // 7.843 ns apiece on a 33.793 ns update. Two functions cannot share
            // a local, so this one is built again. 8.10 §13 measures what that
            // costs and what pays for it: a sleeping body reaches neither half,
            // and a settled scene sleeps.
            //
            // It is built BEFORE the orientation advance on purpose, so that
            // the angular-momentum report below reads the same rotation the
            // monolithic version read.
            const mat3 body_to_world = mat3_from_quat(b.orientation);

            // (5) The orientation advance. NOT an addition: `q += omega*h` is
            // not a rotation — it is not even a unit quaternion — and 8.3 §7 is
            // the derivation of what goes there instead.
            //
            // Skipped for the momentum formulation, which advanced its own
            // orientation in the velocity half because its derivation cannot be
            // cut in half. See there.
            if (b.gyroscopic != gyroscopic_mode::momentum)
            {
                b.orientation = advance_orientation(b.orientation, b.angular_velocity, h, spin_);
            }

            // (6) ...and only now are the accumulators cleared, so that a debug
            // overlay, an assertion or the contact solver running in the gap
            // can still read what was applied during the step that just ran.
            b.force = vec3{};
            b.torque = vec3{};

            // A `sqrt` PER BODY, and it was measured before being kept. The
            // obvious saving is to compare squared speeds and take one root at
            // the end; 8.2 §10.2 timed both and found 1.036, 0.968 and 0.969
            // over three runs — a tie, and once a loss. `sqrtss` is a pipelined
            // single instruction on every target this engine builds for, and
            // the version that reads as what it means wins by default when the
            // measurement is a wash.
            const float speed = length(b.state.velocity);
            if (speed > report_.max_speed) { report_.max_speed = speed; }

            const float spin = length(b.angular_velocity);
            if (spin > report_.max_spin) { report_.max_spin = spin; }

            // Momentum excludes immovable bodies: theirs is either zero or
            // infinite and neither belongs in a sum that 8.9 compares across a
            // collision. Sleeping bodies are excluded too and it costs nothing
            // — `update_sleep` zeroed their velocities, so their contribution
            // is exactly the zero vector.
            const float mass = 1.0f / b.inv_mass;
            report_.momentum += b.state.velocity * mass;

            // Angular momentum about the WORLD ORIGIN, which is two terms and
            // the second one is the one people forget: `I*omega` is the body's
            // spin about its own centre, and `r x (m*v)` is the angular
            // momentum its linear motion carries about the origin. A body
            // sailing past in a straight line without rotating at all has the
            // second and not the first, which is how a planet's orbit conserves
            // one.
            report_.angular_momentum +=
                body_to_world * (b.inertia_local
                                 * (transpose(body_to_world) * b.angular_velocity))
                + cross(b.state.position, b.state.velocity * mass);
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

rigid_body make_box(vec3 position, float mass, vec3 half_extents)
{
    rigid_body b = make_dynamic(position, mass);
    set_inertia(b, inertia_solid_box(mass, half_extents));
    return b;
}

rigid_body make_sphere(vec3 position, float mass, float radius)
{
    rigid_body b = make_dynamic(position, mass);
    set_inertia(b, inertia_solid_sphere(mass, radius));
    return b;
}

rigid_body make_fixed(vec3 position)
{
    rigid_body b;
    b.state.position = position;
    b.inv_mass = 0.0f;

    // Nine zeros, for exactly the reason the one zero above is there: no torque
    // can spin this, exactly, with no branch and no sentinel. 8.2's argument
    // for `inv_mass = 0`, made nine floats wide. The forward tensor is zero too,
    // which makes the body's angular momentum exactly zero rather than infinite.
    b.inv_inertia_local = mat3{vec3{}, vec3{}, vec3{}};
    b.inertia_local = mat3{vec3{}, vec3{}, vec3{}};
    b.kind = body_kind::fixed;
    return b;
}

rigid_body make_kinematic(vec3 position, vec3 velocity)
{
    rigid_body b;
    b.state.position = position;
    b.state.velocity = velocity;
    b.inv_mass = 0.0f;

    // Immovable by torques as well as by forces. Set `angular_velocity`
    // directly to make it turn — that is what kinematic means.
    b.inv_inertia_local = mat3{vec3{}, vec3{}, vec3{}};
    b.inertia_local = mat3{vec3{}, vec3{}, vec3{}};
    b.kind = body_kind::kinematic;
    return b;
}

} // namespace engine::phys
