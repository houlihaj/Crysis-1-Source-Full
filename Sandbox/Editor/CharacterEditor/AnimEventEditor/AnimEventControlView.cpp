//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------

#include "StdAfx.h"
#include "IAnimEventDoc.h"
#include "AnimEventControlView.h"
#include "AnimEventDocVisitors.h"
#include "IAnimEventDocJob.h"
#include "IAnimEventDocChangeTarget.h"
#include "IAnimEventDocChangeDescription.h"
#include "IAnimEventVisitable.h"
#include "AnimEventJobs.h"

class AnimEventControlViewImpl : public AnimEventControlView
{
public:
	AnimEventControlViewImpl();
	virtual ~AnimEventControlViewImpl();

	virtual void ReferenceDoc(IAnimEventDoc* pDoc);
	virtual void Refresh(IAnimEventDocChangeDescription* pDescription);
	virtual void SetTime(float fTime);
	virtual void AddListener(IAnimEventControlViewListener* pListener);

private:
	IAnimEventDoc* pDoc;
	float fTime;
	std::set<IAnimEventControlViewListener*> listeners;
};

AnimEventControlView* AnimEventControlView::Create()
{
	return new AnimEventControlViewImpl();
}

AnimEventControlViewImpl::AnimEventControlViewImpl()
:	pDoc(pDoc),
	fTime(0.0f)
{
}

AnimEventControlViewImpl::~AnimEventControlViewImpl()
{
}

void AnimEventControlViewImpl::ReferenceDoc(IAnimEventDoc* pDoc)
{
	this->pDoc = pDoc;
	if (this->pDoc != 0)
		this->pDoc->SetTime(this->fTime);
}

void AnimEventControlViewImpl::Refresh(IAnimEventDocChangeDescription* pDescription)
{
	// Check whether the time has changed.
	if (pDescription != 0)
	{
		if (pDescription->HasTimeChanged())
		{
			// Notify listeners.
			for_each(this->listeners.begin(), this->listeners.end(), std::bind2nd(std::mem_fun<void, IAnimEventControlViewListener>(&IAnimEventControlViewListener::OnAnimEventControlViewTimeChanged), this->pDoc->GetTime()));
		}
	}
}

void AnimEventControlViewImpl::SetTime(float fTime)
{
	this->fTime = fTime;
	if (this->pDoc != 0)
	{
		this->pDoc->SetTime(this->fTime);

		// Get the selected items.
		AnimEventBuildSelectionSetVisitor* pVisitor = AnimEventBuildSelectionSetVisitor::Create();
		this->pDoc->GetVisitable()->Visit(pVisitor);

		// Check whether we need to change the times of events.
		const std::set<int> selectedIDs = pVisitor->GetSelectedIDS();
		if (!selectedIDs.empty())
		{
			this->pDoc->AcquireJob(new AnimEventChangeTimeJob(this->fTime, selectedIDs));
		}

		delete pVisitor;
	}
}

void AnimEventControlViewImpl::AddListener(IAnimEventControlViewListener* pListener)
{
	this->listeners.insert(pListener);
}
