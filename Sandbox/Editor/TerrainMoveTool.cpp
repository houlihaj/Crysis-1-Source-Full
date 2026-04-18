////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   TerrainMoveTool.cpp
//  Version:     v1.00
//  Created:     11/1/2002 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: Terrain Modification Tool implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "TerrainPanel.h"
#include "TerrainMoveTool.h"
#include "TerrainMoveToolPanel.h"
#include "Viewport.h"
#include "Heightmap.h"
#include "VegetationMap.h"
#include "TerrainGrid.h"
#include "Objects\ObjectManager.h"

#include "ITransformManipulator.h"

#include "I3DEngine.h"

class CUndoTerrainMoveTool : public IUndoObject
{
public:
	CUndoTerrainMoveTool( CTerrainMoveTool *pMoveTool )
	{
		m_posSourceUndo = pMoveTool->m_source.pos;
		m_posTargetUndo = pMoveTool->m_target.pos;
	}
protected:
	virtual int GetSize() { return sizeof(*this); }
	virtual const char* GetDescription() { return ""; };

	virtual void Undo( bool bUndo )
	{
		CEditTool *pTool = GetIEditor()->GetEditTool();
		if (!pTool || !pTool->IsKindOf(RUNTIME_CLASS(CTerrainMoveTool)))
			return;
		CTerrainMoveTool *pMoveTool = (CTerrainMoveTool*)pTool;
		if (bUndo)
		{
			m_posSourceRedo = pMoveTool->m_source.pos;
			m_posTargetRedo = pMoveTool->m_target.pos;
		}
		pMoveTool->m_source.pos = m_posSourceUndo;
		pMoveTool->m_target.pos = m_posTargetUndo;
		if(pMoveTool->m_source.isSelected)
			pMoveTool->Select(1);
		if(pMoveTool->m_target.isSelected)
			pMoveTool->Select(2);
	}
	virtual void Redo()
	{
		CEditTool *pTool = GetIEditor()->GetEditTool();
		if (!pTool || !pTool->IsKindOf(RUNTIME_CLASS(CTerrainMoveTool)))
			return;
		CTerrainMoveTool *pMoveTool = (CTerrainMoveTool*)pTool;
		pMoveTool->m_source.pos = m_posSourceRedo;
		pMoveTool->m_target.pos = m_posTargetRedo;
		if(pMoveTool->m_source.isSelected)
			pMoveTool->Select(1);
		if(pMoveTool->m_target.isSelected)
			pMoveTool->Select(2);
	}
private:
	Vec3 m_posSourceRedo;
	Vec3 m_posTargetRedo;
	Vec3 m_posSourceUndo;
	Vec3 m_posTargetUndo;
};


//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CTerrainMoveTool,CEditTool)

Vec3 CTerrainMoveTool::m_dym(512,512,1024);
//SMTBox CTerrainMoveTool::m_source;
//SMTBox CTerrainMoveTool::m_target;

//////////////////////////////////////////////////////////////////////////
CTerrainMoveTool::CTerrainMoveTool()
{
	m_pointerPos(0,0,0);
	m_archive = 0;

	m_panelId = 0;
	m_panel = 0;
}

//////////////////////////////////////////////////////////////////////////
CTerrainMoveTool::~CTerrainMoveTool()
{
	m_pointerPos(0,0,0);
	if (m_archive)
		delete m_archive;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveTool::BeginEditParams( IEditor *ie,int flags )
{
	m_ie = ie;

	CUndo undo( "Unselect All" );
	GetIEditor()->ClearSelection();

	if (!m_panelId)
	{
		m_panel = new CTerrainMoveToolPanel(this,AfxGetMainWnd());
		m_panelId = m_ie->AddRollUpPage( ROLLUP_TERRAIN,"Terrain Move Tool",m_panel );
		AfxGetMainWnd()->SetFocus();
	}

	if(m_source.isCreated)
		m_source.isShow = true;
	if(m_target.isCreated)
		m_target.isShow = true;

	//CPoint hmapSrcMin,hmapSrcMax;
	//hmapSrcMin = GetIEditor()->GetHeightmap()->WorldToHmap(srcBox.min);
	//hmapSrcMax = GetIEditor()->GetHeightmap()->WorldToHmap(srcBox.max);
	//m_srcRect.SetRect( hmapSrcMin,hmapSrcMax );
			
	//GetIEditor()->GetHeightmap()->ExportBlock( m_srcRect,*m_archive );
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveTool::EndEditParams()
{
	if (m_panelId)
	{
		m_ie->RemoveRollUpPage(ROLLUP_TERRAIN,m_panelId);
		m_panelId = 0;
		m_panel = 0;
	}
	Select(0);
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainMoveTool::MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	m_pointerPos = view->ViewToWorld( point,0,true );

	if (event == eMouseMove)
	{
		if(m_source.isSelected && !m_source.isCreated)
		{
			m_source.pos = view->ViewToWorld( point,0,true );
			Select(1);
		}
		if(m_target.isSelected && !m_target.isCreated)
		{
			m_target.pos = view->ViewToWorld( point,0,true );
			Select(2);
		}
	}
	else if (event == eMouseLDown)
	{
		// Close tool.
		if(!m_source.isSelected && !m_target.isSelected)
			GetIEditor()->SetEditTool(0);
	}
	else if (event == eMouseLUp)
	{
		if(m_source.isSelected && !m_source.isCreated)
		{
			m_source.isCreated = true;
			m_panel->UpdateButtons();
		}
		else if(m_target.isSelected && !m_target.isCreated)
		{
			m_target.isCreated = true;
			m_panel->UpdateButtons();
		}
		else
			GetIEditor()->AcceptUndo( "Move Box" );
		CorrectPosition();
	}
	else
	{
		
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveTool::Display( DisplayContext &dc )
{
	if(m_source.isShow)
	{
		dc.SetColor( RGB(255,128,128),1 );
		if(m_source.isSelected)
			dc.SetColor(dc.GetSelectedColor());
		dc.DrawWireBox(m_source.pos - m_dym/2, m_source.pos + m_dym/2);
	}

	if(m_target.isShow)
	{
		dc.SetColor( RGB(128,128,255),1 );
		if(m_target.isSelected)
			dc.SetColor(dc.GetSelectedColor());
		dc.DrawWireBox(m_target.pos - m_dym/2, m_target.pos + m_dym/2);
	}

	/*
	BBox box;
	GetIEditor()->GetSelectedRegion(box);

	Vec3 p1 = GetIEditor()->GetHeightmap()->HmapToWorld( CPoint(m_srcRect.left,m_srcRect.top) );
	Vec3 p2 = GetIEditor()->GetHeightmap()->HmapToWorld( CPoint(m_srcRect.right,m_srcRect.bottom) );

	Vec3 ofs = m_pointerPos - p1;
	p1 += ofs;
	p2 += ofs;

	dc.SetColor( RGB(0,0,255) );
	dc.DrawTerrainRect( p1.x,p1.y,p2.x,p2.y,0.2f );
	*/
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainMoveTool::OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{
	bool bProcessed = false;
	return bProcessed;
}

bool CTerrainMoveTool::OnKeyUp( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveTool::Move(bool isCopy, bool bOnlyVegetation, bool bOnlyTerrain)
{
	// Move terrain area.
	CUndo undo( "Copy Area" );
	CWaitCursor wait;

	CHeightmap *pHeightamp = GetIEditor()->GetHeightmap();
	assert( pHeightamp );

	CRect rc;
	//rc.left   = pHeightamp->WorldToHmap(m_source.pos-m_dym/2).x;
	//rc.top    = pHeightamp->WorldToHmap(m_source.pos-m_dym/2).y;
	//rc.right  = pHeightamp->WorldToHmap(m_source.pos+m_dym/2).x;
	//rc.bottom = pHeightamp->WorldToHmap(m_source.pos+m_dym/2).y;


	LONG bx1 = pHeightamp->WorldToHmap(m_source.pos-m_dym/2).x;
	LONG by1 = pHeightamp->WorldToHmap(m_source.pos-m_dym/2).y;
	LONG bx2 = pHeightamp->WorldToHmap(m_source.pos+m_dym/2).x;
	LONG by2 = pHeightamp->WorldToHmap(m_source.pos+m_dym/2).y;

	CPoint hmapPosStart = pHeightamp->WorldToHmap(m_target.pos-m_dym/2) - CPoint(bx1,by1);;

	LONG blocksize = 512;

	Vec3 moveOffset;

	for(LONG by = by1; by<by2; by+=blocksize)
	for(LONG bx = bx1; bx<bx2; bx+=blocksize)
	{
		rc.left	= bx;
		rc.top = by;
		rc.right = min(bx + blocksize+1, bx2);
		rc.bottom = min(by + blocksize+1, by2);

		if (m_archive)
			delete m_archive;
		m_archive = new CXmlArchive("Root");
		// Switch archive to saving.
		m_archive->bLoading = false;

		pHeightamp->ExportBlock( rc, *m_archive, isCopy && (bOnlyVegetation || (!bOnlyVegetation && !bOnlyTerrain)));

		// Switch archive to loading.
		m_archive->bLoading = true;

		// Move terrain heightmap block.
	
		CPoint hmapPos = hmapPosStart + CPoint(bx, by);
		CPoint offset = pHeightamp->ImportBlock( *m_archive, hmapPos, true, (m_target.pos-m_source.pos).z, bOnlyVegetation && !bOnlyTerrain);

		moveOffset = pHeightamp->HmapToWorld(offset);
		moveOffset.z += (m_target.pos-m_source.pos).z;

		if(!bOnlyVegetation || bOnlyTerrain)
		{
			CRGBLayer * pLayer = &pHeightamp->m_TerrainRGBTexture;
			uint32 nRes = pLayer->CalcMaxLocalResolution((float)rc.left/pHeightamp->GetWidth(), (float)rc.top/pHeightamp->GetHeight(), (float)rc.right/pHeightamp->GetWidth(), (float)rc.bottom/pHeightamp->GetHeight());
			float fTerrainSize = (float)GetIEditor()->Get3DEngine()->GetTerrainSize();

			int iImageX = (rc.left+offset.x)*nRes/pHeightamp->GetWidth();
			int iImageY = (rc.top+offset.y)*nRes/pHeightamp->GetHeight();
			{
				CImage image;
				image.Allocate((rc.right-rc.left)*nRes/pHeightamp->GetWidth(), (rc.bottom-rc.top)*nRes/pHeightamp->GetHeight());
				pLayer->GetSubImageStretched((float)rc.left/pHeightamp->GetWidth(), (float)rc.top/pHeightamp->GetHeight(), (float)rc.right/pHeightamp->GetWidth(), (float)rc.bottom/pHeightamp->GetHeight(), image);
				pLayer->SetSubImageStretched(
					((float)rc.left+ offset.x)/pHeightamp->GetWidth(), ((float)rc.top+   offset.y)/pHeightamp->GetHeight(), 
					((float)rc.right+offset.x)/pHeightamp->GetWidth(), ((float)rc.bottom+offset.y)/pHeightamp->GetHeight(), image);
			}

			for(int x=rc.left; x<=rc.right; x++)
				for(int y=rc.top; y<=rc.bottom; y++)
				{
					uint32 lid = pHeightamp->GetLayerIdAt(x, y);
					pHeightamp->SetLayerIdAt(x+offset.x, y+offset.y, lid);
				}
			
			pHeightamp->UpdateLayerTexture(rc+offset);
		}
		if (m_archive)
		{
			delete m_archive;
			m_archive = 0;
		}
	}


	if(!isCopy && (bOnlyVegetation || (!bOnlyVegetation && !bOnlyTerrain)))
	{
		BBox box;
		box.min = m_source.pos-m_dym/2;
		box.max = m_source.pos+m_dym/2;
		pHeightamp->GetVegetationMap()->RepositionArea(box, moveOffset);
	}

	if(!bOnlyVegetation && !bOnlyTerrain)
	{
		GetIEditor()->ClearSelection();
		BBox selBox(m_source.pos-m_dym/2, m_source.pos+m_dym/2);
		if(!isCopy)
			GetIEditor()->GetObjectManager()->MoveObjects(selBox, moveOffset, isCopy);
		/*
		GetIEditor()->GetObjectManager()->SelectObjects(selBox);

		if(isCopy)
		{
			for (int i = 0; i < GetIEditor()->GetSelection()->GetCount(); i++)
			{
				CBaseObject *obj = GetIEditor()->GetSelection()->GetObject(i);
				if(obj)
				{
					CBaseObject * newobj = GetIEditor()->GetObjectManager()->CloneObject(obj);
					if(newobj)
						newobj->SetPos(obj->GetPos()+moveOffset);
				}
			}
		}
		else
			GetIEditor()->GetSelection()->Move( moveOffset,false,true );
			*/
	}

	/*
	// Load selection from archive.
	XmlNodeRef objRoot = m_archive->root->findChild("Objects");
	if (objRoot)
	{
		GetIEditor()->ClearSelection();
		CObjectArchive ar( GetIEditor()->GetObjectManager(),objRoot,true );
		GetIEditor()->GetObjectManager()->LoadObjects( ar,false );
	}
	*/
	GetIEditor()->ClearSelection();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveTool::SetArchive( CXmlArchive *ar )
{
	if (m_archive)
		delete m_archive;
	m_archive = ar;

	int x1,y1,x2,y2;
	// Load rect size our of archive.
	m_archive->root->getAttr( "X1",x1 );
	m_archive->root->getAttr( "Y1",y1 );
	m_archive->root->getAttr( "X2",x2 );
	m_archive->root->getAttr( "Y2",y2 );

	m_srcRect.SetRect( x1,y1,x2,y2 );
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveTool::Select(int nBox)
{
	m_source.isSelected = false;
	m_target.isSelected = false;

	if(nBox==0)
	{
		GetIEditor()->ShowTransformManipulator(false);
		m_source.isShow = false;
		m_target.isShow = false;
	}

	if(nBox==1)
	{
		m_source.isSelected = true;
		m_source.isShow = true;
		ITransformManipulator *pManipulator = GetIEditor()->ShowTransformManipulator(true);
		Matrix34 tm;
		tm.SetIdentity();
		tm.SetTranslation( m_source.pos );
		pManipulator->SetTransformation( COORDS_LOCAL, tm );
	}

	if(nBox==2)
	{
		m_target.isSelected = true;
		m_target.isShow = true;
		ITransformManipulator *pManipulator = GetIEditor()->ShowTransformManipulator(true);
		Matrix34 tm;
		tm.SetIdentity();
		tm.SetTranslation( m_target.pos );
		pManipulator->SetTransformation( COORDS_LOCAL, tm );
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveTool::OnManipulatorDrag( CViewport *view,ITransformManipulator *pManipulator,CPoint &p0,CPoint &p1,const Vec3 &value )
{
	int editMode = GetIEditor()->GetEditMode();
	if (editMode == eEditModeMove)
	{
		CHeightmap *pHeightamp = GetIEditor()->GetHeightmap();
		GetIEditor()->RestoreUndo();

		Vec3 pos = m_source.pos;
		Vec3 val = value;

		Vec3 max = pHeightamp->HmapToWorld(CPoint(pHeightamp->GetWidth(), pHeightamp->GetHeight()));

		uint wid = max.x;
		uint hey = max.y;
		
		if(m_target.isSelected)
			pos = m_target.pos;

		pManipulator = GetIEditor()->ShowTransformManipulator(true);
		Matrix34 tm = pManipulator->GetTransformation(COORDS_LOCAL);

		if (CUndo::IsRecording())
			CUndo::Record( new CUndoTerrainMoveTool(this) );

		if(m_source.isSelected)
		{
			Vec3 p = m_source.pos+val;
			if(p.x - m_dym.x/2 < 0)
				val.x = m_dym.x/2 - m_source.pos.x;
			if(p.y - m_dym.y/2 < 0)
				val.y = m_dym.y/2 - m_source.pos.y;

			if(p.x + m_dym.x/2 >= wid)
				val.x = wid - m_dym.x/2 - m_source.pos.x;
			if(p.y + m_dym.y/2 >= hey)
				val.y = hey - m_dym.y/2 - m_source.pos.y;

			m_source.pos+=val;
		}
		
		if(m_target.isSelected)
		{
			Vec3 p = m_target.pos+val;
			if(p.x - m_dym.x/2 < 0)
				val.x = m_dym.x/2 - m_target.pos.x;
			if(p.y - m_dym.y/2 < 0)
				val.y = m_dym.y/2 - m_target.pos.y;

			if(p.x + m_dym.x/2 >= wid)
				val.x = wid - m_dym.x/2 - m_target.pos.x;
			if(p.y + m_dym.y/2 >= hey)
				val.y = hey - m_dym.y/2 - m_target.pos.y;

			m_target.pos+=val;
		}

		tm.SetTranslation( pos + val );
		pManipulator->SetTransformation( COORDS_LOCAL,tm );

	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveTool::CorrectPosition()
{
	CHeightmap *pHeightamp = GetIEditor()->GetHeightmap();
	GetIEditor()->RestoreUndo();

	Vec3 max = pHeightamp->HmapToWorld(CPoint(pHeightamp->GetWidth(), pHeightamp->GetHeight()));

	uint wid = max.x;
	uint hey = max.y;

	if(m_source.pos.x - m_dym.x/2 < 0)
		m_source.pos.x = m_dym.x/2;
	if(m_source.pos.y - m_dym.y/2 < 0)
		m_source.pos.y = m_dym.y/2;

	if(m_source.pos.x + m_dym.x/2 >= wid)
		m_source.pos.x = wid-m_dym.x/2;
	if(m_source.pos.y + m_dym.y/2 >= hey)
		m_source.pos.y = hey-m_dym.y/2;

	if(m_target.pos.x - m_dym.x/2 < 0)
		m_target.pos.x = m_dym.x/2;
	if(m_target.pos.y - m_dym.y/2 < 0)
		m_target.pos.y = m_dym.y/2;

	if(m_target.pos.x + m_dym.x/2 >= wid)
		m_target.pos.x = wid-m_dym.x/2;
	if(m_target.pos.y + m_dym.y/2 >= hey)
		m_target.pos.y = hey-m_dym.y/2;


	if ( m_source.isSelected || m_target.isSelected )
	{
		ITransformManipulator * pManipulator = GetIEditor()->ShowTransformManipulator(true);
		Matrix34 tm = pManipulator->GetTransformation(COORDS_LOCAL);
		if(m_source.isSelected)
			tm.SetTranslation( m_source.pos );
		if(m_target.isSelected)
			tm.SetTranslation( m_target.pos );
		pManipulator->SetTransformation( COORDS_LOCAL,tm );
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveTool::SetDym(Vec3 dym)
{	
	m_dym = dym;
	CorrectPosition();
}