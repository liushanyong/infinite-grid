#include "acgi/AcGiWorldDraw.h"

#include <cassert>
#include <cmath>

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
    assert(!draw.polyPolygon(1, instances, positions, vertexCounts, polygon,
                             nullptr, nullptr, fillColors, fillOpacity));
    assert(geometry.fills.size() == 4);
    assert(geometry.strokes.size() == 4);
    for (const acdb::Triangle &triangle : geometry.fills)
        assert(triangle.common.color.renderColor().r > 0.99f);

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
    AcGiFaceData faces;
    faces.setTrueColors(fillColors, 1);
    AcGiVertexData vertices;
    vertices.setTrueColors(vertexColors, 4);
    vertices.setNormals(vertexNormals, 4);
    AcGiEdgeData edges;
    edges.setVisibility(edgeVisibility, 4);
    assert(!meshDraw.mesh(2, 2, meshVertices, &edges, &faces, &vertices));
    assert(meshGeometry.fills.size() == 2);
    assert(meshGeometry.strokes.size() == 2);
    assert(meshGeometry.fills[0].hasVertexColors);
    assert(meshGeometry.fills[0].hasVertexNormals);

    acdb::TessellatedEntity shellGeometry;
    acgs::ViewportDraw shellContext(shellGeometry);
    AcGiWorldDraw shellDraw(shellContext);
    const Adesk::Int32 shellFaces[] = {4, 0, 1, 3, 2, 0};
    assert(!shellDraw.shell(4, meshVertices, 6, shellFaces, &edges, &faces,
                            &vertices));
    assert(shellGeometry.fills.size() == 2);
    assert(shellGeometry.strokes.size() == 2);
    assert(shellGeometry.fills[0].hasVertexColors);

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
}
