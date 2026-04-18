// CharPanel_Preset.cpp : implementation file
//

#include "stdafx.h"
#include "Resource.h"
#include "AnimationInfoLoader.h"
#include <I3DEngine.h>
#include <ICryAnimation.h>
#include "ModelViewport.h"
#include "CryCharMorphParams.h"
#include "CharacterEditor.h"
#include "Material/MaterialManager.h"
#include "StringDlg.h"
#include "CharPanel_Preset.h"

/////////////////////////////////////////////////////////////////////////////
// CharPanel_Preset dialog
/////////////////////////////////////////////////////////////////////////////

CharPanel_Preset::CharPanel_Preset( CModelViewport *vp,CWnd* pParent ) : CDialog(CharPanel_Preset::IDD, pParent)
{
	m_ModelViewPort = vp;
	//m_tree_nodes.SetCanCapture(false);
	m_animLoader = new CAnimationInfoLoader;
	m_pAnimDesc = 0;
	m_pBonePreset = 0;
	m_fMinRot = 0.00000001f;
	m_fMaxRot = 0.000001f;
	m_fMinPos = 0.00000001f;
	m_fMaxPos = 0.000001f;

	Create(MAKEINTRESOURCE(IDD),pParent);
}

CharPanel_Preset::~CharPanel_Preset()
{
	if(m_animLoader)
		delete m_animLoader;
	m_animLoader = 0;
}



void CharPanel_Preset::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TREENODES, m_tree_nodes);
}


BEGIN_MESSAGE_MAP(CharPanel_Preset, CDialog)
	ON_WM_CREATE()
	ON_NOTIFY(TVN_SELCHANGED, IDC_TREENODES, OnTvnSelchangedTreeNodes)

	ON_CONTROL(WMU_FS_CHANGED, IDC_ROT_ERROR, OnSlider_RotError)
	ON_EN_CHANGE(IDC_ROT_ERROR_NUMBER, OnNumber_RotError)

	ON_CONTROL(WMU_FS_CHANGED, IDC_POS_ERROR, OnSlider_PosError)
	ON_EN_CHANGE(IDC_POS_ERROR_NUMBER, OnNumber_PosError)

	ON_BN_CLICKED(IDC_SAVEPRESET, OnBnClickedSavePreset)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CharPanel_Preset message handlers


BOOL CharPanel_Preset::OnInitDialog() 
{
	CDialog::OnInitDialog();

	m_animLoader->LoadDescription(PathUtil::GetGameFolder() + CBA_FILE);

	for (int i=0; i<m_animLoader->m_PresetFinder.m_Presets.size(); i++)
	{
		m_ModelViewPort->m_pCharPanel_Animation->SetPreset(m_animLoader->m_PresetFinder.m_Presets[i].m_Name.c_str());
	}

	m_RotError.SubclassDlgItem( IDC_ROT_ERROR,this );
	m_RotError.SetRangeFloat( 0,1 );
	m_RotError.SetValue(0);
	m_RotError.EnableWindow(TRUE);

	m_RotErrorNumber.Create( this,IDC_ROT_ERROR_NUMBER );
	m_RotErrorNumber.SetRange( 0,10000 );
	m_RotErrorNumber.SetInternalPrecision( 1 );
	m_RotErrorNumber.SetValue(0);
	m_RotErrorNumber.EnableWindow(TRUE);

	//--------------------------------------------------------------------

	m_PosError.SubclassDlgItem( IDC_POS_ERROR,this );
	m_PosError.SetRangeFloat( 0,1 );
	m_PosError.SetValue(0);
	m_PosError.EnableWindow(TRUE);

	m_PosErrorNumber.Create( this,IDC_POS_ERROR_NUMBER );
	m_PosErrorNumber.SetRange( 0,10000 );
	m_PosErrorNumber.SetInternalPrecision( 1 );
	m_PosErrorNumber.SetValue(0);
	m_PosErrorNumber.EnableWindow(TRUE);

	//--------------------------------------------------------------------

	
	EnableNodesError(false);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}


void CharPanel_Preset::FillChildrenNodes(HTREEITEM root, int16 idRoot)
{

	ICharacterInstance* pInstance = m_ModelViewPort->GetCharacterBase();
	if (pInstance==0)
		return;

	ISkeletonPose * pSkeletonPose = pInstance->GetISkeletonPose();

	uint32 numJoints=0;
	if (pSkeletonPose)
		numJoints=pSkeletonPose->GetJointCount();
  
	for(int i=0; i<numJoints; i++)
	{
		int16 id = pSkeletonPose->GetParentIDByID(i);
		if(idRoot==pSkeletonPose->GetParentIDByID(i))
		{
			const char* pRootName = pSkeletonPose->GetJointNameByID(i);
			uint32 contId = pSkeletonPose->GetJointCRC32(i);
			HTREEITEM hChildItem = m_tree_nodes.InsertItem( pRootName, root );
			m_tree_nodes.SetItemData(hChildItem, contId);
			FillChildrenNodes(hChildItem, i);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CharPanel_Preset::InitNodes()
{
	EnableNodesError(false);
	
	CString filename = m_ModelViewPort->m_pCharPanel_Animation->GetCurrFileName();
	string name;
	const char * ch = strrchr(filename, '\\');
	const char * ch1 = strrchr(filename, '/');
	if(ch1>ch)
		ch=ch1;
	if(ch)
		name = ch+1;
	else
		name = filename;

	m_animDef = m_animLoader->GetAnimationDefinitionFromModel(name);
	GetDlgItem(IDC_TREENODES)->EnableWindow(m_animDef!=0);

	m_tree_nodes.DeleteAllItems();
	FillChildrenNodes(TVI_ROOT, -1);
	UpdateBonePreset();
}


void CharPanel_Preset::EnableNodesError(bool bEnable)
{
	GetDlgItem(IDC_TREENODES)->EnableWindow(bEnable);
	GetDlgItem(IDC_SAVEPRESET)->EnableWindow(bEnable);
	GetDlgItem(IDC_ROT_ERROR)->EnableWindow(bEnable);
	GetDlgItem(IDC_ROT_ERROR_NUMBER)->EnableWindow(bEnable);
	GetDlgItem(IDC_POS_ERROR)->EnableWindow(bEnable);
	GetDlgItem(IDC_POS_ERROR_NUMBER)->EnableWindow(bEnable);
}


//////////////////////////////////////////////////////////////////////////
void CharPanel_Preset::OnTvnSelchangedTreeNodes(NMHDR *pNMHDR, LRESULT *pResult)
{
	UpdateBonePreset();
}

//////////////////////////////////////////////////////////////////////////
void CharPanel_Preset::UpdateBonePreset()
{
	EnableNodesError(false);
	CString strAnimName = m_ModelViewPort->m_pCharPanel_Animation->GetCurrAnimName();

	m_pAnimDesc = 0;
	m_pBonePreset = 0;
	bool bEnablePreset = false;
	
	GetDlgItem(IDC_TREENODES)->EnableWindow(strlen(strAnimName)!=0);
	if(strlen(strAnimName))
	{
		if(m_animDef)
		{
			string name = strAnimName;
			m_pAnimDesc = m_animDef->GetAnimationDesc(name);
			m_ModelViewPort->m_pCharPanel_Animation->SetPreset(m_pAnimDesc->m_Preset.m_Name.c_str());
			m_pBonePreset = &m_pAnimDesc->m_Preset;
			bEnablePreset = true;

			HTREEITEM hSelItem = m_tree_nodes.GetSelectedItem();
			if(hSelItem)
			{
				uint32 id = m_tree_nodes.GetItemData(hSelItem);
				BonePreset * pBonePreset = m_pAnimDesc->m_Preset.FindBonePreset(id);
				if(!pBonePreset)
				{
					BonePreset newPreset;
					newPreset.m_fPosError = m_pAnimDesc->GetMinPosError();
					newPreset.m_fRotError = m_pAnimDesc->GetMinRotError();
					m_pAnimDesc->m_Preset.m_BoneInfoMap[id] = newPreset;
					pBonePreset = m_pAnimDesc->m_Preset.FindBonePreset(id);
				}

				m_pCurBone = pBonePreset;

				m_fMinRot = m_pAnimDesc->GetMinRotError();
				m_fMaxRot = m_pAnimDesc->GetMaxRotError();
				m_fMinPos = m_pAnimDesc->GetMinPosError();
				m_fMaxPos = m_pAnimDesc->GetMaxPosError();

				float val;
				val = (m_pCurBone->m_fRotError - m_fMinRot)/(m_fMaxRot - m_fMinRot)*10000.0f;
				m_RotErrorNumber.SetValue(val);
				OnNumber_RotError();

				val = (m_pCurBone->m_fPosError - m_fMinPos)/(m_fMaxPos - m_fMinPos)*10000.0f;
				m_PosErrorNumber.SetValue(val);
				OnNumber_PosError();

				EnableNodesError(true);
			}
		}
	}
	m_ModelViewPort->m_pCharPanel_Animation->EnablePreset(bEnablePreset);
}

//////////////////////////////////////////////////////////////////////////
void CharPanel_Preset::OnBnClickedSavePreset()
{
	int nRes = MessageBox("Do you want to create a new preset?", "Saving preset...", MB_YESNOCANCEL);
	if(nRes==IDCANCEL)
		return;
	if(nRes==IDYES)
	{
		CStringDlg strDlg("New preset name");
		if(strDlg.DoModal()==IDCANCEL)
			return;
		CString newName = strDlg.GetString();
		m_pBonePreset->m_Name = newName;
	}
	m_animLoader->AddPreset(*m_pBonePreset);
	m_animDef->AddAnimationPreset(m_pBonePreset, m_ModelViewPort->m_pCharPanel_Animation->GetCurrAnimName());

	SaveCBAFile();
	//m_animLoader->SaveDescription("e:\\test.cba");
	
	UpdateBonePreset();
}

void CharPanel_Preset::UpdateCurBoneRot(float val)
{
	if(!m_pCurBone)
		return;
	double k = (double)val/10000.0;
	m_pCurBone->m_fRotError = float((double)m_fMinRot-k*m_fMinRot + k*m_fMaxRot); // do not change this form of linear interpolation
}

void CharPanel_Preset::UpdateCurBonePos(float val)
{
	if(!m_pCurBone)
		return;
	double k = (double)val/10000.0;
	m_pCurBone->m_fPosError = float((double)m_fMinPos-k*m_fMinPos + k*m_fMaxPos); // do not change this form of linear interpolation
}

void CharPanel_Preset::OnSlider_RotError()
{
	float val = m_RotError.GetValue();
	val = (1.0f/(1.005f-val) - 1.0f/1.005f) * 50;
	if(val<0.00001f)
		val = 0.0f;
	if(val>9900.0f)
		val = 10000.0f;
	m_RotErrorNumber.SetValue( val );
	UpdateCurBoneRot(val);
}

void CharPanel_Preset::OnNumber_RotError()
{
	float val = m_RotErrorNumber.GetValue();
	UpdateCurBoneRot(val);
	val = 1.005f - 1.0f/(val/50.0f + 1.0f/1.005f);
	m_RotError.SetValue( val );
}

void CharPanel_Preset::OnSlider_PosError()
{
	float val = m_PosError.GetValue();
	val = (1.0f/(1.005f-val) - 1.0f/1.005f) * 50;
	if(val<0.00001f)
		val = 0.0f;
	if(val>9900.0f)
		val = 10000.0f;
	m_PosErrorNumber.SetValue( val );
	UpdateCurBonePos(val);
}

void CharPanel_Preset::OnNumber_PosError()
{
	float val = m_PosErrorNumber.GetValue();
	UpdateCurBonePos(val);
	val = 1.005f - 1.0f/(val/50.0f + 1.0f/1.005f);
	m_PosError.SetValue( val );
}

void CharPanel_Preset::ChangeCurPreset(const char * pName)
{
	if(m_pBonePreset)
	{
		m_pAnimDesc->m_Preset.m_Name = pName;
		m_animDef->AddAnimationPreset(m_pBonePreset, m_ModelViewPort->m_pCharPanel_Animation->GetCurrAnimName());
	}
}

void CharPanel_Preset::SaveCBAFile()
{
	m_animLoader->SaveDescription(PathUtil::GetGameFolder() + CBA_FILE);
}
