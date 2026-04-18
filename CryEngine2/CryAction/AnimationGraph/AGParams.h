#ifndef __AGPARAMS_H__
#define __AGPARAMS_H__

#pragma once

#include "IAnimationStateNode.h"

class CAGParams : public IAnimationStateNodeFactory, public IAnimationStateNode
{
public:
	CAGParams( int layer ) : m_layer(layer) {}
	inline virtual ~CAGParams() {};

	// IAnimationStateNode
	virtual void EnterState( SAnimationStateData& data, bool dueToRollback );
	virtual EHasEnteredState HasEnteredState( SAnimationStateData& data ) { return eHES_Instant; }
	virtual bool CanLeaveState( SAnimationStateData& data );
	virtual void LeaveState( SAnimationStateData& data );
	virtual void EnteredState(SAnimationStateData& data ) {}
	virtual void LeftState(SAnimationStateData& data, bool wasEntered) {}
	void GetCompletionTimes( SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky );
	virtual const Params * GetParameters();
	virtual IAnimationStateNodeFactory * GetFactory() { return this; }
	virtual void DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement ) {}
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
		AG_LT_BEGIN_FACTORY(CAGParams);
			AG_LT_ELEM(m_structure);
			// must be last... hacky
			else if (memcmp(&m_preParams, &r.m_preParams, sizeof(m_preParams)) < 0)
				return true;
		AG_LT_END();
	}
	virtual bool GetForceReentering() const { return true; }
	// ~IAnimationStateNodeFactory

  bool IsLooped(){ return (m_preParams.m_nFlags & CA_LOOP_ANIMATION)==CA_LOOP_ANIMATION; }

	virtual void GetStateMemoryStatistics(ICrySizer * s)
	{
	}
	virtual void GetFactoryMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

protected:
	bool SerializeParams( const char * prefix, CryCharAnimationParams& params, const XmlNodeRef& node );
	void AddParams( const char * prefix, std::vector<Params>& params );
	void EndParams( std::vector<Params>& params );

	const int m_layer;

private:
	template <class F>
	static void ProcessParams( F& f );

	CryCharAnimationParams m_preParams;
	CCryName m_structure;

	class Serializer;
	class Adder;

	static const char * GetString( string );
};

template <int LAYER>
class CAGParamsLayer : public CAGParams
{
public:
	CAGParamsLayer() : CAGParams(LAYER) {}
};

#endif
