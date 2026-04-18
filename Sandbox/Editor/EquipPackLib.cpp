#include "StdAfx.h"
#include "EquipPackLib.h"
#include "EquipPack.h"
#include "GameEngine.h"
#include "Util/FileUtil.h"

#include <IEditorGame.h>

#define EQUIPMENTPACKS_PATH "/Libs/EquipmentPacks/"

CEquipPackLib::CEquipPackLib()
{
}

CEquipPackLib::~CEquipPackLib()
{
	Reset();
}

CEquipPack* CEquipPackLib::CreateEquipPack(const CString& name)
{
	TEquipPackMap::iterator iter = m_equipmentPacks.find(name);
	if (iter != m_equipmentPacks.end())
		return 0;

	CEquipPack *pPack=new CEquipPack(this);
	if (!pPack)
		return 0;
	pPack->SetName(name);
	m_equipmentPacks.insert(TEquipPackMap::value_type(name, pPack));
	return pPack;
}

// currently we ignore the bDeleteFromDisk.
// will have to be manually removed via Source control 
bool CEquipPackLib::RemoveEquipPack(const CString& name, bool /* bDeleteFromDisk */ )
{
	TEquipPackMap::iterator iter = m_equipmentPacks.find(name);
	if (iter == m_equipmentPacks.end())
		return false;
	delete iter->second;
	m_equipmentPacks.erase(iter);

#if 0
	if (bDeleteFromDisk)
	{
		CString path = Path::GetGameFolder()+EQUIPMENTPACKS_PATH;
		path += name;
		path += ".xml";
		bool bOK = CFileUtil::OverwriteFile(path);
		if (bOK)
		{
			::DeleteFile(path);
		}
	}
#endif

	return true;
}

CEquipPack* CEquipPackLib::FindEquipPack(const CString& name)
{
	TEquipPackMap::iterator iter = m_equipmentPacks.find(name);
	if (iter == m_equipmentPacks.end())
		return 0;
	return iter->second;
}

bool CEquipPackLib::RenameEquipPack(const CString &name, const CString &newName)
{
	TEquipPackMap::iterator iter = m_equipmentPacks.find(name);
	if (iter == m_equipmentPacks.end())
		return false;

	CEquipPack *pPack = iter->second;
	pPack->SetName(newName);
	m_equipmentPacks.erase(iter);
	m_equipmentPacks.insert(TEquipPackMap::value_type(newName, pPack));
	return true;
}

bool CEquipPackLib::LoadLibs(bool bExportToGame)
{
	IEquipmentSystemInterface* pESI = GetIEditor()->GetGameEngine()->GetIEquipmentSystemInterface();
	if (bExportToGame && pESI)
	{
		pESI->DeleteAllEquipmentPacks();
	}

	CString path = Path::GetGameFolder()+EQUIPMENTPACKS_PATH;
	// std::vector<CString> files;
	// CFileEnum::ScanDirectory(path, "*.xml", files);

	std::vector<CFileUtil::FileDesc> files;
	CFileUtil::ScanDirectory(path,"*.xml", files, false);

	XmlParser parser;
	for (int iFile=0; iFile<files.size(); ++iFile)
	{
		CString filename = path;
		filename += files[iFile].filename;
		// CryLogAlways("Filename '%s'", filename);
		XmlNodeRef node = parser.parse(filename);
		if (node == 0)
		{
			Warning("CEquipPackLib:LoadLibs: Cannot load pack from file '%s'", filename.GetString());
			continue;
		}

		if (node->isTag("EquipPack") == false)
		{
			Warning("CEquipPackLib:LoadLibs: File '%s' is not an equipment pack. Skipped.", filename.GetString());
			continue;
		}

		CString packName;
		if (!node->getAttr("name", packName))
				packName="Unnamed";

		CEquipPack *pPack=CreateEquipPack(packName);
		if (pPack == 0)
			continue;
		pPack->Load(node);
		pPack->SetModified(false);

		if (bExportToGame && pESI)
		{
			pESI->LoadEquipmentPack(node);
		}
	}

	return true;
}

bool CEquipPackLib::SaveLibs(bool bExportToGame)
{
	IEquipmentSystemInterface* pESI = GetIEditor()->GetGameEngine()->GetIEquipmentSystemInterface();
	if (bExportToGame && pESI)
	{
		pESI->DeleteAllEquipmentPacks();
	}

	CString libsPath = Path::GetGameFolder()+EQUIPMENTPACKS_PATH;
	if (!libsPath.IsEmpty())
		CFileUtil::CreateDirectory( libsPath );

	for (TEquipPackMap::iterator iter = m_equipmentPacks.begin(); iter != m_equipmentPacks.end(); ++iter)
	{
		CEquipPack* pPack = iter->second;
		XmlNodeRef packNode = CreateXmlNode("EquipPack");
		pPack->Save(packNode);
		if (pPack->IsModified())
		{
			CString path (libsPath);
			path += iter->second->GetName();
			path += ".xml";
			bool bOK = SaveXmlNode(packNode, path.GetString());
			if (bOK)
				pPack->SetModified(false);
		}
		if (bExportToGame && pESI)
		{
			pESI->LoadEquipmentPack(packNode);
		}
	}
	return true;
}

void CEquipPackLib::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bResetWhenLoad)
{
	if (!xmlNode)
		return;
	if (bLoading)
	{
		if (bResetWhenLoad)
			Reset();
		XmlNodeRef equipXMLNode=xmlNode->findChild("EquipPacks");
		if (equipXMLNode)
		{
			for (int i=0; i<equipXMLNode->getChildCount(); ++i)
			{
				XmlNodeRef node=equipXMLNode->getChild(i);
				if (node->isTag("EquipPack") == false)
					continue;
				CString packName;
				if (!node->getAttr("name", packName))
				{
					CLogFile::FormatLine("Warning: Unnamed EquipPack found !");
					packName="Unnamed";
					node->setAttr("name", packName);
				}
				CEquipPack *pCurPack=CreateEquipPack(packName);
				if (!pCurPack)
				{
					CLogFile::FormatLine("Warning: Unable to create EquipPack %s !", packName.GetString());
					continue;
				}
				pCurPack->Load(node);
				pCurPack->SetModified(false);
				GetIEditor()->GetGameEngine()->GetIEquipmentSystemInterface()->LoadEquipmentPack(node);
			}			
		}
	}
	else
	{
		XmlNodeRef equipXMLNode = xmlNode->newChild("EquipPacks");
		for (TEquipPackMap::iterator iter = m_equipmentPacks.begin(); iter != m_equipmentPacks.end(); ++iter)
		{
			XmlNodeRef packNode = equipXMLNode->newChild("EquipPack");
			iter->second->Save(packNode);
		}
	}
}

void CEquipPackLib::Reset()
{
	for (TEquipPackMap::iterator iter = m_equipmentPacks.begin(); iter != m_equipmentPacks.end(); ++iter)
	{
		delete iter->second;
	}
	m_equipmentPacks.clear();
}

void CEquipPackLib::ExportToGame()
{
	IEquipmentSystemInterface* pESI = GetIEditor()->GetGameEngine()->GetIEquipmentSystemInterface();
	if (pESI)
	{
		pESI->DeleteAllEquipmentPacks();
		for (TEquipPackMap::iterator iter = m_equipmentPacks.begin(); iter != m_equipmentPacks.end(); ++iter)
		{
			XmlNodeRef packNode = CreateXmlNode("EquipPack");
			iter->second->Save(packNode);
			pESI->LoadEquipmentPack(packNode);
		}
	}
}