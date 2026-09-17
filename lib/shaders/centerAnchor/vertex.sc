$input a_position, a_normal
$output v_normal

#include "bgfx_shader.sh"

uniform mat4 uView;
uniform mat4 projection;
uniform vec4 uModelRelativePosition;

void main()
{
    mat4 model = u_model[0];
    v_normal = mul(mat3(model[0].xyz, model[1].xyz, model[2].xyz), a_normal);

    mat4 modelNoTranslation = model;
    modelNoTranslation[3].xyz = vec3(0.0, 0.0, 0.0);
    vec3 worldPosRebased = mul(modelNoTranslation, vec4(a_position, 1.0)).xyz + uModelRelativePosition.xyz;

    gl_Position = mul(projection, mul(uView, vec4(worldPosRebased, 1.0)));
}
