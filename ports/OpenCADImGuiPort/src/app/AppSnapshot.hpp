#pragma once

#include <string>
#include <vector>

struct LayerInfo
{
    std::string name;
    bool visible{true};
    bool locked{false};
};

struct SceneObject
{
    int id{-1};
    std::string name;
    std::string layer{"0"};
    std::string type{"Line"};

    // Lines and rectangles use the two corner/endpoint fields. Circles use the
    // center fields plus radius.
    float x1{0.0f};
    float y1{0.0f};
    float x2{0.0f};
    float y2{0.0f};
    float radius{0.0f};
};

enum class SelectionModifier
{
    Replace,
    Toggle
};

struct AppSnapshot
{
    std::string statusText{"Ready"};
    std::string currentFile;
    std::string selectedObject{"None"};
    std::string currentLayer{"0"};
    std::string activeTool{"Select"};

    bool hasPendingDrawPoint{false};
    float pendingDrawX{0.0f};
    float pendingDrawY{0.0f};

    std::vector<SceneObject> objects;
    int nextObjectId{1};
    int selectedObjectId{-1};
    std::vector<int> selectedObjectIds;

    std::vector<std::string> commandHistory;

    std::vector<LayerInfo> layers{
        {"0", true, false},
        {"Walls", true, false},
        {"Dimensions", true, false},
        {"Annotations", true, false}
    };

    bool canUndo{false};
    bool canRedo{false};

    bool gridEnabled{true};
    bool snapEnabled{true};
    bool orthoEnabled{false};

    float fps{0.0f};
    float frameMs{0.0f};
};
