$input v_uv

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

uniform vec4 uPresentParams;

float presentLuma(vec3 color)
{
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec3 presentFxaa(vec2 uv)
{
    vec2 texel = vec2(uPresentParams.y, uPresentParams.z);
    vec3 rgbNW = texture2D(s_texColor, uv + vec2(-1.0, -1.0) * texel).rgb;
    vec3 rgbNE = texture2D(s_texColor, uv + vec2( 1.0, -1.0) * texel).rgb;
    vec3 rgbSW = texture2D(s_texColor, uv + vec2(-1.0,  1.0) * texel).rgb;
    vec3 rgbSE = texture2D(s_texColor, uv + vec2( 1.0,  1.0) * texel).rgb;
    vec3 rgbM = texture2D(s_texColor, uv).rgb;

    float lumaNW = presentLuma(rgbNW);
    float lumaNE = presentLuma(rgbNE);
    float lumaSW = presentLuma(rgbSW);
    float lumaSE = presentLuma(rgbSE);
    float lumaM = presentLuma(rgbM);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE),
                                   min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE),
                                   max(lumaSW, lumaSE)));

    vec2 direction = vec2(-((lumaNW + lumaNE) - (lumaSW + lumaSE)),
                           ((lumaNW + lumaSW) - (lumaNE + lumaSE)));
    float reduction = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * 0.25,
                          1.0 / 128.0);
    float rcpDirectionMin = 1.0 / (min(abs(direction.x),
                                       abs(direction.y)) + reduction);
    direction = clamp(direction * rcpDirectionMin,
                      vec2_splat(-8.0), vec2_splat(8.0)) * texel;

    vec3 rgbA = 0.5 * (texture2D(s_texColor, uv + direction * (1.0 / 3.0 - 0.5)).rgb +
                       texture2D(s_texColor, uv + direction * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture2D(s_texColor, uv + direction * -0.5).rgb +
        texture2D(s_texColor, uv + direction * 0.5).rgb);
    float lumaB = presentLuma(rgbB);
    return (lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB;
}

void main()
{
    vec3 color = uPresentParams.x > 0.5
        ? presentFxaa(v_uv)
        : texture2D(s_texColor, v_uv).rgb;
    gl_FragColor = vec4(color, 1.0);
}
