#pragma once

#include <glm/glm.hpp>

#include <optional>
#include <vector>

#include "entities/tessellate.h"
#include "rendering/RendererBackend.h"
#include "scene/DrawContext.h"

namespace scene
{

class SceneDrawList;

enum class AcGiShaderAlgorithm
{
    Realistic,
    Cad,
    Conceptual,
    Depth,
    Grayscale,
    Shaded,
    Sketch,
    Wireframe,
    XRay
};

struct AcGiMaterial
{
    AcGiShaderAlgorithm algorithm = AcGiShaderAlgorithm::Shaded;
    glm::vec4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 accentColor{1.0f, 1.0f, 1.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 0.35f;
    float transparency = 0.5f;
    float lineWidth = 1.0f;
};

// A submission-ready mesh command preserves instancing.  Large stress fields
// stay compact on the CPU while the backend retains control of its pipelines.
struct MeshBatchCommand
{
    rendering::MeshType prototype = rendering::MeshType::Cube;
    bool opaque = false;
    bool realistic = false;
    bool cadAlgorithm = false;
    glm::vec4 material{0.0f, 0.35f, 0.0f, 0.5f};
    AcGiMaterial acgiMaterial;
    std::vector<rendering::MeshInstance> instances;
};

// GPU curve commands keep double-precision authoring data on the CPU. The
// submitter rebases them; the curve shader performs the parameter evaluation.
struct CurveBatchCommand
{
    rendering::CurveAlgorithm algorithm = rendering::CurveAlgorithm::Bezier;
    int degree = 3;
    uint32_t sampleCount = 64;
    AcGiMaterial acgiMaterial;
    std::vector<glm::dvec3> controlPoints;
    std::vector<double> weights;
    std::vector<double> knots;
    glm::dvec3 center{0.0};
    glm::dvec3 axisU{1.0, 0.0, 0.0};
    glm::dvec3 axisV{0.0, 1.0, 0.0};
    double radius = 1.0;
    double startAngle = 0.0;
    double sweep = glm::two_pi<double>();
};

// The infinite grid is a protocol command, not a finite tessellated CAD line
// soup.  It keeps the shader-side infinite plane while making the renderer
// call explicit and owned by the scene submitter.
struct GridCommand
{
    rendering::GridRenderData data;
};

// The protocol root for every renderer-visible object in this project.  A
// drawable may collect strokes/fills/points, mesh batches, or protocol commands
// such as the infinite grid into one frame draw list.
class AcGiDrawable
{
public:
    virtual ~AcGiDrawable() = default;

    virtual void collect(SceneDrawList &drawList) const = 0;
};

// Frame-local and static draw lists share one command vocabulary.  Geometry
// carries strokes/fills/points through the existing WorldDraw sink; mesh and
// grid commands cover renderer-accelerated paths that must not be flattened.
class SceneDrawList final : public AcGiDrawable
{
public:
    [[nodiscard]] entities::TessellatedEntity &geometry() { return geometry_; }
    [[nodiscard]] const entities::TessellatedEntity &geometry() const
    {
        return geometry_;
    }

    [[nodiscard]] std::vector<MeshBatchCommand> &meshBatches()
    {
        return meshBatches_;
    }

    [[nodiscard]] std::vector<CurveBatchCommand> &curveBatches()
    {
        return curveBatches_;
    }
    [[nodiscard]] const std::vector<CurveBatchCommand> &curveBatches() const
    {
        return curveBatches_;
    }

    CurveBatchCommand &addCurveBatch()
    {
        curveBatches_.emplace_back();
        return curveBatches_.back();
    }
    [[nodiscard]] const std::vector<MeshBatchCommand> &meshBatches() const
    {
        return meshBatches_;
    }

    void setGrid(const rendering::GridRenderData &data)
    {
        grid_ = GridCommand{data};
    }
    [[nodiscard]] const std::optional<GridCommand> &grid() const
    {
        return grid_;
    }

    MeshBatchCommand &addMeshBatch(const rendering::MeshType prototype,
                                   const bool opaque, const bool realistic,
                                   const glm::vec4 &material,
                                   const bool cadAlgorithm = false)
    {
        meshBatches_.push_back(
            {prototype, opaque, realistic, cadAlgorithm, material, {}});
        meshBatches_.back().acgiMaterial.algorithm =
            realistic ? AcGiShaderAlgorithm::Realistic
                      : AcGiShaderAlgorithm::Shaded;
        return meshBatches_.back();
    }

    void merge(const SceneDrawList &other)
    {
        geometry_.strokes.insert(geometry_.strokes.end(),
                                 other.geometry_.strokes.begin(),
                                 other.geometry_.strokes.end());
        geometry_.fills.insert(geometry_.fills.end(),
                               other.geometry_.fills.begin(),
                               other.geometry_.fills.end());
        geometry_.points.insert(geometry_.points.end(),
                                other.geometry_.points.begin(),
                                other.geometry_.points.end());
        meshBatches_.insert(meshBatches_.end(),
                            other.meshBatches_.begin(),
                            other.meshBatches_.end());
        if (other.grid_)
            grid_ = *other.grid_;
    }

    void collect(SceneDrawList &drawList) const override
    {
        drawList.merge(*this);
    }

    void clear()
    {
        geometry_ = {};
        meshBatches_.clear();
        grid_.reset();
    }

private:
    entities::TessellatedEntity geometry_;
    std::vector<MeshBatchCommand> meshBatches_;
    std::vector<CurveBatchCommand> curveBatches_;
    std::optional<GridCommand> grid_;
};

} // namespace scene
