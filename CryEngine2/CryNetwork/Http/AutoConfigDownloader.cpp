// automatically downloads system.cfgs from a http site - for the beta test servers

#include "StdAfx.h"
#include "AutoConfigDownloader.h"
#include "INetwork.h"

CAutoConfigDownloader * CAutoConfigDownloader::m_pThis = 0;

CAutoConfigDownloader::CAutoConfigDownloader()
{
	m_executing = false;
	m_pThis = this;
	gEnv->pConsole->RegisterString( "sv_autoconfigurl", "", 0, "Automatically download configuration data from a URL", OnCVarChanged );
}

void CAutoConfigDownloader::TriggerRefresh()
{
	if (m_url.empty() || !gEnv->pSystem->IsDedicated())
		return;

	NetLogAlways("Downloading configuration from %s", m_url.c_str());

	INetworkService* pserv = gEnv->pNetwork->GetService("GameSpy");
	if(pserv)
	{
		IFileDownloader* pfd = pserv->GetFileDownloader();
		if(pfd && pfd->IsAvailable() && !pfd->IsDownloading())
		{
			// TODO: update throttling based on game state.
			//	Eg if in MP net game, throttle to not disrupt gameplay.
			//	Otherwise (for patches, etc) DL at max speed
			pfd->SetThrottleParameters(2048, 200);		// eg 10kb/sec

			m_buffer.resize(0);

			SFileDownloadParameters dl;
			dl.pStream = this;
			dl.sourceFilename = m_url;
			pfd->DownloadFile( dl );
			m_executing = true;
		}
	}
}

void CAutoConfigDownloader::Update()
{
	if (!m_toExec.empty())
	{
		string cmd;
		for (size_t i=0; i<m_toExec.size(); i++)
		{
			switch (m_toExec[i])
			{
			case '\r':
			case '\n':
				if (!cmd.empty() && cmd[0] != '#')
					gEnv->pConsole->ExecuteString(cmd.c_str());
				cmd.resize(0);
				break;
			default:
				cmd += m_toExec[i];
				break;
			}
		}
		if (!cmd.empty() && cmd[0] != '#')
			gEnv->pConsole->ExecuteString(cmd.c_str());
		m_toExec.resize(0);
	}
}

void CAutoConfigDownloader::GotData( const uint8 * pData, uint32 length )
{
	m_buffer += string((const char *)pData, (const char *)(pData+length));
}

void CAutoConfigDownloader::Complete( bool success )
{
	m_executing = false;
	if (success)
		m_toExec = m_buffer;
	else
		NetWarning("Configuration download failed");
	m_buffer.resize(0);
}

void CAutoConfigDownloader::OnCVarChanged( ICVar * pVar )
{
	m_pThis->Reconfigure(pVar->GetString());
}

void CAutoConfigDownloader::Reconfigure( const string& loc )
{
	m_url = loc;
	TriggerRefresh();
}
