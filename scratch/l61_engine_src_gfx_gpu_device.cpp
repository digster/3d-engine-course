// engine/src/gfx/gpu_device.cpp — creating the device, claiming the window, and asking
// the device what it turned out to be.

#include <engine/gfx/gpu_device.hpp>

#include <engine/core/log.hpp>

#include <cstdio>
#include <cstring>
#include <utility>

namespace engine {

const char* name_of(gpu_status s)
{
    switch (s)
    {
    case gpu_status::ok:                 return "ok";
    case gpu_status::no_device:          return "no GPU device could be created";
    case gpu_status::window_not_claimed: return "the window could not be claimed";
    }
    return "unknown";
}

const char* name_of(SDL_GPUTextureFormat f)
{
    // Not a complete table — there are dozens of formats and the point of this
    // function is a readable log line. Started as "the formats a swapchain
    // realistically arrives as" (Lesson 4.2) and gained the depth formats in
    // Lesson 4.7, which is the natural way for a table like this to grow: when a
    // lesson starts printing something, it adds the row.
    switch (f)
    {
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:      return "B8G8R8A8_UNORM";
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:      return "R8G8B8A8_UNORM";
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB: return "B8G8R8A8_UNORM_SRGB";
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB: return "R8G8B8A8_UNORM_SRGB";
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT:  return "R16G16B16A16_FLOAT";
    case SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM:   return "R10G10B10A2_UNORM";

    // Lesson 4.7's depth formats. Only D16_UNORM is guaranteed by SDL; the rest
    // are asked for with SDL_GPUTextureSupportsFormat.
    case SDL_GPU_TEXTUREFORMAT_D16_UNORM:           return "D16_UNORM";
    case SDL_GPU_TEXTUREFORMAT_D24_UNORM:           return "D24_UNORM";
    case SDL_GPU_TEXTUREFORMAT_D32_FLOAT:           return "D32_FLOAT";
    case SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT:   return "D24_UNORM_S8_UINT";
    case SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT:   return "D32_FLOAT_S8_UINT";

    case SDL_GPU_TEXTUREFORMAT_INVALID:             return "INVALID";
    default:                                        return "(other)";
    }
}

const char* name_of(SDL_GPUSwapchainComposition c)
{
    // Lesson 6.1. The names are short because a log line is where they appear,
    // but the SEMANTICS are the whole lesson and belong beside them:
    //   SDR         8-bit swapchain whose values ARE sRGB codes  -> we encode
    //   SDR_LINEAR  _SRGB swapchain, shaders see linear          -> it encodes
    //   HDR_*       float / 10-bit, values outside [0,1] allowed -> Module 6 later
    switch (c)
    {
    case SDL_GPU_SWAPCHAINCOMPOSITION_SDR:                 return "SDR";
    case SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR:          return "SDR_LINEAR";
    case SDL_GPU_SWAPCHAINCOMPOSITION_HDR_EXTENDED_LINEAR: return "HDR_EXTENDED_LINEAR";
    case SDL_GPU_SWAPCHAINCOMPOSITION_HDR10_ST2084:        return "HDR10_ST2084";
    default:                                               return "(other)";
    }
}

bool is_srgb_format(SDL_GPUTextureFormat f)
{
    return f == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB
        || f == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
}

void format_shader_formats(SDL_GPUShaderFormat mask, char* out, std::size_t cap)
{
    if (cap == 0) { return; }
    out[0] = '\0';

    // Parallel arrays rather than a switch, because this function joins several
    // names and a switch would have to be called from a loop anyway.
    const SDL_GPUShaderFormat bits[] = {
        SDL_GPU_SHADERFORMAT_PRIVATE, SDL_GPU_SHADERFORMAT_SPIRV,
        SDL_GPU_SHADERFORMAT_DXBC,    SDL_GPU_SHADERFORMAT_DXIL,
        SDL_GPU_SHADERFORMAT_MSL,     SDL_GPU_SHADERFORMAT_METALLIB
    };
    const char* names[] = {"PRIVATE", "SPIRV", "DXBC", "DXIL", "MSL", "METALLIB"};

    bool first = true;
    for (int i = 0; i < 6; ++i)
    {
        if ((mask & bits[i]) == 0u) { continue; }
        if (!first) { SDL_strlcat(out, " | ", cap); }
        SDL_strlcat(out, names[i], cap);
        first = false;
    }
    if (first) { SDL_strlcat(out, "(none)", cap); }
}

gpu_device::~gpu_device()
{
    destroy();
}

gpu_device::gpu_device(gpu_device&& other) noexcept
    : device_(other.device_), window_(other.window_), report_(other.report_)
{
    // The moved-from object must not release what it no longer owns. Forgetting
    // these three lines gives a double free that only fires when a device is
    // moved — i.e. never in testing and always in the field.
    other.device_ = nullptr;
    other.window_ = nullptr;
    other.report_ = {};
}

gpu_device& gpu_device::operator=(gpu_device&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        device_ = other.device_;
        window_ = other.window_;
        report_ = other.report_;
        other.device_ = nullptr;
        other.window_ = nullptr;
        other.report_ = {};
    }
    return *this;
}

gpu_report gpu_device::create(SDL_Window* window, bool debug)
{
    destroy();

    gpu_report r;

    // What we can supply, not what we want. SDL picks a backend that can consume
    // at least one of these, so naming all three is how one binary runs on Metal,
    // Vulkan and D3D12 — Lesson 4.3 compiles all three from one HLSL source.
    r.asked = SDL_GPU_SHADERFORMAT_SPIRV
            | SDL_GPU_SHADERFORMAT_DXIL
            | SDL_GPU_SHADERFORMAT_MSL;

    // The third parameter names a specific backend ("vulkan", "metal",
    // "direct3d12"). nullptr means "you choose", which is what we want everywhere
    // except when reproducing a driver bug.
    device_ = SDL_CreateGPUDevice(r.asked, debug, nullptr);
    if (device_ == nullptr)
    {
        r.status = gpu_status::no_device;
        ENGINE_LOG_ERROR(engine::log_gpu, "SDL_CreateGPUDevice failed: %s", SDL_GetError());
        report_ = r;
        return r;
    }

    // A null window means "no window, and none wanted" — Lesson 4.3. SDL_gpu.h
    // states that rendering entirely offscreen is supported, and a headless
    // device is how the shader harness runs with nothing on screen.
    if (window != nullptr)
    {
        if (!SDL_ClaimWindowForGPUDevice(device_, window))
        {
            // Leave nothing half-built. A device with no window is a perfectly
            // legal object, but it is not what THIS caller asked for, and
            // returning it would make `ok()` a lie.
            ENGINE_LOG_ERROR(engine::log_gpu, "SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
            SDL_DestroyGPUDevice(device_);
            device_ = nullptr;
            r.status = gpu_status::window_not_claimed;
            report_ = r;
            return r;
        }
        window_ = window;
    }

    // ---- Now ask the device what it is ------------------------------------
    r.status = gpu_status::ok;
    r.driver = SDL_GetGPUDeviceDriver(device_);
    r.granted = SDL_GetGPUShaderFormats(device_);

    // Everything below is a property of the PAIRING, so with no window there is
    // nothing to ask and the fields keep their defaults. Asking anyway would mean
    // calling swapchain functions with a null window, which is not a question
    // with an answer.
    if (window != nullptr)
    {
        r.supports_immediate =
            SDL_WindowSupportsGPUPresentMode(device_, window, SDL_GPU_PRESENTMODE_IMMEDIATE);
        r.supports_mailbox =
            SDL_WindowSupportsGPUPresentMode(device_, window, SDL_GPU_PRESENTMODE_MAILBOX);

        // ---- LESSON 6.1: ASK FOR A SWAPCHAIN THAT ENCODES FOR US ------------
        //
        // SDL claims a window with SDL_GPU_SWAPCHAINCOMPOSITION_SDR, and its
        // header says exactly what that means: "B8G8R8A8 or R8G8B8A8 swapchain.
        // **Pixel values are in sRGB encoding.**" A shader writing there is
        // writing CODES, not light.
        //
        // Our fragment shader returns light — `base * (incoming + ambient) + …`
        // is a quantity of light and has been since Lesson 3.6. From Module 4
        // until this lesson those linear values went into an SDR swapchain
        // untouched, and the display read them as codes. THE PICTURE WAS TOO
        // DARK, worst in the shadows: linear 0.05 was shown as 0.0040, which is
        // 12.4x too dark, while linear 0.8 was shown as 0.60, only 1.3x off. That
        // shape is why it survived four modules of looking at it — the bright
        // half looks nearly right and the dark half looks "moody".
        //
        // SDR_LINEAR is the fix and it is one call: the swapchain becomes an
        // _SRGB format, so the hardware applies the transfer function on write
        // and the shader goes on writing light. Free, exact, and done in the
        // fixed-function blend/write stage where it belongs.
        //
        // ASKED FOR, NEVER ASSUMED. SDL guarantees only SDR, so a machine may
        // refuse — and `output_encodes_in_hardware` below is how the renderer
        // finds out that it has to encode in the shader instead. Both paths are
        // real and both are tested (verify_61 §E, §F).
        if (SDL_WindowSupportsGPUSwapchainComposition(
                device_, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)
            && SDL_SetGPUSwapchainParameters(device_, window,
                                             SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
                                             SDL_GPU_PRESENTMODE_VSYNC))
        {
            r.composition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR;
        }
        else
        {
            // Not an error, and deliberately not logged as one: SDR is a valid
            // machine, and the engine has a correct path for it. What would be an
            // error is not NOTICING, which is what the flag below prevents.
            r.composition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
        }

        // Re-ask rather than assume: the format is a consequence of the
        // composition, and reading it back is how we learn whether the request
        // actually took effect.
        r.swapchain_format = SDL_GetGPUSwapchainTextureFormat(device_, window);
        r.output_encodes_in_hardware = is_srgb_format(r.swapchain_format);
    }
    r.frames_in_flight = 2;   // SDL's documented default at device creation

    report_ = r;
    return r;
}

void gpu_device::destroy()
{
    if (device_ == nullptr) { return; }

    // Order matters and is the reason this class exists. Release the claim first:
    // the swapchain belongs to the pairing, not to either party, and destroying
    // the device while a window still references it is undefined.
    if (window_ != nullptr)
    {
        SDL_ReleaseWindowFromGPUDevice(device_, window_);
        window_ = nullptr;
    }

    SDL_DestroyGPUDevice(device_);
    device_ = nullptr;
    report_ = {};
}

bool gpu_device::set_present_mode(SDL_GPUPresentMode mode)
{
    if (device_ == nullptr || window_ == nullptr) { return false; }

    if (!SDL_WindowSupportsGPUPresentMode(device_, window_, mode))
    {
        // Not an error worth logging as one: asking for a mode and being told no
        // is the supported way to find out. The caller decides what to do next.
        return false;
    }

    // THE COMPOSITION IS CARRIED, NOT RESET — Lesson 6.1's bug, in miniature.
    // SDL_SetGPUSwapchainParameters sets BOTH parameters, so passing a literal
    // SDR here (which this function did until 6.1) would silently undo the
    // linear swapchain the moment anybody changed the present mode. A function
    // that takes two parameters and is called to change one of them must pass
    // the other one through.
    if (!SDL_SetGPUSwapchainParameters(device_, window_, report_.composition, mode))
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "SDL_SetGPUSwapchainParameters failed: %s", SDL_GetError());
        return false;
    }

    // The format can change with the parameters, so re-ask rather than assume.
    report_.swapchain_format = SDL_GetGPUSwapchainTextureFormat(device_, window_);
    report_.output_encodes_in_hardware = is_srgb_format(report_.swapchain_format);
    return true;
}

bool gpu_device::set_frames_in_flight(Uint32 frames)
{
    if (device_ == nullptr) { return false; }
    if (frames < 1 || frames > 3) { return false; }   // SDL's documented range

    if (!SDL_SetGPUAllowedFramesInFlight(device_, frames))
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "SDL_SetGPUAllowedFramesInFlight(%u) failed: %s", frames, SDL_GetError());
        return false;
    }
    report_.frames_in_flight = frames;
    return true;
}

void gpu_device::log_report() const
{
    const gpu_report& r = report_;

    ENGINE_LOG_INFO(engine::log_gpu, "GPU device: %s", name_of(r.status));
    if (!r.ok()) { return; }

    ENGINE_LOG_INFO(engine::log_gpu, "  driver          : %s", r.driver);

    const int drivers = SDL_GetNumGPUDrivers();
    for (int i = 0; i < drivers; ++i)
    {
        const char* name = SDL_GetGPUDriver(i);
        ENGINE_LOG_INFO(engine::log_gpu, "  driver [%d]      : %s%s", i, name,
                (name != nullptr && r.driver != nullptr
                 && SDL_strcmp(name, r.driver) == 0) ? "   <- chosen" : "");
    }

    char asked[64];
    char granted[64];
    format_shader_formats(r.asked, asked, sizeof(asked));
    format_shader_formats(r.granted, granted, sizeof(granted));
    ENGINE_LOG_INFO(engine::log_gpu, "  shaders asked   : %s", asked);
    ENGINE_LOG_INFO(engine::log_gpu, "  shaders granted : %s", granted);

    ENGINE_LOG_INFO(engine::log_gpu, "  swapchain format: %s", name_of(r.swapchain_format));
    ENGINE_LOG_INFO(engine::log_gpu, "  composition     : %s  (%s encodes sRGB)",
            name_of(r.composition),
            r.output_encodes_in_hardware ? "the hardware" : "the SHADER");
    ENGINE_LOG_INFO(engine::log_gpu, "  present modes   : VSYNC%s%s",
            r.supports_immediate ? " IMMEDIATE" : "",
            r.supports_mailbox ? " MAILBOX" : "");
    ENGINE_LOG_INFO(engine::log_gpu, "  frames in flight: %u", r.frames_in_flight);
}

} // namespace engine
