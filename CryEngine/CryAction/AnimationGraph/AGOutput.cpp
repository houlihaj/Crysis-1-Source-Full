#include "StdAfx.h"
#include "AGOutput.h"
#include "AnimationGraphState.h"

void CAGOutput::EnterState( SAnimationStateData& data, bool dueToRollback )
{
}

void CAGOutput::EnteredState( SAnimationStateData& data )
{
	data.pState->SetOutput( m_output );
}

bool CAGOutput::CanLeaveState( SAnimationStateData& data )
{
	return true;
}

void CAGOutput::LeaveState( SAnimationStateData& data )
{
}

void CAGOutput::LeftState( SAnimationStateData& data, bool wasEntered )
{
	if (wasEntered)
		data.pState->ClearOutput( m_output );
}

void CAGOutput::GetCompletionTimes( SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky )
{
	hard = sticky = 0.0f;
}

const CAGOutput::Params * CAGOutput::GetParameters()
{
	static const Params params[] = 
	{
		{true,  "string", "name",    "name",                                      "" },
		{true,  "string", "value",    "value",                                      "" },
		{0}
	};
	return params;
}

bool CAGOutput::Init( const XmlNodeRef& node, IAnimationGraphPtr pGraph )
{
	m_output = pGraph->DeclareOutput( node->getAttr("name"), node->getAttr("value") );
	return true;
}

void CAGOutput::Release()
{
	delete this;
}

IAnimationStateNode * CAGOutput::Create()
{
	return this;
}

const char * CAGOutput::GetCategory()
{
	return "Output";
}

const char * CAGOutput::GetName()
{
	return "Output";
}
