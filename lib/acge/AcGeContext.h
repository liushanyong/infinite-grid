#pragma once

#include "AcGePort.h"

#include <cstddef>

#include "AcGePoint.h"
#include "AcGeTol.h"

// Source-level AcGeContext aligned with the ObjectARX reference (gegbl.h):
// the library-global tunables used as defaults throughout GeLib.
//
// gTol is the "global" default tolerance object; gOrthoVector computes an
// orthogonal vector for an input vector; gErrorFunc is invoked by library
// error paths. The allocator hooks exist for API-shape parity only -- this
// port uses plain C++ allocation and never calls them. All members are
// C++17 inline variables so the header stays translation-unit independent.

struct ACGE_PORT AcGeContext
{
    inline static AcGeTol gTol{};

    inline static void (*gErrorFunc)() = nullptr;
    inline static void (*gOrthoVector)(const AcGeVector3d &, AcGeVector3d &) =
        [](const AcGeVector3d &vector, AcGeVector3d &ortho)
    {
        ortho = vector.perpVector();
    };

    inline static void *(*gAllocMem)(std::size_t) = nullptr;
    inline static void (*gFreeMem)(void *) = nullptr;

    // Convenience accessor mirroring ARX usage patterns: the global
    // tolerance used as a default argument throughout the library.
    static const AcGeTol &gTolerance() { return gTol; }
};