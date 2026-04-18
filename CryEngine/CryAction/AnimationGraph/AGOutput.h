#ifndef __AGOUTPUT_H__
#define __AGOUTPUT_H__

#pragma once

#include "IAnimationStateNode.h"

class CAGOutput : public IAnimationStateNodeFactory, public IAnimationStateNode
{
public:
	// IAnimationStateNode
	virtual void EnterState( SAnimationStateData& data, bool dueToRollback );
	virtual EHasEnteredState HasEnteredState( SAnimationStateData& data ) { return eHES_Instant; }
	virtual bool CanLeaveState( SAnimationStateData& data );
	virtual void LeaveState( SAnimationStateData& data );
	virtual void EnteredState(SAnimationStateData& data );
	virtual void LeftState(SAnimationStateData& data, bool wasEntered);
	void GetCompletionTimes( SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky );
	virtual const Params * GetParameters();
	virtual IAnimationStateNodeFactory * GetFactory() { return this; }
	virtual void DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement ) {}
	// ~IAnimationStateNode

	// IAnimationStateNodeFactory
	virtual bool Init( const XmlNodeRef& node, IAnimationGraphPtr pGraph );
	virtual void Release();
	virtual IAnimationStateNode * Create();
	virtual const char * GetCategory();
	virtual const char * GetName();
	virtual void SerializeAsFile(bool reading, AG_FILE *file)
	{
		SerializeAsFile_NodeBase(reading, file);

		FileSerializationHelper h(reading, file);
		h.Value(&m_output);
	}

	virtual bool IsLessThan( IAnimationStateNodeFactory * pFactory )
	{
		AG_LT_BEGIN_FACTORY(CAGOutput);
			AG_LT_ELEM(m_output);
		AG_LT_END();
	}
	// ~IAnimationStateNodeFactory

	virtual void GetStateMemoryStatistics(ICrySizer * s)
	{
	}
	virtual void GetFactoryMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

private:
	int m_output;
};

#endif
