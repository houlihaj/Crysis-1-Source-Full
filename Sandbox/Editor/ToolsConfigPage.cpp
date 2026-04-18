////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   toolsconfigpage.cpp
//  Version:     v1.00
//  Created:     27/11/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ToolsConfigPage.h"
#include "ExternalTools.h"

// CToolsConfigPage dialog

IMPLEMENT_DYNAMIC(CToolsConfigPage, CXTResizePropertyPage)
CToolsConfigPage::CToolsConfigPage()
	: CXTResizePropertyPage(CToolsConfigPage::IDD)
{

		// Call CXTEditListBox::SetListEditStyle to set the type of edit list. You can
	// pass in LBS_XT_NOTOOLBAR if you don't want the toolbar displayed.
	m_editList.SetListEditStyle(_T(" &User Commands:"),	LBS_XT_DEFAULT);
}

CToolsConfigPage::~CToolsConfigPage()
{
	// Free tools.
	for (int i = 0; i < m_tools.size(); i++)
	{
		delete m_tools[i];
	}
	m_tools.clear();
}

void CToolsConfigPage::DoDataExchange(CDataExchange* pDX)
{
	CXTResizePropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO1, m_consoleCmd);
	DDX_Control(pDX, IDC_TOGGLEVARONOFF, m_toggleVar);
	DDX_Control(pDX, IDC_EDIT_LIST, m_editList);
	DDX_Control(pDX, IDC_EDITOR_COMMAND, m_editorCmd);
}

BEGIN_MESSAGE_MAP(CToolsConfigPage, CXTResizePropertyPage)
	ON_BN_CLICKED( IDC_TOGGLEVARONOFF,OnToggleVar )
	
	ON_CBN_SELENDOK( IDC_EDITOR_COMMAND,OnSelEditorCmd )
	ON_CBN_EDITUPDATE( IDC_EDITOR_COMMAND,OnChangeEditorCmd )
	ON_CBN_SELENDOK( IDC_COMBO1,OnSelConsoleCmd )
	ON_CBN_EDITUPDATE( IDC_COMBO1,OnChangeConsoleCmd )

	ON_LBN_SELCHANGE(IDC_EDIT_LIST, OnSelchangeEditList)
	ON_LBN_XT_NEWITEM(IDC_EDIT_LIST, OnNewItem)
END_MESSAGE_MAP()


// CToolsConfigPage message handlers
BOOL CToolsConfigPage::OnInitDialog() 
{
	CXTResizePropertyPage::OnInitDialog();
	
	m_editList.Initialize();
	m_editList.SetCurSel(0);
//	OnSelchangeEditList();

	// Set control resizing.
	SetResize(IDC_EDIT_LIST, SZ_TOP_LEFT, SZ_BOTTOM_RIGHT);
	SetResize(IDC_STATIC1, SZ_BOTTOM_LEFT, SZ_BOTTOM_LEFT);
	SetResize(IDC_STATIC2, SZ_BOTTOM_LEFT, SZ_BOTTOM_LEFT);
	SetResize(IDC_COMBO1, SZ_BOTTOM_LEFT, SZ_BOTTOM_RIGHT);
	SetResize(IDC_EDITOR_COMMAND, SZ_BOTTOM_LEFT, SZ_BOTTOM_RIGHT);

	CExternalToolsManager *pTools = GetIEditor()->GetExternalToolsManager();
	for (int i = 0; i < pTools->GetToolsCount(); i++)
	{
		Tool *tool = new Tool;
		CExternalTool* extTool = pTools->GetTool(i);
		tool->bToggle = extTool->m_variableToggle;
		tool->editorCommand = extTool->m_editorCommand;
		tool->command = extTool->m_command;

		int id = m_editList.AddString(extTool->m_title);
		m_editList.SetItemData( id,(DWORD_PTR)tool );
	}

	{
		IConsole *console = GetIEditor()->GetSystem()->GetIConsole();
		std::vector<const char*> cmds;
		cmds.resize( console->GetNumVars() );
		size_t cmdCount = console->GetSortedVars( &cmds[0],cmds.size() );
		for (int i = 0; i < cmdCount; i++)
		{
			m_consoleCmd.AddString(cmds[i]);
		}
	}

	{
		/// Fill editor commands.
		std::vector<CString> cmds;
		GetIEditor()->GetCommandManager()->GetSortedCmdList(cmds);
		for (int i = 0; i < cmds.size(); i++)
		{
			m_editorCmd.AddString(cmds[i]);
		}
	}
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
void CToolsConfigPage::OnOK()
{
	CExternalToolsManager *pTools = GetIEditor()->GetExternalToolsManager();
	pTools->ClearTools();
	CString title;
	for (int i = 0; i < m_editList.GetCount(); i++)
	{
		m_editList.GetText(i,title);
		if (title.IsEmpty())
			continue;

		Tool *tool = (Tool*)m_editList.GetItemData(i);
		if (!tool)
			continue;

		CExternalTool *extTool = new CExternalTool;

		extTool->m_title = title;
		extTool->m_editorCommand = tool->editorCommand;
		extTool->m_command = tool->command;
		extTool->m_variableToggle = tool->bToggle;
		pTools->AddTool(extTool);
	}
	pTools->Save();
}

//////////////////////////////////////////////////////////////////////////
void CToolsConfigPage::OnSelchangeEditList()
{
	int iIndex = m_editList.GetCurSel();

	Tool* pTool = (Tool*)m_editList.GetItemData(iIndex);
	if (pTool != NULL && iIndex != LB_ERR)
	{
		CString command = pTool->command;
		m_consoleCmd.SetWindowText( command );
		m_editorCmd.SetWindowText(pTool->editorCommand);
		m_toggleVar.SetCheck( (pTool->bToggle)?BST_CHECKED:BST_UNCHECKED );
	}
	else
	{
		m_consoleCmd.SetWindowText(_T(""));
		m_editorCmd.SetWindowText("");
		m_toggleVar.SetCheck(0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CToolsConfigPage::OnChangeEditorCmd()
{
	ReadFromControls(CTRL_COMMAND);
}

//////////////////////////////////////////////////////////////////////////
void CToolsConfigPage::OnChangeConsoleCmd()
{
	ReadFromControls(CTRL_COMMAND);
}

//////////////////////////////////////////////////////////////////////////
void CToolsConfigPage::OnSelEditorCmd()
{
	int sel = m_editorCmd.GetCurSel();
	if (sel != CB_ERR)
	{
		CString s;
		m_editorCmd.GetLBText(sel,s);
		m_editorCmd.SetWindowText(s);
	}
	ReadFromControls(CTRL_COMMAND);
}

//////////////////////////////////////////////////////////////////////////
void CToolsConfigPage::OnSelConsoleCmd()
{
	int sel = m_consoleCmd.GetCurSel();
	if (sel != CB_ERR)
	{
		CString s;
		m_consoleCmd.GetLBText(sel,s);
		m_consoleCmd.SetWindowText(s);
	}
	ReadFromControls(CTRL_COMMAND);
}

//////////////////////////////////////////////////////////////////////////
void CToolsConfigPage::ReadFromControls( int ctrl )
{
	int iIndex = m_editList.GetCurSel();

	if (iIndex != LB_ERR)
	{
		Tool* pTool = (Tool*)m_editList.GetItemData(iIndex);
		if (pTool != NULL)
		{
			switch (ctrl)
			{
			case CTRL_COMMAND:
				m_consoleCmd.GetWindowText( pTool->command );
				m_editorCmd.GetWindowText( pTool->editorCommand );
				break;
			case CTRL_TOGGLE:
				pTool->bToggle = m_toggleVar.GetCheck() != 0;
				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CToolsConfigPage::OnToggleVar()
{
	ReadFromControls(CTRL_TOGGLE);
}

//////////////////////////////////////////////////////////////////////////
void CToolsConfigPage::OnNewItem()
{
	int iItem = m_editList.GetCurrentIndex();
	if (iItem != -1)
	{
		Tool* pTool = new Tool;
		m_tools.push_back(pTool);

		pTool->command = "";
		pTool->bToggle = false;
		
		m_editList.SetItemData(iItem, (DWORD_PTR)pTool);
		m_editList.SetCurSel(iItem);

		m_consoleCmd.SetWindowText(_T(""));
		m_editorCmd.SetWindowText(_T(""));
		m_toggleVar.SetCheck(0);
	}
}