#if defined PS3_PRX
#define PS3_INIT_MODULE 1
#include <InitPRX.h>
#include <ISystem.h>

SYS_MODULE_INFO(CryGameDll, 0, 1, 1);
SYS_MODULE_START(CryGameDll_Start);
SYS_MODULE_STOP(CryGameDll_Stop);

extern "C" int CryGameDll_Start(unsigned, void *argp)
{ return StartPRX("GameDll", argp); }

extern "C" int CryGameDll_Stop(unsigned, void *)
{ return StopPRX("GameDll"); }

SYS_LIB_DECLARE(libGameDll, SYS_LIB_AUTO_EXPORT|SYS_LIB_WEAK_IMPORT);
SYS_LIB_EXPORT(CreateGameStartup, libGameDll);

#endif // PS3_PRX

// vim:ts=2:sw=2

