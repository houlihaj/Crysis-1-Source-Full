////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   trackviewdialog.cpp
//  Version:     v1.00
//  Created:     24/4/2002 by Timur.
//  Compilers:   Visual C++ 7.0
//  Description: CTrackViewDialog Implementation file.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "TrackViewDialog.h"

#include "ViewPane.h"
#include "StringDlg.h"
#include "TVSequenceProps.h"
#include "ViewManager.h"
#include "AnimationContext.h"
#include "AnimationSerializer.h"

#include "Objects\ObjectManager.h"
#include "Objects\Entity.h"
#include "IViewPane.h"

#include <IMovieSystem.h>
#include <IEntitySystem.h>
// CTrackViewDialog dialog

class CTrackViewPaneClass : public TRefCountBase<IViewPaneClass>
{
	//////////////////////////////////////////////////////////////////////////
	// IClassDesc
	//////////////////////////////////////////////////////////////////////////
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_VIEWPANE; };
	virtual REFGUID ClassID()
	{
		// {D21C9FE5-22D3-41e3-B84B-A377AFA0A05C}
		static const GUID guid = 
		{ 0xd21c9fe5, 0x22d3, 0x41e3, { 0xb8, 0x4b, 0xa3, 0x77, 0xaf, 0xa0, 0xa0, 0x5c } };
		return guid;
	}
	virtual const char* ClassName() { return "Track View"; };
	virtual const char* Category() { return "Track View"; };
	//////////////////////////////////////////////////////////////////////////
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CTrackViewDialog); };
	virtual const char* GetPaneTitle() { return _T("Track View"); };
	virtual EDockingDirection GetDockingDirection() { return DOCK_BOTTOM; };
	virtual CRect GetPaneRect() { return CRect(0,0,500,300); };
	virtual bool SinglePane() { return true; };
	virtual bool WantIdleUpdate() { return true; };
};

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::RegisterViewClass()
{
	GetIEditor()->GetClassFactory()->RegisterClass( new CTrackViewPaneClass );
}

IMPLEMENT_DYNCREATE(CTrackViewDialog, CToolbarDialog)


//////////////////////////////////////////////////////////////////////////
CString CTrackViewDialog::m_currSequenceName;
CTrackViewDialog *g_pTrackViewDialog = NULL;

//////////////////////////////////////////////////////////////////////////
CTrackViewDialog::CTrackViewDialog(CWnd* pParent /*=NULL*/)
	: CToolbarDialog(CTrackViewDialog::IDD, pParent)
{
	g_pTrackViewDialog = this;
	m_currSequence = 0;
	m_bRecord = false;
	m_bAutoRecord = false;
	m_bPause = false;
	m_bPlay = false;
	m_bReloading=false;
	m_fLastTime=-1.0f;
	ZeroStruct(m_defaulCameraObjectId);
	m_movieSystem = 0;
	m_fAutoRecordStep = 0.5f;
	GetIEditor()->RegisterNotifyListener(this);
	m_bNeedReloadSequence = false;

	Create( IDD,pParent );
}

CTrackViewDialog::~CTrackViewDialog()
{
	g_pTrackViewDialog = 0;
	GetIEditor()->UnregisterNotifyListener(this);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::PostNcDestroy()
{
	delete this;
}

void CTrackViewDialog::DoDataExchange(CDataExchange* pDX)
{
	CToolbarDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CTrackViewDialog, CToolbarDialog)
//	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_CBN_SELENDOK( IDC_SEQUENCES,OnChangeSequence )
	ON_COMMAND( ID_TV_PREVKEY,OnGoToPrevKey )
	ON_COMMAND( ID_TV_NEXTKEY,OnGoToNextKey )
	ON_COMMAND( ID_TV_ADDKEY,OnAddKey )
	ON_COMMAND( ID_TV_DELKEY,OnDelKey )
	ON_COMMAND( ID_TV_MOVEKEY,OnMoveKey )
	ON_COMMAND( ID_TV_SLIDEKEY,OnSlideKey )
	ON_COMMAND( ID_TV_SCALEKEY,OnScaleKey )
	ON_COMMAND( ID_TV_ADD_SEQUENCE,OnAddSequence )
	ON_COMMAND( ID_TV_DEL_SEQUENCE,OnDelSequence )
	ON_COMMAND( ID_TV_EDIT_SEQUENCE,OnEditSequence )
	ON_COMMAND( ID_TV_RECORD,OnRecord )
	ON_COMMAND( ID_TV_RECORD_AUTO,OnAutoRecord )
	ON_COMMAND( ID_TV_JUMPSTART,OnGoToStart)
	ON_COMMAND( ID_TV_JUMPEND,OnGoToEnd)
	ON_COMMAND( ID_TV_PLAY,OnPlay )
	ON_COMMAND( ID_TV_STOP,OnStop )
	ON_COMMAND( ID_TV_PAUSE,OnPause )
	ON_COMMAND(	ID_PLAY_LOOP, OnLoop)
	ON_COMMAND( ID_TV_COPYPASTEKEYS,OnCopyPasteKeys)
	ON_COMMAND( ID_ADDNODE,OnAddSelectedNode )
	ON_COMMAND( ID_ADDSCENETRACK,OnAddSceneTrack )
	ON_UPDATE_COMMAND_UI(ID_TV_RECORD, OnUpdateRecord)
	ON_UPDATE_COMMAND_UI(ID_TV_RECORD_AUTO, OnUpdateRecordAuto)
	ON_UPDATE_COMMAND_UI(ID_TV_PLAY, OnUpdatePlay)
	ON_UPDATE_COMMAND_UI(ID_TV_PAUSE, OnUpdatePause)
	ON_UPDATE_COMMAND_UI(ID_PLAY_LOOP, OnUpdateLoop)
	ON_UPDATE_COMMAND_UI(ID_TV_COPYPASTEKEYS, OnUpdateCopyPasteKeys)
	ON_UPDATE_COMMAND_UI(ID_ADDNODE, OnAddSelectedNodeUpdate )
	ON_NOTIFY(TBN_DROPDOWN, AFX_IDW_TOOLBAR, OnToolbarDropDown)
	ON_WM_SETFOCUS()
	ON_WM_DESTROY()
//	ON_WM_PAINT()
END_MESSAGE_MAP()


// CTrackViewDialog message handlers
void CTrackViewDialog::OnSize(UINT nType, int cx, int cy)
{
	CToolbarDialog::OnSize(nType, cx, cy);

  if (m_wndSplitter.GetSafeHwnd())
	{
		CRect rc;
		GetClientRect(&rc);
    m_wndSplitter.MoveWindow(&rc,FALSE);
	}
	RecalcLayout();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnToolbarDropDown(NMHDR *pnhdr, LRESULT *plr)
{
	CRect rc;
	CPoint pos;
	GetCursorPos( &pos );
	NMTOOLBAR* pnmtb = (NMTOOLBAR*)pnhdr;
	m_cDlgToolBar.GetToolBarCtrl().GetRect( pnmtb->iItem,rc );
	ClientToScreen( rc );
	pos.x = rc.left;
	pos.y = rc.bottom;

	// Switch on button command id's.
	switch (pnmtb->iItem)
	{
		case ID_TV_PLAY:
			OnPlayMenu(pos);
			break;
		case ID_TV_RECORD_AUTO:
			OnRecordMenu(pos);
			break;
		default:
			return;
	}
	*plr = TBDDRET_DEFAULT;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnRecordMenu( CPoint pos )
{
	struct SScales
	{
		const char *pszText;
		float fScale;
	};
	SScales Scales[]={
		{" 1 sec ", 1.0f},
		{"1/2 sec", 1.0f/2},
		{"1/5 sec", 1.0f/5},
		{"1/10 sec", 1.0f/10},
		{"1/25 sec", 1.0f/25},
		{"1/50 sec", 1.0f/50},
		{"1/100 sec", 1.0f/100},
	};
	int nScales=sizeof(Scales)/sizeof(SScales);
	CMenu menu;
	menu.CreatePopupMenu();
	for (int i=0;i<nScales;i++)
	{
		int flags = 0;
		if (m_fAutoRecordStep == Scales[i].fScale)
			flags |= MF_CHECKED;
		menu.AppendMenu(MF_STRING|flags, i+1, Scales[i].pszText);
	}
	int cmd = menu.TrackPopupMenu( TPM_RETURNCMD|TPM_LEFTALIGN|TPM_LEFTBUTTON,pos.x,pos.y,this );
	if (cmd >= 1 && cmd <= nScales)
		m_fAutoRecordStep = Scales[cmd-1].fScale;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnPlayMenu( CPoint pos )
{
	struct SScales
	{
		const char *pszText;
		float fScale;
	};
	SScales Scales[]={
		{" 2 ", 2.0f},
		{" 1 ", 1.0f},
		{"1/2", 0.5f},
		{"1/4", 0.25f},
		{"1/8", 0.125f},
	};
	int nScales=sizeof(Scales)/sizeof(SScales);
	CMenu menu;
	menu.CreatePopupMenu();
	for (int i=0;i<nScales;i++)
	{
		menu.AppendMenu(MF_STRING, i+1, Scales[i].pszText);
	}
	//////////////////////////////////////////////////////////////////////////
	menu.AppendMenu(MF_SEPARATOR);
	//////////////////////////////////////////////////////////////////////////
	int flags = 0;
	if (GetIEditor()->GetAnimation()->IsPlayingToAVI())
		flags |= MF_CHECKED;
	menu.AppendMenu(MF_STRING|flags, 100, _T("Encode to AVI") );
	if (!m_aviFile.IsEmpty())
	{
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING|MF_GRAYED, 102, m_aviFile );
	}

	menu.AppendMenu(MF_SEPARATOR);


	bool bSequenceCamera=false;

	CViewport* pRendView = GetIEditor()->GetViewManager()->GetViewport(ET_ViewportCamera);
	if(pRendView)
		bSequenceCamera=pRendView->GetCameraObjectState();

	menu.AppendMenu(MF_STRING|((bSequenceCamera)?MF_CHECKED:MF_UNCHECKED), 200, _T("Sequence Camera") );

	int cmd = menu.TrackPopupMenu( TPM_RETURNCMD|TPM_LEFTALIGN|TPM_LEFTBUTTON,pos.x,pos.y,this );
	if (cmd >= 1 && cmd <= nScales && cmd < 100)
		GetIEditor()->GetAnimation()->SetTimeScale(Scales[cmd-1].fScale);

	switch (cmd)
	{
	case 100:
		{
			if (!GetIEditor()->GetAnimation()->IsPlayingToAVI())
			{
				if (CFileUtil::SelectSaveFile( "AVI Files (*.avi)|*.avi","avi","",m_aviFile ))
				{
					GetIEditor()->GetAnimation()->PlayToAVI( true,m_aviFile );
				}
			}
			else
			{
				GetIEditor()->GetAnimation()->PlayToAVI( false );
			}
		}
		break;
	case 101:
		{
			
		}
		break;
	case 200:
		{
			if(pRendView)
				pRendView->ToggleCameraObject();
		}
		break;

	}
}

BOOL CTrackViewDialog::OnInitDialog()
{
	CToolbarDialog::OnInitDialog();

	CRect rc;
	InitToolbar();

	// Resize the toolbar
	GetClientRect(rc);
	m_cDlgToolBar.SetWindowPos(NULL, 0, 0, rc.right, 70, SWP_NOZORDER);
	
	// TODO: Remove this if you don't want tool tips or a resizeable toolbar
	m_cDlgToolBar.SetBarStyle(m_cDlgToolBar.GetBarStyle()|CBRS_TOOLTIPS|CBRS_FLYBY);

	// Create splitter and track view controls.
	GetClientRect(rc);
	rc.right /= 2;
	m_wndNodes.Create( WS_VISIBLE|WS_CHILD| TVS_FULLROWSELECT|TVS_HASBUTTONS|TVS_HASLINES|TVS_SHOWSELALWAYS|TVS_LINESATROOT,rc,this,1 );
	
	// Create key tracks dialog.
	m_wndKeys.Create( NULL,NULL,WS_VISIBLE|WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS|WS_HSCROLL,rc,this,2 );
	m_wndKeys.SetTimeRange( 0,20 );
	m_wndKeys.SetTimeScale( 100 );
	m_wndKeys.SetCurrTime( 1 );
	m_wndKeys.SetTrackDialog( &m_wndTrackProps );

	// Create track graph dialog.
	//m_wndGraph.Create( NULL,NULL,WS_VISIBLE|WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS|WS_HSCROLL,rc,this,2 );
	//m_wndGraph.SetTimeRange( 0,20 );
	//m_wndGraph.SetTimeScale( 100 );
	//m_wndGraph.SetCurrTime( 1 );
	//m_wndGraph.SetTrackDialog( &m_wndTrackProps );


	m_wndSpline.Create( NULL,NULL,WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS|LBS_NOINTEGRALHEIGHT|LBS_OWNERDRAWFIXED|LBS_WANTKEYBOARDINPUT|LBS_DISABLENOSCROLL|WS_HSCROLL,rc,this,3 );
	m_wndSpline.SetTimeRange( 0,20 );
	m_wndSpline.SetTimeScale( 100 );
	m_wndSpline.SetCurrentTime( 1 );

	//m_wndNodes.SetKeyListCtrl( &m_wndGraph );
	m_wndNodes.SetKeyListCtrl( &m_wndKeys );


	m_wndNodes.SetItemHeight( 16 );
	
	GetClientRect(rc);
	m_wndSplitter.CreateStatic( this,1,2 );
	m_wndSplitter.SetPane( 0,0,&m_wndNodes, CSize(150, 100) );
	m_wndSplitter.SetPane( 0,1,&m_wndKeys, CSize(300, 100) );
	//m_wndSplitter.SetPane( 0,1,&m_wndGraph, CSize(300, 100) );
	m_wndSplitter.MoveWindow(&rc, TRUE);

	RecalcLayout();
	InitSequences();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

IMovieSystem* CTrackViewDialog::GetMovieSystem()
{
	if (m_movieSystem)
	{
		return m_movieSystem;
	}
	m_movieSystem = GetIEditor()->GetMovieSystem();

	return m_movieSystem;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::InitToolbar()
{
	// Create the toolbar
	m_cDlgToolBar.CreateEx(this, TBSTYLE_FLAT|TBSTYLE_WRAPABLE,
		WS_CHILD|WS_VISIBLE|CBRS_TOP|CBRS_TOOLTIPS|CBRS_FLYBY|CBRS_SIZE_DYNAMIC);
	m_cDlgToolBar.LoadToolBar24(IDR_TRACKVIEW);
	m_cDlgToolBar.GetToolBarCtrl().SetExtendedStyle(TBSTYLE_EX_DRAWDDARROWS);
	m_cDlgToolBar.SetButtonStyle( m_cDlgToolBar.CommandToIndex(ID_TV_ADDKEY),TBBS_CHECKGROUP );
	m_cDlgToolBar.SetButtonStyle( m_cDlgToolBar.CommandToIndex(ID_TV_MOVEKEY),TBBS_CHECKGROUP );
	m_cDlgToolBar.SetButtonStyle( m_cDlgToolBar.CommandToIndex(ID_TV_SLIDEKEY),TBBS_CHECKGROUP );
	m_cDlgToolBar.SetButtonStyle( m_cDlgToolBar.CommandToIndex(ID_TV_SCALEKEY),TBBS_CHECKGROUP );
	m_cDlgToolBar.SetButtonStyle( m_cDlgToolBar.CommandToIndex(ID_TV_RECORD),TBBS_CHECKBOX );
	m_cDlgToolBar.SetButtonStyle( m_cDlgToolBar.CommandToIndex(ID_TV_RECORD_AUTO),TBBS_CHECKBOX );
	m_cDlgToolBar.SetButtonStyle( m_cDlgToolBar.CommandToIndex(ID_TV_PLAY),TBBS_CHECKBOX );
	m_cDlgToolBar.SetButtonStyle( m_cDlgToolBar.CommandToIndex(ID_TV_PAUSE),TBBS_CHECKBOX );
	int nIndex=m_cDlgToolBar.CommandToIndex(ID_TV_PLAY);
	m_cDlgToolBar.SetButtonStyle(nIndex, m_cDlgToolBar.GetButtonStyle(nIndex) | TBSTYLE_DROPDOWN);
	nIndex=m_cDlgToolBar.CommandToIndex(ID_TV_RECORD_AUTO);
	m_cDlgToolBar.SetButtonStyle(nIndex, m_cDlgToolBar.GetButtonStyle(nIndex) | TBSTYLE_DROPDOWN);
	m_cDlgToolBar.CalcFixedLayout(0,TRUE);

	// Initially move operation is enabled.
	m_cDlgToolBar.GetToolBarCtrl().CheckButton( ID_TV_MOVEKEY,TRUE );

	m_bRecord = false;
	m_bPause = false;
	m_bPlay = false;



	RecalcLayout();

	CRect rc(0,0,0,0);
	int index;
	index = m_cDlgToolBar.CommandToIndex(ID_TV_SEQUENCE);
	m_cDlgToolBar.SetButtonInfo(index, ID_TV_SEQUENCE, TBBS_SEPARATOR, 200);
	m_cDlgToolBar.GetItemRect(index, &rc);
	rc.top++;
	rc.bottom += 400;
	m_sequences.Create( WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST|CBS_SORT,rc,this,IDC_SEQUENCES );
	m_sequences.SetFont( CFont::FromHandle((HFONT)gSettings.gui.hSystemFont) );
	m_sequences.SetParent( &m_cDlgToolBar );

	rc.SetRect(0,0,0,0);
	index = m_cDlgToolBar.CommandToIndex(ID_TV_CURSORPOS);
	m_cDlgToolBar.SetButtonInfo(index, ID_TV_CURSORPOS, TBBS_SEPARATOR, 60);
	m_cDlgToolBar.GetItemRect(index, &rc);
	rc.top++;
	m_CursorPos.Create("0.000", WS_CHILD|WS_VISIBLE|SS_CENTER|SS_CENTERIMAGE|SS_SUNKEN,rc,this,IDC_STATIC);
	m_CursorPos.SetFont( CFont::FromHandle((HFONT)gSettings.gui.hSystemFont) );
	m_CursorPos.SetParent( &m_cDlgToolBar );
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::InitSequences()
{
	CRect rc;
	int index;
	index = m_cDlgToolBar.CommandToIndex(ID_TV_SEQUENCE);
	m_cDlgToolBar.SetButtonInfo(index, ID_TV_SEQUENCE, TBBS_SEPARATOR, 200);
	m_cDlgToolBar.GetItemRect(index, &rc);
	// Expand the rectangle
	rc.left += 2;
	rc.bottom += 400;
	m_sequences.MoveWindow( rc );

	ReloadSequences();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::InvalidateSequence()
{
	m_bNeedReloadSequence = true;
}

void CTrackViewDialog::InvalidateTrackList()
{
	if (m_wndTrackProps.m_hWnd)
	{
		m_wndTrackProps.ReloadKey();
	}
	if (m_wndKeys.m_hWnd)
		m_wndKeys.Invalidate();
}

//////////////////////////////////////////////////////////////////////////
static void AddTrack( IAnimNode *node )
{
	ITcbKey key;
	ZeroStruct(key);

	IAnimBlock *anim = node->GetAnimBlock();
	if (!anim)
		return;

	IAnimTrack *track = anim->GetTrack(APARAM_POS);
	if (!track)
		return;

	track->SetNumKeys( 10 );
	for (int i = 0; i < track->GetNumKeys(); i++)
	{
		key.time = (frand()+1)/2.0f * 20;
		key.SetFloat( (frand()) * 100 );
		track->SetKey(i,&key);
	}

	track->SortKeys();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::Update()
{
	if (m_bNeedReloadSequence)
	{
		m_bNeedReloadSequence = false;
		IAnimSequence *seq = GetMovieSystem()->FindSequence(m_currSequenceName);
		SetCurrentSequence( seq );
	}
	CAnimationContext *ac = GetIEditor()->GetAnimation();
	float fTime=ac->GetTime();
	m_wndKeys.SetCurrTime(fTime);
	if (fTime!=m_fLastTime)
	{
		m_fLastTime=fTime;
		char sText[16];
		sprintf(sText, "%1.3f", fTime);
		m_CursorPos.SetWindowText(sText);
	}

	// Check movie camera.
	//GetIEditor()->GetMovieSystem()->GetCameraParams(
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnGoToPrevKey()
{
	CTrackViewNodes::SItemInfo *pItemInfo=m_wndNodes.GetSelectedNode();
	if ((!pItemInfo) || (!pItemInfo->node))
		return;
	std::vector<IAnimTrack*> vecTracks;
	if (!pItemInfo->track)
	{
		for (int i=0;i<pItemInfo->node->GetAnimBlock()->GetTrackCount();i++)
		{
			IAnimTrack *pTrack=NULL;
			int type;
			if (pItemInfo->node->GetAnimBlock()->GetTrackInfo(i,type,&pTrack))
				vecTracks.push_back(pTrack);
		}
	}else
	{
		vecTracks.push_back(pItemInfo->track);
	}
	float fClosestKeyTime=-1.0f;
	float fClosestDist=1E8;
	float fCurrTime=GetIEditor()->GetAnimation()->GetTime();
	for (std::vector<IAnimTrack*>::iterator It=vecTracks.begin();It!=vecTracks.end();++It)
	{
		IAnimTrack *pTrack=(*It);
		for (int i=0;i<pTrack->GetNumKeys();i++)
		{
			float fKeyTime=pTrack->GetKeyTime(i);
			float fKeyDist=fCurrTime-fKeyTime;
			if ((fKeyDist>0.0f) && (fKeyDist<fClosestDist))
			{
				fClosestDist=fKeyDist;
				fClosestKeyTime=pTrack->GetKeyTime(i);
			}
		}
	}
	if (fClosestKeyTime>=0.0f)
		GetIEditor()->GetAnimation()->SetTime(fClosestKeyTime);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnGoToNextKey()
{
	CTrackViewNodes::SItemInfo *pItemInfo=m_wndNodes.GetSelectedNode();
	if ((!pItemInfo) || (!pItemInfo->node))
		return;
	std::vector<IAnimTrack*> vecTracks;
	if (!pItemInfo->track)
	{
		for (int i=0;i<pItemInfo->node->GetAnimBlock()->GetTrackCount();i++)
		{
			IAnimTrack *pTrack=NULL;
			int type;
			if (pItemInfo->node->GetAnimBlock()->GetTrackInfo(i,type,&pTrack))
				vecTracks.push_back(pTrack);
		}
	}else
	{
		vecTracks.push_back(pItemInfo->track);
	}
	float fClosestKeyTime=-1.0f;
	float fClosestDist=1E8;
	float fCurrTime=GetIEditor()->GetAnimation()->GetTime();
	for (std::vector<IAnimTrack*>::iterator It=vecTracks.begin();It!=vecTracks.end();++It)
	{
		IAnimTrack *pTrack=(*It);
		for (int i=0;i<pTrack->GetNumKeys();i++)
		{
			float fKeyTime=pTrack->GetKeyTime(i);
			float fKeyDist=fKeyTime-fCurrTime;
			if ((fKeyDist>0.0f) && (fKeyDist<fClosestDist))
			{
				fClosestDist=fKeyDist;
				fClosestKeyTime=pTrack->GetKeyTime(i);
			}
		}
	}
	if (fClosestKeyTime>=0.0f)
		GetIEditor()->GetAnimation()->SetTime(fClosestKeyTime);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnAddKey()
{
	m_wndKeys.SetMouseActionMode( TVMODE_ADDKEY );
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnDelKey()
{
	m_wndKeys.DelSelectedKeys();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnMoveKey()
{
	m_wndKeys.SetMouseActionMode( TVMODE_MOVEKEY );
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnSlideKey()
{
	m_wndKeys.SetMouseActionMode( TVMODE_SLIDEKEY );
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnScaleKey()
{
	m_wndKeys.SetMouseActionMode( TVMODE_SCALEKEY );
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnAddSequence()
{
	CStringDlg dlg( "New Sequence" );
	if (dlg.DoModal() == IDOK)
	{
		CUndo undo("Add Sequence");
		CString seq = dlg.GetString();
		if (!seq.IsEmpty() && !GetIEditor()->GetMovieSystem()->FindSequence(seq))
		{
			IAnimSequence *sequence = GetIEditor()->GetMovieSystem()->CreateSequence(seq);
			GetIEditor()->GetObjectManager()->NewObject("SequenceObject", 0, seq);
			//m_sequences.AddString( seq );
			SetCurrentSequence( sequence );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::ReloadSequences()
{
	if (!GetMovieSystem())
		return;

	CAnimationContext *ac = GetIEditor()->GetAnimation();
	if (ac->IsPlaying())
		ac->SetPlaying( false );
	if (ac->IsRecordMode())
		ac->SetRecording(false);

	if (m_wndTrackProps.m_hWnd)
	{
		m_wndTrackProps.DestroyWindow();
	}

	m_bReloading=true;
	GetIEditor()->GetAnimation()->SetRecording( false );
	ISequenceIt *It= GetMovieSystem()->GetSequences();
	m_sequences.ResetContent();
	IAnimSequence *seq=It->first();
	while (seq)
	{
		m_sequences.AddString( seq->GetName() );
		seq=It->next();
	}
	// Select current sequence.

	m_currSequence = 0;
	if (!m_currSequenceName.IsEmpty())
	{
		IAnimSequence *seq = GetMovieSystem()->FindSequence(m_currSequenceName);
		SetCurrentSequence( seq );
	}
	if (!m_currSequence && m_sequences.GetCount() > 0)
	{
		m_sequences.SetCurSel(0);
		m_sequences.GetLBText(0,m_currSequenceName);
		IAnimSequence *seq = GetMovieSystem()->FindSequence(m_currSequenceName);
		SetCurrentSequence( seq );
	}
	It->Release();
	m_bReloading=false;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnDelSequence()
{
	//CString str;
	//str.Format( 
	if (AfxMessageBox( "Delete current sequence?",MB_YESNO|MB_ICONQUESTION ) == IDYES)
	{
		int sel = m_sequences.GetCurSel();
		CString seq;
		m_sequences.GetLBText(sel,seq);
		
		m_sequences.DeleteString(sel);
		
		if (m_sequences.GetCount() > 0)
		{
			m_sequences.SetCurSel(0);
		}
		OnChangeSequence();

		CBaseObject * pObj = GetIEditor()->GetObjectManager()->FindObject(seq);
		if(pObj)
			GetIEditor()->GetObjectManager()->DeleteObject(pObj);
		IAnimSequence *sequence = GetMovieSystem()->FindSequence(seq);
		if (sequence)
			GetMovieSystem()->RemoveSequence(sequence);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnEditSequence()
{
	// Edit current sequence.
	if (!m_currSequence)
		return;

	CTVSequenceProps dlg( m_currSequence,this );
	if (dlg.DoModal() == IDOK)
	{
		// Sequence updated.
		ReloadSequences();
	}
	m_wndKeys.Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnChangeSequence()
{
	int sel = m_sequences.GetCurSel();
	if (sel == LB_ERR)
	{
		SetCurrentSequence( 0 );
		return;
	}
	CString name;
	m_sequences.GetLBText( sel,name );
	// Display current sequence.
	IAnimSequence *seq = GetMovieSystem()->FindSequence(name);
	if (seq != m_currSequence)
	{
		SetCurrentSequence( seq );
	}
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::SetCurrentSequence( IAnimSequence *curr )
{
	bool bChanged = m_currSequence != curr;
	if (curr)
	{
		m_currSequence = curr;
		GetIEditor()->GetAnimation()->SetSequence( m_currSequence );
		m_currSequenceName = curr->GetName();

		m_wndNodes.SetSequence( m_currSequence );
		m_wndKeys.SetSequence( m_currSequence );
		Range timeRange = m_currSequence->GetTimeRange();
		m_wndKeys.SetTimeRange( timeRange.start,timeRange.end );
		m_wndKeys.SetStartMarker( timeRange.start );
		m_wndKeys.SetEndMarker( timeRange.end );
		
		m_sequences.SelectString(-1,m_currSequenceName);
	}
	else
	{
		m_currSequence = curr;
		GetIEditor()->GetAnimation()->SetSequence( 0 );
		m_currSequenceName = "";
		m_wndNodes.SetSequence( 0 );
		m_wndKeys.SetSequence( 0 );
	}
	if (bChanged)
	{
		// Notify all objects to update thier track gizmos.
		GetIEditor()->GetObjectManager()->SendEvent( EVENT_UPDATE_TRACKGIZMO );
	}
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnRecord()
{
	CAnimationContext *ac = GetIEditor()->GetAnimation();
	ac->SetRecording( !ac->IsRecording() );
	m_wndKeys.Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnAutoRecord()
{
	CAnimationContext *ac = GetIEditor()->GetAnimation();
	ac->SetAutoRecording( !ac->IsRecording(),m_fAutoRecordStep );
	m_wndKeys.Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnGoToStart()
{
	CAnimationContext *ac = GetIEditor()->GetAnimation();
	ac->SetTime(ac->GetMarkers().start);
	ac->SetPlaying( false );
	ac->SetRecording( false );

	if (m_currSequence != NULL)
	{
		// Reset sequence to the beginning.
		m_currSequence->Reset(true);
	}

	// Restore default camera in view.
	//GetIEditor()->GetViewManager()->SetCameraObjectId( m_defaulCameraObjectId );
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnGoToEnd()
{
	CAnimationContext *ac = GetIEditor()->GetAnimation();
	ac->SetTime(ac->GetMarkers().end);
	ac->SetPlaying( false );
	ac->SetRecording( false );

	if (m_currSequence != NULL)
	{
		// Reset sequence to the beginning.
		m_currSequence->Reset(true);
	}

	// Restore default camera in view.
	//GetIEditor()->GetViewManager()->SetCameraObjectId( m_defaulCameraObjectId );
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnPlay()
{
	m_defaulCameraObjectId = GetIEditor()->GetViewManager()->GetCameraObjectId();

	CAnimationContext *ac = GetIEditor()->GetAnimation();
	if (!ac->IsPlaying())
		ac->SetPlaying( true );
	else
		ac->SetPlaying( false );

	// Set current camera params.
	//OnSetCamera( GetMovieSystem()->GetCameraParams() );
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnStop()
{
	CAnimationContext *ac = GetIEditor()->GetAnimation();

	bool bWasPlaying = ac->IsPlaying() || ac->IsRecordMode();
	if (ac->IsPlaying())
		ac->SetPlaying( false );
	else
		OnGoToStart();
	ac->SetRecording( false );

	if (m_currSequence != NULL && !bWasPlaying)
	{
		// Reset sequence to the beginning.
		m_currSequence->Reset(true);
	}

	// Restore default camera in view.
	//GetIEditor()->GetViewManager()->SetCameraObjectId( m_defaulCameraObjectId );
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnPause()
{
	CAnimationContext *ac = GetIEditor()->GetAnimation();
	if (ac->IsPaused())
		ac->Resume();
	else
		ac->Pause();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnLoop()
{
	CAnimationContext *ac = GetIEditor()->GetAnimation();
	ac->SetLoopMode(!ac->IsLoopMode());
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnCopyPasteKeys()
{
	if (!m_wndKeys.CopyPasteKeys())
		AfxMessageBox("Copying of keys failed.", MB_ICONEXCLAMATION | MB_OK);
}

void CTrackViewDialog::OnSetFocus(CWnd* pOldWnd)
{
	//CToolbarDialog::OnSetFocus(pOldWnd);

	//if (AfxGetMainWnd())
		//AfxGetMainWnd()->SetFocus();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnUpdateRecord( CCmdUI* pCmdUI )
{
	if (GetIEditor()->GetAnimation()->IsRecordMode())
		pCmdUI->SetCheck(TRUE);
	else
		pCmdUI->SetCheck(FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnUpdateRecordAuto( CCmdUI* pCmdUI )
{
	if (GetIEditor()->GetAnimation()->IsAutoRecording())
		pCmdUI->SetCheck(TRUE);
	else
		pCmdUI->SetCheck(FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnUpdatePlay( CCmdUI* pCmdUI )
{
	if (GetIEditor()->GetAnimation()->IsPlayMode())
		pCmdUI->SetCheck(TRUE);
	else
		pCmdUI->SetCheck(FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnUpdatePause( CCmdUI* pCmdUI )
{
	if (GetIEditor()->GetAnimation()->IsPaused())
		pCmdUI->SetCheck(TRUE);
	else
		pCmdUI->SetCheck(FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnUpdateLoop( CCmdUI* pCmdUI )
{
	if (GetIEditor()->GetAnimation()->IsLoopMode())
		pCmdUI->SetCheck(TRUE);
	else
		pCmdUI->SetCheck(FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnUpdateCopyPasteKeys( CCmdUI* pCmdUI )
{
	pCmdUI->Enable(m_wndKeys.CanCopyPasteKeys());
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
	switch (event)
	{
	case eNotify_OnBeginNewScene:
	case eNotify_OnCloseScene:
		SetCurrentSequence(NULL);
		break;
	case eNotify_OnEndSceneOpen:
		ReloadSequences();
		break;
	case eNotify_OnMissionChange:
		ReloadSequences();
		break;
	case eNotify_OnUpdateTrackView:
		ReloadSequences();
		break;
	case eNotify_OnIdleUpdate:
		Update();
		break;
	case eNotify_OnUpdateTrackViewKeys:
		InvalidateTrackList();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnDestroy()
{
	CToolbarDialog::OnDestroy();

	if (GetMovieSystem())
		GetMovieSystem()->SetCallback(NULL);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnAddSelectedNode()
{
	if (!m_currSequence)
		return;

	m_wndNodes.AddSelectedNodes();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnAddSceneTrack()
{
	if (!m_currSequence)
		return;

	m_wndNodes.AddSceneNodes();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewDialog::OnAddSelectedNodeUpdate( CCmdUI* pCmdUI )
{
	if (GetIEditor()->GetSelection()->IsEmpty() || !m_currSequence)
		pCmdUI->Enable(FALSE);
	else
		pCmdUI->Enable(TRUE);
}