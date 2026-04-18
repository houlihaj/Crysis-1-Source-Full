////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   statobjconstr.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: loading
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "StatObj.h"
#include "IndexedMesh.h"
#include "../RenderDll/Common/Shadow_Renderer.h"
#include <IRenderer.h>
#include <CrySizer.h>



void CStatObj::ProcessStreamOnCompleteError()
{ // file was not loaded successfully
	m_eCCGFStreamingStatus = ecss_NotLoaded;

	// still error - make nice default object
	m_eCCGFStreamingStatus = ecss_NotLoaded;
	LoadCGF("objects/default.cgf");
	assert(	m_eCCGFStreamingStatus == ecss_Ready);
	m_bDefaultObject=true;

	assert(0);
	GetSystem()->Error("Error loading CGF for: %s", m_szFileName.c_str());
}

void CStatObj::StreamOnComplete(IReadStream* pStream, unsigned nError)
{
	m_pReadStream = 0;

	if(pStream->IsError())
	{ // file was not loaded successfully
		ProcessStreamOnCompleteError();
		return;
	}

	//////////////////////////////////////////////////////////////////////////
	// Timur
	//@TODO: Load CGF from buffer. 
	//////////////////////////////////////////////////////////////////////////
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::StreamCCGF(bool bFinishNow)
{
	if(m_eCCGFStreamingStatus != ecss_NotLoaded)
		return;

	// start streaming
	StreamReadParams params;
	params.dwUserData = 0;
	params.nSize = 0;
	params.pBuffer = NULL;
	params.nLoadTime = 10000;
	params.nMaxLoadTime = 10000;
	params.nFlags |= SRP_FLAGS_ASYNC_PROGRESS;

	m_eCCGFStreamingStatus = ecss_InProgress;
	m_pReadStream = GetSystem()->GetStreamEngine()->StartRead("3DEngine", m_szFileName, this, &params);

	if(bFinishNow)
		m_pReadStream->Wait();
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::CheckLoaded()
{
	if(!m_bUseStreaming)
		return;

	m_nMarkedForStreamingFrameId = GetFrameID()+100;

	if(m_eCCGFStreamingStatus == ecss_NotLoaded)
	{ // load now
		assert(m_bUseStreaming == true);
		m_fStreamingTimePerFrame -= GetTimer()->GetAsyncCurTime();
		bool bRes = LoadCGF(m_szFileName);
		m_bUseStreaming = true;
		m_fStreamingTimePerFrame += GetTimer()->GetAsyncCurTime();
	}
	else if(m_eCCGFStreamingStatus == ecss_InProgress)		
	{ // finish now
		m_pReadStream->Wait();
	}
	else
	{ // object is ready
		assert(m_eCCGFStreamingStatus == ecss_Ready);
		assert(m_pRenderMesh && m_nLoadedTrisCount);
	}
}
