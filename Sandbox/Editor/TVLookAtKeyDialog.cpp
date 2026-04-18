// TVLookAtKeyDialog.cpp : implementation file
//

#include "stdafx.h"
#include "TVLookAtKeyDialog.h"
#include "ientitysystem.h"
#include "objects/objectmanager.h"
#include "objects/entity.h"
#include "objects/CameraObject.h"

// TVLookAtKeyDialog dialog

IMPLEMENT_DYNAMIC(CTVLookAtKeyDialog, CDialog)
CTVLookAtKeyDialog::CTVLookAtKeyDialog(CWnd* pParent /*=NULL*/)
	: IKeyDlg(CTVLookAtKeyDialog::IDD, pParent)
{
	m_track = 0;
	m_node = 0;
	m_key = 0;

	m_boneSetNamesMap[eLookAtKeyBoneSet_Eyes] = "Eyes";
	m_boneSetNamesMapReverse["Eyes"] = eLookAtKeyBoneSet_Eyes;
	m_boneSetNamesMap[eLookAtKeyBoneSet_HeadEyes] = "Head and eyes";
	m_boneSetNamesMapReverse["Head and eyes"] = eLookAtKeyBoneSet_HeadEyes;
	m_boneSetNamesMap[eLookAtKeyBoneSet_SpineHeadEyes] = "Spine, head and eyes";
	m_boneSetNamesMapReverse["Spine, head and eyes"] = eLookAtKeyBoneSet_SpineHeadEyes;
}

CTVLookAtKeyDialog::~CTVLookAtKeyDialog()
{
}

void CTVLookAtKeyDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_NAME, m_name);
	DDX_Control(pDX, IDC_BONESET, m_boneSet);
	DDX_Control(pDX, IDC_ID, m_id);
	DDX_Control(pDX, IDC_RELATIVE, m_allowAdditionalTransforms);
}


BEGIN_MESSAGE_MAP(CTVLookAtKeyDialog, CDialog)
	ON_CBN_EDITCHANGE(IDC_NAME, OnUpdateValue)
	ON_CBN_SELCHANGE(IDC_NAME, OnCbnSelchangeName)
	ON_CBN_EDITCHANGE(IDC_BONESET, OnUpdateBoneSet)
	ON_CBN_SELCHANGE(IDC_BONESET, OnCbnSelchangeBoneSet)
	ON_COMMAND(IDC_RELATIVE, OnChangeAllowAdditionalTransforms)
END_MESSAGE_MAP()


// CTVSelectKeyDialog message handlers

BOOL CTVLookAtKeyDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add the options for the bonesets to the combo box.
	m_boneSet.ResetContent();
	for (int i = 0; i < eLookAtKeyBoneSet_COUNT; ++i)
		m_boneSet.AddString(m_boneSetNamesMap[ELookAtKeyBoneSet(i)]);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CTVLookAtKeyDialog::SetKey( IAnimNode *node,IAnimTrack *track,int nkey )
{
	assert( track );

	m_track = track;
	m_node = node;
	m_key = nkey;
	if (!m_track)
		return;

	int nParamId=node->FindTrack(track);
	assert(nParamId!=-1);

	std::vector<CBaseObject*> objects;
	IMovieSystem *pMovieSystem;
	ISequenceIt *pSeqIt;
	IAnimSequence *pSeq;
	// fill list with available items
	m_name.ResetContent();

	// Get All entity nodes
	GetIEditor()->GetObjectManager()->GetObjects( objects );
	for (int i = 0; i < objects.size(); i++)
	{
		if (objects[i]->IsKindOf(RUNTIME_CLASS(CEntity)))
			m_name.AddString( objects[i]->GetName() );
	}
	
	ILookAtKey key;
	m_track->GetKey( m_key,&key );

	m_name.SetWindowText( key.szSelection);
	IAnimNode *camNode = node->GetMovieSystem()->FindNode(key.szSelection);
	if (camNode)
	{
		char sId[32];
		sprintf( sId,"%u",camNode->GetId() );
		m_id.SetWindowText( sId );
	}
	else
	{
		m_id.SetWindowText("0");
	}

	m_allowAdditionalTransforms.SetCheck(key.bAllowAdditionalTransforms);

	m_boneSet.SelectString(-1, m_boneSetNamesMap[key.boneSet]);
}

void CTVLookAtKeyDialog::OnUpdateValue()
{
	if (!m_track)
		return;
	ILookAtKey key;
	m_track->GetKey( m_key,&key );

	CString sName;
	m_name.GetWindowText(sName);
	m_id.SetWindowText("0");

	strncpy( key.szSelection,sName,sizeof(key.szSelection) );
	key.szSelection[sizeof(key.szSelection)-1] = '\0';

	if (sName.GetLength())
	{
		CBaseObject *pObj=GetIEditor()->GetObjectManager()->FindObject(sName);
		if (pObj && (pObj->GetType()==OBJTYPE_ENTITY))
		{
			if (pObj->GetAnimNode())
			{
				uint32 id = (uint32)pObj->GetAnimNode()->GetId();
				char sId[32];
				sprintf( sId,"%u",id );
				m_id.SetWindowText( sId );
			}
		}
	}

	m_track->SetKey( m_key,&key );
	GetIEditor()->GetAnimation()->ForceAnimation();
	RefreshTrackView();
}

void CTVLookAtKeyDialog::OnCbnSelchangeName()
{
	CString sStr;
	m_name.GetLBText(m_name.GetCurSel(), sStr);
	m_name.SetWindowText(sStr);
	OnUpdateValue();
}

void CTVLookAtKeyDialog::OnUpdateBoneSet()
{
	if (!m_track)
		return;
	ILookAtKey key;
	m_track->GetKey( m_key,&key );

	CString sBoneSet;
	m_boneSet.GetWindowText(sBoneSet);
	ELookAtKeyBoneSet boneSet = m_boneSetNamesMapReverse[sBoneSet];

	key.boneSet = boneSet;

	m_track->SetKey( m_key,&key );
	GetIEditor()->GetAnimation()->ForceAnimation();
	RefreshTrackView();
}

void CTVLookAtKeyDialog::OnCbnSelchangeBoneSet()
{
	CString sStr;
	m_boneSet.GetLBText(m_boneSet.GetCurSel(), sStr);
	m_boneSet.SetWindowText(sStr);
	OnUpdateBoneSet();
}

void CTVLookAtKeyDialog::OnChangeAllowAdditionalTransforms()
{
	bool checked = (m_allowAdditionalTransforms.GetCheck() != BST_UNCHECKED);

	if (!m_track)
		return;
	ILookAtKey key;
	m_track->GetKey( m_key,&key );

	key.bAllowAdditionalTransforms = checked;

	m_track->SetKey( m_key,&key );

	GetIEditor()->GetAnimation()->ForceAnimation();
	RefreshTrackView();
}
