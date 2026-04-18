////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   cry3dengine.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Defines the DLL entry point, implements access to other modules
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

// Must be included only once in DLL module.
#include <platform_impl.h>

#include "3dEngine.h"

#define MAX_ERROR_STRING 4096



//////////////////////////////////////////////////////////////////////

struct CSystemEventListner_3DEngine : public ISystemEventListener
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
				break;
			}
		case ESYSTEM_EVENT_LEVEL_LOAD_END:
			{
				Cry3DEngineBase::m_p3DEngine->ClearDebugFPSInfo();
				Cry3DEngineBase::GetObjManager()->FreeNotUsedCGFs();
				break;
			}
		}
	}
};
static CSystemEventListner_3DEngine g_system_event_listener_engine;

I3DEngine * CreateCry3DEngine(ISystem	* pSystem, const char * szInterfaceVersion)
{
	ModuleInitISystem(pSystem);
	pSystem->GetISystemEventDispatcher()->RegisterListener( &g_system_event_listener_engine );
	
	/*
	__time64_t ltime1, ltime2;
	_time64( &ltime1 );
	
	int nElemNum = 100000*64/sizeof(float);
	unsigned short * pShorts = new unsigned short[nElemNum];
	float * pFloats = new float[nElemNum];

	for(int t=0; t<1000; t++)
	{
		for(int i=0; i<nElemNum; i++)
		{
			pFloats[i] = 0.01f*pShorts[i];
		}
	}

	_time64( &ltime2 );
	float fTimeDif = float(ltime2 - ltime1);

	delete [] pShorts;
	delete [] pFloats;

	char buff[32];
	snprintf(buff,"%.2f",fTimeDif/1000);
	MessageBox(0, buff, "aa", MB_OK);

	return 0;
*/
#if !defined(LINUX)
	bool versionOk = false;
#ifdef PS3
	// The version string for PS3 is a complete timestamp, including time of day.
	// We'll only check if the date matches and ignore the time of day.
	const char *timeSep = strrchr(szInterfaceVersion, ' ');
	if (!timeSep)
		versionOk = !strcmp(szInterfaceVersion, g3deInterfaceVersion);
	else
		versionOk = !strncmp(szInterfaceVersion, g3deInterfaceVersion,
		  timeSep - szInterfaceVersion + 1);
#else
	versionOk = !strcmp(szInterfaceVersion, g3deInterfaceVersion);
#endif
//  if (!versionOk)
  //  Log("Error: CreateCry3DEngine(): 3dengine interface version error");
#endif
  C3DEngine * p3DEngine = new C3DEngine(pSystem);
  return p3DEngine;
}

void Cry3DEngineBase::PrintComment(const char *szText,...)
{
	if(szText)
	{
		va_list		arglist;
		char		buf[1024];
		va_start(arglist, szText);
		vsnprintf(buf,sizeof(buf),szText, arglist);
		va_end(arglist);	

		GetLog()->LogV(IMiniLog::eComment,buf,NULL);
	}
}

void Cry3DEngineBase::PrintMessage(const char *szText,...)
{
  if(szText)
  {
    va_list		arglist;
    char		buf[1024];
    va_start(arglist, szText);
    vsnprintf(buf,sizeof(buf),szText, arglist);
    va_end(arglist);	
    GetLog()->Log(buf);
  }

	GetLog()->UpdateLoadingScreen(0);
}

void Cry3DEngineBase::PrintMessagePlus(const char *szText,...)
{
	if(szText)
	{
		va_list		arglist;
		char		buf[1024];
		va_start(arglist, szText);
		vsnprintf(buf,sizeof(buf),szText, arglist);
		va_end(arglist);	
		GetLog()->LogPlus(buf);
	}

	GetLog()->UpdateLoadingScreen(0);
}

float Cry3DEngineBase::GetCurTimeSec() 
{ return (m_pSystem->GetITimer()->GetCurrTime()); }

float Cry3DEngineBase::GetCurAsyncTimeSec() 
{ return (m_pSystem->GetITimer()->GetAsyncCurTime()); }

//////////////////////////////////////////////////////////////////////////
void Cry3DEngineBase::Warning( const char *format,... )
{
	assert(format);
	if (!format)
		return;
	va_list	ArgList;
	char		szBuffer[MAX_ERROR_STRING];
	va_start(ArgList, format);
	vsnprintf(szBuffer, sizeof(szBuffer), format, ArgList);
	va_end(ArgList);

	// Call to validating warning of system.
	m_pSystem->Warning( VALIDATOR_MODULE_3DENGINE,VALIDATOR_WARNING,0,0,"%s",szBuffer );

  GetLog()->UpdateLoadingScreen(0);
}

//////////////////////////////////////////////////////////////////////////
void Cry3DEngineBase::Error( const char *format,... )
{
	assert(format);
	if (!format)
		return;
	va_list	ArgList;
	char		szBuffer[MAX_ERROR_STRING];
	va_start(ArgList, format);
	vsnprintf(szBuffer, sizeof(szBuffer), format, ArgList);
	va_end(ArgList);

	// Call to validating warning of system.
	m_pSystem->Warning( VALIDATOR_MODULE_3DENGINE,VALIDATOR_ERROR,0,0,"%s",szBuffer );

	GetLog()->UpdateLoadingScreen(0);
}

//////////////////////////////////////////////////////////////////////////
void Cry3DEngineBase::FileWarning( int flags,const char *file,const char *format,... )
{
	va_list	ArgList;
	char		szBuffer[MAX_ERROR_STRING];
	va_start(ArgList, format);
	vsnprintf(szBuffer, sizeof(szBuffer), format, ArgList);
	va_end(ArgList);

	// Call to validating warning of system.
	m_pSystem->Warning( VALIDATOR_MODULE_3DENGINE,VALIDATOR_WARNING,flags|VALIDATOR_FLAG_FILE,file,"%s",szBuffer );

	GetLog()->UpdateLoadingScreen(0);
}

IMaterial * Cry3DEngineBase::MakeSystemMaterialFromShader(const char * sShaderName)
{
	IMaterial * pMat = Get3DEngine()->GetMaterialManager()->CreateMaterial( sShaderName );
	pMat->AddRef();
	SShaderItem si;
	si = GetRenderer()->EF_LoadShaderItem(sShaderName, true, EF_SYSTEM);
	pMat->AssignShaderItem(si);
	return pMat;
}

//////////////////////////////////////////////////////////////////////////
bool Cry3DEngineBase::IsValidFile( const char *sFilename )
{
	CCryFile file;
	if (!file.Open(sFilename,"rb",ICryPak::FOPEN_HINT_QUIET))
	{
		return false;
	}
	if (file.GetLength() <= 0)
		return false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool Cry3DEngineBase::IsResourceLocked( const char *sFilename )
{
	IResourceList *pResList = GetPak()->GetRecorderdResourceList(ICryPak::RFOM_NextLevel);
	if (pResList)
	{
		return pResList->IsExist( sFilename );
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void Cry3DEngineBase::DrawBBox( const Vec3 & vMin, const Vec3 & vMax )
{
	GetRenderer()->GetIRenderAuxGeom()->SetRenderFlags(SAuxGeomRenderFlags());
	GetRenderer()->GetIRenderAuxGeom()->DrawAABB(AABB(vMin, vMax),false,ColorB(255,255,255,255),eBBD_Faceted);
}

void Cry3DEngineBase::DrawBBox( const AABB & box, ColorB col )
{
	GetRenderer()->GetIRenderAuxGeom()->SetRenderFlags(SAuxGeomRenderFlags());
	GetRenderer()->GetIRenderAuxGeom()->DrawAABB(box,false,col,eBBD_Faceted);
}

void Cry3DEngineBase::DrawLine( const Vec3 & vMin, const Vec3 & vMax )
{
	GetRenderer()->GetIRenderAuxGeom()->SetRenderFlags(SAuxGeomRenderFlags());
	GetRenderer()->GetIRenderAuxGeom()->DrawLine(vMin, ColorB(255,255,255,255), vMax, ColorB(255,255,255,255));
}

void Cry3DEngineBase::DrawSphere( const Vec3 & vPos, float fRadius, ColorB color )
{
	GetRenderer()->GetIRenderAuxGeom()->SetRenderFlags(SAuxGeomRenderFlags());
	GetRenderer()->GetIRenderAuxGeom()->DrawSphere(vPos, fRadius, color);
}

// Check if preloading is enabled.
bool Cry3DEngineBase::IsPreloadEnabled()
{
	bool bPreload = false;
	ICVar *pSysPreload = GetConsole()->GetCVar( "sys_preload" );
	if (pSysPreload && pSysPreload->GetIVal() != 0)
		bPreload = true;

	return bPreload;
}

//////////////////////////////////////////////////////////////////////////
bool Cry3DEngineBase::CheckMinSpec( uint32 nMinSpec )
{
	if (nMinSpec == CONFIG_DETAIL_SPEC && GetCVars()->e_view_dist_ratio_detail == 0)
		return false;

	if (nMinSpec != 0 && GetCVars()->e_obj_quality != 0 && nMinSpec > GetCVars()->e_obj_quality)
		return false;

	return true;
}

#include <CrtDebugStats.h>

// Common
#include "TypeInfo_impl.h"
#include "Cry_Quat_info.h"
#include "Cry_Matrix_info.h"
#include "CryHeaders_info.h"
#include "CGFContent_info.h"
#include "IIndexedMesh_info.h"
#include "IEntityRenderState_info.h"
#include "LMCompStructures_info.h"

// 3DEngine
//#include "dds_info.h"
#include "LMSerializationManager_info.h"
#include "SkyLightNishita_info.h"
#include "terrain_sector_info.h"
#include "VoxMan_info.h"

#include "ParticleParamsTypeInfo.cpp"

// Manually instantiate templates as needed here.
STRUCT_INFO_T_INSTANTIATE(Ang3_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Plane_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Matrix33_tpl, <float>)
