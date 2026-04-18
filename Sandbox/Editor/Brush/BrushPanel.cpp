////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   brushpanel.cpp
//  Version:     v1.00
//  Created:     2/12/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "BrushPanel.h"

#include "..\Objects\ObjectManager.h"
#include "..\Objects\BrushObject.h"
#include "EditMode\VertexMode.h"

// CBrushPanel dialog

IMPLEMENT_DYNAMIC(CBrushPanel, CDialog)
CBrushPanel::CBrushPanel(CWnd* pParent /*=NULL*/)
	: CDialog(CBrushPanel::IDD, pParent)
{
	m_brushObj = 0;
	Create( IDD,pParent );
}

//////////////////////////////////////////////////////////////////////////
CBrushPanel::~CBrushPanel()
{
}

//////////////////////////////////////////////////////////////////////////
void CBrushPanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SAVE_TO_CGF, m_btn1);
	DDX_Control(pDX, IDC_REFRESH, m_btn2);
	//DDX_Control(pDX, IDC_SIDES, m_sides);
	DDX_Control(pDX, IDC_GEOMETRY_EDIT, m_subObjEdit);
}

//////////////////////////////////////////////////////////////////////////
void CBrushPanel::SetBrush( CBrushObject *obj )
{
	m_brushObj = obj;
}

BEGIN_MESSAGE_MAP(CBrushPanel, CDialog)
	//ON_CBN_SELENDOK(IDC_SIDES, OnCbnSelendokSides)
	ON_BN_CLICKED(IDC_SAVE_TO_CGF, OnBnClickedSavetoCGF)
	ON_BN_CLICKED(IDC_REFRESH, OnBnClickedReload)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
BOOL CBrushPanel::OnInitDialog()
{
	__super::OnInitDialog();

	/*
	CString str;
	for (int i = 0; i < 10; i++)
	{
		str.Format( "%d",i );
		m_sides.AddString( str );
	}
	*/
	//SetResize( IDC_GEOMETRY_EDIT,SZ_HORRESIZE(1) );
	//SetResize( IDC_SAVE_TO_CGF,SZ_RESIZE(0) );
	//SetResize( IDC_REFRESH,SZ_RESIZE(0) );

	m_subObjEdit.SetToolClass( RUNTIME_CLASS(CSubObjectModeTool) );

	return TRUE;  // return TRUE unless you set the focus to a control
}

//////////////////////////////////////////////////////////////////////////
void CBrushPanel::OnCbnSelendokSides()
{
	
}

//////////////////////////////////////////////////////////////////////////
void CBrushPanel::OnBnClickedSavetoCGF()
{
	if (m_brushObj)
	{
		CString filename;		
		if (CFileUtil::SelectSaveFile( "CGF Files (*.cgf)|*.cgf",CRY_GEOMETRY_FILE_EXT,Path::GetGameFolder()+CString("\\objects"),filename ))
		{
			filename = Path::GetRelativePath(filename);
			if (!filename.IsEmpty())
			{
				m_brushObj->SaveToCGF( filename );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrushPanel::OnBnClickedReload()
{
	if (m_brushObj)
	{
		m_brushObj->ReloadGeometry();
	}
	else
	{
		// Reset all selected brushes.
		CSelectionGroup *selection = GetIEditor()->GetSelection();
		for (int i = 0; i < selection->GetCount(); i++)
		{
			CBaseObject *pBaseObj = selection->GetObject(i);
			if (pBaseObj->IsKindOf(RUNTIME_CLASS(CBrushObject)))
				((CBrushObject*)pBaseObj)->ReloadGeometry();
		}
	}
}
