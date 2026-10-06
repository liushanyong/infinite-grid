#pragma once

// STEP writer (namespace brep) — milestone 6-d: AP203/AP214-style
// Part-21 emission of a body as one MANIFOLD_SOLID_BREP.  Entities are
// emitted in strict topological order (points -> vertices -> curves ->
// edges -> oriented edges -> loops -> bounds -> surfaces -> faces ->
// shell -> solid -> product structure), so no forward references ever
// appear.  Line and circle edge geometry are supported (the M4-M6
// surface set); faces carry their ADVANCED_FACE same_sense = .T. with
// surface placements built from the face normals.
//
// There is no ObjectARX STEP API to align with (AutoCAD ships STEP as
// a separate converter), so this file is brep-local by design.

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "brep/BRep.h"

namespace brep
{

namespace detail
{

inline std::string stepReal(double value)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.17g", value);
    std::string text(buffer);
    if (text.find('.') == std::string::npos &&
        text.find('e') == std::string::npos &&
        text.find('E') == std::string::npos)
        text += ".";
    return text;
}

inline std::string stepPoint3(const AcGePoint3d &p)
{
    return "(" + stepReal(p.x) + "," + stepReal(p.y) + "," +
           stepReal(p.z) + ")";
}

inline std::string stepDir3(const AcGeVector3d &v)
{
    return "(" + stepReal(v.x) + "," + stepReal(v.y) + "," +
           stepReal(v.z) + ")";
}

// Stable in-plane reference direction perpendicular to |z|.
inline AcGeVector3d stepRefDir(const AcGeVector3d &z)
{
    AcGeVector3d axis = std::abs(z.z) < 0.9
                            ? AcGeVector3d(0.0, 0.0, 1.0)
                            : AcGeVector3d(1.0, 0.0, 0.0);
    AcGeVector3d ref = axis - z * axis.dotProduct(z);
    const double len = std::sqrt(ref.x * ref.x + ref.y * ref.y +
                                 ref.z * ref.z);
    return AcGeVector3d(ref.x / len, ref.y / len, ref.z / len);
}

} // namespace detail

// Writes |body| to |path| as a single-solid STEP file.  Returns the
// number of entities written (0 on failure).
inline int writeStepBody(const Body *body, const char *path)
{
    if (body == nullptr || body->shell == nullptr || path == nullptr)
        return 0;
    FILE *file = std::fopen(path, "w");
    if (file == nullptr)
        return 0;

    using detail::stepDir3;
    using detail::stepPoint3;
    using detail::stepReal;
    int nextId = 1;
    auto emit = [&](const std::string &text) {
        const int id = nextId++;
        std::fprintf(file, "#%d=%s;\n", id, text.c_str());
        return id;
    };

    std::fprintf(file, "ISO-10303-21;\n");
    std::fprintf(file, "HEADER;\n");
    std::fprintf(file,
                 "FILE_DESCRIPTION(('infinite-grid brep body'),'2;1');\n");
    std::fprintf(file,
                 "FILE_NAME('%s','2026-10-06T00:00:00',(''),(''),"
                 "'infinite-grid brep','infinite-grid','');\n",
                 path);
    std::fprintf(file,
                 "FILE_SCHEMA(('AUTOMOTIVE_DESIGN { 1 0 10303 214 1 1 1 1 "
                 "}'));\n");
    std::fprintf(file, "ENDSEC;\n");
    std::fprintf(file, "DATA;\n");

    std::map<const Vertex *, int> vertexEntity;
    std::map<const Edge *, int> edgeEntity;
    std::vector<int> faceEntities;
    int emitted = 0;

    auto vertexPointId = [&](const Vertex *vertex) {
        auto found = vertexEntity.find(vertex);
        if (found != vertexEntity.end())
            return found->second;
        const int point = emit("CARTESIAN_POINT(''," +
                               stepPoint3(vertex->point) + ")");
        const int id = emit("VERTEX_POINT('',#" +
                            std::to_string(point) + ")");
        vertexEntity[vertex] = id;
        return id;
    };

    auto edgeCurveId = [&](const Edge *edge) {
        auto found = edgeEntity.find(edge);
        if (found != edgeEntity.end())
            return found->second;
        const int startId = vertexPointId(edge->start);
        const int endId = vertexPointId(edge->end);
        int geometry = 0;
        if (edge->isArc)
        {
            const AcGeCircArc3d &arc = edge->arc;
            const int center = emit("CARTESIAN_POINT(''," +
                                    stepPoint3(arc.center) + ")");
            const int zdir = emit("DIRECTION(''," +
                                  stepDir3(arc.normal) + ")");
            const int xdir = emit("DIRECTION(''," +
                                  stepDir3(detail::stepRefDir(
                                      arc.normal)) + ")");
            const int placement =
                emit("AXIS2_PLACEMENT_3D('',#" + std::to_string(center) +
                     ",#" + std::to_string(zdir) + ",#" +
                     std::to_string(xdir) + ")");
            geometry = emit("CIRCLE('',#" + std::to_string(placement) +
                            "," + stepReal(arc.radius) + ")");
        }
        else
        {
            const AcGeVector3d delta =
                edge->end->point - edge->start->point;
            const double length = delta.length();
            const AcGeVector3d dir = length > 0.0
                ? delta * (1.0 / length)
                : AcGeVector3d(1.0, 0.0, 0.0);
            const int point = emit("CARTESIAN_POINT(''," +
                                   stepPoint3(edge->start->point) + ")");
            const int dirEntity = emit("DIRECTION(''," + stepDir3(dir) + ")");
            const int vector = emit("VECTOR('',#" +
                                    std::to_string(dirEntity) + "," +
                                    stepReal(length) + ")");
            geometry = emit("LINE('',#" + std::to_string(point) + ",#" +
                            std::to_string(vector) + ")");
        }
        const int id = emit("EDGE_CURVE('',#" + std::to_string(startId) +
                            ",#" + std::to_string(endId) + ",#" +
                            std::to_string(geometry) + ",.T.)");
        edgeEntity[edge] = id;
        return id;
    };

    for (const Face *face = body->shell->firstFace; face != nullptr;
         face = face->next)
    {
        if (face->outerLoop == nullptr)
            continue;
        std::vector<int> orientedEdges;
        const CoEdge *first = face->outerLoop->first;
        const CoEdge *coedge = first;
        int guard = 0;
        do
        {
            const int edgeId = edgeCurveId(coedge->edge);
            orientedEdges.push_back(emit(
                "ORIENTED_EDGE('',*,*,#" + std::to_string(edgeId) + "," +
                (coedge->forward ? ".T." : ".F.") + ")"));
            coedge = coedge->next;
        } while (coedge != first && guard++ < 256);

        std::string loopRefs;
        for (std::size_t i = 0; i < orientedEdges.size(); ++i)
        {
            if (i > 0)
                loopRefs += ",";
            loopRefs += "#" + std::to_string(orientedEdges[i]);
        }
        const int loop = emit("EDGE_LOOP('',(" + loopRefs + "))");
        const int bound = emit("FACE_OUTER_BOUND('',#" +
                               std::to_string(loop) + ",.T.)");

        const AcGeVector3d z = face->cylindrical
            ? face->cylinder.axis
            : face->surface.normal;
        const AcGePoint3d origin = face->cylindrical
            ? face->cylinder.origin
            : face->surface.origin;
        const int originId = emit("CARTESIAN_POINT(''," +
                                  stepPoint3(origin) + ")");
        const int zdir = emit("DIRECTION(''," + stepDir3(z) + ")");
        const int xdir = emit("DIRECTION(''," +
                              stepDir3(detail::stepRefDir(z)) + ")");
        const int placement =
            emit("AXIS2_PLACEMENT_3D('',#" + std::to_string(originId) +
                 ",#" + std::to_string(zdir) + ",#" +
                 std::to_string(xdir) + ")");
        const int surface =
            face->cylindrical
                ? emit("CYLINDRICAL_SURFACE('',#" +
                       std::to_string(placement) + "," +
                       stepReal(face->cylinder.radius) + ")")
                : emit("PLANE('',#" + std::to_string(placement) + ")");
        const int faceId = emit("ADVANCED_FACE('',(#" +
                                std::to_string(bound) + "),#" +
                                std::to_string(surface) + ",.T.)");
        faceEntities.push_back(faceId);
        ++emitted;
    }
    if (faceEntities.empty())
    {
        std::fclose(file);
        return 0;
    }

    std::string faceRefs;
    for (std::size_t i = 0; i < faceEntities.size(); ++i)
    {
        if (i > 0)
            faceRefs += ",";
        faceRefs += "#" + std::to_string(faceEntities[i]);
    }
    const int shell =
        emit("CLOSED_SHELL('',(" + faceRefs + "))");
    const int solid = emit("MANIFOLD_SOLID_BREP('',#" +
                           std::to_string(shell) + ")");

    // Units, context, and the product structure chain.
    const int lengthUnit =
        emit("(LENGTH_UNIT()NAMED_UNIT(*)SI_UNIT(.MILLI.,.METRE.))");
    const int angleUnit =
        emit("(NAMED_UNIT(*)PLANE_ANGLE_UNIT()SI_UNIT($,.RADIAN.))");
    const int solidAngleUnit =
        emit("(NAMED_UNIT(*)SI_UNIT($,.STERADIAN.))");
    const int uncertainty =
        emit("UNCERTAINTY_MEASURE_WITH_UNIT(LENGTH_MEASURE(1.E-6),#" +
             std::to_string(lengthUnit) + ",'distance_accuracy_value','')");
    const int context =
        emit("(GEOMETRIC_REPRESENTATION_CONTEXT(3)"
             "GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT((#" +
             std::to_string(uncertainty) +
             "))GLOBAL_UNIT_ASSIGNED_CONTEXT((#" +
             std::to_string(lengthUnit) + ",#" +
             std::to_string(angleUnit) + ",#" +
             std::to_string(solidAngleUnit) +
             "))REPRESENTATION_CONTEXT('',''))");
    const int shapeRep =
        emit("ADVANCED_BREP_SHAPE_REPRESENTATION('',(#" +
             std::to_string(solid) + "),#" + std::to_string(context) + ")");
    const int appContext =
        emit("APPLICATION_CONTEXT('core data for automotive mechanical "
             "design processes')");
    const int productContext =
        emit("PRODUCT_CONTEXT('','mechanical','')");
    const int protocol =
        emit("APPLICATION_PROTOCOL_DEFINITION('international standard',"
             "'automotive_design',2000,#" +
             std::to_string(appContext) + ")");
    const int product =
        emit("PRODUCT('body','body','',(#" +
             std::to_string(productContext) + "))");
    const int formation = emit("PRODUCT_DEFINITION_FORMATION('','',#" +
                               std::to_string(product) + ")");
    const int definition = emit("PRODUCT_DEFINITION('design','',#" +
                                std::to_string(formation) + ",#" +
                                std::to_string(protocol) + ")");
    const int definitionShape =
        emit("PRODUCT_DEFINITION_SHAPE('','',#" +
             std::to_string(definition) + ")");
    emit("SHAPE_DEFINITION_REPRESENTATION(#" +
         std::to_string(definitionShape) + ",#" +
         std::to_string(shapeRep) + ")");

    std::fprintf(file, "ENDSEC;\n");
    std::fprintf(file, "END-ISO-10303-21;\n");
    std::fclose(file);
    return nextId - 1;
}

} // namespace brep
