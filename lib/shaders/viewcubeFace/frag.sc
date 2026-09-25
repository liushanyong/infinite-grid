$input v_normal v_color v_region

#include <bgfx_shader.sh>

uniform vec4 uHover;

void main()
{
    if (v_region < -0.5)
    {
        // Analytic compass ring: no triangulated hard edges.
        float radius = length(v_normal.xy);
        float alpha = smoothstep(1.40, 1.49, radius)
                    * (1.0 - smoothstep(1.65, 1.74, radius));
        if (alpha <= 0.0)
        {
            discard;
        }
        gl_FragColor = vec4(v_color.rgb * alpha, alpha);
        return;
    }

    // OpenCADStudio draws the ViewCube as a flat, evenly lit navigation aid.
    vec4 color = v_color;
    if (uHover.x >= 0.0 && abs(v_region - uHover.x) < 0.001)
    {
        color = vec4(0.20, 0.72, 0.66, 1.0);
    }
    gl_FragColor = color;
}
