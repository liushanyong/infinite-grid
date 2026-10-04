$input v_uv, v_color

#include <bgfx_shader.sh>

// Flat filled text: glyphs arrive as exact outline-triangulated polygons,
// so the fragment stage is a plain color pass (resolution independent).
void main()
{
    gl_FragColor = v_color;
}
