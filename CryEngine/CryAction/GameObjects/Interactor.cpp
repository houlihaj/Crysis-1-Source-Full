#include "StdAfx.h"
#include "Interactor.h"
#include "CryAction.h"
#include "IGameRulesSystem.h"
#include "WorldQuery.h"
#include "IActorSystem.h"
#include "CryActionCVars.h"

CInteractor::CInteractor()
{
	m_pQuery = 0;
	m_pGameRules = 0;

	m_nextOverId = m_overId = 0;
	m_nextOverIdx = m_overIdx = -100;
	m_nextOverTime = m_overTime = 0.0f;
	m_sentLongHover = m_sentMessageHover = false;

	m_funcIsUsable = m_funcOnNewUsable = m_funcOnUsableMessage = m_funcOnLongHover = 0;

	m_pTimer = gEnv->pTimer;
	m_pEntitySystem = gEnv->pEntitySystem;

	m_useHoverTime = 0.05f;
	m_unUseHoverTime = 0.2f;
	m_messageHoverTime = 0.05f;
	m_longHoverTime = 5.0f;

	m_lockedByEntityId = m_lockEntityId = 0;
	m_lockIdx = 0;

	m_lastRadius = 2.0f;

	m_queryMethods = "rb";
}

bool CInteractor::Init( IGameObject * pGameObject )
{
	SetGameObject( pGameObject );
	m_pQuery = (CWorldQuery*) GetGameObject()->AcquireExtension("WorldQuery");
	if (!m_pQuery)
		return false;

	m_pQuery->SetProximityRadius(CCryActionCVars::Get().playerInteractorRadius);

	m_pGameRules = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRulesEntity()->GetScriptTable();
	m_pGameRules->GetValue( "IsUsable", m_funcIsUsable );
	m_pGameRules->GetValue( "OnNewUsable", m_funcOnNewUsable );
	m_pGameRules->GetValue( "OnUsableMessage", m_funcOnUsableMessage );
	m_pGameRules->GetValue( "OnLongHover", m_funcOnLongHover );

	return true;
}

void CInteractor::PostInit( IGameObject * pGameObject )
{
	pGameObject->EnableUpdateSlot( this, 0 );
}

CInteractor::~CInteractor()
{
	if (m_pQuery)
		GetGameObject()->ReleaseExtension("WorldQuery");
}

void CInteractor::Release()
{
	delete this;
}

ScriptAnyValue CInteractor::EntityIdToScript( EntityId id )
{
	if (id)
	{
		ScriptHandle hdl;
		hdl.n = id;
		return ScriptAnyValue(hdl);
	}
	else
	{
		return ScriptAnyValue();
	}
}

void CInteractor::Update( SEntityUpdateContext&, int )
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	EntityId newOverId = 0;
	int usableIdx = 0;

	if (m_lockedByEntityId)
	{
		newOverId = m_lockEntityId;
		usableIdx = m_lockIdx;
	}
	else
	{
		if((CCryActionCVars::Get().playerInteractorRadius != m_lastRadius) && m_pQuery)
		{
			m_lastRadius = CCryActionCVars::Get().playerInteractorRadius;
			m_pQuery->SetProximityRadius(m_lastRadius);
		}
			
		PerformQueries(newOverId, usableIdx);
	}
	UpdateTimers(newOverId, usableIdx);
}

void CInteractor::PerformQueries( EntityId& id, int& idx )
{
	SQueryResult bestResult;
	bestResult.entityId = 0;
	bestResult.slotIdx = 0;
	bestResult.hitDistance = 1e12f;
	for (string::const_iterator iter = m_queryMethods.begin(); iter != m_queryMethods.end(); ++iter)
	{
		bool found;
		SQueryResult result;
		switch (*iter)
		{
		case 'r':
			found = PerformRayCastQuery( result );
			break;
		case 'b':
			found = PerformBoundingBoxQuery( result );
			break;
		default:
			GameWarning("Interactor: Invalid query mode '%c'", *iter);
			found = false;
		}
		if (found && result.hitDistance < bestResult.hitDistance)
		{
			if (bestResult.entityId && !result.entityId)
				;
			else
				bestResult = result;
		}
	}
	id = bestResult.entityId;
	idx = bestResult.slotIdx;
}

bool CInteractor::PerformRayCastQuery( SQueryResult& r )
{
	if (const ray_hit * pHit = m_pQuery->RaycastQuery())
	{
		r.hitPosition = pHit->pt;
		r.hitDistance = pHit->dist;
		PerformUsableTestAndCompleteIds( m_pEntitySystem->GetEntityFromPhysics(pHit->pCollider), r );
		return true;
	}
	return false;
}

bool CInteractor::PerformBoundingBoxQuery( SQueryResult& r )
{
	const Entities &entities = m_pQuery->GetEntitiesInFrontOf();

	for (Entities::const_iterator i = entities.begin(); i != entities.end(); ++i)
	{
		IEntity *pEntity = m_pEntitySystem->GetEntity(*i);

		if (!pEntity)
			continue;

		AABB bbox;
		pEntity->GetWorldBounds(bbox);
		r.hitPosition = bbox.GetCenter();
		r.hitDistance = (r.hitPosition - m_pQuery->GetPos()).GetLengthSquared();
		
		if (PerformUsableTestAndCompleteIds( pEntity, r ))
			return true;
	}

	return false;
}

bool CInteractor::PerformUsableTestAndCompleteIds( IEntity * pEntity, SQueryResult& r ) const
{
	bool ok = false;
	r.entityId = 0;
	r.slotIdx = 0;
	if (pEntity && m_funcIsUsable)
	{
		SmartScriptTable pScriptTable = pEntity->GetScriptTable();
		if (pScriptTable.GetPtr())
		{
			int usableIdx;
			bool scriptOk = Script::CallReturn(
				m_pGameRules->GetScriptSystem(), 
				m_funcIsUsable, 
				m_pGameRules, 
				EntityIdToScript(GetEntityId()), 
				EntityIdToScript(pEntity->GetId()), 
				usableIdx);
			if (scriptOk && usableIdx)
			{
				r.entityId = pEntity->GetId();
				r.slotIdx = usableIdx;
				ok = true;
			}
		}
	}
	return ok;
}

bool CInteractor::IsUsable(EntityId entityId) const
{
	IEntity *pEntity=m_pEntitySystem->GetEntity(entityId);

	static SQueryResult dummy;
	return PerformUsableTestAndCompleteIds(pEntity, dummy);
}

void CInteractor::UpdateTimers( EntityId newOverId, int usableIdx )
{
	CTimeValue now = m_pTimer->GetFrameStartTime();

	if (newOverId != m_nextOverId || usableIdx != m_nextOverIdx)
	{
		m_nextOverId = newOverId;
		m_nextOverIdx = usableIdx;
		m_nextOverTime = now;
	}
	float compareTime = m_nextOverId? m_useHoverTime : m_unUseHoverTime;
	if ((m_nextOverId != m_overId || m_nextOverIdx != m_overIdx) && (now - m_nextOverTime).GetSeconds() > compareTime)
	{
		m_overId = m_nextOverId;
		m_overIdx = m_nextOverIdx;
		m_overTime = m_nextOverTime;
		m_sentMessageHover = m_sentLongHover = false;
		if (m_funcOnNewUsable)
			Script::CallMethod(m_pGameRules, m_funcOnNewUsable, EntityIdToScript(GetEntityId()), EntityIdToScript(m_overId), m_overIdx);
	}
	if (m_funcOnUsableMessage && !m_sentMessageHover && (now - m_overTime).GetSeconds() > m_messageHoverTime)
	{
		Script::CallMethod(m_pGameRules, m_funcOnUsableMessage, EntityIdToScript(GetEntityId()), EntityIdToScript(m_overId), m_overId, m_overIdx);

		m_sentMessageHover = true;
	}
	if (m_funcOnLongHover && !m_sentLongHover && (now - m_overTime).GetSeconds() > m_longHoverTime)
	{
		Script::CallMethod(m_pGameRules, m_funcOnLongHover, EntityIdToScript(GetEntityId()), EntityIdToScript(m_overId), m_overIdx);
		m_sentLongHover = true;
	}
}

void CInteractor::FullSerialize( TSerialize ser )
{
	ser.Value("useHoverTime", m_useHoverTime);
	ser.Value("unUseHoverTime", m_unUseHoverTime);
	ser.Value("messageHoverTime", m_messageHoverTime);
	ser.Value("longHoverTime", m_longHoverTime);
	ser.Value("queryMethods", m_queryMethods);
	ser.Value("lastRadius", m_lastRadius);

	if (ser.GetSerializationTarget() == eST_Script && ser.IsReading())
	{
		EntityId lockedById = 0, lockId = 0;
		int lockIdx = 0;
		ser.Value("locker", lockedById);
		if (lockedById)
		{
			if (m_lockedByEntityId && lockedById != m_lockedByEntityId)
			{
				GameWarning("Attempt to change lock status by an entity that did not lock us originally");
			}
			else
			{
				ser.Value("lockId", lockId);
				ser.Value("lockIdx", lockIdx);
				if (lockId && lockIdx)
				{
					m_lockedByEntityId = lockedById;
					m_lockEntityId = lockId;
					m_lockIdx = lockIdx;
				}
				else
				{
					m_lockedByEntityId = 0;
					m_lockEntityId = 0;
					m_lockIdx = 0;
				}
			}
		}
	}

	// the following should not be used as parameters
	if (ser.GetSerializationTarget() != eST_Script)
	{
		ser.Value("nextOverId", m_nextOverId);
		ser.Value("nextOverIdx", m_nextOverIdx);
		ser.Value("nextOverTime", m_nextOverTime);
		ser.Value("overId", m_overId);
		ser.Value("overIdx", m_overIdx);
		ser.Value("overTime", m_overTime);
		ser.Value("sentMessageHover", m_sentMessageHover);
		ser.Value("sentLongHover", m_sentLongHover);

		ser.Value("m_lockedByEntityId", m_lockedByEntityId);
		ser.Value("m_lockEntityId", m_lockEntityId);
		ser.Value("m_lockIdx", m_lockIdx);
	}
}

bool CInteractor::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	return true;
}

void CInteractor::HandleEvent( const SGameObjectEvent& )
{
	
}

void CInteractor::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->Add(m_queryMethods);
}

void CInteractor::PostSerialize()
{
	//?fix? : was invalid sometimes after QL
	m_pQuery = (CWorldQuery*) GetGameObject()->AcquireExtension("WorldQuery");

	if (m_funcOnNewUsable)
		Script::CallMethod(m_pGameRules, m_funcOnNewUsable, EntityIdToScript(GetEntityId()), EntityIdToScript(m_overId), m_overIdx);
}

