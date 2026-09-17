// engine/src/phys/inertia.cpp — equation (1), evaluated.

#include <engine/phys/inertia.hpp>

#include <algorithm>
#include <cmath>

namespace engine::phys
{
namespace
{

/// A tensor of all zeros, which is the "cannot rotate" value everywhere in this
/// file and in rigid_body.hpp. Named so that `return k_no_inertia;` reads as a
/// decision rather than as a typo.
constexpr mat3 k_no_inertia{vec3{}, vec3{}, vec3{}};

}   // namespace

// ---------------------------------------------------------------------------
// The atom
// ---------------------------------------------------------------------------

mat3 inertia_of_point(float mass, vec3 offset)
{
    // Equation (1) for one point. The derivation produces
    //
    //     m * (|r|^2 * identity - outer(r, r))
    //
    // and that expression is NOT what is written below, for a reason 8.3 §4
    // measured rather than guessed at.
    //
    // *** THE LITERAL FORM LOSES THE ANSWER ON A LONG THIN BODY. *** Its
    // diagonal is `|r|^2 - x^2`, which is a sum of three squares with one of
    // them immediately subtracted off again — catastrophic cancellation, in the
    // textbook sense. For a sample at `r = (1000, 0.001, 0)` the true `Ixx/m` is
    // `1e-6`, and in `float` the literal form computes `1000000 - 1000000` and
    // returns **exactly zero**: the 1e-6 was never representable in a sum whose
    // magnitude is 1e6. §4 measures the resulting relative error at 100%.
    //
    // That is not a contrived input. It is a plank, a rod, a rail, a sword — any
    // body far longer than it is thick — and the consequence is a zero principal
    // moment, an inertia tensor `inverse_inertia` then refuses, and a body that
    // silently will not spin about its own length.
    //
    // So the diagonal is formed from the two terms that BELONG in it, and the
    // off-diagonal from the products that belong in that. This is exactly
    // `-m * skew(r) * skew(r)` — the cross-product matrix squared — written out
    // rather than composed, which also makes it nine multiplies instead of two
    // matrix products. It is algebraically identical to the derived form and
    // numerically better everywhere.
    const float x = offset.x;
    const float y = offset.y;
    const float z = offset.z;

    const float ixx = mass * (y * y + z * z);
    const float iyy = mass * (x * x + z * z);
    const float izz = mass * (x * x + y * y);

    const float ixy = -mass * x * y;
    const float ixz = -mass * x * z;
    const float iyz = -mass * y * z;

    // Columns, as ever. The matrix is symmetric, so the written form and the
    // stored form agree element for element — which is the one place in this
    // engine where that is true and worth not relying on.
    return {{ixx, ixy, ixz},
            {ixy, iyy, iyz},
            {ixz, iyz, izz}};
}

mat3 inertia_of_points(std::span<const point_mass> points)
{
    mat3 total = k_no_inertia;
    for (const point_mass& p : points)
    {
        total += inertia_of_point(p.mass, p.offset);
    }
    return total;
}

// ---------------------------------------------------------------------------
// Closed forms
// ---------------------------------------------------------------------------

mat3 inertia_solid_box(float mass, vec3 half_extents)
{
    // Half-extents (a, b, c). Ixx = m*(b^2 + c^2)/3 — the extent along x does
    // not appear, because mass lying along the x axis is at zero distance FROM
    // that axis. Each entry is built from the OTHER two.
    const float a2 = half_extents.x * half_extents.x;
    const float b2 = half_extents.y * half_extents.y;
    const float c2 = half_extents.z * half_extents.z;

    const float k = mass / 3.0f;
    return diagonal(vec3{k * (b2 + c2), k * (a2 + c2), k * (a2 + b2)});
}

mat3 inertia_solid_sphere(float mass, float radius)
{
    const float i = 0.4f * mass * radius * radius;   // 2/5
    return diagonal(vec3{i, i, i});
}

mat3 inertia_hollow_sphere(float mass, float radius)
{
    const float i = (2.0f / 3.0f) * mass * radius * radius;
    return diagonal(vec3{i, i, i});
}

mat3 inertia_solid_cylinder(float mass, float radius, float height)
{
    const float r2 = radius * radius;
    const float across = mass * (3.0f * r2 + height * height) / 12.0f;
    const float along = 0.5f * mass * r2;
    return diagonal(vec3{across, along, across});   // axis is +y
}

mat3 inertia_thin_rod(float mass, float length)
{
    const float across = mass * length * length / 12.0f;
    return diagonal(vec3{across, 0.0f, across});    // exactly zero along the rod
}

mat3 inertia_capsule(float mass, float radius, float cylinder_height)
{
    // Split the mass between the cylinder and the two hemispheres by VOLUME,
    // which is what "uniform density" means: pi*R^2*H against (4/3)*pi*R^3, and
    // the pi*R^2 cancels out of the ratio to leave H against 4R/3.
    const float denom = cylinder_height + (4.0f / 3.0f) * radius;
    if (!(denom > 0.0f))
    {
        return k_no_inertia;   // a capsule of no size
    }

    const float m_cyl = mass * cylinder_height / denom;
    const float m_caps = mass - m_cyl;     // BOTH hemispheres
    const float m_hemi = 0.5f * m_caps;    // one of them

    const float r2 = radius * radius;
    const float h = cylinder_height;

    // Along the axis: the cylinder's own (1/2)mR^2, plus the two hemispheres,
    // which together are a whole sphere and so contribute (2/5)*m_caps*R^2. No
    // parallel-axis term — sliding a hemisphere ALONG the axis does not change
    // its distance from that axis.
    const float along = 0.5f * m_cyl * r2 + 0.4f * m_caps * r2;

    // Across it, the fiddly half. A hemisphere's own centre of mass sits 3R/8
    // from its flat face, so its tensor about that centre is the familiar
    // (2/5)*m*R^2 (taken about the flat face's centre) with the parallel-axis
    // term for 3R/8 REMOVED — that is `unshift_inertia` in scalar form — and it
    // is then shifted out to the capsule's centre, a distance H/2 + 3R/8 away.
    //
    //     (2/5 - 9/64) = 83/320
    //
    // The two checks in 8.3 §5 are what make this transcription trustworthy:
    // H -> 0 must reproduce a solid sphere, and R -> 0 must reproduce a rod.
    const float d = 0.5f * h + 0.375f * radius;
    const float across_cyl = m_cyl * (3.0f * r2 + h * h) / 12.0f;
    const float across_hemi = m_hemi * ((83.0f / 320.0f) * r2 + d * d);

    const float across = across_cyl + 2.0f * across_hemi;
    return diagonal(vec3{across, along, across});
}

// ---------------------------------------------------------------------------
// Moving a tensor
// ---------------------------------------------------------------------------

mat3 shift_inertia(const mat3& about_centre, float mass, vec3 offset)
{
    return about_centre + inertia_of_point(mass, offset);
}

mat3 unshift_inertia(const mat3& about_point, float mass, vec3 offset)
{
    return about_point - inertia_of_point(mass, offset);
}

// ---------------------------------------------------------------------------
// Changing the basis
// ---------------------------------------------------------------------------

mat3 rotate_inertia(const mat3& rotation, const mat3& inertia)
{
    return rotation * inertia * transpose(rotation);
}

mat3 world_inertia(quat orientation, const mat3& body_inertia)
{
    const mat3 r = mat3_from_quat(orientation);
    return r * body_inertia * transpose(r);
}

mat3 world_inverse_inertia(quat orientation, const mat3& inv_body_inertia)
{
    const mat3 r = mat3_from_quat(orientation);
    return r * inv_body_inertia * transpose(r);
}

// ---------------------------------------------------------------------------
// Inverting
// ---------------------------------------------------------------------------

mat3 inverse_inertia(const mat3& inertia)
{
    // `mat3::inverse` already returns zeros on an EXACT zero determinant, which
    // covers the all-zeros tensor of an immovable body. It does not cover the
    // case that actually bites: a tensor that is merely nearly singular — a very
    // thin rod, a very flat plate — where the determinant is a denormal-adjacent
    // number rather than zero and the inverse comes back as 1e30 with no
    // complaint. One step later the body is spinning at a speed with no name.
    //
    // So the test is RELATIVE. The determinant has units of inertia cubed, so
    // the thing to compare it against is the largest diagonal entry cubed.
    const float scale = std::max({std::fabs(inertia.c0.x),
                                  std::fabs(inertia.c1.y),
                                  std::fabs(inertia.c2.z)});
    if (!(scale > 0.0f))
    {
        return k_no_inertia;
    }

    const float det = determinant(inertia);
    if (std::fabs(det) <= 1e-9f * scale * scale * scale)
    {
        return k_no_inertia;
    }

    return inverse(inertia);
}

// ---------------------------------------------------------------------------
// Compound bodies
// ---------------------------------------------------------------------------

inertia_assembly_result inertia_assembly(std::span<const inertia_part> parts)
{
    inertia_assembly_result out;

    // Step 1: total mass and the balance point. The centre of mass is the
    // mass-weighted mean of the parts' centres — which is the definition, and
    // also the reason the parallel-axis theorem's cross term vanishes (8.3 §5).
    for (const inertia_part& p : parts)
    {
        out.mass += p.mass;
        out.centre += p.centre * p.mass;
    }

    if (!(out.mass > 0.0f))
    {
        out.centre = vec3{};
        out.inertia = k_no_inertia;
        return out;
    }
    out.centre *= 1.0f / out.mass;

    // Step 2 and 3: shift each part from its own centre to the assembly's, and
    // add. Order matters not at all — tensors add like masses do — but the
    // SHIFT does: adding the parts' tensors without shifting them gives the
    // tensor of a body whose parts are all piled on top of each other, which is
    // always too small and never reports an error.
    out.inertia = k_no_inertia;
    for (const inertia_part& p : parts)
    {
        out.inertia += shift_inertia(p.inertia, p.mass, p.centre - out.centre);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

inertia_report inspect_inertia(const mat3& inertia)
{
    inertia_report out;

    out.asymmetry = asymmetry(inertia);
    out.trace = trace(inertia);
    out.diagonal = vec3{inertia.c0.x, inertia.c1.y, inertia.c2.z};

    const float biggest = std::max({std::fabs(out.diagonal.x),
                                    std::fabs(out.diagonal.y),
                                    std::fabs(out.diagonal.z)});

    const float off = std::max({std::fabs(inertia.c1.x), std::fabs(inertia.c2.x),
                                std::fabs(inertia.c0.y), std::fabs(inertia.c2.y),
                                std::fabs(inertia.c0.z), std::fabs(inertia.c1.z)});
    out.off_diagonal = (biggest > 0.0f) ? (off / biggest) : 0.0f;

    out.positive = out.diagonal.x > 0.0f && out.diagonal.y > 0.0f && out.diagonal.z > 0.0f;

    // The triangle inequality, on the diagonal as it stands. Sorted ascending,
    // no real mass distribution can have I1 + I2 < I3 — and EQUALITY is
    // achievable and must pass: a flat plate in the xz plane has Iy = Ix + Iz
    // exactly (the perpendicular-axis theorem), so a tolerance is required here
    // rather than optional.
    float m[3] = {out.diagonal.x, out.diagonal.y, out.diagonal.z};
    std::sort(m, m + 3);
    out.triangle = (m[0] + m[1]) >= m[2] * (1.0f - 1e-4f);

    out.usable = out.positive && out.triangle
              && (biggest > 0.0f && out.asymmetry <= 1e-4f * biggest);
    return out;
}

} // namespace engine::phys
