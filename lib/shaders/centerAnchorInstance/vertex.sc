$input a_normal, a_position, i_data0, i_data1, i_data2, i_data3, i_data4
$output v_normal, v_color, v_viewDepth, v_uv, v_worldPos
#include <bgfx_shader.sh>

uniform mat4 uView;
uniform mat4 projection;
uniform vec4 uEyeHigh;
uniform vec4 uEyeLow;
uniform vec4 uLayerOffset;

void main()
{
    // UVs ride in position.w/normal.w so the vertex signature leaves five
    // vec4 instance slots for the double-single transform.
    vec4 color = vec4(i_data0.w, i_data1.w, i_data2.w, i_data3.w);
    vec3 objectTranslation =
        (i_data3.xyz - uEyeHigh.xyz) + (i_data4.xyz - uEyeLow.xyz);
    vec3 eyeRelativePosition =
        a_position.x * i_data0.xyz +
        a_position.y * i_data1.xyz +
        a_position.z * i_data2.xyz +
        objectTranslation;
    vec4 viewPosition = mul(uView, vec4(eyeRelativePosition, 1.0));
    viewPosition.z += uLayerOffset.x;
    vec3 transformedNormal =
        a_normal.x * i_data0.xyz +
        a_normal.y * i_data1.xyz +
        a_normal.z * i_data2.xyz;
    v_normal = normalize(transformedNormal);
    v_color = color;
    v_viewDepth = -viewPosition.z;
    v_uv = vec2(a_position.w, a_normal.w);
    v_worldPos = eyeRelativePosition;
    gl_Position = mul(projection, viewPosition);
}
