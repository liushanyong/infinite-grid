#pragma once

#include <SDL.h>

#include <fstream>
#include <iostream>
#include <sstream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "lib/coordinate/WorldRebase.h"
#include "lib/camera/orbit.h"
#include "lib/camera/fps.h"
#include "lib/rendering/RendererBackend.h"

inline constexpr int SCREEN_WIDTH = 1200;
inline constexpr int SCREEN_HEIGHT = 768;
inline constexpr float halfHMax = 1e8f;
inline constexpr float halfHMin = 1e-4f;

inline bool &useOrthoProjection()
{
    static bool enabled = false;
    return enabled;
}

inline float &orthoHalfHeight()
{
    static float halfHeight = 30.0f;
    return halfHeight;
}

inline WorldRebase &worldRebase()
{
    static WorldRebase instance;
    return instance;
}
