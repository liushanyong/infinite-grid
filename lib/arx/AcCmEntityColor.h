#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <glm/glm.hpp>

class AcCmEntityColor
{
public:
    enum class ColorMethod : std::uint8_t
    {
        kByLayer = 0,
        kByBlock = 1,
        kByColor = 2,
        kByACI = 3,
        kByPen = 4,
        kForeground = 5,
        kLayerOff = 6,
        kLayerFrozen = 7,
        kNone = 8
    };

    AcCmEntityColor() = default;
    AcCmEntityColor(const glm::vec4 &rgba) { setRGB(rgba); }

    void setColorIndex(int value)
    {
        index_ = value;
        method_ = value == 0   ? ColorMethod::kByBlock
                  : value == 256 ? ColorMethod::kByLayer
                                 : ColorMethod::kByACI;
    }

    int colorIndex() const { return index_; }
    ColorMethod colorMethod() const { return method_; }

    void setColorMethod(ColorMethod value) { method_ = value; }

    void setRGB(const glm::vec4 &rgba)
    {
        setRGB(glm::vec3(rgba));
        alpha_ = rgba.a;
    }

    void setRGB(const glm::vec3 &rgb)
    {
        trueColor_ = (component(rgb.r) << 16) |
                     (component(rgb.g) << 8) | component(rgb.b);
        method_ = ColorMethod::kByColor;
        index_ = 0x100;
    }

    void setRGB(std::uint32_t rgb)
    {
        trueColor_ = rgb & 0x00ffffffu;
        method_ = ColorMethod::kByColor;
        index_ = 0x100;
    }

    void setAlpha(float value) { alpha_ = value; }
    glm::vec3 rgb() const
    {
        return {float((trueColor_ >> 16) & 0xffu) / 255.0f,
                float((trueColor_ >> 8) & 0xffu) / 255.0f,
                float(trueColor_ & 0xffu) / 255.0f};
    }
    std::uint32_t rgbValue() const { return trueColor_; }
    float alpha() const { return alpha_; }

    void setFileColor(ColorMethod method, int index, const glm::vec3 &rgb,
                      float alpha = 1.0f)
    {
        method_ = method;
        index_ = index;
        trueColor_ = (component(rgb.r) << 16) |
                     (component(rgb.g) << 8) | component(rgb.b);
        alpha_ = alpha;
    }

    glm::vec4 renderColor() const { return glm::vec4(rgb(), alpha_); }

    AcCmEntityColor &operator=(const glm::vec4 &rgba)
    {
        setRGB(rgba);
        return *this;
    }

private:
    static std::uint32_t component(float value)
    {
        if (!std::isfinite(value))
            return 0;
        return static_cast<std::uint32_t>(
            std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
    }

    ColorMethod method_ = ColorMethod::kByLayer;
    int index_ = 256;
    std::uint32_t trueColor_ = 0x00ffffffu;
    float alpha_ = 1.0f;
};
