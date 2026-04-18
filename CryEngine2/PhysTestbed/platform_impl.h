#ifndef __platform_impl_h__
#define __platform_impl_h__
#pragma once

#include "platform.h"

// Link to CrySystem
#if !defined(_LIB) && !defined(CRYSYSTEM_EXPORTS)
	#ifdef WIN64
	//#pragma comment(lib,"../../../Bin64/CrySystem.lib")
	#else //WIN64
	//#pragma comment(lib,"../../../Bin32/CrySystem.lib")
	#endif //WIN64
#endif

// If we use cry memory manager this should be also included in every module.
#if defined(USING_CRY_MEMORY_MANAGER) && !defined(_SPU)
	#include <CryMemoryManager_impl.h>
#endif

#endif // __platform_impl_h__
