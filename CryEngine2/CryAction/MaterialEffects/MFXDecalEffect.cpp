////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   MFXDecalEffect.cpp
//  Version:     v1.00
//  Created:     28/11/2006 by JohnN/AlexL
//  Compilers:   Visual Studio.NET
//  Description: Decal effect
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "MFXDecalEffect.h"

CMFXDecalEffect::CMFXDecalEffect()
{
}

CMFXDecalEffect::~CMFXDecalEffect()
{
}

void CMFXDecalEffect::ReadXMLNode(XmlNodeRef &node)
{
	IMFXEffect::ReadXMLNode(node);
	
  XmlNodeRef filename = node->findChild("Filename");
	if (filename)
	{
		m_decalParams.filename = filename->getContent();
	}
	
  XmlNodeRef material = node->findChild("Material");	
	if (material)
	{
		m_decalParams.material = material->getContent();
	}
   	
  m_decalParams.minscale = 1.f;
	m_decalParams.maxscale = 1.f;
	m_decalParams.lifetime = 10.0f;
  
  XmlNodeRef scalenode = node->findChild("Scale");
	if (scalenode)
	{
		m_decalParams.minscale = atof(scalenode->getContent());
    m_decalParams.maxscale = m_decalParams.minscale;
	}	  
  
  node->getAttr("minscale", m_decalParams.minscale);
	node->getAttr("maxscale", m_decalParams.maxscale);
	node->getAttr("lifetime", m_decalParams.lifetime);
}

IMFXEffectPtr CMFXDecalEffect::Clone()
{
	CMFXDecalEffect* clone = new CMFXDecalEffect();
	clone->m_decalParams.filename = m_decalParams.filename;
	clone->m_decalParams.material = m_decalParams.material;
	clone->m_decalParams.minscale = m_decalParams.minscale;
	clone->m_decalParams.maxscale = m_decalParams.maxscale;
	clone->m_decalParams.lifetime = m_decalParams.lifetime;
	clone->m_effectParams = m_effectParams;
	return clone;
}

void CMFXDecalEffect::Execute(SMFXRunTimeEffectParams &params)
{
  FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);

	if (!(params.playflags & MFX_PLAY_DECAL))
		return;

	// not on a static object or entity
	
  const float angle = (params.angle != MFX_INVALID_ANGLE) ? params.angle : Random(0.f, gf_PI2);
  
	if (!params.trgRenderNode && !params.trg)
	{
    const float terrainHeight( gEnv->p3DEngine->GetTerrainElevation(params.pos.x, params.pos.y) );
    const float terrainDelta( params.pos.z - terrainHeight );  

		if (terrainDelta > 2.0f || terrainDelta < -0.5f)
			return;

		CryEngineDecalInfo terrainDecal;
		terrainDecal.vPos = Vec3(params.decalPos.x, params.decalPos.y, terrainHeight);
		terrainDecal.vNormal = params.normal;
		terrainDecal.vHitDirection = params.dir[0].GetNormalized();
		terrainDecal.bAdjustPos = false;
		terrainDecal.fLifeTime = m_decalParams.lifetime;

		if (!m_decalParams.material.empty())
			strcpy(terrainDecal.szMaterialName, m_decalParams.material.c_str());
		else
			strcpy(terrainDecal.szTextureName, m_decalParams.filename.c_str());

		terrainDecal.fSize = Random(m_decalParams.minscale, m_decalParams.maxscale);
		terrainDecal.fAngle = angle;
		gEnv->p3DEngine->CreateDecal(terrainDecal);
	}
	else
	{
		CryEngineDecalInfo decal;

		IEntity *pEnt = gEnv->pEntitySystem->GetEntity(params.trg);
		IRenderNode* pRenderNode = NULL;

		if (params.trg && !pEnt)
			return;

		if (pEnt)
		{
			if (pEnt->IsGarbage())
				return;

			IEntityRenderProxy *pRenderProxy = (IEntityRenderProxy*)pEnt->GetProxy(ENTITY_PROXY_RENDER);
			if (pRenderProxy)
				pRenderNode = pRenderProxy->GetRenderNode();
		}
		else
			pRenderNode = params.trgRenderNode;

		// filter out ropes
		if (pRenderNode && pRenderNode->GetRenderNodeType() == eERType_Rope)
			return;

		decal.ownerInfo.pRenderNode = pRenderNode;

		decal.vPos = params.pos;
		decal.vNormal = params.normal;
		decal.vHitDirection = params.dir[0].GetNormalized();
		decal.bAdjustPos = false;
		decal.fLifeTime = m_decalParams.lifetime;

		if (!m_decalParams.material.empty())
			strcpy(decal.szMaterialName, m_decalParams.material.c_str());
		else
			strcpy(decal.szTextureName, m_decalParams.filename.c_str());

		decal.fSize = Random(m_decalParams.minscale, m_decalParams.maxscale);
		decal.fAngle = angle;

		gEnv->p3DEngine->CreateDecal(decal);
	}
}

void CMFXDecalEffect::GetResources(SMFXResourceList &rlist)
{
	SMFXDecalListNode *listNode = SMFXDecalListNode::Create();
	listNode->m_decalParams.filename = m_decalParams.filename.c_str();
	listNode->m_decalParams.material = m_decalParams.material.c_str();
	listNode->m_decalParams.minscale = m_decalParams.minscale;
	listNode->m_decalParams.maxscale = m_decalParams.maxscale;
	listNode->m_decalParams.lifetime = m_decalParams.lifetime;

  SMFXDecalListNode* next = rlist.m_decalList;

  if (!next)
    rlist.m_decalList = listNode;
  else
  { 
    while (next->pNext)
      next = next->pNext;

    next->pNext = listNode;
  }  
}