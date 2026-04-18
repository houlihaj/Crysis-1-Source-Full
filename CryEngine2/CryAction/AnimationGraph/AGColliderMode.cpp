#include "StdAfx.h"
#include "AGColliderMode.h"
#include "ICryAnimation.h"
//#include "GameObject.h"
#include "AnimatedCharacter.h"

CAGColliderMode::CAGColliderMode()
{
	m_colliderMode = eColliderMode_Undefined;
}

CAGColliderMode::~CAGColliderMode()
{
}

void CAGColliderMode::EnterState( SAnimationStateData& data, bool dueToRollback )
{
}

EHasEnteredState CAGColliderMode::HasEnteredState( SAnimationStateData& data )
{
	return eHES_Instant;
}

void CAGColliderMode::EnteredState( SAnimationStateData& data )
{
	if (!data.pEntity || !data.pAnimatedCharacter)
		return;

	data.pAnimatedCharacter->RequestPhysicalColliderMode(m_colliderMode, eColliderModeLayer_AnimGraph);
}

bool CAGColliderMode::CanLeaveState( SAnimationStateData& data )
{
	return true;
}

void CAGColliderMode::LeaveState( SAnimationStateData& data )
{
}

void CAGColliderMode::LeftState( SAnimationStateData& data, bool wasEntered )
{
}

void CAGColliderMode::Update( SAnimationStateData& data )
{
	assert(false);
}

void CAGColliderMode::GetCompletionTimes(SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky)
{
	hard = sticky = 0.0f;
}

const IAnimationStateNodeFactory::Params * CAGColliderMode::GetParameters()
{
	static const Params params[] = 
	{
		{true,  "string", "mode", "ColliderMode", "Undefined" },
		{0}
	};
	return params;
}

IAnimationStateNodeFactory * CAGColliderMode::GetFactory()
{
	return this;
}

void CAGColliderMode::DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement )
{
/*
	float white[] = {1,1,1,1};
	pRenderer->Draw2dLabel( x, y, 1, white, false, "anim: %s", m_name.c_str() );
	y += yIncrement;
*/
}

bool CAGColliderMode::Init(const XmlNodeRef& node, IAnimationGraphPtr pGraph)
{
	string modeStr = node->getAttr("mode");
	for (int mode = eColliderMode_Undefined; mode < eColliderMode_COUNT; mode++)
	{
		if (modeStr == g_szColliderModeString[(EColliderMode)mode])
		{
			m_colliderMode = (EColliderMode)mode;
			return true;
		}
	}

	return false;
}

void CAGColliderMode::Release()
{
	delete this;
}

IAnimationStateNode * CAGColliderMode::Create()
{
	return this;
}

const char * CAGColliderMode::GetCategory()
{
	return "ColliderMode";
}

const char * CAGColliderMode::GetName()
{
	return "ColliderMode";
}
