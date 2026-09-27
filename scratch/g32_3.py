# -*- coding: utf-8 -*-
import sys, io, math
sys.path.insert(0, 'scratch')
import gen_geom32 as G
out = io.StringIO(); w = out.write
LIGHT, DARK = "#E8E2D6", "#3A4058"

w('''
  <h3 id="worked">3.4 A worked example, by hand</h3>

  <p>
    Take the exact edge Lesson 3.1 used, so the two lessons can be read against each other:
    <code>near = 1</code>, <code>far = 100</code>, a horizontal field of view of 90&deg; so that
    <code>f / aspect = 1</code>, and an edge with <code>x_view = 1</code> at both ends running from
    <code>z_view = &minus;1</code> to <code>z_view = &minus;100</code>. Paint a texture coordinate
    along it: <code>u = 0</code> at the near end, <code>u = 1</code> at the far end.
  </p>

  <div class="worked">
    <span class="label">Step 1 &mdash; which point is under the middle pixel?</span>
    <p>
      Because <code>f/aspect = 1</code> and <code>x_view = 1</code>, the screen x of any point on
      this edge is just <code>1/w</code>. So the two ends sit at <code>x_ndc = 1.00</code> and
      <code>x_ndc = 0.01</code>, and the pixel halfway between them is at
      <code>x_ndc = 0.505</code>.
    </p>
    <p>
      That pixel sees the point with <code>1/w = 0.505</code>, i.e.
      <code>w = 1.9801980</code> &mdash; <strong>1.98 units away, out of a 99-unit span</strong>.
      The middle pixel is looking at something barely past the near end.
    </p>
    <p>
      The true <code>u</code> there is the fraction of the way along the edge in <em>view</em>
      space: <code>(1.9801980 &minus; 1) / 99 = 0.00990099</code>.
    </p>
  </div>

  <div class="worked">
    <span class="label">Step 2 &mdash; what affine interpolation says</span>
    <p>
      The pixel is halfway across the screen, so the weights are <code>0.5</code> and
      <code>0.5</code>:
    </p>
    <p><code>u = 0.5 &times; 0 + 0.5 &times; 1 = 0.500000</code></p>
    <p>
      Against a truth of <code>0.0099</code>. That is <strong>50.5&times; too far along the
      texture</strong>. If the texture were a checkerboard with cells one unit wide, this pixel
      would be showing a cell fifty cells away from the one it is actually looking at.
    </p>
  </div>

  <div class="worked">
    <span class="label">Step 3 &mdash; what the correction says</span>
    <p>Pre-divide both corners by their own <code>w</code>:</p>
    <ul>
      <li><code>u/w</code> at the ends: <code>0 / 1 = 0</code> and <code>1 / 100 = 0.01</code></li>
      <li><code>1/w</code> at the ends: <code>1 / 1 = 1</code> and <code>1 / 100 = 0.01</code></li>
    </ul>
    <p>Interpolate both with the same weights, then divide:</p>
    <ul>
      <li>numerator: <code>0.5 &times; 0 + 0.5 &times; 0.01 = 0.005</code></li>
      <li>denominator: <code>0.5 &times; 1 + 0.5 &times; 0.01 = 0.505</code></li>
      <li><code>u = 0.005 / 0.505 = 0.00990099</code></li>
    </ul>
    <p><strong>Exactly the truth.</strong> Not close &mdash; equal, to every digit we computed.</p>
  </div>

  <p>
    Notice the denominator. <code>0.505</code> is the interpolated <code>1/w</code>, and its
    reciprocal is <code>1.98</code> &mdash; the <code>w</code> at that pixel, which we never
    computed directly and never needed to. The division that recovers the attribute recovers the
    depth as a side effect, which is why one divide serves every attribute at once.
  </p>

  <div class="worked">
    <span class="label">Measured over 199,241 random triangles</span>
    <p>
      <code>scratch/verify_32.cpp</code> generates random triangles in the demo's frustum, gives
      each one a random attribute linear over its surface, picks an interior point <em>in screen
      space</em>, and compares both interpolations against the true value at the surface point the
      ray actually hits:
    </p>
    <ul>
      <li>worst error, <strong>perspective-correct</strong>:
        <code>1.3 &times; 10<sup>&minus;12</sup></code> &mdash; double-precision noise, i.e. exact;</li>
      <li>worst error, <strong>affine</strong>: <code>9.89</code>, on attributes whose whole range
        is about &plusmn;13.</li>
    </ul>
  </div>

  <p>
    Figure&nbsp;3 shows the same failure spatially rather than at one point: where the rows of the
    checkerboard <em>should</em> fall, and where affine interpolation puts them.
  </p>
''')

# ---------------- Figure 3: contour comparison ----------------
CX, CY, S = 330.0, 34.0, 250.0
P = G.corners(CX, CY, S)
conts = G.v_contours(CX, CY, S)

w('''
  <figure class="dia bleed">
    <svg viewBox="0 0 660 230" role="img" aria-labelledby="fig3-t fig3-d">
      <title id="fig3-t">Where the rows of the checker land, both ways</title>
      <desc id="fig3-d">A receding quad with five horizontal division lines drawn twice. The
        perspective-correct lines bunch together toward the far edge; the affine lines are evenly
        spaced down the quad. The first row is misplaced by 32 pixels out of a quad 79 pixels tall.</desc>
''')
w('      <polygon points="%s" fill="%s" fill-opacity="0.30" class="ink" stroke-width="1.5"/>\n'
  % (G.fmt(P), "#7A8099"))
w('      <line x1="%.2f" y1="%.2f" x2="%.2f" y2="%.2f" class="ink-soft" stroke-width="1" stroke-dasharray="4 3"/>\n'
  % (P[0][0], P[0][1], P[2][0], P[2][1]))
w('      <!-- affine rows: evenly spaced, wrong -->\n')
for c, a, v in conts:
    w('      <line x1="%.2f" y1="%.2f" x2="%.2f" y2="%.2f" class="ink-soft" stroke-width="2" stroke-dasharray="5 3"/>\n'
      % (a[0][0], a[0][1], a[1][0], a[1][1]))
w('      <!-- correct rows: bunched toward the horizon -->\n')
for c, a, v in conts:
    w('      <line x1="%.2f" y1="%.2f" x2="%.2f" y2="%.2f" class="hi" stroke-width="2.5"/>\n'
      % (c[0][0], c[0][1], c[1][0], c[1][1]))
# the v=1 error bracket, drawn at the left where there is room
c1, a1, _ = conts[0]
w('      <line x1="26" y1="%.2f" x2="26" y2="%.2f" class="ax-x" stroke-width="2"/>\n' % (c1[0][1], a1[0][1]))
w('      <line x1="20" y1="%.2f" x2="32" y2="%.2f" class="ax-x" stroke-width="2"/>\n' % (c1[0][1], c1[0][1]))
w('      <line x1="20" y1="%.2f" x2="32" y2="%.2f" class="ax-x" stroke-width="2"/>\n' % (a1[0][1], a1[0][1]))
w('      <text x="38" y="%.2f" class="xs t-bad">row 1 is 32 px out of place,</text>\n' % (0.5*(c1[0][1]+a1[0][1]) - 2))
w('      <text x="38" y="%.2f" class="xs t-bad">on a quad 79 px tall</text>\n' % (0.5*(c1[0][1]+a1[0][1]) + 10))
w('''      <text x="%.0f" y="24" class="sm" text-anchor="middle" font-weight="700">where the checker rows land</text>
      <text x="470" y="176" class="xs t-hi">solid: perspective-correct</text>
      <text x="470" y="192" class="xs muted">dashed: affine</text>
      <text x="470" y="212" class="xs muted">dotted: the split diagonal</text>
    </svg>
    <figcaption><span class="fignum">Figure 3.</span> The rows of the checkerboard, placed both
      ways on the same quad. Affine interpolation spaces them evenly down the screen because that
      is what &ldquo;affine in the screen position&rdquo; means; the truth crowds them toward the
      horizon. Both agree exactly at the near and far edges &mdash; the error is zero at the
      corners and maximal in between, which is the signature of a chord drawn under a curve.</figcaption>
  </figure>
''' % CX)

# ---------------- Figure 4: u vs screen position ----------------
def u_true(t):
    return (1.0 / (1.0 - 0.99 * t) - 1.0) / 99.0

PW, PH = 250.0, 160.0
pts = " ".join("%.2f,%.2f" % (i / 120.0 * PW, PH - u_true(i / 120.0) * PH) for i in range(121))

w('''
  <p>
    And Figure&nbsp;4 is the same thing as a graph, deliberately drawn to match Lesson 3.1's
    Figure&nbsp;4 &mdash; because it is literally the same curve. Depth was the quantity that came
    out <em>straight</em> after the projection had finished with it; every other attribute is still
    on the curve.
  </p>

  <figure class="dia bleed">
    <svg viewBox="0 0 660 250" role="img" aria-labelledby="fig4-t fig4-d">
      <title id="fig4-t">The texture coordinate against screen position</title>
      <desc id="fig4-d">A plot of the texture coordinate u against horizontal screen position along
        an edge running from the near plane to the far plane. The true value is a curve that hugs
        zero for most of the screen and rises steeply at the far end; affine interpolation is a
        straight line from 0 to 1. At the screen midpoint the straight line reads 0.5 and the curve
        reads 0.0099.</desc>

      <g transform="translate(80,40)">
        <text x="0" y="-18" class="sm" font-weight="700">the texture coordinate u along a receding edge</text>
        <g class="grid" stroke-width="1">
          <path d="M 0 0 H 250 M 0 40 H 250 M 0 80 H 250 M 0 120 H 250"/>
        </g>
        <line x1="0" y1="160" x2="250" y2="160" class="ink" stroke-width="1.5"/>
        <line x1="0" y1="0" x2="0" y2="160" class="ink" stroke-width="1.5"/>
''')
w('        <line x1="0" y1="160" x2="250" y2="0" class="ink-soft" stroke-width="2.5" stroke-dasharray="6 4"/>\n')
w('        <polyline points="%s" class="hi" stroke-width="3" fill="none"/>\n' % pts)
w('''        <line x1="125" y1="80" x2="125" y2="158" class="ink-soft" stroke-width="1" stroke-dasharray="3 3"/>
        <circle cx="125" cy="80" r="4" fill="var(--dia-ink-soft)"/>
        <circle cx="125" cy="158.4" r="4" class="ink" fill="var(--dia-hi)" stroke-width="1.5"/>
        <text x="118" y="76" class="xs mono muted" text-anchor="end">0.500 &mdash; affine</text>
        <text x="133" y="152" class="xs mono t-hi">0.0099 &mdash; the truth</text>
        <text x="-6" y="164" class="xs muted" text-anchor="end">0</text>
        <text x="-6" y="4" class="xs muted" text-anchor="end">1</text>
        <text x="125" y="182" class="xs muted" text-anchor="middle">screen position &rarr;</text>
        <text x="0" y="200" class="xs muted">near end</text>
        <text x="250" y="200" class="xs muted" text-anchor="end">far end</text>
      </g>

      <g transform="translate(400,80)">
        <text x="0" y="-18" class="sm" font-weight="700">the fix, in one line</text>
        <text x="0" y="6" class="xs muted">u is a curve, so do not interpolate u.</text>
        <text x="0" y="26" class="xs muted">Interpolate u/w and 1/w &mdash; both straight &mdash;</text>
        <text x="0" y="46" class="xs muted">and divide one by the other at the end.</text>
        <text x="0" y="76" class="xs mono t-ok">0.005 / 0.505 = 0.00990099</text>
        <text x="0" y="100" class="xs muted">The straight line and the curve agree at</text>
        <text x="0" y="120" class="xs muted">both ends, which is exactly why checking</text>
        <text x="0" y="140" class="xs muted">your corner values never finds this bug.</text>
      </g>
    </svg>
    <figcaption><span class="fignum">Figure 4.</span> The texture coordinate along the worked
      example's edge. This is the same hyperbola as Lesson 3.1's Figure 4 &mdash; the projection
      warps everything on the surface the same way, and depth was special only because the
      projection matrix had already unwarped it. At the screen midpoint the straight line reads
      <code>0.5</code> against a truth of <code>0.0099</code>.</figcaption>
  </figure>

  <h3 id="not-depth">3.5 The one attribute that must not be corrected</h3>

  <p>
    Having learnt a trick this good, the natural instinct is to apply it to everything. Do not apply
    it to depth.
  </p>

  <p>
    Lesson 3.1 &sect;3.4 proved that device depth is <em>already</em> affine in screen space, for
    the precise reason that <code>z_n = -A + B\\cdot(1/w)</code> and <code>1/w</code> is affine.
    Depth arrives pre-corrected: the projection matrix's third row did this exact job in advance,
    which is what that row is <em>for</em>. Push it through the correction a second time and you
    get the interpolation of <code>z_n/w</code> divided by the interpolation of <code>1/w</code>,
    which is not <code>z_n</code> and is not anything.
  </p>

  <div class="callout pitfall">
    <span class="label">Why this one is nasty</span>
    <p>
      It does not crash, it does not look obviously wrong, and it is <em>self-consistent</em> —
      every triangle is corrupted the same way, so surfaces still occlude each other plausibly.
      What you get is subtly wrong occlusion that worsens with distance, which reads like a depth
      <em>precision</em> problem and sends you off to change your near plane. The regression test is
      trivial and worth having: <strong>the contents of the depth buffer must not change at all</strong>
      when perspective correction is switched on. The harness checks exactly that &mdash; over a
      triangle whose colours change on 20,590 pixels, the depth buffer differs on <code>0</code>.
    </p>
  </div>

  <p>
    So the rasterizer's inner loop treats depth and everything else differently, on purpose, and
    the code says so where it happens. That asymmetry is not a wart; it is the shape of the
    problem.
  </p>
''')

open('docs/lessons/03-02-perspective-correct.html', 'a').write(out.getvalue())
print("part 3:", len(out.getvalue()), "chars")
