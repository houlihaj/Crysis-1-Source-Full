//////////////////////////////////////////////////////////////////////
//
//	Physical Entity header
//	
//	File: physicalentity.h
//	Description : PhysicalEntity class declarations
//
//	History:
//	-:Created by Anton Knyazev
//
//////////////////////////////////////////////////////////////////////

#ifndef physicalentity_h
#define physicalentity_h
#pragma once

#include "ISerialize.h"

enum phentity_flags_int { 
	pef_use_geom_callbacks = 0x20000000
};

struct coord_block {
	Vec3 pos;
	quaternionf q;
};

struct coord_block_BBox {
	Vec3 pos;
	quaternionf q;
	float scale;
	Vec3 BBox[2];
};

enum cbbx_flags { part_added=1, update_part_bboxes=2 };	// for ComputeBBox

enum geom_flags_aux { geom_car_wheel=0x40000000, geom_invalid=0x20000000, geom_removed=0x10000000, geom_will_be_destroyed=0x8000000,
											geom_constraint_on_break=geom_proxy };

class CTetrLattice;

struct geom {
	Vec3 pos;
	quaternionf q;
	float scale;
	Vec3 BBox[2];
	phys_geometry *pPhysGeom,*pPhysGeomProxy;
	int id;
	float mass;
	unsigned int flags,flagsCollider;
	float maxdim;
	float minContactDist;
	int surface_idx : 10;
	int idmatBreakable : 14;
	int nMats : 8;
	CTetrLattice *pLattice;
	int *pMatMapping;
	coord_block_BBox *pNewCoords;

#ifdef _DEBUG
	geom()
	{
		uint32* p;
		p=(uint32*)&scale;					p[0]=F32NAN;
		p=(uint32*)&mass;						p[0]=F32NAN;
		p=(uint32*)&maxdim;					p[0]=F32NAN;
		p=(uint32*)&minContactDist;	p[0]=F32NAN;
	}
#endif
};

struct SRigidEntityNetSerialize
{
	Vec3 pos;
	Quat rot;
	Vec3 vel;
	Vec3 angvel;
	bool simclass;

	void Serialize( TSerialize ser );
};

struct SStructuralJoint {
	int id;
	int ipart[2];
	Vec3 pt;
	Vec3 n;
	float maxForcePush,maxForcePull,maxForceShift;
	float maxTorqueBend,maxTorqueTwist;
	int bBreakable,bBroken;
	float size;
	float tension;
	int itens;
};

struct SStructureInfo {
	Vec3 *Pext,*Lext;
	Vec3 *Fext,*Text;
	SStructuralJoint *pJoints;
	int nJoints,nJointsAlloc;
	int nLastBrokenJoints;
	int bModified;
	int idpartBreakOrg;
	int nPartsAlloc;
	float timeLastUpdate;
	int nPrevJoints;
	float prevdt;
	float minSnapshotTime;
	float autoDetachmentDist;
	Vec3 lastExplPos;
	Vec3 *Pexpl,*Lexpl;
};

struct SPartHelper {
	int idx;
	float Minv;
	Matrix33 Iinv;
	Vec3 v,w;
	Vec3 org;
	int bProcessed;
	int ijoint0;
	int isle;
	CPhysicalEntity *pent;
};

struct SStructuralJointHelper {
	int idx;
	Vec3 r0,r1;
	Matrix33 vKinv,wKinv;
	Vec3 P,L;
	int bBroken;
};
struct SStructuralJointDebugHelper {
	int itens;
	quotientf tension;
};

struct SExplosionInfo {
	Vec3 center;
	Vec3 dir;
	float rmin,kr;
};

class CPhysicalWorld;
class CRayGeom;
class CGeometry;

class CPhysicalEntity : public CPhysicalPlaceholder {
public:
	CPhysicalEntity(CPhysicalWorld *pworld);
	virtual ~CPhysicalEntity();
	virtual pe_type GetType() { return PE_STATIC; }

	virtual int AddRef();
	virtual int Release();

	virtual int SetParams(pe_params*,int bThreadSafe=1);
	virtual int GetParams(pe_params*);
	virtual int GetStatus(pe_status*);
	virtual int Action(pe_action*,int bThreadSafe=1);
	virtual int AddGeometry(phys_geometry *pgeom, pe_geomparams* params,int id=-1,int bThreadSafe=1);
	virtual void RemoveGeometry(int id,int bThreadSafe=1);
	virtual float ComputeExtent(GeomQuery& geo, EGeomForm eForm);
	virtual void GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm);
	virtual void *GetForeignData(int itype=0) { return itype==m_iForeignData ? m_pForeignData:0; }
	virtual int GetiForeignData() { return m_iForeignData; }
	virtual IPhysicalWorld *GetWorld() { return (IPhysicalWorld*)m_pWorld; }
	virtual CPhysicalEntity *GetEntity() { return this; }
	virtual CPhysicalEntity *GetEntityFast() { return this; }

	virtual void StartStep(float time_interval) {}
	virtual float GetMaxTimeStep(float time_interval) { return time_interval; }
	virtual float GetLastTimeStep(float time_interval) { return time_interval; }
	virtual int Step(float time_interval) { return 1; }
	virtual int DoStep(float time_interval, int iCaller=0) { return 1; }
	virtual void StepBack(float time_interval) {} 
	virtual int GetContactCount(int nMaxPlaneContacts) { return 0; }
	virtual int RegisterContacts(float time_interval,int nMaxPlaneContacts) { return 0; }
	virtual int Update(float time_interval, float damping) { return 1; }
	virtual float CalcEnergy(float time_interval) { return 0; }
	virtual float GetDamping(float time_interval) { return 1.0f; } 
	virtual bool IgnoreCollisionsWith(CPhysicalEntity *pent, int bCheckConstraints=0) { return false; }
	virtual void GetSleepSpeedChange(int ipart, Vec3 &v,Vec3 &w) { v.zero(); w.zero(); }

	virtual int AddCollider(CPhysicalEntity *pCollider);
	virtual int RemoveCollider(CPhysicalEntity *pCollider, bool bAlwaysRemove=true);
	virtual int RemoveContactPoint(CPhysicalEntity *pCollider, const Vec3 &pt, float mindist2) { return -1; }
	virtual int HasContactsWith(CPhysicalEntity *pent) { return 0; }
	virtual int HasCollisionContactsWith(CPhysicalEntity *pent) { return 0; }
	virtual int HasConstraintContactsWith(CPhysicalEntity *pent, int flagsIgnore=0) { return 0; }
	virtual void AlertNeighbourhoodND();
	virtual int Awake(int bAwake=1,int iSource=0) { return 0; }
	virtual int IsAwake(int ipart=-1) { return 0; }
	int GetColliders(CPhysicalEntity **&pentlist) { pentlist=m_pColliders; return m_nColliders; }
	virtual int RayTrace(CRayGeom *pRay, geom_contact *&pcontacts, volatile int *&plock) { return 0; }
	virtual void ApplyVolumetricPressure(const Vec3 &epicenter, float kr, float rmin) {}
	virtual void OnContactResolved(entity_contact *pContact, int iop, int iGroupId);

	virtual void OnNeighbourSplit(CPhysicalEntity *pentOrig, CPhysicalEntity *pentNew) {}
	virtual void OnStructureUpdate() {}

	virtual RigidBody *GetRigidBody(int ipart=-1,int bWillModify=0);
	virtual RigidBody *GetRigidBodyData(RigidBody *pbody, int ipart=-1) { return GetRigidBody(ipart); }
	virtual float GetMass(int ipart) { return m_parts[ipart].mass; }
	virtual void GetContactMatrix(const Vec3 &pt, int ipart, Matrix33 &K) {}
	virtual void GetSpatialContactMatrix(const Vec3 &pt, int ipart, float Ibuf[][6]) {}
	virtual float GetMassInv() { return 0; }
	virtual int IsPointInside(Vec3 pt);
	virtual void GetLocTransform(int ipart, Vec3 &offs, quaternionf &q, float &scale) {
		if ((unsigned int)ipart<(unsigned int)m_nParts) {
			q = m_qrot*m_parts[ipart].q;
			offs = m_qrot*m_parts[ipart].pos + m_pos;
			scale = m_parts[ipart].scale;
		} else {
			q.SetIdentity(); offs.zero(); scale=1.0f;
		}
	}
	virtual void DetachPartContacts(int ipart,int iop0, CPhysicalEntity *pent,int iop1, int bCheckIfEmpty=1) {}
	int TouchesSphere(const Vec3 &center, float r);

	virtual void DrawHelperInformation(IPhysRenderer *pRenderer, int flags);
	virtual void GetMemoryStatistics(ICrySizer *pSizer);

	virtual int GetStateSnapshot(class CStream &stm, float time_back=0,	int flags=0) { return 0; }
	virtual int GetStateSnapshot(TSerialize ser, float time_back=0, int flags=0);
	virtual int SetStateFromSnapshot(class CStream &stm, int flags=0) { return 0; }
	virtual int SetStateFromSnapshot(TSerialize ser, int flags=0);
	virtual int SetStateFromTypedSnapshot(TSerialize ser, int iSnapshotType, int flags=0);
	virtual int PostSetStateFromSnapshot() { return 1; }
	virtual unsigned int GetStateChecksum() { return 0; }
	virtual int GetStateSnapshotTxt(char *txtbuf,int szbuf, float time_back=0);
	virtual void SetStateFromSnapshotTxt(const char *txtbuf,int szbuf);

	int UpdateStructure(float time_interval, pe_explosion *pexpl, int iCaller=0, Vec3 gravity=Vec3(0));
	virtual void RecomputeMassDistribution(int ipart=-1,int bMassChanged=1) {}

	virtual void ComputeBBox(Vec3 *BBox, int flags=update_part_bboxes);
	void WriteBBox(Vec3 *BBox) {
		m_BBox[0] = BBox[0]; m_BBox[1] = BBox[1];
		if (m_pEntBuddy) {
			m_pEntBuddy->m_BBox[0] = BBox[0];
			m_pEntBuddy->m_BBox[1] = BBox[1];
		}
	}

	void UpdatePartIdmatBreakable(int ipart, int nParts=-1);
	int CapsulizePart(int ipart);

	int GetMatId(int id,int ipart) {
		if (ipart<m_nParts) {
			id += m_parts[ipart].surface_idx-id & id>>31;
			intptr_t mask = iszero_mask(m_parts[ipart].pMatMapping);
			int idummy=0, *pMatMapping = (int*)((intptr_t)m_parts[ipart].pMatMapping & ~mask | (intptr_t)&idummy & mask), nMats;
			nMats = m_parts[ipart].nMats + (65536 & (int)mask);
			return id & (int)mask | pMatMapping[id & ~(int)mask & (id & ~(int)mask)-nMats>>31];
		} else
			return id;
	}

	int m_iDeletionTime;
	volatile int m_nRefCount;
	unsigned int m_flags;
	CPhysicalEntity *m_next,*m_prev;
	CPhysicalWorld *m_pWorld;
	int m_nRefCountPOD   : 16;
	int m_iLastPODUpdate : 16;

	int m_iPrevSimClass : 24;
	int m_bMoved        : 8;
	int m_iGroup;
	CPhysicalEntity *m_next_coll,*m_next_coll1,*m_next_coll2;

	Vec3 m_pos;
	quaternionf m_qrot;
	coord_block *m_pNewCoords;

	CPhysicalEntity **m_pColliders;
	int m_nColliders,m_nCollidersAlloc;

	CPhysicalEntity *m_next_aux,*m_prev_aux;
	CPhysicalEntity *m_pOuterEntity;
	CGeometry *m_pBoundingGeometry;
	int m_bProcessed_aux;

	float m_timeIdle,m_maxTimeIdle;
	int m_bPermanent;

	float m_timeStructUpdate;
	int m_updStage,m_nUpdTicks;
	SExplosionInfo *m_pExpl;

	geom *m_parts,m_defpart;
	int m_nParts,m_nPartsAlloc;
	int m_iLastIdx;
	volatile int m_lockPartIdx;

	plane *m_ground;
	int m_nGroundPlanes;

	SStructureInfo *m_pStructure;
	static SPartHelper *g_parts;
	static SStructuralJointHelper *g_joints;
	static SStructuralJointDebugHelper *g_jointsDbg;
	static int *g_jointidx;
	static int g_nPartsAlloc,g_nJointsAlloc;
};

extern RigidBody g_StaticRigidBody;
extern CPhysicalEntity g_StaticPhysicalEntity;

template<class T> int GetStructSize(T *pstruct);
template<class T> subref *GetSubref(T *pstruct);
template<class T> int GetStructId(T *pstruct);

extern int g_szParams[],g_szAction[],g_szGeomParams[];
extern subref *g_subrefParams[],*g_subrefAction[],*g_subrefGeomParams[];

inline int GetStructSize(pe_params *params) { return g_szParams[params->type]; }
inline int GetStructSize(pe_action *action) { return g_szAction[action->type]; }
inline int GetStructSize(pe_geomparams *geomparams) { return g_szGeomParams[geomparams->type]; }
inline int GetStructSize(void*) { return 0; }
inline subref *GetSubref(pe_params *params) { return g_subrefParams[params->type]; }
inline subref *GetSubref(pe_action *action) { return g_subrefAction[action->type]; }
inline subref *GetSubref(pe_geomparams *geomparams) { return g_subrefGeomParams[geomparams->type]; }
inline subref *GetSubref(void*) { return 0; }
inline int GetStructId(pe_params*) { return 0; }
inline int GetStructId(pe_action*) { return 1; }
inline int GetStructId(pe_geomparams*) { return 2; }
inline int GetStructId(void*) { return 3; }
inline bool StructUsesAuxData(pe_params*) { return false; }
inline bool StructUsesAuxData(pe_action*) { return false; }
inline bool StructUsesAuxData(pe_geomparams*) { return true; }
inline bool StructUsesAuxData(void*) { return true; }

#endif
