// engine/src/anim/clip.cpp — finding two keys, and everything that follows.
//
// Lesson 7.7. The public story is in `engine/anim/clip.hpp`; this file is the
// four things that story needs and one of them is genuinely subtle.
//
//   THE BRACKET SEARCH, twice. `walk` remembers where it was and `search` does
//   not, and the two must agree to the bit or one of them is finding the wrong
//   interval. They are written side by side here so that the comparison in
//   `verify_77.cpp` §C is comparing two real implementations rather than one
//   implementation and a paraphrase of itself.
//
//   ONE EVALUATION PATH, PARAMETERISED BY THE SEARCH. `sample` and `sample_at`
//   differ in four characters' worth of behaviour, and duplicating the channel
//   handling to express that would mean a fix applied to one and not the other.
//   The template parameter below is a *policy*: it is resolved at compile time,
//   so the cursor version pays nothing for the existence of the binary one.
//
//   THE CLAMP, AND WHY IT IS NOT A CONTRADICTION. `transform_blend` deliberately
//   does not clamp its parameter — the engine's rule since Lesson 1.6 is that the
//   caller owns it. The sampler DOES clamp, and the difference is who owns the
//   number. By the time `segment_t` runs, the time has already been wrapped into
//   the clip's own range, so a parameter outside [0, 1] can only mean the time is
//   outside the KEYS' range, and the right answer there is to hold the nearest
//   key. Every DCC tool does this, and glTF only ever defines the interpolation
//   for `t_k < t_c < t_{k+1}`.
//
//   THE GREEDY REDUCTION, which is the one piece of real algorithm in the file.

#include <engine/anim/clip.hpp>

#include <algorithm>
#include <cmath>

namespace engine::anim {

namespace {

// ---- Finding the interval --------------------------------------------------

/// The same index, found from nothing: `log2(n)` steps and no memory.
///
/// The classic "last index satisfying a monotone predicate" search, which is the
/// one shape of binary search that does not have an off-by-one in it: the
/// midpoint rounds UP, the true branch keeps `mid`, and the loop ends when the
/// window is one wide. `t` below the first key collapses `hi` to 0, which is the
/// clamp the bracket wants anyway.
template <typename Key>
std::uint32_t search(const std::vector<Key>& keys, float t)
{
    std::size_t lo = 0;
    std::size_t hi = keys.size() - 2u;
    while (lo < hi)
    {
        const std::size_t mid = lo + (hi - lo + 1u) / 2u;
        if (keys[mid].time <= t) { lo = mid; }
        else { hi = mid - 1u; }
    }
    return static_cast<std::uint32_t>(lo);
}

/// How far the cursor may crawl forward before it gives up and searches.
///
/// **Four, and the number was measured rather than chosen.** Ordinary playback
/// advances the cursor by one interval every frame or two, so any limit above one
/// keeps the common case a walk; a clip whose keys are denser than the frame rate
/// (120 Hz keys sampled at 60) advances two, and four leaves headroom for three
/// without ever paying `log2(n)` on a frame that did not jump.
inline constexpr std::uint32_t k_walk_limit = 4;

/// The largest `i` with `keys[i].time <= t`, clamped into `[0, n - 2]`, starting
/// from where we were last time.
///
/// **A short walk, with a search underneath it.** Playback is coherent: the next
/// frame's time is a few milliseconds after this one's, so the answer is nearly
/// always the answer from last frame or the one after it, and the loop below
/// fails its test immediately or almost immediately. That is the case the cursor
/// exists for and it costs one comparison.
///
/// *** THE CURSOR'S WORST CASE IS THE LOOP WRAP. *** The first version of this
/// function walked BACKWARD as well, with a second `while`, on the argument that
/// the backward case happens once per cycle and is therefore amortised away. That
/// argument is very nearly right, and the measurement is worth having anyway
/// (harness §A, three strategies at two track lengths, one key interval per
/// lookup so that nothing but the length varies):
///
///     31 keys, wrapping every 30 lookups   301 keys, every 300
///       two walks        1.548 ns            two walks        1.729 ns
///       walk + search    1.324 ns            walk + search    0.985 ns
///       binary only      3.626 ns            binary only     10.183 ns
///
/// So: **14% at 31 keys and 43% at 301**, and the reason the saving grows is that
/// a wrap costs `n` steps in the retired version and `log2(n)` in this one, paid
/// once per cycle either way. What it really buys is the shape of the worst case:
/// a backward jump is never ordinary playback, so it does not deserve a linear
/// path, and `search` bounds it however far the caller seeks. Note also the
/// column the cursor is there for at all — against a pure binary search it is
/// **2.74x** on a short track and **10.34x** on a long one.
///
/// *** AND A WARNING ABOUT THE FIRST VERSION OF THAT MEASUREMENT. *** It varied
/// the wrap period by changing the DRIVER'S STEP SIZE, which also changed how
/// many intervals each lookup advanced — so it was measuring two things and
/// reported 43% for one of them. The table above steps exactly one interval per
/// lookup, which is the only way the two rows differ in nothing but length.
///
/// **A stale cursor cannot corrupt the answer**, only its cost, and that is the
/// property that makes this safe to keep across a clip change the caller forgot
/// to announce: both routes are driven by the key times, not by trust.
template <typename Key>
std::uint32_t walk(const std::vector<Key>& keys, float t, std::uint32_t cursor)
{
    const auto last = static_cast<std::uint32_t>(keys.size() - 2u);
    if (cursor > last) { cursor = last; }

    // Backwards: a seek, or the moment the clip looped. Not a walk.
    if (keys[cursor].time > t) { return search(keys, t); }

    for (std::uint32_t step = 0; step < k_walk_limit; ++step)
    {
        if (cursor >= last || keys[cursor + 1u].time > t) { return cursor; }
        ++cursor;
    }

    // Still not there: this is a forward seek wearing a walk's clothes.
    return (cursor < last && keys[cursor + 1u].time <= t) ? search(keys, t) : cursor;
}

/// Where in the interval `t` falls, as a number in [0, 1].
///
/// A zero-length interval returns 0 rather than dividing — `clip_report::unsorted`
/// is what tells the caller that happened, since a clip whose times are strictly
/// increasing (glTF **MUST**) cannot reach it.
[[nodiscard]] float segment_t(float t0, float t1, float t)
{
    const float span = t1 - t0;
    if (!(span > 0.0f)) { return 0.0f; }
    return std::clamp((t - t0) / span, 0.0f, 1.0f);
}

// ---- One channel, evaluated ------------------------------------------------

/// A channel with one key is a constant; the general case brackets and lerps.
template <typename Locate>
[[nodiscard]] vec3 eval(const std::vector<vec3_key>& keys, float t, std::uint32_t& cursor,
                        Locate locate)
{
    if (keys.size() == 1u) { return keys[0].value; }
    cursor = locate(keys, t, cursor);
    const vec3_key& k0 = keys[cursor];
    const vec3_key& k1 = keys[cursor + 1u];
    return lerp(k0.value, k1.value, segment_t(k0.time, k1.time, t));
}

/// The rotation channel. `quat_nlerp` carries `nearest` inside it, so the double
/// cover is paid for here whether or not `canonicalise_rotations` has run — and
/// after it has run, the branch is one that never fires.
template <typename Locate>
[[nodiscard]] quat eval(const std::vector<quat_key>& keys, float t, std::uint32_t& cursor,
                        Locate locate)
{
    if (keys.size() == 1u) { return keys[0].value; }
    cursor = locate(keys, t, cursor);
    const quat_key& k0 = keys[cursor];
    const quat_key& k1 = keys[cursor + 1u];
    return quat_nlerp(k0.value, k1.value, segment_t(k0.time, k1.time, t));
}

struct by_cursor
{
    template <typename Key>
    std::uint32_t operator()(const std::vector<Key>& keys, float t, std::uint32_t prev) const
    {
        return walk(keys, t, prev);
    }
};

struct by_search
{
    template <typename Key>
    std::uint32_t operator()(const std::vector<Key>& keys, float t, std::uint32_t) const
    {
        return search(keys, t);
    }
};

/// The shared body of `sample` and `sample_at`. **Does not wrap the time** — both
/// public entry points do that before calling in, and `validate` deliberately
/// does not, because `wrap_time(c, c.duration)` is 0 for a looping clip and
/// measuring the loop seam against itself would report a perfect zero forever.
template <typename Locate>
void sample_impl(const clip& c, const skeleton& sk, float t, track_cursor* cursors,
                 std::vector<transform>& out, Locate locate)
{
    out.resize(sk.size());
    for (std::size_t j = 0; j < sk.size(); ++j)
    {
        // The fallback is the bind value, per joint and per CHANNEL. That is what
        // makes an upper-body clip legal: the legs have no track, so they stand
        // where the rig says they stand instead of collapsing to the origin.
        transform value = sk.joints[j].local_bind;

        if (j < c.tracks.size())
        {
            const joint_track& track = c.tracks[j];
            track_cursor scratch{};
            track_cursor& cur = (cursors != nullptr) ? cursors[j] : scratch;

            if (!track.position.empty())
            {
                value.position = eval(track.position, t, cur.position, locate);
            }
            if (!track.rotation.empty())
            {
                value.rotation = eval(track.rotation, t, cur.rotation, locate);
            }
            if (!track.scale.empty())
            {
                value.scale = eval(track.scale, t, cur.scale, locate);
            }
        }

        out[j] = value;
    }
}

// ---- Reduction -------------------------------------------------------------

/// Greedy fit-and-split over one channel.
///
/// Keep the first key. Extend a candidate span as far as it will go while every
/// key strictly inside it is reconstructible within tolerance from the span's two
/// endpoints; the moment one is not, close the span at the previous key and make
/// that the new anchor. The last key is always kept, because a clip that ends
/// somewhere other than where it was authored to end is a clip whose loop seam
/// has moved.
///
/// **This is greedy and not optimal, and the difference does not matter.** The
/// optimal fewest-keys reconstruction within a tolerance is a shortest-path
/// problem over `O(n^2)` candidate spans, which is solvable and which buys a few
/// per cent on real curves. Greedy is `O(n^2)` worst case and linear in practice
/// (the inner loop only ever re-scans the keys inside the current span), runs at
/// load time, and is what every exporter this course has read actually does.
template <typename Key, typename Fits>
std::size_t reduce_channel(std::vector<Key>& keys, Fits fits)
{
    if (keys.size() <= 2u) { return 0; }

    std::vector<Key> kept;
    kept.reserve(keys.size());
    kept.push_back(keys.front());

    std::size_t anchor = 0;
    for (std::size_t j = 2; j < keys.size(); ++j)
    {
        bool ok = true;
        for (std::size_t m = anchor + 1u; m < j && ok; ++m)
        {
            ok = fits(keys[anchor], keys[j], keys[m]);
        }
        if (!ok)
        {
            kept.push_back(keys[j - 1u]);
            anchor = j - 1u;
        }
    }
    kept.push_back(keys.back());

    const std::size_t removed = keys.size() - kept.size();
    keys.swap(kept);
    return removed;
}

}   // namespace

// ---- Time ------------------------------------------------------------------

float wrap_time(const clip& c, float t)
{
    // `!(d > 0)` and not `d <= 0`, so that a NaN duration lands here rather than
    // reaching the fmod. Lesson 7.5's rule about comparisons that a NaN passes.
    if (!(c.duration > 0.0f)) { return 0.0f; }
    if (!c.loops) { return std::clamp(t, 0.0f, c.duration); }

    // std::fmod takes the sign of the DIVIDEND, so fmod(-0.1, 2.0) is -0.1 and
    // not 1.9. One add fixes it; leaving it out is the bug this function's doc
    // comment describes.
    float wrapped = std::fmod(t, c.duration);
    if (wrapped < 0.0f) { wrapped += c.duration; }
    return wrapped;
}

void reset_cursors(const clip& c, std::vector<track_cursor>& cursors)
{
    cursors.assign(c.tracks.size(), track_cursor{});
}

// ---- Sampling --------------------------------------------------------------

void sample(const clip& c, const skeleton& sk, float time,
            std::vector<track_cursor>& cursors, std::vector<transform>& out)
{
    // Grown, never shrunk, and never reset: a caller that hands in a default
    // vector on its first frame gets a correct pose and a warm cursor afterwards.
    if (cursors.size() < c.tracks.size())
    {
        cursors.resize(c.tracks.size());
    }
    sample_impl(c, sk, wrap_time(c, time), cursors.data(), out, by_cursor{});
}

void sample_at(const clip& c, const skeleton& sk, float time, std::vector<transform>& out)
{
    sample_impl(c, sk, wrap_time(c, time), nullptr, out, by_search{});
}

// ---- Blending --------------------------------------------------------------

pose_blend_report blend_poses(std::span<const transform> a, std::span<const transform> b,
                              float w, std::vector<transform>& out)
{
    pose_blend_report report;
    report.joints = a.size();
    out.resize(a.size());

    const std::size_t shared = std::min(a.size(), b.size());

    // THE ARGMIN, NOT THE ANGLE. `|scalar|` is monotonically decreasing in the
    // rotation angle, so the joint with the smallest one IS the joint with the
    // largest arc — found with one dot product each, which `nearest` needs
    // anyway. The expensive, well-conditioned metric then runs exactly once, on
    // the winner. Calling `angle_between` per joint would put an atan2 on the
    // per-frame path to answer a question about a single joint.
    float smallest = 2.0f;
    std::size_t worst = 0;

    for (std::size_t j = 0; j < shared; ++j)
    {
        const float scalar = a[j].rotation.w * b[j].rotation.w
                             + dot(a[j].rotation.v, b[j].rotation.v);
        if (scalar < 0.0f) { ++report.flips; }

        const float magnitude = std::fabs(scalar);
        if (magnitude < smallest)
        {
            smallest = magnitude;
            worst = j;
        }

        out[j] = transform_blend(a[j], b[j], w);
    }

    // A pose longer than its partner keeps its own tail. An upper-body blend is
    // exactly this, and dropping the remainder would leave the legs unwritten in
    // a vector the caller is about to compose.
    for (std::size_t j = shared; j < a.size(); ++j) { out[j] = a[j]; }

    if (shared > 0)
    {
        report.worst_arc = angle_between(a[worst].rotation, b[worst].rotation);
    }
    return report;
}

// ---- Load-time work --------------------------------------------------------

std::size_t canonicalise_rotations(clip& c)
{
    std::size_t flipped = 0;
    for (joint_track& track : c.tracks)
    {
        for (std::size_t k = 1; k < track.rotation.size(); ++k)
        {
            const quat previous = track.rotation[k - 1u].value;
            quat& here = track.rotation[k].value;
            const quat aligned = nearest(previous, here);
            if (aligned.w != here.w || aligned.v.x != here.v.x || aligned.v.y != here.v.y
                || aligned.v.z != here.v.z)
            {
                here = aligned;
                ++flipped;
            }
        }
    }
    return flipped;
}

std::size_t key_bytes(const clip& c)
{
    std::size_t total = 0;
    for (const joint_track& track : c.tracks)
    {
        total += track.position.size() * sizeof(vec3_key);
        total += track.rotation.size() * sizeof(quat_key);
        total += track.scale.size() * sizeof(vec3_key);
    }
    return total;
}

namespace {

/// Does this whole channel sit within tolerance of its own first value?
///
/// Checked BEFORE the greedy pass rather than after, because it is the common
/// case and it is cheaper: one comparison per key against a fixed value, instead
/// of a fit-and-split that will keep two keys and then have to be asked the same
/// question anyway.
template <typename Key, typename Near>
[[nodiscard]] bool channel_is_constant(const std::vector<Key>& keys, Near near)
{
    for (std::size_t k = 1; k < keys.size(); ++k)
    {
        if (!near(keys[0].value, keys[k].value)) { return false; }
    }
    return true;
}

/// Collapse a channel to one key, or to none if that key is the bind value.
///
/// Returns how many keys went. The zero-key case is EXACT and not an
/// approximation of the one-key case: `sample_impl` falls back to
/// `sk.joints[j].local_bind` for an empty channel, so deleting a channel whose
/// only value IS that bind value reproduces the same pose bit for bit.
template <typename Key, typename Near, typename Value>
std::size_t collapse(std::vector<Key>& keys, Near near, Value bind)
{
    const std::size_t was = keys.size();
    if (near(keys[0].value, bind)) { keys.clear(); }
    else { keys.resize(1); }
    return was - keys.size();
}

}   // namespace

std::size_t reduce(clip& c, const skeleton& sk, const reduction_limits& limits)
{
    const auto near_position = [&](vec3 a, vec3 b) { return length(a - b) <= limits.position; };
    const auto near_rotation = [&](quat a, quat b) { return angle_between(a, b) <= limits.rotation; };
    const auto near_scale = [&](vec3 a, vec3 b) {
        const vec3 gap = a - b;
        return std::fabs(gap.x) <= limits.scale && std::fabs(gap.y) <= limits.scale
               && std::fabs(gap.z) <= limits.scale;
    };

    std::size_t removed = 0;
    for (std::size_t j = 0; j < c.tracks.size(); ++j)
    {
        joint_track& track = c.tracks[j];
        const transform bind = (j < sk.size()) ? sk.joints[j].local_bind : transform{};

        if (!track.position.empty())
        {
            if (channel_is_constant(track.position, near_position))
            {
                removed += collapse(track.position, near_position, bind.position);
            }
            else
            {
                removed += reduce_channel(track.position,
                    [&](const vec3_key& k0, const vec3_key& k1, const vec3_key& mid) {
                        const float u = segment_t(k0.time, k1.time, mid.time);
                        return near_position(lerp(k0.value, k1.value, u), mid.value);
                    });
            }
        }

        if (!track.rotation.empty())
        {
            if (channel_is_constant(track.rotation, near_rotation))
            {
                removed += collapse(track.rotation, near_rotation, bind.rotation);
            }
            else
            {
                // *** `quat_nlerp` AND NOT `quat_slerp`, AND THIS IS THE LINE THE
                // DOC COMMENT IS ABOUT. *** Reduction manufactures long arcs on
                // purpose, and long arcs are exactly where the two schedules
                // disagree. Fitting against a curve the sampler will not walk
                // means the playback error can exceed the tolerance that was
                // asked for — by more, the better the reduction worked. §H of the
                // harness measures it both ways.
                removed += reduce_channel(track.rotation,
                    [&](const quat_key& k0, const quat_key& k1, const quat_key& mid) {
                        const float u = segment_t(k0.time, k1.time, mid.time);
                        return near_rotation(quat_nlerp(k0.value, k1.value, u), mid.value);
                    });
            }
        }

        if (!track.scale.empty())
        {
            if (channel_is_constant(track.scale, near_scale))
            {
                removed += collapse(track.scale, near_scale, bind.scale);
            }
            else
            {
                removed += reduce_channel(track.scale,
                    [&](const vec3_key& k0, const vec3_key& k1, const vec3_key& mid) {
                        const float u = segment_t(k0.time, k1.time, mid.time);
                        return near_scale(lerp(k0.value, k1.value, u), mid.value);
                    });
            }
        }
    }
    return removed;
}

// ---- Validation ------------------------------------------------------------

namespace {

/// Count what one time array does wrong. Shared by all three channel types,
/// because "strictly increasing, inside [0, duration]" is a statement about times
/// and knows nothing about what is being animated.
template <typename Key>
void audit_times(const std::vector<Key>& keys, float duration, clip_report& report)
{
    if (keys.empty()) { return; }

    ++report.channels;
    if (keys.size() == 1u) { ++report.constant_channels; }
    report.keys += keys.size();

    for (std::size_t k = 0; k < keys.size(); ++k)
    {
        if (k > 0 && !(keys[k].time > keys[k - 1u].time)) { ++report.unsorted; }
        if (keys[k].time < 0.0f || keys[k].time > duration) { ++report.out_of_range; }
    }
}

}   // namespace

clip_report validate(const clip& c, const skeleton& sk)
{
    clip_report report;
    report.tracks = c.tracks.size();
    report.beyond_skeleton = (c.tracks.size() > sk.size()) ? c.tracks.size() - sk.size() : 0;

    for (const joint_track& track : c.tracks)
    {
        audit_times(track.position, c.duration, report);
        audit_times(track.rotation, c.duration, report);
        audit_times(track.scale, c.duration, report);

        for (std::size_t k = 0; k < track.rotation.size(); ++k)
        {
            const quat value = track.rotation[k].value;
            // A tolerance on the SQUARED length, which is the quantity the code
            // actually has; 1e-4 on the square is 5e-5 on the length, three
            // orders above float noise and three below anything an exporter's
            // rounding would produce.
            if (std::fabs(length_squared(value) - 1.0f) > 1.0e-4f) { ++report.unnormalised; }

            if (k > 0)
            {
                const quat previous = track.rotation[k - 1u].value;
                if (previous.w * value.w + dot(previous.v, value.v) < 0.0f)
                {
                    ++report.sign_flips;
                }
            }
        }
    }

    report.key_bytes = key_bytes(c);
    // The HEADERS, and nothing else: not the heap blocks the vectors point at
    // (those are `key_bytes` plus the allocator's own bookkeeping) and not the
    // capacity a `reserve` may have left over. Three vectors per joint, and on a
    // 64-bit toolchain a vector is three pointers.
    report.container_bytes = c.tracks.size() * sizeof(joint_track);

    // ---- The loop seam -----------------------------------------------------
    //
    // Sampled through `sample_impl` DIRECTLY, bypassing `wrap_time`: for a
    // looping clip `wrap_time(c, c.duration)` is 0, so going through the public
    // entry point would compare the start of the clip with itself and report a
    // perfect seam on every clip ever written.
    if (c.loops && c.duration > 0.0f && !sk.empty())
    {
        std::vector<transform> start;
        std::vector<transform> finish;
        sample_impl(c, sk, 0.0f, nullptr, start, by_search{});
        sample_impl(c, sk, c.duration, nullptr, finish, by_search{});

        for (std::size_t j = 0; j < start.size(); ++j)
        {
            const float moved = length(finish[j].position - start[j].position);

            // The root goes in its own field. A walk cycle's root ends the cycle
            // a stride in front of where it began, and that is the animation
            // working rather than failing; lumping it in with the other joints
            // makes the seam measurement useless on exactly the clips that loop.
            if (sk.joints[j].parent == k_no_parent) { report.root_travel = std::fmax(report.root_travel, moved); }
            else { report.loop_gap_position = std::fmax(report.loop_gap_position, moved); }

            report.loop_gap_radians = std::fmax(
                report.loop_gap_radians,
                angle_between(start[j].rotation, finish[j].rotation));
        }
    }

    return report;
}

}   // namespace engine::anim
