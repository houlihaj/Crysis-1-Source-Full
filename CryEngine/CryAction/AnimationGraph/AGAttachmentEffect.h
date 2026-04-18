#ifndef __AGATTACHMENTEFFECT_H__
#define __AGATTACHMENTEFFECT_H__

#pragma once

#include "IAnimationStateNode.h"

class CAGAttachmentEffect;

class CAGAttachmentEffectFactory : public IAnimationStateNodeFactory
{
	friend class CAGAttachmentEffect;

public:
	// IAnimationStateNodeFactory
	virtual bool Init( const XmlNodeRef& node, IAnimationGraphPtr );
	virtual void Release();
	virtual IAnimationStateNode * Create();
	virtual const char * GetCategory();
	virtual const char * GetName();
	virtual const Params * GetParameters();
	virtual void SerializeAsFile(bool reading, AG_FILE *file);

	virtual bool IsLessThan( IAnimationStateNodeFactory * pFactory )
	{
		AG_LT_BEGIN(CAGAttachmentEffectFactory);
			AG_LT_ELEM(m_attachment);
			AG_LT_ELEM(m_pos.x);
			AG_LT_ELEM(m_pos.y);
			AG_LT_ELEM(m_pos.z);
			AG_LT_ELEM(m_effect);
			AG_LT_ELEM(m_sizeInput);
			AG_LT_ELEM(m_countInput);
			AG_LT_ELEM(m_sizeOffset);
			AG_LT_ELEM(m_sizeMult);
			AG_LT_ELEM(m_countOffset);
			AG_LT_ELEM(m_countMult);
		AG_LT_END();
	}

	virtual void GetFactoryMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
		s->Add(m_attachment);
		s->Add(m_effect);
		s->Add(m_sizeInput);
		s->Add(m_countInput);
	}
	// ~IAnimationStateNodeFactory

private:
	void PerformOp( int sizeInput, int countInput, const SAnimationStateData& data );
  const string& GetAttachmentName(){ return m_attachment; }

  string m_attachment;  
  Vec3 m_pos;  
  string m_effect;    

  string m_sizeInput;
  string m_countInput; 	

  float m_sizeOffset, m_sizeMult;
  float m_countOffset, m_countMult;

  CEffectAttachment* m_pEA;
};

class CAGAttachmentEffect : public IAnimationStateNode
{
public:
	CAGAttachmentEffect( CAGAttachmentEffectFactory * pFactory );

	// IAnimationStateNode
	virtual void EnterState( SAnimationStateData& data, bool dueToRollback );
	virtual EHasEnteredState HasEnteredState( SAnimationStateData& data ) { return eHES_Instant; }
	virtual bool CanLeaveState( SAnimationStateData& data );
	virtual void LeaveState( SAnimationStateData& data );
	virtual void EnteredState( SAnimationStateData& data );
	virtual void LeftState( SAnimationStateData& data, bool wasEntered );
	virtual void Update( SAnimationStateData& data );
	virtual void GetCompletionTimes( SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky );
	virtual IAnimationStateNodeFactory * GetFactory() { return m_pFactory; }
	virtual void DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement ) {}
	// ~IAnimationStateNode

	virtual void GetStateMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

private:
	CAGAttachmentEffectFactory * m_pFactory;
	int m_countInputId;
  int m_sizeInputId;
 
};

#endif
