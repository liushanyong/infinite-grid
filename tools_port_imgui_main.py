p = 'main.cpp'
with open(p, 'rb') as f:
    raw = f.read()

nl = b'\r\n' if b'\r\n' in raw[:4000] else b'\n'
L = lambda lines: nl.join(lines)

def sub(old, new, label, count=1):
    global raw
    for v_old, v_new in ((old, new), (old.replace(b'\n', b'\r\n'),
                                     new.replace(b'\n', b'\r\n'))):
        if v_old in raw:
            assert raw.count(v_old) == count, f'{label}: {raw.count(v_old)}x'
            raw = raw.replace(v_old, v_new, count)
            print(label, 'ok')
            return
    raise AssertionError(label)

# 1. includes
sub(b'#include "acgs/AcGsManager.h"',
    b'#include "acgs/AcGsManager.h"\n\n'
    b'#include <imgui.h>\n'
    b'#include "backends/imgui_impl_sdl3.h"',
    'includes')

# 2. statics before the acgsView() accessor
anchor = b'// The view owns the orbit camera; demo code keeps the orbitCam alias.'
assert raw.count(anchor) == 1
statics = L([
b'// ---- ImGui presentation mode ----',
b'// The host owns the ImGui context and the SDL3 platform backend; the',
b'// device consumes the draw data (view 21) and shows the offscreen scenes',
b'// as panel images.  In this mode the panels own the window, so the',
b'// legacy full-window mouse handlers step aside and each panel routes',
b'// its own input to its viewport through the AcGsView facade.',
b'static bool imguiRequested()',
b'{',
b'  static const bool requested = []() {',
b'    const char *value = std::getenv("GRID_IMGUI");',
b'    return value == nullptr || std::strcmp(value, "0") != 0;',
b'  }();',
b'  return requested;',
b'}',
b'',
b'static bool imguiActive()',
b'{',
b'  return imguiRequested() && ImGui::GetCurrentContext() != nullptr;',
b'}',
b'',
b'// GRID_MULTI_VIEW=1 enables the two-panel round-robin (each panel owns',
b'// an AcGsView); the default is one fullscreen panel -- the stable',
b'// baseline while dual-view interaction is being hardened.',
b'static bool multiViewEnabled()',
b'{',
b'  static const bool requested = []() {',
b'    const char *value = std::getenv("GRID_MULTI_VIEW");',
b'    return value != nullptr && std::strcmp(value, "0") != 0;',
b'  }();',
b'  return requested;',
b'}',
b'',
b'static bool imguiWantsMouse()',
b'{',
b'  return imguiActive();',
b'}',
b'',
b'// Per-panel layout cache (device pixels), published by the ImGui block',
b'// and consumed by the frame head (scene render size / round-robin).',
b'static ImVec2 g_panelPx[2] = {ImVec2(0, 0), ImVec2(0, 0)};',
b'static int g_imguiHoveredPanel = -1;',
b''])
raw = raw.replace(anchor, statics + anchor, 1)

# 3. ImGui context init after the renderer banner
sub(b'  std::cout << "Renderer backend: " << gsManager->deviceName()\n'
    b'              << " (" << gsManager->deviceApiName() << ")"\n'
    b'              << std::endl;',
    b'  std::cout << "Renderer backend: " << gsManager->deviceName()\n'
    b'              << " (" << gsManager->deviceApiName() << ")"\n'
    b'              << std::endl;\n\n'
    b'  if (imguiRequested())\n'
    b'  {\n'
    b'    IMGUI_CHECKVERSION();\n'
    b'    ImGui::CreateContext();\n'
    b'    ImGui::StyleColorsDark();\n'
    b'    ImGui_ImplSDL3_InitForOther(window);\n'
    b'    acgs::acgsGetManager()->setImGuiActive(true);\n'
    b'    std::cout << "ImGui presentation mode: on" << std::endl;\n'
    b'  }',
    'context init')

# 4. frame head: round-robin + scene size + slot (before beginFrame)
old = b'  if (!acgs::acgsGetManager()->beginFrame())'
assert raw.count(old) == 1
new = L([
b'  // SYCAD round-robin: each frame renders ONE viewport into the shared',
b'  // scene target, then blits into that viewport\'s persistent final;',
b'  // the other panel keeps displaying its last blit.',
b'  static std::uint64_t s_frameIndex = 0;',
b'  const int renderSlot =',
b'      imguiActive() && multiViewEnabled() &&',
b'              acgs::acgsGetManager()->viewCount() > 1',
b'          ? int(s_frameIndex % 2)',
b'          : 0;',
b'  ++s_frameIndex;',
b'  if (imguiActive() && g_panelPx[renderSlot].x > 0.0f &&',
b'      g_panelPx[renderSlot].y > 0.0f)',
b'  {',
b'    acgs::acgsGetManager()->setSceneRenderSize(',
b'        std::uint32_t(g_panelPx[renderSlot].x),',
b'        std::uint32_t(g_panelPx[renderSlot].y));',
b'  }',
b'  acgs::acgsGetManager()->setActiveSceneSlot(renderSlot);',
b'',
b'  if (!acgs::acgsGetManager()->beginFrame())'])
raw = raw.replace(old, new, 1)

# 5. setActiveView(renderSlot) after beginFrame
sub(b'  if (!acgs::acgsGetManager()->beginFrame())\n    return;',
    b'  if (!acgs::acgsGetManager()->beginFrame())\n    return;\n\n'
    b'  // The scene pipeline reads the ACTIVE view; round-robin aims it at\n'
    b'  // this frame\'s viewport.\n'
    b'  acgs::acgsGetManager()->setActiveView(renderSlot);',
    'setActiveView')

# 6. compositeFrame after endFrame
sub(b'    acgs::acgsGetManager()->endFrame();',
    b'    acgs::acgsGetManager()->endFrame();\n'
    b'    // Display stage: the offscreen product becomes the window image\n'
    b'    // (ImGui panels in ImGui mode, backbuffer resolve otherwise).\n'
    b'    acgs::acgsGetManager()->compositeFrame();',
    'compositeFrame')

with open(p, 'wb') as f:
    f.write(raw)
print('main part 1 done')
