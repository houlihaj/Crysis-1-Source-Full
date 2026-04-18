#pragma once

#include "ISoundMoodManager.h"

class CSoundMoodMgr
{
protected:
	//bool DumpTableRecursive(FILE *pFile, XmlNodeRef pNode, int nTabs=0);
public:
	CSoundMoodMgr();
	virtual ~CSoundMoodMgr();
//	XmlNodeRef GetRootNode() { return m_pRootNode; }
	//bool AddEntry(CString sName, bool bIsGroup);
	//bool DelEntry(CString sName, bool bIsGroup);
	bool Save(CString sFilename=SOUNDMOOD_FILENAME);
	bool Load(CString sFilename=SOUNDMOOD_FILENAME);
	bool Reload(CString sFilename=SOUNDMOOD_FILENAME);
	void MakeTagUnique(XmlNodeRef pParent, XmlNodeRef pNode);
};
