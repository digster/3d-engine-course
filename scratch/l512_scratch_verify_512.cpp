// scratch/verify_512.cpp — Lesson 5.12's harness.
//
//   sh scratch/build_verify_512.sh
//
// SIX SECTIONS, and what each is for:
//
//   §A  collect_renderables does what the header says, including the two ways an
//       entity can be absent from the picture while looking correct in the
//       registry.
//   §B  the matrix -> transform conversion is EXACT, not approximately exact.
//       The header calls it "a trick rather than a design"; a trick that is
//       merely close would be a bug.
//   §C  the view still leads with the smallest pool after the hoist.
//   §D  the three geometry conventions that bit this lesson three times, pinned
//       as facts so the next person only has to read a test.
//   §E  the camera chain, INCLUDING the bug that cost this lesson its first
//       crash — reproduced, with a control that fires.
//   §F  what demos/collector/main.cpp says about itself: every include is a
//       public engine header, and engine.hpp lists every public header.
//
// THIS FILE IS ERA-NEUTRAL ON PURPOSE. It names no field that Module 6 moves, so
// the same text compiles against the engine of Lesson 5.11 and the engine of
// today. That is not a convenience — it is what lets §A's claims be checked
// against both.

// THE UMBRELLA, COMPILED — and this harness is the first thing in the
// repository to do it. `engine.hpp` had no consumer at all: nothing under
// demos/ or engine/src includes it, which is exactly why it could list 40 of 55
// public headers for thirteen lessons without anyone noticing. A header nobody
// compiles cannot be kept correct by being used; §F checks its CONTENTS, and
// this line checks that the fifteen headers added to it actually compile
// together, in that order, with no include cycle.
#include <engine/engine.hpp>

#include <engine/ecs/camera.hpp>
#include <engine/ecs/hierarchy.hpp>
#include <engine/ecs/registry.hpp>
#include <engine/ecs/view.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/renderable.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/transform.hpp>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int checks = 0;
int failures = 0;

void check(bool ok, const char* what)
{
    ++checks;
    if (!ok) { ++failures; }
    std::printf("  %-4s %s\n", ok ? "ok" : "FAIL", what);
}

void checkf(bool ok, const char* fmt, ...)
{
    ++checks;
    if (!ok) { ++failures; }
    std::printf("  %-4s ", ok ? "ok" : "FAIL");
    va_list args;
    va_start(args, fmt);
    std::vprintf(fmt, args);
    va_end(args);
    std::printf("\n");
}

void heading(const char* text) { std::printf("\n%s\n", text); }

/// The largest absolute difference between two 4x4s, element by element.
[[nodiscard]] float max_abs_diff(const engine::mat4& a, const engine::mat4& b)
{
    float worst = 0.0f;
    for (int c = 0; c < 4; ++c)
    {
        for (int r = 0; r < 4; ++r)
        {
            worst = std::fmax(worst, std::fabs(a.at(r, c) - b.at(r, c)));
        }
    }
    return worst;
}

[[nodiscard]] std::string read_file(const char* path)
{
    std::ifstream f(path);
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}

}   // namespace

int main()
{
    using namespace engine;
    using engine::ecs::entity;

    // A pool with one real mesh in it, and one handle that was never valid.
    mesh_pool meshes;
    const mesh_handle cube = meshes.insert(with_normals(cube_mesh(), normal_style::flat));
    const mesh_handle nowhere{};

    // -----------------------------------------------------------------------
    heading("A  collect_renderables: what it draws, and what it counts instead");
    // -----------------------------------------------------------------------
    {
        ecs::registry world;
        ecs::hierarchy tree;
        std::vector<scene_object> out;

        // THREE ENTITIES, THREE OUTCOMES, and the second and third are the ones
        // worth having a function for: both look perfectly correct in the
        // registry and neither appears in the picture.
        const entity drawn = world.create();
        ecs::add_hierarchy_components(world, drawn, transform{});
        world.add<renderable>(drawn, renderable{.mesh = cube});

        const entity unresolved = world.create();          // no world_transform
        world.add<renderable>(unresolved, renderable{.mesh = cube});

        const entity dead_mesh = world.create();
        ecs::add_hierarchy_components(world, dead_mesh, transform{});
        world.add<renderable>(dead_mesh, renderable{.mesh = nowhere});

        // And one with a transform and NO renderable, which must be invisible
        // without being counted as anything — it is not a problem, it is an
        // entity that is not a thing you can see (Lesson 5.8).
        const entity invisible = world.create();
        ecs::add_hierarchy_components(world, invisible, transform{});

        (void)tree.rebuild_and_resolve(world);
        const renderable_report r = collect_renderables(world, meshes, out);

        checkf(r.drawn == 1, "drawn = %zu (expected 1)", r.drawn);
        checkf(out.size() == 1, "out.size() = %zu (expected 1)", out.size());
        checkf(r.unresolved == 1, "unresolved = %zu (expected 1)", r.unresolved);
        checkf(r.missing_mesh == 1, "missing_mesh = %zu (expected 1)", r.missing_mesh);
        check(world.alive(invisible), "an entity with no renderable is simply absent");

        // CALLED TWICE, SAME ANSWER. `out` is cleared and refilled, so a caller
        // that keeps one vector across frames must not accumulate.
        const renderable_report again = collect_renderables(world, meshes, out);
        checkf(again.drawn == r.drawn && out.size() == 1,
               "a second call refills rather than appends (out.size() = %zu)", out.size());
    }

    // -----------------------------------------------------------------------
    heading("B  the matrix -> transform conversion is EXACT");
    // -----------------------------------------------------------------------
    {
        // The header claims the round trip is bit-exact because
        // `transform::rotation` is a general mat3 with no orthonormality
        // requirement, so the whole linear part fits in it. If that is merely
        // approximate, every object in every scene is slightly wrong and nothing
        // says so — which is why this is its own section.
        ecs::registry world;
        ecs::hierarchy tree;
        std::vector<scene_object> out;

        const transform local{.position = {1.25f, -3.5f, 7.75f},
                              .rotation = rotation_y(0.7f) * rotation_x(-0.31f)
                                          * rotation_z(1.9f),
                              .scale = {2.5f, 0.125f, 1.75f}};

        const entity e = world.create();
        ecs::add_hierarchy_components(world, e, local);
        world.add<renderable>(e, renderable{.mesh = cube});
        (void)tree.rebuild_and_resolve(world);

        const ecs::world_transform* w = world.get<ecs::world_transform>(e);
        check(w != nullptr, "the entity resolved");

        (void)collect_renderables(world, meshes, out);
        check(out.size() == 1, "one object came back");

        const mat4 original = w->matrix;
        const mat4 recomposed = parent_from_local(out[0].xform);
        const float worst = max_abs_diff(original, recomposed);
        checkf(worst == 0.0f, "max |element difference| = %.9g (expected exactly 0)",
               static_cast<double>(worst));

        // AND THE CONTROL, because "the difference is zero" is also what you get
        // from comparing something with itself. Perturb one element of the
        // source transform and the same comparison must report a difference —
        // otherwise this section is measuring nothing.
        transform bent = out[0].xform;
        bent.position.x += 1.0f;
        const float control = max_abs_diff(original, parent_from_local(bent));
        checkf(control > 0.5f, "CONTROL: a 1.0 nudge is reported as %.6g (must be > 0.5)",
               static_cast<double>(control));
    }

    // -----------------------------------------------------------------------
    heading("C  the view still leads with the smallest pool");
    // -----------------------------------------------------------------------
    {
        // Lesson 5.8's property, re-checked here because the hoist moved the
        // loop into the engine and a caller can no longer see which pool it
        // walks. A scene is mostly transforms — walls, floors, trigger volumes,
        // spawn points — and only some of them are things you can see.
        ecs::registry world;
        ecs::hierarchy tree;

        constexpr int k_transforms = 120;
        constexpr int k_renderables = 9;
        for (int i = 0; i < k_transforms; ++i)
        {
            const entity e = world.create();
            ecs::add_hierarchy_components(world, e, transform{});
            if (i < k_renderables) { world.add<renderable>(e, renderable{.mesh = cube}); }
        }
        (void)tree.rebuild_and_resolve(world);

        const auto v = world.view<ecs::world_transform, renderable>();
        checkf(v.lead() == 1, "lead pool index = %zu (expected 1, the renderables)",
               v.lead());
        checkf(v.size_hint() == static_cast<std::size_t>(k_renderables),
               "candidates walked = %zu, not %d", v.size_hint(),
               k_transforms);

        std::vector<scene_object> out;
        const renderable_report r = collect_renderables(world, meshes, out);
        checkf(r.drawn == static_cast<std::size_t>(k_renderables),
               "drawn = %zu of %d entities", r.drawn, k_transforms);
    }

    // -----------------------------------------------------------------------
    heading("D  the three geometry conventions that bit this lesson");
    // -----------------------------------------------------------------------
    {
        // Pinned as facts, because prose in a comment is a thing you have to
        // find and a test is a thing that finds you.

        float cube_extent = 0.0f;
        for (const vec3& v : k_cube_vertices)
        {
            cube_extent = std::fmax(cube_extent,
                                    std::fmax(std::fabs(v.x),
                                              std::fmax(std::fabs(v.y), std::fabs(v.z))));
        }
        checkf(std::fabs(cube_extent - 0.5f) < 1e-6f,
               "cube_mesh spans +/-%.4f, so `scale` is the box's FULL SIZE",
               static_cast<double>(cube_extent));

        float icos_radius_min = 1e9f;
        float icos_radius_max = 0.0f;
        for (const vec3& v : k_icosahedron_vertices)
        {
            const float r = length(v);
            icos_radius_min = std::fmin(icos_radius_min, r);
            icos_radius_max = std::fmax(icos_radius_max, r);
        }
        checkf(std::fabs(icos_radius_max - 1.0f) < 1e-5f
                   && std::fabs(icos_radius_min - 1.0f) < 1e-5f,
               "icosahedron_mesh has circumradius %.6f, so `scale` is its RADIUS",
               static_cast<double>(icos_radius_max));

        // AND THEREFORE THE TWO DISAGREE BY A FACTOR OF TWO. Stated as its own
        // check because it is the fact a caller trips over, and neither of the
        // two above says it on its own.
        const float cube_diameter = 2.0f * cube_extent;
        const float icos_diameter = 2.0f * icos_radius_max;
        checkf(std::fabs(icos_diameter / cube_diameter - 2.0f) < 1e-5f,
               "at scale 1 an icosahedron is %.1fx the diameter of a cube",
               static_cast<double>(icos_diameter / cube_diameter));
    }

    // -----------------------------------------------------------------------
    heading("E  the camera chain, and the bug that cost this lesson a crash");
    // -----------------------------------------------------------------------
    {
        // The game parents its camera to a boom, and the boom to the rover. The
        // rover is SCALED and ROLLS, and `view_from_camera` asserts its input is
        // rigid — so the boom's job is to leave the chain with the rover's
        // heading and nothing else.
        //
        // The arithmetic: the rover's basis is H * Rz(bank) * S, so
        //     local = (H * Rz(bank) * S)^-1 * H = S^-1 * Rz(-bank)
        // and the inverse of a product REVERSES the order. Writing the operands
        // the way they read in English gives Rz(-bank) * S^-1, which is a
        // different matrix because a rotation and a non-uniform scale do not
        // commute.
        const vec3 half{0.30f, 0.22f, 0.42f};
        const mat3 unscale{{1.0f / (2.0f * half.x), 0.0f, 0.0f},
                           {0.0f, 1.0f / (2.0f * half.y), 0.0f},
                           {0.0f, 0.0f, 1.0f / (2.0f * half.z)}};
        constexpr float bank = 0.31f;
        const mat3 heading_rot = rotation_y(1.1f);

        auto chain = [&](const mat3& boom_local) {
            ecs::registry world;
            ecs::hierarchy tree;

            const entity rover = world.create();
            ecs::add_hierarchy_components(
                world, rover,
                transform{.position = {2.0f, half.y, -1.0f},
                          .rotation = heading_rot * rotation_z(bank),
                          .scale = {2.0f * half.x, 2.0f * half.y, 2.0f * half.z}});

            const entity boom = world.create();
            ecs::add_hierarchy_components(world, boom,
                                          transform{.rotation = boom_local});
            (void)ecs::set_parent(world, boom, rover);

            const entity cam = world.create();
            ecs::add_hierarchy_components(
                world, cam,
                ecs::look_along({0.0f, 4.4f, -6.8f}, {0.0f, 0.2f, 4.5f},
                                {0.0f, 1.0f, 0.0f}));
            world.add<ecs::camera>(cam, ecs::camera{});
            (void)ecs::set_parent(world, cam, boom);

            (void)tree.rebuild_and_resolve(world);
            return world.get<ecs::world_transform>(cam)->matrix;
        };

        const mat4 right_way = chain(unscale * rotation_z(-bank));
        const mat4 wrong_way = chain(rotation_z(-bank) * unscale);
        const mat4 no_boom = chain(mat3::identity());

        check(is_rigid(right_way),
              "S^-1 * Rz(-bank): the camera's placement is rigid, so view_from_camera "
              "is defined");
        check(!is_rigid(wrong_way),
              "Rz(-bank) * S^-1: NOT rigid — the operands swapped, which is the "
              "crash this lesson shipped first");
        check(!is_rigid(no_boom),
              "CONTROL: with no boom at all the camera inherits the rover's scale "
              "and is not rigid either");

        // AND THE TWO WRONG ONES ARE WRONG DIFFERENTLY, which matters because a
        // single "not rigid" result would be equally consistent with the boom
        // doing nothing at all.
        const float apart = max_abs_diff(wrong_way, no_boom);
        checkf(apart > 0.01f,
               "the swapped boom and the absent boom differ by %.4f, so the boom "
               "IS doing something in the wrong case", static_cast<double>(apart));

        // The heading survives: the camera still sits behind the rover along the
        // rover's own forward axis. Checked as a distance, because "rigid" alone
        // would be satisfied by a camera pointing anywhere at all.
        const vec3 cam_pos = translation_of(right_way);
        const vec3 rover_pos{2.0f, half.y, -1.0f};
        const vec3 offset = cam_pos - rover_pos;
        const vec3 want = heading_rot * vec3{0.0f, 4.4f, -6.8f};
        checkf(length(offset - want) < 1e-4f,
               "the camera sits at H * (0, 4.4, -6.8) from the rover "
               "(error %.3g m)", static_cast<double>(length(offset - want)));
    }

    // -----------------------------------------------------------------------
    heading("F  what the files say about themselves");
    // -----------------------------------------------------------------------
    {
        // THE BOUNDARY. Lesson 5.1 made a private include a compile error, so
        // this cannot fail by the route people worry about — but it can fail by
        // a route nobody watches: a demo that quietly acquires a relative
        // include of its own, or of another demo's header.
        const std::string game = read_file("demos/collector/main.cpp");

        // A DEGENERATE INPUT MUST NOT PASS. An unreadable file has no bad
        // includes in it, and "no bad includes" is exactly what this section
        // reports on success. Printing the size is the cheap half of the fix;
        // requiring it is the other half.
        checkf(game.size() > 10000,
               "read demos/collector/main.cpp (%zu bytes — run from the repo root)",
               game.size());

        int engine_includes = 0;
        int quoted_includes = 0;
        std::size_t at = 0;
        while ((at = game.find("#include", at)) != std::string::npos)
        {
            const std::size_t eol = game.find('\n', at);
            const std::string line = game.substr(at, eol - at);
            at = eol;
            if (line.find("#include <engine/") != std::string::npos) { ++engine_includes; }
            else if (line.find("#include \"") != std::string::npos) { ++quoted_includes; }
        }
        checkf(engine_includes >= 18, "%d public engine headers included", engine_includes);
        checkf(quoted_includes == 0,
               "%d quoted includes — a demo may not include a file by path",
               quoted_includes);

        // THE UMBRELLA. The CMake lint checks this at configure time; this
        // checks it here so that a tree configured before a header was added
        // still reports. Both read the same two things, and neither trusts the
        // other.
        const std::string umbrella = read_file("engine/include/engine/engine.hpp");
        checkf(umbrella.size() > 1000, "read engine.hpp (%zu bytes)", umbrella.size());

        const char* must_list[] = {"engine/ecs/registry.hpp",   "engine/ecs/view.hpp",
                                   "engine/ecs/hierarchy.hpp",  "engine/ecs/camera.hpp",
                                   "engine/asset/asset_store.hpp", "engine/core/actions.hpp",
                                   "engine/core/handle.hpp",    "engine/core/log.hpp",
                                   "engine/gfx/renderable.hpp"};
        int listed = 0;
        for (const char* h : must_list)
        {
            const std::string want = std::string("#include <") + h + ">";
            if (umbrella.find(want) != std::string::npos) { ++listed; }
            else { std::printf("       missing from engine.hpp: %s\n", h); }
        }
        checkf(listed == static_cast<int>(std::size(must_list)),
               "engine.hpp lists %d of %zu headers a game actually needs",
               listed, std::size(must_list));

        // AND `platform/main.hpp` MUST **NOT** BE THERE, which is the one case
        // where absence is the correct answer. An umbrella that hands out a
        // `main()` is a trap, and a completeness check with no exceptions would
        // have "fixed" it.
        check(umbrella.find("#include <engine/platform/main.hpp>") == std::string::npos,
              "engine.hpp does NOT include platform/main.hpp — the documented "
              "exception, still absent");
    }

    std::printf("\n%d checks, %d failure(s)\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
