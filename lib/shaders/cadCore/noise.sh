#ifndef CAD_CORE_NOISE_H
#define CAD_CORE_NOISE_H

#include <bgfx_shader.sh>

float cad_hash(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float cad_noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = cad_hash(i);
    float b = cad_hash(i + vec2(1.0, 0.0));
    float c = cad_hash(i + vec2(0.0, 1.0));
    float d = cad_hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float cad_hatch(vec2 uv, float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    vec2 r = vec2(uv.x * c + uv.y * s, -uv.x * s + uv.y * c);
    float h = 0.0;
    h += step(0.95, sin(r.y * 20.0));
    h += step(0.95, sin(r.x * 20.0));
    h += step(0.95, sin((r.x + r.y) * 20.0));
    h += step(0.95, sin((r.x - r.y) * 20.0));
    return h;
}

#endif
