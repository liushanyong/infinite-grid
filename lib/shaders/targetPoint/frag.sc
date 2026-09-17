$input v_pointCoord

#include "bgfx_shader.sh"

uniform vec4 uColor;

void main()
{
    vec2 c = v_pointCoord;
    float r2 = dot(c, c);
    if (r2 > 1.0)
    {
        discard;
    }

    float z = sqrt(1.0 - r2);
    float shade = 0.4 + 0.6 * z;
    gl_FragColor = vec4(uColor.xyz * shade, 1.0);
}
