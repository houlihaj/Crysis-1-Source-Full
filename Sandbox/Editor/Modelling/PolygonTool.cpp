////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek, 2007.
// -------------------------------------------------------------------------
//  File name:   PolygonTool.cpp
//  Version:     v1.00
//  Created:     11/1/2007 by Timur.
//  Description: Polygon creation edit tool.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <InitGuid.h>
#include "PolygonTool.h"
#include "Geometry\EdMesh.h"

#include "..\Viewport.h"

#include "Objects\ObjectManager.h"
#include "Objects\MeshObject.h"

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CPolygonTool,CEditTool)

//////////////////////////////////////////////////////////////////////////
CPolygonTool::CPolygonTool()
{
	GetIEditor()->SetOperationMode( eModellingMode );

	m_pClassDesc = GetIEditor()->GetClassFactory()->FindClass(CREATE_POLYGON_TOOL_GUID);
	SetStatusText( _T("Create Polygon") );

	m_bMouseCaptured = false;
	m_bCreating = false;
	
	m_hDefaultCursor = AfxGetApp()->LoadCursor(IDC_POINTER_OBJECT_MOVE);

	m_mouseDownPos.SetPoint(0,0);
	m_mouseCurPos.SetPoint(0,0);
	m_pos1 = m_pos2 = Vec3(0,0,0);
}

//////////////////////////////////////////////////////////////////////////
CPolygonTool::~CPolygonTool()
{
}

//////////////////////////////////////////////////////////////////////////
void CPolygonTool::BeginEditParams( IEditor *ie,int flags )
{
	GetIEditor()->ClearSelection();
}

//////////////////////////////////////////////////////////////////////////
void CPolygonTool::EndEditParams()
{
}

void::CPolygonTool::Display( DisplayContext &dc )
{
	if (!m_bCreating)
		return;

	Vec3 v[4];
	v[0] = m_pos1;
	v[1] = m_pos1 + Vec3(m_pos2.x-m_pos1.x,0,0);
	v[2] = Vec3(m_pos2.x,m_pos2.y,m_pos1.z);
	v[3] = m_pos1 + Vec3(0,m_pos2.y-m_pos1.y,0);

	dc.SetColor( ColorB(0,128,200) );
	dc.DrawQuad( v[0],v[1],v[2],v[3] );

	dc.SetColor( ColorB(0,200,0) );
	dc.DrawLine( m_pos1,m_pos2 );
	dc.DrawBall( m_pos1,0.1f );
	dc.DrawBall( m_pos2,0.1f );
}

bool CPolygonTool::OnLButtonDown( CViewport *view,UINT nFlags,CPoint point )
{
	m_mouseDownPos = point;

	// If clicked within selected brush. move this brush.
	HitContext  hitInfo;
	m_pos1 = HitTest( view,point,hitInfo );

	m_bCreating = true;
	view->CaptureMouse();
	m_bMouseCaptured = true;
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CPolygonTool::OnLButtonUp( CViewport *view,UINT nFlags,CPoint point )
{
	bool bResult = false;
	if (m_bCreating)
	{
		HitContext  hitInfo;
		m_pos1 = HitTest( view,point,hitInfo );

		MakePoly( view,m_mouseDownPos,point );
		bResult = true;
	}
	m_bCreating = false;
	if (m_bMouseCaptured)
	{
		view->ReleaseMouse();
	}
	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CPolygonTool::OnSetCursor( CViewport *vp )
{
	vp->SetCursor(m_hDefaultCursor);
	return true;
};

//////////////////////////////////////////////////////////////////////////
bool CPolygonTool::OnMouseMove( CViewport *view,UINT nFlags, CPoint point )
{
	m_mouseCurPos = point;

	HitContext  hitInfo;
	m_pos2 = HitTest( view,point,hitInfo );

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPolygonTool::MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
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

	// Not processed.
	return bProcessed;
}

//////////////////////////////////////////////////////////////////////////
void CPolygonTool::MakePoly( CViewport *view,CPoint p1,CPoint p2 )
{
	CRect rc( p1,p2 );
	rc.NormalizeRect();


	HitContext  hitInfo1;
	HitContext  hitInfo2;
	m_pos1 = HitTest( view,p1,hitInfo1 );
	m_pos2 = HitTest( view,p2,hitInfo2 );
	m_pos2.z = m_pos1.z;

	if (IsVectorsEqual(m_pos1,m_pos2))
		return;
	
	CMeshObject *meshObj = (CMeshObject*)GetIEditor()->NewObject( "MeshObject" );
	m_pObject = meshObj;
	meshObj->SetPos( m_pos1 );

	CEdMesh *pMeshGeometry = CEdMesh::CreateMesh( meshObj->GetName() );
	pMeshGeometry->CreateMesh( meshObj->GetName() );

	MakeMesh( *pMeshGeometry->GetMesh() );

	pMeshGeometry->InvalidateMesh();
	meshObj->SetMeshGeometry(pMeshGeometry);
}

//////////////////////////////////////////////////////////////////////////
void CPolygonTool::MakeMesh( CTriMesh &mesh )
{
	Vec3 pz = Vec3(0,0,0);
	Vec3 v[4];
	v[0] = pz;
	v[1] = pz + Vec3(m_pos2.x-m_pos1.x,0,0);
	v[2] = pz + Vec3(m_pos2.x-m_pos1.x,m_pos2.y-m_pos1.y,pz.z);
	v[3] = pz + Vec3(0,m_pos2.y-m_pos1.y,0);

	// Set Mesh geometry.
	mesh.SetVertexCount(4);
	mesh.SetUVCount(4);
	mesh.SetFacesCount(2);
	
	mesh.pVertices[0].pos = v[0];
	mesh.pVertices[1].pos = v[1];
	mesh.pVertices[2].pos = v[2];
	mesh.pVertices[3].pos = v[3];

	mesh.pUV[0].s = 0; mesh.pUV[0].t = 0;
	mesh.pUV[1].s = 1; mesh.pUV[1].t = 0;
	mesh.pUV[2].s = 1; mesh.pUV[2].t = 1;
	mesh.pUV[3].s = 0; mesh.pUV[3].t = 1;

	Vec3 normal(0,0,1);
	memset( mesh.pFaces,0,sizeof(mesh.pFaces[0])*mesh.nFacesCount );
	mesh.pFaces[0].n[0] = normal;
	mesh.pFaces[0].n[1] = normal;
	mesh.pFaces[0].n[2] = normal;
	mesh.pFaces[0].v[0] = 0;
	mesh.pFaces[0].v[1] = 1;
	mesh.pFaces[0].v[2] = 2;
	mesh.pFaces[0].uv[0] = 0;
	mesh.pFaces[0].uv[1] = 1;
	mesh.pFaces[0].uv[2] = 2;

	mesh.pFaces[1].n[0] = normal;
	mesh.pFaces[1].n[1] = normal;
	mesh.pFaces[1].n[2] = normal;
	mesh.pFaces[1].v[0] = 0;
	mesh.pFaces[1].v[1] = 2;
	mesh.pFaces[1].v[2] = 3;
	mesh.pFaces[1].uv[0] = 0;
	mesh.pFaces[1].uv[1] = 2;
	mesh.pFaces[1].uv[2] = 3;

	mesh.UpdateEdges();
}

//////////////////////////////////////////////////////////////////////////
Vec3 CPolygonTool::HitTest( CViewport *view,CPoint point,HitContext &hitInfo )
{
	if (view->HitTest(point,hitInfo))
	{
		return hitInfo.raySrc + hitInfo.rayDir*hitInfo.dist;
	}
	else
	{
		return view->MapViewToCP(point) + Vec3(0,0,1);
	}
}

//////////////////////////////////////////////////////////////////////////
// Class description.
//////////////////////////////////////////////////////////////////////////
class CPolygonTool_ClassDesc : public CRefCountClassDesc
{
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_EDITTOOL; }
	virtual REFGUID ClassID() { return CREATE_POLYGON_TOOL_GUID; }
	virtual const char* ClassName() { return "EditTool.PolygonTool"; };
	virtual const char* Category() { return "Create"; };
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CPolygonTool); }
};

//////////////////////////////////////////////////////////////////////////
void CPolygonTool::RegisterTool( CRegistrationContext &rc )
{
	rc.pClassFactory->RegisterClass( new CPolygonTool_ClassDesc );
}
