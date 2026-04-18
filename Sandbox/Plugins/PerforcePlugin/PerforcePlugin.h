#ifndef __PerforcePlugin_h__
#define __PerforcePlugin_h__

#pragma once

class CPerforcePlugin : public IPlugin
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

#endif //__PerforcePlugin_h__
