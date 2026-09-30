// scratch/verify_77b_ex5.cpp — Lesson 7.7b, Exercise 5's numbers (and §8's argument).
//
//     c++ -std=c++20 -O2 -I engine/include -I build/_deps/sdl3-src/include \
//         scratch/verify_77b_ex5.cpp build-rel/engine/libengine.a \
//         -L build-rel/_deps/sdl3-build -lSDL3 -Wl,-rpath,$PWD/build-rel/_deps/sdl3-build \
//         -o build/demos/verify_77b_ex5 && ./build/demos/verify_77b_ex5
//
// Import Blender's cubic walk at three tolerances, reduce at
// 0.5 deg, count keys, and measure the worst joint angle against a near-exact import.
#include <engine/anim/import.hpp>
#include <engine/gfx/gltf.hpp>
#include <engine/math/quat.hpp>
#include <cstdio>
#include <vector>
using namespace engine::anim;
int main()
{
    engine::gltf_scene_data f;
    (void)engine::load_gltf("scratch/mannequin_curves.glb", f);
    import_settings exact;
    exact.tolerance = {1e-6f, 8.72665e-6f, 1e-6f};   // 0.0005 deg
    exact.max_split = 256;
    imported_rig ref;
    (void)import_rig(f, 0, exact, ref);
    for (float tol_deg : {0.005f, 0.05f, 0.25f})
    {
        import_settings s;
        s.tolerance.rotation = tol_deg * 3.14159265f / 180.0f;
        s.max_split = 256;
        imported_rig r;
        const rig_import_report rep = import_rig(f, 0, s, r);
        std::size_t imported = 0, reduced = 0;
        float worst_imp = 0, worst_red = 0;
        for (std::size_t c = 0; c < r.clips.size(); ++c)
        {
            imported += validate(r.clips[c], r.sk).keys;
            clip red = r.clips[c];
            (void)reduce(red, r.sk, reduction_limits{});
            reduced += validate(red, r.sk).keys;
            std::vector<engine::transform> a, b, e;
            for (int i = 0; i <= 480; ++i)
            {
                const float t = r.clips[c].duration * static_cast<float>(i) / 480.0f;
                sample_at(ref.clips[c], ref.sk, t, e);
                sample_at(r.clips[c], r.sk, t, a);
                sample_at(red, r.sk, t, b);
                for (std::size_t j = 0; j < e.size(); ++j)
                {
                    worst_imp = std::max(worst_imp, engine::angle_between(e[j].rotation, a[j].rotation));
                    worst_red = std::max(worst_red, engine::angle_between(e[j].rotation, b[j].rotation));
                }
            }
        }
        // Two lines per tolerance, each under 66 columns (the page's <pre> fold).
        std::printf("import at %.3f deg: %4zu keys, %3zu after reduce (capped %zu)\n",
                    static_cast<double>(tol_deg), imported, reduced, rep.capped);
        std::printf("  worst vs exact: imported %.4f deg, reduced %.4f deg\n",
                    static_cast<double>(worst_imp * 57.2957795f),
                    static_cast<double>(worst_red * 57.2957795f));
    }
}
