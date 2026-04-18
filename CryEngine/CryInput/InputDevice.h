/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:	Some useful and shared functionality for input devices.
-------------------------------------------------------------------------
History:
- Dec 16,2005:	Created by Marco Koegler

*************************************************************************/
#ifndef __INPUTDEVICE_H__
#define __INPUTDEVICE_H__
#pragma once

struct IInput;

struct IFFParams
{
	EDeviceId deviceId;
	float strengthA, strengthB;
	float timeInSeconds;
	IFFParams() : strengthA(0), strengthB(0), timeInSeconds(0)
	{
		deviceId = eDI_Unknown;
	}
};

struct IInputDevice
{
	// implement virtual destructor just for safety
	virtual ~IInputDevice(){}

	virtual const CCryName& GetDeviceName() const	= 0;
	virtual EDeviceId GetDeviceId() const	= 0;
	//! init
	virtual bool	Init() = 0;
	//! update
	virtual void	Update() = 0;
	//! set force feedback (returns true if successful)
	virtual bool SetForceFeedback(IFFParams params) = 0;
	//! check for key pressed and held
	virtual bool	InputState(const TKeyName& key, EInputState state) = 0;
	//! set/unset DirectInput to exclusive mode
	virtual bool	SetExclusiveMode(bool value) = 0;
	//! clear the key (pressed) state
	virtual void	ClearKeyState() = 0;
	virtual const char *GetKeyName(const SInputEvent& event, bool bGUI=0) = 0;
	virtual const wchar_t *GetOSKeyName(const SInputEvent& event) = 0;
	virtual SInputSymbol* LookupSymbol(EKeyId id) const = 0;
	virtual bool IsOfDeviceType( EInputDeviceType type ) const = 0;
	virtual void Enable(bool enable) = 0;
	virtual bool IsEnabled() const = 0;
	virtual void OnLanguageChange() = 0;
};

class CInputDevice : public IInputDevice
{
public:
	CInputDevice(IInput& input, const char* deviceName);
	virtual ~CInputDevice();

	// IInputDevice
	virtual const CCryName& GetDeviceName() const	{	return	m_deviceName;	}
	virtual EDeviceId GetDeviceId() const	{ return m_deviceId; };
	virtual bool	Init()	{	return true;	}
	virtual void	Update();
	virtual bool	SetForceFeedback(IFFParams params){	return false; };
	virtual bool	InputState(const TKeyName& key, EInputState state);
	virtual bool	SetExclusiveMode(bool value)	{ return true;	}
	virtual void	ClearKeyState();
	virtual const char *GetKeyName(const SInputEvent& event, bool bGUI=0);
	virtual const wchar_t *GetOSKeyName(const SInputEvent& event);
	virtual SInputSymbol* LookupSymbol(EKeyId id) const;
	virtual void Enable(bool enable);
	virtual bool IsEnabled() const {	return m_enabled;	}
	virtual void OnLanguageChange() {};
	// ~IInputDevice


protected:
	IInput& GetIInput() const	{	return	m_input;	}

	// device dependent id management
	//const TKeyName&	IdToName(TKeyId id) const;
	SInputSymbol*		IdToSymbol(EKeyId id) const;
	uint32					NameToId(const TKeyName& name) const;
	SInputSymbol*		NameToSymbol(const TKeyName& name) const;
	SInputSymbol*		DevSpecIdToSymbol(uint32 devSpecId) const;
	SInputSymbol*		MapSymbol(uint32 deviceSpecificId, EKeyId keyId, const TKeyName& name, SInputSymbol::EType type = SInputSymbol::Button, uint32 user = 0);

protected:
	EDeviceId                 m_deviceId;
	bool											m_enabled;
private:
	IInput&										m_input;				// point to input system in use
	CCryName									m_deviceName;		// name of the device (used for input binding)

	typedef std::map<TKeyName, uint32>				TNameToIdMap;
	typedef std::map<TKeyName, SInputSymbol*>	TNameToSymbolMap;
	typedef std::map<EKeyId, SInputSymbol*>		TIdToSymbolMap;
	typedef std::map<uint32, SInputSymbol*>		TDevSpecIdToSymbolMap;

	TNameToIdMap			m_nameToId;
	TNameToSymbolMap	m_nameToInfo;
	TIdToSymbolMap		m_idToInfo;
	TDevSpecIdToSymbolMap	m_devSpecIdToSymbol;
};

#endif //__INPUTDEVICE_H__
