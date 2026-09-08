// shaders/shadow.frag.hlsl — a fragment shader that writes nothing.
//
// Lesson 6.8. A depth-only pipeline has `num_color_targets = 0`, so there is no
// `SV_Target` for this stage to return and nothing for it to compute. What it
// still does — implicitly, in fixed-function hardware after this function
// returns — is the depth test and the depth write, which is the entire purpose
// of the pass.
//
// WHY THE FILE EXISTS AT ALL. `SDL_GPUGraphicsPipelineCreateInfo` has a
// `fragment_shader` field and SDL's header does not document NULL as a legal
// value for it, so we do not pass one. ⚠ VERIFY: several backends do accept a
// null fragment shader for a depth-only pipeline (Vulkan's
// `VK_EXT_extended_dynamic_state3` and D3D12 both permit it), and if SDL ever
// states that it forwards NULL, this file becomes deletable. Until the header
// says so, an empty shader is the portable spelling — five instructions that the
// driver optimises to none, against a crash on the one backend that disagrees.
//
// It is also the honest place to put the *other* thing a shadow pass sometimes
// needs: an alpha-tested caster — a leaf, a chain-link fence — has to sample its
// albedo here and `discard`, which is why "depth-only" and "no fragment shader"
// are not quite the same statement. Nothing in this course's scenes needs it,
// and the file is where it would go.

void main()
{
}
