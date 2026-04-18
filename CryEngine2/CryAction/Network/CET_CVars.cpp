#include "StdAfx.h"
#include "GameClientChannel.h"
#include "NetHelpers.h"

class CConsoleVarSender : public ICVarDumpSink
{
public:
	CConsoleVarSender( IConsoleVarSink * pSink ) : m_pSink(pSink) {}

	virtual void OnElementFound( ICVar *pCVar )
	{
		if (pCVar->GetFlags() & (VF_NOT_NET_SYNCED | VF_REQUIRE_APP_RESTART))
			return;
		m_pSink->OnAfterVarChange(pCVar);
	}

private:
	IConsoleVarSink * m_pSink;
};

class CCET_CVarSync : public CCET_Base
{
public:
	CCET_CVarSync( IConsoleVarSink * pSink ) : m_pSink(pSink) {}

	const char * GetName() { return "CVarSync"; }

	EContextEstablishTaskResult OnStep( SContextEstablishState& state )
	{
		CConsoleVarSender cvarSender( m_pSink );
		gEnv->pConsole->DumpCVars( &cvarSender, 0 );
		return eCETR_Ok;
	}

private:
	IConsoleVarSink * m_pSink;
};

void AddCVarSync( IContextEstablisher * pEst, EContextViewState state, IConsoleVarSink * pSink )
{
	pEst->AddTask( state, new CCET_CVarSync(pSink) );
}
