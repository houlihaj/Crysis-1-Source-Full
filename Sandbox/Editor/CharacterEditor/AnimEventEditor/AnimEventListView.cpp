//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------

#include "StdAfx.h"
#include "AnimEventListView.h"
#include "IAnimEventView.h"
#include "IAnimEventDocVisitor.h"
#include "IAnimEventDoc.h"
#include "Controls/ObjectListCtrl.h"
#include "IAnimEventDocChangeDescription.h"
#include "IAnimEventDocJob.h"
#include "IAnimEventDocChangeTarget.h"
#include "IAnimEventVisitable.h"
#include "IAnimEventDocJob.h"
#include "AnimEventJobs.h"

class AnimEventListView : public IAnimEventView, public IObjectListCtrlListener
{
	friend class AnimEventSelectionChangedJob;
public:

	AnimEventListView(CObjectListCtrl* pListCtrl, const std::vector<string>& bones);
	virtual ~AnimEventListView();

	virtual void ReferenceDoc(IAnimEventDoc* pDoc);
	virtual void Refresh(IAnimEventDocChangeDescription* pDescription);

	virtual void OnObjectListCtrlSelectionRangeChanged(CObjectListCtrl* pCtrl, int nFirstSelectedItem, int nLastSelectedItem, bool bSelected);
	virtual void OnObjectListCtrlLabelChanged(CObjectListCtrl* pCtrl, int nItem, int nSubItem, const char* szNewText);
	virtual void OnObjectListCtrlItemActivated(CObjectListCtrl* pCtrl, int nItem);

private:

	class ListCtrlItem : public IListCtrlObject
	{
	public:
		ListCtrlItem();
		ListCtrlItem(AnimEventListView* pView, int nID, const char* szAnimPath, const char* szName, float fTime, const char* szParameter, const char* szBone, const Vec3& vOffset, const Vec3& vDir);
		virtual ~ListCtrlItem();

		virtual CString GetText(int nSubItem);
		virtual bool IsSelected();
		virtual CSubeditListCtrl::EditStyle GetEditStyle(int subitem);
		virtual void GetOptions(int subitem, std::vector<string>& options, string& currentOption);

		int nID;
		CString sAnimPath;
		CString sName;
		float fTime;
		CString sParameter;
		CString sBone;
		Vec3 vOffset;
		Vec3 vDir;
		bool bSelected;
		AnimEventListView* pView;
	};

	class BuildFreshVisitor : public IAnimEventDocVisitor
	{
	public:
		BuildFreshVisitor(AnimEventListView* pView);
		virtual ~BuildFreshVisitor();

		virtual void VisitEvent(int nID, const char* szAnimPath, const char* szName, float fTime, const char* szParameter, const char* szBone, const Vec3& vOffset, const Vec3& vDir, bool bSelected);

	private:
		CString sAnimPath;
		AnimEventListView* pView;
	};

	class UpdateSelectionVisitor : public IAnimEventDocVisitor
	{
	public:
		UpdateSelectionVisitor(AnimEventListView* pView);
		virtual ~UpdateSelectionVisitor();

		virtual void VisitEvent(int nID, const char* szAnimPath, const char* szName, float fTime, const char* szParameter, const char* szBone, const Vec3& vOffset, const Vec3& vDir, bool bSelected);

	private:
		AnimEventListView* pView;
	};

	class UpdateEventVisitor : public IAnimEventDocVisitor
	{
	public:
		UpdateEventVisitor(AnimEventListView* pView);
		virtual ~UpdateEventVisitor();

		virtual void VisitEvent(int nID, const char* szAnimPath, const char* szName, float fTime, const char* szParameter, const char* szBone, const Vec3& vOffset, const Vec3& vDir, bool bSelected);

	private:
		AnimEventListView* pView;
	};

	CObjectListCtrl* pListCtrl;
	IAnimEventDoc* pDoc;
	std::map<int, ListCtrlItem> items;
	std::vector<int> ids;
	std::vector<string> bones;
};

class AnimEventSelectionChangedJob : public IAnimEventDocJob
{
public:
	AnimEventSelectionChangedJob(AnimEventListView* pView);
	virtual ~AnimEventSelectionChangedJob();

	virtual void Perform(IAnimEventDocChangeTarget* pTarget);

private:
	AnimEventListView* pView;
	int nFirstSelectedItem;
	int nLastSelectedItem;
	bool bSelected;
};

class AnimEventNameChangeJob : public IAnimEventDocJob
{
public:
	AnimEventNameChangeJob(int nID, const char* szName);
	virtual ~AnimEventNameChangeJob();

	virtual void Perform(IAnimEventDocChangeTarget* pTarget);

private:
	int nID;
	CString sName;
};

IAnimEventView* AnimEventListView_Create(CObjectListCtrl* pListCtrl, const std::vector<string>& bones)
{
	return new AnimEventListView(pListCtrl, bones);
}

AnimEventListView::AnimEventListView(CObjectListCtrl* pListCtrl, const std::vector<string>& bones)
:	pListCtrl(pListCtrl),
	pDoc(0),
	bones(bones)
{
	// Listen to the list control.
	this->pListCtrl->AddListener(this);
}

AnimEventListView::~AnimEventListView()
{
	// Stop listening to the list control.
	this->pListCtrl->RemoveListener(this);
}

void AnimEventListView::ReferenceDoc(IAnimEventDoc* pDoc)
{
	this->pDoc = pDoc;
}

void AnimEventListView::Refresh(IAnimEventDocChangeDescription* pDescription)
{
	if (this->pListCtrl == 0)
		return;

	// Check whether we need to do a complete refresh, or just update the selections.
	if (pDescription == 0 || pDescription->HasEventListChanged())
	{
		// Clear out the display.
		this->pListCtrl->DeleteAllItems();
		this->items.clear();
		this->ids.clear();

		if (this->pDoc == 0)
			return;

		BuildFreshVisitor visitor(this);
		this->pDoc->GetVisitable()->Visit(&visitor);
		this->items.insert(std::make_pair(-1, ListCtrlItem(this, -1, "", "", 0.0f, "", "", Vec3(0, 0, 0), Vec3(0, 0, 0))));
		this->ids.push_back(-1);
		this->pListCtrl->AddObject(&this->items[-1]);
		this->pListCtrl->RedrawItems(0, this->pListCtrl->GetItemCount() - 1);
		this->pListCtrl->UpdateWindow();
	}
	else
	{
		if (pDescription->HaveEventsChanged())
		{
			UpdateEventVisitor visitor(this);
			this->pDoc->GetVisitable()->Visit(&visitor);
			this->pListCtrl->RedrawItems(0, this->pListCtrl->GetItemCount() - 1);
			this->pListCtrl->UpdateWindow();
		}

		if (pDescription->HasSelectionChanged())
		{
			// Just update the selection flags.
			//UpdateSelectionVisitor visitor(this);
			//this->pDoc->Visit(&visitor);
			//this->pListCtrl->RedrawItems(0, this->pListCtrl->GetItemCount() - 1);
			//this->pListCtrl->UpdateWindow();
		}
	}
}

void AnimEventListView::OnObjectListCtrlSelectionRangeChanged(CObjectListCtrl* pCtrl, int nFirstSelectedItem, int nLastSelectedItem, bool bSelected)
{
	// Create a new job that will update the selection in the document.
	this->pDoc->AcquireJob(new AnimEventSelectionChangedJob(this));
}

void AnimEventListView::OnObjectListCtrlLabelChanged(CObjectListCtrl* pCtrl, int nItem, int nSubItem, const char* szNewText)
{
	// Check what item was changed.
	switch (nSubItem)
	{
	case 0:
		{
			// Create a new job that will update the name in the document.
			this->pDoc->AcquireJob(new AnimEventNameChangeJob(this->ids[nItem], szNewText));
		}
		break;

	case 1:
		{
			// Check whether the value is a valid time.
			char* end;
			double value = strtod(szNewText, &end);
			if (szNewText != end)
			{
				// Clamp the value to a valid range.
				if (value < 0.0)
					value = 0.0;
				if (value > 1.0)
					value = 1.0;

				// Create a new job that will update the time in the document.
				std::set<int> ids;
				ids.insert(this->ids[nItem]);
				this->pDoc->AcquireJob(new AnimEventChangeTimeJob(value, ids));
			}
		}
		break;

	case 2:
		{
			// Set the parameter of the item.
			std::set<int> ids;
			ids.insert(this->ids[nItem]);
			this->pDoc->AcquireJob(new AnimEventSetParameterJob(szNewText, ids));
		}
		break;

	case 3:
		{
			const char* str = szNewText;
			if (string("<none>") == str)
				str = "";

			// Set the bone of the item.
			std::set<int> ids;
			ids.insert(this->ids[nItem]);
			this->pDoc->AcquireJob(new AnimEventSetBoneJob(str, ids));
		}
		break;

	case 4:
		{
			// Check whether the value is a valid vector.
			bool valid = false;
			Vec3 vValue;
			if (szNewText)
			{
				float x,y,z;
				if (sscanf(szNewText, "%f,%f,%f", &x, &y, &z) == 3)
				{
					vValue = Vec3(x,y,z);
					valid = true;
				}
			}

			if (valid)
			{
				// Create a new job that will update the time in the document.
				std::set<int> ids;
				ids.insert(this->ids[nItem]);
				this->pDoc->AcquireJob(new AnimEventChangeOffsetJob(vValue, ids));
			}
		}
		break;

	case 5:
		{
			// Check whether the value is a valid vector.
			bool valid = false;
			Vec3 vValue;
			if (szNewText)
			{
				float x,y,z;
				if (sscanf(szNewText, "%f,%f,%f", &x, &y, &z) == 3)
				{
					vValue = Vec3(x,y,z);
					valid = true;
				}
			}

			if (valid)
			{
				// Create a new job that will update the time in the document.
				std::set<int> ids;
				ids.insert(this->ids[nItem]);
				this->pDoc->AcquireJob(new AnimEventChangeDirJob(vValue, ids));
			}
		}
		break;
	}
}

void AnimEventListView::OnObjectListCtrlItemActivated(CObjectListCtrl* pCtrl, int nItem)
{
	// Get the time of this animation.
	float fAnimTime = this->items[this->ids[nItem]].fTime;

	this->pDoc->SetTime(fAnimTime);
}

AnimEventListView::ListCtrlItem::ListCtrlItem()
:	nID(-1),
	sAnimPath(""),
	sName(""),
	fTime(0.0f),
	pView(0),
	sParameter(""),
	sBone(""),
	vOffset(0, 0, 0),
	vDir(0, 0, 0)
{
}

AnimEventListView::ListCtrlItem::ListCtrlItem(AnimEventListView* pView, int nID, const char* szAnimName, const char* szName, float fTime, const char* szParameter, const char* szBone, const Vec3& vOffset, const Vec3& vDir)
:	nID(nID),
	sAnimPath(szAnimName),
	sName(szName),
	fTime(fTime),
	pView(pView),
	sParameter(szParameter),
	sBone(szBone),
	vOffset(vOffset),
	vDir(vDir)
{
}

AnimEventListView::ListCtrlItem::~ListCtrlItem()
{
}

CString AnimEventListView::ListCtrlItem::GetText(int nSubItem)
{
	if (this->nID == -1)
		return "";

	CString str;

	switch (nSubItem)
	{
	case 0:
		str = this->sName;
		break;
	case 1:
		str.Format( "%f",this->fTime );
		break;
	case 2:
		str = this->sParameter;
		break;
	case 3:
		str = this->sBone;
		break;
	case 4:
		str.Format("%.8g,%.8g,%.8g", this->vOffset.x, this->vOffset.y, this->vOffset.z);
		break;
	case 5:
		str.Format("%.8g,%.8g,%.8g", this->vDir.x, this->vDir.y, this->vDir.z);
		break;
	default:
		str = "undefined";
		break;
	}

	return str;
}

bool AnimEventListView::ListCtrlItem::IsSelected()
{
	return this->bSelected;
}

CSubeditListCtrl::EditStyle AnimEventListView::ListCtrlItem::GetEditStyle(int subitem)
{
	if (this->nID == -1)
		return CSubeditListCtrl::EDIT_STYLE_NONE;
	return subitem == 3 ? CSubeditListCtrl::EDIT_STYLE_COMBO : CSubeditListCtrl::EDIT_STYLE_EDIT;
}

void AnimEventListView::ListCtrlItem::GetOptions(int subitem, std::vector<string>& options, string& currentOption)
{
	if (subitem == 3)
	{
		options.push_back("<none>");
		std::copy(pView->bones.begin(), pView->bones.end(), std::back_inserter(options));
		if (this->sBone.IsEmpty())
			currentOption = "<none>";
		else
			currentOption = this->sBone.GetString();
	}
}

AnimEventListView::BuildFreshVisitor::BuildFreshVisitor(AnimEventListView* pView)
:	sAnimPath(pView->pDoc->GetAnim()),
	pView(pView)
{
}

AnimEventListView::BuildFreshVisitor::~BuildFreshVisitor()
{
}

void AnimEventListView::BuildFreshVisitor::VisitEvent(int nID, const char* szAnimPath, const char* szName, float fTime, const char* szParameter, const char* szBone, const Vec3& vOffset, const Vec3& vDir, bool bSelected)
{
	if (this->pView->pListCtrl == 0)
		return;

	if (this->sAnimPath == szAnimPath)
	{
		this->pView->items.insert(std::make_pair(nID, ListCtrlItem(this->pView, nID, szAnimPath, szName, fTime, szParameter, szBone, vOffset, vDir)));
		this->pView->ids.push_back(nID);
		this->pView->pListCtrl->AddObject(&this->pView->items[nID]);
	}
}

AnimEventListView::UpdateSelectionVisitor::UpdateSelectionVisitor(AnimEventListView* pView)
:	pView(pView)
{
}

AnimEventListView::UpdateSelectionVisitor::~UpdateSelectionVisitor()
{
}

void AnimEventListView::UpdateSelectionVisitor::VisitEvent(int nID, const char* szAnimPath, const char* szName, float fTime, const char* szParameter, const char* szBone, const Vec3& vOffset, const Vec3& vDir, bool bSelected)
{
	if (this->pView->pListCtrl == 0)
		return;

	std::map<int, ListCtrlItem>::iterator itItem;
	itItem = this->pView->items.find(nID);
	if (itItem != this->pView->items.end())
		(*itItem).second.bSelected = bSelected;
}

AnimEventListView::UpdateEventVisitor::UpdateEventVisitor(AnimEventListView* pView)
:	pView(pView)
{
}

AnimEventListView::UpdateEventVisitor::~UpdateEventVisitor()
{
}

void AnimEventListView::UpdateEventVisitor::VisitEvent(int nID, const char* szAnimPath, const char* szName, float fTime, const char* szParameter, const char* szBone, const Vec3& vOffset, const Vec3& vDir, bool bSelected)
{
	if (this->pView->pListCtrl == 0)
		return;

	std::map<int, ListCtrlItem>::iterator itItem;
	itItem = this->pView->items.find(nID);
	if (itItem != this->pView->items.end())
	{
		(*itItem).second.sAnimPath = szAnimPath;
		(*itItem).second.sName = szName;
		(*itItem).second.fTime = fTime;
		(*itItem).second.sName = szName;
		(*itItem).second.sParameter = szParameter;
		(*itItem).second.sBone = szBone;
		(*itItem).second.vOffset = vOffset;
		(*itItem).second.vDir = vDir;
		(*itItem).second.bSelected = bSelected;
	}
}

AnimEventSelectionChangedJob::AnimEventSelectionChangedJob(AnimEventListView* pView)
:	pView(pView)
{
}

AnimEventSelectionChangedJob::~AnimEventSelectionChangedJob()
{
}

void AnimEventSelectionChangedJob::Perform(IAnimEventDocChangeTarget* pTarget)
{
	// Create a set of selected items.
	std::set<int> selection;
	POSITION p = this->pView->pListCtrl->GetFirstSelectedItemPosition();
	while (p)
	{
		int nSelected = this->pView->pListCtrl->GetNextSelectedItem(p);
		if (this->pView->ids[nSelected] >= 0)
			selection.insert(this->pView->ids[nSelected]);
	}

	// Loop through all the items.
	for (std::vector<int>::iterator itID = this->pView->ids.begin(); itID != this->pView->ids.end(); ++itID)
	{
		bool bSelected = selection.find(*itID) != selection.end();
		pTarget->SetEventSelected(*itID, bSelected);
	}
}

AnimEventNameChangeJob::AnimEventNameChangeJob(int nID, const char* szName)
:	nID(nID),
	sName(szName)
{
}

AnimEventNameChangeJob::~AnimEventNameChangeJob()
{
}

void AnimEventNameChangeJob::Perform(IAnimEventDocChangeTarget* pTarget)
{
	pTarget->SetEventName(this->nID, this->sName);
}
