#pragma once

// AcDbEntity transform helpers (namespace acdb) — ObjectARX
// AcDbEntity::transformBy over the value-type payloads.  Translation
// covers every supported type; the matrix overload applies to the types
// whose geometry is plain point sets (block-reference rotations and
// non-uniform scales of curves degrade through their sampled forms, so
// they are out of scope here).

#include "acdb/AcDbEntities.h"

namespace acdb
{

// AcDbEntity::transformBy (translation): shifts every geometric point of
// |payload| by |offset|.
inline void transformBy(AcDbEntityVariant &payload,
                        const AcGeVector3d &offset)
{
    std::visit([&](auto &entity) {
        using T = std::decay_t<decltype(entity)>;
        if constexpr (std::is_same_v<T, AcDbBlockReference>)
        {
            entity.position += offset;
        }
        else if constexpr (std::is_same_v<T, AcDbLine>)
        {
            entity.start += offset;
            entity.end += offset;
        }
        else if constexpr (std::is_same_v<T, AcDbRay>)
        {
            entity.start += offset;
        }
        else if constexpr (std::is_same_v<T, AcDbArc> ||
                           std::is_same_v<T, AcDbCircle> ||
                           std::is_same_v<T, AcDbEllipse>)
        {
            entity.center += offset;
        }
        else if constexpr (std::is_same_v<T, AcDbPoint>)
        {
            entity.location += offset;
        }
        else if constexpr (std::is_same_v<T, AcDbXline>)
        {
            entity.point += offset;
        }
        else if constexpr (std::is_same_v<T, AcDbSolid>)
        {
            entity.firstCorner += offset;
            entity.secondCorner += offset;
            entity.thirdCorner += offset;
            entity.fourthCorner += offset;
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
                vertex += offset;
        }
        else if constexpr (std::is_same_v<T, AcDbSpline>)
        {
            for (auto &point : entity.controlPoints)
                point += offset;
            for (auto &point : entity.fitPoints)
                point += offset;
        }
        else if constexpr (std::is_same_v<T, AcDbText> ||
                           std::is_same_v<T, AcDbMText>)
        {
            entity.insertion += offset;
        }
        else if constexpr (std::is_same_v<T, AcDbLight>)
        {
            entity.position += offset;
            entity.target += offset;
        }
        else if constexpr (std::is_same_v<T, AcDbHatch>)
        {
            for (auto &point : entity.outerLoop)
                point += offset;
            for (auto &loop : entity.innerLoops)
                for (auto &point : loop)
                    point += offset;
        }
        else if constexpr (std::is_same_v<T, AcDbPolyFaceMesh>)
        {
            for (auto &position : entity.geometry.positions)
                position += offset;
        }
        else if constexpr (std::is_same_v<T, AcDbMline>)
        {
            for (auto &vertex : entity.vertices)
                vertex += offset;
        }
        else if constexpr (std::is_same_v<T, AcDb3dSolid>)
        {
            for (auto &vertex : entity.vertices)
                vertex += offset;
        }
        else if constexpr (std::is_same_v<T, AcDbRotatedDimension> ||
                           std::is_same_v<T, AcDbAlignedDimension>)
        {
            entity.xLine1Point += offset;
            entity.xLine2Point += offset;
            entity.dimensionLinePoint += offset;
        }
        else if constexpr (std::is_same_v<T, AcDb2LineAngularDimension>)
        {
            entity.xLine1StartPoint += offset;
            entity.xLine1EndPoint += offset;
            entity.xLine2StartPoint += offset;
            entity.xLine2EndPoint += offset;
            entity.arcPoint += offset;
        }
        else if constexpr (std::is_same_v<T, AcDb3PointAngularDimension>)
        {
            entity.centerPoint += offset;
            entity.xLine1Point += offset;
            entity.xLine2Point += offset;
        }
        else if constexpr (std::is_same_v<T, AcDbRadialDimension>)
        {
            entity.centerPoint += offset;
            entity.radiusPoint += offset;
        }
        else if constexpr (std::is_same_v<T, AcDbDiametricDimension>)
        {
            entity.chordPoint += offset;
            entity.farChordPoint += offset;
        }
        else if constexpr (std::is_same_v<T, AcDbOrdinateDimension>)
        {
            entity.featureLocationPoint += offset;
            entity.leaderEndPoint += offset;
        }
        else if constexpr (std::is_same_v<T, AcDbLeader>)
        {
            for (auto &point : entity.points)
                point += offset;
        }
        else if constexpr (std::is_same_v<T, AcDbFcf> ||
                           std::is_same_v<T, AcDbShape> ||
                           std::is_same_v<T, AcDbAttributeDefinition> ||
                           std::is_same_v<T, AcDbAttribute>)
        {
            entity.insertion += offset;
        }
        else if constexpr (std::is_same_v<T, AcDbMInsertBlock>)
        {
            entity.position += offset;
        }
        else
        {
            static_assert(sizeof(T) == 0, "unhandled AcDb entity type");
        }
    }, payload);
}

} // namespace acdb
