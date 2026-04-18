#include "stdafx.h"
#include "GameSerialize.h"

#include "XmlSaveGame.h"
#include "XmlLoadGame.h"
#include "BinarySaveGame.h"

#include "IGameFramework.h"
#include "IPlayerProfiles.h"

namespace 
{
	ISaveGame* CSaveGameCurrentUser()
	{
		IPlayerProfileManager* pPPMgr = CCryAction::GetCryAction()->GetIPlayerProfileManager();
		if (pPPMgr)
		{
			int userCount = pPPMgr->GetUserCount();
			if (userCount > 0)
			{
				IPlayerProfileManager::SUserInfo info;
				pPPMgr->GetUserInfo(0, info);
				IPlayerProfile* pProfile = pPPMgr->GetCurrentProfile(info.userId);
				if (pProfile)
					return pProfile->CreateSaveGame();
				else
					GameWarning("CSaveGameCurrentUser: User '%s' has no active profile. Returning default XMLSaveGame.", info.userId);
			}
			else
				GameWarning("CSaveGameCurrentUser: No User logged in. Returning default XMLSaveGame.");
		}
		return new CXmlSaveGame;
	}

	ILoadGame* CLoadGameCurrentUser()
	{
		IPlayerProfileManager* pPPMgr = CCryAction::GetCryAction()->GetIPlayerProfileManager();
		if (pPPMgr)
		{
			int userCount = pPPMgr->GetUserCount();
			if (userCount > 0)
			{
				IPlayerProfileManager::SUserInfo info;
				pPPMgr->GetUserInfo(0, info);
				IPlayerProfile* pProfile = pPPMgr->GetCurrentProfile(info.userId);
				if (pProfile)
					return pProfile->CreateLoadGame();
				else
					GameWarning("CLoadGameCurrentUser: User '%s' has no active profile. Returning default XmlSaveGame.", info.userId);
			}
			else
				GameWarning("CLoadGameCurrentUser: No User logged in. Returning default XmlLoadGame.");
		}
		return new CXmlLoadGame;
	}

};

void CGameSerialize::RegisterFactories( IGameFramework * pFW )
{
	// save/load game factories
	REGISTER_FACTORY(pFW, "xml", CXmlSaveGame, false);
	REGISTER_FACTORY(pFW, "xml", CXmlLoadGame, false);
//	REGISTER_FACTORY(pFW, "binary", CXmlSaveGame, false);
//	REGISTER_FACTORY(pFW, "binary", CXmlLoadGame, false);

	pFW->RegisterFactory( "xml", CLoadGameCurrentUser, false);
	pFW->RegisterFactory( "xml", CSaveGameCurrentUser, false);

}
