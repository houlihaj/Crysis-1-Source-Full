/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:	BaseInput implementation. This is primarily a "get things to
							compile" thing for new platforms which haven't gotten a
							real input implementation done yet. It implements all
							the listener functionality and offers a uniform device
							interface using IInputDevice.
-------------------------------------------------------------------------
History:
- Dec 05,2005:	Created by Marco Koegler

*************************************************************************/
#ifndef __BASEINPUT_H__
#define __BASEINPUT_H__
#pragma once

#include <platform.h>
struct	IInputDevice;
class		CInputCVars;

class CBaseInput : public IInput, public ISystemEventListener
{
public:
	CBaseInput();
	virtual ~CBaseInput();

	// IInput
	// stub implementation
	virtual bool	Init();
	virtual void	Update(bool bFocus);
	virtual void	ShutDown();
	virtual void	SetExclusiveMode(EDeviceId deviceId, bool exclusive, void *pUser);
	virtual bool	InputState(const TKeyName& keyName, EInputState state);
	virtual const char *GetKeyName(const SInputEvent& event, bool bGUI=0);
	virtual SInputSymbol* LookupSymbol( EDeviceId deviceId, EKeyId keyId );
	virtual const wchar_t* GetOSKeyName(const SInputEvent& event);
	virtual void ClearKeyState();
	virtual void RetriggerKeyState();
	virtual bool Retriggering()	{	return m_retriggering;	}
	virtual bool HasInputDeviceOfType( EInputDeviceType type );

	// listener functions (implemented)
	virtual void	AddEventListener( IInputEventListener *pListener );
	virtual void	RemoveEventListener( IInputEventListener *pListener );
	virtual void	AddConsoleEventListener( IInputEventListener *pListener );
	virtual void	RemoveConsoleEventListener( IInputEventListener *pLstener );
	virtual void	SetExclusiveListener( IInputEventListener *pListener );
	virtual IInputEventListener *GetExclusiveListener();
	virtual void	EnableEventPosting( bool bEnable );
	virtual void	PostInputEvent( const SInputEvent &event );
	virtual void	ForceFeedbackEvent( const SFFOutputEvent &event );
	virtual void	EnableDevice( EDeviceId deviceId, bool enable);
	// ~IInput

	// ISystemEventListener
	virtual void OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam );
	// ~ISystemEventListener

	int		GetModifiers() const	{ return m_modifiers;	}
	void	SetModifiers(int modifiers)	{		m_modifiers = modifiers;	}

protected:
	typedef std::vector<IInputDevice*>	TInputDevices;
	bool	AddInputDevice(IInputDevice* pDevice);
	void	PostHoldEvents();

private:
	// listener functionality
	typedef std::list<IInputEventListener*> TInputEventListeners;
	typedef std::vector<SInputSymbol*> TInputSymbols;
	TInputSymbols					m_holdSymbols;
	TInputEventListeners	m_listeners;
	TInputEventListeners	m_consoleListeners;
	IInputEventListener*	m_pExclusiveListener;

	bool									m_enableEventPosting;
	bool									m_retriggering;
	CCryMutex							m_postInputEventMutex;

	// input device management
	TInputDevices					m_inputDevices;

	// marcok: a bit nasty ... but I want to restrict access to CKeyboard ... this makes sure that
	// even mouse events could have a modifier
	int										m_modifiers;

	//CVars
	CInputCVars*					m_pCVars;
};

#endif //__BASEINPUT_H__

