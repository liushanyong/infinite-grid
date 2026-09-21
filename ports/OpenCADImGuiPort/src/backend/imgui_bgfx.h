#pragma once

#include <bgfx/bgfx.h>

// Minimal Dear ImGui renderer backend backed by bgfx.
//
// Platform input is handled by ImGui's SDL3 backend, while this backend only
// owns the bgfx resources needed to draw an ImGui frame on top of the 3D view.
void imguiBgfxCreate(float fontSize = 18.0f);
void imguiBgfxDestroy();

void imguiBgfxRenderDrawData(struct ImDrawData* drawData, bgfx::ViewId viewId = 255);
