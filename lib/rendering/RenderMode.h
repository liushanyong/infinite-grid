#pragma once

#include <cstdint>

namespace rendering {
// Consolidated CAD visual styles.  Flat/Gouraud variants were redundant in the
// current unified shader, and the edge variants are represented by one mode
// with an explicit edge pass.
enum class RenderMode : std::uint8_t {
    Wireframe2D,
    Wireframe3D,
    HiddenLine,
    Shaded,
    ShadedWithEdges,
    DepthBuffer,
};

struct RenderModeFlags {
    bool face3dFill = false;
    bool meshFill = false;
    bool show3dEdges = true;
    bool hiddenLine = false;
    bool show2dSolidFills = true;
    bool wireframe3d = false;
};

inline constexpr RenderModeFlags renderModeFlags(RenderMode mode) {
    switch (mode) {
    case RenderMode::Wireframe2D:
        return {false, false, true, false, true, false};
    case RenderMode::Wireframe3D:
        return {false, false, true, false, false, true};
    case RenderMode::HiddenLine:
        return {true, true, true, true, true, false};
    case RenderMode::Shaded:
        return {true, true, false, false, true, false};
    case RenderMode::ShadedWithEdges:
        return {true, true, true, false, true, false};
    case RenderMode::DepthBuffer:
        return {true, true, false, false, true, false};
    }
    return {};
}

inline constexpr const char *renderModeLabel(RenderMode mode) {
    switch (mode) {
    case RenderMode::Wireframe2D: return "Wireframe 2D";
    case RenderMode::Wireframe3D: return "Wireframe 3D";
    case RenderMode::HiddenLine: return "Hidden Line";
    case RenderMode::Shaded: return "Shaded";
    case RenderMode::ShadedWithEdges: return "Shaded + Edges";
    case RenderMode::DepthBuffer: return "Depth Buffer";
    }
    return "Wireframe 2D";
}

inline constexpr const char *renderModeCommand(RenderMode mode) {
    switch (mode) {
    case RenderMode::Wireframe2D: return "VISUALSTYLES WIREFRAME2D";
    case RenderMode::Wireframe3D: return "VISUALSTYLES WIREFRAME3D";
    case RenderMode::HiddenLine: return "VISUALSTYLES HIDDENLINE";
    case RenderMode::Shaded: return "VISUALSTYLES SHADED";
    case RenderMode::ShadedWithEdges: return "VISUALSTYLES SHADEDWITHEDGES";
    case RenderMode::DepthBuffer: return "VISUALSTYLES DEPTHBUFFER";
    }
    return "VISUALSTYLES WIREFRAME2D";
}

inline constexpr RenderMode nextRenderMode(RenderMode mode) {
    switch (mode) {
    case RenderMode::Wireframe2D: return RenderMode::Wireframe3D;
    case RenderMode::Wireframe3D: return RenderMode::HiddenLine;
    case RenderMode::HiddenLine: return RenderMode::Shaded;
    case RenderMode::Shaded: return RenderMode::ShadedWithEdges;
    case RenderMode::ShadedWithEdges: return RenderMode::DepthBuffer;
    case RenderMode::DepthBuffer: return RenderMode::Wireframe2D;
    }
    return RenderMode::Wireframe2D;
}

class RenderModeManager {
public:
    void set(RenderMode mode) { mode_ = mode; }
    RenderMode mode() const { return mode_; }
    RenderModeFlags flags() const { return renderModeFlags(mode_); }
    void cycle() { mode_ = nextRenderMode(mode_); }

private:
    RenderMode mode_ = RenderMode::Wireframe2D;
};
} // namespace rendering

