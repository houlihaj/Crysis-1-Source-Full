#include "StdAfx.h"
#include "AGForceStartImmediately.h"

void CAGForceStartImmediately::LeaveState( SAnimationStateData& data )
{
	// TODO: should this be for all/some layers?
	data.overrides[0].ClearOverrides();
}

void CAGForceStartImmediately::GetCompletionTimes( SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky )
{
	hard = sticky = 0.0f;
}

const CAGForceStartImmediately::Params * CAGForceStartImmediately::GetParameters()
{
	static const Params params[] = 
	{
		{0}
	};
	return params;
}

bool CAGForceStartImmediately::Init( const XmlNodeRef& node, IAnimationGraphPtr pGraph )
{
	return true;
}

void CAGForceStartImmediately::Release()
{
	delete this;
}

IAnimationStateNode * CAGForceStartImmediately::Create()
{
	return this;
}

const char * CAGForceStartImmediately::GetCategory()
{
	return "SpecialOverrides";
}

const char * CAGForceStartImmediately::GetName()
{
	return "ForceStartImmediately";
}
