/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------

Rip off NetworkCVars, general CryAction CVars should be here...

-------------------------------------------------------------------------
History:
- 6:6:2007   Created by Benito G.R.

*************************************************************************/
#ifndef __CRYACTIONCVARS_H__
#define __CRYACTIONCVARS_H__

#pragma once

#include "IConsole.h"
#include "IAIRecorder.h"

class CCryActionCVars
{
public:

	float playerInteractorRadius;  //Controls CInteractor action radius
#ifdef AI_LOG_SIGNALS
	int aiLogSignals;
	float aiMaxSignalDuration;
#endif

	static ILINE CCryActionCVars& Get()
	{
		assert(s_pThis);
		return *s_pThis;
	}

private:
	friend class CCryAction; // Our only creator

	CCryActionCVars(); // singleton stuff
	~CCryActionCVars();

	static CCryActionCVars* s_pThis;

};

#endif // __CRYACTIONCVARS_H__
