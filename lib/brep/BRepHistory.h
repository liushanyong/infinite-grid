#pragma once

// brep feature history (namespace brep) — milestone 6-c: a kernel-level
// operation journal with parametric replay.  Every recorded operation
// stores only parameters (positions identify edges by their endpoints),
// so truncating the journal and replaying rebuilds the exact solid —
// the brep-side foundation for document undo and parametric edits.
// Undo at this layer is truncation: pop trailing records and replay.

#include <vector>

#include "brep/BRepBlend.h"
#include "brep/BRepBoolean.h"
#include "brep/BRepBuild.h"

namespace brep
{

enum class FeatureKind
{
    MakeBox,     // point = origin, sizeA = size
    ChamferEdge, // lhs = operand body index; point/point2 = edge
                 // endpoints; sizeA = distance
    FilletEdge,  // lhs = operand body index; point/point2 = edge
                 // endpoints; sizeA = radius
    BooleanOp    // lhs/rhs = operand body indices; boolOp
};

struct FeatureRecord
{
    FeatureKind kind = FeatureKind::MakeBox;
    AcGePoint3d point{0.0, 0.0, 0.0};
    AcGePoint3d point2{0.0, 0.0, 0.0};
    double sizeA = 0.0;
    int lhs = -1;
    int rhs = -1;
    BoolOp boolOp = BoolOp::Union;
};

// Finds the edge whose endpoints match |pa|/|pb| in either order.
inline Edge *findEdgeByPoints(Body *body, const AcGePoint3d &pa,
                              const AcGePoint3d &pb)
{
    if (body == nullptr)
        return nullptr;
    for (Face *face = body->shell->firstFace; face != nullptr;
         face = face->next)
    {
        if (face->outerLoop == nullptr)
            continue;
        CoEdge *coedge = face->outerLoop->first;
        int guard = 0;
        do
        {
            if ((coedge->edge->start->point.distanceTo(pa) < 1.0e-9 &&
                 coedge->edge->end->point.distanceTo(pb) < 1.0e-9) ||
                (coedge->edge->start->point.distanceTo(pb) < 1.0e-9 &&
                 coedge->edge->end->point.distanceTo(pa) < 1.0e-9))
                return coedge->edge;
            coedge = coedge->next;
        } while (coedge != face->outerLoop->first && guard++ < 256);
    }
    return nullptr;
}

// Replays the journal into |arena|; returns the last body (nullptr for
// an empty journal).  Blend operations keep their own heap-anchored
// arenas (same as performBoolean), so intermediate bodies stay valid.
inline Body *replayFeatures(Arena &arena,
                            const std::vector<FeatureRecord> &records)
{
    std::vector<Body *> bodies;
    bodies.reserve(records.size());
    for (const FeatureRecord &record : records)
    {
        switch (record.kind)
        {
        case FeatureKind::MakeBox:
            bodies.push_back(makeBox(arena, record.point, record.sizeA));
            break;
        case FeatureKind::ChamferEdge:
            if (record.lhs < 0 ||
                record.lhs >= int(bodies.size()))
                return bodies.empty() ? nullptr : bodies.back();
            bodies.push_back(chamferEdge(
                bodies[std::size_t(record.lhs)],
                findEdgeByPoints(bodies[std::size_t(record.lhs)],
                                 record.point, record.point2),
                record.sizeA));
            break;
        case FeatureKind::FilletEdge:
            if (record.lhs < 0 ||
                record.lhs >= int(bodies.size()))
                return bodies.empty() ? nullptr : bodies.back();
            bodies.push_back(filletEdge(
                bodies[std::size_t(record.lhs)],
                findEdgeByPoints(bodies[std::size_t(record.lhs)],
                                 record.point, record.point2),
                record.sizeA));
            break;
        case FeatureKind::BooleanOp:
            if (record.lhs < 0 || record.rhs < 0 ||
                record.lhs >= int(bodies.size()) ||
                record.rhs >= int(bodies.size()))
                return bodies.empty() ? nullptr : bodies.back();
            bodies.push_back(performBoolean(
                bodies[std::size_t(record.lhs)],
                bodies[std::size_t(record.rhs)], record.boolOp));
            break;
        }
    }
    return bodies.empty() ? nullptr : bodies.back();
}

} // namespace brep
