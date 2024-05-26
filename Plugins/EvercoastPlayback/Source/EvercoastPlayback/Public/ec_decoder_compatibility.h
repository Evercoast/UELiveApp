#pragma once
#pragma warning(push)
// For some reason UE 4.27 doesn't seem to force C++17 on generated plugin code
#ifdef PLATFORM_IOS
#if PLATFORM_IOS
#pragma GCC diagnostic ignored "-Wc++17-extensions"
#endif
#endif

namespace GhostTree
{
#include "ec/decoding/API_C_decoder.h"
}
#pragma warning(pop)

#ifdef EC_DEFINE_HANDLE
typedef GhostTree::Handle GTHandle;
//#ifndef InvalidHandle
//#define InvalidHandle (DecodingAPIInvalidHandle)
//#endif //InvalidHandle 


#endif //EC_DEFINE_HANDLE

using namespace GhostTree;
