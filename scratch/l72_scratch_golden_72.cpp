// scratch/golden_72.cpp — is the reference render byte-identical after 7.2?
//
// **A NULL INSTRUMENT, AND STRUCTURALLY SO — the sixth in a row.**
// Run it anyway. "I am confident it cannot have changed" is the sentence a test
// exists to check, and this one costs ninety seconds.
//
// THE STRUCTURAL ARGUMENT, AND THIS TIME IT WAS COMPUTED RATHER THAN GREPPED.
// Lesson 7.1 made this argument by grepping include paths, having first been
// caught out by grepping a NAME (`grep -rn euler demos/common` returns two hits
// and both are the Euler characteristic from 3.5's mesh validator). 7.2 found
// the next rung of the same ladder: `grep -rln "math/euler.hpp"` reports
// `math/transform.hpp` and `math/rotation.hpp` as includers, and NEITHER
// INCLUDES IT — both merely mention the path inside a doc comment. A path in
// prose looks exactly like a path in a directive to a substring search.
//
// So the argument is made by walking the include graph instead. Starting from
// the six engine headers `demos/common/demo_scene.cpp` includes — colour,
// debug_draw, framebuffer, depth_buffer, raster, soft_renderer — and following
// every `^#include <engine/...>` transitively, the math headers reachable are
// exactly:
//
//     mat3, mat4, transform, vec2, vec3, vec4
//
// `math/euler.hpp`, `math/axis_angle.hpp` and `math/rotation.hpp` are all
// UNREACHABLE. Lesson 7.2 touched those three, plus `engine/engine.hpp` (which
// 5.12 established is included by nothing) and `demos/gimbal/main.cpp` (which
// the reference scene does not link). Nothing else.
//
// The one edit that deserved a second look is `math/euler.hpp`, because unlike
// 7.1's new file this lesson REMOVED code from an existing header — the metric
// moved out to `math/rotation.hpp`. That is exactly the shape of change that
// breaks a distant translation unit by taking a declaration away from it. It
// cannot here, for two independent reasons: euler.hpp includes rotation.hpp, so
// every name it used to declare is still visible through it; and euler.hpp is
// not in the closure above anyway.
//
// So the picture cannot move, and a byte-identical result is evidence only that
// nobody edited a file they did not mean to.
//
// THE REAL INSTRUMENT for this lesson is verify_72, whose forty-two checks
// include eleven controls that must FAIL for the measurement beside them to mean
// anything — including §E.2, which reproduces Lesson 7.1's published 14.10% and
// 1.556x for the Euler lerp on 7.1's own pair. `rotation_slerp` reporting 0.00%
// excess is worth nothing unless the same instrument still reports 14.10% for
// the thing that genuinely detours.

#include "../demos/common/demo_scene.hpp"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

int main()
{
    const int rc = demo::write_reference_shot("scratch/golden72.ppm");
    auto read = [](const char* p) {
        std::ifstream f(p, std::ios::binary);
        std::ostringstream s;
        s << f.rdbuf();
        return s.str();
    };
    const std::string ours = read("scratch/golden72.ppm");
    const std::string golden = read("scratch/shot_52.ppm");

    // A FAILED READ MUST NOT SCORE AS A MATCH. Two empty strings are equal, and
    // Lesson 6.16 found this exact shape printing `identical=YES` on two failed
    // reads. The sizes are printed, not merely tested.
    std::printf("golden_72: rc=%d  ours=%zu bytes  reference=%zu bytes\n",
                rc, ours.size(), golden.size());
    if (ours.empty() || golden.empty())
    {
        std::printf("golden_72: FAIL — a file did not read, which is not a match\n");
        return 1;
    }
    const bool same = (ours == golden);
    std::printf("golden_72: identical=%s\n", same ? "YES" : "NO");
    return same ? 0 : 1;
}
