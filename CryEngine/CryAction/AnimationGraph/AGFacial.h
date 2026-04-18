#ifndef __AGFACIAL_H__
#define __AGFACIAL_H__

#pragma once

#include "IAnimationStateNode.h"

class CAGFacial : public IAnimationStateNodeFactory, public IAnimationStateNode
{
public:
	CAGFacial();
	virtual ~CAGFacial();

	// IAnimationStateNode
	virtual void EnterState( SAnimationStateData& data, bool dueToRollback );
	virtual EHasEnteredState HasEnteredState( SAnimationStateData& data );
	virtual bool CanLeaveState( SAnimationStateData& data );
	virtual void LeaveState( SAnimationStateData& data );
	virtual void EnteredState( SAnimationStateData& data );
	virtual void LeftState( SAnimationStateData& data, bool wasEntered );
	virtual void Update( SAnimationStateData& data );
	virtual void GetCompletionTimes( SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky );
	virtual const Params * GetParameters();
	virtual IAnimationStateNodeFactory * GetFactory();
	virtual void DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement );
	// ~IAnimationStateNode

	// IAnimationStateNodeFactory
	virtual bool Init( const XmlNodeRef& node, IAnimationGraphPtr );
	virtual void Release();
	virtual IAnimationStateNode * Create();
	virtual const char * GetCategory();
	virtual const char * GetName();
	virtual void SerializeAsFile(bool reading, AG_FILE *file)
	{
		SerializeAsFile_NodeBase(reading, file);

		FileSerializationHelper h(reading, file);
		h.StringValue(&m_expressionNameIdle);
		h.StringValue(&m_expressionNameAlerted);
		h.StringValue(&m_expressionNameCombat);
	}
	virtual bool IsLessThan( IAnimationStateNodeFactory * pFactory )
	{
		AG_LT_BEGIN_FACTORY(CAGFacial);
			AG_LT_ELEM(m_expressionNameIdle);
			AG_LT_ELEM(m_expressionNameAlerted);
			AG_LT_ELEM(m_expressionNameCombat);
		AG_LT_END();
	}
	// ~IAnimationStateNodeFactory

	virtual void GetStateMemoryStatistics(ICrySizer * s)
	{
	}

	virtual void GetFactoryMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
		s->Add(m_expressionNameIdle);
		s->Add(m_expressionNameAlerted);
		s->Add(m_expressionNameCombat);
	}

private:
	string m_expressionNameIdle;
	string m_expressionNameAlerted;
	string m_expressionNameCombat;
};

#endif
