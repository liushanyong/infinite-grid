$input v_worldPos, v_normal, v_view, v_bary, v_color0, v_depth

#include <bgfx_shader.sh>
#include <cadCore/depth.sh>
#include <cadCore/lighting.sh>
#include <cadCore/noise.sh>

uniform vec4 u_baseColor;
uniform vec4 u_lightDir;
uniform vec4 u_cameraPos;
uniform vec4 u_styleParams;
uniform vec4 u_wireframeColor;
uniform vec4 u_strokeParams;
uniform vec4 u_flatShade;

vec3 cad_conceptualLighting(vec3 n, vec3 l, vec3 v, float lightWrap)
{
    float nl = dot(n, l);
    float t = clamp((nl + 1.0) * 0.5, 0.0, 1.0);
    t = pow(t, max(lightWrap, 0.01));
    vec3 warm = vec3(0.95, 0.72, 0.35);
    vec3 cool = vec3(0.35, 0.55, 0.95);
    vec3 color = mix(cool, warm, t);
    float rim = 1.0 - max(dot(n, v), 0.0);
    color += vec3_splat(smoothstep(0.0, 0.7, rim) * 0.3);
    return color;
}

vec3 cad_wireframeLighting(vec3 n, vec3 v, vec3 edgeColor, float edgePower)
{
    float edge = 1.0 - max(dot(n, v), 0.0);
    return edgeColor * pow(clamp(edge, 0.0, 1.0), max(edgePower, 1.0));
}

vec4 cad_xrayEffect(vec4 baseColor, vec3 n, vec3 v, vec3 worldPos,
                    float transparency)
{
    float nv = abs(dot(n, v));
    float edgeAlpha = 1.0 - smoothstep(0.0, 0.2, nv);
    float depthAlpha = 1.0 - smoothstep(0.0, 5.0,
                                         length(u_cameraPos.xyz - worldPos));
    float alpha = mix(edgeAlpha, depthAlpha, transparency);
    vec3 color = mix(baseColor.rgb,
                     mix(vec3(0.08, 0.35, 0.70),
                         vec3(0.85, 0.92, 1.00), nv),
                     nv);
    float pattern = sin(worldPos.x * 2.0) *
                    sin(worldPos.y * 2.0) *
                    sin(worldPos.z * 2.0);
    color += pattern * 0.1 + edgeAlpha * 0.2;
    return vec4(color, clamp(alpha * 0.7, 0.1, 0.7));
}

vec3 cad_sketchEffect(vec3 worldPos, vec3 n, vec3 v, float strokeDensity)
{
    vec2 uv = worldPos.xz * 0.5;
    float brightness = (dot(n, vec3(0.4, 0.8, 0.55)) + 1.0) * 0.5;
    float pattern = 0.0;
    if (brightness < 0.3)
    {
        pattern += cad_hatch(uv, 0.0) * 1.5;
        pattern += cad_hatch(uv * 1.5, 1.57) * 1.0;
    }
    else if (brightness < 0.7)
    {
        pattern += cad_hatch(uv, 0.0) * 0.5;
    }
    pattern += (cad_noise(uv * 10.0) - 0.5) * 0.1;
    pattern *= strokeDensity;
    float rim = 1.0 - max(dot(n, v), 0.0);
    pattern += smoothstep(0.4, 0.8, rim) * 0.5;
    vec3 color = mix(vec3(0.95, 0.91, 0.83),
                     vec3(0.10, 0.10, 0.10),
                     clamp(pattern, 0.0, 1.0));
    color += (cad_noise(worldPos.xz * 5.0) - 0.5) * 0.05;
    float edge = smoothstep(0.5, 0.6, rim);
    return mix(color, vec3(0.10, 0.10, 0.10), edge * 0.8);
}

void main()
{
    gl_FragDepth = cad_outputDepth(gl_FragCoord.z, v_depth);
    vec3 n = normalize(v_normal);
    if (u_flatShade.x > 0.5)
    {
        vec3 flatNormal = normalize(cross(dFdy(v_worldPos),
                                          dFdx(v_worldPos)));
        n = dot(flatNormal, n) < 0.0 ? -flatNormal : flatNormal;
    }
    vec3 v = normalize(v_view);
    vec3 l = normalize(u_lightDir.xyz);
    float style = u_styleParams.x;
    float metallic = u_styleParams.y;
    float roughness = clamp(u_styleParams.z, 0.03, 1.0);
    float transparency = clamp(u_styleParams.w, 0.0, 1.0);

    if (style < 0.5)
    {
        vec3 h = normalize(l + v);
        float ndf = cad_distributionGGX(n, h, roughness);
        float g = cad_geometrySmith(n, v, l, roughness);
        vec3 f0 = mix(vec3_splat(0.04),
                      (u_baseColor.rgb * v_color0.rgb), metallic);
        vec3 f = cad_fresnelSchlick(max(dot(h, v), 0.0), f0);
        vec3 ks = f;
        vec3 kd = (1.0 - ks) * (1.0 - metallic);
        vec3 specular = (ndf * g * f) /
            max(4.0 * max(dot(n, v), 0.0) * max(dot(n, l), 0.001), 0.001);
        vec3 color = vec3_splat(0.03) * (u_baseColor.rgb * v_color0.rgb) +
                     (kd * (u_baseColor.rgb * v_color0.rgb) / 3.14159265 +
                      specular) * cad_lambert(n, l);
        color = color / (color + vec3_splat(1.0));
        gl_FragColor = vec4(pow(color, vec3_splat(1.0 / 2.2)),
                            (u_baseColor.a * v_color0.a));
    }
    else if (style < 1.5)
    {
        gl_FragColor = vec4(cad_conceptualLighting(n, l, v, 2.0),
                            (u_baseColor.a * v_color0.a));
    }
    else if (style < 2.5)
    {
        discard;
    }
    else if (style < 3.5)
    {
        vec3 color = (u_baseColor.rgb * v_color0.rgb) *
                     (0.1 + cad_lambert(n, l));
        color += vec3_splat(cad_blinnPhong(n, l, v, 32.0) * 0.5);
        float gray = dot(color, vec3(0.299, 0.587, 0.114));
        gl_FragColor = vec4(vec3_splat(gray), (u_baseColor.a * v_color0.a));
    }
    else if (style < 4.5)
    {
        vec3 color = (u_baseColor.rgb * v_color0.rgb) *
                     (0.1 + cad_lambert(n, l));
        color += vec3_splat(cad_blinnPhong(n, l, v, 32.0) * 0.5);
        color = color / (color + vec3_splat(1.0));
        gl_FragColor = vec4(pow(color, vec3_splat(1.0 / 2.2)),
                            (u_baseColor.a * v_color0.a));
    }
    else if (style < 5.5)
    {
        gl_FragColor = vec4(
            cad_sketchEffect(v_worldPos, n, v, u_strokeParams.y), 1.0);
    }
    else if (style < 6.5)
    {
        vec3 color = cad_wireframeLighting(n, v, u_wireframeColor.rgb, 2.0);
        vec3 d = fwidth(v_bary);
        vec3 a3 = smoothstep(vec3_splat(0.0), d * 1.2, v_bary);
        float interior = min(min(a3.x, a3.y), a3.z);
        float edge = 1.0 - interior;
        color = mix(color, u_wireframeColor.rgb, edge);
        gl_FragColor = vec4(color, (u_baseColor.a * v_color0.a));
    }
    else if (style < 7.5)
    {
        float depthT = cad_outputDepth(gl_FragCoord.z, v_depth);
        gl_FragColor = vec4(vec3_splat(1.0 - depthT), 1.0);
    }
    else
    {
        gl_FragColor = cad_xrayEffect(u_baseColor, n, v, v_worldPos,
                                      transparency);
    }
}
