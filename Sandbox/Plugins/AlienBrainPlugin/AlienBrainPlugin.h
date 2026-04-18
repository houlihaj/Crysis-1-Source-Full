#ifndef __AlienBrainPlugin_h__
#define __AlienBrainPlugin_h__

#pragma once

class CAlienBrainPlugin : public IPlugin
{
public:
	void Release();
	void ShowAbout();
	const char * GetPluginGUID();
	DWORD GetPluginVersion();
	const char * GetPluginName();
	bool CanExitNow();
	void Serialize(FILE *hFile, bool bIsStoring);
	void ResetContent();
	bool CreateUIElements();
	bool ExportDataToGame(const char * pszGamePath);
};

#endif //__AlienBrainPlugin_h__
