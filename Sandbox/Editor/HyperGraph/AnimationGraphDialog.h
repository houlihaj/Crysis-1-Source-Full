#ifndef __ANIMATIONGRAPHDIALOG_H__
#define __ANIMATIONGRAPHDIALOG_H__

#pragma once

#include "Resource.h"
#include "Controls\TemplDef.h" // message map extensions for templates
#include "ToolbarDialog.h"
#include "Controls\PropertyCtrl.h"

#include "CharacterEditor\CharacterEditor.h"
#include "AnimationGraphStateEditor.h"
#include "AnimationGraphStateQuery.h"
#include "AnimationGraphTester.h"
#include "AnimationGraph.h"
#include "HyperGraphView.h"
#include "AnimationGraphPreviewDialog.h"
#include "AnimationGraphPreviewManager.h"


class CAnimationGraphDialog;


class CAGHyperGraphView: public CHyperGraphView
{
protected:
	virtual void ShowContextMenu( CPoint point, CHyperNode* pNode );

	//////////////////////////////////////////////////////////////////////////
	virtual void UpdateTooltip( CHyperNode* pNode, CHyperNodePort* pPort ) {}
};


class CAnimationGraph;
typedef _smart_ptr<CAnimationGraph> CAnimationGraphPtr;

template < class BASE_TYPE >
class CFlatFramedCtrl : public BASE_TYPE
{
public:
	CFlatFramedCtrl() {}
	template < typename T > CFlatFramedCtrl( T param ) : BASE_TYPE(param) {}
protected:
	//{{AFX_MSG(CFlatFramedCtrl)
	afx_msg void OnNcPaint();
	//}}AFX_MSG
	DECLARE_TEMPLATE_MESSAGE_MAP()
};

BEGIN_TEMPLATE_MESSAGE_MAP_CUSTOM( class BASE_TYPE, CFlatFramedCtrl< BASE_TYPE >, BASE_TYPE )
//{{AFX_MSG_MAP(CFlatFramedCtrl)
ON_WM_NCPAINT()
//}}AFX_MSG_MAP
END_TEMPLATE_MESSAGE_MAP_CUSTOM()

template< class BASE_TYPE >
void CFlatFramedCtrl< BASE_TYPE >::OnNcPaint()
{
	__super::OnNcPaint();
	//	if ( !IsAppThemed() )
	//	{
	CWindowDC dc( this );
	CRect rc; dc.GetClipBox( rc );
	COLORREF color = GetSysColor(COLOR_3DSHADOW);
	dc.Draw3dRect( rc, color, color );
	//	}
}

class CAnimationGraphListCtrl : public CTreeCtrl
{
public:
	CAnimationGraphListCtrl( CAnimationGraphDialog * pParent ) : m_pParent(pParent) {}

	virtual BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
	void ReloadStates();
	void ReloadViews();
	void ReloadInputs();
	void SelectItemByName( const CString& name );
	typedef std::pair< CString, int > TNameIcon;

private:
	CAnimationGraphDialog * m_pParent;
	void AddList( const CString& root, std::vector< TNameIcon >& values );

	CImageList m_imageList;
};

class CAnimationGraphStateParamCtrl : public CDialog, public _reference_target_t
{
	DECLARE_DYNAMIC( CAnimationGraphStateParamCtrl );
public:
	CAnimationGraphStateParamCtrl( const char* name ): m_Name(name) {}
	~CAnimationGraphStateParamCtrl() {}

	virtual BOOL DestroyWindow();
	void AdjustHeight();

	enum { IDD = IDD_PANEL_AG_PARAM };

	CString m_Name;
	CString m_SelectedValue;
	CEdit m_ParamNameEdit;
	CListBox m_ValuesList;

private:
	int m_ItemHeight;

protected:
	DECLARE_MESSAGE_MAP()

	virtual BOOL OnInitDialog();
	virtual void DoDataExchange( CDataExchange* pDX );
	afx_msg BOOL OnEraseBkgnd( CDC* pDC );
	afx_msg void OnListBoxDblClk();
	afx_msg void OnContextMenu( CWnd* pWnd, CPoint pos );
	afx_msg void OnParamNameKillFocus();

public:
	afx_msg void OnListBoxSelChange();

protected:
	virtual void OnOK() { OnParamNameKillFocus(); }
	virtual void OnCancel() { m_ParamNameEdit.SetWindowText( m_Name ); }
};
typedef _smart_ptr< CAnimationGraphStateParamCtrl > CAnimationGraphStateParamCtrlPtr;

class CAnimationGraphStateParamsDlg : public CDialog
{
	DECLARE_DYNCREATE( CAnimationGraphStateParamsDlg );
public:
	CAnimationGraphStateParamsDlg() {}
	~CAnimationGraphStateParamsDlg() {}

	enum { IDD = IDD_PANEL_AG_STATEPARAMS };

protected:
	DECLARE_MESSAGE_MAP()

	afx_msg void OnAddParam();
	afx_msg void OnCheckBoxChanged();

protected:
	virtual void OnOK() {}
	virtual void OnCancel() {}
};

class CAnimationGraphStateParamsPanel : public CDialog
{
	DECLARE_DYNCREATE( CAnimationGraphStateParamsPanel );

	friend class CAnimationGraphStateParamCtrl;

public:
	CAnimationGraphStateParamsPanel() : m_bIgnoreVScroll(false), m_pParamsDeclaration(NULL) {}
	~CAnimationGraphStateParamsPanel() {}

	enum { IDD = IDD_PANEL_PROPERTIES };

	void Init();

	bool OnAddParam( const char* name );
	bool AddParamValue( const char* param, const char* value );
	bool DeleteParamValue( const char* param, const char* value );
	bool RenameParamValue( const char* param, const char* oldValue, const char* newValue );

	void OnSelChanged();

	void SetParamsDeclaration( CParamsDeclaration* pParamsDcl );

protected:
	DECLARE_MESSAGE_MAP()

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

protected:
	virtual void OnOK() {}
	virtual void OnCancel() {}

private:
	int	m_MiniPanelWidth;
	int	m_StateParamsDlgHeight;
	bool m_bIgnoreVScroll;

	CParamsDeclaration* m_pParamsDeclaration;

	void ArrangeMiniPanels();

	bool DeleteParam( CAnimationGraphStateParamCtrl* pParamCtrl );
	bool RenameParam( CAnimationGraphStateParamCtrl* pParamCtrl, const CString& newName );

public:
	CAnimationGraphStateParamsDlg m_StateParamsDlg;
	
	typedef std::list< CAnimationGraphStateParamCtrlPtr > TListParamCtrls;
	TListParamCtrls m_ParamCtrls;

	TParameterizationId m_CurrentSelection;
};

class CAnimationGraphDialog
	: public CXTPFrameWnd
	, public IAnimationGraphListener
	, public IHyperGraphListener
	, ICharacterChangeListener
{
	friend class CAGHyperGraphView;

	DECLARE_DYNCREATE(CAnimationGraphDialog);
public:
	static void RegisterViewClass();

	CAnimationGraphDialog();
	~CAnimationGraphDialog();

	BOOL Create( DWORD dwStyle,const RECT& rect,CWnd* pParentWnd );

	void OnHyperGraphEvent( IHyperNode * pNode, EHyperGraphEvent event );
	void OnLinkEdit( CHyperEdge * pEdge );
	
	void OnStateEvent( EAGStateEvent evt, CAGStatePtr pState );
	void OnViewEvent( EAGViewEvent, CAGViewPtr pView );
	void OnInputEvent( EAGInputEvent, CAGInputPtr pInput );
	void OnGraphModified();
	void OnGraphIslandReport();
	void OnGraphSpeedReport();
	void OnGraphBadCALReport();
	void OnGraphBadNullNodeReport();
	void OnGraphDeadInputReport();
	void OnGraphTransitionLengthReport();
	void OnGraphMatchSpeeds();
	void OnGraphOrphanNodesReport();

	CAnimationGraphPtr GetAnimationGraph();

	void SetParamsDeclaration( CParamsDeclaration * pParamsDeclaration )
	{
		m_stateParamsCtrl.SetParamsDeclaration( pParamsDeclaration );
	}
	void OnStateParamSelChanged()
	{
		if ( ::IsWindow(m_stateParamsCtrl) )
		{
			m_stateEditor.OnStateParamSelChanged(
				m_stateParamsCtrl.m_CurrentSelection.empty() ? NULL : &m_stateParamsCtrl.m_CurrentSelection,
				m_pView ? m_pView->GetName() : "" );
			TParameterizationId::const_iterator it, itEnd = m_stateParamsCtrl.m_CurrentSelection.end();
			for ( it = m_stateParamsCtrl.m_CurrentSelection.begin(); it != itEnd; ++it )
				if ( !it->second.IsEmpty() )
					m_previewManager.SetParameter( it->first, it->second );
		}
	}
	bool ResetParameterization();
	
	bool AddParamValue( const char* param, const char* value ) { return m_stateEditor.AddParamValue( param, value ); }
	bool DeleteParamValue( const char* param, const char* value ) { return m_stateEditor.DeleteParamValue( param, value ); }
	bool RenameParamValue( const char* param, const char* oldValue, const char* newValue ) { return m_stateEditor.RenameParamValue( param, oldValue, newValue ); }

	bool AddParameter( const CString& name ) { return m_stateEditor.AddParameter( name ); }
	bool DeleteParameter( const CString& name ) { return m_stateEditor.DeleteParameter( name ); }
	bool RenameParameter( const CString& oldName, const CString& newName ) { return m_stateEditor.RenameParameter( oldName, newName ); }

	int GetExcludeFromGraph() { return m_vStates.size() != 1 ? -1 : m_vStates[0]->GetExcludeFromGraph(); }
	void SetExcludeFromGraph( bool bExclude ) { if ( m_vStates.size() == 1 ) m_vStates[0]->SetExcludeFromGraph( bExclude ); }
	const CAGViewPtr GetSelectedView() const { return m_pView; }

	enum { IDD = IDD_TRACKVIEWDIALOG };

	CXTPDockingPaneManager* GetDockingPaneManager() { return &m_paneManager; }

protected:
	DECLARE_MESSAGE_MAP()

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();
	afx_msg void OnFileNew();
	afx_msg void OnFileOpen();
	afx_msg void OnFileSave();
	afx_msg void OnFileSaveAs();
	afx_msg void OnGraphAddState();
	afx_msg void OnGraphAddView();
	afx_msg void OnGraphAddInput();
	afx_msg void OnGraphCreateAnimation();
	afx_msg void OnGraphTrial();
	afx_msg void OnAGLinkMappingChanged();
	afx_msg void OnViewPane( UINT cmdID );
	afx_msg void OnViewPaneStateQuery();
	afx_msg void OnStateListSelChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnViewListSelChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnInputListSelChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg LRESULT OnDockingPaneNotify(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnTaskPanelNotify(WPARAM wParam, LPARAM lParam);    

	afx_msg void OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnIconSizeChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnUpdateIconSizeUI(CCmdUI* pCmdUI);
	afx_msg void OnDisplayIconsChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnUpdateDisplayIconsUI(CCmdUI* pCmdUI);
	afx_msg void OnDisplayPreviewChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnUpdateDisplayPreviewUI(CCmdUI* pCmdUI);
	afx_msg void OnContextMenu( CWnd* pWnd, CPoint pos );

private:
	enum EItemType
	{
		eIT_State,
		eIT_View,
		eIT_Input,
		eIT_Root,
		eIT_Error
	};

	void SetGraph( CAnimationGraphPtr pGraph );
	void SetActiveView( CAGViewPtr pView );
	void SetActiveState( CAGStatePtr pState );
	void SetActiveStates( std::vector< CAGStatePtr > vStates );
	void ViewReport( const CString& report );

	void UpdateTitle();
	void ReadDisplayedIconSize();
	void EnableIcons(bool enableIcons);
	void EnablePreview(bool enablePreview);

	void RegisterConsoleVariables();

	// ICharacterChangeListener
	virtual void OnCharacterChanged();

	int m_nBlockSelChanged;

	CXTPDockingPaneManager m_paneManager;

	CAnimationGraphPtr m_pGraph;
	CAGViewPtr m_pView;
	std::vector< CAGStatePtr > m_vStates;
	CFlatFramedCtrl< CAnimationGraphListCtrl > m_stateListCtrl;
	CFlatFramedCtrl< CAnimationGraphListCtrl > m_viewListCtrl;
	CFlatFramedCtrl< CAnimationGraphListCtrl > m_inputListCtrl;
	CAnimationGraphStateEditor m_stateEditor;
	CFlatFramedCtrl< CAnimationGraphStateParamsPanel > m_stateParamsCtrl;
	CAnimationGraphTester m_tester;
	CAnimationGraphStateQuery m_stateQuery;
	_smart_ptr<CHyperGraph> m_pViewHypergraph;
	CFlatFramedCtrl< CAGHyperGraphView > m_view;
	CImageList * m_pDragImage;
	CString m_dragNodeClass;
	CStatusBar m_statusBar;
	CAnimationImageManagerPtr m_pCAnimationImageManager;
	CAnimationGraphPreviewDialog m_previewDialog;
	CAnimationGraphPreviewManager m_previewManager;
	CRollupCtrl m_rollupCtrl;
	CXTPToolBar m_wndToolBar;

	typedef std::pair<int, int> IconSize;
	typedef std::vector<IconSize> IconSizeList;
	IconSizeList m_iconSizes;

	ICVar* m_displayedIconSize;
	ICVar* m_enableIcons;
	ICVar* m_preview;
};

#endif
