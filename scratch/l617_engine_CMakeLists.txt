# engine/CMakeLists.txt — the engine, as a library.
#
# Lesson 5.1. Everything above this line in the course built ONE executable, and
# a single executable can be arranged however you like because nothing outside it
# can tell. A library is different in kind: it has an outside, and the outside is
# the part you can never take back once somebody depends on it.
#
# Three properties below do the entire job of drawing that boundary, and it is
# worth reading them as a group before reading them one at a time.

# STATIC, not SHARED, and not a choice made by shrugging.
#
# A static library is copied into each executable at link time. A shared one is a
# separate file the program finds at startup, which buys you the ability to swap
# the engine without relinking the game — and costs you an ABI you must not break,
# per-call indirection, and a deployment story ("which .dll did it load?").
#
# We are one repository building both sides from the same source, so we get none
# of the benefit and all of the cost. Engines that DO ship shared libraries do it
# for hot reload and for plugin boundaries; hot reload arrives in Module 9 and it
# reloads shaders and assets, not code. If that ever changes, this word is the
# only thing that has to.
add_library(engine STATIC
    # Lesson 5.5. A new directory rather than another file in gfx/: an asset
    # system is not a graphics subsystem — it loads meshes, images, and in
    # Module 7 sounds — and putting it under gfx/ would make the audio loader's
    # home a joke six lessons before it is written.
    src/asset/asset_store.cpp
    src/asset/search_path.cpp
    # Lesson 5.10. The action layer has a source file where the ECS did not,
    # and the reason is that it has real per-frame work to do: `update()` walks
    # the binding table and derives every action's level and edges. A header-only
    # version would put that loop in every translation unit that asks a question
    # about input, which is most of them.
    src/core/actions.cpp
    src/core/clock.cpp
    src/core/fixed_step.cpp
    src/core/input.cpp
    src/core/log.cpp          # Lesson 5.3
    src/core/profile.cpp
    # Lesson 6.11. Compositing, and the two enums that say what a surface's
    # alpha MEANS. Below `colour.cpp` in the list and above it in the dependency
    # graph: `blend.hpp` includes `colour.hpp` and nothing includes `blend.cpp`.
    # Lesson 6.14. Two prefilters that look unrelated and are the same
    # operation: a linear-light box down for coverage, and a widened NDF for
    # shading. A translation unit because the first is a loop over pixels and the
    # second pulls in <cmath>, and neither belongs in every TU that shades.
    src/gfx/antialias.cpp
    src/gfx/blend.cpp
    # Lesson 6.13. The bloom's four filters and the chain that orders them. A
    # translation unit and not a header: every function in it is a loop over
    # pixels, and inlining a loop over 129,600 texels into six call sites buys
    # nothing and costs a rebuild of all six whenever a kernel weight changes.
    src/gfx/bloom.cpp
    src/gfx/clip.cpp
    src/gfx/colour.cpp
    # Lesson 6.15. The environment map and the two integrals it feeds:
    # irradiance (exact for Lambert) and the split sum (approximate, and
    # measured). It sits in gfx/ rather than under a new `env/` because it
    # is a texture type, not a subsystem — the same argument gpu_texture.hpp
    # makes for keeping depth targets beside sampled ones.
    src/gfx/cubemap.cpp
    src/gfx/debug_draw.cpp
    # Lesson 5.11. The QUEUE, split from the drawing above it. Two files rather
    # than one because their include lists must differ: this one is reachable
    # from a physics system, debug_draw.cpp is reachable only from a renderer.
    src/gfx/debug_lines.cpp
    src/gfx/depth_buffer.cpp
    # Lesson 6.11. Bucketing and back-to-front sorting. A translation unit rather
    # than a header because `order_draws` is a real algorithm over a span, and
    # 6.16's frustum culling walks the same list — one place, not two.
    src/gfx/draw_order.cpp
    src/gfx/framebuffer.cpp
    # Lesson 6.17. The frame, declared instead of assembled. A translation unit
    # because `compile` is four real passes over two arrays and belongs in one
    # place, and because nothing that merely *declares* a pass should have to
    # recompile when the scheduler changes.
    src/gfx/frame_graph.cpp
    # Lesson 6.16. The six planes, extracted from the view-projection matrix, and
    # the walk that rejects what they exclude. A translation unit for the reason
    # draw_order.cpp gives one line above: `cull_visible` is a real algorithm over
    # a span, and the extraction has six sign conventions in it that must have
    # exactly one home.
    src/gfx/frustum.cpp
    src/gfx/gpu_buffer.cpp
    src/gfx/gpu_debug.cpp
    src/gfx/gpu_device.cpp
    src/gfx/gpu_mesh.cpp
    # Lesson 6.16. Grouping a culled set into instanced draws. Beside gpu_mesh
    # rather than inside it, because a mesh that knew how many copies of itself
    # exist would be deciding something that belongs to the scene — which is the
    # rule `gpu_mesh::bind` has stated since 4.5 and is why it refuses to bind
    # slot 1.
    src/gfx/instancing.cpp
    src/gfx/gpu_pipeline.cpp
    # Lesson 6.12. The first pass that draws no geometry: one full-screen
    # triangle, one texture in, one texture out. 6.13 turns it into a stack.
    src/gfx/gpu_post.cpp
    src/gfx/gpu_present.cpp
    src/gfx/gpu_scene.cpp
    src/gfx/gpu_shader.cpp
    # Lesson 6.8. The GPU half of the shadow map: a second render pass, a
    # pipeline with no colour target, and a depth texture that can be sampled.
    src/gfx/gpu_shadow.cpp
    src/gfx/gpu_texture.cpp
    # Lesson 6.12. Exposure, the tonemap curves and the CPU resolve. A translation
    # unit rather than a header because `resolve` is a full-screen loop and
    # `measure` walks every pixel — neither belongs in every file that mentions a
    # colour.
    src/gfx/hdr.cpp
    # Lesson 6.6. The second asset format, and it gets its own translation
    # unit for the same reason obj.cpp has one: it is the ONLY file allowed
    # to see cgltf, so the dependency has exactly one place to leak from.
    src/gfx/gltf.cpp
    src/gfx/image.cpp
    src/gfx/mipmap.cpp
    src/gfx/mesh.cpp
    src/gfx/obj.cpp
    src/gfx/raster.cpp
    # Lesson 6.8. The shadow map, and it gets a translation unit rather than a
    # header because it owns buffers and reaches into the whole software
    # pipeline — `collect_triangles` and `draw_triangles` both — which is more
    # than a header should drag into every file that mentions a light.
    src/gfx/cascade.cpp
    src/gfx/shadow.cpp
    src/gfx/soft_renderer.cpp
    src/gfx/texture.cpp
    # Lesson 5.2. Note what is NOT here: platform/main.hpp, which carries
    # ENGINE_MAIN. It is header-only and deliberately so — including it emits a
    # real `main`, and an entry point is not a library's to own. See the comment
    # block at the top of that file.
    src/platform/app.cpp
    src/platform/platform.cpp
    # Lesson 5.11. A new directory again, and the same argument asset/ made in
    # 5.5: tooling UI is not a graphics subsystem. Module 9's editor lands here
    # beside it, and the top-level tools/ directory is a different thing — the
    # executables that USE the engine, not part of it.
    src/ui/debug_ui.cpp
)

# `engine::engine` is an ALIAS, and aliases exist for one reason: a name with a
# double colon in it CANNOT be mistaken for a file. If a demo asks to link
# `engine::engine` and no such target exists, CMake stops at configure time with
# "target not found". If it asks to link plain `engine` and no such target
# exists, CMake happily passes `-lengine` to the linker and you find out much
# later, in a different language, from a different tool. Every library worth
# using exports a namespaced alias — SDL3::SDL3 is one — and now so do we.
add_library(engine::engine ALIAS engine)

# ---- The boundary ----------------------------------------------------------
#
# THIS IS THE MOST IMPORTANT PROPERTY IN THE PROJECT, so read it slowly.
#
#   PUBLIC  include/ : everything that links this library gets this directory on
#                      its include path. It contains ONE thing — a directory
#                      called `engine` — so from outside, the only spellings that
#                      resolve are <engine/gfx/raster.hpp> and its siblings.
#   PRIVATE src/     : the engine's own sources get this, and nobody else does.
#
# The consequence is the whole point of Lesson 5.1: a demo that writes
# `#include "gfx/raster.hpp"` — the spelling every file in this repository used
# until today — does not compile. Not "is discouraged", not "fails review". Does
# not compile, with a message naming the file it could not find.
#
# A boundary maintained by discipline is a boundary that lasts until the week
# somebody is in a hurry. This one is maintained by the include path, which is
# never in a hurry.
target_include_directories(engine
    PUBLIC  include
    PRIVATE src)

# PUBLIC because SDL is in our public headers: `framebuffer.hpp` says `Uint32`,
# `gpu_device.hpp` takes an `SDL_Window*`, and `input.hpp` reads `SDL_Event`.
# Anybody who includes those needs SDL's headers too, and PUBLIC is CMake's word
# for "this dependency is contagious, and it has to be".
#
# It is also an admission. A renderer whose public API names another library's
# types has adopted that library's vocabulary permanently — port to a different
# platform layer and every one of those signatures changes. §6 of the lesson
# weighs that honestly; we keep it, and we keep knowing about it.
target_link_libraries(engine PUBLIC SDL3::SDL3)

# stb_image, by contrast, is PRIVATE: exactly one translation unit
# (src/gfx/image.cpp) includes it, and `image.hpp` deliberately exposes our own
# `engine::image` instead. So the dependency stops at the boundary — a demo can
# neither see stb nor accidentally start depending on it. That is what a good
# wrapper buys, and this line is the proof it worked.
target_include_directories(engine PRIVATE ${stb_SOURCE_DIR})

# cgltf, PRIVATE for the identical reason and with the identical receipt: exactly
# one translation unit (src/gfx/gltf.cpp) includes <cgltf.h>, and `gltf.hpp`
# exposes our own `gltf_scene_data` instead. A demo can neither see cgltf nor
# start depending on it — which is what makes "swap the glTF library" a
# one-file change rather than a project-wide one.
target_include_directories(engine PRIVATE ${cgltf_SOURCE_DIR})

# Dear ImGui, PUBLIC — the opposite call from stb above, argued at length in
# engine/include/engine/ui/debug_ui.hpp. The short version: with stb we wrapped a
# CONCEPT (decode bytes into pixels), which is one function and one type. ImGui's
# value IS its vocabulary — four hundred widget calls — and a wrapper around that
# is a re-spelling with no content that has to be re-spelled again forever.
#
# So demos include <imgui.h> and call ImGui:: directly, and this line is what
# makes that legal. Note precisely what it costs: two more directories on the
# include path of everything that links the engine, and a second third-party
# vocabulary inside our public API. What makes it acceptable is that the only
# code allowed to speak it is TOOLING code.
target_link_libraries(engine PUBLIC imgui::imgui)

engine_set_warnings(engine)
