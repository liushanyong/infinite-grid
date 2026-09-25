$input v_pointCoord, v_color, v_depth

#include "bgfx_shader.sh"

uniform vec4 uPrimParams;

void main()
{
    vec2 c = v_pointCoord;
    float r2 = dot(c, c);
    if (r2 > 1.0)
    {
        discard;
    }

    float z = sqrt(1.0 - r2);
    if (uPrimParams.x > 0.5)
    {
        gl_FragColor = vec4(vec3_splat(1.0 - v_depth), 1.0);
        return;
    }
    float shade = 0.4 + 0.6 * z;
    gl_FragColor = vec4(v_color.xyz * shade, v_color.w);
}
