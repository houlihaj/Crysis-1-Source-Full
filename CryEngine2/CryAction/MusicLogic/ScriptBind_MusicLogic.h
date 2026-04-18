/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Script Binding for MusicLogic

-------------------------------------------------------------------------
History:
- 28:08:2006 : Created by TomasN
*************************************************************************/
#ifndef __SCRIPTBIND_MUSICLOGIC_H__
#define __SCRIPTBIND_MUSICLOGIC_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <IScriptSystem.h>
#include <ScriptHelpers.h>


struct ISystem;
class CMusicLogic;

class CScriptBind_MusicLogic : public CScriptableBase
{
public:
	CScriptBind_MusicLogic(CMusicLogic *pMusicLogic);
	virtual ~CScriptBind_MusicLogic();

protected:
	//int SetMusicState(IFunctionHandler *pH, float fIntensity, float fBoredom);
	int SetEvent(IFunctionHandler *pH, int eMusicEvent);
	int StartLogic(IFunctionHandler *pH);
	int StopLogic(IFunctionHandler *pH);
	//int PauseGame(IFunctionHandler *pH, bool pause);

private:
	void RegisterGlobals();
	void RegisterMethods();

	ISystem						*m_pSystem;
	IScriptSystem			*m_pSS;
	CMusicLogic       *m_pMusicLogic;
};

#endif //__SCRIPTBIND_GAME_H__