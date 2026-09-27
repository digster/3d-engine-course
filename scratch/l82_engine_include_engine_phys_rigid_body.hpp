// engine/include/engine/phys/rigid_body.hpp — a thing with a mass, and the
// table of them.
//
// Lesson 8.2. Lesson 8.1 gave this engine a `motion` — a position and a velocity
// — and a set of rules for advancing one through time. What it did not give it is
// any way to say WHY a body accelerates. You hand `integrate` an acceleration and
// it believes you. That is enough to drop a cube, and it is enough for nothing
// else, for two reasons that look small and are not.
//
//   1. AN ACCELERATION CANNOT BE ADDED UP. Gravity wants to accelerate a body
//      downward, a thruster wants to push it forward, drag wants to slow it, and
//      the wind wants to shove it sideways. Each of those is a separate system
//      that knows nothing about the others, and every one of them wants to write
//      the same field. The last writer wins and the rest of the physics silently
//      does not happen. Forces do not have this problem — they ADD, by Newton's
//      second law being linear in F — which is why every physics engine ever
//      written has an accumulator and not a setter.
//
//   2. AN ACCELERATION DOES NOT KNOW ABOUT MASS, so nothing in 8.1's world can
//      tell a crate from a pebble. Push both with the same thruster and both
//      accelerate identically, which is wrong in a way a player notices
//      immediately. And you cannot fix it by passing `F/m` at the call site,
//      because the whole point of the accumulator above is that the division
//      happens ONCE, after everything has been added, at the moment the step
//      runs.
//
// So: `F = m*a`, an accumulator, and a table to keep the bodies in.
//
// ---- THE TWO THINGS IN THIS FILE THAT ARE NOT OBVIOUS ----------------------
//
// INVERSE MASS, NOT MASS, and the reason is not the divide. See `inv_mass`.
//
// AND A BODY'S POSITION IS IN **WORLD SPACE**, unconditionally, which is a
// restriction rather than a feature and is the second half of this lesson. A
// rigid body integrated in a parent's local space is integrated in a frame that
// may scale it, shear it, and — if the parent rotates — accelerate it. Under a
// parent scaled by 2 the body falls at 2 g. Under a NON-UNIFORMLY scaled parent
// gravity stops pointing down at all: §8 measures a body that falls at 63.4
// degrees off vertical and 1.58 g, with nothing in the scene doing anything
// unusual. `body_world` therefore has no concept of a parent, a hierarchy or a
// transform, and `inspect_frame` below exists so that you can MEASURE what one
// would have done to you rather than take that on trust.
//
// ---- WHAT IS STILL NOT HERE -------------------------------------------------
//
// No rotation. A `rigid_body` here is a point with a mass: it translates and it
// does not spin, `add_force` has no application point, and there is no torque
// and no inertia tensor. That is Lesson 8.3, and it needs 7.4's quaternions.
// Everything in this file is the LINEAR half, and the linear half is genuinely
// separable — momentum and angular momentum do not mix.
//
// No collision (8.4 onward), no constraints (8.10, 8.11), no sleeping (8.10),
// and no continuous collision detection: `step_report::max_travel` measures how
// far the fastest body moves in one step, which is the number that decides
// whether a body will tunnel through a wall, but nothing here does anything
// about it. Lesson 1.8's swept test is the shape of the answer and 8.13 is where
// it comes due.
#pragma once

#include <engine/core/handle.hpp>
#include <engine/core/pool.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/transform.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/integrate.hpp>

#include <cstdint>
#include <span>

namespace engine::phys
{

/// Standard gravity at the Earth's surface, in metres per second squared.
///
/// **9.80665 is a defined constant, not a measurement** — it is fixed by the
/// General Conference on Weights and Measures, and the actual local value varies
/// from about 9.764 at the top of a mountain near the equator to 9.834 at the
/// poles. Games use 9.81, and this engine rounds to it for the same reason
/// everyone does: the third decimal place of gravity is four orders below the
/// smallest thing a player can perceive, and a round number is easier to
/// recognise in a debugger.
///
/// THE NUMBER IS IN METRES AND SECONDS, AND THAT IS WHY conventions.html §3
/// MATTERS. It has said "one unit is one metre" since Module 2, when nothing in
/// the engine could tell the difference. This constant is the first line of code
/// in the course for which a unit is not a matter of taste: put it in a world
/// where a unit is half a metre and everything in it falls at the wrong rate,
/// with a symptom — "floaty" or "toy-like" — that names the bug if you know how
/// to read it. §7 derives the relationship (every duration scales as the square
/// root of the length scale) and measures it.
inline constexpr float k_gravity = 9.81f;

/// The engine's default gravity vector: down is −y, by conventions.html §1.
inline constexpr vec3 k_gravity_down{0.0f, -k_gravity, 0.0f};

// ---------------------------------------------------------------------------
// What kind of body this is
// ---------------------------------------------------------------------------

/// Three answers to two different questions, which is why this is not a bool.
///
/// Engines universally offer these three and the difference between the last two
/// is the one people get wrong, so it is worth stating as two questions rather
/// than three names:
///
///   **"Can a force move it?"** — answered by `inv_mass`, and the answer is no
///   whenever `inv_mass` is 0. A crate landing on a lift must not push the lift
///   down, however heavy the crate is.
///
///   **"Does it move at all?"** — answered by this enum. A lift moves; a floor
///   does not. Both are immovable BY FORCES, and only one of them has a velocity
///   that the crate standing on it will need to be carried by (8.9).
///
/// Conflating the two gives you the two classic bugs. Make the lift dynamic and
/// heavy, and it sags under load and drifts under gravity. Make it static and
/// teleport it each frame, and anything standing on it is left behind, because a
/// body with no velocity imparts none.
///
/// **`fixed` IS WHAT EVERY OTHER ENGINE CALLS `static`.** `static` is a C++
/// keyword and cannot be an enumerator, even inside an `enum class`, so the name
/// has to change; `fixed` is the word this codebase uses and the doc comment
/// says the other one so that a search for it lands here.
enum class body_kind : std::uint8_t
{
    /// Forces move it and gravity pulls on it. The default, and the only kind
    /// that `step` integrates a velocity for.
    dynamic,

    /// Moved by gameplay, never by physics. Its velocity is whatever you set,
    /// it is integrated (so it travels), gravity does not touch it, and no force
    /// or impulse can change it. A lift, a moving platform, a swinging door.
    kinematic,

    /// Does not move, ever. Not integrated at all — `step` skips it entirely,
    /// which is what lets 8.6's broadphase keep every one of these in a
    /// structure it never has to rebuild. The ground, a wall, a rock.
    fixed,
};

/// Human-readable name, for logs and debug UI. Never null.
[[nodiscard]] const char* name_of(body_kind kind);

// ---------------------------------------------------------------------------
// The body
// ---------------------------------------------------------------------------

/// A point with a mass: where it is, how fast it is going, and what is pushing
/// on it.
///
/// **IT HAS A `motion` RATHER THAN BEING ONE**, and 8.1's header said it would.
/// That is composition doing a real job, not an aesthetic preference: every
/// function in `integrate.hpp` — all three rules, both drag helpers, all four
/// diagnostics — operates on a `motion&` and none of them has to learn what a
/// body is. The harness in §3 steps a bare `motion` beside a `rigid_body` and
/// compares them bit for bit, which would be impossible if mass had been added
/// by widening the struct.
///
/// Sixty bytes, which is under a cache line, and the fields are ordered so the
/// two that the step touches every time are first. §10 measures the walk.
struct rigid_body
{
    /// Position and velocity, **in world space**. Metres and metres per second.
    ///
    /// See the file header: this is not negotiable and `body_world` has no way
    /// to express anything else. `place_in_parent` is the bridge to a scene
    /// hierarchy and it converts rather than integrating.
    motion state{};

    /// Newtons, accumulated since the last step, and **cleared by `step`**.
    ///
    /// A force is an INPUT to a step, not a property of a body — it is the sum
    /// of what every system had to say this frame, and next frame they will all
    /// say it again. Leaving it set would mean a thruster fired once keeps
    /// firing forever, which is the single most common bug in a first physics
    /// engine and presents as objects that slowly accelerate away.
    ///
    /// **THE CLEARING IS AT THE END OF THE STEP AND NOT THE START**, which
    /// matters the moment anything wants to look at it: a debug overlay drawing
    /// force arrows, an assertion checking that nothing exceeded a sane
    /// magnitude, the solver in 8.10 reading what the contact generator asked
    /// for. Clear it first and every one of those reads zero.
    vec3 force{};

    /// **One over the mass, in 1/kg. Zero means immovable.**
    ///
    /// Storing the reciprocal is a decision every physics engine makes and the
    /// usual justification — "it turns a divide into a multiply" — is the least
    /// of it. A divide is about four times a multiply on modern hardware and
    /// this happens once per body per step; you would never restructure a public
    /// API for that. Three real reasons, in increasing order of weight:
    ///
    ///   1. **IMMOVABLE IS REPRESENTABLE, AND IT IS THE COMMON CASE.** The floor
    ///      is immovable, and so is every wall, every rock and every piece of
    ///      level geometry — which is most of the bodies in a real scene. Its
    ///      mass is not large, it is *infinite*, and `inv_mass = 0` says so
    ///      exactly, in a float, with no sentinel and no branch. Storing mass
    ///      instead forces either a magic value (and `HUGE_VALF` propagates into
    ///      NaN the moment two of them are subtracted, which §9.1 shows) or a
    ///      separate bool that every piece of arithmetic has to remember to
    ///      consult.
    ///
    ///   2. **THE SOLVER WANTS EXACTLY THIS QUANTITY AND NOT MASS.** Every
    ///      contact and every joint in 8.9–8.11 computes an effective mass that
    ///      is built from `inv_mass_a + inv_mass_b`, and that sum has a clean
    ///      meaning when one of the two is zero. In terms of masses it is
    ///      `(m_a*m_b)/(m_a+m_b)` — the reduced mass — which is what you get
    ///      when you take the reciprocal of a sum of reciprocals, and which
    ///      needs a special case at every infinity. The storage is chosen to
    ///      match the arithmetic that the hot loop actually performs.
    ///
    ///   3. **THE DEGENERATE CASE MOVES TO WHERE IT BELONGS.** With mass
    ///      stored, `mass = 0` is a divide by zero and a massless body is a
    ///      crash. With the reciprocal stored, a massless body is
    ///      `inv_mass = infinity`, which is a body that any force accelerates
    ///      infinitely — still nonsense, but nonsense that does not divide, and
    ///      which `set_mass` refuses at the one place it can be created.
    ///
    /// The default is 1, i.e. one kilogram, because a default of 0 would make
    /// every default-constructed body immovable and that is the kind of default
    /// that costs an afternoon.
    float inv_mass = 1.0f;

    /// Velocity-space damping, in 1/s. **This is not air resistance.**
    ///
    /// Applied by `step` as `apply_drag` — the exact `v *= exp(-k*h)` from 8.1,
    /// so it is frame-rate independent and cannot reverse the velocity the way
    /// `v *= (1 - k*h)` does. `k = 0` is no damping; `k = 1` costs a body 63% of
    /// its speed per second.
    ///
    /// **AND IT IS MASS-INDEPENDENT, WHICH IS EXACTLY WRONG AS PHYSICS AND
    /// EXACTLY RIGHT AS A KNOB.** Real linear drag is a FORCE, `F = -b*v`, so
    /// the acceleration it produces is `-(b/m)*v` and a heavy body is harder for
    /// the air to slow: a cannonball and a feather of the same shape have wildly
    /// different terminal speeds, which is the entire reason Galileo's
    /// experiment needed a tower rather than a bench. This field skips the mass,
    /// so every body damped at the same `k` reaches the SAME terminal speed
    /// regardless of what it weighs, and §5 measures both and puts the numbers
    /// side by side.
    ///
    /// It is here anyway, and named `damping` rather than `drag`, because what
    /// gameplay wants nine times in ten is "bleed off speed so this feels
    /// controllable", which is a stability and feel control with no physical
    /// content at all. When you want the physics, apply `-b*v` through
    /// `add_force` and let the mass do its job; `terminal_speed_damped` and
    /// `terminal_speed_dragged` sit next to each other below so the difference
    /// is one line of reading.
    float damping = 0.0f;

    /// Multiplier on the world's gravity for this body alone.
    ///
    /// The one knob every engine ships, and it is content rather than physics: a
    /// balloon rises, a boss's hammer falls harder than it should because that
    /// is what makes it feel heavy, and a projectile arcs on a curve an artist
    /// chose. 1 is normal, 0 is weightless, negative floats.
    ///
    /// It multiplies the world gravity and nothing else, so it does not change
    /// how a thruster or a contact behaves — which is what distinguishes it from
    /// the mistake of editing the body's mass to get the same visual result.
    float gravity_scale = 1.0f;

    /// Dynamic, kinematic or fixed. See `body_kind` — it answers a different
    /// question from `inv_mass` and both have to be right.
    body_kind kind = body_kind::dynamic;
};

/// A stable reference to a body in a `body_world`. Four bytes, 5.4's split.
using body_id = handle<rigid_body>;

// ---------------------------------------------------------------------------
// Mass
// ---------------------------------------------------------------------------

/// The body's mass in kilograms — `1/inv_mass`, with infinity for immovable.
///
/// Returns `HUGE_VALF` when `inv_mass` is 0, which is the true answer and is
/// also why you should reach for `inv_mass` in any arithmetic. This function is
/// for debug UI and for the rare piece of gameplay that genuinely wants
/// kilograms.
[[nodiscard]] float mass_of(const rigid_body& b);

/// Set the mass in kilograms, refusing the two inputs that are not masses.
///
/// A mass of 0 or less is rejected and the body is left alone — a negative mass
/// accelerates *towards* a push and would break the solver's assumptions from
/// 8.9 onward in ways that are very hard to trace back here, and a mass of zero
/// is the infinite-acceleration case the `inv_mass` doc comment describes. Pass
/// `HUGE_VALF` to make a body immovable, or set `inv_mass = 0` directly, which
/// is what `make_fixed` does.
///
/// @return true if the mass was applied.
bool set_mass(rigid_body& b, float kilograms);

// ---------------------------------------------------------------------------
// Applying things
// ---------------------------------------------------------------------------

/// Add a force, in newtons, for the duration of the next step.
///
/// **Accumulates.** Call it from as many systems as you like, in any order; they
/// sum, because Newton's second law is linear in F. §4 measures the one way that
/// sentence is not quite true in floating point, and what it costs.
///
/// No application point, so no torque: this body cannot spin (8.3).
void add_force(rigid_body& b, vec3 newtons);

/// Add an impulse, in newton-seconds, applied **immediately** to the velocity.
///
/// An impulse is a force integrated over a time — `J = F*dt` — and the reason it
/// is a separate entry point is that the time in question is shorter than a step
/// and nobody knows what it is. A bat hitting a ball, a bullet landing, a
/// character jumping: the interesting quantity is the total change in momentum,
/// `Δv = J/m`, and it does not depend on `h` at all.
///
/// **THIS IS THE FRAME-RATE BUG YOU WILL ACTUALLY HIT.** Write a jump as
/// `add_force(b, up * 500.0f)` for one step and the resulting velocity is
/// `500*h/m`, so the character jumps twice as high at 30 Hz as at 60 — §6
/// measures four rates and four jump heights. Write it as
/// `add_impulse(b, up * 8.0f)` and every rate gives the same jump, because there
/// is no `h` in the arithmetic.
///
/// Every contact response in 8.9 and every constraint in 8.10 is an impulse, for
/// exactly this reason: a collision does not last a step, and pretending it does
/// makes the bounce depend on the frame rate.
void add_impulse(rigid_body& b, vec3 newton_seconds);

/// Add an acceleration directly, in m/s², bypassing the mass.
///
/// For fields rather than pushes. Gravity is the canonical one — it is a force
/// of `m*g`, so the mass cancels and every body accelerates at `g` regardless of
/// what it weighs — and a magnetic sweep, a vertical wind or a designer's
/// "everything in this volume drifts upward" are the same shape.
///
/// Implemented as `add_force(b, a / inv_mass)`, which is `m*a`, so an immovable
/// body is unaffected and the accumulator still sums. That round trip through
/// the mass looks pointless and is the thing that makes it correct: a field that
/// wrote the velocity directly would move the floor.
void add_acceleration(rigid_body& b, vec3 metres_per_second_squared);

/// Discard any accumulated force without stepping. Mostly for tests and for a
/// system that changed its mind.
void clear_force(rigid_body& b);

// ---------------------------------------------------------------------------
// Terminal speeds — the two that look the same and are not
// ---------------------------------------------------------------------------

/// Terminal speed under `rigid_body::damping`: `g/k`. **Independent of mass.**
///
/// Falling under gravity and velocity-space damping, the speed settles where
/// `g = k*v`. There is no mass in that equation because `damping` was defined
/// without one, so a paper cup and a piano damped identically fall at identical
/// terminal speeds. Read this function and the next one together.
[[nodiscard]] float terminal_speed_damped(float g, float k);

/// Terminal speed under a linear drag FORCE `F = -b*v`: `m*g/b`. **Proportional
/// to mass.**
///
/// Falling under gravity and a drag force, the speed settles where `m*g = b*v`.
/// This is the physical one, it is the reason a cannonball beats a feather, and
/// getting it costs one line at the call site:
///
///     add_force(b, body.state.velocity * -drag_coefficient);
///
/// @param b_coefficient Drag coefficient in N·s/m. Must be positive.
[[nodiscard]] float terminal_speed_dragged(float g, float mass, float b_coefficient);

// ---------------------------------------------------------------------------
// Units, and the scaling law
// ---------------------------------------------------------------------------

/// Time to fall `distance` metres from rest under acceleration `g`:
/// `sqrt(2d/g)`.
///
/// A function rather than a comment because §7 measures against it, and because
/// it is the cleanest possible statement of the thing the next function is
/// about: the time depends on the square root of a LENGTH.
[[nodiscard]] float free_fall_time(float distance, float g);

/// The factor every duration in a world is multiplied by when every length in it
/// is multiplied by `length_scale` and gravity is left alone: `sqrt(s)`.
///
/// **THIS IS WHY MINIATURES LOOK LIKE MINIATURES**, and it is the whole of §7.
/// A model ship at 1/8 scale sits in water that falls at the same 9.81 m/s² the
/// real one does, so every splash, every roll and every collapse happens
/// `sqrt(8) = 2.83` times too fast, and an audience reads that instantly without
/// being able to say why. Film crews fix it by overcranking the camera by
/// exactly this factor.
///
/// A game has the same problem in reverse and the symptom has a name. Model your
/// world twice as large as it depicts — a "1.8 m" character built 3.6 units tall
/// — and everything in it takes `sqrt(2) = 1.41` times too long to fall. That is
/// the complaint "the jumping feels floaty", and the fix is not a bigger `g`;
/// see §7.4 for what a bigger `g` does and does not repair.
[[nodiscard]] float time_scale_for_length_scale(float length_scale);

// ---------------------------------------------------------------------------
// Frames: what a parent would have done to you
// ---------------------------------------------------------------------------

/// What integrating a body in some other space would actually produce.
///
/// Nothing in `body_world` calls this. It exists so that the rule in the file
/// header — **bodies are world space, full stop** — is a measurable claim rather
/// than an assertion, and so that a tool can point at a scene node and say why
/// physics must not happen there.
struct frame_report
{
    /// Where a local-space `gravity` actually points once the frame has had its
    /// way with it, in world space.
    vec3 gravity_in_world{};

    /// `|gravity_in_world| / |gravity|`. 1 for any rigid frame — a rotation and
    /// a translation cannot change a length — and equal to the scale factor for
    /// a uniformly scaled one. **2.0 means the body falls at twice gravity.**
    float gain = 1.0f;

    /// The angle between where gravity was supposed to point and where it does,
    /// in degrees. Nonzero whenever the frame rotates, which is expected and
    /// fine for a rotation alone; the interesting case is the one where a
    /// NON-UNIFORM scale changes an angle that a rotation had already set.
    float tilt_degrees = 0.0f;

    /// The three column lengths of the frame's linear part — its scale.
    vec3 scale{1.0f, 1.0f, 1.0f};

    /// `transform_extraction::out_of_square` for this frame: the worst |cosine|
    /// between two of its axes, so 0 for anything square and positive for shear.
    ///
    /// **Lesson 7.5 built this number for a completely different reason** — to
    /// report what a `quat` could not hold when a `mat3` field was narrowed —
    /// and it is the exact diagnostic a physics frame needs, because a sheared
    /// frame is one that does not preserve angles, and a frame that does not
    /// preserve angles turns a sphere into an ellipsoid and a straight fall into
    /// a slanted one.
    float out_of_square = 0.0f;

    /// Is the scale the same on all three axes (within a tolerance)? A uniform
    /// scale is survivable — it changes `gain` and nothing else. A non-uniform
    /// one changes DIRECTIONS, and no amount of tuning recovers from that.
    bool uniform = true;

    /// Is this frame safe to integrate a body in? True only for a frame that is
    /// a rotation and a translation: `gain` 1, square, uniform.
    bool inertial = true;
};

/// Measure what `world_from_parent` would do to a body integrated in its space.
///
/// @param gravity The acceleration as the body's own space would express it.
[[nodiscard]] frame_report inspect_frame(const mat4& world_from_parent, vec3 gravity);

/// Convert a body's world position into a local `transform`, for a scene node
/// whose parent's world matrix is `world_from_parent`.
///
/// The bridge, and the only one: the body owns world space, the scene owns local
/// space, and this converts at the boundary once per frame rather than letting
/// either one leak into the other. `authored` supplies the rotation and scale,
/// which physics does not own yet (8.3 takes the rotation).
[[nodiscard]] transform place_in_parent(const rigid_body& b, const mat4& world_from_parent,
                                        const transform& authored);

// ---------------------------------------------------------------------------
// The world
// ---------------------------------------------------------------------------

/// What one call to `body_world::step` did.
///
/// Every field here is something a later lesson in this module needs, and they
/// are cheap because the step is already walking every body.
struct step_report
{
    /// How many bodies of each kind the walk saw.
    int bodies = 0;
    int dynamic = 0;
    int kinematic = 0;
    int fixed = 0;

    /// The fastest body's speed at the END of the step, in m/s.
    float max_speed = 0.0f;

    /// **How far the fastest body moved during this step, in metres.**
    ///
    /// The tunnelling number. A body that travels further in one step than the
    /// thickness of the wall it is heading for passes through it without any
    /// test ever seeing an overlap, because the tests in 8.4 onward are all
    /// asked about a body's position and never about the segment between two of
    /// them. At 60 Hz a bullet at 400 m/s covers 6.67 m per step.
    ///
    /// Nothing here fixes it; reporting it is how you find out that you need to.
    /// Lesson 1.8's swept test is the shape of the answer, and 8.13's character
    /// controller is where it stops being optional.
    float max_travel = 0.0f;

    /// Total linear momentum of every dynamic body, `sum(m*v)`, in kg·m/s.
    ///
    /// **The conservation law this module is eventually judged by.** Gravity and
    /// thrusters change it — they are external forces, and that is what an
    /// external force IS — but a collision between two bodies must not, to the
    /// last bit the arithmetic allows. 8.9 checks its impulse response against
    /// this field; measuring it now costs one multiply-add per body and means
    /// the instrument predates the thing it measures.
    ///
    /// Immovable bodies are excluded, because their momentum is either zero or
    /// infinite and neither is a useful contribution to a sum.
    vec3 momentum{};
};

/// The table of bodies, and the thing that steps them.
///
/// **A `pool<rigid_body>` and eight lines of policy**, which is deliberate:
/// Lesson 5.4 built generational handles precisely so that subsystems would not
/// each invent their own storage, and a body table is the textbook case for it —
/// bodies are created and destroyed constantly, everything else in the engine
/// wants to refer to them, and the solver in 8.10 wants them in a flat array it
/// can walk without chasing a pointer.
///
/// What the pool gives for free and is worth naming: bodies live in a **dense**
/// array with no holes, so `bodies()` is a contiguous span and `step` is a linear
/// walk; a removal swaps the last body into the hole, so a stale `body_id` fails
/// its generation check instead of silently naming whichever body moved in; and
/// the whole table serialises as integers, which Module 9's scene format needs.
class body_world
{
public:
    /// Add a body. The `body_id` is stable across every later insertion and
    /// removal.
    body_id add(const rigid_body& body);

    /// Remove a body. Returns false if the handle was already stale.
    bool remove(body_id id);

    /// Resolve a handle, or null if it is stale. See `handle::valid` for why
    /// this is a different question from "is the handle non-null".
    [[nodiscard]] rigid_body* get(body_id id);
    [[nodiscard]] const rigid_body* get(body_id id) const;

    /// Every body, densely packed, in no particular order.
    ///
    /// **THE ORDER IS NOT STABLE** — a removal swaps the last element into the
    /// gap — and code that iterates this span must not depend on it. That is not
    /// a defect to work around; it is what keeps the walk contiguous, and 5.4
    /// §6 measured what the alternative costs.
    [[nodiscard]] std::span<rigid_body> bodies();
    [[nodiscard]] std::span<const rigid_body> bodies() const;

    [[nodiscard]] std::size_t size() const;
    void clear();

    /// The world's gravity, applied to every dynamic body scaled by its
    /// `gravity_scale`. Defaults to `k_gravity_down`.
    void set_gravity(vec3 g);
    [[nodiscard]] vec3 gravity() const;

    /// The integration rule, for every body. Defaults to semi-implicit Euler,
    /// which Lesson 8.1 settled and this lesson does not relitigate.
    void set_integrator(integrator rule);
    [[nodiscard]] integrator integrator_rule() const;

    /// Advance every body by `h` seconds.
    ///
    /// **THE ORDER OF OPERATIONS IS THE LESSON**, and it is four steps:
    ///
    ///   1. `a = F * inv_mass` — one multiply, once, after every system has
    ///      added what it had to say, which is the entire reason the accumulator
    ///      exists;
    ///   2. `a += gravity * gravity_scale`, AFTER the division rather than
    ///      before it as a weight of `m*g`. Both are correct physics and only
    ///      one of them needs the mass: §10.1 measures the divide that the other
    ///      arrangement costs at 15% of the whole update. **The consequence is
    ///      that `force` does not contain the body's weight** — see §9, and use
    ///      `mass_of(b) * world.gravity()` if a tool needs to draw it;
    ///   3. the body is integrated by the chosen rule, then damped by 8.1's
    ///      exact `exp(-k*h)`;
    ///   4. the force is cleared, LAST, so that anything wanting to look at what
    ///      was applied this step still can.
    ///
    /// Kinematic bodies skip 1, 2 and the velocity half of 3: they travel at
    /// whatever velocity you set and nothing else touches them. Fixed bodies are
    /// skipped entirely.
    const step_report& step(float h);

    /// What the last `step` saw.
    [[nodiscard]] const step_report& report() const;

private:
    pool<rigid_body> bodies_;
    vec3 gravity_ = k_gravity_down;
    integrator rule_ = integrator::semi_implicit_euler;
    step_report report_{};
};

// ---------------------------------------------------------------------------
// Small conveniences
// ---------------------------------------------------------------------------

/// A dynamic body of `mass` kilograms at `position`, at rest.
[[nodiscard]] rigid_body make_dynamic(vec3 position, float mass);

/// An immovable body at `position`: `inv_mass` 0, kind `fixed`.
[[nodiscard]] rigid_body make_fixed(vec3 position);

/// A kinematic body at `position` moving at `velocity`: immovable by forces,
/// moved by the step, unaffected by gravity.
[[nodiscard]] rigid_body make_kinematic(vec3 position, vec3 velocity);

} // namespace engine::phys
