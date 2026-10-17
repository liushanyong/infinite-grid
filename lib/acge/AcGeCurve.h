#pragma once

#include "AcGePort.h"

#include "AcGePoint.h"

class ACGE_PORT AcGeCurve2d
{
public:
    virtual ~AcGeCurve2d() = default;

    virtual double startParam() const = 0;
    virtual double endParam() const = 0;
    virtual AcGePoint2d pointAt(double param) const = 0;
};

class ACGE_PORT AcGeCurve3d
{
public:
    virtual ~AcGeCurve3d() = default;

    virtual double startParam() const = 0;
    virtual double endParam() const = 0;
    virtual AcGePoint3d pointAt(double param) const = 0;
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
};
