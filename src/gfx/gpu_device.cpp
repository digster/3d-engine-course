// src/gfx/gpu_device.cpp — creating the device, claiming the window, and asking
// the device what it turned out to be.

#include "gfx/gpu_device.hpp"

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
    // Only the formats a swapchain realistically arrives as. The point of the
    // function is a readable log line, not a complete table — and an unknown
    // format printing its number is more useful than a wrong name.
    switch (f)
    {
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:      return "B8G8R8A8_UNORM";
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:      return "R8G8B8A8_UNORM";
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB: return "B8G8R8A8_UNORM_SRGB";
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB: return "R8G8B8A8_UNORM_SRGB";
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT:  return "R16G16B16A16_FLOAT";
    case SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM:   return "R10G10B10A2_UNORM";
    case SDL_GPU_TEXTUREFORMAT_INVALID:             return "INVALID";
    default:                                        return "(other)";
    }
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
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        report_ = r;
        return r;
    }

    if (!SDL_ClaimWindowForGPUDevice(device_, window))
    {
        // Leave nothing half-built. A device with no window is a perfectly legal
        // object — offscreen rendering uses one — but it is not what the caller
        // asked for, and returning it would make `ok()` a lie.
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        SDL_DestroyGPUDevice(device_);
        device_ = nullptr;
        r.status = gpu_status::window_not_claimed;
        report_ = r;
        return r;
    }

    window_ = window;

    // ---- Now ask the device what it is ------------------------------------
    r.status = gpu_status::ok;
    r.driver = SDL_GetGPUDeviceDriver(device_);
    r.granted = SDL_GetGPUShaderFormats(device_);
    r.swapchain_format = SDL_GetGPUSwapchainTextureFormat(device_, window);
    r.supports_immediate =
        SDL_WindowSupportsGPUPresentMode(device_, window, SDL_GPU_PRESENTMODE_IMMEDIATE);
    r.supports_mailbox =
        SDL_WindowSupportsGPUPresentMode(device_, window, SDL_GPU_PRESENTMODE_MAILBOX);
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

    // The composition stays as it is. SDL_SDR is the plain 8-bit-per-channel
    // pipeline; the HDR compositions belong to Module 6, and changing one thing
    // at a time is why this parameter is not a knob yet.
    if (!SDL_SetGPUSwapchainParameters(device_, window_,
                                       SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode))
    {
        SDL_Log("SDL_SetGPUSwapchainParameters failed: %s", SDL_GetError());
        return false;
    }

    // The format can change with the parameters, so re-ask rather than assume.
    report_.swapchain_format = SDL_GetGPUSwapchainTextureFormat(device_, window_);
    return true;
}

bool gpu_device::set_frames_in_flight(Uint32 frames)
{
    if (device_ == nullptr) { return false; }
    if (frames < 1 || frames > 3) { return false; }   // SDL's documented range

    if (!SDL_SetGPUAllowedFramesInFlight(device_, frames))
    {
        SDL_Log("SDL_SetGPUAllowedFramesInFlight(%u) failed: %s", frames, SDL_GetError());
        return false;
    }
    report_.frames_in_flight = frames;
    return true;
}

void gpu_device::log_report() const
{
    const gpu_report& r = report_;

    SDL_Log("GPU device: %s", name_of(r.status));
    if (!r.ok()) { return; }

    SDL_Log("  driver          : %s", r.driver);

    const int drivers = SDL_GetNumGPUDrivers();
    for (int i = 0; i < drivers; ++i)
    {
        const char* name = SDL_GetGPUDriver(i);
        SDL_Log("  driver [%d]      : %s%s", i, name,
                (name != nullptr && r.driver != nullptr
                 && SDL_strcmp(name, r.driver) == 0) ? "   <- chosen" : "");
    }

    char asked[64];
    char granted[64];
    format_shader_formats(r.asked, asked, sizeof(asked));
    format_shader_formats(r.granted, granted, sizeof(granted));
    SDL_Log("  shaders asked   : %s", asked);
    SDL_Log("  shaders granted : %s", granted);

    SDL_Log("  swapchain format: %s", name_of(r.swapchain_format));
    SDL_Log("  present modes   : VSYNC%s%s",
            r.supports_immediate ? " IMMEDIATE" : "",
            r.supports_mailbox ? " MAILBOX" : "");
    SDL_Log("  frames in flight: %u", r.frames_in_flight);
}

} // namespace engine
