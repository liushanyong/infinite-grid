$input a_texcoord0
$output v_endpoint, v_viewDepth

#include "bgfx_shader.sh"

uniform mat4 projection;
uniform vec4 uViewStart;
uniform vec4 uViewEnd;
uniform vec4 uLogDepth;

void main()
{
    float endpoint = a_texcoord0.x;
    v_endpoint = endpoint;
    vec4 viewPosition = mix(uViewStart, uViewEnd, endpoint);
    v_viewDepth = -viewPosition.z;
    gl_Position = mul(projection, viewPosition);
}
