#if defined PS3_PRX
#define PS3_INIT_MODULE 1
#include <InitPRX.h>
#include <ISystem.h>

SYS_MODULE_INFO(CryAction, 0, 1, 1);
SYS_MODULE_START(CryAction_Start);
SYS_MODULE_STOP(CryAction_Stop);

extern "C" int CryAction_Start(unsigned, void *argp)
{ return StartPRX("CryAction", argp); }

extern "C" int CryAction_Stop(unsigned, void *)
{ return StopPRX("CryAction"); }

SYS_LIB_DECLARE(libCryAction, SYS_LIB_AUTO_EXPORT|SYS_LIB_WEAK_IMPORT);
SYS_LIB_EXPORT(CreateGameFramework, libCryAction);

#endif // PS3_PRX

// vim:ts=2:sw=2

