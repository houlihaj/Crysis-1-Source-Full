#include "StdAfx.h"
#include "AGMovementControlMethod.h"
#include "ICryAnimation.h"
//#include "GameObject.h"
#include "AnimatedCharacter.h"
#include "AnimationGraphCVars.h"
#include "AnimationGraphState.h"

CAGMovementControlMethod::CAGMovementControlMethod()
{
	m_horizontal = eMCM_Undefined;
	m_vertical = eMCM_Undefined;
	m_distance = -1.0f;
	m_angle = -1.0f;
}

CAGMovementControlMethod::~CAGMovementControlMethod()
{
}

void CAGMovementControlMethod::EnterState( SAnimationStateData& data, bool dueToRollback )
{
	// TODO: Resurrect CA_FORCE_ROOT_UPDATE and use it here instead of CA_FORCE_SKELETON_UPDATE.
	// We actually need to force update of the root only as that gives us the movement, but
	// since that flag doesn't exist anymore we force update of the full skeleton instead.
	if ((m_horizontal == eMCM_Animation) || (m_vertical == eMCM_Animation))
		data.params[0].m_nFlags |= CA_FORCE_SKELETON_UPDATE;
}

EHasEnteredState CAGMovementControlMethod::HasEnteredState( SAnimationStateData& data )
{
	return eHES_Instant;
}

void CAGMovementControlMethod::EnteredState( SAnimationStateData& data )
{
	if (!data.pEntity || !data.pAnimatedCharacter)
		return;

	if (CAnimationGraphCVars::Get().m_TemplateMCMs == 0)
	{
		return;
	}

	const char* stateName = data.pState->GetCurrentStateName();
	//CryLog("-> EnteredState %s (%s, %s)", stateName, g_szMCMString[m_horizontal], g_szMCMString[m_vertical]);

	data.pAnimatedCharacter->SetMovementControlMethods(m_horizontal, m_vertical);
	data.pAnimatedCharacter->SetLocationClampingOverride(m_distance, m_angle);
}

bool CAGMovementControlMethod::CanLeaveState( SAnimationStateData& data )
{
	return true;
}

void CAGMovementControlMethod::LeaveState( SAnimationStateData& data )
{
}

void CAGMovementControlMethod::LeftState( SAnimationStateData& data, bool wasEntered )
{
}

void CAGMovementControlMethod::Update( SAnimationStateData& data )
{
	assert(false);
}

void CAGMovementControlMethod::GetCompletionTimes(SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky)
{
	hard = sticky = 0.0f;
}

const IAnimationStateNodeFactory::Params * CAGMovementControlMethod::GetParameters()
{
	static const Params params[] = 
	{
		{true,  "string", "horizontal", "Horizontal", g_szMCMString[eMCM_Entity] },
		{true,  "string", "vertical", "Vertical", g_szMCMString[eMCM_Entity] },
		{true,  "float", "distance", "LocationClampDistance", "-1.0" },
		{true,  "float", "angle", "LocationClampAngle", "-1.0" },
		{0}
	};
	return params;
}

IAnimationStateNodeFactory * CAGMovementControlMethod::GetFactory()
{
	return this;
}

void CAGMovementControlMethod::DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement )
{
/*
	float white[] = {1,1,1,1};
	pRenderer->Draw2dLabel( x, y, 1, white, false, "anim: %s", m_name.c_str() );
	y += yIncrement;
*/
}

bool CAGMovementControlMethod::Init(const XmlNodeRef& node, IAnimationGraphPtr pGraph)
{
	string methodHorizontal = node->getAttr("horizontal");
	string methodVertical = node->getAttr("vertical");
	for (int mode = eMCM_Undefined; mode < eMCM_COUNT; mode++)
	{
		if (methodHorizontal == g_szMCMString[(EMovementControlMethod)mode])
		{
			m_horizontal = (EMovementControlMethod)mode;
		}
		if (methodVertical == g_szMCMString[(EMovementControlMethod)mode])
		{
			m_vertical = (EMovementControlMethod)mode;
		}
	}

	bool hasDistance = node->getAttr("distance", m_distance);
	bool hasAngle = node->getAttr("angle", m_angle);

	return ((m_horizontal != eMCM_Undefined) && (m_vertical != eMCM_Undefined));
}

void CAGMovementControlMethod::Release()
{
	delete this;
}

IAnimationStateNode * CAGMovementControlMethod::Create()
{
	return this;
}

const char * CAGMovementControlMethod::GetCategory()
{
	return "MovementControlMethod";
}

const char * CAGMovementControlMethod::GetName()
{
	return "MovementControlMethod";
}
