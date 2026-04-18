/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:	Input implementation for Linux using SDL
-------------------------------------------------------------------------
History:
- Jun 09,2006:	Created by Sascha Demetrio

*************************************************************************/
#ifndef __LINUXINPUT_H__
#define __LINUXINPUT_H__
#pragma once

#ifdef USE_LINUXINPUT

#include "BaseInput.h"
#include "InputDevice.h"

class ILog;

class CLinuxInput : public CBaseInput
{
public:
	CLinuxInput(ISystem *pSystem); 

	virtual bool Init();
	virtual void ShutDown();

private:
	ISystem *m_pSystem;
	ILog *m_pLog;
};

class CLinuxInputDevice : public CInputDevice
{
public:
	CLinuxInputDevice(CLinuxInput& input, const char* deviceName);
	virtual ~CLinuxInputDevice();

	CLinuxInput& GetLinuxInput() const;

private:
	CLinuxInput &m_linuxInput;
};

struct ILog;
struct ICVar;

class CLinuxKeyboardMouse : public CLinuxInputDevice
{
public:
	CLinuxKeyboardMouse(CLinuxInput &input);
	virtual ~CLinuxKeyboardMouse();

	virtual bool Init();
	virtual void Update();
	virtual const char *GetKeyName(const SInputEvent &event, bool bGUI);
	virtual bool IsOfDeviceType( EInputDeviceType type ) { return type == eIDT_Keyboard || type == eIDT_Mouse; }

private:
	unsigned char Event2ASCII(const SInputEvent &event);
	void SetupKeyNames();
	static int ConvertModifiers(unsigned);
	void GrabInput();
	void UngrabInput();
	void PostEvent(SInputSymbol *pSymbol, unsigned keyMod = ~0);

	unsigned m_lastKeySym;
	int m_lastMod;
	unsigned m_lastUNICODE;

	ILog *m_pLog;
	ISystem *m_pSystem;
	IRenderer *m_pRenderer;

	unsigned m_posX, m_posY;

	bool m_bGrabInput;
};

#endif // USE_LINUXINPUT

#endif // __LINUXINPUT_H__

// vim:ts=2:sw=2:tw=78

