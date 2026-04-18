/********************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
File name:   AIActor.cpp
$Id$
Description: implementation of the CAIActor class.

-------------------------------------------------------------------------
History:
14:12:2006 -  Created by Kirill Bulatsev


*********************************************************************/
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "AIActor.h"
#include "CAISystem.h"
#include "AILog.h"
#include "Graph.h"
#include "Leader.h"
#include "ObjectTracker.h"
#include "GoalOp.h"

#include <float.h>
#include <ISystem.h>
#include <ILog.h>
#include <ISerialize.h>
#include "PersonalInterestManager.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#define _ser_value_(val) ser.Value(#val, val)


CAIActor::CAIActor(CAISystem* pAISystem):
	CAIObject(pAISystem),
	m_pProxy(NULL),
	m_healthHistory(0),
	m_pPersonalInterestManager(NULL),
	m_lightLevel(AILL_LIGHT),
	m_usingCombatLight(false),
	m_minHostileRange(0),
	m_maxHostileRange(0),
	m_degenHostileRange(0),
	m_hostilityIncrease(0),
	m_hostilityCooldown(0.0f),
	m_lastHostilityTime(0.0f)
{
	_fastcast_CAIActor = true;

	memset(m_hostilityRestrictions, 0, MAX_RESTRICTED_SPECIES*sizeof(float));
	memset(m_ignoreSpecies, 0, MAX_RESTRICTED_SPECIES*sizeof(bool));
//#if !defined(USER_dejan) && !defined(USER_mikko)
//	AILogComment("CAIObject (%p)", this);
//#endif
}

CAIActor::~CAIActor()
{
//#if !defined(USER_dejan) && !defined(USER_mikko)
//	AILogComment("~CAIObject  %s (%p)", m_sName.c_str(), this);
//#endif

	if (GetProxy())
		GetProxy()->Release();

	CAIGroup* pGroup = GetAISystem()->GetAIGroup(GetGroupId());
	if(pGroup)
		pGroup->RemoveMember(this);

	delete m_healthHistory;
	// Any PIM is owned by the CentralInterestManager - _don't_ delete but do unassign
	if (m_pPersonalInterestManager) 
		m_pPersonalInterestManager->Assign(NULL);
}


//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::Reset(EObjectResetType type)
{
	CAIObject::Reset(type);
	m_lastNavNodeIndex = 0;
	m_lastHideNodeIndex = 0;

	int i = m_State.vSignals.size();
	while ( i-- )
	{
		IAISignalExtraData* pData = m_State.vSignals[i].pEData;
		if ( pData )
			delete (AISignalExtraData*) pData;
	}
	m_State.FullReset();

	ReleaseFormation();

	if(GetProxy())
		GetProxy()->Reset(type);

	m_bEnabled = true;
	m_bUpdatedOnce = false;

	if(m_healthHistory) 
		m_healthHistory->Reset();

	if(m_pPersonalInterestManager) 
	{
		m_pPersonalInterestManager->Reset(); // Possibly return to CIM also
		m_pPersonalInterestManager = NULL;
	}

	// synch self with owner entity if there is one
	IEntity* pEntity(GetEntity());
	if(pEntity)
	{
		m_bEnabled = pEntity->IsActive();
		SetPos(pEntity->GetPos());
	}

	m_lightLevel = AILL_LIGHT;
	m_usingCombatLight = false;

	m_detectionMax.Reset();
	m_detectionSnapshot.Reset();

	m_priorityTargets.clear();
	m_probableTargets.clear();

	memset(m_hostilityRestrictions, 0, MAX_RESTRICTED_SPECIES*sizeof(float));
	memset(m_ignoreSpecies, 0, MAX_RESTRICTED_SPECIES*sizeof(bool));

	m_minHostileRange=0;
	m_maxHostileRange=0;
	m_degenHostileRange=0;
	m_hostilityIncrease=0;
	m_hostilityCooldown=0.0f;
	m_lastHostilityTime=0.0f;
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::ParseParameters(const AIObjectParameters &params)
{
	SetGroupId(params.m_sParamStruct.m_nGroup);
	m_movementAbility = params.m_moveAbility;
}


//====================================================================
// CAIObject::OnObjectRemoved
//====================================================================
void CAIActor::OnObjectRemoved(CAIObject *pObject )
{
	CAIObject::OnObjectRemoved(pObject);

	// make sure no pending signal left from removeed AIObjects
	IEntity* pRemovedEntity(pObject->GetEntity());
	if(pRemovedEntity && !m_State.vSignals.empty())
	{
		for (int idx(0); idx<m_State.vSignals.size(); ++idx)
		{
			AISIGNAL &curSignal(m_State.vSignals[idx]);
			if(curSignal.pSender != pRemovedEntity)
				continue;
			DynArray<AISIGNAL>::iterator ai(m_State.vSignals.begin());
			std::advance(ai, idx);
			IAISignalExtraData* pData = curSignal.pEData;
			if ( pData )
				delete (AISignalExtraData*) pData;
			m_State.vSignals.erase(ai);
			--idx;
		}
	}

	for (unsigned i = 0; i < m_probableTargets.size(); )
	{
		if (m_probableTargets[i].pTarget == pObject)
		{
			m_probableTargets[i] = m_probableTargets.back();
			m_probableTargets.pop_back();
		}
		else
			++i;
	}

	for (unsigned i = 0; i < m_priorityTargets.size(); )
	{
		if (m_priorityTargets[i].pTarget == pObject)
		{
			m_priorityTargets[i] = m_priorityTargets.back();
			m_priorityTargets.pop_back();
		}
		else
			++i;
	}
}



//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::Update(EObjectUpdate type)
{
	if (m_pProxy)
		m_pProxy->Update(&m_State);
	if(GetAISystem()->m_cvDebugDrawDamageControl->GetIVal() > 0)
		UpdateHealthHistory();

	if (type == AIUPDATE_FULL)
		m_lightLevel = GetAISystem()->GetLightManager()->GetLightLevelAt(GetPos(), this, &m_usingCombatLight);
	UpdateCloakScale();
}


//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::UpdateCloakScale()
{
	float	delta(m_Parameters.m_fCloakScaleTarget - m_Parameters.m_fCloakScale);
		if( fabsf(delta)>.05f ) 
		{
			float	scaleStep(GetAISystem()->GetFrameDeltaTime()*.5f);
			if(delta>0.f)
				m_Parameters.m_fCloakScale = scaleStep<delta ? (m_Parameters.m_fCloakScale + scaleStep) : m_Parameters.m_fCloakScaleTarget;
			else
				m_Parameters.m_fCloakScale = scaleStep<-delta ? (m_Parameters.m_fCloakScale - scaleStep) : m_Parameters.m_fCloakScaleTarget;
		}
		else
			m_Parameters.m_fCloakScale = m_Parameters.m_fCloakScaleTarget;
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::UpdateDisabled(EObjectUpdate type)
{
FUNCTION_PROFILER( GetISystem(),PROFILE_AI );
	if (m_pProxy)
		m_pProxy->CheckUpdateStatus();
}

//===================================================================
// SetNavNodes
//===================================================================
void CAIActor::UpdateDamageParts(DamagePartVector& parts)
{
	if(!GetProxy())
		return;

	IPhysicalEntity*	phys = GetProxy()->GetPhysics(true);
	if(!phys)
		return;

	bool	queryDamageValues = true;

	static ISurfaceTypeManager* pSurfaceMan = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();

	pe_status_nparts statusNParts;
	int nParts = phys->GetStatus(&statusNParts);

	if((int)parts.size() != nParts)
	{
		parts.resize(nParts);
		queryDamageValues = true;
	}

	// The global damage table
	SmartScriptTable	pSinglePlayerTable;
	SmartScriptTable	pDamageTable;
	if(queryDamageValues)
	{
		gEnv->pScriptSystem->GetGlobalValue("SinglePlayer", pSinglePlayerTable);
		if(pSinglePlayerTable.GetPtr())
		{
			if(GetType() == AIOBJECT_PLAYER)
				pSinglePlayerTable->GetValue("DamageAIToPlayer", pDamageTable);
			else
				pSinglePlayerTable->GetValue("DamageAIToAI", pDamageTable);
		}
		if(!pDamageTable.GetPtr())
			queryDamageValues = false;
	}

	pe_status_pos statusPos;
	pe_params_part paramsPart;
	for (statusPos.ipart = 0, paramsPart.ipart = 0 ; statusPos.ipart < nParts ; ++statusPos.ipart, ++paramsPart.ipart)
	{
		if(!phys->GetParams(&paramsPart) || !phys->GetStatus(&statusPos))
			continue;

		primitives::box box;
		statusPos.pGeomProxy->GetBBox(&box);

		box.center *= statusPos.scale;
		box.size *= statusPos.scale;

		parts[statusPos.ipart].pos = statusPos.pos + statusPos.q * box.center;
		parts[statusPos.ipart].volume = (box.size.x * 2) * (box.size.y * 2) * (box.size.z * 2);

		if(queryDamageValues)
		{
			const	char*	matName = 0;
			phys_geometry* pGeom = paramsPart.pPhysGeomProxy ? paramsPart.pPhysGeomProxy : paramsPart.pPhysGeom;
			if (pGeom->surface_idx >= 0 &&  pGeom->surface_idx < paramsPart.nMats)
			{
				matName = pSurfaceMan->GetSurfaceType(pGeom->pMatMapping[pGeom->surface_idx])->GetName();      
				parts[statusPos.ipart].surfaceIdx = pGeom->surface_idx;
			}
			else
			{
				parts[statusPos.ipart].surfaceIdx = -1;
			}
			float	damage = 0.0f;
			// The this is a bit dodgy, but the keys in the damage table exclude the 'mat_' part of the material name.
			if(pDamageTable.GetPtr() && matName && strlen(matName) > 4)
				pDamageTable->GetValue(matName + 4, damage);
			parts[statusPos.ipart].damageMult = damage;
		}
	}
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::NotifySignalReceived( const char* szText, IAISignalExtraData* pData )
{
	ListWaitGoalOps::iterator it = m_listWaitGoalOps.begin();
	while ( it != m_listWaitGoalOps.end() )
	{
		COPWaitSignal* pGoalOp = *it;
		if ( pGoalOp->NotifySignalReceived(this, szText, pData) )
			m_listWaitGoalOps.erase( it++ );
		else
			++it;
	}
}


// nSignalID = 10 allow duplicating signals
// nSignalID = 9 use the signal only to notify wait goal operations
//
//------------------------------------------------------------------------------------------------------------------------
//void CAIObject::SetSignal(int nSignalID, const char * szText, IEntity *pSender, IAISignalExtraData* pData)
void CAIActor::SetSignal(int nSignalID, const char * szText, IEntity *pSender, IAISignalExtraData* pData, uint32 crcCode)
{
#ifdef _DEBUG
	if(strlen(szText)>=AISIGNAL::SIGNAL_STRING_LENGTH-1)
	{
		AIWarning("####>CAIObject::SetSignal SIGNAL STRING IS TOO LONG for <%s> :: %s  sz-> %d",m_sName.c_str(),szText,strlen(szText));
		//		AILogEvent("####>CAIObject::SetSignal <%s> :: %s  sz-> %d",m_sName.c_str(),szText,strlen(szText));
	}
#endif // _DEBUG

	// This deletes the passed pData parameter if it wasn't set to NULL
	struct DeleteBeforeReturning
	{
		IAISignalExtraData** _p;
		DeleteBeforeReturning( IAISignalExtraData** p ) : _p(p) {}
		~DeleteBeforeReturning()
		{ 
			if ( *_p )
				delete (AISignalExtraData*)*_p;
		}
	} autoDelete( &pData );

	// always process signals sent only to notify wait goal operation
	if (!crcCode)
	{
		crcCode = g_crcSignals.m_crcGen->GetCRC32(szText);
	}

	if ( nSignalID != AISIGNAL_NOTIFY_ONLY )
	{
		if( nSignalID != AISIGNAL_ALLOW_DUPLICATES	)
		{
			DynArray<AISIGNAL>::iterator ai;
			for (ai=m_State.vSignals.begin();ai!=m_State.vSignals.end();++ai)
			{
				//	if ((*ai).strText == szText)
				//if (!stricmp((*ai).strText,szText))

#ifdef _DEBUG
				if (	!stricmp((*ai).strText,szText) &&	!(*ai).Compare(crcCode) )
				{
					AIWarning("Hash values are different, but strings are identical! %s - %s ",(*ai).strText, szText);
					crcCode = g_crcSignals.m_crcGen->GetCRC32((*ai).strText);
					return;
				}

				if(stricmp((*ai).strText,szText) && (*ai).Compare(crcCode))
				{
					AIWarning("Please report to alexey@crytek.de! Hash values are identical, but strings are different! %s - %s ",(*ai).strText, szText);
				}
#endif // _DEBUG

				if ((*ai).Compare(crcCode))
					return;

			}


		}
		//		if( !stricmp(szText, "wakeup") )
		//		{
		//			Event( AIEVENT_WAKEUP, NULL ); 
		//		return;
		//		}

		if(!m_bEnabled && (nSignalID!=AISIGNAL_INCLUDE_DISABLED && nSignalID!=AISIGNAL_ALLOW_DUPLICATES))
			return;
	}

	//	if((nSignalID >= 0) && !m_bCanReceiveSignals)
	//		return;

	//AILogEvent(" ENEMY %s processing signal %s.",m_sName.c_str(),szText);

	AISIGNAL signal;
	signal.nSignal = nSignalID;

	//signal.strText = szText;
	strncpy(signal.strText,szText,AISIGNAL::SIGNAL_STRING_LENGTH);
	//	strcpy(signal.strText,szText);
	signal.m_nCrcText = crcCode;
	signal.pSender = pSender;
	signal.pEData = pData;
	pData = NULL; // set to NULL to prevent delete pData

	IAIRecordable::RecorderEventData recorderEventData(szText);
	if(nSignalID != AISIGNAL_RECEIVED_PREV_UPDATE)// received last update 
	{
		RecordEvent(IAIRecordable::E_SIGNALRECIEVED, &recorderEventData);
		GetAISystem()->Record(this, IAIRecordable::E_SIGNALRECIEVED, szText);
		NotifySignalReceived( szText, pData );
	}

	// don't let notify signals enter the queue
	if ( nSignalID == AISIGNAL_NOTIFY_ONLY )
	{
		if ( pData )
			GetAISystem()->FreeSignalExtraData( pData );
	}
	else
	{
		// need to make sure constructor signal is always at the back - to be processed first
		if(!m_State.vSignals.empty())
		{
			AISIGNAL backSignal(m_State.vSignals.back());
			if (!stricmp("Constructor", backSignal.strText) )
			{
				m_State.vSignals.pop_back();
				m_State.vSignals.push_back(signal);
				m_State.vSignals.push_back(backSignal);
			}
			else
				m_State.vSignals.push_back(signal);
		}
		else
			m_State.vSignals.push_back(signal);
	}
	

	//	m_bSleeping = false;
	//	m_bEnabled = true;

	/*
	if (m_State.nSignal == 0) 
	{
	m_State.nSignal = nSignalID;
	m_State.szSignalText = szText;
	}s
	else
	{
	int a=1;
	}
	*/

}

//====================================================================
// IsHostile
//====================================================================
bool CAIActor::IsHostile(const IAIObject* pOther, bool bUsingAIIgnorePlayer) const
{
FUNCTION_PROFILER( gEnv->pSystem,PROFILE_AI );
	if (!pOther)
		return false;

	CAIObject* pOtherAI = (CAIObject*)pOther;
	if (pOtherAI->GetType() == AIOBJECT_ATTRIBUTE && pOtherAI->GetAssociation())
		pOtherAI = (CAIObject*)pOtherAI->GetAssociation();

	unsigned short nType = pOtherAI->GetType();
	if(	nType == AIOBJECT_GRENADE )//|| nType == AIOBJECT_RPG )
			return true;

	if (bUsingAIIgnorePlayer && nType == AIOBJECT_PLAYER && GetAISystem()->m_cvIgnorePlayer->GetIVal() > 0)
		return false;

	const CAIActor* pOtherActor = pOtherAI->CastToCAIActor();

// FIXME- handle dummy/memory targets properly
	if (!pOtherActor)
		return false;

	if (bUsingAIIgnorePlayer && (m_Parameters.m_bAiIgnoreFgNode || pOtherActor->GetParameters().m_bAiIgnoreFgNode))
		return false;

	bool res=m_Parameters.m_bSpeciesHostility && pOtherActor->GetParameters().m_bSpeciesHostility 
		&& m_Parameters.m_nSpecies != pOtherActor->GetParameters().m_nSpecies 
						&& pOtherActor->GetParameters().m_nSpecies>=0 && IsSpeciesRelevant(pOtherActor->GetParameters().m_nSpecies);
//		&& GetParameters().m_nSpecies>=0

	if (bUsingAIIgnorePlayer && res)
	{
		float hr=pOtherActor->GetHostilityRangeSqr(m_Parameters.m_nSpecies);

		if (hr)
		{
			return GetPos().GetSquaredDistance(pOtherActor->GetPos()) < hr;

		/*	if (GetPos().GetSquaredDistance(pOtherActor->GetPos()) < hr)
			{
				return true;
			}
			else
			{
				gEnv->pLog->Log("<<<hostility restriction active at: %f  meters>>>",hr);
				return false;
			}*/
		}
	}

	return res;
}


//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::Event(unsigned short eType, SAIEVENT *pEvent)
{
	bool	wasEnabled = m_bEnabled;

	CAIObject::Event(eType, pEvent);

	// Update the group status
	if (wasEnabled != m_bEnabled)
	{
		GetAISystem()->UpdateGroupStatus(GetGroupId());
	}
}

//====================================================================
// 
//====================================================================
bool CAIActor::CanAcquireTarget(IAIObject* pOther) const
{
	if (!pOther)
		return false;
	CAIObject* pOtherAI = (CAIObject*)pOther;
	if (pOtherAI->GetType() == AIOBJECT_ATTRIBUTE && pOtherAI->GetAssociation())
		pOtherAI = (CAIObject*)pOtherAI->GetAssociation();

	CAIActor* pOtherActor = pOtherAI->CastToCAIActor();
	if (!pOtherActor)
		return false;
	if (GetAISystem()->GetCombatClassScale(m_Parameters.m_CombatClass, pOtherActor->GetParameters().m_CombatClass )>0)
		return true;
	return false;
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::SetGroupId(int id)
{
	if (id != GetGroupId())
	{
		GetAISystem()->RemoveFromGroup(GetGroupId(), this);
		CAIObject::SetGroupId(id);
		GetAISystem()->AddToGroup(this);

		CAIObject* pBeacon = (CAIObject*)GetAISystem()->GetBeacon(id);
		if (pBeacon)
			GetAISystem()->UpdateBeacon(id, pBeacon->GetPos(), this);

		m_Parameters.m_nGroup = id;
	}
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::SetParameters(AgentParameters & sParams)
{
	SetGroupId(sParams.m_nGroup);
	m_Parameters = sParams;
}

void CAIActor::UpdateHealthHistory()
{
	if(!GetProxy()) return;
	if(!m_healthHistory)
		m_healthHistory = new SAIValueHistory(100, 0.1f);

	float health = GetProxy()->GetActorHealth() + GetProxy()->GetActorArmor();
	float maxHealth = GetProxy()->GetActorMaxHealth();

	m_healthHistory->Sample(health / maxHealth, GetAISystem()->GetFrameDeltaTime());
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	if(GetProxy())
		GetProxy()->Serialize(ser);
	m_State.Serialize(ser, objectTracker);
	m_Parameters.Serialize(ser);

	CAIObject::Serialize(ser, objectTracker);

	if(ser.IsReading())
	{
		// only alive puppets or leaders should be added to groups
		bool addToGroup = GetProxy()!=NULL ? GetProxy()->GetActorHealth()>0 : CastToCLeader()!=NULL;
		if(addToGroup)	
			GetAISystem()->AddToGroup(this);
		GetAISystem()->AddToSpecies(this, m_Parameters.m_nSpecies);
		m_probableTargets.clear();
		m_priorityTargets.clear();
		m_usingCombatLight = false;
		m_lightLevel = AILL_LIGHT;
	}

	ser.Value("maxPuppetExposure", m_detectionMax.puppetExposure);
	ser.Value("maxPuppetThreat", m_detectionMax.puppetThreat);
	ser.Value("maxVehicleExposure", m_detectionMax.vehicleExposure);
	ser.Value("maxVehicleThreat", m_detectionMax.vehicleThreat);

	ser.Value("snapshotPuppetExposure", m_detectionSnapshot.puppetExposure);
	ser.Value("snapshotPuppetThreat", m_detectionSnapshot.puppetThreat);
	ser.Value("snapshotVehicleExposure", m_detectionSnapshot.vehicleExposure);
	ser.Value("snapshotVehicleThreat", m_detectionSnapshot.vehicleThreat);

	ser.BeginGroup("hostility_restriction");
	string hrName;
	for (int i=0; i<MAX_RESTRICTED_SPECIES; i++)
	{
		hrName.Format("hostilityRange%d", i);
		ser.Value(hrName.c_str(), m_hostilityRestrictions[i]);
		hrName.Format("ignore%d", i);
		ser.Value(hrName.c_str(), m_ignoreSpecies[i]);
	}

	ser.Value("minHostileRange", m_minHostileRange);
	ser.Value("maxHostileRange", m_maxHostileRange);
	ser.Value("degenHostileRange", m_degenHostileRange);
	ser.Value("hostilityIncrease", m_hostilityIncrease);
	ser.Value("hostilityCooldown", m_hostilityCooldown);
	ser.Value("lastHostilityTime", m_lastHostilityTime);

	ser.EndGroup();
}

//
//------------------------------------------------------------------------------------------------------------------------
float	CAIActor::GetSightRange(const CAIObject* pTarget) const
{
	if(pTarget && pTarget->CastToCAIVehicle() && GetParameters().m_PerceptionParams.sightRangeVehicle>0)
		return GetParameters().m_PerceptionParams.sightRangeVehicle;
	return GetParameters().m_PerceptionParams.sightRange;
}


//
//------------------------------------------------------------------------------------------------------------------------
float CAIActor::GetCloakMaxDist( ) const 
{
float cloakMaxD( GetAISystem()->m_cvCloakMaxDist->GetFVal() );
//	cloakMaxD = cloakMaxD + (m_Parameters.m_PerceptionParams.sightRange-cloakMaxD)*(1.f-m_Parameters.m_fCloakScale);
	return cloakMaxD;
}


//
//------------------------------------------------------------------------------------------------------------------------
float CAIActor::GetCloakMinDist( ) const
{
float cloakMinD( GetAISystem()->m_cvCloakMinDist->GetFVal() );
//	cloakMinD = cloakMinD + (m_Parameters.m_PerceptionParams.sightRange-cloakMinD)*(1.f-m_Parameters.m_fCloakScale);
	return cloakMinD;
}

//
//------------------------------------------------------------------------------------------------------------------------
bool CAIActor::IsCloakEffective() const
{
	return !IsUsingCombatLight() && m_Parameters.m_fCloakScale > 0.f && !GetGrabbedEntity();
}


//
//------------------------------------------------------------------------------------------------------------------------
bool CAIActor::IsInvisibleFrom(const Vec3& pos) const 
{
	if (m_Parameters.m_bInvisible)
		return true;
	//is not fully cloaked - can possibly be seen beyond cloakMax distance
	if (!IsCloakEffective())
		return false;
	const float cloakMaxDist = GetCloakMaxDist();
	return Distance::Point_PointSq(GetPos(), pos) > sqr(cloakMaxDist);
}


//
//------------------------------------------------------------------------------------------------------------------------
// Assign an interest manager to this AI.
// NULL is fine; replacing an existing PIM works as if you had first called with NULL
void CAIActor::AssignInterestManager( CPersonalInterestManager * pPIM )
{
	// Check if existing one must be cleared
	if (m_pPersonalInterestManager) 
	{
		m_pPersonalInterestManager->Assign(NULL); // Possibly redundant but best sure
		m_pPersonalInterestManager = NULL;
		// The PIMS are owned by the CentralInterestManager so do _not_ delete!
	}
	
	// Assign new
	m_pPersonalInterestManager = pPIM;
	
	// And set it here if not assigned the NULL PIM  (possibly redundant)
	if (pPIM) pPIM->Assign(this); // Clears in process
}

//
//------------------------------------------------------------------------------------------------------------------------
static void CheckAndAddPhysEntity(std::vector<IPhysicalEntity*>& skips, IPhysicalEntity* pPhys)
{
	if (!pPhys) return;
	pe_status_pos	stat;
	if ( pPhys->GetStatus(&stat)!=0 && (((1 << stat.iSimClass) & COVER_OBJECT_TYPES) != 0) )
		skips.push_back(pPhys);
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::GetPhysicsEntitiesToSkip(std::vector<IPhysicalEntity*>& skips) const
{
	FUNCTION_PROFILER(GetAISystem()->m_pSystem, PROFILE_AI);

	if (!GetProxy())
		return;

	CheckAndAddPhysEntity(skips, GetProxy()->GetPhysics(false));
	CheckAndAddPhysEntity(skips, GetProxy()->GetPhysics(true));

	// if holding something in hands - skip it for the vis check
	IEntity *pGrabbedEntity(GetGrabbedEntity());
	if(pGrabbedEntity)
		CheckAndAddPhysEntity(skips, pGrabbedEntity->GetPhysics());
	
	SAIBodyInfo bi;
	GetProxy()->QueryBodyInfo(bi);
	if (bi.linkedVehicleEntity)
	{
		CheckAndAddPhysEntity(skips, bi.linkedVehicleEntity->GetPhysics());
		for (int i = 0, ni = bi.linkedVehicleEntity->GetChildCount(); i < ni; ++i)
		{
			IEntity* pChild = bi.linkedVehicleEntity->GetChild(i);
			if (pChild)
				CheckAndAddPhysEntity(skips, pChild->GetPhysics());
		}
	}

	if (IEntity* pEnt = GetEntity())
	{
		for (int i = 0, ni = pEnt->GetChildCount(); i < ni; ++i)
		{
			IEntity* pChild = pEnt->GetChild(i);
			if (pChild)
				CheckAndAddPhysEntity(skips, pChild->GetPhysics());
		}
	}

}

//
//------------------------------------------------------------------------------------------------------------------------
float CAIActor::GetHostilityRangeSqr(int species) const
{
	if (species < 0 || species >= MAX_RESTRICTED_SPECIES)
		return 0;
	else
		return m_hostilityRestrictions[species];
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::IncreaseHostilityRangeSqr(int species, float rangesqr)
{
	if (species >= 0 && species < MAX_RESTRICTED_SPECIES)
	{
		if (m_hostilityRestrictions[species] > 0)
		{
			m_hostilityRestrictions[species]=max(m_hostilityRestrictions[species], (min(m_maxHostileRange, rangesqr+m_hostilityIncrease)));
			m_lastHostilityTime=GetAISystem()->GetFrameStartTime();
			// gEnv->pLog->Log("<<<hostility restriction for species %d set to %f>>>", species, sqrt(m_hostilityRestrictions[species]));
		}
	}
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::SetupHostilityRange(int species, float min_range, float max_range, float degen, float increase, float cooldown)
{
	if (species >= 0 && species < MAX_RESTRICTED_SPECIES)
	{
		m_minHostileRange=min_range*min_range;
		m_maxHostileRange=max_range*max_range;
		m_degenHostileRange=degen;
		m_hostilityIncrease=increase*increase;
		m_hostilityCooldown.SetSeconds(cooldown);

		m_hostilityRestrictions[species]=m_minHostileRange;
	}
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::UpdateHostilityRange()
{
	for (int i=0; i<MAX_RESTRICTED_SPECIES; ++i)
	{
		if (m_hostilityRestrictions[i] > m_minHostileRange && GetAISystem()->GetFrameStartTime()-m_lastHostilityTime > m_hostilityCooldown)
		{
			float rest=sqrt(m_hostilityRestrictions[i]);
			rest-=m_degenHostileRange*GetAISystem()->GetFrameDeltaTime();
			m_hostilityRestrictions[i] = max(m_minHostileRange, rest*rest);
			// gEnv->pLog->Log("<<<hostility restriction degen: %f>>>", sqrt(m_hostilityRestrictions[i]));
		}
	}
}
//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::IgnoreSpecies(int species, bool ignore)
{
	if (species >= 0 && species < MAX_RESTRICTED_SPECIES)
		m_ignoreSpecies[species]=ignore;
}
//
//------------------------------------------------------------------------------------------------------------------------
bool CAIActor::IsSpeciesRelevant(int species) const
{
	if (species < 0 || species >= MAX_RESTRICTED_SPECIES)
		return true;
	else if (!m_ignoreSpecies[species])
		return true;
	else
		return false;

//	return !m_ignoreSpecies[species];
}