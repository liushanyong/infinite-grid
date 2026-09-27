$input v_normal, v_color, v_depth, v_uv, v_worldPos

#include <bgfx_shader.sh>
#include <cadCore/depth.sh>
#include <cadCore/lighting.sh>

SAMPLER2D(s_albedo, 0);
uniform vec4 u_material;
uniform vec4 u_rAmbient;
uniform vec4 u_rDirection;
uniform vec4 u_rDirectionColor;
uniform vec4 u_rPointPositions[4];
uniform vec4 u_rPointColors[4];
uniform vec4 u_rParams;

void main()
{
    gl_FragDepth = cad_outputDepth(gl_FragCoord.z, v_depth);
    vec3 n = normalize(v_normal);
    vec3 v = normalize(-v_worldPos);
    vec3 l = normalize(u_rDirection.xyz);
    vec3 baseColor = texture2D(s_albedo, v_uv).rgb * v_color.rgb;
    float metallic = clamp(u_material.x, 0.0, 1.0);
    float roughness = clamp(u_material.y, 0.03, 1.0);
    vec3 f0 = mix(vec3_splat(0.04), baseColor, metallic);
    vec3 color = u_rAmbient.rgb * baseColor;
    color += cad_directLight(n, v, l, u_rDirectionColor.rgb *
                             u_rDirection.w, baseColor, f0, metallic,
                             roughness);

    for (int i = 0; i < 4; ++i)
    {
        if (float(i) >= u_rParams.x)
            break;
        vec3 delta = u_rPointPositions[i].xyz - v_worldPos;
        float distance2 = max(dot(delta, delta), 0.01);
        vec3 light = normalize(delta);
        float radius = max(u_rPointPositions[i].w, 1.0);
        float attenuation = 1.0 / (1.0 + distance2 / (radius * radius));
        color += cad_directLight(n, v, light, u_rPointColors[i].rgb *
                                 attenuation, baseColor, f0, metallic,
                                 roughness);
    }

    color += baseColor * u_material.z;
    color = color / (color + vec3_splat(1.0));
    color = pow(color, vec3_splat(1.0 / 2.2));
    float alpha = v_color.a;
    if (u_material.w > 0.0 && alpha < u_material.w)
        discard;
    gl_FragColor = vec4(color, alpha);
}
