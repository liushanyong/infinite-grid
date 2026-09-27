$input v_color, v_depth

#include <bgfx_shader.sh>
#include <cadCore/depth.sh>

void main()
{
    gl_FragDepth = cad_outputDepth(gl_FragCoord.z, v_depth);
    gl_FragColor = v_color;
}
