#ifndef CAD_CORE_LINE_H
#define CAD_CORE_LINE_H

#include <bgfx_shader.sh>

float cad_lineAlpha(float edge, float edgeSoftness)
{
    float distanceToCenter = 1.0 - abs(edge);
    float aa = clamp(edgeSoftness, 0.02, 1.0);
    return smoothstep(0.0, aa, distanceToCenter);
}

float cad_dashAlpha(float along, float dashLength, float gapLength)
{
    float cycle = dashLength + gapLength;
    if (cycle <= 0.0)
        return 1.0;
    float phase = fract(along / cycle);
    return step(phase, clamp(dashLength / cycle, 0.0, 1.0));
}

float cad_roundCap(vec2 uv)
{
    return 1.0 - step(1.0, length(uv));
}

float cad_squareCap(vec2 uv)
{
    return 1.0 - step(1.0, max(abs(uv.x), abs(uv.y)));
}

float cad_triangleCap(vec2 uv)
{
    return 1.0 - step(1.0, abs(uv.x) + abs(uv.y));
}

float cad_cap(vec2 uv, float capStyle)
{
    if (capStyle < 1.5)
        return cad_squareCap(uv);
    if (capStyle < 2.5)
        return cad_roundCap(uv);
    if (capStyle < 3.5)
        return cad_triangleCap(uv);
    return 1.0 - step(1.0, abs(uv.y));
}

#endif
