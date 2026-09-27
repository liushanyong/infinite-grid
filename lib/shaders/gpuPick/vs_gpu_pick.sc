$input a_position, a_normal, i_data0, i_data1, i_data2, i_data3, i_data4
$output v_pickDepth
#include <bgfx_shader.sh>

uniform mat4 uView;
uniform mat4 projection;
uniform vec4 uEyeHigh;
uniform vec4 uEyeLow;

void main()
{
    vec3 translation =
        (i_data3.xyz - uEyeHigh.xyz) + (i_data4.xyz - uEyeLow.xyz);
    vec3 relative =
        a_position.x * i_data0.xyz +
        a_position.y * i_data1.xyz +
        a_position.z * i_data2.xyz +
        translation;
    vec4 viewPosition = mul(uView, vec4(relative, 1.0));
    v_pickDepth = -viewPosition.z;
    gl_Position = mul(projection, viewPosition);
}
