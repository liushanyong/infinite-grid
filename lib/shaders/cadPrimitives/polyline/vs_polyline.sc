$input a_position, a_color0, a_texcoord0
$output v_color, v_edge, v_depth, v_along

#include <bgfx_shader.sh>

uniform mat4 uView;
uniform mat4 projection;
uniform vec4 uLayerOffset;

void main()
{
    vec4 worldPosition = vec4(a_position, 1.0);
    vec4 viewPosition = mul(uView, worldPosition);
    // Layer compositing: positive offset moves geometry one layer closer.
    viewPosition.z += uLayerOffset.x;
    v_color = a_color0;
    v_edge = a_texcoord0.y * 2.0 - 1.0;
    v_depth = -viewPosition.z;
    v_along = a_texcoord0.x;
    gl_Position = mul(projection, viewPosition);
}
