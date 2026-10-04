$input a_position, a_texcoord0, i_data0, i_data1, i_data2, i_data3
$output v_uv, v_color

#include <bgfx_shader.sh>

uniform mat4 uView;
uniform mat4 uProjection;

void main()
{
    float rightAmount = a_texcoord0.x * i_data2.z;
    float upAmount = a_texcoord0.y * i_data2.w;
    vec3 worldPosition = vec3(
        i_data0.x + i_data1.x * rightAmount + i_data1.z * upAmount,
        i_data0.y + i_data1.y * rightAmount + i_data1.w * upAmount,
        i_data0.z);
    vec4 viewPosition = mul(uView, vec4(worldPosition, 1.0));
    vec4 clipPosition = mul(uProjection, viewPosition);
    gl_Position = clipPosition;
    v_uv = vec2(
        i_data2.x + a_texcoord0.x * i_data2.z,
        i_data2.y + a_texcoord0.y * i_data2.w);
    vec4 colorOut = vec4(i_data3.x, i_data3.y, i_data3.z, 1.0);
    v_color = colorOut;
}
