#include "StdAfx.h"
#include "AnimatedCharacter.h"
#include "CryAction.h"
#include "AnimationGraphManager.h"
#include "AnimationGraphState.h"
#include "AnimationGraphCVars.h"
#include "HumanBlending.h"
#include "PersistantDebug.h"
#include "IFacialAnimation.h"

#include <IViewSystem.h>
#include <ISound.h>

CAnimatedCharacter::SDesiredParams	CAnimatedCharacter::s_desiredParams;
const char* g_szAnimationGraphLayerProperty[eAnimationGraphLayer_COUNT] = { "AnimationGraph" , "UpperBodyGraph" };

#define ANIMCHAR_MEM_DEBUG  // Memory Allocation Tracking
#undef  ANIMCHAR_MEM_DEBUG

#define ENABLE_NAN_CHECK

#undef CHECKQNAN_FLT
#if defined(ENABLE_NAN_CHECK)
#define CHECKQNAN_FLT(x) \
	assert(((*(unsigned*)&(x))&0xff000000) != 0xff000000u && (*(unsigned*)&(x) != 0x7fc00000))
#else
#define CHECKQNAN_FLT(x) (void*)0
#endif

#ifndef CHECKQNAN_VEC
#define CHECKQNAN_VEC(v) \
	CHECKQNAN_FLT(v.x); CHECKQNAN_FLT(v.y); CHECKQNAN_FLT(v.z)
#endif

static float blue[4] = {0,0,1,1};
static float red[4] = {1,0,0,1};
static float yellow[4] = {1,1,0,1};
static float green[4] = {0,1,0,1};
static float white[4] = {1,1,1,1};
static CTimeValue lastTime;
static int y = 100;

bool CheckNAN(const f32 &f)
{
	return ((*(unsigned*)&(f))&0xff000000) != 0xff000000u && (*(unsigned*)&(f) != 0x7fc00000);
}
bool CheckNANVec(Vec3 &v, IEntity *pEntity)
{
	if (!CheckNAN(v.x) || !CheckNAN(v.y) || !CheckNAN(v.z))
	{
		if (ICharacterInstance *pCharacter = pEntity->GetCharacter(0))
		{
			int numAnims = pCharacter->GetISkeletonAnim()->GetNumAnimsInFIFO(0);
			for (int i=0;i<numAnims;++i)
			{
				const CAnimation &anim = pCharacter->GetISkeletonAnim()->GetAnimFromFIFO(0,i);
				CryLogAlways("NAN on AnimationID:%d, model:%s, entity:%s",anim.m_LMG0.m_nAnimID[i],pCharacter->GetFilePath(),pEntity->GetName());
			}
		}

		v.zero();
		return false;
	}
	return true;
}

bool CheckNANVec(const Vec3 &v, IEntity *pEntity)
{
	Vec3 v1(v);
	return CheckNANVec(v1, pEntity);
}

CAnimatedCharacter::CAnimatedCharacter() 
{
	for ( int layer = 0; layer < eAnimationGraphLayer_COUNT; ++layer )
	{
		m_pGraph[layer] = 0;
		m_pAnimationState[layer] = 0;
	}
	int i = sizeof(CAnimatedCharacter);

	m_debugHistoryManager = gEnv->pGame->GetIGameFramework()->CreateDebugHistoryManager();

	m_bAnimationGraphStatePaused = false;

	//ground-alignment
	m_fStandUpTimer=1.0f;
	m_fJumpSmooth=1.0f;
	m_fJumpSmoothRate=1.0f;
	m_fGroundSlopeSmooth=0.0f;
	m_fGroundSlopeRate=0.0f;
	m_fRootHeightSmooth=0.0f;
	m_fRootHeightRate=0.0f;


	m_LLastHeel2D						=Vec2(-99999.0f,-99999.0f);
	m_LLastHeelIVec.normal	=Vec3(0,0,1);
	m_LLastHeelIVec.pos			=Vec3(-99999.0f,-99999.0f,-99999.0f);
	m_LLastHeelIVecSmooth.normal	=Vec3(0,0,1);
	m_LLastHeelIVecSmooth.pos			=Vec3(-99999.0f,-99999.0f,-99999.0f);
	m_LLastHeelIVecSmoothRate.normal	=Vec3(0,0,1);
	m_LLastHeelIVecSmoothRate.pos			=Vec3(-99999.0f,-99999.0f,-99999.0f);

	m_RLastHeel2D						=Vec2(-99999.0f,-99999.0f);
	m_RLastHeelIVec.normal	=Vec3(0,0,1);
	m_RLastHeelIVec.pos			=Vec3(-99999.0f,-99999.0f,-99999.0f);
	m_RLastHeelIVecSmooth.normal	=Vec3(0,0,1);
	m_RLastHeelIVecSmooth.pos			=Vec3(-99999.0f,-99999.0f,-99999.0f);
	m_RLastHeelIVecSmoothRate.normal	=Vec3(0,0,1);
	m_RLastHeelIVecSmoothRate.pos			=Vec3(-99999.0f,-99999.0f,-99999.0f);

	m_pFeetColliderPE = NULL;

	// TODO/NOTE: Should this be called here. It's also called from Init().
	ResetVars();
}

CAnimatedCharacter::~CAnimatedCharacter()
{
	if (m_pAnimationState)
	{
		m_animationGraphStates.RemoveListener( this );
		for ( int layer = 0; layer < eAnimationGraphLayer_COUNT; ++layer )
			if ( m_pAnimationState[layer] )
				m_pAnimationState[layer]->Release();
	}

	if(m_debugHistoryManager)
		m_debugHistoryManager->Release();
}

void CAnimatedCharacter::SetExtraAnimationOffset( const Matrix34& offset )
{
	m_extraAnimationOffset = (QuatT)offset;
}

void CAnimatedCharacter::SetExtraAnimationOffset( const QuatT& offset )
{
	m_extraAnimationOffset = offset;
}

bool CAnimatedCharacter::Init( IGameObject * pGameObject )
{
#ifdef ANIMCHAR_MEM_DEBUG
	CCryAction::GetCryAction()->DumpMemInfo("CAnimatedCharacter::Init %p start", pGameObject);
#endif

	SetGameObject( pGameObject );

	IEntity * pEntity = GetEntity();
	if (!pEntity)
		return false;
	SmartScriptTable pScriptTable = pEntity->GetScriptTable();
	if (!pScriptTable)
		return false;



	int foundLayerCount = 0;
	for ( int layer = 0; layer < eAnimationGraphLayer_COUNT; ++layer)
	{
		const char * animationGraph = 0;
		if (!pScriptTable->GetValue(g_szAnimationGraphLayerProperty[layer], animationGraph) || !animationGraph)
			continue;

		if (strcmp(animationGraph, "") == 0) // Less hardcore, but also less obscure than (!*animationGraph).
			continue;

		m_pGraph[foundLayerCount] = (CAnimationGraph*) &*CCryAction::GetCryAction()->GetAnimationGraphManager()->LoadGraph(animationGraph, false, true);

		if (m_pGraph[foundLayerCount] == NULL)
		{
			CryLogAlways("ERROR: AnimatedCharacter::Init() - Can't load AnimationGraph '%s', game will crash, for sure...", animationGraph);
			continue;
		}

		m_pAnimationState[foundLayerCount] = (CAnimationGraphState*) m_pGraph[foundLayerCount]->CreateState();
		if (m_pAnimationState[foundLayerCount] == NULL)
			continue;

		m_animationGraphStates.AddLayerReference(m_pAnimationState[foundLayerCount]);

		foundLayerCount++;
	}

	m_animationGraphStates.SetAnimatedCharacter( this, 0, NULL );
	m_animationGraphStates.RebindInputs();

	SAnimationStateData data;
	data.pEntity = GetEntity();
	data.pGameObject = GetGameObject();
	data.pAnimatedCharacter = this;
	m_animationGraphStates.SetBasicStateData( data );
	m_animationGraphStates.AddListener( "animchar", this );

	if (foundLayerCount == 0)
	{
		GameWarning("No AnimationGraph layer property defined for entity");
		return false;
	}

	ResetVars();

#ifdef ANIMCHAR_MEM_DEBUG
	CCryAction::GetCryAction()->DumpMemInfo("CAnimatedCharacter::Init %p end", pGameObject);
#endif

	return true;
}

void CAnimatedCharacter::PostInit( IGameObject * pGameObject )
{
	pGameObject->EnableUpdateSlot( this, 0 );
	pGameObject->SetUpdateSlotEnableCondition( this, 0, eUEC_Visible );
	pGameObject->EnablePhysicsEvent(true, eEPE_OnCollisionLogged);
}

void CAnimatedCharacter::Release()
{
	delete this;
}

void CAnimatedCharacter::FullSerialize( TSerialize ser )
{
#define SerializeMember(member)		ser.Value(#member, member)
#define SerializeMemberType(type, member)		{ type temp = member; ser.Value(#member, temp); if (isReading) member = temp; }
#define SerializeNamedType(type, name, type2, var)		{ type temp = var; ser.Value(name, temp); if (isReading) var = (type2)temp; }

	bool isReading = ser.IsReading();

	if (isReading)
		m_lastSerializeReadFrameID = m_curFrameID; /*gEnv->pRenderer->GetFrameID()*/;

	// TODO: Find out what defines an inactive character and what can be safely omitted in that case.
	// Is it safe to ignore inactive entities? Don't they have any active information?
	bool bEntityActive = true; //GetEntity()->IsActive();
	if (!ser.BeginOptionalGroup("AnimatedCharacter", bEntityActive))
		return;

	if (isReading)
		ResetVars();

	SerializeMember(m_prevEntLocation.t);
	SerializeMember(m_prevEntLocation.q);
	SerializeMember(m_animLocation.t);
	SerializeMember(m_animLocation.q);
	SerializeMember(m_prevAnimLocation.t);
	SerializeMember(m_prevAnimLocation.q);

	SerializeMember(m_expectedEntMovement.t);
	SerializeMember(m_expectedEntMovement.q);
	SerializeMember(m_actualEntMovement.t);
	SerializeMember(m_actualEntMovement.q);

	SerializeMember(m_expectedEntMovementLengthPrev);
	SerializeMember(m_actualEntMovementLengthPrev);

	SerializeMember(m_actualEntVelocity);

	SerializeMember(m_requestedEntityMovement.t);
	SerializeMember(m_requestedEntityMovement.q);

	m_animationGraphStates.Serialize( ser );

	SerializeMember(m_currentStance);
	GetParams().ModifyFlags( eACF_ImmediateStance, 0 );

	SerializeMemberType(bool, m_allowLookIk);

	bool hasAnimTarget = (m_animTargetTime > -5) && (m_pAnimTarget != NULL) && (m_pAnimTarget->preparing || m_pAnimTarget->activated);
	if (ser.BeginOptionalGroup("AnimatedCharacter_AnimTarget", hasAnimTarget))
	{
		//SerializeMember(m_animTarget.t);
		//SerializeMember(m_animTarget.q);
		SerializeMember(m_animTargetTime);
		ser.EndGroup();
	}

	SerializeMember(m_overrideClampDistance);
	SerializeMember(m_overrideClampAngle);

	string cml("m_colliderModeLayer");
	for (int layer = 0; layer < eColliderModeLayer_COUNT; layer++)
	{
		uint8 mode = m_colliderModeLayers[layer];
		ser.Value(cml + g_szColliderModeLayerString[layer], mode);
		if (ser.IsReading())
			m_colliderModeLayers[layer] = (EColliderMode)mode;
	}

	for (int slot = 0; slot < eMCMSlot_COUNT; ++slot)
	{
		string mcm;
		mcm.Format("m_movementControlMethodH%d", slot);
		SerializeNamedType(uint8, mcm, EMovementControlMethod, m_movementControlMethod[eMCMComponent_Horizontal][slot]);
		mcm.Format("m_movementControlMethodV%d", slot);
		SerializeNamedType(uint8, mcm, EMovementControlMethod, m_movementControlMethod[eMCMComponent_Vertical][slot]);
	}
	ser.Value("m_elapsedTimeMCM_Horizontal", m_elapsedTimeMCM[eMCMComponent_Horizontal]);
	ser.Value("m_elapsedTimeMCM_Vertical", m_elapsedTimeMCM[eMCMComponent_Vertical]);

	ser.EndGroup(); //AnimatedCharacter
}

void CAnimatedCharacter::PrePhysicsUpdate()
{
	PreAnimationUpdate();

	if (m_curFrameTime <= 0.0f)
		return;

	if (IsAnimGraphUpdateNeeded() && !InCutscene())
	{
		// make sure that the anim graph is unpaused
		m_bAnimationGraphStatePaused = false;
		m_animationGraphStates.Pause( false, eAGP_StartGame );
		m_animationGraphStates.Update();

		// NOTE: This is needed to be re-cached here, unfortunately, since the AGUpdate can delete the memory.
		m_pAnimTarget = m_animationGraphStates.GetAnimationTarget();
	}

	CalculateParamsForCurrentMotions();


	if (m_pCharacter)
	{
		// Turn off auto updating on this character,
		// AnimatedCharacter will be forcing Pre and Post update manually.
		m_pCharacter->SetFlags( m_pCharacter->GetFlags()&(~CS_FLAG_UPDATE) );
	}

	if (m_pCharacter != NULL)
	{
		float scale= GetEntity()->GetWorldTM().GetColumn(0).GetLength(); 
		m_pCharacter->SkeletonPreProcess(m_entLocation, QuatTS(m_animLocation.q,m_animLocation.t,scale), GetISystem()->GetViewCamera(), 0x666);
	}

	UpdateMCMs();
	UpdateCurAnimLocation();

	// Entity orientation is set directly, not requested via physics.
	// We need the current location before calculating and setting the new one.
	m_entLocation.q = GetEntity()->GetWorldRotation();
	CalculateAndRequestPhysicalEntityMovement();

	UpdatePhysicalColliderMode();
	UpdatePhysicsInertia();


	//extern f32 g_fPrintLine;
	//const ColorF cWhite = ColorF(1,0,0,1);
	//gEnv->pRenderer->Draw2dLabel(10, g_fPrintLine, 1.9f, (float*)&cWhite, false, "Update: %x %x",m_simplifyMovement, (m_pCharacter!=0) );
	//g_fPrintLine += 0x20;

	if (m_simplifyMovement && m_pCharacter)
	{
		//if a character is not visible on screen, we still have to update the world position of the attached entity
		QuatT EntLocation;
		EntLocation.q=GetEntity()->GetWorldRotation();
		EntLocation.t=GetEntity()->GetWorldPos();
		m_pCharacter->UpdateAttachedObjectsFast(EntLocation,0.0f,1);
	}
}

void CAnimatedCharacter::Update( SEntityUpdateContext& ctx, int slot )
{
#if _DEBUG && defined(USER_david)
	SCOPED_ENABLE_FLOAT_EXCEPTIONS;
#endif

	//assert(!m_simplifyMovement); // If we have simplified movement, the this GameObject extension should not be updated here.

	assert(m_entLocation.IsValid());
	assert(m_animLocation.IsValid());
	assert(m_colliderModeLayers[eColliderModeLayer_ForceSleep] == eColliderMode_Undefined);

	GetCurrentEntityLocation();

	float EntRotZ = RAD2DEG(m_entLocation.q.GetRotZ());

	assert(m_entLocation.IsValid());
	assert(m_animLocation.IsValid());

	if (HasAtomicUpdate())
		PrePhysicsUpdate();

	assert(m_entLocation.IsValid());
	assert(m_animLocation.IsValid());

	// Everything below here is redundant then m_bGrabbedInViewSpace is true.
	if (m_bGrabbedInViewSpace)
		return;

	ClampAnimLocation();
	
	assert(m_entLocation.IsValid());
	assert(m_animLocation.IsValid());

	QuatT animRenderLocation = CalculateAnimRenderLocation();

	assert(animRenderLocation.IsValid());

	float fDistance = ((ctx.pCamera ? ctx.pCamera->GetPosition() : animRenderLocation.t) - animRenderLocation.t).GetLength();
	float fZoomFactor = 0.001f + 0.999f*(RAD2DEG((ctx.pCamera ? ctx.pCamera->GetFov() : 60.0f))/60.f);

	if (m_pCharacter != NULL)
	{
		float scale= GetEntity()->GetWorldTM().GetColumn(0).GetLength(); 
		m_pCharacter->SkeletonPostProcess(m_entLocation, QuatTS(animRenderLocation.q,animRenderLocation.t,scale), NULL, fDistance * fZoomFactor, 0x666);
	}

	DebugRenderCurLocations();

#ifdef _DEBUG
	DebugGraphQT(m_entLocation, "eDH_EntLocationPosX", "eDH_EntLocationPosY", "eDH_EntLocationOriZ");
	DebugGraphQT(m_animLocation, "eDH_AnimLocationPosX", "eDH_AnimLocationPosY", "eDH_AnimLocationOriZ");

	//DebugDisplayNewLocationsAndMovements(wantedEntLocationClamped, wantedEntMovement, wantedAnimLocationClamped, wantedAnimMovement, m_curFrameTime);
#endif
}

/*
void CAnimatedCharacter::MakePushable(bool enable)
{
	IPhysicalEntity *pPhysEnt = GetEntity()?GetEntity()->GetPhysics():NULL;
	if (pPhysEnt)
	{
		pe_params_flags pFlags;

		pFlags.flagsAND = ~pef_pushable_by_players;
		pFlags.flagsOR = pef_pushable_by_players * enable;

		pPhysEnt->SetParams(&pFlags);
	}
}
*/

void CAnimatedCharacter::ForceRefreshPhysicalColliderMode()
{
	m_forcedRefreshColliderMode=true;
}

//void CAnimatedCharacter::EnablePhysicalCollider(bool enable)
void CAnimatedCharacter::RequestPhysicalColliderMode(EColliderMode mode, EColliderModeLayer layer, const char* tag /* = NULL */)
{
	bool update = (m_colliderModeLayers[layer] != mode);
	if (update || m_forcedRefreshColliderMode)
	{
		m_colliderModeLayersTag[layer] = tag;
		m_colliderModeLayers[layer] = mode;
		UpdatePhysicalColliderMode();
	}
}

void CAnimatedCharacter::HandleEvent( const SGameObjectEvent& event )
{
	switch (event.event)
	{
	case eGFE_BecomeLocalPlayer: m_isClient = true;	break;
	case eGFE_OnCollision:
		{
			const EventPhysCollision* pCollision = static_cast<const EventPhysCollision*>(event.ptr);
			
			// Ignore bullets and insignificant particles, etc.
			// TODO: This early-out condition should ideally be done on a higher level, 
			// to avoid even touching this memory for all bullets and stuff.
			if (pCollision->pEntity[0]->GetType() == PE_PARTICLE)
				break;

			if (m_curFrameID > m_collisionFrameID)
			{
				m_collisionNormalCount = 0;
				m_collisionNormal[0].zero();
				m_collisionNormal[1].zero();
				m_collisionNormal[2].zero();
				m_collisionNormal[3].zero();
			}

			if ((m_curFrameID < m_collisionFrameID) || (m_collisionNormalCount >= 4) || (m_curFrameID <= 10))
				break;

			// Both entities in a collision recieve the same direction of the normal.
			// We need to flip the normal if this is the second entity.
			IPhysicalEntity* pPhysEnt = GetEntity()->GetPhysics();
			if (pPhysEnt == pCollision->pEntity[0])
				m_collisionNormal[m_collisionNormalCount] = pCollision->n;
			else if (pPhysEnt == pCollision->pEntity[1])
				m_collisionNormal[m_collisionNormalCount] = -pCollision->n;
/*
			// This might happen for faked collisions, such as punches.
			else 
				assert(!"Entity recieved collision event without being part of collision!");
*/
			// We only care about the horizontal part, so we remove the vertical component for simplicity.
			m_collisionNormal[m_collisionNormalCount].z = 0.0f; m_collisionNormal[m_collisionNormalCount].Normalize();

#if _DEBUG && defined(USER_david)
			CPersistantDebug* pPD = CCryAction::GetCryAction()->GetPersistantDebug();
			if ((pPD != NULL) && true)
			{
				pPD->Begin(UNIQUE("AnimatedCharacter.HandleEvent.CollisionNormal"), false);
				pPD->AddSphere(pCollision->pt, 0.02f, ColorF(1,0.75f,0.0f,1), 0.1f);
				pPD->AddLine(pCollision->pt, pCollision->pt + m_collisionNormal[m_collisionNormalCount] * 0.5f, ColorF(1,0.75f,0.0f,1), 0.1f);
			}
#endif

			m_collisionFrameID = m_curFrameID;
			m_collisionNormalCount++;

			break;
		}
	case eGFE_ResetAnimationGraphs:
		m_animationGraphStates.Reset();
		break;
	case eGFE_EnablePhysics:
		RequestPhysicalColliderMode(eColliderMode_Undefined, eColliderModeLayer_ForceSleep, "eGFE_EnablePhysics");
		break;
	case eGFE_DisablePhysics:
		RequestPhysicalColliderMode(eColliderMode_Disabled, eColliderModeLayer_ForceSleep, "eGFE_DisablePhysics");
		break;
	}
 
	// AnimationControlled/GameControlled: DEPRECATED in favor of MovementControlMethod controlled by AnimGraph.
}

void CAnimatedCharacter::PushForcedState( const char * state )
{
	m_animationGraphStates.PushForcedState( state );
}

void CAnimatedCharacter::ClearForcedStates()
{
	m_animationGraphStates.ClearForcedStates();
}

void CAnimatedCharacter::ChangeGraph( const char * graphs, int layer)
{
	//CCryAction::GetCryAction()->GetAnimationGraphManager()->LoadGraph(graph);

	m_pGraph[layer] = (_smart_ptr<CAnimationGraph>) m_pAnimationState[layer]->ChangeGraph( graphs );
	m_animationGraphStates.RebindInputs();

	SAnimationStateData data;
	data.pGameObject = GetGameObject();
	data.pEntity = GetGameObject()->GetEntity();
	data.pAnimatedCharacter = this;
	m_pAnimationState[layer]->SetBasicStateData( data );
}

void CAnimatedCharacter::ResetState()
{
	m_animationGraphStates.Reset();
	ResetVars();
}

//inline float GetRotationFixup()
//	{ return 0; }

//inline Quat GetEntityRotation(IEntity *pEntity)
//{
//	return Quat::CreateRotationZ(-GetRotationFixup())*pEntity->GetRotation();
//}

void CAnimatedCharacter::ResetVars()
{
	m_lastResetFrameId = gEnv->pRenderer->GetFrameID();
	m_updateSkeletonSettingsFrameID = 0;
	m_bSimpleMovementSetOnce = false;
	m_curWeaponRaisedPose = eWeaponRaisedPose_None;

	IEntity* pEntity = GetEntity();

	m_bAnimationGraphStatePaused = true;
	m_animationGraphStates.Pause( true, eAGP_StartGame );

	m_currentStance = -1;
	m_requestedStance = -1;
	m_stanceQuery = 0;

	m_moveRequestFrameID = -1; // Why is -1 use here, and not 0?

	m_facialAlertness = 0;

	m_allowLookIk = true;

	if (pEntity != NULL)
	{
		m_entLocation.t = pEntity->GetWorldPos();
		m_entLocation.q = pEntity->GetWorldRotation();
		m_prevEntLocation = m_entLocation;
	}
	else
	{
		m_animLocation.SetIdentity();
		m_prevAnimLocation.SetIdentity();
	}
	m_animLocation = m_entLocation;
	m_prevAnimLocation = m_entLocation;
	m_parentEntLocation.SetIdentity();

	m_devatedPositionTime.SetValue(0);
	m_devatedOrientationTime.SetValue(0);

	m_curFrameTime = 0.0f;
	m_prevFrameTime = 0.0f;
	m_curFrameID = -1; // Why -1, why not 0? (copy pasted from above)

	m_lastSerializeReadFrameID = 0;

	m_updateSkeletonSettingsFrameID = -1;
	m_updateGrabbedInputFrameID = -1;

	m_expectedEntMovement.SetIdentity();
	m_actualEntMovement.SetIdentity();
	m_entTeleportMovement.SetIdentity();

// Procedural Leaning Stuff (wip)
	for (int i = 0; i < NumAxxSamples; i++)
	{
		m_reqLocalEntAxx[i].zero();
		m_reqEntVelo[i].zero();
		m_reqEntTime[i].SetValue(0);
	}
	m_reqLocalEntAxxNextIndex = 0;
	m_smoothedActualEntVelo.zero();
	m_smoothedAmountAxx = 0.0f;
	m_avgLocalEntAxx.zero();

	m_requestedEntityMovement.SetIdentity();
	m_requestedEntityMovementType = RequestedEntMovementType_Undefined;
	m_requestedIJump = 0;
	m_bDisablePhysicalGravity = false;

	m_desiredAnimMovement.SetIdentity();

	m_actualEntVelocity = 0.0f;

	m_actualEntMovementLengthPrev = 0.0f;
	m_expectedEntMovementLengthPrev = 0.0f;

	m_requestedEntMoveDirLH4 = 0;
	m_actualEntMoveDirLH4 = 0;

	for (int slot = 0; slot < eMCMSlot_COUNT; ++slot)
	{
		m_movementControlMethod[eMCMComponent_Horizontal][slot] = eMCM_Entity;
		m_movementControlMethod[eMCMComponent_Vertical][slot] = eMCM_Entity;
	}
	m_elapsedTimeMCM[eMCMComponent_Horizontal] = 0.0f;
	m_elapsedTimeMCM[eMCMComponent_Vertical] = 0.0f;

	m_prevAnimPhaseHash = 0;
	m_prevAnimEntOffsetHash = 0.0f;

	m_prevMoveVeloHash = 0.0f;
	m_prevMoveJump = 0;

	m_prevOffsetDistance = 0.0f;
	m_prevOffsetAngle = 0.0f;
	m_overrideClampDistance = -1.0f;
	m_overrideClampAngle = -1.0f;

	m_collisionFrameID = -1;
	m_collisionNormal[0] = ZERO;
	m_collisionNormal[1] = ZERO;
	m_collisionNormal[2] = ZERO;
	m_collisionNormal[3] = ZERO;

	DestroyExtraSolidCollider();

	m_colliderMode = eColliderMode_Undefined;
	for (int layer = 0; layer < eColliderModeLayer_COUNT; layer++)
	{
		m_colliderModeLayers[layer] = eColliderMode_Undefined;
		m_colliderModeLayersTag[layer] = NULL;
	}

	m_forcedRefreshColliderMode = false;

	// Disable physics as default when reviving/resetting characters.
	// Only a few frames after reset will non-disabled collider be allowed in UpdatePhysicalColliderMode().
	if (pEntity != NULL)
	{
		IPhysicalEntity *pPhysEnt = pEntity->GetPhysics();
		if (pPhysEnt != NULL)
		{
			pe_player_dynamics pd;
			pe_params_part pp;
			pd.bActive = 0;
			pp.flagsAND = ~geom_colltype_player;
			pPhysEnt->SetParams(&pd);
			pPhysEnt->SetParams(&pp);
		}
	}

	for (int inputIndex = 0; inputIndex < eACInputIndex_COUNT; ++inputIndex)
	{
		m_inputID[inputIndex] = m_animationGraphStates.GetInputId(g_szInputIDStr[inputIndex]);
	}

	m_animationGraphStates.SetInput( m_inputID[eACInputIndex_Stance], "null" );

	m_simplifiedAGSpeedInputsRequested = false;
	m_simplifiedAGSpeedInputsActual = false;

	m_sleepAnimGraph = false;
	m_simplifyMovement = false;
	m_forceDisableSlidingContactEvents = false;
	m_noMovementTimer = 0.0f;
	//m_nonMovementFrames = MAX_NON_MOVEMENT_FRAMES; // Why was this initialized to max and not zero?!

	m_bGrabbedInViewSpace = false;
	m_noMovementOverrideExternal = false;

	m_pAnimErrorClampStats = 0;

	m_debugHistoryManager->Clear();

	m_extraAnimationOffset.SetIdentity();

	m_animTarget = IDENTITY;
	m_animTargetTime = -5.0f;

	m_isPlayer = false;
	m_isClient = false;
	if (pEntity != NULL)
	{
		IActorSystem* pActorSystem = CCryAction::GetCryAction()->GetIActorSystem();
		assert(pActorSystem != NULL);
		IActor* pActor = pActorSystem->GetActor(pEntity->GetId());
		assert(pActor != NULL);
		if (pActor != NULL)
		{
			m_isPlayer = pActor->IsPlayer();
			m_isClient = pActor->IsClient();
		}
	}

	if (pEntity != NULL)
	{
		IEntityRenderProxy* pRenderProxy = (IEntityRenderProxy*)(pEntity->GetProxy(ENTITY_PROXY_RENDER));
		if (pRenderProxy != NULL)
		{
			pRenderProxy->UpdateCharactersBeforePhysics(m_isPlayer);
		}
	}

	m_pAnimTarget = NULL;
	m_pCharacter = NULL;
	m_pSkeletonAnim = NULL;
	m_pSkeletonPose = NULL;

	//entity pinter was laways 0
	//m_pCharacter = pEntity->GetCharacter(0);
}

void CAnimatedCharacter::SetOutput( const char * output, const char * value )
{
}

void CAnimatedCharacter::QueryComplete( TAnimationGraphQueryID queryID, bool succeeded )
{
	if (queryID == m_stanceQuery)
	{
		if (succeeded)
			m_currentStance = m_requestedStance;
		else
			CryLog("CAnimatedCharacter::QueryComplete: failed setting stance %d on %s", m_requestedStance, GetEntity()->GetName());
		m_requestedStance = -1;
		m_stanceQuery = 0;
	}
}

void CAnimatedCharacter::AddMovement( const SCharacterMoveRequest& request )
{
	if (request.type != eCMT_None)
	{
		assert(request.rotation.IsValid());
		assert(request.velocity.IsValid());

		CheckNANVec(Vec3(request.velocity),GetEntity());

		// we should have processed a move request before adding a new one
		m_moveRequest = request;
		m_moveRequestFrameID = gEnv->pRenderer->GetFrameID();

		assert(m_moveRequest.rotation.IsValid());
		assert(m_moveRequest.velocity.IsValid());
	}
	else
	{
		m_moveRequest.type = eCMT_None;
		m_moveRequest.velocity.zero();
		m_moveRequest.rotation.SetIdentity();
		m_moveRequest.prediction.nStates = 0;
		m_moveRequest.jumping = false;
		m_moveRequest.allowStrafe = false;
		m_moveRequest.proceduralLeaning = false;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CAnimatedCharacter::IsAnimGraphUpdateNeeded()
{
	return (!m_sleepAnimGraph || m_animationGraphStates.IsUpdateReallyNecessary());
}

//////////////////////////////////////////////////////////////////////////
void CAnimatedCharacter::ProcessEvent( SEntityEvent& event )
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);

	if (!m_pCharacter)
	{
		m_pCharacter = GetEntity()->GetCharacter(0);
		if (m_pCharacter)
		{
			// Turn off auto updating on this character,
			// AnimatedCharacter will be forcing Pre and Post update manually.
			m_pCharacter->SetFlags( m_pCharacter->GetFlags()&(~CS_FLAG_UPDATE) );
		}
	}

	switch (event.event)
	{
	case ENTITY_EVENT_PREPHYSICSUPDATE:
		{
			// TODO: OPT: These three will not change often (if ever), so they can be cached over many frames.
			// TODO: Though, make sure that they are refreshed if/when they actually DO change.
			IEntity* pEntity = GetEntity();
			m_pCharacter = pEntity->GetCharacter(0);
			if (m_pCharacter)
			{
				// Turn off auto updating on this character,
				// AnimatedCharacter will be forcing Pre and Post update manually.
				m_pCharacter->SetFlags( m_pCharacter->GetFlags()&(~CS_FLAG_UPDATE) );
			}

			m_pSkeletonAnim = (m_pCharacter != NULL)? m_pCharacter->GetISkeletonAnim() : NULL;
			m_pSkeletonPose = (m_pCharacter != NULL)? m_pCharacter->GetISkeletonPose() : NULL;

			UpdateSimpleMovementConditions();

			if (HasSplitUpdate())
				PrePhysicsUpdate();
		}
		break;
	case ENTITY_EVENT_XFORM:
		{
			int flags = event.nParam[0];

			if (!(flags & (ENTITY_XFORM_USER|ENTITY_XFORM_PHYSICS_STEP)))
			{
				IEntity* pEntity = GetEntity();
				if (pEntity != NULL)
				{
					QuatT entLocationTeleported = m_entLocation; // maybe don't use this local variable, but instead just m_entLocation, and maybe call UpdateCurEntLocation() here as well, just in case.

					// TODO: Optimize by not doing the merge of full QuatT's twice (once for each component).
					if (flags & ENTITY_XFORM_ROT /*&& flags & (ENTITY_XFORM_TRACKVIEW|ENTITY_XFORM_EDITOR)*/)
					{
						entLocationTeleported.q = pEntity->GetWorldRotation();
					}
					if (flags & ENTITY_XFORM_POS)
					{
						entLocationTeleported.t = pEntity->GetWorldPos();

						if (pEntity->GetParent() == NULL && m_entLocation.t != entLocationTeleported.t)
						{
							// forcing MCM's to eMCM_Entity only when teleported if not inside vehicle
							// (inside vehicles entity can only move by teleporting)
							EMovementControlMethod mcmh = GetMCMH();
							EMovementControlMethod mcmv = GetMCMV();
							bool forceEntity = false;
							if (mcmh == eMCM_Animation)
							{
								forceEntity = true;
								mcmh = eMCM_Entity;
							}
							if (mcmv == eMCM_Animation)
							{
								forceEntity = true;
								mcmv = eMCM_Entity;
							}
							if (forceEntity)
								SetMovementControlMethods(mcmh, mcmv);
						}
					}

					if (!m_bGrabbedInViewSpace)
					{
						// This is only for debugging, not used for anything.
						m_entTeleportMovement = ApplyWorldOffset(m_entTeleportMovement, GetWorldOffset(m_entLocation, entLocationTeleported));

						m_expectedEntMovement = GetWorldOffset(m_entLocation, entLocationTeleported);
						m_entLocation = entLocationTeleported;

						//float EntRotZ = fabs(RAD2DEG(m_entLocation.q.GetRotZ()));

						bool bTeleportAnimation = (flags & ENTITY_XFORM_EDITOR) || (flags & ENTITY_XFORM_TRACKVIEW) || (flags & ENTITY_XFORM_TIMEDEMO);
						bTeleportAnimation |= (m_entLocation.t.GetDistance(m_animLocation.t) > 2.0f);

						if (bTeleportAnimation)
							m_animLocation = m_entLocation;
						else
							m_animLocation = MergeMCM(m_entLocation, m_animLocation, m_animLocation, true);

						ClampAnimLocation();
					}
					else
					{
						//grabbed character
						m_entLocation = entLocationTeleported;
						m_animLocation = entLocationTeleported;
						if (m_pCharacter != NULL)
						{
							float fDistance = (GetISystem()->GetViewCamera().GetPosition() - m_animLocation.t).GetLength();
							float fZoomFactor = 0.001f + 0.999f*(RAD2DEG(GetISystem()->GetViewCamera().GetFov())/60.f);
							
							m_pCharacter->SkeletonPostProcess(m_entLocation, m_animLocation, NULL, fDistance * fZoomFactor, 0xBeef);
						}
					}

					DestroyExtraSolidCollider();
				}
			}
		}
		break;
	case ENTITY_EVENT_SCRIPT_REQUEST_COLLIDERMODE:
		{
			EColliderMode mode = (EColliderMode)event.nParam[0];
			RequestPhysicalColliderMode(mode, eColliderModeLayer_Script);
		}
		break;
	}
}

/* peterb: It was unused and caused a link error on Linux
ILINE static f32 GetYaw( const Vec3& v0, const Vec3& v1)
{
  float a0 = atan2f(v0.y, v0.x);
  float a1 = atan2f(v1.y, v1.x);
  return a1 - a0;
}
*/

static void AdjustWithCondition( float& adjustedValue, uint32 bitField, uint32 increaseBit, float time, float frameTime )
{
	float amount = 0.0f;
	if (frameTime > 1e-4f)
		amount = time/frameTime;
	if ((bitField & increaseBit) == 0)
		amount = -amount;
	adjustedValue = CLAMP(adjustedValue + amount, 0, 1);
}

class CAnimPathAdjuster
{
public:
	CAnimPathAdjuster() : m_targetTime(-1.0f) 
	{
		m_debugDraw = CAnimationGraphCVars::Get().m_showExactPositioningCorrectionPath != 0;
	}

	void SetTargetPos( const Vec3& pos, float radius ) { m_targetPos = pos; m_targetPosRadius = radius; }
	void SetTargetDir( const Vec3& dir, float radius ) { m_targetDir = dir; m_targetDirRadius = radius; }
	void SetFuturePath( ISkeletonAnim * pSkel ) { pSkel->GetFuturePath( m_relPath ); }
	void ForceReaching( float time ) { m_targetTime = time; }
	void SetFrameTime( float time ) { m_frameTime = time; }

	QuatT GetAdjustment()
	{
		IRenderAuxGeom * g = gEnv->pRenderer->GetIRenderAuxGeom();
 		g->SetRenderFlags( e_Def2DPublicRenderflags );

		QuatT out;
		out.t = ZERO;
		out.q = Quat::CreateIdentity();

		if (m_targetPos.GetLengthSquared() < 0.001f)
			return out;

		// figure out absolute path in actor local space
		RelToAbsPath( m_relPath, m_absPath );
		InitScale( m_absPath );
		DrawAbsPathFromTop( g, m_absPath, MAX_POINTS, ColorF(1,0,0,1) );

		// figure out which point we want to get to in the abs path
		int targetPoint = -1;
		float portionOfNext = 0.0f;
		if (m_targetTime < 0) // case: no timelimit
		{
			for (int i=1; i<MAX_POINTS; i++)
			{
				if (m_absPath[i].m_TransRot.t.GetLengthSquared() > m_targetPos.GetLengthSquared())
				{
					targetPoint = i-1;
					float dist0 = m_absPath[i-1].m_TransRot.t.GetLength();
					float dist1 = m_absPath[i].m_TransRot.t.GetLength();
					float distTgt = m_targetPos.GetLength();
					if (i == 1 && dist0 < m_targetPos.GetLength())
						portionOfNext = 1.0f;
					else
						portionOfNext = (distTgt - dist0) / (dist1 - dist0);
					break;
				}
			}
		}
		else if (m_targetTime < 1) // case: timelimit within MAX_POINTS
		{
			targetPoint = int(m_targetTime*10);
			if (targetPoint <= 8)
				portionOfNext = m_targetTime*10 - targetPoint;
			else
			{
				targetPoint = 8;
				portionOfNext = 1.0f;
			}
		}
		if (targetPoint < 0)
			return out;
		portionOfNext = CLAMP(portionOfNext, 0, 1);
		Vec3 tgt = LERP(m_absPath[targetPoint].m_TransRot.t, m_absPath[targetPoint+1].m_TransRot.t, portionOfNext);

		DrawMarker( g, tgt, ColorF(1,0,0,1) );

		float totalTime = (targetPoint + portionOfNext) * 0.1f;

		// figure out rotation necessary
		Vec3 endDirection = Quat::CreateSlerp( m_absPath[targetPoint].m_TransRot.q, m_absPath[targetPoint+1].m_TransRot.q, portionOfNext ).GetColumn1();
		float angle = cry_acosf(endDirection.Dot(m_targetDir));
		if (angle > m_targetDirRadius)
		{
			Quat totalRotationToExact = Quat::CreateRotationV0V1( endDirection, m_targetDir );
			Quat totalRotation = Quat::CreateSlerp( Quat::CreateIdentity(), totalRotationToExact, 1.0f - m_targetDirRadius/angle );
			Quat rotationPerSegment = Quat::CreateSlerp( Quat::CreateIdentity(), totalRotation, 0.1f/totalTime );
			out.q = Quat::CreateSlerp( Quat::CreateIdentity(), totalRotation, m_frameTime/totalTime );
			for (int i=0; i<=targetPoint; i++)
				m_relPath[i].m_TransRot.q *= rotationPerSegment;
			RelToAbsPath( m_relPath, m_absPath, targetPoint+2 );
			tgt = LERP(m_absPath[targetPoint].m_TransRot.t, m_absPath[targetPoint+1].m_TransRot.t, portionOfNext);
			DrawAbsPathFromTop( g, m_absPath, targetPoint+1, ColorF(1,1,0,1) );
			DrawMarker( g, tgt, ColorF(1,1,0,1) );
		}

		// now figure out translation necessary

		float fromPredictedToTargetLength;
		for (int i=0; i<5; i++)
		{
			Vec3 fromPredictedToTarget = tgt - m_targetPos;
			fromPredictedToTargetLength = fromPredictedToTarget.GetLength();
			if (fromPredictedToTargetLength > m_targetPosRadius)
			{
				fromPredictedToTarget *= (fromPredictedToTargetLength - m_targetPosRadius) / fromPredictedToTargetLength;

				Vec3 dx0 = PredictPoint(m_relPath, out.t + Vec3(-0.01f, 0, 0), portionOfNext, targetPoint+1);
				Vec3 dx1 = PredictPoint(m_relPath, out.t + Vec3(+0.01f, 0, 0), portionOfNext, targetPoint+1);
				Vec3 dy0 = PredictPoint(m_relPath, out.t + Vec3(0, -0.01f, 0), portionOfNext, targetPoint+1);
				Vec3 dy1 = PredictPoint(m_relPath, out.t + Vec3(0, +0.01f, 0), portionOfNext, targetPoint+1);
				Vec3 dz0 = PredictPoint(m_relPath, out.t + Vec3(0, 0, -0.01f), portionOfNext, targetPoint+1);
				Vec3 dz1 = PredictPoint(m_relPath, out.t + Vec3(0, 0, +0.01f), portionOfNext, targetPoint+1);

				Vec3 dx = dx0 - dx1;
				Vec3 dy = dy0 - dy1;
				Vec3 dz = dz0 - dz1;

				DrawMarker(g, tgt + dx, ColorF(0,0,1,1));
				DrawMarker(g, tgt + dy, ColorF(0,0,1,1));
				DrawMarker(g, tgt + dz, ColorF(0,0,1,1));

				Matrix33 m(dx, dy, dz);
				m.Invert();
				Vec3 weights = (m * fromPredictedToTarget) * 0.05f;

				Vec3 pred = PredictPoint(m_relPath, out.t + weights, portionOfNext, targetPoint+1);
				DrawMarker( g, pred, ColorF(1,1,1,1) );
				if ((pred-tgt).Dot(m_targetPos-tgt) < 0)
				{
					weights = -weights;
					pred = PredictPoint(m_relPath, out.t + weights, portionOfNext, targetPoint+1);
				}

				float scale = fromPredictedToTargetLength / (pred - tgt).GetLength();
				weights *= scale;
				pred = PredictPoint(m_relPath, out.t + weights, portionOfNext, targetPoint+1);
				DrawMarker( g, pred, ColorF(0,1,0,1) );

				if ((pred - m_targetPos).GetLength() > fromPredictedToTargetLength)
					break;

				out.t += weights;

				tgt = PredictPoint(m_relPath, out.t, portionOfNext, targetPoint+1);
			}
			else
				break;
		}
		DrawMarker( g, m_targetPos, ColorF(0,1,1,1) );
		DrawMeasure( g, tgt, m_targetPos, ColorF(0,1,1,1) );
		DrawCircle( g, m_targetPos, m_targetPosRadius, fromPredictedToTargetLength > m_targetPosRadius? ColorF(1,0,0,1) : ColorF(0,1,1,1) );

		out.t /= 0.1f;

		return out;
	}

private:
	static const int MAX_POINTS = ANIM_FUTURE_PATH_LOOKAHEAD;
	SAnimRoot m_relPath[MAX_POINTS];
	SAnimRoot m_absPath[MAX_POINTS];

	float m_frameTime;
	Vec3 m_targetPos;
	float m_targetPosRadius;
	Vec3 m_targetDir;
	float m_targetDirRadius;
	float m_targetTime;

	Vec3 m_localTargetPos;
	Vec3 m_localTargetDir;

	bool m_debugDraw;

	void RelToAbsPath( const SAnimRoot* relPath, SAnimRoot* absPath, int n = MAX_POINTS, const Vec3& startPt = ZERO, const Quat& startQ = Quat::CreateIdentity() )
	{
		SAnimRoot running;
		running.m_TransRot.t = startPt;
		running.m_TransRot.q = startQ;
		for (int i=0; i<n; i++)
		{
			running.m_TransRot.t += running.m_TransRot.q * relPath[i].m_TransRot.t;
			running.m_TransRot.q *= relPath[i].m_TransRot.q;
			absPath[i] = running;
		}
	}

	Vec3 PredictPoint( const SAnimRoot* relPath, Vec3 trans, float xtra, int n = MAX_POINTS, const Vec3& startPt = ZERO, const Quat& startQ = Quat::CreateIdentity() )
	{
		SAnimRoot running;
		running.m_TransRot.t = startPt;
		running.m_TransRot.q = startQ;
		for (int i=0; i<n; i++)
		{
			running.m_TransRot.t += running.m_TransRot.q * (relPath[i].m_TransRot.t + (i!=0)*trans);
			running.m_TransRot.q *= relPath[i].m_TransRot.q;
		}
		Vec3 final = running.m_TransRot.q * (relPath[n].m_TransRot.t + trans);
		return LERP(running.m_TransRot.t, final, xtra);
	}

	float m_scaleDraw;

	void InitScale( const SAnimRoot* absPath, int n = MAX_POINTS )
	{
		if (m_debugDraw)
		{
			Vec3 maxPt(ZERO);
			for (int i=0; i<n; i++)
			{
				for (int j=0; j<3; j++)
				{
					maxPt[j] = std::max(maxPt[j], fabsf(absPath[i].m_TransRot.t[j]));
				}
			}
			for (int j=0; j<3; j++)
			{
				maxPt[j] = std::max(maxPt[j], fabsf((m_targetPos * (1.0f+m_targetPosRadius))[j]));
			}

			float maxDim = 2.0f * std::max(maxPt.x, maxPt.y);
			if (maxDim < 1.0f)
				maxDim = 1.0f;
			// we want the maximum dimension to take up 70% of the screen
			m_scaleDraw = 0.7f / maxDim;
		}
	}
	Vec3 dpt( Vec3 x )
	{
		x.z = 0;
		x *= m_scaleDraw;
		x.x *= 0.75f;
		x += Vec3(0.5f, 0.5f, 0.0f);
		return x;
	}
	void DrawAbsPathFromTop( IRenderAuxGeom * g, const SAnimRoot* absPath, int n, const ColorF& clr )
	{
		if (m_debugDraw)
		{
			for (int i=1; i<n; i++)
				g->DrawLine( dpt(absPath[i-1].m_TransRot.t), clr, dpt(absPath[i].m_TransRot.t), clr, 2.0f );
		}
	}
	void DrawMarker( IRenderAuxGeom * g, Vec3 p, ColorF clr )
	{
		if (m_debugDraw)
		{
			p = dpt(p);
			Vec3 c = Vec3(0.0125f, 0.0166667f, 0.0f);
			g->DrawLine( p-c, clr, p+c, clr );
			c.y = -c.y;
			g->DrawLine( p-c, clr, p+c, clr );
		}
	}
	void DrawMeasure( IRenderAuxGeom * g, Vec3 p1, Vec3 p2, ColorF clr )
	{
		if (m_debugDraw)
		{
			float dist = (p1-p2).GetLength();
			p1 = dpt(p1);
			p2 = dpt(p2);
			g->DrawLine( p1, clr, p2, clr );
			Vec3 mid = (p1 + p2) * 0.5f;
			mid.x *= 800.0f;
			mid.y *= 600.0f;
			float clrA[] = {clr.r, clr.g, clr.b, clr.a};
			gEnv->pRenderer->Draw2dLabel( mid.x, mid.y, 1.5f, clrA, true, "%.2f", dist );
		}
	}
	void DrawCircle( IRenderAuxGeom * g, Vec3 c, float r, ColorF clr )
	{
		if (m_debugDraw)
		{
			c = dpt(c);
			r *= m_scaleDraw;

			const int n = 20;
			for (int i=0; i<n; i++)
			{
				float a0 = gf_PI2 * i / n;
				float a1 = gf_PI2 * (i+1) / n;

				Vec3 p0 = c+r*Vec3(0.75f*cosf(a0), sinf(a0), 0);
				Vec3 p1 = c+r*Vec3(0.75f*cosf(a1), sinf(a1), 0);
				g->DrawLine( p0, clr, p1, clr );
			}
		}
	}
};

bool CAnimatedCharacter::IsAnimationControlledView() const
{
	ICharacterInstance * pCharacter = GetEntity()->GetCharacter(0);
	float animationControlledView = 0.0f;

	if (pCharacter)
		animationControlledView = pCharacter->GetISkeletonAnim()->GetUserData(eAGUD_AnimationControlledView);

	return animationControlledView > 0.001f;
}

float CAnimatedCharacter::FilterView(SViewParams &viewParams) const
{

	ICharacterInstance * pCharacter = GetEntity()->GetCharacter(0);
	float animationControlledView = 0.0f;

	if (pCharacter)
		animationControlledView = pCharacter->GetISkeletonAnim()->GetUserData(eAGUD_AnimationControlledView);
	
	if (animationControlledView>0.001f)
	{
		viewParams.viewID = 1;
		viewParams.nearplane = 0.1f;

		ISkeletonPose *pSkeletonPose = pCharacter->GetISkeletonPose();
		
		//FIXME:keep IDs and such

		//view position, get the character position from the eyes and blend it with the game desired position
		int id_right = pSkeletonPose->GetJointIDByName("eye_right_bone");
		int id_left = pSkeletonPose->GetJointIDByName("eye_left_bone");
		if (id_right>-1 && id_left>-1)
		{
			Vec3 characterViewPos( pSkeletonPose->GetAbsJointByID(id_right).t );
			characterViewPos += pSkeletonPose->GetAbsJointByID(id_left).t;
			characterViewPos *= 0.5f;

			characterViewPos = GetEntity()->GetSlotWorldTM(0) * characterViewPos;

			viewParams.position = animationControlledView * characterViewPos + (1.0f-animationControlledView) * viewParams.position;
		}

		//and then, same with view rotation
		int id_head = pSkeletonPose->GetJointIDByName("Bip01 Head");
		if (id_head>-1)
		{
		//	Quat characterViewQuat(Quat(GetEntity()->GetSlotWorldTM(0)) * Quat(pSkeleton->GetAbsJMatrixByID(id_head)) * Quat::CreateRotationY(gf_PI*0.5f));
			Quat characterViewQuat(Quat(GetEntity()->GetSlotWorldTM(0)) * pSkeletonPose->GetAbsJointByID(id_head).q * Quat::CreateRotationY(gf_PI*0.5f));

			viewParams.rotation = Quat::CreateSlerp(viewParams.rotation,characterViewQuat,animationControlledView);
		}
	}

  return animationControlledView;
}

extern f32 g_YLine2;
uint32 g_LastSkiningFrameID = ~0;

void CAnimatedCharacter::SetParams( const SAnimatedCharacterParams& params )
{

	uint32 blendFlags = params.flags & (eACF_AlwaysAnimation|eACF_AlwaysPhysics|eACF_PerAnimGraph);
	if (!blendFlags)
		CryError("CAnimatedCharacter::SetParams: no movement blending flags set");
	else if (blendFlags & (blendFlags-1))
		CryError("CAnimatedCharacter::SetParams: too many movement blending flags set");


	m_params = params;
}

void CAnimatedCharacter::RequestStance( int stanceID, const char * name )
{
	//if (m_params.flags & eACF_ImmediateStance)
	{
		m_stanceQuery = 0;
		m_requestedStance = -1;
		m_currentStance = stanceID;
		m_animationGraphStates.SetInput( m_inputID[eACInputIndex_Stance], name );
		if(CAnimationGraphCVars::Get().m_debugErrors)
			m_debugStanceName = name;
	}
/*
	else
	{
		m_requestedStance = stanceID;
		m_animationGraphStates.SetInput( m_inputID[eACInputIndex_Stance], name, &m_stanceQuery );
	}
*/
}

int CAnimatedCharacter::GetCurrentStance()
{
	return m_currentStance;
}

bool CAnimatedCharacter::InStanceTransition()
{
	return m_requestedStance >= 0;
}

void CAnimatedCharacter::DestroyedState(IAnimationGraphState*)
{
	for ( int layer = 0; layer < eAnimationGraphLayer_COUNT; ++layer )
	{
		m_pAnimationState[layer] = 0;
		m_pGraph[layer] = 0;
	}
}


uint32 CAnimatedCharacter::MakeFace(const char*pExpressionName, bool usePreviewChannel, float lifeTime)
{
	uint32 channelId(-1);
		return channelId;
	ICharacterInstance * pCharacter = GetEntity()->GetCharacter(0);
	if (!pCharacter)
		return channelId;
	IFacialInstance * pFacialInstance = pCharacter->GetFacialInstance();
	if (!pFacialInstance)
		return channelId;
	IFacialModel * pFacialModel = pFacialInstance->GetFacialModel();
	if (!pFacialModel)
		return channelId;
	IFacialEffectorsLibrary * pLibrary = pFacialModel->GetLibrary();
	if (!pLibrary)
		return channelId;

	IFacialEffector *pEffector = NULL;
	if (pExpressionName)
	{
		pEffector = pLibrary->Find( pExpressionName );
		if (!pEffector)
		{
			//Timur, Ignore it for now.
			//GameWarning("%s: Unable to find facial expression: '%s'", data.pEntity->GetName(), m_name.c_str());
			return channelId;
		}
	}
	if(usePreviewChannel)
		pFacialInstance->PreviewEffector( pEffector, 1.0f );
	else
		channelId = pFacialInstance->StartEffectorChannel(pEffector, 1.f, .1f, lifeTime);
	return channelId;
}

void CAnimatedCharacter::AllowLookIk( bool allow )
{
	m_allowLookIk = allow;
}

void CAnimatedCharacter::TriggerRecoil(float duration, float distance)
{
	if (m_pSkeletonPose != NULL)
		m_pSkeletonPose->ApplyRecoilAnimation(duration, distance);
}

void CAnimatedCharacter::SetWeaponRaisedPose(EWeaponRaisedPose pose)
{
	if (pose == m_curWeaponRaisedPose)
		return;

	ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
	if (pCharacter == NULL)
		return;

	ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim();
	if (pSkeletonAnim == NULL)
		return;

	m_curWeaponRaisedPose = pose;

	if ((pose == eWeaponRaisedPose_None) || (pose == eWeaponRaisedPose_Fists))
	{
		pSkeletonAnim->StopAnimationInLayer(2, 0.5f);  // Stop weapon raising animation in Layer 2.
		return;
	}

	const char* anim = NULL;
	switch (pose)
	{
		case eWeaponRaisedPose_Pistol:			anim = "combat_idleAimBlockPoses_pistol_01"; break;
		case eWeaponRaisedPose_PistolLft:		anim = "combat_idleAimBlockPoses_dualpistol_left_01"; break;
		case eWeaponRaisedPose_PistolRgt:		anim = "combat_idleAimBlockPoses_dualpistol_right_01"; break;
		case eWeaponRaisedPose_PistolBoth:	anim = "combat_idleAimBlockPoses_dualpistol_01"; break;
		case eWeaponRaisedPose_Rifle:				anim = "combat_idleAimBlockPoses_rifle_01"; break;
		case eWeaponRaisedPose_Rocket:			anim = "combat_idleAimBlockPoses_rocket_01"; break;
		case eWeaponRaisedPose_MG:					anim = "combat_idleAimBlockPoses_mg_01"; break;
	}

	if (anim == NULL)
	{
		m_curWeaponRaisedPose = eWeaponRaisedPose_None;
		return;
	}

	// Start the weapon raising in Layer 2. This will automatically deactivate aim-poses.
	CryCharAnimationParams Params0(0);
	Params0.m_nLayerID = 2; 
	Params0.m_fLayerBlendIn = 0.5f;
	Params0.m_fTransTime = 0.5f;
	Params0.m_nFlags |= CA_LOOP_ANIMATION;
	Params0.m_nFlags |= CA_PARTIAL_SKELETON_UPDATE;
	pSkeletonAnim->StartAnimation(anim, 0, 0,0, Params0);
}

void CAnimatedCharacter::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	if (m_pAnimationState)
	{
		SIZER_COMPONENT_NAME(s, "AnimationGraph");
		m_animationGraphStates.GetMemoryStatistics(s);
	}

	{
		SIZER_COMPONENT_NAME(s, "DebugHistory");
		m_debugHistoryManager->GetMemoryStatistics(s);
	}
}
