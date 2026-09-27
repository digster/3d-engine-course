# -*- coding: utf-8 -*-
import sys, io, math
sys.path.insert(0, 'scratch')
import gen_geom32 as G
out = io.StringIO(); w = out.write

w('''
  <!-- ================= 2. BUILDING INTUITION ================= -->
  <h2 id="intuition"><span class="num">2</span>Building Intuition</h2>

  <p>
    Stand on a tiled floor and look along it. The tile at your feet fills a large part of your
    view; the tile ten paces away is a sliver. They are the same tile. Nothing about the tiles
    changed &mdash; what changed is how much <em>screen</em> each one is entitled to, and that is
    decided by the divide by depth.
  </p>

  <p>
    Now run that backwards, which is what a rasterizer does. You are standing at some pixel,
    halfway down the screen between the near edge of the floor and the far edge, and you want to
    know <strong>which tile you are looking at</strong>. The tempting answer &mdash; halfway
    between the near tile and the far tile &mdash; is wrong, and Figure&nbsp;2 shows how wrong.
  </p>

  <figure class="dia bleed">
    <svg viewBox="0 0 660 260" role="img" aria-labelledby="fig2-t fig2-d">
      <title id="fig2-t">Equal steps along the ground are unequal steps on the screen</title>
      <desc id="fig2-d">A side view: the eye at the left, a vertical screen plane just in front of
        it, and the ground running away to the right. Six equally spaced marks on the ground are
        joined by sight lines to the screen, where their images are crowded together toward the top.
        The first ground step takes half the screen; the last four together take a fifth of it.</desc>

      <!-- the eye -->
      <circle cx="56" cy="150" r="8" class="ink" fill="var(--dia-fill)" stroke-width="1.5"/>
      <text x="56" y="176" class="xs muted" text-anchor="middle">eye</text>

      <!-- ground plane -->
      <line x1="56" y1="216" x2="628" y2="216" class="ink" stroke-width="1.5"/>
      <text x="628" y="234" class="xs muted" text-anchor="end">the ground, running away &rarr;</text>

      <!-- screen plane -->
      <line x1="130" y1="60" x2="130" y2="230" class="ink" stroke-width="2"/>
      <text x="130" y="50" class="xs muted" text-anchor="middle">screen</text>
''')

# Geometry: eye at (56,150). ground y=216 (66 below the eye). screen at x=130 (74 from the eye).
# A ground point at horizontal distance d projects to screen y = 150 + 66*74/d.
EYE_X, EYE_Y, GY, SX = 56.0, 150.0, 216.0, 130.0
H = GY - EYE_Y          # 66
D = SX - EYE_X          # 74
ground_x = [130.0, 205.0, 280.0, 355.0, 430.0, 505.0, 580.0]   # equal steps on the ground

marks = []
for gx in ground_x:
    d = gx - EYE_X
    sy = EYE_Y + H * D / d
    marks.append((gx, sy))

w('      <!-- sight lines from the eye to equally spaced ground marks -->\n')
w('      <g class="ink-soft" stroke-width="1">\n')
for gx, sy in marks:
    w('        <line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke-dasharray="2 3"/>\n'
      % (EYE_X, EYE_Y, gx, GY))
w('      </g>\n')

w('      <!-- the ground marks: equally spaced, by construction -->\n')
for i, (gx, sy) in enumerate(marks):
    w('      <line x1="%.1f" y1="210" x2="%.1f" y2="222" class="hi" stroke-width="2"/>\n' % (gx, gx))
    w('      <text x="%.1f" y="206" class="xs mono muted" text-anchor="middle">%d</text>\n' % (gx, i))

w('      <!-- their images on the screen: crowded -->\n')
for i, (gx, sy) in enumerate(marks):
    w('      <line x1="124" y1="%.1f" x2="136" y2="%.1f" class="ax-y" stroke-width="2"/>\n' % (sy, sy))

w('''      <text x="112" y="%.1f" class="xs mono t-ok" text-anchor="end">0</text>
      <text x="112" y="%.1f" class="xs mono t-ok" text-anchor="end">1</text>
      <text x="112" y="%.1f" class="xs mono t-ok" text-anchor="end">2</text>
      <text x="150" y="%.1f" class="xs t-ok">3, 4, 5, 6 &mdash; all inside these few pixels</text>
      <text x="300" y="112" class="xs muted">equal on the ground &mdash;</text>
      <text x="300" y="126" class="xs t-bad">nowhere near equal on the screen</text>
    </svg>
    <figcaption><span class="fignum">Figure 2.</span> Six equal steps along the ground and where they
      land on the screen. The first step alone takes more screen than the remaining five together.
      Barycentric interpolation walks the screen in equal steps and assumes the surface does too
      &mdash; so at the halfway pixel it reports the halfway tile, when the true answer is barely
      past the first one.</figcaption>
  </figure>
''' % (marks[0][1] + 4, marks[1][1] + 4, marks[2][1] + 4, marks[4][1] + 4))

w('''
  <p>
    Here is the key move, and it is worth pausing on because the whole lesson turns on it. That
    crowding is <em>exactly</em> what dividing by <code>w</code> does &mdash; it is the same
    division, applied to the same points. Lesson 3.1 proved that <code>1/w</code> is a perfectly
    well-behaved, affine function of the screen position: it is the one quantity that <em>does</em>
    march in equal steps as you walk across the screen.
  </p>

  <p>
    So we have a quantity that behaves (<code>1/w</code>) and a quantity that does not (the
    attribute). The trick, and it is the whole trick, is to <strong>multiply the badly-behaved thing
    by the well-behaved one</strong>, interpolate that, and undo the multiplication at the end. It
    should not be obvious that this works. The next section shows that it does, and why.
  </p>

  <!-- ================= 3. THE THEORY ================= -->
  <h2 id="theory"><span class="num">3</span>The Theory</h2>

  <h3 id="promise">3.1 What barycentric interpolation actually promises</h3>

  <p>
    It is worth being precise about what we have been using, because the bug is not in the
    machinery &mdash; the machinery does exactly what it says. Given three values at three screen
    positions, barycentric interpolation produces <strong>the unique affine function of the pixel
    position that agrees with them</strong> (Lesson 2.3 &sect;3.7). That is the guarantee. Not
    &ldquo;the value on the surface&rdquo;, not &ldquo;the right answer&rdquo; &mdash; an affine
    function of <em>x</em> and <em>y</em>, on the screen.
  </p>

  <p>
    So barycentric interpolation of a quantity is correct exactly when that quantity happens to be
    affine in screen space, and wrong otherwise. Lesson 3.1 asked that question about depth and got
    a surprising yes for device depth and a firm no for view-space <code>z</code>. Now we ask it
    about everything else.
  </p>

  <h3 id="linear">3.2 An attribute is linear over the surface</h3>

  <p>
    First, what <em>is</em> true of a texture coordinate. When a modeller assigns uvs to a triangle,
    the uv at an interior point is defined to vary linearly across the <em>surface</em> &mdash;
    that is what it means to texture a flat triangle. The same holds for a vertex colour, and for
    an interpolated normal. So any such attribute <code>a</code> can be written as an affine
    function of the view-space position:
  </p>

  <div class="eq">
    <span class="katex-src">\\[ a(x_v, y_v, z_v) = \\alpha x_v + \\beta y_v + \\gamma z_v + \\delta \\]</span>
    <span class="eq-plain">a = alpha*x_view + beta*y_view + gamma*z_view + delta</span>
  </div>

  <p>
    We never need to know the four constants &mdash; they are fixed by the three corner values and
    the triangle's plane, and they will cancel out of everything below. What matters is only that
    <em>such constants exist</em>, which is precisely the statement &ldquo;the attribute is linear
    over the surface&rdquo;.
  </p>

  <h3 id="derivation">3.3 The derivation: a/w is affine too</h3>

  <p>
    Take the projection relations from Lesson 2.10, in the form Lesson 3.1 rearranged them into,
    writing <em>w</em> for <code>-z_view</code>:
  </p>

  <div class="eq">
    <span class="katex-src">\\[ x_v = \\frac{a_r}{f}\\,x_n\\,w, \\qquad y_v = \\frac{1}{f}\\,y_n\\,w, \\qquad z_v = -w \\]</span>
    <span class="eq-plain">x_view = (aspect/f) * x_ndc * w,   y_view = (1/f) * y_ndc * w,   z_view = -w</span>
  </div>

  <p>Substitute all three into the attribute:</p>

  <div class="eq">
    <span class="katex-src">\\[ a = \\left(\\alpha\\frac{a_r}{f}x_n + \\beta\\frac{1}{f}y_n - \\gamma\\right)w \;+\; \\delta \\]</span>
    <span class="eq-plain">a = ( alpha*(aspect/f)*x_ndc + beta*(1/f)*y_ndc - gamma ) * w  +  delta</span>
  </div>

  <p>
    That is <em>not</em> affine in the screen position &mdash; there is a <code>w</code> sitting in
    the middle of it, and <code>w</code> is a hyperbola across the screen. But now divide the whole
    thing by <code>w</code>:
  </p>

  <div class="eq">
    <span class="katex-src">\\[ \\frac{a}{w} = \\underbrace{\\alpha\\frac{a_r}{f}x_n + \\beta\\frac{1}{f}y_n - \\gamma}_{\\text{affine in }(x_n,\\,y_n)} \;+\; \\delta\\cdot\\underbrace{\\frac{1}{w}}_{\\text{affine, by 3.1}} \\]</span>
    <span class="eq-plain">a/w = ( alpha*(aspect/f)*x_ndc + beta*(1/f)*y_ndc - gamma )  +  delta * (1/w)</span>
  </div>

  <p>
    Read the right-hand side. The first group is a constant plus constants times the screen
    coordinates &mdash; affine, by inspection. The second is a constant times <code>1/w</code>,
    which Lesson 3.1 &sect;3.4 proved is affine in the screen position. A sum of affine functions is
    affine. Therefore:
  </p>

  <div class="callout ok">
    <span class="label">The result</span>
    <p>
      <strong><code>a/w</code> varies affinely across the screen</strong>, for any attribute
      <code>a</code> that is linear over the surface. And <code>1/w</code> does too. So both can be
      interpolated barycentrically &mdash; <em>exactly</em>, not approximately &mdash; and the
      attribute recovered by dividing one by the other:
    </p>
  </div>

  <div class="eq">
    <span class="katex-src">\\[ a \;=\; \\frac{\;\\sum_i f_i \\, (a_i / w_i)\;}{\;\\sum_i f_i \\, (1 / w_i)\;} \\]</span>
    <span class="eq-plain">a = ( f0*a0/w0 + f1*a1/w1 + f2*a2/w2 ) / ( f0/w0 + f1/w1 + f2/w2 )</span>
  </div>

  <p>
    where <code>f0, f1, f2</code> are the same unbiased barycentric weights the fill already
    computes. Three multiply-adds for the numerator, three for the denominator, one divide. That is
    the entire technique, and every piece of it was already sitting in the loop.
  </p>

  <div class="callout note">
    <span class="label">Why this is one derivation and not four</span>
    <p>
      Nothing above mentioned texture coordinates. <code>&alpha;, &beta;, &gamma;, &delta;</code>
      were never given values and cancelled from the conclusion, so the result covers a uv, a colour
      channel, a normal component, a bone weight &mdash; anything linear over the surface, which in
      practice means every attribute a vertex has ever carried. The GPU calls them
      <em>varyings</em> and applies exactly this to all of them, which is why the hardware
      documentation can describe interpolation in one paragraph.
    </p>
  </div>
''')

open('docs/lessons/03-02-perspective-correct.html', 'a').write(out.getvalue())
print("part 2:", len(out.getvalue()), "chars")
