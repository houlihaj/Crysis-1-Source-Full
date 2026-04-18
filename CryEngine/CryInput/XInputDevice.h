#ifndef __XINPUTDEVICE_H__
#define __XINPUTDEVICE_H__
#pragma once

#include"InputDevice.h"

#if defined(USE_DXINPUT) || defined(USE_XENONINPUT)

#if defined(USE_DXINPUT)
#include <xinput.h>
#pragma comment(lib, "xinput.lib")
#elif defined(USE_XENONINPUT)
#include <xtl.h>
#endif

struct ICVar;

/*
Device: 'XBOX 360 For Windows (Controller)' - 'XBOX 360 For Windows (Controller)'
Product GUID:  {028E045E-0000-0000-0000-504944564944}
Instance GUID: {65429170-3725-11DA-8001-444553540000}
*/

class CXInputDevice : public CInputDevice
{
public:
	CXInputDevice(IInput& pInput, int deviceNo);
	virtual ~CXInputDevice();

	// IInputDevice overrides
	virtual bool Init();
	virtual void Update();
	virtual void ClearKeyState();
	virtual bool SetForceFeedback(IFFParams params);
	virtual bool IsOfDeviceType( EInputDeviceType type ) const { return type == eIDT_Gamepad && m_connected; }
	// ~IInputDevice overrides

private:
	void UpdateConnectedState(bool isConnected);
	//void AddInputItem(const IInput::InputItem& item);

	//triggers the speed of the vibration motors -> leftMotor is for low frequencies, the right one is for high frequencies
	bool SetVibration(USHORT leftMotor = 0, USHORT rightMotor = 0, float timing = 0);
	void ProcessAnalogStick(SInputSymbol* pSymbol, SHORT prev, SHORT current, SHORT threshold);

	int							m_deviceNo; //!< device number (from 0 to 3) for this XInput device
	bool						m_connected;
	XINPUT_STATE		m_state;

	float						m_leftMotorRumble, m_rightMotorRumble;
	float						m_fVibrationTimer;
	//std::vector<IInput::InputItem>	mInputQueue;	//!< queued inputs

public:
	static GUID		ProductGUID;
};

#endif //defined(USE_DXINPUT) || defined(USE_XENONINPUT)

#endif //XINPUTDEVICE_H__
