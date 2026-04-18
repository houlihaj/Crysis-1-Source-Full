// SoundMoodDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SoundMoodDlg.h"
#include "SoundMoodMgr.h"
#include "StringDlg.h"
#include "Controls\PropertyItem.h"
#include "ISound.h"
#include "ISoundMoodManager.h"

#include "SoundMoodUI.h"

#define WM_UPDATEPROPERTIES	WM_APP+1
#define IDC_TREE_CTRL AFX_IDW_PANE_FIRST

//////////////////////////////////////////////////////////////////////////
static CSoundMoodUI_CatProps sSoundMoodUI_CatProps;
static CSoundMoodUI_MoodProps sSoundMoodUI_MoodProps;

IMPLEMENT_DYNAMIC(CSoundMoodDlg, CBaseLibraryDialog)
CSoundMoodDlg::CSoundMoodDlg(CWnd* pParent /*=NULL*/)
	: CBaseLibraryDialog(CSoundMoodDlg::IDD, pParent)
{
	m_pMoodManager = NULL;

	if (gEnv->pSoundSystem)
		m_pMoodManager = gEnv->pSoundSystem->GetIMoodManager();

	// Immediately create dialog.
	Create( CSoundMoodDlg::IDD,pParent );
}

CSoundMoodDlg::~CSoundMoodDlg()
{
}

void CSoundMoodDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//DDX_Control(pDX, IDC_PRESETS, m_wndPresets);
	//DDX_Control(pDX, IDC_PRESETS, m_wndSoundMoods);
	//DDX_Control(pDX, IDC_PARAMS, m_wndParams);
}


BEGIN_MESSAGE_MAP(CSoundMoodDlg, CBaseLibraryDialog)
	ON_COMMAND(ID_SOUNDBANK_SCAN, OnScan)
	ON_COMMAND(ID_SAVE, OnSaveSoundMood)
	ON_COMMAND(ID_SOUNDBANK_NEWENTRY, OnAddEntry)
	ON_COMMAND(ID_SOUNDBANK_DELENTRY, OnDelEntry)
	//ON_COMMAND(ID_SOUNDBANK_MUTE_OFF, OnMuteOff)
	//ON_COMMAND(ID_SOUNDBANK_MUTE_ON, OnMuteOn)
	//ON_COMMAND(ID_SOUNDBANK_TOGGLE_PROP, OnToggleProp)
	
	ON_WM_SIZE()
	ON_WM_CLOSE()
	ON_WM_DESTROY()
	ON_NOTIFY(TVN_SELCHANGED, IDC_TREE_CTRL, OnSelChangedItemTree)
//	ON_NOTIFY(NM_RCLICK , IDC_TREE_CTRL, OnNotifyTreeRClick)
	ON_MESSAGE(WM_UPDATEPROPERTIES, OnUpdateProperties)
END_MESSAGE_MAP()

BOOL CSoundMoodDlg::OnInitDialog()
{	
		
	m_pSoundMoodMgr = GetIEditor()->GetSoundMoodMgr();
	m_pSoundMoodMgr->Load();

	CBaseLibraryDialog::OnInitDialog();

	CRect rc;	
	GetClientRect(rc);

	InitToolbar(IDR_SOUNDBANK);

	
	m_wndVSplitter.CreateStatic( this,1,2,WS_CHILD|WS_VISIBLE );

	m_treeCtrl.Create( WS_VISIBLE|WS_CHILD|WS_TABSTOP|WS_BORDER|TVS_HASBUTTONS|TVS_LINESATROOT|TVS_HASLINES,rc,&m_wndVSplitter,IDC_LIBRARY_ITEMS_TREE );
	m_treeCtrl.ModifyStyle( 0,TVS_EDITLABELS );
	m_filesImageList.Create(IDB_SOUND_FILES, 16, 1, RGB (255, 0, 255));
	m_treeCtrl.SetImageList(&m_filesImageList,TVSIL_NORMAL);

	m_wndParams.Create( WS_VISIBLE|WS_CHILD|WS_BORDER,rc,&m_wndVSplitter,2 );
	m_wndParams.SetUpdateCallback(functor(*this, &CSoundMoodDlg::OnParamsChanged));

	int VSplitter = AfxGetApp()->GetProfileInt("Dialogs\\MusicEditor","VSplitter",300 );

	m_wndVSplitter.SetPane( 0,0,&m_treeCtrl,CSize(200,100) );
	m_wndVSplitter.SetPane( 0,1,&m_wndParams,CSize(400,100) );

	
	Update();
	RecalcLayout();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
void CSoundMoodDlg::InitToolbar( UINT nToolbarResID )
{
	CRect rc;	
	GetClientRect(rc);

	// Create the toolbar
//	m_toolbar.CreateEx(this, TBSTYLE_FLAT|TBSTYLE_WRAPABLE,	WS_CHILD|WS_VISIBLE|CBRS_TOP|CBRS_TOOLTIPS|CBRS_FLYBY|CBRS_SIZE_DYNAMIC);
	//m_toolbar.LoadToolBar(IDR_EAXPRESETS);
	VERIFY( m_toolbar.CreateEx(this, TBSTYLE_FLAT|TBSTYLE_WRAPABLE,
		WS_CHILD|WS_VISIBLE|CBRS_TOP|CBRS_TOOLTIPS|CBRS_FLYBY|CBRS_SIZE_DYNAMIC) );

	//VERIFY( m_toolbar.LoadToolBar24(IDR_SoundMood) );
	VERIFY( m_toolbar.LoadToolBar24(IDR_SOUNDBANK) ); // IDR_SoundMood // IDR_DB_MUSIC_BAR

	m_toolbar.SetWindowPos(NULL, 0, 0, rc.right, 70, SWP_NOZORDER);
	CSize sz = m_toolbar.CalcDynamicLayout(TRUE,TRUE);

	//m_toolbar.GetToolBarCtrl().SetExtendedStyle(TBSTYLE_EX_DRAWDDARROWS);
	//m_toolbar.CalcFixedLayout(0, TRUE);
	//CBaseLibraryDialog::InitToolbar();
}

//////////////////////////////////////////////////////////////////////////
bool CSoundMoodDlg::TreeShowMood(IMood *pMood, HTREEITEM hParent)
{
	bool bResult = false;

	if (!pMood)
		return false;

	// show the Master Category
	ICategory *pMaster = pMood->GetMasterCategory();
	if (pMaster)
	{
		const char* sChildName = pMaster->GetName();
		void* pPlatform = pMaster->GetPlatformCategory();
		HTREEITEM hElement;
		
		if (pPlatform)
			hElement = m_treeCtrl.InsertItem(sChildName, 2,2, hParent);
		else
			hElement = m_treeCtrl.InsertItem(sChildName, 4,4, hParent);

		m_treeCtrl.SetItemData(hElement, (DWORD_PTR)pMaster);
		TreeShowCategory(pMaster, hElement);
		m_treeCtrl.SortChildren(hElement);
	}


	// show the inactive categories
	int nNum = pMood->GetCategoryCount();
	for (int j = 0; j<nNum; ++j)
	{

		ICategory *pChild = pMood->GetCategoryByIndex(j);

		if (pChild)
		{
			const char* sChildName = pChild->GetName();
			void* pPlatform = pChild->GetPlatformCategory();
			HTREEITEM hElement;

			if (!pPlatform)
			{
				hElement = m_treeCtrl.InsertItem(sChildName, 4,4, hParent);

				m_treeCtrl.SetItemData(hElement, (DWORD_PTR)pChild);
				TreeShowCategory(pChild, hElement);
				m_treeCtrl.SortChildren(hElement);
			}
		}
	}


	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CSoundMoodDlg::TreeShowCategory(ICategory *pCategory, HTREEITEM hParent)
{
	bool bResult = false;

	if (!pCategory)
		return false;

	int nNum = pCategory->GetChildCount();
	for (int j = 0; j<nNum; ++j)
	{
		ICategory *pChild = pCategory->GetCategoryByIndex(j);

		if (pChild)
		{
			const char* sChildName = pChild->GetName();
			void* pPlatform = pChild->GetPlatformCategory();
			HTREEITEM hElement;
			//if (pChild->GetMuted())
			//hElement = m_treeCtrl.InsertItem(sChildName, 5,5, hParent);
			//else
			if (pPlatform)
				hElement = m_treeCtrl.InsertItem(sChildName, 2,2, hParent);
			else
				hElement = m_treeCtrl.InsertItem(sChildName, 4,4, hParent);
			//else
			//if (pChild->GetMuted())
			//hElement = m_treeCtrl.InsertItem(sChildName, 4,4, hParent);
			//else
			//hElement = m_treeCtrl.InsertItem(sChildName, 1,1, hParent);

			//m_treeCtrl.SetItemData(hElement, (DWORD_PTR)pChild->GetHandle().GetIndex());
			m_treeCtrl.SetItemData(hElement, (DWORD_PTR)pChild);
			TreeShowCategory(pChild, hElement);
			m_treeCtrl.SortChildren(hElement);
		}
	}


	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CSoundMoodDlg::Update()
{
	if (!m_pMoodManager)
		return;

	HTREEITEM hSelectedItem = m_treeCtrl.GetSelectedItem();
	CString sMoodName = m_treeCtrl.GetItemText(hSelectedItem);

	m_treeCtrl.DeleteAllItems();


	int i = 0;
	IMood *pMood = m_pMoodManager->GetMoodPtr(i);

	while (pMood)
	{
		
		const char* sMoodName = pMood->GetName();
		HTREEITEM hMood;

		//if (pGroup->GetMuted())
			//hGroup = m_treeCtrl.InsertItem(sGroupName, 3,3, TVI_ROOT);
		//else
			hMood = m_treeCtrl.InsertItem(sMoodName, 0,0, TVI_ROOT);

		//m_treeCtrl.SetItemData(hMood, (DWORD_PTR)pMood->GetHandle().GetIndex());
		m_treeCtrl.SetItemData(hMood, (DWORD_PTR)NULL);
		
		TreeShowMood(pMood, hMood);
		
		m_treeCtrl.SortChildren(hMood);

		++i;
		pMood = m_pMoodManager->GetMoodPtr(i);
	}

	HTREEITEM hItem = m_treeCtrl.GetSelectedItem();
	HTREEITEM hParent = hItem;

	do 
	{
		hParent = m_treeCtrl.GetParentItem(hItem);
	} while(hParent);

	if (hItem)
	{
		m_treeCtrl.EnsureVisible(hParent);
	}

	
}

//////////////////////////////////////////////////////////////////////////
LRESULT CSoundMoodDlg::OnUpdateProperties(WPARAM wParam, LPARAM lParam)
{
	//Update();
//	int nCurSel=m_wndPresets.GetCurSel();
	//if (nCurSel<0)
	//	return -1;
	//CString sCurSel;
	//m_wndPresets.GetText(nCurSel, sCurSel);
	//m_wndParams.EnableUpdateCallback(false);
	//m_wndParams.CreateItems(m_pSoundMoodMgr->GetRootNode()->findChild(sCurSel));
	//m_wndParams.EnableUpdateCallback(true);
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CSoundMoodDlg::OnParamsChanged(XmlNodeRef pNode)
{
	if (!m_pMoodManager)
		return;

	CPropertyItem *Current = m_wndParams.GetSelectedItem();

	if (!Current)
		return;

	IVariable *CurrentVar = Current->GetVariable();
	//CString sName = Current->GetName();
	//string sVarName= CurrentVar->GetName();

	//int32 nTest=0;
	//CurrentVar->Get(nTest);
	
	HTREEITEM hItem = m_treeCtrl.GetSelectedItem();

	if (!hItem)
		return;

	//CategoryHandle SHandle((uint32)m_treeCtrl.GetItemData( hItem ));
	//ICategory *pElement = m_pMoodManager->GetCategoryPtr(SHandle);
	ICategory *pElement = (ICategory*)m_treeCtrl.GetItemData( hItem );

	if (pElement)
	{
		// this is a Category
		sSoundMoodUI_CatProps.WriteToPtr(pElement);
	}
	else
	{
		// is this a Mood?
		CString sName = m_treeCtrl.GetItemText(hItem);
		IMood *pMood = m_pMoodManager->GetMoodPtr(sName.GetBuffer());
		if (pMood)
			sSoundMoodUI_MoodProps.WriteToPtr(pMood);
	}

//	Update();
	//OnSelChangedItemTree(0,0);

	//if (!m_pSoundMoodMgr->UpdateParameter(pNode))
		//AfxMessageBox("An error occured while updating script-tables.", MB_ICONEXCLAMATION | MB_OK);
}

void CSoundMoodDlg::OnSelChangedItemTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (m_pVars)
		m_pVars = 0;

	m_pVars = new CVarBlock;

	m_wndParams.DeleteAllItems();
	m_wndParams.EnableWindow(FALSE);

	if (m_treeCtrl && m_treeCtrl.GetCount())
	{
		ISoundMoodManager *pMoodManager = NULL;
		if (gEnv->pSoundSystem)
			pMoodManager = gEnv->pSoundSystem->GetIMoodManager();

		if (!pMoodManager)
			return;

		HTREEITEM hItem = m_treeCtrl.GetSelectedItem();

		if (!hItem)
		{
			m_treeCtrl.SelectItem(0);
			return;
		}

		//CategoryHandle SHandle((uint32)m_treeCtrl.GetItemData( hItem ));
		//ICategory *pCategory = pMoodManager->GetCategoryPtr(SHandle);
		ICategory *pCategory = (ICategory*)m_treeCtrl.GetItemData( hItem );

		if (pCategory)
		{
			sSoundMoodUI_CatProps.ReadFromPtr(pCategory);
			m_pVars = sSoundMoodUI_CatProps.CreateVars();

			if (m_pVars)
			{
				m_wndParams.AddVarBlock( m_pVars );
				m_wndParams.ExpandAll();
				m_wndParams.EnableWindow(TRUE);
			}
		}
		else
		{
			// is this a mood?
			CString sName = m_treeCtrl.GetItemText(hItem);
			IMood *pMood = pMoodManager->GetMoodPtr(sName.GetBuffer());

			if (pMood)
			{
				sSoundMoodUI_MoodProps.ReadFromPtr(pMood);
				m_pVars = sSoundMoodUI_MoodProps.CreateVars();

				if (m_pVars)
				{
					m_wndParams.AddVarBlock( m_pVars );
					m_wndParams.ExpandAll();
					m_wndParams.EnableWindow(TRUE);
				}
			}
		}
	}
}

void CSoundMoodDlg::OnSaveSoundMood()
{
	bool bResult = false;

	bResult = m_pSoundMoodMgr->Save();

	if (!bResult)
		AfxMessageBox("Failed to save SoundMood. (Not checked out?)", MB_ICONEXCLAMATION | MB_OK);

}

void CSoundMoodDlg::OnAddEntry()
{
	if (!m_pMoodManager)
		return;

	//HTREEITEM hItem = m_treeCtrl.GetSelectedItem();
	//if (!hItem)
	{
		// new group
		CStringDlg dlgName("Please enter name for new SoundMood:");

		if (dlgName.DoModal()==IDOK)
		{
			CString sName = dlgName.GetString();
			string sMoodName(sName.GetBuffer());

			if (sMoodName.empty())
				return;

			if (m_pMoodManager->GetMoodPtr(sMoodName))
			{
				AfxMessageBox("Cannot add SoundMood entry. That name already exists.", MB_ICONEXCLAMATION | MB_OK);
				return;
			}

			IMood *pMood = m_pMoodManager->AddMood(sMoodName);

			if (!pMood)
				AfxMessageBox("Failed to insert new SoundMood entry.", MB_ICONEXCLAMATION | MB_OK);

			m_pMoodManager->RefreshCategories(sMoodName);
		}
	}
//	else
	//{
		//// new Child

		////ItemDesc *pItemDesc = (ItemDesc*)m_treeCtrl.GetItemData( hItem );
		////ISGElement *pElement = pItemDesc->pElement;
		//CategoryHandle SHandle((uint32)m_treeCtrl.GetItemData( hItem ));
		//ICategory *pElement = m_pMoodManager->GetCategoryPtr(SHandle);


		//if (pElement)
		//{
		//	// todo add name of Parent group
		//	CStringDlg dlgName("Please enter name for new Element:");

		//	if (dlgName.DoModal()==IDOK)
		//	{
		//		CString sName = dlgName.GetString();
		//		string sChildName(sName.GetBuffer());

		//		if (pElement->GetChild(sChildName))
		//		{
		//			AfxMessageBox("Cannot add SoundMood entry. That name already exists.", MB_ICONEXCLAMATION | MB_OK);
		//			return;
		//		}

		//		CategoryHandle	nHandle = pElement->AddChild(sChildName);

		//		if (!(nHandle.IsMoodIDValid()))
		//			AfxMessageBox("Failed to insert new SoundMood entry.", MB_ICONEXCLAMATION | MB_OK);
		//	}
		//}
	//}

	Update();
}

void CSoundMoodDlg::OnDelEntry()
{
	if (!m_pMoodManager)
		return;

	HTREEITEM hItem = m_treeCtrl.GetSelectedItem();
	if (!hItem)
		return;

	//ItemDesc *pItemDesc = (ItemDesc*)m_treeCtrl.GetItemData( hItem );
	//ISGElement *pElement = pItemDesc->pElement;
	//CategoryHandle SHandle((uint32)m_treeCtrl.GetItemData( hItem ));

	// remove a category
	//ICategory *pElement = m_pMoodManager->GetCategoryPtr(SHandle);
	ICategory *pCategory = (ICategory*)m_treeCtrl.GetItemData( hItem );

	if (pCategory)
	{
		//ICategory *pParent = pCategory->GetParentCategory();

		//if (pParent)
		//	pParent->RemoveCategory(pCategory->GetName());
		//else
		//	m_pMoodManager->RemoveMood(pCategory->GetMoodName());
	}
	else
	{
		// remove a mood
		CString sItemName = m_treeCtrl.GetItemText(hItem);
		m_pMoodManager->RemoveMood(sItemName.GetBuffer());
	}


	Update();
}

void CSoundMoodDlg::OnScan()
{
	if (!m_pMoodManager)
		return;

	//HTREEITEM current = m_treeCtrl.GetSelectedItem(); // save current selection

	HTREEITEM hChildItem = m_treeCtrl.GetChildItem(0);
	while (hChildItem != NULL)
	{
		CString sMoodName = m_treeCtrl.GetItemText(hChildItem);
		m_pMoodManager->RefreshCategories(sMoodName.GetBuffer());	
		HTREEITEM hNextItem = m_treeCtrl.GetNextItem(hChildItem, TVGN_NEXT);
		hChildItem = hNextItem;
	}

	//HTREEITEM iter = m_treeCtrl.GetFirstVisibleItem();
	//	
	//m_treeCtrl.Select(iter,0);	
	//	
	//while (iter)
	//{

	//	CString sMoodName = m_treeCtrl.GetItemText(iter);
	//	m_pMoodManager->RefreshCategories(sMoodName.GetBuffer());	
	//	iter = m_treeCtrl.GetNextItem(iter,0);
	//}

	Update();
}


void CSoundMoodDlg::OnMuteOn()
{
	if (!m_pMoodManager)
		return;

	HTREEITEM hItem = m_treeCtrl.GetSelectedItem();
	if (!hItem)
		return;

	//CategoryHandle SHandle((uint32)m_treeCtrl.GetItemData( hItem ));
	//ICategory *pElement = m_pMoodManager->GetCategoryPtr(SHandle);
	ICategory *pElement = (ICategory*)m_treeCtrl.GetItemData( hItem );

	if (pElement)
		pElement->SetMuted(true);

	Update();
}

void CSoundMoodDlg::OnMuteOff()
{
	if (!m_pMoodManager)
		return;

	HTREEITEM hItem = m_treeCtrl.GetSelectedItem();
	if (!hItem)
		return;

	//CategoryHandle SHandle((uint32)m_treeCtrl.GetItemData( hItem ));
	//ICategory *pElement = m_pMoodManager->GetCategoryPtr(SHandle);
	ICategory *pElement = (ICategory*)m_treeCtrl.GetItemData( hItem );

	if (pElement)
		pElement->SetMuted(false);

	Update();
}

void CSoundMoodDlg::OnClose()
{
	//if (!m_pSoundMoodMgr->Save())
	//	AfxMessageBox("An error occured while saving sound-presets.", MB_ICONEXCLAMATION | MB_OK);

		CBaseLibraryDialog::OnClose();
}

void CSoundMoodDlg::OnDestroy()
{
	//if (!m_pSoundMoodMgr->Save();)
	//	AfxMessageBox("An error occured while saving sound-presets.", MB_ICONEXCLAMATION | MB_OK);

	CBaseLibraryDialog::OnDestroy();
}

void CSoundMoodDlg::OnSize(UINT nType, int cx, int cy)
{
	// resize splitter window.
		if (m_wndVSplitter.m_hWnd)
	{
		CRect rc;
		GetClientRect(rc);
		m_wndVSplitter.MoveWindow(rc,FALSE);
	}

	CBaseLibraryDialog::OnSize(nType, cx, cy);
}
