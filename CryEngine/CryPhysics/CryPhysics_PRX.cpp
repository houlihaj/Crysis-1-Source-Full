#if defined PS3_PRX
#define PS3_INIT_MODULE 1
#include <InitPRX.h>
#include <ISystem.h>

SYS_MODULE_INFO(CryPhysics, 0, 1, 1);
SYS_MODULE_START(CryPhysics_Start);
SYS_MODULE_STOP(CryPhysics_Stop);

extern "C" int CryPhysics_Start(unsigned, void *argp)
{ return StartPRX("CryPhysics", argp); }

extern "C" int CryPhysics_Stop(unsigned, void *)
{ return StopPRX("CryPhysics"); }

SYS_LIB_DECLARE(libCryPhysics, SYS_LIB_AUTO_EXPORT|SYS_LIB_WEAK_IMPORT);
SYS_LIB_EXPORT(CreatePhysicalWorld, libCryPhysics);

#endif // PS3_PRX

// vim:ts=2:sw=2

