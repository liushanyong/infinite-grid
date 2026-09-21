#include "DocumentAppController.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    std::string objectLabel(const std::string& name, const std::string& type)
    {
        return name + " (" + type + ")";
    }

    bool parseCoordinate(const std::string& token, float& x, float& y)
    {
        const size_t comma = token.find(',');
        if (comma == std::string::npos)
        {
            return false;
        }

        try
        {
            size_t consumed = 0;
            x = std::stof(token.substr(0, comma), &consumed);
            if (consumed != comma)
            {
                return false;
            }

            const std::string yToken = token.substr(comma + 1);
            y = std::stof(yToken, &consumed);
            return consumed == yToken.size();
        }
        catch (...)
        {
            return false;
        }
    }

    bool parseFloat(const std::string& token, float& value)
    {
        try
        {
            size_t consumed = 0;
            value = std::stof(token, &consumed);
            return consumed == token.size();
        }
        catch (...)
        {
            return false;
        }
    }
}

DocumentAppController::DocumentAppController()
{
    document_.loadDemoDrawing();
}

void DocumentAppController::pushUndo()
{
    undoStack_.push_back(document_.capture());
    if (undoStack_.size() > 64)
    {
        undoStack_.erase(undoStack_.begin());
    }
    redoStack_.clear();
}

void DocumentAppController::undo()
{
    if (undoStack_.empty())
    {
        status_ = "Nothing to undo";
        return;
    }

    redoStack_.push_back(document_.capture());
    document_.restore(undoStack_.back());
    undoStack_.pop_back();
    status_ = "Undo";
}

void DocumentAppController::redo()
{
    if (redoStack_.empty())
    {
        status_ = "Nothing to redo";
        return;
    }

    undoStack_.push_back(document_.capture());
    document_.restore(redoStack_.back());
    redoStack_.pop_back();
    status_ = "Redo";
}

void DocumentAppController::createObjectFromAction(const UiAction& action)
{
    const std::string targetLayer = action.layer.empty() ? document_.currentLayer() : action.layer;
    if (document_.isLayerLocked(targetLayer))
    {
        status_ = "Layer locked: " + targetLayer;
        return;
    }

    pushUndo();
    const SceneObject& object = document_.addObject(
        action.shapeType,
        targetLayer,
        action.x1,
        action.y1,
        action.x2,
        action.y2,
        action.radius
    );

    document_.selectObject(object.id);
    status_ = "Created: " + objectLabel(object.name, object.type);
}

void DocumentAppController::runCommand(const std::string& command)
{
    commandHistory_.push_back(command);
    if (commandHistory_.size() > 64)
    {
        commandHistory_.erase(commandHistory_.begin());
    }

    std::istringstream tokens(command);
    std::string commandName;
    tokens >> commandName;

    if (commandName == "line")
    {
        std::string startToken;
        std::string endToken;
        float x1 = 0.0f;
        float y1 = 0.0f;
        float x2 = 0.0f;
        float y2 = 0.0f;

        if (tokens >> startToken >> endToken
            && parseCoordinate(startToken, x1, y1)
            && parseCoordinate(endToken, x2, y2))
        {
            UiAction create;
            create.type = UiActionType::CreateObject;
            create.shapeType = "Line";
            create.layer = document_.currentLayer();
            create.x1 = x1;
            create.y1 = y1;
            create.x2 = x2;
            create.y2 = y2;
            createObjectFromAction(create);
            status_ = "Created line on layer " + document_.currentLayer();
        }
        else
        {
            status_ = "Usage: line x1,y1 x2,y2";
        }
        return;
    }

    if (commandName == "rectangle")
    {
        std::string firstToken;
        std::string secondToken;
        float x1 = 0.0f;
        float y1 = 0.0f;
        float x2 = 0.0f;
        float y2 = 0.0f;

        if (tokens >> firstToken >> secondToken
            && parseCoordinate(firstToken, x1, y1)
            && parseCoordinate(secondToken, x2, y2))
        {
            UiAction create;
            create.type = UiActionType::CreateObject;
            create.shapeType = "Rectangle";
            create.layer = document_.currentLayer();
            create.x1 = x1;
            create.y1 = y1;
            create.x2 = x2;
            create.y2 = y2;
            createObjectFromAction(create);
            status_ = "Created rectangle on layer " + document_.currentLayer();
        }
        else
        {
            status_ = "Usage: rectangle x1,y1 x2,y2";
        }
        return;
    }

    if (commandName == "circle")
    {
        std::string centerToken;
        std::string radiusToken;
        float x = 0.0f;
        float y = 0.0f;
        float radius = 0.0f;

        if (tokens >> centerToken >> radiusToken
            && parseCoordinate(centerToken, x, y)
            && parseFloat(radiusToken, radius))
        {
            UiAction create;
            create.type = UiActionType::CreateObject;
            create.shapeType = "Circle";
            create.layer = document_.currentLayer();
            create.x1 = x;
            create.y1 = y;
            create.radius = radius;
            createObjectFromAction(create);
            status_ = "Created circle on layer " + document_.currentLayer();
        }
        else
        {
            status_ = "Usage: circle x,y radius";
        }
        return;
    }

    const float offset = 20.0f * static_cast<float>(document_.objects().size() % 8);
    if (command == "add line")
    {
        UiAction create;
        create.type = UiActionType::CreateObject;
        create.shapeType = "Line";
        create.layer = document_.currentLayer();
        create.x1 = offset;
        create.y1 = offset;
        create.x2 = offset + 120.0f;
        create.y2 = offset + 80.0f;
        createObjectFromAction(create);
    }
    else if (command == "add rectangle")
    {
        UiAction create;
        create.type = UiActionType::CreateObject;
        create.shapeType = "Rectangle";
        create.layer = document_.currentLayer();
        create.x1 = offset - 60.0f;
        create.y1 = offset - 40.0f;
        create.x2 = offset + 60.0f;
        create.y2 = offset + 40.0f;
        createObjectFromAction(create);
    }
    else if (command == "add circle")
    {
        UiAction create;
        create.type = UiActionType::CreateObject;
        create.shapeType = "Circle";
        create.layer = document_.currentLayer();
        create.x1 = offset;
        create.y1 = offset;
        create.radius = 60.0f;
        createObjectFromAction(create);
    }
    else if (command == "grid")
    {
        gridEnabled_ = !gridEnabled_;
        status_ = gridEnabled_ ? "Grid enabled" : "Grid disabled";
    }
    else if (command == "snap")
    {
        snapEnabled_ = !snapEnabled_;
        status_ = snapEnabled_ ? "Snap enabled" : "Snap disabled";
    }
    else if (command == "ortho")
    {
        orthoEnabled_ = !orthoEnabled_;
        status_ = orthoEnabled_ ? "Ortho enabled" : "Ortho disabled";
    }
    else if (commandName == "select" && command == "select all")
    {
        UiAction selectAll;
        selectAll.type = UiActionType::SelectAll;
        execute(selectAll);
    }
    else if (commandName == "invert")
    {
        UiAction invert;
        invert.type = UiActionType::InvertSelection;
        execute(invert);
    }
    else if (commandName == "deselect")
    {
        UiAction clear;
        clear.type = UiActionType::SelectObject;
        clear.objectId = -1;
        execute(clear);
    }
    else if (commandName == "save")
    {
        std::string path;
        std::getline(tokens >> std::ws, path);
        saveDrawing(path);
    }
    else if (commandName == "open")
    {
        std::string path;
        std::getline(tokens >> std::ws, path);
        loadDrawing(path);
    }
    else if (command == "delete")
    {
        UiAction deleteAction;
        deleteAction.type = UiActionType::DeleteObject;
        deleteAction.objectId = -1;
        execute(deleteAction);
    }
    else if (command == "exit")
    {
        closeRequested_ = true;
        status_ = "Exit requested";
    }
    else
    {
        status_ = "Command: " + command;
    }
}

void DocumentAppController::saveDrawing(const std::string& filePath)
{
    const std::string targetFile = filePath.empty() ? currentFile_ : filePath;
    if (targetFile.empty())
    {
        status_ = "No file path specified";
        return;
    }

    std::string error;
    if (document_.saveToFile(targetFile, error))
    {
        currentFile_ = targetFile;
        status_ = "Saved: " + currentFile_;
    }
    else
    {
        status_ = error;
    }
}

void DocumentAppController::loadDrawing(const std::string& filePath)
{
    const std::string targetFile = filePath.empty() ? currentFile_ : filePath;
    if (targetFile.empty())
    {
        status_ = "No file path specified";
        return;
    }

    std::string error;
    if (document_.loadFromFile(targetFile, error))
    {
        currentFile_ = targetFile;
        undoStack_.clear();
        redoStack_.clear();
        status_ = "Opened: " + currentFile_;
    }
    else
    {
        status_ = error;
    }
}

void DocumentAppController::execute(const UiAction& action)
{
    switch (action.type)
    {
    case UiActionType::None:
        break;

    case UiActionType::RunCommand:
        runCommand(action.payload);
        break;

    case UiActionType::ToggleGrid:
        gridEnabled_ = !gridEnabled_;
        status_ = gridEnabled_ ? "Grid enabled" : "Grid disabled";
        break;

    case UiActionType::ToggleSnap:
        snapEnabled_ = !snapEnabled_;
        status_ = snapEnabled_ ? "Snap enabled" : "Snap disabled";
        break;

    case UiActionType::ToggleOrtho:
        orthoEnabled_ = !orthoEnabled_;
        status_ = orthoEnabled_ ? "Ortho enabled" : "Ortho disabled";
        break;

    case UiActionType::SetTool:
        document_.setTool(action.payload);
        status_ = "Tool: " + document_.activeTool();
        break;

    case UiActionType::ViewportClick:
    {
        if (document_.activeTool() != "Line"
            && document_.activeTool() != "Rectangle"
            && document_.activeTool() != "Circle")
        {
            break;
        }

        if (!document_.hasPendingDrawPoint())
        {
            document_.setPendingDrawPoint(action.x1, action.y1);
            status_ = document_.activeTool() + ": set first point";
            break;
        }

        if (document_.activeTool() == "Circle")
        {
            const float dx = action.x1 - document_.pendingDrawX();
            const float dy = action.y1 - document_.pendingDrawY();
            const float radius = std::sqrt(dx * dx + dy * dy);
            if (radius < 0.001f)
            {
                status_ = "Circle radius is too small";
                break;
            }

            UiAction create;
            create.type = UiActionType::CreateObject;
            create.shapeType = "Circle";
            create.layer = document_.currentLayer();
            create.x1 = document_.pendingDrawX();
            create.y1 = document_.pendingDrawY();
            create.radius = radius;
            createObjectFromAction(create);
        }
        else
        {
            UiAction create;
            create.type = UiActionType::CreateObject;
            create.shapeType = document_.activeTool();
            create.layer = document_.currentLayer();
            create.x1 = document_.pendingDrawX();
            create.y1 = document_.pendingDrawY();
            create.x2 = action.x1;
            create.y2 = action.y1;
            createObjectFromAction(create);
        }

        document_.clearPendingDrawPoint();
        break;
    }

    case UiActionType::ToggleLayerVisibility:
        if (document_.findLayer(action.payload))
        {
            pushUndo();
        }
        if (document_.toggleLayerVisible(action.payload))
        {
            const LayerInfo* layer = document_.findLayer(action.payload);
            status_ = layer && layer->visible
                ? "Layer visible: " + action.payload
                : "Layer hidden: " + action.payload;
        }
        break;

    case UiActionType::ToggleLayerLocked:
        if (document_.findLayer(action.payload))
        {
            pushUndo();
        }
        if (document_.toggleLayerLocked(action.payload))
        {
            const LayerInfo* layer = document_.findLayer(action.payload);
            status_ = layer && layer->locked
                ? "Layer locked: " + action.payload
                : "Layer unlocked: " + action.payload;
        }
        break;

    case UiActionType::AddLayer:
    {
        if (document_.findLayer(action.payload) || action.payload.empty())
        {
            status_ = action.payload.empty()
                ? "Layer name cannot be empty"
                : "Layer already exists: " + action.payload;
            break;
        }

        pushUndo();
        document_.addLayer(action.payload);
        status_ = "Added layer: " + action.payload;
        break;
    }

    case UiActionType::SelectLayer:
        document_.setCurrentLayer(action.payload);
        status_ = "Current layer: " + action.payload;
        break;

    case UiActionType::SelectObject:
    {
        document_.selectObject(action.objectId, action.selectionModifier);
        const size_t selectedCount = document_.selectedObjectIds().size();
        if (selectedCount == 0)
        {
            status_ = "Selection cleared";
        }
        else if (selectedCount == 1)
        {
            const SceneObject* object = document_.findObject(document_.selectedObjectId());
            status_ = object
                ? "Selected: " + objectLabel(object->name, object->type)
                : "Selection cleared";
        }
        else
        {
            status_ = "Selected " + std::to_string(selectedCount) + " objects";
        }
        break;
    }

    case UiActionType::SetSelection:
    {
        std::vector<int> targetIds;
        if (action.selectionModifier == SelectionModifier::Toggle)
        {
            targetIds = document_.selectedObjectIds();
            for (const int objectId : action.objectIds)
            {
                const auto existing = std::find(targetIds.begin(), targetIds.end(), objectId);
                if (existing == targetIds.end())
                {
                    targetIds.push_back(objectId);
                }
                else
                {
                    targetIds.erase(existing);
                }
            }
        }
        else
        {
            targetIds = action.objectIds;
        }

        document_.setSelection(targetIds);
        status_ = targetIds.empty()
            ? "Selection cleared"
            : "Selected " + std::to_string(targetIds.size()) + " objects";
        break;
    }

    case UiActionType::SelectAll:
    {
        std::vector<int> objectIds;
        objectIds.reserve(document_.objects().size());
        for (const SceneObject& object : document_.objects())
        {
            objectIds.push_back(object.id);
        }
        document_.setSelection(objectIds);
        status_ = objectIds.empty()
            ? "No objects to select"
            : "Selected " + std::to_string(objectIds.size()) + " objects";
        break;
    }

    case UiActionType::InvertSelection:
    {
        std::vector<int> objectIds;
        objectIds.reserve(document_.objects().size());
        for (const SceneObject& object : document_.objects())
        {
            const bool isSelected = std::find(
                document_.selectedObjectIds().begin(),
                document_.selectedObjectIds().end(),
                object.id
            ) != document_.selectedObjectIds().end();
            if (!isSelected)
            {
                objectIds.push_back(object.id);
            }
        }
        document_.setSelection(objectIds);
        status_ = objectIds.empty()
            ? "Selection inverted: none"
            : "Selection inverted: " + std::to_string(objectIds.size()) + " objects";
        break;
    }

    case UiActionType::UpdateObject:
    {
        const SceneObject* object = document_.findObject(action.objectId);
        if (!object)
        {
            break;
        }

        if (document_.isLayerLocked(object->layer))
        {
            status_ = "Layer locked: " + object->layer;
            break;
        }

        pushUndo();
        document_.updateObject(action.objectId, action.x1, action.y1, action.x2, action.y2, action.radius);
        status_ = "Updated: " + object->name;
        break;
    }

    case UiActionType::DeleteObject:
    {
        std::vector<int> objectIds;
        if (action.objectId >= 0)
        {
            objectIds.push_back(action.objectId);
        }
        else
        {
            objectIds = document_.selectedObjectIds();
        }

        size_t lockedCount = 0;
        size_t deletableCount = 0;
        for (const int objectId : objectIds)
        {
            const SceneObject* object = document_.findObject(objectId);
            if (!object)
            {
                continue;
            }
            if (document_.isLayerLocked(object->layer))
            {
                ++lockedCount;
            }
            else
            {
                ++deletableCount;
            }
        }

        if (deletableCount == 0)
        {
            if (lockedCount > 0)
            {
                status_ = "All selected objects are on locked layers";
            }
            break;
        }

        pushUndo();
        for (const int objectId : objectIds)
        {
            const SceneObject* object = document_.findObject(objectId);
            if (object && !document_.isLayerLocked(object->layer))
            {
                document_.deleteObject(objectId);
            }
        }

        status_ = lockedCount == 0
            ? "Deleted " + std::to_string(deletableCount) + " object(s)"
            : "Deleted " + std::to_string(deletableCount) + " object(s), skipped "
              + std::to_string(lockedCount) + " locked object(s)";
        break;
    }

    case UiActionType::AssignObjectLayer:
    {
        const LayerInfo* targetLayer = document_.findLayer(action.payload);
        if (!targetLayer)
        {
            status_ = "Unknown layer: " + action.payload;
            break;
        }
        if (targetLayer->locked)
        {
            status_ = "Layer locked: " + action.payload;
            break;
        }

        std::vector<int> objectIds;
        if (action.objectId >= 0)
        {
            objectIds.push_back(action.objectId);
        }
        else
        {
            objectIds = document_.selectedObjectIds();
        }

        size_t movableCount = 0;
        size_t lockedCount = 0;
        for (const int objectId : objectIds)
        {
            const SceneObject* object = document_.findObject(objectId);
            if (!object || object->layer == action.payload)
            {
                continue;
            }
            if (document_.isLayerLocked(object->layer))
            {
                ++lockedCount;
            }
            else
            {
                ++movableCount;
            }
        }

        if (movableCount == 0)
        {
            status_ = lockedCount > 0
                ? "All selected objects are on locked layers"
                : "Objects already on layer " + action.payload;
            break;
        }

        pushUndo();
        for (const int objectId : objectIds)
        {
            SceneObject* object = document_.findObject(objectId);
            if (object && object->layer != action.payload && !document_.isLayerLocked(object->layer))
            {
                object->layer = action.payload;
            }
        }

        status_ = "Moved " + std::to_string(movableCount) + " object(s) to layer " + action.payload;
        break;
    }

    case UiActionType::CreateObject:
        createObjectFromAction(action);
        break;

    case UiActionType::OpenFile:
        loadDrawing(action.payload);
        break;

    case UiActionType::Save:
        saveDrawing(action.payload);
        break;

    case UiActionType::Undo:
        undo();
        break;

    case UiActionType::Redo:
        redo();
        break;

    case UiActionType::ClearHistory:
        commandHistory_.clear();
        status_ = "Command history cleared";
        break;

    case UiActionType::FocusViewport:
        status_ = "Viewport focused";
        break;

    case UiActionType::Exit:
        closeRequested_ = true;
        status_ = "Exit requested";
        break;
    }
}

AppSnapshot DocumentAppController::snapshot() const
{
    AppSnapshot snapshot;
    snapshot.statusText = status_;
    snapshot.currentFile = currentFile_;
    snapshot.currentLayer = document_.currentLayer();
    snapshot.activeTool = document_.activeTool();
    snapshot.hasPendingDrawPoint = document_.hasPendingDrawPoint();
    snapshot.pendingDrawX = document_.pendingDrawX();
    snapshot.pendingDrawY = document_.pendingDrawY();
    snapshot.objects = document_.objects();
    snapshot.nextObjectId = document_.nextObjectId();
    snapshot.selectedObjectId = document_.selectedObjectId();
    snapshot.selectedObjectIds = document_.selectedObjectIds();
    snapshot.commandHistory = commandHistory_;
    snapshot.layers = document_.layers();
    snapshot.canUndo = !undoStack_.empty();
    snapshot.canRedo = !redoStack_.empty();
    snapshot.gridEnabled = gridEnabled_;
    snapshot.snapEnabled = snapEnabled_;
    snapshot.orthoEnabled = orthoEnabled_;

    const std::vector<int>& selectedIds = document_.selectedObjectIds();
    if (selectedIds.empty())
    {
        snapshot.selectedObject = "None";
    }
    else if (selectedIds.size() == 1)
    {
        const SceneObject* selected = document_.findObject(document_.selectedObjectId());
        snapshot.selectedObject = selected
            ? objectLabel(selected->name, selected->type)
            : "None";
    }
    else
    {
        snapshot.selectedObject = std::to_string(selectedIds.size()) + " objects selected";
    }

    return snapshot;
}

bool DocumentAppController::closeRequested() const
{
    return closeRequested_;
}
