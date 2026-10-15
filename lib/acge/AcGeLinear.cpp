#include "acge/AcGeLinear.h"

#include <cmath>

bool AcGeLine2d::isOn(const AcGePoint2d &point, double &parameter,
                      const AcGeTol &tol) const
{
    const double candidate = parameterOf(point);
    if (distanceTo(point) > std::abs(tol.equalPoint()))
        return false;
    parameter = candidate;
    return true;
}

bool AcGeLine3d::isOn(const AcGePoint3d &point, double &parameter,
                      const AcGeTol &tol) const
{
    const double candidate = parameterOf(point);
    if (distanceTo(point) > std::abs(tol.equalPoint()))
        return false;
    parameter = candidate;
    return true;
}

bool AcGeRay2d::isOn(const AcGePoint2d &point, double &parameter,
                     const AcGeTol &tol) const
{
    const double candidate = parameterOf(point);
    const double pointTolerance = std::abs(tol.equalPoint());
    const double parameterTolerance =
        pointTolerance / std::max(direction.length(), pointTolerance);
    if (candidate < -parameterTolerance ||
        distanceTo(point) > pointTolerance)
        return false;
    parameter = std::max(0.0, candidate);
    return true;
}

bool AcGeRay3d::isOn(const AcGePoint3d &point, double &parameter,
                     const AcGeTol &tol) const
{
    const double candidate = parameterOf(point);
    const double pointTolerance = std::abs(tol.equalPoint());
    const double parameterTolerance =
        pointTolerance / std::max(direction.length(), pointTolerance);
    if (candidate < -parameterTolerance ||
        distanceTo(point) > pointTolerance)
        return false;
    parameter = std::max(0.0, candidate);
    return true;
}
