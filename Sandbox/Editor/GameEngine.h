////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   gameengine.h
//  Version:     v1.00
//  Created:     13/5/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __gameengine_h__
#define __gameengine_h__

#if _MSC_VER > 1000
#pragma once
#endif


struct IEditorGame;
class CStartupLogoDialog;
struct IFlowSystem;
struct IGameTokenSystem;
struct IEquipmentSystemInterface;
struct IInitializeUIInfo;

/** This class serves as a high-level wrapper for CryEngine game.
*/
class CGameEngine : public IEditorNotifyListener
{
public:
	CGameEngine();
	~CGameEngine(void);

	/** Initialize System.
			@return true if Initializion succeded, false overwise.
	*/
	bool Init( bool bPreviewMode,bool bTestMode,const char *sCmdLine,IInitializeUIInfo *logo );

	/** Initialize game.
			@return true if Initializion succeded, false overwise.
	*/
	bool InitGame( const char *sGameDLL );

	// Inititialize Terrain.
	bool InitTerrain();
	
	/** Load new terrain level into 3d engine.
			Also load AI triangulation for this level.
	*/
	bool LoadLevel( const CString &levelPath,const CString &mission,bool bDeleteAIGraph,bool bReleaseResources,bool bInitTerrain );

	/** Reload level if it was already loaded.
	*/
	bool ReloadLevel();

	/** Loads AI triangulation.
	*/
	bool LoadAI( const CString &levelName,const CString &missionName );

	/** Load new mission.
	*/
	bool LoadMission( const CString &mission );
	
	/** Reload environment settings in currently loaded level.
	*/
	bool ReloadEnvironment();

	/** Switch In/Out of game mode.
			@param inGame When true editor switch to game mode.
	*/
	void SetGameMode( bool inGame );

	/** Switch In/Out of AI and Physics simulation mode.
			@param enabled When true editor switch to simulation mode.
	*/
	void SetSimulationMode( bool enabled,bool bOnlyPhysics=false );

	/** Get current simulation mode.
	*/
	bool GetSimulationMode() const { return m_simulationMode; };

	/** Returns true if level is loaded.
	*/
	bool IsLevelLoaded() const { return m_bLevelLoaded; };

	/** Assign new level path name.
	*/
	void SetLevelPath( const CString &path );

	/** Assign new current mission name.
	*/
	void SetMissionName( const CString &mission );

	/** Return name of currently loaded level.
	*/
	const CString& GetLevelName() const { return m_levelName; };
	
	/** Return name of currently active mission.
	*/
	const CString& GetMissionName() const { return m_missionName; };

	/** Get fully specified level path.
	*/
	const CString& GetLevelPath() const { return m_levelPath; };

	/** Query if engine is in game mode.
	*/
	bool IsInGameMode() const { return m_inGameMode; };

	/** Force level loaded variable to true.
	*/
	void SetLevelLoaded( bool bLoaded ) { m_bLevelLoaded = bLoaded; }

	/** Generate All AI navigation data
	*/
	void GenerateAiAllNavigation();

	/** Generate AI Triangulation of currently loaded data.
	*/
	void GenerateAiTriangulation();

	/** Generate AI Waypoint of currently loaded data.
	*/
	void GenerateAiWaypoint();

	/** Generate AI Flight navigation of currently loaded data.
	*/
	void GenerateAiFlightNavigation();

	/** Generate AI 3D nav data of currently loaded data.
	*/
	void GenerateAiNavVolumes();

	/** Loading all the AI navigation data of current level.
	*/
	void LoadAINavigationData();

	/** Does some basic sanity checking of the AI navigation
	*/
	void ValidateAINavigation();

	/** Generates 3D volume voxels only for debugging
	*/
  void Generate3DDebugVoxels();

	/** Forces all entities to be reregistered in sectors again.
	*/
	void ForceRegisterEntitiesInSectors();

	/** Put surface types from Editor to game.
	*/
	void ReloadSurfaceTypes(bool bUpdateEngineTerrain = true);

	/** Sets equipment pack current used by player.
	*/
	void SetPlayerEquipPack( const char *sEqipPackName );

	/** Called when Game resource file must be reloaded.
	*/
	void ReloadResourceFile( const CString &filename );

	/** Query ISystem interface.
	*/
	ISystem* GetSystem() { return m_ISystem; };

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	//! Set player position in game.
	//! @param bEyePos If set then givven position is position of player eyes.
	void SetPlayerViewMatrix( const Matrix34 &tm,bool bEyePos=true );
	
	//! When set, player in game will be every frame synchronized with editor camera.
	void SyncPlayerPosition( bool bEnable );
	bool IsSyncPlayerPosition() const { return m_syncPlayerPosition; };

	void HideLocalPlayer( bool bHide );

	//////////////////////////////////////////////////////////////////////////
	// Game MOD support.
	//////////////////////////////////////////////////////////////////////////
	//! Set game's current Mod name.
	void SetCurrentMOD( const char *sMod );
	//! Returns game's current Mod name.
	const char* GetCurrentMOD() const;

	//! Called every frame.
	void Update();

	virtual void OnEditorNotifyEvent( EEditorNotifyEvent event );

	struct IEntity* GetPlayerEntity();

	// Retrieve pointer to the game flow system implementation.
	IFlowSystem* GetIFlowSystem() const;
	IGameTokenSystem* GetIGameTokenSystem() const;
	IEquipmentSystemInterface* GetIEquipmentSystemInterface() const;

	//! When enabled flow system will be updated at editing mode.
	void EnableFlowSystemUpdate( bool bEnable ) { m_bUpdateFlowSystem = bEnable; };
	bool IsFlowSystemUpdateEnabled() const { return m_bUpdateFlowSystem; }

	void LockResources();
	void UnlockResources();

	//////////////////////////////////////////////////////////////////////////
private:
	/** Completly Reset Game and Editor rendering resources for next level.
	*/
	void ResetResources();
  
private:
	CLogFile m_logFile;

	CString m_levelName;
	CString m_missionName;
	CString m_levelPath;
	CString m_MOD;

	bool m_bLevelLoaded;
	bool m_inGameMode;
	bool m_simulationMode;
	bool m_simulationModeAI;
	bool m_syncPlayerPosition;
	bool m_bUpdateFlowSystem;

	ISystem *m_ISystem;
	I3DEngine *m_I3DEngine;
	IAISystem *m_IAISystem;
	IEntitySystem *m_IEntitySystem;

	Matrix34 m_playerViewTM;

	struct SSystemUserCallback* m_pSystemUserCallback;

	HMODULE m_hSystemHandle;

	HMODULE			m_gameDll;
	IEditorGame *m_pEditorGame;
	class CGameToEditorInterface *m_pGameToEditorInterface;
};


#endif // __gameengine_h__
