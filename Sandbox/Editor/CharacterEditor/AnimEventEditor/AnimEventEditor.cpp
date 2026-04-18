//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#include "StdAfx.h"
#include "AnimEventEditor.h"
#include "IAnimEventData.h"
#include "IAnimEventView.h"
#include "IAnimEventDocJob.h"
#include "AnimEventDoc.h"
#include "IAnimEventDocListener.h"
#include "IAnimEventDocVisitor.h"
#include "AnimEventDocVisitors.h"
#include "AnimEventXMLStore.h"
#include <algorithm>
#include <functional>

class AnimEventEditor : public IAnimEventEditor, public IAnimEventDocListener
{
public:
	AnimEventEditor();
	virtual ~AnimEventEditor();

	virtual void AddView(IAnimEventView* pView);
	virtual void AcquireData(IAnimEventData* pData);
	virtual void AddEvent(const char* szAnimName, const char* szName);
	virtual void DeleteEvents();
	virtual void SetAnim(const char* szAnimName);
	virtual string GetFileName();
	virtual void SetFileName(string sFileName);
	virtual bool Save();
	virtual bool Load();
	virtual string GetErrorString();

	virtual void DocChanged(IAnimEventDocChangeDescription* pDescription);
	bool IsDirty();

private:
	std::auto_ptr<IAnimEventDoc> pDoc;
	std::vector<IAnimEventView*> views;
	std::auto_ptr<IAnimEventData> pData;
	string sFileName;
	string sError;
	bool bDirty;
};

class AnimEventEditorNewEventJob : public IAnimEventDocJob
{
public:
	AnimEventEditorNewEventJob(const char* szAnimName, float fTime, const char* szName);
	virtual ~AnimEventEditorNewEventJob();
	virtual void Perform(IAnimEventDocChangeTarget* pTarget);

private:
	CString sAnimPath;
	float fTime;
	CString sName;
};

class AnimEventEditorDeleteEventJob : public IAnimEventDocJob
{
public:
	AnimEventEditorDeleteEventJob(const std::set<int>& ids);
	virtual ~AnimEventEditorDeleteEventJob();
	virtual void Perform(IAnimEventDocChangeTarget* pTarget);

private:
	std::set<int> ids;
};

IAnimEventEditor* IAnimEventEditor::Create()
{
	return new AnimEventEditor;
}

AnimEventEditor::AnimEventEditor()
:	bDirty(false)
{
	// Create the document.
	this->pDoc = std::auto_ptr<IAnimEventDoc>(new AnimEventDoc());

	// Attach ourselves to the document to listen to events.
	this->pDoc->AddListener(this);
}

AnimEventEditor::~AnimEventEditor()
{
}

void AnimEventEditor::AddView(IAnimEventView* pView)
{
	this->views.push_back(pView);
	pView->ReferenceDoc(this->pDoc.get());
	pView->Refresh(0);
}

void AnimEventEditor::AcquireData(IAnimEventData* pData)
{
	this->pData = std::auto_ptr<IAnimEventData>(pData);
}

void AnimEventEditor::AddEvent(const char* szAnimPath, const char* szName)
{
	this->pDoc->AcquireJob(new AnimEventEditorNewEventJob(Path::ToUnixPath(szAnimPath).MakeLower().GetString(), this->pDoc->GetTime(), szName));
}

void AnimEventEditor::DeleteEvents()
{
	// Create a set object containing the ids of all the selected objects.
	AnimEventBuildSelectionSetVisitor* pVisitor = AnimEventBuildSelectionSetVisitor::Create();
	this->pDoc->GetVisitable()->Visit(pVisitor);

	// Create a job to delete these objects.
	this->pDoc->AcquireJob(new AnimEventEditorDeleteEventJob(pVisitor->GetSelectedIDS()));

	delete pVisitor;
}

void AnimEventEditor::SetAnim(const char* szAnimName)
{
	this->pDoc->SetAnim(szAnimName);
}

void AnimEventEditor::DocChanged(IAnimEventDocChangeDescription* pDescription)
{
	// Set the dirty flag if necessary.
	if (pDescription->HaveEventsChanged())
		this->bDirty = true;

	// The document has changed somehow - refresh all the views.
	std::for_each(this->views.begin(), this->views.end(), std::bind2nd(std::mem_fun<void, IAnimEventView>(&IAnimEventView::Refresh), pDescription));
}

string AnimEventEditor::GetFileName()
{
	return this->sFileName;
}

void AnimEventEditor::SetFileName(string sFileName)
{
	this->sFileName = sFileName;
}

bool AnimEventEditor::Save()
{
	if (this->sFileName.empty())
	{
		this->sError = "No file name set";
		return false;
	}

	std::auto_ptr<AnimEventXMLStore> pStore(AnimEventXMLStore::Create());
	pStore->SetFileName(this->sFileName);
	if (!pStore->Save(this->pDoc.get()))
	{
		this->sError = pStore->GetErrorString();
		return false;
	}

	// Clear the dirty flag.
	this->bDirty = false;

	return true;
}

bool AnimEventEditor::Load()
{
	if (this->sFileName.empty())
	{
		this->sError = "No file name set";
		return false;
	}

	std::auto_ptr<AnimEventXMLStore> pStore(AnimEventXMLStore::Create());
	pStore->SetFileName(this->sFileName);
	if (!pStore->Load(this->pDoc.get()))
	{
		this->sError = pStore->GetErrorString();
		return false;
	}

	// Clear the dirty flag.
	this->bDirty = false;

	return true;
}

string AnimEventEditor::GetErrorString()
{
	return this->sError;
}

bool AnimEventEditor::IsDirty()
{
	return this->bDirty;
}

AnimEventEditorNewEventJob::AnimEventEditorNewEventJob(const char* szAnimPath, float fTime, const char* szName)
:	sAnimPath(szAnimPath),
	fTime(fTime),
	sName(szName)
{
}

AnimEventEditorNewEventJob::~AnimEventEditorNewEventJob()
{
}

void AnimEventEditorNewEventJob::Perform(IAnimEventDocChangeTarget* pTarget)
{
	int nEventID = pTarget->AddAnimEvent(this->sName, this->sAnimPath);
	pTarget->SetAnimEventTime(nEventID, this->fTime);
}

AnimEventEditorDeleteEventJob::AnimEventEditorDeleteEventJob(const std::set<int>& ids)
:	ids(ids)
{
}

AnimEventEditorDeleteEventJob::~AnimEventEditorDeleteEventJob()
{
}

void AnimEventEditorDeleteEventJob::Perform(IAnimEventDocChangeTarget* pTarget)
{
	for (std::set<int>::iterator itID = this->ids.begin(); itID != this->ids.end(); ++itID)
		pTarget->RemoveAnimEvent(*itID);
}
