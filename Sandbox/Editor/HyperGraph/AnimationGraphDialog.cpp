#include "StdAfx.h"
#include "StringDlg.h"
#include "AnimationGraphDialog.h"
#include "IViewPane.h"
#include "resource.h"
#include "AnimationGraph.h"
#include "IslandDetection.h"
#include "SpeedReport.h"
#include "ICryAnimation.h"
#include "TransitionReport.h"
#include "CharacterEditor\CharPanel_Animation.h"
#include "CharacterEditor\ModelViewportCE.h"
#include "PropertiesPanel.h"

#pragma warning(disable:4355) // warning C4355: 'this' : used in base member initializer list

#define ANIMATIONGRAPH_DIALOGFRAME_CLASSNAME "AnimationGraphDialog"
#define ANIMATIONGRAPH_FILE_FILTER "Graph XML Files (*.xml)|*.xml"
#define CHARACTER_FILE_FILTER "Character Files (*.cdf)|*.cdf"

#define IDC_ANIMGRAPH_STATES 1
#define IDC_ANIMGRAPH_STATEPARAMS 2
#define IDC_ANIMGRAPH_VIEWS 3
#define IDC_ANIMGRAPH_INPUTS 4

#define IDW_ANIMGRAPH_STATES_PANE AFX_IDW_CONTROLBAR_FIRST + 20
#define IDW_ANIMGRAPH_STATE_EDITOR_PANE AFX_IDW_CONTROLBAR_FIRST + 21
#define IDW_ANIMGRAPH_TEST_PANE AFX_IDW_CONTROLBAR_FIRST + 22
#define IDW_ANIMGRAPH_PREVIEW_PANE AFX_IDW_CONTROLBAR_FIRST + 23
#define IDW_ANIMGRAPH_PREVIEW_OPTIONS_PANE AFX_IDW_CONTROLBAR_FIRST + 24
#define IDW_ANIMGRAPH_STATE_PARAMS_PANE AFX_IDW_CONTROLBAR_FIRST + 25
#define IDW_ANIMGRAPH_STATE_QUERY_PANE AFX_IDW_CONTROLBAR_FIRST + 26
#define IDW_ANIMGRAPH_VIEWS_PANE AFX_IDW_CONTROLBAR_FIRST + 27
#define IDW_ANIMGRAPH_INPUTS_PANE AFX_IDW_CONTROLBAR_FIRST + 28
#define ANIMATION_GRAPH_PANE_COUNT 9

namespace
{
	const char* s_szCharacterName = "Objects/Characters/human_male/character.cdf";
}

class CAnimationGraphViewClass : public TRefCountBase<IViewPaneClass>
{
	//////////////////////////////////////////////////////////////////////////
	// IClassDesc
	//////////////////////////////////////////////////////////////////////////
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_VIEWPANE; };
	virtual REFGUID ClassID()
	{
		// {1A04D848-2E77-45c7-A817-13E8260C6516}
		static const GUID guid = { 0x1a04d848, 0x2e77, 0x45c7, { 0xa8, 0x17, 0x13, 0xe8, 0x26, 0xc, 0x65, 0x16 } };
		return guid;
	}
	virtual const char* ClassName() { return "Animation Graph"; };
	virtual const char* Category() { return "Animation Graph"; };
	//////////////////////////////////////////////////////////////////////////
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CAnimationGraphDialog); };
	virtual const char* GetPaneTitle() { return _T("Animation Graph"); };
	virtual EDockingDirection GetDockingDirection() { return DOCK_FLOAT; };
	virtual CRect GetPaneRect() { return CRect(200,200,600,500); };
	virtual bool SinglePane() { return false; };
	virtual bool WantIdleUpdate() { return false; };
};


IMPLEMENT_DYNAMIC(CAnimationGraphStateParamCtrl,CDialog);

BEGIN_MESSAGE_MAP(CAnimationGraphStateParamCtrl,CDialog)
	ON_WM_ERASEBKGND()
	ON_LBN_SELCHANGE( IDC_VALUESLIST, OnListBoxSelChange )
	ON_LBN_DBLCLK( IDC_VALUESLIST, OnListBoxDblClk )
	ON_EN_KILLFOCUS( IDC_PARAMNAME, OnParamNameKillFocus )
	ON_WM_CONTEXTMENU()
END_MESSAGE_MAP()

BOOL CAnimationGraphStateParamCtrl::DestroyWindow()
{
	return __super::DestroyWindow();
}

void CAnimationGraphStateParamCtrl::DoDataExchange( CDataExchange* pDX )
{
	__super::DoDataExchange( pDX );
	DDX_Control( pDX, IDC_PARAMNAME, m_ParamNameEdit );
	DDX_Control( pDX, IDC_VALUESLIST, m_ValuesList );
}

BOOL CAnimationGraphStateParamCtrl::OnInitDialog()
{
	__super::OnInitDialog();

	m_ParamNameEdit.SetWindowText( m_Name );

	m_ValuesList.AddString(" [All]");
	m_ValuesList.SetCurSel(0);

	m_ItemHeight = float( m_ValuesList.GetItemHeight(0) ) * 1.333f;
	m_ValuesList.SetItemHeight( 0, m_ItemHeight );

	AdjustHeight();
	return FALSE;
}

void CAnimationGraphStateParamCtrl::AdjustHeight()
{
	int numItems = m_ValuesList.GetCount();
	if ( numItems > 7 )
		numItems = 7;

	CRect rcParent, rcWindow, rcClient;
	m_ValuesList.GetWindowRect( rcWindow );
	m_ValuesList.GetClientRect( rcClient );
	GetWindowRect( rcParent );

	rcParent.bottom -= rcWindow.bottom;
	rcWindow.bottom = rcWindow.top + rcWindow.Height()-rcClient.bottom + numItems*m_ItemHeight;
	m_ValuesList.SetWindowPos( 0, 0, 0, rcWindow.Width(), rcWindow.Height(), SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER );
	rcParent.bottom += rcWindow.bottom;

	CAnimationGraphStateParamsPanel* pParent = (CAnimationGraphStateParamsPanel*) GetParent();
	SetWindowPos( 0, 0, 0, rcParent.Width(), rcParent.Height(), SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER );
	pParent->ArrangeMiniPanels();
}

BOOL CAnimationGraphStateParamCtrl::OnEraseBkgnd( CDC* pDC )
{
	CRect rc;
	GetClientRect( rc );
	pDC->FillSolidRect( 0, 1, rc.right, rc.bottom-1, GetSysColor(COLOR_BTNFACE) );
	pDC->FillSolidRect( 0, 0, rc.right, 1, GetSysColor(COLOR_3DSHADOW) );
	return TRUE;
}

void CAnimationGraphStateParamCtrl::OnParamNameKillFocus()
{
	static bool bIgnore = false;
	if ( bIgnore )
		return;

	CString name;
	m_ParamNameEdit.GetWindowText( name );
	if ( name == m_Name )
		return;

	bIgnore = true;
	if ( name.IsEmpty() )
	{
		if ( AfxMessageBox("This will delete selected state parameter!\n\nAre you sure?", MB_YESNO|MB_DEFBUTTON2) == IDYES )
		{
			if ( !((CAnimationGraphStateParamsPanel*) GetParent())->DeleteParam(this) )
				m_ParamNameEdit.SetWindowText( m_Name );
		}
		else
			m_ParamNameEdit.SetWindowText( m_Name );
	}
	else
	{
		if ( ((CAnimationGraphStateParamsPanel*) GetParent())->RenameParam(this, name) )
		{
			m_Name = name;
			m_ParamNameEdit.SetWindowText( m_Name );
		}
		else
			m_ParamNameEdit.SetWindowText( m_Name );
	}
	bIgnore = false;
}

void CAnimationGraphStateParamCtrl::OnListBoxSelChange()
{
	int sel = m_ValuesList.GetCurSel();
	if ( sel > 0 )
		m_ValuesList.GetText( sel, m_SelectedValue );
	else
		m_SelectedValue.Empty();
	((CAnimationGraphStateParamsPanel*) GetParent())->OnSelChanged();
}

void CAnimationGraphStateParamCtrl::OnListBoxDblClk()
{
	int sel = m_ValuesList.GetCurSel();
	if ( sel <= 0 )
		return;

	CStringDlg dlg;
	CString oldValue;
	m_ValuesList.GetText( sel, oldValue );
	dlg.m_strString = oldValue;
	if ( dlg.DoModal() == IDOK )
	{
		CAnimationGraphStateParamsPanel* pParent = (CAnimationGraphStateParamsPanel*) GetParent();
		if ( pParent->RenameParamValue(m_Name, oldValue, dlg.m_strString) )
		{
			m_ValuesList.DeleteString( sel );
			m_ValuesList.SetCurSel( m_ValuesList.AddString(dlg.m_strString) );
			OnListBoxSelChange();
		}
	}
}

void CAnimationGraphStateParamCtrl::OnContextMenu( CWnd* pWnd, CPoint pos )
{
	__super::OnContextMenu( pWnd, pos );

	if ( pos.x == -1 && pos.y == -1 )
	{
		pos.SetPoint( 5, 5 );
		m_ValuesList.ClientToScreen( &pos );
	}

	int selection = m_ValuesList.GetCurSel();

	CMenu menu;
	VERIFY( menu.CreatePopupMenu() );

	menu.AppendMenu( MF_STRING, ID_AG_PARAMVALUE_ADD, _T("Add...") );
	menu.AppendMenu( MF_STRING | ( selection > 0 ? 0 : MF_DISABLED ), ID_AG_PARAMVALUE_RENAME, _T("Rename...") );
	menu.AppendMenu( MF_STRING | ( selection > 0 ? 0 : MF_DISABLED ), ID_AG_PARAMVALUE_DELETE, _T("Delete...") );

	//menu.AppendMenu( MF_SEPARATOR );

	//CSmartObjectsEditorDialog* pDlg = (CSmartObjectsEditorDialog*) GetParent();

	// track menu	
	int nMenuResult = CXTPCommandBars::TrackPopupMenu( &menu, TPM_NONOTIFY|TPM_RETURNCMD|TPM_LEFTALIGN|TPM_RIGHTBUTTON, pos.x, pos.y, this, NULL );

	CStringDlg dlg;
	int sel = m_ValuesList.GetCurSel();
	CAnimationGraphStateParamsPanel* pParent = (CAnimationGraphStateParamsPanel*) GetParent();
	switch ( nMenuResult )
	{
	case ID_AG_PARAMVALUE_ADD:
		if ( dlg.DoModal() == IDOK )
		{
			if ( pParent->AddParamValue(m_Name, dlg.m_strString) )
			{
				m_ValuesList.SetCurSel( m_ValuesList.AddString(dlg.m_strString) );
				OnListBoxSelChange();
			}
		}
		break;

	case ID_AG_PARAMVALUE_RENAME:
		if ( sel <= 0 )
			return;
		OnListBoxDblClk();
		break;

	case ID_AG_PARAMVALUE_DELETE:
		if ( sel > 0 )
		{
			CString msg, value;
			m_ValuesList.GetText( sel, value );
			msg.Format( "Delete value '%s'?", value );
			if ( AfxMessageBox(msg, MB_YESNO) == IDYES )
			{
				if ( pParent->DeleteParamValue(m_Name, value) )
				{
					m_ValuesList.DeleteString( sel );
					m_ValuesList.SetCurSel( 0 );
					OnListBoxSelChange();
				}
			}
		}
		break;
	}

	AdjustHeight();
	m_ValuesList.SetFocus();
}


IMPLEMENT_DYNCREATE(CAnimationGraphStateParamsPanel,CDialog);

BEGIN_MESSAGE_MAP(CAnimationGraphStateParamsPanel,CDialog)
	ON_WM_SIZE()
	ON_WM_VSCROLL()
END_MESSAGE_MAP()


void CAnimationGraphStateParamsPanel::Init()
{
	m_StateParamsDlg.Create( m_StateParamsDlg.IDD, this );
	m_StateParamsDlg.ShowWindow( SW_HIDE );

	CRect rc;
	m_StateParamsDlg.GetWindowRect( rc );
	m_MiniPanelWidth = rc.Width();
	m_StateParamsDlgHeight = rc.Height();
}

void CAnimationGraphStateParamsPanel::SetParamsDeclaration( CParamsDeclaration* pParamsDcl )
{
	if ( pParamsDcl == m_pParamsDeclaration || !m_hWnd )
		return;
	m_pParamsDeclaration = pParamsDcl;

	LockWindowUpdate();

	SCROLLINFO si;
	si.cbSize = sizeof( SCROLLINFO );
	GetScrollInfo( SB_VERT, &si );
	ScrollWindow( 0, -si.nPos );
	si.nPos = 0;
	si.fMask = SIF_DISABLENOSCROLL | SIF_POS;
	SetScrollInfo( SB_VERT, &si );

	TListParamCtrls::iterator it, itEnd = m_ParamCtrls.end();
	for ( it = m_ParamCtrls.begin(); it != itEnd; ++it )
	{
		CAnimationGraphStateParamCtrl* pCtrl = *it;
		pCtrl->DestroyWindow();
	}
	m_ParamCtrls.clear();
	
	m_StateParamsDlg.ShowWindow( m_pParamsDeclaration ? SW_NORMAL : SW_HIDE );
	if ( m_pParamsDeclaration )
	{
		CParamsDeclaration::iterator it, itEnd = m_pParamsDeclaration->end();
		for ( it = m_pParamsDeclaration->begin(); it != itEnd; ++it )
		{
			CAnimationGraphStateParamCtrl* pParam = new CAnimationGraphStateParamCtrl( it->first );
			m_ParamCtrls.push_back( pParam );

			pParam->Create( CAnimationGraphStateParamCtrl::IDD, this );
			pParam->ShowWindow( SW_NORMAL );

			pParam->m_ParamNameEdit.SetWindowText( it->first );

			TSetOfCString& values = it->second;
			TSetOfCString::iterator itValues, itValuesEnd = values.end();
			for ( itValues = values.begin(); itValues != itValuesEnd; ++itValues )
				pParam->m_ValuesList.AddString( *itValues );
			pParam->AdjustHeight();
		}
	}

	ArrangeMiniPanels();
	OnSelChanged();

	UnlockWindowUpdate();
}

bool CAnimationGraphStateParamsPanel::AddParamValue( const char* param, const char* value )
{
	return ((CAnimationGraphDialog*)GetOwner())->AddParamValue( param, value );
}

bool CAnimationGraphStateParamsPanel::DeleteParamValue( const char* param, const char* value )
{
	return ((CAnimationGraphDialog*)GetOwner())->DeleteParamValue( param, value );
}

bool CAnimationGraphStateParamsPanel::RenameParamValue( const char* param, const char* oldValue, const char* newValue )
{
	return ((CAnimationGraphDialog*)GetOwner())->RenameParamValue( param, oldValue, newValue );
}

bool CAnimationGraphStateParamsPanel::OnAddParam( const char* name )
{
	if ( m_pParamsDeclaration->find( name ) != m_pParamsDeclaration->end() )
	{
		AfxMessageBox( "The entered parameter name already exists for selected state!", MB_OK|MB_ICONERROR );
		return false;
	}

	TListParamCtrls::iterator it, itEnd = m_ParamCtrls.end();
	for ( it = m_ParamCtrls.begin(); it != itEnd; ++it )
	{
		(*it)->m_ValuesList.SetCurSel(0);
		(*it)->OnListBoxSelChange();
	}

	if ( !((CAnimationGraphDialog*)GetOwner())->AddParameter( name ) )
		return false;

	CAnimationGraphStateParamCtrl* pParam = new CAnimationGraphStateParamCtrl( name );
	m_ParamCtrls.push_back( pParam );

	pParam->Create( CAnimationGraphStateParamCtrl::IDD, this );
	pParam->ShowWindow( SW_NORMAL );

	ArrangeMiniPanels();
	OnSelChanged();

	return true;
}

bool CAnimationGraphStateParamsPanel::DeleteParam( CAnimationGraphStateParamCtrl* pParamCtrl )
{
	if ( !((CAnimationGraphDialog*)GetOwner())->DeleteParameter(pParamCtrl->m_Name) )
		return false;

	pParamCtrl->DestroyWindow();
	m_ParamCtrls.remove( pParamCtrl );
	ArrangeMiniPanels();
	OnSelChanged();
	return true;
}

bool CAnimationGraphStateParamsPanel::RenameParam( CAnimationGraphStateParamCtrl* pParamCtrl, const CString& newName )
{
	if ( !((CAnimationGraphDialog*)GetOwner())->RenameParameter(pParamCtrl->m_Name, newName) )
		return false;

	OnSelChanged();
	return true;
}

void CAnimationGraphStateParamsPanel::OnSelChanged()
{
	bool specialized = false;

	m_CurrentSelection.clear();
	TListParamCtrls::iterator it, itEnd = m_ParamCtrls.end();
	for ( it = m_ParamCtrls.begin(); it != itEnd; ++it )
	{
		CAnimationGraphStateParamCtrl* pParamCtrl = *it;
		m_CurrentSelection[ pParamCtrl->m_Name ] = pParamCtrl->m_SelectedValue;
		specialized = specialized || !pParamCtrl->m_SelectedValue.IsEmpty();
	}
	if ( !specialized )
		m_CurrentSelection.clear();

	CAnimationGraphDialog* pOwner = (CAnimationGraphDialog*) GetOwner();
	pOwner->OnStateParamSelChanged();

	CButton* pCheckBox = (CButton*) m_StateParamsDlg.GetDlgItem( IDC_EXCLUDEFROMGRAPH );
	pCheckBox->EnableWindow( specialized );
	if ( specialized )
	{
		switch ( pOwner->GetExcludeFromGraph() )
		{
		case -1:
			pCheckBox->SetCheck( BST_INDETERMINATE );
			break;
		case 0:
			pCheckBox->SetCheck( BST_UNCHECKED );
			break;
		case 1:
			pCheckBox->SetCheck( BST_CHECKED );
			break;
		}
	}
	else
		pCheckBox->SetCheck( BST_UNCHECKED );
}

void CAnimationGraphStateParamsPanel::ArrangeMiniPanels()
{
	if ( !IsWindow(m_StateParamsDlg) )
		return;

	CRect rc;
	GetClientRect( rc );

	SCROLLINFO si;
	si.cbSize = sizeof( SCROLLINFO );
	si.fMask = SIF_POS;
	GetScrollInfo( SB_VERT, &si );

	int numColumns = 1;
	if ( m_MiniPanelWidth && rc.right > m_MiniPanelWidth )
		numColumns = rc.right / m_MiniPanelWidth;

	std::vector< int > yPerColumn;
	std::vector< int > widthPerColumn;
	for ( int i = 0; i < numColumns; ++i )
	{
		yPerColumn.push_back( -si.nPos - (i>0) );
		widthPerColumn.push_back( m_MiniPanelWidth + (i == numColumns-1 ? rc.right % m_MiniPanelWidth : 0) );
	}

	m_StateParamsDlg.MoveWindow( 0, -si.nPos, widthPerColumn[0], m_StateParamsDlgHeight );
	yPerColumn[0] += m_StateParamsDlgHeight;

	TListParamCtrls::iterator it, itEnd = m_ParamCtrls.end();
	for ( it = m_ParamCtrls.begin(); it != itEnd; ++it )
	{
		int currentColumn = 0;
		int minY = yPerColumn[0];
		for ( int i = 1; i < numColumns; ++i )
			if ( yPerColumn[i] < minY )
			{
				minY = yPerColumn[i];
				currentColumn = i;
			}

		CRect rcParam;
		CAnimationGraphStateParamCtrl* pParamCtrl = *it;
		pParamCtrl->GetWindowRect( rcParam );
		pParamCtrl->MoveWindow( currentColumn*m_MiniPanelWidth, minY, widthPerColumn[currentColumn], rcParam.Height() );
		yPerColumn[currentColumn] += rcParam.Height();
	}

	int maxY = yPerColumn[0];
	for ( int i = 1; i < numColumns; ++i )
		if ( yPerColumn[i] > maxY )
			maxY = yPerColumn[i];

	si.cbSize = sizeof( SCROLLINFO );
	si.fMask = SIF_DISABLENOSCROLL | SIF_PAGE | SIF_RANGE;
	si.nMin = 0;
	si.nMax = maxY + si.nPos; // since we started at -si.nPos
	si.nPage = rc.bottom;
	m_bIgnoreVScroll = true;
	SetScrollInfo( SB_VERT, &si );
	m_bIgnoreVScroll = false;
}

void CAnimationGraphStateParamsPanel::OnSize(UINT nType, int cx, int cy)
{
	SCROLLINFO si;
	si.cbSize = sizeof( SCROLLINFO );
	si.fMask = SIF_POS;
	GetScrollInfo( SB_VERT, &si );
	int offset = si.nPos;

	__super::OnSize(nType, cx, cy);
	ArrangeMiniPanels();
	
	si.cbSize = sizeof( SCROLLINFO );
	si.fMask = SIF_POS;
	GetScrollInfo( SB_VERT, &si );
	offset -= si.nPos;
	ScrollWindow( 0, offset );
}

void CAnimationGraphStateParamsPanel::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	__super::OnVScroll(nSBCode, nPos, pScrollBar);
	if ( m_bIgnoreVScroll )
		return;

	SCROLLINFO si;
	si.cbSize = sizeof( SCROLLINFO );
	si.fMask = SIF_ALL;
	GetScrollInfo( SB_VERT, &si );
	int offset = si.nPos;

	switch (nSBCode)
	{
	case SB_BOTTOM: // Scroll to bottom
		si.nPos = si.nMax;
		break;
	case SB_ENDSCROLL: // End scroll
		break;
	case SB_LINEDOWN: // Scroll one line down
		si.nPos += 8;
		break;
	case SB_LINEUP: // Scroll one line up
		si.nPos -= 8;
		break;
	case SB_PAGEDOWN: // Scroll one page down
		si.nPos += si.nPage;
		break;
	case SB_PAGEUP: // Scroll one page up
		si.nPos -= si.nPage;
		break;
	case SB_THUMBPOSITION: // Scroll to the absolute position. The current position is provided in nPos
	case SB_THUMBTRACK: // Drag scroll box to specified position. The current position is provided in nPos
		si.nPos = nPos;
		break;
	case SB_TOP: // Scroll to top
		si.nPos = 0;
		break;
	}

	si.fMask = SIF_POS;
	si.nPos = max(min(si.nPos, si.nMax-(int)si.nPage),0);
	SetScrollInfo( SB_VERT, &si );

	offset -= si.nPos;
	ScrollWindow( 0, offset );
}


IMPLEMENT_DYNCREATE(CAnimationGraphStateParamsDlg,CDialog);

BEGIN_MESSAGE_MAP(CAnimationGraphStateParamsDlg,CDialog)
	ON_COMMAND(IDC_ADDPARAM, OnAddParam)
	ON_BN_CLICKED(IDC_EXCLUDEFROMGRAPH, OnCheckBoxChanged)
END_MESSAGE_MAP()

void CAnimationGraphStateParamsDlg::OnAddParam()
{
	CAnimationGraphStateParamsPanel* pParent = (CAnimationGraphStateParamsPanel*) GetParent();
	CStringDlg dlg;
	if ( dlg.DoModal() == IDOK )
	{
		pParent->OnAddParam( dlg.m_strString );
	}
}

void CAnimationGraphStateParamsDlg::OnCheckBoxChanged()
{
	CAnimationGraphStateParamsPanel* pParent = (CAnimationGraphStateParamsPanel*) GetParent();
	CAnimationGraphDialog* pOwner = (CAnimationGraphDialog*) pParent->GetOwner();
	CButton* pCheckBox = (CButton*) GetDlgItem( IDC_EXCLUDEFROMGRAPH );
	pOwner->SetExcludeFromGraph( pCheckBox->GetCheck() == BST_CHECKED );
}


void CAnimationGraphDialog::RegisterViewClass()
{
	GetIEditor()->GetClassFactory()->RegisterClass( new CAnimationGraphViewClass );
}

IMPLEMENT_DYNCREATE(CAnimationGraphDialog, CXTPFrameWnd);

BEGIN_MESSAGE_MAP(CAnimationGraphDialog, CXTPFrameWnd)
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_WM_DESTROY()
	ON_WM_CLOSE()
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_CONTEXTMENU()

	ON_COMMAND( ID_FILE_NEW,OnFileNew )
	ON_COMMAND( ID_FILE_OPEN,OnFileOpen )
	ON_COMMAND( ID_FILE_SAVE,OnFileSave )
	ON_COMMAND( ID_FILE_SAVE_AS,OnFileSaveAs )
	ON_COMMAND( ID_GRAPH_ADDSTATE,OnGraphAddState )
	ON_COMMAND( ID_GRAPH_ADDVIEW,OnGraphAddView )
	ON_COMMAND( ID_GRAPH_ADDINPUT,OnGraphAddInput )
	ON_COMMAND( ID_GRAPH_CREATEANIMATION,OnGraphCreateAnimation )
	ON_COMMAND( ID_GRAPH_TRIAL,OnGraphTrial )
	ON_COMMAND( ID_GRAPH_ISLANDREPORT,OnGraphIslandReport )
	ON_COMMAND( ID_GRAPH_SPEEDREPORT,OnGraphSpeedReport )
	ON_COMMAND( ID_GRAPH_DEADINPUTREPORT,OnGraphDeadInputReport )
	ON_COMMAND( ID_GRAPH_BADCALFILEREPORT,OnGraphBadCALReport )
	ON_COMMAND( ID_GRAPH_TRANSITIONLENGTHREPORT,OnGraphTransitionLengthReport )
	ON_COMMAND( ID_GRAPH_MATCHMOVEMENTTEMPLATESPEEDSTOANIMATIONSPEEDS,OnGraphMatchSpeeds )
	ON_COMMAND( ID_GRAPH_BADNULLNODEREPORT, OnGraphBadNullNodeReport )
	ON_COMMAND( ID_GRAPH_ORPHANNODEREPORT, OnGraphOrphanNodesReport )
	ON_COMMAND_RANGE( ID_VIEW_STATES, ID_VIEW_STATEPARAMS, OnViewPane )
	ON_COMMAND_RANGE( ID_VIEW_VIEWS, ID_VIEW_INPUTS, OnViewPane )
	ON_COMMAND( ID_VIEW_STATEQUERY, OnViewPaneStateQuery )
	ON_COMMAND( ID_AGLINK_MAPPINGCHANGED, OnAGLinkMappingChanged )

	ON_XTP_EXECUTE( ID_ANIMATION_GRAPH_ICON_SIZE, OnIconSizeChanged )
	ON_UPDATE_COMMAND_UI( ID_ANIMATION_GRAPH_ICON_SIZE, OnUpdateIconSizeUI )
	ON_XTP_EXECUTE( ID_ANIMATION_GRAPH_ICONS, OnDisplayIconsChanged )
	ON_UPDATE_COMMAND_UI( ID_ANIMATION_GRAPH_ICONS, OnUpdateDisplayIconsUI )
	ON_XTP_EXECUTE( ID_ANIMATION_GRAPH_PREVIEW, OnDisplayPreviewChanged )
	ON_UPDATE_COMMAND_UI( ID_ANIMATION_GRAPH_PREVIEW, OnUpdateDisplayPreviewUI )

	ON_NOTIFY( TVN_SELCHANGED, IDC_ANIMGRAPH_STATES, OnStateListSelChanged )
	ON_NOTIFY( TVN_BEGINDRAG, IDC_ANIMGRAPH_STATES, OnBeginDrag )

	ON_NOTIFY( TVN_SELCHANGED, IDC_ANIMGRAPH_VIEWS, OnViewListSelChanged )
	ON_NOTIFY( TVN_SELCHANGED, IDC_ANIMGRAPH_INPUTS, OnInputListSelChanged )

	//////////////////////////////////////////////////////////////////////////
	// XT Commands.
	ON_MESSAGE(XTPWM_DOCKINGPANE_NOTIFY, OnDockingPaneNotify)
	ON_MESSAGE( XTPWM_TASKPANEL_NOTIFY, OnTaskPanelNotify )
END_MESSAGE_MAP()

CAnimationGraphDialog::CAnimationGraphDialog()
	: m_stateListCtrl(this)
	, m_viewListCtrl(this)
	, m_inputListCtrl(this)
	, m_pDragImage(NULL)
	, m_nBlockSelChanged(0)
	, m_previewDialog(s_szCharacterName)
	, m_pView(NULL)
{
	RegisterConsoleVariables();

	//m_enableIcons->Set(true);

	m_iconSizes.push_back(std::make_pair(64, 64));
	m_iconSizes.push_back(std::make_pair(96, 96));
	m_iconSizes.push_back(std::make_pair(128, 128));
	m_iconSizes.push_back(std::make_pair(192, 192));
	m_iconSizes.push_back(std::make_pair(256, 256));
	m_iconSizes.push_back(std::make_pair(512, 512));

	WNDCLASS wndcls;
	HINSTANCE hInst = AfxGetInstanceHandle();
	if (!(::GetClassInfo(hInst, ANIMATIONGRAPH_DIALOGFRAME_CLASSNAME, &wndcls)))
	{
		// otherwise we need to register a new class
		wndcls.style            = 0 /*CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW*/;
		wndcls.lpfnWndProc      = ::DefWindowProc;
		wndcls.cbClsExtra       = wndcls.cbWndExtra = 0;
		wndcls.hInstance        = hInst;
		wndcls.hIcon            = NULL;
		wndcls.hCursor          = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
		wndcls.hbrBackground    = NULL; // (HBRUSH) (COLOR_3DFACE + 1);
		wndcls.lpszMenuName     = NULL;
		wndcls.lpszClassName    = ANIMATIONGRAPH_DIALOGFRAME_CLASSNAME;
		if (!AfxRegisterClass(&wndcls))
		{
			AfxThrowResourceException();
		}
	}
	CRect rc(0,0,0,0);
	BOOL bRes = Create( WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,rc,AfxGetMainWnd() );
	if (!bRes)
		return;
	ASSERT( bRes );

	OnInitDialog();
}

CAnimationGraphDialog::~CAnimationGraphDialog()
{
	m_stateEditor.ClearActiveItem();
}

void
CAnimationGraphDialog::OnDestroy()
{
	m_stateEditor.ClearActiveItem();
	if (m_pGraph)
	{
		m_pGraph->RemoveListener(this);
		m_pGraph = 0;
	}
	if (m_pViewHypergraph)
	{
		m_pViewHypergraph->RemoveListener(this);
		m_pViewHypergraph = 0;
	}

	CXTPDockingPaneLayout layout( GetDockingPaneManager() );
	GetDockingPaneManager()->GetLayout( &layout );
	layout.Save( _T("AnimationGraphLayout") );

	__super::OnDestroy();
}

BOOL CAnimationGraphDialog::Create( DWORD dwStyle,const RECT& rect,CWnd* pParentWnd )
{
	BOOL bReturn = __super::Create( ANIMATIONGRAPH_DIALOGFRAME_CLASSNAME,NULL,dwStyle,rect,pParentWnd );

	m_pCAnimationImageManager = new CAnimationImageManager(this, s_szCharacterName, std::make_pair(64, 64));

	return bReturn;
}

void CAnimationGraphDialog::OnCharacterChanged()
{
	// update the graph and mark it as modified
	if ( m_pGraph )
		m_pGraph->SetCharacterName( m_previewDialog.GetModelViewport()->GetLoadedFileName() );

	// recreate the icons
	m_pCAnimationImageManager->ChangeModelAssetName( m_previewDialog.GetModelViewport()->GetLoadedFileName() );

	// invalidate the entire view.
	SetActiveView( m_pView );
}

BOOL CAnimationGraphDialog::OnInitDialog()
{
	try
	{
		//////////////////////////////////////////////////////////////////////////
		// Initialize the command bars
		if (!InitCommandBars())
			return -1;

	}	catch (CResourceException *e)
	{
		e->Delete();
		return -1;
	}

	ModifyStyleEx( WS_EX_CLIENTEDGE, 0 );

	// Get a pointer to the command bars object.
	CXTPCommandBars* pCommandBars = GetCommandBars();
	if(pCommandBars == NULL)
	{
		TRACE0("Failed to create command bars object.\n");
		return -1;      // fail to create
	}

	// Add the menu bar
	CXTPCommandBar* pMenuBar = pCommandBars->SetMenu( _T("Menu Bar"),IDR_ANIMATIONGRAPH );
	ASSERT(pMenuBar);
	pMenuBar->SetFlags(xtpFlagStretched);
	pMenuBar->EnableCustomization(FALSE);

	//////////////////////////////////////////////////////////////////////////
	GetDockingPaneManager()->InstallDockingPanes(this);
	GetDockingPaneManager()->SetTheme(xtpPaneThemeOffice2003);
	GetDockingPaneManager()->SetThemedFloatingFrames(TRUE);
	//////////////////////////////////////////////////////////////////////////

	/* WARNING: If you add or remove panes, make sure you update the ANIMATION_GRAPH_PANE_COUNT macro
	to the new number of panes or things won't work correctly! */

	// States
	CRect rectStateListCtrl(0,0,220,500);
	m_stateListCtrl.Create( /*WS_VISIBLE|*/WS_CHILD|WS_TABSTOP|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,rectStateListCtrl,this,IDC_ANIMGRAPH_STATES );
	m_stateListCtrl.ReloadStates();

	CXTPDockingPane* pStatesPane = GetDockingPaneManager()->CreatePane( IDW_ANIMGRAPH_STATES_PANE,rectStateListCtrl,dockLeftOf );
	pStatesPane->SetTitle( _T("States") );

	// Views
	CRect rectViewListCtrl(0,0,220,200);
	m_viewListCtrl.Create( /*WS_VISIBLE|*/WS_CHILD|WS_TABSTOP|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,rectViewListCtrl,this,IDC_ANIMGRAPH_VIEWS );
	m_viewListCtrl.ReloadViews();

	CXTPDockingPane* pViewsPane = GetDockingPaneManager()->CreatePane( IDW_ANIMGRAPH_VIEWS_PANE,rectViewListCtrl,dockBottomOf,pStatesPane );
	pViewsPane->SetTitle( _T("Views") );

	// Inputs
	CRect rectInputListCtrl(0,0,220,150);
	m_inputListCtrl.Create( /*WS_VISIBLE|*/WS_CHILD|WS_TABSTOP|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,rectInputListCtrl,this,IDC_ANIMGRAPH_INPUTS );
	m_inputListCtrl.ReloadInputs();

	CXTPDockingPane* pInputsPane = GetDockingPaneManager()->CreatePane( IDW_ANIMGRAPH_INPUTS_PANE,rectInputListCtrl,dockBottomOf,pViewsPane );
	pInputsPane->SetTitle( _T("Inputs") );

	// State Params
	m_stateParamsCtrl.Create( m_stateParamsCtrl.IDD );
	m_stateParamsCtrl.ModifyStyle( 0, WS_VSCROLL );
	m_stateParamsCtrl.ModifyStyleEx( 0, WS_EX_STATICEDGE );
	m_stateParamsCtrl.Init();

	CRect rectStateParamsCtrl( 0, 0, 500, 180);
	CXTPDockingPane* pStateParamsPane = GetDockingPaneManager()->CreatePane( IDW_ANIMGRAPH_STATE_PARAMS_PANE,rectStateParamsCtrl,dockBottomOf );
	pStateParamsPane->SetTitle( _T("State Params") );

	// State Editor
	CXTPDockingPane* pStateEditorPane = m_stateEditor.Init(this, IDW_ANIMGRAPH_STATE_EDITOR_PANE);
	m_stateEditor.GetPanel()->SetOwner(this);

	// Preview Options
	CRect rectPreviewOptionsCtrl(0,0,220,300);
	CXTPDockingPane* pPreviewOptionsPane = GetDockingPaneManager()->CreatePane( IDW_ANIMGRAPH_PREVIEW_OPTIONS_PANE,rectPreviewOptionsCtrl,dockRightOf, pStateEditorPane);
	pPreviewOptionsPane->SetTitle( _T("Preview Options") );
	pPreviewOptionsPane->Hide();

	// Preview
	CRect rectPreviewCtrl(0,0,220,300);
	m_previewDialog.Create( CAnimationGraphPreviewDialog::IDD,this );
	m_previewManager.SetViewport( m_previewDialog.GetModelViewport() );
	m_previewDialog.GetModelViewport()->SetCharacterChangeListener( this );
	CXTPDockingPane* pPreviewPane = GetDockingPaneManager()->CreatePane( IDW_ANIMGRAPH_PREVIEW_PANE,rectPreviewCtrl,dockTopOf, pStateEditorPane);
	pPreviewPane->SetTitle( _T("Preview") );

	m_rollupCtrl.Create(/*WS_VISIBLE|*/WS_CHILD,CRect(4, 4, 187, 362),this, NULL);
	CharPanel_Animation* pAnimationPanel = new CharPanel_Animation(m_previewDialog.GetModelViewport(), NULL);
	m_previewDialog.GetModelViewport()->SetModelPanelA(pAnimationPanel);
	m_previewDialog.GetModelViewport()->UpdateAnimationList();
	/*int animationControlRollupID = */m_rollupCtrl.InsertPage("Character Animation", pAnimationPanel);
	CPropertiesPanel* s_varsPanel;
	s_varsPanel = new CPropertiesPanel(this);
	s_varsPanel->AddVars( m_previewDialog.GetModelViewport()->GetVarObject()->GetVarBlock() );
	/*int propertiesRollupID = */m_rollupCtrl.InsertPage("Debug Options", s_varsPanel);

	// Testing
	m_tester.Init( this );
	CXTPDockingPane* pTestPane = GetDockingPaneManager()->CreatePane( IDW_ANIMGRAPH_TEST_PANE, rectStateListCtrl, dockRightOf );
	pTestPane->SetTitle( _T("Testing") );
	pTestPane->Hide();

	// State Query
	m_stateQuery.Init( this );
	CXTPDockingPane* pStateQueryPane = GetDockingPaneManager()->CreatePane( IDW_ANIMGRAPH_STATE_QUERY_PANE, rectStateListCtrl, dockRightOf, pStateEditorPane );
	pStateQueryPane->SetTitle( _T("State Query") );
	pStateQueryPane->Hide();

	// Main view
	CRect rc;
	GetClientRect(&rc);
	m_view.Create( WS_CHILD|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|WS_VISIBLE, rc, this, AFX_IDW_PANE_FIRST);
	m_view.ModifyStyleEx( 0, WS_EX_STATICEDGE );

	// Status bar
	m_statusBar.Create(this);
#define ID_FILEINFO 1
	static UINT indicators[] = 
	{
		ID_SEPARATOR,
		ID_FILEINFO
	};
	m_statusBar.SetIndicators( indicators, sizeof(indicators)/sizeof(*indicators) );
	m_statusBar.SetPaneInfo(1, ID_FILEINFO, SBPS_NORMAL, 500);
	UpdateTitle();

	// Toolbar
	VERIFY(m_wndToolBar.CreateToolBar(WS_VISIBLE|WS_CHILD|CBRS_TOOLTIPS|CBRS_GRIPPER, this, AFX_IDW_TOOLBAR));
	VERIFY(m_wndToolBar.LoadToolBar(IDR_ANIMATION_GRAPH));
	m_wndToolBar.SetFlags(xtpFlagStretched);
	m_wndToolBar.EnableCustomization(FALSE);

	// Create the resolution combo box.
	{
		CXTPControl *pCtrl = m_wndToolBar.GetControls()->FindControl(xtpControlButton, ID_ANIMATION_GRAPH_ICON_SIZE, TRUE, FALSE);
		if (pCtrl)
		{
			int nIndex = pCtrl->GetIndex();
			CXTPControlComboBox *pCombo = (CXTPControlComboBox*)m_wndToolBar.GetControls()->SetControlType(nIndex,xtpControlComboBox);
			pCombo->SetWidth(80);

			int index = 0;
			for (IconSizeList::iterator itSize = m_iconSizes.begin(); itSize != m_iconSizes.end(); ++itSize)
			{
				int width = (*itSize).first;
				int height = (*itSize).second;
				string text;
				text.Format("%d x %d", width, height);
				pCombo->InsertString(index, text.c_str());
				++index;
			}
			pCombo->SetCurSel(m_displayedIconSize->GetIVal());
			m_pCAnimationImageManager->SetImageSize(m_iconSizes[m_displayedIconSize->GetIVal()]);
		}

		ReadDisplayedIconSize();
	}

	// Create the enable/disable icons button.
	{
		CXTPControl *pCtrl = m_wndToolBar.GetControls()->FindControl(xtpControlButton, ID_ANIMATION_GRAPH_ICONS, TRUE, FALSE);
		if (pCtrl)
		{
			int nIndex = pCtrl->GetIndex();
			CXTPControlButton *pPopup = (CXTPControlButton*)m_wndToolBar.GetControls()->SetControlType(nIndex,xtpControlButton);
			pPopup->SetCaption("Display Icons");
			pPopup->SetStyle(xtpButtonCaption);
		}
	}

	// Create the enable/disable preview button.
	{
		CXTPControl *pCtrl = m_wndToolBar.GetControls()->FindControl(xtpControlButton, ID_ANIMATION_GRAPH_PREVIEW, TRUE, FALSE);
		if (pCtrl)
		{
			int nIndex = pCtrl->GetIndex();
			CXTPControlButton *pPopup = (CXTPControlButton*)m_wndToolBar.GetControls()->SetControlType(nIndex,xtpControlButton);
			pPopup->SetCaption("Preview Animations");
			pPopup->SetStyle(xtpButtonCaption);
			m_previewManager.EnablePreview( m_preview->GetIVal() );
		}
	}

	CXTPDockingPaneLayout layout( GetDockingPaneManager() );
	if ( layout.Load( _T("AnimationGraphLayout") ) && layout.GetPaneList().GetCount() >= ANIMATION_GRAPH_PANE_COUNT )
		GetDockingPaneManager()->SetLayout( &layout );

	return TRUE;
}

void CAnimationGraphDialog::OnFileNew()
{
	SetGraph( new CAnimationGraph(m_pCAnimationImageManager) );
}

void CAnimationGraphDialog::OnFileOpen()
{
	CAnimationGraphPtr pGraph = new CAnimationGraph(m_pCAnimationImageManager);
	CString filename;
	if (CFileUtil::SelectSingleFile( EFILE_TYPE_ANY,filename,ANIMATIONGRAPH_FILE_FILTER ))
	{
		CWaitCursor wait;
		if (pGraph->Load( filename ))
			SetGraph( pGraph );
	}
}

void CAnimationGraphDialog::OnFileSave()
{
	if (!m_pGraph)
		return;

	m_view.ClearSelection();
	m_stateEditor.ClearActiveItem();

	if (m_pGraph->GetFilename().IsEmpty())
		OnFileSaveAs();
	else
	{
		CWaitCursor wait;
		m_pGraph->Save();

		m_stateQuery.RunUnitTests( true, false );
		m_stateQuery.Reload( false );
	}
}

void CAnimationGraphDialog::OnFileSaveAs()
{
	CString filename = m_pGraph->GetFilename();
	CString dir = Path::GetPath(filename);
	if (CFileUtil::SelectSaveFile( ANIMATIONGRAPH_FILE_FILTER,"xml",dir,filename ))
	{
		m_view.ClearSelection();
		m_stateEditor.ClearActiveItem();

		CWaitCursor wait;
		m_pGraph->SetFilename( filename );
		m_pGraph->Save();

		m_stateQuery.Reload( false );
	}
}

void CAnimationGraphDialog::SetGraph( CAnimationGraphPtr pGraph )
{
	m_pView = NULL;
	m_vStates.clear();

	m_view.SetGraph( NULL );
	m_pViewHypergraph = NULL;
	m_stateEditor.ClearActiveItem();
	if (m_pGraph)
		m_pGraph->RemoveListener(this);
	m_pGraph = pGraph;
	if (m_pGraph)
		m_pGraph->AddListener(this);
	m_stateListCtrl.ReloadStates();
	m_viewListCtrl.ReloadViews();
	m_inputListCtrl.ReloadInputs();
	m_tester.Reload();
	m_stateQuery.Reload( false );
	m_previewDialog.SetCharacter( pGraph->GetCharacterName().IsEmpty() ? s_szCharacterName : pGraph->GetCharacterName() );
	pGraph->ShowIcons(m_enableIcons->GetIVal());

	UpdateTitle();
}

void CAnimationGraphDialog::UpdateTitle()
{
	CString titleFormatter;
	if (m_pGraph)
	{
		if (m_pGraph->IsModified())
			titleFormatter += "modified ";
		titleFormatter += m_pGraph->GetFilename();
	}
	else
	{
		titleFormatter += "No file open.";
	}
	m_statusBar.SetPaneText(1, titleFormatter);
}

CAnimationGraphPtr CAnimationGraphDialog::GetAnimationGraph()
{
	return m_pGraph;
}

bool CAnimationGraphDialog::ResetParameterization()
{
	if ( !m_stateEditor.IsParameterized() )
		return true;

	if ( AfxMessageBox("The selected item has custom parameterized data which will be lost by this action! \n\nProceed?", MB_YESNO|MB_DEFBUTTON2) == IDNO )
		return false;

	m_stateEditor.ResetParameterization();
	return true;
}

void CAnimationGraphDialog::OnStateListSelChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	CWaitCursor wait;

	*pResult = TRUE;
	if (!m_pGraph)
		return;
	if (m_nBlockSelChanged)
		return;
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
	HTREEITEM treeitem = pNMTreeView->itemNew.hItem;
	if (treeitem != NULL)
	{
		m_inputListCtrl.SelectItem(NULL);
	}
	CString item = m_stateListCtrl.GetItemText(treeitem);
	m_previewManager.SetState(CAGStatePtr(0));
	m_vStates.clear();
	m_view.ClearSelection();
	SetActiveState( m_pGraph->FindState(item) );
}

void CAnimationGraphDialog::OnViewListSelChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	CWaitCursor wait;

	*pResult = TRUE;
	if (!m_pGraph)
		return;
	if (m_nBlockSelChanged)
		return;
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
	HTREEITEM treeitem = pNMTreeView->itemNew.hItem;
	if (treeitem != NULL)
	{
		m_stateListCtrl.SelectItem(NULL);
		m_inputListCtrl.SelectItem(NULL);
	}
	CString item = m_viewListCtrl.GetItemText(treeitem);
	m_previewManager.SetState(CAGStatePtr(0));
	m_vStates.clear();
	SetActiveView( m_pGraph->FindView(item) );
	m_stateEditor.EditPropertiesOf( m_pGraph->FindView(item) );
}

void CAnimationGraphDialog::OnInputListSelChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	CWaitCursor wait;

	*pResult = TRUE;
	if (!m_pGraph)
		return;
	if (m_nBlockSelChanged)
		return;
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
	HTREEITEM treeitem = pNMTreeView->itemNew.hItem;
	if (treeitem != NULL)
	{
		m_stateListCtrl.SelectItem(NULL);
	}
	CString item = m_inputListCtrl.GetItemText(treeitem);
	m_previewManager.SetState(CAGStatePtr(0));
	m_vStates.clear();
	m_view.ClearSelection();
	m_stateEditor.EditPropertiesOf( m_pGraph->FindInput(item) );
}

void CAnimationGraphDialog::OnGraphModified()
{
	UpdateTitle();
	m_stateQuery.Reload( true );
}

void CAnimationGraphDialog::SetActiveState( CAGStatePtr pState )
{
	m_vStates.clear();
	if ( pState )
	{
		m_vStates.push_back( pState );
		m_stateEditor.EditPropertiesOf( pState );
		m_previewManager.SetState( pState );
		if ( m_pView )
		{
			CHyperNode* pNode = m_pView->GetNodeForState( pState );
			if ( pNode && !pNode->IsSelected() )
				m_view.ShowAndSelectNode( pNode, true );
		}
	}
	else
	{
		m_stateEditor.ClearActiveItem();
		m_previewManager.SetState( CAGStatePtr(0) );
	}
}

void CAnimationGraphDialog::SetActiveStates( std::vector< CAGStatePtr > vStates )
{
	if ( vStates.empty() )
		SetActiveState( NULL );
	else if ( vStates.size() == 1 )
		SetActiveState( vStates[0] );
	else
	{
		m_stateEditor.EditPropertiesOf( vStates );
		m_previewManager.SetState( CAGStatePtr(0) );
		if ( m_pView )
		{
			std::vector< CAGStatePtr >::iterator it, itEnd = vStates.end();
			for ( it = vStates.begin(); it != itEnd; ++it )
			{
				CAGState* pState = *it;
				CHyperNode* pNode = m_pView->GetNodeForState( pState );
				if ( pNode && !pNode->IsSelected() )
					m_view.ShowAndSelectNode( pNode, true );
			}
		}
	}
}

void CAnimationGraphDialog::SetActiveView( CAGViewPtr pView )
{
	if (m_pViewHypergraph)
	{
		m_pViewHypergraph->RemoveListener(this);
		m_pViewHypergraph = NULL;
		m_view.SetGraph(NULL);
	}
	m_pView = pView;
	if (m_pView)
	{
		m_pViewHypergraph = pView->GetHyperGraph();
		m_pViewHypergraph->AddListener(this);
		m_view.SetGraph( m_pViewHypergraph );
	}
}

void CAnimationGraphDialog::OnStateEvent( EAGStateEvent evt, CAGStatePtr pState )
{
	switch (evt)
	{
	case eAGSE_ChangeName:
	case eAGSE_Cloned:
	case eAGSE_Removed:
		++m_nBlockSelChanged;
		m_stateListCtrl.ReloadStates();
		m_tester.Reload();
		m_stateQuery.Reload( true );
		SetActiveView( m_pView );
		if (evt != eAGSE_Removed)
		{
			SetActiveState( pState );
			m_stateListCtrl.SelectItemByName( pState->GetName() );
		}
		--m_nBlockSelChanged;
		break;
	case eAGSE_ChangeUseTemplate:
		m_stateEditor.StateEvent( eSEE_RebuildExtraPanels );
		m_stateQuery.Reload( true );
		break;
	case eAGSE_ChangeParent:
		m_stateQuery.Reload( true );
		break;
	case eAGSE_ChangeIconSnapshotTime:
		{
			m_pCAnimationImageManager->InvalidateImage(pState);

			class InvalidateNodeTreeRecursive
			{
			public:
				static void Perform(CHyperGraph* pViewHypergraph, CHyperNode* pNode)
				{
					if (pNode)
					{
						pViewHypergraph->InvalidateNode(pNode);
						pNode->Invalidate(true);
						IHyperGraphEnumerator* pEnum = pNode->GetChildrenEnumerator();
						if (pEnum)
						{
							for (IHyperNode* pChild = pEnum->GetFirst(); pChild; pChild = pEnum->GetNext())
								Perform(pViewHypergraph, static_cast<CHyperNode*>(pChild));
							pEnum->Release();
						}
					}
				}
			};

			InvalidateNodeTreeRecursive::Perform(m_pViewHypergraph, m_pView->GetNodeForState(pState));
		}
		break;
	}
}

void CAnimationGraphDialog::OnViewEvent( EAGViewEvent evt, CAGViewPtr pView )
{
	switch (evt)
	{
	case eAGVE_ChangeName:
		++m_nBlockSelChanged;
		m_viewListCtrl.ReloadViews();
		m_tester.Reload();
		SetActiveView( m_pView );
		m_viewListCtrl.SelectItemByName( m_pView->GetName() );
		--m_nBlockSelChanged;
		break;
	case eAGVE_Removed:
		++m_nBlockSelChanged;
		m_stateEditor.ClearActiveItem();
		m_viewListCtrl.ReloadViews();
		SetActiveView( NULL );
		--m_nBlockSelChanged;
		break;
	}
}

void CAnimationGraphDialog::OnInputEvent( EAGInputEvent evt, CAGInputPtr pInput )
{
	switch (evt)
	{
	case eAGIE_ChangeName:
	case eAGIE_ChangeType:
		++m_nBlockSelChanged;
		m_inputListCtrl.ReloadInputs();
		m_tester.Reload();
		m_stateQuery.Reload( true );
		SetActiveView( m_pView );
		m_inputListCtrl.SelectItemByName( pInput->GetName() );
		--m_nBlockSelChanged;
		break;
	}
}

LRESULT CAnimationGraphDialog::OnTaskPanelNotify(WPARAM wParam, LPARAM lParam)
{
	switch(wParam) 
	{
	case XTP_TPN_CLICK:
		{
			CXTPTaskPanelGroupItem* pItem = (CXTPTaskPanelGroupItem*)lParam;
			UINT nCmdID = pItem->GetID();
			if (nCmdID < 60)
				m_tester.OnCommand(nCmdID);
			else if (nCmdID < 100)
				m_stateQuery.OnCommand(nCmdID);
			else
				m_stateEditor.OnCommand(nCmdID);
		}
		break;

	case XTP_TPN_RCLICK:
		break;
	}

	return 0;
}

void CAnimationGraphDialog::OnHyperGraphEvent( IHyperNode * pNode, EHyperGraphEvent event )
{
	switch (event)
	{
	case EHG_NODE_CHANGE:
		{
			std::vector< CHyperNode* > nodes;
			m_view.GetSelectedNodes( nodes, CHyperGraphView::SELECTION_SET_ONLY_PARENTS );
			if ( nodes.size() == 1 )
				SetActiveState( ((CAGNode*)nodes[0])->GetState() );
			else if ( nodes.size() > 1 )
			{
				if ( m_pView )
				{
					std::vector< CAGStatePtr > states;
					std::vector< CHyperNode* >::iterator it, itEnd = nodes.end();
					for ( it = nodes.begin(); it != itEnd; ++it )
						if ( CAGState* pState = ((CAGNode*)*it)->GetState() )
							states.push_back( pState );
					SetActiveStates( states );
				}
			}
			else
			{
				SetActiveState( NULL );
				if ( m_pView && !nodes.size() )
					m_view.ClearSelection();
			}
		}
		break;
	}
}

void CAnimationGraphDialog::OnLinkEdit( CHyperEdge * pEdge )
{
	CAGLinkPtr pLink = ((CAGEdge*)pEdge)->pLink;
	m_stateEditor.EditPropertiesOf( pLink );
}

void CAnimationGraphDialog::OnBeginDrag( NMHDR* pNMHDR, LRESULT* pResult )
{
	NM_TREEVIEW * pNMTreeView = (NM_TREEVIEW*)pNMHDR;

	*pResult = TRUE;
	bool doDrag = false;

	HTREEITEM hItem = pNMTreeView->itemNew.hItem;
	m_dragNodeClass = m_stateListCtrl.GetItemText(hItem);
	doDrag = !!m_pView && m_pView->CanAddState( m_pGraph->FindState(m_dragNodeClass) );

	if (doDrag)
	{
		m_pDragImage = m_stateListCtrl.CreateDragImage( hItem );
		if (m_pDragImage)
		{
			m_pDragImage->BeginDrag(0, CPoint(-10, -10));
			CRect rc;
			GetWindowRect(rc);
			CPoint p = pNMTreeView->ptDrag;
			ClientToScreen( &p );
			p.x -= rc.left;
			p.y -= rc.top;
			m_pDragImage->DragEnter( this, p );
			SetCapture();

			*pResult = FALSE;
		}
	}
}

void CAnimationGraphDialog::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_pDragImage)
	{
		CRect rc;
		GetWindowRect(rc);
		CPoint p = point;
		ClientToScreen(&p);
		p.x -= rc.left;
		p.y -= rc.top;
		m_pDragImage->DragMove(p);
		return;
	}
	__super::OnMouseMove(nFlags, point);
}

void CAnimationGraphDialog::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_pDragImage)
	{
		CPoint p;
		GetCursorPos( &p );

		m_pDragImage->DragLeave( this );
		m_pDragImage->EndDrag();
		delete m_pDragImage;
		m_pDragImage = 0;
		ReleaseCapture();

		CWnd *wnd = CWnd::WindowFromPoint( p );
		if (wnd == &m_view)
		{
			m_view.ScreenToClient(&p);
			m_view.CreateNode( m_dragNodeClass,p );
		}

		return;
	}
	__super::OnLButtonUp(nFlags, point);
}

void CAGHyperGraphView::ShowContextMenu( CPoint point, CHyperNode* pNode )
{
	OnLButtonDown( 0, point );
	OnLButtonUp( 0, point );
	if ( pNode )
	{
		ClientToScreen( &point );
		( (CAnimationGraphDialog*) GetParent() )->OnContextMenu( this, point );
	}
}

void CAnimationGraphDialog::OnContextMenu( CWnd* pWnd, CPoint pos )
{
	if ( pos.x == -1 && pos.y == -1 )
	{
		__super::OnContextMenu( pWnd, pos );
		return;
	}

	CWnd* wnd = WindowFromPoint( pos );
	if ( !wnd || wnd != &m_stateListCtrl /*&& wnd != &m_viewListCtrl && wnd != &m_inputListCtrl*/ && wnd != &m_view )
	{
		__super::OnContextMenu( pWnd, pos );
		return;
	}

	if ( wnd == &m_stateListCtrl )
	{
		TVHITTESTINFO hit;
		hit.pt = pos;
		m_stateListCtrl.ScreenToClient( &hit.pt );
		m_stateListCtrl.HitTest( &hit );
		if ( !hit.hItem )
		{
			__super::OnContextMenu( pWnd, pos );
			return;
		}
		m_stateListCtrl.SetFocus();
		if ( m_vStates.size() != 1 && m_stateListCtrl.GetSelectedItem() == hit.hItem )
			m_stateListCtrl.SelectItem( 0 );
		m_stateListCtrl.SelectItem( hit.hItem );
	}

	CAGState* pState = m_vStates.size() == 1 ? m_vStates[0] : NULL;
	if ( wnd == &m_view )
	{
		CPoint local( pos );
		m_view.ScreenToClient( &local );
		CAGNode* pNode = (CAGNode*) m_view.GetNodeAtPoint( local );
		if ( pNode )
			pState = pNode->GetState();
	}
	if ( !pState )
	{
		__super::OnContextMenu( pWnd, pos );
		return;
	}

	CMenu menu, viewMenu, fromMenu, toMenu, childMenu, activeParamMenu, excludedParamMenu;
	VERIFY( menu.CreatePopupMenu() );
	VERIFY( viewMenu.CreatePopupMenu() );
	VERIFY( fromMenu.CreatePopupMenu() );
	VERIFY( toMenu.CreatePopupMenu() );
	VERIFY( childMenu.CreatePopupMenu() );
	VERIFY( activeParamMenu.CreatePopupMenu() );
	VERIFY( excludedParamMenu.CreatePopupMenu() );

	int id = 15000;
	for ( CAnimationGraph::view_iterator it = m_pGraph->ViewBegin(); it != m_pGraph->ViewEnd(); ++it )
	{
		CAGViewPtr pView = *it;
		if ( pView->HasState(pState) )
			viewMenu.AppendMenu( MF_STRING | ( m_pView == pView ? MF_CHECKED : 0 ), id++, pView->GetName() );
	}
	int idStates = id;
	std::vector< CAGStatePtr > states;
	m_pGraph->StatesLinkedTo( states, pState );
	std::vector< CAGStatePtr >::iterator it, itEnd = states.end();
	for ( it = states.begin(); it != itEnd; ++it )
	{
		CAGState* pStateIt = *it;
		fromMenu.AppendMenu( MF_STRING, id++, pStateIt->GetName() );
	}
	states.clear();
	m_pGraph->StatesLinkedFrom( states, pState );
	itEnd = states.end();
	for ( it = states.begin(); it != itEnd; ++it )
	{
		CAGStatePtr pStateIt = *it;
		toMenu.AppendMenu( MF_STRING, id++, pStateIt->GetName() );
	}
	states.clear();
	m_pGraph->ChildStates( states, pState );
	itEnd = states.end();
	for ( it = states.begin(); it != itEnd; ++it )
	{
		CAGStatePtr pStateIt = *it;
		childMenu.AppendMenu( MF_STRING, id++, pStateIt->GetName() );
	}

	// Create active and excluded parameters menu lists
	int idStateParams = id;
	std::vector< TParameterizationId > paramIds;
	pState->GetParameterizations( paramIds );
	std::vector< TParameterizationId >::const_iterator itIds;
	for ( itIds = paramIds.begin(); itIds != paramIds.end(); ++itIds )
	{
		if ( pState->IsParameterizationExcluded( &(*itIds) ) )
		{
			excludedParamMenu.AppendMenu( MF_STRING, id++, itIds->AsString() );
		}
		else
		{
			activeParamMenu.AppendMenu( MF_STRING, id++, itIds->AsString() );
		}
	}
	

	menu.AppendMenu( MF_POPUP, (UINT) viewMenu.m_hMenu, _T("Find in View") );
	menu.AppendMenu( MF_SEPARATOR );
	menu.AppendMenu( MF_POPUP, (UINT) fromMenu.m_hMenu, _T("Linked From") );
	menu.AppendMenu( MF_POPUP, (UINT) toMenu.m_hMenu, _T("Linked To") );
	menu.AppendMenu( MF_POPUP, (UINT) childMenu.m_hMenu, _T("Child States") );
	menu.AppendMenu( MF_SEPARATOR );
	menu.AppendMenu( MF_POPUP, (UINT) activeParamMenu.m_hMenu, _T("Active Param Combinations") );
	menu.AppendMenu( MF_POPUP, (UINT) excludedParamMenu.m_hMenu, _T("Excluded Param Combinations") );

	if ( wnd == &m_view )
	{
		menu.AppendMenu( MF_SEPARATOR );
		menu.AppendMenu( MF_STRING, 32767, _T("Remove from View") );
		menu.AppendMenu( MF_STRING, 32766, _T("Remove and Disconnect") );
	}

	// track menu	
	int nMenuResult = CXTPCommandBars::TrackPopupMenu( &menu, TPM_NONOTIFY|TPM_RETURNCMD|TPM_LEFTALIGN|TPM_RIGHTBUTTON, pos.x, pos.y, this, NULL );

	if ( nMenuResult == 32766 )
	{
		// Remove and Disconnect
		m_view.SendMessage( WM_COMMAND, ID_EDIT_DELETE, 0 );
	}
	else if ( nMenuResult == 32767 )
	{
		// Remove from View
		m_view.SendMessage( WM_COMMAND, ID_EDIT_DELETE_KEEP_LINKS, 0 );
	}
	if ( nMenuResult >= 15000 && nMenuResult < idStates )
	{
		CWaitCursor wait;
		CString view;
		viewMenu.GetMenuString( nMenuResult, view, MF_BYCOMMAND );
		CAGViewPtr pView = m_pGraph->FindView( view );
		if ( pView != m_pView )
		{
			SetActiveView( pView );
			m_viewListCtrl.SelectItemByName( view );
		}
		CHyperNode* pNode = m_pView->GetNodeForState( pState );
		if ( pNode )
			m_view.ShowAndSelectNode( pNode, true );
		m_stateListCtrl.SelectItemByName( pState->GetName() );
		m_view.SetFocus();
	}
	else if ( nMenuResult >= idStates && nMenuResult < idStateParams )
	{
		CWaitCursor wait;
		CString state;
		if (!fromMenu.GetMenuString( nMenuResult, state, MF_BYCOMMAND ))
			if (!toMenu.GetMenuString( nMenuResult, state, MF_BYCOMMAND ))
				childMenu.GetMenuString( nMenuResult, state, MF_BYCOMMAND );

		HTREEITEM hState = m_stateListCtrl.GetRootItem();
		while ( hState && m_stateListCtrl.GetItemText(hState) != state )
			hState = m_stateListCtrl.GetNextItem( hState, TVGN_NEXT );
		if ( hState )
		{
			m_stateListCtrl.EnsureVisible( hState );
			m_stateListCtrl.SelectItem( hState );
			SetActiveState( m_pGraph->FindState(state) );
			if ( m_pView )
			{
				CHyperNode* pNode = m_pView->GetNodeForState( pState );
				if ( pNode )
					m_view.ShowAndSelectNode( pNode, true );
			}
		}
	}
	else if ( nMenuResult > idStateParams )
	{
		// TODO: Handle state params here...
	}
}

void CAnimationGraphDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);
	if (::IsWindow(m_rollupCtrl.m_hWnd))
		m_rollupCtrl.Invalidate();
}

void CAnimationGraphDialog::OnIconSizeChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMXTPCONTROL* tagNMCONTROL = (NMXTPCONTROL*)pNMHDR;
	CXTPControlComboBox* pControl = (CXTPControlComboBox*)tagNMCONTROL->pControl;
	if (pControl->GetType() == xtpControlComboBox)
	{
		int sel = pControl->GetCurSel();
		if (sel != CB_ERR)
		{
			const IconSize& iconSize = m_iconSizes[sel];
			m_pCAnimationImageManager->SetImageSize(iconSize);

			// Invalidate the entire view.
			SetActiveView(m_pView);
		}
	}
	*pResult = 1;
}

void CAnimationGraphDialog::OnUpdateIconSizeUI(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_enableIcons->GetIVal());

	if (m_iconSizes[m_displayedIconSize->GetIVal()] != m_pCAnimationImageManager->GetImageSize())
	{
		CXTPControlComboBox *pControl = (CXTPControlComboBox*)m_wndToolBar.GetControls()->FindControl(xtpControlComboBox, ID_ANIMATION_GRAPH_ICON_SIZE, TRUE, FALSE);
		if (!pControl || pControl->GetType() != xtpControlComboBox)
			return;
		int sel = CB_ERR;
		IconSize iconSize = m_pCAnimationImageManager->GetImageSize();
		for (int index = 0; index < int(m_iconSizes.size()); ++index)
		{
			if (iconSize == m_iconSizes[index])
				sel = index;
		}

		if (sel != CB_ERR && sel != pControl->GetCurSel())
			pControl->SetCurSel(sel);

		ReadDisplayedIconSize();
	}
}

void CAnimationGraphDialog::ReadDisplayedIconSize()
{
	CXTPControlComboBox *pControl = (CXTPControlComboBox*)m_wndToolBar.GetControls()->FindControl(xtpControlComboBox, ID_ANIMATION_GRAPH_ICON_SIZE, TRUE, FALSE);
	if (!pControl || pControl->GetType() != xtpControlComboBox)
		return;

	if (pControl->GetCurSel() != CB_ERR)
		m_displayedIconSize->Set(pControl->GetCurSel());
	else
		m_displayedIconSize->Set(0);
}

void CAnimationGraphDialog::OnDisplayIconsChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMXTPCONTROL* tagNMCONTROL = (NMXTPCONTROL*)pNMHDR;
	CXTPControlButton* pControl = (CXTPControlButton*)tagNMCONTROL->pControl;
	if (pControl->GetType() == xtpControlButton)
	{
		pControl->SetChecked(!pControl->GetChecked());
		EnableIcons(pControl->GetChecked());
	}
	*pResult = 1;
}

void CAnimationGraphDialog::OnUpdateDisplayIconsUI(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(TRUE);

	CXTPControlButton *pControl = (CXTPControlButton*)m_wndToolBar.GetControls()->FindControl(xtpControlButton, ID_ANIMATION_GRAPH_ICONS, TRUE, FALSE);
	if (!pControl || pControl->GetType() != xtpControlButton)
		return;

	pControl->SetChecked(m_enableIcons->GetIVal());
}

void CAnimationGraphDialog::OnDisplayPreviewChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMXTPCONTROL* tagNMCONTROL = (NMXTPCONTROL*)pNMHDR;
	CXTPControlButton* pControl = (CXTPControlButton*)tagNMCONTROL->pControl;
	if (pControl->GetType() == xtpControlButton)
	{
		pControl->SetChecked(!pControl->GetChecked());
		EnablePreview(pControl->GetChecked());
	}
	*pResult = 1;
}

void CAnimationGraphDialog::OnUpdateDisplayPreviewUI(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(TRUE);

	CXTPControlButton *pControl = (CXTPControlButton*)m_wndToolBar.GetControls()->FindControl(xtpControlButton, ID_ANIMATION_GRAPH_PREVIEW, TRUE, FALSE);
	if (!pControl || pControl->GetType() != xtpControlButton)
		return;

	pControl->SetChecked(m_preview->GetIVal());
}

//////////////////////////////////////////////////////////////////////////
LRESULT CAnimationGraphDialog::OnDockingPaneNotify(WPARAM wParam, LPARAM lParam)
{
	if (wParam == XTP_DPN_SHOWWINDOW)
	{
		// get a pointer to the docking pane being shown.
		CXTPDockingPane* pwndDockWindow = (CXTPDockingPane*)lParam;
		if (!pwndDockWindow->IsValid())
		{
			switch (pwndDockWindow->GetID())
			{
			case IDW_ANIMGRAPH_STATES_PANE:
				pwndDockWindow->Attach(&m_stateListCtrl);
				m_stateListCtrl.SetOwner( this );
				break;
			case IDW_ANIMGRAPH_VIEWS_PANE:
				pwndDockWindow->Attach(&m_viewListCtrl);
				m_viewListCtrl.SetOwner( this );
				break;
			case IDW_ANIMGRAPH_INPUTS_PANE:
				pwndDockWindow->Attach(&m_inputListCtrl);
				m_inputListCtrl.SetOwner( this );
				break;
			case IDW_ANIMGRAPH_STATE_EDITOR_PANE:
				pwndDockWindow->Attach(m_stateEditor.GetPanel());
				m_stateEditor.GetPanel()->SetOwner( this );
				break;
			case IDW_ANIMGRAPH_STATE_PARAMS_PANE:
				pwndDockWindow->Attach(&m_stateParamsCtrl);
				m_stateParamsCtrl.SetOwner( this );
				break;
			case IDW_ANIMGRAPH_TEST_PANE:
				pwndDockWindow->Attach(&m_tester);
				m_tester.SetOwner( this );
				break;
			case IDW_ANIMGRAPH_STATE_QUERY_PANE:
				pwndDockWindow->Attach(&m_stateQuery);
				m_stateQuery.SetOwner( this );
				break;
			case IDW_ANIMGRAPH_PREVIEW_PANE:
				pwndDockWindow->Attach(&m_previewDialog);
				m_previewDialog.SetOwner( this );
				break;
			case IDW_ANIMGRAPH_PREVIEW_OPTIONS_PANE:
				pwndDockWindow->Attach(&m_rollupCtrl);
				m_rollupCtrl.SetOwner( this );
				break;
			default:
				return FALSE;
			}
		}
		return TRUE;
	}
	else if (wParam == XTP_DPN_CLOSEPANE)
	{
		// get a pointer to the docking pane being shown.
		CXTPDockingPane* pwndDockWindow = (CXTPDockingPane*)lParam;
		if (pwndDockWindow->IsValid())
		{
			switch (pwndDockWindow->GetID())
			{
			case IDW_ANIMGRAPH_STATES_PANE:
				break;
			}
		}
	}

	return FALSE;
}

void CAnimationGraphDialog::OnAGLinkMappingChanged()
{
	m_stateEditor.StateEvent( eSEE_InitContainer );
}

void CAnimationGraphDialog::OnViewPane( UINT cmdID )
{
	int pane = 0;
	switch ( cmdID )
	{
	case ID_VIEW_STATES:
		pane = IDW_ANIMGRAPH_STATES_PANE;
		break;
	case ID_VIEW_VIEWS:
		pane = IDW_ANIMGRAPH_VIEWS_PANE;
		break;
	case ID_VIEW_INPUTS:
		pane = IDW_ANIMGRAPH_INPUTS_PANE;
		break;
	case ID_VIEW_TESTING:
		pane = IDW_ANIMGRAPH_TEST_PANE;
		break;
	case ID_VIEW_PREVIEW:
		pane = IDW_ANIMGRAPH_PREVIEW_PANE;
		break;
	case ID_VIEW_PREVIEWOPTIONS:
		pane = IDW_ANIMGRAPH_PREVIEW_OPTIONS_PANE;
		break;
	case ID_VIEW_STATEEDITOR:
		pane = IDW_ANIMGRAPH_STATE_EDITOR_PANE;
		break;
	case ID_VIEW_STATEPARAMS:
		pane = IDW_ANIMGRAPH_STATE_PARAMS_PANE;
		break;
	default:
		assert( !"invalid command id" );
		return;
	}
	m_paneManager.ShowPane( pane, TRUE );
}

void CAnimationGraphDialog::OnViewPaneStateQuery()
{
	m_paneManager.ShowPane( IDW_ANIMGRAPH_STATE_QUERY_PANE, TRUE );
}

void CAnimationGraphDialog::OnGraphAddState()
{
	if ( m_pGraph == NULL )
	{
		return;
	}

	CString name = GenerateName(m_pGraph, &CAnimationGraph::FindState, "state");
	m_pGraph->AddState( new CAGState(m_pGraph, name) );
	m_stateListCtrl.ReloadStates();
	m_tester.Reload();
	m_stateQuery.Reload( true );
	m_stateListCtrl.SelectItemByName(name);
}

void CAnimationGraphDialog::OnGraphAddView()
{
	if ( m_pGraph == NULL )
	{
		return;
	}

	CString name = GenerateName(m_pGraph, &CAnimationGraph::FindView, "view");
	m_pGraph->AddView( new CAGView(m_pGraph, name) );
	m_viewListCtrl.ReloadViews();
	m_tester.Reload();
	m_viewListCtrl.SelectItemByName(name);
}

void CAnimationGraphDialog::OnGraphAddInput()
{
	if ( m_pGraph == NULL )
	{
		return;
	}

	CString name = GenerateName(m_pGraph, &CAnimationGraph::FindInput, "input");
	m_pGraph->AddInput( new CAGInput(m_pGraph, name) );
	m_inputListCtrl.ReloadInputs();
	m_tester.Reload();
	m_stateQuery.Reload( true );
	m_inputListCtrl.SelectItemByName(name);
}

void CAnimationGraphDialog::OnGraphCreateAnimation()
{
	AfxMessageBox("Not yet implemented, sorry.");
}

void CAnimationGraphDialog::OnGraphIslandReport()
{
	if (m_pGraph)
	{
		ViewReport( GenerateIslandReport(m_pGraph) );
	}
}

void CAnimationGraphDialog::OnGraphBadNullNodeReport()
{
	if (m_pGraph)
	{
		ViewReport( GenerateNullNodesWithNoForceLeaveReport(m_pGraph) );
	}
}

void CAnimationGraphDialog::OnGraphOrphanNodesReport()
{
	if (m_pGraph)
	{
		ViewReport( GenerateOrphanNodesReport(m_pGraph) );
	}
}

void CAnimationGraphDialog::OnGraphSpeedReport()
{
	CString filename;
	if (CFileUtil::SelectSingleFile( EFILE_TYPE_ANY,filename,CHARACTER_FILE_FILTER ))
	{
		if (ICharacterInstance * pInstance = GetISystem()->GetIAnimationSystem()->CreateInstance(filename))
		{
			ViewReport( GenerateSpeedReport(pInstance->GetIAnimationSet()) );
			pInstance->Release();
		}
		else
			AfxMessageBox("Couldn't create character " + filename);
	}
}

void CAnimationGraphDialog::OnGraphBadCALReport()
{
	if ( m_pGraph == NULL )
	{
		return;
	}

	CString filename;
	if (CFileUtil::SelectSingleFile( EFILE_TYPE_ANY,filename,CHARACTER_FILE_FILTER ))
	{
		if (ICharacterInstance * pInstance = GetISystem()->GetIAnimationSystem()->CreateInstance(filename))
		{
			ViewReport( GenerateBadCALReport(pInstance->GetIAnimationSet(), m_pGraph) );
			pInstance->Release();
		}
		else
			AfxMessageBox("Couldn't create character " + filename);
	}
}

void CAnimationGraphDialog::OnGraphMatchSpeeds()
{
	if ( m_pGraph == NULL )
	{
		return;
	}

	CString filename;
	if (CFileUtil::SelectSingleFile( EFILE_TYPE_ANY,filename,CHARACTER_FILE_FILTER ))
	{
		if (ICharacterInstance * pInstance = GetISystem()->GetIAnimationSystem()->CreateInstance(filename))
		{
			ViewReport( MatchMovementSpeedsToAnimations(pInstance->GetIAnimationSet(), m_pGraph) );
			pInstance->Release();
		}
		else
			AfxMessageBox("Couldn't create character " + filename);
	}
}

void CAnimationGraphDialog::OnGraphDeadInputReport()
{
	if ( m_pGraph == NULL )
	{
		return;
	}

	ViewReport( GenerateDeadInputsReport( m_pGraph ) );
}

void CAnimationGraphDialog::OnGraphTransitionLengthReport()
{
	if ( m_pGraph == NULL )
	{
		return;
	}

	CString out;
	FindLongTransitions( out, m_pGraph, 2 );
	ViewReport( out );
}

void CAnimationGraphDialog::ViewReport( const CString& report )
{
	FILE * f = fopen("animation_graph_report.txt", "wt");
	if (f)
	{
		fwrite( (const char *)report, report.GetLength(), 1, f );
		fclose(f);
		ShellExecute( GetSafeHwnd(), "open", "animation_graph_report.txt", NULL, ".", SW_SHOW );
	}
}

void CAnimationGraphDialog::OnGraphTrial()
{
	CString errorString;

	if (m_pGraph)
	{
		XmlNodeRef graph = m_pGraph->ToXml();
		if (graph)
		{
			CString filename = m_pGraph->GetFilename();
			filename = PathUtil::GetFile( (const char*)filename );
			if (!GetISystem()->GetIAnimationGraphSystem()->TrialAnimationGraph( filename, graph ))
				errorString = "Unable to load animation graph into game engine";
		}
		else
		{
			errorString = "Unable to convert graph to XML";
		}
	}

	if (!errorString.IsEmpty())
		AfxMessageBox( errorString );
}

/*
 * CAnimationGraphListCtrl
 */

BOOL CAnimationGraphListCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	BOOL ret = __super::Create( dwStyle|WS_BORDER|TVS_HASBUTTONS|/*TVS_LINESATROOT|*/TVS_HASLINES|TVS_SHOWSELALWAYS,rect,pParentWnd,nID );

	CMFCUtils::LoadTrueColorImageList( m_imageList,IDB_HYPERGRAPH_COMPONENTS,16,RGB(255,0,255) );
	SetImageList( &m_imageList,TVSIL_NORMAL );
	SetItemHeight(14);

	return ret;
}

inline bool CompareNames( const CAnimationGraphListCtrl::TNameIcon& ni1, const CAnimationGraphListCtrl::TNameIcon& ni2 )
{
	return ni1.first < ni2.first;
}

void CAnimationGraphListCtrl::AddList( const CString& root, std::vector< TNameIcon >& items)
{
	std::sort( items.begin(), items.end(), CompareNames );
	for (std::vector< TNameIcon >::const_iterator iter = items.begin(); iter != items.end(); ++iter )
	{
		InsertItem( iter->first, iter->second, iter->second );
	}
}

void CAnimationGraphListCtrl::ReloadStates()
{
	SetRedraw(FALSE);
	DeleteAllItems();

	if (CAnimationGraphPtr pGraph = m_pParent->GetAnimationGraph())
	{
		std::set< CString > parents;

		std::vector< TNameIcon > items;
		for (CAnimationGraph::state_iterator iter = pGraph->StateBegin(); iter != pGraph->StateEnd(); ++iter)
		{
			CAGState* pState = *iter;
			if ( CAGState* pParent = pState->GetParent() )
				parents.insert( pParent->GetName() );
			int icon = !pState->IncludeInGame() ? 1 : pState->AllowSelect() ? 3 : 2;
			if ( !pState->IsNullState() )
				icon += 3;
			items.push_back( TNameIcon(pState->GetName(), icon) );
		}
		AddList( "States", items );

		// Look at all of the state items and make the parent states bold
		TCHAR szText[1024];
		TVITEM item;
		item.mask = TVIF_TEXT | TVIF_HANDLE | TVIF_STATE;
		item.pszText = szText;
		item.cchTextMax = 1024;
		for ( item.hItem = GetRootItem(); item.hItem; item.hItem = GetNextItem(item.hItem, TVGN_NEXT) )
		{
			if ( GetItem(&item) )
			{
				if ( parents.find(item.pszText) == parents.end() )
					continue;
				item.state = TVIS_BOLD;
				item.stateMask = TVIS_BOLD;
				SetItem(&item);
				item.stateMask = 0;
			}
		}
	}

	SetRedraw(TRUE);
}

void CAnimationGraphListCtrl::ReloadViews()
{
	SetRedraw(FALSE);
	DeleteAllItems();

	if (CAnimationGraphPtr pGraph = m_pParent->GetAnimationGraph())
	{
		std::vector< TNameIcon > items;
		for (CAnimationGraph::view_iterator iter = pGraph->ViewBegin(); iter != pGraph->ViewEnd(); ++iter)
			items.push_back( TNameIcon((*iter)->GetName(),7) );
		AddList( "Views", items );
	}

	SetRedraw(TRUE);
}

void CAnimationGraphListCtrl::ReloadInputs()
{
	SetRedraw(FALSE);
	DeleteAllItems();

	if (CAnimationGraphPtr pGraph = m_pParent->GetAnimationGraph())
	{
		std::vector< TNameIcon > items;
		for (CAnimationGraph::input_iterator iter = pGraph->InputBegin(); iter != pGraph->InputEnd(); ++iter)
		{
			CAGInput* pInput = *iter;
			EAnimationGraphInputType iType = pInput->GetType();
			items.push_back( TNameIcon(pInput->GetName(), iType==eAGIT_Float?8:iType==eAGIT_Integer?9:10) );
		}
		AddList( "Inputs", items );
	}

	SetRedraw(TRUE);
}

void CAnimationGraphListCtrl::SelectItemByName( const CString& state )
{
	for (HTREEITEM item = GetRootItem(); item; item = GetNextSiblingItem(item))
	{
		if (state == GetItemText(item))
		{
			SelectItem( item );
			return;
		}
	}
	this->SelectItem(NULL);
}

//////////////////////////////////////////////////////////////////////////
BOOL CAnimationGraphDialog::PreTranslateMessage(MSG* pMsg)
{
	bool bFramePreTranslate = true;
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
	{
		CWnd* pWnd = CWnd::GetFocus();
		if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CEdit)))
			bFramePreTranslate = false;
	}

	if (bFramePreTranslate)
	{
		// allow tooltip messages to be filtered
		if (__super::PreTranslateMessage(pMsg))
			return TRUE;
	}

	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
	{
		// All keypresses are translated by this frame window

		::TranslateMessage(pMsg);
		::DispatchMessage(pMsg);

		return TRUE;
	}

	return FALSE;
}

void CAnimationGraphDialog::EnableIcons(bool enableIcons)
{
	if (m_enableIcons->GetIVal() != int(enableIcons))
	{
		m_enableIcons->Set(enableIcons);
		if (m_pGraph)
			m_pGraph->ShowIcons(enableIcons);

		// Invalidate the entire view.
		SetActiveView(m_pView);
	}
}

void CAnimationGraphDialog::EnablePreview(bool enablePreview)
{
	if (m_preview->GetIVal() != int(enablePreview))
	{
		m_preview->Set(enablePreview);
		m_previewManager.EnablePreview(enablePreview);
	}
}

void CAnimationGraphDialog::RegisterConsoleVariables()
{
	m_displayedIconSize = gEnv->pConsole->GetCVar("ag_icon_size");
	if (!m_displayedIconSize)
	{
		m_displayedIconSize = gEnv->pConsole->RegisterInt("ag_icon_size",0,0,
			"Size of icons on the animation graph (0-4).\n"
			"Default is 0.\n");
	}

	m_enableIcons = gEnv->pConsole->GetCVar("ag_display_icons");
	if (!m_enableIcons)
	{
		m_enableIcons = gEnv->pConsole->RegisterInt("ag_display_icons",1,0,
			"Flag indicating whether icons are displayed on the animation graph.\n"
			"Default is 1.\n");
	}

	m_preview = gEnv->pConsole->GetCVar("ag_preview");
	if (!m_preview)
	{
		m_preview = gEnv->pConsole->RegisterInt("ag_preview", 1, 0,
			"Flag indicating whether animations are previewed in the preview window of the animation graph.\n"
			"Default is 1.\n");
	}
}
