/*=============================================================================
	CommonRender.cpp: Crytek Common render helper functions and structures declarations.
	Copyright (c) 2001 Crytek Studios. All Rights Reserved.

	Revision history:
		* Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"

// Resource manager internal variables.
ResourceClassMap CBaseResource::m_sResources;

CBaseResource& CBaseResource::operator=(const CBaseResource& Src)
{
  return *this;
}

CBaseResource::~CBaseResource()
{
  UnRegister();
}

bool CBaseResource::IsValid()
{
  SResourceContainer *pContainer = GetResourcesForClass(m_ClassName);
  if (!pContainer)
    return false;

  ResourceClassMapItor itRM = m_sResources.find(m_ClassName);

  if (itRM == m_sResources.end())
    return false;
  if (itRM->second != pContainer)
    return false;
  ResourcesMapItor itRL = itRM->second->m_RMap.find(m_Name);
  if (itRL == itRM->second->m_RMap.end())
    return false;
  if (itRL->second != this)
    return false;

  return true;
}

SResourceContainer *CBaseResource::GetResourcesForClass(const CCryName& className)
{
  ResourceClassMapItor itRM = m_sResources.find(className);
  if (itRM == m_sResources.end())
    return NULL;
  return itRM->second;
}

CBaseResource *CBaseResource::GetResource(const CCryName& className, int nID, bool bAddRef)
{
  SResourceContainer *pRL = GetResourcesForClass(className);
  if (!pRL)
    return NULL;

  //assert(pRL->m_RList.size() > nID);
  if (nID >= (int)pRL->m_RList.size() || nID < 0)
    return NULL;
  CBaseResource *pBR = pRL->m_RList[nID];
  if (pBR)
  {
    if (bAddRef)
      pBR->AddRef();
    return pBR;
  }
  return NULL;
}

CBaseResource *CBaseResource::GetResource(const CCryName& className, const CCryName& Name, bool bAddRef)
{
  SResourceContainer *pRL = GetResourcesForClass(className);
  if (!pRL)
    return NULL;

  CBaseResource *pBR = NULL;
  ResourcesMapItor itRL = pRL->m_RMap.find(Name);
  if (itRL != pRL->m_RMap.end())
  {
    pBR = itRL->second;
    if (bAddRef)
      pBR->AddRef();
    return pBR;
  }
  return NULL;
}

bool CBaseResource::Register(const CCryName& className, const CCryName& Name)
{
  SResourceContainer *pRL = GetResourcesForClass(className);
  if (!pRL)
  {
    pRL = new SResourceContainer;
    m_sResources.insert(ResourceClassMapItor::value_type(className, pRL));
  }

  assert(pRL);
  if (!pRL)
    return false;

  ResourcesMapItor itRL = pRL->m_RMap.find(Name);
  if (itRL != pRL->m_RMap.end())
    return false;
  pRL->m_RMap.insert(ResourcesMapItor::value_type(Name, this));
  int nIndex;
  if (pRL->m_AvailableIDs.size())
  {
    ResourceIds::iterator it = pRL->m_AvailableIDs.end()-1;
    nIndex = *it;
    pRL->m_AvailableIDs.erase(it);
    assert(nIndex < (int)pRL->m_RList.size());
    pRL->m_RList[nIndex] = this;
  }
  else
  {
    nIndex = pRL->m_RList.size();
    pRL->m_RList.push_back(this);
  }
  m_nID = nIndex;
  m_Name = Name;
  m_ClassName = className;
  m_nRefCount = 1;

  return true;
}

bool CBaseResource::UnRegister()
{
  if (IsValid())
  {
    SResourceContainer *pContainer = GetResourcesForClass(m_ClassName);
    assert(pContainer);
    if (pContainer)
    {
      pContainer->m_RMap.erase(m_Name);
      pContainer->m_RList[m_nID] = NULL;
      pContainer->m_AvailableIDs.push_back(m_nID);
    }
    return true;
  }
  return false;
}

//=================================================================

SResourceContainer::~SResourceContainer()
{
  ResourcesMapItor it;

  for (it=m_RMap.begin(); it!=m_RMap.end(); it++)
  {
    CBaseResource *pRes = it->second;
    if (pRes && CRenderer::CV_r_printmemoryleaks)
      iLog->Log("Warning: ~SResourceContainer: Resource %s was not deleted (%d)", pRes->GetName(), pRes->GetRefCounter());
    SAFE_RELEASE(pRes);
  }
  m_RMap.clear();
  m_RList.clear();
  m_AvailableIDs.clear();
}

void CBaseResource::ShutDown()
{
  if (CRenderer::CV_r_releaseallresourcesonexit)
  {
    ResourceClassMapItor it;
    for (it=m_sResources.begin(); it!=m_sResources.end(); it++)
    {
      SResourceContainer *pCN = it->second;
      SAFE_DELETE(pCN);
    }
    m_sResources.clear();
  }
}
