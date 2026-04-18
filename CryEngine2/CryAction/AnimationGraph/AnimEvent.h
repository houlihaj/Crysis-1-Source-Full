#ifndef __ANIMEVENT_H__
#define __ANIMEVENT_H__

#pragma once

#include "IAnimationStateNode.h"

class CAnimEventFactory : public IAnimationStateNodeFactory, public IAnimationStateNode
{
public:
	CAnimEventFactory();
	~CAnimEventFactory();

	// IAnimationStateNode
	virtual void EnterState( SAnimationStateData& data, bool dueToRollback );
	virtual EHasEnteredState HasEnteredState( SAnimationStateData& data ) { return eHES_Instant; }
	virtual bool CanLeaveState( SAnimationStateData& data );
	virtual void LeaveState( SAnimationStateData& data );
	virtual void EnteredState( SAnimationStateData& data );
	virtual void LeftState( SAnimationStateData& data, bool wasEntered );
	void GetCompletionTimes( SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky ) 
	{
		hard = sticky = 0.0f;
	}
	virtual const Params * GetParameters();
	virtual IAnimationStateNodeFactory * GetFactory() { return this; }
	virtual void DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement );
	// ~IAnimationStateNode

	// IAnimationStateNodeFactory
	virtual bool Init( const XmlNodeRef& node, IAnimationGraphPtr );
	virtual void Release();
	virtual IAnimationStateNode * Create();
	virtual const char * GetCategory();
	virtual const char * GetName();
	virtual bool IsLessThan( IAnimationStateNodeFactory * pFactory )
	{
		AG_LT_BEGIN_FACTORY(CAnimEventFactory);
			AG_LT_ELEM(m_onEnter);
			AG_LT_ELEM(m_onExit);
			AG_LT_ELEM(m_matchAnimation);
			AG_LT_ELEM(m_sendFlags);
		AG_LT_END();
	}
	virtual void SerializeAsFile(bool reading, AG_FILE *file)
	{
		SerializeAsFile_NodeBase(reading, file);
		FileSerializationHelper h(reading, file);
		h.StringValue(&m_onEnter);
		h.StringValue(&m_onExit);
		if (reading)
			CacheEventIDs();
		h.Value(&m_matchAnimation);
		h.Value(&m_sendFlags);
	}
	// ~IAnimationStateNodeFactory


	virtual void GetStateMemoryStatistics(ICrySizer * s)
	{
	}
	virtual void GetFactoryMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
		s->Add(m_onEnter);
		s->Add(m_onExit);
	}

private:
	void CacheEventIDs();

	// remember to update IsLessThan() and SerializeAsFile()
	string m_onEnter;
	uint32 m_onEnterID;
	string m_onExit;
	uint32 m_onExitID;
	bool m_matchAnimation;
	int m_sendFlags;

	static void SendEvent( SAnimationStateData& data, uint32 eventID);
};

#endif
