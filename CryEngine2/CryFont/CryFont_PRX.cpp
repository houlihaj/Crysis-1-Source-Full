#if defined PS3_PRX
#define PS3_INIT_MODULE 1
#include <InitPRX.h>
#include <ISystem.h>

SYS_MODULE_INFO(CryFont, 0, 1, 1);
SYS_MODULE_START(CryFont_Start);
SYS_MODULE_STOP(CryFont_Stop);

extern "C" int CryFont_Start(unsigned, void *argp)
{ return StartPRX("CryFont", argp); }

extern "C" int CryFont_Stop(unsigned, void *)
{ return StopPRX("CryFont"); }

SYS_LIB_DECLARE(libCryFont, SYS_LIB_AUTO_EXPORT|SYS_LIB_WEAK_IMPORT);
SYS_LIB_EXPORT(CreateCryFontInterface, libCryFont);

#endif // PS3_PRX

// vim:ts=2:sw=2

