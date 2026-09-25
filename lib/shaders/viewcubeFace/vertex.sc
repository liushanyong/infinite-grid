$input a_position a_normal a_color0 a_region
$output v_normal v_color v_region

#include <bgfx_shader.sh>

uniform mat4 uView;
uniform mat4 uProj;

void main()
{
    vec4 viewPosition = mul(uView, vec4(a_position, 1.0));
    // a_normal is the cube face normal, but for the analytic compass ring it
    // also carries un-rotated local XY coordinates used by the fragment mask.
    // The flat ViewCube does not consume the normal for lighting, so keep it
    // in local space (matching the previous ring behavior and avoiding arcs).
    v_normal = a_normal;
    v_color = a_color0;
    v_region = a_region;
    gl_Position = mul(uProj, viewPosition);
}
