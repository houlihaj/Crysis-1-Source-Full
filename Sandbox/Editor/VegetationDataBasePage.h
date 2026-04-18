////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   vegetationpanel.h
//  Version:     v1.00
//  Created:     31/7/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __vegetationpanel_h__
#define __vegetationpanel_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include <XTToolkitPro.h>

#include "Controls\PropertyCtrl.h"
#include "Controls\MltiTree.h"
#include "Controls\PreviewModelCtrl.h"
#include "ToolbarDialog.h"
#include "Controls\FillSliderCtrl.h"
#include "Controls\SplitterWndEx.h"
#include "DataBaseDialog.h"

/////////////////////////////////////////////////////////////////////////////
// CVegetationDataBasePage dialog

class CVegetationObject;
class CVegetationMap;
class CPanelPreview;

class CVegetationDataBasePage : public CDataBaseDialogPage, public IEditorNotifyListener
{
// Construction
public:
	CVegetationDataBasePage(CWnd* pParent = NULL);   // standard constructor
	~CVegetationDataBasePage();

	static void RegisterViewClass();

	//////////////////////////////////////////////////////////////////////////
	// CDataBaseDialogPage implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void SetActive( bool bActive );
	virtual void Update();
	//////////////////////////////////////////////////////////////////////////
	
	virtual void OnEditorNotifyEvent( EEditorNotifyEvent event );

	void SelectObject( CVegetationObject *object );
	void UpdateInfo();

// Dialog Data
	//{{AFX_DATA(CVegetationDataBasePage)
	enum { IDD = IDD_DATABASE };
	CFillSliderCtrl	m_radius;
	CColorCheckBox	m_paintButton;
	//}}AFX_DATA

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVegetationDataBasePage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};

	void ReloadObjects();
	void DoIdleUpdate();
	void SyncSelection();

	void OnSelectionChange();

	void GetSelectedObjects( std::vector<CVegetationObject*> &objects );
	void AddLayerVars( CVarBlock *pVarBlock,CVegetationObject *pObject=NULL );
	void OnLayerVarChange( IVariable *pVar );
	void GotoObjectMaterial();

	CVegetationObject* FindObject( REFGUID guid );
	void ClearSelection();

	// Generated message map functions
	//{{AFX_MSG(CVegetationDataBasePage)
	afx_msg void OnBrushRadiusChange();
	afx_msg void OnPaint();
	virtual BOOL OnInitDialog();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	afx_msg void OnAdd();
	afx_msg void OnClone();
	afx_msg void OnReplace();
	afx_msg void OnRemove();

	afx_msg void OnNewCategory();
	afx_msg void OnDistribute();
	afx_msg void OnDistributeMask();
	afx_msg void OnClear();
	afx_msg void OnClearMask();
	afx_msg void OnBnClickedMerge();
	afx_msg void OnBnClickedCreateSel();
	afx_msg void OnBnClickedImport();
	afx_msg void OnBnClickedExport();
	afx_msg void OnBnClickedScale();

	afx_msg void OnHide();
	afx_msg void OnUnhide();
	afx_msg void OnHideAll();
	afx_msg void OnUnhideAll();

	afx_msg LRESULT OnTaskPanelNotify(WPARAM wParam, LPARAM lParam);
	afx_msg void OnReportClick(NMHDR * pNotifyStruct, LRESULT * /*result*/);
	afx_msg void OnReportRClick(NMHDR * pNotifyStruct, LRESULT * /*result*/);
	afx_msg void OnReportSelChange(NMHDR * pNotifyStruct, LRESULT * /*result*/);
	afx_msg void OnReportHyperlink(NMHDR * pNotifyStruct, LRESULT * /*result*/);
	afx_msg void OnReportKeyDown(NMHDR * pNotifyStruct, LRESULT * /*result*/);


	//////////////////////////////////////////////////////////////////////////
	// Controls.
	//////////////////////////////////////////////////////////////////////////
	CXTPTaskPanel m_wndTaskPanel;

	CPropertyCtrl m_propertyCtrl;

	CXTPReportControl m_wndReport;

	CSplitterWndEx m_splitWnd;
	CPreviewModelCtrl m_previewCtrl;
	//////////////////////////////////////////////////////////////////////////

	//CMultiTree m_objectsTree;
	CImageList m_taskImageList;
	CImageList m_reportImageList;

	typedef std::vector<CVegetationObject*> Selection;
	CVegetationMap* m_vegetationMap;

	bool m_bIgnoreSelChange;
	bool m_bActive;

	TSmartPtr<CVarBlock> m_varBlock;

	bool m_bSyncSelection;

	//Command for correct sync. Used in DoIdleUpdate()
	int m_nCommand;
};

#endif // __vegetationpanel_h__