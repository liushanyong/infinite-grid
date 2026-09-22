#include "OpenCADStudioAppController.hpp"

#include <algorithm>
#include <utility>

namespace
{
    ocs::SelectionModifier toBridgeModifier(SelectionModifier modifier)
    {
        return modifier == SelectionModifier::Toggle
            ? ocs::SelectionModifier::Toggle
            : ocs::SelectionModifier::Replace;
    }

    std::string selectedLabel(const AppSnapshot& snapshot)
    {
        if (snapshot.selectedObjectIds.empty())
        {
            return "None";
        }
        if (snapshot.selectedObjectIds.size() == 1)
        {
            const int selectedId = snapshot.selectedObjectId;
            const auto selected = std::find_if(
                snapshot.objects.begin(),
                snapshot.objects.end(),
                [selectedId](const SceneObject& object) { return object.id == selectedId; }
            );
            if (selected != snapshot.objects.end())
            {
                return selected->name + " (" + selected->type + ")";
            }
        }

        return std::to_string(snapshot.selectedObjectIds.size()) + " objects selected";
    }
}

OpenCADStudioAppController::OpenCADStudioAppController(
    std::unique_ptr<ocs::IOpenCADStudioBridge> bridge
)
    : bridge_(std::move(bridge))
{
}

void OpenCADStudioAppController::apply(const ocs::BridgeResult& result)
{
    if (!result.status.empty())
    {
        lastStatus_ = result.ok ? result.status : "Error: " + result.status;
    }
}

ocs::CadHandle OpenCADStudioAppController::handleForUiId(int objectId) const
{
    if (objectId < 0)
    {
        return 0;
    }

    for (const auto& [handle, uiId] : handleToUiId_)
    {
        if (uiId == objectId)
        {
            return handle;
        }
    }
    return 0;
}

std::vector<ocs::CadHandle> OpenCADStudioAppController::handlesForUiIds(
    const std::vector<int>& objectIds
) const
{
    std::vector<ocs::CadHandle> handles;
    handles.reserve(objectIds.size());
    for (const int objectId : objectIds)
    {
        const ocs::CadHandle handle = handleForUiId(objectId);
        if (handle != 0)
        {
            handles.push_back(handle);
        }
    }
    return handles;
}

int OpenCADStudioAppController::uiIdForHandle(ocs::CadHandle handle) const
{
    if (handle == 0)
    {
        return -1;
    }

    const auto existing = handleToUiId_.find(handle);
    if (existing != handleToUiId_.end())
    {
        return existing->second;
    }

    // Native handles stay stable across undo, so deleted and restored objects
    // keep the same outline/properties identity in the UI.
    handleToUiId_.emplace(handle, nextUiId_);
    ++nextUiId_;
    return nextUiId_ - 1;
}

void OpenCADStudioAppController::runCommand(const std::string& command)
{
    apply(bridge_->runCommand(command));
}

void OpenCADStudioAppController::execute(const UiAction& action)
{
    switch (action.type)
    {
    case UiActionType::None:
        break;

    case UiActionType::RunCommand:
        runCommand(action.payload);
        break;

    case UiActionType::ToggleGrid:
        apply(bridge_->toggleGrid());
        break;

    case UiActionType::ToggleSnap:
        apply(bridge_->toggleSnap());
        break;

    case UiActionType::ToggleOrtho:
        apply(bridge_->toggleOrtho());
        break;

    case UiActionType::AddLayer:
        apply(bridge_->addLayer(action.payload));
        break;

    case UiActionType::SelectLayer:
        apply(bridge_->selectLayer(action.payload));
        break;

    case UiActionType::ToggleLayerVisibility:
    {
        const ocs::BridgeSnapshot current = bridge_->snapshot();
        const auto layer = std::find_if(
            current.layers.begin(),
            current.layers.end(),
            [&action](const ocs::BridgeLayer& entry) { return entry.name == action.payload; }
        );
        if (layer != current.layers.end())
        {
            apply(bridge_->setLayerVisible(action.payload, !layer->visible));
        }
        break;
    }

    case UiActionType::ToggleLayerLocked:
    {
        const ocs::BridgeSnapshot current = bridge_->snapshot();
        const auto layer = std::find_if(
            current.layers.begin(),
            current.layers.end(),
            [&action](const ocs::BridgeLayer& entry) { return entry.name == action.payload; }
        );
        if (layer != current.layers.end())
        {
            apply(bridge_->setLayerLocked(action.payload, !layer->locked));
        }
        break;
    }

    case UiActionType::SetTool:
        apply(bridge_->setTool(action.payload));
        break;

    case UiActionType::ViewportClick:
        apply(bridge_->viewportPoint(
            static_cast<double>(action.x1),
            static_cast<double>(action.y1)
        ));
        break;

    case UiActionType::SelectObject:
    {
        std::vector<ocs::CadHandle> handles;
        if (action.objectId >= 0)
        {
            const ocs::CadHandle handle = handleForUiId(action.objectId);
            if (handle != 0)
            {
                handles.push_back(handle);
            }
        }
        apply(bridge_->setSelection(handles, toBridgeModifier(action.selectionModifier)));
        break;
    }

    case UiActionType::SetSelection:
        apply(bridge_->setSelection(
            handlesForUiIds(action.objectIds),
            toBridgeModifier(action.selectionModifier)
        ));
        break;

    case UiActionType::SelectAll:
        apply(bridge_->selectAll());
        break;

    case UiActionType::InvertSelection:
        apply(bridge_->invertSelection());
        break;

    case UiActionType::UpdateObject:
    {
        ocs::BridgeObjectRequest request;
        request.type = action.shapeType;
        request.layer = action.layer;
        request.x1 = action.x1;
        request.y1 = action.y1;
        request.x2 = action.x2;
        request.y2 = action.y2;
        request.radius = action.radius;
        apply(bridge_->updateObject(handleForUiId(action.objectId), request));
        break;
    }

    case UiActionType::AssignObjectLayer:
    {
        std::vector<ocs::CadHandle> handles;
        if (action.objectId >= 0)
        {
            handles.push_back(handleForUiId(action.objectId));
        }
        else
        {
            const ocs::BridgeSnapshot current = bridge_->snapshot();
            handles = current.selectedHandles;
        }
        apply(bridge_->assignLayer(handles, action.payload));
        break;
    }

    case UiActionType::DeleteObject:
    {
        std::vector<ocs::CadHandle> handles;
        if (action.objectId >= 0)
        {
            const ocs::CadHandle handle = handleForUiId(action.objectId);
            if (handle != 0)
            {
                handles.push_back(handle);
            }
        }
        else
        {
            handles = bridge_->snapshot().selectedHandles;
        }
        apply(bridge_->deleteObjects(handles));
        break;
    }

    case UiActionType::CreateObject:
    {
        ocs::BridgeObjectRequest request;
        request.type = action.shapeType;
        request.layer = action.layer;
        request.x1 = action.x1;
        request.y1 = action.y1;
        request.x2 = action.x2;
        request.y2 = action.y2;
        request.radius = action.radius;
        const ocs::BridgeCreateResult created = bridge_->createObject(request);
        apply({created.ok, created.status});
        break;
    }

    case UiActionType::OpenFile:
        apply(bridge_->openFile(action.payload));
        break;

    case UiActionType::Save:
        apply(bridge_->saveFile(action.payload));
        break;

    case UiActionType::Undo:
        apply(bridge_->undo());
        break;

    case UiActionType::Redo:
        apply(bridge_->redo());
        break;

    case UiActionType::ClearHistory:
        apply(bridge_->clearCommandHistory());
        break;

    case UiActionType::FocusViewport:
        apply(bridge_->focusViewport());
        break;

    case UiActionType::Exit:
        apply(bridge_->requestClose());
        break;
    }
}

AppSnapshot OpenCADStudioAppController::snapshot() const
{
    const ocs::BridgeSnapshot current = bridge_->snapshot();

    AppSnapshot result;
    result.statusText = current.statusText.empty() ? lastStatus_ : current.statusText;
    result.currentFile = current.currentFile;
    result.currentLayer = current.currentLayer;
    result.activeTool = current.activeTool;
    result.hasPendingDrawPoint = current.hasPendingDrawPoint;
    result.pendingDrawX = static_cast<float>(current.pendingDrawX);
    result.pendingDrawY = static_cast<float>(current.pendingDrawY);
    result.commandHistory = current.commandHistory;
    result.canUndo = current.canUndo;
    result.canRedo = current.canRedo;
    result.gridEnabled = current.gridEnabled;
    result.snapEnabled = current.snapEnabled;
    result.orthoEnabled = current.orthoEnabled;

    result.objects.reserve(current.objects.size());
    for (const ocs::BridgeObject& object : current.objects)
    {
        SceneObject& target = result.objects.emplace_back();
        target.id = uiIdForHandle(object.handle);
        target.name = object.name;
        target.layer = object.layer;
        target.type = object.type;
        target.x1 = static_cast<float>(object.x1);
        target.y1 = static_cast<float>(object.y1);
        target.x2 = static_cast<float>(object.x2);
        target.y2 = static_cast<float>(object.y2);
        target.radius = static_cast<float>(object.radius);
    }

    if (!result.objects.empty())
    {
        result.nextObjectId = std::max_element(
            result.objects.begin(),
            result.objects.end(),
            [](const SceneObject& left, const SceneObject& right) { return left.id < right.id; }
        )->id + 1;
    }

    for (const ocs::CadHandle handle : current.selectedHandles)
    {
        const int uiId = uiIdForHandle(handle);
        if (uiId >= 0)
        {
            result.selectedObjectIds.push_back(uiId);
        }
    }

    result.selectedObjectId = uiIdForHandle(current.primaryHandle);
    if (result.selectedObjectId < 0 && !result.selectedObjectIds.empty())
    {
        result.selectedObjectId = result.selectedObjectIds.back();
    }
    result.selectedObject = selectedLabel(result);
    result.layers.reserve(current.layers.size());
    for (const ocs::BridgeLayer& layer : current.layers)
    {
        result.layers.push_back({layer.name, layer.visible, layer.locked});
    }

    return result;
}

bool OpenCADStudioAppController::closeRequested() const
{
    return closeRequested_ || bridge_->snapshot().closeRequested;
}
