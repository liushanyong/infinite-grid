#pragma once

#include <bgfx/bgfx.h>

// Minimal Dear ImGui renderer backend backed by bgfx.
//
// Platform input is handled by ImGui's SDL3 backend, while this backend only
// owns the bgfx resources needed to draw an ImGui frame on top of the 3D view.
void imguiBgfxCreate(float fontSize = 18.0f, const char* cjkFontPath = nullptr);
void imguiBgfxDestroy();

void imguiBgfxRenderDrawData(struct ImDrawData* drawData, bgfx::ViewId viewId = 255);

// Opt-in: clear the window backbuffer to |rgba| (bgfx byte order, R most
// significant) at the start of every UI view.  Hosts that let the ImGui
// panels own the window (no backbuffer scene beneath) need this so any
// row a widget does not paint is a deterministic color instead of stale
// frame pixels; default remains no-clear for hosts that composite the
// scene into the backbuffer beneath the UI.
void imguiBgfxSetViewClear(unsigned int rgba);

// Returns the font-atlas texture used by the bgfx ImGui backend.
bgfx::TextureHandle imguiBgfxGetFontTexture();
