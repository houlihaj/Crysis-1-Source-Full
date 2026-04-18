#ifndef __ACTIONGAME_H__
#define __ACTIONGAME_H__

#pragma once

#define MAX_CACHED_EFFECTS	8

#include "IGameRulesSystem.h"
#include "IMaterialEffects.h"

struct SGameStartParams;
struct SGameContextParams;
struct INetNub;
class CGameClientNub;
class CGameServerNub;
class CGameContext;
class CGameClientChannel;
class CGameServerChannel;
struct IGameObject;
struct INetContext;
struct INetwork;
struct IActor;
struct IGameTokenSystem;
struct IScriptTable;
class CScriptRMI;
class CGameStats;

//////////////////////////////////////////////////////////////////////////
struct SBrokenObjRec
{
	int itype;
	IRenderNode *pBrush;
	EntityId idEnt;
	int islot;
	IStatObj *pStatObjOrg;
	float mass;
};

//////////////////////////////////////////////////////////////////////////
struct SBreakEvent
{
	void Serialize( TSerialize ser )
	{
		ser.Value("type", itype);
		ser.Value("idEnt", idEnt);
		ser.Value("objPos", objPos);
		ser.Value("objCenter", objCenter);
		ser.Value("objVolume", objVolume);
		ser.Value("pos", pos);
		ser.Value("rot", rot);
		ser.Value("scale", scale);
		ser.Value("pt", pt);
		ser.Value("n", n);
		ser.Value("vloc0", vloc[0]); ser.Value("vloc1", vloc[1]);
		ser.Value("mass0", mass[0]); ser.Value("mass1", mass[1]);
		ser.Value("partid0", partid[0]); ser.Value("partid1", partid[1]);
		ser.Value("idmat0", idmat[0]); ser.Value("idmat1", idmat[1]);
		ser.Value("penetration", penetration);
		ser.Value("idxChunkId0", idxChunkId0);
		ser.Value("nChunks", nChunks);
		ser.Value("seed", seed);
		ser.Value("energy", energy);
		ser.Value("radius", radius);
	}

	int itype;
	EntityId idEnt;
	Vec3 objPos,objCenter;
	float objVolume;

	Vec3 pos;
	Quat rot;
	Vec3 scale;

	Vec3 pt;
	Vec3 n;
	Vec3 vloc[2];
	float mass[2];
	int partid[2];
	int idmat[2];
	float penetration;
	float energy;
	float radius;

	int idxChunkId0;
	int nChunks;
	int seed;
};

struct SBrokenEntPart
{
	void Serialize( TSerialize ser )
	{
		ser.Value("idSrcEnt", idSrcEnt);
		ser.Value("idNewEnt", idNewEnt);
	}
	EntityId idSrcEnt;
	EntityId idNewEnt;
};

struct SBrokenVegPart
{
	void Serialize( TSerialize ser )
	{
		ser.Value("pos", pos);
		ser.Value("volume", volume);
		ser.Value("idNewEnt", idNewEnt);
	}
	Vec3 pos;
	float volume;
	EntityId idNewEnt;
};

struct SEntityCollHist 
{
	SEntityCollHist *pnext;
	float velImpact,velSlide2,velRoll2;
	int imatImpact[2],imatSlide[2],imatRoll[2];
	float timeRolling,timeNotRolling;
	float rollTimeout,slideTimeout;
	float mass;
};

struct SEntityHits
{
	SEntityHits *pnext;
	Vec3 *pHits,hit0;
	int *pnHits,nhits0;
	int nHits,nHitsAlloc;
	float hitRadius;
	int hitpoints;
	int maxdmg;
	int nMaxHits;
	float timeUsed,lifeTime;
};

struct STreeBreakInst;
struct STreePieceThunk {
	IPhysicalEntity *pPhysEntNew;
	STreePieceThunk *pNext;
	STreeBreakInst *pParent;
};

struct STreeBreakInst {
	IPhysicalEntity *pPhysEntSrc;
	IPhysicalEntity *pPhysEntNew0;
	STreePieceThunk *pNextPiece;
	STreeBreakInst *pThis;
	STreeBreakInst *pNext;
	IStatObj *pStatObj;
	float cutHeight,cutSize;
};

//////////////////////////////////////////////////////////////////////////
class CActionGame	: public IHitListener, public _reference_target_t
{
public:
	CActionGame( CScriptRMI * );
	~CActionGame();

	bool Init( const SGameStartParams * );
	bool ChangeGameContext( const SGameContextParams * );
	bool BlockingSpawnPlayer();

	bool IsServer() const { return m_pServerNub != 0; }
	bool IsClient() const { return m_pClientNub != 0; }
	CGameServerNub * GetGameServerNub() { return m_pGameServerNub; }
	CGameClientNub * GetGameClientNub() { return m_pGameClientNub; }
	CGameContext * GetGameContext() { return m_pGameContext; }
	IActor * GetClientActor();
	bool ControlsEntity(EntityId);

	// returns true if should be let go
	bool Update();

	void AddGlobalPhysicsCallback(int event, void (*)(const EventPhys*, void*), void *);
	void RemoveGlobalPhysicsCallback(int event, void (*)(const EventPhys*, void*), void *);

	void FixBrokenObjects();
	void Serialize( TSerialize ser );
	void SerializeBreakableObjects( TSerialize ser );
	void FlushBreakableObjects();
	void ClearBreakHistory();

	void InitImmersiveness();
	void UpdateImmersiveness();

	void OnEditorSetGameMode( bool bGameMode );

	INetNub * GetServerNetNub() { return m_pServerNub; }

	void DumpStats();

	void GetMemoryStatistics(ICrySizer * s);

	static CActionGame * Get() { return s_this; }

	static void RegisterCVars();

	// helper functions
	static IGameObject *GetEntityGameObject(IEntity *pEntity);
	static IGameObject *GetPhysicalEntityGameObject(IPhysicalEntity *pPhysEntity);

	static void PerformPlaneBreak( const EventPhysCollision& epc, SBreakEvent * pRecordedEvent );

private:
	static IEntity* GetEntity( int i, void * p )
	{
		if (i == PHYS_FOREIGN_ID_ENTITY)
			return (IEntity*)p;
		return NULL;
	}

	void EnablePhysicsEvents(bool enable);

  void CreateGameStats();
  void ReleaseGameStats();

	static int OnBBoxOverlap(const EventPhys * pEvent);
	static int OnCollisionLogged( const EventPhys * pEvent );
	static int OnPostStepLogged( const EventPhys * pEvent );
	static int OnStateChangeLogged( const EventPhys * pEvent );
	static int OnCreatePhysicalEntityLogged( const EventPhys * pEvent );
	static int OnUpdateMeshLogged( const EventPhys * pEvent );
	static int OnRemovePhysicalEntityPartsLogged( const EventPhys * pEvent );

	static int OnCollisionImmediate( const EventPhys * pEvent );
	static int OnPostStepImmediate( const EventPhys * pEvent );
	static int OnStateChangeImmediate( const EventPhys * pEvent );
	static int OnCreatePhysicalEntityImmediate( const EventPhys * pEvent );
	static int OnUpdateMeshImmediate( const EventPhys * pEvent );

	virtual void OnHit(const HitInfo&) {}
	virtual void OnExplosion(const ExplosionInfo&);
	virtual void OnServerExplosion(const ExplosionInfo&){}

	static void OnCollisionLogged_MaterialFX( const EventPhys * pEvent );
	static void OnCollisionLogged_Breakable( const EventPhys * pEvent );
	static void OnCollisionLogged_NotifyAI( const EventPhys * pEvent );
	static void OnPostStepLogged_MaterialFX( const EventPhys * pEvent );
	static void OnStateChangeLogged_MaterialFX( const EventPhys * pEvent );

	bool ProcessHitpoints(const Vec3 &pt, IPhysicalEntity *pent,int partid, ISurfaceType *pMat, int iDamage=1);
	SBreakEvent &RegisterBreakEvent(const EventPhysCollision *pColl, float energy);
	int ReuseBrokenTrees( const EventPhysCollision *pCEvent, float size, int flags );
	EntityId UpdateEntityIdForBrokenPart(EntityId idSrc);
	EntityId UpdateEntityIdForVegetationBreak(IRenderNode *pVeg);
	void RegisterEntsForBreakageReuse(IPhysicalEntity *pPhysEnt,int partid, IPhysicalEntity *pPhysEntNew, float h,float size);
	void RemoveEntFromBreakageReuse(IPhysicalEntity *pEntity, int bRemoveOnlyIfSecondary);
	void ClearTreeBreakageReuseLog();

	bool ConditionHavePlayer( CGameClientChannel * );
	bool ConditionHaveConnection( CGameClientChannel * );
	bool ConditionInGame( CGameClientChannel * );
	typedef bool (CActionGame::*BlockingConditionFunction)(CGameClientChannel*);
	bool BlockingConnect( BlockingConditionFunction, bool requireClientChannel );

	void CallOnEditorSetGameMode( IEntity *pEntity, bool bGameMode );

	bool IsStale();

	void AddProtectedPath( const char * root );

	enum EProceduralBreakType
	{
		ePBT_Normal = 0x10,
		ePBT_Glass = 0x01,
	};
	enum EProceduralBreakFlags
	{
		ePBF_ObeyCVar = 0x01,
		ePBF_AllowGlass = 0x02,
		ePBF_DefaultAllow = 0x08,
	};
	uint8 m_proceduralBreakFlags;
	bool AllowProceduralBreaking( uint8 proceduralBreakType );

	static CActionGame *s_this;

	IEntitySystem     *m_pEntitySystem;
	INetwork          *m_pNetwork;
	INetNub						*m_pClientNub;
	INetNub						*m_pServerNub;
	CGameClientNub    *m_pGameClientNub;
	CGameServerNub    *m_pGameServerNub;
	CGameContext			*m_pGameContext;
	INetContext				*m_pNetContext;
	IGameTokenSystem  *m_pGameTokenSystem;
	IPhysicalWorld    *m_pPhysicalWorld;
	IMaterialEffects  *m_pMaterialEffects;

	typedef std::pair<void (*)(const EventPhys*, void*), void*>	TGlobalPhysicsCallback;
	typedef std::set<TGlobalPhysicsCallback>										TGlobalPhysicsCallbackSet;
	struct SPhysCallbacks
	{
		TGlobalPhysicsCallbackSet	collision[2];
		TGlobalPhysicsCallbackSet	postStep[2];
		TGlobalPhysicsCallbackSet	stateChange[2];
		TGlobalPhysicsCallbackSet	createEntityPart[2];
		TGlobalPhysicsCallbackSet updateMesh[2];

	} m_globalPhysicsCallbacks;
	
	std::vector<SBrokenObjRec> m_brokenObjs;
	std::vector<SBreakEvent> m_breakEvents;
	std::vector<SBrokenEntPart> m_brokenEntParts;
	std::vector<SBrokenVegPart> m_brokenVegParts;
	std::vector<EntityId> m_broken2dChunkIds;
	std::map<EntityId,int> m_entPieceIdx;
	std::map<IPhysicalEntity*,STreeBreakInst*> m_mapBrokenTreesByPhysEnt;
	std::map<IStatObj*,STreeBreakInst*> m_mapBrokenTreesByCGF;
	std::map<IPhysicalEntity*,STreePieceThunk*> m_mapBrokenTreesChunks;
	bool m_bLoading; 
	int m_iCurBreakEvent;

  CGameStats *m_pGameStats;

	SEntityCollHist *m_pCHSlotPool,*m_pFreeCHSlot0;
	std::map<int,SEntityCollHist*> m_mapECH;

	SEntityHits *m_pEntHits0;
	std::map<int,SEntityHits*> m_mapEntHits;
	typedef struct SVegCollisionStatus 
	{
		IRenderNode *rn;
		IStatObj *pStat;
		int branchNum;
		SVegCollisionStatus()
		{
			branchNum = -1;
			rn = 0;
			pStat = 0;
		}
	} SVegCollisionStatus;
	std::map< EntityId, Vec3 > m_vegStatus;
	std::map< EntityId, SVegCollisionStatus* > m_treeStatus;
	std::map< EntityId, SVegCollisionStatus* > m_treeBreakStatus;

	int		m_nEffectCounter;
	SMFXRunTimeEffectParams	m_lstCachedEffects[MAX_CACHED_EFFECTS];
	static int g_procedural_breaking;
	static int g_joint_breaking;
	static float g_tree_cut_reuse_dist;
	static int s_waterMaterialId;
};

#endif
