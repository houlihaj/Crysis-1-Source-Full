////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   ShapeObject.cpp
//  Version:     v1.00
//  Created:     10/10/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: CShapeObject implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ShapeObject.h"
#include "..\ShapePanel.h"
#include "..\Viewport.h"
#include "Util\Triangulate.h"
#include "AI\AIManager.h"

#include <I3DEngine.h>
#include <IAISystem.h>

#include <vector>
#include "IEntitySystem.h"

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CShapeObject,CEntity)
IMPLEMENT_DYNCREATE(CAIForbiddenAreaObject,CShapeObject)
IMPLEMENT_DYNCREATE(CAIForbiddenBoundaryObject,CShapeObject)
IMPLEMENT_DYNCREATE(CAIPathObject,CShapeObject)
IMPLEMENT_DYNCREATE(CAIShapeObject,CShapeObject)
IMPLEMENT_DYNCREATE(CAINavigationModifierObject,CShapeObject)
IMPLEMENT_DYNCREATE(CAIOcclusionPlaneObject,CShapeObject)

#define RAY_DISTANCE 100000.0f

//////////////////////////////////////////////////////////////////////////
int CShapeObject::m_rollupId = 0;
CShapePanel* CShapeObject::m_panel = 0;
int CShapeObject::m_rollupMultyId = 0;
CShapeMultySelPanel* CShapeObject::m_panelMulty = 0;
CAxisHelper CShapeObject::m_selectedPointAxis;

//////////////////////////////////////////////////////////////////////////
CShapeObject::CShapeObject()
{
	m_useAxisHelper = false;
	m_bForce2D = false;
	mv_closed = true;

	mv_areaId = 0;
	
	mv_groupId = 0;
	mv_priority = 0;
	mv_width = 0;
	mv_height = 0;
	mv_obstructRoof = false;
	mv_obstructFloor = false;
	mv_displayFilled = false;
	
	m_bbox.min = m_bbox.max = Vec3(0,0,0);
	m_selectedPoint = -1;
	m_lowestHeight = 0;
	m_bIgnoreGameUpdate = true;
	m_bAreaModified = true;
	m_bDisplayFilledWhenSelected = true;
	m_entityClass = "AreaShape";
	m_bPerVertexHeight = false;

	m_numSplitPoints = 0;
	m_mergeIndex = -1;

	m_updateSucceed = true;

	SetColor( Vec2Rgb(Vec3(0,0.8f,1)) );
	UseMaterialLayersMask(false);
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::Done()
{
	m_entities.Clear();
	m_points.clear();
	UpdateGameArea(true);
	__super::Done();
}

//////////////////////////////////////////////////////////////////////////
bool CShapeObject::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	m_bIgnoreGameUpdate = true;

	bool res = __super::Init( ie,prev,file );

	if (prev)
	{
		m_points = ((CShapeObject*)prev)->m_points;
		m_bIgnoreGameUpdate = false;
		mv_closed = ((CShapeObject*)prev)->mv_closed;
		CalcBBox();
	}

	m_bIgnoreGameUpdate = false;

	return res;
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::InitVariables()
{
	AddVariable( mv_width,"Width",functor(*this,&CShapeObject::OnShapeChange) );
	AddVariable( mv_height,"Height",functor(*this,&CShapeObject::OnShapeChange) );
	AddVariable( mv_areaId,"AreaId",functor(*this,&CShapeObject::OnShapeChange) );
	AddVariable( mv_groupId,"GroupId",functor(*this,&CShapeObject::OnShapeChange) );
	AddVariable( mv_priority,"Priority",functor(*this,&CShapeObject::OnShapeChange) );
	AddVariable( mv_closed,"Closed",functor(*this,&CShapeObject::OnShapeChange) );
	AddVariable( mv_obstructRoof,"ObstructRoof",functor(*this,&CShapeObject::OnShapeChange) );
	AddVariable( mv_obstructFloor,"ObstructFloor",functor(*this,&CShapeObject::OnShapeChange) );
	AddVariable( mv_displayFilled,"DisplayFilled" );
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::SetName( const CString &name )
{
	__super::SetName( name );
	m_bAreaModified = true;

	if (!IsOnlyUpdateOnUnselect() && !m_bIgnoreGameUpdate)
		UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::InvalidateTM( int nWhyFlags )
{
	__super::InvalidateTM(nWhyFlags);
	m_bAreaModified = true;
//	CalcBBox();

	if (nWhyFlags & TM_RESTORE_UNDO) // Can skip updating game object when restoring undo.
		return;

	if (!IsOnlyUpdateOnUnselect() && !m_bIgnoreGameUpdate)
		UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::GetBoundBox( BBox &box )
{
	box.SetTransformedAABB( GetWorldTM(),m_bbox );
	float s = 1.0f;
	box.min -= Vec3(s,s,s);
	box.max += Vec3(s,s,s);
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::GetLocalBounds( BBox &box )
{
	box = m_bbox;
}

//////////////////////////////////////////////////////////////////////////
bool CShapeObject::HitTest( HitContext &hc )
{
	// First check if ray intersect our bounding box.
	float tr = hc.distanceTollerance/2 + SHAPE_CLOSE_DISTANCE;

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

		if (!mv_closed && j == 0 && i != 0)
			continue;

		Vec3 pi = wtm.TransformPoint(m_points[i]);
		Vec3 pj = wtm.TransformPoint(m_points[j]);

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

		if (mv_height > 0)
		{
			if (RayToLineDistance( rayLineP1,rayLineP2,pi+Vec3(0,0,mv_height),pj+Vec3(0,0,mv_height),d,intPnt ))
			{
				if (d < minDist)
				{
					bWasIntersection = true;
					minDist = d;
					intPnt = ip;
				}
			}
			if (RayToLineDistance( rayLineP1,rayLineP2,pi,pi+Vec3(0,0,mv_height),d,intPnt ))
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

	float fShapeCloseDistance = SHAPE_CLOSE_DISTANCE*hc.view->GetScreenScaleFactor(intPnt) * 0.01f;

	if (bWasIntersection && minDist < fShapeCloseDistance+hc.distanceTollerance)
	{
		hc.dist = hc.raySrc.GetDistance(intPnt);
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::BeginEditParams( IEditor *ie,int flags )
{
	CBaseObject::BeginEditParams( ie,flags );

	if (!m_panel)
	{
		m_panel = new CShapePanel;
		m_panel->Create( CShapePanel::IDD );
		m_rollupId = ie->AddRollUpPage( ROLLUP_OBJECTS,"Shape Parameters",m_panel );
	}
	if (m_panel)
		m_panel->SetShape( this );
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::EndEditParams( IEditor *ie )
{
	if (m_rollupId != 0)
	{
		ie->RemoveRollUpPage( ROLLUP_OBJECTS,m_rollupId );
		CalcBBox();
	}
	m_rollupId = 0;
	m_panel = 0;

	__super::EndEditParams( ie );
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::BeginEditMultiSelParams( bool bAllOfSameType )
{
	__super::BeginEditMultiSelParams( bAllOfSameType );
	if(!bAllOfSameType)
		return;

	if (!m_panelMulty)
	{
		m_panelMulty = new CShapeMultySelPanel;
		m_rollupMultyId = GetIEditor()->AddRollUpPage( ROLLUP_OBJECTS,"Multy Shape Operation", m_panelMulty );
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::EndEditMultiSelParams()
{
	if (m_rollupMultyId != 0)
	{
		GetIEditor()->RemoveRollUpPage( ROLLUP_OBJECTS,m_rollupMultyId );
		CalcBBox();
	}
	m_rollupMultyId = 0;
	m_panelMulty = 0;

	__super::EndEditMultiSelParams();
}


//////////////////////////////////////////////////////////////////////////
int CShapeObject::MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	if (event == eMouseMove || event == eMouseLDown || event == eMouseLDblClick)
	{
		Vec3 pos = view->MapViewToCP(point);

		bool firstTime = false;
		if (m_points.size() < 2)
		{
			SetPos( pos );
		}

		pos.z += GetShapeZOffset();
		
		if (m_points.size() == 0)
		{
			InsertPoint( -1,Vec3(0,0,0) );
			firstTime = true;
		}
		else
		{
			SetPoint( m_points.size()-1,pos - GetWorldPos() );
		}

		if (event == eMouseLDblClick)
		{
			if (m_points.size() > GetMinPoints())
			{
				m_points.pop_back(); // Remove last unneeded point.
				EndCreation();
				return MOUSECREATE_OK;
			}
			else
				return MOUSECREATE_ABORT;
		}

		if (event == eMouseLDown)
		{
			Vec3 vlen = Vec3(pos.x,pos.y,0) - Vec3(GetWorldPos().x,GetWorldPos().y,0);
			/* Disable that for now.
			if (m_points.size() > 2 && vlen.GetLength() < SHAPE_CLOSE_DISTANCE)
			{
				EndCreation();
				return MOUSECREATE_OK;
			}
			*/
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
void CShapeObject::EndCreation()
{
	SetClosed(mv_closed);
	if (m_panel)
		m_panel->SetShape(this);
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::Display( DisplayContext &dc )
{
	//dc.renderer->EnableDepthTest(false);
	
	const Matrix34 &wtm = GetWorldTM();
	COLORREF col = 0;

	float fPointSize = 0.5f;
	if(!IsSelected())
		fPointSize *= 0.5f;

	bool bPointSelected = false;
	if (m_selectedPoint >= 0 && m_selectedPoint < m_points.size())
	{
		bPointSelected = true;
	}

	bool bLineWidth = 0;

	if (!m_updateSucceed)
	{
		// Draw Error in update
		dc.SetColor( GetColor() );
		dc.DrawTextLabel(wtm.GetTranslation(), 2.0f, "Error!", true);
		CString msg("\n\n");
		msg += GetName();
		msg += " (see log)";
		dc.DrawTextLabel(wtm.GetTranslation(), 1.2f, msg, true);
	}

	if (m_points.size() > 1)
	{
		if (IsSelected())
		{
			col = dc.GetSelectedColor();
			dc.SetColor( col );
		}
		else if (IsHighlighted() && !bPointSelected)
		{
			dc.SetColor( RGB(255,120,0) );
			dc.SetLineWidth(3);
			bLineWidth = true;
		}
		else
		{
			if (IsFrozen())
				dc.SetFreezeColor();
			else
				dc.SetColor( GetColor() );
			col = GetColor();
		}
		dc.SetAlpha( 0.8f );

		int num = m_points.size();
		if (num < GetMinPoints())
			num = 1;
		for (int i = 0; i < num; i++)
		{
			int j = (i < m_points.size()-1) ? i+1 : 0;
			if (!mv_closed && j == 0 && i != 0)
				continue;
			
			Vec3 p0 = wtm.TransformPoint(m_points[i]);
			Vec3 p1 = wtm.TransformPoint(m_points[j]);

			ColorB colMergedTmp = dc.GetColor();
			if(m_mergeIndex==i)
				dc.SetColor( RGB(255,255,127) );
			dc.DrawLine( p0,p1 );
			dc.SetColor( colMergedTmp );
			//DrawTerrainLine( dc,pos+m_points[i],pos+m_points[j] );
			
			if (mv_height != 0)
			{
				BBox box;
				box.SetTransformedAABB( GetWorldTM(),m_bbox );
				m_lowestHeight = box.min.z;
				// Draw second capping shape from above.
				float z0 = 0;
				float z1 = 0;
				if (m_bPerVertexHeight)
				{
					z0 = p0.z + mv_height;
					z1 = p1.z + mv_height;
				}
				else
				{
					z0 = m_lowestHeight + mv_height;
					z1 = z0;
				}
				dc.DrawLine( p0,Vec3(p0.x,p0.y,z0) );
				dc.DrawLine( Vec3(p0.x,p0.y,z0),Vec3(p1.x,p1.y,z1) );

				if (mv_displayFilled || (gSettings.viewports.bFillSelectedShapes && IsSelected()) )
				{
					dc.CullOff();
					ColorB c = dc.GetColor();
					dc.SetAlpha( 0.3f );
					dc.DrawQuad( p0,Vec3(p0.x,p0.y,z0),Vec3(p1.x,p1.y,z1),p1 );
					dc.CullOn();
					dc.SetColor(c);
					if(!IsHighlighted())
						dc.SetAlpha( 0.8f );
				}
			}
		}

		// Draw selected point.
		if (bPointSelected && m_useAxisHelper)
		{
			// The axis is drawn before the points because the points have higher pick priority and should appear on top of the axis.
			Matrix34	axis;
			axis.SetTranslationMat(wtm.TransformPoint(m_points[m_selectedPoint]));
			m_selectedPointAxis.SetMode(CAxisHelper::MOVE_MODE);
			m_selectedPointAxis.DrawAxis(axis, dc);
		}

		// Draw points without depth test to make them editable even behind other objects.
		if(IsSelected())
			dc.DepthTestOff();

		if (IsFrozen())
			col = dc.GetFreezeColor();
		else
			col = GetColor();

		for (int i = 0; i < num; i++)
		{
			Vec3 p0 = wtm.TransformPoint(m_points[i]);
			float fScale = fPointSize*dc.view->GetScreenScaleFactor(p0) * 0.01f;
			Vec3 sz(fScale,fScale,fScale);

			if(bPointSelected && i == m_selectedPoint)
				dc.SetSelectedColor();
			else
				dc.SetColor(col);

			dc.DrawWireBox( p0-sz,p0+sz );
		}

		if(IsSelected())
			dc.DepthTestOn();
	}

	if (!m_entities.IsEmpty())
	{
		Vec3 vcol = Rgb2Vec(col);
		int num = m_entities.GetCount();
		for (int i = 0; i < num; i++)
		{
			CBaseObject *obj = m_entities.Get(i);
			if (!obj)
				continue;
			int p1,p2;
			float dist;
			Vec3 intPnt;
			GetNearestEdge( obj->GetWorldPos(),p1,p2,dist,intPnt );
			dc.DrawLine( intPnt,obj->GetWorldPos(),ColorF(vcol.x,vcol.y,vcol.z,0.7f),ColorF(1,1,1,0.7f) );
		}
	}

	if (mv_closed && !IsFrozen())
	{
		//if (mv_displayFilled) // || (IsSelected() && m_bDisplayFilledWhenSelected) || (IsHighlighted() && m_bDisplayFilledWhenSelected))
		if (mv_displayFilled || (gSettings.viewports.bFillSelectedShapes && IsSelected()))
		{
			if (IsHighlighted())
				dc.SetColor( GetColor(),0.1f );
			else
				dc.SetColor( GetColor(),0.3f );
			static std::vector<Vec3> tris;
			tris.resize(0);
			tris.reserve( m_points.size()*3 );
			if (CTriangulate::Process( m_points,tris ))
			{
				for (int i = 0; i < tris.size(); i += 3)
				{
					dc.DrawTri( wtm.TransformPoint(tris[i]),wtm.TransformPoint(tris[i+1]),wtm.TransformPoint(tris[i+2]) );
				}
			}
		}
	}

	if(m_numSplitPoints>0)
	{
		COLORREF col = GetColor();
		dc.SetColor( RGB(127,255,127) );
		for(int i = 0; i<m_numSplitPoints; i++)
		{
			Vec3 p0 = wtm.TransformPoint(m_splitPoints[i]);
			float fScale = fPointSize*dc.view->GetScreenScaleFactor(p0) * 0.01f;
			Vec3 sz(fScale,fScale,fScale);
			dc.DrawWireBox( p0-sz,p0+sz );
		}
		if(m_numSplitPoints==2)
		{
			Vec3 p0 = wtm.TransformPoint(m_splitPoints[0]);
			Vec3 p1 = wtm.TransformPoint(m_splitPoints[1]);
			dc.DrawLine( p0, p1);
		}
		dc.SetColor( col );
	}

	if (bLineWidth)
		dc.SetLineWidth(0);
	
	//dc.renderer->EnableDepthTest(true);

	DrawDefault(dc,GetColor());
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::DrawTerrainLine( DisplayContext &dc,const Vec3 &p1,const Vec3 &p2 )
{
	float len = (p2-p1).GetLength();
	int steps = len/2;
	if (steps <= 1)
	{
		dc.DrawLine( p1,p2 );
		return;
	}
	Vec3 pos1 = p1;
	Vec3 pos2 = p1;
	for (int i = 0; i < steps-1; i++)
	{
		pos2 = p1 + (1.0f/i)*(p2-p1);
		pos2.z = dc.engine->GetTerrainElevation(pos2.x,pos2.y);
		dc.SetColor( i*2,0,0,1 );
		dc.DrawLine( pos1,pos2 );
		pos1 = pos2;
	}
	//dc.DrawLine( pos2,p2 );
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::Serialize( CObjectArchive &ar )
{
	m_bIgnoreGameUpdate = true;
	__super::Serialize( ar );
	m_bIgnoreGameUpdate = false;

	XmlNodeRef xmlNode = ar.node;
	if (ar.bLoading)
	{
		m_bAreaModified = true;
		m_bIgnoreGameUpdate = true;
		m_entities.Clear();

		GUID entityId;
		if (xmlNode->getAttr( "EntityId",entityId ))
		{
			// For Backward compatability.
			//m_entities.push_back(entityId);
			ar.SetResolveCallback( this,entityId,functor(m_entities,&CSafeObjectsArray::Add) );
		}

		// Load Entities.
		m_points.clear();
		XmlNodeRef points = xmlNode->findChild( "Points" );
		if (points)
		{
			for (int i = 0; i < points->getChildCount(); i++)
			{
				XmlNodeRef pnt = points->getChild(i);
				Vec3 pos;
				pnt->getAttr( "Pos",pos );
				m_points.push_back(pos);
			}
		}
		XmlNodeRef entities = xmlNode->findChild( "Entities" );
		if (entities)
		{
			for (int i = 0; i < entities->getChildCount(); i++)
			{
				XmlNodeRef ent = entities->getChild(i);
				GUID entityId;
				if (ent->getAttr( "Id",entityId ))
				{
					//m_entities.push_back(id);
					ar.SetResolveCallback( this,entityId,functor(m_entities,&CSafeObjectsArray::Add) );
				}
			}
		}

		if (ar.bUndo)
		{
			// Update game area only in undo mode.
			m_bIgnoreGameUpdate = false;
		}
		CalcBBox();
		UpdateGameArea();

		// Update UI.
		if (m_panel && m_panel->GetShape() == this)
			m_panel->SetShape(this);
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
				pnt->setAttr( "Pos",m_points[i] );
			}
		}
		// Save Entities.
		if (!m_entities.IsEmpty())
		{
			XmlNodeRef nodes = xmlNode->newChild( "Entities" );
			for (int i = 0; i < m_entities.GetCount(); i++)
			{
				XmlNodeRef entNode = nodes->newChild( "Entity" );
				if (m_entities.Get(i))
					entNode->setAttr( "Id",m_entities.Get(i)->GetId() );
			}
		}
	}
	m_bIgnoreGameUpdate = false;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CShapeObject::Export( const CString &levelPath,XmlNodeRef &xmlNode )
{
	XmlNodeRef objNode = __super::Export( levelPath,xmlNode );
	return objNode;
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::CalcBBox()
{
	if (m_points.empty())
		return;

	// Reposition shape, so that shape object position is in the middle of the shape.
	Vec3 center = m_points[0];
	if (center.x != 0 || center.y != 0 || center.z != 0)
	{
		// May not work correctly if shape is transformed.
		for (int i = 0; i < m_points.size(); i++)
		{
			m_points[i] -= center;
		}
		Matrix34 ltm = GetLocalTM();
		SetPos( GetPos() + ltm.TransformVector(center) );
	}

	// First point must always be 0,0,0.
	m_bbox.Reset();
	for (int i = 0; i < m_points.size(); i++)
	{
		m_bbox.Add( m_points[i] );
	}
	if (m_bbox.min.x > m_bbox.max.x)
	{
		m_bbox.min = m_bbox.max = Vec3(0,0,0);
	}
	BBox box;
	box.SetTransformedAABB( GetWorldTM(),m_bbox );
	m_lowestHeight = box.min.z;

	if (m_bPerVertexHeight)
		m_bbox.max.z += mv_height;
	else
		m_bbox.max.z = max( m_bbox.max.z,(float)(m_lowestHeight+mv_height) );
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::SetSelected( bool bSelect )
{
	bool bWasSelected = IsSelected();
	__super::SetSelected( bSelect );
	if (!bSelect && bWasSelected)
	{
		// When unselecting shape, update area in game.
		if (m_bAreaModified)
			UpdateGameArea();
		m_bAreaModified = false;
	}
	m_mergeIndex = -1;
}

//////////////////////////////////////////////////////////////////////////
int CShapeObject::InsertPoint( int index,const Vec3 &point )
{
	if (GetPointCount() >= GetMaxPoints())
		return GetPointCount()-1;
	int newindex;
	StoreUndo( "Insert Point" );

	m_bAreaModified = true;

	if (index < 0 || index >= m_points.size())
	{
		m_points.push_back( point );
		newindex = m_points.size()-1;
	}
	else
	{
		m_points.insert( m_points.begin()+index,point );
		newindex = index;
	}
	SetPoint( newindex,point );
	CalcBBox();
	return newindex;
}

//////////////////////////////////////////////////////////////////////////
int CShapeObject::SetSplitPoint(int index, const Vec3 &point, int numPoint)
{
	if(numPoint>1)
		return -1;
	if(numPoint<0)
	{
		m_numSplitPoints = 0;
		return -1;
	}

	if(numPoint==1 && index==m_splitIndicies[0])
		return 1;

	if(index!=-2)// if index==-2 need change only m_numSplitPoints
	{
		m_splitIndicies[numPoint] = index;
		m_splitPoints[numPoint] = point;
	}
	m_numSplitPoints = numPoint+1;
	return m_numSplitPoints;
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::Split()
{
	if(m_numSplitPoints!=2)
		return;

	int in0 = InsertPoint(m_splitIndicies[0], m_splitPoints[0]);
	if(m_splitIndicies[0]!=-1 && m_splitIndicies[1]!=-1 && m_splitIndicies[1]>m_splitIndicies[0])
		m_splitIndicies[1]++;
	int in1 = InsertPoint(m_splitIndicies[1], m_splitPoints[1]);

	if(in1<=in0)
	{
		in0++;
		int vs=in1;
		in1 = in0;
		in0 = vs;
	}

	CShapeObject * pNewShape = (CShapeObject *)GetObjectManager()->CloneObject(this);
	int i;
	for(i = in0+1; i<in1; i++)
		RemovePoint(in0+1);
	if(pNewShape)
	{
		int cnt = pNewShape->GetPointCount();
		for(i = in1+1; i<cnt; i++)
			pNewShape->RemovePoint(in1+1);
		for(i = 0; i<in0; i++)
			pNewShape->RemovePoint(0);
	}

	m_bAreaModified = true;
	if (!IsOnlyUpdateOnUnselect())
		UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::SetMergeIndex( int index )
{
	m_mergeIndex = index;
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::Merge( CShapeObject * shape )
{
	if(!shape || m_mergeIndex<0 ||  shape->m_mergeIndex<0)
		return;

	const Matrix34 &tm = GetWorldTM();
	const Matrix34 &shapeTM = shape->GetWorldTM();

	int index = m_mergeIndex+1;

	Vec3 p0 = tm.TransformPoint(GetPoint(m_mergeIndex));
	Vec3 p1 = tm.TransformPoint(GetPoint(m_mergeIndex+1<GetPointCount()? m_mergeIndex+1 : 0));

	Vec3 p2 = shapeTM.TransformPoint(shape->GetPoint(shape->m_mergeIndex));
	Vec3 p3 = shapeTM.TransformPoint(shape->GetPoint(shape->m_mergeIndex+1<shape->GetPointCount()? shape->m_mergeIndex+1 : 0));

	float sum1 = p0.GetDistance(p2) + p1.GetDistance(p3);
	float sum2 = p0.GetDistance(p3) + p1.GetDistance(p2);

	if(sum2<sum1)
	{
		shape->ReverseShape();
		shape->m_mergeIndex = shape->GetPointCount()-1-shape->m_mergeIndex;
	}

	int i;
	for(i=shape->m_mergeIndex+1; i<shape->GetPointCount(); i++)
	{
		Vec3 pnt = shapeTM.TransformPoint(shape->GetPoint(i));
		pnt = tm.GetInverted().TransformPoint(pnt);
		InsertPoint(index , pnt);
	}
	for(i=0; i<=shape->m_mergeIndex; i++)
	{
		Vec3 pnt = shapeTM.TransformPoint(shape->GetPoint(i));
		pnt = tm.GetInverted().TransformPoint(pnt);
		InsertPoint(index , pnt);
	}

	shape->SetMergeIndex(-1);
	GetObjectManager()->DeleteObject(shape);

	m_bAreaModified = true;
	if (!IsOnlyUpdateOnUnselect())
		UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::RemovePoint( int index )
{
	if ((index >= 0 || index < m_points.size()) && m_points.size() > GetMinPoints())
	{
		m_bAreaModified = true;
		StoreUndo( "Remove Point" );
		m_points.erase( m_points.begin()+index );
		CalcBBox();

		m_bAreaModified = true;
		if (!IsOnlyUpdateOnUnselect())
			UpdateGameArea();
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::PostClone( CBaseObject *pFromObject,CObjectCloneContext &ctx )
{
	__super::PostClone( pFromObject,ctx );

	CShapeObject *pFromShape = (CShapeObject*)pFromObject;
	// Clone event targets.
	if (!pFromShape->m_entities.IsEmpty())
	{
		int numEntities = pFromShape->m_entities.GetCount();
		for (int i = 0; i < numEntities; i++)
		{
			CBaseObject *pTarget = pFromShape->m_entities.Get(i);
			CBaseObject *pClonedTarget = ctx.FindClone( pTarget );
			if (!pClonedTarget)
				pClonedTarget = pTarget; // If target not cloned, link to original target.

			// Add cloned event.
			if (pClonedTarget)
				AddEntity( pClonedTarget );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::AddEntity( CBaseObject *entity )
{
	assert( entity );

	m_bAreaModified = true;

	StoreUndo( "Add Entity" );
	m_entities.Add( entity );
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::RemoveEntity( int index )
{
	assert( index >= 0 && index < m_entities.GetCount() );
	StoreUndo( "Remove Entity" );

	m_bAreaModified = true;

	if (index < m_entities.GetCount())
		m_entities.Remove( m_entities.Get(index) );
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CShapeObject::GetEntity( int index )
{
	assert( index >= 0 && index < m_entities.GetCount() );
	return m_entities.Get(index);
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::SetClosed( bool bClosed )
{
	StoreUndo( "Set Closed" );
	mv_closed = bClosed;

	m_bAreaModified = true;

	if (mv_closed)
	{
		UpdateGameArea();
	}
}

//////////////////////////////////////////////////////////////////////////
int CShapeObject::GetAreaId()
{
	return mv_areaId;
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::SelectPoint( int index )
{
	m_selectedPoint = index;
}

void CShapeObject::SetPoint( int index,const Vec3 &pos )
{
	Vec3 p = pos;
	if (m_bForce2D)
	{
		if (!m_points.empty())
			p.z = m_points[0].z; // Keep original Z.
	}
	if (index >= 0 && index < m_points.size())
	{
		m_points[index] = p;
	}
	m_bAreaModified = true;
	if (!IsOnlyUpdateOnUnselect())
		UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::ReverseShape()
{
	StoreUndo( "Reverse Shape" );
	std::reverse(m_points.begin(), m_points.end());
	if(mv_closed)
	{
		// Keep the same starting point for closed paths.
		Vec3	tmp(m_points.back());
		for(size_t i = m_points.size() - 1; i > 0; --i)
			m_points[i] = m_points[i - 1];
		m_points[0] = tmp;
	}
	m_bAreaModified = true;
	if (!IsOnlyUpdateOnUnselect())
		UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
int CShapeObject::GetNearestPoint( const Vec3 &raySrc,const Vec3 &rayDir,float &distance )
{
	int index = -1;
	float minDist = FLT_MAX;
	Vec3 rayLineP1 = raySrc;
	Vec3 rayLineP2 = raySrc + rayDir*RAY_DISTANCE;
	Vec3 intPnt;
	const Matrix34 &wtm = GetWorldTM();
	for (int i = 0; i < m_points.size(); i++)
	{
		float d = PointToLineDistance( rayLineP1,rayLineP2,wtm.TransformPoint(m_points[i]),intPnt );
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
void CShapeObject::GetNearestEdge( const Vec3 &pos,int &p1,int &p2,float &distance,Vec3 &intersectPoint )
{
	p1 = -1;
	p2 = -1;
	float minDist = FLT_MAX;
	Vec3 intPnt;
	
	const Matrix34 &wtm = GetWorldTM();

	for (int i = 0; i < m_points.size(); i++)
	{
		int j = (i < m_points.size()-1) ? i+1 : 0;

		if (!mv_closed && j == 0 && i != 0)
			continue;

		float d = PointToLineDistance( wtm.TransformPoint(m_points[i]),wtm.TransformPoint(m_points[j]),pos,intPnt );
		if (d < minDist)
		{
			minDist = d;
			p1 = i;
			p2 = j;
			intersectPoint = intPnt;
		}
	}
	distance = minDist;
}

//////////////////////////////////////////////////////////////////////////
bool CShapeObject::RayToLineDistance( const Vec3 &rayLineP1,const Vec3 &rayLineP2,const Vec3 &pi,const Vec3 &pj,float &distance,Vec3 &intPnt )
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
void CShapeObject::GetNearestEdge( const Vec3 &raySrc,const Vec3 &rayDir,int &p1,int &p2,float &distance,Vec3 &intersectPoint )
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

		if (!mv_closed && j == 0 && i != 0)
			continue;

		Vec3 pi = wtm.TransformPoint(m_points[i]);
		Vec3 pj = wtm.TransformPoint(m_points[j]);

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
	distance = minDist;
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::UpdateGameArea( bool bRemove )
{
	if (m_bIgnoreGameUpdate || bRemove)
		return;

	if (!m_entity)
		return;

	if (GetPointCount() < 2)
		return;

	if (!mv_closed)
		return;

	IEntityAreaProxy *pArea = (IEntityAreaProxy*)m_entity->CreateProxy( ENTITY_PROXY_AREA );
	if (!pArea)
		return;
	
	std::vector<Vec3> points;
	if (GetPointCount() > 0)
	{
		points.resize(GetPointCount());
		for (int i = 0; i < GetPointCount(); i++)
		{
			points[i] = GetPoint(i);
		}
		pArea->SetPoints( &points[0],points.size(),GetHeight() );
	}

	pArea->SetProximity( GetWidth() );
	pArea->SetID( mv_areaId );
	pArea->SetGroup( mv_groupId );
	pArea->SetPriority( mv_priority );
	pArea->SetObstruction(mv_obstructRoof, mv_obstructFloor);

	pArea->ClearEntities();
	for (int i = 0; i < GetEntityCount(); i++)
	{
		CEntity *pEntity = (CEntity*)GetEntity(i);
		if (pEntity && pEntity->GetIEntity())
			pArea->AddEntity( pEntity->GetIEntity()->GetId() );
	}

	m_bAreaModified = false;
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::OnShapeChange( IVariable *var )
{
	m_bAreaModified = true;
	if (!m_bIgnoreGameUpdate)
		UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::SpawnEntity()
{
	if (!m_entityClass.IsEmpty())
	{
		__super::SpawnEntity();
		UpdateGameArea();
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::OnEvent( ObjectEvent event )
{
	switch (event)
	{
	case EVENT_ALIGN_TOGRID:
		{
			AlignToGrid();
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::PostLoad( CObjectArchive &ar )
{
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
bool CShapeObject::HitTestRect( HitContext &hc )
{
	BBox box;
	// Retrieve world space bound box.
	GetBoundBox( box );

	// Project all edges to viewport.
	const Matrix34 &wtm = GetWorldTM();
	for (int i = 0; i < m_points.size(); i++)
	{
		int j = (i < m_points.size()-1) ? i+1 : 0;
		if (!mv_closed && j == 0 && i != 0)
			continue;

		Vec3 p0 = wtm.TransformPoint(m_points[i]);
		Vec3 p1 = wtm.TransformPoint(m_points[j]);

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

/*

	// transform all 8 vertices into world space
	CPoint p[8] = 
	{ 
		hc.view->WorldToView(Vec3d(box.min.x,box.min.y,box.min.z)),
			hc.view->WorldToView(Vec3d(box.min.x,box.max.y,box.min.z)),
			hc.view->WorldToView(Vec3d(box.max.x,box.min.y,box.min.z)),
			hc.view->WorldToView(Vec3d(box.max.x,box.max.y,box.min.z)),
			hc.view->WorldToView(Vec3d(box.min.x,box.min.y,box.max.z)),
			hc.view->WorldToView(Vec3d(box.min.x,box.max.y,box.max.z)),
			hc.view->WorldToView(Vec3d(box.max.x,box.min.y,box.max.z)),
			hc.view->WorldToView(Vec3d(box.max.x,box.max.y,box.max.z))
	};

	CRect objrc,temprc;

	objrc.left = 10000;
	objrc.top = 10000;
	objrc.right = -10000;
	objrc.bottom = -10000;
	// find new min/max values
	for(int i=0; i<8; i++)
	{
		objrc.left = min(objrc.left,p[i].x);
		objrc.right = max(objrc.right,p[i].x);
		objrc.top = min(objrc.top,p[i].y);
		objrc.bottom = max(objrc.bottom,p[i].y);
	}
	if (objrc.IsRectEmpty())
	{
		// Make objrc at least of size 1.
		objrc.bottom += 1;
		objrc.right += 1;
	}
	if (temprc.IntersectRect( objrc,hc.rect ))
	{
		hc.object = this;
		return true;
	}
	return false;
	*/
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::AlignToGrid()
{
	CViewport *view = GetIEditor()->GetActiveView();
	if (!view)
		return;

	CUndo undo( "Align Shape To Grid" );
	StoreUndo( "Align Shape To Grid" );

	Matrix34 wtm = GetWorldTM();
	Matrix34 invTM = wtm.GetInverted();
	for (int i = 0; i < GetPointCount(); i++)
	{
		Vec3 pnt = wtm.TransformPoint( GetPoint(i) );
		pnt = view->SnapToGrid( pnt );
		SetPoint( i,invTM.TransformPoint(pnt) );
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CAIForbiddenAreaObject
//////////////////////////////////////////////////////////////////////////
CAIForbiddenAreaObject::CAIForbiddenAreaObject()
{
	m_bDisplayFilledWhenSelected = true;
	SetColor( RGB(255,0,0) );
}

//////////////////////////////////////////////////////////////////////////
void CAIForbiddenAreaObject::UpdateGameArea( bool bRemove )
{
	if (m_bIgnoreGameUpdate)
		return;
	if (!IsCreateGameObjects())
		return;

	IAISystem *ai = GetIEditor()->GetSystem()->GetAISystem();
	if (ai)
	{
		if (m_updateSucceed && !m_lastGameArea.IsEmpty())
			ai->DeleteNavigationShape( m_lastGameArea );
		if (bRemove)
			return;

		if (ai->DoesNavigationShapeExists(GetName(), AREATYPE_FORBIDDEN))
		{
			ai->Error("Forbidden Area", "Shape '%s' already exists in AIsystem, please rename the shape.", GetName() );
			m_updateSucceed = false;
			return;
		}

		std::vector<Vec3> worldPts;
		const Matrix34 &wtm = GetWorldTM();
		for (int i = 0; i < GetPointCount(); i++)
			worldPts.push_back(wtm.TransformPoint(GetPoint(i)));

		m_lastGameArea = GetName();
		if (!worldPts.empty())
    {
      SNavigationShapeParams params(m_lastGameArea, AREATYPE_FORBIDDEN, false, mv_closed, &worldPts[0], worldPts.size());
			m_updateSucceed = ai->CreateNavigationShape(params);
    }
	}
	m_bAreaModified = false;
}

//////////////////////////////////////////////////////////////////////////
// CAIForbiddenBoundaryObject
//////////////////////////////////////////////////////////////////////////
CAIForbiddenBoundaryObject::CAIForbiddenBoundaryObject()
{
	m_bDisplayFilledWhenSelected = true;
	SetColor( RGB(255,255,0) );
}

//////////////////////////////////////////////////////////////////////////
void CAIForbiddenBoundaryObject::UpdateGameArea( bool bRemove )
{
	if (m_bIgnoreGameUpdate)
		return;
	if (!IsCreateGameObjects())
		return;

	IAISystem *ai = GetIEditor()->GetSystem()->GetAISystem();
	if (ai)
	{
		if (m_updateSucceed && !m_lastGameArea.IsEmpty())
			ai->DeleteNavigationShape( m_lastGameArea );
		if (bRemove)
			return;

		if (ai->DoesNavigationShapeExists(GetName(), AREATYPE_FORBIDDENBOUNDARY))
		{
			ai->Error("Forbidden Boundary", "Shape '%s' already exists in AIsystem, please rename the shape.", GetName() );
			m_updateSucceed = false;
			return;
		}

		std::vector<Vec3> worldPts;
		const Matrix34 &wtm = GetWorldTM();
		for (int i = 0; i < GetPointCount(); i++)
			worldPts.push_back(wtm.TransformPoint(GetPoint(i)));

		m_lastGameArea = GetName();
		if (!worldPts.empty())
    {
      SNavigationShapeParams params(m_lastGameArea, AREATYPE_FORBIDDENBOUNDARY, false, mv_closed, &worldPts[0], worldPts.size());
			m_updateSucceed = ai->CreateNavigationShape(params);
    }
	}
	m_bAreaModified = false;
}

//////////////////////////////////////////////////////////////////////////
// CAIPathObject.
//////////////////////////////////////////////////////////////////////////
CAIPathObject::CAIPathObject()
{
	SetColor( RGB(180,180,180) );
  SetClosed(false);
  m_bRoad = true;
	m_bValidatePath = false;
	
	m_navType.AddEnumItem("Unset", (int)IAISystem::NAV_UNSET);
	m_navType.AddEnumItem("Triangular", (int)IAISystem::NAV_TRIANGULAR);
	m_navType.AddEnumItem("Waypoint Human", (int)IAISystem::NAV_WAYPOINT_HUMAN);
	m_navType.AddEnumItem("Waypoint 3D Surface", (int)IAISystem::NAV_WAYPOINT_3DSURFACE);
	m_navType.AddEnumItem("Flight", (int)IAISystem::NAV_FLIGHT);
	m_navType.AddEnumItem("Volume", (int)IAISystem::NAV_VOLUME);
	m_navType.AddEnumItem("Road", (int)IAISystem::NAV_ROAD);
	m_navType.AddEnumItem("SmartObject", (int)IAISystem::NAV_SMARTOBJECT);
	m_navType.AddEnumItem("Free 2D", (int)IAISystem::NAV_FREE_2D);

	m_navType = (int)IAISystem::NAV_UNSET;
	m_anchorType = "";
	
	AddVariable( m_bRoad,"Road",functor(*this,&CAIPathObject::OnShapeChange) );
	AddVariable( m_navType,"PathNavType",functor(*this,&CAIPathObject::OnShapeChange) );
	AddVariable( m_anchorType,"AnchorType",functor(*this,&CAIPathObject::OnShapeChange), IVariable::DT_AI_ANCHOR );
	AddVariable( m_bValidatePath,"ValidatePath" ); //,functor(*this,&CAIPathObject::OnShapeChange) );

	m_bDisplayFilledWhenSelected = false;
}

void CAIPathObject::UpdateGameArea( bool bRemove )
{
	if (m_bIgnoreGameUpdate)
		return;
	if (!IsCreateGameObjects())
		return;

	IAISystem *ai = GetIEditor()->GetSystem()->GetAISystem();
	if (ai)
	{
		if (m_updateSucceed && !m_lastGameArea.IsEmpty())
			ai->DeleteNavigationShape( m_lastGameArea );
		if (bRemove)
			return;

		if (ai->DoesNavigationShapeExists(GetName(), AREATYPE_PATH, m_bRoad))
		{
			ai->Error("AI Path", "Path '%s' already exists in AIsystem, please rename the path.", GetName() );
			m_updateSucceed = false;
			return;
		}

		std::vector<Vec3> worldPts;
		const Matrix34 &wtm = GetWorldTM();
		for (int i = 0; i < GetPointCount(); i++)
			worldPts.push_back(wtm.TransformPoint(GetPoint(i)));

		m_lastGameArea = GetName();
		if (!worldPts.empty())
    {
			CAIManager *pAIMgr = GetIEditor()->GetAI();
			CString	anchorType(m_anchorType);
			int	type = pAIMgr->AnchorActionToId(anchorType);
			SNavigationShapeParams params(m_lastGameArea, AREATYPE_PATH, m_bRoad, mv_closed, &worldPts[0], worldPts.size(), 0, m_navType, type);
			m_updateSucceed = ai->CreateNavigationShape(params);
    }
	}
	m_bAreaModified = false;
}

void CAIPathObject::DrawSphere(DisplayContext &dc, const Vec3& p0, float r, int n)
{
	// Note: The Aux Render already supports drawing spheres, but they appear
	// shaded when rendered and there is no cylinder rendering available.	
	// This function is here just for the purpose to make the rendering of
	// the validatedsegments more consistent.
	Vec3	a0, a1, b0, b1;
	for(int j = 0; j < n/2; j++ )
	{
		float theta1 = j * gf_PI2 / n - gf_PI/2;
		float theta2 = (j + 1) * gf_PI2 / n - gf_PI/2;

		for(int i = 0; i <= n; i++ )
		{
			float theta3 = i * gf_PI2 / n;

			a0.x = p0.x + r * cosf(theta2) * cosf(theta3);
			a0.y = p0.y + r * sinf(theta2);
			a0.z = p0.z + r * cosf(theta2) * sinf(theta3);

			b0.x = p0.x + r * cosf(theta1) * cosf(theta3);
			b0.y = p0.y + r * sinf(theta1);
			b0.z = p0.z + r * cosf(theta1) * sinf(theta3);

			if(i > 0)
				dc.DrawQuad(a0, b0, b1, a1);

			a1 = a0;
			b1 = b0;
		}
	}
}

void CAIPathObject::DrawCylinder(DisplayContext &dc, const Vec3& p0, const Vec3& p1, float rad, int n)
{
	Vec3	dir(p1 - p0);
	Vec3	a = dir.GetOrthogonal();
	Vec3	b = dir.Cross(a);
	a.NormalizeSafe();
	b.NormalizeSafe();

	for(int i = 0; i < n; i++)
	{
		float	a0 = ((float)i / (float)n) * gf_PI2;
		float	a1 = ((float)(i+1) / (float)n) * gf_PI2;
		Vec3	n0 = sinf(a0) * rad * a + cosf(a0) * rad * b;
		Vec3	n1 = sinf(a1) * rad * a + cosf(a1) * rad * b;

		dc.DrawQuad(p0 + n0, p1 + n0, p1 + n1, p0 + n1);
	}
}

bool CAIPathObject::IsSegmentValid(const Vec3& p0, const Vec3& p1, float rad)
{
	AABB	box;
	box.Reset();
	box.Add(p0);
	box.Add(p1);
	box.min -= Vec3(rad, rad, rad);
	box.max += Vec3(rad, rad, rad);

	IPhysicalEntity **entities;
	unsigned nEntities = gEnv->pPhysicalWorld->GetEntitiesInBox(box.min, box.max, entities, ent_static|ent_ignore_noncolliding);

	primitives::sphere spherePrim;
	spherePrim.center = p0;
	spherePrim.r = rad;
	Vec3	dir(p1 - p0);

	unsigned hitCount = 0;
	ray_hit hit;
	IPhysicalWorld* pPhysics = gEnv->pPhysicalWorld;

	for (unsigned iEntity = 0 ; iEntity < nEntities ; ++iEntity)
	{
		IPhysicalEntity *pEntity = entities[iEntity];
		if (pPhysics->CollideEntityWithPrimitive(pEntity, spherePrim.type, &spherePrim, dir, &hit))
		{
			return false;
			break;
		}
	}
	return true;
}

void CAIPathObject::Display( DisplayContext &dc )
{
	// Display path validation
	if(m_bValidatePath && IsSelected())
	{
		dc.DepthWriteOff();
		int num = m_points.size();
		if(!m_bRoad && m_navType == (int)IAISystem::NAV_VOLUME && num >= 3)
		{
			const float rad = mv_width;
			const Matrix34 &wtm = GetWorldTM();

			if(!mv_closed)
			{
				Vec3 p0 = wtm.TransformPoint(m_points.front());
				float	viewScale = dc.view->GetScreenScaleFactor(p0);
				if(viewScale < 0.01f) viewScale = 0.01f;
				unsigned n = CLAMP((unsigned)(300.0f / viewScale), 4, 20);
				DrawSphere(dc, p0, rad, n);
			}

			for (int i = 0; i < num; i++)
			{
				int j = (i < m_points.size()-1) ? i+1 : 0;
				if (!mv_closed && j == 0 && i != 0)
					continue;

				Vec3 p0 = wtm.TransformPoint(m_points[i]);
				Vec3 p1 = wtm.TransformPoint(m_points[j]);

				float	viewScale = max(dc.view->GetScreenScaleFactor(p0), dc.view->GetScreenScaleFactor(p1));
				if(viewScale < 0.01f) viewScale = 0.01f;
				unsigned n = CLAMP((unsigned)(300.0f / viewScale), 4, 20);

				if(IsSegmentValid(p0, p1, rad))
					dc.SetColor(1,1,1,0.25f);
				else
					dc.SetColor(1,0.5f,0,0.25f);

				DrawCylinder(dc, p0, p1, rad, n);
				DrawSphere(dc, p1, rad, n);
			}
		}
		dc.DepthWriteOn();
	}

	// Display the direction of the path
	if(m_points.size() > 1)
	{
		const Matrix34 &wtm = GetWorldTM();
		Vec3	p0 = wtm.TransformPoint(m_points[0]);
		Vec3	p1 = wtm.TransformPoint(m_points[1]);
		if(IsSelected())
			dc.SetColor(dc.GetSelectedColor());
		else if(IsHighlighted())
			dc.SetColor( RGB(255,120,0) );
		else
			dc.SetColor(GetColor());
		Vec3	pt((p1 - p0)/2);
		if(pt.GetLength() > 1.0f)
			pt.SetLength(1.0f);
		dc.DrawArrow(p0, p0 + pt);
	}

	CShapeObject::Display(dc);
}

//////////////////////////////////////////////////////////////////////////
// CAIShapeObject.
//////////////////////////////////////////////////////////////////////////
CAIShapeObject::CAIShapeObject()
{
	SetColor( RGB(30,65,120) );
	SetClosed(true);

	m_anchorType = "";
	AddVariable( m_anchorType,"AnchorType",functor(*this,&CAIShapeObject::OnShapeChange), IVariable::DT_AI_ANCHOR );

	m_lightLevel.AddEnumItem("Default", AILL_NONE);
	m_lightLevel.AddEnumItem("Light", AILL_LIGHT);
	m_lightLevel.AddEnumItem("Medium", AILL_MEDIUM);
	m_lightLevel.AddEnumItem("Dark", AILL_DARK);
	m_lightLevel = AILL_NONE;
	AddVariable( m_lightLevel,"LightLevel",functor(*this,&CAIShapeObject::OnShapeChange) );

	m_bDisplayFilledWhenSelected = false;
}

void CAIShapeObject::UpdateGameArea( bool bRemove )
{
	if (m_bIgnoreGameUpdate)
		return;
	if (!IsCreateGameObjects())
		return;

	IAISystem *ai = GetIEditor()->GetSystem()->GetAISystem();
	if (ai)
	{
		if (m_updateSucceed && !m_lastGameArea.IsEmpty())
			ai->DeleteNavigationShape( m_lastGameArea );
		if (bRemove)
			return;

		if (ai->DoesNavigationShapeExists(GetName(), AREATYPE_GENERIC))
		{
			ai->Error("AI Shape", "Shape '%s' already exists in AIsystem, please rename the shape.", GetName() );
			m_updateSucceed = false;
			return;
		}

		std::vector<Vec3> worldPts;
		const Matrix34 &wtm = GetWorldTM();
		for (int i = 0; i < GetPointCount(); i++)
			worldPts.push_back(wtm.TransformPoint(GetPoint(i)));

		m_lastGameArea = GetName();
		if (!worldPts.empty())
		{
			CAIManager *pAIMgr = GetIEditor()->GetAI();
			CString	anchorType(m_anchorType);
			int	type = pAIMgr->AnchorActionToId(anchorType);
			SNavigationShapeParams params(m_lastGameArea, AREATYPE_GENERIC, false, mv_closed, &worldPts[0], worldPts.size(),
				GetHeight(), 0, type, (EAILightLevel)(int)m_lightLevel);
			m_updateSucceed = ai->CreateNavigationShape(params);
		}
	}
	m_bAreaModified = false;
}

//////////////////////////////////////////////////////////////////////////
// CAINavigationModifierObject
//////////////////////////////////////////////////////////////////////////
CAINavigationModifierObject::CAINavigationModifierObject()
{
	m_navType.AddEnumItem("Human Waypoint", NMT_WAYPOINTHUMAN);
	m_navType.AddEnumItem("Volume", NMT_VOLUME);
	m_navType.AddEnumItem("Flight", NMT_FLIGHT);
	m_navType.AddEnumItem("Water", NMT_WATER);
	m_navType.AddEnumItem("3D Surface Waypoint ", NMT_WAYPOINT_3DSURFACE);
	m_navType.AddEnumItem("Extra Link Cost", NMT_EXTRA_NAV_COST);
	m_navType.AddEnumItem("Dynamic Triangulation", NMT_DYNAMIC_TRIANGULATION);
	m_navType.AddEnumItem("Free 2D", NMT_FREE_2D);
  m_navType.AddEnumItem("Triangulation", NMT_TRIANGULATION);
	m_navType = NMT_WAYPOINTHUMAN;

  m_waypointConnections.AddEnumItem("Designer", WPCON_DESIGNER_NONE);
  m_waypointConnections.AddEnumItem("Designer-Dynamic", WPCON_DESIGNER_PARTIAL);
  m_waypointConnections.AddEnumItem("Auto", WPCON_AUTO_NONE);
  m_waypointConnections.AddEnumItem("Auto-Dynamic", WPCON_AUTO_PARTIAL);
  m_waypointConnections = WPCON_DESIGNER_NONE;
	m_oldWaypointConnections = m_waypointConnections;

	m_lightLevel.AddEnumItem("Default", AILL_NONE);
	m_lightLevel.AddEnumItem("Light", AILL_LIGHT);
	m_lightLevel.AddEnumItem("Medium", AILL_MEDIUM);
	m_lightLevel.AddEnumItem("Dark", AILL_DARK);
	m_lightLevel = AILL_NONE;

  m_bDisplayFilledWhenSelected = true;
	SetColor( RGB(24,128,231) );
  m_bCalculate3DNav = true;
  m_f3DNavVolumeRadius = 10.0f;
	m_fNodeAutoConnectDistance = 8.0f;
  m_fExtraLinkCostFactor = 0.0f;
  m_bVehiclesInHumanNav = false;
  m_fExtraLinkCostFactor.SetLimits(-1.0f, 100.0f);
  m_fTriangulationSize = 1.0f;
	AddVariable( m_navType,"NavType",functor(*this,&CAINavigationModifierObject::OnShapeChange) );
	AddVariable( m_waypointConnections,"WaypointConnections",functor(*this,&CAINavigationModifierObject::OnShapeChange) );
	AddVariable( m_fNodeAutoConnectDistance,"NodeAutoConnectDistance",functor(*this,&CAINavigationModifierObject::OnShapeChange) );
	AddVariable( m_bCalculate3DNav,"Calculate3DNav",functor(*this,&CAINavigationModifierObject::OnShapeChange) );
  AddVariable( m_f3DNavVolumeRadius,"ThreeDNavVolumeRadius",functor(*this,&CAINavigationModifierObject::OnShapeChange) );
	AddVariable( m_fExtraLinkCostFactor,"ExtraLinkCostFactor",functor(*this,&CAINavigationModifierObject::OnShapeChange) );
	AddVariable( m_fTriangulationSize,"TriangulationSize",functor(*this,&CAINavigationModifierObject::OnShapeChange) );
  AddVariable( m_bVehiclesInHumanNav,"VehiclesInHumanNav",functor(*this,&CAINavigationModifierObject::OnShapeChange) );
	AddVariable( m_lightLevel,"LightLevel",functor(*this,&CAINavigationModifierObject::OnShapeChange) );
}

void CAINavigationModifierObject::Serialize( CObjectArchive &ar )
{
	XmlNodeRef xmlNode = ar.node;
	if (ar.bLoading)
	{
    bool oldAutoGenerateNav = false;
		xmlNode->getAttr("AutoGenerateNav", oldAutoGenerateNav);
    if (oldAutoGenerateNav)
      m_waypointConnections = WPCON_AUTO_PARTIAL;
  }
  __super::Serialize(ar);

	if (ar.bLoading)
  {
    bool useAreaIDForNavType = true;
    xmlNode->getAttr("UseAreaIDForNavType", useAreaIDForNavType);
    if (useAreaIDForNavType)
    {
      m_navType = GetAreaId();
      mv_areaId = 0;
    }
  }
  else
  {
    bool useAreaIDForNavType = false;
    xmlNode->setAttr("UseAreaIDForNavType", useAreaIDForNavType);
  }

}


void CAINavigationModifierObject::UpdateGameArea( bool bRemove )
{
	if (m_bIgnoreGameUpdate)
		return;
	if (!IsCreateGameObjects())
		return;

	IAISystem *ai = GetIEditor()->GetSystem()->GetAISystem();
	if (ai)
	{
		if (m_updateSucceed && !m_lastGameArea.IsEmpty())
			ai->DeleteNavigationShape( m_lastGameArea );
		if (bRemove)
			return;

		if (ai->DoesNavigationShapeExists(GetName(), AREATYPE_NAVIGATIONMODIFIER))
		{
			ai->Error("Navigation Modifier", "Shape '%s' already exists in AIsystem, please rename the shape.", GetName() );
			m_updateSucceed = false;
			return;
		}

		std::vector<Vec3> worldPts;
		const Matrix34 &wtm = GetWorldTM();
		for (int i = 0; i < GetPointCount(); i++)
			worldPts.push_back(wtm.TransformPoint(GetPoint(i)));

		m_lastGameArea = GetName();
    if (m_waypointConnections < 0)
      m_waypointConnections = 0;
    else if (m_waypointConnections > WPCON_MAXVALUE)
      m_waypointConnections = WPCON_MAXVALUE;
		if (!worldPts.empty())
    {
      SNavigationShapeParams params(m_lastGameArea, AREATYPE_NAVIGATIONMODIFIER, false, mv_closed, &worldPts[0], worldPts.size(), 
				GetHeight(), m_navType, 0, (EAILightLevel)(int)m_lightLevel,
				m_fNodeAutoConnectDistance, (EWaypointConnections) (int) m_waypointConnections, m_bVehiclesInHumanNav,
        m_bCalculate3DNav, m_f3DNavVolumeRadius, m_fExtraLinkCostFactor, m_fTriangulationSize);
			m_updateSucceed = ai->CreateNavigationShape(params);
		  if (m_oldWaypointConnections != m_waypointConnections)
			  ai->SetUseAutoNavigation(GetName(), (EWaypointConnections) (int) m_waypointConnections);
		  m_oldWaypointConnections = m_waypointConnections;
    }
	}
	m_bAreaModified = false;
}

//////////////////////////////////////////////////////////////////////////
// CAIOclussionPlaneObject
//////////////////////////////////////////////////////////////////////////
CAIOcclusionPlaneObject::CAIOcclusionPlaneObject()
{
	m_bDisplayFilledWhenSelected = true;
	SetColor( RGB(24,90,231) );
	m_bForce2D = true;
	mv_closed = true;
	mv_displayFilled = true;
}

void CAIOcclusionPlaneObject::UpdateGameArea( bool bRemove )
{
	if (m_bIgnoreGameUpdate)
		return;
	if (!IsCreateGameObjects())
		return;

	IAISystem *ai = GetIEditor()->GetSystem()->GetAISystem();
	if (ai)
	{
		if (m_updateSucceed && !m_lastGameArea.IsEmpty())
			ai->DeleteNavigationShape( m_lastGameArea );
		if (bRemove)
			return;

		if (ai->DoesNavigationShapeExists(GetName(), AREATYPE_OCCLUSION_PLANE))
		{
			ai->Error("OcclusionPlane", "Shape '%s' already exists in AIsystem, please rename the shape.", GetName() );
			m_updateSucceed = false;
			return;
		}

		std::vector<Vec3> worldPts;
		const Matrix34 &wtm = GetWorldTM();
		for (int i = 0; i < GetPointCount(); i++)
			worldPts.push_back(wtm.TransformPoint(GetPoint(i)));

		m_lastGameArea = GetName();
		if (!worldPts.empty())
    {
      SNavigationShapeParams params(m_lastGameArea,AREATYPE_OCCLUSION_PLANE, false, mv_closed, &worldPts[0], worldPts.size(), GetHeight());
			m_updateSucceed = ai->CreateNavigationShape(params);
    }
	}
	m_bAreaModified = false;
}