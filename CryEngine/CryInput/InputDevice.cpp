#include "StdAfx.h"
#include "InputDevice.h"

CInputDevice::CInputDevice(IInput& input, const char* deviceName)
: m_input(input)
, m_deviceName(deviceName)
, m_deviceId(eDI_Unknown)
, m_enabled(true)
{
}

CInputDevice::~CInputDevice()
{
	while(m_idToInfo.size())
	{
		TIdToSymbolMap::iterator iter = m_idToInfo.begin();
		SInputSymbol *pSymbol = (*iter).second;
		m_idToInfo.erase(iter);
		SAFE_DELETE(pSymbol);
	}
}

void CInputDevice::Update()
{
}

bool CInputDevice::InputState(const TKeyName& keyName, EInputState state)
{
	SInputSymbol* pSymbol = NameToSymbol(keyName);
	if (pSymbol && pSymbol->state == state)
		return true;

	return false;
}

void CInputDevice::ClearKeyState()
{
	for (TIdToSymbolMap::iterator i = m_idToInfo.begin(); i != m_idToInfo.end(); ++i)
	{
		SInputSymbol* pSymbol = (*i).second;
		if (pSymbol && pSymbol->value > 0.0)
		{
			SInputEvent event;
			event.deviceId = m_deviceId;
			event.keyName = pSymbol->name;
			event.keyId = pSymbol->keyId;
			event.state = eIS_Released;
			event.value = 0.0f;
			event.pSymbol = pSymbol;
			pSymbol->value = 0.0f;
			pSymbol->state = eIS_Released;
			m_input.PostInputEvent(event);
		}
	}
}

const char* CInputDevice::GetKeyName(const SInputEvent& event, bool bGUI)
{
	return event.keyName;
}

const wchar_t* CInputDevice::GetOSKeyName(const SInputEvent& event)
{
	return L"";
}


/*
const TKeyName& CInputDevice::IdToName(TKeyId id) const
{
	static TKeyName sUnknown("<unknown>");

	TIdToSymbolMap::const_iterator i = m_idToInfo.find(id);
	if (i != m_idToInfo.end())
		return (*i).second->name;
	else
		return sUnknown;
}
*/

SInputSymbol* CInputDevice::IdToSymbol(EKeyId id) const
{
	TIdToSymbolMap::const_iterator i = m_idToInfo.find(id);
	if (i != m_idToInfo.end())
		return (*i).second;
	else
		return 0;
}

SInputSymbol* CInputDevice::DevSpecIdToSymbol(uint32 devSpecId) const
{
	TDevSpecIdToSymbolMap::const_iterator i = m_devSpecIdToSymbol.find(devSpecId);
	if (i != m_devSpecIdToSymbol.end())
		return (*i).second;
	else
		return 0;
}


uint32 CInputDevice::NameToId(const TKeyName& name) const
{
	TNameToIdMap::const_iterator i = m_nameToId.find(name);
	if (i != m_nameToId.end())
		return (*i).second;
	else
		return 0xffffffff;
}

SInputSymbol* CInputDevice::NameToSymbol(const TKeyName& name) const
{
	TNameToSymbolMap::const_iterator i = m_nameToInfo.find(name);
	if (i != m_nameToInfo.end())
		return (*i).second;
	else
		return 0;
}

SInputSymbol* CInputDevice::MapSymbol(uint32 deviceSpecificId, EKeyId keyId, const TKeyName& name, SInputSymbol::EType type, uint32 user)
{
	SInputSymbol* pSymbol = new SInputSymbol(deviceSpecificId, keyId, name, type);
	pSymbol->user = user;
	pSymbol->deviceId = m_deviceId;
	m_idToInfo[keyId] = pSymbol;
	m_devSpecIdToSymbol[deviceSpecificId] = pSymbol;
	m_nameToId[name] = deviceSpecificId;
	m_nameToInfo[name] = pSymbol;

	return pSymbol;
}

//////////////////////////////////////////////////////////////////////////
SInputSymbol* CInputDevice::LookupSymbol(EKeyId id) const
{
	return IdToSymbol(id);
}

void CInputDevice::Enable(bool enable)
{
	m_enabled = enable;
}