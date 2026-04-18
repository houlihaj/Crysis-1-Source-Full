////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   statobjman.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Loading trees, buildings, ragister/unregister entities for rendering
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "StatObj.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "terrain_sector.h"
#include "CullBuffer.h"
#include "3dEngine.h"
#include "IndexedMesh.h"
#include "WaterVolumes.h"
#include "Brush.h"
#include "LMCompStructures.h"

void CObjManager::PreloadNearObjects()
{
//  if(!GetCVars()->e_stream_cgf)
    return;

	FUNCTION_PROFILER_3DENGINE;

  CVisAreaManager * pVisAreaManager = GetVisAreaManager();
//  CVisArea * pVisArea = pVisAreaManager->m_pCurArea ? pVisAreaManager->m_pCurArea : pVisAreaManager->m_pCurPortal;
  if(CStatObj::m_fStreamingTimePerFrame<CGF_STREAMING_MAX_TIME_PER_FRAME)// && pVisArea)
  { // mark this and neighbor sectors
/*    pVisArea->MarkForStreaming();
    CVisArea * areaArray[8]={0,0,0,0,0,0,0,0};
    int nCount = pVisArea->GetVisAreaConnections((IVisArea **)&areaArray,8);
    for(int n=0; n<nCount; n++)
      areaArray[n]->MarkForStreaming();
  */
    // load marked objects
		for (LoadedObjects::iterator it = m_lstLoadedObjects.begin(); it != m_lstLoadedObjects.end(); ++it)
    {
      if((*it)->m_bUseStreaming && !(*it)->GetRenderMesh() &&	(*it)->m_nMarkedForStreamingFrameId > GetFrameID()+10)
      {	// streaming
        CStatObj::m_fStreamingTimePerFrame -= GetTimer()->GetAsyncCurTime();
        bool bRes = (*it)->LoadCGF((*it)->m_szFileName );
        (*it)->m_bUseStreaming=true;
        CStatObj::m_fStreamingTimePerFrame += GetTimer()->GetAsyncCurTime();
      }

      if(CStatObj::m_fStreamingTimePerFrame>CGF_STREAMING_MAX_TIME_PER_FRAME)
        break;
    }
  }
}

void CObjManager::CheckUnload()
{
  if(GetCVars()->e_stream_cgf)
		for (LoadedObjects::iterator it = m_lstLoadedObjects.begin(); it != m_lstLoadedObjects.end(); ++it)
  {
    CStatObj * pStatObj = (*it);
    if(pStatObj->m_bUseStreaming && pStatObj->GetRenderMesh() &&
      max(pStatObj->m_nLastRendFrameId, pStatObj->m_nMarkedForStreamingFrameId) < GetFrameID()-100)
    {
			int p;
      for(p=0; p<2; p++)
      { // check is phys geometry still in use
        phys_geometry * pPhysGeom = pStatObj->GetPhysGeom(p);
        if(pPhysGeom && pPhysGeom->nRefCount>1)
          break;
      }

      if(p==2)
      {
        pStatObj->ShutDown();
        pStatObj->Init();
        PrintMessage("Unloaded: %s", pStatObj->m_szFileName.c_str());
        break;
      }
    }
  }
}

void CObjManager::GetObjectsStreamingStatus(int & nReady, int & nTotalInStreaming, int & nTotal)
{
  nReady=nTotalInStreaming=nTotal=0;
	for (LoadedObjects::iterator it = m_lstLoadedObjects.begin(); it != m_lstLoadedObjects.end(); ++it)
  {
    nTotal++;
    if((*it)->m_bUseStreaming)
    {
      nTotalInStreaming++;
      if((*it)->GetRenderMesh())
      {
        CStatObj * p = (*it);
        nReady++;
      }
    }
    else
    {
      CStatObj * p = (*it);
      p=p;
      p->m_szFileName;
    }
  }
}

