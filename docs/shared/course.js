/* ==========================================================================
   Build a Professional 3D Game Engine — shared page script
   ==========================================================================
   Theme toggle, table-of-contents scrollspy, and the syntax highlighter, for
   every page in docs/. Linked (not inlined) between each page's SHARED-SCRIPT
   markers, immediately before the two KaTeX CDN tags — which stay inline in the
   pages because they carry SRI hashes and an inline onload handler.

   Loaded as a plain classic script at end of body, so it runs at exactly the
   point the inline copy used to: after the DOM is parsed, before KaTeX's
   deferred render.

   Not a build artifact — see the note in course.css.
   ========================================================================== */

(function () {
  "use strict";

  /* ---- Theme toggle: OS default, with a manual override in localStorage ---- */
  var root = document.documentElement;
  var KEY = "engine-course-theme";
  try {
    var saved = localStorage.getItem(KEY);
    if (saved) { root.setAttribute("data-theme", saved); root.style.colorScheme = saved; }
  } catch (e) { /* private mode — fall back to the OS preference */ }

  var btn = document.getElementById("theme-toggle");
  if (btn) {
    btn.addEventListener("click", function () {
      var cur = root.getAttribute("data-theme");
      if (!cur) {
        cur = window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
      }
      var next = cur === "dark" ? "light" : "dark";
      root.setAttribute("data-theme", next);
      root.style.colorScheme = next;
      try { localStorage.setItem(KEY, next); } catch (e) {}
    });
  }

  /* ---- Syntax highlighting -------------------------------------------------
     A deliberately small tokeniser for C++/HLSL, CMake, and shell. It reads
     textContent and rebuilds escaped HTML, so it can never execute page content
     or mangle a listing — worst case a token is mis-coloured.

     Why not highlight.js from a CDN? Because a lesson must render from a bare
     filesystem with no network, and colour is not worth a dependency that can
     be absent. This is ~60 lines and always there.

     Keyword vs type matters: `kw` is consulted before `ty`, so a word listed in
     both lands in .tok-k. Fundamental types (bool, char, int, …) belong in
     CPP_TYPES only — putting them in CPP_KEYWORDS would silently recolour every
     declaration in the course.
     -------------------------------------------------------------------------- */
  var CPP_KEYWORDS = ("alignas alignof and asm auto break case catch class concept const consteval "
    + "constexpr constinit const_cast continue co_await co_return co_yield decltype default defined "
    + "delete do dynamic_cast else enum explicit export extern false final for friend goto if inline "
    + "mutable namespace new noexcept not nullptr operator or override private protected public "
    + "register reinterpret_cast requires return sizeof static static_assert static_cast struct "
    + "switch template this thread_local throw true try typedef typeid typename union using virtual "
    + "volatile while cbuffer register_space numthreads in out inout").split(" ");

  var CPP_TYPES = ("bool char char8_t char16_t char32_t double float int long short signed unsigned "
    + "void wchar_t size_t uint8_t uint16_t uint32_t uint64_t int8_t int16_t int32_t int64_t "
    + "Uint8 Uint16 Uint32 Uint64 Sint8 Sint16 Sint32 Sint64 "
    + "float2 float3 float4 float4x4 float3x3 half int2 int3 int4 uint uint2 uint3 uint4 matrix "
    + "Texture2D TextureCube SamplerState StructuredBuffer").split(" ");

  var kw = Object.create(null), ty = Object.create(null);
  CPP_KEYWORDS.forEach(function (w) { kw[w] = 1; });
  CPP_TYPES.forEach(function (w) { ty[w] = 1; });

  function esc(s) {
    return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
  }
  function span(cls, s) { return '<span class="tok-' + cls + '">' + esc(s) + "</span>"; }

  // One regex, alternation ordered so longer/greedier forms win.
  var RE = new RegExp([
    "(\\/\\*[\\s\\S]*?\\*\\/|\\/\\/[^\\n]*|#[^\\n]*)",       // 1 comment / preprocessor
    '("(?:\\\\.|[^"\\\\])*"|\'(?:\\\\.|[^\'\\\\])*\')',      // 2 string / char
    "(\\b\\d[\\w.]*\\b)",                                     // 3 number
    "([A-Za-z_]\\w*)(?=\\s*\\()",                             // 4 call site
    "([A-Za-z_]\\w*)",                                        // 5 identifier
    "([{}()\\[\\];,.<>+\\-*/%=!&|^~?:]+)"                     // 6 punctuation
  ].join("|"), "g");

  function highlightCLike(src) {
    var out = "", last = 0, m;
    RE.lastIndex = 0;
    while ((m = RE.exec(src)) !== null) {
      if (m.index > last) out += esc(src.slice(last, m.index));
      if (m[1])      out += span(m[1].charAt(0) === "#" ? "p" : "c", m[1]);
      else if (m[2]) out += span("s", m[2]);
      else if (m[3]) out += span("n", m[3]);
      else if (m[4]) out += kw[m[4]] ? span("k", m[4]) : span("f", m[4]);
      else if (m[5]) out += kw[m[5]] ? span("k", m[5])
                          : ty[m[5]] ? span("t", m[5])
                          : esc(m[5]);
      else if (m[6]) out += span("o", m[6]);
      last = RE.lastIndex;
    }
    out += esc(src.slice(last));
    return out;
  }

  // Shell: `#` comments anywhere, plus Windows-batch `::` comments — but only at
  // the start of a line, because `::` is also a namespace separator and a CMake
  // target separator (`SDL3::SDL3`), which do appear mid-line in shell listings.
  var SH_RE = /(^|\n)(\s*)(::[^\n]*)|(#[^\n]*)|("(?:\\.|[^"\\])*"|'[^']*')|(^|\n)(\s*)([\w.\/\\-]+)/g;
  function highlightShell(src) {
    var out = "", last = 0, m;
    SH_RE.lastIndex = 0;
    while ((m = SH_RE.exec(src)) !== null) {
      if (m.index > last) out += esc(src.slice(last, m.index));
      if (m[3])      out += esc(m[1] || "") + esc(m[2] || "") + span("c", m[3]);
      else if (m[4]) out += span("c", m[4]);
      else if (m[5]) out += span("s", m[5]);
      else           out += esc(m[6] || "") + esc(m[7] || "") + span("f", m[8]);
      last = SH_RE.lastIndex;
    }
    out += esc(src.slice(last));
    return out;
  }

  // CMake: comments, strings, ${vars}, and command names (word before "(").
  var CMAKE_RE = /(#[^\n]*)|("(?:\\.|[^"\\])*")|(\$\{[^}]*\})|(^|\n)(\s*)([A-Za-z_]\w*)(?=\s*\()/g;
  function highlightCMake(src) {
    var out = "", last = 0, m;
    CMAKE_RE.lastIndex = 0;
    while ((m = CMAKE_RE.exec(src)) !== null) {
      if (m.index > last) out += esc(src.slice(last, m.index));
      if (m[1])      out += span("c", m[1]);
      else if (m[2]) out += span("s", m[2]);
      else if (m[3]) out += span("n", m[3]);
      else           out += esc(m[4] || "") + esc(m[5] || "") + span("f", m[6]);
      last = CMAKE_RE.lastIndex;
    }
    out += esc(src.slice(last));
    return out;
  }

  var DISPATCH = {
    "lang-cpp": highlightCLike, "lang-hlsl": highlightCLike, "lang-c": highlightCLike,
    "lang-bash": highlightShell, "lang-sh": highlightShell,
    "lang-cmake": highlightCMake
  };
  document.querySelectorAll(".listing pre code").forEach(function (el) {
    for (var cls in DISPATCH) {
      if (el.classList.contains(cls)) {
        el.innerHTML = DISPATCH[cls](el.textContent);
        return;
      }
    }
  });
})();
