// engine/src/phys/cast.cpp — conservative advancement: Newton's method on a
// convex distance.
//
// Lesson 8.13. Read `cast.hpp` first; it carries the argument. This file is one
// loop, and every line of it is one of four questions: how far apart are the
// shapes now (GJK), how fast is the motion closing that gap (a dot product),
// how far can the mover go before the gap is `skin` (a division), and have we
// arrived (a comparison).

#include <engine/phys/cast.hpp>

namespace engine::phys
{

const char* name_of(cast_status status)
{
    switch (status)
    {
    case cast_status::clear:           return "clear";
    case cast_status::hit:             return "hit";
    case cast_status::started_inside:  return "started inside";
    case cast_status::iteration_limit: return "iteration limit";
    }
    return "?";
}

cast_result cast(const convex& mover, vec3 displacement, const convex& obstacle, const cast_config& cfg)
{
    cast_result out;

    const float len = length(displacement);

    // The mover, placed at the current iterate. `convex::support` answers
    // RELATIVE to `origin` (8.5's split), so moving the whole shape is moving
    // one vector — the adapter's pointer still views the untouched shape.
    convex at = mover;
    gjk_config gcfg = cfg.gjk;

    float t = 0.0f;
    for (int i = 0; i < cfg.max_iterations; ++i)
    {
        at.origin = mover.origin + displacement * t;
        const gjk_result g = gjk_distance(at, obstacle, gcfg);
        out.gjk_iterations += g.iterations;
        out.iterations = i + 1;

        if (g.status != gjk_status::separated)
        {
            // Overlapping (or within GJK's own contact margin, which is the
            // same thing to a cast). At the start that is the caller's problem
            // and EPA's question. Later it means the step landed ON the contact
            // — which, with no skin, is exactly where a flat face sends it —
            // and GJK cannot tell touching from overlapping there, so report the
            // last iterate it could still measure. `cast_config::skin` is the
            // measured case for never getting here.
            if (i == 0)
            {
                out.status = cast_status::started_inside;
                out.t = 0.0f;
                return out;
            }
            out.status = cast_status::hit;
            return out;   // `t`, `distance`, normal and point are the previous iterate's
        }

        // `direction` points from the mover toward the obstacle; the closing
        // rate of the gap is the motion's component along it. Per unit of `t`,
        // so it carries the displacement's length.
        const float approach = dot(displacement, g.direction);

        // THE GAP IS THE LOWER BOUND, NOT GJK'S `distance`. `distance` is the
        // length between two witness points — an UPPER bound, short of proof by
        // up to GJK's tolerance — and a step sized from an upper bound can end
        // inside the skin by exactly that slack (8.13 §3: 377 of 6,049 hits, by
        // up to 0.17 mm). The
        // lower bound is the width of the slab between the two supporting planes
        // normal to `direction`, and under a translation that slab narrows at
        // exactly `approach` per unit `t`: stepping to where IT reaches `skin`
        // cannot cross the skin, whatever `direction` is. That is the separating
        // plane of 8.4, turned into a time.
        const float gap = certify(at, obstacle, g).lower;

        out.t = t;
        out.distance = gap;   // the certified lower bound
        out.surface_normal = -g.direction;
        out.point = g.point_b;

        // Not closing, or closing so slowly that the whole of what is left of
        // the motion cannot bring the gap below `skin` by more than
        // `min_approach · len`. The slab normal to `direction` narrows at
        // exactly `approach` per unit `t` and not a hair faster, so this is a
        // proof of "clear" rather than a heuristic.
        if (approach <= cfg.min_approach * len)
        {
            out.status = cast_status::clear;
            out.t = 1.0f;
            return out;
        }

        // Arrived: within tolerance of the skin, and still closing.
        const float excess = gap - cfg.skin;
        if (excess <= cfg.tolerance * len)
        {
            out.status = cast_status::hit;
            return out;
        }

        // The Newton step toward `f = skin`: to where the slab is `skin` wide.
        // It cannot overshoot — see the header — so the only way out of the
        // motion is past its end.
        t += excess / approach;
        if (t >= 1.0f)
        {
            out.status = cast_status::clear;
            out.t = 1.0f;
            return out;
        }

        // Warm start the next GJK from this one's answer: the closest features
        // barely move between Newton steps, which is the case 8.5 §G measured.
        gcfg.initial_direction = g.direction;
    }

    // Out of steps. The iterate we stopped at was never evaluated, but it is
    // still safe — it is where the last separating slab narrowed to `skin` — so
    // report it; `distance` and the normal describe the iterate before it.
    out.status = cast_status::iteration_limit;
    out.t = t;
    return out;
}

} // namespace engine::phys
