// ConsoleSCB.cpp: implementation of the CConsoleSCB class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ConsoleSCB.h"
#include "PropertiesDialog.h"

static CPropertiesDialog *gPropertiesDlg = 0;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CConsoleEdit, CEdit)
	ON_WM_GETDLGCODE()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CConsoleSCB, CWnd)
    //{{AFX_MSG_MAP(CConsoleSCB)
    ON_WM_CREATE()
		ON_WM_SIZE()
		ON_WM_DESTROY()
		ON_WM_SETFOCUS()
		ON_EN_SETFOCUS( IDC_EDIT,OnEditSetFocus )
		ON_EN_KILLFOCUS( IDC_EDIT,OnEditKillFocus )
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

static CVarBlock* VarBlockFromConsoleVars()
{
	IConsole *console = GetIEditor()->GetSystem()->GetIConsole();
	std::vector<const char*> cmds;
	cmds.resize( console->GetNumVars() );
	size_t cmdCount = console->GetSortedVars( &cmds[0],cmds.size() );

	CVarBlock *vb = new CVarBlock;
	IVariable* pVariable = 0;
	for (int i = 0; i < cmdCount; i++)
	{
		ICVar *pCVar = console->GetCVar(cmds[i]);
		if (!pCVar)
			continue;
		int varType = pCVar->GetType();

		switch(varType) 
		{
			case CVAR_INT:
				pVariable = new CVariable<int>;
				pVariable->Set( pCVar->GetIVal() );
				break;
			case CVAR_FLOAT:
				pVariable = new CVariable<float>;
				pVariable->Set( pCVar->GetFVal() );
				break;
			case CVAR_STRING:
				pVariable = new CVariable<CString>;
				pVariable->Set( pCVar->GetString() );
				break;
			default: 
				assert(0);
		}

		pVariable->SetDescription(pCVar->GetHelp());
		pVariable->SetName(cmds[i]);

		if (pVariable)
			vb->AddVariable(pVariable);
	}
	return vb;
}

static void OnConsoleVariableUpdated( IVariable *pVar )
{
	if (!pVar)
		return;
	CString varName = pVar->GetName();
	ICVar *pCVar = GetIEditor()->GetSystem()->GetIConsole()->GetCVar(varName);
	if (!pCVar)
		return;
	if (pVar->GetType() == IVariable::INT)
	{
		int val;
		pVar->Get(val);
		pCVar->Set(val);
	}
	else if (pVar->GetType() == IVariable::FLOAT)
	{
		float val;
		pVar->Get(val);
		pCVar->Set(val);
	}
	else if (pVar->GetType() == IVariable::STRING)
	{
		CString val;
		pVar->Get(val);
		pCVar->Set(val);
	}
}

//////////////////////////////////////////////////////////////////////////
static CString popup_helper( HWND hwnd,int x,int y )
{
	IConsole *console = GetIEditor()->GetSystem()->GetIConsole();

	TSmartPtr<CVarBlock> vb = VarBlockFromConsoleVars();
	XmlNodeRef node;
	if (!gPropertiesDlg)
		gPropertiesDlg = new CPropertiesDialog( "Console Variables",node,AfxGetMainWnd(), true );
	if (!gPropertiesDlg->m_hWnd)
	{
		gPropertiesDlg->Create( CPropertiesDialog::IDD,AfxGetMainWnd() );
		gPropertiesDlg->SetUpdateCallback( functor(OnConsoleVariableUpdated) );
	}
	gPropertiesDlg->ShowWindow( SW_SHOW );
	gPropertiesDlg->GetPropertyCtrl()->AddVarBlock( vb );

	/*
	HMENU hm = CreatePopupMenu();
	for (int i = 0; i < cmdCount; i++) {
		AppendMenu( hm,MF_STRING,i+1,cmds[i] );
	}
	int res = TrackPopupMenuEx( hm,TPM_LEFTALIGN|TPM_TOPALIGN|TPM_RETURNCMD,x,y,hwnd,NULL );
	if (res > 0) {
		return cmds[res-1];
	}
	*/
	return "";
}

UINT CConsoleEdit::OnGetDlgCode()
{
	UINT code = CEdit::OnGetDlgCode();
	code |= DLGC_WANTMESSAGE; 
	return code; 
}

BOOL CConsoleEdit::PreTranslateMessage(MSG* msg)
{
	if (msg->message == WM_LBUTTONDBLCLK || msg->message == WM_RBUTTONDOWN)
	{
		CPoint pnt;
		GetCursorPos(&pnt);
		CString str = popup_helper( GetSafeHwnd(),pnt.x,pnt.y );
		if (!str.IsEmpty())
		{
			SetWindowText( str+" " );
		}
	}
	else if (msg->message == WM_CHAR)
	{
		switch (msg->wParam)
		{
		case '~':
		case '`':
			// disable log.
			GetIEditor()->ShowConsole( false );
			SetWindowText( "" );
			break;
		}
	}	else if (msg->message == WM_KEYDOWN)
	{
		IConsole *console = GetIEditor()->GetSystem()->GetIConsole();

		if(msg->wParam!=VK_TAB)
			console->ResetAutoCompletion();

		switch (msg->wParam)
		{
		case VK_RETURN:
			{
				CString str;
				GetWindowText( str );
				// Execute this string as command.
				if (!str.IsEmpty())
				{
					CLogFile::WriteLine( str );
					GetIEditor()->GetSystem()->GetIConsole()->ExecuteString( str );
					m_history.erase( std::remove(m_history.begin(),m_history.end(),str),m_history.end() );
					m_history.push_back(str);
				}
				SetWindowText( "" );
				return 0;
			}
			return TRUE;
			break;

		case VK_TAB:
			{
				GetAsyncKeyState(VK_CONTROL);
				bool bCtrl = GetAsyncKeyState(VK_CONTROL) != 0;
			
				CString inputStr,newStr;
				GetWindowText( inputStr );
				inputStr = inputStr.SpanExcluding( " " );

				if (!bCtrl)
				{
					newStr = console->ProcessCompletion(inputStr);
					newStr = console->AutoComplete( inputStr );
				}
				else
				{
					newStr = console->AutoCompletePrev( inputStr );
				}

				if (!newStr.IsEmpty()) 
				{
					newStr += " ";
					SetWindowText( newStr );
				}
				CString str;
				GetWindowText( str );
				SetSel( str.GetLength(),str.GetLength() );
				return TRUE;
			}
			return TRUE;
			break;

		case VK_UP:
		case VK_DOWN:
			{
				CString str;
				CMenu menu;
				menu.CreatePopupMenu();
				for (int i = 0; i < m_history.size(); i++) {
					menu.AppendMenu( MF_STRING,i+1,m_history[i] );
				}
				CPoint pnt;
				GetCursorPos(&pnt);
				int res = ::TrackPopupMenuEx( menu.GetSafeHmenu(),TPM_LEFTALIGN|TPM_TOPALIGN|TPM_RETURNCMD,pnt.x,pnt.y,GetSafeHwnd(),NULL );
				if (res > 0) {
					str = m_history[res-1];
				}
				if (!str.IsEmpty())
				{
					SetWindowText( str );
					SetSel( str.GetLength(),str.GetLength() );
				}
			}
			return TRUE;

		case VK_ESCAPE:
			// disable log.
			GetIEditor()->ShowConsole( false );
			return TRUE;
			break;
		}
	}

	return CEdit::PreTranslateMessage(msg);
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CConsoleSCB::CConsoleSCB()
{
}

CConsoleSCB::~CConsoleSCB()
{
	if (gPropertiesDlg)
		delete gPropertiesDlg;
	gPropertiesDlg = 0;
}

int CConsoleSCB::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	////////////////////////////////////////////////////////////////////////
	// Create the edit control and register it
	////////////////////////////////////////////////////////////////////////

	RECT rcEdit;
	CFont cListBoxFont;

	if (CWnd::OnCreate(lpCreateStruct) == -1)
        return -1;

	/*
	// Create the edit control
	VERIFY(m_cListBox.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | 
		LBS_NOSEL, rcEdit, this, NULL));
	m_cListBox.ShowWindow( SW_HIDE );
	*/
	
	m_dialog.Create( CConsoleSCB_InternalDialog::IDD,this );
	m_dialog.ShowWindow(SW_SHOW);

	m_edit.Create( WS_CHILD|WS_VISIBLE|ES_MULTILINE|WS_VSCROLL|ES_AUTOVSCROLL|ES_READONLY, rcEdit, &m_dialog, NULL );
	m_edit.SetFont( CFont::FromHandle( (HFONT)gSettings.gui.hSystemFont) );
	m_edit.ModifyStyleEx( 0,WS_EX_STATICEDGE );
	m_edit.SetLimitText(0);
	

	m_input.Create( WS_CHILD|WS_VISIBLE|ES_WANTRETURN|ES_AUTOHSCROLL|WS_TABSTOP, rcEdit, &m_dialog, IDC_EDIT );
	m_input.SetFont( CFont::FromHandle( (HFONT)::GetStockObject(SYSTEM_FONT)) );
	m_input.ModifyStyleEx( 0,WS_EX_STATICEDGE );
	m_input.SetWindowText( "" );

/*
	// Change
	cListBoxFont.Attach(gSettings.gui.hSystemFont);
	m_cListBox.SetFont(&cListBoxFont);
	cListBoxFont.Detach();
	*/

	// Attach / register the listbox control
	//CLogFile::AttachListBox(m_cListBox.m_hWnd);
	CLogFile::AttachEditBox(m_edit.GetSafeHwnd());

	return 0;
}

void CConsoleSCB::OnSize(UINT nType, int cx, int cy)
{
	////////////////////////////////////////////////////////////////////////
	// Resize the edit control
	////////////////////////////////////////////////////////////////////////

	RECT rcEdit;

	CWnd::OnSize(nType, cx, cy);

	// Get the size of the client window
	GetClientRect(&rcEdit);
/*
	// Set the position of the listbox
	m_cListBox.SetWindowPos(NULL, rcEdit.left + 3, rcEdit.top + 3, rcEdit.right - 6, 
		rcEdit.bottom - 6, SWP_NOZORDER);
		*/
	int inputH = 18;

	m_dialog.MoveWindow( rcEdit.left,rcEdit.top,rcEdit.right,rcEdit.bottom );

	m_edit.SetWindowPos(NULL, rcEdit.left, rcEdit.top + 2, rcEdit.right, rcEdit.bottom - 1 - inputH, SWP_NOZORDER);

	m_input.SetWindowPos(NULL, rcEdit.left, rcEdit.bottom-inputH, rcEdit.right, rcEdit.bottom - 2, SWP_NOZORDER);
	m_input.SetFocus();
}

void CConsoleSCB::OnDestroy()
{
	////////////////////////////////////////////////////////////////////////
	// Unregister the edit control
	////////////////////////////////////////////////////////////////////////
	CLogFile::AttachEditBox(NULL);

	CLogFile::WriteLine("Console control bar destroied");
}

void CConsoleSCB::OnSetFocus( CWnd* pOldWnd )
{
	m_input.SetFocus();
}

BOOL CConsoleSCB::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	return TRUE;
}

void CConsoleSCB::OnEditSetFocus()
{
	// Disable accelerators when Edit gets focus.
	GetIEditor()->EnableAcceleratos( false );
}
	
void CConsoleSCB::OnEditKillFocus()
{
	// Enable accelerators when Edit loose focus.
	GetIEditor()->EnableAcceleratos( true );
}

void CConsoleSCB::SetInputFocus()
{
	m_input.SetFocus();
	m_input.SetWindowText( "" );
}
