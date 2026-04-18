////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   ObjectMode.cpp
//  Version:     v1.00
//  Created:     18/11/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <InitGuid.h>
#include "ObjectMode.h"
#include "Viewport.h"
#include "ViewManager.h"
#include "Heightmap.h"
#include "GameEngine.h"
#include "Objects\ObjectManager.h"
#include "Objects\Entity.h"
#include <HyperGraph/FlowGraphManager.h>
#include <HyperGraph/FlowGraph.h>
#include <HyperGraph/FlowGraphHelpers.h>
#include <HyperGraph/HyperGraphDialog.h>
#include <HyperGraph/FlowGraphSearchCtrl.h>

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CObjectMode,CEditTool)

//////////////////////////////////////////////////////////////////////////
CObjectMode::CObjectMode()
{
	m_pClassDesc = GetIEditor()->GetClassFactory()->FindClass(OBJECT_MODE_GUID);
	SetStatusText( _T("Object Selection") );

	m_openContext = false;
	m_commandMode = NothingMode;
	m_MouseOverObject = GuidUtil::NullGuid;
}

//////////////////////////////////////////////////////////////////////////
void CObjectMode::Display( struct DisplayContext &dc )
{
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	switch (event) {
	case eMouseLDown:
		return OnLButtonDown(view,flags,point);
		break;
	case eMouseLUp:
		return OnLButtonUp(view,flags,point);
		break;
	case eMouseLDblClick:
		return OnLButtonDblClk(view,flags,point);
		break;
	case eMouseRDown:
		return OnRButtonDown(view,flags,point);
		break;
	case eMouseRUp:
		return OnRButtonUp(view,flags,point);
		break;
	case eMouseMove:
		return OnMouseMove(view,flags,point);
		break;
	case eMouseMDown:
		return OnMButtonDown(view,flags,point);
		break;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{
	if (nChar == VK_ESCAPE)
	{
		GetIEditor()->ClearSelection();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnKeyUp( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnLButtonDown( CViewport *view,int nFlags, CPoint point) 
{
	// CPointF ptMarker;
	CPoint ptCoord;
	int iCurSel = -1;

	if (GetIEditor()->IsInGameMode())
	{
		// Ignore clicks while in game.
		return false;
	}

	// Save the mouse down position
	m_cMouseDownPos = point;

	view->ResetSelectionRegion();

	Vec3 pos = view->SnapToGrid( view->ViewToWorld( point ) );

	// Show marker position in the status bar
	//sprintf(szNewStatusText, "X:%g Y:%g Z:%g",pos.x,pos.y,pos.z );

	// Swap X/Y
	int unitSize = 1;
	CHeightmap *pHeightmap = GetIEditor()->GetHeightmap();
	if (pHeightmap)
		unitSize = pHeightmap->GetUnitSize();
	float hx = pos.y / unitSize;
	float hy = pos.x / unitSize;
	float hz = GetIEditor()->GetTerrainElevation(pos.x,pos.y);

	char szNewStatusText[512];
	sprintf(szNewStatusText, "Heightmap Coordinates: HX:%g HY:%g HZ:%g",hx,hy,hz );
	GetIEditor()->SetStatusText(szNewStatusText);

	// Get contrl key status.
	bool bAltClick = CheckVirtualKey(VK_MENU);
	bool bCtrlClick = (nFlags & MK_CONTROL);
	bool bShiftClick = (nFlags & MK_SHIFT);

	bool bAddSelect = bCtrlClick;
	bool bUnselect = bAltClick;
	bool bNoRemoveSelection = bAddSelect || bUnselect;

	bool bLockSelection = GetIEditor()->IsSelectionLocked();

	int numUnselected = 0;
	int numSelected = 0;

	//	m_activeAxis = 0;

	HitContext hitInfo;
	hitInfo.view = view;
	if (bAddSelect || bUnselect)
	{
		// If adding or removing selection from the object, ignore hitting selection axis.
		hitInfo.bIgnoreAxis = true;
	}
	if (view->HitTest( point,hitInfo ))
	{
		if (hitInfo.axis != 0)
		{
			GetIEditor()->SetAxisConstrains( (AxisConstrains)hitInfo.axis );
			bLockSelection = true;
		}
		if (hitInfo.axis != 0)
			view->SetAxisConstrain( hitInfo.axis );
	}
	CBaseObject *hitObj = hitInfo.object;

	int editMode = GetIEditor()->GetEditMode();

	Matrix34 userTM = GetIEditor()->GetViewManager()->GetGrid()->GetOrientationMatrix();

	if (hitObj)
	{
		Matrix34 tm = hitInfo.object->GetWorldTM();
		tm.OrthonormalizeFast();
		view->SetConstructionMatrix( COORDS_LOCAL,tm );
		if (hitInfo.object->GetParent())
		{
			Matrix34 parentTM = hitInfo.object->GetParent()->GetWorldTM();
			parentTM.OrthonormalizeFast();
			parentTM.SetTranslation( tm.GetTranslation() );
			view->SetConstructionMatrix( COORDS_PARENT,parentTM );
		}
		else
		{
			Matrix34 parentTM;
			parentTM.SetIdentity();
			parentTM.SetTranslation( tm.GetTranslation() );
			view->SetConstructionMatrix( COORDS_PARENT,parentTM );
		}
		userTM.SetTranslation( tm.GetTranslation() );
		view->SetConstructionMatrix( COORDS_USERDEFINED,userTM );
	}
	else
	{
		Matrix34 tm;
		tm.SetIdentity();
		tm.SetTranslation( pos );
		userTM.SetTranslation( pos );
		view->SetConstructionMatrix( COORDS_LOCAL,tm );
		view->SetConstructionMatrix( COORDS_PARENT,tm );
		view->SetConstructionMatrix( COORDS_USERDEFINED,userTM );
	}

	if (editMode == eEditModeMove)
	{
		if (!bNoRemoveSelection)
			SetCommandMode( MoveMode );
		// Check for Move to position.
		if (bCtrlClick && bShiftClick)
		{
			// Ctrl-Click on terain will move selected objects to specified location.
			MoveSelectionToPos( view,pos );
			bLockSelection = true;
		}

		if (hitObj && hitObj->IsSelected() && !bNoRemoveSelection)
			bLockSelection = true;
	}
	else  if (editMode == eEditModeRotate)
	{
		if (!bNoRemoveSelection)
			SetCommandMode( RotateMode );
		if (hitObj && hitObj->IsSelected() && !bNoRemoveSelection)
			bLockSelection = true;
	}
	else if (editMode == eEditModeScale)
	{
		if (!bNoRemoveSelection)
			SetCommandMode( ScaleMode );
		if (hitObj && hitObj->IsSelected() && !bNoRemoveSelection)
			bLockSelection = true;
	}
	else if (hitObj != 0 && GetIEditor()->GetSelectedObject() == hitObj && !bAddSelect && !bUnselect)
	{
		bLockSelection = true;
	}

	if (!bLockSelection)
	{
		// If not selection locked.
		view->BeginUndo();

		if (!bNoRemoveSelection)
		{
			// Current selection should be cleared
			numUnselected = GetIEditor()->GetObjectManager()->ClearSelection();
		}

		if (hitObj)
		{
			numSelected = 1;

			if (!bUnselect)
			{
				if (hitObj->IsSelected())
					bUnselect = true;
			}

			if (!bUnselect)
				GetIEditor()->GetObjectManager()->SelectObject( hitObj );
			else
				GetIEditor()->GetObjectManager()->UnselectObject( hitObj );
		}
		if (view->IsUndoRecording())
			view->AcceptUndo( "Select Object(s)" );

		if (numSelected == 0 || editMode == eEditModeSelect)
		{
			// If object is not selected.
			// Capture mouse input for this window.
			SetCommandMode( SelectMode );
		}
	}

	if (GetCommandMode() == MoveMode ||
		GetCommandMode() == RotateMode ||
		GetCommandMode() == ScaleMode)
	{
		view->BeginUndo();
	}

	//////////////////////////////////////////////////////////////////////////
	// Change cursor, must be before Capture mouse.
	//////////////////////////////////////////////////////////////////////////
	SetObjectCursor(view,hitObj,true);

	//////////////////////////////////////////////////////////////////////////
	view->CaptureMouse();
	//////////////////////////////////////////////////////////////////////////

	UpdateStatusText();

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnLButtonUp( CViewport *view,int nFlags, CPoint point) 
{
	if (GetIEditor()->IsInGameMode())
	{
		// Ignore clicks while in game.
		return true;
	}

	// Reset the status bar caption
	GetIEditor()->SetStatusText("Ready");

	//////////////////////////////////////////////////////////////////////////
	if (view->IsUndoRecording())
	{
		if (GetCommandMode() == MoveMode)
		{
			view->AcceptUndo( "Move Selection" );
		}
		else if (GetCommandMode() == RotateMode)
		{
			view->AcceptUndo( "Rotate Selection" );
		}
		else if (GetCommandMode() == ScaleMode)
		{
			view->AcceptUndo( "Scale Selection" );
		}
		else
		{
			view->CancelUndo();
		}
	}
	//////////////////////////////////////////////////////////////////////////

	if (GetCommandMode() == SelectMode && (!GetIEditor()->IsSelectionLocked()))
	{
		bool bUnselect = CheckVirtualKey(VK_MENU);
		CRect selectRect = view->GetSelectionRectangle();
		if (!selectRect.IsRectEmpty())
		{
			// Ignore too small rectangles.
			if (selectRect.Width() > 5 && selectRect.Height() > 5)
			{
				GetIEditor()->GetObjectManager()->SelectObjectsInRect( view,selectRect,!bUnselect );
			}
		}

		if (GetIEditor()->GetEditMode() == eEditModeSelectArea)
		{
			BBox box;
			GetIEditor()->GetSelectedRegion( box );

			//////////////////////////////////////////////////////////////////////////
			GetIEditor()->ClearSelection();

			SEntityProximityQuery query;
			query.box = box;
			gEnv->pEntitySystem->QueryProximity( query );
			for (int i = 0; i < query.nCount; i++)
			{
				IEntity *pIEntity = query.pEntities[i];
				CEntity *pEntity = CEntity::FindFromEntityId( pIEntity->GetId() );
				if (pEntity)
				{
					GetIEditor()->GetObjectManager()->SelectObject( pEntity );
				}
			}
			//////////////////////////////////////////////////////////////////////////
			/*

			if (fabs(box.min.x-box.max.x) > 0.5f && fabs(box.min.y-box.max.y) > 0.5f)
			{
				//@FIXME: restore it later.
				//Timur[1/14/2003]
				//SelectRectangle( box,!bUnselect );
				//SelectObjectsInRect( m_selectedRect,!bUnselect );
				GetIEditor()->GetObjectManager()->SelectObjects( box,bUnselect );
				GetIEditor()->UpdateViews(eUpdateObjects);
			}
			*/
		}
	}
	// Release the restriction of the cursor
	view->ReleaseMouse();

	if (GetIEditor()->GetEditMode() != eEditModeSelectArea)
	{
		view->ResetSelectionRegion();
	}
	// Reset selected rectangle.
	view->SetSelectionRectangle( CPoint(0,0),CPoint(0,0) );

	// Restore default editor axis constrain.
	if (GetIEditor()->GetAxisConstrains() != view->GetAxisConstrain())
	{
		view->SetAxisConstrain( GetIEditor()->GetAxisConstrains() );
	}

	SetCommandMode( NothingMode );

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnLButtonDblClk( CViewport *view,int nFlags, CPoint point)
{
	// If shift clicked, Move the camera to this place.
	if (nFlags & MK_SHIFT)
	{
		// Get the heightmap coordinates for the click position
		Vec3 v = view->ViewToWorld( point );
		if (!(v.x == 0 && v.y == 0 && v.z == 0))
		{
			Matrix34 tm = view->GetViewTM();
			Vec3 p = tm.GetTranslation();
			float height = p.z - GetIEditor()->GetTerrainElevation(p.x,p.y);
			if (height < 1) height = 1;
			p.x = v.x;
			p.y = v.y;
			p.z = GetIEditor()->GetTerrainElevation( p.x,p.y ) + height;
			tm.SetTranslation(p);
			view->SetViewTM(tm);
		}
	}
	else
	{
		// Check if double clicked on object.
		HitContext hitInfo;
		view->HitTest( point,hitInfo );

		CBaseObject *hitObj = hitInfo.object;
		if (hitObj)
		{
			// Fire double click event on hitted object.
			hitObj->OnEvent( EVENT_DBLCLICK );
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnRButtonDown( CViewport *view,int nFlags, CPoint point) 
{
	// Save the mouse down position
	m_openContext = true;
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnRButtonUp( CViewport *view,int nFlags, CPoint point) 
{
	if (m_openContext)
	{
		// Check if double clicked on object.
		HitContext hitInfo;
		view->HitTest( point,hitInfo );

		CBaseObject *hitObj = hitInfo.object;
		if (hitObj && hitObj->IsKindOf(RUNTIME_CLASS(CEntity)))
		{
			CEntity *pEntity = (CEntity*)hitObj;

			std::vector<CFlowGraph*> flowgraphs;
			CFlowGraph* entityFG = 0;
			FlowGraphHelpers::FindGraphsForEntity(pEntity, flowgraphs, entityFG);

			unsigned int id=1;
			CMenu menu;
			menu.CreatePopupMenu();

			if (flowgraphs.size() > 0)
			{
				CMenu fgMenu;
				fgMenu.CreatePopupMenu();

				std::vector<CFlowGraph*>::const_iterator iter (flowgraphs.begin());
				while (iter != flowgraphs.end()) 
				{
					CString name;
					FlowGraphHelpers::GetHumanName(*iter, name);
					if (*iter == entityFG) {
						name+=" <GraphEntity>";
						fgMenu.AppendMenu(MF_STRING, id, name);
						if (flowgraphs.size() > 1) fgMenu.AppendMenu(MF_SEPARATOR);
					} else {
						fgMenu.AppendMenu(MF_STRING, id, name);
					}
					++id;
					++iter;
				}
				menu.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(fgMenu.GetSafeHmenu()), "Flow Graphs");
				menu.AppendMenu(MF_SEPARATOR);
			}

			// events
			int baseEventId = id;
			CEntityScript *pScript = pEntity->GetScript();

			if (pScript && pScript->GetEventCount()>0)
			{
				CMenu eventMenu;
				eventMenu.CreatePopupMenu();
				for (int i=0; i<pScript->GetEventCount(); ++i)
				{
					CString sourceEvent = pScript->GetEvent(i);
					eventMenu.AppendMenu(MF_STRING, id++, sourceEvent);
				}
				
				menu.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(eventMenu.GetSafeHmenu()), "Events");
				menu.AppendMenu(MF_SEPARATOR);
			}

			// add reload script
			int reloadScriptId = id;
			menu.AppendMenu(MF_STRING, reloadScriptId, "Reload Script");
			++id;

			CPoint p;
			::GetCursorPos(&p);
			int chosen = menu.TrackPopupMenuEx( TPM_RETURNCMD|TPM_LEFTBUTTON|TPM_TOPALIGN|TPM_LEFTALIGN, p.x, p.y, view, NULL );
			if (chosen > 0)
			{
				if (chosen == reloadScriptId)
				{
					CEntityScript *script = pEntity->GetScript();
					script->Reload();
					pEntity->Reload(true);
				}
				else if (chosen <= flowgraphs.size())
				{
					GetIEditor()->GetFlowGraphManager()->OpenView( flowgraphs[chosen-1] );
					CWnd* pWnd = GetIEditor()->FindView( "Flow Graph" );
					if ( pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CHyperGraphDialog)) )
					{
						CHyperGraphDialog* pHGDlg = (CHyperGraphDialog*) pWnd;
						CFlowGraphSearchCtrl* pSC = pHGDlg->GetSearchControl();
						if (pSC)
						{
							CFlowGraphSearchOptions* pOpts = CFlowGraphSearchOptions::GetSearchOptions();
							pOpts->m_bIncludeEntities = true;
							pOpts->m_findSpecial = CFlowGraphSearchOptions::eFLS_None;
							pOpts->m_LookinIndex = CFlowGraphSearchOptions::eFL_Current;
							pSC->Find(pEntity->GetName(), false, true, true);
						}
					}
				}
				else
				{
					CEntityScript *pScript = pEntity->GetScript();
					if (pScript && pEntity->GetIEntity())
						pScript->SendEvent( pEntity->GetIEntity(), pScript->GetEvent(chosen - baseEventId));
				}
			}
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnMButtonDown( CViewport *view,int nFlags, CPoint point) 
{
	if (GetIEditor()->GetGameEngine()->GetSimulationMode())
	{
		// Get control key status.
		bool bAltClick = CheckVirtualKey(VK_MENU);
		bool bCtrlClick = CheckVirtualKey(VK_CONTROL);
		bool bShiftClick = CheckVirtualKey(VK_SHIFT);

		if (bCtrlClick)
		{
			// In simulation mode awake objects under the cursor when Ctrl+MButton pressed.
			AwakeObjectAtPoint(view,point);
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CObjectMode::AwakeObjectAtPoint( CViewport *view,CPoint point )
{
	// In simulation mode awake objects under the cursor.
	// Check if double clicked on object.
	HitContext hitInfo;
	view->HitTest( point,hitInfo );
	CBaseObject *hitObj = hitInfo.object;
	if (hitObj)
	{
		IPhysicalEntity *pent = hitObj->GetCollisionEntity();
		if (pent)
		{
			pe_action_awake pa;
			pa.bAwake = true;
			pent->Action(&pa);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::CheckVirtualKey( int virtualKey )
{
	GetAsyncKeyState(virtualKey);
	if (GetAsyncKeyState(virtualKey))
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CObjectMode::MoveSelectionToPos( CViewport *view,Vec3 &pos )
{
	view->BeginUndo();
	// Find center of selection.
	Vec3 center = GetIEditor()->GetSelection()->GetCenter();
	GetIEditor()->GetSelection()->Move( pos-center,false,true );
	view->AcceptUndo( "Move Selection" );
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnMouseMove( CViewport *view,int nFlags, CPoint point)
{
	if (GetIEditor()->IsInGameMode())
	{
		// Ignore while in game.
		return true;
	}

	m_openContext = false;
	SetObjectCursor(view,0);

	Vec3 pos = view->SnapToGrid( view->ViewToWorld( point ) );

	// get world/local coordinate system setting.
	int coordSys = GetIEditor()->GetReferenceCoordSys();

	// get current axis constrains.
	if (GetCommandMode() == MoveMode)
	{
		GetIEditor()->RestoreUndo();

		Vec3 v;
		//m_cMouseDownPos = point;
		bool followTerrain = false;
		if (view->GetAxisConstrain() == AXIS_TERRAIN)
		{
			followTerrain = true;
			Vec3 p1 = view->SnapToGrid(view->ViewToWorld( m_cMouseDownPos ));
			Vec3 p2 = view->SnapToGrid(view->ViewToWorld( point ));
			v = p2 - p1;
			v.z = 0;
		}
		else
		{
			Vec3 p1 = view->MapViewToCP(m_cMouseDownPos);
			Vec3 p2 = view->MapViewToCP(point);
			if (p1.IsZero() || p2.IsZero())
				return true;
			v = view->GetCPVector(p1,p2);

			//Matrix invParent = m_parentConstructionMatrix;
			//invParent.Invert();
			//p1 = invParent.TransformVector(p1);
			//p2 = invParent.TransformVector(p2);
			//v = p2 - p1;
		}

		//if (GetCommandModifier() == ContentMode)
		//	GetIEditor()->GetSelection()->MoveContent( v);
		//else
			GetIEditor()->GetSelection()->Move( v,followTerrain,coordSys );

		return true;
	}
	else if (GetCommandMode() == RotateMode)
	{
		GetIEditor()->RestoreUndo();

		Ang3 ang(0,0,0);
		float ax = point.x - m_cMouseDownPos.x;
		float ay = point.y - m_cMouseDownPos.y;
		switch (view->GetAxisConstrain())
		{
		case AXIS_X: ang.x = ay; break;
		case AXIS_Y: ang.y = ay; break;
		case AXIS_Z: ang.z = ay; break;
		case AXIS_XY: ang(ax,ay,0); break;
		case AXIS_XZ: ang(ax,0,ay); break;
		case AXIS_YZ: ang(0,ay,ax); break;
		case AXIS_TERRAIN: ang(ax,ay,0); break;
		};

		ang = view->GetViewManager()->GetGrid()->SnapAngle(ang);

		//m_cMouseDownPos = point;
		GetIEditor()->GetSelection()->Rotate( ang,coordSys );
		return true;
	}
	else if (GetCommandMode() == ScaleMode)
	{
		GetIEditor()->RestoreUndo();

		Vec3 scl(0,0,0);
		float ay = 1.0f - 0.01f*(point.y - m_cMouseDownPos.y);
		if (ay < 0.01f) ay = 0.01f;
		scl(ay,ay,ay);
		switch (view->GetAxisConstrain())
		{
		case AXIS_X: scl(ay,1,1); break;
		case AXIS_Y: scl(1,ay,1); break;
		case AXIS_Z: scl(1,1,ay); break;
		case AXIS_XY: scl(ay,ay,ay); break;
		case AXIS_XZ: scl(ay,ay,ay); break;
		case AXIS_YZ: scl(ay,ay,ay); break;
		case AXIS_XYZ: scl(ay,ay,ay); break;
		case AXIS_TERRAIN: scl(ay,ay,ay); break;
		};
		GetIEditor()->GetSelection()->Scale( scl,coordSys );
		return true;
	}
	else if (GetCommandMode() == SelectMode)
	{
		// Ignore select when selection locked.
		if (GetIEditor()->IsSelectionLocked())
			return true;

		CRect rc( m_cMouseDownPos,point );
		if (GetIEditor()->GetEditMode() == eEditModeSelectArea)
			view->OnDragSelectRectangle( CPoint(rc.left,rc.top),CPoint(rc.right,rc.bottom),false );
		else
		{
			view->SetSelectionRectangle( rc.TopLeft(),rc.BottomRight() );
		}
		//else
		//OnDragSelectRectangle( CPoint(rc.left,rc.top),CPoint(rc.right,rc.bottom),true );
	}

	if (!(nFlags & MK_RBUTTON))
	{
		// Track mouse movements.
		HitContext hitInfo;
		if (view->HitTest( point,hitInfo ))
		{
			SetObjectCursor(view,hitInfo.object);
		}
	}

	if ((nFlags & MK_MBUTTON) && GetIEditor()->GetGameEngine()->GetSimulationMode())
	{
		// Get control key status.
		bool bAltClick = CheckVirtualKey(VK_MENU);
		bool bCtrlClick = CheckVirtualKey(VK_CONTROL);
		bool bShiftClick = CheckVirtualKey(VK_SHIFT);
		if (bCtrlClick)
		{
			// In simulation mode awake objects under the cursor when Ctrl+MButton pressed.
			AwakeObjectAtPoint(view,point);
		}
	}

	UpdateStatusText();
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CObjectMode::SetObjectCursor( CViewport *view,CBaseObject *hitObj,bool bChangeNow )
{
	EStdCursor cursor = STD_CURSOR_DEFAULT;
	CString m_cursorStr;

	CBaseObject *pMouseOverObject = NULL;
	if (!GuidUtil::IsEmpty(m_MouseOverObject))
		pMouseOverObject = GetIEditor()->GetObjectManager()->FindObject( m_MouseOverObject );

	//HCURSOR hPrevCursor = m_hCurrCursor;
	if (pMouseOverObject)
	{
		pMouseOverObject->SetHighlight(false);
	}
	if (hitObj)
		m_MouseOverObject = hitObj->GetId();
	else
		m_MouseOverObject = GUID_NULL;
	pMouseOverObject = hitObj;
	bool bHitSelectedObject = false;
	if (pMouseOverObject)
	{
		if (GetCommandMode() != SelectMode)
		{
			pMouseOverObject->SetHighlight(true);
			m_cursorStr = pMouseOverObject->GetName();
			cursor = STD_CURSOR_HIT;
			if (pMouseOverObject->IsSelected())
				bHitSelectedObject = true;
		}
	}
	else
	{
		m_cursorStr = "";
		cursor = STD_CURSOR_DEFAULT;
	}
	// Get control key status.
	bool bAltClick = CheckVirtualKey(VK_MENU);
	bool bCtrlClick = CheckVirtualKey(VK_CONTROL);
	bool bShiftClick = CheckVirtualKey(VK_SHIFT);
	
	bool bAddSelect = bCtrlClick;
	bool bUnselect = bAltClick;
	bool bNoRemoveSelection = bAddSelect || bUnselect;

	bool bLockSelection = GetIEditor()->IsSelectionLocked();

	if (GetCommandMode() == SelectMode || GetCommandMode() == NothingMode)
	{
		if (bAddSelect)
			cursor = STD_CURSOR_SEL_PLUS;
		if (bUnselect)
			cursor = STD_CURSOR_SEL_MINUS;

		if ((bHitSelectedObject && !bNoRemoveSelection) || bLockSelection)
		{
			int editMode = GetIEditor()->GetEditMode();
			if (editMode == eEditModeMove)
			{
				cursor = STD_CURSOR_MOVE;
			}
			else if (editMode == eEditModeRotate)
			{
				cursor = STD_CURSOR_ROTATE;
			}
			else if (editMode == eEditModeScale)
			{
				cursor = STD_CURSOR_SCALE;
			}
		}
	}
	else if (GetCommandMode() == MoveMode)
	{
		cursor = STD_CURSOR_MOVE;
	}
	else if (GetCommandMode() == RotateMode)
	{
		cursor = STD_CURSOR_ROTATE;
	}
	else if (GetCommandMode() == ScaleMode)
	{
		cursor = STD_CURSOR_SCALE;
	}

	/*
	if (bChangeNow)
	{
		if (GetCapture() == NULL)
		{
			if (m_hCurrCursor)
				SetCursor( m_hCurrCursor );
			else
				SetCursor( m_hDefaultCursor );
		}
	}
	*/
	view->SetCurrentCursor( cursor,m_cursorStr );
}


//////////////////////////////////////////////////////////////////////////
// Class description.
//////////////////////////////////////////////////////////////////////////
class CObjectMode_ClassDesc : public CRefCountClassDesc
{
	//! This method returns an Editor defined GUID describing the class this plugin class is associated with.
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_EDITTOOL; }

	//! Return the GUID of the class created by plugin.
	virtual REFGUID ClassID() 
	{
		return OBJECT_MODE_GUID;
	}

	//! This method returns the human readable name of the class.
	virtual const char* ClassName() { return "EditTool.ObjectMode"; };

	//! This method returns Category of this class, Category is specifing where this plugin class fits best in
	//! create panel.
	virtual const char* Category() { return "Select"; };
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CObjectMode); }
	//////////////////////////////////////////////////////////////////////////
};


//////////////////////////////////////////////////////////////////////////
void CObjectMode::RegisterTool( CRegistrationContext &rc )
{
	rc.pClassFactory->RegisterClass( new CObjectMode_ClassDesc );
}

//////////////////////////////////////////////////////////////////////////
void CObjectMode::UpdateStatusText()
{
	CString str;
	int nCount = GetIEditor()->GetSelection()->GetCount();
	if (nCount > 0)
	{
		str.Format( "%d Object(s) Selected",nCount );
	}
	else
		str.Format( "No Selection",nCount );
	SetStatusText( str );
}
