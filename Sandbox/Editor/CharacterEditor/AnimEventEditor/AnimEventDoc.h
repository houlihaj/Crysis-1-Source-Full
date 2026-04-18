//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __ANIMEVENTDOC_H__
#define __ANIMEVENTDOC_H__

#include "IAnimEventDocChangeTarget.h"
#include "IAnimEventDocChangeDescription.h"
#include "IAnimEventDocVisitor.h"
#include "IAnimEventDoc.h"
#include "IAnimEventVisitable.h"
#include <map>
#include <string>
#include <vector>

class IAnimEventDocListener;

class AnimEventDoc : public IAnimEventDocChangeTarget, public IAnimEventDoc, public IAnimEventVisitable, public IAnimEventDocVisitor
{
public:
	AnimEventDoc();
	virtual ~AnimEventDoc();

	virtual void AcquireJob(IAnimEventDocJob* pJob);
	virtual IAnimEventVisitable* GetVisitable();
	virtual void Visit(IAnimEventDocVisitor* pVisitor);
	virtual void AddListener(IAnimEventDocListener* pListener);
	virtual float GetTime();
	virtual void SetTime(float fTime);
	virtual CString GetAnim();
	virtual void SetAnim(const char* szAnimName);

	virtual int AddAnimEvent(const char* szName, const char* szAnimPath);
	virtual void RemoveAnimEvent(int nEventID);
	virtual void SetAnimEventTime(int nEventID, float fTime);
	virtual void SetEventSelected(int nEventID, bool bSelected);
	virtual void SetEventName(int nEventID, const char* szName);
	virtual void SetEventParameter(int nEventID, const char* szParameter);
	virtual void SetEventBone(int nEventID, const char* szBone);
	virtual void SetAnimEventOffset(int nEventID, const Vec3& vOffset);
	virtual void SetAnimEventDir(int nEventID, const Vec3& vDir);

	virtual void Build(IAnimEventVisitable* pVisitable);
	virtual void VisitEvent(int nID, const char* szAnimPath, const char* szName, float fTime, const char* szParameter, const char* szBone, const Vec3& vOffset, const Vec3& vDir, bool bSelected);

private:

	class Event
	{
	public:
		int nID;
		CString sAnimPath;
		CString sName;
		float fTime;
		CString sParameter;
		CString sBone;
		Vec3 vOffset;
		Vec3 vDir;
		bool bSelected;
	};

	class AnimEventDocChangeRecord : public IAnimEventDocChangeDescription
	{
	public:
		AnimEventDocChangeRecord();
		virtual ~AnimEventDocChangeRecord();

		virtual bool HasSelectionChanged();
		virtual bool HaveEventsChanged();
		virtual bool HasEventListChanged();
		virtual bool HasTimeChanged();
		virtual bool HasAnimChanged();

		void SetSelectionChanged();
		void SetEventsChanged();
		void SetEventListChanged();
		void SetTimeChanged();
		void SetAnimChanged();
		void Clear();

	private:
		bool m_bSelectionChanged;
		bool m_bEventListChanged;
		bool m_bEventsChanged;
		bool m_bTimeChanged;
		bool m_bAnimChanged;
	};

	std::map<int, Event> events;
	std::vector<IAnimEventDocListener*> listeners;
	int nLastID;
	AnimEventDocChangeRecord changeRecord;
	float fTime;
	CString sAnimName;
};

#endif //__ANIMEVENTDOC_H__
