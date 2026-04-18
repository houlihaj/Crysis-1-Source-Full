#ifndef __AGWAITFORUBALLOWED_H__
#define __AGWAITFORUBALLOWED_H__

#pragma once

#include "IAnimationStateNode.h"

class CAGWaitForUBAllowed : public IAnimationStateNodeFactory, public IAnimationStateNode
{
public:
	inline CAGWaitForUBAllowed() {}
	inline virtual ~CAGWaitForUBAllowed() {}

	// IAnimationStateNode
	virtual void EnterState( SAnimationStateData& data, bool dueToRollback );
	virtual EHasEnteredState HasEnteredState( SAnimationStateData& data );
	virtual bool CanLeaveState( SAnimationStateData& data );
	virtual void LeaveState( SAnimationStateData& data );
	virtual void EnteredState( SAnimationStateData& data ) {}
	virtual void LeftState( SAnimationStateData& data, bool wasEntered ) {}
	virtual void Update( SAnimationStateData& data );
	virtual void GetCompletionTimes( SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky );
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
	virtual void SerializeAsFile(bool reading, AG_FILE *file);

	virtual bool IsLessThan( IAnimationStateNodeFactory * pFactory )
	{
		AG_LT_BEGIN_FACTORY(CAGWaitForUBAllowed);
		AG_LT_END();
	}
	// ~IAnimationStateNodeFactory

	virtual void GetFactoryMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
	virtual void GetStateMemoryStatistics(ICrySizer * s)
	{
	}

private:
	// remember to update IsLessThan, SerializeAsFile
};


#endif
