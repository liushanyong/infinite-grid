#pragma once

#include "AcGePort.h"

#include <algorithm>
#include <cmath>
#include <concepts>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

#include "AcGeInterval.h"
#include "AcGePoint.h"
#include "AcGePointOnCurve.h"
#include "AcGeTol.h"

// Source-level AcGeCurveCurveInt2d/3d aligned with the ObjectARX reference
// (gecint2d.h / gecint3d.h): intersection-set objects for two curves.
//
// ARX builds these from the AcGeCurve2d/3d hierarchy; this port type-erases
// any two curves providing pointAt(double) (or pointAtAngle(double)) -- the
// same protocol as the point-on-curve wrappers -- and computes:
//   * transversal intersection points via control-polygon overlap
//     subdivision seeding plus Gauss-Newton refinement of the squared
//     distance (NURBS Book section 7.1 idea), and
//   * overlapping (coincident) parameter ranges via seed coincidence
//     grouping with bisection-refined endpoints.
//
// Results expose the documented query API: numIntPoints, intPoint,
// intPointTol, getIntParams, orderWrt1/orderWrt2, isTangential,
// isTransversal, overlapCount, getOverlapRanges, overlapDirection, and
// tolerance. Identical curves (infinite overlap) and the ARX configuration
// queries (getIntConfigs/getIntRanges) are out of scope.

namespace ge
{
namespace detail
{

// Dimension-agnostic curve-curve intersection engine.
template <class PointT, class VectorT>
class CurveCurveIntersector
{
public:
    struct IntersectionRecord
    {
        double param1 = 0.0;
        double param2 = 0.0;
        PointT point{};
        double tolerance = 0.0;
        bool tangential = false;
    };

    struct OverlapRecord
    {
        double start1 = 0.0;
        double end1 = 0.0;
        double start2 = 0.0;
        double end2 = 0.0;
        int direction = 1; // +1 same direction, -1 opposite.
    };

    using Core = PointOnCurveCore<PointT, VectorT>;

    struct Seed
    {
        double t1 = 0.0;
        double s = 0.0;
        bool coincident = false;
    };

    CurveCurveIntersector() = default;
    CurveCurveIntersector(const Core &curve1, const Core &curve2,
                          double low1, double high1, double low2,
                          double high2, double tolerance)
        : m_curve1(curve1), m_curve2(curve2), m_low1(low1), m_high1(high1),
          m_low2(low2), m_high2(high2), m_tolerance(tolerance)
    {
        validateRanges();
        compute();
    }

    const std::vector<IntersectionRecord> &intersections() const
    {
        return m_intersections;
    }
    const std::vector<OverlapRecord> &overlaps() const { return m_overlaps; }
    double tolerance() const { return m_tolerance; }
    // 0-based rank of each intersection along curve1 / curve2.
    const std::vector<std::size_t> &order1() const { return m_order1; }
    const std::vector<std::size_t> &order2() const { return m_order2; }

private:
    static constexpr int kSegments = 64;
    static constexpr int kSeeds = 65;

    void validateRanges() const
    {
        if (!(m_low1 < m_high1) || !(m_low2 < m_high2) ||
            !std::isfinite(m_low1) || !std::isfinite(m_high1) ||
            !std::isfinite(m_low2) || !std::isfinite(m_high2))
            throw std::invalid_argument(
                "CurveCurveInt requires finite increasing parameter "
                "ranges.");
        if (!(m_tolerance > 0.0) || !std::isfinite(m_tolerance))
            throw std::invalid_argument(
                "CurveCurveInt requires a positive tolerance.");
    }

    static VectorT derivative1(const Core &curve, double u)
    {
        if (curve.firstDeriv)
            return curve.firstDeriv(u);
        const double h = Core::step1(u);
        return (curve.point(u + h) - curve.point(u - h)) * (0.5 / h);
    }

    struct Segment
    {
        double low = 0.0;
        double high = 0.0;
        double minX = 0.0;
        double minY = 0.0;
        double minZ = 0.0;
        double maxX = 0.0;
        double maxY = 0.0;
        double maxZ = 0.0;
    };

    static void expand(Segment &box, const PointT &point)
    {
        box.minX = std::min(box.minX, point.x);
        box.minY = std::min(box.minY, point.y);
        box.maxX = std::max(box.maxX, point.x);
        box.maxY = std::max(box.maxY, point.y);
        if constexpr (requires { point.z; })
        {
            box.minZ = std::min(box.minZ, point.z);
            box.maxZ = std::max(box.maxZ, point.z);
        }
    }

    static std::vector<Segment> segmentCurves(const Core &curve, double low,
                                              double high, double tolerance)
    {
        std::vector<Segment> segments(kSegments);
        const double step = (high - low) / kSegments;
        for (int i = 0; i < kSegments; ++i)
        {
            Segment &box = segments[i];
            box.low = low + step * i;
            box.high = low + step * (i + 1);
            box.minX = box.minY = box.minZ =
                std::numeric_limits<double>::max();
            box.maxX = box.maxY = box.maxZ =
                -std::numeric_limits<double>::max();
            // Four samples per segment keep curved chords inside the box
            // well below the intersection tolerance.
            for (int k = 0; k <= 3; ++k)
                expand(box,
                       curve.point(box.low + (box.high - box.low) * k / 3.0));
            box.minX -= tolerance;
            box.minY -= tolerance;
            box.minZ -= tolerance;
            box.maxX += tolerance;
            box.maxY += tolerance;
            box.maxZ += tolerance;
        }
        return segments;
    }

    static bool boxesOverlap(const Segment &a, const Segment &b)
    {
        bool result = a.minX <= b.maxX && a.maxX >= b.minX &&
                      a.minY <= b.maxY && a.maxY >= b.minY;
        // 2D segments leave the z bounds at their sentinel values; only
        // test them for 3D points.
        if constexpr (requires { PointT{}.z; })
            result = result && a.minZ <= b.maxZ && a.maxZ >= b.minZ;
        return result;
    }
    bool refinePair(double &t1, double &t2) const
    {
        for (int iteration = 0; iteration < 30; ++iteration)
        {
            const VectorT d = m_curve1.point(t1) - m_curve2.point(t2);
            if (d.length() <= m_tolerance)
                return true;
            const VectorT a1 = derivative1(m_curve1, t1);
            const VectorT b1 = derivative1(m_curve2, t2);
            const double k11 = a1.dotProduct(a1);
            const double k12 = -a1.dotProduct(b1);
            const double k22 = b1.dotProduct(b1);
            const double determinant = k11 * k22 - k12 * k12;
            if (!(std::abs(determinant) > 1.0e-30))
            {
                // Tangential contact: the 2x2 system is singular because
                // the tangents are parallel. Alternate projection onto each
                // curve, which stays well-posed and converges to the exact
                // tangent point from either side.
                for (int iteration2 = 0; iteration2 < 12; ++iteration2)
                {
                    VectorT d1 = m_curve1.point(t1) - m_curve2.point(t2);
                    VectorT b1 = derivative1(m_curve2, t2);
                    const double bb = b1.dotProduct(b1);
                    if (bb > 0.0)
                    {
                        const double step = d1.dotProduct(b1) / bb;
                        if (!std::isfinite(step))
                            break;
                        t2 = std::clamp(t2 + step, m_low2, m_high2);
                    }
                    VectorT a1 = derivative1(m_curve1, t1);
                    const double aa = a1.dotProduct(a1);
                    if (aa > 0.0)
                    {
                        const double step = d1.dotProduct(a1) / aa;
                        if (!std::isfinite(step))
                            break;
                        t1 = std::clamp(t1 - step, m_low1, m_high1);
                    }
                }
                return (m_curve1.point(t1) - m_curve2.point(t2)).length() <=
                       m_tolerance;
            }
            const double g1 = a1.dotProduct(d);
            const double g2 = -b1.dotProduct(d);
            const double delta1 = (-k22 * g1 + k12 * g2) / determinant;
            const double delta2 = (k12 * g1 - k11 * g2) / determinant;
            if (!std::isfinite(delta1) || !std::isfinite(delta2))
                return false;
            t1 = std::clamp(t1 + delta1, m_low1, m_high1);
            t2 = std::clamp(t2 + delta2, m_low2, m_high2);
            if (std::abs(delta1) <= 1.0e-12 * std::max(1.0, std::abs(t1)) &&
                std::abs(delta2) <= 1.0e-12 * std::max(1.0, std::abs(t2)))
                break;
        }
        return (m_curve1.point(t1) - m_curve2.point(t2)).length() <=
               m_tolerance;
    }

    static double crossMagnitudeOf(const VectorT &a, const VectorT &b)
    {
        if constexpr (requires { a.crossProduct(b).length(); })
            return a.crossProduct(b).length();
        else
            return std::abs(a.x * b.y - a.y * b.x);
    }

    // Alternating projection polish. A no-op at an exact transversal
    // intersection (each projection returns the same point). For
    // near-parallel tangents the 2x2 Newton system is singular everywhere,
    // so the tangent point is located by a 1-D ternary search minimizing
    // the distance to curve2, which converges to the exact contact point
    // and makes both adjacent segment seeds agree.
    void polishPair(double &t1, double &t2) const
    {
        double previousDistance =
            (m_curve1.point(t1) - m_curve2.point(t2)).length();
        for (int iteration = 0; iteration < 12; ++iteration)
        {
            VectorT d = m_curve1.point(t1) - m_curve2.point(t2);
            VectorT b = derivative1(m_curve2, t2);
            const double bb = b.dotProduct(b);
            if (bb > 0.0)
            {
                const double step = d.dotProduct(b) / bb;
                if (std::isfinite(step))
                    t2 = std::clamp(t2 + step, m_low2, m_high2);
            }
            d = m_curve1.point(t1) - m_curve2.point(t2);
            VectorT a = derivative1(m_curve1, t1);
            const double aa = a.dotProduct(a);
            if (aa > 0.0)
            {
                const double step = d.dotProduct(a) / aa;
                if (std::isfinite(step))
                    t1 = std::clamp(t1 - step, m_low1, m_high1);
            }
            const double newDistance =
                (m_curve1.point(t1) - m_curve2.point(t2)).length();
            if (newDistance >= previousDistance)
                break;
            previousDistance = newDistance;
        }

        const VectorT a = derivative1(m_curve1, t1);
        const VectorT b = derivative1(m_curve2, t2);
        const double scale = a.length() * b.length();
        if (scale > 0.0 &&
            crossMagnitudeOf(a, b) <= 1.0e-3 * scale)
        {
            const double window =
                4.0 * std::sqrt(2.0 * m_tolerance) /
                std::max(a.length(), 1.0e-30);
            double lo = std::max(m_low1, t1 - window);
            double hi = std::min(m_high1, t1 + window);
            for (int iteration = 0; iteration < 100; ++iteration)
            {
                const double middle1 = lo + (hi - lo) / 3.0;
                const double middle2 = hi - (hi - lo) / 3.0;
                double s1 = t2;
                double s2 = t2;
                const double distance1 = closestOnCurve2(middle1, s1);
                const double distance2 = closestOnCurve2(middle2, s2);
                if (distance1 < distance2)
                {
                    hi = middle2;
                    t2 = s1;
                }
                else
                {
                    lo = middle1;
                    t2 = s2;
                }
            }
            t1 = 0.5 * (lo + hi);
        }
    }

    // Distance from C1(t1) to the closest point of C2, refining s.
    double closestOnCurve2(double t1, double &s) const
    {
        for (int iteration = 0; iteration < 20; ++iteration)
        {
            const VectorT d = m_curve1.point(t1) - m_curve2.point(s);
            const VectorT b = derivative1(m_curve2, s);
            const double bb = b.dotProduct(b);
            if (!(bb > 0.0))
                break;
            const double delta = d.dotProduct(b) / bb;
            if (!std::isfinite(delta))
                break;
            s = std::clamp(s + delta, m_low2, m_high2);
            if (std::abs(delta) <= 1.0e-12 * std::max(1.0, std::abs(s)))
                break;
        }
        return (m_curve1.point(t1) - m_curve2.point(s)).length();
    }

    bool coincidentAt(double t1, double &sGuess) const
    {
        double s = sGuess;
        const double distance = closestOnCurve2(t1, s);
        if (distance <= m_tolerance)
        {
            sGuess = s;
            return true;
        }
        return false;
    }

    static bool isTangentialAt(const Core &curve1, double t1,
                               const Core &curve2, double t2)
    {
        const VectorT a1 = derivative1(curve1, t1);
        const VectorT b1 = derivative1(curve2, t2);
        const double scale = a1.length() * b1.length();
        if (scale == 0.0)
            return true;
        const double crossMagnitude = crossMagnitudeOf(a1, b1);
        // Near-tangential Newton residuals leave a small angular
        // uncertainty; 1e-4 rad keeps genuine tangencies classified as
        // tangential without misclassifying ordinary crossings.
        return crossMagnitude <= 1.0e-4 * scale;
    }

    void compute()
    {
        const double span1 = m_high1 - m_low1;
        const double span2 = m_high2 - m_low2;

        const std::vector<Segment> segments1 =
            segmentCurves(m_curve1, m_low1, m_high1, m_tolerance);
        const std::vector<Segment> segments2 =
            segmentCurves(m_curve2, m_low2, m_high2, m_tolerance);

        // Seed + refine transversal intersections.
        for (const Segment &a : segments1)
        {
            for (const Segment &b : segments2)
            {
                if (!boxesOverlap(a, b))
                    continue;
                double t1 = 0.5 * (a.low + a.high);
                double t2 = 0.5 * (b.low + b.high);
                if (refinePair(t1, t2))
                {
                    polishPair(t1, t2);
                    IntersectionRecord record;
                    record.param1 = t1;
                    record.param2 = t2;
                    record.point = m_curve1.point(t1);
                    record.tolerance = m_tolerance;
                    record.tangential =
                        isTangentialAt(m_curve1, t1, m_curve2, t2);
                    m_intersections.push_back(record);
                }
            }
        }

        detectOverlaps();
        filterOverlapPoints(span1, span2);
        deduplicate(span1, span2);
        sortAndRank();
    }

    void detectOverlaps()
    {
        // Sample curve1, project each sample onto curve2, and group
        // consecutive coincident samples into overlap ranges.
        std::vector<double> params2(kSeeds);
        std::vector<PointT> points2(kSeeds);
        for (int j = 0; j < kSeeds; ++j)
        {
            params2[j] = m_low2 + (m_high2 - m_low2) * j / (kSeeds - 1);
            points2[j] = m_curve2.point(params2[j]);
        }

        auto nearestOnCurve2 = [&](const PointT &p)
        {
            int best = 0;
            double bestDistance = std::numeric_limits<double>::max();
            for (int j = 0; j < kSeeds; ++j)
            {
                const double distance = distanceSquared(p, points2[j]);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = j;
                }
            }
            return params2[best];
        };

        std::vector<Seed> seeds(kSeeds);
        for (int i = 0; i < kSeeds; ++i)
        {
            seeds[i].t1 = m_low1 + (m_high1 - m_low1) * i / (kSeeds - 1);
            seeds[i].s = nearestOnCurve2(m_curve1.point(seeds[i].t1));
            seeds[i].coincident = coincidentAt(seeds[i].t1, seeds[i].s);
        }

        int i = 0;
        while (i < kSeeds)
        {
            if (!seeds[i].coincident)
            {
                ++i;
                continue;
            }
            int last = i;
            while (last + 1 < kSeeds && seeds[last + 1].coincident)
                ++last;
            if (last - i + 1 >= 3)
            {
                OverlapRecord overlap;
                double sStart = seeds[i].s;
                double sEnd = seeds[last].s;
                overlap.start1 =
                    refineOverlapEndpoint(seeds, i, /*forward=*/false, sStart);
                overlap.end1 =
                    refineOverlapEndpoint(seeds, last, /*forward=*/true, sEnd);
                overlap.start2 = sStart;
                overlap.end2 = sEnd;
                overlap.direction = sEnd >= sStart ? 1 : -1;
                m_overlaps.push_back(overlap);
            }
            i = last + 1;
        }

        // Deduplicate overlaps (Newton noise can duplicate the same range).
        std::sort(m_overlaps.begin(), m_overlaps.end(),
                  [](const OverlapRecord &a, const OverlapRecord &b)
                  { return a.start1 < b.start1; });
        m_overlaps.erase(
            std::unique(
                m_overlaps.begin(), m_overlaps.end(),
                [this](const OverlapRecord &a, const OverlapRecord &b)
                {
                    return std::abs(a.start1 - b.start1) <=
                               1.0e-6 * (m_high1 - m_low1) &&
                           std::abs(a.end1 - b.end1) <=
                               1.0e-6 * (m_high1 - m_low1);
                }),
            m_overlaps.end());
    }

    // Bisect between the adjacent non-coincident seed to tighten an
    // overlap endpoint. `forward` tightens the range end.
    double refineOverlapEndpoint(const std::vector<Seed> &seeds, int index,
                                 bool forward, double &sRef) const
    {
        const double inside = seeds[static_cast<std::size_t>(index)].t1;
        double outside;
        if (forward)
            outside = index + 1 < static_cast<int>(seeds.size())
                          ? seeds[static_cast<std::size_t>(index + 1)].t1
                          : m_high1;
        else
            outside = index > 0 ? seeds[static_cast<std::size_t>(index - 1)].t1
                                : m_low1;
        double coincident = inside;
        double notCoincident = outside;
        for (int iteration = 0; iteration < 40; ++iteration)
        {
            const double mid = 0.5 * (coincident + notCoincident);
            double s = sRef;
            if (coincidentAt(mid, s))
            {
                coincident = mid;
                sRef = s;
            }
            else
            {
                notCoincident = mid;
            }
        }
        return coincident;
    }

    void filterOverlapPoints(double span1, double span2)
    {
        if (m_overlaps.empty())
            return;
        m_intersections.erase(
            std::remove_if(
                m_intersections.begin(), m_intersections.end(),
                [this, span1, span2](const IntersectionRecord &record)
                {
                    for (const OverlapRecord &overlap : m_overlaps)
                    {
                        if (record.param1 >=
                                overlap.start1 - 1.0e-6 * span1 &&
                            record.param1 <= overlap.end1 + 1.0e-6 * span1)
                            return true;
                        if (record.param2 >=
                                overlap.start2 - 1.0e-6 * span2 &&
                            record.param2 <= overlap.end2 + 1.0e-6 * span2)
                            return true;
                    }
                    return false;
                }),
            m_intersections.end());
    }

    void deduplicate(double, double)
    {
        std::sort(m_intersections.begin(), m_intersections.end(),
                  [](const IntersectionRecord &a, const IntersectionRecord &b)
                  { return a.param1 < b.param1; });
        // Merge records whose curves meet at the same physical location.
        // Strict rule: both curves pass within tolerance. Tangential
        // contacts additionally use the contact window sqrt(2*tol)*speed --
        // curves within tolerance of tangency span that arc around the
        // tangent point, so adjacent seeds legitimately land inside it
        // (also handles closed-curve seam duplicates param 0 == 2pi).
        const double tangentialWindow =
            4.0 * std::sqrt(2.0 * m_tolerance);
        m_intersections.erase(
            std::unique(m_intersections.begin(), m_intersections.end(),
                        [this, tangentialWindow](const IntersectionRecord &a,
                                                 const IntersectionRecord &b)
                        {
                            double window = m_tolerance;
                            if (a.tangential && b.tangential)
                                window = std::max(window, tangentialWindow);
                            return (m_curve1.point(a.param1) -
                                    m_curve1.point(b.param1))
                                       .length() <= window &&
                                   (m_curve2.point(a.param2) -
                                    m_curve2.point(b.param2))
                                       .length() <= window;
                        }),
            m_intersections.end());
    }

    void sortAndRank()
    {
        std::sort(m_intersections.begin(), m_intersections.end(),
                  [](const IntersectionRecord &a, const IntersectionRecord &b)
                  { return a.param1 < b.param1; });
        m_order1.resize(m_intersections.size());
        for (std::size_t i = 0; i < m_intersections.size(); ++i)
            m_order1[i] = i; // already sorted by param1
        std::vector<std::size_t> byParam2(m_intersections.size());
        for (std::size_t i = 0; i < m_intersections.size(); ++i)
            byParam2[i] = i;
        std::sort(byParam2.begin(), byParam2.end(),
                  [&](std::size_t a, std::size_t b)
                  {
                      return m_intersections[a].param2 <
                             m_intersections[b].param2;
                  });
        m_order2.resize(m_intersections.size());
        for (std::size_t rank = 0; rank < byParam2.size(); ++rank)
            m_order2[byParam2[rank]] = rank;
    }

    static double distanceSquared(const PointT &a, const PointT &b)
    {
        const double dx = a.x - b.x;
        const double dy = a.y - b.y;
        if constexpr (requires { a.z; })
        {
            const double dz = a.z - b.z;
            return dx * dx + dy * dy + dz * dz;
        }
        else
        {
            return dx * dx + dy * dy;
        }
    }

    Core m_curve1;
    Core m_curve2;
    double m_low1 = 0.0;
    double m_high1 = 1.0;
    double m_low2 = 0.0;
    double m_high2 = 1.0;
    double m_tolerance = 1.0e-7;
    std::vector<IntersectionRecord> m_intersections;
    std::vector<OverlapRecord> m_overlaps;
    std::vector<std::size_t> m_order1;
    std::vector<std::size_t> m_order2;
};

} // namespace detail
} // namespace ge

// Shared implementation for the 2D/3D intersection-set objects. The ARX
// constructor overloads that take an AcGeInterval map onto the interval
// overloads; without intervals the full curve domains are used.
template <class PointT, class VectorT>
class AcGeCurveCurveIntBase
{
public:
    using Intersector = ge::detail::CurveCurveIntersector<PointT, VectorT>;

    AcGeCurveCurveIntBase() = default;

    template <class Curve1, class Curve2>
    AcGeCurveCurveIntBase(const Curve1 &curve1, const Curve2 &curve2,
                          const AcGeTol &tol = AcGeTol{})
    {
        set(curve1, curve2, tol);
    }

    template <class Curve1, class Curve2>
    AcGeCurveCurveIntBase(const Curve1 &curve1, const Curve2 &curve2,
                          const AcGeInterval &range1,
                          const AcGeInterval &range2,
                          const AcGeTol &tol = AcGeTol{})
    {
        set(curve1, curve2, range1, range2, tol);
    }

    template <class Curve1, class Curve2>
    AcGeCurveCurveIntBase &set(const Curve1 &curve1, const Curve2 &curve2,
                               const AcGeTol &tol = AcGeTol{})
    {
        double low1 = 0.0;
        double high1 = 1.0;
        double low2 = 0.0;
        double high2 = 1.0;
        domainOf(curve1, low1, high1);
        domainOf(curve2, low2, high2);
        return setRanges(curve1, curve2, low1, high1, low2, high2, tol);
    }

    template <class Curve1, class Curve2>
    AcGeCurveCurveIntBase &set(const Curve1 &curve1, const Curve2 &curve2,
                               const AcGeInterval &range1,
                               const AcGeInterval &range2,
                               const AcGeTol &tol = AcGeTol{})
    {
        return setRanges(curve1, curve2, range1.lowerBound(),
                         range1.upperBound(), range2.lowerBound(),
                         range2.upperBound(), tol);
    }

    int numIntPoints() const
    {
        return m_intersector
                   ? static_cast<int>(m_intersector->intersections().size())
                   : 0;
    }

    PointT intPoint(int index) const
    {
        return intersector().intersections().at(checkIndex(index)).point;
    }

    double intPointTol(int index) const
    {
        return intersector()
            .intersections()
            .at(static_cast<std::size_t>(index))
            .tolerance;
    }

    bool getIntParams(int index, double &param1, double &param2) const
    {
        const auto &record =
            intersector().intersections().at(checkIndex(index));
        param1 = record.param1;
        param2 = record.param2;
        return true;
    }

    // 0-based rank of the intersection point along curve1 / curve2.
    int orderWrt1(int index) const
    {
        return static_cast<int>(
            intersector().order1().at(static_cast<std::size_t>(index)));
    }
    int orderWrt2(int index) const
    {
        return static_cast<int>(
            intersector().order2().at(static_cast<std::size_t>(index)));
    }

    bool isTangential(int index) const
    {
        return intersector()
            .intersections()
            .at(static_cast<std::size_t>(index))
            .tangential;
    }
    bool isTransversal(int index) const { return !isTangential(index); }

    int overlapCount() const
    {
        return m_intersector ? static_cast<int>(m_intersector->overlaps().size())
                             : 0;
    }

    bool getOverlapRanges(int index, AcGeInterval &range1,
                          AcGeInterval &range2) const
    {
        const auto &overlap =
            intersector().overlaps().at(static_cast<std::size_t>(index));
        range1 = AcGeInterval{overlap.start1, overlap.end1};
        range2 = AcGeInterval{overlap.start2, overlap.end2};
        return true;
    }

    int overlapDirection() const
    {
        const auto &overlaps = intersector().overlaps();
        if (overlaps.empty())
            return 0;
        return overlaps.front().direction;
    }

    double tolerance() const
    {
        return m_intersector ? m_intersector->tolerance() : 0.0;
    }

protected:
    const Intersector &intersector() const
    {
        if (!m_intersector)
            throw std::logic_error(
                "CurveCurveInt has no curves assigned.");
        return *m_intersector;
    }

    static std::size_t checkIndex(int index)
    {
        if (index < 0)
            throw std::out_of_range(
                "CurveCurveInt intersection index is out of range.");
        return static_cast<std::size_t>(index);
    }

private:
    template <class Curve>
    static void domainOf(const Curve &curve, double &low, double &high)
    {
        if constexpr (requires {
                          { curve.minParameter() } -> std::convertible_to<double>;
                          { curve.maxParameter() } -> std::convertible_to<double>;
                      })
        {
            low = curve.minParameter();
            high = curve.maxParameter();
        }
        else if constexpr (requires {
                               { curve.startParam() } -> std::convertible_to<double>;
                               { curve.endParam() } -> std::convertible_to<double>;
                           })
        {
            low = curve.startParam();
            high = curve.endParam();
        }
        else if constexpr (requires {
                               { curve.startAngle } -> std::convertible_to<double>;
                               { curve.endAngle } -> std::convertible_to<double>;
                           })
        {
            // Conic arcs expose their angle domain as data members.
            low = curve.startAngle;
            high = curve.endAngle;
        }
        else
        {
            low = 0.0;
            high = 1.0;
        }
    }

    template <class Curve1, class Curve2>
    AcGeCurveCurveIntBase &setRanges(const Curve1 &curve1,
                                     const Curve2 &curve2, double low1,
                                     double high1, double low2, double high2,
                                     const AcGeTol &tol)
    {
        using Core = typename Intersector::Core;
        Core core1;
        core1.assign(curve1);
        Core core2;
        core2.assign(curve2);
        m_intersector.emplace(core1, core2, low1, high1, low2, high2,
                              tol.equalPoint() > 0.0 ? tol.equalPoint()
                                                     : 1.0e-7);
        return *this;
    }

    std::optional<Intersector> m_intersector;
};

class ACGE_PORT AcGeCurveCurveInt2d
    : public AcGeCurveCurveIntBase<AcGePoint2d, AcGeVector2d>
{
public:
    using AcGeCurveCurveIntBase<AcGePoint2d, AcGeVector2d>::AcGeCurveCurveIntBase;
};

class ACGE_PORT AcGeCurveCurveInt3d
    : public AcGeCurveCurveIntBase<AcGePoint3d, AcGeVector3d>
{
public:
    using AcGeCurveCurveIntBase<AcGePoint3d, AcGeVector3d>::AcGeCurveCurveIntBase;

    // ARX plane-normal constructor: the normal is stored and returned by
    // planeNormal() (used by ARX for planar projection).
    template <class Curve1, class Curve2>
    AcGeCurveCurveInt3d(const Curve1 &curve1, const Curve2 &curve2,
                        const AcGeVector3d &planeNormal,
                        const AcGeTol &tol = AcGeTol{})
        : AcGeCurveCurveIntBase<AcGePoint3d, AcGeVector3d>(curve1, curve2, tol),
          m_planeNormal(planeNormal)
    {
    }
    template <class Curve1, class Curve2>
    AcGeCurveCurveInt3d(const Curve1 &curve1, const Curve2 &curve2,
                        const AcGeInterval &range1,
                        const AcGeInterval &range2,
                        const AcGeVector3d &planeNormal,
                        const AcGeTol &tol = AcGeTol{})
        : AcGeCurveCurveIntBase<AcGePoint3d, AcGeVector3d>(curve1, curve2,
                                                           range1, range2, tol),
          m_planeNormal(planeNormal)
    {
    }

    AcGeVector3d planeNormal() const { return m_planeNormal; }

private:
    AcGeVector3d m_planeNormal{0.0, 0.0, 1.0};
};

