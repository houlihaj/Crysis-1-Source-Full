////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   PrefabPanel.h
//  Version:     v1.00
//  Created:     14/11/2003 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __PrefabPanel_h__
#define __PrefabPanel_h__
#pragma once

#include <XTToolkitPro.h>

#include "Controls\PickObjectButton.h"

class CBaseObject;
class CPrefabObject;
// CPrefabPanel dialog

class CPrefabPanel : public CXTResizeDialog, public IPickObjectCallback
{
	DECLARE_DYNCREATE(CPrefabPanel)

public:
	CPrefabPanel(CWnd* pParent = NULL);   // standard constructor
	virtual ~CPrefabPanel();

	void SetObject( CPrefabObject *object );

	// Dialog Data
	enum { IDD = IDD_PANEL_PREFAB };

protected:
	//////////////////////////////////////////////////////////////////////////
	// Override IPickObjectCallback
	virtual void OnPick( CBaseObject *picked );
	virtual bool OnPickFilter( CBaseObject *picked );
	virtual void OnCancelPick();
	//////////////////////////////////////////////////////////////////////////

	virtual void OnOK() {};
	virtual void OnCancel() {};

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	afx_msg void OnBnClickedExtractSelected();
	afx_msg void OnBnClickedExtractAll();
	afx_msg void OnBnClickedPrefab();
	afx_msg void OnBnClickedOpen();
	afx_msg void OnBnClickedClose();
	afx_msg void OnBnClickedPick();
	afx_msg void OnBnClickedRemove();
	afx_msg void OnBnClickedUpdatePrefab();
	afx_msg void OnBnClickedReloadPrefab();
	afx_msg void OnItemchangedObjects(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkObjects(NMHDR* pNMHDR, LRESULT* pResult);

	DECLARE_MESSAGE_MAP()

	void ReloadObjects();
	void ReloadListCtrl();

//////////////////////////////////////////////////////////////////////////
protected:
	//////////////////////////////////////////////////////////////////////////
	struct ChildRecord
	{
		_smart_ptr<CBaseObject> pObject;
		int level;
		bool bSelected;
	};
	void RecursivelyGetAllPrefabChilds( CBaseObject *obj,std::vector<ChildRecord> &childs,int level );

	
protected:
	CPrefabObject* m_object;

	CListCtrl m_listCtrl;
	CCustomButton m_btns[8];
	CCustomButton m_btnOpen;
	CCustomButton m_btnClose;
	CPickObjectButton m_pickButton;
	CStatic m_objectsText;
	CEdit m_prefabName;
	int m_type;
	
	std::vector<ChildRecord> m_objects;
};

#endif // __PrefabPanel_h__

