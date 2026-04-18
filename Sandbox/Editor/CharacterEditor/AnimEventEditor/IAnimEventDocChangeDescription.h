//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __IANIMEVENTDOCCHANGEDESCRIPTION_H__
#define __IANIMEVENTDOCCHANGEDESCRIPTION_H__

class IAnimEventDocChangeDescription
{
public:
	virtual ~IAnimEventDocChangeDescription() = 0 {}

	virtual bool HasSelectionChanged() = 0;
	virtual bool HasEventListChanged() = 0;
	virtual bool HaveEventsChanged() = 0;
	virtual bool HasTimeChanged() = 0;
	virtual bool HasAnimChanged() = 0;
};

#endif //__IANIMEVENTDOCCHANGEDESCRIPTION_H__
