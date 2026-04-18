#pragma once

#include "BaseLibraryDialog.h"
#include "Controls\PropertyCtrl.h"

class  CSoundMoodMgr;
struct ISoundMoodManager;
struct ICategory;
struct IMood;

class CSoundMoodDlg : public CBaseLibraryDialog
{
	DECLARE_DYNAMIC(CSoundMoodDlg)

	enum ETreeItemType
	{
		EITEM_GROUP,
		EITEM_SUBGROUP,
		EITEM_SOUND,
	};
	//! Description of item in tree control.
	struct ItemDesc
	{
		ItemDesc *pParentItem;
		ETreeItemType type;
		//CMusicThemeLibItem *pItem;
		ICategory *pElement;
		
		//////////////////////////////////////////////////////////////////////////
		ItemDesc() : pParentItem(0),type(EITEM_GROUP), pElement(0) {};
		//ItemDesc( ItemDesc &item ) : pParentItem(&item),type(EITEM_SUBGROUP),pItem(item.pItem) {};
	};



protected:
		
	CSplitterWndEx m_wndVSplitter; // Vertical splitscreen

	ISoundMoodManager	*m_pMoodManager;
	CSoundMoodMgr *m_pSoundMoodMgr;
	//CListBox m_wndPresets;
	//CTreeCtrl m_wndSoundMoods;
	CPropertyCtrl m_wndParams;
	CVarBlockPtr m_pVars;

	CImageList m_filesImageList;

	int nWidthPresets;

protected:
	void InitToolbar( UINT nToolbarResID );
	void Update();

protected:
	virtual BOOL OnInitDialog();
	//virtual void PostNcDestroy() { delete this; };

public:
	CSoundMoodDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSoundMoodDlg();
	void OnParamsChanged(XmlNodeRef pNode);

	void OnCopy(){}
	void OnPaste() {}
	void SetActive( bool bActive ){}

// Dialog Data
	enum { IDD = IDD_DATABASE }; //IDD_DATABASE //IDD_EAXPRESETS

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	bool TreeShowMood(IMood *pMood, HTREEITEM hParent);
	bool TreeShowCategory(ICategory *pCategory, HTREEITEM hParent);

	DECLARE_MESSAGE_MAP()
public:
	afx_msg LRESULT OnUpdateProperties(WPARAM wParam, LPARAM lParam);
	//afx_msg void OnSelChangedItemTree();
	afx_msg void OnSelChangedItemTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnScan();
	afx_msg void OnSaveSoundMood();
	afx_msg void OnAddEntry();
	afx_msg void OnDelEntry();
	afx_msg void OnMuteOn();
	afx_msg void OnMuteOff();
	//afx_msg void OnToggleProp();

	afx_msg void OnClose();
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
};
