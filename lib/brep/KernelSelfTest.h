#pragma once

// Kernel self-test (hidden runtime mode): GRID_SELFTEST=kernel runs the
// geom2d + brep milestone checks in-process inside the trusted WINDOW
// host, printing PASS/FAIL and setting the process exit path.  Exists
// because freshly linked small test executables from temp directories
// are flagged by antivirus on this machine.

#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

#include "ge/ge2d.h"
#include "brep/BRepBuild.h"
#include "brep/Validate.h"

namespace brep
{

inline int runKernelSelfTest()
{
    // GUI-subsystem processes have no valid stderr handle; the
    // self-test report goes through a real file instead.
    FILE *report = std::fopen("kernel_selftest.log", "w");
    if (report == nullptr)
        return 99;
    std::fprintf(report, "[ST] enter\n");
    int fails = 0;
    auto check = [&](bool condition, const char *what) {
        if (!condition)
        {
            ++fails;
            std::printf("FAIL %s\n", what);
        }
    };

    std::fprintf(report, "[ST] geom2d start\n");
    // ---------------- geom2d (milestone 1 tail) ----------------
    {
        using namespace ge;
        AcGeLineSeg2d a{{0.0, 0.0}, {4.0, 0.0}};
        AcGeLineSeg2d cross{{2.0, -2.0}, {2.0, 2.0}};
        auto hit = intersect(a, cross);
        check(hit.size() == 1, "line-line crossing yields one point");
        check(!hit.empty() && std::abs(hit[0].x - 2.0) < 1.0e-9 &&
                  (!hit.empty() && std::abs(hit[0].y) < 1.0e-9),
              "line-line crossing point");

        AcGeLineSeg2d par{{0.0, 1.0}, {4.0, 1.0}};
        check(intersect(a, par).empty(), "parallel lines yield none");

        AcGeCircle2d unit{{0.0, 0.0}, 1.0};
        AcGeLineSeg2d through{{-2.0, 0.0}, {2.0, 0.0}};
        check(intersect(through, unit).size() == 2,
              "line-circle yields two points");
        AcGeLineSeg2d tangentY{{-2.0, 1.0}, {2.0, 1.0}};
        auto tangentHit = intersect(tangentY, unit);
        check(tangentHit.size() == 1, "tangent line yields one point");

        AcGeCircle2d offset{{1.0, 0.0}, 1.0};
        check(intersect(unit, offset).size() == 2,
              "circle-circle yields two points");

        AcGeCircArc2d quarter{unit.center, 1.0, 0.0, 1.5707963267948966};
        AcGeLineSeg2d diagonal{{-2.0, -2.0}, {2.0, 2.0}};
        auto arcHit = intersect(diagonal, quarter);
        check(arcHit.size() == 1 &&
                  !arcHit.empty() &&
                  std::abs(arcHit[0].x - 0.7071067811865476) < 1.0e-6,
              "line-arc filters to the quarter span");
        auto arcArc = intersect(
            quarter, AcGeCircArc2d{{1.0, 0.0}, 1.0, 0.0, 6.283185307179586});
        check(arcArc.size() == 1 &&
                  !arcArc.empty() &&
                  std::abs(arcArc[0].y - 0.8660254037844386) < 1.0e-6,
              "arc-arc filters to both spans");
    }

    std::fprintf(report, "[ST] brep start\n");
    // ---------------- brep (milestone 2) ----------------
    {
        Arena arena;
        Body *box = makeBox(arena, {0.0, 0.0, 0.0}, 10.0);
        const double volume = signedVolume(box->shell);
        {
            int fi = 0;
            for (const Face *f = box->shell->firstFace; f;
                 f = f->next, ++fi)
            {
                const CoEdge *first = f->outerLoop->first;
                const AcGePoint3d v0 = first->forward
                    ? first->edge->start->point
                    : first->edge->end->point;
                const AcGeVector3d v0v(v0.x, v0.y, v0.z);
                double faceVol = 0.0;
                const CoEdge *pc = first;
                const CoEdge *ce = first->next;
                while (ce != first)
                {
                    const AcGePoint3d pv = pc->forward
                        ? pc->edge->end->point : pc->edge->start->point;
                    const AcGePoint3d cv = ce->forward
                        ? ce->edge->end->point : ce->edge->start->point;
                    const AcGeVector3d pv3(pv.x, pv.y, pv.z);
                    const AcGeVector3d cv3(cv.x, cv.y, cv.z);
                    faceVol += v0v.crossProduct(pv3).dotProduct(cv3) / 6.0;
                    pc = ce;
                    ce = ce->next;
                }
                std::fprintf(report, "face[%d] vol=%.4f normal=(%.1f,%.1f,%.1f)\n",
                             fi, faceVol, f->surface.normal.x,
                             f->surface.normal.y, f->surface.normal.z);
                ++fi;
            }
        }

        std::printf("volume=%.6f\n", volume);
        check(std::abs(volume - 1000.0) < 1.0e-6,
              "box signed volume is +1000");
        {
            int fi = 0;
            for (const Face *f = box->shell->firstFace; f;
                 f = f->next, ++fi)
            {
                const CoEdge *first = f->outerLoop->first;
                const AcGePoint3d v0 = first->forward
                    ? first->edge->start->point
                    : first->edge->end->point;
                const AcGeVector3d v0v(v0.x, v0.y, v0.z);
                double faceVol = 0.0;
                int ringLen = 0;
                const CoEdge *pc = first;
                const CoEdge *ce = first->next;
                while (ce != first)
                {
                    ++ringLen;
                    const AcGePoint3d pv = pc->forward
                        ? pc->edge->end->point : pc->edge->start->point;
                    const AcGePoint3d cv = ce->forward
                        ? ce->edge->end->point : ce->edge->start->point;
                    const AcGeVector3d pv3(pv.x, pv.y, pv.z);
                    const AcGeVector3d cv3(cv.x, cv.y, cv.z);
                    const double triDet =
                        v0v.crossProduct(pv3).dotProduct(cv3) / 6.0;
                    faceVol += triDet;
                    std::fprintf(report, "   tri det=%.4f\n", triDet);
                    pc = ce;
                    ce = ce->next;
                }
                std::fprintf(report, "face[%d] ring=%d closed=%s vol=%.4f normal=(%.1f,%.1f,%.1f)\n",
                             fi, ringLen, ce == first ? "yes" : "NO", faceVol,
                             f->surface.normal.x, f->surface.normal.y,
                             f->surface.normal.z);
                {
                    const brep::CoEdge *ce2 = first;
                    int k = 0;
                    do {
                        std::fprintf(report, "   [%d] ce=%p edge=%p next=%p fwd=%d start=(%.1f,%.1f,%.1f)\n",
                                     k, (const void*)ce2, (const void*)ce2->edge,
                                     (const void*)ce2->next, int(ce2->forward),
                                     ce2->edge->start->point.x,
                                     ce2->edge->start->point.y,
                                     ce2->edge->start->point.z);
                        ce2 = ce2->next;
                        ++k;
                    } while (ce2 != first && k < 8);
                }
                ++fi;
            }
        }
        check(validate(box).empty(), "clean box validates with no issues");

        // Inside-out detection: an inside-out shell has every
        // coedge traversing its edge backwards (all face rings run
        // clockwise around the outward normals).  Radial pairing
        // stays consistent (both sides flip), but the signed volume
        // flips negative.
        Arena inwardArena;
        Body *inward = makeBox(inwardArena, {0.0, 0.0, 0.0}, 10.0);
        for (const Face *face = inward->shell->firstFace; face;
             face = face->next)
            for (const CoEdge *ce = face->outerLoop->first;;)
            {
                const_cast<CoEdge *>(ce)->forward =
                    !ce->forward;
                ce = ce->next;
                if (ce == face->outerLoop->first)
                    break;
            }
        std::fprintf(report, "flipped-first-fwd=%d\n",
                     int(inward->shell->firstFace->outerLoop->first->forward));
        std::fprintf(report, "inward volume=%.4f\n",
                     signedVolume(inward->shell));
        bool inwardDetected = false;
        for (const ValidateIssue &issue : validate(inward))
            if (issue.error == ValidateError::kShellInward)
                inwardDetected = true;
        check(inwardDetected, "inside-out shell detected");
        check(signedVolume(inward->shell) < 0.0,
              "inside-out volume negative");

        // Broken radial pairing detected.
        Arena brokenArena;
        Body *broken = makeBox(brokenArena, {0.0, 0.0, 0.0}, 10.0);
        {
            Face *face = broken->shell->firstFace;
            Edge *edge = face->outerLoop->first->edge;
            edge->coedge[0]->partner = edge->coedge[0];
            edge->coedge[1]->partner = edge->coedge[1];
        }
        bool partnerDetected = false;
        for (const ValidateIssue &issue : validate(broken))
            if (issue.error == ValidateError::kCoedgePartnerBroken)
                partnerDetected = true;
        check(partnerDetected, "broken radial pairing detected");
    }

    std::fprintf(report, fails == 0 ? "KERNEL SELFTEST PASSED\n"
                                : "KERNEL SELFTEST FAILED\n");
    std::fflush(report);
    std::fclose(report);
    std::printf(fails == 0 ? "KERNEL SELFTEST PASSED\n"
                           : "KERNEL SELFTEST FAILED\n");
    return fails;
}

} // namespace brep
