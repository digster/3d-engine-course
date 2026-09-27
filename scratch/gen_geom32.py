"""Figure geometry for Lesson 3.2, computed rather than eyeballed.

A receding quad on the ground, checkered, drawn two ways:
  - perspective-correct: the uv lattice projected through the real camera
  - affine: the uv lattice mapped through each triangle's LINEAR screen map

Both are exact; the difference between them is the lesson.
"""

# --- the figure's own camera ------------------------------------------------
F = 1.0          # focal length
EYE_H = 1.0      # eye height above the ground plane
HALF_W = 1.5     # the quad's half width
Z_NEAR = -2.0    # near edge, in view space
Z_FAR = -12.0    # far edge

U_CELLS = 4      # checker cells across
V_CELLS = 6      # checker cells deep


def project(x, y, z, cx, cy, scale):
    """View space -> SVG pixels. y is up in view space, down in SVG."""
    w = -z
    return (cx + (F * x / w) * scale, cy - (F * y / w) * scale)


def world_at(u, v):
    """The point on the ground plane with texture coordinate (u, v)."""
    x = -HALF_W + (u / U_CELLS) * (2 * HALF_W)
    z = Z_NEAR + (v / V_CELLS) * (Z_FAR - Z_NEAR)
    return (x, -EYE_H, z)


def corners(cx, cy, scale):
    """The quad's four screen corners, in mesh order: nl, nr, fr, fl."""
    return [project(*world_at(0, 0), cx, cy, scale),
            project(*world_at(U_CELLS, 0), cx, cy, scale),
            project(*world_at(U_CELLS, V_CELLS), cx, cy, scale),
            project(*world_at(0, V_CELLS), cx, cy, scale)]


UV = [(0.0, 0.0), (float(U_CELLS), 0.0), (float(U_CELLS), float(V_CELLS)), (0.0, float(V_CELLS))]
TRIS = [(0, 1, 2), (0, 2, 3)]      # the same split main.cpp's quad_mesh uses


def affine_point(tri, P, u, v):
    """Where AFFINE interpolation puts texture coordinate (u,v) on `tri`.

    Affine interpolation is a linear map from screen barycentrics to uv, so
    inverting it is a 2x2 solve — and the answer is exact, not sampled.
    """
    i0, i1, i2 = tri
    u0, v0 = UV[i0]
    a11, a12 = UV[i1][0] - u0, UV[i2][0] - u0
    a21, a22 = UV[i1][1] - v0, UV[i2][1] - v0
    rhs1, rhs2 = u - u0, v - v0
    det = a11 * a22 - a12 * a21
    b1 = (rhs1 * a22 - a12 * rhs2) / det
    b2 = (a11 * rhs2 - rhs1 * a21) / det
    p0, p1, p2 = P[i0], P[i1], P[i2]
    return (p0[0] + b1 * (p1[0] - p0[0]) + b2 * (p2[0] - p0[0]),
            p0[1] + b1 * (p1[1] - p0[1]) + b2 * (p2[1] - p0[1]))


def correct_point(u, v, cx, cy, scale):
    """Where the texture coordinate (u,v) ACTUALLY lands: project the surface."""
    return project(*world_at(u, v), cx, cy, scale)


def fmt(pts):
    return " ".join("%.2f,%.2f" % p for p in pts)


def cells_correct(cx, cy, scale):
    """Filled dark cells of the checker, projected properly."""
    out = []
    for j in range(V_CELLS):
        for i in range(U_CELLS):
            if (i + j) % 2:
                continue
            quad = [correct_point(i, j, cx, cy, scale),
                    correct_point(i + 1, j, cx, cy, scale),
                    correct_point(i + 1, j + 1, cx, cy, scale),
                    correct_point(i, j + 1, cx, cy, scale)]
            out.append(fmt(quad))
    return out


def cells_affine(cx, cy, scale):
    """The same cells as AFFINE interpolation places them, per triangle.

    Returned per triangle, because each triangle's lattice is a different
    parallelogram grid — they agree only along the diagonal they share. The SVG
    clips each set to its own triangle.
    """
    P = corners(cx, cy, scale)
    per_tri = []
    for tri in TRIS:
        cells = []
        for j in range(V_CELLS):
            for i in range(U_CELLS):
                if (i + j) % 2:
                    continue
                quad = [affine_point(tri, P, i, j),
                        affine_point(tri, P, i + 1, j),
                        affine_point(tri, P, i + 1, j + 1),
                        affine_point(tri, P, i, j + 1)]
                cells.append(fmt(quad))
        per_tri.append((fmt([P[tri[0]], P[tri[1]], P[tri[2]]]), cells))
    return per_tri


def v_contours(cx, cy, scale):
    """Where the v = 1..5 lines land, both ways, as (correct, affine) segments.

    Only the correct ones curve-in toward the horizon; the affine ones are evenly
    spaced, which is the whole error in one picture.
    """
    P = corners(cx, cy, scale)
    out = []
    for v in range(1, V_CELLS):
        c = (correct_point(0, v, cx, cy, scale), correct_point(U_CELLS, v, cx, cy, scale))
        # the affine v-line, taken on the triangle that owns the left edge
        a = (affine_point(TRIS[1], P, 0, v), affine_point(TRIS[0], P, U_CELLS, v))
        out.append((c, a, v))
    return out


if __name__ == "__main__":
    cx, cy, scale = 160.0, 40.0, 200.0
    print("corners:", [tuple(round(c, 2) for c in p) for p in corners(cx, cy, scale)])
    print("\ncorrect v-lines vs affine v-lines (svg y):")
    for c, a, v in v_contours(cx, cy, scale):
        print("  v=%d  correct y=%7.2f   affine y=%7.2f   off by %6.2f px"
              % (v, c[0][1], a[0][1], a[0][1] - c[0][1]))
