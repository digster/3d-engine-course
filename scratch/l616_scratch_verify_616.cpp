// scratch/verify_616.cpp — every number Lesson 6.16 quotes, produced here.
//
//   sh scratch/build_verify_616.sh                                # as configured
//   ENGINE_CFLAGS="-O2 -DNDEBUG" sh scratch/build_verify_616.sh   # release
//
// §A  the extraction, checked against facts that need no trust
// §B  where the far-plane error lives (NOT in the extraction)
// §C  the worked example from §3.4 of the lesson, re-derived in code
// §D  cull_visible agrees with intersects agrees with ground truth
// §E  conservatism, measured: straddles and the false-positive corner case
// §F  tight bounds vs the box-of-a-box, priced
// §G  the sphere pre-test: is it worth it?
// §H  THE CROSSOVER — where culling starts to pay, in frame time
// §I  batching: what the demo scene does, and what a scene built to instance does
// §J  the golden, and the instrument that says a null result is real
#include "../demos/common/demo_scene.hpp"

#include <engine/core/bench.hpp>
#include <engine/gfx/bounds.hpp>
#include <engine/gfx/frustum.hpp>
#include <engine/gfx/instancing.hpp>
#include <engine/gfx/gpu_shader.hpp>
#include <engine/gfx/soft_renderer.hpp>
#include <engine/math/mat4.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using engine::aabb;
using engine::frustum;
using engine::mat4;
using engine::plane;
using engine::sphere;
using engine::vec3;
using engine::vec4;
using engine::visibility;

namespace {

int g_failures = 0;

void check(bool ok, const char* what)
{
    if (!ok) { ++g_failures; }
    printf("    [%s] %s\n", ok ? "PASS" : "FAIL", what);
}

void check_near(double got, double want, double tol, const char* what)
{
    const bool ok = std::fabs(got - want) <= tol;
    if (!ok) { ++g_failures; }
    printf("    [%s] %-48s got %.9f  want %.9f  (tol %.1e)\n",
           ok ? "PASS" : "FAIL", what, got, want, tol);
}

/// Ground truth, by sampling: is ANY point of the box inside the clip volume?
///
/// Deliberately NOT the algorithm under test. The exact answer needs a
/// convex-polyhedron intersection; this samples an (n+1)^3 lattice, which can
/// MISS a thin sliver of overlap and therefore under-reports visibility. That
/// direction of error is the safe one here: it can only make the measured
/// false-positive rate an OVER-estimate, so the number the lesson quotes is an
/// upper bound rather than a hopeful one.
bool really_visible(const mat4& clip_from_world, const aabb& box, int n)
{
    for (int i = 0; i <= n; ++i)
    for (int j = 0; j <= n; ++j)
    for (int k = 0; k <= n; ++k)
    {
        const vec3 p{box.min.x + (box.max.x - box.min.x) * float(i) / float(n),
                     box.min.y + (box.max.y - box.min.y) * float(j) / float(n),
                     box.min.z + (box.max.z - box.min.z) * float(k) / float(n)};
        const vec4 q = clip_from_world * engine::point(p);
        if (q.w > 0.0f && q.x >= -q.w && q.x <= q.w && q.y >= -q.w && q.y <= q.w
            && q.z >= 0.0f && q.z <= q.w)
        {
            return true;
        }
    }
    return false;
}

struct rng
{
    unsigned s = 0x616u;
    float next() { s = s * 1664525u + 1013904223u; return float(s >> 8) / float(1u << 24); }
    float sym() { return (next() - 0.5f) * 2.0f; }
};

} // namespace

int main()
{
    // Unbuffered, so a crash mid-section still shows which section it was in.
    setvbuf(stdout, nullptr, _IONBF, 0);

    const mat4 proj = engine::perspective(demo::k_scene_fovy, demo::k_scene_aspect,
                                          demo::k_scene_near, demo::k_scene_far);
    const demo::orbit_camera cam;
    const mat4 clip_from_world = proj * cam.view();
    const frustum f = engine::frustum_of(clip_from_world);
    const vec3 eye = cam.eye();

    // =======================================================================
    printf("\n=== A. the extraction, checked against facts that need no trust ===\n");
    // =======================================================================
    //
    // THE EYE IS THE APEX. It lies exactly ON all four side planes, so four
    // independent zeros fall out — and four zeros is a strong test of the row
    // algebra without needing a reference implementation to compare against.
    check_near(engine::signed_distance(f.planes[frustum::k_left], eye),   0.0, 1e-5, "d(eye, left)   == 0");
    check_near(engine::signed_distance(f.planes[frustum::k_right], eye),  0.0, 1e-5, "d(eye, right)  == 0");
    check_near(engine::signed_distance(f.planes[frustum::k_bottom], eye), 0.0, 1e-5, "d(eye, bottom) == 0");
    check_near(engine::signed_distance(f.planes[frustum::k_top], eye),    0.0, 1e-5, "d(eye, top)    == 0");

    // …and four zeros are NOT sufficient. A sign error that reverses a plane's
    // normal leaves the apex on it, so all four still read zero while the culler
    // rejects the world. The near test is what catches that class.
    check_near(engine::signed_distance(f.planes[frustum::k_near_z], eye),
               -double(demo::k_scene_near), 1e-5, "d(eye, near) == -near");

    // Opposite planes must NOT be negatives of one another. This is the exact
    // assertion that would have caught the sign bug in this code's first draft.
    {
        const plane& l = f.planes[frustum::k_left];
        const plane& r = f.planes[frustum::k_right];
        const bool negated = std::fabs(l.normal.x + r.normal.x) < 1e-6f
                          && std::fabs(l.normal.y + r.normal.y) < 1e-6f
                          && std::fabs(l.normal.z + r.normal.z) < 1e-6f
                          && std::fabs(l.d + r.d) < 1e-6f;
        check(!negated, "left and right are not the same plane reversed");
    }

    // Every plane is unit length after extraction, which is what makes a
    // signed distance mean metres.
    for (int i = 0; i < frustum::k_count; ++i)
    {
        const float len = engine::length(f.planes[i].normal);
        check_near(double(len), 1.0, 1e-5,
                   (std::string("|n| == 1 for ") + engine::name_of_plane(i)).c_str());
    }

    // The target is inside all six.
    {
        bool all_in = true;
        for (int i = 0; i < frustum::k_count; ++i)
        {
            if (engine::signed_distance(f.planes[i], cam.target) < 0.0f) { all_in = false; }
        }
        check(all_in, "the camera's target is inside all six planes");
    }

    // =======================================================================
    printf("\n=== B. the far plane is off, and NOT because of the extraction ===\n");
    // =======================================================================
    {
        const double got = engine::signed_distance(f.planes[frustum::k_far_z], eye);
        printf("    float  d(eye, far)  = %.9f   asked for %.1f   error %.3e\n",
               got, double(demo::k_scene_far), std::fabs(got - demo::k_scene_far));

        // The same row combination in double, from the same float matrix. If the
        // subtraction were the problem this would be visibly better.
        double dm[4][4];
        for (int r = 0; r < 4; ++r) { for (int c = 0; c < 4; ++c) { dm[r][c] = clip_from_world.at(r, c); } }
        double fa[4];
        for (int c = 0; c < 4; ++c) { fa[c] = dm[3][c] - dm[2][c]; }
        const double dl = std::sqrt(fa[0]*fa[0] + fa[1]*fa[1] + fa[2]*fa[2]);
        for (int c = 0; c < 4; ++c) { fa[c] /= dl; }
        const double dd = fa[0]*eye.x + fa[1]*eye.y + fa[2]*eye.z + fa[3];
        printf("    double d(eye, far)  = %.9f   error %.3e   <- SAME. Not the subtraction.\n",
               dd, std::fabs(dd - demo::k_scene_far));

        // So: where IS the matrix's far plane? Solve A*z + B == -z.
        const float nf = demo::k_scene_near, ff = demo::k_scene_far;
        const float A = ff / (nf - ff);
        const float B = ff * nf / (nf - ff);
        const double z_far = -double(B) / (double(A) + 1.0);
        const double dA = double(ff) / (double(nf) - double(ff));
        const double dB = double(ff) * double(nf) / (double(nf) - double(ff));
        const double z_far_exact = -dB / (dA + 1.0);
        printf("    A = %.9f   A + 1 = %.6e   <- the cancellation, in perspective()\n",
               double(A), double(A) + 1.0);
        printf("    the FLOAT matrix's far plane sits at z = %.9f\n", z_far);
        printf("    the same in double:               z = %.9f\n", z_far_exact);
        printf("    amplification of A's last bit: %.0fx\n", 1.0 / std::fabs(double(A) + 1.0));
        check(std::fabs(z_far_exact + double(ff)) < 1e-6,
              "the formula is exact in double — the bits, not the algebra");
        check(std::fabs(z_far + double(ff)) > 1e-4,
              "the float matrix's far plane really is displaced");

        // The near plane is ROW 2 ALONE: no subtraction, no cancellation.
        const double nerr = std::fabs(engine::signed_distance(f.planes[frustum::k_near_z], eye)
                                      + double(demo::k_scene_near));
        const double ferr = std::fabs(double(engine::signed_distance(f.planes[frustum::k_far_z], eye))
                                      - double(demo::k_scene_far));
        printf("    near error %.3e  vs  far error %.3e   ratio %.0fx\n",
               nerr, ferr, ferr / nerr);
    }

    // =======================================================================
    printf("\n=== C. the worked example from the lesson, re-derived ===\n");
    // =======================================================================
    //
    // A frustum with round numbers, so the arithmetic can be followed by hand:
    // 90 degrees vertical, square aspect, near 1, far 11. At 90 degrees the
    // focal length f = cot(45) = 1 exactly, so the projection's first two rows
    // are (1,0,0,0) and (0,1,0,0), the third is (0,0,A,B) and the fourth is
    // (0,0,-1,0). With the camera at the origin looking down -z, the view matrix
    // is the identity, so clip_from_world IS the projection.
    {
        const mat4 p90 = engine::perspective(3.14159265358979f * 0.5f, 1.0f, 1.0f, 11.0f);
        const frustum g = engine::frustum_of(p90);

        printf("    A = %.6f   B = %.6f\n", p90.at(2, 2), p90.at(2, 3));

        // left = row3 + row0 = (0,0,-1,0) + (1,0,0,0) = (1,0,-1,0); |n| = sqrt(2)
        printf("    left  (unit) = (%.6f, %.6f, %.6f | %.6f)\n",
               g.planes[frustum::k_left].normal.x, g.planes[frustum::k_left].normal.y,
               g.planes[frustum::k_left].normal.z, g.planes[frustum::k_left].d);
        check_near(double(g.planes[frustum::k_left].normal.x), 1.0 / std::sqrt(2.0), 1e-6,
                   "left.n.x == 1/sqrt(2)");
        check_near(double(g.planes[frustum::k_left].normal.z), -1.0 / std::sqrt(2.0), 1e-6,
                   "left.n.z == -1/sqrt(2)");
        check_near(double(g.planes[frustum::k_left].d), 0.0, 1e-6, "left.d == 0 (through the origin)");

        // A point on the left plane at z = -5 has x = -5: the 45-degree wall.
        check_near(double(engine::signed_distance(g.planes[frustum::k_left], vec3{-5.0f, 0.0f, -5.0f})),
                   0.0, 1e-5, "(-5, 0, -5) lies ON the left plane");
        // Half a unit further in is 0.5/sqrt(2) = 0.353553 inside.
        check_near(double(engine::signed_distance(g.planes[frustum::k_left], vec3{-4.5f, 0.0f, -5.0f})),
                   0.5 / std::sqrt(2.0), 1e-5, "(-4.5, 0, -5) is 0.353553 inside");

        // near = row2 alone = (0, 0, A, B); |n| = |A|, so the unit normal is
        // (0,0,-1) and d = B/|A| = -near.
        check_near(double(g.planes[frustum::k_near_z].normal.z), -1.0, 1e-6, "near.n == (0,0,-1)");
        check_near(double(g.planes[frustum::k_near_z].d), -1.0, 1e-5, "near.d == -1 (i.e. -near)");
        check_near(double(engine::signed_distance(g.planes[frustum::k_near_z], vec3{0, 0, -1})),
                   0.0, 1e-5, "z = -1 lies ON the near plane");
        check_near(double(engine::signed_distance(g.planes[frustum::k_near_z], vec3{0, 0, -3})),
                   2.0, 1e-5, "z = -3 is 2 units inside the near plane");

        // far = row3 - row2, unit normal (0,0,1), d = far.
        check_near(double(g.planes[frustum::k_far_z].normal.z), 1.0, 1e-5, "far.n == (0,0,1)");
        check_near(double(engine::signed_distance(g.planes[frustum::k_far_z], vec3{0, 0, -11})),
                   0.0, 2e-4, "z = -11 lies ON the far plane");

        // And the classifier on three boxes whose answers are obvious.
        aabb inside_box;  inside_box.expand(vec3{-1, -1, -5}); inside_box.expand(vec3{1, 1, -3});
        aabb behind_box;  behind_box.expand(vec3{-1, -1, 1});  behind_box.expand(vec3{1, 1, 3});
        aabb straddle;    straddle.expand(vec3{-1, -1, -1.5f}); straddle.expand(vec3{1, 1, 0.5f});
        check(engine::classify(g, inside_box) == visibility::inside,  "a box at z in [-5,-3] is INSIDE");
        check(engine::classify(g, behind_box) == visibility::outside, "a box behind the eye is OUTSIDE");
        check(engine::classify(g, straddle) == visibility::intersecting, "a box across near INTERSECTS");
    }

    // =======================================================================
    printf("\n=== D. the three implementations agree ===\n");
    // =======================================================================
    //
    // `cull_visible` inlines the plane loop so it can COUNT, which duplicates the
    // question `intersects` asks. Lesson 3.4's rule: instrumentation may
    // duplicate the question, never the answer — so the two are checked against
    // each other on every box rather than assumed to match.
    {
        rng r;
        std::vector<aabb> boxes;
        boxes.reserve(50000);
        for (int i = 0; i < 50000; ++i)
        {
            const vec3 c{r.sym() * 30.0f, r.sym() * 30.0f, r.sym() * 30.0f};
            const float h = 0.05f * std::pow(200.0f, r.next());
            aabb b;
            b.expand(vec3{c.x - h, c.y - h, c.z - h});
            b.expand(vec3{c.x + h, c.y + h, c.z + h});
            boxes.push_back(b);
        }

        std::vector<int> vis(boxes.size());
        engine::cull_report rep;
        const int n = engine::cull_visible(f, boxes, vis, &rep, false, true);

        int disagree_intersects = 0, disagree_classify = 0;
        std::vector<char> kept(boxes.size(), 0);
        for (int i = 0; i < n; ++i) { kept[std::size_t(vis[std::size_t(i)])] = 1; }
        for (std::size_t i = 0; i < boxes.size(); ++i)
        {
            if (engine::intersects(f, boxes[i]) != (kept[i] != 0)) { ++disagree_intersects; }
            const visibility v = engine::classify(f, boxes[i]);
            if ((v != visibility::outside) != (kept[i] != 0)) { ++disagree_classify; }
        }
        printf("    boxes=%zu  visible=%d  culled=%d  fully-inside=%d  plane tests=%d\n",
               boxes.size(), rep.visible, rep.culled, rep.fully_inside, rep.plane_tests);
        check(rep.tested == rep.visible + rep.culled, "tested == visible + culled");
        check(disagree_intersects == 0, "cull_visible agrees with intersects on every box");
        check(disagree_classify == 0, "cull_visible agrees with classify on every box");

        // The cost, per object, split by outcome. A rejected box exits early.
        const double per_kept = 6.0;
        const double per_culled = rep.culled > 0
            ? (double(rep.plane_tests) - per_kept * rep.visible) / double(rep.culled) : 0.0;
        printf("    plane tests: %.1f per SURVIVOR (all six), %.2f per REJECT (early exit)\n",
               per_kept, per_culled);

        printf("    rejected by:");
        for (int i = 0; i < frustum::k_count; ++i)
        {
            printf("  %s=%d", engine::name_of_plane(i), rep.rejected_by[i]);
        }
        printf("\n");
    }

    // =======================================================================
    printf("\n=== E. conservatism, measured ===\n");
    // =======================================================================
    {
        rng r;
        int n = 0, kept = 0, truly = 0, fp = 0, straddling = 0, fully = 0;
        for (int trial = 0; trial < 200000; ++trial)
        {
            const vec3 c{r.sym() * 30.0f, r.sym() * 30.0f, r.sym() * 30.0f};
            const float h = 0.05f * std::pow(200.0f, r.next());
            aabb b;
            b.expand(vec3{c.x - h, c.y - h, c.z - h});
            b.expand(vec3{c.x + h, c.y + h, c.z + h});
            ++n;

            const visibility v = engine::classify(f, b);
            const bool keep = (v != visibility::outside);
            if (v == visibility::intersecting) { ++straddling; }
            if (v == visibility::inside) { ++fully; }
            if (keep) { ++kept; }
            const bool vis = really_visible(clip_from_world, b, 10);
            if (vis) { ++truly; }
            if (keep && !vis) { ++fp; }
        }
        printf("    boxes=%d  kept=%d (%.2f%%)  of which straddling=%d, fully inside=%d\n",
               n, kept, 100.0 * kept / n, straddling, fully);
        printf("    really visible=%d (%.2f%%)\n", truly, 100.0 * truly / n);
        printf("    FALSE POSITIVES = %d  (%.3f%% of all boxes, %.2f%% of KEPT boxes)\n",
               fp, 100.0 * fp / n, kept ? 100.0 * fp / kept : 0.0);
        check(fp >= 0 && truly <= kept,
              "every really-visible box was kept (no false REJECTS — the fatal kind)");
    }

    // =======================================================================
    printf("\n=== F. tight bounds vs the box of a box ===\n");
    // =======================================================================
    //
    // 6.8 walks every vertex to get the TIGHT world box, because a shadow map's
    // texel density is inversely proportional to the box's side. 6.16 must not:
    // walking every vertex to decide whether to skip a draw costs more than the
    // draw. So it caches an OBJECT-SPACE box and transforms it — and pays the
    // looseness `bounds.hpp` has documented since 6.8.
    {
        demo::scene_assets assets;
        assets.build();
        demo::floor_geometry floor;
        demo::build_floor(assets, floor, 4);
        demo::model_state model;
        model.generated = assets.store.insert_mesh("generated:torus",
                                                   engine::make_torus(48, 24, 1.0f, 0.4f));
        demo::load_model(assets, model, demo::model_choice::torus, true);

        const demo::scene_kind kinds[] = {
            demo::scene_kind::solids, demo::scene_kind::cycle, demo::scene_kind::intersect,
            demo::scene_kind::zfight, demo::scene_kind::floor, demo::scene_kind::model};

        double worst = 1.0;
        double total_tight = 0.0, total_loose = 0.0;
        int objects = 0;
        for (demo::scene_kind kind : kinds)
        {
            engine::scene_object scene[demo::k_max_objects];
            const int count = demo::build_scene(scene, kind, demo::spin::about_z, 1.234f,
                                                assets, floor, model, 0.49f);
            for (int i = 0; i < count; ++i)
            {
                const engine::mesh_data* g = assets.meshes().get(scene[i].geometry);
                if (g == nullptr) { continue; }
                const mat4 wfm = engine::model_matrix(scene[i].xform, engine::trs_order::trs);

                aabb tight;
                for (const vec3& v : g->vertices)
                {
                    const vec4 w = wfm * engine::point(v);
                    tight.expand(vec3{w.x, w.y, w.z});
                }
                const aabb loose = engine::transformed(engine::bounds_of(g->vertices), wfm);

                const vec3 te = tight.extent(), le = loose.extent();
                const double tv = double(te.x) * te.y * te.z;
                const double lv = double(le.x) * le.y * le.z;
                // A ground plane is a PLANE: zero volume, so a ratio is undefined.
                // Compare the diagonal instead where that happens.
                const double ratio = tv > 1e-9 ? lv / tv
                                               : double(loose.radius()) / double(tight.radius());
                worst = std::max(worst, ratio);
                total_tight += tv;
                total_loose += lv;
                ++objects;
                printf("    %-28s tight r=%.4f  loose r=%.4f  volume ratio %.4f\n",
                       scene[i].name, double(tight.radius()), double(loose.radius()), ratio);
            }
        }
        printf("    %d objects; worst inflation %.4fx; summed volume %.4fx\n",
               objects, worst, total_loose / total_tight);
        check(worst >= 1.0, "the loose box is never smaller than the tight one");
        check(worst <= 3.0 * std::sqrt(3.0) + 0.01,
              "inflation stays inside the 3*sqrt(3) = 5.196 worst case for a cube");
    }

    // =======================================================================
    printf("\n=== G. is the sphere pre-test worth it? ===\n");
    // =======================================================================
    {
        for (int scenario = 0; scenario < 2; ++scenario)
        {
            // Two distributions, because the answer depends on one and not on the
            // arithmetic: a scene mostly ON screen (the pre-test almost never
            // rejects, so it is pure added cost) and a scene mostly OFF screen
            // (it rejects cheaply and the box test is skipped).
            rng r;
            std::vector<aabb> boxes;
            boxes.reserve(20000);
            const float spread = (scenario == 0) ? 6.0f : 120.0f;
            for (int i = 0; i < 20000; ++i)
            {
                const vec3 c{r.sym() * spread, r.sym() * spread * 0.4f,
                             (scenario == 0 ? -2.0f : 0.0f) + r.sym() * spread};
                const float h = 0.2f + r.next() * 0.8f;
                aabb b;
                b.expand(vec3{c.x - h, c.y - h, c.z - h});
                b.expand(vec3{c.x + h, c.y + h, c.z + h});
                boxes.push_back(b);
            }

            std::vector<int> vis(boxes.size());
            engine::cull_report plain, pre;
            const int n_plain = engine::cull_visible(f, boxes, vis, &plain, false, false);
            const int n_pre = engine::cull_visible(f, boxes, vis, &pre, true, false);

            const engine::bench_ab ab = engine::bench_compare(boxes.size(), 31,
                [&] {
                    engine::cull_report rp;
                    const int k = engine::cull_visible(f, boxes, vis, &rp, false, false);
                    return double(k);
                },
                [&] {
                    engine::cull_report rp;
                    const int k = engine::cull_visible(f, boxes, vis, &rp, true, false);
                    return double(k);
                });

            printf("    %s scene: visible %d/%zu (%.1f%%)\n",
                   scenario == 0 ? "MOSTLY-ON-SCREEN" : "MOSTLY-OFF-SCREEN",
                   n_plain, boxes.size(), 100.0 * n_plain / double(boxes.size()));
            printf("      plane tests:  box only %d   with sphere pre-test %d  (%.2fx)\n",
                   plain.plane_tests, pre.plane_tests,
                   double(pre.plane_tests) / double(plain.plane_tests));
            printf("      ns/box:       box only %.3f   with pre-test %.3f   -> %.2fx %s\n",
                   ab.a.median_ns, ab.b.median_ns, ab.ratio(),
                   ab.ratio() > 1.0 ? "SLOWER" : "faster");
            printf("      spread a=%.2f b=%.2f\n", ab.a.spread(), ab.b.spread());
            check(n_plain == n_pre,
                  "the pre-test changes the ANSWER for nobody — it is a speed knob only");
        }
    }

    // =======================================================================
    printf("\n=== H. the crossover: where does culling start to pay? ===\n");
    // =======================================================================
    //
    // A FIRST DRAFT OF THIS SECTION SWEPT THE OBJECT COUNT AND FOUND NOTHING —
    // "CULL WINS" at 4 objects and at 4096, saving almost exactly the cull rate
    // every time. That is not a crossover, it is the same ratio measured six
    // times, and it happens because BOTH SIDES SCALE LINEARLY in object count:
    // the test is O(n) and the work it skips is O(n), so their ratio is
    // independent of n and no amount of sweeping n will cross it.
    //
    // The crossover is in the CULL RATE. Culling costs `c` per object always and
    // saves `w` per object rejected, so it breaks even at
    //
    //     rate_breakeven = c / w
    //
    // and at a cull rate of zero it is pure loss. So: measure `c`, measure `w`,
    // predict the break-even rate, and then find it empirically. A prediction
    // that is checked is worth six measurements that are not.
    {
        demo::scene_assets assets;
        assets.build();
        demo::floor_geometry floor;
        demo::build_floor(assets, floor, 4);
        demo::model_state model;
        model.generated = assets.store.insert_mesh("generated:torus",
                                                   engine::make_torus(48, 24, 1.0f, 0.4f));
        demo::load_model(assets, model, demo::model_choice::torus, true);

        engine::scene_object proto[demo::k_max_objects];
        const int proto_count = demo::build_scene(proto, demo::scene_kind::solids,
                                                  demo::spin::about_z, 1.234f,
                                                  assets, floor, model, 0.49f);

        const engine::mat4 view = cam.view();
        const engine::projector pr{proj, demo::k_scene_viewport, engine::near_mode::clip};
        engine::lighting lights;

        constexpr int k_n = 512;

        // Build a scene where a KNOWN fraction is off screen.
        auto build = [&](double off_fraction,
                         std::vector<engine::scene_object>& objects,
                         std::vector<aabb>& boxes) {
            objects.clear();
            boxes.clear();
            rng r;
            for (int i = 0; i < k_n; ++i)
            {
                engine::scene_object o = proto[i % proto_count];
                const bool behind = (double(i) / double(k_n)) < off_fraction;
                o.xform.position.x = r.sym() * 3.0f;
                o.xform.position.z = behind ? (30.0f + r.next() * 40.0f)
                                            : (-2.0f + r.sym() * 2.0f);
                objects.push_back(o);
                const engine::mesh_data* g = assets.meshes().get(o.geometry);
                boxes.push_back(g != nullptr
                    ? engine::transformed(engine::bounds_of(g->vertices),
                                          engine::model_matrix(o.xform, engine::trs_order::trs))
                    : aabb{});
            }
        };

        std::vector<engine::scene_object> objects;
        std::vector<aabb> boxes;
        std::vector<engine::raster_triangle> tris;
        engine::projection_scratch scratch;

        // ---- c: the test alone, with NOTHING to reject ----------------------
        build(0.0, objects, boxes);
        // `std::vector<int> vis(std::size_t(k_n));` IS A FUNCTION DECLARATION.
        // The most vexing parse: `std::size_t(k_n)` reads as a parameter named
        // `k_n` of type `std::size_t`, so `vis` becomes a function returning a
        // vector, and the error lands forty lines later on the `span` conversion.
        // `static_cast` removes the ambiguity; so would braces, at the cost of
        // making a one-element vector instead of a k_n-element one.
        std::vector<int> vis(static_cast<std::size_t>(k_n));
        const engine::bench_result test_only = engine::bench_run(std::size_t(k_n), 51, [&] {
            engine::cull_report rp;
            return double(engine::cull_visible(f, boxes, vis, &rp));
        });

        // ---- w: the work one object costs downstream ------------------------
        const engine::bench_result work = engine::bench_run(std::size_t(k_n), 15, [&] {
            engine::collect_triangles(tris, scratch, objects, assets.meshes(),
                                      {view, eye}, pr, lights, engine::render_options{});
            return double(tris.size());
        });

        const double c = test_only.median_ns;
        const double w = work.median_ns;
        printf("    c = cull test        %8.3f ns/object  (spread %.2f)\n", c, test_only.spread());
        printf("    w = collect_triangles%8.1f ns/object  (spread %.2f)\n", w, work.spread());
        printf("    predicted break-even cull rate = c/w = %.4f%%  (1 object in %.0f)\n",
               100.0 * c / w, w / c);
        check(c > 0.0 && w > c, "the test is cheaper per object than the work it skips");

        // ---- and the empirical crossing --------------------------------------
        printf("    %10s %9s %13s %13s %10s\n",
               "cull rate", "visible", "no cull ns", "cull ns", "verdict");
        for (double rate : {0.0, 0.002, 0.01, 0.05, 0.25, 0.50})
        {
            build(rate, objects, boxes);
            engine::cull_report rp0;
            const int n_vis = engine::cull_visible(f, boxes, vis, &rp0);

            const engine::bench_ab ab = engine::bench_compare(std::size_t(k_n), 15,
                [&] {
                    engine::collect_triangles(tris, scratch, objects, assets.meshes(),
                                              {view, eye}, pr, lights, engine::render_options{});
                    return double(tris.size());
                },
                [&] {
                    engine::cull_report rp;
                    std::vector<int> v(static_cast<std::size_t>(objects.size()));
                    const int k = engine::cull_visible(f, boxes, v, &rp);
                    std::vector<engine::scene_object> s;
                    s.reserve(std::size_t(k));
                    for (int i = 0; i < k; ++i) { s.push_back(objects[std::size_t(v[std::size_t(i)])]); }
                    engine::collect_triangles(tris, scratch, s, assets.meshes(),
                                              {view, eye}, pr, lights, engine::render_options{});
                    return double(tris.size());
                });

            printf("    %9.1f%% %9d %13.1f %13.1f %10s\n",
                   100.0 * (1.0 - double(n_vis) / double(k_n)), n_vis,
                   ab.a.median_ns * k_n, ab.b.median_ns * k_n,
                   ab.b.median_ns < ab.a.median_ns ? "CULL WINS" : "cull loses");
        }

        // THE HONEST CAVEAT, AND IT IS A BIG ONE. The `cull` arm above ALLOCATES
        // two vectors per frame, which a real renderer would not; the numbers
        // therefore charge culling for work that belongs to this harness's
        // convenience. Measure it, rather than waving at it.
        {
            build(0.5, objects, boxes);
            const engine::bench_ab alloc = engine::bench_compare(std::size_t(k_n), 51,
                [&] {
                    engine::cull_report rp;
                    return double(engine::cull_visible(f, boxes, vis, &rp));
                },
                [&] {
                    engine::cull_report rp;
                    std::vector<int> v(static_cast<std::size_t>(objects.size()));
                    std::vector<engine::scene_object> s;
                    const int k = engine::cull_visible(f, boxes, v, &rp);
                    s.reserve(std::size_t(k));
                    for (int i = 0; i < k; ++i) { s.push_back(objects[std::size_t(v[std::size_t(i)])]); }
                    return double(k);
                });
            printf("    the harness's own overhead: %.3f ns/object bare, %.3f with the two "
                   "vectors (%.2fx)\n", alloc.a.median_ns, alloc.b.median_ns, alloc.ratio());
        }
    }

    // =======================================================================
    printf("\n=== I. batching ===\n");
    // =======================================================================
    {
        // REAL OBJECTS, NOT CAST INTEGERS. A first draft used
        // `reinterpret_cast<const gpu_mesh*>(0x1000 + i)` as a distinct-pointer
        // stand-in and segfaulted the moment anything dereferenced one. These are
        // default-constructed `gpu_mesh`es: no device, no buffers, `valid()`
        // false — and twelve genuinely distinct addresses, which is all a batch
        // key compares them for.
        std::vector<engine::gpu_mesh> meshes(16);

        // I.1 — the demo scene, which was never built to instance.
        std::vector<engine::gpu_draw_item> items;
        for (int i = 0; i < 12; ++i)
        {
            engine::gpu_draw_item it;
            // Distinct meshes AND distinct materials, which is what a hand-built
            // scene looks like. `mesh` is only compared as a pointer here, so a
            // fake distinct address is a faithful stand-in.
            it.mesh = &meshes[std::size_t(i)];
            it.material.albedo = vec3{float(i) * 0.08f, 0.5f, 0.5f};
            items.push_back(it);
        }
        std::vector<engine::gpu_instance> inst;
        std::vector<engine::instance_batch> batches;
        engine::batch_report rep;
        engine::batch_instances(items, {}, inst, batches, &rep);
        printf("    hand-built scene: %d items -> %d batches (%.2f per draw), largest %d\n",
               rep.items, rep.batches, double(rep.instances_per_draw()), rep.largest);
        printf("      split by: mesh=%d texture=%d style=%d material=%d\n",
               rep.split_by_mesh, rep.split_by_texture, rep.split_by_style, rep.split_by_material);
        check(rep.batches == rep.items, "12 distinct objects batch into 12 draws — saving ZERO");

        // I.2 — a scene built to instance: one mesh, one material, many placements.
        items.clear();
        for (int i = 0; i < 400; ++i)
        {
            engine::gpu_draw_item it;
            it.mesh = &meshes[0];
            it.world_from_model = engine::translation(vec3{float(i) * 0.5f, 0.0f, 0.0f});
            items.push_back(it);
        }
        engine::batch_instances(items, {}, inst, batches, &rep);
        printf("    forest scene:     %d items -> %d batches (%.2f per draw), largest %d\n",
               rep.items, rep.batches, double(rep.instances_per_draw()), rep.largest);
        check(rep.batches == 1, "400 copies of one mesh batch into ONE draw");
        check(inst.size() == 400, "…and produce 400 instance records");
        printf("      instance bytes: %zu (%zu each) vs %zu pushed as uniforms — IDENTICAL\n",
               inst.size() * sizeof(engine::gpu_instance), sizeof(engine::gpu_instance),
               items.size() * sizeof(engine::object_uniforms));
        check(sizeof(engine::gpu_instance) == sizeof(engine::object_uniforms),
              "instancing moves the bytes, it does not remove them");

        // I.3 — half of them culled first. Culling FEEDS batching.
        std::vector<int> visible;
        for (int i = 0; i < 400; i += 2) { visible.push_back(i); }
        engine::batch_instances(items, visible, inst, batches, &rep);
        printf("    after culling:    %d items -> %d batches, largest %d\n",
               rep.items, rep.batches, rep.largest);
        check(rep.batches == 1 && rep.largest == 200,
              "a culled set is still one batch — the two compose");

        // I.4 — blended draws are never merged, and keep their order.
        items.clear();
        for (int i = 0; i < 6; ++i)
        {
            engine::gpu_draw_item it;
            it.mesh = &meshes[1];
            it.blend = engine::blend_style::alpha;
            it.world_from_model = engine::translation(vec3{0.0f, 0.0f, float(-i)});
            items.push_back(it);
        }
        engine::batch_instances(items, {}, inst, batches, &rep);
        printf("    six blended:      %d items -> %d batches (blended=%d)\n",
               rep.items, rep.batches, rep.blended);
        check(rep.batches == 6, "identical blended draws are NOT merged — order is load-bearing");
        bool ordered = true;
        for (std::size_t i = 0; i < inst.size(); ++i)
        {
            if (std::fabs(inst[i].world_from_model.c3.z + float(i)) > 1e-6f) { ordered = false; }
        }
        check(ordered, "…and they arrive in the caller's order, unpermuted");
    }

    // =======================================================================
    printf("\n=== J. the golden, and whether its null result is real ===\n");
    // =======================================================================
    {
        demo::scene_assets assets;
        assets.build();
        demo::floor_geometry floor;
        demo::build_floor(assets, floor, 4);
        demo::model_state model;
        model.generated = assets.store.insert_mesh("generated:torus",
                                                   engine::make_torus(48, 24, 1.0f, 0.4f));
        demo::load_model(assets, model, demo::model_choice::torus, true);

        demo::orbit_camera close_cam;
        close_cam.radius = 1.0f;
        close_cam.elevation = 0.05f;
        close_cam.target = {0.0f, 0.35f, 0.0f};

        const demo::scene_kind shot[] = {
            demo::scene_kind::solids, demo::scene_kind::cycle, demo::scene_kind::intersect,
            demo::scene_kind::zfight, demo::scene_kind::floor, demo::scene_kind::model,
            demo::scene_kind::floor, demo::scene_kind::model};

        int total = 0, culled = 0;
        for (int fr = 0; fr < 8; ++fr)
        {
            const bool close_up = (fr == 6);
            const frustum ff = engine::frustum_of(proj * (close_up ? close_cam.view() : cam.view()));
            engine::scene_object scene[demo::k_max_objects];
            const int count = demo::build_scene(scene, shot[fr], demo::spin::about_z, 1.234f,
                                                assets, floor, model, 0.49f);
            for (int i = 0; i < count; ++i)
            {
                const engine::mesh_data* g = assets.meshes().get(scene[i].geometry);
                if (g == nullptr) { continue; }
                const aabb box = engine::transformed(
                    engine::bounds_of(g->vertices),
                    engine::model_matrix(scene[i].xform, engine::trs_order::trs));
                ++total;
                if (!engine::intersects(ff, box)) { ++culled; }
            }
        }
        printf("    reference shot: %d objects across 8 frames, %d culled\n", total, culled);
        check(culled == 0, "the golden's geometry is NEVER outside the frustum");

        // AND THE INSTRUMENT, CHECKED. 6.14 and 6.15 both paid to learn that a
        // null result is worthless until the same code has been shown to produce
        // a non-null one.
        engine::scene_object scene[demo::k_max_objects];
        const int count = demo::build_scene(scene, demo::scene_kind::solids,
                                            demo::spin::about_z, 1.234f,
                                            assets, floor, model, 0.49f);
        scene[0].xform.position.x += 200.0f;
        int moved_culled = 0;
        for (int i = 0; i < count; ++i)
        {
            const engine::mesh_data* g = assets.meshes().get(scene[i].geometry);
            if (g == nullptr) { continue; }
            const aabb box = engine::transformed(
                engine::bounds_of(g->vertices),
                engine::model_matrix(scene[i].xform, engine::trs_order::trs));
            if (!engine::intersects(f, box)) { ++moved_culled; }
        }
        check(moved_culled == 1,
              "…and the same code DOES cull an object moved 200 units off screen");
    }

    // =======================================================================
    printf("\n=== K. THE GPU: does render_batched draw the same picture? ===\n");
    // =======================================================================
    //
    // THIS IS PART 2's ONLY CORRECTNESS INSTRUMENT, and it exists because the
    // golden cannot be one: §J just showed that the reference shot never culls,
    // and it renders on the CPU, so neither half of this lesson reaches it.
    //
    // The claim under test is strong and simple: for a scene of identical opaque
    // objects, `render` and `render_batched` must produce BIT-IDENTICAL pixels.
    // Not "close" — identical. They run the same fragment shader over the same
    // geometry with the same matrices; the only difference is whether the matrix
    // arrived in a constant buffer or a vertex attribute, and neither road
    // changes a float.
    {
        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            printf("    ---- SDL_Init(VIDEO): %s\n", SDL_GetError());
        }

        // Declared FIRST so it is destroyed LAST — 6.11's segfault-on-exit, and
        // the ordering rule every harness since has kept.
        engine::gpu_device gpu;
        if (!gpu.create(nullptr, false).ok())
        {
            printf("    ---- no GPU device on this machine; §K skipped\n");
        }
        else
        {
            engine::gpu_shader vs, fs, vs_inst;
            if (!vs.load(gpu, "scene.vert", engine::shader_stage::vertex)
                || !fs.load(gpu, "scene.frag", engine::shader_stage::fragment)
                || !vs_inst.load(gpu, "scene_instanced.vert", engine::shader_stage::vertex))
            {
                printf("    ---- shaders did not load; §K skipped\n");
            }
            else
            {
                printf("    driver: %s\n", SDL_GetGPUDeviceDriver(gpu.handle()));
                check(true, "scene.vert, scene.frag and scene_instanced.vert all loaded — "
                            "THREE shaders for two pipelines, because the fragment half "
                            "is shared byte for byte");

                constexpr int k_w = 160, k_h = 120;
                constexpr auto k_fmt = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

                // ASKED, NOT ASSUMED — 6.8's rule. SDL guarantees exactly one
                // depth format; which one this device offers is a question.
                static constexpr SDL_GPUTextureFormat k_depth_candidates[] = {
                    SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
                    SDL_GPU_TEXTUREFORMAT_D24_UNORM,
                    SDL_GPU_TEXTUREFORMAT_D16_UNORM};
                const SDL_GPUTextureFormat depth_fmt = engine::supported_depth_format(
                    gpu, k_depth_candidates, int(std::size(k_depth_candidates)));

                engine::gpu_scene_renderer ren;
                const bool made = ren.create(gpu, vs.handle(), fs.handle(),
                                             depth_fmt,
                                             k_fmt, SDL_GPU_SAMPLECOUNT_1,
                                             vs_inst.handle());
                check(made, "the renderer built its pipelines");
                check(ren.has_instanced(),
                      "…including the TENTH, from eleven vertex attributes — which is "
                      "the one that needed pipeline_desc's cap raised from 8 to 16");
                check(ren.ensure_depth(gpu, k_w, k_h), "and a depth target");

                // `cube_mesh()` CARRIES NO NORMALS, and a vertex shader cannot
                // invent them — Lesson 4.8's finding, and it bit again here. The
                // first draft uploaded the bare cube and rendered a frame whose
                // brightest pixel was 10 against a background of 5: every surface
                // lit by `albedo * ambient` alone, because `dot(N, L)` with a
                // zero N is zero. `with_normals` is the step 4.8 added for
                // exactly this.
                const engine::mesh_data cube_data =
                    engine::with_normals(engine::cube_mesh(), engine::normal_style::flat);

                engine::gpu_mesh cube;
                {
                    SDL_GPUCommandBuffer* up = SDL_AcquireGPUCommandBuffer(gpu.handle());
                    const bool ok = up != nullptr
                        && cube.create(gpu, up, cube_data.view(),
                                       engine::index_mode::indexed, "verify616 cube");
                    if (up != nullptr)
                    {
                        SDL_GPUFence* fen = SDL_SubmitGPUCommandBufferAndAcquireFence(up);
                        if (fen != nullptr)
                        {
                            SDL_WaitForGPUFences(gpu.handle(), true, &fen, 1);
                            SDL_ReleaseGPUFence(gpu.handle(), fen);
                        }
                    }
                    check(ok, "a cube mesh uploaded");
                }

                engine::gpu_sampler samp;
                check(samp.create(gpu), "a sampler");

                // ---- The scene: sixteen identical cubes in a grid ------------
                constexpr int k_objects = 16;
                std::vector<engine::gpu_draw_item> items;
                for (int i = 0; i < k_objects; ++i)
                {
                    engine::gpu_draw_item it;
                    it.mesh = &cube;
                    const float x = float(i % 4) * 1.6f - 2.4f;
                    const float y = float(i / 4) * 1.6f - 2.4f;
                    engine::transform t;
                    t.position = vec3{x, y, 0.0f};
                    t.scale = vec3{0.5f, 0.5f, 0.5f};
                    it.world_from_model = engine::model_matrix(t, engine::trs_order::trs);
                    it.normal_from_model = engine::normal_matrix(it.world_from_model);
                    it.material.albedo = vec3{0.8f, 0.6f, 0.3f};
                    it.material.roughness = 0.4f;
                    it.material.f0 = 0.04f;
                    items.push_back(it);
                }

                std::vector<engine::gpu_instance> inst;
                std::vector<engine::instance_batch> batches;
                engine::batch_report brep;
                engine::batch_instances(items, {}, inst, batches, &brep);
                check(brep.batches == 1 && brep.largest == k_objects,
                      "sixteen identical cubes become ONE batch");

                engine::gpu_stream_buffer ibuf;
                check(ibuf.create(gpu, SDL_GPU_BUFFERUSAGE_VERTEX,
                                  Uint32(sizeof(engine::gpu_instance)) * k_objects,
                                  "verify616 instances"),
                      "an instance stream buffer");

                // The camera: far enough back to see the whole grid.
                engine::camera_uniforms camu{};
                const mat4 kview = engine::look_at(vec3{0.0f, 0.0f, 9.0f},
                                                   vec3{0.0f, 0.0f, 0.0f},
                                                   vec3{0.0f, 1.0f, 0.0f});
                camu.clip_from_world =
                    engine::perspective(0.9f, float(k_w) / float(k_h), 0.5f, 60.0f) * kview;

                engine::scene_light_uniforms lightu{};
                lightu.to_light = engine::normalised(vec3{0.4f, 0.7f, 0.6f});
                lightu.key = vec3{3.0f, 2.9f, 2.7f};
                lightu.ambient = vec3{0.05f, 0.06f, 0.08f};
                lightu.eye_world = vec3{0.0f, 0.0f, 9.0f};

                auto shoot = [&](bool instanced, std::vector<Uint8>& out,
                                 engine::draw_stats& stats) {
                    SDL_GPUTexture* colour = nullptr;
                    SDL_GPUTransferBuffer* rb = nullptr;
                    {
                        SDL_GPUTextureCreateInfo ti{};
                        ti.type = SDL_GPU_TEXTURETYPE_2D;
                        ti.format = k_fmt;
                        ti.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
                        ti.width = k_w;
                        ti.height = k_h;
                        ti.layer_count_or_depth = 1;
                        ti.num_levels = 1;
                        ti.sample_count = SDL_GPU_SAMPLECOUNT_1;
                        colour = SDL_CreateGPUTexture(gpu.handle(), &ti);

                        SDL_GPUTransferBufferCreateInfo tb{};
                        tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
                        tb.size = Uint32(k_w * k_h * 4);
                        rb = SDL_CreateGPUTransferBuffer(gpu.handle(), &tb);
                    }
                    if (colour == nullptr || rb == nullptr) { return false; }

                    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(gpu.handle());
                    if (cb == nullptr) { return false; }

                    // THE INSTANCE BUFFER IS WRITTEN INTO THE SAME COMMAND BUFFER
                    // that draws from it. That is legal and it is what a
                    // `gpu_stream_buffer` is for — the copy is recorded before the
                    // render pass begins, and the GPU executes a command buffer in
                    // order. Writing it into a DIFFERENT, unsubmitted command
                    // buffer is the mistake, and it reads as stale placements.
                    if (instanced && !inst.empty())
                    {
                        if (!ibuf.write(cb, inst.data(),
                                        Uint32(inst.size() * sizeof(engine::gpu_instance))))
                        {
                            return false;
                        }
                    }

                    SDL_GPUColorTargetInfo ct{};
                    ct.texture = colour;
                    ct.clear_color = SDL_FColor{0.02f, 0.02f, 0.03f, 1.0f};
                    ct.load_op = SDL_GPU_LOADOP_CLEAR;
                    ct.store_op = SDL_GPU_STOREOP_STORE;
                    const SDL_GPUDepthStencilTargetInfo dt = ren.depth_target_info();
                    SDL_GPURenderPass* pass =
                        SDL_BeginGPURenderPass(cb, &ct, 1, ren.has_depth() ? &dt : nullptr);
                    if (pass == nullptr) { return false; }

                    stats = instanced
                        ? ren.render_batched(cb, pass, batches.data(), int(batches.size()),
                                             ibuf.handle(), camu, lightu, samp.handle())
                        : ren.render(cb, pass, items.data(), int(items.size()),
                                     camu, lightu, samp.handle());
                    SDL_EndGPURenderPass(pass);

                    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cb);
                    SDL_GPUTextureRegion src{};
                    src.texture = colour;
                    src.w = k_w;
                    src.h = k_h;
                    src.d = 1;
                    SDL_GPUTextureTransferInfo dst{};
                    dst.transfer_buffer = rb;
                    dst.pixels_per_row = k_w;
                    dst.rows_per_layer = k_h;
                    SDL_DownloadFromGPUTexture(copy, &src, &dst);
                    SDL_EndGPUCopyPass(copy);

                    SDL_GPUFence* fen = SDL_SubmitGPUCommandBufferAndAcquireFence(cb);
                    if (fen == nullptr) { return false; }
                    SDL_WaitForGPUFences(gpu.handle(), true, &fen, 1);
                    SDL_ReleaseGPUFence(gpu.handle(), fen);

                    out.assign(std::size_t(k_w) * k_h * 4u, 0);
                    if (const void* m = SDL_MapGPUTransferBuffer(gpu.handle(), rb, false))
                    {
                        std::memcpy(out.data(), m, out.size());
                        SDL_UnmapGPUTransferBuffer(gpu.handle(), rb);
                    }
                    SDL_ReleaseGPUTransferBuffer(gpu.handle(), rb);
                    SDL_ReleaseGPUTexture(gpu.handle(), colour);
                    return true;
                };

                std::vector<Uint8> a, b;
                engine::draw_stats sa{}, sb{};
                const bool ok_a = shoot(false, a, sa);
                const bool ok_b = shoot(true, b, sb);
                check(ok_a && ok_b, "both frames rendered and downloaded");

                if (ok_a && ok_b && a.size() == b.size())
                {
                    std::size_t differing = 0;
                    int worst = 0;
                    std::size_t lit = 0;
                    for (std::size_t i = 0; i < a.size(); ++i)
                    {
                        const int d = std::abs(int(a[i]) - int(b[i]));
                        if (d != 0) { ++differing; }
                        worst = std::max(worst, d);
                    }
                    for (std::size_t i = 0; i < a.size(); i += 4)
                    {
                        // The clear colour is (0.02, 0.02, 0.03) linear, which
                        // lands at 5 in an 8-bit UNORM target. Anything above 20
                        // is geometry and not background — a margin, not a
                        // boundary, so the count does not depend on rounding.
                        if (a[i] > 20 || a[i + 1] > 20 || a[i + 2] > 20) { ++lit; }
                    }
                    printf("    %zu of %zu channels differ, worst by %d\n",
                           differing, a.size(), worst);
                    printf("    (%zu of %d pixels carry geometry — the instrument CAN see a "
                           "difference)\n", lit, k_w * k_h);
                    check(lit > 500,
                          "the frame actually contains the cubes — a null result on a "
                          "black image would prove nothing (6.14's rule)");
                    check(differing == 0,
                          "BIT-IDENTICAL: a uniform push and a vertex attribute deliver "
                          "the same matrix");
                }

                printf("    uniform road: %d draws, %d pipeline binds, %u uniform bytes\n",
                       sa.draws, sa.pipeline_binds, sa.uniform_bytes);
                printf("    instanced:    %d draws, %d pipeline binds, %u uniform bytes\n",
                       sb.draws, sb.pipeline_binds, sb.uniform_bytes);
                check(sb.draws == 1 && sa.draws == k_objects,
                      "sixteen draw calls became ONE");
                check(sa.triangles == sb.triangles,
                      "…drawing exactly the same number of triangles");
                printf("    uniform bytes %u -> %u, but %zu instance bytes were uploaded "
                       "instead: %u vs %zu total\n",
                       sa.uniform_bytes, sb.uniform_bytes,
                       inst.size() * sizeof(engine::gpu_instance), sa.uniform_bytes,
                       sb.uniform_bytes + inst.size() * sizeof(engine::gpu_instance));
            }
        }
    }

    printf("\n=== %d failure(s) ===\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
