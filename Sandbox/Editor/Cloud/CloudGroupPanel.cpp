////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2005.
// -------------------------------------------------------------------------
//  File name:   CloudGroupPanel.cpp
//  Version:     v1.00
//  Created:     10/02/2005 by Sergiy Shaykin.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CloudGroupPanel.h"

// CCloudGroupPanel dialog

IMPLEMENT_DYNAMIC(CCloudGroupPanel, CXTResizeDialog)
CCloudGroupPanel::CCloudGroupPanel(CWnd* pParent /*=NULL*/)
	: CXTResizeDialog(CCloudGroupPanel::IDD, pParent)
{
	Create( IDD,pParent );
	m_cloud = 0;
}

//////////////////////////////////////////////////////////////////////////
CCloudGroupPanel::~CCloudGroupPanel()
{
	m_cloud = 0;
}

//////////////////////////////////////////////////////////////////////////
void CCloudGroupPanel::DoDataExchange(CDataExchange* pDX)
{
	CXTResizeDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CCloudGroupPanel, CXTResizeDialog)
	ON_BN_CLICKED(IDC_CLOUDS_GENERATE, OnCloudsGenerate)
	ON_BN_CLICKED(IDC_CLOUDS_IMPORT, OnCloudsImport)
	ON_BN_CLICKED(IDC_CLOUDS_EXPORT, OnCloudsExport)
	ON_BN_CLICKED(IDC_CLOUDS_BROWSE, OnCloudsBrowse)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
BOOL CCloudGroupPanel::OnInitDialog()
{
	CXTResizeDialog::OnInitDialog();

	//SetFileBrowseText();

	return TRUE;  // return TRUE unless you set the focus to a control
}

void CCloudGroupPanel::Init(CCloudGroup * cloud)
{
	m_cloud = cloud;
	SetFileBrowseText();
}

void CCloudGroupPanel::OnCloudsGenerate()
{
	if(m_cloud)
		m_cloud->Generate();
}

void CCloudGroupPanel::OnCloudsImport()
{
	if(m_cloud)
		m_cloud->Import();
}


void CCloudGroupPanel::OnCloudsBrowse()
{
	if(m_cloud)
		m_cloud->BrowseFile();
}

void CCloudGroupPanel::OnCloudsExport()
{
	if(m_cloud)
		m_cloud->Export();
}

void CCloudGroupPanel::SetFileBrowseText()
{
	if(m_cloud)
	{
		SetDlgItemText(IDC_CLOUDS_FILE, m_cloud->GetExportFileName());
	}
}