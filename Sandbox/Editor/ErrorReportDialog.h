////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   errorreportdialog.h
//  Version:     v1.00
//  Created:     30/5/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __errorreportdialog_h__
#define __errorreportdialog_h__
#pragma once

// CErrorReportDialog dialog
#include <XTToolkitPro.h>
#include "ErrorReport.h"

class CErrorReportDialog : public CXTResizeDialog
{
	DECLARE_DYNAMIC(CErrorReportDialog)

public:
	CErrorReportDialog( CErrorReport *report,CWnd* pParent = NULL);   // standard constructor
	virtual ~CErrorReportDialog();

	static void Open( CErrorReport *pReport );
	static void Close();
	void CopyToClipboard();

	void SendInMail();
	void OpenInExcel();

// Dialog Data
	enum { IDD = IDD_ERROR_REPORT };

protected:
	virtual void OnOK();
	virtual void OnCancel();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual void PostNcDestroy();
	afx_msg void OnNMDblclkErrors(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnSelectObjects();
	afx_msg void OnSize( UINT nType,int cx,int cy );
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);

	afx_msg void OnReportItemClick(NMHDR * pNotifyStruct, LRESULT * result);
	afx_msg void OnReportItemRClick(NMHDR * pNotifyStruct, LRESULT * result);
	afx_msg void OnReportColumnRClick(NMHDR * pNotifyStruct, LRESULT * result);
	afx_msg void OnReportItemDblClick(NMHDR * pNotifyStruct, LRESULT * result);
	afx_msg void OnReportHyperlink(NMHDR * pNotifyStruct, LRESULT * result);
	afx_msg void OnReportKeyDown(NMHDR * pNotifyStruct, LRESULT * result);


	void ReloadErrors();

	DECLARE_MESSAGE_MAP()

	CErrorReport *m_pErrorReport;
	CImageList m_imageList;

	static CErrorReportDialog* m_instance;

	std::vector<CErrorRecord> m_errorRecords;

	CXTPReportControl m_wndReport;
	CXTPReportSubListControl m_wndSubList;
	CXTPReportFilterEditControl m_wndFilterEdit;
};

#endif // __errorreportdialog_h__
