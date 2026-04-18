#include "StdAfx.h"
#include "AnimEvent.h"
#include "GameObjects/GameObject.h"

CAnimEventFactory::CAnimEventFactory() : m_matchAnimation(false)
{
}

CAnimEventFactory::~CAnimEventFactory()
{
}

const IAnimationStateNodeFactory::Params * CAnimEventFactory::GetParameters()
{
	static const Params params[] = 
	{
		{false, "string", "onEnter",      "On Enter",          ""},
		{false, "string", "onExit",       "On Exit"            ""},
		{false, "bool", "matchAnimation",       "matchAnimation"            ""},
		{0}
	};
	return params;
}

void CAnimEventFactory::EnteredState( SAnimationStateData& data )
{
	if (m_matchAnimation)
		SendEvent( data, m_onEnterID );
}

void CAnimEventFactory::LeftState( SAnimationStateData& data, bool wasEntered )
{
	if (m_matchAnimation && wasEntered)
		SendEvent( data, m_onExitID );
}

void CAnimEventFactory::EnterState( SAnimationStateData& data, bool dueToRollback )
{
	if (!m_matchAnimation)
		SendEvent( data, m_onEnterID );
}

void CAnimEventFactory::LeaveState( SAnimationStateData& data )
{
	if (!m_matchAnimation)
		SendEvent( data, m_onExitID );
}

void CAnimEventFactory::DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement )
{
	float white[] = {1,1,1,1};
	if (!m_onEnter.empty())
	{
		pRenderer->Draw2dLabel( x, y, 1, white, false, "onEnter: %s", m_onEnter.c_str() );
		y += yIncrement;
	}
	if (!m_onExit.empty())
	{
		pRenderer->Draw2dLabel( x, y, 1, white, false, "onExit: %s", m_onExit.c_str() );
		y += yIncrement;
	}
}

bool CAnimEventFactory::CanLeaveState( SAnimationStateData& data )
{
	return true;
}

void CAnimEventFactory::CacheEventIDs()
{
	IGameObjectSystem* pGOSys = CCryAction::GetCryAction()->GetIGameObjectSystem();
	m_onEnterID = pGOSys->GetEventID(m_onEnter.c_str());
	m_onExitID = pGOSys->GetEventID(m_onExit.c_str());
}

bool CAnimEventFactory::Init( const XmlNodeRef& node, IAnimationGraphPtr )
{
	m_sendFlags = eGOEF_ToAll;
	m_onEnter = node->getAttr("onEnter");
	m_onExit = node->getAttr("onExit");
	CacheEventIDs();
	m_matchAnimation = false;
	node->getAttr("matchAnimation", m_matchAnimation);
	
	bool noScript(false);
	node->getAttr("noScript", noScript);

	if (noScript)
		m_sendFlags &= ~eGOEF_ToScriptSystem;
	
	return true;
}

void CAnimEventFactory::Release()
{
	delete this;
}

IAnimationStateNode * CAnimEventFactory::Create()
{
	return this;
}

const char * CAnimEventFactory::GetCategory()
{
	return "Event";
}

const char * CAnimEventFactory::GetName()
{
	return "Event";
}

void CAnimEventFactory::SendEvent( SAnimationStateData& data, uint32 eventID )
{
	if (eventID != IGameObjectSystem::InvalidEventID)
	{
		SGameObjectEvent event( eventID, eGOEF_ToAll );
		data.pGameObject->SendEvent( event );
	}
}
