$input v_color, v_edge, v_depth, v_along

#include <bgfx_shader.sh>
#include <cadCore/depth.sh>
#include <cadCore/line.sh>

uniform vec4 uPrimParams;
uniform vec4 uDepthDisplay;

void main()
{
    gl_FragDepth = cad_outputDepth(gl_FragCoord.z, v_depth);
    float alpha = cad_lineAlpha(v_edge, uPrimParams.y);
    if (uPrimParams.w > 0.5)
        alpha *= cad_dashAlpha(v_along, 0.36, 0.24);
    if (alpha <= 0.0)
        discard;
    if (uPrimParams.z > 6.5 && uPrimParams.z < 7.5)
    {
        float depthT = uDepthDisplay.x > 0.5
            ? gl_FragCoord.z
            : cad_outputDepth(gl_FragCoord.z, v_depth);
        gl_FragColor = vec4(vec3_splat(1.0 - depthT), alpha);
        return;
    }
    gl_FragColor = vec4(v_color.rgb, v_color.a * alpha);
}
