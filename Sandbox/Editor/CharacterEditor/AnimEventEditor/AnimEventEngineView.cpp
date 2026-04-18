//---------------------------------------------------------------------------
// Copyright 2006 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------

#include "stdafx.h"
#include <ICryAnimation.h>
#include "AnimEventEngineView.h"
#include "IAnimEventDoc.h"
#include "IAnimEventVisitable.h"
#include "IAnimEventDocVisitor.h"
#include "IAnimEventDocChangeDescription.h"
#include <map>

class AnimEventEngineDocVisitor : public IAnimEventDocVisitor
{
public:
	AnimEventEngineDocVisitor();
	virtual ~AnimEventEngineDocVisitor();

	virtual void VisitEvent(int nID, const char* szAnimPath, const char* szName, float fTime, const char* szParameter, const char* szBone, const Vec3& vOffset, const Vec3& vDir, bool bSelected);

private:
	typedef std::map<string, int> AnimationAssetMap;
	AnimationAssetMap animationAssetMap;
	IAnimEvents* pAnimEventsManager;
};

AnimEventEngineView::AnimEventEngineView(CModelViewportCE* pModelViewportCE)
:	pDoc(0),
	pModelViewportCE(pModelViewportCE)
{
}

AnimEventEngineView::~AnimEventEngineView()
{
}

void AnimEventEngineView::ReferenceDoc(IAnimEventDoc* pDoc)
{
	this->pDoc = pDoc;
}

void AnimEventEngineView::Refresh(IAnimEventDocChangeDescription* pDescription)
{
	// Check whether it is necessary to update the events.
	bool bEventsMayHaveChanged = true;
	if (pDescription)
		bEventsMayHaveChanged = pDescription->HasEventListChanged() || pDescription->HaveEventsChanged() || pDescription->HasTimeChanged();

	// Create a visitor and visit all the animation events in the document.
	if (bEventsMayHaveChanged)
	{
		AnimEventEngineDocVisitor visitor;
		this->pDoc->GetVisitable()->Visit(&visitor);
	}
}

AnimEventEngineDocVisitor::AnimEventEngineDocVisitor()
:	pAnimEventsManager(GetISystem()->GetIAnimationSystem()->GetIAnimEvents())
{
}

AnimEventEngineDocVisitor::~AnimEventEngineDocVisitor()
{
}

void AnimEventEngineDocVisitor::VisitEvent(int nID, const char* szAnimPath, const char* szName, float fTime, const char* szParameter, const char* szBone, const Vec3& vOffset, const Vec3& vDir, bool bSelected)
{
	// Look for this animation asset in our asset map. If it is not there, then this is the first event for this
	// animation that we have encountered in the document.
	AnimationAssetMap::iterator itAssetEntry = this->animationAssetMap.find(szAnimPath);
	if (itAssetEntry == this->animationAssetMap.end())
	{
		// This is the first time we have encountered this animation. We should clear all global events registered for this
		// animation, so that we can re-add them with the updated information. First we need to find the global ID.
		int animationID = this->pAnimEventsManager->GetGlobalAnimID(szAnimPath);

		// Delete all the events for this animation.
		this->pAnimEventsManager->DeleteAllEventsForAnimation(animationID);

		// Add the item to the map.
		itAssetEntry = this->animationAssetMap.insert(std::make_pair(szAnimPath, animationID)).first;
	}

	// Add this event to the global event list.
	int animationID = (*itAssetEntry).second;
	this->pAnimEventsManager->AddAnimEvent(animationID, szName, szParameter, szBone, fTime, vOffset, vDir);
}
