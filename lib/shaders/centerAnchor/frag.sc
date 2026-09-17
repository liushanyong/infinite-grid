$input v_normal

#include "bgfx_shader.sh"

uniform vec4 uCubeOpacity;
uniform vec4 uObjectColor;

void main()
{
    vec3 normal = normalize(v_normal);
    vec3 lightDirection = normalize(vec3(0.4, 0.8, 0.55));
    float diffuse = max(dot(normal, lightDirection), 0.0);
    vec3 color = uObjectColor.xyz * (0.45 + 0.75 * diffuse);
    gl_FragColor = vec4(color, uCubeOpacity.x);
}
