/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 6:9:2004   12:46 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "GameObjects/GameObject.h"
#include "CryAction.h"
#include "Network/GameContext.h" 
#include "Network/GameClientChannel.h"
#include "Network/GameServerChannel.h"
#include "Network/GameClientNub.h"
#include "Network/GameServerNub.h"
#include "Serialization/SerializeScriptTableWriter.h"
#include "Serialization/SerializeScriptTableReader.h"
#include "GameObjectSystem.h"
#include "ITextModeConsole.h"

// ugly: for GetMovementController()
#include "IActorSystem.h"
#include "IVehicleSystem.h"

#define GET_FLAG_FOR_SLOT(flag, slot) (((flag) & (1<<(slot)))!=0)
#define SET_FLAG_FOR_SLOT(flag, slot, value) if (value) { flag |= (1<<(slot)); } else { flag &= ~(1<<(slot)); }

static const size_t MAX_REMOVING_EXTENSIONS = 16;
static const size_t MAX_ADDING_EXTENSIONS = 16;
static const size_t NOT_IN_UPDATE_MODE = size_t(-1);
static EntityId updatingEntity = 0;
static size_t numRemovingExtensions = NOT_IN_UPDATE_MODE;
static IGameObjectSystem::ExtensionID removingExtensions[MAX_REMOVING_EXTENSIONS];
CGameObject::SExtension CGameObject::m_addingExtensions[CGameObject::MAX_ADDING_EXTENSIONS];
int CGameObject::m_nAddingExtension = 0;
CGameObjectSystem * CGameObject::m_pGOS = 0;

static const float UPDATE_TIMEOUT_HUGE = 1e20f;
static const float DISTANCE_CHECKER_TIMEOUT = 1.3f;
static const float FAR_AWAY_DISTANCE = 150.0f;

// MUST BE SYNCHRONIZED WITH EUpdateStates
const float CGameObject::UpdateTimeouts[eUS_COUNT_STATES] =
{
	3.0f, // eUS_Visible_Close
	5.0f, // eUS_Visible_FarAway
	UPDATE_TIMEOUT_HUGE, // eUS_NotVisible_Close
	UPDATE_TIMEOUT_HUGE, // eUS_NotVisible_FarAway
	27.0f, // eUS_CheckVisibility_Close
	13.0f, // eUS_CheckVisibility_FarAway
};
const char * CGameObject::UpdateNames[eUS_COUNT_STATES] =
{
	"Visible_Close", // eUS_Visible_Close
	"Visible_FarAway", // eUS_Visible_FarAway
	"NotVisible_Close", // eUS_NotVisible_Close
	"NotVisible_FarAway", // eUS_NotVisible_FarAway
	"CheckVisibility_Close", // eUS_CheckVisibility_Close
	"CheckVisibility_FarAway", // eUS_CheckVisibility_FarAway
};
const char * CGameObject::EventNames[eUSE_COUNT_EVENTS] =
{
	"BecomeVisible",
	"BecomeClose",
	"BecomeFarAway",
	"Timeout",
};
const CGameObject::EUpdateState CGameObject::UpdateTransitions[eUS_COUNT_STATES][eUSE_COUNT_EVENTS] =
{
	/* 
  //eUS_CheckVisibility_FarAway:// {eUS_CheckVisibility_FarAway, eUS_CheckVisibility_FarAway, eUS_CheckVisibility_FarAway, eUS_CheckVisibility_FarAway}
	                                 {eUSE_BecomeVisible         , eUSE_BecomeClose           , eUSE_BecomeFarAway         , eUSE_Timeout               }
  */
	/*eUS_Visible_Close:*/           {eUS_INVALID                , eUS_Visible_Close          , eUS_Visible_FarAway        , eUS_CheckVisibility_Close  },
	/*eUS_Visible_FarAway:*/         {eUS_INVALID                , eUS_Visible_Close          , eUS_Visible_FarAway        , eUS_CheckVisibility_FarAway},
	/*eUS_NotVisible_Close:*/        {eUS_Visible_Close          , eUS_NotVisible_Close       , eUS_NotVisible_FarAway     , eUS_NotVisible_Close       },
	/*eUS_NotVisible_FarAway:*/      {eUS_Visible_FarAway        , eUS_NotVisible_Close       , eUS_NotVisible_FarAway     , eUS_NotVisible_FarAway     },
	/*eUS_CheckVisibility_Close:*/   {eUS_Visible_Close          , eUS_CheckVisibility_Close  , eUS_CheckVisibility_FarAway, eUS_NotVisible_Close       },
	/*eUS_CheckVisibility_FarAway:*/ {eUS_Visible_FarAway        , eUS_CheckVisibility_Close  , eUS_CheckVisibility_FarAway, eUS_NotVisible_FarAway     },
};

static int g_TextModeY = 0;
static float g_y;
static CTimeValue g_lastUpdate;
static int g_showUpdateState = 0;

static AABB CreateDistanceAABB( Vec3 center, float radius = FAR_AWAY_DISTANCE )
{
	static const float CHECK_GRANULARITY = 2.0f;
	Vec3 mn, mx;
	for (int i=0; i<3; i++)
	{
		center[i] = cry_floorf( center[i] / CHECK_GRANULARITY ) * CHECK_GRANULARITY;
		mn[i] = center[i] - radius * 0.5f;
		mx[i] = center[i] + radius * 0.5f;
	}
	return AABB(mn, mx);
}

static ICVar* g_pForceFastUpdateCVar = NULL;
static ICVar* g_pVisibilityTimeoutCVar = NULL;
static ICVar* g_pVisibilityTimeoutTimeCVar = NULL;

static std::set<CGameObject*> g_updateSchedulingProfile;

void CGameObject::CreateCVars()
{
	g_pForceFastUpdateCVar = gEnv->pConsole->RegisterInt("g_goForceFastUpdate", 0, VF_CHEAT, "GameObjects IsProbablyVisible->TRUE && IsProbablyDistant()->FALSE");
	g_pVisibilityTimeoutCVar = gEnv->pConsole->RegisterInt("g_VisibilityTimeout", 0, VF_CHEAT, "Adds visibility timeout to IsProbablyVisible() calculations");
	g_pVisibilityTimeoutTimeCVar = gEnv->pConsole->RegisterFloat("g_VisibilityTimeoutTime", 30, VF_CHEAT, "Visibility timeout time used by IsProbablyVisible() calculations");
	gEnv->pConsole->Register("g_showUpdateState", &g_showUpdateState, 0, VF_CHEAT, "Show the game object update state of any activated entities; 3: actor objects only");
}

//------------------------------------------------------------------------
CGameObject::CGameObject() : 
	m_pActionDelegate(0),
	m_pViewDelegate(0),
	m_pProfileManager(0),
	m_channelId(0), 
	m_enabledAspects(0xffu),
	m_enabledPhysicsEvents(0),
	m_forceUpdate(0),
	m_updateState(eUS_NotVisible_FarAway),
	m_updateTimer(0.1f),
	m_isBoundToNetwork(false),
	m_distanceChecker(0),
	m_inRange(false),
	m_justExchanging(false),
	m_aiMode(eGOAIAM_VisibleOrInRange),
	m_physDisableMode(eADPM_Never),
	m_prePhysicsUpdateRule(ePPU_Never),
	m_bPrePhysicsEnabled(false),
	m_pSchedulingProfiles(NULL),
	m_predictionHandle(0),
	m_bPhysicsDisabled(false),
	m_bNoSyncPhysics(false),
	m_bNeedsNetworkRebind(false)
{
	if (!m_pGOS)
		m_pGOS = (CGameObjectSystem*) CCryAction::GetCryAction()->GetIGameObjectSystem();

	for (int i=0; i<8; i++)
		m_profiles[i] = 255;

	m_bVisible=true;
}

//------------------------------------------------------------------------
CGameObject::~CGameObject()
{
	g_updateSchedulingProfile.erase(this);
}

//------------------------------------------------------------------------
ILINE bool CGameObject::TestIsProbablyVisible( EUpdateState state )
{	
	if (g_pForceFastUpdateCVar && g_pForceFastUpdateCVar->GetIVal())
		return false;

	switch (state)
	{
	case eUS_Visible_Close:
	case eUS_Visible_FarAway:
	case eUS_CheckVisibility_Close:
	case eUS_CheckVisibility_FarAway:
		return true;
	case eUS_NotVisible_FarAway:
	case eUS_NotVisible_Close:
		return false;
	}
	assert(false);
	return false;
}

//------------------------------------------------------------------------
ILINE bool CGameObject::TestIsProbablyDistant( EUpdateState state )
{	
	if (g_pForceFastUpdateCVar && g_pForceFastUpdateCVar->GetIVal())
		return true;

	switch (state)
	{
	case eUS_Visible_FarAway:
	case eUS_CheckVisibility_FarAway:
	case eUS_NotVisible_FarAway:
		return true;
	case eUS_CheckVisibility_Close:
	case eUS_Visible_Close:
	case eUS_NotVisible_Close:
		return false;
	}
	assert(false);
	return false;
}

//------------------------------------------------------------------------
bool CGameObject::IsProbablyVisible()
{
	// this would be a case of driver in a tank - hidden, but needes to be updated
	if(GetEntity()->IsHidden() && (GetEntity()->GetFlags() & ENTITY_FLAG_UPDATE_HIDDEN))
		return true;

	if(GetEntity()->IsHidden())
		return false;

	if (g_pVisibilityTimeoutCVar && g_pVisibilityTimeoutCVar->GetIVal())
	{ 
		IEntityRenderProxy *pProxy=(IEntityRenderProxy*)GetEntity()->GetProxy( ENTITY_PROXY_RENDER );		
		if (pProxy)
		{
			float fDiff=GetISystem()->GetITimer()->GetCurrTime() - pProxy->GetLastSeenTime();
			if (fDiff>g_pVisibilityTimeoutTimeCVar->GetFVal())
			{
				if (m_bVisible)
				{
					if (g_pVisibilityTimeoutCVar->GetIVal()==2)
						CryLogAlways("Entity %s not visible for %f seconds - visibility timeout",GetEntity()->GetName(),fDiff);
				}
				m_bVisible=false;				
				return (false);
			}
		}
	}

	if (!m_bVisible)
	{
		if (g_pVisibilityTimeoutCVar->GetIVal()==2)
		CryLogAlways("Entity %s is now visible again after visibility timeout",GetEntity()->GetName());
	}
	m_bVisible=true;

	return TestIsProbablyVisible(m_updateState);
}

//------------------------------------------------------------------------
bool CGameObject::IsProbablyDistant()
{
	//it's hidden - far away
	if(GetEntity()->IsHidden() && !(GetEntity()->GetFlags() & ENTITY_FLAG_UPDATE_HIDDEN))
		return true;
	if (g_pVisibilityTimeoutCVar && g_pVisibilityTimeoutCVar->GetIVal())
	{	
		if (!m_bVisible)
			return (true);
	}
	return TestIsProbablyDistant(m_updateState);
}

//------------------------------------------------------------------------
void CGameObject::UpdateStateEvent( EUpdateStateEvent evt )
{
	EUpdateState newState = UpdateTransitions[m_updateState][evt];

	if (newState == eUS_INVALID)
	{
		//GameWarning("%s: visibility activation requested invalid state (%s on event %s)", GetEntity()->GetName(), UpdateNames[m_updateState], EventNames[m_updateState] );
		return;
	}

	if (newState != m_updateState)
	{
    if (TestIsProbablyVisible(newState) && !TestIsProbablyVisible(m_updateState))
    {
      SGameObjectEvent goe(eGFE_OnBecomeVisible, eGOEF_ToExtensions);
      SendEvent(goe);
    }

		m_updateState = newState;
    m_updateTimer = UpdateTimeouts[m_updateState];

		uint32 flags = m_pEntity->GetFlags();
		if (UpdateTransitions[m_updateState][eUSE_BecomeVisible] != eUS_INVALID)
			flags |= ENTITY_FLAG_SEND_RENDER_EVENT;
		else
			flags &= ~ENTITY_FLAG_SEND_RENDER_EVENT;
		m_pEntity->SetFlags(flags);

		EvaluateUpdateActivation();
	}
}

//------------------------------------------------------------------------
ILINE bool CGameObject::ShouldUpdateSlot( const SExtension * pExt, int slot, bool checkAIDisable )
{	
	bool debug = false;
// 	if(gEnv->pConsole->GetCVar("es_logDrawnActors")->GetIVal() != 0)
// 	{
// 		if(CCryAction::GetCryAction()->GetIActorSystem()->GetActor(GetEntityId()))
// 			debug = true;
// 	}

	if(debug)
	{
		CryLogAlways("ShouldUpdateSlot: Beginning check for %s: ext %s, slot %d", GetEntity()->GetName(), m_pGOS->GetName(pExt->id), slot);
	}

	if (checkAIDisable)
		if (GET_FLAG_FOR_SLOT(pExt->flagDisableWithAI, slot))
		{
			if(debug)
				CryLogAlways("---ShouldUpdateSlot: checkAIDisable: returning false");
			return false;	 
		}

	if (pExt->forceEnables[slot])
	{
		if(debug)
			CryLogAlways("---ShouldUpdateSlot: forceEnable: returning true");
		return true;
	}

	if (GET_FLAG_FOR_SLOT(pExt->flagNeverUpdate, slot))
	{
		if(debug)
			CryLogAlways("---ShouldUpdateSlot: flagNeverUpdate: returning false");
		return false;
	}

	if (!pExt->updateEnables[slot])
	{
		if(debug)
			CryLogAlways("---ShouldUpdateSlot: updateEnables[]: returning false");
		return false;
	}

	bool visibleCheck = true;
	if (GET_FLAG_FOR_SLOT(pExt->flagUpdateWhenVisible, slot))
		visibleCheck = TestIsProbablyVisible(m_updateState);

	bool inRangeCheck = true;
	if (GET_FLAG_FOR_SLOT(pExt->flagUpdateWhenInRange, slot))
		inRangeCheck = !TestIsProbablyDistant(m_updateState);

	if (GET_FLAG_FOR_SLOT(pExt->flagUpdateCombineOr, slot))
	{
		if(debug)
			CryLogAlways("---ShouldUpdateSlot: flagUpdateCombineOr: %s || %s: returning %s", visibleCheck ? "visible" : "invisible", inRangeCheck ? "inRange" : "not in range", visibleCheck || inRangeCheck ? "true" : "false");
		return visibleCheck || inRangeCheck;
	}
	else
	{
		if(debug)
			CryLogAlways("---ShouldUpdateSlot: flagUpdateCombineAnd: %s && %s: returning %s", visibleCheck ? "visible" : "invisible", inRangeCheck ? "inRange" : "not in range", visibleCheck && inRangeCheck ? "true" : "false");
		return visibleCheck && inRangeCheck;
	}
}

//------------------------------------------------------------------------
void CGameObject::Init(IEntity *pEntity, SEntitySpawnParams &spawnParams)
{
	m_pEntity = pEntity;
	m_entityId = pEntity->GetId();

	m_pSchedulingProfiles = ((CGameObjectSystem*)CCryAction::GetCryAction()->GetIGameObjectSystem())->GetEntitySchedulerProfiles(m_pEntity);

	IGameObjectSystem * pGOS = m_pGOS;

	if (!m_extensions.empty())
	{
		TExtensions preExtensions;
		m_extensions.swap(preExtensions);
		// this loop repopulates m_extensions
		for (TExtensions::const_iterator iter = preExtensions.begin(); iter != preExtensions.end(); ++iter)
			ChangeExtension( pGOS->GetName(iter->id), eCE_Activate );

		// set sticky bits, clear activated bits (extensions activated before initialization are sticky)
		for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end(); ++iter)
		{
			iter->sticky = iter->activated;
			iter->activated = false;
		}
	}

	GetEntity()->SetFlags( GetEntity()->GetFlags() | ENTITY_FLAG_SEND_RENDER_EVENT );

	// safety set... make sure the spawn serializer doesnt get used twice in the case that two entities are accidently
	// spawned by DefaultSpawn
	CCryAction::GetCryAction()->GetIGameObjectSystem()->SetSpawnSerializer(NULL);
}

bool CGameObject::BindToNetwork(EBindToNetworkMode mode)
{
	CGameContext * pGameContext = CCryAction::GetCryAction()->GetGameContext();
	if (!pGameContext)
		return false;
	INetContext * pNetContext = pGameContext->GetNetContext();
	if (!pNetContext)
		return false;

	if (m_isBoundToNetwork)
	{
		switch (mode)
		{	
		case eBTNM_NowInitialized:
			if (!m_isBoundToNetwork)
				return false;
			// fall through
		case eBTNM_Force:
			m_isBoundToNetwork = false;
			break;
		case eBTNM_Normal:
			return true;
		}
	}
	else if (mode == eBTNM_NowInitialized)
		return false;

	if (GetEntity()->GetFlags() & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY))
		return false;

	if (!GetEntity()->IsInitialized())
	{
		m_isBoundToNetwork = true;
		return true;
	}

	static const uint8 gameObjectAspects =
		eEA_GameClientDynamic |
		eEA_GameServerDynamic |
		eEA_GameClientStatic |
		eEA_GameServerStatic;

	uint32 aspects = 0;
	INetChannel * pControllingChannel = NULL;
	IEntityScriptProxy * pScriptProxy = NULL;

	aspects |= gameObjectAspects;

	if (!m_bNoSyncPhysics && (GetEntity()->GetProxy( ENTITY_PROXY_PHYSICS ) || m_pProfileManager))
	{
		aspects |= eEA_Physics;
		//		aspects &= ~eEA_Volatile;
	}
	if (pScriptProxy = (IEntityScriptProxy*) GetEntity()->GetProxy( ENTITY_PROXY_SCRIPT ))
		aspects |= eEA_Script;

	if (m_channelId)
	{
		CGameServerNub * pServerNub = CCryAction::GetCryAction()->GetGameServerNub();
		if (!pServerNub)
		{
			GameWarning("Unable to bind object to network (%s)", GetEntity()->GetName());
			return false;
		}
		CGameServerChannel * pServerChannel = pServerNub->GetChannel( m_channelId );
		if (!pServerChannel)
		{
			GameWarning("Unable to bind object to network (%s)", GetEntity()->GetName());
			return false;
		}
		pServerChannel->SetPlayerId( GetEntity()->GetId() );
		pControllingChannel = pServerChannel->GetNetChannel();
	}

	if (gEnv->bServer)
	{
		bool isStatic = CCryAction::GetCryAction()->IsInLevelLoad();
		if (GetEntity()->GetFlags() & ENTITY_FLAG_NEVER_NETWORK_STATIC)
			isStatic = false;
		pNetContext->BindObject( GetEntityId(), aspects, isStatic );
		if (pControllingChannel)
		{
			//pNetContext->DelegateAuthority( GetEntityId(), pControllingChannel );
		}
	}

	// will this work :)
	if (pControllingChannel)
	{
		pControllingChannel->DeclareWitness( GetEntityId() );
	}

	m_isBoundToNetwork = true;
	g_updateSchedulingProfile.insert(this);
	EvaluateUpdateActivation();

	return true;
}

//------------------------------------------------------------------------
void CGameObject::Done()
{
	FlushExtensions(true);
	m_pGOS->SetPostUpdate(this, false);
	if (m_distanceChecker)
		gEnv->pEntitySystem->RemoveEntity( m_distanceChecker );
	g_updateSchedulingProfile.erase(this);
}

//------------------------------------------------------------------------
void CGameObject::Release()
{
	delete this;
}

//------------------------------------------------------------------------
void CGameObject::DebugUpdateState()
{
	if(g_showUpdateState == 3)
	{
		// only show actors
		if(!CCryAction::GetCryAction()->GetIActorSystem()->GetActor(GetEntityId()))
			return;
	}

	ITextModeConsole* pTMC = GetISystem()->GetITextModeConsole();

	float white[] = {1,1,1,1};

	char buf[512];
	char * pOut = buf;

	pOut += sprintf(pOut, "%s: %s", GetEntity()->GetName(), UpdateNames[m_updateState]);
	if (m_updateTimer<(0.5f*UPDATE_TIMEOUT_HUGE))
		pOut += sprintf(pOut, " timeout:%.2f", m_updateTimer);
	if (ShouldUpdateAI())
		pOut += sprintf(pOut, m_aiMode==eGOAIAM_Always ? "AI_ALWAYS" : " AIACTIVE");
	if (m_bPrePhysicsEnabled)
		pOut += sprintf(pOut, " PREPHYSICS");
	if (m_prePhysicsUpdateRule != ePPU_Never)
		pOut += sprintf(pOut, " pprule=%s", m_prePhysicsUpdateRule==ePPU_Always? "always" : "whenai");
	if (m_forceUpdate)
		pOut += sprintf(pOut, " force:%d", m_forceUpdate);
	if (m_physDisableMode != eADPM_Never)
		pOut += sprintf(pOut, " physDisable:%d", m_physDisableMode);
	gEnv->pRenderer->Draw2dLabel( 10, g_y+=10, 1, white, false, "%s", buf );
	if (pTMC)
		pTMC->PutText(0, g_TextModeY++, buf);

	if (g_showUpdateState >= 2)
	{
		bool checkAIDisable = !ShouldUpdateAI() && GetEntity()->GetAI();
		for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end(); ++iter)
		{
			for (int slot = 0; slot < MAX_UPDATE_SLOTS_PER_EXTENSION; slot++)
			{
				pOut = buf;
				pOut += sprintf(pOut, "%s[%d]: en:%d", m_pGOS->GetName(iter->id), slot, iter->updateEnables[slot]);

				if (iter->forceEnables[slot])
					pOut += sprintf(pOut, " force:%d", int(iter->forceEnables[slot]));

				bool done = false;
				if (checkAIDisable)
					if (done = GET_FLAG_FOR_SLOT(iter->flagDisableWithAI, slot))
						pOut += sprintf(pOut, " ai-disable");

				if (!done && (done = GET_FLAG_FOR_SLOT(iter->flagNeverUpdate, slot)))
					pOut += sprintf(pOut, " never");

				if (!done)
				{
					bool visibleCheck = true;
					if (GET_FLAG_FOR_SLOT(iter->flagUpdateWhenVisible, slot))
						visibleCheck = TestIsProbablyVisible(m_updateState);

					bool inRangeCheck = true;
					if (GET_FLAG_FOR_SLOT(iter->flagUpdateWhenInRange, slot))
						inRangeCheck = !TestIsProbablyDistant(m_updateState);

					pOut += sprintf(pOut, " %s", visibleCheck? "visible" : "!visible");
					if (GET_FLAG_FOR_SLOT(iter->flagUpdateCombineOr, slot))
						pOut += sprintf(pOut, " ||");
					else
						pOut += sprintf(pOut, " &&");
					pOut += sprintf(pOut, " %s", inRangeCheck? "in-range" : "!in-range");
				}
				if (ShouldUpdateSlot(&*iter, slot, checkAIDisable))
				{
					gEnv->pRenderer->Draw2dLabel( 20, g_y+=10, 1, white, false, "%s", buf );
					if (pTMC)
						pTMC->PutText(1, g_TextModeY++, buf);
				}
				else
				{
					// output info in red if not updating
					float red[] = {0.8f,0,0,1};
					gEnv->pRenderer->Draw2dLabel( 20, g_y+=10, 1, red, false, "%s", buf);
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameObject::Update(SEntityUpdateContext &ctx)
{
	extern f32 g_YLine2;
	g_YLine2=200;

	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	if (gEnv->pTimer->GetFrameStartTime() != g_lastUpdate)
	{
		g_lastUpdate = gEnv->pTimer->GetFrameStartTime();
		g_y = 10;
		g_TextModeY = 0;
	}

	assert(numRemovingExtensions == NOT_IN_UPDATE_MODE);
	assert(updatingEntity == 0);
	numRemovingExtensions = 0;
	m_nAddingExtension = 0;
	updatingEntity = GetEntityId();

	UpdateDistanceChecker( ctx.fFrameTime );

	m_updateTimer -= ctx.fFrameTime;
	if (m_updateTimer < 0.0f)
	{
		// be pretty sure that we won't trigger twice...
		m_updateTimer = UPDATE_TIMEOUT_HUGE;
		UpdateStateEvent( eUSE_Timeout );
	}

	if (g_showUpdateState)
	{
		DebugUpdateState();
	}

	/*
	 * UPDATE EXTENSIONS
	 */
	IGameObjectSystem * pGameObjectSystem = m_pGOS;
	bool shouldUpdateAI = ShouldUpdateAI();
	bool keepUpdating = shouldUpdateAI;
	bool checkAIDisableOnSlots = !shouldUpdateAI && GetEntity()->GetAI();
	for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end(); ++iter)
	{
#ifndef NDEBUG
		const char* name = pGameObjectSystem->GetName(iter->id);
#endif
		for (int i=0; i<MAX_UPDATE_SLOTS_PER_EXTENSION; i++)
		{
			iter->forceEnables[i] -= iter->forceEnables[i] != 0;
			if (ShouldUpdateSlot(&*iter, i, checkAIDisableOnSlots))
			{
				iter->pExtension->Update(ctx, i);
				keepUpdating = true;
			}
		}
	}

	if (!keepUpdating && m_forceUpdate<=0)
		SetActivation(false);

	/*
	 * REMOVE EXTENSIONS
	 */
	while (numRemovingExtensions)
	{
		for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end(); ++iter)
		{
			if (iter->id == removingExtensions[numRemovingExtensions-1])
			{
				RemoveExtension(iter);
				break;
			}
		}

		numRemovingExtensions --;
	}
	/*
	 * ADD EXTENSIONS
	 */
	for (int i=0; i<m_nAddingExtension; i++)
		m_extensions.push_back(m_addingExtensions[i]);
	std::sort(m_extensions.begin(), m_extensions.end());
	for (int i=0; i<m_nAddingExtension; i++)
	{
		m_addingExtensions[i].pExtension->PostInit(this);
		m_addingExtensions[i] = SExtension();
	}
	m_nAddingExtension = 0;

	assert(numRemovingExtensions != NOT_IN_UPDATE_MODE);
	assert(updatingEntity == GetEntityId());
	numRemovingExtensions = NOT_IN_UPDATE_MODE;
	updatingEntity = 0;
}

void CGameObject::UpdateSchedulingProfile()
{
	bool remove = false;
	if (m_isBoundToNetwork && m_pSchedulingProfiles)
	{
		// We need to check NetContext here, because it's NULL in a dummy editor game session (or at least while starting up the editor).
		INetContext * pNetContext = CCryAction::GetCryAction()->GetGameContext()->GetNetContext();
		if (pNetContext && pNetContext->SetSchedulingParams( GetEntityId(), m_pSchedulingProfiles->normal, m_pSchedulingProfiles->owned ))
			remove = true;
	}
	else
	{
		remove = true;
	}
	if (remove)
	{
		g_updateSchedulingProfile.erase(this);
	}
}

void CGameObject::UpdateSchedulingProfiles()
{
	for (std::set<CGameObject*>::iterator it = g_updateSchedulingProfile.begin(); it != g_updateSchedulingProfile.end(); )
	{
		std::set<CGameObject*>::iterator next = it;
		++next;
		(*it)->UpdateSchedulingProfile();
		it = next;
	}
}

void CGameObject::ForceUpdateExtension( IGameObjectExtension * pExt, int slot )
{
	SExtension * pInfo = GetExtensionInfo(pExt);
	if (pInfo)
	{
		pInfo->forceEnables[slot] = 255;
		SetActivation(true);
	}
}

//------------------------------------------------------------------------
void CGameObject::ProcessEvent(SEntityEvent &event)
{
	if ( m_pEntity )
	{
		switch (event.event)
		{
		case ENTITY_EVENT_RENDER:
			UpdateStateEvent( eUSE_BecomeVisible );
			break;

		case ENTITY_EVENT_INIT:
			if (m_bNeedsNetworkRebind)
			{
				m_bNeedsNetworkRebind = false;
				BindToNetwork(eBTNM_Force);
			}
			break;
		case ENTITY_EVENT_ENTERAREA:
			if (m_distanceChecker && event.nParam[2] == m_distanceChecker)
			{
				if (IEntity * pEntity = gEnv->pEntitySystem->GetEntity(event.nParam[0]))
				{
					if (CGameObject * pObj = (CGameObject*) pEntity->GetProxy(ENTITY_PROXY_USER))
					{
						if (!pObj->m_inRange)
						{
							pObj->m_inRange = true;
							pObj->UpdateStateEvent( eUSE_BecomeClose );
						}
					}
				}
			}
			break;

		case ENTITY_EVENT_LEAVEAREA:
			if (m_distanceChecker && event.nParam[2] == m_distanceChecker)
			{
				if (IEntity * pEntity = gEnv->pEntitySystem->GetEntity(event.nParam[0]))
				{
					if (CGameObject * pObj = (CGameObject*) pEntity->GetProxy(ENTITY_PROXY_USER))
					{
						if (pObj->m_inRange)
						{
							pObj->m_inRange = false;
							if (pObj->m_inRange == 0)
								pObj->UpdateStateEvent( eUSE_BecomeFarAway );
						}
					}
				}
			}
			break;

		case ENTITY_EVENT_POST_SERIALIZE:
			PostSerialize();
			break;

		case ENTITY_EVENT_DONE:
			if (m_isBoundToNetwork)
			{
				// check if we're still bound
				CGameContext * pGameContext = CCryAction::GetCryAction()->GetGameContext();
				if (pGameContext)
				{
					INetContext * pNetContext = pGameContext->GetNetContext();
					if (pNetContext)
					{
						m_isBoundToNetwork = pNetContext->IsBound(GetEntityId());
						m_bNeedsNetworkRebind = !m_isBoundToNetwork;
					}
				}
			}
			break;
		case ENTITY_EVENT_HIDE:
		case ENTITY_EVENT_UNHIDE:
			EvaluateUpdateActivation();
		}


		for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end(); ++iter)
		{
			iter->pExtension->ProcessEvent(event);
		}
	}
}

//------------------------------------------------------------------------
IGameObjectExtension * CGameObject::GetExtensionWithRMIBase( const void * pBase )
{
	for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end(); ++iter)
	{
		if (iter->pExtension->GetRMIBase() == pBase)
			return iter->pExtension;
	}
	return NULL;
}

//------------------------------------------------------------------------
void CGameObject::SerializeXML(XmlNodeRef &entityNode, bool loading)
{
}

//------------------------------------------------------------------------
bool CGameObject::CaptureActions( IActionListener * pAL )
{
	if (m_pActionDelegate || !pAL)
		return false;
	m_pActionDelegate = pAL;
	return true;
}

bool CGameObject::CaptureView( IGameObjectView * pGOV )
{
	if (m_pViewDelegate || !pGOV)
		return false;
	m_pViewDelegate = pGOV;
	return true;
}

bool CGameObject::CaptureProfileManager( IGameObjectProfileManager * pPM )
{
	if (m_pProfileManager || !pPM)
		return false;
	m_pProfileManager = pPM;
	return true;
}

void CGameObject::ReleaseActions( IActionListener * pAL )
{
	if (m_pActionDelegate != pAL)
		return;
	m_pActionDelegate = 0;
}

void CGameObject::ReleaseView( IGameObjectView * pGOV )
{
	if (m_pViewDelegate != pGOV)
		return;
	m_pViewDelegate = 0;
}

void CGameObject::ReleaseProfileManager( IGameObjectProfileManager * pPM )
{
	if (m_pProfileManager != pPM)
		return;
	m_pProfileManager = 0;
}

//------------------------------------------------------------------------
void CGameObject::UpdateView(SViewParams &viewParams)
{
	if (m_pViewDelegate)
		m_pViewDelegate->UpdateView( viewParams );
}

//------------------------------------------------------------------------
void CGameObject::PostUpdateView(SViewParams &viewParams)
{
	if (m_pViewDelegate)
		m_pViewDelegate->PostUpdateView( viewParams );
}

//------------------------------------------------------------------------
void CGameObject::OnAction( const ActionId& actionId, int activationMode, float value )
{
	if (m_pActionDelegate)
		m_pActionDelegate->OnAction( actionId, activationMode, value );
	else
		GameWarning("Action sent to an entity that doesn't implement action-listener; entity class is %s, and action is %s",
			GetEntity()->GetClass()->GetName(), actionId.c_str());
}

void CGameObject::AfterAction()
{
	if (m_pActionDelegate)
		m_pActionDelegate->AfterAction();
}

//------------------------------------------------------------------------
IEntity *CGameObject::GetEntity() const
{
	return m_pEntity;
}

//////////////////////////////////////////////////////////////////////////
bool CGameObject::NeedSerialize()
{
	return true;
}

static const char * AspectProfileName(int i)
{
	static char buffer[9] = {0};
	if (!buffer[0])
	{
		buffer[0] = 'a';
		buffer[1] = 'p';
		buffer[2] = 'r';
		buffer[3] = 'o';
		buffer[4] = 'f';
		buffer[5] = 'i';
		buffer[6] = 'l';
		buffer[7] = 'e';
	}
	buffer[8] = '0' + i;
	return buffer;
}

//------------------------------------------------------------------------
void CGameObject::Serialize( TSerialize ser )
{
	FullSerialize( ser );
}

void CGameObject::FullSerialize( TSerialize ser )
{
	assert(ser.GetSerializationTarget() != eST_Network);

	if (ser.BeginOptionalGroup("GameObject",true))
	{
		IGameObjectSystem * pGameObjectSystem = m_pGOS;
		std::vector<string> activatedExtensions;
		int count = 0;
		string name;
		if (ser.IsWriting())
		{
			for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end(); ++iter)
			{
				if (iter->activated)
					activatedExtensions.push_back( pGameObjectSystem->GetName(iter->id) );
			}
			if (!activatedExtensions.empty())
			{
				ser.Value("activatedExtensions", activatedExtensions);
			}

			count = m_extensions.size();
			ser.Value("numExtensions", count);
			for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end(); ++iter)
			{
				ser.BeginGroup("Extension");
				name = pGameObjectSystem->GetName(iter->id);
				ser.Value("name", name);
				iter->pExtension->FullSerialize(ser);
				ser.EndGroup();
			}

			for (int i=0; i<8; i++)
			{
				int aspect = 1<<i;

				uint8 profile = 255;
				if (m_pProfileManager)
				{
					if (CGameContext * pGC = CCryAction::GetCryAction()->GetCryAction()->GetGameContext())
						if (INetContext * pNC = pGC->GetNetContext())
							profile = pNC->GetAspectProfile( GetEntityId(), aspect );
				}
				ser.Value(AspectProfileName(i), profile);
			}
		}
		else //reading
		{
			//			FlushExtensions(false);
			ser.Value("activatedExtensions", activatedExtensions);
			for (std::vector<string>::iterator iter = activatedExtensions.begin(); iter != activatedExtensions.end(); ++iter)
				ChangeExtension(iter->c_str(), eCE_Activate);

			ser.Value("numExtensions", count);
			while (count--)
			{
				ser.BeginGroup("Extension");
				ser.Value("name", name);
				IGameObjectExtension * pExt = AcquireExtension(name.c_str());
				if (pExt)
				{
					pExt->FullSerialize(ser);
					ReleaseExtension(name.c_str());
				}
				ser.EndGroup();
			}

			//physicalize after serialization with the correct parameters
			if (m_pProfileManager)
			{
				for (int i=0; i<8; i++)
				{
					int aspect = 1<<i;

					uint8 profile = 0;
					ser.Value(AspectProfileName(i), profile);
					SetAspectProfile((EEntityAspects)aspect, profile, false );
				}
			}

		}

		ser.EndGroup(); //GameObject
	}
}

//------------------------------------------------------------------------
bool CGameObject::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	for (TExtensions::iterator it = m_extensions.begin(); it != m_extensions.end(); ++it)
		if (!it->pExtension->NetSerialize( ser, aspect, profile, flags ))
			return false;

	if (aspect == eEA_Physics && !m_pProfileManager)
	{
		if (IEntityPhysicalProxy * pProxy = (IEntityPhysicalProxy*) GetEntity()->GetProxy( ENTITY_PROXY_PHYSICS ))
			pProxy->Serialize( ser );
		else
			return false;
	}

	return true;
}

//------------------------------------------------------------------------
void CGameObject::PostSerialize()
{
	for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end(); ++iter)
	{
		if(iter->pExtension)
			iter->pExtension->PostSerialize();
	}
	m_distanceChecker = 0;
}

//------------------------------------------------------------------------
void CGameObject::SetAuthority( bool auth )
{
	g_updateSchedulingProfile.insert(this);
	for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end(); ++iter)
		iter->pExtension->SetAuthority( auth );
}

//------------------------------------------------------------------------
void CGameObject::InitClient( int channelId )
{
	for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end(); ++iter)
		iter->pExtension->InitClient( channelId );
}


//------------------------------------------------------------------------
void CGameObject::PostInitClient( int channelId )
{
	for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end(); ++iter)
		iter->pExtension->PostInitClient( channelId );
}

//------------------------------------------------------------------------
void CGameObject::ChangedNetworkState( uint8 aspects )
{
	CGameContext * pGameContext = CCryAction::GetCryAction()->GetGameContext();
	if (!pGameContext)
		return;
	INetContext * pNetContext = pGameContext->GetNetContext();
	if (!pNetContext)
		return;
	IEntity * pEntity = GetEntity();
	if (!pEntity)
		return;
	pNetContext->ChangedAspects( pEntity->GetId(), aspects, NULL );
}

//------------------------------------------------------------------------
void CGameObject::EnableAspect(uint8 aspects, bool enable)
{
	CGameContext * pGameContext = CCryAction::GetCryAction()->GetGameContext();
	if (!pGameContext)
		return;

	if (enable)
		m_enabledAspects |= aspects;
	else
		m_enabledAspects &= ~aspects;

	pGameContext->EnableAspects(GetEntityId(), aspects, enable);
}

//------------------------------------------------------------------------
template <class T>
ILINE bool CGameObject::DoGetSetExtensionParams( const char * extension, SmartScriptTable params )
{
	IGameObjectSystem * pGameObjectSystem = m_pGOS;
	SExtension ext;
	ext.id = pGameObjectSystem->GetID(extension);
	if (ext.id != IGameObjectSystem::InvalidExtensionID)
	{
		TExtensions::iterator iter = std::lower_bound( m_extensions.begin(), m_extensions.end(), ext );

		if (iter != m_extensions.end() && ext.id == iter->id)
		{
			T impl(params);
			CSimpleSerialize<T> ser(impl);
			iter->pExtension->FullSerialize(TSerialize(&ser));
			return ser.Ok();
		}
	}
	return false;
}

IGameObjectExtension *CGameObject::QueryExtension(const char *extension) const
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	IGameObjectExtension * pRet = NULL;

	IGameObjectSystem * pGameObjectSystem = m_pGOS;
	SExtension ext;
	ext.id = pGameObjectSystem->GetID(extension);

	if (m_pEntity && (ext.id != IGameObjectSystem::InvalidExtensionID))
	{
		// locate the extension
		TExtensions::const_iterator iter = std::lower_bound( m_extensions.begin(), m_extensions.end(), ext );
		if (iter != m_extensions.end() && iter->id == ext.id)
			return iter->pExtension;
	}

	return 0;
}

bool CGameObject::GetExtensionParams( const char * extension, SmartScriptTable params )
{
	return DoGetSetExtensionParams<CSerializeScriptTableWriterImpl>(extension, params);
}

bool CGameObject::SetExtensionParams( const char * extension, SmartScriptTable params )
{
	return DoGetSetExtensionParams<CSerializeScriptTableReaderImpl>(extension, params);
}

//------------------------------------------------------------------------
IGameObjectExtension * CGameObject::ChangeExtension( const char * name, EChangeExtension change )
{
	ClearCache();

	IGameObjectExtension * pRet = NULL;

	IGameObjectSystem * pGameObjectSystem = m_pGOS;
	SExtension ext;
	// fetch extension id
	ext.id = pGameObjectSystem->GetID(name);

	if (m_pEntity) // init has been called, things are safe
	{
		m_justExchanging = true;
		if (ext.id != IGameObjectSystem::InvalidExtensionID)
		{
			// locate the extension
			TExtensions::iterator iter = std::lower_bound( m_extensions.begin(), m_extensions.end(), ext );

			// we're adding an extension
			if (change == eCE_Activate || change == eCE_Acquire)
			{
				if (iter != m_extensions.end() && iter->id == ext.id)
				{
					iter->refCount += (change == eCE_Acquire);
					iter->activated |= (change == eCE_Activate);
					pRet = iter->pExtension;
				}
				else
				{
					ext.refCount += (change == eCE_Acquire);
					ext.activated |= (change == eCE_Activate);
					pRet = ext.pExtension = pGameObjectSystem->Instantiate( ext.id, this );
					if (ext.pExtension)
					{
						if (updatingEntity == GetEntityId())
						{
							if (m_nAddingExtension == MAX_ADDING_EXTENSIONS)
								CryError("Too many extensions added in a single frame");
							m_addingExtensions[m_nAddingExtension++] = ext;
						}
						else
						{
							m_extensions.push_back( ext );
							std::sort(m_extensions.begin(), m_extensions.end());
							ext.pExtension->PostInit(this);
						}
					}
				}
			}
			else // change == eCE_Deactivate || change == eCE_Release
			{

				if (iter == m_extensions.end() || iter->id != ext.id)
				{
					if (change == eCE_Release)
					{
						assert(0);
						CryError("Attempt to release a non-existant extension %s", name);
					}
				}
				else
				{
					iter->refCount -= (change == eCE_Release);
					if (change == eCE_Deactivate)
						iter->activated = false;

					if (!iter->refCount && !iter->activated && !iter->sticky)
					{
						if (updatingEntity == GetEntityId() && numRemovingExtensions != NOT_IN_UPDATE_MODE)
						{
							if (numRemovingExtensions == MAX_REMOVING_EXTENSIONS)
								CryError("Trying to removing too many game object extensions in one frame");
							removingExtensions[numRemovingExtensions++] = iter->id;
						}
						else
							RemoveExtension( iter );
					}
				}
			}
		}
		m_justExchanging = false;
	}
	else // init has not been called... we simply make a temporary extensions array, and fill in the details later
	{
		// only safe to Activate at this stage
		assert( change == eCE_Activate );
		m_extensions.push_back(ext);
		pRet = (IGameObjectExtension *)0xf0f0f0f0;
	}

	return pRet;
}

void CGameObject::RemoveExtension( TExtensions::iterator iter )
{
	ClearCache();
	if (iter->postUpdate)
		DisablePostUpdates(iter->pExtension);
	IGameObjectExtension * pExtension = iter->pExtension;
	std::swap( m_extensions.back(), *iter );
	m_extensions.pop_back();
	std::sort(m_extensions.begin(), m_extensions.end());
	pExtension->Release();
}

//------------------------------------------------------------------------
void CGameObject::FlushExtensions(bool includeStickyBits)
{
	ClearCache();
	IGameObjectSystem * pGameObjectSystem = m_pGOS;
	static std::vector<IGameObjectSystem::ExtensionID> activatedExtensions_top;
	static bool inFlush = false;

	std::vector<IGameObjectSystem::ExtensionID> activatedExtensions_recurse;
	std::vector<IGameObjectSystem::ExtensionID> * pActivatedExtensions;

	bool wasInFlush = inFlush;
	if (inFlush)
	{
		pActivatedExtensions = &activatedExtensions_recurse;
	}
	else
	{
		pActivatedExtensions = &activatedExtensions_top;
		inFlush = true;
	}

	assert(pActivatedExtensions->empty());
	for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end(); ++iter)
	{
		if (includeStickyBits && iter->sticky)
		{
			iter->sticky = false;
			iter->activated = true;
		}
		if (iter->activated)
			pActivatedExtensions->push_back( iter->id );
	}
	while (!pActivatedExtensions->empty())
	{
		ChangeExtension(m_pGOS->GetName(pActivatedExtensions->back()), eCE_Deactivate);
		pActivatedExtensions->pop_back();
	}
	if (includeStickyBits)
	{
		//assert( m_extensions.empty() );
		assert( 0 == m_pProfileManager );
		assert( 0 == m_pActionDelegate );
		assert( 0 == m_pViewDelegate );
	}

	inFlush = wasInFlush;
}

uint8 CGameObject::GetUpdateSlotEnables( IGameObjectExtension * pExtension, int slot )
{
  SExtension * pExt = GetExtensionInfo(pExtension);
  if (pExt)
  {
    return pExt->updateEnables[slot];
  }
  return 0;
}

void CGameObject::EnableUpdateSlot( IGameObjectExtension * pExtension, int slot )
{
	SExtension * pExt = GetExtensionInfo(pExtension);
	if (pExt)
	{
		assert( 255 != pExt->updateEnables[slot] );
		++pExt->updateEnables[slot];
	}
	EvaluateUpdateActivation();
}

void CGameObject::DisableUpdateSlot( IGameObjectExtension * pExtension, int slot )
{
	bool hasUpdates = false;
	SExtension * pExt = GetExtensionInfo(pExtension);
	if (pExt)
	{
		if (pExt->updateEnables[slot])
			--pExt->updateEnables[slot];
	}
	EvaluateUpdateActivation();
}

void CGameObject::EnablePostUpdates( IGameObjectExtension * pExtension )
{
	SExtension * pExt = GetExtensionInfo(pExtension);
	if (pExt)
	{
		pExt->postUpdate = true;
		m_pGOS->SetPostUpdate(this, true);
	}
}

void CGameObject::DisablePostUpdates( IGameObjectExtension * pExtension )
{
	bool hasPostUpdates = false;
	for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end(); ++iter)
	{
		if (iter->pExtension == pExtension)
		{
			iter->postUpdate = false;
		}
		hasPostUpdates |= iter->postUpdate;
	}
	m_pGOS->SetPostUpdate(this, hasPostUpdates);
}

void CGameObject::HandleEvent( const SGameObjectEvent& evt )
{
	switch (evt.event)
	{
	case eGFE_EnablePhysics:
		break;
	case eGFE_DisablePhysics:
		{
			if (IPhysicalEntity * pEnt = GetEntity()->GetPhysics())
			{
				pe_action_awake awake;
				awake.bAwake = false;
				pEnt->Action(&awake);
			}
		}
		break;
	}
}

//------------------------------------------------------------------------
void CGameObject::SendEvent( const SGameObjectEvent& event )
{
	if (event.flags & eGOEF_ToGameObject)
		HandleEvent( event );
	if (event.flags & eGOEF_ToScriptSystem)
	{
		SmartScriptTable scriptTable = GetEntity()->GetScriptTable();
		if (event.event != IGameObjectSystem::InvalidEventID)
		{
			const char* eventName = m_pGOS->GetEventName(event.event);
			if (!!scriptTable && eventName && scriptTable->HaveValue(eventName))
			{
				Script::CallMethod( scriptTable, eventName );
			}

		}
	}
	if (event.flags & eGOEF_ToExtensions)
	{
		if (event.target == IGameObjectSystem::InvalidExtensionID)
		{
			for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end(); ++iter)
			{
				iter->pExtension->HandleEvent(event);
			}
		}
		else
		{
			SExtension ext;
			ext.id = event.target;
			TExtensions::iterator iter = std::lower_bound( m_extensions.begin(), m_extensions.end(), ext );
			if (iter != m_extensions.end())
				iter->pExtension->HandleEvent(event);
		}
	}
}

void CGameObject::SetChannelId(uint16 id)
{
	m_channelId = id;

	for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end(); ++iter)
	{
		if (iter->pExtension)
			iter->pExtension->SetChannelId(id);
	}
}

INetChannel *CGameObject::GetNetChannel() const
{
	CGameServerNub *pServerNub = CCryAction::GetCryAction()->GetGameServerNub();
	if (pServerNub)
	{
		CGameServerChannel *pChannel=pServerNub->GetChannel(m_channelId);
		if (pChannel)
			return pChannel->GetNetChannel();
	}

	return 0;
}

void CGameObject::DoInvokeRMI( _smart_ptr<CRMIBody> pBody, unsigned where, int channel )
{
	// 'where' flag validation
	if (where & eRMI_ToClientChannel)
	{
		if (channel <= 0)
		{
			GameWarning("InvokeRMI: ToClientChannel specified, but no channel specified");
			return;
		}
		if (where & eRMI_ToOwnClient)
		{
			GameWarning("InvokeRMI: ToOwnClient and ToClientChannel specified - not supported");
			return;
		}
	}
	if (where & eRMI_ToOwnClient)
	{
		if (m_channelId == 0)
		{
			GameWarning("InvokeRMI: ToOwnClient specified, but no own client");
			return;
		}
		where &= ~eRMI_ToOwnClient;
		where |= eRMI_ToClientChannel;
		channel = m_channelId;
	}
	if (where & eRMI_ToAllClients)
	{
		where &= ~eRMI_ToAllClients;
		where |= eRMI_ToOtherClients;
		channel = -1;
	}

	CCryAction * pFramework = CCryAction::GetCryAction();

	if (where & eRMI_ToServer)
	{
		CGameClientNub * pClientNub = pFramework->GetGameClientNub();
		bool called = false;
		if (pClientNub)
		{
			CGameClientChannel * pChannel = pClientNub->GetGameClientChannel();
			if (pChannel)
			{
				INetChannel * pNetChannel = pChannel->GetNetChannel();
				bool isLocal = pNetChannel->IsLocal();
				bool send = true;
				if ((where & eRMI_NoLocalCalls) != 0)
					if (isLocal)
						send = false;
				if ((where & eRMI_NoRemoteCalls) != 0)
					if (!isLocal)
						send = false;
				if (send)
				{
					pNetChannel->DispatchRMI( &*pBody );
					called = true;
				}
			}
		}
		if (!called)
		{
			GameWarning( "InvokeRMI: RMI via client (to server) requested but we are not a client" );
		}
	}
	if (where & (eRMI_ToClientChannel | eRMI_ToOtherClients))
	{
		CGameServerNub * pServerNub = pFramework->GetGameServerNub();
		if (pServerNub)
		{
			TServerChannelMap * pChannelMap = pServerNub->GetServerChannelMap();
			for (TServerChannelMap::iterator iter = pChannelMap->begin(); iter != pChannelMap->end(); ++iter)
			{
				bool isOwn = iter->first == channel;
				if (isOwn && !(where & eRMI_ToClientChannel) && !IsDemoPlayback())
					continue;
				if (!isOwn && !(where & eRMI_ToOtherClients))
					continue;
				INetChannel * pNetChannel = iter->second->GetNetChannel();
				if (!pNetChannel)
					continue;
				bool isLocal = pNetChannel->IsLocal();
				if (isLocal && (where & eRMI_NoLocalCalls) != 0)
					continue;
				if (!isLocal && (where & eRMI_NoRemoteCalls) != 0)
					continue;
				pNetChannel->DispatchRMI( &*pBody );
			}
		}
		else
		{
			GameWarning( "InvokeRMI: RMI via server (to client) requested but we are not a server" );
		}
	}
}

void CGameObject::PostUpdate(float frameTime)
{
	for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end(); ++iter)
	{
		if (iter->postUpdate)
			iter->pExtension->PostUpdate(frameTime);
	}
}

void CGameObject::SetUpdateSlotEnableCondition( IGameObjectExtension * pExtension, int slot, EUpdateEnableCondition condition )
{
	bool whenVisible = false;
	bool whenInRange = false;
	bool combineOr = false;
  bool disableWithAI = true;
	bool neverUpdate = false;

	switch (condition)
	{
	case eUEC_Never:
		neverUpdate = true;
		break;
	case eUEC_Always:
		whenVisible = false;
		whenInRange = false;
		combineOr = false;
		break;
	case eUEC_Visible:
		whenVisible = true;
		whenInRange = false;
		combineOr = false;
		break;
	case eUEC_VisibleIgnoreAI:
		whenVisible = true;
		whenInRange = false;
		combineOr = false;
		disableWithAI = false;
		break;
	case eUEC_InRange:
		whenVisible = false;
		whenInRange = true;
		combineOr = false;
		break;
	case eUEC_VisibleAndInRange:
		whenVisible = true;
		whenInRange = true;
		combineOr = false;
		break;
	case eUEC_VisibleOrInRange:
		whenVisible = true;
		whenInRange = true;
		combineOr = true;
		break;
  case eUEC_VisibleOrInRangeIgnoreAI:
    whenVisible = true;
    whenInRange = true;
    combineOr = true;
    disableWithAI = false;
    break;
  case eUEC_WithoutAI:
    disableWithAI = false;
    break;
	}

	SExtension * pExt = GetExtensionInfo(pExtension);
	if (pExt)
	{
		SET_FLAG_FOR_SLOT(pExt->flagUpdateWhenVisible, slot, whenVisible);
		SET_FLAG_FOR_SLOT(pExt->flagUpdateWhenInRange, slot, whenInRange);
		SET_FLAG_FOR_SLOT(pExt->flagUpdateCombineOr, slot, combineOr);
    SET_FLAG_FOR_SLOT(pExt->flagDisableWithAI, slot, disableWithAI);
		SET_FLAG_FOR_SLOT(pExt->flagNeverUpdate, slot, neverUpdate);
	}

	EvaluateUpdateActivation();
}


bool CGameObject::ShouldUpdate()
{
	// evaluate main-loop activation
	bool shouldUpdateAI(!GetEntity()->IsHidden() && (IsProbablyVisible() || !IsProbablyDistant()));
	bool shouldBeActivated = shouldUpdateAI;
	bool hasAI = NULL != GetEntity()->GetAI();
	bool checkAIDisableOnSlots = !shouldUpdateAI && hasAI;
	for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end() && !shouldBeActivated; ++iter)
	{
		for (int slot=0; slot<4 && !shouldBeActivated; slot++)
		{
			shouldBeActivated |= ShouldUpdateSlot( &*iter, slot, checkAIDisableOnSlots );
		}
	}
	return shouldBeActivated;
}

void CGameObject::EvaluateUpdateActivation()
{
	// evaluate main-loop activation
	bool shouldUpdateAI = ShouldUpdateAI();
	bool shouldBeActivated = shouldUpdateAI;
	bool hasAI = NULL != GetEntity()->GetAI();
	bool checkAIDisableOnSlots = !shouldUpdateAI && hasAI;
	for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end() && !shouldBeActivated; ++iter)
	{
		for (int slot=0; slot<4 && !shouldBeActivated; slot++)
		{
			shouldBeActivated |= ShouldUpdateSlot( &*iter, slot, checkAIDisableOnSlots );
		}
	}

	if (m_forceUpdate<=0)
		SetActivation(shouldBeActivated);
	shouldBeActivated |= (m_forceUpdate>0);

	// evaluate pre-physics activation
	bool shouldActivatePrePhysics = false;
	switch (m_prePhysicsUpdateRule)
	{
	case ePPU_WhenAIActivated:
		if (hasAI)
			shouldActivatePrePhysics = ShouldUpdateAI();
		else
	case ePPU_Always: // yes this is what i intended...
			shouldActivatePrePhysics = true;
		break;
	case ePPU_Never:
		break;
	default:
		assert(false);
	}

	if (shouldActivatePrePhysics != m_bPrePhysicsEnabled)
	{
		m_pEntity->PrePhysicsActivate(shouldActivatePrePhysics);
		m_bPrePhysicsEnabled = shouldActivatePrePhysics;
	}

	if (TestIsProbablyVisible(m_updateState))
		SetPhysicsDisable(false);
	else switch (m_physDisableMode)
	{
	default:
		assert(false);
	case eADPM_Never:
	case eADPM_WhenAIDeactivated:
		break;
	case eADPM_WhenInvisibleAndFarAway:
		SetPhysicsDisable(!TestIsProbablyVisible(m_updateState) && TestIsProbablyDistant(m_updateState));
		break;
	}
}

void CGameObject::SetActivation( bool activate )
{
	bool wasActivated = m_pEntity->IsActive();

	if (TestIsProbablyVisible(m_updateState))
		SetPhysicsDisable(false);
	else switch (m_physDisableMode)
	{
	default:
		assert(false);
	case eADPM_Never:
		SetPhysicsDisable(false);
	case eADPM_WhenInvisibleAndFarAway:
		break;
	case eADPM_WhenAIDeactivated:
		if (wasActivated && !activate)
			SetPhysicsDisable(true);
		break;
	}

	if (wasActivated != activate)
	{
		GetEntity()->Activate(activate);
	}

	if (activate)
		SetPhysicsDisable(false);
}

void CGameObject::SetPhysicsDisable( bool disablePhysics )
{
	if (disablePhysics != m_bPhysicsDisabled)
	{
		m_bPhysicsDisabled = disablePhysics;
		//CryLogAlways("[gobj] %s physics on %s", disablePhysics? "disable" : "enable", GetEntity()->GetName());
		SGameObjectEvent goe(disablePhysics? eGFE_DisablePhysics : eGFE_EnablePhysics, eGOEF_ToExtensions | eGOEF_ToGameObject);
		SendEvent(goe);
	}
}

/*
void CGameObject::SetVisibility( bool isVisible )
{
	if (isVisible != m_isVisible)
	{
//		CryLogAlways("%s is now %svisible", m_pEntity->GetName(), isVisible? "" : "in");

		uint32 flags = m_pEntity->GetFlags();
		if (isVisible)
			flags &= ~ENTITY_FLAG_SEND_RENDER_EVENT;
		else
			flags |= ENTITY_FLAG_SEND_RENDER_EVENT;
		m_pEntity->SetFlags(flags);

		m_isVisible = isVisible;
	}
}
*/

IWorldQuery * CGameObject::GetWorldQuery()
{
	if (IWorldQuery *query = (IWorldQuery*)AcquireExtension("WorldQuery"))
		return query;
	else
		return NULL;
}

IMovementController * CGameObject::GetMovementController()
{
	if (IActor * pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(m_pEntity->GetId()))
		return pActor->GetMovementController();
	else if (IVehicle * pVehicle = CCryAction::GetCryAction()->GetIVehicleSystem()->GetVehicle(m_pEntity->GetId()))
		return pVehicle->GetMovementController();
	else
		return NULL;
}

bool CGameObject::SetAspectProfile( EEntityAspects aspect, uint8 profile, bool fromNetwork )
{
	bool ok = DoSetAspectProfile(aspect, profile, fromNetwork);

	if (ok && aspect == eEA_Physics && m_isBoundToNetwork && gEnv->bMultiplayer)
	{
		if (IPhysicalEntity * pEnt = GetEntity()->GetPhysics())
		{
			pe_params_flags flags;
			flags.flagsOR = pef_log_collisions;
			pEnt->SetParams(&flags);
		}
	}

	return ok;
}

bool CGameObject::DoSetAspectProfile( EEntityAspects aspect, uint8 profile, bool fromNetwork )
{
	if (m_isBoundToNetwork)
	{
		if (fromNetwork)
		{
			if (m_pProfileManager)
			{
				if (m_pProfileManager->SetAspectProfile( aspect, profile ))
				{
					m_profiles[IntegerLog2(uint8(aspect))] = profile;
					return true;
				}
			}
			else
				return false;
		}
		else
		{
			if (CGameContext* pGameContext = CCryAction::GetCryAction()->GetGameContext())
			{
				if (INetContext * pNetContext = pGameContext->GetNetContext())
				{
					pNetContext->SetAspectProfile( GetEntityId(), aspect, profile );
					m_profiles[IntegerLog2(uint8(aspect))] = profile;
					return true;
				}
			}
			return false;
		}
	}
	else if (m_pProfileManager)
	{
		//assert( !fromNetwork );
		if(m_pProfileManager->SetAspectProfile( aspect, profile ))
		{
			m_profiles[IntegerLog2(uint8(aspect))] = profile;
			return true;
		}
	}
	return false;
}

void CGameObject::AttachDistanceChecker()
{
	if (CCryAction::GetCryAction()->IsEditing())
		return;
	if (!m_distanceChecker)
	{
		IEntity * pDistanceChecker = m_pGOS->CreatePlayerProximityTrigger();
		if (pDistanceChecker)
		{
			((IEntityTriggerProxy*)pDistanceChecker->GetProxy(ENTITY_PROXY_TRIGGER))->ForwardEventsTo(GetEntityId());
			m_distanceCheckerTimer = -1.0f;
			m_distanceChecker = pDistanceChecker->GetId();
			UpdateDistanceChecker(0.0f);
		}
	}
}

void CGameObject::ForceUpdate(bool force)
{
	if (force)
		++m_forceUpdate;
	else
		--m_forceUpdate;

	assert(m_forceUpdate>=0);
}

void CGameObject::UpdateDistanceChecker( float frameTime )
{
	if (!m_distanceChecker)
		return;
	bool ok = false;
	IEntity * pDistanceChecker = gEnv->pEntitySystem->GetEntity( m_distanceChecker );
	if (pDistanceChecker)
	{
		m_distanceCheckerTimer -= frameTime;
		if (m_distanceCheckerTimer < 0.0f)
		{
			IEntityTriggerProxy * pProxy = (IEntityTriggerProxy*)pDistanceChecker->GetProxy(ENTITY_PROXY_TRIGGER);
			if (pProxy)
			{
				pProxy->SetTriggerBounds( CreateDistanceAABB(GetEntity()->GetWorldPos()) );
				m_distanceCheckerTimer = DISTANCE_CHECKER_TIMEOUT;
				ok = true;
			}
		}
		else
		{
			ok = true;
		}
	}
	if (!ok)
	{
    if (pDistanceChecker)
		  gEnv->pEntitySystem->RemoveEntity(m_distanceChecker, true);
		m_distanceChecker = 0;
		AttachDistanceChecker();
	}
}

struct SContainerSer : public ISerializableInfo
{
	void SerializeWith(TSerialize ser)
	{
		for (size_t i=0; i<m_children.size(); i++)
			m_children[i]->SerializeWith(ser);
	}

	std::vector<ISerializableInfoPtr> m_children;
};

ISerializableInfoPtr CGameObject::GetSpawnInfo()
{
	_smart_ptr<SContainerSer> pC;

	for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end(); ++iter)
	{
		if (iter->pExtension)
		{
			ISerializableInfoPtr pS = iter->pExtension->GetSpawnInfo();
			if (pS)
			{
				if (!pC)
					pC = new SContainerSer;
				pC->m_children.push_back(pS);
			}
		}
	}
	return &*pC;
}

void CGameObject::SetNetworkParent( EntityId id )
{
	CCryAction::GetCryAction()->GetGameContext()->GetNetContext()->SetParentObject( GetEntityId(), id );
}

uint8 CGameObject::GetDefaultProfile( EEntityAspects aspect )
{
	if (m_pProfileManager)
		return m_pProfileManager->GetDefaultProfile(aspect);
	else
		return 0;
}

bool CGameObject::SetAIActivation(EGameObjectAIActivationMode mode)
{
	// this should be done only for gameobjects with AI. Otherwise breaks weapons (scope update, etc) 
	if(!GetEntity()->GetAI())
		return false;

	if (m_aiMode != mode)
	{
		m_aiMode = mode;
		EvaluateUpdateActivation(); // need to recheck any updates on slots
	}

	return GetEntity()->IsActive();
}

bool CGameObject::ShouldUpdateAI()
{
	if (GetEntity()->IsHidden() || !GetEntity()->GetAI())
		return false;

	switch (m_aiMode)
	{
	case eGOAIAM_Never:
		return false;
	case eGOAIAM_Always:
		return true;
	case eGOAIAM_VisibleOrInRange:
		return IsProbablyVisible() || !IsProbablyDistant();
	default:
		assert(false);
		return false;
	}
}

void CGameObject::EnablePrePhysicsUpdate( EPrePhysicsUpdate updateRule )
{
	m_prePhysicsUpdateRule = updateRule;
	EvaluateUpdateActivation();
}

void CGameObject::Pulse( uint32 pulse )
{
	if (CGameContext * pGC = CCryAction::GetCryAction()->GetGameContext())
		pGC->GetNetContext()->PulseObject(GetEntityId(), pulse);
}

void CGameObject::PostRemoteSpawn()
{
	for (size_t i = 0; i < m_extensions.size(); ++i)
		m_extensions[i].pExtension->PostRemoteSpawn();
}

void CGameObject::GetMemoryStatistics(ICrySizer * s)
{
	{
		SIZER_SUBCOMPONENT_NAME(s, "GameObject");
		s->Add(*this);
		s->AddContainer(m_extensions);
	}

	for (size_t i=0; i<m_extensions.size(); i++)
	{
		SIZER_SUBCOMPONENT_NAME(s, m_pGOS->GetName( m_extensions[i].id ));
		m_extensions[i].pExtension->GetMemoryStatistics(s);
	}
}

void CGameObject::RegisterAsPredicted()
{
	assert(!m_predictionHandle);
	m_predictionHandle = CCryAction::GetCryAction()->GetNetContext()->RegisterPredictedSpawn(CCryAction::GetCryAction()->GetClientChannel(), GetEntityId());
}

int CGameObject::GetPredictionHandle()
{
	return m_predictionHandle;
}

void CGameObject::RegisterAsValidated(IGameObject* pGO, int predictionHandle)
{
	if (!pGO)
		return;
	m_predictionHandle = predictionHandle;
	CCryAction::GetCryAction()->GetNetContext()->RegisterValidatedPredictedSpawn(pGO->GetNetChannel(), m_predictionHandle, GetEntityId());
}

void CGameObject::SetAutoDisablePhysicsMode( EAutoDisablePhysicsMode mode )
{
	if (m_physDisableMode != mode)
	{
		m_physDisableMode = mode;
		EvaluateUpdateActivation();
	}
}

void CGameObject::RequestRemoteUpdate( uint8 aspectMask )
{
	if (INetContext * pNC = CCryAction::GetCryAction()->GetNetContext())
		pNC->RequestRemoteUpdate( GetEntityId(), aspectMask );
}
