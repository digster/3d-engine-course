// engine/src/asset/search_path.cpp — names into paths.

#include <engine/asset/search_path.hpp>

#include <engine/core/log.hpp>

#include <SDL3/SDL_filesystem.h>

namespace engine {
namespace {

/// Would this name escape the root it is joined to?
///
/// Rejects any component that is exactly `..`, and any absolute path. Deliberately
/// a *syntactic* check rather than a canonicalising one: canonicalisation needs the
/// filesystem, differs per platform, and is exactly the sort of clever code that
/// grows a bypass. A name with `..` in it is not a name we want to serve, whether
/// or not it happens to resolve somewhere harmless today.
[[nodiscard]] bool escapes(std::string_view name)
{
    if (name.empty()) { return true; }
    if (name.front() == '/' || name.front() == '\\') { return true; }

    // Windows drive letters: "C:\..." is absolute even without a leading slash.
    if (name.size() >= 2 && name[1] == ':') { return true; }

    std::size_t start = 0;
    while (start <= name.size())
    {
        std::size_t end = name.find_first_of("/\\", start);
        if (end == std::string_view::npos) { end = name.size(); }
        if (name.substr(start, end - start) == "..") { return true; }
        start = end + 1;
    }
    return false;
}

/// Join a root and a name, inserting exactly one separator.
[[nodiscard]] std::string join(std::string_view root, std::string_view name)
{
    std::string out(root);
    if (!out.empty() && out.back() != '/' && out.back() != '\\') { out += '/'; }
    out.append(name);
    return out;
}

}   // namespace

search_path search_path::beside_executable(std::string_view subdir)
{
    search_path sp;

    // SDL3 returns a CACHED const char* here and the caller must NOT free it —
    // unlike SDL2, where SDL_GetBasePath returned memory you owned. It can be
    // null on platforms that cannot answer, in which case we fall back to a plain
    // relative root and accept that it depends on the working directory. That is
    // a worse answer, and it is better than no answer.
    const char* base = SDL_GetBasePath();
    sp.add_root(base != nullptr ? join(base, subdir) : std::string(subdir));
    return sp;
}

search_path search_path::standard()
{
    return beside_executable("assets");
}

void search_path::add_root(std::string dir)
{
    roots_.push_back(std::move(dir));
}

void search_path::prepend_root(std::string dir)
{
    roots_.insert(roots_.begin(), std::move(dir));
}

resolved_path search_path::resolve(std::string_view name) const
{
    if (escapes(name))
    {
        ENGINE_LOG_ERROR(log_asset, "search_path: refusing name '%.*s' — absolute, "
                                    "empty, or containing '..'",
                         static_cast<int>(name.size()), name.data());
        resolved_path bad;
        bad.refused = true;
        return bad;
    }

    for (std::size_t i = 0; i < roots_.size(); ++i)
    {
        std::string candidate = join(roots_[i], name);

        SDL_PathInfo info{};
        if (SDL_GetPathInfo(candidate.c_str(), &info) && info.type == SDL_PATHTYPE_FILE)
        {
            // A DIRECTORY that happens to share the name is not a hit. Without
            // the type test, `resolve("shaders")` would report success and the
            // caller would fail later, in a loader, with a much worse message.
            resolved_path found;
            found.path = std::move(candidate);
            found.root_index = static_cast<int>(i);
            found.bytes = info.size;
            return found;
        }
    }

    // NOT an error. A name that is not on disk is an ordinary thing to ask about
    // — `asset_store` uses this to decide whether to generate content instead —
    // so the failure is reported to the caller and logged only at debug. The
    // caller knows whether it is a problem; we do not.
    ENGINE_LOG_DEBUG(log_asset, "search_path: '%.*s' not found in %zu root(s)",
                     static_cast<int>(name.size()), name.data(), roots_.size());
    return {};
}

}   // namespace engine
