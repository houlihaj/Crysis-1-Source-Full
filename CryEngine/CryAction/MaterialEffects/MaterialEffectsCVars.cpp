/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:	CVars for the MaterialEffects subsystem
-------------------------------------------------------------------------
History:
- 02:03:2006  12:00 : Created by AlexL

*************************************************************************/

#include "StdAfx.h"
#include "MaterialEffectsCVars.h"
#include <CryAction.h>

#include "MaterialEffects.h"
#include "MaterialFGManager.h"

CMaterialEffectsCVars* CMaterialEffectsCVars::s_pThis = 0;

namespace
{
	void FGEffectsReload(IConsoleCmdArgs* pArgs)
	{
		CMaterialEffects* pMFX = static_cast<CMaterialEffects*>(CCryAction::GetCryAction()->GetIMaterialEffects());

		if(!pMFX)
			return;

		CMaterialFGManager* pMFGMgr = pMFX->GetFGManager();
		pMFGMgr->ReloadFlowGraphs();

	}

	void MFXReload(IConsoleCmdArgs* pArgs)
	{
		CMaterialEffects* pMFX = static_cast<CMaterialEffects*>(CCryAction::GetCryAction()->GetIMaterialEffects());

		if(!pMFX)
			return;

		pMFX->Load(""); // reloads current spreadsheet
	}

};

CMaterialEffectsCVars::CMaterialEffectsCVars()
{
	assert (s_pThis == 0);
	s_pThis = this;

	IConsole *pConsole = gEnv->pConsole;

	pConsole->Register( "mfx_ParticleImpactThresh", &mfx_ParticleImpactThresh, 2.0f, VF_CHEAT, "Impact threshold for particle effects. Default: 2.0" );
	pConsole->Register( "mfx_SoundImpactThresh", &mfx_SoundImpactThresh, 1.5f, VF_CHEAT, "Impact threshold for sound effects. Default: 1.5" );
	pConsole->Register( "mfx_RaisedSoundImpactThresh", &mfx_RaisedSoundImpactThresh, 3.5f, VF_CHEAT, "Impact threshold for sound effects if we're rolling. Default: 3.5" );
	pConsole->Register( "mfx_Debug", &mfx_Debug, 0, 0, "Turns on MaterialEffects debug messages. 1=Collisions, 2=Breakage, 3=Both" );
	pConsole->Register( "mfx_Enable", &mfx_Enable, 1, VF_CHEAT, "Enables MaterialEffects." );
	pConsole->Register( "mfx_pfx_minScale", &mfx_pfx_minScale, .5f, 0, "Min scale (when particle is close)");
	pConsole->Register( "mfx_pfx_maxScale", &mfx_pfx_maxScale, 1.5f, 0, "Max scale (when particle is far)");
	pConsole->Register( "mfx_pfx_maxDist", &mfx_pfx_maxDist, 35.0f, 0, "Max dist (how far away before scale is clamped)");
	pConsole->Register( "mfx_Timeout", &mfx_Timeout, 0.2f, 0, "Timeout (in seconds) to avoid playing effects too often");
	pConsole->Register( "mfx_EnableFGEffects", &mfx_EnableFGEffects, 1, VF_CHEAT, "Enabled Flowgraph based Material effects. Default: On" );
	pConsole->Register( "mfx_SerializeFGEffects", &mfx_SerializeFGEffects, 1, VF_CHEAT, "Serialize Flowgraph based effects. Default: On" );
	pConsole->Register("g_blood", &g_blood, 1, 0, "Toggle blood effects");

	//FlowGraph HUD effects
	pConsole->AddCommand("mfx_ReloadFGEffects", FGEffectsReload, 0, "Reload MaterialEffect's FlowGraphs");
	//Reload Excel Spreadsheet
	pConsole->AddCommand("mfx_Reload", MFXReload, 0, "Reload MFX Spreadsheet");
}

CMaterialEffectsCVars::~CMaterialEffectsCVars()
{
	assert (s_pThis != 0);
	s_pThis = 0;

	IConsole *pConsole = gEnv->pConsole;

	pConsole->RemoveCommand("mfx_Reload");
	pConsole->RemoveCommand("mfx_ReloadHUDEffects");
	
	pConsole->UnregisterVariable("mfx_ParticleImpactThresh", true);
	pConsole->UnregisterVariable("mfx_SoundImpactThresh", true);
	pConsole->UnregisterVariable("mfx_RaisedSoundImpactThresh", true);
	pConsole->UnregisterVariable("mfx_Debug", true);
	pConsole->UnregisterVariable("mfx_Enable", true);
	pConsole->UnregisterVariable("mfx_pfx_minScale", true);
	pConsole->UnregisterVariable("mfx_pfx_maxScale", true);
	pConsole->UnregisterVariable("mfx_pfx_maxDist", true);
	pConsole->UnregisterVariable("mfx_Timeout", true);
	pConsole->UnregisterVariable("mfx_EnableFGEffects", true);
	pConsole->UnregisterVariable("mfx_SerializeFGEffects", true);
	pConsole->UnregisterVariable("g_blood", true);
}