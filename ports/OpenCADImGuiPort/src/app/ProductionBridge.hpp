#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ocs
{
    using CadHandle = std::uint64_t;

    enum class SelectionModifier
    {
        Replace,
        Toggle
    };

    struct BridgeResult
    {
        bool ok{false};
        std::string status;
    };

    struct BridgeCreateResult
    {
        bool ok{false};
        std::string status;
        CadHandle handle{0};
    };

    struct BridgeObjectRequest
    {
        std::string type;
        std::string layer;
        double x1{0.0};
        double y1{0.0};
        double x2{0.0};
        double y2{0.0};
        double radius{0.0};
    };

    struct BridgeObject
    {
        CadHandle handle{0};
        std::string name;
        std::string layer;
        std::string type;
        double x1{0.0};
        double y1{0.0};
        double x2{0.0};
        double y2{0.0};
        double radius{0.0};
    };

    struct BridgeLayer
    {
        std::string name;
        bool visible{true};
        bool locked{false};
    };

    struct BridgeSnapshot
    {
        std::string statusText{"Ready"};
        std::string currentFile;
        std::string currentLayer{"0"};
        std::string activeTool{"Select"};
        std::string selectedLabel{"None"};

        bool hasPendingDrawPoint{false};
        double pendingDrawX{0.0};
        double pendingDrawY{0.0};

        std::vector<BridgeObject> objects;
        std::vector<CadHandle> selectedHandles;
        CadHandle primaryHandle{0};
        std::vector<std::string> commandHistory;
        std::vector<BridgeLayer> layers;

        bool canUndo{false};
        bool canRedo{false};
        bool gridEnabled{true};
        bool snapEnabled{true};
        bool orthoEnabled{false};
        bool closeRequested{false};
    };

    // The first production implementation will be a Rust cdylib exposing this
    // semantic model over a C ABI. Tests use a fake bridge until that library
    // is linked into this independent port.
    class IOpenCADStudioBridge
    {
    public:
        virtual ~IOpenCADStudioBridge() = default;

        virtual BridgeSnapshot snapshot() const = 0;

        virtual BridgeResult runCommand(const std::string& command) = 0;
        virtual BridgeCreateResult createObject(const BridgeObjectRequest& request) = 0;
        virtual BridgeResult updateObject(
            CadHandle handle,
            const BridgeObjectRequest& geometry
        ) = 0;
        virtual BridgeResult deleteObjects(const std::vector<CadHandle>& handles) = 0;
        virtual BridgeResult assignLayer(
            const std::vector<CadHandle>& handles,
            const std::string& layer
        ) = 0;

        virtual BridgeResult setSelection(
            const std::vector<CadHandle>& handles,
            SelectionModifier modifier
        ) = 0;
        virtual BridgeResult selectAll() = 0;
        virtual BridgeResult invertSelection() = 0;

        virtual BridgeResult addLayer(const std::string& name) = 0;
        virtual BridgeResult selectLayer(const std::string& name) = 0;
        virtual BridgeResult setLayerVisible(const std::string& name, bool visible) = 0;
        virtual BridgeResult setLayerLocked(const std::string& name, bool locked) = 0;

        virtual BridgeResult setTool(const std::string& tool) = 0;
        virtual BridgeResult viewportPoint(double x, double y) = 0;
        virtual BridgeResult toggleGrid() = 0;
        virtual BridgeResult toggleSnap() = 0;
        virtual BridgeResult toggleOrtho() = 0;

        virtual BridgeResult openFile(const std::string& path) = 0;
        virtual BridgeResult saveFile(const std::string& path) = 0;
        virtual BridgeResult undo() = 0;
        virtual BridgeResult redo() = 0;
        virtual BridgeResult clearCommandHistory() = 0;
        virtual BridgeResult focusViewport() = 0;
        virtual BridgeResult requestClose() = 0;
    };
}
