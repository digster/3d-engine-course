// scratch/golden_71.cpp — is the reference render byte-identical after 7.1?
//
// **A NULL INSTRUMENT, AND STRUCTURALLY SO — the fifth in a row.**
// Run it anyway. "I am confident it cannot have changed" is the sentence a test
// exists to check, and this one costs ninety seconds.
//
// THE STRUCTURAL ARGUMENT, which is what actually carries weight.
// `demo::write_reference_shot` renders through `soft_renderer.cpp`,
// `raster.cpp` and `framebuffer.cpp`, driven by `demos/common/demo_scene.cpp`,
// and that translation unit includes exactly six engine headers: colour,
// debug_draw, framebuffer, depth_buffer, raster and soft_renderer. Lesson 7.1
// added `math/euler.hpp` and a demo, and changed a doc comment in
// `math/transform.hpp` and one #include line in `engine/engine.hpp`. Of those:
//
//   - math/euler.hpp is a NEW header with no source file, reachable from
//     nothing the reference scene compiles. `grep -rn 'math/euler' engine/`
//     returns exactly one line, and it is inside `engine.hpp`.
//   - engine.hpp is included by NOTHING — that is the whole finding of 5.12's
//     §3, and it is still true, which is why the configure-time lint that
//     fired on this lesson had to exist.
//   - the transform.hpp change is a comment. It named Lesson 7.1 as the lesson
//     that replaces the stored `mat3` with a quaternion; 7.1 is Euler angles
//     and 7.4 is quaternions, so the comment was a promise to the wrong lesson.
//
// BEWARE THE GREP THAT LOOKS LIKE EVIDENCE. `grep -rn euler demos/common` DOES
// return two hits, and neither is this lesson: they are the **Euler
// characteristic**, V − E + F, which Lesson 3.5's mesh validator prints. Two
// unrelated things named after the same man, in the one directory this check is
// about. A structural argument made by grepping a name rather than an include
// path would have called that a dependency.
//
// So the picture cannot move, and a byte-identical result is evidence only that
// nobody edited a file they did not mean to.
//
// THE REAL INSTRUMENT for this lesson is verify_71, whose thirty-four checks
// include six controls that must FAIL for the measurement beside them to mean
// anything — and one of those controls did fail on the first run, at 65.51°
// against a threshold of 80, which is how the threshold got fixed.

#include "../demos/common/demo_scene.hpp"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

int main()
{
    const int rc = demo::write_reference_shot("scratch/golden71.ppm");
    auto read = [](const char* p) {
        std::ifstream f(p, std::ios::binary);
        std::ostringstream s;
        s << f.rdbuf();
        return s.str();
    };
    const std::string ours = read("scratch/golden71.ppm");
    const std::string golden = read("scratch/shot_52.ppm");

    // A FAILED READ MUST NOT SCORE AS A MATCH. Two empty strings are equal, and
    // Lesson 6.16 found this exact shape printing `identical=YES` on two failed
    // reads. The sizes are printed, not merely tested.
    if (ours.empty() || golden.empty())
    {
        std::printf("FAIL: could not read both files (ours=%zu golden=%zu). "
                    "Run from the repository root with the binary in build/demos/.\n",
                    ours.size(), golden.size());
        return 2;
    }
    if (ours.size() != golden.size())
    {
        std::printf("FAIL: sizes differ (ours=%zu golden=%zu)\n", ours.size(), golden.size());
        return 1;
    }

    std::size_t differing = 0;
    for (std::size_t i = 0; i < ours.size(); ++i)
    {
        if (ours[i] != golden[i]) { ++differing; }
    }
    std::printf("write_reference_shot rc=%d, %zu bytes, %zu differing\n",
                rc, ours.size(), differing);
    std::printf("%s\n", differing == 0 ? "GOLDEN BYTE-IDENTICAL (as it has been since 5.1)"
                                       : "GOLDEN MOVED");
    return differing == 0 ? 0 : 1;
}
