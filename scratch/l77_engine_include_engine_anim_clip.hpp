// engine/include/engine/anim/clip.hpp — motion, recorded and played back.
//
// Lesson 7.7. `anim/skeleton.hpp` turns a POSE into a palette and `anim/skin.hpp`
// spends the palette on vertices. Neither of them has any idea where a pose comes
// from, and that was on purpose: every function in both files takes
// `std::span<const transform>` and asks no further questions. Lesson 7.6 filled
// that span from two sliders. This file fills it from data.
//
//     a clip is a function from TIME to POSE
//
// That sentence is the whole design, and the interesting part is the word
// "function" — because a clip does not STORE poses. It stores a handful of
// samples per channel and reconstructs everything between them, which is the
// difference between 3.6 MB and 40 KB for the same thirty seconds of motion.
//
// THE LAYOUT IS THE FIRST DECISION, AND IT IS NOT THE OBVIOUS ONE. The obvious
// layout is an array of poses, one per frame: `pose[frame][joint]`. It is simple,
// it samples in constant time, and it is what a naive recorder produces. It is
// also wrong in two ways that compound. It forces every joint to be sampled at
// the same rate as the busiest one, and it cannot express "this joint never
// moves" — which is most of them, because a rig animates rotation nearly
// everywhere and translation almost nowhere. The layout below is glTF's, and
// every other format's: one independent track per joint, and inside it one
// independent channel per field, each with ITS OWN times.
//
//     clip
//      +- tracks[j]                       joint j
//           +- position  [ (t, v) ... ]   its own times
//           +- rotation  [ (t, q) ... ]   its own times
//           +- scale     [ (t, v) ... ]   its own times
//
// An EMPTY channel means "use the skeleton's bind value" and costs nothing. A
// channel with ONE key is a constant. Both are common, and together they are
// where most of the saving is: §9 of the lesson measures a walk-shaped clip at
// 33.3% of its channels carrying any data at all.
//
// WHAT IS NOT IN THE CLIP, AND WHY IT MATTERS MORE THAN IT SOUNDS. There is no
// playback time here, and no cursor. A clip is an ASSET: one copy, shared by
// every character playing it, immutable once loaded. Putting `float time` in this
// struct would mean two characters could not play the same walk out of phase, and
// putting the search cursor here would mean they could not play it at all without
// fighting over a `std::uint32_t`. The mutable half lives in the caller, in a
// `std::vector<track_cursor>`, exactly as `compose_pose`'s scratch arrays do.
// It is the same split `skeleton` and `pose` already make, one level up: the
// shared thing is static and the per-instance thing is a span the caller owns.
//
// THE INTERPOLATION RULES LIVE IN `math/transform.hpp`, not here, and that file's
// Lesson 7.7 block comment is where the three decisions — lerp the position,
// nlerp the rotation, lerp the scale — are argued out with their numbers.

#pragma once

#include <engine/anim/skeleton.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/transform.hpp>
#include <engine/math/vec3.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace engine::anim {

/// One recorded sample: a time, and what the value was at that time.
///
/// **A template, and it is the smallest useful one in the engine**, so it is
/// worth saying what a template buys here that a macro or two hand-written
/// structs would not. `keyframe<vec3>` and `keyframe<quat>` are two distinct
/// types with two distinct sizes (16 and 20 bytes), generated once from one
/// definition, with no runtime dispatch and no common base class — the compiler
/// stamps out exactly the two it is asked for. That is the C++ feature this
/// course has been avoiding until it paid for itself: a container of a type that
/// varies, where every variant is known at compile time.
///
/// The time is ABSOLUTE, in seconds from the start of the clip, rather than a
/// delta from the previous key. Deltas would save nothing (the field is the same
/// four bytes) and would cost the sampler a running sum, which is the sort of
/// state that makes a random seek impossible.
template <typename T>
struct keyframe
{
    float time = 0.0f;
    T value{};
};

/// A translation or scale key: 16 bytes.
using vec3_key = keyframe<vec3>;

/// A rotation key: 20 bytes, because `quat` is four floats and `float time`
/// makes five. The extra four bytes over a `vec3_key` are the reason §9's
/// storage table separates the two rather than quoting one number per key.
using quat_key = keyframe<quat>;

/// Everything one joint does, as three independent channels.
///
/// **Parallel to `skeleton::joints` by INDEX**, which is the same contract the
/// inverse bind array keeps and the same one it has to earn: an exporter's joint
/// order is not ours (glTF's `skins.joints` is an arbitrary permutation), so the
/// importer that reorders the skeleton must reorder the tracks in the same pass
/// or every joint in the character is animated by its neighbour. A clip with
/// FEWER tracks than the skeleton has joints is legal and useful — an upper-body
/// clip is exactly that — and the joints past the end take their bind values.
struct joint_track
{
    /// Where the joint sits, in its parent's space. Usually EMPTY: a limb does
    /// not slide along its parent, it pivots, so the only joints that genuinely
    /// translate are the root (which carries the character across the ground) and
    /// the handful an animator has deliberately offset.
    std::vector<vec3_key> position;

    /// Which way the joint faces. The channel that carries nearly all of the
    /// motion, and nearly all of the bytes.
    std::vector<quat_key> rotation;

    /// How big. Almost always empty, and when it is not, it is usually a squash
    /// on one or two joints for a few frames.
    std::vector<vec3_key> scale;

    [[nodiscard]] bool empty() const
    {
        return position.empty() && rotation.empty() && scale.empty();
    }

    [[nodiscard]] std::size_t keys() const
    {
        return position.size() + rotation.size() + scale.size();
    }
};

/// A named piece of recorded motion.
///
/// **`duration` is authored, not derived, and the difference is the loop seam.**
/// The obvious definition — the largest time on any key — is wrong for exactly
/// the clips people care most about. A two-second walk cycle authored at 30 Hz
/// has its last key at frame 59, which is `59/30 = 1.9667 s`, and a loop that
/// restarts there is a loop that is 1/30th of a second short and lands one frame
/// out of step every cycle. The exporter knows the intended length; the key times
/// only know where somebody happened to put a key. `validate` measures what the
/// stated duration actually costs (`loop_gap_position`, `loop_gap_radians`) so a
/// wrong one is visible rather than merely felt.
struct clip
{
    /// What it is called, and the reason it is a `std::string` rather than an
    /// index: a clip is looked up by name by gameplay code, by a debug UI, and by
    /// the importer that made it, and "clip 7" is not a bug report.
    std::string name;

    /// Length in seconds. See above — authored, and not the last key's time.
    float duration = 0.0f;

    /// Whether `wrap_time` wraps or clamps past the end.
    ///
    /// A property of the CLIP and not of the playback, which is a choice worth
    /// defending: a walk cycle loops and a death animation does not, and that
    /// fact belongs to the motion rather than to whoever is playing it. A caller
    /// that wants to override it can wrap the time itself and pass the result.
    bool loops = true;

    /// One per joint, by index; may be shorter than the skeleton.
    std::vector<joint_track> tracks;

    [[nodiscard]] std::size_t size() const { return tracks.size(); }
    [[nodiscard]] bool empty() const { return tracks.empty(); }

    void clear()
    {
        name.clear();
        duration = 0.0f;
        tracks.clear();
    }
};

/// Where each of one joint's three channels was looked up last time.
///
/// **The entire optimisation of §4, and it is twelve bytes.** Finding the two
/// keys that bracket a time is a search, and a binary search over `n` keys is
/// `log2(n)` unpredictable branches per channel per joint per frame. Playback is
/// not a random access pattern: the next frame's time is a few milliseconds after
/// this one's, which is either the same interval or the next one. Remembering
/// where you were turns the search into a comparison that is almost always false.
///
/// It lives in the CALLER, never in the clip — see this file's header comment.
/// One `std::vector<track_cursor>` per playing character: 12 bytes a joint, so
/// 1.2 KB for a hundred-joint rig, against a clip that may be megabytes and is
/// shared by all of them.
struct track_cursor
{
    std::uint32_t position = 0;
    std::uint32_t rotation = 0;
    std::uint32_t scale = 0;
};

/// What `validate` found. Counts rather than a bool, like every other report in
/// this engine, because "how" is the question you ask next.
struct clip_report
{
    std::size_t tracks = 0;             ///< joints the clip has something to say about
    std::size_t channels = 0;           ///< of a possible 3 x tracks, how many carry keys
    std::size_t constant_channels = 0;  ///< …of which these hold exactly one key
    std::size_t keys = 0;               ///< every key in every channel
    std::size_t key_bytes = 0;          ///< what those keys weigh
    std::size_t container_bytes = 0;    ///< what the std::vectors around them weigh

    /// Keys whose time is **not strictly greater** than their predecessor's.
    ///
    /// The precondition the bracket search relies on and cannot afford to
    /// re-check per frame. glTF states it as a **MUST**: "The values represent
    /// time in seconds with `time[0] >= 0.0`, and strictly increasing values,
    /// i.e., `time[n + 1] > time[n]`." Two equal times make the segment length
    /// zero and the normalised parameter a division by zero; a decreasing pair
    /// makes the cursor walk the wrong way and never arrive.
    std::size_t unsorted = 0;

    /// Keys before 0 or after `duration`.
    ///
    /// Not fatal — the sampler clamps — but it means either the duration is wrong
    /// or the export trimmed the wrong range, and both of those show up as a
    /// character that freezes for a moment at one end of its loop.
    std::size_t out_of_range = 0;

    /// Rotation keys that are not unit quaternions, beyond float tolerance.
    ///
    /// glTF requires unit rotations ("If defined, the rotation quaternion **MUST**
    /// be unit"). A non-unit key is not a rotation at all: `quat_nlerp` normalises
    /// its result so the pose stays finite, but the SCHEDULE is wrong in
    /// proportion to how far off unit the endpoints are, which reads as a joint
    /// that eases oddly on one interval and correctly on the next.
    std::size_t unnormalised = 0;

    /// Adjacent rotation keys on **opposite halves of the sphere**.
    ///
    /// Not an error, and deliberately not part of `ok()`: `q` and `-q` are the
    /// same orientation (Lesson 7.4 §8) and an exporter has no obligation to keep
    /// the signs consistent. It is counted because it is the single most
    /// expensive-looking cheap bug in animation — interpolate across such a pair
    /// without `nearest` and the character takes the 359-degree route between two
    /// poses one degree apart, for exactly one keyframe interval. See
    /// `canonicalise_rotations`, which is how you pay for it once instead of
    /// every frame forever.
    std::size_t sign_flips = 0;

    /// Tracks past the end of the skeleton: the clip animates joints that do not
    /// exist. Almost always an importer that reordered one array and not the
    /// other.
    std::size_t beyond_skeleton = 0;

    /// **The discontinuity a loop would step over**, in units and in radians.
    ///
    /// Sample the clip at `t = 0` and at `t = duration` and compare the two poses.
    /// A clip that loops cleanly has both of these at float noise, because its
    /// last key repeats its first. A clip whose duration is one frame short — the
    /// `59/30` case in `clip`'s comment above — has a gap the size of one frame of
    /// motion, which is a visible tick once per cycle and is very hard to diagnose
    /// by looking at the character, because the animation is correct everywhere
    /// except at a single instant.
    ///
    /// Zero for a clip that does not loop, where the question is not asked.
    ///
    /// **THE ROOT JOINTS ARE EXCLUDED, AND FINDING OUT WHY IS WORTH THE FIELD
    /// BELOW.** The first version of this measurement included them and reported
    /// a 1.200-unit seam on a walk cycle that is perfect — because the root
    /// carries the character forward, so it is *supposed* to end the cycle 1.2
    /// units from where it started. An instrument that cannot tell root motion
    /// from a broken loop reports every correct walk as broken. So the travel is
    /// split out and named.
    float loop_gap_position = 0.0f;
    float loop_gap_radians = 0.0f;

    /// **How far the root joints move over one cycle: root motion, measured.**
    ///
    /// Not a defect — it is the distance the character is meant to cover, and a
    /// walk cycle with zero here is one that moonwalks on the spot. It is
    /// reported because an animation system has to do something deliberate with
    /// it: apply it to the character's WORLD transform and subtract it from the
    /// pose, or the character snaps back to the origin every time the clip wraps.
    /// This engine does neither yet; Lesson 7.7 §10 says so plainly and names it
    /// as the gap it is.
    float root_travel = 0.0f;

    /// Nothing structural is wrong. `sign_flips` and the loop gaps are
    /// measurements, not verdicts: the first is legal data and the second depends
    /// on what the clip is for.
    [[nodiscard]] bool ok() const
    {
        return unsorted == 0 && out_of_range == 0 && unnormalised == 0
               && beyond_skeleton == 0;
    }
};

/// What a pose blend just did. Returned rather than stored, because it is a
/// property of one call and not of anything that persists.
struct pose_blend_report
{
    std::size_t joints = 0;

    /// **The largest angle between the two inputs' LOCAL orientations, in
    /// radians** — the whole rotation angle, not the quaternion half-angle.
    ///
    /// *Local*, and that word does real work. Two poses of a six-joint chain that
    /// differ by 20 degrees at every joint report 20 degrees here, not the 100
    /// the tip has accumulated, because the thing being interpolated is the
    /// per-joint transform and the per-joint arc is what sets nlerp's error. The
    /// number that grows down the chain is a property of the composition, and
    /// §7 of the lesson measures what it does — but it is not this number and
    /// substituting one for the other would over-report the error by the depth
    /// of the skeleton.
    ///
    /// This is the number that says whether `transform_blend`'s nlerp is the right
    /// rule for the blend you are actually doing. Lesson 7.5's table, restated:
    /// under 30 degrees the nlerp and slerp schedules differ by 0.13 degrees, at
    /// 90 by 4.07, at 150 by 26.34. A cross-fade between two clips at the same
    /// phase of the same walk reports a few degrees; a cross-fade between a walk
    /// and a ragdoll recovery reports over a hundred, and that is a blend which
    /// wants `transform_blend_slerp` — or, better, a shorter fade.
    float worst_arc = 0.0f;

    /// Joints where `nearest` had to negate one of the two quaternions.
    ///
    /// Zero says the two poses' sign conventions agree; a large count says they
    /// came from different exporters, which is harmless here and is worth knowing
    /// before you go looking for a bug elsewhere.
    std::size_t flips = 0;
};

/// How much a channel is allowed to move before `reduce` must keep a key.
///
/// **The units are the content's, not a percentage**, which is deliberate: a
/// tolerance of "0.1%" means something different on a joint two metres from the
/// root than on one two centimetres from it, and the thing an animator can
/// actually judge is "half a degree" or "a millimetre".
struct reduction_limits
{
    /// Metres (or whatever the rig's unit is). 1 mm at the engine's 1 unit = 1 m.
    float position = 1.0e-3f;

    /// Radians. Half a degree by default — about the smallest rotation of a limb
    /// a viewer can see on a moving character at gameplay distance.
    float rotation = 8.72665e-3f;

    /// A ratio-free absolute tolerance on each scale component, matching the
    /// componentwise lerp the sampler uses.
    float scale = 1.0e-3f;
};

/// Check a clip's structure, measure what it weighs, and measure its loop seam.
///
/// O(keys) plus two samples of the whole clip, so it is a load-time tool. Pass
/// the skeleton's joint count so that `beyond_skeleton` can be answered; pass the
/// skeleton itself to `sample_at` if you want the poses the loop gap is measured
/// from.
[[nodiscard]] clip_report validate(const clip& c, const skeleton& sk);

/// Put every rotation key on the same half of the sphere as its predecessor.
///
/// **Paying for the double cover once, at load, instead of every frame forever.**
/// `nearest` inside the sampler is one dot product and one conditional negate per
/// rotation key sampled, which is per joint per frame per character; the same fix
/// applied here is one dot product per key in the file, total, and it is the
/// version that can also REPORT what it found. After this runs, every adjacent
/// pair in every rotation channel has a non-negative dot product, so `nearest`
/// inside the sampler becomes a test that is never true.
///
/// **The sampler keeps calling it anyway**, and that is not an oversight. A clip
/// can arrive from anywhere — a procedural generator, a network stream, a tool
/// that edited keys after this ran — and the cost of being wrong (a whole-body
/// 359-degree spin lasting one keyframe interval) is enormously out of proportion
/// to the cost of a comparison that predicts perfectly. What this function buys is
/// not the cycles; it is the COUNT, which tells you something about your content
/// pipeline that the runtime cannot.
///
/// Returns the number of keys it negated. Cross-fades between two clips are NOT
/// covered by this — two separately canonicalised clips can still disagree with
/// each other, which is what `pose_blend_report::flips` is for.
std::size_t canonicalise_rotations(clip& c);

/// Map any time onto the clip: wrapping if it loops, clamping if it does not.
///
/// **`std::fmod` is not this function, and the gap is a real bug.**
/// `std::fmod(-0.1f, 2.0f)` is `-0.1f`, not `1.9f` — C's `fmod` takes the sign of
/// the dividend. A clip played with a negative time, which happens the moment
/// anything scrubs backwards, runs time dilation below 1.0 with an offset, or
/// starts a cross-fade with a negative lead-in, then hands the sampler a time
/// before its first key.
///
/// **WHICH OF TWO DIFFERENT WRONG THINGS HAPPENS THEN IS DECIDED THREE FUNCTIONS
/// AWAY**, by whether the sampler clamps its normalised parameter. This one does,
/// so the character FREEZES on the clip's first pose for as long as the time
/// stays negative — a walk cycle that locks up for a tenth of a second every time
/// something nudges its clock below zero, and resumes perfectly afterwards.
/// Without that clamp the parameter goes negative instead and the pose
/// EXTRAPOLATES backwards out of the start of the clip, which is worse for
/// exactly the reason Lesson 7.5 gives: it is finite, it is smooth, it moves in a
/// plausible direction, and it is wrong. Section B of the harness measures both.
///
/// A duration of zero or NaN returns 0 rather than dividing.
[[nodiscard]] float wrap_time(const clip& c, float t);

/// Resize `cursors` to the clip and set every one of them to the start.
///
/// Call it when the clip changes, and after any seek large enough that walking
/// backwards from where the cursor is would cost more than starting over. Not
/// needed for an ordinary loop wrap: the backward walk in `sample` is bounded by
/// the track length and happens once per cycle, which §4 measures at a cost too
/// small to design around.
void reset_cursors(const clip& c, std::vector<track_cursor>& cursors);

/// **Sample the clip into a local pose** — the span every function in
/// `anim/skeleton.hpp` already takes.
///
/// `out` is resized to the skeleton's joint count and every joint is written, so
/// the result is always a complete, composable pose. A joint with no track, or a
/// track whose channel is empty, takes its value from `sk.joints[j].local_bind` —
/// which is what makes an upper-body clip a legal clip rather than a way to lose
/// the legs.
///
/// `time` is wrapped by `wrap_time` before anything else, so any float is a legal
/// argument and the caller's clock never has to know the duration.
///
/// `cursors` is grown to the clip's track count if it is short, so a default
/// `std::vector<track_cursor>{}` is a valid first call. It is read AND written:
/// this is the mutable half of playback and two characters must not share one.
void sample(const clip& c, const skeleton& sk, float time,
            std::vector<track_cursor>& cursors, std::vector<transform>& out);

/// The same sample, with a binary search instead of a cursor.
///
/// Identical output, and that identity is `verify_77.cpp` section C's whole job:
/// two different search strategies over the same data must agree to the bit, or
/// one of them is finding the wrong interval. Use it for one-off lookups — a
/// reference pose for an additive layer, a thumbnail at t = 0, the two samples
/// `validate` takes to measure the loop seam — where there is no previous frame
/// for a cursor to remember and `log2(n)` beats resetting one.
void sample_at(const clip& c, const skeleton& sk, float time, std::vector<transform>& out);

/// **Cross-fade: the pose `w` of the way from `a` to `b`**, joint by joint.
///
/// `w = 0` is `a`, `w = 1` is `b`, and each joint is `transform_blend`.
///
/// *** THIS IS NOT THE SAME WEIGHTED SUM AS LINEAR BLEND SKINNING, AND THE
/// DIFFERENCE IS THE POINT OF LESSON 7.7 §7. *** Both blend "two poses", and they
/// blend different objects. `anim/skin.hpp` sums MATRICES, which is forced on it —
/// a vertex is influenced by several joints at once and there is no other place
/// to put the weights — and the price is that the sum of two rotations is not a
/// rotation, so a twisted limb collapses to `cos(theta/2)` of its radius. This
/// function sums TRANSFORMS, one joint at a time, and a blended `transform` still
/// has a unit quaternion in it. So bone lengths are preserved EXACTLY, the
/// character cannot shrink, and the collapse that dominates §9 of Lesson 7.6
/// simply does not arise.
///
/// The cheap wrong version of this function blends the composed model matrices
/// instead — same inputs, one line later in the pipeline — and it is LBS one
/// level up, with the accumulated whole-chain angle in place of the per-joint
/// one. §7 measures a six-joint chain shortening by 0.46 units out of 5 on a
/// blend two poses are 100 degrees apart at the tip; the error grows down the
/// chain, so it is worst at hands and feet, which is where anything a player
/// holds is attached.
///
/// Spans of unequal length blend over the shorter and copy the rest from `a`,
/// which keeps a partial upper-body blend renderable.
pose_blend_report blend_poses(std::span<const transform> a, std::span<const transform> b,
                              float w, std::vector<transform>& out);

/// Drop every key the sampler could have reconstructed anyway.
///
/// A greedy fit-and-split over each channel independently: keep the first key,
/// then extend a candidate span as far as it can go while every key strictly
/// inside it is within `limits` of the straight-line (or nlerp) value the sampler
/// would have produced from the span's two endpoints; when one is not, close the
/// span at the previous key and start again from there.
///
/// **THE ERROR IS MEASURED WITH THE SAMPLER'S OWN RULE, and that is not
/// pedantry.** Reduction's entire job is to replace many small steps with one
/// large one, so it MANUFACTURES exactly the long arcs on which nlerp and slerp
/// disagree — the thing Lesson 7.5 measured at 0.13 degrees over 30 and 4.07 over
/// 90. Fit against slerp and play back with nlerp and the playback error can
/// exceed the tolerance you asked for, by a margin that grows with how well the
/// reduction worked. Section G of the harness measures it.
///
/// **Three stages, and only the first is the algorithm.** A channel whose keys
/// never depart from its first value by more than `limits` collapses to ONE key;
/// if that one value is also the joint's bind value, it collapses to NONE, and
/// the sampler's fallback reproduces it exactly. Those two stages are where the
/// bytes go on real content — a rig animates rotation nearly everywhere and
/// translation almost nowhere, so most of the 3N channels are not merely cheap
/// but absent — and they are why this takes the skeleton.
///
/// Returns the number of keys removed.
///
/// **NOT A FIXED POINT, AND MEASURED RATHER THAN ASSUMED.** A second pass fits
/// against the first pass's output rather than against the original curve, so in
/// principle the tolerances could add — a pipeline that reduces on export and
/// again on import would be spending the budget twice. Run to convergence on the
/// harness's walk clip it does not: pass two removes 4 more keys of 191, pass
/// three removes none, and the playback error stays at 0.4918 degrees to four
/// decimal places. The greedy anchors move; the curve they are approximating does
/// not. Worth knowing rather than worth fearing — but check it on your own
/// content before you chain two reducers, because the argument that it is safe is
/// a measurement and not a proof.
std::size_t reduce(clip& c, const skeleton& sk, const reduction_limits& limits);

/// What the keys weigh, in bytes, excluding the containers around them.
///
/// The exclusion is the interesting part and §9 spends a paragraph on it: three
/// `std::vector`s per joint are 72 bytes of headers on this implementation, and a
/// well-elided clip can carry MORE header than data. That is not an argument
/// against elision; it is an argument for a flat arena with offsets, which is
/// Module 9's custom-allocator lesson arriving early and uninvited.
[[nodiscard]] std::size_t key_bytes(const clip& c);

}   // namespace engine::anim
