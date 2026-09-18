$input v_texcoord

#include "bgfx_shader.sh"

SAMPLER2D(uSceneColor, 0);

void main()
{
    gl_FragColor = texture2D(uSceneColor, v_texcoord);
}
