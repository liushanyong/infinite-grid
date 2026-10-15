#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

// Source-level AcGeInterval subset for bounded/unbounded parameter domains.
class AcGeInterval
{
    double m_lower = 0.0;
    double m_upper = 0.0;
    double m_tolerance = 1.0e-12;
    bool m_boundedBelow = false;
    bool m_boundedAbove = false;

    void validateBounds() const
    {
        if (m_boundedBelow && m_boundedAbove && m_lower > m_upper)
            throw std::invalid_argument("AcGeInterval lower bound exceeds upper bound.");
    }

public:
    explicit AcGeInterval(double tolerance = 1.0e-12)
        : m_tolerance(std::abs(tolerance))
    {
    }

    AcGeInterval(double lower, double upper, double tolerance = 1.0e-12)
        : m_lower(lower), m_upper(upper), m_tolerance(std::abs(tolerance)),
          m_boundedBelow(true), m_boundedAbove(true)
    {
        validateBounds();
    }

    AcGeInterval(bool boundedBelow, double bound,
                 double tolerance = 1.0e-12)
        : m_lower(bound), m_upper(bound), m_tolerance(std::abs(tolerance)),
          m_boundedBelow(boundedBelow), m_boundedAbove(true)
    {
    }

    AcGeInterval &set(double lower, double upper)
    {
        m_lower = lower;
        m_upper = upper;
        m_boundedBelow = true;
        m_boundedAbove = true;
        validateBounds();
        return *this;
    }

    AcGeInterval &set()
    {
        m_lower = 0.0;
        m_upper = 0.0;
        m_boundedBelow = false;
        m_boundedAbove = false;
        return *this;
    }

    AcGeInterval &set(bool boundedBelow, double bound)
    {
        m_upper = bound;
        m_boundedAbove = true;
        m_boundedBelow = boundedBelow;
        m_lower = bound;
        return *this;
    }

    AcGeInterval &setLower(double lower)
    {
        if (m_boundedAbove && lower > m_upper)
            throw std::invalid_argument("AcGeInterval lower bound exceeds upper bound.");
        m_lower = lower;
        m_boundedBelow = true;
        return *this;
    }

    AcGeInterval &setUpper(double upper)
    {
        if (m_boundedBelow && m_lower > upper)
            throw std::invalid_argument("AcGeInterval lower bound exceeds upper bound.");
        m_upper = upper;
        m_boundedAbove = true;
        return *this;
    }

    AcGeInterval &setTolerance(double tolerance)
    {
        m_tolerance = std::abs(tolerance);
        return *this;
    }

    bool contains(double value) const
    {
        return (!m_boundedBelow || value >= m_lower - m_tolerance) &&
               (!m_boundedAbove || value <= m_upper + m_tolerance);
    }

    bool contains(const AcGeInterval &other) const
    {
        const bool containsLower =
            !other.m_boundedBelow ||
            (m_boundedBelow && m_lower <= other.m_lower + m_tolerance);
        const bool containsUpper =
            !other.m_boundedAbove ||
            (m_boundedAbove && m_upper >= other.m_upper - m_tolerance);
        return containsLower && containsUpper;
    }

    bool intersectWith(const AcGeInterval &other, AcGeInterval &result) const
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

    bool isDisjoint(const AcGeInterval &other) const
    {
        AcGeInterval ignored;
        return !intersectWith(other, ignored);
    }

    void getMerge(const AcGeInterval &other, AcGeInterval &result) const
    {
        result.m_tolerance = std::max(m_tolerance, other.m_tolerance);
        result.m_boundedBelow = m_boundedBelow && other.m_boundedBelow;
        result.m_boundedAbove = m_boundedAbove && other.m_boundedAbove;
        if (result.m_boundedBelow)
            result.m_lower = std::min(m_lower, other.m_lower);
        if (result.m_boundedAbove)
            result.m_upper = std::max(m_upper, other.m_upper);
    }

    double lowerBound() const { return m_lower; }
    double upperBound() const { return m_upper; }
    double tolerance() const { return m_tolerance; }
    double length() const
    {
        return isBounded() ? m_upper - m_lower : -1.0;
    }
    double element() const
    {
        if (m_boundedAbove)
            return m_upper;
        return m_boundedBelow ? m_lower : 0.0;
    }
    void getBounds(double &lower, double &upper) const
    {
        lower = m_lower;
        upper = m_upper;
    }

    bool isBounded() const { return m_boundedBelow && m_boundedAbove; }
    bool isBoundedBelow() const { return m_boundedBelow; }
    bool isBoundedAbove() const { return m_boundedAbove; }
    bool isUnBounded() const { return !isBounded(); }
    bool isSingleton() const
    {
        return isBounded() && std::abs(m_upper - m_lower) <= m_tolerance;
    }
    bool isEqualAtLower(double value) const
    {
        return m_boundedBelow && std::abs(m_lower - value) <= m_tolerance;
    }
    bool isEqualAtUpper(double value) const
    {
        return m_boundedAbove && std::abs(m_upper - value) <= m_tolerance;
    }
    bool isEqualAtLower(const AcGeInterval &other) const
    {
        return m_boundedBelow && other.m_boundedBelow &&
               std::abs(m_lower - other.m_lower) <= m_tolerance;
    }
    bool isEqualAtUpper(const AcGeInterval &other) const
    {
        return m_boundedAbove && other.m_boundedAbove &&
               std::abs(m_upper - other.m_upper) <= m_tolerance;
    }
    bool isContinuousAtUpper(const AcGeInterval &other) const
    {
        return m_boundedAbove && other.m_boundedBelow &&
               std::abs(m_upper - other.m_lower) <= m_tolerance;
    }

    bool operator==(const AcGeInterval &other) const
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
    bool operator!=(const AcGeInterval &other) const { return !(*this == other); }
};
