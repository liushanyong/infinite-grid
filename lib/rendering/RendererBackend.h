#pragma once

#include <SDL.h>

#include <memory>

#include <glm/glm.hpp>

namespace rendering
{

enum class BackendType
{
    Bgfx,
};

struct GridRenderData
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 invViewProj;
    glm::mat4 viewProj;
    glm::vec3 camFront;
    glm::vec3 orthoPlaneCenter;
    glm::vec3 orthoRight;
    glm::vec3 orthoUp;
    glm::vec3 planeOriginRelative;
    float plane;
    glm::vec3 planeNormal;
    glm::vec3 planeTangentU;
    glm::vec3 planeTangentV;
    glm::vec3 axisColorU;
    glm::vec3 axisColorV;
    glm::vec3 startAxisOrigin;
    glm::vec3 startAxisDirection;
    float startAxisVisible;
    glm::vec3 startAxisLine;
    glm::vec2 axisOriginGridRelative;
    glm::vec3 axisLineX;
    glm::vec3 axisLineZ;
    float orthoPlaneValid;
    float groundRelativeY;
    float isOrtho;
    float step;
    glm::vec2 axisVisible;
    float screenHeight;
    float screenWidth;
    glm::vec3 gridColorMajor;
    glm::vec3 gridColorMinor;
    float gridOpacity;
};

struct CubeRenderData
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 modelRelativePosition;
    glm::vec3 objectColor;
    float opacity;
};

struct AabbRenderData
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 relativeMin;
    glm::vec3 relativeMax;
    glm::vec3 color;
    float opacity;
};

struct WorldLineRenderData
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 relativeStart;
    glm::vec3 relativeEnd;
    float lineWidth;
};

struct TargetPointRenderData
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 relativePosition;
    float pointSize;
    glm::vec3 color;
    float isOrtho;
};

class RendererBackend
{
public:
    virtual ~RendererBackend() = default;

    virtual const char *name() const = 0;
    virtual Uint32 windowFlags() const = 0;
    virtual bool configureSDL() = 0;
    virtual bool initialize(SDL_Window *window) = 0;
    virtual void shutdown() = 0;
    virtual void beginFrame(const glm::vec4 &clearColor) = 0;
    virtual void endFrame() = 0;
    virtual void present() = 0;
    virtual void drawGrid(const GridRenderData &data) = 0;
    virtual void drawCube(const CubeRenderData &data) = 0;
    virtual void drawAabb(const AabbRenderData &data) = 0;
    virtual void drawWorldLine(const WorldLineRenderData &data) = 0;
    virtual void drawTargetPoint(const TargetPointRenderData &data) = 0;
};

std::unique_ptr<RendererBackend> createRenderer(BackendType type);

} // namespace rendering
