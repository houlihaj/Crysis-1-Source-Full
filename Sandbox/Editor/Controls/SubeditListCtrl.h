#if !defined(AFX_SUBEDITLISTVIEW_H__8F13463E_50C9_4411_AAE2_A2BE4E9EA46F__INCLUDED_)
#define AFX_SUBEDITLISTVIEW_H__8F13463E_50C9_4411_AAE2_A2BE4E9EA46F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// SubeditListView.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CSubeditListCtrl view

class CSubEdit : public CEdit
{
// Construction
public:
	CSubEdit();

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSubEdit)
	//}}AFX_VIRTUAL

// Implementation
public:
	int m_x;
	int m_cx;
	virtual ~CSubEdit();

	// Generated message map functions
protected:
	//{{AFX_MSG(CSubEdit)
	afx_msg void OnWindowPosChanging(WINDOWPOS FAR* lpwndpos);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

class CSubeditListCtrl : public CListCtrl
{
	class ComboBoxEditSession : public CComboBox
	{
	public:
		enum {ID_COMBO_BOX = 40000};

		ComboBoxEditSession();

		bool IsRunning() const;
		void Begin(CSubeditListCtrl* list, int itemIndex, int subItemIndex, const std::vector<string>& options, const string& currentOption);
		void End(bool accept);

		afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
		DECLARE_MESSAGE_MAP()

	private:
		CSubeditListCtrl* m_list;
		int m_itemIndex;
		int m_subItemIndex;
	};

protected:

	CSubeditListCtrl();           // protected constructor used by dynamic creation

// Attributes
public:

	enum EditStyle
	{
		EDIT_STYLE_NONE,
		EDIT_STYLE_EDIT,
		EDIT_STYLE_COMBO
	};

// Operations
public:
	CSubEdit m_editWnd;
	int m_item;
	int m_subitem;


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSubeditListCtrl)
	public:
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CSubeditListCtrl();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CSubeditListCtrl)
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnBeginLabelEdit(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEndLabelEdit(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnComboSelChange();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	virtual void TextChanged(int item, int subitem, const char* szText);
	virtual void GetOptions(int item, int subitem, std::vector<string>& options, string& currentOption) = 0;
	virtual EditStyle GetEditStyle(int item, int subitem) = 0;

	void EditLabelCombo(int itemIndex, int subItemIndex, const std::vector<string>& options, const string& currentOption);

	ComboBoxEditSession m_comboBoxEditSession;
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SUBEDITLISTVIEW_H__8F13463E_50C9_4411_AAE2_A2BE4E9EA46F__INCLUDED_)
