// TVSelectKeyDialog.cpp : implementation file
//

#include "stdafx.h"
#include "TVFaceSeqKeyDialog.h"
#include <ICryAnimation.h>
#include <IFacialAnimation.h>
#include "IFacialEditor.h"

// CTVFaceSeqKeyDialog dialog

IMPLEMENT_DYNAMIC(CTVFaceSeqKeyDialog, CDialog)
CTVFaceSeqKeyDialog::CTVFaceSeqKeyDialog(CWnd* pParent /*=NULL*/)
	: IKeyDlg(CTVFaceSeqKeyDialog::IDD, pParent)
{
	m_track = 0;
	m_node = 0;
	m_key = 0;
}

CTVFaceSeqKeyDialog::~CTVFaceSeqKeyDialog()
{
}

void CTVFaceSeqKeyDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_NAME, m_name);
}


BEGIN_MESSAGE_MAP(CTVFaceSeqKeyDialog, CDialog)
	ON_CBN_EDITCHANGE(IDC_NAME, OnUpdateValue)
	ON_CBN_SELCHANGE(IDC_NAME, OnCbnSelchangeName)
	ON_COMMAND(IDC_OPEN_IN_FE, OnOpenInFE)
END_MESSAGE_MAP()


// CTVFaceSeqKeyDialog message handlers

BOOL CTVFaceSeqKeyDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CTVFaceSeqKeyDialog::SetKey( IAnimNode *node,IAnimTrack *track,int nkey )
{
	assert( track );

	m_track = track;
	m_node = node;
	m_key = nkey;
	if (!m_track)
		return;

	int nParamId=node->FindTrack(track);
	assert(nParamId!=-1);

	if (track->GetType() != ATRACK_FACESEQ)
	return;

	m_name.ResetContent();

	// Find editor object who owns this node.
	IEntity *entity = node->GetEntity();
	if (entity)
	{
		// Add available animations.
		ICharacterInstance *pCharacter = entity->GetCharacter(0);
		if (pCharacter)
		{
			IAnimationSet* pAnimations = pCharacter->GetIAnimationSet();
			assert (pAnimations);

			m_name.AddString("");

			uint32 numAnims		= pAnimations->GetNumFacialAnimations();
			for (uint32 i=0; i < numAnims; i++)
				m_name.AddString(pAnimations->GetFacialAnimationName(i));
		}
	}

	IFaceSeqKey key;
	m_track->GetKey( m_key,&key );

	m_name.SelectString( -1,key.szSelection );
}

void CTVFaceSeqKeyDialog::OnUpdateValue()
{
	if (!m_track)
		return;
	//EAnimValue valueType = m_track->GetValueType();
	IFaceSeqKey key;
	m_track->GetKey( m_key,&key );

	CString sName;
	m_name.GetWindowText(sName);

	strncpy( key.szSelection,sName,sizeof(key.szSelection) );
	key.szSelection[sizeof(key.szSelection)-1] = '\0';

	IEntity* pEntity = (m_node ? m_node->GetEntity() : 0);
	ICharacterInstance* pCharacter = (pEntity ? pEntity->GetCharacter(0) : 0);
	IFacialInstance* pFacialInstance = (pCharacter ? pCharacter->GetFacialInstance() : 0);
	IFacialAnimSequence* pSequence = (pFacialInstance && key.szSelection ? pFacialInstance->LoadSequence(key.szSelection) : 0);
	if (pSequence)
		key.fDuration = pSequence->GetTimeRange().Length();

	m_track->SetKey( m_key,&key );
	GetIEditor()->GetAnimation()->ForceAnimation();
	RefreshTrackView();
}

void CTVFaceSeqKeyDialog::OnCbnSelchangeName()
{
	CString sStr;
	m_name.GetLBText(m_name.GetCurSel(), sStr);
	m_name.SetWindowText(sStr);
	OnUpdateValue();
}

void CTVFaceSeqKeyDialog::OnOpenInFE()
{
	// Open the facial editor if it is not already open.
	IEditor* pEditor = GetIEditor();
	IFacialEditor* pFacialEditor = (pEditor ? pEditor->GetFacialEditor(true) : 0);
	if (!pFacialEditor)
		CryMessageBox("Unable to open facial editor.", "Facial Editor Error", MB_ICONERROR | MB_OK);

	// Find the character from the node.
	string characterPath;
	string sequencePath;
	{
		IEntity* pEntity = (m_node ? m_node->GetEntity() : 0);
		ICharacterInstance* pCharacter = (pEntity ? pEntity->GetCharacter(0) : 0);
		{
			const char* characterPathBuffer = (pCharacter ? pCharacter->GetFilePath() : 0);
			characterPath = (characterPathBuffer ? characterPathBuffer : "");
		}

		// Find the sequence from the key.
		{
			IFaceSeqKey key;
			m_track->GetKey( m_key,&key );
			const char* sequenceName = key.szSelection;
			IAnimationSet* pAnimationSet = (pCharacter ? pCharacter->GetIAnimationSet() : 0);
			const char* sequencePathBuffer = (pAnimationSet && sequenceName ? pAnimationSet->GetFacialAnimationPathByName(sequenceName) : 0);
			sequencePath = (sequencePathBuffer ? sequencePathBuffer : "");
		}
	}

	bool foundCharacter = !characterPath.empty();
	bool foundSequence = !sequencePath.empty();

	if (!foundSequence)
	{
		string message = "Unable to find sequence path";
		if (!foundCharacter)
			message += " - character path could not be found";
		message += ".";

		CryMessageBox(message.c_str(), "Facial Editor Error", MB_ICONERROR | MB_OK);
	}

	if (pFacialEditor && !characterPath.empty())
		pFacialEditor->LoadCharacter(characterPath.c_str());
	if (pFacialEditor && !sequencePath.empty())
		pFacialEditor->LoadSequence(sequencePath.c_str());
}
