//////////////////////////////////////////////////////////////////////
//
//	Physical Area
//	
//	File: physarea.cpp
//	Description : CPhysArea class implementation
//
//	History:
//	-:Created by Anton Knyazev
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "geoman.h"
#include "bvtree.h"
#include "geometry.h"
#include "overlapchecks.h"
#include "raybv.h"
#include "raygeom.h"
#include "rigidbody.h"
#include "physicalplaceholder.h"
#include "physicalentity.h"
#include "physicalworld.h"
#include "trimesh.h"
#include "ISerialize.h"
#include "GeomQuery.h"

class CPhysicalStub : public IPhysicalEntity {
public:
	CPhysicalStub() {}

	virtual pe_type GetType() { return PE_STATIC; }
	virtual int AddRef() { return 0; }
	virtual int Release() { return 0; }
	virtual int SetParams(pe_params* params,int bThreadSafe=1) { return 0; }
	virtual int GetParams(pe_params* params) { return 0; }
	virtual int GetStatus(pe_status* status) { return 0; }
	virtual int Action(pe_action* action,int bThreadSafe=1) { return 0; }

	virtual int AddGeometry(phys_geometry *pgeom, pe_geomparams* params,int id=-1,int bThreadSafe=1) { return -1; }
	virtual void RemoveGeometry(int id,int bThreadSafe=1) {}
	virtual float ComputeExtent(GeomQuery& geo, EGeomForm eForm) { return 0.f; }
	virtual void GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm) { ran.vPos.zero(); ran.vNorm.zero(); }

	virtual void *GetForeignData(int itype=0) { return 0; }
	virtual int GetiForeignData() { return 0; }

	virtual int GetStateSnapshot(class CStream &stm, float time_back=0, int flags=0) { return 0; }
	virtual int GetStateSnapshot(TSerialize ser, float time_back=0, int flags=0) { return 0; }
	virtual int SetStateFromSnapshot(class CStream &stm, int flags=0) { return 0; }
	virtual int SetStateFromSnapshot(TSerialize ser, int flags=0) { return 0; }
	virtual int SetStateFromTypedSnapshot(TSerialize ser, int type, int flags/* =0 */) {return 0;}
	virtual int PostSetStateFromSnapshot() { return 0; }
	virtual int GetStateSnapshotTxt(char *txtbuf,int szbuf, float time_back=0) { return 0; }
	virtual void SetStateFromSnapshotTxt(const char *txtbuf,int szbuf) {}
	virtual unsigned int GetStateChecksum() { return 0; }

	virtual void StartStep(float time_interval) {}
	virtual int Step(float time_interval) { return 0; }
	virtual int DoStep(float time_interval, int iCaller) { return 0; }
	virtual void StepBack(float time_interval) {}
	virtual IPhysicalWorld *GetWorld() { return 0; }

	virtual void GetMemoryStatistics(ICrySizer *pSizer) {}
};
CPhysicalStub g_stub;

static void SignalEventAreaChange( CPhysicalWorld* pWorld, CPhysArea* pArea )
{
	EventPhysAreaChange epac;
	epac.pEntity = pArea;
	epac.boxAffected[0] = pArea->m_BBox[0];
	epac.boxAffected[1] = pArea->m_BBox[1];
	pWorld->SignalEvent(&epac,0);
}

CPhysArea::CPhysArea(CPhysicalWorld *pWorld) 
{ 
	m_pWorld=pWorld; m_npt=0; m_pt=0; m_ptSpline=0; m_pGeom=0; m_next=m_nextBig=0; 
	m_iSimClass=5; MARK_UNUSED m_gravity,m_pb.waterPlane.origin,m_pb.iMedium; m_size.zero(); m_rsize.zero(); m_bUniform=1; m_damping=0;
	m_falloff0=0; m_rfalloff0=1;
	m_iGThunk0 = 0; m_pEntBuddy = 0;
	m_R.SetIdentity(); m_scale=m_rscale = 1;
	m_offset0.zero(); m_R0.SetIdentity();
	m_offset.zero(); 
	m_ptLastCheck.Set(1E10f,1E10f,1E10f);
	m_BBox[0].zero(); m_BBox[1].zero();
	m_bUseCallback = 0;
	m_pFlows = 0;
	m_bDeleted = 0;
	m_lockRef = 0;
}

CPhysArea::~CPhysArea() 
{ 
	if (m_pt) { 
		delete[] m_pt; delete[] m_idxSort[0]; delete[] m_idxSort[1]; delete[] m_pMask; 
	}
	if (m_ptSpline)
		delete[] m_ptSpline;
	if (m_pGeom)
		m_pGeom->Release();
	if (m_pFlows)
		delete[] m_pFlows;
}


CPhysicalEntity *CPhysArea::GetEntity() 
{
	return (CPhysicalEntity*)&g_stub; 
}


int CPhysArea::ApplyParams(const Vec3& pt, Vec3& gravity,const Vec3 &vel, pe_params_buoyancy *pbdst,int nBuoys,int nMaxBuoys,int &iMedium0, 
													 IPhysicalEntity *pent)
{
	Vec3 ptloc = ((pt-m_offset)*m_R)*m_rscale;
	int bRes=1,bUseBuoy=0;

	if (!is_unused(m_pb.waterPlane.origin) && 
			!(pent && ((CPhysicalEntity*)pent)->m_flags & pef_ignore_ocean && this==m_pWorld->m_pGlobalArea)) 
	{
		if (m_pb.iMedium==iMedium0)
			nBuoys=0,iMedium0=-1,bRes=0;
		if (nBuoys<nMaxBuoys)
			pbdst[nBuoys]=m_pb, bUseBuoy=1;
	}
	if (!is_unused(m_gravity) && is_unused(gravity)) 
		gravity.zero();

	if (m_ptSpline && !m_bUniform) {
		Vec3 p0,p1,p2,v0,v1,v2,gcent(ZERO),gpull;
		int iClosestSeg; float tClosest,mindist;
		mindist = FindSplineClosestPt(ptloc, iClosestSeg,tClosest);
		p0 = m_ptSpline[max(0,iClosestSeg-1)]; p1 = m_ptSpline[iClosestSeg]; p2 = m_ptSpline[min(m_npt-1,iClosestSeg+1)];
		v2 = (p0+p2)*0.5f-p1; v1 = p1-p0; v0 = (p0+p1)*0.5f;
		if (iClosestSeg+tClosest>0 && iClosestSeg+tClosest<m_npt)
			gcent = ((v2*tClosest+v1)*tClosest+v0-ptloc).normalized();
		gpull = (v2*max(0.01f,min(1.99f,2*tClosest))+v1).normalized();
		if (!m_bUseCallback) {
			float t = sqr(1-sqrt_tpl(mindist)/m_zlim[0]);
			t = t*(1-m_falloff0)+m_falloff0;
			if (!is_unused(m_gravity))
				gravity += m_R*(gpull*t+gcent*(1-t))*m_gravity.z-vel*m_damping;
			if (bUseBuoy)
				pbdst[nBuoys].waterFlow = m_R*(gpull*t+gcent*(1-t))*m_pb.waterFlow.z;
		} else {
			EventPhysArea epa;
			epa.pt=pt; epa.gravity=m_gravity; epa.pb=m_pb; epa.pent=pent;
			epa.ptref=(v2*tClosest+v1)*tClosest+v0;
			epa.dirref=gpull;
			m_pWorld->SignalEvent(&epa,0);
			if (is_unused(epa.gravity))
				gravity += epa.gravity;
			if (!is_unused(epa.pb.waterPlane.origin) && nBuoys<nMaxBuoys)
				pbdst[nBuoys] = epa.pb;
		}
	}	else {
		float dist = sqr(ptloc.x*m_rsize.x)+sqr(ptloc.y*m_rsize.y)+sqr(ptloc.z*m_rsize.z);
		if (dist>1)
			bRes = 0;
		else if (dist<1) if (!m_bUseCallback) {
			if (dist>0)
				dist = sqrt_tpl(dist);
			dist = 1-max(0.0f,dist-m_falloff0)*m_rfalloff0;
			if (m_bUniform) {
				if (!is_unused(m_gravity)) {
					if (m_bUniform==2)
						gravity += m_R*m_gravity*dist;
					else
						gravity += m_gravity*dist;
				}
			}	else {
				Vec3 dir = (pt-m_offset).normalized();
				if (!is_unused(m_gravity))
					gravity += dir*(dist*m_gravity.z);	
				if (bUseBuoy)
					pbdst[nBuoys].waterFlow = dir*m_pb.waterFlow.z;
			}
			if (bUseBuoy)	{
				if (m_pGeom && m_pGeom->GetType()==GEOM_HEIGHTFIELD) {
					if (pent && m_pb.waterPlane.origin.z>-1000.0f) {
						heightfield *phf = (heightfield*)m_pGeom->GetData();
						CPhysicalPlaceholder *ppc = (CPhysicalPlaceholder*)pent;
						int i,j,istep,jstep,npt;
						Vec3 sz,org,n=m_pb.waterPlane.n;
						float det,maxdet;
						Matrix33 Rabs,C,tmp;
						vector2di ibbox[2];
						m_pb.waterPlane.n;
						m_pb.waterPlane.origin;
						org = (ppc->m_BBox[0]+ppc->m_BBox[1])*0.5f;
						sz = (ppc->m_BBox[1]-ppc->m_BBox[0])*0.5f;
						if (fabs_tpl((org-m_pb.waterPlane.origin)*n)-sz*n.abs() < 1.5f) {
							org = ((org-m_offset)*m_R)*m_rscale;
							sz = (Rabs=m_R).Fabs()*sz;
							ibbox[0].x = max(0, min(phf->size.x-1, float2int((org.x-sz.x)*phf->stepr.x-0.5f)));
							ibbox[0].y = max(0, min(phf->size.y-1, float2int((org.y-sz.y)*phf->stepr.y-0.5f)));
							ibbox[1].x = max(0, min(phf->size.x-1, float2int((org.x+sz.x)*phf->stepr.x+0.5f)));
							ibbox[1].y = max(0, min(phf->size.y-1, float2int((org.y+sz.y)*phf->stepr.y+0.5f)));
							org.zero(); C.SetZero();
							istep=(ibbox[1].x-ibbox[0].x>>2)+1; jstep=(ibbox[1].y-ibbox[0].y>>2)+1;
							for(i=ibbox[0].x,npt=0;i<=ibbox[1].x;i+=istep) for(j=ibbox[0].y;j<=ibbox[1].y;j+=jstep,npt++) {
								sz.Set((i-ibbox[0].x)*phf->step.x, (j-ibbox[0].y)*phf->step.y, phf->fpGetHeightCallback(i,j));
								C += dotproduct_matrix(sz,sz,tmp); org += sz;
							}
							org /= npt;
							C -= dotproduct_matrix(org,org,tmp)*(float)npt;
							org += Vec3(ibbox[0].x*phf->step.x, ibbox[0].y*phf->step.y, 0);

							for(i=0,j=-1,maxdet=0;i<3;i++) {
								det = C(inc_mod3[i],inc_mod3[i])*C(dec_mod3[i],dec_mod3[i]) - C(dec_mod3[i],inc_mod3[i])*C(inc_mod3[i],dec_mod3[i]);
								if (fabs(det)>fabs(maxdet)) 
									maxdet=det, j=i;
							}
							if (j>=0) {
								det = 1.0/maxdet;
								n[j] = 1;
								n[inc_mod3[j]] = -(C(inc_mod3[j],j)*C(dec_mod3[j],dec_mod3[j]) - C(dec_mod3[j],j)*C(inc_mod3[j],dec_mod3[j]))*det;
								n[dec_mod3[j]] = -(C(inc_mod3[j],inc_mod3[j])*C(dec_mod3[j],j) - C(dec_mod3[j],inc_mod3[j])*C(inc_mod3[j],j))*det;
								n = m_R*n.normalized();

								pbdst[nBuoys].waterPlane.origin = m_R*org*m_scale+m_offset;
								pbdst[nBuoys].waterPlane.n = n*sgnnz(n*pbdst[nBuoys].waterPlane.n);
							}
						}
					}
				}	else if (m_pt && m_pGeom) {
					pbdst[nBuoys].waterPlane.origin = m_R*m_trihit.pt[0]+m_offset;
					pbdst[nBuoys].waterPlane.n = m_R*m_trihit.n;
					if (m_pFlows) {
						CTriMesh *pMesh = (CTriMesh*)m_pGeom;
						float rarea = 1.0f/(pMesh->m_pVertices[pMesh->m_pIndices[m_trihit.idx*3+1]]-pMesh->m_pVertices[pMesh->m_pIndices[m_trihit.idx*3]] ^ 
																pMesh->m_pVertices[pMesh->m_pIndices[m_trihit.idx*3+2]]-pMesh->m_pVertices[pMesh->m_pIndices[m_trihit.idx*3]]).len(); 
						pbdst[nBuoys].waterFlow.zero();
						for(int i=0;i<3;i++)
							pbdst[nBuoys].waterFlow += m_pFlows[pMesh->m_pIndices[m_trihit.idx*3+i]]*(rarea*
								((pMesh->m_pVertices[pMesh->m_pIndices[m_trihit.idx*3+i]]-ptloc ^ 
								  pMesh->m_pVertices[pMesh->m_pIndices[m_trihit.idx*3+inc_mod3[i]]]-ptloc)*m_trihit.n));
						pbdst[nBuoys].waterFlow = m_R*pbdst[nBuoys].waterFlow;
					}
				}
				if (m_bUniform==2)
					pbdst[nBuoys].waterFlow = m_R*pbdst[nBuoys].waterFlow*dist;
				else
					pbdst[nBuoys].waterFlow *= dist;
			}	else if (m_damping>0 && nBuoys<nMaxBuoys)	{
				pbdst[nBuoys].iMedium = 1;
				pbdst[nBuoys].waterPlane.origin.Set(0,0,1E8f);
				pbdst[nBuoys].waterPlane.n.Set(0,0,1);
				pbdst[nBuoys].waterDamping = m_damping;
				pbdst[nBuoys].waterDensity = 0;
				pbdst[nBuoys].waterResistance = 0;
				pbdst[nBuoys].waterFlow.zero();
				bUseBuoy = 1;
			}
		}	else {
			EventPhysArea epa;
			epa.pt=pt; epa.gravity=m_gravity; epa.pb=m_pb; epa.pent=pent;
			epa.ptref=m_offset; epa.dirref.zero();
			m_pWorld->SignalEvent(&epa,0);
			if (is_unused(epa.gravity))
				gravity += epa.gravity;
			if (!is_unused(epa.pb.waterPlane.origin) && nBuoys<nMaxBuoys)
				pbdst[nBuoys] = epa.pb;
		}
	}

	return bRes&bUseBuoy;
}


/*void CPhysArea::UpdateGravity(const Vec3& pt, Vec3& gravity, const Vec3 &vel)
{
	if (is_unused(m_gravity))
		return;
	if (is_unused(gravity)) gravity.zero();
	Vec3 ptloc = ((pt-m_offset)*m_R)*m_rscale;

	if (m_ptSpline && !m_bUniform) {
		Vec3 p0,p1,p2,v0,v1,v2,gcent(ZERO),gpull;
		FindSplineClosestPt(ptloc);
		p0 = m_ptSpline[max(0,m_iClosestSeg-1)]; p1 = m_ptSpline[m_iClosestSeg]; p2 = m_ptSpline[min(m_npt-1,m_iClosestSeg+1)];
		v2 = (p0+p2)*0.5f-p1; v1 = p1-p0; v0 = (p0+p1)*0.5f;
		if (m_iClosestSeg+m_tClosest>0 && m_iClosestSeg+m_tClosest<m_npt)
			gcent = ((v2*m_tClosest+v1)*m_tClosest+v0-ptloc).normalized();
		gpull = (v2*max(0.01f,min(1.99f,2*m_tClosest))+v1).normalized();
		float t = sqr(1-sqrt_tpl(m_mindist)/m_zlim[0]);
    t = t*0.2f+0.8f;
		gravity += m_R*(gpull*t+gcent*(1-t))*m_gravity.len()-vel*m_damping;
	}	else {
		float dist = sqr(ptloc.x*m_rsize.x)+sqr(ptloc.y*m_rsize.y)+sqr(ptloc.z*m_rsize.z);
		if (dist<1) {
			Vec3 gloc = m_bUniform ? m_gravity : (pt-m_offset).normalized()*m_gravity.z;
			if (dist>0)
				dist = sqrt_tpl(dist);
			gravity += gloc*(1-max(0.0f,dist-m_falloff0)*m_rfalloff0);
		}
	}
}

int CPhysArea::UpdateBuoyancyParams(const Vec3 &pt, pe_params_buoyancy *pbdst,int nBuoys,int nMaxBuoys,int &iMedium0)
{
	int bRes=0;
	if (!is_unused(m_pb.waterPlane.origin)) {
		if (m_pb.iMedium==iMedium0)
			nBuoys=0,iMedium0=-1;
		else bRes=1;
		if (nBuoys<nMaxBuoys)	{
			pbdst[nBuoys] = m_pb;
			Vec3 ptloc = ((pt-m_offset)*m_R)*m_rscale;
			if (m_ptSpline && !m_bUniform) {
				Vec3 p0,p1,p2,v0,v1,v2,vcent(ZERO),vpull;
				FindSplineClosestPt(ptloc);
				p0 = m_ptSpline[max(0,m_iClosestSeg-1)]; p1 = m_ptSpline[m_iClosestSeg]; p2 = m_ptSpline[min(m_npt-1,m_iClosestSeg+1)];
				v2 = (p0+p2)*0.5f-p1; v1 = p1-p0; v0 = (p0+p1)*0.5f;
				if (m_iClosestSeg+m_tClosest>0 && m_iClosestSeg+m_tClosest<m_npt)
					vcent = ((v2*m_tClosest+v1)*m_tClosest+v0-ptloc).normalized();
				vpull = (v2*max(0.01f,min(1.99f,2*m_tClosest))+v1).normalized();
				float t = sqr(1-sqrt_tpl(m_mindist)/m_zlim[0]);
				t = t*0.1f+0.9f;
				pbdst[nBuoys].waterFlow = m_R*(vpull*t+vcent*(1-t))*m_pb.waterFlow.len();
			}	else {
				if (!m_bUniform)
					pbdst[nBuoys].waterFlow = (pt-m_offset).normalized()*m_pb.waterFlow.z;
				float dist = sqr(ptloc.x*m_rsize.x)+sqr(ptloc.y*m_rsize.y)+sqr(ptloc.z*m_rsize.z);
				if (dist>0)
					pbdst[nBuoys].waterFlow *= 1-max(0.0f,sqrt_tpl(dist)-m_falloff0)*m_rfalloff0;
			}
		}
	}
	return bRes;
}*/


int CPhysArea::CheckPoint(const Vec3& pttest)
{
	Vec3 ptloc = ((pttest-m_offset)*m_rscale)*m_R;
	int iClosestSeg; float tClosest;

	if (m_pGeom && !m_pt)
		return m_pGeom->PointInsideStatus(ptloc);
	else if (m_pt) {
		int i,iend,i1,istart[2],ilim[2],bInside=0;
		quotientf y,ymax;
		Vec2 dp;
		if (!inrange(ptloc.z, m_zlim[0],m_zlim[1]))
			return 0;

		for(iend=0; iend<2; iend++) {
			ilim[0]=-1; ilim[1]=m_npt;
			do {
				i = ilim[0]+ilim[1]>>1;
				i1 = m_idxSort[iend][i]+1; i1 &= i1-m_npt>>31;
				ilim[isneg(ptloc.x-minmax(m_pt[m_idxSort[iend][i]].x,m_pt[i1].x,iend))] = i;
			}	while(ilim[1] > ilim[0]+1);
			istart[iend] = ilim[1];
		}
		if (istart[0]-1>>31 | m_npt-1-istart[1]>>31)
			return 0;

		for(i=0; i<istart[0]; i++) 
			m_pMask[m_idxSort[0][i]>>5] |= 1u<<(m_idxSort[0][i]&31);
		for(i=istart[1],ymax=1E10f; i<m_npt; i++) if (m_pMask[m_idxSort[1][i]>>5] & 1u<<(m_idxSort[1][i]&31)) {
			dp = m_pt[m_idxSort[1][i]+1 & m_idxSort[1][i]-m_npt+1>>31]-m_pt[m_idxSort[1][i]];
			y.set(((ptloc.x-m_pt[m_idxSort[1][i]].x)*dp.y+m_pt[m_idxSort[1][i]].y*dp.x)*dp.x, sqr(dp.x));
			if (y>ptloc.y && y<ymax) {
				ymax = y; bInside = isneg(dp.x);
			}
		}
		for(i=0;i<istart[0];i++) 
			m_pMask[m_idxSort[0][i]>>5] ^= 1u<<(m_idxSort[0][i]&31);
		if (bInside && m_pGeom)
			bInside &= ((CTriMesh*)m_pGeom)->PointInsideStatusMesh(Vec3(ptloc.x,ptloc.y,ptloc.z-(m_zlim[1]-m_zlim[0])),&m_trihit);

		return bInside;
	} else if (m_ptSpline)
		return isneg(FindSplineClosestPt(ptloc,iClosestSeg,tClosest)-sqr(m_zlim[0]));

	return 0;
}


float CPhysArea::FindSplineClosestPt(const Vec3 &ptloc, int &iClosestSegRet,float &tClosestRet)
{
	int i,j,iClosestSeg;
	Vec3 p0,p1,p2,v0,v1,v2,BBox[2];
	float dist,mindist,tClosest;
	real t[4];
	{ ReadLock lock(m_lockUpdate); 
		if ((ptloc-m_ptLastCheck).len2()==0) {
			iClosestSegRet = m_iClosestSeg; tClosestRet = m_tClosest;
			return m_mindist;
		}
	}
	mindist = (ptloc-m_ptSpline[0]).len2(); iClosestSeg=0; tClosest=0;
	if ((dist=(ptloc-m_ptSpline[m_npt-1]).len2())<mindist)
		mindist=dist, iClosestSeg=m_npt-1, tClosest=1;

	for(i=0;i<m_npt;i++) {
		p0 = m_ptSpline[max(0,i-1)]; p1 = m_ptSpline[i]; p2 = m_ptSpline[min(m_npt-1,i+1)];
		BBox[0] = min(min(p0,p1),p2);	BBox[0] -= Vec3(1,1,1)*m_zlim[0];
		BBox[1] = max(max(p0,p1),p2);	BBox[1] += Vec3(1,1,1)*m_zlim[0];
		if (PtInAABB(BBox,ptloc)) {
			v2 = (p0+p2)*0.5f-p1; v1 = p1-p0; v0 = (p0+p1)*0.5f-ptloc;
			for(j=(P3((v2*v2)*2)+P2((v2*v1)*3)+P1(v1*v1+(v2*v0)*2)+real(v1*v0)).findroots(0,1,t)-1;j>=0;j--)
				if ((dist=((v2*t[j]+v1)*t[j]+v0).len2())<mindist)
					mindist=dist, iClosestSeg=i, tClosest=t[j];
		}
	}

	{ WriteLock lock(m_lockUpdate); 
		m_ptLastCheck=ptloc; 
		m_mindist=mindist; m_iClosestSeg=iClosestSeg; m_tClosest=tClosest;
		iClosestSegRet=m_iClosestSeg; tClosestRet=m_tClosest;
	}
	
	return mindist;
}


int CPhysArea::FindSplineClosestPt(const Vec3 &org, const Vec3 &dir, Vec3 &ptray, Vec3 &ptspline)
{
	int i,j;
	Vec3 p0,p1,p2,v0,v1,v2,v0d,v1d,v2d,pt,ptmin,ptmax;
	float dist,mindist,h,dlen2=dir.len2();
	box bbox;
	ray aray;
	real t[4];
	mindist=(m_ptSpline[0]-org).len2(); ptray=org; ptspline=m_ptSpline[0];
	if ((dist=(m_ptSpline[0]-org-dir).len2()) < mindist)
		mindist=dist, ptray=org+dir;
	if ((dist=(m_ptSpline[m_npt-1]-org).len2()) < mindist)
		mindist=dist, ptray=org, ptspline=m_ptSpline[m_npt-1];
	if ((dist=(m_ptSpline[m_npt-1]-org-dir).len2()) < mindist)
		mindist=dist, ptray=org+dir, ptspline=m_ptSpline[m_npt-1];
	mindist *= dlen2;	ptray *= dlen2;
	bbox.Basis.SetIdentity();
	bbox.bOriented = 0;
	aray.dir = dir;
	aray.origin = org;

	for(i=0;i<m_npt;i++) {
		p0 = m_ptSpline[max(0,i-1)]; p1 = m_ptSpline[i]; p2 = m_ptSpline[min(m_npt-1,i+1)];
		ptmin = min(min(p0,p1),p2);	ptmax = max(max(p0,p1),p2);	
		bbox.center = (ptmin+ptmax)*0.5f;
		bbox.size = (ptmax-ptmin)*0.5f+Vec3(1,1,1)*m_zlim[0];
		if (box_ray_overlap_check(&bbox,&aray)) {
			v2 = (p0+p2)*0.5f-p1; v1 = p1-p0; v0 = (p0+p1)*0.5f-org-dir;
			for(j=(P3((v2*v2)*2)+P2((v2*v1)*3)+P1(v1*v1+(v2*v0)*2)+real(v1*v0)).findroots(0,1,t)-1;j>=0;j--)
				if ((dist=(pt=(v2*t[j]+v1)*t[j]+v0).len2()*dlen2)<mindist)
					mindist=dist, ptspline=pt+org+dir, ptray=(org+dir)*dlen2;
			v0 += dir;
			for(j=(P3((v2*v2)*2)+P2((v2*v1)*3)+P1(v1*v1+(v2*v0)*2)+real(v1*v0)).findroots(0,1,t)-1;j>=0;j--)
				if ((dist=(pt=(v2*t[j]+v1)*t[j]+v0).len2()*dlen2)<mindist)
					mindist=dist, ptspline=pt+org, ptray=org*dlen2;
			v2d = v2^dir; v1d = v1^dir; v0d = v0^dir;
			for(j=(P3((v2d*v2d)*2)+P2((v2d*v1d)*3)+P1(v1d*v1d+(v2d*v0d)*2)+real(v1d*v0d)).findroots(0,1,t)-1;j>=0;j--) 
				if (inrange(h=(pt=(v2*t[j]+v1)*t[j]+v0)*dir,0.0f,dlen2) && (dist=(pt^dir).len2())<mindist)
					mindist=dist, ptspline=pt+org, ptray=org*dlen2+dir*h;
		}
	}

	if (mindist < sqr(m_zlim[0])*dlen2) {
		ptray /= dlen2; return 1;
	}
	return 0;
}


int CPhysArea::RayTrace(const Vec3 &org, const Vec3 &dir, ray_hit *pHit, pe_params_pos *pp)
{
	geom_world_data gwd;
	geom_contact *pcontacts;
	intersection_params ip;
	int ncont,iClosestSeg;
	float tClosest;
	if (pp) {
		quaternionf qrot;
		gwd.offset = pp->pos; qrot = pp->q; 
		if (!is_unused(pp->scale)) gwd.scale = pp->scale;
		get_xqs_from_matrices(pp->pMtx3x4,pp->pMtx3x3, gwd.offset,qrot,gwd.scale);
		gwd.R = Matrix33(qrot);
	} else {
		gwd.R = m_R;
		gwd.offset = m_offset;
		gwd.scale = m_scale;
	}
	pHit->pCollider = this; 
	pHit->partid=pHit->surface_idx=pHit->idmatOrg=pHit->foreignIdx = 0;

	if (m_pGeom && m_pGeom->GetType()==GEOM_TRIMESH) {
		CRayGeom aray(org,dir);
		ncont = m_pGeom->Intersect(&aray,&gwd,0,&ip,pcontacts);
		WriteLockCond lockColl(*ip.plock,0); lockColl.SetActive(isneg(-ncont));
		for(; ncont>0 && pcontacts[ncont-1].n*dir>0; ncont--);
		if (ncont>0) {
			pHit->dist = pcontacts[ncont-1].t;
			pHit->pt = pcontacts[ncont-1].pt; 
			pHit->n = pcontacts[ncont-1].n;
			return 1;
		}
	}	else if (m_npt>0 && m_ptSpline && m_pb.iMedium!=0) {
		Vec3 ptray,ptspline;
		float rscale = gwd.scale==1.0f ? 1.0f:1.0f/gwd.scale;
		if (FindSplineClosestPt(((org-gwd.offset)*rscale)*gwd.R, dir*gwd.R, ptray,ptspline)) {
			pHit->pt = gwd.R*ptray*gwd.scale+gwd.offset;
			pHit->n = gwd.R*(ptray-ptspline).normalized();
			pHit->dist = (pHit->pt-org)*dir.normalized();
			return 1;
		}
	}	else if (m_pb.iMedium==0) {
		quotientf t((m_pb.waterPlane.origin-org)*m_pb.waterPlane.n, dir*m_pb.waterPlane.n);
		if (inrange(t.x, 0.0f,t.y)) {
			pHit->dist = (t.x/=t.y)*dir.len();
			pHit->pt = org+dir*t.x;
			pHit->n = m_pb.waterPlane.n;
			if (m_pGeom) {
				if (m_pGeom->GetType()!=GEOM_HEIGHTFIELD)
					return m_pGeom->PointInsideStatus(((pHit->pt-gwd.offset)*gwd.R)*(gwd.scale==1.0f?1.0f:1.0f/gwd.scale));
				else {
					heightfield *phf = (heightfield*)m_pGeom->GetData();
					CRayGeom aray(org,dir);
					if (dir.len2()>sqr((phf->step.x+phf->step.y)*3*m_scale)) {
						aray.m_ray.dir = aray.m_dirn*((phf->step.x+phf->step.y)*m_scale*3);
						aray.m_ray.origin = pHit->pt-aray.m_ray.dir*0.5f;
					}
					if (ncont = m_pGeom->Intersect(&aray,&gwd,0,&ip,pcontacts)) {
						WriteLockCond lockColl(*ip.plock,0); lockColl.SetActive();
						pHit->pt = pcontacts[ncont-1].pt;
						pHit->dist = (pHit->pt-org)*aray.m_dirn;
						pHit->n = pcontacts[ncont-1].n;
					}
					return 1;
				}
			}	else if (m_npt>0 && m_ptSpline)
				return FindSplineClosestPt(((pHit->pt-gwd.offset)*gwd.R)/gwd.scale,iClosestSeg,tClosest) < m_zlim[0];
			else if (m_pt)
				return CheckPoint(pHit->pt);
			else
				return 1;
		}
	}
	return 0;
}

float CPhysArea::ComputeExtent(GeomQuery& geo, EGeomForm eForm)
{
	float extent = 0.f;
	if (m_pt) {
		assert(0);
	} else if (m_ptSpline) {
		float cross = CircleExtent(EGeomForm(eForm-1), m_zlim[0]);
		if (cross > 0.f)
		{
			geo.AllocParts(m_npt-1);
			for (int i = 0; i < m_npt-1; i++)
			{
				// approximate spline segment lengths with point distance.
				float part_extent = (m_ptSpline[i+1] - m_ptSpline[i]).GetLength();
				part_extent *= cross;
				geo.GetPart(i).SetExtent(part_extent);
				extent += part_extent;
			}
		}
	}
	else if (m_pGeom)
		extent = geo.GetExtent(m_pGeom, eForm);

	return extent * ScaleExtent(eForm, m_scale);
}

void CPhysArea::GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm)
{
	if (m_pt) {
		assert(0);
	} else if (m_ptSpline) {
		// choose random segment.
		geo.GetExtent(this, eForm);
		int i = geo.GetRandomPart();

		// generate random point along segment; compute tangent as well
		float t = physics_frand(1.f);
		if (t <= 0.5f)
			i++;

		Vec3 tang;
		if (i==0)
		{
			tang = m_ptSpline[1]-m_ptSpline[0];
			ran.vPos = m_ptSpline[0] + tang * (t-0.5f);
		}
		else if (i==m_npt-1)
		{
			tang = m_ptSpline[m_npt-1]-m_ptSpline[m_npt-2];
			ran.vPos = m_ptSpline[m_npt-2] + tang * (t+0.5f);
		}
		else
		{
			Vec3 const& p0 = m_ptSpline[i-1];
			Vec3 const& p1 = m_ptSpline[i];
			Vec3 const& p2 = m_ptSpline[i+1];
			Vec3 v2 = (p0+p2)*0.5f-p1,
					 v1 = p1-p0,
					 v0 = (p0+p1)*0.5f;
			ran.vPos = (v2*t+v1)*t+v0;
			tang = v2*(2.f*t)+v1;
		}

		// compute radial axes from tangent
		Vec3 x( max(abs(tang.y),abs(tang.z)), max(abs(tang.z),abs(tang.x)), max(abs(tang.x),abs(tang.y)) );
		Vec3 y = tang ^ x;
		x.Normalize();
		y.Normalize();

		// generate random displacement from spline.
		Vec2 xy = CircleRandomPoint(EGeomForm(eForm-1), m_zlim[0]);
		Vec3 dis = x * xy.x + y * xy.y;
		
		ran.vPos += dis;
		ran.vNorm = dis.normalized();
	}
	else if (m_pGeom)
		m_pGeom->GetRandomPos(ran, geo, eForm);

	ran.vPos = m_R*ran.vPos * m_scale + m_offset;
	ran.vNorm = m_R*ran.vNorm;
}


int CPhysArea::SetParams(pe_params *_params, int bThreadSafe)
{
	ChangeRequest<pe_params> req(this,m_pWorld,_params,bThreadSafe);
	if (req.IsQueued())
		return 1;

	if (_params->type==pe_params_pos::type_id) {
		pe_params_pos *params = (pe_params_pos*)_params;
		WriteLock lock(m_pWorld->m_lockAreas);
		SignalEventAreaChange(m_pWorld, this);
		get_xqs_from_matrices(params->pMtx3x4,params->pMtx3x3, params->pos,params->q,params->scale);
		Vec3 pos,sz;
		if (!is_unused(params->pos)) m_offset = m_offset0+params->pos;
		if (!is_unused(params->q)) m_R = Matrix33(params->q)*m_R0;
		if (!is_unused(params->scale)) m_rscale = 1/(m_scale = params->scale);
		if (m_pGeom) {
			box abox;
			m_pGeom->GetBBox(&abox);
			abox.Basis *= m_R.T();
			sz = (abox.size*abox.Basis.Fabs())*m_scale;
			pos = m_offset + m_R*abox.center*m_scale;
			m_BBox[0] = pos-sz; m_BBox[1] = pos+sz;
		} else if (m_pt) {
			sz = Matrix33(m_R).Fabs()*m_size0*m_scale;
			pos = (m_offset-m_offset0) + (m_R*m_R0.T())*m_offset0*m_scale;
			m_BBox[0] = pos-sz; m_BBox[1] = pos+sz;
		}	else if (m_ptSpline) {
			m_BBox[0]=m_BBox[1] = m_R*m_ptSpline[0]*m_scale+m_offset;
			for(int i=0;i<m_npt;i++) {
				Vec3 ptw = m_R*m_ptSpline[i]*m_scale+m_offset;
				m_BBox[0] = min(m_BBox[0],ptw); m_BBox[1] = max(m_BBox[1],ptw);
			}
			m_BBox[0] -= Vec3(m_zlim[0],m_zlim[0],m_zlim[0]);
			m_BBox[1] += Vec3(m_zlim[0],m_zlim[0],m_zlim[0]);
		}
		m_pWorld->RepositionArea(this);
		return 1;
	}

	if (_params->type==pe_simulation_params::type_id) {
		pe_simulation_params *params = (pe_simulation_params*)_params;
		if (!is_unused(params->gravity)) {
			m_gravity = params->gravity;
			if (m_pWorld->m_pGlobalArea==this)
				m_pWorld->m_vars.gravity = m_gravity;
		}
		if (!is_unused(params->damping)) m_damping = params->damping;
		SignalEventAreaChange(m_pWorld, this);
		return 1;
	}

	if (_params->type==pe_params_area::type_id) {
		pe_params_area *params = (pe_params_area*)_params;
		if (!is_unused(params->gravity)) {
			m_gravity = params->gravity;
			if (m_pWorld->m_pGlobalArea==this)
				m_pWorld->m_vars.gravity = m_gravity;
		}
		if (!is_unused(params->size)) {
			m_size = params->size;
			m_rsize.Set(m_size.x>0 ? 1/m_size.x:0, m_size.y>0 ? 1/m_size.y:0, m_size.z>0 ? 1/m_size.z:0);
		}
		if (!is_unused(params->bUniform)) m_bUniform = params->bUniform;
		if (!is_unused(params->falloff0)) 
			m_rfalloff0 = (m_falloff0=params->falloff0)<1.0f ? 1.0f/(1-params->falloff0):0;
		if (!is_unused(params->damping)) m_damping = params->damping;
		if (!is_unused(params->bUseCallback)) m_bUseCallback = params->bUseCallback;
		if (!is_unused(params->pGeom) && params->pGeom!=m_pGeom) {
			if (m_pGeom) m_pGeom->Release();
			m_pGeom = params->pGeom;
		}
		SignalEventAreaChange(m_pWorld, this);
		return 1;
	}

	if (_params->type==pe_params_buoyancy::type_id) {
		pe_params_buoyancy *params = (pe_params_buoyancy*)_params;
		if (!is_unused(params->waterPlane.origin) && is_unused(m_pb.waterPlane.origin)) {
			m_pb.waterDensity = 1000.0f; m_pb.waterDamping = 0.0f;
			m_pb.waterPlane.n.Set(0,0,1);	m_pb.waterPlane.origin.zero();
			m_pb.waterFlow.zero(); m_pb.waterResistance = 1000.0f;
		}
		if (!is_unused(params->iMedium)) m_pb.iMedium = params->iMedium;
		if (!is_unused(params->waterDensity)) m_pb.waterDensity = params->waterDensity;
		if (!is_unused(params->waterDamping)) m_pb.waterDamping = params->waterDamping;
		if (!is_unused(params->waterPlane.n)) m_pb.waterPlane.n = params->waterPlane.n;
		if (!is_unused(params->waterPlane.origin)) m_pb.waterPlane.origin = params->waterPlane.origin;
		if (!is_unused(params->waterFlow)) m_pb.waterFlow = params->waterFlow;
		if (!is_unused(params->waterResistance)) m_pb.waterResistance = params->waterResistance;
		if (m_pWorld->m_pGlobalArea==this)
			m_pWorld->m_bCheckWaterHits = m_pWorld->m_matWater>=0 && !is_unused(m_pb.waterPlane.origin) ? ent_water:0;
		SignalEventAreaChange(m_pWorld, this);
		return 1;
	}

	return CPhysicalPlaceholder::SetParams(_params,bThreadSafe);
}

int CPhysArea::GetParams(pe_params *_params)
{
	if (_params->type==pe_simulation_params::type_id) {
		pe_simulation_params *params = (pe_simulation_params*)_params;
		params->gravity = m_gravity;
		params->damping = m_damping;
		return 1;
	}

	if (_params->type==pe_params_area::type_id) {
		pe_params_area *params = (pe_params_area*)_params;
		params->gravity = m_gravity;
		params->size = m_size;
		params->bUniform = m_bUniform;
		params->damping = m_damping;
		params->falloff0 = m_falloff0;
		params->bUseCallback = m_bUseCallback;
		params->pGeom = m_pGeom;
		return 1;
	}

	if (_params->type==pe_params_buoyancy::type_id) {
		*(pe_params_buoyancy*)_params = m_pb;
		return 1;
	}

	return CPhysicalPlaceholder::GetParams(_params);
}

int CPhysArea::GetStatus(pe_status *_status)
{
	if (_status->type==pe_status_pos::type_id) {
		pe_status_pos *status = (pe_status_pos*)_status;
		status->partid=status->ipart = 0;
		status->flags=status->flagsOR=status->flagsAND = 0;
		status->pos = m_offset;
		status->BBox[0] = m_BBox[0]-m_offset; 
		status->BBox[1]=  m_BBox[1]-m_offset;
		status->q = quaternionf(m_R);
		status->scale = m_scale;
		status->iSimClass = m_iSimClass;
		if (status->pMtx3x4)
			(*status->pMtx3x4 = m_R*m_scale).SetTranslation(m_offset);
		if (status->pMtx3x3)
			(Matrix33&)*status->pMtx3x3 = m_R*m_scale;
		status->pGeom=status->pGeomProxy = m_pGeom;
		return 1;
	}

	if (_status->type==pe_status_contains_point::type_id)
	{
		if (this == m_pWorld->m_pGlobalArea)
			return 1;
		return CheckPoint(((pe_status_contains_point*)_status)->pt);
	}

	if (_status->type==pe_status_area::type_id)
	{
		pe_status_area *status = (pe_status_area*)_status;
		status->pLockUpdate = &m_lockRef;

		bool bUniformForce = m_bUniform && (m_rsize.IsZero() || m_rfalloff0 == 0.f);
		bool bContained = m_pWorld->m_pGlobalArea == this || (!m_pGeom && !m_npt);

		if (!status->size.IsZero())
		{
			if (!bContained)
			{
				// non-global area, check box intersection.
				Vec3 min = status->ctr - status->size,
						 max = status->ctr + status->size;
				if (min.x > m_BBox[1].x || m_BBox[0].x > max.x || 
						min.y > m_BBox[1].y || m_BBox[0].y > max.y || 
						min.z > m_BBox[1].z || m_BBox[0].z > max.z)
					return 0;

				// adjust query point into area.
				min.CheckMax(m_BBox[0]);
				max.CheckMin(m_BBox[1]);
				status->ctr = (min+max)*0.5f;
			}

			if (status->bUniformOnly)
			{
				if (!bUniformForce)
					return -1;

				if (!bContained)
				{
					// check full box containment.
					if (m_npt)
						return -1;
					if (m_pGeom)
					{
						if (m_pGeom->GetType() == GEOM_SPHERE)
						{
							Vec3 ctrdist = status->ctr - (m_BBox[0] + m_BBox[1])*0.5f;
							float radii = status->size.GetLength() + (m_BBox[1] - m_BBox[0]).GetLength();
							if (ctrdist.GetLengthSquared() >= sqr(radii))
								return -1;
						}
						else if (m_pGeom->IsConvex(0.f))
						{
							// See if it contains all bounding box points.
							for (int i = 0; i < 8; i++)
							{
								Vec3 pt = status->ctr;
								pt.x += status->size.x * (i&1 ? 1.f : -1.f);
								pt.y += status->size.y * (i&2 ? 1.f : -1.f);
								pt.z += status->size.z * (i&4 ? 1.f : -1.f);
								if (!m_pGeom->PointInsideStatus(pt))
									return -1;
							}
						}
						else
							return -1;
					}
					bContained = true;
				}
			}
		}

		if (bUniformForce)
		{
			// quick test for containment.
			if (!bContained && !CheckPoint(status->ctr))
				return 0;

			status->gravity = m_gravity;
			status->pb = m_pb;

			if (m_bUniform==2)
			{
				if (!is_unused(status->gravity))
					status->gravity = m_R*status->gravity;
				if (!is_unused(status->pb.waterFlow))
					status->pb.waterFlow = m_R*status->pb.waterFlow;
			}
			return 1;
		}

		// general force query
		if (m_npt)
		{
			// CheckPoint a prerequisite for point-set shapes.
			if (!CheckPoint(status->ctr))
				return 0;
		}
		int iMedium = -1;
		return ApplyParams(status->ctr, status->gravity, status->vel, &status->pb, 0, 1, iMedium, 0) || !is_unused(status->gravity);
	}

	if (_status->type==pe_status_extent::type_id)
	{
		pe_status_extent *status = (pe_status_extent*)_status;
		assert(status->pGeo);
		status->pGeo->GetExtent(this, status->eForm);
		return 1;
	}

	if (_status->type==pe_status_random::type_id)
	{
		pe_status_random *status = (pe_status_random*)_status;
		assert(status->pGeo);
		GetRandomPos(status->ran, *status->pGeo, status->eForm);
		return 1;
	}

	return 0;
}

void CPhysArea::DrawHelperInformation(IPhysRenderer *pRenderer, int flags)
{	
	//ReadLock lock(m_lockUpdate);
	if (flags & pe_helper_bbox) {
		int i,j;
		Vec3 sz,center,pt[8];
		center = (m_BBox[1]+m_BBox[0])*0.5f;
		sz = (m_BBox[1]-m_BBox[0])*0.5f;
		for(i=0;i<8;i++)
			pt[i] = Vec3(sz.x*((i<<1&2)-1),sz.y*((i&2)-1),sz.z*((i>>1&2)-1))+center;
		for(i=0;i<8;i++) for(j=0;j<3;j++) if (i&1<<j)
			pRenderer->DrawLine(pt[i],pt[i^1<<j],m_iSimClass);
	}

	if (flags & pe_helper_geometry)	{
		if (m_pGeom) {
			geom_world_data gwd;
			gwd.R = m_R;
			gwd.offset = m_offset;
			gwd.scale = m_scale;
			m_pGeom->DrawWireframe(pRenderer,&gwd, flags>>16, m_iSimClass);
		}	
		if (m_ptSpline) {
			int i,j; float t,f,x,y,dist=0;
			const float cos15=0.96592582f,sin15=0.25881904f;
			Vec3 p0,p1,p2,v0,v1,v2,pt,pt0,ptc0,ptc1;
			Vec3_tpl<Vec3> axes;
			for(i=0; i<m_npt; i++) {
				p0 = m_R*m_ptSpline[max(0,i-1)]*m_scale+m_offset; 
				p1 = m_R*m_ptSpline[i]*m_scale+m_offset; 
				p2 = m_R*m_ptSpline[min(m_npt-1,i+1)]*m_scale+m_offset;
				v2 = (p0+p2)*0.5f-p1; v1 = p1-p0; v0 = (p0+p1)*0.5f;
				for(t=0.1f,pt0=(p0+p1)*0.5f; t<=1.01f; t+=0.1f,pt0=pt) {
					pt = (v2*t+v1)*t+v0;
					pRenderer->DrawLine(pt0,pt,m_iSimClass);
					if ((dist += (pt-pt0).len())>0.2f) {
						dist -= 0.2f; axes.z = (v2*(t*2)+v1).normalized();
						axes.x = axes.z.GetOrthogonal().normalized(); axes.y = axes.z^axes.x;
						for(j=0,x=cos15*m_zlim[0],y=sin15*m_zlim[0],ptc0=pt+axes.x*m_zlim[0]; j<24; j++,ptc0=ptc1) {
							ptc1 = pt+axes.x*x+axes.y*y;
							pRenderer->DrawLine(ptc0,ptc1,m_iSimClass);
							f=x; x=x*cos15-y*sin15; y=y*cos15+f*sin15;
						}
					}
				}
			}
		}
		if (m_pt) {
			int i0,i1,j;
			for(j=0;j<2;j++) for(i0=0;i0<m_npt;i0++) {
				i1 = i0+1 & i0-m_npt+1>>31;
				pRenderer->DrawLine(m_R*Vec3(m_pt[i0].x,m_pt[i0].y,m_zlim[j])*m_scale+m_offset,
														m_R*Vec3(m_pt[i1].x,m_pt[i1].y,m_zlim[j])*m_scale+m_offset, m_iSimClass);
			}
		}
	}
}


void CPhysArea::GetMemoryStatistics(ICrySizer *pSizer)
{
	pSizer->AddObject(this, sizeof(CPhysicalEntity));
	if (m_pt) {
		pSizer->AddObject(m_pt, sizeof(m_pt[0]), m_npt+1);
		pSizer->AddObject(m_idxSort[0], sizeof(m_idxSort[0][0]), m_npt);
		pSizer->AddObject(m_idxSort[1], sizeof(m_idxSort[1][0]), m_npt);
		pSizer->AddObject(m_pMask, sizeof(m_pMask[0]), ((m_npt-1>>5)+1));
	}
	if (m_ptSpline)
		pSizer->AddObject(m_ptSpline, sizeof(m_ptSpline[0]), m_npt);
	if (m_pGeom)
		m_pGeom->GetMemoryStatistics(pSizer);
	if (m_pFlows)
		pSizer->AddObject(m_pFlows, sizeof(m_pFlows[0]), m_npt);
}


static inline void swap(float *pval,int *pidx, int i1,int i2) {	
	float x=pval[i1]; pval[i1]=pval[i2]; pval[i2]=x;
	int i=pidx[i1]; pidx[i1]=pidx[i2]; pidx[i2]=i;
}
static void qsort(float *pval,int *pidx, int ileft,int iright)
{
	if (ileft>=iright) return;
	int i,ilast; 
	swap(pval,pidx, ileft,ileft+iright>>1);
	for(ilast=ileft,i=ileft+1; i<=iright; i++)
	if (pval[i] < pval[ileft])
		swap(pval,pidx, ++ilast,i);
	swap(pval,pidx, ileft,ilast);

	qsort(pval,pidx, ileft,ilast-1);
	qsort(pval,pidx, ilast+1,iright);
}

IPhysicalEntity *CPhysicalWorld::AddArea(Vec3 *pt,int npt, float zmin,float zmax, const Vec3 &pos,const quaternionf &q,float scale, 
																				 const Vec3 &normal, int *pTessIdx,int nTessTris,Vec3 *pFlows)
{
	if (npt<=0)
		return 0;
	WriteLock lock(m_lockAreas);
	if (!m_pGlobalArea)	{
		m_pGlobalArea = new CPhysArea(this);
		m_pGlobalArea->m_gravity = m_vars.gravity;
	}

	CPhysArea *pArea = new CPhysArea(this);
	int i,j,k,iend;
	float *xstart,seglen,len,maxdist;
	Vec3 n,p0,p1,BBox[2],sz,center,nscrew,axisx;
	Matrix33 C;
	m_nAreas++; m_nTypeEnts[PE_AREA]++;

	len=seglen = (pt[0]-pt[npt-1]).len();
	pArea->m_offset0 = (pt[0]+pt[npt-1])*seglen;
	for(i=0; i<npt-1; i++) {
		len += seglen=(pt[i+1]-pt[i]).len();
		pArea->m_offset0 += (pt[i+1]+pt[i])*seglen;
	}
	pArea->m_offset0 /= len*2;
	nscrew = pt[npt-1]-pArea->m_offset0 ^ pt[0]-pArea->m_offset0;
	for(i=0,C.SetZero(); i<npt-1; i++) { 
		p0 = pt[i]-pArea->m_offset0; p1 = pt[i+1]-pArea->m_offset0;	seglen = (p1-p0).len();
		nscrew += p0^p1;
		for(j=0;j<3;j++) for(k=0;k<3;k++)
			C(j,k) += seglen*(2*(p0[j]*p0[k]+p1[j]*p1[k]) + p0[j]*p1[k]+p0[k]*p1[j]);
	}

	real eval[3];
	Vec3r Cbuf[3],eigenAxes[3];
	for(i=0;i<3;i++) Cbuf[i]=C.GetRow(i);
	matrix Cmtx(3,3,mtx_symmetric,Cbuf[0]),eigenBasis(3,3,0,eigenAxes[0]); 
	Cmtx.jacobi_transformation(eigenBasis,eval,0);
	if (normal.len2()==0) {
		n = eigenAxes[idxmin3(eval)]; n *= (i=sgnnz(n*nscrew));
		axisx = eigenAxes[idxmax3(eval)]*i;
	}	else {
		n = normal; axisx = eigenAxes[idxmax3(eval)];
		(axisx -= n*(axisx*n)).normalize();
	}

	/*for(i=0,maxdet=0;i<3;i++) {
		det = C(inc_mod3[i],inc_mod3[i])*C(dec_mod3[i],dec_mod3[i]) - 
					C(dec_mod3[i],inc_mod3[i])*C(inc_mod3[i],dec_mod3[i]);
		if (fabs_tpl(det)>fabs_tpl(maxdet)) { 
			maxdet=det; j=i; 
		}
	}
	det = 1/maxdet; n[j] = 1;
	n[inc_mod3[j]] = -(C(inc_mod3[j],j)*C(dec_mod3[j],dec_mod3[j]) - C(dec_mod3[j],j)*C(inc_mod3[j],dec_mod3[j]))*det;
	n[dec_mod3[j]] = -(C(inc_mod3[j],inc_mod3[j])*C(dec_mod3[j],j) - C(dec_mod3[j],inc_mod3[j])*C(inc_mod3[j],j))*det;
	n.normalize(); n *= sgnnz(n*nscrew);*/

	pArea->m_R0.SetColumn(2,n);
	pArea->m_R0.SetColumn(0,axisx);
	pArea->m_R0.SetColumn(1,n^axisx);
	pArea->m_pt = new vector2df[(pArea->m_npt=npt)+1];
	pArea->m_rscale = 1/(pArea->m_rscale = scale);
	BBox[0] = Vec3(VMAX); BBox[1] = Vec3(VMIN);
	for(i=0,maxdist=0;i<npt;i++) {
		p0 = (pt[i]-pArea->m_offset0)*pArea->m_R0;
		pArea->m_pt[i] = vector2df(p0); maxdist = max(maxdist,fabs_tpl(p0.z));
		BBox[0].x=min_safe(BBox[0].x,pArea->m_pt[i].x); BBox[0].y=min_safe(BBox[0].y,pArea->m_pt[i].y);
		BBox[1].x=max_safe(BBox[1].x,pArea->m_pt[i].x);	BBox[1].y=max_safe(BBox[1].y,pArea->m_pt[i].y);
	}
	BBox[0].z=pArea->m_zlim[0] = zmin; BBox[1].z=pArea->m_zlim[1] = zmax;
	pArea->m_size0 = (BBox[1]-BBox[0])*0.5f;
	pArea->m_idxSort[0] = new int[npt];
	pArea->m_idxSort[1] = new int[npt];
	xstart = new float[npt];

	for(iend=0; iend<2; iend++)	{
		for(i=0;i<npt;i++) {
			pArea->m_idxSort[iend][i] = i; 
			j = i+1 & i-npt+1>>31;
			k = isneg(pArea->m_pt[i].x-pArea->m_pt[j].x)^iend;
			xstart[i] = pArea->m_pt[i&-k | j&~-k].x;
		}	
		qsort(xstart,pArea->m_idxSort[iend], 0,npt-1);
	}
	delete[] xstart;
	pArea->m_offset = pArea->m_offset0+pos;
	pArea->m_R = Matrix33(q)*pArea->m_R0;
	memset(pArea->m_pMask = new unsigned int[(npt-1>>5)+1],0,((npt-1>>5)+1)*sizeof(int));

	sz = Matrix33(pArea->m_R).Fabs()*pArea->m_size0*scale;
	center = pArea->m_offset+pArea->m_R*(BBox[0]+BBox[1])*(pArea->m_scale*0.5f);
	pArea->m_BBox[0] = center-sz; 
	pArea->m_BBox[1] = center+sz;

	if (maxdist>0.05f || pFlows) {
		int nTris, *pidx=new int[npt*3];
		Vec3 *pvtx=new Vec3[npt];
		MARK_UNUSED pArea->m_pt[npt].x;
		if (pTessIdx && nTessTris)
			memcpy(pidx,pTessIdx,(nTris=nTessTris)*3*sizeof(int));
		else
			nTris = TriangulatePoly(pArea->m_pt,npt,pidx,npt*3);
		if (pFlows)
			pArea->m_pFlows = new Vec3[npt];
		for(i=0;i<npt;i++) {
			pvtx[i] = (pt[i]-pArea->m_offset0)*pArea->m_R0;
			if (pFlows)
				pArea->m_pFlows[i] = pFlows[i]*pArea->m_R0;
		}
		if (pArea->m_pGeom = CreateMesh(pvtx,strided_pointer<unsigned short>((unsigned short*)pidx
#if defined(XENON) || defined(PS3)
			+1
#endif
			,sizeof(int)),0,0,nTris,
			mesh_SingleBB|mesh_shared_vtx|mesh_shared_idx))
			((CTriMesh*)pArea->m_pGeom)->m_flags &= ~(mesh_shared_vtx|mesh_shared_idx);
	}

	pArea->m_next = m_pGlobalArea->m_next;
	m_pGlobalArea->m_next = pArea;
	RepositionArea(pArea);

	return pArea;
}


IPhysicalEntity *CPhysicalWorld::AddArea(IGeometry *pGeom, const Vec3& pos,const quaternionf &q,float scale)
{
	WriteLock lock(m_lockAreas);
	if (!m_pGlobalArea) {
		m_pGlobalArea = new CPhysArea(this);
		m_pGlobalArea->m_gravity = m_vars.gravity;
	}

	CPhysArea *pArea = new CPhysArea(this);
	m_nAreas++;	m_nTypeEnts[PE_AREA]++;
	pArea->m_pGeom = pGeom;
	pArea->m_offset = pos;
	pArea->m_R = Matrix33(q);
	pArea->m_rscale = 1.0f/(pArea->m_scale=scale);

	pArea->m_offset0.zero();
	pArea->m_R0.SetIdentity();

	box abox;
	Vec3 sz,center;
	pGeom->GetBBox(&abox);
	abox.Basis *= pArea->m_R.T();
	sz = (abox.size*abox.Basis.Fabs())*scale;
	center = pos + q*abox.center*scale;
	pArea->m_BBox[0] = center-sz;
	pArea->m_BBox[1] = center+sz;

	pArea->m_next = m_pGlobalArea->m_next;
	m_pGlobalArea->m_next = pArea;
	RepositionArea(pArea);

	return pArea;
}


IPhysicalEntity *CPhysicalWorld::AddArea(Vec3 *pt,int npt, float r, const Vec3 &pos,const quaternionf &q,float scale)
{
	WriteLock lock(m_lockAreas);
	if (!m_pGlobalArea) {
		m_pGlobalArea = new CPhysArea(this);
		m_pGlobalArea->m_gravity = m_vars.gravity;
	}

	CPhysArea *pArea = new CPhysArea(this);
	m_nAreas++;	m_nTypeEnts[PE_AREA]++;
	pArea->m_offset = pos;
	pArea->m_R = Matrix33(q);
	pArea->m_rscale = 1.0f/(pArea->m_scale=scale);
	pArea->m_offset0.zero();
	pArea->m_R0.SetIdentity();
	pArea->m_zlim[0] = r;

	pArea->m_ptSpline = new Vec3[pArea->m_npt = npt];
	pArea->m_BBox[0]=pArea->m_BBox[1] = q*pt[0]*scale+pos;
	for(int i=0;i<npt;i++) {
		pArea->m_ptSpline[i] = pt[i];
		Vec3 ptw = q*pt[i]*scale+pos;
		pArea->m_BBox[0] = min(pArea->m_BBox[0],ptw); 
		pArea->m_BBox[1] = max(pArea->m_BBox[1],ptw);
	}
	pArea->m_BBox[0] -= Vec3(r,r,r);
	pArea->m_BBox[1] += Vec3(r,r,r);
	pArea->m_damping = 1.0f;
	pArea->m_falloff0 = 0.8f;

	pArea->m_next = m_pGlobalArea->m_next;
	m_pGlobalArea->m_next = pArea;
	RepositionArea(pArea);

	return pArea;
}


IPhysicalEntity *CPhysicalWorld::AddGlobalArea()
{
	WriteLock lock(m_lockAreas);
	if (!m_pGlobalArea)	{
		m_pGlobalArea = new CPhysArea(this);
		m_pGlobalArea->m_gravity = m_vars.gravity;

		EventPhysAreaChange epac;
		epac.pEntity = m_pGlobalArea;
		epac.boxAffected[0] = Vec3(-FLT_MAX);
		epac.boxAffected[1] = Vec3(FLT_MAX);
		SignalEvent(&epac,0);
	}
	return m_pGlobalArea;
}


void CPhysicalWorld::RepositionArea(CPhysArea *pArea)
{
	int res;
	CPhysArea *pPrevArea;
	for(pPrevArea=m_pGlobalArea; pPrevArea && pPrevArea->m_nextBig!=pArea; pPrevArea=pPrevArea->m_nextBig);

	if ((res = RepositionEntity(pArea,1))!=0) {
		AtomicAdd(&m_lockGrid,-WRITE_LOCK_VAL);
		if (res!=-1) {
			if (pPrevArea) {
				pPrevArea->m_nextBig = pArea->m_nextBig;
				m_nBigAreas--;
			}
			pArea->m_nextBig = 0;
		} else if (!pPrevArea) {
			pArea->m_nextBig = m_pGlobalArea->m_nextBig;
			m_pGlobalArea->m_nextBig = pArea;
			m_nBigAreas++;
		}
	}

	SignalEventAreaChange(this, pArea);
}


void CPhysicalWorld::RemoveArea(IPhysicalEntity *_pArea)
{
	WriteLock lock1(m_lockAreas),lock(m_lockGrid);
	CPhysArea *pPrevArea,*pArea=(CPhysArea*)_pArea;
	pArea->m_bDeleted = 2;
	for(pPrevArea=m_pGlobalArea; pPrevArea && pPrevArea->m_next!=pArea; pPrevArea=pPrevArea->m_next);
	if (pPrevArea) pPrevArea->m_next = pArea->m_next;
	for(pPrevArea=m_pGlobalArea; pPrevArea && pPrevArea->m_nextBig!=pArea; pPrevArea=pPrevArea->m_nextBig);
	if (pPrevArea) { 
		pPrevArea->m_nextBig = pArea->m_nextBig; m_nBigAreas--;
	}

	SignalEventAreaChange(this, pArea);
	DetachEntityGridThunks(pArea);
	m_nAreas--;	m_nTypeEnts[PE_AREA]--;
	pArea->m_next=m_pDeletedAreas; m_pDeletedAreas=pArea;
	m_prevGEAobjtypes = -1;
}

int CPhysicalWorld::CheckAreas(const Vec3 &ptc, Vec3 &gravity, pe_params_buoyancy *pb, int nMaxBuoys, const Vec3 &vel, IPhysicalEntity *pent, int iCaller)
{
	if (!m_pGlobalArea)
		return 0;
	if (m_vars.bMultithreaded)
		iCaller = iszero(m_idThread-(int)GetCurrentThreadId())^1;
	WriteLock lock0(m_lockCaller[iCaller]);
	ReadLock lock1(m_lockAreas);
	CPhysArea *pArea;
	CPhysicalEntity **pEnts;
	Vec3 gravityGlobal;
	MARK_UNUSED gravity,gravityGlobal;
	int nBuoys,iMedium0=-1;
	if (nBuoys=m_pGlobalArea->ApplyParams(ptc, gravityGlobal,vel, pb,0,nMaxBuoys,iMedium0, pent))
		iMedium0 = pb->iMedium;

	if (m_nBigAreas==m_nAreas) {
		for(pArea=m_pGlobalArea->m_nextBig; pArea; pArea=pArea->m_nextBig)
			if (!pArea->m_bDeleted && PtInAABB(pArea->m_BBox,ptc) && pArea->CheckPoint(ptc))
				nBuoys += pArea->ApplyParams(ptc, gravity,vel, pb,nBuoys,nMaxBuoys,iMedium0, pent);
	} else {
		for(int i=GetEntitiesAround(ptc,ptc,pEnts,ent_areas,0,0,iCaller)-1; i>=0; i--)
			if ((pArea=(CPhysArea*)pEnts[i])->CheckPoint(ptc))
				nBuoys += pArea->ApplyParams(ptc, gravity,vel, pb,nBuoys,nMaxBuoys,iMedium0, pent);
	}
	if (is_unused(gravity))
		gravity = gravityGlobal;

	return nBuoys | nBuoys-1>>31;
}

void CPhysicalWorld::SetWaterMat(int imat) 
{ 
	m_matWater=imat; 
	m_bCheckWaterHits = m_matWater>=0 && m_pGlobalArea && !is_unused(m_pGlobalArea->m_pb.waterPlane.origin) ? ent_water : 0;
}

IPhysicalEntity *CPhysicalWorld::GetNextArea(IPhysicalEntity *pPrevArea)
{
	if (!pPrevArea)	{
		if (!m_pGlobalArea)
			return 0;
		ReadLockCond lock(m_lockAreas,1); lock.SetActive(0);
		return m_pGlobalArea;
	} else {
		CPhysArea *pNextArea;
		for(pNextArea=((CPhysArea*)pPrevArea)->m_next; pNextArea && pNextArea->m_bDeleted; pNextArea=(CPhysArea*)pNextArea->m_next);
		if (!pNextArea) {
			CryInterlockedAdd(&m_lockAreas,-1);
			return 0;
		}	else 
			return (IPhysicalEntity*)pNextArea;
	}
}