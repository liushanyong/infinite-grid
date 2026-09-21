#include "InputRouter.hpp"

void InputRouter::update(bool wantMouse, bool wantKeyboard, bool wantTextInput)
{
    wantMouse_ = wantMouse;
    wantKeyboard_ = wantKeyboard;
    wantTextInput_ = wantTextInput;
}
