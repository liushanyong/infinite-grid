#include "brep/BRep.h"

#include <cmath>
#include <vector>

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
        // Divergence theorem over a fan triangulation of each face from
        // its ring's first vertex: triangle (v0, ring[i], ring[i+1])
        // contributes det[v0, prev, cur] / 6 = ((v0 x prev) . cur)/6.
        // Positive for counter-clockwise rings around outward normals;
        // negative when the shell is inside-out.
        //
        // The ring's vertex sequence is collected first (guarded walk),
        // so the fan below is a plain, provably-terminating loop.
        const CoEdge *first = face->outerLoop->first;
        std::vector<AcGePoint3d> ring;
        const CoEdge *coedge = first;
        int guard = 0;
        while (guard++ < 64)
        {
            ring.push_back(coedge->forward ? coedge->edge->start->point
                                           : coedge->edge->end->point);
            coedge = coedge->next;
            if (coedge == first)
                break;
        }
        if (ring.size() < 3)
            continue;
        {
            static FILE *dbg = std::fopen("brep_vol_debug.log", "w");
            if (dbg)
            {
                std::fprintf(dbg, "[FACE] ring=%zu\n", ring.size());
                for (const AcGePoint3d &pt : ring)
                    std::fprintf(dbg, "  v=(%.2f,%.2f,%.2f)\n",
                                 pt.x, pt.y, pt.z);
                std::fflush(dbg);
            }
        }

        const AcGePoint3d v0 = ring[0];
        const AcGeVector3d v0v(v0.x, v0.y, v0.z);
        for (std::size_t i = 1; i + 1 < ring.size(); ++i)
        {
            const AcGeVector3d pv(ring[i].x, ring[i].y, ring[i].z);
            const AcGeVector3d cv(ring[i + 1].x, ring[i + 1].y,
                                  ring[i + 1].z);
            const AcGeVector3d cross = v0v.crossProduct(pv);
            volume += cross.dotProduct(cv) / 6.0;
        }
    }
    return volume;
}

} // namespace brep
