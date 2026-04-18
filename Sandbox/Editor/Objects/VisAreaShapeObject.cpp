////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   VisAreaShapeObject.cpp
//  Version:     v1.00
//  Created:     10/12/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "VisAreaShapeObject.h"

#include <I3DEngine.h>
#include <ISound.h> // to RecomputeSoundOcclusion() when deleting a vis area

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CVisAreaShapeObject,CShapeObject)
IMPLEMENT_DYNCREATE(COccluderShapeObject,CVisAreaShapeObject)
IMPLEMENT_DYNCREATE(CPortalShapeObject,CVisAreaShapeObject)

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
C3DEngineAreaObjectBase::C3DEngineAreaObjectBase()
{
	m_area = 0;
	mv_closed = true;
	m_bPerVertexHeight = true;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef C3DEngineAreaObjectBase::Export( const CString &levelPath,XmlNodeRef &xmlNode )
{
	XmlNodeRef objNode = CBaseObject::Export( levelPath,xmlNode );

	// Export Points
	if (!m_points.empty())
	{
		const Matrix34 &wtm = GetWorldTM();
		XmlNodeRef points = objNode->newChild( "Points" );
		for (int i = 0; i < m_points.size(); i++)
		{
			XmlNodeRef pnt = points->newChild( "Point" );
			pnt->setAttr( "Pos",wtm.TransformPoint(m_points[i]) );
		}
	}

	return objNode;
}

//////////////////////////////////////////////////////////////////////////
void C3DEngineAreaObjectBase::Done()
{
	if (m_area)
	{
		// reset the listener vis area in the unlucky case that we are deleting the
		// vis area where the listener is currently in
		if(GetIEditor()->GetSystem()->GetISoundSystem())
			GetIEditor()->GetSystem()->GetISoundSystem()->RecomputeSoundOcclusion(false,false,true);
		GetIEditor()->Get3DEngine()->DeleteVisArea(m_area);
		m_area = 0;
	}
	__super::Done();
}

//////////////////////////////////////////////////////////////////////////
bool C3DEngineAreaObjectBase::CreateGameObject()
{
	if (!m_area)
	{
		m_area = GetIEditor()->Get3DEngine()->CreateVisArea();
		m_bAreaModified = true;
		UpdateGameArea(false);
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
// CVisAreaShapeObject
//////////////////////////////////////////////////////////////////////////
CVisAreaShapeObject::CVisAreaShapeObject()
{
	mv_height = 5;
	m_bDisplayFilledWhenSelected = true;

	mv_vAmbientColor = Vec3(0.0625f,0.0625f,0.0625f);
	mv_bAffectedBySun = false;
  mv_bIgnoreSkyColor = false;
	mv_fViewDistRatio = 100.f;
	mv_bSkyOnly = false;
	mv_vMergeBrushes = false;
  mv_bOceanIsVisible = false;
	mv_fVolumetricFogDensityMultiplier = 1.0f;

	SetColor( RGB(255,128,0) );
}

//////////////////////////////////////////////////////////////////////////
void CVisAreaShapeObject::InitVariables()
{
	AddVariable( mv_height,"Height",functor(*this,&CVisAreaShapeObject::OnShapeChange) );
	AddVariable( mv_displayFilled,"DisplayFilled" );

	AddVariable( mv_vAmbientColor,"AmbientColor",functor(*this,&CVisAreaShapeObject::OnShapeChange), IVariable::DT_COLOR );
  AddVariable( mv_bAffectedBySun,"AffectedBySun",functor(*this,&CVisAreaShapeObject::OnShapeChange) );
  AddVariable( mv_bIgnoreSkyColor,"IgnoreSkyColor",functor(*this,&CVisAreaShapeObject::OnShapeChange) );
	AddVariable( mv_fViewDistRatio,"ViewDistRatio",functor(*this,&CVisAreaShapeObject::OnShapeChange) );
	AddVariable( mv_bSkyOnly,"SkyOnly",functor(*this,&CVisAreaShapeObject::OnShapeChange) );
	AddVariable( mv_vMergeBrushes,"MergeBrushes",functor(*this,&CVisAreaShapeObject::OnShapeChange) );
  AddVariable( mv_bOceanIsVisible,"OceanIsVisible",functor(*this,&CVisAreaShapeObject::OnShapeChange) );
	AddVariable( mv_fVolumetricFogDensityMultiplier,"FogDensityMultiplier",functor(*this,&CVisAreaShapeObject::OnShapeChange) );
}

//////////////////////////////////////////////////////////////////////////
void CVisAreaShapeObject::UpdateGameArea( bool bRemove )
{
	if (bRemove)
		return;
	if (!m_bAreaModified)
		return;

	if (m_area)
	{
		std::vector<Vec3> points;
		if (GetPointCount() > 3)
		{
			const Matrix34 &wtm = GetWorldTM();
			points.resize(GetPointCount());
			for (int i = 0; i < GetPointCount(); i++)
			{
				points[i] = wtm.TransformPoint( GetPoint(i) );
			}

			Vec3 vAmbClr = mv_vAmbientColor;

      SVisAreaInfo info;
      info.fHeight = GetHeight();
      info.vAmbientColor = vAmbClr;
      info.bAffectedByOutLights = mv_bAffectedBySun;
      info.bIgnoreSkyColor = mv_bIgnoreSkyColor;
      info.bSkyOnly = mv_bSkyOnly;
      info.fViewDistRatio = mv_fViewDistRatio;
      info.bDoubleSide = true;
      info.bUseDeepness = false;
      info.bUseInIndoors = false;
      info.bMergeBrushes = mv_vMergeBrushes;
      info.bOceanIsVisible = mv_bOceanIsVisible;
			info.fVolumetricFogDensityMultiplier = mv_fVolumetricFogDensityMultiplier;

			GetIEditor()->Get3DEngine()->UpdateVisArea( m_area, &points[0],points.size(), GetName(), info, true );
		}
	}
	m_bAreaModified = false;
}

//////////////////////////////////////////////////////////////////////////
// CPortalShapeObject
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CPortalShapeObject::CPortalShapeObject()
{
	m_bDisplayFilledWhenSelected = true;
	SetColor( RGB(100,250,60) );

	mv_bUseDeepness = true;
	mv_bDoubleSide = true;
}

//////////////////////////////////////////////////////////////////////////
void CPortalShapeObject::InitVariables()
{
	CVisAreaShapeObject::InitVariables();

  AddVariable( mv_bUseDeepness,"UseDeepness",functor(*this,&CPortalShapeObject::OnShapeChange) );
	AddVariable( mv_bDoubleSide,"DoubleSide",functor(*this,&CPortalShapeObject::OnShapeChange) );
}

//////////////////////////////////////////////////////////////////////////
void CPortalShapeObject::UpdateGameArea( bool bRemove )
{
	if (bRemove)
		return;
	
	if (!m_bAreaModified)
		return;

	if (m_area)
	{
		std::vector<Vec3> points;
		if (GetPointCount() > 3)
		{
			const Matrix34 &wtm = GetWorldTM();
			points.resize(GetPointCount());
			for (int i = 0; i < GetPointCount(); i++)
			{
				points[i] = wtm.TransformPoint( GetPoint(i) );
			}

			CString name = CString("Portal_") + GetName();

			Vec3 vAmbClr = mv_vAmbientColor;

      SVisAreaInfo info;
      info.fHeight = GetHeight();
      info.vAmbientColor = vAmbClr;
      info.bAffectedByOutLights = mv_bAffectedBySun;
      info.bIgnoreSkyColor = mv_bIgnoreSkyColor;
      info.bSkyOnly = mv_bSkyOnly;
      info.fViewDistRatio = mv_fViewDistRatio;
      info.bDoubleSide = true;
      info.bUseDeepness = false;
      info.bUseInIndoors = false;
      info.bMergeBrushes = mv_vMergeBrushes;
      info.bOceanIsVisible = mv_bOceanIsVisible;
			info.fVolumetricFogDensityMultiplier = mv_fVolumetricFogDensityMultiplier;

      GetIEditor()->Get3DEngine()->UpdateVisArea( m_area, &points[0],points.size(), name, info, true );
		}
	}
	m_bAreaModified = false;
}

//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
COccluderShapeObject::COccluderShapeObject()
{
	m_bDisplayFilledWhenSelected = true;
	SetColor( RGB(200,128,60) );

	mv_height = 5;
	mv_displayFilled = true;

	mv_bUseInIndoors = false;
	mv_bDoubleSide = true;
  mv_fViewDistRatio = 100.f;
}

//////////////////////////////////////////////////////////////////////////
void COccluderShapeObject::InitVariables()
{
	AddVariable( mv_height,"Height",functor(*this,&COccluderShapeObject::OnShapeChange) );
	AddVariable( mv_displayFilled,"DisplayFilled" );

  AddVariable( mv_fViewDistRatio,"CullDistRatio",functor(*this,&COccluderShapeObject::OnShapeChange) );
  AddVariable( mv_bUseInIndoors,"UseInIndoors",functor(*this,&COccluderShapeObject::OnShapeChange) );
	AddVariable( mv_bDoubleSide,"DoubleSide",functor(*this,&COccluderShapeObject::OnShapeChange) );
}

//////////////////////////////////////////////////////////////////////////
void COccluderShapeObject::UpdateGameArea( bool bRemove )
{
	if (bRemove)
		return;
	if (!m_bAreaModified)
		return;

	if (m_area)
	{
		std::vector<Vec3> points;
		if (GetPointCount() > 1)
		{
			const Matrix34 &wtm = GetWorldTM();
			points.resize(GetPointCount());
			for (int i = 0; i < GetPointCount(); i++)
			{
				points[i] = wtm.TransformPoint( GetPoint(i) );
			}
			if (!m_points[0].IsEquivalent(m_points[1]))
			{
				CString name = CString("OcclArea_") + GetName();

        SVisAreaInfo info;
        info.fHeight = GetHeight();
        info.vAmbientColor = Vec3(0,0,0);
        info.bAffectedByOutLights = false;
        info.bSkyOnly = false;
        info.fViewDistRatio = mv_fViewDistRatio;
        info.bDoubleSide = true;
        info.bUseDeepness = false;
        info.bUseInIndoors = mv_bUseInIndoors;
        info.bMergeBrushes = false;
        info.bOceanIsVisible = false;

				GetIEditor()->Get3DEngine()->UpdateVisArea( m_area, &points[0],points.size(), name, info, false );
			}
		}
	}
	m_bAreaModified = false;
}
