# -*- coding: utf-8 -*-
import io, math
out = io.StringIO(); w = out.write
AMB, TEA, VIO = "#E0A83C", "#3CB8A8", "#A070D8"

# --- Figure 5 curve data: near=1, far=100, f/aspect=1 -----------------------
A = 100.0 / (1.0 - 100.0)          # -1.0101010101
B = 100.0 * 1.0 / (1.0 - 100.0)    # -1.0101010101
def w_of(t):  return 1.0 / (1.0 - 0.99 * t)      # view depth |z_v| at screen param t
def zn_of(t): return -A + B / w_of(t)            # device depth  (== t, exactly)

PW, PH = 240.0, 176.0                # panel plot area
def px(t): return t * PW
def py(v): return PH - v * PH        # v in [0,1]

true_view = " ".join("%.2f,%.2f" % (px(i / 120.0), py((w_of(i / 120.0) - 1.0) / 99.0))
                     for i in range(121))

# --- Figure 6: where evenly-spaced depth codes land in distance -------------
codes = [i / 10.0 for i in range(11)]
dists = [(-B / (c - (-A))) if c < 1.0 else 100.0 for c in codes]
# w = B/(z + A) with A,B negative:  w = B/(z+A)
dists = [B / (c + A) for c in codes[:-1]] + [100.0]

out_lines = []

w('''
  <h3 id="derivation">3.4 Deriving it: device depth is affine in screen space</h3>

  <p>
    A triangle is flat. In view space its three vertices lie in a plane, and every point of its
    interior lies in that plane too. Write the plane in the usual way, with a normal
    <strong>n</strong> and a constant <em>d</em>:
  </p>

  <div class="eq">
    <span class="katex-src">\\[ n_x x_v + n_y y_v + n_z z_v = d \\]</span>
    <span class="eq-plain">n.x * x_view + n.y * y_view + n.z * z_view = d</span>
  </div>

  <p>
    Now bring in the projection from Lesson 2.10. Writing <em>w</em> for
    <code>-z_view</code> &mdash; the positive distance in front of the camera, which is exactly what
    the projection matrix copies into the fourth component &mdash; the two screen coordinates and the
    device depth are
  </p>

  <div class="eq">
    <span class="katex-src">\\[ x_n = \\frac{f}{a}\\cdot\\frac{x_v}{w}, \\qquad y_n = f\\cdot\\frac{y_v}{w}, \\qquad z_n = \\frac{A z_v + B}{w} \\]</span>
    <span class="eq-plain">x_ndc = (f/aspect) * x_view / w,   y_ndc = f * y_view / w,   z_ndc = (A*z_view + B) / w</span>
  </div>

  <p>
    with <em>f</em> = cot(fovy/2) and the depth-row constants
    <em>A</em>&nbsp;=&nbsp;far/(near&nbsp;&minus;&nbsp;far),
    <em>B</em>&nbsp;=&nbsp;far&middot;near/(near&nbsp;&minus;&nbsp;far), both derived in Lesson 2.10
    &sect;3.5. Everything below is bookkeeping on those three lines.
  </p>

  <p>
    <strong>Step one: rearrange the projection.</strong> Solve the first two for the view-space
    coordinates, and note that <code>z_v = -w</code> by definition:
  </p>

  <div class="eq">
    <span class="katex-src">\\[ x_v = \\frac{a}{f}\\,x_n\\,w, \\qquad y_v = \\frac{1}{f}\\,y_n\\,w, \\qquad z_v = -w \\]</span>
    <span class="eq-plain">x_view = (aspect/f) * x_ndc * w,   y_view = (1/f) * y_ndc * w,   z_view = -w</span>
  </div>

  <p>
    <strong>Step two: substitute into the plane equation.</strong> Every term now carries a factor of
    <em>w</em>, so <em>w</em> comes straight out:
  </p>

  <div class="eq">
    <span class="katex-src">\\[ w\\left[\\frac{a\\,n_x}{f}x_n + \\frac{n_y}{f}y_n - n_z\\right] = d \\qquad\\Longrightarrow\\qquad \\frac{1}{w} = \\frac{1}{d}\\left[\\frac{a\\,n_x}{f}x_n + \\frac{n_y}{f}y_n - n_z\\right] \\]</span>
    <span class="eq-plain">w * [ (aspect*n.x/f)*x_ndc + (n.y/f)*y_ndc - n.z ] = d,  so  1/w = (1/d) * [ same bracket ]</span>
  </div>

  <p>
    Look hard at the right-hand side. It is a constant, plus a constant times
    <code>x_ndc</code>, plus a constant times <code>y_ndc</code>. That is the definition of an
    <strong>affine function of the screen position</strong>. So:
  </p>

  <div class="callout ok">
    <span class="label">The key fact</span>
    <p>
      Over any flat triangle, <strong>1/w varies affinely across the screen</strong>. Not
      approximately &mdash; exactly. This one sentence is the foundation of both this lesson and the
      next: Lesson 3.2 uses it to build perspective-correct interpolation for every attribute a
      vertex carries.
    </p>
  </div>

  <p>
    <strong>Step three: express device depth in terms of 1/w.</strong> Substitute
    <code>z_v = -w</code> into the depth row:
  </p>

  <div class="eq">
    <span class="katex-src">\\[ z_n = \\frac{A(-w) + B}{w} = -A + B\\cdot\\frac{1}{w} \\]</span>
    <span class="eq-plain">z_ndc = (A*(-w) + B)/w = -A + B*(1/w)</span>
  </div>

  <p>
    Device depth is an affine function of <code>1/w</code>, and <code>1/w</code> is an affine function
    of the screen position. An affine function of an affine function is affine. Therefore:
  </p>

  <div class="callout ok">
    <span class="label">The result</span>
    <p>
      <strong>Device depth varies affinely across the screen.</strong> And barycentric interpolation
      computes precisely the affine function that agrees with three given values at three given points
      (Lesson 2.3 &sect;3.7). So interpolating <code>z_ndc</code> from the three corners with the
      ordinary barycentric weights is not an approximation that happens to be good enough &mdash; it
      is the <em>exact</em> answer.
    </p>
  </div>

  <p>
    Two footnotes on the derivation, both worth having.
  </p>

  <p>
    <strong>The viewport transform does not break it.</strong> What we actually store is the third
    output of <code>viewport::to_screen</code>, which is
    <code>min_depth + z_ndc * (max_depth - min_depth)</code> &mdash; an affine function of
    <code>z_ndc</code>, hence still affine in the screen position. Interpolating the stored value is
    equally exact.
  </p>

  <p>
    <strong>The one degenerate case is not a case.</strong> The derivation divides by <em>d</em>, which
    is zero exactly when the triangle&rsquo;s plane passes through the eye. Such a triangle is edge-on:
    it projects to a line segment, has zero screen area, and is rejected by
    <code>fill_triangle</code>&rsquo;s <code>area == 0</code> test before any of this runs. There is no
    interior to interpolate over, so there is nothing to be wrong about.
  </p>

  <h4>And now the control: why view-space z fails</h4>

  <p>
    Run the same argument on <code>z_v</code>. We have <code>z_v = -w</code>, and from step two
    <code>1/w</code> is affine, so
  </p>

  <div class="eq">
    <span class="katex-src">\\[ z_v \;=\; -w \;=\; \\frac{-1}{\\text{(affine in }x_n, y_n)} \\]</span>
    <span class="eq-plain">z_view = -w = -1 / (an affine function of the screen position)</span>
  </div>

  <p>
    A reciprocal of an affine function is a <em>hyperbola</em>, not a line. Interpolating
    <code>z_v</code> linearly across the screen draws a chord where the truth is a curve, and near the
    far end of a deep triangle that chord is nowhere near the curve.
  </p>

  <p>
    There is one exception, and it is the reason this bug is so good at hiding. If the triangle&rsquo;s
    plane is <em>parallel to the screen</em> &mdash; <code>n_x = n_y = 0</code> &mdash; then the
    bracket in step two is constant, <code>1/w</code> is constant, and <code>z_v</code> is constant
    too. A flat wall facing the camera interpolates perfectly whichever depth you use. So a renderer
    with this bug looks completely correct on a test scene of boxes and floors, and falls apart on the
    first steeply-angled surface. If you ever find yourself writing &ldquo;it works on my test
    scene&rdquo;, this is the family of bug that sentence belongs to.
  </p>

  <div class="worked">
    <span class="label">Measured, not asserted</span>
    <p>
      <code>scratch/verify_31.cpp</code> generates 200,000 random triangles in the demo&rsquo;s
      frustum, picks a random interior point <em>in screen space</em>, computes the true surface point
      by intersecting the eye ray with the triangle&rsquo;s plane, and compares it against both
      interpolations. Over 199,273 well-conditioned samples:
    </p>
    <ul>
      <li>worst error interpolating <strong>device depth</strong>:
        <code>8.2 &times; 10<sup>&minus;13</sup></code> &mdash; double-precision noise, i.e. zero;</li>
      <li>worst error interpolating <strong>view-space z</strong>: <code>5.73 units</code>, which is
        <strong>243.6%</strong> of the true depth at that pixel.</li>
    </ul>
  </div>

  <h3 id="worked">3.5 A worked example, by hand</h3>

  <p>
    Abstractions deserve numbers. Take the projection from Lesson 2.10&rsquo;s worked example &mdash;
    <code>near = 1</code>, <code>far = 100</code> &mdash; and pick a horizontal field of view of
    90&deg; so that <code>f / aspect = 1</code> and the arithmetic stays readable.
  </p>

  <p>First the depth-row constants:</p>

  <div class="worked">
    <span class="label">The constants</span>
    <ul>
      <li><code>A = far / (near &minus; far) = 100 / (1 &minus; 100) = &minus;100/99 = &minus;1.0101010101</code></li>
      <li><code>B = far &times; near / (near &minus; far) = 100 / (&minus;99) = &minus;1.0101010101</code></li>
    </ul>
    <p>Check the two anchors, using <code>z_n = &minus;A + B/w</code>:</p>
    <ul>
      <li><code>w = 1</code> (the near plane): <code>1.0101010101 &minus; 1.0101010101 = 0</code> &check;</li>
      <li><code>w = 100</code> (the far plane): <code>1.0101010101 &minus; 0.0101010101 = 1</code> &check;</li>
      <li><code>w = 2</code> (Lesson 2.10&rsquo;s number): <code>1.0101010101 &minus; 0.5050505050 = 0.5050505</code> &check;</li>
    </ul>
  </div>

  <p>
    Now an edge that runs away from the camera. Put both ends at <code>x_view = 1</code>,
    <code>y_view = 0</code>, one at <code>z_view = &minus;1</code> (on the near plane) and one at
    <code>z_view = &minus;100</code> (on the far plane). Because <code>f/aspect = 1</code> and
    <code>x_view = 1</code> along the whole edge, its screen x is simply <code>1/w</code>:
  </p>

  <div class="worked">
    <span class="label">The two endpoints</span>
    <ul>
      <li>near end: <code>w = 1</code>, so <code>x_ndc = 1.00</code> and <code>z_ndc = 0</code></li>
      <li>far end: <code>w = 100</code>, so <code>x_ndc = 0.01</code> and <code>z_ndc = 1</code></li>
    </ul>
    <p>
      The <strong>screen midpoint</strong> is halfway between those two screen positions:
      <code>x_ndc = (1.00 + 0.01) / 2 = 0.505</code>. Which point of the edge actually lands there?
      Since <code>x_ndc = 1/w</code>, it is the point with
      <code>w = 1 / 0.505 = 1.9801980</code>, i.e. <code>z_view = &minus;1.98</code>.
    </p>
    <p><strong>That is the whole story in one number.</strong> The pixel halfway across the screen is
      not halfway along the edge in depth &mdash; it is 1.98 units away out of a span of 99.</p>
  </div>

  <p>Now interpolate, both ways, and compare against the truth.</p>

  <div class="worked">
    <span class="label">Device depth &mdash; exact</span>
    <p>
      Linear interpolation of the endpoint values at the screen midpoint:
      <code>0.5 &times; 0 + 0.5 &times; 1 = 0.500000</code>.
    </p>
    <p>
      The true device depth at <code>w = 1.9801980</code>:
      <code>&minus;A + B/w = 1.0101010 &minus; 1.0101010/1.9801980 = 1.0101010 &minus; 0.5101010 = 0.500000</code>.
    </p>
    <p><strong>They agree exactly.</strong> Not to four places &mdash; exactly, as the derivation promised.</p>
  </div>

  <div class="worked">
    <span class="label">View-space depth &mdash; wrong by a factor of 25</span>
    <p>
      Linear interpolation of the endpoint values at the same pixel:
      <code>0.5 &times; (&minus;1) + 0.5 &times; (&minus;100) = &minus;50.5</code>.
    </p>
    <p>The truth, from above: <code>&minus;1.98</code>.</p>
    <p>
      The naive answer places that pixel <strong>25.5&times; further away than it is</strong>. Put a
      second surface at 10 units and the whole edge would vanish behind it, when in reality most of
      the edge is in front.
    </p>
  </div>

  <p>
    Figure 4 is that comparison plotted across the whole edge rather than at one point.
  </p>
''')

# ---------------- Figure 4: two-panel interpolation graph ----------------
w('''
  <figure class="dia bleed">
    <svg viewBox="0 0 660 300" role="img" aria-labelledby="fig4-t fig4-d">
      <title id="fig4-t">Device depth interpolates linearly across the screen; view depth does not</title>
      <desc id="fig4-d">Two plots against horizontal screen position. Left: device depth. The true
        value and its linear interpolation are the same straight line from 0 to 1, so they coincide
        exactly. Right: view-space depth in world units. The true value is a hyperbola that stays near
        1 for most of the screen and only shoots up to 100 at the very right-hand edge, while the
        linear interpolation is a straight line far above it; at the screen midpoint the true value is
        1.98 and the straight line reads 50.5.</desc>

      <!-- ================= left panel: device depth ================= -->
      <g transform="translate(52,54)">
        <text x="0" y="-30" class="sm" font-weight="700">device depth (what we store)</text>
        <text x="0" y="-14" class="xs t-ok">true value and linear interpolation COINCIDE</text>
        <g class="grid" stroke-width="1">
          <path d="M 0 0 H 240 M 0 44 H 240 M 0 88 H 240 M 0 132 H 240"/>
        </g>
        <line x1="0" y1="176" x2="240" y2="176" class="ink" stroke-width="1.5"/>
        <line x1="0" y1="0" x2="0" y2="176" class="ink" stroke-width="1.5"/>
        <line x1="0" y1="176" x2="240" y2="0" class="hi" stroke-width="3"/>
        <circle cx="120" cy="88" r="4" class="ink" fill="var(--dia-hi)" stroke-width="1.5"/>
        <text x="128" y="84" class="xs mono t-hi">0.500</text>
        <text x="-6" y="180" class="xs muted" text-anchor="end">0</text>
        <text x="-6" y="4" class="xs muted" text-anchor="end">1</text>
        <text x="120" y="196" class="xs muted" text-anchor="middle">screen position &rarr;</text>
        <text x="0" y="214" class="xs muted">near end</text>
        <text x="240" y="214" class="xs muted" text-anchor="end">far end</text>
      </g>

      <!-- ================= right panel: view depth ================= -->
      <g transform="translate(370,54)">
        <text x="0" y="-30" class="sm" font-weight="700">view depth, in world units</text>
        <text x="0" y="-14" class="xs t-bad">the straight line is nowhere near the truth</text>
        <g class="grid" stroke-width="1">
          <path d="M 0 0 H 240 M 0 44 H 240 M 0 88 H 240 M 0 132 H 240"/>
        </g>
        <line x1="0" y1="176" x2="240" y2="176" class="ink" stroke-width="1.5"/>
        <line x1="0" y1="0" x2="0" y2="176" class="ink" stroke-width="1.5"/>
''')
w('        <!-- naive linear interpolation -->\n')
w('        <line x1="0" y1="176" x2="240" y2="0" class="ink-soft" stroke-width="2.5" stroke-dasharray="6 4"/>\n')
w('        <!-- the truth -->\n')
w('        <polyline points="%s" class="hi" stroke-width="3" fill="none"/>\n' % true_view)
w('''        <line x1="120" y1="88" x2="120" y2="174" class="ink-soft" stroke-width="1" stroke-dasharray="3 3"/>
        <circle cx="120" cy="88" r="4" fill="var(--dia-ink-soft)"/>
        <circle cx="120" cy="174" r="4" class="ink" fill="var(--dia-hi)" stroke-width="1.5"/>
        <text x="128" y="84" class="xs mono muted">50.5 &mdash; the lerp</text>
        <text x="128" y="168" class="xs mono t-hi">1.98 &mdash; the truth</text>
        <text x="-6" y="180" class="xs muted" text-anchor="end">1</text>
        <text x="-6" y="4" class="xs muted" text-anchor="end">100</text>
        <text x="120" y="196" class="xs muted" text-anchor="middle">screen position &rarr;</text>
        <text x="0" y="214" class="xs muted">near end</text>
        <text x="240" y="214" class="xs muted" text-anchor="end">far end</text>
      </g>
    </svg>
    <figcaption><span class="fignum">Figure 4.</span> The same edge, the same pixels, two choices of
      what to store. <strong>Left:</strong> device depth is exactly linear in the screen position, so
      the interpolation and the truth are the same line and cannot be told apart. <strong>Right:</strong>
      view-space depth is a hyperbola; the dashed line is what linear interpolation would give and the
      solid curve is the truth. At the screen midpoint they read 50.5 and 1.98. The two plots are the
      same physical edge &mdash; only the vertical quantity differs.</figcaption>
  </figure>

  <p>
    One more consequence worth noticing before we move on, because it is the seed of Lesson 3.2. The
    right-hand plot is what happens to <em>any</em> quantity that varies linearly along the surface:
    a texture coordinate, a vertex colour, a normal. Depth is special precisely because the projection
    has <em>already</em> put it into its screen-affine form. Everything else a vertex carries is still
    on the wrong side of that curve, and straightening it out is exactly what 3.2 is about.
  </p>
''')

with open('docs/lessons/03-01-z-buffer.html', 'a') as f:
    f.write(out.getvalue())
print("part 3 appended:", len(out.getvalue()), "chars")
