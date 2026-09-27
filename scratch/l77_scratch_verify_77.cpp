// scratch/verify_77.cpp — every number Lesson 7.7 prints, measured rather than
// asserted.
//
// Build and run:  sh scratch/build_verify_77.sh
//
// Nine sections, in the lesson's order:
//
//   A  two bracket searches, one answer — and what a stale cursor costs
//   B  wrap_time, and the two different wrong things fmod does
//   C  the scale decision: lerp against geometric, with the ratios attached
//   D  nlerp at the arcs a clip actually contains
//   E  the loop seam, and the duration that is one frame short
//   F  the double cover inside a track, and what canonicalising is worth
//   G  cross-fade: blending poses against blending matrices
//   H  reduction, and fitting against a curve you will not play
//   I  what a clip weighs, and what sampling one costs
//
// EVERY SECTION CARRIES A CONTROL, for the reason 7.1 states and 7.2 through 7.6
// repeat: a check whose degenerate case is a pass is not a check. 7.6 sharpened
// that into a question — ASK WHAT THE CONTROL WOULD SAY IF THE THING WERE
// COMPLETELY BROKEN — after its normal check ran at the bind pose, where every
// palette matrix is the identity and the wrong rule gives the right answer. Two
// controls here are written against that question specifically: A.3 corrupts the
// cursor array rather than merely not warming it (an unwarmed cursor of 0 is the
// right answer on frame 0), and G.4 blends at w = 0 and w = 1, where the two
// methods MUST agree, so that the disagreement in between cannot be an artefact
// of the instrument.
//
// THE FIXTURE IS A PLAUSIBLE HUMANOID — 23 joints, arms and legs and a spine —
// because this lesson's claims are about CONTENT and not about arithmetic.
// "34.8% of channels carry data" is a statement about how rigs are animated, and
// measuring it on 7.6's six-joint tube would have been measuring a tube. Section
// G goes the other way and uses a straight chain, because its claim IS about
// arithmetic and a straight chain is the shape where the prediction can be
// written down.
//
// OUTPUT WIDTH IS A CONSTRAINT, not a preference. The lesson quotes this
// program's transcript in <pre> blocks that scroll and never wrap, and the fold
// is at about 66 characters. Nothing below exceeds 66.

#include <engine/anim/clip.hpp>
#include <engine/anim/skeleton.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/transform.hpp>
#include <engine/math/vec3.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <string>
#include <vector>

using engine::mat4;
using engine::quat;
using engine::transform;
using engine::vec3;
using engine::anim::clip;
using engine::anim::clip_report;
using engine::anim::joint;
using engine::anim::joint_index;
using engine::anim::joint_track;
using engine::anim::k_no_parent;
using engine::anim::quat_key;
using engine::anim::reduction_limits;
using engine::anim::skeleton;
using engine::anim::track_cursor;
using engine::anim::vec3_key;

namespace {

constexpr float k_pi = std::numbers::pi_v<float>;

int checks_run = 0;
int checks_passed = 0;
double sink = 0.0;

float deg(float radians) { return radians * 180.0f / k_pi; }
float rad(float degrees) { return degrees * k_pi / 180.0f; }

void check(const char* name, bool ok)
{
    ++checks_run;
    if (ok) { ++checks_passed; }
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
}

// ---- The fixture: a 23-joint humanoid ---------------------------------------
//
// Parent-before-child by construction, which `anim::validate` then confirms
// rather than assumes. The offsets are roughly a 1.8-unit person with the origin
// between the feet, so the numbers in section I ("the root travels 1.2 units")
// are readable as metres.

struct joint_spec
{
    const char* name;
    int parent;
    vec3 offset;
};

const joint_spec k_rig[] = {
    {"root",        -1, {0.00f, 0.00f, 0.00f}},
    {"hips",         0, {0.00f, 0.95f, 0.00f}},
    {"spine1",       1, {0.00f, 0.12f, 0.00f}},
    {"spine2",       2, {0.00f, 0.14f, 0.00f}},
    {"chest",        3, {0.00f, 0.16f, 0.00f}},
    {"neck",         4, {0.00f, 0.18f, 0.00f}},
    {"head",         5, {0.00f, 0.10f, 0.00f}},
    {"clavicle.L",   4, {0.05f, 0.14f, 0.00f}},
    {"upperarm.L",   7, {0.13f, 0.00f, 0.00f}},
    {"forearm.L",    8, {0.28f, 0.00f, 0.00f}},
    {"hand.L",       9, {0.25f, 0.00f, 0.00f}},
    {"clavicle.R",   4, {-0.05f, 0.14f, 0.00f}},
    {"upperarm.R",  11, {-0.13f, 0.00f, 0.00f}},
    {"forearm.R",   12, {-0.28f, 0.00f, 0.00f}},
    {"hand.R",      13, {-0.25f, 0.00f, 0.00f}},
    {"thigh.L",      1, {0.09f, -0.05f, 0.00f}},
    {"shin.L",      15, {0.00f, -0.42f, 0.00f}},
    {"foot.L",      16, {0.00f, -0.41f, 0.00f}},
    {"toe.L",       17, {0.00f, -0.06f, 0.12f}},
    {"thigh.R",      1, {-0.09f, -0.05f, 0.00f}},
    {"shin.R",      19, {0.00f, -0.42f, 0.00f}},
    {"foot.R",      20, {0.00f, -0.41f, 0.00f}},
    {"toe.R",       21, {0.00f, -0.06f, 0.12f}},
};

constexpr std::size_t k_joints = sizeof(k_rig) / sizeof(k_rig[0]);

[[nodiscard]] skeleton build_rig()
{
    skeleton sk;
    sk.joints.resize(k_joints);
    for (std::size_t j = 0; j < k_joints; ++j)
    {
        joint& jt = sk.joints[j];
        jt.name = k_rig[j].name;
        jt.parent = (k_rig[j].parent < 0) ? k_no_parent
                                          : static_cast<joint_index>(k_rig[j].parent);
        jt.local_bind.position = k_rig[j].offset;
    }
    engine::anim::bake_inverse_binds(sk);
    return sk;
}

// ---- The motion the fixture is baked from -----------------------------------
//
// A walk-shaped function of phase, NOT a recording: every channel below has a
// closed form, so "the reduction lost 0.31 degrees" is measured against the thing
// the keys were made from rather than against a denser set of keys. Sinusoids at
// the cycle rate, opposite phases left and right, a vertical bob at twice the
// rate, and forward travel on the root — which is the only translation in the
// whole rig, and that is the point of section I.

[[nodiscard]] transform walk_pose(std::size_t j, float phase)
{
    const float w = 2.0f * k_pi * phase;
    transform t;
    t.position = k_rig[j].offset;

    auto swing = [&](float amp_deg, float bias_deg, float offset, vec3 axis) {
        return engine::quat_from_axis_angle(
            engine::normalised(axis), rad(bias_deg + amp_deg * std::sin(w + offset)));
    };

    switch (j)
    {
    case 0:    // root: forward travel and a bob at twice the step rate
        t.position = t.position + vec3{0.0f, 0.035f * std::sin(2.0f * w), 1.2f * phase};
        break;
    case 1: t.rotation = swing(3.0f, 0.0f, 0.0f, {0, 1, 0}); break;        // hips yaw
    case 2: t.rotation = swing(2.0f, 1.0f, k_pi, {1, 0, 0}); break;        // spine1
    case 3: t.rotation = swing(1.5f, 1.0f, k_pi, {1, 0, 0}); break;        // spine2
    case 4: t.rotation = swing(2.5f, 0.0f, 0.5f, {0, 1, 0}); break;        // chest
    case 5: t.rotation = swing(1.0f, 0.0f, 0.0f, {0, 1, 0}); break;        // neck
    case 8:  t.rotation = swing(22.0f, 0.0f, k_pi, {1, 0, 0}); break;      // upperarm.L
    case 9:  t.rotation = swing(14.0f, -18.0f, k_pi * 0.5f, {1, 0, 0}); break;  // forearm.L
    case 12: t.rotation = swing(22.0f, 0.0f, 0.0f, {1, 0, 0}); break;      // upperarm.R
    case 13: t.rotation = swing(14.0f, -18.0f, -k_pi * 0.5f, {1, 0, 0}); break; // forearm.R
    case 15: t.rotation = swing(28.0f, 0.0f, 0.0f, {1, 0, 0}); break;      // thigh.L
    case 16: t.rotation = swing(24.0f, -24.0f, -1.1f, {1, 0, 0}); break;   // shin.L
    case 17: t.rotation = swing(16.0f, 0.0f, 2.0f, {1, 0, 0}); break;      // foot.L
    case 19: t.rotation = swing(28.0f, 0.0f, k_pi, {1, 0, 0}); break;      // thigh.R
    case 20: t.rotation = swing(24.0f, -24.0f, k_pi - 1.1f, {1, 0, 0}); break;  // shin.R
    case 21: t.rotation = swing(16.0f, 0.0f, k_pi + 2.0f, {1, 0, 0}); break;    // foot.R
    default: break;   // head, clavicles, hands, toes: they do not move
    }
    return t;
}

/// Bake the walk into a clip at `hz`, writing EVERY channel of EVERY joint —
/// including the ones that never change.
///
/// **That is the naive recorder, on purpose.** Section I's storage table needs a
/// before as well as an after, and `reduce` is what turns one into the other. A
/// baker that skipped the constant channels would have hidden the measurement
/// inside the fixture.
[[nodiscard]] clip bake_walk(float duration, float hz, bool close_the_loop)
{
    clip c;
    c.name = "walk";
    c.duration = duration;
    c.loops = true;
    c.tracks.resize(k_joints);

    const int frames = static_cast<int>(std::lround(duration * hz));
    const int count = close_the_loop ? frames + 1 : frames;

    for (int f = 0; f < count; ++f)
    {
        const float t = static_cast<float>(f) / hz;
        const float phase = t / duration;
        for (std::size_t j = 0; j < k_joints; ++j)
        {
            const transform p = walk_pose(j, phase);
            c.tracks[j].position.push_back({t, p.position});
            c.tracks[j].rotation.push_back({t, p.rotation});
            c.tracks[j].scale.push_back({t, p.scale});
        }
    }
    return c;
}

// ---- The three bracket strategies, written out ------------------------------
//
// `engine_walk` is what `anim/clip.cpp` does today, `walk_two_loops` is what it
// did before section A was measured, and `harness_search` is the search both fall
// back to. All three must return the same index for the same input, and A.1
// checks the engine's two public routes against each other on real poses; these
// exist so that A.4 can time all three side by side, including the retired one.

template <typename Key>
[[nodiscard]] std::uint32_t harness_search(const std::vector<Key>& keys, float t)
{
    std::size_t lo = 0;
    std::size_t hi = keys.size() - 2u;
    while (lo < hi)
    {
        const std::size_t mid = lo + (hi - lo + 1u) / 2u;
        if (keys[mid].time <= t) { lo = mid; } else { hi = mid - 1u; }
    }
    return static_cast<std::uint32_t>(lo);
}

/// The retired version: walk forward, then walk backward, both unbounded.
template <typename Key>
[[nodiscard]] std::uint32_t walk_two_loops(const std::vector<Key>& keys, float t,
                                           std::uint32_t cursor)
{
    const auto last = static_cast<std::uint32_t>(keys.size() - 2u);
    if (cursor > last) { cursor = last; }
    while (cursor < last && keys[cursor + 1u].time <= t) { ++cursor; }
    while (cursor > 0u && keys[cursor].time > t) { --cursor; }
    return cursor;
}

/// What the engine does now: a bounded forward walk over a binary search.
template <typename Key>
[[nodiscard]] std::uint32_t engine_walk(const std::vector<Key>& keys, float t,
                                        std::uint32_t cursor)
{
    const auto last = static_cast<std::uint32_t>(keys.size() - 2u);
    if (cursor > last) { cursor = last; }
    if (keys[cursor].time > t) { return harness_search(keys, t); }
    for (std::uint32_t step = 0; step < 4u; ++step)
    {
        if (cursor >= last || keys[cursor + 1u].time > t) { return cursor; }
        ++cursor;
    }
    return (cursor < last && keys[cursor + 1u].time <= t) ? harness_search(keys, t) : cursor;
}

// ---- Small instruments ------------------------------------------------------

[[nodiscard]] float pose_gap_position(const std::vector<transform>& a,
                                      const std::vector<transform>& b)
{
    float worst = 0.0f;
    for (std::size_t j = 0; j < a.size() && j < b.size(); ++j)
    {
        worst = std::fmax(worst, engine::length(a[j].position - b[j].position));
    }
    return worst;
}

[[nodiscard]] float pose_gap_radians(const std::vector<transform>& a,
                                     const std::vector<transform>& b)
{
    float worst = 0.0f;
    for (std::size_t j = 0; j < a.size() && j < b.size(); ++j)
    {
        worst = std::fmax(worst, engine::angle_between(a[j].rotation, b[j].rotation));
    }
    return worst;
}

[[nodiscard]] bool pose_identical(const std::vector<transform>& a,
                                  const std::vector<transform>& b)
{
    if (a.size() != b.size()) { return false; }
    for (std::size_t j = 0; j < a.size(); ++j)
    {
        if (a[j].position.x != b[j].position.x || a[j].position.y != b[j].position.y
            || a[j].position.z != b[j].position.z) { return false; }
        if (a[j].rotation != b[j].rotation) { return false; }
        if (a[j].scale.x != b[j].scale.x || a[j].scale.y != b[j].scale.y
            || a[j].scale.z != b[j].scale.z) { return false; }
    }
    return true;
}

/// The reference the whole harness checks reductions against: the closed form the
/// keys were baked from, sampled densely.
void true_pose(float phase, std::vector<transform>& out)
{
    out.resize(k_joints);
    for (std::size_t j = 0; j < k_joints; ++j) { out[j] = walk_pose(j, phase); }
}

double now_seconds()
{
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

/// Microseconds per repetition, best of three.
///
/// **Best of three and not the mean**, which is the convention Lesson 7.5's
/// benchmark fixed and the right one for this question: the noise on a timing is
/// one-sided (a scheduler interruption can only make a run slower), so the
/// minimum is the closest thing to the machine's own answer. The first run also
/// pays for cold caches and a cold branch predictor, and here that was not a
/// rounding error — the first version of section I ran each loop once and
/// reported the 301-key clip as FASTER than the 31-key one.
template <typename Body>
double best_of_three(int reps, Body body)
{
    double best = 1.0e9;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        const double t0 = now_seconds();
        for (int r = 0; r < reps; ++r) { body(r); }
        const double t1 = now_seconds();
        best = std::fmin(best, (t1 - t0) * 1.0e6 / static_cast<double>(reps));
    }
    return best;
}

}   // namespace

// ============================================================================
// A. Two bracket searches, one answer
// ============================================================================

void section_a(const skeleton& sk, const clip& c)
{
    std::printf("\nA. Two searches for the same interval\n");

    std::vector<track_cursor> cursors;
    std::vector<transform> by_cursor;
    std::vector<transform> by_search;

    // A whole playback, several loops long, at a rate nothing divides evenly —
    // so the times land inside intervals rather than on keys, which is where a
    // bracket can be off by one without anybody noticing.
    const int steps = 977;
    float worst_gap = 0.0f;
    bool identical = true;
    for (int s = 0; s < steps; ++s)
    {
        const float t = static_cast<float>(s) * 0.00713f;
        engine::anim::sample(c, sk, t, cursors, by_cursor);
        engine::anim::sample_at(c, sk, t, by_search);
        if (!pose_identical(by_cursor, by_search)) { identical = false; }
        worst_gap = std::fmax(worst_gap, pose_gap_position(by_cursor, by_search));
    }
    std::printf("   %d samples over %.2f s of a %.2f s clip\n", steps,
                static_cast<double>(static_cast<float>(steps) * 0.00713f),
                static_cast<double>(c.duration));
    std::printf("   worst position gap between the two routes %.3e\n",
                static_cast<double>(worst_gap));
    check("A.1  cursor and binary search agree to the bit", identical);

    // A.2 — the cursor is not merely warm, it is CORRECT after a backward jump.
    // Play forward to the end, then sample the start again without resetting.
    engine::anim::sample(c, sk, 0.97f, cursors, by_cursor);
    engine::anim::sample(c, sk, 0.03f, cursors, by_cursor);
    engine::anim::sample_at(c, sk, 0.03f, by_search);
    check("A.2  and after a backward jump, still to the bit",
          pose_identical(by_cursor, by_search));

    // CONTROL — every cursor jammed at the last interval, which is the worst
    // possible lie: not "unwarmed" (0 is the right answer on frame 0) but
    // actively wrong in the direction the forward walk cannot fix.
    std::vector<track_cursor> corrupt(c.tracks.size(), track_cursor{9999u, 9999u, 9999u});
    bool control_ok = true;
    for (int s = 0; s < 97; ++s)
    {
        const float t = static_cast<float>(s) * 0.00713f;
        engine::anim::sample(c, sk, t, corrupt, by_cursor);
        engine::anim::sample_at(c, sk, t, by_search);
        if (!pose_identical(by_cursor, by_search)) { control_ok = false; }
    }
    std::printf("   CONTROL cursors jammed at 9999: same poses %s\n",
                control_ok ? "yes" : "NO");
    check("A.3  CONTROL: a stale cursor costs time, not truth", control_ok);

    // ---- The three strategies, on one key array ---------------------------
    //
    // **THE DRIVER ADVANCES EXACTLY ONE KEY INTERVAL PER LOOKUP**, which is the
    // only way the two track lengths below differ in nothing but their length.
    // The first version of this measurement varied the wrap period by changing
    // the step size, so each lookup on the "more wraps" row also crossed ten
    // intervals instead of one — it was measuring two things at once and
    // attributed both to the wrap.
    //
    // `walk_two_loops` is the RETIRED implementation, kept here for the same
    // reason `angle_between_by_cosine` is kept in `quat.hpp`: so that the
    // comparison is against the real previous thing rather than against a
    // description of it.
    std::printf("   one lookup per key interval, best of three:\n");
    std::printf("     keys   two walks   walk+search   binary\n");
    float short_ratio = 0.0f;
    float long_ratio = 0.0f;
    for (int keys : {31, 301})
    {
        std::vector<quat_key> track;
        for (int i = 0; i < keys; ++i)
        {
            track.push_back({static_cast<float>(i) / static_cast<float>(keys - 1),
                             engine::quat::identity()});
        }
        const int intervals = keys - 1;
        const int reps = 2000000;

        std::uint32_t c = 0;
        const double two = best_of_three(reps, [&](int r) {
            c = walk_two_loops(track, static_cast<float>(r % intervals)
                                          / static_cast<float>(intervals), c);
            sink += static_cast<double>(c);
        }) * 1000.0;
        c = 0;
        const double mixed = best_of_three(reps, [&](int r) {
            c = engine_walk(track, static_cast<float>(r % intervals)
                                       / static_cast<float>(intervals), c);
            sink += static_cast<double>(c);
        }) * 1000.0;
        const double binary = best_of_three(reps, [&](int r) {
            sink += static_cast<double>(harness_search(
                track, static_cast<float>(r % intervals) / static_cast<float>(intervals)));
        }) * 1000.0;

        (keys == 31 ? short_ratio : long_ratio) =
            static_cast<float>(binary / mixed);
        std::printf("   %6d   %7.3f ns   %7.3f ns  %7.3f ns\n", keys, two, mixed, binary);
    }
    std::printf("   the cursor beats the search by %.2fx and %.2fx\n",
                static_cast<double>(short_ratio), static_cast<double>(long_ratio));
    check("A.4  a cursor beats a search, and by more on a long track",
          short_ratio > 1.2f && long_ratio > short_ratio);
}

// ============================================================================
// B. wrap_time, and what fmod does instead
// ============================================================================

void section_b(const skeleton& sk, const clip& c)
{
    std::printf("\nB. Wrapping a time onto a clip\n");
    std::printf("      t     wrap_time     fmod    differ\n");

    const float probe[] = {-0.100f, -0.001f, 0.000f, 0.500f,
                           1.000f, 1.250f, 2.750f, -3.400f};
    int differ = 0;
    for (float t : probe)
    {
        const float w = engine::anim::wrap_time(c, t);
        const float f = std::fmod(t, c.duration);
        const bool d = std::fabs(w - f) > 1e-6f;
        differ += d ? 1 : 0;
        std::printf("   %7.3f    %8.4f  %8.4f    %s\n", static_cast<double>(t),
                    static_cast<double>(w), static_cast<double>(f), d ? "yes" : "");
    }
    check("B.1  fmod and wrap_time disagree below zero", differ == 3);

    // What the disagreement COSTS, in pose. Two wrongnesses, and which one you
    // get is decided by the clamp inside segment_t.
    std::vector<transform> correct;
    std::vector<transform> frozen;
    std::vector<transform> first_key;
    engine::anim::sample_at(c, sk, -0.100f, correct);          // wrapped: 0.9 s in
    engine::anim::sample_at(c, sk, 0.000f, first_key);
    {
        // The fmod route, reproduced here: hand the sampler a negative time. The
        // engine clamps the segment parameter, so this must equal the first key.
        clip open = c;
        open.loops = false;      // clamp instead of wrap, which is what fmod left
        engine::anim::sample_at(open, sk, -0.100f, frozen);
    }
    const float freeze_gap = pose_gap_radians(correct, frozen);
    std::printf("   the clamped (frozen) pose vs the right one  %6.3f deg\n",
                static_cast<double>(deg(freeze_gap)));
    std::printf("   the frozen pose vs the clip's first key     %6.3f deg\n",
                static_cast<double>(deg(pose_gap_radians(frozen, first_key))));
    check("B.2  a negative time freezes on the first pose",
          pose_gap_radians(frozen, first_key) < 1e-5f && freeze_gap > rad(1.0f));

    // And the OTHER wrongness: the same negative time with the clamp removed.
    // Computed here by hand, since the engine will not do it.
    //
    // THE TWO FAILURES POINT IN OPPOSITE DIRECTIONS, which is the finding. The
    // clamp's error is LOUD and BOUNDED — a visible freeze, never worse than the
    // clip's own range. The extrapolation's error is QUIET and UNBOUNDED: at a
    // tenth of a second back it is under a degree, which is why nobody catches
    // it, and it keeps going.
    {
        const joint_track& track = c.tracks[15];               // thigh.L
        const quat_key& k0 = track.rotation[0];
        const quat_key& k1 = track.rotation[1];
        const float span = k1.time - k0.time;
        std::printf("   CONTROL the same times with NO clamp on u:\n");
        std::printf("        t       u    extrapolated   frozen\n");
        float furthest = 0.0f;
        for (float t : {-0.033f, -0.100f, -0.300f, -0.600f})
        {
            const float u = (t - k0.time) / span;              // NOT clamped
            const quat guessed = engine::quat_nlerp(k0.value, k1.value, u);
            engine::anim::sample_at(c, sk, t, correct);
            const float extrap = deg(engine::angle_between(guessed, correct[15].rotation));
            const float freeze = deg(engine::angle_between(first_key[15].rotation,
                                                           correct[15].rotation));
            furthest = std::fmax(furthest, extrap);
            std::printf("   %7.3f  %6.2f    %8.3f deg  %6.3f deg\n",
                        static_cast<double>(t), static_cast<double>(u),
                        static_cast<double>(extrap), static_cast<double>(freeze));
        }
        check("B.3  CONTROL: unclamped, it extrapolates and keeps going",
              furthest > 5.0f);
    }
}

// ============================================================================
// C. The scale decision
// ============================================================================

void section_c()
{
    std::printf("\nC. Scale: linear against geometric\n");
    std::printf("    a -> b    lerp mid   geom mid    gap\n");

    const float pairs[][2] = {{1.0f, 1.0f}, {1.0f, 1.05f}, {1.0f, 1.25f},
                              {1.0f, 2.0f}, {1.0f, 4.0f}, {1.0f, 8.0f}};
    float worst_small = 0.0f;
    for (const auto& p : pairs)
    {
        const float lerp_mid = 0.5f * (p[0] + p[1]);
        const float geom_mid = std::sqrt(p[0] * p[1]);
        const float gap = 100.0f * (lerp_mid - geom_mid) / geom_mid;
        if (p[1] <= 1.25f) { worst_small = std::fmax(worst_small, gap); }
        std::printf("   %4.2f -> %4.2f   %8.5f   %8.5f  %6.3f%%\n",
                    static_cast<double>(p[0]), static_cast<double>(p[1]),
                    static_cast<double>(lerp_mid), static_cast<double>(geom_mid),
                    static_cast<double>(gap));
    }
    std::printf("   worst over the squash-and-stretch range   %6.3f%%\n",
                static_cast<double>(worst_small));
    check("C.1  under 1.25x the two rules are within 1%", worst_small < 1.0f);

    // Splitting the growth into keys is the animator's own fix, and it converges
    // fast — because the gap is the arithmetic-minus-geometric mean of the
    // per-segment ratio, and that ratio is the n-th root.
    std::printf("   1 -> 8 split into n keys:\n");
    std::printf("     n   per-step ratio    gap\n");
    for (int n : {1, 2, 3, 6, 12})
    {
        const float ratio = std::pow(8.0f, 1.0f / static_cast<float>(n));
        const float gap = 100.0f * (0.5f * (1.0f + ratio) - std::sqrt(ratio))
                          / std::sqrt(ratio);
        std::printf("   %5d   %12.5f  %6.3f%%\n", n, static_cast<double>(ratio),
                    static_cast<double>(gap));
    }
    check("C.2  six keys bring a 59% gap under 2%",
          100.0f * (0.5f * (1.0f + std::pow(8.0f, 1.0f / 6.0f))
                    - std::sqrt(std::pow(8.0f, 1.0f / 6.0f)))
              / std::sqrt(std::pow(8.0f, 1.0f / 6.0f)) < 2.0f);

    // The blocker. Geometric interpolation toward zero does not shrink — it
    // teleports on the first frame after t = 0 and stays there.
    std::printf("   geometric 1 -> 0, by t:\n");
    std::printf("       t    a*(b/a)^t     lerp\n");
    bool teleports = true;
    for (float t : {0.0f, 0.01f, 0.25f, 0.50f, 0.75f, 1.0f})
    {
        const float geom = std::pow(0.0f, t);           // 1 at t = 0, else 0
        const float lin = 1.0f - t;
        if (t > 0.0f && geom != 0.0f) { teleports = false; }
        std::printf("   %5.2f   %10.6f  %8.4f\n", static_cast<double>(t),
                    static_cast<double>(geom), static_cast<double>(lin));
    }
    check("C.3  a geometric shrink to zero is a teleport", teleports);

    std::printf("   CONTROL ln(-1) = %s, ln(0) = %.1f\n",
                std::isnan(std::log(-1.0f)) ? "NaN" : "finite",
                static_cast<double>(std::log(0.0f)));
    check("C.4  CONTROL: mirrored and hidden have no logarithm",
          std::isnan(std::log(-1.0f)) && std::isinf(std::log(0.0f)));
}

// ============================================================================
// D. nlerp at the arcs a clip contains
// ============================================================================

void section_d()
{
    std::printf("\nD. How far apart two adjacent keys really are\n");
    std::printf("   (every angle below is a ROTATION angle. The sphere\n");
    std::printf("    arc a quaternion walks is HALF of it - 7.4's factor\n");
    std::printf("    of two, still charging rent.)\n");
    std::printf("   rate deg/s   at 30 Hz   nlerp err   slerp\n");

    const vec3 axis = engine::normalised(vec3{0.31f, 0.86f, -0.41f});
    float worst_clip_rate = 0.0f;
    for (float rate : {90.0f, 180.0f, 360.0f, 720.0f})
    {
        const float arc = rate / 30.0f;              // degrees between two keys
        const quat a = engine::quat::identity();
        const quat b = engine::quat_from_axis_angle(axis, rad(arc));

        float worst = 0.0f;
        for (int s = 0; s <= 256; ++s)
        {
            const float t = static_cast<float>(s) / 256.0f;
            worst = std::fmax(worst, engine::angle_between(engine::quat_nlerp(a, b, t),
                                                           engine::quat_slerp(a, b, t)));
        }
        if (rate <= 720.0f) { worst_clip_rate = std::fmax(worst_clip_rate, deg(worst)); }
        std::printf("   %10.0f   %7.2f   %9.6f   exact\n", static_cast<double>(rate),
                    static_cast<double>(arc), static_cast<double>(deg(worst)));
    }
    std::printf("   worst over every rate above             %8.6f deg\n",
                static_cast<double>(worst_clip_rate));
    check("D.1  inside a clip nlerp is within 0.1 deg of slerp",
          worst_clip_rate < 0.1f);

    // CONTROL — the same instrument on the arcs a CROSS-FADE can reach, where
    // the same function is not fine at all. 7.5's threshold restated from the
    // other side.
    std::printf("   CONTROL the same measurement at blend arcs:\n");
    std::printf("      arc deg   nlerp err\n");
    float worst_blend = 0.0f;
    for (float arc : {30.0f, 73.5f, 90.0f, 150.0f})
    {
        const quat a = engine::quat::identity();
        const quat b = engine::quat_from_axis_angle(axis, rad(arc));
        float worst = 0.0f;
        for (int s = 0; s <= 256; ++s)
        {
            const float t = static_cast<float>(s) / 256.0f;
            worst = std::fmax(worst, engine::angle_between(engine::quat_nlerp(a, b, t),
                                                           engine::quat_slerp(a, b, t)));
        }
        worst_blend = std::fmax(worst_blend, deg(worst));
        std::printf("   %10.1f   %9.6f\n", static_cast<double>(arc),
                    static_cast<double>(deg(worst)));
    }
    check("D.2  CONTROL: at blend arcs the same rule is not fine",
          worst_blend > 4.0f);
}

// ============================================================================
// E. The loop seam
// ============================================================================

void section_e(const skeleton& sk)
{
    std::printf("\nE. The loop seam, and the frame that is missing\n");

    const clip closed = bake_walk(1.0f, 30.0f, true);    // 31 keys, last at 1.000
    const clip open = bake_walk(1.0f, 30.0f, false);     // 30 keys, last at 0.967

    const clip_report closed_report = engine::anim::validate(closed, sk);
    const clip_report open_report = engine::anim::validate(open, sk);

    std::printf("   keys at 30 Hz over 1.00 s:\n");
    std::printf("     closed (last key AT 1.000)  %zu keys\n",
                closed.tracks[15].rotation.size());
    std::printf("     open   (last key at 0.967)  %zu keys\n",
                open.tracks[15].rotation.size());
    std::printf("   loop gap, closed   %8.4f deg  %.3e units\n",
                static_cast<double>(deg(closed_report.loop_gap_radians)),
                static_cast<double>(closed_report.loop_gap_position));
    std::printf("   loop gap, open     %8.4f deg  %.3e units\n",
                static_cast<double>(deg(open_report.loop_gap_radians)),
                static_cast<double>(open_report.loop_gap_position));
    check("E.1  a closed loop's seam is float noise",
          deg(closed_report.loop_gap_radians) < 1e-3f);
    check("E.2  one missing frame is a visible tick",
          deg(open_report.loop_gap_radians) > 1.0f);

    // ROOT MOTION IS NOT A SEAM, and the first version of this instrument said
    // it was. The root ends the cycle a stride in front of where it started —
    // that is the walk working — so it is reported separately.
    std::printf("   root travel over the cycle  %8.4f units\n",
                static_cast<double>(closed_report.root_travel));
    std::printf("   non-root position seam      %.3e units\n",
                static_cast<double>(closed_report.loop_gap_position));
    check("E.3  a perfect loop still moves its root a stride",
          closed_report.root_travel > 1.0f && closed_report.loop_gap_position < 1e-5f);

    // CONTROL — the keys are perfect and only the stated length is wrong. A
    // duration LONGER than the keys cannot produce a seam (the sampler holds the
    // last key, which is the first one again); a duration SHORTER cuts the cycle
    // off in the middle, which is the case that bites.
    clip cut = closed;
    cut.duration = 0.5f;
    const clip_report cut_report = engine::anim::validate(cut, sk);
    clip stretched = closed;
    stretched.duration = 1.5f;
    const clip_report stretched_report = engine::anim::validate(stretched, sk);
    std::printf("   CONTROL duration on a 1.0 s clip:\n");
    std::printf("     1.5 -> gap %7.4f deg, %zu keys out of range\n",
                static_cast<double>(deg(stretched_report.loop_gap_radians)),
                stretched_report.out_of_range);
    std::printf("     0.5 -> gap %7.4f deg, %zu keys out of range\n",
                static_cast<double>(deg(cut_report.loop_gap_radians)),
                cut_report.out_of_range);
    check("E.4  CONTROL: short cuts the cycle, long only holds",
          deg(cut_report.loop_gap_radians) > 10.0f
              && deg(stretched_report.loop_gap_radians) < 1e-3f
              && cut_report.out_of_range > 0);
}

// ============================================================================
// F. The double cover inside a track
// ============================================================================

void section_f(const skeleton& sk, const clip& clean)
{
    std::printf("\nF. The double cover, inside one rotation track\n");

    // An exporter with no sign policy: every other key written as -q.
    clip flipped = clean;
    for (joint_track& track : flipped.tracks)
    {
        for (std::size_t k = 1; k < track.rotation.size(); k += 2)
        {
            track.rotation[k].value = -track.rotation[k].value;
        }
    }

    const clip_report clean_report = engine::anim::validate(clean, sk);
    const clip_report flipped_report = engine::anim::validate(flipped, sk);
    std::printf("   sign flips reported: clean %zu, flipped %zu\n",
                clean_report.sign_flips, flipped_report.sign_flips);
    check("F.1  validate sees an inconsistent export",
          clean_report.sign_flips == 0 && flipped_report.sign_flips > 0);

    // THE ENGINE DOES NOT CARE, and saying so is the honest half of this
    // section: `quat_nlerp` calls `nearest`, so the samples are identical.
    std::vector<transform> from_clean;
    std::vector<transform> from_flipped;
    float worst = 0.0f;
    for (int s = 0; s <= 400; ++s)
    {
        const float t = static_cast<float>(s) / 400.0f;
        engine::anim::sample_at(clean, sk, t, from_clean);
        engine::anim::sample_at(flipped, sk, t, from_flipped);
        worst = std::fmax(worst, pose_gap_radians(from_clean, from_flipped));
    }
    std::printf("   this engine's samples differ by  %.3e deg\n",
                static_cast<double>(deg(worst)));
    check("F.2  nearest inside nlerp already pays for it", deg(worst) < 1e-4f);

    // WHO DOES CARE: anything that lerps without `nearest` — a vertex shader
    // doing the blend, an exporter, a naive reimplementation. Written out here
    // rather than described.
    auto naive_lerp = [](quat a, quat b, float t) {
        return engine::normalised(quat{a.w + (b.w - a.w) * t, a.v + (b.v - a.v) * t});
    };
    const joint_track& track = flipped.tracks[15];
    float naive_worst = 0.0f;
    for (int s = 0; s < 400; ++s)
    {
        const float t = static_cast<float>(s) / 400.0f;
        const std::size_t i = std::min<std::size_t>(
            static_cast<std::size_t>(t * 30.0f), track.rotation.size() - 2u);
        const quat_key& k0 = track.rotation[i];
        const quat_key& k1 = track.rotation[i + 1u];
        const float u = (t - k0.time) / (k1.time - k0.time);
        const quat got = naive_lerp(k0.value, k1.value, u);
        engine::anim::sample_at(clean, sk, t, from_clean);
        naive_worst = std::fmax(naive_worst,
                                engine::angle_between(got, from_clean[15].rotation));
    }
    std::printf("   CONTROL a lerp without nearest, worst error\n");
    std::printf("     on thigh.L  %8.3f deg\n", static_cast<double>(deg(naive_worst)));
    std::printf("     across one flipped interval (t, error):\n");
    for (float t : {0.0333f, 0.0400f, 0.0500f, 0.0600f, 0.0666f})
    {
        const std::size_t i = std::min<std::size_t>(
            static_cast<std::size_t>(t * 30.0f), track.rotation.size() - 2u);
        const quat_key& k0 = track.rotation[i];
        const quat_key& k1 = track.rotation[i + 1u];
        const float u = (t - k0.time) / (k1.time - k0.time);
        engine::anim::sample_at(clean, sk, t, from_clean);
        std::printf("       %6.4f  %8.3f deg\n", static_cast<double>(t),
                    static_cast<double>(deg(engine::angle_between(
                        naive_lerp(k0.value, k1.value, u), from_clean[15].rotation))));
    }
    check("F.3  CONTROL: without nearest the joint spins", deg(naive_worst) > 90.0f);

    // And the fix, applied once at load.
    clip repaired = flipped;
    const std::size_t negated = engine::anim::canonicalise_rotations(repaired);
    const clip_report repaired_report = engine::anim::validate(repaired, sk);
    std::printf("   canonicalise negated %zu keys; flips now %zu\n",
                negated, repaired_report.sign_flips);
    check("F.4  one load-time pass, and the data is consistent",
          repaired_report.sign_flips == 0 && negated > 0);
    check("F.5  and running it again changes nothing",
          engine::anim::canonicalise_rotations(repaired) == 0);
}

// ============================================================================
// G. Cross-fade: poses against matrices
// ============================================================================

namespace {

/// A straight chain of `n` joints one unit apart along +y — 7.6's fixture, and
/// the shape where the prediction can be written down.
[[nodiscard]] skeleton straight_chain(int n)
{
    skeleton sk;
    sk.joints.resize(static_cast<std::size_t>(n));
    for (int j = 0; j < n; ++j)
    {
        joint& jt = sk.joints[static_cast<std::size_t>(j)];
        jt.parent = (j == 0) ? k_no_parent : static_cast<joint_index>(j - 1);
        jt.name = "j" + std::to_string(j);
        jt.local_bind.position = (j == 0) ? vec3{} : vec3{0.0f, 1.0f, 0.0f};
    }
    engine::anim::bake_inverse_binds(sk);
    return sk;
}

[[nodiscard]] std::vector<transform> curl_pose(const skeleton& sk, float per_joint)
{
    std::vector<transform> pose;
    engine::anim::rest_pose(sk, pose);
    for (std::size_t j = 1; j < pose.size(); ++j)
    {
        pose[j].rotation = engine::quat_from_axis_angle({0.0f, 0.0f, 1.0f}, per_joint);
    }
    return pose;
}

/// Blend two affine matrices entry by entry — **the wrong method, written out
/// here rather than in the engine**, because the engine should not offer it.
///
/// Sixteen independent lerps. It is exactly what "just blend the matrices"
/// means, it is what a renderer that only ever sees `model_from_joint` is tempted
/// to do, and it is linear blend skinning one level up: the linear part is the
/// average of two rotations, which is not a rotation.
[[nodiscard]] mat4 matrix_lerp(const mat4& a, const mat4& b, float t)
{
    auto mix = [&](engine::vec4 x, engine::vec4 y) {
        return engine::vec4{x.x + (y.x - x.x) * t, x.y + (y.y - x.y) * t,
                            x.z + (y.z - x.z) * t, x.w + (y.w - x.w) * t};
    };
    return mat4{mix(a.c0, b.c0), mix(a.c1, b.c1), mix(a.c2, b.c2), mix(a.c3, b.c3)};
}

/// Bone lengths of a composed pose, measured from the joint matrices.
[[nodiscard]] std::vector<float> bone_lengths(const std::vector<mat4>& model)
{
    std::vector<float> out;
    for (std::size_t j = 1; j < model.size(); ++j)
    {
        out.push_back(engine::length(engine::translation_of(model[j])
                                     - engine::translation_of(model[j - 1u])));
    }
    return out;
}

}   // namespace

void section_g()
{
    std::printf("\nG. Cross-fading a pose, and cross-fading a matrix\n");

    const int n = 6;
    const skeleton sk = straight_chain(n);

    std::printf("   per-joint   pose blend   matrix blend   cos(d/2)\n");
    std::printf("      gap deg   chain len      chain len    predicts\n");

    float worst_pose_error = 0.0f;
    float worst_matrix_prediction_gap = 0.0f;
    float headline_matrix_len = 0.0f;

    for (float gap_deg : {0.0f, 5.0f, 10.0f, 20.0f, 30.0f, 45.0f})
    {
        const std::vector<transform> a = curl_pose(sk, 0.0f);
        const std::vector<transform> b = curl_pose(sk, rad(gap_deg));

        // Route 1 — blend the POSES, then compose. `transform_blend` per joint.
        std::vector<transform> blended;
        engine::anim::blend_poses(a, b, 0.5f, blended);
        std::vector<mat4> posed_blend;
        engine::anim::compose_pose(sk, blended, posed_blend);
        const std::vector<float> pose_bones = bone_lengths(posed_blend);

        // Route 2 — compose BOTH, then blend the matrices. The cheap wrong one.
        std::vector<mat4> model_a;
        std::vector<mat4> model_b;
        engine::anim::compose_pose(sk, a, model_a);
        engine::anim::compose_pose(sk, b, model_b);
        std::vector<mat4> mixed(model_a.size());
        for (std::size_t j = 0; j < model_a.size(); ++j)
        {
            mixed[j] = matrix_lerp(model_a[j], model_b[j], 0.5f);
        }
        const std::vector<float> matrix_bones = bone_lengths(mixed);

        float pose_len = 0.0f;
        float matrix_len = 0.0f;
        for (std::size_t k = 0; k < pose_bones.size(); ++k)
        {
            pose_len += pose_bones[k];
            matrix_len += matrix_bones[k];
            worst_pose_error = std::fmax(worst_pose_error, std::fabs(pose_bones[k] - 1.0f));
        }

        // THE PREDICTION. Bone k runs between joints k-1 and k, so its direction
        // is joint k-1's y axis; the two poses turn that joint by (k-1)*gap, and
        // the midpoint of two unit vectors an angle apart is cos(half) long.
        float predicted = 0.0f;
        for (int k = 1; k < n; ++k)
        {
            predicted += std::cos(0.5f * static_cast<float>(k - 1) * rad(gap_deg));
        }
        worst_matrix_prediction_gap =
            std::fmax(worst_matrix_prediction_gap, std::fabs(matrix_len - predicted));
        if (gap_deg == 20.0f) { headline_matrix_len = matrix_len; }

        std::printf("   %10.1f   %9.5f      %9.5f   %9.5f\n",
                    static_cast<double>(gap_deg), static_cast<double>(pose_len),
                    static_cast<double>(matrix_len), static_cast<double>(predicted));
    }

    std::printf("   worst bone-length error, pose route  %.3e\n",
                static_cast<double>(worst_pose_error));
    check("G.1  a pose blend preserves every bone exactly",
          worst_pose_error < 1e-5f);
    std::printf("   worst gap from sum cos(k d / 2)      %.3e\n",
                static_cast<double>(worst_matrix_prediction_gap));
    check("G.2  a matrix blend shortens by the same cosine",
          worst_matrix_prediction_gap < 1e-4f);
    std::printf("   at 20 deg per joint the chain loses  %.4f of 5\n",
                static_cast<double>(5.0f - headline_matrix_len));
    check("G.3  and the loss is not small", 5.0f - headline_matrix_len > 0.2f);

    // CONTROL — at w = 0 and w = 1 no blending happens, so the two routes MUST
    // agree. If they did not, the disagreement above would be the instrument.
    const std::vector<transform> a = curl_pose(sk, 0.0f);
    const std::vector<transform> b = curl_pose(sk, rad(20.0f));
    float control_worst = 0.0f;
    for (float w : {0.0f, 1.0f})
    {
        std::vector<transform> blended;
        engine::anim::blend_poses(a, b, w, blended);
        std::vector<mat4> posed_blend;
        std::vector<mat4> model_a;
        std::vector<mat4> model_b;
        engine::anim::compose_pose(sk, blended, posed_blend);
        engine::anim::compose_pose(sk, a, model_a);
        engine::anim::compose_pose(sk, b, model_b);
        for (std::size_t j = 0; j < posed_blend.size(); ++j)
        {
            const mat4 mixed = matrix_lerp(model_a[j], model_b[j], w);
            control_worst = std::fmax(control_worst,
                                      engine::length(engine::translation_of(mixed)
                                                     - engine::translation_of(posed_blend[j])));
        }
    }
    std::printf("   CONTROL at w = 0 and w = 1 the routes differ by %.3e\n",
                static_cast<double>(control_worst));
    check("G.4  CONTROL: with nothing to blend they agree", control_worst < 1e-5f);

    // And the report that tells you which regime you are in.
    std::vector<transform> ignored;
    const engine::anim::pose_blend_report rep =
        engine::anim::blend_poses(a, b, 0.5f, ignored);
    std::printf("   blend report: %zu joints, worst arc %.3f deg, %zu flips\n",
                rep.joints, static_cast<double>(deg(rep.worst_arc)), rep.flips);
    std::printf("     (the LOCAL arc: every joint differs by 20. The TIP has\n");
    std::printf("      accumulated 100, and that is not this number.)\n");
    check("G.5  worst_arc is the LOCAL arc, not the accumulated one",
          std::fabs(deg(rep.worst_arc) - 20.0f) < 0.01f);
}

// ============================================================================
// H. Reduction
// ============================================================================

void section_h(const skeleton& sk)
{
    std::printf("\nH. Throwing keys away\n");
    std::printf("   (err is against the CLOSED FORM the keys were baked\n");
    std::printf("    from, so the first row is what sampling at 30 Hz\n");
    std::printf("    costs before any key is thrown away at all.)\n");
    std::printf("   tol deg    keys   bytes   err deg   hdr/keys\n");

    std::vector<transform> sampled;
    std::vector<transform> truth;
    float coarse_ratio = 0.0f;

    for (float tol_deg : {0.00f, 0.05f, 0.25f, 0.50f, 2.00f, 8.00f})
    {
        clip reduced = bake_walk(1.0f, 30.0f, true);
        const reduction_limits limits{1.0e-3f, rad(tol_deg), 1.0e-3f};
        if (tol_deg > 0.0f) { engine::anim::reduce(reduced, sk, limits); }
        const clip_report rep = engine::anim::validate(reduced, sk);

        float worst_rot = 0.0f;
        // s < 600, not <=: at phase exactly 1 the time wraps to 0 and the root's
        // 1.2 units of travel would be charged to the reduction.
        for (int s = 0; s < 600; ++s)
        {
            const float phase = static_cast<float>(s) / 600.0f;
            engine::anim::sample_at(reduced, sk, phase * reduced.duration, sampled);
            true_pose(phase, truth);
            worst_rot = std::fmax(worst_rot, pose_gap_radians(sampled, truth));
        }
        const float ratio = static_cast<float>(rep.container_bytes)
                            / static_cast<float>(rep.key_bytes);
        coarse_ratio = ratio;
        std::printf("   %7.2f  %6zu  %6zu  %8.4f   %7.2fx\n",
                    static_cast<double>(tol_deg), rep.keys, rep.key_bytes,
                    static_cast<double>(deg(worst_rot)), static_cast<double>(ratio));
        sink += static_cast<double>(worst_rot);
    }
    std::printf("   the std::vector headers are fixed, so the better\n");
    std::printf("   the reduction the more of the clip they are: %.2fx\n",
                static_cast<double>(coarse_ratio));
    check("H.0  a well-reduced clip is mostly container", coarse_ratio > 1.0f);

    // WHICH KEYS SURVIVE, on one channel, so that the figure showing the
    // reduction is drawn from the reduction rather than from a sketch of one.
    for (float tol_deg : {0.50f, 2.00f})
    {
        clip reduced = bake_walk(1.0f, 30.0f, true);
        engine::anim::reduce(reduced, sk, reduction_limits{1e-3f, rad(tol_deg), 1e-3f});
        std::printf("   thigh.L rotation keys kept at %.2f deg (%zu of 31):\n",
                    static_cast<double>(tol_deg), reduced.tracks[15].rotation.size());
        // WRAPPED AT NINE PER LINE. The lesson quotes this transcript inside a
        // <pre> that scrolls and never wraps, and nineteen times six characters
        // is 119 — Lesson 7.3 lost fourteen numbers off the right-hand edge
        // before anybody noticed.
        for (std::size_t k = 0; k < reduced.tracks[15].rotation.size(); ++k)
        {
            if (k % 9u == 0) { std::printf("    "); }
            std::printf(" %.3f", static_cast<double>(reduced.tracks[15].rotation[k].time));
            if (k % 9u == 8u) { std::printf("\n"); }
        }
        if (reduced.tracks[15].rotation.size() % 9u != 0) { std::printf("\n"); }
    }

    // The tolerance must actually bound the playback error. Note this is a claim
    // about RECONSTRUCTING THE KEYS, not about the closed form they were baked
    // from: the reduction can only promise to reproduce what it was given.
    {
        clip full = bake_walk(1.0f, 30.0f, true);
        clip reduced = full;
        const reduction_limits limits{1.0e-3f, rad(0.5f), 1.0e-3f};
        const std::size_t removed = engine::anim::reduce(reduced, sk, limits);
        float worst = 0.0f;
        std::vector<transform> dense;
        for (int s = 0; s <= 1200; ++s)
        {
            const float t = static_cast<float>(s) / 1200.0f;
            engine::anim::sample_at(reduced, sk, t, sampled);
            engine::anim::sample_at(full, sk, t, dense);
            worst = std::fmax(worst, pose_gap_radians(sampled, dense));
        }
        std::printf("   at 0.50 deg: removed %zu keys, worst vs the\n", removed);
        std::printf("     unreduced clip %6.4f deg\n", static_cast<double>(deg(worst)));
        check("H.1  the tolerance bounds the reconstruction", deg(worst) <= 0.51f);

        // *** NOT A FIXED POINT — AND THE INTERESTING PART IS HOW LITTLE THAT
        // COSTS. *** The second pass fits against the FIRST pass's output rather
        // than against the original curve, so in principle the budgets add. Run
        // it to convergence and measure, rather than assuming.
        std::printf("   CONTROL reduce again, at the same tolerance:\n");
        std::printf("     pass   removed   keys   error deg\n");
        float settled = deg(worst);
        for (int pass = 2; pass <= 5; ++pass)
        {
            const std::size_t again = engine::anim::reduce(reduced, sk, limits);
            float twice = 0.0f;
            for (int s = 0; s <= 1200; ++s)
            {
                const float t = static_cast<float>(s) / 1200.0f;
                engine::anim::sample_at(reduced, sk, t, sampled);
                engine::anim::sample_at(full, sk, t, dense);
                twice = std::fmax(twice, pose_gap_radians(sampled, dense));
            }
            settled = deg(twice);
            std::printf("   %6d  %8zu  %5zu   %9.4f\n", pass, again,
                        engine::anim::validate(reduced, sk).keys, static_cast<double>(settled));
        }
        check("H.2  CONTROL: a second pass moves keys, not the error",
              settled <= 0.51f);
    }

    // *** FITTING AGAINST A CURVE YOU WILL NOT PLAY. *** One joint turning at a
    // constant rate about a fixed axis: slerp reproduces that EXACTLY, so a
    // slerp-fitted reduction keeps two keys and claims zero error — and then the
    // sampler walks it with nlerp.
    {
        skeleton one = straight_chain(2);
        clip spin;
        spin.name = "spin";
        spin.duration = 1.0f;
        spin.loops = false;
        spin.tracks.resize(2);
        const vec3 axis = engine::normalised(vec3{0.31f, 0.86f, -0.41f});
        for (int f = 0; f <= 30; ++f)
        {
            const float t = static_cast<float>(f) / 30.0f;
            spin.tracks[1].rotation.push_back(
                {t, engine::quat_from_axis_angle(axis, rad(120.0f) * t)});
        }

        // The fit the engine does: nlerp.
        clip by_nlerp = spin;
        engine::anim::reduce(by_nlerp, one, reduction_limits{1e-3f, rad(0.5f), 1e-3f});

        // The fit a textbook would do: slerp, which is exact on this curve, so
        // reduce it by hand to the two endpoints.
        clip by_slerp = spin;
        by_slerp.tracks[1].rotation = {spin.tracks[1].rotation.front(),
                                       spin.tracks[1].rotation.back()};

        float nlerp_err = 0.0f;
        float slerp_err = 0.0f;
        std::vector<transform> got;
        std::vector<transform> want;
        for (int s = 0; s <= 600; ++s)
        {
            const float t = static_cast<float>(s) / 600.0f;
            engine::anim::sample_at(spin, one, t, want);
            engine::anim::sample_at(by_nlerp, one, t, got);
            nlerp_err = std::fmax(nlerp_err, pose_gap_radians(got, want));
            engine::anim::sample_at(by_slerp, one, t, got);
            slerp_err = std::fmax(slerp_err, pose_gap_radians(got, want));
        }
        std::printf("   a 120 deg constant-rate turn, tolerance 0.50:\n");
        std::printf("     fitted with nlerp  %2zu keys, plays %7.4f deg out\n",
                    by_nlerp.tracks[1].rotation.size(), static_cast<double>(deg(nlerp_err)));
        std::printf("     fitted with slerp  %2zu keys, plays %7.4f deg out\n",
                    by_slerp.tracks[1].rotation.size(), static_cast<double>(deg(slerp_err)));
        std::printf("     the slerp fit overshoots its budget %5.1fx\n",
                    static_cast<double>(deg(slerp_err) / 0.5f));
        check("H.3  fitting with the sampler's rule keeps the promise",
              deg(nlerp_err) <= 0.51f);
        check("H.4  CONTROL: fitting with the other rule breaks it",
              deg(slerp_err) > 4.0f * 0.5f);
    }
}

// ============================================================================
// I. What it weighs, and what it costs
// ============================================================================

void section_i(const skeleton& sk)
{
    std::printf("\nI. Bytes, and nanoseconds\n");

    clip full = bake_walk(1.0f, 30.0f, true);
    const clip_report raw = engine::anim::validate(full, sk);

    clip lean = full;
    const std::size_t removed = engine::anim::reduce(lean, sk, reduction_limits{});
    const clip_report thin = engine::anim::validate(lean, sk);

    const std::size_t frames = full.tracks[0].rotation.size();
    const std::size_t pose_array = frames * k_joints * sizeof(transform);

    std::printf("   %zu joints, %zu frames at 30 Hz\n", k_joints, frames);
    std::printf("   layout                      bytes    of raw\n");
    std::printf("   pose array (40 B/joint)   %8zu   %6.1f%%\n", pose_array,
                100.0 * static_cast<double>(pose_array) / static_cast<double>(raw.key_bytes));
    std::printf("   per-channel, every key    %8zu   100.0%%\n", raw.key_bytes);
    std::printf("   per-channel, reduced      %8zu   %6.1f%%\n", thin.key_bytes,
                100.0 * static_cast<double>(thin.key_bytes)
                    / static_cast<double>(raw.key_bytes));
    check("I.1  a naive per-channel bake is BIGGER than a pose array",
          raw.key_bytes > pose_array);
    check("I.2  and elision turns that around several times over",
          thin.key_bytes * 5 < pose_array);

    std::printf("   channels carrying data: %zu of %zu  (%.1f%%)\n", thin.channels,
                3 * thin.tracks,
                100.0 * static_cast<double>(thin.channels)
                    / static_cast<double>(3 * thin.tracks));
    std::printf("     of which constant: %zu;  keys removed %zu\n",
                thin.constant_channels, removed);
    std::printf("     channels elided entirely: %zu\n", 3 * thin.tracks - thin.channels);
    std::printf("   what survived, joint by joint (P/R/S, . = gone):\n");
    for (std::size_t j = 0; j < lean.tracks.size(); ++j)
    {
        const joint_track& tr = lean.tracks[j];
        std::printf("     %-12s %c %c %c   %3zu keys\n", k_rig[j].name,
                    tr.position.empty() ? '.' : 'P', tr.rotation.empty() ? '.' : 'R',
                    tr.scale.empty() ? '.' : 'S', tr.keys());
    }
    std::printf("   std::vector headers        %8zu  (%.2fx the keys)\n",
                thin.container_bytes,
                static_cast<double>(thin.container_bytes)
                    / static_cast<double>(thin.key_bytes));
    check("I.3  the headers are a real fraction of a lean clip",
          thin.container_bytes * 3 > thin.key_bytes);

    // What the whole thing would cost at production scale.
    const double per_second = static_cast<double>(thin.key_bytes);
    std::printf("   scaled up: 100 joints, 30 s of this density\n");
    std::printf("     naive pose array %8.2f MB\n",
                100.0 * 40.0 * 30.0 * 30.0 / 1.0e6);
    std::printf("     this clip        %8.2f MB\n",
                per_second * 30.0 * (100.0 / static_cast<double>(k_joints)) / 1.0e6);

    // ---- Timing ------------------------------------------------------------
    std::vector<track_cursor> cursors;
    std::vector<transform> pose;
    std::vector<mat4> posed;
    std::vector<mat4> palette;

    const int reps = 40000;

    const double cursor_us = best_of_three(reps, [&](int r) {
        engine::anim::sample(full, sk, static_cast<float>(r % 60) * (1.0f / 60.0f),
                             cursors, pose);
        sink += static_cast<double>(pose[15].rotation.w);
    });
    const double search_us = best_of_three(reps, [&](int r) {
        engine::anim::sample_at(full, sk, static_cast<float>(r % 60) * (1.0f / 60.0f), pose);
        sink += static_cast<double>(pose[15].rotation.w);
    });
    const double palette_us = best_of_three(reps, [&](int) {
        engine::anim::skinning_palette(sk, pose, posed, palette);
        sink += static_cast<double>(palette[15].c3.x);
    });

    // AND THE SAME PAIR ON A LONG TRACK, because 31 keys is log2 = 4.95 and the
    // cursor's whole advantage is the difference between that and 1. A
    // ten-second bake is 301 keys and log2 = 8.23 — and the cursor does not
    // notice. NEITHER CLIP IS REDUCED HERE: the comparison is about how the two
    // searches scale with track length, and reducing one and not the other would
    // have been comparing two different lengths as well as two different
    // searches. (The first attempt did exactly that and reduced a ten-second clip
    // down to 14 keys, which made the "long" track the shorter one.)
    const clip lengthy = bake_walk(10.0f, 30.0f, true);
    std::vector<track_cursor> long_cursors;
    const double long_cursor_us = best_of_three(reps, [&](int r) {
        engine::anim::sample(lengthy, sk, static_cast<float>(r % 600) * (1.0f / 60.0f),
                             long_cursors, pose);
        sink += static_cast<double>(pose[15].rotation.w);
    });
    const double long_search_us = best_of_three(reps, [&](int r) {
        engine::anim::sample_at(lengthy, sk, static_cast<float>(r % 600) * (1.0f / 60.0f),
                                pose);
        sink += static_cast<double>(pose[15].rotation.w);
    });

    std::printf("   sampling %zu keys/track (unreduced):\n", full.tracks[15].rotation.size());
    std::printf("   (best of three)\n");
    std::printf("   sample, cursor      %7.3f us  (%5.2f ns/joint)\n", cursor_us,
                cursor_us * 1000.0 / static_cast<double>(k_joints));
    std::printf("   sample, binary      %7.3f us  (%5.2f ns/joint)\n", search_us,
                search_us * 1000.0 / static_cast<double>(k_joints));
    std::printf("   the cursor is       %7.2fx\n", search_us / cursor_us);
    std::printf("   compose + palette   %7.3f us\n", palette_us);
    std::printf("   the same clip 10x longer (%zu keys/track):\n",
                lengthy.tracks[15].rotation.size());
    std::printf("     cursor %7.3f us   binary %7.3f us   %5.2fx\n", long_cursor_us,
                long_search_us, long_search_us / long_cursor_us);
    check("I.4  remembering where you were beats searching", cursor_us < search_us);
    check("I.5  and the gap widens with the track length",
          long_search_us / long_cursor_us > search_us / cursor_us);

    // *** THE PLAYBACK CLOCK MUST BE WRAPPED, NOT ACCUMULATED — and this is the
    // confound that made the two rows above disagree with themselves until it was
    // found. *** Both loops originally drove the sampler with `r / 60`, a time
    // that climbs to 666 seconds, and the 301-key clip came out FASTER than the
    // 31-key one. `std::fmod` is not constant time: its cost grows with the
    // QUOTIENT, so a one-second clip at t = 666 does ten times the reduction a
    // ten-second clip does. Measured directly below, with nothing else in the
    // loop.
    const int fm_reps = 400000;
    std::printf("   std::fmod, per call, by how far the clock has run:\n");
    std::printf("     duration   t bounded   t at 6666 s\n");
    double worst_growth = 1.0;
    for (float dur : {1.0f, 10.0f})
    {
        volatile float keep = 0.0f;
        const double bounded = best_of_three(fm_reps, [&](int r) {
            keep = keep + std::fmod(static_cast<float>(r % 600) * (1.0f / 60.0f), dur);
        }) * 1000.0;
        const double grown = best_of_three(fm_reps, [&](int r) {
            keep = keep + std::fmod(static_cast<float>(r) * (1.0f / 60.0f), dur);
        }) * 1000.0;
        worst_growth = std::fmax(worst_growth, grown / bounded);
        sink += static_cast<double>(keep);
        std::printf("     %8.1f  %8.3f ns  %8.3f ns\n", static_cast<double>(dur),
                    bounded, grown);
    }
    std::printf("   letting the clock run costs up to  %.2fx\n", worst_growth);
    check("I.6  a clock that grows makes the wrap grow with it",
          worst_growth > 1.5);

    // CONTROL — a clip whose channels were ALL elided samples to the bind pose
    // exactly, which is what makes the zero-key case a representation and not an
    // approximation.
    clip nothing;
    nothing.name = "still";
    nothing.duration = 1.0f;
    nothing.tracks.resize(k_joints);
    std::vector<transform> rest;
    engine::anim::rest_pose(sk, rest);
    engine::anim::sample_at(nothing, sk, 0.37f, pose);
    std::printf("   CONTROL an empty clip vs the rest pose  %.3e\n",
                static_cast<double>(pose_gap_position(pose, rest)));
    check("I.7  CONTROL: no keys means the bind pose, exactly",
          pose_identical(pose, rest));
}

// ============================================================================

int main()
{
    std::printf("verify_77 - Lesson 7.7, Sampling and Blending\n");

    // WARM THE MACHINE BEFORE MEASURING IT. Section A's benchmark is the first
    // timing in the process, and on a laptop the first hundred milliseconds run
    // at a lower clock than everything after them: the same table came out 30%
    // slower on a cold process than on a warm one, three runs in a row, which is
    // larger than any difference it is trying to report. `best_of_three` cannot
    // fix that on its own — all three attempts are inside the ramp.
    {
        double spin = 1.0;
        const double until = now_seconds() + 0.15;
        while (now_seconds() < until)
        {
            for (int i = 0; i < 4096; ++i) { spin = spin * 1.0000001 + 1.0e-9; }
        }
        sink += spin * 1.0e-12;
    }

    const skeleton sk = build_rig();
    const engine::anim::skeleton_report rig = engine::anim::validate(sk);
    std::printf("   rig: %zu joints, depth %zu, residual %.3e\n", rig.joints, rig.depth,
                static_cast<double>(rig.worst_bind_residual));

    const clip walk = bake_walk(1.0f, 30.0f, true);

    section_a(sk, walk);
    section_b(sk, walk);
    section_c();
    section_d();
    section_e(sk);
    section_f(sk, walk);
    section_g();
    section_h(sk);
    section_i(sk);

    std::printf("\n%d / %d checks passed\n", checks_passed, checks_run);
    std::printf("(sink %.3f)\n", sink);
    return (checks_passed == checks_run) ? 0 : 1;
}
