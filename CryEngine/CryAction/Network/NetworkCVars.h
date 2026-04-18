#ifndef __NETWORKCVARS_H__
#define __NETWORKCVARS_H__

#pragma once

#include "IConsole.h"

class CNetworkCVars
{
public:
	int BreakageLog;
	float VoiceVolume;
	float PhysSyncPingSmooth;
	float PhysSyncLagSmooth;
	int PhysDebug;
	int BreakTimeoutFrames;

	static ILINE CNetworkCVars& Get()
	{
		assert(s_pThis);
		return *s_pThis;
	}

private:
	friend class CCryAction; // Our only creator

	CNetworkCVars(); // singleton stuff
	~CNetworkCVars();
	CNetworkCVars(const CNetworkCVars&);
	CNetworkCVars& operator= (const CNetworkCVars&);

	static CNetworkCVars* s_pThis;
};

#endif
