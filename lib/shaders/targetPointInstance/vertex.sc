$input a_position, i_data0, i_data1, i_data2
$output v_pointCoord, v_color

#include "bgfx_shader.sh"

void main()
{
    // CPU has already projected the rebase-relative center and applied the
    // depth bias/log mapping. This keeps each impostor exactly compatible
    // with the existing single-point path while enabling one submission.
    gl_Position = vec4(i_data0.xy + a_position.xy * i_data1.xy, i_data0.zw);
    v_pointCoord = a_position.xy;
    v_color = i_data2;
}
