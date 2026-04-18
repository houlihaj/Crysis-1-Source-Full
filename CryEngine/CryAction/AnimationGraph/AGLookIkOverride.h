#ifndef __AGLookIkOverride_h__
#define __AGLookIkOverride_h__

#pragma once

#include "IAnimationStateNode.h"

class CAGLookIkOverride : public IAnimationStateNodeFactory, public IAnimationStateNode
{
public:
	CAGLookIkOverride();
	virtual ~CAGLookIkOverride();

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
	virtual void DebugDraw(SAnimationStateData& data, IRenderer* pRenderer, int x, int& y, int yIncrement);
	virtual void GetStateMemoryStatistics(ICrySizer* s) {}
	virtual void GetFactoryMemoryStatistics(ICrySizer* s) { s->Add(*this); }
	// ~IAnimationStateNode

	// IAnimationStateNodeFactory
	virtual bool Init(const XmlNodeRef& node, IAnimationGraphPtr);
	virtual void Release();
	virtual IAnimationStateNode * Create();
	virtual const char * GetCategory();
	virtual const char * GetName();
	virtual void SerializeAsFile(bool reading, AG_FILE *file)
	{
		SerializeAsFile_NodeBase(reading, file);

		FileSerializationHelper h(reading, file);
		h.Value(&m_bOverride);
	}
	virtual bool IsLessThan(IAnimationStateNodeFactory * pFactory)
	{
		AG_LT_BEGIN_FACTORY(CAGLookIkOverride);
			AG_LT_ELEM(m_bOverride);
		AG_LT_END();
	}
	// ~IAnimationStateNodeFactory

private:
	bool m_bOverride;
};

#endif // __AGLookIkOverride_h__
