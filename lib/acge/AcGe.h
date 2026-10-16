#pragma once

#include "AcGePort.h"

// Umbrella header for the AcGe-compatible geometry value-type layer.
// Signatures follow the ObjectARX AcGe kernel (see ObjectARX docs,
// "AcGe Overview"); the implementation is original and glm-interopable.

#include "AcGePoint.h"
#include "AcGeTol.h"
#include "AcGeMatrix2d.h"
#include "AcGeMatrix.h"
#include "AcGeQuaternion.h"
#include "AcGeInterval.h"
#include "AcGeBoundedPlane.h"
#include "AcGeSurface3d.h"
#include "AcGeLinear.h"
#include "AcGeScale.h"
#include "AcGeCurve3d.h"
#include "AcGeConic.h"
#include "AcGeIntersect3d.h"
#include "AcGeUtil.h"
#include "AcGeNurbsCore.h"
#include "AcGeBspline.h"
#include "AcGeNurbsSpline.h"
#include "AcGeCurves2d.h"
#include "AcGeIntersect.h"
#include "AcGeProject.h"
#include "AcGeOctree.h"
#include "AcGeQuadtree.h"
#include "AcGe2d.h"
#include "AcGePolyline.h"
#include "AcGeOffset.h"
#include "AcGeTriangle.h"
#include "AcGeDelaunay2d.h"
#include "AcGeClipBoundary2d.h"
#include "AcGeCompositeCurve.h"
#include "AcGeOffsetCurve.h"
#include "AcGeSweptSurface.h"
