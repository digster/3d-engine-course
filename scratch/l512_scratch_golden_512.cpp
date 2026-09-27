// scratch/golden_512.cpp — is the reference render byte-identical after 5.12?
//
// **A NULL INSTRUMENT, AND STRUCTURALLY SO.**
// Run anyway: "I am confident it cannot have changed" is what a test is for.
//
// The structural argument, which is what actually carries weight here.
// `demo::write_reference_shot` renders through `soft_renderer.cpp`,
// `raster.cpp` and `framebuffer.cpp`, driven by `demos/common/demo_scene.cpp`.
// Lesson 5.12 created `gfx/renderable.{hpp,cpp}`, added a new demo, completed
// `engine.hpp`'s include list and added a CMake lint. Of those:
//
//   - renderable.cpp is a NEW translation unit that nothing in demo_scene.cpp
//     calls. It reads a registry; the reference scene has no registry.
//   - engine.hpp gained fourteen #include lines and no declarations. Nothing
//     under demos/common includes it at all (checked: grep).
//   - the CMake lint emits a message and never a compile flag.
//
// So the picture cannot move, and a byte-identical result is evidence only that
// nobody edited a file they did not mean to. That is worth ninety seconds.
//
// THE REAL INSTRUMENT for this lesson is verify_512 §B — the matrix round trip
// that every object drawn through `collect_renderables` depends on — and it
// carries a control that fires at 1.0.

#include "../demos/common/demo_scene.hpp"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

int main()
{
    const int rc = demo::write_reference_shot("scratch/golden512.ppm");
    auto read = [](const char* p) {
        std::ifstream f(p, std::ios::binary);
        std::ostringstream s;
        s << f.rdbuf();
        return s.str();
    };
    const std::string ours = read("scratch/golden512.ppm");
    const std::string golden = read("scratch/shot_52.ppm");

    // A FAILED READ MUST NOT SCORE AS A MATCH. Two empty strings are equal, and
    // Lesson 6.16 found this exact shape printing `identical=YES` on two failed
    // reads. The sizes are printed, not just tested.
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
    // NO ORDINAL. Every lesson since 5.1 has numbered this result ("the
    // eleventh lesson", "the twelfth"), and Lesson 5.12 is inserted into an
    // already-published chain — 6.1 claims the twelfth. Claiming a number here
    // would either collide with it or force an off-by-one correction through
    // eighteen published pages, for a count no reader can check. So: no number.
    std::printf("%s\n", differing == 0 ? "GOLDEN BYTE-IDENTICAL (as since 5.1)"
                                       : "GOLDEN MOVED");
    return differing == 0 ? 0 : 1;
}
