#pragma once

#include "IAppController.hpp"
#include "ProductionBridge.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

// Translates the immutable ImGui action boundary into the production bridge
// model. Stable UI ids decouple panels from OpenCADStudio's native handles.
class OpenCADStudioAppController final : public IAppController
{
public:
    explicit OpenCADStudioAppController(std::unique_ptr<ocs::IOpenCADStudioBridge> bridge);

    void execute(const UiAction& action) override;
    AppSnapshot snapshot() const override;
    bool closeRequested() const override;

private:
    ocs::CadHandle handleForUiId(int objectId) const;
    std::vector<ocs::CadHandle> handlesForUiIds(const std::vector<int>& objectIds) const;
    int uiIdForHandle(ocs::CadHandle handle) const;
    void apply(const ocs::BridgeResult& result);
    void runCommand(const std::string& command);

    std::unique_ptr<ocs::IOpenCADStudioBridge> bridge_;
    std::unordered_map<ocs::CadHandle, int> handleToUiId_;
    mutable int nextUiId_{1};
    mutable std::string lastStatus_{"Ready"};
    bool closeRequested_{false};
};
