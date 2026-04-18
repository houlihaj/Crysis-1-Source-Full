#if defined PS3_PRX
#define PS3_INIT_MODULE 1
#include <InitPRX.h>
#include <ISystem.h>

SYS_MODULE_INFO(XRenderD3D10, 0, 1, 1);
SYS_MODULE_START(XRenderD3D10_Start);
SYS_MODULE_STOP(XRenderD3D10_Stop);

extern "C" int XRenderD3D10_Start(unsigned, void *argp)
{ return StartPRX("XRenderD3D10", argp); }

extern "C" int XRenderD3D10_Stop(unsigned, void *)
{ return StopPRX("XRenderD3D10"); }

SYS_LIB_DECLARE(libXRenderD3D10, SYS_LIB_AUTO_EXPORT|SYS_LIB_WEAK_IMPORT);
SYS_LIB_EXPORT(PackageRenderConstructor, libXRenderD3D10);

SYS_LIB_DECLARE(libRender, SYS_LIB_AUTO_EXPORT|SYS_LIB_WEAK_IMPORT);
SYS_LIB_EXPORT(PackageRenderConstructor, libRender);

#endif // PS3_PRX

// vim:ts=2:sw=2

