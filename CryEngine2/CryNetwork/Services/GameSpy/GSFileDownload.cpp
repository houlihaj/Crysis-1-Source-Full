/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:  Gamespy file downloading
-------------------------------------------------------------------------
History:
- 29/01/2007   : Steve Humphreys, Created
*************************************************************************/

#include "StdAfx.h"
#include "GSFileDownload.h"
#include "Network.h"

static const float UPDATE_INTERVAL = 0.05f;

class CGSFileDownloader::CDownloadThread : public CrySimpleThread<>, public IFileDownload
{
	friend class CGSFileDownloader;
public:
	CDownloadThread(SFileDownloadParameters& dl)
	{
		m_shouldQuit = false;
		m_pStream = dl.pStream;
		BeginDownload(dl);
		m_downloadError = eFDE_NoError;
	}

	void Run()
	{
		CryThreadSetName( -1,"NetFileDownload" );
		while (!m_shouldQuit)
		{
			if(m_request >= 0)
			{
				CryAutoLock<CryFastLock> lock(m_lock);
				if(ghttpRequestThink(m_request) == GHTTPFalse)
					m_result = eDR_Fail;
			}
			Sleep(100);
		}
	}

	void Cancel()
	{
		CryAutoLock<CryFastLock> lock(m_lock);
		if(m_result == eDR_InProgress)
		{
			ghttpCancelRequest(m_request);
			if (m_pStream)
				m_pStream->Complete(false);
		}
		m_result = eDR_Fail;
		m_bytesreceived = 0;
		m_size = 0;
		m_shouldQuit = true;
	}

	virtual bool Finished() const
	{
		return (m_result != eDR_InProgress);
	}

	virtual bool WasSuccessful() const
	{
		return (m_result == eDR_Success);
	}

	IDownloadStream * GetStream()
	{
		return m_pStream;
	}

	virtual float GetProgress() const
	{
		float ret = 0.0f;
		switch(m_result)
		{
		case eDR_Fail:
		default:
			ret = 0.0f;
			break;

		case eDR_Success:
			ret = 1.0f;
			break;

		case eDR_InProgress:
			if(m_size > 0)
				ret = (float)m_bytesreceived / m_size;
			break;			
		}
		return ret;
	}

	void ThrottleDownload(int datasize, int timedelay)
	{
		ghttpThrottleSettings(datasize, timedelay);
		if(datasize == 0 || timedelay == 0)
		{
			ghttpSetThrottle(m_request, GHTTPFalse);
		}
		else
		{
			ghttpSetThrottle(m_request, GHTTPTrue);
		}
	}

private:
	void BeginDownload(SFileDownloadParameters& dl)
	{
		// NB pass this ptr as user-param -> will be sent back to us in callbacks
		if (dl.pStream)
		{
			m_request = ghttpStreamEx( dl.sourceFilename.c_str(), NULL, NULL, GHTTPFalse, GHTTPFalse, &CGSFileDownloader::ProgressCallback, &CGSFileDownloader::CompletedCallback, this );
			m_bytesreceived = 0;
			m_size = -1;
		}
		else
		{
			string file = dl.destPath + "/" + dl.destFilename;
			m_request = ghttpSaveEx(dl.sourceFilename.c_str(), file.c_str(), NULL, NULL, GHTTPFalse, GHTTPFalse, &CGSFileDownloader::ProgressCallback, &CGSFileDownloader::CompletedCallback, this);
			m_bytesreceived = 0;
			m_size = dl.fileSize;
		}
		if(m_request >= 0)
		{
			m_result = eDR_InProgress;
		}
		else
		{
			m_result = eDR_Fail;
			if(dl.pStream)
				dl.pStream->Complete(false);
		}
	}

	CryFastLock m_lock;

	GHTTPRequest m_request;
	EDownloadResult m_result;
	int m_size;
	int m_bytesreceived;
	IDownloadStream * m_pStream;

	EFileDownloadError m_downloadError;

	bool m_shouldQuit;
};

//////////////////////////////////////////////////////////////

// static data
unsigned char CGSFileDownloader::m_md5Checksum[16];
MD5_CTX CGSFileDownloader::m_MD5Context;

CGSFileDownloader::CGSFileDownloader()
{
	ghttpStartup();
}

CGSFileDownloader::~CGSFileDownloader()
{
	if(m_downloadThread.get())
	{
		m_downloadThread->Cancel();
		m_downloadThread.release();		
	}
	ghttpCleanup();
}

bool CGSFileDownloader::IsAvailable() const
{
	return true;
}

void CGSFileDownloader::DownloadFile(SFileDownloadParameters& dl)
{
	if(!m_downloadThread.get() || m_downloadThread->Finished())
	{
		if(m_downloadThread.get())
		{
			m_downloadThread->Cancel();
			m_downloadThread.release();
		}
		m_downloadThread.reset(new CDownloadThread(dl));
		m_downloadThread->Start();
		m_downloadThread->ThrottleDownload(m_throttleDataSize, m_throttleTimeDelay);

		MD5Init(&m_MD5Context);
	}
}

void CGSFileDownloader::SetThrottleParameters(int datasize, int timedelay)
{
	m_throttleDataSize = datasize;
	m_throttleTimeDelay = timedelay;

	if(m_downloadThread.get())
		m_downloadThread->ThrottleDownload(datasize, timedelay);
}

bool CGSFileDownloader::IsDownloading() const
{
	if(!m_downloadThread.get()) 
		return false;

	return (!m_downloadThread->Finished());
}

float CGSFileDownloader::GetDownloadProgress() const
{
	if(m_downloadThread.get())
		return m_downloadThread->GetProgress();

	return 0.0f;
}

int CGSFileDownloader::GetDownloadError() const
{
	if(m_downloadThread.get())
		return m_downloadThread->m_downloadError;

	return eFDE_NoError;
}

const unsigned char* CGSFileDownloader::GetFileMD5() const
{
	if(m_downloadThread.get() && m_downloadThread->WasSuccessful())
		return m_md5Checksum;

	return NULL;
}

void CGSFileDownloader::Stop() 
{
	if(m_downloadThread.get())
		m_downloadThread->Cancel();
}

void CGSFileDownloader::ProgressCallback(GHTTPRequest request, GHTTPState state, const char* buffer, GHTTPByteCount bufferlen, GHTTPByteCount bytesreceived, GHTTPByteCount totalsize, void* param)
{
	CGSFileDownloader::CDownloadThread* pThread = (CGSFileDownloader::CDownloadThread*)param;
	if(pThread)
	{
		NET_ASSERT(pThread->m_request == request);

		pThread->m_bytesreceived = bytesreceived;
		pThread->m_size = totalsize;
		pThread->m_result = eDR_InProgress;

		if(bufferlen >0)
		{
			if (IDownloadStream * pStream = pThread->GetStream())
			{
				pStream->GotData( (const uint8*)buffer, bufferlen );
			}
		}

		MD5Update(&m_MD5Context, (unsigned char*)buffer, bufferlen);
	}
}

GHTTPBool CGSFileDownloader::CompletedCallback(GHTTPRequest request, GHTTPResult result, char* buffer, GHTTPByteCount bufferlen, void* param)
{
	CGSFileDownloader::CDownloadThread* pThread = (CGSFileDownloader::CDownloadThread*)param;
	if(pThread)
	{
		NET_ASSERT(pThread->m_request == request);

		pThread->m_bytesreceived = pThread->m_size;
		if(result != GHTTPSuccess)
		{
			CryLog("File download failed: %d", result);
			pThread->m_result = eDR_Fail;
		}
		else
		{
			pThread->m_result = eDR_Success;

			MD5Final(m_md5Checksum, &m_MD5Context);
		}

		pThread->m_downloadError = static_cast<EFileDownloadError>(result);

		if (IDownloadStream * pStream = pThread->GetStream())
			pStream->Complete(pThread->WasSuccessful());
	}

	return GHTTPTrue;
}
