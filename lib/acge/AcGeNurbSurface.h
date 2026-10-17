#pragma once

#include "AcGeBspline.h"

// ObjectARX names the concrete NURBS surface class AcGeNurbSurface (the
// AcGeNurbsSurface spelling exists as an older alias); this port's NURBS
// surface value type is AcGeNurbsSurface3d, so the documented spelling is
// provided as an alias.
using AcGeNurbSurface = ge::AcGeNurbsSurface3d;
