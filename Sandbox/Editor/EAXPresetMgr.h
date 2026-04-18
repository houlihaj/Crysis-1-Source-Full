#pragma once

#include "IReverbManager.h"

struct ISound;

class CEAXPresetMgr : ISoundEventListener
{
protected:
	XmlNodeRef m_pRootNode;
	XmlNodeRef m_pParamTemplateNode;
	string		m_sPresetSelected;
	_smart_ptr<ISound> m_pSound;
protected:
	bool DumpTableRecursive(FILE *pFile, XmlNodeRef pNode, int nTabs=0);
	void InitNodes();
	
	//! Callback event.
	void OnSoundEvent( ESoundCallbackEvent event,ISound *pSound );

public:
	CEAXPresetMgr();
	virtual ~CEAXPresetMgr();
	XmlNodeRef GetRootNode() { return m_pRootNode; }
	bool AddPreset(CString sName);
	bool DelPreset(CString sName);
	bool PreviewPreset(CString sName);
	bool Save(CString sFilename=REVERB_PRESETS_FILENAME_LUA);
	bool Load(CString sFilename=REVERB_PRESETS_FILENAME_LUA);
	bool Reload(CString sFilename=REVERB_PRESETS_FILENAME_LUA);
	bool UpdateParameter(XmlNodeRef pNode);
	void MakeTagUnique(XmlNodeRef pParent, XmlNodeRef pNode);
};
