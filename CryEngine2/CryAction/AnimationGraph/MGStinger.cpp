#include "StdAfx.h"
#include "MGStinger.h"
#include "IMusicSystem.h"

CMGStinger::CMGStinger()
{
}

CMGStinger::~CMGStinger()
{
}

void CMGStinger::EnterState( SAnimationStateData& data, bool dueToRollback )
{
}

EHasEnteredState CMGStinger::HasEnteredState( SAnimationStateData& data )
{
	return eHES_Instant;
}

void CMGStinger::EnteredState( SAnimationStateData& data )
{
	if (gEnv->pMusicSystem)
		gEnv->pMusicSystem->PlayStinger();
}

bool CMGStinger::CanLeaveState( SAnimationStateData& data )
{
	return true;
}

void CMGStinger::LeaveState( SAnimationStateData& data )
{
}

void CMGStinger::LeftState( SAnimationStateData& data, bool wasEntered )
{
}

void CMGStinger::Update( SAnimationStateData& data )
{
	assert(false);
}

void CMGStinger::GetCompletionTimes(SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky)
{
	hard = sticky = 0.0f;
}

const IAnimationStateNodeFactory::Params * CMGStinger::GetParameters()
{
	static const Params params[] = 
	{
		{0}
	};
	return params;
}

IAnimationStateNodeFactory * CMGStinger::GetFactory()
{
	return this;
}

void CMGStinger::DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement )
{
}

bool CMGStinger::Init(const XmlNodeRef& node, IAnimationGraphPtr pGraph)
{
	return true;
}

void CMGStinger::Release()
{
	delete this;
}

IAnimationStateNode * CMGStinger::Create()
{
	return this;
}

const char * CMGStinger::GetCategory()
{
	return "MusicStinger";
}

const char * CMGStinger::GetName()
{
	return "MusicStinger";
}
