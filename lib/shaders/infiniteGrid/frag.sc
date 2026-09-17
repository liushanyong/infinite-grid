$input v_rayOrigin, v_rayDir, v_ndc

#include "bgfx_shader.sh"

uniform mat4 uViewProj;
uniform vec4 uGroundRelativeY;
uniform vec4 uOriginRelative;
uniform vec4 uPlaneNormal;
uniform vec4 uPlaneTangentU;
uniform vec4 uPlaneTangentV;
uniform vec4 uAxisColorU;
uniform vec4 uAxisColorV;
uniform vec4 uStartAxisOrigin;
uniform vec4 uStartAxisDirection;
uniform vec4 uStartAxisVisible;
uniform vec4 uStartAxisLine;
uniform vec4 uAxisOriginGridRelative;
uniform vec4 uAxisLineX;
uniform vec4 uAxisLineZ;
uniform vec4 uStep;
uniform vec4 uAxisVisible;
uniform vec4 uScreenHeight;
uniform vec4 uScreenWidth;
uniform vec4 uIsOrtho;
uniform vec4 uOrthoPlaneValid;
uniform vec4 uOrthoPlaneCenter;
uniform vec4 uOrthoRight;
uniform vec4 uOrthoUp;
uniform vec4 uGridColorMajor;
uniform vec4 uGridColorMinor;
uniform vec4 uGridOpacity;

float preciseMod(float a, float b)
{
    return a - floor(a / b + 0.5) * b;
}

float gridLine1D(float coord, float step, float halfWidthPx,
                 float derivative)
{
    float dist = abs(preciseMod(coord, step));
    float aa = derivative;
    float width = halfWidthPx * derivative;
    return 1.0 - smoothstep(width - aa, width + aa, dist);
}

float filteredGridLine(float coord, float step, float halfWidthPx,
                       float derivative, float minSpacingPx)
{
    float spacingPx = step / max(derivative, 1e-8);
    float averageCoverage =
        clamp((2.0 * halfWidthPx) / spacingPx, 0.0, 1.0);
    float fadeStartPx = minSpacingPx * 0.5;
    if (spacingPx <= fadeStartPx)
        return averageCoverage;

    float pointLine = gridLine1D(coord, step, halfWidthPx, derivative);
    float pointWeight = smoothstep(fadeStartPx, minSpacingPx, spacingPx);
    return mix(averageCoverage, pointLine, pointWeight);
}

float axisLine1D(vec4 axisLine, vec2 ndc)
{
    float signedDistance = dot(axisLine.xy, ndc) + axisLine.z;
    vec2 pixelGradient = vec2(
        axisLine.x * 2.0 / uScreenWidth.x,
        axisLine.y * 2.0 / uScreenHeight.x);
    float distancePx =
        abs(signedDistance) / max(length(pixelGradient), 1e-20);
    return 1.0 - smoothstep(1.5, 3.5, distancePx);
}

vec2 planeCoordinates(vec3 planePoint)
{
    return vec2(dot(planePoint, uPlaneTangentU.xyz),
                dot(planePoint, uPlaneTangentV.xyz));
}

// A view direction within five degrees of the grid plane is too grazing to
// display reliably; hide it instead of letting aliasing dominate the screen.
const float kMinPlaneCos = 0.087155743; // sin(5 degrees)

void main()
{
    vec3 p;
    vec2 derivative;
    float grazingFade;

    if (uIsOrtho.x > 0.5)
    {
        if (uOrthoPlaneValid.x < 0.5)
            discard;

        p = uOrthoPlaneCenter.xyz
          + uOrthoRight.xyz * v_ndc.x
          + uOrthoUp.xyz * v_ndc.y;

        vec3 dCoordDxWorld = uOrthoRight.xyz * (2.0 / uScreenWidth.x);
        vec3 dCoordDyWorld = uOrthoUp.xyz * (2.0 / uScreenHeight.x);
        vec2 orthoDerivative =
            abs(vec2(dot(dCoordDxWorld, uPlaneTangentU.xyz),
                     dot(dCoordDxWorld, uPlaneTangentV.xyz))) +
            abs(vec2(dot(dCoordDyWorld, uPlaneTangentU.xyz),
                     dot(dCoordDyWorld, uPlaneTangentV.xyz)));
        derivative = clamp(orthoDerivative,
                           vec2(1e-8, 1e-8), vec2(1e30, 1e30));
        grazingFade = 1.0;
    }
    else
    {
        vec3 normal = normalize(uPlaneNormal.xyz);
        float dirN = dot(v_rayDir, normal);
        float originN = dot(v_rayOrigin, normal);
        float t = abs(dirN) > 1e-6
                      ? (-originN / dirN)
                      : -1.0;
        p = v_rayOrigin + t * v_rayDir;
        vec2 perspectiveDerivative = abs(dFdx(planeCoordinates(p))) +
                                     abs(dFdy(planeCoordinates(p)));
        derivative = clamp(perspectiveDerivative,
                           vec2(1e-8, 1e-8), vec2(1e30, 1e30));
        vec3 rayDir = normalize(v_rayDir);
        float planeCos = abs(dot(rayDir, normal));
        if (planeCos < kMinPlaneCos)
            discard;
        grazingFade = 1.0;
        if (t < 0.0)
            discard;
    }

    vec3 relativePos = uOriginRelative.xyz + p;
    vec4 clipPos = mul(uViewProj, vec4(relativePos, 1.0));
    // The CPU supplies a D3D-depth view/projection, so clipPos.z / clipPos.w
    // is already in [0, 1] (equivalent to the reference GL shader's
    // ndc_z * 0.5 + 0.5). Emulate glDepthRange(1/65536, 1.0) as a linear
    // remap. Values > 1 (beyond far plane) must fail against the cleared
    // depth just like the reference does; SV_Depth is clamped to [0,1]
    // for UNORM depth buffers on D3D, so an out-of-range value would
    // silently become 1.0 and equal-pass, painting a persistent "outline"
    // of anti-aliased grid edges on the far plane. Discard those pixels
    // explicitly. Values < 0 (in front of near plane) still fail LEQUAL
    // once clamped to 0.
    float rawDepth = clipPos.z / clipPos.w;
    if (rawDepth > 1.0)
        discard;
    gl_FragDepth = 1.0 / 65536.0 + (1.0 - 1.0 / 65536.0) * rawDepth;

    float minorStep = uStep.x * 0.1;
    vec2 gridCoord = planeCoordinates(p);
    float majorX = filteredGridLine(gridCoord.x, uStep.x, 1.5,
                                    derivative.x, 4.0);
    float majorZ = filteredGridLine(gridCoord.y, uStep.x, 1.5,
                                    derivative.y, 4.0);
    float minorX = filteredGridLine(gridCoord.x, minorStep, 0.8,
                                    derivative.x, 2.5);
    float minorZ = filteredGridLine(gridCoord.y, minorStep, 0.8,
                                    derivative.y, 2.5);
    float major = max(majorX, majorZ);
    float minor = max(minorX, minorZ);

    float axisU = 0.0;
    float axisV = 0.0;
    if (uIsOrtho.x > 0.5)
    {
        if (uAxisVisible.x > 0.5)
            axisU = axisLine1D(uAxisLineX, v_ndc);
        if (uAxisVisible.y > 0.5)
            axisV = axisLine1D(uAxisLineZ, v_ndc);
    }
    else
    {
        if (uAxisVisible.x > 0.5)
        {
            float axisUDist = abs(gridCoord.y - uAxisOriginGridRelative.y);
            axisU = 1.0 - smoothstep(1.5 * derivative.y,
                                     3.5 * derivative.y, axisUDist);
        }
        if (uAxisVisible.y > 0.5)
        {
            float axisVDist = abs(gridCoord.x - uAxisOriginGridRelative.x);
            axisV = 1.0 - smoothstep(1.5 * derivative.x,
                                     3.5 * derivative.x, axisVDist);
        }
    }

    float startAxis = 0.0;
    if (uStartAxisVisible.x > 0.5)
    {
        // Use an NDC line computed on the CPU.  A plane-space derivative can
        // become degenerate when a custom axis is highly foreshortened, and
        // the old smoothstep then covers the whole viewport.
        startAxis = axisLine1D(uStartAxisLine, v_ndc);
    }

    vec3 color = uGridColorMinor.xyz;
    float alpha = minor * 0.2;

    color = mix(color, uGridColorMajor.xyz, major);
    alpha = max(alpha, major * 0.5);

    // RGB maps to XYZ.  Each plane shows its two in-plane axes.
    color = mix(color, uAxisColorU.xyz, axisU);
    color = mix(color, uAxisColorV.xyz, axisV);
    color = mix(color, vec3(1.0, 0.85, 0.1), startAxis);
    alpha = max(alpha, max(max(axisU, axisV), startAxis) * 0.8);

    alpha *= grazingFade * uGridOpacity.x;
    if (alpha <= 0.0)
        discard;

    gl_FragColor = vec4(color, alpha);
}
