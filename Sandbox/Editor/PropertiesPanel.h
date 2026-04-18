#if !defined(AFX_PROPERTIESPANEL_H__AD3E2ECE_EFEB_4A4C_81A7_216B2BC11BC5__INCLUDED_)
#define AFX_PROPERTIESPANEL_H__AD3E2ECE_EFEB_4A4C_81A7_216B2BC11BC5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// PropertiesPanel.h : header file
//

#include "Controls\PropertyCtrl.h"

/////////////////////////////////////////////////////////////////////////////
// CPropertiesPanel dialog

class CPropertiesPanel : public CDialog
{
// Construction
public:
	CPropertiesPanel( CWnd* pParent = NULL );   // standard constructor

	typedef Functor1<IVariable*> UpdateCallback;

// Dialog Data
	//{{AFX_DATA(CPropertiesPanel)
	enum { IDD = IDD_PANEL_PROPERTIES };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

	void ReloadValues();

	void DeleteVars();
	void AddVars( class CVarBlock *vb,const UpdateCallback &func=NULL );

	void SetVarBlock( class CVarBlock *vb,const UpdateCallback &func=NULL );

	void SetMultiSelect( bool bEnable );
	bool IsMultiSelect() const { return m_multiSelect; };

	//////////////////////////////////////////////////////////////////////////
	// Helper functions.
	//////////////////////////////////////////////////////////////////////////
	template <class T>
	void SyncValue( CSmartVariable<T> &var,T& value,bool bCopyToUI,IVariable *pSrcVar=NULL )
	{
		if (bCopyToUI)
			var = value;
		else
		{
			if (!pSrcVar || pSrcVar == var.GetVar())
				value = var;
		}
	}
	void SyncValueFlag( CSmartVariable<bool> &var,int &nFlags,int flag,bool bCopyToUI,IVariable *pVar=NULL )
	{
		if (bCopyToUI)
		{
			var = (nFlags & flag) != 0;
		}
		else
		{
			if (!pVar || var.GetVar() == pVar)
				nFlags = (var) ? (nFlags | flag) : (nFlags & (~flag));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void AddVariable( CVariableBase &varArray,CVariableBase &var,const char *varName,unsigned char dataType=IVariable::DT_SIMPLE )
	{
		if (varName)
			var.SetName(varName);
		var.SetDataType(dataType);
		varArray.AddChildVar(&var);
	}
	//////////////////////////////////////////////////////////////////////////
	void AddVariable( CVarBlock *vars,CVariableBase &var,const char *varName,unsigned char dataType=IVariable::DT_SIMPLE )
	{
		if (varName)
			var.SetName(varName);
		var.SetDataType(dataType);
		vars->AddVariable(&var);
	}
	//////////////////////////////////////////////////////////////////////////

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPropertiesPanel)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};

	void OnPropertyChanged( IVariable *pVar );

	// Generated message map functions
	//{{AFX_MSG(CPropertiesPanel)
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	CPropertyCtrl m_wndProps;
	XmlNodeRef m_template;
	bool m_multiSelect;
	TSmartPtr<CVarBlock> m_varBlock;

	std::list<UpdateCallback> m_updateCallbacks;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PROPERTIESPANEL_H__AD3E2ECE_EFEB_4A4C_81A7_216B2BC11BC5__INCLUDED_)
