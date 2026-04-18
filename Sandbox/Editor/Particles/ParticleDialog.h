////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   particledialog.h
//  Version:     v1.00
//  Created:     17/6/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __particledialog_h__
#define __particledialog_h__
#pragma once

#include "BaseLibraryDialog.h"
#include "Controls\SplitterWndEx.h"
#include "Controls\TreeCtrlEx.h"
#include "Controls\PropertyCtrlEx.h"
#include "Controls\PreviewModelCtrl.h"

class CParticleItem;
class CParticleManager;

/** Dialog which hosts entity prototype library.
*/
class CParticleDialog : public CBaseLibraryDialog
{
	DECLARE_DYNAMIC(CParticleDialog)
public:
	CParticleDialog( CWnd *pParent );
	~CParticleDialog();

	// Called every frame.
	void Update();

	virtual UINT GetDialogMenuID();

public:
	afx_msg void OnAssignParticleToSelection();
	afx_msg void OnResetParticleOnSelection();
	afx_msg void OnGetParticleFromSelection();
	afx_msg void OnActivateAll();
	afx_msg void OnEntityStart();
	afx_msg void OnEntityStop();

protected:
	void DoDataExchange(CDataExchange* pDX);
	BOOL OnInitDialog();
	
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	
	afx_msg void OnAddItem();
	afx_msg void OnPlay();
	afx_msg void OnUpdatePlay( CCmdUI* pCmdUI );
	afx_msg void OnDrawSelection();
	afx_msg void OnDrawBox();
	afx_msg void OnDrawSphere();
	afx_msg void OnDrawTeapot();
	afx_msg void OnAddSubItem();
	afx_msg void OnDelSubItem();
	afx_msg void OnUpdateItemSelected( CCmdUI* pCmdUI );
	afx_msg void OnUpdateObjectSelected( CCmdUI* pCmdUI );
	afx_msg void OnUpdateAssignMtlToSelection( CCmdUI *pCmdUI );
	afx_msg void OnUpdateConvertFromEntity( CCmdUI *pCmdUI );
	afx_msg void OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnNotifyTreeRClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnNotifyTreeLClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnPickMtl();
	afx_msg void OnUpdatePickMtl( CCmdUI* pCmdUI );
	afx_msg void OnCopy();
	afx_msg void OnPaste();
	afx_msg void OnSelectAssignedObjects();

	//////////////////////////////////////////////////////////////////////////
	// Some functions can be overriden to modify standart functionality.
	//////////////////////////////////////////////////////////////////////////
	virtual void InitToolbar( UINT nToolbarResID );
	virtual HTREEITEM InsertItemToTree( CBaseLibraryItem *pItem,HTREEITEM hParent );
	virtual void SelectItem( CBaseLibraryItem *item,bool bForceReload=false );

	//////////////////////////////////////////////////////////////////////////
	CParticleItem* GetSelectedParticle();
	void OnUpdateProperties( IVariable *var );

	bool AssignParticleToEntity( CParticleItem *pItem, CBaseObject *pObject, Vec3 const* pPos = NULL );
	CBaseObject* CreateParticleEntity( CParticleItem *pItem, Vec3 const& pos, CBaseObject* pParent = NULL );

	void UpdateItemState( CParticleItem* pItem, bool bRecursive = false );

	//void LoadGeometry( const CString &filename );
	//void ReleaseGeometry();
	//void AssignMtlToGeometry();

	//void SetTextureVars( CVariableArray *texVar,CParticleItem *mtl,int id,const CString &name );
	void SetParticleVars( CParticleItem *mtl );

	void DropToItem( HTREEITEM hItem,HTREEITEM hSrcItem,CParticleItem *pMtl );
	void ConvertContinuous(float fSpawnPeriod);
	class CEntity* GetItemFromEntity();

	enum EDrawType
	{
		DRAW_BOX,
		DRAW_SPHERE,
		DRAW_TEAPOT,
		DRAW_SELECTION,
	};
	

	DECLARE_MESSAGE_MAP()

	CSplitterWndEx m_wndHSplitter;
	CSplitterWndEx m_wndVSplitter;
	
	CPreviewModelCtrl m_previewCtrl;
	CPropertyCtrlEx m_propsCtrl;
	CImageList m_imageList;

	CImageList *m_dragImage;

	// Object to render.
	CString m_visualObject;
	IStatObj *m_pGeometry;
	IRenderNode *m_pRenderNode;

	bool m_bRealtimePreviewUpdate;
	bool m_bOwnGeometry;

	// Particle manager.
	CParticleManager *m_pPartManager;

	CVarBlockPtr m_vars;
	class CParticleUIDefinition* pParticleUI;

	EDrawType m_drawType;
	CString m_geometryFile;

	HTREEITEM m_hDropItem;
	HTREEITEM m_hDraggedItem;

	TSmartPtr<CParticleItem> m_pDraggedMtl;
};

#endif // __particledialog_h__
