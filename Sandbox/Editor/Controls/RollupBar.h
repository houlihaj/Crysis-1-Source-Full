// RollupBar.h: interface for the CRollupBar class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ROLLUPBAR_H__4FB67648_FCE7_4827_A538_FE0D05FDE4C6__INCLUDED_)
#define AFX_ROLLUPBAR_H__4FB67648_FCE7_4827_A538_FE0D05FDE4C6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "RollupCtrl.h"
//#include <XTToolkitPro.h>

class CRollupBar : public CWnd
{
public:
	CRollupBar();
	virtual ~CRollupBar();

	void SetRollUpCtrl( int i,CRollupCtrl *pCtrl,const char *sTooltip );
	//! Select Object/Terrain
	void Select( int num );
	int GetSelection() { return m_selectedCtrl; };
	CRollupCtrl* GetCurrentCtrl();

protected:
		//{{AFX_MSG(CMyBar)
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
		afx_msg void OnTabSelect(NMHDR* pNMHDR, LRESULT* pResult);
	    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()

	CTabCtrl m_tabCtrl;
	//CXTPTabControl m_tabCtrl;
	CImageList m_tabImageList;
	std::vector<CRollupCtrl*> m_controls;
	int m_selectedCtrl;
};

#endif // !defined(AFX_ROLLUPBAR_H__4FB67648_FCE7_4827_A538_FE0D05FDE4C6__INCLUDED_)
