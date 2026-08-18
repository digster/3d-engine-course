// shaders/mesh.vert.hlsl — a real mesh, placed by per-instance data.
//
// Lesson 4.5. Read the input struct below and notice what it does NOT say: which
// of these six attributes arrive once per vertex and which arrive once per
// instance. That distinction is real — `position` is fetched 1,225 times per
// instance and `placement` once — and it is invisible here, because it lives
// entirely in `SDL_GPUVertexBufferDescription::input_rate` on the C++ side.
// Instancing costs one enum value, not a new shader stage.
//
// WHY THE CAMERA IS A PILE OF CONSTANTS. Every matrix in Lessons 2.9 and 2.10 has
// to reach the GPU somehow, and "somehow" is a uniform buffer, which is Lesson 4.6.
// Rather than pretend, this shader writes out the projection as ARITHMETIC — which
// is all a projection matrix ever was. Compare it with the matrix on the
// Conventions page and the correspondence is line for line:
//
//     P = | t/a   0    0    0  |      clip.x = (t/a) * view.x
//         |  0    t    0    0  |      clip.y =   t   * view.y
//         |  0    0    R   R*n |      clip.z =   R   * (view.z + n)
//         |  0    0   -1    0  |      clip.w =  -view.z
//
// with t = 1/tan(fovY/2) and R = f/(n-f). Nothing is simplified away and nothing
// is different; Lesson 4.6 changes only where the numbers come from.

// ---- The camera, frozen -----------------------------------------------------
// Eye (0, 2.3, 5.0) looking at (0, -0.8, 0): a pitch of 31.8 degrees down, which
// is `cos` and `sin` below rather than an angle because a shader has no business
// evaluating a trig function that is the same for every vertex of every frame.
static const float3 k_eye        = float3(0.0f, 2.3f, 5.0f);
static const float  k_pitch_cos  = 0.8499027f;
static const float  k_pitch_sin  = 0.5269397f;

// fovY 55 degrees at 16:9, near 0.3, far 100 — the same numbers Module 3's scene
// camera uses, so the two pictures are comparable.
static const float  k_focal_x    = 1.0805524f;   // t / aspect
static const float  k_focal_y    = 1.9209821f;   // t = 1 / tan(fovY/2)
static const float  k_depth_a    = -1.0030090f;  // R = f / (n - f)
static const float  k_near       = 0.3f;

struct Input
{
    // ---- Per vertex, from buffer slot 0 (gpu_vertex_pnu, 32 bytes) ----------
    float3 position  : TEXCOORD0;
    float3 normal    : TEXCOORD1;
    float2 uv        : TEXCOORD2;

    // ---- Per instance, from buffer slot 1 (gpu_instance, 28 bytes) ----------
    float4 placement : TEXCOORD3;   // xyz = world offset, w = uniform scale
    float2 spin      : TEXCOORD4;   // (cos, sin) of a rotation about the x axis
    float4 tint      : TEXCOORD5;   // stored as four BYTES; arrives as four floats
};

struct Output
{
    float3 normal   : TEXCOORD0;
    float2 uv       : TEXCOORD1;
    float4 tint     : TEXCOORD2;
    float4 position : SV_Position;
};

Output main(Input input)
{
    Output output;

    // ---- Model -> world -----------------------------------------------------
    //
    // Scale, then rotate, then translate — Lesson 2.8's order, for Lesson 2.8's
    // reasons. The rotation is about x and matches `engine::rotation_x` exactly:
    // that matrix sends y to (0, c, s) and z to (0, -s, c), so
    //     y' = c*y - s*z      z' = s*y + c*z
    // and writing it out here rather than building a float3x3 keeps this file
    // free of HLSL's row/column-order question, which is Lesson 4.6's to answer.
    const float c = input.spin.x;
    const float s = input.spin.y;

    const float3 scaled = input.position * input.placement.w;
    const float3 spun = float3(scaled.x,
                               c * scaled.y - s * scaled.z,
                               s * scaled.y + c * scaled.z);
    const float3 world = spun + input.placement.xyz;

    // The normal takes the ROTATION and nothing else. It is a direction, so the
    // translation must not touch it (Lesson 2.7's w = 0), and the scale is
    // UNIFORM, which is the only reason we may reuse the rotation as-is — Lesson
    // 3.6 showed that a non-uniform scale needs the inverse transpose instead.
    output.normal = float3(input.normal.x,
                           c * input.normal.y - s * input.normal.z,
                           s * input.normal.y + c * input.normal.z);

    // ---- World -> view ------------------------------------------------------
    // Translate by -eye, then pitch. No yaw and no roll: the camera in this lesson
    // does not move, because a camera that moves needs a matrix per frame and a
    // matrix per frame needs Lesson 4.6.
    const float3 rel = world - k_eye;
    const float3 view = float3(rel.x,
                               k_pitch_cos * rel.y - k_pitch_sin * rel.z,
                               k_pitch_sin * rel.y + k_pitch_cos * rel.z);

    // ---- View -> clip -------------------------------------------------------
    // The four lines from the header comment. `w = -view.z` is the perspective
    // divide waiting to happen, and it is the reason Lesson 2.7 spent a whole
    // lesson on a fourth coordinate.
    output.position = float4(k_focal_x * view.x,
                             k_focal_y * view.y,
                             k_depth_a * (view.z + k_near),
                             -view.z);

    output.uv = input.uv;
    output.tint = input.tint;
    return output;
}
