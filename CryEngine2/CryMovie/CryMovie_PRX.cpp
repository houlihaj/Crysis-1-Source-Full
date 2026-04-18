#if defined PS3_PRX
#define PS3_INIT_MODULE 1
#include <InitPRX.h>
#include <ISystem.h>

SYS_MODULE_INFO(CryMovie, 0, 1, 1);
SYS_MODULE_START(CryMovie_Start);
SYS_MODULE_STOP(CryMovie_Stop);

extern "C" int CryMovie_Start(unsigned, void *argp)
{ return StartPRX("MovieSystem", argp); }

extern "C" int CryMovie_Stop(unsigned, void *)
{ return StopPRX("MovieSystem"); }

SYS_LIB_DECLARE(libCryMovie, SYS_LIB_AUTO_EXPORT|SYS_LIB_WEAK_IMPORT);
SYS_LIB_EXPORT(CreateMovieSystem, libCryMovie);

#endif // PS3_PRX

// vim:ts=2:sw=2

