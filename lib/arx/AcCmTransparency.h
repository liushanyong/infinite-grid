#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

class AcCmTransparency
{
public:
    enum transparencyMethod
    {
        kByLayer = 0,
        kByBlock,
        kByAlpha,
        kErrorValue
    };

    AcCmTransparency() = default;
    explicit AcCmTransparency(std::uint8_t alpha)
    {
        setAlpha(alpha);
    }
    explicit AcCmTransparency(double alpha)
    {
        const double bounded =
            std::isfinite(alpha) ? std::clamp(alpha, 0.0, 100.0) : 0.0;
        setAlpha(static_cast<std::uint8_t>(std::lround(bounded * 2.55)));
    }

    std::uint8_t alpha() const { return alpha_; }
    void setAlpha(std::uint8_t alpha)
    {
        alpha_ = alpha;
        method_ = kByAlpha;
    }
    bool isByAlpha() const { return method_ == kByAlpha; }
    transparencyMethod method() const { return method_; }
    void setMethod(transparencyMethod method)
    {
        method_ = method >= kByLayer && method <= kErrorValue
                      ? method
                      : kErrorValue;
    }

private:
    std::uint8_t alpha_ = 255;
    transparencyMethod method_ = kByLayer;
};
