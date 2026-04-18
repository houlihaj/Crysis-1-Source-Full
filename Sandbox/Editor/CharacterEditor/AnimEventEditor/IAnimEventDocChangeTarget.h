//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __IANIMEVENTDOCCHANGETARGET_H__
#define __IANIMEVENTDOCCHANGETARGET_H__

class IAnimEventDocChangeTarget
{
public:
	virtual ~IAnimEventDocChangeTarget() = 0 {};

	virtual int AddAnimEvent(const char* szName, const char* szAnimPath) = 0;
	virtual void RemoveAnimEvent(int nEventID) = 0;
	virtual void SetAnimEventTime(int nEventID, float fTime) = 0;
	virtual void SetEventSelected(int nEventID, bool bSelected) = 0;
	virtual void SetEventName(int nEventID, const char* szName) = 0;
	virtual void SetEventParameter(int nEventID, const char* szName) = 0;
	virtual void SetEventBone(int nEventID, const char* szBone) = 0;
	virtual void SetAnimEventOffset(int nEventID, const Vec3& vOffset) = 0;
	virtual void SetAnimEventDir(int nEventID, const Vec3& vDir) = 0;
};

#endif //__IANIMEVENTDOCCHANGETARGET_H__
