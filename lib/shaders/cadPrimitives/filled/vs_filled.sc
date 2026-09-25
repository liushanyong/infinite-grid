$input a_position, a_color0
$output v_color, v_depth

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
    v_depth = -viewPosition.z;
    gl_Position = mul(projection, viewPosition);
}
