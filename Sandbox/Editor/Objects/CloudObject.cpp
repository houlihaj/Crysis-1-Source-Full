////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   CloudObject.cpp
//  Version:     v1.00
//  Created:     05/02/2005 by Sergiy Shaykin.
//  Compilers:   Visual C++ 6.0
//  Description: CCloudObject implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CloudObject.h"

#include "ObjectManager.h"
#include "..\Viewport.h"

#include "PanelTreeBrowser.h"

#include "Entity.h"
#include "Geometry\EdMesh.h"
#include "Material\Material.h"
#include "Material\MaterialManager.h"
#include "Settings.h"

#include <I3Dengine.h>
#include <IEntitySystem.h>
#include <IEntityRenderState.h>


//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CCloudObject,CBaseObject)


//////////////////////////////////////////////////////////////////////////
CCloudObject::CCloudObject()
{
	m_bNotSharedGeom = false;
	m_bIgnoreNodeUpdate = false;

	m_width = 1;
	m_height = 1;
	m_length = 1;
	mv_spriteRow = -1;
	m_angleVariations = 0.0f;

	//mv_useSize = true;
	mv_sizeSprites = 0;
	mv_randomSizeValue = 0;


	AddVariable( mv_spriteRow,"SpritesRow","Sprites Row");
	mv_spriteRow.SetLimits(-1, 64);
	AddVariable( m_width,"Width",functor(*this,&CCloudObject::OnSizeChange) );
	m_width.SetLimits(0, 99999);
	AddVariable( m_height,"Height",functor(*this,&CCloudObject::OnSizeChange) );
	m_height.SetLimits(0, 99999);
	AddVariable( m_length,"Length",functor(*this,&CCloudObject::OnSizeChange) );
	m_length.SetLimits(0, 99999);
	AddVariable( m_angleVariations,"AngleVariations","Angle Variations",functor(*this,&CCloudObject::OnSizeChange), IVariable::DT_ANGLE );
	m_angleVariations.SetLimits(0, 99999);

	//AddVariable( mv_useSize,"UseSize","Use Size from Cloud",functor(*this,&CCloudObject::OnSizeChange) );
	AddVariable( mv_sizeSprites,"SizeofSprites", "Size of Sprites",functor(*this,&CCloudObject::OnSizeChange));
	mv_sizeSprites.SetLimits(0, 99999);
	AddVariable( mv_randomSizeValue,"SizeVariation","Size Variation",functor(*this,&CCloudObject::OnSizeChange) );
	mv_randomSizeValue.SetLimits(0, 99999);
}

//////////////////////////////////////////////////////////////////////////
void CCloudObject::OnSizeChange(IVariable *pVar)
{
	InvalidateTM(	TM_POS_CHANGED | TM_ROT_CHANGED | TM_SCL_CHANGED);
}


//////////////////////////////////////////////////////////////////////////
bool CCloudObject::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	SetColor( RGB(127,127,255) );
	
	if (prev)
	{
		CCloudObject *cloudObj = (CCloudObject*)prev;
	}

	// Must be after SetCloud call.
	bool res = CBaseObject::Init( ie,prev,file );
	
	if (prev)
	{
		CCloudObject *cloudObj = (CCloudObject*)prev;
		m_bbox = cloudObj->m_bbox;
	}

	return res;
}

//////////////////////////////////////////////////////////////////////////
void CCloudObject::GetLocalBounds( BBox &box )
{
	box.min.x = -m_width/2;
	box.min.y = -m_length/2;
	box.min.z = -m_height/2;

	box.max.x = m_width/2;
	box.max.y = m_length/2;
	box.max.z = m_height/2;
}

//////////////////////////////////////////////////////////////////////////
int CCloudObject::MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	if (event == eMouseMove || event == eMouseLDown || event == eMouseLUp)
	{
		Vec3 pos;
		pos = view->MapViewToCP( point );
		SetPos(pos);

		if (event == eMouseLDown)
			return MOUSECREATE_OK;

		return MOUSECREATE_CONTINUE;
	}
	return CBaseObject::MouseCreateCallback( view,event,point,flags );
}

//////////////////////////////////////////////////////////////////////////
void CCloudObject::Display( DisplayContext &dc )
{
	const Matrix34 &wtm = GetWorldTM();
	Vec3 wp = wtm.GetTranslation();

	if (IsSelected())
		dc.SetSelectedColor();
	else if (IsFrozen())
		dc.SetFreezeColor();
	else
		dc.SetColor( GetColor() );

	dc.PushMatrix( wtm );
	BBox box;
	GetLocalBounds(box);
	dc.DrawWireBox( box.min,box.max);
	dc.PopMatrix();

	DrawDefault( dc );
}

//////////////////////////////////////////////////////////////////////////
bool CCloudObject::HitTest( HitContext &hc )
{
	Vec3 origin = GetWorldPos();

	BBox box;
	GetBoundBox(box);

	float radius = sqrt((box.max.x-box.min.x) * (box.max.x-box.min.x) + (box.max.y-box.min.y) * (box.max.y-box.min.y) + (box.max.z-box.min.z) * (box.max.z-box.min.z))/2;

	Vec3 w = origin - hc.raySrc;
	w = hc.rayDir.Cross( w );
	float d = w.GetLength();

	if (d < radius + hc.distanceTollerance)
	{
		hc.dist = hc.raySrc.GetDistance(origin);
		return true;
	}
	return false;
}