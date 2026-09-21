#pragma once

#include "DocumentStore.hpp"
#include "IAppController.hpp"

#include <string>
#include <vector>

struct DocumentMemento;

// DocumentAppController is the port's application adapter. The ImGui layer only
// knows IAppController; the next integration step is to implement this seam
// against the production CAD application instead of DocumentStore.
class DocumentAppController final : public IAppController
{
public:
    DocumentAppController();

    void execute(const UiAction& action) override;
    AppSnapshot snapshot() const override;
    bool closeRequested() const override;

private:
    void runCommand(const std::string& command);
    void createObjectFromAction(const UiAction& action);
    void pushUndo();
    void undo();
    void redo();
    void saveDrawing(const std::string& filePath);
    void loadDrawing(const std::string& filePath);

    DocumentStore document_;
    std::string status_{"Ready"};
    std::string currentFile_;
    std::vector<std::string> commandHistory_;
    std::vector<DocumentMemento> undoStack_;
    std::vector<DocumentMemento> redoStack_;

    bool gridEnabled_{true};
    bool snapEnabled_{true};
    bool orthoEnabled_{false};
    bool closeRequested_{false};
};
