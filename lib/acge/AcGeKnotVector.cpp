#include "acge/AcGeKnotVector.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace
{

constexpr int kDefaultGrowLength = 8;

bool finiteValue(double v)
{
    return std::isfinite(v);
}

// Lower bound of the group of knots equal (within eps) to knots[i].
size_t groupStart(const std::vector<double> &knots, size_t i, double eps)
{
    size_t start = i;
    while (start > 0 && std::abs(knots[start - 1] - knots[i]) <= eps)
        --start;
    return start;
}

// Upper bound (exclusive) of the group of knots equal (within eps) to knots[i].
size_t groupEnd(const std::vector<double> &knots, size_t i, double eps)
{
    size_t end = i + 1;
    while (end < knots.size() && std::abs(knots[end] - knots[i]) <= eps)
        ++end;
    return end;
}

} // namespace

void AcGeKnotVector::checkIndex(int i) const
{
    if (i < 0 || static_cast<size_t>(i) >= m_knots.size())
        throw std::out_of_range("AcGeKnotVector index out of range.");
}

AcGeKnotVector::AcGeKnotVector(double eps) : m_tolerance(std::abs(eps))
{
    if (!finiteValue(m_tolerance))
        throw std::invalid_argument("AcGeKnotVector tolerance must be finite.");
}

AcGeKnotVector::AcGeKnotVector(int size, int growSize, double eps)
    : m_tolerance(std::abs(eps)),
      m_growLength(growSize > 0 ? growSize : kDefaultGrowLength)
{
    if (!finiteValue(m_tolerance))
        throw std::invalid_argument("AcGeKnotVector tolerance must be finite.");
    if (size < 0)
        throw std::invalid_argument("AcGeKnotVector size must be nonnegative.");
    m_knots.reserve(static_cast<size_t>(size));
}

AcGeKnotVector::AcGeKnotVector(int size, const double *knots, double eps)
    : AcGeKnotVector(size, kDefaultGrowLength, eps)
{
    if (size > 0 && knots == nullptr)
        throw std::invalid_argument("AcGeKnotVector knot array is null.");
    if (size > 0)
        m_knots.assign(knots, knots + size);
    for (size_t i = 1; i < m_knots.size(); ++i)
    {
        if (!finiteValue(m_knots[i]) ||
            m_knots[i] < m_knots[i - 1] - m_tolerance)
            throw std::invalid_argument(
                "AcGeKnotVector knots must be nondecreasing finite values.");
    }
}

AcGeKnotVector::AcGeKnotVector(const AcGeDoubleArray &knots, double eps)
    : AcGeKnotVector(static_cast<int>(knots.size()),
                     knots.empty() ? nullptr : knots.data(), eps)
{
}

AcGeKnotVector::AcGeKnotVector(int plusMult, const AcGeKnotVector &src)
    : m_tolerance(src.m_tolerance), m_growLength(src.m_growLength)
{
    if (plusMult < 0)
        throw std::invalid_argument(
            "AcGeKnotVector plusMult must be nonnegative.");
    if (plusMult == 0 || src.m_knots.empty())
        return;

    size_t i = 0;
    while (i < src.m_knots.size())
    {
        const size_t end = groupEnd(src.m_knots, i, m_tolerance);
        m_knots.insert(m_knots.end(), static_cast<size_t>(plusMult),
                       src.m_knots[i]);
        m_knots.insert(m_knots.end(),
                       src.m_knots.begin() + static_cast<std::ptrdiff_t>(i),
                       src.m_knots.begin() + static_cast<std::ptrdiff_t>(end));
        i = end;
    }
}
AcGeKnotVector &AcGeKnotVector::set(int size, double start, double end)
{
    if (size < 2 || !finiteValue(start) || !finiteValue(end) ||
        end <= start)
        throw std::invalid_argument(
            "AcGeKnotVector uniform set needs a finite nondegenerate "
            "range.");
    m_knots.resize(static_cast<size_t>(size));
    const double step = (end - start) / static_cast<double>(size - 1);
    for (int i = 0; i < size; ++i)
        m_knots[static_cast<size_t>(i)] =
            start + step * static_cast<double>(i);
    m_knots.back() = end;
    return *this;
}

AcGeKnotVector &AcGeKnotVector::set(int size, const double *knots, double eps)
{
    *this = AcGeKnotVector(size, knots, eps);
    return *this;
}

AcGeKnotVector &AcGeKnotVector::operator=(const AcGeDoubleArray &knots)
{
    *this = AcGeKnotVector(
        static_cast<int>(knots.size()),
        knots.empty() ? nullptr : knots.data(), m_tolerance);
    return *this;
}

double AcGeKnotVector::operator[](int i) const
{
    checkIndex(i);
    return m_knots[static_cast<size_t>(i)];
}

double &AcGeKnotVector::operator[](int i)
{
    checkIndex(i);
    return m_knots[static_cast<size_t>(i)];
}

AcGeKnotVector &AcGeKnotVector::append(double u)
{
    return insert(u);
}

AcGeKnotVector &AcGeKnotVector::append(AcGeKnotVector &tail, double knotRatio)
{
    if (tail.m_knots.empty())
        return *this;
    if (m_knots.empty())
    {
        m_knots = tail.m_knots;
        return *this;
    }

    double tailStart = tail.m_knots.front();
    double tailEnd = tail.m_knots.back();
    const double tailSpan = tailEnd - tailStart;
    if (tailSpan <= 0.0)
        throw std::invalid_argument(
            "AcGeKnotVector::append tail must have positive extent.");

    double headStart = m_knots.front();
    double headEnd = m_knots.back();
    const double headSpan = headEnd - headStart;

    // knotRatio = length(this) / length(tail) after scaling; zero keeps the
    // tail parameterization as-is.
    double a = 1.0;
    double b = headEnd - tailStart;
    if (knotRatio != 0.0)
    {
        if (knotRatio < 0.0 || !finiteValue(knotRatio))
            throw std::invalid_argument(
                "AcGeKnotVector::append knotRatio must be nonnegative.");
        a = headSpan / (knotRatio * tailSpan);
        if (!finiteValue(a) || a <= 0.0)
            throw std::invalid_argument(
                "AcGeKnotVector::append knotRatio is not representable.");
        b = headEnd - a * tailStart;
        for (double &k : tail.m_knots)
            k = a * k + b;
    }
    else
    {
        for (double &k : tail.m_knots)
            k = k + b;
    }
    m_knots.insert(m_knots.end(), tail.m_knots.begin(), tail.m_knots.end());
    return *this;
}

const double *AcGeKnotVector::asArrayPtr() const
{
    return m_knots.data();
}

double *AcGeKnotVector::asArrayPtr()
{
    return m_knots.data();
}

bool AcGeKnotVector::contains(double param) const
{
    return !m_knots.empty() && param >= m_knots.front() &&
           param <= m_knots.back();
}

double AcGeKnotVector::startParam() const
{
    if (m_knots.empty())
        throw std::logic_error("AcGeKnotVector is empty.");
    return m_knots.front();
}

double AcGeKnotVector::endParam() const
{
    if (m_knots.empty())
        throw std::logic_error("AcGeKnotVector is empty.");
    return m_knots.back();
}

void AcGeKnotVector::getDistinctKnots(AcGeDoubleArray &knots) const
{
    knots.clear();
    for (size_t i = 0; i < m_knots.size();)
    {
        knots.push_back(m_knots[i]);
        i = groupEnd(m_knots, i, m_tolerance);
    }
}

int AcGeKnotVector::getInterval(int ord, double par,
                                AcGeInterval &interval) const
{
    if (ord < 1)
        throw std::invalid_argument("AcGeKnotVector order must be positive.");
    const int count = static_cast<int>(m_knots.size());
    if (count - ord < 1)
        throw std::invalid_argument(
            "AcGeKnotVector has too few knots for the spline order.");

    // Largest knot index i in [ord-1, count-ord-1] with knots[i] <= par.
    int low = ord - 1;
    int high = count - ord - 1;
    if (par < m_knots[static_cast<size_t>(low)] - m_tolerance ||
        par > m_knots[static_cast<size_t>(high + 1)] + m_tolerance)
        throw std::out_of_range(
            "AcGeKnotVector parameter is outside the spline domain.");

    while (low < high)
    {
        const int mid = low + (high - low + 1) / 2;
        if (m_knots[static_cast<size_t>(mid)] <= par + m_tolerance)
            low = mid;
        else
            high = mid - 1;
    }
    interval.set(m_knots[static_cast<size_t>(low)],
                 m_knots[static_cast<size_t>(low + 1)]);
    return low;
}

AcGeKnotVector &AcGeKnotVector::insert(double u)
{
    if (!finiteValue(u))
        throw std::invalid_argument("AcGeKnotVector knot must be finite.");
    // Insert after existing equal values so near-duplicates (within eps)
    // keep growing the multiplicity of the same knot.
    const auto pos = std::upper_bound(m_knots.begin(), m_knots.end(), u);
    m_knots.insert(pos, u);
    return *this;
}

AcGeKnotVector &AcGeKnotVector::insertAt(int indx, double u, int multiplicity)
{
    if (!finiteValue(u))
        throw std::invalid_argument("AcGeKnotVector knot must be finite.");
    if (multiplicity < 1)
        throw std::invalid_argument(
            "AcGeKnotVector multiplicity must be positive.");
    checkIndex(indx);
    m_knots.insert(m_knots.begin() + indx,
                   static_cast<size_t>(multiplicity), u);
    return *this;
}

bool AcGeKnotVector::isEmpty() const
{
    return m_knots.empty();
}

bool AcGeKnotVector::isEqualTo(const AcGeKnotVector &other) const
{
    if (m_knots.size() != other.m_knots.size())
        return false;
    const double eps = std::max(m_tolerance, other.m_tolerance);
    for (size_t i = 0; i < m_knots.size(); ++i)
    {
        if (std::abs(m_knots[i] - other.m_knots[i]) > eps)
            return false;
    }
    return true;
}

bool AcGeKnotVector::isOn(double knot) const
{
    if (m_knots.empty())
        return false;
    const auto pos = std::lower_bound(
        m_knots.begin(), m_knots.end(), knot - m_tolerance);
    return pos != m_knots.end() && *pos <= knot + m_tolerance;
}

int AcGeKnotVector::length() const
{
    return static_cast<int>(m_knots.size());
}

int AcGeKnotVector::logicalLength() const
{
    return length();
}

int AcGeKnotVector::physicalLength() const
{
    return static_cast<int>(m_knots.capacity());
}

int AcGeKnotVector::multiplicityAt(int i) const
{
    checkIndex(i);
    const size_t idx = static_cast<size_t>(i);
    const size_t start = groupStart(m_knots, idx, m_tolerance);
    const size_t end = groupEnd(m_knots, idx, m_tolerance);
    return static_cast<int>(end - start);
}

int AcGeKnotVector::numIntervals() const
{
    int count = 0;
    for (size_t i = 1; i < m_knots.size(); ++i)
    {
        if (m_knots[i] - m_knots[i - 1] > m_tolerance)
            ++count;
    }
    return count;
}

AcGeKnotVector &AcGeKnotVector::removeAt(int i)
{
    checkIndex(i);
    m_knots.erase(m_knots.begin() + i);
    return *this;
}

AcGeKnotVector &AcGeKnotVector::removeSubVector(int startIndex, int endIndex)
{
    checkIndex(startIndex);
    checkIndex(endIndex);
    if (startIndex > endIndex)
        throw std::invalid_argument(
            "AcGeKnotVector subvector start exceeds end.");
    m_knots.erase(m_knots.begin() + startIndex,
                  m_knots.begin() + endIndex + 1);
    return *this;
}

AcGeKnotVector &AcGeKnotVector::reverse()
{
    if (m_knots.empty())
        return *this;
    const double lo = m_knots.front();
    const double hi = m_knots.back();
    for (double &k : m_knots)
        k = lo + hi - k;
    std::reverse(m_knots.begin(), m_knots.end());
    return *this;
}

AcGeKnotVector &AcGeKnotVector::setGrowLength(int growLength)
{
    if (growLength <= 0)
        throw std::invalid_argument(
            "AcGeKnotVector grow length must be positive.");
    m_growLength = growLength;
    return *this;
}

AcGeKnotVector &AcGeKnotVector::setLogicalLength(int newLength)
{
    if (newLength < 0)
        throw std::invalid_argument(
            "AcGeKnotVector length must be nonnegative.");
    m_knots.resize(static_cast<size_t>(newLength));
    return *this;
}

AcGeKnotVector &AcGeKnotVector::setPhysicalLength(int newLength)
{
    if (newLength < 0)
        throw std::invalid_argument(
            "AcGeKnotVector length must be nonnegative.");
    m_knots.reserve(static_cast<size_t>(newLength));
    return *this;
}

AcGeKnotVector &AcGeKnotVector::setRange(double lower, double upper)
{
    if (m_knots.empty())
        throw std::logic_error("AcGeKnotVector is empty.");
    if (!finiteValue(lower) || !finiteValue(upper) || upper <= lower)
        throw std::invalid_argument(
            "AcGeKnotVector::setRange needs a finite nondegenerate range.");
    const double lo = m_knots.front();
    const double hi = m_knots.back();
    if (hi - lo <= m_tolerance)
        throw std::invalid_argument(
            "AcGeKnotVector::setRange needs a nondegenerate knot span.");
    const double a = (upper - lower) / (hi - lo);
    const double b = upper - a * hi;
    for (double &k : m_knots)
        k = a * k + b;
    return *this;
}

AcGeKnotVector &AcGeKnotVector::setTolerance(double eps)
{
    if (!finiteValue(eps))
        throw std::invalid_argument("AcGeKnotVector tolerance must be finite.");
    m_tolerance = std::abs(eps);
    return *this;
}

int AcGeKnotVector::split(double par, AcGeKnotVector *pKnot1, int multLast,
                          AcGeKnotVector *pKnot2, int multFirst) const
{
    if (pKnot1 == nullptr || pKnot2 == nullptr)
        throw std::invalid_argument("AcGeKnotVector::split outputs are null.");
    if (!finiteValue(par))
        throw std::invalid_argument("AcGeKnotVector split parameter must be finite.");
    if (multLast < 1 || multFirst < 1)
        throw std::invalid_argument(
            "AcGeKnotVector::split multiplicities must be positive.");
    if (m_knots.empty() || par < m_knots.front() - m_tolerance ||
        par > m_knots.back() + m_tolerance)
        throw std::out_of_range(
            "AcGeKnotVector::split parameter is outside the knot range.");

    auto equalAt = [&](size_t i) {
        return std::abs(m_knots[i] - par) <= m_tolerance;
    };

    pKnot1->m_knots.clear();
    pKnot2->m_knots.clear();
    pKnot1->m_tolerance = m_tolerance;
    pKnot2->m_tolerance = m_tolerance;

    // Head: knots strictly below par, then the split parameter grown or
    // truncated to exactly multLast entries (inserted or deleted knots).
    size_t i = 0;
    while (i < m_knots.size() && m_knots[i] < par - m_tolerance)
        pKnot1->m_knots.push_back(m_knots[i++]);
    pKnot1->m_knots.insert(pKnot1->m_knots.end(), static_cast<size_t>(multLast), par);

    // Source knots equal to par are absorbed by the split knots.
    while (i < m_knots.size() && equalAt(i))
        ++i;

    // Tail: the split parameter with multFirst, then the remaining knots.
    pKnot2->m_knots.assign(static_cast<size_t>(multFirst), par);
    while (i < m_knots.size())
        pKnot2->m_knots.push_back(m_knots[i++]);

    return static_cast<int>(pKnot1->m_knots.size());
}

double AcGeKnotVector::tolerance() const
{
    return m_tolerance;
}

int AcGeKnotVector::growLength() const
{
    return m_growLength;
}
