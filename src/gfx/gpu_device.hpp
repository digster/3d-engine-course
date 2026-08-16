// src/gfx/gpu_device.hpp — the GPU device, and the window it draws into.
//
// Lesson 4.2. This is the first file in the engine that talks to hardware we do
// not control. Everything in `gfx/` before it — the framebuffer, the rasterizer,
// the depth buffer, the sampler — runs on the same processor as the code that
// calls it, so a call and its effect are the same event. From here they are not,
// and that single change is what this file exists to make manageable.
//
// Two objects, and their lifetimes are the whole design:
//
//   THE DEVICE is the connection to the hardware. Creating it picks a backend
//   (Metal, Vulkan, D3D12), loads a driver, and allocates memory we can never see
//   directly. It is expensive, it is created once, and every GPU object in the
//   engine is created *from* it and must be released *before* it.
//
//   THE CLAIM is the arrangement between the device and a window. A claimed
//   window gains a SWAPCHAIN — the images the display actually scans out — and
//   the device gains the right to hand us one per frame. Claiming is not
//   creating: the window existed before, and it survives the release.
//
// Both are owned by one object here, because their orders are opposite (claim
// after create, release before destroy) and an owner whose destructor knows the
// order is strictly better than a comment asking you to remember it.

#pragma once

#include <SDL3/SDL.h>

#include <cstddef>

namespace engine {

/// Why device creation failed, or that it did not.
///
/// A small enum rather than a bool, because the three failures want three
/// different messages to the user: "your drivers are too old" is a different
/// conversation from "this window is already spoken for".
enum class gpu_status
{
    ok,

    /// `SDL_CreateGPUDevice` returned null. No backend on this machine could
    /// satisfy the request — most often a Vulkan installation too old for
    /// SDL_GPU's feature floor, or a headless session with no GPU at all.
    no_device,

    /// The device exists, but `SDL_ClaimWindowForGPUDevice` refused. The usual
    /// cause is a window that already belongs to something else — an
    /// `SDL_Renderer` created on it earlier, for instance, which is exactly why
    /// this engine's GPU path takes the window *before* any renderer is made.
    window_not_claimed
};

/// A human-readable name for a status, for logs and for the HUD.
[[nodiscard]] const char* name_of(gpu_status s);

/// What the device turned out to be — every field *asked for*, never assumed.
///
/// The habit this struct enforces is the point of it. A GPU program that assumes
/// its swapchain format, or assumes a present mode is available, works perfectly
/// on the machine it was written on and fails somewhere else, silently, months
/// later. Every field below has a query behind it, and Lesson 4.2 §2 prints the
/// lot at startup so a bug report can begin with facts.
struct gpu_report
{
    gpu_status status = gpu_status::no_device;

    /// The backend SDL chose: "metal", "vulkan", "direct3d12". Owned by SDL —
    /// a string literal inside the library, valid for the device's lifetime.
    const char* driver = "none";

    /// The shader formats we *asked* for, and the ones the device *has*.
    ///
    /// These differ more often than not, and the difference is not a failure.
    /// The mask we pass to `SDL_CreateGPUDevice` says "here is what I can supply";
    /// `SDL_GetGPUShaderFormats` answers "here is what I accept". On this machine
    /// we ask for SPIRV|DXIL|MSL and are granted MSL|METALLIB — one format we
    /// named, one we did not. Lesson 4.3 needs the *granted* set to decide which
    /// file to load.
    SDL_GPUShaderFormat asked = SDL_GPU_SHADERFORMAT_INVALID;
    SDL_GPUShaderFormat granted = SDL_GPU_SHADERFORMAT_INVALID;

    /// The pixel format of the swapchain textures this window will hand us.
    ///
    /// Not a constant, and not ours to pick: it is negotiated between the display,
    /// the compositor and the driver. A pipeline whose colour target format does
    /// not match this exactly will fail to create in Lesson 4.4, so this value is
    /// load-bearing rather than informational.
    SDL_GPUTextureFormat swapchain_format = SDL_GPU_TEXTUREFORMAT_INVALID;

    /// Which present modes this window supports. VSYNC is guaranteed everywhere;
    /// the other two are not, which is why they are asked about rather than used.
    bool supports_immediate = false;
    bool supports_mailbox = false;

    /// How many frames may be pending on the GPU at once. SDL's default is 2, and
    /// this field records what we actually set, because it is the single knob
    /// that trades latency against throughput (§6).
    Uint32 frames_in_flight = 2;

    [[nodiscard]] bool ok() const { return status == gpu_status::ok; }
};

/// Owns an `SDL_GPUDevice`, and the claim on one window.
///
/// **Movable, not copyable** — the house pattern for a resource type (C++ Style
/// §4). Two objects owning one device would release it twice, and there is no
/// meaningful way to copy a connection to hardware.
///
/// Construction is two-phase: the default constructor cannot fail, and `create`
/// reports what happened. That is forced by the engine core having no exceptions
/// (C++ Style §5) — a constructor has no way to say "no". The cost is that
/// `valid()` exists at all; the benefit is that every failure path is visible in
/// a signature.
class gpu_device
{
public:
    gpu_device() = default;
    ~gpu_device();

    gpu_device(const gpu_device&) = delete;
    gpu_device& operator=(const gpu_device&) = delete;

    gpu_device(gpu_device&& other) noexcept;
    gpu_device& operator=(gpu_device&& other) noexcept;

    /// Create the device and claim `window`.
    ///
    /// Both halves, deliberately, because a half-built device is a state nobody
    /// wants to handle: if the claim fails the device is destroyed before this
    /// returns, and the object is left exactly as invalid as it started.
    ///
    /// @param window The window to draw into. Must not already belong to an
    ///               `SDL_Renderer` — SDL will refuse the claim if it does.
    /// @param debug  Turn on the backend's validation layer. It catches API
    ///               misuse that would otherwise be a black screen, and it costs
    ///               real time, so it is a parameter rather than a constant.
    /// @return       The report; check `ok()` before using anything else.
    [[nodiscard]] gpu_report create(SDL_Window* window, bool debug);

    /// Release the claim and destroy the device. Safe to call twice; called by
    /// the destructor. **Every GPU object created from this device must already
    /// be released** — SDL owns the diagnostic when they are not, and it is not
    /// a gentle one.
    void destroy();

    [[nodiscard]] bool valid() const { return device_ != nullptr; }

    /// The raw handle, for the SDL calls this class does not wrap.
    ///
    /// Deliberately exposed rather than hidden behind a hundred forwarding
    /// methods. This is a teaching codebase and SDL_GPU is the subject; wrapping
    /// its whole surface would hide exactly what the student came to learn. The
    /// wrapper exists for *lifetime*, not for concealment.
    [[nodiscard]] SDL_GPUDevice* handle() const { return device_; }

    /// The window whose swapchain we present to, or `nullptr`.
    [[nodiscard]] SDL_Window* window() const { return window_; }

    [[nodiscard]] const gpu_report& report() const { return report_; }

    /// Ask for a different present mode. Returns false and changes nothing if the
    /// window does not support it — which is why `gpu_report` records support
    /// separately from choice.
    [[nodiscard]] bool set_present_mode(SDL_GPUPresentMode mode);

    /// Set how many frames may be pending at once, 1 to 3.
    ///
    /// Note SDL's own warning, quoted because it is the sort of thing that turns
    /// a benchmark into fiction: this call "will stall and flush the command
    /// queue". Never per frame; once, at a settings change.
    [[nodiscard]] bool set_frames_in_flight(Uint32 frames);

    /// Write the whole report to the log, one line per fact.
    void log_report() const;

private:
    SDL_GPUDevice* device_ = nullptr;   ///< owning by contract; released in destroy()
    SDL_Window* window_ = nullptr;      ///< NOT owned — claimed, and released, only
    gpu_report report_{};
};

/// The name SDL uses for a texture format, for logs. Covers the formats this
/// engine can meet as a swapchain; anything else reports its numeric value.
[[nodiscard]] const char* name_of(SDL_GPUTextureFormat f);

/// The names of the bits set in a shader-format mask, e.g. "MSL | METALLIB".
///
/// @param out  Buffer to write into; always null-terminated.
/// @param cap  Its capacity in bytes.
void format_shader_formats(SDL_GPUShaderFormat mask, char* out, std::size_t cap);

} // namespace engine
