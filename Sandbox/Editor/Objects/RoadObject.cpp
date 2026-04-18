////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   RoadObject.cpp
//  Version:     v1.00
//  Created:     25/07/2005 by Sergiy Shaykin.
//  Compilers:   Visual C++ 6.0
//  Description: CRoadObject implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "RoadObject.h"
#include "..\RoadPanel.h"
#include "..\Viewport.h"
#include "Util\Triangulate.h"
#include "Heightmap.h"
#include "Material\Material.h"

#include <I3DEngine.h>
#include <CryArray.h>

//////////////////////////////////////////////////////////////////////////
// class CRoad Sector


//bool CRoadSector::isHidden=false;

/*
void CRoadSector::Init(CMaterial * pMat)
{
	if(points[0]!=points[1] && points[0]!=points[2] && points[1]!=points[2] && points[2]!=points[3] && points[1]!=points[3])
	{
		if(!m_pRoadSector)
			m_pRoadSector = GetIEditor()->Get3DEngine()->CreateRenderNode(eERType_RoadObject);
		if(m_pRoadSector)
		{
			int renderFlags = 0;
			if(isHidden)
				renderFlags = ERF_HIDDEN;

			m_pRoadSector->SetRndFlags( renderFlags );

			((IRoadRenderNode *)m_pRoadSector)->SetVertices(&points[0], points.size(), t0, t1, t0, t1);
			if(pMat)
				pMat->AssignToEntity( m_pRoadSector );
			else
				m_pRoadSector->SetMaterial(0);
		}
	}
}
*/

void CRoadSector::Release()
{
	if(m_pRoadSector)
		GetIEditor()->Get3DEngine()->DeleteRenderNode(m_pRoadSector);
	m_pRoadSector = 0;
}


//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CRoadObject,CBaseObject)

#define RAY_DISTANCE 100000.0f

//////////////////////////////////////////////////////////////////////////
int CRoadObject::m_rollupId = 0;
CRoadPanel* CRoadObject::m_panel = 0;

//////////////////////////////////////////////////////////////////////////
CRoadObject::CRoadObject()
{
	m_bForce2D = false;
	m_bNeedUpdateSectors = true;
	mv_width = 4.0f;
	mv_borderWidth = 6.0f;
	mv_step = 4.0f;
	mv_tileLength = 4.0f;
	mv_sortPrio = 0;

	m_bIgnoreParamUpdate = false;
	
	m_bbox.min = m_bbox.max = Vec3(0,0,0);
	m_selectedPoint = -1;
	//m_entityClass = "AreaRoad";

	SetColor( Vec2Rgb(Vec3(0,0.8f,1)) );
	mv_ratioViewDist = 100;

  m_pszEditParams = "Road Parameters";
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::Done()
{
	m_points.clear();
	m_sectors.clear();
	__super::Done();
}

//////////////////////////////////////////////////////////////////////////
bool CRoadObject::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	bool res = __super::Init( ie,prev,file );

	if (prev)
	{
		m_points = ((CRoadObject*)prev)->m_points;
		CalcBBox();
		SetRoadSectors();
	}

	return res;
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::InitBaseVariables()
{
	AddVariable( mv_width,"Width",functor(*this,&CRoadObject::OnParamChange) );
	AddVariable( mv_borderWidth,"BorderWidth",functor(*this,&CRoadObject::OnParamChange) );
	AddVariable( mv_step,"StepSize",functor(*this,&CRoadObject::OnParamChange) );
	AddVariable( mv_ratioViewDist,"ViewDistRatio",functor(*this,&CRoadObject::OnParamChange) );
	mv_ratioViewDist.SetLimits( 0,255 );
	AddVariable( mv_tileLength,"TileLength",functor(*this,&CRoadObject::OnParamChange) );
	mv_step.SetLimits(1.0f, 10.f);
	mv_tileLength.SetLimits(0.001f, 1000.f);
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::InitVariables()
{
	InitBaseVariables();

	AddVariable(mv_sortPrio, "SortPriority", functor(*this,&CRoadObject::OnParamChange));
	mv_sortPrio.SetLimits(0, 255);
}

//////////////////////////////////////////////////////////////////////////
CString CRoadObject::GetUniqueName() const
{
	char prefix[32];
	sprintf(prefix, "%p ", this);
	return CString(prefix) + GetName();
}


//////////////////////////////////////////////////////////////////////////
void CRoadObject::InvalidateTM( int nWhyFlags )
{
	__super::InvalidateTM(nWhyFlags);
	SetRoadSectors();
//	CalcBBox();
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::GetBoundBox( BBox &box )
{
	box.SetTransformedAABB( GetWorldTM(),m_bbox );
	float s = 1.0f;
	box.min -= Vec3(s,s,s);
	box.max += Vec3(s,s,s);
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::GetLocalBounds( BBox &box )
{
	box = m_bbox;
}

//////////////////////////////////////////////////////////////////////////
bool CRoadObject::HitTest( HitContext &hc )
{
	// First check if ray intersect our bounding box.
	float tr = hc.distanceTollerance/2 + ROAD_CLOSE_DISTANCE;

	// Find intersection of line with zero Z plane.
	float minDist = FLT_MAX;
	Vec3 intPnt;
	//GetNearestEdge( hc.raySrc,hc.rayDir,p1,p2,minDist,intPnt );

	bool bWasIntersection = false;
	Vec3 ip;
	Vec3 rayLineP1 = hc.raySrc;
	Vec3 rayLineP2 = hc.raySrc + hc.rayDir*RAY_DISTANCE;
	const Matrix34 &wtm = GetWorldTM();

	for (int i = 0; i < m_points.size(); i++)
	{
		int j = (i < m_points.size()-1) ? i+1 : 0;

		if (j == 0 && i != 0)
			continue;

		int kn=6;
		for(int k=0; k<kn; k++)
		{
			Vec3 pi = wtm.TransformPoint(GetBezierPos(m_points, i, float(k)/kn));
			Vec3 pj = wtm.TransformPoint(GetBezierPos(m_points, i, float(k)/kn+1.0f/kn));

			float d = 0;
			if (RayToLineDistance( rayLineP1,rayLineP2,pi,pj,d,ip ))
			{
				if (d < minDist)
				{
					bWasIntersection = true;
					minDist = d;
					intPnt = ip;
				}
			}
		}
	}

	float fRoadCloseDistance = ROAD_CLOSE_DISTANCE*hc.view->GetScreenScaleFactor(intPnt) * 0.01f;

	if (bWasIntersection && minDist < fRoadCloseDistance+hc.distanceTollerance)
	{
		hc.dist = hc.raySrc.GetDistance(intPnt);
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::BeginEditParams( IEditor *ie,int flags )
{
	CBaseObject::BeginEditParams( ie,flags );

	if (!m_panel)
	{
		m_panel = new CRoadPanel;
		m_rollupId = ie->AddRollUpPage( ROLLUP_OBJECTS, m_pszEditParams, m_panel );
	}
	if (m_panel)
		m_panel->SetRoad( this );
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::EndEditParams( IEditor *ie )
{
	if (m_rollupId != 0)
	{
		ie->RemoveRollUpPage( ROLLUP_OBJECTS,m_rollupId );
		//CalcBBox();
	}
	m_rollupId = 0;
	m_panel = 0;

	__super::EndEditParams( ie );
}

//////////////////////////////////////////////////////////////////////////
int CRoadObject::MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	if (event == eMouseMove || event == eMouseLDown || event == eMouseLDblClick)
	{
		Vec3 pos = view->MapViewToCP(point);

		if (m_points.size() < 2)
		{
			SetPos( pos );
		}

		pos.z += ROAD_Z_OFFSET;
		
		if (m_points.size() == 0)
		{
			InsertPoint( -1,Vec3(0,0,0) );
		}
		else
		{
			SetPoint( m_points.size()-1,pos - GetWorldPos() );
		}

		if (event == eMouseLDblClick)
		{
			if (m_points.size() > GetMinPoints())
			{
				// Remove last unneeded point.
				m_points.pop_back();
				SetRoadSectors();
				EndCreation();
				return MOUSECREATE_OK;
			}
			else
				return MOUSECREATE_ABORT;
		}

		if (event == eMouseLDown)
		{
			Vec3 vlen = Vec3(pos.x,pos.y,0) - Vec3(GetWorldPos().x,GetWorldPos().y,0);
			if (m_points.size() > 2 && vlen.GetLength() < ROAD_CLOSE_DISTANCE)
			{
				EndCreation();
				return MOUSECREATE_OK;
			}
			if (GetPointCount() >= GetMaxPoints())
			{
				EndCreation();
				return MOUSECREATE_OK;
			}

			InsertPoint( -1,pos-GetWorldPos() );
		}
		return MOUSECREATE_CONTINUE;
	}
	return __super::MouseCreateCallback( view,event,point,flags );
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::EndCreation()
{
	if (m_panel)
		m_panel->SetRoad(this);
}

//////////////////////////////////////////////////////////////////////////
Vec3 CRoadObject::GetBezierPos(CRoadPointVector & points, int index, float t)
{
	//Vec3 p0 = points[index].pos;
	//Vec3 pf0 = points[index].forw;
	//Vec3 p1 = points[index+1].pos;
	//Vec3 pb1 = points[index+1].back;
	return	points[index].pos*		((1-t)*(1-t)*(1-t))+
					points[index].forw*		(3*t*(1-t)*(1-t))+
					points[index+1].back*	(3*t*t*(1-t))+
					points[index+1].pos*	(t*t*t);
}


float CRoadObject::GetLocalWidth(int index, float t)
{
	float kof = t;
	int i = index;

	if(index>=m_points.size()-1)
	{
		kof = 1.0f+t;
		i = m_points.size()-2;
		if(i<0)
			return mv_width;
	}

	float an1 = m_points[i  ].isDefaultWidth ? mv_width : m_points[i  ].width;
	float an2 = m_points[i+1].isDefaultWidth ? mv_width : m_points[i+1].width;

	if(an1 == an2)
		return an1;

	float af = kof*2 - 1.0f;
	float ed = 1.0f;
	if(af<0.0f)
		ed = -1.0f;
	//af = ed-af;
	af = af;
	//af = ed-af;
	af = (af + 1.0f)/2;
	return ((1.0f-af)*an1 + af*an2);
}


//////////////////////////////////////////////////////////////////////////
Vec3 CRoadObject::GetLocalBezierNormal(int index, float t)
{
	float kof = t;
	int i = index;

	if(index>=m_points.size()-1)
	{
		kof = 1.0f+t;
		i = m_points.size()-2;
		if(i<0)
			return Vec3(0, 0, 0);
	}

	Vec3 e = GetBezierPos(m_points, i, kof+0.0001f)-GetBezierPos(m_points, i, kof-0.0001f);
	if(e.x<0.00001f && e.x>-0.00001f && e.y<0.00001f && e.y>-0.00001f && e.z<0.00001f && e.z>-0.00001f)
		return Vec3(0, 0, 0);

	Vec3 n;

	float an1 = m_points[i].angle;
	float an2 = m_points[i+1].angle;

	if(-0.00001f>an1 || an1>0.00001f || -0.00001f>an2 || an2>0.00001f)
	{
		float af = kof*2 - 1.0f;
		float ed = 1.0f;
		if(af<0.0f)
			ed = -1.0f;
		af = ed-af;
		af = af*af*af;
		af = ed-af;
		af = (af + 1.0f)/2;
		float angle = ((1.0f-af)*an1 + af*an2)*3.141593f/180.0f;

		e.Normalize();
		n = e.Cross(Vec3(0,0,1));
		n = n.GetRotated(e,angle);
	}
	else
		n = e.Cross(Vec3(0,0,1));

	if(n.x>0.00001f || n.x<-0.00001f || n.y>0.00001f || n.y<-0.00001f || n.z>0.00001f || n.z<-0.00001f)
		n.Normalize();
	return n;
}


//////////////////////////////////////////////////////////////////////////
Vec3 CRoadObject::GetBezierNormal(int index, float t)
{
	float kof = 0.0f;
	int i = index;
	if(index>=m_points.size()-1)
	{
		kof = 1.0f;
		i = m_points.size()-2;
		if(i<0)
			return Vec3(0, 0, 0);
	}
	const Matrix34 &wtm = GetWorldTM();
	Vec3 p0 = wtm.TransformPoint(GetBezierPos(m_points, i, t+0.0001f+kof));
	Vec3 p1 = wtm.TransformPoint(GetBezierPos(m_points, i, t-0.0001f+kof));

	Vec3 e = p0-p1;
	Vec3 n = e.Cross(Vec3(0,0,1));

	//if(n.GetLength()>0.00001f)
	if(n.x>0.00001f || n.x<-0.00001f || n.y>0.00001f || n.y<-0.00001f || n.z>0.00001f || n.z<-0.00001f)
		n.Normalize();
	return n;
}

//////////////////////////////////////////////////////////////////////////
float CRoadObject::GetBezierSegmentLength(int index, float t)
{
	const Matrix34 &wtm = GetWorldTM();
	int kn=int(t*20)+1;
	float fRet = 0.0f;
	for (int k=0; k<kn; k++)
	{
		Vec3 e = GetBezierPos(m_points, index, t*float(k)/kn ) - 
		GetBezierPos(m_points, index, t*(float(k)/kn+1.0f/kn));
		fRet += e.GetLength();
	}
	return fRet;
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::SetRoadSectors()
{
	const Matrix34 &wtm = GetWorldTM();

	int sizeOld = m_sectors.size();
	int sizeNew = 0;

	int points_size = m_points.size();

	float fSegLen = 0.0f;

	for (int i = 0; i < points_size-1; i++)
	{
		int kn=int((GetBezierSegmentLength(i)+0.5f) / mv_step);
		if(kn==0)
			kn = 1;
		for (int k=0; k<=kn; k++)
		{
			if(i != points_size-2 && k==kn)
				break;
			float t = float(k)/kn;
			Vec3 p = GetBezierPos(m_points, i, t);
			Vec3 n = GetLocalBezierNormal(i, t);

			float fWidth = GetLocalWidth(i, t);

			Vec3 r = p + 0.5f*fWidth*n;
			Vec3 l = p - 0.5f*fWidth*n;

			if(sizeNew >= sizeOld)
			{
				CRoadSector sector;
				m_sectors.push_back(sector);
			}

			m_sectors[sizeNew].points.clear();
			m_sectors[sizeNew].points.push_back(l);
			m_sectors[sizeNew].points.push_back(r);
			m_sectors[sizeNew].points.push_back(l);
			m_sectors[sizeNew].points.push_back(r);

			m_sectors[sizeNew].t0 = (fSegLen + GetBezierSegmentLength(i, t))/mv_tileLength;

			sizeNew++;
		}
		fSegLen += GetBezierSegmentLength(i);
	}

	if(sizeNew<sizeOld)
		m_sectors.resize(sizeNew);

	for (int i = 0; i < m_sectors.size(); i++)
	{
		m_sectors[i].points[0] = wtm.TransformPoint(m_sectors[i].points[0]);
		m_sectors[i].points[1] = wtm.TransformPoint(m_sectors[i].points[1]);

		if(i > 0)
		{
			// adjust mesh
			m_sectors[i-1].points[2] = m_sectors[i].points[0];
			m_sectors[i-1].points[3] = m_sectors[i].points[1];
			m_sectors[i-1].t1 = m_sectors[i].t0;
		}
	}

	if(m_sectors.size()>0)
		m_sectors.pop_back();

	// mark end of the road for road alpha fading
	if(m_sectors.size()>0)
		m_sectors[m_sectors.size()-1].t1 *= -1.f;

	// call only in the end of this function
	UpdateSectors();
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::UpdateSector( CRoadSector &sector )
{
/*	CMaterial *pMat = GetMaterial();
	if (!sector.m_pRoadSector)
		sector.m_pRoadSector = GetIEditor()->Get3DEngine()->CreateRenderNode(eERType_RoadObject);
	if (sector.m_pRoadSector)
	{
		int renderFlags = 0;
		if (sector.isHidden)
			renderFlags = ERF_HIDDEN;

		sector.m_pRoadSector->SetRndFlags( renderFlags );

		((IRoadRenderNode *)sector.m_pRoadSector)->SetVertices(&sector.points[0],sector.points.size(),sector.t0,sector.t1);
		if (pMat)
			pMat->AssignToEntity( sector.m_pRoadSector );
		else
			sector.m_pRoadSector->SetMaterial(0);
	}*/
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::UpdateSectors()
{
	if(!m_bNeedUpdateSectors)
		return;

	if(!m_sectors.size())
		return;

	bool isHidden = CheckFlags(OBJFLAG_INVISIBLE);

	for(int i=0; i<m_sectors.size(); i++)
	{
		if(m_sectors[i].m_pRoadSector)
			GetIEditor()->Get3DEngine()->DeleteRenderNode(m_sectors[i].m_pRoadSector);
		m_sectors[i].m_pRoadSector = 0;
	}

	const int MAX_TRAPEZOIDS_IN_CHUNK = 32;

	int nChunksNum = m_sectors.size()/MAX_TRAPEZOIDS_IN_CHUNK+1;

	CRoadSector &sectorFirstGlobal = m_sectors[0];
	CRoadSector &sectorLastGlobal = m_sectors[m_sectors.size()-1];

	for(int nChunkId=0; nChunkId<nChunksNum; nChunkId++)
	{
		int nStartSecId = nChunkId*MAX_TRAPEZOIDS_IN_CHUNK;
		int nSectorsNum = min(MAX_TRAPEZOIDS_IN_CHUNK, (int)m_sectors.size()-nStartSecId);

		CRoadSector &sectorFirst = m_sectors[nStartSecId];

		// make render node
		if (!sectorFirst.m_pRoadSector)
			sectorFirst.m_pRoadSector = GetIEditor()->Get3DEngine()->CreateRenderNode(eERType_RoadObject_NEW);
		if (!sectorFirst.m_pRoadSector)
			return;
		int renderFlags = 0;
		if (isHidden)
			renderFlags = ERF_HIDDEN;
		sectorFirst.m_pRoadSector->SetRndFlags( renderFlags );
		sectorFirst.m_pRoadSector->SetViewDistRatio( mv_ratioViewDist );
		sectorFirst.m_pRoadSector->SetMinSpec( GetMinSpec() );
		sectorFirst.m_pRoadSector->SetMaterialLayers( GetMaterialLayersMask() );

		// make list of verts
		PodArray<Vec3> lstPoints;
		for (int i = 0; i < nSectorsNum; i++)
		{
			lstPoints.Add(m_sectors[nStartSecId+i].points[0]);
			lstPoints.Add(m_sectors[nStartSecId+i].points[1]);
		}

		CRoadSector &sectorLast = m_sectors[nStartSecId+nSectorsNum-1];
		lstPoints.Add(sectorLast.points[2]);
		lstPoints.Add(sectorLast.points[3]);

		//assert(lstPoints.Count()>=4);
		assert(!(lstPoints.Count()&1));

		IRoadRenderNode* pRoadRN = static_cast<IRoadRenderNode*>(sectorFirst.m_pRoadSector);
		pRoadRN->SetVertices(lstPoints.GetElements(), lstPoints.Count(), fabs(sectorFirst.t0), fabs(sectorLast.t1), fabs(sectorFirstGlobal.t0), fabs(sectorLastGlobal.t1));
		pRoadRN->SetSortPriority(mv_sortPrio);

		if (GetMaterial())
			GetMaterial()->AssignToEntity( sectorFirst.m_pRoadSector );
		else
			sectorFirst.m_pRoadSector->SetMaterial(NULL);
	}
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::SetHidden( bool bHidden )
{
	CBaseObject::SetHidden( bHidden );
	//CRoadSector::isHidden = bHidden;
	UpdateSectors();
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::UpdateVisibility( bool visible )
{
	if (visible == CheckFlags(OBJFLAG_INVISIBLE))
	{
		CBaseObject::UpdateVisibility(visible);
		//CBaseObject::SetHidden( bHidden );
		//CRoadSector::isHidden = !visible;
		UpdateSectors();
	}
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::SetMaterial( CMaterial *mtl )
{
	CBaseObject::SetMaterial(mtl);
	UpdateSectors();
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::DrawBezierSpline(DisplayContext &dc, CRoadPointVector & points, COLORREF col, bool isDrawJoints, bool isDrawRoad)
{
	const Matrix34 &wtm = GetWorldTM();
	float fPointSize = 0.5f;
	
	if(isDrawRoad)
	{
		dc.SetColor( RGB(127, 127, 255));
		for (int i = 0; i < m_sectors.size(); i++)
		{
			dc.DrawLine(m_sectors[i].points[0], m_sectors[i].points[1]);
			for (int k = 0; k < m_sectors[i].points.size(); k+=2)
			{
				if(k+3 < m_sectors[i].points.size())
				{
					dc.DrawLine(m_sectors[i].points[k+1], m_sectors[i].points[k+3]);
					dc.DrawLine(m_sectors[i].points[k+3], m_sectors[i].points[k+2]);
					dc.DrawLine(m_sectors[i].points[k+2], m_sectors[i].points[k]);
				}
			}
		}
	}
	
	
	dc.SetColor( col );
	for (int i = 0; i < points.size(); i++)
	{
		Vec3 p0 = wtm.TransformPoint(points[i].pos);

		// Draw Bezier joints
		if (isDrawJoints)
		{
			ColorB colf = dc.GetColor();

			Vec3 pf0 = wtm.TransformPoint(points[i].forw);
			float fScalef = fPointSize*dc.view->GetScreenScaleFactor(pf0) * 0.01f;
			Vec3 szf(fScalef,fScalef,fScalef);
			dc.SetColor( RGB(0, 200, 0));
			dc.DrawLine( p0,pf0 );
			dc.SetColor( RGB(0, 255, 0));
			dc.DrawWireBox( pf0-szf,pf0+szf );
		
			Vec3 pb0 = wtm.TransformPoint(points[i].back);
			float fScaleb = fPointSize*dc.view->GetScreenScaleFactor(pb0) * 0.01f;
			Vec3 szb(fScaleb,fScaleb,fScaleb);
			dc.SetColor( RGB(0, 200, 0));
			dc.DrawLine( p0,pb0 );
			dc.SetColor( RGB(0, 255, 0));
			dc.DrawWireBox( pb0-szb,pb0+szb );

			dc.SetColor( colf );
		}

		if(i < points.size()-1)
		{
			int kn=int((GetBezierSegmentLength(i)+0.5f) / mv_step);
			if(kn==0)
				kn = 1;
			for (int k=0; k<kn; k++)
			{
				Vec3 p = wtm.TransformPoint(GetBezierPos(points, i, float(k)/kn));
				Vec3 p1 = wtm.TransformPoint(GetBezierPos(points, i, float(k)/kn+1.0f/kn));
				dc.DrawLine(p, p1);
			}
		}

		float fScale = fPointSize*dc.view->GetScreenScaleFactor(p0) * 0.01f;
		Vec3 sz(fScale,fScale,fScale);
		dc.DrawWireBox( p0-sz,p0+sz );
	}
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::Display( DisplayContext &dc )
{
	//dc.renderer->EnableDepthTest(false);
	
	const Matrix34 &wtm = GetWorldTM();
	COLORREF col = 0;

	float fPointSize = 0.5f;

	bool bPointSelected = false;
	if (m_selectedPoint >= 0 && m_selectedPoint < m_points.size())
	{
		bPointSelected = true;
	}

	if (m_points.size() > 1)
	{
		if ((IsSelected() || IsHighlighted()))
		{
			col = dc.GetSelectedColor();
			dc.SetColor( col );
		}
		else
		{
			if (IsFrozen())
				dc.SetFreezeColor();
			else
				dc.SetColor( GetColor() );
			col = GetColor();
		}

		DrawBezierSpline(dc, m_points, col, false, IsSelected());

		// Draw selected point.
		if (bPointSelected)
		{
			dc.SetColor( RGB(0, 0, 255));
			//dc.SetSelectedColor( 0.5f );

			Vec3 selPos = wtm.TransformPoint(m_points[m_selectedPoint].pos);
			//dc.renderer->DrawBall( selPos,1.0f );
			float fScale = fPointSize*dc.view->GetScreenScaleFactor(selPos) * 0.01f;
			Vec3 sz(fScale,fScale,fScale);
			dc.DrawWireBox( selPos-sz,selPos+sz );

			DrawAxis( dc,m_points[m_selectedPoint].pos,0.15f );
		}
	}

	DrawDefault(dc,GetColor());
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::Serialize( CObjectArchive &ar )
{
	m_bIgnoreParamUpdate = true;

	m_bNeedUpdateSectors = false;
	__super::Serialize( ar );
	m_bNeedUpdateSectors = true;

	XmlNodeRef xmlNode = ar.node;
	if (ar.bLoading)
	{
		m_bNeedUpdateSectors = false;
		m_points.clear();
		XmlNodeRef points = xmlNode->findChild( "Points" );
		if (points)
		{
			for (int i = 0; i < points->getChildCount(); i++)
			{
				XmlNodeRef pnt = points->getChild(i);
				CRoadPoint pt;
				pnt->getAttr( "Pos",pt.pos );
				pnt->getAttr( "Back",pt.back );
				pnt->getAttr( "Forw",pt.forw );
				pnt->getAttr( "Angle",pt.angle );
				pnt->getAttr( "Width",pt.width );
				pnt->getAttr( "IsDefaultWidth",pt.isDefaultWidth );
				m_points.push_back(pt);
			}
		}

		CalcBBox();

		// Update UI.
		if (m_panel && m_panel->GetRoad() == this)
			m_panel->SetRoad(this);

		m_bNeedUpdateSectors = true;
		SetRoadSectors();
	}
	else
	{
		// Saving.
	
		// Save Points.
		if (!m_points.empty())
		{
			XmlNodeRef points = xmlNode->newChild( "Points" );
			for (int i = 0; i < m_points.size(); i++)
			{
				XmlNodeRef pnt = points->newChild( "Point" );
				pnt->setAttr( "Pos",m_points[i].pos );
				pnt->setAttr( "Back",m_points[i].back );
				pnt->setAttr( "Forw",m_points[i].forw );
				pnt->setAttr( "Angle",m_points[i].angle );
				pnt->setAttr( "Width",m_points[i].width );
				pnt->setAttr( "IsDefaultWidth",m_points[i].isDefaultWidth );
			}
		}
	}
	m_bIgnoreParamUpdate = false;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CRoadObject::Export( const CString &levelPath,XmlNodeRef &xmlNode )
{
	//XmlNodeRef objNode = __super::Export( levelPath,xmlNode );
	//return objNode;
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::CalcBBox()
{
	if (m_points.empty())
		return;

	// Reposition Road, so that Road object position is in the middle of the Road.
	Vec3 center = m_points[0].pos;
	if (center.x != 0 || center.y != 0 || center.z != 0)
	{
		// May not work correctly if Road is transformed.
		for (int i = 0; i < m_points.size(); i++)
		{
			m_points[i].pos -= center;
		}
		Matrix34 ltm = GetLocalTM();
		SetPos( GetPos() + ltm.TransformVector(center) );
	}

	for (int i = 0; i < m_points.size(); i++)
		BezierCorrection(i);

	// First point must always be 0,0,0.
	m_bbox.Reset();
	for (int i = 0; i < m_points.size(); i++)
	{
		m_bbox.Add( m_points[i].pos );
	}
	if (m_bbox.min.x > m_bbox.max.x)
	{
		m_bbox.min = m_bbox.max = Vec3(0,0,0);
	}
	BBox box;
	box.SetTransformedAABB( GetWorldTM(),m_bbox );

	//m_bbox.max.z = max( m_bbox.max.z,(float)mv_height );
	m_bbox.max.z = max( m_bbox.max.z, 0.0f);
}

//////////////////////////////////////////////////////////////////////////
int CRoadObject::InsertPoint( int index,const Vec3 &point )
{
	if (GetPointCount() >= GetMaxPoints())
		return GetPointCount()-1;
	int newindex;
	StoreUndo( "Insert Point" );

	newindex = m_points.size();


	if (index < 0 || index >= m_points.size())
	{
		CRoadPoint pt;
		pt.pos = point;
		pt.back = point;
		pt.forw = point;
		m_points.push_back( pt );
		newindex = m_points.size()-1;
	}
	else
	{
		CRoadPoint pt;
		pt.pos = point;
		pt.back = point;
		pt.forw = point;
		m_points.insert( m_points.begin()+index,pt );
		newindex = index;
	}
	SetPoint( newindex,point );
	CalcBBox();
	SetRoadSectors();
	return newindex;
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::RemovePoint( int index )
{
	if ((index >= 0 || index < m_points.size()) && m_points.size() > 2)
	{
		StoreUndo( "Remove Point" );
		m_points.erase( m_points.begin()+index );
		CalcBBox();
		SetRoadSectors();
	}
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::SelectPoint( int index )
{
	if( m_selectedPoint == index )
		return;
	m_selectedPoint = index;
	if(m_panel)
		m_panel->SelectPoint(index);
}

//////////////////////////////////////////////////////////////////////////
float CRoadObject::GetPointAngle()
{
	int index = m_selectedPoint;
	if(0<=index && index < m_points.size())
		return m_points[index].angle;
	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::SetPointAngle( float angle )
{
	int index = m_selectedPoint;
	if(0<=index && index < m_points.size())
		m_points[index].angle = angle;
	SetRoadSectors();
}


float CRoadObject::GetPointWidth( )
{
	int index = m_selectedPoint;
	if(0<=index && index < m_points.size())
		return m_points[index].width;
	return 0.0f;
}

void CRoadObject::SetPointWidth( float width )
{
	int index = m_selectedPoint;
	if(0<=index && index < m_points.size())
		m_points[index].width = width;
	SetRoadSectors();
}

bool CRoadObject::IsPointDefaultWidth( )
{
	int index = m_selectedPoint;
	if(0<=index && index < m_points.size())
		return m_points[index].isDefaultWidth;
	return true;
}

void CRoadObject::PointDafaultWidthIs( bool isDefault )
{
	int index = m_selectedPoint;
	if(0<=index && index < m_points.size())
	{
		m_points[index].isDefaultWidth = isDefault;
		m_points[index].width = mv_width;
	}
	SetRoadSectors();
}



//////////////////////////////////////////////////////////////////////////
void CRoadObject::BezierAnglesCorrection(CRoadPointVector & points, int index)
{
	int maxindex = points.size()-1;
	if(index<0 || index>maxindex)
		return;

	Vec3 & p2 = points[index].pos;
	Vec3 & back = points[index].back;
	Vec3 & forw = points[index].forw;

	if(index == 0)
	{
		back = p2;
		if(maxindex==1)
		{
			Vec3 & p3 = points[index+1].pos;
			forw = p2 + (p3-p2)/3;
		}
		else
		if(maxindex>0)
		{
			Vec3 & p3 = points[index+1].pos;
			Vec3 & pb3 = points[index+1].back;

			float lenOsn = (pb3-p2).GetLength();
			float lenb = (p3-p2).GetLength();

			forw = p2 + (pb3-p2)/(lenOsn/lenb*3);
		}
	}

	if(index == maxindex)
	{
		forw=p2;
		if(index>0)
		{
			Vec3 & p1 = points[index-1].pos;
			Vec3 & pf1 = points[index-1].forw;

			float lenOsn = (pf1-p2).GetLength();
			float lenf = (p1-p2).GetLength();

			if(lenOsn>0.000001f && lenf>0.000001f)
				back = p2 + (pf1-p2)/(lenOsn/lenf*3);
			else
				back = p2;
		}
	}

	if(1 <= index && index <= maxindex-1)
	{
		Vec3 & p1 = points[index-1].pos;
		Vec3 & p3 = points[index+1].pos;

		float lenOsn = (p3-p1).GetLength();
		float lenb = (p1-p2).GetLength();
		float lenf = (p3-p2).GetLength();

		back = p2 + (p1-p3)/(lenOsn/lenb*3);
		forw = p2 + (p3-p1)/(lenOsn/lenf*3);
	}
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::BezierCorrection(int index)
{
	BezierAnglesCorrection(m_points, index-1);
	BezierAnglesCorrection(m_points, index);
	BezierAnglesCorrection(m_points, index+1);
	BezierAnglesCorrection(m_points, index-2);
	BezierAnglesCorrection(m_points, index+2);
}


//////////////////////////////////////////////////////////////////////////
void CRoadObject::SetPoint( int index,const Vec3 &pos )
{
	Vec3 p = pos;
	if (m_bForce2D)
	{
		if (!m_points.empty())
			p.z = m_points[0].pos.z; // Keep original Z.
	}

	if (0 <= index && index < m_points.size())
	{
		m_points[index].pos = p;
		BezierCorrection(index);
		SetRoadSectors();
	}
}

//////////////////////////////////////////////////////////////////////////
int CRoadObject::GetNearestPoint( const Vec3 &raySrc,const Vec3 &rayDir,float &distance )
{
	int index = -1;
	float minDist = FLT_MAX;
	Vec3 rayLineP1 = raySrc;
	Vec3 rayLineP2 = raySrc + rayDir*RAY_DISTANCE;
	Vec3 intPnt;
	const Matrix34 &wtm = GetWorldTM();
	for (int i = 0; i < m_points.size(); i++)
	{
		float d = PointToLineDistance( rayLineP1,rayLineP2,wtm.TransformPoint(m_points[i].pos),intPnt );
		if (d < minDist)
		{
			minDist = d;
			index = i;
		}
	}
	distance = minDist;
	return index;
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::GetNearestEdge( const Vec3 &pos,int &p1,int &p2,float &distance,Vec3 &intersectPoint )
{
	p1 = -1;
	p2 = -1;
	float minDist = FLT_MAX;
	Vec3 intPnt;
	
	const Matrix34 &wtm = GetWorldTM();

	for (int i = 0; i < m_points.size(); i++)
	{
		int j = (i < m_points.size()-1) ? i+1 : 0;

		if (j == 0 && i != 0)
			continue;

		int kn=6;
		for(int k=0; k<kn; k++)
		{
			Vec3 a1 = wtm.TransformPoint(GetBezierPos(m_points, i, float(k)/kn));
			Vec3 a2 = wtm.TransformPoint(GetBezierPos(m_points, i, float(k)/kn+1.0f/kn));

			float d = PointToLineDistance( a1, a2, pos, intPnt );
			if (d < minDist)
			{
				minDist = d;
				p1 = i;
				p2 = j;
				intersectPoint = intPnt;
			}
		}
	}
	distance = minDist;
}

//////////////////////////////////////////////////////////////////////////
bool CRoadObject::RayToLineDistance( const Vec3 &rayLineP1,const Vec3 &rayLineP2,const Vec3 &pi,const Vec3 &pj,float &distance,Vec3 &intPnt )
{
	Vec3 pa,pb;
	float ua,ub;
	if (!LineLineIntersect( pi,pj, rayLineP1,rayLineP2, pa,pb,ua,ub ))
		return false;

	// If before ray origin.
	if (ub < 0)
		return false;

	float d = 0;
	if (ua < 0)
		d = PointToLineDistance( rayLineP1,rayLineP2,pi,intPnt );
	else if (ua > 1)
		d = PointToLineDistance( rayLineP1,rayLineP2,pj,intPnt );
	else
	{
		intPnt = rayLineP1 + ub*(rayLineP2-rayLineP1);
		d = (pb-pa).GetLength();
	}
	distance = d;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::GetNearestEdge( const Vec3 &raySrc,const Vec3 &rayDir,int &p1,int &p2,float &distance,Vec3 &intersectPoint )
{
	p1 = -1;
	p2 = -1;
	float minDist = FLT_MAX;
	Vec3 intPnt;
	Vec3 rayLineP1 = raySrc;
	Vec3 rayLineP2 = raySrc + rayDir*RAY_DISTANCE;

	const Matrix34 &wtm = GetWorldTM();

	for (int i = 0; i < m_points.size(); i++)
	{
		int j = (i < m_points.size()-1) ? i+1 : 0;

		if (j == 0 && i != 0)
			continue;

		int kn=6;
		for(int k=0; k<kn; k++)
		{
			Vec3 pi = wtm.TransformPoint(GetBezierPos(m_points, i, float(k)/kn));
			Vec3 pj = wtm.TransformPoint(GetBezierPos(m_points, i, float(k)/kn+1.0f/kn));

			float d = 0;
			if (!RayToLineDistance( rayLineP1,rayLineP2,pi,pj,d,intPnt ))
				continue;
			
			if (d < minDist)
			{
				minDist = d;
				p1 = i;
				p2 = j;
				intersectPoint = intPnt;
			}
		}
	}
	distance = minDist;
}


//////////////////////////////////////////////////////////////////////////
void CRoadObject::OnParamChange( IVariable *var )
{
	if (!m_bIgnoreParamUpdate)
		SetRoadSectors();
}

//////////////////////////////////////////////////////////////////////////
bool CRoadObject::HitTestRect( HitContext &hc )
{
	BBox box;
	// Retrieve world space bound box.
	GetBoundBox( box );

	// Project all edges to viewport.
	const Matrix34 &wtm = GetWorldTM();
	for (int i = 0; i < m_points.size(); i++)
	{
		int j = (i < m_points.size()-1) ? i+1 : 0;
		if (j == 0 && i != 0)
			continue;

		Vec3 p0 = wtm.TransformPoint(m_points[i].pos);
		Vec3 p1 = wtm.TransformPoint(m_points[j].pos);

		CPoint pnt0 = hc.view->WorldToView( p0 );
		CPoint pnt1 = hc.view->WorldToView( p1 );

		// See if pnt0 to pnt1 line intersects with rectangle.
		// check see if one point is inside rect and other outside, or both inside.
		bool in0 = hc.rect.PtInRect(pnt0);
		bool in1 = hc.rect.PtInRect(pnt0);
		if ((in0 && in1) || (in0 && !in1) || (!in0 && in1))
		{
			hc.object = this;
			return true;
		}
	}

	return false;
}

void CRoadObject::SetSelected( bool bSelect )
{
	__super::SetSelected(bSelect);
	SetRoadSectors();
}


//////////////////////////////////////////////////////////////////////////
void CRoadObject::AlignHeightMap()
{

	if (!GetIEditor()->IsUndoRecording())
		GetIEditor()->BeginUndo();

	CHeightmap *heightmap = GetIEditor()->GetHeightmap();
	int unitSize = heightmap->GetUnitSize();
	const Matrix34 &wtm = GetWorldTM();

	//float width = mv_width / unitSize;
	//if(width<2)
		//width = 2;

	int minx=0, miny=0, maxx=0, maxy=0;
	bool bIsInitMinMax = false;



	for (int i = 0; i < m_points.size()-1; i++)
	{
		float fminx=0, fminy=0, fmaxx=0, fmaxy=0;
		bool bIsInitminmax = false;
		int kn = int (0.5f + (GetBezierSegmentLength(i)+1)*4);

		Vec3 tmp;

		for(int k=0; k<=kn; k++)
		{
			float t = float(k)/kn;
			Vec3 p = GetBezierPos(m_points, i, t);
			Vec3 n = GetLocalBezierNormal(i, t);

			float fWidth = GetLocalWidth(i, t);
			if(fWidth<2)
				fWidth = 2;

			Vec3 p1 = p + (0.5f*fWidth + mv_borderWidth + 0.5f*unitSize)*n;
			Vec3 p2 = p - (0.5f*fWidth + mv_borderWidth + 0.5f*unitSize)*n;

			p1 = wtm.TransformPoint(p1);
			p2 = wtm.TransformPoint(p2);

			tmp = p1;

			if(!bIsInitminmax)
			{
				fminx = p1.x;
				fminy = p1.y;
				fmaxx = p1.x;
				fmaxy = p1.y;
				bIsInitminmax = true;
			}
			fminx = min(fminx, p1.x);
			fminx = min(fminx, p2.x);
			fminy = min(fminy, p1.y);
			fminy = min(fminy, p2.y);
			fmaxx = max(fmaxx, p1.x);
			fmaxx = max(fmaxx, p2.x);
			fmaxy = max(fmaxy, p1.y);
			fmaxy = max(fmaxy, p2.y);
		}

		//for(int x = int(fminx); x <= int(fmaxx)+1; x++)
			//for(int y = int(fminy); y <= int(fmaxy)+1; y++)

		heightmap->RecordUndo( int(fminy/unitSize)-1,int(fminx/unitSize)-1,int(fmaxy/unitSize)+unitSize-int(fminy/unitSize)+1,int(fmaxx/unitSize)+unitSize-int(fminx/unitSize)+1);

		for(int ty = int(fminx/unitSize); ty <= int(fmaxx/unitSize)+unitSize; ty++)
			for(int tx = int(fminy/unitSize); tx <= int(fmaxy/unitSize)+unitSize; tx++)
			{
				int x = ty*unitSize;
				int y = tx*unitSize;
				
				Vec3 p3 = Vec3(x, y, 0.0f);

				int findk = -1;
				//float mind = 0.5f*mv_width + mv_borderWidth+0.5f*unitSize;
				float fWidth = GetLocalWidth(i, 0);
				float fWidth1 = GetLocalWidth(i, 1);
				if(fWidth<fWidth1)
					fWidth = fWidth1;
				float mind = 0.5f*fWidth + mv_borderWidth+0.5f*unitSize;

				for(int k=0; k<kn; k++)
				{
					float t = float(k)/kn;
					Vec3 p1 = wtm.TransformPoint(GetBezierPos(m_points, i, t));
					Vec3 p2 = wtm.TransformPoint(GetBezierPos(m_points, i, t+1.0f/kn));
					p1.z = 0.0f;
					p2.z = 0.0f;

					

					//Vec3 d = p2 - p1;
					//float u = d.Dot(p3-p1) / (d).GetLengthSquared();
					//if (-0.1f <= u && u <=1.1f)
					//{
						float d = PointToLineDistance (p1, p2, p3);
						if(d < mind)
						{
							findk = k;
							mind = d;
						}
					//}
				}
				
				if(findk!=-1)
				{
					float t = float(findk)/kn;

					float st = 1.0f/kn;
					int sign = 1;
					for(int tt=0; tt<24; tt++)
					{
						Vec3 p0 = wtm.TransformPoint(GetBezierPos(m_points, i, t));;
						p0.z = 0.0f;
						
						Vec3 ploc = GetBezierPos(m_points, i, t);
						Vec3 nloc = GetLocalBezierNormal(i, t);

						Vec3 p1 = wtm.TransformPoint(ploc + nloc);
						p1.z=0.0f;

						if(((p0-p1).Cross(p3-p1).z)<0.0f)
							sign = 1;
						else
							sign = -1;
						
						st/=1.618f;
						t = t+sign*st;
					}

					if(t<0.0f || t>1.0f)
						continue;

					
					Vec3 p1 = wtm.TransformPoint(GetBezierPos(m_points, i, t));
					Vec3 p2 = wtm.TransformPoint(GetBezierPos(m_points, i, t+1.0f/kn));

					Vec3 p1_0 = p1;
					p1_0.z = 0.0f;
					p2.z = 0.0f;
					Vec3 p = Vec3(x,y, 0.0f);
					Vec3 e = (p2-p1_0).Cross(p1_0-p);

					Vec3 p1loc = GetBezierPos(m_points, i, t);
					Vec3 nloc = GetLocalBezierNormal(i, t);

					Vec3 n = wtm.TransformPoint(p1loc + nloc) - p1;
					n.Normalize();

					Vec3 nproj = n;
					nproj.z = 0.0f;

					float kof = nproj.GetLength();

					float length = (p1_0-p).GetLength()/kof;

					Vec3 pos;
					
					if(e.z<0.0f)
						pos = p1-length*n;
					else
						pos = p1+length*n;

					float fWidth = GetLocalWidth(i, t);
					if(length <= fWidth/2+mv_borderWidth+0.5*unitSize)
					{
						//int tx = RoundFloatToInt(y / unitSize);
						//int ty = RoundFloatToInt(x / unitSize);

						float z;
						if(length <= fWidth/2+0.5*unitSize)
							z = pos.z;
						else
						{
							//continue;
							float kof = (length - (fWidth/2+0.5*unitSize))/mv_borderWidth;
							kof = 1.0f-(cos(kof*3.141593f)+1.0f)/2;
							float z1 = heightmap->GetXY(tx, ty);
							z = kof * z1 + (1.0f-kof)*pos.z;
						}
						
						heightmap->SetXY(tx, ty, z);

						if(!bIsInitMinMax)
						{
							minx = tx;
							miny = ty;
							maxx = tx;
							maxy = ty;
							bIsInitMinMax = true;
						}
						if(minx > tx) minx = tx;
						if(miny > ty) miny = ty;
						if(maxx < tx) maxx = tx;
						if(maxy < ty) maxy = ty;
					}
				}
			}
		//break;
	}

	/*
	for (int i = 0; i < m_points.size()-1; i++)
	{
		int kn = int (0.5f + (GetBezierSegmentLength(i)+1)/unitSize*4)*2;
		for(int k=0; k<kn; k++)
		{
			Vec3 p = wtm.TransformPoint(GetBezierPos(m_points, i, float(k)/kn));
			Vec3 n = GetBezierNormal(i, float(k)/kn);
			int kl = width*4/2;
			for(int y = -kl; y<=kl; y++)
			{
				float f = (0.5f*(width*2+0.5f))*float(y)/kl;
				Vec3 pos = p+f*n;
				int tx = RoundFloatToInt(pos.y / unitSize);
				int ty = RoundFloatToInt(pos.x / unitSize);
				if(tx<0) tx = 0;
				if(ty<0) ty = 0;
				if(tx>heightmap->GetWidth()-1) tx = heightmap->GetWidth()-1;
				if(ty>heightmap->GetHeight()-1) ty = heightmap->GetHeight()-1;
				heightmap->SetXY(tx, ty, pos.z);
				if(!bIsInitMinMax)
				{
					minx = tx;
					miny = ty;
					maxx = tx;
					maxy = ty;
					bIsInitMinMax = true;
				}
				if(minx > tx) minx = tx;
				if(miny > ty) miny = ty;
				if(maxx < tx) maxx = tx;
				if(maxy < ty) maxy = ty;
			}
		}
	}
	*/

	int w = maxx-minx;
	if(w<maxy-miny) w = maxy-miny;
	heightmap->UpdateEngineTerrain( minx,miny,w,w,true,false );


	if (GetIEditor()->IsUndoRecording())
		GetIEditor()->AcceptUndo( "Heightmap Aligning" );

	SetRoadSectors();
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::SetMinSpec( uint32 nSpec )
{
	__super::SetMinSpec(nSpec);
	UpdateSectors();
}

//////////////////////////////////////////////////////////////////////////
void CRoadObject::SetMaterialLayersMask( uint32 nLayersMask )
{
	__super::SetMaterialLayersMask(nLayersMask);
	UpdateSectors();
}
