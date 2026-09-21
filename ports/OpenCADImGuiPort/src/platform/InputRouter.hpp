#pragma once

class InputRouter
{
public:
    void update(bool wantMouse, bool wantKeyboard, bool wantTextInput);

    bool wantMouse() const { return wantMouse_; }
    bool wantKeyboard() const { return wantKeyboard_; }
    bool wantTextInput() const { return wantTextInput_; }

    bool viewportMouseAvailable() const { return !wantMouse_; }
    bool viewportKeyboardAvailable() const { return !wantKeyboard_; }

private:
    bool wantMouse_{false};
    bool wantKeyboard_{false};
    bool wantTextInput_{false};
};
