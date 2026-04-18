
#include "StdAfx.h"
#include <ISystem.h>
#include <ICryAnimation.h>
#include <IActorSystem.h>
#include <IAnimationGraph.h>
#include <IAnimatedCharacter.h>
#include <GameObjects/GameObject.h>
#include "IAnimatedCharacter.h"

#include "FlowBaseNode.h"
#include "CryAction.h"
#include "IMovementController.h"

namespace {
	IAnimationGraphState* GetCurAGState( IEntity* pEntity )
	{
		if (!pEntity)
			return 0;
#if 1
		IActor *pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(pEntity->GetId());
		if (pActor)
			return pActor->GetAnimationGraphState();
#else
		// we could also go through
		// must be CGameObject (not IGameObject, would need more casts)
		CGameObject * pGameObject = (CGameObject*)pEntity->GetProxy(ENTITY_PROXY_USER);
		if (!pGameObject)
			return 0;
		IAnimatedCharacter * pAnimChar = (IAnimatedCharacter*) pGameObject->QueryExtension("AnimatedCharacter");
		if (pAnimChar)
			return pAnimChar->GetAnimationGraphState();
#endif
		return 0;
	}
};

class CPlayCGA_Node : public CFlowBaseNode
{
	IEntity *m_pEntity;
public:
	CPlayCGA_Node( SActivationInfo * pActInfo ) : m_pEntity(0)
	{
	};

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CPlayCGA_Node(pActInfo);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void Serialize(SActivationInfo *pActInfo, TSerialize ser)
	{
		// TODO: what to do on load?
		// sequence might have played, or should we replay from start or trigger done or ???
		// we set the entity to our best knowledge
		if (ser.IsReading())
			m_pEntity = pActInfo->pEntity;
	}

	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<string>( "CGA_File",_HELP("CGA Filename") ),
			InputPortConfig<string>( "anim_CGA_Animation",_HELP("CGA Animation name") ),
			InputPortConfig<bool>( "Trigger",_HELP("Starts the animation") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("Done", _HELP("Set to TRUE when animation is finished")),
			{0}
		};
		config.sDescription = _HELP( "Plays a CGA Animation" );
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event,SActivationInfo *pActInfo )
	{
		switch (event)
		{
			case eFE_Activate:
			{
						
				m_pEntity = pActInfo->pEntity;
				if(m_pEntity != NULL)
				{
					if(IsPortActive(pActInfo, 0))
					{
						m_pEntity->LoadCharacter(0, GetPortString(pActInfo, 0));
					}
					if (!IsPortActive(pActInfo,2)) break;
					ICharacterInstance* pCharacter = m_pEntity->GetCharacter(0);
					if(pCharacter != NULL)
					{
						CryCharAnimationParams params;
						pCharacter->GetISkeletonAnim()->StartAnimation(GetPortString(pActInfo,1),0,0,0, params);
						//pCharacter->SetFlags(pCharacter->GetFlags() | CS_FLAG_UPDATE_ALWAYS); doesn't seem to matter
						pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
						ActivateOutput(pActInfo, 0, false);
					}
					else CryLogAlways("PlayCGA - Get/LoadCharacter failed");
				}
				else CryLogAlways("PlayCGA - Invalid entity pointer");
				break;
			}

			case eFE_Initialize:
				m_pEntity = pActInfo->pEntity;
				if(m_pEntity)
				{
					m_pEntity->LoadCharacter(0, GetPortString(pActInfo, 0));
				}
				break;

			case eFE_Update:
			{
				if (m_pEntity != NULL) {
					ICharacterInstance* pCharacter = m_pEntity->GetCharacter(0);
					//CryLogAlways("Using deprecated AnimAnimation node in animation graph (entity %s)", data.pEntity->GetName());
					/*if(pCharacter->GetCurrentAnimation(0) == -1)
					{
						ActivateOutput(pActInfo, 0, true);
						pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
					}*/

					QuatT offset(m_pEntity->GetSlotLocalTM(0,false));
					QuatT renderLocation(m_pEntity->GetSlotWorldTM(0));
					QuatTS animLocation = renderLocation * offset; // TODO: This might be wrong.
					float fDistance = (GetISystem()->GetViewCamera().GetPosition() - animLocation.t).GetLength();
					pCharacter->SkeletonPreProcess(renderLocation, animLocation, GetISystem()->GetViewCamera(),0 );
					pCharacter->SkeletonPostProcess(renderLocation, animLocation, 0, fDistance, 0);
				}
				break;
			}
		};
	};
};

class CAnimationBoneInfo_Node : public CFlowBaseNode
{
	IEntity *m_pEntity;
public:
	CAnimationBoneInfo_Node( SActivationInfo * pActInfo ) : m_pEntity (0)
	{
	};

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CAnimationBoneInfo_Node(pActInfo);
	}

	virtual void Serialize(SActivationInfo *pActInfo, TSerialize ser)
	{
		if (ser.IsReading())
			OnChange(pActInfo);
	}

	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<string>( "bone_BoneName",_HELP("Name of Bone to get info for") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("LocalPos", _HELP("Position of bone in Local Space")),
			OutputPortConfig<Vec3>("LocalRot", _HELP("Rotation of bone in Local Space")),
			OutputPortConfig<Vec3>("WorldPos", _HELP("Position of bone in World Space")),
			OutputPortConfig<Vec3>("WorldRot", _HELP("Rotation of bone in World Space")),
			{0}
		};
		config.sDescription = _HELP( "Outputs information of the bone [BoneName] of the character of the attached entity" );
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_DEBUG);
	}


	void OnChange(SActivationInfo* pActInfo)
	{
		struct UpdateChanger
		{
			UpdateChanger(SActivationInfo *info, bool update) : info(info), update(update) {}
			~UpdateChanger() { info->pGraph->SetRegularlyUpdated(info->myID, update); }
			SActivationInfo *info;
			bool update;
		};

		UpdateChanger update(pActInfo, false);

		m_pEntity = pActInfo->pEntity;
		if (0 == m_pEntity)
			return;

		const string& boneName = GetPortString(pActInfo,0);
		if (boneName.empty())
			return;

		ICharacterInstance* pCharacter = m_pEntity->GetCharacter(0);
		if (pCharacter == 0)
			return;

		ISkeletonPose * pSkeletonPose = pCharacter->GetISkeletonPose();
		int16 boneID = pSkeletonPose->GetJointIDByName(boneName.c_str());
		if (boneID < 0)
		{
			CryLogAlways("[flow] Animations:BoneInfo: Cannot find bone '%s' in character 0 of entity '%s'", boneName.c_str(), m_pEntity->GetName());
		}
		update.update = boneID >= 0;
	}

	virtual void ProcessEvent( EFlowEvent event,SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			OnChange(pActInfo);
			break;

		case eFE_Initialize:
			OnChange(pActInfo);
			break;

		case eFE_Update:
			{
				if(!m_pEntity)
				{
					assert (m_pEntity != 0);
					return;
				}
				ICharacterInstance* pCharacter = m_pEntity->GetCharacter(0);
				if (pCharacter == 0)
					break;
				ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
				int16 boneID = pSkeletonPose->GetJointIDByName(GetPortString(pActInfo, 0).c_str());
				if (boneID < 0)
				{
					CryLogAlways("[flow] Animations:BoneInfo Cannot find bone '%s' in character 0 of entity '%s'", GetPortString(pActInfo, 0).c_str(), m_pEntity->GetName());
					break;
				}
			//	Matrix34 mat = pSkeleton->GetAbsJMatrixByID(boneID);
				Matrix34 mat = Matrix34(pSkeletonPose->GetAbsJointByID(boneID));
				ActivateOutput(pActInfo, 0, mat.GetTranslation());
				mat.OrthonormalizeFast();
				Ang3 angles = Ang3::GetAnglesXYZ(Matrix33(Quat(mat)));
				ActivateOutput(pActInfo, 1, Vec3(angles));

				Matrix34 matWorld = m_pEntity->GetSlotWorldTM(0) * mat;
				ActivateOutput(pActInfo, 2, matWorld.GetTranslation());
				matWorld.OrthonormalizeFast();
				Ang3 anglesWorld = Ang3::GetAnglesXYZ(Matrix33(Quat(matWorld)));
				ActivateOutput(pActInfo, 3, Vec3(anglesWorld));

			}
			break;
		}
	}
};

//////////////////////////////////////////////////////////////////////////
class CPlayAnimation_Node : public CFlowBaseNode
{
	ICharacterInstance *m_pCharacter;
	uint32 m_token;
	bool m_pausedAnimGraph;
	bool m_bForcedActivate;
	uint32 m_oldFlags;
	int m_playingState;
	float m_animTime;
	bool m_bFirstUpdate;
public:
	enum EInputs {
		IN_START,
		IN_STOP,
		IN_ANIMATION,
		IN_BLEND_TIME,
		IN_LOOP,
		IN_FORCE_UPDATE,
		IN_PAUSE_ANIM_GRAPH,
		IN_CONTROL_MOVEMENT
	};
	enum EOutputs {
		OUT_DONE,
		OUT_ALMOST_DONE,
	};
	enum EStates {
		STATE_INIT,
		STATE_PLAYING,
		STATE_STOPPED,
		STATE_FINISHED,
	};
	CPlayAnimation_Node( SActivationInfo * pActInfo )
	{
		m_pCharacter = 0;
		m_pausedAnimGraph = false;
		m_bForcedActivate = false;
		m_oldFlags = 0;
		m_token = 0;
		m_playingState = STATE_INIT;
		m_animTime = 0;
		m_bFirstUpdate=false;
	};
	~CPlayAnimation_Node()
	{
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CPlayAnimation_Node(pActInfo);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void Serialize(SActivationInfo *pActInfo, TSerialize ser)
	{
		ser.Value("m_token", m_token);
		ser.Value("m_pausedAnimGraph", m_pausedAnimGraph);
		ser.Value("m_bForcedActivate", m_bForcedActivate);
		ser.Value("m_oldFlags", m_oldFlags);
		ser.Value("m_animTime", m_animTime);

		//patch2 playing fix
		ser.Value("m_playingState", m_playingState); //writes 0 if not available on reading
		if(ser.IsReading() && m_playingState>=STATE_PLAYING)
			{
			pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
			m_bFirstUpdate=true;
		}
	}

	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void( "Start",_HELP("Starts the animation") ),
			InputPortConfig_Void( "Stop",_HELP("Stops the animation") ),
			InputPortConfig<string>( "anim_Animation",  _HELP("Animation name"), 0, _UICONFIG("ref_entity=entityId") ),
			InputPortConfig<float>( "BlendInTime",_HELP("Blend in time") ),
			InputPortConfig<bool>( "Loop",_HELP("When True animation will loop and will never stop") ),
			InputPortConfig<bool>( "ForceUpdate",false,_HELP("When True animation will play even if not visible") ),
			InputPortConfig<bool>( "PauseAnimGraph",false,_HELP("When True animation graph will not be involved in animation until this animation ends") ),
			InputPortConfig<bool>( "ControlMovement",false,_HELP("When True this animation will control the entities movement") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Done", _HELP("Send an event when animation is finished") ),
			OutputPortConfig_Void("AlmostDone", _HELP("Send an event when animation is almost finished") ),
			{0}
		};
		config.sDescription = _HELP( "Plays an Animation" );
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event,SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (pActInfo->pEntity)
				{
					if(IsPortActive(pActInfo, IN_START))
					{
						StartAnimation(pActInfo);
					}
					else if (IsPortActive(pActInfo, IN_STOP))
					{
						StopAnimation(pActInfo);
					}
				}
				break;
			}

		case eFE_Initialize:
			break;

		case eFE_Update:
			{
				if(m_bFirstUpdate)
				{
					switch(m_playingState)
					{
						case STATE_PLAYING:
							StartAnimation(pActInfo, true, m_animTime);
							break;
						case STATE_STOPPED:
							StopAnimation(pActInfo);
							break;
						case STATE_FINISHED:
							StartAnimation(pActInfo, true, 1.f);
							break;
					}
					m_bFirstUpdate=false;
				}
				
				bool tokenFound = false;
				bool almostDone = false;
				
				ICharacterInstance* pCharacter = pActInfo->pEntity ? pActInfo->pEntity->GetCharacter(0) : NULL;
				if (pCharacter)
				{
					ISkeletonAnim* pSkel = pCharacter->GetISkeletonAnim();
					for (int i=0; i<pSkel->GetNumAnimsInFIFO(0); i++)
					{
						if (pSkel->GetAnimFromFIFO(0, i).m_AnimParams.m_nUserToken == m_token)
						{
							tokenFound = true;
							m_animTime = pSkel->GetAnimFromFIFO(0,i).m_fAnimTime;
							almostDone = m_animTime > 0.85f;
						}
					}
				}
				if (tokenFound && m_pausedAnimGraph)
					if (IAnimationGraphState * pState = GetCurAGState(pActInfo->pEntity))
						pState->Pause( true, eAGP_PlayAnimationNode );
				if (almostDone)
					ActivateOutput( pActInfo, OUT_ALMOST_DONE, SFlowSystemVoid() );
				else if (!tokenFound)
				{
					ActivateOutput( pActInfo, OUT_DONE, SFlowSystemVoid() );
					m_playingState = STATE_FINISHED;

					pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
					if (m_pausedAnimGraph && pActInfo->pEntity)
						if (IAnimationGraphState * pState = GetCurAGState(pActInfo->pEntity))
							pState->Pause( false, eAGP_PlayAnimationNode );
					m_pausedAnimGraph = false;

					if (m_bForcedActivate && pActInfo->pEntity)
					{
						pActInfo->pEntity->Activate(false);
						m_bForcedActivate = false;
					}

					if (m_oldFlags)
						if (pActInfo->pEntity)
							if (IGameObject * pGameObject = CCryAction::GetCryAction()->GetGameObject(pActInfo->pEntity->GetId()))
								if (IAnimatedCharacter * pAnimChar = static_cast<IAnimatedCharacter*>(pGameObject->QueryExtension("AnimatedCharacter")))
								{
									SAnimatedCharacterParams params = pAnimChar->GetParams();
									params.flags = m_oldFlags;
									pAnimChar->SetParams( params );
								}
				}
			}
			break;
		};
	};

	void StartAnimation(SActivationInfo *pActInfo, bool forceUpdate = false, float keyTime = -1.f)
	{
		ICharacterInstance* pCharacter = pActInfo->pEntity->GetCharacter(0);
		if (pCharacter)
		{
			CryCharAnimationParams aparams;
			aparams.m_fTransTime = GetPortFloat(pActInfo,IN_BLEND_TIME);
			if (GetPortBool(pActInfo,IN_LOOP))
				aparams.m_nFlags |= CA_LOOP_ANIMATION;
			m_bForcedActivate = false;
			if (GetPortBool(pActInfo,IN_FORCE_UPDATE) || forceUpdate)
			{
				aparams.m_nFlags |= CA_FORCE_SKELETON_UPDATE;
				if (pActInfo->pEntity->IsActive() == false)
				{
					m_bForcedActivate = true;
					pActInfo->pEntity->Activate(true); // maybe unforce update as well
				}
			}
			const string& animation = GetPortString(pActInfo,IN_ANIMATION);

			ISkeletonAnim* pISkeletonAnim = pCharacter->GetISkeletonAnim();

			aparams.m_nLayerID = 0;
			aparams.m_nUserToken = ChooseToken( pCharacter );
			if(keyTime>-1.f)
				aparams.m_fKeyTime=keyTime;
			const bool bStarted = pISkeletonAnim->StartAnimation( animation,0, 0,0, aparams );
			if (bStarted)
			{
				m_token = aparams.m_nUserToken;
				m_playingState = STATE_PLAYING;
			}

			pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );

			m_pausedAnimGraph = GetPortBool(pActInfo, IN_PAUSE_ANIM_GRAPH);
			if (m_pausedAnimGraph)
				if (IAnimationGraphState * pState = GetCurAGState(pActInfo->pEntity))
					pState->Pause( true, eAGP_PlayAnimationNode );

			if (!m_oldFlags)
				if (pActInfo->pEntity)
					if (GetPortBool(pActInfo, IN_CONTROL_MOVEMENT))
						if (IGameObject * pGameObject = CCryAction::GetCryAction()->GetGameObject(pActInfo->pEntity->GetId()))
							if (IAnimatedCharacter * pAnimChar = static_cast<IAnimatedCharacter*>(pGameObject->QueryExtension("AnimatedCharacter")))
							{
								SAnimatedCharacterParams params = pAnimChar->GetParams();
								m_oldFlags = params.flags;
								params.flags &= ~(eACF_AlwaysAnimation | eACF_AlwaysPhysics | eACF_PerAnimGraph);
								params.flags |= eACF_AlwaysAnimation;
								pAnimChar->SetParams( params );
							}
		}
	}

	void StopAnimation(SActivationInfo *pActInfo)
	{
		m_playingState = STATE_STOPPED;

		if (m_pausedAnimGraph)
		{
			if (IAnimationGraphState * pState = GetCurAGState(pActInfo->pEntity))
				pState->Pause( false, eAGP_PlayAnimationNode );
			m_pausedAnimGraph = false;
		}
		else if (pActInfo->pEntity)
		{
			if (m_bForcedActivate)
			{
				pActInfo->pEntity->Activate(false);
				m_bForcedActivate = false;
			}
			if (ICharacterInstance * pChar = pActInfo->pEntity->GetCharacter(0))
			{
				ISkeletonAnim* pSkel = pChar->GetISkeletonAnim();
				for (int i=0; i<pSkel->GetNumAnimsInFIFO(0); i++)
				{
					CAnimation& anim = pSkel->GetAnimFromFIFO(0, i);
					if (anim.m_AnimParams.m_nUserToken == m_token)
					{
						anim.m_bActivated = false; // so removal will always work, maybe add a new method to IS
						pSkel->RemoveAnimFromFIFO(0, i);
						break;
					}
				}
			}
		}
	}

private:
	uint32 ChooseToken( ICharacterInstance * pCharacter )
	{
		ISkeletonAnim* pSkel = pCharacter->GetISkeletonAnim();
		uint32 maxToken = 0;
		for (int i=0; i<pSkel->GetNumAnimsInFIFO(0); i++)
			maxToken = max( pSkel->GetAnimFromFIFO(0, i).m_AnimParams.m_nUserToken, maxToken );
		if (!maxToken) // choose a (hopefully safe default)
			maxToken = 0xffff0000;
		else
			maxToken += 10000;
		return maxToken;
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_LookAt : public CFlowBaseNode
{
	IEntity *m_pEntity;
public:
	enum EInputs {
		IN_START,
		IN_STOP,
		IN_FOV,
		IN_BLEND,
		IN_TARGET,
		IN_TARGET_POS,
		IN_PLAYER,
	};
	CFlowNode_LookAt( SActivationInfo * pActInfo ) : m_pEntity(0) {}
	~CFlowNode_LookAt() {};

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CFlowNode_LookAt(pActInfo);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void Serialize(SActivationInfo *pActInfo, TSerialize ser)
	{
		// TODO: what to do on load?
		// sequence might have played, or should we replay from start or trigger done or ???
		// we set the entity to our best knowledge
		if (ser.IsReading())
			m_pEntity = pActInfo->pEntity;
	}

	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType( "Start",_HELP("Starts Look at ") ),
			InputPortConfig_AnyType( "Stop",_HELP("Stops Look at") ),
			InputPortConfig<float>( "FieldOfView",90,_HELP("LookAt Field of view (Degrees)") ),
			InputPortConfig<float>( "Blending",1,_HELP("Blending with animation value") ),
			InputPortConfig<EntityId>( "Target",(EntityId)0,_HELP("Look at target entity") ),
			InputPortConfig<Vec3>( "TargetPos",Vec3(0,0,0),_HELP("Look at target position") ),
			InputPortConfig<bool>( "LookAtPlayer",false,_HELP("When true looks at player") ),
			{0}
		};
		config.sDescription = _HELP( "Activates character Look At IK" );
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_ADVANCED);
	}
	virtual void ProcessEvent( EFlowEvent event,SActivationInfo *pActInfo )
	{
		Vec3 vTarget(0,0,0);
		float fFov = 0;
		float fBlend = 0;
		ICharacterInstance * pCharacter = 0;
		if (pActInfo->pEntity)
		{
			pCharacter = pActInfo->pEntity->GetCharacter(0);
			if (pCharacter)
			{
				// Update look IK on the character.
				if (!GetPortBool(pActInfo,IN_PLAYER))
				{
					vTarget = GetPortVec3(pActInfo,IN_TARGET_POS);
					IEntity *pEntity = gEnv->pEntitySystem->GetEntity( (EntityId)GetPortEntityId(pActInfo,IN_TARGET) );
					if (pEntity)
						vTarget = pEntity->GetWorldPos();
				}
				else if (IActor * pActor = CCryAction::GetCryAction()->GetClientActor())
				{
					if (IMovementController * pMoveController = pActor->GetMovementController())
					{
						SMovementState movementState;
						pMoveController->GetMovementState(movementState);
						vTarget = movementState.eyePosition + GetPortVec3(pActInfo,IN_TARGET_POS);
					}
				}
				fFov = GetPortFloat(pActInfo,IN_FOV);
				fBlend = GetPortFloat(pActInfo,IN_BLEND);
			}
		}

		float lookIKBlends[5];
		lookIKBlends[0] = 0.00f * fBlend;
		lookIKBlends[1] = 0.00f * fBlend;
		lookIKBlends[2] = 0.00f * fBlend;
		lookIKBlends[3] = 0.10f * fBlend;
		lookIKBlends[4] = 0.60f * fBlend;

		switch (event)
		{
		case eFE_Update:
			if (pActInfo->pEntity && pCharacter)
			{
				pCharacter->GetISkeletonPose()->SetLookIK( 1,DEG2RAD(fFov),vTarget,0 );
			}
			break;

		case eFE_Activate:
			if (pActInfo->pEntity)
			{
				if(IsPortActive(pActInfo, IN_START))
				{
					pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
				}
				if(IsPortActive(pActInfo, IN_STOP))
				{
					pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
					if (pCharacter)
					{
						// Turns off look ik.
						pCharacter->GetISkeletonPose()->SetLookIK( 0,DEG2RAD(fFov),vTarget,0 );
					}
				}
			}
			break;

		case eFE_Initialize:
			break;
		};
	};
};

class CFlowNode_AGControl : public CFlowBaseNode
{
public:
	enum EInputs {
		IN_AG_PAUSE = 0
	};

	CFlowNode_AGControl( SActivationInfo * pActInfo )
	{
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<bool>("Pause", false, _HELP("Pauses AnimationGraph updates of the attached entity"), _HELP("Pause")),
			{0}
		};
		config.pInputPorts = inputs;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Controls the AnimationGraph of the attached entity");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, IN_AG_PAUSE))
			{
				if (pActInfo->pEntity)
				{
					IAnimationGraphState *pCurState = GetCurAGState(pActInfo->pEntity);
					if (pCurState)
					{
						pCurState->Pause(GetPortBool(pActInfo, IN_AG_PAUSE), eAGP_FlowGraph);
					}
				}
			}
			break;
		}
	}
};

class CFlowNode_AGSetInput : public CFlowBaseNode
{
public:
	CFlowNode_AGSetInput( SActivationInfo * pActInfo )
	{
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<SFlowSystemVoid>("In", _HELP("Causes the node to set its value"), _HELP("In")),
			InputPortConfig<string>("Input", "Signal", _HELP("Input"), _HELP("Input")),
			InputPortConfig<string>("Value", "none" _HELP("Value"), _HELP("Value")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Done", _HELP("Send an event when the input is set") ),
			{0}
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Controls the AnimationGraph of the attached entity");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
				if (pActInfo->pEntity)
				{
					IAnimationGraphState *pCurState = GetCurAGState(pActInfo->pEntity);
					if (pCurState)
					{
						pCurState->SetInput( GetPortString(pActInfo, 1).c_str(), GetPortString(pActInfo, 2).c_str() );
					}
				}
			}
			ActivateOutput( pActInfo, 0, SFlowSystemVoid() );
			break;
		}
	}
};

class CFlowNode_AGLockInput : public CFlowBaseNode
{
public:
	CFlowNode_AGLockInput( SActivationInfo * pActInfo )
	{
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<SFlowSystemVoid>("Lock", _HELP("Causes the node to set its value"), _HELP("Lock")),
			InputPortConfig<SFlowSystemVoid>("Unlock", _HELP("Causes the node to set its value"), _HELP("Unlock")),
			InputPortConfig<string>("Input", "InstantZDelta", _HELP("Input"), _HELP("Input")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Done", _HELP("Send an event when animation is finished") ),
			{0}
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Controls the AnimationGraph of the attached entity");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			int action = int(IsPortActive(pActInfo, 0)) - int(IsPortActive(pActInfo, 1));
			IAnimationGraphState * pState = GetCurAGState(pActInfo->pEntity);
			if(!pState)
			{
				assert(pState);
				return;
			}
			IAnimationGraphState::InputID id = pState->GetInputId( GetPortString(pActInfo, 2) );
			bool lock = false;
			switch (action)
			{
			case 0:
				break;
			case 1:
				lock = true;
			case -1:
				pState->LockInput( id, lock );
				break;
			default:
				assert(false);
			}
			ActivateOutput( pActInfo, 0, SFlowSystemVoid() );
			break;
		}
	}
};

class CFlowNode_AGOutput : public CFlowBaseNode, public IAnimationGraphStateListener
{
public:
	CFlowNode_AGOutput( EntityId id = 0 ) : m_currentEntity(0) 
	{
		SetEntity(id);
	}
	~CFlowNode_AGOutput() { SetEntity(0); }
	CFlowNode_AGOutput( const CFlowNode_AGOutput& output ) : m_currentEntity(0)
	{
		SetEntity(m_currentEntity);
	}

	virtual void Serialize(SActivationInfo *, TSerialize ser)
	{
		ser.Value("entity", this, &CFlowNode_AGOutput::GetEntity, &CFlowNode_AGOutput::SetEntity);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_SetEntityId:
			if(pActInfo->pEntity)
				SetEntity( pActInfo->pEntity->GetId() );
			break;
		}
	}

	void QueryComplete(TAnimationGraphQueryID queryID, bool succeeded) {}

	EntityId GetEntity() const { return m_currentEntity; }

private:
	EntityId m_currentEntity;

	void SetEntity( EntityId id )
	{
		IEntitySystem * pES = gEnv->pEntitySystem;
		if (IEntity * pCurEntity = pES->GetEntity(m_currentEntity))
			if (IAnimationGraphState * pState = GetCurAGState(pCurEntity))
				pState->RemoveListener(this);
		m_currentEntity = id;
		if (IEntity * pCurEntity = pES->GetEntity(m_currentEntity))
			if (IAnimationGraphState * pState = GetCurAGState(pCurEntity))
				pState->AddListener("agoutput", this);
	}
};

class CFlowNode_AGWatchOutput : public CFlowNode_AGOutput
{
public:
	CFlowNode_AGWatchOutput( SActivationInfo * pActInfo, EntityId id = 0 ) : CFlowNode_AGOutput(id), m_actInfo(*pActInfo)
	{
	}

	IFlowNodePtr Clone(SActivationInfo *pActInfo)
	{
		return new CFlowNode_AGWatchOutput(pActInfo, GetEntity());
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<string>("Name", _HELP("Name of the output"), _HELP("Name")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<string>("Value", _HELP("Output a string whenever the animation graph output is set") ),
			{0}
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Controls the AnimationGraph of the attached entity");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		CFlowNode_AGOutput::ProcessEvent(event, pActInfo);

		switch (event)
		{
		case eFE_Initialize:
			m_actInfo = *pActInfo;

			const char * value = "";
			if (IAnimationGraphState * pState = GetCurAGState(pActInfo->pEntity))
				value = pState->QueryOutput(GetPortString(pActInfo, 0));
			ActivateOutput(pActInfo, 0, string(value));
			break;
		}
	}

	void SetOutput(const char * output, const char * value)
	{
		if (m_actInfo.pGraph)
			if (GetPortString(&m_actInfo, 0) == output)
				ActivateOutput( &m_actInfo, 0, string(value) );
	}
	void DestroyedState(IAnimationGraphState*) {}

private:
	SActivationInfo m_actInfo;
};

class CFlowNode_AGCheckOutput : public CFlowNode_AGOutput
{
public:
	CFlowNode_AGCheckOutput( SActivationInfo * pActInfo, EntityId id = 0 ) : CFlowNode_AGOutput(id), m_actInfo(*pActInfo)
	{
	}

	IFlowNodePtr Clone(SActivationInfo *pActInfo)
	{
		return new CFlowNode_AGCheckOutput(pActInfo, GetEntity());
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<string>("Name", _HELP("Name of the output"), _HELP("Name")),
			InputPortConfig<string>("CheckValue", _HELP("Name of the output"), _HELP("CheckValue")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("OnEqual", _HELP("Output a string whenever the animation graph output is set") ),
			OutputPortConfig_AnyType("OnNotEqual", _HELP("Output a string whenever the animation graph output is set") ),
			OutputPortConfig<bool>("Equal", _HELP("Output a string whenever the animation graph output is set") ),
			{0}
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Controls the AnimationGraph of the attached entity");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		assert(pActInfo);
		CFlowNode_AGOutput::ProcessEvent(event, pActInfo);

		switch (event)
		{
		case eFE_Initialize:
			m_actInfo = *pActInfo;

		case eFE_Update:
			{
				const char * value = "";
				if (IAnimationGraphState * pState = GetCurAGState(pActInfo->pEntity))
					value = pState->QueryOutput(GetPortString(pActInfo, 0));
				bool same = GetPortString(pActInfo, 1) == value;
				ActivateOutput( pActInfo, !same, SFlowSystemVoid() );
				ActivateOutput( pActInfo, 2, same );
				pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
			}
			break;
		}
	}
	void DestroyedState(IAnimationGraphState*) {}

	void SetOutput(const char * output, const char * value)
	{
		if (m_actInfo.pGraph)
			m_actInfo.pGraph->SetRegularlyUpdated( m_actInfo.myID, true );
	}

private:
	SActivationInfo m_actInfo;
};

class CFlowNode_AGReset : public CFlowBaseNode
{
public:
	CFlowNode_AGReset( SActivationInfo * pActInfo )
	{
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<SFlowSystemVoid>("Reset", _HELP("Causes the animation graph to lose its state"), _HELP("Reset")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Done", _HELP("Send an event when animation is finished")),
			{0}
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Controls the AnimationGraph of the attached entity");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			IAnimationGraphState * pState = GetCurAGState(pActInfo->pEntity);
			if(!pState)
				return;
			pState->Reset();
			ActivateOutput( pActInfo, 0, SFlowSystemVoid() );
			break;
		}
	}
};

class CFlowNode_StopAnimation : public CFlowBaseNode
{
public:
	CFlowNode_StopAnimation( SActivationInfo * pActInfo )
	{
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<SFlowSystemVoid>("Stop", _HELP("Stops animation playing"), _HELP("Stop!")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Done", _HELP("Send an event when animation is finished")),
			{0}
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Controls the AnimationGraph of the attached entity");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			if (pActInfo->pEntity)
				if (ICharacterInstance * pCharacter = pActInfo->pEntity->GetCharacter(0))
					pCharacter->GetISkeletonAnim()->StopAnimationsAllLayers();
			ActivateOutput( pActInfo, 0, SFlowSystemVoid() );
			break;
		}
	}
};

class CFlowNode_NoAiming : public CFlowBaseNode
{
public:
	CFlowNode_NoAiming( SActivationInfo * pActInfo )
	{
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<SFlowSystemVoid>("Enable", _HELP("Stops animation playing"), _HELP("Dont Aim!")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Done", _HELP("Send an event when animation is finished")),
			{0}
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Controls the AnimationGraph of the attached entity");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			if (pActInfo->pEntity)
				if (CGameObject * pGameObject = (CGameObject*)pActInfo->pEntity->GetProxy(ENTITY_PROXY_USER))
					if (IMovementController * pMC = pGameObject->GetMovementController())
					{
						CMovementRequest mc;
						mc.SetNoAiming();
						pMC->RequestMovement(mc);
					}
			ActivateOutput( pActInfo, 0, SFlowSystemVoid() );
			break;
		}
	}
};

// TODO: post VS2 we need to make a general system for this
class CFlowNode_SynchronizeTwoAnimations : public CFlowBaseNode
{
public:
	CFlowNode_SynchronizeTwoAnimations( SActivationInfo * pActInfo )
	{
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<EntityId>("Entity1", _HELP("Entity1")),
			InputPortConfig<EntityId>("Entity2", _HELP("Entity2")),
			InputPortConfig<string>("anim_Animation1", _HELP("Animation1"), 0, _UICONFIG("ref_entity=Entity1") ),
			InputPortConfig<string>("anim_Animation2", _HELP("Animation2"), 0, _UICONFIG("ref_entity=Entity2") ),
			InputPortConfig<float>("ResyncTime", 0.2f, _HELP("ResyncTime")),
			InputPortConfig<float>("MaxPercentSpeedChange", 10, _HELP("MaxPercentSpeedChange")),
			{0}
		};
		config.pInputPorts = inputs;
		config.sDescription = _HELP("Synchronize two animations on two entities");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
			break;
		case eFE_Update:
			Update( pActInfo );
			break;
		}
	}

private:
	void Update( SActivationInfo * pActInfo )
	{
		IEntity * pEnt1 = gEnv->pEntitySystem->GetEntity( GetPortEntityId(pActInfo, 0) );
		IEntity * pEnt2 = gEnv->pEntitySystem->GetEntity( GetPortEntityId(pActInfo, 1) );
		if (!pEnt1 || !pEnt2)
			return;
		ICharacterInstance * pChar1 = pEnt1->GetCharacter(0);
		ICharacterInstance * pChar2 = pEnt2->GetCharacter(0);
		if (!pChar1 || !pChar2)
			return;
		ISkeletonAnim* pSkel1 = pChar1->GetISkeletonAnim();
		ISkeletonAnim* pSkel2 = pChar2->GetISkeletonAnim();
		if (pSkel1->GetNumAnimsInFIFO(0)==0 || pSkel2->GetNumAnimsInFIFO(0)==0)
			return;
		CAnimation * pAnim1 = &pSkel1->GetAnimFromFIFO(0,0);
		CAnimation * pAnim2 = &pSkel2->GetAnimFromFIFO(0,0);
		if (pAnim1->m_LMG0.m_nAnimID[0] != pChar1->GetIAnimationSet()->GetAnimIDByName( GetPortString(pActInfo, 2) ))
			return;
		if (pAnim2->m_LMG0.m_nAnimID[0] != pChar2->GetIAnimationSet()->GetAnimIDByName( GetPortString(pActInfo, 3) ))
			return;
		if (pAnim2->m_fAnimTime < pAnim1->m_fAnimTime)
			std::swap( pAnim1, pAnim2 );
		float tm1 = pAnim1->m_fAnimTime;
		float tm2 = pAnim2->m_fAnimTime;
		if (tm2 - 0.5f > tm1)
		{
			tm1 += 1.0f;
			std::swap( pAnim1, pAnim2 );
			std::swap( tm1, tm2 );
		}
		float catchupTime = GetPortFloat(pActInfo, 4);
		float gamma = (tm2 - tm1) / catchupTime;
		float pb2 = (- gamma + sqrtf(gamma * gamma + 4.0f)) / 2.0f;
		float pb1 = 1.0f / pb2;
		float maxPB = GetPortFloat(pActInfo, 5) / 100.0f + 1.0f;
		float minPB = 1.0f / maxPB;
		pb2 = CLAMP(pb2,minPB,maxPB);
		pb1 = CLAMP(pb1,minPB,maxPB);
		pAnim1->m_AnimParams.m_fPlaybackSpeed = pb1;
		pAnim2->m_AnimParams.m_fPlaybackSpeed = pb2;
	}
};

// TODO: post VS2 we use events for this
class CFlowNode_TriggerOnKeyTime : public CFlowBaseNode
{
public:
	CFlowNode_TriggerOnKeyTime( SActivationInfo * pActInfo, bool state = false ) : m_state(state)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<string>("anim_Animation", _HELP("Animation1")),
			InputPortConfig<float>("TriggerTime", 0.2f, _HELP("TriggerTime")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Trigger", _HELP("Send an event when animation is finished")),
			{0}
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Synchronize two animations on two entities");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
			break;
		case eFE_Update:
			Update( pActInfo );
			break;
		}
	}

	virtual IFlowNodePtr Clone(SActivationInfo * pInfo)
	{
		return new CFlowNode_TriggerOnKeyTime(pInfo, m_state);
	}

	virtual void Serialize(SActivationInfo *, TSerialize ser)
	{
		ser.Value("state", m_state);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

private:
	void Update( SActivationInfo * pActInfo )
	{
		if (!pActInfo->pEntity)
		{
			m_state = false;
			return;
		}
		ICharacterInstance * pChar = pActInfo->pEntity->GetCharacter(0);
		if (!pChar)
		{
			m_state = false;
			return;
		}
		ISkeletonAnim* pSkel = pChar->GetISkeletonAnim();
		if (pSkel->GetNumAnimsInFIFO(0) < 1)
		{
			m_state = false;
			return;
		}
		CAnimation& anim = pSkel->GetAnimFromFIFO(0,0);
		if (anim.m_LMG0.m_nAnimID[0] != pChar->GetIAnimationSet()->GetAnimIDByName( GetPortString(pActInfo, 0) ))
		{
			m_state = false;
			return;
		}
		float triggerTime = GetPortFloat(pActInfo, 1);
		if (anim.m_fAnimTime < triggerTime)
		{
			m_state = false;
		}
		else if (anim.m_fAnimTime > triggerTime + 0.5f)
		{
			m_state = false;
		}
		else if (!m_state)
		{
			ActivateOutput( pActInfo, 0, SFlowSystemVoid() );
			m_state = true;
		}
	}

	bool m_state;
};

class CFlowNode_AttachmentControl : public CFlowBaseNode
{
public:
	CFlowNode_AttachmentControl( SActivationInfo * pActInfo )
	{
	}

	enum INPUTS
	{
		EIP_Attachment = 0,
		EIP_Show,
		EIP_Hide
	};

	enum OUTPUTS
	{
		EOP_Shown = 0,
		EOP_Hidden
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<string>("Attachment", _HELP("Attachment"), 0, _UICONFIG("dt=attachment,ref_entity=entityId") ),
			InputPortConfig_Void("Show", _HELP("Show the attachment")),
			InputPortConfig_Void("Hide", _HELP("Hide the attachment")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Shown", _HELP("Triggered when Shown")),
			OutputPortConfig_Void("Hidden", _HELP("Triggered when Hidden")),
			{0}
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("[CUTSCENE ONLY] Show/Hide Character Attachments.");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event != eFE_Activate)
			return;

		if (pActInfo->pEntity == 0)
			return;

		ICharacterInstance* pChar = pActInfo->pEntity->GetCharacter(0);
		if (pChar == 0)
			return;

		IAttachmentManager* pAttMgr = pChar->GetIAttachmentManager();
		if (pAttMgr == 0)
			return;

		const string& attachment = GetPortString(pActInfo, EIP_Attachment);
		IAttachment* pAttachment = pAttMgr->GetInterfaceByName(attachment.c_str());
		if (pAttachment == 0)
			return;

		const bool bHide = IsPortActive(pActInfo, EIP_Hide);
		const bool bShow = IsPortActive(pActInfo, EIP_Show);
		if (bHide || bShow)
		{
			pAttachment->HideAttachment(bHide);
			ActivateOutput(pActInfo, bHide ? EOP_Hidden : EOP_Shown, true);
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

};


REGISTER_FLOW_NODE( "Animations:PlayCGA", CPlayCGA_Node );
REGISTER_FLOW_NODE( "Animations:BoneInfo", CAnimationBoneInfo_Node );
REGISTER_FLOW_NODE( "Animations:PlayAnimation", CPlayAnimation_Node );
REGISTER_FLOW_NODE( "Animations:LookAt", CFlowNode_LookAt );
REGISTER_FLOW_NODE_SINGLETON( "Animations:AnimGraphControl", CFlowNode_AGControl );
REGISTER_FLOW_NODE_SINGLETON( "Animations:AnimGraphSetInput", CFlowNode_AGSetInput );
REGISTER_FLOW_NODE_SINGLETON( "Animations:AnimGraphLockInput", CFlowNode_AGLockInput );
REGISTER_FLOW_NODE( "Animations:AnimGraphWatchOutput", CFlowNode_AGWatchOutput );
REGISTER_FLOW_NODE( "Animations:AnimGraphCheckOutput", CFlowNode_AGCheckOutput );
REGISTER_FLOW_NODE_SINGLETON( "Animations:AnimGraphReset", CFlowNode_AGReset );
REGISTER_FLOW_NODE_SINGLETON( "Animations:StopAnimation", CFlowNode_StopAnimation );
REGISTER_FLOW_NODE_SINGLETON( "Animations:NoAiming", CFlowNode_NoAiming );
REGISTER_FLOW_NODE( "Animations:SynchronizeTwoAnimations", CFlowNode_SynchronizeTwoAnimations );
REGISTER_FLOW_NODE( "Animations:TriggerOnKeyTime", CFlowNode_TriggerOnKeyTime );
REGISTER_FLOW_NODE_SINGLETON( "Animations:AttachmentControl", CFlowNode_AttachmentControl );
