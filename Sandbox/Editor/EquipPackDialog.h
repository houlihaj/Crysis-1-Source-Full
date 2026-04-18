////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   equippackdialog.h
//  Version:     v1.00
//  Created:     27/06/2002 by Lennert Schneider.
//  Compilers:   Visual C++ 7.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Controls\PropertyCtrl.h"

#include "EquipPack.h"

struct IVariable;

typedef std::list<SEquipment>	TLstString;
typedef TLstString::iterator	TLstStringIt;

// CEquipPackDialog dialog

class CEquipPackDialog : public CXTResizeDialog
{
	DECLARE_DYNAMIC(CEquipPackDialog)

private:
	CWnd     m_AmmoPropWndParent;
	CPropertyCtrl m_AmmoPropWnd;
	CComboBox m_EquipPacksList;
	CListBox m_AvailEquipList;
	CListBox m_EquipList;
	CButton m_OkBtn;
	CButton m_ExportBtn;
	CButton m_AddBtn;
	CButton m_DeleteBtn;
	CButton m_RenameBtn;
	CButton m_InsertBtn;
	CButton m_RemoveBtn;
	CEdit   m_PrimaryEdit;

	TLstString m_lstAvailEquip;
	CString m_sCurrEquipPack;
	CVarBlockPtr m_pVarBlock;
	bool m_bChanged;
	bool m_bEditMode;

private:
	void UpdateEquipPacksList();
	void UpdateEquipPackParams();
	void UpdatePrimary();
	void AmmoUpdateCallback(IVariable *pVar);
	SEquipment* GetEquipment(CString sDesc);
	CVarBlockPtr CreateVarBlock(CEquipPack* pEquip);

public:
	void SetCurrEquipPack(CString &sValue) { m_sCurrEquipPack=sValue; }
	CString GetCurrEquipPack() { return m_sCurrEquipPack; }
	void SetEditMode(bool bEditMode) { m_bEditMode = bEditMode; }

public:
	CEquipPackDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CEquipPackDialog();
// Dialog Data
	enum { IDD = IDD_EQUIPPACKS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnCbnSelchangeEquippack();
	afx_msg void OnBnClickedAdd();
	afx_msg void OnBnClickedDelete();
	afx_msg void OnBnClickedRename();
	afx_msg void OnBnClickedInsert();
	afx_msg void OnBnClickedRemove();
	afx_msg void OnLbnSelchangeEquipavaillst();
	afx_msg void OnLbnSelchangeEquipusedlst();
	afx_msg void OnBnClickedImport();
	afx_msg void OnBnClickedExport();
	afx_msg void OnBnClickedMoveUp();
	afx_msg void OnBnClickedMoveDown();
	afx_msg void OnDestroy();
};
