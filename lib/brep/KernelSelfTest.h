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
#include "brep/BRepBoolean.h"
#include "brep/BRepBlend.h"
#include "brep/BRepRayCast.h"

namespace brep
{

inline int runKernelSelfTest()
{
    // GUI-subsystem processes have no valid stderr handle; the
    // self-test report goes through a real file instead.
    FILE *report = std::fopen("kernel_selftest.log", "w");
    if (report == nullptr)
        return 99;
    int fails = 0;
    auto check = [&](bool condition, const char *what) {
        if (!condition)
        {
            ++fails;
            std::printf("FAIL %s\n", what);
        }
    };

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

    // ---------------- brep (milestone 2) ----------------
    {
        Arena arena;
        std::fflush(report);
        Body *box = makeBox(arena, {0.0, 0.0, 0.0}, 10.0);
        std::fflush(report);
        std::fflush(report);
        const double volume = signedVolume(box->shell);
        std::fflush(report);
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
        std::fflush(report);
        check(validate(box).empty(), "clean box validates with no issues");
        std::fflush(report);

        // Inside-out detection: an inside-out shell has every
        // coedge traversing its edge backwards (all face rings run
        // clockwise around the outward normals).  Radial pairing
        // stays consistent (both sides flip), but the signed volume
        // flips negative.
        Arena inwardArena;
        Body *inward = makeBox(inwardArena, {0.0, 0.0, 0.0}, 10.0);
        for (Face *face = inward->shell->firstFace; face;
             face = face->next)
        {
                Loop *loop = face->outerLoop;
                std::vector<CoEdge *> ring;
                CoEdge *c = loop->first;
                do {
                    ring.push_back(c);
                    c = c->next;
                } while (c != loop->first);
                const std::size_t n = ring.size();
                for (std::size_t i = 0; i < n; ++i)
                {
                    ring[i]->next =
                        ring[(i + n - 1) % n];
                    ring[i]->prev = ring[(i + 1) % n];
                    ring[i]->forward = !ring[i]->forward;
                }
                loop->first = ring[0];
            }
        std::fprintf(report, "flipped-first-fwd=%d\n",
                     int(inward->shell->firstFace->outerLoop->first->forward));
        std::fprintf(report, "inward volume=%.4f\n",
                     signedVolume(inward->shell));
        {
            int fi = 0;
            for (const Face *f = inward->shell->firstFace; f;
                 f = f->next, ++fi)
            {
                const CoEdge *first = f->outerLoop->first;
                const AcGePoint3d v0 = first->forward
                    ? first->edge->start->point
                    : first->edge->end->point;
                const AcGeVector3d v0v(v0.x, v0.y, v0.z);
                double fv = 0.0;
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
                    fv += v0v.crossProduct(pv3).dotProduct(cv3) / 6.0;
                    pc = ce;
                    ce = ce->next;
                }
                std::fprintf(stderr, "inward face[%d] vol=%.4f\n", fi, fv);
                ++fi;
            }
        }
        std::fprintf(report, "POST fwd=%d\n",
                     int(inward->shell->firstFace->outerLoop->first->forward));
        // ---- milestone 3-a: extrudePolygon ----
        {
            Arena extArena;
            AcGePoint3d profile[4] = {{0.0, 0.0, 0.0}, {10.0, 0.0, 0.0},
                                     {10.0, 10.0, 0.0}, {0.0, 10.0, 0.0}};
            AcGeVector3d normal;
            Body *extruded = extrudePolygon(extArena, profile, 4,
                                            {0.0, 0.0, 10.0}, normal);
            check(std::abs(normal.z - 1.0) < 1.0e-9,
                  "extrude profile normal by Newell is +z");
            const double extVol = signedVolume(extruded->shell);
            std::fprintf(report, "extrude volume=%.4f\n", extVol);
            check(std::abs(extVol - 1000.0) < 1.0e-6,
                  "extruded box volume is +1000");
            const auto extIssues = validate(extruded);
            check(extIssues.empty(), "extruded box validates clean");

            std::size_t vtx = 0, edges = 0, faces = 0;
            std::map<const void *, int> seen;
            for (const Face *f = extruded->shell->firstFace; f;
                 f = f->next)
            {
                ++faces;
                const CoEdge *ce2 = f->outerLoop->first;
                do {
                    ++edges;
                    seen[ce2->edge->start] += 1;
                    ce2 = ce2->next;
                } while (ce2 != f->outerLoop->first);
            }
            vtx = seen.size();
            check(faces == 6, "extruded box has 6 faces");
            check(edges == 24, "extruded box has 24 loop coedges (12 edges x 2 sides)");
            check(vtx == 8, "extruded box has 8 deduplicated vertices");
            std::fprintf(report, "extrude faces=%zu coedge-runs=%zu vertices=%zu\n",
                         faces, edges, vtx);
        }
        // ---- milestone 3-b: box-box boolean ----
        {
            Arena boolArena;
            Body *boxA = makeBox(boolArena, {0.0, 0.0, 0.0}, 10.0);
            Body *boxB = makeBox(boolArena, {5.0, 2.0, 3.0}, 10.0);

            std::fprintf(report, "[BOOL] union\n");
            Body *uni = performBoolean(boxA, boxB, BoolOp::Union);
            const double uniVol = signedVolume(uni->shell);
            std::fprintf(report, "[BOOL] union volume=%.4f\n", uniVol);
            check(std::abs(uniVol - 1720.0) < 1.0e-6,
                  "union volume is 1720");
            {
                const auto uniIssues = validate(uni);
                check(uniIssues.empty(), "union validates clean");
                for (const ValidateIssue &issue : uniIssues)
                    std::fprintf(report, "  union issue: %s\n",
                                 validateErrorName(issue.error));
            }

            std::fprintf(report, "[BOOL] intersect\n");
            Body *inter = performBoolean(boxA, boxB,
                                         BoolOp::Intersect);
            const double interVol = signedVolume(inter->shell);
            std::fprintf(report, "[BOOL] intersect volume=%.4f\n", interVol);
            check(std::abs(interVol - 280.0) < 1.0e-6,
                  "intersect volume is 280");
            check(validate(inter).empty(), "intersect validates clean");

            std::fprintf(report, "[BOOL] subtract\n");
            Body *sub = performBoolean(boxA, boxB,
                                       BoolOp::Subtract);
            const double subVol = signedVolume(sub->shell);
            std::fprintf(report, "[BOOL] subtract volume=%.4f\n", subVol);
            check(std::abs(subVol - 720.0) < 1.0e-6,
                  "subtract volume is 720");
            check(validate(sub).empty(), "subtract validates clean");
        }
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

        // ---- milestone 4: chamfer / thicken / fillet / raycast ----
        auto findEdgeByPoints = [](Body *b, const AcGePoint3d &pa,
                                   const AcGePoint3d &pb) -> Edge * {
            for (Face *f = b->shell->firstFace; f != nullptr;
                 f = f->next)
            {
                CoEdge *ce = f->outerLoop->first;
                do {
                    if ((ce->edge->start->point.distanceTo(pa) <
                             1.0e-9 &&
                         ce->edge->end->point.distanceTo(pb) <
                             1.0e-9) ||
                        (ce->edge->start->point.distanceTo(pb) <
                             1.0e-9 &&
                         ce->edge->end->point.distanceTo(pa) <
                             1.0e-9))
                        return ce->edge;
                    ce = ce->next;
                } while (ce != f->outerLoop->first);
            }
            return nullptr;
        };
        auto countFaces = [](const Body *b) {
            int count = 0;
            for (const Face *f = b->shell->firstFace; f != nullptr;
                 f = f->next)
                ++count;
            return count;
        };

        {
            Arena chArena;
            Body *box = makeBox(chArena, {0.0, 0.0, 0.0}, 10.0);
            Edge *target = findEdgeByPoints(
                box, {10.0, 0.0, 10.0}, {10.0, 0.0, 0.0});
            check(target != nullptr, "chamfer target edge found");
            Body *ch = chamferEdge(box, target, 1.0);
            check(ch != nullptr, "chamfer succeeds");
            if (ch != nullptr)
            {
                const double chVol = signedVolume(ch->shell);
                std::fprintf(report, "chamfer volume=%.6f faces=%d\n",
                             chVol, countFaces(ch));
                check(std::abs(chVol - 995.0) < 1.0e-6,
                      "chamfer volume is 995");
                check(validate(ch).empty(), "chamfer validates clean");
                check(countFaces(ch) == 7,
                      "chamfered box has 7 faces");
            }
        }

        {
            Arena thArena;
            Builder builder(thArena);
            std::map<PosKey, Vertex *> vertices;
            std::map<std::pair<PosKey, PosKey>,
                     std::pair<Edge *, bool>> edgeMap;
            Body *sheet = builder.createBody();
            const AcGePoint3d square[4] = {
                {0.0, 0.0, 0.0}, {10.0, 0.0, 0.0},
                {10.0, 10.0, 0.0}, {0.0, 10.0, 0.0}};
            makePlanarFaceFromCorners(builder, sheet->shell, vertices,
                                      edgeMap, square, 4,
                                      {0.0, 0.0, 1.0});
            AcGeVector3d outNormal;
            Body *slab = thickenSheet(thArena, sheet, 2.0, outNormal);
            check(slab != nullptr, "thicken succeeds");
            if (slab != nullptr)
            {
                const double slabVol = signedVolume(slab->shell);
                std::fprintf(report, "thicken volume=%.6f\n", slabVol);
                check(std::abs(slabVol - 200.0) < 1.0e-6,
                      "thickened slab volume is 200");
                check(validate(slab).empty(),
                      "thickened slab validates clean");
                check(std::abs(outNormal.z - 1.0) < 1.0e-9,
                      "thicken normal is +z");
            }
        }

        {
            Arena fiArena;
            Body *box = makeBox(fiArena, {0.0, 0.0, 0.0}, 10.0);
            Edge *target = findEdgeByPoints(
                box, {10.0, 0.0, 10.0}, {10.0, 0.0, 0.0});
            check(target != nullptr, "fillet target edge found");
            Body *fr = filletEdge(box, target, 2.0);
            check(fr != nullptr, "fillet succeeds");
            if (fr != nullptr)
            {
                const double pi = 3.14159265358979323846;
                const double expected =
                    1000.0 - (1.0 - pi / 4.0) * 2.0 * 2.0 * 10.0;
                const double frVol = signedVolume(fr->shell);
                int cylinderFaces = 0;
                int arcEdges = 0;
                std::map<const void *, bool> arcSeen;
                for (const Face *f = fr->shell->firstFace; f != nullptr;
                     f = f->next)
                {
                    if (f->cylindrical)
                        ++cylinderFaces;
                    const CoEdge *ce = f->outerLoop->first;
                    do {
                        if (ce->edge->isArc)
                            arcSeen[ce->edge] = true;
                        ce = ce->next;
                    } while (ce != f->outerLoop->first);
                }
                arcEdges = int(arcSeen.size());
                std::fprintf(report,
                             "fillet volume=%.9f faces=%d cylinders=%d"
                             " arcs=%d\n",
                             frVol, countFaces(fr), cylinderFaces,
                             arcEdges);
                check(std::abs(frVol - expected) < 1.0e-6,
                      "fillet volume is 991.4159");
                check(validate(fr).empty(), "fillet validates clean");
                check(countFaces(fr) == 7, "filleted box has 7 faces");
                check(cylinderFaces == 1,
                      "fillet adds one cylinder face");
                check(arcEdges == 2,
                      "fillet has two shared arc edges");
            }
        }

        {
            Arena rcArena;
            Body *box = makeBox(rcArena, {0.0, 0.0, 0.0}, 10.0);
            auto hit = rayCast(box, {20.0, 5.0, 5.0}, {-1.0, 0.0, 0.0});
            check(hit.has_value(), "raycast hits the box");
            if (hit.has_value())
            {
                check(std::abs(hit->point.x - 10.0) < 1.0e-9 &&
                          std::abs(hit->point.y - 5.0) < 1.0e-9,
                      "raycast hit point on px face");
                check(hit->face != nullptr &&
                          hit->face->surface.normal.x > 0.9,
                      "raycast hit face is px");
                check(std::abs(hit->distance - 10.0) < 1.0e-9,
                      "raycast hit distance is 10");
            }
            auto miss = rayCast(box, {20.0, 5.0, 5.0}, {1.0, 0.0, 0.0});
            check(!miss.has_value(), "raycast away from the box misses");

            // Curved patch picking through the filleted body.
            Arena frArena;
            Body *fbox = makeBox(frArena, {0.0, 0.0, 0.0}, 10.0);
            Edge *target = findEdgeByPoints(
                fbox, {10.0, 0.0, 10.0}, {10.0, 0.0, 0.0});
            Body *fr = filletEdge(fbox, target, 2.0);
            check(fr != nullptr, "fillet for raycast succeeds");
            if (fr != nullptr)
            {
                const double quarter = 0.5857864376269049;
                auto chit = rayCast(fr, {20.0, quarter, 5.0},
                                    {-1.0, 0.0, 0.0});
                check(chit.has_value() && chit->face != nullptr &&
                          chit->face->cylindrical,
                      "raycast hits the fillet cylinder");
                if (chit.has_value())
                {
                    // Tessellated chord error ~0.015 at 16 slices.
                    std::fprintf(report,
                                 "raycast cylinder hit=(%.4f,%.4f,%.4f)"
                                 " n=(%.3f,%.3f,%.3f)\n",
                                 chit->point.x, chit->point.y,
                                 chit->point.z, chit->normal.x,
                                 chit->normal.y, chit->normal.z);
                    check(std::abs(chit->point.x - 9.41421356237) < 0.03,
                          "cylinder hit x matches the arc");
                    check(chit->normal.x > 0.65 &&
                              chit->normal.x < 0.76 &&
                              chit->normal.y < -0.65 &&
                              chit->normal.y > -0.76,
                          "cylinder hit normal is radial");
                }
            }
        }
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
