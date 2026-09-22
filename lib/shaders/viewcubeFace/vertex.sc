$input a_position a_normal a_color0 a_region
$output v_normal v_color v_region

#include <bgfx_shader.sh>

uniform mat4 uView;
uniform mat4 uProj;

void main()
{
    vec4 viewPosition = mul(uView, vec4(a_position, 1.0));
    vec4 viewNormal = mul(uView, vec4(a_normal, 0.0));
    v_normal = viewNormal.xyz;
    v_color = a_color0;
    v_region = a_region;
    gl_Position = mul(uProj, viewPosition);
}
