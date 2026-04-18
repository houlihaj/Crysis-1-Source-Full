////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   statobjmandraw.cpp
//  Version:     v1.00
//  Created:     18/12/2002 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Visibility areas
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "StatObj.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "terrain_sector.h"
#include "3dEngine.h"
#include "CullBuffer.h"
#include "StringUtils.h"

void CVisArea::Update(const Vec3 * pPoints, int nCount, const char * szName, const SVisAreaInfo & info)
{
	strncpy(m_sName, szName, sizeof(m_sName));
	strlwr(m_sName);

	m_bThisIsPortal = CryStringUtils::stristr(m_sName,"portal") != 0;
  m_bIgnoreSky = (strstr(m_sName,"ignoresky") != 0) || info.bIgnoreSkyColor;

	m_fHeight = info.fHeight;
	m_vAmbColor = info.vAmbientColor;
	m_bAffectedByOutLights = info.bAffectedByOutLights;
  m_bSkyOnly = info.bSkyOnly;
	m_fViewDistRatio = info.fViewDistRatio;
	m_bDoubleSide = info.bDoubleSide;
	m_bUseDeepness = info.bUseDeepness;
	m_bUseInIndoors = info.bUseInIndoors;
	m_bMergeBrushes = info.bMergeBrushes;
  m_bOceanVisible = info.bOceanIsVisible;
	m_fVolumetricFogDensityMultiplier = info.fVolumetricFogDensityMultiplier;

	m_lstShapePoints.PreAllocate(nCount,nCount);
	
	if(nCount)
		memcpy(&m_lstShapePoints[0], pPoints, sizeof(Vec3)*nCount);

	// update bbox
	m_boxArea.max=SetMinBB();
	m_boxArea.min=SetMaxBB();

	for(int i=0; i<nCount; i++)
	{
		m_boxArea.max.CheckMax(pPoints[i]);
		m_boxArea.min.CheckMin(pPoints[i]);

		m_boxArea.max.CheckMax(pPoints[i]+Vec3(0,0,m_fHeight));
		m_boxArea.min.CheckMin(pPoints[i]+Vec3(0,0,m_fHeight));
	}

	UpdateGeometryBBox();
	m_RAMSolution[0].Init(this,m_lstShapePoints,m_lstConnections);
	m_RAMSolution[1].Init(this,m_lstShapePoints,m_lstConnections);
}

#define PORTAL_GEOM_BBOX_EXTENT 1.5f

void CVisArea::UpdateGeometryBBox()
{
	m_boxStatics.max = m_boxArea.max;
	m_boxStatics.min = m_boxArea.min;

	if(IsPortal())
	{ // fix for big objects passing portal
		m_boxStatics.max+=Vec3(PORTAL_GEOM_BBOX_EXTENT,PORTAL_GEOM_BBOX_EXTENT,PORTAL_GEOM_BBOX_EXTENT);
		m_boxStatics.min-=Vec3(PORTAL_GEOM_BBOX_EXTENT,PORTAL_GEOM_BBOX_EXTENT,PORTAL_GEOM_BBOX_EXTENT);
	}

	if(m_pObjectsTree)
		m_boxStatics.Add(m_pObjectsTree->GetObjectsBBox());
}
/*
void CVisArea::MarkForStreaming()
{
	for(int i=0; i<m_lstEntities[STATIC_OBJECTS].Count(); i++)
	{
		if(m_lstEntities[STATIC_OBJECTS][i].pNode->GetEntityStatObj(0))
    {
      ((CStatObj*)m_lstEntities[STATIC_OBJECTS][i].pNode->GetEntityStatObj(0))->m_nMarkedForStreamingFrameId = GetFrameID()+100;
//      m_lstEntities[STATIC_OBJECTS][i]->CheckPhysicalized();
    }
	}
}*/

CVisArea::CVisArea() 
{
//	m_eAreaType = eAreaType_VisArea;
	m_boxStatics.min=m_boxStatics.max=m_boxArea.min=m_boxArea.max=Vec3(0,0,0);
	m_sName[0]=0;
	m_nRndFrameId=-1;
	m_bActive=true;
	m_fHeight=0;
	m_vAmbColor(0,0,0);
	m_vConnNormals[0]=m_vConnNormals[1]=Vec3(0,0,0);
	m_bAffectedByOutLights = false;
	m_fDistance=0;
  m_bOceanVisible=m_bSkyOnly=false;
	m_fVolumetricFogDensityMultiplier = 1.0f;
	memset(m_arrOcclCamera, 0, sizeof(m_arrOcclCamera));
	m_fViewDistRatio = 100.f;
	m_bDoubleSide = true;
	m_bUseDeepness = false;
	m_bUseInIndoors = false;
	memset(m_arrvActiveVerts,0,sizeof(m_arrvActiveVerts));
	m_bIgnoreSky = m_bThisIsPortal = false;
}

CVisArea::~CVisArea()
{
  for(int i=0; i<MAX_RECURSION_LEVELS; i++)
    SAFE_DELETE(m_arrOcclCamera[i]);

/*
  for(int nStatic=0; nStatic<2; nStatic++)
  for(int i=0; i<m_lstEntities[nStatic].Count(); i++)
    if(m_lstEntities[nStatic][i]->GetEntityVisArea()==this)
      m_lstEntities[nStatic][i]->GetEntityVisArea()=0;
			*/
}

bool InsidePolygon(Vec3 *polygon,int N,Vec3 p)
{
  int counter = 0;
  int i;
  double xinters;
  Vec3 p1,p2;

  p1 = polygon[0];
  for (i=1;i<=N;i++) {
    p2 = polygon[i % N];
    if (p.y > min(p1.y,p2.y)) {
      if (p.y <= max(p1.y,p2.y)) {
        if (p.x <= max(p1.x,p2.x)) {
          if (p1.y != p2.y) {
            xinters = (p.y-p1.y)*(p2.x-p1.x)/(p2.y-p1.y)+p1.x;
            if (p1.x == p2.x || p.x <= xinters)
              counter++;
          }
        }
      }
    }
    p1 = p2;
  }

  if (counter % 2 == 0)
    return(false);
  else
    return(true);
}

bool CVisArea::IsBoxOverlapVisArea(const AABB & objBox)
{
  if( !Overlap::AABB_AABB(objBox,m_boxArea) )
    return false;

  static PodArray<Vec3> polygonA; polygonA.Clear();
  polygonA.Add(Vec3(objBox.min.x, objBox.min.y, objBox.min.z ));
  polygonA.Add(Vec3(objBox.min.x, objBox.max.y, objBox.min.z ));
  polygonA.Add(Vec3(objBox.max.x, objBox.max.y, objBox.min.z ));
  polygonA.Add(Vec3(objBox.max.x, objBox.min.y, objBox.min.z ));
  return Overlap::Polygon_Polygon2D< PodArray<Vec3> >(polygonA, m_lstShapePoints);
}

bool CVisArea::IsPointInsideVisArea(const Vec3 & vPos)
{
	if( Overlap::Point_AABB(vPos,m_boxArea) )
	if(InsidePolygon(&m_lstShapePoints[0], m_lstShapePoints.Count(), vPos))
		return true;

	return false;
}


bool InsideSpherePolygon(Vec3 *polygon,int N,Sphere &S)
{
	int counter = 0;
	int i;
	Vec3 p1,p2;

	p1 = polygon[0];
	for (i=1;i<=N;i++) {
		p2 = polygon[i % N];
		Vec3 vA( p1 - S.center );
		Vec3 vD( p2 - p1);
		vA.z = vD.z = 0;
		vD.NormalizeSafe();
		float fB = vD.Dot( vA );
		float fC = vA.Dot( vA ) - S.radius * S.radius;
		if( fB*fB >= fC )			//at least 1 root
				return(true);
		p1 = p2;
	}

	return(false);
}

float LineSegDistanceSqr(const Vec3& vPos, const Vec3& vP0, const Vec3& vP1)
{
	// Dist of line seg A(+D) from origin:
	// P = A + D t[0..1]
	// d^2(t) = (A + D t)^2 = A^2 + 2 A*D t + D^2 t^2
	// d^2\t = 2 A*D + 2 D^2 t = 0
	// tmin = -A*D / D^2 clamp(0,1)
	// Pmin = A + D tmin
	Vec3 vP = vP0 - vPos;
	Vec3 vD = vP1 - vP0;
	float fN = -(vP * vD);
	if (fN > 0.f)
	{
		float fD = vD.GetLengthSquared();
		if (fN >= fD)
			vP += vD;
		else
			vP += vD * (fN/fD);
	}
	return vP.GetLengthSquared();
}

bool CVisArea::IsSphereInsideVisArea(const Vec3 & vPos, const f32 fRadius)
{
	Sphere S(vPos,fRadius);
	if( Overlap::Sphere_AABB( S, m_boxArea ) )
		if(InsidePolygon(&m_lstShapePoints[0], m_lstShapePoints.Count(), vPos) || InsideSpherePolygon(&m_lstShapePoints[0], m_lstShapePoints.Count(), S) )
			return true;

	return false;
}

// Current scheme: Will never move sphere center, just clip radius, even to 0.
bool CVisArea::ClipToVisArea(bool bInside, Sphere& sphere, Vec3 const& vNormal)
{
	FUNCTION_PROFILER_3DENGINE;

	/*
		Clip		PointZ	PointXY

		In			In			In					inside, clip Z and XY
		In			In			Out					outside, return 0
		In			Out			In					outside, return 0
		In			Out			Out					outside, return 0

		Out			In			In					inside, return 0
		Out			In			Out					outside, clip XY
		Out			Out			In					outside, clip Z
		Out			Out			Out					outside, clip XY
	*/

	bool bClipXY = false, bClipZ = false;
	if (bInside)
	{
		// Clip to 0 if center outside.
		if (!IsPointInsideVisArea(sphere.center))
		{
			sphere.radius = 0.f;
			return true;
		}
		bClipXY = bClipZ = true;
	}
	else
	{
		if (Overlap::Point_AABB(sphere.center, m_boxArea))
		{
			if (InsidePolygon(m_lstShapePoints.begin(), m_lstShapePoints.size(), sphere.center))
			{
				sphere.radius = 0.f;
				return true;
			}
			else
				bClipXY = true;
		}
		else if (InsidePolygon(m_lstShapePoints.begin(), m_lstShapePoints.size(), sphere.center))
			bClipZ = true;
		else
			bClipXY = true;
	}

	float fOrigRadius = sphere.radius;
	if (bClipZ)
	{
		// Check against vertical planes.
		float fOrigRadius = sphere.radius;
		float fDist = min(abs(m_boxArea.max.z - sphere.center.z), abs(sphere.center.z - m_boxArea.min.z));
		float fRadiusScale = sqrt_tpl(max(1.f - sqr(vNormal.z), 0.f));
		if (fDist < sphere.radius * fRadiusScale)
		{
			sphere.radius = fDist / fRadiusScale;
			if (sphere.radius <= 0.f)
				return true;
		}
	}

	if (bClipXY)
	{
		Vec3 vP1 = m_lstShapePoints[0];
		vP1.z = clamp_tpl(sphere.center.z, m_boxArea.min.z, m_boxArea.max.z);
		for (int n = m_lstShapePoints.Count()-1; n >= 0; n--)
		{
			Vec3 vP0 = m_lstShapePoints[n];
			vP0.z = vP1.z;
			
			// Compute nearest vector from center to plane.
			Vec3 vP = vP0 - sphere.center;
			Vec3 vD = vP1 - vP0;
			float fN = -(vP * vD);
			if (fN > 0.f)
			{
				float fD = vD.GetLengthSquared();
				if (fN >= fD)
					vP += vD;
				else
					vP += vD * (fN/fD);
			}

			// Check distance only in planar direction.
			float fDist = vP.GetLength() * 0.99f;
			float fRadiusScale = fDist > 0.f ? sqrt_tpl(max(1.f - sqr(vNormal*vP)/sqr(fDist), 0.f)) : 1.f;
			if (fDist < sphere.radius * fRadiusScale)
			{
				sphere.radius = fDist / fRadiusScale;
				if (sphere.radius <= 0.f)
					return true;
			}

			vP1 = vP0;
		}
	}

	return sphere.radius < fOrigRadius;
}

bool CVisArea::FindVisAreaReqursive(IVisArea * pAnotherArea, int nMaxReqursion, bool bSkipDisabledPortals, PodArray<CVisArea*> & arrVisitedParents )
{
  arrVisitedParents.Add(this);

	if(pAnotherArea == this)
		return true;

	if( pAnotherArea == NULL && IsConnectedToOutdoor() )
		return true;

	bool bFound = false;

	if(nMaxReqursion > 1)
  {
		for(int p=0; p<m_lstConnections.Count(); p++)
    {
			if(!bSkipDisabledPortals || m_lstConnections[p]->IsActive())
			{
				if( -1 == arrVisitedParents.Find( m_lstConnections[p] ) )
        {
					if( m_lstConnections[p]->FindVisAreaReqursive(pAnotherArea, nMaxReqursion-1, bSkipDisabledPortals, arrVisitedParents ) )
					{
						bFound = true;
            break;
					}
        }
			}
    }
  }

	return bFound;
}

bool CVisArea::FindVisArea(IVisArea * pAnotherArea, int nMaxRecursion, bool bSkipDisabledPortals)
{ 
  // collect visited areas in order to prevent visiting it again
	static PodArray<CVisArea*> arrVisitedParents; arrVisitedParents.Clear();
	arrVisitedParents.PreAllocate( nMaxRecursion, 0 );

	return FindVisAreaReqursive(pAnotherArea, nMaxRecursion, bSkipDisabledPortals, arrVisitedParents );
}

void CVisArea::FindSurroundingVisAreaReqursive( int nMaxReqursion, bool bSkipDisabledPortals, PodArray<IVisArea*> * pVisitedAreas, int nMaxVisitedAreas, int nDeepness, PodArray<CVisArea*> * pUnavailableAreas )
{
	int nAreaId = pUnavailableAreas->Count();
	pUnavailableAreas->Add(this);

	if(pVisitedAreas)
		pVisitedAreas->Add( this );

	if(nMaxReqursion > (nDeepness+1) )
		for(int p=0; p<m_lstConnections.Count(); p++)
			if(!bSkipDisabledPortals || m_lstConnections[p]->IsActive())
			{
				if( -1 == pUnavailableAreas->Find( m_lstConnections[p] ) )
					 m_lstConnections[p]->FindSurroundingVisAreaReqursive( nMaxReqursion, bSkipDisabledPortals, pVisitedAreas, nMaxVisitedAreas, nDeepness+1, pUnavailableAreas );
			}

}

void CVisArea::FindSurroundingVisArea( int nMaxReqursion, bool bSkipDisabledPortals, PodArray<IVisArea*> * pVisitedAreas, int nMaxVisitedAreas, int nDeepness )
{
	if(pVisitedAreas)
	{
		if(pVisitedAreas->capacity()<nMaxVisitedAreas)
			pVisitedAreas->PreAllocate( nMaxVisitedAreas,nMaxVisitedAreas );
	}

  static PodArray<CVisArea*> lUnavailableAreas; lUnavailableAreas.Clear();
	lUnavailableAreas.PreAllocate( nMaxVisitedAreas,0 );

	FindSurroundingVisAreaReqursive( nMaxReqursion,bSkipDisabledPortals,pVisitedAreas,nMaxVisitedAreas,nDeepness, &lUnavailableAreas );
}

/*
bool CVisArea::PreloadVisArea(int nMaxReqursion, bool * pbOutdoorFound, CVisArea * pParentToAvoid, Vec3 vPrevPortalPos, float fPrevPortalDistance)
{
	FUNCTION_PROFILER_3DENGINE;
	if(IsPortal())
	{
		fPrevPortalDistance += vPrevPortalPos.GetDistance((m_boxArea.min+m_boxArea.max)*0.5f);
		vPrevPortalPos = (m_boxArea.min+m_boxArea.max)*0.5f;
	}

	PreloadResources(vPrevPortalPos, fPrevPortalDistance);

	if(IsConnectedToOutdoor())
		*pbOutdoorFound = true;

	if(nMaxReqursion>0)
		for(int p=0; p<m_lstConnections.Count(); p++)
			if(m_lstConnections[p] != pParentToAvoid)
				if(GetCurTimeSec()>(m_fPreloadStartTime+0.010f)||
					m_lstConnections[p]->PreloadVisArea(nMaxReqursion-1, pbOutdoorFound, this, vPrevPortalPos, fPrevPortalDistance))
					return true;

	return false;
}
*/
int CVisArea::GetVisFrameId()
{
	return m_nRndFrameId;
}

bool CVisArea::IsConnectedToOutdoor()
{
	if(IsPortal()) // check if this portal has just one connection
		return m_lstConnections.Count()==1;

	// find portals with just one connection
	for(int p=0; p<m_lstConnections.Count(); p++)
	{
		CVisArea * pPortal = m_lstConnections[p];
		if(pPortal->m_lstConnections.Count()==1)
			return true;
	}

	return false;
}


bool Is2dLinesIntersect(float x1,float y1,float x2,float y2,float x3,float y3,float x4,float y4)
{
	float fDiv=(y4-y3)*(x2-x1)-(x4-x3)*(y2-y1);

	if(fabs(fDiv)<0.00001f)
		return false;

	float ua = ((x4-x3)*(y1-y3)-(y4-y3)*(x1-x3))/fDiv;
	float ub = ((x2-x1)*(y1-y3)-(y2-y1)*(x1-x3))/fDiv;

	return ua>0 && ua<1 && ub>0 && ub<1;
}


Vec3 CVisArea::GetConnectionNormal(CVisArea * pPortal)
{
//	if(strstr(pPortal->m_sName,"ab09_portal11"))
	//	int t=0;

	assert(m_lstShapePoints.Count()>=3);
	// find side of shape intersecting with portal
	int nIntersNum = 0;
	Vec3 arrNormals[2]={Vec3(0,0,0),Vec3(0,0,0)};
	for(int v=0; v<m_lstShapePoints.Count(); v++)
	{
		nIntersNum=0;
		arrNormals[0]=Vec3(0,0,0);
		arrNormals[1]=Vec3(0,0,0);
		for(int p=0; p<pPortal->m_lstShapePoints.Count(); p++)
		{
			const Vec3 & v0 = m_lstShapePoints[v];
			const Vec3 & v1 = m_lstShapePoints[(v+1)%m_lstShapePoints.Count()];
			const Vec3 & p0 = pPortal->m_lstShapePoints[p];
			const Vec3 & p1 = pPortal->m_lstShapePoints[(p+1)%pPortal->m_lstShapePoints.Count()];

			if(Is2dLinesIntersect(v0.x,v0.y,v1.x,v1.y,p0.x,p0.y,p1.x,p1.y))
			{
				Vec3 vNormal = (v0-v1).GetNormalized().Cross(Vec3(0,0,1.f));			
				if(nIntersNum<2)
					arrNormals[nIntersNum++] = (IsShapeClockwise()) ? -vNormal : vNormal;
			}
		}

		if(nIntersNum==2)
			break;
	}

	if(nIntersNum == 2 && 
		//IsEquivalent(arrNormals[0] == arrNormals[1])
		arrNormals[0].IsEquivalent(arrNormals[1],VEC_EPSILON)
		)
		return arrNormals[0];

	{
		int nBottomPoints=0;
		for(int p=0; p<pPortal->m_lstShapePoints.Count() && p<4; p++)
			if(IsPointInsideVisArea(pPortal->m_lstShapePoints[p]))
				nBottomPoints++;

		int nUpPoints=0;
		for(int p=0; p<pPortal->m_lstShapePoints.Count() && p<4; p++)
			if(IsPointInsideVisArea(pPortal->m_lstShapePoints[p]+Vec3(0,0,pPortal->m_fHeight)))
				nUpPoints++;

		if(nBottomPoints==0 && nUpPoints==4)
			return Vec3(0,0,1);

		if(nBottomPoints==4 && nUpPoints==0)
			return Vec3(0,0,-1);
	}

	return Vec3(0,0,0);
}

void CVisArea::UpdatePortalCameraPlanes(CCamera & cam, Vec3 * pVerts,bool NotForcePlaneSet)
{
	const Vec3& vCamPos = GetCamera().GetPosition();
	Plane plane;

	plane.SetPlane(pVerts[0],pVerts[2],pVerts[1]);
	cam.SetFrustumPlane(FR_PLANE_NEAR, plane);

	plane = *GetCamera().GetFrustumPlane(FR_PLANE_FAR);
	cam.SetFrustumPlane(FR_PLANE_FAR, plane);

	plane.SetPlane(vCamPos,pVerts[3],pVerts[2]);	// update plane only if it reduces fov
	if(!NotForcePlaneSet || plane.n.Dot(cam.GetFrustumPlane(FR_PLANE_LEFT)->n)<
		cam.GetFrustumPlane(FR_PLANE_RIGHT)->n.Dot(cam.GetFrustumPlane(FR_PLANE_LEFT)->n))
		cam.SetFrustumPlane(FR_PLANE_RIGHT, plane);

	plane.SetPlane(vCamPos,pVerts[1],pVerts[0]);	// update plane only if it reduces fov
	if(!NotForcePlaneSet || plane.n.Dot(cam.GetFrustumPlane(FR_PLANE_RIGHT)->n)<
		cam.GetFrustumPlane(FR_PLANE_LEFT)->n.Dot(cam.GetFrustumPlane(FR_PLANE_RIGHT)->n))
		cam.SetFrustumPlane(FR_PLANE_LEFT, plane);

	plane.SetPlane(vCamPos,pVerts[0],pVerts[3]);	// update plane only if it reduces fov
	if(!NotForcePlaneSet || plane.n.Dot(cam.GetFrustumPlane(FR_PLANE_TOP)->n)<
		cam.GetFrustumPlane(FR_PLANE_BOTTOM)->n.Dot(cam.GetFrustumPlane(FR_PLANE_TOP)->n))
		cam.SetFrustumPlane(FR_PLANE_BOTTOM, plane);

	plane.SetPlane(vCamPos,pVerts[2],pVerts[1]); // update plane only if it reduces fov
	if(!NotForcePlaneSet || plane.n.Dot(cam.GetFrustumPlane(FR_PLANE_BOTTOM)->n)<
		cam.GetFrustumPlane(FR_PLANE_TOP)->n.Dot(cam.GetFrustumPlane(FR_PLANE_BOTTOM)->n))
		cam.SetFrustumPlane(FR_PLANE_TOP, plane);

	Vec3 arrvPortVertsCamSpace[4];
	for(int i=0; i<4; i++)
		arrvPortVertsCamSpace[i] = pVerts[i]-cam.GetPosition();
	cam.SetFrustumVertices(arrvPortVertsCamSpace);

	if(GetCVars()->e_portals==5)
	{
		float farrColor[4] = {1,1,1,1};
		//		GetRenderer()->SetMaterialColor(1,1,1,1);
		DrawLine(pVerts[0],pVerts[1]);
		GetRenderer()->DrawLabelEx(pVerts[0],1,farrColor,false,true,"0");
		DrawLine(pVerts[1],pVerts[2]);
		GetRenderer()->DrawLabelEx(pVerts[1],1,farrColor,false,true,"1");
		DrawLine(pVerts[2],pVerts[3]);
		GetRenderer()->DrawLabelEx(pVerts[2],1,farrColor,false,true,"2");
		DrawLine(pVerts[3],pVerts[0]);
		GetRenderer()->DrawLabelEx(pVerts[3],1,farrColor,false,true,"3");
	}
}

/*void CVisArea::UpdatePortalCameraPlanes(CCamera & cam, Vec3 * pVerts,const PodArray<Vec3>& rClippedPortal,bool Upright)
{ // todo: do also take into account GetCamera()
	const Vec3& vCamPos = GetCamera().GetPosition();
	Plane plane;

	if(Upright)
		return UpdatePortalCameraPlanes(cam,pVerts,Upright);

//	plane.SetPlane(pVerts[0],pVerts[2],pVerts[1]);
  plane = *GetCamera().GetFrustumPlane(FR_PLANE_NEAR);
	cam.SetFrustumPlane(FR_PLANE_NEAR, plane);

	plane = *GetCamera().GetFrustumPlane(FR_PLANE_FAR);
	cam.SetFrustumPlane(FR_PLANE_FAR, plane);

	const Vec3&	rRight	=	GetCamera().GetMatrix().GetColumn0();
	const Vec3&	rUp			=	GetCamera().GetMatrix().GetColumn2();

	//get extends of the portal by finding the most extending vertices in screenspace
	f32 BestMaxX,BestMinX,BestMaxY,BestMinY;
	int MaxXIdx,MinXIdx,MaxYIdx,MinYIdx;

	MaxXIdx=MinXIdx=MaxYIdx=MinYIdx=0;
	BestMaxX	=	(rClippedPortal[MaxXIdx]-vCamPos).normalized().Dot(cam.GetFrustumPlane(FR_PLANE_RIGHT)->n);
	BestMinX	=	(rClippedPortal[MinXIdx]-vCamPos).normalized().Dot(cam.GetFrustumPlane(FR_PLANE_LEFT)->n);
	BestMaxY	=	(rClippedPortal[MaxYIdx]-vCamPos).normalized().Dot(cam.GetFrustumPlane(FR_PLANE_TOP)->n);
	BestMinY	=	(rClippedPortal[MinYIdx]-vCamPos).normalized().Dot(cam.GetFrustumPlane(FR_PLANE_BOTTOM)->n);
	f32 Dist;
	for(uint32 a=1;a<rClippedPortal.Count();a++)
	{
		Dist	=	(rClippedPortal[a]-vCamPos).normalized().Dot(cam.GetFrustumPlane(FR_PLANE_RIGHT)->n);
		if(Dist>BestMaxX)
		{
			BestMaxX	=	Dist;
			MaxXIdx	=	a;
		}
		Dist	=	(rClippedPortal[a]-vCamPos).normalized().Dot(cam.GetFrustumPlane(FR_PLANE_LEFT)->n);
		if(Dist>BestMinX)
		{
			BestMinX	=	Dist;
			MinXIdx	=	a;
		}
		Dist	=	(rClippedPortal[a]-vCamPos).normalized().Dot(cam.GetFrustumPlane(FR_PLANE_TOP)->n);
		if(Dist>BestMaxY)
		{
			BestMaxY	=	Dist;
			MaxYIdx	=	a;
		}
		Dist	=	(rClippedPortal[a]-vCamPos).normalized().Dot(cam.GetFrustumPlane(FR_PLANE_BOTTOM)->n);
		if(Dist>BestMinY)
		{
			BestMinY	=	Dist;
			MinYIdx	=	a;
		}
	}

	if(BestMaxX<0.f)
	{
		plane.SetPlane(vCamPos,rClippedPortal[MaxXIdx],rClippedPortal[MaxXIdx]+rUp);
		f32 aa=plane.n.Dot(cam.GetFrustumPlane(FR_PLANE_RIGHT)->n);
		cam.SetFrustumPlane(FR_PLANE_RIGHT, plane);
	}
	if(BestMinX<0.f)
	{
		plane.SetPlane(vCamPos,rClippedPortal[MinXIdx],rClippedPortal[MinXIdx]-rUp);
		f32 aa=plane.n.Dot(cam.GetFrustumPlane(FR_PLANE_LEFT)->n);
		cam.SetFrustumPlane(FR_PLANE_LEFT, plane);
	}
	if(BestMaxY<0.f)
	{
		plane.SetPlane(vCamPos,rClippedPortal[MaxYIdx],rClippedPortal[MaxYIdx]-rRight);
		f32 aa=plane.n.Dot(cam.GetFrustumPlane(FR_PLANE_TOP)->n);
		cam.SetFrustumPlane(FR_PLANE_TOP, plane);
	}
	if(BestMinY<0.f)
	{
		plane.SetPlane(vCamPos,rClippedPortal[MinYIdx],rClippedPortal[MinYIdx]+rRight);
		f32 aa=plane.n.Dot(cam.GetFrustumPlane(FR_PLANE_BOTTOM)->n);
		cam.SetFrustumPlane(FR_PLANE_BOTTOM, plane);
	}

	Vec3 arrvPortVertsCamSpace[4];
	for(int i=0; i<4; i++)
		arrvPortVertsCamSpace[i] = pVerts[i]-cam.GetPosition();
	cam.SetFrustumVertices(arrvPortVertsCamSpace);

	if(GetCVars()->e_portals==5)
	{
    float farrColor[4] = {1,1,1,1};
//		GetRenderer()->SetMaterialColor(1,1,1,1);
		DrawLine(pVerts[0],pVerts[1]);
		GetRenderer()->DrawLabelEx(pVerts[0],1,farrColor,false,true,"0");
		DrawLine(pVerts[1],pVerts[2]);
		GetRenderer()->DrawLabelEx(pVerts[1],1,farrColor,false,true,"1");
		DrawLine(pVerts[2],pVerts[3]);
		GetRenderer()->DrawLabelEx(pVerts[2],1,farrColor,false,true,"2");
		DrawLine(pVerts[3],pVerts[0]);
		GetRenderer()->DrawLabelEx(pVerts[3],1,farrColor,false,true,"3");
	}
}*/

int __cdecl CVisAreaManager__CmpDistToPortal(const void* v1, const void* v2);

void CVisArea::PreRender( int nReqursionLevel, 
													CCamera CurCamera, CVisArea * pParent, CVisArea * pCurPortal, 
													bool * pbOutdoorVisible, PodArray<CCamera> * plstOutPortCameras, bool * pbSkyVisible, bool * pbOceanVisible, 
													PodArray<CVisArea*> & lstVisibleAreas)
{
	// mark as rendered
	if(!GetObjManager()->m_nRenderStackLevel)
	{
		if(m_nRndFrameId != GetFrameID())
			m_lstCurCameras.Clear();

		m_nRndFrameId = GetFrameID();
	}

	if(m_bAffectedByOutLights)
		GetVisAreaManager()->m_bSunIsNeeded = true;

	m_lstCurCameras.Add(CurCamera);

  if(lstVisibleAreas.Find(this)<0)
	  lstVisibleAreas.Add(this);

	// check recursion and portal activity
	if(!nReqursionLevel || !m_bActive)
		return;

	if(	pParent && m_bThisIsPortal && m_lstConnections.Count()==1 && // detect entrance
		 !IsPointInsideVisArea(GetCamera().GetPosition()) && // detect camera in outdoors
		 !CurCamera.IsAABBVisible_F( m_boxArea )) // if invisible 
		 return; // stop recursion

	bool bCanSeeThruThisArea = true;

	// prepare new camera for next areas
	if(m_bThisIsPortal && m_lstConnections.Count() && 
    (this!=pCurPortal || !pCurPortal->IsPointInsideVisArea(CurCamera.GetPosition())))
	{
		Vec3 vCenter = m_boxArea.GetCenter();
		float fRadius = m_boxArea.GetRadius();

		Vec3 vPortNorm = (!pParent || pParent == m_lstConnections[0] || m_lstConnections.Count()==1) ? 
			m_vConnNormals[0] : m_vConnNormals[1];

		// exit/entrance portal has only one normal in direction to outdoors, so flip it to the camera
		if(m_lstConnections.Count()==1 && !pParent)
			vPortNorm = -vPortNorm; 

		// back face check
		Vec3 vPortToCamDir = CurCamera.GetPosition() - vCenter;
		if(vPortToCamDir.Dot(vPortNorm)<0)
			return;

		if(!m_bDoubleSide)
			if(vPortToCamDir.Dot(m_vConnNormals[0])<0)
				return;

		Vec3 arrPortVerts[4];
		Vec3 arrPortVertsOtherSide[4]; 
		bool barrPortVertsOtherSideValid = false;
		if(pParent && !vPortNorm.IsEquivalent(Vec3(0,0,0),VEC_EPSILON) && vPortNorm.z)
		{ // up/down portal
			int nEven = IsShapeClockwise();
			if(vPortNorm.z>0)
				nEven=!nEven;
			for(int i=0; i<4; i++)
			{
				arrPortVerts[i] = m_lstShapePoints[nEven ? (3-i) : i]+Vec3(0,0,m_fHeight)*(vPortNorm.z>0);
				arrPortVertsOtherSide[i] = m_lstShapePoints[nEven ? (3-i) : i]+Vec3(0,0,m_fHeight)*(vPortNorm.z<0);
			}
			barrPortVertsOtherSideValid = true;
		}
		else if(!vPortNorm.IsEquivalent(Vec3(0,0,0),VEC_EPSILON)	&& vPortNorm.z==0)
		{ // basic portal
			Vec3 arrInAreaPoint[2]={Vec3(0,0,0),Vec3(0,0,0)};
			int arrInAreaPointId[2]={-1,-1};
			int nInAreaPointCounter=0;

			Vec3 arrOutAreaPoint[2]={Vec3(0,0,0),Vec3(0,0,0)};
			int nOutAreaPointCounter=0;

			// find 2 points of portal in this area (or in this outdoors)
			for(int i=0; i<m_lstShapePoints.Count() && nInAreaPointCounter<2; i++)
			{ 
				Vec3 vTestPoint = m_lstShapePoints[i]+Vec3(0,0,m_fHeight*0.5f);
				CVisArea * pAnotherArea = m_lstConnections[0];
				if((pParent && (pParent->IsPointInsideVisArea(vTestPoint))) || 
					(!pParent && (!pAnotherArea->IsPointInsideVisArea(vTestPoint))) )
				{
					arrInAreaPointId[nInAreaPointCounter] = i;
					arrInAreaPoint[nInAreaPointCounter++] = m_lstShapePoints[i];
				}
			}

			// find 2 points of portal not in this area (or not in this outdoors)
			for(int i=0; i<m_lstShapePoints.Count() && nOutAreaPointCounter<2; i++)
			{ 
				Vec3 vTestPoint = m_lstShapePoints[i]+Vec3(0,0,m_fHeight*0.5f);
				CVisArea * pAnotherArea = m_lstConnections[0];
				if((pParent && (pParent->IsPointInsideVisArea(vTestPoint))) || 
					(!pParent && (!pAnotherArea->IsPointInsideVisArea(vTestPoint))) )
				{
				}
				else
				{
					arrOutAreaPoint[nOutAreaPointCounter++] = m_lstShapePoints[i];
				}
			}

			if(nInAreaPointCounter==2)
			{ // success, take into account volume and portal shape versts order
				int nEven = IsShapeClockwise();
				if(arrInAreaPointId[1]-arrInAreaPointId[0] != 1)
					nEven = !nEven;

				arrPortVerts[0] = arrInAreaPoint[nEven];
				arrPortVerts[1] = arrInAreaPoint[nEven]+Vec3(0,0,m_fHeight);
				arrPortVerts[2] = arrInAreaPoint[!nEven]+Vec3(0,0,m_fHeight);
				arrPortVerts[3] = arrInAreaPoint[!nEven];

				nEven = !nEven;

				arrPortVertsOtherSide[0] = arrOutAreaPoint[nEven];
				arrPortVertsOtherSide[1] = arrOutAreaPoint[nEven]+Vec3(0,0,m_fHeight);
				arrPortVertsOtherSide[2] = arrOutAreaPoint[!nEven]+Vec3(0,0,m_fHeight);
				arrPortVertsOtherSide[3] = arrOutAreaPoint[!nEven];
				barrPortVertsOtherSideValid = true;
			}
			else
			{ // something wrong
				Warning( "CVisArea::PreRender: Invalid portal: %s", m_sName);
				return;
			}
		}
		else if(!pParent && vPortNorm.z==0 && m_lstConnections.Count()==1)
		{ // basic entrance portal
			Vec3 vBorder = (vPortNorm.Cross(Vec3(0,0,1.f))).GetNormalized()*fRadius;
			arrPortVerts[0] = vCenter - Vec3(0,0,1.f)*fRadius - vBorder;
			arrPortVerts[1] = vCenter + Vec3(0,0,1.f)*fRadius - vBorder;
			arrPortVerts[2] = vCenter + Vec3(0,0,1.f)*fRadius + vBorder;
			arrPortVerts[3] = vCenter - Vec3(0,0,1.f)*fRadius + vBorder;
		}
		else if(!pParent && vPortNorm.z!=0 && m_lstConnections.Count()==1)
		{ // up/down entrance portal
			Vec3 vBorder = (vPortNorm.Cross(Vec3(0,1,0.f))).GetNormalized()*fRadius;
			arrPortVerts[0] = vCenter - Vec3(0,1,0.f)*fRadius + vBorder;
			arrPortVerts[1] = vCenter + Vec3(0,1,0.f)*fRadius + vBorder;
			arrPortVerts[2] = vCenter + Vec3(0,1,0.f)*fRadius - vBorder;
			arrPortVerts[3] = vCenter - Vec3(0,1,0.f)*fRadius - vBorder;
		}
		else
		{ // something wrong or areabox portal - use simple solution
			if( vPortNorm.IsEquivalent(Vec3(0,0,0),VEC_EPSILON) )
				vPortNorm = (vCenter-GetCamera().GetPosition()).GetNormalized();

			Vec3 vBorder = (vPortNorm.Cross(Vec3(0,0,1.f))).GetNormalized()*fRadius;
			arrPortVerts[0] = vCenter - Vec3(0,0,1.f)*fRadius - vBorder;
			arrPortVerts[1] = vCenter + Vec3(0,0,1.f)*fRadius - vBorder;
			arrPortVerts[2] = vCenter + Vec3(0,0,1.f)*fRadius + vBorder;
			arrPortVerts[3] = vCenter - Vec3(0,0,1.f)*fRadius + vBorder;
		}

		if(GetCVars()->e_portals==4) // make color recursion dependent
			GetRenderer()->SetMaterialColor(1,1,GetObjManager()->m_nRenderStackLevel==0,1);

    Vec3 vPortalFaceCenter = (arrPortVerts[0]+arrPortVerts[1]+arrPortVerts[2]+arrPortVerts[3])/4;
    vPortToCamDir = CurCamera.GetPosition() - vPortalFaceCenter;
    if(vPortToCamDir.Dot(vPortNorm)<0)
      return;

		const bool Upright	=	(fabsf(vPortNorm.z)<FLT_EPSILON);
		CCamera camParent = CurCamera;

    // clip portal quad by camera planes
    static PodArray<Vec3> lstPortVertsClipped; lstPortVertsClipped.Clear(); 
    lstPortVertsClipped.AddList(arrPortVerts, 4);
    ClipPortalVerticesByCameraFrustum(&lstPortVertsClipped, camParent);

    AABB aabb; aabb.Reset();

    if(lstPortVertsClipped.Count()>2)
    {
      // find screen space bounds of clipped portal 
      for(int i=0; i<lstPortVertsClipped.Count(); i++)
      {
        Vec3 vSS;
        GetRenderer()->ProjectToScreen(lstPortVertsClipped[i].x, lstPortVertsClipped[i].y, lstPortVertsClipped[i].z, &vSS.x, &vSS.y, &vSS.z);
        vSS.y = 100 - vSS.y;
        aabb.Add(vSS);
      }
    }
    else 
    {
      if(!CVisArea::IsSphereInsideVisArea(CurCamera.GetPosition(), CurCamera.GetNearPlane()))
        bCanSeeThruThisArea = false;
    }

    if(lstPortVertsClipped.Count()>2 && aabb.min.z>0.01f)
    {
      static PodArray<Vec3> lstPortVertsSS;
      lstPortVertsSS.PreAllocate(4,4);

      // get 3d positions of portal bounds
      {
        int i=0;
        float w = GetRenderer()->GetWidth();
        float h = GetRenderer()->GetHeight();
        float d = 0.01f;

        GetRenderer()->UnProjectFromScreen(aabb.min.x*w/100, aabb.min.y*h/100, d, &lstPortVertsSS[i].x, &lstPortVertsSS[i].y, &lstPortVertsSS[i].z); i++;
        GetRenderer()->UnProjectFromScreen(aabb.min.x*w/100, aabb.max.y*h/100, d, &lstPortVertsSS[i].x, &lstPortVertsSS[i].y, &lstPortVertsSS[i].z); i++;
        GetRenderer()->UnProjectFromScreen(aabb.max.x*w/100, aabb.max.y*h/100, d, &lstPortVertsSS[i].x, &lstPortVertsSS[i].y, &lstPortVertsSS[i].z); i++;
        GetRenderer()->UnProjectFromScreen(aabb.max.x*w/100, aabb.min.y*h/100, d, &lstPortVertsSS[i].x, &lstPortVertsSS[i].y, &lstPortVertsSS[i].z); i++;

        CurCamera.m_ScissorInfo.x1 = ushort(CLAMP(aabb.min.x*w/100,0,w));
        CurCamera.m_ScissorInfo.y1 = ushort(CLAMP(aabb.min.y*h/100,0,h));
        CurCamera.m_ScissorInfo.x2 = ushort(CLAMP(aabb.max.x*w/100,0,w));
        CurCamera.m_ScissorInfo.y2 = ushort(CLAMP(aabb.max.y*h/100,0,h));
      }

      if(GetCVars()->e_portals==4)
        for(int i=0; i<lstPortVertsSS.Count(); i++)
      {
        float farrColor[4] = { float((nReqursionLevel&1)>0),float((nReqursionLevel&2)>0),float((nReqursionLevel&4)>0),1};
        ColorF c(farrColor[0], farrColor[1], farrColor[2], farrColor[3]);
        DrawSphere(lstPortVertsSS[i],0.002f,c);
        GetRenderer()->DrawLabelEx(lstPortVertsSS[i],0.1f,farrColor,false,true,"%d",i);
      }

      UpdatePortalCameraPlanes(CurCamera, lstPortVertsSS.GetElements(), Upright);

      bCanSeeThruThisArea = 
        (CurCamera.m_ScissorInfo.x1 < CurCamera.m_ScissorInfo.x2) && 
        (CurCamera.m_ScissorInfo.y1 < CurCamera.m_ScissorInfo.y2);
    }

		if(m_bUseDeepness && bCanSeeThruThisArea && barrPortVertsOtherSideValid)
		{
			Vec3 vOtherSideBoxMax = SetMinBB();
			Vec3 vOtherSideBoxMin = SetMaxBB();
			for(int i=0; i<4; i++)
			{
				vOtherSideBoxMin.CheckMin(arrPortVertsOtherSide[i]-Vec3(0.01f,0.01f,0.01f));
				vOtherSideBoxMax.CheckMax(arrPortVertsOtherSide[i]+Vec3(0.01f,0.01f,0.01f));
			}

			bCanSeeThruThisArea = CurCamera.IsAABBVisible_E(AABB(vOtherSideBoxMin,vOtherSideBoxMax));
		}

		if(bCanSeeThruThisArea && pParent && m_lstConnections.Count()==1)
		{ // set this camera for outdoor
      if(nReqursionLevel>=1)
      {
        if(!m_bSkyOnly)
        {
          if(plstOutPortCameras)
			    {
				    plstOutPortCameras->Add(CurCamera);
				    plstOutPortCameras->Last().m_pPortal = this;
			    }
			    if(pbOutdoorVisible)
				    *pbOutdoorVisible = true;
        }
				else if(pbSkyVisible)
					*pbSkyVisible = true;
      }

			return;
		}
	}

	// sort portals by distance
  if(!m_bThisIsPortal && m_lstConnections.Count())
  {
    for(int p=0; p<m_lstConnections.Count(); p++)
    {
      CVisArea * pNeibVolume = m_lstConnections[p];
      pNeibVolume->m_fDistance = CurCamera.GetPosition().GetDistance((pNeibVolume->m_boxArea.min+pNeibVolume->m_boxArea.max)*0.5f);
    }

    qsort(&m_lstConnections[0], m_lstConnections.Count(), 
      sizeof(m_lstConnections[0]), CVisAreaManager__CmpDistToPortal);
  }

  if(m_bOceanVisible && pbOceanVisible)
    *pbOceanVisible = true;

	// recurse to connections
	for(int p=0; p<m_lstConnections.Count(); p++)
	{
		CVisArea * pNeibVolume = m_lstConnections[p];
		if(pNeibVolume != pParent)
		{
			if(!m_bThisIsPortal)
			{ // skip far portals
				float fRadius = (pNeibVolume->m_boxArea.max-pNeibVolume->m_boxArea.min).GetLength()*0.5f;
				if(pNeibVolume->m_fDistance*m_fZoomFactor > fRadius*pNeibVolume->m_fViewDistRatio)
					continue;

				Vec3 vPortNorm = (this == pNeibVolume->m_lstConnections[0] || pNeibVolume->m_lstConnections.Count()==1) ? 
					pNeibVolume->m_vConnNormals[0] : pNeibVolume->m_vConnNormals[1];
			
				// back face check
				Vec3 vPortToCamDir = CurCamera.GetPosition() - pNeibVolume->GetAABBox()->GetCenter();
				if(vPortToCamDir.Dot(vPortNorm)<0)
					continue;
			}

			if((bCanSeeThruThisArea || m_lstConnections.Count()==1) && (m_bThisIsPortal || CurCamera.IsAABBVisible_F( pNeibVolume->m_boxStatics )))
				pNeibVolume->PreRender(nReqursionLevel-1, CurCamera, this, pCurPortal, pbOutdoorVisible, plstOutPortCameras, pbSkyVisible, pbOceanVisible, lstVisibleAreas);
		}
	}

	if(GetCVars()->e_ram_maps==1)
	{
		static PodArray<IRenderNode*>	lstLights;
		lstLights.Clear();
		if(m_pObjectsTree)
			m_pObjectsTree->GetObjectsByType(lstLights,eERType_Light,NULL);
		if(IsPortal())
			m_RAMSolution[m_RAMFrame&1].CalcLightingPortal(this,m_RAMSolution[(m_RAMFrame&1)^1],m_lstConnections,lstLights);
		else
			m_RAMSolution[m_RAMFrame&1].CalcLightingCell(this,m_RAMSolution[(m_RAMFrame&1)^1],m_lstConnections,lstLights);
		m_RAMFrame++;
	}
	else if(GetCVars()->e_ram_maps==2)
	{
		m_RAMSolution[0].Reset();
		m_RAMSolution[1].Reset();
	}
}

//! return list of visareas connected to specified visarea (can return portals and sectors)
int CVisArea::GetRealConnections(IVisArea ** pAreas, int nMaxConnNum, bool bSkipDisabledPortals)
{
  int nOut = 0;
  for(int nArea=0; nArea<m_lstConnections.Count(); nArea++)
  {
    if(nOut<nMaxConnNum)
      pAreas[nOut] = (IVisArea*)m_lstConnections[nArea];
    nOut++;
  }
  return nOut;
}

//! return list of sectors conected to specified sector or portal (returns sectors only)
// todo: change the way it returns data
int CVisArea::GetVisAreaConnections(IVisArea ** pAreas, int nMaxConnNum, bool bSkipDisabledPortals)
{
	int nOut = 0;
	if(IsPortal())
	{
/*		for(int nArea=0; nArea<m_lstConnections.Count(); nArea++)
		{
			if(nOut<nMaxConnNum)
				pAreas[nOut] = (IVisArea*)m_lstConnections[nArea];
			nOut++;
		}*/
    return min(nMaxConnNum,GetRealConnections(pAreas, nMaxConnNum, bSkipDisabledPortals));
	}
	else
	{
		for(int nPort=0; nPort<m_lstConnections.Count(); nPort++)
		{
			CVisArea * pPortal = m_lstConnections[nPort];
			assert(pPortal->IsPortal());
			for(int nArea=0; nArea<pPortal->m_lstConnections.Count(); nArea++)
			{
				if(pPortal->m_lstConnections[nArea]!=this)
          if(!bSkipDisabledPortals || pPortal->IsActive())
				{
					if(nOut<nMaxConnNum)
						pAreas[nOut] = (IVisArea*)pPortal->m_lstConnections[nArea];
					nOut++;
					break; // take first valid connection
				}
			}
		}
	}

	return min(nMaxConnNum,nOut);
}

bool CVisArea::IsPortalValid()
{
	if(m_lstConnections.Count()>2 || m_lstConnections.Count()==0)
		return false;

	for(int i=0; i<m_lstConnections.Count(); i++)
	if(m_vConnNormals[i].IsEquivalent(Vec3(0,0,0),VEC_EPSILON))
		return false;

  if(m_lstConnections.Count()>1)
    if( m_vConnNormals[0].Dot(m_vConnNormals[1])>-0.99f )
      return false;

	return true;
}

bool CVisArea::IsPortalIntersectAreaInValidWay(CVisArea * pPortal)
{
	const Vec3 & v1Min = pPortal->m_boxArea.min;
	const Vec3 & v1Max = pPortal->m_boxArea.max;
	const Vec3 & v2Min = m_boxArea.min;
	const Vec3 & v2Max = m_boxArea.max;

	if(v1Max.x>v2Min.x && v2Max.x>v1Min.x)
	if(v1Max.y>v2Min.y && v2Max.y>v1Min.y)
	if(v1Max.z>v2Min.z && v2Max.z>v1Min.z)
	{
		// vertical portal
		for(int v=0; v<m_lstShapePoints.Count(); v++)
		{
			int nIntersNum=0;
			bool arrIntResult[4] = { 0,0,0,0 };
			for(int p=0; p<pPortal->m_lstShapePoints.Count() && p<4; p++)
			{
				const Vec3 & v0 = m_lstShapePoints[v];
				const Vec3 & v1 = m_lstShapePoints[(v+1)%m_lstShapePoints.Count()];
				const Vec3 & p0 = pPortal->m_lstShapePoints[p];
				const Vec3 & p1 = pPortal->m_lstShapePoints[(p+1)%pPortal->m_lstShapePoints.Count()];

				if(Is2dLinesIntersect(v0.x,v0.y,v1.x,v1.y,p0.x,p0.y,p1.x,p1.y))
				{
					nIntersNum++;
					arrIntResult[p] = true;
				}
			}
			if(nIntersNum==2 && arrIntResult[0]==arrIntResult[2] && arrIntResult[1]==arrIntResult[3])
				return true;
		}

		// horisontal portal
		{
			int nBottomPoints=0, nUpPoints=0;
			for(int p=0; p<pPortal->m_lstShapePoints.Count() && p<4; p++)
				if(IsPointInsideVisArea(pPortal->m_lstShapePoints[p]))
					nBottomPoints++;

			for(int p=0; p<pPortal->m_lstShapePoints.Count() && p<4; p++)
				if(IsPointInsideVisArea(pPortal->m_lstShapePoints[p]+Vec3(0,0,pPortal->m_fHeight)))
					nUpPoints++;

			if(nBottomPoints==0 && nUpPoints==4)
				return true;

			if(nBottomPoints==4 && nUpPoints==0)
				return true;
		}
	}

	return false;
}

bool CVisArea::IsPortal()
{
	return m_bThisIsPortal;
}
/*
void CVisArea::SetTreeId(int nTreeId)
{
	if(m_nTreeId == nTreeId)
		return;

	m_nTreeId = nTreeId;

	for(int p=0; p<m_lstConnections.Count(); p++)
		m_lstConnections[p]->SetTreeId(nTreeId);
}
*/
bool CVisArea::IsShapeClockwise()
{
	float fClockWise = 
		(m_lstShapePoints[0].x-m_lstShapePoints[1].x)*(m_lstShapePoints[2].y-m_lstShapePoints[1].y)-
		(m_lstShapePoints[0].y-m_lstShapePoints[1].y)*(m_lstShapePoints[2].x-m_lstShapePoints[1].x);        

	return fClockWise>0;
}

void CVisArea::DrawAreaBoundsIntoCBuffer(CCullBuffer * pCBuffer)
{
	assert(!"temprary not supported");

/*  if(m_lstShapePoints.Count()!=4)
    return;
 
  Vec3 arrVerts[8];
  int arrIndices[24];

  int v=0;
  int i=0;
  for(int p=0; p<4 && p<m_lstShapePoints.Count(); p++)
  {
    arrVerts[v++] = m_lstShapePoints[p];
    arrVerts[v++] = m_lstShapePoints[p] + Vec3(0,0,m_fHeight);

    arrIndices[i++] = (p*2+0)%8;
    arrIndices[i++] = (p*2+1)%8;
    arrIndices[i++] = (p*2+2)%8;
    arrIndices[i++] = (p*2+3)%8;
    arrIndices[i++] = (p*2+2)%8;
    arrIndices[i++] = (p*2+1)%8;
  }

  Matrix34 mat;
  mat.SetIdentity();
  pCBuffer->AddMesh(arrVerts,8,arrIndices,24,&mat);*/
}

void CVisArea::CalcAmbientCube(f32* pAmbientCube,const Vec3& rAABBMin,const Vec3& rAABBMax)
{
	FUNCTION_PROFILER_3DENGINE;
	if(IsPortal())
		m_RAMSolution[m_nRndFrameId&1].CalcAmbientCubePortal(this,pAmbientCube,rAABBMin,rAABBMax);
	else
		m_RAMSolution[m_nRndFrameId&1].CalcAmbientCubeArea(pAmbientCube,rAABBMin,rAABBMax);
}

void CVisArea::CalcHQAmbientCube(f32* pAmbientCube,const Vec3& rAABBMin,const Vec3& rAABBMax)
{
	FUNCTION_PROFILER_3DENGINE;
	if(IsPortal())
		m_RAMSolution[m_nRndFrameId&1].CalcHQAmbientCubePortal(this,pAmbientCube,rAABBMin,rAABBMax);
	else
		m_RAMSolution[m_nRndFrameId&1].CalcHQAmbientCubeArea(pAmbientCube,rAABBMin,rAABBMax);
}


void CVisArea::ClipPortalVerticesByCameraFrustum(PodArray<Vec3> * pPolygon, const CCamera & cam)
{
  CCoverageBuffer::ClipPolygon(pPolygon, *cam.GetFrustumPlane(FR_PLANE_RIGHT));
  CCoverageBuffer::ClipPolygon(pPolygon, *cam.GetFrustumPlane(FR_PLANE_LEFT));
  CCoverageBuffer::ClipPolygon(pPolygon, *cam.GetFrustumPlane(FR_PLANE_TOP));
  CCoverageBuffer::ClipPolygon(pPolygon, *cam.GetFrustumPlane(FR_PLANE_BOTTOM));
  CCoverageBuffer::ClipPolygon(pPolygon, *cam.GetFrustumPlane(FR_PLANE_NEAR));
}

void CVisArea::GetMemoryUsage(ICrySizer*pSizer)
{
//  pSizer->AddContainer(m_lstEntities[STATIC_OBJECTS]);
  //pSizer->AddContainer(m_lstEntities[DYNAMIC_OBJECTS]);

	// TODO: include obects tree


	if(m_pObjectsTree)
	{
		SIZER_COMPONENT_NAME(pSizer, "IndoorObjectsTree");
		pSizer->AddObject(m_pObjectsTree, m_pObjectsTree->GetMemoryUsage(pSizer));
	}

//  for(int nStatic=0; nStatic<2; nStatic++)
  //for(int i=0; i<m_lstEntities[nStatic].Count(); i++)
//    nSize += m_lstEntities[nStatic][i].pNode->GetMemoryUsage();

  pSizer->AddObject(this,sizeof(*this));
}

void CVisArea::UpdateOcclusionFlagInTerrain()
{
  if(m_bAffectedByOutLights && !m_bThisIsPortal)
  {
    Vec3 vCenter = GetAABBox()->GetCenter();
    if(vCenter.z < GetTerrain()->GetZApr(vCenter.x, vCenter.y))
    {
      AABB box = *GetAABBox();
      box.min.z=0; box.min-=Vec3(8,8,8);
      box.max.z=16000; box.max+=Vec3(8,8,8);
      static PodArray<CTerrainNode*> lstResult; lstResult.Clear();
      GetTerrain()->IntersectWithBox(box, &lstResult);
      for(int i=0; i<lstResult.Count(); i++)
        if(lstResult[i]->m_nTreeLevel<=2)
          lstResult[i]->m_bNoOcclusion = true;
    }
  }
}
