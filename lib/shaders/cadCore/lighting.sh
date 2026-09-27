#ifndef CAD_CORE_LIGHTING_H
#define CAD_CORE_LIGHTING_H

#include <bgfx_shader.sh>

float cad_distributionGGX(vec3 n, vec3 h, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float nh = max(dot(n, h), 0.0);
    float nh2 = nh * nh;
    float denominator = nh2 * (a2 - 1.0) + 1.0;
    return a2 / max(3.14159265 * denominator * denominator, 0.001);
}

float cad_geometrySmith(vec3 n, vec3 v, vec3 l, float roughness)
{
    float nv = max(dot(n, v), 0.0);
    float nl = max(dot(n, l), 0.0);
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float g1 = nv / (nv * (1.0 - k) + k);
    float g2 = nl / (nl * (1.0 - k) + k);
    return g1 * g2;
}

vec3 cad_fresnelSchlick(float cosTheta, vec3 f0)
{
    float x = clamp(1.0 - cosTheta, 0.0, 1.0);
    float x2 = x * x;
    float x5 = x2 * x2 * x;
    return f0 + (1.0 - f0) * x5;
}

vec3 cad_directLight(vec3 n, vec3 v, vec3 l, vec3 radiance,
                     vec3 baseColor, vec3 f0, float metallic,
                     float roughness)
{
    vec3 h = normalize(l + v);
    float ndf = cad_distributionGGX(n, h, roughness);
    float geometry = cad_geometrySmith(n, v, l, roughness);
    vec3 fresnel = cad_fresnelSchlick(max(dot(h, v), 0.0), f0);
    vec3 specular = (ndf * geometry * fresnel) /
        max(4.0 * max(dot(n, v), 0.0) * max(dot(n, l), 0.001), 0.001);
    vec3 diffuse = (1.0 - fresnel) * (1.0 - metallic) * baseColor / 3.14159265;
    return (diffuse + specular) * radiance * max(dot(n, l), 0.0);
}

float cad_lambert(vec3 n, vec3 l)
{
    return max(dot(n, l), 0.0);
}

float cad_blinnPhong(vec3 n, vec3 l, vec3 v, float shininess)
{
    vec3 h = normalize(l + v);
    float nh = max(dot(n, h), 0.0);
    return pow(nh, max(shininess, 1.0));
}

#endif
