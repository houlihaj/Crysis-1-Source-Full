#include "StdAfx.h"
#include "MGMood.h"
#include "IMusicSystem.h"

CMGMood::CMGMood()
{
	m_bStartFromBeginning = false;
}

CMGMood::~CMGMood()
{
}

void CMGMood::EnterState( SAnimationStateData& data, bool dueToRollback )
{
}

EHasEnteredState CMGMood::HasEnteredState( SAnimationStateData& data )
{
	return eHES_Instant;
}

void CMGMood::EnteredState( SAnimationStateData& data )
{
	if(gEnv->pMusicSystem)
		gEnv->pMusicSystem->SetMood( m_name.c_str(), m_bStartFromBeginning, false );
}

bool CMGMood::CanLeaveState( SAnimationStateData& data )
{
	return true;
}

void CMGMood::LeaveState( SAnimationStateData& data )
{
}

void CMGMood::LeftState( SAnimationStateData& data, bool wasEntered )
{
}

void CMGMood::Update( SAnimationStateData& data )
{
	assert(false);
}

void CMGMood::GetCompletionTimes(SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky)
{
	hard = sticky = 0.0f;
}

const IAnimationStateNodeFactory::Params * CMGMood::GetParameters()
{
	static const Params params[] = 
	{
		{true,  "string", "name",    "Name",                                      "" },
		{0}
	};
	return params;
}

IAnimationStateNodeFactory * CMGMood::GetFactory()
{
	return this;
}

void CMGMood::DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement )
{
	float white[] = {1,1,1,1};
	pRenderer->Draw2dLabel( x, y, 1, white, false, "anim: %s", m_name.c_str() );
	y += yIncrement;
}

bool CMGMood::Init(const XmlNodeRef& node, IAnimationGraphPtr pGraph)
{
	m_name = node->getAttr("name");
	node->getAttr("startfrombeginning", m_bStartFromBeginning);
	return true;
}

void CMGMood::Release()
{
	delete this;
}

IAnimationStateNode * CMGMood::Create()
{
	return this;
}

const char * CMGMood::GetCategory()
{
	return "MusicMood";
}

const char * CMGMood::GetName()
{
	return "MusicMood";
}
