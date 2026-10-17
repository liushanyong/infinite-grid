#pragma once

#include "AcGePort.h"

#include <array>
#include <cmath>
#include <functional>
#include <stdexcept>

#include "AcGePoint.h"

namespace AcGe
{
enum ExternalEntityKind
{
    kAcisEntity,
    kExternalEntityUndefined
};
}

namespace ge::detail
{
inline void validateExternalRange(double low, double high)
{
    if (!std::isfinite(low) || !std::isfinite(high) || !(low < high))
        throw std::invalid_argument(
            "External geometry requires a finite increasing parameter range.");
}
}

class ACGE_PORT AcGeExternalCurve2d
{
public:
    using Evaluator = std::function<AcGePoint2d(double)>;

    AcGeExternalCurve2d() = default;
    explicit AcGeExternalCurve2d(
        Evaluator evaluator, double low = 0.0, double high = 1.0,
        AcGe::ExternalEntityKind kind = AcGe::kExternalEntityUndefined)
    {
        set(std::move(evaluator), low, high, kind);
    }

    AcGeExternalCurve2d &set(
        Evaluator evaluator, double low = 0.0, double high = 1.0,
        AcGe::ExternalEntityKind kind = AcGe::kExternalEntityUndefined)
    {
        if (!evaluator)
            throw std::invalid_argument(
                "ExternalCurve2d requires an evaluator.");
        ge::detail::validateExternalRange(low, high);
        m_evaluator = std::move(evaluator);
        m_range = {low, high};
        m_kind = kind;
        return *this;
    }

    bool isDefined() const { return static_cast<bool>(m_evaluator); }
    AcGe::ExternalEntityKind externalCurveKind() const { return m_kind; }
    double minParameter() const { return m_range[0]; }
    double maxParameter() const { return m_range[1]; }
    AcGePoint2d pointAt(double parameter) const
    {
        requireDefined();
        return m_evaluator(parameter);
    }
    bool isOwnerOfCurve() const { return m_owner; }
    AcGeExternalCurve2d &setToOwnCurve()
    {
        m_owner = true;
        return *this;
    }

private:
    Evaluator m_evaluator;
    std::array<double, 2> m_range{0.0, 1.0};
    AcGe::ExternalEntityKind m_kind = AcGe::kExternalEntityUndefined;
    bool m_owner = false;

    void requireDefined() const
    {
        if (!m_evaluator)
            throw std::logic_error("ExternalCurve2d is not defined.");
    }
};

class ACGE_PORT AcGeExternalCurve3d
{
public:
    using Evaluator = std::function<AcGePoint3d(double)>;
    using Derivative = std::function<AcGeVector3d(double)>;

    AcGeExternalCurve3d() = default;
    explicit AcGeExternalCurve3d(
        Evaluator evaluator, double low = 0.0, double high = 1.0,
        AcGe::ExternalEntityKind kind = AcGe::kExternalEntityUndefined,
        Derivative derivative = {})
    {
        set(std::move(evaluator), low, high, kind, std::move(derivative));
    }

    AcGeExternalCurve3d &set(
        Evaluator evaluator, double low = 0.0, double high = 1.0,
        AcGe::ExternalEntityKind kind = AcGe::kExternalEntityUndefined,
        Derivative derivative = {})
    {
        if (!evaluator)
            throw std::invalid_argument(
                "ExternalCurve3d requires an evaluator.");
        ge::detail::validateExternalRange(low, high);
        m_evaluator = std::move(evaluator);
        m_derivative = std::move(derivative);
        m_range = {low, high};
        m_kind = kind;
        return *this;
    }

    bool isDefined() const { return static_cast<bool>(m_evaluator); }
    AcGe::ExternalEntityKind externalCurveKind() const { return m_kind; }
    double minParameter() const { return m_range[0]; }
    double maxParameter() const { return m_range[1]; }
    AcGePoint3d pointAt(double parameter) const
    {
        requireDefined();
        return m_evaluator(parameter);
    }
    AcGeVector3d firstDeriv(double parameter) const
    {
        requireDefined();
        if (!m_derivative)
            throw std::logic_error(
                "ExternalCurve3d has no analytic derivative.");
        return m_derivative(parameter);
    }
    bool isOwnerOfCurve() const { return m_owner; }
    AcGeExternalCurve3d &setToOwnCurve()
    {
        m_owner = true;
        return *this;
    }

private:
    Evaluator m_evaluator;
    Derivative m_derivative;
    std::array<double, 2> m_range{0.0, 1.0};
    AcGe::ExternalEntityKind m_kind = AcGe::kExternalEntityUndefined;
    bool m_owner = false;

    void requireDefined() const
    {
        if (!m_evaluator)
            throw std::logic_error("ExternalCurve3d is not defined.");
    }
};

class ACGE_PORT AcGeExternalSurface
{
public:
    using Evaluator = std::function<AcGePoint3d(double, double)>;

    AcGeExternalSurface() = default;
    explicit AcGeExternalSurface(
        Evaluator evaluator, double uLow = 0.0, double uHigh = 1.0,
        double vLow = 0.0, double vHigh = 1.0,
        AcGe::ExternalEntityKind kind = AcGe::kExternalEntityUndefined)
    {
        set(std::move(evaluator), uLow, uHigh, vLow, vHigh, kind);
    }

    AcGeExternalSurface &set(
        Evaluator evaluator, double uLow = 0.0, double uHigh = 1.0,
        double vLow = 0.0, double vHigh = 1.0,
        AcGe::ExternalEntityKind kind = AcGe::kExternalEntityUndefined)
    {
        if (!evaluator)
            throw std::invalid_argument(
                "ExternalSurface requires an evaluator.");
        ge::detail::validateExternalRange(uLow, uHigh);
        ge::detail::validateExternalRange(vLow, vHigh);
        m_evaluator = std::move(evaluator);
        m_uRange = {uLow, uHigh};
        m_vRange = {vLow, vHigh};
        m_kind = kind;
        return *this;
    }

    bool isDefined() const { return static_cast<bool>(m_evaluator); }
    AcGe::ExternalEntityKind externalSurfaceKind() const { return m_kind; }
    double minParameterU() const { return m_uRange[0]; }
    double maxParameterU() const { return m_uRange[1]; }
    double minParameterV() const { return m_vRange[0]; }
    double maxParameterV() const { return m_vRange[1]; }
    AcGePoint3d pointAt(double u, double v) const
    {
        requireDefined();
        return m_evaluator(u, v);
    }
    bool isOwnerOfSurface() const { return m_owner; }
    AcGeExternalSurface &setToOwnSurface()
    {
        m_owner = true;
        return *this;
    }

private:
    Evaluator m_evaluator;
    std::array<double, 2> m_uRange{0.0, 1.0};
    std::array<double, 2> m_vRange{0.0, 1.0};
    AcGe::ExternalEntityKind m_kind = AcGe::kExternalEntityUndefined;
    bool m_owner = false;

    void requireDefined() const
    {
        if (!m_evaluator)
            throw std::logic_error("ExternalSurface is not defined.");
    }
};
