// engine/src/gfx/antialias.cpp — prefiltering, twice.
//
// Lesson 6.14. The argument is in antialias.hpp. What is here is two filters that
// look unrelated and are the same operation: remove the frequencies the sample
// grid cannot carry, before they get a chance to masquerade as lower ones.

#include <engine/gfx/antialias.hpp>

#include <engine/gfx/framebuffer.hpp>

#include <algorithm>
#include <cmath>

namespace engine {

// ---- Geometric ---------------------------------------------------------------

void resolve_supersampled(const framebuffer& hi, framebuffer& lo, int factor,
                          bool encoded_average)
{
    if (factor < 1) { factor = 1; }

    // Dimensions must agree exactly. A mismatch is a no-op rather than a partial
    // write — the same rule `resolve` follows, and for the same reason: half a
    // resolved frame reads as a rendering bug in whichever half you notice.
    if (hi.width() != lo.width() * factor || hi.height() != lo.height() * factor)
    {
        return;
    }

    const float inv = 1.0f / static_cast<float>(factor * factor);

    for (int y = 0; y < lo.height(); ++y)
    {
        Uint32* out = lo.row(y);
        for (int x = 0; x < lo.width(); ++x)
        {
            if (encoded_average)
            {
                // THE BUG, SELECTABLE. Averaging stored bytes treats sRGB codes as
                // if they were quantities of light. They are not — the transfer
                // function is there precisely because they are not — so this
                // delivers 42.9% of the light at a half-covered edge and every
                // silhouette acquires a dark fringe.
                unsigned r = 0;
                unsigned g = 0;
                unsigned b = 0;
                for (int j = 0; j < factor; ++j)
                {
                    const Uint32* in = hi.row(y * factor + j);
                    for (int i = 0; i < factor; ++i)
                    {
                        const Uint32 p = in[x * factor + i];
                        r += (p >> 16) & 0xFFu;
                        g += (p >> 8) & 0xFFu;
                        b += p & 0xFFu;
                    }
                }
                const unsigned n = static_cast<unsigned>(factor * factor);
                out[x] = pack_argb(static_cast<Uint8>(r / n),
                                   static_cast<Uint8>(g / n),
                                   static_cast<Uint8>(b / n));
                continue;
            }

            // THE CORRECT PATH: decode each sample to light, average the light,
            // re-encode once. Lesson 6.1's rule, arriving for the eighth time in
            // this module — and note that the encode happens EXACTLY ONCE, which
            // is the half of that rule people forget.
            linear_rgb acc{};
            for (int j = 0; j < factor; ++j)
            {
                const Uint32* in = hi.row(y * factor + j);
                for (int i = 0; i < factor; ++i)
                {
                    const linear_rgb s = to_linear(in[x * factor + i]);
                    acc.r += s.r;
                    acc.g += s.g;
                    acc.b += s.b;
                }
            }
            out[x] = to_encoded({acc.r * inv, acc.g * inv, acc.b * inv});
        }
    }
}

// ---- Shading -----------------------------------------------------------------

float filtered_alpha2(float alpha2, vec3 dndx, vec3 dndy)
{
    // The normal's variance across the pixel, from its screen-space derivatives.
    // `dot(d, d)` rather than `length(d)` because the quantity that adds is the
    // VARIANCE, not the standard deviation — which is the entire content of the
    // derivation and the easiest thing to get wrong by reaching for a length.
    const float variance = k_specular_aa_sigma2 * (dot(dndx, dndx) + dot(dndy, dndy));

    // The clamp, before the add. Without it a silhouette — where the normal
    // swings through most of a hemisphere inside one pixel — filters to fully
    // rough, and a chrome bumper goes matte exactly at its own edge.
    const float kernel = std::min(2.0f * variance, k_specular_aa_kappa);

    // `alpha2` is alpha SQUARED, and the filtered result is clamped to 1 because
    // alpha above 1 is not a rougher surface, it is a broken NDF.
    return std::clamp(alpha2 + kernel, 0.0f, 1.0f);
}

float filtered_roughness(float roughness, vec3 dndx, vec3 dndy)
{
    const float alpha = alpha_from_roughness(roughness);
    const float filtered = std::sqrt(filtered_alpha2(alpha * alpha, dndx, dndy));
    return roughness_from_alpha(filtered);
}

float lobe_to_variation_ratio(float roughness, float normal_turn_per_pixel)
{
    if (!(normal_turn_per_pixel > 0.0f)) { return 0.0f; }
    return ggx_lobe_half_angle(alpha_from_roughness(roughness)) / normal_turn_per_pixel;
}

} // namespace engine
