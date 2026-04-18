////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   AutoCubemapObject.cpp
//  Version:     v1.00
//  Created:     6/5/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: Special tag point for comment.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "AutoCubemapObject.h"

#include "..\Viewport.h"
#include "Settings.h"

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CAutoCubemapObject,CBaseObject)

//////////////////////////////////////////////////////////////////////////
CAutoCubemapObject::CAutoCubemapObject()
{
	SetColor( RGB(0,200,200) );

	m_pRenderNode = 0;
	m_bIgnoreUpdateRenderNode = false;

	mv_alwaysUpdate = false;
	mv_sizeX = 10;
	mv_sizeY = 10;
	mv_sizeZ = 10;

	AddVariable( mv_alwaysUpdate,"AlwaysUpdate",functor(*this,&CAutoCubemapObject::OnParamChange) );
	AddVariable( mv_sizeX,"SizeX",functor(*this,&CAutoCubemapObject::OnParamChange) );
	AddVariable( mv_sizeY,"SizeY",functor(*this,&CAutoCubemapObject::OnParamChange) );
	AddVariable( mv_sizeZ,"SizeZ",functor(*this,&CAutoCubemapObject::OnParamChange) );
}

//////////////////////////////////////////////////////////////////////////
void CAutoCubemapObject::Done()
{
	if (m_pRenderNode)
	{
		gEnv->p3DEngine->DeleteRenderNode(m_pRenderNode);
		m_pRenderNode = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CAutoCubemapObject::CreateGameObject()
{
	m_pRenderNode = (IAutoCubeMapRenderNode*)gEnv->p3DEngine->CreateRenderNode(eERType_AutoCubeMap);
	UpdateRenderNode();
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CAutoCubemapObject::SetScale( const Vec3 &scale )
{
	// Ignore scale.
}

//////////////////////////////////////////////////////////////////////////
int CAutoCubemapObject::MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	if (event == eMouseMove || event == eMouseLDown)
	{
		Vec3 pos;
		if (GetIEditor()->GetAxisConstrains() != AXIS_TERRAIN)
		{
			pos = view->MapViewToCP(point);
		}
		else
		{
			// Snap to terrain.
			bool hitTerrain;
			pos = view->ViewToWorld( point,&hitTerrain );
			//if (hitTerrain)
			//{
			//	pos.z = GetIEditor()->GetTerrainElevation(pos.x,pos.y) + GetRadius()*2;
			//}
		}

		pos = view->SnapToGrid(pos);
		SetPos( pos );
		if (event == eMouseLDown)
			return MOUSECREATE_OK;
		return MOUSECREATE_CONTINUE;
	}
	return CBaseObject::MouseCreateCallback( view,event,point,flags );
}

//////////////////////////////////////////////////////////////////////////
void CAutoCubemapObject::Display( DisplayContext &dc )
{
	const Matrix34 &wtm = GetWorldTM();

	Vec3 wp = wtm.GetTranslation();

	if (IsSelected())
	{
		if (IsFrozen())
			dc.SetFreezeColor();
		else if (IsSelected())
		{
			dc.SetSelectedColor();
		}
		else
		{
			dc.SetColor(GetColor());
		}

		Vec3 vmax(0.5f * Vec3(mv_sizeX, mv_sizeY, mv_sizeZ));
		Vec3 vmin(-vmax);

		dc.PushMatrix( wtm );
		dc.DrawWireBox( vmin,vmax );
		dc.PopMatrix();
	}

	DrawDefault( dc );
}

//////////////////////////////////////////////////////////////////////////
bool CAutoCubemapObject::HitTest( HitContext &hc )
{
	return __super::HitTest(hc);
	
	//Vec3 origin = GetWorldPos();
	//float radius = GetRadius();

	//Vec3 w = origin - hc.raySrc;
	//w = hc.rayDir.Cross( w );
	//float d = w.GetLength();

	//if (d < radius + hc.distanceTollerance)
	//{
	//	hc.dist = hc.raySrc.GetDistance(origin);
	//	return true;
	//}
	//return false;	
}

//////////////////////////////////////////////////////////////////////////
void CAutoCubemapObject::InvalidateTM( int nWhyFlags )
{
	__super::InvalidateTM(nWhyFlags);
	UpdateRenderNode();
}

//////////////////////////////////////////////////////////////////////////
void CAutoCubemapObject::GetLocalBounds( BBox &box )
{
	box.max = 0.5f * Vec3(mv_sizeX, mv_sizeY, mv_sizeZ);
	box.min = -box.max;
}

//////////////////////////////////////////////////////////////////////////
void CAutoCubemapObject::Serialize( CObjectArchive &ar )
{
	m_bIgnoreUpdateRenderNode = true;
	__super::Serialize(ar);	
	m_bIgnoreUpdateRenderNode = false;
	if (ar.bLoading)
	{
		UpdateRenderNode();
	}
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CAutoCubemapObject::Export( const CString &levelPath,XmlNodeRef &xmlNode )
{
	// Dont export this. Only relevant for editor.
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CAutoCubemapObject::OnParamChange( IVariable *var )
{
	UpdateRenderNode();
}

//////////////////////////////////////////////////////////////////////////
void CAutoCubemapObject::UpdateRenderNode()
{
	if (m_bIgnoreUpdateRenderNode)
		return;

	if (m_pRenderNode)
	{
		const Matrix34 wtm(GetWorldTM());
		SAutoCubeMapProperties properties;
		properties.m_flags = 0;
		assert(Vec3(GetScale() - Vec3(1,1,1)).GetLengthSquared() < 1e-4f);
		properties.m_obb.SetOBB(Matrix33(wtm), 0.5f * Vec3(mv_sizeX, mv_sizeY, mv_sizeZ), wtm.GetTranslation());
		properties.m_refPos = wtm.GetTranslation(); // for now the cube map ref position is equal to the obb center
		m_pRenderNode->SetProperties(properties);
		m_pRenderNode->SetMatrix(wtm);

		m_pRenderNode->SetMinSpec( GetMinSpec() );
		m_pRenderNode->SetMaterialLayers( GetMaterialLayersMask() );
	}
}

//////////////////////////////////////////////////////////////////////////
void CAutoCubemapObject::SetMinSpec( uint32 nSpec )
{
	__super::SetMinSpec(nSpec);
	UpdateRenderNode();
}
