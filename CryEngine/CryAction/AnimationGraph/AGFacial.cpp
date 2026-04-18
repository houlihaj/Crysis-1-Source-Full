#include "StdAfx.h"
#include "AGFacial.h"
#include "ICryAnimation.h"
#include "IFacialAnimation.h"
#include "AnimatedCharacter.h"

CAGFacial::CAGFacial()
{
}

CAGFacial::~CAGFacial()
{
}

void CAGFacial::EnterState( SAnimationStateData& data, bool dueToRollback )
{
}

EHasEnteredState CAGFacial::HasEnteredState( SAnimationStateData& data )
{
	return eHES_Instant;
}

void CAGFacial::EnteredState( SAnimationStateData& data )
{
	if (!data.pEntity || !data.pAnimatedCharacter)
		return;

	// TODO: This needs to dig up the AI alertness first, and pick the expression that is associated with that alertness.
	const char* requested = NULL;
	
	int alertness = data.pAnimatedCharacter->GetFacialAlertnessLevel();
	if (alertness == 0)
		requested = m_expressionNameIdle;
	else if (alertness == 1)
			requested = m_expressionNameAlerted;
	else if (alertness == 2)
		requested = m_expressionNameCombat;

	if ( requested == NULL || requested[0] == 0 )
		return;

	IEntity* pEntity = data.pEntity;
	ICharacterInstance* pCharacter = (pEntity ? pEntity->GetCharacter(0) : 0);
	IFacialInstance* pFacialInstance = (pCharacter ? pCharacter->GetFacialInstance() : 0);

	const char* comma = strchr(requested, ',');
	if (comma == NULL && pFacialInstance)
	{
		IFacialAnimSequence* pSequence = pFacialInstance->LoadSequence(requested);
		if (pFacialInstance && pSequence)
			pFacialInstance->PlaySequence(pSequence, eFacialSequenceLayer_AGStateAndAIAlertness);
	}
	else if (pFacialInstance)
	{
		std::vector<const char*> commas;
		commas.push_back(requested);
		do
		{
			commas.push_back(++comma);
			comma = strchr(comma, ',');
		} while (comma != NULL);

		unsigned int r = Random((uint32)commas.size());
		if (r == commas.size()-1)
		{
			IFacialAnimSequence* pSequence = pFacialInstance->LoadSequence(commas.back());
			if (pFacialInstance && pSequence)
				pFacialInstance->PlaySequence(pSequence, eFacialSequenceLayer_AGStateAndAIAlertness);
		}
		else
		{
			string req(commas[r], commas[r+1] - 1);
			IFacialAnimSequence* pSequence = pFacialInstance->LoadSequence(req.c_str());
			if (pFacialInstance && pSequence)
				pFacialInstance->PlaySequence(pSequence, eFacialSequenceLayer_AGStateAndAIAlertness);
		}
	}
}

bool CAGFacial::CanLeaveState( SAnimationStateData& data )
{
	return true;
}

void CAGFacial::LeaveState( SAnimationStateData& data )
{
}

void CAGFacial::LeftState( SAnimationStateData& data, bool wasEntered )
{
	IEntity* pEntity = data.pEntity;
	ICharacterInstance* pCharacter = (pEntity ? pEntity->GetCharacter(0) : 0);
	IFacialInstance* pFacialInstance = (pCharacter ? pCharacter->GetFacialInstance() : 0);
	if (pFacialInstance)
		pFacialInstance->PlaySequence(0, eFacialSequenceLayer_AGStateAndAIAlertness);
}

void CAGFacial::Update( SAnimationStateData& data )
{
	assert(false);
}

void CAGFacial::GetCompletionTimes(SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky)
{
	hard = sticky = 0.0f;
}

const IAnimationStateNodeFactory::Params * CAGFacial::GetParameters()
{
	static const Params params[] = 
	{
		{true,  "string", "idle",    "Idle",         "" },
		{true,  "string", "alerted",    "Alerted",         "" },
		{true,  "string", "combat",    "Combat",         "" },
		{true,  "string", "all",    "Always",         "" },
		{0}
	};
	return params;
}

IAnimationStateNodeFactory * CAGFacial::GetFactory()
{
	return this;
}

void CAGFacial::DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement )
{
	float white[] = {1,1,1,1};
	pRenderer->Draw2dLabel( x, y, 1, white, false, "idle: %s", m_expressionNameIdle.c_str() );
	y += yIncrement;
	pRenderer->Draw2dLabel( x, y, 1, white, false, "alerted: %s", m_expressionNameAlerted.c_str() );
	y += yIncrement;
	pRenderer->Draw2dLabel( x, y, 1, white, false, "combat: %s", m_expressionNameCombat.c_str() );
	y += yIncrement;
}

bool CAGFacial::Init(const XmlNodeRef& node, IAnimationGraphPtr pGraph)
{
	m_expressionNameIdle = node->getAttr("idle");
	m_expressionNameAlerted = node->getAttr("alerted");
	m_expressionNameCombat = node->getAttr("combat");

	if (node->haveAttr("all"))
	{
		m_expressionNameIdle = node->getAttr("all");
		m_expressionNameAlerted = m_expressionNameIdle;
		m_expressionNameCombat = m_expressionNameIdle;
	}

	return true;
}

void CAGFacial::Release()
{
	delete this;
}

IAnimationStateNode * CAGFacial::Create()
{
	return this;
}

const char * CAGFacial::GetCategory()
{
	return "FacialEffector";
}

const char * CAGFacial::GetName()
{
	return "FacialEffector";
}
