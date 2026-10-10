$input v_uv

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

uniform vec4 u_outline_params;
uniform vec4 u_outline_color;

float readId(vec2 uv)
{
    vec4 bytes = texture2D(s_texColor, uv) * 255.0;
    // The pick pass packs the object id with its low byte in the blue
    // channel (u_object_index encoding), so decode with matching weights or
    // the decoded id never equals u_outline_params.x and nothing is drawn.
    return bytes.b + bytes.g * 256.0 +
           bytes.r * 65536.0 + bytes.a * 16777216.0;
}

void main()
{
    float selectedId = u_outline_params.x;
    bool outlineAll = u_outline_params.y > 0.5;
    float id = readId(v_uv);
    vec4 centerBytes = texture2D(s_texColor, v_uv);
    float minByte = min(min(centerBytes.r, centerBytes.g),
                        min(centerBytes.b, centerBytes.a));
    bool background = minByte > 254.5;
    if (outlineAll)
    {
        if (background)
        {
            gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
            return;
        }
    }
    else
    {
        if (id != selectedId)
        {
            gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
            return;
        }
        if (selectedId < 0.5)
        {
            gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
            return;
        }
    }

    float left  = readId(v_uv - vec2(u_viewTexel.x, 0.0));
    float right = readId(v_uv + vec2(u_viewTexel.x, 0.0));
    float down  = readId(v_uv - vec2(0.0, u_viewTexel.y));
    float up    = readId(v_uv + vec2(0.0, u_viewTexel.y));

    bool selected = outlineAll ? !background : id == selectedId;
    bool boundary = outlineAll
        ? left != id || right != id || down != id || up != id
        : left != selectedId || right != selectedId ||
          down != selectedId || up != selectedId;
    float tintAlpha = selected && !boundary ? 0.10 : 0.0;
    float rimAlpha = selected && boundary ? 0.48 : 0.0;
    gl_FragColor = vec4(u_outline_color.rgb,
                        max(tintAlpha, rimAlpha) * u_outline_color.a);
}
