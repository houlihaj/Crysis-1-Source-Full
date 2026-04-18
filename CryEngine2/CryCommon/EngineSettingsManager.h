////////////////////////////////////////////////////////////////////////////
//
//  CryENGINE2 Source File.
//  Copyright (C), Crytek Studios, 2008.
// -------------------------------------------------------------------------
//  File name:   EnigneSettingsManager.h
//  Version:     v1.00
//  Created:     12/07/2004 by Benjamin Peters
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __ENGINESETTINGSMANAGER_H__
#define __ENGINESETTINGSMANAGER_H__
#pragma once

#if !defined(PS3)

#include "CryModuleDefs.h"												// ECryModule

#ifdef NOT_USE_CRY_STRING
	#include <string>																// STL string
	typedef std::string string;
#else
	#include <platform.h>														// string
#endif


#include <map>


#if defined(WIN32) || defined(WIN64)

#include "IEngineSettingsManager.h"


//////////////////////////////////////////////////////////////////////////
// Manages storage and loading of all information for tools and CryENGINE, by either registry or an INI file.
// Information can be read and set by key-to-value functions.
// Specific information can be set by a dialog application called by this class.
// If the engine root path is not found, a fall-back dialog is opened.
class CEngineSettingsManager : public IEngineSettingsManager
{
public:
	// prepares CEngineSettingsManager to get requested information either from registry or an INI file,
	// if existent as a file with name an directory equal to the module, or from registry.
	CEngineSettingsManager(const char* moduleName=NULL, const char* iniFileName=NULL);

	void RestoreDefaults();

	// stores/loads user specific information for modules to/from registry or INI file
	template <typename T> bool GetModuleSpecificEntry(const string& key, T& value);
	template <typename T> bool SetModuleSpecificEntry(const string& key, const T& value);

	virtual bool HasKey(const string& key, bool bForcePull);
	virtual void GetValueByRef(const string& key, string& value, bool bForcePull);
	virtual void GetValueByRef(const string& key, int& value, bool bForcePull);
	template <typename T> T GetValue(const string& key);
	virtual void SetKey(const string& key, const char* value);
	virtual void SetKey(const string& key, const string& value);
	virtual void SetKey(const string& key, bool value);

	virtual bool StoreData();
	void CallSettingsDialog(void* hParent);
	void CallRootPathDialog(void* hParent);

	virtual void SetRootPath( const string& szRootPath );

	// returns path determined either by registry or by INI file
	virtual string GetRootPath();
	void	SetParentDialog(unsigned long window);

private:
	typedef std::map<string,string> TKeyValueMap;

	string				m_sModuleName;					// name to store key-value pairs of modules in (registry) or to identify INI file
	string				m_sModuleFileName;				// used in case of data being loaded from INI file
	bool					m_bGetDataFromRegistry;
	TKeyValueMap	m_keyToValueMap;

	void*					m_hBtnBrowse;
	unsigned long				m_hWndParent;

private:
	string Trim(string& str);

	void LoadEngineSettingsFromRegistry();
	bool StoreEngineSettingsToRegistry();

	// parses a file and stores all flags in a private key-value-map
	bool LoadValuesFromConfigFile(const char* szFileName);

	bool SetRegValue(void* key, const string& valueName, const string& value);
	bool SetRegValue(void* key, const string& valueName, bool value);
	bool SetRegValue(void* key, const string& valueName, int value);
	bool GetRegValue(void* key, const string& valueName, string& value);
	bool GetRegValue(void* key, const string& valueName, bool& value);
	bool GetRegValue(void* key, const string& valueName, int& value);

	class RegKey
	{
	public:
		RegKey(const string& key, bool writeable);
		~RegKey();
		void* pKey;
	};

public:
	long HandleProc(void* pWnd, long uMsg, long wParam, long lParam);
};

template <typename T> bool CEngineSettingsManager::GetModuleSpecificEntry(const string& key, T& value)
{
	if (!m_bGetDataFromRegistry)
	{
		if (!HasKey(key, false))
			return false;
		value = GetValue<T>(key);
		return true;
	}

	RegKey superKey(string("Software\\Crytek\\Settings\\")+m_sModuleName, false);
	bool result = false;
	if (superKey.pKey)
	{
		T regValue;
		if (result = GetRegValue(superKey.pKey, key, regValue))
		{
			value = regValue;
			result = true;
		}
	}

	return result;
}

template <typename T> bool CEngineSettingsManager::SetModuleSpecificEntry(const string& key, const T& value)
{
	SetKey(key, string(value));
	if (!m_bGetDataFromRegistry)
		return StoreData();

	RegKey superKey(string("Software\\Crytek\\Settings\\")+m_sModuleName, true);
	if (superKey.pKey)
		return SetRegValue(superKey.pKey, key, value);
	return false;
}

template <typename T> T CEngineSettingsManager::GetValue(const string& key)
{
	T value;
	GetValueByRef(key, value, false);
	return value;
}

#endif // defined(WIN32) || defined(WIN64)
#endif//PS3

#endif // __RESOURCECOMPILERHELPER_H__
