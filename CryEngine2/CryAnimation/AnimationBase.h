////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Crytek Character Animation source code
//	
//	History:
//	20/10/2004 - Created by Ivo Herzeg <ivo@crytek.de>
//
//  Contains:
//  provides access-interfaces to all DLLs in the system 
/////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _CRY_ANIMATION_BASE_HEADER_
#define _CRY_ANIMATION_BASE_HEADER_

#define SKINNING_SW (1)
//#define EXTREME_TEST (1)

#include <I3DEngine.h>
#include <ICryAnimation.h>

#include "FrameProfiler.h"
#include "cvars.h"
#include "AnimationManager.h"

ILINE void g_LogToFile (const char* szFormat, ...) PRINTF_PARAMS(1, 2);


class CharacterManager;

struct ISystem;
struct IConsole;
struct ITimer;
struct ILog;
struct ICryPak;
struct IStreamEngine;

struct IRenderer;
struct IPhysicalWorld;
struct I3DEngine;
class CCamera;
class Crc32Gen;

//////////////////////////////////////////////////////////////////////////
// There's only one ISystem in the process, just like there is one CharacterManager.
// So this ISystem is kept in the global pointer and is initialized upon creation
// of the CharacterManager and is valid until its destruction. 
// Upon destruction, it's NULLed. In any case, there must be no object needing it 
// after that since the animation system is only active when the Manager object is alive
//////////////////////////////////////////////////////////////////////////

//global interfaces
extern ISystem*						g_pISystem;
extern IConsole*					g_pIConsole;
extern ITimer*						g_pITimer;
extern ILog*							g_pILog;
extern ICryPak*						g_pIPak;
extern IStreamEngine*			g_pIStreamEngine;

extern IRenderer*					g_pIRenderer;
extern IRenderAuxGeom*		g_pAuxGeom;
extern IPhysicalWorld*		g_pIPhysicalWorld;
extern I3DEngine*					g_pI3DEngine;

extern Crc32Gen*					g_pCrc32Gen;

extern bool g_bProfilerOn;
extern f32	g_fCurrTime;
extern f32	g_fFrameTime;
extern f32	g_AverageFrameTime;
extern CAnimation g_DefaultAnim;
extern CharacterManager* g_pCharacterManager; 
extern QuatT g_IdentityQuatT;
#define g_AnimationManager g_pCharacterManager->GetAnimationManager()

//extern CAnimationManager g_AnimationManager;



// initializes the global values - just remembers the pointer to the system that will
// be kept valid until deinitialization of the class (that happens upon destruction of the
// CharacterManager instance). Also initializes the console variables
__forceinline void g_InitInterfaces() 
{
	assert (g_pISystem);
	g_pIConsole				= g_pISystem->GetIConsole();
	g_pITimer					= g_pISystem->GetITimer();
	g_pILog						= g_pISystem->GetILog();
	g_pIPak						=	g_pISystem->GetIPak();
	g_pIStreamEngine	=	g_pISystem->GetStreamEngine();
	g_CpuFlags				= g_pISystem->GetCPUFlags();

	//we initialize this pointers in CharacterManager::Update() 
	g_pIRenderer			= NULL;	//pISystem->GetIRenderer();
	g_pIPhysicalWorld	= NULL;	//pISystem->GetIPhysicalWorld();
	g_pI3DEngine			=	NULL;	//pISystem->GetI3DEngine();

	g_pCrc32Gen				= g_pISystem->GetCrc32Gen();
	//---------------------------------------------------
	
	Console::GetInst().Init();

}


// deinitializes the Class - actually just NULLs the system pointer and deletes the variables
__forceinline void g_DeleteInterfaces()
{
	g_pISystem				= NULL;
	g_pITimer					= NULL;
	g_pILog						= NULL;
	g_pIConsole				= NULL;
	g_pIPak						= NULL;
	g_pIStreamEngine	= NULL;;

	g_pIRenderer			= NULL;
	g_pIPhysicalWorld	= NULL;
	g_pI3DEngine			=	NULL;
}



// Common single structure for time\memory statistics
struct AnimStatisticsInfo 
{
	int64 m_iDBASizes;
	int64 m_iCAFSizes;
	int64 m_iInstancesSizes;
	int64 m_iModelsSizes;

	AnimStatisticsInfo() : m_iDBASizes(0), m_iCAFSizes(0), m_iInstancesSizes(0), m_iModelsSizes(0) {}

	void Clear() 	{ m_iDBASizes = 0; m_iCAFSizes = 0; m_iInstancesSizes = 0; m_iModelsSizes = 0;	}
	// int - for convenient output to log
	int GetInstancesSize() { return (int)(m_iInstancesSizes - GetModelsSize() - m_iDBASizes - m_iCAFSizes); }
	int GetModelsSize() { return (int)(m_iModelsSizes - m_iDBASizes - m_iCAFSizes); }
};

extern  AnimStatisticsInfo g_AnimStatisticsInfo;


// collector profilers: collect the total time spent on something
extern double g_dTimeAnimLoadBind;
extern double g_dTimeAnimLoadBindNoCal;
extern double g_dTimeAnimLoadBindPreallocate;
extern double g_dTimeGeomLoad;
extern double g_dTimeGeomPostInit;
extern double g_dTimeShaderLoad;
extern double g_dTimeGeomChunkLoad;
extern double g_dTimeGeomChunkLoadFileIO;
extern double g_dTimeGenRenderArrays;
extern double g_dTimeAnimLoadFile;
extern f32 g_NAN;


#define ENABLE_GET_MEMORY_USAGE 1


// this is current frame id PLUS OR MINUS a few frames.
// can be used in places where it's really not significant for functionality but speed is a must.
extern int g_nFrameID;

// this is true when the game runs in such a mode that requires all bones be updated every frame
extern bool g_bUpdateBonesAlways;

#ifndef AUTO_PROFILE_SECTION
#pragma message ("Warning: ITimer not included")
#else
#undef AUTO_PROFILE_SECTION
#endif

#define AUTO_PROFILE_SECTION(g_fTimer) CITimerAutoProfiler<double> __section_auto_profiler(g_pITimer, g_fTimer)

#define DEFINE_PROFILER_FUNCTION() FUNCTION_PROFILER_FAST(g_pISystem, PROFILE_ANIMATION, g_bProfilerOn)
#define DEFINE_PROFILER_SECTION(NAME) FRAME_PROFILER_FAST(NAME, g_pISystem, PROFILE_ANIMATION, g_bProfilerOn)

#endif 
