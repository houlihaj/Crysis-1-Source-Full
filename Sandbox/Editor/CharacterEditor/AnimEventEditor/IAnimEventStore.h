//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __IANIMEVENTSTORE_H__
#define __IANIMEVENTSTORE_H__

class IAnimEventDoc;

class IAnimEventStore
{
public:
	virtual ~IAnimEventStore() {};

	virtual bool Save(IAnimEventDoc* pDoc) = 0;
	virtual bool Load(IAnimEventDoc* pDoc) = 0;
	virtual string GetErrorString() = 0;
};

#endif //__IANIMEVENTSTORE_H__
