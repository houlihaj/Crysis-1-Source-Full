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

#include "stdafx.h"
#include <ICryAnimation.h>
#include <IFacialAnimation.h>
#include "CharacterManager.h"

// Must be included once in the module.
#include <platform_impl.h>

//////////////////////////////////////////////////////////////////////////
struct CSystemEventListner_Animation : public ISystemEventListener
{
public:
	virtual void OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam )
	{
		switch (event)
		{
		case ESYSTEM_EVENT_RANDOM_SEED:
			g_random_generator.seed(wparam);
			break;
		case ESYSTEM_EVENT_LEVEL_RELOAD:
			{
				STLALLOCATOR_CLEANUP;
				// clean animations cache and statistics
				g_AnimationManager.ClearChache();
				g_AnimationManager.ClearStatistics();
				IFacialAnimation* pFacialAnimation = (g_pCharacterManager ? g_pCharacterManager->GetIFacialAnimation() : 0);
				if (pFacialAnimation)
					pFacialAnimation->ClearAllCaches();
				break;
			}
		case ESYSTEM_EVENT_LEVEL_LOAD_END:
			g_pCharacterManager->UnlockResources();
		}
	}
};
static CSystemEventListner_Animation g_system_event_listener_anim;

//////////////////////////////////////////////////////////////////////////
// The main symbol exported from CryAnimation: creator of the main (root) interface/object
ICharacterManager* CreateCharManager(ISystem* pSystem, const char* szInterfaceVersion)
{
	ModuleInitISystem(pSystem);
	g_pISystem = pSystem;
	g_InitInterfaces();

	pSystem->GetISystemEventDispatcher()->RegisterListener( &g_system_event_listener_anim );

#if ENABLE_FRAME_PROFILER	
	if (!cpu::hasRDTSC())
	{
		sdsdsds
		pSystem->GetILog()->LogToFile ("CryAnimation: (Critical) this is development version of animation system, with the profiling enabled. Built-in profiler uses RDTSC function for precise time profiling. This machine doesn't seem to have RDTSC instruction implemented. Please recompile with ENABLE_FRAME_PROFILER 0. Don't use ca_Profile 1");
	}
#endif

	g_pCharacterManager=new CharacterManager;
	g_pCharacterManager->m_IsDedicatedServer=g_pISystem->IsDedicated();
	return static_cast<ICharacterManager*>(g_pCharacterManager);
}




// cached interfaces - valid during the whole session, when the character manager is alive; then get erased

ISystem*						g_pISystem				= NULL;
ITimer*							g_pITimer					= NULL;
ILog*								g_pILog						= NULL;
IConsole*						g_pIConsole				= NULL;
ICryPak*						g_pIPak						= NULL;
IStreamEngine*			g_pIStreamEngine	= NULL;;

IRenderer*					g_pIRenderer			= NULL;
IRenderAuxGeom*			g_pAuxGeom				= NULL;
IPhysicalWorld*			g_pIPhysicalWorld	= NULL;
I3DEngine*					g_pI3DEngine			= NULL;

Crc32Gen*						g_pCrc32Gen				= NULL;

f32 g_AverageFrameTime=0;
CAnimation g_DefaultAnim;
CharacterManager* g_pCharacterManager; 
QuatT g_IdentityQuatT = QuatT(IDENTITY);


ILINE void g_LogToFile (const char* szFormat, ...)
{
	char szBuffer[0x800];
	va_list args;
	va_start(args,szFormat);
	_vsnprintf (szBuffer, sizeof(szBuffer), szFormat, args);
	va_end(args);
	g_pILog->LogToFile ("%s", szBuffer);
}









f32 g_fCurrTime=0;
f32 g_fFrameTime=0;
//#ifdef _XBOX
  //int CRYANIMATION_API g_CpuFlags;
//#else
int g_CpuFlags;
//#endif

// this is current frame id PLUS OR MINUS a few frames.
// can be used in places where it's really not significant for functionality but speed is a must.
int g_nFrameID = 0;

// this is true when the game runs in such a mode that requires all bones be updated every frame
bool g_bUpdateBonesAlways = false;

bool g_bProfilerOn = false;



double g_dTimeAnimLoadBind;
double g_dTimeAnimLoadBindPreallocate;
double g_dTimeAnimLoadBindNoCal;
double g_dTimeGeomLoad;
double g_dTimeGeomPostInit;
double g_dTimeShaderLoad;
double g_dTimeGeomChunkLoad;
double g_dTimeGeomChunkLoadFileIO;
double g_dTimeGenRenderArrays;
double g_dTimeAnimLoadFile;


AnimStatisticsInfo g_AnimStatisticsInfo;

#include "TypeInfo_impl.h"
#include "CryHeaders_info.h"
#include "CGFContent_info.h"
#include "Cry_Vector3_info.h"
#include "Cry_Quat_info.h"
#include "Cry_Matrix_info.h"
#include "Cry_Color_info.h"

