#pragma once

// brep (namespace brep) — milestone 2: the half-edge (coedge) topology
// over the arena, in the ACIS/OBJ spirit referenced by the roadmap:
//
//   Body ─ Shell ─ Face ─ Loop ─ CoEdge ─ Edge ─ Vertex
//
// Each Edge carries exactly two CoEdges (one per adjacent face, opposite
// radial directions).  Each Loop is a doubly-linked ring of CoEdges
// traversed counter-clockwise around outward-facing face normals.  With
// that convention a closed shell's signed volume is positive; a shell
// built inside-out validates negative — brep::validate reports it.

#include <cstdint>
#include <optional>
#include <string>

#include "brep/Arena.h"
#include "ge/gepoint.h"

namespace brep
{

class CoEdge;
class Edge;
class Loop;
class Face;
class Shell;
class Body;

// ---- geometry payload (M2 keeps planar faces; curves arrive with M3) ----

struct Plane
{
    AcGePoint3d origin{0.0, 0.0, 0.0};
    AcGeVector3d normal{0.0, 0.0, 1.0};

    double distanceTo(const AcGePoint3d &point) const
    {
        const AcGeVector3d delta = point - origin;
        return delta.dotProduct(normal);
    }
};

// ---- topology ----

struct Vertex
{
    AcGePoint3d point{0.0, 0.0, 0.0};
    CoEdge *firstCoEdge = nullptr; // one incident coedge (spoke)
};

struct Edge
{
    Vertex *start = nullptr;
    Vertex *end = nullptr;
    CoEdge *coedge[2] = {nullptr, nullptr}; // the two sides
};

struct CoEdge
{
    Edge *edge = nullptr;
    Loop *loop = nullptr;
    CoEdge *next = nullptr; // around the loop
    CoEdge *prev = nullptr; // around the loop
    CoEdge *partner = nullptr; // radial mate on the other face
    bool forward = true; // traversal along edge start->end when true
};

struct Loop
{
    CoEdge *first = nullptr; // entry into the ring
    Face *face = nullptr;
};

struct Face
{
    Loop *outerLoop = nullptr; // single loop in M2
    Shell *shell = nullptr;
    Plane surface;
    Face *next = nullptr; // shell face list
};

struct Shell
{
    Face *firstFace = nullptr;
    Body *body = nullptr;
};

struct Body
{
    Shell *shell = nullptr;
};

// ---- construction helpers (arena-owned) ----

class Builder
{
public:
    explicit Builder(Arena &arena) : arena_(arena) {}

    Vertex *createVertex(const AcGePoint3d &point)
    {
        Vertex *vertex = arena_.create<Vertex>();
        vertex->point = point;
        return vertex;
    }

    // Creates an edge with both coedges (radial pair), linking the
    // vertices' spokes.
    Edge *createEdge(Vertex *start, Vertex *end)
    {
        Edge *edge = arena_.create<Edge>();
        edge->start = start;
        edge->end = end;
        CoEdge *forward = arena_.create<CoEdge>();
        CoEdge *reverse = arena_.create<CoEdge>();
        forward->edge = edge;
        forward->forward = true;
        reverse->edge = edge;
        reverse->forward = false;
        forward->partner = reverse;
        reverse->partner = forward;
        edge->coedge[0] = forward;
        edge->coedge[1] = reverse;
        // Spoke insertion (any incident coedge works).
        if (start->firstCoEdge == nullptr)
            start->firstCoEdge = forward;
        if (end->firstCoEdge == nullptr)
            end->firstCoEdge = reverse;
        return edge;
    }

    // Creates a loop from an ordered list of coedges, closing the ring
    // and assigning loop/face back-pointers.
    Loop *createLoop(Face *face, CoEdge **coedges, std::size_t count)
    {
        Loop *loop = arena_.create<Loop>();
        loop->face = face;
        for (std::size_t i = 0; i < count; ++i)
        {
            coedges[i]->loop = loop;
            coedges[i]->next = coedges[(i + 1) % count];
            coedges[i]->prev = coedges[(i + count - 1) % count];
        }
        loop->first = coedges[0];
        return loop;
    }

    Face *createFace(Shell *shell, const Plane &surface)
    {
        Face *face = arena_.create<Face>();
        face->shell = shell;
        face->surface = surface;
        face->next = shell->firstFace;
        shell->firstFace = face;
        return face;
    }

    Shell *createShell(Body *body)
    {
        Shell *shell = arena_.create<Shell>();
        shell->body = body;
        return shell;
    }

    Body *createBody()
    {
        Body *body = arena_.create<Body>();
        body->shell = createShell(body);
        return body;
    }

private:
    Arena &arena_;
};

// Signed volume of a shell by the divergence theorem: sum over faces of
// (1/6) * faceNormalArea dot any point on the face.  Positive for an
// outward-oriented closed shell, negative when inside-out.
double signedVolume(const Shell *shell);

} // namespace brep
