// CharacterEditor.cpp : implementation file
//

#include "stdafx.h"

#include <I3DEngine.h>
#include "ICryAnimation.h"
#include <IRenderer.h>
#include "IRenderAuxGeom.h"
#include <Cderr.h>
//#include "IRendererD3D9.h"

#include "CryEdit.h"
#include "CharacterEditor.h"
#include "PropertiesPanel.h"
#include "ThumbnailGenerator.h"
#include "FileTypeUtils.h"

#include "IViewPane.h"
#include "ModelViewport.h"
#include "ModelViewportCE.h"
#include "StringUtils.h"
#include "CharPanel_Preset.h"


class CCharacterEditorViewClass : public TRefCountBase<IViewPaneClass>
{
	//////////////////////////////////////////////////////////////////////////
	// IClassDesc
	//////////////////////////////////////////////////////////////////////////
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_VIEWPANE; };
	virtual REFGUID ClassID()
	{
		// {D21C9FE5-22D3-41e3-B84B-A377AFA0A05C}
		static const GUID guid =	{ 0xd21c9fe5, 0x22d3, 0x41e3, { 0xb8, 0x4b, 0xa3, 0x77, 0xaf, 0xa0, 0xa0, 0x5c } };
		return guid;
	}
	virtual const char* ClassName() { return "Character Editor"; };
	virtual const char* Category() { return "Character Editor"; };
	//////////////////////////////////////////////////////////////////////////
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CCharacterEditor); };
	virtual const char* GetPaneTitle() { return _T("Character Editor"); };
	virtual EDockingDirection GetDockingDirection() { return DOCK_FLOAT; };
	virtual CRect GetPaneRect() { return CRect(200,200,800,700); };
	virtual bool SinglePane() { return false; };
	virtual bool WantIdleUpdate() { return true; };
};

//////////////////////////////////////////////////////////////////////////
void CCharacterEditor::RegisterViewClass()
{
	GetIEditor()->GetClassFactory()->RegisterClass( new CCharacterEditorViewClass );
}

#define TAB_ATTACHMENTS 0


IMPLEMENT_DYNCREATE(CCharacterEditor, CToolbarDialog)



//////////////////////////////////////////////////////////////////////////
CCharacterEditor::~CCharacterEditor()
{
	GetIEditor()->UnregisterNotifyListener(this);
	if (m_rollupCtrl.m_hWnd)
		m_rollupCtrl.DestroyWindow();
	delete m_pResizePane;
	//if(m_pCharPanel_Preset)
		//delete m_pCharPanel_Preset;
	m_pCharPanel_Preset = 0;
}




void CCharacterEditor::PostNcDestroy()
{
	delete this;
}

void CCharacterEditor::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
	DDX_Control(pDX,IDC_TAB,m_TabSelect);
	DDX_Control(pDX,IDC_PANE1,m_pane1);
	DDX_Control(pDX,IDC_PANE2,m_pane2);
}


BEGIN_MESSAGE_MAP(CCharacterEditor, CToolbarDialog)
	ON_COMMAND(ID_FILE_NEW,  OnFileNew)
	ON_COMMAND(ID_FILE_LOAD, OnFileLoad)
	ON_COMMAND(ID_FILE_SAVE, OnFileSave)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE, OnUpdateUIFileSave)
	ON_COMMAND(ID_FILE_SAVE_AS, OnFileSaveAs)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_AS, OnUpdateUIFileSaveAs)

	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
	ON_BN_CLICKED(IDC_BUTTON2, OnBnClickedExit)
	ON_BN_CLICKED(IDC_BUTTON1, OnBnClickedBrowseFile)
	ON_EN_CHANGE(IDC_EDIT1, OnEnChangeEdit1)
	ON_BN_CLICKED(IDC_BUTTON3, OnBnClickedButton3)
	ON_NOTIFY(TCN_SELCHANGE,IDC_TAB,OnTabSelChanged)

	ON_COMMAND(ID_CHAREDIT_MOVE, OnMoveMode)
	ON_UPDATE_COMMAND_UI(ID_CHAREDIT_MOVE, OnMoveModeUpdateUI)

	ON_COMMAND(ID_CHAREDIT_ROTATE, OnRotateMode)
	ON_UPDATE_COMMAND_UI(ID_CHAREDIT_ROTATE, OnRotateModeUpdateUI)

END_MESSAGE_MAP()



//////////////////////////////////////////////////////////////////////////



BOOL CCharacterEditor::OnInitDialog()
{
	__super::OnInitDialog();
	CRect rc;
	GetClientRect(rc);

	//m_wndMenuBar.CreateToolBar(WS_VISIBLE|WS_CHILD|CBRS_TOOLTIPS|CBRS_GRIPPER, this, AFX_IDW_TOOLBAR + 10 );
	//m_wndMenuBar.LoadMenuBar(IDD_CHARACTER_EDITOR);
	//m_wndMenuBar.SetFlags(xtpFlagStretched);
	
	//SetMenuBar(&m_wndMenuBar);
	VERIFY(InitCommandBars());
	CXTPCommandBars* pCommandBars = GetCommandBars();
	m_pWndMenuBar = pCommandBars->SetMenu(_T("Menu Bar"), IDD_CHARACTER_EDITOR);

	// Create the toolbar
	m_toolbar.Create( this,WS_CHILD|WS_VISIBLE|CBRS_TOP|CBRS_TOOLTIPS|CBRS_FLYBY );
	VERIFY( m_toolbar.LoadToolBar(IDR_CHARACTER_EDITOR) );

	m_pModelViewportCE = new CModelViewportCE;
	m_pModelViewportCE->SetType( ET_ViewportModel );
	m_pModelViewportCE->SetParent(&m_pane1);
	//m_pModelViewportCE->Create( &m_pane1,ET_ViewportModel,"Model Preview" );
	m_pModelViewportCE->MoveWindow(rc);
	m_pModelViewportCE->ShowWindow(SW_SHOW);

	m_rollupCtrl.Create( WS_VISIBLE|WS_CHILD,CRect(4, 4, 187, 362),&m_pane2, NULL);

	int idx;

	//add page for "Character Animation"
	CharPanel_Animation* pAnimationPanel = new CharPanel_Animation(m_pModelViewportCE,NULL);
	m_pModelViewportCE->SetModelPanelA(pAnimationPanel);
	idx=m_rollupCtrl.InsertPage( "Character Animation",pAnimationPanel );
	m_rollupCtrl.ExpandPage(idx,1);

	//add page for Preset
	m_pCharPanel_Preset = new CharPanel_Preset(m_pModelViewportCE,NULL);
	m_pModelViewportCE->SetCharPanelPreset(m_pCharPanel_Preset);
	idx=m_rollupCtrl.InsertPage( "Preset", m_pCharPanel_Preset);
	m_rollupCtrl.ExpandPage(idx,0);


	//add page for "Bone Attachment"
	/*	m_pModelViewportCE->SetModelPanelW( new CharPanel_BAttach(m_pModelViewportCE,NULL) );
	idx=m_rollupCtrl.InsertPage( "Bone Attachment",m_pModelViewportCE->GetModelPanelW()  );
	m_rollupCtrl.ExpandPage(idx,0);

	//add page for "Face Attachment"
	m_pModelViewportCE->SetModelPanelF( new CharPanel_FAttach(m_pModelViewportCE,NULL) );
	idx=m_rollupCtrl.InsertPage( "Face Attachment",m_pModelViewportCE->GetModelPanelF()  );
	m_rollupCtrl.ExpandPage(idx,0);*/

	//add page for "Debug Options"
	s_varsPanel = new CPropertiesPanel(this);
	s_varsPanel->AddVars( m_pModelViewportCE->GetVarObject()->GetVarBlock() );
	idx=m_rollupCtrl.InsertPage( "Debug Options",s_varsPanel);
	m_rollupCtrl.ExpandPage(idx,0);

	// Make tab panels.
	m_AttachmentsDlg.Create(CAttachmentsDlg::IDD,&m_TabSelect);
	m_AttachmentsDlg.m_ButtonCLEAR.EnableWindow(FALSE);
	m_AttachmentsDlg.m_ButtonRENAME.EnableWindow(FALSE);
	m_AttachmentsDlg.m_ButtonREMOVE.EnableWindow(FALSE);
	m_AttachmentsDlg.m_ButtonEXPORT.EnableWindow(FALSE);
	m_AttachmentsDlg.m_pModelViewportCE = m_pModelViewportCE;

	m_AnimationControlDlg.Create(CAnimationControlDlg::IDD, &m_TabSelect);
	m_AnimationControlDlg.m_pModelViewportCE = m_pModelViewportCE;

	m_CharacterPropertiesDlg.Create(CCharacterPropertiesDlg::IDD, &m_TabSelect);
	m_CharacterPropertiesDlg.SetCharacterChangeListener(this);
	m_CharacterPropertiesDlg.m_pModelViewportCE = m_pModelViewportCE;

	m_MorphingDlg.Create(CMorphingDlg::IDD,&m_TabSelect);
	m_MorphingDlg.m_button[0].EnableWindow(FALSE);
	m_MorphingDlg.m_button[1].EnableWindow(FALSE);
	m_MorphingDlg.m_button[2].EnableWindow(FALSE);
	m_MorphingDlg.m_button[3].EnableWindow(FALSE);
	m_MorphingDlg.m_button[4].EnableWindow(FALSE);
	m_MorphingDlg.m_button[5].EnableWindow(FALSE);
	m_MorphingDlg.m_button[6].EnableWindow(FALSE);
	m_MorphingDlg.m_button[7].EnableWindow(FALSE);

	m_MorphingDlg.m_button[0].SetCheck(FALSE);
	m_MorphingDlg.m_button[1].SetCheck(FALSE);
	m_MorphingDlg.m_button[2].SetCheck(FALSE);
	m_MorphingDlg.m_button[3].SetCheck(FALSE);
	m_MorphingDlg.m_button[4].SetCheck(FALSE);
	m_MorphingDlg.m_button[5].SetCheck(FALSE);
	m_MorphingDlg.m_button[6].SetCheck(FALSE);
	m_MorphingDlg.m_button[7].SetCheck(FALSE);

	m_MorphingDlg.m_button[0].SetBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[0].SetPushedBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[1].SetBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[1].SetPushedBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[2].SetBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[2].SetPushedBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[3].SetBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[3].SetPushedBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[4].SetBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[4].SetPushedBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[5].SetBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[5].SetPushedBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[6].SetBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[6].SetPushedBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[7].SetBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[7].SetPushedBkColor(RGB(0xcf,0xcf,0xcf));


	m_MorphingDlg.m_pModelViewportCE=m_pModelViewportCE;

	m_TabSelect.AddControl(_T("Character"), &m_CharacterPropertiesDlg);
	m_TabSelect.AddControl( _T("Shape Deformation"),&m_MorphingDlg );
	m_TabSelect.AddControl(_T("Animation Control"), &m_AnimationControlDlg);
	m_TabSelect.AddControl( _T("Attachments"),&m_AttachmentsDlg );
	//m_TabSelect.AddControl( _T("Move Speeds"),&m_AttachmentsDlg );
	m_TabSelect.SetCurSel(3);
	//m_TabSelect.EnableWindow(FALSE);

	// Set autoresize parameters
	SetResize( IDC_TAB,CXTResizeRect(0,1,1,1) );
	SetResize( IDC_PANE2,CXTResizeRect(1,0,1,1) );
	SetResize( IDC_PANE1,SZ_RESIZE(1) );

	RecalcLayout();
	m_pModelViewportCE->m_pAttachmentsDlg = &m_AttachmentsDlg;
	m_pModelViewportCE->m_pMorphingDlg = &m_MorphingDlg;
	m_pModelViewportCE->m_pTabSelect	= &m_TabSelect;
	m_pModelViewportCE->m_pAnimationControlDlg = &m_AnimationControlDlg;
	m_pModelViewportCE->m_pCharacterPropertiesDlg = &m_CharacterPropertiesDlg;
	
	//LoadCharacter( "objects/box.cgf",1 );
	//LoadCharacter( "objects\\characters\\mercenaries\\merc_scout\\merc_scout.chr" );
//	LoadCharacter( "objects\\characters\\human\\squad\\base_uniform\\squad_baseuniform_ryan_miller.cdf" );
//LoadCharacter( "objects\\characters\\human\\asian\\Lifleman_light\\Lifleman_light.cdf" );


//	m_pModelViewportCE->m_FixedCamInViewport=0;
//	pCvar = gEnv->pConsole->GetCVar("ca_FixedCamInViewport");
//	if(pCvar && pCvar->GetIVal()==1)
//		m_pModelViewportCE->m_FixedCamInViewport=1;
//	GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ca_DrawBodyMoveDir")->Set( 1 );

	const char* pLoadName=GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ca_CharEditModel")->GetString( );
	LoadCharacter(pLoadName);


	m_AnimationControlDlg.ModelChanged();

	return TRUE;
}



BOOL CCharacterEditor::PreTranslateMessage(MSG* pMsg)
{
	//if( GetFocus() == GetDlgItem( IDC_PANE1 ) )
	{
		if( WM_KEYDOWN == pMsg->message || WM_KEYUP == pMsg->message )
		{
			int mes = pMsg->message;
			int key = pMsg->wParam;
			switch( pMsg->message )
			{
			case WM_KEYDOWN:
				{
					m_pModelViewportCE->m_bKey[pMsg->wParam]=1;
					break;
				}
			case WM_KEYUP:
				{
					m_pModelViewportCE->m_bKey[pMsg->wParam]=0;
					break;
				}
			default:
				{
					break;
				}
			}
			return TRUE;
		}
	}

	return CDialog::PreTranslateMessage( pMsg );
}


//////////////////////////////////////////////////////////////////////////
void CCharacterEditor::OnTabSelChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	int nCurSel = m_TabSelect.GetCurSel();
	switch (nCurSel)
	{
	case TAB_ATTACHMENTS:
		{
			m_AttachmentsDlg.ShowWindow(SW_SHOW);
			break;
		}
	default:
		m_AttachmentsDlg.ShowWindow(SW_HIDE);
	}
	*pResult = 0;
}




void CCharacterEditor::OnEnChangeEdit1()
{

}




void CCharacterEditor::OnBnClickedOk() { /*	OnOK();*/ };


void CCharacterEditor::OnFileNew() {
	m_pModelViewportCE->ReleaseObject();
	m_strCharDefPathName="NoPathDefined";

	m_AttachmentsDlg.CharacterChanged=0;
	m_AttachmentsDlg.ReloadAttachment();
	m_AttachmentsDlg.ClearBones();

	m_MorphingDlg.m_DeformationSlider.EnableWindow(FALSE);
	m_MorphingDlg.m_DeformationNumber.EnableWindow(FALSE);

	m_MorphingDlg.m_UniformScalingSlider.EnableWindow(TRUE);
	m_MorphingDlg.m_UniformScalingNumber.EnableWindow(TRUE);

	m_MorphingDlg.m_button[0].EnableWindow(FALSE);
	m_MorphingDlg.m_button[1].EnableWindow(FALSE);
	m_MorphingDlg.m_button[2].EnableWindow(FALSE);
	m_MorphingDlg.m_button[3].EnableWindow(FALSE);
	m_MorphingDlg.m_button[4].EnableWindow(FALSE);
	m_MorphingDlg.m_button[5].EnableWindow(FALSE);
	m_MorphingDlg.m_button[6].EnableWindow(FALSE);
	m_MorphingDlg.m_button[7].EnableWindow(FALSE);

	m_MorphingDlg.m_button[0].SetCheck(FALSE);
	m_MorphingDlg.m_button[1].SetCheck(FALSE);
	m_MorphingDlg.m_button[2].SetCheck(FALSE);
	m_MorphingDlg.m_button[3].SetCheck(FALSE);
	m_MorphingDlg.m_button[4].SetCheck(FALSE);
	m_MorphingDlg.m_button[5].SetCheck(FALSE);
	m_MorphingDlg.m_button[6].SetCheck(FALSE);
	m_MorphingDlg.m_button[7].SetCheck(FALSE);

	m_MorphingDlg.m_button[0].SetBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[0].SetPushedBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[1].SetBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[1].SetPushedBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[2].SetBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[2].SetPushedBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[3].SetBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[3].SetPushedBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[4].SetBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[4].SetPushedBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[5].SetBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[5].SetPushedBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[6].SetBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[6].SetPushedBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[7].SetBkColor(RGB(0xcf,0xcf,0xcf));
	m_MorphingDlg.m_button[7].SetPushedBkColor(RGB(0xcf,0xcf,0xcf));

	m_MorphingDlg.m_nColor=9;

}

void CCharacterEditor::OnFileLoad()
{
/*	char szFilters[] = "Character Definition Files|*.cdf; | Character Definition Files (*.cdf)|*.cdf | All files (*.*)|*.*| |";
	CFileDialog dlg(TRUE, NULL, NULL, OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR, szFilters);

	if (dlg.DoModal() == IDOK) 
	{
		char ext[_MAX_EXT];
		_splitpath( dlg.GetPathName(),NULL,NULL,NULL,ext );
		if (stricmp(ext,".cdf") == 0)
		{
			CLogFile::WriteLine("Importing Character Definitions...");
			CharDefPathName = dlg.GetPathName();
			m_pModelViewportCE->LoadCharDef( CharDefPathName );
		}

		BeginWaitCursor();
	}*/

	CString file;
	//SetCurrentDirectory(GetISystem()-)
	if (CFileUtil::SelectSingleFile( EFILE_TYPE_GEOMETRY,file ))
	{
		// Select the default save location.
		string sExtension = PathUtil::GetExt(file.GetString());
		sExtension = CryStringUtils::toLower(sExtension);
		m_strCharDefPathName = file;
		LoadCharacter( file );
		m_AnimationControlDlg.ModelChanged();
	}

	//-------------------------------------------------------------------------------
	m_AttachmentsDlg.CharacterChanged=0;
	m_AttachmentsDlg.UpdateList();
}

void CCharacterEditor::OnFileSave()
{
	CString path =  Path::GamePathToFullPath( (const char*)m_strCharDefPathName );
	if (path.GetLength() == 0 || CryGetFileAttributes( path )== INVALID_FILE_ATTRIBUTES)
	{
		// Since there is no filename associated with the character, allow the user to choose one.
		OnFileSaveAs();
	}
	else
	{
		CLogFile::WriteLine("Exporting Character Definitions...");
		if (m_pModelViewportCE->GetAnimationSystem()->SaveCharacterDefinition( m_pModelViewportCE->GetCharacterBase(), m_strCharDefPathName ))
			m_AttachmentsDlg.CharacterChanged=0;
	}
}
void CCharacterEditor::OnUpdateUIFileSave( CCmdUI* pCmdUI )
{
	pCmdUI->Enable( m_AttachmentsDlg.CharacterChanged );
}


void CCharacterEditor::OnFileSaveAs()
{
	char szFilters[] = "Character Definition Files (*.cdf)|*.cdf| ";
	CString path = Path::GetPath( Path::GamePathToFullPath( (const char*)m_strCharDefPathName ) );
	CString file = PathUtil::GetFileName(m_strCharDefPathName);

	uint32 test = CFileUtil::SelectSaveFile( szFilters,"cdf", path,file );

	if (test) 
	{
		CLogFile::WriteLine("Exporting Character Definitions...");
		m_pModelViewportCE->GetAnimationSystem()->SaveCharacterDefinition( m_pModelViewportCE->GetCharacterBase(), file );
		m_AttachmentsDlg.CharacterChanged=0;
	}

}

void CCharacterEditor::OnUpdateUIFileSaveAs( CCmdUI* pCmdUI )
{
	// [MichaelS 21/12/2005] We should be able to save to a different file, even if the file has not been changed.
	//pCmdUI->Enable( m_AttachmentsDlg.CharacterChanged );
	pCmdUI->Enable( TRUE );
}

void CCharacterEditor::OnBnClickedExit()
{
	// TODO: Add your control notification handler code here
	OnOK();
}

void CCharacterEditor::OnBnClickedBrowseFile()
{
	// TODO: Add your control notification handler code here
	CString file;
	bool t=CFileUtil::SelectSingleFile( EFILE_TYPE_GEOMETRY,file );
	if (t)
	{
		LoadCharacter( file );
		m_fileEdit.SetWindowText( file );

		m_AnimationControlDlg.ModelChanged();
	}
}



void CCharacterEditor::OnBnClickedButton3()
{
	// TODO: Add your control notification handler code here
	CString filename;
	m_fileEdit.GetWindowText( filename );
	LoadCharacter( filename );
}

//////////////////////////////////////////////////////////////////////////
void CCharacterEditor::OnDestroy()
{
	this->m_AnimationControlDlg.Close();

	__super::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////
void CCharacterEditor::OnSize(UINT nType, int cx, int cy)
{
	if (m_pane1.m_hWnd && m_pWndMenuBar)
	{
		CRect rc,rcorg;
		GetClientRect(rc);
		rcorg = rc;

		int p2w = 260;

		CRect menuRc = rc;
		menuRc.bottom = 24;
		m_pWndMenuBar->MoveWindow(menuRc);

		rc.top += menuRc.bottom;
		rc.DeflateRect(4,0,4,4);

		CRect toolRc = rc;
		toolRc.bottom = toolRc.top + 32;
		toolRc.right -= p2w;
		m_toolbar.MoveWindow(toolRc);

		CRect pane2rc = rc;
		pane2rc.left = pane2rc.right - p2w;
		m_pane2.MoveWindow(pane2rc);

		CRect pane1rc = rc;
		pane1rc.top = toolRc.bottom + 4;
		pane1rc.right = pane2rc.left-4;
		pane1rc.bottom = rc.bottom - 210;
		m_pane1.MoveWindow(pane1rc);

		CRect tabRc = rc;
		tabRc.top = pane1rc.bottom + 4;
		tabRc.right = pane2rc.left - 4;
		m_TabSelect.MoveWindow(tabRc);

		m_pane1.GetClientRect(rc);
		m_pModelViewportCE->MoveWindow(rc);

		m_pane2.GetClientRect(rc);
		m_rollupCtrl.MoveWindow(rc);
	}
}

//////////////////////////////////////////////////////////////////////////
void CCharacterEditor::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
	switch (event)
	{
	case eNotify_OnIdleUpdate:
		//DoIdleUpdate();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CCharacterEditor::DoIdleUpdate()
{
	m_pModelViewportCE->Update();
}


//-------------------------------------------------------------------


void CCharacterEditor::OnMoveMode( ) {

	m_pModelViewportCE->m_Button_MOVE+=1;
	m_pModelViewportCE->m_Button_MOVE&=1;
	if (m_pModelViewportCE->m_Button_MOVE) m_pModelViewportCE->m_Button_ROTATE=0;

	ICharacterInstance* pCharacter=m_pModelViewportCE->GetCharacterBase();
	if (pCharacter) {
		if (m_pModelViewportCE->m_Button_MOVE) 
			ResetCharEditor(pCharacter);

		pCharacter->SetResetMode(m_pModelViewportCE->m_Button_MOVE);
	}
}
void CCharacterEditor::OnMoveModeUpdateUI( CCmdUI *pCmdUI ) 
{
	pCmdUI->SetCheck(m_pModelViewportCE->m_Button_MOVE);
}

//-------------------------------------------------------------------

void CCharacterEditor::OnRotateMode() 
{
	m_pModelViewportCE->m_Button_ROTATE+=1;
	m_pModelViewportCE->m_Button_ROTATE&=1;
	if (m_pModelViewportCE->m_Button_ROTATE) m_pModelViewportCE->m_Button_MOVE=0;

	ICharacterInstance* pCharacter=m_pModelViewportCE->GetCharacterBase();
	if (pCharacter) 
	{
		if (m_pModelViewportCE->m_Button_ROTATE) 
			ResetCharEditor(pCharacter);
		pCharacter->SetResetMode(m_pModelViewportCE->m_Button_ROTATE);
	}
}

void CCharacterEditor::OnRotateModeUpdateUI( CCmdUI *pCmdUI ) 
{
	pCmdUI->SetCheck(m_pModelViewportCE->m_Button_ROTATE);
}


void CCharacterEditor::ResetCharEditor(ICharacterInstance* pCharacter) 
{
	CharPanel_Animation* pCharPanel_Animation=m_pModelViewportCE->m_pCharPanel_Animation;
	pCharPanel_Animation->SetBlendSpaceSliderX(0.0f);
	pCharPanel_Animation->SetBlendSpaceSliderY(0.0f);
	pCharPanel_Animation->SetBlendSpaceSliderZ(0.0f);
	pCharPanel_Animation->SetMotionInterpolationSlider(0.5f);

//	if ( m_pModelViewportCE->GetPlayerControl() )
	{
		m_pModelViewportCE->SetPlayerPos(Vec3(ZERO));
		m_pModelViewportCE->m_GridOrigin=Vec3(ZERO);
		m_pModelViewportCE->m_pCharPanel_Animation->m_PlayControl.SetCheck(0);
		m_pModelViewportCE->m_pCharPanel_Animation->m_FixedCamera.SetCheck(0);
		m_pModelViewportCE->m_pCharPanel_Animation->m_PathFollowing.SetCheck(0);
		m_pModelViewportCE->m_pCharPanel_Animation->m_AttachedCamera.SetCheck(0);
		m_pModelViewportCE->m_pCharPanel_Animation->m_Idle2Move.SetCheck(0);
		m_pModelViewportCE->m_pCharPanel_Animation->m_IdleStep.SetCheck(0);
	}

	if ( m_pModelViewportCE->GetPlayerControl() )
	{
		m_pModelViewportCE->SetPlayerPos(Vec3(ZERO));
		m_pModelViewportCE->m_pCharPanel_Animation->m_PlayControl.SetCheck(0);
		m_pModelViewportCE->m_pCharPanel_Animation->m_FixedCamera.SetCheck(0);
		m_pModelViewportCE->m_pCharPanel_Animation->m_PathFollowing.SetCheck(0);
		m_pModelViewportCE->m_pCharPanel_Animation->m_AttachedCamera.SetCheck(0);
		m_pModelViewportCE->m_pCharPanel_Animation->m_Idle2Move.SetCheck(0);
		m_pModelViewportCE->m_pCharPanel_Animation->m_IdleStep.SetCheck(0);
		m_pModelViewportCE->SetPlayerControl(0);
		Vec3 VPos=Vec3(0,-3,2);
		Vec3 VTar=Vec3(0,0,1);
		Matrix33 orient; orient.SetRotationVDir( (VTar-VPos).GetNormalized() );
		m_pModelViewportCE->m_Camera.SetMatrix( Matrix34(orient,Vec3(0,-3,2))  );
	}


	//pCharacter->StopAllAnimations();
	pCharacter->GetISkeletonPose()->SetDefaultPose();
	IAttachmentManager* pIAttachmentManager = pCharacter->GetIAttachmentManager();
	uint32 numAttachmnets = pIAttachmentManager->GetAttachmentCount();
	for (uint32 i=0; i<numAttachmnets; i++) 
	{
		IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByIndex(i);
		IAttachmentObject* pIAttachmentObject = pIAttachment->GetIAttachmentObject();
		if (pIAttachmentObject) 
		{
			ICharacterInstance* pICharInstance = pIAttachmentObject->GetICharacterInstance();
			if (pICharInstance)
			{
				pCharacter->GetISkeletonPose()->SetDefaultPose();
			}
		}
	}

}


void CCharacterEditor::OnCharacterChanged()
{
	this->m_AttachmentsDlg.CharacterChanged = 1;
}

void CCharacterEditor::LoadCharacter(const CString& file, float scale)
{
	if (m_pModelViewportCE)
		m_pModelViewportCE->LoadObject(file, scale);

}
