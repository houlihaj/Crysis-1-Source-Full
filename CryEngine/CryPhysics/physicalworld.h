//////////////////////////////////////////////////////////////////////
//
//	Physical Entity header
//	
//	File: physicalentity.h
//	Description : PhysicalEntity and PhysicalWorld classes declarations
//
//	History:
//	-:Created by Anton Knyazev
//
//////////////////////////////////////////////////////////////////////

#ifndef physicalworld_h
#define physicalworld_h
#pragma once

#include "physicalentity.h"

const int NSURFACETYPES = 512;
const int PLACEHOLDER_CHUNK_SZLG2 = 8;
const int PLACEHOLDER_CHUNK_SZ = 1<<PLACEHOLDER_CHUNK_SZLG2;
const int QUEUE_SLOT_SZ = 8192;

const int PENT_SETPOSED = 1<<16;
const int PENT_QUEUED_BIT = 17;
const int PENT_QUEUED = 1<<PENT_QUEUED_BIT;

class CPhysicalPlaceholder;
class CPhysicalEntity;
class CPhysArea;
struct pe_gridthunk;
enum { pef_step_requested = 0x40000000 };

#define CONTACT_END(pFirstContact) ((entity_contact*)&(pFirstContact))

struct EventClient {
	int (*OnEvent)(const EventPhys*);
	EventClient *next;
	float priority;
};

const int EVENT_CHUNK_SZ = 8192-sizeof(void*);
struct EventChunk {
	EventChunk *next;
};

struct SExplosionShape {
	int id;
	IGeometry *pGeom;
	float size,rsize;
	int idmat;
	float probability;
	int iFirstByMat,nSameMat;
	int bCreateConstraint;
};

struct SRwiRequest {
	void *pForeignData;
	int iForeignData;
	Vec3 org,dir;
	int objtypes;
	unsigned int flags;
	ray_hit *hits;
	int nMaxHits;
  int idSkipEnts[5];
	ray_hit_cached *phitLast;
	int nSkipEnts;
	int iCaller;
};

#define max_def(a,b) (a) + ((b)-(a) & (a)-(b)>>31)
struct SPwiRequest {
	void *pForeignData;
	int iForeignData;
	int itype;
	char pprim[max_def(max_def(max_def(sizeof(box),sizeof(cylinder)),sizeof(capsule)),sizeof(sphere))];
	Vec3 sweepDir;
	int entTypes;
	int geomFlagsAll;
	int geomFlagsAny;
	int idSkipEnts[4];
	int nSkipEnts;
};

struct pe_PODcell {
	int inextActive;
	float lifeTime;
	Vec2 zlim;
};


class CPhysicalWorld : public IPhysicalWorld, public IPhysUtils, public CGeomManager {
public:
	CPhysicalWorld(ILog *pLog);
	~CPhysicalWorld();

	virtual void Init();
	virtual void Shutdown(int bDeleteEntities = 1);
	virtual void Release() { delete this; }
	virtual IGeomManager* GetGeomManager() { return this; }
	virtual IPhysUtils* GetPhysUtils() { return this; }

	virtual void SetupEntityGrid(int axisz, Vec3 org, int nx,int ny, float stepx,float stepy);
	virtual void DeactivateOnDemandGrid();
	virtual void RegisterBBoxInPODGrid(const Vec3 *BBox);
	virtual int AddRefEntInPODGrid(IPhysicalEntity *pent, const Vec3 *BBox=0);
	virtual IPhysicalEntity *SetHeightfieldData(const heightfield *phf,int *pMatMapping=0,int nMats=0);
	virtual IPhysicalEntity *GetHeightfieldData(heightfield *phf);
	virtual int SetSurfaceParameters(int surface_idx, float bounciness,float friction, unsigned int flags=0);
	virtual int GetSurfaceParameters(int surface_idx, float &bounciness,float &friction, unsigned int &flags);
	virtual PhysicsVars *GetPhysVars() { return &m_vars; }

	void GetPODGridCellBBox(int ix,int iy, Vec3 &center,Vec3 &size);
	pe_PODcell *getPODcell(int ix,int iy)	{
		int i, imask = -(inrange(ix,-1,m_entgrid.size.x) & inrange(iy,-1,m_entgrid.size.y));
		i = (ix>>3)*m_PODstride.x + (iy>>3)*m_PODstride.y;
		i = i + ((m_entgrid.size.x*m_entgrid.size.y>>6)-i & ~imask) & -m_bHasPODGrid;
		INT_PTR pmask = -iszero((INT_PTR)m_pPODCells[i]);
		pe_PODcell *pcell0 = (pe_PODcell*)(((INT_PTR)m_pPODCells[i] & ~pmask) + ((INT_PTR)m_pDummyPODcell & pmask));
		imask &= -m_bHasPODGrid & ~pmask;
		return pcell0 + ((ix&7)+(iy&7)*8 & imask);
	}

	virtual IPhysicalEntity* CreatePhysicalEntity(pe_type type, pe_params* params=0, void *pForeignData=0,int iForeignData=0, int id=-1)
	{ return CreatePhysicalEntity(type,0.0f,params,pForeignData,iForeignData,id); }
	virtual IPhysicalEntity* CreatePhysicalEntity(pe_type type, float lifeTime, pe_params* params=0, void *pForeignData=0,int iForeignData=0, 
		int id=-1,IPhysicalEntity *pHostPlaceholder=0);
	virtual IPhysicalEntity *CreatePhysicalPlaceholder(pe_type type, pe_params* params=0, void *pForeignData=0,int iForeignData=0, int id=-1);
	virtual int DestroyPhysicalEntity(IPhysicalEntity *pent, int mode=0, int bThreadSafe=0);
	virtual int SetPhysicalEntityId(IPhysicalEntity *pent, int id, int bReplace=1, int bThreadSafe=0);
	virtual int GetPhysicalEntityId(IPhysicalEntity *pent);
	int GetFreeEntId();
	virtual IPhysicalEntity* GetPhysicalEntityById(int id);
	int IsPlaceholder(CPhysicalPlaceholder *pent) {
		if (!pent) return 0;
		int iChunk; for(iChunk=0; iChunk<m_nPlaceholderChunks && (unsigned int)(pent-m_pPlaceholders[iChunk])>=(unsigned int)PLACEHOLDER_CHUNK_SZ; iChunk++);
		return iChunk<m_nPlaceholderChunks ? (iChunk<<PLACEHOLDER_CHUNK_SZLG2 | pent-m_pPlaceholders[iChunk])+1 : 0;
	}

	virtual void TimeStep(float time_interval, int flags=ent_all|ent_deleted);
	int ResolveGroupContacts(int i,int nAnimatedObjects);
	virtual float GetPhysicsTime() { return m_timePhysics; }
	virtual int GetiPhysicsTime() { return m_iTimePhysics; }
	virtual void SetPhysicsTime(float time) { 
		m_timePhysics = time; 
		if (m_vars.timeGranularity>0) 
			m_iTimePhysics = (int)(m_timePhysics/m_vars.timeGranularity+0.5f); 
	}
	virtual void SetiPhysicsTime(int itime) { m_timePhysics = (m_iTimePhysics=itime)*m_vars.timeGranularity; }
	virtual void SetSnapshotTime(float time_snapshot,int iType=0) { 
		m_timeSnapshot[iType] = time_snapshot; 
		if (m_vars.timeGranularity>0) 
			m_iTimeSnapshot[iType] = (int)(time_snapshot/m_vars.timeGranularity+0.5f); 
	}
	virtual void SetiSnapshotTime(int itime_snapshot,int iType=0) { 
		m_iTimeSnapshot[iType] = itime_snapshot; m_timeSnapshot[iType] = itime_snapshot*m_vars.timeGranularity; 
	}

	// *important* if request RWIs queued iForeignData should be a EPhysicsForeignIds
	virtual int RayWorldIntersection(Vec3 org,Vec3 dir, int objtypes, unsigned int flags, ray_hit *hits,int nmaxhits, 
		IPhysicalEntity **pSkipEnts=0,int nSkipEnts=0, void *pForeignData=0,int iForeignData=0, 
		const char *pNameTag="RayWorldIntersection(Physics)", ray_hit_cached *phitLast=0, int iCaller=0);
	virtual int TracePendingRays(int bDoTracing=1);

	virtual void SimulateExplosion(pe_explosion *pexpl, IPhysicalEntity **pSkipEnts=0,int nSkipEnts=0,
		int iTypes=ent_rigid|ent_sleeping_rigid|ent_living|ent_independent, int iCaller=0);
	virtual float IsAffectedByExplosion(IPhysicalEntity *pent, Vec3 *impulse=0);
	virtual float CalculateExplosionExposure(pe_explosion *pexpl, IPhysicalEntity *pient);
	virtual void ResetDynamicEntities();
	virtual void DestroyDynamicEntities();
	virtual void PurgeDeletedEntities();
	virtual int DeformPhysicalEntity(IPhysicalEntity *pent, const Vec3 &ptHit,const Vec3 &dirHit,float r, int flags=0);
	virtual void UpdateDeformingEntities(float time_interval);
	virtual int GetEntityCount(int iEntType) { return m_nTypeEnts[iEntType]; }
	virtual int ReserveEntityCount(int nNewEnts);

	virtual void DrawPhysicsHelperInformation(IPhysRenderer *pRenderer,int iCaller=0);

	virtual void GetMemoryStatistics(ICrySizer *pSizer);

	virtual int CollideEntityWithBeam(IPhysicalEntity *_pent, Vec3 org,Vec3 dir,float r, ray_hit *phit);
	virtual int RayTraceEntity(IPhysicalEntity *pient, Vec3 origin,Vec3 dir, ray_hit *pHit, pe_params_pos *pp=0);
	virtual int CollideEntityWithPrimitive(IPhysicalEntity *_pent, int itype, primitive *pprim, Vec3 dir, ray_hit *phit);

	virtual float PrimitiveWorldIntersection(int itype, primitive *pprim, const Vec3 &sweepDir=Vec3(ZERO), int entTypes=ent_all, 
		geom_contact **ppcontact=0, int geomFlagsAll=0,int geomFlagsAny=geom_colltype0|geom_colltype_player, intersection_params *pip=0,
		void *pForeignData=0, int iForeignData=0, IPhysicalEntity **pSkipEnts=0,int nSkipEnts=0, const char *pNameTag="PrimitiveWorldIntersection");

	virtual int GetEntitiesInBox(Vec3 ptmin,Vec3 ptmax, IPhysicalEntity **&pList, int objtypes, int szListPrealloc) {
		WriteLock lock(m_lockCaller[1]);
		return GetEntitiesAround(ptmin,ptmax, (CPhysicalEntity**&)pList, objtypes, 0, szListPrealloc, 1);
	}
	int GetEntitiesAround(const Vec3 &ptmin,const Vec3 &ptmax, CPhysicalEntity **&pList, int objtypes, CPhysicalEntity *pPetitioner=0, 
		int szListPrealoc=0, int iCaller=0);
	void ChangeEntitySimClass(CPhysicalEntity *pent);
	int RepositionEntity(CPhysicalPlaceholder *pobj, int flags=3, Vec3 *BBox=0, int bQueued=0);
	void DetachEntityGridThunks(CPhysicalPlaceholder *pobj);
	void ScheduleForStep(CPhysicalEntity *pent);
	CPhysicalEntity *CheckColliderListsIntegrity();

	virtual int CoverPolygonWithCircles(strided_pointer<vector2df> pt,int npt,bool bConsecutive, const vector2df &center, 
		vector2df *&centers,float *&radii, float minCircleRadius) 
	{ return ::CoverPolygonWithCircles(pt,npt,bConsecutive, center, centers,radii, minCircleRadius); }
	virtual void DeletePointer(void *pdata) { if (pdata) delete[] (char*)pdata; }
	virtual int qhull(strided_pointer<Vec3> pts, int npts, index_t*& pTris) { return ::qhull(pts,npts,pTris); }
	virtual void SetPhysicsStreamer(IPhysicsStreamer *pStreamer) { m_pPhysicsStreamer=pStreamer; }
	virtual void SetPhysicsEventClient(IPhysicsEventClient *pEventClient) { m_pEventClient=pEventClient; }
	virtual float GetLastEntityUpdateTime(IPhysicalEntity *pent) { return m_updateTimes[((CPhysicalPlaceholder*)pent)->m_iSimClass & 7]; }
	virtual volatile int *GetInternalLock(int idx) {
		switch (idx) {
			case PLOCK_WORLD_STEP: return &m_lockStep; 
			case PLOCK_CALLER0: return m_lockCaller+0; 
			case PLOCK_CALLER1: return m_lockCaller+1;
			case PLOCK_QUEUE: return &m_lockQueue;
			case PLOCK_AREAS: return &m_lockAreas;
		}
		return 0;
	}

	void AddEntityProfileInfo(CPhysicalEntity *pent,int nTicks);
	virtual int GetEntityProfileInfo(phys_profile_info *&pList)	{	pList=m_pEntProfileData; return m_nProfiledEnts; }
	void AddFuncProfileInfo(const char *name,int nTicks);
	virtual int GetFuncProfileInfo(phys_profile_info *&pList)	{	pList=m_pFuncProfileData; return m_nProfileFunx; }

	// *important* if provided callback function return 0, other registered listeners are not called anymore.
	virtual void AddEventClient(int type, int (*func)(const EventPhys*), int bLogged, float priority=1.0f);
	virtual int RemoveEventClient(int type, int (*func)(const EventPhys*), int bLogged);
	virtual void PumpLoggedEvents();
	virtual void ClearLoggedEvents();
	void CleanseEventsQueue();
	EventPhys *AllocEvent(int id,int sz);
	template<class Etype> int OnEvent(unsigned int flags, Etype *pEvent, Etype **pEventLogged=0) {
		int res = 0;
		if ((flags & Etype::flagsCall)==Etype::flagsCall)
			res = SignalEvent(pEvent, 0);
		if ((flags & Etype::flagsLog)==Etype::flagsLog) {
			WriteLock lock(m_lockEventsQueue);
			Etype *pDst = (Etype*)AllocEvent(Etype::id,sizeof(Etype));
			*pDst = *pEvent; pDst->next = 0;
			(m_pEventLast ? m_pEventLast->next : m_pEventFirst) = pDst;
			m_pEventLast = pDst;
			if (pEventLogged)
				*pEventLogged = pDst;
			if (Etype::id==(const int)EventPhysPostStep::id) {
				CPhysicalPlaceholder *ppc = (CPhysicalPlaceholder*)((EventPhysPostStep*)pEvent)->pEntity;
				if (ppc->m_bProcessed & PENT_SETPOSED)
					AtomicAdd(&ppc->m_bProcessed, -PENT_SETPOSED);
			}
		}
		return res;
	}
	template<class Etype> int SignalEvent(Etype *pEvent, int bLogged) {
		int nres = 0;
		EventClient *pClient;
		for(pClient = m_pEventClients[Etype::id][bLogged]; pClient; pClient=pClient->next)
			nres += pClient->OnEvent(pEvent);
		return nres;
	}

	virtual int SerializeWorld(const char *fname, int bSave);
	virtual int SerializeGeometries(const char *fname, int bSave);

	virtual IPhysicalEntity *AddGlobalArea();
	virtual IPhysicalEntity *AddArea(Vec3 *pt,int npt, float zmin,float zmax, const Vec3 &pos=Vec3(0,0,0), const quaternionf &q=quaternionf(), 
		float scale=1.0f, const Vec3& normal=Vec3(ZERO), int *pTessIdx=0,int nTessTris=0,Vec3 *pFlows=0);
	virtual IPhysicalEntity *AddArea(IGeometry *pGeom, const Vec3& pos,const quaternionf &q,float scale);
	virtual IPhysicalEntity *AddArea(Vec3 *pt,int npt, float r, const Vec3 &pos,const quaternionf &q,float scale);
	virtual void RemoveArea(IPhysicalEntity *pArea);
	virtual int CheckAreas(const Vec3 &ptc, Vec3 &gravity, pe_params_buoyancy *pb, int nMaxBuoys=1, const Vec3 &vel=Vec3(ZERO), 
		IPhysicalEntity *pent=0, int iCaller=0);
	int CheckAreas(CPhysicalEntity *pent, Vec3 &gravity, pe_params_buoyancy *pb,int nMaxBuoys=1, const Vec3 &vel=Vec3(ZERO), int iCaller=0) {
		if (!m_pGlobalArea || pent->m_flags & pef_ignore_areas)
			return 0;
		return CheckAreas((pent->m_BBox[0]+pent->m_BBox[1])*0.5f, gravity, pb, nMaxBuoys, vel, pent, iCaller);
	}
	void RepositionArea(CPhysArea *pArea);
	virtual IPhysicalEntity *GetNextArea(IPhysicalEntity *pPrevArea=0);

	virtual void SetWaterMat(int imat);
	virtual int GetWaterMat() { return m_matWater; }
	virtual int SetWaterManagerParams(pe_params *params);
	virtual int GetWaterManagerParams(pe_params *params);
	virtual int GetWatermanStatus(pe_status *status);
	virtual void DestroyWaterManager();

	virtual int AddExplosionShape(IGeometry *pGeom, float size,int idmat, float probability=1.0f);
	virtual void RemoveExplosionShape(int id);
	IGeometry *GetExplosionShape(float size,int idmat, float &scale, int &bCreateConstraint);
	int DeformEntityPart(CPhysicalEntity *pent,int i, pe_explosion *pexpl, geom_world_data *gwd,geom_world_data *gwd1, int iSource=0);
	void MarkEntityAsDeforming(CPhysicalEntity *pent);
	void ClonePhysGeomInEntity(CPhysicalEntity *pent,int i,IGeometry *pNewGeom);

	void AllocRequestsQueue(int sz) {
		if (m_nQueueSlotSize+sz+1 > QUEUE_SLOT_SZ) {
			if (m_nQueueSlots==m_nQueueSlotsAlloc)
				ReallocateList(m_pQueueSlots, m_nQueueSlots,m_nQueueSlotsAlloc+=8, true);
			if (!m_pQueueSlots[m_nQueueSlots])
				m_pQueueSlots[m_nQueueSlots] = new char[max(sz+1,QUEUE_SLOT_SZ)];
			++m_nQueueSlots; m_nQueueSlotSize = 0;
		}
	}
	void *QueueData(void *ptr, int sz) {
		memcpy(m_pQueueSlots[m_nQueueSlots-1]+m_nQueueSlotSize, ptr, sz);	m_nQueueSlotSize += sz;
		*(int*)(m_pQueueSlots[m_nQueueSlots-1]+m_nQueueSlotSize) = -1;
		return m_pQueueSlots[m_nQueueSlots-1]+m_nQueueSlotSize-sz;
	}
	template<class T> T* QueueData(const T& data) {
		*(T*)(m_pQueueSlots[m_nQueueSlots-1]+m_nQueueSlotSize) = data; m_nQueueSlotSize += sizeof(data);
		*(int*)(m_pQueueSlots[m_nQueueSlots-1]+m_nQueueSlotSize) = -1;
		return (T*)(m_pQueueSlots[m_nQueueSlots-1]+m_nQueueSlotSize-sizeof(data));
	}

	entity_contact *AllocContact();
	void FreeContact(entity_contact *pContact);

	float GetFriction(int imat0,int imat1,int bDynamic=0) {
		float *pTable = (float*)((intptr_t)m_FrictionTable&~(intptr_t)-bDynamic | (intptr_t)m_DynFrictionTable&(intptr_t)-bDynamic);
		return max(0.0f,pTable[imat0 & NSURFACETYPES-1]+pTable[imat1 & NSURFACETYPES-1])*0.5f;
	}
	float GetBounciness(int imat0,int imat1) {
		return (m_BouncinessTable[imat0 & NSURFACETYPES-1]+m_BouncinessTable[imat1 & NSURFACETYPES-1])*0.5f;
	}

	void SavePhysicalEntityPtr(TSerialize ser, CPhysicalEntity *pent);
	CPhysicalEntity *LoadPhysicalEntityPtr(TSerialize ser);

	IPhysicalWorld *GetIWorld() { return this; }

	virtual void SerializeGarbageTypedSnapshot( TSerialize ser, int iSnapshotType, int flags );

	PhysicsVars m_vars;
	ILog *m_pLog;
	IPhysicsStreamer *m_pPhysicsStreamer;
	IPhysicsEventClient *m_pEventClient;
	IPhysRenderer *m_pRenderer;

	CPhysicalEntity *m_pTypedEnts[8],*m_pTypedEntsPerm[8];
	CPhysicalEntity **m_pTmpEntList,**m_pTmpEntList1,**m_pTmpEntList2;
	float *m_pGroupMass,*m_pMassList;
	int *m_pGroupIds,*m_pGroupNums;
	grid m_entgrid;
	int m_iEntAxisz;
	int *m_pEntGrid;
	pe_gridthunk *m_gthunks;
	int m_thunkPoolSz,m_iFreeGThunk0;
	pe_PODcell **m_pPODCells,m_dummyPODcell,*m_pDummyPODcell;
	vector2di m_PODstride;
	volatile int m_lockPODGrid;
	int m_bHasPODGrid,m_iActivePODCell0;
	int m_nEnts,m_nEntsAlloc;
	int m_nDynamicEntitiesDeleted;
	CPhysicalPlaceholder **m_pEntsById;
	int m_nIdsAlloc, m_iNextId;
	int m_iNextIdDown,m_lastExtId,m_nExtIds;
	int m_bGridThunksChanged;
	int m_bUpdateOnlyFlagged;
	int m_nTypeEnts[10];
	int m_bEntityCountReserved;
	volatile int m_lockEntIdList;
	volatile int m_nGEA[2];
	volatile int m_nEntListAllocs;
	int m_nOnDemandListFailures;
	int m_iLastPODUpdate;
	Vec3 m_prevGEABBox[2];
	int m_prevGEAobjtypes;
	int m_nprevGEAEnts;

	int m_nPlaceholders,m_nPlaceholderChunks,m_iLastPlaceholder;
	CPhysicalPlaceholder **m_pPlaceholders;
	int *m_pPlaceholderMap;
	CPhysicalEntity *m_pEntBeingDeleted;

	int *m_pGridStat[6],*m_pGridDyn[6];
	int m_nOccRes;
	Vec3 m_lastEpicenter,m_lastEpicenterImp,m_lastExplDir;
	float m_lastRmax;
	CPhysicalEntity **m_pExplVictims;
	float *m_pExplVictimsFrac;
	Vec3 *m_pExplVictimsImp;
	int m_nExplVictims,m_nExplVictimsAlloc;

	CPhysicalEntity *m_pHeightfield[2];
	Matrix33 m_HeightfieldBasis;
	Vec3 m_HeightfieldOrigin;

	float m_timePhysics,m_timeSurplus,m_timeSnapshot[4];
	int m_iTimePhysics,m_iTimeSnapshot[4];
	float m_updateTimes[8];
	int m_iSubstep,m_bWorldStep,m_iCurGroup;
	int m_bCurGroupInvisible;
	float m_curGroupMass;
	CPhysicalEntity *m_pAuxStepEnt;
	phys_profile_info m_pEntProfileData[16];
	int m_nProfiledEnts;
	phys_profile_info *m_pFuncProfileData;
	int m_nProfileFunx,m_nProfileFunxAlloc;
	volatile int m_lockFuncProfiler;
	float m_groupTimeStep;
	float m_lastTimeInterval;
	volatile int m_idThread;
	volatile int m_idPODThread;

	float m_BouncinessTable[NSURFACETYPES];
	float m_FrictionTable[NSURFACETYPES];
	float m_DynFrictionTable[NSURFACETYPES];
	unsigned int m_SurfaceFlagsTable[NSURFACETYPES];
	int m_matWater,m_bCheckWaterHits;
	class CWaterMan *m_pWaterMan;
	Vec3 m_posViewer;

	volatile int m_lockStep,m_lockGrid,m_lockCaller[2],m_lockQueue,m_lockList;
	char **m_pQueueSlots;
	int m_nQueueSlots,m_nQueueSlotsAlloc;
	int m_nQueueSlotSize;

	CPhysArea *m_pGlobalArea;
	int m_nAreas,m_nBigAreas;
	volatile int m_lockAreas;
	CPhysArea *m_pDeletedAreas;

	volatile int m_lockEventsQueue,m_iLastLogPump;
	EventPhys *m_pEventFirst,*m_pEventLast,*m_pFreeEvents[EVENT_TYPES_NUM];
	EventClient *m_pEventClients[EVENT_TYPES_NUM][2];
	EventChunk *m_pFirstEventChunk,*m_pCurEventChunk;
	int m_szCurEventChunk;
	int m_nEvents[EVENT_TYPES_NUM];
	volatile int m_idStep;

	entity_contact *m_pFreeContact,*m_pLastFreeContact;
	int m_nFreeContacts;
	int m_nContactsAlloc;

	SExplosionShape *m_pExpl;
	int m_nExpl,m_nExplAlloc,m_idExpl;
	CPhysicalEntity **m_pDeformingEnts;
	int m_nDeformingEnts,m_nDeformingEntsAlloc;
	volatile int m_lockDeformingEntsList;

	SRwiRequest *m_rwiQueue;
	int m_rwiQueueHead,m_rwiQueueTail;
	int m_rwiQueueSz,m_rwiQueueAlloc;
	volatile int m_lockRwiQueue;
	ray_hit *m_pRwiHitsHead,*m_pRwiHitsTail;
	int m_rwiHitsPoolSize;
	int m_rwiPoolEmpty;
	volatile int m_lockRwiHitsPool;
	CPhysicalEntity *m_pentLastHit[2];
	int m_ipartLastHit[2],m_inodeLastHit[2];
	volatile int m_lockTPR;

	SPwiRequest *m_pwiQueue;
	int m_pwiQueueHead,m_pwiQueueTail;
	int m_pwiQueueSz,m_pwiQueueAlloc;
	volatile int m_lockPwiQueue;
};


class CPhysArea : public CPhysicalPlaceholder {
public:
	CPhysArea(CPhysicalWorld *pWorld);
	~CPhysArea();
	virtual pe_type GetType() { return PE_AREA; }
	virtual CPhysicalEntity *GetEntity();
	virtual CPhysicalEntity *GetEntityFast() { return GetEntity(); }
	virtual int AddRef() { CryInterlockedAdd(&m_lockRef, 1); return m_lockRef; }
	virtual int Release() { CryInterlockedAdd(&m_lockRef, -1); return m_lockRef; }

	virtual int SetParams(pe_params* params,int bThreadSafe=1);
	virtual int GetParams(pe_params* params);
	virtual int GetStatus(pe_status* status);
	virtual IPhysicalWorld *GetWorld() { return m_pWorld; }

	int CheckPoint(const Vec3& pttest);
	int ApplyParams(const Vec3& pt, Vec3& gravity,const Vec3 &vel, pe_params_buoyancy *pbdst,int nBuoys,int nMaxBuoys,int &iMedium0, IPhysicalEntity *pent);
	float FindSplineClosestPt(const Vec3 &ptloc, int &iClosestSeg,float &tClosest);
	int FindSplineClosestPt(const Vec3 &org, const Vec3 &dir, Vec3 &ptray, Vec3 &ptspline);
	virtual void DrawHelperInformation(IPhysRenderer *pRenderer, int flags);
	int RayTrace(const Vec3 &org, const Vec3 &dir, ray_hit *pHit, pe_params_pos *pp=0);

	virtual float ComputeExtent(GeomQuery& geo, EGeomForm eForm);
	virtual void GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm);

	virtual void GetMemoryStatistics(ICrySizer *pSizer);

	int m_bDeleted;
	CPhysArea *m_next,*m_nextBig;
	CPhysicalWorld *m_pWorld;

	Vec3 m_offset;
	Matrix33 m_R;
	float m_scale,m_rscale;

	Vec3 m_offset0;
	Matrix33 m_R0;

	IGeometry *m_pGeom;
	Vec2 *m_pt;
	int m_npt;
	float m_zlim[2];
	int *m_idxSort[2];
	Vec3 *m_pFlows;
	unsigned int *m_pMask;
	Vec3 m_size0;
	Vec3 *m_ptSpline;
	Vec3 m_ptLastCheck;
	int m_iClosestSeg;
	float m_tClosest,m_mindist;
	indexed_triangle m_trihit;

	pe_params_buoyancy m_pb;
	Vec3 m_gravity;
	Vec3 m_size,m_rsize;
	int m_bUniform;
	float m_falloff0,m_rfalloff0;
	float m_damping;
	int m_bUseCallback;
	volatile int m_lockRef;
};


extern int g_nPhysWorlds;
extern CPhysicalWorld *g_pPhysWorlds[];

#ifdef ENTITY_PROFILER_ENABLED
#define PHYS_ENTITY_PROFILER CPhysEntityProfiler ent_profiler(this);
#else
#define PHYS_ENTITY_PROFILER
#endif

#ifndef PHYS_FUNC_PROFILER_DISABLED
#define PHYS_FUNC_PROFILER(name) CPhysFuncProfiler func_profiler((name),this);
#else
#define PHYS_FUNC_PROFILER
#endif

struct CPhysEntityProfiler {
	int64 m_iStartTime;
	CPhysicalEntity *m_pEntity;

	CPhysEntityProfiler(CPhysicalEntity *pent) {
		m_pEntity = pent;
		m_iStartTime = CryGetTicks();
	}
	~CPhysEntityProfiler() {
		if (m_pEntity->m_pWorld->m_vars.bProfileEntities)
			m_pEntity->m_pWorld->AddEntityProfileInfo(m_pEntity,CryGetTicks()-m_iStartTime);
	}
};

struct CPhysFuncProfiler {
	int64 m_iStartTime;
	const char *m_name;
	CPhysicalWorld *m_pWorld;

	CPhysFuncProfiler(const char *name, CPhysicalWorld *pWorld) {
		m_name = name; m_pWorld = pWorld;
		m_iStartTime = CryGetTicks();
	}
	~CPhysFuncProfiler() {
		if (m_pWorld->m_vars.bProfileFunx)
			m_pWorld->AddFuncProfileInfo(m_name, CryGetTicks()-m_iStartTime);
	}
};

template<class T> inline bool StructChangesPos(T*) { return false; }
inline bool StructChangesPos(pe_params *params) { 
	pe_params_pos *pp = (pe_params_pos*)params;
	return params->type==pe_params_pos::type_id && (!is_unused(pp->pos) || !is_unused(pp->pMtx3x4) && pp->pMtx3x4) && !(pp->bRecalcBounds & 16); 
}

template<class T> inline void OnStructQueued(T *params, CPhysicalWorld *pWorld, void *ptrAux,int iAux) {}
inline void OnStructQueued(pe_geomparams *params, CPhysicalWorld *pWorld, void *ptrAux,int iAux) { pWorld->AddRefGeometry((phys_geometry*)ptrAux); }

template<class T> struct ChangeRequest {
	CPhysicalWorld *m_pWorld;
	int m_bQueued,m_bLocked;
	T *m_pQueued;

	ChangeRequest(CPhysicalPlaceholder *pent, CPhysicalWorld *pWorld, T *params, int bInactive, void *ptrAux=0,int iAux=0) : 
	m_pWorld(pWorld),m_bQueued(0),m_bLocked(0) 
	{
		if (bInactive<=0 && pent->m_iSimClass!=7) {
			if (m_pWorld->m_lockStep || m_pWorld->m_lockTPR || pent->m_bProcessed>=PENT_QUEUED || bInactive<0) {
				subref *psubref;
				int szSubref,szTot;
				WriteLock lock(m_pWorld->m_lockQueue);
				AtomicAdd(&pent->m_bProcessed, PENT_QUEUED);
				for(psubref=GetSubref(params),szSubref=0; psubref; psubref=psubref->next)
					if (*(char**)((char*)params+psubref->iPtrOffs) && !is_unused(*(char**)((char*)params+psubref->iPtrOffs)))
						szSubref += ((*(int*)((char*)params+max(0,psubref->iSzOffs)) & -psubref->iSzOffs>>31) + psubref->iSzFixed)*psubref->iSzUnit;
				szTot = sizeof(int)*2+sizeof(void*)+GetStructSize(params)+szSubref;
				if (StructUsesAuxData(params))
					szTot += sizeof(void*)+sizeof(int);
				m_pWorld->AllocRequestsQueue(szTot);
				m_pWorld->QueueData(GetStructId(params));
				m_pWorld->QueueData(szTot);
				m_pWorld->QueueData(pent);
				if (StructUsesAuxData(params)) {
					m_pWorld->QueueData(ptrAux);
					m_pWorld->QueueData(iAux);
				}
				m_pQueued = (T*)m_pWorld->QueueData(params, GetStructSize(params));
				for(psubref=GetSubref(params); psubref; psubref=psubref->next) {
					szSubref = ((*(int*)((char*)params+max(0,psubref->iSzOffs)) & -psubref->iSzOffs>>31) + psubref->iSzFixed)*psubref->iSzUnit;
					if (*(char**)((char*)params+psubref->iPtrOffs) && !is_unused(*(char**)((char*)params+psubref->iPtrOffs)))
						*(void**)((char*)m_pQueued+psubref->iPtrOffs) = m_pWorld->QueueData(*(char**)((char*)params+psubref->iPtrOffs), szSubref);
				}
				OnStructQueued(params,pWorld,ptrAux,iAux);
				m_bQueued = 1;
			}	else {
				SpinLock(&m_pWorld->m_lockStep, 0,1);
				m_bLocked = 1;
			}
		} 
		if (StructChangesPos(params) && !(pent->m_bProcessed & PENT_SETPOSED))
			AtomicAdd(&pent->m_bProcessed, PENT_SETPOSED);
	}
	~ChangeRequest() { AtomicAdd(&m_pWorld->m_lockStep,-m_bLocked); }
	int IsQueued() { return m_bQueued; }
	T *GetQueuedStruct() { return m_pQueued; }
};

#endif