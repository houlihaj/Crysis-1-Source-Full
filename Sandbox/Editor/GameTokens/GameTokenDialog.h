////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   GameTokenDialog.h
//  Version:     v1.00
//  Created:     10/11/2003 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __GameTokenDialog_h__
#define __GameTokenDialog_h__
#pragma once

#include "BaseLibraryDialog.h"
#include "Controls\SplitterWndEx.h"
#include "Controls\TreeCtrlEx.h"
#include "Controls\PropertyCtrl.h"
#include "Controls\PreviewModelCtrl.h"
#include "Controls/ListCtrlEx.h"

class CGameTokenItem;
class CGameTokenManager;

class CGameTokenTreeContainerDialog : public CToolbarDialog
{
public:
	CTreeCtrl *m_pTreeCtrl;

public:
	enum { IDD = IDD_DATABASE };
	CGameTokenTreeContainerDialog() : CToolbarDialog(IDD) { m_pTreeCtrl = 0; };

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy)
	{
		__super::OnSize(nType,cx,cy);
		if (m_pTreeCtrl)
			m_pTreeCtrl->SetWindowPos(NULL,0,0,cx,cy,SWP_NOREDRAW|SWP_NOCOPYBITS );
		RecalcLayout();
	}
};

/** Dialog which hosts entity prototype library.
*/
class CGameTokenDialog : public CBaseLibraryDialog
{
	DECLARE_DYNAMIC(CGameTokenDialog);
public:
	CGameTokenDialog( CWnd *pParent );
	~CGameTokenDialog();

	// Called every frame.
	void Update();

	virtual UINT GetDialogMenuID();

	CGameTokenItem* GetSelectedGameToken();
	void UpdateSelectedItemInReport();

protected:
	void DoDataExchange(CDataExchange* pDX);
	BOOL OnInitDialog();

	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	
	afx_msg void OnAddItem();
	afx_msg void OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTreeRClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnCopy();
	afx_msg void OnPaste();

	afx_msg LRESULT OnTaskPanelNotify(WPARAM wParam, LPARAM lParam);
	afx_msg void OnReportDblClick(NMHDR * pNotifyStruct, LRESULT * /*result*/);
	afx_msg void OnReportSelChange(NMHDR * pNotifyStruct, LRESULT * /*result*/);
	
	afx_msg void OnTokenTypeChange();
	afx_msg void OnTokenValueChange();
	afx_msg void OnTokenInfoChnage();

	//////////////////////////////////////////////////////////////////////////
	// Some functions can be overriden to modify standart functionality.
	//////////////////////////////////////////////////////////////////////////
	virtual void InitToolbar( UINT nToolbarResID );
	virtual void ReloadItems();
	virtual void SelectItem( CBaseLibraryItem *item,bool bForceReload=false );
	virtual void DeleteItem( CBaseLibraryItem *pItem );

	//////////////////////////////////////////////////////////////////////////
	// IEditorNotifyListener listener implementation
	//////////////////////////////////////////////////////////////////////////
	virtual void OnEditorNotifyEvent( EEditorNotifyEvent event );
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	//void OnUpdateProperties( IVariable *var );
	
	void DropToItem( HTREEITEM hItem,HTREEITEM hSrcItem,CGameTokenItem *pMtl );

	DECLARE_MESSAGE_MAP()

	CSplitterWndEx m_wndHSplitter;
	CSplitterWndEx m_wndVSplitter;

	CGameTokenTreeContainerDialog m_treeContainerDlg;
	
	CPropertyCtrl m_propsCtrl;
	CImageList m_imageList;

	CImageList *m_dragImage;

	// Manager.
	CGameTokenManager *m_pGameTokenManager;

	HTREEITEM m_hDropItem;
	HTREEITEM m_hDraggedItem;

	CXTPReportControl m_wndReport;
	CXTPTaskPanel m_wndTaskPanel;
	CImageList m_taskImageList;
	
	CEdit m_tokenValueEdit;
	CEdit m_tokenInfoEdit;
	CComboBox m_tokenTypeCombo;
};

#endif // __GameTokenDialog_h__
