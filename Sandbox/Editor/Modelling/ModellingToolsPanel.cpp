////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek, 2007.
// -------------------------------------------------------------------------
//  File name:   ModellingToolsPanel.cpp
//  Version:     v1.00
//  Created:     18/1/2007 by Timur.
//  Description: Polygon creation edit tool.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ModellingToolsPanel.h"
#include "ObjectCreateTool.h"

#include "..\Objects\SubObjSelection.h"
#include "..\EditMode\SubObjSelectionTypePanel.h"
#include "..\EditMode\SubObjSelectionPanel.h"
#include "..\EditMode\SubObjDisplayPanel.h"

/////////////////////////////////////////////////////////////////////////////
// CModellingToolsPanel dialog


CModellingToolsPanel::CModellingToolsPanel(CWnd* pParent /*=NULL*/)
	: CDialog(CModellingToolsPanel::IDD, pParent)
{
	m_lastPressed = -1;
	m_nTimer = 0;

	m_pTypePanel = 0;
	m_selectionTypePanelId = 0;
	m_selectionPanelId = 0;
	m_displayPanelId = 0;
}

CModellingToolsPanel::~CModellingToolsPanel()
{
	ReleaseButtons();
}


void CModellingToolsPanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CModellingToolsPanel, CDialog)
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_WM_SIZE()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CModellingToolsPanel message handlers

BOOL CModellingToolsPanel::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) 
{
	/*
	// Route the commands to the view
	if (nID == ID_INSERT_ENTITY && AfxGetMainWnd())
	{
		AfxGetMainWnd()->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
	}
	//((CFrameWnd *) (AfxGetMainWnd()))->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
	*/
	
	return CDialog::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

void CModellingToolsPanel::UncheckAll()
{
	m_lastPressed = -1;
	for (int i = 0; i < m_buttons.size(); i++)
	{
		m_buttons[i]->SetCheck( BST_UNCHECKED );
	}
}

BOOL CModellingToolsPanel::OnCommand(WPARAM wParam, LPARAM lParam) 
{
	// TODO: Add your specialized code here and/or call the base class
	if (HIWORD(wParam) == BN_CLICKED)
	{
		int ctrlId = LOWORD(wParam);

		OnButtonPressed( ctrlId );
		return TRUE;
	}
	
	return CDialog::OnCommand(wParam, lParam);
}

BOOL CModellingToolsPanel::OnInitDialog() 
{
	CDialog::OnInitDialog();

	CreateButtons();
	
	return FALSE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
void CModellingToolsPanel::CreateButtons()
{
	int xOffset = 5;
	int yOffset = 4;

	int ySeprator = 4;

	int m_buttonHeight = 18;

	if (gSettings.gui.bWindowsVista)
	{
		m_buttonHeight = 20;
		ySeprator = 2;
	}

	int row = 0;
	int col = 0;

	CRect rc;
	GetClientRect( rc );

	int buttonWidth = ((rc.right - rc.left) - 3*xOffset)/2;
	int newHeight = 0;

	XmlParser parser;
	XmlNodeRef root = parser.parse( "Editor/ModellingPanel.xml");
	if (!root)
	{
		return;
	}

	int nCmd = 0;
	for (int nGroup = 0; nGroup < root->getChildCount(); nGroup++)
	{
		XmlNodeRef groupNode = root->getChild(nGroup);
		if (!groupNode->isTag("Group"))
			continue;
		for (int nItem = 0; nItem < groupNode->getChildCount(); nItem++)
		{
			XmlNodeRef itempNode = groupNode->getChild(nItem);
			if (!itempNode->isTag("Item"))
				continue;
			CString iconFile = itempNode->getAttr("Icon");
			CString name = itempNode->getAttr("Name");
			CString tool = itempNode->getAttr("Tool");
			m_commandMap[name] = tool;

			//////////////////////////////////////////////////////////////////////////
			CColorCheckBox *button = new CColorCheckBox;
			button->SetPushedBkColor( RGB(255,255,0) );
			//button->SetBevel(1);
			//button->SetBkColor( RGB(255,255,0) );
			CRect brc;

			brc.left = xOffset + col*(buttonWidth + xOffset);
			brc.right = brc.left + buttonWidth;

			brc.top = yOffset + row*(m_buttonHeight + ySeprator);
			brc.bottom = brc.top + m_buttonHeight;

			newHeight = brc.bottom + ySeprator;

			//button->Create( category,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTORADIOBUTTON|BS_PUSHLIKE,brc,this,i );
			//button->Create( category,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,brc,this,i );
			button->Create( name,WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,brc,this,nCmd );
			button->SetFont( CFont::FromHandle( (HFONT)gSettings.gui.hSystemFont) );
			//button->ModifyStyleEx( 0,WS_EX_STATICEDGE,SWP_FRAMECHANGED );
			button->SetCheck(BST_UNCHECKED);
			m_buttons.push_back( button );

			col++;
			brc.left = xOffset + col*(buttonWidth + xOffset);
			brc.right = brc.left + buttonWidth;
			if (brc.right > rc.right-4)
			{
				col = 0;
				row++;
			}
			//////////////////////////////////////////////////////////////////////////
			nCmd++;
		}
	}

	SetWindowPos( NULL,0,0,rc.right-rc.left,newHeight+4,SWP_NOMOVE );
}

//////////////////////////////////////////////////////////////////////////
void CModellingToolsPanel::ReleaseButtons()
{
	for (int i = 0; i < m_buttons.size(); i++)
	{
		delete m_buttons[i];
	}
	m_buttons.clear();
}

void CModellingToolsPanel::OnButtonPressed( int i )
{
	ASSERT( i >= 0 && i < m_buttons.size() );

	if (i == m_lastPressed)
	{
		UncheckAll();
		if (GetIEditor()->GetEditTool())
			GetIEditor()->SetEditTool(0);
		return;
	}
	
	UncheckAll();

	m_lastPressed = i;
	m_buttons[i]->SetCheck(BST_CHECKED);

	CString name;
	m_buttons[i]->GetWindowText( name );

	m_currentToolName = stl::find_in_map(m_commandMap,name,"");
	if (!m_currentToolName.IsEmpty())
	{
		GetIEditor()->SetEditTool( m_currentToolName );
	}

	// Start monitoring button state.
	StartTimer();
}

void CModellingToolsPanel::OnTimer(UINT_PTR nIDEvent) 
{
	CDialog::OnTimer(nIDEvent);

	CEditTool *tool = GetIEditor()->GetEditTool();
	if (!tool || !tool->GetClassDesc() || strcmp(tool->GetClassDesc()->ClassName(),m_currentToolName)!=0)
	{
		UncheckAll();
		StopTimer();
	}
}

void CModellingToolsPanel::StartTimer()
{
	StopTimer();
	m_nTimer = SetTimer(1,500,NULL);
}
	
void CModellingToolsPanel::StopTimer()
{
	if (m_nTimer != 0)
		KillTimer(m_nTimer);
	m_nTimer = 0;
}

void CModellingToolsPanel::OnDestroy() 
{
	StopTimer();
	CDialog::OnDestroy();
}

void CModellingToolsPanel::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);

	/*
	static bIgnoreSize = false;
	if (m_buttons.size() > 0 && !bIgnoreSize)
	{
		bIgnoreSize = true;
		ReleaseButtons();
		CreateButtons();
		bIgnoreSize = false;
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
void CModellingToolsPanel::InitDefaultModellingPanels()
{
	//////////////////////////////////////////////////////////////////////////
	// Show selection dialog.
	//////////////////////////////////////////////////////////////////////////

	if (!m_selectionTypePanelId)
	{
		m_pTypePanel = new CSubObjSelectionTypePanel(AfxGetMainWnd());
		m_pTypePanel->SelectElemtType(SO_ELEM_VERTEX);
		m_selectionTypePanelId = GetIEditor()->AddRollUpPage( ROLLUP_MODELLING,"Selection Type",m_pTypePanel);
	}
	if (!m_selectionPanelId)
	{
		CSubObjSelectionPanel *pPanel = new CSubObjSelectionPanel(AfxGetMainWnd());
		m_selectionPanelId = GetIEditor()->AddRollUpPage( ROLLUP_MODELLING,"Selection",pPanel );
	}
	/*
	if (!m_displayPanelId)
	{
	CPropertiesPanel *pPanel = new CPropertiesPanel( AfxGetMainWnd() );
	pPanel->AddVars( m_pDisplayPanelUI->pVarBlock,functor(*m_pDisplayPanelUI,&CDisplayPanelUI::OnVarChanged) );
	m_displayPanelId = GetIEditor()->AddRollUpPage( ROLLUP_OBJECTS,"Display Selection",pPanel );
	}
	*/
	if (!m_displayPanelId)
	{
		CSubObjDisplayPanel *pPanel = new CSubObjDisplayPanel( AfxGetMainWnd() );
		m_displayPanelId = GetIEditor()->AddRollUpPage( ROLLUP_MODELLING,"Display Selection",pPanel );
	}
}
