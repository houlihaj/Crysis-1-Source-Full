#pragma once

class CBoneComboBoxManager;

class CAttachmentsDlg : public CDialog
{

public:
	DECLARE_DYNAMIC(CAttachmentsDlg)

	CAttachmentsDlg( CWnd* pParent = NULL )	: CDialog(CAttachmentsDlg::IDD, pParent)	
	{
		m_AttachmentType=0;
		CharacterChanged=0;
		m_pBoneComboBoxManager=0;
	}

	~CAttachmentsDlg();

	// Dialog Data
	enum { IDD = IDD_CHARACTER_EDITOR_ATTACHMENTS };

	void ClearBones();
	void AddBone( const CString &bone );
	void SelectBone( const CString &bone );
	void UpdateList();
	void ReloadAttachment();
	void OnOK() {};
	void OnCancel() {};
	CString GetBonenameFromWindow();

	afx_msg void OnBnClicked_NEW();
	afx_msg void OnBnClicked_RENAME();
	afx_msg void OnBnClicked_REMOVE();
	afx_msg void OnBnClicked_IMPORT();
	afx_msg void OnBnClicked_EXPORT();

	afx_msg void OnAttachmentSelect();
	afx_msg void OnClicked_FaceAttach();
	afx_msg void OnClicked_BoneAttach();
	afx_msg void OnClicked_SkinAttach();
	afx_msg void OnClicked_HideAttachment();
	afx_msg void OnClicked_PhysAttachment();
	afx_msg void OnClicked_AlignBoneAttachment();

	afx_msg void OnBnClicked_FILEBROWSE();
	afx_msg void OnBnClicked_CLEAR();
	afx_msg void OnBnClicked_APPLY();
	afx_msg void OnBnClicked_MATERIALBROWSE();
	afx_msg void OnBnClicked_DFLTMATERIAL();

	afx_msg void OnHingeSelect();
	afx_msg void OnLimitOrDampingChange();
	afx_msg void OnBoneSelect();

	class CModelViewportCE *m_pModelViewportCE;
	CButton m_ButtonCLEAR;

	CButton m_ButtonRENAME;
	CButton m_ButtonREMOVE;
	CButton m_ButtonEXPORT;
	
	CButton m_browseObjectBtn;
	CComboBox m_boneName;
	CEdit m_objectName;
	CEdit m_materialName;
	CListBox m_attachmentsList;
	int m_AttachmentType;
	int m_AlignBoneAttachment;
	uint32 CharacterChanged;
	CComboBox m_hingeIndex;
	CNumberCtrl m_hingeLimit;
	CNumberCtrl m_hingeDamping;

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	struct IAttachment* GetSelected();
	CBoneComboBoxManager* m_pBoneComboBoxManager;

	DECLARE_MESSAGE_MAP()
};

