$input v_normal, v_viewDepth

#include "bgfx_shader.sh"

uniform vec4 uCubeOpacity;
uniform vec4 uObjectColor;
uniform vec4 uLogDepth;
uniform vec4 uPrimParams;
uniform vec4 uDepthDisplay;

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
    if (uPrimParams.x > 6.5 && uPrimParams.x < 7.5)
    {
        float depthT = uDepthDisplay.x > 0.5
            ? gl_FragCoord.z
            : outputDepth(gl_FragCoord.z, v_viewDepth);
        gl_FragColor = vec4(vec3_splat(1.0 - depthT), 1.0);
        return;
    }
    vec3 normal = normalize(v_normal);
    vec3 lightDirection = normalize(vec3(0.4, 0.8, 0.55));
    float diffuse = max(dot(normal, lightDirection), 0.0);
    vec3 color = uObjectColor.xyz * (0.45 + 0.75 * diffuse);
    gl_FragColor = vec4(color, uCubeOpacity.x);
}
