////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   MaterialDialog.h
//  Version:     v1.00
//  Created:     22/1/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __materialdialogdialog_h__
#define __materialdialogdialog_h__
#pragma once

#include "BaseLibraryDialog.h"
#include "Controls\SplitterWndEx.h"
#include "Controls\TreeCtrlEx.h"
#include "Controls\PropertyCtrl.h"
#include "Controls\PropertyCtrlEx.h"
#include "Controls\PreviewModelCtrl.h"
#include "MaterialImageListCtrl.h"
#include "MaterialBrowser.h"

class CMaterial;
class CMaterialManager;
class CMatEditPreviewDlg;
class CMaterialSender;

/** Dialog which hosts entity prototype library.
*/

class CMaterialDialog : public CBaseLibraryDialog, public IMaterialBrowserListener
{
	DECLARE_DYNCREATE(CMaterialDialog)
public:
	CMaterialDialog();
	~CMaterialDialog();

	static void RegisterViewClass();

	virtual UINT GetDialogMenuID();

public:
	afx_msg void OnAssignMaterialToSelection();
	afx_msg void OnResetMaterialOnSelection();
	afx_msg void OnGetMaterialFromSelection();

protected:
	virtual void PostNcDestroy();
	void DoDataExchange(CDataExchange* pDX);
	BOOL OnInitDialog();
	
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	
	afx_msg void OnAddItem();
	afx_msg void OnUpdateMtlSelected( CCmdUI* pCmdUI );
	afx_msg void OnUpdateObjectSelected( CCmdUI* pCmdUI );
	afx_msg void OnUpdateAssignMtlToSelection( CCmdUI *pCmdUI );
	afx_msg void OnPickMtl();
	afx_msg void OnUpdatePickMtl( CCmdUI* pCmdUI );
	afx_msg void OnCopy();
	afx_msg void OnPaste();
	afx_msg void OnGenCubemap();
	afx_msg void OnMaterialPreview();
	afx_msg void OnSelectAssignedObjects();
	afx_msg void OnChangedBrowserListType();

	//////////////////////////////////////////////////////////////////////////
	// Some functions can be overriden to modify standart functionality.
	//////////////////////////////////////////////////////////////////////////
	virtual void InitToolbar( UINT nToolbarResID );
	virtual HTREEITEM InsertItemToTree( CBaseLibraryItem *pItem,HTREEITEM hParent );
	virtual void SelectItem( CBaseLibraryItem *item,bool bForceReload=false );
	virtual void DeleteItem( CBaseLibraryItem *pItem );
	virtual bool SetItemName( CBaseLibraryItem *item,const CString &groupName,const CString &itemName );
	virtual void ReloadItems();

	//////////////////////////////////////////////////////////////////////////
	// IMaterialBrowserListener implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnBrowserSelectItem( IDataBaseItem* pItem,bool bForce );
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IEditorNotifyListener listener implementation
	//////////////////////////////////////////////////////////////////////////
	virtual void OnEditorNotifyEvent( EEditorNotifyEvent event );
	//////////////////////////////////////////////////////////////////////////

	void OnIdleUpdate();

	//////////////////////////////////////////////////////////////////////////
	CMaterial* GetSelectedMaterial();
	void OnUpdateProperties( IVariable *var );

	void UpdateShaderParamsUI( CMaterial *pMtl );

	void UpdatePreview();

	//void SetTextureVars( CVariableArray *texVar,CMaterial *mtl,int id,const CString &name );
	void SetMaterialVars( CMaterial *mtl );

	enum EDrawType
	{
		DRAW_BOX,
		DRAW_SPHERE,
		DRAW_TEAPOT,
		DRAW_SELECTION,
	};
	

	DECLARE_MESSAGE_MAP()

	CMaterialBrowserCtrl m_wndMtlBrowser;
	CSplitterWndEx m_wndVSplitter;
	CSplitterWndEx m_wndRightSplitter;
	CDlgStatusBar m_statusBar;
	// Library control.
	CComboBox m_listTypeCtrl;
	
	CPreviewModelCtrl * m_pPreviewCtrl;
	CPropertyCtrlEx m_propsCtrl;
	CImageList m_imageList;
	CDialog m_emptyDlg;

	CImageList *m_dragImage;

	// Material manager.
	CMaterialManager *m_pMatManager;

	CVarBlockPtr m_vars;
	CVarBlockPtr m_publicVars;
	CPropertyItem *m_publicVarsItems;
	CVarBlockPtr m_shaderGenParamsVars;
	CPropertyItem *m_shaderGenParamsVarsItem;

  // Material layers presets vars
  CVarBlockPtr m_varsMtlLayersPresets;          
  CPropertyItem *m_varsMtlLayersPresetsItems;   

  // Material layers shader vars
  CVarBlockPtr m_varsMtlLayersShader[MTL_LAYER_MAX_SLOTS];
  CPropertyItem *m_varsMtlLayersShaderItems[MTL_LAYER_MAX_SLOTS];

  // Material layers shader params vars
  CVarBlockPtr m_varsMtlLayersShaderParams[MTL_LAYER_MAX_SLOTS];
  CPropertyItem *m_varsMtlLayersShaderParamsItems[MTL_LAYER_MAX_SLOTS];

  class CMaterialUI *m_pMaterialUI;

	HTREEITEM m_hDropItem;
	HTREEITEM m_hDraggedItem;

	TSmartPtr<CMaterial> m_pDraggedMtl;
	TSmartPtr<CMaterial> m_pCurrentMaterial;

	CMatEditPreviewDlg * m_pPreviewDlg;

	CMaterialImageListCtrl m_mtlImageListCtrl;

	float m_lastUpdateTime;
};

#endif // __materialdialogdialog_h__
