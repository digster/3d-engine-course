// engine/src/phys/integrate.cpp — the parts of Lesson 8.1 that are not templates.
//
// The general stepper is a template in the header, because an acceleration
// function that does not inline is a function call per body per step and §10
// measures that at more than the step itself costs. What lands here is
// everything that does not depend on a callable: the constant-acceleration
// overload (gravity, which is most of what a game integrates), the two drag
// helpers, and the four diagnostic functions the lesson's harness checks its
// own claims against.
//
// Nothing in this file is hot. `area_factor` and `shadow_energy` are called once
// per frame by a debug overlay at most; the stepping in `integrate` below is the
// only loop-resident code here and it is eleven multiplies.

#include <engine/phys/integrate.hpp>

#include <cmath>

namespace engine::phys
{

const char* name_of(integrator rule)
{
    switch (rule)
    {
    case integrator::explicit_euler:      return "explicit Euler";
    case integrator::semi_implicit_euler: return "semi-implicit Euler";
    case integrator::velocity_verlet:     return "velocity Verlet";
    }
    // Unreachable for a valid enumerator, and not an assert: this is a naming
    // function used by logs and debug UI, and a log line is the last place that
    // should be able to stop the program. Lesson 5.3's rule.
    return "?";
}

void integrate(motion& m, vec3 acceleration, float h, integrator rule)
{
    switch (rule)
    {
    case integrator::explicit_euler:
        m.position += m.velocity * h;
        m.velocity += acceleration * h;
        break;

    case integrator::semi_implicit_euler:
        m.velocity += acceleration * h;
        m.position += m.velocity * h;
        break;

    case integrator::velocity_verlet:
        // With `acceleration` constant, `a0` and `a1` are the same vector, so
        // the trapezium average collapses to it and the velocity update is
        // identical to the other two. Only the position line differs — and that
        // `0.5*a*h*h` is precisely the term the Taylor expansion says the other
        // two rules are missing, which is why this case is EXACT here. Not
        // approximately: the closed form for constant acceleration is
        // `x + v*h + a*h^2/2`, and that is the line below.
        m.position += m.velocity * h + acceleration * (0.5f * h * h);
        m.velocity += acceleration * h;
        break;
    }
}

void apply_drag(motion& m, float k, float h)
{
    // `std::exp` and not `1 - k*h`, for the reason the header gives at length:
    // the approximation is unstable, frame-rate dependent, and looks fine.
    //
    // A negative `k` would be an accelerating "drag", which is a bug in the
    // caller rather than a mode; it is left to do exactly what the arithmetic
    // says rather than being silently clamped, because a silently clamped
    // physical constant is a bug that never surfaces.
    m.velocity *= std::exp(-k * h);
}

float damping_factor(float retained_per_second, float h)
{
    // `pow(r, h)`, with the two degenerate inputs handled rather than trusted to
    // the libm: `pow(0, 0)` is 1 by the standard, which is the right answer for
    // a zero-length step and the wrong one for total damping, and a negative `r`
    // makes `pow` return NaN and take the whole simulation with it.
    if (retained_per_second <= 0.0f) { return (h <= 0.0f) ? 1.0f : 0.0f; }
    return std::pow(retained_per_second, h);
}

float natural_frequency(float stiffness, float mass)
{
    if (mass <= 0.0f) { return 0.0f; }
    return std::sqrt(stiffness / mass);
}

float max_stable_step(integrator rule, float omega)
{
    if (omega <= 0.0f)
    {
        // No restoring force, so nothing to be unstable about: every rule below
        // reduces to straight-line motion, which is exact at any step size.
        // Reported as infinity rather than as a large number, because the caller
        // is dividing by it or comparing against it and either wants the real
        // answer.
        return HUGE_VALF;
    }

    switch (rule)
    {
    // THE ZERO IS THE LESSON. See the header: there is no stable step size, so
    // there is no largest one, and the honest return value is not a small
    // positive number. A caller writing `h = 0.5f * max_stable_step(...)` gets
    // zero and a simulation that does not move, which is a far better bug report
    // than a simulation that quietly explodes twenty seconds in.
    case integrator::explicit_euler:      return 0.0f;

    // |trace| <= 2 for the update matrix, which works out to h*omega <= 2 for
    // both. Derived in §4.2 and §8; the two rules have the same trace and
    // therefore the same limit, which is not a coincidence — both are
    // area-preserving, so their eigenvalues multiply to 1 and the only way off
    // the unit circle is through a real pair.
    case integrator::semi_implicit_euler:
    case integrator::velocity_verlet:     return 2.0f / omega;
    }
    return 0.0f;
}

float spring_energy(const motion& m, float omega)
{
    return 0.5f * (length_squared(m.velocity)
                   + omega * omega * length_squared(m.position));
}

float shadow_energy(const motion& m, float omega, float h)
{
    // (|v|^2 + w^2|x|^2 - h*w^2*(x.v)) / 2.
    //
    // The 3D form is the per-axis 1D form summed, because `a = -w^2 x` decouples
    // into three independent scalar oscillators — each component of the position
    // drives only the matching component of the acceleration. The dot product is
    // that sum of three products, which is why this generalises without a word
    // of extra derivation.
    const float w2 = omega * omega;
    return 0.5f * (length_squared(m.velocity)
                   + w2 * length_squared(m.position)
                   - h * w2 * dot(m.position, m.velocity));
}

float area_factor(integrator rule, float omega, float h)
{
    const float u = h * h * omega * omega;
    switch (rule)
    {
    case integrator::explicit_euler:      return 1.0f + u;
    case integrator::semi_implicit_euler: return 1.0f;
    case integrator::velocity_verlet:     return 1.0f;
    }
    return 1.0f;
}


// ---- Orientation (Lesson 8.3) ----------------------------------------------

float spin_inflation(float omega_magnitude, float h)
{
    // |q + (h/2)*w_pure*q| with |q| = 1. The product `w_pure*q` is orthogonal to
    // `q` in 4-D — a fact worth checking once rather than believing: their dot
    // product is `-dot(w, q.v)*q.w + dot(w*q.w + w x q.v, q.v)`, and the first
    // two terms cancel while the cross product is perpendicular to `q.v`. So the
    // step is a right-angled triangle, and Pythagoras gives the length directly.
    const float half = 0.5f * omega_magnitude * h;
    return std::sqrt(1.0f + half * half);
}

float spin_angle_error(float omega_magnitude, float h)
{
    const float asked = omega_magnitude * h;
    if (asked <= 0.0f) { return 0.0f; }

    // After renormalising, the result is the unit quaternion whose imaginary
    // part points along `omega` with `tan(theta/2) = asked/2` — so the angle it
    // actually turns through is twice that arctangent.
    const float achieved = 2.0f * std::atan(0.5f * asked);
    return achieved / asked - 1.0f;
}

} // namespace engine::phys
