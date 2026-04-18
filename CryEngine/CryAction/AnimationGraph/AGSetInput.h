#ifndef __AGSETINPUT_H__
#define __AGSETINPUT_H__

#pragma once

#include "IAnimationStateNode.h"


class CAGSetInputFactory : public IAnimationStateNodeFactory
{
	friend class CAGSetInputNode;

public:
	CAGSetInputFactory();
	virtual ~CAGSetInputFactory();

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
		h.StringValue(&m_input);
		h.Value(&m_inputID);
		h.StringValue(&m_value);
		h.Value(&m_restoreOnLeave);
	}
	virtual const Params * GetParameters();
	virtual bool IsLessThan( IAnimationStateNodeFactory * pFactory )
	{
		AG_LT_BEGIN(CAGSetInputFactory);
			AG_LT_ELEM(m_time);
			AG_LT_ELEM(m_input);
			AG_LT_ELEM(m_value);
			AG_LT_ELEM(m_restoreOnLeave);
		AG_LT_END();
	}
	// ~IAnimationStateNodeFactory

	virtual void GetFactoryMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
		s->Add(m_input);
		s->Add(m_value);
	}

private:
	float m_time;
	string m_input;
	string m_value;
	IAnimationGraph::InputID m_inputID;
	bool m_restoreOnLeave;
};


class CAGSetInputNode : public IAnimationStateNode
{
public:
	CAGSetInputNode( CAGSetInputFactory * pFactory );
	virtual ~CAGSetInputNode();

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
	CAGSetInputFactory * m_pFactory;
	string m_valueToRestore;
};

#endif
