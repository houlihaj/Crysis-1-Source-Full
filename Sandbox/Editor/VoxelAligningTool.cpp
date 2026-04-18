////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   VoxelAligningTool.cpp
//  Version:     v1.00
//  Created:     25/08/2005 by Sergiy Shaykin.
//  Compilers:   Visual C++ 6.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "VoxelAligningTool.h"
#include "ObjectTypeBrowser.h"
#include "PanelTreeBrowser.h"
#include "Viewport.h"
#include "Objects\ObjectManager.h"

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CVoxelAligningTool,CEditTool)

//////////////////////////////////////////////////////////////////////////
CVoxelAligningTool::CVoxelAligningTool()
{
	m_bIsDragging = false;
	m_curObj = 0;

	CSelectionGroup *sel = GetIEditor()->GetSelection();
	if (!sel->IsEmpty())
		m_curObj = sel->GetObject(0);
}

//////////////////////////////////////////////////////////////////////////
CVoxelAligningTool::~CVoxelAligningTool()
{
}

//////////////////////////////////////////////////////////////////////////
void CVoxelAligningTool::Display( DisplayContext &dc )
{
}

//////////////////////////////////////////////////////////////////////////
bool CVoxelAligningTool::MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	CSelectionGroup *sel = GetIEditor()->GetSelection();
	if (sel->IsEmpty() || m_curObj != sel->GetObject(0))
	{
		GetIEditor()->SetEditTool(0);
		return true;
	}

	if (event == eMouseLDown)
	{
		GetIEditor()->BeginUndo();
		m_bIsDragging = true;

		Vec3 pos = m_curObj->GetPos();
		CPoint pt = GetIEditor()->GetActiveView()->WorldToView(pos);
		m_difPoint = pt-point;
		m_q = m_curObj->GetRotation();
	}

	if (event == eMouseLDown || event == eMouseMove)
	{
		if(!m_bIsDragging)
			return true;

		CPoint pt = point + m_difPoint;
		Vec3 pos = GetIEditor()->GetActiveView()->ViewToWorld(pt, 0, true);
		m_curObj->SetPos(pos);
		Vec3 n = GetIEditor()->GetActiveView()->ViewToWorldNormal(pt, true);
		Vec3 z = m_q * Vec3(0,0,1);
		n.Normalize();
		z.Normalize();
		Quat nq;
		nq.SetRotationV0V1(z, n);
		m_curObj->SetRotation(nq * m_q);
	}

	if (event == eMouseLUp)
	{
		GetIEditor()->AcceptUndo( "Surface Normal Aligning" );
		m_bIsDragging = false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CVoxelAligningTool::BeginEditParams( IEditor *ie,int flags )
{
}

//////////////////////////////////////////////////////////////////////////
void CVoxelAligningTool::EndEditParams()
{
}

//////////////////////////////////////////////////////////////////////////
bool CVoxelAligningTool::OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{ 
	if (nChar == VK_ESCAPE)
	{
		GetIEditor()->SetEditTool(0);
	}
	return false; 
}