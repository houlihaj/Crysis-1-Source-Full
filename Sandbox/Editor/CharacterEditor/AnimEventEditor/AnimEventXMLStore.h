//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __ANIMEVENTXMLSTORE_H__
#define __ANIMEVENTXMLSTORE_H__

#include "IAnimEventStore.h"

class AnimEventXMLStore : public IAnimEventStore
{
public:
	virtual void SetFileName(const string& sFileName) = 0;
	static AnimEventXMLStore* Create();
};

#endif //__ANIMEVENTXMLSTORE_H__
