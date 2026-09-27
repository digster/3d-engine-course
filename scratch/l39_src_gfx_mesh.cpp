// src/gfx/mesh.cpp — measuring geometry we did not write.
//
// Lesson 3.5. `mesh.hpp` has been header-only since Lesson 2.12, because a mesh was
// data and data needs no code. This file exists the moment geometry starts arriving
// from disk, and it holds the two things a hand-typed mesh never needed:
//
//   validate()    — does this data have the properties the renderer assumes?
//   make_torus()  — a mesh big enough that the answer is not obvious by eye.
//
// The validator is the interesting one, and the honest way to introduce it is to
// admit what it is for. Lesson 2.12 checked the icosahedron by hand: Euler's
// formula, every edge shared by exactly two faces, uniform vertex degree, outward
// winding. That was reasonable for twelve vertices we had typed ourselves. It is not
// reasonable for a file, and it is not reasonable EVER again — from here on the
// engine consumes data it did not author, and data it did not author gets checked.

#include "gfx/mesh.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <map>

namespace engine {
namespace {

/// A position reduced to the exact bits of its three floats, for welding.
///
/// **Exact equality, not a tolerance**, and that is a real simplification worth
/// naming. Two vertices a millionth of a unit apart are one point of the surface to
/// any human and two points to this function. Real weld tools quantise to a grid or
/// search a spatial hash within a radius, which costs a spatial structure and a
/// threshold nobody can choose correctly for all models. Exact matching is the
/// ninety-percent picture, and it is exactly right for the case we care about here:
/// a seam split by an EXPORTER, whose two copies came from one number and are
/// therefore bit-identical. Module 5's asset pipeline is where a tolerance belongs.
///
/// `std::memcpy` rather than a cast or a union, because reading a float through an
/// `unsigned*` is undefined behaviour in C++ (the strict-aliasing rule). Every
/// compiler turns this memcpy into no instructions at all; it is a note to the
/// optimiser about types, not a copy.
using position_key = std::array<std::uint32_t, 3>;

[[nodiscard]] position_key key_of(vec3 p)
{
    // NEGATIVE ZERO. -0.0f and +0.0f compare equal as floats but have different
    // bits, so a bitwise key would file them separately and a mesh whose seam
    // straddles an axis would not weld. Adding +0.0f normalises the sign of zero
    // (IEEE-754: (-0) + (+0) = +0) and leaves every other value untouched.
    const float xs = p.x + 0.0f;
    const float ys = p.y + 0.0f;
    const float zs = p.z + 0.0f;

    position_key k{};
    std::memcpy(&k[0], &xs, sizeof(float));
    std::memcpy(&k[1], &ys, sizeof(float));
    std::memcpy(&k[2], &zs, sizeof(float));
    return k;
}

/// How many times an undirected edge was traversed each way.
///
/// This pair of counters is the whole topology test. In a closed, consistently wound
/// surface every edge is shared by exactly two triangles, and those two triangles
/// walk it in OPPOSITE directions — because each walks its own boundary
/// counter-clockwise, and the shared edge is on the left of one and the right of the
/// other. So `forward == 1 && backward == 1` is the healthy case, and each way of
/// departing from it is a different, nameable defect:
///
///     forward + backward == 1     a rim. The surface has a boundary here.
///     forward + backward  > 2     three or more faces meet: not a surface.
///     forward == 2 (or back == 2) two faces, both walking the same way: one of
///                                 them is wound backwards.
struct edge_use
{
    int forward = 0;
    int backward = 0;
};

} // namespace

mesh_report validate(const mesh& m)
{
    mesh_report r;
    r.vertices = static_cast<int>(m.vertices.size());
    r.triangles = static_cast<int>(m.triangle_count());

    if (m.vertices.empty()) { return r; }

    // ---- Pass 1: weld positions ------------------------------------------
    //
    // `std::map`, not `std::unordered_map`, and the choice is deliberate rather than
    // careless. Validation runs ONCE, when an asset is loaded; the OBJ loader's
    // de-duplication runs per face and does reach for a hash map. Matching the
    // container to how hot the loop actually is — rather than always reaching for
    // the fastest one — is most of what "performance-aware" means in practice.
    std::map<position_key, int> welded;
    std::vector<int> weld_of(m.vertices.size(), 0);
    for (std::size_t i = 0; i < m.vertices.size(); ++i)
    {
        const auto [it, inserted] = welded.emplace(key_of(m.vertices[i]),
                                                   static_cast<int>(welded.size()));
        (void)inserted;
        weld_of[i] = it->second;
    }
    r.welded_vertices = static_cast<int>(welded.size());
    r.split_vertices = r.vertices - r.welded_vertices;

    // ---- Pass 2: walk the triangles --------------------------------------
    std::vector<bool> used(m.vertices.size(), false);
    std::map<std::uint64_t, edge_use> edges;
    std::map<int, bool> welded_used;   // welded ids that appear in a good triangle

    double volume6 = 0.0;   // six times the signed volume; divided at the end

    for (std::size_t f = 0; f < m.triangle_count(); ++f)
    {
        const std::uint16_t ia = m.indices[f * 3 + 0];
        const std::uint16_t ib = m.indices[f * 3 + 1];
        const std::uint16_t ic = m.indices[f * 3 + 2];

        // OUT OF RANGE FIRST, and skipped entirely. Every line below this one
        // indexes the arrays, so the check is not a nicety — it is the difference
        // between a diagnostic and a crash. A file is allowed to be wrong; a
        // program that reads files is not allowed to be surprised by it.
        if (ia >= m.vertices.size() || ib >= m.vertices.size() || ic >= m.vertices.size())
        {
            ++r.out_of_range;
            continue;
        }

        used[ia] = true;
        used[ib] = true;
        used[ic] = true;

        const int wa = weld_of[ia];
        const int wb = weld_of[ib];
        const int wc = weld_of[ic];

        // DEGENERATE, in the only sense that matters to topology: two of the three
        // corners are the same POINT. That happens for a repeated index, and equally
        // for two distinct array entries holding identical positions — which is
        // exactly what a seam split produces, so the welded ids are the right thing
        // to compare. Such a triangle covers no pixels, contributes no edges, and
        // would file a self-loop into the edge map if we let it through.
        const vec3 a = m.vertices[ia];
        const vec3 b = m.vertices[ib];
        const vec3 c = m.vertices[ic];
        const vec3 n = cross(b - a, c - a);
        if (wa == wb || wb == wc || wa == wc || n == vec3{})
        {
            ++r.degenerate;
            continue;
        }

        welded_used[wa] = true;
        welded_used[wb] = true;
        welded_used[wc] = true;

        // The signed volume of the tetrahedron this triangle forms with the ORIGIN,
        // times six: det[a, b, c] = dot(a, cross(b, c)). Summed over a closed
        // surface the parts outside the solid cancel exactly and what is left is the
        // enclosed volume — the divergence theorem, and §6.4 derives it. Accumulated
        // in `double` because it is a sum of signed terms that cancel almost
        // completely, which is the classic way to lose every digit you had.
        volume6 += static_cast<double>(dot(a, cross(b, c)));

        const int tri[3] = {wa, wb, wc};
        for (int e = 0; e < 3; ++e)
        {
            const int from = tri[e];
            const int to = tri[(e + 1) % 3];

            // One key per UNDIRECTED edge — the pair sorted — with the direction
            // recorded as which counter is bumped. Keying on the ordered pair
            // instead would file A→B and B→A separately and the whole test would
            // dissolve: every edge would look like a boundary.
            const int lo = (from < to) ? from : to;
            const int hi = (from < to) ? to : from;
            const std::uint64_t key = (static_cast<std::uint64_t>(lo) << 32)
                                    | static_cast<std::uint64_t>(hi);

            edge_use& u = edges[key];
            if (from == lo) { ++u.forward; } else { ++u.backward; }
        }
    }

    r.edges = static_cast<int>(edges.size());
    for (const auto& [key, u] : edges)
    {
        (void)key;
        const int total = u.forward + u.backward;
        if (total == 1)      { ++r.boundary_edges; }
        else if (total > 2)  { ++r.nonmanifold_edges; }
        if (u.forward > 1 || u.backward > 1) { ++r.reversed_edges; }
    }

    for (std::size_t i = 0; i < used.size(); ++i)
    {
        if (!used[i]) { ++r.unused_vertices; }
    }

    // V - E + F on the WELDED, REFERENCED graph. Vertices nobody names are not part
    // of the surface, and counting them would shift the characteristic by exactly
    // the number of them — which is why `unused_vertices` is reported alongside.
    const int good_faces = r.triangles - r.out_of_range - r.degenerate;
    r.euler = static_cast<int>(welded_used.size()) - r.edges + good_faces;

    r.signed_volume = static_cast<float>(volume6 / 6.0);
    return r;
}

void flip_uv_v(mesh_data& m)
{
    // One subtraction per uv, and no other consequence anywhere. Note in particular
    // that it does NOT touch the indices: flipping a texture coordinate moves where
    // a corner samples from, not which corners exist, so the vertex splits the
    // loader worked out in 3.5 are unaffected. (Flipping the *image* instead would
    // have been the other way to fix this, and it is worse: it duplicates work per
    // texture rather than per mesh, and it makes the texels disagree with the file
    // they came from.)
    for (vec2& uv : m.uvs) { uv.y = 1.0f - uv.y; }
}

mesh_data make_torus(int major_segments, int minor_segments,
                     float major_radius, float minor_radius)
{
    mesh_data out;
    if (major_segments < 3 || minor_segments < 3) { return out; }

    constexpr float k_tau = 6.28318530717958648f;   // 2 pi, one full turn

    const int nu = major_segments;
    const int nv = minor_segments;

    // (nu + 1) x (nv + 1) vertices for nu x nv quads, and the "+ 1" IS the seam.
    //
    // Going once around the ring, the last column of vertices sits at exactly the
    // same POSITIONS as the first — u = 1 and u = 0 are the same place. They are
    // stored twice anyway, because their texture coordinates differ: the seam vertex
    // must say u = 1 for the quad ending there and u = 0 for the quad starting
    // there, and one vertex cannot hold two values. That is the index problem
    // (Lesson 3.5 §3) appearing in geometry we generate ourselves, which is the best
    // possible evidence that it is not an artefact of some file format.
    out.vertices.reserve(static_cast<std::size_t>(nu + 1) * static_cast<std::size_t>(nv + 1));
    out.uvs.reserve(out.vertices.capacity());
    out.normals.reserve(out.vertices.capacity());

    for (int i = 0; i <= nu; ++i)
    {
        const float u = static_cast<float>(i) / static_cast<float>(nu);

        // THE ANGLE COMES FROM `i % nu`, NOT FROM `u`, and this is not a
        // micro-optimisation — it is what makes the seam weldable.
        //
        // The last column is the same place as the first, so its position must be
        // the same NUMBER, not merely the same point in principle. Computing it as
        // cos(1.0 * tau) gives 0.99999994-ish and sin(1.0 * tau) gives -1.7e-7,
        // because a float cannot hold 2*pi exactly. Those two copies then differ in
        // the last bit, no welder recognises them, and a watertight torus reports a
        // 48-edge hole along a seam that is not there. Wrapping the INDEX makes the
        // last column reuse the first column's angle exactly. Measured in §E.
        const int iu = i % nu;
        const float au = static_cast<float>(iu) / static_cast<float>(nu) * k_tau;
        const float cu = std::cos(au);
        const float su = std::sin(au);

        for (int j = 0; j <= nv; ++j)
        {
            const float v = static_cast<float>(j) / static_cast<float>(nv);
            const int jv = j % nv;              // same argument, around the tube
            const float av = static_cast<float>(jv) / static_cast<float>(nv) * k_tau;
            const float cv = std::cos(av);
            const float sv = std::sin(av);

            // The tube's centre circle sits at radius `major` in the xz plane; a
            // point on the surface is that circle pushed `minor` units in the
            // direction (cos v outward, sin v up) of the tube's own cross-section.
            const float ring = major_radius + minor_radius * cv;
            out.vertices.push_back({ring * cu, minor_radius * sv, ring * su});

            // The normal needs no derivative and no cross product: the surface point
            // is the tube centre plus `minor` times a unit vector, and THAT unit
            // vector is the normal. A sphere's normal is its position for the same
            // reason. Analytic normals are exact, and they are why a generated mesh
            // is a better test subject than a loaded one — any error is ours.
            out.normals.push_back({cv * cu, sv, cv * su});
            out.uvs.push_back({u, v});
        }
    }

    const auto index_of = [nv](int i, int j) {
        return static_cast<std::uint16_t>(i * (nv + 1) + j);
    };

    out.indices.reserve(static_cast<std::size_t>(nu) * static_cast<std::size_t>(nv) * 6);
    for (int i = 0; i < nu; ++i)
    {
        for (int j = 0; j < nv; ++j)
        {
            const std::uint16_t v00 = index_of(i, j);
            const std::uint16_t v01 = index_of(i, j + 1);
            const std::uint16_t v11 = index_of(i + 1, j + 1);
            const std::uint16_t v10 = index_of(i + 1, j);

            // (v00, v01, v11) and (v00, v11, v10) — note the ORDER, which is not the
            // one that reads naturally. Working the derivatives out by hand,
            // cross(dP/du, dP/dv) points INWARD for this parametrisation, so winding
            // a quad in the (u, v) direction would give every face the wrong way
            // round and back-face culling would show us the inside of the tube.
            // Swapping the two makes it outward. Verified, not assumed: the signed
            // volume of the result is positive and converges to 2·pi²·R·r².
            out.indices.push_back(v00); out.indices.push_back(v01); out.indices.push_back(v11);
            out.indices.push_back(v00); out.indices.push_back(v11); out.indices.push_back(v10);
        }
    }

    return out;
}

} // namespace engine
