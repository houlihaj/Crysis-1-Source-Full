// CryInput.cpp : Defines the entry point for the DLL application.
//

#include "StdAfx.h"

// Included only once per DLL module.
#include <platform_impl.h>

#include "BaseInput.h"

#if defined(WIN32) || defined(XENON)
#ifndef _LIB
BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved )
{
	return TRUE;
}
#endif
#endif // WIN32

IInput *CreateInput( ISystem *pSystem, void* hwnd)
{
	ModuleInitISystem(pSystem);
	IInput *pInput = 0;
	if (!pSystem->IsDedicated())
	{
#if defined(USE_DXINPUT)
		pInput = new CDXInput(pSystem, (HWND) hwnd);
#elif defined(USE_PS3INPUT)
		pInput = new CPS3Input(pSystem);
#elif defined(USE_XENONINPUT)
		pInput = new CXenonInput(pSystem);
#elif defined(USE_LINUXINPUT)
		pInput = new CLinuxInput(pSystem);
#else
		pInput = new CBaseInput();
#endif
	}
	else
		pInput = new CBaseInput();

	if (!pInput->Init())
	{
		delete pInput;
		return 0;
	}
	return pInput;
}

#include <CrtDebugStats.h>
