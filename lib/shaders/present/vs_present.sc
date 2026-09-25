$input a_position, a_texcoord0
$output v_uv

#include <bgfx_shader.sh>

void main()
{
    v_uv = a_texcoord0;
    gl_Position = vec4(a_position, 0.0, 1.0);
}
