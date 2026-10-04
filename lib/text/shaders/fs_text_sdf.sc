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
    if (opacity <= 0.0)
        discard;
    gl_FragColor = vec4(v_color.rgb, v_color.a * opacity);
}
