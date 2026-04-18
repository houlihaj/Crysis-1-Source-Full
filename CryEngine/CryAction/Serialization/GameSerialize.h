#ifndef __GAME_SERIALIZE_H__
#define __GAME_SERIALIZE_H__

#pragma once

#include "ISaveGame.h"
#include "ILoadGame.h"

class CCryAction;
struct IGameFramework;
struct SGameStartParams;
struct ISaveGame;

enum ELoadGameResult
{
	eLGR_Ok,
	eLGR_Failed,
	eLGR_FailedAndDestroyedState
};

class CGameSerialize
{
public:
	typedef ISaveGame * (*SaveGameFactory)();
	typedef ILoadGame * (*LoadGameFactory)();
  
	void RegisterSaveGameFactory( const char * name, SaveGameFactory factory );
	void RegisterLoadGameFactory( const char * name, LoadGameFactory factory );
	void GetMemoryStatistics(ICrySizer * s)
	{
		s->AddContainer(m_saveGameFactories);
		s->AddContainer(m_loadGameFactories);
	}

	bool SaveGame( CCryAction * pCryAction, const char * method, const char * name, ESaveGameReason reason = eSGR_QuickSave, const char* checkPointName = NULL);
	ELoadGameResult LoadGame( CCryAction * pCryAction, const char * method, const char * path, SGameStartParams& params, bool requireQuickLoad, bool loadingSaveGame = false);

	void RegisterFactories( IGameFramework * pFW );

private:
	void Clean();
	void ReserveEntityIds();
	void FlushActivatableGameObjectExtensions();
	bool RepositionEntities( const std::vector<struct SBasicEntityData>& basicEntityData, bool insistOnAllEntitiesBeingThere );
	void DeleteDynamicEntities();
	void DumpEntities();

private:
	std::map<string, SaveGameFactory> m_saveGameFactories;
	std::map<string, LoadGameFactory> m_loadGameFactories;

	std::vector<EntityId> m_savedEntityIds;
};

#endif
