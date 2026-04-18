#if defined PS3_PRX
#define PS3_INIT_MODULE 1
#include <InitPRX.h>
#include <ISystem.h>

SYS_MODULE_INFO(CryInput, 0, 1, 1);
SYS_MODULE_START(CryInput_Start);
SYS_MODULE_STOP(CryInput_Stop);

extern "C" int CryInput_Start(unsigned, void *argp)
{ return StartPRX("Input", argp); }

extern "C" int CryInput_Stop(unsigned, void *)
{ return StopPRX("Input"); }

SYS_LIB_DECLARE(libCryInput, SYS_LIB_AUTO_EXPORT|SYS_LIB_WEAK_IMPORT);
SYS_LIB_EXPORT(CreateInput, libCryInput);

#endif // PS3_PRX

// vim:ts=2:sw=2

