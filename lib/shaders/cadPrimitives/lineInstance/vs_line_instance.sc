$input a_position, i_data0, i_data1, i_data2, i_data3
$output v_color, v_edge, v_depth, v_along

#include <bgfx_shader.sh>

uniform mat4 uView;
uniform mat4 projection;
uniform vec4 uLayerOffset;

void main()
{
    vec3 start = i_data0.xyz;
    vec3 end = i_data1.xyz;
    vec3 viewStart = mul(uView, vec4(start, 1.0)).xyz;
    vec3 viewEnd = mul(uView, vec4(end, 1.0)).xyz;
    vec2 delta = viewEnd.xy - viewStart.xy;
    float len = max(length(delta), 1.0e-6);
    vec2 dir = delta / len;
    vec2 side = vec2(-dir.y, dir.x);
    float edge = a_position.y * 2.0 - 1.0;
    vec3 viewPosition = mix(viewStart, viewEnd, a_position.x);
    viewPosition.xy += side * i_data2.w * edge;
    viewPosition.z += uLayerOffset.x;

    v_color = vec4(i_data2.rgb, i_data3.x);
    v_edge = edge;
    v_depth = -viewPosition.z;
    v_along = mix(i_data0.w, i_data1.w, a_position.x);
    gl_Position = mul(projection, vec4(viewPosition, 1.0));
}
