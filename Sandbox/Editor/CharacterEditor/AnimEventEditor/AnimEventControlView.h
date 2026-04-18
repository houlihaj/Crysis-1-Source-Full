//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __ANIMEVENTCONTROLVIEW_H__
#define __ANIMEVENTCONTROLVIEW_H__

#include "IAnimEventView.h"

class IAnimEventControlViewListener
{
public:
	virtual void OnAnimEventControlViewTimeChanged(float fTime) = 0;
};

class AnimEventControlView : public IAnimEventView
{
public:
	virtual void SetTime(float fTime) = 0;
	virtual void AddListener(IAnimEventControlViewListener* pListener) = 0;

	static AnimEventControlView* Create();
};

#endif //__ANIMEVENTCONTROLVIEW_H__
