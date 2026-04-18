#if defined PS3_PRX
#define PS3_INIT_MODULE 1
#include <InitPRX.h>
#include <ISystem.h>

SYS_MODULE_INFO(CryAISystem, 0, 1, 1);
SYS_MODULE_START(CryAISystem_Start);
SYS_MODULE_STOP(CryAISystem_Stop);

extern "C" int CryAISystem_Start(unsigned, void *argp)
{ return StartPRX("AISystem", argp); }

extern "C" int CryAISystem_Stop(unsigned, void *)
{ return StopPRX("AISystem"); }

SYS_LIB_DECLARE(libCryAISystem, SYS_LIB_AUTO_EXPORT|SYS_LIB_WEAK_IMPORT);
SYS_LIB_EXPORT(CreateAISystem, libCryAISystem);

#endif // PS3_PRX

// vim:ts=2:sw=2

