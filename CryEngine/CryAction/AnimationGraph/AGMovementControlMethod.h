#ifndef __AGMovementControlMethod_h__
#define __AGMovementControlMethod_h__

#pragma once

#include "IAnimationStateNode.h"

class CAGMovementControlMethod : public IAnimationStateNodeFactory, public IAnimationStateNode
{
public:
	CAGMovementControlMethod();
	virtual ~CAGMovementControlMethod();

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
		h.Value(&m_horizontal);
		h.Value(&m_vertical);
		h.Value(&m_distance);
		h.Value(&m_angle);
	}
	virtual bool IsLessThan(IAnimationStateNodeFactory * pFactory)
	{
		AG_LT_BEGIN_FACTORY(CAGMovementControlMethod);
			AG_LT_ELEM(m_horizontal);
			AG_LT_ELEM(m_vertical);
			AG_LT_ELEM(m_distance);
			AG_LT_ELEM(m_angle);
		AG_LT_END();
	}
	// ~IAnimationStateNodeFactory

private:
	EMovementControlMethod m_horizontal;
	EMovementControlMethod m_vertical;
	float m_distance;
	float m_angle;
};

#endif // __AGMovementControlMethod_h__
