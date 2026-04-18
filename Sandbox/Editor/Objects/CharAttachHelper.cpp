////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek 2001-2007.
// -------------------------------------------------------------------------
//  File name:   CharAttachHelper.cpp
//  Version:     v1.00
//  Created:     09/06/2007 by Timur.
//  Description: Entity character attachment helper object.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CharAttachHelper.h"
#include <ICryAnimation.h>

IMPLEMENT_DYNCREATE(CCharacterAttachHelperObject,CEntity)

float CCharacterAttachHelperObject::m_charAttachHelperScale = 1.0f;

//////////////////////////////////////////////////////////////////////////
CCharacterAttachHelperObject::CCharacterAttachHelperObject()
{
	m_entityClass = "CharacterAttachHelper";
	UseMaterialLayersMask(false);
}

//////////////////////////////////////////////////////////////////////////
void CCharacterAttachHelperObject::Display( DisplayContext &dc )
{
	__super::Display(dc);

	dc.SetLineWidth(4.0f);
	float s = 1.0f*GetHelperScale();

	if (m_entity)
	{
		Matrix34 tm = m_entity->GetWorldTM();

		if (IsSelected())
			dc.SetSelectedColor();
		else
			dc.SetColor( GetColor() );
	
		dc.SetLineWidth(4.0f);
		dc.DrawLine( tm.TransformPoint(Vec3(0,0,0)),tm.TransformPoint(Vec3(s,0,0)),ColorF(1,0,0),ColorF(1,0,0) );
		dc.DrawLine( tm.TransformPoint(Vec3(0,0,0)),tm.TransformPoint(Vec3(0,s,0)),ColorF(0,1,0),ColorF(0,1,0) );
		dc.DrawLine( tm.TransformPoint(Vec3(0,0,0)),tm.TransformPoint(Vec3(0,0,s)),ColorF(0,0,1),ColorF(0,0,1) );
		dc.SetLineWidth(0);

		dc.SetColor( ColorB(0,255,0,255) );
		dc.DrawLine( tm.TransformPoint(Vec3(0,0,0)),GetWorldPos() );
	}

	const Matrix34 &tm = GetWorldTM();

	if (IsSelected())
		dc.SetSelectedColor();
	else
		dc.SetColor( GetColor() );

	dc.SetLineWidth(4.0f);
	dc.DrawLine( tm.TransformPoint(Vec3(0,0,0)),tm.TransformPoint(Vec3(s,0,0)),ColorF(1,0,0),ColorF(1,0,0) );
	dc.DrawLine( tm.TransformPoint(Vec3(0,0,0)),tm.TransformPoint(Vec3(0,s,0)),ColorF(0,1,0),ColorF(0,1,0) );
	dc.DrawLine( tm.TransformPoint(Vec3(0,0,0)),tm.TransformPoint(Vec3(0,0,s)),ColorF(0,0,1),ColorF(0,0,1) );
	dc.SetLineWidth(0);
}
