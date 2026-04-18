// CharPanel_Character.cpp : implementation file
//

#include "stdafx.h"
#include "CharPanel_Character.h"
#include "MaterialSelectorDialog.h"
#include "ModelViewportCE.h"
#include "Material\MaterialManager.h"
#include "CharacterEditor.h"

// CCharacterPropertiesDlg dialog

IMPLEMENT_DYNAMIC(CCharacterPropertiesDlg, CDialog)

CCharacterPropertiesDlg::CCharacterPropertiesDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CCharacterPropertiesDlg::IDD, pParent)
{
	m_pModelViewportCE = 0;

	pCharacterChangeListener = 0;
}

CCharacterPropertiesDlg::~CCharacterPropertiesDlg()
{
}

void CCharacterPropertiesDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_MATERIAL_NAME, m_materialEdit);
	DDX_Control(pDX, IDC_DEFAULT_MTL, m_defaultMtlBtn);
	DDX_Control(pDX, IDC_MATERIAL, m_browseMtlBtn);
}

BOOL CCharacterPropertiesDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetMaterial(NULL);

	return TRUE;
}

void CCharacterPropertiesDlg::OnOK()
{
	// Don't call the base class implementation - somehow ok messages are being generated
	// in response to command messages - this would close the window unless we stop it here.
}

//////////////////////////////////////////////////////////////////////////
void CCharacterPropertiesDlg::SetMaterial(CMaterial *pMaterial)
{
	IMaterial* pOverrideMaterial = 0;
	if (m_pModelViewportCE != 0)
	{
		ICharacterInstance* pCharacterInstance = m_pModelViewportCE->GetCharacterBase();
		if (pCharacterInstance != 0)
			pOverrideMaterial = pCharacterInstance->GetMaterialOverride();
	}
	if (pOverrideMaterial != 0)
		m_materialEdit.SetWindowText(pOverrideMaterial->GetName());
	else
		m_materialEdit.SetWindowText("");
}

BEGIN_MESSAGE_MAP(CCharacterPropertiesDlg, CDialog)
	ON_BN_CLICKED(IDC_DEFAULT_MTL, OnBnClickedDefaultMtl)
	ON_BN_CLICKED(IDC_MATERIAL, OnBnClickedMaterial)
END_MESSAGE_MAP()

// CCharacterPropertiesDlg message handlers

//////////////////////////////////////////////////////////////////////////
void CCharacterPropertiesDlg::OnBnClickedDefaultMtl()
{
	m_materialEdit.SetWindowText("");
	m_pModelViewportCE->GetCharacterBase()->SetMaterial(0);
	this->SendOnCharacterChanged();
}

//////////////////////////////////////////////////////////////////////////
void CCharacterPropertiesDlg::OnBnClickedMaterial()
{
	CMaterialSelectorDialog dlg;
	dlg.SetMaterialName(m_pModelViewportCE->GetCharacterBase()->GetMaterial()->GetName());
	if (dlg.DoModal() == IDOK)
	{
		m_materialEdit.SetWindowText(dlg.GetMaterialName());

		_smart_ptr<CMaterial> pMaterial = GetIEditor()->GetMaterialManager()->LoadMaterial(dlg.GetMaterialName(), false);
		_smart_ptr<IMaterial> pIMaterial = 0;
		if (pMaterial)
			pIMaterial = pMaterial->GetMatInfo();
		m_pModelViewportCE->GetCharacterBase()->SetMaterial(pIMaterial);

		this->SendOnCharacterChanged();
	}
}

void CCharacterPropertiesDlg::SetCharacterChangeListener(ICharacterChangeListener* pListener)
{
	this->pCharacterChangeListener = pListener;
}

void CCharacterPropertiesDlg::SendOnCharacterChanged()
{
	if (this->pCharacterChangeListener)
		this->pCharacterChangeListener->OnCharacterChanged();
}
