$input v_uv, v_color

#include <bgfx_shader.sh>

SAMPLER2D(s_atlas, 0);
uniform vec4 uInvAtlasSize; // 1/w, 1/h, width in world units, pixel range

void main()
{
    // Glyph atlas: BGRA8 with the signed distance stored in alpha.
    vec2 uv = v_uv * uInvAtlasSize.xy;
    float distance = texture2D(s_atlas, uv).a;

    // Map the per-glyph pixel range (provided by the renderer as
    // uInvAtlasSize.w * uInvAtlasSize.z) into a screen-space derivative,
    // giving sub-pixel sharpness at any view scale.
    float worldRange = uInvAtlasSize.w;
    float threshold = 0.5;
    float aa = 0.5 / worldRange;
    float coverage = smoothstep(threshold - aa, threshold + aa, distance);

    if (coverage <= 0.0)
        discard;
    gl_FragColor = vec4(v_color.rgb * coverage, v_color.a * coverage);
}
