#pragma once

// ObjectARX-style export port for the AcGe layer.
//
// Static-library build (default): ACGE_PORT expands to nothing and the
// headers behave exactly like before.  To ship acge as a DLL, define
// ACGE_DLL for the whole build; the acge target also defines
// ACGE_BUILDING so its own translation units export while every
// consumer imports:
//
//   target_compile_definitions(acge PRIVATE ACGE_BUILDING)
//   # all targets: ACGE_DLL
//
// The C4251 policy for classes with STL members is a deferred decision
// (see the split-plan notes); flipping the switch does not change
// source semantics today.
#ifdef ACGE_DLL
#  ifdef ACGE_BUILDING
#    define ACGE_PORT __declspec(dllexport)
#  else
#    define ACGE_PORT __declspec(dllimport)
#  endif
#else
#  define ACGE_PORT
#endif
