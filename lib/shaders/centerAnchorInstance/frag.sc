$input v_normal, v_color, v_viewDepth, v_uv, v_worldPos

#include <bgfx_shader.sh>

SAMPLER2D(s_albedo, 0);
uniform vec4 uLogDepth;
uniform vec4 uEdgeOverride;
uniform vec4 uPrimParams;
uniform vec4 uMeshSurface;

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

vec2 triplanarUv(vec3 position, vec3 normal)
{
    vec3 scaled = abs(position) * 0.01;
    vec3 weight = abs(normal);
    float selectedZ = step(weight.z, max(weight.x, weight.y));
    float selectedX = step(weight.x, weight.y) * (1.0 - selectedZ);
    float selectedY = 1.0 - selectedX - selectedZ;
    return scaled.xy * selectedZ + scaled.zy * selectedX + scaled.xz * selectedY;
}

void main()
{
    gl_FragDepth = outputDepth(gl_FragCoord.z, v_viewDepth);
    vec3 normal = normalize(v_normal);
    vec3 lightDirection = normalize(vec3(0.4, 0.8, 0.55));
    float diffuse = max(dot(normal, lightDirection), 0.0);
    vec2 uv = mix(v_uv, triplanarUv(v_worldPos, normal),
                  clamp(uMeshSurface.y, 0.0, 1.0));
    vec3 albedo = texture2D(s_albedo, uv).rgb;
    float shading = mix(1.0, 0.45 + 0.75 * diffuse,
                        clamp(uMeshSurface.x, 0.0, 1.0));
    vec3 color = albedo * v_color.xyz * shading;
    float alpha = v_color.w;
    if (uEdgeOverride.x > 0.5)
    {
        color = vec3(0.95, 0.94, 0.92);
        alpha = 1.0;
    }
    gl_FragColor = vec4(color, alpha);
}
