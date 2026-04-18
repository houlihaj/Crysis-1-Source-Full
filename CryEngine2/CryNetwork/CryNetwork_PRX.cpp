#if defined PS3_PRX
#define PS3_INIT_MODULE 1
#include <InitPRX.h>

SYS_MODULE_INFO(CryNetwork, 0, 1, 1);
SYS_MODULE_START(CryNetwork_Start);
SYS_MODULE_STOP(CryNetwork_Stop);

#if 0
PS3SystemEnvironment *gPS3Env = NULL;

extern "C" { void __init(); void __fini(); }
struct _CryMemoryManagerPoolHelper { static void Init(); };

extern "C" int CryNetwork_Start(unsigned, void *argp)
{
	assert(argp != NULL);
	gPS3Env = reinterpret_cast<PS3SystemEnvironment *>(argp);
fprintf(stderr, "CryNetwork_Start: initializing MM...\n");
PS3_BREAK;
  _CryMemoryManagerPoolHelper::Init();
fprintf(stderr, "CryNetwork_Start: calling __init()...\n");
PS3_BREAK;
  __init();
fprintf(stderr, "CryNetwork_Start: done.\n");
  return SYS_PRX_RESIDENT;
}

extern "C" int CryNetwork_Stop(unsigned, void *)
{
  __fini();
  return SYS_PRX_STOP_OK;
}
#endif

extern "C" int CryNetwork_Start(unsigned, void *argp)
{ return StartPRX("CryNetwork", argp); }

extern "C" int CryNetwork_Stop(unsigned, void *)
{ return StopPRX("CryNetwork"); }

SYS_LIB_DECLARE(libCryNetwork, SYS_LIB_AUTO_EXPORT|SYS_LIB_WEAK_IMPORT);
SYS_LIB_EXPORT(CreateNetwork, libCryNetwork);

#endif // PS3_PRX

// vim:ts=2:sw=2

