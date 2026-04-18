////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   AnimationGraphPreviewDialog.cpp
//  Version:     v1.00
//  Created:     5/5/2006 by MichaelS.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "AnimationGraphPreviewDialog.h"
#include "CharacterEditor/ModelViewportCE.h"

#include <ICryAnimation.h>

IMPLEMENT_DYNAMIC(CAnimationGraphPreviewDialog,CToolbarDialog)

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CAnimationGraphPreviewDialog, CToolbarDialog)
	ON_WM_SIZE()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
// CAnimationGraphPreviewDialog
//////////////////////////////////////////////////////////////////////////
CAnimationGraphPreviewDialog::CAnimationGraphPreviewDialog(const char* szCharacterName) : CToolbarDialog(IDD,NULL), m_sCharacterName(szCharacterName)
{
	m_pModelViewport = 0;
}

//////////////////////////////////////////////////////////////////////////
CAnimationGraphPreviewDialog::~CAnimationGraphPreviewDialog()
{
}

//////////////////////////////////////////////////////////////////////////
void CAnimationGraphPreviewDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	CRect rcClient;
	GetClientRect(rcClient);
	if (!m_pModelViewport || !m_pModelViewport->m_hWnd)
		return;

	m_pModelViewport->MoveWindow( rcClient );

}

//////////////////////////////////////////////////////////////////////////
void CAnimationGraphPreviewDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
}


//////////////////////////////////////////////////////////////////////////
BOOL CAnimationGraphPreviewDialog::OnInitDialog()
{
	CRect rcClient;
	GetClientRect(rcClient);

	m_pModelViewport = new CModelViewportCE;
	m_pModelViewport->SetType( ET_ViewportModel );
	m_pModelViewport->SetParent(this);
	//m_pModelViewport->Create( &m_pane1,ET_ViewportModel,"Model Preview" );
	m_pModelViewport->MoveWindow(rcClient);
	m_pModelViewport->ShowWindow(SW_SHOW);

	m_pModelViewport->LoadObject(m_sCharacterName.c_str());

	RecalcLayout();
	//MoveWindow(&rect);
	//ShowWindow(SW_SHOW);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CAnimationGraphPreviewDialog::RedrawPreview()
{
	if (m_pModelViewport)
		m_pModelViewport->Update();
}

//////////////////////////////////////////////////////////////////////////
ICharacterInstance* CAnimationGraphPreviewDialog::GetCharacter()
{
	ICharacterInstance* pInstance = 0;
	if (m_pModelViewport)
		pInstance = m_pModelViewport->GetCharacterBase();
	return pInstance;
}

//////////////////////////////////////////////////////////////////////////
void CAnimationGraphPreviewDialog::SetCharacter(const char* szCharacterName)
{
	m_sCharacterName = szCharacterName;
	if ( m_pModelViewport )
		m_pModelViewport->LoadObject( m_sCharacterName.c_str() );
}
