//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------

#include "StdAfx.h"
#include "AnimEventDoc.h"
#include "IAnimEventDocJob.h"
#include "IAnimEventDocVisitor.h"
#include "IAnimEventDocListener.h"
#include "Util/PathUtil.h"

#include <functional>
#include <algorithm>

AnimEventDoc::AnimEventDoc()
:	nLastID(100),
	fTime(0.0f)
{
}

AnimEventDoc::~AnimEventDoc()
{
}

void AnimEventDoc::AcquireJob(IAnimEventDocJob* pJob)
{
	// Assume that no changes will take place - when changes are made, record the fact in this object.
	this->changeRecord.Clear();

	// Perform the job.
	pJob->Perform(this);

	// Notify listeners.
	std::for_each(this->listeners.begin(), this->listeners.end(), std::bind2nd(std::mem_fun<void, IAnimEventDocListener>(&IAnimEventDocListener::DocChanged), &this->changeRecord));

	// Delete the job.
	delete pJob;
}

IAnimEventVisitable* AnimEventDoc::GetVisitable()
{
	return this;
}

void AnimEventDoc::Visit(IAnimEventDocVisitor* pVisitor)
{
	std::map<int, Event>::iterator itEvent;
	for (itEvent = this->events.begin(); itEvent != this->events.end(); ++itEvent)
	{
		const Event& event = (*itEvent).second;
		pVisitor->VisitEvent(event.nID, event.sAnimPath, event.sName, event.fTime, event.sParameter, event.sBone, event.vOffset, event.vDir, event.bSelected);
	}
}

void AnimEventDoc::AddListener(IAnimEventDocListener* pListener)
{
	this->listeners.push_back(pListener);
}

float AnimEventDoc::GetTime()
{
	return this->fTime;
}

void AnimEventDoc::SetTime(float fTime)
{
	this->fTime = fTime;

	// Report the change to listeners.
	AnimEventDocChangeRecord changes;
	changes.SetTimeChanged();
	std::for_each(this->listeners.begin(), this->listeners.end(), std::bind2nd(std::mem_fun<void, IAnimEventDocListener>(&IAnimEventDocListener::DocChanged), &changes));
}

CString AnimEventDoc::GetAnim()
{
	return this->sAnimName;
}

void AnimEventDoc::SetAnim(const char* szAnimName)
{
	this->sAnimName = Path::ToUnixPath(szAnimName).MakeLower();

	// Clear the selection.
	for (std::map<int, Event>::iterator itEvent = this->events.begin(); itEvent != this->events.end(); ++itEvent)
		(*itEvent).second.bSelected = false;

	// Report the change to listeners.
	AnimEventDocChangeRecord changes;
	changes.SetAnimChanged();
	std::for_each(this->listeners.begin(), this->listeners.end(), std::bind2nd(std::mem_fun<void, IAnimEventDocListener>(&IAnimEventDocListener::DocChanged), &changes));
}

int AnimEventDoc::AddAnimEvent(const char* szName, const char* szAnimPath)
{
	// Create the new event.
	Event event;
	event.nID = ++this->nLastID;
	event.sAnimPath = szAnimPath;
	event.sName = szName;
	event.fTime = 0.0f;
	event.vOffset = Vec3(0,0,0);
	event.vDir = Vec3(0,0,0);
	event.bSelected = false;

	// Add the event to the map.
	this->events.insert(std::make_pair(event.nID, event));

	// Record that events have changed.
	this->changeRecord.SetEventListChanged();

	return event.nID;
}

void AnimEventDoc::RemoveAnimEvent(int nEventID)
{
	this->events.erase(nEventID);

	// Record that events have changed.
	this->changeRecord.SetEventListChanged();
}

void AnimEventDoc::SetAnimEventTime(int nEventID, float fTime)
{
	assert(this->events.find(nEventID) != this->events.end());
	this->events[nEventID].fTime = fTime;

	// Record that events have changed.
	this->changeRecord.SetEventsChanged();
}

void AnimEventDoc::SetEventSelected(int nEventID, bool bSelected)
{
	assert(this->events.find(nEventID) != this->events.end());
	this->events[nEventID].bSelected = bSelected;

	// Record that the selection has changed.
	this->changeRecord.SetSelectionChanged();
}

void AnimEventDoc::SetEventName(int nEventID, const char* szName)
{
	assert(this->events.find(nEventID) != this->events.end());
	this->events[nEventID].sName = szName;

	// Record that the event has changed.
	this->changeRecord.SetEventsChanged();
}

void AnimEventDoc::SetEventParameter(int nEventID, const char* szParameter)
{
	assert(this->events.find(nEventID) != this->events.end());
	this->events[nEventID].sParameter = szParameter;

	// Record that the event has changed.
	this->changeRecord.SetEventsChanged();
}

void AnimEventDoc::SetEventBone(int nEventID, const char* szBone)
{
	assert(this->events.find(nEventID) != this->events.end());
	this->events[nEventID].sBone = szBone;

	// Record that the event has changed.
	this->changeRecord.SetEventsChanged();
}

void AnimEventDoc::SetAnimEventOffset(int nEventID, const Vec3& vOffset)
{
	assert(this->events.find(nEventID) != this->events.end());
	this->events[nEventID].vOffset = vOffset;

	// Record that the event has changed.
	this->changeRecord.SetEventsChanged();
}

void AnimEventDoc::SetAnimEventDir(int nEventID, const Vec3& vDir)
{
	assert(this->events.find(nEventID) != this->events.end());
	this->events[nEventID].vDir = vDir;

	// Record that the event has changed.
	this->changeRecord.SetEventsChanged();
}

void AnimEventDoc::Build(IAnimEventVisitable* pVisitable)
{
	// Clear out the object.
	this->events.clear();
	this->nLastID = 100;

	// Visit the other object.
	pVisitable->Visit(this);
}

AnimEventDoc::AnimEventDocChangeRecord::AnimEventDocChangeRecord()
{
	this->Clear();
}

AnimEventDoc::AnimEventDocChangeRecord::~AnimEventDocChangeRecord()
{
}

bool AnimEventDoc::AnimEventDocChangeRecord::HasSelectionChanged()
{
	return m_bSelectionChanged;
}

bool AnimEventDoc::AnimEventDocChangeRecord::HaveEventsChanged()
{
	return m_bEventsChanged;
}

bool AnimEventDoc::AnimEventDocChangeRecord::HasEventListChanged()
{
	return m_bEventListChanged;
}

bool AnimEventDoc::AnimEventDocChangeRecord::HasTimeChanged()
{
	return m_bTimeChanged;
}

bool AnimEventDoc::AnimEventDocChangeRecord::HasAnimChanged()
{
	return m_bAnimChanged;
}

void AnimEventDoc::AnimEventDocChangeRecord::SetSelectionChanged()
{
	m_bSelectionChanged = true;
}

void AnimEventDoc::AnimEventDocChangeRecord::SetEventsChanged()
{
	m_bEventsChanged = true;
}

void AnimEventDoc::AnimEventDocChangeRecord::SetEventListChanged()
{
	m_bEventListChanged = true;

	// A change to the event list implies a change to events, and to the selection.
	SetEventsChanged();
	SetSelectionChanged();
}

void AnimEventDoc::AnimEventDocChangeRecord::SetTimeChanged()
{
	m_bTimeChanged = true;
}

void AnimEventDoc::AnimEventDocChangeRecord::SetAnimChanged()
{
	m_bAnimChanged = true;

	// A change to the animation implies pretty much everything has changed.
	m_bSelectionChanged = true;
	m_bEventListChanged = true;
}

void AnimEventDoc::AnimEventDocChangeRecord::Clear()
{
	m_bSelectionChanged = false;
	m_bEventsChanged = false;
	m_bEventListChanged = false;
	m_bTimeChanged = false;
	m_bAnimChanged = false;
}

void AnimEventDoc::VisitEvent(int nID, const char* szAnimPath, const char* szName, float fTime, const char* szParameter, const char* szBone, const Vec3& vOffset, const Vec3& vDir, bool bSelected)
{
	// We have been called as part of building the document - add the event.
	Event event;
	event.nID = ++this->nLastID; // Ignore the passed in id.
	event.sAnimPath = Path::ToUnixPath(szAnimPath).MakeLower();
	event.sName = szName;
	event.fTime = fTime;
	event.sParameter = szParameter;
	event.sBone = szBone;
	event.vOffset = vOffset;
	event.vDir = vDir;
	event.bSelected = bSelected;
	this->events.insert(std::make_pair(event.nID, event));

	// Make sure the id assignment will continue properly.
	if (this->nLastID <= nID)
		this->nLastID = nID + 1;
}
