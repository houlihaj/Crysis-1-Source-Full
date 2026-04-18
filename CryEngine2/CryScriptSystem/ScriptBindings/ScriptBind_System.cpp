////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   ScriptBind_System.cpp
//  Version:     v1.00
//  Created:     8/7/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ScriptBind_System.h"
#include <ICryAnimation.h>
#include <IFont.h>
#include <ILog.h>
#include <IRenderer.h>
#include <I3DEngine.h>
#include <IInput.h>
#include <IEntitySystem.h>
#include <ITimer.h>
#include <IConsole.h>
#include <IAISystem.h>
#include <IAgent.h>
#include <ISound.h>
#include <ICryPak.h>
#include <IGame.h>
#include <ITestSystem.h>
#include <IGameFramework.h>
#include <Cry_Camera.h>
#include <Cry_Geo.h>
#include <IRenderAuxGeom.h>
#include <IBudgetingSystem.h>
#include <time.h>


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

// modes of ScanDirectory function
#define SCANDIR_ALL 0
#define SCANDIR_FILES 1
#define SCANDIR_SUBDIRS 2

/*
 	nMode
 		R_BLEND_MODE__ZERO__SRC_COLOR					        1
		R_BLEND_MODE__SRC_COLOR__ZERO					        2
		R_BLEND_MODE__SRC_COLOR__ONE_MINUS_SRC_COLOR	3
		R_BLEND_MODE__SRC_ALPHA__ONE_MINUS_SRC_ALPHA	4
		R_BLEND_MODE__ONE__ONE                        5
		R_BLEND_MODE__DST_COLOR__SRC_COLOR            6
		R_BLEND_MODE__ZERO__ONE_MINUS_SRC_COLOR       7
		R_BLEND_MODE__ONE__ONE_MINUS_SRC_COLOR	      8
		R_BLEND_MODE__ONE__ZERO                       9
		R_BLEND_MODE__ZERO__ZERO                     10
		R_BLEND_MODE__ONE__ONE_MINUS_SRC_ALPHA       11
		R_BLEND_MODE__SRC_ALPHA__ONE                 12
		R_BLEND_MODE__ADD_SIGNED                     14
*/

static unsigned int sGetBlendState(int nMode)
{
  unsigned int nBlend;
  switch(nMode)
  {
    case 1:
      nBlend = GS_BLSRC_ZERO | GS_BLDST_SRCCOL;
      break;
    case 2:
    case 3:
      assert(0);
      break;
    case 4:
      nBlend = GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
      break;
    case 5:
      nBlend = GS_BLSRC_ONE | GS_BLDST_ONE;
      break;
    case 6:
      nBlend = GS_BLSRC_DSTCOL | GS_BLDST_SRCCOL;
      break;
    case 7:
      nBlend = GS_BLSRC_ZERO | GS_BLDST_ONEMINUSSRCCOL;
      break;
    case 8:
      nBlend = GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCCOL;
      break;
    case 9:
      nBlend = GS_BLSRC_ONE | GS_BLDST_ZERO;
      break;
    case 10:
      nBlend = GS_BLSRC_ZERO | GS_BLDST_ZERO;
      break;
    case 11:
      nBlend = GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA;
      break;
    case 12:
      nBlend = GS_BLSRC_SRCALPHA | GS_BLDST_ONE;
      break;
    case 14:
      nBlend = GS_BLSRC_DSTCOL | GS_BLDST_SRCCOL;
      break;
    default:
      assert(0);
  }
  return nBlend;
}

CScriptBind_System::~CScriptBind_System()
{
}

/////////////////////////////////////////////////////////
/*! Initializes the script-object and makes it available for the scripts.
		@param pScriptSystem Pointer to the ScriptSystem-interface
		@param pSystem Pointer to the System-interface
		@see IScriptSystem
		@see ISystem
*/
CScriptBind_System::CScriptBind_System(IScriptSystem *pScriptSystem, ISystem *pSystem)
{
	m_SkyFadeStart = 1000;
	m_SkyFadeEnd = 200;

	m_pSystem = pSystem;
	m_pLog = m_pSystem->GetILog();
	m_pRenderer = m_pSystem->GetIRenderer();
	m_pConsole = m_pSystem->GetIConsole();
	m_pTimer = m_pSystem->GetITimer();
	m_p3DEngine = m_pSystem->GetI3DEngine();

	pScriptSystem->SetGlobalValue("SCANDIR_ALL",SCANDIR_ALL);
	pScriptSystem->SetGlobalValue("SCANDIR_FILES",SCANDIR_FILES);
	pScriptSystem->SetGlobalValue("SCANDIR_SUBDIRS",SCANDIR_SUBDIRS);

//	r_hdrrendering = m_pSystem->GetIConsole()->GetCVar("r_HDRRendering");								assert(r_hdrrendering);

	CScriptableBase::Init(pScriptSystem,pSystem);
	SetGlobalName("System");

	m_pScriptTimeTable.Create(pScriptSystem);

#undef SCRIPT_REG_CLASSNAME 
#define SCRIPT_REG_CLASSNAME &CScriptBind_System::

	SCRIPT_REG_FUNC(CreateDownload);
	SCRIPT_REG_FUNC(LoadFont);
	SCRIPT_REG_FUNC(ExecuteCommand);
	SCRIPT_REG_FUNC(LogToConsole);
	SCRIPT_REG_FUNC(LogAlways);
	SCRIPT_REG_FUNC(ClearConsole);
	SCRIPT_REG_FUNC(Log);
	SCRIPT_REG_FUNC(Warning);
	SCRIPT_REG_FUNC(Error);
	SCRIPT_REG_TEMPLFUNC(IsEditor, "");
	SCRIPT_REG_FUNC(GetCurrTime);
	SCRIPT_REG_FUNC(GetCurrAsyncTime);
	SCRIPT_REG_FUNC(GetFrameTime);
	SCRIPT_REG_FUNC(GetLocalOSTime);
	SCRIPT_REG_FUNC(DrawLabelImage);
	SCRIPT_REG_FUNC(DrawLabel);
	SCRIPT_REG_FUNC(GetEntity);//<<FIXME>> move to server
	SCRIPT_REG_FUNC(GetEntityClass);//<<FIXME>> move to server
	SCRIPT_REG_FUNC(GetEntities);//<<FIXME>> move to server
	SCRIPT_REG_TEMPLFUNC(GetEntitiesInSphere, "center, radius");//<<FIXME>> move to server
	SCRIPT_REG_TEMPLFUNC(GetEntitiesInSphereByClass, "center, radius, className");//<<FIXME>> move to server
	SCRIPT_REG_TEMPLFUNC(GetPhysicalEntitiesInBox, "center, radius");
	SCRIPT_REG_TEMPLFUNC(GetPhysicalEntitiesInBoxByClass, "center, radius, className");
	SCRIPT_REG_TEMPLFUNC(GetEntitiesByClass, "EntityClass");
	SCRIPT_REG_TEMPLFUNC(GetNearestEntityByClass, "center, radius, className"); // entity
	
	SCRIPT_REG_TEMPLFUNC(GetEntityByName,"sEntityName");
	SCRIPT_REG_TEMPLFUNC(GetEntityIdByName,"sEntityName");

	SCRIPT_REG_FUNC(DeformTerrain);
	SCRIPT_REG_FUNC(DeformTerrainUsingMat);
	SCRIPT_REG_FUNC(ScreenToTexture);
	SCRIPT_REG_FUNC(DrawLine);
	SCRIPT_REG_FUNC(Draw2DLine);
	SCRIPT_REG_FUNC(DrawText);
	SCRIPT_REG_FUNC(DrawImage);
	SCRIPT_REG_FUNC(DrawImageColor);
	SCRIPT_REG_FUNC(DrawImageCoords);
	SCRIPT_REG_FUNC(DrawImageColorCoords);
	SCRIPT_REG_FUNC(DrawTriStrip);
	SCRIPT_REG_FUNC(SetGammaDelta);
	SCRIPT_REG_FUNC(DrawRectShader);
	SCRIPT_REG_FUNC(SetScreenShader);

	SCRIPT_REG_FUNC(ShowConsole);
	SCRIPT_REG_FUNC(CheckHeapValid);

	SCRIPT_REG_FUNC(GetConfigSpec);
	SCRIPT_REG_FUNC(IsMultiplayer);
	
	SCRIPT_REG_FUNC(SetPostProcessFxParam);
	SCRIPT_REG_FUNC(GetPostProcessFxParam);

  SCRIPT_REG_FUNC(SetScreenFx);
  SCRIPT_REG_FUNC(GetScreenFx);

	SCRIPT_REG_FUNC(SetCVar);
	SCRIPT_REG_FUNC(GetCVar);
	SCRIPT_REG_FUNC(AddCCommand);

	SCRIPT_REG_FUNC(SetScissor);

	// CW: added for script based system analysis
	SCRIPT_REG_FUNC(GetCPUQuality);
	SCRIPT_REG_FUNC(GetGPUQuality);
	SCRIPT_REG_FUNC(GetSystemMem);
	SCRIPT_REG_FUNC(GetVideoMem);
	SCRIPT_REG_FUNC(IsPS20Supported);
	SCRIPT_REG_FUNC(IsHDRSupported);

	SCRIPT_REG_FUNC(SetBudget);
	SCRIPT_REG_FUNC(SetVolumetricFogModifiers);

	SCRIPT_REG_FUNC(SetWind);
	SCRIPT_REG_FUNC(GetWind);
	
	SCRIPT_REG_TEMPLFUNC(GetSurfaceTypeIdByName, "surfaceName");
	SCRIPT_REG_TEMPLFUNC(GetSurfaceTypeNameById, "surfaceId");

	SCRIPT_REG_TEMPLFUNC(RemoveEntity, "entityId");
	SCRIPT_REG_TEMPLFUNC(SpawnEntity, "params");
	SCRIPT_REG_FUNC(ActivateLight);
	//	SCRIPT_REG_FUNC(ActivateMainLight);
	SCRIPT_REG_FUNC(SetWaterVolumeOffset);
	SCRIPT_REG_FUNC(IsValidMapPos);
	SCRIPT_REG_FUNC(EnableMainView);
	SCRIPT_REG_FUNC(EnableOceanRendering);
	SCRIPT_REG_FUNC(ScanDirectory);
	SCRIPT_REG_FUNC(DebugStats);
	SCRIPT_REG_FUNC(ViewDistanceSet);
	SCRIPT_REG_FUNC(ViewDistanceGet);
	SCRIPT_REG_FUNC(ApplyForceToEnvironment);
	SCRIPT_REG_FUNC(GetOutdoorAmbientColor);
	SCRIPT_REG_FUNC(SetOutdoorAmbientColor);
	SCRIPT_REG_FUNC(GetTerrainElevation);
	//	SCRIPT_REG_FUNC(SetIndoorColor);
	SCRIPT_REG_FUNC(ActivatePortal);
	SCRIPT_REG_FUNC(DumpMMStats);
	SCRIPT_REG_FUNC(EnumDisplayFormats);
	SCRIPT_REG_FUNC(EnumAAFormats);
	SCRIPT_REG_FUNC(IsPointIndoors);
	SCRIPT_REG_FUNC(SetConsoleImage);
	SCRIPT_REG_TEMPLFUNC(ProjectToScreen, "point");
	SCRIPT_REG_FUNC(EnableHeatVision);
	SCRIPT_REG_FUNC(ShowDebugger);
	//SCRIPT_REG_FUNC(IndoorSoundAllowed);
	SCRIPT_REG_FUNC(DumpMemStats);
	SCRIPT_REG_FUNC(DumpMemoryCoverage);
	SCRIPT_REG_FUNC(ApplicationTest);
	SCRIPT_REG_FUNC(QuitInNSeconds);
	SCRIPT_REG_FUNC(DumpWinHeaps);
	SCRIPT_REG_FUNC(Break);
	SCRIPT_REG_TEMPLFUNC(SetViewCameraFov, "fov");
	SCRIPT_REG_TEMPLFUNC(GetViewCameraFov, "");
	SCRIPT_REG_TEMPLFUNC(IsPointVisible, "point");
	SCRIPT_REG_FUNC(GetViewCameraPos);
	SCRIPT_REG_FUNC(GetViewCameraDir);
	SCRIPT_REG_FUNC(GetViewCameraAngles);
	SCRIPT_REG_FUNC(RayWorldIntersection);
	SCRIPT_REG_FUNC(BrowseURL);
	SCRIPT_REG_FUNC(IsDevModeEnable);
	SCRIPT_REG_FUNC(RayTraceCheck);
	SCRIPT_REG_FUNC(SaveConfiguration);
	SCRIPT_REG_FUNC(SetSystemShaderRenderFlags);
	SCRIPT_REG_FUNC(Quit);
	SCRIPT_REG_FUNC(ClearKeyState);

	SCRIPT_REG_TEMPLFUNC(SetSunColor,"vSunColor");
	SCRIPT_REG_TEMPLFUNC(GetSunColor,"");
	SCRIPT_REG_TEMPLFUNC(SetSkyColor,"vSkyColor");
	SCRIPT_REG_TEMPLFUNC(GetSkyColor,"");

	SCRIPT_REG_TEMPLFUNC(SetSkyHighlight,"tableSkyHighlightParams");
	SCRIPT_REG_TEMPLFUNC(GetSkyHighlight,"");

	SCRIPT_REG_TEMPLFUNC(LoadLocalizationXml,"filename");

	SCRIPT_REG_FUNC(GetHDRDynamicMultiplier);
	SCRIPT_REG_FUNC(SetHDRDynamicMultiplier);
	SCRIPT_REG_FUNC(GetFrameID);
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

int CScriptBind_System::ShowDebugger(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);
	m_pSystem->GetIScriptSystem()->ShowDebugger(NULL, 0, "Invoked From User");
	return pH->EndFunction();
}

int CScriptBind_System::DumpMemStats (IFunctionHandler *pH)
{
	bool bUseKB = false;
	if (pH->GetParamCount() > 0)
		pH->GetParam(1, bUseKB);
	m_pSystem->DumpMemoryUsageStatistics(bUseKB);
	return pH->EndFunction();
}

int CScriptBind_System::DumpMemoryCoverage(IFunctionHandler *pH)
{
	// useful to investigate memory fragmentation
	// every time you call this from the console: #System.DumpMemoryCoverage()
	// it adds a line to "MemoryCoverage.bmp" (generated the first time, there is a max line count)
	m_pSystem->DumpMemoryCoverage();
	return pH->EndFunction();
}


int CScriptBind_System::ApplicationTest(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	
	const char *pszParam;
	pH->GetParam(1, pszParam);
	
	m_pSystem->ApplicationTest(pszParam);

	return pH->EndFunction();
}


int CScriptBind_System::QuitInNSeconds(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	
	float fInNSeconds;
	pH->GetParam(1,fInNSeconds);
	
	if(m_pSystem->GetITestSystem())
		m_pSystem->GetITestSystem()->QuitInNSeconds(fInNSeconds);

	return pH->EndFunction();
}


int CScriptBind_System::DumpWinHeaps (IFunctionHandler *pH)
{
	m_pSystem->DumpWinHeaps();
	return pH->EndFunction();
}

/*! Creates a download object
		@return download object just created
*/
int CScriptBind_System::CreateDownload(IFunctionHandler *pH)
{
	//@TODO: Restore it thru ISystem interface.
	/*
	// this cast is a hack, because i don't want to change the ISystem interface at this point
	CSystem *pSystem = static_cast<CSystem *>(m_pSystem);
#if !defined(LINUX)
	if (pSystem)
	{
		CHTTPDownloader *pDL = pSystem->m_pDownloadManager->CreateDownload();

		return pH->EndFunction(pDL->GetScriptTable());
	}
#endif
	*/
	return pH->EndFunction();
}

/*! Loads a font and makes it available for future-selection
		@param name of font-xml-file (no suffix)
*/
int CScriptBind_System::LoadFont(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	const char *pszName;
	pH->GetParam(1, pszName);

	ICryFont *pICryFont=m_pSystem->GetICryFont();

	if(pICryFont)
	{
		string szFontPath = "fonts/";
		/*
		m_pSS->GetGlobalValue("g_language", szLanguage);

		if (!szLanguage || !strlen(szLanguage))
		{
			szFontPath += "english";
		}
		else
		{
			szFontPath += szLanguage;
		}
		*/

		szFontPath += pszName;
		szFontPath += ".xml";

		IFFont *pIFont = pICryFont->NewFont(pszName);

		if (!pIFont->Load(szFontPath.c_str()))
		{
			m_pLog->Log((string("Error loading digital font from ")+szFontPath).c_str());
		}
	}
	return pH->EndFunction();
}

//-------------------------------------------------------------------------------------------------
int CScriptBind_System::ExecuteCommand(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	const char *szCmd;

	if (pH->GetParam(1, szCmd))
	{
		m_pConsole->ExecuteString(szCmd);
	}

	return pH->EndFunction();
}


/*! Write a string to the console
		@param String to write
		@see CScriptBind_System::Log
*/
int CScriptBind_System::LogToConsole(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	LogString(pH,true);

	return (pH->EndFunction());
}

/*! log even with log verbosity 0 - without <LUA>
@param String to write
*/
int CScriptBind_System::LogAlways(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	const char *sParam=NULL;

	pH->GetParam(1,sParam);

	if(sParam)
		CryLogAlways("%s",sParam);

	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::Warning(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	const char *sParam = "";
	if (pH->GetParam(1,sParam))
	{
		m_pSystem->Warning( VALIDATOR_MODULE_SCRIPTSYSTEM,VALIDATOR_WARNING,0,NULL,"%s",sParam );
	}

	return (pH->EndFunction());
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::Error(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	const char *sParam = "";
	if (pH->GetParam(1,sParam))
	{
		m_pSystem->Warning( VALIDATOR_MODULE_SCRIPTSYSTEM,VALIDATOR_ERROR,0,NULL,"%s",sParam );
	}

	return (pH->EndFunction());
}

int CScriptBind_System::IsEditor(IFunctionHandler *pH)
{
	return pH->EndFunction(m_pSystem->IsEditor());
}

/*! Clear the console
*/
int CScriptBind_System::ClearConsole(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	m_pConsole->Clear();

	return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*! Write a message into the log file and the console
		@param String to write
		@see stuff
*/
int CScriptBind_System::Log(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	LogString(pH,false);

	return (pH->EndFunction());
}

/////////////////////////////////////////////////////////////////////////////////
//log a string to the console and or to the file with support for different
//languages
void CScriptBind_System::LogString(IFunctionHandler *pH,bool bToConsoleOnly)
{
	const char *sParam=NULL;
	string szText;

	pH->GetParam(1,sParam);

	if (sParam)
  {
    // add the "<Lua> " prefix to understand that this message
		// has been called from a script function
    char sLogMessage[1024];

		if(sParam[0]<=5 && sParam[0]!=0)
		{
			sLogMessage[0] = sParam[0];
			strcpy(&sLogMessage[1], "<Lua> ");
			strncat(sLogMessage, &sParam[1], sizeof(sLogMessage)-7);
		}
		else
		{
			strcpy(sLogMessage, "<Lua> ");
			strncat(sLogMessage, sParam, sizeof(sLogMessage)-7);
		}

		sLogMessage[sizeof(sLogMessage)-1]=0;

		if (bToConsoleOnly)
			m_pLog->LogToConsole(sLogMessage);
		else
			m_pLog->Log(sLogMessage);
  }

}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::SetConsoleImage(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	const char *pszName;
	bool bRemoveCurrent;
	pH->GetParam(1, pszName);
	pH->GetParam(2, bRemoveCurrent);

	//remove the previous image
	//ITexPic *pPic=m_pConsole->GetImage();
	//pPic->Release(false); //afaik true removes the ref counter
	//m_pConsole->SetImage(NULL); //remove the image

	//load the new image
	ITexture *pPic=m_pRenderer->EF_LoadTexture(pszName,FT_DONT_RELEASE,eTT_2D);
	m_pConsole->SetImage(pPic,bRemoveCurrent);

	return pH->EndFunction();
}


/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::GetCurrTime(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);
	float fTime = m_pTimer->GetCurrTime();
	return pH->EndFunction(fTime);
}

int CScriptBind_System::GetCurrAsyncTime(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);
	float fTime = m_pTimer->GetAsyncCurTime();
	return pH->EndFunction(fTime);
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::GetFrameTime(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);
	float fTime = m_pTimer->GetFrameTime();
	return pH->EndFunction(fTime);
}
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::GetLocalOSTime(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);
	//! Get time.
#if defined(LINUX) || defined(PS3)
	time_t long_time = time( NULL );
	struct tm *newtime = localtime( &long_time ); /* Convert to local time. */
#else
	__time64_t long_time;
	_time64( &long_time );                /* Get time as long integer. */
	struct tm *newtime = _localtime64( &long_time ); /* Convert to local time. */
#endif

	float ftime = 0;
	if (newtime)
	{
		m_pScriptTimeTable->BeginSetGetChain();
		m_pScriptTimeTable->SetValueChain("sec",newtime->tm_sec);
		m_pScriptTimeTable->SetValueChain("min",newtime->tm_min);
		m_pScriptTimeTable->SetValueChain("hour",newtime->tm_hour);
		m_pScriptTimeTable->SetValueChain("isdst",newtime->tm_isdst);
		m_pScriptTimeTable->SetValueChain("mday",newtime->tm_mday);
		m_pScriptTimeTable->SetValueChain("wday",newtime->tm_wday);
		m_pScriptTimeTable->SetValueChain("mon",newtime->tm_mon);
		m_pScriptTimeTable->SetValueChain("yday",newtime->tm_yday);
		m_pScriptTimeTable->SetValueChain("year",newtime->tm_year);
		m_pScriptTimeTable->EndSetGetChain();
	}
	return pH->EndFunction(m_pScriptTimeTable);
}
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::ShowConsole(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	int nParam = 0;
	pH->GetParam(1, nParam);
  m_pConsole->ShowConsole(nParam != 0);
	return pH->EndFunction();
}


/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::CheckHeapValid(IFunctionHandler *pH)
{
	const char *name="<noname>";
	if (pH->GetParamCount()>0 && pH->GetParamType(1)==svtString)
		pH->GetParam(1, name);

	if (!IsHeapValid())
	{
		CryLogAlways("IsHeapValid failed: %s", name);
		assert(IsHeapValid());
	}

	return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::GetConfigSpec(IFunctionHandler *pH)
{
	static ICVar* e_obj_quality(gEnv->pConsole->GetCVar("e_obj_quality"));
	int obj_quality = CONFIG_VERYHIGH_SPEC;
	if (e_obj_quality)
		obj_quality = e_obj_quality->GetIVal();

	return pH->EndFunction( obj_quality );
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::IsMultiplayer(IFunctionHandler *pH)
{
	return pH->EndFunction(gEnv->bMultiplayer);
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*!Get an entity by id
	@param nID the entity id
*/
int CScriptBind_System::GetEntity(IFunctionHandler *pH)
{
	//support also number type
	EntityId eID(0);
	if (pH->GetParamType(1) == svtNumber)
	{
		pH->GetParam(1,eID);
	}
	else
	{
		ScriptHandle sh;
		pH->GetParam(1,sh);
		eID = sh.n;
	}

	IEntity *pEntity = m_pSystem->GetIEntitySystem()->GetEntity(eID);

	if(pEntity)
	{
		IScriptTable *pObject = pEntity->GetScriptTable();

		if (pObject)
		{
			return pH->EndFunction(pObject);
		}
	}

	return pH->EndFunction();
}

int CScriptBind_System::GetEntityClass(IFunctionHandler *pH)
{
	EntityId eID(0);
	if (pH->GetParamType(1) == svtNumber)
	{
		pH->GetParam(1,eID);
	}
	else
	{
		ScriptHandle sh;
		pH->GetParam(1,sh);
		eID = sh.n;
	}

	IEntity *pEntity = m_pSystem->GetIEntitySystem()->GetEntity(eID);

	if(pEntity)
		return pH->EndFunction(pEntity->GetClass()->GetName());

	return pH->EndFunction();
}


/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*!return a all entities currently present in the level
	@return a table filled with all entities currently present in the level
*/
int CScriptBind_System::GetEntities(IFunctionHandler *pH)
{
	//SCRIPT_CHECK_PARAMETERS(0);
	Vec3 center(0,0,0);
	float radius(0);

	if (pH->GetParamCount()>1)
	{
		pH->GetParam(1,center);
		pH->GetParam(2,radius);
	}

	SmartScriptTable pObj(m_pSS);
	int k = 0;

	IEntityItPtr pIIt = m_pSystem->GetIEntitySystem()->GetEntityIterator();
	IEntity *pEntity = NULL;

	while(pEntity = pIIt->Next())
	{
		if (radius)
			if ((pEntity->GetWorldPos() - center).len2()>radius*radius)
				continue;

		if (pEntity->GetScriptTable())
		{
			/*ScriptHandle handle;
			handle.n = pEntity->GetId();*/

			pObj->SetAt(k, pEntity->GetScriptTable());
			k++;
		}
	}
	return pH->EndFunction(*pObj);
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*!return a all entities for a specified entity class
@return a table filled with all entities of a specified entity class
*/
int CScriptBind_System::GetEntitiesByClass(IFunctionHandler *pH, const char* EntityClass)
{
	if ( EntityClass == NULL || EntityClass[0] == '\0' )
		return pH->EndFunction();

	IEntityClass *pClass = m_pSystem->GetIEntitySystem()->GetClassRegistry()->FindClass(EntityClass);
	
	if (!pClass)
	{
		return pH->EndFunction();
	}

	SmartScriptTable pObj(m_pSS);
	IEntityItPtr pIIt = m_pSystem->GetIEntitySystem()->GetEntityIterator();
	IEntity *pEntity = 0;
	int	k = 1;

	pIIt->MoveFirst();

	while(pEntity = pIIt->Next())
	{
		if (pEntity->GetClass() == pClass)
		{
			if (pEntity->GetScriptTable())
			{
				pObj->SetAt(k++, pEntity->GetScriptTable());
			}
		}
	}

	return pH->EndFunction(*pObj);
}


/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*!return a all entities for a specified entity class
@return a table filled with all entities of a specified entity class in a radius
*/
int CScriptBind_System::GetEntitiesInSphere(IFunctionHandler *pH, Vec3 center, float radius)
{
	SmartScriptTable pObj(m_pSS);
	IEntityItPtr pIIt = m_pSystem->GetIEntitySystem()->GetEntityIterator();
	IEntity *pEntity = 0;
	int	k = 1;

	pIIt->MoveFirst();

	while(pEntity = pIIt->Next())
	{
		if ((pEntity->GetWorldPos()-center).len2() <= radius*radius)
		{
			if (pEntity->GetScriptTable())
			{
				pObj->SetAt(k++, pEntity->GetScriptTable());
			}
		}
	}

	return pH->EndFunction(*pObj);
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*!return a all entities for a specified entity class
@return a table filled with all entities of a specified entity class in a radius
*/
int CScriptBind_System::GetEntitiesInSphereByClass(IFunctionHandler *pH, Vec3 center, float radius, const char *EntityClass)
{
	if ( EntityClass == NULL || EntityClass[0] == '\0' )
		return pH->EndFunction();

	IEntityClass *pClass = m_pSystem->GetIEntitySystem()->GetClassRegistry()->FindClass(EntityClass);

	if (!pClass)
	{
		return pH->EndFunction();
	}

	SmartScriptTable pObj(m_pSS);
	IEntityItPtr pIIt = m_pSystem->GetIEntitySystem()->GetEntityIterator();
	IEntity *pEntity = 0;
	int	k = 1;

	pIIt->MoveFirst();

	while(pEntity = pIIt->Next())
	{
		if ((pEntity->GetClass() == pClass) && ((pEntity->GetWorldPos()-center).len2() <= radius*radius))
		{
			if (pEntity->GetScriptTable())
			{
				pObj->SetAt(k++, pEntity->GetScriptTable());
			}
		}
	}

	return pH->EndFunction(*pObj);
}

inline bool Filter(struct __finddata64_t& fd, int nScanMode)
{
	if (!strcmp(fd.name, ".") || !strcmp(fd.name, ".."))
		return false;

	switch (nScanMode)
	{
	case SCANDIR_ALL:
		return true;
	case SCANDIR_SUBDIRS:
		return 0 != (fd.attrib & _A_SUBDIR);
	case SCANDIR_FILES:
		return 0 == (fd.attrib & _A_SUBDIR);
	default:
		return false;
	}
}

inline bool Filter(struct _finddata_t& fd, int nScanMode)
{
	if (!strcmp(fd.name, ".") || !strcmp(fd.name, ".."))
		return false;

	switch (nScanMode)
	{
	case SCANDIR_ALL:
		return true;
	case SCANDIR_SUBDIRS:
		return 0 != (fd.attrib & _A_SUBDIR);
	case SCANDIR_FILES:
		return 0 == (fd.attrib & _A_SUBDIR);
	default:
		return false;
	}
}

/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::ScanDirectory(IFunctionHandler *pH)
{
	if (pH->GetParamCount()<1)
		return pH->EndFunction();

	SmartScriptTable pObj(m_pSS);
	int k=1;

	const char *pszFolderName;
	if (!pH->GetParam(1,pszFolderName))
		return pH->EndFunction(*pObj);

	int nScanMode=SCANDIR_SUBDIRS;

	if (pH->GetParamCount()>1)
		pH->GetParam(2, nScanMode);

	{
		_finddata_t c_file;
		intptr_t hFile;

		if ((hFile = m_pSystem->GetIPak()->FindFirst((string(pszFolderName) + "\\*.*").c_str(), &c_file)) == -1L)
		{
			return pH->EndFunction(*pObj);
		}
		else
		{
			do
			{
				if (Filter (c_file, nScanMode))
				{
					pObj->SetAt(k, c_file.name);
					k++;
				}
			}
			while(m_pSystem->GetIPak()->FindNext(hFile, &c_file)==0);

			m_pSystem->GetIPak()->FindClose(hFile);
		}
	}

	return pH->EndFunction(*pObj);
}

/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::DrawLabelImage(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);

	Vec3 vPos(0,0,0);
	float fSize;

	ScriptHandle handle;
	if (!pH->GetParams(vPos,fSize,handle))
		return pH->EndFunction();

	int nTextureId = handle.n;
	if (nTextureId)
	{
		if (m_pRenderer)
			m_pRenderer->DrawLabelImage(vPos,fSize,nTextureId);
	}
	return (pH->EndFunction());
}

/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::DrawLabel(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(7);

	Vec3 vPos(0,0,0);
	float fSize;
	const char *text;
	float r(1);
	float g(1);
	float b(1);
	float alpha(1);

	if (!pH->GetParams(vPos, fSize, text, r, g, b, alpha))
		return pH->EndFunction();

	if (m_pRenderer)
	{
		float color[] = {r,g,b,alpha};

		m_pRenderer->DrawLabelEx(vPos,fSize,color,true,true,text);
		//m_pRenderer->DrawLabel(vPos, fSize, text);
	}

	return (pH->EndFunction());
}


/////////////////////////////////////////////////////////////////////////////////
/*!return all entities contained in a certain radius
		@param oVec center of the sphere
		@param fRadius length of the radius
		@return a table filled with all entities contained in the specified radius
*/
int CScriptBind_System::GetPhysicalEntitiesInBox(IFunctionHandler *pH, Vec3 center, float radius)
{
	IEntitySystem *pEntitySystem = m_pSystem->GetIEntitySystem();


	SEntityProximityQuery query;
	query.box.min = center - Vec3(radius,radius,radius);
	query.box.max = center + Vec3(radius,radius,radius);
	pEntitySystem->QueryProximity( query );

	int n = query.nCount;
	if (n > 0)
	{
		SmartScriptTable		tbl(m_pSS);

		int k = 0;
		for (int i=0; i < n; i++)
		{
			IEntity *pEntity = query.pEntities[i];
			if (pEntity)
			{
				// The physics can return multiple records per one entity, filter out entities of same id.
				if (!pEntity->GetPhysics())
					continue;
				IScriptTable *pEntityTable = pEntity->GetScriptTable();
				if (pEntityTable)
				{
					tbl->SetAt(++k, pEntityTable);
				}
			}
		}
		if (k)
			return pH->EndFunction(tbl);
	}

	return pH->EndFunction();
}

int CScriptBind_System::GetPhysicalEntitiesInBoxByClass(IFunctionHandler *pH, Vec3 center, float radius, const char *className)
{
	IEntitySystem *pEntitySystem = m_pSystem->GetIEntitySystem();

	IEntityClass *pClass = pEntitySystem->GetClassRegistry()->FindClass(className);
	if (!pClass)
	{
		return pH->EndFunction();
	}

	SEntityProximityQuery query;
	query.box.min = center - Vec3(radius,radius,radius);
	query.box.max = center + Vec3(radius,radius,radius);
	query.pEntityClass = pClass;
	pEntitySystem->QueryProximity( query );

	int n = query.nCount;
	if (n > 0)
	{
		SmartScriptTable		tbl(m_pSS);

		int k = 0;
		for (int i=0; i < n; i++)
		{
			IEntity *pEntity = query.pEntities[i];
			if (pEntity)
			{
				// The physics can return multiple records per one entity, filter out entities of same id.
				if (!pEntity->GetPhysics())
					continue;
				IScriptTable *pEntityTable = pEntity->GetScriptTable();
				if (pEntityTable)
				{
					tbl->SetAt(++k, pEntityTable);
				}
			}
		}
		if (k)
			return pH->EndFunction(tbl);
	}
	return pH->EndFunction();
}

int CScriptBind_System::GetNearestEntityByClass(IFunctionHandler *pH, Vec3 center, float radius, const char *className)
{
	IEntitySystem *pEntitySystem = m_pSystem->GetIEntitySystem();
	IEntityClass *pClass = pEntitySystem->GetClassRegistry()->FindClass(className);
	if (!pClass)
		return pH->EndFunction();

	ScriptHandle ignore[2];
	ignore[0].n=ignore[1].n=0;

	if (pH->GetParamCount()>3)
		pH->GetParam(4, ignore[0]);
	if (pH->GetParamCount()>4)
		pH->GetParam(5, ignore[1]);

	SEntityProximityQuery query;
	query.box.min = center - Vec3(radius,radius,radius);
	query.box.max = center + Vec3(radius,radius,radius);
	query.pEntityClass = pClass;
	pEntitySystem->QueryProximity( query );


	int closest=-1;
	float closestdist2=1e+8f;

	int n = query.nCount;
	if (n > 0)
	{
		for (int i=0; i<n; i++)
		{
			IEntity *pEntity = query.pEntities[i];
			if (pEntity && pEntity->GetId()!=ignore[0].n && pEntity->GetId()!=ignore[1].n)
			{
				float dist2=(pEntity->GetWorldPos()-center).len2();
				if (dist2<closestdist2)
				{
					closest=i;
					closestdist2=dist2;
				}
			}
		}

		if (closest>-1)
			return pH->EndFunction(query.pEntities[closest]->GetScriptTable());
	}

	return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::GetEntityByName(IFunctionHandler *pH,const char *sEntityName)
{
	IEntity *pEntity = m_pSystem->GetIEntitySystem()->FindEntityByName(sEntityName);
	if(pEntity)
	{
		IScriptTable *pObject = pEntity->GetScriptTable();
		return pH->EndFunction(pObject);
	}
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::GetEntityIdByName(IFunctionHandler *pH,const char *sEntityName)
{
	IEntity *pEntity = m_pSystem->GetIEntitySystem()->FindEntityByName(sEntityName);
	if(pEntity)
	{
		ScriptHandle handle;
		handle.n = pEntity->GetId();
		return pH->EndFunction(handle);
	}
	return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*!create a terrain deformation at the given point
	this function is called when a projectile or a grenade explode
	@param oVec explosion position
	@param fSize explosion radius
*/

int CScriptBind_System::DeformTerrainInternal( IFunctionHandler *pH, bool nameIsMaterial )
{
	if (pH->GetParamCount() < 3)
		return pH->EndFunction();

	Vec3 vPos;
	float fSize;
	const char *name = 0;

	pH->GetParams(vPos, fSize, name);

	bool bDeform = true;

	if (pH->GetParamCount() > 3)
		pH->GetParam(4, bDeform);

	m_p3DEngine->OnExplosion(vPos, fSize, bDeform);

	return pH->EndFunction();
}

int CScriptBind_System::DeformTerrain(IFunctionHandler *pH)
{
	return DeformTerrainInternal( pH, false );
}

int CScriptBind_System::DeformTerrainUsingMat(IFunctionHandler *pH)
{
	return DeformTerrainInternal( pH, true );
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::ScreenToTexture(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);
	m_pRenderer->ScreenToTexture();
	return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::DrawLine(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS( 6 );

	IRenderAuxGeom* pRenderAuxGeom( m_pSystem->GetIRenderer()->GetIRenderAuxGeom() );

	Vec3 p1( 0, 0, 0 );
	Vec3 p2( 0, 0, 0 );
	pH->GetParam( 1, p1 );
	pH->GetParam( 2, p2 );

	float r, g, b, a;
	pH->GetParam( 3, r );
	pH->GetParam( 4, g );
	pH->GetParam( 5, b );
	pH->GetParam( 6, a );
	ColorB col( (unsigned char) ( r * 255.0f ), (unsigned char) ( g * 255.0f), 
		(unsigned char) ( b * 255.0f ), (unsigned char) ( a * 255.0f ) );

	pRenderAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
	pRenderAuxGeom->DrawLine( p1, col, p2, col );

	return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::Draw2DLine(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS( 8 );

	IRenderAuxGeom* pRenderAuxGeom( m_pSystem->GetIRenderer()->GetIRenderAuxGeom() );

	Vec3 p1( 0, 0, 0 );
	Vec3 p2( 0, 0, 0 );
	pH->GetParam( 1, p1.x );
	pH->GetParam( 2, p1.y );
	pH->GetParam( 3, p2.x );
	pH->GetParam( 4, p2.y );

	// do normalization as exiting scripts assume a virtual window size of 800x600
	// however the auxiliary geometry rendering system uses normalized device coords for 2d primitive rendering
	const float c_Normalize2Dx( 1.0f / 800.0f );
	const float c_Normalize2Dy( 1.0f / 600.0f );
	p1.x *= c_Normalize2Dx;
	p1.y *= c_Normalize2Dy;
	p2.x *= c_Normalize2Dx;
	p2.y *= c_Normalize2Dy;

	float r, g, b, a;
	pH->GetParam( 5, r );
	pH->GetParam( 6, g );
	pH->GetParam( 7, b );
	pH->GetParam( 8, a );
	ColorB col( (unsigned char) ( r * 255.0f ), (unsigned char) ( g * 255.0f), 
		(unsigned char) ( b * 255.0f ), (unsigned char) ( a * 255.0f ) );


	SAuxGeomRenderFlags renderFlags( e_Def2DPublicRenderflags );

	if (a < 1.0)
	{
		renderFlags.SetAlphaBlendMode( e_AlphaBlended );
	}

	pRenderAuxGeom->SetRenderFlags( renderFlags );
	pRenderAuxGeom->DrawLine( p1, col, p2, col );

	return pH->EndFunction();
}

int CScriptBind_System::DrawText(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS_MIN(3);

	float x=0;
	float y=0;
	const char *text="";
	float size=3;
	float color[]={1,1,1,1};

	pH->GetParam(1, x);
	pH->GetParam(2, y);
	pH->GetParam(3, text);
	if (pH->GetParamCount() > 3 )
	{
		pH->GetParam(4, size);
		if (pH->GetParamCount() > 4 )
		{
			pH->GetParam(5, color[0]);
			if (pH->GetParamCount() > 5 )
			{
				pH->GetParam(6, color[1]);
				if (pH->GetParamCount() > 6 )
				{
					pH->GetParam(7, color[2]);
					if (pH->GetParamCount() > 7 )
						pH->GetParam(8, color[3]);
				}
			}
		}
	}
	gEnv->pRenderer->Draw2dLabel(x, y, size, color, false, text);
	return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*!draw an sprite on screen
	@param nTid texture id
	@param nPx x position(the screen is normalized at 800x600)
	@param nPy y position(the screen is normalized at 800x600)
	@param w width
	@param h height
	@param nMode blending mode (the number must be specified)
		R_BLEND_MODE__ZERO__SRC_COLOR					        1
		R_BLEND_MODE__SRC_COLOR__ZERO					        2
		R_BLEND_MODE__SRC_COLOR__ONE_MINUS_SRC_COLOR	3
		R_BLEND_MODE__SRC_ALPHA__ONE_MINUS_SRC_ALPHA	4
		R_BLEND_MODE__ONE__ONE                        5
		R_BLEND_MODE__DST_COLOR__SRC_COLOR            6
		R_BLEND_MODE__ZERO__ONE_MINUS_SRC_COLOR       7
		R_BLEND_MODE__ONE__ONE_MINUS_SRC_COLOR	      8
		R_BLEND_MODE__ONE__ZERO                       9
		R_BLEND_MODE__ZERO__ZERO                     10
		R_BLEND_MODE__ONE__ONE_MINUS_SRC_ALPHA       11
		R_BLEND_MODE__SRC_ALPHA__ONE                 12
		R_BLEND_MODE__ADD_SIGNED                     14
*/
int CScriptBind_System::DrawImage(IFunctionHandler *pH)
{
	int nTid;
	int nPx;
	int nPy;
	int w;
	int h;
	int nMode;
	ScriptHandle handle;

	if (!pH->GetParams(handle,nPx,nPy,w,h,nMode))
		return pH->EndFunction();

	nTid = handle.n;
	if (nTid)
	{
		pH->GetParam(2,nPx);
		pH->GetParam(3,nPy);
		pH->GetParam(4,w);
		pH->GetParam(5,h);
		pH->GetParam(6,nMode);

		if (nMode!=0)
			m_pRenderer->SetState(GS_NODEPTHTEST | sGetBlendState(nMode));

		m_pRenderer->Draw2dImage((float)nPx,(float)nPy,(float)w,(float)h,nTid, 0.f, 1.f, 1.f, 0.f);

		if (nMode!=0)
			m_pRenderer->SetState(GS_DEPTHWRITE);
	}
	return pH->EndFunction();
}
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*!draw an sprite on screen
	@param nTid texture id
	@param nPx x position(the screen is normalized at 800x600)
	@param nPy y position(the screen is normalized at 800x600)
	@param w width
	@param h height
	@param nMode blending mode (the number must be specified)
		R_BLEND_MODE__ZERO__SRC_COLOR					        1
		R_BLEND_MODE__SRC_COLOR__ZERO					        2
		R_BLEND_MODE__SRC_COLOR__ONE_MINUS_SRC_COLOR	3
		R_BLEND_MODE__SRC_ALPHA__ONE_MINUS_SRC_ALPHA	4
		R_BLEND_MODE__ONE__ONE                        5
		R_BLEND_MODE__DST_COLOR__SRC_COLOR            6
		R_BLEND_MODE__ZERO__ONE_MINUS_SRC_COLOR       7
		R_BLEND_MODE__ONE__ONE_MINUS_SRC_COLOR	      8
		R_BLEND_MODE__ONE__ZERO                       9
		R_BLEND_MODE__ZERO__ZERO                     10
		R_BLEND_MODE__ONE__ONE_MINUS_SRC_ALPHA       11
		R_BLEND_MODE__SRC_ALPHA__ONE                 12
		R_BLEND_MODE__ADD_SIGNED                     14
	@param r red component
	@param g green component
	@param b blue component
	@param a alpha component
*/
int CScriptBind_System::DrawImageColor(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(10);
	int nTid;
	float nPx;
	float nPy;
	float w;
	float h;
	int nMode;
	float r,g,b,a;
	//pH->GetParam(1,nTid);
	ScriptHandle handle;
	pH->GetParam(1,handle);
	nTid = handle.n;
	if(nTid)
	{
		pH->GetParam(2,nPx);
		pH->GetParam(3,nPy);
		pH->GetParam(4,w);
		pH->GetParam(5,h);
		pH->GetParam(6,nMode);
		pH->GetParam(7,r);
		pH->GetParam(8,g);
		pH->GetParam(9,b);
		pH->GetParam(10,a);

		if (nMode!=0)
    	m_pRenderer->SetState(GS_NODEPTHTEST | sGetBlendState(nMode));

		m_pRenderer->Draw2dImage(nPx,nPy,w,h,nTid,0,1,1,0,0,r,g,b,a);

		if (nMode!=0)
    	m_pRenderer->SetState(GS_DEPTHWRITE);
	}
	return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*!draw an sprite on screen
@param nTid texture id
@param nPx x position(the screen is normalized at 800x600)
@param nPy y position(the screen is normalized at 800x600)
@param w width
@param h height
@param nMode blending mode (the number must be specified)
R_BLEND_MODE__ZERO__SRC_COLOR					        1
R_BLEND_MODE__SRC_COLOR__ZERO					        2
R_BLEND_MODE__SRC_COLOR__ONE_MINUS_SRC_COLOR	3
R_BLEND_MODE__SRC_ALPHA__ONE_MINUS_SRC_ALPHA	4
R_BLEND_MODE__ONE__ONE                        5
R_BLEND_MODE__DST_COLOR__SRC_COLOR            6
R_BLEND_MODE__ZERO__ONE_MINUS_SRC_COLOR       7
R_BLEND_MODE__ONE__ONE_MINUS_SRC_COLOR	      8
R_BLEND_MODE__ONE__ZERO                       9
R_BLEND_MODE__ZERO__ZERO                     10
R_BLEND_MODE__ONE__ONE_MINUS_SRC_ALPHA       11
R_BLEND_MODE__SRC_ALPHA__ONE                 12
R_BLEND_MODE__ADD_SIGNED                     14
@param u1 1st u component of the texcoords
@param v1 1st v component of the texcoords
@param u2 2nd u component of the texcoords
@param v2 2nd v component of the texcoords
*/
int CScriptBind_System::DrawImageCoords(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(10);

	int nTid;
	int nPx;
	int nPy;
	int w;
	int h;
	int nMode;
	float u1, v1, u2, v2;
	//pH->GetParam(1,nTid);
	ScriptHandle handle;
	pH->GetParam(1,handle);
	nTid = handle.n;
	if(nTid)
	{
		pH->GetParam(2,nPx);
		pH->GetParam(3,nPy);
		pH->GetParam(4,w);
		pH->GetParam(5,h);
		pH->GetParam(6,nMode);
		pH->GetParam(7,u1);
		pH->GetParam(8,v1);
		pH->GetParam(9,u2);
		pH->GetParam(10,v2);
		if (nMode!=0)
    	m_pRenderer->SetState(GS_NODEPTHTEST | sGetBlendState(nMode));

 		m_pRenderer->Draw2dImage((float)nPx,(float)nPy,(float)w,(float)h,nTid, u1, v1, u2, v2);

		if (nMode!=0)
    	m_pRenderer->SetState(GS_DEPTHWRITE);
	}
	return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*!draw an sprite on screen
	@param nTid texture id
	@param nPx x position(the screen is normalized at 800x600)
	@param nPy y position(the screen is normalized at 800x600)
	@param w width
	@param h height
	@param nMode blending mode (the number must be specified)
		R_BLEND_MODE__ZERO__SRC_COLOR					        1
		R_BLEND_MODE__SRC_COLOR__ZERO					        2
		R_BLEND_MODE__SRC_COLOR__ONE_MINUS_SRC_COLOR	3
		R_BLEND_MODE__SRC_ALPHA__ONE_MINUS_SRC_ALPHA	4
		R_BLEND_MODE__ONE__ONE                        5
		R_BLEND_MODE__DST_COLOR__SRC_COLOR            6
		R_BLEND_MODE__ZERO__ONE_MINUS_SRC_COLOR       7
		R_BLEND_MODE__ONE__ONE_MINUS_SRC_COLOR	      8
		R_BLEND_MODE__ONE__ZERO                       9
		R_BLEND_MODE__ZERO__ZERO                     10
		R_BLEND_MODE__ONE__ONE_MINUS_SRC_ALPHA       11
		R_BLEND_MODE__SRC_ALPHA__ONE                 12
		R_BLEND_MODE__ADD_SIGNED                     14
	@param r red component
	@param g green component
	@param b blue component
	@param a alpha component
	@param u1 1st u component of the texcoords
	@param v1 1st v component of the texcoords
	@param u2 2nd u component of the texcoords
	@param v2 2nd v component of the texcoords
*/
int CScriptBind_System::DrawImageColorCoords(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(14);
	int nTid;
	float nPx;
	float nPy;
	float w;
	float h;
	int nMode;
	float r,g,b,a;
	float u1, v1, u2, v2;
	ScriptHandle handle;
	pH->GetParam(1,handle);
	nTid = handle.n;
	if (nTid)
	{
		//pH->GetParam(1,nTid);
		pH->GetParam(2,nPx);
		pH->GetParam(3,nPy);
		pH->GetParam(4,w);
		pH->GetParam(5,h);
		pH->GetParam(6,nMode);
		pH->GetParam(7,r);
		pH->GetParam(8,g);
		pH->GetParam(9,b);
		pH->GetParam(10,a);
		pH->GetParam(11,u1);
		pH->GetParam(12,v1);
		pH->GetParam(13,u2);
		pH->GetParam(14,v2);

		if (nMode!=0)
    	m_pRenderer->SetState(GS_NODEPTHTEST | sGetBlendState(nMode));

		m_pRenderer->Draw2dImage(nPx,nPy,w,h,nTid,u1,v1,u2,v2,0,r,g,b,a);

		if (nMode!=0)
    	m_pRenderer->SetState(GS_DEPTHWRITE);
	}
	return pH->EndFunction();
}
/////////////////////////////////////////////////////////////////////////////////

struct _vtx_temp_t {
	float x,y,z;
	unsigned char c[4];
	float u,v;
};
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::DrawTriStrip(IFunctionHandler *pH)
{
#define _MAX_VTXS 10
	int nTid;
	int nCookie=0;
	int nMode=0;
	float a,r,g,b;
	ScriptHandle handle;
	pH->GetParam(1,handle);
	nTid = handle.n;
	pH->GetParam(4,r);
	pH->GetParam(5,g);
	pH->GetParam(6,b);
	pH->GetParam(7,a);
	SmartScriptTable vtxs(m_pSS,true);
	if (nTid && pH->GetParam(2,nMode)  && pH->GetParam(3,vtxs))
	{
		SmartScriptTable vtx(m_pSS,true);
		_vtx_temp_t v[_MAX_VTXS];
		int nvtxs=0;
		IScriptTable::Iterator iter = vtxs->BeginIteration();
		ScriptAnyValue any;
		any.type = ANY_TTABLE;
		any.table = vtx;
		while(vtxs->MoveNext(iter) && nvtxs<_MAX_VTXS)
		{
			v[nvtxs].z=0;
			v[nvtxs].c[0] = (uint8)(r*0xFF);
			v[nvtxs].c[1] = (uint8)(g*0xFF);
			v[nvtxs].c[2] = (uint8)(b*0xFF);
			v[nvtxs].c[3] = (uint8)(a*0xFF);
			vtx->GetValue("x",v[nvtxs].x);
			vtx->GetValue("y",v[nvtxs].y);
			vtx->GetValue("u",v[nvtxs].u);
			vtx->GetValue("v",v[nvtxs].v);
			nvtxs++;
		}
		vtxs->EndIteration(iter);
		if(nvtxs)
		{
			CVertexBuffer vb(&v,VERTEX_FORMAT_P3F_COL4UB_TEX2F,nvtxs);
			m_pRenderer->Set2DMode(true,800,600);
			m_pRenderer->SetTexture(nTid);
      unsigned int nState = GS_NODEPTHTEST;
			m_pRenderer->SetCullMode(R_CULL_DISABLE);
			if (nMode!=0)
			{
        nState |= sGetBlendState(nMode);
        m_pRenderer->SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
			}
      m_pRenderer->SetState(nState);
			m_pRenderer->DrawTriStrip(&vb,nvtxs);
			if (nMode!=0){
				m_pRenderer->SetState(GS_DEPTHWRITE);
        m_pRenderer->SetColorOp(eCO_REPLACE, eCO_REPLACE, DEF_TEXARG0, DEF_TEXARG0);
			}
			//m_pRenderer->SetCullMode(R_CULL_DISABLE);
			m_pRenderer->Set2DMode(false,0,0);
		}

	}

	return pH->EndFunction();
#undef _MAX_VTXS
}
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::DrawRectShader(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(9);
	const char *pszShaderName;
	float x, y, w, h, r, g, b, a;
	pH->GetParam(1, pszShaderName);
	pH->GetParam(2, x);
	pH->GetParam(3, y);
	pH->GetParam(4, w);
	pH->GetParam(5, h);
	pH->GetParam(6, r);
	pH->GetParam(7, g);
	pH->GetParam(8, b);
	pH->GetParam(9, a);
  IShader *sh = m_pRenderer->EF_LoadShader((char *)pszShaderName);
  if (!(sh->GetFlags() & EF_NOTFOUND))
  {
    m_pRenderer->EF_StartEf();
    ColorF col(r,g,b,a);
    //m_pRenderer->EF_DrawEf(sh, x, y, w, h, col);
    m_pRenderer->Set2DMode(true,800,600);
    m_pRenderer->EF_EndEf2D(true);
    m_pRenderer->Set2DMode(false,800,600);
  }
	return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::SetScreenShader(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	const char *pszShaderName;

  pH->GetParam(1, pszShaderName);

//  m_pSystem->GetI3DEngine()->SetScreenShader(pszShaderName);
	assert(!"SetScreenShader is not supported anymore");

  return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
// <<NOTE>> check 3dScreenEffects for a list of effects names and respective parameters

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

int CScriptBind_System::SetPostProcessFxParam(IFunctionHandler *pH)
{
  SCRIPT_CHECK_PARAMETERS(2);  
  const char *pszEffectParam=0;    
  pH->GetParam(1, pszEffectParam);  

  ScriptVarType type=pH->GetParamType(2);
  switch( type )
  {
  case svtNumber:
    {
      float fValue=-1;
      pH->GetParam(2, fValue);
      m_pSystem->GetI3DEngine()->SetPostEffectParam(pszEffectParam, fValue);

      break;
    }
  case svtObject:
    {
      Vec3 pValue = Vec3(0, 0, 0);
      pH->GetParam(2, pValue);     
      m_pSystem->GetI3DEngine()->SetPostEffectParamVec4(pszEffectParam, Vec4(pValue, 1));

      break;
    }
  case svtString:
    {
      const char *pszValue = 0;
      pH->GetParam(2, pszValue);  
      m_pSystem->GetI3DEngine()->SetPostEffectParamString(pszEffectParam, pszValue);    

      break;
    }
  }

  return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::GetPostProcessFxParam(IFunctionHandler *pH)
{  
  SCRIPT_CHECK_PARAMETERS(2);
  const char *pszEffectParam=0;
  pH->GetParam(1, pszEffectParam);

  ScriptVarType type=pH->GetParamType(2);

  switch( type )
  {
  case svtNumber:
    {
      float fValue = 0;

      pH->GetParam(2, fValue);
      m_pSystem->GetI3DEngine()->GetPostEffectParam(pszEffectParam, fValue);
      return pH->EndFunction( fValue );
        
      break;
    }
  case svtString:
    { 
      const char *pszValue=0;
      pH->GetParam(2, pszValue);  
      m_pSystem->GetI3DEngine()->GetPostEffectParamString(pszEffectParam, pszValue);    
      return pH->EndFunction( pszValue );

      break;
    }
  }

  return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::SetScreenFx(IFunctionHandler *pH)
{
  return SetPostProcessFxParam(pH);
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::GetScreenFx(IFunctionHandler *pH)
{
  return GetPostProcessFxParam(pH);
}

/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::SetCVar(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);  
	
	const char *sCVarName=0;    
	pH->GetParam(1, sCVarName);

	ICVar* pCVar = m_pSystem->GetIConsole()->GetCVar(sCVarName);

	if(!pCVar)
		m_pLog->LogError("Script SetCVar('%s') not found - fix this",sCVarName);
	else
	{
		ScriptVarType type=pH->GetParamType(2);
		if(type == svtNumber)
		{
			float fValue=0;
			pH->GetParam(2, fValue);
			pCVar->Set(fValue);
		}
		else if(type == svtString)
		{
			const char *sValue="";
			pH->GetParam(2, sValue);  
			pCVar->Set(sValue);
		}
	}

	return pH->EndFunction();
}

int CScriptBind_System::GetCVar(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	const char *sCVarName=0;    
	pH->GetParam(1, sCVarName);

	ICVar* pCVar = m_pSystem->GetIConsole()->GetCVar(sCVarName);

	// fallback
	if(!pCVar)
		m_pLog->LogError("Script GetCVar('%s') not found - fix this",sCVarName);
	else
	{
		if (pCVar->GetType()==CVAR_FLOAT || pCVar->GetType()==CVAR_INT)
		{
			return pH->EndFunction(pCVar->GetFVal());
		}
		else if (pCVar->GetType()==CVAR_STRING)
		{
			return pH->EndFunction(pCVar->GetString());
		}
	}

	return pH->EndFunction();
}




/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::AddCCommand(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);  
	
	const char *sCCommandName=0;    
	pH->GetParam(1, sCCommandName);		assert(sCCommandName);

	const char *sCommand=0;    
	pH->GetParam(2, sCommand);				assert(sCCommandName);

	const char *sHelp=0;    
	pH->GetParam(3, sHelp);						assert(sHelp);

	m_pSystem->GetIConsole()->AddCommand(sCCommandName,sCommand,0,sHelp);

	return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////////////
// set scissoring screen area
int CScriptBind_System::SetScissor(IFunctionHandler *pH)
{
  SCRIPT_CHECK_PARAMETERS(4);
  float x, y, w, h;

  pH->GetParam(1, x);
  pH->GetParam(2, y);
  pH->GetParam(3, w);
  pH->GetParam(4, h);

  m_pSystem->GetIRenderer()->SetScissor(
	  (int)m_pSystem->GetIRenderer()->ScaleCoordX(x),
	  (int)m_pSystem->GetIRenderer()->ScaleCoordY(y),
	  (int)m_pSystem->GetIRenderer()->ScaleCoordX(w),
	  (int)m_pSystem->GetIRenderer()->ScaleCoordY(h));

  return pH->EndFunction();
}

int CScriptBind_System::ActivateLight(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	const char *pszLightName;
	bool bActive;
	pH->GetParam(1, pszLightName);
	pH->GetParam(2, bActive);

	assert(!"m_p3DEngine->ActivateLight by name is not supported anymore.");

//	m_p3DEngine->ActivateLight(pszLightName, bActive);
	return pH->EndFunction();
}
/*
int CScriptBind_System::ActivateMainLight(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	bool bActive;
	CScriptVector oVec(m_pSS);
	if(pH->GetParam(1,*oVec))
	{
		pH->GetParam(2, bActive);
		m_p3DEngine->GetBuildingManager()->ActivateMainLight(oVec.Get(), bActive);
	}
	return pH->EndFunction();
}	*/
/*
int CScriptBind_System::SetSkyBox(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	const char *pszShaderName;
	float fBlendTime;
	bool bUseWorldBrAndColor;
	pH->GetParam(1, pszShaderName);
	pH->GetParam(2, fBlendTime);
	pH->GetParam(3, bUseWorldBrAndColor);
	m_p3DEngine->SetSkyBox(pszShaderName);// not supported now: fBlendTime, bUseWorldBrAndColor);
	return pH->EndFunction();
}*/

int CScriptBind_System::IsValidMapPos(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	bool bValid=false;

	Vec3 v(0,0,0);
	if(pH->GetParam(1,v))
	{
		int nTerrainSize=m_p3DEngine->GetTerrainSize();
		float fOut=(float)(nTerrainSize+500);
		if(v.x<-500 || v.y<-500 || v.z>500 || v.x>fOut || v.y>fOut)
			bValid=false;
		else
			bValid=true;
	}
	return pH->EndFunction(bValid);
}


int CScriptBind_System::EnableMainView(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

  assert(0); // feature unimplemented

  /*bool bEnable;
	pH->GetParam(1,bEnable);
	if (m_p3DEngine)
		m_p3DEngine->EnableMainViewRendering(bEnable);*/

	return pH->EndFunction();
}

int CScriptBind_System::DebugStats(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	bool cp;
	pH->GetParam(1,cp);
	m_pSystem->DebugStats(cp, false);
	return pH->EndFunction();
}


//
//------------------------------------------------------------------------------
int CScriptBind_System::ViewDistanceSet(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	float		viewDist;
	pH->GetParam(1,viewDist);
	if(viewDist<20)
		viewDist = 20;

  // Tell Vlad if this feature is needed
  m_pLog->LogWarning("Direct call of I3DEngine::SetMaxViewDistance() is not supported");
  assert(!"Direct call of I3DEngine::SetMaxViewDistance() is not supported");

//	m_p3DEngine->SetMaxViewDistance(viewDist);

	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::ViewDistanceGet(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);
	float	fDist = m_p3DEngine->GetMaxViewDistance( );
	return pH->EndFunction( fDist );
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::SetSunColor(IFunctionHandler *pH,Vec3 vColor)
{
//	assert(!"Direct call of I3DEngine::SetSunColor() is not supported anymore. Use Time of day featue instead.");
	m_p3DEngine->SetGlobalParameter( E3DPARAM_SUN_COLOR,vColor );
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::GetSunColor(IFunctionHandler *pH)
{
	Vec3 vColor;
	m_p3DEngine->GetGlobalParameter( E3DPARAM_SUN_COLOR,vColor );
	return pH->EndFunction(vColor);
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::SetSkyColor(IFunctionHandler *pH,Vec3 vColor)
{
//	assert(!"Direct call of I3DEngine::SetSkyColor() is not supported anymore. Use Time of day feature instead.");
	m_p3DEngine->SetGlobalParameter( E3DPARAM_SKY_COLOR,vColor );
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::GetSkyColor(IFunctionHandler *pH)
{
	Vec3 vColor;
	m_p3DEngine->GetGlobalParameter( E3DPARAM_SKY_COLOR,vColor );
	return pH->EndFunction(vColor);
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::SetSkyHighlight(IFunctionHandler *pH,SmartScriptTable tbl)
{
	float fSize = 0;
	Vec3 vColor(0,0,0);
	Vec3 vPos(0,0,0);
	tbl->GetValue( "size",fSize );
	tbl->GetValue( "color",vColor );
	tbl->GetValue( "position",vPos );
	m_p3DEngine->SetGlobalParameter( E3DPARAM_SKY_HIGHLIGHT_SIZE,fSize );
	m_p3DEngine->SetGlobalParameter( E3DPARAM_SKY_HIGHLIGHT_COLOR,vColor );
	m_p3DEngine->SetGlobalParameter( E3DPARAM_SKY_HIGHLIGHT_POS,vPos );

	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::GetSkyHighlight(IFunctionHandler *pH,SmartScriptTable params)
{
	float fSize = 0;
	Vec3 vColor(0,0,0);
	Vec3 vPos(0,0,0);
	fSize = m_p3DEngine->GetGlobalParameter( E3DPARAM_SKY_HIGHLIGHT_SIZE );
	m_p3DEngine->GetGlobalParameter( E3DPARAM_SKY_HIGHLIGHT_COLOR,vColor );
	m_p3DEngine->GetGlobalParameter( E3DPARAM_SKY_HIGHLIGHT_POS,vPos );

	params->SetValue( "size",fSize );
	params->SetValue( "color",vColor );
	params->SetValue( "position",vPos );

	return pH->EndFunction();
}

//
//------------------------------------------------------------------------------
int CScriptBind_System::ApplyForceToEnvironment(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	float	force;
	float	radius;

	Vec3 pos(0,0,0);
	pH->GetParam(1,pos);
	pH->GetParam(2, radius);
	pH->GetParam(3,force);

	m_p3DEngine->ApplyForceToEnvironment(pos,radius,force);

	return pH->EndFunction();
}


//------------------------------------------------------------------------------
int CScriptBind_System::GetOutdoorAmbientColor(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	Vec3 v3Color = m_p3DEngine->GetSkyColor();
	return pH->EndFunction(v3Color);
}

//
//------------------------------------------------------------------------------
int CScriptBind_System::SetOutdoorAmbientColor(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	Vec3 v3Color(0,0,0);
	pH->GetParam(1,v3Color);

	assert(!"Direct call of I3DEngine::SetSkyColor() is not supported anymore. Use Time of day featue instead.");
//	m_p3DEngine->SetSkyColor( v3Color );
	return pH->EndFunction();
}

//
//------------------------------------------------------------------------------

//
//------------------------------------------------------------------------------
int CScriptBind_System::GetTerrainElevation(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	Vec3 v3Pos;//,v3SysDir;
	pH->GetParam(1,v3Pos);
	float	elevation;

	elevation = m_p3DEngine->GetTerrainElevation(v3Pos.x, v3Pos.y);
	return pH->EndFunction( elevation );
}

/////////////////////////////////////////////////////////////////////////////////
/*int CScriptBind_System::SetIndoorColor(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	CScriptObjectColor oColor(m_pSS,true);
  Vec3 v3Color;
	pH->GetParam(1,*oColor);
	v3Color=oColor.Get();
	m_p3DEngine->GetBuildingManager()->SetIndoorAmbientColor( v3Color );
	return pH->EndFunction();
}	*/

/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::ActivatePortal(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);

	Vec3 vPos;
	ScriptHandle nID;
	pH->GetParam(1,vPos);
	bool bActivate;
	pH->GetParam(2,bActivate);
	pH->GetParam(3,nID);

	IEntity *pEnt = m_pSystem->GetIEntitySystem()->GetEntity(nID.n);

  m_p3DEngine->ActivatePortal( vPos, bActivate, pEnt ? pEnt->GetName() : "[Not specified by script]" );

	if(gEnv->pSoundSystem)
		gEnv->pSoundSystem->RecomputeSoundOcclusion(false,true);

	return (pH->EndFunction());
}

//
//------------------------------------------------------------------------------


int CScriptBind_System::DumpMMStats(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);
	m_pSystem->DumpMMStats(true);
	//m_pSS->GetMemoryStatistics(NULL);
	m_pLog->Log("***SCRIPT GC COUNT [%d kb]",m_pSS->GetCGCount());
	return pH->EndFunction();
}



int CScriptBind_System::EnumDisplayFormats(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);
	m_pLog->Log("Enumerating display settings...");
	SmartScriptTable pDispArray(m_pSS);
	SDispFormat* Formats = NULL;
	unsigned int i;
	unsigned int numFormats = m_pRenderer->EnumDisplayFormats(NULL);
	if(numFormats)
	{
		Formats = new SDispFormat[numFormats];
		m_pRenderer->EnumDisplayFormats(Formats);
	}

	for (i=0; i<numFormats; i++)
	{
		SDispFormat *pForm = &Formats[i];
		SmartScriptTable pDisp(m_pSS);
		pDisp->SetValue("width", pForm->m_Width);
		pDisp->SetValue("height", pForm->m_Height);
		pDisp->SetValue("bpp", pForm->m_BPP);

		// double check for multiple entries of the same resolution/color depth -- CW
		bool bInsert( true );
		for( int j( 0 ); j < pDispArray->Count(); ++j )
		{
			SmartScriptTable pDispCmp( m_pSS );
			if( false != pDispArray->GetAt( j + 1, pDispCmp ) )
			{
				int iWidthCmp( 0 );
				pDispCmp->GetValue( "width", iWidthCmp );

				int iHeightCmp( 0 );
				pDispCmp->GetValue( "height", iHeightCmp );

				int iBppCmp( 0 );
				pDispCmp->GetValue( "bpp", iBppCmp );

				if( pForm->m_Width == iWidthCmp &&
					pForm->m_Height == iHeightCmp &&
					pForm->m_BPP == iBppCmp )
				{
					bInsert = false;
					break;
				}
			}
		}
		if( false != bInsert )
		{
			pDispArray->SetAt( pDispArray->Count()+1,pDisp );
		}
	}

	if(numFormats==0)				// renderer is not doing his job
	{
		{
			SmartScriptTable pDisp(m_pSS);
			pDisp->SetValue("width", 640);
			pDisp->SetValue("height", 480);
			pDisp->SetValue("bpp", 32);
			pDispArray->SetAt(1, pDisp);
		}
		{
			SmartScriptTable pDisp(m_pSS);
			pDisp->SetValue("width", 800);
			pDisp->SetValue("height", 600);
			pDisp->SetValue("bpp", 32);
			pDispArray->SetAt(2, pDisp);
		}
		{
			SmartScriptTable pDisp(m_pSS);
			pDisp->SetValue("width", 1024);
			pDisp->SetValue("height", 768);
			pDisp->SetValue("bpp", 32);
			pDispArray->SetAt(3, pDisp);
		}
	}

	if(Formats)
		delete[] Formats;

	return pH->EndFunction(pDispArray);
}

int CScriptBind_System::EnumAAFormats(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);

	SDispFormat DispFmt;

	pH->GetParam(1,DispFmt.m_Width);
	pH->GetParam(2,DispFmt.m_Height);
	pH->GetParam(3,DispFmt.m_BPP);

	m_pLog->Log("Enumerating FSAA modes...");
	SmartScriptTable pAAArray(m_pSS);
	int numFormats = m_pRenderer->EnumAAFormats(DispFmt,NULL);
	if(numFormats)
	{
		SAAFormat* AAFormats = new SAAFormat[numFormats];
		m_pRenderer->EnumAAFormats(DispFmt,AAFormats);

		for (unsigned int i=0; i<numFormats; i++)
		{
			SAAFormat *pAAForm = &AAFormats[i];
			SmartScriptTable pAA(m_pSS);

			pAA->SetValue("desc", pAAForm->szDescr);
			pAA->SetValue("samples", pAAForm->nSamples);
			pAA->SetValue("quality", pAAForm->nQuality);

			pAAArray->SetAt( pAAArray->Count()+1,pAA );
		}

		delete []AAFormats;
	}

	return pH->EndFunction(pAAArray);
}

int CScriptBind_System::IsPointIndoors(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	bool bInside=false;

	Vec3 vPos(0,0,0);
	if(pH->GetParam(1,vPos))
	{
		I3DEngine *p3dEngine=m_pSystem->GetI3DEngine();
		if (p3dEngine)
			bInside = p3dEngine->GetVisAreaFromPos(vPos)!=0;
	}
	return pH->EndFunction(bInside);
}

int CScriptBind_System::SetGammaDelta(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	float fDelta = 0;
	pH->GetParam(1, fDelta);

  m_pSystem->GetIRenderer()->SetGammaDelta(fDelta);

	return pH->EndFunction();
}

int CScriptBind_System::ProjectToScreen(IFunctionHandler *pH, Vec3 vec)
{
	Vec3 res(0,0,0);
	m_pRenderer->ProjectToScreen(vec.x, vec.y, vec.z, &res.x, &res.y, &res.z);
	
	res.x *= 8.0f;
	res.y *= 6.0f;

	return pH->EndFunction(Script::SetCachedVector(res, pH, 2));
}

int CScriptBind_System::EnableHeatVision(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	int nEnable=0;
	if(pH->GetParam(1, nEnable))
	{
		assert(!"GetI3DEngine()->EnableHeatVision() is not supported anymore");
//		m_pSystem->GetI3DEngine()->EnableHeatVision(nEnable>0);
	}
	return pH->EndFunction();
}

/*
int CScriptBind_System::IndoorSoundAllowed(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	CScriptVector pVecOrigin(m_pSS,false);
	CScriptVector pVecDestination(m_pSS,false);

	IGraph *pGraph = m_pSystem->GetAISystem()->GetNodeGraph();

	Vec3 start = pVecOrigin.Get();
	Vec3 end = pVecDestination.Get();

	if ((start.x<0)||(start.x>2000.f)||(start.y<0)||(start.y>2000.f))
	{
		m_pLog->LogError("[CRASHWARNING] Unnaturally high value placed for sound detection");
		return pH->EndFunction(true); // there is no sound occlusion in outdoor
	}

	if ((end.x<0)||(end.x>2000.f)||(end.y<0)||(end.y>2000.f))
	{
		m_pLog->LogError("[CRASHWARNING] Unnaturally high value placed for sound detection");
		return pH->EndFunction(true); // there is no sound occlusion in outdoor
	}

	GraphNode *pStart = pGraph->GetEnclosing(pVecOrigin.Get());


	if (pStart->nBuildingID<0)
		return pH->EndFunction(true); // there is no sound occlusion in outdoor

	GraphNode *pEnd = pGraph->GetEnclosing(pVecDestination.Get());

	if (pStart->nBuildingID!=pEnd->nBuildingID)
		return pH->EndFunction(false); // if in 2 different buildings we cannot hear the sound for sure

	// make the real indoor sound occlusion check (doors, sectors etc.)
	return (m_pSystem->GetI3DEngine()->IsVisAreasConnected(pStart->pArea,pEnd->pArea,1,false));


	//IIndoorBase *pIndoor = m_p3DEngine->GetBuildingManager();
	//if (pIndoor)
	{
		// make the real indoor sound occlusion check (doors, sectors etc.)
		return pH->EndFunction(pIndoor->IsSoundPotentiallyHearable(pStart->nBuildingID,pStart->nSector,pEnd->nSector));
	//}

	return pH->EndFunction(true);
}
*/


int CScriptBind_System::EnableOceanRendering(IFunctionHandler *pH)
{
//	SCRIPT_CHECK_PARAMETERS(1);
  bool bOcean=true;
  if(pH->GetParam(1,bOcean))
	  if (m_p3DEngine)
		  m_p3DEngine->EnableOceanRendering(bOcean);

  return pH->EndFunction();
}

int CScriptBind_System::Break(IFunctionHandler *pH)
{
#ifdef WIN32
	CryError("CScriptBind_System:Break");
#endif
	return pH->EndFunction();
}



int CScriptBind_System::SetWaterVolumeOffset(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(4);

	assert(!"SetWaterLevel is not supported by 3dengine for now");

/*
	const char *pszWaterVolumeName=0;
	float fWaterOffsetX=0;
	float fWaterOffsetY=0;
	float fWaterOffsetZ=0;

	pH->GetParam(1, pszWaterVolumeName);
	pH->GetParam(2, fWaterOffsetX);
	pH->GetParam(3, fWaterOffsetY);
	pH->GetParam(4, fWaterOffsetZ);

	IWaterVolume * pWaterVolume = pszWaterVolumeName ? m_p3DEngine->FindWaterVolumeByName(pszWaterVolumeName) : 0;
	if(!pWaterVolume)
	{
		m_pSystem->Warning( VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, 0, 0,
			"CScriptBind_System::SetWaterVolumeOffset: Water volume not found: %s", pszWaterVolumeName ? pszWaterVolumeName : "Name is not set");
		return pH->EndFunction(false);
	}

	pWaterVolume->SetPositionOffset(Vec3(fWaterOffsetX,fWaterOffsetY,fWaterOffsetZ));
*/
	return pH->EndFunction();
}

int CScriptBind_System::SetViewCameraFov(IFunctionHandler *pH, float fov)
{
	CCamera &Camera=m_pSystem->GetViewCamera();
	
	Camera.SetFrustum(Camera.GetViewSurfaceX(), Camera.GetViewSurfaceZ(), fov);

	return pH->EndFunction();
}


int CScriptBind_System::GetViewCameraFov(IFunctionHandler *pH)
{
	CCamera &Camera=m_pSystem->GetViewCamera();
	return pH->EndFunction( Camera.GetFov() );
}


int CScriptBind_System::IsPointVisible(IFunctionHandler *pH, Vec3 point)
{
	CCamera &Camera=m_pSystem->GetViewCamera();
	return pH->EndFunction( Camera.IsPointVisible(point) );
}

int CScriptBind_System::GetViewCameraPos(IFunctionHandler *pH)
{
	//SCRIPT_CHECK_PARAMETERS(0);
	CCamera &Camera=m_pSystem->GetViewCamera();
	return pH->EndFunction( Script::SetCachedVector( Camera.GetPosition(), pH, 1 ) );
}

int CScriptBind_System::GetViewCameraDir(IFunctionHandler *pH)
{
	//SCRIPT_CHECK_PARAMETERS(0);
	CCamera &Camera=m_pSystem->GetViewCamera();
	Matrix34 cameraMatrix = Camera.GetMatrix();

	return pH->EndFunction( Script::SetCachedVector( cameraMatrix.GetColumn(1), pH, 1 ) );
}

int CScriptBind_System::GetViewCameraAngles(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);
	CCamera &Camera=m_pSystem->GetViewCamera();

	return pH->EndFunction(Vec3(RAD2DEG(Ang3::GetAnglesXYZ(Matrix33(Camera.GetMatrix())))));
}

int CScriptBind_System::RayWorldIntersection(IFunctionHandler *pH)
{
	assert(pH->GetParamCount()>=3 && pH->GetParamCount()<=6);

	Vec3 vPos(0,0,0);
	Vec3 vDir(0,0,0);

	int nMaxHits,iEntTypes=ent_all;

	pH->GetParam(1, vPos);
	pH->GetParam(2, vDir);
	pH->GetParam(3, nMaxHits);
	pH->GetParam(4, iEntTypes);

	if (nMaxHits>10)
		nMaxHits=10;

	ray_hit RayHit[10];

	//filippo: added support for skip certain entities.
	int skipId1 = -1;
	int skipId2 = -1;
	int skipIdCount = 0;

	IPhysicalEntity	*skipPhys[2] = {0,0};

	pH->GetParam(5, skipId1);
	pH->GetParam(6, skipId2);

	if (skipId1!=-1)
	{
		++skipIdCount;
		IEntity	*skipEnt1 = m_pSystem->GetIEntitySystem()->GetEntity(skipId1);
		if (skipEnt1)
			skipPhys[0] = skipEnt1->GetPhysics();
	}

	if (skipId2!=-1)
	{
		++skipIdCount;
		IEntity	*skipEnt2 = m_pSystem->GetIEntitySystem()->GetEntity(skipId2);
		if (skipEnt2)
			skipPhys[1] = skipEnt2->GetPhysics();
	}

	Vec3 src = vPos;
	Vec3 dst = vDir-src;

	int nHits= m_pSystem->GetIPhysicalWorld()->RayWorldIntersection(src, dst, iEntTypes,
		geom_colltype0<<rwi_colltype_bit | rwi_stop_at_pierceable, RayHit, nMaxHits,skipPhys,skipIdCount);

	SmartScriptTable pObj(m_pSS);

	for (int i=0;i<nHits;i++)
	{
		SmartScriptTable pHitObj(m_pSS);
		ray_hit &Hit=RayHit[i];
		pHitObj->SetValue("pos",Hit.pt);
		pHitObj->SetValue("normal",Hit.n);
		pHitObj->SetValue("dist", Hit.dist);
		pHitObj->SetValue("surface", Hit.surface_idx);
		pObj->SetAt(i+1, pHitObj);
	}

	return pH->EndFunction(*pObj);
}


//-------------------------------------------------------------------------------------------------
//
int CScriptBind_System::RayTraceCheck(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(4);
	Vec3 src, dst;
	int	skipId1, skipId2;

	pH->GetParam(1, src);
	pH->GetParam(2, dst);
	pH->GetParam(3, skipId1);
	pH->GetParam(4, skipId2);

	IEntity	*skipEnt1 = m_pSystem->GetIEntitySystem()->GetEntity(skipId1);
	IEntity	*skipEnt2 = m_pSystem->GetIEntitySystem()->GetEntity(skipId2);
	IPhysicalEntity	*skipPhys[2] = {0,0};

	if(skipEnt1) skipPhys[0] = skipEnt1->GetPhysics();
	if(skipEnt2) skipPhys[1] = skipEnt2->GetPhysics();

	ray_hit RayHit;
	//TODO? add an extraparam to specify what kind of objects to check? now its world and static
	int nHits = m_pSystem->GetIPhysicalWorld()->RayWorldIntersection(src, dst-src, ent_static|ent_terrain,	rwi_ignore_noncolliding |  rwi_stop_at_pierceable, &RayHit, 1, skipPhys,2 );

	return pH->EndFunction((bool)(nHits==0));
}
//////////////////////////////////////////////////////////////////////////

int CScriptBind_System::GetCPUQuality( IFunctionHandler* pH )
{
	SCRIPT_CHECK_PARAMETERS( 0 );

	// return cpu quality -- that is, clock speed of CPU in MHz
	float fMHz( (float) ( 1e-6 / m_pSystem->GetSecondsPerCycle() ) );

	//@TODO: Fix and uncomment this later.
	/*
	if( false != IsAMD64() )
	{
		fMHz *= 1.5f; // AMD64 with 2 GHz is equal to a 3.0 GHz machine
		fMHz = ( fMHz < 3000.0f ) ? 3000.0f : fMHz;
	}
	*/

	int iSysSpec( 0 );
	if( fMHz < 1600.0f )
	{
		// < 1.7 GHz (1.9 GHz = safety net) is minimum spec
		iSysSpec = 0;
	}
	else if( fMHz < 2500.0f )
	{
		// < 2.6 GHz (2.5 GHz = safety net) is medium spec
		iSysSpec = 1;
	}
	else if( fMHz < 2900.0f )
	{
		// < 3.0 GHz (2.9 GHz = safety net) is high spec
		iSysSpec = 2;
	}
	else
	{
		// else very high spec
		iSysSpec = 3;
	}

	return( pH->EndFunction( iSysSpec ) );
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::GetGPUQuality( IFunctionHandler* pH )
{
	/*
	SCRIPT_CHECK_PARAMETERS( 0 );
	static int s_iGPUQuality( -1 );
#if !defined(LINUX)
	if( -1 == s_iGPUQuality )
	{
		HMODULE hDDraw( LoadLibrary( "ddraw.dll" ) );
		if( 0 != hDDraw )
		{
			s_iGPUQuality = 666; // Unknown card

			typedef HRESULT (WINAPI *FP_DirectDrawCreateEx) ( GUID* lpGUID, void* lplpDD, REFIID iid, IUnknown* pUnkOuter );
			FP_DirectDrawCreateEx fpDirectDrawCreateEx( (FP_DirectDrawCreateEx) GetProcAddress( hDDraw, "DirectDrawCreateEx" ) );

			LPDIRECTDRAW7 pDD7( 0 );
			if( SUCCEEDED( fpDirectDrawCreateEx( 0, &pDD7, IID_IDirectDraw7, 0 ) ) )
			{
				DDDEVICEIDENTIFIER2 DeviceIdentifier;
				memset( &DeviceIdentifier, 0, sizeof( DeviceIdentifier ) );
				if( SUCCEEDED( pDD7->GetDeviceIdentifier( &DeviceIdentifier, 0 ) ) )
				{
					switch( DeviceIdentifier.dwVendorId )
					{
					case 0x1002: // ATI
						{
							switch( DeviceIdentifier.dwDeviceId )
							{
								case 0x4c64: s_iGPUQuality = 0; break; // Radeon Mobility 9000
								case 0x4c66: s_iGPUQuality = 0; break; // Radeon Mobility 9000
								case 0x4966: s_iGPUQuality = 0; break; // Radeon 9000
								case 0x496e: s_iGPUQuality = 0; break; // Radeon 9000 - Secondary
								case 0x514d: s_iGPUQuality = 0; break; // Radeon 9100
								case 0x5834: s_iGPUQuality = 0; break; // Radeon 9100 IGP
								case 0x4242: s_iGPUQuality = 0; break; // Radeon 8500 DV
								case 0x4152: s_iGPUQuality = 1; break; // Radeon 9600
								case 0x4172: s_iGPUQuality = 1; break; // Radeon 9600 - Secondary
								case 0x4164: s_iGPUQuality = 1; break; // Radeon 9500 - Secondary
								case 0x4144: s_iGPUQuality = 2; break; // Radeon 9500
								case 0x4e45: s_iGPUQuality = 2; break; // Radeon 9500 Pro / 9700
								case 0x4150: s_iGPUQuality = 1; break; // Radeon 9600 Pro
								case 0x4151: s_iGPUQuality = 1; break; // Radeon 9600
								case 0x4170: s_iGPUQuality = 1; break; // Radeon 9600 Pro - Secondary
								case 0x4171: s_iGPUQuality = 1; break; // Radeon 9600 - Secondary
								case 0x4e46: s_iGPUQuality = 1; break; // Radeon 9600 TX
								case 0x4e66: s_iGPUQuality = 1; break; // Radeon 9600 TX - Secondary
								case 0x4e44: s_iGPUQuality = 3; break; // Radeon 9700 Pro
								case 0x4e64: s_iGPUQuality = 3; break; // Radeon 9700 Pro - Secondary
								case 0x4e65: s_iGPUQuality = 2; break; // Radeon 9500 Pro / 9700 - Secondary
								case 0x4e49: s_iGPUQuality = 3; break; // Radeon 9800
								case 0x4e69: s_iGPUQuality = 3; break; // Radeon 9800 - Secondary
								case 0x4148: s_iGPUQuality = 3; break; // Radeon 9800
								case 0x4168: s_iGPUQuality = 3; break; // Radeon 9800 - Secondary
								case 0x4e48: s_iGPUQuality = 3; break; // Radeon 9800 Pro
								case 0x4e68: s_iGPUQuality = 3; break; // Radeon 9800 Pro - Secondary
								case 0x4e4a: s_iGPUQuality = 3; break; // Radeon 9800 XT
								case 0x4e6a: s_iGPUQuality = 3; break; // Radeon 9800 XT - Secondary
								case 0x5960: s_iGPUQuality = 1; break; // Radeon 9200 Pro
								case 0x5940: s_iGPUQuality = 1; break; // Radeon 9200 Pro - Secondary
								case 0x5961: s_iGPUQuality = 0; break; // Radeon 9200
								case 0x5941: s_iGPUQuality = 1; break; // Radeon 9200 - Secondary
								case 0x5964: s_iGPUQuality = 1; break; // Radeon 9200SE
								case 0x514c: s_iGPUQuality = 0; break; // Radeon 8500
								case 0x514e: s_iGPUQuality = 0; break; // Radeon 8500
								case 0x514f: s_iGPUQuality = 0; break; // Radeon 8500
				        case 0x4136: s_iGPUQuality = 0; break; // IGP 320
				        case 0x4137: s_iGPUQuality = 0; break; // IGP 340
				        case 0x4a49: s_iGPUQuality = 3; break; // Radeon X800 Pro
				        case 0x4a4a: s_iGPUQuality = 3; break; // Radeon X800 SE
				        case 0x4a4b: s_iGPUQuality = 3; break; // Radeon X800
				        case 0x4a4c: s_iGPUQuality = 3; break; // Radeon X800 Series
				        case 0x4a50: s_iGPUQuality = 3; break; // Radeon X800 XT
				        case 0x4a69: s_iGPUQuality = 3; break; // Radeon X800 Pro Secondary
				        case 0x4a6a: s_iGPUQuality = 3; break; // Radeon X800 SE Secondary
				        case 0x4a6b: s_iGPUQuality = 3; break; // Radeon X800 Secondary
				        case 0x4a6c: s_iGPUQuality = 3; break; // Radeon X800 Series Secondary
				        case 0x4a70: s_iGPUQuality = 3; break; // Radeon X800 XT Secondary
							}
						}

					case 0x10b4: // NVIDIA
					case 0x12d2:
					case 0x10de:
						{
							switch( DeviceIdentifier.dwDeviceId )
							{
								case 0x0152: s_iGPUQuality = 0; break; // GeForce2 Ultra
								case 0x0153: s_iGPUQuality = 0; break; // Quadro2 Pro
								case 0x0170: s_iGPUQuality = 0; break; // GeForce4 MX 460
								case 0x0171: s_iGPUQuality = 0; break; // GeForce4 MX 440
								case 0x0172: s_iGPUQuality = 0; break; // GeForce4 MX 420
								case 0x0173: s_iGPUQuality = 0; break; // GeForce4 MX 440-SE
								case 0x0174: s_iGPUQuality = 0; break; // GeForce4 Go 440
								case 0x0175: s_iGPUQuality = 0; break; // GeForce4 Go 420
								case 0x0176: s_iGPUQuality = 0; break; // GeForce4 Go 420
								case 0x0178: s_iGPUQuality = 0; break; // Quadro4 550 XGL
								case 0x0179: s_iGPUQuality = 0; break; // GeForce4 Go 440
								case 0x017a: s_iGPUQuality = 0; break; // Quadro NVS
								case 0x017b: s_iGPUQuality = 0; break; // Quadro 550 XGL
								case 0x0181: s_iGPUQuality = 0; break; // GeForce4 MX 440 with AGP8X
								case 0x0182: s_iGPUQuality = 0; break; // GeForce4 MX 440SE with AGP8X
								case 0x0183: s_iGPUQuality = 0; break; // GeForce4 MX 420 with AGP8X
								case 0x0188: s_iGPUQuality = 0; break; // Quadro4 580 XGL
								case 0x018a: s_iGPUQuality = 0; break; // Quadro NVS with AGP8X
								case 0x018b: s_iGPUQuality = 0; break; // Quadro4 380 XGL
								case 0x01f0: s_iGPUQuality = 0; break; // GeForce4 MX Integrated GPU (nForce2)
								case 0x0200: s_iGPUQuality = 0; break; // GeForce3
								case 0x0201: s_iGPUQuality = 0; break; // GeForce3 Ti200
								case 0x0202: s_iGPUQuality = 0; break; // GeForce3 Ti500
								case 0x0203: s_iGPUQuality = 0; break; // Quadro DCC
								case 0x0250: s_iGPUQuality = 1; break; // GeForce4 Ti 4600
								case 0x0251: s_iGPUQuality = 1; break; // GeForce4 Ti 4400
								case 0x0253: s_iGPUQuality = 0; break; // GeForce4 Ti 4200
								case 0x0258: s_iGPUQuality = 0; break; // Quadro4 900 XGL
								case 0x0259: s_iGPUQuality = 0; break; // Quadro4 750 XGL
								case 0x025b: s_iGPUQuality = 0; break; // Quadro4 700 XGL
								case 0x0280: s_iGPUQuality = 1; break; // GeForce4 Ti 4800
								case 0x0281: s_iGPUQuality = 0; break; // GeForce4 Ti4200 with AGP8X
								case 0x0282: s_iGPUQuality = 0; break; // GeForce4 Ti 4800 SE
								case 0x0288: s_iGPUQuality = 0; break; // Quadro4 980 XGL
								case 0x0289: s_iGPUQuality = 0; break; // Quadro4 780 XGL
								case 0x02a0: s_iGPUQuality = 0; break; // GeForce3 XBOX
								case 0x0301: s_iGPUQuality = 1; break; // GeForce FX 5800 Ultra
								case 0x0302: s_iGPUQuality = 1; break; // GeForce FX 5800
								case 0x0308: s_iGPUQuality = 1; break; // Quadro FX 2000
								case 0x0309: s_iGPUQuality = 1; break; // Quadro FX 1000
								case 0x030a: s_iGPUQuality = 1; break; // ICE FX 2000
								case 0x0311: s_iGPUQuality = 1; break; // GeForce FX 5600 Ultra
								case 0x0312: s_iGPUQuality = 1; break; // GeForce FX 5600
								case 0x0313: s_iGPUQuality = 1; break; // NV31
								case 0x0314: s_iGPUQuality = 1; break; // GeForce FX 5600XT
								case 0x031a: s_iGPUQuality = 1; break; // GeForce FX Go5600
								case 0x0321: s_iGPUQuality = 1; break; // GeForce FX 5200 Ultra
								case 0x0322: s_iGPUQuality = 1; break; // GeForce FX 5200
								case 0x0323: s_iGPUQuality = 1; break; // GeForce FX 5200SE
								case 0x032b: s_iGPUQuality = 1; break; // Quadro FX 500
								case 0x032f: s_iGPUQuality = 1; break; // NV34GL
								case 0x0330: s_iGPUQuality = 3; break; // GeForce FX 5900 Ultra
								case 0x0331: s_iGPUQuality = 2; break; // GeForce FX 5900
								case 0x0332: s_iGPUQuality = 2; break; // NV35
								case 0x0333: s_iGPUQuality = 3; break; // GeForce FX 5950 Ultra
								case 0x0338: s_iGPUQuality = 2; break; // Quadro FX 3000
								case 0x0341: s_iGPUQuality = 2; break; // GeForce FX 5700 Ultra
								case 0x0342: s_iGPUQuality = 2; break; // GeForce FX 5700
								case 0x034e: s_iGPUQuality = 1; break; // Quadro FX 1100
								case 0x0040:													 // GeForce 6800 Ultra
								case 0x0041:													 // GeForce 6800
								case 0x0042:
								case 0x0043:
								case 0x0044:
								case 0x0045:
								case 0x0046:
								case 0x0047:
								case 0x0048:
								case 0x0049:													 // NV40GL
								case 0x004a:
								case 0x004b:
								case 0x004c:
								case 0x004d:
								case 0x004e:													 // NV40GL
								case 0x004f: s_iGPUQuality = 3; break; // NV40 (GeForce ???)
								case 0x00f9: s_iGPUQuality = 3; break; // GeForce 6800 Ultra
								case 0x00fa: s_iGPUQuality = 2; break; // GeForce PCX 5750
								case 0x00fb: s_iGPUQuality = 2; break; // GeForce PCX 5900
								case 0x00fc: s_iGPUQuality = 1; break; // GeForce PCX 5300
								case 0x00fd: s_iGPUQuality = 2; break; // Quadro PCI-E Series
								case 0x00fe: s_iGPUQuality = 2; break; // Quadro FX 1300
								case 0x00ff: s_iGPUQuality = 1; break; // GeForce PCX 4300
							}
						}
					}
				}
				pDD7->Release();
			}

			FreeLibrary( hDDraw );
		}
	}

  // Hack: No gpu detected ? Could be newer gpu, check for pixel shaders support
  if(s_iGPUQuality==666)
  {
    if(m_pRenderer->GetFeatures() & RFT_HW_PS20)
    {
      s_iGPUQuality=2;
    }
    else
    if(m_pRenderer->GetFeatures() & RFT_HW_TS)
    {
      s_iGPUQuality=1;
    }
    else
    {
      s_iGPUQuality=0;
    }
  }  

#endif
	// return gpu quality level in range [0..3]
	return( pH->EndFunction( s_iGPUQuality ) );
	*/
	return( pH->EndFunction( 0 ) );
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::GetVideoMem( IFunctionHandler* pH )
{
	/*
	SCRIPT_CHECK_PARAMETERS( 0 );
	static DWORD s_dwTotalVideoMemory( 0xFFFFFFFF );
#if !defined(LINUX)
	if( 0xFFFFFFFF == s_dwTotalVideoMemory )
	{
		s_dwTotalVideoMemory = 0;

		HMODULE hDDraw( LoadLibrary( "ddraw.dll" ) );
		if( 0 != hDDraw )
		{
			typedef HRESULT (WINAPI *FP_DirectDrawCreateEx) ( GUID* lpGUID, void* lplpDD, REFIID iid, IUnknown* pUnkOuter );
			FP_DirectDrawCreateEx fpDirectDrawCreateEx( (FP_DirectDrawCreateEx) GetProcAddress( hDDraw, "DirectDrawCreateEx" ) );

			LPDIRECTDRAW7 pDD7( 0 );
			if( SUCCEEDED( fpDirectDrawCreateEx( 0, &pDD7, IID_IDirectDraw7, 0 ) ) )
			{
				DDSCAPS2 sDDSCaps2;
				ZeroMemory( &sDDSCaps2, sizeof( sDDSCaps2 ) );
				sDDSCaps2.dwCaps = DDSCAPS_LOCALVIDMEM;

				HRESULT hr( pDD7->GetAvailableVidMem( &sDDSCaps2, &s_dwTotalVideoMemory, 0 ) );
				assert( S_OK == hr );

				pDD7->Release();
			}

			FreeLibrary( hDDraw );
		}
	}
#endif
	// return size of available video memory in MB
	return( pH->EndFunction( (int) ( s_dwTotalVideoMemory >> 20 ) ) );
	*/
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::IsPS20Supported( IFunctionHandler* pH )
{
	SCRIPT_CHECK_PARAMETERS( 0 );
	bool bPS20( 0 != ( m_pSystem->GetIRenderer()->GetFeatures() & RFT_HW_PS20 ) );
	return( pH->EndFunction( false != bPS20 ? 1 : 0 ) );
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::IsHDRSupported( IFunctionHandler* pH )
{
	SCRIPT_CHECK_PARAMETERS(0);

	if (m_pSystem->GetIRenderer()->GetFeatures() & RFT_HW_HDR)
	{
		return pH->EndFunction((int)1);
	}
	else
	{
		return pH->EndFunction();
	}
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::SetBudget( IFunctionHandler* pH )
{
	SCRIPT_CHECK_PARAMETERS( 6 );

	int sysMemLimitInMB( 512 );
	pH->GetParam( 1, sysMemLimitInMB );

	int videoMemLimitInMB( 256 );
	pH->GetParam( 2, videoMemLimitInMB );

	float frameTimeLimitInMS( 50.0f );
	pH->GetParam( 3, frameTimeLimitInMS );

	int soundChannelsPlayingLimit( 64 );
	pH->GetParam( 4, soundChannelsPlayingLimit );

	int soundMemLimitInMB( 64 );
	pH->GetParam( 5, soundMemLimitInMB );

	int numDrawCallsLimit( 2000 );
	pH->GetParam( 6, numDrawCallsLimit );

	GetISystem()->GetIBudgetingSystem()->SetBudget( sysMemLimitInMB, videoMemLimitInMB, frameTimeLimitInMS, soundChannelsPlayingLimit, soundMemLimitInMB, numDrawCallsLimit );

	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::SetVolumetricFogModifiers(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);

	float gobalDensityModifier(0);
	pH->GetParam( 1, gobalDensityModifier );
	
	float atmosphereHeightModifier(0);
	pH->GetParam( 2, atmosphereHeightModifier );

	gEnv->p3DEngine->SetVolumetricFogModifiers(gobalDensityModifier, atmosphereHeightModifier);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_System::SetWind( IFunctionHandler *pH )
{
	SCRIPT_CHECK_PARAMETERS( 1 );
	Vec3 vWind( 0, 0, 0 );
	pH->GetParam( 1, vWind );
	m_p3DEngine->SetWind( vWind );
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_System::GetWind( IFunctionHandler *pH )
{
	return pH->EndFunction( m_p3DEngine->GetWind( AABB(ZERO), false ) );
}

//------------------------------------------------------------------------
int CScriptBind_System::GetSurfaceTypeIdByName( IFunctionHandler *pH, const char* surfaceName)
{
	ISurfaceType *pSurface = m_p3DEngine->GetMaterialManager()->GetSurfaceTypeByName(surfaceName);

	if (pSurface)
		return pH->EndFunction(pSurface->GetId());

	return pH->EndFunction();
}

int CScriptBind_System::GetSurfaceTypeNameById( IFunctionHandler *pH, int surfaceId )
{
	ISurfaceType *pSurface = m_p3DEngine->GetMaterialManager()->GetSurfaceType(surfaceId);

	if (pSurface)
		return pH->EndFunction(pSurface->GetName());

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_System::RemoveEntity(IFunctionHandler *pH, ScriptHandle entityId)
{
	m_pSystem->GetIEntitySystem()->RemoveEntity(entityId.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_System::SpawnEntity(IFunctionHandler *pH, SmartScriptTable params)
{
	const char				*entityClass = 0;
	const char				*entityName = "";
	const char				*archetypeName = 0;
	ScriptHandle			entityId;
	SmartScriptTable	propsTable(m_pSS, true);
	SmartScriptTable  propsInstanceTable(m_pSS, true);

	Vec3 pos(0.0f, 0.0f, 0.0f);
	Vec3 dir(1.0f, 0.0f, 0.0f);
	Vec3 scale(1.0f, 1.0f, 1.0f);
	int flags = 0;
	bool props, propsInstance;

	{
		CScriptSetGetChain chain(params);

		chain.GetValue("id", entityId);
		chain.GetValue("class", entityClass);
		chain.GetValue("name", entityName);
		chain.GetValue("position", pos);
		chain.GetValue("orientation", dir);		//orientation unit vector
		chain.GetValue("scale", scale);
		chain.GetValue("flags", flags);
		chain.GetValue("archetype",archetypeName);
		props = chain.GetValue("properties", propsTable);
		propsInstance = chain.GetValue("propertiesInstance", propsInstanceTable);
	}

	ScriptHandle hdl;
	IEntity	*pProtoEntity(NULL);
	
	if(pH->GetParamCount()>1 && pH->GetParam(2,hdl))
	{
		int nID = hdl.n;
		pProtoEntity = m_pSystem->GetIEntitySystem()->GetEntity(nID);
	}
  
	if (!entityClass)
	{
		return pH->EndFunction();
	}

	if(dir.IsZero(.1f))
	{
		dir = Vec3(1.0f, 0.0f, 0.0f);
		m_pLog->Log("Error: zero orientation CScriptBind_System::SpawnEntity. Entity name %s", entityName);
	}
	else dir.NormalizeSafe();

	SEntitySpawnParams spawnParams;
	spawnParams.id = entityId.n;
	spawnParams.qRotation = Quat(Matrix33::CreateRotationVDir(dir));
	spawnParams.vPosition = pos;
	spawnParams.vScale = scale;
	spawnParams.sName = entityName;
	spawnParams.pUserData = 0;
	spawnParams.pClass = m_pSystem->GetIEntitySystem()->GetClassRegistry()->FindClass(entityClass);
	
	if (archetypeName)
	{
		spawnParams.pArchetype = m_pSystem->GetIEntitySystem()->LoadEntityArchetype( archetypeName );
	}

	if (!spawnParams.pClass)
	{
		m_pLog->Log("Error: no such entity class %s (entity name: %s)", entityClass, entityName);
		return pH->EndFunction();
	}
	// if there is a prototype - use some flags of prototype entity
	spawnParams.nFlags = flags |
			(pProtoEntity ? 
				pProtoEntity->GetFlags() & (ENTITY_FLAG_CASTSHADOW | ENTITY_FLAG_GOOD_OCCLUDER | ENTITY_FLAG_RECVWIND | ENTITY_FLAG_OUTDOORONLY) 
				: 0);

	IEntity				*pEntity = m_pSystem->GetIEntitySystem()->SpawnEntity(spawnParams, !props);
	IScriptTable	*pEntityTable = pEntity->GetScriptTable();

	if (props)
	{
		if (pEntityTable)
		{
			SmartScriptTable entityProps(m_pSS, false);

			if (pEntityTable->GetValue("Properties", entityProps))
			{
				MergeTable(entityProps, propsTable);
			}
			if (propsInstance && pEntityTable->GetValue("PropertiesInstance", entityProps))
			{
				MergeTable(entityProps, propsInstanceTable);
			}
		}

		m_pSystem->GetIEntitySystem()->InitEntity(pEntity, spawnParams);
	}

	if (pEntity && pEntityTable)
	{
		return pH->EndFunction(pEntityTable);
	}

	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::IsDevModeEnable(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS( 0 );

	// check if we're running in devmode (cheat mode)
	// to check if we are allowed to enable certain scripts
	// function facilities (god mode, fly mode etc.)
	bool bDevMode = m_pSystem->IsDevMode();

	return pH->EndFunction( bDevMode );
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::SaveConfiguration(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS( 0 );

	m_pSystem->SaveConfiguration();

	return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_System::SetSystemShaderRenderFlags(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	const char *pszShaderName=NULL;
	const char *pszFlagName=NULL;
	bool bEnable = false;
	pH->GetParam(1, pszShaderName);
	pH->GetParam(2, pszFlagName);
	pH->GetParam(3, bEnable);

	typedef struct
	{
		int dwFlagValue;
		char * szFlagName;
	} flagsDesc;

	static flagsDesc arrFlags[] =
	{
		{DLD_TERRAIN_WATER, "DrawWater"},
		{DLD_TERRAIN, "DrawTerrain"},
		{DLD_DETAIL_TEXTURES, "DrawDetailTextures"},
		{DLD_DETAIL_OBJECTS, "DrawDetailObjects"},
		{DLD_FAR_SPRITES, "DrawFarSprites"},
		{DLD_STATIC_OBJECTS, "DrawStaticObjects"},
		{DLD_ENTITIES, "DrawEntities"},
		{DLD_PARTICLES, "DrawParticles"},
		{DLD_TERRAIN_LIGHT, "UseLights"},
		{0,0}
	};

	int dwFlag = 0;
	for(int i=0; arrFlags[i].dwFlagValue; i++)
		if(stricmp(arrFlags[i].szFlagName, pszFlagName)==0)
			dwFlag |= arrFlags[i].dwFlagValue;

	IShader * pShader = m_pRenderer->EF_LoadShader(pszShaderName,EF_SYSTEM);
	/*if(bEnable)
		pShader->SetRenderFlags(pShader->GetRenderFlags() | dwFlag);
	else
		pShader->SetRenderFlags(pShader->GetRenderFlags() & ~dwFlag);
  */

	return pH->EndFunction();
}

//-------------------------------------------------------------------------------------------------
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ShellAPI.h>
#endif
int CScriptBind_System::BrowseURL(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	const char *szURL;
	pH->GetParam(1, szURL);

	// for security reasons, check if it really a url
	if (strlen(szURL) >= 10)
	{
		// check for http and : as in http://
		// : might be on position 5, for https://

		if (!strncmp("http", szURL, 4) && ((szURL[4] == ':') || (szURL[5] == ':')))
		{

#ifdef WIN32
			ShellExecute(0, "open", szURL, 0, 0, SW_MAXIMIZE);
#else

#pragma message("WARNING: CScriptBind_System::BrowseURL() not implemented on this platform!")

#endif
		}
	}

	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::GetSystemMem( IFunctionHandler* pH )
{
	SCRIPT_CHECK_PARAMETERS( 0 );

	int iSysMemInMB = 0;
#ifdef WIN32
	MEMORYSTATUS sMemStat;
	GlobalMemoryStatus( &sMemStat );
	// return size of total physical memory in MB
	iSysMemInMB = sMemStat.dwTotalPhys >> 20;
#endif

	return( pH->EndFunction( iSysMemInMB ) );
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::Quit(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	m_pSystem->Quit();

	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::ClearKeyState(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	m_pSystem->GetIInput()->ClearKeyState();

	return pH->EndFunction();
}

//------------------------------------------------------------------------
void CScriptBind_System::MergeTable(IScriptTable *pDest, IScriptTable *pSrc)
{
	IScriptTable::Iterator it = pSrc->BeginIteration();

	while (pSrc->MoveNext(it))
	{
		if (pSrc->GetAtType(it.nKey) != svtNull)
		{
			if (pSrc->GetAtType(it.nKey) == svtObject)
			{
				SmartScriptTable tbl;

				if (pDest->GetAtType(it.nKey) != svtObject)
				{
					tbl = SmartScriptTable(m_pSS->CreateTable());
					pDest->SetAtAny(it.nKey, tbl);
				}
				else
				{
					tbl = SmartScriptTable(m_pSS, true);
					pDest->GetAt(it.nKey, tbl);
				}

				SmartScriptTable srcTbl;
				it.value.CopyTo(srcTbl);
				MergeTable(tbl, srcTbl);
			}
			else
			{
				pDest->SetAtAny(it.nKey, it.value);
			}
		}
		else if (pSrc->GetValueType(it.sKey) != svtNull)
		{
			if (pSrc->GetValueType(it.sKey) == svtObject)
			{
				SmartScriptTable tbl;

				if (pDest->GetValueType(it.sKey) != svtObject)
				{
					tbl = SmartScriptTable(m_pSS->CreateTable());
					pDest->SetValue(it.sKey, tbl);
				}
				else
				{
					tbl = SmartScriptTable(m_pSS, true);
					pDest->GetValue(it.sKey, tbl);
				}
				
				SmartScriptTable srcTbl;
				it.value.CopyTo(srcTbl);
				MergeTable(tbl, srcTbl);
			}
			else
			{
				pDest->SetValueAny(it.sKey, it.value);
			}
		}
	}

	pSrc->EndIteration(it);
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_System::LoadLocalizationXml( IFunctionHandler *pH,const char *filename )
{
	m_pSystem->GetLocalizationManager()->LoadExcelXmlSpreadsheet( filename );
	return pH->EndFunction();
}



//------------------------------------------------------------------------
int CScriptBind_System::GetHDRDynamicMultiplier( IFunctionHandler *pH )
{
	SCRIPT_CHECK_PARAMETERS( 0 );

	return pH->EndFunction( m_p3DEngine->GetHDRDynamicMultiplier() );
}

//------------------------------------------------------------------------
int CScriptBind_System::SetHDRDynamicMultiplier( IFunctionHandler *pH )
{
	SCRIPT_CHECK_PARAMETERS( 1 );

	float fMul( 1.0f );
	pH->GetParam(1,fMul);

	m_p3DEngine->SetHDRDynamicMultiplier(fMul);
	return pH->EndFunction();
}
//------------------------------------------------------------------------
int CScriptBind_System::GetFrameID( IFunctionHandler *pH )
{
	SCRIPT_CHECK_PARAMETERS( 0 );

	return pH->EndFunction(m_pRenderer->GetFrameID());
}
