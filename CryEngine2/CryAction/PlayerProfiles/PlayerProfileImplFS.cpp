#include "StdAfx.h"
#include <IXml.h>
#include <StringUtils.h>
#include "PlayerProfileImplFS.h"
#include "PlayerProfileImplRSFHelper.h"
#include "PlayerProfile.h"
#include "Serialization/XmlSaveGame.h"
#include "Serialization/XmlLoadGame.h"
#include "BMPHelper.h"
#include "CryAction.h"
#include <StringUtils.h>

#ifndef XENON

class CCommonSaveGameHelper
{
public:
	CCommonSaveGameHelper(ICommonProfileImpl* pImpl) : m_pImpl(pImpl) {}
	bool GetSaveGames(CPlayerProfileManager::SUserEntry* pEntry, CPlayerProfileManager::TSaveGameInfoVec& outVec, const char* altProfileName);
	ISaveGame* CreateSaveGame(CPlayerProfileManager::SUserEntry* pEntry);
	ILoadGame* CreateLoadGame(CPlayerProfileManager::SUserEntry* pEntry);
	bool DeleteSaveGame(CPlayerProfileManager::SUserEntry* pEntry, const char* name);
	bool GetSaveGameThumbnail(CPlayerProfileManager::SUserEntry* pEntry, const char* saveGameName, CPlayerProfileManager::SThumbnail& thumbnail);
	bool MoveSaveGames(CPlayerProfileManager::SUserEntry* pEntry, const char* oldProfileName, const char* newProfileName);
	ILevelRotationFile* GetLevelRotationFile(CPlayerProfileManager::SUserEntry* pEntry, const char* name);

protected:
	bool FetchMetaData(XmlNodeRef& root, CPlayerProfileManager::SSaveGameMetaData& metaData);
	ICommonProfileImpl* m_pImpl;
};


namespace
{
	const char* PROFILE_ROOT_TAG = "Profile";
	const char* PROFILE_NAME_TAG = "Name";
	const char* PROFILE_VERSION_TAG = "Version";

	// a simple serializer. manages sections (XmlNodes)
	// is used during load and save
	class CSerializerXML : public CPlayerProfileManager::IProfileXMLSerializer
	{
	public:
		CSerializerXML(XmlNodeRef& root, bool bLoading) : m_bLoading(bLoading), m_root(root)
		{
			assert (m_root != 0);
		}

		// CPlayerProfileManager::ISerializer
		bool IsLoading() { return m_bLoading; }

		XmlNodeRef AddSection(const char* name)
		{
			XmlNodeRef child = m_root->findChild(name);
			if (child == 0)
			{
				child = m_root->newChild(name);
			}
			return child;
		}

		XmlNodeRef GetSection(const char* name)
		{
			XmlNodeRef child = m_root->findChild(name);
			return child;
		}
		// ~CPlayerProfileManager::ISerializer

		// --- used by impl ---

		void AddSection(XmlNodeRef& node)
		{
			if (node != 0)
				m_root->addChild(node);
		}

		int GetSectionCount()
		{
			return m_root->getChildCount();
		}

		XmlNodeRef GetSectionByIndex(int index)
		{
			assert (index >=0 && index < m_root->getChildCount());
			return m_root->getChild(index);
		}

	protected:
		bool       m_bLoading;
		XmlNodeRef m_root;
	};

	// some helpers
	bool SaveXMLFile(const string& filename, const XmlNodeRef& rootNode)
	{
		if (rootNode == 0)
			return true;

		bool ok = false;
		ok = rootNode->saveToFile(filename.c_str(), 1024*1024);

		if (!ok)
			GameWarning("[PlayerProfiles] PlayerProfileImplFS: Cannot save XML file '%s'", filename.c_str());
		return ok;
	}

	XmlNodeRef LoadXMLFile(const string& filename)
	{
		XmlNodeRef rootNode = GetISystem()->LoadXmlFile(filename.c_str());
		if (rootNode == 0)
		{
//			GameWarning("[PlayerProfiles] PlayerProfileImplFS: Cannot load XML file '%s'", filename.c_str());
		}
		return rootNode;
	}

	bool IsValidFilename(const char* filename)
	{
		const char* invalidChars = "\\/:*?\"<>~|";
		return strpbrk(filename, invalidChars) == 0;
	}
}


//------------------------------------------------------------------------
CPlayerProfileImplFS::CPlayerProfileImplFS() : m_pMgr(0)
{
}

//------------------------------------------------------------------------
CPlayerProfileImplFS::~CPlayerProfileImplFS()
{
}

//------------------------------------------------------------------------
bool CPlayerProfileImplFS::Initialize(CPlayerProfileManager* pMgr)
{
	m_pMgr = pMgr;
	return true;
}

//------------------------------------------------------------------------
void CPlayerProfileImplFS::InternalMakeFSPath(SUserEntry* pEntry, const char* profileName, string& outPath)
{
	if (pEntry)
	{
		outPath = ("%USER%/ProfilesSingle/");
	}
	else
	{
		// default profile always in game folder
		outPath = PathUtil::GetGameFolder();
		outPath.append("/Libs/Config/ProfilesSingle/");
	}
	if (profileName && *profileName)
	{
		outPath.append(profileName);
		outPath.append("/");
	}
}

//------------------------------------------------------------------------
void CPlayerProfileImplFS::InternalMakeFSSaveGamePath(SUserEntry* pEntry, const char* profileName, string& outPath, bool bNeedFolder)
{
	assert (pEntry != 0);
	assert (profileName != 0);

	if (m_pMgr->IsSaveGameFolderShared())
	{
		outPath = m_pMgr->GetSharedSaveGameFolder();
		outPath.append("/");
		if (!bNeedFolder)
		{
			outPath.append(profileName);
			outPath.append("_");
		}
	}
	else
	{
		outPath = "%USER%/ProfilesSingle/";
		outPath.append(profileName);
		outPath.append("/SaveGames/");
	}
}

//------------------------------------------------------------------------
bool CPlayerProfileImplFS::SaveProfile(SUserEntry* pEntry, CPlayerProfile* pProfile, const char* name)
{
	// save the profile into a specific location

	// check, if it's a valid filename
	if (IsValidFilename(name) == false)
		return false;

	// XML for now
	string filename;
	InternalMakeFSPath(pEntry, name, filename);

	// currently all (attributes, action maps, ...) in one file
	XmlNodeRef rootNode = GetISystem()->CreateXmlNode(PROFILE_ROOT_TAG);
	rootNode->setAttr(PROFILE_NAME_TAG, name);
	CSerializerXML serializer(rootNode, false);

	bool ok = pProfile->SerializeXML(&serializer);
	if (ok)
	{
		ok = SaveXMLFile(filename, rootNode);
	}
	return ok;
}

//------------------------------------------------------------------------
bool CPlayerProfileImplFS::LoadProfile(SUserEntry* pEntry, CPlayerProfile* pProfile, const char* name)
{
	// load the profile from a specific location

	// XML for now
	// if pEntry==0 -> use factory default
	string filename;
	InternalMakeFSPath(pEntry, name, filename);

	// currently all (attributes, action maps, ...) in one file
	XmlNodeRef rootNode = LoadXMLFile(filename.c_str());
	if (rootNode == 0)
		return false;

	if (stricmp(rootNode->getTag(), PROFILE_ROOT_TAG) != 0)
	{
		GameWarning("CPlayerProfileImplFS::LoadProfile: Profile '%s' of user '%s' seems to exist, but is invalid (File '%s). Skipped", name, pEntry->userId.c_str(), filename.c_str());
		return false;
	}
	CSerializerXML serializer(rootNode, true);
	bool ok = pProfile->SerializeXML(&serializer);
	return ok;
}

//------------------------------------------------------------------------
bool CPlayerProfileImplFS::LoginUser(SUserEntry* pEntry)
{
	// lookup stored profiles of the user (pEntry->userId) and fill in the pEntry->profileDesc
	// vector 
	pEntry->profileDesc.clear();

	// scan directory for profiles

	string path;
	InternalMakeFSPath(pEntry, "", path);  // no profile name -> only path

	ICryPak * pCryPak = gEnv->pCryPak;
	_finddata_t fd;

	path.TrimRight("/\\");
	string search = path + "/*.xml";
	intptr_t handle = pCryPak->FindFirst( search.c_str(), &fd );
	if (handle != -1)
	{
		do
		{
			// fd.name contains the profile name
			string filename = path;
			filename += "/" ;
			filename += fd.name;
			XmlNodeRef rootNode = LoadXMLFile(filename.c_str());

			// see if the root tag is o.k.
			if (rootNode && stricmp(rootNode->getTag(), PROFILE_ROOT_TAG) == 0)
			{
				string profileName = fd.name;
				PathUtil::RemoveExtension(profileName);
				if (rootNode->haveAttr(PROFILE_NAME_TAG))
				{
					const char* profileHumanName = rootNode->getAttr(PROFILE_NAME_TAG);
					if (profileHumanName && stricmp(profileHumanName, profileName) == 0)
					{
						profileName = profileHumanName;
					}
				}
				pEntry->profileDesc.push_back(SLocalProfileInfo(profileName));
			}
			else
			{
				GameWarning("CPlayerProfileImplFS::LoginUser: Profile '%s' of User '%s' seems to exist, but is invalid (File '%s). Skipped", fd.name, pEntry->userId.c_str(), filename.c_str());
			}
		} while ( pCryPak->FindNext( handle, &fd ) >= 0 );

		pCryPak->FindClose( handle );
	}

	return true;
}

//------------------------------------------------------------------------
bool CPlayerProfileImplFS::DeleteProfile(SUserEntry* pEntry, const char* name)
{
	string path;
	InternalMakeFSPath(pEntry, name, path);  // no profile name -> only path
	bool bOK = gEnv->pCryPak->RemoveFile(path.c_str());
	// in case the savegame folder is shared, we have to delete the savegames from the shared folder
	if (bOK && m_pMgr->IsSaveGameFolderShared())
	{
		CPlayerProfileManager::TSaveGameInfoVec saveGameInfoVec;
		if (GetSaveGames(pEntry, saveGameInfoVec, name)) // provide alternate profileName, because pEntry->pCurrentProfile points to the active profile
		{
			CPlayerProfileManager::TSaveGameInfoVec::iterator iter = saveGameInfoVec.begin();
			CPlayerProfileManager::TSaveGameInfoVec::iterator iterEnd = saveGameInfoVec.end();
			while (iter != iterEnd)
			{
				DeleteSaveGame(pEntry, iter->name);
				++iter;
			}
		}
	}
	return bOK;
}

//------------------------------------------------------------------------
bool CPlayerProfileImplFS::RenameProfile(SUserEntry* pEntry, const char* newName)
{
	// TODO: only rename in the filesystem. for now save new and delete old
	const char* oldName = pEntry->pCurrentProfile->GetName();
	bool ok = SaveProfile(pEntry, pEntry->pCurrentProfile, newName);
	if (!ok)
		return false;

	// move the save games
	if (CPlayerProfileManager::sUseRichSaveGames)
	{
		CRichSaveGameHelper sgHelper(this);
		sgHelper.MoveSaveGames(pEntry, oldName, newName);
	}
	else
	{
		CCommonSaveGameHelper sgHelper(this);
		sgHelper.MoveSaveGames(pEntry, oldName, newName);
	}

	DeleteProfile(pEntry, oldName);
	return true;
}

//------------------------------------------------------------------------
bool CPlayerProfileImplFS::LogoutUser(SUserEntry* pEntry)
{
	return true;
}

//------------------------------------------------------------------------
bool CPlayerProfileImplFS::GetSaveGames(SUserEntry* pEntry, CPlayerProfileManager::TSaveGameInfoVec& outVec, const char* altProfileName)
{
	if (CPlayerProfileManager::sUseRichSaveGames)
	{
		CRichSaveGameHelper sgHelper(this);
		return sgHelper.GetSaveGames(pEntry, outVec, altProfileName);
	}
	else
	{
		CCommonSaveGameHelper sgHelper(this);
		return sgHelper.GetSaveGames(pEntry, outVec, altProfileName);
	}
}

//------------------------------------------------------------------------
ISaveGame* CPlayerProfileImplFS::CreateSaveGame(SUserEntry* pEntry)
{
	if (CPlayerProfileManager::sUseRichSaveGames)
	{
		CRichSaveGameHelper sgHelper(this);
		return sgHelper.CreateSaveGame(pEntry);
	}
	else
	{
		CCommonSaveGameHelper sgHelper(this);
		return sgHelper.CreateSaveGame(pEntry);
	}
}


//------------------------------------------------------------------------
ILoadGame* CPlayerProfileImplFS::CreateLoadGame(SUserEntry* pEntry)
{
	if (CPlayerProfileManager::sUseRichSaveGames)
	{
		CRichSaveGameHelper sgHelper(this);
		return sgHelper.CreateLoadGame(pEntry);
	}
	else
	{
		CCommonSaveGameHelper sgHelper(this);
		return sgHelper.CreateLoadGame(pEntry);
	}
}

//------------------------------------------------------------------------
bool CPlayerProfileImplFS::DeleteSaveGame(SUserEntry* pEntry, const char* name)
{
	if (CPlayerProfileManager::sUseRichSaveGames)
	{
		CRichSaveGameHelper sgHelper(this);
		return sgHelper.DeleteSaveGame(pEntry, name);
	}
	else
	{
		CCommonSaveGameHelper sgHelper(this);
		return sgHelper.DeleteSaveGame(pEntry, name);
	}
}

//------------------------------------------------------------------------
bool CPlayerProfileImplFS::GetSaveGameThumbnail(SUserEntry* pEntry, const char* saveGameName, SThumbnail& thumbnail)
{
	if (CPlayerProfileManager::sUseRichSaveGames)
	{
		CRichSaveGameHelper sgHelper(this);
		return sgHelper.GetSaveGameThumbnail(pEntry, saveGameName, thumbnail);
	}
	else
	{
		CCommonSaveGameHelper sgHelper(this);
		return sgHelper.GetSaveGameThumbnail(pEntry, saveGameName, thumbnail);
	}
}



// Profile Implementation which stores most of the stuff ActionMaps/Attributes in separate files

//------------------------------------------------------------------------
CPlayerProfileImplFSDir::CPlayerProfileImplFSDir() : m_pMgr(0)
{
}

//------------------------------------------------------------------------
CPlayerProfileImplFSDir::~CPlayerProfileImplFSDir()
{
}

//------------------------------------------------------------------------
bool CPlayerProfileImplFSDir::Initialize(CPlayerProfileManager* pMgr)
{
	m_pMgr = pMgr;
	return true;
}

//------------------------------------------------------------------------
void CPlayerProfileImplFSDir::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
}

//------------------------------------------------------------------------
void CPlayerProfileImplFSDir::InternalMakeFSPath(SUserEntry* pEntry, const char* profileName, string& outPath)
{
	if (pEntry)
	{
		outPath = ("%USER%/Profiles/");
	}
	else
	{
		// default profile always in game folder
		outPath = PathUtil::GetGameFolder();
		outPath.append("/Libs/Config/Profiles/");
	}
	if (profileName && *profileName)
	{
		outPath.append(profileName);
		outPath.append("/");
	}
}

//------------------------------------------------------------------------
void CPlayerProfileImplFSDir::InternalMakeFSSaveGamePath(SUserEntry* pEntry, const char* profileName, string& outPath, bool bNeedFolder)
{
	assert (pEntry != 0);
	assert (profileName != 0);

	if (m_pMgr->IsSaveGameFolderShared())
	{
		outPath = m_pMgr->GetSharedSaveGameFolder();
		outPath.append("/");
		if (!bNeedFolder)
		{
			outPath.append(profileName);
			outPath.append("_");
		}
	}
	else
	{
		outPath = "%USER%/Profiles/";
		outPath.append(profileName);
		outPath.append("/SaveGames/");
	}
}


//------------------------------------------------------------------------
bool CPlayerProfileImplFSDir::SaveProfile(SUserEntry* pEntry, CPlayerProfile* pProfile, const char* name)
{
	// save the profile into a specific location

	// check if it's a valid filename
	if (IsValidFilename(name) == false)
		return false;

	string path;
	InternalMakeFSPath(pEntry, name, path);

	XmlNodeRef rootNode = GetISystem()->CreateXmlNode(PROFILE_ROOT_TAG);
	rootNode->setAttr(PROFILE_NAME_TAG, name);

	// save profile.xml 
	bool ok = SaveXMLFile(path+"profile.xml", rootNode);
	if (ok)
	{
		CSerializerXML serializer(rootNode, false);
		ok = pProfile->SerializeXML(&serializer);

		// save action map and attributes into separate files
		if (ok)
		{
			ok &= SaveXMLFile(path+"attributes.xml", serializer.GetSection(CPlayerProfile::ATTRIBUTES_TAG));
			ok &= SaveXMLFile(path+"actionmaps.xml", serializer.GetSection(CPlayerProfile::ACTIONMAPS_TAG));
		}
	}
	return ok;
}

//------------------------------------------------------------------------
bool CPlayerProfileImplFSDir::LoadProfile(SUserEntry* pEntry, CPlayerProfile* pProfile, const char* name)
{
	// load the profile from a specific location

	// XML for now
	string path;
	InternalMakeFSPath(pEntry, name, path);

	XmlNodeRef rootNode = GetISystem()->CreateXmlNode(PROFILE_ROOT_TAG);
	CSerializerXML serializer(rootNode, true);
	XmlNodeRef attrNode = LoadXMLFile(path+"attributes.xml");
	XmlNodeRef actionNode = LoadXMLFile(path+"actionmaps.xml");
	serializer.AddSection(attrNode);
	serializer.AddSection(actionNode);

	bool ok = pProfile->SerializeXML(&serializer);
	return ok;
}

//------------------------------------------------------------------------
bool CPlayerProfileImplFSDir::LoginUser(SUserEntry* pEntry)
{
	// lookup stored profiles of the user (pEntry->userId) and fill in the pEntry->profileDesc
	// vector 
	pEntry->profileDesc.clear();

	// scan directory for profiles

	string path;
	InternalMakeFSPath(pEntry, "", path);  // no profile name -> only path

	ICryPak * pCryPak = gEnv->pCryPak;
	_finddata_t fd;

	path.TrimRight("/\\");
	string search = path + "/*.*";
	intptr_t handle = pCryPak->FindFirst( search.c_str(), &fd );
	if (handle != -1)
	{
		do
		{
			if (strcmp(fd.name, ".") == 0 || strcmp(fd.name, "..") == 0)
				continue;

			if (fd.attrib & _A_SUBDIR)
			{
				// profile name = folder name
				// but make sure there is a profile.xml in it
				string filename = path + "/" + fd.name;
				filename += "/" ;
				filename += "profile.xml";
				XmlNodeRef rootNode = LoadXMLFile(filename.c_str());

				// see if the root tag is o.k.
				if (rootNode && stricmp(rootNode->getTag(), PROFILE_ROOT_TAG) == 0)
				{
					string profileName = fd.name;
					if (rootNode->haveAttr(PROFILE_NAME_TAG))
					{
						const char* profileHumanName = rootNode->getAttr(PROFILE_NAME_TAG);
						if (profileHumanName && stricmp(profileHumanName, profileName) == 0)
						{
							profileName = profileHumanName;
						}
					}
					pEntry->profileDesc.push_back(SLocalProfileInfo(profileName));
				}
				else
				{
					//GameWarning("CPlayerProfileImplFSDir::LoginUser: Profile '%s' of User '%s' seems to exists but is invalid (File '%s). Skipped", fd.name, pEntry->userId.c_str(), filename.c_str());
				}
			}
		} while ( pCryPak->FindNext( handle, &fd ) >= 0 );

		pCryPak->FindClose( handle );
	}

	return true;
}

//------------------------------------------------------------------------
bool CPlayerProfileImplFSDir::DeleteProfile(SUserEntry* pEntry, const char* name)
{
	string path;
	InternalMakeFSPath(pEntry, name, path);  // no profile name -> only path
	bool bOK = gEnv->pCryPak->RemoveDir(path.c_str(), true);
	// in case the savegame folder is shared, we have to delete the savegames from the shared folder
	if (bOK && m_pMgr->IsSaveGameFolderShared())
	{
		CPlayerProfileManager::TSaveGameInfoVec saveGameInfoVec;
		if (GetSaveGames(pEntry, saveGameInfoVec, name)) // provide alternate profileName, because pEntry->pCurrentProfile points to the active profile
		{
			CPlayerProfileManager::TSaveGameInfoVec::iterator iter = saveGameInfoVec.begin();
			CPlayerProfileManager::TSaveGameInfoVec::iterator iterEnd = saveGameInfoVec.end();
			while (iter != iterEnd)
			{
				DeleteSaveGame(pEntry, iter->name);
				++iter;
			}
		}
	}
	return bOK;
}


//------------------------------------------------------------------------
bool CPlayerProfileImplFSDir::RenameProfile(SUserEntry* pEntry, const char* newName)
{
	// TODO: only rename in the filesystem. for now save new and delete old
	const char* oldName = pEntry->pCurrentProfile->GetName();
	bool ok = SaveProfile(pEntry, pEntry->pCurrentProfile, newName);
	if (!ok)
		return false;

	// move the save games
	if (CPlayerProfileManager::sUseRichSaveGames)
	{
		CRichSaveGameHelper sgHelper(this);
		sgHelper.MoveSaveGames(pEntry, oldName, newName);
	}
	else
	{
		CCommonSaveGameHelper sgHelper(this);
		sgHelper.MoveSaveGames(pEntry, oldName, newName);
	}

	DeleteProfile(pEntry, oldName);
	return true;
}


//------------------------------------------------------------------------
bool CPlayerProfileImplFSDir::LogoutUser(SUserEntry* pEntry)
{
	return true;
}

//------------------------------------------------------------------------
bool CPlayerProfileImplFSDir::GetSaveGames(SUserEntry* pEntry, CPlayerProfileManager::TSaveGameInfoVec& outVec, const char* altProfileName)
{
	if (CPlayerProfileManager::sUseRichSaveGames)
	{
		CRichSaveGameHelper sgHelper(this);
		return sgHelper.GetSaveGames(pEntry, outVec, altProfileName);
	}
	else
	{
		CCommonSaveGameHelper sgHelper(this);
		return sgHelper.GetSaveGames(pEntry, outVec, altProfileName);
	}
}

//------------------------------------------------------------------------
ISaveGame* CPlayerProfileImplFSDir::CreateSaveGame(SUserEntry* pEntry)
{
	if (CPlayerProfileManager::sUseRichSaveGames)
	{
		CRichSaveGameHelper sgHelper(this);
		return sgHelper.CreateSaveGame(pEntry);
	}
	else
	{
		CCommonSaveGameHelper sgHelper(this);
		return sgHelper.CreateSaveGame(pEntry);
	}
}

//------------------------------------------------------------------------
ILoadGame* CPlayerProfileImplFSDir::CreateLoadGame(SUserEntry* pEntry)
{
	if (CPlayerProfileManager::sUseRichSaveGames)
	{
		CRichSaveGameHelper sgHelper(this);
		return sgHelper.CreateLoadGame(pEntry);
	}
	else
	{
		CCommonSaveGameHelper sgHelper(this);
		return sgHelper.CreateLoadGame(pEntry);
	}
}

//------------------------------------------------------------------------
bool CPlayerProfileImplFSDir::DeleteSaveGame(SUserEntry* pEntry, const char* name)
{
	if (CPlayerProfileManager::sUseRichSaveGames)
	{
		CRichSaveGameHelper sgHelper(this);
		return sgHelper.DeleteSaveGame(pEntry, name);
	}
	else
	{
		CCommonSaveGameHelper sgHelper(this);
		return sgHelper.DeleteSaveGame(pEntry, name);
	}
}

//------------------------------------------------------------------------
bool CPlayerProfileImplFSDir::GetSaveGameThumbnail(SUserEntry* pEntry, const char* saveGameName, SThumbnail& thumbnail)
{
	if (CPlayerProfileManager::sUseRichSaveGames)
	{
		CRichSaveGameHelper sgHelper(this);
		return sgHelper.GetSaveGameThumbnail(pEntry, saveGameName, thumbnail);
	}
	else
	{
		CCommonSaveGameHelper sgHelper(this);
		return sgHelper.GetSaveGameThumbnail(pEntry, saveGameName, thumbnail);
	}
}


ILevelRotationFile* CPlayerProfileImplFSDir::GetLevelRotationFile(SUserEntry* pEntry, const char* name)
{
  CCommonSaveGameHelper sgHelper(this);
  return sgHelper.GetLevelRotationFile(pEntry,name);
}

//------------------------------------------------------------------------
// COMMON SaveGameHelper used by both CPlayerProfileImplFSDir and CPlayerProfileImplFS
//------------------------------------------------------------------------

bool CCommonSaveGameHelper::GetSaveGameThumbnail(CPlayerProfileManager::SUserEntry* pEntry, const char* saveGameName, CPlayerProfileManager::SThumbnail& thumbnail)
{
	assert (pEntry->pCurrentProfile != 0);
	if (pEntry->pCurrentProfile == 0)
	{
		GameWarning("CCommonSaveGameHelper:GetSaveGameThumbnail: Entry for user '%s' has no current profile", pEntry->userId.c_str());
		return false;
	}
	const char* name = saveGameName;
	string filename;
	m_pImpl->InternalMakeFSSaveGamePath(pEntry, pEntry->pCurrentProfile->GetName(), filename, true);
	string strippedName = PathUtil::ReplaceExtension(PathUtil::GetFile(name), ".bmp");
	filename.append(strippedName);

	int width = 0;
	int height = 0;
	int depth = 0;
	bool bSuccess = BMPHelper::LoadBMP(filename, 0, width, height, depth);
	if (bSuccess)
	{
		thumbnail.data.resize(width*height*depth);
		bSuccess = BMPHelper::LoadBMP(filename, thumbnail.data.begin(), width, height, depth);
		if (bSuccess)
		{
			thumbnail.height = height;
			thumbnail.width = width;
			thumbnail.depth = depth;
		}
	}
	if (!bSuccess)
		thumbnail.ReleaseData();
	return bSuccess;
}


bool CCommonSaveGameHelper::FetchMetaData(XmlNodeRef& root, CPlayerProfileManager::SSaveGameMetaData& metaData)
{
	// TODO: use CXmlLoadGame for this
	XmlNodeRef metaDataNode = root;
	if (metaDataNode->isTag("Metadata") == false)
		metaDataNode = root->findChild("Metadata");
	if (metaDataNode == 0)
		return false;
	bool ok = true;
	ok &= GetAttr(metaDataNode, "level", metaData.levelName);
	ok &= GetAttr(metaDataNode, "gameRules", metaData.gameRules);
	ok &= GetAttr(metaDataNode, "version", metaData.fileVersion);
	ok &= GetAttr(metaDataNode, "build", metaData.buildVersion);
	ok &= GetTimeAttr(metaDataNode, "saveTime", metaData.saveTime);
	metaData.xmlMetaDataNode = metaDataNode;
	return ok;
}


bool CCommonSaveGameHelper::GetSaveGames(CPlayerProfileManager::SUserEntry* pEntry, CPlayerProfileManager::TSaveGameInfoVec& outVec, const char* altProfileName="")
{
	// Scan savegames directory for XML files
	// we scan only for save game meta information
	string path;
	string profileName = (altProfileName && *altProfileName) ? altProfileName : pEntry->pCurrentProfile->GetName();
	m_pImpl->InternalMakeFSSaveGamePath(pEntry, profileName, path, true); 

	const bool bNeedProfilePrefix = m_pImpl->GetManager()->IsSaveGameFolderShared();
	string profilePrefix = profileName;
	profilePrefix+='_';
	size_t profilePrefixLen = profilePrefix.length();

	ICryPak * pCryPak = gEnv->pCryPak;
	_finddata_t fd;

	path.TrimRight("/\\");
	string search = path + "/*.CRYSISJMSF";
	intptr_t handle = pCryPak->FindFirst( search.c_str(), &fd );
	if (handle != -1)
	{
		CPlayerProfileManager::SSaveGameInfo sgInfo;
		do
		{
			if (strcmp(fd.name, ".") == 0 || strcmp(fd.name, "..") == 0)
				continue;

			if (bNeedProfilePrefix)
			{
				if (strnicmp(profilePrefix, fd.name, profilePrefixLen) != 0)
					continue;
			}

			sgInfo.name = fd.name;
			if (bNeedProfilePrefix) // skip profile_ prefix (we made sure this is valid by comparism above)
				sgInfo.humanName = fd.name+profilePrefixLen;
			else
				sgInfo.humanName = fd.name;
			PathUtil::RemoveExtension(sgInfo.humanName);
			sgInfo.description = "no description";

			bool ok = false;

			// filename construction
			string metaFilename = path;
			metaFilename.append("/");
			metaFilename.append(fd.name);
			metaFilename = PathUtil::ReplaceExtension(metaFilename, ".meta");
			XmlNodeRef rootNode = LoadXMLFile(metaFilename.c_str());
			// see if the root tag is o.k.
			if (rootNode && stricmp(rootNode->getTag(), "Metadata") == 0)
			{
				// get meta data
				ok = FetchMetaData(rootNode, sgInfo.metaData);
			}
			else
			{
				// when there is no meta information, we currently reject the savegame
				// ok = true; // un-comment this, if you want to accept savegames without meta
			}

			if (ok)
			{
				outVec.push_back(sgInfo);
			}
			else
			{
				GameWarning("CPlayerProfileImplFS::GetSaveGames: SaveGame '%s' of user '%s' is invalid", fd.name, pEntry->userId.c_str());
			}
		} while ( pCryPak->FindNext( handle, &fd ) >= 0 );

		pCryPak->FindClose( handle );
	}

	return true;
}


ISaveGame* CCommonSaveGameHelper::CreateSaveGame(CPlayerProfileManager::SUserEntry* pEntry)
{
	class CXMLSaveGameFSDir : public CXmlSaveGame
	{
	public:
		CXMLSaveGameFSDir(ICommonProfileImpl* pImpl, CPlayerProfileImplFSDir::SUserEntry* pEntry)
		{
			m_pProfileImpl = pImpl;
			m_pEntry = pEntry;
			assert (m_pProfileImpl != 0);
			assert (m_pEntry != 0);
		}

		// ILoadGame
		virtual bool Init(const char* name)
		{
			assert (m_pEntry->pCurrentProfile != 0);
			if (m_pEntry->pCurrentProfile == 0)
			{
				GameWarning("CXMLSaveGameFSDir: Entry for user '%s' has no current profile", m_pEntry->userId.c_str());
				return false;
			}
			string path;
			m_pProfileImpl->InternalMakeFSSaveGamePath(m_pEntry, m_pEntry->pCurrentProfile->GetName(), path, false);
			// make directory or use the SaveXMLFile helper function
			// CryCreateDirectory(...)
			string strippedName = PathUtil::GetFile(name);
			path.append(strippedName);
			return CXmlSaveGame::Init(path.c_str());
		}

		virtual bool Write( const char * filename, XmlNodeRef data )
		{
			// creates directory as well
			const string fname (filename);
			//bool bOK = ::SaveXMLFile(fname, data);
			bool bOK = CXmlSaveGame::Write(fname, data); // writes compressed file
			if (bOK)
			{
				// we save the meta information also in a separate file
				XmlNodeRef meta = data->findChild("Metadata");
				if (meta)
				{
					const string metaPath = PathUtil::ReplaceExtension(fname, ".meta");
					bOK = ::SaveXMLFile(metaPath, meta);
				}
			}
			if (m_thumbnail.data.size() > 0)
			{
				const string bmpPath = PathUtil::ReplaceExtension(fname, ".bmp");
				BMPHelper::SaveBMP(bmpPath, m_thumbnail.data.begin(), m_thumbnail.width, m_thumbnail.height, m_thumbnail.depth, false);
			}
			return bOK;
		}

		virtual uint8* SetThumbnail(const uint8* imageData, int width, int height, int depth)
		{
			m_thumbnail.width = width;
			m_thumbnail.height = height;
			m_thumbnail.depth = depth;
			size_t size = width*height*depth;
			m_thumbnail.data.resize(size);
			if (imageData)
				memcpy(m_thumbnail.data.begin(), imageData, size);
			else
			{
				if (m_thumbnail.depth == 3)
				{
					uint8* p = (uint8*) m_thumbnail.data.begin();
					size_t n = size;
					while (n)
					{
						*p++=0x00; // B
						*p++=0x00; // G
						*p++=0x00; // R
						n-=3;
					}
				}
				else if (m_thumbnail.depth == 4)
				{
					const uint32 col = RGBA8(0x00,0x00,0x00,0x00); // alpha see through
					uint32* p = (uint32*) m_thumbnail.data.begin();
					size_t n = size >> 2;
					while (n--)
						*p++=col;
				}
				else 
				{
					memset(m_thumbnail.data.begin(), 0, size);
				}
			}
			return m_thumbnail.data.begin();
		}

		virtual bool SetThumbnailFromBMP(const char* filename)
		{
			int width = 0;
			int height = 0;
			int depth = 0;
			bool bSuccess = BMPHelper::LoadBMP(filename, 0, width, height, depth, true);
			if (bSuccess)
			{
				CPlayerProfileManager::SThumbnail thumbnail;
				thumbnail.data.resize(width*height*depth);
				bSuccess = BMPHelper::LoadBMP(filename, thumbnail.data.begin(), width, height, depth, true);
				if (bSuccess)
				{
					SetThumbnail(thumbnail.data.begin(), width, height, depth);
				}
			}
			return bSuccess;
		}

		ICommonProfileImpl* m_pProfileImpl;
		CPlayerProfileImplFSDir::SUserEntry* m_pEntry;
		CPlayerProfileImplFSDir::SThumbnail m_thumbnail;
	};

	return new CXMLSaveGameFSDir(m_pImpl, pEntry);
}

ILoadGame*  CCommonSaveGameHelper::CreateLoadGame(CPlayerProfileManager::SUserEntry* pEntry)
{
	class CXMLLoadGameFSDir : public CXmlLoadGame
	{
	public:
		CXMLLoadGameFSDir(ICommonProfileImpl* pImpl, CPlayerProfileImplFSDir::SUserEntry* pEntry)
		{
			m_pImpl = pImpl;
			m_pEntry = pEntry;
			assert (m_pImpl != 0);
			assert (m_pEntry != 0);
		}

		// ILoadGame
		virtual bool Init(const char* name)
		{
			assert (m_pEntry->pCurrentProfile != 0);
			if (m_pEntry->pCurrentProfile == 0)
			{
				GameWarning("CXMLLoadGameFSDir: Entry for user '%s' has no current profile", m_pEntry->userId.c_str());
				return false;
			}

			string filename;
			// figure out, if 'name' is an absolute path or a profile-relative path
			if (gEnv->pCryPak->IsAbsPath(name) == false)
			{
				// no full path, assume 'name' is local to profile directory
				bool bNeedFolder = true;
				if (m_pImpl->GetManager()->IsSaveGameFolderShared())
				{
					// if the savegame's name doesn't start with a profile_ prefix
					// add one (for quickload)
					string profilePrefix = m_pEntry->pCurrentProfile->GetName();
					profilePrefix.append("_");
					size_t profilePrefixLen = profilePrefix.length();
					if (strnicmp(name, profilePrefix, profilePrefixLen) != 0)
						bNeedFolder = false;
				}
				m_pImpl->InternalMakeFSSaveGamePath(m_pEntry, m_pEntry->pCurrentProfile->GetName(), filename, bNeedFolder);
				string strippedName = PathUtil::GetFile(name);
				filename.append(strippedName);
			}
			else
			{
				// it's an abs path, assign it
				filename.assign(name);
			}
			return CXmlLoadGame::Init(filename.c_str());
		}

		ICommonProfileImpl* m_pImpl;
		CPlayerProfileImplFSDir::SUserEntry* m_pEntry;

	};

	return new CXMLLoadGameFSDir(m_pImpl, pEntry);

}

bool CCommonSaveGameHelper::DeleteSaveGame(CPlayerProfileManager::SUserEntry* pEntry, const char* name)
{
	string filename;
	m_pImpl->InternalMakeFSSaveGamePath(pEntry, pEntry->pCurrentProfile->GetName(), filename, true);
	string strippedName = PathUtil::GetFile(name);
	filename.append(strippedName);
	bool ok = gEnv->pCryPak->RemoveFile(filename.c_str()); // normal savegame
	filename = PathUtil::ReplaceExtension(filename, ".meta");
	gEnv->pCryPak->RemoveFile(filename.c_str()); // meta data
	filename = PathUtil::ReplaceExtension(filename, ".bmp");
	gEnv->pCryPak->RemoveFile(filename.c_str()); // thumbnail
	return ok;
}

bool CCommonSaveGameHelper::MoveSaveGames(CPlayerProfileManager::SUserEntry* pEntry, const char* oldProfileName, const char* newProfileName)
{
	// move savegames or, if savegame folder is shared, rename them
	string oldSaveGamesPath;
	string newSaveGamesPath;
	m_pImpl->InternalMakeFSSaveGamePath(pEntry, oldProfileName, oldSaveGamesPath, true);
	m_pImpl->InternalMakeFSSaveGamePath(pEntry, newProfileName, newSaveGamesPath, true);

	CPlayerProfileManager* pMgr = m_pImpl->GetManager();
	if (pMgr->IsSaveGameFolderShared() == false)
	{
		// move complete folder
		pMgr->MoveFileHelper(oldSaveGamesPath, newSaveGamesPath);
	}
	else
	{
		// save game folder is shared, move file by file
		CPlayerProfileManager::TSaveGameInfoVec saveGameInfoVec;
		if (GetSaveGames(pEntry, saveGameInfoVec, oldProfileName)) 
		{
			CPlayerProfileManager::TSaveGameInfoVec::iterator iter = saveGameInfoVec.begin();
			CPlayerProfileManager::TSaveGameInfoVec::iterator iterEnd = saveGameInfoVec.end();
			string oldPrefix = oldProfileName;
			oldPrefix+="_";
			size_t oldPrefixLen = oldPrefix.length();
			string newPrefix = newProfileName;
			newPrefix+="_";
			while (iter != iterEnd)
			{
				const string& oldSGName = iter->name;
				// begins with old profile's prefix?
				if (strnicmp(oldSGName, oldPrefix, oldPrefixLen) == 0)
				{
					string newSGName = newPrefix;
					newSGName.append(oldSGName, oldPrefixLen, oldSGName.length()-oldPrefixLen);
					string oldPath = oldSaveGamesPath + oldSGName;
					string newPath = newSaveGamesPath + newSGName;
					pMgr->MoveFileHelper(oldPath, newPath); // savegame

					oldPath = PathUtil::ReplaceExtension(oldPath, ".meta");
					newPath = PathUtil::ReplaceExtension(newPath, ".meta");
					pMgr->MoveFileHelper(oldPath, newPath); // meta

					oldPath = PathUtil::ReplaceExtension(oldPath, ".bmp");
					newPath = PathUtil::ReplaceExtension(newPath, ".bmp");
					pMgr->MoveFileHelper(oldPath, newPath); // thumbnail
				}
				++iter;
			}
		}
	}
	return true;
}

class CLevelRotationFile : public ILevelRotationFile
{
public:
  CLevelRotationFile(const string& fname):m_filename(fname)
  {}
  ~CLevelRotationFile()
  {}
  
  virtual bool Save(XmlNodeRef r)
  {
    return r->saveToFile(m_filename,256*1024);
  }

  virtual XmlNodeRef Load()
  {
    if(FILE* f = gEnv->pCryPak->FOpen(m_filename,"rb"))
    {
      gEnv->pCryPak->FClose(f);
      return gEnv->pSystem->LoadXmlFile(m_filename);
    }
    else
        return gEnv->pSystem->LoadXmlFile("Libs/Config/Profiles/Default/levelrotation/levelrotation.xml");
  }

  virtual void Complete()
  {
    delete this;
  }
private:
  string m_filename;
};

ILevelRotationFile* CCommonSaveGameHelper::GetLevelRotationFile(CPlayerProfileManager::SUserEntry* pEntry, const char* name)
{
  string filename = gEnv->pSystem->GetRootFolder();
	if (filename.empty())
	{
		m_pImpl->InternalMakeFSPath(pEntry, pEntry->pCurrentProfile->GetName(), filename);
		filename.append("levelrotation/");
	}
	string strippedName = PathUtil::GetFile(name);
  filename.append(strippedName);
  filename.append(".xml");
  return new CLevelRotationFile(filename);
}

#endif
