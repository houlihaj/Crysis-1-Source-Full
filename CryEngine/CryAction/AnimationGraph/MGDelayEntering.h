#ifndef __MGDELAYENTERING_H__
#define __MGDELAYENTERING_H__

#pragma once

#include "IAnimationStateNode.h"

class CMGDelayEnteringFactory : public IAnimationStateNodeFactory
{
	friend class CMGDelayEnteringNode;

public:
	CMGDelayEnteringFactory();
	virtual ~CMGDelayEnteringFactory();

	// IAnimationStateNodeFactory
	virtual bool Init( const XmlNodeRef& node, IAnimationGraphPtr );
	virtual void Release();
	virtual IAnimationStateNode * Create();
	virtual const char * GetCategory();
	virtual const char * GetName();
	virtual void SerializeAsFile(bool reading, AG_FILE *file)
	{
		FileSerializationHelper h(reading, file);
		h.Value(&m_time);
	}
	virtual const Params * GetParameters();

	virtual bool IsLessThan( IAnimationStateNodeFactory * pFactory )
	{
		AG_LT_BEGIN(CMGDelayEnteringFactory);
			AG_LT_ELEM(m_time);
		AG_LT_END();
	}
	// ~IAnimationStateNodeFactory

	virtual void GetFactoryMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

private:
	float m_time;
};

class CMGDelayEnteringNode : public IAnimationStateNode
{
public:
	CMGDelayEnteringNode( CMGDelayEnteringFactory * pFactory );
	virtual ~CMGDelayEnteringNode();

	// IAnimationStateNode
	virtual void EnterState( SAnimationStateData& data, bool dueToRollback );
	virtual EHasEnteredState HasEnteredState( SAnimationStateData& data );
	virtual bool CanLeaveState( SAnimationStateData& data );
	virtual void LeaveState( SAnimationStateData& data );
	virtual void EnteredState( SAnimationStateData& data );
	virtual void LeftState( SAnimationStateData& data, bool wasEntered );
	virtual void Update( SAnimationStateData& data );
	virtual void GetCompletionTimes( SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky );
	virtual IAnimationStateNodeFactory * GetFactory();
	virtual void DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement );
	// ~IAnimationStateNode

	virtual void GetStateMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

private:
	float m_timeRemaining;
	CMGDelayEnteringFactory * m_pFactory;
};

#endif
