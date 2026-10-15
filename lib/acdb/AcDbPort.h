#pragma once

// ObjectARX-style export port for the AcDb layer.
//
// Static-library build (default): ACDB_PORT expands to nothing.  To ship
// acdb as a DLL, define ACDB_DLL for the whole build; the acdb target
// also defines ACDB_BUILDING so its own translation units export while
// every consumer imports (same layout as AcGePort.h / AcGiPort.h /
// AcGsPort.h).  The C4251 policy for payload structs with std:: members
// is a deferred decision (see the acge split-plan notes).
#ifdef ACDB_DLL
#  ifdef ACDB_BUILDING
#    define ACDB_PORT __declspec(dllexport)
#  else
#    define ACDB_PORT __declspec(dllimport)
#  endif
#else
#  define ACDB_PORT
#endif
