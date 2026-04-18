// TerrainMoveToolPanel.cpp : implementation file
//

#include "stdafx.h"
#include "TerrainMoveToolPanel.h"
#include "TerrainMoveTool.h"

#include "CryEditDoc.h"

#include <I3DEngine.h>
#include "TerrainMoveTool.h"

/////////////////////////////////////////////////////////////////////////////
// CTerrainMoveToolPanel dialog


CTerrainMoveToolPanel::CTerrainMoveToolPanel(CTerrainMoveTool *tool,CWnd* pParent /*=NULL*/)
	: CDialog(CTerrainMoveToolPanel::IDD, pParent)
{
	//{{AFX_DATA_INIT(CTerrainMoveToolPanel)
	//}}AFX_DATA_INIT

	m_tool = tool;
	Create( IDD,pParent );

	assert( tool != 0 );
	
}

void CTerrainMoveToolPanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTerrainMoveToolPanel)
	DDX_Control(pDX, IDC_SELECT_SOURCE, m_selectSource);
	DDX_Control(pDX, IDC_SELECT_TARGET, m_selectTarget);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTerrainMoveToolPanel, CDialog)
	//{{AFX_MSG_MAP(CTerrainMoveToolPanel)
	ON_EN_UPDATE(IDC_DYMX, OnUpdateNumbers)
	ON_EN_UPDATE(IDC_DYMY, OnUpdateNumbers)
	ON_EN_UPDATE(IDC_DYMZ, OnUpdateNumbers)
	ON_BN_CLICKED(IDC_SELECT_SOURCE, OnSelectSource)
	ON_BN_CLICKED(IDC_SELECT_TARGET, OnSelectTarget)
	ON_BN_CLICKED(IDC_COPY, OnCopyButton)
	ON_BN_CLICKED(IDC_MOVE, OnMoveButton)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTerrainMoveToolPanel message handlers

BOOL CTerrainMoveToolPanel::OnInitDialog() 
{
	CDialog::OnInitDialog();

	CHeightmap *pHeightmap = GetIEditor()->GetHeightmap();
	Vec3 max = pHeightmap->HmapToWorld(CPoint(pHeightmap->GetWidth(), pHeightmap->GetHeight()));

	m_dymX.Create( this,IDC_DYMX );
	m_dymX.SetRange( 0, max.x);
	m_dymX.SetValue(m_tool->GetDym().x);

	m_dymY.Create( this,IDC_DYMY );
	m_dymY.SetRange( 0, max.y);
	m_dymY.SetValue(m_tool->GetDym().y);

	m_dymZ.Create( this,IDC_DYMZ );
	m_dymZ.SetRange( 0, 10000);
	m_dymZ.SetValue(m_tool->GetDym().z);

	UpdateButtons();

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


//////////////////////////////////////////////////////////////////////////
void CTerrainMoveToolPanel::OnCopyButton() 
{
	bool bOnlyVegetation = false;
	bool bOnlyTerrain = false;
	CButton * but = (CButton *) GetDlgItem(IDC_ONLY_VEGETATION);
	if(but)
		bOnlyVegetation = but->GetCheck();
	but = (CButton *) GetDlgItem(IDC_ONLY_TERRAIN);
	if(but)
		bOnlyTerrain = but->GetCheck();
	m_tool->Move(true, bOnlyVegetation, bOnlyTerrain);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveToolPanel::OnMoveButton() 
{
	bool bOnlyVegetation = false;
	bool bOnlyTerrain = false;
	CButton * but = (CButton *) GetDlgItem(IDC_ONLY_VEGETATION);
	if(but)
		bOnlyVegetation = but->GetCheck();
	but = (CButton *) GetDlgItem(IDC_ONLY_TERRAIN);
	if(but)
		bOnlyTerrain = but->GetCheck();
	m_tool->Move(false, bOnlyVegetation, bOnlyTerrain);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveToolPanel::OnSelectSource() 
{
	m_selectSource.SetCheck(1);
	m_selectTarget.SetCheck(0);
	m_tool->Select(1);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveToolPanel::OnSelectTarget() 
{
	m_selectSource.SetCheck(0);
	m_selectTarget.SetCheck(1);
	m_tool->Select(2);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveToolPanel::OnUpdateNumbers() 
{
	m_tool->SetDym(Vec3(m_dymX.GetValue(), m_dymY.GetValue(), m_dymZ.GetValue()));
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveToolPanel::UpdateButtons()
{
	CWnd * but = 0;
	if(m_tool->m_source.isCreated && m_tool->m_target.isCreated)
	{
		if(but = GetDlgItem(IDC_COPY))
			but->EnableWindow(true);
		if(but = GetDlgItem(IDC_MOVE))
			but->EnableWindow(true);
	}
	else
	{
		if(but = GetDlgItem(IDC_COPY))
			but->EnableWindow(false);
		if(but = GetDlgItem(IDC_MOVE))
			but->EnableWindow(false);
	}
}
