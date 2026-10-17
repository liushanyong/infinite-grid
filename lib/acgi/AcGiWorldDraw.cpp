#include "acgi/AcGiWorldDraw.h"

#include "acgi/AcGiTextEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <climits>
#include <limits>
#include <type_traits>
#include <utility>

// Return convention (ObjectARX contract): the primitive emitters return
// kFalse (false) when the primitive was stored, kTrue (true) when the
// operation was terminated — see the AcGiGeometry header note.  Internal
// helpers in this file use plain bool (true = ok) and adapt at the boundary.

namespace
{
constexpr double kTwoPi = 6.2831853071795864769;

long double signedDistance(const AcGePoint2d &edgeStart,
                           const AcGePoint2d &edgeEnd,
                           const AcGePoint2d &point, double winding)
{
    if (!std::isfinite(edgeStart.x) || !std::isfinite(edgeStart.y) ||
        !std::isfinite(edgeEnd.x) || !std::isfinite(edgeEnd.y) ||
        !std::isfinite(point.x) || !std::isfinite(point.y))
        return -std::numeric_limits<long double>::infinity();
    const long double dx =
        static_cast<long double>(edgeEnd.x) - edgeStart.x;
    const long double dy =
        static_cast<long double>(edgeEnd.y) - edgeStart.y;
    const long double length = std::hypot(dx, dy);
    if (!(length > 0.0L) || !std::isfinite(length))
        return -std::numeric_limits<long double>::infinity();
    const long double px =
        static_cast<long double>(point.x) - edgeStart.x;
    const long double py =
        static_cast<long double>(point.y) - edgeStart.y;
    return ((dx * py - dy * px) / length) * winding;
}

std::vector<std::array<size_t, 3>>
triangulatePolygon(const std::vector<AcGePoint3d> &vertices)
{
    if (vertices.size() < 3)
        return {};

    AcGeVector3d normal;
    for (size_t index = 0; index < vertices.size(); ++index)
    {
        const AcGePoint3d &current = vertices[index];
        const AcGePoint3d &next = vertices[(index + 1) % vertices.size()];
        normal.x += (current.y - next.y) * (current.z + next.z);
        normal.y += (current.z - next.z) * (current.x + next.x);
        normal.z += (current.x - next.x) * (current.y + next.y);
    }
    const double normalLengthSq = normal.lengthSq();
    if (!(normalLengthSq > 1.0e-24) || !std::isfinite(normalLengthSq))
        return {};
    normal = normal.normal();
    const AcGeVector3d u =
        AcGeCircArc3d(vertices.front(), normal, 1.0).referenceAxis();
    const AcGeVector3d v = normal.crossProduct(u);
    std::vector<AcGePoint2d> projected;
    projected.reserve(vertices.size());
    for (const AcGePoint3d &point : vertices)
    {
        const AcGeVector3d relative = point - vertices.front();
        const AcGePoint2d planar(relative.dotProduct(u), relative.dotProduct(v));
        if (!std::isfinite(planar.x) || !std::isfinite(planar.y))
            return {};
        projected.push_back(planar);
    }

    long double areaTwice = 0.0L;
    for (size_t index = 0; index < projected.size(); ++index)
    {
        const AcGePoint2d &current = projected[index];
        const AcGePoint2d &next = projected[(index + 1) % projected.size()];
        areaTwice += static_cast<long double>(current.x) * next.y -
                     static_cast<long double>(current.y) * next.x;
    }
    if (!std::isfinite(areaTwice) || areaTwice == 0.0L)
        return {};
    const long double winding = areaTwice > 0.0L ? 1.0L : -1.0L;
    std::vector<size_t> remaining(vertices.size());
    for (size_t index = 0; index < remaining.size(); ++index)
        remaining[index] = index;

    const auto cross = [&](size_t a, size_t b, size_t c) {
        const long double abx =
            static_cast<long double>(projected[b].x) - projected[a].x;
        const long double aby =
            static_cast<long double>(projected[b].y) - projected[a].y;
        const long double acx =
            static_cast<long double>(projected[c].x) - projected[a].x;
        const long double acy =
            static_cast<long double>(projected[c].y) - projected[a].y;
        return (abx * acy - aby * acx) * winding;
    };
    const auto insideTriangle = [&](size_t point, size_t a, size_t b,
                                    size_t c) {
        return cross(a, b, point) > 1.0e-12L &&
               cross(b, c, point) > 1.0e-12L &&
               cross(c, a, point) > 1.0e-12L;
    };

    std::vector<std::array<size_t, 3>> triangles;
    triangles.reserve(vertices.size() - 2);
    while (remaining.size() > 3)
    {
        bool clippedEar = false;
        for (size_t index = 0; index < remaining.size(); ++index)
        {
            const size_t previous =
                remaining[(index + remaining.size() - 1) % remaining.size()];
            const size_t current = remaining[index];
            const size_t next = remaining[(index + 1) % remaining.size()];
            if (cross(previous, current, next) <= 1.0e-12L)
                continue;
            bool containsVertex = false;
            for (const size_t candidate : remaining)
            {
                if (candidate != previous && candidate != current &&
                    candidate != next &&
                    insideTriangle(candidate, previous, current, next))
                {
                    containsVertex = true;
                    break;
                }
            }
            if (containsVertex)
                continue;
            triangles.push_back({previous, current, next});
            remaining.erase(remaining.begin() +
                            static_cast<std::ptrdiff_t>(index));
            clippedEar = true;
            break;
        }
        if (!clippedEar)
            return {};
    }
    triangles.push_back({remaining[0], remaining[1], remaining[2]});
    return triangles;
}

bool triangulatePolygonWithHoles(
    const AcGePoint3d *vertices, const std::vector<std::vector<size_t>> &rings,
    std::vector<size_t> &mergedIndices,
    std::vector<std::array<size_t, 3>> &triangles)
{
    if (vertices == nullptr || rings.empty() || rings.front().size() < 3)
        return false;

    const std::vector<size_t> &outer = rings.front();
    AcGeVector3d normal;
    for (size_t index = 0; index < outer.size(); ++index)
    {
        const AcGePoint3d &current = vertices[outer[index]];
        const AcGePoint3d &next = vertices[outer[(index + 1) % outer.size()]];
        normal.x += (current.y - next.y) * (current.z + next.z);
        normal.y += (current.z - next.z) * (current.x + next.x);
        normal.z += (current.x - next.x) * (current.y + next.y);
    }
    if (normal.isZeroLength())
        return false;
    normal = normal.normal();
    const AcGePoint3d origin = vertices[outer.front()];
    const AcGeVector3d u = AcGeCircArc3d(origin, normal, 1.0).referenceAxis();
    const AcGeVector3d v = normal.crossProduct(u);
    const auto project = [&](size_t index) {
        const AcGeVector3d relative = vertices[index] - origin;
        return AcGePoint2d(relative.dotProduct(u), relative.dotProduct(v));
    };
    double scale = 1.0;
    for (const auto &ring : rings)
    {
        if (ring.size() < 3)
            return false;
        for (const size_t index : ring)
        {
            const AcGeVector3d relative = vertices[index] - origin;
            if (!std::isfinite(relative.x) || !std::isfinite(relative.y) ||
                !std::isfinite(relative.z))
                return false;
            scale = std::max(scale, relative.length());
        }
    }
    for (const auto &ring : rings)
        for (const size_t index : ring)
            if (std::abs((vertices[index] - origin).dotProduct(normal)) >
                scale * 1.0e-8)
                return false;
    const double tolerance = scale * 1.0e-10;
    const auto ringArea = [&](const std::vector<size_t> &ring) {
        long double area = 0.0L;
        for (size_t index = 0; index < ring.size(); ++index)
        {
            const AcGePoint2d a = project(ring[index]);
            const AcGePoint2d b = project(ring[(index + 1) % ring.size()]);
            area += static_cast<long double>(a.x) * b.y -
                    static_cast<long double>(a.y) * b.x;
        }
        return area;
    };
    std::vector<std::vector<size_t>> oriented = rings;
    if (std::abs(ringArea(oriented.front())) <= 1.0e-24L)
        return false;
    if (ringArea(oriented.front()) < 0.0L)
        std::reverse(oriented.front().begin(), oriented.front().end());
    for (size_t index = 1; index < oriented.size(); ++index)
    {
        if (std::abs(ringArea(oriented[index])) <= 1.0e-24L)
            return false;
        if (ringArea(oriented[index]) > 0.0L)
            std::reverse(oriented[index].begin(), oriented[index].end());
    }

    const auto pointInRing = [&](const AcGePoint2d &point,
                                 const std::vector<size_t> &ring) {
        bool inside = false;
        for (size_t index = 0, previous = ring.size() - 1;
             index < ring.size(); previous = index++)
        {
            const AcGePoint2d a = project(ring[previous]);
            const AcGePoint2d b = project(ring[index]);
            const long double cross =
                (static_cast<long double>(b.x) - a.x) *
                    (static_cast<long double>(point.y) - a.y) -
                (static_cast<long double>(b.y) - a.y) *
                    (static_cast<long double>(point.x) - a.x);
            if (std::abs(cross) <=
                    tolerance * std::hypot(b.x - a.x, b.y - a.y) &&
                point.x >= std::min(a.x, b.x) - tolerance &&
                point.x <= std::max(a.x, b.x) + tolerance &&
                point.y >= std::min(a.y, b.y) - tolerance &&
                point.y <= std::max(a.y, b.y) + tolerance)
                return false;
            if ((a.y > point.y) != (b.y > point.y))
            {
                const double crossingX =
                    a.x + (point.y - a.y) * (b.x - a.x) / (b.y - a.y);
                if (point.x < crossingX)
                    inside = !inside;
            }
        }
        return inside;
    };
    const auto orientation = [](const AcGePoint2d &a, const AcGePoint2d &b,
                                const AcGePoint2d &c) {
        return (static_cast<long double>(b.x) - a.x) *
                   (static_cast<long double>(c.y) - a.y) -
               (static_cast<long double>(b.y) - a.y) *
                   (static_cast<long double>(c.x) - a.x);
    };
    const auto edgeTolerance = [&](const AcGePoint2d &a,
                                   const AcGePoint2d &b) {
        return tolerance * std::hypot(b.x - a.x, b.y - a.y);
    };
    const auto onSegment = [&](const AcGePoint2d &a, const AcGePoint2d &b,
                               const AcGePoint2d &point) {
        return std::abs(orientation(a, b, point)) <= edgeTolerance(a, b) &&
               point.x >= std::min(a.x, b.x) - tolerance &&
               point.x <= std::max(a.x, b.x) + tolerance &&
               point.y >= std::min(a.y, b.y) - tolerance &&
               point.y <= std::max(a.y, b.y) + tolerance;
    };
    const auto intersects = [&](size_t aIndex, size_t bIndex, size_t cIndex,
                                size_t dIndex) {
        const AcGePoint2d a = project(aIndex);
        const AcGePoint2d b = project(bIndex);
        const AcGePoint2d c = project(cIndex);
        const AcGePoint2d d = project(dIndex);
        const long double o1 = orientation(a, b, c);
        const long double o2 = orientation(a, b, d);
        const long double o3 = orientation(c, d, a);
        const long double o4 = orientation(c, d, b);
        const double abTolerance = edgeTolerance(a, b);
        const double cdTolerance = edgeTolerance(c, d);
        if (((o1 > abTolerance && o2 < -abTolerance) ||
             (o1 < -abTolerance && o2 > abTolerance)) &&
            ((o3 > cdTolerance && o4 < -cdTolerance) ||
             (o3 < -cdTolerance && o4 > cdTolerance)))
            return true;
        return onSegment(a, b, c) || onSegment(a, b, d) ||
               onSegment(c, d, a) || onSegment(c, d, b);
    };

    for (size_t holeIndex = 1; holeIndex < oriented.size(); ++holeIndex)
    {
        const auto &hole = oriented[holeIndex];
        if (!pointInRing(project(hole.front()), oriented.front()))
            return false;
        for (size_t index = 0; index < hole.size(); ++index)
        {
            const size_t a = hole[index];
            const size_t b = hole[(index + 1) % hole.size()];
            for (const auto &ring : oriented)
                for (size_t edge = 0; edge < ring.size(); ++edge)
                {
                    const size_t c = ring[edge];
                    const size_t d = ring[(edge + 1) % ring.size()];
                    if ((a == c || a == d || b == c || b == d) &&
                        &ring == &hole)
                        continue;
                    if (intersects(a, b, c, d))
                        return false;
                }
        }
    }
    for (size_t firstHole = 1; firstHole < oriented.size(); ++firstHole)
        for (size_t secondHole = firstHole + 1; secondHole < oriented.size();
             ++secondHole)
            if (pointInRing(project(oriented[firstHole].front()),
                            oriented[secondHole]) ||
                pointInRing(project(oriented[secondHole].front()),
                            oriented[firstHole]))
                return false;

    mergedIndices = oriented.front();
    std::vector<std::pair<size_t, size_t>> bridges;
    for (size_t holeIndex = 1; holeIndex < oriented.size(); ++holeIndex)
    {
        const auto &hole = oriented[holeIndex];
        size_t holePosition = 0;
        for (size_t index = 1; index < hole.size(); ++index)
        {
            const AcGePoint2d candidate = project(hole[index]);
            const AcGePoint2d selected = project(hole[holePosition]);
            if (candidate.x > selected.x ||
                (candidate.x == selected.x && candidate.y < selected.y))
                holePosition = index;
        }
        const size_t holeVertex = hole[holePosition];
        const AcGePoint2d holePoint = project(holeVertex);
        size_t bestPosition = mergedIndices.size();
        double bestDistance = std::numeric_limits<double>::infinity();
        for (size_t candidatePosition = 0;
             candidatePosition < mergedIndices.size(); ++candidatePosition)
        {
            const size_t candidateVertex = mergedIndices[candidatePosition];
            if (candidateVertex == holeVertex)
                continue;
            const AcGePoint2d candidatePoint = project(candidateVertex);
            const AcGePoint2d midpoint((holePoint.x + candidatePoint.x) * 0.5,
                                       (holePoint.y + candidatePoint.y) * 0.5);
            if (!pointInRing(midpoint, oriented.front()))
                continue;
            bool insideHole = false;
            for (size_t otherHole = 1; otherHole < oriented.size(); ++otherHole)
                if (pointInRing(midpoint, oriented[otherHole]))
                {
                    insideHole = true;
                    break;
                }
            if (insideHole)
                continue;

            bool blocked = false;
            for (const auto &ring : oriented)
            {
                for (size_t edge = 0; edge < ring.size(); ++edge)
                {
                    const size_t a = ring[edge];
                    const size_t b = ring[(edge + 1) % ring.size()];
                    if (a == holeVertex || b == holeVertex ||
                        a == candidateVertex || b == candidateVertex)
                        continue;
                    if (intersects(holeVertex, candidateVertex, a, b))
                    {
                        blocked = true;
                        break;
                    }
                }
                if (blocked)
                    break;
            }
            for (const auto &[a, b] : bridges)
            {
                if (a == holeVertex || b == holeVertex ||
                    a == candidateVertex || b == candidateVertex)
                    continue;
                if (intersects(holeVertex, candidateVertex, a, b))
                {
                    blocked = true;
                    break;
                }
            }
            if (blocked)
                continue;
            const double dx = candidatePoint.x - holePoint.x;
            const double dy = candidatePoint.y - holePoint.y;
            const double distance = dx * dx + dy * dy;
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestPosition = candidatePosition;
            }
        }
        if (bestPosition == mergedIndices.size())
            return false;

        const size_t bridgeVertex = mergedIndices[bestPosition];
        std::vector<size_t> merged;
        merged.reserve(mergedIndices.size() + hole.size() + 2);
        merged.insert(merged.end(), mergedIndices.begin(),
                      mergedIndices.begin() +
                          static_cast<std::ptrdiff_t>(bestPosition + 1));
        merged.push_back(holeVertex);
        for (size_t offset = 1; offset < hole.size(); ++offset)
            merged.push_back(hole[(holePosition + offset) % hole.size()]);
        merged.push_back(holeVertex);
        merged.push_back(bridgeVertex);
        merged.insert(merged.end(),
                      mergedIndices.begin() +
                          static_cast<std::ptrdiff_t>(bestPosition + 1),
                      mergedIndices.end());
        mergedIndices = std::move(merged);
        bridges.emplace_back(holeVertex, bridgeVertex);
    }

    std::vector<AcGePoint3d> mergedPoints;
    mergedPoints.reserve(mergedIndices.size());
    for (const size_t index : mergedIndices)
        mergedPoints.push_back(vertices[index]);
    triangles = triangulatePolygon(mergedPoints);
    return !triangles.empty();
}

long double cross2d(const AcGePoint2d &a, const AcGePoint2d &b,
                    const AcGePoint2d &c)
{
    return (static_cast<long double>(b.x) - a.x) *
               (static_cast<long double>(c.y) - a.y) -
           (static_cast<long double>(b.y) - a.y) *
               (static_cast<long double>(c.x) - a.x);
}

long double polygonAreaTwice(const std::vector<AcGePoint2d> &polygon)
{
    long double area = 0.0L;
    for (size_t index = 0; index < polygon.size(); ++index)
    {
        const AcGePoint2d &a = polygon[index];
        const AcGePoint2d &b = polygon[(index + 1) % polygon.size()];
        area += static_cast<long double>(a.x) * b.y -
                static_cast<long double>(a.y) * b.x;
    }
    return area;
}

bool pointOnSegment2d(const AcGePoint2d &a, const AcGePoint2d &b,
                       const AcGePoint2d &point, double tolerance)
{
    const long double edgeLength = std::hypot(b.x - a.x, b.y - a.y);
    return std::abs(cross2d(a, b, point)) <= tolerance * edgeLength &&
           point.x >= std::min(a.x, b.x) - tolerance &&
           point.x <= std::max(a.x, b.x) + tolerance &&
           point.y >= std::min(a.y, b.y) - tolerance &&
           point.y <= std::max(a.y, b.y) + tolerance;
}

bool segmentsIntersect2d(const AcGePoint2d &a, const AcGePoint2d &b,
                         const AcGePoint2d &c, const AcGePoint2d &d,
                         double tolerance)
{
    const long double abTolerance =
        tolerance * std::hypot(b.x - a.x, b.y - a.y);
    const long double cdTolerance =
        tolerance * std::hypot(d.x - c.x, d.y - c.y);
    const long double o1 = cross2d(a, b, c);
    const long double o2 = cross2d(a, b, d);
    const long double o3 = cross2d(c, d, a);
    const long double o4 = cross2d(c, d, b);
    if (((o1 > abTolerance && o2 < -abTolerance) ||
         (o1 < -abTolerance && o2 > abTolerance)) &&
        ((o3 > cdTolerance && o4 < -cdTolerance) ||
         (o3 < -cdTolerance && o4 > cdTolerance)))
        return true;
    return pointOnSegment2d(a, b, c, tolerance) ||
           pointOnSegment2d(a, b, d, tolerance) ||
           pointOnSegment2d(c, d, a, tolerance) ||
           pointOnSegment2d(c, d, b, tolerance);
}

bool normalizeClipPolygon(std::vector<AcGePoint2d> &polygon)
{
    if (polygon.size() < 3)
        return false;
    for (const AcGePoint2d &point : polygon)
        if (!std::isfinite(point.x) || !std::isfinite(point.y))
            return false;
    if (polygon.front().distanceTo(polygon.back()) <= 1.0e-10)
        polygon.pop_back();
    if (polygon.size() < 3)
        return false;
    for (size_t index = 0; index < polygon.size();)
    {
        const size_t next = (index + 1) % polygon.size();
        if (polygon[index].distanceTo(polygon[next]) <= 1.0e-10)
        {
            polygon.erase(polygon.begin() +
                          static_cast<std::ptrdiff_t>(next));
            if (polygon.size() < 3)
                return false;
            if (next == 0)
                index = 0;
        }
        else
        {
            ++index;
        }
    }
    bool changed = true;
    while (changed && polygon.size() > 3)
    {
        changed = false;
        for (size_t index = 0; index < polygon.size(); ++index)
        {
            const AcGePoint2d &previous =
                polygon[(index + polygon.size() - 1) % polygon.size()];
            const AcGePoint2d &current = polygon[index];
            const AcGePoint2d &next = polygon[(index + 1) % polygon.size()];
            const double scale =
                std::hypot(current.x - previous.x, current.y - previous.y) +
                std::hypot(next.x - current.x, next.y - current.y);
            if (std::abs(cross2d(previous, current, next)) <=
                    1.0e-10 * scale &&
                (current.x - previous.x) * (current.x - next.x) +
                        (current.y - previous.y) * (current.y - next.y) <=
                    1.0e-10)
            {
                polygon.erase(polygon.begin() +
                              static_cast<std::ptrdiff_t>(index));
                changed = true;
                break;
            }
        }
    }
    if (polygon.size() < 3 || std::abs(polygonAreaTwice(polygon)) <= 1.0e-20L)
        return false;

    const double tolerance = 1.0e-10;
    for (size_t first = 0; first < polygon.size(); ++first)
    {
        const size_t firstNext = (first + 1) % polygon.size();
        for (size_t second = first + 1; second < polygon.size(); ++second)
        {
            const size_t secondNext = (second + 1) % polygon.size();
            if (first == second || firstNext == second ||
                secondNext == first)
                continue;
            if (segmentsIntersect2d(polygon[first], polygon[firstNext],
                                    polygon[second], polygon[secondNext],
                                    tolerance))
                return false;
        }
    }
    return true;
}

bool pointInSimplePolygon(const std::vector<AcGePoint2d> &polygon,
                          const AcGePoint2d &point)
{
    bool inside = false;
    for (size_t index = 0, previous = polygon.size() - 1;
         index < polygon.size(); previous = index++)
    {
        const AcGePoint2d &a = polygon[previous];
        const AcGePoint2d &b = polygon[index];
        if (pointOnSegment2d(a, b, point, 1.0e-10))
            return true;
        if ((a.y > point.y) != (b.y > point.y))
        {
            const double intersectionX =
                a.x + (point.y - a.y) * (b.x - a.x) / (b.y - a.y);
            if (point.x < intersectionX)
                inside = !inside;
        }
    }
    return inside;
}

double ccwNormalized(double angle)
{
    const double wrapped = std::fmod(angle, kTwoPi);
    return wrapped < 0.0 ? wrapped + kTwoPi : wrapped;
}

// The circumarc through three points, swept CCW from |first| through
// |second| to |third|.  Returns false when the points are collinear.
bool circumArcThrough(const AcGePoint3d &first, const AcGePoint3d &second,
                      const AcGePoint3d &third, AcGeCircArc3d &arc)
{
    const AcGeVector3d a = second - first;
    const AcGeVector3d b = third - first;
    const AcGeVector3d cross = a.crossProduct(b);
    const double crossLengthSq = cross.lengthSq();
    if (!(crossLengthSq > 1.0e-30))
        return false;
    const AcGeVector3d toCenter =
        (b.crossProduct(cross) * a.lengthSq() +
         cross.crossProduct(a) * b.lengthSq()) *
        (0.5 / crossLengthSq);
    const AcGePoint3d center = first + toCenter;
    const double radius = toCenter.length();
    AcGeVector3d normal = cross.normal();

    // CCW sweep first -> third must pass through second; when it does not,
    // flip the plane normal (which mirrors the CCW sense) and re-measure.
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        const AcGeVector3d u =
            AcGeCircArc3d(center, normal, radius).referenceAxis();
        const AcGeVector3d v = normal.crossProduct(u);
        const auto angleOf = [&](const AcGePoint3d &point) {
            const AcGeVector3d radial = point - center;
            return std::atan2(radial.dotProduct(v), radial.dotProduct(u));
        };
        const double startAngle = angleOf(first);
        const double toSecond = ccwNormalized(angleOf(second) - startAngle);
        const double toThird = ccwNormalized(angleOf(third) - startAngle);
        if (toSecond <= toThird || attempt == 1)
        {
            arc.set(center, normal, radius, startAngle, startAngle + toThird);
            return true;
        }
        normal = -normal;
    }
    return false;
}

std::vector<AcGePoint3d> sampleArcPoints(const acgs::ViewportDraw &context,
                                         const AcGeCircArc3d &arc)
{
    const acdb::TesselationOptions options = context.optionsFor(arc.radius);
    const double sweep = arc.endAngle - arc.startAngle;
    const int segments =
        std::max(8, static_cast<int>(std::ceil(sweep / options.angleStep)));
    std::vector<AcGePoint3d> samples;
    samples.reserve(static_cast<size_t>(segments) + 1);
    for (int index = 0; index <= segments; ++index)
    {
        const double t = sweep * static_cast<double>(index) / segments;
        samples.push_back(arc.pointAt(arc.startAngle + t));
    }
    return samples;
}

// Sector/chord closure strokes and their optionally filled fan.
Adesk::Boolean emitArcClosure(AcGiWorldDraw &graphics,
                              const AcGePoint3d &fanOrigin,
                              const AcGePoint3d &start,
                              const AcGePoint3d &end,
                              const std::vector<AcGePoint3d> &samples,
                              AcGiArcType arcType)
{
    if (arcType == AcGiArcType::kAcGiArcSimple)
        return false;
    if (arcType == AcGiArcType::kAcGiArcSector)
    {
        graphics.worldLine(fanOrigin, start);
        graphics.worldLine(fanOrigin, end);
    }
    else // kAcGiArcChord
    {
        graphics.worldLine(start, end);
    }
    if (graphics.fillType() == kAcGiFillAlways)
        for (size_t index = 1; index < samples.size(); ++index)
            graphics.worldTriangle(fanOrigin, samples[index - 1],
                                   samples[index]);
    return false;
}

void applyVertexData(acdb::Triangle &triangle,
                     const std::array<AcGePoint3d, 3> &sourcePoints,
                     const std::array<size_t, 3> &sourceIndices,
                     const AcGiVertexData *vertexData)
{
    if (vertexData == nullptr)
        return;
    const AcGeVector3d edge0 = sourcePoints[1] - sourcePoints[0];
    const AcGeVector3d edge1 = sourcePoints[2] - sourcePoints[0];
    const double d00 = edge0.dotProduct(edge0);
    const double d01 = edge0.dotProduct(edge1);
    const double d11 = edge1.dotProduct(edge1);
    const double denominator = d00 * d11 - d01 * d01;
    if (!(std::abs(denominator) > 1.0e-30) ||
        !std::isfinite(denominator))
        return;

    const std::array<AcGePoint3d, 3> outputPoints{
        triangle.a, triangle.b, triangle.c};
    for (size_t outputIndex = 0; outputIndex < outputPoints.size();
         ++outputIndex)
    {
        const AcGeVector3d relative = outputPoints[outputIndex] - sourcePoints[0];
        const double d20 = relative.dotProduct(edge0);
        const double d21 = relative.dotProduct(edge1);
        const double weight1 = (d11 * d20 - d01 * d21) / denominator;
        const double weight2 = (d00 * d21 - d01 * d20) / denominator;
        const double weight0 = 1.0 - weight1 - weight2;
        if (vertexData->trueColors.size() > sourceIndices[0] &&
            vertexData->trueColors.size() > sourceIndices[1] &&
            vertexData->trueColors.size() > sourceIndices[2])
        {
            triangle.vertexColors[outputIndex] =
                glm::vec4(vertexData->trueColors[sourceIndices[0]].renderColor()) *
                    static_cast<float>(weight0) +
                glm::vec4(vertexData->trueColors[sourceIndices[1]].renderColor()) *
                    static_cast<float>(weight1) +
                glm::vec4(vertexData->trueColors[sourceIndices[2]].renderColor()) *
                    static_cast<float>(weight2);
            triangle.vertexColors[outputIndex].a *=
                triangle.common.color.alpha();
            triangle.hasVertexColors = true;
        }
        if (vertexData->normals.size() > sourceIndices[0] &&
            vertexData->normals.size() > sourceIndices[1] &&
            vertexData->normals.size() > sourceIndices[2])
        {
            AcGeVector3d normal =
                vertexData->normals[sourceIndices[0]] * weight0 +
                vertexData->normals[sourceIndices[1]] * weight1 +
                vertexData->normals[sourceIndices[2]] * weight2;
            const double lengthSq = normal.lengthSq();
            if (lengthSq > 1.0e-24 && std::isfinite(lengthSq))
            {
                triangle.vertexNormals[outputIndex] = normal.normal();
                triangle.hasVertexNormals = true;
            }
        }
        const auto copyMappingCoords =
            [&](AcGiVertexData::MapChannel channel,
                std::array<AcGePoint3d, 3> &output, bool &hasOutput) {
                const AcGePoint3d *coords = vertexData->mappingCoords(channel);
                if (coords == nullptr)
                    return;
                output[outputIndex] =
                    coords[sourceIndices[0]] +
                    (coords[sourceIndices[1]] - coords[sourceIndices[0]]) *
                        weight1 +
                    (coords[sourceIndices[2]] - coords[sourceIndices[0]]) *
                        weight2;
                hasOutput = true;
            };
        copyMappingCoords(AcGiVertexData::kAllChannels,
                          triangle.allMappingCoords,
                          triangle.hasAllMappingCoords);
        copyMappingCoords(AcGiVertexData::kMapChannels,
                          triangle.mapMappingCoords,
                          triangle.hasMapMappingCoords);
        const AcGePoint3d *textureCoords =
            vertexData->mappingCoords(AcGiVertexData::kMapChannels);
        if (textureCoords == nullptr)
            textureCoords =
                vertexData->mappingCoords(AcGiVertexData::kAllChannels);
        if (textureCoords != nullptr)
        {
            const AcGePoint3d &coordinate =
                triangle.hasMapMappingCoords
                    ? triangle.mapMappingCoords[outputIndex]
                    : triangle.allMappingCoords[outputIndex];
            triangle.textureCoords[outputIndex] =
                glm::vec2(static_cast<float>(coordinate.x),
                          static_cast<float>(coordinate.y));
            triangle.hasTextureCoords = true;
        }
    }
}

void pointCloudBounds(const AcGePoint3d *points, size_t count,
                      AcGePoint3d &minimum, AcGePoint3d &maximum)
{
    if (points == nullptr || count == 0)
        return;
    minimum = maximum = points[0];
    for (size_t index = 1; index < count; ++index)
    {
        const AcGePoint3d &point = points[index];
        if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
            !std::isfinite(point.z))
            continue;
        minimum.x = std::min(minimum.x, point.x);
        minimum.y = std::min(minimum.y, point.y);
        minimum.z = std::min(minimum.z, point.z);
        maximum.x = std::max(maximum.x, point.x);
        maximum.y = std::max(maximum.y, point.y);
        maximum.z = std::max(maximum.z, point.z);
    }
}

AcGeMatrix3d mapperAutoTransform(AcGiMapper::AutoTransform mode,
                                 const AcGePoint3d &minimum,
                                 const AcGePoint3d &maximum)
{
    if (mode != AcGiMapper::kObject && mode != AcGiMapper::kModel)
        return AcGeMatrix3d();

    const glm::dvec3 size{maximum.x - minimum.x, maximum.y - minimum.y,
                          maximum.z - minimum.z};
    glm::dvec3 scale{1.0};
    for (int axis = 0; axis < 3; ++axis)
    {
        if (std::abs(size[axis]) > 1.0e-30)
            scale[axis] = 1.0 / size[axis];
    }
    AcGeMatrix3d result;
    result.m = glm::scale(glm::dmat4(1.0), scale) *
               glm::translate(glm::dmat4(1.0),
                              -glm::dvec3(minimum.x, minimum.y, minimum.z));
    return result;
}

void applyMapper(acdb::Triangle &triangle, const AcGiMapper *mapper,
                 const std::array<AcGePoint3d, 3> &sourcePoints,
                 const AcGeMatrix3d *autoTransform = nullptr)
{
    if (mapper == nullptr)
        return;
    triangle.mapperUTiling =
        static_cast<std::uint8_t>(mapper->uTiling());
    triangle.mapperVTiling =
        static_cast<std::uint8_t>(mapper->vTiling());

    const bool hasInputCoordinates =
        triangle.hasMapMappingCoords || triangle.hasAllMappingCoords;
    if (!hasInputCoordinates &&
        mapper->projection() == AcGiMapper::kInheritProjection)
        return;

    const AcGeVector3d normal =
        (sourcePoints[1] - sourcePoints[0])
            .crossProduct(sourcePoints[2] - sourcePoints[0]);
    const double absX = std::abs(normal.x);
    const double absY = std::abs(normal.y);
    const double absZ = std::abs(normal.z);
    const auto project = [&](const AcGePoint3d &point) {
        switch (mapper->projection())
        {
        case AcGiMapper::kBox:
            if (absX >= absY && absX >= absZ)
                return AcGePoint3d(point.y, point.z, 0.0);
            if (absY >= absZ)
                return AcGePoint3d(point.x, point.z, 0.0);
            return AcGePoint3d(point.x, point.y, 0.0);
        case AcGiMapper::kCylinder:
        {
            const double angle = std::atan2(point.y, point.x);
            return AcGePoint3d(angle / kTwoPi + 0.5, point.z, 0.0);
        }
        case AcGiMapper::kSphere:
        {
            const double length = std::sqrt(point.x * point.x +
                                            point.y * point.y +
                                            point.z * point.z);
            if (!(length > 1.0e-30))
                return AcGePoint3d(0.5, 0.5, 0.0);
            return AcGePoint3d(
                std::atan2(point.y, point.x) / kTwoPi + 0.5,
                std::asin(std::clamp(point.z / length, -1.0, 1.0)) /
                        3.14159265358979323846 +
                    0.5,
                0.0);
        }
        case AcGiMapper::kInheritProjection:
        case AcGiMapper::kPlanar:
        default:
            return AcGePoint3d(point.x, point.y, 0.0);
        }
    };

    std::array<glm::vec2, 3> uv{};
    for (size_t index = 0; index < uv.size(); ++index)
    {
        AcGePoint3d coordinates;
        if (triangle.hasMapMappingCoords)
            coordinates = triangle.mapMappingCoords[index];
        else if (triangle.hasAllMappingCoords)
            coordinates = triangle.allMappingCoords[index];
        else
            coordinates = project(sourcePoints[index]);

        if (!hasInputCoordinates &&
            mapper->projection() != AcGiMapper::kInheritProjection)
            coordinates = project(sourcePoints[index]);
        const AcGeMatrix3d transform =
            autoTransform == nullptr
                ? mapper->transform()
                : mapper->transform() * *autoTransform;
        coordinates = transformBy(coordinates, transform);
        triangle.mapMappingCoords[index] = coordinates;
        uv[index] = glm::vec2(static_cast<float>(coordinates.x),
                              static_cast<float>(coordinates.y));
    }
    triangle.hasMapMappingCoords = true;
    triangle.textureCoords = uv;
    triangle.hasTextureCoords = true;
}

bool validMappingCoords(const AcGiVertexData *vertexData, size_t vertexCount)
{
    if (vertexData == nullptr)
        return true;
    for (const AcGiVertexData::MapChannel channel :
         {AcGiVertexData::kAllChannels, AcGiVertexData::kMapChannels})
    {
        const AcGePoint3d *coords = vertexData->mappingCoords(channel);
        if (coords == nullptr)
            continue;
        for (size_t index = 0; index < vertexCount; ++index)
            if (!std::isfinite(coords[index].x) ||
                !std::isfinite(coords[index].y) ||
                !std::isfinite(coords[index].z))
                return false;
    }
    return true;
}

void applyTriangleNormal(acdb::Triangle &triangle,
                         const AcGeVector3d *normal = nullptr)
{
    AcGeVector3d value;
    if (normal != nullptr)
        value = *normal;
    else
        value = glm::cross(triangle.b - triangle.a, triangle.c - triangle.a);
    const double lengthSq = value.lengthSq();
    if (!(lengthSq > 1.0e-24) || !std::isfinite(lengthSq))
        return;
    value = value.normal();
    triangle.vertexNormals = {value, value, value};
    triangle.hasVertexNormals = true;
}

bool validNormals(const std::vector<AcGeVector3d> &normals)
{
    return std::all_of(normals.begin(), normals.end(),
                       [](const AcGeVector3d &normal) {
                           const double lengthSq = normal.lengthSq();
                           return std::isfinite(normal.x) &&
                                  std::isfinite(normal.y) &&
                                  std::isfinite(normal.z) &&
                                  lengthSq > 1.0e-24 &&
                                  std::isfinite(lengthSq);
                       });
}
} // namespace

// ---- AcGiCommonDraw state implementations ----

namespace
{

// AcGiContext view over the draw context's state (ObjectARX subset).
class GiDrawContext final : public AcGiContext
{
public:
    explicit GiDrawContext(const AcGiWorldDraw *draw) : draw_(draw) {}

    AcGiRegenType regenType() const override
    {
        return draw_->regenType();
    }
    Adesk::Boolean isDragging() const override { return draw_->isDragging(); }
    Adesk::Boolean isPsOut() const override { return false; }
    Adesk::Boolean isPlotGeneration() const override { return false; }
    acdb::AcDbDatabase *database() const override
    {
        return draw_->gsContext().database();
    }
    bool isBoundaryClipping() const override
    {
        return draw_->hasActiveClipBoundary();
    }
    bool isNesting() const override { return draw_->nestingDepth() > 0; }
    AcCmEntityColor effectiveColor() const override
    {
        return AcCmEntityColor(draw_->gsSubEntityTraits().traits().color);
    }
    double byBlockLineWeight() const override
    {
        return draw_->gsSubEntityTraits().traits().lineWeight;
    }
    bool supportsTrueTypeText() override { return true; }
    bool supportsOwnerDraw() override { return true; }
    bool supportsFillTrait() const override { return true; }
    bool supportsCurveTrait() const override { return true; }

private:
    const AcGiWorldDraw *draw_;
};

} // namespace

AcGiContext *AcGiWorldDraw::context()
{
    if (!commonContext_)
        commonContext_ = std::make_unique<GiDrawContext>(this);
    return commonContext_.get();
}

double AcGiWorldDraw::deviation(AcGiDeviationType type,
                                const AcGePoint3d &point) const
{
    (void)type;
    if (gsContext().hasViewportFrame())
    {
        AcGiViewport viewport(gsContext().viewportFrame());
        AcGePoint2d pixels;
        if (viewport.getNumPixelsInUnitSquare(point, pixels))
        {
            const double pixelsPerUnit = std::max(pixels.x, pixels.y);
            if (pixelsPerUnit > 1.0e-12)
                return std::max(1.0e-9, 0.25 / pixelsPerUnit);
        }
    }
    return gsContext().deviation(1.0);
}

// ---- AcGiViewportDraw raster-image / owner-draw / viewport extras ----

Adesk::Boolean AcGiViewportDraw::rasterImageDc(
    const AcGePoint3d &origin, const AcGeVector3d &u, const AcGeVector3d &v,
    const AcGiImageBGRA32 &imageSource, TransparencyMode transparencyMode)
{
    if (!gsContext().hasViewportFrame())
        return true;
    AcGeMatrix3d worldToModel;
    getWorldToModelTransform(worldToModel);
    const auto toModelWorld = [&](const AcGePoint3d &device) {
        return transformBy(gsContext().dcToWorld(device), worldToModel);
    };
    const AcGePoint3d worldOrigin = toModelWorld(origin);
    const AcGeVector3d worldU = toModelWorld(origin + u) - worldOrigin;
    const AcGeVector3d worldV = toModelWorld(origin + v) - worldOrigin;
    return image(imageSource, worldOrigin, worldU, worldV, transparencyMode);
}

Adesk::Boolean AcGiViewportDraw::rasterImageDc(
    const AcGePoint3d &origin, const AcGeVector3d &u, const AcGeVector3d &v,
    const acdb::AcDbObjectId &imageEntityId, ImageSource source,
    Adesk::UInt32 imageWidth, Adesk::UInt32 imageHeight,
    Adesk::Int16 imageColorDepth, bool transparency)
{
    (void)origin;
    (void)u;
    (void)v;
    (void)source;
    (void)imageColorDepth;
    if (!rasterImageResolver_)
        return true; // no consumption protocol installed by the host
    const AcGiImageBGRA32 *resolved = rasterImageResolver_(imageEntityId);
    if (resolved == nullptr || resolved->width() != imageWidth ||
        resolved->height() != imageHeight)
        return true;
    return rasterImageDc(origin, u, v, *resolved,
                         transparency ? kTransparency8Bit
                                      : kTransparencyOff);
}

Adesk::UInt32 AcGiViewportDraw::sequenceNumber() const
{
    return gsContext().hasViewportFrame()
               ? static_cast<Adesk::UInt32>(std::max(
                     0, gsContext().viewportFrame().viewportNumber))
               : 0;
}

Adesk::Boolean AcGiViewportDraw::isValidId(Adesk::ULongPtr acgiId) const
{
    // The context accepts exactly the ids it minted through mintAcgiId().
    return acgiId != 0 && acgiId <= gsContext().mintedAcgiIdCount();
}

acdb::AcDbObjectId AcGiViewportDraw::viewportObjectId() const
{
    // The host binds the AcDbViewportTableRecord id through
    // AcGsView::setViewportObjectId / ViewportFrame::viewportObjectId.
    return gsContext().hasViewportFrame()
               ? gsContext().viewportFrame().viewportObjectId
               : acdb::AcDbObjectId{};
}

bool AcGiWorldDraw::worldLine(const AcGePoint3d &from, const AcGePoint3d &to)
{
    AcGePoint3d start = apply(from);
    AcGePoint3d end = apply(to);
    for (const auto &[clippedStart, clippedEnd] : clipSegment(start, end))
    {
        acdb::Stroke &stroke = sink().addStroke();
        stroke.points = {clippedStart, clippedEnd};
        applyTraits(stroke);
    }
    return false;
}

bool AcGiWorldDraw::worldPolyline(const AcGePoint3d *points, int count,
                                  bool closed)
{
    if (!points || count < 2)
        return true;
    if (!clipStack_.empty())
    {
        const int segmentCount = closed ? count : count - 1;
        for (int index = 0; index < segmentCount; ++index)
            worldLine(points[index], points[(index + 1) % count]);
        return false;
    }
    acdb::Stroke &stroke = sink().addStroke(closed);
    stroke.points.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
        stroke.points.push_back(apply(points[i]));
    applyTraits(stroke);
    return false;
}

bool AcGiWorldDraw::polygon(Adesk::UInt32 pointCount,
                            const AcGePoint3d *vertexList)
{
    if (vertexList == nullptr || pointCount < 2 ||
        pointCount > static_cast<Adesk::UInt32>(INT_MAX))
        return true;

    if (fillType() == kAcGiFillAlways && pointCount >= 3)
    {
        const std::vector<AcGePoint3d> vertices(vertexList,
                                                 vertexList + pointCount);
        for (const auto &indices : triangulatePolygon(vertices))
            worldTriangle(vertices[indices[0]], vertices[indices[1]],
                          vertices[indices[2]]);
    }
    return worldPolyline(vertexList, static_cast<int>(pointCount), true);
}

bool AcGiWorldDraw::polyPolygon(
    const Adesk::UInt32 numPolygonIndices,
    const Adesk::UInt32 *numPolygonPositions,
    const AcGePoint3d *polygonPositions,
    const Adesk::UInt32 *numPolygonPoints,
    const AcGePoint3d *polygonPoints,
    const AcCmEntityColor *outlineColors,
    const AcGiLineType *outlineTypes,
    const AcCmEntityColor *fillColors,
    const AcCmTransparency *fillOpacities)
{
    if (numPolygonIndices == 0 || numPolygonPositions == nullptr ||
        polygonPositions == nullptr || numPolygonPoints == nullptr ||
        polygonPoints == nullptr)
        return true;

    size_t totalPositions = 0;
    size_t totalPoints = 0;
    for (Adesk::UInt32 type = 0; type < numPolygonIndices; ++type)
    {
        const size_t instanceCount = numPolygonPositions[type];
        const size_t vertexCount = numPolygonPoints[type];
        if ((instanceCount != 0 && vertexCount < 3) ||
            vertexCount > static_cast<size_t>(INT_MAX) ||
            instanceCount >
                std::numeric_limits<size_t>::max() - totalPositions ||
            vertexCount >
                std::numeric_limits<size_t>::max() - totalPoints)
            return true;
        totalPositions += instanceCount;
        totalPoints += vertexCount;
    }
    (void)totalPositions;
    (void)totalPoints;

    const acgs::DrawTraits originalTraits =
        draw_.subEntityTraits().traits();
    size_t positionIndex = 0;
    size_t pointIndex = 0;
    for (Adesk::UInt32 type = 0; type < numPolygonIndices; ++type)
    {
        const Adesk::UInt32 instanceCount = numPolygonPositions[type];
        const Adesk::UInt32 vertexCount = numPolygonPoints[type];
        glm::vec4 fillColor = originalTraits.color;
        if (fillColors != nullptr)
            fillColor = fillColors[type].renderColor();
        if (fillOpacities != nullptr &&
            fillOpacities[type].isByAlpha())
            fillColor.a *=
                static_cast<float>(fillOpacities[type].alpha()) / 255.0f;

        for (Adesk::UInt32 instance = 0; instance < instanceCount; ++instance)
        {
            std::vector<AcGePoint3d> vertices;
            vertices.reserve(vertexCount);
            const AcGeVector3d insertion = polygonPositions[positionIndex++].asVector();
            for (Adesk::UInt32 vertex = 0; vertex < vertexCount; ++vertex)
                vertices.push_back(polygonPoints[pointIndex + vertex] + insertion);

            if (fillColor.a > 0.0f)
            {
                draw_.subEntityTraits().setColor(fillColor);
                for (const auto &indices : triangulatePolygon(vertices))
                    worldTriangle(vertices[indices[0]], vertices[indices[1]],
                                  vertices[indices[2]]);
            }

            glm::vec4 outlineColor = originalTraits.color;
            if (outlineColors != nullptr)
                outlineColor = outlineColors[type].renderColor();
            draw_.subEntityTraits().setColor(outlineColor);
            if (outlineTypes != nullptr && outlineTypes[type].name != nullptr)
                draw_.subEntityTraits().setLineType(outlineTypes[type].name);
            worldPolyline(vertices.data(), static_cast<int>(vertices.size()), true);
        }
        pointIndex += vertexCount;
        draw_.subEntityTraits().setFrom(originalTraits);
    }
    draw_.subEntityTraits().setFrom(originalTraits);
    return false;
}

bool AcGiWorldDraw::image(const AcGiImageBGRA32 &imageSource,
                          const AcGePoint3d &position,
                          const AcGeVector3d &u, const AcGeVector3d &v,
                          TransparencyMode transparencyMode)
{
    constexpr size_t kMaximumRasterPixels = 65536;
    const size_t width = imageSource.width();
    const size_t height = imageSource.height();
    const AcGiPixelBGRA32 *pixels = imageSource.image();
    if (width == 0 || height == 0 || pixels == nullptr ||
        width > std::numeric_limits<std::uint32_t>::max() ||
        height > std::numeric_limits<std::uint32_t>::max() ||
        width > std::numeric_limits<size_t>::max() / height ||
        width * height > kMaximumRasterPixels ||
        width * height > std::numeric_limits<size_t>::max() / 4)
        return true;

    const acgs::DrawTraits originalTraits =
        draw_.subEntityTraits().traits();
    acdb::RasterImage raster;
    raster.width = static_cast<std::uint32_t>(width);
    raster.height = static_cast<std::uint32_t>(height);
    raster.bgra.resize(width * height * 4);
    for (size_t i = 0; i < width * height; ++i)
    {
        const AcGiPixelBGRA32 &pixel = pixels[i];
        std::uint8_t alpha = 255;
        if (transparencyMode == kTransparency1Bit)
            alpha = pixel.a == 255 ? 255 : 0;
        else if (transparencyMode == kTransparency8Bit)
            alpha = pixel.a;
        raster.bgra[i * 4 + 0] = pixel.b;
        raster.bgra[i * 4 + 1] = pixel.g;
        raster.bgra[i * 4 + 2] = pixel.r;
        raster.bgra[i * 4 + 3] = alpha;
    }
    raster.contentHash = 14695981039346656037ull;
    for (const std::uint8_t byte : raster.bgra)
    {
        raster.contentHash ^= byte;
        raster.contentHash *= 1099511628211ull;
    }
    auto &geometry = sink().geometry();
    geometry.images.push_back(std::move(raster));
    const std::uint32_t imageIndex =
        static_cast<std::uint32_t>(geometry.images.size());

    const AcGePoint3d p00 = position;
    const AcGePoint3d p10 = position + u;
    const AcGePoint3d p11 = position + u + v;
    const AcGePoint3d p01 = position + v;
    draw_.subEntityTraits().setColor(glm::vec4(1.0f));
    size_t firstFill = geometry.fills.size();
    worldTriangle(p00, p10, p11);
    worldTriangle(p00, p11, p01);
    if (geometry.fills.size() >= firstFill + 2)
    {
        acdb::Triangle &first = geometry.fills[firstFill];
        first.imageIndex = imageIndex;
        first.hasTextureCoords = true;
        first.textureCoords = {glm::vec2(0, 0), glm::vec2(1, 0),
                               glm::vec2(1, 1)};
        acdb::Triangle &second = geometry.fills[firstFill + 1];
        second.imageIndex = imageIndex;
        second.hasTextureCoords = true;
        second.textureCoords = {glm::vec2(0, 0), glm::vec2(1, 1),
                                glm::vec2(0, 1)};
    }

    for (size_t row = 0; row < height; ++row)
    {
        for (size_t column = 0; column < width; ++column)
        {
            const AcGiPixelBGRA32 &pixel = pixels[row * width + column];
            float alpha = 1.0f;
            if (transparencyMode == kTransparency1Bit)
                alpha = pixel.a == 255 ? 1.0f : 0.0f;
            else if (transparencyMode == kTransparency8Bit)
                alpha = static_cast<float>(pixel.a) / 255.0f;
            if (alpha == 0.0f)
                continue;

            draw_.subEntityTraits().setColor(
                glm::vec4(static_cast<float>(pixel.r) / 255.0f,
                          static_cast<float>(pixel.g) / 255.0f,
                          static_cast<float>(pixel.b) / 255.0f, alpha));
            const double x0 = static_cast<double>(column) / width;
            const double x1 = static_cast<double>(column + 1) / width;
            const double y0 = static_cast<double>(row) / height;
            const double y1 = static_cast<double>(row + 1) / height;
            const AcGePoint3d p00 = position + u * x0 + v * y0;
            const AcGePoint3d p10 = position + u * x1 + v * y0;
            const AcGePoint3d p11 = position + u * x1 + v * y1;
            const AcGePoint3d p01 = position + u * x0 + v * y1;
            firstFill = geometry.fills.size();
            worldTriangle(p00, p10, p11);
            if (geometry.fills.size() > firstFill)
            {
                acdb::Triangle &triangle = geometry.fills[firstFill];
                triangle.imageIndex = imageIndex;
                triangle.imageFallback = true;
            }
            firstFill = geometry.fills.size();
            worldTriangle(p00, p11, p01);
            if (geometry.fills.size() > firstFill)
            {
                acdb::Triangle &triangle = geometry.fills[firstFill];
                triangle.imageIndex = imageIndex;
                triangle.imageFallback = true;
            }
        }
    }
    draw_.subEntityTraits().setFrom(originalTraits);
    return false;
}

bool AcGiWorldDraw::edge(const std::vector<AcGiEdge2d> &edges)
{
    if (edges.empty())
        return true;

    for (const AcGiEdge2d &edge : edges)
    {
        std::vector<AcGePoint3d> points;
        std::visit(
            [&](const auto &curve) {
                using Curve = std::decay_t<decltype(curve)>;
                if constexpr (std::is_same_v<Curve, AcGeLineSeg2d>)
                {
                    points.emplace_back(curve.startPoint().x,
                                        curve.startPoint().y, 0.0);
                    points.emplace_back(curve.endPoint().x,
                                        curve.endPoint().y, 0.0);
                }
                else
                {
                    double sweep = 0.0;
                    double radius = 0.0;
                    if constexpr (std::is_same_v<Curve, AcGeCircArc2d>)
                    {
                        sweep = curve.sweep() *
                                (curve.isClockWise() ? -1.0 : 1.0);
                        radius = curve.radius();
                    }
                    else
                    {
                        sweep = curve.endAng() - curve.startAng();
                        radius = curve.majorRadius();
                    }
                    const acdb::TesselationOptions options =
                        gsContext().optionsFor(radius);
                    if (!(options.angleStep > 0.0) ||
                        !std::isfinite(options.angleStep) ||
                        !std::isfinite(sweep) || sweep == 0.0)
                        return;
                    const int maxSegments =
                        std::clamp(options.maxSegments, 1, 65536);
                    const int minSegments =
                        std::clamp(options.minSegments, 1, maxSegments);
                    const double desiredSegments =
                        std::ceil(std::abs(sweep) / options.angleStep);
                    const int segments =
                        desiredSegments >= maxSegments
                            ? maxSegments
                            : std::max(minSegments,
                                       static_cast<int>(desiredSegments));
                    points.reserve(static_cast<size_t>(segments) + 1);
                    for (int index = 0; index <= segments; ++index)
                    {
                        const double t =
                            static_cast<double>(index) / segments;
                        AcGePoint2d point;
                        if constexpr (std::is_same_v<Curve, AcGeCircArc2d>)
                        {
                            const double angle =
                                curve.startAng() + sweep * t;
                            point = curve.pointAtAngle(angle);
                        }
                        else
                        {
                            const double angle =
                                curve.startAng() + sweep * t;
                            point = curve.pointAt(angle);
                        }
                        points.emplace_back(point.x, point.y, 0.0);
                    }
                }
            },
            edge.curve);
        if (points.size() < 2)
            return true;
        if (worldPolyline(points.data(), static_cast<int>(points.size()), false))
            return true;
    }
    return false;
}


bool AcGiWorldDraw::behaviorNeedsViewport(
    AcGiPositionTransformBehavior behavior) const
{
    return behavior != kAcGiWorldPosition;
}

bool AcGiWorldDraw::behaviorNeedsViewport(
    AcGiScaleTransformBehavior behavior) const
{
    return behavior != kAcGiWorldScale;
}

AcGeVector3d AcGiWorldDraw::viewportWorldOffset(
    const AcGePoint3d &offset) const
{
    if (!gsContext().hasViewportFrame())
        return offset.asVector();
    return gsContext().dcToWorldVector(AcGePoint3d{}, offset);
}

AcGeVector3d AcGiWorldDraw::viewportWorldScaleAxis(
    double value, size_t axis) const
{
    if (!gsContext().hasViewportFrame() || value == 0.0)
        return AcGeVector3d();
    AcGePoint3d unit;
    if (axis == 0)
        unit = AcGePoint3d(1.0, 0.0, 0.0);
    else if (axis == 1)
        unit = AcGePoint3d(0.0, 1.0, 0.0);
    else
        unit = AcGePoint3d(0.0, 0.0, 1.0);
    return gsContext().dcToWorldVector(AcGePoint3d{}, unit) * value;
}

AcGeMatrix3d AcGiWorldDraw::screenOrientationMatrix() const
{
    if (!gsContext().hasViewportFrame())
        return AcGeMatrix3d();
    const AcGeMatrix3d eyeToWorld = gsContext().viewportFrame().eyeToWorld;
    const AcGeVector3d xAxis = transformBy(AcGeVector3d::kXAxis, eyeToWorld);
    const AcGeVector3d yAxis = transformBy(AcGeVector3d::kYAxis, eyeToWorld);
    const AcGeVector3d zAxis = transformBy(AcGeVector3d::kZAxis, eyeToWorld);
    AcGeMatrix3d result;
    result.m[0] = glm::dvec4(xAxis.x, xAxis.y, xAxis.z, 0.0);
    result.m[1] = glm::dvec4(yAxis.x, yAxis.y, yAxis.z, 0.0);
    result.m[2] = glm::dvec4(zAxis.x, zAxis.y, zAxis.z, 0.0);
    return result;
}

AcGeMatrix3d AcGiWorldDraw::pushPositionTransform(
    AcGiPositionTransformBehavior behavior, const AcGePoint3d &offset)
{
    AcGeVector3d translation = offset.asVector();
    if (behaviorNeedsViewport(behavior))
    {
        const AcGeVector3d screenOffset = viewportWorldOffset(offset);
        translation = behavior == kAcGiWorldWithScreenOffsetPosition
                          ? translation + screenOffset
                          : screenOffset;
    }
    const AcGeMatrix3d matrix =
        AcGeMatrix3d::setToTranslation(translation);
    pushModelTransform(matrix);
    return matrix;
}

AcGeMatrix3d AcGiWorldDraw::pushScaleTransform(
    AcGiScaleTransformBehavior behavior, const AcGePoint3d &extents)
{
    if (!behaviorNeedsViewport(behavior) || !gsContext().hasViewportFrame())
    {
        const AcGeMatrix3d matrix = AcGeMatrix3d::setToScaling(
            AcGeVector3d(extents.x, extents.y, extents.z));
        pushModelTransform(matrix);
        return matrix;
    }

    AcGeMatrix3d result;
    result.m = glm::dmat4(1.0);
    result.m[0][0] = viewportWorldScaleAxis(extents.x, 0).length();
    result.m[1][1] = viewportWorldScaleAxis(extents.y, 1).length();
    result.m[2][2] = viewportWorldScaleAxis(extents.z, 2).length();
    pushModelTransform(result);
    return result;
}

AcGeMatrix3d AcGiWorldDraw::pushOrientationTransform(
    AcGiOrientationTransformBehavior behavior)
{
    AcGeMatrix3d matrix;
    if (behavior == kAcGiScreenOrientation ||
        behavior == kAcGiZAxisOrientation)
        matrix = screenOrientationMatrix();
    pushModelTransform(matrix);
    return matrix;
}

Adesk::Boolean AcGiViewportDraw::polylineEye(
    const Adesk::UInt32 nbPoints, const AcGePoint3d *vertexList)
{
    if (vertexList == nullptr || nbPoints < 2 ||
        !gsContext().hasViewportFrame())
        return true;
    std::vector<AcGePoint3d> points;
    points.reserve(nbPoints);
    for (Adesk::UInt32 index = 0; index < nbPoints; ++index)
    {
        points.push_back(modelPointFromWorld(
            gsContext().eyeToWorld(vertexList[index])));
    }
    return worldPolyline(points.data(), static_cast<int>(points.size()),
                         false);
}

Adesk::Boolean AcGiViewportDraw::polygonEye(
    const Adesk::UInt32 nbPoints, const AcGePoint3d *vertexList)
{
    if (vertexList == nullptr || nbPoints < 2 ||
        !gsContext().hasViewportFrame())
        return true;
    std::vector<AcGePoint3d> points;
    points.reserve(nbPoints);
    for (Adesk::UInt32 index = 0; index < nbPoints; ++index)
    {
        points.push_back(modelPointFromWorld(
            gsContext().eyeToWorld(vertexList[index])));
    }
    return worldPolyline(points.data(), static_cast<int>(points.size()),
                         true);
}

Adesk::Boolean AcGiViewportDraw::polylineDc(
    const Adesk::UInt32 nbPoints, const AcGePoint3d *vertexList)
{
    if (vertexList == nullptr || nbPoints < 2 ||
        !gsContext().hasViewportFrame())
        return true;
    std::vector<AcGePoint3d> points;
    points.reserve(nbPoints);
    for (Adesk::UInt32 index = 0; index < nbPoints; ++index)
    {
        points.push_back(modelPointFromWorld(
            gsContext().dcToWorld(vertexList[index])));
    }
    return worldPolyline(points.data(), static_cast<int>(points.size()),
                         false);
}

Adesk::Boolean AcGiViewportDraw::polygonDc(
    const Adesk::UInt32 nbPoints, const AcGePoint3d *vertexList)
{
    if (vertexList == nullptr || nbPoints < 2 ||
        !gsContext().hasViewportFrame())
        return true;
    std::vector<AcGePoint3d> points;
    points.reserve(nbPoints);
    for (Adesk::UInt32 index = 0; index < nbPoints; ++index)
    {
        points.push_back(modelPointFromWorld(
            gsContext().dcToWorld(vertexList[index])));
    }
    return worldPolyline(points.data(), static_cast<int>(points.size()),
                         true);
}

Adesk::Boolean AcGiViewportDraw::ownerDrawDc(
    Adesk::Int32 viewportNumber, Adesk::Int32 left, Adesk::Int32 top,
    Adesk::Int32 right, Adesk::Int32 bottom, AcGiOwnerDraw *ownerDraw)
{
    (void)ownerDraw;
    acgs::OwnerDrawCommand command;
    command.kind = acgs::OwnerDrawCommand::Kind::Dc;
    command.viewportNumber = viewportNumber;
    command.left = left;
    command.top = top;
    command.right = right;
    command.bottom = bottom;
    gsContext().ownerDrawCommands().push_back(command);
    return false; // stored: the host flushes pending owner-draw requests
}

Adesk::Boolean AcGiViewportDraw::ownerDraw3d(
    AcGePoint3d &minimum, AcGePoint3d &maximum,
    AcGiOwnerDraw3d *ownerDraw)
{
    (void)ownerDraw;
    acgs::OwnerDrawCommand command;
    command.kind = acgs::OwnerDrawCommand::Kind::World3d;
    command.minimum = minimum;
    command.maximum = maximum;
    gsContext().ownerDrawCommands().push_back(command);
    return false; // stored: the host flushes pending owner-draw requests
}

namespace
{
int analyticSegmentCount(double sweep, const acdb::TesselationOptions &options)
{
    const int maxSegments = std::clamp(options.maxSegments, 1, 65536);
    const int minSegments = std::clamp(options.minSegments, 1, maxSegments);
    if (!(std::abs(sweep) > 0.0) || !(options.angleStep > 0.0) ||
        !std::isfinite(sweep) || !std::isfinite(options.angleStep))
        return std::max(minSegments, 32);
    const double desired = std::ceil(std::abs(sweep) / options.angleStep);
    return desired >= maxSegments
               ? maxSegments
               : std::max(minSegments, static_cast<int>(desired));
}

double analyticCurveRadius(const AcGeCurve3d &curve)
{
    if (const auto *circle = dynamic_cast<const AcGeCircle3d *>(&curve))
        return circle->radius;
    if (const auto *arc = dynamic_cast<const AcGeCircArc3d *>(&curve))
        return arc->radius;
    if (const auto *ellipse =
            dynamic_cast<const AcGeEllipArc3d *>(&curve))
        return std::max(ellipse->majorRadius(), ellipse->minorRadius());
    return 0.0;
}

double analyticCurveRadius(const AcGeCurve2d &curve)
{
    if (const auto *arc = dynamic_cast<const AcGeCircArc2d *>(&curve))
        return arc->radius();
    if (const auto *ellipse =
            dynamic_cast<const AcGeEllipArc2d *>(&curve))
        return ellipse->majorRadius();
    return 0.0;
}

std::vector<AcGePoint3d> sampleBaseCurve3d(
    const AcGeCurve3d &curve, const acdb::TesselationOptions &options)
{
    if (const auto *line = dynamic_cast<const AcGeLineSeg3d *>(&curve))
        return {line->startPoint(), line->endPoint()};

    const double start = curve.startParam();
    const double end = curve.endParam();
    const int segments = analyticSegmentCount(end - start, options);
    std::vector<AcGePoint3d> points;
    points.reserve(static_cast<size_t>(segments) + 1);
    for (int index = 0; index <= segments; ++index)
    {
        points.push_back(curve.pointAt(
            start + (end - start) * (double(index) / segments)));
    }
    return points;
}

std::vector<AcGePoint3d> sampleBaseCurve2d(
    const AcGeCurve2d &curve, const acdb::TesselationOptions &options)
{
    if (const auto *line = dynamic_cast<const AcGeLineSeg2d *>(&curve))
    {
        const AcGePoint2d start = line->startPoint();
        const AcGePoint2d end = line->endPoint();
        return {AcGePoint3d(start.x, start.y, 0.0),
                AcGePoint3d(end.x, end.y, 0.0)};
    }

    const double start = curve.startParam();
    const double end = curve.endParam();
    const int segments = analyticSegmentCount(end - start, options);
    std::vector<AcGePoint3d> points;
    points.reserve(static_cast<size_t>(segments) + 1);
    for (int index = 0; index <= segments; ++index)
    {
        const AcGePoint2d point = curve.pointAt(
            start + (end - start) * (double(index) / segments));
        points.emplace_back(point.x, point.y, 0.0);
    }
    return points;
}
} // namespace

// Deviation-driven refinement for generic (non-analytic-radius) curves:
// midpoint subdivision while the curve midpoint strays from the chord by
// more than |tolerance|.  Bounded by depth and point budget so pathological
// curves cannot loop.
// Derivative-aware midpoint: weight the parametric midpoint by the tangent
// magnitudes so refinement approximates arc-length parameterization even
// when the curve's parameterization is uneven.  Falls back to the plain
// midpoint when the curve reports no tangents (generic base curves).
double adaptiveMidParam3d(const AcGeCurve3d &curve, double a, double b)
{
    AcGeVector3d ta;
    AcGeVector3d tb;
    if (curve.tangent(ta, a) && curve.tangent(tb, b))
    {
        const double la = ta.length();
        const double lb = tb.length();
        if (la > 1.0e-300 && lb > 1.0e-300)
        {
            const double mid = (a * lb + b * la) / (la + lb);
            if (mid > a && mid < b)
                return mid;
        }
    }
    return 0.5 * (a + b);
}

double adaptiveMidParam2d(const AcGeCurve2d &curve, double a, double b)
{
    AcGeVector2d ta;
    AcGeVector2d tb;
    if (curve.tangent(ta, a) && curve.tangent(tb, b))
    {
        const double la = ta.length();
        const double lb = tb.length();
        if (la > 1.0e-300 && lb > 1.0e-300)
        {
            const double mid = (a * lb + b * la) / (la + lb);
            if (mid > a && mid < b)
                return mid;
        }
    }
    return 0.5 * (a + b);
}

void appendAdaptiveCurve3d(const AcGeCurve3d &curve, double a,
                           const AcGePoint3d &pa, double b,
                           const AcGePoint3d &pb, double tolerance, int depth,
                           size_t maxPoints, std::vector<AcGePoint3d> &out)
{
    if (depth >= 12 || out.size() >= maxPoints)
        return;
    const double mid = adaptiveMidParam3d(curve, a, b);
    if (!(std::abs(b - a) > 0.0) || !std::isfinite(mid))
        return;
    const AcGePoint3d pm = curve.pointAt(mid);
    if (!std::isfinite(pm.x) || !std::isfinite(pm.y) || !std::isfinite(pm.z))
        return;
    const AcGePoint3d chordMid{(pa.x + pb.x) * 0.5, (pa.y + pb.y) * 0.5,
                               (pa.z + pb.z) * 0.5};
    if (!(pm.distanceTo(chordMid) > tolerance))
        return;
    appendAdaptiveCurve3d(curve, a, pa, mid, pm, tolerance, depth + 1,
                          maxPoints, out);
    out.push_back(pm);
    appendAdaptiveCurve3d(curve, mid, pm, b, pb, tolerance, depth + 1,
                          maxPoints, out);
}

void appendAdaptiveCurve2d(const AcGeCurve2d &curve, double a,
                           const AcGePoint3d &pa, double b,
                           const AcGePoint3d &pb, double tolerance, int depth,
                           size_t maxPoints, std::vector<AcGePoint3d> &out)
{
    if (depth >= 12 || out.size() >= maxPoints)
        return;
    const double mid = adaptiveMidParam2d(curve, a, b);
    if (!(std::abs(b - a) > 0.0) || !std::isfinite(mid))
        return;
    const AcGePoint2d pm2d = curve.pointAt(mid);
    if (!std::isfinite(pm2d.x) || !std::isfinite(pm2d.y))
        return;
    const AcGePoint3d pm(pm2d.x, pm2d.y, 0.0);
    const AcGePoint3d chordMid{(pa.x + pb.x) * 0.5, (pa.y + pb.y) * 0.5, 0.0};
    if (!(pm.distanceTo(chordMid) > tolerance))
        return;
    appendAdaptiveCurve2d(curve, a, pa, mid, pm, tolerance, depth + 1,
                          maxPoints, out);
    out.push_back(pm);
    appendAdaptiveCurve2d(curve, mid, pm, b, pb, tolerance, depth + 1,
                          maxPoints, out);
}

double coarseSampleExtent(const std::vector<AcGePoint3d> &points)
{
    double minX = 0.0, minY = 0.0, minZ = 0.0;
    double maxX = 0.0, maxY = 0.0, maxZ = 0.0;
    bool first = true;
    for (const AcGePoint3d &point : points)
    {
        if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
            !std::isfinite(point.z))
            continue;
        if (first)
        {
            minX = maxX = point.x;
            minY = maxY = point.y;
            minZ = maxZ = point.z;
            first = false;
            continue;
        }
        minX = std::min(minX, point.x);
        minY = std::min(minY, point.y);
        minZ = std::min(minZ, point.z);
        maxX = std::max(maxX, point.x);
        maxY = std::max(maxY, point.y);
        maxZ = std::max(maxZ, point.z);
    }
    if (first)
        return 0.0;
    return std::hypot(maxX - minX, maxY - minY, maxZ - minZ);
}

Adesk::Boolean AcGiWorldDraw::curve(const AcGeCurve3d *curve)
{
    if (curve == nullptr)
        return true;
    const double curveRadius = analyticCurveRadius(*curve);
    const acdb::TesselationOptions options =
        gsContext().optionsFor(curveRadius);
    std::vector<AcGePoint3d> points = sampleBaseCurve3d(*curve, options);
    if (points.size() < 2)
        return true;

    // Deviation-driven refinement: analytic radius curves already derive an
    // angle step from the viewport tolerance, while generic curves (splines,
    // composites) get true chord-error subdivision.  The coarse extent gives
    // a scale-relative fallback when no viewport density is known.
    const double extent = coarseSampleExtent(points);
    double tolerance = std::max(options.minSegmentLength, extent * 1.0e-3);
    const double pixelTolerance = gsContext().deviation(1.0);
    if (pixelTolerance > 0.0 && pixelTolerance < tolerance)
        tolerance = pixelTolerance;
    if (points.size() > 2 || curveRadius <= 0.0)
    {
        const size_t maxPoints = 65536;
        const double start = curve->startParam();
        const double end = curve->endParam();
        std::vector<AcGePoint3d> refined;
        refined.reserve(points.size());
        refined.push_back(points.front());
        for (size_t index = 1; index < points.size(); ++index)
        {
            const double tA =
                start + (end - start) * (double(index - 1) /
                                         double(points.size() - 1));
            const double tB =
                start + (end - start) * (double(index) /
                                         double(points.size() - 1));
            appendAdaptiveCurve3d(*curve, tA, refined.back(), tB, points[index],
                                  tolerance, 0, maxPoints, refined);
            refined.push_back(points[index]);
            if (refined.size() >= maxPoints)
                break;
        }
        points = std::move(refined);
    }
    points.erase(std::remove_if(points.begin(), points.end(),
                                [](const AcGePoint3d &point) {
                                    return !std::isfinite(point.x) ||
                                           !std::isfinite(point.y) ||
                                           !std::isfinite(point.z);
                                }),
                 points.end());
    if (points.size() < 2)
        return true;
    return worldPolyline(points.data(), static_cast<int>(points.size()),
                         false);
}

Adesk::Boolean AcGiWorldDraw::edge(const AcGeCurve2d *const *edges,
                                   Adesk::UInt32 nbEdges)
{
    if (edges == nullptr || nbEdges == 0)
        return true;

    for (Adesk::UInt32 index = 0; index < nbEdges; ++index)
    {
        if (edges[index] == nullptr)
            return true;
        const AcGeCurve2d &curve = *edges[index];
        const double curveRadius = analyticCurveRadius(curve);
        const acdb::TesselationOptions options =
            gsContext().optionsFor(curveRadius);
        std::vector<AcGePoint3d> points = sampleBaseCurve2d(curve, options);
        const double extent = coarseSampleExtent(points);
        double tolerance = std::max(options.minSegmentLength, extent * 1.0e-3);
        const double pixelTolerance = gsContext().deviation(1.0);
        if (pixelTolerance > 0.0 && pixelTolerance < tolerance)
            tolerance = pixelTolerance;
        if (points.size() > 2)
        {
            const size_t maxPoints = 65536;
            const double start = curve.startParam();
            const double end = curve.endParam();
            std::vector<AcGePoint3d> refined;
            refined.reserve(points.size());
            refined.push_back(points.front());
            for (size_t index = 1; index < points.size(); ++index)
            {
                const double tA =
                    start + (end - start) * (double(index - 1) /
                                             double(points.size() - 1));
                const double tB =
                    start + (end - start) * (double(index) /
                                             double(points.size() - 1));
                appendAdaptiveCurve2d(curve, tA, refined.back(), tB,
                                      points[index], tolerance, 0, maxPoints,
                                      refined);
                refined.push_back(points[index]);
                if (refined.size() >= maxPoints)
                    break;
            }
            points = std::move(refined);
        }
        points.erase(std::remove_if(points.begin(), points.end(),
                                    [](const AcGePoint3d &point) {
                                        return !std::isfinite(point.x) ||
                                               !std::isfinite(point.y) ||
                                               !std::isfinite(point.z);
                                    }),
                     points.end());
        if (points.size() < 2)
            return true;
        if (worldPolyline(points.data(),
                          static_cast<int>(points.size()), false))
            return true;
    }
    return false;
}

bool AcGiWorldDraw::worldTriangle(const AcGePoint3d &a, const AcGePoint3d &b,
                                  const AcGePoint3d &c, bool fillIs3DFace)
{
    for (const std::vector<AcGePoint3d> &clipped :
         clipPolygon({apply(a), apply(b), apply(c)}))
    {
        for (size_t index = 2; index < clipped.size(); ++index)
        {
            acdb::Triangle triangle{{}, clipped[0], clipped[index - 1],
                                    clipped[index]};
            applyTraits(triangle, fillIs3DFace);
            sink().appendTriangle(triangle);
        }
    }
    return false;
}

bool AcGiWorldDraw::worldPoint(const AcGePoint3d &position,
                               acge::PointDisplayStyle displayStyle)
{
    const acgs::DrawTraits &traits = draw_.subEntityTraits().traits();
    AcGePoint3d worldPosition = apply(position);
    if (!clipPoint(worldPosition))
        return false;
    acdb::TessellatedPoint point;
    point.location = worldPosition;
    point.common = traits.toEntityCommon();
    point.materialId = traits.materialId;
    point.pointSize =
        traits.lineWeight > 0.0 ? traits.lineWeight : 7.0;
    point.displayStyle = displayStyle;
    sink().appendPoint(point);
    return false;
}

bool AcGiWorldDraw::worldInfiniteLine(const AcGePoint3d &start,
                                      const AcGeVector3d &direction)
{
    const AcGeVector3d dir = direction.normal();
    if (dir.lengthSq() <= 0.0)
        return true;
    constexpr double kProxyLength = 1.0e6;
    if (!clipStack_.empty())
        return worldLine(start, start + dir * kProxyLength);
    acdb::Stroke &stroke = sink().addStroke(false);
    stroke.points = {apply(start),
                     apply(start + dir * kProxyLength)};
    stroke.semiInfinite = true;
    applyTraits(stroke);
    return false;
}

bool AcGiWorldDraw::pushClipBoundaryImpl(const AcGiClipBoundary *boundary)
{
    if (boundary == nullptr)
        return false;
    const std::vector<AcGePoint2d> &inputPoints =
        boundary->m_aptPoints.empty() ? boundary->points : boundary->m_aptPoints;
    if (inputPoints.size() < 2)
        return false;

    AcGeVector3d normal = boundary->m_vNormal;
    const double normalLengthSq = normal.lengthSq();
    if (!(normalLengthSq > 1.0e-24) || !std::isfinite(normalLengthSq))
        return false;

    std::vector<AcGePoint2d> points = inputPoints;
    if (points.size() == 2)
    {
        const double minX = std::min(points[0].x, points[1].x);
        const double minY = std::min(points[0].y, points[1].y);
        const double maxX = std::max(points[0].x, points[1].x);
        const double maxY = std::max(points[0].y, points[1].y);
        points = {{minX, minY}, {maxX, minY},
                  {maxX, maxY}, {minX, maxY}};
    }
    if (!normalizeClipPolygon(points))
        return false;

    ClipFrame frame;
    frame.boundary = std::move(points);
    frame.toClipSpace =
        boundary->m_xToClipSpace * boundary->m_xInverseBlockRefXForm;
    frame.clippingFront = boundary->m_bClippingFront;
    frame.clippingBack = boundary->m_bClippingBack;
    frame.frontClipZ = boundary->m_dFrontClipZ;
    frame.backClipZ = boundary->m_dBackClipZ;
    if ((frame.clippingFront && !std::isfinite(frame.frontClipZ)) ||
        (frame.clippingBack && !std::isfinite(frame.backClipZ)))
        return false;
    const long double areaTwice = polygonAreaTwice(frame.boundary);
    if (!std::isfinite(areaTwice) || areaTwice == 0.0L)
        return false;
    frame.winding = areaTwice > 0.0L ? 1.0 : -1.0;
    int turnSign = 0;
    for (size_t index = 0; index < frame.boundary.size(); ++index)
    {
        const AcGePoint2d &previous =
            frame.boundary[(index + frame.boundary.size() - 1) %
                           frame.boundary.size()];
        const AcGePoint2d &current = frame.boundary[index];
        const AcGePoint2d &next =
            frame.boundary[(index + 1) % frame.boundary.size()];
        const long double turn = cross2d(previous, current, next);
        if (std::abs(turn) <= 1.0e-10L)
            continue;
        const int currentSign = turn > 0.0L ? 1 : -1;
        if (turnSign != 0 && currentSign != turnSign)
            frame.convex = false;
        turnSign = currentSign;
    }
    std::vector<AcGePoint3d> boundary3d;
    boundary3d.reserve(frame.boundary.size());
    for (const AcGePoint2d &point : frame.boundary)
        boundary3d.emplace_back(point.x, point.y, 0.0);
    frame.triangles = triangulatePolygon(boundary3d);
    if (frame.triangles.empty())
        return false;
    std::vector<AcGePoint3d> displayBoundary;
    if (boundary->m_bDrawBoundary)
    {
        const AcGeMatrix3d clipToWorld = frame.toClipSpace.inverted();
        displayBoundary.reserve(frame.boundary.size());
        for (const AcGePoint2d &point : frame.boundary)
        {
            const AcGePoint3d world =
                transformBy(AcGePoint3d(point.x, point.y, 0.0), clipToWorld);
            if (!std::isfinite(world.x) || !std::isfinite(world.y) ||
                !std::isfinite(world.z))
            {
                displayBoundary.clear();
                break;
            }
            displayBoundary.push_back(world);
        }
    }
    clipStack_.push_back(std::move(frame));
    if (displayBoundary.size() >= 3)
    {
        acdb::Stroke &stroke = sink().addStroke(true);
        stroke.points.reserve(displayBoundary.size());
        for (const AcGePoint3d &point : displayBoundary)
            stroke.points.emplace_back(glm::dvec3(point));
        applyTraits(stroke);
    }
    return true;
}

void AcGiWorldDraw::popClipBoundaryImpl()
{
    if (!clipStack_.empty())
        clipStack_.pop_back();
}

bool AcGiWorldDraw::clipPoint(AcGePoint3d &point) const
{
    for (const ClipFrame &frame : clipStack_)
    {
        AcGePoint3d clipPoint;
        if (!frame.coordinates(point, clipPoint))
            return false;
        if ((frame.clippingFront &&
             clipPoint.z > frame.frontClipZ + 1.0e-10) ||
            (frame.clippingBack &&
             clipPoint.z < frame.backClipZ - 1.0e-10))
            return false;
        const AcGePoint2d projected(clipPoint.x, clipPoint.y);
        if (!pointInSimplePolygon(frame.boundary, projected))
            return false;
    }
    return true;
}

std::vector<std::pair<AcGePoint3d, AcGePoint3d>>
AcGiWorldDraw::clipSegment(const AcGePoint3d &start,
                           const AcGePoint3d &end) const
{
    const AcGeVector3d direction = end - start;
    std::vector<std::pair<double, double>> ranges{{0.0, 1.0}};
    for (const ClipFrame &frame : clipStack_)
    {
        std::vector<std::pair<double, double>> clippedRanges;
        for (const auto &[rangeStart, rangeEnd] : ranges)
        {
            const AcGePoint3d worldStart = start + direction * rangeStart;
            const AcGePoint3d worldEnd = start + direction * rangeEnd;
            AcGePoint3d clipStart;
            AcGePoint3d clipEnd;
            if (!frame.coordinates(worldStart, clipStart) ||
                !frame.coordinates(worldEnd, clipEnd))
                continue;
            const AcGeVector2d segmentDirection(clipEnd.x - clipStart.x,
                                                clipEnd.y - clipStart.y);
            std::vector<double> breaks{0.0, 1.0};
            for (size_t index = 0; index < frame.boundary.size(); ++index)
            {
                const AcGePoint2d &edgeStart = frame.boundary[index];
                const AcGePoint2d &edgeEnd =
                    frame.boundary[(index + 1) % frame.boundary.size()];
                const AcGeVector2d edgeDirection =
                    edgeEnd - edgeStart;
                const AcGeVector2d offset =
                    edgeStart - AcGePoint2d(clipStart.x, clipStart.y);
                const double denominator =
                    segmentDirection.x * edgeDirection.y -
                    segmentDirection.y * edgeDirection.x;
                if (std::abs(denominator) > 1.0e-14)
                {
                    const double t =
                        (offset.x * edgeDirection.y -
                         offset.y * edgeDirection.x) /
                        denominator;
                    const double u =
                        (offset.x * segmentDirection.y -
                         offset.y * segmentDirection.x) /
                        denominator;
                    if (t > 1.0e-12 && t < 1.0 - 1.0e-12 &&
                        u >= -1.0e-12 && u <= 1.0 + 1.0e-12)
                        breaks.push_back(t);
                }
                else if (std::abs(offset.x * segmentDirection.y -
                                  offset.y * segmentDirection.x) <= 1.0e-12)
                {
                    const double lengthSq = segmentDirection.lengthSq();
                    if (lengthSq > 1.0e-24)
                    {
                        const AcGePoint2d origin(clipStart.x, clipStart.y);
                        for (const AcGePoint2d &point : {edgeStart, edgeEnd})
                        {
                            const double t =
                                (point - origin).dotProduct(segmentDirection) /
                                lengthSq;
                            if (t > 1.0e-12 && t < 1.0 - 1.0e-12)
                                breaks.push_back(t);
                        }
                    }
                }
            }
            const double deltaZ = clipEnd.z - clipStart.z;
            if (std::abs(deltaZ) > 1.0e-14)
            {
                if (frame.clippingFront)
                {
                    const double t =
                        (frame.frontClipZ - clipStart.z) / deltaZ;
                    if (t > 1.0e-12 && t < 1.0 - 1.0e-12)
                        breaks.push_back(t);
                }
                if (frame.clippingBack)
                {
                    const double t =
                        (frame.backClipZ - clipStart.z) / deltaZ;
                    if (t > 1.0e-12 && t < 1.0 - 1.0e-12)
                        breaks.push_back(t);
                }
            }
            std::sort(breaks.begin(), breaks.end());
            breaks.erase(std::unique(breaks.begin(), breaks.end(),
                                     [](double a, double b) {
                                         return std::abs(a - b) <= 1.0e-12;
                                     }),
                         breaks.end());
            const auto inside = [&](double t) {
                const AcGePoint3d point =
                    worldStart + (worldEnd - worldStart) * t;
                AcGePoint3d clip;
                if (!frame.coordinates(point, clip) ||
                    (frame.clippingFront &&
                     clip.z > frame.frontClipZ + 1.0e-10) ||
                    (frame.clippingBack &&
                     clip.z < frame.backClipZ - 1.0e-10))
                    return false;
                return pointInSimplePolygon(
                    frame.boundary, AcGePoint2d(clip.x, clip.y));
            };
            for (size_t index = 1; index < breaks.size(); ++index)
            {
                const double localStart = breaks[index - 1];
                const double localEnd = breaks[index];
                if (localEnd - localStart <= 1.0e-12 ||
                    !inside((localStart + localEnd) * 0.5))
                    continue;
                clippedRanges.emplace_back(
                    rangeStart + (rangeEnd - rangeStart) * localStart,
                    rangeStart + (rangeEnd - rangeStart) * localEnd);
            }
        }
        std::sort(clippedRanges.begin(), clippedRanges.end());
        ranges.clear();
        for (const auto &range : clippedRanges)
        {
            if (!ranges.empty() &&
                range.first <= ranges.back().second + 1.0e-12)
                ranges.back().second =
                    std::max(ranges.back().second, range.second);
            else
                ranges.push_back(range);
        }
        if (ranges.empty())
            break;
    }
    std::vector<std::pair<AcGePoint3d, AcGePoint3d>> result;
    result.reserve(ranges.size());
    for (const auto &[first, last] : ranges)
        result.emplace_back(start + direction * first,
                            start + direction * last);
    return result;
}

std::vector<std::vector<AcGePoint3d>>
AcGiWorldDraw::clipPolygon(std::vector<AcGePoint3d> polygon) const
{
    std::vector<std::vector<AcGePoint3d>> components;
    if (polygon.size() < 3)
        return components;
    components.push_back(std::move(polygon));
    for (const ClipFrame &frame : clipStack_)
    {
        const auto clipConvex = [&](std::vector<AcGePoint3d> subject,
                                    const std::vector<AcGePoint2d> &clipRing,
                                    double winding) {
            for (size_t edgeIndex = 0;
                 edgeIndex < clipRing.size() && !subject.empty();
                 ++edgeIndex)
            {
                const AcGePoint2d &edgeStart = clipRing[edgeIndex];
                const AcGePoint2d &edgeEnd =
                    clipRing[(edgeIndex + 1) % clipRing.size()];
                const auto side = [&](const AcGePoint3d &point) {
                    AcGePoint3d clipPoint;
                    if (!frame.coordinates(point, clipPoint))
                        return std::numeric_limits<long double>::quiet_NaN();
                    return signedDistance(
                        edgeStart, edgeEnd,
                        AcGePoint2d(clipPoint.x, clipPoint.y), winding);
                };
                std::vector<AcGePoint3d> output;
                output.reserve(subject.size() + 1);
                AcGePoint3d previous = subject.back();
                long double previousSide = side(previous);
                if (!std::isfinite(previousSide))
                    return std::vector<AcGePoint3d>{};
                for (const AcGePoint3d &current : subject)
                {
                    const long double currentSide = side(current);
                    if (!std::isfinite(currentSide))
                        return std::vector<AcGePoint3d>{};
                    const bool previousInside = previousSide >= -1.0e-10L;
                    const bool currentInside = currentSide >= -1.0e-10L;
                    if (previousInside != currentInside)
                    {
                        const double t = static_cast<double>(std::clamp(
                            previousSide / (previousSide - currentSide),
                            0.0L, 1.0L));
                        output.push_back(previous + (current - previous) * t);
                    }
                    if (currentInside)
                        output.push_back(current);
                    previous = current;
                    previousSide = currentSide;
                }
                subject = std::move(output);
            }
            return subject;
        };

        std::vector<std::vector<AcGePoint3d>> clippedComponents;
        for (const auto &component : components)
        {
            if (frame.convex)
            {
                std::vector<AcGePoint3d> clipped =
                    clipConvex(component, frame.boundary, frame.winding);
                if (clipped.size() >= 3)
                    clippedComponents.push_back(std::move(clipped));
            }
            else
            {
                for (const auto &indices : frame.triangles)
                {
                    const std::vector<AcGePoint2d> triangle{
                        frame.boundary[indices[0]], frame.boundary[indices[1]],
                        frame.boundary[indices[2]]};
                    const double winding =
                        cross2d(triangle[0], triangle[1], triangle[2]) > 0.0L
                            ? 1.0
                            : -1.0;
                    std::vector<AcGePoint3d> clipped =
                        clipConvex(component, triangle, winding);
                    if (clipped.size() >= 3)
                        clippedComponents.push_back(std::move(clipped));
                }
            }
        }
        components = std::move(clippedComponents);
        for (std::vector<AcGePoint3d> &component : components)
        {
            const auto clipDepth = [&](double planeZ, bool keepBelow) {
                if (component.empty())
                    return;
                std::vector<AcGePoint3d> output;
                output.reserve(component.size() + 1);
                AcGePoint3d previous = component.back();
                AcGePoint3d previousClip;
                if (!frame.coordinates(previous, previousClip))
                {
                    component.clear();
                    return;
                }
                long double previousSide =
                    keepBelow ? planeZ - previousClip.z
                              : previousClip.z - planeZ;
                for (const AcGePoint3d &current : component)
                {
                    AcGePoint3d currentClip;
                    if (!frame.coordinates(current, currentClip))
                    {
                        component.clear();
                        return;
                    }
                    const long double currentSide =
                        keepBelow ? planeZ - currentClip.z
                                  : currentClip.z - planeZ;
                    const bool previousInside =
                        previousSide >= -1.0e-10L;
                    const bool currentInside = currentSide >= -1.0e-10L;
                    if (previousInside != currentInside)
                    {
                        const double t = static_cast<double>(std::clamp(
                            previousSide / (previousSide - currentSide),
                            0.0L, 1.0L));
                        output.push_back(previous + (current - previous) * t);
                    }
                    if (currentInside)
                        output.push_back(current);
                    previous = current;
                    previousSide = currentSide;
                }
                component = std::move(output);
            };
            if (frame.clippingFront)
                clipDepth(frame.frontClipZ, true);
            if (frame.clippingBack)
                clipDepth(frame.backClipZ, false);
        }
        components.erase(
            std::remove_if(components.begin(), components.end(),
                           [](const auto &component) {
                               return component.size() < 3;
                           }),
            components.end());
    }
    return components;
}

void AcGiWorldDraw::applyTraits(acdb::Stroke &stroke) const
{
    const acgs::DrawTraits &traits = draw_.subEntityTraits().traits();
    stroke.common = traits.toEntityCommon();
    stroke.lineWeight = traits.lineWeight;
    stroke.materialId = traits.materialId;
}

void AcGiWorldDraw::applyTraits(acdb::Triangle &triangle,
                                bool fillIs3DFace) const
{
    const acgs::DrawTraits &traits = draw_.subEntityTraits().traits();
    triangle.common = traits.toEntityCommon();
    triangle.is3DFace = fillIs3DFace;
    triangle.materialId = traits.materialId;
}

bool AcGiWorldDraw::worldCircle(const AcGeCircArc3d &arc)
{
    const double sweep = arc.endAngle - arc.startAngle;
    if (!(arc.radius > 0.0) || !(sweep > 0.0))
        return true;
    const acdb::TesselationOptions options = draw_.optionsFor(arc.radius);
    const int segments = std::max(
        8, static_cast<int>(std::ceil(sweep / options.angleStep)));
    const bool closed = arc.isFullCircle();
    const int pointCount = closed ? segments : segments + 1;
    std::vector<AcGePoint3d> points;
    points.reserve(static_cast<size_t>(pointCount));
    for (int i = 0; i < pointCount; ++i)
    {
        const double t = sweep * static_cast<double>(i) / segments;
        points.push_back(arc.pointAt(arc.startAngle + t));
    }
    if (closed && fillType() == kAcGiFillAlways)
        for (int index = 0; index < pointCount; ++index)
            worldTriangle(arc.center, points[index],
                          points[(index + 1) % pointCount]);
    return worldPolyline(points.data(), pointCount, closed);
}

bool AcGiWorldDraw::text(const AcGePoint3d &position,
                         const AcGeVector3d &normal,
                         const AcGeVector3d &direction, const char *message,
                         const AcGiTextStyle &style)
{
    if (!message || !*message)
        return true;
    const double height = style.textSize;
    if (!(height > 0.0) || !std::isfinite(height))
        return true;

    // SHX vector glyphs: when the style names a text style whose SHX fonts
    // the acdb system loaded, each glyph stroke becomes an ordinary CAD
    // stroke (cap line at the anchor, baseline one em down) — same output
    // the stroke pipeline serves for any other SHX geometry.
    if (style.fileName != nullptr && *style.fileName != '\0' &&
        acgi::textEngine().shxStyleFontBound(style.fileName))
    {
        const AcUtf8String utf8Glyphs(message);
        const AcGeVector3d right = direction.normal();
        const AcGeVector3d up = normal.normal().crossProduct(right);
        for (const rendering::ShxGlyphStroke &glyphStroke :
             acgi::textEngine().shxStrokes(utf8Glyphs))
        {
            worldLine(position +
                          AcGeVector3d(right * (glyphStroke.fromX * height) +
                                       up * (glyphStroke.fromY * height)),
                      position +
                          AcGeVector3d(right * (glyphStroke.toX * height) +
                                       up * (glyphStroke.toY * height)));
        }
        return false;
    }

    acgi::TextRequest request;
    request.position = glm::dvec3(apply(position));
    request.normal = glm::dvec3(normal.normal());
    request.direction = glm::dvec3(direction.normal());
    request.message = message;
    request.height = height;
    request.xScale = style.xScale;
    request.color = draw_.subEntityTraits().traits().color;
    acgi::textRequests().push_back(std::move(request));

    if (!acgi::textFrameFallback())
        return false;

    size_t lineBreaks = 0;
    size_t longest = 0;
    size_t current = 0;
    for (const char *character = message; *character; ++character)
    {
        if (*character == 10)
        {
            ++lineBreaks;
            longest = std::max(longest, current);
            current = 0;
        }
        else
        {
            ++current;
        }
    }
    longest = std::max(longest, current);
    const size_t lineCount = lineBreaks + 1;
    const double width = std::max(double(longest) * height * 0.6 *
                                      style.xScale,
                                  height);
    const glm::dvec3 right = glm::dvec3(direction.normal());
    const glm::dvec3 up =
        glm::normalize(glm::cross(glm::dvec3(normal.normal()), right));
    const glm::dvec3 frameHeight = up * (height * double(lineCount));

    const AcGePoint3d framePoints[] = {
        position,
        position + AcGeVector3d(right * width),
        position + AcGeVector3d(right * width + frameHeight),
        position + AcGeVector3d(frameHeight),
        position};
    worldPolyline(framePoints, 5, true);
    worldPoint(position);
    return false;
}

bool AcGiWorldDraw::text(const AcGePoint3d &position,
                         const AcGeVector3d &normal,
                         const AcGeVector3d &direction, const ACHAR *message,
                         const AcGiTextStyle &style)
{
    if (!message)
        return true;
    const AcUtf8String utf8Message =
        arxcompat::toUtf8(AcStringView(message));
    return text(position, normal, direction, utf8Message.c_str(), style);
}

bool AcGiWorldDraw::worldEllipse(const AcGeEllip3d &ellipse)
{
    const double sweep = ellipse.endAngle - ellipse.startAngle;
    if (!(sweep > 0.0))
        return true;
    const acdb::TesselationOptions options =
        draw_.optionsFor(ellipse.majorRadius());
    const int segments = std::max(
        8, static_cast<int>(std::ceil(sweep / options.angleStep)));
    const bool closed = sweep >= kTwoPi - 1.0e-12;
    const int emitted = closed ? segments : segments + 1;
    std::vector<AcGePoint3d> points;
    points.reserve(static_cast<size_t>(emitted));
    for (int i = 0; i < emitted; ++i)
    {
        const double t = sweep * static_cast<double>(i) / segments;
        points.push_back(apply(ellipse.pointAt(ellipse.startAngle + t)));
    }
    return worldPolyline(points.data(), emitted, closed);
}

bool AcGiWorldDraw::circle(const AcGePoint3d &center, const double radius,
                           const AcGeVector3d &normal)
{
    if (!(radius > 0.0))
        return true;
    // Full circle: the default angle span of the center/normal/radius form.
    return worldCircle(AcGeCircArc3d(center, normal.normal(), radius));
}

bool AcGiWorldDraw::circle(const AcGePoint3d &first, const AcGePoint3d &second,
                           const AcGePoint3d &third)
{
    AcGeCircArc3d through;
    if (!circumArcThrough(first, second, third, through))
        return true;
    return worldCircle(
        AcGeCircArc3d(through.center, through.normal, through.radius));
}

bool AcGiWorldDraw::circularArc(const AcGePoint3d &center, const double radius,
                                const AcGeVector3d &normal,
                                const AcGeVector3d &startVector,
                                const double sweepAngle,
                                const AcGiArcType arcType)
{
    if (!(radius > 0.0) || sweepAngle == 0.0)
        return true;
    AcGeCircArc3d arc(center, normal.normal(), radius, 0.0, sweepAngle);
    const AcGeVector3d u = arc.referenceAxis();
    const AcGeVector3d v = arc.normal.crossProduct(u);
    const double startAngle = std::atan2(startVector.dotProduct(v),
                                         startVector.dotProduct(u));
    arc.setAngles(startAngle, startAngle + sweepAngle);
    if (worldCircle(arc))
        return true;
    return emitArcClosure(*this, center, arc.startPoint(), arc.endPoint(),
                          sampleArcPoints(gsContext(), arc), arcType);
}

bool AcGiWorldDraw::circularArc(const AcGePoint3d &start,
                                const AcGePoint3d &point,
                                const AcGePoint3d &end,
                                const AcGiArcType arcType)
{
    AcGeCircArc3d arc;
    if (!circumArcThrough(start, point, end, arc))
        return true;
    if (worldCircle(arc))
        return true;
    return emitArcClosure(*this, arc.center, arc.startPoint(), arc.endPoint(),
                          sampleArcPoints(gsContext(), arc), arcType);
}

bool AcGiWorldDraw::ellipticalArc(const AcGePoint3d &center,
                                  const AcGeVector3d &normal,
                                  double majorAxisLength,
                                  double minorAxisLength,
                                  double startAngleInRad,
                                  double endAngleInRad, double tiltAngleInRad,
                                  AcGiArcType arcType)
{
    if (!(majorAxisLength > 0.0) || !(minorAxisLength > 0.0) ||
        endAngleInRad == startAngleInRad)
        return true;
    const AcGeVector3d unitNormal = normal.normal();
    const AcGeVector3d reference =
        AcGeCircArc3d(center, unitNormal, majorAxisLength).referenceAxis();
    const AcGeVector3d planeSide = unitNormal.crossProduct(reference);
    const AcGeVector3d majorAxis =
        (reference * std::cos(tiltAngleInRad) +
         planeSide * std::sin(tiltAngleInRad)) *
        majorAxisLength;
    const double ratio =
        std::clamp(minorAxisLength / majorAxisLength, 0.0, 1.0);
    const AcGeEllip3d ellipse(center, majorAxis, unitNormal, ratio,
                              startAngleInRad, endAngleInRad);
    if (worldEllipse(ellipse))
        return true;
    if (arcType == AcGiArcType::kAcGiArcSimple)
        return false;

    // Closure areas: spokes from the center (sector) or the chord line
    // (chord), plus the fan fill over the same segmentation worldEllipse
    // used for the boundary.
    const double sweep = endAngleInRad - startAngleInRad;
    const acdb::TesselationOptions options =
        gsContext().optionsFor(ellipse.majorRadius());
    const int segments = std::max(
        8, static_cast<int>(std::ceil(sweep / options.angleStep)));
    std::vector<AcGePoint3d> samples;
    samples.reserve(static_cast<size_t>(segments) + 1);
    for (int index = 0; index <= segments; ++index)
    {
        const double t = sweep * static_cast<double>(index) / segments;
        samples.push_back(ellipse.pointAt(startAngleInRad + t));
    }
    const AcGePoint3d start = ellipse.startPoint();
    const AcGePoint3d end = ellipse.endPoint();
    const AcGePoint3d fanOrigin =
        arcType == AcGiArcType::kAcGiArcSector
            ? center
            : start + (end - start) * 0.5;
    return emitArcClosure(*this, fanOrigin, start, end, samples, arcType);
}

bool AcGiWorldDraw::mesh(const Adesk::UInt32 rows, const Adesk::UInt32 columns,
                         const AcGePoint3d *vertexList,
                         const AcGiEdgeData *edgeData,
                         const AcGiFaceData *faceData,
                         const AcGiVertexData *vertexData,
                         const bool autoGenerateNormals)
{
    if (rows < 2 || columns < 2 || vertexList == nullptr)
        return true;
    const uint64_t vertexCount = uint64_t(rows) * columns;
    const uint64_t faceCount = uint64_t(rows - 1) * (columns - 1);
    const uint64_t horizontalEdgeCount = uint64_t(rows) * (columns - 1);
    const uint64_t verticalEdgeCount = uint64_t(rows - 1) * columns;
    if (horizontalEdgeCount >
        std::numeric_limits<uint64_t>::max() - verticalEdgeCount)
        return true;
    const uint64_t edgeCount = horizontalEdgeCount + verticalEdgeCount;
    if (vertexCount > std::numeric_limits<size_t>::max() ||
        faceCount > std::numeric_limits<size_t>::max() ||
        edgeCount > std::numeric_limits<size_t>::max())
        return true;
    const size_t vertexCountSize = static_cast<size_t>(vertexCount);
    const size_t faceCountSize = static_cast<size_t>(faceCount);
    const size_t edgeCountSize = static_cast<size_t>(edgeCount);
    const acdb::AcDbObjectId *edgeLinetypeIds =
        edgeData == nullptr ? nullptr : edgeData->linetypeIds();
    if ((faceData != nullptr &&
         ((!faceData->trueColors.empty() &&
           faceData->trueColors.size() != faceCountSize) ||
          (!faceData->colors.empty() &&
           faceData->colors.size() != faceCountSize) ||
          (!faceData->layers.empty() &&
           faceData->layers.size() != faceCountSize) ||
          (!faceData->selectionMarkers.empty() &&
           faceData->selectionMarkers.size() != faceCountSize) ||
          (!faceData->materials.empty() &&
           faceData->materials.size() != faceCountSize) ||
          (faceData->mapperValues != nullptr &&
           faceData->mapperCountKnown &&
           faceData->mapperCount != faceCountSize) ||
          (!faceData->visibility.empty() &&
           faceData->visibility.size() != faceCountSize) ||
          (!faceData->normals.empty() &&
           faceData->normals.size() != faceCountSize) ||
          (!faceData->transparency.empty() &&
           faceData->transparency.size() != faceCountSize) ||
          !validNormals(faceData->normals))) ||
        (vertexData != nullptr &&
         ((!vertexData->trueColors.empty() &&
           vertexData->trueColors.size() != vertexCountSize) ||
          (!vertexData->normals.empty() &&
           vertexData->normals.size() != vertexCountSize) ||
          !validNormals(vertexData->normals) ||
          !validMappingCoords(vertexData, vertexCountSize))) ||
        (edgeData != nullptr &&
         ((!edgeData->trueColors.empty() &&
           edgeData->trueColors.size() != edgeCountSize) ||
          (!edgeData->colors.empty() &&
           edgeData->colors.size() != edgeCountSize) ||
          (!edgeData->layers.empty() &&
           edgeData->layers.size() != edgeCountSize) ||
          (!edgeData->linetypeIdValues.empty() &&
           edgeData->linetypeIdValues.size() != edgeCountSize) ||
          (!edgeData->selectionMarkers.empty() &&
           edgeData->selectionMarkers.size() != edgeCountSize) ||
          (!edgeData->lineTypes.empty() &&
           edgeData->lineTypes.size() != edgeCountSize) ||
          (!edgeData->visibility.empty() &&
           edgeData->visibility.size() != edgeCountSize))))
        return true;

    const acgs::DrawTraits originalTraits =
        draw_.subEntityTraits().traits();
    AcGePoint3d mapperObjectMinimum;
    AcGePoint3d mapperObjectMaximum;
    pointCloudBounds(vertexList, size_t(rows) * columns,
                     mapperObjectMinimum, mapperObjectMaximum);
    const AcGePoint3d mapperModelMinimum = apply(mapperObjectMinimum);
    const AcGePoint3d mapperModelMaximum = apply(mapperObjectMaximum);
    size_t faceIndex = 0;
    auto emitTriangle = [&](const std::array<AcGePoint3d, 3> &points,
                            const std::array<size_t, 3> &indices) {
        if (fillType() == kAcGiFillNever)
            return;
        const size_t firstFill = sink().geometry().fills.size();
        const std::array<AcGePoint3d, 3> worldPoints{
            apply(points[0]), apply(points[1]), apply(points[2])};
        worldTriangle(points[0], points[1], points[2]);
        for (size_t fillIndex = firstFill;
             fillIndex < sink().geometry().fills.size(); ++fillIndex)
        {
            acdb::Triangle &triangle = sink().geometry().fills[fillIndex];
            if (faceData != nullptr)
            {
                if (!faceData->layers.empty())
                    triangle.layerId = faceData->layers[faceIndex];
                if (!faceData->materials.empty())
                    triangle.materialId = faceData->materials[faceIndex];
                if (!faceData->selectionMarkers.empty())
                    triangle.selectionMarker =
                        faceData->selectionMarkers[faceIndex];
                if (!faceData->colors.empty())
                {
                    triangle.colorIndex = faceData->colors[faceIndex];
                    triangle.hasColorIndex = true;
                }
            }
            applyVertexData(triangle, worldPoints, indices, vertexData);
            const AcGiMapper *faceMapper =
                faceData != nullptr && faceData->mapperValues != nullptr
                    ? &faceData->mapperValues[faceIndex]
                    : nullptr;
            AcGeMatrix3d mapperAutoMatrix;
            if (faceMapper != nullptr)
            {
                mapperAutoMatrix = mapperAutoTransform(
                    faceMapper->autoTransform(),
                    faceMapper->autoTransform() == AcGiMapper::kModel
                        ? mapperModelMinimum
                        : mapperObjectMinimum,
                    faceMapper->autoTransform() == AcGiMapper::kModel
                        ? mapperModelMaximum
                        : mapperObjectMaximum);
            }
            applyMapper(triangle, faceMapper, worldPoints,
                        faceMapper == nullptr ? nullptr : &mapperAutoMatrix);
            if (vertexData == nullptr || vertexData->normals.empty())
            {
                if (faceData != nullptr && !faceData->normals.empty())
                    applyTriangleNormal(triangle, &faceData->normals[faceIndex]);
                else if (autoGenerateNormals)
                    applyTriangleNormal(triangle);
            }
        }
    };

    for (Adesk::UInt32 row = 0; row + 1 < rows; ++row)
    {
        for (Adesk::UInt32 column = 0; column + 1 < columns; ++column)
        {
            const size_t i00 = size_t(row) * columns + column;
            const size_t i01 = i00 + 1;
            const size_t i10 = size_t(row + 1) * columns + column;
            const size_t i11 = i10 + 1;
            if (fillType() == kAcGiFillNever)
            {
                ++faceIndex;
                continue;
            }
            if (faceData != nullptr &&
                !faceData->visibility.empty() &&
                faceData->visibility[faceIndex] == 0)
            {
                ++faceIndex;
                continue;
            }
            draw_.subEntityTraits().setFrom(originalTraits);
            if (faceData != nullptr && !faceData->trueColors.empty())
                draw_.subEntityTraits().setColor(
                    faceData->trueColors[faceIndex].renderColor());
            if (faceData != nullptr && !faceData->transparency.empty() &&
                faceData->transparency[faceIndex].isByAlpha())
            {
                glm::vec4 color = draw_.subEntityTraits().traits().color;
                color.a *=
                    static_cast<float>(
                        faceData->transparency[faceIndex].alpha()) /
                    255.0f;
                draw_.subEntityTraits().setColor(color);
            }
            const AcGePoint3d points[] = {vertexList[i00], vertexList[i01],
                                          vertexList[i10], vertexList[i11]};
            emitTriangle({points[0], points[1], points[3]},
                         {i00, i01, i11});
            emitTriangle({points[0], points[3], points[2]},
                         {i00, i11, i10});
            ++faceIndex;
        }
    }
    draw_.subEntityTraits().setFrom(originalTraits);

    const bool emitEdges =
        edgeData != nullptr &&
        (!edgeData->colors.empty() || !edgeData->trueColors.empty() ||
         !edgeData->layers.empty() || edgeLinetypeIds != nullptr ||
         !edgeData->selectionMarkers.empty() || !edgeData->lineTypes.empty() ||
         !edgeData->visibility.empty());
    if (emitEdges)
    {
        auto emitEdge = [&](size_t edgeIndex, const AcGePoint3d &a,
                            const AcGePoint3d &b) {
            if (!edgeData->visibility.empty() &&
                edgeData->visibility[edgeIndex] == 0)
                return;
            if (!edgeData->trueColors.empty())
                draw_.subEntityTraits().setColor(
                    edgeData->trueColors[edgeIndex].renderColor());
            if (!edgeData->lineTypes.empty())
                draw_.subEntityTraits().setLineType(
                    edgeData->lineTypes[edgeIndex]);
            const size_t firstStroke = sink().geometry().strokes.size();
            worldLine(a, b);
            for (size_t strokeIndex = firstStroke;
                 strokeIndex < sink().geometry().strokes.size(); ++strokeIndex)
            {
                acdb::Stroke &stroke =
                    sink().geometry().strokes[strokeIndex];
                if (!edgeData->layers.empty())
                    stroke.layerId = edgeData->layers[edgeIndex];
                if (edgeLinetypeIds != nullptr)
                    stroke.linetypeId = edgeLinetypeIds[edgeIndex];
                if (!edgeData->selectionMarkers.empty())
                    stroke.selectionMarker =
                        edgeData->selectionMarkers[edgeIndex];
                if (!edgeData->colors.empty())
                {
                    stroke.colorIndex = edgeData->colors[edgeIndex];
                    stroke.hasColorIndex = true;
                }
            }
        };
        size_t edgeIndex = 0;
        for (Adesk::UInt32 row = 0; row < rows; ++row)
            for (Adesk::UInt32 column = 0; column + 1 < columns; ++column)
                emitEdge(edgeIndex++, vertexList[size_t(row) * columns + column],
                         vertexList[size_t(row) * columns + column + 1]);
        for (Adesk::UInt32 column = 0; column < columns; ++column)
            for (Adesk::UInt32 row = 0; row + 1 < rows; ++row)
                emitEdge(edgeIndex++,
                         vertexList[size_t(row) * columns + column],
                         vertexList[size_t(row + 1) * columns + column]);
        draw_.subEntityTraits().setFrom(originalTraits);
    }
    (void)autoGenerateNormals;
    return false;
}

bool AcGiWorldDraw::shell(const Adesk::UInt32 nbVertex,
                          const AcGePoint3d *vertexList,
                          const Adesk::UInt32 faceListSize,
                          const Adesk::Int32 *faceList,
                          const AcGiEdgeData *edgeData,
                          const AcGiFaceData *faceData,
                          const AcGiVertexData *vertexData,
                          const struct resbuf *resbufData,
                          bool autoGenerateNormals)
{
    (void)resbufData;
    if (nbVertex == 0 || vertexList == nullptr ||
        (faceListSize != 0 && faceList == nullptr))
        return true;

    struct Face
    {
        std::vector<size_t> outer;
        std::vector<std::vector<size_t>> holes;
    };
    struct Edge
    {
        size_t start;
        size_t end;
    };
    std::vector<Face> faces;
    std::vector<Edge> edges;
    Face *currentFace = nullptr;
    Adesk::UInt32 position = 0;
    while (position < faceListSize)
    {
        const Adesk::Int32 count = faceList[position++];
        if (count == 0)
            break;
        const uint64_t faceSize =
            count < 0 ? uint64_t(-int64_t(count)) : uint64_t(count);
        if (faceSize < 3 || faceSize > faceListSize - position)
            return true; // malformed face list
        if (count > 0)
        {
            faces.emplace_back();
            currentFace = &faces.back();
        }
        else if (currentFace == nullptr)
        {
            return true;
        }
        std::vector<size_t> &ring =
            count > 0 ? currentFace->outer : currentFace->holes.emplace_back();
        ring.reserve(static_cast<size_t>(faceSize));
        for (uint64_t index = 0; index < faceSize; ++index)
        {
            const Adesk::Int32 vertexIndex = faceList[position++];
            if (vertexIndex < 0 ||
                static_cast<Adesk::UInt32>(vertexIndex) >= nbVertex)
                return true;
            ring.push_back(static_cast<size_t>(vertexIndex));
        }
        for (size_t index = 0; index < ring.size(); ++index)
        {
            edges.push_back({ring[index], ring[(index + 1) % ring.size()]});
        }
    }

    const acdb::AcDbObjectId *edgeLinetypeIds =
        edgeData == nullptr ? nullptr : edgeData->linetypeIds();
    if ((faceData != nullptr &&
         ((!faceData->trueColors.empty() &&
           faceData->trueColors.size() != faces.size()) ||
          (!faceData->colors.empty() &&
           faceData->colors.size() != faces.size()) ||
          (!faceData->layers.empty() &&
           faceData->layers.size() != faces.size()) ||
          (!faceData->selectionMarkers.empty() &&
           faceData->selectionMarkers.size() != faces.size()) ||
          (!faceData->materials.empty() &&
           faceData->materials.size() != faces.size()) ||
          (faceData->mapperValues != nullptr &&
           faceData->mapperCountKnown &&
           faceData->mapperCount != faces.size()) ||
          (!faceData->visibility.empty() &&
           faceData->visibility.size() != faces.size()) ||
          (!faceData->normals.empty() &&
           faceData->normals.size() != faces.size()) ||
          (!faceData->transparency.empty() &&
           faceData->transparency.size() != faces.size()) ||
          !validNormals(faceData->normals))) ||
        (vertexData != nullptr &&
         ((!vertexData->trueColors.empty() &&
           vertexData->trueColors.size() != nbVertex) ||
          (!vertexData->normals.empty() &&
           vertexData->normals.size() != nbVertex) ||
          !validNormals(vertexData->normals) ||
          !validMappingCoords(vertexData, nbVertex))) ||
        (edgeData != nullptr &&
         ((!edgeData->trueColors.empty() &&
           edgeData->trueColors.size() != edges.size()) ||
          (!edgeData->colors.empty() &&
           edgeData->colors.size() != edges.size()) ||
          (!edgeData->layers.empty() &&
           edgeData->layers.size() != edges.size()) ||
          (!edgeData->linetypeIdValues.empty() &&
           edgeData->linetypeIdValues.size() != edges.size()) ||
          (!edgeData->selectionMarkers.empty() &&
           edgeData->selectionMarkers.size() != edges.size()) ||
          (!edgeData->lineTypes.empty() &&
           edgeData->lineTypes.size() != edges.size()) ||
          (!edgeData->visibility.empty() &&
           edgeData->visibility.size() != edges.size()))))
        return true;

    const acgs::DrawTraits originalTraits =
        draw_.subEntityTraits().traits();
    AcGePoint3d mapperObjectMinimum;
    AcGePoint3d mapperObjectMaximum;
    pointCloudBounds(vertexList, nbVertex, mapperObjectMinimum,
                     mapperObjectMaximum);
    const AcGePoint3d mapperModelMinimum = apply(mapperObjectMinimum);
    const AcGePoint3d mapperModelMaximum = apply(mapperObjectMaximum);
    for (size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex)
    {
        const Face &face = faces[faceIndex];
        draw_.subEntityTraits().setFrom(originalTraits);
        if (faceData != nullptr &&
            !faceData->visibility.empty() &&
            faceData->visibility[faceIndex] == 0)
            continue;
        if (fillType() == kAcGiFillNever)
            continue;
        std::vector<std::vector<size_t>> contours;
        contours.reserve(face.holes.size() + 1);
        contours.push_back(face.outer);
        contours.insert(contours.end(), face.holes.begin(), face.holes.end());
        std::vector<size_t> mergedIndices;
        std::vector<std::array<size_t, 3>> triangleIndices;
        if (!triangulatePolygonWithHoles(vertexList, contours, mergedIndices,
                                         triangleIndices))
        {
            draw_.subEntityTraits().setFrom(originalTraits);
            return true;
        }
        if (faceData != nullptr && !faceData->trueColors.empty())
            draw_.subEntityTraits().setColor(
                faceData->trueColors[faceIndex].renderColor());
        if (faceData != nullptr && !faceData->transparency.empty() &&
            faceData->transparency[faceIndex].isByAlpha())
        {
            glm::vec4 color = draw_.subEntityTraits().traits().color;
            color.a *=
                static_cast<float>(faceData->transparency[faceIndex].alpha()) /
                255.0f;
            draw_.subEntityTraits().setColor(color);
        }
        for (const auto &triangle : triangleIndices)
        {
            const std::array<size_t, 3> indices{
                mergedIndices[triangle[0]], mergedIndices[triangle[1]],
                mergedIndices[triangle[2]]};
            const std::array<AcGePoint3d, 3> trianglePoints{
                vertexList[indices[0]], vertexList[indices[1]],
                vertexList[indices[2]]};
            const size_t firstFill = sink().geometry().fills.size();
            const std::array<AcGePoint3d, 3> worldPoints{
                apply(trianglePoints[0]), apply(trianglePoints[1]),
                apply(trianglePoints[2])};
            worldTriangle(trianglePoints[0], trianglePoints[1],
                          trianglePoints[2]);
            for (size_t fillIndex = firstFill;
                 fillIndex < sink().geometry().fills.size(); ++fillIndex)
            {
                acdb::Triangle &triangle = sink().geometry().fills[fillIndex];
                if (faceData != nullptr)
                {
                    if (!faceData->layers.empty())
                        triangle.layerId = faceData->layers[faceIndex];
                    if (!faceData->materials.empty())
                        triangle.materialId = faceData->materials[faceIndex];
                    if (!faceData->selectionMarkers.empty())
                        triangle.selectionMarker =
                            faceData->selectionMarkers[faceIndex];
                    if (!faceData->colors.empty())
                    {
                        triangle.colorIndex = faceData->colors[faceIndex];
                        triangle.hasColorIndex = true;
                    }
                }
                applyVertexData(triangle, worldPoints, indices, vertexData);
                const AcGiMapper *faceMapper =
                    faceData != nullptr && faceData->mapperValues != nullptr
                        ? &faceData->mapperValues[faceIndex]
                        : nullptr;
                AcGeMatrix3d mapperAutoMatrix;
                if (faceMapper != nullptr)
                {
                    mapperAutoMatrix = mapperAutoTransform(
                        faceMapper->autoTransform(),
                        faceMapper->autoTransform() == AcGiMapper::kModel
                            ? mapperModelMinimum
                            : mapperObjectMinimum,
                        faceMapper->autoTransform() == AcGiMapper::kModel
                            ? mapperModelMaximum
                            : mapperObjectMaximum);
                }
                applyMapper(triangle, faceMapper, worldPoints,
                            faceMapper == nullptr ? nullptr
                                                  : &mapperAutoMatrix);
                if (vertexData == nullptr || vertexData->normals.empty())
                {
                    if (faceData != nullptr && !faceData->normals.empty())
                        applyTriangleNormal(triangle,
                                            &faceData->normals[faceIndex]);
                    else if (autoGenerateNormals)
                        applyTriangleNormal(triangle);
                }
            }
        }
    }
    draw_.subEntityTraits().setFrom(originalTraits);

    const bool emitEdges =
        edgeData != nullptr &&
        (!edgeData->colors.empty() || !edgeData->trueColors.empty() ||
         !edgeData->layers.empty() || edgeLinetypeIds != nullptr ||
         !edgeData->selectionMarkers.empty() || !edgeData->lineTypes.empty() ||
         !edgeData->visibility.empty());
    if (emitEdges)
    {
        for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex)
        {
            if (!edgeData->visibility.empty() &&
                edgeData->visibility[edgeIndex] == 0)
                continue;
            draw_.subEntityTraits().setFrom(originalTraits);
            if (!edgeData->trueColors.empty())
                draw_.subEntityTraits().setColor(
                    edgeData->trueColors[edgeIndex].renderColor());
            if (!edgeData->lineTypes.empty())
                draw_.subEntityTraits().setLineType(
                    edgeData->lineTypes[edgeIndex]);
            const size_t firstStroke = sink().geometry().strokes.size();
            worldLine(vertexList[edges[edgeIndex].start],
                      vertexList[edges[edgeIndex].end]);
            for (size_t strokeIndex = firstStroke;
                 strokeIndex < sink().geometry().strokes.size(); ++strokeIndex)
            {
                acdb::Stroke &stroke =
                    sink().geometry().strokes[strokeIndex];
                if (!edgeData->layers.empty())
                    stroke.layerId = edgeData->layers[edgeIndex];
                if (edgeLinetypeIds != nullptr)
                    stroke.linetypeId = edgeLinetypeIds[edgeIndex];
                if (!edgeData->selectionMarkers.empty())
                    stroke.selectionMarker =
                        edgeData->selectionMarkers[edgeIndex];
                if (!edgeData->colors.empty())
                {
                    stroke.colorIndex = edgeData->colors[edgeIndex];
                    stroke.hasColorIndex = true;
                }
            }
        }
        draw_.subEntityTraits().setFrom(originalTraits);
    }
    return false;
}
