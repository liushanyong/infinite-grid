$input a_position
$output v_texcoord

#include "bgfx_shader.sh"

void main()
{
    vec2 ndc = a_position.xy;
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_texcoord = ndc * 0.5 + 0.5;
}
