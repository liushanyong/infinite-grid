#pragma once

// brep::validate — milestone 2's self-checks.  A body must pass all five
// checks to be usable downstream; the orientation check is the one the
// roadmap calls out ("自己检出里外翻"): an inside-out shell carries a
// negative signed volume and fails with kShellInward.

#include <string>
#include <vector>

#include "brep/BRep.h"

namespace brep
{

enum class ValidateError
{
    kOk = 0,
    kEdgeWithoutTwoCoEdges,   // non-manifold or half-built edge
    kCoedgePartnerBroken,     // radial pairing inconsistent
    kLoopNotClosed,           // next-chain fails to close or connect
    kVertexMismatch,          // coedge endpoints disagree with vertices
    kShellInward,             // signed volume negative (inside-out)
    kShellOpen,               // signed volume ~0 with faces present
    kFaceLoopMissing
};

struct ValidateIssue
{
    ValidateError error;
    std::string detail;
};

inline const char *validateErrorName(ValidateError error)
{
    switch (error)
    {
    case ValidateError::kOk: return "ok";
    case ValidateError::kEdgeWithoutTwoCoEdges: return "edge-without-two-coedges";
    case ValidateError::kCoedgePartnerBroken: return "coedge-partner-broken";
    case ValidateError::kLoopNotClosed: return "loop-not-closed";
    case ValidateError::kVertexMismatch: return "vertex-mismatch";
    case ValidateError::kShellInward: return "shell-inward";
    case ValidateError::kShellOpen: return "shell-open";
    case ValidateError::kFaceLoopMissing: return "face-loop-missing";
    }
    return "unknown";
}

inline std::vector<ValidateIssue> validate(const Body *body,
                                           double volumeEpsilon = 1.0e-9)
{
    std::vector<ValidateIssue> issues;
    if (body == nullptr || body->shell == nullptr)
    {
        issues.push_back({ValidateError::kShellOpen, "no shell"});
        return issues;
    }

    // Check 1/2: every edge exactly two coedges, radial partners intact.
    // Walk faces -> loops -> coedges so every edge of the body is seen.
    for (const Face *face = body->shell->firstFace; face != nullptr;
         face = face->next)
    {
        if (face->outerLoop == nullptr)
        {
            issues.push_back({ValidateError::kFaceLoopMissing, "face"});
            continue;
        }
        const CoEdge *first = face->outerLoop->first;
        const CoEdge *coedge = first;
        if (first == nullptr)
        {
            issues.push_back({ValidateError::kFaceLoopMissing, "empty loop"});
            continue;
        }
        std::size_t ringLength = 0;
        do
        {
            const Edge *edge = coedge->edge;
            if (edge == nullptr ||
                edge->coedge[0] == nullptr || edge->coedge[1] == nullptr)
            {
                issues.push_back({ValidateError::kEdgeWithoutTwoCoEdges,
                                  edge ? "edge" : "null edge"});
            }
            else
            {
                const CoEdge *a = edge->coedge[0];
                const CoEdge *b = edge->coedge[1];
                if (a->partner != b || b->partner != a)
                    issues.push_back({ValidateError::kCoedgePartnerBroken,
                                      "radial pair"});
                if (a->forward == b->forward)
                    issues.push_back({ValidateError::kCoedgePartnerBroken,
                                      "same direction"});
            }

            // Check 3/4: loop closure and vertex agreement along the ring.
            const CoEdge *next = coedge->next;
            if (next == nullptr)
            {
                issues.push_back({ValidateError::kLoopNotClosed, "null next"});
            }
            else
            {
                const AcGePoint3d ringEnd =
                    coedge->forward ? coedge->edge->end->point
                                    : coedge->edge->start->point;
                const AcGePoint3d ringNextStart =
                    next->forward ? next->edge->start->point
                                  : next->edge->end->point;
                if (ringEnd.distanceTo(ringNextStart) > 1.0e-9)
                    issues.push_back({ValidateError::kLoopNotClosed,
                                      "gap in ring"});
                const AcGePoint3d declaredEnd =
                    next->forward ? next->edge->start->point
                                  : next->edge->end->point;
                if (ringNextStart.distanceTo(declaredEnd) > 1.0e-9)
                    issues.push_back({ValidateError::kVertexMismatch,
                                      "endpoint != vertex"});
            }
            coedge = next;
            ++ringLength;
            if (ringLength > 4096)
            {
                issues.push_back({ValidateError::kLoopNotClosed,
                                  "ring did not close"});
                break;
            }
        } while (coedge != first);
    }

    // Check 5: shell orientation (the inside-out detector).
    const double volume = signedVolume(body->shell);
    const bool hasFaces = body->shell->firstFace != nullptr;
    if (hasFaces)
    {
        if (volume < -volumeEpsilon)
            issues.push_back({ValidateError::kShellInward,
                              "negative signed volume"});
        else if (volume < volumeEpsilon)
            issues.push_back({ValidateError::kShellOpen,
                              "degenerate volume"});
    }
    return issues;
}

} // namespace brep
