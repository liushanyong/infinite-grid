$input a_position, a_normal, a_texcoord0, i_data0, i_data1, i_data2, i_data3
$output v_worldPos, v_normal, v_view, v_bary, v_color0, v_depth

#include <bgfx_shader.sh>

uniform mat4 u_cadView;
uniform mat4 u_cadProjection;
uniform vec4 u_cameraPos;

void main()
{
    vec3 worldPosRebased =
        a_position.x * i_data0.xyz +
        a_position.y * i_data1.xyz +
        a_position.z * i_data2.xyz +
        vec3(i_data0.w, i_data1.w, i_data2.w);
    vec4 worldPosition = vec4(worldPosRebased, 1.0);
    vec4 viewPosition = mul(u_cadView, worldPosition);
    // View-space depth for the shared logarithmic depth mapping.
    v_depth = -viewPosition.z;

    v_worldPos = worldPosRebased;
    v_normal = normalize(
        a_position.x * i_data0.xyz +
        a_position.y * i_data1.xyz +
        a_position.z * i_data2.xyz);
    v_view = normalize(u_cameraPos.xyz - worldPosRebased);
    v_bary = a_texcoord0;
    v_color0 = i_data3;
    gl_Position = mul(u_cadProjection, viewPosition);
}
