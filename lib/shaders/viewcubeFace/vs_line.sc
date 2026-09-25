$input a_position
$output v_color

#include <bgfx_shader.sh>

uniform mat4 uProj;

void main()
{
    gl_Position = mul(uProj, vec4(a_position, 1.0));
    v_color = vec4(0.05, 0.08, 0.13, 1.0);
}
