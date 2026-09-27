// scratch/verify_511.cpp — Lesson 5.11's harness: the debug-line queue, the
// input seam, and the UI that is not there.
//
//   §A  the primitives: what each shape costs, in lines, and where they land
//   §B  LIFETIMES, and the two orderings that look identical until they do not
//   §C  the bound, and the counter that makes it visible
//   §D  the flush: geometry lands where the maths says, and the honest count
//   §E  THE MASK: a level can be withheld; a DELTA cannot, and the fix
//   §F  debug_ui with no window — the path every --shot in this repo takes
//   §G  the golden is still byte-identical
//
// §B AND §E ARE THE TWO THAT WOULD CATCH A PLAUSIBLE WRONG IMPLEMENTATION.
// Everything else checks behaviour that is hard to get wrong. §B checks an
// expiry rule whose obvious form leaks in exactly the situations a debug view is
// used in (a paused clock), and §E checks a cursor rule that is invisible until
// somebody drags a slider — which is to say, invisible in a first implementation
// and in the first test written for it.
//
// Build and run:  sh scratch/build_verify_511.sh

#include <engine/core/actions.hpp>
#include <engine/core/assert.hpp>
#include <engine/gfx/debug_draw.hpp>
#include <engine/gfx/debug_lines.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/math/transform.hpp>
#include <engine/ui/debug_ui.hpp>

#include "../demos/common/demo_scene.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>

namespace {

using engine::debug_lines;
using engine::vec3;

int g_checks = 0;
int g_failures = 0;

void check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok) { ++g_failures; }
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
}

void checkf(bool ok, const char* fmt, ...)
{
    char line[512];
    va_list args;
    va_start(args, fmt);
    SDL_vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    check(ok, line);
}

[[nodiscard]] std::string read_file(const char* path)
{
    std::size_t size = 0;
    void* data = SDL_LoadFile(path, &size);
    if (data == nullptr) { return {}; }
    std::string out(static_cast<const char*>(data), size);
    SDL_free(data);
    return out;
}

[[nodiscard]] bool near_eq(float a, float b, float eps = 1e-5f)
{
    return std::fabs(a - b) <= eps;
}

[[nodiscard]] bool same(vec3 a, vec3 b, float eps = 1e-5f)
{
    return near_eq(a.x, b.x, eps) && near_eq(a.y, b.y, eps) && near_eq(a.z, b.z, eps);
}

/// The same six-function device Lesson 5.10's harness used, unchanged — which is
/// itself a small result: `masked_input` wraps anything satisfying the concept,
/// so the fake that tested the map tests the mask too.
struct fake_input
{
    std::set<int> keys;
    std::set<int> buttons;
    float mx = 0.0f;
    float my = 0.0f;
    float wx = 0.0f;
    float wy = 0.0f;

    [[nodiscard]] bool key_down(SDL_Scancode k) const
    {
        return keys.count(static_cast<int>(k)) != 0;
    }
    [[nodiscard]] bool mouse_down(int b) const { return buttons.count(b) != 0; }
    [[nodiscard]] float mouse_x() const { return mx; }
    [[nodiscard]] float mouse_y() const { return my; }
    [[nodiscard]] float wheel_x() const { return wx; }
    [[nodiscard]] float wheel_y() const { return wy; }
};

static_assert(engine::input_snapshot<fake_input>);
static_assert(engine::input_snapshot<engine::masked_input<fake_input>>);

// ===========================================================================
//  §A — THE PRIMITIVES
// ===========================================================================

void section_a_primitives()
{
    std::printf("\n=== A. The primitives ===\n");

    debug_lines q;
    check(q.size() == 0 && q.dropped() == 0, "a fresh queue is empty and has dropped nothing");
    checkf(q.capacity() == engine::k_default_debug_line_capacity,
           "default capacity %zu lines", q.capacity());

    q.line({0.0f, 0.0f, 0.0f}, {1.0f, 2.0f, 3.0f}, 0xFF112233u);
    check(q.size() == 1, "line() queues exactly one segment");
    check(same(q.lines()[0].a, {0.0f, 0.0f, 0.0f})
              && same(q.lines()[0].b, {1.0f, 2.0f, 3.0f}),
          "…with the endpoints it was given, in world space, untransformed");
    check(q.lines()[0].colour == 0xFF112233u && q.lines()[0].remaining == 0.0f,
          "…and the colour it was given, with the default single-frame lifetime");

    q.clear();
    q.ray({1.0f, 0.0f, 0.0f}, {0.0f, 4.0f, 0.0f}, 0xFFFFFFFFu);
    check(q.size() == 1 && same(q.lines()[0].b, {1.0f, 4.0f, 0.0f}),
          "ray() ends at origin + direction — the shape a normal or a velocity wants");

    // ---- axes: three lines, in the course's colours, from the matrix's columns
    q.clear();
    const engine::transform t{.position = {5.0f, 0.0f, 0.0f},
                              .rotation = engine::mat3::identity(),
                              .scale = {1.0f, 1.0f, 1.0f}};
    q.axes(engine::parent_from_local(t), 2.0f);
    check(q.size() == 3, "axes() is exactly three lines");
    check(q.lines()[0].colour == engine::k_axis_x_colour
              && q.lines()[1].colour == engine::k_axis_y_colour
              && q.lines()[2].colour == engine::k_axis_z_colour,
          "…in x/y/z = red/green/blue, the course convention, from ONE set of constants");
    check(same(q.lines()[0].a, {5.0f, 0.0f, 0.0f})
              && same(q.lines()[0].b, {7.0f, 0.0f, 0.0f}),
          "…rooted at the transform's position, length 2 along its own x");

    // A rotation the arrows must follow. Ninety degrees about z sends x to +y.
    q.clear();
    const engine::transform spun{.position = {},
                                 .rotation = engine::rotation_z(1.5707963268f),
                                 .scale = {1.0f, 1.0f, 1.0f}};
    q.axes(engine::parent_from_local(spun), 1.0f);
    check(same(q.lines()[0].b, {0.0f, 1.0f, 0.0f}, 1e-4f),
          "the x arrow follows the ROTATION — 90 deg about z sends it to +y, which is the "
          "w = 0 rule working; sent as a point it would also pick up the translation");

    // ---- boxes
    q.clear();
    q.box(vec3{0.0f, 0.0f, 0.0f}, vec3{1.0f, 1.0f, 1.0f}, 0xFFFFFFFFu);
    check(q.size() == 12, "an axis-aligned box is 12 edges, not 8 corners and not 24 halves");

    std::size_t along_x = 0;
    for (const engine::debug_line& l : q.lines())
    {
        if (near_eq(l.a.y, l.b.y) && near_eq(l.a.z, l.b.z)) { ++along_x; }
    }
    check(along_x == 4, "…four of them along each axis, which is what the edge table encodes");

    q.clear();
    const engine::transform half{.position = {10.0f, 0.0f, 0.0f},
                                 .rotation = engine::mat3::identity(),
                                 .scale = {0.5f, 0.5f, 0.5f}};
    q.box(engine::parent_from_local(half), vec3{1.0f, 1.0f, 1.0f}, 0xFFFFFFFFu);
    bool inside = true;
    for (const engine::debug_line& l : q.lines())
    {
        inside = inside && near_eq(std::fabs(l.a.x - 10.0f), 0.5f)
                 && near_eq(std::fabs(l.a.y), 0.5f) && near_eq(std::fabs(l.a.z), 0.5f);
    }
    check(inside, "the oriented box transforms its CORNERS — a half-unit box at x = 10 under a "
                  "0.5 scale, not an axis-aligned box that forgot to rotate");

    // ---- sphere
    q.clear();
    q.sphere({0.0f, 0.0f, 0.0f}, 3.0f, 0xFFFFFFFFu, 0.0f, 16);
    check(q.size() == 48, "a sphere is 3 great circles of `segments` — 3 x 16 = 48 lines");
    bool on_radius = true;
    for (const engine::debug_line& l : q.lines())
    {
        on_radius = on_radius && near_eq(engine::length(l.a), 3.0f, 1e-3f);
    }
    check(on_radius, "…every vertex exactly `radius` from the centre");

    q.clear();
    q.sphere({0.0f, 0.0f, 0.0f}, 1.0f, 0xFFFFFFFFu, 0.0f, 1);
    check(q.size() == 9, "segments is clamped to a minimum of 3 (3 x 3 = 9), because a "
                         "one-segment circle is a degenerate line, not a smaller sphere");

    // ---- wire_mesh
    q.clear();
    const engine::mesh cube = engine::cube_mesh();
    q.wire_mesh(cube.vertices, cube.indices, engine::mat4::identity(), 0xFFFFFFFFu);
    checkf(q.size() == cube.indices.size(),
           "wire_mesh queues 3 edges per triangle: %zu indices -> %zu lines, shared edges "
           "twice, exactly as draw_mesh has drawn them since Lesson 2.8",
           cube.indices.size(), q.size());

    // A malformed mesh is the one a debug drawer is most likely to be handed,
    // because a broken mesh is WHY you reached for a wireframe.
    q.clear();
    const std::uint16_t bad[3] = {0, 1, 9999};
    q.wire_mesh(cube.vertices, bad, engine::mat4::identity(), 0xFFFFFFFFu);
    check(q.size() == 1,
          "an out-of-range index costs only the two edges that TOUCH it — the third, (0,1), is "
          "well-formed and is drawn. Skipping the whole triangle would be defensible and is "
          "worse: you would be told nothing about the part of the mesh that is fine");
    check(same(q.lines()[0].a, cube.vertices[0]) && same(q.lines()[0].b, cube.vertices[1]),
          "…and it is that edge, not some clamped stand-in — a debug drawer must never invent "
          "geometry, because the invented geometry is what you would then go and debug");
}

// ===========================================================================
//  §B — LIFETIMES
// ===========================================================================

void section_b_lifetimes()
{
    std::printf("\n=== B. Lifetimes ===\n");

    // ---- the default: drawn once, then gone
    debug_lines q;
    q.line({}, {1.0f, 0.0f, 0.0f}, 0xFFFFFFFFu);          // seconds omitted
    check(q.size() == 1, "queued");
    q.advance(1.0f / 60.0f);
    check(q.size() == 0, "a default-lifetime line is gone after ONE advance — it was drawn on "
                         "the frame it was queued and never again");

    // ---- THE dt == 0 CASE, which is the whole reason the test precedes the
    //      subtraction. A paused clock, a single-frame --shot, a breakpoint.
    q.clear();
    q.line({}, {1.0f, 0.0f, 0.0f}, 0xFFFFFFFFu);
    q.advance(0.0f);
    check(q.size() == 0, "…and gone even when dt is EXACTLY ZERO. Written the obvious way — "
                         "subtract, then test — this line would live forever, and it would do "
                         "it while the clock is paused, which is when you are looking hardest");

    // ---- a real lifetime survives the frames it was asked for
    q.clear();
    q.line({}, {1.0f, 0.0f, 0.0f}, 0xFFFFFFFFu, 0.5f);
    int frames = 0;
    while (q.size() > 0 && frames < 1000)
    {
        q.advance(1.0f / 60.0f);
        ++frames;
    }
    checkf(frames == 31, "a 0.5 s line at 60 Hz survives %d frames — 30 to spend its time and "
                         "one more on which its clock has already reached zero. Two seconds "
                         "plus one frame is the right way round to be wrong",
           frames);

    // ---- what a human can actually see
    const float one_frame_ms = 1000.0f / 60.0f;
    checkf(one_frame_ms < 20.0f,
           "the argument for lifetimes in one number: a single-frame marker is on screen for "
           "%.1f ms, and a person needs something on the order of 250 ms to notice it",
           static_cast<double>(one_frame_ms));

    // ---- THE ORDERING TRAP, as a measurement rather than a warning
    q.clear();
    q.line({}, {1.0f, 0.0f, 0.0f}, 0xFFFFFFFFu);
    q.line({}, {0.0f, 1.0f, 0.0f}, 0xFFFFFFFFu);
    q.line({}, {0.0f, 0.0f, 1.0f}, 0xFFFFFFFFu);
    const std::size_t before_flush = q.size();
    q.advance(1.0f / 60.0f);                       // the WRONG order: age, then draw
    checkf(before_flush == 3 && q.size() == 0,
           "advance() BEFORE the flush leaves %zu of 3 lines to draw. This is the ordering bug, "
           "and its symptom is a debug system that draws nothing — which reads as 'my code did "
           "not run' and sends you looking in the wrong place entirely",
           q.size());

    // ---- mixed lifetimes: the survivors keep their order
    q.clear();
    q.line({1.0f, 0.0f, 0.0f}, {}, 0xAAu, 0.0f);
    q.line({2.0f, 0.0f, 0.0f}, {}, 0xBBu, 1.0f);
    q.line({3.0f, 0.0f, 0.0f}, {}, 0xCCu, 0.0f);
    q.line({4.0f, 0.0f, 0.0f}, {}, 0xDDu, 1.0f);
    q.advance(1.0f / 60.0f);
    check(q.size() == 2 && near_eq(q.lines()[0].a.x, 2.0f) && near_eq(q.lines()[1].a.x, 4.0f),
          "a compacting sweep keeps the survivors in order — the 2nd and 4th, in that order, "
          "which is what makes a frame's drawing stable rather than shuffling every time "
          "something expires");
}

// ===========================================================================
//  §C — THE BOUND
// ===========================================================================

void section_c_bound()
{
    std::printf("\n=== C. The bound ===\n");

    debug_lines q(10);
    checkf(q.capacity() == 10, "a queue can be built with any capacity: %zu", q.capacity());

    for (int i = 0; i < 25; ++i) { q.line({}, {1.0f, 0.0f, 0.0f}, 0xFFFFFFFFu); }
    check(q.size() == 10 && q.full(), "it stops at its capacity rather than growing");
    checkf(q.dropped() == 15, "…and COUNTS what it refused: %zu. A bound with no counter is a "
                              "bug that presents as a rendering artifact — the one line you "
                              "are hunting is missing, and nothing distinguishes that from the "
                              "thing not existing",
           q.dropped());

    // The bound is checked in one place, so a compound shape is subject to it too
    // — and is not partially emitted in some special way. Twelve edges into two
    // remaining slots is two edges and ten drops.
    debug_lines r(2);
    r.box(vec3{}, vec3{1.0f, 1.0f, 1.0f}, 0xFFFFFFFFu);
    check(r.size() == 2 && r.dropped() == 10,
          "every primitive funnels through one push(), so a 12-edge box into 2 free slots is "
          "2 kept and 10 dropped — no primitive can forget to ask");

    r.clear();
    check(r.size() == 0 && r.dropped() == 0, "clear() forgets the lines AND the drop counter");
}

// ===========================================================================
//  §D — THE FLUSH
// ===========================================================================

void section_d_flush()
{
    std::printf("\n=== D. The flush ===\n");

    constexpr int W = 200;
    constexpr int H = 100;
    engine::framebuffer fb(W, H);

    // An orthographic-ish setup would hide arithmetic mistakes; a real
    // perspective projector is what the demo uses, so it is what this checks.
    const engine::mat4 proj = engine::perspective(1.0f, static_cast<float>(W)
                                                            / static_cast<float>(H),
                                                  0.1f, 100.0f);
    const engine::projector pr{proj,
                               engine::viewport{0.0f, 0.0f, static_cast<float>(W),
                                                static_cast<float>(H), 0.0f, 1.0f},
                               engine::near_mode::clip};

    // Camera at the origin looking down -z, which makes the view matrix the
    // identity and keeps this test about the QUEUE rather than about look_at.
    const engine::mat4 view = engine::mat4::identity();

    debug_lines q;
    q.line({-1.0f, 0.0f, -4.0f}, {1.0f, 0.0f, -4.0f}, 0xFFFF0000u);

    fb.clear(0xFF000000u);
    const int drawn = engine::draw_debug_lines(fb, view, pr, q);
    check(drawn == 1, "one queued line, one drawn");

    // A horizontal line through the origin at z = -4 must land on the middle row.
    int painted = 0;
    int min_x = W;
    int max_x = -1;
    for (int y = 0; y < H; ++y)
    {
        for (int x = 0; x < W; ++x)
        {
            if (fb.row(y)[x] == 0xFFFF0000u)
            {
                ++painted;
                if (y != H / 2) { painted += 100000; }   // wrong row: fail loudly
                min_x = std::min(min_x, x);
                max_x = std::max(max_x, x);
            }
        }
    }
    checkf(painted > 0 && painted < 1000,
           "…and it painted %d pixels, every one of them on row %d — the centre, which is where "
           "y = 0 goes",
           painted, H / 2);

    // Where the ends land, by hand. x_ndc = x * cot(fovy/2) / (aspect * -z_view),
    // then screen = (ndc + 1) * w/2. cot(0.5) = 1.830488, aspect = 2, z = -4:
    //   x_ndc = 1 * 1.830488 / (2 * 4) = 0.228811
    //   screen = (1 + 0.228811) * 100 = 122.88  ->  pixel 122 or 123
    const float cot_half = 1.0f / std::tan(0.5f);
    const float expect_x = (1.0f + cot_half / (2.0f * 4.0f)) * (W / 2.0f);
    checkf(std::fabs(static_cast<float>(max_x) - expect_x) <= 1.5f,
           "the right-hand end landed at x = %d and the projection says %.2f — the debug line "
           "goes through the same maths as the scene, because it goes through the same "
           "line3_world",
           max_x, static_cast<double>(expect_x));
    check(std::abs((W - 1 - max_x) - min_x) <= 1,
          "…and the two ends are symmetric about the centre column");

    // ---- THE HONEST COUNT. A line entirely behind the eye survives nothing.
    q.clear();
    q.line({-1.0f, 0.0f, 4.0f}, {1.0f, 0.0f, 4.0f}, 0xFFFF0000u);   // +z is BEHIND
    fb.clear(0xFF000000u);
    check(engine::draw_debug_lines(fb, view, pr, q) == 0,
          "a line behind the near plane reports 0 drawn, so a HUD reading 'queued 152, drawn "
          "151' names a real event instead of printing the queue size twice");

    // ---- and one that straddles it is clipped, not dropped
    q.clear();
    q.line({0.0f, 0.0f, 4.0f}, {0.0f, 0.0f, -4.0f}, 0xFFFF0000u);
    fb.clear(0xFF000000u);
    check(engine::draw_debug_lines(fb, view, pr, q) == 1,
          "…while one that STRADDLES it is clipped and drawn — Lesson 3.3's clipper, reached "
          "through the queue without the queue knowing it exists");

    // ---- the flush does not consume the queue
    check(q.size() == 1, "a flush is a read: the queue still holds its line afterwards, so a "
                         "split-screen game can flush the same queue twice");
}

// ===========================================================================
//  §E — THE MASK
// ===========================================================================

void section_e_mask()
{
    std::printf("\n=== E. The mask ===\n");

    engine::action_map map;
    const engine::action_id fire = map.declare("fire");
    const engine::action_id look = map.declare("look_x");
    map.bind_key(fire, SDL_SCANCODE_SPACE);
    map.bind_mouse_axis(look, engine::mouse_axis::x, 1.0f);

    fake_input in;
    engine::masked_input<fake_input> gate;

    // ---- a LEVEL can be withheld, and withholding it produces the right EDGE
    in.keys.insert(SDL_SCANCODE_SPACE);
    gate.update(in, false, false);
    map.update(gate);
    check(map.held(fire) && map.pressed(fire), "unblocked: the key is held and the press fired");

    gate.update(in, true, false);            // a text field takes focus
    map.update(gate);
    check(!map.held(fire), "blocked: the key is reported UP even though it is physically down");
    check(map.released(fire),
          "…and the RELEASE edge fires, which is the behaviour you want: clicking into a text "
          "field should let go of the movement keys. Skipping update() instead would freeze "
          "the level and the camera would fly away while you type");

    in.keys.erase(SDL_SCANCODE_SPACE);       // released while the UI had focus
    gate.update(in, false, false);
    map.update(gate);
    check(!map.held(fire) && !map.pressed(fire),
          "a key released DURING the block produces no phantom press when the block lifts — "
          "because `input` was fed every event all along; only the map's view was masked");

    // ---- the wheel is already a delta, so zeroing it is exact
    in.wy = 3.0f;
    const engine::action_id zoom = map.declare("zoom");
    map.bind_mouse_axis(zoom, engine::mouse_axis::wheel_y, 1.0f);
    gate.update(in, false, true);
    map.update(gate);
    check(map.value(zoom) == 0.0f, "a blocked wheel contributes exactly 0");
    gate.update(in, false, false);
    map.update(gate);
    check(near_eq(map.value(zoom), 3.0f), "…and an unblocked one contributes the whole notch");

    // =======================================================================
    //  THE WORKED EXAMPLE FROM §6, RUN AS A TEST
    // =======================================================================
    //
    // The cursor sits at x = 100. The UI takes the mouse for three frames while
    // it travels to 160. Then the UI lets go and it moves on to 170.
    //
    //   naive "report 0 while blocked"      -> the release frame sees +170
    //   naive "freeze the last position"    -> the release frame sees +70
    //   the virtual cursor                  -> the release frame sees +10
    //
    // Only the third is the distance the mouse actually moved during a frame the
    // game was listening.
    engine::action_map m2;
    const engine::action_id look2 = m2.declare("look_x");
    m2.bind_mouse_axis(look2, engine::mouse_axis::x, 1.0f);

    fake_input mouse;
    engine::masked_input<fake_input> vg;

    mouse.mx = 100.0f;
    vg.update(mouse, false, false);
    m2.update(vg);                                   // frame 0: latch, no delta

    mouse.mx = 120.0f;
    vg.update(mouse, false, true);
    m2.update(vg);
    checkf(m2.value(look2) == 0.0f && near_eq(vg.cursor_offset_x(), 20.0f),
           "blocked frame 1: 100 -> 120 real, delta 0, offset %.0f",
           static_cast<double>(vg.cursor_offset_x()));

    mouse.mx = 145.0f;
    vg.update(mouse, false, true);
    m2.update(vg);
    checkf(m2.value(look2) == 0.0f && near_eq(vg.cursor_offset_x(), 45.0f),
           "blocked frame 2: 120 -> 145 real, delta 0, offset %.0f",
           static_cast<double>(vg.cursor_offset_x()));

    mouse.mx = 160.0f;
    vg.update(mouse, false, true);
    m2.update(vg);
    checkf(m2.value(look2) == 0.0f && near_eq(vg.cursor_offset_x(), 60.0f)
               && near_eq(vg.mouse_x(), 100.0f),
           "blocked frame 3: 145 -> 160 real, delta 0, offset %.0f — and the reported cursor "
           "has not moved from 100 since the block began",
           static_cast<double>(vg.cursor_offset_x()));

    mouse.mx = 170.0f;
    vg.update(mouse, false, false);                  // the UI lets go
    m2.update(vg);
    checkf(near_eq(m2.value(look2), 10.0f),
           "THE RELEASE FRAME: 160 -> 170 real, and the action sees %.0f — one frame's real "
           "movement. Report 0 while blocked and it would be 170; freeze the position and it "
           "would be 70, which is the version that ships and whips the camera round",
           static_cast<double>(m2.value(look2)));
    checkf(near_eq(vg.cursor_offset_x(), 60.0f),
           "…and the offset stopped growing the moment the block lifted: still %.0f",
           static_cast<double>(vg.cursor_offset_x()));

    mouse.mx = 175.0f;
    vg.update(mouse, false, false);
    m2.update(vg);
    check(near_eq(m2.value(look2), 5.0f),
          "…and every frame after it is an ordinary delta again, permanently 60 px behind the "
          "real cursor and permanently correct about how far it moved");
}

// ===========================================================================
//  §F — THE UI THAT IS NOT THERE
// ===========================================================================

void section_f_headless_ui()
{
    std::printf("\n=== F. debug_ui with no window ===\n");

    engine::debug_ui ui;
    check(!ui.running(), "a fresh debug_ui is not running");
    check(!ui.start(nullptr, nullptr),
          "start() with no window returns false — which is `--shot` and every headless run in "
          "this repository, and is a configuration rather than a failure");
    check(!ui.running(), "…and it stays not-running");

    // EVERY CALL IS A SAFE NO-OP, which is the property that lets a program be
    // written once and run on all three surfaces. If any of these faulted, every
    // demo would need an `if` around its whole UI.
    ui.begin_frame();
    ui.render();
    SDL_Event e{};
    e.type = SDL_EVENT_KEY_DOWN;
    check(!ui.handle_event(e), "handle_event is a no-op and reports that it used nothing");
    check(!ui.wants_keyboard() && !ui.wants_mouse() && !ui.wants_text(),
          "and it claims neither the keyboard, the mouse nor text — so `masked_input` blocks "
          "nothing and the game behaves exactly as it did before this lesson");
    ui.stop();
    check(!ui.running(), "stop() on a UI that never started is safe and idempotent");

    checkf(ui.version() != nullptr && ui.version()[0] != '\0',
           "the ImGui version is a BUILD fact, available with no context: %s", ui.version());
}

// ===========================================================================
//  §G — THE GOLDEN
// ===========================================================================

void section_g_golden()
{
    std::printf("\n=== G. The golden ===\n");

    const char* path = "build/demos/verify_511.ppm";
    check(demo::write_reference_shot(path) == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file(path);
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(), "same size: %zu bytes", ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL to the golden written in Lesson 5.1 — ELEVEN lessons. 5.11 split "
          "debug_draw in two, moved the axis colours into shared constants and gave line3() a "
          "return value, and the reference scene is unchanged to the last bit");
}

}   // namespace

int main()
{
    std::printf("verify_511 — Lesson 5.11: ImGui and the debug-draw system\n");
    std::printf("(debug assertions %s)\n",
                engine::debug_assertions_enabled() ? "ON" : "OFF");

    section_a_primitives();
    section_b_lifetimes();
    section_c_bound();
    section_d_flush();
    section_e_mask();
    section_f_headless_ui();
    section_g_golden();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
