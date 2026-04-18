//////////////////////////////////////////////////////////////////////
//
//	Physical Placeholder header
//	
//	File: physicalplaceholder.h
//	Description : PhysicalPlaceholder class declarations
//
//	History:
//	-:Created by Anton Knyazev
//
//////////////////////////////////////////////////////////////////////

#ifndef physicalplaceholder_h
#define physicalplaceholder_h
#pragma once

struct pe_gridthunk {
	uint64 inext : 20; // int64 for tighter packing
	uint64 iprev : 20;
	uint64 inextOwned : 20;
	uint64 iSimClass : 3;
	uint64 bFirstInCell : 1;
	unsigned char BBox[4];
	class CPhysicalPlaceholder *pent;
};
class CPhysicalEntity;

class CPhysicalPlaceholder : public IPhysicalEntity {
public:
	CPhysicalPlaceholder() { 
		m_lockUpdate = 0; 
		m_iGThunk0 = 0; m_pEntBuddy = 0;	m_bProcessed = 0;
		m_ig[0].x=m_ig[1].x=m_ig[0].y=m_ig[1].y = -2;
		m_pForeignData = 0;
		m_iForeignData = m_iForeignFlags = 0;
	}

	virtual CPhysicalEntity *GetEntity();
	virtual CPhysicalEntity *GetEntityFast() { return (CPhysicalEntity*)m_pEntBuddy; }

	virtual int AddRef() { return 0; }
	virtual int Release() { return 0; }

	virtual pe_type GetType();
	virtual int SetParams(pe_params* params,int bThreadSafe=1);
	virtual int GetParams(pe_params* params);
	virtual int GetStatus(pe_status* status);
	virtual int Action(pe_action* action,int bThreadSafe=1);

	virtual int AddGeometry(phys_geometry *pgeom, pe_geomparams* params,int id=-1,int bThreadSafe=1);
	virtual void RemoveGeometry(int id,int bThreadSafe=1);

	virtual void *GetForeignData(int itype=0) { return m_iForeignData==itype ? m_pForeignData : 0; }
	virtual int GetiForeignData() { return m_iForeignData; }

	virtual int GetStateSnapshot(class CStream &stm, float time_back=0, int flags=0);
	virtual int GetStateSnapshot(TSerialize ser, float time_back=0, int flags=0);
	virtual int SetStateFromSnapshot(class CStream &stm, int flags=0);
	virtual int SetStateFromSnapshot(TSerialize ser, int flags=0);
	virtual int SetStateFromTypedSnapshot(TSerialize ser, int type, int flags=0);
	virtual int PostSetStateFromSnapshot();
	virtual int GetStateSnapshotTxt(char *txtbuf,int szbuf, float time_back=0);
	virtual void SetStateFromSnapshotTxt(const char *txtbuf,int szbuf);
	virtual unsigned int GetStateChecksum();

	virtual void StartStep(float time_interval);
	virtual int Step(float time_interval);
	virtual int DoStep(float time_interval,int iCaller) { return 1; }
	virtual void StepBack(float time_interval);
	virtual IPhysicalWorld *GetWorld();

	virtual void GetMemoryStatistics(ICrySizer *pSizer) {};

	Vec3 m_BBox[2];

	void *m_pForeignData;
	int m_iForeignData  : 16;
	int m_iForeignFlags : 16;

	struct vec2dpacked {
		int x : 16;
		int y : 16;
	};
	vec2dpacked m_ig[2];
	int m_iGThunk0;

	CPhysicalPlaceholder *m_pEntBuddy;
	volatile unsigned int m_bProcessed;
	int m_id : 24;
	int m_iSimClass : 8;
	volatile int m_lockUpdate;
};

#endif