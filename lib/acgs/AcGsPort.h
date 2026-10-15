#pragma once

// ObjectARX-style export port for the AcGs layer.
//
// Static-library build (default): ACGS_PORT expands to nothing.  To ship
// acgs as a DLL, define ACGS_DLL for the whole build; the acgs target
// also defines ACGS_BUILDING so its own translation units export while
// every consumer imports (same layout as AcGePort.h / AcGiPort.h).
#ifdef ACGS_DLL
#  ifdef ACGS_BUILDING
#    define ACGS_PORT __declspec(dllexport)
#  else
#    define ACGS_PORT __declspec(dllimport)
#  endif
#else
#  define ACGS_PORT
#endif
