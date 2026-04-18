#ifndef __MGMOOD_H__
#define __MGMOOD_H__

#pragma once

#include "IAnimationStateNode.h"

class CMGMood : public IAnimationStateNodeFactory, public IAnimationStateNode
{
public:
	CMGMood();
	virtual ~CMGMood();

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
		h.StringValue(&m_name);
	}
	virtual bool IsLessThan( IAnimationStateNodeFactory * pFactory )
	{
		AG_LT_BEGIN_FACTORY(CMGMood);
			AG_LT_ELEM(m_name);
			AG_LT_ELEM(m_bStartFromBeginning);
		AG_LT_END();
	}
	// ~IAnimationStateNodeFactory

	virtual void GetStateMemoryStatistics(ICrySizer * s)
	{
	}
	virtual void GetFactoryMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
		s->Add(m_name);
	}

private:
	string m_name;
	bool m_bStartFromBeginning;
};

#endif
