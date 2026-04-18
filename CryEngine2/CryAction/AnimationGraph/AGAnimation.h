#ifndef __AGANIMATION_H__
#define __AGANIMATION_H__

#pragma once

#include "IAnimationStateNode.h"

class CAGAnimation : public IAnimationStateNodeFactory, public IAnimationStateNode
{
public:
	inline CAGAnimation( int layer ) : m_layer(layer), m_ensureInStack(false) {}
	inline virtual ~CAGAnimation() {}

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
		AG_LT_BEGIN_FACTORY(CAGAnimation);
			AG_LT_ELEM(m_layer);
			AG_LT_ELEM(m_ensureInStack);
			AG_LT_ELEM(m_forceEnableAiming);
			AG_LT_ELEM(m_forceLeaveWhenFinished);
			AG_LT_ELEM(m_stopCurrentAnimation);
			AG_LT_ELEM(m_directToLayer0);
			AG_LT_ELEM(m_oneShot);
			AG_LT_ELEM(m_interruptCurrentAnim);
			AG_LT_ELEM(m_stickyOutTime);
			AG_LT_ELEM(m_animations[0]);
			AG_LT_ELEM(m_animations[1]);
			AG_LT_ELEM(m_aimPoses[0]);
			AG_LT_ELEM(m_aimPoses[1]);
			AG_LT_ELEM(m_SPSpeedMultiplier);
			AG_LT_ELEM(m_MPSpeedMultiplier);
			AG_LT_ELEM(m_stayInStateUntil);
			AG_LT_ELEM(m_forceInStateUntil);
		AG_LT_END();
	}
	// ~IAnimationStateNodeFactory

	// special hook for CAnimationGraphState
	SAnimationDesc GetDesc( IEntity * pEntity, const CAnimationGraphState* pState );
	Vec2 GetMinMaxSpeed( IEntity * pEntity );

	virtual void GetFactoryMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
	virtual void GetStateMemoryStatistics(ICrySizer * s)
	{
	}

protected:
	int GetLayer() { return m_layer; }
	virtual CryCharAnimationParams GetAnimationParams( SAnimationStateData& data );

private:
	bool IsTopOfStack( const SAnimationStateData& data );
	bool IsActivated( const SAnimationStateData& data, bool * pFound = NULL );

	// remember to update IsLessThan, SerializeAsFile
	int m_layer;
	bool m_ensureInStack;
	bool m_forceEnableAiming;
	bool m_forceLeaveWhenFinished;
	bool m_stopCurrentAnimation;
	bool m_directToLayer0;
	bool m_oneShot;
	bool m_interruptCurrentAnim;
	float m_stickyOutTime;
	CCryName m_animations[2];
	CCryName m_aimPoses[2];
	float m_SPSpeedMultiplier; // single-player speed multiplier
	float m_MPSpeedMultiplier; // multi-player speed multiplier
	float m_stayInStateUntil;
	float m_forceInStateUntil;

	// kludge
	CTimeValue m_animationLength;
};

template <int N>
class CAGAnimationLayer : public CAGAnimation
{
public:
	CAGAnimationLayer() : CAGAnimation(N) {}
};

#endif
