#ifndef __AGTRANSITIONPARAMS_H__
#define __AGTRANSITIONPARAMS_H__

#pragma once

#include "AGParams.h"

class CAGTransitionParams : public CAGParams
{
public:
	CAGTransitionParams( int layer ) : CAGParams(layer) {}

	// IAnimationStateNode
	virtual void EnterState( SAnimationStateData& data, bool dueToRollback );
	virtual void LeaveState( SAnimationStateData& data );
	virtual const Params * GetParameters();
	// ~IAnimationStateNode

	// IAnimationStateNodeFactory
	virtual bool Init( const XmlNodeRef& node, IAnimationGraphPtr );
	virtual const char * GetName();
	virtual void SerializeAsFile(bool reading, AG_FILE *file)
	{
		CAGParams::SerializeAsFile(reading, file);
		FileSerializationHelper h(reading, file);
		h.Value(&m_overrides.overrideStartAfter);
		h.Value(&m_overrides.overrideStartAtKeyFrame);
		h.Value(&m_overrides.startAtKeyFrame);
	}
	virtual bool IsLessThan( IAnimationStateNodeFactory * pFactory )
	{
		AG_LT_BEGIN_PARENT(CAGTransitionParams, CAGParams);
			AG_LT_ELEM(m_overrides);
		AG_LT_END();
	}
	// ~IAnimationStateNodeFactory

private:
	SAnimationOverrides m_overrides;
};

template <int LAYER>
class CAGTransitionParamsLayer : public CAGTransitionParams
{
public:
	CAGTransitionParamsLayer() : CAGTransitionParams(LAYER) {}
};

#endif
