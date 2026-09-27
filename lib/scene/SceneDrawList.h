#pragma once

#include <glm/glm.hpp>

#include <optional>
#include <vector>

#include "entities/tessellate.h"
#include "rendering/RendererBackend.h"
#include "scene/DrawContext.h"

namespace scene
{

// A submission-ready mesh command preserves instancing.  Large stress fields
// stay compact on the CPU while the backend retains control of its pipelines.
struct MeshBatchCommand
{
    rendering::MeshType prototype = rendering::MeshType::Cube;
    bool opaque = false;
    bool realistic = false;
    bool cadAlgorithm = false;
    glm::vec4 material{0.0f, 0.35f, 0.0f, 0.5f};
    std::vector<rendering::MeshInstance> instances;
};

// The infinite grid is a protocol command, not a finite tessellated CAD line
// soup.  It keeps the shader-side infinite plane while making the renderer
// call explicit and owned by the scene submitter.
struct GridCommand
{
    rendering::GridRenderData data;
};

// Frame-local and static draw lists share one command vocabulary.  Geometry
// carries strokes/fills/points through the existing WorldDraw sink; mesh and
// grid commands cover renderer-accelerated paths that must not be flattened.
class SceneDrawList
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
        return meshBatches_.back();
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
    std::optional<GridCommand> grid_;
};

} // namespace scene
