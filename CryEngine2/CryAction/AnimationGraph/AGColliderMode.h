#ifndef __AGColliderMode_h__
#define __AGColliderMode_h__

#pragma once

#include "IAnimationStateNode.h"

class CAGColliderMode : public IAnimationStateNodeFactory, public IAnimationStateNode
{
public:
	CAGColliderMode();
	virtual ~CAGColliderMode();

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
		h.Value(&m_colliderMode);
	}
	virtual bool IsLessThan(IAnimationStateNodeFactory * pFactory)
	{
		AG_LT_BEGIN_FACTORY(CAGColliderMode);
			AG_LT_ELEM(m_colliderMode);
		AG_LT_END();
	}
	// ~IAnimationStateNodeFactory

private:
	EColliderMode m_colliderMode;
};

#endif // __AGColliderMode_h__
