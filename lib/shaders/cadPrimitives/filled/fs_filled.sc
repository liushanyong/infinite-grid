$input v_color, v_uv, v_depth

#include <bgfx_shader.sh>
#include <cadCore/depth.sh>

uniform vec4 uPrimParams;
uniform vec4 u_material;
uniform vec4 uDepthDisplay;
uniform sampler2D s_albedo;

void main()
{
    vec4 color = v_color;
    if (uPrimParams.z > 0.5)
        color *= texture2D(s_albedo, v_uv);
    if (color.a <= 0.0)
        discard;
    if (uPrimParams.y > 0.5)
    {
        // Slab-free overlay depth (GPU ID screen overlays such as SDF
        // text): the visible text pass draws without a depth test and
        // without the depth-slab discard, so its ID geometry must ignore
        // the slab too, or text outside [near, far] loses its ID while
        // still visible on screen.
        gl_FragDepth = gl_FragCoord.z;
        gl_FragColor = vec4(color.rgb * u_material.rgb, color.a);
        return;
    }
    gl_FragDepth = cad_outputDepth(gl_FragCoord.z, v_depth);
    if (uPrimParams.x > 6.5 && uPrimParams.x < 7.5)
    {
        float depthT = uDepthDisplay.x > 0.5
            ? gl_FragCoord.z
            : cad_outputDepth(gl_FragCoord.z, v_depth);
        gl_FragColor = vec4(vec3_splat(1.0 - depthT), 1.0);
        return;
    }
    gl_FragColor = vec4(color.rgb * u_material.rgb, color.a);
}
