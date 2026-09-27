// scratch/golden_75.cpp — is the reference render byte-identical after 7.4?
//
// **NOT A NULL INSTRUMENT THIS TIME, AND THAT IS THE NEWS.** Seven lessons in a
// row this harness has been run to confirm an argument that had already proved
// the picture could not move. Lesson 7.5 edits `math/transform.hpp`, which
// `scratch/closure_74.py` reports is IN the closure of `demos/common/demo_scene.cpp`
// — so the structural argument does not apply and the run is the only evidence
// there is.
//
//     closure: 66 files reachable from demo_scene.cpp + the software render path
//       outside                         engine/include/engine/math/quat.hpp
//       outside                         engine/include/engine/engine.hpp
//       outside                         demos/gimbal/main.cpp
//       IN CLOSURE  <-- golden must RUN  engine/include/engine/math/transform.hpp
//     VERDICT: golden must be run as a real instrument
//
// WHAT THE EDIT ACTUALLY WAS. Comments, and only comments: the promise in
// `transform`'s doc block moved from Lesson 7.4 to Lesson 7.5 and gained the
// measured reason it moved. Not one token of code changed, so the picture
// cannot move either — which is a second structural argument, and a WEAKER one
// than the closure argument it replaces, because it rests on a human reading a
// diff rather than on a graph walk. That asymmetry is the reason to run this.
//
// The rule the closure tool encodes is worth restating, since this is the first
// lesson where it fired. It is not "did you edit a file the renderer uses" — it
// is "is the file you edited REACHABLE by include from the thing the golden
// renders". Lesson 7.1 was caught grepping a NAME (`grep -rn euler
// demos/common` returns two hits, both the Euler characteristic from 3.5's mesh
// validator). Lesson 7.2 was caught grepping a PATH (`grep -rln
// "math/euler.hpp"` reports transform.hpp and rotation.hpp as includers, and
// NEITHER includes it — both merely mention the path in a doc comment). The
// tool walks `^#include <...>` transitively and is the only form that is right.
//
// THE OTHER INSTRUMENT for this lesson is verify_74, whose forty-two checks
// include ten controls that must FAIL for the measurement beside them to mean
// anything — including §C.6, which builds the WRONG sandwich, `conj(q) v q`,
// and demands that it reverse every composition rather than merely differ from
// the right one.

// BUILD AND RUN, and both halves of this matter:
//
//     cmake --build build --target demo_common engine
//     c++ -std=c++20 -Wall -Wextra -O2 \
//         -I engine/include -I demos/common -I build/_deps/sdl3-src/include \
//         scratch/golden_75.cpp \
//         build/demos/libdemo_common.a build/engine/libengine.a build/libimgui.a \
//         -L build/_deps/sdl3-build -lSDL3 \
//         -Wl,-rpath,$PWD/build/_deps/sdl3-build \
//         -o build/demos/golden_75
//     ./build/demos/golden_75          # FROM THE REPOSITORY ROOT
//
// THE BINARY GOES IN build/demos/ AND THE COMMAND IS RUN FROM THE ROOT, and
// they are two different requirements that have each cost a lesson.
//
//   The binary's LOCATION is the asset search path. Lesson 3.5's rule is that a
//   program finds its data with `SDL_GetBasePath()` — the directory the
//   executable lives in — and `engine_use_assets` is what puts `assets/` there.
//   Built one directory higher, this harness reported `identical=NO` WITH THE
//   CORRECT BYTE COUNT, because `torus.obj` was not found and two of the eight
//   shots drew nothing. A size check would have passed it. That was 7.2.
//
//   The WORKING DIRECTORY is where `scratch/` is. Run from `build/demos/`, both
//   the write and the reference read resolve to paths that do not exist — and
//   the first run of this file did exactly that, printing `ours=0 bytes
//   reference=0 bytes`. It did not score as a match, because 6.16 taught this
//   harness that two empty strings are equal and the guard below has been there
//   ever since. A test that fails loudly on its own misconfiguration is worth
//   the four lines it costs.

#include "../demos/common/demo_scene.hpp"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

int main()
{
    const int rc = demo::write_reference_shot("scratch/golden75.ppm");
    auto read = [](const char* p) {
        std::ifstream f(p, std::ios::binary);
        std::ostringstream s;
        s << f.rdbuf();
        return s.str();
    };
    const std::string ours = read("scratch/golden75.ppm");
    const std::string golden = read("scratch/shot_52.ppm");

    // A FAILED READ MUST NOT SCORE AS A MATCH. Two empty strings are equal, and
    // Lesson 6.16 found this exact shape printing `identical=YES` on two failed
    // reads. The sizes are printed, not merely tested.
    std::printf("golden_75: rc=%d  ours=%zu bytes  reference=%zu bytes\n",
                rc, ours.size(), golden.size());
    if (ours.empty() || golden.empty())
    {
        std::printf("golden_75: FAIL — a file did not read, which is not a match\n");
        return 1;
    }
    const bool same = (ours == golden);
    std::printf("golden_75: identical=%s\n", same ? "YES" : "NO");
    return same ? 0 : 1;
}
