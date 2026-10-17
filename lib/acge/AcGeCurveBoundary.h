#pragma once

#include "AcGePort.h"

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <vector>

#include "AcGeArrays.h"
#include "AcGePoint.h"

// Source-level AcGeCurveBoundary aligned with the ObjectARX reference
// (gecbndry.h): the boundary geometry of a bounded surface. One instance
// holds a loop of elements; each element is either a 3D space curve or a 3D
// position (degenerate element), optionally paired one-to-one with a
// parameter-space (2D) curve or position, plus per-element orientation
// flags for both arrays.
//
// Because this port has no AcGeEntity3d/AcGeCurve2d hierarchy, both
// contours are type-erased: a 3D element is a callback returning
// pointAt(double) (empty callback == degenerate position), and likewise
// for the 2D contour. Ownership of the stored curve callables follows the
// ARX ownerOfCurves flag semantics (value semantics here; the flag is
// tracked for API parity).

class ACGE_PORT AcGeCurveBoundary
{
public:
    // One element of the 3D space contour. An empty `curve` callback marks
    // a degenerate element whose geometry is `position`.
    struct Contour3dElement
    {
        std::function<AcGePoint3d(double)> curve;
        AcGePoint3d position{0.0, 0.0, 0.0};
        bool orientation = true;
    };

    // One element of the parameter-space contour.
    struct Contour2dElement
    {
        std::function<AcGePoint2d(double)> curve;
        AcGePoint2d position{0.0, 0.0};
        bool orientation = true;
    };

    AcGeCurveBoundary() = default;

    AcGeCurveBoundary(const std::vector<Contour3dElement> &contour3d,
                      const std::vector<Contour2dElement> &contour2d,
                      bool ownerOfCurves = false)
    {
        set(contour3d, contour2d, ownerOfCurves);
    }

    AcGeCurveBoundary &set(const std::vector<Contour3dElement> &contour3d,
                           const std::vector<Contour2dElement> &contour2d,
                           bool ownerOfCurves = false)
    {
        if (!contour3d.empty() && !contour2d.empty() &&
            contour3d.size() != contour2d.size())
            throw std::invalid_argument(
                "CurveBoundary 3D and 2D contours must correspond "
                "one-to-one.");
        if (contour3d.empty() && contour2d.empty())
            throw std::invalid_argument(
                "CurveBoundary requires a non-empty contour.");
        m_contour3d = contour3d;
        m_contour2d = contour2d;
        m_ownerOfCurves = ownerOfCurves;
        return *this;
    }

    int numElements() const
    {
        return static_cast<int>(
            m_contour3d.empty() ? m_contour2d.size() : m_contour3d.size());
    }

    void getContour(std::vector<Contour3dElement> &contour3d,
                    std::vector<Contour2dElement> &contour2d) const
    {
        contour3d = m_contour3d;
        contour2d = m_contour2d;
    }

    // Degenerate boundary: a single element that is a position.
    bool isDegenerate() const
    {
        return numElements() == 1 && isPosition3d(0);
    }

    // ARX overload: returns the degenerate position and, when present, the
    // paired parameter-space curve.
    bool isDegenerate(AcGePoint3d &position) const
    {
        if (!isDegenerate())
            return false;
        position = m_contour3d.front().position;
        return true;
    }

    bool isOwnerOfCurves() const { return m_ownerOfCurves; }

    // Value semantics mean the contour callables are always owned by this
    // object; the flag only records the ARX ownership request.
    void setToOwnCurves() { m_ownerOfCurves = true; }

private:
    bool isPosition3d(int index) const
    {
        return !static_cast<bool>(m_contour3d.at(static_cast<std::size_t>(index)).curve);
    }

    std::vector<Contour3dElement> m_contour3d;
    std::vector<Contour2dElement> m_contour2d;
    bool m_ownerOfCurves = false;
};