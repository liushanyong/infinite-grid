$input v_uv

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texId, 1);

uniform vec4 uPresentParams;
uniform vec4 u_outline_params;
uniform vec4 u_outline_color;

float presentLuma(vec3 color)
{
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec3 presentFxaa(vec2 uv)
{
    vec2 texel = u_outline_params.zw;
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

vec3 presentIdDebug(vec2 uv)
{
    vec4 bytes = texture2D(s_texColor, uv) * 255.0;
    bool background = bytes.r > 254.5 && bytes.g > 254.5 &&
                      bytes.b > 254.5 && bytes.a > 254.5;
    if (background)
        return vec3(1.0, 1.0, 1.0);

    float id = bytes.r * 65536.0 + bytes.g * 256.0 +
               bytes.b + bytes.a * 16777216.0;
    float minId = uPresentParams.y;
    float maxId = uPresentParams.z;
    float normalized = maxId > minId
        ? clamp((id - minId) / (maxId - minId), 0.0, 1.0)
        : 0.5;

    // Deliberately simple debug colormap: low ID -> cyan/green,
    // high ID -> yellow/red.
    return vec3(0.15 + 0.85 * normalized,
                0.15 + 0.85 * (1.0 - normalized),
                0.30 + 0.70 * (1.0 - abs(normalized * 2.0 - 1.0)));
}

float presentReadId(vec2 uv)
{
    vec4 bytes = texture2D(s_texId, uv) * 255.0;
    return bytes.b + bytes.g * 256.0 +
           bytes.r * 65536.0 + bytes.a * 16777216.0;
}

vec3 presentSelection(vec2 uv, vec3 color)
{
    float id = presentReadId(uv);
    vec4 idBytes = texture2D(s_texId, uv) * 255.0;
    bool background = min(min(idBytes.r, idBytes.g),
                          min(idBytes.b, idBytes.a)) > 254.5;
    bool outlineAll = u_outline_params.y > 0.5;
    float selectedId = u_outline_params.x;
    bool selected = !outlineAll && selectedId > 0.5 && id == selectedId;
    if (!selected && (!outlineAll || background))
        return color;

    vec2 texel = vec2(uPresentParams.y, uPresentParams.z);
    float left = presentReadId(uv - vec2(texel.x, 0.0));
    float right = presentReadId(uv + vec2(texel.x, 0.0));
    float down = presentReadId(uv - vec2(0.0, texel.y));
    float up = presentReadId(uv + vec2(0.0, texel.y));
    bool boundary = outlineAll
        ? left != id || right != id || down != id || up != id
        : left != selectedId || right != selectedId ||
          down != selectedId || up != selectedId;

    if (boundary)
        color = mix(color, u_outline_color.rgb,
                    0.48 * u_outline_color.a);
    return color;
}

void main()
{
    bool idDebug = uPresentParams.w > 0.5;
    vec3 color = idDebug
        ? presentIdDebug(v_uv)
        : (uPresentParams.x > 0.5
            ? presentFxaa(v_uv)
            : texture2D(s_texColor, v_uv).rgb);
    if (!idDebug)
        color = presentSelection(v_uv, color);
    gl_FragColor = vec4(color, 1.0);
}
