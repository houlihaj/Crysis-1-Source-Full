#if defined PS3_PRX
#define PS3_INIT_MODULE 1
#include <InitPRX.h>
#include <ISystem.h>

SYS_MODULE_INFO(CrySoundSystem, 0, 1, 1);
SYS_MODULE_START(CrySoundSystem_Start);
SYS_MODULE_STOP(CrySoundSystem_Stop);

extern "C" int CrySoundSystem_Start(unsigned, void *argp)
{ return StartPRX("CrySoundSystem", argp); }

extern "C" int CrySoundSystem_Stop(unsigned, void *)
{ return StopPRX("CrySoundSystem"); }

SYS_LIB_DECLARE(libCrySoundSystem, SYS_LIB_AUTO_EXPORT|SYS_LIB_WEAK_IMPORT);
SYS_LIB_EXPORT(CreateSoundSystem, libCrySoundSystem);

#endif // PS3_PRX

// vim:ts=2:sw=2

