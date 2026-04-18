//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __IANIMEVENTVISITABLE_H__
#define __IANIMEVENTVISITABLE_H__

class IAnimEventDocVisitor;

class IAnimEventVisitable
{
public:
	virtual void Visit(IAnimEventDocVisitor* pVisitor) = 0;
};

#endif //__IANIMEVENTVISITABLE_H__
