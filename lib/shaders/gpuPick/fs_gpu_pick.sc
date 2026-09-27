$input v_pickDepth

#include <bgfx_shader.sh>
#include <cadCore/depth.sh>

uniform vec4 u_object_index;

void main()
{
    gl_FragDepth = cad_outputDepth(gl_FragCoord.z, v_pickDepth);
    gl_FragColor = u_object_index;
}
