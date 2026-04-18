#include "StdAfx.h"
#include <ctype.h>
#include <IScriptSystem.h>
#include <ISound.h>
#include "ISoundMoodManager.h"
#include <IEntitySystem.h>
#include "SoundMoodMgr.h"

#define ADDNODE(_type, _name, _min, _max, _value) \
	pPropNode=CreateXmlNode(#_name); \
	pPropNode->setAttr("type", #_type); \
	pPropNode->setAttr("min", #_min); \
	pPropNode->setAttr("max", #_max); \
	pPropNode->setAttr("value", #_value); \
	m_pParamTemplateNode->addChild(pPropNode);

CSoundMoodMgr::CSoundMoodMgr()
{
}

CSoundMoodMgr::~CSoundMoodMgr()
{
}
//
//bool CSoundMoodMgr::AddEntry(CString sName, bool bIsGroup)
//{
//	IGroupManager *pGroupManager = gEnv->pSoundSystem->GetGroupManagerPtr();
//
//	if (!pGroupManager)
//		return false;
//
//	if (sName.GetLength()<=0)
//		return false;
//
//	// check if name is valid
//	for (int i=0;i<sName.GetLength();i++)
//	{
//		char c=sName[i];
//		if (!i)
//		{
//			if (!isalpha(c))
//				return false;
//		}else
//		{
//			if (!isalnum(c))
//				return false;
//		}
//	}
//	string sElementName(sName.GetBuffer());
//	SSGHandle nHandle;
//	
//	if (bIsGroup)
//	{
//		if (pGroupManager->GetGroupPtr(sGroupName))
//			return false;
//
//		nHandle = pGroupManager->AddGroup(sGroupName);
//	}
//	else
//	{
//		pGroupManager->GetElementHandle(sE)
//	}
//
//
//	return (nHandle.IsGroupIDValid());
//}
//
//bool CSoundMoodMgr::DelEntry(CString sName, bool bIsGroup)
//{
//	bool bResult = false;
//	IGroupManager *pGroupManager = gEnv->pSoundSystem->GetGroupManagerPtr();
//
//	if (!pGroupManager)
//		return bResult;
//	
//	string sGroupName(sName.GetBuffer());
//	ISGElement *pGroup = pGroupManager->GetGroupPtr(sGroupName);
//	if (pGroup)
//	{
//		bResult = pGroupManager->RemoveGroup(pGroup->GetHandle());
//	}
//	return bResult;
//}

bool CSoundMoodMgr::Reload(CString sFilename)
{
	if (!Save() || !Load())
	{
		AfxMessageBox("An error occured while reloading sound-presets. Is Database read-only ?", MB_ICONEXCLAMATION | MB_OK);
		return false;
	}
	return true;
}

bool CSoundMoodMgr::Save(CString sFilename)
{
	bool bResult = false;

	ISoundMoodManager *pMoodManager = NULL;
	if (gEnv->pSoundSystem)
		pMoodManager = gEnv->pSoundSystem->GetIMoodManager();

	if (!pMoodManager)
		return bResult;

	string sFullFileName = "/" + sFilename;
	sFullFileName = PathUtil::GetGameFolder() + sFullFileName;

	XmlNodeRef root = GetISystem()->CreateXmlNode("Export");
	
	pMoodManager->Serialize(root, false);
	
	bResult = SaveXmlNode( root,sFullFileName.c_str());

	return bResult;
}

bool CSoundMoodMgr::Load(CString sFilename)
{

	bool bResult = false;

	ISoundMoodManager *pMoodManager = NULL;
	if (gEnv->pSoundSystem)
		pMoodManager = gEnv->pSoundSystem->GetIMoodManager();

	if (!pMoodManager)
		return bResult;

	string sFullFileName = "/" + sFilename;
	sFullFileName = PathUtil::GetGameFolder() + sFullFileName;
	
	XmlNodeRef root = GetISystem()->LoadXmlFile( sFullFileName.c_str());
	pMoodManager->Serialize(root, true);

	return true;
}

