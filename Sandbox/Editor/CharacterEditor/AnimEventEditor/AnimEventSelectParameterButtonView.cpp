//---------------------------------------------------------------------------
// Copyright 2006 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------

#include "StdAfx.h"
#include "IAnimEventDoc.h"
#include "AnimEventSelectParameterButtonView.h"
#include "IAnimEventDocJob.h"
#include "IAnimEventVisitable.h"
#include "AnimEventDocVisitors.h"
#include "IAnimEventDocChangeTarget.h"
#include "AnimEventJobs.h"
#include "Particles\ParticleManager.h"
#include "Particles\ParticleItem.h"

class AnimEventSelectParameterButtonViewImpl : public AnimEventSelectParameterButtonView
{
public:
	AnimEventSelectParameterButtonViewImpl(CButton& button, Type type);
	virtual ~AnimEventSelectParameterButtonViewImpl();

	virtual void ReferenceDoc(IAnimEventDoc* pDoc);
	virtual void Refresh(IAnimEventDocChangeDescription* pDescription);
	virtual void OnPressed();

private:
	IAnimEventDoc* pDoc;
	CButton& button;
	Type type;
};

class AnimEventFindParameterVisitor : public IAnimEventDocVisitor
{
public:
	virtual ~AnimEventFindParameterVisitor() {}

	virtual void VisitEvent(int nID, const char* szAnimPath, const char* szName, float fTime, const char* szParameter, const char* szBone, const Vec3& vOffset, const Vec3& vDir, bool bSelected);

	virtual const string& GetParameter() {return this->sParameter;}

private:
	string sParameter;
};

void AnimEventFindParameterVisitor::VisitEvent(int nID, const char* szAnimPath, const char* szName, float fTime, const char* szParameter, const char* szBone, const Vec3& vOffset, const Vec3& vDir, bool bSelected)
{
	if (bSelected)
		this->sParameter = szParameter;
}

AnimEventSelectParameterButtonView* AnimEventSelectParameterButtonView::Create(CButton& button, Type type)
{
	return new AnimEventSelectParameterButtonViewImpl(button, type);
}

AnimEventSelectParameterButtonViewImpl::AnimEventSelectParameterButtonViewImpl(CButton& button, Type type)
:	pDoc(0),
	button(button),
	type(type)
{
}

AnimEventSelectParameterButtonViewImpl::~AnimEventSelectParameterButtonViewImpl()
{
}

void AnimEventSelectParameterButtonViewImpl::ReferenceDoc(IAnimEventDoc* pDoc)
{
	this->pDoc = pDoc;
}

void AnimEventSelectParameterButtonViewImpl::Refresh(IAnimEventDocChangeDescription* pDescription)
{
	// Get the selected items.
	AnimEventBuildSelectionSetVisitor* pVisitor = AnimEventBuildSelectionSetVisitor::Create();
	this->pDoc->GetVisitable()->Visit(pVisitor);

	// Check whether we need to change the parameters of events.
	this->button.EnableWindow(!pVisitor->GetSelectedIDS().empty());
}

void AnimEventSelectParameterButtonViewImpl::OnPressed()
{
	// Find an existing parameter to use as a default selection for the sound browser.
	AnimEventFindParameterVisitor parameterVisitor;
	this->pDoc->GetVisitable()->Visit(&parameterVisitor);

	// Allow the user to select a sound.
	switch (this->type)
	{
	case TYPE_SOUND:
		{
			CString sSoundFileName = parameterVisitor.GetParameter().c_str();
			if (CFileUtil::SelectSingleFile(EFILE_TYPE_SOUND, sSoundFileName))
			{
				// Get the selected items.
				AnimEventBuildSelectionSetVisitor* pVisitor = AnimEventBuildSelectionSetVisitor::Create();
				this->pDoc->GetVisitable()->Visit(pVisitor);

				// Check whether we need to change the parameters of events.
				const std::set<int> selectedIDs = pVisitor->GetSelectedIDS();
				if (!selectedIDs.empty())
				{
					this->pDoc->AcquireJob(new AnimEventSetParameterNameJob(sSoundFileName, "sound", selectedIDs));
				}

				delete pVisitor;
			}
		}
		break;

	case TYPE_EFFECT:
		{
			// Find the currently selected particle effect, if any.
			CParticleManager* pPartManager = GetIEditor()->GetParticleManager();
			CParticleItem* pParticleItem = (pPartManager ? (CParticleItem*)pPartManager->GetSelectedItem() : 0);
			if (!pParticleItem)
			{
				CryMessageBox("No effect selected - Please first select a particle effect in the database view.", "Cannot Find Effect", MB_OK | MB_ICONERROR);
			}
			else
			{
				IParticleEffect* pEffect = pParticleItem->GetEffect();
				const char* effectName = (pEffect ? pEffect->GetName() : 0);
				if (!effectName)
				{
					CryMessageBox("Unable to get name from particle effect - reason unknown.", "Cannot Find Effect", MB_OK | MB_ICONERROR);
				}
				else
				{
					// Get the selected items.
					AnimEventBuildSelectionSetVisitor* pVisitor = AnimEventBuildSelectionSetVisitor::Create();
					this->pDoc->GetVisitable()->Visit(pVisitor);

					// Check whether we need to change the parameters of events.
					const std::set<int> selectedIDs = pVisitor->GetSelectedIDS();
					if (!selectedIDs.empty())
					{
						this->pDoc->AcquireJob(new AnimEventSetParameterNameJob(effectName, "effect", selectedIDs));
					}

					delete pVisitor;
				}
			}
		}
		break;
	}
}
