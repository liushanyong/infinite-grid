#include "acge/AcGeConic.h"

#include <cmath>
#include <stdexcept>

namespace ge
{

AcGeParabola2d::AcGeParabola2d(const AcGePoint2d &parabolaVertex,
                               const AcGeVector2d &axisDirection,
                               double focalParameter)
    : vertex(parabolaVertex), focalLength(std::abs(focalParameter))
{
    setAxis(axisDirection);
    if (focalLength == 0.0)
        throw std::invalid_argument("Parabola focal length must be nonzero.");
}

AcGeParabola2d &AcGeParabola2d::setAxis(const AcGeVector2d &direction)
{
    const AcGeVector2d unit = direction.normal();
    if (unit.isZeroLength())
        throw std::invalid_argument("Parabola axis must be nonzero.");
    axis = unit;
    return *this;
}

AcGeParabola3d::AcGeParabola3d(const AcGePoint3d &parabolaVertex,
                               const AcGeVector3d &axisDirection,
                               const AcGeVector3d &planeNormal,
                               double focalParameter)
    : vertex(parabolaVertex), focalLength(std::abs(focalParameter))
{
    setFrame(axisDirection, planeNormal);
    if (focalLength == 0.0)
        throw std::invalid_argument("Parabola focal length must be nonzero.");
}

AcGeParabola3d &AcGeParabola3d::setFrame(const AcGeVector3d &axisDirection,
                                         const AcGeVector3d &planeNormal)
{
    normal = planeNormal.normal();
    if (normal.isZeroLength())
        throw std::invalid_argument("Parabola plane normal must be nonzero.");
    axis = axisDirection - normal * axisDirection.dotProduct(normal);
    axis.normalize();
    if (axis.isZeroLength())
        throw std::invalid_argument("Parabola axis must lie in its plane.");
    return *this;
}

AcGeHyperbola2d::AcGeHyperbola2d(const AcGePoint2d &hyperbolaCenter,
                                 const AcGeVector2d &majorDirection,
                                 const AcGeVector2d &minorDirection,
                                 double semiMajorRadius, double semiMinorRadius,
                                 int branchSign)
    : center(hyperbolaCenter), majorRadius(std::abs(semiMajorRadius)),
      minorRadius(std::abs(semiMinorRadius)), branch(branchSign < 0 ? -1 : 1)
{
    setAxes(majorDirection, minorDirection);
    if (majorRadius == 0.0 || minorRadius == 0.0)
        throw std::invalid_argument("Hyperbola radii must be nonzero.");
}

AcGeHyperbola2d &AcGeHyperbola2d::setAxes(const AcGeVector2d &majorDirection,
                                          const AcGeVector2d &minorDirection)
{
    majorAxis = majorDirection.normal();
    if (majorAxis.isZeroLength())
        throw std::invalid_argument("Hyperbola major axis must be nonzero.");
    minorAxis = minorDirection -
                majorAxis * minorDirection.dotProduct(majorAxis);
    minorAxis.normalize();
    if (minorAxis.isZeroLength())
        throw std::invalid_argument("Hyperbola axes must not be parallel.");
    return *this;
}

AcGeHyperbola3d::AcGeHyperbola3d(const AcGePoint3d &hyperbolaCenter,
                                 const AcGeVector3d &majorDirection,
                                 const AcGeVector3d &planeNormal,
                                 double semiMajorRadius, double semiMinorRadius,
                                 int branchSign)
    : center(hyperbolaCenter), majorRadius(std::abs(semiMajorRadius)),
      minorRadius(std::abs(semiMinorRadius)), branch(branchSign < 0 ? -1 : 1)
{
    setFrame(majorDirection, planeNormal);
    if (majorRadius == 0.0 || minorRadius == 0.0)
        throw std::invalid_argument("Hyperbola radii must be nonzero.");
}

AcGeHyperbola3d &AcGeHyperbola3d::setFrame(const AcGeVector3d &majorDirection,
                                           const AcGeVector3d &planeNormal)
{
    normal = planeNormal.normal();
    if (normal.isZeroLength())
        throw std::invalid_argument("Hyperbola plane normal must be nonzero.");
    majorAxis =
        majorDirection - normal * majorDirection.dotProduct(normal);
    majorAxis.normalize();
    if (majorAxis.isZeroLength())
        throw std::invalid_argument("Hyperbola major axis must lie in its plane.");
    return *this;
}

} // namespace ge
