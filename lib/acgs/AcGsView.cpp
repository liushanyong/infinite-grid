// AcGsView implementation: every definition here was previously scattered
// through main.cpp (submitAcGiDrawable, the text flush loop, the camera-
// space frustum-clip chain, the CPU curve sampler).  The behaviour is
// unchanged; the only edits replace main.cpp globals with the injected
// ViewFrameContext.

#include "acgs/AcGsView.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <tuple>
#include <iostream>
#include <limits>

#include "acgi/AcGiLineType.h"
#include "acgi/AcGiTextEngine.h"
#include "acgi/AcGiTextQueue.h"
#include "rendering/RendererBackend.h"

namespace acgs
{

rendering::SurfaceMaterial toSurfaceMaterial(
    const scene::AcGiMaterial &material)
{
    rendering::SurfaceMaterial result;
    result.algorithm =
        static_cast<rendering::SurfaceAlgorithm>(material.algorithm);
    result.baseColor = material.baseColor;
    result.accentColor = material.accentColor;
    result.metallic = material.metallic;
    result.roughness = material.roughness;
    result.transparency = material.transparency;
    result.lineWidth = material.lineWidth;
    return result;
}

namespace
{

// Projection matrices are float32, while slab bounds are accumulated in
// double.  Convert in the outward direction so rounding can never move a
// near/far plane inside a bounds that was calculated to contain it.
float floatExpandOutward(double value, bool downward)
{
    constexpr float kNegativeInfinity = -std::numeric_limits<float>::infinity();
    constexpr float kPositiveInfinity = std::numeric_limits<float>::infinity();
    const float direction = downward ? kNegativeInfinity : kPositiveInfinity;
    float result = static_cast<float>(value);

    if ((downward && static_cast<double>(result) > value) ||
        (!downward && static_cast<double>(result) < value))
    {
        result = std::nextafterf(result, direction);
    }

    for (int i = 0; i < 3 && std::isfinite(result); ++i)
        result = std::nextafterf(result, direction);
    return result;
}

bool lineDebugEnabled()
{
    static const bool enabled = [] {
        const char *value = std::getenv("GRID_LINE_DEBUG");
        return value && *value && std::strcmp(value, "0") != 0;
    }();
    return enabled;
}

bool colorIsCloseToBackground(const glm::vec4 &color)
{
    const glm::vec3 delta = glm::vec3(color) - glm::vec3(kClearColor);
    return glm::dot(delta, delta) <= 0.04f;
}

// Clip a world segment against the half-space set expressed as
// (value, slope) pairs: value + slope * t >= 0 with t in [0, 1].
template <size_t N>
bool clipWorldSegmentToHalfSpaces(
    const glm::dvec3 &startWorld, const glm::dvec3 &endWorld,
    const CameraSpacePoint &startCamera,
    const CameraSpacePoint &endCamera,
    const double (&constraints)[N][2],
    glm::dvec3 &clippedStart, glm::dvec3 &clippedEnd)
{
    const glm::dvec3 deltaWorld = endWorld - startWorld;
    const double dx = endCamera.x - startCamera.x;
    const double dy = endCamera.y - startCamera.y;
    const double dz = endCamera.depth - startCamera.depth;

    double tEnter = 0.0;
    double tExit = 1.0;
    for (const auto &constraint : constraints)
    {
        const double a = constraint[0];
        const double b = constraint[1];
        if (a < 0.0)
        {
            if (b <= 0.0)
                return false;
            tEnter = std::max(tEnter, -a / b);
        }
        else if (b < 0.0)
        {
            tExit = std::min(tExit, a / -b);
        }
    }

    if (tEnter > tExit)
        return false;

    clippedStart = startWorld + deltaWorld * tEnter;
    clippedEnd = startWorld + deltaWorld * tExit;
    return true;
}

bool clipReferenceSegmentToOrtho(const ViewFrameContext &frame,
                                 const glm::dvec3 &startWorld,
                                 const glm::dvec3 &endWorld,
                                 double nearPlane, double farPlane,
                                 double halfWidth, double halfHeight,
                                 glm::dvec3 &clippedStart,
                                 glm::dvec3 &clippedEnd)
{
    const CameraSpacePoint start = toCameraSpace(
        startWorld, frame.cameraPos, frame.cameraRight, frame.cameraUp,
        frame.cameraFront);
    const CameraSpacePoint end = toCameraSpace(
        endWorld, frame.cameraPos, frame.cameraRight, frame.cameraUp,
        frame.cameraFront);
    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double dz = end.depth - start.depth;

    const double constraints[6][2] = {
        {start.depth - nearPlane, dz},
        {farPlane - start.depth, -dz},
        {start.x + halfWidth, dx},
        {halfWidth - start.x, -dx},
        {start.y + halfHeight, dy},
        {halfHeight - start.y, -dy},
    };
    return clipWorldSegmentToHalfSpaces(startWorld, endWorld, start, end,
                                        constraints, clippedStart,
                                        clippedEnd);
}

bool clipReferenceSegmentToPerspective(const ViewFrameContext &frame,
                                       const glm::dvec3 &startWorld,
                                       const glm::dvec3 &endWorld,
                                       double nearPlane, double farPlane,
                                       double tanHalfVertical,
                                       double tanHalfHorizontal,
                                       glm::dvec3 &clippedStart,
                                       glm::dvec3 &clippedEnd)
{
    const CameraSpacePoint start = toCameraSpace(
        startWorld, frame.cameraPos, frame.cameraRight, frame.cameraUp,
        frame.cameraFront);
    const CameraSpacePoint end = toCameraSpace(
        endWorld, frame.cameraPos, frame.cameraRight, frame.cameraUp,
        frame.cameraFront);
    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double dz = end.depth - start.depth;

    const double constraints[6][2] = {
        {start.depth - nearPlane, dz},
        {farPlane - start.depth, -dz},
        {start.x + start.depth * tanHalfHorizontal,
         dx + dz * tanHalfHorizontal},
        {-start.x + start.depth * tanHalfHorizontal,
         -dx + dz * tanHalfHorizontal},
        {start.y + start.depth * tanHalfVertical,
         dy + dz * tanHalfVertical},
        {-start.y + start.depth * tanHalfVertical,
         -dy + dz * tanHalfVertical},
    };
    return clipWorldSegmentToHalfSpaces(startWorld, endWorld, start, end,
                                        constraints, clippedStart,
                                        clippedEnd);
}

// Clip a semi-infinite world ray (startWorld toward endWorld) directly to
// the camera frustum.  This avoids both an arbitrary tessellation endpoint
// and a huge proxy segment: the visible span is defined by the same
// near/far and screen planes the GPU uses.  All interval math stays in
// double.
bool clipSemiInfiniteRayToView(const ViewFrameContext &frame,
                               const glm::dvec3 &startWorld,
                               const glm::dvec3 &endWorld,
                               double nearPlane, double farPlane,
                               glm::dvec3 &clippedStart,
                               glm::dvec3 &clippedEnd,
                               bool flattenToSlabCenter = true)
{
    const CameraSpacePoint start = toCameraSpace(
        startWorld, frame.cameraPos, frame.cameraRight, frame.cameraUp,
        frame.cameraFront);
    const CameraSpacePoint end = toCameraSpace(
        endWorld, frame.cameraPos, frame.cameraRight, frame.cameraUp,
        frame.cameraFront);

    const glm::dvec3 directionWorld = endWorld - startWorld;
    const double directionLength = glm::length(directionWorld);
    if (directionLength <= 1.0e-18)
        return false;

    const glm::dvec3 direction = directionWorld / directionLength;
    const double dx = (end.x - start.x) / directionLength;
    const double dy = (end.y - start.y) / directionLength;
    const double dz = (end.depth - start.depth) / directionLength;

    double tEnter = 0.0;
    double tExit = std::numeric_limits<double>::infinity();
    bool rejected = false;
    auto accumulate = [&](double value, double slope) {
        // value + slope * t >= 0
        if (slope > 1.0e-15)
            tEnter = std::max(tEnter, -value / slope);
        else if (slope < -1.0e-15)
            tExit = std::min(tExit, -value / slope);
        else if (value < 0.0)
            rejected = true;
    };

    if (frame.ortho)
    {
        // In orthographic view an infinite CAD ray is a screen overlay:
        // lateral position is independent of camera depth.  Clip it to the
        // viewport sides only, then place the visible span at the stable
        // ortho slab center.  If it were clipped by the object slab too, a
        // grazing ray could shorten while OrthoSize grows because the slab
        // changes independently of the screen rectangle.
        const double halfHeight = frame.orthoSize;
        const double halfWidth =
            halfHeight * double(frame.viewportWidth) /
            std::max(1, frame.viewportHeight);
        accumulate(start.x + halfWidth, dx);
        accumulate(halfWidth - start.x, -dx);
        accumulate(start.y + halfHeight, dy);
        accumulate(halfHeight - start.y, -dy);
    }
    else
    {
        const double tanHalfVertical = std::tan(glm::radians(45.0) * 0.5);
        const double tanHalfHorizontal =
            tanHalfVertical * double(frame.viewportWidth) /
            std::max(1, frame.viewportHeight);
        accumulate(start.depth - nearPlane, dz);
        accumulate(farPlane - start.depth, -dz);
        accumulate(start.x + start.depth * tanHalfHorizontal,
                   dx + dz * tanHalfHorizontal);
        accumulate(-start.x + start.depth * tanHalfHorizontal,
                   -dx + dz * tanHalfHorizontal);
        accumulate(start.y + start.depth * tanHalfVertical,
                   dy + dz * tanHalfVertical);
        accumulate(-start.y + start.depth * tanHalfVertical,
                   -dy + dz * tanHalfVertical);
    }

    if (rejected || !std::isfinite(tExit) || tEnter > tExit)
        return false;

    clippedStart = startWorld + direction * tEnter;
    clippedEnd = startWorld + direction * tExit;

    if (flattenToSlabCenter && frame.ortho)
    {
        // Moving a point along Front does not move its ortho projection,
        // but it puts the emitted ribbon safely inside the hardware depth
        // range.
        const double slabCenter =
            std::max(0.001, frame.orbitDistance);
        const double depthMargin =
            std::max(1.0e-3, (farPlane - nearPlane) * 1.0e-3);
        const double minimumDepth = nearPlane + depthMargin;
        const double maximumDepth = farPlane - depthMargin;
        if (minimumDepth > maximumDepth)
            return false;
        const double renderDepth =
            glm::clamp(slabCenter, minimumDepth, maximumDepth);

        auto flattenDepth = [&](const glm::dvec3 &worldPoint) {
            const double depth =
                glm::dot(worldPoint - frame.cameraPos, frame.cameraFront);
            return worldPoint +
                   frame.cameraFront * (renderDepth - depth);
        };
        clippedStart = flattenDepth(clippedStart);
        clippedEnd = flattenDepth(clippedEnd);
    }

    return true;
}

} // namespace

// CPU sampling of a curve batch command using the same evaluator as
// picking/ID geometry: Bezier via de Casteljau, BSpline/NURBS via the
// standard de Boor recursion, clamped to the shader's CAD_CURVE_MAX_CP
// window so CPU and GPU evaluate identical geometry.
std::vector<glm::dvec3> AcGsView::sampleCurveBatch(
    const scene::CurveBatchCommand &curve)
{
    std::vector<glm::dvec3> result;
    const int samples =
        std::clamp(static_cast<int>(curve.sampleCount), 2, 512);
    if (curve.algorithm == rendering::CurveAlgorithm::Arc)
    {
        // CurveBatchCommand::axisU/axisV are unit directions; the visible
        // curve shader applies radius.  CPU sampling/ID geometry must apply
        // the same radius or an analytic Arc collapses to a point-sized ID
        // trace and disappears from the full-scene ID buffer.
        const double radius = std::max(0.0, curve.radius);
        result.reserve(samples);
        for (int i = 0; i < samples; ++i)
        {
            const double t = static_cast<double>(i) / (samples - 1);
            const double angle = curve.startAngle + curve.sweep * t;
            result.push_back(curve.center +
                             curve.axisU * (radius * std::cos(angle)) +
                             curve.axisV * (radius * std::sin(angle)));
        }
        return result;
    }

    // Match the GPU shader's CAD_CURVE_MAX_CP capacity so CPU picking,
    // ID geometry, and visible rendering evaluate the same curve.
    // BSpline/NURBS need one spare slot for the clamped knot vector;
    // Bezier can use all 16 control-point slots.
    const size_t controlCount =
        curve.algorithm == rendering::CurveAlgorithm::Bezier
            ? curve.controlPoints.size()
            : std::min(curve.controlPoints.size(), size_t(16 - 1));
    if (controlCount < 2)
        return result;

    if (curve.algorithm == rendering::CurveAlgorithm::Bezier)
    {
        std::vector<glm::dvec4> points;
        points.reserve(controlCount);
        for (size_t i = 0; i < controlCount; ++i)
        {
            const double weight =
                i < curve.weights.size() ? curve.weights[i] : 1.0;
            points.push_back(
                glm::dvec4(curve.controlPoints[i] * weight, weight));
        }

        result.reserve(samples);
        for (int sample = 0; sample < samples; ++sample)
        {
            const double t = static_cast<double>(sample) / (samples - 1);
            std::vector<glm::dvec4> value = points;
            while (value.size() > 1)
            {
                for (size_t i = 0; i + 1 < value.size(); ++i)
                    value[i] = glm::mix(value[i], value[i + 1], t);
                value.pop_back();
            }
            const glm::dvec4 &homogeneous = value.front();
            result.push_back(glm::dvec3(homogeneous) /
                             std::max(1.0e-12, homogeneous.w));
        }
        return result;
    }

    const int degree = std::clamp(curve.degree, 1, 3);
    if (controlCount < static_cast<size_t>(degree) + 1)
        return result;

    std::vector<double> knots = curve.knots;
    if (knots.size() < controlCount + degree + 1)
    {
        knots.clear();
        knots.reserve(controlCount + degree + 1);
        const size_t innerCount = controlCount - degree - 1;
        for (size_t i = 0; i <= degree; ++i)
            knots.push_back(0.0);
        for (size_t i = 1; i <= innerCount; ++i)
            knots.push_back(static_cast<double>(i) / (innerCount + 1));
        for (size_t i = 0; i <= degree; ++i)
            knots.push_back(1.0);
    }

    auto findSpan = [&](double t) {
        const size_t firstSpan = static_cast<size_t>(degree);
        if (controlCount < firstSpan + 1)
            return firstSpan;
        // Standard knot-span convention: span s covers
        // [knots[s], knots[s+1]) and uses cp[s-degree..s]; valid spans
        // are [degree, controlCount-1].
        // Match the shader: the final knot is at controlCount + degree,
        // not controlCount - 1.  The old test collapsed interior samples
        // into the last span for clamped knot vectors.
        const size_t lastKnotIndex = std::min(
            controlCount + static_cast<size_t>(degree), knots.size() - 1);
        // Keep span + degree inside the 16-entry knot window shared with
        // the GPU evaluation; mirrors the span clamp in cad_bspline/cad_nurbs.
        const size_t lastSpan =
            std::min(controlCount - 1, size_t(std::max(0, 15 - degree)));
        if (t >= knots[lastKnotIndex])
            return lastSpan;
        if (t <= knots[firstSpan])
            return firstSpan;
        for (size_t span = firstSpan; span < lastSpan; ++span)
        {
            if (t < knots[span + 1])
                return span;
        }
        return lastSpan;
    };

    result.reserve(samples);
    for (int sample = 0; sample < samples; ++sample)
    {
        const double t = static_cast<double>(sample) / (samples - 1);
        const size_t span = findSpan(t);
        std::vector<glm::dvec4> points(degree + 1);
        for (int i = 0; i <= degree; ++i)
        {
            const size_t index = span - degree + i;
            const double weight =
                index < curve.weights.size() ? curve.weights[index] : 1.0;
            points[i] =
                glm::dvec4(curve.controlPoints[index] * weight, weight);
        }
        for (int r = 1; r <= degree; ++r)
        {
            for (int j = degree; j >= r; --j)
            {
                const size_t index = span - degree + j;
                const double denominator =
                    knots[index + degree - r + 1] - knots[index];
                const double alpha = denominator > 1.0e-12
                                         ? (t - knots[index]) / denominator
                                         : 0.0;
                points[j] =
                    (1.0 - alpha) * points[j - 1] + alpha * points[j];
            }
        }
        const glm::dvec4 &homogeneous = points[degree];
        result.push_back(glm::dvec3(homogeneous) /
                         std::max(1.0e-12, homogeneous.w));
    }
    return result;
}

float envLayer(const char *name)
{
    const char *value = std::getenv(name);
    return value ? float(std::atof(value)) : 0.0f;
}

CameraSpacePoint toCameraSpace(const glm::dvec3 &worldPosition,
                               const glm::dvec3 &cameraPosition,
                               const glm::dvec3 &cameraRight,
                               const glm::dvec3 &cameraUp,
                               const glm::dvec3 &cameraFront)
{
    const glm::dvec3 delta = worldPosition - cameraPosition;
    return {glm::dot(delta, cameraRight), glm::dot(delta, cameraUp),
            glm::dot(delta, cameraFront)};
}

AcGsView &AcGsView::instance()
{
    static AcGsView view;
    return view;
}

void AcGsView::attach(rendering::RendererBackend *backend)
{
    backend_ = backend;
}

// Depth-slab hysteresis: expansion is applied immediately so nothing is
// clipped while moving, but shrinkage is delayed until the candidate slab
// has stayed stable for twenty frames.
struct AcGsView::DepthSlabStabilizer
{
    bool initialized = false;
    double stableNear = 0.0;
    double stableFar = 0.0;
    double pendingNear = 0.0;
    double pendingFar = 0.0;
    int stableFrames = 0;

    void reset()
    {
        initialized = false;
        stableFrames = 0;
    }

    void apply(double candidateNear, double candidateFar,
               double &outNear, double &outFar)
    {
        if (!initialized)
        {
            stableNear = pendingNear = candidateNear;
            stableFar = pendingFar = candidateFar;
            stableFrames = 0;
            initialized = true;
        }
        else if (candidateNear < stableNear || candidateFar > stableFar)
        {
            stableNear = std::min(stableNear, candidateNear);
            stableFar = std::max(stableFar, candidateFar);
            pendingNear = stableNear;
            pendingFar = stableFar;
            stableFrames = 0;
        }
        else
        {
            const double magnitude = std::max(
                {std::abs(stableNear), std::abs(stableFar),
                 std::abs(candidateNear), std::abs(candidateFar)});
            const double epsilon = std::max(1.0e-4, magnitude * 1.0e-4);
            if (std::abs(candidateNear - pendingNear) > epsilon ||
                std::abs(candidateFar - pendingFar) > epsilon)
            {
                pendingNear = candidateNear;
                pendingFar = candidateFar;
                stableFrames = 0;
            }
            else
            {
                ++stableFrames;
            }
            if (stableFrames >= 20)
            {
                stableNear = pendingNear;
                stableFar = pendingFar;
                stableFrames = 0;
            }
        }
        outNear = stableNear;
        outFar = stableFar;
    }
};

AcGsView::DepthSlabStabilizer &AcGsView::slabStabilizer(DepthSlab slab)
{
    static DepthSlabStabilizer stabilizers[3];
    return stabilizers[static_cast<int>(slab)];
}

void AcGsView::zoomExtents(const glm::dvec3 &minimum,
                             const glm::dvec3 &maximum)
{
    // The viewport rectangle of the current frame context.
    const double aspect =
        double(frame_.viewportWidth) / double(std::max(1, frame_.viewportHeight));
    orbitCamera_.fitToBounds(minimum, maximum, aspect);}

void AcGsView::focusOn(const glm::dvec3 &target, double distance)
{
    orbitCamera_.setOrbit(target, distance);
    orbitCamera_.fitDepthToBounds(target, target);}

void AcGsView::resetDepthSlabs()
{
    for (const DepthSlab slab :
         {DepthSlab::Ortho, DepthSlab::Perspective, DepthSlab::Overlay})
    {
        slabStabilizer(slab).reset();
    }
}

void AcGsView::stabilizeDepthSlab(DepthSlab slab, double candidateNear,
                                  double candidateFar, double &outNear,
                                  double &outFar)
{
    double stableNear = 0.0;
    double stableFar = 0.0;
    slabStabilizer(slab).apply(candidateNear, candidateFar, stableNear,
                               stableFar);
    outNear = floatExpandOutward(stableNear, true);
    outFar = floatExpandOutward(stableFar, false);
}

glm::vec4 AcGsView::contrastColor(const glm::vec4 &color) const
{
    if (!colorIsCloseToBackground(color))
        return color;
    return glm::vec4(1.0f - color.r, 1.0f - color.g, 1.0f - color.b,
                     color.a);
}

glm::vec3 AcGsView::ribbonSide(const glm::vec3 &direction,
                               const glm::vec3 &front,
                               float halfWidth) const
{
    glm::vec3 sideAxis = glm::cross(direction, front);
    if (glm::length(sideAxis) < 1.0e-5f)
        sideAxis = glm::cross(direction, glm::vec3(0.0f, 0.0f, 1.0f));
    if (glm::length(sideAxis) < 1.0e-5f)
        sideAxis = glm::cross(direction, glm::vec3(1.0f, 0.0f, 0.0f));
    return glm::normalize(sideAxis) * halfWidth;
}

float AcGsView::strokeHalfWidth(const entities::Stroke &stroke,
                                float fallback) const
{
    return stroke.lineWeight > 0.0
               ? static_cast<float>(stroke.lineWeight) * 0.5f
               : fallback;
}

bool AcGsView::clipStrokeSegment(const glm::dvec3 &startWorld,
                                 const glm::dvec3 &endWorld,
                                 glm::dvec3 &clippedStart,
                                 glm::dvec3 &clippedEnd,
                                 bool semiInfiniteRay) const
{
    // Keep a tiny safety band outside the hardware clip planes.  Geometry
    // that lies exactly on a plane (reference-line endpoints, the debug
    // frustum wireframe, the grid visible quad) must never be rejected by
    // floating-point jitter; the band stays pixel-exact because hardware
    // still clips at the true planes.
    double nearDepth = frame_.slabNear;
    double farDepth = frame_.slabFar;
    const double slabEpsilon =
        std::max(1.0e-6, (farDepth - nearDepth) * 1.0e-6);
    nearDepth -= slabEpsilon;
    farDepth += slabEpsilon;
    const double angularEpsilon = 1.0e-6;
    if (semiInfiniteRay)
    {
        // A Ray must not stop at its tessellation proxy length.  Clip the
        // analytic half-line directly against the same slab and viewport.
        return clipSemiInfiniteRayToView(
            frame_, startWorld, endWorld, nearDepth, farDepth,
            clippedStart, clippedEnd);
    }
    if (frame_.ortho)
    {
        const double halfHeight =
            frame_.orthoSize * (1.0 + angularEpsilon);
        return clipReferenceSegmentToOrtho(
            frame_, startWorld, endWorld, nearDepth, farDepth,
            halfHeight * (1.0 + angularEpsilon) *
                double(frame_.viewportWidth) /
                std::max(1, frame_.viewportHeight),
            halfHeight, clippedStart, clippedEnd);
    }
    const double tanHalfVertical =
        std::tan(glm::radians(45.0) * 0.5) * (1.0 + angularEpsilon);
    const double tanHalfHorizontal =
        tanHalfVertical * double(frame_.viewportWidth) /
        std::max(1, frame_.viewportHeight);
    return clipReferenceSegmentToPerspective(
        frame_, startWorld, endWorld, nearDepth, farDepth, tanHalfVertical,
        tanHalfHorizontal, clippedStart, clippedEnd);
}

bool AcGsView::clipSemiInfiniteRay(const glm::dvec3 &startWorld,
                                   const glm::dvec3 &endWorld,
                                   double nearDepth, double farDepth,
                                   glm::dvec3 &clippedStart,
                                   glm::dvec3 &clippedEnd,
                                   bool flattenToSlabCenter) const
{
    return clipSemiInfiniteRayToView(frame_, startWorld, endWorld, nearDepth,
                                     farDepth, clippedStart, clippedEnd,
                                     flattenToSlabCenter);
}

bool AcGsView::clipSegmentToOrtho(const glm::dvec3 &startWorld,
                                  const glm::dvec3 &endWorld,
                                  double nearPlane, double farPlane,
                                  double halfWidth, double halfHeight,
                                  glm::dvec3 &clippedStart,
                                  glm::dvec3 &clippedEnd) const
{
    return clipReferenceSegmentToOrtho(frame_, startWorld, endWorld,
                                       nearPlane, farPlane, halfWidth,
                                       halfHeight, clippedStart, clippedEnd);
}

bool AcGsView::clipSegmentToPerspective(
    const glm::dvec3 &startWorld, const glm::dvec3 &endWorld,
    double nearPlane, double farPlane, double tanHalfVertical,
    double tanHalfHorizontal, glm::dvec3 &clippedStart,
    glm::dvec3 &clippedEnd) const
{
    return clipReferenceSegmentToPerspective(frame_, startWorld, endWorld,
                                             nearPlane, farPlane,
                                             tanHalfVertical,
                                             tanHalfHorizontal, clippedStart,
                                             clippedEnd);
}

void AcGsView::submitMeshBatch(scene::MeshBatchCommand &command,
                               const rendering::DoubleSingleVec3 &eye,
                               const SubmitOptions &options)
{
    if (!backend_ || command.instances.empty())
        return;

    const glm::mat4 &view = frame_.view;
    const glm::mat4 &projection = frame_.projection;
    const glm::vec4 &logDepth = frame_.logDepth;
    const float pixelSizeWorld = options.pixelSizeWorld > 0.0f
                                     ? options.pixelSizeWorld
                                     : frame_.pixelSizeWorld;
    const float edgeSoftness = options.edgeSoftness > 0.0f
                                   ? options.edgeSoftness
                                   : frame_.edgeSoftness;

    if (command.cadAlgorithm)
    {
        const rendering::CadAlgorithmDemoRenderData renderData{
            .view = view,
            .projection = projection,
            .instances = command.instances.data(),
            .instanceCount = static_cast<uint32_t>(command.instances.size()),
            .mesh = command.prototype,
            .renderMode = visualStyle_.mode(),
            .cameraPos = eye.high + eye.low,
            .lightDir = glm::vec3(0.4f, 0.8f, 0.55f),
            .metallic = command.acgiMaterial.metallic,
            .roughness = command.acgiMaterial.roughness,
            .transparency = command.acgiMaterial.transparency,
            .strokeWidth = command.acgiMaterial.lineWidth,
            .strokeDensity = 1.0f,
            .layer = envLayer("GRID_MESH_LAYER"),
            .logDepth = logDepth,
            .eye = eye,
            .material = toSurfaceMaterial(command.acgiMaterial),
            .edgeHalfWidth = std::max(command.acgiMaterial.lineWidth * 0.5f,
                                      pixelSizeWorld),
            .edgeSoftness = edgeSoftness,
        };
        backend_->drawCadAlgorithmDemo(renderData);
        return;
    }

    const rendering::MeshInstancesRenderData renderData{
        .view = view,
        .projection = projection,
        .instances = command.instances.data(),
        .instanceCount = static_cast<uint32_t>(command.instances.size()),
        .mesh = command.prototype,
        .opaque = command.opaque,
        .layer = envLayer("GRID_MESH_LAYER"),
        .logDepth = logDepth,
        .eye = eye,
        .diffuseTextureIndex = frame_.meshTextureIndex,
        .headlight = frame_.meshHeadlight,
        .triplanarUv = frame_.meshTriplanar,
        .realistic = command.realistic,
        .material = command.material,
        .edgeHalfWidth = std::max(command.acgiMaterial.lineWidth * 0.5f,
                                  pixelSizeWorld),
        .edgeSoftness = edgeSoftness,
    };
    backend_->drawMeshInstances(renderData);
}

void AcGsView::submit(scene::SceneDrawList &drawList,
                      const SubmitOptions &options)
{
    if (!backend_)
        return;

    const ViewFrameContext &ctx = frame_;
    const float pixelSizeWorld = options.pixelSizeWorld > 0.0f
                                     ? options.pixelSizeWorld
                                     : ctx.pixelSizeWorld;
    const float edgeSoftness = options.edgeSoftness > 0.0f
                                   ? options.edgeSoftness
                                   : ctx.edgeSoftness;
    const float pointSize =
        options.pointSize > 0.0f ? options.pointSize : ctx.pointSize;
    const glm::mat4 &overlayProjection = options.overlayProjection
                                             ? *options.overlayProjection
                                             : ctx.overlayProjection;

    if (drawList.lights())
        backend_->setRealisticLights(drawList.lights()->data);

    if (drawList.grid())
        backend_->drawGrid(drawList.grid()->data);

    const rendering::DoubleSingleVec3 ownEye =
        rendering::encodeDoubleSingle(ctx.cameraPos);
    const rendering::DoubleSingleVec3 &eye =
        options.eye ? *options.eye : ownEye;
    for (scene::MeshBatchCommand &batch : drawList.meshBatches())
        submitMeshBatch(batch, eye, options);

    static std::vector<rendering::PrimVertex> polylineVertices;
    polylineVertices.clear();
    static std::vector<rendering::LineInstance> lineInstances;
    lineInstances.clear();
    auto appendAcGiRibbon = [&](const glm::vec3 &ra, const glm::vec3 &rb,
                                const glm::vec4 &color, float halfWidth,
                                float u0, float u1) {
        const glm::vec3 direction = rb - ra;
        if (glm::length(direction) < 1.0e-5f)
            return;
        const float minimumHalfWidth =
            pixelSizeWorld > 0.0f ? pixelSizeWorld * 1.0f : 1.0f;
        halfWidth = std::max(halfWidth, minimumHalfWidth);
        // The polyline fragment shader treats v=[0,1] as symmetric edges,
        // so the visible opaque core lies at the quad midpoint.  Emit a
        // centered ribbon; a one-sided quad would shift every rendered
        // line by half its width.
        const glm::vec4 strokeColor = contrastColor(color);
        const glm::vec3 side =
            ribbonSide(direction, ctx.cameraFront, halfWidth);
        polylineVertices.push_back({ra - side, strokeColor, {u0, 0.0f}});
        polylineVertices.push_back({ra + side, strokeColor, {u0, 1.0f}});
        polylineVertices.push_back({rb + side, strokeColor, {u1, 1.0f}});
        polylineVertices.push_back({ra - side, strokeColor, {u0, 0.0f}});
        polylineVertices.push_back({rb + side, strokeColor, {u1, 1.0f}});
        polylineVertices.push_back({rb - side, strokeColor, {u1, 0.0f}});
    };
    auto appendLineInstance = [&](const glm::vec3 &ra, const glm::vec3 &rb,
                                  const glm::vec4 &color, float halfWidth) {
        const glm::vec3 direction = rb - ra;
        if (glm::length(direction) < 1.0e-5f)
            return;
        const float minimumHalfWidth =
            pixelSizeWorld > 0.0f ? pixelSizeWorld * 1.0f : 1.0f;
        halfWidth = std::max(halfWidth, minimumHalfWidth);
        const glm::vec4 strokeColor = contrastColor(color);
        lineInstances.push_back({
            glm::vec4(ra, 0.0f),
            glm::vec4(rb, 1.0f),
            glm::vec4(glm::vec3(strokeColor), halfWidth),
            glm::vec4(strokeColor.a, 0.0f, 0.0f, 0.0f),
        });
    };

    // Curves were previously hardware PT_LINESTRIPs, which do not give
    // reliable line AA on D3D11.  Sample on CPU using the same evaluator
    // as picking/ID, frustum-clip each segment, and reuse the screen-space
    // ribbon pipeline.
    for (const scene::CurveBatchCommand &curve : drawList.curveBatches())
    {
        if (curve.controlPoints.empty() &&
            curve.algorithm != rendering::CurveAlgorithm::Arc)
        {
            continue;
        }
        const std::vector<glm::dvec3> points = sampleCurveBatch(curve);
        if (points.size() < 2)
            continue;
        const float halfWidth =
            std::max(curve.acgiMaterial.lineWidth * 0.5f,
                     pixelSizeWorld);
        for (size_t i = 0; i + 1 < points.size(); ++i)
        {
            glm::dvec3 clippedStart, clippedEnd;
            if (!clipStrokeSegment(points[i], points[i + 1], clippedStart,
                                   clippedEnd, false))
            {
                continue;
            }
            appendLineInstance(
                glm::vec3(clippedStart - ctx.cameraPos),
                glm::vec3(clippedEnd - ctx.cameraPos),
                curve.acgiMaterial.baseColor, halfWidth);
        }
    }
    auto appendLinePatternSegment = [&](const glm::vec3 &ra,
                                        const glm::vec3 &rb,
                                        const entities::Stroke &stroke) {
        const std::string &type = stroke.common.lineType;
        const AcGiLineType *lineType = acgiFindLineType(type.c_str());
        if (!lineType)
        {
            appendAcGiRibbon(ra, rb, stroke.common.color, 0.0f, 0.0f, 0.0f);
            return;
        }
        const std::vector<AcGiLineTypeMark> pattern =
            acgiLineTypeMarks(*lineType);

        const float halfWidth = strokeHalfWidth(stroke);
        const glm::dvec3 start(ra);
        const glm::dvec3 end(rb);
        const double total = glm::length(end - start);
        if (total < 1.0e-12)
            return;

        double patternLength = 0.0;
        for (const auto &mark : pattern)
            patternLength += mark.length;
        double distance = 0.0;
        size_t markIndex = 0;
        while (distance < total && !pattern.empty())
        {
            const auto &mark = pattern[markIndex % pattern.size()];
            const double markLength = mark.length;
            const double next = std::min(distance + markLength, total);
            if (mark.stroke && next > distance)
            {
                appendAcGiRibbon(
                    glm::vec3(start + (end - start) * (distance / total)),
                    glm::vec3(start + (end - start) * (next / total)),
                    stroke.common.color, halfWidth,
                    float(distance / total), float(next / total));
            }
            distance = next;
            ++markIndex;
        }
    };

    size_t debugClipRejected = 0;
    size_t debugClipAccepted = 0;
    for (const entities::Stroke &stroke : drawList.geometry().strokes)
    {
        if (!stroke.common.visible || stroke.points.size() < 2)
            continue;
        const float halfWidth = strokeHalfWidth(stroke);
        const size_t strokeCount = stroke.points.size();
        const bool patterned = stroke.common.lineType != "ByLayer" &&
                               stroke.common.lineType != "CONTINUOUS";
        // Closed strokes also emit the wrap segment back to their first
        // point.
        const size_t strokeSegmentCount =
            stroke.closed ? strokeCount : strokeCount - 1;
        for (size_t i = 0; i < strokeSegmentCount; ++i)
        {
            // Frustum-clip in double before emitting the ribbon: infinite
            // strokes (Ray/XLine) contribute only their visible span.
            glm::dvec3 clippedStart, clippedEnd;
            if (!clipStrokeSegment(
                    stroke.points[i], stroke.points[(i + 1) % strokeCount],
                    clippedStart, clippedEnd, stroke.semiInfinite))
            {
                ++debugClipRejected;
                continue;
            }
            ++debugClipAccepted;
            if (lineDebugEnabled() && stroke.semiInfinite)
            {
                std::printf(
                    "[BODY_RAY] cam=(%.6f,%.6f,%.6f) start=(%.6f,%.6f,%.6f) "
                    "end=(%.6f,%.6f,%.6f) half=%.6f\n",
                    ctx.cameraPos.x, ctx.cameraPos.y, ctx.cameraPos.z,
                    clippedStart.x, clippedStart.y, clippedStart.z,
                    clippedEnd.x, clippedEnd.y, clippedEnd.z,
                    std::max(halfWidth, pixelSizeWorld));
            }
            if (patterned)
            {
                appendLinePatternSegment(
                    glm::vec3(clippedStart - ctx.cameraPos),
                    glm::vec3(clippedEnd - ctx.cameraPos), stroke);
            }
            else
            {
                appendLineInstance(
                    glm::vec3(clippedStart - ctx.cameraPos),
                    glm::vec3(clippedEnd - ctx.cameraPos),
                    stroke.common.color, halfWidth);
            }
        }
    }

    if (!lineInstances.empty())
    {
        const rendering::LineInstancesRenderData lineData{
            .view = ctx.view,
            .projection = overlayProjection,
            .instances = lineInstances.data(),
            .instanceCount = static_cast<uint32_t>(lineInstances.size()),
            .logDepth = ctx.logDepth,
            .edgeSoftness = edgeSoftness,
            .layer = envLayer("GRID_LINE_LAYER"),
        };
        backend_->drawLineInstances(lineData);
    }

    if (!polylineVertices.empty())
    {
        if (lineDebugEnabled())
        {
            glm::vec2 ndcMin(std::numeric_limits<float>::max());
            glm::vec2 ndcMax(std::numeric_limits<float>::lowest());
            for (size_t i = 0; i < polylineVertices.size(); i += 6)
            {
                const glm::vec4 projected =
                    overlayProjection * ctx.view *
                    glm::vec4(polylineVertices[i].position, 1.0f);
                const glm::vec2 ndc = glm::vec2(projected) / projected.w;
                ndcMin = glm::min(ndcMin, ndc);
                ndcMax = glm::max(ndcMax, ndc);
            }
            std::cout << "[LINE_DEBUG] ribbon vertices="
                      << polylineVertices.size()
                      << " ndcMin=(" << ndcMin.x << ", " << ndcMin.y
                      << ") ndcMax=(" << ndcMax.x << ", " << ndcMax.y
                      << ")" << std::endl;
        }
        const rendering::PolylineRenderData polylineData{
            .view = ctx.view,
            .projection = overlayProjection,
            .vertices = polylineVertices.data(),
            .vertexCount = static_cast<uint32_t>(polylineVertices.size()),
            .logDepth = ctx.logDepth,
            .edgeSoftness = edgeSoftness,
            .layer = envLayer("GRID_LINE_LAYER"),
        };
        backend_->drawPolylines(polylineData);
    }

    const bool submitDebug = [] {
        const char *v = std::getenv("GRID_SUBMIT_DEBUG");
        return v && *v && std::strcmp(v, "0") != 0;
    }();
    static int submitDebugFrame = 0;
    const bool submitDebugFrameNow =
        submitDebug && (submitDebugFrame++ % 90 == 0);

    static std::vector<rendering::FillVertex> fillVertices;
    static std::vector<rendering::FillVertex> surfaceFillVertices;
    fillVertices.clear();
    surfaceFillVertices.clear();
    for (const entities::Triangle &triangle : drawList.geometry().fills)
    {
        if (!triangle.common.visible)
            continue;
        std::vector<rendering::FillVertex> &vertices =
            triangle.is3DFace ? surfaceFillVertices : fillVertices;
        const glm::vec4 fillColor =
            contrastColor(triangle.common.color);
        vertices.push_back(
            {glm::vec3(triangle.a - ctx.cameraPos), fillColor});
        vertices.push_back(
            {glm::vec3(triangle.b - ctx.cameraPos), fillColor});
        vertices.push_back(
            {glm::vec3(triangle.c - ctx.cameraPos), fillColor});
    }
    for (const bool is3DFace : {false, true})
    {
        std::vector<rendering::FillVertex> &vertices =
            is3DFace ? surfaceFillVertices : fillVertices;
        if (vertices.empty())
            continue;
        const rendering::FilledTrianglesRenderData fillData{
            .view = ctx.view,
            .projection = ctx.projection,
            .vertices = vertices.data(),
            .vertexCount = static_cast<uint32_t>(vertices.size()),
            .is3DFace = is3DFace,
            .layer = envLayer("GRID_FILL_LAYER"),
            .logDepth = ctx.logDepth,
            .material = toSurfaceMaterial(scene::AcGiMaterial{}),
        };
        backend_->drawFilledTriangles(fillData);
    }

    static std::vector<rendering::TargetPointInstance> points;
    points.clear();
    for (const entities::TessellatedPoint &point :
         drawList.geometry().points)
    {
        if (!point.common.visible)
            continue;
        points.push_back({glm::vec3(point.location - ctx.cameraPos),
                          glm::vec3(contrastColor(point.common.color)),
                          float(point.pointSize)});
    }
    if (submitDebugFrameNow)
    {
        std::printf(
            "[SUBMIT] strokes_in=%zu line_inst=%zu ribbon=%zu fills_in=%zu "
            "fill_out=%zu points_in=%zu pts_out=%zu slabs=[%.1f %.1f] "
            "cam=(%.0f,%.0f,%.0f) ortho=%d clip_ok=%zu clip_rej=%zu\n",
            drawList.geometry().strokes.size(), lineInstances.size(),
            polylineVertices.size(), drawList.geometry().fills.size(),
            fillVertices.size() + surfaceFillVertices.size(),
            drawList.geometry().points.size(), points.size(),
            ctx.slabNear, ctx.slabFar, ctx.cameraPos.x, ctx.cameraPos.y,
            ctx.cameraPos.z, int(ctx.ortho), debugClipAccepted,
            debugClipRejected);
        if (submitDebugFrameNow) std::fflush(stdout);
    }
    if (!points.empty())
    {
        const rendering::TargetPointInstancesRenderData pointData{
            .view = ctx.view,
            .projection = overlayProjection,
            .instances = points.data(),
            .instanceCount = static_cast<uint32_t>(points.size()),
            .pointSize = pointSize,
            .pixelSizeWorld = pixelSizeWorld,
            .isOrtho = ctx.ortho ? 1.0f : 0.0f,
            .logDepth = ctx.logDepth,
        };
        backend_->drawTargetPointInstances(pointData);
    }
}

void AcGsView::appendRibbon(std::vector<rendering::PrimVertex> &vertices,
                            const glm::dvec3 &startWorld,
                            const glm::dvec3 &endWorld, float halfWidth,
                            float u0, float u1, bool centered,
                            const glm::vec4 &color) const
{
    const glm::vec3 start(startWorld - frame_.cameraPos);
    const glm::vec3 end(endWorld - frame_.cameraPos);
    const glm::vec3 direction = end - start;
    if (glm::length(direction) < 1.0e-5f)
        return;
    const glm::vec3 side =
        ribbonSide(direction, frame_.cameraFront, halfWidth);
    if (centered)
    {
        vertices.push_back({start - side, color, {u0, 0.0f}});
        vertices.push_back({start + side, color, {u0, 1.0f}});
        vertices.push_back({end + side, color, {u1, 1.0f}});
        vertices.push_back({start - side, color, {u0, 0.0f}});
        vertices.push_back({end + side, color, {u1, 1.0f}});
        vertices.push_back({end - side, color, {u1, 0.0f}});
    }
    else
    {
        vertices.push_back({start, color, {u0, 0.0f}});
        vertices.push_back({start + side, color, {u0, 1.0f}});
        vertices.push_back({end + side, color, {u1, 1.0f}});
        vertices.push_back({start, color, {u0, 0.0f}});
        vertices.push_back({end + side, color, {u1, 1.0f}});
        vertices.push_back({end, color, {u1, 0.0f}});
    }
}

void AcGsView::drawRibbonVertices(
    std::vector<rendering::PrimVertex> &vertices, float edgeSoftness,
    float layer) const
{
    if (!backend_ || vertices.empty())
        return;
    const rendering::PolylineRenderData data{
        .view = frame_.view,
        .projection = frame_.overlayProjection,
        .vertices = vertices.data(),
        .vertexCount = static_cast<uint32_t>(vertices.size()),
        .logDepth = frame_.logDepth,
        .edgeSoftness = edgeSoftness,
        .layer = layer,
    };
    backend_->drawPolylines(data);
    vertices.clear();
}

void AcGsView::drawLineInstanceBatch(
    std::vector<rendering::LineInstance> &instances, float edgeSoftness,
    float layer) const
{
    if (!backend_ || instances.empty())
        return;
    const rendering::LineInstancesRenderData data{
        .view = frame_.view,
        .projection = frame_.overlayProjection,
        .instances = instances.data(),
        .instanceCount = static_cast<uint32_t>(instances.size()),
        .logDepth = frame_.logDepth,
        .edgeSoftness = edgeSoftness,
        .layer = layer,
    };
    backend_->drawLineInstances(data);
    instances.clear();
}

void AcGsView::drawFillTriangles(
    std::vector<rendering::FillVertex> &vertices, const glm::mat4 &view,
    const glm::mat4 &projection, bool is3DFace, float layer) const
{
    if (!backend_ || vertices.empty())
        return;
    const rendering::FilledTrianglesRenderData data{
        .view = view,
        .projection = projection,
        .vertices = vertices.data(),
        .vertexCount = static_cast<uint32_t>(vertices.size()),
        .is3DFace = is3DFace,
        .layer = layer,
        .logDepth = frame_.logDepth,
        .material = toSurfaceMaterial(scene::AcGiMaterial{}),
    };
    backend_->drawFilledTriangles(data);
}

void AcGsView::drawFillBoundary(const entities::TessellatedEntity &tess,
                                size_t begin, size_t count, float halfWidth,
                                const glm::vec4 &color) const
{
    if (!backend_ || count == 0)
        return;
    // Shared-edge analysis: a triangle edge used by exactly one triangle is
    // a boundary edge; edges shared by two stay hidden in wireframe modes.
    struct FillEdgeKey
    {
        double v[6];
        bool operator<(const FillEdgeKey &o) const
        {
            return std::memcmp(v, o.v, sizeof(v)) < 0;
        }
    };
    std::map<FillEdgeKey, int> useCounts;
    std::map<FillEdgeKey, std::pair<glm::dvec3, glm::dvec3>> edgeEnds;
    const size_t last = std::min(begin + count, tess.fills.size());
    for (size_t i = begin; i < last; ++i)
    {
        const entities::Triangle &triangle = tess.fills[i];
        if (!triangle.common.visible)
            continue;
        const glm::dvec3 corners[3] = {triangle.a, triangle.b, triangle.c};
        for (int e = 0; e < 3; ++e)
        {
            const glm::dvec3 &p = corners[e];
            const glm::dvec3 &q = corners[(e + 1) % 3];
            FillEdgeKey key;
            const glm::dvec3 *first = &p;
            const glm::dvec3 *second = &q;
            if (std::tie(q.x, q.y, q.z) < std::tie(p.x, p.y, p.z))
                std::swap(first, second);
            key.v[0] = first->x; key.v[1] = first->y; key.v[2] = first->z;
            key.v[3] = second->x; key.v[4] = second->y; key.v[5] = second->z;
            ++useCounts[key];
            edgeEnds[key] = {*first, *second};
        }
    }
    std::vector<rendering::PrimVertex> vertices;
    for (const auto &entry : useCounts)
    {
        if (entry.second != 1)
            continue;
        const auto &ends = edgeEnds[entry.first];
        appendRibbon(vertices, ends.first, ends.second, halfWidth, 0.0f,
                     1.0f, true, color);
    }
    if (!vertices.empty())
        drawRibbonVertices(vertices, 0.15f, envLayer("GRID_LINE_LAYER"));
}

int AcGsView::flushTextRequests()
{
    if (!backend_ || !acgi::textEngine().sdfReady())
        return 0;

    constexpr glm::mat4 identityView(1.0f);
    const ViewFrameContext &ctx = frame_;
    // Frustum culling: skip text whose oriented bounding rectangle lies
    // entirely outside the ortho viewport (perspective keeps drawing —
    // its frustum test hooks in at the same call when needed).
    const double orthoHalfHeight =
        ctx.ortho ? ctx.orthoSize : std::numeric_limits<double>::max();
    const double orthoHalfWidth =
        orthoHalfHeight * double(ctx.viewportWidth) /
        std::max(1, ctx.viewportHeight);
    int glyphsDrawn = 0;
    for (const acgi::TextRequest &request : acgi::textRequests())
    {
        if (ctx.ortho &&
            !acgi::textEngine().intersectsOrthoViewport(
                request, ctx.cameraPos, ctx.cameraRight, ctx.cameraUp,
                ctx.cameraFront, orthoHalfWidth, orthoHalfHeight))
        {
            continue;
        }
        glyphsDrawn += acgi::textEngine().drawText(
            *backend_, identityView, ctx.projection, ctx.cameraPos,
            ctx.cameraRight, ctx.cameraUp, ctx.cameraFront, request);
    }
    return glyphsDrawn;
}

} // namespace acgs
