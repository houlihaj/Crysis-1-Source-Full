//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------

#include "StdAfx.h"
#include "AnimEventDocVisitors.h"

class AnimEventBuildSelectionSetVisitorImpl : public AnimEventBuildSelectionSetVisitor
{
public:
	AnimEventBuildSelectionSetVisitorImpl();
	virtual ~AnimEventBuildSelectionSetVisitorImpl();

	virtual void VisitEvent(int nID, const char* szAnimPath, const char* szName, float fTime, const char* szParameter, const char* szBone, const Vec3& vOffset, const Vec3& vDir, bool bSelected);

	virtual const std::set<int>& GetSelectedIDS();

private:
	std::set<int> selectedIds;
};

AnimEventBuildSelectionSetVisitor* AnimEventBuildSelectionSetVisitor::Create()
{
	return new AnimEventBuildSelectionSetVisitorImpl;
}

AnimEventBuildSelectionSetVisitorImpl::AnimEventBuildSelectionSetVisitorImpl()
{
}

AnimEventBuildSelectionSetVisitorImpl::~AnimEventBuildSelectionSetVisitorImpl()
{
}

void AnimEventBuildSelectionSetVisitorImpl::VisitEvent(int nID, const char* szAnimPath, const char* szName, float fTime, const char* szParameter, const char* szBone, const Vec3& vOffset, const Vec3& vDir, bool bSelected)
{
	if (bSelected)
		this->selectedIds.insert(nID);
}

const std::set<int>& AnimEventBuildSelectionSetVisitorImpl::GetSelectedIDS()
{
	return this->selectedIds;
}
