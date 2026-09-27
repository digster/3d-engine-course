# -*- coding: utf-8 -*-
import io
out = io.StringIO(); w = out.write

def esc(s):
    return s.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')

w('''
  <!-- ================= 4. IMPLEMENTATION ================= -->
  <h2 id="implementation"><span class="num">4</span>Implementation</h2>

  <p>
    Four pieces: a new type to hold the depths, a new field on <code>vertex</code>, four lines in the
    rasterizer&rsquo;s inner loop, and a demo that can show you both algorithms side by side. We build
    them in that order.
  </p>

  <h3>4.1 Where the depth buffer lives &mdash; and where it does not</h3>

  <p>
    The obvious move is to bolt a <code>std::vector&lt;float&gt;</code> onto <code>framebuffer</code>
    and be done. Resist it, for three reasons, the third of which settles the matter.
  </p>

  <p>
    <strong>Not every framebuffer wants depth.</strong> Pong, the 2-D triangle demos, the HUD overlay,
    and every post-processing intermediate Module 6 will introduce are colour-only. Fusing the two
    types makes each of them allocate a depth buffer it never reads &mdash; 8&nbsp;MB of nothing at
    1080p.
  </p>

  <p>
    <strong>They are different formats.</strong> Colour is four 8-bit channels packed into a
    <code>Uint32</code>; depth is a single value with its own precision story. Their clear values are
    unrelated: a background colour, and <em>far</em>.
  </p>

  <p>
    <strong>The hardware keeps them separate</strong>, and this is the one that decides it. Here is
    SDL_GPU&rsquo;s render-pass entry point, from <code>SDL3/SDL_gpu.h</code>:
  </p>

  <figure class="listing">
    <figcaption>
      <span class="path">SDL3/SDL_gpu.h &mdash; the shape we are copying</span>
      <span class="lang" data-lang="cpp">C++</span>
    </figcaption>
    <pre><code class="lang-cpp">extern SDL_DECLSPEC SDL_GPURenderPass * SDLCALL SDL_BeginGPURenderPass(
    SDL_GPUCommandBuffer *command_buffer,
    const SDL_GPUColorTargetInfo *color_target_infos,
    Uint32 num_color_targets,
    const SDL_GPUDepthStencilTargetInfo *depth_stencil_target_info);   // may be NULL</code></pre>
  </figure>

  <p>
    Colour targets are an <em>array</em>; the depth-stencil target is a <em>separate, nullable
    parameter</em>. They are two independent attachments with their own load and store operations. If
    we model that split now, Module 4 is a rename rather than a redesign &mdash; and we get the right
    answer for the right reason today as well.
  </p>

  <p>So <code>depth_buffer</code> is its own type, deliberately shaped as a twin of
    <code>framebuffer</code>: same constructor arguments, same <code>index = y * width + x</code>, same
    <code>clear</code>, same clamped <code>row()</code> escape hatch for inner loops. If you can read
    one you can read the other.</p>

  <figure class="listing">
    <figcaption>
      <span class="path">src/gfx/depth_buffer.hpp &mdash; the core of it</span>
      <span class="tag new">new</span>
      <span class="lang" data-lang="cpp">C++</span>
    </figcaption>
    <pre><code class="lang-cpp">class depth_buffer
{
public:
    /// The far value. A cleared buffer holds this, so the first triangle to
    /// cover a pixel always wins.
    static constexpr float k_far = 1.0f;

    depth_buffer(int width, int height, depth_format format = depth_format::f32);

    void clear(float depth = k_far);

    [[nodiscard]] float depth_at(int x, int y) const;
    [[nodiscard]] float quantise(float depth) const;

    [[nodiscard]] float* row(int y);
    [[nodiscard]] const float* row(int y) const;
    // …
};</code></pre>
  </figure>

  <p>
    Notice what is <em>not</em> there: a <code>test_and_set</code>. It is tempting &mdash; it would name
    the algorithm in one method &mdash; and it is the wrong place for it. On real hardware the
    comparison is pipeline state, not storage:
  </p>

  <figure class="listing">
    <figcaption>
      <span class="path">SDL3/SDL_gpu.h &mdash; the depth test is pipeline state, not buffer state</span>
      <span class="lang" data-lang="cpp">C++</span>
    </figcaption>
    <pre><code class="lang-cpp">typedef struct SDL_GPUDepthStencilState
{
    SDL_GPUCompareOp compare_op;   /**&lt; The comparison operator used for depth testing. */
    // …
    bool enable_depth_test;        /**&lt; true enables the depth test. */
    bool enable_depth_write;       /**&lt; true enables depth writes. */
    // …
} SDL_GPUDepthStencilState;</code></pre>
  </figure>

  <p>
    Three independent knobs, and different passes set them differently: a shadow pass writes depth and
    no colour; a transparent pass tests depth and does not write it. Baking &ldquo;less-than, always
    write&rdquo; into the buffer would make those unexpressible. <strong>The buffer stores and clears;
    the rasterizer compares.</strong>
  </p>

  <p>
    The one thing <code>depth_buffer</code> does carry that <code>framebuffer</code> does not is a
    <strong>format</strong>, because &sect;3.6 is not an aside &mdash; it is a decision the type should
    be able to express:
  </p>

  <figure class="listing">
    <figcaption>
      <span class="path">src/gfx/depth_buffer.hpp &mdash; precision as a first-class choice</span>
      <span class="tag new">new</span>
      <span class="lang" data-lang="cpp">C++</span>
    </figcaption>
    <pre><code class="lang-cpp">enum class depth_format
{
    f32,       ///&lt; full float precision — no quantisation at all
    unorm24,   ///&lt; 2^24 - 1 evenly spaced codes in [0, 1]
    unorm16    ///&lt; 2^16 - 1 codes. Enough to see z-fighting on demand.
};

[[nodiscard]] float quantise(float depth) const
{
    if (codes_ &lt;= 0.0f) { return depth; }
    return std::round(depth * codes_) / codes_;
}</code></pre>
  </figure>

  <div class="callout note">
    <span class="label">What we model, and what we do not</span>
    <p>
      Storage stays <code>float</code> in every case; we round to the format&rsquo;s grid on write. The
      <em>behaviour</em> &mdash; which is what z-fighting is &mdash; is therefore exactly right, while
      the memory saving of a genuine 16-bit buffer is the part we are not modelling. That is the
      ninety-percent picture: the missing ten percent is bandwidth, and it starts to matter in Module 4
      where the depth attachment is a real GPU texture with a real format.
    </p>
    <p>
      One detail that is not a simplification: <code>2^24 &minus; 1 = 16777215</code> and
      <code>2^16 &minus; 1 = 65535</code> are both exactly representable in a <code>float</code>, so
      <code>round(z * codes) / codes</code> lands on a genuine grid point rather than near one.
    </p>
  </div>

  <h3>4.2 A vertex learns its depth</h3>

  <p>
    Lesson 2.4 built <code>vertex</code> and predicted its future in a comment: <em>&ldquo;Module 3
    adds depth (<code>z</code>), texture coordinates and normals as the lessons that need them arrive
    &mdash; each one three more lines in the same loop.&rdquo;</em> Time to collect on the first of
    those.
  </p>

  <figure class="listing">
    <figcaption>
      <span class="path">src/gfx/raster.hpp &mdash; the new field</span>
      <span class="tag modified">modified</span>
      <span class="lang" data-lang="cpp">C++</span>
    </figcaption>
    <pre><code class="lang-cpp">struct vertex
{
    int x = 0;                       ///&lt; pixel column
    int y = 0;                       ///&lt; pixel row
    float z = 0.0f;                  ///&lt; DEVICE depth in [0,1] — not view-space z
    Uint32 colour = 0xFFFFFFFFu;     ///&lt; ARGB8888 as stored — i.e. sRGB-encoded
};</code></pre>
  </figure>

  <p>
    Putting <code>z</code> inside <code>vertex</code> rather than passing three loose floats is the
    same argument Lesson 2.4 &sect;4.2 made about colour, and it now pays a second time.
    <code>fill_triangle</code> reorients a backwards-wound triangle by swapping two vertices; every
    attribute must move with the position it belongs to. Because depth lives in the struct, the
    existing <code>std::swap(v1, v2)</code> carries it automatically. Had we passed
    <code>z0, z1, z2</code> alongside, there would now be a fourth thing to remember to swap &mdash;
    and forgetting it produces a triangle with perfect geometry and inverted depth, which is a
    genuinely horrible bug to look at.
  </p>

  <div class="callout pitfall">
    <span class="label">The compiler catches the call sites, and that is by design</span>
    <p>
      Inserting <code>z</code> <em>before</em> <code>colour</code> breaks every brace-initialised
      <code>vertex</code> in the codebase &mdash; and breaks them <strong>loudly</strong>:
    </p>
    <pre><code>error: constant expression evaluates to 4278190335 which cannot be
       narrowed to type &#39;float&#39; [-Wc++11-narrowing]
   engine::vertex{vx[2], vy[2], engine::pack_argb(0, 0, 255)},</code></pre>
    <p>
      A <code>Uint32</code> cannot silently become a <code>float</code> in list-initialisation, so the
      old three-argument form is a compile error rather than a colour quietly landing in the depth
      field. That is aggregate initialisation earning its keep. Had <code>vertex</code> had a
      constructor taking <code>(int, int, Uint32)</code>, the same edit would have compiled and
      produced garbage.
    </p>
  </div>

  <h3>4.3 Four lines in the inner loop</h3>

  <p>
    Now the rasterizer. The depth-tested fill is not a new function &mdash; it is the Gouraud fill from
    Lesson 2.4 with an attachment slot:
  </p>

  <figure class="listing">
    <figcaption>
      <span class="path">src/gfx/raster.hpp &mdash; one rasterizer, one attachment slot</span>
      <span class="tag modified">modified</span>
      <span class="lang" data-lang="cpp">C++</span>
    </figcaption>
    <pre><code class="lang-cpp">/// @param depth  the depth attachment to test and write against, or `nullptr`.
void fill_triangle(framebuffer&amp; fb, depth_buffer* depth,
                   const vertex&amp; a, const vertex&amp; b, const vertex&amp; c,
                   blend_space space = blend_space::linear);

/// The same fill with no depth attachment — `fill_triangle(fb, nullptr, …)`.
inline void fill_triangle(framebuffer&amp; fb,
                          const vertex&amp; a, const vertex&amp; b, const vertex&amp; c,
                          blend_space space = blend_space::linear)
{
    fill_triangle(fb, nullptr, a, b, c, space);
}</code></pre>
  </figure>

  <div class="callout cpp">
    <span class="label">C++ in place &mdash; when a raw pointer is the right tool</span>
    <p>
      This course has said since Lesson 1.5 that the engine core owns nothing through raw pointers.
      That rule is about <em>ownership</em>, and this parameter owns nothing. What a raw pointer gives
      us here that a reference cannot is <strong>optionality</strong>: a reference must refer to
      something, and &ldquo;there is no depth attachment&rdquo; is a state we need to express.
    </p>
    <p>
      The alternatives were two overloads with duplicated loops (rejected &mdash; the fill&rsquo;s
      set-up is subtle, and <code>raster.cpp</code> already argues that duplicating something subtle is
      how one bias ends up wrong in one of three places), or
      <code>std::optional&lt;std::reference_wrapper&lt;depth_buffer&gt;&gt;</code> (rejected &mdash; it
      is a mouthful that compiles to the same pointer). A non-owning nullable pointer is the honest
      spelling, and it is exactly what <code>SDL_BeginGPURenderPass</code> uses for the same idea.
    </p>
  </div>

  <p>
    Inside, one hoist and one test. The depth row comes out of the loop for the same reason the colour
    row does &mdash; the row index does not change across a scanline:
  </p>

  <figure class="listing">
    <figcaption>
      <span class="path">src/gfx/raster.cpp &mdash; hoisted per scanline</span>
      <span class="tag modified">modified</span>
      <span class="lang" data-lang="cpp">C++</span>
    </figcaption>
    <pre><code class="lang-cpp">Uint32* const row = fb.row(y);
float* const zrow = (depth != nullptr) ? depth-&gt;row(y) : nullptr;</code></pre>
  </figure>

  <p>and then, per pixel, the whole of the z-buffer:</p>

  <figure class="listing">
    <figcaption>
      <span class="path">src/gfx/raster.cpp &mdash; the depth test</span>
      <span class="tag modified">modified</span>
      <span class="lang" data-lang="cpp">C++</span>
    </figcaption>
    <pre><code class="lang-cpp">bool visible = true;
if (zrow != nullptr)
{
    const float z = depth-&gt;quantise(f0 * v0.z + f1 * v1.z + f2 * v2.z);
    visible = z &lt; zrow[x];
    if (visible) { zrow[x] = z; }
}

if (visible)
{
    const rgb3 mixed{f0 * c0.r + f1 * c1.r + f2 * c2.r,
                     f0 * c0.g + f1 * c1.g + f2 * c2.g,
                     f0 * c0.b + f1 * c1.b + f2 * c2.b};
    row[x] = pixel_from(mixed, space);
}</code></pre>
  </figure>

  <p>Five things are decided in those twelve lines, and none of them is arbitrary.</p>

  <p>
    <strong>The weights are <code>f0, f1, f2</code> &mdash; the unbiased ones.</strong> They are the
    same three numbers the colour interpolation uses, computed by subtracting the top-left rule&rsquo;s
    bias back out (Lesson 2.4 &sect;3.5). Leaving the bias in would shift the entire depth field by a
    fraction of a pixel and break the sum-to-one identity. It is exactly the mistake 2.4 warned about,
    made on a new attribute, and it would be far harder to spot here: a colour field displaced by a
    twentieth of a pixel is invisible, and a depth field displaced by the same amount produces a thin
    seam of wrong occlusion where two triangles meet.
  </p>

  <p>
    <strong>We interpolate <code>v.z</code>, the device depth.</strong> Because of &sect;3.4 this is
    exact and not an approximation. Three multiply-adds &mdash; the same three lines as a colour
    channel &mdash; is genuinely all it takes, and that is the payoff for having done the derivation.
  </p>

  <p>
    <strong>Quantise, then compare.</strong> That is the order the hardware uses: the incoming
    fragment&rsquo;s depth is converted to the attachment&rsquo;s format and <em>then</em> tested
    against a value already in that format. Compare first and round afterwards and two surfaces that
    store identically would still order themselves, which is precisely the flicker we want to be able
    to reproduce on demand.
  </p>

  <p>
    <strong><code>&lt;</code>, not <code>&lt;=</code>.</strong> On an exact tie the pixel already there
    keeps it. This matters for coplanar geometry: with <code>&lt;</code> the first triangle drawn wins
    every tie and the result is <em>stable</em>; with <code>&lt;=</code> the last one does, and any
    surface that gets drawn twice flickers. It is the same choice as
    <code>SDL_GPU_COMPAREOP_LESS</code>, which is what essentially every renderer defaults to.
  </p>

  <p>
    <strong>The colour is computed after the test.</strong> A pixel that loses never pays for its
    blend. That is early-Z, in miniature &mdash; see &sect;3.7.
  </p>

  <h3>4.4 The demo: two algorithms, one scene, and a number</h3>

  <p>
    An engine that can only show you the right answer teaches half as much. The demo runs
    <em>both</em> strategies over identical geometry every frame and counts the pixels they disagree
    about.
  </p>

  <p>
    First, the whole scene is projected once into one flat list. That flatness is not incidental: a
    painter&rsquo;s algorithm has to sort <em>across</em> objects, because sorting each object&rsquo;s
    triangles separately is wrong the moment two objects overlap.
  </p>

  <figure class="listing">
    <figcaption>
      <span class="path">src/main.cpp &mdash; a triangle, ready to rasterise</span>
      <span class="tag modified">modified</span>
      <span class="lang" data-lang="cpp">C++</span>
    </figcaption>
    <pre><code class="lang-cpp">struct raster_triangle
{
    engine::vertex v[3];

    /// Average VIEW-space z of the three corners: the painter&#39;s algorithm&#39;s
    /// entire idea, and its entire problem. View z is negative in front of the
    /// camera, so a MORE negative key is further away and sorts first.
    float sort_key = 0.0f;
};</code></pre>
  </figure>

  <p>Then the two paths, which differ by two lines:</p>

  <figure class="listing">
    <figcaption>
      <span class="path">src/main.cpp &mdash; one function, two algorithms</span>
      <span class="tag modified">modified</span>
      <span class="lang" data-lang="cpp">C++</span>
    </figcaption>
    <pre><code class="lang-cpp">void draw_triangles(engine::framebuffer&amp; fb, engine::depth_buffer* depth,
                    std::vector&lt;raster_triangle&gt;&amp; tris, bool sorted)
{
    if (sorted)
    {
        std::sort(tris.begin(), tris.end(),
                  [](const raster_triangle&amp; a, const raster_triangle&amp; b)
                  { return a.sort_key &lt; b.sort_key; });
    }

    for (const raster_triangle&amp; t : tris)
    {
        engine::fill_triangle(fb, depth, t.v[0], t.v[1], t.v[2]);
    }
}</code></pre>
  </figure>

  <p>
    That is the entire difference between the two techniques, and it is worth staring at. The
    painter&rsquo;s algorithm needs a global sort of the whole scene, <span class="badge">O(n log n)</span>
    and growing, and it is still wrong. The z-buffer needs no sort, no ordering, and no knowledge of
    any other triangle, and it is right.
  </p>

  <p>
    The ground grid is drawn first, as plain lines, with no depth interaction at all. That is not
    laziness &mdash; it is the painter&rsquo;s algorithm surviving as a legitimate special case. A
    background is the one thing you always know is behind everything, so it needs no test. That is
    exactly how a skybox works, and we will use the same reasoning in Module 6. (Giving lines a real
    depth test is Exercise 3.1.2.)
  </p>

  <p>
    Finally, the measurement. After drawing the scene in the selected mode, the demo renders the
    <em>other</em> mode into a scratch framebuffer over the same background and counts differing
    pixels inside the viewport rectangle. At 320&times;180 the second pass costs microseconds, and it
    turns &ldquo;the painter&rsquo;s algorithm is wrong&rdquo; from a claim into a live number that
    moves as you orbit.
  </p>

  <h3>4.5 Building a scene that breaks it</h3>

  <p>
    The demo ships four scenes on the <kbd>C</kbd> key: the Module 2 solids, the cycle, the
    intersecting pair, and the near-coplanar pair. The cycle is the one worth reading the construction
    of, because it is Lesson 2.5&rsquo;s claim used as a tool rather than admired.
  </p>

  <figure class="listing">
    <figcaption>
      <span class="path">src/main.cpp &mdash; a plank, built from its own axes</span>
      <span class="tag modified">modified</span>
      <span class="lang" data-lang="cpp">C++</span>
    </figcaption>
    <pre><code class="lang-cpp">const engine::vec3 along = end_b - end_a;

const engine::vec3 axis_y = engine::normalised(along);               // model +y
const engine::vec3 axis_x = engine::normalised(                      // model +x
    engine::vec3{-(b.y - a.y), b.x - a.x, 0.0f});
const engine::vec3 axis_z = engine::cross(axis_x, axis_y);           // model +z

engine::transform t;
t.rotation = engine::mat3{axis_x, axis_y, axis_z};
t.scale    = {width, engine::length(along) + 2.0f * overhang, 1.0f};
t.position = (end_a + end_b) * 0.5f;</code></pre>
  </figure>

  <p>
    <strong>A rotation matrix is its three columns, and its columns are where the basis vectors
    land</strong> (Lesson 2.5 &sect;3.2). We want the quad&rsquo;s own x axis to run across the plank,
    its y axis along it, and its z axis to be the face normal &mdash; so we build those three
    directions and hand them over as columns. No angle is computed anywhere, and no
    <code>rotation_x/y/z</code> is composed. The three are orthonormal by construction:
    <code>axis_x</code> lies in the ground plane and is perpendicular to the plank&rsquo;s horizontal
    run, so it is perpendicular to <code>axis_y</code> whatever the tilt, and <code>axis_z</code> is
    their cross product. The harness checks the determinant is <code>+1.000000</code> &mdash; a
    reflection here would flip every triangle&rsquo;s winding and quietly matter in Lesson 3.4.
  </p>

  <p>
    Three planks laid along the sides of an equilateral triangle, each tilted so that it ends
    <code>+0.55</code> towards the camera at one end and <code>&minus;0.55</code> away at the other,
    give exactly the weave of Figure 1. The <code>overhang</code> lengthens each quad
    <em>without moving its plane</em>, so the depths at the two named corners are still exactly
    &plusmn;tilt &mdash; it exists only so the planks properly cross rather than merely touching, and a
    sliver of overlap is not a demonstration.
  </p>

  <div class="worked">
    <span class="label">Verified, not asserted</span>
    <p><code>scratch/verify_31_render.cpp</code> builds the same transforms, reads the plank planes
      back out of the matrices, and reports:</p>
    <pre><code>at C2: plank A z = +0.5500   plank B z = -0.5500  -&gt; A in front
at C3: plank B z = +0.5500   plank C z = -0.5500  -&gt; B in front
at C1: plank C z = +0.5500   plank A z = -0.5500  -&gt; C in front
plank A: width 1.0500, length 3.3516, axes at 90.000 deg
plank A basis determinant: +1.000000</code></pre>
    <p>
      A over B over C over A. And with the camera facing the weave, all three plank centres are
      equidistant from the eye, so their average depths are identical to the last bit &mdash; a
      renderer that sorts whole objects, as many do for transparency, has literally nothing to
      compare.
    </p>
  </div>
''')

with open('docs/lessons/03-01-z-buffer.html', 'a') as f:
    f.write(out.getvalue())
print("part 5 appended:", len(out.getvalue()), "chars")
