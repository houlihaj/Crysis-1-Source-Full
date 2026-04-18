// XTPSyntaxEditPropertiesPage.h : header file
//
// This file is a part of the XTREME TOOLKIT PRO MFC class library.
// (c)1998-2007 Codejock Software, All Rights Reserved.
//
// THIS SOURCE FILE IS THE PROPERTY OF CODEJOCK SOFTWARE AND IS NOT TO BE
// RE-DISTRIBUTED BY ANY MEANS WHATSOEVER WITHOUT THE EXPRESSED WRITTEN
// CONSENT OF CODEJOCK SOFTWARE.
//
// THIS SOURCE CODE CAN ONLY BE USED UNDER THE TERMS AND CONDITIONS OUTLINED
// IN THE XTREME SYNTAX EDIT LICENSE AGREEMENT. CODEJOCK SOFTWARE GRANTS TO
// YOU (ONE SOFTWARE DEVELOPER) THE LIMITED RIGHT TO USE THIS SOFTWARE ON A
// SINGLE COMPUTER.
//
// CONTACT INFORMATION:
// support@codejock.com
// http://www.codejock.com
//
/////////////////////////////////////////////////////////////////////////////

//{{AFX_CODEJOCK_PRIVATE
#if !defined(__XTPSYNTAXEDITPROPERTIESPAGE_H__)
#define __XTPSYNTAXEDITPROPERTIESPAGE_H__
//}}AFX_CODEJOCK_PRIVATE

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CXTPSyntaxEditPropertiesDlg;
class CXTPSyntaxEditView;

//{{AFX_CODEJOCK_PRIVATE

//===========================================================================
// CXTPSyntaxEditTipWnd window
//===========================================================================
class _XTP_EXT_CLASS CXTPSyntaxEditTipWnd : public CWnd
{
public:
	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	CXTPSyntaxEditTipWnd();
	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	virtual ~CXTPSyntaxEditTipWnd();
	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	virtual BOOL Create(CListBox* pListBox);
	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	virtual BOOL IsOwnerDrawn();
	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	virtual BOOL ShowTip(int iIndex);
	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	virtual BOOL HideTip();
	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	virtual COLORREF GetTextColor() const;
	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	virtual COLORREF GetBackColor() const;

protected:

	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	virtual BOOL RegisterWindowClass(HINSTANCE hInstance = NULL);
	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	virtual BOOL OwnerDrawTip(CDC* pDC, CRect rClient);
	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	virtual BOOL DrawTip(CDC* pDC, CRect rClient);
	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	virtual BOOL CalcItemRect(int iItem, CRect& rItem);

	//{{AFX_VIRTUAL(CXTPSyntaxEditTipWnd)
	//}}AFX_VIRTUAL

	//{{AFX_CODEJOCK_PRIVATE
	#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
	#endif
	//}}AFX_CODEJOCK_PRIVATE

	//{{AFX_MSG(CXTPSyntaxEditTipWnd)
	afx_msg void OnPaint();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg LRESULT OnNcHitTest(CPoint point);
	afx_msg void OnNcPaint();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

protected:
	int         m_iIndex;       // TODO
	UINT        m_uIDEvent1;    // TODO
	UINT        m_uIDEvent2;    // TODO
	CRect       m_rWindow;      // TODO
	CPoint      m_ptCursor;     // TODO
	CListBox*   m_pListBox;     // TODO

private:
	void SetTipTimer();
	void KillTipTimer();
};


//===========================================================================
// CXTPSyntaxEditTipListBox
//===========================================================================
class _XTP_EXT_CLASS CXTPSyntaxEditTipListBox : public CListBox
{
public:
	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	CXTPSyntaxEditTipListBox();
	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	int HitTest(LPPOINT pPoint = NULL) const;
	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	int HitTest(CPoint point, BOOL bIsClient = FALSE) const;
	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	int ShowTip(CPoint point, BOOL bIsClient = FALSE);
	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	BOOL SelChanged() const;

protected:
	//{{AFX_VIRTUAL(CXTPSyntaxEditTipListBox)
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	//}}AFX_VIRTUAL

protected:
	DWORD                   m_dwIdx;        // TODO
	CXTPSyntaxEditTipWnd    m_wndInfoTip;   // TODO
};


//===========================================================================
// Summary:
//===========================================================================
class _XTP_EXT_CLASS CXTPSyntaxEditTipComboBox : public CComboBox
{
public:
	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	CXTPSyntaxEditTipListBox& GetListBox();
protected:
	//{{AFX_MSG(CXTPSyntaxEditTipComboBox)
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnDestroy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:
	CXTPSyntaxEditTipListBox m_wndListBox;
};

AFX_INLINE CXTPSyntaxEditTipListBox& CXTPSyntaxEditTipComboBox::GetListBox() {
	return m_wndListBox;
}

//===========================================================================
// Summary:
//     TODO
//===========================================================================
class _XTP_EXT_CLASS CXTPSyntaxEditPropertiesPageEdit : public CPropertyPage
{
	DECLARE_DYNCREATE(CXTPSyntaxEditPropertiesPageEdit)

public:
	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Parameters:
	//     pParentDlg - TODO
	// -------------------------------------------------------------------
	CXTPSyntaxEditPropertiesPageEdit(CXTPSyntaxEditView* pEditView=NULL);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	virtual ~CXTPSyntaxEditPropertiesPageEdit();

	//{{AFX_DATA(CXTPSyntaxEditPropertiesPageEdit)
	enum { IDD = XTP_IDD_EDIT_PAGEEDITOR };
	BOOL    m_bAutoReload;
	BOOL    m_bHorzScrollBar;
	BOOL    m_bVertScrollBar;
	BOOL    m_bSyntaxColor;
	BOOL    m_bAutoIndent;
	BOOL    m_bSelMargin;
	BOOL    m_bLineNumbers;
	int     m_nCaretStyle;
	int     m_nTabType;
	int     m_nTabSize;
	CButton m_btnRadioSpaces;
	CButton m_btnRadioTab;
	CButton m_btnRadioCaretThin;
	CButton m_btnRadioCaretThick;
	//}}AFX_DATA

	//{{AFX_VIRTUAL(CXTPSyntaxEditPropertiesPageEdit)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	public:
	virtual BOOL OnApply();
	//}}AFX_VIRTUAL

protected:

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	BOOL ReadRegistryValues();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	BOOL WriteRegistryValues();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	void SetModified(BOOL bChanged = TRUE);

	//{{AFX_MSG(CXTPSyntaxEditPropertiesPageEdit)
	virtual BOOL OnInitDialog();
	afx_msg void OnChkAutoReload();
	afx_msg void OnChkHorzScrollBar();
	afx_msg void OnChkVertScrollBar();
	afx_msg void OnChkSyntaxColor();
	afx_msg void OnChkAutoIndent();
	afx_msg void OnChkSelMargin();
	afx_msg void OnChkLineNumbers();
	afx_msg void OnChangeTabsSize();
	afx_msg void OnTabsSpaces();
	afx_msg void OnTabsTab();
	afx_msg void OnCaretThin();
	afx_msg void OnCaretThick();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

protected:
	BOOL                m_bModified;    // TODO
	CXTPSyntaxEditView* m_pEditView;    // TODO
};

//===========================================================================
// Summary:
//     TODO
//===========================================================================
class _XTP_EXT_CLASS CXTPSyntaxEditPropertiesPageFont : public CPropertyPage
{
	DECLARE_DYNCREATE(CXTPSyntaxEditPropertiesPageFont)

public:
	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Parameters:
	//     pParentDlg - TODO
	// -------------------------------------------------------------------
	CXTPSyntaxEditPropertiesPageFont(CXTPSyntaxEditView* pEditView=NULL);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	virtual ~CXTPSyntaxEditPropertiesPageFont();

	// -------------------------------------------------------------------
	// Summary: TODO
	// -------------------------------------------------------------------
	CFont& GetEditFont();
	// -------------------------------------------------------------------
	// Summary: TODO
	// -------------------------------------------------------------------
	BOOL GetSafeLogFont(LOGFONT& lf);
	// -------------------------------------------------------------------
	// Summary: TODO
	// -------------------------------------------------------------------
	BOOL CreateSafeFontIndirect(CFont& editFont, const LOGFONT& lf);

	//{{AFX_DATA(CXTPSyntaxEditPropertiesPageFont)
	enum { IDD = XTP_IDD_EDIT_PAGEFONT };
	CXTPSyntaxEditColorComboBox m_wndComboHiliteText;
	CXTPSyntaxEditColorComboBox m_wndComboHiliteBack;
	CXTPSyntaxEditColorComboBox m_wndComboText;
	CXTPSyntaxEditColorComboBox m_wndComboBack;
	CXTPSyntaxEditColorSampleText   m_txtSampleSel;
	CXTPSyntaxEditColorSampleText   m_txtSample;
	CButton     m_btnCustomText;
	CButton     m_btnCustomBack;
	CButton     m_btnCustomHiliteText;
	CButton     m_btnCustomHiliteBack;
	CXTPSyntaxEditTipComboBox   m_wndComboScript;
	CXTPSyntaxEditTipComboBox   m_wndComboStyle;
	CXTPSyntaxEditTipComboBox   m_wndComboSize;
	CXTPSyntaxEditTipComboBox   m_wndComboName;
	BOOL    m_bStrikeOut;
	BOOL    m_bUnderline;
	CString m_csName;
	CString m_csSize;
	CString m_csStyle;
	COLORREF m_crHiliteText;
	COLORREF m_crHiliteBack;
	COLORREF m_crText;
	COLORREF m_crBack;
	//}}AFX_DATA

	//{{AFX_VIRTUAL(CXTPSyntaxEditPropertiesPageFont)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	public:
	virtual BOOL OnApply();
	//}}AFX_VIRTUAL

protected:

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	void InitFontCombo();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	void InitStyleCombo();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	void InitSizeCombo();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	void InitScriptCombo();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	void InitColorComboxes();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	void UpdateSampleFont();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	void UpdateSampleColors();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Returns:
	//     TODO
	// -------------------------------------------------------------------
	int GetLBText(CComboBox& comboBox, CString& csItemText);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	BOOL ReadRegistryValues();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	BOOL WriteRegistryValues();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	void SetModified(BOOL bChanged = TRUE);

	//{{AFX_MSG(CXTPSyntaxEditPropertiesPageFont)
	virtual BOOL OnInitDialog();
	afx_msg void OnSelChangeComboNames();
	afx_msg void OnSelChangeComboStyles();
	afx_msg void OnSelChangeComboSizes();
	afx_msg void OnChkStrikeOut();
	afx_msg void OnChkUnderline();
	afx_msg void OnSelEndOkScript();
	afx_msg void OnBtnCustomText();
	afx_msg void OnBtnCustomBack();
	afx_msg void OnBtnCustomHiliteText();
	afx_msg void OnBtnCustomtHiliteBack();
	afx_msg void OnSelEndOkHiliteText();
	afx_msg void OnSelEndOkHiliteBack();
	afx_msg void OnSelEndOkText();
	afx_msg void OnSelEndOkBack();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

protected:
	const UINT m_uFaceSize;

	BYTE                m_iCharSet;     // TODO
	BOOL                m_bModified;    // TODO
	CXTPSyntaxEditView* m_pEditView;    // TODO
	CFont               m_editFont;     // TODO
};


//---------------------------------------------------------------------------

AFX_INLINE CFont& CXTPSyntaxEditPropertiesPageFont::GetEditFont() {
	return m_editFont;
}

//===========================================================================
// Summary:
//     TODO
//===========================================================================
class _XTP_EXT_CLASS CXTPSyntaxEditPropertiesPageColor : public CPropertyPage
{
	DECLARE_DYNCREATE(CXTPSyntaxEditPropertiesPageColor)

public:
	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Parameters:
	//     pParentDlg - TODO
	// -------------------------------------------------------------------
	CXTPSyntaxEditPropertiesPageColor(CXTPSyntaxEditView* pEditView=NULL);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	virtual ~CXTPSyntaxEditPropertiesPageColor();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	BOOL WriteRegistryValues();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	void SetModified(BOOL bChanged = TRUE);

	//{{AFX_DATA(CXTPSyntaxEditPropertiesPageColor)
	enum { IDD = XTP_IDD_EDIT_PAGECOLOR };
	CXTPSyntaxEditColorSampleText   m_txtSampleSel;
	CXTPSyntaxEditColorSampleText   m_txtSample;
	CXTPSyntaxEditColorComboBox m_wndComboHiliteText;
	CXTPSyntaxEditColorComboBox m_wndComboHiliteBack;
	CXTPSyntaxEditColorComboBox m_wndComboText;
	CXTPSyntaxEditColorComboBox m_wndComboBack;
	CButton     m_btnBold;
	CButton     m_btnItalic;
	CButton     m_btnUnderline;
	CButton     m_btnCustomText;
	CButton     m_btnCustomBack;
	CButton     m_btnCustomHiliteText;
	CButton     m_btnCustomHiliteBack;
	CXTPSyntaxEditTipListBox    m_lboxName;
	CXTPSyntaxEditTipListBox    m_lboxProp;
	CStatic     m_gboxSampleText;
	BOOL    m_bBold;
	BOOL    m_bItalic;
	BOOL    m_bUnderline;
	COLORREF m_crHiliteText;
	COLORREF m_crHiliteBack;
	COLORREF m_crText;
	COLORREF m_crBack;
	//}}AFX_DATA

	//{{AFX_VIRTUAL(CXTPSyntaxEditPropertiesPageColor)
	public:
	virtual BOOL OnSetActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	public:
	virtual BOOL OnApply();
	//}}AFX_VIRTUAL

protected:

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	void UpdateSampleColors();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	void UpdateFont();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	BOOL InitSchemaClasses(XTP_EDIT_SCHEMAFILEINFO* pSchemaInfo);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	void InitClassData(const XTP_EDIT_LEXCLASSINFO& infoClass);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	BOOL IsTopLevelClass(const XTP_EDIT_LEXCLASSINFO& infoClass);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	void EnableControls();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	CString GetPropValue(const XTP_EDIT_LEXPROPINFO& infoProp) const;

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	void SetDefaults();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	int PropExists(CXTPSyntaxEditLexPropInfoArray& arrProp, LPCTSTR lpszPropName);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	BOOL UpdateFontValue(BOOL& bValue, LPCTSTR lpszPropName);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	BOOL UpdateColorValue(CXTPSyntaxEditColorComboBox& combo, COLORREF& color, LPCTSTR lpszPropName);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	CString GetDisplayName(const XTP_EDIT_LEXCLASSINFO& info) const;

	//{{AFX_MSG(CXTPSyntaxEditPropertiesPageColor)
	virtual BOOL OnInitDialog();
	afx_msg void OnBtnCustomText();
	afx_msg void OnBtnCustomBack();
	afx_msg void OnBtnCustomHiliteText();
	afx_msg void OnBtnCustomtHiliteBack();
	afx_msg void OnChkBold();
	afx_msg void OnChkItalic();
	afx_msg void OnChkUnderline();
	afx_msg void OnSelEndOkHiliteText();
	afx_msg void OnSelEndOkHiliteBack();
	afx_msg void OnSelEndOkText();
	afx_msg void OnSelEndOkBack();
	afx_msg void OnSelChangeSchemaNames();
	afx_msg void OnSelChangeSchemaProp();
	afx_msg void OnDblClickSchema();
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

protected:
	BOOL                                    m_bModified;        // TODO
	CXTPSyntaxEditView*                     m_pEditView;        // TODO
	CXTPSyntaxEditConfigurationManagerPtr   m_ptrConfigMgr;     // TODO
	CXTPSyntaxEditTextSchemesManager*       m_pTextSchemesMgr;  // TODO
	CFont                                   m_editFont;         // TODO
	CMapStringToPtr                         m_mapLexClassInfo;
	CXTPSyntaxEditLexClassInfoArray*        m_parLexClassInfo;
};

//}}AFX_CODEJOCK_PRIVATE

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(__XTPSYNTAXEDITPROPERTIESPAGE_H__)
