#include "acge/AcGeInterval.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

void AcGeInterval::validateBounds() const
{
    if (m_boundedBelow && m_boundedAbove && m_lower > m_upper)
        throw std::invalid_argument("AcGeInterval lower bound exceeds upper bound.");
}

AcGeInterval::AcGeInterval(double lower, double upper, double tolerance)
    : m_lower(lower), m_upper(upper), m_tolerance(std::abs(tolerance)),
      m_boundedBelow(true), m_boundedAbove(true)
{
    validateBounds();
}

AcGeInterval &AcGeInterval::set(double lower, double upper)
{
    m_lower = lower;
    m_upper = upper;
    m_boundedBelow = true;
    m_boundedAbove = true;
    validateBounds();
    return *this;
}

AcGeInterval &AcGeInterval::setLower(double lower)
{
    if (m_boundedAbove && lower > m_upper)
        throw std::invalid_argument("AcGeInterval lower bound exceeds upper bound.");
    m_lower = lower;
    m_boundedBelow = true;
    return *this;
}

AcGeInterval &AcGeInterval::setUpper(double upper)
{
    if (m_boundedBelow && m_lower > upper)
        throw std::invalid_argument("AcGeInterval lower bound exceeds upper bound.");
    m_upper = upper;
    m_boundedAbove = true;
    return *this;
}

bool AcGeInterval::contains(const AcGeInterval &other) const
{
    const bool containsLower =
        !other.m_boundedBelow ||
        (m_boundedBelow && m_lower <= other.m_lower + m_tolerance);
    const bool containsUpper =
        !other.m_boundedAbove ||
        (m_boundedAbove && m_upper >= other.m_upper - m_tolerance);
    return containsLower && containsUpper;
}

bool AcGeInterval::intersectWith(const AcGeInterval &other,
                                 AcGeInterval &result) const
{
    result.m_tolerance = std::max(m_tolerance, other.m_tolerance);
    result.m_boundedBelow = m_boundedBelow || other.m_boundedBelow;
    result.m_boundedAbove = m_boundedAbove || other.m_boundedAbove;
    if (m_boundedBelow && other.m_boundedBelow)
        result.m_lower = std::max(m_lower, other.m_lower);
    else if (m_boundedBelow)
        result.m_lower = m_lower;
    else if (other.m_boundedBelow)
        result.m_lower = other.m_lower;

    if (m_boundedAbove && other.m_boundedAbove)
        result.m_upper = std::min(m_upper, other.m_upper);
    else if (m_boundedAbove)
        result.m_upper = m_upper;
    else if (other.m_boundedAbove)
        result.m_upper = other.m_upper;

    if (result.m_boundedBelow && result.m_boundedAbove &&
        result.m_lower > result.m_upper + result.m_tolerance)
        return false;
    if (result.m_boundedBelow && result.m_boundedAbove &&
        result.m_lower > result.m_upper)
    {
        const double touch = (result.m_lower + result.m_upper) * 0.5;
        result.m_lower = touch;
        result.m_upper = touch;
    }
    return true;
}

void AcGeInterval::getMerge(const AcGeInterval &other,
                            AcGeInterval &result) const
{
    result.m_tolerance = std::max(m_tolerance, other.m_tolerance);
    result.m_boundedBelow = m_boundedBelow && other.m_boundedBelow;
    result.m_boundedAbove = m_boundedAbove && other.m_boundedAbove;
    if (result.m_boundedBelow)
        result.m_lower = std::min(m_lower, other.m_lower);
    if (result.m_boundedAbove)
        result.m_upper = std::max(m_upper, other.m_upper);
}

bool AcGeInterval::operator==(const AcGeInterval &other) const
{
    return m_boundedBelow == other.m_boundedBelow &&
           m_boundedAbove == other.m_boundedAbove &&
           (!m_boundedBelow ||
            std::abs(m_lower - other.m_lower) <=
                std::max(m_tolerance, other.m_tolerance)) &&
           (!m_boundedAbove ||
            std::abs(m_upper - other.m_upper) <=
                std::max(m_tolerance, other.m_tolerance));
}
