#if defined PS3_PRX
#define PS3_INIT_MODULE 1
#include <InitPRX.h>
#include <ISystem.h>

SYS_MODULE_INFO(CryEntitySystem, 0, 1, 1);
SYS_MODULE_START(CryEntitySystem_Start);
SYS_MODULE_STOP(CryEntitySystem_Stop);

extern "C" int CryEntitySystem_Start(unsigned, void *argp)
{ return StartPRX("CryEntitySystem", argp); }

extern "C" int CryEntitySystem_Stop(unsigned, void *)
{ return StopPRX("CryEntitySystem"); }

SYS_LIB_DECLARE(libCryEntitySystem, SYS_LIB_AUTO_EXPORT|SYS_LIB_WEAK_IMPORT);
SYS_LIB_EXPORT(CreateEntitySystem, libCryEntitySystem);

#endif // PS3_PRX

// vim:ts=2:sw=2

