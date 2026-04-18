#include "StdAfx.h"
#include "IAnimationGraph.h"
#include "AnimatedCharacter.h"
#include "AnimationGraphState.h"
#include "AGSetInput.h"

CAGSetInputNode::CAGSetInputNode( CAGSetInputFactory * pFactory )
	: IAnimationStateNode(eASNF_Update)
	, m_pFactory(pFactory)
	, m_timeRemaining(-1.0f)
{
}

CAGSetInputNode::~CAGSetInputNode()
{
}

void CAGSetInputNode::EnterState( SAnimationStateData& data, bool dueToRollback )
{
}

EHasEnteredState CAGSetInputNode::HasEnteredState( SAnimationStateData& data )
{
	return eHES_Instant;
}

void CAGSetInputNode::EnteredState( SAnimationStateData& data )
{
	if ( m_timeRemaining == -1.0f )
	{
		m_timeRemaining = m_pFactory->m_time;
		if (m_timeRemaining == 0.0f)
		{
			if (m_pFactory->m_restoreOnLeave)
			{
				char valueToRestore[256] = "";
				data.pState->GetInput(m_pFactory->m_inputID, valueToRestore);
				m_valueToRestore = valueToRestore;
			}
			data.pState->SetInput( m_pFactory->m_inputID, m_pFactory->m_value.c_str(), NULL );
		}
	}
}

bool CAGSetInputNode::CanLeaveState( SAnimationStateData& data )
{
	return true;
}

void CAGSetInputNode::LeaveState( SAnimationStateData& data )
{
	if (m_timeRemaining == 0.0f && m_pFactory->m_restoreOnLeave == true)
	{
		char currentValue[256] = "";
		data.pState->GetInput( m_pFactory->m_inputID, currentValue );
		if ( m_pFactory->m_value == currentValue )
			data.pState->SetInput( m_pFactory->m_inputID, m_valueToRestore.c_str(), NULL );
	}
	m_timeRemaining = 0.0f;
}

void CAGSetInputNode::LeftState( SAnimationStateData& data, bool wasEntered )
{
	delete this;
}

void CAGSetInputNode::Update( SAnimationStateData& data )
{
	if (m_timeRemaining <= 0.0f)
		return;
	m_timeRemaining -= gEnv->pTimer->GetFrameTime();
	if (m_timeRemaining > 0.0f)
		return;
	m_timeRemaining = 0.0f;
	if (m_pFactory->m_restoreOnLeave)
	{
		char valueToRestore[256] = "";
		data.pState->GetInput(m_pFactory->m_inputID, valueToRestore);
		m_valueToRestore = valueToRestore;
	}
	data.pState->SetInput( m_pFactory->m_inputID, m_pFactory->m_value.c_str(), NULL );
}

void CAGSetInputNode::GetCompletionTimes(SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky)
{
	hard = sticky = 0.0f;
}

IAnimationStateNodeFactory * CAGSetInputNode::GetFactory()
{
	return m_pFactory;
}

void CAGSetInputNode::DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement )
{
	float white[] = {1,1,1,1};
	pRenderer->Draw2dLabel( x, y, 1, white, false, "time remaining: %.2f / %.2f", m_timeRemaining, m_pFactory->m_time );
	y += yIncrement;
}

/*
 * Factory here
 */

CAGSetInputFactory::CAGSetInputFactory()
	: m_time(0.0f)
	, m_inputID(~0)
	, m_restoreOnLeave(true)
{
}

CAGSetInputFactory::~CAGSetInputFactory()
{
}

bool CAGSetInputFactory::Init( const XmlNodeRef& node, IAnimationGraphPtr pGraph )
{
	node->getAttr( "time", m_time );
	m_input = node->getAttr( "input" );
	m_inputID = pGraph->LookupInputId( m_input.c_str() );
	m_value = node->getAttr( "value" );
	node->getAttr( "restoreOnLeave", m_restoreOnLeave );
	return true;
}

void CAGSetInputFactory::Release()
{
	delete this;
}

IAnimationStateNode * CAGSetInputFactory::Create()
{
	return new CAGSetInputNode( this );
}

const char * CAGSetInputFactory::GetCategory()
{
	return "SetInput";
}

const char * CAGSetInputFactory::GetName()
{
	return "SetInput";
}

const IAnimationStateNodeFactory::Params * CAGSetInputFactory::GetParameters()
{
	static const Params params[] = 
	{
		{true,  "float",   "time",            "Time",              "" },
		{true,  "string",  "input",           "Input Name",        "" },
		{true,  "string",  "value",           "Input Value",       "" },
		{true,  "bool",    "restoreOnLeave",  "Restore on Leave",  "" },
		{0}
	};
	return params;
}
