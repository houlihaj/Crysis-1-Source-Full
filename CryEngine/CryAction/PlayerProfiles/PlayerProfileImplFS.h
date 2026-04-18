#ifndef __PLAYERPROFILEIMPLFS_H__
#define __PLAYERPROFILEIMPLFS_H__

#if _MSC_VER > 1000
#	pragma once
#endif

#include "PlayerProfileManager.h"

struct ICommonProfileImpl
{
	virtual void InternalMakeFSPath(CPlayerProfileManager::SUserEntry* pEntry, const char* profileName, string& outPath) = 0;
	virtual void InternalMakeFSSaveGamePath(CPlayerProfileManager::SUserEntry* pEntry, const char* profileName, string& outPath, bool bNeedFolder) = 0;
	virtual CPlayerProfileManager* GetManager() = 0;
};

#if 0
template<class T>
class CCommonSaveGameHelper
{
public:
	CCommonSaveGameHelper(T* pImpl) : m_pImpl(pImpl) {}
	bool GetSaveGames(CPlayerProfileManager::SUserEntry* pEntry, CPlayerProfileManager::TSaveGameInfoVec& outVec, const char* altProfileName);
	ISaveGame* CreateSaveGame(CPlayerProfileManager::SUserEntry* pEntry);
	ILoadGame* CreateLoadGame(CPlayerProfileManager::SUserEntry* pEntry);

protected:
	bool FetchMetaData(XmlNodeRef& root, CPlayerProfileManager::SSaveGameMetaData& metaData);
	T* m_pImpl;
};
#endif


class CPlayerProfileImplFS : public CPlayerProfileManager::IPlatformImpl, public ICommonProfileImpl
{
public:
	CPlayerProfileImplFS();

	// CPlayerProfileManager::IPlatformImpl
	virtual bool Initialize(CPlayerProfileManager* pMgr);
	virtual void Release() { delete this; }
	virtual bool LoginUser(SUserEntry* pEntry);
	virtual bool LogoutUser(SUserEntry* pEntry);
	virtual bool SaveProfile(SUserEntry* pEntry, CPlayerProfile* pProfile, const char* name);
	virtual bool LoadProfile(SUserEntry* pEntry, CPlayerProfile* pProfile, const char* name);
	virtual bool DeleteProfile(SUserEntry* pEntry, const char* name);
	virtual bool RenameProfile(SUserEntry* pEntry, const char* newName);
	virtual bool GetSaveGames(SUserEntry* pEntry, CPlayerProfileManager::TSaveGameInfoVec& outVec, const char* altProfileName);
	virtual ISaveGame* CreateSaveGame(SUserEntry* pEntry);
	virtual ILoadGame* CreateLoadGame(SUserEntry* pEntry);
	virtual bool DeleteSaveGame(SUserEntry* pEntry, const char* name);
	virtual bool GetSaveGameThumbnail(SUserEntry* pEntry, const char* saveGameName, SThumbnail& thumbnail);
	// ~CPlayerProfileManager::IPlatformImpl

	// ICommonProfileImpl
	void InternalMakeFSPath(SUserEntry* pEntry, const char* profileName, string& outPath);
	void InternalMakeFSSaveGamePath(SUserEntry* pEntry, const char* profileName, string& outPath, bool bNeedFolder);
	virtual CPlayerProfileManager* GetManager() { return m_pMgr; }
	// ~ICommonProfileImpl

protected:
	virtual ~CPlayerProfileImplFS();

private:
	CPlayerProfileManager* m_pMgr;
};

class CPlayerProfileImplFSDir : public CPlayerProfileManager::IPlatformImpl, public ICommonProfileImpl
{
public:
	CPlayerProfileImplFSDir();

	// CPlayerProfileManager::IPlatformImpl
	virtual bool Initialize(CPlayerProfileManager* pMgr);
	virtual void Release() { delete this; }
	virtual bool LoginUser(SUserEntry* pEntry);
	virtual bool LogoutUser(SUserEntry* pEntry);
	virtual bool SaveProfile(SUserEntry* pEntry, CPlayerProfile* pProfile, const char* name);
	virtual bool LoadProfile(SUserEntry* pEntry, CPlayerProfile* pProfile, const char* name);
	virtual bool DeleteProfile(SUserEntry* pEntry, const char* name);
	virtual bool RenameProfile(SUserEntry* pEntry, const char* newName);
	virtual bool GetSaveGames(SUserEntry* pEntry, CPlayerProfileManager::TSaveGameInfoVec& outVec, const char* altProfileName);
	virtual ISaveGame* CreateSaveGame(SUserEntry* pEntry);
	virtual ILoadGame* CreateLoadGame(SUserEntry* pEntry);
	virtual bool DeleteSaveGame(SUserEntry* pEntry, const char* name);
	virtual ILevelRotationFile* GetLevelRotationFile(SUserEntry* pEntry, const char* name);
	virtual bool GetSaveGameThumbnail(SUserEntry* pEntry, const char* saveGameName, SThumbnail& thumbnail);
	virtual void GetMemoryStatistics(ICrySizer * s);
	// ~CPlayerProfileManager::IPlatformImpl

	// ICommonProfileImpl
	virtual void InternalMakeFSPath(SUserEntry* pEntry, const char* profileName, string& outPath);
	virtual void InternalMakeFSSaveGamePath(SUserEntry* pEntry, const char* profileName, string& outPath, bool bNeedFolder);
	virtual CPlayerProfileManager* GetManager() { return m_pMgr; }
	// ~ICommonProfileImpl

protected:
	virtual ~CPlayerProfileImplFSDir(); 

private:
	CPlayerProfileManager* m_pMgr;
};

#endif
