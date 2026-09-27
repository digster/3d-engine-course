# -*- coding: utf-8 -*-
import io
out = io.StringIO(); w = out.write

def esc(s):
    return s.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')

def listing(path, tag, lang, label):
    src = open(path, encoding='utf-8').read().rstrip('\n')
    return ('''
  <figure class="listing">
    <figcaption>
      <span class="path">%s</span>
      <span class="tag %s">%s</span>
      <span class="lang" data-lang="%s">%s</span>
    </figcaption>
    <pre><code class="lang-%s">%s</code></pre>
  </figure>
''' % (path, tag, tag, lang, label, lang, esc(src)))

w('''
  <!-- ================= 4. IMPLEMENTATION ================= -->
  <h2 id="implementation"><span class="num">4</span>Implementation</h2>

  <p>
    Four changes, and three of them are small. A vertex learns its <code>1/w</code>; the fill's
    growing pile of trailing parameters becomes an object; the inner loop pre-divides and divides
    back; and the demo gets a floor worth looking at.
  </p>

  <h3 id="impl-vertex">4.1 A vertex learns 1/w</h3>

  <p>
    The correction needs the clip-space <code>w</code> at each corner. We have been computing it
    and throwing it away for two lessons: <code>perspective_divide</code> consumes <code>w</code>
    and does not hand it back. So <code>project()</code> keeps it, and <code>vertex</code> stores
    it &mdash; pre-inverted.
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
    float z = 0.0f;                  ///&lt; DEVICE depth in [0,1]  (3.1)
    float inv_w = 1.0f;              ///&lt; 1 / clip-space w       (3.2)
    float u = 0.0f;                  ///&lt; texture coordinate     (3.2)
    float v = 0.0f;
    Uint32 colour = 0xFFFFFFFFu;     ///&lt; ARGB8888 as stored
};</code></pre>
  </figure>

  <p>
    <strong>Stored inverted, not as <code>w</code>.</strong> Two reasons, both about the inner loop.
    It is <code>1/w</code> that gets interpolated, so storing <code>w</code> would mean a divide per
    vertex and then the same divide again per pixel. And the &ldquo;divide back&rdquo; at the end
    becomes a multiply by a reciprocal we had to compute anyway.
  </p>

  <div class="callout ok">
    <span class="label">The default is load-bearing</span>
    <p>
      <code>inv_w = 1.0f</code> is not a placeholder &mdash; it is the correct value for the 2-D
      case. With <code>w = 1</code> at every corner the numerator and denominator both scale by 1,
      the division is by exactly one, and the perspective-correct path returns precisely the affine
      answer. <strong>Which is right</strong>: affine interpolation is correct under an orthographic
      projection, and a flat 2-D fill is orthographic. Every triangle drawn in Modules 1 and 2 goes
      through the new code and comes out bit-identical &mdash; the harness checks 17,275 covered
      pixels and finds <code>0</code> differences.
    </p>
  </div>

  <h3 id="impl-style">4.2 Four trailing parameters become a pipeline object</h3>

  <p>
    <code>fill_triangle</code> ended Lesson 3.1 with a <code>blend_space</code> parameter. This
    lesson wants to add two more &mdash; the interpolation mode and how a pixel turns
    <code>(u,v)</code> into a colour &mdash; and Lesson 3.6 will want a fourth. Four trailing
    enums at every call site is where an API starts to rot.
  </p>

  <figure class="listing">
    <figcaption>
      <span class="path">src/gfx/raster.hpp &mdash; render state, gathered</span>
      <span class="tag modified">modified</span>
      <span class="lang" data-lang="cpp">C++</span>
    </figcaption>
    <pre><code class="lang-cpp">struct fill_style
{
    interpolation interp = interpolation::perspective;
    shading shade = shading::vertex_colour;
    blend_space space = blend_space::linear;
};

void fill_triangle(framebuffer&amp; fb, depth_buffer* depth,
                   const vertex&amp; a, const vertex&amp; b, const vertex&amp; c,
                   fill_style style = {});</code></pre>
  </figure>

  <p>
    This is tidiness, but it is not <em>only</em> tidiness &mdash; it is the shape the hardware has.
    A GPU does not take render state as arguments to a draw call. It bakes state into a
    <strong>pipeline object</strong> built once and bound before drawing, because validating and
    compiling that state per draw would be ruinous.
    <code>SDL_GPUGraphicsPipelineCreateInfo</code> is this struct, several times over. Module 4
    makes the argument properly; adopting the shape now means Lesson 3.6 costs one field instead of
    one more parameter at every call site.
  </p>

  <p>
    Every field defaults to the <em>correct</em> value, so <code>fill_triangle(fb, a, b, c)</code>
    is right and each deliberately-broken mode has to be asked for by name. That is the same
    discipline as <code>blend_space::encoded</code> and <code>draw_line_naive</code>: keep the
    failure, make it opt-in.
  </p>

  <div class="callout cpp">
    <span class="label">C++ in place &mdash; designated initialisers</span>
    <p>
      C++20 lets you name the fields you are setting:
      <code>fill_style{.interp = interpolation::affine}</code>. The rest take their defaults, and
      the reader does not have to count commas or remember the order. Two rules that catch people:
      the initialisers must appear in <strong>declaration order</strong> (unlike C), and you cannot
      mix named and positional. Both restrictions exist so that a struct's field order stays a
      private matter, which is exactly what you want here — 3.6 will insert a field and no call
      site should care.
    </p>
    <p>
      The same feature cleans up the 2-D call sites in the demo, where <code>vertex</code> now has
      seven fields and only two of them are interesting:
      <code>vertex{.x = vx[0], .y = vy[0], .colour = red}</code>.
    </p>
  </div>

  <h3 id="impl-loop">4.3 The correction, in the loop</h3>

  <p>Per triangle, pre-divide the attributes:</p>

  <figure class="listing">
    <figcaption>
      <span class="path">src/gfx/raster.cpp &mdash; hoisted per triangle</span>
      <span class="tag modified">modified</span>
      <span class="lang" data-lang="cpp">C++</span>
    </figcaption>
    <pre><code class="lang-cpp">const bool correct = (style.interp == interpolation::perspective);
const float iw0 = correct ? v0.inv_w : 1.0f;
const float iw1 = correct ? v1.inv_w : 1.0f;
const float iw2 = correct ? v2.inv_w : 1.0f;

const rgb3 p0{c0.r * iw0, c0.g * iw0, c0.b * iw0};
const rgb3 p1{c1.r * iw1, c1.g * iw1, c1.b * iw1};
const rgb3 p2{c2.r * iw2, c2.g * iw2, c2.b * iw2};

const float pu0 = v0.u * iw0, pv0 = v0.v * iw0;
const float pu1 = v1.u * iw1, pv1 = v1.v * iw1;
const float pu2 = v2.u * iw2, pv2 = v2.v * iw2;</code></pre>
  </figure>

  <p>
    <strong>The affine mode is not a second code path.</strong> It is this same arithmetic with
    every <code>1/w</code> forced to 1 &mdash; which is not a trick, it is what affine interpolation
    <em>is</em>: a perspective renderer behaving as though <code>w</code> were 1 everywhere, which
    is to say doing orthographic interpolation. Writing it this way is shorter than two loops and it
    says the thing.
  </p>

  <p>Then, per pixel:</p>

  <figure class="listing">
    <figcaption>
      <span class="path">src/gfx/raster.cpp &mdash; one divide, every attribute</span>
      <span class="tag modified">modified</span>
      <span class="lang" data-lang="cpp">C++</span>
    </figcaption>
    <pre><code class="lang-cpp">const float w_recip = 1.0f / (f0 * iw0 + f1 * iw1 + f2 * iw2);

if (style.shade == shading::uv_checker)
{
    const float uu = (f0 * pu0 + f1 * pu1 + f2 * pu2) * w_recip;
    const float vv = (f0 * pv0 + f1 * pv1 + f2 * pv2) * w_recip;
    row[x] = checker_at(uu, vv);
}
else
{
    const rgb3 mixed{(f0 * p0.r + f1 * p1.r + f2 * p2.r) * w_recip,
                     (f0 * p0.g + f1 * p1.g + f2 * p2.g) * w_recip,
                     (f0 * p0.b + f1 * p1.b + f2 * p2.b) * w_recip};
    row[x] = pixel_from(mixed, style.space);
}</code></pre>
  </figure>

  <p>
    <code>f0, f1, f2</code> are the same unbiased weights everything else uses &mdash; the top-left
    rule's bias subtracted back out, exactly as Lesson 2.4 established and Lesson 3.1 repeated. One
    reciprocal serves the uv and all three colour channels, which is the amortisation &sect;3.6
    promised.
  </p>

  <p>
    And immediately above it, untouched, sits the depth test &mdash; interpolating <code>v.z</code>
    <em>directly</em>, with no correction, for the reason &sect;3.5 gave. The comment in the source
    says so at the point where somebody would otherwise "fix" it.
  </p>

  <h3 id="impl-checker">4.4 Something to paint on it</h3>

  <p>
    A uv that nothing reads proves nothing, and we have no textures until Lesson 3.9. So the
    rasterizer gains one procedural pattern:
  </p>

  <figure class="listing">
    <figcaption>
      <span class="path">src/gfx/raster.cpp &mdash; the debug checker</span>
      <span class="tag modified">modified</span>
      <span class="lang" data-lang="cpp">C++</span>
    </figcaption>
    <pre><code class="lang-cpp">[[nodiscard]] Uint32 checker_at(float u, float v)
{
    const float cu = std::floor(u);
    const float cv = std::floor(v);
    const bool light = (static_cast&lt;int&gt;(cu) + static_cast&lt;int&gt;(cv)) % 2 == 0;
    return light ? pack_argb(232, 226, 214) : pack_argb(58, 64, 88);
}</code></pre>
  </figure>

  <div class="callout pitfall">
    <span class="label"><code>std::floor</code>, not a cast</span>
    <p>
      A cast to <code>int</code> truncates <strong>toward zero</strong>, so <code>-0.5</code> and
      <code>+0.5</code> both land in cell <code>0</code> and the pattern grows a doubled cell
      straddling the origin &mdash; a seam that appears only where a uv goes negative, which is
      exactly what happens the moment a mesh is tiled or a uv is offset. Our floor's uvs run from
      <code>-3</code> to <code>+3</code> across, so this one would have shipped visible.
      <code>std::floor</code> is the function that means &ldquo;which cell&rdquo; for negative
      numbers too.
    </p>
  </div>

  <p>
    A <code>shading</code> enum inside the rasterizer is frankly a placeholder for a fragment
    shader, and the header says so. A real renderer lets the caller supply the function; Module 4
    does exactly that and calls it a fragment shader. Lesson 3.6 will add a lighting term and this
    enum will start to strain &mdash; which is the point at which the right structure will have
    earned itself, rather than being asserted three lessons early.
  </p>

  <h3 id="impl-floor">4.5 A floor, and the viewport that finally moves</h3>

  <p>
    The demo needs a surface with a violent depth gradient. The floor runs from about a unit in
    front of the camera to thirty-seven units away &mdash; a <strong>37:1 range of
    <code>w</code></strong> &mdash; because that ratio is precisely what decides how loud the
    artifact is.
  </p>

  <p>
    That immediately broke something. Every scene since Lesson 2.10 has drawn into an inset
    172&times;96.75 rectangle with the HUD beside it, and a ground plane <em>cannot</em> fit in a
    sub-rectangle: measured across every extent worth having
    (<code>scratch/fit_floor.cpp</code>), the near edge always projects outside the NDC box. That is
    not a tuning failure. A surface you are standing on fills the bottom of your view &mdash; which
    is the same property that makes it a good subject for this lesson.
  </p>

  <figure class="listing">
    <figcaption>
      <span class="path">src/main.cpp &mdash; a second viewport</span>
      <span class="tag modified">modified</span>
      <span class="lang" data-lang="cpp">C++</span>
    </figcaption>
    <pre><code class="lang-cpp">constexpr engine::viewport k_full_viewport{0.0f, 0.0f,
                                          static_cast&lt;float&gt;(k_fb_width),
                                          static_cast&lt;float&gt;(k_fb_height), 0.0f, 1.0f};

// …later, per frame:
const engine::viewport&amp; vp = (scene_mode == scene_kind::floor)
                           ? k_full_viewport : k_scene_viewport;</code></pre>
  </figure>

  <p>
    So the floor draws through a viewport that is the whole framebuffer. This costs nothing and
    needs no new projection matrix, because <strong>320&times;180 is already 16:9</strong> &mdash;
    the same aspect the projection bakes in. It is also the first time this course has changed the
    viewport at all, which is the entire reason Lesson 2.11 made it a <code>struct</code> with a
    rectangle in it rather than three constants. The change was to thread it through
    <code>project</code>, <code>line3</code>, <code>draw_world</code>, <code>draw_mesh</code> and
    <code>collect_triangles</code> as a parameter &mdash; which is what it always should have been.
  </p>

  <p>
    The floor itself is built at runtime, and that is worth a paragraph because it is the first
    mesh in this course that is:
  </p>

  <figure class="listing">
    <figcaption>
      <span class="path">src/main.cpp &mdash; a mesh that needs an owner</span>
      <span class="tag modified">modified</span>
      <span class="lang" data-lang="cpp">C++</span>
    </figcaption>
    <pre><code class="lang-cpp">struct floor_geometry
{
    std::vector&lt;engine::vec3&gt; vertices;
    std::vector&lt;engine::vec2&gt; uvs;
    std::vector&lt;std::uint16_t&gt; indices;
    int cells = 0;                       ///&lt; quads per side; 0 = not built yet

    [[nodiscard]] engine::mesh view() const { return {vertices, indices, uvs}; }
};</code></pre>
  </figure>

  <p>
    An <code>engine::mesh</code> is a pair of non-owning spans (Lesson 2.12). Every mesh so far has
    viewed <code>inline constexpr</code> arrays with program lifetime, so nobody had to own
    anything. A floor whose tessellation changes at runtime does, and the demo ends up holding three
    vectors and a <code>view()</code> that manufactures a mesh over them. That awkwardness is not a
    design failure &mdash; it is the exact pressure that produces Module 5's asset system, and it is
    better felt than described.
  </p>

  <p>
    One detail in <code>build_floor</code> is load-bearing: the uvs are computed from the
    <strong>world position</strong>, not from the grid index. That is what makes the checker pattern
    bit-identical at every tessellation level, so pressing <kbd>T</kbd> changes only how many
    triangles the interpolation has to span. Without it you would be comparing two different
    pictures and learning nothing.
  </p>

  <h3 id="impl-measure">4.6 Measuring it</h3>

  <p>
    Lesson 3.1 built a comparison harness into the demo: render the scene twice with one thing
    changed and count the pixels that disagree. That generalises for free &mdash; the only question
    is <em>which</em> thing to change.
  </p>

  <figure class="listing">
    <figcaption>
      <span class="path">src/main.cpp &mdash; one comparison, two questions</span>
      <span class="tag modified">modified</span>
      <span class="lang" data-lang="cpp">C++</span>
    </figcaption>
    <pre><code class="lang-cpp">if (checkered)
{
    engine::fill_style other = style;
    other.interp = (interp == engine::interpolation::perspective)
                 ? engine::interpolation::affine
                 : engine::interpolation::perspective;
    draw_triangles(scratch_fb, want_painter ? nullptr : &amp;scratch_depth,
                   scene_tris, want_painter, other);
    interp_wrong = count_differences(fb, scratch_fb, vp);
    painter_wrong = 0;
}</code></pre>
  </figure>

  <p>
    On the solid scenes the second render changes the hidden-surface strategy (3.1's number); on the
    floor it changes the interpolation (3.2's). One scratch framebuffer, one extra pass, and the
    claim &ldquo;this is wrong&rdquo; becomes a number that moves while you orbit.
  </p>
''')

# ---------------- listings ----------------
w('''
  <!-- ================= 5. COMPLETE CODE LISTINGS ================= -->
  <h2 id="listings"><span class="num">5</span>Complete Code Listings</h2>

  <p>Every file changed this lesson, in full.</p>

  <div class="tbl-scroll">
    <table>
      <caption class="visually-hidden">File manifest for Lesson 3.2</caption>
      <thead><tr><th>Path</th><th>State</th><th>Purpose</th></tr></thead>
      <tbody>
        <tr><td><code>src/gfx/raster.hpp</code></td><td>modified</td>
            <td><code>vertex</code> gains <code>inv_w</code>, <code>u</code>, <code>v</code>; the
                <code>interpolation</code> and <code>shading</code> enums; <code>fill_style</code>.</td></tr>
        <tr><td><code>src/gfx/raster.cpp</code></td><td>modified</td>
            <td>Pre-divide per triangle, divide back per pixel, and the debug checker.</td></tr>
        <tr><td><code>src/gfx/mesh.hpp</code></td><td>modified</td>
            <td>A <code>uvs</code> span and <code>uv_at()</code>; uvs on the quad.</td></tr>
        <tr><td><code>src/main.cpp</code></td><td>modified</td>
            <td>The floor scene, runtime tessellation, the viewport threaded as a parameter, and
                the affine-vs-correct counter.</td></tr>
      </tbody>
    </table>
  </div>
''')

w(listing('src/gfx/raster.hpp', 'modified', 'cpp', 'C++'))
w(listing('src/gfx/raster.cpp', 'modified', 'cpp', 'C++'))
w(listing('src/gfx/mesh.hpp', 'modified', 'cpp', 'C++'))
w(listing('src/main.cpp', 'modified', 'cpp', 'C++'))

open('docs/lessons/03-02-perspective-correct.html', 'a').write(out.getvalue())
print("part 5:", len(out.getvalue()), "chars")
