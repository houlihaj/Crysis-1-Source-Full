/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2007.
-------------------------------------------------------------------------
Description:
Generic PRX initialization code shared by all PRX modules.
This is a PS3 specific file.
-------------------------------------------------------------------------
History:
- Jul 10, 2007:	Created by Sascha Demetrio

*************************************************************************/

#if !defined _INITPRX_H_
#define _INITPRX_H_

#include <ISystem.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/prx.h>
#include <sys/return_code.h>
#include <sys/paths.h>

struct PS3SystemEnvironment;
extern PS3SystemEnvironment *gPS3Env;

inline bool LoadPRX(const char *moduleName, const char *sprxBaseName)
{
	static const char prxDir[] = SYS_APP_HOME;
	static const char outputSuffix[] = PS3_OUTPUT_SUFFIX;
	char fileName[1024];

	snprintf(fileName, sizeof fileName, "%s/%s%s.sprx",
			prxDir, sprxBaseName, outputSuffix);
	fileName[sizeof fileName - 1] = 0;
	sys_prx_id_t prxId = sys_prx_load_module(fileName, 0, NULL);
	if (prxId <= CELL_OK)
	{
		fprintf(stderr,
				"error loading PRX module '%s' %s\n",
				moduleName, fileName);
		return false;
	}
	int prxResult = 0;
	if (sys_prx_start_module(
				prxId,
				1 /* argument size, ignored */,
				static_cast<void *>(gPS3Env),
				&prxResult,
				0,
				NULL) != CELL_OK)
	{
		fprintf(stderr,
				"error starting PRX module '%s' %s\n",
				moduleName, fileName);
		return false;
	}
	if (prxResult < 0)
	{
		fprintf(stderr,
				"initialization failed for PRX module '%s' %s\n",
				moduleName, fileName);
		return false;
	}
	return true;
}

// Define PS3_INIT_MODULE if this file is included from a *_PRX.cpp module
// initialization file.
#if defined PS3_INIT_MODULE
extern "C" { void __init(); void __fini(); }
struct _CryMemoryManagerPoolHelper { static void Init(); };
PS3SystemEnvironment *gPS3Env = NULL;

inline int StartPRX(
		const char *moduleName __attribute__ ((unused)),
		void *arg
		)
{
	assert(arg != NULL);
	assert(gPS3Env == NULL);
	gPS3Env = reinterpret_cast<PS3SystemEnvironment *>(arg);
  _CryMemoryManagerPoolHelper::Init();
  __init();
	return SYS_PRX_RESIDENT;
}

inline int StopPRX(const char *moduleName __attribute__ ((unused)))
{
	__fini();
	return SYS_PRX_STOP_OK;
}
#endif // PS3_INIT_MODULE

#endif

// vim:ts=2:sw=2

