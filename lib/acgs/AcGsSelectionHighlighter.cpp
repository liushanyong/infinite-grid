#include "acgs/AcGsSelectionHighlighter.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <map>
#include <vector>

#include "acgs/AcGsView.h"

namespace acgs
{

namespace
{

bool lineDebugEnabled()
{
    static const bool enabled = [] {
        const char *value = std::getenv("GRID_LINE_DEBUG");
        return value && *value && std::strcmp(value, "0") != 0;
    }();
    return enabled;
}

} // namespace

void AcGsSelectionHighlighter::drawFillOutline(
    const entities::TessellatedEntity &tess, size_t begin, size_t count,
    float pixelSizeWorld)
{
    if (begin >= tess.fills.size())
        return;
    const size_t last = std::min(begin + count, tess.fills.size());

    struct BoundaryKey
    {
        glm::dvec3 a;
        glm::dvec3 b;
        bool operator<(const BoundaryKey &other) const
        {
            if (a.x != other.a.x) return a.x < other.a.x;
            if (a.y != other.a.y) return a.y < other.a.y;
            if (a.z != other.a.z) return a.z < other.a.z;
            if (b.x != other.b.x) return b.x < other.b.x;
            if (b.y != other.b.y) return b.y < other.b.y;
            return b.z < other.b.z;
        }
    };
    struct BoundaryUse
    {
        glm::dvec3 start;
        glm::dvec3 end;
        glm::dvec3 normal;
        glm::dvec3 center;
        bool is3DFace;
    };

    std::map<BoundaryKey, std::vector<BoundaryUse>> boundaryUses;
    auto canonical = [](const glm::dvec3 &p, const glm::dvec3 &q) {
        return p.x < q.x || (p.x == q.x && (p.y < q.y ||
            (p.y == q.y && p.z <= q.z)))
            ? BoundaryKey{p, q} : BoundaryKey{q, p};
    };
    auto addTriangle = [&](const entities::Triangle &triangle) {
        if (!triangle.common.visible)
            return;
        const glm::dvec3 normal = glm::cross(triangle.b - triangle.a,
                                             triangle.c - triangle.a);
        const bool degenerate =
            glm::dot(normal, normal) <= 1.0e-24;
        const glm::dvec3 unit =
            degenerate ? glm::dvec3(0.0) : glm::normalize(normal);
        const glm::dvec3 center =
            (triangle.a + triangle.b + triangle.c) / 3.0;
        const std::pair<glm::dvec3, glm::dvec3> edges[3] = {
            {triangle.a, triangle.b},
            {triangle.b, triangle.c},
            {triangle.c, triangle.a},
        };
        for (const auto &edge : edges)
            boundaryUses[canonical(edge.first, edge.second)].push_back(
                {edge.first, edge.second, unit, center, triangle.is3DFace});
    };
    for (size_t i = begin; i < last; ++i)
        addTriangle(tess.fills[i]);

    static std::vector<rendering::PrimVertex> outlineVertices;
    outlineVertices.clear();
    // The fill covers the ribbon's inner half, so only the outward half is
    // visible. Use two copies of the shared half-width to keep the visible
    // border at 2 * kOutlineWidthPixels, matching the previous in-plane
    // offset thickness.
    const float outlineWidth = 2.0f * outlineWidthWorld(pixelSizeWorld);
    // A closed 3D solid has no used-once boundary edges; its selection
    // outline is the view-dependent silhouette instead: edges whose two
    // adjacent triangles face opposite sides of the camera.  These ribbons
    // are drawn twice as wide so the solid reads as boldly selected as a
    // planar fill.
    const float silhouetteWidth = 2.0f * outlineWidth;
    const ViewFrameContext &frame = view_.frame();
    const bool ortho = frame.ortho;
    auto facesCamera = [&](const BoundaryUse &use) {
        if (ortho)
            return glm::dot(use.normal, glm::dvec3(frame.cameraFront)) < 0.0;
        return glm::dot(use.normal, use.center - frame.cameraPos) > 0.0;
    };
    for (const auto &[key, uses] : boundaryUses)
    {
        (void)key;
        if (uses.size() == 1)
        {
            view_.appendRibbon(outlineVertices, uses.front().start,
                               uses.front().end, outlineWidth, 0.0f, 1.0f,
                               true, kOutlineColor);
        }
        else if (uses.size() == 2 && uses.front().is3DFace &&
                 uses.back().is3DFace)
        {
            const BoundaryUse &first = uses.front();
            const BoundaryUse &second = uses.back();
            if (facesCamera(first) == facesCamera(second))
                continue; // both sides face the same way: interior edge
            view_.appendRibbon(outlineVertices, first.start, first.end,
                               silhouetteWidth, 0.0f, 1.0f, true,
                               kOutlineColor);
        }
    }

    view_.drawRibbonVertices(outlineVertices, 0.15f, 1.0f);
}

void AcGsSelectionHighlighter::drawPointHighlight(
    const entities::TessellatedEntity &tess, size_t begin, size_t count,
    float pixelSizeWorld)
{
    if (begin >= tess.points.size())
        return;
    const size_t last = std::min(begin + count, tess.points.size());

    const ViewFrameContext &frame = view_.frame();
    auto worldPerPixel = [&](const glm::dvec3 &worldPoint) {
        if (frame.ortho)
            return 2.0 * frame.orthoSize / double(frame.viewportHeight);
        const double viewDepth = std::max(1.0e-9,
            glm::dot(worldPoint - frame.cameraPos,
                     glm::dvec3(frame.cameraFront)));
        return 2.0 * viewDepth * std::tan(glm::radians(45.0) * 0.5) /
               double(frame.viewportHeight);
    };

    static std::vector<rendering::PrimVertex> outlineVertices;
    outlineVertices.clear();
    const glm::vec3 right(frame.cameraRight);
    const glm::vec3 up(frame.cameraUp);
    constexpr int kCircleSegments = 32;
    for (size_t i = begin; i < last; ++i)
    {
        const entities::TessellatedPoint &point = tess.points[i];
        if (!point.common.visible)
            continue;

        const double pixelsPerWorldUnit = 1.0 /
            std::max(worldPerPixel(point.location), 1.0e-12);
        // The point impostor shader uses 2 * pointSize as the visible
        // screen-space radius, so the outline inner edge must match it.
        const double innerPixels = double(point.pointSize);
        const double outerPixels = innerPixels + kOutlineWidthPixels;
        const double innerRadius = innerPixels / pixelsPerWorldUnit;
        const double outerRadius = outerPixels / pixelsPerWorldUnit;
        const glm::vec3 center(point.location - frame.cameraPos);
        for (int segment = 0; segment < kCircleSegments; ++segment)
        {
            const double angle0 = glm::two_pi<double>() * double(segment) /
                                  double(kCircleSegments);
            const double angle1 = glm::two_pi<double>() *
                                  double(segment + 1) /
                                  double(kCircleSegments);
            const glm::vec3 inner0 = center +
                right * float(innerRadius * std::cos(angle0)) +
                up * float(innerRadius * std::sin(angle0));
            const glm::vec3 outer0 = center +
                right * float(outerRadius * std::cos(angle0)) +
                up * float(outerRadius * std::sin(angle0));
            const glm::vec3 inner1 = center +
                right * float(innerRadius * std::cos(angle1)) +
                up * float(innerRadius * std::sin(angle1));
            const glm::vec3 outer1 = center +
                right * float(outerRadius * std::cos(angle1)) +
                up * float(outerRadius * std::sin(angle1));

            outlineVertices.push_back({inner0, kOutlineColor, {0.5f, 0.5f}});
            outlineVertices.push_back({outer0, kOutlineColor, {0.5f, 0.5f}});
            outlineVertices.push_back({outer1, kOutlineColor, {0.5f, 0.5f}});
            outlineVertices.push_back({inner0, kOutlineColor, {0.5f, 0.5f}});
            outlineVertices.push_back({outer1, kOutlineColor, {0.5f, 0.5f}});
            outlineVertices.push_back({inner1, kOutlineColor, {0.5f, 0.5f}});
        }
    }

    view_.drawRibbonVertices(outlineVertices, 0.15f, 1.0f);
}

void AcGsSelectionHighlighter::drawStrokeOutline(
    const entities::TessellatedEntity &tess, size_t begin, size_t count,
    float pixelSizeWorld)
{
    static std::vector<rendering::LineInstance> outlineLineInstances;
    outlineLineInstances.clear();

    for (size_t i = begin;
         i < begin + count && i < tess.strokes.size(); ++i)
    {
        const entities::Stroke &stroke = tess.strokes[i];
        const size_t pointCount = stroke.points.size();
        if (!stroke.common.visible || pointCount < 2)
            continue;

        // Match the visible ribbon's pixel-size floor, then add one shared
        // outline half-width. The full width grows by two copies of
        // kOutlineWidthPixels.
        const float sourceHalfWidth =
            std::max(view_.strokeHalfWidth(stroke), pixelSizeWorld);
        const float halfWidth =
            sourceHalfWidth + outlineWidthWorld(pixelSizeWorld);
        const size_t segmentCount =
            stroke.closed ? pointCount : pointCount - 1;
        for (size_t j = 0; j < segmentCount; ++j)
        {
            const size_t next = (j + 1) % pointCount;
            // The outline must use the same visible span as the source
            // stroke.  In particular a semi-infinite Ray must not draw an
            // outline around its finite tessellation proxy only.
            glm::dvec3 clippedStart, clippedEnd;
            if (!view_.clipStrokeSegment(stroke.points[j],
                                         stroke.points[next], clippedStart,
                                         clippedEnd, stroke.semiInfinite))
            {
                continue;
            }
            if (lineDebugEnabled() && stroke.semiInfinite)
            {
                const ViewFrameContext &frame = view_.frame();
                std::printf(
                    "[OUTLINE_RAY] cam=(%.6f,%.6f,%.6f) start=(%.6f,%.6f,"
                    "%.6f) end=(%.6f,%.6f,%.6f) half=%.6f\n",
                    frame.cameraPos.x, frame.cameraPos.y, frame.cameraPos.z,
                    clippedStart.x, clippedStart.y, clippedStart.z,
                    clippedEnd.x, clippedEnd.y, clippedEnd.z, halfWidth);
            }
            // Visible CAD strokes use the screen-space line-instance
            // pipeline.  A world-space CPU ribbon can look offset from the
            // body in perspective, especially for long semi-infinite rays.
            // Keep outline and body on the same centerline/expansion path.
            outlineLineInstances.push_back({
                glm::vec4(glm::vec3(clippedStart - view_.frame().cameraPos),
                          0.0f),
                glm::vec4(glm::vec3(clippedEnd - view_.frame().cameraPos),
                          1.0f),
                glm::vec4(glm::vec3(kOutlineColor), halfWidth),
                glm::vec4(kOutlineColor.a, 0.0f, 0.0f, 0.0f),
            });
        }
    }

    view_.drawLineInstanceBatch(outlineLineInstances, 2.0f, 0.0f);
}

void AcGsSelectionHighlighter::drawCurveOutline(
    const acgs::CurveBatchCommand &curve, float pixelSizeWorld)
{
    const std::vector<glm::dvec3> points =
        AcGsView::sampleCurveBatch(curve);
    const size_t segmentCount = points.size() > 1 ? points.size() - 1 : 0;
    const float halfWidth =
        std::max(curve.acgiMaterial.lineWidth * 0.5f, 1.0f) +
        outlineWidthWorld(pixelSizeWorld);

    static std::vector<rendering::PrimVertex> outlineVertices;
    outlineVertices.clear();
    for (size_t i = 0; i < segmentCount; ++i)
    {
        view_.appendRibbon(outlineVertices, points[i], points[i + 1],
                           halfWidth,
                           float(i) / float(std::max<size_t>(segmentCount, 1)),
                           float(i + 1) /
                               float(std::max<size_t>(segmentCount, 1)),
                           true, kOutlineColor);
    }
    view_.drawRibbonVertices(outlineVertices, 0.15f, 1.0f);
}

} // namespace acgs
