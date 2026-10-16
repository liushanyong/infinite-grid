#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "acdb/AcDbEntities.h"
#include "acdb/AcDbGltf.h"
#include "acdb/AcDbTessellate.h"
#include "acgi/AcGiWorldDraw.h"
#include "acgi/AcGiTextQueue.h"
#include "acgs/AcGsMath.h"
#include "acgs/AcGsPicking.h"
#include "acgs/model/AcGsDocumentReplayer.h"
#include "rendering/RenderTypes.h"

namespace application::demo
{

struct GltfStaticMesh
{
    rendering::StaticMeshVertices cpu;
    std::uint32_t gpuId = 0;
};

struct MeshEntityRecord
{
    acdb::AcDbPolyFaceMesh entity;
    AcGePoint3d worldPosition;
    float size;
    rendering::MeshType mesh = rendering::MeshType::Cube;
    std::shared_ptr<GltfStaticMesh> staticMesh;
    std::uint32_t diffuseTextureIndex = 0;
    float renderScale = 0.0f;

    float instanceScale() const { return renderScale > 0.0f ? renderScale : size; }

    bool realistic() const
    {
        return entity.style == acdb::MeshStyle::Realistic;
    }

    const std::string &displayName() const
    {
        return entity.name;
    }
};

using LargeCoordinateObject = MeshEntityRecord;

enum class VisibilityKind
{
    MeshObject,
    CenterCube,
    CadMesh,
    CadStroke,
    CadFill,
    CadPoint,
    CadCurve,
    CadText
};

struct GpuPickEntity
{
    VisibilityKind kind = VisibilityKind::MeshObject;
    const MeshEntityRecord *mesh = nullptr;
    const acgs::AcGsEntityRange *cadRange = nullptr;
    const acgs::CurveBatchCommand *curve = nullptr;
    const acgi::TextRequest *text = nullptr;

    bool operator==(const GpuPickEntity &other) const
    {
        return kind == other.kind && mesh == other.mesh &&
               cadRange == other.cadRange &&
               curve == other.curve &&
               text == other.text;
    }
};

struct MeshInstanceCacheEntry
{
    rendering::MeshInstance instance;
    AcGePoint3d position;
    float size = 0.0f;
    acgs::math::Vec4 color;
    acgs::math::Vec4 material;
};

enum class VisibilityState
{
    Offscreen,
    Tiny,
    Visible
};

struct VisibilityCandidate
{
    VisibilityKind kind = VisibilityKind::MeshObject;
    std::size_t entityIndex = 0;
    const acgs::AcGsEntityRange *cadRange = nullptr;
    std::size_t rangeBegin = 0;
    std::size_t rangeCount = 0;
    const MeshEntityRecord *mesh = nullptr;
    const acgs::CurveBatchCommand *curve = nullptr;
    acgs::math::DVec3 min{0.0};
    acgs::math::DVec3 max{0.0};
    acgs::math::DVec3 center{0.0};
    double lodSize = 0.0;
    acgs::math::Vec4 overlayColor{1.0f};
    float overlayPointSize = 2.0f;
};

struct VectorPrimitivesTessellation : acgs::AcGsMetafile
{
    std::vector<MeshEntityRecord> meshes;
};

struct GltfDemoModelAsset
{
    acdb::AcDbGltfImport import;
    std::vector<std::shared_ptr<GltfStaticMesh>> meshes;
    std::string name;
    double scale = 1.0;
    bool valid = false;
};

struct CadPairedBandPoints
{
    const acdb::Stroke *left = nullptr;
    const acdb::Stroke *right = nullptr;
    std::size_t segmentCount = 0;
};

struct WorldAabb
{
    acgs::math::DVec3 min;
    acgs::math::DVec3 max;
    bool valid = false;
};

struct CameraSpaceAabb
{
    double minX;
    double maxX;
    double minY;
    double maxY;
    double minDepth;
    double maxDepth;
};

struct PickRay
{
    AcGePoint3d origin;
    AcGeVector3d direction;
    acgs::AcGsPickView view;
};

struct PickDebugTrace
{
    std::string meshName;
    double meshDepth = 0.0;
    bool meshHit = false;
    std::string cadName;
    double cadDepth = 0.0;
    bool cadHit = false;
    std::string cadOverlayName;
    VisibilityKind cadOverlayKind = VisibilityKind::CadStroke;
    double cadOverlayDepth = 0.0;
    bool cadOverlayHit = false;
    std::size_t cadStrokeCount = 0;
    std::size_t cadFillCount = 0;
    std::size_t cadPointCount = 0;
};

struct PickResult
{
    AcGePoint3d pivot;
    double hitDepth = 0.0;
    bool hit = false;
    std::string objectName = "Scene";
    std::optional<std::size_t> faceIndex;
};

struct AutofocusResult
{
    std::string entityName;
    AcGePoint3d hitPivot;
    double viewDepth = 0.0;
    std::optional<std::size_t> faceIndex;
};

} // namespace application::demo
