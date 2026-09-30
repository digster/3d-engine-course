// demos/mannequin/main.cpp — a character somebody else built, playing in this engine.
//
// Lesson 7.7b. Every skinned thing this repository drew before this program was
// built by this repository: 7.6's tube, laid out joint by joint, weighted by a
// formula, animated by clips baked from functions. That proves the arithmetic
// and nothing about content, because data you write yourself agrees with your
// code by construction. This program loads `assets/mannequin.glb`, which was
// written by Blender's glTF exporter (scratch/make_mannequin.py authored it and
// Blender made every decision the importer has to cope with), and plays it with
// the same four calls 7.6 and 7.7 use:
//
//     sample  ->  skinning_palette  ->  skin_into  ->  draw
//
// Nothing between the file and those calls is in this program. `load_gltf` reads
// the bytes and `anim::import_rig` turns them into a skeleton, two skinned meshes
// and two clips; after that the character is indistinguishable from one the
// engine built itself, which is the point.
//
// WHAT TO DO WITH IT, in the order that makes the point:
//
//   1. Watch it walk. Look at its FACE: the dark plate is on the front of its
//      head, and it is looking at you. glTF puts an asset's front at +Z and this
//      camera looks down -Z, so an unrotated character faces the camera — which
//      is the opposite of what Lesson 6.6 said, and this is the asset that
//      settled it. [2] plays the wave; [1] the walk; [Up]/[Down] cross-fades.
//   2. Press [O]. The same file, imported the way a loader that walks the node
//      array would build it: Blender writes children before parents, so every
//      joint's parent comes AFTER it, and `compose_pose` — which requires
//      parent-first — treats each one as a root. The body comes apart and every
//      limb lands at its own offset from the feet. That is what the sort in
//      `import_rig` is for, and why it is paid once at load.
//   3. Press [Q]. The rotations read as w, x, y, z — a blind copy of the file's
//      x, y, z, w. Every joint is wrong by 111 to 180 degrees, and not by one
//      fixed rotation, which is why no single "fix-up" hides it.
//   4. Press [N]. No inverse binds: 7.6's classic bug, on a real character.
//   5. Press [R] to play the clips after 7.7's `reduce`. The panel says how
//      many keys that removed; your eyes will not find the difference, which is
//      the half-degree budget working.
//
//     cmake --build build --target mannequin
//     ./build/demos/mannequin                                   play with it
//     ./build/demos/mannequin --clip wave --time 0.9           the wave, mid-swing
//     ./build/demos/mannequin --order-file --shot scratch/x.ppm      the unsorted bug
//     ./build/demos/mannequin --xyzw --shot scratch/x.ppm             the swizzle bug
//     ./build/demos/mannequin --time 0.25 --shot scratch/x.ppm        headless
//     ./build/demos/mannequin --xyzw --wide --shot scratch/x.ppm      …found, below the floor
//
// THE RULES ARE 5.1'S. Only public headers. The asset is found by the engine's own
// search path, beside the executable, where `engine_use_assets` copies it.

#include <engine/anim/clip.hpp>
#include <engine/anim/import.hpp>
#include <engine/anim/skeleton.hpp>
#include <engine/anim/skin.hpp>
#include <engine/asset/search_path.hpp>
#include <engine/core/actions.hpp>
#include <engine/ecs/camera.hpp>
#include <engine/ecs/hierarchy.hpp>
#include <engine/ecs/registry.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/debug_draw.hpp>
#include <engine/gfx/debug_lines.hpp>
#include <engine/gfx/depth_buffer.hpp>
#include <engine/gfx/gltf.hpp>
#include <engine/gfx/image.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/material.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/projector.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/gfx/renderable.hpp>
#include <engine/gfx/scene.hpp>
#include <engine/gfx/soft_renderer.hpp>
#include <engine/gfx/viewport.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/transform.hpp>
#include <engine/math/vec3.hpp>
#include <engine/platform/app.hpp>
#include <engine/platform/main.hpp>
#include <engine/ui/debug_ui.hpp>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>
#include <vector>

using engine::mat4;
using engine::quat;
using engine::transform;
using engine::vec3;
using engine::anim::imported_rig;
using engine::anim::joint_index;
using engine::anim::k_no_parent;
using engine::ecs::entity;

namespace {

constexpr int k_width = 960;
constexpr int k_height = 540;

constexpr float k_pi = std::numbers::pi_v<float>;
constexpr float k_rad = k_pi / 180.0f;

/// The same clear colour as `rig`, for the same reason: the lesson's figure
/// generator treats a pixel as background only when all three channels are
/// below 14, and a background it does not recognise comes out as a grey slab.
const Uint32 k_background = engine::pack_argb(11, 12, 13);

/// Skeleton lines: amber for the skin's joints, grey for the ancestor the
/// importer carried in (Blender's `Armature` node), so the one joint no vertex
/// is weighted to is visibly a different kind of thing.
const Uint32 k_bone = engine::pack_argb(242, 186, 92);
const Uint32 k_ancestor = engine::pack_argb(120, 126, 140);
const Uint32 k_floor = engine::pack_argb(44, 48, 56);

// ---- The two import bugs, reproduced on purpose ----------------------------------

/// Rebuild a rig with its joints in a different order: `order[new] = old`.
///
/// Every array that is parallel to the joints moves together — the joints and
/// their parents, the inverse binds, each vertex's joint indices, and each clip's
/// tracks — which is exactly the pass `import_rig` performs after its sort, run
/// here with a BAD order to show what the good one prevents. Parents are remapped
/// but not re-sorted, so a parent that lands after its child stays there.
[[nodiscard]] imported_rig reorder(const imported_rig& in, const std::vector<std::size_t>& order)
{
    const std::size_t n = order.size();
    std::vector<joint_index> new_of_old(n);
    for (std::size_t j = 0; j < n; ++j) { new_of_old[order[j]] = static_cast<joint_index>(j); }

    imported_rig out = in;
    for (std::size_t j = 0; j < n; ++j)
    {
        const std::size_t old = order[j];
        out.sk.joints[j] = in.sk.joints[old];
        const joint_index p = in.sk.joints[old].parent;
        out.sk.joints[j].parent = (p == k_no_parent) ? k_no_parent : new_of_old[p];
        out.sk.joint_from_model[j] = in.sk.joint_from_model[old];
        out.joint_node[j] = in.joint_node[old];
    }
    for (engine::anim::skinned_mesh& m : out.meshes)
    {
        for (engine::anim::skin_influence& inf : m.influences)
        {
            for (joint_index& j : inf.joints) { j = new_of_old[j]; }
        }
    }
    for (std::size_t c = 0; c < in.clips.size(); ++c)
    {
        for (std::size_t j = 0; j < n; ++j)
        {
            out.clips[c].tracks[j] = (order[j] < in.clips[c].tracks.size())
                                         ? in.clips[c].tracks[order[j]]
                                         : engine::anim::joint_track{};
        }
    }
    return out;
}

/// The order a loader that walks the NODE ARRAY would build: joints sorted by
/// their node index. Blender writes children first, so this is almost exactly
/// child-before-parent — the worst order there is.
[[nodiscard]] imported_rig in_node_order(const imported_rig& in)
{
    std::vector<std::size_t> order(in.sk.size());
    for (std::size_t j = 0; j < order.size(); ++j) { order[j] = j; }
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return in.joint_node[a] < in.joint_node[b];
    });
    return reorder(in, order);
}

/// Every rotation read as if the file's x, y, z, w were w, x, y, z — what a
/// `memcpy` of the file's four floats into a `quat` does. The inverse binds are
/// matrices and are untouched, which is part of why the result is so violent:
/// the rest pose no longer matches the bind, so the character is wrong before
/// it moves.
[[nodiscard]] quat blind(quat q)
{
    // q came from the file as (x, y, z, w) = (q.v.x, q.v.y, q.v.z, q.w); a blind
    // copy puts those four floats into (w, x, y, z) in that order.
    return quat{q.v.x, vec3{q.v.y, q.v.z, q.w}};
}

[[nodiscard]] imported_rig swizzled(const imported_rig& in)
{
    imported_rig out = in;
    for (engine::anim::joint& j : out.sk.joints) { j.local_bind.rotation = blind(j.local_bind.rotation); }
    for (engine::anim::clip& c : out.clips)
    {
        for (engine::anim::joint_track& t : c.tracks)
        {
            for (engine::anim::quat_key& k : t.rotation) { k.value = blind(k.value); }
        }
    }
    return out;
}

}   // namespace

// ---- The program -----------------------------------------------------------

class mannequin_app final : public engine::app
{
public:
    [[nodiscard]] engine::app_config configure(int argc, char* argv[]) override
    {
        for (int i = 1; i < argc; ++i)
        {
            if (SDL_strcmp(argv[i], "--shot") == 0 && i + 1 < argc) { shot_path_ = argv[++i]; }
            else if (SDL_strcmp(argv[i], "--clip") == 0 && i + 1 < argc)
            {
                clip_ = (SDL_strcmp(argv[++i], "wave") == 0) ? 1 : 0;
            }
            else if (SDL_strcmp(argv[i], "--time") == 0 && i + 1 < argc)
            {
                const double given = SDL_atof(argv[++i]);
                time_ = static_cast<float>(given);
            }
            else if (SDL_strcmp(argv[i], "--fade") == 0 && i + 1 < argc)
            {
                const double given = SDL_atof(argv[++i]);
                fade_ = std::clamp(static_cast<float>(given), 0.0f, 1.0f);
            }
            else if (SDL_strcmp(argv[i], "--yaw") == 0 && i + 1 < argc)
            {
                const double given = SDL_atof(argv[++i]);
                yaw_ = static_cast<float>(given) * k_rad;
            }
            else if (SDL_strcmp(argv[i], "--order-file") == 0) { variant_ = variant::file_order; }
            else if (SDL_strcmp(argv[i], "--xyzw") == 0) { variant_ = variant::blind_xyzw; }
            else if (SDL_strcmp(argv[i], "--no-bind") == 0) { use_inverse_binds_ = false; }
            else if (SDL_strcmp(argv[i], "--reduced") == 0) { reduced_ = true; }
            else if (SDL_strcmp(argv[i], "--no-skeleton") == 0) { show_skeleton_ = false; }
            else if (SDL_strcmp(argv[i], "--wide") == 0) { wide_ = true; }
            else if (SDL_strcmp(argv[i], "--asset") == 0 && i + 1 < argc) { asset_name_ = argv[++i]; }
        }

        return {.title = "mannequin — a character from a glTF file",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    [[nodiscard]] bool on_start() override
    {
        declare_actions();

        // ---- The load: a path, a parse, an import ------------------------------
        //
        // Three calls and three reports, each answering a different question: is
        // the file where it should be, is it glTF, and is it a character. The
        // engine does not throw (CLAUDE.md §4), so each failure is a branch here
        // with the report's own words in the log.
        const engine::resolved_path where = engine::search_path::standard().resolve(asset_name_);
        if (!where.ok())
        {
            SDL_Log("mannequin: '%s' not found beside the executable", asset_name_);
            return false;
        }
        file_report_ = engine::load_gltf(where.path.c_str(), file_);
        if (!file_report_.ok())
        {
            SDL_Log("mannequin: %s: %s", where.path.c_str(), engine::name_of(file_report_.status));
            return false;
        }
        import_ = engine::anim::import_rig(file_, 0, engine::anim::import_settings{}, rig_);
        if (!import_.ok())
        {
            SDL_Log("mannequin: import: %s", engine::anim::name_of(import_.status));
            return false;
        }

        // Which nodes are the skin's own joints, for the skeleton overlay's colours.
        is_skin_node_.assign(file_.nodes.size(), 0);
        for (int n : file_.skins[0].joints) { is_skin_node_[static_cast<std::size_t>(n)] = 1; }

        // The two bugs, built once so that toggling them costs nothing per frame.
        file_order_ = in_node_order(rig_);
        file_order_report_ = engine::anim::validate(file_order_.sk);
        swizzled_ = swizzled(rig_);

        // Lesson 7.7's reducer, applied to a COPY: the imported clips stay as the
        // file said, and [R] plays the reduced ones.
        reduced_rig_ = rig_;
        for (engine::anim::clip& c : reduced_rig_.clips)
        {
            keys_removed_ += engine::anim::reduce(c, reduced_rig_.sk, engine::anim::reduction_limits{});
        }
        for (const engine::anim::clip& c : rig_.clips)
        {
            clip_reports_.push_back(engine::anim::validate(c, rig_.sk));
        }

        // ---- One entity per skinned mesh ---------------------------------------
        //
        // Each mesh starts as its bind geometry in the engine's own pool, and
        // `skin_into` overwrites its positions and normals every frame — the
        // same arrangement `rig` uses. The material comes from the file's
        // description: base colour is linear in glTF and `material::tint` is
        // encoded, so it is re-encoded here, as `asset_store` does (6.5, 6.6).
        for (std::size_t m = 0; m < rig_.meshes.size(); ++m)
        {
            handles_.push_back(meshes_.insert(rig_.meshes[m].bind));
            const int mi = rig_.mesh_material[m];
            const engine::gltf_material_desc desc =
                (mi >= 0 && static_cast<std::size_t>(mi) < file_.materials.size())
                    ? file_.materials[static_cast<std::size_t>(mi)]
                    : engine::gltf_default_material();

            const entity e = world_.create();
            engine::ecs::add_hierarchy_components(world_, e, character_placement());
            world_.add<engine::renderable>(
                e, engine::renderable{.mesh = handles_.back(),
                                      .mat = {.tint = engine::to_encoded(desc.base_colour),
                                              .surface = desc.surface},
                                      .closed = true});
            bodies_.push_back(e);
        }

        lights_.key.direction = engine::normalised(vec3{-0.35f, -0.70f, -0.62f});
        lights_.key.colour = {1.0f, 0.97f, 0.92f};
        lights_.key.irradiance = engine::k_reference_irradiance;
        lights_.ambient = {0.16f, 0.17f, 0.22f};

        camera_ = world_.create();
        engine::ecs::add_hierarchy_components(world_, camera_, camera_placement());
        world_.add<engine::ecs::camera>(camera_, engine::ecs::camera{.fovy = 32.0f * k_rad});

        (void)tree_.rebuild_and_resolve(world_);
        (void)ui_.start(window(), renderer());
        return true;
    }

    void on_input() override
    {
        ui_.begin_frame();
        gate_.update(in(), ui_.wants_keyboard(), ui_.wants_mouse());
        actions_.update(gate_);

        if (actions_.pressed(a_quit_)) { request_quit(); }
        if (actions_.pressed(a_walk_)) { clip_ = 0; }
        if (actions_.pressed(a_wave_)) { clip_ = 1; }
        if (actions_.pressed(a_play_)) { playing_ = !playing_; }
        if (actions_.pressed(a_order_))
        {
            variant_ = (variant_ == variant::file_order) ? variant::as_imported : variant::file_order;
        }
        if (actions_.pressed(a_xyzw_))
        {
            variant_ = (variant_ == variant::blind_xyzw) ? variant::as_imported : variant::blind_xyzw;
        }
        if (actions_.pressed(a_bind_)) { use_inverse_binds_ = !use_inverse_binds_; }
        if (actions_.pressed(a_reduced_)) { reduced_ = !reduced_; }
        if (actions_.pressed(a_skeleton_)) { show_skeleton_ = !show_skeleton_; }
        if (actions_.pressed(a_turn_)) { yaw_ = (yaw_ == 0.0f) ? k_pi : 0.0f; }
    }

    void on_fixed_step(float h) override
    {
        // A headless run never gets here: every picture is reachable from a flag.
        if (shot_path_ != nullptr) { return; }
        if (playing_) { time_ += h; }
        time_ += actions_.value(a_scrub_) * h * 0.6f;
        // Wrapped every step, against the LONGER clip, for 7.7's reason: fmod's
        // cost grows with the quotient, and an unbounded clock pays it forever.
        time_ = engine::anim::wrap_time(active().clips[1], time_);
        fade_ = std::clamp(fade_ + actions_.value(a_fade_) * h * 1.2f, 0.0f, 1.0f);
    }

    void on_frame(float alpha) override
    {
        (void)alpha;   // a 60 Hz step on a clip sampled at 30: a frame is at most half a key stale

        fb().clear(k_background);
        depth_.clear();

        const imported_rig& use = active();

        // ---- The four calls that are the lesson -----------------------------
        //
        // Sample (7.7), blend if cross-fading (7.7), compose and build the
        // palette (7.6), deform (7.6). Nothing here knows the character came from
        // a file, and that is what an importer is FOR.
        const std::size_t a = static_cast<std::size_t>(clip_);
        engine::anim::sample(use.clips[a], use.sk, time_, cursors_a_, pose_a_);
        if (fade_ > 0.0f)
        {
            engine::anim::sample(use.clips[1 - a], use.sk, time_, cursors_b_, pose_b_);
            (void)engine::anim::blend_poses(pose_a_, pose_b_, fade_, pose_);
        }
        else
        {
            pose_ = pose_a_;
        }
        engine::anim::skinning_palette(use.sk, pose_, posed_, palette_);

        // [N] — the classic bug from 7.6, one substitution.
        const std::vector<mat4>& palette = use_inverse_binds_ ? palette_ : posed_;
        for (std::size_t m = 0; m < use.meshes.size() && m < handles_.size(); ++m)
        {
            if (engine::mesh_data* dst = meshes_.get(handles_[m]))
            {
                engine::anim::skin_into(use.meshes[m], palette, *dst);
            }
        }

        // The whole character turns by [T] — a yaw on the ENTITY, outside the
        // importer, which is where 6.6 said the facing fix belongs.
        for (entity e : bodies_)
        {
            if (engine::transform* placement = world_.get<engine::transform>(e))
            {
                *placement = character_placement();
            }
        }
        (void)tree_.rebuild_and_resolve(world_);

        const engine::ecs::world_transform* cam_placement =
            world_.get<engine::ecs::world_transform>(camera_);
        const engine::ecs::camera* cam = world_.get<engine::ecs::camera>(camera_);
        if (cam_placement == nullptr || cam == nullptr) { return; }

        const mat4 view = engine::ecs::view_from_camera(*cam_placement);
        const vec3 eye = engine::ecs::eye_of(*cam_placement);
        const engine::projector proj{
            engine::ecs::projection_of(*cam, static_cast<float>(k_width)
                                                 / static_cast<float>(k_height)),
            engine::viewport{0.0f, 0.0f, static_cast<float>(k_width),
                             static_cast<float>(k_height), 0.0f, 1.0f},
            engine::near_mode::clip};

        debug_.clear();
        queue_floor();

        objects_.clear();
        collect_ = engine::collect_renderables(world_, meshes_, objects_);
        const engine::render_options opts{.cull = engine::cull_choice::back,
                                          .normals = engine::normal_source::vertex,
                                          .shading = engine::shade_eval::gouraud};
        engine::collect_triangles(triangles_, scratch_, objects_, meshes_, {view, eye}, proj,
                                  lights_, opts);
        const engine::fill_style style{.shade = engine::shading::vertex_colour,
                                       .cull = engine::cull_mode::back,
                                       .eye = eye};
        engine::draw_triangles(fb(), &depth_, triangles_, false, style);

        if (show_skeleton_) { queue_skeleton(use); }
        debug_drawn_ = engine::draw_debug_lines(fb(), view, proj, debug_);
        debug_.advance(time().dt());

        if (shot_path_ != nullptr) { write_shot(); }
    }

    void on_overlay() override
    {
        // Guarded: a `--shot` run has no ImGui context (see `rig`).
        if (ui_.running()) { build_panel(); }
        ui_.render();
    }

    void on_event(const SDL_Event& event) override { (void)ui_.handle_event(event); }

    void on_stop() override { ui_.stop(); }

private:
    enum class variant { as_imported, file_order, blind_xyzw };

    [[nodiscard]] const imported_rig& active() const
    {
        switch (variant_)
        {
        case variant::file_order: return file_order_;
        case variant::blind_xyzw: return swizzled_;
        case variant::as_imported: break;
        }
        return reduced_ ? reduced_rig_ : rig_;
    }

    void declare_actions()
    {
        a_quit_ = actions_.declare("quit");
        a_walk_ = actions_.declare("walk");
        a_wave_ = actions_.declare("wave");
        a_play_ = actions_.declare("play");
        a_scrub_ = actions_.declare("scrub");
        a_fade_ = actions_.declare("fade");
        a_order_ = actions_.declare("order");
        a_xyzw_ = actions_.declare("xyzw");
        a_bind_ = actions_.declare("bind");
        a_reduced_ = actions_.declare("reduced");
        a_skeleton_ = actions_.declare("skeleton");
        a_turn_ = actions_.declare("turn");

        (void)actions_.bind_key(a_quit_, SDL_SCANCODE_ESCAPE);
        (void)actions_.bind_key(a_walk_, SDL_SCANCODE_1);
        (void)actions_.bind_key(a_wave_, SDL_SCANCODE_2);
        (void)actions_.bind_key(a_play_, SDL_SCANCODE_SPACE);
        (void)actions_.bind_key(a_scrub_, SDL_SCANCODE_RIGHT, +1.0f);
        (void)actions_.bind_key(a_scrub_, SDL_SCANCODE_LEFT, -1.0f);
        (void)actions_.bind_key(a_fade_, SDL_SCANCODE_UP, +1.0f);
        (void)actions_.bind_key(a_fade_, SDL_SCANCODE_DOWN, -1.0f);
        (void)actions_.bind_key(a_order_, SDL_SCANCODE_O);
        (void)actions_.bind_key(a_xyzw_, SDL_SCANCODE_Q);
        (void)actions_.bind_key(a_bind_, SDL_SCANCODE_N);
        (void)actions_.bind_key(a_reduced_, SDL_SCANCODE_R);
        (void)actions_.bind_key(a_skeleton_, SDL_SCANCODE_K);
        (void)actions_.bind_key(a_turn_, SDL_SCANCODE_T);
    }

    /// Where the character stands: at the origin, turned by `yaw_` about +y.
    /// Model space IS the file's scene space (the importer carried every node
    /// above the joints into the skeleton), so this is the only placement there
    /// is — nothing hidden in the file moves it.
    [[nodiscard]] transform character_placement() const
    {
        transform t;
        t.rotation = engine::quat_y(yaw_);
        return t;
    }

    [[nodiscard]] transform camera_placement() const
    {
        // In front of the character (glTF's front is +z) and a little to its
        // left, aimed at the chest, so a walk's swing reads in depth. `--wide`
        // backs off far enough to find a character a bug has thrown out of frame.
        if (wide_)
        {
            return engine::ecs::look_along({1.5f, 0.1f, 3.3f}, {0.0f, -0.62f, 0.0f},
                                           {0.0f, 1.0f, 0.0f});
        }
        return engine::ecs::look_along({1.25f, 1.35f, 3.4f}, {0.0f, 0.92f, 0.0f},
                                       {0.0f, 1.0f, 0.0f});
    }

    // ---- Drawing -----------------------------------------------------------

    void queue_floor()
    {
        // A grid on y = 0 — the character's feet are at model y = 0, so the floor
        // is where contact is judged by eye.
        for (int i = -4; i <= 4; ++i)
        {
            const float f = static_cast<float>(i) * 0.25f;
            debug_.line({f, 0.0f, -1.0f}, {f, 0.0f, 1.0f}, k_floor);
            debug_.line({-1.0f, 0.0f, f}, {1.0f, 0.0f, f}, k_floor);
        }
    }

    void queue_skeleton(const imported_rig& use)
    {
        const mat4 world = engine::parent_from_local(character_placement());
        for (std::size_t j = 0; j < use.sk.size() && j < posed_.size(); ++j)
        {
            const joint_index p = use.sk.joints[j].parent;
            if (p == k_no_parent || p >= posed_.size()) { continue; }
            const vec3 a = engine::xyz(world * engine::point(engine::translation_of(posed_[p])));
            const vec3 b = engine::xyz(world * engine::point(engine::translation_of(posed_[j])));
            // Coloured by what the PARENT is — a skin joint or a carried ancestor
            // — looked up by its glTF node, so the [O] view keeps the colours.
            const int node = use.joint_node[p];
            const bool ancestor = node >= 0 && static_cast<std::size_t>(node) < is_skin_node_.size()
                                  && !is_skin_node_[static_cast<std::size_t>(node)];
            debug_.line(a, b, ancestor ? k_ancestor : k_bone);
        }
    }

    void build_panel()
    {
        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(392.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("mannequin"))
        {
            ImGui::Text("%s", file_report_.generator.c_str());
            ImGui::Text("nodes %zu, joints %zu = %zu skin + %zu ancestor",
                        file_.nodes.size(), import_.joints, import_.skin_joints,
                        import_.ancestors);
            ImGui::Text("file-order breaks %zu, resorted %zu", import_.file_order_breaks,
                        import_.resorted);
            ImGui::Text("bind vs rest %.2e", static_cast<double>(import_.bind_vs_rest));
            ImGui::Text("vertices %zu in %zu meshes, 1-4 infl: %zu %zu %zu %zu",
                        import_.vertices, import_.meshes, import_.influences[1],
                        import_.influences[2], import_.influences[3], import_.influences[4]);
            ImGui::Separator();
            const engine::anim::clip& c = active().clips[static_cast<std::size_t>(clip_)];
            ImGui::Text("clip %s  t %6.3f / %.2f s  %s", c.name.c_str(),
                        static_cast<double>(time_), static_cast<double>(c.duration),
                        playing_ ? "[Space]" : "PAUSED");
            ImGui::Text("fade %.2f -> %s  ([Up]/[Down])", static_cast<double>(fade_),
                        rig_.clips[static_cast<std::size_t>(1 - clip_)].name.c_str());
            ImGui::Text("keys in file %zu, imported %zu, reduced -%zu",
                        import_.keys_in, import_.keys_out, keys_removed_);
            ImGui::Text("playing %s [R]", reduced_ ? "REDUCED clips" : "clips as imported");
            ImGui::Separator();
            ImGui::Text("joint order    %s [O]", variant_ == variant::file_order
                                                    ? "NODE ARRAY — the bug" : "sorted");
            if (variant_ == variant::file_order)
            {
                ImGui::Text("  out_of_order %zu of %zu", file_order_report_.out_of_order,
                            file_order_report_.joints);
            }
            ImGui::Text("rotations      %s [Q]", variant_ == variant::blind_xyzw
                                                    ? "read w-first — the bug" : "xyzw -> wxyz");
            ImGui::Text("inverse binds  %s [N]", use_inverse_binds_ ? "the file's" : "OFF — the bug");
            ImGui::Separator();
            ImGui::Text("%zu objects, %zu triangles, %d lines", collect_.drawn,
                        triangles_.size(), debug_drawn_);
            ImGui::TextUnformatted("[1] walk [2] wave [T] turn [K] skeleton");
        }
        ImGui::End();
    }

    void write_shot()
    {
        const engine::anim::clip& c = active().clips[static_cast<std::size_t>(clip_)];
        std::printf("mannequin: %s, %d nodes, %zu joints (%zu skin + %zu ancestor)\n",
                    file_report_.generator.c_str(), static_cast<int>(file_.nodes.size()),
                    import_.joints, import_.skin_joints, import_.ancestors);
        std::printf("mannequin: clip %s t %.3f s, fade %.2f, %s%s%s%s\n", c.name.c_str(),
                    static_cast<double>(time_), static_cast<double>(fade_),
                    variant_ == variant::file_order ? "NODE ORDER " : "",
                    variant_ == variant::blind_xyzw ? "BLIND XYZW " : "",
                    use_inverse_binds_ ? "" : "NO BINDS ",
                    reduced_ ? "clips reduced" : "clips as imported");
        if (variant_ == variant::file_order)
        {
            std::printf("mannequin: node order leaves %zu of %zu joints out of order\n",
                        file_order_report_.out_of_order, file_order_report_.joints);
        }
        std::printf("mannequin: %zu vertices, %zu triangles drawn, %d lines\n",
                    import_.vertices, triangles_.size(), debug_drawn_);
        request_quit(engine::save_ppm(fb(), shot_path_));
    }

    // ---- State -------------------------------------------------------------

    const char* shot_path_ = nullptr;
    const char* asset_name_ = "mannequin.glb";

    int clip_ = 0;              ///< 0 walk, 1 wave (the file's order)
    float time_ = 0.0f;
    float fade_ = 0.0f;
    float yaw_ = 0.0f;
    bool playing_ = true;
    bool use_inverse_binds_ = true;
    bool reduced_ = false;
    bool show_skeleton_ = true;
    bool wide_ = false;
    variant variant_ = variant::as_imported;

    engine::gltf_scene_data file_;
    engine::gltf_report file_report_{};
    engine::anim::rig_import_report import_{};
    imported_rig rig_;
    imported_rig reduced_rig_;
    imported_rig file_order_;
    imported_rig swizzled_;
    engine::anim::skeleton_report file_order_report_{};
    std::vector<engine::anim::clip_report> clip_reports_;
    std::vector<char> is_skin_node_;
    std::size_t keys_removed_ = 0;

    // The mutable half of playback, one set per playing instance (7.7).
    std::vector<engine::anim::track_cursor> cursors_a_;
    std::vector<engine::anim::track_cursor> cursors_b_;
    std::vector<transform> pose_a_;
    std::vector<transform> pose_b_;
    std::vector<transform> pose_;
    std::vector<mat4> posed_;
    std::vector<mat4> palette_;

    engine::ecs::registry world_;
    engine::ecs::hierarchy tree_;
    std::vector<entity> bodies_;
    entity camera_{};

    engine::mesh_pool meshes_;
    std::vector<engine::mesh_handle> handles_;

    engine::lighting lights_{};
    engine::depth_buffer depth_{k_width, k_height};
    std::vector<engine::scene_object> objects_;
    std::vector<engine::raster_triangle> triangles_;
    engine::projection_scratch scratch_;
    engine::renderable_report collect_{};
    engine::debug_lines debug_;
    int debug_drawn_ = 0;

    engine::masked_input<engine::input> gate_;
    engine::action_map actions_;
    engine::action_id a_quit_{}, a_walk_{}, a_wave_{}, a_play_{}, a_scrub_{}, a_fade_{};
    engine::action_id a_order_{}, a_xyzw_{}, a_bind_{}, a_reduced_{}, a_skeleton_{}, a_turn_{};

    engine::debug_ui ui_;
};

ENGINE_MAIN(mannequin_app)
