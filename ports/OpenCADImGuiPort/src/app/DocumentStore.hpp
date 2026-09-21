#pragma once

#include "AppSnapshot.hpp"

#include <string>
#include <vector>

// DocumentStore owns the state that belongs to the CAD document. It is
// intentionally independent from ImGui and SDL so the UI port can later be
// connected to the production CAD document implementation through this seam.
struct DocumentMemento
{
    std::vector<SceneObject> objects;
    std::vector<LayerInfo> layers;
    std::string currentLayer;
    std::string activeTool;
    int nextObjectId{-1};
    int selectedObjectId{-1};
    std::vector<int> selectedObjectIds;
    bool hasPendingDrawPoint{false};
    float pendingDrawX{0.0f};
    float pendingDrawY{0.0f};
};

class DocumentStore
{
public:
    DocumentMemento capture() const;
    void restore(const DocumentMemento& memento);

    void loadDemoDrawing();

    std::string serialize() const;
    bool loadFromText(const std::string& text, std::string& error);
    bool saveToFile(const std::string& filePath, std::string& error);
    bool loadFromFile(const std::string& filePath, std::string& error);

    const std::vector<SceneObject>& objects() const { return objects_; }
    const std::vector<LayerInfo>& layers() const { return layers_; }

    const std::string& currentLayer() const { return currentLayer_; }
    const std::string& activeTool() const { return activeTool_; }

    int selectedObjectId() const { return selectedObjectId_; }
    const std::vector<int>& selectedObjectIds() const { return selectedObjectIds_; }
    int nextObjectId() const { return nextObjectId_; }

    bool hasPendingDrawPoint() const { return hasPendingDrawPoint_; }
    float pendingDrawX() const { return pendingDrawX_; }
    float pendingDrawY() const { return pendingDrawY_; }

    const SceneObject* findObject(int objectId) const;
    SceneObject* findObject(int objectId);

    bool isLayerLocked(const std::string& layerName) const;
    bool isLayerVisible(const std::string& layerName) const;
    const LayerInfo* findLayer(const std::string& layerName) const;
    LayerInfo* findLayer(const std::string& layerName);

    bool addLayer(const std::string& layerName);
    void setCurrentLayer(const std::string& layerName);
    bool toggleLayerVisible(const std::string& layerName);
    bool toggleLayerLocked(const std::string& layerName);

    void setTool(const std::string& toolName);
    SceneObject& addObject(
        const std::string& type,
        const std::string& layer,
        float x1,
        float y1,
        float x2,
        float y2,
        float radius
    );

    bool updateObject(
        int objectId,
        float x1,
        float y1,
        float x2,
        float y2,
        float radius
    );

    bool deleteObject(int objectId);
    void selectObject(int objectId, SelectionModifier modifier = SelectionModifier::Replace);
    void setSelection(const std::vector<int>& objectIds);

    void setPendingDrawPoint(float x, float y);
    void clearPendingDrawPoint();

private:
    std::vector<SceneObject> objects_;
    std::vector<LayerInfo> layers_{
        {"0", true, false},
        {"Walls", true, false},
        {"Dimensions", true, false},
        {"Annotations", true, false}
    };

    std::string currentLayer_{"0"};
    std::string activeTool_{"Select"};

    int nextObjectId_{1};
    int selectedObjectId_{-1};
    std::vector<int> selectedObjectIds_;

    bool hasPendingDrawPoint_{false};
    float pendingDrawX_{0.0f};
    float pendingDrawY_{0.0f};
};
