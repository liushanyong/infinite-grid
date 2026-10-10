$input v_uv, v_color

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);
uniform vec4 u_params0; // .x = texture LOD (0 = base)

void main()
{
    // Per-glyph R8 SDF texture: 0.5 = glyph boundary, 1.0 = far inside,
    // 0.0 = far outside.  fwidth gives resolution-independent AA.
    float sigDist = texture2DLod(s_texColor, v_uv, u_params0.x).x;
    float width = fwidth(sigDist);
    float edge = 0.5;
    float opacity = clamp(smoothstep(edge - width, edge + width, sigDist),
                          0.0, 1.0);
    if (u_params0.y > 0.5)
    {
        float outerOpacity = smoothstep(edge - 3.0 * width,
                                        edge - width, sigDist);
        float outlineOpacity = max(outerOpacity - opacity, 0.0);
        float highlightOpacity =
            (0.20 * opacity + 0.78 * outlineOpacity) * v_color.a;
        if (highlightOpacity <= 0.0)
            discard;
        gl_FragColor = vec4(v_color.rgb, highlightOpacity);
        return;
    }
    if (opacity <= 0.0)
        discard;
    gl_FragColor = vec4(v_color.rgb, v_color.a * opacity);
}
