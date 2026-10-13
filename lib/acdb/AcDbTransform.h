#pragma once

// AcDbEntity transform helpers (namespace acdb) — ObjectARX
// AcDbEntity::transformBy over the value-type payloads.  Translation
// covers every supported type; the matrix overload applies to the types
// whose geometry is plain point sets (block-reference rotations and
// non-uniform scales of curves degrade through their sampled forms, so
// they are out of scope here).

#include <glm/glm.hpp>

#include "acdb/AcDbEntities.h"

namespace acdb
{

// AcDbEntity::transformBy (translation): shifts every geometric point of
// |payload| by |offset|.
inline void transformBy(AcDbEntityVariant &payload, const glm::dvec3 &offset)
{
    // AcGe value types add vectors, not points.
    const AcGeVector3d shift(offset.x, offset.y, offset.z);
    std::visit([&](auto &entity) {
        using T = std::decay_t<decltype(entity)>;
        if constexpr (std::is_same_v<T, AcDbBlockReference>)
        {
            // AcGePoint3d takes the offset as a vector.
            entity.position += AcGeVector3d(offset);
        }
        else if constexpr (std::is_same_v<T, AcDbLine>)
        {
            entity.start += shift;
            entity.end += shift;
        }
        else if constexpr (std::is_same_v<T, AcDbRay>)
        {
            entity.start += shift;
        }
        else if constexpr (std::is_same_v<T, AcDbArc> ||
                           std::is_same_v<T, AcDbCircle> ||
                           std::is_same_v<T, AcDbEllipse>)
        {
            entity.center += shift;
        }
        else if constexpr (std::is_same_v<T, AcDbPoint>)
        {
            entity.location += shift;
        }
        else if constexpr (std::is_same_v<T, AcDbXline>)
        {
            entity.point += shift;
        }
        else if constexpr (std::is_same_v<T, AcDbSolid>)
        {
            entity.firstCorner += shift;
            entity.secondCorner += shift;
            entity.thirdCorner += shift;
            entity.fourthCorner += shift;
        }
        else if constexpr (std::is_same_v<T, AcDbPolyline>)
        {
            // LWPOLYLINE: 2D vertices shift in-plane; elevation carries
            // the height.
            const AcGeVector2d shift2(offset.x, offset.y);
            for (auto &vertex : entity.vertices)
                vertex += shift2;
            entity.elevation += offset.z;
        }
        else if constexpr (std::is_same_v<T, AcDb3dPolyline>)
        {
            for (auto &vertex : entity.vertices)
                vertex += shift;
        }
        else if constexpr (std::is_same_v<T, AcDbSpline>)
        {
            for (auto &point : entity.controlPoints)
                point += shift;
            for (auto &point : entity.fitPoints)
                point += shift;
        }
        else if constexpr (std::is_same_v<T, AcDbText> ||
                           std::is_same_v<T, AcDbMText>)
        {
            entity.insertion += shift;
        }
        else if constexpr (std::is_same_v<T, AcDbLight>)
        {
            entity.position += shift;
            entity.target += shift;
        }
        else if constexpr (std::is_same_v<T, AcDbHatch>)
        {
            for (auto &point : entity.outerLoop)
                point += shift;
            for (auto &loop : entity.innerLoops)
                for (auto &point : loop)
                    point += shift;
        }
        else if constexpr (std::is_same_v<T, AcDbPolyFaceMesh>)
        {
            // glm positions take the glm offset directly.
            for (auto &position : entity.geometry.positions)
                position += AcGeVector3d(offset);
        }
        else if constexpr (std::is_same_v<T, AcDbMline>)
        {
            for (auto &vertex : entity.vertices)
                vertex += shift;
        }
        else if constexpr (std::is_same_v<T, AcDb3dSolid>)
        {
            for (auto &vertex : entity.vertices)
                vertex += shift;
        }
        else if constexpr (std::is_same_v<T, AcDbRotatedDimension> ||
                           std::is_same_v<T, AcDbAlignedDimension>)
        {
            entity.xLine1Point += shift;
            entity.xLine2Point += shift;
            entity.dimensionLinePoint += shift;
        }
        else if constexpr (std::is_same_v<T, AcDb2LineAngularDimension>)
        {
            entity.xLine1StartPoint += shift;
            entity.xLine1EndPoint += shift;
            entity.xLine2StartPoint += shift;
            entity.xLine2EndPoint += shift;
            entity.arcPoint += shift;
        }
        else if constexpr (std::is_same_v<T, AcDb3PointAngularDimension>)
        {
            entity.centerPoint += shift;
            entity.xLine1Point += shift;
            entity.xLine2Point += shift;
        }
        else if constexpr (std::is_same_v<T, AcDbRadialDimension>)
        {
            entity.centerPoint += shift;
            entity.radiusPoint += shift;
        }
        else if constexpr (std::is_same_v<T, AcDbDiametricDimension>)
        {
            entity.chordPoint += shift;
            entity.farChordPoint += shift;
        }
        else if constexpr (std::is_same_v<T, AcDbOrdinateDimension>)
        {
            entity.featureLocationPoint += shift;
            entity.leaderEndPoint += shift;
        }
        else if constexpr (std::is_same_v<T, AcDbLeader>)
        {
            for (auto &point : entity.points)
                point += shift;
        }
        else if constexpr (std::is_same_v<T, AcDbFcf> ||
                           std::is_same_v<T, AcDbShape> ||
                           std::is_same_v<T, AcDbAttributeDefinition> ||
                           std::is_same_v<T, AcDbAttribute>)
        {
            entity.insertion += shift;
        }
        else if constexpr (std::is_same_v<T, AcDbMInsertBlock>)
        {
            entity.position += shift;
        }
        else
        {
            static_assert(sizeof(T) == 0, "unhandled AcDb entity type");
        }
    }, payload);
}

} // namespace acdb
