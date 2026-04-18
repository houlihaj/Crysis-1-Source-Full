////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2005.
// -------------------------------------------------------------------------
//  File name:   propertyctrlext.cpp
//  Version:     v1.00
//  Created:     19-10-2005 by MichaelR.
//  Compilers:   Visual Studio.NET
//  Description: Implementation of CPropertyCtrlExt.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "PropertyCtrlExt.h"

#include "Objects/Entity.h"
#include "Objects/SelectionGroup.h"
#include "Controls/PropertyItem.h"
#include "VehicleData.h"

// CPropertyCtrlExt

IMPLEMENT_DYNAMIC(CPropertyCtrlExt, CPropertyCtrl)


BEGIN_MESSAGE_MAP(CPropertyCtrlExt, CPropertyCtrl)
  ON_WM_RBUTTONUP()  
END_MESSAGE_MAP()


CPropertyCtrlExt::CPropertyCtrlExt()
: CPropertyCtrl()
{  
  m_preSelChangeFunc = 0;
}


void CPropertyCtrlExt::OnRButtonUp(UINT nFlags, CPoint point)
{ 
  // Popup Menu with Event selection.
  CMenu menu;
  menu.CreatePopupMenu();
  //menu.AppendMenu( MF_SEPARATOR,0,_T("") );

  CPropertyItem* pItem = 0;
    
  if (m_multiSelectedItems.size() == 1)
  { 
    pItem = m_multiSelectedItems[0];
    if (!pItem || !pItem->GetVariable()) 
      return;

    IVariable* pVar = pItem->GetVariable();
        
    if (pVar->GetDataType() == IVariable::DT_EXTARRAY)    
    {      
      // if clicked on an extendable array
      if (pVar->GetUserData())
      {
        string name("Add ");
        name += static_cast<IVariable*>(pVar->GetUserData())->GetName();  
        menu.AppendMenu( MF_STRING, 1, name );
      }        
    }
    else if (pItem->GetParent() && pItem->GetParent()->GetVariable() 
      && pItem->GetParent()->GetVariable()->GetDataType() == IVariable::DT_EXTARRAY)
    {
      // if clicked on child of extendable array
      string name("Delete ");
      name += pVar->GetName();
      menu.AppendMenu( MF_STRING, 2, name );
    }
    else if (0 == strcmp(pVar->GetName(), "effect"))
    {
      menu.AppendMenu( MF_STRING, 3, "Get effect from selection" );
    }
  }

  CPoint p;
  ::GetCursorPos(&p);
  int res = ::TrackPopupMenuEx( menu.GetSafeHmenu(),TPM_LEFTBUTTON|TPM_RETURNCMD,p.x,p.y,GetSafeHwnd(),NULL );
  switch (res)
  {
  case 1:    
    OnAddChild();
    break;
  case 2:
    OnDeleteChild(pItem);
  case 3:
    OnGetEffect(pItem);
  }
  
  //	CWnd::OnRButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlExt::OnAddChild()
{
  if (!m_multiSelectedItems.size() == 1)
    return;

  CPropertyItem* pItem = m_multiSelectedItems[0];  
  IVariable* pVar = pItem->GetVariable();
  IVariable* pClone = 0;

  if (pVar->GetUserData())
  {
    pClone = static_cast<IVariable*>(pVar->GetUserData())->Clone(true);  
    pVar->AddChildVar(pClone);

    CPropertyItemPtr pNewItem = new CPropertyItem(this);
    pItem->AddChild(pNewItem);
    pNewItem->SetVariable(pClone);

    Invalidate();
  }    
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlExt::OnDeleteChild(CPropertyItem* pItem)
{
  // remove item and variable
  CPropertyItem* pParent = pItem->GetParent();  
  IVariable* pVar = pItem->GetVariable();

  pItem->DestroyInPlaceControl();
  pParent->RemoveChild(pItem);
  pParent->GetVariable()->DeleteChildVar(pVar);

  Invalidate();
}



//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlExt::SelectItem( CPropertyItem *item )
{
  // call PreSelect callback
  if (m_preSelChangeFunc)
    m_preSelChangeFunc( item );

  CPropertyCtrl::SelectItem( item );
}


//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlExt::OnItemChange( CPropertyItem *item )
{
  CPropertyCtrl::OnItemChange(item); 

  // handle class-like items that need to insert/remove subitems
  if (item->GetType() == ePropertySelection && item->GetName() == "class")
  {
    IVariable* pVar = item->GetVariable();

    CString classname;
    pVar->Get(classname);
    
    // get parent var/item of class item
    CPropertyItem* pClassItem = item;
    if (!pClassItem)
      return;

    CPropertyItem* pParentItem = pClassItem->GetParent();

    assert(pParentItem);
    if (!pParentItem)
      return;
    
    if (IVariablePtr pMainVar = pParentItem->GetVariable())
    {       
      // first remove child vars that belong to other classes
      if (IVarEnumList* pEnumList = pVar->GetEnumList())
      {
        CPropertyItemPtr pSubItem = 0;

        for (int i=0; i<pEnumList->GetItemsCount(); ++i)
        { 
          // if sub variables for enum classes are found
          // delete their property items and variables
          if (IVariable* pSubVar = GetChildVar(pMainVar, pEnumList->GetItemName(i)))
          {
            pSubItem = pParentItem->FindItemByVar(pSubVar);
            if (pSubItem)
            {
              pParentItem->RemoveChild(pSubItem);
              pMainVar->DeleteChildVar(pSubVar);
            }            
          }
        }

        // now check if we have extra properties for that part class
        if (IVariable* pSubProps = CreateDefaultVar(classname, true))  
        {           
          // add new PropertyItem
          pSubItem = new CPropertyItem(this);
          pParentItem->AddChild(pSubItem); 
          pSubItem->SetVariable(pSubProps);

          pMainVar->AddChildVar(pSubProps);

          pSubItem->SetExpanded(true);
        }
        Invalidate();
      }
    }     
         
  }
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlExt::OnGetEffect(CPropertyItem* pItem)
{
  CSelectionGroup *pSel = GetIEditor()->GetSelection();
  for (int i = 0; i < pSel->GetCount(); ++i)
  {
    CBaseObject* pObject = pSel->GetObject(i);
    if (pObject->IsKindOf(RUNTIME_CLASS(CEntity)))
    {
      CEntity *pEntity = (CEntity*)pObject;
      if (pEntity->GetProperties())
      {
        IVariable *pVar = pEntity->GetProperties()->FindVariable( "ParticleEffect" );
        if (pVar)
        {
          CString effect;
          pVar->Get(effect);
          
          pItem->SetValue(effect);

          break;
        }
      }
    }
  }
}

