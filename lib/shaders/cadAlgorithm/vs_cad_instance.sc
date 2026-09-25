$input a_position, a_normal, a_texcoord0, i_data0, i_data1, i_data2, i_data3, i_data4
$output v_worldPos, v_normal, v_view, v_bary, v_color0, v_depth

#include <bgfx_shader.sh>

uniform mat4 u_cadView;
uniform mat4 u_cadProjection;
uniform vec4 uEyeHigh;
uniform vec4 uEyeLow;
uniform vec4 u_cameraPos;
uniform vec4 uLayerOffset;

void main()
{
    vec4 color = vec4(i_data0.w, i_data1.w, i_data2.w, i_data3.w);
    vec3 objectTranslation =
        (i_data3.xyz - uEyeHigh.xyz) + (i_data4.xyz - uEyeLow.xyz);
    vec3 eyeRelativePosition =
        a_position.x * i_data0.xyz +
        a_position.y * i_data1.xyz +
        a_position.z * i_data2.xyz +
        objectTranslation;
    vec4 viewPosition = mul(u_cadView, vec4(eyeRelativePosition, 1.0));
    viewPosition.z += uLayerOffset.x;
    v_depth = -viewPosition.z;

    v_worldPos = eyeRelativePosition;
    v_normal = normalize(
        a_position.x * i_data0.xyz +
        a_position.y * i_data1.xyz +
        a_position.z * i_data2.xyz);
    v_view = normalize(-eyeRelativePosition);
    v_bary = a_texcoord0;
    v_color0 = color;
    gl_Position = mul(u_cadProjection, viewPosition);
}
