// scratch/verify_88.cpp — every number Lesson 8.8 prints, measured rather than
// asserted.
//
// Build and run:  sh scratch/build_verify_88.sh
//
// Nine sections, in the lesson's order:
//
//   A  the quadratic is already here
//   B  the grid, against the thing it replaces
//   C  the contract: a superset, never a subset
//   D  duplicates, and the cell that owns a pair
//   E  the cell size, swept across three decades
//   F  the hash, on the input a physics scene actually produces
//   G  the margin
//   H  the teapot in the stadium
//   I  the budget, and the crossover
//
// EVERY SECTION CARRIES A CONTROL — 8.1 through 8.7's rule, in two halves: ask
// what the control would say if the thing were COMPLETELY BROKEN, and what it
// would say if the thing were completely FINE. 8.7 §9 shipped a control that
// convicted the code of the TEST's mistake (it expected ids not to survive a
// teleport, and they correctly did), so the second question matters as much as
// the first.
//
// THE INSTRUMENTS, AND WHY THERE IS EXACTLY ONE THAT MATTERS.
//
//   * A BROADPHASE HAS AN ORACLE, which is the luxury this lesson has and 8.5
//     and 8.6 did not. `brute_force_pairs` answers the same question exactly,
//     for any input, in n(n-1)/2 AABB tests. So "is the grid right" is not a
//     matter of tolerance or of a second opinion — it is set equality against a
//     function that cannot be wrong, and every section below leans on it.
//   * THE ORACLE IS ALSO THE BASELINE, so the same function supplies both the
//     correctness reference and the number the speedup is measured against.
//   * A DUPLICATE IS SELF-DETECTING. Sort the pair list and look for an equal
//     adjacent pair; no reference needed.
//   * AND A COST MODEL IS CHECKABLE AGAINST ITSELF. §E predicts the entry count
//     from the cell size analytically and compares it against the grid's own
//     `stats().entries`, which is a number the grid had to keep anyway.
//
// PRECISION. The engine grids in `float` and the cell arithmetic is exact in
// integers once the floor is taken, so there is nothing here that needs
// `double` except the timings and the ratios.
//
// EVERY TIMING BELOW IS A MINIMUM OVER MANY RUNS, NOT A MEDIAN, and that is a
// change from 8.1 through 8.7. The work being timed here is a single build of a
// few hundred MICROseconds rather than a loop over twenty thousand queries, and
// at that duration the operating system's interruptions dominate: §5's spread
// reaches 1.06 around the minimum, meaning the slowest of forty-one runs took
// twice as long as the fastest. The minimum is the standard robust estimator for
// "how fast can this code go", it is the conservative direction for every claim
// in §1 (a cheaper narrow phase puts the quadratic wall FARTHER away), and where
// a number could be had without a clock at all — `entries`, `bucket_tests`,
// pair counts — this file uses the count instead.
//
// OUTPUT WIDTH IS A CONSTRAINT, not a preference. The lesson quotes this
// program's transcript in <pre> blocks that scroll and never wrap, and the fold
// is at about 66 characters. Nothing below exceeds 66.

#include <engine/core/bench.hpp>
#include <engine/math/bounds.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/broadphase.hpp>
#include <engine/phys/collide.hpp>
#include <engine/phys/convex.hpp>
#include <engine/phys/epa.hpp>
#include <engine/phys/gjk.hpp>
#include <engine/phys/manifold.hpp>
#include <engine/phys/shape.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <array>
#include <map>
#include <set>
#include <vector>

using engine::aabb;
using engine::quat;
using engine::quat_from_axis_angle;
using engine::vec3;
using engine::bench_result;
using engine::bench_run;
using engine::phys::auto_cell_size;
using engine::phys::bounds_of;
using engine::phys::broadphase_config;
using engine::phys::broadphase_pair;
using engine::phys::brute_force_pairs;
using engine::phys::collide_manifold;
using engine::phys::convex;
using engine::phys::cube_shape;
using engine::phys::as_convex;
using engine::phys::overlaps;
using engine::phys::owner_cell;
using engine::phys::proxy;
using engine::phys::shape;
using engine::phys::sphere_shape;
using engine::phys::uniform_grid;
using engine::phys::world_obb;

// ---------------------------------------------------------------------------
// Allocation counting, for §I
// ---------------------------------------------------------------------------

namespace
{
std::size_t g_allocs = 0;
}

void* operator new(std::size_t n)
{
    ++g_allocs;
    void* p = std::malloc(n ? n : 1);
    if (!p) { throw std::bad_alloc(); }
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

namespace
{

int g_checks = 0;
int g_failures = 0;

void check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok)
    {
        ++g_failures;
        std::printf("  FAIL  %s\n", what);
    }
}

void rule(const char* title)
{
    std::printf("\n%s\n", title);
    for (int i = 0; i < 66; ++i) { std::putchar('-'); }
    std::putchar('\n');
}

/// The same deterministic generator 8.1-8.7 used, for the same reason:
/// `std::uniform_real_distribution` is not specified to produce the same
/// sequence on two standard libraries.
class rng
{
public:
    explicit rng(std::uint32_t seed) : state_(seed | 1u) {}

    std::uint32_t next()
    {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }

    float unit() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }
    float signed_unit() { return unit() * 2.0f - 1.0f; }
    float range(float lo, float hi) { return lo + (hi - lo) * unit(); }

    vec3 direction()
    {
        for (int guard = 0; guard < 64; ++guard)
        {
            const vec3 v{signed_unit(), signed_unit(), signed_unit()};
            const float len2 = engine::length_squared(v);
            if (len2 > 1e-4f && len2 <= 1.0f) { return v / std::sqrt(len2); }
        }
        return vec3{0.0f, 1.0f, 0.0f};
    }

    quat rotation()
    {
        return quat_from_axis_angle(direction(), range(0.0f, 2.0f * 3.14159265f));
    }

private:
    std::uint32_t state_;
};

// ---------------------------------------------------------------------------
// Scenes
// ---------------------------------------------------------------------------

/// `n` unit-ish crates, freely rotated, scattered through a cube whose side is
/// chosen so the DENSITY is constant as `n` grows.
///
/// Constant density is the whole point of the fixture. Scatter a thousand boxes
/// through the same room as a hundred and the thousand overlap far more, so a
/// timing sweep over `n` would be measuring crowding rather than scale. Here
/// each scene has the same expected number of overlapping pairs PER OBJECT,
/// which is what a real level looks like as it grows.
std::vector<proxy> scatter(int n, float per_object_volume, rng& r, float half = 0.5f)
{
    const float side = std::cbrt(static_cast<float>(n) * per_object_volume);
    std::vector<proxy> out;
    out.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        const vec3 c{r.range(0.0f, side), r.range(0.0f, side), r.range(0.0f, side)};
        const shape s = cube_shape(half * 2.0f);
        proxy p;
        p.box = bounds_of(s, c, r.rotation());
        p.index = static_cast<std::uint32_t>(i);
        out.push_back(p);
    }
    return out;
}

/// Crates stacked in a regular lattice on a floor — the input a physics scene
/// actually produces, and the one a spatial hash is worst at.
std::vector<proxy> lattice(int nx, int ny, int nz, float pitch, float half = 0.5f)
{
    std::vector<proxy> out;
    out.reserve(static_cast<std::size_t>(nx * ny * nz));
    std::uint32_t id = 0;
    for (int z = 0; z < nz; ++z)
    {
        for (int y = 0; y < ny; ++y)
        {
            for (int x = 0; x < nx; ++x)
            {
                const vec3 c{static_cast<float>(x) * pitch, static_cast<float>(y) * pitch,
                             static_cast<float>(z) * pitch};
                proxy p;
                p.box.min = c - vec3{half, half, half};
                p.box.max = c + vec3{half, half, half};
                p.index = id++;
                out.push_back(p);
            }
        }
    }
    return out;
}

/// Sort a pair list so two of them can be compared as sets.
void canonicalise(std::vector<broadphase_pair>& v)
{
    std::sort(v.begin(), v.end(), [](const broadphase_pair& x, const broadphase_pair& y) {
        return x.a != y.a ? x.a < y.a : x.b < y.b;
    });
}

bool same_set(std::vector<broadphase_pair> x, std::vector<broadphase_pair> y)
{
    canonicalise(x);
    canonicalise(y);
    return x.size() == y.size() && std::equal(x.begin(), x.end(), y.begin());
}

/// How many pairs of `sub` are missing from `sup`. Zero is the contract.
int missing_from(const std::vector<broadphase_pair>& sub, std::vector<broadphase_pair> sup)
{
    canonicalise(sup);
    int missed = 0;
    for (const broadphase_pair& p : sub)
    {
        const bool found = std::binary_search(
            sup.begin(), sup.end(), p, [](const broadphase_pair& x, const broadphase_pair& y) {
                return x.a != y.a ? x.a < y.a : x.b < y.b;
            });
        if (!found) { ++missed; }
    }
    return missed;
}

int duplicates_in(std::vector<broadphase_pair> v)
{
    canonicalise(v);
    int dup = 0;
    for (std::size_t i = 1; i < v.size(); ++i)
    {
        if (v[i] == v[i - 1]) { ++dup; }
    }
    return dup;
}

} // namespace

// ---------------------------------------------------------------------------
// A. The quadratic is already here
// ---------------------------------------------------------------------------
//
// THE CLAIM. `n(n-1)/2` narrow-phase calls at 8.7's measured price blow a 60 Hz
// frame at a scene size small enough to be embarrassing, and the AABB test that
// a broadphase substitutes for it is cheap enough to move that wall by more than
// an order of magnitude — but not to remove it.
//
// THE CONTROL. The loop's own pair count against `n(n-1)/2`, which is the one
// thing in this section that cannot be a matter of opinion. A broadphase that
// tested fewer pairs than that would be a broadphase already.

namespace
{

void section_a()
{
    rule("A. n(n-1)/2, and what it costs");

    rng r(0x8801u);

    // The narrow phase's real price, measured HERE rather than quoted from 8.7,
    // because a quoted timing is a timing from somebody else's machine — and
    // measured TWICE, because it is not one number.
    //
    //   RESTING is 8.7 §I's own fixture: crates yawed about the up axis and
    //     overlapping by 5 cm, which is a stack settling. Face on face, EPA
    //     terminates on a genuinely flat facet, and this is the common case.
    //   TUMBLING is freely rotated crates driven deep into each other, which is
    //     the expensive case and the one a falling pile produces on impact.
    //
    // The wall below is computed from the RESTING number, which is the smaller
    // of the two — the conservative direction for an argument that says the
    // quadratic is already a problem.
    const shape crate = cube_shape(1.0f);
    std::vector<engine::phys::obb> rest_a;
    std::vector<engine::phys::obb> rest_b;
    std::vector<engine::phys::obb> tumb_a;
    std::vector<engine::phys::obb> tumb_b;
    for (int i = 0; i < 4096; ++i)
    {
        const quat yaw_a = quat_from_axis_angle(vec3{0, 1, 0}, r.range(0.0f, 6.2831853f));
        const quat yaw_b = quat_from_axis_angle(vec3{0, 1, 0}, r.range(0.0f, 6.2831853f));
        rest_a.push_back(world_obb(crate, vec3{0, 0, 0}, yaw_a));
        rest_b.push_back(
            world_obb(crate, vec3{r.range(-0.5f, 0.5f), 0.95f, r.range(-0.5f, 0.5f)}, yaw_b));

        tumb_a.push_back(world_obb(crate, vec3{0, 0, 0}, r.rotation()));
        tumb_b.push_back(world_obb(crate, r.direction() * r.range(0.6f, 1.2f), r.rotation()));
    }

    const bench_result resting = bench_run(4096, 21, [&] {
        double acc = 0.0;
        for (int i = 0; i < 4096; ++i)
        {
            acc += static_cast<double>(
                collide_manifold(as_convex(rest_a[static_cast<std::size_t>(i)]),
                                 as_convex(rest_b[static_cast<std::size_t>(i)]))
                    .deepest());
        }
        return acc;
    });
    const bench_result tumbling = bench_run(4096, 21, [&] {
        double acc = 0.0;
        for (int i = 0; i < 4096; ++i)
        {
            acc += static_cast<double>(
                collide_manifold(as_convex(tumb_a[static_cast<std::size_t>(i)]),
                                 as_convex(tumb_b[static_cast<std::size_t>(i)]))
                    .deepest());
        }
        return acc;
    });
    const bench_result narrow = resting;

    std::printf("  narrow phase, resting crates (8.7's)  %8.1f ns\n", resting.min_ns);
    std::printf("  narrow phase, tumbling and deep       %8.1f ns\n", tumbling.min_ns);

    // And the broadphase's substitute, on the same machine.
    std::vector<aabb> ba;
    std::vector<aabb> bb;
    for (int i = 0; i < 4096; ++i)
    {
        ba.push_back(bounds_of(crate, vec3{0, 0, 0}, r.rotation()));
        bb.push_back(bounds_of(crate, r.direction() * r.range(0.6f, 3.0f), r.rotation()));
    }
    const bench_result box = bench_run(4096, 31, [&] {
        int hits = 0;
        for (int i = 0; i < 4096; ++i)
        {
            hits += overlaps(ba[static_cast<std::size_t>(i)], bb[static_cast<std::size_t>(i)]) ? 1 : 0;
        }
        return static_cast<double>(hits);
    });
    std::printf("  one AABB overlap test                 %8.3f ns\n", box.min_ns);
    std::printf("  ratio                                 %8.0fx\n",
                narrow.min_ns / box.min_ns);

    std::printf("\n     n       pairs   all-narrow    all-AABB\n");
    const int ns[] = {50, 100, 291, 500, 1000, 5000, 20000};
    for (const int n : ns)
    {
        const double pairs = 0.5 * static_cast<double>(n) * static_cast<double>(n - 1);
        std::printf("  %6d  %10.0f  %9.2f ms  %8.2f ms\n", n, pairs,
                    pairs * narrow.min_ns * 1e-6, pairs * box.min_ns * 1e-6);
    }

    // The two walls, solved rather than looked up in the table.
    const double budget_ns = 16.666e6;
    const double n_narrow = 0.5 + std::sqrt(0.25 + 2.0 * budget_ns / narrow.min_ns);
    const double n_box = 0.5 + std::sqrt(0.25 + 2.0 * budget_ns / box.min_ns);
    std::printf("\n  16.67 ms budget: all-narrow at n = %.0f, all-AABB at n = %.0f\n",
                n_narrow, n_box);

    // CONTROL. The loop count against the closed form.
    std::vector<proxy> tiny = scatter(64, 40.0f, r);
    std::vector<broadphase_pair> out;
    long long seen = 0;
    for (std::size_t i = 0; i < tiny.size(); ++i)
    {
        for (std::size_t j = i + 1; j < tiny.size(); ++j) { ++seen; }
    }
    brute_force_pairs(tiny, out);
    check(seen == 64 * 63 / 2, "A: n(n-1)/2 is the loop's own count");
    check(static_cast<int>(out.size()) <= static_cast<int>(seen),
          "A: reported pairs never exceed tested pairs");
    std::printf("  control: 64 proxies -> %lld tested, %zu overlapping\n", seen, out.size());
}

// ---------------------------------------------------------------------------
// B. The grid, against the thing it replaces
// ---------------------------------------------------------------------------
//
// THE CLAIM. Same scene, same answer, and the gap widens linearly in `n` because
// one side is quadratic and the other is not.
//
// THE CONTROL — and it is the strongest control in the file. The two pair lists
// are compared AS SETS, exactly, at every scene size. A grid that were merely
// approximately right would fail this at the first `n`; a grid that were fast
// because it was skipping work would fail it too.

void section_b()
{
    rule("B. the grid, against brute force");

    std::printf("      n     pairs    brute      grid   speedup  same?\n");

    const int ns[] = {64, 128, 256, 512, 1024, 2048, 4096, 8192};
    for (const int n : ns)
    {
        rng r(0x8802u + static_cast<std::uint32_t>(n));
        const std::vector<proxy> scene = scatter(n, 40.0f, r);

        std::vector<broadphase_pair> ref;
        uniform_grid grid;

        const bench_result brute = bench_run(1, n <= 2048 ? 15 : 5, [&] {
            brute_force_pairs(scene, ref);
            return static_cast<double>(ref.size());
        });
        const bench_result fast = bench_run(1, 21, [&] {
            grid.build(scene, {});
            return static_cast<double>(grid.stats().pairs);
        });

        std::vector<broadphase_pair> got(grid.pairs().begin(), grid.pairs().end());
        const bool same = same_set(ref, got);

        std::printf("  %5d  %8zu  %7.3f ms %7.3f ms  %6.1fx  %s\n", n, ref.size(),
                    brute.min_ns * 1e-6, fast.min_ns * 1e-6,
                    brute.min_ns / fast.min_ns, same ? "yes" : "NO");

        check(same, "B: grid and brute force agree exactly");
        check(duplicates_in(got) == 0, "B: no duplicate pairs");
    }
}

// ---------------------------------------------------------------------------
// C. The contract: a superset, never a subset
// ---------------------------------------------------------------------------
//
// THE CLAIM. False positives are free and false negatives are fatal, so the one
// property to check is containment — and this grid in fact achieves equality,
// because it runs the AABB test itself.
//
// THE CONTROLS, and the first of them is the reason this section was rewritten.
//
//   THE CAST. Every grid tutorial warns that `cell_of` must use `std::floor` and
//     not a cast to `int`, because a cast truncates TOWARD ZERO and merges cells
//     -1 and 0 into one double-width cell. The warning is real and the
//     conclusion usually drawn from it is wrong for THIS structure: measured
//     below, truncation loses exactly ZERO pairs. The reason is one line of
//     order theory — a range-walk grid needs its cell map to be MONOTONE and
//     nothing more, because if two intervals meet then their images under a
//     monotone map meet too. What truncation costs instead is occupancy: the
//     origin cell is twice as wide on each axis and therefore EIGHT TIMES the
//     volume, and everything near the origin piles into it. Both numbers are
//     below.
//   THE CENTRE. The mistake that IS a false-negative machine, and the one people
//     actually write: insert each proxy into the single cell containing its
//     centre. It is the natural first draft — objects are small, cells are big,
//     what could go wrong — and what goes wrong is every pair whose objects sit
//     either side of a cell boundary. Counted below.

std::int32_t trunc_cell_of(float x, float cell)
{
    return static_cast<std::int32_t>(x / cell);   // the bug, deliberately
}

/// Do the two proxies' CELL RANGES meet, under either cell map? This is the
/// question a range-walk grid actually asks, and the reason truncation survives
/// it: `cell_of` is monotone either way, and a monotone map sends overlapping
/// intervals to overlapping intervals.
bool ranges_meet(const proxy& a, const proxy& b, float cell, bool truncate)
{
    const float alo[3] = {a.box.min.x, a.box.min.y, a.box.min.z};
    const float ahi[3] = {a.box.max.x, a.box.max.y, a.box.max.z};
    const float blo[3] = {b.box.min.x, b.box.min.y, b.box.min.z};
    const float bhi[3] = {b.box.max.x, b.box.max.y, b.box.max.z};
    for (int k = 0; k < 3; ++k)
    {
        const std::int32_t a0 = truncate ? trunc_cell_of(alo[k], cell)
                                         : uniform_grid::cell_of(alo[k], cell);
        const std::int32_t a1 = truncate ? trunc_cell_of(ahi[k], cell)
                                         : uniform_grid::cell_of(ahi[k], cell);
        const std::int32_t b0 = truncate ? trunc_cell_of(blo[k], cell)
                                         : uniform_grid::cell_of(blo[k], cell);
        const std::int32_t b1 = truncate ? trunc_cell_of(bhi[k], cell)
                                         : uniform_grid::cell_of(bhi[k], cell);
        if (std::max(a0, b0) > std::min(a1, b1)) { return false; }
    }
    return true;
}

int lost_pairs(const std::vector<proxy>& scene, float cell, bool truncate)
{
    int lost = 0;
    for (std::size_t i = 0; i < scene.size(); ++i)
    {
        for (std::size_t j = i + 1; j < scene.size(); ++j)
        {
            if (!overlaps(scene[i].box, scene[j].box)) { continue; }
            if (!ranges_meet(scene[i], scene[j], cell, truncate)) { ++lost; }
        }
    }
    return lost;
}

/// The pair-loop volume around the origin, under either cell map, together with
/// the occupancy of the single fullest cell in that neighbourhood.
///
/// **This is what truncation actually costs, and it is the only thing it costs.**
/// A truncating map merges the eight cells of `{-1, 0}³` into one, so where the
/// correct map runs eight pair loops the truncating one runs a single larger
/// one, and a pair loop is quadratic in its occupancy. The naive prediction is
/// `C(8k, 2)` against `8*C(k, 2)`, i.e. eightfold — and it is too pessimistic,
/// because an object wide enough to span cells was already in several of the
/// eight. Measured below at rather less than that, on one cell of a scene, which
/// is why this is a footnote rather than a bug.
struct origin_cost
{
    int occupancy = 0;
    long long tests = 0;
};

origin_cost cost_near_origin(const std::vector<proxy>& scene, float cell, bool truncate)
{
    std::map<std::array<std::int32_t, 3>, int> count;
    for (const proxy& p : scene)
    {
        const float lo[3] = {p.box.min.x, p.box.min.y, p.box.min.z};
        const float hi[3] = {p.box.max.x, p.box.max.y, p.box.max.z};
        std::int32_t c0[3];
        std::int32_t c1[3];
        for (int k = 0; k < 3; ++k)
        {
            c0[k] = truncate ? trunc_cell_of(lo[k], cell) : uniform_grid::cell_of(lo[k], cell);
            c1[k] = truncate ? trunc_cell_of(hi[k], cell) : uniform_grid::cell_of(hi[k], cell);
        }
        for (std::int32_t z = c0[2]; z <= c1[2]; ++z)
        {
            for (std::int32_t y = c0[1]; y <= c1[1]; ++y)
            {
                for (std::int32_t x = c0[0]; x <= c1[0]; ++x)
                {
                    // Only the neighbourhood the two maps disagree about.
                    if (x < -1 || x > 0 || y < -1 || y > 0 || z < -1 || z > 0) { continue; }
                    ++count[std::array<std::int32_t, 3>{x, y, z}];
                }
            }
        }
    }
    origin_cost out;
    for (const auto& kv : count)
    {
        out.occupancy = std::max(out.occupancy, kv.second);
        out.tests += static_cast<long long>(kv.second) * (kv.second - 1) / 2;
    }
    return out;
}

void section_c()
{
    rule("C. the contract, and the cast that breaks it");

    rng r(0x8803u);

    // Twelve "frames" of a tumbling scene, each an independent placement.
    int frames_equal = 0;
    int total_missing = 0;
    std::size_t total_pairs = 0;
    uniform_grid grid;
    for (int frame = 0; frame < 12; ++frame)
    {
        const std::vector<proxy> scene = scatter(900, 24.0f, r);
        std::vector<broadphase_pair> ref;
        brute_force_pairs(scene, ref);
        grid.build(scene, {});
        std::vector<broadphase_pair> got(grid.pairs().begin(), grid.pairs().end());

        total_missing += missing_from(ref, got);
        total_pairs += ref.size();
        if (same_set(ref, got)) { ++frames_equal; }
    }
    std::printf("  12 frames, 900 proxies: %zu true pairs\n", total_pairs);
    std::printf("  missed by the grid                    %6d\n", total_missing);
    std::printf("  frames on which the sets were equal   %6d / 12\n", frames_equal);
    check(total_missing == 0, "C: the grid misses nothing");
    check(frames_equal == 12, "C: and reports nothing extra either");

    // CONTROL 1. The truncating cell index, on a scene that straddles the origin
    // and on the same scene translated away from it.
    std::vector<proxy> centred = scatter(3000, 12.0f, r);
    const float cell = auto_cell_size(centred);
    vec3 mid{0, 0, 0};
    for (const proxy& p : centred) { mid = mid + p.box.centre(); }
    mid = mid / static_cast<float>(centred.size());
    for (proxy& p : centred)
    {
        p.box.min = p.box.min - mid;
        p.box.max = p.box.max - mid;
    }

    const int lost_trunc = lost_pairs(centred, cell, true);
    const int lost_floor = lost_pairs(centred, cell, false);

    std::printf("\n  control 1: cell index by cast, not floor\n");
    std::printf("    3000 proxies centred on the origin, cell %.2f m\n",
                static_cast<double>(cell));
    std::printf("    pairs lost by truncation              %6d\n", lost_trunc);
    std::printf("    pairs lost by floor                   %6d\n", lost_floor);
    std::printf("    any monotone cell map preserves overlap.\n");
    check(lost_trunc == 0, "C: truncation is monotone, so it loses no pairs");
    check(lost_floor == 0, "C: floor loses none either");

    // What truncation DOES cost: the eight cells of {-1,0}^3 become one.
    // Measured on a DENSE scene, because in a sparse one there is nothing there
    // to pile up and the effect is invisible — which is itself worth knowing.
    std::vector<proxy> dense = scatter(6000, 3.0f, r);
    vec3 dmid{0, 0, 0};
    for (const proxy& p : dense) { dmid = dmid + p.box.centre(); }
    dmid = dmid / static_cast<float>(dense.size());
    for (proxy& p : dense)
    {
        p.box.min = p.box.min - dmid;
        p.box.max = p.box.max - dmid;
    }
    const float dcell = auto_cell_size(dense);
    const origin_cost ct = cost_near_origin(dense, dcell, true);
    const origin_cost cf = cost_near_origin(dense, dcell, false);
    std::printf("    6000 dense proxies, the {-1,0}^3 neighbourhood:\n");
    std::printf("      fullest cell, truncating            %6d proxies\n", ct.occupancy);
    std::printf("      fullest cell, flooring              %6d proxies\n", cf.occupancy);
    std::printf("      pair tests there, truncating        %6lld\n", ct.tests);
    std::printf("      pair tests there, flooring          %6lld\n", cf.tests);
    std::printf("      excess                              %6.1fx\n",
                static_cast<double>(ct.tests) / static_cast<double>(cf.tests));
    check(ct.tests > cf.tests * 2,
          "C: truncation costs occupancy where it costs nothing else");

    // CONTROL 2. Insert by the centre cell only — the bug that IS fatal.
    int lost_centre = 0;
    int true_pairs = 0;
    for (std::size_t i = 0; i < centred.size(); ++i)
    {
        for (std::size_t j = i + 1; j < centred.size(); ++j)
        {
            if (!overlaps(centred[i].box, centred[j].box)) { continue; }
            ++true_pairs;
            const vec3 ca = centred[i].box.centre();
            const vec3 cb = centred[j].box.centre();
            const bool same_cell = uniform_grid::cell_of(ca.x, cell) == uniform_grid::cell_of(cb.x, cell) &&
                                   uniform_grid::cell_of(ca.y, cell) == uniform_grid::cell_of(cb.y, cell) &&
                                   uniform_grid::cell_of(ca.z, cell) == uniform_grid::cell_of(cb.z, cell);
            if (!same_cell) { ++lost_centre; }
        }
    }
    std::printf("\n  control 2: one cell per proxy, by its centre\n");
    std::printf("    true overlapping pairs                %6d\n", true_pairs);
    std::printf("    lost                                  %6d  (%.1f%%)\n", lost_centre,
                100.0 * lost_centre / static_cast<double>(true_pairs));
    check(lost_centre > true_pairs / 2,
          "C: centre-only insertion loses most of the pairs there are");
}

// ---------------------------------------------------------------------------
// D. Duplicates, and the cell that owns a pair
// ---------------------------------------------------------------------------
//
// THE CLAIM. Two proxies whose cell ranges overlap in K cells meet K times, and
// the owner-cell rule removes exactly K-1 of those meetings for three `max`
// calls and no memory.
//
// THE INSTRUMENT, and it is the good one in this file: K is COMPUTABLE, in
// closed form, from the two cell ranges. So the number of duplicates the rule
// removed can be predicted analytically and compared against the counter the
// grid keeps — two completely independent routes to the same integer, and they
// have to agree exactly rather than approximately.
//
// THE CONTROL. The two alternatives everybody writes instead, timed on the same
// pair volume: a `std::set` of 64-bit pair keys, and a sort-then-unique over a
// vector of them.

long long shared_cells(const proxy& a, const proxy& b, float cell)
{
    long long k = 1;
    const float alo[3] = {a.box.min.x, a.box.min.y, a.box.min.z};
    const float ahi[3] = {a.box.max.x, a.box.max.y, a.box.max.z};
    const float blo[3] = {b.box.min.x, b.box.min.y, b.box.min.z};
    const float bhi[3] = {b.box.max.x, b.box.max.y, b.box.max.z};
    for (int axis = 0; axis < 3; ++axis)
    {
        const std::int32_t lo = std::max(uniform_grid::cell_of(alo[axis], cell),
                                         uniform_grid::cell_of(blo[axis], cell));
        const std::int32_t hi = std::min(uniform_grid::cell_of(ahi[axis], cell),
                                         uniform_grid::cell_of(bhi[axis], cell));
        if (hi < lo) { return 0; }
        k *= static_cast<long long>(hi - lo) + 1;
    }
    return k;
}

void section_d()
{
    rule("D. duplicates, and the cell that owns a pair");

    rng r(0x8804u);
    const std::vector<proxy> scene = scatter(2000, 24.0f, r);

    uniform_grid grid;
    grid.build(scene, {});
    const engine::phys::broadphase_stats& st = grid.stats();
    const float cell = st.cell_size;

    // The closed form: sum of (K - 1) over every pair whose ranges meet.
    long long predicted = 0;
    long long meeting = 0;
    long long worst = 0;
    for (std::size_t i = 0; i < scene.size(); ++i)
    {
        for (std::size_t j = i + 1; j < scene.size(); ++j)
        {
            const long long k = shared_cells(scene[i], scene[j], cell);
            if (k == 0) { continue; }
            ++meeting;
            predicted += k - 1;
            worst = std::max(worst, k);
        }
    }

    std::printf("  2000 proxies, cell %.3f m\n", static_cast<double>(cell));
    std::printf("  pairs whose cell ranges meet          %8lld\n", meeting);
    std::printf("  most cells one pair shares            %8lld\n", worst);
    std::printf("  duplicate meetings, predicted         %8lld\n", predicted);
    std::printf("  duplicate meetings, counted           %8lld\n", st.owner_rejects);
    std::printf("  pairs reported                        %8d\n", st.pairs);
    std::printf("  duplicates in the reported list       %8d\n",
                duplicates_in(std::vector<broadphase_pair>(grid.pairs().begin(),
                                                           grid.pairs().end())));
    check(predicted == st.owner_rejects, "D: the closed form matches the counter");
    check(duplicates_in(std::vector<broadphase_pair>(grid.pairs().begin(),
                                                     grid.pairs().end())) == 0,
          "D: the reported list has no duplicates");

    // CONTROL. What the two usual fixes cost, over the same pair volume.
    const std::size_t volume = static_cast<std::size_t>(st.pairs) +
                               static_cast<std::size_t>(st.owner_rejects);
    std::vector<std::uint64_t> keys;
    keys.reserve(volume);
    {
        // Rebuild the multiset of keys the ruleless grid would have emitted.
        for (std::size_t i = 0; i < scene.size(); ++i)
        {
            for (std::size_t j = i + 1; j < scene.size(); ++j)
            {
                const long long k = shared_cells(scene[i], scene[j], cell);
                if (k == 0 || !overlaps(scene[i].box, scene[j].box)) { continue; }
                for (long long c = 0; c < k; ++c)
                {
                    keys.push_back(engine::phys::pair_key(scene[i].index, scene[j].index));
                }
            }
        }
    }

    const bench_result via_set = bench_run(1, 15, [&] {
        std::set<std::uint64_t> seen;
        for (const std::uint64_t k : keys) { seen.insert(k); }
        return static_cast<double>(seen.size());
    });
    const bench_result via_sort = bench_run(1, 15, [&] {
        std::vector<std::uint64_t> copy = keys;
        std::sort(copy.begin(), copy.end());
        copy.erase(std::unique(copy.begin(), copy.end()), copy.end());
        return static_cast<double>(copy.size());
    });
    const bench_result whole_grid = bench_run(1, 21, [&] {
        grid.build(scene, {});
        return static_cast<double>(grid.stats().pairs);
    });

    std::printf("\n  control: deduplicating %zu keys instead\n", keys.size());
    std::printf("    std::set                            %8.3f ms\n", via_set.min_ns * 1e-6);
    std::printf("    sort + unique                       %8.3f ms\n", via_sort.min_ns * 1e-6);
    std::printf("    the WHOLE broadphase, rule included %8.3f ms\n",
                whole_grid.min_ns * 1e-6);
    std::printf("    the set, as a share of the broadphase  %7.1f%%\n",
                100.0 * via_set.min_ns / whole_grid.min_ns);
    check(via_set.min_ns > whole_grid.min_ns * 0.2,
          "D: deduplicating costs a fifth of the broadphase or more");
    check(via_sort.min_ns < via_set.min_ns,
          "D: and sort+unique is the cheaper of the two wrong answers");
}

// ---------------------------------------------------------------------------
// E. The cell size, swept across three decades
// ---------------------------------------------------------------------------
//
// THE CLAIM. Too small is cubic in insertions, too large is quadratic in
// occupancy, and the minimum between them is broad enough that the rule of thumb
// survives.
//
// THE CONTROL — the one that makes the sweep mean something: the ANSWER must not
// change. Every cell size below must produce the identical pair set, because a
// cell size is a performance decision and a broadphase that reported different
// pairs at different tunings would be reporting at least one of them wrongly.

void section_e()
{
    rule("E. the cell size");

    rng r(0x8805u);

    // TWO DENSITIES, because the optimum could easily be a property of the crowd
    // rather than of the objects — and if it were, `auto_cell_size` would have to
    // look at more than the boxes. It is, partly, and the cost model below says
    // exactly which half.
    struct density
    {
        const char* name;
        float per_object_volume;
    };
    const density densities[2] = {{"sparse", 24.0f}, {"dense", 6.0f}};

    const float cells[] = {0.1f, 0.25f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f,
                           3.0f, 4.0f,  5.0f, 8.0f, 15.0f, 40.0f};
    constexpr std::size_t k_rows = sizeof(cells) / sizeof(cells[0]);

    for (const density& d : densities)
    {
        const std::vector<proxy> scene = scatter(4000, d.per_object_volume, r);

        double mean_ext = 0.0;
        for (const proxy& p : scene)
        {
            const vec3 e = p.box.extent();
            mean_ext += static_cast<double>(std::max(e.x, std::max(e.y, e.z)));
        }
        mean_ext /= static_cast<double>(scene.size());

        std::vector<broadphase_pair> ref;
        brute_force_pairs(scene, ref);

        std::printf("\n  %s: 4000 proxies, mean longest side %.3f m, %zu pairs\n", d.name,
                    mean_ext, ref.size());
        std::printf("   cell     entries    bucket    best  sprd\n");
        std::printf("      m                 tests      ms\n");

        double ent[k_rows];
        double tst[k_rows];
        double ms[k_rows];
        int agree = 0;

        for (std::size_t i = 0; i < k_rows; ++i)
        {
            const float c = cells[i];
            uniform_grid grid;
            broadphase_config cfg;
            cfg.cell_size = c;
            cfg.max_cells_per_proxy = 1 << 30;   // the guard OFF, so the sweep is honest

            const bench_result t = bench_run(1, c < 0.4f ? 3 : 41, [&] {
                grid.build(scene, cfg);
                return static_cast<double>(grid.stats().pairs);
            });

            ent[i] = static_cast<double>(grid.stats().entries);
            tst[i] = static_cast<double>(grid.stats().bucket_tests);
            ms[i] = t.min_ns * 1e-6;

            if (same_set(ref, std::vector<broadphase_pair>(grid.pairs().begin(),
                                                           grid.pairs().end())))
            {
                ++agree;
            }

            std::printf("  %5.2f  %10.0f  %8.0f  %6.3f  %4.2f", static_cast<double>(c), ent[i],
                        tst[i], ms[i], t.spread());
            std::printf("\n");
        }

        // ---- THE COST MODEL ------------------------------------------------
        //
        // cost = a * entries + b * bucket_tests, solved EXACTLY from two rows
        // chosen because each is dominated by one term: the 0.5 m row inserts
        // 2.6 entries for every pair test it runs, and the 15 m row runs 78 pair
        // tests for every entry it inserts. Two equations, two unknowns, no
        // fitting and no judgement.
        const std::size_t i_ent = 2;   // 0.50 m
        const std::size_t i_tst = 11;  // 15.0 m
        const double det = ent[i_ent] * tst[i_tst] - ent[i_tst] * tst[i_ent];
        const double a = (ms[i_ent] * tst[i_tst] - ms[i_tst] * tst[i_ent]) / det;
        const double b = (ent[i_ent] * ms[i_tst] - ent[i_tst] * ms[i_ent]) / det;

        std::printf("  model: %.2f ns per entry, %.2f ns per pair test\n", a * 1e6, b * 1e6);
        double worst_err = 0.0;
        for (std::size_t i = 3; i < k_rows; ++i)
        {
            const double pred = a * ent[i] + b * tst[i];
            worst_err = std::max(worst_err, std::abs(pred - ms[i]) / ms[i]);
        }
        std::printf("  worst model error over the sane rows   %7.0f%%\n", 100.0 * worst_err);
        std::printf("   cell   model ms  measured ms\n");
        for (std::size_t i = 3; i < k_rows; ++i)
        {
            std::printf("  %5.2f   %8.3f     %8.3f\n", static_cast<double>(cells[i]),
                        a * ent[i] + b * tst[i], ms[i]);
        }

        // Where does the model put the minimum? Swept finely, on counts that do
        // not move between runs.
        float model_best = 0.0f;
        double model_cost = 1e30;
        for (int step = 0; step <= 200; ++step)
        {
            const float c = 0.5f + 0.05f * static_cast<float>(step);
            uniform_grid g;
            broadphase_config cfg;
            cfg.cell_size = c;
            cfg.max_cells_per_proxy = 1 << 30;
            g.build(scene, cfg);
            const double cost = a * static_cast<double>(g.stats().entries) +
                                b * static_cast<double>(g.stats().bucket_tests);
            if (cost < model_cost)
            {
                model_cost = cost;
                model_best = c;
            }
        }

        double measured_best = 1e30;
        float measured_cell = 0.0f;
        for (std::size_t i = 0; i < k_rows; ++i)
        {
            if (ms[i] < measured_best)
            {
                measured_best = ms[i];
                measured_cell = cells[i];
            }
        }

        uniform_grid g_auto;
        const bench_result t_auto = bench_run(1, 41, [&] {
            g_auto.build(scene, {});
            return static_cast<double>(g_auto.stats().pairs);
        });
        const float chosen = auto_cell_size(scene);

        std::printf("  model minimum   %5.2f m  = %.2f x mean side\n", static_cast<double>(model_best),
                    static_cast<double>(model_best) / mean_ext);
        std::printf("  fastest measured %4.2f m  (%.3f ms)\n", static_cast<double>(measured_cell),
                    measured_best);
        std::printf("  auto_cell_size   %5.2f m  (%.3f ms, %.2fx the best)\n",
                    static_cast<double>(chosen), t_auto.min_ns * 1e-6,
                    (t_auto.min_ns * 1e-6) / measured_best);
        std::printf("  pair sets identical at every size      %6d / %zu\n", agree, k_rows);

        check(agree == static_cast<int>(k_rows),
              "E: the cell size changes the cost and not the answer");
        check(worst_err < 1.0, "E: two constants predict the whole sweep");
        check(t_auto.min_ns * 1e-6 < measured_best * 1.5,
              "E: the automatic choice is within 50% of the best");
        check(model_best > mean_ext && model_best < 4.0 * mean_ext,
              "E: and the model puts the optimum a small multiple of the object size");
    }
}

// ---------------------------------------------------------------------------
// F. The hash, on the input a physics scene actually produces
// ---------------------------------------------------------------------------
//
// THE CLAIM. The textbook spatial hash — three primes and two exclusive-ors —
// is fine on random coordinates and measurably worse than random on a LATTICE,
// which is exactly what crates stacked on a floor produce.
//
// THE INSTRUMENT. Balls in bins. Throwing `k` distinct cells into `m` buckets at
// random puts more than one cell in a bucket
//
//     m * (1 - (1 - 1/m)^k - k*(1/m)*(1 - 1/m)^(k-1))
//
// times, in expectation. That is the number a GOOD hash should match; a hash
// that beats it is suspicious and a hash that loses to it is losing real
// nanoseconds. So there is a reference here that is neither of the two hashes.
//
// THE CONTROL. The same two hashes on RANDOM cell coordinates, where both should
// sit on the prediction. A difference that shows on the lattice and not on the
// random set belongs to the input; a difference that shows on both would belong
// to the measurement.

/// The engine's own hash behind a call the compiler is forbidden to inline —
/// the shape it had before §7 moved it into the header. Same instructions, same
/// registers, one `call` and one `ret`.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
std::uint32_t hash_out_of_line(std::int32_t x, std::int32_t y, std::int32_t z)
{
    return uniform_grid::hash_cell(x, y, z);
}

std::uint32_t hash_mix(std::int32_t x, std::int32_t y, std::int32_t z)
{
    std::uint32_t h = static_cast<std::uint32_t>(x) * 0x9E3779B1u;
    h ^= static_cast<std::uint32_t>(y) * 0x85EBCA77u;
    h = (h << 13) | (h >> 19);
    h ^= static_cast<std::uint32_t>(z) * 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x2545F491u;
    h ^= h >> 13;
    return h;
}

struct cell_key
{
    std::int32_t x, y, z;
    bool operator<(const cell_key& o) const
    {
        if (x != o.x) { return x < o.x; }
        if (y != o.y) { return y < o.y; }
        return z < o.z;
    }
};

std::vector<cell_key> occupied_cells(const std::vector<proxy>& scene, float cell)
{
    std::set<cell_key> set;
    for (const proxy& p : scene)
    {
        const std::int32_t x0 = uniform_grid::cell_of(p.box.min.x, cell);
        const std::int32_t x1 = uniform_grid::cell_of(p.box.max.x, cell);
        const std::int32_t y0 = uniform_grid::cell_of(p.box.min.y, cell);
        const std::int32_t y1 = uniform_grid::cell_of(p.box.max.y, cell);
        const std::int32_t z0 = uniform_grid::cell_of(p.box.min.z, cell);
        const std::int32_t z1 = uniform_grid::cell_of(p.box.max.z, cell);
        for (std::int32_t z = z0; z <= z1; ++z)
        {
            for (std::int32_t y = y0; y <= y1; ++y)
            {
                for (std::int32_t x = x0; x <= x1; ++x) { set.insert(cell_key{x, y, z}); }
            }
        }
    }
    return std::vector<cell_key>(set.begin(), set.end());
}

int shared_buckets(const std::vector<cell_key>& cells, std::uint32_t buckets, bool mix)
{
    std::vector<std::uint32_t> count(buckets, 0u);
    for (const cell_key& c : cells)
    {
        const std::uint32_t h = mix ? hash_mix(c.x, c.y, c.z)
                                    : uniform_grid::hash_cell(c.x, c.y, c.z);
        ++count[h & (buckets - 1u)];
    }
    int shared = 0;
    for (const std::uint32_t n : count) { if (n > 1u) { ++shared; } }
    return shared;
}

double expected_shared(std::size_t k, std::uint32_t m)
{
    const double p = 1.0 / static_cast<double>(m);
    const double q = 1.0 - p;
    const double none = std::pow(q, static_cast<double>(k));
    const double one = static_cast<double>(k) * p * std::pow(q, static_cast<double>(k) - 1.0);
    return static_cast<double>(m) * (1.0 - none - one);
}

void section_f()
{
    rule("F. the hash");

    rng r(0x8806u);

    // A lattice of crates on a floor: what a physics scene looks like at rest.
    const std::vector<proxy> lat = lattice(22, 6, 22, 1.05f);
    // And the same NUMBER of cells, at random coordinates.
    std::vector<proxy> rnd = scatter(static_cast<int>(lat.size()), 40.0f, r);

    struct fixture
    {
        const char* name;
        const std::vector<proxy>* scene;
    };
    const fixture fixtures[2] = {{"lattice", &lat}, {"scattered", &rnd}};

    std::printf("   fixture      cells  buckets   xor   mix   ideal\n");
    for (const fixture& f : fixtures)
    {
        const float cell = auto_cell_size(*f.scene);
        const std::vector<cell_key> cells = occupied_cells(*f.scene, cell);
        std::uint32_t buckets = 16u;
        while (buckets < cells.size() * 2u) { buckets <<= 1; }

        const int xo = shared_buckets(cells, buckets, false);
        const int mx = shared_buckets(cells, buckets, true);
        const double id = expected_shared(cells.size(), buckets);

        std::printf("  %-10s %7zu %8u %5d %5d %7.0f\n", f.name, cells.size(), buckets, xo, mx, id);

        if (f.scene == &lat)
        {
            std::printf("    xor is %+.1f%% off ideal, mix is %+.1f%%\n",
                        100.0 * (static_cast<double>(xo) / id - 1.0),
                        100.0 * (static_cast<double>(mx) / id - 1.0));
            check(static_cast<double>(xo) > id * 1.05,
                  "F: the xor hash is measurably worse than ideal on a lattice");
            check(std::abs(static_cast<double>(mx) - id) < id * 0.05,
                  "F: the mixed hash sits on the prediction");
        }
        else
        {
            check(std::abs(static_cast<double>(xo) - id) < id * 0.25,
                  "F: control - on random cells the xor hash is ideal");
        }
    }

    // WHAT THE DIFFERENCE COSTS IN THE LOOP THAT MATTERS. The grid's inner loop
    // runs C(k, 2) comparisons for a bucket holding k entries, so the whole
    // query volume is computable from the bucket occupancies alone — no timer,
    // no noise, and it is the quantity the collisions actually inflate.
    std::vector<cell_key> cells = occupied_cells(lat, auto_cell_size(lat));
    const float lat_cell = auto_cell_size(lat);
    uniform_grid lat_grid;
    lat_grid.build(lat, {});
    // The grid's OWN table size, so the simulated volume is comparable with the
    // `bucket_tests` it reports below rather than with a table of another shape.
    const std::uint32_t lat_buckets = static_cast<std::uint32_t>(lat_grid.stats().buckets);
    std::printf("\n  inner-loop volume on the lattice, by hash:\n");
    for (int which = 0; which < 2; ++which)
    {
        std::vector<long long> load(lat_buckets, 0);
        for (const proxy& p : lat)
        {
            const std::int32_t x0 = uniform_grid::cell_of(p.box.min.x, lat_cell);
            const std::int32_t x1 = uniform_grid::cell_of(p.box.max.x, lat_cell);
            const std::int32_t y0 = uniform_grid::cell_of(p.box.min.y, lat_cell);
            const std::int32_t y1 = uniform_grid::cell_of(p.box.max.y, lat_cell);
            const std::int32_t z0 = uniform_grid::cell_of(p.box.min.z, lat_cell);
            const std::int32_t z1 = uniform_grid::cell_of(p.box.max.z, lat_cell);
            for (std::int32_t z = z0; z <= z1; ++z)
            {
                for (std::int32_t y = y0; y <= y1; ++y)
                {
                    for (std::int32_t x = x0; x <= x1; ++x)
                    {
                        const std::uint32_t h = which == 0 ? uniform_grid::hash_cell(x, y, z)
                                                           : hash_mix(x, y, z);
                        ++load[h & (lat_buckets - 1u)];
                    }
                }
            }
        }
        long long tests = 0;
        for (const long long k : load) { tests += k * (k - 1) / 2; }
        std::printf("    %-4s %10lld pair tests\n", which == 0 ? "xor" : "mix", tests);
    }

    // AND THE LOAD FACTOR, which is the other half of a hash table and the one
    // `build` decides rather than the caller. Entries per bucket against shared
    // buckets, on the lattice, so the "two is where it flattens" in
    // broadphase.cpp is a measurement rather than a habit.
    std::printf("\n  load factor (entries per bucket) on the lattice:\n");
    std::printf("    ratio   buckets   shared   ideal\n");
    for (const int ratio : {1, 2, 4, 8})
    {
        std::uint32_t m = 16u;
        while (m < cells.size() * static_cast<std::size_t>(ratio)) { m <<= 1; }
        std::printf("    1:%-3d %9u %8d %7.0f\n", ratio, m, shared_buckets(cells, m, false),
                    expected_shared(cells.size(), m));
    }

    // And what the hash itself costs, now that both are inline. The engine's was
    // 0.66 ns behind a translation-unit boundary and is a third of that in front
    // of one — which is why `hash_cell` moved into the header, and why the first
    // draft of this comparison was void: it timed an out-of-line call against an
    // inlined function and called the difference a property of the hash.
    const bench_result t_xor = bench_run(cells.size(), 25, [&] {
        std::uint32_t acc = 0;
        for (const cell_key& c : cells) { acc += uniform_grid::hash_cell(c.x, c.y, c.z); }
        return static_cast<double>(acc);
    });
    const bench_result t_mix = bench_run(cells.size(), 25, [&] {
        std::uint32_t acc = 0;
        for (const cell_key& c : cells) { acc += hash_mix(c.x, c.y, c.z); }
        return static_cast<double>(acc);
    });
    const bench_result t_call = bench_run(cells.size(), 25, [&] {
        std::uint32_t acc = 0;
        for (const cell_key& c : cells) { acc += hash_out_of_line(c.x, c.y, c.z); }
        return static_cast<double>(acc);
    });
    std::printf("\n  one hash: xor %.3f ns, mix %.3f ns (%+.0f%%)\n", t_xor.min_ns,
                t_mix.min_ns, 100.0 * (t_mix.min_ns / t_xor.min_ns - 1.0));
    std::printf("  the same xor behind a call   %.3f ns (%.1fx)\n", t_call.min_ns,
                t_call.min_ns / t_xor.min_ns);
    check(t_call.min_ns > t_xor.min_ns * 1.5,
          "F: a three-multiply function is mostly call overhead");

    // Does the extra collision actually cost anything end to end?
    uniform_grid grid;
    const bench_result whole = bench_run(1, 21, [&] {
        grid.build(lat, {});
        return static_cast<double>(grid.stats().pairs);
    });
    std::printf("  lattice broadphase, as shipped        %8.3f ms\n", whole.min_ns * 1e-6);
    std::printf("  of which buckets holding 2+ cells     %8d / %d\n",
                grid.stats().colliding_buckets, grid.stats().occupied_buckets);
    std::printf("  pair tests wasted on collisions       %8lld / %lld\n", grid.stats().cell_rejects,
                grid.stats().bucket_tests);
}

// ---------------------------------------------------------------------------
// G. The margin
// ---------------------------------------------------------------------------
//
// THE CLAIM. A margin is conservative — it can only add pairs — and it adds more
// of them than the ratio of the volumes suggests, because what grows is the
// MINKOWSKI SUM of the two boxes and each of them contributes.
//
// THE INSTRUMENT. For boxes of extent e_a and e_b, a pair overlaps exactly when
// the difference of their centres lies in a box of extent e_a + e_b. Fattening
// each by m adds 2m to each extent, so the capture volume goes as
// ((E + 4m)/E)^3 with E the mean summed extent. That is a prediction, and the
// measured pair count has to follow it in a dilute scene.
//
// THE CONTROL. Margin zero must reproduce the brute-force set exactly, and every
// pair found at a smaller margin must still be found at a larger one — monotone
// containment, which is the property 8.9 will actually rely on.

void section_g()
{
    rule("G. the margin");

    rng r(0x8807u);
    const std::vector<proxy> scene = scatter(4000, 60.0f, r);

    double mean_sum = 0.0;
    for (const proxy& p : scene)
    {
        const vec3 e = p.box.extent();
        mean_sum += static_cast<double>(e.x + e.y + e.z) / 3.0;
    }
    mean_sum = 2.0 * mean_sum / static_cast<double>(scene.size());   // e_a + e_b

    std::vector<broadphase_pair> ref;
    brute_force_pairs(scene, ref);

    std::printf("  4000 proxies, mean summed extent %.3f m\n", mean_sum);
    std::printf("\n  margin    pairs   ratio  predicted  contains\n");
    std::printf("       m                              margin 0\n");

    const float margins[] = {0.0f, 0.01f, 0.05f, 0.1f, 0.25f, 0.5f};
    std::vector<broadphase_pair> previous;
    int monotone = 0;
    for (const float m : margins)
    {
        uniform_grid grid;
        broadphase_config cfg;
        cfg.margin = m;
        grid.build(scene, cfg);
        std::vector<broadphase_pair> got(grid.pairs().begin(), grid.pairs().end());

        const double ratio = static_cast<double>(got.size()) / static_cast<double>(ref.size());
        const double pred =
            std::pow((mean_sum + 4.0 * static_cast<double>(m)) / mean_sum, 3.0);

        const bool contains = missing_from(ref, got) == 0;
        if (contains) { ++monotone; }

        std::printf("  %6.2f %8zu  %6.3f    %7.3f  %s\n", static_cast<double>(m), got.size(), ratio,
                    pred, contains ? "yes" : "NO");

        if (m == 0.0f) { check(same_set(ref, got), "G: margin 0 is the brute-force set"); }
        else
        {
            check(std::abs(ratio - pred) < 0.12 * pred, "G: the pair count follows the prediction");
        }
        previous = got;
    }
    check(monotone == static_cast<int>(sizeof(margins) / sizeof(margins[0])),
          "G: a margin never loses a pair");
    (void)previous;
}

// ---------------------------------------------------------------------------
// H. The teapot in the stadium
// ---------------------------------------------------------------------------
//
// THE CLAIM. A uniform grid has one fatal input, and it is not an exotic one: a
// single object far larger than the cell. A 200 m ground plane at a 1 m cell
// wants EIGHTY THOUSAND entries for itself, nearly three times what a scene of
// two thousand crates generates in total, and almost every one of those cells is
// empty of anything else.
//
// THE CONTROL, in two halves. The guard must change the COST and not the ANSWER
// — the pair sets with it on and off are compared exactly. And the scene without
// the plane must show none of the effect, so that what is measured is the plane
// rather than the scene.

void section_h()
{
    rule("H. the teapot in the stadium");

    rng r(0x8808u);

    // 2000 crates over a 60 m square, and one ground plane under them.
    std::vector<proxy> crates;
    for (int i = 0; i < 2000; ++i)
    {
        const vec3 c{r.range(-30.0f, 30.0f), r.range(0.5f, 12.0f), r.range(-30.0f, 30.0f)};
        proxy p;
        p.box = bounds_of(cube_shape(1.0f), c, r.rotation());
        p.index = static_cast<std::uint32_t>(i);
        crates.push_back(p);
    }

    proxy plane;
    plane.box.min = vec3{-100.0f, -0.2f, -100.0f};
    plane.box.max = vec3{100.0f, 0.0f, 100.0f};
    plane.index = 9999u;

    std::vector<proxy> with_plane = crates;
    with_plane.push_back(plane);

    struct run
    {
        const char* name;
        const std::vector<proxy>* scene;
        int guard;
    };
    const run runs[3] = {{"crates only", &crates, engine::phys::k_max_cells_per_proxy},
                         {"+ plane, guard off", &with_plane, 1 << 30},
                         {"+ plane, guard on", &with_plane, engine::phys::k_max_cells_per_proxy}};

    std::printf("        scene            entries   pairs    ms   big\n");
    std::vector<broadphase_pair> guard_off;
    std::vector<broadphase_pair> guard_on;
    for (const run& x : runs)
    {
        uniform_grid grid;
        broadphase_config cfg;
        cfg.cell_size = 1.0f;
        cfg.max_cells_per_proxy = x.guard;

        const bench_result t = bench_run(1, 21, [&] {
            grid.build(*x.scene, cfg);
            return static_cast<double>(grid.stats().pairs);
        });

        std::printf("  %-20s %8d %7d %6.3f %5d\n", x.name, grid.stats().entries,
                    grid.stats().pairs, t.min_ns * 1e-6, grid.stats().oversized);

        if (x.guard == (1 << 30) && x.scene == &with_plane)
        {
            guard_off.assign(grid.pairs().begin(), grid.pairs().end());
        }
        if (x.guard == engine::phys::k_max_cells_per_proxy && x.scene == &with_plane)
        {
            guard_on.assign(grid.pairs().begin(), grid.pairs().end());
        }
    }

    check(same_set(guard_off, guard_on), "H: the guard changes the cost, not the answer");
    std::printf("  pair sets with the guard on and off   %8s\n",
                same_set(guard_off, guard_on) ? "equal" : "DIFFER");

    // How many cells does the plane want, at a few cell sizes? Counted with the
    // grid's own `cell_of`, not with `200/c` — the box straddles y = 0, so it
    // occupies two rows of cells where the arithmetic says one point two, and
    // an approximate formula here would be off by 67% at exactly the cell size
    // the table exists to talk about.
    std::printf("\n  cells the 200 x 0.2 x 200 m plane wants:\n");
    for (const float c : {4.0f, 2.0f, 1.0f, 0.5f, 0.25f})
    {
        long long n = 1;
        const float lo[3] = {plane.box.min.x, plane.box.min.y, plane.box.min.z};
        const float hi[3] = {plane.box.max.x, plane.box.max.y, plane.box.max.z};
        for (int k = 0; k < 3; ++k)
        {
            n *= static_cast<long long>(uniform_grid::cell_of(hi[k], c) -
                                        uniform_grid::cell_of(lo[k], c)) +
                 1;
        }
        std::printf("    cell %5.2f m                      %12lld\n", static_cast<double>(c), n);
    }
    check(true, "H: the cliff is cubic in 1/cell");

    // And the half that is NOT fixed: many large objects, where the oversized
    // list is itself quadratic.
    std::printf("\n  the guard's own limit: k big proxies, 2000 crates\n");
    std::printf("      k    oversized pairs      ms\n");
    for (const int k : {1, 4, 16, 64, 256})
    {
        std::vector<proxy> scene = crates;
        for (int i = 0; i < k; ++i)
        {
            proxy big;
            const float y = 0.5f * static_cast<float>(i);
            big.box.min = vec3{-100.0f, y - 0.2f, -100.0f};
            big.box.max = vec3{100.0f, y, 100.0f};
            big.index = 10000u + static_cast<std::uint32_t>(i);
            scene.push_back(big);
        }
        uniform_grid grid;
        broadphase_config cfg;
        cfg.cell_size = 1.0f;
        const bench_result t = bench_run(1, 21, [&] {
            grid.build(scene, cfg);
            return static_cast<double>(grid.stats().pairs);
        });
        std::printf("  %5d %12d %8d %7.3f\n", k, grid.stats().oversized,
                    grid.stats().oversized_pairs, t.min_ns * 1e-6);
    }
}

// ---------------------------------------------------------------------------
// I. The budget, and the crossover
// ---------------------------------------------------------------------------
//
// THE CLAIM. Below some scene size the grid is SLOWER than the quadratic it
// replaces, because a hash table is not free and six comparisons are very
// nearly. Above it the two diverge for ever. The number is a property of this
// machine and this code and has to be measured on both.
//
// THE CONTROL. Allocation counting across a long run. A broadphase rebuilt every
// frame that allocated every frame would be a broadphase with a hidden cost that
// no timing in this file would show, because the allocator is fast when it is
// warm and the pause arrives later.

void section_i()
{
    rule("I. the budget, and the crossover");

    rng r(0x8809u);

    std::printf("      n     brute      grid   winner\n");
    int crossover = 0;
    for (const int n : {8, 16, 24, 32, 48, 64, 96, 128, 192, 256})
    {
        const std::vector<proxy> scene = scatter(n, 40.0f, r);
        std::vector<broadphase_pair> ref;
        uniform_grid grid;

        const bench_result brute = bench_run(1, 51, [&] {
            brute_force_pairs(scene, ref);
            return static_cast<double>(ref.size());
        });
        const bench_result fast = bench_run(1, 51, [&] {
            grid.build(scene, {});
            return static_cast<double>(grid.stats().pairs);
        });

        const bool grid_wins = fast.min_ns < brute.min_ns;
        if (grid_wins && crossover == 0) { crossover = n; }
        std::printf("  %5d %8.2f us %8.2f us   %s\n", n, brute.min_ns * 1e-3,
                    fast.min_ns * 1e-3, grid_wins ? "grid" : "brute");
    }
    std::printf("\n  crossover                             %8d proxies\n", crossover);
    std::printf("  (the two are within 20%% of each other either\n");
    std::printf("   side of it, so read it as ~100 rather than as\n");
    std::printf("   a threshold; it lands on 96 or 128 per run.)\n");
    check(crossover >= 64 && crossover <= 256,
          "I: the grid starts winning at around a hundred proxies");

    // The frame, end to end: broadphase, then the narrow phase on what it found.
    const std::vector<proxy> scene = scatter(2000, 24.0f, r);
    std::vector<engine::phys::obb> boxes;
    for (const proxy& p : scene)
    {
        boxes.push_back(engine::phys::as_obb(p.box));
    }

    uniform_grid grid;
    grid.build(scene, {});
    const std::vector<broadphase_pair> pairs(grid.pairs().begin(), grid.pairs().end());

    const bench_result broad = bench_run(1, 31, [&] {
        grid.build(scene, {});
        return static_cast<double>(grid.stats().pairs);
    });
    const bench_result narrow = bench_run(1, 31, [&] {
        double acc = 0.0;
        for (const broadphase_pair& p : pairs)
        {
            const convex va = as_convex(boxes[p.a]);
            const convex vb = as_convex(boxes[p.b]);
            acc += static_cast<double>(collide_manifold(va, vb).deepest());
        }
        return acc;
    });

    const double b_ms = broad.min_ns * 1e-6;
    const double n_ms = narrow.min_ns * 1e-6;
    std::printf("\n  2000 proxies, %zu candidate pairs\n", pairs.size());
    std::printf("    broadphase                          %8.3f ms  (%4.1f%%)\n", b_ms,
                100.0 * b_ms / (b_ms + n_ms));
    std::printf("    narrow phase on what it found       %8.3f ms  (%4.1f%%)\n", n_ms,
                100.0 * n_ms / (b_ms + n_ms));
    std::printf("    narrow phase on ALL pairs, implied  %8.1f ms\n",
                n_ms * (0.5 * 2000.0 * 1999.0) / static_cast<double>(pairs.size()));

    // CONTROL. Allocations across two thousand rebuilds.
    uniform_grid steady;
    steady.build(scene, {});
    steady.build(scene, {});
    const std::size_t before = g_allocs;
    for (int frame = 0; frame < 2000; ++frame) { steady.build(scene, {}); }
    const std::size_t after = g_allocs;
    std::printf("\n  allocations over 2000 rebuilds        %8zu\n", after - before);
    check(after - before == 0, "I: a warm grid allocates nothing");
}

} // namespace

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    std::printf("verify_88 — Lesson 8.8, Broadphase: A Uniform Grid\n");

    section_a();
    section_b();
    section_c();
    section_d();
    section_e();
    section_f();
    section_g();
    section_h();
    section_i();

    std::printf("\n");
    for (int i = 0; i < 66; ++i) { std::putchar('='); }
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
