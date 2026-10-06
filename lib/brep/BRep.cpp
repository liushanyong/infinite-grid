#include "brep/BRep.h"

#include <cmath>

namespace brep
{

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
        // Divergence theorem with a fan triangulation from the ring's
        // first vertex: each ring triangle (v0, prev, cur) contributes
        // det[v0, prev, cur] / 6 = ((v0 x prev) . cur)/6.  Positive for
        // counter-clockwise rings around outward normals; negative when
        // the shell is inside-out.
        // Fan triangles: (v0, prev, cur) for every ring edge EXCEPT the
        // closing edge (prev = last, cur = first's start) — a fan from a
        // corner of an N-gon has exactly N-2 triangles; including the
        // wrap triangle would subtract the complementary half again.
        const CoEdge *first = face->outerLoop->first;
        const AcGePoint3d v0 = first->forward
                                   ? first->edge->start->point
                                   : first->edge->end->point;
        const AcGeVector3d v0v(v0.x, v0.y, v0.z);
        const CoEdge *previousCoEdge = first;
        const CoEdge *coedge = first->next;
        while (true)
        {
            const AcGePoint3d prev = previousCoEdge->forward
                                         ? previousCoEdge->edge->end->point
                                         : previousCoEdge->edge->start->point;
            const AcGePoint3d cur = coedge->forward
                                        ? coedge->edge->end->point
                                        : coedge->edge->start->point;
            const AcGeVector3d pv(prev.x, prev.y, prev.z);
            const AcGeVector3d cv(cur.x, cur.y, cur.z);
            const AcGeVector3d cross = v0v.crossProduct(pv);
            volume += cross.dotProduct(cv) / 6.0;
            if (coedge->next == first) // last ring edge: fan complete
                break;
            previousCoEdge = coedge;
            coedge = coedge->next;
        }
    }
    return volume;
}

} // namespace brep
