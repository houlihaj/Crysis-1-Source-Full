////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   cry3denginebase.h
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Access to external stuff used by 3d engine. Most 3d engine classes 
//               are derived from this base class to access other interfaces
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef _Cry3DEngineBase_h_
#define _Cry3DEngineBase_h_

struct ISystem;
struct IRenderer;
struct ILog;
struct IPhysicalWorld;
struct ITimer;
struct IConsole;
struct I3DEngine;
struct CVars;
struct CVisAreaManager;
struct IMaterialManager;
class  CTerrain;
class	 CIndirectLighting;
class  CObjManager;
class C3DEngine;
class CPartManager;
class CDecalManager;
class CRainManager;
class CCloudsManager;
class CSkyLightManager;
class CWaterWaveManager;
class CRenderMeshMerger;

#define DISTANCE_TO_THE_SUN 1000000

struct Cry3DEngineBase
{
  static ISystem * m_pSystem;
  static IRenderer * m_pRenderer;
  static ITimer * m_pTimer;
  static ILog * m_pLog;
  static IPhysicalWorld * m_pPhysicalWorld;
  static IConsole * m_pConsole;
  static C3DEngine * m_p3DEngine;
  static CVars * m_pCVars;
  static ICryPak * m_pCryPak;
	static IMaterialManager* m_pMaterialManager;
	static CObjManager *m_pObjManager;
	static CTerrain *m_pTerrain;
	static CPartManager   * m_pPartManager;
	static CDecalManager  * m_pDecalManager;
	static CCloudsManager* m_pCloudsManager;
	static CVisAreaManager* m_pVisAreaManager;
	static CMatMan        * m_pMatMan;
	static CSkyLightManager* m_pSkyLightManager;
  static CWaterWaveManager *m_pWaterWaveManager;
	static CRenderMeshMerger *m_pRenderMeshMerger;

  static int m_nRenderStackLevel;
	static float m_fZoomFactor;
  static int m_dwRecursionDrawFlags[MAX_RECURSION_LEVELS];
	static int m_nRenderFrameID;
	static int m_nRenderMainFrameID;
	static bool m_bProfilerEnabled;
	static float m_fPreloadStartTime;

  static int m_CpuFlags;
  static ESystemConfigSpec m_LightConfigSpec;
	static bool m_bEditorMode;
	static CCamera m_Camera;
	static int m_arrInstancesCounter[eERType_Last];

	// components access
  ILINE static ISystem						* GetSystem() { return m_pSystem; }
  ILINE static IRenderer					* GetRenderer() { return m_pRenderer; }
  ILINE static ITimer							* GetTimer() { return m_pTimer; }
  ILINE static ILog								* GetLog() 
	{ 
#if defined(__SPU__)
		return GetISPULog();
#else
		return m_pLog; 
#endif
	}
  ILINE static IPhysicalWorld			* GetPhysicalWorld() { return m_pPhysicalWorld;}
  ILINE static IConsole						* GetConsole() { return m_pConsole; }
  ILINE static C3DEngine					* Get3DEngine() { return m_p3DEngine; }
	ILINE static CObjManager				* GetObjManager() { return m_pObjManager; };
	ILINE static CTerrain						* GetTerrain() { return m_pTerrain; };
  ILINE static CVars							* GetCVars() { return m_pCVars; }
  ILINE static CVisAreaManager		* GetVisAreaManager() { return m_pVisAreaManager; }
  ILINE static ICryPak						* GetPak() { return m_pCryPak; }
	ILINE static IMaterialManager		* GetMatMan() { return (IMaterialManager*)m_pMatMan; }
	ILINE static CCloudsManager			* GetCloudsManager() { return m_pCloudsManager; }
  ILINE static CWaterWaveManager	* GetWaterWaveManager() { return m_pWaterWaveManager; };
	ILINE static CRenderMeshMerger	* GetSharedRenderMeshMerger() { return m_pRenderMeshMerger; };

	ILINE static int GetFrameID() { return m_nRenderFrameID; };
	ILINE static int GetMainFrameID() { return m_nRenderMainFrameID; };

	ILINE static const CCamera & GetCamera() { /*assert(sizeof(Cry3DEngineBase) == 0); */return m_Camera; }
	ILINE static void SetCamera(const CCamera & newCam) { m_Camera = newCam; }

  float GetCurTimeSec();
	float GetCurAsyncTimeSec();
	void PrintMessage(const char *szText,...) PRINTF_PARAMS(2, 3);
	void PrintMessagePlus(const char *szText,...) PRINTF_PARAMS(2, 3);
	void PrintComment(const char *szText,...) PRINTF_PARAMS(2, 3);

	// Validator warning.
	static void Warning( const char *format,... ) PRINTF_PARAMS(1, 2);
	static void Error( const char *format,... ) PRINTF_PARAMS(1, 2);
	static void FileWarning( int flags,const char *file,const char *format,... )
		PRINTF_PARAMS(3, 4);
	
	CRenderObject * GetIdentityCRenderObject() 
	{
		CRenderObject * pCRenderObject = GetRenderer()->EF_GetObject(true);
    if (!pCRenderObject)
      return NULL;
		pCRenderObject->m_II.m_Matrix.SetIdentity();
		return pCRenderObject;
	}

	static bool IsValidFile( const char *sFilename );
	static bool IsResourceLocked( const char *sFilename );

	static bool IsPreloadEnabled();

	IMaterial * MakeSystemMaterialFromShader(const char * sShaderName);
	void DrawBBox( const Vec3 & vMin, const Vec3 & vMax );
	void DrawBBox( const AABB & box, ColorB col = Col_White );
	void DrawLine( const Vec3 & vMin, const Vec3 & vMax );
	void DrawSphere( const Vec3 & vPos, float fRadius, ColorB color = ColorB(255,255,255,255) );

	int & GetInstCount(EERType eType) { return m_arrInstancesCounter[eType]; }

	uint32 GetMinSpecFromRenderNodeFlags( uint32 dwRndFlags ) { return (dwRndFlags&ERF_SPEC_BITS_MASK) >> ERF_SPEC_BITS_SHIFT; }
	bool CheckMinSpec( uint32 nMinSpec );
};

#endif // _Cry3DEngineBase_h_
