$input a_position
$output v_color, v_depth

#include <bgfx_shader.sh>
#include <cadCore/curve.sh>

uniform mat4 uView;
uniform mat4 projection;
uniform vec4 uLayerOffset;
uniform vec4 uCurveCP[CAD_CURVE_MAX_CP];
uniform vec4 uCurveKnot[4];
uniform vec4 uCurveParams;
uniform vec4 uCurveColor;
uniform vec4 uArc;

void main()
{
    vec3 cp[CAD_CURVE_MAX_CP];
    float knots[CAD_CURVE_MAX_CP];
    float weights[CAD_CURVE_MAX_CP];
    for (int i = 0; i < CAD_CURVE_MAX_CP; ++i)
    {
        cp[i] = uCurveCP[i].xyz;
        weights[i] = max(uCurveCP[i].w, 0.001);
        knots[i] = uCurveKnot[i / 4][i - 4 * (i / 4)];
    }

    int algorithm = int(uCurveParams.x);
    int degree = int(uCurveParams.y);
    int numCP = int(uCurveParams.z);
    float t = a_position.x;
    vec3 position = vec3_splat(0.0);
    if (algorithm == 0)
        position = cad_deCasteljau(cp, degree, t);
    else if (algorithm == 1)
        position = cad_bspline(cp, knots, degree, numCP, t);
    else if (algorithm == 2)
        position = cad_nurbs(cp, knots, weights, degree, numCP, t);
    else
        position = cad_arc(cp[0], cp[1], cp[2], uArc.x,
                           uArc.y, uArc.z, t);

    vec4 viewPosition = mul(uView, vec4(position, 1.0));
    viewPosition.z += uLayerOffset.x;
    v_color = uCurveColor;
    v_depth = -viewPosition.z;
    gl_Position = mul(projection, viewPosition);
}
