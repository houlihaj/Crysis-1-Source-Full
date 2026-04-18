////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   MaterialBrowser.h
//  Version:     v1.00
//  Created:     26/8/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __MaterialBrowser_h__
#define __MaterialBrowser_h__
#pragma once

#include "MaterialFileTree.h"
#include "IDataBaseManager.h"
#include "ToolbarDialog.h"

struct IDataBaseItem;
struct IDataBaseManager;
class  CMaterial;

class CMaterialBrowserCtrl;
class CMaterialImageListCtrl;

//////////////////////////////////////////////////////////////////////////
class CItemBrowserTreeCtrl : public CTreeCtrl
{
public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);

	DECLARE_MESSAGE_MAP()
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
};

//////////////////////////////////////////////////////////////////////////
// CMaterialBrowserDlg
//////////////////////////////////////////////////////////////////////////
class CMaterialBrowserDlg : public CDialog
{
public:
	enum { IDD = IDD_MATERIAL_BROWSER };
	CMaterialBrowserDlg();
	~CMaterialBrowserDlg();

	void SetMtlBrowser( CMaterialBrowserCtrl *pMtlBrowser ) { m_pMtlBrowser = pMtlBrowser; }
	void SetListType( int type );

	BOOL OnInitDialog();

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnRadioButton();
	afx_msg void OnReload();
	afx_msg void OnFilterChange();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK() {};
	virtual void OnCancel() {};

	CEdit m_filterCtrl;
	CCustomButton m_reloadButton;
	int m_iSelection;
	CMaterialBrowserCtrl* m_pMtlBrowser;
};

//////////////////////////////////////////////////////////////////////////
struct IMaterialBrowserListener
{
	virtual void OnBrowserSelectItem( IDataBaseItem* pItem,bool bForce ) = 0;
};


//////////////////////////////////////////////////////////////////////////
// CMaterialBrowserCtrl
//////////////////////////////////////////////////////////////////////////
class CMaterialBrowserCtrl : public CToolbarDialog, public IDataBaseManagerListener
{
public:
	enum EViewType {
    VIEW_LEVEL = 0,
		VIEW_ALL = 1,
		VIEW_MATLIB = 2,
	};

	CMaterialBrowserCtrl();
	~CMaterialBrowserCtrl();

	enum { IDD = IDD_DATABASE };

	void SetListener( IMaterialBrowserListener *pListener ) { m_pListener = pListener; }
	BOOL Create( const RECT& rect,CWnd* pParentWnd,UINT nID );
	virtual HTREEITEM InsertItemToTree( IDataBaseItem *pItem,HTREEITEM hParent,int nSubMtlSlot=-1 );

	void ReloadItems( EViewType viewType );
	EViewType GetViewType() const { return m_viewType; };

	void ClearItems();
	void SetFilter( const CString &filter );

	void AddItemToTree( IDataBaseItem *pItem,bool bLockRedraw=true );
	void SelectItem( IDataBaseItem *pItem );
	void DeleteItem();

	afx_msg void OnCopy();
	afx_msg void OnCopyName();
	afx_msg void OnPaste();
	afx_msg void OnCut();
	afx_msg void OnClone();
	afx_msg void OnAddNewMaterial();
	afx_msg void OnAddNewMultiMaterial();
	afx_msg void OnToggleFilter();
	afx_msg void OnReloadItems();
	afx_msg void OnFilterUpdateUI( CCmdUI *pCmdUI );
	bool CanPaste();

	void SetImageListCtrl( CMaterialImageListCtrl *pCtrl );

	//////////////////////////////////////////////////////////////////////////
	// IDataBaseManagerListener implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnDataBaseItemEvent( IDataBaseItem *pItem,EDataBaseItemEvent event );
	//////////////////////////////////////////////////////////////////////////

protected:
	// Item definition.
	struct SItemInfo;
	enum ESourceControlOp
	{
		ESCM_IMPORT,
		ESCM_CHECKIN,
		ESCM_CHECKOUT,
		ESCM_UNDO_CHECKOUT,
		ESCM_GETLATEST,
	};

	void ReloadLevelItems();
	void ReloadAllItems();

	CString GetFullItemName( HTREEITEM hItem );
	void DropToItem( HTREEITEM hItem,HTREEITEM hSrcItem );
	CMaterial* GetMaterial( HTREEITEM hItem );
	void RemoveEmptyParent( HTREEITEM hItem );
	void ReloadTreeSubMtls( HTREEITEM hItem );
	void RemoveItemFromTree( HTREEITEM hItem );
	SItemInfo* GetItemInfo( HTREEITEM hItem );
	SItemInfo* GetParentItemInfo( HTREEITEM hItem );
	void DeleteItem( HTREEITEM hItem );

	void SetSelectedItem( HTREEITEM hItem );
	
	void OnAddSubMtl();
	void OnSelectAssignedObjects();
	void OnAssignMaterialToSelection();
	void OnRenameItem();
	void OnSetSubMtlCount( HTREEITEM hItem );

	void DoSourceControlOp( HTREEITEM hItem,ESourceControlOp );

	void OnMakeSubMtlSlot( HTREEITEM hItem );
	void OnClearSubMtlSlot( HTREEITEM hItem );
	void SetSubMaterial( HTREEITEM hMultiSubMtl,int nSlot,CMaterial *pSubMaterial,bool bSelectSubMtl );

	void UpdateItemState( HTREEITEM hItem );
	
	void SetItemStateFromFileAttributes( HTREEITEM hItem,uint32 attr,bool bFromCGF );
	void OnImageListCtrlSelect( struct CImageListCtrlItem *pItem );
	void OnSaveToFile( bool bMulti );

	void RefreshSelected();

	void SortTreeCtrl(HTREEITEM hItem);


	//////////////////////////////////////////////////////////////////////////
	// Messages.
	//////////////////////////////////////////////////////////////////////////
	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnSelChangedItemTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTreeGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTreeRClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTreeRButtonDown(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTreeItemExpending(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTreeKeyDown(NMHDR* pNMHDR, LRESULT* pResult);
	//////////////////////////////////////////////////////////////////////////


private:
	CXTPToolBar m_wndToolBar;
	CItemBrowserTreeCtrl m_treeCtrl;
	CImageList m_imageList;
	CString m_filterText;
	bool m_bIgnoreSelectionChange;
	
	//CMaterialBrowserDlg m_browserDlg;
	CMaterialManager *m_pMatMan;
	IMaterialBrowserListener *m_pListener;
	CMaterialImageListCtrl *m_pMaterialImageListCtrl;

	EViewType m_viewType;

	typedef std::set<SItemInfo*> Items;
	Items m_items;

	typedef std::map<CString,HTREEITEM,stl::less_stricmp<CString> > NameMap;
	NameMap m_nameMap;

	//////////////////////////////////////////////////////////////////////////
	// Drag&Drop support.
	//////////////////////////////////////////////////////////////////////////
	CImageList *m_dragImage;
	HTREEITEM m_hDropItem;
	HTREEITEM m_hDraggedItem;
	HTREEITEM m_hCurrentItem;
	TSmartPtr<CMaterial> m_pCurrentItem;

	CMaterial* m_pLastActiveMultiMaterial;

	HCURSOR m_hCursorDefault;
	HCURSOR m_hCursorNoDrop;
	HCURSOR m_hCursorCreate;
	HCURSOR m_hCursorReplace;
};

#endif // __MaterialBrowser_h__

