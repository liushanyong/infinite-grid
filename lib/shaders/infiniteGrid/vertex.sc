$input a_position
$output v_rayOrigin, v_rayDir, v_ndc

#include "bgfx_shader.sh"

uniform mat4 uInvViewProj;
uniform vec4 uCamFront;
uniform vec4 uOrthoPlaneCenter;
uniform vec4 uOrthoRight;
uniform vec4 uOrthoUp;
uniform vec4 uOriginRelative;
uniform vec4 uIsOrtho;

void main()
{
    vec2 ndc = a_position.xy;
    gl_Position = vec4(ndc, 1.0, 1.0);
    v_ndc = ndc;

    vec3 nearP;
    vec3 dir;
    if (uIsOrtho.x > 0.5)
    {
        nearP = uOrthoPlaneCenter.xyz;
        dir = uCamFront.xyz;
    }
    else
    {
        vec4 nearH = mul(uInvViewProj, vec4(ndc, -1.0, 1.0));
        vec4 sampleH = mul(uInvViewProj, vec4(ndc, 0.0, 1.0));
        vec3 nearW = nearH.xyz / nearH.w;
        vec3 sampleW = sampleH.xyz / sampleH.w;
        nearP = nearW - uOriginRelative.xyz;
        dir = sampleW - nearW;
    }

    v_rayOrigin = nearP;
    v_rayDir = dir;
}
