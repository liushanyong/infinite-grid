#pragma once

#include <string>
#include <vector>

#include "AppSnapshot.hpp"

enum class UiActionType
{
    None,
    RunCommand,
    ToggleGrid,
    ToggleSnap,
    ToggleOrtho,
    AddLayer,
    SelectLayer,
    ToggleLayerVisibility,
    ToggleLayerLocked,
    SetTool,
    ViewportClick,
    SelectObject,
    SelectAll,
    SetSelection,
    InvertSelection,
    UpdateObject,
    AssignObjectLayer,
    DeleteObject,
    CreateObject,
    OpenFile,
    Save,
    FocusViewport,
    Undo,
    Redo,
    ClearHistory,
    Exit
};

struct UiAction
{
    UiActionType type{UiActionType::None};
    std::string payload;

    int objectId{-1};
    SelectionModifier selectionModifier{SelectionModifier::Replace};
    std::vector<int> objectIds;
    std::string shapeType{"Line"};
    std::string layer;
    float x1{0.0f};
    float y1{0.0f};
    float x2{0.0f};
    float y2{0.0f};
    float radius{0.0f};
};
