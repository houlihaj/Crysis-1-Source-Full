#include "StdAfx.h"
#include "AGAnimation.h"
#include "AnimationGraphState.h"

#define LAYER_NAMES LAYER_NAMES_AGANIM

static const char * LAYER_NAMES[] = {
	"AnimationLayer1",
	"AnimationLayer2",
	"AnimationLayer3",
	"AnimationLayer4",
	"AnimationLayer5",
	"AnimationLayer6",
	"AnimationLayer7",
	"AnimationLayer8",
	"AnimationLayer9",
};

void CAGAnimation::EnterState( SAnimationStateData& data, bool dueToRollback )
{
	ICharacterInstance* pICharacter = data.pEntity->GetCharacter(0);
	if (!pICharacter)
	{
		GameWarning("Entity '%s' of class '%s' has no character attached", data.pEntity->GetName(), data.pEntity->GetClass()->GetName());
		return;
	}
	ISkeletonAnim* pISkeletonAnim = pICharacter->GetISkeletonAnim();
	IAnimationSet* pAnimSet = pICharacter->GetIAnimationSet();


	if ( !gEnv->bClient )
	{
		data.hurried = false;
	}
	else if ( m_animations[0].empty() )
	{
		if ( m_layer > 0 )
		{
			data.hurried = false;
			int nAnimsInFIFO = pISkeletonAnim->GetNumAnimsInFIFO( m_layer );
			if ( nAnimsInFIFO > 0 )
			{
				CAnimation& anim = pISkeletonAnim->GetAnimFromFIFO( m_layer, nAnimsInFIFO-1 );
				if ( anim.m_AnimParams.m_nFlags & CA_LOOP_ANIMATION )
					pISkeletonAnim->StopAnimationInLayer( m_layer, 0.3f );
			}
		}
	}
	else
	{
		string animation0 = data.pState->ExpandVariationInputs( m_animations[0].c_str() );
		string animation1 = data.pState->ExpandVariationInputs( m_animations[1].c_str() );
		string aimPose0 = data.pState->ExpandVariationInputs( m_aimPoses[0].c_str() );
		string aimPose1 = data.pState->ExpandVariationInputs( m_aimPoses[1].c_str() );

		int id0 = pAnimSet->GetAnimIDByName(animation0.c_str());
		if (id0 < 0)
			GameWarning("Entity %s:cannot find animation %s", data.pEntity->GetName(),animation0.c_str());
		int id1 = -1;
		if (!animation1.empty())
			if ((id1 = pAnimSet->GetAnimIDByName(animation1.c_str())) < 0) 
				GameWarning("Entity %s:cannot find animation %s", data.pEntity->GetName(),animation1.c_str());

		CryCharAnimationParams params = GetAnimationParams(data);
		m_animationLength = fabsf( pAnimSet->GetDuration_sec(id0) / params.m_fPlaybackSpeed );

		CryCharAnimationParams backParams;
		if ( int numAnims = pISkeletonAnim->GetNumAnimsInFIFO(m_layer) )
		{
			backParams = pISkeletonAnim->GetAnimFromFIFO(m_layer, numAnims-1).m_AnimParams;
			if (backParams.m_nFlags & CA_LOOP_ANIMATION)
			{
				if (params.m_nFlags & CA_START_AFTER)
				{
					//GameWarning("Trying to start an animation after a looping animation; ignoring START_AFTER flag");
					params.m_nFlags &= ~CA_START_AFTER;
				}
			}
		}

		if (dueToRollback)
		{
			if (params.m_nFlags & CA_REPEAT_LAST_KEY)
				return;

			// this animation has already started fading out
			// we need to add it to the queue again (modified flags should help)
			params.m_nFlags |= CA_ALLOW_ANIM_RESTART;
			params.m_nFlags |= CA_TRANSITION_TIMEWARPING;
			params.m_nFlags &= ~CA_START_AFTER;
			params.m_nFlags &= ~CA_START_AT_KEYTIME;
			params.m_fTransTime = 0.01f;
		}

		if (data.hurried)
		{
			data.hurried = false;
			params.m_nFlags &= ~(CA_START_AT_KEYTIME | CA_START_AFTER);
			params.m_fTransTime = 0.2f;
		}

		if (m_stopCurrentAnimation)
			pISkeletonAnim->StopAnimationInLayer(params.m_nLayerID,0.0f);

		int checkSize = pISkeletonAnim->GetNumAnimsInFIFO(m_layer);
		bool result = pISkeletonAnim->StartAnimation( 
			animation0.c_str(), 
			animation1.empty()? 0 : animation1.c_str(),
			aimPose0.empty()? 0 : aimPose0.c_str(),
			aimPose1.empty()? 0 : aimPose1.c_str(), 
			params );
		if ( result )
		{
			assert( (checkSize + 1) == pISkeletonAnim->GetNumAnimsInFIFO(m_layer) );
		}
		else
		{
			backParams.m_nUserToken = params.m_nUserToken;
		}
	}
}

CryCharAnimationParams CAGAnimation::GetAnimationParams( SAnimationStateData& data )
{
	CryCharAnimationParams params = data.params[m_layer];
	if ( data.overrideTransitionTime != 0.0f )
		params.m_fTransTime = data.overrideTransitionTime;

	if (data.overrides[m_layer].overrideStartAfter)
		params.m_nFlags |= CA_START_AFTER;
	if (data.overrides[m_layer].overrideStartAtKeyFrame)
	{
		params.m_nFlags |= CA_START_AT_KEYTIME;
		params.m_fKeyTime = data.overrides[m_layer].startAtKeyFrame;
	}

	// clear overrides after consuming
	data.overrides[m_layer].ClearOverrides();

	if (m_oneShot)
		params.m_nFlags &= ~CA_LOOP_ANIMATION;

	params.m_nLayerID = m_layer;
//	params.m_nCopyTimeFromLayer = -1;
	params.m_nUserToken = data.pState->GetCurrentToken();

	params.m_fPlaybackSpeed = data.pGameObject->GetChannelId() != 0 /*CCryAction::GetCryAction()->IsMultiplayer()*/
		? m_MPSpeedMultiplier : m_SPSpeedMultiplier;
	params.m_fUserData[eAGUD_AnimationControlledView] = data.animationControlledView? 1.0f : 0.0f;
	params.m_fUserData[eAGUD_MovementControlMethodH] = data.MovementControlMethodH;
	params.m_fUserData[eAGUD_MovementControlMethodV] = data.MovementControlMethodV;
	params.m_fUserData[eAGUD_AdditionalTurnMultiplier] = data.additionalTurnMultiplier;

	for (size_t i=eAGUD_NUM_BUILTINS; i<NUM_ANIMATION_USER_DATA_SLOTS; i++)
		params.m_fUserData[i] = data.userData[i];

//	params.nFlags |= 

	if (m_interruptCurrentAnim)
		params.m_nFlags &= ~(CA_START_AFTER | CA_START_AT_KEYTIME);

	if ( (m_layer == 0) && (data.canMix == false) )
		params.m_nFlags |= CA_DISABLE_MULTILAYER;

	if ( data.pState->IsUsingTriggeredTransition() )
		params.m_nFlags |= CA_FULL_ROOT_PRIORITY;

	return params;
}

EHasEnteredState CAGAnimation::HasEnteredState( SAnimationStateData& data )
{
	if (m_animations[0].empty())
	{
		if (m_layer!=0)
		{
			if (ICharacterInstance * pChar = data.pEntity->GetCharacter(0))
			{
				ISkeletonAnim* pSkel = pChar->GetISkeletonAnim();
				int nAnimsInFIFO = pSkel->GetNumAnimsInFIFO(m_layer);
				return nAnimsInFIFO ? eHES_Waiting : eHES_Entered;
			}
		}
		return eHES_Instant;
	}

	if (!gEnv->bClient)
		return eHES_Entered;

	if (ICharacterInstance * pChar = data.pEntity->GetCharacter(0))
	{
		ISkeletonAnim* pSkel = pChar->GetISkeletonAnim();
		int nAnimsInFIFO = pSkel->GetNumAnimsInFIFO(m_layer);
		for (int i=0; i<nAnimsInFIFO; i++)
		{
			CAnimation& anim = pSkel->GetAnimFromFIFO(m_layer, i);
			if (anim.m_AnimParams.m_nUserToken == data.pState->GetCurrentToken())      
      {
        // hacky: allow hidden characters to enter their state [MR/Craig]
        if (anim.m_bActivated || (data.pEntity->IsHidden() && data.pEntity->GetFlags()&ENTITY_FLAG_UPDATE_HIDDEN) || !(data.pEntity->GetSlotFlags(0)&ENTITY_SLOT_RENDER))
          return eHES_Entered;
        else
				  return eHES_Waiting;      
      }
		}
	}
	return eHES_Instant;
}

bool CAGAnimation::CanLeaveState( SAnimationStateData& data )
{
	if (m_animations[0].empty())
		return true;

	bool found = false;
	bool activated = IsActivated( data, &found );

	if (found && activated && m_stayInStateUntil > 0.001f)
	{
		ICharacterInstance * pChar = data.pEntity->GetCharacter(0);
		ISkeletonAnim* pSkel = pChar->GetISkeletonAnim();
		int nAnimsInFIFO = pSkel->GetNumAnimsInFIFO(m_layer);
		for (int i=0; i<nAnimsInFIFO; i++)
		{
			CAnimation& anim = pSkel->GetAnimFromFIFO(m_layer, i);
			if (anim.m_AnimParams.m_nUserToken == data.pState->GetCurrentToken())
			{
				if (anim.m_fAnimTime < m_forceInStateUntil)
					return false;
				if ((!data.queryChanged) && (anim.m_fAnimTime < m_stayInStateUntil))
					return false;
				break;
			}
		}
	}

	if (found && m_ensureInStack && activated)
	{
		if (!IsTopOfStack(data))
			return false;
	}

	if (found)
		return activated;

	return true;
}

bool CAGAnimation::IsActivated(const SAnimationStateData& data, bool * pFound /* = NULL  */)
{
	bool activated = false;
	bool found = false;

	if (ICharacterInstance * pChar = data.pEntity->GetCharacter(0))
	{
		ISkeletonAnim* pSkel = pChar->GetISkeletonAnim();
		int nAnimsInFIFO = pSkel->GetNumAnimsInFIFO(m_layer);
		for (int i=0; i<nAnimsInFIFO; i++)
		{
			CAnimation& anim = pSkel->GetAnimFromFIFO(m_layer, i);
			if (anim.m_AnimParams.m_nUserToken == data.pState->GetCurrentToken())
			{
				found = true;

        // hacky: allow activation for hidden characters [MR/Craig]
        activated = anim.m_bActivated || (data.pEntity->IsHidden() && data.pEntity->GetFlags()&ENTITY_FLAG_UPDATE_HIDDEN) || !(data.pEntity->GetSlotFlags(0)&ENTITY_SLOT_RENDER);
        break;
			}
		}
	}

	if (pFound)
		*pFound = found;
	return activated;
}

void CAGAnimation::LeaveState( SAnimationStateData& data )
{
	if (m_animations[0].empty())
		return;

	if (ICharacterInstance * pChar = data.pEntity->GetCharacter(0))
	{
		ISkeletonAnim* pSkel = pChar->GetISkeletonAnim();
		int nAnimsInFIFO = pSkel->GetNumAnimsInFIFO(m_layer);
		for (int i=0; i<nAnimsInFIFO; i++)
		{
			CAnimation& anim = pSkel->GetAnimFromFIFO(m_layer, i);
			if (anim.m_AnimParams.m_nUserToken == data.pState->GetCurrentToken())
			{
				if (!anim.m_bActivated)
					pSkel->RemoveAnimFromFIFO(m_layer, i);
				else if (anim.m_fAnimTime >= m_forceInStateUntil && anim.m_fAnimTime < m_stayInStateUntil)
					data.overrides[m_layer].ClearOverrides();
			}
		}
	}

	if (m_forceLeaveWhenFinished)
		data.pState->InvalidateQueriedState();
}

void CAGAnimation::DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement )
{
	float white[] = {1,1,1,1};
	pRenderer->Draw2dLabel( x, y, 1, white, false, "anim: %s", m_animations[0].c_str() );
	y += yIncrement;
	if (m_ensureInStack)
	{
		pRenderer->Draw2dLabel( x, y, 1, white, false, "ensure top of stack: %d", IsTopOfStack(data) );
		y += yIncrement;
	}
	if (m_forceLeaveWhenFinished)
	{
		pRenderer->Draw2dLabel( x, y, 1, white, false, "force leave state when finished" );
		y += yIncrement;
	}
	if (m_forceInStateUntil < m_stayInStateUntil)
	{
		pRenderer->Draw2dLabel( x, y, 1, white, false, "stay in state until: %f (%f forced)", m_stayInStateUntil, m_forceInStateUntil );
		y += yIncrement;
	}
	else if (m_stayInStateUntil)
	{
		pRenderer->Draw2dLabel( x, y, 1, white, false, "stay in state until: %f", m_stayInStateUntil );
		y += yIncrement;
	}
	pRenderer->Draw2dLabel( x, y, 1, white, false, "is activated: %d", IsActivated(data) );
	y += yIncrement;
}

void CAGAnimation::Update( SAnimationStateData& data )
{
	assert(m_forceLeaveWhenFinished);

	if (m_forceLeaveWhenFinished && IsTopOfStack(data))
	{
		ICharacterInstance* pCharacter = data.pEntity->GetCharacter(0);
		if (pCharacter != NULL)
		{
			ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim();
			int nAnimsInFIFO = pSkeletonAnim->GetNumAnimsInFIFO(m_layer);
			for (int i = 0; i < nAnimsInFIFO; i++)
			{
				CAnimation& anim = pSkeletonAnim->GetAnimFromFIFO(m_layer, i);
				if (anim.m_AnimParams.m_nUserToken == data.pState->GetCurrentToken())
				{
					static float waitUntilTime = 0.99f;
					if (anim.m_fAnimTime >= waitUntilTime)
					{
						data.pState->ForceLeaveCurrentState();
					}
				}
			}
		}
	}
}

bool CAGAnimation::IsTopOfStack( const SAnimationStateData& data )
{
	ICharacterInstance * pCharacter = data.pEntity->GetCharacter(0);
	if (!pCharacter)
		return true;
	IAnimationSet* pIAnimationSet = pCharacter->GetIAnimationSet();
	ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim();

	int nAnimsCurrently = pSkeletonAnim->GetNumAnimsInFIFO(m_layer);
	switch (nAnimsCurrently)
	{
	case 0:
		// no animation playing, better get one
		return true;
	case 1:
		{
			// this is the last animation in the queue - must be on top of the stack
			return true;
			/*
			CAnimation& animation = pSkeletonAnim->GetAnimFromFIFO(m_layer,0);
			int32 id = (animation.m_LMG0.m_nLMGID >= 0) ? animation.m_LMG0.m_nLMGID : animation.m_LMG0.m_nAnimID[0];

			int32 id0 = pIAnimationSet->GetAnimIDByName( m_animations[0].c_str() );
			int32 id1 = pIAnimationSet->GetAnimIDByName( m_animations[1].c_str() );

			return (id == id0) || (id == id1);
			*/
		}
		break;
	default:
		// > 1 animation playing... wait
		return false;
	}
}

void CAGAnimation::GetCompletionTimes( SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky )
{
	hard = 0.0f;
	if (m_stickyOutTime > 0)
		sticky = start + m_animationLength - m_stickyOutTime;
	else
		sticky = 0.0f;
}

const IAnimationStateNodeFactory::Params * CAGAnimation::GetParameters()
{
	static const Params params[] = 
	{
		{true,  "string", "animation1",    "Animation 1",                                      "" },
		{true,  "string", "animation2",    "Animation 2",                                      "" },
		{true,  "string", "aimPose",    "AimPose 1",                                      "" },
		{true,  "string", "aimPose2",    "AimPose 2",                                      "" },
		{true,  "bool",   "ensureInStack", "Wait for animation to be playing?",                "0"},
		{true,  "float",  "stickyOutTime", "Sticky out time (or <0 for non-sticky playback)",  "-1"},
		{true,  "bool",   "forceEnableAiming", "Force enable aiming and usage of aimposes",  "0"},
		{true,  "bool",   "forceLeaveWhenFinished", "Force leaving this state when finished",  "0"},
		{true,  "float",  "speedMultiplier", "Speed multiplier (increases playback speed on this animation)", "1"},
		{true,  "float",  "MPSpeedMultiplier", "Multiplayer speed multiplier (increases playback speed on this animation in multiplayer)", "1"},
		{true,  "bool",   "stopCurrentAnimation", "Stop the current animation?",                "0"},
		{true,  "bool",   "interruptCurrAnim", "Interrupt current animation?",                "0"},
		{0}
	};
	return params;
}

bool CAGAnimation::Init( const XmlNodeRef& node, IAnimationGraphPtr pGraph )
{
	bool ok = true;
	ok &= node->getAttr("ensureInStack", m_ensureInStack);
	ok &= node->getAttr("stickyOutTime", m_stickyOutTime);
	if (!node->getAttr("forceEnableAiming", m_forceEnableAiming))
		m_forceEnableAiming = false;
	ok &= node->getAttr("forceLeaveWhenFinished", m_forceLeaveWhenFinished);

	m_animations[0] = pGraph->RegisterVariationInputs( node->getAttr("animation1") );
	m_animations[1] = pGraph->RegisterVariationInputs( node->getAttr("animation2") );
	m_aimPoses[0] = pGraph->RegisterVariationInputs( node->getAttr("aimPose") );
	m_aimPoses[1] = pGraph->RegisterVariationInputs( node->getAttr("aimPose2") );
	if (m_aimPoses[0] == "none")
		m_aimPoses[0] = "";
	if (m_aimPoses[1] == "none")
		m_aimPoses[1] = "";

	m_SPSpeedMultiplier = 1.0f;
	if (node->getAttr("speedMultiplier")[0])
		node->getAttr("speedMultiplier", m_SPSpeedMultiplier);
	m_MPSpeedMultiplier = m_SPSpeedMultiplier;
	if (node->getAttr("MPSpeedMultiplier")[0])
		node->getAttr("MPSpeedMultiplier", m_MPSpeedMultiplier);
	m_stayInStateUntil = 0;
	node->getAttr("stayInStateUntil", m_stayInStateUntil);
	m_forceInStateUntil = m_stayInStateUntil;
	if (node->getAttr("forceInStateUntil")[0])
	{
		node->getAttr("forceInStateUntil", m_forceInStateUntil);
		if ( m_forceInStateUntil > m_stayInStateUntil )
			m_stayInStateUntil = m_forceInStateUntil;
	}

	m_stopCurrentAnimation = false;
	node->getAttr("stopCurrentAnimation", m_stopCurrentAnimation);

	m_interruptCurrentAnim = false;
	node->getAttr("interruptCurrAnim", m_interruptCurrentAnim);

	m_directToLayer0 = false;
	node->getAttr("directToLayer0", m_directToLayer0);

	if (m_forceLeaveWhenFinished)
		this->flags |= eASNF_Update;

	m_oneShot = false;
	node->getAttr("oneShot", m_oneShot);

	return ok;
}

SAnimationDesc CAGAnimation::GetDesc( IEntity * pEntity, const CAnimationGraphState* pState )
{
	assert(pEntity);
	SAnimationDesc desc;

	ICharacterInstance* pCharacter = pEntity->GetCharacter(0);
	if (pCharacter == NULL)
		return desc;

	IAnimationSet* pAnimSet = pCharacter->GetIAnimationSet();
	if (pAnimSet == NULL)
		return desc;

	string animation0 = pState == NULL ? m_animations[0].c_str() : pState->ExpandVariationInputs( m_animations[0].c_str() ).c_str();

	int id = pAnimSet->GetAnimIDByName(animation0.c_str());
	if (id < 0)
		return desc;

	uint32 flags = pAnimSet->GetAnimationFlags(id);
	if ((flags & CA_ASSET_LOADED) == 0)
		return desc;

	CryAnimationPath path = pAnimSet->GetAnimationPath(animation0.c_str());
	const QuatT& a = path.m_key0;
	const QuatT& b = path.m_key1;
	desc.movement.translation = b.t - a.t;
	desc.movement.rotation = a.q.GetInverted() * b.q;
	desc.movement.duration = pAnimSet->GetDuration_sec( id );
	desc.startLocation = pAnimSet->GetAnimationStartLocation(animation0.c_str());
	desc.properties = pAnimSet->GetAnimationSelectionProperties(animation0.c_str());
	desc.initialized = pState != NULL && animation0 == m_animations[0];
	return desc;
}

Vec2 CAGAnimation::GetMinMaxSpeed( IEntity * pEntity )
{
	if (ICharacterInstance * pInst = pEntity->GetCharacter(0))
	{
		int id = pInst->GetIAnimationSet()->GetAnimIDByName(m_animations[0].c_str());
		if (id >= 0)
		{
			return pInst->GetIAnimationSet()->GetMinMaxSpeedAsset_msec(id);
		}
	}
	return Vec2(0,0);
}

void CAGAnimation::Release()
{
	delete this;
}

IAnimationStateNode * CAGAnimation::Create()
{
	return this;
}

const char * CAGAnimation::GetCategory()
{
	return LAYER_NAMES[m_layer];
}

const char * CAGAnimation::GetName()
{
	return LAYER_NAMES[m_layer];
}

void CAGAnimation::SerializeAsFile(bool reading, AG_FILE *file)
{
	SerializeAsFile_NodeBase(reading, file);

	FileSerializationHelper h(reading, file);

	h.Value(&m_layer);
	h.Value(&m_ensureInStack);
	h.Value(&m_forceEnableAiming);
	h.Value(&m_forceLeaveWhenFinished);
	h.Value(&m_stopCurrentAnimation);
	h.Value(&m_interruptCurrentAnim);
	h.Value(&m_directToLayer0);
	h.Value(&m_stickyOutTime);
	h.Value(m_animations, 2);
	h.Value(m_aimPoses, 2);
	h.Value(&m_SPSpeedMultiplier);
	h.Value(&m_MPSpeedMultiplier);
	h.Value(&m_stayInStateUntil);
	h.Value(&m_forceInStateUntil);
	h.Value(&m_oneShot);
}

#undef LAYER_NAMES
