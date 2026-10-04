$input v_uv, v_color

#include <bgfx_shader.sh>

SAMPLER2D(s_atlas, 0);
uniform vec4 uInvAtlasSize; // .w = SDF pixel range in world units

void main()
{
    // v_uv is already the atlas UV (0..1); the atlas stores the signed
    // distance in alpha (0 = far outside, 128 = boundary, 255 = far inside).
    float distance = texture2D(s_atlas, v_uv).a;

    float worldRange = uInvAtlasSize.w;
    float threshold = 0.5;
    float aa = 0.5 / worldRange;
    float coverage = smoothstep(threshold - aa, threshold + aa, distance);

    if (coverage <= 0.0)
        discard;
    gl_FragColor = vec4(v_color.rgb * coverage, v_color.a * coverage);
}
