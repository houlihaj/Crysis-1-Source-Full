#include "StdAfx.h"
#include "AnimationGraphState.h"
#include "AGWaitForUBAllowed.h"

void CAGWaitForUBAllowed::EnterState( SAnimationStateData& data, bool dueToRollback )
{
	if ( ICharacterInstance* pChar = data.pEntity->GetCharacter(0) )
	{
		int layer = data.pState->GetLayerIndex();
		ISkeletonAnim* pSkel = pChar->GetISkeletonAnim();
		int nAnimsInFIFO = pSkel->GetNumAnimsInFIFO( layer );
		if ( nAnimsInFIFO > 0 )
		{
			CAnimation& anim = pSkel->GetAnimFromFIFO( layer, nAnimsInFIFO-1 );
			if ( anim.m_AnimParams.m_nFlags & CA_LOOP_ANIMATION )
				pSkel->StopAnimationInLayer( layer, 0.3f );
		}
	}
}

EHasEnteredState CAGWaitForUBAllowed::HasEnteredState( SAnimationStateData& data )
{
	if ( data.queryChanged )
		return eHES_Entered;

	if ( ICharacterInstance* pChar = data.pEntity->GetCharacter(0) )
	{
		ISkeletonAnim* pSkel = pChar->GetISkeletonAnim();
		int nAnimsInFIFO = pSkel->GetNumAnimsInFIFO( data.pState->GetLayerIndex() );
		return nAnimsInFIFO ? eHES_Waiting : eHES_Entered;
	}
	return eHES_Instant;
}

bool CAGWaitForUBAllowed::CanLeaveState( SAnimationStateData& data )
{
	if ( (!data.queryChanged) && (HasEnteredState(data) == eHES_Waiting) )
		return false;

	bool allowed = data.pState->DoesParentLayerAllowMixing();
	return allowed;
}

void CAGWaitForUBAllowed::LeaveState( SAnimationStateData& data )
{
}

void CAGWaitForUBAllowed::DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement )
{
	float white[] = {1,1,1,1};
	pRenderer->Draw2dLabel( x, y, 1, white, false, "can leave state: %s", CanLeaveState(data) ? "true" : "false" );
	y += yIncrement;
}

void CAGWaitForUBAllowed::Update( SAnimationStateData& data )
{
}

void CAGWaitForUBAllowed::GetCompletionTimes( SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky )
{
	hard = 0.0f;
	sticky = 0.0f;
}

const IAnimationStateNodeFactory::Params * CAGWaitForUBAllowed::GetParameters()
{
	static const Params params[] = 
	{
		{0}
	};
	return params;
}

bool CAGWaitForUBAllowed::Init( const XmlNodeRef& node, IAnimationGraphPtr )
{
	return true;
}

void CAGWaitForUBAllowed::Release()
{
	delete this;
}

IAnimationStateNode * CAGWaitForUBAllowed::Create()
{
	return this;
}

const char * CAGWaitForUBAllowed::GetCategory()
{
	return "WaitForUBAllowed";
}

const char * CAGWaitForUBAllowed::GetName()
{
	return "WaitForUBAllowed";
}

void CAGWaitForUBAllowed::SerializeAsFile(bool reading, AG_FILE *file)
{
	SerializeAsFile_NodeBase(reading, file);
}
