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

#ifndef __GSFILEDOWNLOAD_H__
#define __GSFILEDOWNLOAD_H__

#pragma once

#include "CryThread.h"
#include "INetwork.h"
#include "INetworkService.h"
#include "NetTimer.h"
#include "GameSpy/ghttp/ghttp.h"
#include "GameSpy/md5.h"

enum EDownloadResult
{
	eDR_Fail = 0,
	eDR_Success,
	eDR_InProgress,
};

class CGSFileDownloader : public CMultiThreadRefCount, public IFileDownloader
{
public:
	CGSFileDownloader();
	virtual ~CGSFileDownloader();

	virtual bool IsAvailable() const;

	// IFileDownloader
	virtual void DownloadFile(SFileDownloadParameters& dl);
	virtual void SetThrottleParameters(int datasize, int timedelay);
	virtual bool IsDownloading() const;
	virtual float GetDownloadProgress() const;
	virtual const unsigned char* GetFileMD5() const;
	virtual void Stop();
	virtual int GetDownloadError() const;
	// ~IFileDownloader

private:
	// callbacks
	static void ProgressCallback(GHTTPRequest request, GHTTPState state, const char* buffer, GHTTPByteCount bufferlen, GHTTPByteCount bytesreceived, GHTTPByteCount totalsize, void* param);
	static GHTTPBool CompletedCallback(GHTTPRequest request, GHTTPResult result, char* buffer, GHTTPByteCount bufferlen, void* param);

	// threads
	class CDownloadThread;
	std::auto_ptr<CDownloadThread> m_downloadThread;

	static MD5_CTX m_MD5Context;
	static unsigned char m_md5Checksum[16];

	// throttling
	int m_throttleDataSize;
	int m_throttleTimeDelay;	// ms
};



#endif // __GSFILEDOWNLOAD_H__