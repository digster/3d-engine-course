// engine/include/engine/phys/integrate.hpp — how a velocity becomes a position.
//
// Lesson 8.1, and the first file in `engine::phys`. It is small on purpose:
// everything in this header is the answer to one question, and the question is
// the oldest one in simulation. You know where a body is and how fast it is
// going; a force tells you how fast that speed is changing. Advance all of it by
// `h` seconds. Go.
//
// ---- WHAT THIS FILE IS NOT -------------------------------------------------
//
// It is not the loop. `engine::fixed_step` has handed out a constant `h` since
// Lesson 1.4 and needed no changes to do it — 1.4 established WHY the frame rate
// must not decide how far the simulation advances, and this file is what runs
// inside the step it hands you. Nothing here knows what a frame is.
//
// It is not a rigid body either. There is no mass, no orientation, no inertia
// tensor and no collision: `motion` is a point with a position and a velocity,
// which is the smallest thing that can be integrated at all. Mass arrives in 8.2
// with `F = ma`, orientation in 8.3.
//
// ---- THE ENTIRE LESSON, IN ONE PARAGRAPH -----------------------------------
//
// Write the two update lines in the order that first occurs to you —
//
//     position += velocity * h;      // using the velocity you HAD
//     velocity += acceleration * h;
//
// — and you have explicit (forward) Euler, which is **unconditionally
// unstable**. Not "inaccurate", not "needs a small step": on a spring it adds
// energy on every single step, for every step size, forever. Swap the two lines
// so the position uses the velocity it has just been given, and you have
// semi-implicit (symplectic) Euler, which preserves phase-space area **exactly**
// and keeps a spring on a bounded orbit for as long as you care to run it.
//
// Same number of multiplies. Same number of loads. Same cache behaviour. One of
// them is wrong at every step size and the other is right up to `h = 2/w`, and
// the difference between them is which of two lines you type first. That is the
// rarest thing in engineering — a free lunch — and §4 of the lesson proves it
// with a determinant rather than asking you to believe it.
//
// ---- WHY A SPRING IS THE TEST CASE AND NOT A CHERRY-PICKED ONE -------------
//
// Under constant acceleration — gravity, and nothing else — all three rules
// below behave: their position error grows LINEARLY in time and is about
// `0.5*a*h*t`, which at 60 Hz is eight centimetres after a second of falling.
// You will not see this bug in your first demo.
//
// You will see it the moment a force depends on POSITION, and in a physics
// engine essentially every force does. A contact that pushes a box out of the
// floor is a stiff spring. A joint that holds a ragdoll's elbow together is a
// stiff spring. A soft-body edge, a suspension arm, a spring-damper camera: all
// springs. `a = -w^2 x` is not a toy chosen because it is easy; it is the
// linearisation of every restoring force in the module, and if an integrator
// cannot survive it, nothing built on top of it can.
#pragma once

#include <engine/math/vec3.hpp>

namespace engine::phys
{

/// The kinematic state of one point: where it is, and how fast it is going.
///
/// Deliberately NOT a rigid body. No mass (8.2), no orientation (8.3), no
/// handle (8.2 puts these in a table). A struct of two vectors is what an
/// integrator actually operates on, and keeping it that small is what lets the
/// whole of §4's analysis be about six floats rather than about an engine.
///
/// The units are this course's, fixed on conventions.html §3: metres and
/// seconds, so `velocity` is m/s and the acceleration you pass in is m/s².
struct motion
{
    vec3 position{};
    vec3 velocity{};
};

/// Which rule to advance the state with.
///
/// Named for what they are in the literature, because you will meet them again
/// in papers: "symplectic Euler", "semi-implicit Euler" and "Euler-Cromer" are
/// three names for `semi_implicit_euler`, and velocity Verlet is what molecular
/// dynamics has used since 1967 and what most game engines quietly use for
/// projectiles.
///
/// There is no `implicit_euler`. Backward Euler is unconditionally *stable* and
/// it is the obvious next thing to reach for — but it computes the new state
/// from forces evaluated AT the new state, which is an implicit equation and
/// therefore a root find inside every step. For the linear spring the solve is
/// closed form and the lesson does it by hand in §4.3, where it turns out to
/// have the opposite disease: it removes energy on every step, so a pendulum
/// grinds to a halt and a stack of boxes sinks. Costly AND lossy is why no game
/// engine ships it for general dynamics. Where it does appear — cloth, hair — it
/// is because that damping is wanted.
enum class integrator : int
{
    /// position from the OLD velocity. Unconditionally unstable on a spring;
    /// here to be measured, not to be used.
    explicit_euler,

    /// position from the NEW velocity. Area-preserving, stable for `h*w < 2`,
    /// and the default for everything in this module.
    semi_implicit_euler,

    /// Second-order and still area-preserving, at the price of a second force
    /// evaluation per step. Exact for constant acceleration.
    velocity_verlet,
};

/// Human-readable name, for logs and debug UI. Never null.
[[nodiscard]] const char* name_of(integrator rule);

// ---------------------------------------------------------------------------
// Stepping
// ---------------------------------------------------------------------------

/// Advance `m` by `h` seconds under a **constant** acceleration.
///
/// This is the gravity case, and it is separated from the general one because it
/// is the case where the choice of rule very nearly does not matter — see §5.
/// All three agree on the velocity to the last bit; they disagree about position
/// by exactly `0.5*a*h*t` after `t` seconds, with `explicit_euler` falling short
/// and `semi_implicit_euler` overshooting by the same amount, and
/// `velocity_verlet` landing on the closed-form answer exactly.
void integrate(motion& m, vec3 acceleration, float h, integrator rule);

/// Advance `m` by `h` seconds under an acceleration that depends on **position**.
///
/// @param accel Anything callable as `vec3 accel(vec3 position)`. A template
///        rather than a `std::function` so that a one-line spring inlines into
///        the step and costs nothing — §10 measures the difference, and it is
///        the whole cost of the call.
///
/// **POSITION ONLY, and that restriction is load-bearing.** The area-preserving
/// property that makes `semi_implicit_euler` worth having is a statement about
/// forces derived from a potential, which means forces that depend on where you
/// are and not on how fast you are going. A drag force `a = -k*v` is not one,
/// and feeding it through here gets you plain explicit Euler on the velocity
/// with plain explicit Euler's stability limit — which is `h < 2/k`, and which
/// your simulation will find for you the first time somebody raises the drag
/// coefficient. Use `apply_drag` for that, which solves it exactly instead.
template <class Accel>
void integrate(motion& m, Accel&& accel, float h, integrator rule)
{
    switch (rule)
    {
    case integrator::explicit_euler:
    {
        // The order below is the bug, written out. `position` is advanced with
        // the velocity as it stood at the START of the step, so the two updates
        // are independent and the whole step is one evaluation of the state at
        // time t extrapolated forward. That independence is exactly what makes
        // the update matrix in §4.1 have a determinant bigger than one.
        const vec3 a = accel(m.position);
        m.position += m.velocity * h;
        m.velocity += a * h;
        break;
    }

    case integrator::semi_implicit_euler:
    {
        // ...and the fix, which is the same two lines the other way up. The
        // position now moves at the velocity the body will have at the END of
        // the step, which couples the updates — and coupling them is what drops
        // the determinant to exactly 1.
        const vec3 a = accel(m.position);
        m.velocity += a * h;
        m.position += m.velocity * h;
        break;
    }

    case integrator::velocity_verlet:
    {
        // Move with the acceleration you have, then correct the velocity with
        // the average of the accelerations at both ends of the step. That
        // average is a trapezium rule, which is why this one is second order.
        //
        // TWO EVALUATIONS OF `accel`, and this implementation does not hide it.
        // Production Verlet caches `a1` and reuses it as the next step's `a0`,
        // which halves the force work at the price of a state field that the
        // other two rules have no use for. That is the right trade for a solver
        // whose force evaluation is a broadphase query; it is the wrong trade
        // for a header whose job this lesson is to make legible, and §10
        // measures what the honesty costs.
        const vec3 a0 = accel(m.position);
        m.position += m.velocity * h + a0 * (0.5f * h * h);
        const vec3 a1 = accel(m.position);
        m.velocity += (a0 + a1) * (0.5f * h);
        break;
    }
    }
}

// ---------------------------------------------------------------------------
// Drag, which is a different problem
// ---------------------------------------------------------------------------

/// Apply linear drag `a = -k*v` for `h` seconds, exactly.
///
/// @param k Drag coefficient per second. `v` decays to `1/e` of itself in `1/k`
///          seconds.
///
/// THE EXACT SOLUTION, not a step of anything. `dv/dt = -k*v` has the closed
/// form `v(t) = v0 * exp(-k*t)`, so there is no reason to approximate it and
/// two reasons not to: an explicit step is `v *= (1 - h*k)`, which REVERSES the
/// velocity once `h*k > 1` and diverges once `h*k > 2` — a drag force that
/// accelerates things — and it is frame-rate dependent in a way that survives
/// every code review because it looks like nothing.
///
/// Position is left alone. Drag changes where a body ends up, of course, but it
/// does so through the velocity, and folding the exact position integral in here
/// would quietly make this function a stepper. Call it beside `integrate`, in
/// whichever order your fixed step defines; at 60 Hz the difference between the
/// two orders is below a millimetre per second of simulated drag, and §7 gives
/// the number.
void apply_drag(motion& m, float k, float h);

/// The per-step multiplier for "retain `retained_per_second` of the velocity
/// every second", at a step of `h`.
///
/// This is `pow(retained_per_second, h)`, and it exists because the two-character
/// version of it — `v *= 0.99f;` once per step — is the single most common
/// frame-rate dependency in gameplay code. At 30 Hz that line keeps 74% of the
/// velocity per second; at 60 Hz, 55%; at 120 Hz, 30%. Same constant, same code,
/// three different games. §7 measures all three.
[[nodiscard]] float damping_factor(float retained_per_second, float h);

// ---------------------------------------------------------------------------
// Choosing `h`, and knowing when you cannot
// ---------------------------------------------------------------------------

/// Angular frequency of a mass `mass` on a spring of stiffness `stiffness`,
/// in radians per second: `sqrt(k/m)`.
///
/// The number every other function in this section wants. Divide by 2*pi for the
/// oscillation rate in Hz, which is the number to compare against your step rate.
[[nodiscard]] float natural_frequency(float stiffness, float mass);

/// The largest `h` at which `rule` stays bounded on a spring of angular
/// frequency `omega`.
///
/// **Returns 0 for `explicit_euler`, and that zero is the whole lesson.** There
/// is no step size at which explicit Euler is stable on an oscillator; the
/// answer is not "small", it is "none". The other two return `2/omega`, which is
/// a hard ceiling and not a target: at `h*omega = 2` the orbit is a bounded but
/// wildly distorted thing, and §6 shows what the last 10% before the limit looks
/// like. Budget a factor of five or so.
///
/// Reading it the other way round is how it is actually used: a 60 Hz step and a
/// safety factor of 5 admits `omega <= 2*60/5 = 24 rad/s`, which is a spring of
/// about 3.8 Hz. Anything stiffer needs sub-stepping — or, in Module 8's real
/// answer, needs to stop being a spring and become a constraint (8.10).
[[nodiscard]] float max_stable_step(integrator rule, float omega);

// ---------------------------------------------------------------------------
// Diagnostics: what is actually conserved
// ---------------------------------------------------------------------------

/// Mechanical energy per unit mass of a point on a spring `a = -omega^2 * x`:
/// `(|v|^2 + omega^2 * |x|^2) / 2`.
///
/// The number to watch in a debug overlay while you are choosing a step size. It
/// should be flat. If it climbs, something in your loop is explicit Euler.
[[nodiscard]] float spring_energy(const motion& m, float omega);

/// The quantity `semi_implicit_euler` conserves **exactly** on that spring —
/// `(|v|^2 + omega^2*|x|^2 - h*omega^2*(x . v)) / 2` — for the given `h`.
///
/// This is the deep fact of §4.4 and it is worth stating plainly, because it is
/// what makes the difference between "does not blow up" and "is correct enough
/// to ship". Semi-implicit Euler does NOT conserve `spring_energy`: the true
/// energy wobbles up and down, by `h*omega` peak-to-peak in relative terms —
/// 10.5% at 60 Hz on a 1 Hz spring. What it conserves is this slightly sheared
/// cousin, and it conserves it to the last bit, forever. The orbit is a level
/// set of THIS, which is a closed ellipse, which is why the wobble never
/// accumulates into a trend.
///
/// The shear term is also where the stability limit comes from: the quadratic
/// form above is positive-definite — an ellipse rather than a hyperbola —
/// precisely when `h*omega < 2`. Beyond it there is no bounded level set to sit
/// on, and the trajectory runs off along a hyperbola. Stability and the
/// existence of this conserved quantity are the same statement.
[[nodiscard]] float shadow_energy(const motion& m, float omega, float h);

/// The factor by which one step of `rule` scales area in phase space, for a
/// spring of angular frequency `omega`.
///
/// The determinant of the 2x2 update matrix, and the single number this lesson
/// is built around:
///
///   - `explicit_euler`       `1 + h^2*omega^2`   — always > 1. Grows.
///   - `semi_implicit_euler`  `1`                 — exactly. Preserves.
///   - `velocity_verlet`      `1`                 — exactly. Preserves.
///
/// (Backward Euler, which this header does not implement, is `1/(1 + h^2*omega^2)`
/// — always < 1, always shrinking, the exact reciprocal of the explicit one.)
///
/// Available as a function rather than only as prose because §4.2 MEASURES it:
/// the harness steps a small parallelogram of states through each rule and
/// compares the area it comes out with against this prediction. A claim about a
/// determinant that is never evaluated is a claim you are asking to be believed.
[[nodiscard]] float area_factor(integrator rule, float omega, float h);

} // namespace engine::phys
