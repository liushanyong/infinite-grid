#include "OpenCADStudioAppController.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    using ocs::BridgeLayer;
    using ocs::BridgeObject;
    using ocs::BridgeResult;
    using ocs::BridgeSnapshot;
    using ocs::CadHandle;
    using ocs::IOpenCADStudioBridge;
    using ocs::SelectionModifier;

    class FakeBridge final : public IOpenCADStudioBridge
    {
    public:
        BridgeSnapshot snapshot() const override
        {
            BridgeSnapshot result;
            result.objects = objects;
            result.selectedHandles = selected;
            result.primaryHandle = primary;
            result.layers = layers;
            result.currentLayer = currentLayer;
            result.canUndo = canUndo;
            result.canRedo = canRedo;
            result.statusText = status;
            result.closeRequested = closeRequested;
            result.commandHistory = commandHistory;
            return result;
        }

        BridgeResult runCommand(const std::string& command) override
        {
            commandHistory.push_back(command);
            lastCommand = command;
            return done("Command: " + command);
        }

        ocs::BridgeCreateResult createObject(const ocs::BridgeObjectRequest& request) override
        {
            BridgeObject object;
            object.handle = nextHandle++;
            object.name = request.type + " " + std::to_string(object.handle);
            object.type = request.type;
            object.layer = request.layer;
            object.x1 = request.x1;
            object.y1 = request.y1;
            object.x2 = request.x2;
            object.y2 = request.y2;
            object.radius = request.radius;
            objects.push_back(object);
            select(object.handle, SelectionModifier::Replace);
            canUndo = true;
            canRedo = false;
            return {true, "Created " + object.name, object.handle};
        }

        BridgeResult updateObject(CadHandle handle, const ocs::BridgeObjectRequest& geometry) override
        {
            BridgeObject* object = find(handle);
            if (!object)
            {
                return {false, "Unknown object"};
            }
            object->x1 = geometry.x1;
            object->y1 = geometry.y1;
            object->x2 = geometry.x2;
            object->y2 = geometry.y2;
            object->radius = geometry.radius;
            canUndo = true;
            return {true, "Updated " + object->name};
        }

        BridgeResult deleteObjects(const std::vector<CadHandle>& handles) override
        {
            objects.erase(
                std::remove_if(
                    objects.begin(),
                    objects.end(),
                    [&](const BridgeObject& object) {
                        return std::find(handles.begin(), handles.end(), object.handle)
                            != handles.end();
                    }
                ),
                objects.end()
            );
            for (const CadHandle handle : handles)
            {
                select(handle, SelectionModifier::Replace);
            }
            canUndo = true;
            return {true, "Deleted"};
        }

        BridgeResult assignLayer(
            const std::vector<CadHandle>& handles,
            const std::string& layer
        ) override
        {
            for (CadHandle handle : handles)
            {
                if (BridgeObject* object = find(handle))
                {
                    object->layer = layer;
                }
            }
            canUndo = true;
            return {true, "Moved to " + layer};
        }

        BridgeResult setSelection(
            const std::vector<CadHandle>& handles,
            SelectionModifier modifier
        ) override
        {
            if (modifier == SelectionModifier::Replace)
            {
                selected = handles;
            }
            else
            {
                for (CadHandle handle : handles)
                {
                    const auto existing = std::find(selected.begin(), selected.end(), handle);
                    if (existing == selected.end())
                    {
                        selected.push_back(handle);
                    }
                    else
                    {
                        selected.erase(existing);
                    }
                }
            }
            primary = selected.empty() ? 0 : selected.back();
            return {true, "Selection changed"};
        }

        BridgeResult selectAll() override
        {
            selected.clear();
            for (const BridgeObject& object : objects)
            {
                selected.push_back(object.handle);
            }
            primary = selected.empty() ? 0 : selected.back();
            return {true, "Selected all"};
        }

        BridgeResult invertSelection() override
        {
            std::vector<CadHandle> inverted;
            for (const BridgeObject& object : objects)
            {
                if (std::find(selected.begin(), selected.end(), object.handle) == selected.end())
                {
                    inverted.push_back(object.handle);
                }
            }
            selected = inverted;
            primary = selected.empty() ? 0 : selected.back();
            return {true, "Selection inverted"};
        }

        BridgeResult addLayer(const std::string& name) override
        {
            layers.push_back({name, true, false});
            return {true, "Added layer " + name};
        }

        BridgeResult selectLayer(const std::string& name) override
        {
            currentLayer = name;
            return {true, "Current layer " + name};
        }

        BridgeResult setLayerVisible(const std::string& name, bool visible) override
        {
            BridgeLayer* layer = findLayer(name);
            if (layer)
            {
                layer->visible = visible;
            }
            return {true, "Layer visibility changed"};
        }

        BridgeResult setLayerLocked(const std::string& name, bool locked) override
        {
            BridgeLayer* layer = findLayer(name);
            if (layer)
            {
                layer->locked = locked;
            }
            return {true, "Layer lock changed"};
        }

        BridgeResult setTool(const std::string& tool) override
        {
            activeTool = tool;
            return {true, "Tool " + tool};
        }

        BridgeResult viewportPoint(double x, double y) override
        {
            lastPointX = x;
            lastPointY = y;
            return {true, "Viewport point"};
        }

        BridgeResult toggleGrid() override { return {true, "Grid toggled"}; }
        BridgeResult toggleSnap() override { return {true, "Snap toggled"}; }
        BridgeResult toggleOrtho() override { return {true, "Ortho toggled"}; }
        BridgeResult openFile(const std::string& path) override
        {
            currentFile = path;
            return {true, "Opened " + path};
        }
        BridgeResult saveFile(const std::string& path) override
        {
            currentFile = path;
            return {true, "Saved " + path};
        }
        BridgeResult undo() override
        {
            canUndo = false;
            canRedo = true;
            return {true, "Undo"};
        }
        BridgeResult redo() override
        {
            canUndo = true;
            canRedo = false;
            return {true, "Redo"};
        }
        BridgeResult clearCommandHistory() override
        {
            commandHistory.clear();
            return {true, "History cleared"};
        }
        BridgeResult focusViewport() override { return {true, "Viewport focused"}; }
        BridgeResult requestClose() override
        {
            closeRequested = true;
            return {true, "Exit requested"};
        }

        std::vector<BridgeObject> objects;
        std::vector<BridgeLayer> layers{{"0", true, false}, {"Walls", true, false}};
        std::vector<CadHandle> selected;
        CadHandle primary{0};
        std::string currentLayer{"0"};
        std::string activeTool{"Select"};
        std::string status{"Ready"};
        std::string currentFile;
        std::string lastCommand;
        std::vector<std::string> commandHistory;
        double lastPointX{0.0};
        double lastPointY{0.0};
        bool canUndo{false};
        bool canRedo{false};
        bool closeRequested{false};

    private:
        static BridgeResult done(const std::string& text) { return {true, text}; }

        BridgeObject* find(CadHandle handle)
        {
            const auto found = std::find_if(
                objects.begin(),
                objects.end(),
                [handle](const BridgeObject& object) { return object.handle == handle; }
            );
            return found == objects.end() ? nullptr : &*found;
        }

        BridgeLayer* findLayer(const std::string& name)
        {
            const auto found = std::find_if(
                layers.begin(),
                layers.end(),
                [&name](const BridgeLayer& layer) { return layer.name == name; }
            );
            return found == layers.end() ? nullptr : &*found;
        }

        void select(CadHandle handle, SelectionModifier modifier)
        {
            setSelection({handle}, modifier);
        }

        CadHandle nextHandle{10};
    };

    int countObjects(const AppSnapshot& snapshot, const std::string& type = {})
    {
        int count = 0;
        for (const SceneObject& object : snapshot.objects)
        {
            if (type.empty() || object.type == type)
            {
                ++count;
            }
        }
        return count;
    }

    void testSnapshotMappingAndSelection()
    {
        auto bridge = std::make_unique<FakeBridge>();
        FakeBridge& fake = *bridge;
        fake.objects.push_back({1, "Line 1", "0", "Line", 0, 0, 100, 100, 0});
        fake.objects.push_back({2, "Circle 2", "Walls", "Circle", 0, 0, 0, 0, 20});
        OpenCADStudioAppController controller(std::move(bridge));

        AppSnapshot snapshot = controller.snapshot();
        assert(countObjects(snapshot) == 2);
        assert(snapshot.objects[0].id == 1);
        assert(snapshot.objects[1].id == 2);

        UiAction select;
        select.type = UiActionType::SelectObject;
        select.objectId = 1;
        controller.execute(select);
        snapshot = controller.snapshot();
        assert(fake.selected.size() == 1 && fake.selected[0] == 1);
        assert(snapshot.selectedObjectIds.size() == 1);
        assert(snapshot.selectedObjectId == 1);

        select.selectionModifier = SelectionModifier::Toggle;
        controller.execute(select);
        assert(fake.selected.empty());
    }

    void testCreationUpdateAndDeleteUseNativeHandles()
    {
        auto bridge = std::make_unique<FakeBridge>();
        FakeBridge& fake = *bridge;
        OpenCADStudioAppController controller(std::move(bridge));

        UiAction create;
        create.type = UiActionType::CreateObject;
        create.shapeType = "Line";
        create.layer = "0";
        create.x1 = 1;
        create.y1 = 2;
        create.x2 = 3;
        create.y2 = 4;
        controller.execute(create);

        AppSnapshot snapshot = controller.snapshot();
        assert(countObjects(snapshot, "Line") == 1);
        assert(snapshot.selectedObjectId == 1);
        assert(fake.objects[0].handle == 10);

        UiAction update = create;
        update.type = UiActionType::UpdateObject;
        update.objectId = snapshot.selectedObjectId;
        update.x2 = 50;
        update.y2 = 60;
        controller.execute(update);
        assert(fake.objects[0].x2 == 50);
        assert(controller.snapshot().canUndo);

        UiAction remove;
        remove.type = UiActionType::DeleteObject;
        remove.objectId = snapshot.selectedObjectId;
        controller.execute(remove);
        assert(controller.snapshot().objects.empty());
    }

    void testCommandViewportFilesAndHistory()
    {
        auto bridge = std::make_unique<FakeBridge>();
        FakeBridge& fake = *bridge;
        OpenCADStudioAppController controller(std::move(bridge));

        UiAction command;
        command.type = UiActionType::RunCommand;
        command.payload = "LINE 0,0 10,10";
        controller.execute(command);
        assert(fake.lastCommand == "LINE 0,0 10,10");
        assert(controller.snapshot().commandHistory.size() == 1);

        UiAction click;
        click.type = UiActionType::ViewportClick;
        click.x1 = 12.5f;
        click.y1 = -3.25f;
        controller.execute(click);
        assert(fake.lastPointX == 12.5);
        assert(fake.lastPointY == -3.25);

        UiAction save;
        save.type = UiActionType::Save;
        save.payload = "test.dxf";
        controller.execute(save);
        assert(controller.snapshot().currentFile == "test.dxf");

        UiAction clear;
        clear.type = UiActionType::ClearHistory;
        controller.execute(clear);
        assert(controller.snapshot().commandHistory.empty());
    }
}

int main()
{
    testSnapshotMappingAndSelection();
    testCreationUpdateAndDeleteUseNativeHandles();
    testCommandViewportFilesAndHistory();

    std::cout << "OpenCADStudioAppController tests passed\n";
    return 0;
}
