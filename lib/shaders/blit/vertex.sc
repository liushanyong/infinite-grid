$input a_position
$output v_texcoord

#include "bgfx_shader.sh"

void main()
{
    vec2 ndc = a_position.xy;
    gl_Position = vec4(ndc, 0.0, 1.0);
    // D3D11 NDC y=+1 is the top of the screen, and texture V=0 is the
    // first (top) row.  Sampling with v = ndc.y * 0.5 + 0.5 therefore
    // shows the scene upside down; flip V to compensate.  X is already
    // consistent between the RT and the window.
    v_texcoord = vec2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
}
