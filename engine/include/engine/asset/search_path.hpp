// engine/include/engine/asset/search_path.hpp — where an asset name turns into a file.
//
// Lesson 5.5. Until now this engine has had THREE different answers to "where do
// files live", and none of them knew about the others:
//
//   engine::asset_path()      (gfx/obj.hpp, Lesson 3.5) — SDL_GetBasePath() plus
//                             "assets/". Its own comment calls itself "a
//                             placeholder for Module 5's asset system".
//   the demo                  assembles std::strings around that call, once per
//                             load, at three call sites.
//   the shader loader         (gfx/gpu_shader.cpp) has its own copy of the same
//                             SDL_GetBasePath() logic for "shaders/".
//
// Three copies of one idea is the usual sign that the idea has no name. Its name
// is a SEARCH PATH: an ordered list of roots, and a rule for turning a NAME into
// the first file that answers to it.
//
// WHY ORDERED, AND WHY THAT IS THE WHOLE FEATURE. A single directory needs no
// list. What a list buys is OVERRIDE: put a directory in front and everything in
// it shadows the shipped asset of the same name, without moving, renaming or
// deleting anything. That one property is how every one of these works:
//
//   - mods and user content            (front root = the mod folder)
//   - a localisation or platform pack  (front root = assets_ja/, assets_lowend/)
//   - editing an asset while the game runs, out of the source tree rather than
//     out of the copy CMake made next to the binary (Module 8's hot reload)
//   - a test fixture standing in for a real asset, which is what verify_55 does
//
// A NAME IS NOT A PATH, and keeping them different types in your head is what
// makes the rest of the asset system possible. `"torus.obj"` is a name: it is
// stable, it is what a scene file records, and it is the key the asset store
// caches on. `/Users/…/build/demos/assets/torus.obj` is where that name resolved
// TODAY, on this machine, with these roots. Store the first; compute the second.

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace engine {

/// What `resolve()` found, or did not.
///
/// A report rather than a bare string, for the reason Lesson 5.3 settled: the
/// failure needs to say enough for somebody to act on it, and "" says nothing. A
/// missing asset is usually a missing *root*, and the caller cannot tell those
/// apart from an empty string.
struct resolved_path
{
    std::string path;          ///< the absolute path, or empty if not found
    int root_index = -1;       ///< which root answered; -1 if none did
    std::uint64_t bytes = 0;   ///< the file's size, which SDL hands us for free

    /// The name was rejected before any root was tried — absolute, empty, or
    /// containing `..`. Distinguished from "not found" because the two are
    /// different problems with different fixes, and because `resolve` has already
    /// logged this one: a caller that logs a second line for it produces two
    /// entries for one failure, which Lesson 5.3 forbade.
    bool refused = false;

    [[nodiscard]] bool ok() const { return root_index >= 0; }
};

/// An ordered list of directories, and the rule for finding a name inside them.
///
/// Copyable and cheap to pass; there is no global one, deliberately (see the
/// argument in `asset_store`). A program that wants two — a game and its editor,
/// say — simply has two.
class search_path
{
public:
    search_path() = default;

    /// The standard arrangement: `<base>/assets/`, where base is where the
    /// executable lives.
    ///
    /// **Next to the binary, never relative to the working directory.** CMake
    /// copies `assets/` beside every executable after each build, and
    /// `SDL_GetBasePath()` finds it wherever the user happened to be standing
    /// when they launched the program. That distinction is the difference between
    /// "works when I run it from the build folder" and "works" — it is Lesson
    /// 3.5's argument, kept, and now stated in one place instead of three.
    [[nodiscard]] static search_path standard();

    /// A path with one root: `<base>/<subdir>`, where base is where the
    /// executable lives.
    ///
    /// **The only place in the engine that calls `SDL_GetBasePath()`.** Assets,
    /// shaders and (Module 7) sounds each want their own root; they want the same
    /// *rule* for finding it, and one function is how you get one rule. Before
    /// Lesson 5.5 there were two copies of these four lines in two subsystems, and
    /// the second was written by copying the first — which is how you end up with
    /// two answers to a question that has one.
    [[nodiscard]] static search_path beside_executable(std::string_view subdir);

    /// Add a root to be searched LAST. Use for additional content.
    void add_root(std::string dir);

    /// Add a root to be searched FIRST. Use for overrides — a mod, a test
    /// fixture, a live edit out of the source tree.
    void prepend_root(std::string dir);

    [[nodiscard]] std::span<const std::string> roots() const { return roots_; }

    [[nodiscard]] bool empty() const { return roots_.empty(); }

    /// Find `name` in the roots, in order. The first hit wins.
    ///
    /// Existence is checked with `SDL_GetPathInfo(path, &info)`, which SDL3
    /// documents as returning false when the file does not exist — and which
    /// fills in the size at the same time, so a caller that wants to know how big
    /// an asset is does not pay for a second look at the filesystem.
    ///
    /// **A name containing `..` is refused.** Names come from scene files, from
    /// configuration, and eventually from users; a name that can climb out of
    /// every root is a name that can read `/etc/passwd`. This is the cheapest
    /// possible check and it belongs at the boundary where a name becomes a path,
    /// which is here.
    [[nodiscard]] resolved_path resolve(std::string_view name) const;

private:
    std::vector<std::string> roots_;
};

}   // namespace engine
