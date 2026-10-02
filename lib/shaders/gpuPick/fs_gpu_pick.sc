$input v_pickDepth, v_objectId

#include <bgfx_shader.sh>
#include <cadCore/depth.sh>

void main()
{
    gl_FragDepth = cad_outputDepth(gl_FragCoord.z, v_pickDepth);
    gl_FragColor = v_objectId;
}
