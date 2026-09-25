$input v_uv v_color

#include <bgfx_shader.sh>

SAMPLER2D(s_font, 0);

void main()
{
    vec4 texel = texture2D(s_font, v_uv);
    // Premultiplied output for the private ViewCube framebuffer.
    gl_FragColor = vec4(v_color.rgb * texel.rgb * texel.a, v_color.a * texel.a);
}
