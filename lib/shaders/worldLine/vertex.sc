$input a_texcoord0
$output v_endpoint

#include "bgfx_shader.sh"

uniform mat4 uView;
uniform mat4 projection;
uniform vec4 uRelativeStart;
uniform vec4 uRelativeEnd;

void main()
{
    float endpoint = a_texcoord0.x;
    v_endpoint = endpoint;
    vec3 relativePosition = mix(uRelativeStart.xyz, uRelativeEnd.xyz, endpoint);
    gl_Position = mul(projection, mul(uView, vec4(relativePosition, 1.0)));
}
