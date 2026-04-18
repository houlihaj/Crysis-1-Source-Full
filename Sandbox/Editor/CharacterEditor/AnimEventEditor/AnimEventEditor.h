//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __ANIMEVENTEDITOR_H__
#define __ANIMEVENTEDITOR_H__

class IAnimEventView;
class IAnimEventData;

class IAnimEventEditor
{
public:
	static IAnimEventEditor* Create();

	virtual ~IAnimEventEditor() = 0 {};
	virtual void AddView(IAnimEventView* pView) = 0;
	virtual void AcquireData(IAnimEventData* pData) = 0;

	virtual void AddEvent(const char* szAnimName, const char* szName) = 0;
	virtual void DeleteEvents() = 0;

	virtual void SetAnim(const char* szAnimName) = 0;

	virtual string GetFileName() = 0;
	virtual void SetFileName(string sFileName) = 0;
	virtual bool Save() = 0;
	virtual bool Load() = 0;
	virtual string GetErrorString() = 0;

	virtual bool IsDirty() = 0;
};

#endif //__ANIMEVENTEDITOR_H__
