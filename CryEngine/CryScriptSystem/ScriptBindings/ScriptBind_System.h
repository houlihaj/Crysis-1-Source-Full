////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   ScriptBind_System.h
//  Version:     v1.00
//  Created:     8/7/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ScriptBind_System_h__
#define __ScriptBind_System_h__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <IScriptSystem.h>

struct ISystem;
struct ILog;
struct IRenderer;
struct IConsole;
struct IInput;
struct ITimer;
struct IEntitySystem;
struct I3DEngine;
struct IPhysicalWorld;
struct ICVar;

/*! 
	<title System>
	Syntax: System

	This class implements script-functions for exposing the System functionalities

	REMARKS:
	this object doesn't have a global mapping(is not present as global variable into the script state)
		
	IMPLEMENTATIONS NOTES:
	These function will never be called from C-Code. They're script-exclusive.
*/

class CScriptBind_System : public CScriptableBase
{ 
public:
	CScriptBind_System(IScriptSystem *pScriptSystem,ISystem *pSystem);
	virtual ~CScriptBind_System();
	virtual void GetMemoryStatistics(ICrySizer *pSizer) { pSizer->Add(this); };
public:
	int CreateDownload(IFunctionHandler *pH);
	int LoadFont(IFunctionHandler *pH); //string (return void)
	int ExecuteCommand(IFunctionHandler *pH); //string (return void)
	int LogToConsole(IFunctionHandler *pH); //string (return void)
	int ClearConsole(IFunctionHandler *pH); // void (return void)
	int Log(IFunctionHandler *pH); // void (string))
	int LogAlways(IFunctionHandler *pH); // void (string))
	int Warning(IFunctionHandler *pH); //string (return void)
	int Error(IFunctionHandler *pH); //string (return void)
	int IsEditor(IFunctionHandler *pH);
	int GetCurrTime(IFunctionHandler *pH); //void (return float)
	int GetCurrAsyncTime(IFunctionHandler *pH); //void (return float)
	int GetFrameTime(IFunctionHandler *pH); //void (return float)
	int GetLocalOSTime(IFunctionHandler *pH); //void (return float)
	int ShowConsole(IFunctionHandler *pH); //int (return void)

	int CheckHeapValid(IFunctionHandler *pH); //int (return void)

	int GetConfigSpec(IFunctionHandler *pH);
	int IsMultiplayer(IFunctionHandler *pH);
	
	int GetEntity(IFunctionHandler *pH); //int (return obj)
	int GetEntityClass(IFunctionHandler *pH); //int (return string)

	int GetEntities(IFunctionHandler *pH); //int (return obj[])
	int GetEntitiesByClass(IFunctionHandler *pH, const char* EntityClass);
	int GetEntitiesInSphere(IFunctionHandler *pH, Vec3 center, float radius);
	int GetEntitiesInSphereByClass(IFunctionHandler *pH, Vec3 center, float radius, const char* EntityClass);
	int GetPhysicalEntitiesInBox(IFunctionHandler *pH, Vec3 center, float radius); // table of entities
	int GetPhysicalEntitiesInBoxByClass(IFunctionHandler *pH, Vec3 center, float radius, const char *className); // table of entities
	int GetNearestEntityByClass(IFunctionHandler *pH, Vec3 center, float radius, const char *className); // entity
	
	// <title GetEntityByName>
	// Syntax: System.GetEntityByName( const char *sEntityName )
	// Description:
	//    Retrieve entity table for the first entity with specified name.
	//    If multiple entities with same name exist, first one found is returned.
	// Arguments:
	//    sEntityName - Name of the entity to search.
	int GetEntityByName(IFunctionHandler *pH,const char *sEntityName);

	// <title GetEntityIdByName>
	// Syntax: System.GetEntityIdByName( const char *sEntityName )
	// Description:
	//    Retrieve entity Id for the first entity with specified name.
	//    If multiple entities with same name exist, first one found is returned.
	// Arguments:
	//    sEntityName - Name of the entity to search.
	int GetEntityIdByName(IFunctionHandler *pH,const char *sEntityName);

	int DrawLabelImage(IFunctionHandler *pH); //Vec3, float,int (return void)
	int DrawLabel(IFunctionHandler *pH); //Vec3, float,char* (return void)
	int DeformTerrain(IFunctionHandler *pH);
	int DeformTerrainUsingMat(IFunctionHandler *pH);
	int ApplyForceToEnvironment(IFunctionHandler *pH);//Vec3 pos,float radius,float force

	int ScreenToTexture(IFunctionHandler *pH); //void(return void)
	int DrawTriStrip(IFunctionHandler *pH);
	int DrawLine(IFunctionHandler *pH);
	int Draw2DLine(IFunctionHandler *pH);
	int DrawText(IFunctionHandler *pH);
	int DrawImage(IFunctionHandler *pH); //int,int,int,int,int,int (return void)
	int DrawImageColor(IFunctionHandler *pH); //int,int,int,int,int,int,float,float,float,float (return void)
	int DrawImageColorCoords(IFunctionHandler *pH); //int,int,int,int,int,int,float,float,float,float, float, float, float, float (return void)
	int DrawImageCoords(IFunctionHandler *pH); //int,int,int,int,int,int,float,float,float,float, float, float, float, float (return void)
  int SetGammaDelta(IFunctionHandler *pH);	//float
	int DrawRectShader(IFunctionHandler *pH);	//const char*,int,int,int,int
  int SetScreenShader(IFunctionHandler *pH);//const char*
    
  int SetPostProcessFxParam(IFunctionHandler *pH);//const char*, const char*, float  
	int GetPostProcessFxParam(IFunctionHandler *pH);//const char*, const char*    

  int SetScreenFx(IFunctionHandler *pH);//const char*, const char*, float  
  int GetScreenFx(IFunctionHandler *pH);//const char*, const char*    

	int SetCVar(IFunctionHandler *pH);
	int GetCVar(IFunctionHandler *pH);
	int AddCCommand(IFunctionHandler *pH);	//const char *, const char *, const char * (return void)

  int SetScissor(IFunctionHandler *pH);//int, int, int, int

	// CW: added for script based system analysis
	int GetCPUQuality( IFunctionHandler *pH );
	int GetGPUQuality( IFunctionHandler *pH );
	int GetSystemMem( IFunctionHandler *pH );
	int GetVideoMem( IFunctionHandler *pH );
	int IsPS20Supported( IFunctionHandler *pH );
	int IsHDRSupported( IFunctionHandler *pH );
	
	int SetBudget( IFunctionHandler *pH );		
	int SetVolumetricFogModifiers( IFunctionHandler *pH );

	int SetWind( IFunctionHandler *pH );
	int GetWind( IFunctionHandler *pH );

	int GetSurfaceTypeIdByName( IFunctionHandler *pH, const char* surfaceName);
	int GetSurfaceTypeNameById( IFunctionHandler *pH, int surfaceId );

	int RemoveEntity(IFunctionHandler *pH, ScriptHandle entityId); //int (return void)
	int SpawnEntity(IFunctionHandler *pH, SmartScriptTable params); //int (return int)
	int ActivateLight(IFunctionHandler *pH); //name, activate
//	int ActivateMainLight(IFunctionHandler *pH); //pos, activate
//	int SetSkyBox(IFunctionHandler *pH); //szShaderName, fBlendTime, bUseWorldBrAndColor
	int SetWaterVolumeOffset(IFunctionHandler *pH); //szShaderName, fBlendTime, bUseWorldBrAndColor
	int IsValidMapPos(IFunctionHandler *pH);
	int EnableMainView(IFunctionHandler *pH);
	int EnableOceanRendering(IFunctionHandler *pH); // int
	int	ScanDirectory(IFunctionHandler *pH);
	int DebugStats(IFunctionHandler *pH);
	int ViewDistanceSet(IFunctionHandler *pH);
	int ViewDistanceGet(IFunctionHandler *pH);
	int GetOutdoorAmbientColor(IFunctionHandler *pH);
	int SetOutdoorAmbientColor(IFunctionHandler *pH);
	int GetTerrainElevation(IFunctionHandler *pH);
//	int SetIndoorColor(IFunctionHandler *pH);
	int	ActivatePortal(IFunctionHandler *pH);
	int DumpMMStats(IFunctionHandler *pH);
	int EnumAAFormats(IFunctionHandler *pH);
	int EnumDisplayFormats(IFunctionHandler *pH);
	int IsPointIndoors(IFunctionHandler *pH);
	int SetConsoleImage(IFunctionHandler *pH);
	int ProjectToScreen(IFunctionHandler *pH, Vec3 vec);
	int EnableHeatVision(IFunctionHandler *pH);
	int ShowDebugger(IFunctionHandler *pH);
	int DumpMemStats (IFunctionHandler *pH);
	int DumpMemoryCoverage(IFunctionHandler *pH);
	int ApplicationTest (IFunctionHandler *pH);
	int QuitInNSeconds(IFunctionHandler *pH);
	int DumpWinHeaps (IFunctionHandler *pH);
	int Break(IFunctionHandler *pH);
	int SetViewCameraFov(IFunctionHandler *pH, float fov);
	int GetViewCameraFov(IFunctionHandler *pH);
	int IsPointVisible(IFunctionHandler *pH, Vec3 point);
	int GetViewCameraPos(IFunctionHandler *pH);
	int GetViewCameraDir(IFunctionHandler *pH);
	int GetViewCameraAngles(IFunctionHandler *pH);
	int RayWorldIntersection(IFunctionHandler *pH);
	int RayTraceCheck(IFunctionHandler *pH);
	int BrowseURL(IFunctionHandler *pH);
	int IsDevModeEnable(IFunctionHandler* pH);
	int SaveConfiguration(IFunctionHandler *pH);
	int SetSystemShaderRenderFlags(IFunctionHandler *pH);
	int Quit(IFunctionHandler *pH);
	int GetHDRDynamicMultiplier(IFunctionHandler *pH);
	int SetHDRDynamicMultiplier(IFunctionHandler *pH);
	int GetFrameID(IFunctionHandler *pH);

	// <title ClearKeyState>
	// Syntax: System.ClearKeyState()
	// Description:
	//    Clear the key state
	int ClearKeyState(IFunctionHandler *pH);

	// <title SetSunColor>
	// Syntax: System.SetSunColor( Vec3 vColor )
	// Description:
	//    Set color of the sun, only relevant outdoors.
	// Arguments:
	//    vColor - Sun Color as an {x,y,z} vector (x=r,y=g,z=b).
	int SetSunColor(IFunctionHandler *pH,Vec3 vColor );
	
	// <title GetSunColor>
	// Syntax: Vec3 System.GetSunColor()
	// Description:
	//    Retrieve color of the sun outdoors.
	// Return:
	//    Sun Color as an {x,y,z} vector (x=r,y=g,z=b).
	int GetSunColor(IFunctionHandler *pH );

	// <title SetSkyColor>
	// Syntax: System.SetSkyColor( Vec3 vColor )
	// Description:
	//    Set color of the sky (outdoors ambient color).
	// Arguments:
	//    vColor - Sky Color as an {x,y,z} vector (x=r,y=g,z=b).
	int SetSkyColor(IFunctionHandler *pH,Vec3 vColor );

	// <title GetSkyColor>
	// Syntax: Vec3 System.GetSkyColor()
	// Description:
	//    Retrieve color of the sky (outdoor ambient color).
	// Return:
	//    Sky Color as an {x,y,z} vector (x=r,y=g,z=b).
	int GetSkyColor(IFunctionHandler *pH );

	// <title SetSkyHighlight>
	// Syntax: System.SetSkyHighlight( Table params )
	// Description:
	//    Set Sky highlighing parameters.
	// See Also: GetSkyHighlight
	// Arguments:
	//    params - Table with Sky highlighing parameters.
	//      <TABLE>
	//          Highligh Params                  Meaning
	//          -------------                    -----------
	//          size                             Sky highlight scale.
	//          color                            Sky highlight color.
	//          direction                        Direction of the sky highlight in world space.
	//          pod                              Position of the sky highlight in world space.
	//       </TABLE>
	int SetSkyHighlight(IFunctionHandler *pH,SmartScriptTable params );

	// <title SetSkyHighlight>
	// Syntax: System.SetSkyHighlight( Table params )
	// Description:
	//    Retrieves Sky highlighing parameters.
	//    see SetSkyHighlight for parameters description.
	// See Also: SetSkyHighlight
	int GetSkyHighlight(IFunctionHandler *pH,SmartScriptTable params );


	// <title LoadLocalizationXml>
	// Syntax: System.LoadLocalizationXml( string filename )
	// Description:
	//    Loads Excel exported xml file with text and dialog localization data.
	int LoadLocalizationXml( IFunctionHandler *pH,const char *filename );

private:
	void MergeTable(IScriptTable *pDest, IScriptTable *pSrc);
	//log a string to console and/or to disk with support for different languages
	void	LogString(IFunctionHandler *pH,bool bToConsoleOnly);
	int DeformTerrainInternal( IFunctionHandler *pH, bool nameIsMaterial );

private:
	ISystem *m_pSystem;
	ILog *m_pLog;
	IRenderer *m_pRenderer;
	IConsole *m_pConsole;
	ITimer *m_pTimer;
	I3DEngine *m_p3DEngine;

//	ICVar *r_hdrrendering;						// the Cvar is created in cry3dengine, this is just a pointer


	//Vlad is too lazy to add this to 3DEngine - so I have to put it here. It should 
	//not be here, but I have to put it somewhere.....
	float	m_SkyFadeStart;		// when fogEnd less - start to fade sky to fog
	float	m_SkyFadeEnd;			// when fogEnd less - no sky, just fog

	SmartScriptTable m_pScriptTimeTable;
	SmartScriptTable m_pGetEntitiesCacheTable;
};

#endif // __ScriptBind_System_h__
