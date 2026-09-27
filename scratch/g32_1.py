# -*- coding: utf-8 -*-
import sys, io
sys.path.insert(0, 'scratch')
import gen_geom32 as G

out = io.StringIO(); w = out.write
LIGHT, DARK = "#E8E2D6", "#3A4058"

w('''<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>3.2 &mdash; Perspective-Correct Interpolation &middot; Build a Professional 3D Game Engine</title>
<meta name="description" content="Why textures swim on a receding surface, and the one-line fix that follows from a fact Lesson 3.1 already proved: interpolate a/w and 1/w, then divide.">

<!-- ==========================================================================
     SHARED COURSE STYLESHEET  —  v1.0
     ==========================================================================
     This block is IDENTICAL in every lesson file. It is duplicated rather than
     linked because each lesson must be a fully self-contained document that
     renders from a bare filesystem with no network and no build step.

     Source of truth: docs/_template/lesson-template.html
     ========================================================================== -->
<!-- SHARED-CSS:BEGIN -->
<!-- SHARED-CSS:END -->

<!-- KaTeX (optional). If unreachable the raw TeX remains readable, and every
     equation is also stated in prose + .eq-plain, so nothing is lost. -->
<link rel="stylesheet"
      href="https://cdn.jsdelivr.net/npm/katex@0.16.11/dist/katex.min.css"
      integrity="sha384-nB0miv6/jRmo5UMMR1wu3Gz6NLsoTkbqJghGIsx//Rlm+ZU03BU6SQNC66uf4l5+"
      crossorigin="anonymous">
</head>
<body>

<header class="masthead">
  <div class="masthead-inner">
    <a class="course" href="../index.html">Build a Professional 3D Game Engine</a>
    <span class="spacer"></span>
    <a href="../index.html">Contents</a>
    <a href="../conventions.html">Conventions</a>
    <a href="../math-toolbox.html">Math Toolbox</a>
    <button class="theme-toggle" id="theme-toggle" type="button" aria-label="Toggle colour theme">Theme</button>
  </div>
</header>

<div class="wrap">

  <nav class="lesson-nav" aria-label="Lesson navigation (top)">
    <a class="prev-l" href="03-01-z-buffer.html">
      <span class="dir">&larr; Previous</span>
      <span class="ttl">3.1 &mdash; The Painter&rsquo;s Problem and the Z-Buffer</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="03-03-near-plane-clipping.html">
      <span class="dir">Next &rarr;</span>
      <span class="ttl">3.3 &mdash; Near-Plane Clipping</span>
    </a>
  </nav>

  <div class="lesson-head">
    <div class="eyebrow">Module 3 &mdash; Software Rasterizer II &middot; Lesson 3.2</div>
    <h1>Perspective-Correct Interpolation</h1>
    <p class="deck">
      Lesson 3.1 proved that depth interpolates exactly across a triangle, and quietly noted that
      nothing else does. This is the bill for that sentence. We put a checkerboard on a floor,
      watch it buckle, and fix it with a division that costs one instruction per pixel and
      follows directly from a fact we already proved.
    </p>

    <div class="meta">
      <dl>
        <dt>Time</dt>
        <dd>&asymp; 3&ndash;4 hours</dd>

        <dt>Prereqs</dt>
        <dd>
          <a href="02-03-barycentric.html">2.3 Barycentric coordinates</a>,
          <a href="02-04-attribute-interpolation.html">2.4 Interpolating attributes</a>,
          <a href="02-10-perspective.html">2.10 Perspective</a>,
          <a href="03-01-z-buffer.html">3.1 &sect;3.4 especially</a>
        </dd>

        <dt>Files</dt>
        <dd>
          <code>src/gfx/raster.hpp</code>, <code>src/gfx/raster.cpp</code>,
          <code>src/gfx/mesh.hpp</code>, <code>src/main.cpp</code>
        </dd>

        <dt>Milestone</dt>
        <dd>A checkered plane running to the horizon that is <em>right</em> &mdash; drawn with two
          triangles, not two thousand. Plus a key that puts the artifact back, and a counter
          measuring it.</dd>
      </dl>
    </div>
  </div>

  <section class="objectives" aria-labelledby="obj-h">
    <h2 id="obj-h">By the end of this lesson you will be able to&hellip;</h2>
    <ul>
      <li>&hellip;recognise affine texture warping on sight, and read the split direction of a quad
        off the artifact.</li>
      <li>&hellip;<strong>derive</strong> that <code>a/w</code> is affine in screen space from the
        fact that <code>1/w</code> is, in five lines.</li>
      <li>&hellip;implement the correction, and say exactly which attribute must <em>not</em> receive
        it and why.</li>
      <li>&hellip;explain why subdivision reduces the error quadratically and never removes it, and
        why that trade defined the look of an entire console generation.</li>
      <li>&hellip;state the cost honestly: one divide per pixel, amortised over every attribute.</li>
    </ul>
  </section>

  <details class="toc" open>
    <summary>Contents</summary>
    <ol>
      <li><a href="#problem">The Problem</a></li>
      <li><a href="#intuition">Building Intuition</a></li>
      <li><a href="#theory">The Theory</a>
        <ol>
          <li><a href="#promise">What barycentric interpolation actually promises</a></li>
          <li><a href="#linear">An attribute is linear over the surface</a></li>
          <li><a href="#derivation">The derivation: a/w is affine too</a></li>
          <li><a href="#worked">A worked example, by hand</a></li>
          <li><a href="#not-depth">The one attribute that must not be corrected</a></li>
          <li><a href="#cost">What it costs, and the escape route people took instead</a></li>
        </ol>
      </li>
      <li><a href="#implementation">Implementation</a></li>
      <li><a href="#listings">Complete Code Listings</a></li>
      <li><a href="#build">Build &amp; Run</a></li>
      <li><a href="#pitfalls">Common Pitfalls &amp; Debugging</a></li>
      <li><a href="#exercises">Exercises</a></li>
      <li><a href="#recap">Recap &amp; Next</a></li>
      <li><a href="#reading">Further Reading</a></li>
    </ol>
  </details>

  <!-- ================= 1. THE PROBLEM ================= -->
  <h2 id="problem"><span class="num">1</span>The Problem</h2>

  <p>
    Put a checkerboard on the ground and look along it. Every square is the same size in the world;
    on screen they should shrink smoothly toward a vanishing point, in the way that is so familiar
    it is invisible. Draw that floor with the rasterizer we have and you get Figure&nbsp;1, left.
  </p>
''')

# ---------------- Figure 1: the artifact, both ways ----------------
CY, S = 26.0, 190.0
LX, RX = 160.0, 500.0

w('''
  <figure class="dia bleed">
    <svg viewBox="0 0 660 200" role="img" aria-labelledby="fig1-t fig1-d">
      <title id="fig1-t">A checkered floor drawn with affine and with perspective-correct interpolation</title>
      <desc id="fig1-d">Two renderings of the same receding checkered quad. On the left, affine
        interpolation: the checker squares are all the same depth on screen, so the pattern looks
        stretched and the cell edges kink where the quad is split into two triangles. On the right,
        perspective-correct interpolation: the rows of squares bunch up toward the horizon and the
        pattern reads as a floor.</desc>
''')

def panel(cx, label, sub, correct, cls):
    b = []
    P = G.corners(cx, CY, S)
    b.append('      <text x="%.0f" y="20" class="sm" text-anchor="middle" font-weight="700">%s</text>'
             % (cx, label))
    b.append('      <text x="%.0f" y="34" class="xs %s" text-anchor="middle">%s</text>' % (cx, cls, sub))
    b.append('      <polygon points="%s" fill="%s" class="ink" stroke-width="1"/>' % (G.fmt(P), LIGHT))
    if correct:
        for c in G.cells_correct(cx, CY, S):
            b.append('      <polygon points="%s" fill="%s"/>' % (c, DARK))
    else:
        for k, (tripts, cells) in enumerate(G.cells_affine(cx, CY, S)):
            cid = 'f1clip%d' % (k if cx < 300 else k + 2)
            b.append('      <defs><clipPath id="%s"><polygon points="%s"/></clipPath></defs>'
                     % (cid, tripts))
            b.append('      <g clip-path="url(#%s)">' % cid)
            for c in cells:
                b.append('        <polygon points="%s" fill="%s"/>' % (c, DARK))
            b.append('      </g>')
    b.append('      <polygon points="%s" fill="none" class="ink" stroke-width="1.5"/>' % G.fmt(P))
    return "\n".join(b) + "\n"

w(panel(LX, "affine", "the pattern the rasterizer draws today", False, "t-bad"))
w(panel(RX, "perspective-correct", "what the geometry actually says", True, "t-ok"))
w('''      <text x="%.0f" y="160" class="xs muted" text-anchor="middle">two triangles</text>
      <text x="%.0f" y="160" class="xs muted" text-anchor="middle">the same two triangles</text>
      <text x="%.0f" y="176" class="xs muted" text-anchor="middle">the kink runs along their shared diagonal</text>
      <text x="%.0f" y="176" class="xs muted" text-anchor="middle">rows bunch toward the horizon</text>
    </svg>
    <figcaption><span class="fignum">Figure 1.</span> The same quad, the same camera, the same two
      triangles &mdash; only how the texture coordinates are carried across the interior differs.
      Every corner is <em>exactly right</em> in both pictures; the error is entirely in the middle,
      which is why checking your vertices will never find it. Both diagrams are computed, not drawn:
      the left is what an affine map does to the uv lattice, the right is that lattice projected
      through the camera.</figcaption>
  </figure>
''' % (LX, RX, LX, RX))

w('''
  <p>
    The squares are the wrong size everywhere except at the corners. Worse, the pattern
    <strong>kinks along the diagonal</strong> the quad happens to be split on &mdash; a line that
    exists nowhere in the geometry, nowhere in the texture, and only in our choice of how to cut a
    rectangle into two triangles. Split it the other way and the crease moves with it.
  </p>

  <div class="callout warn">
    <span class="label">You have seen this before</span>
    <p>
      This is the defining visual signature of the first generation of 3-D consoles. The PlayStation
      had no perspective correction in hardware at all, so its floors and walls warped exactly like
      this, and the warping <em>swam</em> as the camera moved &mdash; because the error depends on
      where the triangle is, not on what is painted on it. If you have ever wondered why those games
      look the way they do in a way that emulators reproduce so faithfully, this is most of the
      answer.
    </p>
  </div>

  <p>
    Notice what is <em>not</em> wrong. The triangle is in the right place. Its edges are correct.
    The three corners have exactly the right texture coordinates. The depth buffer is untouched and
    was right in Lesson 3.1 and is still right now. The bug is confined to the interior of a
    triangle, which is the one place we have not looked since Lesson 2.4 &mdash; and the reason it
    survived that long is that Module 2's demo triangles were all roughly parallel to the screen,
    where, as we are about to prove, the error is exactly zero.
  </p>

  <p>
    So: what does barycentric interpolation actually promise, and why is that promise not what we
    want?
  </p>
''')

open('docs/lessons/03-02-perspective-correct.html', 'w').write(out.getvalue())
print("part 1:", len(out.getvalue()), "chars")
