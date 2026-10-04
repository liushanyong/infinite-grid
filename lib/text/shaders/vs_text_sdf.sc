$input a_position, a_texcoord0, a_color0
$output v_uv, v_color

#include <bgfx_shader.sh>

// u_modelViewProj is a bgfx predefined uniform: bound automatically from
// setViewTransform (view * projection) with an identity model transform.
// Vertices arrive in view space; no manual matrix binding needed.
void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_uv = a_texcoord0;
    v_color = a_color0;
}
