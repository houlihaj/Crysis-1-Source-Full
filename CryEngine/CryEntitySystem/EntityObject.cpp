////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   EntityObject.cpp
//  Version:     v1.00
//  Created:     18/5/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "EntityObject.h"
#include "Entity.h"
#include "Force.h"

#include <IRenderer.h>
#include <IEntityRenderState.h>
#include <I3DEngine.h>
#include <ICryAnimation.h>

#define MAX_CHARACTER_LOD 10

namespace
{
	Matrix34 sIdentMatrix = Matrix34::CreateIdentity();
}

//////////////////////////////////////////////////////////////////////////
CEntityObject::~CEntityObject()
{
	ReleaseObjects();
	if (m_pXForm)
		delete m_pXForm;
}

//////////////////////////////////////////////////////////////////////////
void CEntityObject::ReleaseObjects()
{
	if (pStatObj)
	{
		pStatObj->Release();
		pStatObj = NULL;
	}
	else if (pCharacter)
	{
		if (ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim())
			pSkeletonAnim->SetEventCallback(0, 0);

		pCharacter->Release();
		pCharacter = NULL;
	}
	else if (pLight)
	{
		pLight->ReleaseNode();
		pLight = NULL;
	}
	else if (GetParticleEmitter())
	{
		IParticleEmitter* pEmitter = GetParticleEmitter();
		pEmitter->Activate(false);
		pEmitter->Release();
		pChildRenderNode = 0;
	}
	else if (pChildRenderNode)
	{
		pChildRenderNode->ReleaseNode();
		pChildRenderNode = 0;
	}
	if (pFoliage)
	{
		pFoliage->Release();
		pFoliage = 0;
	}

	if(m_pRNTmpData)
		GetISystem()->GetI3DEngine()->FreeRNTmpData(&m_pRNTmpData);
	assert(!m_pRNTmpData);
}

//////////////////////////////////////////////////////////////////////////
bool CEntityObject::GetLocalBounds( AABB &bounds )
{
	bool bInitialized = false;
	if (pStatObj)
	{
		bounds.Add( pStatObj->GetBoxMin() );
		bounds.Add( pStatObj->GetBoxMax() );
		bInitialized = true;
	}
	else if (pCharacter)
	{
		AABB aabb=pCharacter->GetAABB();
		bounds.Add(aabb.min);
		bounds.Add(aabb.max);
		bInitialized = true;
	}
	else if (pLight)
	{
		// Make some small default bounds.
		Vec3 v(0.1f,0.1f,0.1f);
		bounds.Add( -v );
		bounds.Add( v );
		bInitialized = true;
	}
	else if (pChildRenderNode)
	{
		pChildRenderNode->GetLocalBounds( bounds );
		bInitialized = true;
	}
	return bInitialized;
}

//////////////////////////////////////////////////////////////////////////
void CEntityObject::SetParent( CEntityObject *pParentSlot )
{
	pParent = pParentSlot;
	if (!m_pXForm)
		m_pXForm = new SEntityObjectXForm;
	bWorldTMValid = false;
}

//////////////////////////////////////////////////////////////////////////
void CEntityObject::SetLocalTM( const Matrix34 &localTM )
{
	if (!m_pXForm)
		m_pXForm = new SEntityObjectXForm;
  
	m_pXForm->localTM = localTM;
	bWorldTMValid = false;   
	if (pParent)  
	{
		pParent->bWorldTMValid = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityObject::UpdateWorldTM( CEntity *pEntity )
{
	if (!pParent)
	{
		// Add entity matrix as parent.
		if (!m_pXForm)
			m_worldTM = pEntity->GetWorldTM_Fast();
		else
			m_worldTM = pEntity->GetWorldTM_Fast() * m_pXForm->localTM; 
	}
	else 
	{
		if (!m_pXForm)
			m_worldTM = pParent->GetWorldTM(pEntity);
		else
			m_worldTM = pParent->GetWorldTM(pEntity) * m_pXForm->localTM;
	}
	bWorldTMValid = true;
	
  // Update previous matrix, first time.		
	if (!bPrevWorldTMValid)
		m_worldPrevTM = m_worldTM;
}

//////////////////////////////////////////////////////////////////////////
const Matrix34& CEntityObject::GetLocalTM() const
{
	if (m_pXForm)
		return m_pXForm->localTM;
	return sIdentMatrix;
};

//////////////////////////////////////////////////////////////////////////
const Matrix34& CEntityObject::GetWorldTM( CEntity *pEntity )
{
	if (!bWorldTMValid)
	{
		UpdateWorldTM(pEntity);
	}
  return m_worldTM;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityObject::Render( CEntity *pEntity,SRendParams &rParams,int nRndFlags,CRenderProxy *pRenderProxy )
{
	bool bDrawn = false;

	if (!bWorldTMValid)
	{
		UpdateWorldTM(pEntity);
	}
  
	// Override with custom slot material.
	IMaterial *pPrevMtl = rParams.pMaterial;
	if (pMaterial)
		rParams.pMaterial = pMaterial;

	int nOldObjectFlags = rParams.dwFObjFlags;
	rParams.dwFObjFlags |= dwFObjFlags;

	//////////////////////////////////////////////////////////////////////////

	// Draw static object.
	if (pStatObj)
	{
    rParams.pMatrix = &m_worldTM;
    if( rParams.nMotionBlurAmount || rParams.nMaterialLayers & (1<<2) )
    {
      rParams.pPrevMatrix = &m_worldPrevTM;
    }
    
		rParams.dwFObjFlags |= FOB_TRANS_MASK;
		rParams.pFoliage = pFoliage;
		rParams.ppRNTmpData = &m_pRNTmpData;

		pStatObj->Render( rParams );
		bDrawn = true;
	}
	//////////////////////////////////////////////////////////////////////////
	
	//////////////////////////////////////////////////////////////////////////
	// Draw character.
	if (pCharacter)
	{
		QuatTS Offset;
		Matrix34 PhysLocation;
		QuatT PhysicalPosition;
		PhysicalPosition.t = pEntity->GetWorldPos();
		PhysicalPosition.q = pEntity->GetWorldRotation();

		PhysLocation = Matrix34(PhysicalPosition);    
		rParams.pMatrix = &PhysLocation;
		Offset = QuatTS(PhysLocation.GetInverted()*m_worldTM);
		//	rParams.pMatrix = &m_worldTM;
		//	Offset.SetIdentity();

    if( (rParams.nMotionBlurAmount || rParams.nMaterialLayers & (1<<2) ) ) 
      rParams.pPrevMatrix = &m_worldPrevTM;

    Matrix34 pFinalPhysLocation = m_worldPrevTM;
 		pCharacter->Render( rParams, Offset, &pFinalPhysLocation);
		bDrawn = true;

    if( !(rParams.dwFObjFlags & FOB_RENDER_INTO_SHADOWMAP) )
    {
      bPrevWorldTMValid = true;
      m_worldPrevTM = pFinalPhysLocation; //PhysLocation;

			// If We render character, make sure it is also gets animation activated.
			if (!pEntity->m_bInActiveList)
				pEntity->ActivateForNumUpdates(8);
    }
	}
  
	if (pChildRenderNode)
	{
		rParams.pMatrix = &m_worldTM;
    
    if( rParams.nMotionBlurAmount || rParams.nMaterialLayers & (1<<2) )
    {      
		  rParams.pPrevMatrix = &m_worldPrevTM;
    }

		pChildRenderNode->m_dwRndFlags = nRndFlags;
		pChildRenderNode->Render( rParams );
	}

	//////////////////////////////////////////////////////////////////////////
	rParams.pMaterial = pPrevMtl;
	rParams.dwFObjFlags = nOldObjectFlags;

	if ( !(rParams.dwFObjFlags & FOB_RENDER_INTO_SHADOWMAP)) // Should also ignore rendering into the recursion.
	{
    if( !pCharacter ) //|| pCharacter && !(pRenderProxy->m_nFlags & CRenderProxy::FLAG_PREUPDATE_ANIM)  )
    {
		  bPrevWorldTMValid = true;
		  m_worldPrevTM = m_worldTM;
    }

		if (pFoliage)
		{
			pFoliage->SetFlags(pFoliage->GetFlags() & ~IFoliage::FLAG_FROZEN | -(int)(rParams.nMaterialLayers&MTL_LAYER_FROZEN) & IFoliage::FLAG_FROZEN);
			static ICVar *g_pWindActivationDist = gEnv->pConsole->GetCVar("e_foliage_wind_activation_dist");
			float maxdist = g_pWindActivationDist ? g_pWindActivationDist->GetFVal() : 0.0f;
			Vec3 pos = m_worldTM.GetTranslation();
			if ((gEnv->pSystem->GetViewCamera().GetPosition()-pos).len2()<sqr(maxdist) && gEnv->p3DEngine->GetWind(AABB(pos),false).len2()>101.0f)
				pStatObj->PhysicalizeFoliage(pEntity->GetPhysics(),m_worldTM,pFoliage,0,4);
		}
	}
  
	return bDrawn;
}

//////////////////////////////////////////////////////////////////////////
void CEntityObject::OnXForm( CEntity *pEntity )
{
	UpdateWorldTM(pEntity);
 	
	if (pLight)
	{
		ILightSource *pLightSource = pLight;
		// Update light positions.
		CDLight *pLight = &pLightSource->GetLightProperties();

		pLight->m_Origin = m_worldTM.GetTranslation();
		pLight->MakeBaseParams();
		pLight->SetMatrix(m_worldTM);
		pLight->m_sName = pEntity->GetName(); // For debugging only.
		pLightSource->SetMatrix(m_worldTM);
	}
	if (pChildRenderNode)
	{
		pChildRenderNode->SetMatrix( m_worldTM );
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityObject::OnNotSeenTimeout()
{
	if (pCharacter)
		pCharacter->ReleaseTemporaryResources();
}

//////////////////////////////////////////////////////////////////////////
void CEntityObject::Update( CEntity *pEntity,bool bVisible,bool &bBoundsChanged )
{
	bBoundsChanged = false;
	if (pCharacter)
	{
		int characterFlags = pCharacter->GetFlags();
		if ((flags&ENTITY_SLOT_RENDER) || (characterFlags & CS_FLAG_UPDATE_ALWAYS))
		{
			bBoundsChanged = true; // Lets bounds of character change constantly.	
			if (characterFlags & CS_FLAG_UPDATE)
			{
				if (!bWorldTMValid)
					UpdateWorldTM(pEntity);

				QuatT PhysicalPosition;
				PhysicalPosition.t = pEntity->GetWorldPos();
				PhysicalPosition.q = pEntity->GetWorldRotation();
				QuatTS AnimatedCharacter = QuatTS( m_worldTM );

				float fDistance = (GetISystem()->GetViewCamera().GetPosition() - AnimatedCharacter.t).GetLength();
				float fZoomFactor = 0.001f + 0.999f*(RAD2DEG(GetISystem()->GetViewCamera().GetFov())/60.f);

				pCharacter->SkeletonPreProcess( PhysicalPosition, AnimatedCharacter, GetISystem()->GetViewCamera(),0 );
				pCharacter->SkeletonPostProcess( PhysicalPosition, AnimatedCharacter, 0, fDistance * fZoomFactor, 0xdead );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
