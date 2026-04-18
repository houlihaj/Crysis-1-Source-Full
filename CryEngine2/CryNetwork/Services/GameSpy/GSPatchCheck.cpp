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

#include "StdAfx.h"

#include "GSPatchCheck.h"

#include "GameSpy.h"
#include "Network.h"
#include "GameSpy/ghttp/ghttp.h"

static const float UPDATE_INTERVAL = 0.01f;

CGSPatchCheck::CGSPatchCheck()
{
	m_updateTimer = 0;
	m_status = ePCS_None;
	m_updateMandatory = false;
	m_installOnExit = false;
	m_usageTracked = false;
	m_patchURL.clear();
	m_patchName.clear();
	m_patchFileName.clear();
}

CGSPatchCheck::~CGSPatchCheck()
{
	if(m_updateTimer)
	{
		TIMER.CancelTimer(m_updateTimer);
		m_updateTimer=0;
	}
}

bool CGSPatchCheck::IsAvailable() const
{
	INetworkService *serv=gEnv->pSystem->GetINetwork()->GetService("GameSpy");
	if(serv && serv->GetState() == eNSS_Ok)
		return true;

	return false;
}

bool CGSPatchCheck::CheckForUpdate()
{
	// skip checks once the first one has been done (status is unlikely to change during a play session)
	if(m_status != ePCS_None)
		return false;

	char versionID[30];
	gEnv->pSystem->GetProductVersion().ToString(versionID);
	//sprintf(versionID, "0.0.0.1");	// old build - use for testing.
	if(PTTrue == ptCheckForPatch(PATCH_PRODUCT_ID, versionID, PATCH_DISTRIBUTION_ID, &PatchCallback, false, this))
	{
		m_status = ePCS_Checking;

		// set a timer (we need to keep updating ghttp until the result returns)
		SCOPED_GLOBAL_LOCK;
		m_updateTimer = TIMER.AddTimer( gEnv->pTimer->GetAsyncTime()+UPDATE_INTERVAL, TimerCallback, this );

		return true;
	}

	m_status = ePCS_None;
	return false;
}

bool CGSPatchCheck::IsUpdateAvailable() const
{
	return (m_status == ePCS_PatchAvailable);
}

bool CGSPatchCheck::IsUpdateMandatory() const
{
	return (IsUpdateAvailable() && m_updateMandatory);
}

bool CGSPatchCheck::IsUpdateCheckPending() const
{
	return (m_status == ePCS_Checking);
}

const char* CGSPatchCheck::GetPatchURL() const
{
	if(IsUpdateAvailable())
		return m_patchURL.c_str();

	return NULL;
}

const char* CGSPatchCheck::GetPatchName() const
{
	if(IsUpdateAvailable())
		return m_patchName.c_str();

	return NULL;
}

void CGSPatchCheck::SetInstallOnExit(bool install)
{
	if(IsUpdateAvailable())
		m_installOnExit = install;
}

bool CGSPatchCheck::GetInstallOnExit() const
{
	return (IsUpdateAvailable() && m_installOnExit);
}

void CGSPatchCheck::SetPatchFileName(const char* filename)
{
	if(filename)
		m_patchFileName = filename;
}

const char* CGSPatchCheck::GetPatchFileName() const
{
	const char* filename = NULL;
	if(IsUpdateAvailable())
		filename = m_patchFileName.c_str();

	return filename;
}

void CGSPatchCheck::TrackUsage()
{
	if(!m_usageTracked)
	{
		char versionID[30];
		gEnv->pSystem->GetProductVersion().ToString(versionID);
		//sprintf(versionID, "0.0.0.1");	// old build - use for testing.
		if(PTTrue == ptTrackUsage(0, PATCH_PRODUCT_ID, versionID, PATCH_DISTRIBUTION_ID, false))
			m_usageTracked = true;
	}
}

void CGSPatchCheck::PatchCallback(PTBool available, PTBool mandatory, const gsi_char * versionName, int fileID, const gsi_char * downloadURL, void * param )
{
	CGSPatchCheck* pThis = static_cast<CGSPatchCheck*>(param);
	if(pThis)
	{
		if(available == PTTrue)
		{
			pThis->m_status = ePCS_PatchAvailable;
			pThis->m_updateMandatory	= (mandatory == PTTrue) ? true : false;
			pThis->m_patchURL = downloadURL;
			pThis->m_patchName = versionName;
		}
		else
		{
			pThis->m_status = ePCS_PatchUnavailable;
			pThis->m_updateMandatory = false;
			pThis->m_patchURL.clear();
			pThis->m_patchName.clear();
		}
	}
}

void CGSPatchCheck::TimerCallback(NetTimerId,void* pIn,CTimeValue)
{
	SCOPED_GLOBAL_LOCK;
	ghttpThink();

	CGSPatchCheck* pThis = static_cast<CGSPatchCheck*>(pIn);

	if(pThis->IsUpdateCheckPending())
		pThis->m_updateTimer = TIMER.AddTimer( g_time + UPDATE_INTERVAL, &TimerCallback, pIn);
	else
		pThis->m_updateTimer = 0;
}