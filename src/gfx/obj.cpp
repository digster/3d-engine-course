// src/gfx/obj.cpp — the parser, the index problem, and the writer.
//
// Lesson 3.5. Read `obj.hpp` first: it says what the format is and what we support.
// This file is the how, and it is organised as the three things that are actually
// hard, in the order they bite:
//
//   1. Reading numbers out of text without lying about failures.
//   2. Resolving an OBJ index — 1-based, possibly negative, possibly absent.
//   3. Turning three independent index streams into one, which is the lesson.

#include "gfx/obj.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace engine {
namespace {

// ---- Reading text ----------------------------------------------------------

[[nodiscard]] constexpr bool is_blank(char c) { return c == ' ' || c == '\t'; }

/// Drop leading spaces and tabs from `s`, in place.
void skip_blanks(std::string_view& s)
{
    std::size_t i = 0;
    while (i < s.size() && is_blank(s[i])) { ++i; }
    s.remove_prefix(i);
}

/// Take the next whitespace-delimited token, consuming it from `s`.
/// Returns an empty view at end of line, which every caller treats as "no more".
[[nodiscard]] std::string_view take_token(std::string_view& s)
{
    skip_blanks(s);
    std::size_t i = 0;
    while (i < s.size() && !is_blank(s[i])) { ++i; }
    const std::string_view token = s.substr(0, i);
    s.remove_prefix(i);
    return token;
}

/// Parse a float, strictly: the WHOLE token must be a number, and it must be finite.
///
/// Two decisions worth defending.
///
/// **`std::strtof`, not `SDL_strtod`.** SDL's is documented to make fewer guarantees
/// than the C runtime's — its handling of scientific notation is explicitly
/// *unspecified* — and exporters emit `1.0e-5` all the time. Checked against
/// `SDL3/SDL_stdinc.h`, not assumed.
///
/// **`std::from_chars` would be better and is not portable enough yet.** It is
/// locale-independent by definition, which `strtof` is not: `strtof` reads the
/// decimal point through `LC_NUMERIC`, so on a machine whose locale writes `1,5` a
/// program that has called `setlocale(LC_ALL, "")` will parse "1.5" as 1 and lose
/// the fraction silently. We never call `setlocale`, and neither does SDL, so we are
/// safe — but this is one of the great asset-pipeline bugs and it deserves naming
/// rather than luck. Floating-point `from_chars` was the last piece of C++17 to
/// reach the standard libraries; check `__cpp_lib_to_chars` before reaching for it.
/// Exercise 3.5.4.
[[nodiscard]] bool to_float(std::string_view token, float& out)
{
    char buffer[64];
    if (token.empty() || token.size() >= sizeof(buffer)) { return false; }
    std::memcpy(buffer, token.data(), token.size());
    buffer[token.size()] = '\0';

    char* stopped = nullptr;
    const float value = std::strtof(buffer, &stopped);

    // The whole token, or nothing. `strtof("1.0abc")` happily returns 1.0 and points
    // `stopped` at the 'a'; accepting that would mean a typo in a model file becomes
    // geometry instead of an error.
    if (stopped != buffer + token.size()) { return false; }
    if (!std::isfinite(value)) { return false; }

    out = value;
    return true;
}

/// Parse a signed decimal integer, strictly. Hand-rolled because it is six lines,
/// exact, locale-proof, and we need the sign anyway: OBJ indices may be negative.
[[nodiscard]] bool to_int(std::string_view token, int& out)
{
    if (token.empty()) { return false; }

    std::size_t i = 0;
    bool negative = false;
    if (token[0] == '-') { negative = true; i = 1; }
    else if (token[0] == '+') { i = 1; }
    if (i >= token.size()) { return false; }

    long long value = 0;
    for (; i < token.size(); ++i)
    {
        if (token[i] < '0' || token[i] > '9') { return false; }
        value = value * 10 + (token[i] - '0');
        // A guard rail, not a limit: no real file names element 100,000,000, and
        // stopping here means the accumulator cannot overflow no matter what the
        // file says. Signed overflow is undefined behaviour, so "it would be a
        // ridiculous number anyway" is not a defence.
        if (value > 100000000LL) { return false; }
    }

    out = static_cast<int>(negative ? -value : value);
    return true;
}

// ---- OBJ indices -----------------------------------------------------------

/// One face corner as the FILE writes it: raw OBJ indices, `0` meaning absent.
///
/// Zero is safe as the "absent" marker because **0 is not a legal OBJ index** —
/// the format is 1-based, so the numbering starts at 1 and negative values count
/// backwards. There is no valid element 0 for the sentinel to collide with.
struct corner_ref
{
    int position = 0;
    int uv = 0;
    int normal = 0;
};

/// Split `v`, `v/vt`, `v//vn` or `v/vt/vn` into its parts.
///
/// The empty middle field of `v//vn` is the whole reason this is not a `sscanf`
/// one-liner: "1//3" means position 1, NO texture coordinate, normal 3, and any
/// pattern-based parse that expects a number after the first slash mis-reads it.
[[nodiscard]] bool parse_corner(std::string_view token, corner_ref& out)
{
    out = corner_ref{};

    std::string_view field[3];
    int fields = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= token.size(); ++i)
    {
        if (i == token.size() || token[i] == '/')
        {
            if (fields == 3) { return false; }   // more than three: not OBJ
            field[fields++] = token.substr(start, i - start);
            start = i + 1;
        }
    }

    if (fields == 0 || field[0].empty()) { return false; }
    if (!to_int(field[0], out.position)) { return false; }
    if (fields >= 2 && !field[1].empty() && !to_int(field[1], out.uv)) { return false; }
    if (fields >= 3 && !field[2].empty() && !to_int(field[2], out.normal)) { return false; }
    return true;
}

/// Turn a raw OBJ index into a 0-based array index, or -1 if it names nothing.
///
/// Two rules, and getting either wrong corrupts the whole model quietly.
///
/// **1-BASED.** `f 1 2 3` means the FIRST three positions, which live at array
/// slots 0, 1 and 2. Forgetting this is the single most common OBJ bug; the model
/// still loads, every triangle is built from its neighbours' corners, and the result
/// looks like the mesh was hit with a hammer.
///
/// **NEGATIVE MEANS RELATIVE.** `-1` is the most recently defined element, `-2` the
/// one before it. This exists so that a file can be concatenated onto another
/// without renumbering, and exporters that emit per-object chunks do use it. With
/// `count` elements seen so far, `-1` must resolve to `count - 1`, which is exactly
/// `count + raw`.
[[nodiscard]] int resolve_index(int raw, int count)
{
    if (raw > 0) { return (raw <= count) ? raw - 1 : -1; }
    if (raw < 0) { const int i = count + raw; return (i >= 0) ? i : -1; }
    return -1;   // 0 is not a legal index in this format
}

// ---- The unified vertex ----------------------------------------------------

/// A resolved corner: three 0-based indices, `-1` for absent.
///
/// **This triple is the key to the whole lesson.** Two face corners are the same
/// vertex if and only if all three of their attribute indices match. Same position
/// but a different uv is a DIFFERENT vertex, because a vertex buffer fetches
/// position and uv together and cannot serve two answers from one slot.
struct corner_key
{
    int position = -1;
    int uv = -1;
    int normal = -1;

    [[nodiscard]] bool operator==(const corner_key&) const = default;
};

/// FNV-1a over the three indices.
///
/// Hand-written because the standard library has no hash for a user-defined struct
/// and never will — it cannot know which fields matter. FNV-1a is four lines, has no
/// table, and mixes well enough that the map's buckets stay short; a hash that
/// merely added the three fields would collide for every (p, t, n) and (t, p, n)
/// pair, turning the map into a linked list and the loader into an O(n²) crawl.
struct corner_hash
{
    [[nodiscard]] std::size_t operator()(const corner_key& k) const noexcept
    {
        std::uint64_t h = 1469598103934665603ull;              // FNV offset basis
        const auto mix = [&h](int value) {
            h ^= static_cast<std::uint32_t>(value);
            h *= 1099511628211ull;                             // FNV prime
        };
        mix(k.position);
        mix(k.uv);
        mix(k.normal);
        return static_cast<std::size_t>(h);
    }
};

} // namespace

const char* name_of(obj_status s)
{
    switch (s)
    {
    case obj_status::ok:                return "ok";
    case obj_status::cannot_open:       return "cannot open file";
    case obj_status::no_faces:          return "no faces in file";
    case obj_status::bad_face:          return "malformed face";
    case obj_status::bad_index:         return "index out of range";
    case obj_status::too_many_vertices: return "too many vertices for a uint16 index";
    }
    return "?";
}

obj_report parse_obj(std::string_view text, mesh_data& out)
{
    obj_report report;
    out.clear();

    // The file's three independent streams, exactly as written. These are NOT the
    // engine's arrays — they are the raw material the index problem consumes.
    std::vector<vec3> file_positions;
    std::vector<vec2> file_uvs;
    std::vector<vec3> file_normals;

    // (position, uv, normal) -> our vertex index. Reserved generously because the
    // rehash of a growing hash map is the most expensive thing in this function, and
    // "one vertex per position" is a floor we know we will exceed.
    std::unordered_map<corner_key, std::uint16_t, corner_hash> unified;

    std::vector<std::uint16_t> face_corners;   // reused per face; no per-face alloc
    face_corners.reserve(16);

    int line_number = 0;
    std::size_t cursor = 0;

    const auto fail = [&](obj_status s) {
        report.status = s;
        report.line = line_number;
        return report;
    };

    while (cursor <= text.size())
    {
        // ---- One line -----------------------------------------------------
        const std::size_t newline = text.find('\n', cursor);
        std::string_view line = (newline == std::string_view::npos)
                              ? text.substr(cursor)
                              : text.substr(cursor, newline - cursor);
        cursor = (newline == std::string_view::npos) ? text.size() + 1 : newline + 1;
        ++line_number;

        // CRLF. A file written on Windows ends every line with "\r\n", and the '\r'
        // rides along on the last token — so "0.5\r" reaches `to_float`, fails the
        // whole-token check, and the model refuses to load on one platform and not
        // the other. Two lines here; hours of confusion otherwise.
        while (!line.empty() && (line.back() == '\r' || is_blank(line.back())))
        {
            line.remove_suffix(1);
        }

        const std::string_view keyword = take_token(line);
        if (keyword.empty() || keyword[0] == '#') { continue; }   // blank or comment

        // ---- Geometry statements ------------------------------------------
        if (keyword == "v")
        {
            vec3 p{};
            if (!to_float(take_token(line), p.x)) { return fail(obj_status::bad_face); }
            if (!to_float(take_token(line), p.y)) { return fail(obj_status::bad_face); }
            if (!to_float(take_token(line), p.z)) { return fail(obj_status::bad_face); }
            // A `v` may carry a fourth rational-geometry weight, and some tools
            // append three more numbers as a vertex colour. Both are extensions we
            // do not use; trailing tokens are ignored rather than rejected, because
            // refusing to load a model over a field we do not need would be rude.
            file_positions.push_back(p);
            ++report.positions;
            continue;
        }

        if (keyword == "vt")
        {
            vec2 t{};
            if (!to_float(take_token(line), t.x)) { return fail(obj_status::bad_face); }
            // The second coordinate is OPTIONAL — a 1-D texture has only `u`. Rare,
            // legal, and a parser that demands two numbers falls over on it.
            const std::string_view second = take_token(line);
            if (!second.empty() && !to_float(second, t.y)) { return fail(obj_status::bad_face); }
            file_uvs.push_back(t);
            ++report.uvs;
            continue;
        }

        if (keyword == "vn")
        {
            vec3 n{};
            if (!to_float(take_token(line), n.x)) { return fail(obj_status::bad_face); }
            if (!to_float(take_token(line), n.y)) { return fail(obj_status::bad_face); }
            if (!to_float(take_token(line), n.z)) { return fail(obj_status::bad_face); }
            // NOT normalised here. The file says what it says, and silently changing
            // a value on the way in makes the loader a place where data can differ
            // from its source. Lesson 3.6 normalises where it shades.
            file_normals.push_back(n);
            ++report.normals;
            continue;
        }

        // ---- Faces: the whole lesson ---------------------------------------
        if (keyword == "f")
        {
            face_corners.clear();

            for (std::string_view token = take_token(line); !token.empty();
                 token = take_token(line))
            {
                corner_ref raw;
                if (!parse_corner(token, raw)) { return fail(obj_status::bad_face); }

                // Resolve against the counts SEEN SO FAR, which is what makes a
                // negative index mean what it means. It also means a positive index
                // pointing at an element defined later in the file fails here rather
                // than silently reading zeroes — a limitation named in obj.hpp.
                corner_key key;
                key.position = resolve_index(raw.position,
                                             static_cast<int>(file_positions.size()));
                if (key.position < 0) { return fail(obj_status::bad_index); }

                if (raw.uv != 0)
                {
                    key.uv = resolve_index(raw.uv, static_cast<int>(file_uvs.size()));
                    if (key.uv < 0) { return fail(obj_status::bad_index); }
                }
                if (raw.normal != 0)
                {
                    key.normal = resolve_index(raw.normal,
                                               static_cast<int>(file_normals.size()));
                    if (key.normal < 0) { return fail(obj_status::bad_index); }
                }

                // ---- THE INDEX PROBLEM, in a dozen lines ----------------
                //
                // Has this exact combination been seen before? If so it is the same
                // vertex and we reuse its index. If not it is a new vertex, even
                // when its position is one we already store — because the thing
                // being indexed downstream is the whole vertex, not the position.
                //
                // Written as find-then-insert rather than `try_emplace`, which would
                // be one lookup instead of two on a miss. The reason is the ceiling
                // check: `try_emplace` needs the new index as an ARGUMENT, so it
                // would compute `uint16_t(65536)` — a silent 0 — before we ever got
                // to test the limit. The value is never used, but code whose
                // correctness rests on "we return before that wrong number matters"
                // is code that breaks the next time somebody edits it.
                std::uint16_t index = 0;
                if (const auto found = unified.find(key); found != unified.end())
                {
                    index = found->second;
                    ++report.reused_corners;
                }
                else
                {
                    if (out.vertices.size() >= k_max_mesh_vertices)
                    {
                        return fail(obj_status::too_many_vertices);
                    }
                    index = static_cast<std::uint16_t>(out.vertices.size());
                    unified.emplace(key, index);

                    out.vertices.push_back(file_positions[
                        static_cast<std::size_t>(key.position)]);
                    // A uv and a normal for EVERY vertex, so the three arrays stay
                    // index-parallel — `mesh` requires it, and a ragged attribute
                    // array is a crash waiting for the first mesh that mixes face
                    // formats. If the file turned out to have no `vt` at all, the
                    // array is discarded wholesale at the end.
                    out.uvs.push_back(key.uv >= 0
                        ? file_uvs[static_cast<std::size_t>(key.uv)] : vec2{});
                    out.normals.push_back(key.normal >= 0
                        ? file_normals[static_cast<std::size_t>(key.normal)] : vec3{});
                }

                face_corners.push_back(index);
            }

            if (face_corners.size() < 3) { return fail(obj_status::bad_face); }

            ++report.faces;
            const int corners = static_cast<int>(face_corners.size());
            if (corners > report.max_corners) { report.max_corners = corners; }
            if (corners > 3) { ++report.ngons; }

            // ---- Triangulate by FANNING from corner 0 -------------------
            //
            // (0, k-1, k) for k = 2 … n-1, which is exactly the fan Lesson 3.3's
            // clipper emits — and it makes exactly the same assumption, which is
            // worth stating precisely rather than inheriting: **a fan is correct if
            // and only if corner 0 can SEE the whole polygon** (star-shaped about
            // it). Convexity is the sufficient condition people quote, and it is
            // sufficient because in a convex polygon every corner sees everything.
            // A concave face may fan perfectly well from one corner and grow fins
            // from another — so the bug depends on where the exporter started
            // listing, which is what makes it intermittent across files.
            //
            // Quads from a modelling tool are reliably convex (usually planar too);
            // a hand-authored n-gon may not be. Robust triangulation is ear
            // clipping, which needs the polygon's plane and a containment test —
            // Module 5's asset pipeline.
            for (std::size_t k = 2; k < face_corners.size(); ++k)
            {
                const std::uint16_t a = face_corners[0];
                const std::uint16_t b = face_corners[k - 1];
                const std::uint16_t c = face_corners[k];

                // A face that names the same vertex twice produces a triangle with
                // no area. Real files contain these — usually from a merge that
                // collapsed two points — and they are not an error in the FILE, so
                // they are dropped and counted rather than fatal. The distinction
                // this loader draws throughout: **malformed is fatal, silly is not.**
                if (a == b || b == c || a == c) { ++report.degenerate; continue; }

                out.indices.push_back(a);
                out.indices.push_back(b);
                out.indices.push_back(c);
                ++report.triangles;
            }
            continue;
        }

        // ---- Everything else ------------------------------------------------
        if (keyword == "o" || keyword == "g" || keyword == "s"
            || keyword == "usemtl" || keyword == "mtllib"
            || keyword == "l" || keyword == "p")
        {
            ++report.skipped_lines;
            continue;
        }

        // An unknown keyword is COUNTED, not fatal. OBJ has a long tail of
        // extensions and a loader that dies on the first one it has never seen is
        // useless in practice. The count is the honest part: it lets the report say
        // "I ignored 412 lines of this file", which is a very different statement
        // from silence.
        ++report.unknown_lines;
    }

    if (report.faces == 0) { return fail(obj_status::no_faces); }

    // If the file carried no texture coordinates or no normals, drop the arrays of
    // zeroes we built alongside the positions. `mesh` treats empty as "absent", and
    // absent is the truth — a mesh full of (0,0) uvs would claim to have texture
    // coordinates and quietly map every pixel to one texel.
    if (report.uvs == 0) { out.uvs.clear(); out.uvs.shrink_to_fit(); }
    if (report.normals == 0) { out.normals.clear(); out.normals.shrink_to_fit(); }

    report.vertices = static_cast<int>(out.vertices.size());
    report.split_vertices = report.vertices - report.positions;
    report.status = obj_status::ok;
    report.line = 0;
    return report;
}

obj_report load_obj(const char* path, mesh_data& out)
{
    obj_report report;
    out.clear();

    std::size_t size = 0;
    // SDL_LoadFile allocates, reads the whole file, and appends a zero byte that is
    // NOT counted in `size`. The zero costs nothing and is the reason a `string_view`
    // over this buffer is safe to hand to a parser that may look one past a token.
    void* data = SDL_LoadFile(path, &size);
    if (data == nullptr)
    {
        report.status = obj_status::cannot_open;
        return report;
    }

    report = parse_obj(std::string_view(static_cast<const char*>(data), size), out);

    // SDL_free, not delete or free: memory allocated inside SDL must be returned to
    // SDL's allocator. On Windows a DLL can be linked against a different C runtime
    // than the program, and freeing across that boundary corrupts the heap.
    SDL_free(data);
    return report;
}

bool save_obj(const char* path, const mesh& m)
{
    // ---- Compact each attribute stream separately ------------------------
    //
    // The mirror image of loading. Our vertices are unified — position, uv and
    // normal travelling together — and a seam split means several of them share a
    // position exactly. An exporter writes each DISTINCT value once and lets the
    // faces name them independently, which is both smaller and what every other tool
    // produces. Doing it here is what makes the round trip a real test: the file we
    // write has the same shape as a file we did not.
    std::unordered_map<std::uint64_t, int> seen_position;
    std::unordered_map<std::uint64_t, int> seen_uv;
    std::unordered_map<std::uint64_t, int> seen_normal;

    std::vector<int> position_of(m.vertices.size(), 0);
    std::vector<int> uv_of(m.vertices.size(), 0);
    std::vector<int> normal_of(m.vertices.size(), 0);

    std::string text;
    text.reserve(m.vertices.size() * 96 + m.indices.size() * 12);

    char buffer[192];
    const auto emit = [&](int written) {
        if (written > 0) { text.append(buffer, static_cast<std::size_t>(written)); }
    };

    // A key from exact float bits, so two values are "the same" only when they are
    // bit-identical — the same rule `validate()` welds by, and the right one here:
    // the duplicates we are compacting came from one computation, so they match
    // exactly or they are genuinely different numbers.
    const auto key2 = [](vec2 v) {
        std::uint32_t a = 0;
        std::uint32_t b = 0;
        const float x = v.x + 0.0f;   // normalise -0.0 to +0.0
        const float y = v.y + 0.0f;
        std::memcpy(&a, &x, 4);
        std::memcpy(&b, &y, 4);
        return (static_cast<std::uint64_t>(a) << 32) | b;
    };
    const auto key3 = [](vec3 v) {
        std::uint32_t a = 0;
        std::uint32_t b = 0;
        std::uint32_t c = 0;
        const float x = v.x + 0.0f;
        const float y = v.y + 0.0f;
        const float z = v.z + 0.0f;
        std::memcpy(&a, &x, 4);
        std::memcpy(&b, &y, 4);
        std::memcpy(&c, &z, 4);
        // Three 32-bit words folded into 64 — a hash, so collisions are possible in
        // principle. They cost nothing but a duplicated line in the file, because
        // the map is only ever used to AVOID writing a value twice; a false match
        // would be a correctness bug, so the fold mixes rather than truncates.
        std::uint64_t h = (static_cast<std::uint64_t>(a) << 32) | b;
        h ^= static_cast<std::uint64_t>(c) * 1099511628211ull;
        return h;
    };

    text += "# Written by the engine's save_obj (Lesson 3.5).\n";
    emit(std::snprintf(buffer, sizeof(buffer), "# %zu vertices, %zu triangles\n",
                       m.vertices.size(), m.triangle_count()));
    text += "o mesh\n";

    for (std::size_t i = 0; i < m.vertices.size(); ++i)
    {
        const vec3 p = m.vertices[i];
        const auto [entry, inserted] =
            seen_position.try_emplace(key3(p), static_cast<int>(seen_position.size()) + 1);
        position_of[i] = entry->second;
        if (inserted)
        {
            // "%.9g" — NINE significant digits, which is the number that makes a
            // float survive a round trip through decimal text exactly (FLT_DECIMAL_DIG).
            // Fewer is prettier and loses bits; more is noise. The exactness is what
            // lets the round-trip test assert equality instead of "close enough".
            emit(std::snprintf(buffer, sizeof(buffer), "v %.9g %.9g %.9g\n",
                               static_cast<double>(p.x), static_cast<double>(p.y),
                               static_cast<double>(p.z)));
        }
    }

    if (!m.uvs.empty())
    {
        for (std::size_t i = 0; i < m.vertices.size(); ++i)
        {
            const vec2 t = m.uv_at(i);
            const auto [entry, inserted] =
                seen_uv.try_emplace(key2(t), static_cast<int>(seen_uv.size()) + 1);
            uv_of[i] = entry->second;
            if (inserted)
            {
                emit(std::snprintf(buffer, sizeof(buffer), "vt %.9g %.9g\n",
                                   static_cast<double>(t.x), static_cast<double>(t.y)));
            }
        }
    }

    if (!m.normals.empty())
    {
        for (std::size_t i = 0; i < m.vertices.size(); ++i)
        {
            const vec3 n = m.normal_at(i);
            const auto [entry, inserted] =
                seen_normal.try_emplace(key3(n), static_cast<int>(seen_normal.size()) + 1);
            normal_of[i] = entry->second;
            if (inserted)
            {
                emit(std::snprintf(buffer, sizeof(buffer), "vn %.9g %.9g %.9g\n",
                                   static_cast<double>(n.x), static_cast<double>(n.y),
                                   static_cast<double>(n.z)));
            }
        }
    }

    const bool have_uvs = !m.uvs.empty();
    const bool have_normals = !m.normals.empty();

    for (std::size_t f = 0; f < m.triangle_count(); ++f)
    {
        text += 'f';
        for (int corner = 0; corner < 3; ++corner)
        {
            const std::size_t v = m.indices[f * 3 + static_cast<std::size_t>(corner)];
            if (v >= m.vertices.size()) { continue; }

            if (have_uvs && have_normals)
            {
                emit(std::snprintf(buffer, sizeof(buffer), " %d/%d/%d",
                                   position_of[v], uv_of[v], normal_of[v]));
            }
            else if (have_normals)
            {
                // `v//vn` — the double slash is not a typo, it is how OBJ says
                // "no texture coordinate here".
                emit(std::snprintf(buffer, sizeof(buffer), " %d//%d",
                                   position_of[v], normal_of[v]));
            }
            else if (have_uvs)
            {
                emit(std::snprintf(buffer, sizeof(buffer), " %d/%d",
                                   position_of[v], uv_of[v]));
            }
            else
            {
                emit(std::snprintf(buffer, sizeof(buffer), " %d", position_of[v]));
            }
        }
        text += '\n';
    }

    return SDL_SaveFile(path, text.data(), text.size());
}

std::string asset_path(const char* relative)
{
    // SDL3 returns a CACHED const char* here and the caller must not free it —
    // unlike SDL2, where SDL_GetBasePath returned memory you owned. It can be null
    // on platforms that cannot answer, in which case we fall back to a plain
    // relative path and accept that it depends on the working directory.
    const char* base = SDL_GetBasePath();

    std::string path = (base != nullptr) ? std::string(base) : std::string();
    path += "assets/";
    path += relative;
    return path;
}

} // namespace engine
