$input v_normal, v_color, v_depth, v_uv, v_worldPos

#include <bgfx_shader.sh>

SAMPLER2D(s_albedo, 0);
uniform vec4 uLogDepth;
uniform vec4 u_material;
uniform vec4 u_rAmbient;
uniform vec4 u_rDirection;
uniform vec4 u_rDirectionColor;
uniform vec4 u_rPointPositions[4];
uniform vec4 u_rPointColors[4];
uniform vec4 u_rParams;

float distributionGGX(vec3 n, vec3 h, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float nh = max(dot(n, h), 0.0);
    float nh2 = nh * nh;
    float denominator = nh2 * (a2 - 1.0) + 1.0;
    return a2 / max(3.14159265 * denominator * denominator, 0.001);
}

float geometrySmith(vec3 n, vec3 v, vec3 l, float roughness)
{
    float nv = max(dot(n, v), 0.0);
    float nl = max(dot(n, l), 0.0);
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float g1 = nv / (nv * (1.0 - k) + k);
    float g2 = nl / (nl * (1.0 - k) + k);
    return g1 * g2;
}

vec3 fresnelSchlick(float cosTheta, vec3 f0)
{
    float x = clamp(1.0 - cosTheta, 0.0, 1.0);
    float x2 = x * x;
    float x5 = x2 * x2 * x;
    return f0 + (1.0 - f0) * x5;
}

vec3 directLight(vec3 n, vec3 v, vec3 l, vec3 radiance,
                 vec3 baseColor, vec3 f0, float metallic, float roughness)
{
    vec3 h = normalize(l + v);
    float ndf = distributionGGX(n, h, roughness);
    float geometry = geometrySmith(n, v, l, roughness);
    vec3 fresnel = fresnelSchlick(max(dot(h, v), 0.0), f0);
    vec3 specular = (ndf * geometry * fresnel) /
        max(4.0 * max(dot(n, v), 0.0) * max(dot(n, l), 0.001), 0.001);
    vec3 diffuse = (1.0 - fresnel) * (1.0 - metallic) * baseColor / 3.14159265;
    return (diffuse + specular) * radiance * max(dot(n, l), 0.0);
}

float outputDepth(float rawDepth, float viewDepth)
{
    if (uLogDepth.x < 0.5)
        return rawDepth;
    if (viewDepth < uLogDepth.y || viewDepth > uLogDepth.z)
        discard;
    float numerator = log2(max(viewDepth / uLogDepth.y, 1.0));
    float denominator = log2(max(uLogDepth.z / uLogDepth.y, 1.000001));
    return clamp(numerator / denominator, 0.0, 1.0);
}

void main()
{
    gl_FragDepth = outputDepth(gl_FragCoord.z, v_depth);
    vec3 n = normalize(v_normal);
    vec3 v = normalize(-v_worldPos);
    vec3 l = normalize(u_rDirection.xyz);
    vec3 baseColor = texture2D(s_albedo, v_uv).rgb * v_color.rgb;
    float metallic = clamp(u_material.x, 0.0, 1.0);
    float roughness = clamp(u_material.y, 0.03, 1.0);
    vec3 f0 = mix(vec3_splat(0.04), baseColor, metallic);
    vec3 color = u_rAmbient.rgb * baseColor;
    color += directLight(n, v, l, u_rDirectionColor.rgb *
                         u_rDirection.w, baseColor, f0, metallic, roughness);

    for (int i = 0; i < 4; ++i)
    {
        if (float(i) >= u_rParams.x)
            break;
        vec3 delta = u_rPointPositions[i].xyz - v_worldPos;
        float distance2 = max(dot(delta, delta), 0.01);
        vec3 light = normalize(delta);
        float radius = max(u_rPointPositions[i].w, 1.0);
        float attenuation = 1.0 / (1.0 + distance2 / (radius * radius));
        color += directLight(n, v, light, u_rPointColors[i].rgb *
                             attenuation, baseColor, f0, metallic, roughness);
    }

    color += baseColor * u_material.z;
    color = color / (color + vec3_splat(1.0));
    color = pow(color, vec3_splat(1.0 / 2.2));
    float alpha = v_color.a;
    if (u_material.w > 0.0 && alpha < u_material.w)
        discard;
    gl_FragColor = vec4(color, alpha);
}
