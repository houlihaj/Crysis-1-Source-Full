// forces the clearing of start after overrides... for better controllability under some circumstances

#ifndef __AGFORCESTARTIMMEDIATELY_H__
#define __AGFORCESTARTIMMEDIATELY_H__

#pragma once

#include "IAnimationStateNode.h"

class CAGForceStartImmediately : public IAnimationStateNodeFactory, public IAnimationStateNode
{
public:
	// IAnimationStateNode
	virtual void EnterState( SAnimationStateData& data, bool dueToRollback ) {}
	virtual EHasEnteredState HasEnteredState( SAnimationStateData& data ) { return eHES_Instant; }
	virtual bool CanLeaveState( SAnimationStateData& data ) {return true;}
	virtual void LeaveState( SAnimationStateData& data );
	virtual void EnteredState(SAnimationStateData& data ) {}
	virtual void LeftState(SAnimationStateData& data, bool wasEntered) {}
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
	virtual void SerializeAsFile(bool reading, AG_FILE *file) {SerializeAsFile_NodeBase(reading, file);}
	virtual bool IsLessThan( IAnimationStateNodeFactory * pFactory ) { return false; }
	// ~IAnimationStateNodeFactory

	virtual void GetStateMemoryStatistics(ICrySizer * s)
	{
	}
	virtual void GetFactoryMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

private:
};

#endif
