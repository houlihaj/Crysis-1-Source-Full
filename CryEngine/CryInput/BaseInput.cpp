/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:	BaseInput
-------------------------------------------------------------------------
History:
- Dec 05,2005:	Created by Marco Koegler

*************************************************************************/
#include "StdAfx.h"
#include "BaseInput.h"
#include "InputDevice.h"
#include "InputCVars.h"

void CleanupCryInput()
{
	if(gEnv->pInput)
		gEnv->pInput->ShutDown();
}

CBaseInput::CBaseInput() 
	: m_pExclusiveListener(0)
	, m_enableEventPosting(true)
	, m_retriggering(false)
	, m_modifiers(0)
	, m_pCVars(new CInputCVars())
{
	GetISystem()->GetISystemEventDispatcher()->RegisterListener(this);

	g_pInputCVars = m_pCVars;

	m_holdSymbols.reserve(64);
	atexit(CleanupCryInput);
}

CBaseInput::~CBaseInput()
{
	ShutDown();
	GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);
	delete m_pCVars;
	m_pCVars = 0;
	g_pInputCVars = 0;
}

bool CBaseInput::Init()
{
	m_modifiers = 0;
	// always pass
	return true;
}

void CBaseInput::Update(bool bFocus)
{
	FUNCTION_PROFILER( GetISystem(),PROFILE_INPUT );

	PostHoldEvents();
	
	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		if ((*i)->IsEnabled())
			(*i)->Update();
	}

	// send commit event after all input processing for this frame has finished
	SInputEvent event;
	event.modifiers = m_modifiers;
	event.deviceId = eDI_Unknown;
	event.state = eIS_Changed;
	event.value = 0;
	event.keyName = "commit";
	event.keyId = eKI_SYS_Commit;
	PostInputEvent(event);
}

void CBaseInput::ShutDown()
{
	std::for_each(m_inputDevices.begin(), m_inputDevices.end(), stl::container_object_deleter());
	m_inputDevices.clear();
}

void CBaseInput::SetExclusiveMode(EDeviceId deviceId, bool exclusive, void *pUser)
{
	if (g_pInputCVars->i_debug)
	{
		gEnv->pLog->Log("InputDebug: SetExclusiveMode(%d, %s)", deviceId, exclusive?"true":"false");
	}

	if(deviceId < m_inputDevices.size())
	{
		m_inputDevices[deviceId]->ClearKeyState();
		m_inputDevices[deviceId]->SetExclusiveMode(exclusive);

		// try to flush the device ... perform a dry update
		EnableEventPosting(false);
		m_inputDevices[deviceId]->Update();
		EnableEventPosting(true);
	}
}

bool CBaseInput::InputState(const TKeyName& keyName, EInputState state)
{
	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		if ((*i)->InputState(keyName, state)) return true;
	}
	return false;
}

const char* CBaseInput::GetKeyName(const SInputEvent& event, bool bGUI)
{
	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		const char* ret = (*i)->GetKeyName(event, bGUI);
		if (ret) return ret;
	}

	return "";
}

const wchar_t* CBaseInput::GetOSKeyName(const SInputEvent& event)
{
	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		const wchar_t* ret = (*i)->GetOSKeyName(event);
		if (ret && *ret) 
			return ret;
	}
	return L"";
}

//////////////////////////////////////////////////////////////////////////
SInputSymbol* CBaseInput::LookupSymbol( EDeviceId deviceId, EKeyId keyId )
{
	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		IInputDevice *pDevice = *i;
		if (pDevice->GetDeviceId() == deviceId)
			return pDevice->LookupSymbol(keyId);
	}

	return NULL;
}


void CBaseInput::ClearKeyState()
{
	if (g_pInputCVars->i_debug)
	{
		gEnv->pLog->Log("InputDebug: ClearKeyState");
	}
	m_modifiers = 0;
	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		(*i)->ClearKeyState();
	}
	m_retriggering = false;

	m_holdSymbols.clear();
	if (g_pInputCVars->i_debug)
	{
		gEnv->pLog->Log("InputDebug: ~ClearKeyState");
	}
}

void CBaseInput::RetriggerKeyState()
{
	if (g_pInputCVars->i_debug)
	{
		gEnv->pLog->Log("InputDebug: RetriggerKeyState");
	}

	m_retriggering = true;
	SInputEvent event;

	const size_t count = m_holdSymbols.size();
	for (int i=0; i < count; ++i)
	{
		EInputState oldState = m_holdSymbols[i]->state;
		m_holdSymbols[i]->state = eIS_Pressed;
		m_holdSymbols[i]->AssignTo(event, GetModifiers());
		PostInputEvent(event);
		m_holdSymbols[i]->state = oldState;
	}
	m_retriggering = false;
	if (g_pInputCVars->i_debug)
	{
		gEnv->pLog->Log("InputDebug: ~RetriggerKeyState");
	}
}

void CBaseInput::AddEventListener( IInputEventListener *pListener )
{
	// Add new listener to list if not added yet.
	if (std::find(m_listeners.begin(),m_listeners.end(),pListener) == m_listeners.end())
	{
		m_listeners.push_back( pListener );
	}
}

void CBaseInput::RemoveEventListener( IInputEventListener *pListener )
{
	// Remove listener if it is in list.
	TInputEventListeners::iterator it = std::find(m_listeners.begin(),m_listeners.end(),pListener);
	if (it != m_listeners.end())
	{
		m_listeners.erase( it );
	}
}

void CBaseInput::AddConsoleEventListener( IInputEventListener *pListener )
{
	if (std::find(m_consoleListeners.begin(),m_consoleListeners.end(),pListener) == m_consoleListeners.end())
	{
		m_consoleListeners.push_back( pListener );
	}
}

void CBaseInput::RemoveConsoleEventListener( IInputEventListener *pListener )
{
	TInputEventListeners::iterator it = std::find(m_consoleListeners.begin(),m_consoleListeners.end(),pListener);
	if (it != m_consoleListeners.end()) m_consoleListeners.erase( it );
}

void CBaseInput::SetExclusiveListener( IInputEventListener *pListener )
{
	m_pExclusiveListener = pListener;
}

IInputEventListener *CBaseInput::GetExclusiveListener()
{
	return m_pExclusiveListener;
}

void CBaseInput::EnableEventPosting ( bool bEnable )
{
	m_enableEventPosting = bEnable;
	if (g_pInputCVars->i_debug)
	{
		gEnv->pLog->Log("InputDebug: EnableEventPosting(%s)", bEnable?"true":"false");
	}
}

void CBaseInput::PostInputEvent( const SInputEvent &event )
{
	FUNCTION_PROFILER( GetISystem(),PROFILE_INPUT );
	//CCryMutex::CLock postInputLock(m_postInputEventMutex);

	if (!m_enableEventPosting) return;
	if (event.keyId == eKI_Unknown && event.state != eIS_UI) return;

	if (g_pInputCVars->i_debug)
	{
		// log out key press and release events
		if (event.state == eIS_Pressed || event.state == eIS_Released)
			gEnv->pLog->Log("InputDebug: '%s' - %s", event.keyName.c_str(), event.state == eIS_Pressed?"pressed":"released");
	}

	// console listeners get to process the event first
	for (TInputEventListeners::const_iterator it = m_consoleListeners.begin(); it != m_consoleListeners.end(); ++it)
	{
		bool ret = false;
		if (event.state != eIS_UI)
			ret = (*it)->OnInputEvent(event);
		else
			ret = (*it)->OnInputEventUI(event);

		if (ret)
			return;
	}

	// exclusive listener can filter out the event if he wants to and cause this call to return
	// before any of the regular listeners get to process it
	if (m_pExclusiveListener)
	{ 
		bool ret = false;
		if (event.state != eIS_UI)
			ret = m_pExclusiveListener->OnInputEvent(event);
		else
			ret = m_pExclusiveListener->OnInputEventUI(event);
		
		if (ret)
			return;
	}

	// Send this event to all listeners.
	for (TInputEventListeners::const_iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
	{
		assert(*it);
		if (*it == NULL)
			continue;

		bool ret = false;
		if (event.state != eIS_UI)
			ret = (*it)->OnInputEvent(event);
		else
			ret = (*it)->OnInputEventUI(event);

		if (ret) break;
	}

	// if it's a keypress, then add it to the "hold" list
	if (!m_retriggering)
	{
		if (event.pSymbol && event.pSymbol->state == eIS_Pressed)
		{
			event.pSymbol->state = eIS_Down;
			m_holdSymbols.push_back(event.pSymbol);
		}
		else if (event.pSymbol && event.pSymbol->state == eIS_Released && !m_holdSymbols.empty())
		{
			// remove hold key
			int slot = -1;
			int last = m_holdSymbols.size()-1;

			for (int i=last; i>=0; --i)
			{
				if (m_holdSymbols[i] == event.pSymbol)
				{
					slot = i;
					break;
				}
			}
			if (slot != -1)
			{
				// swap last and found symbol
				m_holdSymbols[slot] = m_holdSymbols[last];
				// pop last ... which is now the one we want to get rid of
				m_holdSymbols.pop_back();
			}
		}
	}
}

bool CBaseInput::AddInputDevice(IInputDevice* pDevice)
{
	if (pDevice)
	{
		if (pDevice->Init())
		{
			m_inputDevices.push_back(pDevice);
			return true;
		}
		delete pDevice;
	}
	return false;
}

void CBaseInput::PostHoldEvents()
{
	FUNCTION_PROFILER( GetISystem(),PROFILE_INPUT );
	SInputEvent event;
	
	const size_t count = m_holdSymbols.size();
	for (int i=0; i < count; ++i)
	{
		m_holdSymbols[i]->AssignTo(event, GetModifiers());
		PostInputEvent(event);
	}
}

void CBaseInput::ForceFeedbackEvent(const SFFOutputEvent &event)
{	
	if(g_pInputCVars->i_forcefeedback == 0)
		return;

	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		if((*i)->GetDeviceId() == event.deviceId)
		{
			IFFParams params;
			params.timeInSeconds = event.timeInSeconds;
			switch(event.eventId)
			{
			case eFF_Rumble_Basic:
				params.strengthA = event.amplifierS;
				params.strengthB = event.amplifierA;
				break;
			default:
				break;
			}

			if ((*i)->SetForceFeedback(params))
				break;
		}
	}
}

void	CBaseInput::EnableDevice( EDeviceId deviceId, bool enable)
{
	if(deviceId < m_inputDevices.size())
	{
		// try to flush the device ... perform a dry update
		EnableEventPosting(false);
		m_inputDevices[deviceId]->Update();
		EnableEventPosting(true);
		m_inputDevices[deviceId]->Enable(enable);
	}
}


bool CBaseInput::HasInputDeviceOfType( EInputDeviceType type )
{
	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		if ((*i)->IsOfDeviceType( type ))
			return true;
	}
	return false;
}

void CBaseInput::OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam )
{
	if (event == ESYSTEM_EVENT_LANGUAGE_CHANGE)
	{
		for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
		{
			(*i)->OnLanguageChange();
		}
	}
}
