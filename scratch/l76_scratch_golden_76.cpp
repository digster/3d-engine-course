// scratch/golden_76.cpp — is the reference render byte-identical after 7.5?
//
// **NOT A NULL INSTRUMENT, FOR THE SECOND LESSON RUNNING.** Lesson 7.6 edits
// `math/transform.hpp` — it adds `local_from_parent`, the function Lesson 2.8
// declined to write until something needed it — and `scratch/closure_76.py`
// reports that file IN the closure of `demos/common/demo_scene.cpp`, so the
// structural argument does not apply and this run is the only evidence there is.
//
//     closure: 70 files reachable from demo_scene.cpp + the software render path
//       IN CLOSURE  <-- golden must RUN  engine/include/engine/math/transform.hpp
//       outside                         engine/include/engine/anim/skeleton.hpp
//       outside                         engine/include/engine/anim/skin.hpp
//       outside                         engine/include/engine/engine.hpp
//       outside                         demos/rig/main.cpp
//     VERDICT: golden must be run as a real instrument
//
// WHAT THE EDIT ACTUALLY WAS. One new `inline` function and a doc comment, in a
// header nothing in the closure calls — `parent_from_local` did not move a
// character and `local_from_parent` has no caller inside `demo_scene.cpp`'s
// world. So there is a second, structural argument that the picture cannot
// move, and it is WEAKER than the closure argument it would replace, because it
// rests on a human reading a diff rather than on a graph walk. That asymmetry is
// the whole reason to run this.
//
// It is not quite a null argument either. An added overload can change which
// function an existing call resolves to, and an added `inline` definition in a
// header changes what the optimiser has in front of it. Neither is likely; both
// are the kind of thing that is cheap to check and expensive to assume.
//
// The rule the closure tool encodes is worth restating. It is not "did you edit
// a file the renderer uses" — it is "is the file you edited REACHABLE by include
// from the thing the golden renders". Lesson 7.1 was caught grepping a NAME
// (`grep -rn euler demos/common` returns two hits, both the Euler characteristic
// from 3.5's mesh validator). Lesson 7.2 was caught grepping a PATH (`grep -rln
// "math/euler.hpp"` reports transform.hpp and rotation.hpp as includers, and
// NEITHER includes it — both merely mention the path in a doc comment). The tool
// walks `^#include <...>` transitively and is the only form that is right.
//
// THE OTHER INSTRUMENT for this lesson is verify_76, whose thirty-two checks
// include eleven controls that must FAIL for the measurement beside them to mean
// anything — including §D.3, which skins the mesh with the posed joint matrices
// and no inverse binds, and demands that the tube leave the building rather than
// merely differ.

// BUILD AND RUN, and both halves of this matter:
//
//     cmake --build build --target demo_common engine
//     c++ -std=c++20 -Wall -Wextra -O2 \
//         -I engine/include -I demos/common -I build/_deps/sdl3-src/include \
//         scratch/golden_76.cpp \
//         build/demos/libdemo_common.a build/engine/libengine.a build/libimgui.a \
//         -L build/_deps/sdl3-build -lSDL3 \
//         -Wl,-rpath,$PWD/build/_deps/sdl3-build \
//         -o build/demos/golden_76
//     ./build/demos/golden_76          # FROM THE REPOSITORY ROOT
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
    const int rc = demo::write_reference_shot("scratch/golden76.ppm");
    auto read = [](const char* p) {
        std::ifstream f(p, std::ios::binary);
        std::ostringstream s;
        s << f.rdbuf();
        return s.str();
    };
    const std::string ours = read("scratch/golden76.ppm");
    const std::string golden = read("scratch/shot_52.ppm");

    // A FAILED READ MUST NOT SCORE AS A MATCH. Two empty strings are equal, and
    // Lesson 6.16 found this exact shape printing `identical=YES` on two failed
    // reads. The sizes are printed, not merely tested.
    std::printf("golden_76: rc=%d  ours=%zu bytes  reference=%zu bytes\n",
                rc, ours.size(), golden.size());
    if (ours.empty() || golden.empty())
    {
        std::printf("golden_76: FAIL — a file did not read, which is not a match\n");
        return 1;
    }
    const bool same = (ours == golden);
    std::printf("golden_76: identical=%s\n", same ? "YES" : "NO");
    return same ? 0 : 1;
}
