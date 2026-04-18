
#include "StdAfx.h"
#include "VehicleComp.h"

#include "VehiclePrototype.h"
#include "VehicleData.h"
#include "VehicleEditorDialog.h"
#include "Viewport.h"

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CVehicleComponent,CBaseObject)


//////////////////////////////////////////////////////////////////////////
CVehicleComponent::CVehicleComponent()
{ 
  m_pVehicle = 0;  
  m_pVar = 0;
  m_pMinBound = 0;
  m_pMaxBound = 0;  
  m_partBounds = false;
}


//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::Display( DisplayContext &dc )
{  
  // only draw if selected
  if (!IsSelected())
    return;
  
  COLORREF wireColor,solidColor;  
  float alpha = 0.4f;  
  wireColor = dc.GetSelectedColor();
  solidColor = GetColor();    
  
  dc.PushMatrix( GetWorldTM() );
  BBox box;
  GetLocalBounds(box);
  dc.SetColor( solidColor,alpha );
  //dc.DepthWriteOff();
  dc.DrawSolidBox( box.min,box.max );
  //dc.DepthWriteOn();

  dc.SetColor( wireColor,1 );
  dc.SetLineWidth(3.0f);
  dc.DrawWireBox( box.min,box.max );
  dc.SetLineWidth(0);  
  dc.PopMatrix();
    
  DrawDefault(dc);
}

//////////////////////////////////////////////////////////////////////////
bool CVehicleComponent::HitTest( HitContext &hc )
{
  if (!IsSelected())
    return false;

  // select on edges 
  Vec3 p;    
  Matrix34 invertWTM = GetWorldTM();
  Vec3 worldPos = invertWTM.GetTranslation();
  invertWTM.Invert();

  Vec3 xformedRaySrc = invertWTM.TransformPoint(hc.raySrc);
  Vec3 xformedRayDir = invertWTM.TransformVector(hc.rayDir).GetNormalized();

  float epsilonDist = hc.view->GetScreenScaleFactor( worldPos ) * 0.01f;
  float hitDist;

  float tr = hc.distanceTollerance/2 + 1;
  BBox box, mbox;  
  GetLocalBounds(mbox);
  box.min = mbox.min - Vec3(tr+epsilonDist,tr+epsilonDist,tr+epsilonDist);
  box.max = mbox.max + Vec3(tr+epsilonDist,tr+epsilonDist,tr+epsilonDist);
  if (box.IsIntersectRay( xformedRaySrc,xformedRayDir,p ))
  {
    if (mbox.RayEdgeIntersection( xformedRaySrc,xformedRayDir,epsilonDist,hitDist,p ))
    {
      hc.dist = xformedRaySrc.GetDistance(p);
      return true;
    }
  }
  return false;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::GetBoundBox( BBox &box )
{   
  // Transform local bounding box into world space.
  GetLocalBounds( box );    
  box.SetTransformedAABB( GetWorldTM(), box );  
}

//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::GetLocalBounds( BBox &box )
{ 
  if (m_partBounds)
  {
    // currently we return zero, cause we don't have access to parts' bounds
    box.min.zero();
    box.max.zero();
  }
  else if (m_min.GetLengthSquared() == 0 && m_max.GetLengthSquared() == 0)
  {
    // if bounds zero, use vehicle bounds
    if (m_pVehicle)
    {
      m_pVehicle->GetLocalBounds(box);      
      box.min = box.min.CompMul(GetScale());
      box.max = box.max.CompMul(GetScale());
    }
  }
  else
  {
    box.min = m_min.CompMul(GetScale());
    box.max = m_max.CompMul(GetScale());
  }
  
}


//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::UpdateObjectFromVar()
{
  if (!m_pVar)
  {
    // create default var here
    m_pVar = CreateDefaultChildOf("Components");
  }
  
  // check if Component has a name, otherwise set to new
  CString sName("NewComponent");    
  if (IVariable* pName = GetChildVar( m_pVar, "name" ))
  {      
    pName->Get(sName);    
  }
  SetName(sName);
  
  // store pointers to variables that are needed for displaying etc.
  m_pMinBound = GetChildVar(m_pVar, "minBound");
  m_pMaxBound = GetChildVar(m_pVar, "maxBound");   

  m_partBounds = false;
  if (IVariable* pPartBounds = GetChildVar(m_pVar, "useBoundsFromParts"))
  {
    pPartBounds->Get(m_partBounds);
  }
    
  // place component in bbox center
  Matrix34 mat = m_pVehicle->GetWorldTM();

  if (m_pMinBound && m_pMaxBound)
  {    
    m_pMinBound->Get(m_min);
    m_pMaxBound->Get(m_max);

    Vec3 center = 0.5f * (m_min + m_max);
    
    // store min and max relative to center
    m_min = m_min - center;
    m_max = m_max - center;

    if (!(m_min.x<=m_max.x && m_min.y<=m_max.y && m_min.z<=m_max.z))
      Warning("[VehicleComp] Invalid bounds (%s)", GetName());
        
    mat.SetTranslation( mat.TransformPoint(center) ); 
  }  
  SetWorldTM(mat);    
    
  SetModified();  
}



//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::SetVariable(IVariable* pVar)
{
  m_pVar = pVar;   
  if (pVar)
    UpdateObjectFromVar();    
}



//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::Done()
{
  VeedLog("[CVehicleComponent:Done] <%s>", GetName());   
 
  // here Listeners are notified of deletion
  // ie. here parents must erase child's variable ptr, not before
  CBaseObject::Done();   
}


//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::AttachChild( CBaseObject* child, bool bKeepPos)
{
	child->AddEventListener( functor(*this,&CVehicleComponent::OnObjectEvent) );

  CBaseObject::AttachChild(child, bKeepPos);
}

//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::OnObjectEvent( CBaseObject *node, int event )
{ 
}

//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::PostClone( CBaseObject *pFromObject,CObjectCloneContext &ctx )
{
  CBaseObject *pFromParent = pFromObject->GetParent();
  if (pFromParent)
  {
    CBaseObject *pChildParent = ctx.FindClone( pFromParent );
    if (pChildParent)
      pChildParent->AttachChild( this,false );
    else
    {
      // component was cloned and attached to same parent       
      if (pFromParent->IsKindOf(RUNTIME_CLASS(CVehiclePrototype))) 
        ((CVehiclePrototype*)pFromParent)->AddComponent(this);
      else
        pFromParent->AttachChild(this, false);
    }
  }
}



//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::UpdateVarFromObject()
{ 
  if (!m_pVar || !m_pVehicle)
    return;

  IVariable* pVar = GetChildVar(m_pVar, "name");
  if (!pVar)
    Log("ChildVar <name> not found in Component!");
  else
    pVar->Set(GetName());

  // update bounds
  if (pVar = GetChildVar(m_pVar, "useBoundsFromParts"))
    pVar->Get(m_partBounds);

  if (m_partBounds)
  {
    // set bounds to zero in case partBounds are used
    m_pMinBound->Set(Vec3(ZERO));
    m_pMaxBound->Set(Vec3(ZERO));
  }
  else
  {
    // if bounds are used, or the scale was changed, update vars
    if (m_min.GetLengthSquared() + m_max.GetLengthSquared() > 0 || GetScale() != Vec3(1,1,1))
    {
      BBox box;
      GetLocalBounds(box);
      box.min = GetWorldTM()*box.min;
      box.max = GetWorldTM()*box.max;
      
      // transform to vehicle space
      Matrix34 wTMInv = m_pVehicle->GetCEntity()->GetIEntity()->GetWorldTM().GetInverted();
      box.min = wTMInv.TransformPoint(box.min);
      box.max = wTMInv.TransformPoint(box.max);

      if (!(box.min.x<=box.max.x && box.min.y<=box.max.y && box.min.z<=box.max.z))
        Warning("[VehicleComp] Invalid bounds (%s)", GetName());

      m_pMinBound->Set(box.min);
      m_pMaxBound->Set(box.max);
    }

    // reset scale
    if (m_min.IsZero() && m_max.IsZero())
      SetScale(Vec3(1,1,1));     
  }

  // update m_min and m_max
  m_pMinBound->Get(m_min);
  m_pMaxBound->Get(m_max);
  Vec3 center = 0.5f * (m_min + m_max);
  m_min = m_min - center;
  m_max = m_max - center;

  if (!(m_min.x<=m_max.x && m_min.y<=m_max.y && m_min.z<=m_max.z))
    Warning("[VehicleComp] Invalid bounds (%s)", GetName());

  UpdateObjectFromVar();
}

//////////////////////////////////////////////////////////////////////////
int CVehicleComponent::GetIconIndex()
{  
  return VEED_COMP_ICON; 
}

//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::UpdateScale(float scale)
{ 
  Vec3 vec;

  if (IVariable* pVar = GetChildVar(m_pVar, "minBound"))
  { 
    pVar->Get(vec);          
    pVar->Set(vec *= scale);    
  }        
  if (IVariable* pVar = GetChildVar(m_pVar, "maxBound"))
  { 
    pVar->Get(vec);          
    pVar->Set(vec *= scale);    
  }        

  UpdateObjectFromVar();
}