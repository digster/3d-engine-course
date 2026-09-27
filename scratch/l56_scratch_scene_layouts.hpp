// scratch/scene_layouts.hpp — six ways to store a scene, and two ways to walk one.
//
// Lesson 5.6. Shared by `bench_56.cpp`, which TIMES these layouts, and by
// `verify_56.cpp`, which checks that they all describe the SAME SCENE — because a
// timing comparison between two arms that compute different answers is not a
// comparison, it is a bug report with a stopwatch attached.
//
// One implementation, two consumers: the same argument this course made about
// `build_scene` (5.1), the search path (5.5) and the A/B timing loop (5.6 itself).
// A benchmark whose subject is transcribed into its own test has tested the
// transcription.

#pragma once

#include <engine/gfx/scene.hpp>
#include <engine/math/transform.hpp>

#include <algorithm>
#include <cstddef>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

namespace layouts {

// A deterministic pseudo-random transform, so every layout holds the SAME scene
// and the accumulated answers can be compared bit for bit.
[[nodiscard]] inline engine::transform make_transform(std::uint32_t i)
{
    // A cheap integer hash, then three angles out of it. No <random> here: the
    // values must be identical across layouts and across runs, and a PRNG whose
    // sequence depends on how many times it has been called is a bad way to get
    // that.
    const auto f = [](std::uint32_t x, std::uint32_t salt) {
        x = (x ^ salt) * 2654435761u;
        x ^= x >> 15;
        return static_cast<float>(x % 4096u) / 4096.0f;
    };

    engine::transform t;
    t.position = {f(i, 1u) * 200.0f - 100.0f,
                  f(i, 2u) * 200.0f - 100.0f,
                  f(i, 3u) * 200.0f - 100.0f};
    t.rotation = engine::rotation_y(f(i, 4u) * 6.2831853f)
               * engine::rotation_x(f(i, 5u) * 6.2831853f);
    t.scale = {0.5f + f(i, 6u), 0.5f + f(i, 7u), 0.5f + f(i, 8u)};
    return t;
}

/// ALL SIXTEEN ENTRIES of a model matrix, and the count matters.
///
/// The first version read five of them, which sounds harmless and is a confound
/// that would have flattered exactly one arm. `parent_from_local` is inlined into
/// the flat, pointer, SoA and tree loops, so a compiler that can see only five
/// entries being used is free to stop computing the other eleven. It CANNOT see
/// through the virtual call, so the virtual arm computed the whole matrix while
/// its rivals computed a third of one — and the resulting "3x cost of `virtual`"
/// would have been partly the cost of doing more work.
///
/// Reading everything removes the asymmetry. The arms now provably build the same
/// matrix, and what is left of the gap is attributable to the call.
[[nodiscard]] inline double sample(const engine::mat4& m)
{
    return static_cast<double>(
        (m.c0.x + m.c0.y + m.c0.z + m.c0.w) + (m.c1.x + m.c1.y + m.c1.z + m.c1.w)
      + (m.c2.x + m.c2.y + m.c2.z + m.c2.w) + (m.c3.x + m.c3.y + m.c3.z + m.c3.w));
}

// EVERY WORKLOAD TAKES A `bias`, AND IT IS NOT DECORATION — IT IS THE FIX FOR A
// BENCHMARK THAT WAS LYING.
//
// The first version of this file repeated each pass `inner` times over the same
// const data, which makes the pass a PURE FUNCTION OF NOTHING: same inputs, same
// answer, every time. A compiler is entitled to compute it once and reuse it, and
// at n = 4 it did — the tree arm reported 0.166 ns per item, which is 2.3 cycles
// for four matrix builds and is not a fast layout, it is a loop that did not
// happen. (The tell is arithmetic, not suspicion: `parent_from_local` is nine
// multiplies and sixteen stores. Two cycles is not a plausible price for it, so
// something had to be wrong.)
//
// The bias varies per pass and is added per ELEMENT, so the running accumulator's
// value — and therefore its rounding — depends on it. Floating-point addition is
// not associative, so the compiler cannot lift the sum out and correct it
// afterwards: it has to run the loop again. One `addsd` per object, paid
// identically by every arm, in exchange for measuring work that occurred.

// ---------------------------------------------------------------------------
// The six layouts
// ---------------------------------------------------------------------------

/// 1. FLAT — what the engine has. Contiguous scene_objects, walked in order.
struct flat_scene
{
    std::vector<engine::scene_object> objects;

    explicit flat_scene(std::size_t n)
    {
        objects.resize(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            objects[i].xform = make_transform(static_cast<std::uint32_t>(i));
            objects[i].name = "o";
        }
    }

    [[nodiscard]] double full(double bias) const
    {
        double acc = 0.0;
        for (const engine::scene_object& o : objects)
        {
            acc += sample(engine::parent_from_local(o.xform)) + bias;
        }
        return acc;
    }
};

/// 2/3. POINTERS — one heap allocation per object, reached through an array of
/// pointers. This is what an OOP scene graph is: `std::vector<Entity*>`, or a
/// container of `shared_ptr`, or a parent holding `unique_ptr` children.
///
/// Two variants, and separating them is the point: `ordered` visits the
/// allocations in the order they were made, `shuffled` does not. Real scenes
/// start ordered and become shuffled — objects are created and destroyed, the
/// allocator reuses holes, and the draw list gets sorted by material.
struct pointer_scene
{
    std::vector<std::unique_ptr<engine::scene_object>> owned;

    /// Allocations made BETWEEN the objects, because a real program allocates
    /// other things too. Without them `new` returns adjacent blocks and "pointer
    /// chasing" quietly measures a contiguous walk with an extra load — which is
    /// a much kinder benchmark than the situation it is meant to represent.
    ///
    /// **THEY MUST BE THE SAME SIZE AS THE OBJECT**, and the first version was
    /// not. Twenty-four-byte spacers between ninety-six-byte objects go into a
    /// DIFFERENT SIZE CLASS, so the allocator packs each class in its own run and
    /// the objects came out contiguous anyway — `verify_56` §E measured 497 of 499
    /// consecutive objects exactly `sizeof(scene_object)` apart, which is to say
    /// the fragmentation this array exists to create did not happen. Matching the
    /// size puts spacer and object in one class, one run, alternating.
    std::vector<std::unique_ptr<std::byte[]>> spacers;
    std::vector<engine::scene_object*> ordered;
    std::vector<engine::scene_object*> shuffled;

    explicit pointer_scene(std::size_t n, std::uint32_t seed = 12345u)
    {
        owned.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            auto o = std::make_unique<engine::scene_object>();
            o->xform = make_transform(static_cast<std::uint32_t>(i));
            o->name = "o";
            ordered.push_back(o.get());
            owned.push_back(std::move(o));

            // A few bytes of junk between objects, because a real program
            // allocates other things too. Without this the allocations come out
            // adjacent and "pointer chasing" measures a contiguous walk with an
            // extra load.
            // Three same-size-class blocks per object, so consecutive objects
            // end up about four allocations apart: far enough that each one costs
            // its own cache line and the prefetcher gets nothing for free.
            for (int k = 0; k < 3; ++k)
            {
                spacers.push_back(std::make_unique<std::byte[]>(sizeof(engine::scene_object)));
                spacers.back()[0] = static_cast<std::byte>(i & 0xFFu);
            }
        }
        shuffled = ordered;
        std::mt19937 rng(seed);
        std::shuffle(shuffled.begin(), shuffled.end(), rng);
    }

    [[nodiscard]] static double full(const std::vector<engine::scene_object*>& v, double bias)
    {
        double acc = 0.0;
        for (const engine::scene_object* o : v)
        {
            acc += sample(engine::parent_from_local(o->xform)) + bias;
        }
        return acc;
    }
};

/// 4. SoA — the transform taken apart into three parallel arrays.
struct soa_scene
{
    std::vector<engine::vec3> position;
    std::vector<engine::mat3> rotation;
    std::vector<engine::vec3> scale;

    explicit soa_scene(std::size_t n)
    {
        position.resize(n);
        rotation.resize(n);
        scale.resize(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            const engine::transform t = make_transform(static_cast<std::uint32_t>(i));
            position[i] = t.position;
            rotation[i] = t.rotation;
            scale[i] = t.scale;
        }
    }

    [[nodiscard]] double full(double bias) const
    {
        double acc = 0.0;
        for (std::size_t i = 0; i < position.size(); ++i)
        {
            const engine::mat3 axes{rotation[i].c0 * scale[i].x,
                                    rotation[i].c1 * scale[i].y,
                                    rotation[i].c2 * scale[i].z};
            acc += sample(engine::affine(axes, position[i])) + bias;
        }
        return acc;
    }
};

/// 5. VIRTUAL — an object hierarchy with a virtual call per object, which is what
/// the scene graph being argued against actually looks like. Being fair to it
/// matters: if the comparison is against a straw man, the conclusion is worthless.
struct node
{
    virtual ~node() = default;
    [[nodiscard]] virtual engine::mat4 world() const = 0;
};

struct mesh_node final : node
{
    engine::transform xform;
    [[nodiscard]] engine::mat4 world() const override
    {
        return engine::parent_from_local(xform);
    }
};

/// …and a second concrete type, so the call site is genuinely polymorphic. With
/// one implementation the branch predictor gets it right every time and the
/// measurement flatters the virtual call enormously — which is itself worth
/// knowing, and is why both variants are timed.
struct light_node final : node
{
    engine::transform xform;
    [[nodiscard]] engine::mat4 world() const override
    {
        return engine::parent_from_local(xform);
    }
};

struct virtual_scene
{
    std::vector<std::unique_ptr<node>> mono;    ///< all mesh_node
    std::vector<std::unique_ptr<node>> poly;    ///< mesh_node / light_node, mixed
    std::vector<std::unique_ptr<std::byte[]>> spacers;

    explicit virtual_scene(std::size_t n)
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            auto m = std::make_unique<mesh_node>();
            m->xform = make_transform(static_cast<std::uint32_t>(i));
            mono.push_back(std::move(m));

            if ((i * 2654435761u) % 3u == 0u)
            {
                auto l = std::make_unique<light_node>();
                l->xform = make_transform(static_cast<std::uint32_t>(i));
                poly.push_back(std::move(l));
            }
            else
            {
                auto m2 = std::make_unique<mesh_node>();
                m2->xform = make_transform(static_cast<std::uint32_t>(i));
                poly.push_back(std::move(m2));
            }
            // Three same-size-class blocks per object, so consecutive objects
            // end up about four allocations apart: far enough that each one costs
            // its own cache line and the prefetcher gets nothing for free.
            for (int k = 0; k < 3; ++k)
            {
                spacers.push_back(std::make_unique<std::byte[]>(sizeof(engine::scene_object)));
                spacers.back()[0] = static_cast<std::byte>(i & 0xFFu);
            }
        }
    }

    [[nodiscard]] static double full(const std::vector<std::unique_ptr<node>>& v, double bias)
    {
        double acc = 0.0;
        for (const std::unique_ptr<node>& n : v) { acc += sample(n->world()) + bias; }
        return acc;
    }
};

/// 6. TREE — a linked scene tree, traversed depth-first. Every engine's first
/// scene representation, and the thing this lesson is named after.
struct tree_node
{
    engine::transform xform;
    tree_node* first_child = nullptr;
    tree_node* next_sibling = nullptr;

    // A second child/sibling chain over the same nodes, in a shuffled order. Two
    // link pairs rather than two node sets, so the two tree arms differ in
    // VISIT ORDER and in nothing else — not in allocation, not in footprint, not
    // in which addresses exist.
    tree_node* alt_child = nullptr;
    tree_node* alt_sibling = nullptr;
};

struct tree_scene
{
    std::vector<std::unique_ptr<tree_node>> owned;
    std::vector<std::unique_ptr<std::byte[]>> spacers;
    tree_node* root = nullptr;          ///< children in allocation order
    tree_node* shuffled_root = nullptr; ///< …and in a random one

    /// A wide, shallow tree — one root with `n-1` children — because a deep one
    /// would be measuring recursion depth rather than layout. The children are
    /// linked in reverse allocation order, which is what push-front does and what
    /// every hand-written child list does.
    ///
    /// TWO CHILD ORDERS, and the second one is here because the first was too
    /// kind. Linked in allocation order, a tree walks almost exactly as fast as an
    /// array — the addresses still march forward and the prefetcher does not care
    /// that it is being led by pointers. That is a real result and it is not the
    /// state a tree is in after an hour of play: objects are created and
    /// destroyed, the allocator fills holes, children are re-parented and sorted.
    /// Measuring only the fresh tree would have been arguing against a straw man
    /// in the engine's favour.
    explicit tree_scene(std::size_t n)
    {
        owned.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            auto t = std::make_unique<tree_node>();
            t->xform = make_transform(static_cast<std::uint32_t>(i));
            owned.push_back(std::move(t));
            // Three same-size-class blocks per object, so consecutive objects
            // end up about four allocations apart: far enough that each one costs
            // its own cache line and the prefetcher gets nothing for free.
            for (int k = 0; k < 3; ++k)
            {
                spacers.push_back(std::make_unique<std::byte[]>(sizeof(engine::scene_object)));
                spacers.back()[0] = static_cast<std::byte>(i & 0xFFu);
            }
        }
        if (owned.empty()) { return; }

        // Chain 1: allocation order, through `next_sibling`.
        root = owned[0].get();
        for (std::size_t i = 1; i < owned.size(); ++i)
        {
            owned[i]->next_sibling = root->first_child;
            root->first_child = owned[i].get();
        }

        // Chain 2: the same nodes, shuffled, through `alt_sibling` — a second
        // link field so both orders can exist over ONE set of nodes and the only
        // difference between the two arms is the order they are visited in.
        std::vector<tree_node*> order;
        order.reserve(owned.size() - 1);
        for (std::size_t i = 1; i < owned.size(); ++i) { order.push_back(owned[i].get()); }
        std::mt19937 rng(9876u);
        std::shuffle(order.begin(), order.end(), rng);

        shuffled_root = owned[0].get();
        for (tree_node* t : order)
        {
            t->alt_sibling = shuffled_root->alt_child;
            shuffled_root->alt_child = t;
        }
    }

    [[nodiscard]] static double walk(const tree_node* n, double bias)
    {
        double acc = 0.0;
        for (; n != nullptr; n = n->next_sibling)
        {
            acc += sample(engine::parent_from_local(n->xform)) + bias;
            acc += walk(n->first_child, bias);
        }
        return acc;
    }

    [[nodiscard]] static double walk_alt(const tree_node* n, double bias)
    {
        double acc = 0.0;
        for (; n != nullptr; n = n->alt_sibling)
        {
            acc += sample(engine::parent_from_local(n->xform)) + bias;
            acc += walk_alt(n->alt_child, bias);
        }
        return acc;
    }

    [[nodiscard]] double full(double bias) const { return walk(root, bias); }
    [[nodiscard]] double full_shuffled(double bias) const { return walk_alt(shuffled_root, bias); }
};

// ---------------------------------------------------------------------------
// Workload 2 — partial access
// ---------------------------------------------------------------------------

constexpr float k_radius_sq = 60.0f * 60.0f;

// AN INT COUNTER, NOT A DOUBLE, and the first version got this wrong. `hits +=
// 1.0` is a serial chain of double additions with ~4 cycles of latency each; the
// cull arithmetic is three multiplies, two adds and a compare. The accumulator was
// therefore the bottleneck in BOTH arms, they came out identical at every n, and
// the layout difference the workload exists to expose was completely masked.
// Integer addition is one cycle and vectorises, which puts the memory back on the
// critical path where it belongs.

[[nodiscard]] inline double cull_flat(const std::vector<engine::scene_object>& v, double bias)
{
    const float r2 = k_radius_sq + static_cast<float>(bias);
    int hits = 0;
    for (const engine::scene_object& o : v)
    {
        const engine::vec3 p = o.xform.position;
        if (p.x * p.x + p.y * p.y + p.z * p.z < r2) { ++hits; }
    }
    return static_cast<double>(hits);
}

[[nodiscard]] inline double cull_soa(const std::vector<engine::vec3>& v, double bias)
{
    const float r2 = k_radius_sq + static_cast<float>(bias);
    int hits = 0;
    for (const engine::vec3& p : v)
    {
        if (p.x * p.x + p.y * p.y + p.z * p.z < r2) { ++hits; }
    }
    return static_cast<double>(hits);
}

}   // namespace layouts
