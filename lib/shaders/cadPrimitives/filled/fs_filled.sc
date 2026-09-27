$input v_color, v_depth

#include <bgfx_shader.sh>
#include <cadCore/depth.sh>

uniform vec4 uPrimParams;
uniform vec4 u_material;

void main()
{
    gl_FragDepth = cad_outputDepth(gl_FragCoord.z, v_depth);
    if (uPrimParams.x > 6.5 && uPrimParams.x < 7.5)
    {
        float depthT = cad_outputDepth(gl_FragCoord.z, v_depth);
        gl_FragColor = vec4(vec3_splat(1.0 - depthT), 1.0);
        return;
    }
    gl_FragColor = vec4(v_color.rgb * u_material.rgb, v_color.a);
}
