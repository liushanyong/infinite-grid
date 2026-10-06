#include "brep/BRep.h"

#include <cmath>
#include <vector>

namespace brep
{

namespace
{

// Angular position of |point| on the arc's plane basis (u, w) around the
// arc center; matches AcGeCircArc3d::pointAt's parameterization.
double arcAngle(const AcGeCircArc3d &arc, const AcGePoint3d &point,
                const AcGeVector3d &u, const AcGeVector3d &w)
{
    const AcGeVector3d delta = point - arc.center;
    return std::atan2(delta.dotProduct(w), delta.dotProduct(u));
}

double wrapTwoPi(double angle)
{
    const double twoPi = 6.2831853071795864769;
    double a = std::fmod(angle, twoPi);
    if (a < 0.0)
        a += twoPi;
    return a;
}

} // namespace

double signedVolume(const Shell *shell)
{
    if (shell == nullptr || shell->firstFace == nullptr)
        return 0.0;
    double volume = 0.0;
    for (const Face *face = shell->firstFace; face != nullptr;
         face = face->next)
    {
        if (face->outerLoop == nullptr ||
            face->outerLoop->first == nullptr)
            continue;

        // Divergence theorem: V = (1/3) * sum over faces of the surface
        // integral of (p . n) dA.
        //
        // Planar face: p . n is the constant offset (n . p0), so the
        // face contributes (1/3)(n.p0) * area.  The area is the chord
        // fan (0.5 * sum (vi x vi+1) . n) plus, per arc side, the
        // circular segment between chord and arc whenever the arc
        // bulges outward (away from the ring interior).
        //
        // Cylindrical face: with p = c + r*n(phi) + z*k (c on the axis,
        // k the unit axis), p . n(phi) = c . n(phi) + r, and the patch
        // contributes (1/3) * r * L * (c . integral(n dphi) + r * dphi)
        // over its angular span and axial length L.

        const CoEdge *first = face->outerLoop->first;

        if (face->cylindrical)
        {
            // The patch's arc sides carry the angular span in their
            // canonical geometry (CCW from startAngle to endAngle,
            // traversal-independent); the straight side parallel to the
            // axis gives L.
            const AcGeCircArc3d *arcSeen = nullptr;
            double axialLength = 0.0;
            const CoEdge *coedge = first;
            int guard = 0;
            while (guard++ < 256)
            {
                const Edge *edge = coedge->edge;
                if (edge->isArc)
                {
                    if (arcSeen == nullptr)
                        arcSeen = &edge->arc;
                }
                else
                {
                    axialLength = std::max(axialLength,
                                           edge->start->point.distanceTo(
                                               edge->end->point));
                }
                coedge = coedge->next;
                if (coedge == first)
                    break;
            }
            if (arcSeen == nullptr || axialLength <= 0.0)
                continue; // not the expected two-arc patch shape

            const AcGeCircArc3d &arc = *arcSeen;
            const AcGeVector3d u = arc.referenceAxis();
            const AcGeVector3d w = arc.normal.crossProduct(u);
            const double phi0 = arc.startAngle;
            const double dphi = arc.endAngle - arc.startAngle;
            if (dphi <= 0.0)
                continue;
            const double intU = std::sin(phi0 + dphi) - std::sin(phi0);
            const double intW = -(std::cos(phi0 + dphi) - std::cos(phi0));
            const AcGeVector3d integralN = u * intU + w * intW;
            const AcGeVector3d c(arc.center.x, arc.center.y, arc.center.z);
            volume += arc.radius * axialLength *
                      (c.dotProduct(integralN) + arc.radius * dphi) / 3.0;
            continue;
        }

        // Planar face: collect the ring (traversal starts) with arc
        // sides remembered for the segment corrections.
        struct RingEntry
        {
            AcGePoint3d point;
            const Edge *arcEdge;    // when the side to the NEXT entry
            bool arcForwardAsTraversed;
        };
        std::vector<RingEntry> ring;
        const CoEdge *coedge = first;
        int guard = 0;
        while (guard++ < 256)
        {
            RingEntry entry;
            entry.point = coedge->forward ? coedge->edge->start->point
                                          : coedge->edge->end->point;
            entry.arcEdge = coedge->edge->isArc ? coedge->edge : nullptr;
            entry.arcForwardAsTraversed = coedge->forward;
            ring.push_back(entry);
            coedge = coedge->next;
            if (coedge == first)
                break;
        }
        if (ring.size() < 3)
            continue;

        const AcGeVector3d &n = face->surface.normal;
        const AcGePoint3d &p0 = ring[0].point;
        const double planeOffset = n.dotProduct(AcGeVector3d(
            p0.x, p0.y, p0.z));

        double area = 0.0;
        for (std::size_t i = 0; i < ring.size(); ++i)
        {
            const AcGePoint3d &a = ring[i].point;
            const AcGePoint3d &b = ring[(i + 1) % ring.size()].point;
            const AcGeVector3d av(a.x, a.y, a.z);
            const AcGeVector3d bv(b.x, b.y, b.z);
            area += 0.5 * av.crossProduct(bv).dotProduct(n);

            const Edge *arcEdge = ring[i].arcEdge;
            if (arcEdge == nullptr)
                continue;
            // Segment between chord a->b and the arc.  Bulge direction
            // versus the ring interior decides the sign.
            const AcGeCircArc3d &arc = arcEdge->arc;
            const AcGeVector3d u = arc.referenceAxis();
            const AcGeVector3d w = arc.normal.crossProduct(u);
            const double phiA =
                arcAngle(arc, arcEdge->start->point, u, w);
            const double phiB = arcAngle(arc, arcEdge->end->point, u, w);
            const double span = wrapTwoPi(phiB - phiA); // canonical CCW
            if (span < 1.0e-9 || span > 3.14159265358979323846)
                continue;
            const double midAngle = phiA + 0.5 * span;
            const AcGePoint3d arcMid = arc.pointAt(midAngle);
            const AcGePoint3d chordMid(
                0.5 * (a.x + b.x), 0.5 * (a.y + b.y), 0.5 * (a.z + b.z));
            // Interior side of the chord in the face plane: n x (b-a).
            const AcGeVector3d chordDir = b - a;
            const AcGeVector3d inward = n.crossProduct(chordDir);
            const AcGeVector3d bulge = arcMid - chordMid;
            const double segmentArea =
                0.5 * arc.radius * arc.radius * (span - std::sin(span));
            if (bulge.dotProduct(inward) < 0.0)
                area += segmentArea; // bulges outward: region gained
            else
                area -= segmentArea; // dents inward: region lost
        }
        volume += planeOffset * area / 3.0;
    }
    return volume;
}

} // namespace brep
