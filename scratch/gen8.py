# -*- coding: utf-8 -*-
import io
out = io.StringIO(); w = out.write

w('''
  <!-- ================= 8. EXERCISES ================= -->
  <h2 id="exercises"><span class="num">8</span>Exercises</h2>

  <div class="exercise">
    <h3><span class="xnum">3.1.1</span>Break it on purpose<span class="difficulty">warm-up</span></h3>
    <p>
      Before you run it, write down what you expect. Then change the depth clear from
      <code>k_far</code> to <code>0.0f</code> and run. Then change the test from <code>&lt;</code> to
      <code>&gt;</code> (leaving the clear at <code>k_far</code>) and run again. Then set the clear to
      <code>0.5f</code>.
    </p>
    <details>
      <summary>Hint</summary>
      <p>
        For each case, ask one question: what does the buffer <em>claim</em> is already there, and can
        an incoming fragment beat it? That single question predicts all three.
      </p>
    </details>
    <details>
      <summary>Solution</summary>
      <p>
        <strong>Clear to 0:</strong> nothing draws. Zero is the near plane, so the buffer claims the
        whole screen is already occupied by the closest possible surface and every fragment loses.
      </p>
      <p>
        <strong>Test with <code>&gt;</code>:</strong> the scene turns inside out &mdash; you see the far
        side of every solid, because now the <em>furthest</em> surface wins each pixel. This is a
        genuinely useful mode, by the way: it is how you render the back faces of a volume for
        volumetric effects.
      </p>
      <p>
        <strong>Clear to 0.5:</strong> everything nearer than the halfway depth draws normally and
        everything beyond it is cut away, as though a sheet of glass were sitting in the scene. Given
        &sect;3.6 you can even say where it is: <code>w = B / (0.5 + A)</code>, which for our
        <code>near = 0.3, far = 100</code> puts the sheet about 0.6 units from the eye &mdash; so in
        practice nearly the whole scene vanishes. That is the crowding of Figure 5, felt rather than
        read.
      </p>
    </details>
  </div>

  <div class="exercise">
    <h3><span class="xnum">3.1.2</span>A depth-tested line<span class="difficulty">medium</span></h3>
    <p>
      The ground grid is drawn first and never depth-tested, so it is always behind everything. Give
      <code>draw_line</code> a depth-aware sibling that takes a start and end depth, interpolates
      between them along the line, and tests each pixel. Then draw the grid <em>after</em> the solids
      and confirm it is occluded correctly &mdash; and that grid lines in front of an object now draw
      over it.
    </p>
    <details>
      <summary>Hint</summary>
      <p>
        Bresenham already walks the major axis one pixel at a time, so you know how many steps there
        will be before you start: <code>steps = max(|dx|, |dy|)</code>. Interpolate the depth with a
        running add of <code>(z1 - z0) / steps</code>. Is that interpolation exact? Think about
        &sect;3.4: a line is a degenerate triangle, and the same argument applies &mdash; device depth
        is affine along it in screen space.
      </p>
    </details>
    <details>
      <summary>Solution sketch</summary>
      <p>
        Yes, it is exact, for the same reason and with the same proof. The interesting part is what you
        see afterwards: hairline gaps where a line meets a surface it lies exactly on, because the line
        rasterizer and the triangle rasterizer disagree about which pixel centre a shared edge belongs
        to. That is the same class of problem as shadow-map acne, and the same family of fixes applies
        &mdash; a small depth bias on the line, or drawing lines with <code>&lt;=</code>. Which is
        better, and why? There is no clean answer, which is itself worth knowing before Module 6 asks
        the question in earnest.
      </p>
    </details>
  </div>

  <div class="exercise">
    <h3><span class="xnum">3.1.3</span>Measure early-Z<span class="difficulty">medium</span></h3>
    <p>
      The z-buffer needs no order, but order still affects <em>cost</em>. Add a counter to
      <code>fill_triangle</code> for pixels that fail the depth test, and report it in the HUD. Then
      draw the scene sorted <strong>front to back</strong> (reverse the painter&rsquo;s comparator) and
      compare the counts against back-to-front and against unsorted.
    </p>
    <details>
      <summary>Hint</summary>
      <p>
        A file-scope counter reset each frame is fine for a measurement; do not ship it. Think about
        what you expect first: which order lets the buffer reject the most work, and why?
      </p>
    </details>
    <details>
      <summary>Solution sketch</summary>
      <p>
        Front-to-back rejects the most: the nearest surface arrives first and everything behind it
        fails immediately. Back-to-front rejects almost nothing &mdash; every fragment wins, so every
        one is shaded, and the buffer is written over and over. The images are identical; only the work
        differs. On our flat-coloured demo you will struggle to see this in the frame time, and that is
        an honest result worth recording: the saving scales with how expensive shading is, and shading
        is currently free. Come back to this exercise after Module 6 and the numbers will be dramatic.
        This is exactly why real engines sort roughly front-to-back for opaque geometry even though the
        z-buffer does not require any order at all.
      </p>
    </details>
  </div>

  <div class="exercise">
    <h3><span class="xnum">3.1.4</span>Reversed-Z<span class="difficulty">hard</span></h3>
    <p>
      Map the <em>near</em> plane to depth 1 and the <em>far</em> plane to depth 0, clear to
      <code>0.0f</code>, and test with <code>&gt;</code>. Then measure the near-coplanar scene at
      <code>D32_FLOAT</code> and at <code>D16_UNORM</code>, both ways round. Predict the two results
      before you measure.
    </p>
    <details>
      <summary>Hint</summary>
      <p>
        You do not need a new matrix function. Look at how <code>perspective()</code> derives
        <code>A</code> and <code>B</code> from <code>near</code> and <code>far</code>, and work out what
        happens if you pass those two arguments the other way round. Then check the two anchors by
        hand, as &sect;3.5 does.
      </p>
      <p>
        For the prediction: <code>unorm</code> codes are evenly spaced in <code>[0,1]</code>. Are
        <code>float</code> values?
      </p>
    </details>
    <details>
      <summary>Solution</summary>
      <p>
        <code>perspective(fovy, aspect, far, near)</code> &mdash; the two swapped &mdash; gives exactly
        <code>A&prime; = near/(far &minus; near)</code> and <code>B&prime; = far&middot;near/(far &minus;
        near)</code>, which map <code>z_v = &minus;near</code> to 1 and <code>z_v = &minus;far</code> to
        0. Check both by hand before running anything.
      </p>
      <p>
        <strong>With D16_UNORM, nothing changes.</strong> Unorm codes are evenly spaced, so reversing
        the mapping just relabels them; the precision at any given distance is identical. If you
        expected an improvement here, that is the misconception this exercise exists to remove.
      </p>
      <p>
        <strong>With D32_FLOAT, the improvement is enormous.</strong> Floating point has its own
        precision distribution, crowded near zero: values just above 0 are spaced roughly
        <code>10<sup>&minus;38</sup></code> apart while values just below 1 are spaced
        <code>6 &times; 10<sup>&minus;8</sup></code> apart. Standard-Z puts the distant geometry &mdash;
        where <code>1/w</code> crowding has already stolen your precision &mdash; up near 1, where float
        is at its coarsest, so the two effects <em>compound</em>. Reversed-Z puts it near 0 where float
        is at its finest, and the two effects very nearly <em>cancel</em>. This is why reversed-Z with a
        float depth buffer is close to standard practice in modern engines: it is almost free and it
        very nearly removes depth precision as a thing you have to think about.
      </p>
      <p>
        Two things to watch: the clear value and the compare op must change together (clearing to 0 and
        testing with <code>&lt;</code> reproduces Exercise 3.1.1&rsquo;s black screen), and in Module 4
        this becomes two fields &mdash; <code>clear_depth</code> on
        <code>SDL_GPUDepthStencilTargetInfo</code>, and <code>compare_op</code> on
        <code>SDL_GPUDepthStencilState</code>.
      </p>
    </details>
  </div>

  <div class="exercise">
    <h3><span class="xnum">3.1.5</span>Where the painter&rsquo;s algorithm still lives<span class="difficulty">open</span></h3>
    <p>
      We have spent a lesson demolishing sorting. Now find the case where it is still mandatory. Add a
      partially transparent surface to the demo &mdash; blend the incoming colour with what is already
      in the framebuffer instead of replacing it &mdash; and try to make it look right using only the
      z-buffer. Then work out what you actually have to do.
    </p>
    <details>
      <summary>Hint</summary>
      <p>
        The depth test answers &ldquo;which one do I keep?&rdquo;. Blending needs &ldquo;what was
        behind this, and in what order did the layers stack?&rdquo;. Ask whether one float per pixel
        can answer the second question. Then ask what happens if the transparent surface writes depth,
        and what happens if it does not.
      </p>
    </details>
    <details>
      <summary>Solution sketch</summary>
      <p>
        It cannot be done with the depth test alone, and the reason is precise: blending is not
        commutative, so the result depends on the order the layers arrive, and the z-buffer has thrown
        that order away by design. The standard answer is a hybrid every engine uses:
      </p>
      <ul>
        <li>draw all <strong>opaque</strong> geometry first, z-buffered, in any order (roughly
          front-to-back, for the early-Z reason in 3.1.3);</li>
        <li>then draw <strong>transparent</strong> geometry <em>sorted back to front</em>, with the
          depth test still <strong>on</strong> (so it is correctly hidden by opaque things) but depth
          writes <strong>off</strong> (so transparent surfaces do not occlude each other). That is
          <code>enable_depth_test = true, enable_depth_write = false</code>, and it is exactly why
          SDL_GPU exposes those as two separate flags.</li>
      </ul>
      <p>
        So the painter&rsquo;s algorithm is not dead &mdash; it has been demoted to the one job the
        z-buffer structurally cannot do, and it drags all three of &sect;1.3&rsquo;s failures along with
        it. Intersecting transparent surfaces still look wrong in shipped games today; if you have ever
        seen two smoke particles pop as the camera turns, you have watched a cycle being re-sorted.
        Order-independent transparency is an active research area precisely because this is unsolved.
        A good answer to this exercise ends with a paragraph on what you would try &mdash; per-triangle
        sorting, splitting, depth peeling, weighted-blended OIT &mdash; and what each costs.
      </p>
    </details>
  </div>

  <!-- ================= 9. RECAP & NEXT ================= -->
  <h2 id="recap"><span class="num">9</span>Recap &amp; Next</h2>

  <div class="recap">
    <h2>What the engine can do now that it could not before</h2>
    <ul>
      <li>
        <strong>Hidden-surface removal, correctly, for any geometry.</strong> Intersecting triangles,
        cyclic overlaps and triangles stretched in depth all resolve without being special-cased,
        because visibility is decided per pixel rather than per object.
      </li>
      <li>
        <strong>Filled 3-D.</strong> <code>fill_triangle</code> has existed since Lesson 2.2 and had
        never met the coordinate pipeline. It has now: the milestone mesh is opaque.
      </li>
      <li>
        <strong>Order independence.</strong> Triangles can be submitted in any order, which is what
        makes the same algorithm work on a GPU in Module 4 and across threads in Module 8.
      </li>
      <li>
        <strong>A depth attachment shaped like the hardware&rsquo;s.</strong> Separate from the colour
        buffer, with its own format and clear value, and a comparison that lives in the rasterizer
        rather than in the storage &mdash; the same split SDL_GPU makes.
      </li>
      <li>
        <strong>A precision budget you can compute.</strong> <code>&Delta;w = &Delta;z &middot;
        w&sup2; &middot; (1/near &minus; 1/far)</code> predicts z-fighting in real units before you see
        it, and says which knob to turn.
      </li>
    </ul>
    <h2>And one thing worth taking beyond this engine</h2>
    <ul>
      <li>
        <strong>Device depth is affine in screen space; view depth is not.</strong> That derivation is
        not a detail of our rasterizer &mdash; it is the reason the GPU interpolates depth for free and
        the reason every other vertex attribute needs the correction Lesson 3.2 is about. The bracket
        it produced &mdash; <em>1/w varies affinely across the screen</em> &mdash; is one of the two or
        three most useful facts in real-time rendering.
      </li>
    </ul>
  </div>

  <p>
    <strong>Next: Lesson 3.2 &mdash; Perspective-Correct Interpolation.</strong> Section 3.4 left a
    loose end lying in plain sight. Depth interpolates exactly because the projection had already
    converted it into a screen-affine quantity. <em>Nothing else a vertex carries has had that done to
    it.</em> Texture coordinates, vertex colours and normals are all linear along the surface, and the
    surface is <code>1/w</code>-warped on the screen &mdash; so interpolating them the way we have been
    is the right-hand plot of Figure 4, applied to a checkerboard. In 3.2 we put a texture on a floor,
    watch it swim and buckle exactly where the theory says it must, and then derive the fix from the
    one bracket this lesson already proved: divide the attributes by <em>w</em>, interpolate those, and
    divide back at the end.
  </p>

  <!-- ================= 10. FURTHER READING ================= -->
  <h2 id="reading"><span class="num">10</span>Further Reading</h2>

  <div class="reading">
    <ul>
      <li>
        <strong>Edwin Catmull, <em>A Subdivision Algorithm for Computer Display of Curved
        Surfaces</em></strong> (PhD thesis, University of Utah, 1974). Where the z-buffer is first
        described. Worth reading for the tone as much as the content: the memory cost is treated as the
        idea&rsquo;s central weakness. Wolfgang Stra&szlig;er&rsquo;s 1974 thesis reached the same place
        independently.
      </li>
      <li>
        <strong>Newell, Newell &amp; Sancha, &ldquo;A solution to the hidden surface problem&rdquo;</strong>
        (ACM National Conference, 1972). The painter&rsquo;s algorithm done as well as it can be done,
        cycle detection and polygon splitting included. Reading it is the fastest way to appreciate how
        much complexity the z-buffer deleted.
      </li>
      <li>
        <strong>Akenine-M&ouml;ller, Haines, Hoffman et al., <em>Real-Time Rendering</em>, 4th
        edition.</strong> &sect;2.5.2 for the z-buffer in the pipeline; &sect;23.7 for how the hardware
        actually implements it, including depth compression and hierarchical-Z. If you buy one book
        for this course, this is it.
      </li>
      <li>
        <strong>Nathan Reed, &ldquo;Depth Precision Visualized&rdquo;</strong> (2015). The clearest
        treatment anywhere of &sect;3.6 and of reversed-Z, with the graphs this lesson&rsquo;s Figure 5
        is a simplified cousin of. Read it before Exercise 3.1.4.
      </li>
      <li>
        <strong>Scratchapixel, &ldquo;Rasterization: a Practical Implementation&rdquo;.</strong>
        Covers the depth buffer and perspective-correct interpolation together, from the same
        first-principles angle as this course.
      </li>
      <li>
        <strong>Fuchs, Kedem &amp; Naylor, &ldquo;On Visible Surface Generation by A Priori Tree
        Structures&rdquo;</strong> (SIGGRAPH 1980), and Fabien Sanglard&rsquo;s <em>Game Engine Black
        Book: DOOM</em>. BSP trees are the painter&rsquo;s algorithm taken to its logical conclusion:
        pre-split the world so that a correct back-to-front order always exists. It is what you build
        when you cannot afford a depth buffer, and in 1993 nobody could.
      </li>
      <li>
        <strong>SDL3 wiki:</strong> <code>SDL_GPUDepthStencilState</code>,
        <code>SDL_GPUDepthStencilTargetInfo</code>, <code>SDL_BeginGPURenderPass</code>,
        <code>SDL_GPUTextureFormat</code>. Every design decision in <code>depth_buffer.hpp</code> has a
        counterpart in one of those four pages; Module 4 will use them for real.
      </li>
    </ul>
  </div>

  <details class="state">
    <summary>STATE</summary>
    <pre>course: Build a Professional 3D Game Engine (SDL3 + C++20)
version: Module 3, Lesson 3.1 complete

conventions:
  right-handed world, +Y up, camera looks down -Z
  column-major matrices, written row-by-row, applied as M * v
  clip/NDC: SDL_GPU — x,y in [-1,1] with +Y UP; z in [0,1], 0 = near
  DEPTH: smaller is nearer; clear to 1.0 (far); compare with &lt;
  viewport: +Y DOWN; the flip lives in viewport::to_screen
  winding: counter-clockwise = front face (stated in NDC)
  units: 1 unit = 1 metre; angles in radians internally
  naming: snake_case; types, functions and files alike
  axis colours: x/y/z = red/green/blue

completed:
  MODULE 0 (0.1-0.6) · MODULE 1 (1.1-1.8) · MODULE 2 COMPLETE (2.1-2.12)
  MODULE 3 (in progress)
  - 3.1 The Painter&#39;s Problem and the Z-Buffer

capabilities:
  - window, event loop, input state, fixed timestep with interpolation
  - CPU framebuffer presented through an SDL streaming texture
  - lines (Bresenham), triangles (edge functions), barycentric interpolation
  - vec2/3/4, mat2/3/4 (look_at, perspective), transform, viewport
  - mesh (indexed geometry: cube, quad, icosahedron)
  - THE COMPLETE GEOMETRY PIPELINE: model -&gt; world -&gt; view -&gt; clip -&gt; NDC -&gt; screen
  - DEPTH BUFFER: per-pixel hidden-surface removal, D32/D24/D16 precision,
    depth-tested filled triangles, painter-vs-z-buffer comparison in the demo

files:
  src/: main.cpp
  src/core/: input.hpp/.cpp, clock.hpp/.cpp, fixed_step.hpp/.cpp
  src/gfx/: colour.hpp/.cpp, depth_buffer.hpp/.cpp, framebuffer.hpp/.cpp,
            raster.hpp/.cpp, viewport.hpp, mesh.hpp
  src/math/: vec2.hpp, vec3.hpp, vec4.hpp, mat2.hpp, mat3.hpp, mat4.hpp, transform.hpp
  src/game/: pong.hpp/.cpp
  docs/: index.html, conventions.html, math-toolbox.html, cpp-style.html
  docs/lessons/: 00-01 .. 00-06, 01-01 .. 01-08, 02-01 .. 02-12, 03-01

next: 3.2 — Perspective-Correct Interpolation</pre>
  </details>

  <nav class="lesson-nav" aria-label="Lesson navigation (bottom)">
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

  <footer class="foot">
    <p>
      Build a Professional 3D Game Engine &middot;
      <a href="../index.html">Contents</a> &middot;
      <a href="../conventions.html">Conventions</a> &middot;
      <a href="../math-toolbox.html">Math Toolbox</a> &middot;
      <a href="../cpp-style.html">C++ Style</a>
    </p>
    <p>MIT licensed. Copyright &copy; 2026 digster.</p>
  </footer>
</div>

<!-- ==========================================================================
     PAGE SCRIPTS
     ========================================================================== -->
<!-- SHARED-SCRIPT:BEGIN -->
<!-- SHARED-SCRIPT:END -->
</body>
</html>
''')

with open('docs/lessons/03-01-z-buffer.html', 'a') as f:
    f.write(out.getvalue())
print("part 8 appended:", len(out.getvalue()), "chars")
