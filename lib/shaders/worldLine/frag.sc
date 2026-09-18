$input v_endpoint

#include "bgfx_shader.sh"

uniform vec4 uColor;

void main()
{
    gl_FragColor = uColor;
}
