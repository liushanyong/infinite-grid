$input a_position
$output v_pointCoord, v_depth

#include "bgfx_shader.sh"

uniform mat4 uView;
uniform mat4 projection;
uniform vec4 uRelativePosition;
uniform vec4 uPointSize;

void main()
{
    gl_Position = vec4(uRelativePosition.xy + a_position.xy * uPointSize.xy,
                       uRelativePosition.zw);
    v_pointCoord = a_position.xy;
    v_depth = uRelativePosition.z;
}
