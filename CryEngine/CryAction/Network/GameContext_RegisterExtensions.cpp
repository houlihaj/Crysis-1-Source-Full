#include "StdAfx.h"
#include "GameContext.h"
#include "VoiceListener.h"

void CGameContext::RegisterExtensions( IGameFramework * pFW )
{
	REGISTER_FACTORY(pFW, "VoiceListener", CVoiceListener, false);
}
