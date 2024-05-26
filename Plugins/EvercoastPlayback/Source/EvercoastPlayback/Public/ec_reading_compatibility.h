#pragma once

namespace GhostTree
{
#include "ec/reading/API_C_reader.h"
}

#ifdef EC_DEFINE_HANDLE
typedef GhostTree::Handle GTHandle;
//#ifndef InvalidHandle 
//#define InvalidHandle (ReadingAPIInvalidHandle)
//#endif //InvalidHandle 

#endif //EC_DEFINE_HANDLE

using namespace GhostTree;
