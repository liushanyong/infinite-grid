$input v_normal v_color v_region

#include <bgfx_shader.sh>

uniform vec4 uHover;

void main()
{
    // OpenCADStudio draws the ViewCube as a flat, evenly lit navigation aid.
    vec4 color = v_color;
    if (uHover.x > -0.5 && abs(v_region * 25.0 - uHover.x) < 0.5)
    {
        color = vec4(0.20, 0.72, 0.66, 1.0);
    }
    gl_FragColor = color;
}
