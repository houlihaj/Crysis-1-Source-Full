#include "StdAfx.h"
#include "AGLookIkOverride.h"
#include "ICryAnimation.h"
#include "AnimatedCharacter.h"

CAGLookIkOverride::CAGLookIkOverride()
{
 	m_bOverride = false;
}

CAGLookIkOverride::~CAGLookIkOverride()
{
}

void CAGLookIkOverride::EnterState( SAnimationStateData& data, bool dueToRollback )
{
}

EHasEnteredState CAGLookIkOverride::HasEnteredState( SAnimationStateData& data )
{
	return eHES_Instant;
}

void CAGLookIkOverride::EnteredState( SAnimationStateData& data )
{
	if (!data.pEntity || !data.pAnimatedCharacter)
		return;

	data.pAnimatedCharacter->AllowLookIk( !m_bOverride );
}

bool CAGLookIkOverride::CanLeaveState( SAnimationStateData& data )
{
	return true;
}

void CAGLookIkOverride::LeaveState( SAnimationStateData& data )
{
	if (!data.pEntity || !data.pAnimatedCharacter)
		return;

	data.pAnimatedCharacter->AllowLookIk( m_bOverride );
}

void CAGLookIkOverride::LeftState( SAnimationStateData& data, bool wasEntered )
{
}

void CAGLookIkOverride::Update( SAnimationStateData& data )
{
	assert(false);
}

void CAGLookIkOverride::GetCompletionTimes(SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky)
{
	hard = sticky = 0.0f;
}

const IAnimationStateNodeFactory::Params * CAGLookIkOverride::GetParameters()
{
	static const Params params[] = 
	{
		{true,  "string", "override", "LookIk", "0" },
		{0}
	};
	return params;
}

IAnimationStateNodeFactory * CAGLookIkOverride::GetFactory()
{
	return this;
}

void CAGLookIkOverride::DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement )
{
/*
	float white[] = {1,1,1,1};
	pRenderer->Draw2dLabel( x, y, 1, white, false, "anim: %s", m_name.c_str() );
	y += yIncrement;
*/
}

bool CAGLookIkOverride::Init(const XmlNodeRef& node, IAnimationGraphPtr pGraph)
{
	string modeStr = node->getAttr("override");
	m_bOverride = modeStr == "1";
	return m_bOverride || modeStr == "0";
}

void CAGLookIkOverride::Release()
{
	delete this;
}

IAnimationStateNode * CAGLookIkOverride::Create()
{
	return this;
}

const char * CAGLookIkOverride::GetCategory()
{
	return "LookIk";
}

const char * CAGLookIkOverride::GetName()
{
	return "LookIk";
}
