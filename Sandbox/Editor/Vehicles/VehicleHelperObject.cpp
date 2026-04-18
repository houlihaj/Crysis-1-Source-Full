////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   VehicleHelper.cpp
//  Version:     v1.00
//  Created:     21/06/2005 by MichaelR
//  Compilers:   Visual C++ 6.0
//  Description: Vehicle Helper Object implementation
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "VehicleHelperObject.h"
#include "..\Viewport.h"

#include "VehicleData.h"
#include "VehiclePrototype.h"
#include "VehiclePart.h"

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CVehicleHelper,CBaseObject)

#define RADIUS 0.05f

//////////////////////////////////////////////////////////////////////////
CVehicleHelper::CVehicleHelper()
{	
  m_pVar = 0;
  m_fromGeometry = false;
  m_pPart = 0;
  m_pVehicle = 0;
}


CVehicleHelper::~CVehicleHelper()
{ 
}


void CVehicleHelper::UpdateVarFromObject()
{
  assert(m_pVar);
  if (!m_pVar || !m_pVehicle)
    return;

  IEntity* pEnt = m_pVehicle->GetCEntity()->GetIEntity();
  if (!pEnt)
  {
    Warning("[CVehicleHelper::UpdateVariable] pEnt is null, returning");
    return;
  }

  if (IVariable* pVar = GetChildVar(m_pVar, "name"))
    pVar->Set(GetName());

  if (IVariable* pVar = GetChildVar(m_pVar, "position"))
  { 
    Vec3 pos = pEnt->GetWorldTM().GetInvertedFast().TransformPoint(GetWorldPos());
    pVar->Set(pos);    
  }

  IVariable* pVar = GetChildVar(m_pVar, "direction");
  if (!pVar)
  {
    pVar = new CVariable<Vec3>;
    pVar->SetName("direction");
    m_pVar->AddChildVar(pVar);    
  }       

  Matrix33 relTM = Matrix33(pEnt->GetWorldTM().GetInvertedFast()) * Matrix33(GetWorldTM());  
  Vec3 dir = relTM.TransformVector(FORWARD_DIRECTION);
  pVar->Set( dir );

}

//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::UpdateObjectFromVar()
{  
  if (!m_pVar || !m_pVehicle)
    return;

  if (IVariable* pVar = GetChildVar(m_pVar, "position"))
  { 
    Vec3 local(ZERO);
    pVar->Get(local);
    Matrix34 tm = GetWorldTM();
    tm.SetTranslation( m_pVehicle->GetCEntity()->GetIEntity()->GetWorldTM().TransformPoint(local) );
    SetWorldTM(tm);
  }
}

//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::Done()
{	
  VeedLog("[CVehicleHelper:Done] <%s>", GetName());
  CBaseObject::Done();
}

//////////////////////////////////////////////////////////////////////////
bool CVehicleHelper::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	SetColor( RGB(255,255,0) );
	bool res = CBaseObject::Init( ie,prev,file );
	
	return res;
}



//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::BeginEditParams( IEditor *ie,int flags )
{	
}

//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::EndEditParams( IEditor *ie )
{
	CBaseObject::EndEditParams( ie );
}


//////////////////////////////////////////////////////////////////////////
int CVehicleHelper::MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	if (event == eMouseMove || event == eMouseLDown)
	{
		Vec3 pos;
		// Position 1 meter above ground when creating.
		if (GetIEditor()->GetAxisConstrains() != AXIS_TERRAIN)
		{
			pos = view->MapViewToCP(point);
		}
		else
		{
			// Snap to terrain.
			bool hitTerrain;
			pos = view->ViewToWorld( point,&hitTerrain );
			if (hitTerrain)
			{
				pos.z = GetIEditor()->GetTerrainElevation(pos.x,pos.y) + 1.0f;
			}
			pos = view->SnapToGrid(pos);
		}
		SetPos( pos );
		if (event == eMouseLDown)
			return MOUSECREATE_OK;
		return MOUSECREATE_CONTINUE;
	}
	return CBaseObject::MouseCreateCallback( view,event,point,flags );
}

//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::PostClone( CBaseObject *pFromObject,CObjectCloneContext &ctx )
{
  CBaseObject *pFromParent = pFromObject->GetParent();
  if (pFromParent)
  {
    CBaseObject *pChildParent = ctx.FindClone( pFromParent );
    if (pChildParent)
      pChildParent->AttachChild( this,false );
    else
    {
      // helper was cloned and attached to same parent       
      if (pFromParent->IsKindOf(RUNTIME_CLASS(CVehiclePart))) 
        ((CVehiclePart*)pFromParent)->AddHelper( this, 0 );
      else
        pFromParent->AttachChild(this, false);
    }
  }
}

//////////////////////////////////////////////////////////////////////////
bool CVehicleHelper::HitTest( HitContext &hc )
{
	Vec3 origin = GetWorldPos();
	float radius = RADIUS;

	Vec3 w = origin - hc.raySrc;
	w = hc.rayDir.Cross( w );
	float d = w.GetLength();

  if (d < radius + hc.distanceTollerance)
  {
    Vec3 i0;
    if (Intersect::Ray_SphereFirst(Ray(hc.raySrc,hc.rayDir),Sphere(origin,radius),i0))
    {
      hc.dist = hc.raySrc.GetDistance(i0);
      return true;
    }
    hc.dist = hc.raySrc.GetDistance(origin);
    return true;
  }	
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::Display( DisplayContext &dc )
{
	COLORREF color = GetColor();
  float radius = RADIUS;
	
	//dc.SetColor( color, 0.5f );
	//dc.DrawBall( GetPos(), radius );

	if (IsSelected())
	{
		dc.SetSelectedColor( 0.6f );		
	}

  BBox box;
  GetLocalBounds( box );
  dc.PushMatrix( GetWorldTM() );

  if (!IsHighlighted())
  { 
    dc.SetColor( color, 0.8f );    
    dc.SetLineWidth(2);
    dc.DrawWireBox( box.min,box.max );       
  }
  
  if (!IsSelected())
  {
    // direction vector      
    Vec3 dirEndPos(0,4*box.max.y,0);  
    dc.DrawArrow( Vec3(0,box.max.y,0), dirEndPos, 0.15f );
  }
  
  dc.PopMatrix();

  // draw label
  if (dc.flags & DISPLAY_HIDENAMES)
  {     
    Vec3 p(GetWorldPos());    
    DrawLabel( dc, p, RGB(255,255,255) );   
  }
    
	DrawDefault( dc );
}


//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::GetBoundBox( BBox &box )
{  
  // Transform local bounding box into world space.
  GetLocalBounds( box );
  box.SetTransformedAABB( GetWorldTM(), box );  
}

//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::GetLocalBounds( BBox &box )
{ 
  // return local bounds  
  float r = RADIUS;
  box.min = -Vec3(r,r,r);
  box.max = Vec3(r,r,r);  
}

//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::GetBoundSphere( Vec3 &pos,float &radius )
{
  pos = GetPos();
  radius = RADIUS;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::UpdateScale(float scale)
{ 
  if (IVariable* pPos = GetChildVar(m_pVar, "position"))
  {
    Vec3 pos;
    pPos->Get(pos);          
    pPos->Set(pos *= scale);
    UpdateObjectFromVar(); 
  }        
}



