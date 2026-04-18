////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2007.
// -------------------------------------------------------------------------
//  File name:   SubtitleManager.h
//  Version:     v1.00
//  Created:     29/01/2007 by AlexL.
//  Compilers:   Visual Studio.NET 2005
//  Description: Subtitle Manager Implementation 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SUBTITLEMANAGER_H__
#define __SUBTITLEMANAGER_H__
#pragma once

#include "ISubtitleManager.h"
#include <ISound.h>

class CSubtitleManager : public ISubtitleManager, public ISoundSystemEventListener
{
public:
	CSubtitleManager();
	virtual ~CSubtitleManager();

	// ISubtitleManager
	virtual void SetHandler(ISubtitleHandler* pHandler) { m_pHandler = pHandler; }
	virtual ISubtitleHandler* GetHandler() { return m_pHandler ? m_pHandler : &m_dummyHandler; }
	virtual void SetEnabled(bool bEnabled);
	virtual void SetAutoMode(bool bOn);
	virtual void ShowSubtitle(ISound* pSound, bool bShow);
	virtual void ShowSubtitle(const char* subtitleLabel, bool bShow);
	// ~ISubtitleManager

	// ISoundSystemEventListener
	virtual void OnSoundSystemEvent(ESoundSystemCallbackEvent event, ISound *pSound);
	// ~ISoundSystemEventListener

protected:
	struct CSubtitleHandlerDummy : public ISubtitleHandler
	{
		CSubtitleHandlerDummy() {}
		void ShowSubtitle(ISound* pSound, bool bShow) {}
		void ShowSubtitle(const char* subtitleLabel, bool bShow) {}
	};
	CSubtitleHandlerDummy m_dummyHandler;
	ISubtitleHandler* m_pHandler;
	bool m_bEnabled;
	bool m_bAutoMode;

};

#endif // __SUBTITLEMANAGER_H__