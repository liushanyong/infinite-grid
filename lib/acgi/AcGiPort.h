#pragma once

// ObjectARX-style export port for the AcGi layer.
//
// Static-library build (default): ACGI_PORT expands to nothing.  To ship
// acgi as a DLL, define ACGI_DLL for the whole build; the acgi target
// also defines ACGI_BUILDING so its own translation units export while
// every consumer imports (same layout as AcGePort.h).
#ifdef ACGI_DLL
#  ifdef ACGI_BUILDING
#    define ACGI_PORT __declspec(dllexport)
#  else
#    define ACGI_PORT __declspec(dllimport)
#  endif
#else
#  define ACGI_PORT
#endif
