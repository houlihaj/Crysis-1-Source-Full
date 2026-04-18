
#include "stdafx.h"
#include "VehicleData.h"

#include "VehicleXMLLoader.h"


IVariable* CVehicleData::m_pDefaults(0);
XmlNodeRef CVehicleData::m_xmlDef(0);


//////////////////////////////////////////////////////////////////////////
const IVariable* CVehicleData::GetDefaultVar()
{ 
  // force reload of default var if debugdraw is on
  static ICVar* pCVar = gEnv->pConsole->GetCVar("v_debugdraw");  
  if ((pCVar->GetIVal() == DEBUGDRAW_VEED) && m_pDefaults != 0)  
  {     
    m_pDefaults->Release();
    m_pDefaults = 0;
  }    

  if (m_pDefaults == 0)
  { 
    IVehicleData* pData = VehicleDataLoad( GetXMLDef(), VEED_DEFAULTS );
    if (!pData)
    {
      Log("GetDefaultVar: returned zero!");      
    }
    else 
      m_pDefaults = pData->GetRoot();  
  }
  return m_pDefaults; 
}


//////////////////////////////////////////////////////////////////////////
const XmlNodeRef& CVehicleData::GetXMLDef()
{ 
  // force reload of xml def if debugdraw is on
  static ICVar* pCVar = gEnv->pConsole->GetCVar("v_debugdraw");  
  if ((pCVar->GetIVal() == DEBUGDRAW_VEED) && m_xmlDef != 0)  
  { 
    m_xmlDef = 0;
  }    

  if (m_xmlDef == 0)
    m_xmlDef = GetISystem()->LoadXmlFile( VEHICLE_XML_DEF );
  return m_xmlDef;
}

//////////////////////////////////////////////////////////////////////////
IVariable* GetChildVar( const IVariable* array, const char* name, bool recursive /*=false*/)
{  
  if (array == 0)
    return 0;

  // first search top level  
  for (int i=0; i<array->NumChildVars(); ++i)
  {
    IVariable* var = array->GetChildVar(i);
    if ( 0 == strcmp(name, var->GetName()) )
      return var;
  }
  if (recursive)
  {
    for (int i=0; i<array->NumChildVars(); ++i)
    {
      if (IVariable* pVar = GetChildVar( array->GetChildVar(i), name, recursive ))
        return pVar;      
    }
  }
  return NULL;
}

//////////////////////////////////////////////////////////////////////////
IVariable* GetOrCreateChildVar(IVariable* array, const char* name, bool searchRec/*=false*/, bool createRec/*=false*/)
{
  IVariable* pRes = GetChildVar(array, name, searchRec);
  if (!pRes)
  {
    if (IVariable* pDef = GetChildVar(CVehicleData::GetDefaultVar(), name, true))
    {
      pRes = pDef->Clone(createRec);

      if (!createRec)
        pRes->DeleteAllChilds(); // VarArray clones children also with non-recursive

      array->AddChildVar(pRes);
    }
  }
  return pRes;
}

//////////////////////////////////////////////////////////////////////////
bool HasChildVar( const IVariable* array, const IVariable* child, bool recursive /*= false*/ )
{
  if (!array || !child)
    return 0;

  // first search top level  
  for (int i=0; i<array->NumChildVars(); ++i)
  { 
    if (array->GetChildVar(i) == child)
      return true;
  }
  if (recursive)
  {
    for (int i=0; i<array->NumChildVars(); ++i)
    {
      if (HasChildVar( array->GetChildVar(i), child, recursive ))
        return true;      
    }
  }
  return false;
}

//////////////////////////////////////////////////////////////////////////
IVariable* GetParentVar( IVariable* array, const IVariable* child )
{
  if (!array || !child)
    return 0;

  // first search top level  
  for (int i=0; i<array->NumChildVars(); ++i)
  { 
    if (array->GetChildVar(i) == child)
      return array;
  }  
  // search childs
  for (int i=0; i<array->NumChildVars(); ++i)
  {
    if (IVariable* parent = GetParentVar(array->GetChildVar(i), child))
      return parent;      
  }

  return 0;
}

//////////////////////////////////////////////////////////////////////////
IVariable* CreateDefaultVar( const char* name, bool rec /*= false*/ )
{
  IVariable* pDef = GetChildVar( CVehicleData::GetDefaultVar(), name, true );  
  if (!pDef)
  {
    //VeedLog("Default var <%s> not found!", name);
    return 0;
  }  
  return pDef->Clone(rec);  
}

//////////////////////////////////////////////////////////////////////////
IVariable* CreateDefaultChildOf( const char* name )
{
  IVariable* pParent = GetChildVar( CVehicleData::GetDefaultVar(), name, true );  
  if (!pParent)
  {
    Log("Default parent <%s> not found!", name);
    return 0;
  }  
  if (pParent->NumChildVars() > 0)
    return pParent->GetChildVar(0)->Clone(true);
  else
    Log("Default Parent <%s> has no children!");
  
  return 0;
}

//////////////////////////////////////////////////////////////////////////
void ReplaceChildVars( IVariable* from, IVariable* to)
{
  if (!from || !to)
    return;

  to->DeleteAllChilds();
  for (int i=0; i<from->NumChildVars(); ++i)
  {
    to->AddChildVar(from->GetChildVar(i));
  }
}

//////////////////////////////////////////////////////////////////////////
void CVehicleData::FillDefaults(IVariable* pVar, const char* defaultVar, const IVariable* pParent/*=GetDefaultVar*/)
{   
  IVariable* pDef = GetChildVar( pParent, defaultVar, true );
  if (!pDef)
  {
    Log("Default var <%s> not found!", defaultVar);
    return;
  }

  // if null, just clone
  if (!pVar)
  {
    pVar = pDef->Clone(true);
    return;
  }

  // else rec. compare children and add missing vars
  for (int i=0; i<pDef->NumChildVars(); ++i)
  {
    IVariable* pChild = GetChildVar( pVar, pDef->GetChildVar(i)->GetName() );
    if (!pChild)
    {
      // if null, just clone
      IVariable* pClone = pDef->GetChildVar(i)->Clone(true);      
      pVar->AddChildVar(pClone);
    }    
    else
    {
      // if present, descent to children
      FillDefaults( pChild, pChild->GetName(), pDef );
    }
  }

}

//////////////////////////////////////////////////////////////////////////
void DumpVariable( IVariable* pVar, int level/*=0*/)
{
  if (!pVar)
    return;

  string tab("");
  for (int i=0; i<level; ++i)
    tab.append("\t");

  CString value;
  pVar->Get(value);
  Log("%s<%s> = '%s'", tab.c_str(), pVar->GetName(), (const char*)value);

  for (int i=0; i<pVar->NumChildVars(); ++i)
    DumpVariable(pVar->GetChildVar(0), level++);
}

//////////////////////////////////////////////////////////////////////////
/*
#define CREATE_VAR_TYPE(ELSE_TYPE) \
  if (false); \
  ELSE_TYPE(Vec3); \
  ELSE_TYPE(int); \
  ELSE_TYPE(float); \
  ELSE_TYPE(CString); \
  ELSE_TYPE(bool);
    

IVariable* CreateVarFromDefNode( XmlNodeRef node )
{
  // create variable of corresponding type
  IVariable* pVar = 0;
  if (0 != strcmp(node->getTag(), "Property"))
  {
    pVar = new CVariableArray;
  }
  else 
  {
    string type = node->getAttr("type");
    if (type == "string")
      type = "CString";    

#define VAR_TYPE(varType) \
    else if (0 == strcmp(type.c_str(), #varType)) do { \
      pVar = new CVariable<varType>; \
      varType val; \
      node->getAttr("default", val); \
      pVar->Set(val); } \
    while (0)    
    CREATE_VAR_TYPE(VAR_TYPE);
#undef VAR_TYPE    
  }

  return pVar;
}
*/


