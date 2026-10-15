#include "acdb/AcDbDwgBridge.h"

namespace acdb
{

glm::vec4 aciToColor(int index)
{
    switch (index)
    {
    case 1: return {1.0f, 0.0f, 0.0f, 1.0f};
    case 2: return {1.0f, 1.0f, 0.0f, 1.0f};
    case 3: return {0.0f, 1.0f, 0.0f, 1.0f};
    case 4: return {0.0f, 1.0f, 1.0f, 1.0f};
    case 5: return {0.0f, 0.0f, 1.0f, 1.0f};
    case 6: return {1.0f, 0.0f, 1.0f, 1.0f};
    case 7: return {1.0f, 1.0f, 1.0f, 1.0f};
    case 8: return {0.5f, 0.5f, 0.5f, 1.0f};
    case 9: return {0.75f, 0.75f, 0.75f, 1.0f};
    case 250: return {0.2f, 0.2f, 0.2f, 1.0f};
    case 251: return {0.314f, 0.314f, 0.314f, 1.0f};
    case 252: return {0.412f, 0.412f, 0.412f, 1.0f};
    case 253: return {0.51f, 0.51f, 0.51f, 1.0f};
    case 254: return {0.745f, 0.745f, 0.745f, 1.0f};
    case 255: return {1.0f, 1.0f, 1.0f, 1.0f};
    default: break;
    }
    if (index >= 10 && index <= 249)
    {
        static constexpr float values[] = {
            1.0f, 1.0f, 0.647f, 0.647f, 0.498f,
            0.498f, 0.298f, 0.298f, 0.149f, 0.149f};
        const int hueIndex = (index - 10) / 10;
        const int shade = (index - 10) % 10;
        const float hue = static_cast<float>(hueIndex) * 15.0f;
        const float value = values[shade];
        const float saturation = (shade % 2 == 0) ? 1.0f : 0.5f;
        const float chroma = value * saturation;
        const float x = chroma *
            (1.0f - std::abs(std::fmod(hue / 60.0f, 2.0f) - 1.0f));
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        if (hue < 60.0f)
            red = chroma, green = x;
        else if (hue < 120.0f)
            red = x, green = chroma;
        else if (hue < 180.0f)
            green = chroma, blue = x;
        else if (hue < 240.0f)
            green = x, blue = chroma;
        else if (hue < 300.0f)
            red = x, blue = chroma;
        else
            red = chroma, blue = x;
        const float offset = value - chroma;
        return {red + offset, green + offset, blue + offset, 1.0f};
    }
    return {1.0f, 1.0f, 1.0f, 1.0f};
}

AcCmEntityColor::ColorMethod dwgColorMethod(std::uint8_t method)
{
    switch (method)
    {
    case 0xc0: return AcCmEntityColor::ColorMethod::kByLayer;
    case 0xc1: return AcCmEntityColor::ColorMethod::kByBlock;
    case 0xc3: return AcCmEntityColor::ColorMethod::kByColor;
    case 0xc4: return AcCmEntityColor::ColorMethod::kByPen;
    case 0xc5: return AcCmEntityColor::ColorMethod::kForeground;
    case 0xc6: return AcCmEntityColor::ColorMethod::kLayerOff;
    case 0xc7: return AcCmEntityColor::ColorMethod::kLayerFrozen;
    case 0xc8: return AcCmEntityColor::ColorMethod::kNone;
    default: return AcCmEntityColor::ColorMethod::kByACI;
    }
}

AcCmEntityColor dwgColorToEntityColor(const Dwg_Color &color)
{
    AcCmEntityColor result;
    const int index = color.index < 0 ? -color.index : color.index;
    AcCmEntityColor::ColorMethod method = dwgColorMethod(color.method);
    if (method == AcCmEntityColor::ColorMethod::kByACI && index == 256)
        method = AcCmEntityColor::ColorMethod::kByLayer;
    else if (method == AcCmEntityColor::ColorMethod::kByACI && index == 0)
        method = AcCmEntityColor::ColorMethod::kByBlock;
    const glm::vec4 rgba = [&] {
        if (color.method == 0xc3)
        {
            const std::uint32_t rgb = color.rgb & 0x00ffffffu;
            return glm::vec4(float((rgb >> 16) & 0xffu) / 255.0f,
                             float((rgb >> 8) & 0xffu) / 255.0f,
                             float(rgb & 0xffu) / 255.0f, 1.0f);
        }
        return aciToColor(index);
    }();
    result.setFileColor(method, color.index, glm::vec3(rgba));
    result.setAlpha(rgba.a);
    return result;
}

glm::vec4 dwgColorToColor(const Dwg_Color &color)
{
    return dwgColorToEntityColor(color).renderColor();
}

double lineWeightValueToPixels(int hundredthsOfMillimeter)
{
    return hundredthsOfMillimeter >= 0
               ? (double(hundredthsOfMillimeter) / 100.0) * (96.0 / 25.4)
               : 0.0;
}

double dwgLineWeightToPixels(BITCODE_RC lineWeight)
{
    const int hundredthsOfMillimeter =
        dxf_cvt_lweight(static_cast<BITCODE_BSd>(lineWeight));
    return lineWeightValueToPixels(hundredthsOfMillimeter);
}

int nearestAciIndex(const glm::vec4 &color)
{
    static constexpr std::pair<int, glm::vec3> kPalette[] = {
        {1, {1, 0, 0}}, {2, {1, 1, 0}}, {3, {0, 1, 0}},
        {4, {0, 1, 1}}, {5, {0, 0, 1}}, {6, {1, 0, 1}},
        {7, {1, 1, 1}}, {8, {0.5, 0.5, 0.5}}, {9, {0.75, 0.75, 0.75}},
        {250, {0.2, 0.2, 0.2}}, {251, {0.357, 0.357, 0.357}},
        {252, {0.517, 0.517, 0.517}}, {253, {0.678, 0.678, 0.678}},
        {254, {0.839, 0.839, 0.839}}, {255, {1, 1, 1}},
    };
    int best = 7;
    double bestDistance = 1.0e30;
    for (const auto &[index, rgb] : kPalette)
    {
        const glm::dvec3 delta = glm::dvec3(color) - glm::dvec3(rgb);
        const double distance = glm::dot(delta, delta);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = index;
        }
    }
    return best;
}

} // namespace acdb
