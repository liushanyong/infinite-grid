$input a_position, a_normal, i_data0, i_data1, i_data2, i_data3, i_data4
$output v_color, v_edge, v_depth, v_along

#include <bgfx_shader.sh>

uniform mat4 uView;
uniform mat4 projection;
uniform vec4 uEyeHigh;
uniform vec4 uEyeLow;
uniform vec4 uLayerOffset;
uniform vec4 uEdgeRibbon;

void main()
{
    // The static edge buffer stores both segment endpoints per ribbon vertex:
    // position.xyz = start, normal.xyz = end, position.w = along,
    // normal.w = ribbon side.  One instance is one mesh instance.
    vec3 objectTranslation =
        (i_data3.xyz - uEyeHigh.xyz) + (i_data4.xyz - uEyeLow.xyz);
    vec3 start =
        a_position.x * i_data0.xyz +
        a_position.y * i_data1.xyz +
        a_position.z * i_data2.xyz +
        objectTranslation;
    vec3 end =
        a_normal.x * i_data0.xyz +
        a_normal.y * i_data1.xyz +
        a_normal.z * i_data2.xyz +
        objectTranslation;

    vec3 viewStart = mul(uView, vec4(start, 1.0)).xyz;
    vec3 viewEnd = mul(uView, vec4(end, 1.0)).xyz;
    vec2 delta = viewEnd.xy - viewStart.xy;
    float segmentLength = max(length(delta), 1.0e-6);
    vec2 direction = delta / segmentLength;
    vec2 side = vec2(-direction.y, direction.x);
    float along = a_position.w;
    float edge = a_normal.w;

    vec3 viewPosition = mix(viewStart, viewEnd, along);
    viewPosition.xy += side * uEdgeRibbon.x * edge;
    viewPosition.z += uLayerOffset.x;

    // Preserve the per-instance edge color and opacity from the fill payload.
    v_color = vec4(i_data0.w, i_data1.w, i_data2.w,
                   clamp(i_data3.w, 0.0, 1.0));
    v_edge = edge;
    v_depth = -viewPosition.z;
    v_along = along;
    gl_Position = mul(projection, vec4(viewPosition, 1.0));
}
