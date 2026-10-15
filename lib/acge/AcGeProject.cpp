#include "acge/AcGeProject.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace ge
{

std::optional<SurfaceProjection> projectPointOnSurface(
    const AcGeNurbsSurface3d &surface, const AcGePoint3d &point,
    double tolerance)
{
    constexpr int kSeedCount = 9; // 9x9 grid of seeds against local minima
    const double spanU = surface.maxParameterU() - surface.minParameterU();
    const double spanV = surface.maxParameterV() - surface.minParameterV();
    std::optional<SurfaceProjection> best;

    for (int su = 0; su < kSeedCount; ++su)
    {
        for (int sv = 0; sv < kSeedCount; ++sv)
        {
            double u = surface.minParameterU() +
                       spanU * double(su) / (kSeedCount - 1);
            double v = surface.minParameterV() +
                       spanV * double(sv) / (kSeedCount - 1);
            for (int iteration = 0; iteration < 32; ++iteration)
            {
                const auto derivatives =
                    surface.derivativesAt(u, v, 2, 2);
                const AcGeVector3d &su1 = derivatives[1][0];
                const AcGeVector3d &sv1 = derivatives[0][1];
                const AcGeVector3d difference =
                    surface.pointAt(u, v) - point;
                const double k11 = su1.dotProduct(su1);
                const double k12 = su1.dotProduct(sv1);
                const double k22 = sv1.dotProduct(sv1);
                const double determinant = k11 * k22 - k12 * k12;
                if (std::abs(determinant) < 1.0e-14)
                    break;
                const double rhsU = -su1.dotProduct(difference);
                const double rhsV = -sv1.dotProduct(difference);
                const double deltaU = (k22 * rhsU - k12 * rhsV) / determinant;
                const double deltaV = (k11 * rhsV - k12 * rhsU) / determinant;
                u = std::clamp(u + deltaU, surface.minParameterU(),
                               surface.maxParameterU());
                v = std::clamp(v + deltaV, surface.minParameterV(),
                               surface.maxParameterV());
                if (std::abs(deltaU) * spanU < 1.0e-12 &&
                    std::abs(deltaV) * spanV < 1.0e-12)
                    break;
            }
            const AcGePoint3d closest = surface.pointAt(u, v);
            const double distance = (closest - point).length();
            if (distance <= tolerance)
                return SurfaceProjection{u, v, closest, distance};
            if (!best || distance < best->distance)
                best = SurfaceProjection{u, v, closest, distance};
        }
    }
    return best;
}

} // namespace ge
