$input a_position, a_texcoord0, a_color0
$output v_uv, v_color

#include <bgfx_shader.sh>

uniform mat4 uView;
uniform mat4 uProjection;

void main()
{
    // CPU-expanded glyph quads: position is already in world space, uv
    // addresses the SDF atlas cell, color carries the text tint.
    vec4 viewPosition = mul(uView, vec4(a_position, 1.0));
    gl_Position = mul(uProjection, viewPosition);
    v_uv = a_texcoord0;
    v_color = a_color0;
}
