#if !defined(AFX_CUSTOMCOLORDIALOG_H__E1491910_A0B4_48B3_9556_CB079E1EAC7F__INCLUDED_)
#define AFX_CUSTOMCOLORDIALOG_H__E1491910_A0B4_48B3_9556_CB079E1EAC7F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// CustomColorDialog.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CCustomColorDialog dialog

class CCustomColorDialog : public CColorDialog
{
	DECLARE_DYNAMIC(CCustomColorDialog)
public:
	typedef Functor1<COLORREF> ColorChangeCallback;

	CCustomColorDialog(COLORREF clrInit = 0, DWORD dwFlags = 0,CWnd* pParentWnd = NULL);
	~CCustomColorDialog();

	void SetColorChangeCallback( ColorChangeCallback cb ) { m_callback = cb; };

protected:
	//{{AFX_MSG(CCustomColorDialog)
	afx_msg void OnPickColor();
	virtual BOOL OnInitDialog();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	void PickMode( bool bEnable );
	void OnColorChange( COLORREF col );

	CButton m_pickColor;
	bool m_bPickMode;
	HCURSOR m_pickerCusror;
	ColorChangeCallback m_callback;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CUSTOMCOLORDIALOG_H__E1491910_A0B4_48B3_9556_CB079E1EAC7F__INCLUDED_)
