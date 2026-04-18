//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __IANIMEVENTDOCLISTENER_H__
#define __IANIMEVENTDOCLISTENER_H__

class IAnimEventDocChangeDescription;

class IAnimEventDocListener
{
public:
	virtual ~IAnimEventDocListener() = 0 {};

	virtual void DocChanged(IAnimEventDocChangeDescription* pDescription) = 0;
};

#endif //__IANIMEVENTDOCLISTENER_H__
