#pragma once

#include "PropertiesPanel.h"
#include "ToolbarDialog.h"
#include "Controls\AnimSequences.h"
#include "Controls\RollupCtrl.h"

#include "CharPanel_Animation.h"
#include "CharPanel_BAttach.h"
#include "CharPanel_Attachments.h"
#include "CharPanel_Morphing.h"
#include "CharPanel_AnimationControl.h"
#include "CharPanel_Character.h"



//////////////////////////////////////////////////////////////////////////
class ICharacterChangeListener
{
public:
	virtual void OnCharacterChanged() = 0;
};

class CCharacterEditor : public CToolbarDialog, public IEditorNotifyListener, public ICharacterChangeListener
{
	DECLARE_DYNCREATE(CCharacterEditor)

public:
	static void RegisterViewClass();

	CCharacterEditor( CWnd* pParent =NULL) : CToolbarDialog(CCharacterEditor::IDD, pParent)
	{
		m_pResizePane = NULL;
		m_strCharDefPathName="NoPathDefined";
		m_pCharPanel_Preset = 0;
		m_pWndMenuBar = 0;
		Create( IDD,pParent );
		GetIEditor()->RegisterNotifyListener(this);
	}
	virtual ~CCharacterEditor();
	virtual BOOL PreTranslateMessage(MSG* pMsg);

	virtual void OnCharacterChanged();

	// Dialog Data
	enum { IDD = IDD_CHARACTER_EDITOR };


protected:
	void ResetCharEditor(ICharacterInstance* pCharacter); 
	void DoIdleUpdate();
	virtual void PostNcDestroy();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	virtual void OnEditorNotifyEvent( EEditorNotifyEvent event );

	void LoadCharacter(const CString& file, float scale = 1.0f);

	afx_msg void OnFileNew();
	afx_msg void OnFileLoad();
	afx_msg void OnFileSave();
	afx_msg void OnUpdateUIFileSave( CCmdUI* pCmdUI );
	afx_msg void OnFileSaveAs();
	afx_msg void OnUpdateUIFileSaveAs( CCmdUI* pCmdUI );

	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedExit();
	afx_msg void OnBnClickedBrowseFile();
	afx_msg void OnEnChangeEdit1();
	afx_msg void OnBnClickedButton3();
	afx_msg void OnTabSelChanged(NMHDR* pNMHDR, LRESULT* pResult);

	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);

	afx_msg void OnMoveMode();
	afx_msg void OnMoveModeUpdateUI( CCmdUI *pCmdUI );

	afx_msg void OnRotateMode();
	afx_msg void OnRotateModeUpdateUI( CCmdUI *pCmdUI );

	DECLARE_MESSAGE_MAP()

//CTabCtrl

	CXTTabCtrl m_TabSelect;
	CXTPMenuBar *m_pWndMenuBar;
	CWnd m_pane1;
	CWnd m_pane2;
	class CModelViewportCE *m_pModelViewportCE;
	class CharPanel_Preset * m_pCharPanel_Preset;
	CRollupCtrl m_rollupCtrl;
	CEdit m_fileEdit;
	CDlgToolBar m_toolbar;

	CAttachmentsDlg m_AttachmentsDlg;
	CMorphingDlg m_MorphingDlg;
	CAnimationControlDlg m_AnimationControlDlg;
	CCharacterPropertiesDlg m_CharacterPropertiesDlg;

	CXTResizeDialog* m_pResizePane;
	CPropertiesPanel* s_varsPanel;

	string m_strCharDefPathName;
//string sDefaultFileName

private:
};
