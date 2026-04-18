#include "StdAfx.h"
#include "MGTheme.h"
#include "IMusicSystem.h"

CMGTheme::CMGTheme()
{
}

CMGTheme::~CMGTheme()
{
}

void CMGTheme::EnterState( SAnimationStateData& data, bool dueToRollback )
{
}

EHasEnteredState CMGTheme::HasEnteredState( SAnimationStateData& data )
{
	return eHES_Instant;
}

void CMGTheme::EnteredState( SAnimationStateData& data )
{
	if (gEnv->pMusicSystem && strcmp( m_name, "" ))
		gEnv->pMusicSystem->SetTheme( m_name.c_str() );
}

bool CMGTheme::CanLeaveState( SAnimationStateData& data )
{
	return true;
}

void CMGTheme::LeaveState( SAnimationStateData& data )
{
}

void CMGTheme::LeftState( SAnimationStateData& data, bool wasEntered )
{
}

void CMGTheme::Update( SAnimationStateData& data )
{
	assert(false);
}

void CMGTheme::GetCompletionTimes(SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky)
{
	hard = sticky = 0.0f;
}

const IAnimationStateNodeFactory::Params * CMGTheme::GetParameters()
{
	static const Params params[] = 
	{
		{true,  "string", "name",    "Name",                                      "" },
		{0}
	};
	return params;
}

IAnimationStateNodeFactory * CMGTheme::GetFactory()
{
	return this;
}

void CMGTheme::DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement )
{
	float white[] = {1,1,1,1};
	pRenderer->Draw2dLabel( x, y, 1, white, false, "anim: %s", m_name.c_str() );
	y += yIncrement;
}

bool CMGTheme::Init(const XmlNodeRef& node, IAnimationGraphPtr pGraph)
{
	m_name = node->getAttr("name");
	return true;
}

void CMGTheme::Release()
{
	delete this;
}

IAnimationStateNode * CMGTheme::Create()
{
	return this;
}

const char * CMGTheme::GetCategory()
{
	return "MusicTheme";
}

const char * CMGTheme::GetName()
{
	return "MusicTheme";
}
