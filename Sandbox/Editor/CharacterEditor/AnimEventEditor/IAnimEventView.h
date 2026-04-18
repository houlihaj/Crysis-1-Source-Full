//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __IANIMEVENTVIEW_H__
#define __IANIMEVENTVIEW_H__

class IAnimEventDoc;
class IAnimEventDocChangeDescription;

class IAnimEventView
{
public:
	virtual ~IAnimEventView() = 0 {};

	virtual void ReferenceDoc(IAnimEventDoc* pDoc) = 0;
	virtual void Refresh(IAnimEventDocChangeDescription* pDescription) = 0;
};

#endif //__IANIMEVENTVIEW_H__
