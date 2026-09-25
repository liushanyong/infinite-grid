$input a_position a_texcoord0 a_color0 a_texcoord1
$output v_uv v_color

#include <bgfx_shader.sh>

uniform vec4 uScreen; // width, height, 1 / width, 1 / height
uniform vec4 uDepth;  // eye-Z scale, eye-Z offset, unused, unused

void main()
{
    vec2 clipXY = a_position.xy * uScreen.zw * 2.0 - 1.0;
    clipXY.y = -clipXY.y; // ImGui Y is down, framebuffer clip Y is up.
    v_uv = a_texcoord0;
    v_color = a_color0;
    gl_Position = vec4(clipXY, uDepth.x * a_texcoord1 + uDepth.y, 1.0);
}
