// automatically downloads system.cfgs from a http site - for the beta test servers

#ifndef __AUTOCONFIGDOWNLOADER_H__
#define __ATUOCONFIGDOWNLOADER_H__

#pragma once

#include "IConsole.h"
#include "INetworkService.h"

class CAutoConfigDownloader : private IDownloadStream
{
public:
	CAutoConfigDownloader();

	void TriggerRefresh();
	void Update();

private:
	void GotData( const uint8 * pData, uint32 length );
	void Complete( bool success );

	void Reconfigure( const string& loc );
	string m_url;
	string m_buffer;
	string m_toExec;
	bool m_executing;

	static CAutoConfigDownloader * m_pThis;

	static void OnCVarChanged( ICVar * pVar );
};

#endif
