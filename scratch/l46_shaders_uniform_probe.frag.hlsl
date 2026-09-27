// shaders/uniform_probe.frag.hlsl — one pixel, one question about a uniform.
//
// Lesson 4.6. Reading a uniform back is not a thing an API offers: the data goes
// one way. So we ask the shader, and the shader answers in the only currency it
// has, which is the colour of a pixel.
//
// THE SCHEME. The target is 4x4 or larger; pixel (x, y) reports one element of the
// matrix — row y, column x, in HLSL's own indexing — scaled so that a value of n
// arrives back as the byte n. The caller pushes a matrix whose element (r, c) is
// 16*r + c + 1, reads the 4x4 patch, and now knows exactly which of the sixteen
// floats it wrote landed at which of the sixteen places the shader can name.
//
// That is the whole experiment, and it settles a claim this course has had on
// record since Lesson 2.6 without ever checking it.

cbuffer Probe : register(b0, space3)
{
    // The matrix under test. Written by the caller as sixteen floats in our
    // mat4's storage order: c0.x, c0.y, c0.z, c0.w, c1.x, ...
    float4x4 m;

    // A second question, asked at the same time: where does the compiler put a
    // field? These five are arranged so that a C++ struct with the same fields in
    // the same order does NOT agree with what the shader reads — which is the bug
    // this lesson exists to make visible, in bytes rather than in adjectives.
    float  scalar_a;
    float3 vector_b;
    float  scalar_c;
    float2 pair_d;
    float3 vector_e;
};

float4 main(float4 position : SV_Position) : SV_Target0
{
    // SV_Position in a fragment shader is the PIXEL CENTRE, so the first pixel is
    // (0.5, 0.5) and truncating gives 0. Rounding would give 1 and the whole probe
    // would be off by one — a mistake worth naming, because it produces a table
    // that looks plausible and is shifted.
    const int x = int(position.x);
    const int y = int(position.y);

    float value = 0.0f;

    if (y < 4)
    {
        // Rows 0-3: the matrix, in HLSL's indexing. m[row][col].
        value = m[y][x % 4];
    }
    else if (y == 4)
    {
        // Row 4: a scalar, then a float3 that follows it in the same register.
        if (x == 0) { value = scalar_a; }
        else if (x == 1) { value = vector_b.x; }
        else if (x == 2) { value = vector_b.y; }
        else if (x == 3) { value = vector_b.z; }
    }
    else if (y == 5)
    {
        // Row 5: a scalar, a float2, and a float3 that CANNOT follow them without
        // straddling a 16-byte boundary — so the compiler moves it, and a C++
        // struct that did not moves nothing.
        if (x == 0) { value = scalar_c; }
        else if (x == 1) { value = pair_d.x; }
        else if (x == 2) { value = pair_d.y; }
        else if (x == 3) { value = vector_e.x; }
        else if (x == 4) { value = vector_e.y; }
        else if (x == 5) { value = vector_e.z; }
    }

    // Encoded so that an integer value of n comes back as the byte n: divide by
    // 255 here, and the _UNORM target multiplies by 255 on the way in. Values
    // outside 0..255 clamp, which reads as 0 or 255 and is itself informative.
    return float4(value / 255.0f, value / 255.0f, value / 255.0f, 1.0f);
}
