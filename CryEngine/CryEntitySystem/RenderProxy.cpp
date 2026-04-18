////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   RenderProxy.h
//  Version:     v1.00
//  Created:     19/5/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "RenderProxy.h"
#include "Entity.h"
#include "EntityObject.h"
#include "ISerialize.h"
#include "ParticleParams.h"

#include <IRenderer.h>
#include <IEntityRenderState.h>
#include <ISystem.h>
#include <ICryAnimation.h>
#include "../CryAction/IActorSystem.h"

float CRenderProxy::gsWaterLevel;

CEntityTimeoutList* CRenderProxy::s_pTimeoutList = 0;

inline void CheckIfBBoxValid( const AABB &box )
{
	float v = box.min.x + box.min.y + box.min.z + box.max.x + box.max.y + box.max.z;
	if (!_finite(v))
	{
		CryWarning( VALIDATOR_MODULE_ENTITYSYSTEM,VALIDATOR_WARNING,"BBox is not valid" );
	}
}

//////////////////////////////////////////////////////////////////////////
CRenderProxy::CRenderProxy( CEntity *pEntity )
{
	m_pEntity = pEntity;
	m_entityId = pEntity->GetId(); // Store so we can use it during Render() (when we shouldn't touch the entity itself).
	m_nFlags = 0;

	m_localBBox.min.Set(0,0,0);
	m_localBBox.max.Set(0,0,0);
	m_WSBBox.min=Vec3(ZERO);
	m_WSBBox.max=Vec3(ZERO);

	m_pMaterial = pEntity->m_pMaterial;
  
  m_nMotionBlurAmount = 128;
  m_nVisionParams = 0; 
  m_nMaterialLayers = 0;
  m_nMaterialLayersBlend = 0;
  m_nOpacity = 255;
  m_vWindBending = Vec2(ZERO);
	m_bHasVertexScatterBuffer = false;

	memset(m_arrScatterVertexBuffer, 0, sizeof(m_arrScatterVertexBuffer));

	UpdateEntityFlags();

	if (pEntity->m_bInitialized)
	{
		AddFlags(FLAG_POST_INIT);
		RegisterForRendering(true);
	}

	//set water level to avoid accessing it all the time
	gsWaterLevel = GetI3DEngine()->GetWaterLevel();
	m_fLastSeenTime = GetISystem()->GetITimer()->GetCurrTime();

}

//////////////////////////////////////////////////////////////////////////
CRenderProxy::~CRenderProxy()
{
  GetI3DEngine()->FreeRenderNodeState(this); // internally calls UnRegisterEntity(this)

	// Free all slots.
	for (uint32 i = 0; i < m_slots.size(); i++)
	{
		if (m_slots[i])
			delete m_slots[i];
	}
	m_slots.clear();
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::PostInit()
{
	AddFlags(FLAG_POST_INIT);
	RegisterForRendering(true);
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::Release()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::UpdateEntityFlags()
{
	int nEntityFlags = m_pEntity->GetFlags();
	m_nEntityFlags = nEntityFlags;
	
	if (nEntityFlags & ENTITY_FLAG_CASTSHADOW)
		m_dwRndFlags |= ERF_CASTSHADOWMAPS;
	else
		m_dwRndFlags &= ~ERF_CASTSHADOWMAPS;
	
	if (nEntityFlags & ENTITY_FLAG_CASTSCATTER)
		m_dwRndFlags |= ERF_CASTSCATTERMAPS;
	else
		m_dwRndFlags &= ~ERF_CASTSCATTERMAPS;

	if (nEntityFlags & ENTITY_FLAG_GOOD_OCCLUDER)
		m_dwRndFlags |= ERF_GOOD_OCCLUDER;
	else
		m_dwRndFlags &= ~ERF_GOOD_OCCLUDER;

	if (nEntityFlags & ENTITY_FLAG_OUTDOORONLY)
		m_dwRndFlags |= ERF_OUTDOORONLY;
	else
		m_dwRndFlags &= ~ERF_OUTDOORONLY;

  if(nEntityFlags & ENTITY_FLAG_RECVWIND)
    m_dwRndFlags |= ERF_RECVWIND;
  else
    m_dwRndFlags &= ~ERF_RECVWIND;

	IMaterial* pMat = GetRenderMaterial();
	if (pMat && pMat->IsSubSurfScatterCaster())
		m_dwRndFlags |= ERF_SUBSURFSCATTER;
	else
		m_dwRndFlags &= ~ERF_SUBSURFSCATTER;

	if (pMat && pMat->GetFlags() & MTL_FLAG_VISIBLE_ONLY_IN_EDITOR)
		m_dwRndFlags |= ERF_VISIBLE_ONLY_IN_EDITOR;
	else
		m_dwRndFlags &= ~ERF_VISIBLE_ONLY_IN_EDITOR;

}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::UpdateUseOccluders( bool bUse )
{
	if (bUse)
	{
//		m_bCharacter = true;
		m_nInternalFlags |= HAS_OCCLUSION_PROXY;
		m_dwRndFlags |= ERF_GOOD_OCCLUDER;
//		GetI3DEngine()->RegisterEntity(this);
	}
	else
	{
		m_nInternalFlags &= ~HAS_OCCLUSION_PROXY;
		m_dwRndFlags &= ~ERF_GOOD_OCCLUDER;
//		GetI3DEngine()->UnRegisterEntity(this);
//		m_bCharacter = false;
	}
}


//////////////////////////////////////////////////////////////////////////
bool CRenderProxy::CheckCharacterForMorphs(ICharacterInstance* pCharacter)
{
	class Recurser
	{
	public:
		Recurser(bool& hasMorphs): m_hasMorphs(hasMorphs) {}

		void operator()(ICharacterInstance* pCharacter)
		{
			IAnimationSet* pAnimationSet = (pCharacter ? pCharacter->GetIAnimationSet() : 0);
			int numMorphTargets = (pAnimationSet ? pAnimationSet->numMorphTargets() : 0);
			if (numMorphTargets)
				m_hasMorphs = true;

			IAttachmentManager* pAttachmentManager = (pCharacter ? pCharacter->GetIAttachmentManager() : 0);
			for (int attachmentIndex = 0, end = (pAttachmentManager ? pAttachmentManager->GetAttachmentCount() : 0); attachmentIndex < end; ++attachmentIndex)
			{
				IAttachment* pAttachment = (pAttachmentManager ? pAttachmentManager->GetInterfaceByIndex(attachmentIndex) : 0);
				IAttachmentObject* pAttachmentObject = (pAttachment ? pAttachment->GetIAttachmentObject() : 0);
				ICharacterInstance* pAttachmentCharacter = (pAttachmentObject ? pAttachmentObject->GetICharacterInstance() : 0);
				if (pAttachmentCharacter)
					(*this)(pAttachmentCharacter);
			}
		}

	private:
		bool& m_hasMorphs;
	};

	bool hasMorphs = false;
	Recurser recurser(hasMorphs);
	recurser(pCharacter);
	return hasMorphs;
}

//////////////////////////////////////////////////////////////////////////
int CRenderProxy::AnimEventCallback(ICharacterInstance *pCharacter, void *userdata)
{
	CRenderProxy* pInstance = static_cast< CRenderProxy* >(userdata);
	if (pInstance)
		pInstance->AnimationEvent(pCharacter, pCharacter->GetISkeletonAnim()->GetLastAnimEvent());
	return 1;
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::AnimationEvent(ICharacterInstance *pCharacter, const AnimEventInstance &animEvent)
{
	// Send an entity event.
	SEntityEvent event(ENTITY_EVENT_ANIM_EVENT);
	event.nParam[0] = (INT_PTR)&animEvent;
	event.nParam[1] = (INT_PTR)pCharacter;
	m_pEntity->SendEvent(event);
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::ExpandCompoundSlot0()
{
	int i,nSlots = (int)m_slots.size();
	IStatObj::SSubObject *pSubObj;
	IStatObj *pStatObj;
	Matrix34 mtx;

	if (nSlots==1 && m_slots[0] && m_slots[0]->pStatObj && m_slots[0]->pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND)
	{	// if we currently have a compound cgf in slot 0, expand the this compound cgf into subobjects
		pStatObj = m_slots[0]->pStatObj; pStatObj->AddRef();
		mtx = GetSlotLocalTM(0,false);
		FreeSlot(0); nSlots = 0;
		for(i=0; i<pStatObj->GetSubObjectCount(); i++) if (pSubObj=pStatObj->GetSubObject(i))
		{
			m_slots.push_back(new CEntityObject);
			m_slots[nSlots]->SetLocalTM(mtx*pSubObj->localTM);
			m_slots[nSlots]->pStatObj = pSubObj->pStatObj;
			if (pSubObj->pStatObj)
			{
				pSubObj->pStatObj->AddRef();
				if (!pSubObj->bHidden)
					m_slots[nSlots]->flags |= ENTITY_SLOT_RENDER;
				if (strstr(pSubObj->properties,"entity"))
					m_slots[nSlots]->flags |= ENTITY_SLOT_BREAK_AS_ENTITY;
			}
			nSlots++;
		}
		pStatObj->Release();
		m_pEntity->AddFlags(ENTITY_FLAG_SLOTS_CHANGED);
	}
}

//////////////////////////////////////////////////////////////////////////
CEntityObject* CRenderProxy::AllocSlot( int nIndex )
{
	CEntityObject* pSlot = NULL;
	if (nIndex >= (int)m_slots.size())
	{
		m_slots.resize( nIndex+1 );
		m_slots[nIndex] = new CEntityObject;
	}
	else
	{
		if (m_slots[nIndex] == NULL)
		{
			m_slots[nIndex] = new CEntityObject;
		}
	}
	return m_slots[nIndex];
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::FreeSlot( int nIndex )
{
//	assert(nIndex>=0 && nIndex < (int)m_slots.size());
	
	if ((nIndex>=0 && nIndex < (int)m_slots.size()) && (m_slots[nIndex] != NULL))
	{
		CEntityObject *pSlot = m_slots[nIndex];
		delete pSlot;
		m_slots[nIndex] = NULL;

		if (nIndex == m_slots.size()-1)
		{
			// If Last slot, then cut away all empty slots at the end of the array.
			for (int i = nIndex; i >= 0 && m_slots[i] == NULL; i--)
				m_slots.pop_back();
		}
		// Check if any other slot references deleted slot as a parent, if yes reset it to NULL.
		for (uint32 i = 0; i < m_slots.size(); i++)
		{
			if (m_slots[i] && m_slots[i]->pParent == pSlot)
				m_slots[i]->pParent = NULL;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::FreeAllSlots()
{
	// Free all slots.
	for (uint32 i = 0; i < m_slots.size(); i++)
	{
		if (m_slots[i])
			delete m_slots[i];
	}
	m_slots.clear();
}

//////////////////////////////////////////////////////////////////////////
CEntityObject* CRenderProxy::GetOrMakeSlot( int &nSlot )
{
	if (nSlot < 0)
	{
		nSlot = m_slots.size();
	}
	return AllocSlot(nSlot);
}

//////////////////////////////////////////////////////////////////////////
CEntityObject* CRenderProxy::GetParentSlot( int nIndex ) const
{
	if (IsSlotValid(nIndex))
	{
		return Slot(nIndex)->pParent;
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
bool CRenderProxy::SetParentSlot( int nParentIndex,int nChildIndex )
{
	if (Slot(nParentIndex) && Slot(nChildIndex))
	{
		CEntityObject *pChildSlot = Slot(nChildIndex);
		pChildSlot->SetParent( Slot(nParentIndex) );
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
IStatObj *CRenderProxy::GetCompoundObj() const
{ 
	return m_slots.size()==1 && Slot(0)->pStatObj && Slot(0)->pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND ? Slot(0)->pStatObj : 0;
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::SetSlotLocalTM( int nIndex,const Matrix34 &localTM,int nWhyFlags )
{
	if (IsSlotValid(nIndex))
	{
		CEntityObject *pSlot = Slot(nIndex);
		Matrix34 identTM;
		identTM.SetIdentity();    
		if (pSlot->m_pXForm || !localTM.IsEquivalent(identTM, 0.001f))
		{
			Slot(nIndex)->SetLocalTM( localTM );
			Slot(nIndex)->OnXForm( m_pEntity );

			if (!(nWhyFlags & ENTITY_XFORM_NOT_REREGISTER)) // A special optimization for characters
			{
				// Invalidate bounding box.
				InvalidateBounds(true,true);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
const Matrix34& CRenderProxy::GetSlotWorldTM( int nIndex ) const
{
	static Matrix34 temp;
	if ((nIndex & ~ENTITY_SLOT_ACTUAL)<PARTID_CGA)
	{
		IStatObj *pCompObj;
		IStatObj::SSubObject *pSubObj;
		if (!(nIndex & ENTITY_SLOT_ACTUAL) && (pCompObj = GetCompoundObj()))
			return (pSubObj=pCompObj->GetSubObject(nIndex)) ? 
				(temp=Slot(0)->GetWorldTM(m_pEntity)*pSubObj->tm) : Slot(0)->GetWorldTM(m_pEntity);
		nIndex &= ~ENTITY_SLOT_ACTUAL;
		if (IsSlotValid(nIndex))
			return Slot(nIndex)->GetWorldTM(m_pEntity);
	} else
	{
		int nSlot,nSubSlot = nIndex%PARTID_CGA;
		nSlot = nIndex/PARTID_CGA-1;
		if (IsSlotValid(nSlot) && Slot(nSlot)->pCharacter)			
			return temp = Slot(nSlot)->GetWorldTM(m_pEntity) *	Matrix34(Slot(nSlot)->pCharacter->GetISkeletonPose()->GetAbsJointByID(nSubSlot));
	}
	temp.SetIdentity();
	return temp;
}

//////////////////////////////////////////////////////////////////////////
const Matrix34& CRenderProxy::GetSlotLocalTM( int nIndex, bool bRelativeToParent ) const
{
	static Matrix34 temp;
	if ((nIndex & ~ENTITY_SLOT_ACTUAL)>=PARTID_CGA)
	{
		int nSlot,nSubSlot = nIndex%PARTID_CGA;
		nSlot = nIndex/PARTID_CGA-1;
		if (IsSlotValid(nSlot) && Slot(nSlot)->pCharacter)
			if(Slot(nSlot)->pCharacter)
				return temp = Slot(nSlot)->GetLocalTM() *	Matrix34(Slot(nSlot)->pCharacter->GetISkeletonPose()->GetAbsJointByID(nSubSlot));
	}	else 
	{
		IStatObj *pCompObj;
		IStatObj::SSubObject *pSubObj;
		static Matrix34 temp;
		if (!(nIndex & ENTITY_SLOT_ACTUAL) && (pCompObj = GetCompoundObj()))
			return (pSubObj = pCompObj->GetSubObject(nIndex)) ? (temp = Slot(0)->GetLocalTM()*pSubObj->tm) : Slot(0)->GetLocalTM();
		else if (IsSlotValid(nIndex &= ~ENTITY_SLOT_ACTUAL))
		{// Check if this slot have parent
			if (Slot(nIndex)->pParent && !bRelativeToParent)
			{
				// Combine all local transforms hierarchy.
				temp = Slot(nIndex)->GetLocalTM();
				CEntityObject *pParentSlot = Slot(nIndex);
				while (pParentSlot = pParentSlot->pParent)
				{
					temp = pParentSlot->GetLocalTM() * temp;
				}
				return temp;
			}
			return Slot(nIndex)->GetLocalTM();
		}
	}
	temp.SetIdentity();
	return temp;
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::SetLocalBounds( const AABB &bounds,bool bDoNotRecalculate )
{
	m_localBBox = bounds;
	if (bDoNotRecalculate)
	{
		AddFlags(FLAG_BBOX_FORCED);
		ClearFlags(FLAG_BBOX_INVALID);
	}
	else
		ClearFlags(FLAG_BBOX_FORCED);
	AddFlags(FLAG_BBOX_VALID_LOCAL);
	InvalidateBounds( true,false );
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::InvalidateLocalBounds()
{
	InvalidateBounds( true,true );
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::CalcLocalBounds()
{
	// If local bounding box is forced from outside, do not calculate it automatically.
	if (CheckFlags(FLAG_BBOX_FORCED))
		return;

	bool bBBoxInitialized = false;
	AABB box;
	m_localBBox.Reset();
	int nSlots = GetSlotCount();
	for (int i = 0; i < nSlots; i++)
	{
		CEntityObject *pSlot = Slot(i);
		if (pSlot && (pSlot->flags & ENTITY_SLOT_RENDER)) // Only renderable slots contribute to the bounding box.
		{
			box.Reset();
			if (pSlot->GetLocalBounds( box ))
			{
				CheckIfBBoxValid( box );

				// Check if slot have transformation.
				if (pSlot->HaveLocalMatrix())
				{
					// If offset matrix is used transform slot bounds by slot local offset matrix.
					box.SetTransformedAABB( GetSlotLocalTM(i, false),box );
				}
				
				CheckIfBBoxValid( box );

				// Add local slot bounds to local rendering bounds.
				m_localBBox.Add(box.min);
				m_localBBox.Add(box.max);
				bBBoxInitialized = true;
			}
		}
	}
	
	if (!bBBoxInitialized)
	{
		// If bounding box not initialized, this entity should not be rendered.
		// @TODO: unregister from sector?
		AddFlags(FLAG_BBOX_INVALID);
		m_localBBox.min.Set(0,0,0);
		m_localBBox.max.Set(0,0,0);
	}
	else
	{
		ClearFlags(FLAG_BBOX_INVALID);
	}
	AddFlags(FLAG_BBOX_VALID_LOCAL);
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::GetLocalBounds( AABB &bbox )
{
	if (!CheckFlags(FLAG_BBOX_VALID_LOCAL))
	{
		CalcLocalBounds();
	}
	bbox = m_localBBox;
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::GetWorldBounds( AABB &bbox )
{
	if (!CheckFlags(FLAG_BBOX_VALID_LOCAL))
	{
		CalcLocalBounds();
	}
	bbox = m_localBBox;
	bbox.SetTransformedAABB( m_pEntity->GetWorldTM_Fast(),bbox );

	CheckIfBBoxValid( bbox );
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::ProcessEvent( SEntityEvent &event )
{
	switch (event.event)
	{
	case ENTITY_EVENT_XFORM:
		OnEntityXForm( event.nParam[0] );
		break;
	case ENTITY_EVENT_HIDE:
	case ENTITY_EVENT_INVISIBLE:
		OnHide(true);
		break;
	case ENTITY_EVENT_UNHIDE:
	case ENTITY_EVENT_VISIBLE:
		OnHide(false);
		break;
	case ENTITY_EVENT_NOT_SEEN_TIMEOUT:
		{
			if (CVar::pDebugNotSeenTimeout->GetIVal())
				CryLogAlways("RenderProxy not seen: id = \"0x%X\" entity = \"%s\" class = \"%s\"", m_pEntity->GetId(), m_pEntity->GetName(), m_pEntity->GetClass()->GetName());

			for (Slots::iterator it = m_slots.begin(),endit = m_slots.end(); it != endit; ++it) 
			{
				CEntityObject *pSlot = *it;
				if (pSlot && (pSlot->flags & ENTITY_SLOT_RENDER))
					pSlot->OnNotSeenTimeout();
				if (pSlot && pSlot->pCharacter)
					pSlot->pCharacter->KillAllSkeletonEffects();
			}
		}
		break;
	case ENTITY_EVENT_MATERIAL:
		SetCustomMaterial( (IMaterial*)(event.nParam[0]) );
		break;
	case ENTITY_EVENT_ANIM_EVENT:
		{
			const AnimEventInstance* pAnimEvent = reinterpret_cast<const AnimEventInstance*>(event.nParam[0]);
			if (pAnimEvent)
			{
				ICharacterInstance* pCharacter = reinterpret_cast<ICharacterInstance*>(event.nParam[1]);
				const char* eventName = (pAnimEvent ? pAnimEvent->m_EventName : 0);

				// If the event is an effect event, play the event.
				ISkeletonAnim* pSkeletonAnim = (pCharacter ? pCharacter->GetISkeletonAnim() : 0);
				ISkeletonPose* pSkeletonPose = (pCharacter ? pCharacter->GetISkeletonPose() : 0);
				if (!CheckFlags(FLAG_HIDDEN) && pSkeletonAnim && pAnimEvent->m_EventName && stricmp(pAnimEvent->m_EventName, "effect") == 0 && pAnimEvent->m_CustomParameter)
				{
					for (Slots::iterator it = m_slots.begin(),endit = m_slots.end(); it != endit; ++it) 
					{
						CEntityObject *pSlot = *it;
						if (pSlot && pSlot->pCharacter == pCharacter)
							pSlot->pCharacter->SpawnSkeletonEffect(pAnimEvent->m_AnimID, pAnimEvent->m_AnimPathName, pAnimEvent->m_CustomParameter, pAnimEvent->m_BonePathName, pAnimEvent->m_vOffset, pAnimEvent->m_vDir, pSlot->m_worldTM);
					}
				}
			}
		}
		break;
  case ENTITY_EVENT_RESET:
    OnReset();
    break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::OnEntityXForm( int nWhyFlags )
{
	if (!(nWhyFlags & ENTITY_XFORM_NOT_REREGISTER)) // A special optimization for characters
	{
		InvalidateBounds(false,true);
	}

	// Invalidate cached world matrices for all slots.
	for (uint32 i = 0,num = m_slots.size(); i < num; i++)
	{
		if (m_slots[i])
			m_slots[i]->OnXForm(m_pEntity);
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::OnHide( bool bHide )
{
	if (CheckFlags(FLAG_HIDDEN) != bHide)
	{
		//@TODO: uncomment this.
		//SetRndFlags(ERF_HIDDEN,bHide);
		if (bHide)
		{
			AddFlags(FLAG_HIDDEN);
			RegisterForRendering(false);
		}
		else
		{
			ClearFlags(FLAG_HIDDEN);
			RegisterForRendering(true);
		}
		OnEntityXForm(0);

		// update hidden flag for all slots rendering their stuff explicitly
		for (uint32 i = 0; i < m_slots.size(); i++)
		{
			if (m_slots[i])
			{
				IRenderNode* pRenderNode = m_slots[i]->pChildRenderNode;
				const uint32 mask = (1<<eERType_FogVolume) | (1<<eERType_ParticleEmitter) | (1<<eERType_Cloud) | (1<<eERType_VolumeObject);
				if (pRenderNode && (1<<pRenderNode->GetRenderNodeType()) & mask)
					pRenderNode->Hide( bHide );

				ICharacterInstance* pCharacter = m_slots[i]->pCharacter;
				if (pCharacter)
				{
					IAttachmentManager* pAttachmentManager = pCharacter->GetIAttachmentManager();
					if (pAttachmentManager)
					{
						for (int j=0; j<pAttachmentManager->GetAttachmentCount(); ++j)
						{
							IAttachment* pAttachment = pAttachmentManager->GetInterfaceByIndex(j);
							if (pAttachment)
							{
								IAttachmentObject* pIAttachmentObject = pAttachment->GetIAttachmentObject();
								if (pIAttachmentObject && IAttachmentObject::eAttachment_Effect == pIAttachmentObject->GetAttachmentType())
								{
									IParticleEmitter* pParticleEmitter = static_cast<CEffectAttachment*>(pIAttachmentObject)->GetEmitter();
									if (pParticleEmitter)
										pParticleEmitter->Activate(!bHide);
								}
							}
						}
					}
				}
      }
		}
	}
}

//////////////////////////////////////////////////////////////////

void CRenderProxy::OnReset( )
{
  m_nVisionParams = 0;
}

//////////////////////////////////////////////////////////////////

void CRenderProxy::UpdateMaterialLayersRendParams( SRendParams &pRenderParams )
{
  if( pRenderParams.dwFObjFlags & FOB_RENDER_INTO_SHADOWMAP )
  {
    // No update during shadow rendering
    pRenderParams.nMaterialLayers = 0; 
    pRenderParams.nMaterialLayersBlend = 0;  
    return;
  }

  uint8 nMaterialLayers = m_nMaterialLayers;
  if( nMaterialLayers || m_nMaterialLayersBlend)
  {
    // Automagically blend layers
    // 0: frozen, 1: wet, 2: cloak, 3: object/world space blend

    // MTL_LAYER_WET_MASK/MTL_LAYER_FROZEN_MASK, etcetc

    // Disable rain layer if inside vis area
    if( nMaterialLayers & MTL_LAYER_WET || m_nMaterialLayersBlend & MTL_LAYER_BLEND_WET )
    {
      IVisArea *pEntityVisArea = GetEntityVisArea();
      if( pEntityVisArea && !pEntityVisArea->IsPortal())
        nMaterialLayers &= ~MTL_LAYER_WET;
    }

    float fFrameTime = 255.0f * GetISystem()->GetITimer()->GetFrameTime();

    float nSrcBlend= 0.0f, nDstBlend = 0.0f;
    float nFinalBlendF = (nMaterialLayers&MTL_LAYER_FROZEN)? 255.0f : 0.0f;

    nSrcBlend = ((m_nMaterialLayersBlend&MTL_LAYER_BLEND_WET) >> 16);
    nDstBlend = (nMaterialLayers&MTL_LAYER_WET)? 255 : 0;
    float nFinalBlendW = nDstBlend;
    if( nSrcBlend != nDstBlend )
    {
      nFinalBlendW = (nSrcBlend < nDstBlend)?nSrcBlend + fFrameTime: nSrcBlend - fFrameTime;       
      nFinalBlendW = clamp_tpl<float>(nFinalBlendW, 0.0f, 255.0f);
    }

    nSrcBlend = ((m_nMaterialLayersBlend&MTL_LAYER_BLEND_CLOAK) >> 8);
    nDstBlend = (nMaterialLayers&MTL_LAYER_CLOAK)? 255.0f : 0.0f;
    float nFinalBlendC = nDstBlend;
    if( nSrcBlend != nDstBlend )
    {
      nFinalBlendC = (nSrcBlend < nDstBlend)?nSrcBlend + fFrameTime: nSrcBlend - fFrameTime;       
      nFinalBlendC = clamp_tpl<float>(nFinalBlendC, 0.0f, 255.0f);
    }

    if( nMaterialLayers&MTL_LAYER_CLOAK )
      pRenderParams.nMotionBlurAmount = 128;  

    nSrcBlend = ((m_nMaterialLayersBlend&MTL_LAYER_BLEND_DYNAMICFROZEN));
    nDstBlend = (nMaterialLayers&MTL_LAYER_DYNAMICFROZEN)? 255.0f : 0.0f; 
    float nFinalBlendDynF = nDstBlend;
    if( nSrcBlend != nDstBlend ) 
    {
      nFinalBlendDynF = (nSrcBlend < nDstBlend)?nSrcBlend + fFrameTime: nSrcBlend - fFrameTime;       
      nFinalBlendDynF = clamp_tpl<float>(nFinalBlendDynF, 0.0f, 255.0f);
    }

    m_nMaterialLayersBlend = (uint32) ((int_round(nFinalBlendF)) <<24) | ((int_round(nFinalBlendW))<<16) |((int_round(nFinalBlendC)) <<8)|(int_round(nFinalBlendDynF));  

    pRenderParams.nMaterialLayers = nMaterialLayers;   
    pRenderParams.nMaterialLayersBlend = m_nMaterialLayersBlend;  

    // Use object space for movable stuff
    pRenderParams.dwFObjFlags |= FOB_MTLLAYERS_OBJSPACE;
    if( m_nMaterialLayersBlend & 0x00ffffff )
      pRenderParams.dwFObjFlags |= FOB_MTLLAYERS_BLEND;       
  }
}

//////////////////////////////////////////////////////////////////////////
bool CRenderProxy::Render( const SRendParams& inRenderParams )
{
	FUNCTION_PROFILER( GetISystem(),PROFILE_ENTITY );

	if(m_nFlags & FLAG_BBOX_INVALID)
	{
		if(IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_pEntity->GetId()))
		{
			if(pActor->GetHealth() > 0)
				CryLogAlways("Invalid BBox for entity: %s", m_pEntity->GetName() );
		}
	}

	// Not draw if invalid bounding box.
	if (m_nFlags&(FLAG_HIDDEN|FLAG_BBOX_INVALID))
		return false;

	// Check if anything was actually drawn.
	bool bSomethingWasDrawn = false;

	// Make a local copy of render params as some of the parameters will be modified.
	SRendParams rParams(inRenderParams);

	// Lightmaps are not allowed for entities.
//	rParams.pLightMapInfo = 0;
//	rParams.pLMTCBuffer = 0;
	rParams.m_pLMData	=	0;
  
	//if (m_pShaderPublicParams)
 	//	m_pShaderPublicParams->AssignToRenderParams( rParams );

	// But VertexScattering is
 	if (m_bHasVertexScatterBuffer)
 	{
 		rParams.m_pScatterBuffer = m_arrScatterVertexBuffer;
 		rParams.mVertexScatterTransformZ = m_VertexScatterTransformZ;
 	}
 	else
		rParams.m_pScatterBuffer = NULL;

	if (m_pMaterial)
		rParams.pMaterial = m_pMaterial;

  if (m_nVisionParams)
    rParams.nVisionParams = m_nVisionParams;

	//if (CheckFlags(FLAG_HAS_ENV_LIGHTING))
		//rParams.dwFObjFlags |= FOB_ENVLIGHTING;

  rParams.nMotionBlurAmount = 0;
  if( CVar::pMotionBlur && CVar::pMotionBlur->GetIVal() > 1)
  {
    rParams.nMotionBlurAmount = 128;  
  }

  UpdateMaterialLayersRendParams( rParams );
  
  rParams.fAlpha = GetOpacity();
  
	if (m_nEntityFlags & ENTITY_FLAG_SEND_RENDER_EVENT)
	{
		SEntityEvent event(ENTITY_EVENT_RENDER);
		event.nParam[0] = (INT_PTR)&inRenderParams;
		m_pEntity->SendEvent(event);
		m_fLastSeenTime=GetISystem()->GetITimer()->GetCurrTime();
	}
	if (s_pTimeoutList && (m_nEntityFlags & ENTITY_FLAG_SEND_NOT_SEEN_TIMEOUT))
		s_pTimeoutList->ResetTimeout(m_entityId);


	//////////////////////////////////////////////////////////////////////////
	// Draw all slots.
	for (Slots::iterator it = m_slots.begin(),endit = m_slots.end(); it != endit; ++it) 
	{
		CEntityObject *pSlot = *it;
		if (pSlot && (pSlot->flags & ENTITY_SLOT_RENDER))
		{          
			if (pSlot->Render( m_pEntity, rParams, m_dwRndFlags,this ))
				bSomethingWasDrawn = true;
		}
		// Tbyte TODO: Currently only slot #0 is supported
		rParams.m_pScatterBuffer = NULL;
	}
 	//////////////////////////////////////////////////////////////////////////
	
	return bSomethingWasDrawn;
}

IStatObj *CRenderProxy::GetEntityStatObj( unsigned int nPartId, unsigned int nSubPartId, Matrix34 * pMatrix /* = NULL */, bool bReturnOnlyVisible /* = false */)
{
	if (!IsSlotValid(nPartId))
		return NULL;
	
  if (pMatrix)
		*pMatrix = GetSlotWorldTM(nPartId|ENTITY_SLOT_ACTUAL);
  
  if(!bReturnOnlyVisible || Slot(nPartId)->flags & ENTITY_SLOT_RENDER)
	  return Slot(nPartId)->pStatObj;

  return NULL;
}


ICharacterInstance *CRenderProxy::GetEntityCharacter( unsigned int nSlot, Matrix34 * pMatrix /* = NULL */, bool bReturnOnlyVisible /* = false */)
{
	if (!IsSlotValid(nSlot))
		return NULL;
	if (pMatrix)
		*pMatrix = GetSlotWorldTM(nSlot);
  
  if(!bReturnOnlyVisible || Slot(nSlot)->flags & ENTITY_SLOT_RENDER)
  	return Slot(nSlot)->pCharacter;

  return NULL;
}


//////////////////////////////////////////////////////////////////////////
void CRenderProxy::Update( SEntityUpdateContext &ctx )
{
	if (!CheckFlags(FLAG_UPDATE))
		return;

	FUNCTION_PROFILER( GetISystem(),PROFILE_ENTITY );

	bool bBoundsChanged = false;
	//////////////////////////////////////////////////////////////////////////
	// Update all slots.
	for (uint32 i = 0; i < m_slots.size(); i++)
	{    
		if (m_slots[i] && m_slots[i]->bUpdate)
		{
			bool bSlotBoundsChanged = false;
			
		/*
			if (!CheckFlags(FLAG_PREUPDATE_ANIM))
			{
				m_slots[i]->PreUpdateCharacter( m_pEntity, CheckFlags(FLAG_NOW_VISIBLE), bSlotBoundsChanged, i );
				m_slots[i]->PostUpdateCharacter( m_pEntity, CheckFlags(FLAG_NOW_VISIBLE), bSlotBoundsChanged, i );
			}*/
			
			m_slots[i]->Update( m_pEntity,CheckFlags(FLAG_NOW_VISIBLE),bSlotBoundsChanged );
			
			if (bSlotBoundsChanged)
				bBoundsChanged = true;
		}
	}
	if (bBoundsChanged)
		InvalidateBounds( true,false );
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::SetCustomMaterial( IMaterial *pMaterial )
{
	m_pMaterial = pMaterial;

	if (m_nFlags & FLAG_HAS_LIGHTS)
	{
		for (uint32 i = 0; i < m_slots.size(); i++)
		{
			CEntityObject *pSlot = m_slots[i];
			if (pSlot && pSlot->pLight)
			{
				if (pSlot->pMaterial)
					pSlot->pLight->SetMaterial( pSlot->pMaterial );
				pSlot->pLight->SetMaterial( m_pMaterial );
			}
		}
	}
	else if (m_nFlags & FLAG_HAS_CHILDRENDERNODES)
	{
		for (uint32 i = 0; i < m_slots.size(); i++)
		{
			CEntityObject* pSlot(m_slots[i]);
			IRenderNode* pChildRenderNode(pSlot ? pSlot->pChildRenderNode : 0);
			if (pChildRenderNode)
				pChildRenderNode->SetMaterial(m_pMaterial);
		}
	}

	if (!pMaterial)
		pMaterial = GetRenderMaterial();

	if (pMaterial && pMaterial->IsSubSurfScatterCaster())
		m_dwRndFlags |= ERF_SUBSURFSCATTER;
	else
		m_dwRndFlags &= ~ERF_SUBSURFSCATTER;

	if (pMaterial && pMaterial->GetFlags() & MTL_FLAG_VISIBLE_ONLY_IN_EDITOR)
		m_dwRndFlags |= ERF_VISIBLE_ONLY_IN_EDITOR;
	else
		m_dwRndFlags &= ~ERF_VISIBLE_ONLY_IN_EDITOR;
};

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::InvalidateBounds( bool bLocal,bool bWorld )
{
	if (bLocal)
		ClearFlags(FLAG_BBOX_VALID_LOCAL);
	
	// Check if initialization of entity already finished.
	if (CheckFlags(FLAG_POST_INIT))
	{
		// World bounding box changed.
		RegisterForRendering(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::RegisterForRendering( bool bEnable )
{
	if ( bEnable )
	{
		RegisterIn3DEngine();
	}
	else
	{
		UnRegisterFrom3DEngine();
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::RegisterIn3DEngine()
{
	bool hidden = CheckFlags( FLAG_HIDDEN );
	if ( hidden )
	{
		UnRegisterFrom3DEngine();
		return;
	}

	bool bBoxValidLocal = CheckFlags( FLAG_BBOX_VALID_LOCAL );
	if ( ! bBoxValidLocal )
	{
		CalcLocalBounds();
	}

	bool invalidBBox = ( m_localBBox.IsEmpty() || CheckFlags( FLAG_BBOX_INVALID ) );
	if ( invalidBBox )
	{
		UnRegisterFrom3DEngine();
		return;
	}

	m_WSBBox.SetTransformedAABB( m_pEntity->GetWorldTM_Fast(), m_localBBox );

	AddFlags( FLAG_REGISTERED_IN_3DENGINE );
	GetI3DEngine()->RegisterEntity( this );
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::UnRegisterFrom3DEngine()
{
	bool registeredIn3DEngine = CheckFlags( FLAG_REGISTERED_IN_3DENGINE );
	if ( ! registeredIn3DEngine )
	{
		return;
	}
	
	GetI3DEngine()->FreeRenderNodeState( this ); // internally calls UnRegisterEntity(this)
	ClearFlags( FLAG_REGISTERED_IN_3DENGINE );
}

//////////////////////////////////////////////////////////////////////////
int CRenderProxy::GetSlotId( CEntityObject *pSlot ) const
{
	for (uint32 i = 0; i < m_slots.size(); i++)
	{
		if (m_slots[i] == pSlot)
			return i;
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////
bool CRenderProxy::GetSlotInfo( int nIndex,SEntitySlotInfo &slotInfo ) const
{
	if (!IsSlotValid(nIndex))
		return false;
	CEntityObject *pSlot = Slot(nIndex);
	slotInfo.nFlags = pSlot->flags;
	if (pSlot->pParent)
		slotInfo.nParentSlot = GetSlotId(pSlot->pParent);
	else
		slotInfo.nParentSlot = -1;

	slotInfo.pStatObj = pSlot->pStatObj;
	slotInfo.pCharacter = pSlot->pCharacter;
	slotInfo.pLight = pSlot->pLight;
	slotInfo.pMaterial = pSlot->pMaterial;
	slotInfo.pLocalTM = &pSlot->GetLocalTM();
	slotInfo.pWorldTM = &pSlot->GetWorldTM(m_pEntity);
	slotInfo.pChildRenderNode = pSlot->pChildRenderNode;
  slotInfo.pParticleEmitter = pSlot->GetParticleEmitter();
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::SetSlotFlags( int nSlot,uint32 nFlags )
{
	if (nSlot < 0)
	{
		// All slots.
		for (Slots::iterator it = m_slots.begin(),endit = m_slots.end(); it != endit; ++it) 
		{
			CEntityObject *pSlot = *it;
			if (pSlot)
				pSlot->SetFlags(nFlags);
		}
	}
	else if (IsSlotValid(nSlot))
	{
		Slot(nSlot)->SetFlags(nFlags);
	}
	//if (nFlags & (ENTITY_SLOT_RENDER|ENTITY_SLOT_RENDER_NEAREST))
	{
		// If any slot to be rendered, make sure it is registered in 3d engine.
		//if (!CheckFlags(FLAG_REGISTERED_IN_3DENGINE))
		{
			InvalidateBounds(true,true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
uint32 CRenderProxy::GetSlotFlags( int nSlot )
{
	if (IsSlotValid(nSlot))
	{
		return Slot(nSlot)->flags;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::SetSlotMaterial( int nSlot,IMaterial *pMaterial )
{
	if (IsSlotValid(nSlot))
	{
    CEntityObject *pSlot = Slot(nSlot);
		pSlot->pMaterial = pMaterial;

    if (pSlot->pLight)
      pSlot->pLight->SetMaterial(pMaterial);
	}
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CRenderProxy::GetSlotMaterial( int nSlot )
{
	if (IsSlotValid(nSlot))
	{
		return Slot(nSlot)->pMaterial;
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
int CRenderProxy::SetSlotGeometry( int nSlot,IStatObj *pStatObj )
{
	if (m_nInternalFlags & DECAL_OWNER)
		m_nInternalFlags |= UPDATE_DECALS;

	if ((nSlot & ~ENTITY_SLOT_ACTUAL)<PARTID_CGA)
	{
		IStatObj *pCompObj=0;
		if (!(nSlot & ENTITY_SLOT_ACTUAL))
			pCompObj = GetCompoundObj();
		else
			nSlot &= ~ENTITY_SLOT_ACTUAL;
		if (pCompObj && (!pStatObj || !(pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND)))
		{
			if (!(pCompObj->GetFlags() & STATIC_OBJECT_GENERATED))
			{
				IStatObj *pNewCompObj = pCompObj->Clone(false,false,true);
				pNewCompObj->SetFlags( pNewCompObj->GetFlags()|STATIC_OBJECT_GENERATED );
				pNewCompObj->AddRef();
				Slot(0)->ReleaseObjects();
				Slot(0)->pStatObj = pCompObj = pNewCompObj;
			}
			IStatObj::SSubObject *pSubObj;
			if (!(pSubObj = pCompObj->GetSubObject(nSlot)))
			{
				pCompObj->SetSubObjectCount(nSlot+1);
				pSubObj = pCompObj->GetSubObject(nSlot);
			}	else if (pSubObj->pStatObj==pStatObj) 
			{
				pCompObj->Invalidate(false);
				return nSlot;
			}
			if (!(pSubObj->bHidden = pStatObj==0))
				pStatObj->AddRef();
			pSubObj->pStatObj = pStatObj;
			pCompObj->Invalidate(false);
		}	else
		{
			CEntityObject *pSlot = GetOrMakeSlot(nSlot);

			// Check if loading the same object.
			if (pSlot->pStatObj == pStatObj)
			{
				return nSlot;
			}

			if (pStatObj)
				pStatObj->AddRef();

			pSlot->ReleaseObjects();

			if (pStatObj)
				pSlot->flags |= ENTITY_SLOT_RENDER;
			else
				pSlot->flags &= ~ENTITY_SLOT_RENDER;

			pSlot->pStatObj = pStatObj;
			pSlot->bUpdate = false;
		}
	} else
	{
		int nCGASlot=nSlot/PARTID_CGA-1, nSubSlot=nSlot%PARTID_CGA;
		CEntityObject *pSlot = GetOrMakeSlot(nCGASlot);

		if (!pSlot->pCharacter || pSlot->pCharacter->GetISkeletonPose()->GetStatObjOnJoint(nSubSlot)==pStatObj)
			return nSlot;
		pSlot->pCharacter->GetISkeletonPose()->SetStatObjOnJoint(nSubSlot,pStatObj);
	}

	InvalidateBounds( true,true );

	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
int CRenderProxy::SetSlotCharacter( int nSlot, ICharacterInstance* pCharacter )
{
	CEntityObject *pSlot = GetOrMakeSlot(nSlot);

	// Check if loading the same character
	if (pSlot->pCharacter == pCharacter)
	{
		return nSlot;
	}

	if (pCharacter)
		pCharacter->AddRef();

	pSlot->ReleaseObjects();

	if (pCharacter)
	{
		pSlot->flags |= ENTITY_SLOT_RENDER;
		pSlot->bUpdate = true;
		m_nFlags |= FLAG_UPDATE; // For Characters we need render proxy to be updated.
		pCharacter->SetFlags( pCharacter->GetFlags() | CS_FLAG_UPDATE );

		// Characters with morphs have buffers that should be cleaned up after a period of time, so set this flag to receive
		// callbacks when we are not rendered for a certain period of time.
		if (m_pEntity && CheckCharacterForMorphs(pCharacter))
		{
			m_pEntity->SetFlags(m_pEntity->GetFlags() | ENTITY_FLAG_SEND_NOT_SEEN_TIMEOUT);
			if (CVar::pDebugNotSeenTimeout->GetIVal())
				CryLogAlways("CRenderProxy::SetSlotCharacter(): Setting ENTITY_FLAG_SEND_NOT_SEEN_TIMEOUT. id = \"0x%X\" entity = \"%s\" class = \"%s\"", m_pEntity->GetId(), m_pEntity->GetName(), m_pEntity->GetClass()->GetName());
		}
	}
	else
		pSlot->flags &= ~ENTITY_SLOT_RENDER;

	pSlot->pCharacter = pCharacter;
	pSlot->bUpdate = false;

	InvalidateBounds( true,true );

	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
int CRenderProxy::LoadGeometry( int nSlot,const char *sFilename,const char *sGeomName,int nLoadFlags )
{
	CEntityObject *pSlot = GetOrMakeSlot(nSlot);

	if (!sFilename || (sFilename[0]==0))
	{
		EntityWarning( "[RenderProxy::LoadGeometry] Called with empty filename, Entity: %s",m_pEntity->GetEntityTextDescription() );
		return -1;
	}

	// Check if loading the same object.
	if ((pSlot->pStatObj) && (pSlot->pStatObj->IsSameObject( sFilename,sGeomName )))
	{
		return nSlot;
	}

	IStatObj *pStatObj;
	if (sGeomName && sGeomName[0])
	{
		IStatObj::SSubObject *pSubObject = 0;
		pStatObj = GetI3DEngine()->LoadStatObj( sFilename,sGeomName,&pSubObject );
		if (pStatObj)
		{
			// Will keep a reference to the stat obj.
			pStatObj->AddRef();
		}
		else
		{
			EntityFileWarning( sFilename,"Failed to load sub-object geometry (%s)", sGeomName );
			return -1;
		}
		if (pSubObject && !pSubObject->bIdentityMatrix)
		{
			// Set sub object matrix into the slot transformation matrix.
			pSlot->SetLocalTM(pSubObject->tm);
		}
	}
	else
	{
		pStatObj = GetI3DEngine()->LoadStatObj( sFilename );
		if (pStatObj)
		{
			pStatObj->AddRef();
		}
	}

	pSlot->ReleaseObjects();
	pSlot->pStatObj = pStatObj;
	pSlot->flags |= ENTITY_SLOT_RENDER;
	pSlot->bUpdate = false;

	UpdateEntityFlags();

	InvalidateBounds( true,true );

	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
int CRenderProxy::LoadCharacter( int nSlot,const char *sFilename,int nLoadFlags )
{
	CEntityObject *pSlot = GetOrMakeSlot(nSlot);

	if (!sFilename || (sFilename[0]==0))
	{
		EntityWarning( "[RenderProxy::LoadCharacter] Called with empty filename, Entity: %s",m_pEntity->GetEntityTextDescription() );
		return -1;
	}

	// Check if loading the same object.
	if ((pSlot->pCharacter) && (pSlot->pCharacter->IsModelFileEqual( sFilename )))
	{
		ISkeletonPose* pISkeletonPose = pSlot->pCharacter->GetISkeletonPose();
		pISkeletonPose->DestroyCharacterPhysics();
		pISkeletonPose->SetDefaultPose();
		//pISkeleton->Update(m_pEntity->GetWorldTM());
		return nSlot;
	}

	// First must make the character, then release previous one.
	ICharacterInstance *pCharacter = NULL;

	//check the file extension to use the correct function: MakeCharacter for cgf/chr, LoadCharacterDefinition for cdf
	pCharacter = GetISystem()->GetIAnimationSystem()->CreateInstance( sFilename );
	if (pCharacter)
		pCharacter->AddRef();

	pSlot->ReleaseObjects();
	if (pCharacter)
	{
		pSlot->pCharacter = pCharacter;
		pSlot->flags |= ENTITY_SLOT_RENDER;
		pSlot->bUpdate = true;
		m_nFlags |= FLAG_UPDATE; // For Characters we need render proxy to be updated.

		pCharacter->SetFlags( pCharacter->GetFlags() | CS_FLAG_UPDATE );

		// Register ourselves to listen for animation events coming from the character.
		if (ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim())
			pSkeletonAnim->SetEventCallback(AnimEventCallback, this);

		// Characters with morphs have buffers that should be cleaned up after a period of time, so set this flag to receive
		// callbacks when we are not rendered for a certain period of time.
		if (m_pEntity && CheckCharacterForMorphs(pCharacter))
		{
			m_pEntity->SetFlags(m_pEntity->GetFlags() | ENTITY_FLAG_SEND_NOT_SEEN_TIMEOUT);
			if (CVar::pDebugNotSeenTimeout->GetIVal())
				CryLogAlways("CRenderProxy::SetSlotCharacter(): Setting ENTITY_FLAG_SEND_NOT_SEEN_TIMEOUT. id = \"0x%X\" entity = \"%s\" class = \"%s\"", m_pEntity->GetId(), m_pEntity->GetName(), m_pEntity->GetClass()->GetName());
		}
		InvalidateBounds( true,true );
	}
	else
	{
		pSlot->flags &= ~ENTITY_SLOT_RENDER;
		pSlot->bUpdate = false;
		RegisterForRendering(false);
	}

	return (pCharacter) ? nSlot : -1;
}

//////////////////////////////////////////////////////////////////////////
int	CRenderProxy::SetParticleEmitter(int nSlot, IParticleEmitter* pEmitter, bool bSerialize)
{
	CEntityObject *pSlot = GetOrMakeSlot(nSlot);
	pSlot->ReleaseObjects();
	pSlot->bSerialize = bSerialize;
	pEmitter->AddRef();
	pSlot->pChildRenderNode = pEmitter;

	pEmitter->SetEntity(m_pEntity, nSlot);

	if (pSlot->pMaterial)
	{
		pSlot->pChildRenderNode->SetMaterial(pSlot->pMaterial);
	}
	m_nFlags |= FLAG_UPDATE; // For Particles we need render proxy to be updated.
	m_nFlags |= FLAG_HAS_PARTICLES; //Mark as particles emitter

	pEmitter->SetViewDistRatio(GetViewDistRatio());
	InvalidateBounds( true,true );

	UpdateEntityFlags();

	return nSlot;
}

int	CRenderProxy::LoadParticleEmitter( int nSlot, IParticleEffect *pEffect, SpawnParams const* pParams, bool bPrime, bool bSerialize)
{
	assert(pEffect);

	// Spawn/attach params.
	SpawnParams params( pEffect->GetParticleParams().eAttachType, pEffect->GetParticleParams().eAttachForm );
	if (pParams)
		params = *pParams;

	Matrix34 const& TM = nSlot < 0 ? m_pEntity->GetWorldTM() : GetOrMakeSlot(nSlot)->GetWorldTM(m_pEntity);
	IParticleEmitter* pEmitter = pEffect->Spawn( params.bIndependent, TM );
	if(m_pEntity && !strnicmp("train", m_pEntity->GetName(),5))
		pEmitter->SetMoveRelEmitterTrue();

	pEmitter->SetSpawnParams(params);
	if (bPrime)
		// Set emitter as though it's been running indefinitely.
		pEmitter->Prime();

	if (params.bIndependent)
	{
		pEmitter->SetEntity(m_pEntity, nSlot);
		return nSlot;
	}
	else
		return SetParticleEmitter(nSlot, pEmitter, bSerialize);
}

//////////////////////////////////////////////////////////////////////////
int CRenderProxy::LoadLight( int nSlot,CDLight *pLight )
{
	assert(pLight);
	CEntityObject *pSlot = GetOrMakeSlot(nSlot);

	if (pSlot->pMaterial)
	{
		//pLight->pMaterial = pSlot->pMaterial;
	}

	if (!pSlot->pLight)
	{
		pSlot->ReleaseObjects();
		pSlot->pLight = GetI3DEngine()->CreateLightSource();
	}

	if (pSlot->pLight)
	{
		pLight->m_nEntityId = m_pEntity->GetId();
		pSlot->pLight->SetLightProperties(*pLight);
		if (pSlot->pMaterial)
			pSlot->pLight->SetMaterial( pSlot->pMaterial );
		else
			pSlot->pLight->SetMaterial( m_pMaterial );
		
		pSlot->pLight->SetViewDistRatio(GetViewDistRatio());
		pSlot->pLight->SetLodRatio(GetLodRatio());
	}

	m_nFlags |= FLAG_HAS_LIGHTS;
	
	// Update light positions.
	pSlot->OnXForm( m_pEntity );
	
	pSlot->flags &= (~ENTITY_SLOT_RENDER);
	pSlot->bUpdate = false;
	InvalidateBounds( true,true );

	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
int CRenderProxy::LoadCloud( int nSlot,const char *sFilename )
{
	CEntityObject *pSlot = GetOrMakeSlot(nSlot);

	pSlot->ReleaseObjects();
	ICloudRenderNode *pCloud = (ICloudRenderNode*)GetI3DEngine()->CreateRenderNode( eERType_Cloud );

	pSlot->pChildRenderNode = pCloud;

	pCloud->LoadCloud( sFilename );
	pCloud->SetMaterial( m_pMaterial );

	// Update slot position.
	pSlot->OnXForm( m_pEntity );

	m_nFlags |= FLAG_HAS_CHILDRENDERNODES;

	//pSlot->flags |= ENTITY_SLOT_RENDER;
	pSlot->bUpdate = false;
	InvalidateBounds( true,true );

	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
int CRenderProxy::SetCloudMovementProperties(int nSlot, const SCloudMovementProperties& properties)
{
	CEntityObject *pSlot = GetOrMakeSlot(nSlot);
	IRenderNode* pRenderNode(pSlot->pChildRenderNode);
	if (pRenderNode && pRenderNode->GetRenderNodeType() == eERType_Cloud)
	{
		ICloudRenderNode* pCloud((ICloudRenderNode*)pRenderNode);
		pCloud->SetMovementProperties(properties);
	}
	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
int CRenderProxy::LoadFogVolume( int nSlot, const SFogVolumeProperties& properties )
{
	CEntityObject *pSlot = GetOrMakeSlot( nSlot );

	IFogVolumeRenderNode* pFogVolume( (IFogVolumeRenderNode*) pSlot->pChildRenderNode );
	if( 0 == pFogVolume )
	{
		pSlot->ReleaseObjects();
		
		pFogVolume = (IFogVolumeRenderNode*) GetI3DEngine()->CreateRenderNode( eERType_FogVolume );
		pSlot->pChildRenderNode = pFogVolume;
	}

	assert( 0 != pFogVolume );
	pFogVolume->SetFogVolumeProperties( properties );

	// Update slot position.
	pSlot->OnXForm( m_pEntity );

	//pSlot->flags |= ENTITY_SLOT_RENDER;
	pSlot->bUpdate = false;

	SetLocalBounds( AABB(-properties.m_size*0.5f,properties.m_size*0.5f),true );

	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
int CRenderProxy::FadeGlobalDensity( int nSlot, float fadeTime, float newGlobalDensity )
{
	CEntityObject *pSlot = GetOrMakeSlot(nSlot);
	IRenderNode* pRenderNode(pSlot->pChildRenderNode);
	if (pRenderNode && pRenderNode->GetRenderNodeType() == eERType_FogVolume)
	{
		IFogVolumeRenderNode* pFogVolume((IFogVolumeRenderNode*)pRenderNode);
		pFogVolume->FadeGlobalDensity(fadeTime, newGlobalDensity);
	}
	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
int CRenderProxy::LoadVolumeObject(int nSlot, const char* sFilename)
{
	CEntityObject *pSlot = GetOrMakeSlot(nSlot);

	pSlot->ReleaseObjects();
	IVolumeObjectRenderNode* pVolObj = (IVolumeObjectRenderNode*) GetI3DEngine()->CreateRenderNode(eERType_VolumeObject);

	pSlot->pChildRenderNode = pVolObj;

	pVolObj->LoadVolumeData(sFilename);
	if (m_pMaterial)
		pVolObj->SetMaterial(m_pMaterial);

	// Update slot position
	pSlot->OnXForm(m_pEntity);

	m_nFlags |= FLAG_HAS_CHILDRENDERNODES;

	//pSlot->flags |= ENTITY_SLOT_RENDER;
	pSlot->bUpdate = false;
	InvalidateBounds(true, true);

	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
int CRenderProxy::SetVolumeObjectMovementProperties(int nSlot, const SVolumeObjectMovementProperties& properties)
{
	CEntityObject *pSlot = GetOrMakeSlot(nSlot);
	IRenderNode* pRenderNode(pSlot->pChildRenderNode);
	if (pRenderNode && pRenderNode->GetRenderNodeType() == eERType_VolumeObject)
	{
		IVolumeObjectRenderNode* pVolObj((IVolumeObjectRenderNode*)pRenderNode);
		pVolObj->SetMovementProperties(properties);
	}
	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
ICharacterInstance* CRenderProxy::GetCharacter( int nSlot )
{
	nSlot = nSlot & (~ENTITY_SLOT_ACTUAL);
	if (!IsSlotValid(nSlot))
		return NULL;
	return Slot(nSlot)->pCharacter;
}

//////////////////////////////////////////////////////////////////////////
IStatObj* CRenderProxy::GetStatObj( int nSlot )
{
	if ((nSlot & ~ENTITY_SLOT_ACTUAL)<PARTID_CGA)	
	{
		IStatObj *pCompObj;
		if (!(nSlot & ENTITY_SLOT_ACTUAL) && (pCompObj = GetCompoundObj()))
			return (unsigned int)pCompObj->GetSubObjectCount()>(unsigned int)nSlot ? 
							pCompObj->GetSubObject(nSlot)->pStatObj : 0;
		nSlot &= ~ENTITY_SLOT_ACTUAL;
		if (!IsSlotValid(nSlot))
			return NULL;
		return Slot(nSlot)->pStatObj;
	} else
	{
		int nSubSlot = nSlot%PARTID_CGA;
		nSlot = nSlot/PARTID_CGA-1;
		if (!IsSlotValid(nSlot) || !Slot(nSlot)->pCharacter)
			return NULL;
		return Slot(nSlot)->pCharacter->GetISkeletonPose()->GetStatObjOnJoint(nSubSlot);
	}
}

//////////////////////////////////////////////////////////////////////////
IParticleEmitter* CRenderProxy::GetParticleEmitter( int nSlot )
{
	if (!IsSlotValid(nSlot))
		return NULL;
	
	return Slot(nSlot)->GetParticleEmitter();
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CRenderProxy::GetRenderMaterial( int nSlot )
{
	CEntityObject *pSlot = NULL;
	if (nSlot>=PARTID_CGA)
		nSlot = nSlot/PARTID_CGA-1;
	if (nSlot >= 0)
		pSlot = GetSlot(nSlot);
	else
	{
		for (uint32 i = 0; i < m_slots.size(); i++)
		{
			if (m_slots[i] && (m_slots[i]->flags & ENTITY_SLOT_RENDER))
			{
				pSlot = m_slots[i];
				break;
			}
		}
	}

	if (!pSlot)
		return NULL;

	if (pSlot->pMaterial)
		return pSlot->pMaterial;
	
	if (m_pMaterial)
		return m_pMaterial;

	if (pSlot->pStatObj)
		return pSlot->pStatObj->GetMaterial();

	if (pSlot->pCharacter)
		return pSlot->pCharacter->GetMaterial();

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::SerializeXML( XmlNodeRef &entityNode,bool bLoading )
{
	if (bLoading)
	{
		int nLodRatio;
		int nViewDistRatio;
		if (entityNode->getAttr("LodRatio",nLodRatio))
		{
			// Change LOD ratio.
			SetLodRatio(nLodRatio);
		}
		if (entityNode->getAttr("ViewDistRatio",nViewDistRatio))
		{
			// Change ViewDistRatio ratio.
			SetViewDistRatio(nViewDistRatio);
		}
		uint32 nMaterialLayers = m_nMaterialLayers;
		if (entityNode->getAttr("MatLayersMask",nMaterialLayers))
		{
			m_nMaterialLayers = nMaterialLayers;
		}
	}
}

#define DEBUG_PARTICLES	0

//////////////////////////////////////////////////////////////////////////
bool CRenderProxy::NeedSerialize()
{
	return (m_nFlags&FLAG_HAS_PARTICLES) != 0 && GetMaterialLayers() != 0;
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::Serialize( TSerialize ser )
{
	// Particles.
	if (ser.GetSerializationTarget() != eST_Network)
	{
		if (ser.BeginOptionalGroup("RenderProxy",true))
		{
			if (ser.IsWriting())
			{
				//////////////////////////////////////////////////////////////////////////
				// Saving.
				//////////////////////////////////////////////////////////////////////////
				{
					int nLodRatio = GetLodRatio();
					int nViewDistRatio = GetViewDistRatio();
					uint32 nMaterialLayers = GetMaterialLayers();
					//ser.Value("LodR",nLodRatio);
					//ser.Value("ViewDistR",nViewDistRatio);
					ser.Value("MatLayers",nMaterialLayers);
				}

				if (ser.BeginOptionalGroup("Particles",(m_nFlags&FLAG_HAS_PARTICLES) != 0 ))
				{
					// Count serializable
					int nSerSlots = 0;
					for (int i = 0; i < m_slots.size(); ++i)
					{
						IParticleEmitter* pEmitter = GetParticleEmitter(i);
						if (pEmitter && pEmitter->IsAlive())
						{
							if (m_slots[i]->bSerialize)
							{
								ser.BeginGroup("Slot");
								++nSerSlots;
								ser.Value("slot", i);
								pEmitter->Serialize(ser);
								ser.EndGroup();
							}
						}
					}
					ser.Value("count", nSerSlots);
					ser.EndGroup(); //Particles
				}
			}
			else
			{
				//////////////////////////////////////////////////////////////////////////
				// Loading.
				//////////////////////////////////////////////////////////////////////////
				{
					int nLodRatio=0;
					int nViewDistRatio=0;
					uint32 nMaterialLayers = 0;
					//ser.Value("LodR",nLodRatio);
					//ser.Value("ViewDistR",nViewDistRatio);
					ser.Value("MatLayers",nMaterialLayers);

					// Change lod ratio.
					//SetLodRatio(nLodRatio);
          //SetViewDistRatio(nViewDistRatio);
					SetMaterialLayers(nMaterialLayers);
				}

				if (ser.BeginOptionalGroup("Particles",true))
				{
					int nSerSlots = 0;
					ser.Value("count", nSerSlots);
					int nCurSlot = -1;
					for (int i = 0; i < nSerSlots; ++i)
					{
						ser.BeginGroup("Slot");
						int nSlot = -1;
						ser.Value("slot", nSlot);
						if (nSlot >= 0)
						{
							CEntityObject *pSlot = GetOrMakeSlot(nSlot);
							IParticleEmitter* pSerEmitter = GetI3DEngine()->CreateParticleEmitter( false, pSlot->GetWorldTM(m_pEntity), ParticleParams() );
							assert(pSerEmitter);
							pSerEmitter->Serialize(ser);
							pSerEmitter->Activate(1);
							SetParticleEmitter(nSlot, pSerEmitter, true);
						}
						ser.EndGroup();
					}
					ser.EndGroup();//Particles
				}
			}
			
			ser.EndGroup(); // RenderProxy
		}
	}
	
#if DEBUG_PARTICLES
	for (int i = 0; i < m_slots.size(); ++i)
	{
		IParticleEmitter* pEmitter = GetParticleEmitter(i);
		if (pEmitter && pEmitter->IsAlive())
			CryWarning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_COMMENT, 
				"Entity Particle: %s, slot %d, serialize %d: %s", 
				m_pEntity->GetName(), i, m_slots[i]->bSerialize, pEmitter->GetDebugString('s').c_str());
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
/*IShaderPublicParams* CRenderProxy::GetShaderPublicParams( bool bCreate )
{
	if (m_pShaderPublicParams == NULL && bCreate)
		m_pShaderPublicParams = gEnv->pRenderer->CreateShaderPublicParams();

	return m_pShaderPublicParams;
}*/

void CRenderProxy::SetMaterialLayersMask( uint8 nMtlLayers ) 
{ 
  uint8 old = GetMaterialLayersMask();
  SetMaterialLayers(nMtlLayers);

  if (nMtlLayers != old)
  {
    SEntityEvent event(ENTITY_EVENT_MATERIAL_LAYER);
    event.nParam[0] = nMtlLayers;
    event.nParam[1] = old;
    m_pEntity->SendEvent(event);
  }
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::SetMaterial( IMaterial *pMat )
{
	m_pEntity->SetMaterial(pMat);
}
//////////////////////////////////////////////////////////////////////////
IMaterial* CRenderProxy::GetMaterial(Vec3 * pHitPos)
{
	return m_pMaterial;
};

int CRenderProxy::GetEditorObjectId() const
{
	return m_pEntity->GetGuid();
}

const char* CRenderProxy::GetEntityClassName() const { return m_pEntity->GetClass()->GetName(); };
Vec3 CRenderProxy::GetPos(bool bWorldOnly) const { return bWorldOnly ? m_pEntity->GetWorldPos() : m_pEntity->GetPos(); }
const Ang3& CRenderProxy::GetAngles(int realA) const { static Ang3 angles(m_pEntity->GetWorldAngles()); return angles; };
float CRenderProxy::GetScale() const { return m_pEntity->GetScale().x; };
const char* CRenderProxy::GetName() const { return m_pEntity->GetName(); };
void CRenderProxy::GetRenderBBox( Vec3 &mins,Vec3 &maxs ) { mins = m_WSBBox.min; maxs = m_WSBBox.max; }
void CRenderProxy::GetBBox( Vec3 &mins,Vec3 &maxs ) { mins = m_WSBBox.min; maxs = m_WSBBox.max; }

float CRenderProxy::GetMaxViewDist()
{
	if (!(m_nEntityFlags & ENTITY_FLAG_CUSTOM_VIEWDIST_RATIO))
	{
		static ICVar * p_e_view_dist_min = GetISystem()->GetIConsole()->GetCVar("e_view_dist_min");
		static ICVar * p_e_view_dist_ratio = GetISystem()->GetIConsole()->GetCVar("e_view_dist_ratio");
		static ICVar * p_e_view_dist_ratio_detail = GetISystem()->GetIConsole()->GetCVar("e_view_dist_ratio_detail");
		assert(p_e_view_dist_min && p_e_view_dist_ratio && p_e_view_dist_ratio_detail);
		uint32 nMinSpec = (m_dwRndFlags&ERF_SPEC_BITS_MASK) >> ERF_SPEC_BITS_SHIFT;
		if (nMinSpec == CONFIG_DETAIL_SPEC)
      return max(p_e_view_dist_min->GetFVal(),GetBBox().GetRadius()*p_e_view_dist_ratio_detail->GetFVal()*GetViewDistRatioNormilized());
		return max(p_e_view_dist_min->GetFVal(),GetBBox().GetRadius()*p_e_view_dist_ratio->GetFVal()*GetViewDistRatioNormilized());
	}
	else
	{
		static ICVar * p_e_view_dist_min = GetISystem()->GetIConsole()->GetCVar("e_view_dist_min");
		static ICVar * p_e_view_dist_ratio = GetISystem()->GetIConsole()->GetCVar("e_view_dist_custom_ratio");
		//static ICVar * p_e_view_dist_ratio = GetISystem()->GetIConsole()->GetCVar("e_view_dist_ratio");
		assert(p_e_view_dist_min && p_e_view_dist_ratio);
		float s = max( max( (m_WSBBox.max.x-m_WSBBox.min.x),(m_WSBBox.max.y-m_WSBBox.min.y) ),(m_WSBBox.max.z-m_WSBBox.min.z) );
		return max(p_e_view_dist_min->GetFVal(),s*p_e_view_dist_ratio->GetFVal()*GetViewDistRatioNormilized());
	}
}

//////////////////////////////////////////////////////////////////////////

void CRenderProxy::SetViewDistRatio(int nViewDistRatio) 
{ 
	IRenderNode::SetViewDistRatio(nViewDistRatio);

	for (uint32 i = 0; i < m_slots.size(); i++) 
	{
		if (m_slots[i] && m_slots[i]->pChildRenderNode)
		{ 
			m_slots[i]->pChildRenderNode->SetViewDistRatio(nViewDistRatio);
		}

		if (m_slots[i] && m_slots[i]->pLight)
		{ 
			m_slots[i]->pLight->SetViewDistRatio(nViewDistRatio);
      m_slots[i]->pLight->m_fWSMaxViewDist = m_slots[i]->pLight->GetMaxViewDist();
		}
	}
}

void CRenderProxy::SetMinSpec(int nMinSpec) 
{ 
  IRenderNode::SetMinSpec(nMinSpec);

  for (uint32 i = 0; i < m_slots.size(); i++) 
  {
    if (m_slots[i] && m_slots[i]->pChildRenderNode)
    { 
      m_slots[i]->pChildRenderNode->SetMinSpec(nMinSpec);
    }

    if (m_slots[i] && m_slots[i]->pLight)
    { 
      m_slots[i]->pLight->SetMinSpec(nMinSpec);
    }
  }
}

void CRenderProxy::SetLodRatio(int nLodRatio) 
{ 
	IRenderNode::SetLodRatio(nLodRatio);

	for (uint32 i = 0; i < m_slots.size(); i++) 
	{
		if (m_slots[i] && m_slots[i]->pChildRenderNode)
		{ 
			m_slots[i]->pChildRenderNode->SetLodRatio(nLodRatio);
		}
		if (m_slots[i] && m_slots[i]->pLight)
		{ 
			m_slots[i]->pLight->SetLodRatio(nLodRatio);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CRenderProxy::IsRenderProxyVisAreaVisible() const
{
	// test last render frame id
	if(GetEntityVisArea())
		if(abs(GetEntityVisArea()->GetVisFrameId() - gEnv->pRenderer->GetFrameID())<=MAX_FRAME_ID_STEP_PER_FRAME)
			return true;

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CRenderProxy::GetMemoryStatistics( ICrySizer *pSizer )
{
	int nSize = sizeof(*this);
	int nSlots = (int)m_slots.size();
	for (int i = 0; i < nSlots; i++) 
	{
		if (m_slots[i])
			nSize += sizeof(m_slots[i]);
	}
	pSizer->AddObject(this,nSize);
}

bool CRenderProxy::IsMovableByGame() const
{
  if(IPhysicalEntity * pPhysEnt = m_pEntity->GetPhysics())
  {
    if(pe_type eType = pPhysEnt->GetType())
    {
      return !(eType == PE_NONE || eType == PE_STATIC || eType == PE_AREA);
    }
  }

  return false;
}

IRenderMesh * CRenderProxy::GetRenderMesh(int nLod)
{ 
	// Tbyte TODO: Currently only slot #0 is supported
	IStatObj * pStatObj = GetStatObj(0) ? GetStatObj(0)->GetLodObject(nLod) : NULL; 
	return pStatObj ? pStatObj->GetRenderMesh() : NULL;
}

void CRenderProxy::SetVertexScattering(const SScatteringObjectData& ScatteringObjectData)
{
	IRenderer* pIRenderer = gEnv->pRenderer;
	// Tbyte TODO: Currently only slot #0 is supported
	IStatObj* pIStatObj = GetStatObj(0);
	if (pIStatObj == NULL)
		return;

	m_VertexScatterTransformZ = ScatteringObjectData.m_VertexScatterTransformZ;

	int subobjectcount = pIStatObj->GetSubObjectCount();

	m_bHasVertexScatterBuffer = false;
	if (pIStatObj->GetRenderMesh())
	{
		for (int nLod=0; nLod<MAX_VERTEX_SCATTER_LODS_NUM; ++nLod)
		{
			if (m_arrScatterVertexBuffer[nLod])
				pIRenderer->DeleteRenderMesh(m_arrScatterVertexBuffer[nLod]);
			SVertexScatterLODLevel* vsll = ScatteringObjectData.m_VertexScatterSubObjects[0].m_VertexScatterLODLevels[nLod];
			if (vsll)
			{
				m_arrScatterVertexBuffer[nLod] = pIRenderer->CreateRenderMeshInitialized(vsll->m_VertexScatterValues, vsll->m_nNumVertices, VERTEX_FORMAT_SCATTER, NULL, 0, R_PRIMV_TRIANGLES, "VertexScatter", pIStatObj->GetFilePath());
				m_bHasVertexScatterBuffer = true;
			}
			else
				m_arrScatterVertexBuffer[nLod] = NULL;
		}
	}
	else
	{
		for (int i=0; i<m_SubObjectScatterVertexBuffer.size(); ++i)
			if (m_SubObjectScatterVertexBuffer[i])
				pIRenderer->DeleteRenderMesh(m_SubObjectScatterVertexBuffer[i]);

		// Tbyte TODO: LOD with submeshes
		m_SubObjectScatterVertexBuffer.resize(ScatteringObjectData.m_nNumSubObjects);
		for (int i=0; i<ScatteringObjectData.m_nNumSubObjects; ++i)
		{
			SVertexScatterLODLevel* vsll = ScatteringObjectData.m_VertexScatterSubObjects[i].m_VertexScatterLODLevels[0];
			if (vsll)
			{
				m_SubObjectScatterVertexBuffer[i] = pIRenderer->CreateRenderMeshInitialized(vsll->m_VertexScatterValues, vsll->m_nNumVertices, VERTEX_FORMAT_SCATTER, NULL, 0, R_PRIMV_TRIANGLES, "VertexScatter", pIStatObj->GetFilePath());
				m_bHasVertexScatterBuffer = true;
			}
			else
				m_SubObjectScatterVertexBuffer[i] = NULL;
		}
	}
}
