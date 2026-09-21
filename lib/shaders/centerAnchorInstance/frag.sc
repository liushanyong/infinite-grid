$input v_normal, v_color, v_viewDepth

#include "bgfx_shader.sh"

uniform vec4 uLogDepth;

float outputDepth(float rawDepth, float viewDepth)
{
    if (uLogDepth.x < 0.5)
        return rawDepth;

    if (viewDepth < uLogDepth.y || viewDepth > uLogDepth.z)
        discard;

    float numerator = log2(max(viewDepth / uLogDepth.y, 1.0));
    float denominator = log2(max(uLogDepth.z / uLogDepth.y, 1.000001));
    return clamp(numerator / denominator, 0.0, 1.0);
}

void main()
{
    gl_FragDepth = outputDepth(gl_FragCoord.z, v_viewDepth);
    vec3 normal = normalize(v_normal);
    vec3 lightDirection = normalize(vec3(0.4, 0.8, 0.55));
    float diffuse = max(dot(normal, lightDirection), 0.0);
    vec3 color = v_color.xyz * (0.45 + 0.75 * diffuse);
    gl_FragColor = vec4(color, v_color.w);
}
