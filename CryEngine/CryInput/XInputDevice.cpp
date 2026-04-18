#include "StdAfx.h"
#include "XInputDevice.h"
#include <IConsole.h>
#include <platform.h>
#include <CryThread.h>

#if defined(USE_DXINPUT) || defined(USE_XENONINPUT)

GUID	CXInputDevice::ProductGUID = { 0x028e045E, 0x0000, 0x0000, { 0x00,0x00,0x50,0x49,0x44,0x56,0x49,0x44} };
#define INPUT_DEADZONE  ( 0.24f * FLOAT(0x7FFF) )  // Default to 24% of the +/- 32767 range.   This is a reasonable default value but can be altered if needed.

const char* deviceNames[] =
{
	"xinput0",
	"xinput1",
	"xinput2",
	"xinput3"
};

// a few internal ids
#define _XINPUT_GAMEPAD_LEFT_TRIGGER			(1 << 17)
#define _XINPUT_GAMEPAD_RIGHT_TRIGGER			(1 << 18)
#define _XINPUT_GAMEPAD_LEFT_THUMB_X			(1 << 19)
#define _XINPUT_GAMEPAD_LEFT_THUMB_Y			(1 << 20)
#define _XINPUT_GAMEPAD_RIGHT_THUMB_X			(1 << 21)
#define _XINPUT_GAMEPAD_RIGHT_THUMB_Y			(1 << 22)
#define _XINPUT_GAMEPAD_LEFT_TRIGGER_BTN	(1 << 23)	// left trigger usable as a press/release button
#define _XINPUT_GAMEPAD_RIGHT_TRIGGER_BTN	(1 << 24)	// right trigger usable as a press/release button
#define _XINPUT_GAMEPAD_LEFT_THUMB_UP			(1 << 25)
#define _XINPUT_GAMEPAD_LEFT_THUMB_DOWN		((1 << 25) + 1)
#define _XINPUT_GAMEPAD_LEFT_THUMB_LEFT		((1 << 25) + 2)
#define _XINPUT_GAMEPAD_LEFT_THUMB_RIGHT	((1 << 25) + 3)
#define _XINPUT_GAMEPAD_RIGHT_THUMB_UP		(1 << 26)
#define _XINPUT_GAMEPAD_RIGHT_THUMB_DOWN	((1 << 26) + 1)
#define _XINPUT_GAMEPAD_RIGHT_THUMB_LEFT	((1 << 26) + 2)
#define _XINPUT_GAMEPAD_RIGHT_THUMB_RIGHT	((1 << 26) + 3)

CCryMutex g_xinputThreadMutex;
bool g_bQuitConnectionMonitor = false;
CCryThread *g_pConnectionThread = 0;
//int g_numControllers = 0;
bool g_bConnected[4] = { false };

void g_connectionWorker(void *param)
{
	CryThreadSetName( -1,"InputWorker" );

	IInput *pInput = (IInput*)param;
	XINPUT_CAPABILITIES caps;

	while (!g_bQuitConnectionMonitor)
	{
		for (DWORD i = 0; i < 4; ++i)
		{
			DWORD r = XInputGetCapabilities(i, XINPUT_FLAG_GAMEPAD, &caps);
			{
				CCryMutex::CLock lock(g_xinputThreadMutex);
				g_bConnected[i] = r == ERROR_SUCCESS;
			}
		}

		Sleep(1000);
	}
}

CXInputDevice::CXInputDevice(IInput& input, int deviceNo) : CInputDevice(input, deviceNames[deviceNo]),
m_deviceNo(deviceNo), m_connected(false)
{
	memset( &m_state, 0, sizeof(XINPUT_STATE) );
	m_deviceId = eDI_XI;
	SetVibration(0, 0, 0);
}

CXInputDevice::~CXInputDevice()
{
	SetVibration(0, 0, 0);

	if (g_pConnectionThread)
	{
		g_bQuitConnectionMonitor = true;
		SAFE_DELETE(g_pConnectionThread);
	}
}

bool CXInputDevice::Init()
{
	// setup input event names
	MapSymbol(XINPUT_GAMEPAD_DPAD_UP, eKI_XI_DPadUp, "xi_dpad_up");
	MapSymbol(XINPUT_GAMEPAD_DPAD_DOWN, eKI_XI_DPadDown, "xi_dpad_down");
	MapSymbol(XINPUT_GAMEPAD_DPAD_LEFT, eKI_XI_DPadLeft, "xi_dpad_left");
	MapSymbol(XINPUT_GAMEPAD_DPAD_RIGHT, eKI_XI_DPadRight, "xi_dpad_right");
	MapSymbol(XINPUT_GAMEPAD_START, eKI_XI_Start, "xi_start");
	MapSymbol(XINPUT_GAMEPAD_BACK, eKI_XI_Back, "xi_back");
	MapSymbol(XINPUT_GAMEPAD_LEFT_THUMB, eKI_XI_ThumbL, "xi_thumbl");
	MapSymbol(XINPUT_GAMEPAD_RIGHT_THUMB, eKI_XI_ThumbR, "xi_thumbr");
	MapSymbol(XINPUT_GAMEPAD_LEFT_SHOULDER, eKI_XI_ShoulderL, "xi_shoulderl");
	MapSymbol(XINPUT_GAMEPAD_RIGHT_SHOULDER, eKI_XI_ShoulderR, "xi_shoulderr");
	MapSymbol(XINPUT_GAMEPAD_A, eKI_XI_A, "xi_a");
	MapSymbol(XINPUT_GAMEPAD_B, eKI_XI_B, "xi_b");
	MapSymbol(XINPUT_GAMEPAD_X, eKI_XI_X, "xi_x");
	MapSymbol(XINPUT_GAMEPAD_Y, eKI_XI_Y, "xi_y");
	MapSymbol(_XINPUT_GAMEPAD_LEFT_TRIGGER, eKI_XI_TriggerL, "xi_triggerl",	SInputSymbol::Trigger);
	MapSymbol(_XINPUT_GAMEPAD_RIGHT_TRIGGER, eKI_XI_TriggerR, "xi_triggerr",	SInputSymbol::Trigger);
	MapSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_X, eKI_XI_ThumbLX, "xi_thumblx",		SInputSymbol::Axis);
	MapSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_Y, eKI_XI_ThumbLY, "xi_thumbly",		SInputSymbol::Axis);
	MapSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_UP, eKI_XI_ThumbLUp, "xi_thumbl_up");
	MapSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_DOWN, eKI_XI_ThumbLDown, "xi_thumbl_down");
	MapSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_LEFT, eKI_XI_ThumbLLeft, "xi_thumbl_left");
	MapSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_RIGHT, eKI_XI_ThumbLRight, "xi_thumbl_right");
	MapSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_X, eKI_XI_ThumbRX, "xi_thumbrx",		SInputSymbol::Axis);
	MapSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_Y, eKI_XI_ThumbRY, "xi_thumbry",		SInputSymbol::Axis);
	MapSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_UP, eKI_XI_ThumbRUp, "xi_thumbr_up");
	MapSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_DOWN, eKI_XI_ThumbRDown, "xi_thumbr_down");
	MapSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_LEFT, eKI_XI_ThumbRLeft, "xi_thumbr_left");
	MapSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_RIGHT, eKI_XI_ThumbRRight, "xi_thumbr_right");
	MapSymbol(_XINPUT_GAMEPAD_LEFT_TRIGGER_BTN, eKI_XI_TriggerLBtn, "xi_triggerl_btn");	// trigger usable as a button
	MapSymbol(_XINPUT_GAMEPAD_RIGHT_TRIGGER_BTN, eKI_XI_TriggerRBtn, "xi_triggerr_btn");	// trigger usable as a button

	if (!g_pConnectionThread)
	{
		g_pConnectionThread = new CCryThread(g_connectionWorker, &GetIInput());
	}

	m_leftMotorRumble = m_rightMotorRumble = 0.0f;

	return true;
}

void CXInputDevice::Update()
{
	FUNCTION_PROFILER( GetISystem(),PROFILE_INPUT );

	bool connected = false;

	{
		CCryMutex::CLock lock(g_xinputThreadMutex);
		connected = g_bConnected[m_deviceNo];
	}

	UpdateConnectedState(connected);

	// interpret input
	if (m_connected)
	{
		XINPUT_STATE state;
		memset( &state, 0, sizeof(XINPUT_STATE) );
		if ( ERROR_SUCCESS != XInputGetState(m_deviceNo, &state) )
			return;

		float now = GetISystem()->GetITimer()->GetFrameStartTime().GetSeconds();
		if((m_fVibrationTimer && m_fVibrationTimer < now) || g_pInputCVars->i_forcefeedback == 0 || gEnv->pSystem->IsPaused())
		{
			m_fVibrationTimer = 0;
			SetVibration(0, 0, 0.0f);
		}

		if (state.dwPacketNumber != m_state.dwPacketNumber)
		{
			SInputEvent event;
			SInputSymbol*	pSymbol = 0;

			if( (state.Gamepad.sThumbLX < INPUT_DEADZONE && state.Gamepad.sThumbLX > -INPUT_DEADZONE) && 
				(state.Gamepad.sThumbLY < INPUT_DEADZONE && state.Gamepad.sThumbLY > -INPUT_DEADZONE) ) 
			{	
				state.Gamepad.sThumbLX = 0;
				state.Gamepad.sThumbLY = 0;
			}

			if( (state.Gamepad.sThumbRX < INPUT_DEADZONE && state.Gamepad.sThumbRX > -INPUT_DEADZONE) && 
				(state.Gamepad.sThumbRY < INPUT_DEADZONE && state.Gamepad.sThumbRY > -INPUT_DEADZONE) ) 
			{
				state.Gamepad.sThumbRX = 0;
				state.Gamepad.sThumbRY = 0;
			}

			// compare new values against cached value and only send out changes as new input
			WORD buttonsChange = m_state.Gamepad.wButtons ^ state.Gamepad.wButtons;
			if (buttonsChange)
			{	
				for (int i = 0; i < 16; ++i)
				{
					uint32 id = (1 << i);
					if (buttonsChange & id && (pSymbol = DevSpecIdToSymbol(id)))
					{
						pSymbol->PressEvent((state.Gamepad.wButtons & id) != 0);
						pSymbol->AssignTo(event);
						event.deviceId = eDI_XI;
						GetIInput().PostInputEvent(event);
					}
				}
			}

			// now we have done the digital buttons ... let's do the analog stuff
			if (m_state.Gamepad.bLeftTrigger != state.Gamepad.bLeftTrigger)
			{
				pSymbol = DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_TRIGGER);
				pSymbol->ChangeEvent(state.Gamepad.bLeftTrigger / 255.0f);
				pSymbol->AssignTo(event);
				event.deviceId = eDI_XI;
				GetIInput().PostInputEvent(event);

				//--- Check previous and current trigger against threshold for digital press/release event
				bool bIsPressed=state.Gamepad.bLeftTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD ? true : false;
				bool bWasPressed=m_state.Gamepad.bLeftTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD ? true : false;
				if(bIsPressed!=bWasPressed)
				{
					pSymbol=DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_TRIGGER_BTN);
					pSymbol->PressEvent(bIsPressed);
					pSymbol->AssignTo(event);
					event.deviceId = eDI_XI;
					GetIInput().PostInputEvent(event);
				}
			}
			if (m_state.Gamepad.bRightTrigger != state.Gamepad.bRightTrigger)
			{
				pSymbol = DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_TRIGGER);
				pSymbol->ChangeEvent(state.Gamepad.bRightTrigger / 255.0f);
				pSymbol->AssignTo(event);
				event.deviceId = eDI_XI;
				GetIInput().PostInputEvent(event);

				//--- Check previous and current trigger against threshold for digital press/release event
				bool bIsPressed=state.Gamepad.bRightTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
				bool bWasPressed=m_state.Gamepad.bRightTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
				if(bIsPressed!=bWasPressed)
				{
					pSymbol=DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_TRIGGER_BTN);
					pSymbol->PressEvent(bIsPressed);
					pSymbol->AssignTo(event);
					event.deviceId = eDI_XI;
					GetIInput().PostInputEvent(event);
				}
			}
			if (m_state.Gamepad.sThumbLX != state.Gamepad.sThumbLX)
			{
				pSymbol = DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_X);
				if(state.Gamepad.sThumbLX==0.f)
					pSymbol->ChangeEvent(0.f);
				else
					pSymbol->ChangeEvent((state.Gamepad.sThumbLX + 32768) / 32767.5f - 1.0f);
				pSymbol->AssignTo(event);
				event.deviceId = eDI_XI;
				GetIInput().PostInputEvent(event);
				//--- Check previous and current state to generate digital press/release event
				static SInputSymbol* pSymbolLeft = DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_LEFT);
				ProcessAnalogStick(pSymbolLeft, m_state.Gamepad.sThumbLX, state.Gamepad.sThumbLX, -25000);
				static SInputSymbol* pSymbolRight = DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_RIGHT);
				ProcessAnalogStick(pSymbolRight, m_state.Gamepad.sThumbLX, state.Gamepad.sThumbLX, 25000);
			}
			if (m_state.Gamepad.sThumbLY != state.Gamepad.sThumbLY)
			{
				pSymbol = DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_Y);
				if(state.Gamepad.sThumbLY==0.f)
					pSymbol->ChangeEvent(0.f);
				else
					pSymbol->ChangeEvent((state.Gamepad.sThumbLY + 32768) / 32767.5f - 1.0f);
				pSymbol->AssignTo(event);
				event.deviceId = eDI_XI;
				GetIInput().PostInputEvent(event);
				//--- Check previous and current state to generate digital press/release event
				static SInputSymbol* pSymbolUp = DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_UP);
				ProcessAnalogStick(pSymbolUp, m_state.Gamepad.sThumbLY, state.Gamepad.sThumbLY, 25000);
				static SInputSymbol* pSymbolDown = DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_DOWN);
				ProcessAnalogStick(pSymbolDown, m_state.Gamepad.sThumbLY, state.Gamepad.sThumbLY, -25000);
			}
			if (m_state.Gamepad.sThumbRX != state.Gamepad.sThumbRX)
			{
				pSymbol = DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_X);
				if(state.Gamepad.sThumbRX==0.f)
					pSymbol->ChangeEvent(0.f);
				else
					pSymbol->ChangeEvent((state.Gamepad.sThumbRX + 32768) / 32767.5f - 1.0f);
				pSymbol->AssignTo(event);
				event.deviceId = eDI_XI;
				GetIInput().PostInputEvent(event);
				//--- Check previous and current state to generate digital press/release event
				static SInputSymbol* pSymbolLeft = DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_LEFT);
				ProcessAnalogStick(pSymbolLeft, m_state.Gamepad.sThumbRX, state.Gamepad.sThumbRX, -25000);
				static SInputSymbol* pSymbolRight = DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_RIGHT);
				ProcessAnalogStick(pSymbolRight, m_state.Gamepad.sThumbRX, state.Gamepad.sThumbRX, 25000);
			}
			if (m_state.Gamepad.sThumbRY != state.Gamepad.sThumbRY)
			{
				pSymbol = DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_Y);
				if(state.Gamepad.sThumbRY==0.f)
					pSymbol->ChangeEvent(0.f);
				else
					pSymbol->ChangeEvent((state.Gamepad.sThumbRY + 32768) / 32767.5f - 1.0f);
				pSymbol->AssignTo(event);
				event.deviceId = eDI_XI;
				GetIInput().PostInputEvent(event);
				//--- Check previous and current state to generate digital press/release event
				static SInputSymbol* pSymbolUp = DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_UP);
				ProcessAnalogStick(pSymbolUp, m_state.Gamepad.sThumbRY, state.Gamepad.sThumbRY, 25000);
				static SInputSymbol* pSymbolDown = DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_DOWN);
				ProcessAnalogStick(pSymbolDown, m_state.Gamepad.sThumbRY, state.Gamepad.sThumbRY, -25000);
			}

			// update cache
			m_state = state;
		}
	}
}

void CXInputDevice::ClearKeyState()
{
	m_fVibrationTimer = 0;
	SetVibration(0, 0, 0.0f);
	CInputDevice::ClearKeyState();
}

void CXInputDevice::UpdateConnectedState(bool isConnected)
{
	if (m_connected != isConnected)
	{
		SInputEvent event;
		event.deviceId = eDI_XI;
		event.state = eIS_Changed;

		// state has changed, emit event here
		if (isConnected)
		{
			// connect
			//printf("xinput%d connected\n", m_deviceNo);
			event.keyId = eKI_XI_Connect;
			event.keyName = "connect";
		}
		else
		{
			// disconnect
			//printf("xinput%d disconnected\n", m_deviceNo);
			event.keyId = eKI_XI_Disconnect;
			event.keyName = "disconnect";
		}
		m_connected = isConnected;
		GetIInput().PostInputEvent(event);
	}
}

bool CXInputDevice::SetForceFeedback(IFFParams params)
{
	return SetVibration(min(1.0f, fabsf(params.strengthA)) * 65535, min(1.0f, fabsf(params.strengthB)) * 65535, params.timeInSeconds);
}

bool CXInputDevice::SetVibration(USHORT leftMotor, USHORT rightMotor, float timing)
{
	//if(g_bConnected[m_deviceNo])
	if (m_connected)
	{
		XINPUT_VIBRATION vibration;

		float now = GetISystem()->GetITimer()->GetFrameStartTime().GetSeconds();

		if(m_fVibrationTimer) //old rumble active
		{
			float oldRumbleRatio = 1.0f;
			if (timing > 0.0f)
				oldRumbleRatio = min(1.0f, (fabsf(m_fVibrationTimer - now) / timing));

			float newLeftMotor = 0; 
			float newRightMotor = 0;
			newLeftMotor += m_leftMotorRumble * oldRumbleRatio;
			newRightMotor += m_leftMotorRumble * oldRumbleRatio;
			leftMotor = max(leftMotor, (USHORT)newLeftMotor);
			rightMotor = max(rightMotor, (USHORT)newRightMotor);
		}

		ZeroMemory( &vibration, sizeof(XINPUT_VIBRATION) );
		vibration.wLeftMotorSpeed = m_leftMotorRumble = leftMotor;
		vibration.wRightMotorSpeed = m_rightMotorRumble = rightMotor;
		XInputSetState( m_deviceNo, &vibration );
		if(timing)
			m_fVibrationTimer = now + timing;
		else
			m_fVibrationTimer = 0.0f;
		return true;
	}
	return false;
}

void CXInputDevice::ProcessAnalogStick(SInputSymbol* pSymbol, SHORT prev, SHORT current, SHORT threshold)
{
	bool send = false;
	if (prev >= threshold && current < threshold)
	{
		// release
		send = true;
		pSymbol->PressEvent(false);
	}
	else if (prev <= threshold && current > threshold)
	{
		// press
		send = true;
		pSymbol->PressEvent(true);
	}

	if (send)
	{
		SInputEvent event;
		pSymbol->AssignTo(event);
		event.deviceId = eDI_XI;
		GetIInput().PostInputEvent(event);
	}
}


#endif //defined(USE_DXINPUT) || defined(USE_XENONINPUT)
