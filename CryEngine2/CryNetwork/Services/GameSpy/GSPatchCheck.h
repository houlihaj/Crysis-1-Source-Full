/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:  Gamespy patch / update checking
-------------------------------------------------------------------------
History:
- 21/02/2007   : Steve Humphreys, Created
*************************************************************************/

#ifndef __GSPATCHCHECK_H__
#define __GSPATCHCHECK_H__

#pragma once

#include "INetwork.h"
#include "INetworkService.h"
#include "NetTimer.h"

#include "GameSpy/pt/pt.h"

enum EPatchCheckStatus
{
	ePCS_None,							// haven't sent a request yet
	ePCS_Checking,					// checking... waiting for responce
	ePCS_PatchAvailable,		// yes, there is a patch for download
	ePCS_PatchUnavailable,	// no, we have the most recent version
};

class CGSPatchCheck : public CMultiThreadRefCount, public IPatchCheck
{
public:
	CGSPatchCheck();
	virtual ~CGSPatchCheck();

	virtual bool IsAvailable() const;

	// IPatchCheck
	virtual bool CheckForUpdate();
	virtual bool IsUpdateCheckPending() const;
	virtual bool IsUpdateAvailable() const;
	virtual bool IsUpdateMandatory() const;
	virtual const char* GetPatchURL() const;
	virtual const char* GetPatchName() const;
	virtual void SetInstallOnExit(bool install);
	virtual bool GetInstallOnExit() const;
	virtual void SetPatchFileName(const char* filename);
	virtual const char* GetPatchFileName() const;
	virtual void TrackUsage();
	// ~IPatchCheck

private:
	static void PatchCallback(PTBool available, PTBool mandatory, const gsi_char * versionName, int fileID, const gsi_char * downloadURL, void * param ); 
	static void TimerCallback(NetTimerId,void*,CTimeValue);
	
	NetTimerId m_updateTimer;
	EPatchCheckStatus m_status;
	string m_patchURL;
	string m_patchName;
	string m_patchFileName;
	bool m_updateMandatory;
	bool m_installOnExit;
	bool m_usageTracked;
};

#endif // __GSPATCHCHECK_H__