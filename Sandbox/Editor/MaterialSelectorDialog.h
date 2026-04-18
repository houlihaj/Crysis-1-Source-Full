//---------------------------------------------------------------------------
// Copyright 2006 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------

#pragma once

#include "resource.h"
#include "Material/MaterialBrowser.h"
#include "Material/MaterialImageListCtrl.h"

// CMaterialSelectorDialog dialog

class CMaterialSelectorDialog : public CDialog
{
	DECLARE_DYNAMIC(CMaterialSelectorDialog)

public:
	CMaterialSelectorDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CMaterialSelectorDialog();

	void SetMaterialName(const char* szMaterialName);
	const char* GetMaterialName();

// Dialog Data
	enum { IDD = IDD_MATERIAL_SELECTOR };

protected:
	class CMaterialBrowserListener : public IMaterialBrowserListener
	{
	public:
		CMaterialBrowserListener(CString& sMaterialName, CEdit& materialNameEdit, CMaterialImageListCtrl& matrialImageListCtrl);
		virtual void OnBrowserSelectItem(IDataBaseItem* pItem, bool bForce);

	private:
		CString& sMaterialName;
		CEdit& materialNameEdit;
		CMaterialImageListCtrl& matrialImageListCtrl;
	};

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	afx_msg void OnCancel();

	DECLARE_MESSAGE_MAP()

	BOOL OnInitDialog();

	CMaterialBrowserCtrl m_materialBrowserCtrl;
	CStatic m_materialBrowserParent;
	CEdit m_materialNameEdit;

	CString sMaterialName;
	CMaterialBrowserListener browserListener;
	CStatic m_materialImageListParent;
	CMaterialImageListCtrl m_matrialImageListCtrl;
};
