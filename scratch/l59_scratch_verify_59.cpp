// scratch/verify_59.cpp — Lesson 5.9's harness: the hierarchy, the camera, and the order.
//
//   §A  the composition rule: a child follows, three levels deep, scale included
//   §B  editing the shape: set_parent, re-parenting, and the cycle it refuses
//   §C  THE ORDER: parents strictly before children, and the levels partition
//   §D  the camera: an entity, and the identity that says the maths is right
//   §E  death: the orphan policy, destroy_subtree, and a cycle written by hand
//   §F  all four resolve strategies produce IDENTICAL matrices
//   §G  the golden is still byte-identical
//
// §C is the section that checks the thing the lesson is about. Every other
// section would pass just as well if `hierarchy` recursed, or sorted by entity
// id, or got lucky — the ANSWER is the same whatever order you compute it in, so
// a test of the answer cannot see the order at all. §C walks `level(i)` and
// asserts that every entity's parent appeared in a STRICTLY earlier level, which
// is the property the whole design exists to guarantee.
//
// §F is the exactness claim that bench_59 deliberately does not make. The
// benchmark's arms accumulate a running checksum and report agreement on that;
// here the four arms' full matrix arrays are compared element by element, with no
// clock running, which is where a claim about correctness belongs.
//
// Build and run:  sh scratch/build_verify_59.sh

#include <engine/ecs/camera.hpp>
#include <engine/ecs/hierarchy.hpp>
#include <engine/ecs/registry.hpp>

#include "hier_probe.hpp"

#include "../demos/common/demo_scene.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using engine::ecs::entity;
using engine::ecs::hierarchy;
using engine::ecs::hierarchy_report;
using engine::ecs::null_entity;
using engine::ecs::parent;
using engine::ecs::parent_status;
using engine::ecs::registry;
using engine::ecs::world_transform;
using engine::transform;

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

[[nodiscard]] float worst_element(const engine::mat4& a, const engine::mat4& b)
{
    float worst = 0.0f;
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            const float d = std::fabs(a.at(r, c) - b.at(r, c));
            if (d > worst) { worst = d; }
        }
    }
    return worst;
}

[[nodiscard]] bool near_vec(engine::vec3 a, engine::vec3 b, float eps = 1e-4f)
{
    return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps
        && std::fabs(a.z - b.z) < eps;
}

/// Where an entity ended up, in world space.
[[nodiscard]] engine::vec3 world_pos(registry& w, entity e)
{
    const world_transform* wt = w.get<world_transform>(e);
    return (wt != nullptr) ? engine::translation_of(wt->matrix) : engine::vec3{};
}

// ===========================================================================
//  §A — THE COMPOSITION RULE
// ===========================================================================

void section_a_composition()
{
    std::printf("\n=== A. The composition rule ===\n");

    registry w;
    hierarchy tree;

    const entity sun = w.create();
    engine::ecs::add_hierarchy_components(w, sun, transform{.position = {0.0f, 0.0f, 0.0f}});

    const entity planet = w.create();
    engine::ecs::add_hierarchy_components(w, planet, transform{.position = {10.0f, 0.0f, 0.0f}});
    check(engine::ecs::set_parent(w, planet, sun) == parent_status::ok, "planet parented to sun");

    const entity moon = w.create();
    engine::ecs::add_hierarchy_components(w, moon, transform{.position = {0.0f, 2.0f, 0.0f}});
    check(engine::ecs::set_parent(w, moon, planet) == parent_status::ok,
          "moon parented to planet — DEPTH 3");

    tree.mark_topology_changed();
    const hierarchy_report r = tree.rebuild_and_resolve(w);

    checkf(r.entities == 3 && r.roots == 1 && r.levels == 3,
           "report: %zu rows, %zu root, %zu levels", r.entities, r.roots, r.levels);
    check(r.orphans == 0 && r.cycles == 0, "…no orphans, no cycles");

    check(near_vec(world_pos(w, moon), {10.0f, 2.0f, 0.0f}),
          "the moon is at (10, 2, 0) — its own (0,2,0) composed through the planet's (10,0,0), "
          "and NOTHING computed that: it is what parent * local means");

    // Move the root. Everything under it follows, and no code says so.
    w.get<transform>(sun)->position = {100.0f, 0.0f, 0.0f};
    tree.resolve(w);
    check(near_vec(world_pos(w, moon), {110.0f, 2.0f, 0.0f}),
          "move the SUN and the moon two levels down follows — without a rebuild, because "
          "moving a thing is not a change of shape");
    check(near_vec(world_pos(w, planet), {110.0f, 0.0f, 0.0f}), "…and so does the planet");

    // Rotating the parent rotates the child's whole frame.
    w.get<transform>(sun)->position = {0.0f, 0.0f, 0.0f};
    w.get<transform>(sun)->rotation = engine::rotation_y(1.57079633f);   // +90 deg about y
    tree.resolve(w);
    check(near_vec(world_pos(w, planet), {0.0f, 0.0f, -10.0f}, 1e-3f),
          "rotate the sun 90 deg about y and the planet swings from +x to -z — a rotation on a "
          "parent moves its children's whole coordinate frame");

    // SCALE COMPOSES, which is the one that surprises people.
    w.get<transform>(sun)->rotation = engine::mat3::identity();
    w.get<transform>(sun)->scale = {0.5f, 0.5f, 0.5f};
    tree.resolve(w);
    check(near_vec(world_pos(w, planet), {5.0f, 0.0f, 0.0f}),
          "halve the SUN's scale and the planet's world distance halves too (10 -> 5)");
    check(near_vec(world_pos(w, moon), {5.0f, 1.0f, 0.0f}),
          "…and the moon's own 2 units become 1, because scale composes down the whole chain — "
          "which is why a '1-metre' prop under a scaled parent is not 1 metre");

    // A root is unaffected by any of it.
    check(near_vec(world_pos(w, sun), {0.0f, 0.0f, 0.0f}), "the root's world position is its own");
}

// ===========================================================================
//  §B — EDITING THE SHAPE
// ===========================================================================

void section_b_editing()
{
    std::printf("\n=== B. Editing the shape ===\n");

    registry w;
    hierarchy tree;

    const entity a = w.create();
    const entity b = w.create();
    const entity c = w.create();
    for (const entity e : {a, b, c})
    {
        engine::ecs::add_hierarchy_components(w, e, transform{.position = {1.0f, 0.0f, 0.0f}});
    }

    check(engine::ecs::set_parent(w, b, a) == parent_status::ok, "b under a");
    check(engine::ecs::set_parent(w, c, b) == parent_status::ok, "c under b");

    // THE CYCLE, refused three ways.
    check(engine::ecs::set_parent(w, a, a) == parent_status::cycle,
          "an entity cannot be its own parent");
    check(engine::ecs::set_parent(w, a, b) == parent_status::cycle,
          "a cannot be parented to its own child");
    check(engine::ecs::set_parent(w, a, c) == parent_status::cycle,
          "…nor to its GRANDchild — the check walks the whole chain, not one link");
    check(engine::ecs::set_parent(w, b, c) == parent_status::cycle,
          "…and the same for b under its own child");

    check(engine::ecs::is_ancestor_of(w, a, c), "is_ancestor_of(a, c) — two levels up");
    check(!engine::ecs::is_ancestor_of(w, c, a), "…and not the other way round");

    // Dead entities.
    const entity dead = w.create();
    w.destroy(dead);
    check(engine::ecs::set_parent(w, dead, a) == parent_status::dead_child,
          "a dead child is refused, by name");
    check(engine::ecs::set_parent(w, a, dead) == parent_status::dead_parent,
          "…and so is a dead parent, by a different name");

    // RE-PARENTING, and the world position that has to move with it.
    tree.mark_topology_changed();
    tree.rebuild_and_resolve(w);
    check(near_vec(world_pos(w, c), {3.0f, 0.0f, 0.0f}), "c starts at 3 (1+1+1, three deep)");

    check(engine::ecs::set_parent(w, c, a) == parent_status::ok, "re-parent c from b to a");
    tree.mark_topology_changed();
    const hierarchy_report r = tree.rebuild_and_resolve(w);
    check(near_vec(world_pos(w, c), {2.0f, 0.0f, 0.0f}),
          "…and c is now at 2, because its LOCAL transform never changed — re-parenting keeps "
          "the local placement and therefore moves the object, which is the behaviour an "
          "editor has to offer a choice about");
    checkf(r.levels == 2, "the tree is now 2 levels deep, not 3 (%zu)", r.levels);

    // Detaching.
    check(engine::ecs::set_parent(w, c, null_entity) == parent_status::ok, "detach c");
    tree.mark_topology_changed();
    const hierarchy_report r2 = tree.rebuild_and_resolve(w);
    check(near_vec(world_pos(w, c), {1.0f, 0.0f, 0.0f}), "…c is a root now, at its own 1");
    checkf(r2.roots == 2, "…and the world has exactly %zu roots: a and c. The entity that was "
           "created and destroyed above left a free SLOT, not a root — a dead id is not a row "
           "in any pool", r2.roots);
}

// ===========================================================================
//  §C — THE ORDER
// ===========================================================================
//
// The section that checks the DESIGN rather than the answer.

void section_c_order()
{
    std::printf("\n=== C. The order ===\n");

    registry w;
    hierarchy tree;

    // Build a 4-deep tree, then deliberately CHURN the pools so that dense order
    // has nothing to do with tree order — which is the situation Lesson 5.7 says
    // is normal and this lesson exists to handle.
    std::vector<entity> level_entities[4];
    std::vector<entity> fillers;

    auto make = [&w](transform t) {
        const entity e = w.create();
        engine::ecs::add_hierarchy_components(w, e, t);
        return e;
    };

    for (int i = 0; i < 12; ++i)
    {
        level_entities[0].push_back(make(transform{.position = {1.0f, 0.0f, 0.0f}}));
    }

    // Twelve childless fillers, created HERE — early in the dense array, and
    // before the deep levels exist. See the note on the churn below.
    for (int i = 0; i < 12; ++i)
    {
        const entity e = make(transform{.position = {1.0f, 0.0f, 0.0f}});
        engine::ecs::set_parent(w, e, level_entities[0][static_cast<std::size_t>(i)]);
        fillers.push_back(e);
    }

    for (int d = 1; d < 4; ++d)
    {
        for (int i = 0; i < 12; ++i)
        {
            const entity e = make(transform{.position = {1.0f, 0.0f, 0.0f}});
            engine::ecs::set_parent(w, e, level_entities[d - 1][static_cast<std::size_t>(i) % 12]);
            level_entities[d].push_back(e);
        }
    }

    // CHURN — and getting this right took THREE attempts, each of which is a
    // lesson about swap-and-pop rather than about hierarchies.
    //
    //   1. Destroying a scattering of LEAVES scrambled nothing. Swap-and-pop
    //      fills a hole with the pool's LAST element, and in a tree built level
    //      by level the last elements are themselves leaves — so a leaf replaced
    //      a leaf and every parent still preceded every child.
    //   2. Destroying fillers created at the END had the same problem for the
    //      same reason: they were the last elements, so they only ever replaced
    //      each other.
    //   3. What works is destroying something EARLY while deep entities sit at
    //      the end. Each destruction then drags a level-3 entity forward into the
    //      level-1 region, ahead of its own level-2 parent.
    //
    // Twice in a row the test PASSED for a reason that had nothing to do with the
    // code under test — which is the failure mode a test of an ordering property
    // is most prone to, and the reason the assertion below is on a count greater
    // than zero rather than on the answer being right.
    for (const entity e : fillers) { w.destroy(e); }

    tree.mark_topology_changed();
    const hierarchy_report r = tree.rebuild_and_resolve(w);
    checkf(r.levels == 4, "a 4-level tree after churn (%zu)", r.levels);

    // THE CLAIM: every entity's parent appears in a STRICTLY earlier level.
    {
        bool ok = true;
        std::size_t checked = 0;
        for (std::size_t lvl = 0; lvl < tree.levels(); ++lvl)
        {
            for (const entity e : tree.level(lvl))
            {
                const parent* p = w.get<parent>(e);
                if (p == nullptr || !w.alive(p->value)) { continue; }
                ++checked;
                if (tree.depth_of(p->value) + 1u != lvl) { ok = false; }
            }
        }
        checkf(ok && checked > 0,
               "every one of %zu parented entities sits exactly one level below its parent — "
               "this is the topological property, and it is the ONLY thing here that a "
               "recursive resolver would also satisfy while a dense-order walk would not",
               checked);
    }

    // The levels partition the order exactly: no gaps, no repeats.
    {
        std::size_t total = 0;
        for (std::size_t lvl = 0; lvl < tree.levels(); ++lvl) { total += tree.level(lvl).size(); }
        checkf(total == tree.order().size() && total == r.entities,
               "the levels partition the order: %zu = %zu = %zu rows",
               total, tree.order().size(), r.entities);
    }

    // Level 0 is exactly the roots.
    {
        bool ok = true;
        for (const entity e : tree.level(0))
        {
            const parent* p = w.get<parent>(e);
            if (p != nullptr && w.alive(p->value)) { ok = false; }
        }
        checkf(ok && tree.level(0).size() == r.roots,
               "level 0 is exactly the %zu roots, and nothing in it has a live parent", r.roots);
    }

    // And the answer is right too: an entity at depth d is at world x = d + 1.
    {
        bool ok = true;
        for (std::size_t lvl = 0; lvl < tree.levels(); ++lvl)
        {
            for (const entity e : tree.level(lvl))
            {
                if (!near_vec(world_pos(w, e), {static_cast<float>(lvl) + 1.0f, 0.0f, 0.0f}))
                {
                    ok = false;
                }
            }
        }
        check(ok, "…and an entity at depth d lands at x = d + 1, every one of them");
    }

    // A dense-order walk would get it WRONG on this world, which is the whole
    // reason the order exists. Demonstrated rather than asserted.
    {
        engine::ecs::pool<transform>* transforms = w.storage_if<transform>();
        std::size_t out_of_order = 0;
        for (std::size_t i = 0; i < transforms->entities().size(); ++i)
        {
            const entity e = transforms->entities()[i];
            const parent* p = w.get<parent>(e);
            if (p == nullptr || !w.alive(p->value)) { continue; }
            // where does the parent sit in dense order?
            for (std::size_t j = 0; j < transforms->entities().size(); ++j)
            {
                if (transforms->entities()[j] == p->value)
                {
                    if (j > i) { ++out_of_order; }
                    break;
                }
            }
        }
        checkf(out_of_order > 0,
               "%zu entities appear in the transform pool BEFORE their own parent — a "
               "dense-order walk would compose them against a stale matrix and produce a "
               "picture that is wrong in a way nothing reports", out_of_order);
    }
}

// ===========================================================================
//  §D — THE CAMERA
// ===========================================================================

void section_d_camera()
{
    std::printf("\n=== D. The camera ===\n");

    const engine::vec3 eye{2.5f, 1.5f, 4.0f};
    const engine::vec3 target{0.2f, 0.4f, -0.1f};
    const engine::vec3 up{0.0f, 1.0f, 0.0f};

    // THE IDENTITY: placing a camera and inverting its placement is Lesson 2.9's
    // look_at, exactly.
    {
        const transform t = engine::ecs::look_along(eye, target, up);
        const engine::mat4 placement = engine::parent_from_local(t);
        const engine::mat4 view = engine::rigid_inverse(placement);
        const float worst = worst_element(view, engine::look_at(eye, target, up));
        checkf(worst == 0.0f,
               "rigid_inverse(parent_from_local(look_along(...))) IS look_at(...) — worst "
               "element difference %.3e, bit for bit", static_cast<double>(worst));
    }

    check(engine::is_rigid(engine::parent_from_local(engine::ecs::look_along(eye, target, up))),
          "…and the placement it builds is rigid, which is rigid_inverse's precondition");
    check(!engine::is_rigid(engine::parent_from_local(transform{.scale = {2.0f, 1.0f, 1.0f}})),
          "is_rigid rejects a scaled placement — the case a camera must never be in");

    // The camera as an entity.
    registry w;
    hierarchy tree;

    const entity cam = w.create();
    engine::ecs::add_hierarchy_components(w, cam, engine::ecs::look_along(eye, target, up));
    w.add<engine::ecs::camera>(cam, engine::ecs::camera{});
    check(engine::ecs::set_active_camera(w, cam), "set_active_camera");

    tree.mark_topology_changed();
    tree.rebuild_and_resolve(w);

    check(engine::ecs::find_active_camera(w) == cam, "find_active_camera returns it");
    {
        const world_transform* wt = w.get<world_transform>(cam);
        const float worst = worst_element(engine::ecs::view_from_camera(*wt),
                                          engine::look_at(eye, target, up));
        checkf(worst == 0.0f, "…and its view matrix is look_at's, through the whole ECS "
               "(worst %.3e)", static_cast<double>(worst));
        check(near_vec(engine::ecs::eye_of(*wt), eye), "…and eye_of() is where we put it");
    }

    // A SECOND camera, and the switch that is two structural changes.
    const entity cam2 = w.create();
    engine::ecs::add_hierarchy_components(w, cam2,
                                          engine::ecs::look_along({0.0f, 8.0f, 0.1f},
                                                                  {0.0f, 0.0f, 0.0f}, up));
    w.add<engine::ecs::camera>(cam2, engine::ecs::camera{.fovy = 1.0f});
    check(engine::ecs::set_active_camera(w, cam2), "switch to a second camera");
    check(engine::ecs::find_active_camera(w) == cam2, "…the new one is active");
    check(!w.has<engine::ecs::active_camera>(cam), "…and the old one lost the tag — a switch is "
                                                   "moving a component, not repointing a pointer");

    // A PARENTED CAMERA, which is the whole reason the camera is an entity.
    const entity car = w.create();
    engine::ecs::add_hierarchy_components(w, car, transform{.position = {50.0f, 0.0f, 0.0f}});
    check(engine::ecs::set_parent(w, cam2, car) == parent_status::ok, "park the camera on a car");
    tree.mark_topology_changed();
    tree.rebuild_and_resolve(w);
    check(near_vec(engine::ecs::eye_of(*w.get<world_transform>(cam2)), {50.0f, 8.0f, 0.1f}),
          "…and it rides: the camera's eye is the car's position plus its own offset, resolved "
          "by exactly the same pass as every other entity");

    // Destroy the active camera and the query says so, rather than dangling.
    w.destroy(cam2);
    tree.mark_topology_changed();
    tree.rebuild_and_resolve(w);
    check(engine::ecs::find_active_camera(w) == null_entity,
          "destroy the active camera and find_active_camera returns null — a case a scene can "
          "legitimately be in, and not a dangling pointer");

    // Projection carries the camera's own numbers and the SURFACE's aspect.
    {
        const engine::ecs::camera c{.fovy = 1.2f, .near_plane = 0.25f, .far_plane = 500.0f};
        const engine::mat4 p16 = engine::ecs::projection_of(c, 16.0f / 9.0f);
        const engine::mat4 p4 = engine::ecs::projection_of(c, 4.0f / 3.0f);
        check(p16 != p4, "the same camera projects differently at two aspect ratios — which is "
                         "why aspect is a parameter and not a field");
        check(worst_element(p16, engine::perspective(1.2f, 16.0f / 9.0f, 0.25f, 500.0f)) == 0.0f,
              "…and projection_of is exactly Lesson 2.10's perspective()");
    }
}

// ===========================================================================
//  §E — DEATH
// ===========================================================================

void section_e_death()
{
    std::printf("\n=== E. Death ===\n");

    // ---- the orphan policy -------------------------------------------------
    {
        registry w;
        hierarchy tree;

        const entity sun = w.create();
        engine::ecs::add_hierarchy_components(w, sun, transform{.position = {5.0f, 0.0f, 0.0f}});
        const entity planet = w.create();
        engine::ecs::add_hierarchy_components(w, planet, transform{.position = {3.0f, 0.0f, 0.0f}});
        engine::ecs::set_parent(w, planet, sun);
        const entity moon = w.create();
        engine::ecs::add_hierarchy_components(w, moon, transform{.position = {1.0f, 0.0f, 0.0f}});
        engine::ecs::set_parent(w, moon, planet);

        tree.mark_topology_changed();
        tree.rebuild_and_resolve(w);
        check(near_vec(world_pos(w, moon), {9.0f, 0.0f, 0.0f}), "moon at 9 = 5 + 3 + 1");

        w.destroy(planet);
        tree.mark_topology_changed();
        const hierarchy_report r = tree.rebuild_and_resolve(w);

        checkf(r.orphans == 1, "killing the middle entity leaves %zu orphan", r.orphans);
        checkf(r.roots == 2 && r.levels == 1, "…which becomes a ROOT: %zu roots, %zu level",
               r.roots, r.levels);
        check(near_vec(world_pos(w, moon), {1.0f, 0.0f, 0.0f}),
              "…and the orphan is now at its own local 1, not at 9 — it stopped following, "
              "which is the documented policy and is why the count exists");
        check(w.alive(moon), "…and it is STILL ALIVE: a transform system does not delete "
                             "entities nobody asked it to");
    }

    // ---- destroy_subtree, the explicit alternative -------------------------
    {
        registry w;
        hierarchy tree;

        const entity root = w.create();
        engine::ecs::add_hierarchy_components(w, root, transform{});
        std::vector<entity> kids;
        for (int i = 0; i < 4; ++i)
        {
            const entity k = w.create();
            engine::ecs::add_hierarchy_components(w, k, transform{});
            engine::ecs::set_parent(w, k, root);
            kids.push_back(k);
            for (int j = 0; j < 3; ++j)
            {
                const entity g = w.create();
                engine::ecs::add_hierarchy_components(w, g, transform{});
                engine::ecs::set_parent(w, g, k);
            }
        }
        const entity bystander = w.create();
        engine::ecs::add_hierarchy_components(w, bystander, transform{});

        checkf(w.size() == 18, "a 3-level tree of 17 plus one bystander (%zu)", w.size());

        const std::size_t killed = engine::ecs::destroy_subtree(w, kids[1]);
        checkf(killed == 4, "destroy_subtree on a child killed %zu — it and its 3 grandchildren",
               killed);
        checkf(w.size() == 14, "…leaving %zu", w.size());
        check(w.alive(bystander) && w.alive(root) && w.alive(kids[0]),
              "…and touched nothing outside the subtree");

        tree.mark_topology_changed();
        const hierarchy_report r = tree.rebuild_and_resolve(w);
        checkf(r.orphans == 0, "…with NO orphans (%zu), because the whole subtree went together",
               r.orphans);
    }

    // ---- a cycle written by hand -------------------------------------------
    //
    // `set_parent` refuses to make one, but `parent` is a public component and
    // nothing stops a caller writing it directly. The resolver must survive that,
    // because a hierarchy that can loop is a HANG rather than a wrong picture.
    {
        registry w;
        hierarchy tree;

        const entity a = w.create();
        const entity b = w.create();
        const entity c = w.create();
        for (const entity e : {a, b, c})
        {
            engine::ecs::add_hierarchy_components(w, e, transform{.position = {1.0f, 0.0f, 0.0f}});
        }
        const entity safe = w.create();
        engine::ecs::add_hierarchy_components(w, safe, transform{.position = {7.0f, 0.0f, 0.0f}});

        // a -> b -> c -> a, written straight into the components.
        w.add<parent>(a, parent{c});
        w.add<parent>(b, parent{a});
        w.add<parent>(c, parent{b});

        tree.mark_topology_changed();
        const hierarchy_report r = tree.rebuild_and_resolve(w);

        check(true, "a hand-written 3-cycle did not hang the resolver — which is the only "
                    "thing that actually matters here");
        checkf(r.cycles >= 1, "…and it was COUNTED (%zu), not silently absorbed", r.cycles);
        checkf(r.entities == 4, "…all %zu entities were still resolved", r.entities);
        check(near_vec(world_pos(w, safe), {7.0f, 0.0f, 0.0f}),
              "…and the entity outside the cycle is unaffected: one bad link does not cost the "
              "frame");
    }
}

// ===========================================================================
//  §F — THE FOUR STRATEGIES AGREE, EXACTLY
// ===========================================================================

void section_f_arms()
{
    std::printf("\n=== F. All four resolve strategies ===\n");

    constexpr std::size_t k_n = 20000;
    constexpr int k_depth = 8;

    hier::world wa = hier::make_forest(k_n, k_depth, true);
    hier::world wb = hier::make_forest(k_n, k_depth, true);
    hier::world wc = hier::make_forest(k_n, k_depth, true);
    hier::world wd = hier::make_forest(k_n, k_depth, true);

    (void)hier::resolve_recursive(wa);
    (void)hier::resolve_levels(wb);

    // Arm C permutes its rows, so its output has to be compared through the
    // permutation. Recording where each row went is the only fair way to do it,
    // and getting this wrong would make the arm look broken when it is not.
    std::vector<std::uint32_t> new_row(k_n);
    for (std::uint32_t i = 0; i < k_n; ++i) { new_row[wc.order[i]] = i; }
    hier::permute_to_level_order(wc);
    (void)hier::resolve_levels_packed(wc);

    std::vector<std::uint8_t> all_dirty(k_n, 1u);
    (void)hier::resolve_dirty(wd, all_dirty);

    float worst_b = 0.0f;
    float worst_c = 0.0f;
    float worst_d = 0.0f;
    for (std::uint32_t r = 0; r < k_n; ++r)
    {
        worst_b = std::fmax(worst_b, worst_element(wa.out[r], wb.out[r]));
        worst_c = std::fmax(worst_c, worst_element(wa.out[r], wc.out[new_row[r]]));
        worst_d = std::fmax(worst_d, worst_element(wa.out[r], wd.out[r]));
    }

    checkf(worst_b == 0.0f, "recursive and level-index agree BIT FOR BIT over %zu entities "
           "(worst %.3e)", k_n, static_cast<double>(worst_b));
    checkf(worst_c == 0.0f, "…and level-packed, through its permutation (worst %.3e)",
           static_cast<double>(worst_c));
    checkf(worst_d == 0.0f, "…and dirty-with-everything-dirty (worst %.3e)",
           static_cast<double>(worst_d));

    // The dirty arm's real claim: a PARTIAL pass leaves the same matrices as a
    // full one, for the rows it was supposed to touch and for the ones it skipped.
    {
        hier::world full = hier::make_forest(k_n, k_depth, true);
        hier::world part = hier::make_forest(k_n, k_depth, true);
        (void)hier::resolve_levels(full);      // establish a baseline in `part` too
        (void)hier::resolve_levels(part);

        // Move one entity, mark only it, and resolve partially.
        const std::uint32_t victim = 7;
        part.local[victim].position.x += 3.0f;
        full.local[victim].position.x += 3.0f;

        std::vector<std::uint8_t> mask(k_n, 0u);
        mask[victim] = 1u;
        const std::size_t reach = hier::dirty_reach(part, mask);
        (void)hier::resolve_dirty(part, mask);
        (void)hier::resolve_levels(full);

        float worst = 0.0f;
        for (std::uint32_t r = 0; r < k_n; ++r)
        {
            worst = std::fmax(worst, worst_element(full.out[r], part.out[r]));
        }
        checkf(worst == 0.0f,
               "moving ONE entity and resolving only its subtree (%zu rows of %zu) leaves the "
               "world bit-identical to a full pass (worst %.3e)",
               reach, k_n, static_cast<double>(worst));
        checkf(reach > 1 && reach < k_n,
               "…and it really was partial: %zu rows touched, not 1 and not %zu — the "
               "descendants of a moved entity have to move too, which is the amplification "
               "the lesson's §6 measures",
               reach, k_n);
    }

    // The exactness claim the benchmark's checksum could not make, and the reason:
    // a double accumulator over 1e5 floats of this magnitude never rounds.
    {
        double sum_a = 0.0;
        double sum_rev = 0.0;
        for (std::uint32_t r = 0; r < k_n; ++r) { sum_a += static_cast<double>(wa.out[r].c3.x); }
        for (std::uint32_t r = k_n; r-- > 0;) { sum_rev += static_cast<double>(wa.out[r].c3.x); }
        float f_fwd = 0.0f;
        float f_rev = 0.0f;
        for (std::uint32_t r = 0; r < k_n; ++r) { f_fwd += wa.out[r].c3.x; }
        for (std::uint32_t r = k_n; r-- > 0;) { f_rev += wa.out[r].c3.x; }

        check(sum_a == sum_rev,
              "summing the same floats forwards and backwards into a DOUBLE gives the same "
              "answer — no partial sum ever needs more than ~41 bits, so nothing rounds");
        check(f_fwd != f_rev,
              "…and into a FLOAT it does not. That is the knob that proves the explanation "
              "rather than asserting it, and it is why bench_59 can report `agree`");
    }
}

// ===========================================================================
//  §G — THE GOLDEN
// ===========================================================================

void section_g_golden()
{
    std::printf("\n=== G. The golden ===\n");

    const char* path = "build/demos/verify_59.ppm";
    check(demo::write_reference_shot(path) == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file(path);
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(), "same size: %zu bytes", ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL to the golden written in Lesson 5.1 — NINE lessons. 5.9 added a "
          "hierarchy, a camera component and one function to mat4.hpp, and touched no line "
          "the reference scene executes");
}

}   // namespace

int main()
{
    std::printf("verify_59 — Lesson 5.9: transform hierarchy and the camera system\n");
    std::printf("(debug assertions %s)\n",
                engine::debug_assertions_enabled() ? "ON" : "OFF");

    section_a_composition();
    section_b_editing();
    section_c_order();
    section_d_camera();
    section_e_death();
    section_f_arms();
    section_g_golden();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
