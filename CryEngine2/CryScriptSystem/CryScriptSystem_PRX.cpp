#if defined PS3_PRX
#define PS3_INIT_MODULE 1
#include <InitPRX.h>
#include <ISystem.h>

SYS_MODULE_INFO(CryScriptSystem, 0, 1, 1);
SYS_MODULE_START(CryScriptSystem_Start);
SYS_MODULE_STOP(CryScriptSystem_Stop);

extern "C" int CryScriptSystem_Start(unsigned, void *argp)
{ return StartPRX("ScriptSystem", argp); }

extern "C" int CryScriptSystem_Stop(unsigned, void *)
{ return StopPRX("ScriptSystem"); }

SYS_LIB_DECLARE(libCryScriptSystem, SYS_LIB_AUTO_EXPORT|SYS_LIB_WEAK_IMPORT);
SYS_LIB_EXPORT(CreateScriptSystem, libCryScriptSystem);

#endif // PS3_PRX

// vim:ts=2:sw=2

