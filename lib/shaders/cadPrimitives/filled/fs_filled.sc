$input v_color, v_depth

#include <bgfx_shader.sh>

uniform vec4 uLogDepth;
uniform vec4 uPrimParams;

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
    gl_FragDepth = outputDepth(gl_FragCoord.z, v_depth);
    if (uPrimParams.x > 6.5 && uPrimParams.x < 7.5)
    {
        float depthT = outputDepth(gl_FragCoord.z, v_depth);
        gl_FragColor = vec4(vec3_splat(1.0 - depthT), 1.0);
        return;
    }
    gl_FragColor = v_color;
}
