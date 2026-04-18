
#include "stdafx.h"
#include "VehicleMovementPanel.h"

#include "Controls\PropertyItem.h"

#include "VehicleData.h"
#include "VehicleEditorDialog.h"
#include "VehiclePrototype.h"


IMPLEMENT_DYNAMIC(CVehicleMovementPanel,CWnd)

CVehicleMovementPanel::CVehicleMovementPanel(CVehicleEditorDialog* pDialog)	
: m_pVehicle( 0 )
{	
  assert(pDialog); // dialog is needed
  m_pDialog = pDialog;  
  
}

CVehicleMovementPanel::~CVehicleMovementPanel()
{
}

BEGIN_MESSAGE_MAP(CVehicleMovementPanel, CWnd)
  ON_WM_CREATE()
  ON_WM_SIZE()
END_MESSAGE_MAP()



void CVehicleMovementPanel::ExpandProps(CPropertyItem* pItem, bool expand/*=true*/)
{
  // expand all children and their children
  for (int i=0; i<pItem->GetChildCount(); ++i)
  {    
    CPropertyItem* pChild = pItem->GetChild(i);
    m_propsCtrl.Expand(pChild, true);

    for (int j=0; j<pChild->GetChildCount(); ++j)
    {
      m_propsCtrl.Expand(pChild->GetChild(j), true);
    } 
  }
}


void CVehicleMovementPanel::OnMovementTypeChange(IVariable* pVar)
{
  // ask if user is sure? :)
    
  m_movementBlock.Clear();
  
  // load defaults
  const IVariable* pDefaults = CVehicleData::GetDefaultVar();  
  IVariable* pRoot = m_pVehicle->GetVariable();
        
  if (IVariable* pParams = GetChildVar(pDefaults, "MovementParams"))
  {
    CString type;
    pVar->Get(type);
    Log("looking up defaults for movement type %s", type);
    if (IVariable* pType = GetChildVar(pParams, type))
    { 
      IVariable* pNewType = pType->Clone(true);

      // fetch property item holding the type var
      IVariable* pMoveParams = GetChildVar(pRoot, "MovementParams");
      CPropertyItemPtr pTypeItem = 0;
      if (pMoveParams && pMoveParams->NumChildVars() > 0)
      {
        // delete old var/item
        IVariable* pOldType = pMoveParams->GetChildVar(0);      
        pTypeItem = m_propsCtrl.GetRootItem()->FindItemByVar(pMoveParams)->FindItemByVar(pOldType);        
        assert(pTypeItem != NULL);                  
        
        pTypeItem->RemoveAllChildren();
        pMoveParams->DeleteChildVar(pOldType);       
      }
      else
      {
        pTypeItem = new CPropertyItem(&m_propsCtrl);
        m_propsCtrl.GetRootItem()->FindItemByVar(pMoveParams)->AddChild(pTypeItem);        
      }
      pTypeItem->SetVariable(pNewType);
			if (pMoveParams)
				pMoveParams->AddChildVar(pNewType);

      // expand "Wheeled" subtable if present
      if (IVariable* pWheeledVar = GetChildVar(pNewType, "Wheeled"))      
        pTypeItem->FindItemByVar(pWheeledVar)->SetExpanded(true);
    }
  }

  m_propsCtrl.Invalidate();
    
}

void CVehicleMovementPanel::UpdateVehiclePrototype(CVehiclePrototype* pProt)
{
  assert(pProt);
  m_pVehicle = pProt;          
  m_propsCtrl.DeleteAllItems();  
  m_propsCtrl.SetPreSelChangeCallback( functor(*m_pDialog, &CVehicleEditorDialog::OnPropsSelChanged));

  m_typeBlock.Clear();
  m_movementBlock.Clear();
  m_physicsBlock.Clear();
  
  // get data from root var  
  IVariable* pRoot = pProt->GetVariable();
  
  // add enum for selecting movement type
  CVariableEnum<CString>* pTypeVar = new CVariableEnum<CString>;    
  pTypeVar->SetName("Type");

  { 
    // fetch available movement types
    const XmlNodeRef& def = CVehicleData::GetXMLDef();
    XmlNodeRef node;
    for (int i=0; i<def->getChildCount(); ++i)
    {
      node = def->getChild(i);
      if (0 == strcmp("MovementParams", node->getAttr("name")))
      {
        XmlNodeRef moveNode;
        for (int j=0; j<node->getChildCount(); j++)
        { 
          CString name = node->getChild(j)->getAttr("name");
          pTypeVar->AddEnumItem( name, name );
        }
        break;
      }
    }
    
    m_typeBlock.AddVariable(pTypeVar, NULL);
    
    CPropertyItem* pItem = m_propsCtrl.AddVarBlock( &m_typeBlock );
    ExpandProps(pItem);            
  }
 
  // add movementParams
  if (IVariable* pVar = GetChildVar(pRoot, "MovementParams"))
  { 
    m_movementBlock.AddVariable(pVar, "MovementParams");
    CPropertyItem* pItem = m_propsCtrl.AddVarBlock( &m_movementBlock );
    ExpandProps(pItem);
    
    // set type enum to actual type
    if (pVar->NumChildVars() > 0)
    {      
      CString sType = pVar->GetChildVar(0)->GetName();      
      pTypeVar->Set(sType);

      // expand "Wheeled" subtable if present
      if (IVariable* pWheeledVar = GetChildVar(pVar->GetChildVar(0), "Wheeled"))      
        pItem->FindItemByVar(pWheeledVar)->SetExpanded(true);
    }
    
  }
  
  if (IVariable* pVar = GetChildVar(pRoot, "Physics"))
  { 
    m_physicsBlock.AddVariable(pVar, NULL);
    CPropertyItem* pItem = m_propsCtrl.AddVarBlock( &m_physicsBlock );
    ExpandProps(pItem);
  }   
  
	m_typeBlock.AddOnSetCallback( functor(*this,&CVehicleMovementPanel::OnMovementTypeChange) );

  m_propsCtrl.Invalidate(TRUE);
}

int CVehicleMovementPanel::OnCreate(LPCREATESTRUCT lpCreateStruct)
{    
  int nRes = __super::OnCreate(lpCreateStruct);
  
  m_propsCtrl.Create( WS_CHILD|WS_VISIBLE, CRect(0,0,300,350), this );  
  m_propsCtrl.ExpandAll();    

  return nRes;  
}


void CVehicleMovementPanel::OnSize(UINT nType, int cx, int cy) 
{
  __super::OnSize(nType, cx, cy);

  CRect rc;
  GetClientRect( rc );
  
  if (m_propsCtrl.m_hWnd)
    m_propsCtrl.MoveWindow( 0, 0, rc.right, rc.bottom, true );
}



