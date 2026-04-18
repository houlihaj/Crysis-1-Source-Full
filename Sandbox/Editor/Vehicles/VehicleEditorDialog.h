////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   VehicleEditorDialog.h
//  Version:     v1.00
//  Created:     02-06-2005 by MichaelR.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __VehicleEditorDialog_h__
#define __VehicleEditorDialog_h__
#pragma once

#include <list>

#include "ToolbarDialog.h"
#include "Controls\PropertyCtrl.h"
//#include "Controls\SplitterWndEx.h"

#include "VehicleDialogComponent.h"

class CVehiclePrototype;
class CVehicleMovementPanel;
class CVehiclePartsPanel;
class CVehicleFXPanel;



//////////////////////////////////////////////////////////////////////////
//
// Main Dialog for Veed, the Vehicle Editor.
//
//////////////////////////////////////////////////////////////////////////
class CVehicleEditorDialog : public CXTPFrameWnd
{
  DECLARE_DYNCREATE(CVehicleEditorDialog)
public:
  static void RegisterViewClass();
  
  CVehicleEditorDialog();
  ~CVehicleEditorDialog();

  BOOL Create( DWORD dwStyle,const RECT& rect,CWnd* pParentWnd );  
  void RepositionPropertiesCtrl();
    
  CXTPTaskPanel& GetTaskPanel() { return m_taskPanel; }
  CVehiclePartsPanel* GetPartsPanel() { return m_partsPanel; }
  CVehicleMovementPanel* GetMovementPanel() { return m_movementPanel; }

  enum { IDD = IDD_TRACKVIEWDIALOG };

  // vehicle logic
  void SetVehiclePrototype( CVehiclePrototype* pProt );  
  CVehiclePrototype* GetVehiclePrototype(){ return m_pVehicle; }  
  
  bool ApplyToVehicle( CString filename = "" );  
  
  void OnPrototypeEvent( CBaseObject *object, int event );
  void OnEntityEvent( CBaseObject *object, int event );
  
  void RecalcLayout( BOOL bNotify = TRUE );
  void Close();

  void GetObjectsByClass(const CRuntimeClass* pClass, std::vector<CBaseObject*>& objects);

protected:
  DECLARE_MESSAGE_MAP()  
  virtual void OnOK() {};
  virtual void OnCancel() {};

  virtual BOOL PreTranslateMessage(MSG* pMsg);
  virtual BOOL OnInitDialog();
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual void PostNcDestroy();  
  afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg void OnSetFocus(CWnd* pOldWnd);
  afx_msg void OnDestroy();
  afx_msg void OnClose();  
  //afx_msg BOOL OnEraseBkgnd(CDC* pDC);
  afx_msg void OnFileNew();
  afx_msg void OnFileOpen();
  afx_msg void OnFileSave();
  afx_msg void OnFileSaveAs();
  afx_msg LRESULT OnDockingPaneNotify(WPARAM wParam, LPARAM lParam);    
  afx_msg LRESULT OnTaskPanelNotify(WPARAM wParam, LPARAM lParam);    
  
  afx_msg void OnMovementEdit();  
  afx_msg void OnPartsEdit();  
  afx_msg void OnFXEdit();  
  afx_msg void OnModsEdit();
  afx_msg void OnApplyChanges();
    
  void CreateTaskPanel();  
  CXTPDockingPaneManager* GetDockingPaneManager() { return &m_paneManager; }
  
  void OnPropsSelChanged(CPropertyItem* pItem);
  void DeleteEditorObjects();  
  void EnableEditingLinks(bool enable);
  bool OpenVehicle(bool silent = false);
  
private:  
  
  // data 
  CVehiclePrototype* m_pVehicle;  
  
  // dialog stuff  
  CXTPDockingPaneManager m_paneManager;    
  CXTPTaskPanel m_taskPanel;
  CXTPDockingPane* m_pTaskDockPane; 
    
  typedef std::list<IVehicleDialogComponent*> TVeedComponent;  
  TVeedComponent m_panels;
  CVehicleMovementPanel* m_movementPanel;
  CVehiclePartsPanel* m_partsPanel;
  CVehicleFXPanel* m_FXPanel;


  // Map of library items to tree ctrl.  
  TPartToTreeMap m_partToTree;

  CImageList m_taskImageList;

  friend class CVehiclePartsPanel;      
  friend class CVehicleFXPanel;
  friend class CVehicleMovementPanel;
};

#endif // __VehicleEditorDialog_h__
