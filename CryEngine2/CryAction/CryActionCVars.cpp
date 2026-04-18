#include "StdAfx.h"
#include "CryActionCVars.h"
#include "IAIRecorder.h"

CCryActionCVars * CCryActionCVars::s_pThis = 0;

CCryActionCVars::CCryActionCVars()
{
	assert(!s_pThis);
	s_pThis = this;

	IConsole * console = gEnv->pConsole;

	console->Register("g_playerInteractorRadius", &playerInteractorRadius, 2.0f, VF_CHEAT, "Maximum radius at which player can interact with other entities");
#ifdef AI_LOG_SIGNALS
	console->Register("ai_LogSignals", &aiLogSignals, 0, VF_CHEAT, "Maximum radius at which player can interact with other entities");
	console->Register("ai_MaxSignalDuration", &aiMaxSignalDuration, 3.f, VF_CHEAT, "Maximum radius at which player can interact with other entities");
#endif
}

CCryActionCVars::~CCryActionCVars()
{
	assert (s_pThis != 0);
	s_pThis = 0;

	IConsole *pConsole = gEnv->pConsole;

	pConsole->UnregisterVariable("g_playerInteractorRadius", true);
#ifdef AI_LOG_SIGNALS
	pConsole->UnregisterVariable("ai_LogSignals", true);
#endif
}