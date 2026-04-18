#if defined PS3_PRX
#define PS3_INIT_MODULE 1
#include <InitPRX.h>
#include <ISystem.h>

SYS_MODULE_INFO(Cry3DEngine, 0, 1, 1);
SYS_MODULE_START(Cry3DEngine_Start);
SYS_MODULE_STOP(Cry3DEngine_Stop);

extern "C" int Cry3DEngine_Start(unsigned, void *argp)
{ return StartPRX("3DEngine", argp); }

extern "C" int Cry3DEngine_Stop(unsigned, void *)
{ return StopPRX("3DEngine"); }

SYS_LIB_DECLARE(libCry3DEngine, SYS_LIB_AUTO_EXPORT|SYS_LIB_WEAK_IMPORT);
SYS_LIB_EXPORT(CreateCry3DEngine, libCry3DEngine);

#endif // PS3_PRX

// vim:ts=2:sw=2

