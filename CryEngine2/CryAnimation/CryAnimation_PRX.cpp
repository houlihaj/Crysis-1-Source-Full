#if defined PS3_PRX
#define PS3_INIT_MODULE 1
#include <InitPRX.h>
#include <ISystem.h>

SYS_MODULE_INFO(CryAnimation, 0, 1, 1);
SYS_MODULE_START(CryAnimation_Start);
SYS_MODULE_STOP(CryAnimation_Stop);

extern "C" int CryAnimation_Start(unsigned, void *argp)
{ return StartPRX("CryAnimation", argp); }

extern "C" int CryAnimation_Stop(unsigned, void *)
{ return StopPRX("CryAnimation"); }

SYS_LIB_DECLARE(libCryAnimation, SYS_LIB_AUTO_EXPORT|SYS_LIB_WEAK_IMPORT);
SYS_LIB_EXPORT(CreateCharManager, libCryAnimation);

#endif // PS3_PRX

// vim:ts=2:sw=2

