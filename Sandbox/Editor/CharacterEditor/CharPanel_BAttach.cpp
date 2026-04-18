// CharPanel_BAttach.cpp : implementation file
//

#include "stdafx.h"
#include <I3DEngine.h>
#include <ICryAnimation.h>
#include "CharPanel_BAttach.h"
//#include "CharPanel_FAttach.h"

#include "ModelViewport.h"

/////////////////////////////////////////////////////////////////////////////
// CharPanel_BAttach dialog
/////////////////////////////////////////////////////////////////////////////


void CharPanel_BAttach::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
//	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_OBJECT, m_objectName);
	DDX_Control(pDX, IDC_BROWSE_OBJECT,m_browseObjectBtn);
	DDX_Control(pDX, IDC_WEAPONMODEL, m_attachBtn);
	DDX_Control(pDX, IDC_DETACH, m_detachBtn);
	DDX_Control(pDX, IDC_DETACHALL, m_detachAllBtn);
	DDX_Control(pDX, IDC_BONE, m_boneName);
}


BEGIN_MESSAGE_MAP(CharPanel_BAttach, CDialog)
	ON_WM_CREATE()
	ON_BN_CLICKED(IDC_WEAPONMODEL, OnBnClickedAttachObject)
	ON_BN_CLICKED(IDC_DETACH, OnBnClickedDetachObject)
	ON_BN_CLICKED(IDC_DETACHALL, OnBnClickedDetachAll)
	ON_BN_CLICKED(IDC_BROWSE_OBJECT, OnBnClickedBrowseObject)
END_MESSAGE_MAP()


BOOL CharPanel_BAttach::OnInitDialog() 
{
	CDialog::OnInitDialog();
	m_boneName.SetWindowText( "weapon_bone" );
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CharPanel_BAttach::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) 
{
	// Route the commands to the MainFrame
	if (AfxGetMainWnd())
		AfxGetMainWnd()->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
	
	return CDialog::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}



void CharPanel_BAttach::OnBnClickedAttachObject()
{
	CString bone,model;
	m_boneName.GetWindowText( bone );
	m_objectName.GetWindowText( model );
	m_ModelViewPort->AttachObjectToBone( model,bone );
}

void CharPanel_BAttach::OnBnClickedDetachObject()
{
	CString bone,model;
	m_boneName.GetWindowText( bone );
	m_objectName.GetWindowText( model );

	//ICharacterInstance* pCharacter = m_ModelViewPort->GetCharacterBase();
	//int nBone = pCharacter->GetModel()->GetBoneByName(bone);
	//if (nBone >= 0)
	//	pCharacter->GetIAttachmentManager()->DetachAllFromBone(nBone);
}

void CharPanel_BAttach::OnBnClickedDetachAll()
{
//	m_ModelViewPort->GetCharacterBase()->GetIAttachmentManager()->DetachAll();
}

void CharPanel_BAttach::OnBnClickedBrowseObject()
{
	CString relFileName;
	if (CFileUtil::SelectSingleFile( EFILE_TYPE_GEOMETRY,relFileName ))
	{
		m_objectName.SetWindowText( relFileName );
	}
}

//////////////////////////////////////////////////////////////////////////
void CharPanel_BAttach::ClearBones()
{
	m_boneName.ResetContent();
}

//////////////////////////////////////////////////////////////////////////
void CharPanel_BAttach::AddBone( const CString &bone )
{
	m_boneName.AddString( bone );
}

//////////////////////////////////////////////////////////////////////////
void CharPanel_BAttach::SelectBone( const CString &bone )
{
	m_boneName.SelectString( -1,bone );
}

