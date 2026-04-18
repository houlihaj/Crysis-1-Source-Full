//////////////////////////////////////////////////////////////////////
//
//	Soft Entity header
//	
//	File: softentity.h
//	Description : SoftEntity class declaration
//
//	History:
//	-:Created by Anton Knyazev
//
//////////////////////////////////////////////////////////////////////

#ifndef softentity_h
#define softentity_h
#pragma once

struct se_vertex {
	~se_vertex() { if (pContactEnt) pContactEnt->Release(); }
	Vec3 pos,vel;
	Vec3 pos0,posorg,vel0;
	float massinv;
	float volume;
	Vec3 n,ncontact;
	int bSeparating;
	int iSorted,iSorted0;
	int idx;
	float area;
	int iStartEdge,iEndEdge,bFullFan;
	float rnEdges;
	CPhysicalEntity *pContactEnt;
	int iContactPart;
	int iContactNode;
	Vec3 vcontact;
	int surface_idx[2];
	int bAttached;
	Vec3 ptAttach;
	Vec3 P,dv,r,d;
};

struct se_edge {
	int ivtx[2];
	float len0;
	float len,rlen;
	float kd;
	float kmass;
};


class CSoftEntity : public CPhysicalEntity {
 public:
	CSoftEntity(CPhysicalWorld *pworld);
	virtual ~CSoftEntity();
	virtual pe_type GetType() { return PE_SOFT; }

	virtual int AddGeometry(phys_geometry *pgeom, pe_geomparams* params,int id=-1,int bThreadSafe=1);
	virtual void RemoveGeometry(int id,int bThreadSafe=1);
	virtual int SetParams(pe_params *_params,int bThreadSafe=1);
	virtual int GetParams(pe_params *_params);
	virtual int Action(pe_action*,int bThreadSafe=1);
	virtual int GetStatus(pe_status*);

	virtual int Awake(int bAwake=1,int iSource=0) { if (m_bAwake=bAwake) m_nSlowFrames=0; return 1; }
	virtual int IsAwake(int ipart=-1) { return m_bAwake; }
	virtual void AlertNeighbourhoodND();

	virtual void StartStep(float time_interval);
	virtual float GetMaxTimeStep(float time_interval);
	virtual int Step(float time_interval);
	virtual int RayTrace(CRayGeom *pRay, geom_contact *&pcontacts, volatile int *&plock);
	virtual void ApplyVolumetricPressure(const Vec3 &epicenter, float kr, float rmin);
	virtual float GetMass(int ipart) { return m_parts[0].mass/m_nVtx; }

	enum snapver { SNAPSHOT_VERSION = 10 };
	virtual int GetStateSnapshot(CStream &stm, float time_back=0,int flags=0);
	virtual int SetStateFromSnapshot(CStream &stm, int flags);

	virtual void DrawHelperInformation(IPhysRenderer *pRenderer, int flags);
	virtual void GetMemoryStatistics(ICrySizer *pSizer);

	se_vertex *m_vtx;
	se_edge *m_edges;
	int *m_pVtxEdges;
	int m_nVtx,m_nEdges;
	int m_nConnectedVtx;
	Vec3 m_offs0;
	quaternionf m_qrot0;
	int m_bMeshUpdated;

	float m_timeStepFull;
	float m_timeStepPerformed;

	Vec3 m_gravity;
	float m_Emin;
	float m_maxAllowedStep;
	int m_bAwake,m_nSlowFrames;
	float m_damping;
	float m_accuracy;
	int m_nMaxIters;
	float m_prevTimeInterval;

	float m_thickness;
	float m_ks,m_kdRatio;
	float m_maxSafeStep;
	float m_density;
	float m_coverage;
	float m_friction;
	float m_impulseScale;
	float m_explosionScale;
	float m_collImpulseScale;
	float m_maxCollImpulse;
	int m_collTypes;
	float m_massDecay;

	float m_waterResistance;
	float m_airResistance;
	Vec3 m_wind;
	Vec3 m_wind0,m_wind1;
	float m_windTimer;
	float m_windVariance;

	volatile int m_lockSoftBody;
};

#endif