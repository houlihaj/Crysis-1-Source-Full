#ifndef __PLAYERPROFILEMANAGER_H__
#define __PLAYERPROFILEMANAGER_H__

#if _MSC_VER > 1000
#	pragma once
#endif

#include <IPlayerProfiles.h>

class CPlayerProfile;
struct ISaveGame;
struct ILoadGame;

class CPlayerProfileManager : public IPlayerProfileManager
{
public:
	struct IPlatformImpl;

	// profile description
	struct SLocalProfileInfo
	{
		SLocalProfileInfo() {}
		SLocalProfileInfo(const string& name )
		{
			SetName(name);
		}
		SLocalProfileInfo(const char* name)
		{
			SetName(name);
		}

		void SetName(const char* name)
		{
			m_name = name;
		}

		void SetName(const string& name)
		{
			m_name = name;
		}

		const string& GetName() const
		{
			return m_name;
		}

		int compare(const char* name) const
		{
			return m_name.compareNoCase(name);
		}

	private:
		string m_name; // name of the profile
	};

	// per user data
	struct SUserEntry
	{
		SUserEntry(const string& inUserId) : userId(inUserId), pCurrentProfile(0), pCurrentPreviewProfile(0) {}
		string userId;
		CPlayerProfile* pCurrentProfile;
		CPlayerProfile* pCurrentPreviewProfile;
		std::vector<SLocalProfileInfo> profileDesc;
	};
	typedef std::vector<SUserEntry*> TUserVec;

	struct SSaveGameMetaData
	{
		SSaveGameMetaData() {
			levelName = gameRules = buildVersion = "<undefined>";
			saveTime = 0;
			fileVersion = -1;
		}
		void CopyTo(ISaveGameEnumerator::SGameMetaData& data)
		{
			data.levelName = levelName;
			data.gameRules = gameRules;
			data.fileVersion = fileVersion;
			data.buildVersion = buildVersion;
			data.saveTime = saveTime;
			data.xmlMetaDataNode = xmlMetaDataNode;
		}
		string levelName;
		string gameRules;
		int    fileVersion;
		string buildVersion;
		time_t saveTime;
		XmlNodeRef xmlMetaDataNode;
	};

	struct SThumbnail
	{
		SThumbnail() : width(0), height(0), depth(0) {}
		DynArray<uint8> data;
		int   width;
		int   height;
		int   depth;
		bool IsValid() const { return data.size() && width && height && depth; }
		void ReleaseData()
		{
			data.clear();
			width = height = depth = 0;
		}
	};

	struct SSaveGameInfo
	{
		string name;
		string humanName;
		string description;
		SSaveGameMetaData metaData;
		SThumbnail thumbnail;
		void CopyTo(ISaveGameEnumerator::SGameDescription& desc)
		{
			desc.name = name;
			desc.humanName = humanName;
			desc.description = description;
			metaData.CopyTo(desc.metaData);
		}
	};
	typedef std::vector<SSaveGameInfo> TSaveGameInfoVec;


	// members

	CPlayerProfileManager(CPlayerProfileManager::IPlatformImpl* pImpl); // CPlayerProfileManager takes ownership of IPlatformImpl*
	virtual ~CPlayerProfileManager();

	// IPlayerProfileManager
	virtual bool Initialize();
	virtual bool Shutdown();
	virtual int  GetUserCount();
	virtual bool GetUserInfo(int index, IPlayerProfileManager::SUserInfo& outInfo);
	virtual const char* GetCurrentUser();
	virtual bool LoginUser(const char* userId, bool& bOutFirstTime);
	virtual bool LogoutUser(const char* userId);
	virtual int  GetProfileCount(const char* userId);
	virtual bool GetProfileInfo(const char* userId, int index, IPlayerProfileManager::SProfileDescription& outInfo);
	virtual bool CreateProfile(const char* userId, const char* profileName, bool bOverrideIfPresent, IPlayerProfileManager::EProfileOperationResult& result);
	virtual bool DeleteProfile(const char* userId, const char* profileName, IPlayerProfileManager::EProfileOperationResult& result);
	virtual bool RenameProfile(const char* userId, const char* newName, IPlayerProfileManager::EProfileOperationResult& result);
	virtual bool SaveProfile(const char* userId, IPlayerProfileManager::EProfileOperationResult& result);
	virtual IPlayerProfile* ActivateProfile(const char* userId, const char* profileName);
	virtual IPlayerProfile* GetCurrentProfile(const char* userId);
	virtual bool ResetProfile(const char* userId);
	virtual IPlayerProfile* GetDefaultProfile();
	virtual const IPlayerProfile* PreviewProfile(const char* userId, const char* profileName);
	virtual void SetSharedSaveGameFolder(const char* sharedSaveGameFolder); 
	virtual const char* GetSharedSaveGameFolder()
	{
		return m_sharedSaveGameFolder.c_str();
	}
	virtual void GetMemoryStatistics(ICrySizer * s);
	// ~IPlayerProfileManager

	// maybe move to IPlayerProfileManager i/f
	virtual ISaveGameEnumeratorPtr CreateSaveGameEnumerator(const char* userId, CPlayerProfile* pProfile);
	virtual ISaveGame* CreateSaveGame(const char* userId, CPlayerProfile* pProfile);
	virtual ILoadGame* CreateLoadGame(const char* userId, CPlayerProfile* pProfile);
	virtual bool DeleteSaveGame(const char* userId, CPlayerProfile* pProfile, const char* name);

	virtual ILevelRotationFile* GetLevelRotationFile(const char* userId, CPlayerProfile* pProfile, const char* name);

	const string& GetSharedSaveGameFolder() const 
	{
		return m_sharedSaveGameFolder;
	}

	bool IsSaveGameFolderShared() const
	{
		return m_sharedSaveGameFolder.empty() == false;
	}

	// helper to move file or directory -> should eventually go into CrySystem/CryPak
	// only implemented for WIN32/WIN64
	bool MoveFileHelper(const char* existingFileName, const char* newFileName);

public:
	struct IProfileXMLSerializer
	{
		virtual bool IsLoading() = 0;
		virtual XmlNodeRef AddSection(const char* name) = 0;
		virtual XmlNodeRef GetSection(const char* name) = 0;
	};

	struct IPlatformImpl
	{
		typedef CPlayerProfileManager::SUserEntry SUserEntry;
		typedef CPlayerProfileManager::SLocalProfileInfo SLocalProfileInfo;
		typedef CPlayerProfileManager::SThumbnail SThumbnail;
		virtual bool Initialize(CPlayerProfileManager* pMgr) = 0;
		virtual void Release() = 0; // must delete itself
		virtual bool LoginUser(SUserEntry* pEntry) = 0;
		virtual bool LogoutUser(SUserEntry* pEntry) = 0;
		virtual bool SaveProfile(SUserEntry* pEntry, CPlayerProfile* pProfile, const char* name) = 0;
		virtual bool LoadProfile(SUserEntry* pEntry, CPlayerProfile* pProfile, const char* name) = 0;
		virtual bool DeleteProfile(SUserEntry* pEntry, const char* name) = 0;
		virtual bool RenameProfile(SUserEntry* pEntry, const char* newName) = 0;
		virtual bool GetSaveGames(SUserEntry* pEntry, TSaveGameInfoVec& outVec, const char* altProfileName="") = 0; // if altProfileName == "", uses current profile of SUserEntry
		virtual ISaveGame* CreateSaveGame(SUserEntry* pEntry) = 0;
		virtual ILoadGame* CreateLoadGame(SUserEntry* pEntry) = 0;
		virtual bool DeleteSaveGame(SUserEntry* pEntry, const char* name) = 0;
    virtual ILevelRotationFile* GetLevelRotationFile(SUserEntry* pEntry, const char* name) = 0;
		virtual bool GetSaveGameThumbnail(SUserEntry* pEntry, const char* saveGameName, SThumbnail& thumbnail) = 0;
		virtual void GetMemoryStatistics(ICrySizer * s) = 0;
	};

	SUserEntry* FindEntry(const char* userId) const;

protected:

	SUserEntry* FindEntry(const char* userId, TUserVec::iterator& outIter);

	SLocalProfileInfo* GetLocalProfileInfo(SUserEntry* pEntry, const char* profileName) const;
	SLocalProfileInfo* GetLocalProfileInfo(SUserEntry* pEntry, const char* profileName, std::vector<SLocalProfileInfo>::iterator& outIter) const;

public:
	static int sUseRichSaveGames;
	static int sRSFDebugWrite;
	static int sRSFDebugWriteOnLoad;

protected:
	bool m_bInitialized;
	CPlayerProfile* m_pDefaultProfile;
	TUserVec m_userVec;
	IPlatformImpl* m_pImpl;
	string m_curUserID;
	string m_sharedSaveGameFolder;
};


#endif
