//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __ANIMEVENTDOCVISITORS_H__
#define __ANIMEVENTDOCVISITORS_H__

#include "IAnimEventDocVisitor.h"

class AnimEventBuildSelectionSetVisitor : public IAnimEventDocVisitor
{
public:
	static AnimEventBuildSelectionSetVisitor* Create();

	virtual const std::set<int>& GetSelectedIDS() = 0;
};

#endif //__ANIMEVENTDOCVISITORS_H__
