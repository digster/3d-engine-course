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
  <!-- ================= 5. COMPLETE CODE LISTINGS ================= -->
  <h2 id="listings"><span class="num">5</span>Complete Code Listings</h2>

  <p>Every file created or changed this lesson, in full.</p>

  <div class="tbl-scroll">
    <table>
      <caption class="visually-hidden">File manifest for Lesson 3.1</caption>
      <thead><tr><th>Path</th><th>State</th><th>Purpose</th></tr></thead>
      <tbody>
        <tr><td><code>src/gfx/depth_buffer.hpp</code></td><td><strong>new</strong></td>
            <td>The depth attachment: storage, clear, format, and the reasoning for keeping it out of <code>framebuffer</code>.</td></tr>
        <tr><td><code>src/gfx/depth_buffer.cpp</code></td><td><strong>new</strong></td>
            <td>Allocation (to <em>far</em>, never zero), clear, clamped row access.</td></tr>
        <tr><td><code>src/gfx/raster.hpp</code></td><td>modified</td>
            <td><code>vertex</code> gains <code>z</code>; <code>fill_triangle</code> gains a nullable depth attachment.</td></tr>
        <tr><td><code>src/gfx/raster.cpp</code></td><td>modified</td>
            <td>The depth test in the inner loop: interpolate, quantise, compare, write.</td></tr>
        <tr><td><code>src/gfx/mesh.hpp</code></td><td>modified</td>
            <td><code>quad_mesh()</code> — the flat panel the failure scenes are built from.</td></tr>
        <tr><td><code>src/main.cpp</code></td><td>modified</td>
            <td>Filled triangles through the 3-D pipeline, four scenes, four hidden-surface modes, and the disagreement counter.</td></tr>
        <tr><td><code>CMakeLists.txt</code></td><td>modified</td>
            <td>One new source file.</td></tr>
      </tbody>
    </table>
  </div>
''')

w(listing('src/gfx/depth_buffer.hpp', 'new', 'cpp', 'C++'))
w(listing('src/gfx/depth_buffer.cpp', 'new', 'cpp', 'C++'))
w(listing('src/gfx/raster.hpp', 'modified', 'cpp', 'C++'))
w(listing('src/gfx/raster.cpp', 'modified', 'cpp', 'C++'))
w(listing('src/gfx/mesh.hpp', 'modified', 'cpp', 'C++'))
w(listing('CMakeLists.txt', 'modified', 'cmake', 'CMake'))
w(listing('src/main.cpp', 'modified', 'cpp', 'C++'))

with open('docs/lessons/03-01-z-buffer.html', 'a') as f:
    f.write(out.getvalue())
print("part 6 appended:", len(out.getvalue()), "chars")
