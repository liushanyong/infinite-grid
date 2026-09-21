#include "DocumentAppController.hpp"

#include <algorithm>
#include <cassert>

#include <filesystem>
#include "DocumentStore.hpp"
#include <iostream>
#include <string>

namespace
{
    bool hasSelection(const AppSnapshot& snapshot, int objectId)
    {
        return std::find(
            snapshot.selectedObjectIds.begin(),
            snapshot.selectedObjectIds.end(),
            objectId
        ) != snapshot.selectedObjectIds.end();
    }

    int countLayers(const AppSnapshot& snapshot)
    {
        return static_cast<int>(snapshot.layers.size());
    }

    int countObjects(const AppSnapshot& snapshot, const std::string& type = {})
    {
        int count = 0;
        for (const auto& object : snapshot.objects)
        {
            if (type.empty() || object.type == type)
            {
                ++count;
            }
        }
        return count;
    }

    void testInitialDocument()
    {
        DocumentAppController controller;
        const AppSnapshot snapshot = controller.snapshot();
        assert(countObjects(snapshot) == 3);
        assert(snapshot.currentLayer == "0");
        assert(snapshot.selectedObjectId == -1);
    }

    void testCommandCreationAndSelection()
    {
        DocumentAppController controller;
        UiAction command;
        command.type = UiActionType::RunCommand;
        command.payload = "line 10,20 30,40";
        controller.execute(command);

        AppSnapshot snapshot = controller.snapshot();
        assert(countObjects(snapshot, "Line") == 2);
        assert(snapshot.selectedObjectId >= 0);
        assert(snapshot.selectedObject != "None");
    }

    void testLockedLayerBlocksCreationAndUpdate()
    {
        DocumentAppController controller;
        AppSnapshot snapshot = controller.snapshot();
        const int initialCount = countObjects(snapshot);

        UiAction lock;
        lock.type = UiActionType::ToggleLayerLocked;
        lock.payload = snapshot.currentLayer;
        controller.execute(lock);

        UiAction command;
        command.type = UiActionType::RunCommand;
        command.payload = "line 1,1 2,2";
        controller.execute(command);
        snapshot = controller.snapshot();
        assert(countObjects(snapshot) == initialCount);

        assert(snapshot.selectedObjectId == -1);
        UiAction update;
        update.type = UiActionType::UpdateObject;
        update.objectId = 1;
        update.x2 = 999.0f;
        update.y2 = 999.0f;
        controller.execute(update);

        for (const auto& object : controller.snapshot().objects)
        {
            if (object.id == 1)
            {
                assert(object.x2 == 120.0f);
            }
        }

        controller.execute(lock);
        command.payload = "line 3,3 4,4";
        controller.execute(command);
        snapshot = controller.snapshot();
        assert(countObjects(snapshot) == initialCount + 1);
    }

    void testInteractiveToolCreatesCircle()
    {
        DocumentAppController controller;
        AppSnapshot snapshot = controller.snapshot();
        const int initialCount = countObjects(snapshot, "Circle");

        UiAction setTool;
        setTool.type = UiActionType::SetTool;
        setTool.payload = "Circle";
        controller.execute(setTool);

        UiAction click;
        click.type = UiActionType::ViewportClick;
        click.x1 = 10.0f;
        click.y1 = 20.0f;
        controller.execute(click);

        snapshot = controller.snapshot();
        assert(snapshot.hasPendingDrawPoint);
        assert(countObjects(snapshot, "Circle") == initialCount);

        click.x1 = 60.0f;
        click.y1 = 20.0f;
        controller.execute(click);
        snapshot = controller.snapshot();
        assert(!snapshot.hasPendingDrawPoint);
        assert(countObjects(snapshot, "Circle") == initialCount + 1);
    }

    void testDeleteAndClearHistory()
    {
        DocumentAppController controller;
        UiAction select;
        select.type = UiActionType::SelectObject;
        select.objectId = 1;
        controller.execute(select);
        assert(controller.snapshot().selectedObjectId == 1);

        UiAction remove;
        remove.type = UiActionType::DeleteObject;
        remove.objectId = 1;
        controller.execute(remove);

        AppSnapshot snapshot = controller.snapshot();
        assert(countObjects(snapshot) == 2);
        assert(snapshot.selectedObjectId == -1);

        UiAction command;
        command.type = UiActionType::RunCommand;
        command.payload = "grid";
        controller.execute(command);
        assert(!controller.snapshot().commandHistory.empty());

        UiAction clear;
        clear.type = UiActionType::ClearHistory;
        controller.execute(clear);
        assert(controller.snapshot().commandHistory.empty());
    }

    void testUndoRedo()
    {
        DocumentAppController controller;
        assert(countObjects(controller.snapshot()) == 3);
        assert(!controller.snapshot().canUndo);

        UiAction command;
        command.type = UiActionType::RunCommand;
        command.payload = "line 10,20 30,40";
        controller.execute(command);
        assert(countObjects(controller.snapshot()) == 4);
        assert(controller.snapshot().canUndo);

        UiAction undo;
        undo.type = UiActionType::Undo;
        controller.execute(undo);
        assert(countObjects(controller.snapshot()) == 3);
        assert(controller.snapshot().canRedo);

        UiAction redo;
        redo.type = UiActionType::Redo;
        controller.execute(redo);
        assert(countObjects(controller.snapshot()) == 4);
        assert(controller.snapshot().canUndo);

        UiAction remove;
        remove.type = UiActionType::DeleteObject;
        remove.objectId = controller.snapshot().selectedObjectId;
        controller.execute(remove);
        assert(countObjects(controller.snapshot()) == 3);

        controller.execute(undo);
        assert(countObjects(controller.snapshot()) == 4);
        assert(controller.snapshot().selectedObjectId >= 0);

        controller.execute(redo);
        assert(countObjects(controller.snapshot()) == 3);
        assert(controller.snapshot().selectedObjectId == -1);
    }

    void testAssignObjectLayer()
    {
        DocumentAppController controller;

        UiAction assign;
        assign.type = UiActionType::AssignObjectLayer;
        assign.objectId = 1;
        assign.payload = "Annotations";
        controller.execute(assign);

        AppSnapshot snapshot = controller.snapshot();
        bool assigned = false;
        for (const auto& object : snapshot.objects)
        {
            if (object.id == 1)
            {
                assigned = object.layer == "Annotations";
            }
        }
        assert(assigned);
        assert(snapshot.canUndo);

        UiAction lock;
        lock.type = UiActionType::ToggleLayerLocked;
        lock.payload = "Annotations";
        controller.execute(lock);

        assign.objectId = 2;
        assign.payload = "Annotations";
        controller.execute(assign);

        for (const auto& object : controller.snapshot().objects)
        {
            if (object.id == 2)
            {
                assert(object.layer == "Walls");
            }
        }
    }

    void testDocumentSerializationRoundTrip()
    {
        DocumentStore original;
        original.loadDemoDrawing();
        const std::string text = original.serialize();

        DocumentStore loaded;
        std::string error;
        assert(loaded.loadFromText(text, error));
        assert(loaded.objects().size() == 3);
        assert(loaded.layers().size() == 4);
        assert(loaded.currentLayer() == original.currentLayer());
        assert(loaded.selectedObjectId() == original.selectedObjectId());

        const std::filesystem::path filePath =
            std::filesystem::temp_directory_path() / "OpenCADImGuiPortDocumentTest.ocad";
        DocumentStore fileOriginal;
        fileOriginal.loadDemoDrawing();
        assert(fileOriginal.saveToFile(filePath.string(), error));

        DocumentStore fileLoaded;
        assert(fileLoaded.loadFromFile(filePath.string(), error));
        assert(fileLoaded.objects().size() == 3);
        assert(fileLoaded.layers().size() == 4);
        std::filesystem::remove(filePath);
    }

    void testControllerFileRoundTrip()
    {
        DocumentAppController controller;
        const std::filesystem::path filePath =
            std::filesystem::temp_directory_path() / "OpenCADImGuiPortControllerTest.ocad";

        UiAction save;
        save.type = UiActionType::Save;
        save.payload = filePath.string();
        controller.execute(save);

        UiAction create;
        create.type = UiActionType::CreateObject;
        create.shapeType = "Line";
        create.x1 = 1.0f;
        create.y1 = 2.0f;
        create.x2 = 30.0f;
        create.y2 = 40.0f;
        controller.execute(create);
        assert(countObjects(controller.snapshot()) == 4);

        UiAction open;
        open.type = UiActionType::OpenFile;
        open.payload = filePath.string();
        controller.execute(open);

        AppSnapshot snapshot = controller.snapshot();
        assert(countObjects(snapshot) == 3);
        assert(snapshot.currentFile == filePath.string());
        assert(!snapshot.canUndo);
        assert(!snapshot.canRedo);

        std::filesystem::remove(filePath);
    }

    void testAddLayer()
    {
        DocumentAppController controller;
        assert(countLayers(controller.snapshot()) == 4);

        UiAction action;
        action.type = UiActionType::AddLayer;
        action.payload = "Electrical";
        controller.execute(action);

        AppSnapshot snapshot = controller.snapshot();
        assert(countLayers(snapshot) == 5);
        assert(snapshot.canUndo);

        action.type = UiActionType::AddLayer;
        action.payload = "Electrical";
        controller.execute(action);
        snapshot = controller.snapshot();
        assert(countLayers(snapshot) == 5);

        UiAction undo;
        undo.type = UiActionType::Undo;
        controller.execute(undo);
        assert(countLayers(controller.snapshot()) == 4);
    }

    void testMultiSelectionSemantics()
    {
        DocumentAppController controller;

        UiAction selectFirst;
        selectFirst.type = UiActionType::SelectObject;
        selectFirst.objectId = 1;
        controller.execute(selectFirst);
        assert(controller.snapshot().selectedObjectIds.size() == 1);
        assert(controller.snapshot().selectedObjectId == 1);

        selectFirst.objectId = 2;
        selectFirst.selectionModifier = SelectionModifier::Toggle;
        controller.execute(selectFirst);
        AppSnapshot snapshot = controller.snapshot();
        assert(snapshot.selectedObjectIds.size() == 2);
        assert(hasSelection(snapshot, 1));
        assert(hasSelection(snapshot, 2));
        assert(snapshot.selectedObjectId == 2);

        selectFirst.objectId = 2;
        controller.execute(selectFirst);
        snapshot = controller.snapshot();
        assert(snapshot.selectedObjectIds.size() == 1);
        assert(hasSelection(snapshot, 1));
        assert(snapshot.selectedObjectId == 1);

        selectFirst.objectId = -1;
        selectFirst.selectionModifier = SelectionModifier::Replace;
        controller.execute(selectFirst);
        snapshot = controller.snapshot();
        assert(snapshot.selectedObjectIds.empty());
        assert(snapshot.selectedObjectId == -1);
    }

    void testBatchDeleteAndAssignLayer()
    {
        DocumentAppController controller;

        UiAction selectFirst;
        selectFirst.type = UiActionType::SelectObject;
        selectFirst.objectId = 1;
        controller.execute(selectFirst);

        selectFirst.objectId = 2;
        selectFirst.selectionModifier = SelectionModifier::Toggle;
        controller.execute(selectFirst);
        assert(controller.snapshot().selectedObjectIds.size() == 2);

        UiAction assign;
        assign.type = UiActionType::AssignObjectLayer;
        assign.objectId = -1;
        assign.payload = "Annotations";
        controller.execute(assign);

        AppSnapshot snapshot = controller.snapshot();
        for (const auto& object : snapshot.objects)
        {
            if (object.id == 1 || object.id == 2)
            {
                assert(object.layer == "Annotations");
            }
        }

        UiAction undo;
        undo.type = UiActionType::Undo;
        controller.execute(undo);
        for (const auto& object : controller.snapshot().objects)
        {
            if (object.id == 1)
            {
                assert(object.layer == "0");
            }
            if (object.id == 2)
            {
                assert(object.layer == "Walls");
            }
        }

        // Restore the selection that existed before undo.
        selectFirst.objectId = 1;
        selectFirst.selectionModifier = SelectionModifier::Replace;
        controller.execute(selectFirst);
        selectFirst.objectId = 2;
        selectFirst.selectionModifier = SelectionModifier::Toggle;
        controller.execute(selectFirst);

        UiAction remove;
        remove.type = UiActionType::DeleteObject;
        remove.objectId = -1;
        controller.execute(remove);

        snapshot = controller.snapshot();
        assert(countObjects(snapshot) == 1);
        assert(snapshot.selectedObjectIds.empty());

        controller.execute(undo);
        snapshot = controller.snapshot();
        assert(countObjects(snapshot) == 3);
        assert(snapshot.selectedObjectIds.size() == 2);
        assert(hasSelection(snapshot, 1));
        assert(hasSelection(snapshot, 2));
    }

    void testSelectionSerialization()
    {
        DocumentStore original;
        original.loadDemoDrawing();
        original.selectObject(1);
        original.selectObject(2, SelectionModifier::Toggle);
        assert(original.selectedObjectIds().size() == 2);

        DocumentStore loaded;
        std::string error;
        assert(loaded.loadFromText(original.serialize(), error));
        assert(loaded.selectedObjectIds().size() == 2);
        assert(loaded.selectedObjectIds()[0] == 1);
        assert(loaded.selectedObjectIds()[1] == 2);
        assert(loaded.selectedObjectId() == 2);
    }

    void testSelectAllAndInvert()
    {
        DocumentAppController controller;

        UiAction all;
        all.type = UiActionType::SelectAll;
        controller.execute(all);

        AppSnapshot snapshot = controller.snapshot();
        assert(snapshot.selectedObjectIds.size() == 3);
        assert(snapshot.selectedObjectId == 3);

        UiAction invert;
        invert.type = UiActionType::InvertSelection;
        controller.execute(invert);
        assert(controller.snapshot().selectedObjectIds.empty());

        UiAction select;
        select.type = UiActionType::SelectObject;
        select.objectId = 1;
        controller.execute(select);
        controller.execute(invert);

        snapshot = controller.snapshot();
        assert(snapshot.selectedObjectIds.size() == 2);
        assert(!hasSelection(snapshot, 1));
        assert(hasSelection(snapshot, 2));
        assert(hasSelection(snapshot, 3));
    }

    void testSetSelectionAction()
    {
        DocumentAppController controller;

        UiAction action;
        action.type = UiActionType::SetSelection;
        action.objectIds = {1, 2};
        controller.execute(action);

        AppSnapshot snapshot = controller.snapshot();
        assert(snapshot.selectedObjectIds.size() == 2);
        assert(snapshot.selectedObjectId == 2);

        action.objectIds = {2};
        action.selectionModifier = SelectionModifier::Toggle;
        controller.execute(action);

        snapshot = controller.snapshot();
        assert(snapshot.selectedObjectIds.size() == 1);
        assert(hasSelection(snapshot, 1));
        assert(snapshot.selectedObjectId == 1);
    }
}

int main()
{
    testInitialDocument();
    testCommandCreationAndSelection();
    testLockedLayerBlocksCreationAndUpdate();
    testInteractiveToolCreatesCircle();
    testDeleteAndClearHistory();
    testUndoRedo();
    testAssignObjectLayer();
    testDocumentSerializationRoundTrip();
    testControllerFileRoundTrip();
    testAddLayer();
    testMultiSelectionSemantics();
    testBatchDeleteAndAssignLayer();
    testSelectionSerialization();
    testSelectAllAndInvert();
    testSetSelectionAction();

    std::cout << "DocumentAppController tests passed\n";
    return 0;
}
