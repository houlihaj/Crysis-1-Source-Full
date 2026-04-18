// SoundBrowserDialog.cpp : implementation file
//

#include "stdafx.h"
#include "SoundBrowserDialog.h"
#include "CustomFileDialog.h"
#include <ISound.h>

//#include "Controls\PropertyCtrl.h"

// CSoundBrowserDialog dialog


bool CSoundBrowserDialog::m_isAutoPlay = true;
bool CSoundBrowserDialog::m_isCached = false;
std::vector<SCachElem> CSoundBrowserDialog::m_cachElems;

IMPLEMENT_DYNAMIC(CSoundBrowserDialog, CDialog)
CSoundBrowserDialog::CSoundBrowserDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CSoundBrowserDialog::IDD, pParent)
{
	//m_propWnd = 0;
	m_isBrowse = false;
	m_pSound = NULL;
	m_ListenerID = LISTENERID_INVALID;
}

CSoundBrowserDialog::~CSoundBrowserDialog()
{
	if (m_pSound)
		m_pSound->Stop();
	
	if (gEnv->pSoundSystem && m_ListenerID != LISTENERID_INVALID)
		gEnv->pSoundSystem->RemoveListener(m_ListenerID);
}

void CSoundBrowserDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SOUNDSTREE, m_soundsTree);
}


BEGIN_MESSAGE_MAP(CSoundBrowserDialog, CDialog)
	ON_NOTIFY(TVN_SELCHANGED, IDC_SOUNDSTREE, OnTvnSelchangedTree)
	ON_NOTIFY(NM_DBLCLK, IDC_SOUNDSTREE, OnTvnDoubleClick)
	ON_BN_CLICKED(IDC_BROWSE, OnBrowseBtn)
	ON_BN_CLICKED(IDC_AUTOPLAY, OnAutoPlayBtn)
	ON_BN_CLICKED(IDC_PLAY, OnPlayBtn)
	ON_BN_CLICKED(IDC_REFRESH, OnRefreshBtn)
	ON_WM_SIZE()
END_MESSAGE_MAP()


void CSoundBrowserDialog::Init(const CString & name)
{
	m_initName = name;
	
	if (gEnv->pSoundSystem && m_ListenerID == LISTENERID_INVALID)
	{
		m_ListenerID = gEnv->pSoundSystem->CreateListener();
		IListener *pListener = gEnv->pSoundSystem->GetListener(m_ListenerID);
		pListener->SetRecordLevel(1.0f);
		pListener->SetActive(true);
		gEnv->pSoundSystem->Update(eSoundUpdateMode_All);
	}

}

void CSoundBrowserDialog::CollectCach()
{
	BeginWaitCursor();

	m_cachElems.clear();

	CFileUtil::FileArray fa;
	CFileUtil::ScanDirectory("", "*.fdp", fa, true);

	for(int f = 0; f<fa.size(); f++)
	{
		XmlParser parser;
		XmlNodeRef root = parser.parse( fa[f].filename );
		char path[_MAX_PATH];
		strcpy(path, fa[f].filename);
		char * ch;
		while(ch = strchr(path,'\\'))
			*ch='/';
		if(ch = strrchr(path,'/'))
			*ch = 0;

		if(root)
		{
			const char * pName="";
			XmlNodeRef name = root->findChild("name");
			if(name)
				pName = name->getContent();
			//HTREEITEM hProjItem = m_soundsTree.InsertItem( path, 0, 0, TVI_ROOT);
			m_cachElems.push_back(SCachElem(0, path));
			for(int i=0; i<root->getChildCount();i++)
			{
				XmlNodeRef gr= root->getChild(i);
				if(!strcmp(gr->getTag(), "eventgroup"))
				{
					const char * pNameGr="";
					XmlNodeRef name = gr->findChild("name");
					if(name)
						pNameGr = name->getContent();
					//HTREEITEM hGroupItem = m_soundsTree.InsertItem( CString(pNameGr), 2, 2, hProjItem);
					m_cachElems.push_back(SCachElem(1, pNameGr));
					for(int j=0; j<gr->getChildCount();j++)
					{
						XmlNodeRef ev= gr->getChild(j);
						if(!strcmp(ev->getTag(), "event"))
						{
							const char * pNameEv="";
							XmlNodeRef name = ev->findChild("name");
							if(name)
								pNameEv = name->getContent();
							//HTREEITEM hEventItem = m_soundsTree.InsertItem( CString(pNameEv), 9, 9, hGroupItem);
							m_cachElems.push_back(SCachElem(2, pNameEv));
						}
					}
				}
			}
		}
	}

	EndWaitCursor();
	m_isCached = true;
}


void CSoundBrowserDialog::FillSoundsTree()
{
	m_soundsTree.DeleteAllItems();
	m_treeMap.clear();

	HTREEITEM hProjItem=0;
	HTREEITEM hGroupItem = 0;
	HTREEITEM hEventItem;
	CString path;
	CString pNameGr;
	CString pNameEv;

	for(int i = 0; i<m_cachElems.size(); i++)
	{
		if(m_cachElems[i].level==0)
		{
			if(hProjItem)
				m_soundsTree.SortChildren(hProjItem);
			path = m_cachElems[i].name;
			hProjItem = m_soundsTree.InsertItem( path, 0, 0, TVI_ROOT);
		}
		if(m_cachElems[i].level==1)
		{
			if(hGroupItem)
				m_soundsTree.SortChildren(hGroupItem);
			pNameGr = m_cachElems[i].name;
			hGroupItem = m_soundsTree.InsertItem( pNameGr, 2, 2, hProjItem);
		}
		if(m_cachElems[i].level==2)
		{
			pNameEv = m_cachElems[i].name;
			hEventItem = m_soundsTree.InsertItem( pNameEv, 9, 9, hGroupItem);

			char str[_MAX_PATH];
			strcpy( str, path );
			strcat( str, ":" );
			strcat( str, pNameGr );
			strcat( str, ":" );
			strcat( str, pNameEv );
			m_treeMap[hEventItem] = str;

			if(!stricmp(m_initName, str))
			{
				m_soundsTree.Expand(hProjItem, TVE_EXPAND);
				m_soundsTree.Expand(hGroupItem, TVE_EXPAND);
				m_soundsTree.SelectItem(hEventItem);
			}
		}
	}
	if(hProjItem)
		m_soundsTree.SortChildren(hProjItem);

	if(hGroupItem)
		m_soundsTree.SortChildren(hGroupItem);
}

BOOL CSoundBrowserDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	if(CCustomFileDialog::m_bForceBrowse)
	{
		m_isBrowse = true;
		EndDialog( IDOK );
		return TRUE;
	}

	CButton * rb = (CButton *)GetDlgItem(IDC_AUTOPLAY);
	if(rb)
		rb->SetCheck(m_isAutoPlay);

	CMFCUtils::LoadTrueColorImageList( m_imageListFiles,IDB_FILES_IMAGE,16,RGB(255,0,255) );

	if(!m_isCached)
		CollectCach();

	//m_imageListFiles.Create( MAKEINTRESOURCE(IDB_FILES_IMAGE),16,1,RGB(255,255,255) );
	m_imageListFiles.SetOverlayImage( 1,1 );
	m_soundsTree.SetImageList(&m_imageListFiles,TVSIL_NORMAL);

	FillSoundsTree();

	WINDOWPLACEMENT wp;
	GetWindowPlacement(&wp);

	CWnd * wnd;

	if(wnd = GetDlgItem(IDC_AUTOPLAY))
	{
		WINDOWPLACEMENT wp2;
		wnd->GetWindowPlacement(&wp2);
		m_buttonLeft = wp.rcNormalPosition.right-wp.rcNormalPosition.left-wp2.rcNormalPosition.left;
	}

	if(wnd = GetDlgItem(IDC_SOUNDSTREE))
	{
		WINDOWPLACEMENT wp2;
		wnd->GetWindowPlacement(&wp2);
		m_treeBot= wp.rcNormalPosition.bottom-wp.rcNormalPosition.top-wp2.rcNormalPosition.bottom;
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}


void CSoundBrowserDialog::Play(CString name)
{
	// Play preview sound.
	if (gEnv->pSoundSystem)
	{
		// stop the previous sound
		if (m_pSound)
			m_pSound->Stop();

		// start new one
		m_pSound = gEnv->pSoundSystem->CreateSound( name.GetString(),FLAG_SOUND_2D|FLAG_SOUND_STEREO|FLAG_SOUND_16BITS|FLAG_SOUND_LOAD_SYNCHRONOUSLY );
		
		if (m_pSound)
			m_pSound->Play();
	}
}


void CSoundBrowserDialog::OnTvnSelchangedTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
	m_return = m_treeMap[pNMTreeView->itemNew.hItem];

	CButton * rb = (CButton *)GetDlgItem(IDC_AUTOPLAY);
	if(rb && rb->GetCheck())
		if (!m_return.IsEmpty())
			Play(m_return);
}

//////////////////////////////////////////////////////////////////////////
void CSoundBrowserDialog::OnTvnDoubleClick(NMHDR *pNMHDR, LRESULT *pResult)
{
	if(!m_return.IsEmpty())
		EndDialog(IDOK);
}

bool CSoundBrowserDialog::IsBrowse()
{
	return m_isBrowse;
}

void CSoundBrowserDialog::OnBrowseBtn()
{
	m_isBrowse = true;
	EndDialog( IDOK );
}

void CSoundBrowserDialog::OnPlayBtn()
{
	if (!m_return.IsEmpty())
		Play(m_return);
}


void CSoundBrowserDialog::OnRefreshBtn()
{
	CollectCach();
	FillSoundsTree();
}


void CSoundBrowserDialog::OnAutoPlayBtn()
{
	CButton * rb = (CButton *)GetDlgItem(IDC_AUTOPLAY);
	if(rb)
		m_isAutoPlay = rb->GetCheck();
	if(!m_isAutoPlay)
		if (m_pSound)
			m_pSound->Stop();
}



const CString & CSoundBrowserDialog::GetString()
{
	return m_return;
}

void CSoundBrowserDialog::OnSize(UINT nType, int cx, int cy) 
{
	CWnd * wnd;
	int Items[] = {IDC_PLAY, IDC_AUTOPLAY, IDOK, IDCANCEL, IDC_BROWSE, IDC_REFRESH};

	WINDOWPLACEMENT wp;
	GetWindowPlacement(&wp);

	for(int i=0; i<sizeof(Items)/sizeof(Items[0]); i++)
	{
		if(wnd = GetDlgItem(Items[i]))
		{
			WINDOWPLACEMENT wp2;
			wnd->GetWindowPlacement(&wp2);
			int cx =wp.rcNormalPosition.right-wp.rcNormalPosition.left-m_buttonLeft - wp2.rcNormalPosition.left;
			wp2.rcNormalPosition.left+=cx;
			wp2.rcNormalPosition.right+=cx;
			wnd->SetWindowPlacement(&wp2);
		}
	}

	if(wnd = GetDlgItem(IDC_SOUNDSTREE))
	{
		WINDOWPLACEMENT wp2;
		wnd->GetWindowPlacement(&wp2);
		wp2.rcNormalPosition.right = wp.rcNormalPosition.right-wp.rcNormalPosition.left-m_buttonLeft-5;
		wp2.rcNormalPosition.bottom = wp.rcNormalPosition.bottom-wp.rcNormalPosition.top - m_treeBot;
		wnd->SetWindowPlacement(&wp2);
	}

}

