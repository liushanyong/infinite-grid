$input v_color, v_edge, v_depth

#include <bgfx_shader.sh>
#include <cadCore/depth.sh>

uniform vec4 uPrimParams;

void main()
{
    gl_FragDepth = cad_outputDepth(gl_FragCoord.z, v_depth);
    // Anti-aliased ribbon edge: v_edge spans [-1, 1]; the outer band is
    // faded over uPrimParams.x (ribbon units, roughly one pixel).
    float aa = max(uPrimParams.x, 0.02);
    float alpha = 1.0 - smoothstep(1.0 - aa, 1.0, abs(v_edge));
    if (alpha <= 0.0)
        discard;
    if (uPrimParams.z > 6.5 && uPrimParams.z < 7.5)
    {
        float depthT = cad_outputDepth(gl_FragCoord.z, v_depth);
        gl_FragColor = vec4(vec3_splat(1.0 - depthT), alpha);
        return;
    }
    gl_FragColor = vec4(v_color.rgb, v_color.a * alpha);
}
