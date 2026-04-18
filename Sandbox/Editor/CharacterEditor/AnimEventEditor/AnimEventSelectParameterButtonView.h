//---------------------------------------------------------------------------
// Copyright 2006 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __ANIMEVENTSELECTPARAMETERBUTTONVIEW_H__
#define __ANIMEVENTSELECTPARAMETERBUTTONVIEW_H__

#include "IAnimEventView.h"

class AnimEventSelectParameterButtonView : public IAnimEventView
{
public:
	enum Type
	{
		TYPE_SOUND,
		TYPE_EFFECT
	};

	virtual void OnPressed() = 0;

	static AnimEventSelectParameterButtonView* Create(CButton& button, Type type);
};

#endif //__ANIMEVENTSELECTPARAMETERBUTTONVIEW_H__
