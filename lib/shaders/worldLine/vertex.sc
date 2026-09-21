$input a_position
$output v_endpoint, v_viewDepth

#include "bgfx_shader.sh"

uniform mat4 projection;
uniform vec4 uViewStart;
uniform vec4 uViewEnd;
uniform vec4 uLineWidth;
uniform vec4 uDepthBias;
uniform vec4 uScreenWidth;
uniform vec4 uScreenHeight;

void main()
{
    float t = a_position.x;
    float side = a_position.y;

    vec4 clip0 = mul(projection, vec4(uViewStart.xyz, 1.0));
    vec4 clip1 = mul(projection, vec4(uViewEnd.xyz, 1.0));
    vec2 ndc0 = clip0.xy / clip0.w;
    vec2 ndc1 = clip1.xy / clip1.w;

    // Screen-space perpendicular offset; CPU clipping guarantees w > 0.
    vec2 deltaPx = (ndc1 - ndc0) * (0.5 * vec2(uScreenWidth.x, uScreenHeight.x));
    vec2 normalPx = normalize(vec2(-deltaPx.y, deltaPx.x) + vec2(1e-9, 0.0));
    vec2 offsetNdc = normalPx * (uLineWidth.x * 0.5) *
                     (2.0 / vec2(uScreenWidth.x, uScreenHeight.x));

    vec2 ndc = mix(ndc0, ndc1, t) + side * offsetNdc;
    float z = mix(clip0.z / clip0.w, clip1.z / clip1.w, t);
    gl_Position = vec4(ndc, z, 1.0);
    v_endpoint = t;
    v_viewDepth = mix(-uViewStart.z, -uViewEnd.z, t);
}
