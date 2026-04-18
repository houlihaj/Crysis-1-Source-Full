////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   Entity.h
//  Version:     v1.00
//  Created:     18/5/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Entity.h"
#include "AffineParts.h"
#include "EntitySystem.h"
#include "IEntityClass.h"
#include "SoundProxy.h"
#include "AreaProxy.h"
#include "FlowGraphProxy.h"
#include "ISerialize.h"
#include "TriggerProxy.h"
#include "PartitionGrid.h"
#include "ProximityTriggerSystem.h"
#include "CameraProxy.h"
#include "EntityObject.h"
#include "RopeProxy.h"
#include "Boids/BoidsProxy.h"

#include <IAISystem.h>
#include <IAgent.h>
#include "IRenderAuxGeom.h"
#include "ICryAnimation.h"
#include "IGame.h"
#include "IGameFramework.h"
#include "../CryAction/ILevelSystem.h"		

// enable this to check nan's on position updates... useful for debugging some weird crashes
#define ENABLE_NAN_CHECK

#ifdef ENABLE_NAN_CHECK
#define CHECKQNAN_FLT(x) \
	assert(*(unsigned*)&(x) != 0xffffffffu)
#else
#define CHECKQNAN_FLT(x) (void*)0
#endif

#define CHECKQNAN_VEC(v) \
	CHECKQNAN_FLT(v.x); CHECKQNAN_FLT(v.y); CHECKQNAN_FLT(v.z)

namespace
{
	Matrix34 sIdentityMatrix = Matrix34::CreateIdentity();
}

//////////////////////////////////////////////////////////////////////////
CEntity::CEntity( CEntitySystem *pEntitySystem,SEntitySpawnParams &params )
{
	m_pClass = params.pClass;
	m_pArchetype = params.pArchetype;
	m_nID = params.id;
	m_flags = params.nFlags;
	m_guid = params.guid;

	// Set flags.
	m_bActive = 0;
	m_bInActiveList = 0;

	m_bBoundsValid = 0;
	m_bInitialized = 0;
	m_bHidden = 0;
	m_bInvisible =0;
	m_bGarbage = 0;
	m_bUseProxyArray = 0;
	m_nUpdateCounter = 0;
	m_bHaveEventListeners = 0;
	m_bTrigger = 0;
	m_bWasRellocated = 0;
	m_bNotInheritXform = 0;
	
	m_pGridLocation = 0;
	m_pProximityEntity = 0;

	m_eUpdatePolicy = ENTITY_UPDATE_NEVER;
	m_pBinds = NULL;
	m_pAIObject = NULL;
	m_pSmartObject = NULL;

	// Clear all proxies.
	m_proxy.simple.pProxyArray[0] = 0;
	m_proxy.simple.pProxyArray[1] = 0;
	m_proxy.simple.pProxyArray[2] = 0;
	//ZeroStruct( m_proxy );

	m_pEntityLinks = 0;
  m_bTrainEntity = false;

	//////////////////////////////////////////////////////////////////////////
	// Initialize basic parameters.
	//////////////////////////////////////////////////////////////////////////
	if (params.sName && params.sName[0] != '\0')
		SetName( params.sName );
	CHECKQNAN_VEC(params.vPosition);
	m_vPos = params.vPosition;
	m_qRotation = params.qRotation;
	m_vScale = params.vScale;

	CalcLocalTM( m_worldTM );

	//////////////////////////////////////////////////////////////////////////
	// Check if entity needs to create a proxy class.
	IEntityClass::UserProxyCreateFunc pUserProxyCreateFunc = m_pClass->GetUserProxyCreateFunc();
	if (pUserProxyCreateFunc)
	{
		IEntityProxy *pUserProxy = pUserProxyCreateFunc( this,params,m_pClass->GetUserProxyData() );
		if (pUserProxy)
		{
			// Assign user proxy if successfully created.
			SetProxy( ENTITY_PROXY_USER,pUserProxy );
		}
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Check if entity needs to create a script proxy.
	IEntityScript *pEntityScript = m_pClass->GetIEntityScript();
	if (pEntityScript)
	{
		// Creates a script proxy.
		IEntityProxy *pScriptProsy = CreateScriptProxy( this,pEntityScript,&params );
		if (pScriptProsy)
			SetProxy( ENTITY_PROXY_SCRIPT,pScriptProsy );
	}
}

//////////////////////////////////////////////////////////////////////////
CEntity::~CEntity()
{
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetName( const char *sName )
{
	g_pIEntitySystem->ChangeEntityName(this,sName);
	if(m_pAIObject)
		m_pAIObject->SetName(sName);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetFlags( uint32 flags )
{
	if (flags != m_flags)
	{
		m_flags = flags;
		CRenderProxy *pRenderProxy = GetRenderProxy();
		if (pRenderProxy)
		{
			pRenderProxy->UpdateEntityFlags();
		}
		if (m_pGridLocation)
			m_pGridLocation->nEntityFlags = flags;
	}
};

//////////////////////////////////////////////////////////////////////////
void CEntity::UpdateUseOccluders( bool bUse )
{
	CRenderProxy *pRenderProxy = GetRenderProxy();
	if (pRenderProxy)
	{
		pRenderProxy->UpdateEntityFlags();
		if (bUse)
			pRenderProxy->UpdateUseOccluders( true );
		else
			pRenderProxy->UpdateUseOccluders( false );
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::SendEvent( SEntityEvent &event )
{
	FUNCTION_PROFILER_FAST( GetISystem(),PROFILE_ENTITY,g_bProfilerEnabled );

	CTimeValue t0;
	if (CVar::es_DebugEvents)
	{
		t0 = gEnv->pTimer->GetAsyncTime();
	}

	if (CVar::es_DisableTriggers)
	{
		static IEntityClass *pProximityTriggerClass = NULL;
		static IEntityClass *pAreaTriggerClass = NULL;

		if (pProximityTriggerClass == NULL || pAreaTriggerClass == NULL)
		{
			IEntitySystem *pEntitySystem = gEnv->pEntitySystem;

			if (pEntitySystem != NULL)
			{
				IEntityClassRegistry *pClassRegistry = pEntitySystem->GetClassRegistry();

				if (pClassRegistry != NULL)
				{
					pProximityTriggerClass = pClassRegistry->FindClass("ProximityTrigger");
					pAreaTriggerClass = pClassRegistry->FindClass("AreaTrigger");
				}
			}
		}

		if (GetClass() == pProximityTriggerClass || GetClass() == pAreaTriggerClass)
		{
			if (event.event == ENTITY_EVENT_ENTERAREA || event.event == ENTITY_EVENT_LEAVEAREA)
			{
				return true;
			}
		}
	}

	// AI object position must be updated first so proxies could eventually use it (as CSmartObjects does)
	switch (event.event)
	{
	case ENTITY_EVENT_XFORM:
		if (!m_bGarbage)
		{
			if (m_pAIObject)
				UpdateAIObject();
		}
		break;
	case ENTITY_EVENT_ENTERAREA:
		if (!m_bGarbage)
		{
			// tell sound proxy before script proxy
			IEntitySoundProxy *pSoundProxy = (IEntitySoundProxy*)GetProxy(ENTITY_PROXY_SOUND);
			if (pSoundProxy)
			{
				pSoundProxy->ProcessEvent(event);
			}
		}
		break;

	case ENTITY_EVENT_RESET:
		{
			// Activate entity if was deactivated:
			if (m_bGarbage)
			{
				m_bGarbage = false;
				// If entity was deleted in game, ressurect it.
				SEntityEvent entevnt;
				entevnt.event = ENTITY_EVENT_INIT;
				SendEvent( entevnt );
			}

			//CryLogAlways( "%s became visible",GetEntityTextDescription() );
			// [marco] needed when going in and out of game mode in the editor
			CRenderProxy *pRenderProxy = GetRenderProxy();
			if (pRenderProxy)
			{
				pRenderProxy->SetLastSeenTime(GetISystem()->GetITimer()->GetCurrTime());			
				ICharacterInstance* pCharacterInstance = pRenderProxy->GetCharacter(0);
				if (pCharacterInstance)
					pCharacterInstance->SetAnimationSpeed(1.0f);
			}
			break;
		}
	case ENTITY_EVENT_INIT:
		m_bGarbage = false;
		break;

	case ENTITY_EVENT_ANIM_EVENT:
		{
			// If the event is a sound event, make sure we have a sound proxy.
			const AnimEventInstance* pAnimEvent = reinterpret_cast<const AnimEventInstance*>(event.nParam[0]);
			const char* eventName = (pAnimEvent ? pAnimEvent->m_EventName : 0);
			if (eventName && stricmp(eventName, "sound") == 0)
			{
				if (!GetProxy(ENTITY_PROXY_SOUND))
					CreateProxy(ENTITY_PROXY_SOUND);
			}
		}
		break;

	case ENTITY_EVENT_DONE:
		// When deleting should detach all children.
		{
			//g_pIEntitySystem->RemoveTimerEvent(GetId(), -1);
			DeallocBindings();
		}
		break;
	}

	if (!m_bGarbage)
	{
		// Broadcast event to proxies.
		int nProxyCount = ProxyCount();
		for (int i = 0; i < nProxyCount; i++)
		{
			IEntityProxy *pProxy = GetProxyAt(i);
			if (pProxy)
				pProxy->ProcessEvent( event );
		}
		// Give entity system a chance to check the event, and notify other listeners.
		g_pIEntitySystem->OnEntityEvent( this,event );

		if (CVar::es_DebugEvents)
		{
			CTimeValue t1 = gEnv->pTimer->GetAsyncTime();
			LogEvent( event,t1-t0 );
		}

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::Init( SEntitySpawnParams &params )
{
	// Initialize all currently existing proxies.
	for (int i = ENTITY_PROXY_SCRIPT+1; i < ProxyCount(); i++)
	{
		IEntityProxy *pProxy = GetProxyAt(i);
		if (pProxy)
			pProxy->Init( this,params );
	}

	//Script Proxy is initialized last.
	CScriptProxy *pScriptProxy = GetScriptProxy();
	if (pScriptProxy)
		pScriptProxy->Init(this,params);

	// Make sure position is registered.
	if (!m_bWasRellocated)
		OnRellocate(ENTITY_XFORM_POS);

	//Render Proxy is initialized last.
	CRenderProxy *pRenderProxy = GetRenderProxy();
	if (pRenderProxy)
		pRenderProxy->PostInit();
	m_bInitialized = true;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::Update( SEntityUpdateContext &ctx )
{
	FUNCTION_PROFILER_FAST( GetISystem(),PROFILE_ENTITY,g_bProfilerEnabled );

	if (m_bHidden && !CheckFlags(ENTITY_FLAG_UPDATE_HIDDEN))
		return;

	// Broadcast event to proxies.
	// Start after render proxy.
	for (int i = ENTITY_PROXY_RENDER+1; i < ProxyCount(); i++)
	{
		IEntityProxy *pProxy = GetProxyAt(i);
		if (pProxy)
			pProxy->Update( ctx );
	}

	IEntityProxy *pRenderProxy = GetRenderProxy();
	if (pRenderProxy)
		pRenderProxy->Update(ctx);
//	UpdateAIObject();
	// Update render proxy last.

	if (m_nUpdateCounter != 0)
	{
		if (--m_nUpdateCounter == 0)
		{
			SetUpdateStatus();
		}
	}
}

void CEntity::PrePhysicsUpdate( float fFrameTime )
{
	FUNCTION_PROFILER_FAST( GetISystem(), PROFILE_ENTITY, g_bProfilerEnabled );

	SEntityEvent evt( ENTITY_EVENT_PREPHYSICSUPDATE );
	evt.fParam[0] = fFrameTime;

	int nProxyCount = ProxyCount();
	for (int i = ENTITY_PROXY_RENDER+1; i < nProxyCount; i++)
	{
		IEntityProxy *pProxy = GetProxyAt(i);
		if (pProxy)
			pProxy->ProcessEvent(evt);
	}

	IEntityProxy *pRenderProxy = GetRenderProxy();
	if (pRenderProxy)
		pRenderProxy->ProcessEvent(evt);
}

bool CEntity::IsPrePhysicsActive()
{
	return g_pIEntitySystem->IsPrePhysicsActive(this);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::ShutDown()
{
	ENTITY_PROFILER

	// Remove entity name from the map.
	g_pIEntitySystem->ChangeEntityName(this,"");

	RemoveAllEntityLinks();

	if (GetPhysicalProxy())
	{	// makes sure physics is destroyed when other proxies are still alive
		SEntityPhysicalizeParams epp; epp.type = PE_NONE;
		GetPhysicalProxy()->Physicalize(epp);
	}

	if (m_pGridLocation)
	{
		// Free grid location.
		g_pIEntitySystem->GetPartitionGrid()->FreeLocation(m_pGridLocation);
		m_pGridLocation = 0;
		m_flags |= ENTITY_FLAG_NO_PROXIMITY;
	}

	if (m_pProximityEntity)
	{
		g_pIEntitySystem->GetProximityTriggerSystem()->RemoveEntity( m_pProximityEntity );
		m_pProximityEntity = 0;
	}

	//////////////////////////////////////////////////////////////////////////
	// release AI object
	if (m_pAIObject)
	{
		if (gEnv->pAISystem)
			gEnv->pAISystem->RemoveObject(m_pAIObject);
		m_pAIObject = NULL;
		m_flags &= ~ENTITY_FLAG_HAS_AI;
	}

	//////////////////////////////////////////////////////////////////////////
	// release SmartObject
	if (m_pSmartObject)
	{
		if (gEnv->pAISystem)
			gEnv->pAISystem->RemoveSmartObject(this);
		m_pSmartObject = NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	// remove timers
	g_pIEntitySystem->RemoveTimerEvent(GetId(), -1);

	//////////////////////////////////////////////////////////////////////////
	// Release all proxies.
	// First release UserProxy
	if (m_bUseProxyArray)
	{
		// call Done on every proxy
		for (int i = 0; i < m_proxy.array.numProxies; i++)
		{
			IEntityProxy *pProxy = m_proxy.array.pProxyArray[i];
			if (pProxy)
			{
				pProxy->Done();
			}
		}
		for (int i = 0; i < m_proxy.array.numProxies; i++)
		{
			IEntityProxy *pProxy = m_proxy.array.pProxyArray[i];
			if (pProxy)
			{
				pProxy->Release();
				m_proxy.array.pProxyArray[i] = 0;
			}
		}

		delete []m_proxy.array.pProxyArray;
		m_bUseProxyArray = 0;
		m_proxy.simple.pProxyArray[0] = 0;
		m_proxy.simple.pProxyArray[1] = 0;
		m_proxy.simple.pProxyArray[2] = 0;
	}
	else
	{
		// call Done on every proxy
		for (int i = 0; i < 3; i++)
		{
			IEntityProxy *pProxy = m_proxy.simple.pProxyArray[i];
			if (pProxy)
			{
				pProxy->Done();
			}
		}

		for (int i = 0; i < 3; i++)
		{
			IEntityProxy *pProxy = m_proxy.simple.pProxyArray[i];
			if (pProxy)
			{
				pProxy->Release();
				m_proxy.simple.pProxyArray[i] = 0;
			}
		}
	}
	//////////////////////////////////////////////////////////////////////////

	DeallocBindings();
}

//////////////////////////////////////////////////////////////////////////
void CEntity::EnableInheritXForm( bool bEnable )
{
	m_bNotInheritXform = !bEnable;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::AttachChild( IEntity *pChildEntity,int nAttachFlags )
{
	assert(pChildEntity);
	// In debug mode check for attachment recursion.
#ifdef _DEBUG
	assert( this != pChildEntity && "Trying to Attach to itself" );
	for (IEntity *pParent = GetParent(); pParent; pParent = pParent->GetParent())
	{
		assert( pParent != pChildEntity && "Recursive Attachment" );

	}
#endif
	if (pChildEntity == this)
	{
		EntityWarning( "Trying to attaching Entity %s to itself",GetEntityTextDescription() );
		return;
	}

	// Child entity must be also CEntity.
	CEntity *pChild = reinterpret_cast<CEntity*>(pChildEntity);

	// If not already attached to this node.
	if (pChild->m_pBinds && pChild->m_pBinds->pParent == this)
		return;

	// Make sure binding structure allocated for entity and child.
	AllocBindings();
	pChild->AllocBindings();

	Matrix34 childTM;
	if (nAttachFlags & ATTACHMENT_KEEP_TRANSFORMATION)
	{
		childTM = pChild->GetWorldTM();
	}

	// Add to child list first to make sure node not get deleted while re-attaching.
	m_pBinds->childs.push_back( pChild );
	if (pChild->m_pBinds->pParent)
		pChild->DetachThis();	// Detach node if attached to other parent.	

	// Assign this entity as parent to child entity.
	pChild->m_pBinds->pParent = this;

	if (nAttachFlags & ATTACHMENT_KEEP_TRANSFORMATION)
	{
		// Keep old world space transformation.
		pChild->SetWorldTM( childTM );
	}
	else if (pChild->m_flags & ENTITY_FLAG_TRIGGER_AREAS)
	{
		// If entity should trigger areas, when attaching it make sure local translation is reset to (0,0,0).
		// This prevents invalid position to propogate to area manager and incorrectly leave/enter areas.
		pChild->m_vPos.Set(0,0,0);
		pChild->InvalidateTM(ENTITY_XFORM_FROM_PARENT);
	}
	else
		pChild->InvalidateTM(ENTITY_XFORM_FROM_PARENT);

	// Send attach event.
	SEntityEvent event(ENTITY_EVENT_ATTACH);
	event.nParam[0] = pChild->GetId();
	SendEvent( event );
}

//////////////////////////////////////////////////////////////////////////
void CEntity::DetachAll( int nDetachFlags  )
{
	if (m_pBinds)
	{
		while (!m_pBinds->childs.empty())
		{
			CEntity* pChild = m_pBinds->childs.front();
			pChild->DetachThis(nDetachFlags);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::DetachThis( int nDetachFlags,int nWhyFlags )
{
	if (m_pBinds && m_pBinds->pParent)
	{
		Matrix34 worldTM;

		bool bKeepTransform = (nDetachFlags & ATTACHMENT_KEEP_TRANSFORMATION) || (m_flags & ENTITY_FLAG_TRIGGER_AREAS);

		if (bKeepTransform)
		{
			worldTM = GetWorldTM();
		}

		// Copy parent to temp var, erasing child from parent may delete this node if child referenced only from parent.
		CEntity * pParent = m_pBinds->pParent;
		m_pBinds->pParent = NULL;

		// Remove child pointer from parent array of childs.
		SBindings *pParentBinds = pParent->m_pBinds;
		if (pParentBinds)
		{
			stl::find_and_erase( pParentBinds->childs,this );
		}

		if (bKeepTransform)
		{
			// Keep old world space transformation.
			SetLocalTM( worldTM,nWhyFlags|ENTITY_XFORM_FROM_PARENT );
		}
		else
			InvalidateTM(nWhyFlags|ENTITY_XFORM_FROM_PARENT);

		// Send detach event to parent.
		SEntityEvent event(ENTITY_EVENT_DETACH);
		event.nParam[0] = GetId();
		pParent->SendEvent( event );

    // Send DETACH_THIS event to child
    SEntityEvent eventThis(ENTITY_EVENT_DETACH_THIS);    
    SendEvent( eventThis );
	}
}

//////////////////////////////////////////////////////////////////////////
int CEntity::GetChildCount() const
{
	if (m_pBinds)
		return m_pBinds->childs.size();
	return 0;
}

//////////////////////////////////////////////////////////////////////////
IEntity* CEntity::GetChild( int nIndex ) const
{
	if (m_pBinds)
	{
		if (nIndex >= 0 && nIndex < (int)m_pBinds->childs.size())
			return m_pBinds->childs[nIndex];
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::AllocBindings()
{
	if (!m_pBinds)
	{
		m_pBinds = new SBindings;
		m_pBinds->pParent = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::DeallocBindings()
{
	if (m_pBinds)
	{
		DetachAll();
		if (m_pBinds->pParent)
			DetachThis(0);
		delete m_pBinds;
	}
	m_pBinds = NULL;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetWorldTM( const Matrix34 &tm,int nWhyFlags )
{
	if (GetParent() && !m_bNotInheritXform)
	{
		Matrix34 localTM = GetParent()->GetWorldTM().GetInverted() * tm;
		SetLocalTM( localTM,nWhyFlags );
	}
	else
	{
		SetLocalTM( tm,nWhyFlags );
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetLocalTM( const Matrix34 &tm,int nWhyFlags )
{
	if (m_bNotInheritXform && (nWhyFlags&ENTITY_XFORM_FROM_PARENT)) // Ignore parent x-forms.
		return;

	AffineParts affineParts;
	affineParts.SpectralDecompose(tm);

	SetPosRotScale( affineParts.pos,affineParts.rot,affineParts.scale,nWhyFlags );
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetPosRotScale( const Vec3 &vPos,const Quat &qRotation,const Vec3 &vScale,int nWhyFlags )
{
	int changed = 0;

	if (m_bNotInheritXform && (nWhyFlags&ENTITY_XFORM_FROM_PARENT)) // Ignore parent x-forms.
		return;

	if (m_vPos != vPos)
	{
		nWhyFlags |= ENTITY_XFORM_POS;
		changed++;
		CHECKQNAN_VEC(vPos);
		m_vPos = vPos;
	}

	if (m_qRotation.v != qRotation.v || m_qRotation.w != qRotation.w)
	{
		nWhyFlags |= ENTITY_XFORM_ROT;
		changed++;
		m_qRotation = qRotation;
	}

	if (m_vScale != vScale)
	{
		nWhyFlags |= ENTITY_XFORM_SCL;
		changed++;
		m_vScale = vScale;
	}

	if (changed != 0)
	{
		InvalidateTM(nWhyFlags);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::CalcLocalTM( Matrix34 &tm ) const
{
	tm = Matrix34(m_qRotation);
	tm = Matrix33::CreateScale(m_vScale) * tm;

	// Translate matrix.
	tm.SetTranslation(m_vPos);
}

//////////////////////////////////////////////////////////////////////////
Matrix34 CEntity::GetLocalTM() const
{
	Matrix34 tm;
	CalcLocalTM( tm );
	return tm;
}

//////////////////////////////////////////////////////////////////////////
const Matrix34& CEntity::GetWorldTM() const
{
	return m_worldTM;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::OnRellocate( int nWhyFlags )
{
	if (m_flags & ENTITY_FLAG_TRIGGER_AREAS && (nWhyFlags&ENTITY_XFORM_POS))
	{
		// todo introduce a different flag for Craigs trigger proxy thing and dont test for "Player" anymore
		// also sound volume entities, which move with the player shouldnt call this
		if ((strcmp(m_pClass->GetName(),"Player") == 0) || (strcmp(m_pClass->GetName(),"CameraSource") == 0))
		{
			g_pIEntitySystem->GetAreaManager()->MarkPlayerForUpdate( GetId() );
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Reposition entity in the partition grid.
	//////////////////////////////////////////////////////////////////////////
	if ((m_flags & ENTITY_FLAG_NO_PROXIMITY) == 0 && (!m_bHidden || (nWhyFlags&ENTITY_XFORM_EDITOR)) )
	{
		if (nWhyFlags&ENTITY_XFORM_POS)
		{
			m_bWasRellocated = true;
			Vec3 wp = GetWorldPos();
			m_pGridLocation = g_pIEntitySystem->GetPartitionGrid()->Rellocate(m_pGridLocation,wp,this);
			if (m_pGridLocation)
				m_pGridLocation->nEntityFlags = m_flags;

			if (!m_bTrigger)
			{
				if (!m_pProximityEntity)
					m_pProximityEntity = g_pIEntitySystem->GetProximityTriggerSystem()->CreateEntity( m_nID );
				g_pIEntitySystem->GetProximityTriggerSystem()->MoveEntity( m_pProximityEntity,wp );
			}
		}
	}
	else if (m_pGridLocation)
	{
		m_bWasRellocated = true;
		g_pIEntitySystem->GetPartitionGrid()->FreeLocation(m_pGridLocation);
		m_pGridLocation = 0;

		if (m_bHidden)
		{
			if (!m_bTrigger)
			{
				//////////////////////////////////////////////////////////////////////////
				if (m_pProximityEntity && !(m_flags&ENTITY_FLAG_LOCAL_PLAYER)) // Hidden local player still should trigger proximity triggers in editor.
				{
					g_pIEntitySystem->GetProximityTriggerSystem()->RemoveEntity( m_pProximityEntity );
					m_pProximityEntity = 0;
				}
			}
		}
	}
	//////////////////////////////////////////////////////////////////////////
}

//////////////////////////////////////////////////////////////////////////
void CEntity::InvalidateTM( int nWhyFlags )
{
	if (nWhyFlags & ENTITY_XFORM_FROM_PARENT)
	{
		nWhyFlags |= ENTITY_XFORM_POS|ENTITY_XFORM_ROT|ENTITY_XFORM_SCL;
	}

	CalcLocalTM( m_worldTM );

	if (m_pBinds)
	{
		if (m_pBinds->pParent && !m_bNotInheritXform)
			m_worldTM = m_pBinds->pParent->m_worldTM * m_worldTM;

		// Invalidate matrices for all child objects.
		for (int i = 0; i < (int)m_pBinds->childs.size(); i++)
		{
			if (m_pBinds->childs[i])
			{
				m_pBinds->childs[i]->InvalidateTM(nWhyFlags|ENTITY_XFORM_FROM_PARENT);
			}
		}
	}

	OnRellocate(nWhyFlags);

	// Send transform event.
	SEntityEvent event(ENTITY_EVENT_XFORM);
	event.nParam[0] = nWhyFlags;
	SendEvent( event );
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetPos( const Vec3 &vPos,int nWhyFlags )
{
	CHECKQNAN_VEC(vPos);
	if (m_vPos == vPos)
		return;

	m_vPos = vPos;
	InvalidateTM(nWhyFlags|ENTITY_XFORM_POS); // Position change.
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetRotation( const Quat &qRotation,int nWhyFlags )
{
	if (m_qRotation.v == qRotation.v && m_qRotation.w == qRotation.w)
		return;

	m_qRotation = qRotation;
	InvalidateTM(nWhyFlags|ENTITY_XFORM_ROT); // Rotation change.
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetScale( const Vec3 &vScale,int nWhyFlags )
{
	if (m_vScale == vScale)
		return;

	m_vScale = vScale;
	InvalidateTM(nWhyFlags|ENTITY_XFORM_SCL); // Scale change.
}

//////////////////////////////////////////////////////////////////////////
Ang3 CEntity::GetWorldAngles() const
{
	if (m_vScale == Vec3(1,1,1))
	{
		Quat q = Quat( GetWorldTM() );
		Ang3 angles = Ang3::GetAnglesXYZ(Matrix33(q));
		return angles;
	}
	else
	{
		Matrix34 tm = GetWorldTM();
		tm.OrthonormalizeFast();
		Quat q = Quat(tm);
		Ang3 angles = Ang3::GetAnglesXYZ(Matrix33(q));
		return angles;
	}
}

//////////////////////////////////////////////////////////////////////////
Quat CEntity::GetWorldRotation() const
{
	if (m_vScale == Vec3(1,1,1))
	{
		return Quat( GetWorldTM() );
	}
	else
	{
		Matrix34 tm = GetWorldTM();
		tm.OrthonormalizeFast();
		return Quat(tm);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::GetWorldBounds( AABB &bbox )
{
	GetLocalBounds(bbox);
	bbox.SetTransformedAABB( GetWorldTM(),bbox );
}

//////////////////////////////////////////////////////////////////////////
void CEntity::GetLocalBounds( AABB &bbox )
{
	bbox.min.Set(0,0,0);
	bbox.max.Set(0,0,0);

	if (GetRenderProxy())
	{
		GetRenderProxy()->GetLocalBounds( bbox );
	}
	else if (GetPhysicalProxy())
	{
		GetPhysicalProxy()->GetLocalBounds( bbox );
	}
	else 
	{
		IEntityTriggerProxy *pTriggerProxy = (IEntityTriggerProxy*)GetProxy( ENTITY_PROXY_TRIGGER );
		if (pTriggerProxy)
		{
			pTriggerProxy->GetTriggerBounds( bbox );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetUpdateStatus()
{
	bool bEnable = GetUpdateStatus();

	g_pIEntitySystem->ActivateEntity( this,bEnable );

	if (m_pAIObject)
		if (IUnknownProxy * pProxy = m_pAIObject->GetProxy())
			pProxy->EnableUpdate(bEnable);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::Activate( bool bActive )
{
	m_bActive = bActive;
	m_nUpdateCounter = 0;
	SetUpdateStatus();
}

//////////////////////////////////////////////////////////////////////////
void CEntity::PrePhysicsActivate( bool bActive )
{
	g_pIEntitySystem->ActivatePrePhysicsUpdateForEntity( this, bActive );
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetTimer( int nTimerId,int nMilliSeconds )
{
	KillTimer(nTimerId);
	SEntityTimerEvent timeEvent;
	timeEvent.entityId = m_nID;
	timeEvent.nTimerId = nTimerId;
	timeEvent.nMilliSeconds = nMilliSeconds;
	g_pIEntitySystem->AddTimerEvent( timeEvent );
}

//////////////////////////////////////////////////////////////////////////
void CEntity::KillTimer( int nTimerId )
{
	g_pIEntitySystem->RemoveTimerEvent( m_nID,nTimerId );
}

//////////////////////////////////////////////////////////////////////////
void CEntity::Hide( bool bHide )
{
	if (m_bHidden != bHide)
	{
		m_bHidden = bHide;

		// Update registered locations
		OnRellocate(ENTITY_XFORM_POS);

		if (bHide)
		{
			SEntityEvent e(ENTITY_EVENT_HIDE);
			SendEvent( e );
		}
		else
		{
			SEntityEvent e(ENTITY_EVENT_UNHIDE);
			SendEvent( e );
		}

		// Propogate Hide flag to the child entities.
		if (m_pBinds)
		{
			for (int i = 0; i < (int)m_pBinds->childs.size(); i++)
			{
				if (m_pBinds->childs[i] != NULL)
				{
					m_pBinds->childs[i]->Hide( bHide );
				}
			}
		}

		SetUpdateStatus();
	}
}


//////////////////////////////////////////////////////////////////////////
void CEntity::Invisible(bool bInvisible)
{
	if (m_bInvisible != bInvisible)
  {
    m_bInvisible = bInvisible;

    if (bInvisible)
    {
      SEntityEvent e(ENTITY_EVENT_INVISIBLE);
      SendEvent( e );
    }
    else
    {
      SEntityEvent e(ENTITY_EVENT_VISIBLE);
      SendEvent( e );
    }

    // Propagate invisible flag to the child entities.
    if (m_pBinds)
    {
      for (int i = 0; i < (int)m_pBinds->childs.size(); i++)
      {
        if (m_pBinds->childs[i] != NULL)
        {
          m_pBinds->childs[i]->Invisible( bInvisible );
        }
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetUpdatePolicy( EEntityUpdatePolicy eUpdatePolicy )
{
	m_eUpdatePolicy = eUpdatePolicy;
}

//////////////////////////////////////////////////////////////////////////
const char* CEntity::GetEntityTextDescription() const
{
	static string sTextDesc;
	sTextDesc = m_szName + " (" + m_pClass->GetName() + ")";
	return sTextDesc;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SerializeXML( XmlNodeRef &node,bool bLoading )
{
	// Serialize proxies.
	for (int i = 0; i < ProxyCount(); i++)
	{
		IEntityProxy *pProxy = GetProxyAt(i);
		if (pProxy)
			pProxy->SerializeXML( node,bLoading );
	}
}

//////////////////////////////////////////////////////////////////////////
IEntityProxy* CEntity::GetProxy( EEntityProxy proxy ) const
{
	IEntityProxy *pProxy = GetProxyByType(proxy);
	return pProxy;
}

//////////////////////////////////////////////////////////////////////////
int CEntity::AllocNewProxyIndex()
{
	if (m_bUseProxyArray)
	{
		int oldn = m_proxy.array.numProxies;
		// Reallocate with 1 more element.
		IEntityProxy **pArray = new IEntityProxy*[oldn+1];
		memcpy( pArray,m_proxy.array.pProxyArray,sizeof(IEntityProxy*)*oldn ); // Copy old array.
		delete []m_proxy.array.pProxyArray;
		m_proxy.array.pProxyArray = pArray;
		m_proxy.array.pProxyArray[oldn] = 0;
		m_proxy.array.numProxies = oldn+1;
		return oldn;
	}
	else
	{
		m_bUseProxyArray = true;
		// Preallocate 3 elements always for render/physics/script proxy.
		IEntityProxy **pArray = new IEntityProxy*[4];
		pArray[0] = m_proxy.simple.pProxyArray[0]; // Copy render proxy.
		pArray[1] = m_proxy.simple.pProxyArray[1]; // Copy phsyics proxy.
		pArray[2] = m_proxy.simple.pProxyArray[2];
		pArray[3] = 0;
		m_proxy.array.pProxyArray = pArray;
		m_proxy.array.numProxies = 4;
		return 3; // Return 3rd element after render/physics/scripts proxy.
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetProxy( EEntityProxy proxy,IEntityProxy *pProxy )
{
	if (pProxy->GetType() != proxy)
		return ;

	switch (proxy)
	{
	case ENTITY_PROXY_RENDER:
		if (!m_bUseProxyArray)
			m_proxy.simple.pProxyArray[proxy] = pProxy;
		else
			m_proxy.array.pProxyArray[proxy] = pProxy;
		return;
		break;
	case ENTITY_PROXY_PHYSICS:
		if (!m_bUseProxyArray)
			m_proxy.simple.pProxyArray[proxy] = pProxy;
		else
			m_proxy.array.pProxyArray[proxy] = pProxy;
		return;
		break;
	case ENTITY_PROXY_SCRIPT:
		if (!m_bUseProxyArray)
			m_proxy.simple.pProxyArray[proxy] = pProxy;
		else
			m_proxy.array.pProxyArray[proxy] = pProxy;
		return;
		break;
	}

	int nIndex = AllocNewProxyIndex();
	m_proxy.array.pProxyArray[nIndex] = pProxy;

	return;
}

//////////////////////////////////////////////////////////////////////////
IEntityProxy* CEntity::CreateProxy( EEntityProxy proxy )
{
	IEntityProxy *pProxy = GetProxyByType(proxy);
	if (!pProxy)
	{
		switch (proxy)
		{
		case ENTITY_PROXY_RENDER:
			pProxy = new CRenderProxy(this);
			SetProxy( ENTITY_PROXY_RENDER,pProxy );
			break;
		case ENTITY_PROXY_PHYSICS:
			pProxy = new CPhysicalProxy(this);
			SetProxy( ENTITY_PROXY_PHYSICS,pProxy );
			break;
		case ENTITY_PROXY_SOUND:
			pProxy = new CSoundProxy(this);
			SetProxy( ENTITY_PROXY_SOUND,pProxy );
			break;
		case ENTITY_PROXY_AI:
			//m_pProxy[proxy] = new CPhysicalProxy(this);
			//return true;
			break;
		case ENTITY_PROXY_AREA:
			pProxy = new CAreaProxy(this);
			SetProxy( ENTITY_PROXY_AREA,pProxy );
			break;
		case ENTITY_PROXY_FLOWGRAPH:
			pProxy = new CFlowGraphProxy(this);
			SetProxy( ENTITY_PROXY_FLOWGRAPH,pProxy );
			break;
		case ENTITY_PROXY_SUBSTITUTION:
			pProxy = new CSubstitutionProxy(this);
			SetProxy( ENTITY_PROXY_SUBSTITUTION,pProxy );
			break;
		case ENTITY_PROXY_TRIGGER:
			pProxy = new CTriggerProxy(this);
			SetProxy( ENTITY_PROXY_TRIGGER,pProxy );
			break;
		case ENTITY_PROXY_CAMERA:
			pProxy = new CCameraProxy(this);
			SetProxy( ENTITY_PROXY_CAMERA,pProxy );
			break;
		case ENTITY_PROXY_ROPE:
			pProxy = new CRopeProxy(this);
			SetProxy( ENTITY_PROXY_ROPE,pProxy );
			break;
		case ENTITY_PROXY_BOIDS:
			pProxy = new CBoidsProxy(this);
			SetProxy( ENTITY_PROXY_BOIDS,pProxy );
			break;
		case ENTITY_PROXY_BOID_OBJECT:
			pProxy = new CBoidObjectProxy(this);
			SetProxy( ENTITY_PROXY_BOID_OBJECT,pProxy );
			break;
		}
	}
	return pProxy;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::Serialize( TSerialize ser, int nFlags )
{
	if (nFlags & ENTITY_SERIALIZE_POSITION)
	{
		ser.Value( "position", m_vPos, 'wrld' );
	}
	if (nFlags & ENTITY_SERIALIZE_SCALE)
	{
		// TODO: check dimensions
		// TODO: define an AC_WorldPos type
		// TODO: check dimensions
		ser.Value( "scale", m_vScale );
	}
	if (nFlags & ENTITY_SERIALIZE_ROTATION)
	{
		ser.Value( "rotation", m_qRotation, 'ori3' );
	}

	if (nFlags & ENTITY_SERIALIZE_GEOMETRIES)
	{
		int i,nSlots;
		IPhysicalEntity *pPhysics = GetPhysics();
		pe_params_part pp;
		bool bHasIt = false;
		float mass = 0;

		if (!ser.IsReading())
		{
			nSlots = GetSlotCount();
			ser.Value("numslots", nSlots);
			CEntityObject *pSlot;
			for(i=0;i<nSlots;i++)	if ((pSlot=GetSlot(i)) && pSlot->pStatObj)
			{
				int nSlotId = i;
				ser.BeginGroup("Slot");
				ser.Value( "id",nSlotId );
				bool bHasXfrom = pSlot->m_pXForm!=0;
				ser.Value("hasXform", bHasXfrom);
				if (bHasXfrom)
				{
					Vec3 col;
					col=pSlot->m_pXForm->localTM.GetColumn(0); ser.Value("Xform0", col);
					col=pSlot->m_pXForm->localTM.GetColumn(1); ser.Value("Xform1", col);
					col=pSlot->m_pXForm->localTM.GetColumn(2); ser.Value("Xform2", col);
					col=pSlot->m_pXForm->localTM.GetTranslation(); ser.Value("XformT", col);
				}
				//ser.Value("hasmat", bHasIt=pSlot->pMaterial!=0);
				//if (bHasIt)
				//	ser.Value("matname", pSlot->pMaterial->GetName());
				ser.Value("flags", pSlot->flags);
				bool bSlotUpdate = pSlot->bUpdate;
				ser.Value("update", bSlotUpdate );
				if (pPhysics)
				{
					if (pSlot->pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND) 
					{
						MARK_UNUSED pp.partid;
						for(pp.ipart=0,mass=0; pPhysics->GetParams(&pp); pp.ipart++)
							mass += pp.mass;
					} else 
					{
						pp.partid = i; MARK_UNUSED pp.ipart;
						if (pPhysics->GetParams(&pp))
							mass = pp.mass;
					}
					ser.Value("mass", mass);
				}

				int bHeavySer = 0;
				if (pPhysics && pPhysics->GetType()==PE_RIGID)
					if (pSlot->pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND)
					{
						IStatObj::SSubObject *pSubObj;
						for(i=0; i<pSlot->pStatObj->GetSubObjectCount(); i++) 
							if ((pSubObj = pSlot->pStatObj->GetSubObject(i)) && pSubObj->pStatObj)
								bHeavySer |= pSubObj->pStatObj->GetFlags() & STATIC_OBJECT_GENERATED;
					} else
						bHeavySer = pSlot->pStatObj->GetFlags() & STATIC_OBJECT_GENERATED;

				if (bHeavySer) // prevent heavy mesh serialization of modified statobjects
					ser.Value("noobj", bHasIt=true);
				else
				{
					ser.Value("noobj", bHasIt=false);
					gEnv->p3DEngine->SaveStatObj(pSlot->pStatObj, ser);
				}
				ser.EndGroup(); //Slot
			}
		} else
		{
			bool bClearAfterLoad = false;
			pe_action_remove_all_parts arp;
			if (pPhysics)
				pPhysics->Action(&arp);
			for(i=GetSlotCount()-1;i>=0;i--)
				FreeSlot(i);
			ser.Value("numslots", nSlots);
			for (int iter = 0; iter<nSlots;iter++)
			{
				int nSlotId = -1;
				ser.BeginGroup("Slot");
				ser.Value( "id",nSlotId );

				if (!GetRenderProxy())
					CreateProxy(ENTITY_PROXY_RENDER);
				CEntityObject *pSlot = GetRenderProxy()->AllocSlot(nSlotId);

				ser.Value("hasXform", bHasIt);
				if (bHasIt)
				{
					Vec3 col; Matrix34 mtx;
					ser.Value("Xform0",col); mtx.SetColumn(0,col);
					ser.Value("Xform1",col); mtx.SetColumn(1,col);
					ser.Value("Xform2",col); mtx.SetColumn(2,col);
					ser.Value("XformT",col); mtx.SetTranslation(col);
					SetSlotLocalTM(nSlotId, mtx);
				}

				ser.Value("flags", pSlot->flags);
				ser.Value("update", bHasIt);
				pSlot->bUpdate = bHasIt;
				if (pPhysics)
					ser.Value("mass", mass);

				ser.Value("noobj", bHasIt);
				if (bHasIt)
					bClearAfterLoad = true;
				else
					SetStatObj(gEnv->p3DEngine->LoadStatObj(ser), ENTITY_SLOT_ACTUAL|nSlotId, pPhysics!=0, mass);

				ser.EndGroup(); //Slot
			}
			if (bClearAfterLoad)
			{
				if (pPhysics)
					pPhysics->Action(&arp);
				for(i=GetSlotCount()-1;i>=0;i--)
					FreeSlot(i);
			}
		}
	}

	if (nFlags & ENTITY_SERIALIZE_PROXIES)
	{

		//CryLogAlways("Serializing proxies for %s of class %s", GetName(), GetClass()->GetName());
		bool bSaveProxies = ser.GetSerializationTarget() == eST_Network; // always save for network stream
		if (!bSaveProxies && !ser.IsReading())
		{
			int proxyNrSerialized = 0;
			while(true)
			{
				EEntityProxy proxy = ProxySerializationOrder[proxyNrSerialized];
				if(proxy == ENTITY_PROXY_LAST)
					break;
				IEntityProxy *pProxy = GetProxy(proxy);
				if (pProxy)
				{
					if (pProxy->NeedSerialize())
					{
						bSaveProxies = true;
						break;
					}
				}
				proxyNrSerialized++;
			}
		}

		if (ser.BeginOptionalGroup("EntityProxies",bSaveProxies))
		{
			int proxyCount = ProxyCount();

			bool bHasSubst;
			if (!ser.IsReading())
				ser.Value("bHasSubst", bHasSubst=GetProxy(ENTITY_PROXY_SUBSTITUTION)!=0);
			else
			{
				ser.Value("bHasSubst", bHasSubst);
				if (bHasSubst && !GetProxy(ENTITY_PROXY_SUBSTITUTION))
					CreateProxy(ENTITY_PROXY_SUBSTITUTION);
			}

			//serializing the available proxies in the specified order (physics after script and user ..) [JAN]
			int proxyNrSerialized = 0;
			while(true)
			{
				EEntityProxy proxy = ProxySerializationOrder[proxyNrSerialized];

				if(proxy == ENTITY_PROXY_LAST)
					break;

				IEntityProxy *pProxy = GetProxy(proxy);
				if(pProxy)
				{
					pProxy->Serialize( ser );
				}

				proxyNrSerialized++;
			}
			assert(proxyNrSerialized == ENTITY_PROXY_LAST);	//this checks whether every proxy is in the order list

			ser.EndGroup(); //EntityProxies
		}

		//////////////////////////////////////////////////////////////////////////
		// @HACK Horrible Hack for usables on BasicEntity.
		//////////////////////////////////////////////////////////////////////////
		{
			IScriptTable *pScriptTable = GetScriptTable();
			if (pScriptTable)
			{
				if (ser.IsWriting())
				{
					if (pScriptTable->HaveValue("__usable"))
					{
						bool bUsable = false;
						pScriptTable->GetValue("__usable",bUsable);
						int nUsable = (bUsable) ? 1 : -1;
						ser.Value( "__usable",nUsable );
					}
				}
				else
				{
					int nUsable = 0;
					ser.Value( "__usable",nUsable );
					if (nUsable == 1)
						pScriptTable->SetValue("__usable",1 );
					else if (nUsable == -1)
						pScriptTable->SetValue("__usable",0 );
					else
						pScriptTable->SetToNull("__usable");
				}
			}
		}
		//////////////////////////////////////////////////////////////////////////

		if(m_pAIObject)
		{
			// serialization of AI is done within AI, but AI needs to be bale to work
			// out how to link up its objects with us.
			// This call should be a nop, but it's made here just so that I can write this
			// comment...
			m_pAIObject->SetEntityID(m_nID);

			ICharacterInstance* pICharacterInstance = GetCharacter(0);
			if (pICharacterInstance)
			{
				pICharacterInstance->Serialize(ser);
			}
		}
	}

	if (nFlags & ENTITY_SERIALIZE_PROPERTIES)
	{
		CScriptProxy *pScriptProxy = (CScriptProxy*)GetProxyAt(ENTITY_PROXY_SCRIPT);
		if (pScriptProxy)
			pScriptProxy->SerializeProperties(ser);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::Physicalize( SEntityPhysicalizeParams &params )
{
	if (!GetPhysicalProxy())
		CreateProxy(ENTITY_PROXY_PHYSICS);
	GetPhysicalProxy()->Physicalize( params );
}

//////////////////////////////////////////////////////////////////////////
void CEntity::EnablePhysics(bool enable)
{
	CPhysicalProxy* pPhysProxy = (CPhysicalProxy*)GetProxy(ENTITY_PROXY_PHYSICS);
	if(pPhysProxy)
	{
		if(enable)
			pPhysProxy->SetFlags(pPhysProxy->GetFlags()&~CPhysicalProxy::FLAG_PHYSICS_DISABLED);
		else 
			pPhysProxy->SetFlags(pPhysProxy->GetFlags()|CPhysicalProxy::FLAG_PHYSICS_DISABLED);
	}
}

//////////////////////////////////////////////////////////////////////////
IPhysicalEntity* CEntity::GetPhysics() const
{
	if (GetPhysicalProxy())
		return GetPhysicalProxy()->GetPhysicalEntity();
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
// Custom entity material.
//////////////////////////////////////////////////////////////////////////
void CEntity::SetMaterial( IMaterial *pMaterial )
{
	m_pMaterial = pMaterial;

	SEntityEvent event;
	event.event = ENTITY_EVENT_MATERIAL;
	event.nParam[0] = (INT_PTR)pMaterial;
	SendEvent( event );
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CEntity::GetMaterial()
{
	return m_pMaterial;
}

//////////////////////////////////////////////////////////////////////////
int CEntity::PhysicalizeSlot(int slot, SEntityPhysicalizeParams &params)
{
	if (CPhysicalProxy* proxy = GetPhysicalProxy())
	{
		return proxy->AddSlotGeometry(slot, params);
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::UnphysicalizeSlot(int slot)
{
	if (CPhysicalProxy* proxy = GetPhysicalProxy())
	{
		proxy->RemoveSlotGeometry(slot);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::UpdateSlotPhysics(int slot)
{
	if (CPhysicalProxy* proxy = GetPhysicalProxy())
	{
		proxy->UpdateSlotGeometry(slot,GetStatObj(slot));
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::IsSlotValid( int nSlot ) const
{
	if (GetRenderProxy())
		return GetRenderProxy()->IsSlotValid(nSlot);
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::FreeSlot( int nSlot )
{
	if (GetRenderProxy())
		GetRenderProxy()->FreeSlot(nSlot);
}

//////////////////////////////////////////////////////////////////////////
int  CEntity::GetSlotCount() const
{
	if (GetRenderProxy())
		return GetRenderProxy()->GetSlotCount();
	return false;
}

//////////////////////////////////////////////////////////////////////////
CEntityObject* CEntity::GetSlot( int nSlot ) const
{
	if (GetRenderProxy())
		return GetRenderProxy()->GetSlot(nSlot);
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::GetSlotInfo( int nSlot,SEntitySlotInfo &slotInfo ) const
{
	if (GetRenderProxy())
		return GetRenderProxy()->GetSlotInfo(nSlot,slotInfo);
	return false;
}

//////////////////////////////////////////////////////////////////////////
const Matrix34& CEntity::GetSlotWorldTM( int nSlot ) const
{
	if (GetRenderProxy())
		return GetRenderProxy()->GetSlotWorldTM(nSlot);
	return sIdentityMatrix;
}

//////////////////////////////////////////////////////////////////////////
const Matrix34& CEntity::GetSlotLocalTM( int nSlot, bool bRelativeToParent ) const
{
	if (GetRenderProxy())
		return GetRenderProxy()->GetSlotLocalTM(nSlot, bRelativeToParent);
	return sIdentityMatrix;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetSlotLocalTM( int nSlot,const Matrix34 &localTM,int nWhyFlags )
{
	CHECKQNAN_VEC(localTM.GetColumn(0));
	CHECKQNAN_VEC(localTM.GetColumn(1));
	CHECKQNAN_VEC(localTM.GetColumn(2));
	CHECKQNAN_VEC(localTM.GetColumn(3));

	if (GetRenderProxy())
		return GetRenderProxy()->SetSlotLocalTM(nSlot,localTM);
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::SetParentSlot( int nParentSlot,int nChildSlot )
{
	if (GetRenderProxy())
		return GetRenderProxy()->SetParentSlot(nParentSlot,nChildSlot);
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetSlotMaterial( int nSlot,IMaterial *pMaterial )
{
	if (GetRenderProxy())
		GetRenderProxy()->SetSlotMaterial(nSlot,pMaterial);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetSlotFlags( int nSlot,uint32 nFlags )
{
	if (GetRenderProxy())
		return GetRenderProxy()->SetSlotFlags(nSlot,nFlags);
}

//////////////////////////////////////////////////////////////////////////
uint32 CEntity::GetSlotFlags( int nSlot ) const
{
	if (GetRenderProxy())
		return GetRenderProxy()->GetSlotFlags(nSlot);
	return 0;
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::ShouldUpdateCharacter( int nSlot) const
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
ICharacterInstance* CEntity::GetCharacter( int nSlot )
{
	if (GetRenderProxy())
		return GetRenderProxy()->GetCharacter(nSlot);
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
int CEntity::SetCharacter( ICharacterInstance *pCharacter, int nSlot )
{
	int nUsedSlot = -1;

	if (!GetRenderProxy())
		CreateProxy(ENTITY_PROXY_RENDER);

	nUsedSlot = GetRenderProxy()->SetSlotCharacter(nSlot, pCharacter);
	if (GetPhysicalProxy())
		GetPhysicalProxy()->UpdateSlotGeometry(nUsedSlot);

	return nUsedSlot;
}

//////////////////////////////////////////////////////////////////////////
IStatObj* CEntity::GetStatObj( int nSlot )
{
	if (GetRenderProxy())
		return GetRenderProxy()->GetStatObj(nSlot);
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
int CEntity::SetStatObj( IStatObj *pStatObj, int nSlot,bool bUpdatePhysics, float mass )
{
	if (!GetRenderProxy())
		CreateProxy(ENTITY_PROXY_RENDER);

	nSlot = GetRenderProxy()->SetSlotGeometry(nSlot,pStatObj);
	if (bUpdatePhysics && GetPhysicalProxy())
		GetPhysicalProxy()->UpdateSlotGeometry(nSlot,pStatObj,mass);

	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
IParticleEmitter* CEntity::GetParticleEmitter( int nSlot )
{
	if (GetRenderProxy())
		return GetRenderProxy()->GetParticleEmitter(nSlot);
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
int CEntity::LoadGeometry( int nSlot,const char *sFilename,const char *sGeomName,int nLoadFlags )
{
	if (!GetRenderProxy())
		CreateProxy(ENTITY_PROXY_RENDER);
	nSlot = GetRenderProxy()->LoadGeometry( nSlot,sFilename,sGeomName,nLoadFlags );

	if (nLoadFlags & EF_AUTO_PHYSICALIZE)
	{
		// Also physicalize geometry.
		SEntityPhysicalizeParams params;
		params.nSlot = nSlot;
		IStatObj *pStatObj = GetStatObj(ENTITY_SLOT_ACTUAL|nSlot);
		float mass,density;
		if (!pStatObj || pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND ||
			  pStatObj->GetPhysicalProperties(mass,density) && (mass>=0 || density>=0))
			params.type = PE_RIGID;
		else
			params.type = PE_STATIC;
		Physicalize( params );

		IPhysicalEntity *pe=GetPhysics();
		if (pe)
		{
			pe_action_awake aa;
			aa.bAwake = 0;
			pe->Action(&aa);
			pe_params_foreign_data pfd;
			pfd.iForeignFlagsOR = PFF_UNIMPORTANT;
			pe->SetParams(&pfd);
		}

		// Mark as AI hideable unless the object flags are preventing it.
		if (pStatObj && (pStatObj->GetFlags() & STATIC_OBJECT_NO_AUTO_HIDEPOINTS) == 0)
			AddFlags(ENTITY_FLAG_AI_HIDEABLE);
	}
	
	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
int CEntity::LoadCharacter( int nSlot,const char *sFilename,int nLoadFlags )
{
	if (!GetRenderProxy())
		CreateProxy(ENTITY_PROXY_RENDER);
	ICharacterInstance *pChar;
	if ((pChar = GetRenderProxy()->GetCharacter(nSlot)) && GetPhysicalProxy() && pChar->GetISkeletonPose()->GetCharacterPhysics()==GetPhysics())
		GetPhysicalProxy()->AttachToPhysicalEntity(0);

	nSlot = GetRenderProxy()->LoadCharacter( nSlot,sFilename,nLoadFlags );
	if (nLoadFlags & EF_AUTO_PHYSICALIZE)
	{
		ICharacterInstance *pCharacter = GetCharacter(nSlot);
		if (pCharacter)
			pCharacter->SetFlags(pCharacter->GetFlags()|CS_FLAG_UPDATE_ONRENDER);

		// Also physicalize geometry.
		SEntityPhysicalizeParams params;
		params.nSlot = nSlot;
		params.type = PE_RIGID;
		Physicalize( params );

		IPhysicalEntity *pe=GetPhysics();
		if (pe)
		{
			pe_action_awake aa;
			aa.bAwake = 0;
			pe->Action(&aa);
			pe_params_foreign_data pfd;
			pfd.iForeignFlagsOR = PFF_UNIMPORTANT;
			pe->SetParams(&pfd);
		}
	}
	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
int	CEntity::SetParticleEmitter( int nSlot, IParticleEmitter* pEmitter, bool bSerialize )
{
	if (!GetRenderProxy())
		CreateProxy(ENTITY_PROXY_RENDER);

	return GetRenderProxy()->SetParticleEmitter( nSlot, pEmitter, bSerialize );
}

//////////////////////////////////////////////////////////////////////////
int	CEntity::LoadParticleEmitter( int nSlot, IParticleEffect *pEffect, SpawnParams const* params, bool bPrime, bool bSerialize)
{
	if (!GetRenderProxy())
		CreateProxy(ENTITY_PROXY_RENDER);
	return GetRenderProxy()->LoadParticleEmitter( nSlot, pEffect, params, bPrime, bSerialize );
}

//////////////////////////////////////////////////////////////////////////
int CEntity::LoadLight( int nSlot,CDLight *pLight )
{
	if (!GetRenderProxy())
		CreateProxy(ENTITY_PROXY_RENDER);
	return GetRenderProxy()->LoadLight( nSlot,pLight );
}

//////////////////////////////////////////////////////////////////////////
int CEntity::LoadCloud( int nSlot,const char *sFilename )
{
	if (!GetRenderProxy())
		CreateProxy(ENTITY_PROXY_RENDER);
	return GetRenderProxy()->LoadCloud( nSlot,sFilename );
}

//////////////////////////////////////////////////////////////////////////
int CEntity::SetCloudMovementProperties(int nSlot, const SCloudMovementProperties& properties)
{
	if (!GetRenderProxy())
		CreateProxy(ENTITY_PROXY_RENDER);
	return GetRenderProxy()->SetCloudMovementProperties(nSlot, properties);
}

//////////////////////////////////////////////////////////////////////////
int CEntity::LoadFogVolume( int nSlot, const SFogVolumeProperties& properties )
{
	if (!GetRenderProxy())
		CreateProxy(ENTITY_PROXY_RENDER);
	return GetRenderProxy()->LoadFogVolume( nSlot, properties );
}

//////////////////////////////////////////////////////////////////////////
int CEntity::FadeGlobalDensity( int nSlot, float fadeTime, float newGlobalDensity )
{
	if (!GetRenderProxy())
		CreateProxy(ENTITY_PROXY_RENDER);
	return GetRenderProxy()->FadeGlobalDensity(nSlot, fadeTime, newGlobalDensity);
}

//////////////////////////////////////////////////////////////////////////
int CEntity::LoadVolumeObject(int nSlot, const char *sFilename)
{
	if (!GetRenderProxy())
		CreateProxy(ENTITY_PROXY_RENDER);
	return GetRenderProxy()->LoadVolumeObject(nSlot, sFilename);
}

//////////////////////////////////////////////////////////////////////////
int CEntity::SetVolumeObjectMovementProperties(int nSlot, const SVolumeObjectMovementProperties& properties)
{
	if (!GetRenderProxy())
		CreateProxy(ENTITY_PROXY_RENDER);
	return GetRenderProxy()->SetVolumeObjectMovementProperties(nSlot, properties);
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::RegisterInAISystem(unsigned short type, const AIObjectParameters &params)
{
	m_flags &= ~ENTITY_FLAG_HAS_AI;
	IAISystem *pSystem = g_pIEntitySystem->GetSystem()->GetAISystem();
	if (pSystem)
	{
		if (m_pAIObject)
		{
			pSystem->RemoveObject(m_pAIObject);
			m_pAIObject = 0;
		}

		if (type == 0)
			return true;

//		if (m_pAIObject = pSystem->CreateAIObject(type, this))
		if (m_pAIObject = pSystem->CreateAIObject(type, NULL))
		{
			m_flags |= ENTITY_FLAG_HAS_AI;

			m_pAIObject->SetEntityID(m_nID);
			IAIActor* pAIActor = m_pAIObject->CastToIAIActor();
			if(pAIActor)
				pAIActor->ParseParameters(params);
			m_pAIObject->SetName(m_szName);

			UpdateAIObject();

			// Send a (fake) transform event since smart objects system uses AI object position which is slightly different
		//	SEntityEvent event(ENTITY_EVENT_XFORM);
		//	event.nParam[0] = 0;
		//	SendEvent( event );

			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// reflect changes in position or orientation to the AI object
void CEntity::UpdateAIObject( )
{
	if (!m_pAIObject)
		return;
	Vec3 dirForw = GetWorldTM().TransformVector(FORWARD_DIRECTION);
	m_pAIObject->SetPos(GetWorldPos(), dirForw);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::ActivateForNumUpdates( int numUpdates )
{
	if (m_bActive)
		return;

	if(m_pAIObject && m_pAIObject->GetProxy())
		return;

	if (m_nUpdateCounter != 0)
	{
		m_nUpdateCounter = numUpdates;
		return;
	}
	
	m_nUpdateCounter = numUpdates;
	SetUpdateStatus();
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetPhysicsState( XmlNodeRef & physicsState)
{
	if(physicsState)
	{
		IPhysicalEntity *physic = GetPhysics();
		if (!physic && GetCharacter(0))
			physic = GetCharacter(0)->GetISkeletonPose()->GetCharacterPhysics(0);
		if(physic )
		{
			IXmlSerializer * pSerializer = GetISystem()->GetXmlUtils()->CreateXmlSerializer();
			if (pSerializer)
			{
				ISerialize * ser = pSerializer->GetReader(physicsState);
				physic->SetStateFromSnapshot(ser);
				physic->PostSetStateFromSnapshot();
				pSerializer->Release();
				for(int i=GetSlotCount()-1;i>=0;i--) if (GetSlot(i)->pCharacter)
					if (GetSlot(i)->pCharacter->GetISkeletonPose()->GetCharacterPhysics()==physic)
						GetSlot(i)->pCharacter->GetISkeletonPose()->SynchronizeWithPhysicalEntity(physic);
					else if (GetSlot(i)->pCharacter->GetISkeletonPose()->GetCharacterPhysics(0)==physic)
						GetSlot(i)->pCharacter->GetISkeletonPose()->SynchronizeWithPhysicalEntity(0,m_vPos,m_qRotation);
				ActivateForNumUpdates(5);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IEntityLink* CEntity::GetEntityLinks()
{
	return m_pEntityLinks;
};

//////////////////////////////////////////////////////////////////////////
IEntityLink* CEntity::AddEntityLink( const char *sLinkName,EntityId entityId )
{
	IEntityLink *pNewLink = new IEntityLink;
	strncpy( pNewLink->name,sLinkName,sizeof(pNewLink->name) );
	pNewLink->name[sizeof(pNewLink->name)-1] = 0; // Null terminate string.
	pNewLink->entityId = entityId;
	pNewLink->next = 0;

	if (m_pEntityLinks)
	{
		IEntityLink *pLast = m_pEntityLinks;
		while (pLast->next) pLast = pLast->next;
		pLast->next = pNewLink;
	}
	else
	{
		m_pEntityLinks = pNewLink;
	}

	return pNewLink;
};

//////////////////////////////////////////////////////////////////////////
void CEntity::RemoveEntityLink( IEntityLink* pLink )
{
	if (!m_pEntityLinks || !pLink)
		return;

	if (m_pEntityLinks == pLink)
	{
		m_pEntityLinks = m_pEntityLinks->next;
	}
	else
	{
		IEntityLink *pLast = m_pEntityLinks;
		while (pLast->next && pLast->next != pLink) pLast = pLast->next;
		if (pLast->next == pLink)
		{
			pLast->next = pLink->next;
		}
	}

	delete pLink;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::RemoveAllEntityLinks()
{
	IEntityLink *pLink = m_pEntityLinks;
	while (pLink)
	{
		IEntityLink *pNext = pLink->next;
		delete pLink;
		pLink = pNext;
	}
	m_pEntityLinks = 0;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::GetMemoryStatistics( ICrySizer *pSizer )
{
	int nSize = sizeof(*this);
	if (GetRenderProxy())
		GetRenderProxy()->GetMemoryStatistics( pSizer );

	if (GetPhysicalProxy())
		nSize += sizeof(CPhysicalProxy);
	if (GetScriptProxy())
		nSize += sizeof(CScriptProxy);
	
	pSizer->AddObject(this, nSize);
}

//////////////////////////////////////////////////////////////////////////
#define DEF_ENTITY_EVENT_NAME(x) {x,#x}
struct SEventName
{
	EEntityEvent event;
	const char *name;
} s_eventsName[] = 
{
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_XFORM),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_TIMER),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_INIT),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_DONE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_VISIBLITY),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_RESET),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ATTACH),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_DETACH),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_DETACH_THIS),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_HIDE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_UNHIDE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ENABLE_PHYSICS),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_PHYSICS_CHANGE_STATE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_SCRIPT_EVENT),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ENTERAREA),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_LEAVEAREA),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ENTERNEARAREA),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_LEAVENEARAREA),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_MOVEINSIDEAREA),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_MOVENEARAREA),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_PHYS_POSTSTEP),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_PHYS_BREAK),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_AI_DONE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_SOUND_DONE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_NOT_SEEN_TIMEOUT),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_COLLISION),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_RENDER),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_PREPHYSICSUPDATE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_START_LEVEL),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_START_GAME),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ENTER_SCRIPT_STATE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_LEAVE_SCRIPT_STATE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_PRE_SERIALIZE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_POST_SERIALIZE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_INVISIBLE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_VISIBLE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_MATERIAL),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ONHIT),
};

//////////////////////////////////////////////////////////////////////////
void CEntity::LogEvent( SEntityEvent &event,CTimeValue dt )
{
	static int s_LastLoggedFrame = 0;

	int nFrameId = gEnv->pRenderer->GetFrameID(false);
	if (CVar::es_DebugEvents == 2 && s_LastLoggedFrame < nFrameId && s_LastLoggedFrame > nFrameId-10)
	{
		// On Next frame turn it off.
		CVar::es_DebugEvents = 0;
		return;
	}

	// Find event, quite slow but ok for logging.
	char sNameId[32];
	const char *sName = "";
	for (int i = 0; i < sizeof(s_eventsName)/sizeof(s_eventsName[0]); i++)
	{
		if (s_eventsName[i].event == event.event)
		{
			sName = s_eventsName[i].name;
			break;
		}
	}
	if (!*sName)
	{
		sprintf( sNameId,"%d",(int)event.event );
		sName = sNameId;
	}
	s_LastLoggedFrame = nFrameId;

	float fTimeMs = dt.GetMilliSeconds();
	CryLogAlways( "<Frame:%d><EntityEvent> [%s](%X)\t[%.2fms]\t%s",nFrameId,sName,(int)event.nParam[0],fTimeMs,GetEntityTextDescription() );}
