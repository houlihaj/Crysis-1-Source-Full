//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __IANIMEVENTDOCVISITOR_H__
#define __IANIMEVENTDOCVISITOR_H__

class IAnimEventDocVisitor
{
public:
	virtual ~IAnimEventDocVisitor() = 0 {};

	virtual void VisitEvent(int nID, const char* szAnimPath, const char* szName, float fTime, const char* szParameter, const char* szBone, const Vec3& vOffset, const Vec3& vDir, bool bSelected) = 0;
};

#endif //__IANIMEVENTDOCVISITOR_H__
