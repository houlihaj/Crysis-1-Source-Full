// CharEditAttachmentsDlg.cpp : implementation file
//

#include "stdafx.h"
#include <I3DEngine.h>
#include <ICryAnimation.h>

#include "StringDlg.h"
#include "CharPanel_Attachments.h"
#include "CharacterEditor.h"
#include "ModelViewportCE.h"
#include ".\charpanel_attachments.h"
#include "MaterialSelectorDialog.h"
#include "Material/MaterialManager.h"

IMPLEMENT_DYNAMIC(CAttachmentsDlg, CDialog)

int g_hingeIdx[] = { -1, 0x00,0x10,0x08,0x18, 0x04,0x14,0x0C,0x1C, 
												 0x01,0x11,0x09,0x19, 0x05,0x15,0x0D,0x1D,
												 0x02,0x12,0x0A,0x1A, 0x06,0x16,0x0E,0x1E };
char *g_hingeIdxDesc[] = { 
	"no hinge", "-x,-y","-x,+y","-x,-z","-x,+z", "+x,-y","+x,+y","+x,-z","+x,+z",
							"-y,-z","-y,+z","-y,-x","-y,+x", "+y,-z","+y,+z","+y,-x","+y,+x",
							"-z,-x","-z,+x","-z,-y","-z,+y", "+z,-x","+z,+x","+z,-y","+z,+y" };


class CBoneList
{
public:
	CBoneList()
	{
	}

	void Clear()
	{
		m_indexNameMap.clear();
		m_nameIndexMap.clear();
	}

	int AddBone(const CString& name)
	{
		int index = int(m_indexNameMap.size());
		m_indexNameMap.push_back(name);
		m_nameIndexMap.insert(std::make_pair(name, index));
		return index;
	}

	const CString& NameFromIndex(int index)
	{
		return m_indexNameMap[index];
	}

	int IndexFromName(const CString& name)
	{
		return m_nameIndexMap[name];
	}

private:
	std::vector<CString> m_indexNameMap;
	std::map<CString, int> m_nameIndexMap;
};

class CBoneComboBoxManager
{
public:
	CBoneComboBoxManager(CComboBox& comboBox)
		:	m_comboBox(comboBox),
		m_selectionIndex(-1)
	{
	}

	void Clear()
	{
		m_boneList.Clear();
		//for (int i = m_comboBox.GetCount()-1; i >= 0; i--)
		//	 m_comboBox->DeleteString( i );
		m_comboBox.ResetContent();
	}

	void AddBone(const CString& name)
	{
		int index = m_boneList.AddBone(name);
		CString text;
		text.Format("%.2d - %s", index, name.GetString());
		m_comboBox.AddString(text.GetString());
	}

	void SelectBone(const CString& name)
	{
		m_selectionIndex = m_boneList.IndexFromName(name);
		m_comboBox.SetCurSel(m_selectionIndex);
	}

	void SelectBone(int index)
	{
		m_selectionIndex = index;
		m_comboBox.SetCurSel(m_selectionIndex);
	}

	const CString& GetSelectedBone()
	{
		if (m_selectionIndex == -1)
		{
			static CString empty("");
			return empty;
		}
		return m_boneList.NameFromIndex(m_selectionIndex);
	}

private:
	CBoneList m_boneList;
	CComboBox& m_comboBox;
	int m_selectionIndex;
};

BOOL CAttachmentsDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_pBoneComboBoxManager = new CBoneComboBoxManager(m_boneName);

	m_hingeLimit.Create(this,IDC_HINGE_LIMIT,CNumberCtrl::LEFTALIGN );
	m_hingeLimit.SetRange(0,180.0f);
	m_hingeLimit.SetInteger(true);
	m_hingeDamping.Create(this,IDC_HINGE_DAMPING,CNumberCtrl::LEFTALIGN );
	m_hingeDamping.SetRange(0,10.0f);

	m_hingeIndex.ResetContent();
	for(int i=0;i<sizeof(g_hingeIdx)/sizeof(g_hingeIdx[0]);i++)
		m_hingeIndex.AddString(g_hingeIdxDesc[i]);

	return TRUE;
}


void CAttachmentsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_OBJECT, m_objectName);
	DDX_Control(pDX, IDC_MATERIAL, m_materialName);

	DDX_Control(pDX, IDC_CLEAR_BUTTON, m_ButtonCLEAR);

	DDX_Control(pDX, IDC_BUTTON_RENAME, m_ButtonRENAME);
	DDX_Control(pDX, IDC_BUTTON_REMOVE, m_ButtonREMOVE);
	DDX_Control(pDX, IDC_BUTTON_EXPORT, m_ButtonEXPORT);

	DDX_Control(pDX, IDC_BROWSE_OBJECT,m_browseObjectBtn);
	DDX_Control(pDX, IDC_BONE, m_boneName);
	DDX_Control(pDX, IDC_ATTACHMENTS,m_attachmentsList );

	DDX_Radio(pDX, IDC_BUTTON_BONEATTACH,m_AttachmentType );
	DDX_Check(pDX, IDC_BUTTON_ALIGNBONEATTACHMENT,m_AlignBoneAttachment );

	DDX_Control(pDX, IDC_HINGE_INDEX, m_hingeIndex);
}


BEGIN_MESSAGE_MAP(CAttachmentsDlg, CDialog)
	ON_BN_CLICKED(IDC_BUTTON_NEW, OnBnClicked_NEW)
	ON_BN_CLICKED(IDC_BUTTON_RENAME, OnBnClicked_RENAME)
	ON_BN_CLICKED(IDC_BUTTON_REMOVE, OnBnClicked_REMOVE)
	ON_BN_CLICKED(IDC_BUTTON_IMPORT, OnBnClicked_IMPORT)
	ON_BN_CLICKED(IDC_BUTTON_EXPORT, OnBnClicked_EXPORT)

	ON_LBN_SELCHANGE(IDC_ATTACHMENTS,OnAttachmentSelect)

	ON_BN_CLICKED(IDC_BUTTON_FACEATTACH, OnClicked_FaceAttach)
	ON_BN_CLICKED(IDC_BUTTON_BONEATTACH, OnClicked_BoneAttach)
	ON_BN_CLICKED(IDC_BUTTON_SKINATTACH, OnClicked_SkinAttach)

	ON_BN_CLICKED(IDC_BROWSE_OBJECT, OnBnClicked_FILEBROWSE)
	ON_BN_CLICKED(IDC_BROWSE_MATERIAL, OnBnClicked_MATERIALBROWSE)
	ON_BN_CLICKED(IDC_DEFAULT_MATERIAL, OnBnClicked_DFLTMATERIAL)
	ON_BN_CLICKED(IDC_BUTTON_CLEAR, OnBnClicked_CLEAR)
	ON_BN_CLICKED(IDC_BUTTON_APPLY, OnBnClicked_APPLY)
	ON_BN_CLICKED(IDC_BUTTON_HIDEATTACH, OnClicked_HideAttachment)
	ON_BN_CLICKED(IDC_BUTTON_ALIGNBONEATTACHMENT, OnClicked_AlignBoneAttachment)
	ON_BN_CLICKED(IDC_BUTTON_PHYSATTACH, OnClicked_PhysAttachment)

	ON_CBN_SELCHANGE(IDC_BONE, OnBoneSelect)

	ON_CBN_SELCHANGE(IDC_HINGE_INDEX, OnHingeSelect)
	ON_EN_CHANGE(IDC_HINGE_LIMIT, OnLimitOrDampingChange)
	ON_EN_CHANGE(IDC_HINGE_DAMPING, OnLimitOrDampingChange)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CAttachmentsDlg::ClearBones()
{
	//m_boneName.ResetContent();
	m_pBoneComboBoxManager->Clear();
}

//////////////////////////////////////////////////////////////////////////
void CAttachmentsDlg::AddBone( const CString &bone )
{
	//m_boneName.AddString( bone );
	m_pBoneComboBoxManager->AddBone(bone);
}

//////////////////////////////////////////////////////////////////////////
void CAttachmentsDlg::SelectBone( const CString &bone )
{
	//m_boneName.SelectString( -1,bone );
	m_pBoneComboBoxManager->SelectBone(bone);
}

CString CAttachmentsDlg::GetBonenameFromWindow()
{
	//CString bonename;
	//m_boneName.GetWindowText( bonename );
	//return bonename; 
	return m_pBoneComboBoxManager->GetSelectedBone();
}

//////////////////////////////////////////////////////////////////////////
void CAttachmentsDlg::ReloadAttachment()
{
	m_attachmentsList.ResetContent();

	ICharacterInstance *pCharacter = m_pModelViewportCE->GetCharacterBase();
	if (!pCharacter)
		return;

	int num = pCharacter->GetIAttachmentManager()->GetAttachmentCount();
	for (int i = 0; i < num; i++)
	{
		IAttachment *pAttachment = pCharacter->GetIAttachmentManager()->GetInterfaceByIndex(i);
		m_attachmentsList.AddString( pAttachment->GetName() ) ;
	}
	m_attachmentsList.SetCurSel( 0 );

	if (num) { 
		m_ButtonEXPORT.EnableWindow(TRUE); 
	}
	else { 
		m_ButtonEXPORT.EnableWindow(FALSE); 
	}

}


void CAttachmentsDlg::UpdateList() {

	ICharacterInstance* pCharacter = m_pModelViewportCE->GetCharacterBase();

	if (pCharacter) {
		IAttachmentManager* pIAttachmentManager = pCharacter->GetIAttachmentManager();
		if (pCharacter) 
		{
			//update selection window
			m_attachmentsList.ResetContent();
			int anum = pIAttachmentManager->GetAttachmentCount();
			if (anum) 
			{
				CString aname; 
				for (int i = 0; i < anum; i++)
				{
					IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByIndex(i);
					aname = pIAttachment->GetName();
					m_attachmentsList.AddString( aname );
				}
				IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByIndex(0);
				aname = pIAttachment->GetName();
				int32 idx = m_attachmentsList.FindString(-1, aname);
				m_attachmentsList.SetCurSel( idx );
			}
			if (anum) { 
				m_ButtonRENAME.EnableWindow(TRUE); 
				m_ButtonREMOVE.EnableWindow(TRUE); 
				m_ButtonEXPORT.EnableWindow(TRUE); 
			}	else {  
				m_ButtonRENAME.EnableWindow(FALSE); 
				m_ButtonREMOVE.EnableWindow(FALSE); 
				m_ButtonEXPORT.EnableWindow(FALSE); 
			}
		}
	}
}



void CAttachmentsDlg::OnBnClicked_NEW()
{
	CharacterChanged=1;

	//	CString relFileName;
	CStringDlg dlg( _T( "Enter Attachment Name" ),this );
	dlg.SetString( "Default" );

	if (dlg.DoModal() == IDOK)
	{
		CString name = dlg.GetString();

		ICharacterInstance *pCharacter = m_pModelViewportCE->GetCharacterBase();
		if (!pCharacter)
			return;

		IAttachmentManager* pIAttachmentManager = pCharacter->GetIAttachmentManager();

		IAttachment* pAttachment = pIAttachmentManager->GetInterfaceByName(name);
		if (pAttachment) return;  //if name exists, don't do anything

		if (m_AttachmentType==CA_BONE) 
		{
			//CString bonename;
			//m_boneName.GetWindowText( bonename );
			CString bonename = m_pBoneComboBoxManager->GetSelectedBone();
			pIAttachmentManager->CreateAttachment( name,CA_BONE,bonename );
			uint32 num = pIAttachmentManager->GetAttachmentCount();
			if (num==1) 
			{
				IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByIndex(0);
				m_pModelViewportCE->m_ArcBall.DragRotation.SetIdentity();
				m_pModelViewportCE->m_ArcBall.ObjectRotation	=	pIAttachment->GetAttAbsoluteDefault().q;
				m_pModelViewportCE->m_ArcBall.sphere.center		= pIAttachment->GetAttAbsoluteDefault().t; 
			}
		}
		if (m_AttachmentType==CA_FACE)
		{
			pIAttachmentManager->CreateAttachment( name,CA_FACE );
		}
		if (m_AttachmentType==CA_SKIN)
		{
			pIAttachmentManager->CreateAttachment( name,CA_SKIN );
		}

		//------------------------------------------------------------------

		m_attachmentsList.ResetContent();

		int num = pIAttachmentManager->GetAttachmentCount();
		for (int i = 0; i < num; i++)
		{
			IAttachment *pAttachment = pIAttachmentManager->GetInterfaceByIndex(i);
			m_attachmentsList.AddString( pAttachment->GetName() ) ;
		}
		int32 idx = m_attachmentsList.FindString(-1, name);
		m_attachmentsList.SetCurSel( idx );
		
		m_ButtonRENAME.EnableWindow(TRUE);
		m_ButtonREMOVE.EnableWindow(TRUE);
		m_ButtonEXPORT.EnableWindow(TRUE);
	}
}



void CAttachmentsDlg::OnBnClicked_RENAME()
{
	CharacterChanged=1;

	int nSel = m_attachmentsList.GetCurSel();
	if (nSel == LB_ERR)
		return;

	CString oldname;
	m_attachmentsList.GetText(nSel,oldname);

	CString relFileName;
	CStringDlg dlg( _T( "Enter New Name" ),this );
	dlg.SetString( oldname );

	if (dlg.DoModal() == IDOK)
	{
		CString newname = dlg.GetString();

		ICharacterInstance *pCharacter = m_pModelViewportCE->GetCharacterBase();
		if (!pCharacter)
			return;

		IAttachmentManager* pIAttachmentManager = pCharacter->GetIAttachmentManager();

		IAttachment* pAttachment = pIAttachmentManager->GetInterfaceByName(newname);
		if (pAttachment) return; //if name exists, don't do anything

		m_attachmentsList.DeleteString(nSel);

		pAttachment = pIAttachmentManager->GetInterfaceByName(oldname);
		assert(pAttachment);
		pAttachment->ReName( newname );

		//------------------------------------------------------------------

		m_attachmentsList.ResetContent();

		int num = pIAttachmentManager->GetAttachmentCount();
		for (int i = 0; i < num; i++)
		{
			IAttachment *pAttachment = pIAttachmentManager->GetInterfaceByIndex(i);
			m_attachmentsList.AddString( pAttachment->GetName() ) ;
		}
		int32 idx = m_attachmentsList.FindString(-1, newname);
		m_attachmentsList.SetCurSel( idx );
	}


}


void CAttachmentsDlg::OnBnClicked_REMOVE()
{
	CharacterChanged=1;

	int nSel = m_attachmentsList.GetCurSel();
	if (nSel == LB_ERR)
		return;

	CString name;
	m_attachmentsList.GetText(nSel,name);

	ICharacterInstance *pCharacter = m_pModelViewportCE->GetCharacterBase();
	if (pCharacter==0)
		return;

	m_pModelViewportCE->m_SelectedAttachment=0;

	IAttachmentManager* pIAttachmentManager = pCharacter->GetIAttachmentManager();
	IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByName(name);
	uint32 type = pIAttachment->GetType();
	int32 result = pIAttachmentManager->RemoveAttachmentByInterface(pIAttachment);
//	int32 result = pIAttachmentManager->RemoveAttachmentByName(name);
	assert(result);
	m_attachmentsList.DeleteString(nSel);
	
	if (type==CA_SKIN) {
		pIAttachmentManager->ProjectAllAttachment();
	}

	//initialize selection
	uint32 numAttachment = pIAttachmentManager->GetAttachmentCount();
	if (numAttachment) 
	{ 
		m_ButtonEXPORT.EnableWindow(TRUE); 
		m_ButtonRENAME.EnableWindow(FALSE);
		m_ButtonREMOVE.EnableWindow(FALSE);

		IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByIndex(m_pModelViewportCE->m_SelectedAttachment);  
		uint32 type = pIAttachment->GetType();
		if (type==CA_BONE || type==CA_FACE) 
		{
			m_pModelViewportCE->m_ArcBall.DragRotation.SetIdentity();
			m_pModelViewportCE->m_ArcBall.ObjectRotation	=	pIAttachment->GetAttAbsoluteDefault().q;
			m_pModelViewportCE->m_ArcBall.sphere.center		= pIAttachment->GetAttAbsoluteDefault().t; 
		}
		m_pModelViewportCE->m_pAttachmentsDlg->UpdateList();
		string name = pIAttachment->GetName();
		uint32 n = m_pModelViewportCE->m_pAttachmentsDlg->m_attachmentsList.FindString(-1,name);
		m_pModelViewportCE->m_pAttachmentsDlg->m_attachmentsList.SetCurSel(n);
		m_pModelViewportCE->m_pAttachmentsDlg->OnAttachmentSelect();
		m_pModelViewportCE->m_pAttachmentsDlg->CharacterChanged=1;
	} 
	else 
	{ 
		m_ButtonRENAME.EnableWindow(FALSE);
		m_ButtonREMOVE.EnableWindow(FALSE);
		m_ButtonEXPORT.EnableWindow(FALSE); 
	}


}




void CAttachmentsDlg::OnBnClicked_IMPORT()
{

	char szFilters[] = "Attachment List Files|*.atl; | Attachment List Files (*.atl)|*.atl | All files (*.*)|*.*| |";
	CAutoDirectoryRestoreFileDialog dlg(TRUE, NULL, NULL, OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR, szFilters);

	if (dlg.DoModal() == IDOK) 
	{
		char ext[_MAX_EXT];
		_splitpath( dlg.GetPathName(),NULL,NULL,NULL,ext );
		if (stricmp(ext,".atl") == 0)
		{
			CLogFile::WriteLine("Importing Attachment List...");
			CString ATL_FileName = dlg.GetPathName();
			ICharacterInstance* pCharacter = m_pModelViewportCE->GetCharacterBase();
			if (pCharacter) {
				IAttachmentManager* pIAttachmentManager = pCharacter->GetIAttachmentManager();
				pIAttachmentManager->LoadAttachmentList( ATL_FileName );

				//initialize selection
				uint32 numAttachment = pIAttachmentManager->GetAttachmentCount();
				if (numAttachment) { 
					m_ButtonEXPORT.EnableWindow(TRUE); 
					m_ButtonRENAME.EnableWindow(FALSE);
					m_ButtonREMOVE.EnableWindow(FALSE);

					IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByIndex(0);  
					uint32 type = pIAttachment->GetType();
					if (type==CA_BONE || type==CA_FACE) 
					{
						m_pModelViewportCE->m_ArcBall.DragRotation.SetIdentity();
						m_pModelViewportCE->m_ArcBall.ObjectRotation	=	pIAttachment->GetAttAbsoluteDefault().q;
						m_pModelViewportCE->m_ArcBall.sphere.center		= pIAttachment->GetAttAbsoluteDefault().t; 
					}
					m_pModelViewportCE->m_pAttachmentsDlg->UpdateList();
					string name = pIAttachment->GetName();
					uint32 n = m_pModelViewportCE->m_pAttachmentsDlg->m_attachmentsList.FindString(-1,name);
					m_pModelViewportCE->m_pAttachmentsDlg->m_attachmentsList.SetCurSel(n);
					m_pModelViewportCE->m_pAttachmentsDlg->OnAttachmentSelect();
					m_pModelViewportCE->m_pAttachmentsDlg->CharacterChanged=1;
				} 
				else 
				{ 
					m_ButtonRENAME.EnableWindow(FALSE);
					m_ButtonREMOVE.EnableWindow(FALSE);
					m_ButtonEXPORT.EnableWindow(FALSE); 
				}
			}
		}
		BeginWaitCursor();
	}

	//-------------------------------------------------------------------------------
	CharacterChanged=1;
	UpdateList();
}

void CAttachmentsDlg::OnBnClicked_EXPORT()
{
	char szFilters[] = "Attachment List Files (*.atl)|*.atl| ";
	CAutoDirectoryRestoreFileDialog dlg(FALSE, "atl", NULL, OFN_OVERWRITEPROMPT|OFN_NOCHANGEDIR, szFilters);

	// Show the dialog
	if (dlg.DoModal() == IDOK) 
	{
		BeginWaitCursor();
		char ext[_MAX_EXT];
		_splitpath( dlg.GetPathName(),NULL,NULL,NULL,ext );
		if (stricmp(ext,".atl") == 0)
		{
			CLogFile::WriteLine("Exporting Attachment List ...");
			CString ATL_FileName = dlg.GetPathName();
			ICharacterInstance* pCharacter = m_pModelViewportCE->GetCharacterBase();
			if (pCharacter) {
				pCharacter->GetIAttachmentManager()->SaveAttachmentList( ATL_FileName );
			}
			//m_tabAttachments.CharacterChanged=0;
		}
		EndWaitCursor();
	}
}



IAttachment* CAttachmentsDlg::GetSelected()
{
	CharacterChanged=1;

	m_ButtonRENAME.EnableWindow(FALSE);
  m_ButtonREMOVE.EnableWindow(FALSE);

	int nSel = m_attachmentsList.GetCurSel();
	if (nSel == LB_ERR)
		return 0;

	m_ButtonRENAME.EnableWindow(TRUE);
	m_ButtonREMOVE.EnableWindow(TRUE);

	CString name;
	m_attachmentsList.GetText(nSel,name);

	ICharacterInstance *pCharacter = m_pModelViewportCE->GetCharacterBase();
	if (!pCharacter)
		return 0;

	IAttachmentManager* pAttachmentManager = pCharacter->GetIAttachmentManager();
	IAttachment* pAttachment = pAttachmentManager->GetInterfaceByName(name);
	assert(pAttachment);

	if (pAttachment->GetIAttachmentObject()==0) m_ButtonCLEAR.EnableWindow(FALSE);
	if (pAttachment->GetIAttachmentObject()!=0) m_ButtonCLEAR.EnableWindow(TRUE);

	m_pModelViewportCE->m_SelectedAttachment			=	pAttachmentManager->GetIndexByName(name);
	m_pModelViewportCE->m_ArcBall.DragRotation.SetIdentity();
	m_pModelViewportCE->m_ArcBall.ObjectRotation	=	pAttachment->GetAttAbsoluteDefault().q;
	m_pModelViewportCE->m_ArcBall.sphere.center		= pAttachment->GetAttAbsoluteDefault().t; 
	return pAttachment;
}

void CAttachmentsDlg::OnAttachmentSelect()
{
	CharacterChanged=1;

	ICharacterInstance* pCharacter = m_pModelViewportCE->m_pCharacterBase;
	assert(pCharacter);
	IAttachment* pIAttachment = GetSelected();

	if (pIAttachment==0) 
		return;

	CString ObjectFilePath;

	CCustomButton* pButton=(CCustomButton*)GetDlgItem(IDC_BUTTON_HIDEATTACH);
	pButton->SetCheck(pIAttachment->IsAttachmentHidden());

	pButton=(CCustomButton*)GetDlgItem(IDC_BUTTON_PHYSATTACH);
	pButton->SetCheck((pIAttachment->GetFlags() & FLAGS_ATTACH_PHYSICALIZED)!=0);

	if (pIAttachment->GetType() == CA_BONE)	
	{
		m_AttachmentType = CA_BONE;
		m_boneName.EnableWindow(TRUE);

		IAttachmentObject* pIAttachmentObject = pIAttachment->GetIAttachmentObject();
		if (pIAttachmentObject)
		{
			ICharacterInstance* pICharInstance = pIAttachmentObject->GetICharacterInstance();
			if (pICharInstance)
			{
				ObjectFilePath = pICharInstance->GetFilePath();
				m_pModelViewportCE->m_pCharacterAnim = pICharInstance;
				m_pModelViewportCE->UpdateAnimationList();
			}

			IStatObj* pIStaticObject = pIAttachmentObject->GetIStatObj();
			if (pIStaticObject)
				ObjectFilePath = pIStaticObject->GetFilePath();

			//set bone-name into the window
			uint32 BoneID = pIAttachment->GetBoneID();
			//int numBones = pCharacter->GetISkeleton()->GetJointCount();
			//const char *str = pCharacter->GetISkeleton()->GetJointNameByID(BoneID);
			//m_boneName.SetWindowText(str);
			m_pBoneComboBoxManager->SelectBone(BoneID);

		}
	}

	if (pIAttachment->GetType() == CA_FACE)	
	{
		m_AttachmentType = CA_FACE;
		m_boneName.EnableWindow(FALSE);
		IAttachmentObject* pIAttachmentObject = pIAttachment->GetIAttachmentObject();
		if (pIAttachmentObject)
		{
			ICharacterInstance* pICharInstance = pIAttachmentObject->GetICharacterInstance();
			if (pICharInstance)
			{
				ObjectFilePath = pICharInstance->GetFilePath();
				m_pModelViewportCE->m_pCharacterAnim = pICharInstance;
				m_pModelViewportCE->UpdateAnimationList();
			}

			IStatObj* pIStaticObject = pIAttachmentObject->GetIStatObj();
			if (pIStaticObject)
				ObjectFilePath = pIStaticObject->GetFilePath();
		}
	}

	if (pIAttachment->GetType() == CA_SKIN)	{
		m_AttachmentType = CA_SKIN;
		m_boneName.EnableWindow(FALSE);
		IAttachmentObject* pIAttachmentObject = pIAttachment->GetIAttachmentObject();
		if (pIAttachmentObject)
		{
				ICharacterInstance* pICharacter = pIAttachmentObject->GetICharacterInstance();
				if (pICharacter) 
				{
					ObjectFilePath = pICharacter->GetFilePath();
					m_pModelViewportCE->m_pCharacterAnim = pICharacter;
					m_pModelViewportCE->UpdateAnimationList();
				}
		}
	}

	m_objectName.SetWindowText(ObjectFilePath);

	// Display the current material in the edit control.
	IAttachmentObject* pIAttachmentObject = pIAttachment->GetIAttachmentObject();
	if (pIAttachmentObject)
	{
		CString sMaterialName;
		IMaterial* pMaterial = pIAttachmentObject->GetMaterialOverride();
		if (pMaterial != 0)
			sMaterialName = pMaterial->GetName();
		m_materialName.SetWindowText(sMaterialName);
	}

	int idx,i;
	float limit,damping;
	pIAttachment->GetHingeParams(idx,limit,damping);
	for(i=sizeof(g_hingeIdx)/sizeof(g_hingeIdx[0])-1;i>0 && g_hingeIdx[i]!=idx;i--);
	m_hingeIndex.SetCurSel(i);
	m_hingeLimit.SetValue(FtoI(limit));
	m_hingeDamping.SetValue(damping);
	m_hingeDamping.SetStep(0.5);
	m_hingeLimit.EnableWindow(i>0);
	m_hingeDamping.EnableWindow(i>0);

	UpdateData( FALSE );
}







//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------

void CAttachmentsDlg::OnClicked_BoneAttach()
{
	CharacterChanged=1;
	m_AttachmentType=CA_BONE;
	m_boneName.EnableWindow(TRUE);
}

void CAttachmentsDlg::OnClicked_FaceAttach()
{
	CharacterChanged=1;
	m_AttachmentType=CA_FACE;
	m_boneName.EnableWindow(FALSE);
}

void CAttachmentsDlg::OnClicked_SkinAttach()
{
	CharacterChanged=1;
	m_AttachmentType=CA_SKIN;
	m_boneName.EnableWindow(FALSE);
}


void CAttachmentsDlg::OnClicked_HideAttachment()
{
	CharacterChanged=1;

	CString relFileName;
	m_objectName.GetWindowText( relFileName );

	IAttachment* pIAttachment = GetSelected();	
	if (!pIAttachment)
		return;

	CCustomButton* pButton=(CCustomButton*)GetDlgItem(IDC_BUTTON_HIDEATTACH);
	int check = pButton->GetCheck();
	pIAttachment->HideAttachment(check);

}

void CAttachmentsDlg::OnClicked_AlignBoneAttachment()
{
	CharacterChanged=1;

	CString relFileName;
	m_objectName.GetWindowText( relFileName );

	IAttachment* pIAttachment = GetSelected();	
	if (!pIAttachment)
		return;

	CCustomButton* pButton=(CCustomButton*)GetDlgItem(IDC_BUTTON_ALIGNBONEATTACHMENT);
	int check = pButton->GetCheck();
	pIAttachment->AlignBoneAttachment(check);

}

void CAttachmentsDlg::OnClicked_PhysAttachment()
{
	CharacterChanged=1;
	IAttachment* pIAttachment = GetSelected();	
	if (!pIAttachment)
		return;

	CCustomButton* pButton=(CCustomButton*)GetDlgItem(IDC_BUTTON_PHYSATTACH);
	int check = pButton->GetCheck();
	pIAttachment->SetFlags(pIAttachment->GetFlags() & ~FLAGS_ATTACH_PHYSICALIZED | 
		FLAGS_ATTACH_PHYSICALIZED & -pButton->GetCheck()>>31);
}
















void CAttachmentsDlg::OnBnClicked_FILEBROWSE()
{
	CString relFileName;
	if (CFileUtil::SelectSingleFile( EFILE_TYPE_GEOMETRY,relFileName ))
	{
		m_objectName.SetWindowText( relFileName );
	}
}






void CAttachmentsDlg::OnBnClicked_CLEAR()
{
	CharacterChanged=1;

	IAttachment* pIAttachment = GetSelected();	
	if (!pIAttachment)
		return;
	pIAttachment->ClearBinding();	
	
	uint32 type = pIAttachment->GetType();
	if (type==CA_SKIN) {
		ICharacterInstance *pCharacter = m_pModelViewportCE->GetCharacterBase();
		if (!pCharacter)
			return;
		pCharacter->GetIAttachmentManager()->ProjectAllAttachment();
	}

	GetSelected();	
}

void CAttachmentsDlg::OnBnClicked_APPLY()
{
	CharacterChanged=1;

	ICharacterInstance *pCharacter = m_pModelViewportCE->GetCharacterBase();
	if (!pCharacter)
		return;

	IAttachmentManager* pIAttachmentManager = pCharacter->GetIAttachmentManager();

	CString relFileName;
	m_objectName.GetWindowText( relFileName );

	IAttachment* pIAttachment = GetSelected();	
	if (!pIAttachment)
		return;

	pIAttachment->ClearBinding();

	IAttachmentObject* pIAttachmentObject = 0;
	switch (m_AttachmentType)
	{
		case CA_BONE:
		{
			uint32 type = pIAttachment->GetType();
			if (type==CA_SKIN) 
				pIAttachmentManager->ProjectAllAttachment();

			string fileExt = Path::GetExt( relFileName );

			bool IsCDF = (0 == stricmp(fileExt,"cdf"));
			bool IsCHR = (0 == stricmp(fileExt,"chr"));
			bool IsCGA = (0 == stricmp(fileExt,"cga"));
			bool IsCGF = (0 == stricmp(fileExt,"cgf"));
			if (IsCDF || IsCHR || IsCGA) 
			{
				ICharacterInstance* pIChildCharacter = m_pModelViewportCE->GetAnimationSystem()->CreateInstance( relFileName  );
				if (pIChildCharacter) 
				{
					CCHRAttachment* pCharacterAttachment = new CCHRAttachment();
					pCharacterAttachment->m_pCharInstance  = pIChildCharacter;
					pIAttachmentObject = (IAttachmentObject*)pCharacterAttachment;
				}
			}
			if (IsCGF) 
			{
				IStatObj* pIStatObj = gEnv->p3DEngine->LoadStatObj( relFileName );
				if (pIStatObj) 
				{
					CCGFAttachment* pStatAttachment = new CCGFAttachment();
					pStatAttachment->pObj  = pIStatObj;
					pIAttachmentObject = (IAttachmentObject*)pStatAttachment;
				}
			}

			//CString bonename;
			//m_boneName.GetWindowText( bonename );
			CString bonename = m_pBoneComboBoxManager->GetSelectedBone();
			pIAttachment->SetType(CA_BONE,bonename);
			pIAttachment->AddBinding(pIAttachmentObject);
			GetSelected();	
		}
		break;

		case CA_FACE:
		{
			uint32 type = pIAttachment->GetType();
			if (type==CA_SKIN) 
				pIAttachmentManager->ProjectAllAttachment();

			string fileExt = Path::GetExt( relFileName );

			bool IsCHR = (0 == stricmp(fileExt,"chr"));
			bool IsCGF = (0 == stricmp(fileExt,"cgf"));
			bool IsCDF = (0 == stricmp(fileExt,"cdf"));
			if (IsCDF) 
			{
				ICharacterInstance* pIChildCharacter = m_pModelViewportCE->GetAnimationSystem()->CreateInstance(  relFileName  );
				if (pIChildCharacter) 
				{
					CCHRAttachment* pCharacterAttachment = new CCHRAttachment();
					pCharacterAttachment->m_pCharInstance  = pIChildCharacter;
					pIAttachmentObject = (IAttachmentObject*)pCharacterAttachment;
				}
			}
			if (IsCHR) 
			{
				ICharacterInstance* pIChildCharacter = m_pModelViewportCE->GetAnimationSystem()->CreateInstance( relFileName );
				if (pIChildCharacter) 
				{
					CCHRAttachment* pCharacterAttachment = new CCHRAttachment();
					pCharacterAttachment->m_pCharInstance  = pIChildCharacter;
					pIAttachmentObject = (IAttachmentObject*)pCharacterAttachment;
				}
			}
			if (IsCGF) 
			{
				IStatObj* pIStatObj = gEnv->p3DEngine->LoadStatObj( relFileName );
				if (pIStatObj) 
				{
					CCGFAttachment* pStatAttachment = new CCGFAttachment();
					pStatAttachment->pObj  = pIStatObj;
					pIAttachmentObject = (IAttachmentObject*)pStatAttachment;
				}
			}

			if (type!=CA_FACE) pIAttachment->SetType(CA_FACE);
			pIAttachment->AddBinding(pIAttachmentObject);
			GetSelected();	
		}
		break;

		case CA_SKIN:
		{
			uint32 type = pIAttachment->GetType();
		
			string fileExt = Path::GetExt( relFileName );

			bool IsCDF = (0 == stricmp(fileExt,"cdf"));
			bool IsCHR = (0 == stricmp(fileExt,"chr"));
			if (IsCDF) 
			{
				ICharacterInstance* pIChildCharacter = m_pModelViewportCE->GetAnimationSystem()->CreateInstance(  relFileName  );
				if (pIChildCharacter) 
				{
					CCHRAttachment* pCharacterAttachment = new CCHRAttachment();
					pCharacterAttachment->m_pCharInstance = pIChildCharacter;
					pIAttachmentObject = (IAttachmentObject*)pCharacterAttachment;
				}
			}
			if (IsCHR) 
			{
				ICharacterInstance* pIChildCharacter = m_pModelViewportCE->GetAnimationSystem()->CreateInstance( relFileName );
				if (pIChildCharacter) 
				{
					CCHRAttachment* pCharacterAttachment = new CCHRAttachment();
					pCharacterAttachment->m_pCharInstance  = pIChildCharacter;
					pIAttachmentObject = (IAttachmentObject*)pCharacterAttachment;
				}
			}
			if (type!=CA_SKIN) pIAttachment->SetType(CA_SKIN);
			pIAttachment->AddBinding(pIAttachmentObject);
			GetSelected();	

			pIAttachmentManager->ProjectAllAttachment();
		}
		break;
	}

	// If we created an attachment object, set the material.
	if (pIAttachmentObject != 0)
	{
		CString sMaterialName;
		m_materialName.GetWindowText(sMaterialName);
		_smart_ptr<CMaterial> pMaterial = GetIEditor()->GetMaterialManager()->LoadMaterial(sMaterialName, false);
		_smart_ptr<IMaterial> pIMaterial = 0;
		if (pMaterial)
			pIMaterial = pMaterial->GetMatInfo();
		pIAttachmentObject->SetMaterial(pIMaterial);
	}

	pIAttachment->SetHingeParams(g_hingeIdx[m_hingeIndex.GetCurSel()], m_hingeLimit.GetValue(), m_hingeDamping.GetValue());
}

void CAttachmentsDlg::OnBnClicked_MATERIALBROWSE()
{
	CMaterialSelectorDialog dlg;
	IAttachment* pIAttachment = GetSelected();
	if (GetSelected() && GetSelected()->GetIAttachmentObject() && GetSelected()->GetIAttachmentObject()->GetMaterial())
	{
		dlg.SetMaterialName(GetSelected()->GetIAttachmentObject()->GetMaterial()->GetName());
	}
	if (IDOK == dlg.DoModal())
	{
		m_materialName.SetWindowText(dlg.GetMaterialName());
	}
}

void CAttachmentsDlg::OnBnClicked_DFLTMATERIAL()
{
	m_materialName.SetWindowText("");
}

void CAttachmentsDlg::OnHingeSelect()
{
	CharacterChanged=1;

	BOOL bEnable = m_hingeIndex.GetCurSel()>0;
	m_hingeLimit.EnableWindow(bEnable);
	m_hingeDamping.EnableWindow(bEnable);
}
void CAttachmentsDlg::OnLimitOrDampingChange()
{
	CharacterChanged=1;
}

void CAttachmentsDlg::OnBoneSelect()
{
	m_pBoneComboBoxManager->SelectBone(m_boneName.GetCurSel());
}

CAttachmentsDlg::~CAttachmentsDlg()
{
	if (m_pBoneComboBoxManager)
		delete m_pBoneComboBoxManager;
}
