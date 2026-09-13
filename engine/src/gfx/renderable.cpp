// engine/src/gfx/renderable.cpp — the loop that was written three times.
//
// Everything the header forward-declared is included HERE, which is the whole
// point of the split: the ECS is compiled once, in this translation unit,
// instead of once per file that stores a `renderable`.

#include <engine/gfx/renderable.hpp>

#include <engine/core/log.hpp>
#include <engine/ecs/hierarchy.hpp>
#include <engine/ecs/registry.hpp>
#include <engine/math/transform.hpp>

namespace engine {

renderable_report collect_renderables(ecs::registry& world, const mesh_pool& meshes,
                                      std::vector<scene_object>& out)
{
    renderable_report report{};

    // `clear()` keeps the capacity; only the size goes to zero. This is the line
    // that makes a steady-state frame allocation-free, and it is worth one
    // sentence because the obvious alternative — returning a fresh vector — is
    // one character shorter to write and allocates every frame, for ever.
    out.clear();

    // ONE PASS, TWO POOLS, AND THE VIEW PICKS WHICH TO WALK. Lesson 5.8's view
    // leads with the SMALLEST pool and probes the others, so a world with four
    // hundred transforms and six renderables walks six candidates rather than
    // four hundred. Nothing here has to know that, which is what it means for a
    // component to be hoisted correctly: the work moved, the knowledge did not.
    //
    // NOTE THE MISSING `const` ON THE TEMPLATE ARGUMENTS. `view<const renderable>`
    // does not compile: the view stores `pool<Ts>*`, and `pool<const T>` is not a
    // thing this ECS can instantiate. That is the same defect the header's
    // `@param world` note describes, seen from the inside — the lambda takes its
    // parameters by `const&` because that is the only place constness can still
    // be spelled.
    world.view<ecs::world_transform, renderable>().each(
        [&](const ecs::world_transform& w, const renderable& r) {
            const mesh_data* geometry = meshes.get(r.mesh);
            if (geometry == nullptr)
            {
                ++report.missing_mesh;
                return;
            }

            out.push_back(scene_object{
                // The exact matrix -> transform conversion the header explains.
                // `linear_of` is the upper-left 3x3 and `translation_of` the
                // fourth column, so the recomposition `T * R * S` with S = 1
                // reproduces the original affine matrix to the bit.
                .xform = {.position = translation_of(w.matrix),
                          .rotation = linear_of(w.matrix),
                          .scale = {1.0f, 1.0f, 1.0f}},
                .geometry = r.mesh,
                // A CONSTANT, AND THE HEADER ARGUES FOR WHY. Naming entities is
                // a separate concern with three future customers and one present
                // one; inventing a `name` component here to fill this field
                // would be a guess with a struct attached.
                .name = "renderable",
                .mat = r.mat,
                .closed = r.closed,
            });
            ++report.drawn;
        });

    // ---- The entities that are NOT in the picture -------------------------
    //
    // The view above cannot report these, by construction: an entity missing
    // `world_transform` never enters it. So they are counted separately, and the
    // cost is a second walk of the renderable pool — which is worth it, because
    // the symptom this catches is "the orb is simply not there" and the cause is
    // four lines away in the caller's spawn function.
    //
    // `hierarchy::resolve` NEVER CREATES COMPONENTS (Lesson 5.9, deliberately),
    // so `add<renderable>` without `add_hierarchy_components` is silently
    // invisible. That decision is right — the shape of the world stays the
    // caller's — and this counter is the cheap half of paying for it.
    if (const ecs::pool<renderable>* pool = world.storage_if<renderable>())
    {
        for (const ecs::entity e : pool->entities())
        {
            if (!world.has<ecs::world_transform>(e)) { ++report.unresolved; }
        }
    }

    // AT WARNING, NOT AT ERROR, AND ONLY WHEN IT HAPPENS. Lesson 5.3's two axes:
    // the category says the graphics subsystem is speaking, the level says this
    // is survivable. A frame that draws everything says nothing at all, which is
    // the property that makes the message worth reading when it does appear.
    if (report.unresolved != 0 || report.missing_mesh != 0)
    {
        ENGINE_LOG_WARN(log_gfx,
                        "collect_renderables: %zu drawn, %zu unresolved "
                        "(no world_transform), %zu with a dead mesh handle",
                        report.drawn, report.unresolved, report.missing_mesh);
    }

    return report;
}

}   // namespace engine
