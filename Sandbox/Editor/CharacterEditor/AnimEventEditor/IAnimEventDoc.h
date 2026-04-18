//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __IANIMEVENTDOC_H__
#define __IANIMEVENTDOC_H__

class IAnimEventDocJob;
class IAnimEventVisitable;
class IAnimEventDocListener;

class IAnimEventDoc
{
public:
	virtual ~IAnimEventDoc() = 0 {};
	virtual IAnimEventVisitable* GetVisitable() = 0;
	virtual void AcquireJob(IAnimEventDocJob* pJob) = 0;
	virtual void AddListener(IAnimEventDocListener* pListener) = 0;
	virtual float GetTime() = 0;
	virtual void SetTime(float fTime) = 0;
	virtual CString GetAnim() = 0;
	virtual void SetAnim(const char* szAnimName) = 0;
	virtual void Build(IAnimEventVisitable* pVisitable) = 0;
};

#endif //__IANIMEVENTDOC_H__
