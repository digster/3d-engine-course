# -*- coding: utf-8 -*-
import sys, io
sys.path.insert(0, 'scratch')
from gen_geom import weave, fmt

C, P, O = weave(158, 162, 80, 20, 36)
# plank colours: amber, teal, violet (the demo's tints)
AMB, TEA, VIO = "#E0A83C", "#3CB8A8", "#A070D8"

out = io.StringIO()
w = out.write

w('''<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>3.1 &mdash; The Painter&#39;s Problem and the Z-Buffer &middot; Build a Professional 3D Game Engine</title>
<meta name="description" content="Why sorting triangles by depth cannot work, and how one float per pixel solves hidden-surface removal exactly — including why device depth interpolates correctly and view-space depth does not.">

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
    <a class="prev-l" href="02-12-wireframe-mesh.html">
      <span class="dir">&larr; Previous</span>
      <span class="ttl">2.12 &mdash; Milestone: A Spinning Wireframe Mesh</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="03-02-perspective-correct.html">
      <span class="dir">Next &rarr;</span>
      <span class="ttl">3.2 &mdash; Perspective-Correct Interpolation</span>
    </a>
  </nav>

  <div class="lesson-head">
    <div class="eyebrow">Module 3 &mdash; Software Rasterizer II &middot; Lesson 3.1</div>
    <h1>The Painter&rsquo;s Problem and the Z-Buffer</h1>
    <p class="deck">
      Module 2 built every transform that puts a triangle on the screen. It never asked which
      triangle you should actually <em>see</em>. This lesson shows why the obvious answer &mdash;
      sort them and paint back to front &mdash; cannot be made to work, derives the one that can,
      and turns the wireframe into a solid.
    </p>

    <div class="meta">
      <dl>
        <dt>Time</dt>
        <dd>&asymp; 4&ndash;5 hours</dd>

        <dt>Prereqs</dt>
        <dd>
          <a href="02-03-barycentric.html">2.3 Barycentric coordinates</a>,
          <a href="02-04-attribute-interpolation.html">2.4 Interpolating attributes</a>,
          <a href="02-10-perspective.html">2.10 Perspective</a>,
          <a href="02-11-viewport.html">2.11 The viewport transform</a>,
          <a href="02-12-wireframe-mesh.html">2.12</a>
        </dd>

        <dt>Files</dt>
        <dd>
          <code>src/gfx/depth_buffer.hpp</code> (new),
          <code>src/gfx/depth_buffer.cpp</code> (new),
          <code>src/gfx/raster.hpp</code>, <code>src/gfx/raster.cpp</code>,
          <code>src/gfx/mesh.hpp</code>, <code>src/main.cpp</code>,
          <code>CMakeLists.txt</code>
        </dd>

        <dt>Milestone</dt>
        <dd>The icosahedron stops being see-through. Solid, correctly occluded 3-D, with a
          key that switches between sorting and per-pixel depth and a counter that says how many
          pixels sorting gets wrong.</dd>
      </dl>
    </div>
  </div>

  <section class="objectives" aria-labelledby="obj-h">
    <h2 id="obj-h">By the end of this lesson you will be able to&hellip;</h2>
    <ul>
      <li>&hellip;state the three independent reasons the painter&rsquo;s algorithm cannot be repaired, and
        construct a scene that defeats it.</li>
      <li>&hellip;implement a depth buffer and a depth-tested triangle fill, and say what to clear it to
        and why the other choice draws nothing at all.</li>
      <li>&hellip;<strong>derive</strong> why device depth interpolates exactly across a triangle in screen
        space while view-space depth does not &mdash; and why that error hides on flat walls.</li>
      <li>&hellip;predict z-fighting from <code>near</code>, <code>far</code> and the depth format, in
        real units, before you see it.</li>
      <li>&hellip;recognise the artifacts &mdash; a scene eating itself, a black screen, a shimmering
        surface &mdash; and name the cause of each on sight.</li>
    </ul>
  </section>

  <details class="toc" open>
    <summary>Contents</summary>
    <ol>
      <li><a href="#problem">The Problem</a>
        <ol>
          <li><a href="#see-through">The solid you can see through</a></li>
          <li><a href="#painter">The painter&rsquo;s algorithm, and why it looks like the answer</a></li>
          <li><a href="#three-failures">Three failures, none of them fixable</a></li>
        </ol>
      </li>
      <li><a href="#intuition">Building Intuition</a></li>
      <li><a href="#theory">The Theory</a>
        <ol>
          <li><a href="#algorithm">The algorithm, stated</a></li>
          <li><a href="#clear">What to clear it to</a></li>
          <li><a href="#which-depth">Which depth do we store?</a></li>
          <li><a href="#derivation">Deriving it: device depth is affine in screen space</a></li>
          <li><a href="#worked">A worked example, by hand</a></li>
          <li><a href="#precision">Precision, and z-fighting</a></li>
          <li><a href="#cost">What it costs</a></li>
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

  <h3 id="see-through">1.1 The solid you can see through</h3>

  <p>
    Lesson 2.12 ended with an icosahedron spinning in perspective, and with an honest list of what
    was still wrong with it. Top of that list: <strong>you can see straight through it</strong>. The
    edges on the far side of the solid are drawn over the edges on the near side, not because
    anything is transparent, but because nothing in the renderer has ever compared two surfaces and
    decided which one you are looking at.
  </p>

  <p>
    We papered over it. <code>draw_mesh</code> dimmed distant edges so your eye could guess at the
    shape &mdash; a cue, and we said so at the time. Fill those triangles in instead of outlining
    them and the cue stops helping: whichever triangle happens to be drawn last simply wins, and the
    back of the solid paints over the front. The picture is not merely ugly, it is <em>wrong</em>, and
    it is wrong in a way that changes every time the object turns.
  </p>

  <div class="callout warn">
    <span class="label">The failure mode</span>
    <p>
      Draw filled triangles in index order and the image depends on the order the triangles happen to
      appear in the array. Rotate the mesh and the picture flickers between interpretations, because
      the array order never changes but which faces are in front does. There is no amount of
      cleverness in the fill routine that can fix this: the fill does not know what else is on screen.
    </p>
  </div>

  <h3 id="painter">1.2 The painter&rsquo;s algorithm, and why it looks like the answer</h3>

  <p>
    The obvious fix is the one a painter uses. Paint the background first, then the middle distance,
    then the foreground; each layer covers what is behind it. In code: compute a depth for every
    triangle, sort back to front, draw in that order. This is the <strong>painter&rsquo;s
    algorithm</strong>, and Exercise 2.12.5 asked you to try it precisely so that this lesson could
    take it away from you.
  </p>

  <p>
    It is not a straw man. It was the standard technique in the early 1970s, it is what Newell,
    Newell and Sancha refined in 1972, and its descendant &mdash; BSP-tree ordering &mdash; is how
    <em>Doom</em> and <em>Quake</em> resolved visibility. It also still has a job today, which we come
    back to at the end of the lesson. And on our milestone scene it appears to work perfectly: sort
    the icosahedron&rsquo;s twenty triangles by average view-space depth, draw them furthest first,
    and a convincing solid appears.
  </p>

  <p>
    So what depth do you sort by? A triangle has three vertices at three different depths. The usual
    answer is the average of the three &mdash; the centroid&rsquo;s depth. Hold on to that, because it
    is the first of the three cracks.
  </p>

  <h3 id="three-failures">1.3 Three failures, none of them fixable</h3>

  <p>
    <strong>Failure one: a triangle does not have <em>a</em> depth.</strong> A long triangle running
    away from the camera spans a range of depths. Its average describes none of them. Two such
    triangles can each be in front of the other &mdash; over different parts of the screen &mdash;
    and any single number you compute for each is a summary that has thrown away the fact you needed.
  </p>

  <p>
    You can attack this by splitting long triangles into shorter ones. That trades a correctness
    problem for an unbounded amount of geometry, and it does not touch the next two failures at all.
  </p>

  <p>
    <strong>Failure two: the correct order can be a cycle.</strong> Take three flat panels and weave
    them, exactly as you would weave three sticks: each one passes over the next and under the one
    before. Figure&nbsp;1 shows the arrangement. Panel A is in front of B at the corner they share; B
    is in front of C at theirs; C is in front of A at theirs. Now put those three panels in an order.
  </p>

  <p>
    You cannot. &ldquo;In front of&rdquo; here is not a transitive relation, and a sort assumes it is.
    Any sorting algorithm you write &mdash; by centroid, by nearest vertex, by farthest vertex,
    by anything &mdash; produces <em>some</em> order, and at least one of the three overlaps will be
    painted the wrong way round.
  </p>
''')

# ---------------- Figure 1: the weave + cycle graph ----------------
w('''
  <figure class="dia bleed">
    <svg viewBox="0 0 660 330" role="img" aria-labelledby="fig1-t fig1-d">
      <title id="fig1-t">Three woven panels forming a cyclic depth order</title>
      <desc id="fig1-d">On the left, three long flat panels are laid along the sides of a triangle and
        woven: panel A passes in front of panel B at the bottom-left corner, B in front of C at the
        bottom-right corner, and C in front of A at the top corner. On the right, a directed graph of
        three nodes A, B and C with arrows A to B, B to C and C back to A, forming a loop labelled
        &quot;no valid order&quot;.</desc>
''')
# planks first, then the "over" patches
w('      <g>\n')
w('        <polygon points="%s" fill="%s" fill-opacity="0.92" class="ink" stroke-width="1"/>\n' % (P[0], AMB))
w('        <polygon points="%s" fill="%s" fill-opacity="0.92" class="ink" stroke-width="1"/>\n' % (P[1], TEA))
w('        <polygon points="%s" fill="%s" fill-opacity="0.92" class="ink" stroke-width="1"/>\n' % (P[2], VIO))
w('      </g>\n')
w('      <!-- the three "over" crossings, each redrawn on top of the panel it covers -->\n')
w('      <g>\n')
w('        <polygon points="%s" fill="%s" fill-opacity="0.92" class="ink" stroke-width="1"/>\n' % (O[0], AMB))
w('        <polygon points="%s" fill="%s" fill-opacity="0.92" class="ink" stroke-width="1"/>\n' % (O[1], TEA))
w('        <polygon points="%s" fill="%s" fill-opacity="0.92" class="ink" stroke-width="1"/>\n' % (O[2], VIO))
w('      </g>\n')
w('''      <text x="30" y="120" class="sm" font-weight="700">A</text>
      <text x="158" y="238" class="sm" font-weight="700">B</text>
      <text x="284" y="120" class="sm" font-weight="700">C</text>
      <text x="60" y="258" class="xs muted">A over B</text>
      <text x="228" y="258" class="xs muted">B over C</text>
      <text x="186" y="30" class="xs muted">C over A</text>
      <text x="158" y="305" class="sm" text-anchor="middle" font-weight="700">the arrangement</text>

      <!-- the cycle, as a graph -->
      <g transform="translate(400,0)">
        <defs>
          <marker id="f1arrow" viewBox="0 0 10 10" refX="9" refY="5"
                  markerWidth="6" markerHeight="6" orient="auto-start-reverse">
            <path d="M 0 0 L 10 5 L 0 10 z" fill="var(--dia-hi)"/>
          </marker>
        </defs>
        <circle cx="105" cy="70" r="26" fill="%s" fill-opacity="0.92" class="ink" stroke-width="1"/>
        <circle cx="40" cy="185" r="26" fill="%s" fill-opacity="0.92" class="ink" stroke-width="1"/>
        <circle cx="170" cy="185" r="26" fill="%s" fill-opacity="0.92" class="ink" stroke-width="1"/>
        <text x="105" y="75" class="sm t-inv" text-anchor="middle" font-weight="700">A</text>
        <text x="40" y="190" class="sm t-inv" text-anchor="middle" font-weight="700">B</text>
        <text x="170" y="190" class="sm t-inv" text-anchor="middle" font-weight="700">C</text>
        <path d="M 88 92 L 58 163" class="hi" stroke-width="2" fill="none" marker-end="url(#f1arrow)"/>
        <path d="M 68 196 L 141 196" class="hi" stroke-width="2" fill="none" marker-end="url(#f1arrow)"/>
        <path d="M 155 161 L 122 92" class="hi" stroke-width="2" fill="none" marker-end="url(#f1arrow)"/>
        <text x="20" y="128" class="xs t-hi">in front of</text>
        <text x="105" y="228" class="xs t-hi" text-anchor="middle">in front of</text>
        <text x="168" y="128" class="xs t-hi">in front of</text>
        <text x="105" y="278" class="sm" text-anchor="middle" font-weight="700">the order it demands</text>
        <text x="105" y="298" class="xs t-bad" text-anchor="middle">a loop &mdash; no sort can satisfy it</text>
      </g>
    </svg>
    <figcaption><span class="fignum">Figure 1.</span> The painter&rsquo;s cycle. Three panels, three
      overlaps, and the &ldquo;is in front of&rdquo; relation they force is a loop rather than an
      order. This is not a contrived edge case you can hope to avoid &mdash; it is three sticks lying
      on a table. Our demo builds exactly this arrangement, and the harness in
      <code>scratch/verify_31.cpp</code> checks all three overlaps numerically. Worse: with the
      camera facing it, all three panels are the <em>same</em> average distance from the eye, so the
      sort key cannot even express a preference.</figcaption>
  </figure>
''' % (AMB, TEA, VIO))

open('docs/lessons/03-01-z-buffer.html', 'w').write(out.getvalue())
print("part 1 written:", len(out.getvalue()), "chars")
