// docs/_template/check-page.js — the browser-side half of lesson verification.
//
// Paste into a real Chromium console (or run via Playwright's evaluate) on a
// lesson page served over HTTP. NOT part of the published pages; this is an
// authoring tool, like apply-shared.py.
//
// Returns an object with one key per check. Every array should be empty and
// every count should match. See docs/_template/README.md §13.
//
// Why each check exists — every one earned its place by catching something:
//
//   1. HIGHLIGHTER ROUND-TRIP. The syntax highlighter rewrites innerHTML of
//      every listing. If it ever mangles one, the page still renders and the
//      damage is silent. Comparing live textContent against a DOMParser parse
//      of the same file (which runs no scripts) proves no listing was corrupted.
//
//   2. KATEX POSITIVE SIGNAL. Raw TeX on the page is *also* the documented
//      CDN-unreachable fallback, so "no console errors" passed for six lessons
//      while the maths renderer was entirely absent (Lesson 1.8). Check that
//      .katex count equals .eq count, and that exactly TWO katex script tags
//      exist — more means a duplicated block that double-renders.
//
//   3. SVG GEOMETRY, in three flavours, because each misses what the others
//      catch:
//        a. spill    — a label outside its own viewBox. MUST use
//                      getBoundingClientRect(): getBBox() is in LOCAL
//                      coordinates, so anything inside a <g transform> is
//                      compared against the wrong origin and reports false
//                      positives (three of them in Lesson 1.8).
//        b. overlap  — two labels on top of each other.
//        c. onShape  — a label sitting on a line or curve. Neither (a) nor (b)
//                      can see this, and it has now shipped twice: Lesson 1.3's
//                      Euler figure and Lesson 2.1's Figure 4. Sampling points
//                      along each stroke and testing them against text boxes is
//                      the cheapest thing that catches it.
//
//   4. SHARED ASSETS. The CSS and page script are linked from docs/shared/
//      rather than inlined, so a wrong relative href now breaks a page — and
//      breaks it silently. Nothing throws; the page renders unstyled and inert,
//      which reads as an unfinished page rather than a broken one. The correct
//      prefix depends on the page's depth, so this breaks by MOVING a file, not
//      by editing one. Checked by positive signal, like KaTeX above: the sheet's
//      rules are in effect and the highlighter actually ran.
//
//      Do NOT decide this on styleSheets[].cssRules. Over file:// WebKit makes
//      every file its own origin and throws a SecurityError reading CSSOM, on a
//      sheet that loaded and applied correctly — the naive probe reports a
//      perfectly good page as broken. Computed style is the honest signal.
//
//   5. BADGE VOCABULARY. Listing captions use .tag with the modifier
//      `modified`; manifest tables use .badge with `mod`. Crossing the two
//      produces `class="tag mod"`, which reads correctly, renders the correct
//      word, and matches no rule — the badge goes silently grey instead of
//      amber. 82 of them shipped across 14 pages that way.
//
//   6. LISTING FOLD. Long listings are clamped by CSS and expanded by a
//      control the shared script adds. If the script ever fails to reach a
//      listing, the clamp still applies and the code becomes unreachable on a
//      page that looks perfectly normal. Check that nothing is clipped without
//      a way to open it, and that both controls carry working ARIA.
//
// What none of these catch: a label on a filled <rect>, and a line drawn
// through the wrong row of a stacked diagram. Look at the rendered figure.

(function checkPage() {
  const out = {};

  // ---- 6. LISTING FOLD ----------------------------------------------------
  // Measured FIRST, while the page is still in the state a reader actually
  // loads it in — everything below this block runs against a fully expanded
  // page instead (see the expand-all at the end of this section).
  out.collapsibleListings = document.querySelectorAll('.listing.is-collapsible').length;
  out.clippedWithoutToggle = [];
  out.listingA11y = [];

  document.querySelectorAll('.listing').forEach(fig => {
    const pre = fig.querySelector('pre');
    if (!pre) { return; }
    const path = fig.querySelector('.path')?.textContent || '(unknown)';

    // THE failure mode this feature can introduce: content taller than its box
    // with no control to reveal it. Silently unreachable code, on a page that
    // looks completely fine. Note this also fires en masse if course.js failed
    // to load at all — the CSS clamp is in force but nothing ever added a
    // toggle — which is exactly the right alarm for that too.
    if (pre.scrollHeight > pre.clientHeight + 2 &&
        fig.querySelectorAll('.listing-toggle, .listing-fold').length === 0) {
      out.clippedWithoutToggle.push(path);
    }

    fig.querySelectorAll('.listing-toggle, .listing-fold').forEach(b => {
      const controls = b.getAttribute('aria-controls');
      if (b.getAttribute('aria-expanded') === null) {
        out.listingA11y.push({ path, why: 'no aria-expanded' });
      } else if (!controls || !document.getElementById(controls)) {
        out.listingA11y.push({ path, why: 'aria-controls unresolved: ' + controls });
      }
    });
  });

  // Expand everything before measuring anything else. Every check below then
  // sees the page exactly as it did before folding existed — and, crucially,
  // a clamp can never mask a horizontal-overflow or wrapped-listing regression
  // by hiding the offending lines.
  document.querySelectorAll('.listing.is-collapsed').forEach(fig => {
    fig.classList.remove('is-collapsed');
    fig.classList.add('is-open');
  });

  // ---- 3. SVG geometry ----------------------------------------------------
  const spill = [], overlap = [], onShape = [];

  document.querySelectorAll('figure.dia svg').forEach((svg, fi) => {
    const svgBox = svg.getBoundingClientRect();
    const texts = [...svg.querySelectorAll('text')]
      .map(t => ({ r: t.getBoundingClientRect(), s: t.textContent.trim() }))
      .filter(o => o.r.width > 0 && o.s.length > 0);

    // (a) spill outside the viewBox
    texts.forEach(t => {
      const over = Math.max(t.r.right - svgBox.right, svgBox.left - t.r.left,
                            t.r.bottom - svgBox.bottom, svgBox.top - t.r.top);
      if (over > 0.5) {
        spill.push({ fig: fi + 1, text: t.s.slice(0, 40), overPx: +over.toFixed(1) });
      }
    });

    // (b) text over text
    for (let i = 0; i < texts.length; i++) {
      for (let j = i + 1; j < texts.length; j++) {
        const a = texts[i].r, b = texts[j].r;
        const ox = Math.min(a.right, b.right) - Math.max(a.left, b.left);
        const oy = Math.min(a.bottom, b.bottom) - Math.max(a.top, b.top);
        if (ox > 2 && oy > 2) {
          overlap.push({ fig: fi + 1, a: texts[i].s.slice(0, 28), b: texts[j].s.slice(0, 28) });
        }
      }
    }

    // (c) text over a stroke.
    //
    // Skip anything inside <defs>: an arrowhead's <path> lives there and is
    // never painted at its own coordinates, but it still answers
    // getTotalLength() and getScreenCTM(), so including it reports collisions
    // at wherever the marker template happens to sit. That false positive cost
    // ten minutes the first time.
    const seen = new Set();
    svg.querySelectorAll('line, polyline, path').forEach(el => {
      if (el.closest('defs, marker, clipPath, mask, symbol')) { return; }

      // Graph paper is meant to sit under labels — that is what makes it graph
      // paper. Note `.grid` is usually set on the wrapping <g>, so this has to
      // be closest() and not getAttribute(): reading the attribute off the
      // <path> itself returns null and every gridline reads as an unclassed
      // stroke.
      if (el.closest('.grid')) { return; }
      let length = 0;
      try { length = el.getTotalLength ? el.getTotalLength() : 0; } catch (e) { return; }
      if (!length) { return; }

      const matrix = el.getScreenCTM();
      if (!matrix) { return; }

      const n = Math.min(200, Math.max(12, Math.round(length / 3)));
      const pts = [];
      for (let i = 0; i <= n; i++) {
        const p = el.getPointAtLength(length * i / n);
        const q = svg.createSVGPoint();
        q.x = p.x; q.y = p.y;
        pts.push(q.matrixTransform(matrix));
      }

      texts.forEach(t => {
        // 1.5px of slack: a descender grazing a rule is not a collision.
        const pad = 1.5;
        const inside = pts.filter(p =>
          p.x > t.r.left + pad && p.x < t.r.right - pad &&
          p.y > t.r.top + pad && p.y < t.r.bottom - pad);
        if (inside.length > 2) {
          // Report the nearest class in the ancestry, not the element's own —
          // these diagrams set the styling class on a wrapping <g>.
          const owner = el.closest('[class]');
          const cls = owner ? owner.getAttribute('class') : '(none)';
          const key = fi + '|' + t.s + '|' + el.tagName + '|' + cls;
          if (!seen.has(key)) {
            seen.add(key);
            onShape.push({ fig: fi + 1, text: t.s.slice(0, 38), shape: el.tagName, cls });
          }
        }
      });
    });
  });

  out.svgSpill = spill;
  out.svgTextOverlap = overlap;
  out.svgTextOnShape = onShape;
  out.figures = document.querySelectorAll('figure.dia svg').length;

  // ---- 3b. FIGURE NUMBERS IN PAGE ORDER -----------------------------------
  //
  // Added after Lesson 6.12, because this is the SECOND time the same drift
  // shipped: 6.10 rendered `fig4.png` captioned "Figure 5", and 6.12's first
  // build put the resolve diagram at 4 while the body showed the per-channel
  // comparison first. Both happened for the same structural reason — the
  // NUMBERS live in build_NN.py and the ORDER lives in the body fragments, so
  // they are two files that have to agree and nothing was making them.
  //
  // Nothing else here can catch it. The caption is present, the figure renders,
  // every reference resolves; only a human reading the captions in sequence
  // would notice, and a human reading 3,700 lines does not.
  out.figOrder = (() => {
    const nums = [...document.querySelectorAll('figure.dia .fignum')]
      .map(el => {
        const m = /Figure\s+(\d+)/.exec(el.textContent || '');
        return m ? Number(m[1]) : null;
      });
    const bad = [];
    nums.forEach((n, i) => {
      if (n !== i + 1) { bad.push({ position: i + 1, says: n }); }
    });
    return bad;
  })();

  // ---- 4. SHARED ASSETS ---------------------------------------------------
  // The CSS and page script are linked from docs/shared/, not inlined, so a
  // wrong relative href is now a real failure mode — and a silent one. Nothing
  // throws: the page renders unstyled and inert, which reads as "a page nobody
  // has styled yet" rather than as a bug. The prefix depends on the page's
  // depth (shared/… at docs/, ../shared/… at docs/lessons/), so this breaks by
  // moving a file, not by editing one.
  //
  // Check the POSITIVE signal, the same way the KaTeX check does: prove the
  // stylesheet's rules are actually in effect, not merely that the <link> tag
  // is present in the markup — a 404 leaves the tag sitting there looking fine.
  out.sharedCssLinked = !!document.querySelector('link[href$="shared/course.css"]');
  // Reading .cssRules is the obvious probe and the wrong one to trust alone:
  // over file:// WebKit treats every file as its own origin and throws a
  // SecurityError on CSSOM access, even though the sheet loaded and applied
  // perfectly. So a throw is INCONCLUSIVE (null), never a failure — the
  // computed-style probe below is what actually decides.
  out.sharedCssLoaded = (() => {
    const sheet = [...document.styleSheets]
      .find(s => s.href && s.href.includes('shared/course.css'));
    if (!sheet) return false;              // never even reached the sheet list
    try { return sheet.cssRules.length > 0; }
    catch (e) { return null; }             // WebKit + file://: cannot tell from here
  })();
  // The decisive check, and the only one that survives every engine and both
  // protocols: is a value only the shared sheet defines actually in effect?
  // --bg is #fdfdfb; with no stylesheet the UA default is white/transparent.
  const probe = document.querySelector('.listing pre') || document.body;
  out.sharedCssApplied = getComputedStyle(probe).backgroundColor !== 'rgba(0, 0, 0, 0)' &&
                         getComputedStyle(probe).backgroundColor !== 'rgb(255, 255, 255)';
  // The shared script signs its work in the markup: the highlighter rewrites
  // every listing into <span class="tok-…">. That is the only trace it leaves
  // that survives into the DOM, so it is the signal. Pages with no listings
  // (index, math-toolbox) cannot be probed this way — report null rather than
  // false, so "nothing to check" never reads as "the script is dead".
  out.sharedJsRan = document.querySelectorAll('.listing pre code').length === 0
    ? null
    : document.querySelectorAll('.listing [class^="tok-"]').length > 0;
  out.sharedOk = out.sharedCssLinked && out.sharedCssLoaded !== false
              && out.sharedCssApplied && out.sharedJsRan !== false;

  // ---- 2. KaTeX -----------------------------------------------------------
  out.eqBlocks = document.querySelectorAll('.eq').length;
  out.katexRendered = document.querySelectorAll('.katex').length;
  out.katexScriptTags = document.querySelectorAll('script[src*="katex"]').length;
  out.katexOk = out.katexRendered === out.eqBlocks && out.katexScriptTags === 2;

  // ---- 5. BADGE VOCABULARY ------------------------------------------------
  // The sheet carries two parallel badge systems and they are easy to cross:
  // manifest tables use .badge with `mod`, listing captions use .tag with
  // `modified`. Writing `class="tag mod"` reads perfectly in the source, renders
  // the right word, and matches no rule at all — so the badge silently falls
  // back to the neutral grey of a bare .tag instead of amber. That shipped in 82
  // badges across 14 pages before anyone noticed, because nothing about it looks
  // wrong until you see a grey and an amber "modified" in the same page.
  //
  // Cheapest possible guard: enumerate the modifiers .tag actually defines.
  // 'unchanged' arrived in Lesson 6.1, for a page that reproduces a file it did
  // not edit. Note that this list and course.css's rules are two places that have
  // to agree — which is the very duplication this check exists to police, so it
  // is worth saying: ADD THE CSS RULE FIRST, then this line.
  const TAG_MODIFIERS = ['new', 'modified', 'unchanged'];
  out.unknownTagClasses = [];
  document.querySelectorAll('.listing figcaption .tag').forEach(el => {
    [...el.classList].forEach(cls => {
      if (cls !== 'tag' && !TAG_MODIFIERS.includes(cls)) {
        out.unknownTagClasses.push({
          cls,
          path: el.closest('figure')?.querySelector('.path')?.textContent || '(unknown)'
        });
      }
    });
  });

  // ---- layout -------------------------------------------------------------
  out.pageScrollsX =
    document.documentElement.scrollWidth > document.documentElement.clientWidth + 1;
  out.wrappedListings = [...document.querySelectorAll('.listing pre code')]
    .filter(c => getComputedStyle(c).whiteSpace.includes('wrap')).length;

  // ---- 1. highlighter round-trip (async, so it resolves last) -------------
  return fetch(location.href).then(r => r.text()).then(src => {
    const pristine = new DOMParser().parseFromString(src, 'text/html');
    const live = [...document.querySelectorAll('.listing pre code')];
    const orig = [...pristine.querySelectorAll('.listing pre code')];
    out.listings = live.length;
    out.corruptedListings = [];
    for (let i = 0; i < live.length; i++) {
      if (live[i].textContent !== orig[i].textContent) {
        out.corruptedListings.push(
          live[i].closest('figure')?.querySelector('.path')?.textContent || ('#' + i));
      }
    }
    out.pass = spill.length === 0 && overlap.length === 0 && onShape.length === 0
            && out.corruptedListings.length === 0 && out.katexOk && out.sharedOk
            && !out.pageScrollsX && out.wrappedListings === 0
            && out.unknownTagClasses.length === 0
            && out.clippedWithoutToggle.length === 0 && out.listingA11y.length === 0
            && out.figOrder.length === 0;
    return out;
  });
})();
