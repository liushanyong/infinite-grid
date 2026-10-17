#include "acgi/AcGiWorldDraw.h"

#include <cassert>
#include <cmath>
#include <utility>

int main()
{
    acdb::TessellatedEntity geometry;
    acgs::ViewportDraw context(geometry);
    AcGiWorldDraw draw(context);

    AcGiClipBoundary clip;
    clip.m_aptPoints = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
    assert(draw.pushClipBoundary(&clip));
    assert(!draw.worldLine({-1.0, 0.5, 0.0}, {2.0, 0.5, 0.0}));
    assert(geometry.strokes.size() == 1);
    assert(std::abs(geometry.strokes[0].points[0].x - 0.0) < 1.0e-9);
    assert(std::abs(geometry.strokes[0].points[1].x - 1.0) < 1.0e-9);

    assert(!draw.worldTriangle({-1.0, 0.0, 0.0}, {2.0, 0.0, 0.0},
                               {0.5, 2.0, 0.0}));
    assert(!geometry.fills.empty());
    for (const acdb::Triangle &triangle : geometry.fills)
        for (const AcGePoint3d point : {triangle.a, triangle.b, triangle.c})
            assert(point.x >= -1.0e-9 && point.x <= 1.0 + 1.0e-9 &&
                   point.y >= -1.0e-9 && point.y <= 1.0 + 1.0e-9);

    assert(!draw.worldPoint({2.0, 2.0, 0.0}));
    assert(geometry.points.empty());
    assert(!draw.worldPoint({0.5, 0.5, 0.0}));
    assert(geometry.points.size() == 1);
    draw.popClipBoundary();
    assert(!draw.worldLine({-1.0, 0.5, 0.0}, {2.0, 0.5, 0.0}));
    assert(geometry.strokes.size() == 2);
    assert(std::abs(geometry.strokes.back().points.front().x + 1.0) < 1.0e-9);

    AcGiClipBoundary rectangularClip;
    rectangularClip.m_aptPoints = {{-1.0, -1.0}, {1.0, 1.0}};
    assert(draw.pushClipBoundary(&rectangularClip));
    assert(!draw.worldLine({-2.0, 0.0, 0.0}, {2.0, 0.0, 0.0}));
    assert(geometry.strokes.size() == 3);
    assert(std::abs(geometry.strokes.back().points.front().x + 1.0) < 1.0e-9);
    assert(std::abs(geometry.strokes.back().points.back().x - 1.0) < 1.0e-9);
    draw.popClipBoundary();

    acdb::TessellatedEntity concaveClipGeometry;
    acgs::ViewportDraw concaveClipContext(concaveClipGeometry);
    AcGiWorldDraw concaveClipDraw(concaveClipContext);
    AcGiClipBoundary concaveClip;
    concaveClip.m_aptPoints = {
        {0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {3.0, 4.0},
        {3.0, 1.0}, {1.0, 1.0}, {1.0, 4.0}, {0.0, 4.0}};
    assert(concaveClipDraw.pushClipBoundary(&concaveClip));
    assert(!concaveClipDraw.worldLine({-1.0, 2.0, 0.0},
                                      {5.0, 2.0, 0.0}));
    assert(concaveClipGeometry.strokes.size() == 2);
    assert(std::abs(concaveClipGeometry.strokes[0].points.front().x) < 1.0e-9);
    assert(std::abs(concaveClipGeometry.strokes[0].points.back().x - 1.0) <
           1.0e-9);
    assert(std::abs(concaveClipGeometry.strokes[1].points.front().x - 3.0) <
           1.0e-9);
    assert(std::abs(concaveClipGeometry.strokes[1].points.back().x - 4.0) <
           1.0e-9);
    assert(!concaveClipDraw.worldPoint({2.0, 2.0, 0.0}));
    assert(!concaveClipDraw.worldPoint({0.5, 2.0, 0.0}));
    assert(concaveClipGeometry.points.size() == 1);
    assert(!concaveClipDraw.worldTriangle({-5.0, -2.0, 0.0},
                                          {9.0, -2.0, 0.0},
                                          {2.0, 8.0, 0.0}));
    double concaveFillArea = 0.0;
    for (const acdb::Triangle &triangle : concaveClipGeometry.fills)
    {
        const double area =
            std::abs((triangle.b.x - triangle.a.x) *
                         (triangle.c.y - triangle.a.y) -
                     (triangle.b.y - triangle.a.y) *
                         (triangle.c.x - triangle.a.x)) *
            0.5;
        concaveFillArea += area;
        const double centroidX =
            (triangle.a.x + triangle.b.x + triangle.c.x) / 3.0;
        const double centroidY =
            (triangle.a.y + triangle.b.y + triangle.c.y) / 3.0;
        assert(!(centroidX > 1.0 && centroidX < 3.0 && centroidY > 1.0));
    }
    assert(std::abs(concaveFillArea - 10.0) < 1.0e-8);
    concaveClipDraw.popClipBoundary();

    acdb::TessellatedEntity fillTypeGeometry;
    acgs::ViewportDraw fillTypeContext(fillTypeGeometry);
    AcGiWorldDraw fillTypeDraw(fillTypeContext);
    const AcGePoint3d fillTypePolygon[] = {
        {0.0, 0.0, 0.0}, {2.0, 0.0, 0.0}, {1.0, 1.0, 0.0}};
    assert(fillTypeDraw.fillType() == kAcGiFillAlways);
    assert(!fillTypeDraw.polygon(3, fillTypePolygon));
    assert(fillTypeGeometry.fills.size() == 1);
    assert(fillTypeGeometry.strokes.size() == 1);
    fillTypeDraw.setFillType(kAcGiFillNever);
    assert(fillTypeDraw.fillType() == kAcGiFillNever);
    assert(!fillTypeDraw.polygon(3, fillTypePolygon));
    assert(fillTypeGeometry.fills.size() == 1);
    assert(fillTypeGeometry.strokes.size() == 2);
    assert(!fillTypeDraw.circle({0.0, 0.0, 0.0}, 1.0, {0.0, 0.0, 1.0}));
    assert(fillTypeGeometry.fills.size() == 1);
    fillTypeDraw.setFillType(kAcGiFillAlways);
    assert(!fillTypeDraw.circle({0.0, 0.0, 0.0}, 1.0, {0.0, 0.0, 1.0}));
    assert(fillTypeGeometry.fills.size() > 1);
    fillTypeDraw.setFillType(kAcGiFillNever);
    const size_t fillsBeforeUnfilledArc = fillTypeGeometry.fills.size();
    const size_t strokesBeforeUnfilledArc = fillTypeGeometry.strokes.size();
    assert(!fillTypeDraw.circularArc({0.0, 0.0, 0.0}, 1.0,
                                     {0.0, 0.0, 1.0}, {1.0, 0.0, 0.0},
                                     1.0, kAcGiArcSector));
    assert(fillTypeGeometry.fills.size() == fillsBeforeUnfilledArc);
    assert(fillTypeGeometry.strokes.size() > strokesBeforeUnfilledArc + 1);

    acdb::TessellatedEntity boundaryDisplayGeometry;
    acgs::ViewportDraw boundaryDisplayContext(boundaryDisplayGeometry);
    AcGiWorldDraw boundaryDisplayDraw(boundaryDisplayContext);
    AcGiClipBoundary displayBoundary;
    displayBoundary.m_aptPoints =
        {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
    displayBoundary.m_bDrawBoundary = true;
    assert(boundaryDisplayDraw.pushClipBoundary(&displayBoundary));
    assert(boundaryDisplayGeometry.strokes.size() == 1);
    assert(boundaryDisplayGeometry.strokes.front().closed);
    assert(boundaryDisplayGeometry.strokes.front().points.size() == 4);
    assert(std::abs(boundaryDisplayGeometry.strokes.front().points[2].x -
                    1.0) < 1.0e-9);
    boundaryDisplayDraw.popClipBoundary();

    acdb::TessellatedEntity transformedClipGeometry;
    acgs::ViewportDraw transformedClipContext(transformedClipGeometry);
    AcGiWorldDraw transformedClipDraw(transformedClipContext);
    AcGiClipBoundary transformedClip;
    transformedClip.m_aptPoints = {{0.0, -1.0}, {1.0, 1.0}};
    transformedClip.m_xToClipSpace =
        AcGeMatrix3d::setToTranslation({-6.0, 0.0, 0.0});
    transformedClip.m_xInverseBlockRefXForm =
        AcGeMatrix3d::setToTranslation({-4.0, 0.0, 0.0});
    assert(transformedClipDraw.pushClipBoundary(&transformedClip));
    assert(!transformedClipDraw.worldLine({9.0, 0.0, 0.0},
                                          {11.0, 0.0, 0.0}));
    assert(transformedClipGeometry.strokes.size() == 1);
    assert(std::abs(transformedClipGeometry.strokes[0].points.front().x -
                    10.0) < 1.0e-9);
    assert(std::abs(transformedClipGeometry.strokes[0].points.back().x -
                    11.0) < 1.0e-9);
    transformedClipDraw.popClipBoundary();

    acdb::TessellatedEntity depthClipGeometry;
    acgs::ViewportDraw depthClipContext(depthClipGeometry);
    AcGiWorldDraw depthClipDraw(depthClipContext);
    AcGiClipBoundary depthClip;
    depthClip.m_aptPoints = {{-2.0, -2.0}, {2.0, 2.0}};
    depthClip.m_bClippingFront = true;
    depthClip.m_bClippingBack = true;
    depthClip.m_dFrontClipZ = 1.0;
    depthClip.m_dBackClipZ = -1.0;
    assert(depthClipDraw.pushClipBoundary(&depthClip));
    assert(!depthClipDraw.worldLine({0.0, 0.0, -2.0},
                                    {0.0, 0.0, 2.0}));
    assert(depthClipGeometry.strokes.size() == 1);
    assert(std::abs(depthClipGeometry.strokes[0].points.front().z + 1.0) <
           1.0e-9);
    assert(std::abs(depthClipGeometry.strokes[0].points.back().z - 1.0) <
           1.0e-9);
    assert(!depthClipDraw.worldTriangle({-1.0, -1.0, -2.0},
                                        {1.0, -1.0, 2.0},
                                        {0.0, 1.0, 0.0}));
    assert(!depthClipGeometry.fills.empty());
    for (const acdb::Triangle &triangle : depthClipGeometry.fills)
        for (const AcGePoint3d point : {triangle.a, triangle.b, triangle.c})
            assert(point.z >= -1.0 - 1.0e-9 &&
                   point.z <= 1.0 + 1.0e-9);
    depthClipDraw.popClipBoundary();

    const Adesk::UInt32 instances[] = {2};
    const Adesk::UInt32 vertexCounts[] = {4};
    const AcGePoint3d positions[] = {{0.0, 0.0, 0.0}, {2.0, 0.0, 0.0}};
    const AcGePoint3d polygon[] = {
        {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
        {1.0, 1.0, 0.0}, {0.0, 1.0, 0.0}};
    const AcCmEntityColor fillColors[] = {
        AcCmEntityColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f))};
    const AcCmTransparency fillOpacity[] = {
        AcCmTransparency(static_cast<std::uint8_t>(255))};
    const size_t fillsBeforePolyPolygon = geometry.fills.size();
    const size_t strokesBeforePolyPolygon = geometry.strokes.size();
    assert(!draw.polyPolygon(1, instances, positions, vertexCounts, polygon,
                             nullptr, nullptr, fillColors, fillOpacity));
    assert(geometry.fills.size() == fillsBeforePolyPolygon + 4);
    assert(geometry.strokes.size() == strokesBeforePolyPolygon + 2);
    for (size_t index = fillsBeforePolyPolygon; index < geometry.fills.size();
         ++index)
    {
        const acdb::Triangle &triangle = geometry.fills[index];
        assert(triangle.common.color.renderColor().r > 0.99f);
    }

    acdb::TessellatedEntity meshGeometry;
    acgs::ViewportDraw meshContext(meshGeometry);
    AcGiWorldDraw meshDraw(meshContext);
    const AcGePoint3d meshVertices[] = {
        {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0}, {1.0, 1.0, 0.0}};
    const AcCmEntityColor vertexColors[] = {
        AcCmEntityColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)),
        AcCmEntityColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)),
        AcCmEntityColor(glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)),
        AcCmEntityColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f))};
    const AcGeVector3d vertexNormals[] = {
        {0.0, 0.0, 1.0}, {0.0, 0.0, 1.0},
        {0.0, 0.0, 1.0}, {0.0, 0.0, 1.0}};
    const Adesk::UInt8 edgeVisibility[] = {1, 0, 1, 0};
    const AcGiLineType dashedLineType{"DASHED", nullptr, 0};
    const AcGiLineType edgeLineTypes[] = {
        dashedLineType, dashedLineType, dashedLineType, dashedLineType};
    AcGiFaceData faces;
    faces.setTrueColors(fillColors, 1);
    AcGiVertexData vertices;
    vertices.setTrueColors(vertexColors, 4);
    vertices.setNormals(vertexNormals, 4);
    AcGiEdgeData edges;
    edges.setVisibility(edgeVisibility, 4);
    edges.setLineTypes(edgeLineTypes, 4);
    assert(!meshDraw.mesh(2, 2, meshVertices, &edges, &faces, &vertices));
    assert(meshGeometry.fills.size() == 2);
    assert(meshGeometry.strokes.size() == 2);
    assert(meshGeometry.strokes.front().common.lineType == "DASHED");
    assert(meshGeometry.fills[0].hasVertexColors);
    assert(meshGeometry.fills[0].hasVertexNormals);
    acdb::TessellatedEntity wireMeshGeometry;
    acgs::ViewportDraw wireMeshContext(wireMeshGeometry);
    AcGiWorldDraw wireMeshDraw(wireMeshContext);
    wireMeshDraw.setFillType(kAcGiFillNever);
    assert(!wireMeshDraw.mesh(2, 2, meshVertices, &edges));
    assert(wireMeshGeometry.fills.empty());
    assert(wireMeshGeometry.strokes.size() == 2);
    assert(wireMeshDraw.fillType() == kAcGiFillNever);

    acdb::TessellatedEntity shellGeometry;
    acgs::ViewportDraw shellContext(shellGeometry);
    AcGiWorldDraw shellDraw(shellContext);
    const Adesk::Int32 shellFaces[] = {4, 0, 1, 3, 2, 0};
    assert(!shellDraw.shell(4, meshVertices, 6, shellFaces, &edges, &faces,
                            &vertices));
    assert(shellGeometry.fills.size() == 2);
    assert(shellGeometry.strokes.size() == 2);
    assert(shellGeometry.strokes.front().common.lineType == "DASHED");
    assert(shellGeometry.fills[0].hasVertexColors);
    acdb::TessellatedEntity wireShellGeometry;
    acgs::ViewportDraw wireShellContext(wireShellGeometry);
    AcGiWorldDraw wireShellDraw(wireShellContext);
    wireShellDraw.setFillType(kAcGiFillNever);
    assert(!wireShellDraw.shell(4, meshVertices, 6, shellFaces, &edges));
    assert(wireShellGeometry.fills.empty());
    assert(wireShellGeometry.strokes.size() == 2);
    assert(wireShellDraw.fillType() == kAcGiFillNever);

    acdb::TessellatedEntity shellHoleGeometry;
    acgs::ViewportDraw shellHoleContext(shellHoleGeometry);
    AcGiWorldDraw shellHoleDraw(shellHoleContext);
    const AcGePoint3d shellHoleVertices[] = {
        {0.0, 0.0, 0.0}, {4.0, 0.0, 0.0},
        {4.0, 4.0, 0.0}, {0.0, 4.0, 0.0},
        {1.0, 1.0, 0.0}, {1.0, 3.0, 0.0},
        {3.0, 3.0, 0.0}, {3.0, 1.0, 0.0}};
    const Adesk::Int32 shellHoleFaceList[] = {
        4, 0, 1, 2, 3, -4, 4, 5, 6, 7};
    const Adesk::UInt8 shellHoleEdgeVisibility[] = {1, 1, 1, 1, 1, 1, 1, 1};
    AcGiEdgeData shellHoleEdges;
    shellHoleEdges.setVisibility(shellHoleEdgeVisibility, 8);
    assert(!shellHoleDraw.shell(8, shellHoleVertices, 10, shellHoleFaceList,
                                &shellHoleEdges, &faces));
    assert(shellHoleGeometry.strokes.size() == 8);
    assert(shellHoleGeometry.fills.front().hasVertexNormals);
    double shellHoleArea = 0.0;
    for (const acdb::Triangle &triangle : shellHoleGeometry.fills)
        shellHoleArea +=
            0.5 * glm::length(glm::cross(triangle.b - triangle.a,
                                         triangle.c - triangle.a));
    assert(std::abs(shellHoleArea - 12.0) < 1.0e-8);
    const auto triangleContains = [](const glm::dvec2 &point,
                                     const acdb::Triangle &triangle) {
        const glm::dvec2 a(triangle.a.x, triangle.a.y);
        const glm::dvec2 b(triangle.b.x, triangle.b.y);
        const glm::dvec2 c(triangle.c.x, triangle.c.y);
        const auto cross = [](const glm::dvec2 &u, const glm::dvec2 &v) {
            return u.x * v.y - u.y * v.x;
        };
        const double orientation = cross(b - a, c - a);
        const double ab = cross(b - a, point - a);
        const double bc = cross(c - b, point - b);
        const double ca = cross(a - c, point - c);
        return orientation >= 0.0
                   ? ab >= -1.0e-10 && bc >= -1.0e-10 && ca >= -1.0e-10
                   : ab <= 1.0e-10 && bc <= 1.0e-10 && ca <= 1.0e-10;
    };
    for (double y = 1.2; y < 2.9; y += 0.3)
        for (double x = 1.2; x < 2.9; x += 0.3)
            for (const acdb::Triangle &triangle : shellHoleGeometry.fills)
                assert(!triangleContains({x, y}, triangle));

    acdb::TessellatedEntity multiHoleGeometry;
    acgs::ViewportDraw multiHoleContext(multiHoleGeometry);
    AcGiWorldDraw multiHoleDraw(multiHoleContext);
    const AcGePoint3d multiHoleVertices[] = {
        {0.0, 0.0, 0.0}, {10.0, 0.0, 0.0},
        {10.0, 10.0, 0.0}, {0.0, 10.0, 0.0},
        {1.0, 1.0, 0.0}, {1.0, 3.0, 0.0},
        {3.0, 3.0, 0.0}, {3.0, 1.0, 0.0},
        {6.0, 6.0, 0.0}, {6.0, 8.0, 0.0},
        {8.0, 8.0, 0.0}, {8.0, 6.0, 0.0}};
    const Adesk::Int32 multiHoleFaceList[] = {
        4, 0, 1, 2, 3, -4, 4, 5, 6, 7, -4, 8, 9, 10, 11};
    const Adesk::UInt8 multiHoleEdgeVisibility[] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    AcGiEdgeData multiHoleEdges;
    multiHoleEdges.setVisibility(multiHoleEdgeVisibility, 12);
    assert(!multiHoleDraw.shell(12, multiHoleVertices, 15,
                                multiHoleFaceList, &multiHoleEdges, &faces));
    double multiHoleArea = 0.0;
    for (const acdb::Triangle &triangle : multiHoleGeometry.fills)
        multiHoleArea +=
            0.5 * glm::length(glm::cross(triangle.b - triangle.a,
                                         triangle.c - triangle.a));
    assert(std::abs(multiHoleArea - 92.0) < 1.0e-8);
    for (const auto &holeBounds :
         {std::pair{1.0, 3.0}, std::pair{6.0, 8.0}})
        for (double y = holeBounds.first + 0.2;
             y < holeBounds.second - 0.1; y += 0.3)
            for (double x = holeBounds.first + 0.2;
                 x < holeBounds.second - 0.1; x += 0.3)
                for (const acdb::Triangle &triangle : multiHoleGeometry.fills)
                    assert(!triangleContains({x, y}, triangle));

    acdb::TessellatedEntity imageGeometry;
    acgs::ViewportDraw imageContext(imageGeometry);
    AcGiWorldDraw imageDraw(imageContext);
    const AcGiPixelBGRA32 pixel{0, 0, 255, 128};
    const AcGiImageBGRA32 image(1, 1, &pixel);
    assert(!imageDraw.image(image, {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
                            {0.0, 1.0, 0.0}));
    assert(imageGeometry.fills.size() == 2);
    assert(std::abs(imageGeometry.fills.front().common.color.alpha() -
                    128.0f / 255.0f) < 1.0e-5f);

    acdb::TessellatedEntity binaryAlphaGeometry;
    acgs::ViewportDraw binaryAlphaContext(binaryAlphaGeometry);
    AcGiWorldDraw binaryAlphaDraw(binaryAlphaContext);
    const AcGiPixelBGRA32 binaryAlphaPixels[] = {
        {0, 0, 255, 0}, {0, 255, 0, 254}, {255, 0, 0, 255}};
    const AcGiImageBGRA32 binaryAlphaImage(3, 1, binaryAlphaPixels);
    assert(!binaryAlphaDraw.image(
        binaryAlphaImage, {0.0, 0.0, 0.0}, {3.0, 0.0, 0.0},
        {0.0, 1.0, 0.0}, AcGiGeometry::kTransparency1Bit));
    assert(binaryAlphaGeometry.fills.size() == 2);
    assert(binaryAlphaGeometry.fills.front().common.color.renderColor().b >
           0.99f);
    assert(binaryAlphaGeometry.fills.front().common.color.renderColor().r <
           0.01f);

    acdb::TessellatedEntity opaqueModeGeometry;
    acgs::ViewportDraw opaqueModeContext(opaqueModeGeometry);
    AcGiWorldDraw opaqueModeDraw(opaqueModeContext);
    assert(!opaqueModeDraw.image(
        binaryAlphaImage, {0.0, 0.0, 0.0}, {3.0, 0.0, 0.0},
        {0.0, 1.0, 0.0}, AcGiGeometry::kTransparencyOff));
    assert(opaqueModeGeometry.fills.size() == 6);
    assert(std::abs(opaqueModeGeometry.fills.front().common.color.alpha() -
                    1.0f) < 1.0e-5f);

    acdb::TessellatedEntity edgeGeometry;
    acgs::ViewportDraw edgeContext(edgeGeometry);
    AcGiWorldDraw edgeDraw(edgeContext);
    const std::vector<AcGiEdge2d> edgeLoop{
        {AcGeLineSeg2d(AcGePoint2d(0.0, 0.0), AcGePoint2d(1.0, 0.0))},
        {AcGeLineSeg2d(AcGePoint2d(1.0, 0.0), AcGePoint2d(0.5, 1.0))},
        {AcGeLineSeg2d(AcGePoint2d(0.5, 1.0), AcGePoint2d(0.0, 0.0))}};
    assert(!edgeDraw.edge(edgeLoop));
    assert(edgeGeometry.fills.empty());
    assert(edgeGeometry.strokes.size() == edgeLoop.size());

    acdb::TessellatedEntity curvedEdgeGeometry;
    acgs::ViewportDraw curvedEdgeContext(curvedEdgeGeometry);
    AcGiWorldDraw curvedEdgeDraw(curvedEdgeContext);
    const std::vector<AcGiEdge2d> curvedEdges{
        {AcGeCircArc2d(AcGePoint2d(0.0, 0.0), 1.0, 0.0, 1.5707963267948966)},
        {AcGeEllipArc2d(AcGePoint2d(2.0, 0.0), AcGeVector2d::kXAxis,
                        AcGeVector2d::kYAxis, 2.0, 1.0, 0.0,
                        1.5707963267948966)}};
    assert(!curvedEdgeDraw.edge(curvedEdges));
    assert(curvedEdgeGeometry.fills.empty());
    assert(curvedEdgeGeometry.strokes.size() == curvedEdges.size());
    assert(curvedEdgeGeometry.strokes.front().points.size() > 2);
}
