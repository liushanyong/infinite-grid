#include "acgi/AcGiWorldDraw.h"

#include "acgs/model/AcGsModel.h"
#include "acgs/model/AcGsDocumentReplayer.h"
#include "acdb/AcDbDatabase.h"
#include "acdb/AcDbStore.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <cmath>
#include <limits>
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
    const short faceColors[] = {5};
    const short edgeColors[] = {2, 3, 4, 5};
    const acdb::AcDbObjectId faceLayers[] = {
        acdb::AcDbObjectId(acdb::AcDbHandle{101})};
    const acdb::AcDbObjectId edgeLayers[] = {
        acdb::AcDbObjectId(acdb::AcDbHandle{201}),
        acdb::AcDbObjectId(acdb::AcDbHandle{202}),
        acdb::AcDbObjectId(acdb::AcDbHandle{203}),
        acdb::AcDbObjectId(acdb::AcDbHandle{204})};
    const acdb::AcDbObjectId edgeLinetypes[] = {
        acdb::AcDbObjectId(acdb::AcDbHandle{301}),
        acdb::AcDbObjectId(acdb::AcDbHandle{302}),
        acdb::AcDbObjectId(acdb::AcDbHandle{303}),
        acdb::AcDbObjectId(acdb::AcDbHandle{304})};
    const acdb::AcDbObjectId materials[] = {
        acdb::AcDbObjectId(acdb::AcDbHandle{401})};
    const Adesk::LongPtr faceMarkers[] = {0x1234};
    const Adesk::LongPtr edgeMarkers[] = {0x11, 0x22, 0x33, 0x44};
    const AcCmTransparency faceTransparency[] = {
        AcCmTransparency(static_cast<std::uint8_t>(128))};
    AcGiFaceData faces;
    faces.setColors(faceColors, 1);
    faces.setTrueColors(fillColors, 1);
    faces.setLayers(faceLayers, 1);
    faces.setSelectionMarkers(faceMarkers, 1);
    faces.setMaterials(materials, 1);
    faces.setTransparency(faceTransparency, 1);
    AcGiVertexData vertices;
    vertices.setTrueColors(vertexColors, 4);
    vertices.setNormals(vertexNormals, 4);
    assert(vertices.orientationFlag() == kAcGiNoOrientation);
    vertices.setOrientationFlag(kAcGiClockwise);
    assert(vertices.orientationFlag() == kAcGiClockwise);
    AcGePoint3d allMappingCoords[] = {
        {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0}, {1.0, 1.0, 0.0}};
    AcGePoint3d mapMappingCoords[] = {
        {0.25, 0.25, 0.0}, {0.75, 0.25, 0.0},
        {0.25, 0.75, 0.0}, {0.75, 0.75, 0.0}};
    vertices.setMappingCoords(AcGiVertexData::kAllChannels,
                              allMappingCoords);
    vertices.setMappingCoords(AcGiVertexData::kMapChannels,
                              mapMappingCoords);
    assert(vertices.mappingCoords(AcGiVertexData::kAllChannels) ==
           allMappingCoords);
    assert(vertices.mappingCoords(AcGiVertexData::kMapChannels) ==
           mapMappingCoords);
    vertices.mappingCoords(AcGiVertexData::kAllChannels)[0].x = 0.5;
    assert(allMappingCoords[0].x == 0.5);
    vertices.setMappingCoords(AcGiVertexData::kMapChannels, nullptr);
    assert(vertices.mappingCoords(AcGiVertexData::kMapChannels) == nullptr);
    vertices.setMappingCoords(AcGiVertexData::kMapChannels,
                              mapMappingCoords);
    AcGiEdgeData edges;
    edges.setColors(edgeColors, 4);
    edges.setLayers(edgeLayers, 4);
    edges.setLineTypes(edgeLinetypes);
    assert(edges.linetypeIds() == edgeLinetypes);
    edges.setSelectionMarkers(edgeMarkers, 4);
    edges.setVisibility(edgeVisibility, 4);
    edges.setLineTypes(edgeLineTypes, 4);
    assert(!meshDraw.mesh(2, 2, meshVertices, &edges, &faces, &vertices));
    assert(meshGeometry.fills.size() == 2);
    assert(meshGeometry.strokes.size() == 2);
    assert(meshGeometry.strokes.front().common.lineType == "DASHED");
    assert(meshGeometry.strokes[0].layerId == edgeLayers[0]);
    assert(meshGeometry.strokes[0].linetypeId == edgeLinetypes[0]);
    assert(meshGeometry.strokes[0].selectionMarker == edgeMarkers[0]);
    assert(meshGeometry.strokes[0].hasColorIndex);
    assert(meshGeometry.strokes[0].colorIndex == edgeColors[0]);
    assert(meshGeometry.strokes[1].layerId == edgeLayers[2]);
    assert(meshGeometry.strokes[1].selectionMarker == edgeMarkers[2]);
    assert(meshGeometry.fills[0].layerId == faceLayers[0]);
    assert(meshGeometry.fills[0].materialId == materials[0]);
    assert(meshGeometry.fills[0].selectionMarker == faceMarkers[0]);
    assert(meshGeometry.fills[0].hasColorIndex);
    assert(meshGeometry.fills[0].colorIndex == faceColors[0]);
    assert(std::abs(meshGeometry.fills[0].common.color.alpha() -
                    128.0f / 255.0f) < 1.0e-5f);
    assert(meshGeometry.fills[0].hasVertexColors);
    assert(meshGeometry.fills[0].hasVertexNormals);
    assert(std::abs(meshGeometry.fills[0].vertexColors[0].a -
                    128.0f / 255.0f) < 1.0e-5f);
    assert(meshGeometry.fills[0].hasAllMappingCoords);
    assert(meshGeometry.fills[0].hasMapMappingCoords);
    assert(std::abs(meshGeometry.fills[0].allMappingCoords[0].x - 0.5) <
           1.0e-9);
    assert(std::abs(meshGeometry.fills[0].mapMappingCoords[0].x - 0.25) <
           1.0e-9);
    assert(std::abs(meshGeometry.fills[0].mapMappingCoords[0].y - 0.25) <
           1.0e-9);
    assert(std::abs(meshGeometry.fills[0].mapMappingCoords[1].x - 0.75) <
           1.0e-9);
    assert(std::abs(meshGeometry.fills[0].mapMappingCoords[2].y - 0.75) <
           1.0e-9);
    assert(meshGeometry.fills[0].hasTextureCoords);
    assert(std::abs(meshGeometry.fills[0].textureCoords[0].x - 0.25f) <
           1.0e-6f);
    AcGiMapper mappedFaceMapper;
    mappedFaceMapper.setTransform(
        AcGeMatrix3d::setToTranslation(AcGeVector3d(0.5, 0.25, 0.0)));
    mappedFaceMapper.setUTiling(AcGiMapper::kTile);
    mappedFaceMapper.setVTiling(AcGiMapper::kMirror);
    AcGiFaceData mappedFaceData;
    mappedFaceData.setMappers(&mappedFaceMapper, 1);
    acdb::TessellatedEntity mappedGeometry;
    acgs::ViewportDraw mappedContext(mappedGeometry);
    AcGiWorldDraw mappedDraw(mappedContext);
    assert(!mappedDraw.mesh(2, 2, meshVertices, nullptr, &mappedFaceData,
                            &vertices));
    assert(mappedGeometry.fills.size() == 2);
    assert(mappedGeometry.fills[0].hasTextureCoords);
    assert(std::abs(mappedGeometry.fills[0].textureCoords[0].x - 0.75f) <
           1.0e-6f);
    assert(std::abs(mappedGeometry.fills[0].textureCoords[0].y - 0.5f) <
           1.0e-6f);
    assert(mappedGeometry.fills[0].mapperUTiling == AcGiMapper::kTile);
    assert(mappedGeometry.fills[0].mapperVTiling == AcGiMapper::kMirror);
    AcGiMapper planarMapper;
    planarMapper.setProjection(AcGiMapper::kPlanar);
    AcGiFaceData planarFaceData;
    planarFaceData.setMappers(&planarMapper, 1);
    acdb::TessellatedEntity planarGeometry;
    acgs::ViewportDraw planarContext(planarGeometry);
    AcGiWorldDraw planarDraw(planarContext);
    assert(!planarDraw.mesh(2, 2, meshVertices, nullptr, &planarFaceData));
    assert(planarGeometry.fills.size() == 2);
    assert(planarGeometry.fills[0].hasTextureCoords);
    for (const glm::vec2 &uv : planarGeometry.fills[0].textureCoords)
        assert(uv.x >= 0.0f && uv.x <= 1.0f &&
               uv.y >= 0.0f && uv.y <= 1.0f);
    AcGiVertexData malformedVertices = vertices;
    AcGePoint3d invalidMappingCoords[] = {
        {0.0, 0.0, 0.0},
        {std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {1.0, 1.0, 0.0}};
    malformedVertices.setMappingCoords(AcGiVertexData::kMapChannels,
                                       invalidMappingCoords);
    assert(meshDraw.mesh(2, 2, meshVertices, nullptr, nullptr,
                         &malformedVertices));
    assert(meshGeometry.fills.size() == 2);
    AcGiFaceData nativeMapperApi;
    nativeMapperApi.setMappers(nullptr);
    assert(nativeMapperApi.mappers() == nullptr);
    assert(!nativeMapperApi.mapperCountKnownData());
    AcGiFaceData malformedMappers;
    int mapperStorage = 0;
    malformedMappers.setMappers(
        reinterpret_cast<const AcGiMapper *>(&mapperStorage), 2);
    assert(meshDraw.mesh(2, 2, meshVertices, nullptr, &malformedMappers));
    assert(meshGeometry.fills.size() == 2);
    AcGiEdgeData malformedEdges;
    malformedEdges.setLayers(edgeLayers, 1);
    assert(meshDraw.mesh(2, 2, meshVertices, &malformedEdges));
    assert(meshGeometry.fills.size() == 2);
    assert(meshGeometry.strokes.size() == 2);
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
    assert(shellGeometry.strokes.front().layerId == edgeLayers[0]);
    assert(shellGeometry.strokes.front().linetypeId == edgeLinetypes[0]);
    assert(shellGeometry.strokes.front().selectionMarker == edgeMarkers[0]);
    assert(shellGeometry.fills.front().layerId == faceLayers[0]);
    assert(shellGeometry.fills.front().materialId == materials[0]);
    assert(shellGeometry.fills.front().selectionMarker == faceMarkers[0]);
    assert(std::abs(shellGeometry.fills.front().common.color.alpha() -
                    128.0f / 255.0f) < 1.0e-5f);
    assert(shellGeometry.fills[0].hasVertexColors);
    assert(std::abs(shellGeometry.fills[0].vertexColors[0].a -
                    128.0f / 255.0f) < 1.0e-5f);
    assert(shellGeometry.fills[0].hasAllMappingCoords);
    assert(shellGeometry.fills[0].hasMapMappingCoords);
    assert(std::abs(shellGeometry.fills[0].mapMappingCoords[0].x - 0.25) <
           1.0e-9);
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
    assert(imageGeometry.fills.size() == 4);
    assert(imageGeometry.fills[0].imageIndex == 1);
    assert(!imageGeometry.fills[0].imageFallback);
    assert(imageGeometry.fills.back().imageFallback);
    assert(std::abs(imageGeometry.fills.back().common.color.alpha() -
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
    assert(binaryAlphaGeometry.images.size() == 1);
    assert(binaryAlphaGeometry.images[0].width == 3);
    assert(binaryAlphaGeometry.images[0].height == 1);
    assert(binaryAlphaGeometry.images[0].bgra[3] == 0);
    assert(binaryAlphaGeometry.fills.size() == 4);
    const auto fallbackFill = std::find_if(
        binaryAlphaGeometry.fills.begin(), binaryAlphaGeometry.fills.end(),
        [](const acdb::Triangle &triangle) { return triangle.imageFallback; });
    assert(fallbackFill != binaryAlphaGeometry.fills.end());
    assert(fallbackFill->common.color.renderColor().b > 0.99f);
    assert(fallbackFill->common.color.renderColor().g < 0.01f);
    assert(fallbackFill->common.color.renderColor().r < 0.01f);
    assert(binaryAlphaGeometry.fills[0].hasTextureCoords);
    assert(binaryAlphaGeometry.fills[0].imageIndex == 1);

    acdb::TessellatedEntity opaqueModeGeometry;
    acgs::ViewportDraw opaqueModeContext(opaqueModeGeometry);
    AcGiWorldDraw opaqueModeDraw(opaqueModeContext);
    assert(!opaqueModeDraw.image(
        binaryAlphaImage, {0.0, 0.0, 0.0}, {3.0, 0.0, 0.0},
        {0.0, 1.0, 0.0}, AcGiGeometry::kTransparencyOff));
    assert(opaqueModeGeometry.fills.size() == 8);
    assert(std::abs(opaqueModeGeometry.fills.front().common.color.alpha() -
                    1.0f) < 1.0e-5f);

    acgs::AcGsModel materialModel;
    acgs::AcGsModel materialPatch;
    const acdb::AcDbObjectId materialId{acdb::AcDbHandle{1001}};
    acgs::AcGiMaterial material;
    material.baseColor = {0.25f, 0.5f, 0.75f, 0.8f};
    materialPatch.setMaterial(materialId, material, 1);
    acdb::Triangle materialTriangle;
    materialTriangle.materialId = materialId;
    materialTriangle.imageIndex = 1;
    materialPatch.geometry().fills.push_back(materialTriangle);
    materialPatch.geometry().images.push_back({2, 1, {0, 0, 255, 255}, 0});
    materialModel.geometry().images.push_back({1, 1, {255, 0, 0, 255}, 0});
    materialModel.merge(materialPatch);
    assert(materialModel.geometry().images.size() == 2);
    assert(materialModel.geometry().fills.front().imageIndex == 2);
    const acgs::AcGiMaterialBinding *binding =
        materialModel.findMaterial(materialId);
    assert(binding != nullptr);
    assert(binding->imageIndex == 2);
    assert(binding->material == material);
    materialModel.geometry().fills.clear();
    materialModel.clearKeepCapacity();
    assert(materialModel.geometry().images.empty());
    assert(materialModel.findMaterial(materialId) == nullptr);

    acdb::TessellatedEntity traitGeometry;
    acgs::ViewportDraw traitContext(traitGeometry);
    AcGiWorldDraw traitDraw(traitContext);
    const acdb::AcDbObjectId traitMaterialId{acdb::AcDbHandle{1002}};
    traitDraw.setMaterial(traitMaterialId);
    assert(traitDraw.materialId() == traitMaterialId);
    assert(!traitDraw.worldLine({0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}));
    assert(!traitDraw.worldTriangle({0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
                                    {0.0, 1.0, 0.0}));
    assert(!traitDraw.worldPoint({0.5, 0.5, 0.0}));
    assert(traitGeometry.strokes.front().materialId == traitMaterialId);
    assert(traitGeometry.fills.front().materialId == traitMaterialId);
    assert(traitGeometry.points.front().materialId == traitMaterialId);

    acdb::AcDbPolyFaceMesh replayMesh;
    replayMesh.handle = acdb::AcDbHandle{2002};
    replayMesh.name = "MaterialReplayMesh";
    replayMesh.geometry.positions = {
        AcGePoint3d(0.0, 0.0, 0.0), AcGePoint3d(1.0, 0.0, 0.0),
        AcGePoint3d(0.0, 1.0, 0.0)};
    replayMesh.geometry.indices = {0, 1, 2};
    replayMesh.material.baseColorFactor = {0.2f, 0.4f, 0.8f, 0.9f};
    replayMesh.material.renderClass = acdb::RenderClass::Realistic;
    const acdb::AcDbObjectId replayMaterialId{replayMesh.handle};
    replayMesh.materialId = replayMaterialId;
    acdb::AcDbDatabase replayDocument;
    const acdb::AcDbHandle replayHandle =
        replayDocument.addEntity(std::move(replayMesh));
    assert(replayHandle == acdb::AcDbHandle{2002});
    acgs::AcGsMetafile replayTarget;
    acgs::AcGsDocumentReplayer replay;
    replay.record(replayDocument, {}, replayTarget);
    assert(replayTarget.geometry.fills.size() == 1);
    assert(replayTarget.geometry.fills.front().materialId ==
           replayMaterialId);
    const acgs::AcGiMaterialBinding *replayBinding =
        replayTarget.findMaterial(replayMaterialId);
    assert(replayBinding != nullptr);
    assert(replayBinding->material.baseColor.r > 0.19f);
    assert(replayBinding->material.baseColor.r < 0.21f);
    assert(replayBinding->material.algorithm ==
           acgs::AcGiShaderAlgorithm::Realistic);

    acdb::AcDbDatabase materialDocument;
    acdb::AcDbMaterial sharedMaterial;
    sharedMaterial.baseColorFactor = {0.3f, 0.6f, 0.9f, 1.0f};
    sharedMaterial.metallicFactor = 0.25f;
    sharedMaterial.roughnessFactor = 0.4f;
    const acdb::AcDbHandle sharedHandle =
        materialDocument.setMaterial("SharedSteel", sharedMaterial);
    assert(sharedHandle.isValid());
    assert(materialDocument.materialCount() == 1);
    assert(materialDocument.findMaterial("SharedSteel") != nullptr);

    acdb::AcDbPolyFaceMesh sharedMesh;
    sharedMesh.handle = acdb::AcDbHandle{3002};
    sharedMesh.name = "SharedMaterialMesh";
    sharedMesh.geometry.positions = {
        AcGePoint3d(0.0, 0.0, 0.0), AcGePoint3d(1.0, 0.0, 0.0),
        AcGePoint3d(0.0, 1.0, 0.0)};
    sharedMesh.geometry.indices = {0, 1, 2};
    sharedMesh.materialId = acdb::AcDbObjectId(sharedHandle);
    materialDocument.addEntity(std::move(sharedMesh));

    acgs::AcGsMetafile sharedTarget;
    acgs::AcGsDocumentReplayer sharedReplay;
    sharedReplay.record(materialDocument, {}, sharedTarget);
    assert(sharedTarget.geometry.fills.size() == 1);
    const acgs::AcGiMaterialBinding *sharedBinding =
        sharedTarget.findMaterial(acdb::AcDbObjectId(sharedHandle));
    assert(sharedBinding != nullptr);
    assert(sharedBinding->material.metallic > 0.24f &&
           sharedBinding->material.metallic < 0.26f);
    assert(sharedTarget.geometry.fills.front().materialId ==
           acdb::AcDbObjectId(sharedHandle));

    const std::filesystem::path materialStore =
        std::filesystem::temp_directory_path() /
        "AcGiWorldDrawMaterialStore.db";
    const acdb::StoreResult saved =
        acdb::saveDatabase(materialDocument,
                           materialStore.string().c_str());
    assert(saved.ok);
    acdb::AcDbDatabase loadedMaterialDocument;
    const acdb::StoreResult loaded =
        acdb::loadDatabase(materialStore.string().c_str(),
                           loadedMaterialDocument);
    assert(loaded.ok);
    assert(loadedMaterialDocument.materialCount() == 1);
    const acdb::AcDbMaterialRecord *loadedRecord =
        loadedMaterialDocument.findMaterial("SharedSteel");
    assert(loadedRecord != nullptr);
    assert(loadedRecord->material == sharedMaterial);
    const acdb::AcDbEntityVariant *loadedMesh =
        loadedMaterialDocument.getEntity(acdb::AcDbHandle{3002});
    assert(loadedMesh != nullptr);
    assert(acdb::common(*loadedMesh).materialId ==
           acdb::AcDbObjectId(sharedHandle));
    std::error_code ignored;
    std::filesystem::remove(materialStore, ignored);

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

    // ---- behavior transforms, viewport eye/DC protocol, curves, mapper ownership ----
    acdb::TessellatedEntity behaviorGeometry;
    acgs::ViewportDraw behaviorContext(behaviorGeometry);
    AcGiWorldDraw behaviorDraw(behaviorContext);
    behaviorDraw.pushPositionTransform(kAcGiWorldPosition,
                                       AcGePoint3d(1.0, 2.0, 3.0));
    AcGeMatrix3d actualTransform;
    behaviorDraw.getModelToWorldTransform(actualTransform);
    const AcGePoint3d actualPosition =
        transformBy(AcGePoint3d(), actualTransform);
    assert(std::abs(actualPosition.x - 1.0) < 1.0e-12 &&
           std::abs(actualPosition.y - 2.0) < 1.0e-12 &&
           std::abs(actualPosition.z - 3.0) < 1.0e-12);
    behaviorDraw.popModelTransform();

    // Simple identity camera: eye x/y/z are world x/y/z; DC maps eye
    // [0,1] to [0,1] while z stays in eye depth.
    AcGeMatrix3d worldToEye;
    AcGeMatrix3d eyeToDc;
    behaviorContext.setViewportFrame(1, 0.0, 0.0, 100.0, 100.0,
                                     worldToEye, eyeToDc);
    behaviorDraw.pushPositionTransform(kAcGiViewportPosition,
                                       AcGePoint3d(0.25, 0.5, 0.0));
    behaviorDraw.getModelToWorldTransform(actualTransform);
    const AcGePoint3d viewportPosition =
        transformBy(AcGePoint3d(), actualTransform);
    assert(std::abs(viewportPosition.x - 0.25) < 1.0e-12 &&
           std::abs(viewportPosition.y - 0.5) < 1.0e-12);
    behaviorDraw.popModelTransform();

    AcGePoint3d eyePoints[] = {{0.0, 0.0, 2.0}, {1.0, 1.0, 2.0}};
    assert(!behaviorDraw.polylineEye(2, eyePoints));
    assert(!behaviorGeometry.strokes.empty());
    assert(std::abs(behaviorGeometry.strokes.back().points.front().x -
                    0.0) < 1.0e-12);
    assert(std::abs(behaviorGeometry.strokes.back().points.back().x -
                    1.0) < 1.0e-12);
    AcGePoint3d dcPoints[] = {{0.25, 0.25, 0.0}, {0.75, 0.75, 0.0}};
    assert(!behaviorDraw.polygonDc(2, dcPoints));
    assert(behaviorGeometry.strokes.back().closed);

    AcGiOwnerDraw ownerDrawToken;
    AcGiOwnerDraw3d ownerDraw3dToken;
    assert(behaviorDraw.ownerDrawDc(1, 0, 0, 100, 100, &ownerDrawToken));
    AcGePoint3d ownerMinimum;
    AcGePoint3d ownerMaximum;
    assert(behaviorDraw.ownerDraw3d(ownerMinimum, ownerMaximum,
                                    &ownerDraw3dToken));

    acdb::TessellatedEntity curveGeometry;
    acgs::ViewportDraw curveContext(curveGeometry);
    AcGiWorldDraw curveDraw(curveContext);
    AcGeLineSeg3d line3d(AcGePoint3d(0.0, 0.0, 0.0),
                         AcGePoint3d(2.0, 0.0, 0.0));
    AcGeCurve3d genericCurve(line3d);
    assert(std::abs(genericCurve.length() - 2.0) < 1.0e-12);
    assert(!curveDraw.curve(&genericCurve));
    assert(curveGeometry.strokes.size() == 1);
    assert(curveGeometry.strokes.back().points.size() == 2);
    AcGeCircle3d circle3d(AcGePoint3d(0.0, 0.0, 0.0),
                          AcGeVector3d::kZAxis, 1.0);
    AcGeCurve3d genericCircle(circle3d);
    assert(!curveDraw.curve(&genericCircle));
    assert(curveGeometry.strokes.back().points.size() > 16);

    AcGeCircArc2d arc2d(AcGePoint2d(0.0, 0.0), 1.0, 0.0,
                        1.5707963267948966);
    AcGeCurve2d genericArc2d(arc2d);
    const std::vector<AcGiEdge2d> genericEdges{{genericArc2d}};
    acdb::TessellatedEntity genericEdgeGeometry;
    acgs::ViewportDraw genericEdgeContext(genericEdgeGeometry);
    AcGiWorldDraw genericEdgeDraw(genericEdgeContext);
    assert(!genericEdgeDraw.edge(genericEdges));
    assert(genericEdgeGeometry.strokes.size() == 1);
    assert(genericEdgeGeometry.strokes.front().points.size() > 2);

    AcGiMapper autoMapper;
    autoMapper.setProjection(AcGiMapper::kPlanar);
    autoMapper.setAutoTransform(AcGiMapper::kObject);
    AcGiFaceData autoFaceData;
    autoFaceData.setMappers(&autoMapper, 1);
    assert(autoFaceData.mappers() != nullptr);
    assert(autoFaceData.mapperCountData() == 1);
    assert(autoFaceData.mapperCountKnownData());
    AcGePoint3d autoMeshVertices[] = {
        {0.0, 0.0, 0.0}, {2.0, 0.0, 0.0},
        {0.0, 2.0, 0.0}, {2.0, 2.0, 0.0}};
    acdb::TessellatedEntity autoMapperGeometry;
    acgs::ViewportDraw autoMapperContext(autoMapperGeometry);
    AcGiWorldDraw autoMapperDraw(autoMapperContext);
    assert(!autoMapperDraw.mesh(2, 2, autoMeshVertices, nullptr,
                                &autoFaceData));
    assert(autoMapperGeometry.fills.size() == 2);
    assert(autoMapperGeometry.fills.front().hasTextureCoords);
    assert(std::abs(autoMapperGeometry.fills.front().textureCoords[0].x -
                    0.0f) < 1.0e-6f);
    assert(std::abs(autoMapperGeometry.fills.front().textureCoords[2].x -
                    1.0f) < 1.0e-6f);

    // Counted setters own their mapper values; borrowed setters retain ARX
    // lifetime semantics.
    AcGiFaceData owningFaceData;
    owningFaceData.setMappers(std::vector<AcGiMapper>{autoMapper});
    const AcGiMapper *ownedMapper = owningFaceData.mappers();
    assert(ownedMapper != nullptr && ownedMapper->autoTransform() ==
                                         AcGiMapper::kObject);

    // ARX-style AcGeCurve2d pointer arrays adapt to the edge-value pipeline.
    AcGeLineSeg2d genericLine2d(AcGePoint2d(0.0, 0.0),
                                AcGePoint2d(1.0, 0.0));
    AcGeCurve2d genericEdgeCurves[] = {
        AcGeCurve2d(genericLine2d), AcGeCurve2d(arc2d)};
    acdb::TessellatedEntity nativeEdgeGeometry;
    acgs::ViewportDraw nativeEdgeContext(nativeEdgeGeometry);
    AcGiWorldDraw nativeEdgeDraw(nativeEdgeContext);
    assert(!nativeEdgeDraw.edge(genericEdgeCurves, 2));
    assert(nativeEdgeGeometry.strokes.size() == 2);
    assert(nativeEdgeGeometry.strokes.front().points.size() == 2);
    assert(nativeEdgeGeometry.strokes.back().points.size() > 2);

    AcGiFaceData borrowedFaceData;
    {
        AcGiMapper borrowedMapper;
        borrowedFaceData.setMappers(&borrowedMapper);
        assert(borrowedFaceData.mappers() == &borrowedMapper);
    }
}
