#include "StdAfx.h"
#include "PlayerProfileManager.h"
#include "PlayerProfile.h"
#include "CryAction.h"
#include "IActionMapManager.h"

#define SHARED_SAVEGAME_FOLDER "%USER%/SaveGames"

namespace
{
	const char* FACTORY_DEFAULT_NAME = "default";
	const char* PLAYER_DEFAULT_PROFILE_NAME  = "default";
}

namespace TESTING
{
	void DumpProfiles(IPlayerProfileManager* pFS, const char* userId)
	{
		int nProfiles = pFS->GetProfileCount(userId);
		IPlayerProfileManager::SProfileDescription desc;
		CryLogAlways("User %s has %d profiles", userId, nProfiles);
		for (int i=0;i<nProfiles;++i)
		{
			pFS->GetProfileInfo(userId, i, desc);
			CryLogAlways("#%d: '%s'", i, desc.name);
		}
	}

	void DumpAttrs(IPlayerProfile* pProfile)
	{
		if (pProfile == 0)
			return;
		IAttributeEnumeratorPtr pEnum = pProfile->CreateAttributeEnumerator();
		IAttributeEnumerator::SAttributeDescription desc;
		CryLogAlways("Attributes of profile %s", pProfile->GetName());
		int i = 0;
		TFlowInputData val;
		while (pEnum->Next(desc))
		{
			pProfile->GetAttribute(desc.name, val);
			string sVal;
			val.GetValueWithConversion(sVal);
			CryLogAlways("Attr %d: %s=%s", i, desc.name, sVal.c_str());
			++i;
		}
	}

	void DumpSaveGames(IPlayerProfile* pProfile)
	{
		ISaveGameEnumeratorPtr pSGE = pProfile->CreateSaveGameEnumerator();
		ISaveGameEnumerator::SGameDescription desc;
		CryLogAlways("SaveGames for Profile '%s'", pProfile->GetName());
		char timeBuf[256];
		struct tm *timePtr;
		for (int i=0; i<pSGE->GetCount(); ++i)
		{
			pSGE->GetDescription(i, desc);
			timePtr = localtime(&desc.metaData.saveTime);
			const char* timeString = timeBuf;
			if (strftime(timeBuf, sizeof(timeBuf), "%#c", timePtr) == 0)
				timeString = asctime(timePtr);
			CryLogAlways("SaveGame %d/%d: name='%s' humanName='%s' desc='%s'", i, pSGE->GetCount()-1, desc.name, desc.humanName, desc.description);
			CryLogAlways("MetaData: level=%s gr=%s version=%d build=%s savetime=%s",
				desc.metaData.levelName, desc.metaData.gameRules, desc.metaData.fileVersion, desc.metaData.buildVersion,
				timeString);
		}
	}

	void DumpActionMap(IPlayerProfile* pProfile, const char* name)
	{
		IActionMap* pMap = pProfile->GetActionMap(name);
		if (pMap)
		{
			int iAction = 0;
			IActionMapBindInfoIteratorPtr pIter = pMap->CreateBindInfoIterator();
			while (const SActionMapBindInfo* pInfo = pIter->Next())
			{
				CryLogAlways("Action %d: '%s'", iAction++, pInfo->action);
				for (int i=0; i<pInfo->nKeys; ++i)
				{
					CryLogAlways("Key %d/%d: '%s'", i, pInfo->nKeys-1, pInfo->keys[i]);
				}
			}
		}
	}

	void DumpMap(IConsoleCmdArgs* args)
	{
		IActionMapManager* pAM = CCryAction::GetCryAction()->GetIActionMapManager();
		IActionMapIteratorPtr iter = pAM->CreateActionMapIterator();
		while (IActionMap* pMap = iter->Next())
		{
			CryLogAlways("ActionMap: '%s' 0x%p", pMap->GetName(), pMap);
			int iAction = 0;
			IActionMapBindInfoIteratorPtr pIter = pMap->CreateBindInfoIterator();
			while (const SActionMapBindInfo* pInfo = pIter->Next())
			{
				CryLogAlways("Action %d: '%s'", iAction++, pInfo->action);
				for (int i=0; i<pInfo->nKeys; ++i)
				{
					CryLogAlways("Key %d/%d: '%s'", i, pInfo->nKeys-1, pInfo->keys[i]);
				}
			}
		}
	}

	void TestProfile( IConsoleCmdArgs* args )
	{
		const char* userName = GetISystem()->GetUserName();
		IPlayerProfileManager::EProfileOperationResult result;
		IPlayerProfileManager* pFS = CCryAction::GetCryAction()->GetIPlayerProfileManager();

		// test renaming current profile
#if 0
		pFS->RenameProfile(userName, "newOne4", result);
		return;
#endif

		bool bFirstTime = false;
		pFS->LoginUser(userName, bFirstTime);
		DumpProfiles(pFS,userName);
		pFS->DeleteProfile(userName, "PlayerCool", result);
		IPlayerProfile* pProfile = pFS->ActivateProfile(userName, "default");
		if (pProfile)
		{
			DumpActionMap(pProfile, "default");
			DumpAttrs(pProfile);
			pProfile->SetAttribute("hallo2", TFlowInputData(222));
			pProfile->SetAttribute("newAddedAttribute", TFlowInputData(24.10f));
			DumpAttrs(pProfile);
			pProfile->SetAttribute("newAddedAttribute", TFlowInputData(25.10f));
			DumpAttrs(pProfile);
			pProfile->ResetAttribute("newAddedAttribute");
			pProfile->ResetAttribute("hallo2");
			DumpAttrs(pProfile);
			DumpSaveGames(pProfile);
			float fVal;
			pProfile->GetAttribute("newAddedAttribute", fVal);
			pProfile->SetAttribute("newAddedAttribute", 2.22f);
		}
		else
		{
			CryLogAlways("Can't activate profile 'default'");
		}

		const IPlayerProfile* pPreviewProfile = pFS->PreviewProfile(userName, "previewTest");
		if (pPreviewProfile)
		{
			float fVal;
			pPreviewProfile->GetAttribute("previewData", fVal);
			pPreviewProfile->GetAttribute("previewData2", fVal, true);
		}

		DumpProfiles(pFS,userName);
		// pFS->RenameProfile(userName, "new new profile");
		pFS->LogoutUser(userName);
	}
}

int CPlayerProfileManager::sUseRichSaveGames = 0;
int CPlayerProfileManager::sRSFDebugWrite = 0;
int CPlayerProfileManager::sRSFDebugWriteOnLoad = 0;

//------------------------------------------------------------------------
CPlayerProfileManager::CPlayerProfileManager(CPlayerProfileManager::IPlatformImpl* pImpl) :
	m_bInitialized(false),
	m_pDefaultProfile(0),
	m_pImpl(pImpl)
{
	assert (m_pImpl != 0);

	// FIXME: TODO: temp stuff
	static bool testInit = false;
	if (testInit == false)
	{
		testInit = true;
		IConsole * pC = ::gEnv->pConsole;
		pC->Register("pp_RichSaveGames", &CPlayerProfileManager::sUseRichSaveGames, 1, 0, "Enable RichSaveGame Format for SaveGames");
		pC->Register("pp_RSFDebugWrite", &CPlayerProfileManager::sRSFDebugWrite, gEnv->pSystem->IsDevMode() ? 1 : 0, 0, "When RichSaveGames are enabled, save plain XML Data alongside for debugging");
		pC->Register("pp_RSFDebugWriteOnLoad", &CPlayerProfileManager::sRSFDebugWriteOnLoad, 0, 0, "When RichSaveGames are enabled, save plain XML Data alongside for debugging when loading a savegame");
		pC->AddCommand("test_profile", TESTING::TestProfile);
		pC->AddCommand("dump_action_maps", TESTING::DumpMap);
	}

	m_sharedSaveGameFolder = SHARED_SAVEGAME_FOLDER; // by default, use a shared savegame folder (Games For Windows Requirement)
	m_sharedSaveGameFolder.TrimRight("/\\");
}

//------------------------------------------------------------------------
CPlayerProfileManager::~CPlayerProfileManager()
{
	// don't call virtual Shutdown or any other virtual function,
	// but delete things directly
	if (m_bInitialized)
	{
		GameWarning("[PlayerProfiles] CPlayerProfileManager::~CPlayerProfileManager Shutdown not called!");
	}
	std::for_each(m_userVec.begin(), m_userVec.end(), stl::container_object_deleter());
	SAFE_DELETE(m_pDefaultProfile);
	if (m_pImpl)
	{
		m_pImpl->Release();
		m_pImpl = 0;
	}

	IConsole * pC = ::gEnv->pConsole;
	pC->UnregisterVariable("pp_RichSaveGames", true);
	pC->RemoveCommand("test_profile");
	pC->RemoveCommand("dump_action_maps");
}

//------------------------------------------------------------------------
bool CPlayerProfileManager::Initialize()
{
 	if (m_bInitialized)
		return true;

	m_pImpl->Initialize(this);

	m_pDefaultProfile = new CPlayerProfile(this, FACTORY_DEFAULT_NAME, "", false);
	bool ok = m_pImpl->LoadProfile(0, m_pDefaultProfile, m_pDefaultProfile->GetName());
	if (!ok)
	{
		GameWarning("[PlayerProfiles] Cannot load factory default profile '%s'", FACTORY_DEFAULT_NAME);
		SAFE_DELETE(m_pDefaultProfile);
	}

	m_bInitialized = ok;
	return m_bInitialized;
}

//------------------------------------------------------------------------
bool CPlayerProfileManager::Shutdown()
{
	while (m_userVec.empty () == false)
	{
		SUserEntry* pEntry = m_userVec.back();
		LogoutUser(pEntry->userId);
	}
	SAFE_DELETE(m_pDefaultProfile);
	m_bInitialized = false;
	return true;
}

//------------------------------------------------------------------------
int CPlayerProfileManager::GetUserCount()
{
	return m_userVec.size();
}

//------------------------------------------------------------------------
bool CPlayerProfileManager::GetUserInfo(int index, IPlayerProfileManager::SUserInfo& outInfo)
{
	if (index < 0 || index >= m_userVec.size())
	{
		assert (index >= 0 && index < m_userVec.size());
		return false;
	}

	SUserEntry* pEntry = m_userVec[index];
	outInfo.userId = pEntry->userId;
	return true;
}

//------------------------------------------------------------------------
bool CPlayerProfileManager::LoginUser(const char* userId, bool& bOutFirstTime)
{
	if (m_bInitialized == false)
		return false;

	bOutFirstTime = false;

	m_curUserID = userId;

	// user already logged in
	SUserEntry* pEntry = FindEntry(userId);
	if (pEntry)
		return true;

	SUserEntry* newEntry = new SUserEntry(userId);
	newEntry->pCurrentProfile = 0;
	// login the user and fill SUserEntry
	bool ok = m_pImpl->LoginUser(newEntry);

	if (!ok)
	{
		delete newEntry;
		GameWarning("[PlayerProfiles] Cannot login user '%s'", userId);
		return false;
	}

	// no entries yet -> create default profile by copying/saving factory default
	if (newEntry->profileDesc.empty())
	{
		// save the default profile
		ok = m_pImpl->SaveProfile(newEntry, m_pDefaultProfile, PLAYER_DEFAULT_PROFILE_NAME);
		if (!ok)
		{
			GameWarning("[PlayerProfiles] Login of user '%s' failed because default profile '%s' cannot be created", userId, PLAYER_DEFAULT_PROFILE_NAME);

			delete newEntry;
			// TODO: maybe assign use factory default in this case, but then
			// cannot save anything incl. save-games
			return false;
		}
		CryLogAlways("[PlayerProfiles] Login of user '%s': First Time Login - Successfully created default profile '%s'", userId, PLAYER_DEFAULT_PROFILE_NAME);
		SLocalProfileInfo info(PLAYER_DEFAULT_PROFILE_NAME);
		newEntry->profileDesc.push_back(info);
		bOutFirstTime = true;
	}

	CryLogAlways("[PlayerProfiles] Login of user '%s' successful.", userId);
	if (bOutFirstTime == false)
	{
		CryLogAlways("[PlayerProfiles] Found %d profiles.", newEntry->profileDesc.size());
		for (int i=0; i<newEntry->profileDesc.size(); ++i)
			CryLogAlways("   Profile %d : '%s'", i, newEntry->profileDesc[i].GetName().c_str());
	}

	m_userVec.push_back(newEntry);
	return true;
}

//------------------------------------------------------------------------
bool CPlayerProfileManager::LogoutUser(const char* userId)
{
	if (!m_bInitialized)
		return false;

	// check if user already logged in
	TUserVec::iterator iter;
	SUserEntry* pEntry = FindEntry(userId, iter);
	if (pEntry == 0)
	{
		GameWarning("[PlayerProfiles] Logout of user '%s' failed [was not logged in]", userId);
		return false;
	}

	// auto-save profile
	if (pEntry->pCurrentProfile)
	{
		bool ok = m_pImpl->SaveProfile(pEntry, pEntry->pCurrentProfile, pEntry->pCurrentProfile->GetName());
		if (!ok)
		{
			GameWarning("[PlayerProfiles] Logout of user '%s': Couldn't save profile '%s'", userId, pEntry->pCurrentProfile->GetName());
		}
	}

	m_pImpl->LogoutUser(pEntry);

	// delete entry from vector
	m_userVec.erase(iter);

	SAFE_DELETE(pEntry->pCurrentProfile);
	SAFE_DELETE(pEntry->pCurrentPreviewProfile);

	// delete entry
	delete pEntry;

	return true;
}

//------------------------------------------------------------------------
int CPlayerProfileManager::GetProfileCount(const char* userId)
{
	SUserEntry* pEntry = FindEntry(userId);
	if (pEntry == 0)
		return 0;
	return pEntry->profileDesc.size();
}

//------------------------------------------------------------------------
bool CPlayerProfileManager::GetProfileInfo(const char* userId, int index, IPlayerProfileManager::SProfileDescription& outInfo)
{
	if (!m_bInitialized)
		return false;

	SUserEntry* pEntry = FindEntry(userId);
	if (pEntry == 0)
	{
		GameWarning("[PlayerProfiles] GetProfileInfo: User '%s' not logged in", userId);
		return false;
	}
	int count = pEntry->profileDesc.size();
	if (index >= count)
	{
		assert (index < count);
		return false;
	}
	SLocalProfileInfo& info = pEntry->profileDesc[index];
	outInfo.name = info.GetName().c_str();
	return true;
}

//------------------------------------------------------------------------
bool CPlayerProfileManager::CreateProfile(const char* userId, const char* profileName, bool bOverride, IPlayerProfileManager::EProfileOperationResult& result)
{
	if (!m_bInitialized)
	{
		result = ePOR_NotInitialized;
		return false;
	}
	SUserEntry* pEntry = FindEntry(userId);
	if (pEntry == 0)
	{
		GameWarning("[PlayerProfiles] CreateProfile: User '%s' not logged in", userId);
		result = ePOR_UserNotLoggedIn;
		return false;
	}

	SLocalProfileInfo* pInfo = GetLocalProfileInfo(pEntry, profileName);
	if (pInfo != 0 && !bOverride) // profile with this name already exists
	{
		GameWarning("[PlayerProfiles] CreateProfile: User '%s' already has a profile with name '%s'", userId, profileName);
		result = ePOR_NameInUse;
		return false;
	}

	result = ePOR_Unknown;
	bool ok = false;
	if (pEntry->pCurrentProfile == 0)
	{
		// save the default profile
		ok = m_pImpl->SaveProfile(pEntry, m_pDefaultProfile, profileName);
	}
	else if (pEntry->pCurrentProfile)
	{
		ok = m_pImpl->SaveProfile(pEntry, pEntry->pCurrentProfile, profileName);
	}
	if (ok)
	{
		if (pInfo == 0) // if we override, it's already present
		{
			SLocalProfileInfo info (profileName);
			pEntry->profileDesc.push_back(info);
		}
		result = ePOR_Success;
	}
	else
	{
		GameWarning("[PlayerProfiles] CreateProfile: User '%s' cannot create profile '%s'", userId, profileName);
	}
	return ok;
}

//------------------------------------------------------------------------
bool CPlayerProfileManager::DeleteProfile(const char* userId, const char* profileName, IPlayerProfileManager::EProfileOperationResult& result)
{
	if (!m_bInitialized)
	{
		result = ePOR_NotInitialized;
		return false;
	}
	SUserEntry* pEntry = FindEntry(userId);
	if (pEntry == 0)
	{
		GameWarning("[PlayerProfiles] DeleteProfile: User '%s' not logged in", userId);
		result = ePOR_UserNotLoggedIn;
		return false;
	}

	// cannot delete last profile
	if (pEntry->profileDesc.size() <= 1)
	{
		GameWarning("[PlayerProfiles] DeleteProfile: User '%s' cannot delete last profile", userId);
		result = ePOR_ProfileInUse;
		return false;
	}

	// make sure there is such a profile
	std::vector<SLocalProfileInfo>::iterator iter;
	SLocalProfileInfo* info = GetLocalProfileInfo(pEntry, profileName, iter);
	if (info)
	{
		bool ok = m_pImpl->DeleteProfile(pEntry, profileName);
		if (ok)
		{
			pEntry->profileDesc.erase(iter);
			// if the profile was the current profile, delete it
			if (pEntry->pCurrentProfile != 0 && stricmp(profileName, pEntry->pCurrentProfile->GetName()) == 0)
			{
				delete pEntry->pCurrentProfile;
				pEntry->pCurrentProfile = 0;
				// TODO: Maybe auto-select a profile
			}
			result = ePOR_Success;
			return true;
		}
		result = ePOR_Unknown;
	}
	else
		result = ePOR_NoSuchProfile;

	return false;
}

//------------------------------------------------------------------------
bool CPlayerProfileManager::RenameProfile(const char* userId, const char* newName, IPlayerProfileManager::EProfileOperationResult& result)
{
	if (!m_bInitialized)
	{
		result = ePOR_NotInitialized;
		return false;
	}

	SUserEntry* pEntry = FindEntry(userId);
	if (pEntry == 0)
	{
		GameWarning("[PlayerProfiles] RenameProfile: User '%s' not logged in", userId);
		result =  ePOR_UserNotLoggedIn;
		return false;
	}

	// make sure there is no such profile
	if (GetLocalProfileInfo(pEntry, newName) != 0)
	{
		result = ePOR_NoSuchProfile;
		return false;
	}

	// can only rename current active profile
	if (pEntry->pCurrentProfile == 0)
	{
		result = ePOR_NoActiveProfile;
		return false;
	}

	if (stricmp(pEntry->pCurrentProfile->GetName(), PLAYER_DEFAULT_PROFILE_NAME) == 0)
	{
		GameWarning("[PlayerProfiles] RenameProfile: User '%s' cannot rename default profile", userId);
		result = ePOR_DefaultProfile;
		return false;
	}

	result = ePOR_Unknown;
	bool ok = m_pImpl->RenameProfile(pEntry, newName);

	if (ok)
	{
		// assign a new name in the info DB
		SLocalProfileInfo* info = GetLocalProfileInfo(pEntry, pEntry->pCurrentProfile->GetName());
		info->SetName(newName);

		// assign a new name in the profile itself
		pEntry->pCurrentProfile->SetName(newName);
		result = ePOR_Success;
	}
	else
	{
		GameWarning("[PlayerProfiles] RenameProfile: Failed to rename profile to '%s' for user '%s' ", newName, userId);
	}
	return ok;
}

//------------------------------------------------------------------------
bool CPlayerProfileManager::SaveProfile(const char* userId, IPlayerProfileManager::EProfileOperationResult& result)
{
	if (!m_bInitialized)
	{
		result = ePOR_NotInitialized;
		return false;
	}

	SUserEntry* pEntry = FindEntry(userId);
	if (pEntry == 0)
	{
		GameWarning("[PlayerProfiles] SaveProfile: User '%s' not logged in", userId);
		result = ePOR_UserNotLoggedIn;
		return false;
	}

	if (pEntry->pCurrentProfile == 0)
	{
		GameWarning("[PlayerProfiles] SaveProfile: User '%s' not logged in", userId);
		result = ePOR_NoActiveProfile;
		return false;
	}

	result = ePOR_Success;
	bool ok = m_pImpl->SaveProfile(pEntry, pEntry->pCurrentProfile, pEntry->pCurrentProfile->GetName());

	if (!ok)
	{
		GameWarning("[PlayerProfiles] SaveProfile: User '%s' cannot save profile '%s'", userId, pEntry->pCurrentProfile->GetName());
		result = ePOR_Unknown;
	}
	return ok;
}

//------------------------------------------------------------------------
const IPlayerProfile* CPlayerProfileManager::PreviewProfile(const char* userId, const char* profileName)
{
	SUserEntry* pEntry = FindEntry(userId);
	if (pEntry == 0) // no such user
	{
		GameWarning("[PlayerProfiles] ActivateProfile: User '%s' not logged in", userId);
		return 0;
	}

	// if this is the current profile, do nothing
	if (pEntry->pCurrentPreviewProfile != 0 && profileName && stricmp(profileName, pEntry->pCurrentPreviewProfile->GetName()) == 0)
	{
		return pEntry->pCurrentPreviewProfile;
	}

	if (pEntry->pCurrentPreviewProfile != 0)
	{
		delete pEntry->pCurrentPreviewProfile;
		pEntry->pCurrentPreviewProfile = 0;
	}

	if (profileName == 0 || *profileName=='\0')
		return 0;

	SLocalProfileInfo* info = GetLocalProfileInfo(pEntry, profileName);
	if (info == 0) // no such profile
	{
		GameWarning("[PlayerProfiles] PreviewProfile: User '%s' has no profile '%s'", userId, profileName);
		return 0;
	}

	pEntry->pCurrentPreviewProfile = new CPlayerProfile(this, profileName, userId, true);
	const bool ok = m_pImpl->LoadProfile(pEntry, pEntry->pCurrentPreviewProfile, profileName);
	if (!ok)
	{
		GameWarning("[PlayerProfiles] PreviewProfile: User '%s' cannot load profile '%s'", userId, profileName);
		delete pEntry->pCurrentPreviewProfile;
		pEntry->pCurrentPreviewProfile = 0;
	}

	return pEntry->pCurrentPreviewProfile;
}

//------------------------------------------------------------------------
IPlayerProfile* CPlayerProfileManager::ActivateProfile(const char* userId, const char* profileName)
{
	SUserEntry* pEntry = FindEntry(userId);
	if (pEntry == 0) // no such user
	{
		GameWarning("[PlayerProfiles] ActivateProfile: User '%s' not logged in", userId);
		return 0;
	}

	IPlayerProfile* pCurrentProfile = pEntry->pCurrentProfile;

	// if this is the current profile, do nothing
	if (pEntry->pCurrentProfile != 0 && stricmp(profileName, pEntry->pCurrentProfile->GetName()) == 0)
	{
		return pEntry->pCurrentProfile;
	}

	SLocalProfileInfo* info = GetLocalProfileInfo(pEntry, profileName);
	if (info == 0) // no such profile
	{
		GameWarning("[PlayerProfiles] ActivateProfile: User '%s' has no profile '%s'", userId, profileName);
		return 0;
	}
	CPlayerProfile* pNewProfile = new CPlayerProfile(this, profileName, userId, false);

	// we save the current profile before loading the other (because actionmaps are not stored within the profile
	// itself, but taken from the ActionMapManager. So, if we load the new profile and then save the old one,
	// the keybindings of the old-one will get overwritten by the newly loaded one
	if (pEntry->pCurrentProfile != 0)
		m_pImpl->SaveProfile(pEntry, pEntry->pCurrentProfile, pEntry->pCurrentProfile->GetName());

	const bool ok = m_pImpl->LoadProfile(pEntry, pNewProfile, profileName);
	
	if (ok)
	{
		if (pEntry->pCurrentProfile != 0)
		{
			SAFE_DELETE(pEntry->pCurrentProfile);
		}
		pEntry->pCurrentProfile = pNewProfile;
	}
	else
	{
		GameWarning("[PlayerProfiles] ActivateProfile: User '%s' cannot load profile '%s'", userId, profileName);
		SAFE_DELETE(pNewProfile);
	}
	return pEntry->pCurrentProfile;
}

//------------------------------------------------------------------------
IPlayerProfile* CPlayerProfileManager::GetCurrentProfile(const char* userId)
{
	SUserEntry* pEntry = FindEntry(userId);
	return pEntry ? pEntry->pCurrentProfile : 0;
}

//------------------------------------------------------------------------
const char* CPlayerProfileManager::GetCurrentUser()
{
	return m_curUserID;
}

//------------------------------------------------------------------------
bool CPlayerProfileManager::ResetProfile(const char* userId)
{
	SUserEntry* pEntry = FindEntry(userId);
	if (pEntry == 0) // no such user
	{
		GameWarning("[PlayerProfiles] ResetProfile: User '%s' not logged in", userId);
		return false;
	}

	if (pEntry->pCurrentProfile == 0)
		return false;

	pEntry->pCurrentProfile->Reset();
	return true;
}

//------------------------------------------------------------------------
IPlayerProfile* CPlayerProfileManager::GetDefaultProfile()
{
	return m_pDefaultProfile;
}

//------------------------------------------------------------------------
CPlayerProfileManager::SUserEntry*
CPlayerProfileManager::FindEntry(const char* userId) const
{
	TUserVec::const_iterator iter = m_userVec.begin();
	while (iter != m_userVec.end())
	{
		const SUserEntry* pEntry = *iter;
		if (pEntry->userId.compare(userId) == 0)
		{
			return (const_cast<SUserEntry*> (pEntry));
		}
		++iter;
	}
	return 0;
}

//------------------------------------------------------------------------
CPlayerProfileManager::SUserEntry*
CPlayerProfileManager::FindEntry(const char* userId, CPlayerProfileManager::TUserVec::iterator& outIter) 
{
	TUserVec::iterator iter = m_userVec.begin();
	while (iter != m_userVec.end())
	{
		SUserEntry* pEntry = *iter;
		if (pEntry->userId.compare(userId) == 0)
		{
			outIter = iter;
			return pEntry;
		}
		++iter;
	}
	outIter = iter;
	return 0;
}

//------------------------------------------------------------------------
CPlayerProfileManager::SLocalProfileInfo* 
CPlayerProfileManager::GetLocalProfileInfo(SUserEntry* pEntry, const char* profileName) const
{
	std::vector<SLocalProfileInfo>::iterator iter = pEntry->profileDesc.begin();
	while (iter != pEntry->profileDesc.end())
	{
		SLocalProfileInfo& info = *iter;
		if (info.compare(profileName) == 0)
		{
			return & (const_cast<SLocalProfileInfo&> (info));
		}
		++iter;
	}
	return 0;
}

//------------------------------------------------------------------------
CPlayerProfileManager::SLocalProfileInfo* 
CPlayerProfileManager::GetLocalProfileInfo(SUserEntry* pEntry, const char* profileName, std::vector<SLocalProfileInfo>::iterator& outIter) const
{
	std::vector<SLocalProfileInfo>::iterator iter = pEntry->profileDesc.begin();
	while (iter != pEntry->profileDesc.end())
	{
		SLocalProfileInfo& info = *iter;
		if (info.compare(profileName) == 0)
		{
			outIter = iter;
			return & (const_cast<SLocalProfileInfo&> (info));
		}
		++iter;
	}
	outIter = iter;
	return 0;
}

struct CSaveGameThumbnail : public ISaveGameThumbnail
{
	CSaveGameThumbnail(const string& name) : m_nRefs(0), m_name(name) 
	{
	}

	void AddRef()
	{
		++m_nRefs;
	}

	void Release()
	{
		if (0 == --m_nRefs) 
			delete this; 
	}

	void GetImageInfo(int& width, int& height, int& depth)
	{
		width = m_image.width;
		height = m_image.height;
		depth = m_image.depth;
	}

	int GetWidth()
	{
		return m_image.width;
	}

	int GetHeight()
	{
		return m_image.height;
	}

	int GetDepth()
	{
		return m_image.depth;
	}

	const uint8* GetImageData()
	{
		return m_image.data.begin();
	}

	const char* GetSaveGameName()
	{
		return m_name;
	}

	CPlayerProfileManager::SThumbnail m_image;
	string m_name;
	int m_nRefs;
};


// TODO: currently we don't cache thumbnails.
// every entry in CPlayerProfileManager::TSaveGameInfoVec could store the thumbnail
// or we store ISaveGameThumbailPtr there.
// current access pattern (only one thumbnail at a time) doesn't call for optimization/caching
// but: maybe store also image format, so we don't need to swap RB
class CSaveGameEnumerator : public ISaveGameEnumerator
{
public:
	CSaveGameEnumerator(CPlayerProfileManager::IPlatformImpl* pImpl, CPlayerProfileManager::SUserEntry* pEntry) : m_nRefs(0), m_pImpl(pImpl), m_pEntry(pEntry)
	{
		assert (m_pImpl != 0);
		assert (m_pEntry != 0);
		pImpl->GetSaveGames(m_pEntry, m_saveGameInfoVec);
	}

	int GetCount()
	{
		return m_saveGameInfoVec.size();
	}

	bool GetDescription(int index, SGameDescription& desc)
	{
		if (index < 0 || index >= m_saveGameInfoVec.size())
			return false;

		CPlayerProfileManager::SSaveGameInfo& info = m_saveGameInfoVec[index];
		info.CopyTo(desc);
		return true;
	}

	virtual ISaveGameThumbailPtr GetThumbnail(int index)
	{
		if (index >= 0 && index < m_saveGameInfoVec.size())
		{
			ISaveGameThumbailPtr pThumbnail = new CSaveGameThumbnail(m_saveGameInfoVec[index].name);
			CSaveGameThumbnail* pCThumbnail = static_cast<CSaveGameThumbnail*> (pThumbnail.get());
			if (m_pImpl->GetSaveGameThumbnail(m_pEntry, m_saveGameInfoVec[index].name, pCThumbnail->m_image))
			{
				return pThumbnail;
			}
		}
		return 0;
	}

	virtual ISaveGameThumbailPtr GetThumbnail(const char* saveGameName)
	{
		CPlayerProfileManager::TSaveGameInfoVec::const_iterator iter = m_saveGameInfoVec.begin();
		CPlayerProfileManager::TSaveGameInfoVec::const_iterator iterEnd = m_saveGameInfoVec.end();
		while (iter != iterEnd)
		{
			if (iter->name.compareNoCase(saveGameName) == 0)
			{
				ISaveGameThumbailPtr pThumbnail = new CSaveGameThumbnail(iter->name);
				CSaveGameThumbnail* pCThumbnail = static_cast<CSaveGameThumbnail*> (pThumbnail.get());
				if (m_pImpl->GetSaveGameThumbnail(m_pEntry, pCThumbnail->m_name, pCThumbnail->m_image))
				{
					return pThumbnail;
				}
				return 0;
			}
			++iter;
		}
		return 0;
	}

	void AddRef()
	{
		++m_nRefs;
	}

	void Release()
	{
		if (0 == --m_nRefs) 
			delete this; 
	}

private:
	int m_nRefs;
	CPlayerProfileManager::IPlatformImpl* m_pImpl;
	CPlayerProfileManager::SUserEntry* m_pEntry;
	CPlayerProfileManager::TSaveGameInfoVec m_saveGameInfoVec;
};

//------------------------------------------------------------------------
ISaveGameEnumeratorPtr CPlayerProfileManager::CreateSaveGameEnumerator(const char* userId, CPlayerProfile* pProfile)
{
	if (!m_bInitialized)
		return 0;

	SUserEntry* pEntry = FindEntry(userId);
	if (pEntry == 0) // no such user
	{
		GameWarning("[PlayerProfiles] CreateSaveGameEnumerator: User '%s' not logged in", userId);
		return 0;
	}

	if (pEntry->pCurrentProfile == 0 || pEntry->pCurrentProfile != pProfile)
		return 0;


	return new CSaveGameEnumerator(m_pImpl, pEntry);
}


//------------------------------------------------------------------------
ISaveGame* CPlayerProfileManager::CreateSaveGame(const char* userId, CPlayerProfile* pProfile)
{
	if (!m_bInitialized)
		return 0;

	SUserEntry* pEntry = FindEntry(userId);
	if (pEntry == 0) // no such user
	{
		GameWarning("[PlayerProfiles] CreateSaveGame: User '%s' not logged in", userId);
		return 0;
	}

	if (pEntry->pCurrentProfile == 0 || pEntry->pCurrentProfile != pProfile)
		return 0;

	return m_pImpl->CreateSaveGame(pEntry);
}

//------------------------------------------------------------------------
ILoadGame* CPlayerProfileManager::CreateLoadGame(const char* userId, CPlayerProfile* pProfile)
{
	if (!m_bInitialized)
		return 0;

	SUserEntry* pEntry = FindEntry(userId);
	if (pEntry == 0) // no such user
	{
		GameWarning("[PlayerProfiles] CreateLoadGame: User '%s' not logged in", userId);
		return 0;
	}

	if (pEntry->pCurrentProfile == 0 || pEntry->pCurrentProfile != pProfile)
		return 0;

	return m_pImpl->CreateLoadGame(pEntry);
}

//------------------------------------------------------------------------
bool CPlayerProfileManager::DeleteSaveGame(const char* userId, CPlayerProfile* pProfile, const char* name)
{
	if (!m_bInitialized)
		return false;

	SUserEntry* pEntry = FindEntry(userId);
	if (pEntry == 0) // no such user
	{
		GameWarning("[PlayerProfiles] DeleteSaveGame: User '%s' not logged in", userId);
		return false;
	}

	if (pEntry->pCurrentProfile == 0 || pEntry->pCurrentProfile != pProfile)
		return false;

	return m_pImpl->DeleteSaveGame(pEntry, name);
}

//------------------------------------------------------------------------
ILevelRotationFile* CPlayerProfileManager::GetLevelRotationFile(const char* userId, CPlayerProfile* pProfile, const char* name)
{
  if( !m_bInitialized )
    return 0;

  SUserEntry* pEntry = FindEntry(userId);
  if (pEntry == 0) // no such user
  {
    GameWarning("[PlayerProfiles] GetLevelRotationFile: User '%s' not logged in", userId);
    return false;
  }

  if (pEntry->pCurrentProfile == 0 || pEntry->pCurrentProfile != pProfile)
    return false;

  return m_pImpl->GetLevelRotationFile(pEntry, name);
}

void CPlayerProfileManager::SetSharedSaveGameFolder(const char* sharedSaveGameFolder)
{
	m_sharedSaveGameFolder = sharedSaveGameFolder;
	m_sharedSaveGameFolder.TrimRight("/\\");
}

// FIXME: need something in crysystem or crypak to move files or directories
#if defined(WIN32) || defined(WIN64)
#include "windows.h"
bool CPlayerProfileManager::MoveFileHelper(const char* existingFileName, const char* newFileName)
{
	char oldPath[ICryPak::g_nMaxPath];
	char newPath[ICryPak::g_nMaxPath];
	// need to adjust aliases and paths (use FLAGS_FOR_WRITING)
	gEnv->pCryPak->AdjustFileName(existingFileName, oldPath, ICryPak::FLAGS_FOR_WRITING);
	gEnv->pCryPak->AdjustFileName(newFileName, newPath, ICryPak::FLAGS_FOR_WRITING);
	return ::MoveFile(oldPath, newPath) != 0;
}
#else
// on all other platforms, just a warning
bool CPlayerProfileManager::MoveFileHelper(const char* existingFileName, const char* newFileName)
{
	char oldPath[ICryPak::g_nMaxPath];
	gEnv->pCryPak->AdjustFileName(existingFileName, oldPath, ICryPak::FLAGS_FOR_WRITING);
	string msg;
	msg.Format("CPlayerProfileManager::MoveFileHelper for this Platform not implemented yet.\nOriginal '%s' will be lost!", oldPath);
	CRY_ASSERT_MESSAGE(0, msg.c_str());
	GameWarning(msg.c_str());
	return false;
}
#endif

void CPlayerProfileManager::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_SUBCOMPONENT_NAME(s, "PlayerProfiles");
	s->Add(*this);
	m_pDefaultProfile->GetMemoryStatistics(s);
	s->AddContainer(m_userVec);
	m_pImpl->GetMemoryStatistics(s);
	s->Add(m_curUserID);
}
