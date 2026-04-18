#include "StdAfx.h"
#include "NetworkCVars.h"

CNetworkCVars * CNetworkCVars::s_pThis = 0;

CNetworkCVars::CNetworkCVars()
{
	assert(!s_pThis);
	s_pThis = this;

	IConsole * c = gEnv->pConsole;

	c->Register("g_breakagelog", &BreakageLog, 0, VF_CHEAT, "Log break events");
	c->Register("cl_voice_volume", &VoiceVolume, 1.0f, 0, "Set VOIP playback volume: 0-1");
	c->Register("net_phys_pingsmooth", &PhysSyncPingSmooth, 0.1f);
	c->Register("net_phys_lagsmooth", &PhysSyncLagSmooth, 0.1f);
	c->Register( "net_phys_debug", &PhysDebug, 0, VF_CHEAT );
	c->Register("g_breaktimeoutframes", &BreakTimeoutFrames, 140, 0);
}
