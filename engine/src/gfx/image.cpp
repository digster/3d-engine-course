// engine/src/gfx/image.cpp — the one translation unit that contains stb_image.
//
// Lesson 4.7. `STB_IMAGE_IMPLEMENTATION` turns the header from declarations into
// definitions, and it must be defined in EXACTLY ONE .cpp in the whole program.
// Do it in two and the linker reports every function twice; do it in none and it
// reports every function missing. Isolating it here means no other file in the
// engine ever includes stb_image at all — they include `image.hpp`, which
// mentions no third-party type in its interface.
//
// That isolation is the point of this file existing rather than the decode
// happening at the call site. A third-party library reaches exactly as far into a
// codebase as its types appear in headers, and stb's reach is this file.

#include <engine/gfx/image.hpp>

#include <engine/core/log.hpp>
#include <engine/gfx/framebuffer.hpp>

#include <SDL3/SDL.h>

#include <vector>

// Turn off the parts we do not want, before the implementation is generated.
//
// STBI_NO_STDIO removes the FILE* API, which we do not use: SDL_LoadFile already
// reads a whole file and is the one path in this engine that knows about
// SDL_GetBasePath. Feeding stb the bytes we already have avoids a second, subtly
// different notion of where a file lives.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP

// Third-party code, compiled with our warning flags, which it was never written
// for. Silence the diagnostics it trips over rather than editing it — an edited
// dependency is one you can no longer update.
#if defined(__clang__) || defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wsign-compare"
#  pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable : 4244 4996)
#endif

#include "stb_image.h"

#if defined(__clang__) || defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

namespace engine {

const char* name_of(image_status s)
{
    switch (s)
    {
    case image_status::ok:          return "ok";
    case image_status::cannot_open: return "cannot open";
    case image_status::bad_format:  return "not an image we can decode";
    case image_status::too_large:   return "larger than we are willing to decode";
    }
    return "unknown";
}

image_report load_image(const char* path, image_data& out)
{
    out = image_data{};
    image_report report;

    if (path == nullptr)
    {
        ENGINE_LOG_ERROR(log_asset, "load_image: null path");
        return report;   // status is cannot_open, the struct's default
    }

    // Read the bytes with SDL, not with stb. One notion of "where files are"
    // (Lesson 3.5's `asset_path`), one error style, one place that knows the
    // returned pointer must be freed with SDL_free.
    std::size_t file_bytes = 0;
    void* file = SDL_LoadFile(path, &file_bytes);
    if (file == nullptr)
    {
        ENGINE_LOG_ERROR(log_asset, "load_image: cannot read %s (%s)", path, SDL_GetError());
        return report;
    }
    report.file_bytes = file_bytes;

    int w = 0;
    int h = 0;
    int channels_in_file = 0;

    // Ask what is in there BEFORE decoding, so an absurd size is refused rather
    // than allocated. stbi_info_from_memory does not decode; it reads the header.
    if (stbi_info_from_memory(static_cast<const stbi_uc*>(file),
                              static_cast<int>(file_bytes), &w, &h, &channels_in_file) == 0)
    {
        ENGINE_LOG_ERROR(log_asset, "load_image: %s is not a format we decode (%s)",
                         path, stbi_failure_reason());
        SDL_free(file);
        report.status = image_status::bad_format;
        return report;
    }

    // Recorded before the size check, so a REJECTED image still reports what it
    // was. "too_large" with no dimensions makes you open the file yourself to
    // find out how large; a report that describes the thing it refused is the
    // difference between a diagnosis and a complaint.
    report.width = w;
    report.height = h;
    report.source_channels = channels_in_file;

    if (w <= 0 || h <= 0
        || static_cast<std::size_t>(w) * static_cast<std::size_t>(h) > k_max_image_texels)
    {
        ENGINE_LOG_ERROR(log_asset, "load_image: %s is %dx%d, beyond the %zu-texel ceiling",
                         path, w, h, k_max_image_texels);
        SDL_free(file);
        report.status = image_status::too_large;
        return report;
    }

    // The 4 is not a preference, it is the whole point: whatever the file held,
    // what comes out is RGBA8, so nothing downstream branches on the file's shape.
    stbi_uc* decoded = stbi_load_from_memory(static_cast<const stbi_uc*>(file),
                                             static_cast<int>(file_bytes),
                                             &w, &h, &channels_in_file, 4);
    SDL_free(file);

    if (decoded == nullptr)
    {
        ENGINE_LOG_ERROR(log_asset, "load_image: %s failed to decode (%s)",
                         path, stbi_failure_reason());
        report.status = image_status::bad_format;
        return report;
    }

    const std::size_t bytes = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u;
    out.pixels.assign(decoded, decoded + bytes);
    out.width = w;
    out.height = h;
    out.source_channels = channels_in_file;

    // stb allocated it, so stb frees it. The copy above is deliberate: the engine
    // owns a std::vector like everything else, and the third-party allocation
    // does not outlive this function.
    stbi_image_free(decoded);

    report.bytes = bytes;
    report.status = image_status::ok;

    // DEBUG, not INFO: this is "what the subsystem did", one line per asset, and
    // a level with four hundred textures in it should not print four hundred
    // lines unless somebody asked. `--log asset=debug` asks.
    ENGINE_LOG_DEBUG(log_asset, "load_image: %s  %dx%d  %d->4 channels  "
                     "%zu bytes from %zu on disk (%.1fx)",
                     path, w, h, channels_in_file, report.bytes, report.file_bytes,
                     report.file_bytes ? static_cast<double>(report.bytes)
                                       / static_cast<double>(report.file_bytes) : 0.0);
    return report;
}

bool save_ppm(const framebuffer& fb, const char* path)
{
    if (path == nullptr) { return false; }

    SDL_IOStream* const io = SDL_IOFromFile(path, "wb");
    if (io == nullptr)
    {
        ENGINE_LOG_ERROR(engine::log_asset, "save_ppm: cannot write %s: %s", path, SDL_GetError());
        return false;
    }

    // P6 is the binary flavour: "P6", width, height, and the maximum channel
    // value, whitespace-separated, then the pixel bytes with no separator at all.
    char header[32];
    const int header_len = SDL_snprintf(header, sizeof(header), "P6\n%d %d\n255\n",
                                        fb.width(), fb.height());

    bool ok = SDL_WriteIO(io, header, static_cast<std::size_t>(header_len))
              == static_cast<std::size_t>(header_len);

    // One buffered row at a time rather than one SDL_WriteIO per pixel. At
    // 320x180 the difference is 57,600 write calls against 180 of them, and the
    // rule generalises: crossing an I/O boundary is expensive per CROSSING, not
    // per byte.
    std::vector<Uint8> row(static_cast<std::size_t>(fb.width()) * 3u);

    for (int y = 0; ok && y < fb.height(); ++y)
    {
        for (int x = 0; x < fb.width(); ++x)
        {
            const Uint32 px = fb.pixel_at(x, y);
            const std::size_t o = static_cast<std::size_t>(x) * 3u;
            row[o + 0] = red_of(px);
            row[o + 1] = green_of(px);
            row[o + 2] = blue_of(px);
        }
        ok = SDL_WriteIO(io, row.data(), row.size()) == row.size();
    }

    if (!SDL_CloseIO(io)) { ok = false; }

    if (!ok) { ENGINE_LOG_ERROR(engine::log_asset, "save_ppm: short write to %s: %s", path, SDL_GetError()); }
    return ok;
}

} // namespace engine
