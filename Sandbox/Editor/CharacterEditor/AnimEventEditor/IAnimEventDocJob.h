//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __IANIMEVENTDOCJOB_H__
#define __IANIMEVENTDOCJOB_H__

class IAnimEventDocChangeTarget;

class IAnimEventDocJob
{
public:
	virtual ~IAnimEventDocJob() = 0 {};

	virtual void Perform(IAnimEventDocChangeTarget* pTarget) = 0;
};

#endif //__IANIMEVENTDOCJOB_H__
