#pragma once

#include "AcGePort.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include "AcGeArrays.h"
#include "AcGePoint.h"

class ACGE_PORT AcGeCurve2d
{
public:
    virtual ~AcGeCurve2d() = default;

    virtual double startParam() const = 0;
    virtual double endParam() const = 0;
    virtual AcGePoint2d pointAt(double param) const = 0;

    AcGePoint2d evalPoint(double param) const { return pointAt(param); }
    AcGePoint2d evalPoint(double param, int derivativeOrder,
                          AcGeVector2dArray &derivatives) const
    {
        if (derivativeOrder < 0 || derivativeOrder > 2)
            throw std::invalid_argument(
                "AcGeCurve2d evalPoint supports derivative orders 0 through 2.");
        derivatives.clear();
        const AcGePoint2d point = pointAt(param);
        if (derivativeOrder == 0)
            return point;

        const double lower = startParam();
        const double upper = endParam();
        const double span = upper - lower;
        if (std::isfinite(span) && !(span > 0.0))
            throw std::domain_error("AcGeCurve2d has no parameter span.");
        double h = 1.0e-4 * std::max(1.0, std::abs(param));
        if (std::isfinite(span))
            h = std::min(h, span * 0.25);
        const bool forward = std::isfinite(lower) && param - h < lower;
        const bool backward = std::isfinite(upper) && param + h > upper;
        const AcGePoint2d p1 =
            pointAt(backward ? param - h : param + h);
        const AcGePoint2d p2 = pointAt(
            forward ? param + 2.0 * h : backward ? param - 2.0 * h
                                                  : param - h);
        derivatives.reserve(static_cast<size_t>(derivativeOrder));
        if (forward)
            derivatives.push_back((p1 - point) / h);
        else if (backward)
            derivatives.push_back((point - p1) / h);
        else
            derivatives.push_back((p1 - p2) / (2.0 * h));
        if (derivativeOrder == 2)
        {
            if (forward)
                derivatives.push_back(((p2 - p1) - (p1 - point)) /
                                       (h * h));
            else if (backward)
                derivatives.push_back(((point - p1) - (p1 - p2)) /
                                       (h * h));
            else
                derivatives.push_back(((p1 - point) - (point - p2)) /
                                       (h * h));
        }
        return point;
    }
};

class ACGE_PORT AcGeCurve3d
{
public:
    virtual ~AcGeCurve3d() = default;

    virtual double startParam() const = 0;
    virtual double endParam() const = 0;
    virtual AcGePoint3d pointAt(double param) const = 0;

    AcGePoint3d evalPoint(double param) const { return pointAt(param); }
    AcGePoint3d evalPoint(double param, int derivativeOrder,
                          AcGeVector3dArray &derivatives) const
    {
        if (derivativeOrder < 0 || derivativeOrder > 2)
            throw std::invalid_argument(
                "AcGeCurve3d evalPoint supports derivative orders 0 through 2.");
        derivatives.clear();
        const AcGePoint3d point = pointAt(param);
        if (derivativeOrder == 0)
            return point;

        const double lower = startParam();
        const double upper = endParam();
        const double span = upper - lower;
        if (std::isfinite(span) && !(span > 0.0))
            throw std::domain_error("AcGeCurve3d has no parameter span.");
        double h = 1.0e-4 * std::max(1.0, std::abs(param));
        if (std::isfinite(span))
            h = std::min(h, span * 0.25);
        const bool forward = std::isfinite(lower) && param - h < lower;
        const bool backward = std::isfinite(upper) && param + h > upper;
        const AcGePoint3d p1 =
            pointAt(backward ? param - h : param + h);
        const AcGePoint3d p2 = pointAt(
            forward ? param + 2.0 * h : backward ? param - 2.0 * h
                                                  : param - h);
        derivatives.reserve(static_cast<size_t>(derivativeOrder));
        if (forward)
            derivatives.push_back((p1 - point) / h);
        else if (backward)
            derivatives.push_back((point - p1) / h);
        else
            derivatives.push_back((p1 - p2) / (2.0 * h));
        if (derivativeOrder == 2)
        {
            if (forward)
                derivatives.push_back(((p2 - p1) - (p1 - point)) /
                                       (h * h));
            else if (backward)
                derivatives.push_back(((point - p1) - (p1 - p2)) /
                                       (h * h));
            else
                derivatives.push_back(((p1 - point) - (point - p2)) /
                                       (h * h));
        }
        return point;
    }
};

class ACGE_PORT AcGeSurface
{
public:
    virtual ~AcGeSurface() = default;

    virtual double minParameterU() const = 0;
    virtual double maxParameterU() const = 0;
    virtual double minParameterV() const = 0;
    virtual double maxParameterV() const = 0;
    virtual AcGePoint3d pointAt(double u, double v) const = 0;

    bool isClosedInU(const AcGeTol &tol = AcGeTol{}) const
    {
        return isClosedAtSeam(true, tol);
    }

    bool isClosedInV(const AcGeTol &tol = AcGeTol{}) const
    {
        return isClosedAtSeam(false, tol);
    }

    AcGePoint3d evalPoint(const AcGePoint2d &param) const
    {
        return pointAt(param.x, param.y);
    }

    AcGePoint3d evalPoint(const AcGePoint2d &param, int derivativeOrder,
                          AcGeVector3dArray &derivatives) const
    {
        if (derivativeOrder < 0 || derivativeOrder > 2)
            throw std::invalid_argument(
                "AcGeSurface evalPoint supports derivative orders 0 through 2.");
        derivatives.clear();
        if (derivativeOrder == 0)
            return pointAt(param.x, param.y);
        AcGeVector3d ignoredNormal;
        return evalPoint(param, derivativeOrder, derivatives, ignoredNormal);
    }

    AcGePoint3d evalPoint(const AcGePoint2d &param, int derivativeOrder,
                          AcGeVector3dArray &derivatives,
                          AcGeVector3d &normal) const
    {
        if (derivativeOrder < 0 || derivativeOrder > 2)
            throw std::invalid_argument(
                "AcGeSurface evalPoint supports derivative orders 0 through 2.");
        derivatives.clear();
        const AcGePoint3d point = pointAt(param.x, param.y);

        const double uSpan = maxParameterU() - minParameterU();
        const double vSpan = maxParameterV() - minParameterV();
        if ((std::isfinite(uSpan) && !(uSpan > 0.0)) ||
            (std::isfinite(vSpan) && !(vSpan > 0.0)))
            throw std::domain_error(
                "AcGeSurface has a degenerate parameter span.");
        double hu = 1.0e-4 * std::max(1.0, std::abs(param.x));
        double hv = 1.0e-4 * std::max(1.0, std::abs(param.y));
        if (std::isfinite(uSpan))
            hu = std::min(hu, uSpan * 0.25);
        if (std::isfinite(vSpan))
            hv = std::min(hv, vSpan * 0.25);
        const auto d1u = [&](double u, double v) {
            if (std::isfinite(minParameterU()) && u - hu < minParameterU())
                return (pointAt(u + hu, v) - pointAt(u, v)) / hu;
            if (std::isfinite(maxParameterU()) && u + hu > maxParameterU())
                return (pointAt(u, v) - pointAt(u - hu, v)) / hu;
            return (pointAt(u + hu, v) - pointAt(u - hu, v)) / (2.0 * hu);
        };
        const auto d1v = [&](double u, double v) {
            if (std::isfinite(minParameterV()) && v - hv < minParameterV())
                return (pointAt(u, v + hv) - pointAt(u, v)) / hv;
            if (std::isfinite(maxParameterV()) && v + hv > maxParameterV())
                return (pointAt(u, v) - pointAt(u, v - hv)) / hv;
            return (pointAt(u, v + hv) - pointAt(u, v - hv)) / (2.0 * hv);
        };
        const auto d2u = [&](double u, double v) {
            if (std::isfinite(minParameterU()) && u - hu < minParameterU())
            {
                const AcGeVector3d first = pointAt(u + hu, v) - pointAt(u, v);
                const AcGeVector3d second =
                    pointAt(u + 2.0 * hu, v) - pointAt(u + hu, v);
                return (second - first) / (hu * hu);
            }
            if (std::isfinite(maxParameterU()) && u + hu > maxParameterU())
            {
                const AcGeVector3d first = pointAt(u, v) - pointAt(u - hu, v);
                const AcGeVector3d second =
                    pointAt(u - hu, v) - pointAt(u - 2.0 * hu, v);
                return (first - second) / (hu * hu);
            }
            return ((pointAt(u + hu, v) - pointAt(u, v)) -
                    (pointAt(u, v) - pointAt(u - hu, v))) /
                   (hu * hu);
        };
        const auto d2v = [&](double u, double v) {
            if (std::isfinite(minParameterV()) && v - hv < minParameterV())
            {
                const AcGeVector3d first = pointAt(u, v + hv) - pointAt(u, v);
                const AcGeVector3d second =
                    pointAt(u, v + 2.0 * hv) - pointAt(u, v + hv);
                return (second - first) / (hv * hv);
            }
            if (std::isfinite(maxParameterV()) && v + hv > maxParameterV())
            {
                const AcGeVector3d first = pointAt(u, v) - pointAt(u, v - hv);
                const AcGeVector3d second =
                    pointAt(u, v - hv) - pointAt(u, v - 2.0 * hv);
                return (first - second) / (hv * hv);
            }
            return ((pointAt(u, v + hv) - pointAt(u, v)) -
                    (pointAt(u, v) - pointAt(u, v - hv))) /
                   (hv * hv);
        };

        const AcGeVector3d du = d1u(param.x, param.y);
        const AcGeVector3d dv = d1v(param.x, param.y);
        if (derivativeOrder > 0)
            derivatives = {du, dv};
        const AcGeVector3d cross = du.crossProduct(dv);
        const double normalLength = cross.length();
        if (normalLength <= 64.0 * std::numeric_limits<double>::epsilon() *
                                du.length() * dv.length())
            throw std::domain_error("AcGeSurface normal is singular.");
        normal = cross / normalLength;
        if (derivativeOrder == 0)
            return point;
        if (derivativeOrder == 2)
        {
            AcGeVector3d duv;
            if (std::isfinite(minParameterV()) &&
                param.y - hv < minParameterV())
                duv = (d1u(param.x, param.y + hv) - du) / hv;
            else if (std::isfinite(maxParameterV()) &&
                     param.y + hv > maxParameterV())
                duv = (du - d1u(param.x, param.y - hv)) / hv;
            else
                duv = (d1u(param.x, param.y + hv) -
                       d1u(param.x, param.y - hv)) /
                      (2.0 * hv);
            derivatives.push_back(d2u(param.x, param.y));
            derivatives.push_back(duv);
            derivatives.push_back(d2v(param.x, param.y));
        }
        return point;
    }

    virtual AcGeVector3d normalAt(double u, double v) const
    {
        AcGeVector3dArray derivatives;
        AcGeVector3d normal;
        (void)evalPoint({u, v}, 1, derivatives, normal);
        return normal;
    }

private:
    bool isClosedAtSeam(bool inU, const AcGeTol &tol) const
    {
        const double lower = inU ? minParameterU() : minParameterV();
        const double upper = inU ? maxParameterU() : maxParameterV();
        if (!std::isfinite(lower) || !std::isfinite(upper) || !(lower < upper))
            return false;

        const double otherLower = inU ? minParameterV() : minParameterU();
        const double otherUpper = inU ? maxParameterV() : maxParameterU();
        std::vector<double> samples;
        if (std::isfinite(otherLower) && std::isfinite(otherUpper))
        {
            if (!(otherLower < otherUpper))
                samples.push_back(otherLower);
            else
            {
                const double span = otherUpper - otherLower;
                samples = {otherLower, otherLower + span * 0.25,
                           otherLower + span * 0.5,
                           otherLower + span * 0.75, otherUpper};
            }
        }
        else if (std::isfinite(otherLower))
            samples = {otherLower, otherLower + 1.0, otherLower + 2.0};
        else if (std::isfinite(otherUpper))
            samples = {otherUpper - 2.0, otherUpper - 1.0, otherUpper};
        else
            samples = {-1.0, 0.0, 1.0};

        for (double sample : samples)
        {
            const AcGePoint3d start =
                inU ? pointAt(lower, sample) : pointAt(sample, lower);
            const AcGePoint3d end =
                inU ? pointAt(upper, sample) : pointAt(sample, upper);
            if (start.distanceTo(end) > tol.equalPoint())
                return false;
        }
        return true;
    }
};
