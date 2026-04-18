#ifndef __AGSound_H__
#define __AGSound_H__

#pragma once

#include "IAnimationStateNode.h"
#include "AnimationGraphCVars.h"

class CAGSound;

class CAGSoundFactory : public IAnimationStateNodeFactory
{
  friend class CAGSound;

public:
  CAGSoundFactory() {}

   // IAnimationStateNodeFactory
  virtual bool Init( const XmlNodeRef& node, IAnimationGraphPtr );
  virtual void Release();
  virtual IAnimationStateNode * Create();
  virtual const char * GetCategory();
  virtual const char * GetName();
  virtual const Params * GetParameters();
	virtual void SerializeAsFile(bool reading, AG_FILE *file);

	bool IsLessThan( IAnimationStateNodeFactory * pFactory )
	{
		AG_LT_BEGIN(CAGSoundFactory);
			AG_LT_ELEM(m_sound);
			AG_LT_ELEM(m_delay);
			AG_LT_ELEM(m_input);
			AG_LT_ELEM(m_soundInput);
			AG_LT_ELEM(m_mult);
			AG_LT_ELEM(m_offset);
			AG_LT_ELEM(m_soundId);
			AG_LT_ELEM(m_soundType);
			AG_LT_ELEM(m_bVoiceSound);
		AG_LT_END();
	}
  // ~IAnimationStateNodeFactory

	virtual void GetFactoryMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

private:
  void PerformOp( float input, const SAnimationStateData& data );
  
  int DebugVal()
  {
		return CAnimationGraphCVars::Get().m_logSounds;
  }
  
  CCryName m_sound;
  float m_delay;

  CCryName m_input;
  CCryName m_soundInput; 
  float m_mult, m_offset;

  tSoundID m_soundId;
	int m_soundType;
	bool m_bVoiceSound;
};

class CAGSound : public IAnimationStateNode
{
public:
  CAGSound( CAGSoundFactory * pFactory );

  // IAnimationStateNode
  virtual void EnterState( SAnimationStateData& data, bool dueToRollback );
	virtual EHasEnteredState HasEnteredState( SAnimationStateData& data ) { return eHES_Instant; }
  virtual bool CanLeaveState( SAnimationStateData& data );
  virtual void LeaveState( SAnimationStateData& data );
  virtual void Update( SAnimationStateData& data );
  virtual void GetCompletionTimes( SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky );
  virtual IAnimationStateNodeFactory * GetFactory() { return m_pFactory; }
	virtual void EnteredState(SAnimationStateData& data ) {}
	virtual void LeftState(SAnimationStateData& data, bool wasEntered) {delete this;}
	virtual void DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement ) {}
  // ~IAnimationStateNode

	virtual void GetStateMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

private:
  void PlaySound( SAnimationStateData& data );

  CAGSoundFactory * m_pFactory;
  int m_inputId;
  float m_delay;
  
};

#endif
