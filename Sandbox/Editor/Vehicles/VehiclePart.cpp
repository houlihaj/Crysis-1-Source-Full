
#include "StdAfx.h"
#include "VehiclePart.h"

#include "Viewport.h"

#include "VehiclePrototype.h"
#include "VehicleHelperObject.h"
#include "VehicleSeat.h"
#include "VehicleData.h"
#include "VehicleEditorDialog.h"

#include <I3DEngine.h>


//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CVehiclePart,CBaseObject)

#define RADIUS 0.05f

//////////////////////////////////////////////////////////////////////////
CVehiclePart::CVehiclePart()
{ 
  m_pVehicle = 0;  
  m_pVar = 0;
  
  m_pYawSpeed = 0;
  m_pYawLimits = 0;
  m_pPitchSpeed = 0;
  m_pPitchLimits = 0;
  m_pHelper = 0;
  m_pPartClass = 0;
}

//////////////////////////////////////////////////////////////////////////
bool CVehiclePart::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
  //SetColor( RGB(0,0,255) );
  bool res = CBaseObject::Init( ie,prev,file );

  return res;
}


void CVehiclePart::DrawRotationLimits( DisplayContext& dc, IVariable* pSpeed, IVariable* pLimits, IVariable* pHelper, int axis)
{
  if (pSpeed)
  {
    float speed = 0;
    pSpeed->Get(speed);
    
    if (speed > 0)
    {
      // get limits
      float min = 0, max = 0;
      if (pLimits && pLimits->NumChildVars() > 1)
      {
        pLimits->GetChildVar(0)->Get(min);
        pLimits->GetChildVar(1)->Get(max);
        
        if (min < -180)
          min = 0;
        if (max > 180)
          max = 0;
      }

      // get draw position: either vehicle pos or helper pos, if available
      Vec3 sourcePos = GetWorldPos();
      if (pHelper)
      {
        CString name;
        pHelper->Get(name);

        if (CBaseObject* pObj = GetIEditor()->GetObjectManager()->FindObject(name))
        {
          if (pObj->IsKindOf(RUNTIME_CLASS(CVehicleHelper)))
            sourcePos = pObj->GetWorldPos();
        }
      }

      // draw limits
      if (min || max)
      { 
        const static Vec3 line(0, 2, 0);
        const static float step = DEG2RAD(10);
        const Matrix34& vehicleTM = m_pVehicle->GetWorldTM();        
        double angleMin=0, angleMax=0, angle=0;
        Matrix34 matMin, matMax;
        Vec3 p0, p1; 
        const float radius = 0.85*line.y;
        float upper = 0;
        int i=0;

        ColorF color(0,1,0, 1);
        if (min > max)
          color = ColorF(1,0,0, 1);
                        
        switch (axis)
        {
        case AXIS_X:          
          angleMin = DEG2RAD(max > 0 ? max : 360+max); // angles for x-rotation
          angleMax = DEG2RAD(min < 0 ? 360+min : min);   

          matMin = vehicleTM.CreateRotationX(angleMin);
          matMax = vehicleTM.CreateRotationX(angleMax);  
          
          p0.y = sourcePos.y + radius*cos(angleMax-step);
          p0.z = sourcePos.z + radius*sin(angleMax-step);
          p0.x = p1.x = sourcePos.x;    

          break;

        case AXIS_Z:
          angleMin = DEG2RAD(min < 0 ? -min : 360-min); // angles for z-rotation
          angleMax = DEG2RAD(max > 0 ? 360-max : -max);

          matMin = vehicleTM.CreateRotationZ(angleMin);
          matMax = vehicleTM.CreateRotationZ(angleMax);  

          p0.x = sourcePos.x + radius*-sin(angleMax-step);
          p0.y = sourcePos.y + radius*cos(angleMax-step);
          p0.z = p1.z = sourcePos.z;   

          break;
        }

        upper = angleMin + step;
        if (upper >= gf_PI2)
          upper = upper-gf_PI2;

        angle = angleMax;    
          
        while (++i < gf_PI2/step) 
        { 
          switch (axis)
          {
          case AXIS_X:
            p1.y = sourcePos.y + radius*cos(angle);
            p1.z = sourcePos.z + radius*sin(angle);   
            break;
          case AXIS_Z:
            p1.x = sourcePos.x + radius*-sin(angle);
            p1.y = sourcePos.y + radius*cos(angle);
            break;
          }          
          dc.DrawLine( p0, p1, color, color );            
          p0 = p1;

          if (angle > upper-0.001 && angle < upper+0.001)
            break;

          if ((angle >= angleMin || (angle < step && angleMin+step>g_PI2)) && (angle <= upper || (angleMin+step > g_PI2 && angle+step>g_PI2)))
            angle = upper;
          else
          {
            angle += step;
            if (angle > g_PI2)
              angle = angle - g_PI2;
          }            
        }       
      
        Vec3 destPos = sourcePos + matMin * line;
        dc.DrawLine(sourcePos, destPos, color, color);
        
        destPos = sourcePos + matMax * line;        
        dc.DrawLine(sourcePos, destPos, color, color);
        
      }   

    }
  }
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::Display( DisplayContext &dc )
{  
  // only draw if selected
  if (!IsSelected())
    return;
  
  COLORREF wireColor,solidColor;  
  float alpha = 0.4f;  
  wireColor = dc.GetSelectedColor();
  solidColor = GetColor();    

  // mass box rendering
  if (GetPartClass() == PARTCLASS_MASS)
  {    
    dc.PushMatrix( GetWorldTM() );
    BBox box;
    GetLocalBounds(box);
    dc.SetColor( solidColor,alpha );
    dc.DepthWriteOff();
    dc.DrawSolidBox( box.min,box.max );
    dc.DepthWriteOn();

    dc.SetColor( wireColor,1 );
    dc.SetLineWidth(3.0f);
    dc.DrawWireBox( box.min,box.max );
    dc.SetLineWidth(0);  
    dc.PopMatrix();
  }
  else if (GetPartClass() == PARTCLASS_WHEEL)
  {
    // use statobj here
    IEntity* pEnt = m_pVehicle->GetCEntity()->GetIEntity();

    for (int i=0; i<pEnt->GetSlotCount(); ++i)
    {
      IStatObj* pStatObj = pEnt->GetStatObj(i);

      if (pStatObj && 0 == strcmp(pStatObj->GetGeoName(), GetName()))
      { 
        //pEnt->GetSlotWorldTM(i);
        AABB box = pStatObj->GetAABB();
        
        dc.SetColor( wireColor, 0.85f );

        dc.PushMatrix( pEnt->GetSlotWorldTM(i) );
        dc.DrawWireBox( box.min, box.max );
        dc.PopMatrix();
      }       
    }
  }
  else
  {
    // draw rotation limits, if available
    DrawRotationLimits(dc, m_pYawSpeed,   m_pYawLimits,   m_pHelper, AXIS_Z);
    DrawRotationLimits(dc, m_pPitchSpeed, m_pPitchLimits, m_pHelper, AXIS_X);  
  }
    
  DrawDefault(dc);
}

//////////////////////////////////////////////////////////////////////////
bool CVehiclePart::HitTest( HitContext &hc )
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
void CVehiclePart::GetBoundBox( BBox &box )
{
  // Transform local bounding box into world space.
  GetLocalBounds( box );
  box.SetTransformedAABB( GetWorldTM(), box );  
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::GetLocalBounds( BBox &box )
{
  // if box class, return actual size
  // todo: geometries
  IVariable* pClass, *pSubVar, *pSize;
  pClass = GetChildVar( m_pVar, "class" );
  
  if (pClass)
  {
    CString classname;
    pClass->Get(classname);
    if (classname == PARTCLASS_MASS)
    {
      if (pSubVar = GetChildVar( m_pVar, PARTCLASS_MASS ))
      {
        if (pSize = GetChildVar( pSubVar, "size"))
        {
          Vec3 size;
          pSize->Get(size);
          box.min = -size;
          box.max = size;
          return;
        }
      }
    }
  }
  
  // return vehicle bbox
  if (m_pVehicle)
    m_pVehicle->GetLocalBounds(box);  
}


//////////////////////////////////////////////////////////////////////////
CVehicleHelper* CVehiclePart::GetHelper(CString name)
{
  for (int i=0; i<GetChildCount(); ++i)
  {
    CBaseObject* child = GetChild(i);
    if (child->IsKindOf(RUNTIME_CLASS(CVehicleHelper)) && child->GetName() == name)
    {
      return (CVehicleHelper*)child;
    }
  }
  return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::UpdateObjectFromVar()
{
  if (!m_pVar)
  {
    // create default var here
    m_pVar = CreateDefaultChildOf("Parts");
    
    // remove default helper, don't want it here
    if (IVariable* pHelpers = GetChildVar(m_pVar, "Helpers"))    
      pHelpers->DeleteAllChilds();
  }
  
  // check if part has a name, otherwise set to new
  CString sName("NewPart");    
  if (IVariable* pName = GetChildVar( m_pVar, "name" ))
  {      
    pName->Get(sName);    
  }
  SetName(sName);

  // place at vehicle origin
  if (IVariable* pPos = GetChildVar(m_pVar, "position"))
  {
    pPos->RemoveOnSetCallback( functor(*this,&CVehiclePart::OnSetPos) );
		pPos->AddOnSetCallback( functor(*this,&CVehiclePart::OnSetPos) );
    OnSetPos(pPos);
  }

  // store pointers to variables that are needed for displaying etc.
  m_pYawSpeed = GetChildVar(m_pVar, "yawSpeed");
  m_pYawLimits = GetChildVar(m_pVar, "yawLimits");
  m_pPitchSpeed = GetChildVar(m_pVar, "pitchSpeed");
  m_pPitchLimits = GetChildVar(m_pVar, "pitchLimits");
  
  m_pHelper = GetChildVar(m_pVar, "helper");    
  if (!m_pHelper && GetParent())
  {
    // if not found, lookup in parent
    if (IVeedObject* pVO = IVeedObject::GetVeedObject(GetParent()))
    {
      m_pHelper = GetChildVar(pVO->GetVariable(), "helper");
    }    
  }

  m_pPartClass = GetChildVar(m_pVar, "class");
  
  SetModified();  
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::OnSetPos(IVariable* pVar)
{
  if (!m_pVehicle)
    return;

  Vec3 pos(ZERO);
  pVar->Get(pos);
  if (GetParent())
    SetPos(pos);
  else if (IEntity* pEnt = m_pVehicle->GetCEntity()->GetIEntity())
    SetPos(pEnt->GetWorldTM().TransformPoint(pos));    
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::OnSetClass(IVariable* pVar)
{  
  if (GetPartClass() == PARTCLASS_MASS)
  {
    SetHidden(false);
  } 
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::AddHelper(CVehicleHelper* pHelper, IVariable* pHelperVar)
{ 
  if (!m_pVar)
    return;

  AttachChild(pHelper, true);
  pHelper->SetPart(this);
  
  // create "Helpers" array if not present [TODO: clean this] 
  IVariable* pHelpers = GetChildVar(m_pVar, "Helpers");
  if (!pHelpers)
  {
    pHelpers = new CVariableArray;
    pHelpers->SetName("Helpers");
    m_pVar->AddChildVar(pHelpers);
  }      
  
  IVariable* pVar = pHelperVar;
  if (!pVar)
  {
    // if not present, create variable for helper and insert into variable tree [TODO: generalize that]        
    pVar = CreateDefaultChildOf("Helpers");
    pHelpers->AddChildVar(pVar);    
  }  
  pHelper->SetVariable(pVar);
  pHelper->SetVehicle(m_pVehicle);
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::RemoveChild( CBaseObject *node )
{
  CBaseObject::RemoveChild(node);
}


//////////////////////////////////////////////////////////////////////////
void CVehiclePart::SetVariable(IVariable* pVar)
{
  m_pVar = pVar;   
  if (pVar)
    UpdateObjectFromVar();    
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::AddPart(CVehiclePart* pPart)
{ 
  AttachChild(pPart, false);

  IVariable* pParts = GetOrCreateChildVar(m_pVar, "Parts");
  if (pParts)  
    pParts->AddChildVar(pPart->GetVariable());        
}


//////////////////////////////////////////////////////////////////////////
void CVehiclePart::Done()
{
  VeedLog("[CVehiclePart:Done] <%s>", GetName());   

  // remove callbacks
  if (IVariable* pPos = GetChildVar(m_pVar, "position"))  
    pPos->RemoveOnSetCallback( functor(*this,&CVehiclePart::OnSetPos) );
      
  int nChildCount = GetChildCount();
  VeedLog("[CVehiclePart]: deleting %i children..", nChildCount); 

  for (int i=nChildCount-1; i>=0; --i)
  {     
    CBaseObject* pChild = GetChild(i);  
    VeedLog("[CVehiclePart]: deleting %s", pChild->GetName()); 
    GetIEditor()->DeleteObject( GetChild(i) );
  }  

  // here Listeners are notified of deletion
  // ie. here parents must erase child's variable ptr, not before
  CBaseObject::Done(); 
  
}


//////////////////////////////////////////////////////////////////////////
void CVehiclePart::AttachChild( CBaseObject* child, bool bKeepPos)
{
  child->AddEventListener( functor(*this,&CVehiclePart::OnObjectEvent) );

  CBaseObject::AttachChild(child, bKeepPos);  
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::OnObjectEvent( CBaseObject *node, int event )
{
  if (event == CBaseObject::ON_DELETE)
  {    
    VeedLog("[CVehiclePart]: ON_DELETE for %s", node->GetName());
    // delete variable
    if (IVeedObject* pVO = IVeedObject::GetVeedObject(node))
    { 
      if (pVO->DeleteVar())
      {
        if (m_pVar && !m_pVar->DeleteChildVar(pVO->GetVariable(), true))
        {
          // if not found, delete in top-level, because not all children attached 
          // to the part have to keep their variable beneath it        
          m_pVehicle->GetVariable()->DeleteChildVar(pVO->GetVariable(), true);
        }
        pVO->SetVariable(0);
        VeedLog("[CVehiclePart] deleting var for %s", node->GetName());
      }
    }  
  }
}


//////////////////////////////////////////////////////////////////////////
void CVehiclePart::UpdateVarFromObject()
{ 
  if (!m_pVar)
    return;

  IEntity* pEnt = m_pVehicle->GetCEntity()->GetIEntity();
  if (!pEnt)
  {
    Warning("[CVehiclePart::UpdateVariable] pEnt is null, returning");
    return;
  }

  IVariable* pVar = GetChildVar(m_pVar, "name");
  if (!pVar)
    Log("ChildVar <name> not found in Part!");
  else
    pVar->Set(GetName());

  // update pos 
  if (IVariable* pVar = GetChildVar(m_pVar, "position"))
  {
    Vec3 pos = pEnt->GetWorldTM().GetInvertedFast().TransformPoint( GetWorldPos() );
    pVar->Set(pos);    
  }

  // update size
  if (GetPartClass() == PARTCLASS_MASS)
  {
    if (IVariable* pSize = GetChildVar(m_pVar, "size", true))
    {     
      BBox box;
      GetLocalBounds(box);        
      pSize->Set( box.max.CompMul(GetScale()) );
      SetScale(Vec3(1,1,1));
    }
  }  
}

//////////////////////////////////////////////////////////////////////////
int CVehiclePart::GetIconIndex()
{
  if (GetPartClass() == PARTCLASS_WHEEL)
    return VEED_WHEEL_ICON;
  else
    return VEED_PART_ICON; 
}

//////////////////////////////////////////////////////////////////////////
CString CVehiclePart::GetPartClass()
{
  CString name("");
  
  if (!m_pPartClass)  
    m_pPartClass = GetChildVar(m_pVar, "class");
    
  if (m_pPartClass)
    m_pPartClass->Get(name);
  
  return name;
}

//////////////////////////////////////////////////////////////////////////
bool CVehiclePart::IsLeaf()
{
  CString partClass = GetPartClass();

  if (partClass == PARTCLASS_WHEEL 
   || partClass == PARTCLASS_LIGHT
   || partClass == PARTCLASS_TREAD
   || partClass == PARTCLASS_MASS)      
  {
    return true;
  }  
  
  return false;
}


//////////////////////////////////////////////////////////////////////////
void CVehiclePart::UpdateScale(float scale)
{ 
  Vec3 vec;

  if (IVariable* pVar = GetChildVar(m_pVar, "position"))
  { 
    pVar->Get(vec);          
    pVar->Set(vec *= scale);    
  }        
  if (IVariable* pVar = GetChildVar(m_pVar, "positionOffset"))
  {    
    pVar->Get(vec);          
    pVar->Set(vec *= scale);    
  }

  if (GetPartClass() == PARTCLASS_MASS)
  {
    IVariable* pSub = GetChildVar(m_pVar, "MassBox");
    if (IVariable* pVar = GetChildVar(pSub, "size"))
    {    
      pVar->Get(vec);          
      pVar->Set(vec *= scale);    
    }
  }

  UpdateObjectFromVar(); 
}


//////////////////////////////////////////////////////////////////////////
void CVehiclePart::OnTreeSelection()
{ 
}
