#pragma once

#include "AcGePort.h"

#include <cstddef>
#include <vector>

#include "AcGeArrays.h"
#include "AcGeInterval.h"

// Source-level AcGeKnotVector aligned with the ObjectARX reference
// (gekvec.h): an ordered, non-decreasing sequence of doubles used by the
// spline entities, plus the documented query/edit API.  Storage is
// std::vector<double>; physical/logical length map to capacity/size and
// growLength is tracked for API compatibility only.

class ACGE_PORT AcGeKnotVector
{
    std::vector<double> m_knots;
    double m_tolerance = 1.0e-10;
    int m_growLength = 8;

    void checkIndex(int i) const;

public:
    AcGeKnotVector() = default;
    explicit AcGeKnotVector(double eps);
    AcGeKnotVector(int size, int growSize, double eps = 1.0e-10);
    AcGeKnotVector(int size, const double *knots, double eps = 1.0e-10);
    AcGeKnotVector(const AcGeDoubleArray &knots, double eps = 1.0e-10);
    AcGeKnotVector(int plusMult, const AcGeKnotVector &src);

    // Uniform knot sequence of `size` knots from start to end.
    AcGeKnotVector &set(int size, double start, double end);
    AcGeKnotVector &set(int size, const double *knots,
                        double eps = 1.0e-10);

    AcGeKnotVector &operator=(const AcGeKnotVector &other) = default;
    AcGeKnotVector &operator=(const AcGeDoubleArray &knots);

    double operator[](int i) const;
    double &operator[](int i);

    AcGeKnotVector &append(double u);
    AcGeKnotVector &append(AcGeKnotVector &tail, double knotRatio = 0.0);
    const double *asArrayPtr() const;
    double *asArrayPtr();
    bool contains(double param) const;
    double startParam() const;
    double endParam() const;
    void getDistinctKnots(AcGeDoubleArray &knots) const;
    int getInterval(int ord, double par, AcGeInterval &interval) const;
    AcGeKnotVector &insert(double u);
    AcGeKnotVector &insertAt(int indx, double u, int multiplicity = 1);
    bool isEmpty() const;
    bool isEqualTo(const AcGeKnotVector &other) const;
    bool isOn(double knot) const;
    int length() const;
    int logicalLength() const;
    int physicalLength() const;
    int multiplicityAt(int i) const;
    int numIntervals() const;
    AcGeKnotVector &removeAt(int i);
    AcGeKnotVector &removeSubVector(int startIndex, int endIndex);
    AcGeKnotVector &reverse();
    AcGeKnotVector &setGrowLength(int growLength);
    AcGeKnotVector &setLogicalLength(int newLength);
    AcGeKnotVector &setPhysicalLength(int newLength);
    AcGeKnotVector &setRange(double lower, double upper);
    AcGeKnotVector &setTolerance(double eps);
    int split(double par, AcGeKnotVector *pKnot1, int multLast,
              AcGeKnotVector *pKnot2, int multFirst) const;
    double tolerance() const;
    int growLength() const;
};
