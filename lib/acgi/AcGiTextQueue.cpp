#include "acgi/AcGiTextQueue.h"

namespace acgi
{

// Definitions live here (not inline in the header) so the function-local
// singletons stay unique when acgi ships as a DLL: inline-function statics
// can be instantiated in both the DLL and its consumer.
AcArray<TextRequest> &textRequests()
{
    static AcArray<TextRequest> requests;
    return requests;
}

bool &textFrameFallback()
{
    static bool fallback = true;
    return fallback;
}

} // namespace acgi
