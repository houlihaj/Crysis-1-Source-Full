#ifndef __MGENDMUSIC_H__
#define __MGENDMUSIC_H__

#pragma once

#include "IAnimationStateNode.h"

class CMGEndMusic : public IAnimationStateNodeFactory, public IAnimationStateNode
{
public:
	CMGEndMusic();
	virtual ~CMGEndMusic();

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
	virtual void SerializeAsFile(bool reading, AG_FILE *file) { SerializeAsFile_NodeBase(reading, file); }

	virtual bool IsLessThan( IAnimationStateNodeFactory * pFactory )
	{
		AG_LT_BEGIN(CMGEndMusic);
			AG_LT_ELEM(m_nFadeType);
			AG_LT_ELEM(m_fForceLimit);
			AG_LT_ELEM(m_sThemeName);
		AG_LT_END();
	}
	// ~IAnimationStateNodeFactory

	virtual void GetStateMemoryStatistics(ICrySizer * s)
	{
	}
	virtual void GetFactoryMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
		s->Add(m_sThemeName);
	}

private:
	int m_nFadeType;
	float m_fForceLimit;
	string m_sThemeName;
};

#endif
