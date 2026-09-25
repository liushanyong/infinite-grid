$input v_color, v_edge, v_depth

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
    // Anti-aliased ribbon edge: v_edge spans [-1, 1]; the outer band is
    // faded over uPrimParams.x (ribbon units, roughly one pixel).
    float aa = max(uPrimParams.x, 0.02);
    float alpha = 1.0 - smoothstep(1.0 - aa, 1.0, abs(v_edge));
    if (alpha <= 0.0)
        discard;
    if (uPrimParams.z > 6.5 && uPrimParams.z < 7.5)
    {
        float depthT = outputDepth(gl_FragCoord.z, v_depth);
        gl_FragColor = vec4(vec3_splat(1.0 - depthT), alpha);
        return;
    }
    gl_FragColor = vec4(v_color.rgb, v_color.a * alpha);
}
