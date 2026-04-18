// TVSoundKeyDialog.cpp : implementation file
//

#include "stdafx.h"
#include "TVExprKeyDialog.h"
#include <ICryAnimation.h>
#include <IEntitySystem.h>

CString CTVExprKeyDialog::m_sLastPath="";

// CTVExprKeyDialog dialog

IMPLEMENT_DYNAMIC(CTVExprKeyDialog, CDialog)
CTVExprKeyDialog::CTVExprKeyDialog(CWnd* pParent /*=NULL*/)
	: IKeyDlg(CTVExprKeyDialog::IDD, pParent)
{
	m_track = 0;
	m_node = 0;
	m_key = 0;
}

CTVExprKeyDialog::~CTVExprKeyDialog()
{
}

void CTVExprKeyDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_NAME, m_Name);
}

BEGIN_MESSAGE_MAP(CTVExprKeyDialog, CDialog)
	ON_EN_UPDATE(IDC_AMP, OnUpdateValue)
	ON_EN_UPDATE(IDC_BLENDIN, OnUpdateValue)
	ON_EN_UPDATE(IDC_HOLD, OnUpdateValue)
	ON_EN_UPDATE(IDC_BLENDOUT, OnUpdateValue)
	ON_CBN_SELCHANGE(IDC_NAME, OnUpdateValue)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CTVExprKeyDialog::AddMorphTargets( ICharacterInstance *pCharInst,const char *sCurrent )
{

	//	ICryCharModel *pModel=pCharInst->GetModel();
	//	if (pModel)
	//	{
	IAnimationSet *pAnimSet=pCharInst->GetIAnimationSet();
	if (pAnimSet)
	{
		int nMorphTargets=pAnimSet->numMorphTargets();
		for (int i=0;i<nMorphTargets;i++)
		{
			const char *pszName=pAnimSet->GetNameMorphTarget(i);
			int nId=m_Name.AddString(pszName);
			if (strcmp(pszName, sCurrent)==0)
				m_Name.SetCurSel(nId);
		}
	}
	//}

	// Add morph targets from character attachments.
	IAttachmentManager *pAttachments = pCharInst->GetIAttachmentManager();
	if (pAttachments)
	{
		for (int i = 0; i < pAttachments->GetAttachmentCount(); i++)
		{
			IAttachment *pAttachment = pAttachments->GetInterfaceByIndex(i);
			if (!pAttachment)
				continue;
			IAttachmentObject *pObj = pAttachment->GetIAttachmentObject();
			if (!pObj)
				continue;
			
			ICharacterInstance *pCharacter = pObj->GetICharacterInstance();
			if (!pCharacter)
				continue;

			AddMorphTargets( pCharacter,sCurrent );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTVExprKeyDialog::SetKey( IAnimNode *node,IAnimTrack *track,int nkey )
{
	assert( track );
	m_track = track;
	m_node = node;
	m_key = nkey;
	if (!m_track)
		return;
	IExprKey key;
	m_track->GetKey( m_key,&key );
	m_Name.ResetContent();
	if (m_node)
	{
		IEntity *pEntity=m_node->GetEntity();
		if (pEntity)
		{
			ICharacterInstance *pCharInst=pEntity->GetCharacter(0);
			if (pCharInst)
			{
				AddMorphTargets( pCharInst,key.pszName );
			}
		}
	}
	m_Amp.SetValue(key.fAmp);
	m_BlendIn.SetValue(key.fBlendIn);
	m_Hold.SetValue(key.fHold);
	m_BlendOut.SetValue(key.fBlendOut);
	OnUpdateValue();
}

// CTVSoundKeyDialog message handlers
BOOL CTVExprKeyDialog::OnInitDialog()
{
	IKeyDlg::OnInitDialog();

	m_Amp.Create(this, IDC_AMP);
	m_Amp.SetRange(0, 10);
	m_Amp.SetInteger(false);

	m_BlendIn.Create(this, IDC_BLENDIN);
	m_BlendIn.SetRange(0, 10);
	m_BlendIn.SetInteger(false);

	m_Hold.Create(this, IDC_HOLD);
	m_Hold.SetRange(0, 10);
	m_Hold.SetInteger(false);

	m_BlendOut.Create(this, IDC_BLENDOUT);
	m_BlendOut.SetRange(0, 10);
	m_BlendOut.SetInteger(false);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CTVExprKeyDialog::OnUpdateValue()
{
	IExprKey key;
	CString sName;
	m_track->GetKey( m_key,&key );
	int nCurSel=m_Name.GetCurSel();
	if (nCurSel>=0)
		m_Name.GetLBText(nCurSel, sName);
	else
		sName="";
	strncpy(key.pszName, sName, sizeof(key.pszName));
	key.pszName[sizeof(key.pszName)-1]=0;
	key.fAmp=m_Amp.GetValue();
	key.fBlendIn=m_BlendIn.GetValue();
	key.fHold=m_Hold.GetValue();
	key.fBlendOut=m_BlendOut.GetValue();
	m_track->SetKey(m_key, &key);
	GetIEditor()->GetAnimation()->ForceAnimation();

	RefreshTrackView();
}