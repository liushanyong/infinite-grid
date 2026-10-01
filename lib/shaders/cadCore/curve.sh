#ifndef CAD_CORE_CURVE_H
#define CAD_CORE_CURVE_H

#include <bgfx_shader.sh>

#define CAD_CURVE_MAX_CP 16

vec3 cad_deCasteljau(vec3 cp[CAD_CURVE_MAX_CP], int degree, float t)
{
    vec3 work[CAD_CURVE_MAX_CP];
    float u = 1.0 - t;
    for (int i = 0; i <= degree; ++i)
        work[i] = cp[i];
    for (int level = 1; level <= degree; ++level)
    {
        for (int i = 0; i <= degree - level; ++i)
            work[i] = u * work[i] + t * work[i + 1];
    }
    return work[0];
}

int cad_findSpan(float knots[CAD_CURVE_MAX_CP], int numCP,
                 int degree, float t)
{
    // Standard knot-span convention: span s covers [knots[s], knots[s+1])
    // and uses control points cp[s-degree..s].  Valid spans are therefore
    // [degree, numCP-1].  A previous edit shrank the range to
    // [degree, numCP-degree-1], which collapsed multi-span curves (e.g. a
    // 5-point cubic never reached cp4).
    int firstSpan = degree;
    int lastSpan = numCP - 1;
    firstSpan = clamp(firstSpan, 0, CAD_CURVE_MAX_CP - 1);
    lastSpan = clamp(lastSpan, firstSpan, CAD_CURVE_MAX_CP - 1);

    // The final knot of a knot vector with numCP control points and
    // degree p is at index numCP + degree.  The previous numCP - 1 index
    // landed in the interior for clamped curves and collapsed every
    // t > 0 sample into the last span.
    int lastKnotIndex = numCP + degree;
    if (lastKnotIndex > CAD_CURVE_MAX_CP - 1)
        lastKnotIndex = CAD_CURVE_MAX_CP - 1;
    if (lastKnotIndex < 0)
        lastKnotIndex = 0;
    if (t >= knots[lastKnotIndex])
        return lastSpan;
    if (t <= knots[degree])
        return firstSpan;
    for (int span = firstSpan; span < lastSpan; ++span)
    {
        if (t < knots[span + 1])
            return span;
    }
    return lastSpan;
}

void cad_bsplineBasis(float knots[CAD_CURVE_MAX_CP], int degree,
                      int span, float t, out float outBasis[CAD_CURVE_MAX_CP])
{
    float left[CAD_CURVE_MAX_CP];
    float right[CAD_CURVE_MAX_CP];
    outBasis[0] = 1.0;
    for (int j = 1; j <= degree; ++j)
    {
        left[j] = t - knots[span + 1 - j];
        right[j] = knots[span + j] - t;
        float saved = 0.0;
        for (int r = 0; r < j; ++r)
        {
            float denominator = right[r + 1] + left[j - r];
            float temp = denominator != 0.0 ? outBasis[r] / denominator : 0.0;
            outBasis[r] = saved + right[r + 1] * temp;
            saved = left[j - r] * temp;
        }
        outBasis[j] = saved;
    }
}

vec3 cad_bspline(vec3 cp[CAD_CURVE_MAX_CP], float knots[CAD_CURVE_MAX_CP],
                 int degree, int numCP, float t)
{
    // cad_bsplineBasis() reads knots[span + degree]; keep it inside the
    // CAD_CURVE_MAX_CP window shared with the CPU sampler.
    int maxSpan = numCP - 1;
    int knotLimit = CAD_CURVE_MAX_CP - 1 - degree;
    if (maxSpan > knotLimit)
        maxSpan = knotLimit;
    if (maxSpan < degree)
        maxSpan = degree;
    int span = clamp(cad_findSpan(knots, numCP, degree, t),
                     degree, maxSpan);
    vec3 result = vec3_splat(0.0);
    float basis[CAD_CURVE_MAX_CP];
    cad_bsplineBasis(knots, degree, span, t, basis);
    for (int i = 0; i <= degree; ++i)
    {
        // cad_bsplineBasis() is relative to [span-degree, span].
        result += basis[i] * cp[span - degree + i];
    }
    return result;
}

vec3 cad_nurbs(vec3 cp[CAD_CURVE_MAX_CP], float knots[CAD_CURVE_MAX_CP],
               float weights[CAD_CURVE_MAX_CP], int degree, int numCP,
               float t)
{
    // cad_bsplineBasis() reads knots[span + degree]; keep it inside the
    // CAD_CURVE_MAX_CP window shared with the CPU sampler.
    int maxSpan = numCP - 1;
    int knotLimit = CAD_CURVE_MAX_CP - 1 - degree;
    if (maxSpan > knotLimit)
        maxSpan = knotLimit;
    if (maxSpan < degree)
        maxSpan = degree;
    int span = clamp(cad_findSpan(knots, numCP, degree, t),
                     degree, maxSpan);
    vec3 numerator = vec3_splat(0.0);
    float denominator = 0.0;
    float basis[CAD_CURVE_MAX_CP];
    cad_bsplineBasis(knots, degree, span, t, basis);
    for (int i = 0; i <= degree; ++i)
    {
        int cpIndex = span - degree + i;
        float weight = basis[i] * weights[cpIndex];
        numerator += weight * cp[cpIndex];
        denominator += weight;
    }
    return denominator > 0.0 ? numerator / denominator : vec3_splat(0.0);
}

vec3 cad_arc(vec3 center, vec3 axisU, vec3 axisV,
             float radius, float startAngle, float sweep, float t)
{
    float angle = startAngle + sweep * t;
    return center + radius * (cos(angle) * normalize(axisU) +
                              sin(angle) * normalize(axisV));
}

#endif
