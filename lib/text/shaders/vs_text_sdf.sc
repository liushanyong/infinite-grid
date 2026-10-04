$input a_position, a_texcoord0, a_color0
$output v_uv, v_color

#include <bgfx_shader.sh>

// Names must match the renderer's uniform handles: "uView" and
// "projection" (see createRenderResources).
uniform mat4 uView;
uniform mat4 projection;

void main()
{
    vec4 viewPosition = mul(uView, vec4(a_position, 1.0));
    gl_Position = mul(projection, viewPosition);
    v_uv = a_texcoord0;
    v_color = a_color0;
}
