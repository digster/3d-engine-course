// src/gfx/image.cpp — the one translation unit that contains stb_image.
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

#include "gfx/image.hpp"

#include <SDL3/SDL.h>

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

image_status load_image(const char* path, image_data& out)
{
    out = image_data{};

    if (path == nullptr) { return image_status::cannot_open; }

    // Read the bytes with SDL, not with stb. One notion of "where files are"
    // (Lesson 3.5's `asset_path`), one error style, one place that knows the
    // returned pointer must be freed with SDL_free.
    std::size_t file_bytes = 0;
    void* file = SDL_LoadFile(path, &file_bytes);
    if (file == nullptr)
    {
        SDL_Log("load_image: cannot read %s (%s)", path, SDL_GetError());
        return image_status::cannot_open;
    }

    int w = 0;
    int h = 0;
    int channels_in_file = 0;

    // Ask what is in there BEFORE decoding, so an absurd size is refused rather
    // than allocated. stbi_info_from_memory does not decode; it reads the header.
    if (stbi_info_from_memory(static_cast<const stbi_uc*>(file),
                              static_cast<int>(file_bytes), &w, &h, &channels_in_file) == 0)
    {
        SDL_Log("load_image: %s is not a format we decode (%s)", path, stbi_failure_reason());
        SDL_free(file);
        return image_status::bad_format;
    }

    if (w <= 0 || h <= 0
        || static_cast<std::size_t>(w) * static_cast<std::size_t>(h) > k_max_image_texels)
    {
        SDL_Log("load_image: %s is %dx%d, beyond the %zu-texel ceiling",
                path, w, h, k_max_image_texels);
        SDL_free(file);
        return image_status::too_large;
    }

    // The 4 is not a preference, it is the whole point: whatever the file held,
    // what comes out is RGBA8, so nothing downstream branches on the file's shape.
    stbi_uc* decoded = stbi_load_from_memory(static_cast<const stbi_uc*>(file),
                                             static_cast<int>(file_bytes),
                                             &w, &h, &channels_in_file, 4);
    SDL_free(file);

    if (decoded == nullptr)
    {
        SDL_Log("load_image: %s failed to decode (%s)", path, stbi_failure_reason());
        return image_status::bad_format;
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

    return image_status::ok;
}

} // namespace engine
