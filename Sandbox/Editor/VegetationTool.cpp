////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   VegetationTool.cpp
//  Version:     v1.00
//  Created:     11/1/2002 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: Places vegetation on terrain.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "VegetationTool.h"
#include "Viewport.h"
#include "VegetationPanel.h"
#include "Heightmap.h"
#include "VegetationMap.h"
#include "Objects\DisplayContext.h"
#include "Objects\BaseObject.h"
#include "NumberDlg.h"
#include "PanelPreview.h"
#include "Settings.h"
#include "Plugin.h"
#include "Include\ITransformManipulator.h"

#include "I3dEngine.h"
#include "IPhysics.h"

#include <InitGuid.h>
// {D25B8229-7FE7-45d4-8AC5-CD6DA1365879}
DEFINE_GUID( VEGETATION_TOOL_GUID, 0xd25b8229, 0x7fe7, 0x45d4, 0x8a, 0xc5, 0xcd, 0x6d, 0xa1, 0x36, 0x58, 0x79);


//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CVegetationTool,CEditTool)

float CVegetationTool::m_brushRadius = 1;
//bool CVegetationTool::m_bPlaceMode = true;

//////////////////////////////////////////////////////////////////////////
CVegetationTool::CVegetationTool()
{
	m_pClassDesc = GetIEditor()->GetClassFactory()->FindClass(VEGETATION_TOOL_GUID);
	m_panelId = 0;
	m_panel = 0;
	m_panelPreview = 0;
	m_panelPreviewId = 0;

	m_pointerPos(0,0,0);

	m_vegetationMap = GetIEditor()->GetHeightmap()->GetVegetationMap();

	GetIEditor()->ClearSelection();

	m_bPaintingMode = false;
	m_bPlaceMode = true;

	m_opMode = OPMODE_NONE;

	SetStatusText( "Click to Place or Remove Vegetation" );
}

//////////////////////////////////////////////////////////////////////////
CVegetationTool::~CVegetationTool()
{
	m_pointerPos(0,0,0);
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::BeginEditParams( IEditor *ie,int flags )
{
	m_ie = ie;
	if (!m_panelId)
	{
		CWaitCursor wait;
		m_panel = new CVegetationPanel(this,AfxGetMainWnd());
		m_panelId = m_ie->AddRollUpPage( ROLLUP_TERRAIN,"Vegetation",m_panel );

		if (gSettings.bPreviewGeometryWindow)
		{
			m_panelPreview = new CPanelPreview(AfxGetMainWnd());
			m_panelPreviewId = m_ie->AddRollUpPage( ROLLUP_TERRAIN,"Object Preview",m_panelPreview );
			m_panel->SetPreviewPanel(m_panelPreview);
		}

		AfxGetMainWnd()->SetFocus();

		GetIEditor()->UpdateViews( eUpdateStatObj );
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::EndEditParams()
{
	if (m_panelPreviewId)
	{
		m_ie->RemoveRollUpPage(ROLLUP_TERRAIN,m_panelPreviewId);
		m_panelPreviewId = 0;
		m_panelPreview = 0;
	}
	if (m_panelId)
	{
		m_ie->RemoveRollUpPage(ROLLUP_TERRAIN,m_panelId);
		m_panel = 0;
		m_panelId = 0;
	}
	GetIEditor()->SetStatusText( "Ready" );
}

//////////////////////////////////////////////////////////////////////////
// Specific mouse events handlers.
bool CVegetationTool::OnLButtonDown( CViewport *view,UINT nFlags,CPoint point )
{
	m_mouseDownPos = point;
	bool bShift = nFlags & MK_SHIFT;
	bool bCtrl = nFlags & MK_CONTROL;
	bool bAlt = (GetAsyncKeyState(VK_MENU) & (1<<15)) != 0;

	m_opMode = OPMODE_NONE;
	if (m_bPaintingMode)
	{
		m_opMode = OPMODE_PAINT;

		if (nFlags&MK_CONTROL)
      m_bPlaceMode = false;
		else
			m_bPlaceMode = true;
		
		GetIEditor()->BeginUndo();
		view->CaptureMouse();
		PaintBrush();
	}
	else
	{
		Matrix34 tm;
		tm.SetIdentity();
		tm.SetTranslation( view->ViewToWorld(point) );
		view->SetConstructionMatrix( COORDS_LOCAL,tm );

		bool bModifySelection = bCtrl || bAlt;

		/*
		if (bCtrl)
		{
			//////////////////////////////////////////////////////////////////////////
			// Hit test current transform manipulator.
			//////////////////////////////////////////////////////////////////////////
			ITransformManipulator *pManipulator = GetIEditor()->GetTransformManipulator();
			if (pManipulator && !bModifySelection)
			{
				HitContext hc;
				hc.view = view;
				hc.point2d = point;
				view->ViewToWorldRay(point,hc.raySrc,hc.rayDir);
				if (pManipulator->HitTestManipulator(hc))
				{
					// Hit axis gizmo.
					//GetIEditor()->SetAxisConstrains( (AxisConstrains)hc.axis );
					//view->SetAxisConstrain( hc.axis );
					return true;
				}
			}
		}
		*/

		m_opMode = OPMODE_SELECT;

		if (bShift)
		{
			PlaceThing();
			m_opMode = OPMODE_MOVE;
			GetIEditor()->BeginUndo();
		}
		else
		{
			CVegetationInstance *pObj = SelectThingAtPoint(view,point,true);
			if (IsThingSelected(pObj))
				bModifySelection = true;

			if (!bModifySelection)
			{
				ClearThingSelection();
			}
			// Select thing.
			if (SelectThingAtPoint(view,point,false,bCtrl))
			{
				if (bAlt)
				{
					if (bCtrl)
						m_opMode = OPMODE_ROTATE;
					else
						m_opMode = OPMODE_SCALE;
				}
				else
					m_opMode = OPMODE_MOVE;
				GetIEditor()->BeginUndo();
			}
		}
		view->CaptureMouse();
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CVegetationTool::IsThingSelected( CVegetationInstance *pObj )
{
	if (!pObj)
		return false;
	for (int i = 0; i < m_selectedThings.size(); i++)
	{
		if (m_selectedThings[i] == pObj)
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CVegetationTool::OnLButtonUp( CViewport *view,UINT nFlags,CPoint point )
{
	if (GetIEditor()->IsUndoRecording())
	{
		if (m_opMode == OPMODE_MOVE)
			GetIEditor()->AcceptUndo( "Vegetation Move" );
		else if (m_opMode == OPMODE_SCALE)
			GetIEditor()->AcceptUndo( "Vegetation Scale" );
		else if (m_opMode == OPMODE_ROTATE)
			GetIEditor()->AcceptUndo( "Vegetation Rotate" );
		else if (m_opMode == OPMODE_PAINT)
			GetIEditor()->AcceptUndo( "Vegetation Paint" );
		else
			GetIEditor()->CancelUndo();
	}
	if (m_opMode == OPMODE_SELECT)
	{
		CRect rect = view->GetSelectionRectangle();
		if (!(nFlags & MK_CONTROL))
		{
			ClearThingSelection();
		}
		if (!rect.IsRectEmpty())
			SelectThingsInRect( view,rect,true );
		/*
		BBox box;
		GetIEditor()->GetSelectedRegion( box );
		if (!box.IsEmpty())
		{
			ClearThingSelection();
			std::vector<CVegetationInstance*> selectedThings;
			m_vegetationMap->GetObjectInstances( box.min.x,box.min.y,box.max.x,box.max.y,selectedThings );
			for (int i = 0; i < selectedThings.size(); i++)
			{
				if (!selectedThings[i]->object->IsHidden())
				{
					SelectThing( selectedThings[i],false );
				}
			}
		}
		*/
	}
	m_opMode = OPMODE_NONE;

	view->ReleaseMouse();

	// Reset selected region.
	view->ResetSelectionRegion();
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CVegetationTool::OnMouseMove( CViewport *view,UINT nFlags,CPoint point )
{
	if (nFlags&MK_CONTROL)
		m_bPlaceMode = false;
	else
		m_bPlaceMode = true;

	if (m_opMode == OPMODE_PAINT)
	{
		if (nFlags&MK_LBUTTON)
			PaintBrush();
	}
	else if (m_opMode == OPMODE_SELECT)
	{
    // Define selection.
		view->SetSelectionRectangle( m_mouseDownPos,point );
		//CRect rc( m_cMouseDownPos,point );
	}
	else if (m_opMode == OPMODE_MOVE)
	{
		GetIEditor()->RestoreUndo();
    // Define selection.
		int axis = GetIEditor()->GetAxisConstrains();
		Vec3 p1 = view->MapViewToCP( m_mouseDownPos );
		Vec3 p2 = view->MapViewToCP( point );
		Vec3 v = view->GetCPVector(p1,p2);
		MoveSelected( view,v,(axis == AXIS_TERRAIN) );

		//m_mouseDownPos = point;
	}
	else if (m_opMode == OPMODE_SCALE)
	{
		GetIEditor()->RestoreUndo();
		// Define selection.
		int y = m_mouseDownPos.y - point.y;
		if (y != 0)
		{
			float fScale = 1.0f + (float)y/100.0f;
			ScaleSelected( fScale );
		}
	}
	else if (m_opMode == OPMODE_ROTATE)
	{
		GetIEditor()->RestoreUndo();
		// Define selection.
		int y = m_mouseDownPos.y - point.y;
		if (y != 0)
		{
			RotateSelected( y );
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CVegetationTool::MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	m_pointerPos = view->ViewToWorld( point,0,true );
	m_mousePos = point;
	m_bPlaceMode = true;

	bool bProcessed = false;
	if (event == eMouseLDown)
	{
		bProcessed = OnLButtonDown( view,flags,point );
	}
	else if (event == eMouseLUp)
	{
		bProcessed = OnLButtonUp( view,flags,point );
	}
	else if (event == eMouseMove)
	{
		bProcessed = OnMouseMove( view,flags,point );
	}

	GetIEditor()->SetMarkerPosition( m_pointerPos );
	int unitSize = GetIEditor()->GetHeightmap()->GetUnitSize();

	if (flags & MK_CONTROL)
	{
		//swap x/y
		float slope = GetIEditor()->GetHeightmap()->GetSlope( m_pointerPos.y/unitSize,m_pointerPos.x/unitSize );
		char szNewStatusText[512];
		sprintf(szNewStatusText, "Slope: %g",slope );
		GetIEditor()->SetStatusText(szNewStatusText);
	}
	else
	{
		GetIEditor()->SetStatusText( "Shift: Place New  Ctrl: Add To Selection  Alt: Scale Selected  Alt+Ctrl: Rotate Selected DEL: Delete Selected" );
	}

	m_prevMousePos = point;

	// Not processed.
	return bProcessed;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::Display( DisplayContext &dc )
{
	if (!m_bPaintingMode)
	{
		if (dc.flags & DISPLAY_2D)
			return;

		
		// Single object 3D display mode.
		for (int i = 0; i < m_selectedThings.size(); i++)
		{
			float radius = m_selectedThings[i]->object->GetObjectSize() * m_selectedThings[i]->scale;
			dc.SetColor( 0,1,0,1 );
			dc.DrawTerrainCircle( m_selectedThings[i]->pos,radius/2.0f,0.1f, m_selectedThings[i]->object->IsAffectedByVoxels() );
			dc.SetColor( 0,0,1.0f,0.7f );
			dc.DrawTerrainCircle( m_selectedThings[i]->pos,10.0f,0.1f, m_selectedThings[i]->object->IsAffectedByVoxels() );
      float aiRadius = m_selectedThings[i]->object->GetAIRadius() * m_selectedThings[i]->scale;
      if (aiRadius > 0.0f)
      {
			  dc.SetColor( 1,0,1.0f,0.7f );
			  dc.DrawTerrainCircle( m_selectedThings[i]->pos,aiRadius,0.1f, m_selectedThings[i]->object->IsAffectedByVoxels() );
      }
		}
	}
	else
	{
		// Brush paint mode.

		if (dc.flags & DISPLAY_2D)
		{
			CPoint p1 = dc.view->WorldToView(m_pointerPos);
			CPoint p2 = dc.view->WorldToView(m_pointerPos+Vec3(m_brushRadius,0,0));
			float radius = sqrtf( (p2.x-p1.x)*(p2.x-p1.x) + (p2.y-p1.y)*(p2.y-p1.y) );
			dc.SetColor( 0,1,0,1 );
			dc.DrawWireCircle2d( p1,radius,0 );
			return;
		}

		dc.SetColor( 0,1,0,1 );
		dc.DrawTerrainCircle( m_pointerPos,m_brushRadius,0.2f, true );

		float col[4] = { 1,1,1,0.8f };
		if (m_bPlaceMode)
			dc.renderer->DrawLabelEx( m_pointerPos+Vec3(0,0,1),1.0f,col,true,true,"Place" );
		else
			dc.renderer->DrawLabelEx( m_pointerPos+Vec3(0,0,1),1.0f,col,true,true,"Remove" );
	}
}

//////////////////////////////////////////////////////////////////////////
bool CVegetationTool::OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{
	bool bProcessed = false;
	if (nChar == VK_ADD)
	{
		if (m_brushRadius < 300)
			m_brushRadius += 1;
		bProcessed = true;
	}
	if (nChar == VK_SUBTRACT)
	{
		if (m_brushRadius > 1)
			m_brushRadius -= 1;
		bProcessed = true;
	}
	if (nChar == VK_DELETE)
	{
		CUndo undo( "Vegetation Delete" );
		if (!m_selectedThings.empty())
		{
			if (AfxMessageBox( "Delete Selected Instances?",MB_YESNO) == IDYES)
			{
				for (int i = 0; i < m_selectedThings.size(); i++)
				{
					m_vegetationMap->DeleteObjInstance( m_selectedThings[i] );
				}
				ClearThingSelection();
				if (m_panel)
					m_panel->UpdateAllObjectsInTree();
			}
		}
		bProcessed = true;
	}
	if (nChar == VK_CONTROL && !(nFlags&(1<<14))) // only once (no repeat).
	{
		m_bPlaceMode = true;
		m_opMode = OPMODE_NONE;
	}
	if (bProcessed && m_panel)
	{
		m_panel->m_radius.SetValue(m_brushRadius);
	}
	return bProcessed;
}

//////////////////////////////////////////////////////////////////////////
bool CVegetationTool::OnKeyUp( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{
	if (nChar == VK_CONTROL)
	{
		m_bPlaceMode = true;
		m_opMode = OPMODE_NONE;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::GetSelectedObjects( std::vector<CVegetationObject*> &objects )
{
	objects.clear();
	for (int i = 0; i < m_vegetationMap->GetObjectCount(); i++)
	{
		if (m_vegetationMap->GetObject(i)->IsSelected())
			objects.push_back( m_vegetationMap->GetObject(i) );
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::PaintBrush()
{
	GetSelectedObjects( m_selectedObjects );

	CRect rc( m_pointerPos.x-m_brushRadius,m_pointerPos.y-m_brushRadius,
						m_pointerPos.x+m_brushRadius,m_pointerPos.y+m_brushRadius );
	if (m_bPlaceMode)
	{
		for (int i = 0; i < m_selectedObjects.size(); i++)
		{
			int numInstances = m_selectedObjects[i]->GetNumInstances();
			m_vegetationMap->PaintBrush( rc,true,m_selectedObjects[i] );
			// If number of instances changed.
			if (numInstances != m_selectedObjects[i]->GetNumInstances() && m_panel)
				m_panel->UpdateObjectInTree( m_selectedObjects[i] );
		}
	}
	else
	{
		for (int i = 0; i < m_selectedObjects.size(); i++)
		{
			int numInstances = m_selectedObjects[i]->GetNumInstances();
			m_vegetationMap->ClearBrush( rc,true,m_selectedObjects[i] );
			// If number of instances changed.
			if (numInstances != m_selectedObjects[i]->GetNumInstances() && m_panel)
				m_panel->UpdateObjectInTree( m_selectedObjects[i] );
		}
	}

	BBox updateRegion;
	updateRegion.min = m_pointerPos - Vec3(m_brushRadius,m_brushRadius,m_brushRadius);
	updateRegion.max = m_pointerPos + Vec3(m_brushRadius,m_brushRadius,m_brushRadius);
	GetIEditor()->UpdateViews( eUpdateStatObj,&updateRegion );
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::PlaceThing()
{
	GetSelectedObjects( m_selectedObjects );

	CUndo undo( "Vegetation Place" );
	if (!m_selectedObjects.empty())
	{
		CVegetationInstance *thing = m_vegetationMap->PlaceObjectInstance( m_pointerPos,m_selectedObjects[0] );
		// If number of instances changed.
		if (thing)
		{
			ClearThingSelection();
			SelectThing(thing);
			if (m_panel)
				m_panel->UpdateObjectInTree( m_selectedObjects[0] );

			BBox updateRegion;
			updateRegion.min = m_pointerPos - Vec3(1,1,1);
			updateRegion.max = m_pointerPos + Vec3(1,1,1);
			GetIEditor()->UpdateViews( eUpdateStatObj,&updateRegion );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::Distribute()
{
	GetSelectedObjects( m_selectedObjects );
	
	for (int i = 0; i < m_selectedObjects.size(); i++)
	{
		int numInstances = m_selectedObjects[i]->GetNumInstances();

		CRect rc( 0,0,m_vegetationMap->GetSize(),m_vegetationMap->GetSize() );
		m_vegetationMap->PaintBrush( rc,false,m_selectedObjects[i] );

		if (numInstances != m_selectedObjects[i]->GetNumInstances() && m_panel)
			m_panel->UpdateObjectInTree( m_selectedObjects[i] );
	}
	if (!m_selectedObjects.empty())
		GetIEditor()->UpdateViews( eUpdateStatObj );

	ClearThingSelection();
}
	
//////////////////////////////////////////////////////////////////////////
void CVegetationTool::DistributeMask( const char *maskFile )
{
	GetSelectedObjects( m_selectedObjects );
	
	for (int i = 0; i < m_selectedObjects.size(); i++)
	{
		int numInstances = m_selectedObjects[i]->GetNumInstances();
		//map->PaintMask( maskFile,m_selectedObjects[i] );

		if (numInstances != m_selectedObjects[i]->GetNumInstances() && m_panel)
			m_panel->UpdateObjectInTree( m_selectedObjects[i] );
	}
	if (!m_selectedObjects.empty())
		GetIEditor()->UpdateViews( eUpdateStatObj );

	ClearThingSelection();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::Clear()
{
	GetSelectedObjects( m_selectedObjects );
	
	for (int i = 0; i < m_selectedObjects.size(); i++)
	{
		int numInstances = m_selectedObjects[i]->GetNumInstances();

		CRect rc( 0,0,m_vegetationMap->GetSize(),m_vegetationMap->GetSize());
		m_vegetationMap->ClearBrush( rc,false,m_selectedObjects[i] );

		if (numInstances != m_selectedObjects[i]->GetNumInstances() && m_panel)
			m_panel->UpdateObjectInTree( m_selectedObjects[i] );
	}
	if (!m_selectedObjects.empty())
		GetIEditor()->UpdateViews( eUpdateStatObj );

	ClearThingSelection();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::ClearMask( const char *maskFile )
{
	//GetSelectedObjects( m_selectedObjects );
	
	/*
	for (int i = 0; i < m_selectedObjects.size(); i++)
	{
		int numInstances = m_selectedObjects[i]->GetNumInstances();

		//m_vegetationMap->ClearMask( maskFile,m_selectedObjects[i] );
		if (numInstances != m_selectedObjects[i]->GetNumInstances())
			m_panel->UpdateObjectInTree( m_selectedObjects[i] );
	}
	*/
	
	m_vegetationMap->ClearMask( maskFile );

	//if (!m_selectedObjects.empty())
		GetIEditor()->UpdateViews( eUpdateStatObj );

	ClearThingSelection();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::DoRandomRotate()
{
	GetSelectedObjects( m_selectedObjects );
	for (int i = 0; i < m_selectedObjects.size(); i++)
	{
		m_vegetationMap->RandomRotateInstances( m_selectedObjects[i] );
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::DoClearRotate()
{
	GetSelectedObjects( m_selectedObjects );
	for (int i = 0; i < m_selectedObjects.size(); i++)
	{
		m_vegetationMap->RandomRotateInstances( m_selectedObjects[i] );
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::HideSelectedObjects( bool bHide )
{
	GetSelectedObjects( m_selectedObjects );

	for (int i = 0; i < m_selectedObjects.size(); i++)
	{
		m_vegetationMap->HideObject( m_selectedObjects[i],bHide );
	}
	if (!m_selectedObjects.empty())
	{
		/*
		GetIEditor()->UpdateViews( eUpdateStatObj );
		CStatObjMap *vegetationMap = GetIEditor()->GetStatObjMap();
		vegetationMap->RemoveObjectsFromTerrain();
		vegetationMap->PlaceObjectsOnTerrain();
		*/
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::RemoveSelectedObjects()
{
	GetSelectedObjects( m_selectedObjects );

	GetIEditor()->BeginUndo();
	for (int i = 0; i < m_selectedObjects.size(); i++)
	{
		int numInstances = m_selectedObjects[i]->GetNumInstances();

		m_vegetationMap->RemoveObject( m_selectedObjects[i] );

		if (numInstances != m_selectedObjects[i]->GetNumInstances() && m_panel)
			m_panel->UpdateObjectInTree( m_selectedObjects[i] );
	}
	GetIEditor()->AcceptUndo( "Remove Brush" );

	ClearThingSelection();

	if (!m_selectedObjects.empty())
	{
		GetIEditor()->UpdateViews( eUpdateStatObj );
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::SetMode( bool paintMode )
{
	if (paintMode)
	{
		GetIEditor()->SetStatusText( "Hold Ctrl to Remove Vegetation" );
	}
	else
	{
		GetIEditor()->SetStatusText( "Push Paint button to start painting" );
	}

	m_bPaintingMode = paintMode;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::ClearThingSelection()
{
  for (int i=0; i<m_selectedThings.size(); i++)
  {
    CVegetationInstance *thing = m_selectedThings[i];
    if (thing->pRenderNode)
      thing->pRenderNode->SetRndFlags(ERF_SELECTED, false);
  }
	m_selectedThings.clear();
	GetIEditor()->ShowTransformManipulator(false);
	if (m_panel)
	{
		m_panel->UnselectAll();
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::SelectThing( CVegetationInstance *thing,bool bSelObject,bool bShowManipulator )
{
	// If already selected.
	if (std::find(m_selectedThings.begin(),m_selectedThings.end(),thing) != m_selectedThings.end())
		return;

	if (thing->object->IsHidden())
		return;

  if (thing->pRenderNode)
    thing->pRenderNode->SetRndFlags(ERF_SELECTED, true);

	m_selectedThings.push_back(thing);
	if (m_panel && bSelObject)
	{
		if (!thing->object->IsSelected())
			m_panel->SelectObject( thing->object,(m_selectedThings.size() > 1) );
	}

	float fAngle = 0;
	float fScale = 1.0f;

	int numThings = (int)m_selectedThings.size();
	Vec3 pos(0,0,0);
	for (int i = 0; i < numThings; i++)
	{
		pos += m_selectedThings[i]->pos;
	}
	if (numThings > 0)
		pos = pos / numThings;

	if (numThings == 1)
	{
		fAngle = (m_selectedThings[0]->angle/255.0f)*g_PI;
		fScale = m_selectedThings[0]->scale;
	}

	if (bShowManipulator)
	{
		ITransformManipulator *pManip = GetIEditor()->ShowTransformManipulator(true);
		if (pManip)
		{
			Matrix34 tm = Matrix34::Create( Vec3(fScale,fScale,fScale),Quat(fAngle,Vec3(0,0,1)).GetNormalized(),pos );

			pManip->SetTransformation( COORDS_LOCAL,tm );
			pManip->SetTransformation( COORDS_PARENT,tm );
			pManip->SetTransformation( COORDS_USERDEFINED,tm );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CVegetationInstance* CVegetationTool::SelectThingAtPoint( CViewport *view,CPoint point,bool bNotSelect,bool bShowManipulator )
{
	Vec3 raySrc,rayDir;
	view->ViewToWorldRay( point,raySrc,rayDir );
	
	bool bCollideTerrain=false;
	Vec3 pos = view->ViewToWorld( point,&bCollideTerrain,true );
	float fTerrainHitDistance = raySrc.GetDistance(pos);

	IPhysicalWorld *pPhysics = GetIEditor()->GetSystem()->GetIPhysicalWorld();
	if (pPhysics)
	{
		int objTypes = ent_static;
		int flags = rwi_stop_at_pierceable|rwi_ignore_terrain_holes;
		//flags = 31;
		ray_hit hit;
		int col = pPhysics->RayWorldIntersection( raySrc,rayDir*1000.0f,objTypes,flags,&hit,1 );
		if (hit.dist > 0 && !hit.bTerrain && hit.dist < fTerrainHitDistance)
		{
			pe_status_pos statusPos;
			hit.pCollider->GetStatus( &statusPos );
			if (!statusPos.pos.IsZero(0.1f))
				pos = statusPos.pos;
		}
	}

	// Find closest thing to this point.
	CVegetationInstance *obj = m_vegetationMap->GetNearestInstance( pos,1.0f );
	if (obj)
	{
		if (!bNotSelect)
			SelectThing(obj,true,bShowManipulator);
		return obj;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::MoveSelected( CViewport *view,const Vec3 &offset,bool bFollowTerrain )
{
	if (m_selectedThings.empty())
		return;
	Vec3 avepos(0,0,0);
	BBox box;
	box.Reset();
	Vec3 newPos,oldPos;
	for (int i = 0; i < m_selectedThings.size(); i++)
	{
		oldPos = m_selectedThings[i]->pos;
		newPos = oldPos + offset;
		newPos = view->SnapToGrid(newPos);
		//if (bFollowTerrain)
		{
			// Make sure object keeps it height.
			//float height = oldPos.z - GetIEditor()->GetTerrainElevation( oldPos.x,oldPos.y );
			//newPos.z = GetIEditor()->GetTerrainElevation( newPos.x,newPos.y ) + height;
		}
		// Always stick to terrain.
    if(I3DEngine *engine = GetIEditor()->GetSystem()->GetI3DEngine())
      newPos.z = engine->GetTerrainElevation( newPos.x, newPos.y, m_selectedThings[i]->object->IsAffectedByVoxels() );

		m_vegetationMap->MoveInstance( m_selectedThings[i],newPos );
		box.Add( newPos );
		avepos += newPos;
	}
	if(!m_selectedThings.empty())
		avepos = avepos / m_selectedThings.size();

	ITransformManipulator *pManipulator = GetIEditor()->GetTransformManipulator();
	if (pManipulator)
	{
		Matrix34 tm = Matrix34::CreateTranslationMat(avepos);
		pManipulator->SetTransformation( COORDS_LOCAL,tm );
		pManipulator->SetTransformation( COORDS_PARENT,tm );
		pManipulator->SetTransformation( COORDS_USERDEFINED,tm );
	}

	box.min = box.min - Vec3(1,1,1);
	box.max = box.max + Vec3(1,1,1);
	GetIEditor()->UpdateViews( eUpdateStatObj,&box );
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::ScaleSelected( float fScale )
{
	if (fScale <= 0.01f)
		return;

	if (!m_selectedThings.empty())
	{
		int numThings = m_selectedThings.size();
		for (int i = 0; i < numThings; i++)
		{
			m_vegetationMap->RecordUndo( m_selectedThings[i] );
			m_selectedThings[i]->scale *= fScale;

			// Force this object to be placed on terrain again.
			m_vegetationMap->MoveInstance(m_selectedThings[i],m_selectedThings[i]->pos);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::RotateSelected( float fAngle )
{
	if (!m_selectedThings.empty())
	{
		int numThings = m_selectedThings.size();
		for (int i = 0; i < numThings; i++)
		{
			m_vegetationMap->RecordUndo( m_selectedThings[i] );
			m_selectedThings[i]->angle += (fAngle/360.0f)*255.0f;

			// Force this object to be placed on terrain again.
			m_vegetationMap->MoveInstance(m_selectedThings[i],m_selectedThings[i]->pos);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::ScaleObjects()
{
	float fScale = 1;
	if (m_selectedThings.size() == 1)
	{
		fScale = m_selectedThings[0]->scale;
	}

	CNumberDlg dlg( AfxGetMainWnd(),fScale,"Scale Selected Object(s)" );
	if (dlg.DoModal() != IDOK)
		return;

	fScale = dlg.GetValue();
	if (fScale <= 0)
		return;

	if (!m_selectedThings.empty())
	{
		int numThings = m_selectedThings.size();
		for (int i = 0; i < numThings; i++)
		{
			m_vegetationMap->RecordUndo( m_selectedThings[i] );
			if (numThings > 1)
				m_selectedThings[i]->scale *= fScale;
			else
				m_selectedThings[i]->scale = fScale;

			// Force this object to be placed on terrain again.
			m_vegetationMap->MoveInstance(m_selectedThings[i],m_selectedThings[i]->pos);
		}
	}
	else
	{
		GetSelectedObjects( m_selectedObjects );
		for (int i = 0; i < m_selectedObjects.size(); i++)
		{
			m_vegetationMap->ScaleObjectInstances( m_selectedObjects[i],fScale );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::SelectThingsInRect( CViewport *view,CRect rect,bool bShowManipulator )
{
	BBox box;
	std::vector<CVegetationInstance*> things;
	m_vegetationMap->GetAllInstances( things );
	if (things.size() > 0)
	{
		if (m_panel)
		{
			m_panel->UnselectAll();
		}
	}
	for (int i = 0; i < (int)things.size(); i++)
	{
		Vec3 pos = things[i]->pos;
		box.min.Set( pos.x-0.1f,pos.y-0.1f,pos.z-0.1f );
		box.max.Set( pos.x+0.1f,pos.y+0.1f,pos.z+0.1f );
		if (!view->IsBoundsVisible(box))
			continue;
		CPoint pnt = view->WorldToView( things[i]->pos );
		if (rect.PtInRect(pnt))
		{
			SelectThing( things[i],true,bShowManipulator );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::Command_Activate()
{
	CEditTool *pTool = GetIEditor()->GetEditTool();
	if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CVegetationTool)))
	{
		// Already active.
		return;
	}
	pTool = new CVegetationTool;
	GetIEditor()->SetEditTool( pTool );
	GetIEditor()->SelectRollUpBar( ROLLUP_TERRAIN );
	AfxGetMainWnd()->RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);
}

//////////////////////////////////////////////////////////////////////////
// Class description.
//////////////////////////////////////////////////////////////////////////
class CVegetationTool_ClassDesc : public CRefCountClassDesc
{
	//! This method returns an Editor defined GUID describing the class this plugin class is associated with.
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_EDITTOOL; }

	//! Return the GUID of the class created by plugin.
	virtual REFGUID ClassID() 
	{
		return VEGETATION_TOOL_GUID;
	}

	//! This method returns the human readable name of the class.
	virtual const char* ClassName() { return "EditTool.VegetationPaint"; };

	//! This method returns Category of this class, Category is specifing where this plugin class fits best in
	//! create panel.
	virtual const char* Category() { return "Terrain"; };

	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CVegetationTool); }
};

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::RegisterTool( CRegistrationContext &rc )
{
	rc.pClassFactory->RegisterClass( new CVegetationTool_ClassDesc );

	rc.pCommandManager->RegisterCommand( "EditTool.VegetationTool.Activate",functor(CVegetationTool::Command_Activate) );
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::OnManipulatorDrag( CViewport *view,ITransformManipulator *pManipulator,CPoint &point0,CPoint &point1,const Vec3 &value )
{
	// get world/local coordinate system setting.
	RefCoordSys coordSys = GetIEditor()->GetReferenceCoordSys();
	int editMode = GetIEditor()->GetEditMode();

	// get current axis constrains.
	if (editMode == eEditModeMove)
	{
		GetIEditor()->RestoreUndo();

		Vec3 v = value;
		v.z = 0;
		MoveSelected( view,v,true );
	}
	if (editMode == eEditModeRotate)
	{
		GetIEditor()->RestoreUndo();

		RotateSelected( RAD2DEG(value.z) );
	}
	if (editMode == eEditModeScale)
	{
		GetIEditor()->RestoreUndo();
		
		float fScale = max(value.x,value.y);
		fScale = max(fScale,value.z);
		ScaleSelected( fScale );
	}
}

//////////////////////////////////////////////////////////////////////////
bool CVegetationTool::IsNeedMoveTool()
{
	return GetIEditor()->GetTransformManipulator() != NULL;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationTool::MoveSelectedInstancesToCategory(CString category)
{
	int numThings = m_selectedThings.size();
	for (int i = 0; i < numThings; i++)
	{
		CVegetationObject * object = m_selectedThings[i]->object;
		if(strcmp(object->GetCategory(), category))
		{
			CVegetationObject * newObject = m_vegetationMap->CreateObject(object);
			if(newObject)
			{
				newObject->SetCategory(category);
				for (int j = i; j < numThings; j++)
				{
					if(m_selectedThings[j]->object==object)
					{
						object->SetNumInstances(object->GetNumInstances()-1);
						m_selectedThings[j]->object = newObject;
						newObject->SetNumInstances(newObject->GetNumInstances()+1);
					}
				}
			}
		}
	}
}