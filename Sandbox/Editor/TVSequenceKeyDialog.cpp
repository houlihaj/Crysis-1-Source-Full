#include "stdafx.h"
#include "TVSequenceKeyDialog.h"

IMPLEMENT_DYNAMIC(CTVSequenceKeyDialog,CDialog)
CTVSequenceKeyDialog::CTVSequenceKeyDialog(CWnd* pParent)	: IKeyDlg(CTVSequenceKeyDialog::IDD,pParent)
{
	m_track = 0;
	m_node = 0;
	m_key = 0;
}

CTVSequenceKeyDialog::~CTVSequenceKeyDialog()
{
}

void CTVSequenceKeyDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX,IDC_NAME,m_name);
	DDX_Control(pDX,IDC_ID,m_id);
	DDX_Control(pDX,IDC_OVERRIDE,m_overrideTimes);
}

BEGIN_MESSAGE_MAP(CTVSequenceKeyDialog,CDialog)
	ON_CBN_EDITCHANGE(IDC_NAME,OnUpdateValue)
	ON_CBN_SELCHANGE(IDC_NAME,OnCbnSelchangeName)
	ON_BN_CLICKED(IDC_OVERRIDE,OnBnClickedOverride)
	ON_EN_UPDATE(IDC_STARTTIME,OnUpdateTimeValue)
	ON_EN_UPDATE(IDC_ENDTIME,OnUpdateTimeValue)
END_MESSAGE_MAP()

BOOL CTVSequenceKeyDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_startTime.Create(this,IDC_STARTTIME);
	m_endTime.Create(this,IDC_ENDTIME);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CTVSequenceKeyDialog::SetKey(IAnimNode *node,IAnimTrack *track,int nkey)
{
	assert(track);

	m_track = track;
	m_node = node;
	m_key = nkey;
	if (!m_track)
		return;

	int nParamId = node->FindTrack(track);
	assert(nParamId != -1);

	std::vector<CBaseObject*> objects;
	IMovieSystem *pMovieSystem;
	ISequenceIt *pSeqIt;
	IAnimSequence *pSeq;

	// fill list with available items
	m_name.ResetContent();
	pMovieSystem = GetIEditor()->GetSystem()->GetIMovieSystem();
	pSeqIt = pMovieSystem->GetSequences();
	pSeq = pSeqIt->first();
	while (pSeq)
	{
		if (pSeq != GetIEditor()->GetAnimation()->GetSequence())
			m_name.AddString(pSeq->GetName());
		pSeq = pSeqIt->next();
	}
	pSeqIt->Release();
	
	ISequenceKey key;
	m_track->GetKey(m_key,&key);

	m_name.SetWindowText(key.szSelection);
	IAnimNode *camNode = node->GetMovieSystem()->FindNode(key.szSelection);
	if (camNode)
	{
		char sId[32];
		sprintf(sId,"%u",camNode->GetId());
		m_id.SetWindowText(sId);
	}
	else
	{
		m_id.SetWindowText("0");
	}

	if (!key.bOverrideTimes)
	{
		IAnimSequence *pSequence = GetIEditor()->GetSystem()->GetIMovieSystem()->FindSequence(key.szSelection);
		if (pSequence)
		{
			key.fStartTime = pSequence->GetTimeRange().start;
			key.fEndTime = pSequence->GetTimeRange().end;
		}
		else
		{
			key.fStartTime = 0.0f;
			key.fEndTime = 0.0f;
		}
	}

	m_overrideTimes.SetCheck(key.bOverrideTimes ? BST_CHECKED : BST_UNCHECKED);
	m_startTime.SetValue(key.fStartTime);
	m_endTime.SetValue(key.fEndTime);

	EnableControls();
}

void CTVSequenceKeyDialog::OnUpdateValue()
{
	if (!m_track)
		return;

	ISequenceKey key;
	m_track->GetKey(m_key,&key);

	CString sName;
	m_name.GetWindowText(sName);
	m_id.SetWindowText("0");

	strncpy(key.szSelection,sName,sizeof(key.szSelection));
	key.szSelection[sizeof(key.szSelection)-1] = '\0';

	if (sName.GetLength())
	{
		CBaseObject *pObj = GetIEditor()->GetObjectManager()->FindObject(sName);
		if (pObj && (pObj->GetType() == OBJTYPE_ENTITY))
		{
			if (pObj->GetAnimNode())
			{
				uint32 id = (uint32)pObj->GetAnimNode()->GetId();
				char sId[32];
				sprintf( sId,"%u",id );
				m_id.SetWindowText(sId);
			}
		}
	}

	IAnimSequence *pSequence = GetIEditor()->GetSystem()->GetIMovieSystem()->FindSequence(key.szSelection);
	if (pSequence)
	{
		key.fDuration = pSequence->GetTimeRange().Length();
		key.fStartTime = pSequence->GetTimeRange().start;
		key.fEndTime = pSequence->GetTimeRange().end;
		key.bOverrideTimes = false;
	}
	else
	{
		key.fDuration = 0.0f;
		key.fStartTime = 0.0f;
		key.fEndTime = 0.0f;
		key.bOverrideTimes = false;
	}

	m_overrideTimes.SetCheck(key.bOverrideTimes ? BST_CHECKED : BST_UNCHECKED);
	m_startTime.SetValue(key.fStartTime);
	m_endTime.SetValue(key.fEndTime);

	EnableControls();

	m_track->SetKey(m_key,&key);
	GetIEditor()->GetAnimation()->ForceAnimation();
	RefreshTrackView();
}

void CTVSequenceKeyDialog::OnCbnSelchangeName()
{
	CString sStr;
	m_name.GetLBText(m_name.GetCurSel(),sStr);
	m_name.SetWindowText(sStr);
	OnUpdateValue();
}

void CTVSequenceKeyDialog::OnBnClickedOverride()
{
	EnableControls();
	OnUpdateTimeValue();
}

void CTVSequenceKeyDialog::OnUpdateTimeValue()
{
	if (!m_track)
		return;

	ISequenceKey key;
	m_track->GetKey(m_key,&key);

	key.bOverrideTimes = m_overrideTimes.GetCheck();

	IAnimSequence *pSequence = GetIEditor()->GetSystem()->GetIMovieSystem()->FindSequence(key.szSelection);

	if (!key.bOverrideTimes)
	{
		if (pSequence)
		{
			key.fStartTime = pSequence->GetTimeRange().start;
			key.fEndTime = pSequence->GetTimeRange().end;
		}
		else
		{
			key.fStartTime = 0.0f;
			key.fEndTime = 0.0f;
		}
	}
	else
	{
		key.fStartTime = m_startTime.GetValue();
		key.fEndTime = m_endTime.GetValue();
	}

	key.fDuration = key.fEndTime-key.fStartTime > 0 ? key.fEndTime-key.fStartTime : 0.0f;

	IMovieSystem *pMovieSystem = GetIEditor()->GetSystem()->GetIMovieSystem();

	if (pMovieSystem != NULL)
	{
		pMovieSystem->SetStartEndTime(pSequence,key.fStartTime,key.fEndTime);
	}

	m_track->SetKey(m_key,&key);
	GetIEditor()->GetAnimation()->ForceAnimation();
	RefreshTrackView();
}

void CTVSequenceKeyDialog::EnableControls()
{
	if (m_overrideTimes.GetCheck())
	{
		m_startTime.EnableWindow(true);
		m_endTime.EnableWindow(true);
	}
	else
	{
		m_startTime.EnableWindow(false);
		m_endTime.EnableWindow(false);
	}
}