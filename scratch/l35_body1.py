# Lesson 3.5 body — part 1: header, objectives, TOC, the problem, intuition.
BODY1 = r"""
  <!-- =================================================================
       SECTION 1 — HEADER BLOCK
       ================================================================= -->
  <div class="lesson-head">
    <div class="eyebrow">Module 3 — Software Rasterizer II: Depth, Light, Texture · Lesson 3.5</div>
    <h1>A Hand-Rolled OBJ Loader</h1>
    <p class="deck">
      The rasterizer is finished and has nothing to draw. Today the engine reads geometry it
      did not write — and discovers that a file's idea of a vertex and the hardware's idea of
      a vertex are not the same idea.
    </p>

    <div class="meta">
      <dl>
        <dt>Time</dt>
        <dd>≈ 5–6 hours</dd>

        <dt>Prereqs</dt>
        <dd>
          <a href="03-04-back-face-culling.html">3.4 — Back-Face Culling</a>,
          <a href="02-12-wireframe-mesh.html">2.12 — A Spinning Wireframe Mesh</a>,
          <a href="../conventions.html">Conventions</a>
        </dd>

        <dt>Files</dt>
        <dd>
          <code>src/gfx/mesh.hpp</code>, <code>src/gfx/mesh.cpp</code>,
          <code>src/gfx/obj.hpp</code>, <code>src/gfx/obj.cpp</code>,
          <code>src/main.cpp</code>, <code>CMakeLists.txt</code>,
          <code>.gitignore</code>, <code>assets/*.obj</code>
        </dd>

        <dt>Milestone</dt>
        <dd>
          A 2,304-triangle torus, read off a disk, spinning under the z-buffer — and a
          validator that answers, with numbers, whether that geometry is safe to cull.
        </dd>
      </dl>
    </div>
  </div>

  <section class="objectives" aria-labelledby="obj-h">
    <h2 id="obj-h">By the end of this lesson you will be able to…</h2>
    <ul>
      <li>…read a Wavefront OBJ file and say what every line in it means.</li>
      <li>…explain why a file's vertex and a vertex buffer's vertex are different things, and
          derive the de-duplication that reconciles them.</li>
      <li>…resolve OBJ's 1-based and negative indices without an off-by-one, and say what
          each of the two mistakes looks like on screen.</li>
      <li>…triangulate an n-gon by fanning, and state precisely what the fan assumes.</li>
      <li>…measure whether geometry from disk is closed, consistently wound, and therefore
          safe to back-face cull — rather than hoping.</li>
      <li>…design an error-reporting scheme for a library that does not throw, and defend the
          line between "malformed" and merely "silly".</li>
      <li>…own heap-allocated geometry without leaking it, and explain why the view type
          you have used since Lesson 2.12 stops being enough here.</li>
    </ul>
  </section>

  <!-- ========================= TOC ========================= -->
  <details class="toc" open>
    <summary>Contents</summary>
    <ol>
      <li><a href="#problem">The Problem</a></li>
      <li><a href="#intuition">Building Intuition</a>
        <ol>
          <li><a href="#intuition-format">The whole format, in one screenful</a></li>
          <li><a href="#intuition-two">Two different things called "a vertex"</a></li>
          <li><a href="#intuition-paper">The paper-model argument</a></li>
        </ol>
      </li>
      <li><a href="#theory">The Theory</a>
        <ol>
          <li><a href="#theory-onebased">Indices start at one</a></li>
          <li><a href="#theory-negative">…and may count backwards</a></li>
          <li><a href="#theory-index">The index problem, derived</a></li>
          <li><a href="#theory-map">The de-duplicating map</a></li>
          <li><a href="#theory-fan">Triangulating a face, and what a fan assumes</a></li>
          <li><a href="#theory-euler">Euler's formula is a statement about spheres</a></li>
          <li><a href="#theory-volume">Signed volume, and "wound outward"</a></li>
          <li><a href="#theory-weld">Welding, and the ulp that hides a seam</a></li>
        </ol>
      </li>
      <li><a href="#implementation">Implementation</a>
        <ol>
          <li><a href="#impl-owning">Geometry that owns itself</a></li>
          <li><a href="#impl-text">Reading numbers out of text</a></li>
          <li><a href="#impl-corner">One face corner</a></li>
          <li><a href="#impl-unify">The unified vertex</a></li>
          <li><a href="#impl-errors">Errors without exceptions</a></li>
          <li><a href="#impl-io">Files, paths, and the build</a></li>
          <li><a href="#impl-writer">A writer, and the round trip</a></li>
          <li><a href="#impl-validate">The validator</a></li>
          <li><a href="#impl-demo">Wiring it to the demo</a></li>
        </ol>
      </li>
      <li><a href="#listings">Complete Code Listings</a></li>
      <li><a href="#build">Build &amp; Run</a></li>
      <li><a href="#pitfalls">Common Pitfalls &amp; Debugging</a></li>
      <li><a href="#exercises">Exercises</a></li>
      <li><a href="#recap">Recap &amp; Next</a></li>
      <li><a href="#reading">Further Reading</a></li>
    </ol>
  </details>

  <!-- =================================================================
       SECTION 2 — THE PROBLEM
       ================================================================= -->
  <h2 id="problem"><span class="num">1</span>The Problem</h2>

  <p>
    The software rasterizer is done. It fills triangles with a fill rule that never draws a
    pixel twice (2.2), interpolates attributes across them by barycentric weights (2.3, 2.4),
    resolves visibility per pixel with a depth buffer (3.1), keeps textures from swimming by
    dividing through <code>w</code> (3.2), cuts geometry against the near plane before the
    divide can destroy it (3.3), and throws away faces that point the wrong way (3.4). It is,
    for a CPU renderer, genuinely finished.
  </p>

  <p>
    And it has nothing to draw. Three meshes, in total, in the whole course:
  </p>

  <figure class="tbl">
    <div class="tbl-scroll">
      <table class="manifest">
        <thead><tr><th>Mesh</th><th>Vertices</th><th>Triangles</th><th>Where it came from</th></tr></thead>
        <tbody>
          <tr><td>cube</td><td>8</td><td>12</td><td>typed by hand, Lesson 2.6</td></tr>
          <tr><td>quad</td><td>4</td><td>2</td><td>typed by hand, Lesson 3.1</td></tr>
          <tr><td>icosahedron</td><td>12</td><td>20</td><td>derived from φ, Lesson 2.12</td></tr>
        </tbody>
      </table>
    </div>
    <figcaption><span class="fignum">Table 1.</span> The entire asset library, after twenty-nine lessons.</figcaption>
  </figure>

  <p>
    Look at what we <em>guaranteed</em> about that geometry by typing it. Every triangle is
    wound counter-clockwise seen from outside, because we wound it. Every surface is closed,
    because we closed it. Every face is a triangle, because we only wrote triangles. Every
    index is in range, because we counted. The whole of Module 3 rests on those four
    properties, and not one of them was ever <em>checked</em> — they were true by
    construction, which is a lovely thing to be able to say about eight vertices and a
    completely worthless thing to be able to say about a model somebody else made.
  </p>

  <div class="callout warn">
    <span class="label">The failure mode — five of them, in fact</span>
    <p>
      Download an OBJ from the internet, point a naive loader at it, and you will hit these
      in roughly this order:
    </p>
    <ul>
      <li><strong>Shattered glass.</strong> Every triangle connects corners that belong to its
        neighbours. Cause: OBJ indices start at <em>one</em>, and you read them as if they
        started at zero.</li>
      <li><strong>Smeared texture, correct silhouette.</strong> The shape is right, the surface
        detail is nonsense. Cause: a face corner names its position, its texture coordinate and
        its normal <em>independently</em>, and you used one index for all three.</li>
      <li><strong>Fins and spikes.</strong> Extra triangles reaching off the surface into space.
        Cause: the file contains quads or larger polygons and you assumed three corners.</li>
      <li><strong>Holes, but only when culling is on.</strong> Parts of a closed solid missing,
        and you can see the inside through the gap. Cause: the file's winding is not
        consistent, and Lesson 3.4's culler believes winding.</li>
      <li><strong>A crash, with no message.</strong> Cause: an index one past the end of an
        array, in a loop with no bounds check, in a build with no assertions.</li>
    </ul>
  </div>

  <p>
    Every one of those is a real bug that has shipped in real engines, and every one of them
    comes from the same root: <strong>a renderer that trusts its input.</strong> Two lessons
    of this module have already written IOUs against exactly this moment.
  </p>

  <div class="callout note">
    <span class="label">Two debts falling due</span>
    <p>
      <strong>Lesson 3.3 §3.9</strong> argued for clipping properly rather than subdividing
      until the artifact hides, and the closing argument was: <em>a loaded mesh's triangle
      count is what it is.</em> You cannot subdivide your way out of a bug in geometry you did
      not author. Today the geometry stops being ours.
    </p>
    <p>
      <strong>Lesson 3.4 §3.6</strong> made back-face culling conditional on a promise —
      <code>scene_object::closed</code>, a <code>bool</code> typed next to the mesh by the
      person who typed the mesh. Today that promise has nobody to make it. By the end of this
      lesson the flag is computed from a measurement, and <code>twisted.obj</code> exists so
      you can watch the measurement earn its keep.
    </p>
  </div>

  <p>
    So the work divides cleanly. <strong>Parsing OBJ is an afternoon</strong>, and this lesson
    will not pretend otherwise — the format is a few line types and some numbers. What
    deserves five hours is everything around it: the structural mismatch between how a file
    stores a vertex and how hardware fetches one; how to fail on bad input without exceptions
    and without lying; and how to find out whether the thing you just loaded is safe to draw.
  </p>

  <p>
    That last one is the part most tutorials skip, and it is the part that separates a loader
    from an <em>asset pipeline</em>. A pipeline is not a parser. It is the place where
    untrusted data becomes trusted data, and something has to do the trusting on purpose.
  </p>

  <!-- =================================================================
       SECTION 3 — BUILDING INTUITION
       ================================================================= -->
  <h2 id="intuition"><span class="num">2</span>Building Intuition</h2>

  <h3 id="intuition-format">2.1 The whole format, in one screenful</h3>

  <p>
    Wavefront OBJ came out of Wavefront Technologies' Advanced Visualizer in the 1980s. It is
    plain ASCII, one statement per line, keyword first. It has no header, no version number,
    no length fields, and no way to say how big anything is — you find out by reading to the
    end. That sounds like a criticism and mostly is not: it is why the format is still
    everywhere forty years later, and why you can open one in a text editor and understand it.
  </p>

  <p>
    Here is a complete, valid model. It is the file we ship as
    <code>assets/cube.obj</code>, with the comments stripped:
  </p>

  <figure class="listing">
    <figcaption>
      <span class="path">the whole format that matters</span>
      <span class="lang" data-lang="cpp">OBJ</span>
    </figcaption>
    <pre><code class="lang-cpp"># a comment runs to the end of the line
o cube                     # an object name. We skip it.

v -0.5 -0.5 -0.5           # a POSITION.  These are numbered 1, 2, 3, … in order.
v  0.5 -0.5 -0.5
v  0.5  0.5 -0.5
                           # …six more…

vt 0.0 0.0                 # a TEXTURE COORDINATE. Numbered 1, 2, 3, … SEPARATELY.
vt 1.0 0.0

vn  0.0  0.0 -1.0          # a NORMAL. Numbered 1, 2, 3, … separately AGAIN.

usemtl none                # a material reference. We skip it.
f 1/1/1 4/4/1 3/3/1 2/2/1  # a FACE: four corners, each naming position/uv/normal.
</code></pre>
  </figure>

  <p>
    That is essentially all of it. There are other statements — <code>l</code> for polylines,
    <code>p</code> for points, <code>s</code> for smoothing groups, <code>g</code> for groups,
    <code>mtllib</code> and <code>usemtl</code> for materials, and a whole free-form-geometry
    sub-language nobody has shipped since the 1990s — but a mesh is
    <code>v</code>, <code>vt</code>, <code>vn</code>, and <code>f</code>.
  </p>

  <p>
    Read that last line again, because it is the entire lesson:
  </p>

  <div class="callout note">
    <span class="label">The sentence this lesson is about</span>
    <p>
      <code>f 1/1/1 4/4/1 3/3/1 2/2/1</code> — a face with four corners, and
      <strong>each corner names its position, its texture coordinate and its normal with
      three separate numbers.</strong> Corner one takes position 1, uv 1 and normal 1. It
      could just as legally have taken position 1, uv 7 and normal 3.
    </p>
  </div>

  <p>
    Figure 1 draws what that means. The file is not one list of vertices. It is
    <strong>three independent lists</strong>, plus a fourth list of faces whose corners reach
    into the first three separately.
  </p>

  <!-- ---- FIGURE 1 ---- -->
  <figure class="dia bleed">
    <svg viewBox="0 0 700 340" role="img" aria-labelledby="fig1-t fig1-d">
      <title id="fig1-t">An OBJ file is three independent index streams plus a face list</title>
      <desc id="fig1-d">Three vertical lists labelled positions, texture coordinates and
        normals, each numbered from one. Below them a face statement with three corners; the
        first corner's three numbers are drawn as three separate arrows reaching up into the
        three different lists. The lists have different lengths, emphasised by a note reading
        "the three lists have nothing to do with each other, not even their lengths".</desc>

      <defs>
        <marker id="f1-ah" viewBox="0 0 10 10" refX="9" refY="5"
                markerWidth="7" markerHeight="7" orient="auto-start-reverse">
          <path d="M0,0 L10,5 L0,10 z" fill="var(--dia-hi)"/>
        </marker>
      </defs>

      <!-- three columns. The counts live in the headings, so the space below the
           boxes stays clear for the three lookup arrows. -->
      <g>
        <rect x="40" y="30" width="150" height="150" rx="6" class="fill-soft ink-soft" stroke-width="1.2"/>
        <text x="115" y="22" class="sm" text-anchor="middle" font-weight="700">v — positions · 8</text>
        <text x="52" y="52" class="mono sm">1  -0.5 -0.5 -0.5</text>
        <text x="52" y="72" class="mono sm">2   0.5 -0.5 -0.5</text>
        <text x="52" y="92" class="mono sm">3   0.5  0.5 -0.5</text>
        <text x="52" y="112" class="mono sm">4  -0.5  0.5 -0.5</text>
        <text x="52" y="132" class="mono sm muted">…</text>
        <text x="52" y="158" class="mono sm">8  -0.5  0.5  0.5</text>
      </g>

      <g>
        <rect x="230" y="30" width="130" height="150" rx="6" class="fill-soft ink-soft" stroke-width="1.2"/>
        <text x="295" y="22" class="sm" text-anchor="middle" font-weight="700">vt — uvs · 4</text>
        <text x="242" y="52" class="mono sm">1   0.0  0.0</text>
        <text x="242" y="72" class="mono sm">2   1.0  0.0</text>
        <text x="242" y="92" class="mono sm">3   1.0  1.0</text>
        <text x="242" y="112" class="mono sm">4   0.0  1.0</text>
      </g>

      <g>
        <rect x="400" y="30" width="150" height="150" rx="6" class="fill-soft ink-soft" stroke-width="1.2"/>
        <text x="475" y="22" class="sm" text-anchor="middle" font-weight="700">vn — normals · 6</text>
        <text x="412" y="52" class="mono sm">1   0  0 -1</text>
        <text x="412" y="72" class="mono sm">2   0  0  1</text>
        <text x="412" y="92" class="mono sm">3  -1  0  0</text>
        <text x="412" y="112" class="mono sm">4   1  0  0</text>
        <text x="412" y="132" class="mono sm">5   0  1  0</text>
        <text x="412" y="152" class="mono sm">6   0 -1  0</text>
      </g>

      <!-- the face statement -->
      <rect x="150" y="252" width="330" height="40" rx="6" class="fill-soft hi" stroke-width="1.6"/>
      <text x="164" y="278" class="mono">f  1/1/1   4/4/1   3/3/1   2/2/1</text>
      <text x="315" y="312" class="sm muted" text-anchor="middle">one face, four corners</text>

      <!-- three arrows out of the FIRST corner -->
      <g class="hi" stroke-width="1.8" fill="none" marker-end="url(#f1-ah)">
        <path d="M188,250 C180,225 140,215 122,185"/>
        <path d="M196,250 C210,220 250,210 268,185"/>
        <path d="M204,250 C260,230 400,215 432,185"/>
      </g>

      <text x="600" y="248" class="sm" font-weight="700">one corner,</text>
      <text x="600" y="264" class="sm" font-weight="700">three lookups</text>
      <text x="600" y="286" class="xs muted">the three lists share</text>
      <text x="600" y="300" class="xs muted">nothing — not even</text>
      <text x="600" y="314" class="xs muted">their lengths</text>
    </svg>
    <figcaption>
      <span class="fignum">Figure 1.</span>
      An OBJ file's four streams. Positions, texture coordinates and normals are numbered
      independently from 1, and a single face corner — here <code>1/1/1</code> — performs
      three separate lookups. Their lengths need not match and usually do not: this file has
      8, 4 and 6.
    </figcaption>
  </figure>

  <h3 id="intuition-two">2.2 Two different things called "a vertex"</h3>

  <p>
    Now hold Figure 1 next to the mesh type you have been using since Lesson 2.12:
  </p>

  <figure class="listing">
    <figcaption>
      <span class="path">src/gfx/mesh.hpp — as of Lesson 3.2</span>
      <span class="lang" data-lang="cpp">C++</span>
    </figcaption>
    <pre><code class="lang-cpp">struct mesh
{
    std::span&lt;const vec3&gt; vertices;             // positions
    std::span&lt;const std::uint16_t&gt; indices;     // triples; each triple is a triangle
    std::span&lt;const vec2&gt; uvs;                  // one per position, or empty
};
</code></pre>
  </figure>

  <p>
    One index array. <code>indices[k] = 7</code> selects <code>vertices[7]</code> <em>and</em>
    <code>uvs[7]</code>, together, as a unit. There is no way to say "position 7 with uv 3",
    and this is not an oversight in our design — it is the shape the hardware requires and
    Module 4 will make unavoidable. A GPU's vertex fetch reads one index, computes one offset,
    and pulls one vertex's worth of bytes out of a buffer. One index in, one vertex out.
  </p>

  <p>
    So the word "vertex" is doing two jobs, and they are not the same job:
  </p>

  <figure class="tbl">
    <div class="tbl-scroll">
      <table class="manifest">
        <thead><tr><th></th><th>In the file</th><th>In a vertex buffer</th></tr></thead>
        <tbody>
          <tr>
            <td>a vertex is…</td>
            <td>a <em>position</em>, one entry in the <code>v</code> list</td>
            <td>a <em>bundle</em> — position and uv and normal, fetched together</td>
          </tr>
          <tr>
            <td>a face corner names…</td>
            <td>three things, independently</td>
            <td>one thing, once</td>
          </tr>
          <tr>
            <td>sharing happens when…</td>
            <td>two corners name the same <code>v</code></td>
            <td>two corners agree about <em>everything</em></td>
          </tr>
        </tbody>
      </table>
    </div>
    <figcaption><span class="fignum">Table 2.</span> The mismatch, stated plainly. Reconciling these two is what the loader is <em>for</em>.</figcaption>
  </figure>

  <p>
    That third row is the crux. In the file, two faces meeting along an edge share the
    positions at that edge, full stop. In a vertex buffer they share a vertex only if they
    also agree about its texture coordinate <em>and</em> its normal. When they disagree — and
    on a cube they always disagree about the normal — the position has to be stored twice, and
    the two copies get different indices.
  </p>

  <h3 id="intuition-paper">2.3 The paper-model argument</h3>

  <p>
    Here is the picture that makes it obvious, and it involves no mathematics at all.
  </p>

  <p>
    Build a cube out of paper. You cut a flat net — six squares joined along their edges —
    fold it up, and glue. Now go to one corner of the finished cube and count. How many
    <em>points of space</em> are there? One. How many <em>corners of paper</em> meet there?
    Three, one from each of the three faces. And each of those three pieces of paper is facing
    a different way.
  </p>

  <p>
    A position is a point of space. A vertex, in the buffer sense, is a corner of paper. The
    cube has 8 of the first and 24 of the second, and Figure 2 is that count.
  </p>

  <!-- ---- FIGURE 2 ---- -->
  <figure class="dia bleed">
    <svg viewBox="0 0 700 320" role="img" aria-labelledby="fig2-t fig2-d">
      <title id="fig2-t">One corner of a cube: one position, three normals, three vertices</title>
      <desc id="fig2-d">On the left, a cube drawn in outline with the corner on its front
        left edge highlighted, and three arrows leaving that corner outward — left in red for
        the minus x face, up in green for the plus y face, and down-left in blue for the plus
        z face — each labelled as the normal of one of the three faces meeting there. On the
        right, the same corner drawn exploded into three separate paper squares pulled apart,
        each carrying one of those arrows and its own vertex number, showing that the single
        point becomes three vertices. A caption line reads 8 positions times 3 faces per
        corner equals 24 vertices.</desc>

      <defs>
        <marker id="f2-x" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
          <path d="M0,0 L10,5 L0,10 z" fill="var(--axis-x)"/></marker>
        <marker id="f2-y" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
          <path d="M0,0 L10,5 L0,10 z" fill="var(--axis-y)"/></marker>
        <marker id="f2-z" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
          <path d="M0,0 L10,5 L0,10 z" fill="var(--axis-z)"/></marker>
      </defs>

      <!-- LEFT: the cube -->
      <g transform="translate(30,20)">
        <text x="135" y="10" class="sm" text-anchor="middle" font-weight="700">on the solid</text>
        <text x="135" y="28" class="xs muted" text-anchor="middle">one point of space, three faces meeting</text>
        <!-- Cube in oblique projection, with the highlighted corner on the FRONT
             LEFT edge. That choice is not cosmetic: from a corner in the middle of
             the silhouette, the toward-the-viewer normal would be drawn crossing
             the front face and would read as pointing INTO the solid — the exact
             opposite of what this figure is about. From here all three leave. -->
        <g class="ink" fill="none" stroke-width="1.6">
          <path d="M110,110 L220,110 L220,215 L110,215 Z"/>
          <path d="M110,110 L145,82 L255,82 L220,110"/>
          <path d="M220,110 L255,82 L255,187 L220,215"/>
        </g>
        <g class="ink-soft" fill="none" stroke-width="1" stroke-dasharray="3 3">
          <path d="M110,215 L145,187 L255,187 M145,187 L145,82"/>
        </g>
        <!-- the highlighted corner: the subtitle above names it, so no label
             needs to sit in the crowded space where three edges meet -->
        <circle cx="110" cy="110" r="5.5" fill="var(--dia-hi)"/>
        <!-- three normals, all leaving the solid -->
        <g stroke-width="2.2" fill="none">
          <line class="ax-x" x1="110" y1="110" x2="40" y2="110" marker-end="url(#f2-x)"/>
          <line class="ax-y" x1="110" y1="110" x2="110" y2="50" marker-end="url(#f2-y)"/>
          <line class="ax-z" x1="110" y1="110" x2="60" y2="155" marker-end="url(#f2-z)"/>
        </g>
        <text x="34" y="102" class="xs lbl-x" text-anchor="end">−x face</text>
        <text x="116" y="54" class="xs lbl-y">+y face</text>
        <text x="54" y="172" class="xs lbl-z" text-anchor="end">+z face</text>
      </g>

      <!-- divider -->
      <line x1="352" y1="40" x2="352" y2="250" class="ink-soft" stroke-width="1" stroke-dasharray="4 4"/>

      <!-- RIGHT: exploded -->
      <g transform="translate(390,20)">
        <text x="130" y="10" class="sm" text-anchor="middle" font-weight="700">in the vertex buffer</text>
        <text x="130" y="28" class="xs muted" text-anchor="middle">three corners of paper, pulled apart</text>
        <!-- three squares pulled apart -->
        <g class="ink" fill="none" stroke-width="1.5">
          <path d="M120,60 L190,60 L190,110 L120,110 Z"/>
          <path d="M55,130 L125,130 L125,180 L55,180 Z"/>
          <path d="M150,150 L220,150 L220,200 L150,200 Z"/>
        </g>
        <g stroke-width="2" fill="none">
          <line class="ax-y" x1="155" y1="58" x2="155" y2="36" marker-end="url(#f2-y)"/>
          <line class="ax-z" x1="88" y1="182" x2="52" y2="214" marker-end="url(#f2-z)"/>
          <line class="ax-x" x1="222" y1="175" x2="258" y2="175" marker-end="url(#f2-x)"/>
        </g>
        <text x="128" y="98" class="mono xs">v17</text>
        <text x="63" y="148" class="mono xs">v03</text>
        <text x="158" y="168" class="mono xs">v22</text>
        <text x="130" y="232" class="sm t-hi" text-anchor="middle" font-weight="700">three vertices</text>
        <text x="130" y="250" class="xs muted" text-anchor="middle">same position, three normals</text>
      </g>

      <text x="350" y="305" class="sm" text-anchor="middle" font-weight="700">8 positions × 3 faces per corner = 24 vertices</text>
    </svg>
    <figcaption>
      <span class="fignum">Figure 2.</span>
      Why a cube has 24 vertices. One <em>point of space</em> where three faces meet is three
      <em>corners of paper</em>, and the three disagree about which way the surface is facing.
      A vertex buffer cannot serve three answers from one slot, so the position is stored
      three times. This is not waste to be optimised away — it is the correct encoding of a
      faceted surface.
    </figcaption>
  </figure>

  <p>
    Two consequences are worth absorbing before any code. First: <strong>a smooth surface does
    not pay this cost.</strong> On a sphere, each corner has one normal — the surface does not
    crease there — so its positions and vertices coincide. The splitting is caused by
    <em>disagreement</em>, not by geometry. Second: <strong>you cannot decide how many vertices
    a mesh has until you have read its faces.</strong> The <code>v</code> count is a lower
    bound and nothing more, which is why the loader below cannot pre-size its output array.
  </p>
"""
