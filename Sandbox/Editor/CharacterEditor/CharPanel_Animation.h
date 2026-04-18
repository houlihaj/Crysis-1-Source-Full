#if !defined(AFX_MODELVIEWPANEL2_H__3C441EB6_B02A_4D2F_A5E9_587E334E9F48__INCLUDED_)
#define AFX_MODELVIEWPANEL2_H__3C441EB6_B02A_4D2F_A5E9_587E334E9F48__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Controls\AnimSequences.h"
#include "Controls\FillSliderCtrl.h"
#include "Controls\MltiTree.h"


/////////////////////////////////////////////////////////////////////////////
// CCharAnimMultiTree
/////////////////////////////////////////////////////////////////////////////

class CharPanel_Animation;
class CCharAnimMultiTree : public CMultiTree
{
public:
	CCharAnimMultiTree();
	void SetParent(CharPanel_Animation * pCharAnimPanel);
private:
	CharPanel_Animation * m_pCharAnimPanel;
protected:
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	DECLARE_MESSAGE_MAP()
};



/////////////////////////////////////////////////////////////////////////////
// CharPanel_Animation dialog
/////////////////////////////////////////////////////////////////////////////

class ICharacterChangeListener;
class CharPanel_Animation : public CDialog
{

public:
	CharPanel_Animation( class CModelViewport *vp,CWnd* pParent = NULL);   // standard constructor
	virtual ~CharPanel_Animation();

	enum { IDD = IDD_CHARPANEL_ANIMATION };

	void SetFileName( const CString &filename );

	void DisableFileBroser() { m_browseFileBtn.EnableWindow(FALSE); }
	void AddAnimName( const CString &name, uint32 id );
	void ClearAnims();

	CString GetCurrFileName();
	CString GetCurrAnimName(int nNum = 0);
	bool GetAnimLightState();
	void ClearBones();
	void AddBone( const CString &bone );
	void SelectBone( const CString &bone );

	f32 GetBlendSpaceFlexX(f32 sec, f32 aft); 
	f32 GetBlendSpaceFlexY(f32 sec, f32 aft); 
	f32 GetBlendSpaceFlexZ(f32 sec, f32 aft); 
	f32 GetMotionInterpolationFlex(f32 sec, f32 aft); 

	f32 GetBlendSpaceSliderX() { return m_BlendSpaceSliderX.GetValue(); }
	f32 GetBlendSpaceSliderY() { return m_BlendSpaceSliderY.GetValue(); }
	f32 GetBlendSpaceSliderZ() { return m_BlendSpaceSliderZ.GetValue(); }
	f32 GetMotionInterpolationSlider() { return m_MotionInterpolationSlider.GetValue(); }


	void SetBlendSpaceSliderX(f32 val) { 
		m_BlendSpaceSliderX.SetValue(val); 
		m_BlendSpaceNumberX.SetValue(val);
	}
	void SetBlendSpaceSliderY(f32 val) { 
		m_BlendSpaceSliderY.SetValue(val);  
		m_BlendSpaceNumberY.SetValue(val);
	}
	void SetBlendSpaceSliderZ(f32 val) { 
		m_BlendSpaceSliderZ.SetValue(val);  
		m_BlendSpaceNumberZ.SetValue(val);
	}
	void SetMotionInterpolationSlider(f32 val) { 
		m_MotionInterpolationSlider.SetValue(val);  
		m_MotionInterpolationNumber.SetValue(val);
	}

	void EnableWindow_BlendSpaceSliderX(uint32 i)	{
		m_BlendSpaceSliderX.EnableWindow(i);
		m_BlendSpaceNumberX.EnableWindow(i);
	}
	void EnableWindow_BlendSpaceSliderY(uint32 i)	{
		m_BlendSpaceSliderY.EnableWindow(i);
		m_BlendSpaceNumberY.EnableWindow(i);
	}
	void EnableWindow_BlendSpaceSliderZ(uint32 i)	{
		m_BlendSpaceSliderZ.EnableWindow(i);
		m_BlendSpaceNumberZ.EnableWindow(i);
	}
	void EnableWindow_MotionInterpolationStatus(uint32 i)	{
		m_MotionInterpolationSlider.EnableWindow(i);
		m_MotionInterpolationNumber.EnableWindow(i);
	}



	int GetLayer() const { return m_layer.GetCurSel(); }

	bool GetAnimationDrivenMotion() { 	return m_AnimationDrivenMotion.GetState() & BST_CHECKED;	}
	bool GetVerticalMovement() { 	return m_VerticalMovement.GetState() & BST_CHECKED;	}

	//animation flags
	bool GetMirrorAnimation() { return m_MirrorAnimation.GetState() & BST_CHECKED;	}
	bool GetLoopAnimation() { return m_LoopAnimation.GetState() & BST_CHECKED;	}
	bool GetRepeatLastKey() { return m_RepeatLastKey.GetState() & BST_CHECKED;	}
	bool GetVTimeWarping(){ return m_VTimeWarping.GetState() & BST_CHECKED;	}
	bool GetPartialBodyUpdate() { return m_PartialBodyUpdate.GetState() & BST_CHECKED;	}
	bool GetDisableMultilayer() { return m_DisableMultilayerAnimation.GetState() & BST_CHECKED;	}
	f32 GetTransitionTime() { return UseTransitionTime() ? m_TransitionTime.GetValue() : -1.0f; 	}
	bool UseTransitionTime(){ return m_UseTransitionTime.GetState() & BST_CHECKED;	}
	bool GetAllowAnimationRestart() { return m_AllowAnimationRestart.GetState() & BST_CHECKED;	}


	bool GetDesiredLocomotionSpeed() { return m_DesiredLocomotionSpeed.GetState() & BST_CHECKED;	}
	bool GetDesiredTurnSpeed() { return m_DesiredTurnSpeed.GetState() & BST_CHECKED;	}
	bool GetLockMoveBody() { return m_LockMoveBody.GetState() & BST_CHECKED;	}

	bool GetFootAnchoring(){	return m_FootAnchoring.GetState() & BST_CHECKED;	}
	bool GetLookIK() { return m_LookIK.GetState() & BST_CHECKED; }
	bool GetAimIK() { return m_AimIK.GetState() & BST_CHECKED; };


	bool GetLArmIK() { return m_LArmIK.GetState() & BST_CHECKED; }
	bool GetRArmIK() { return m_RArmIK.GetState() & BST_CHECKED; }
	bool GetLFootIK() { return m_LFootIK.GetState() & BST_CHECKED; }
	bool GetRFootIK() { return m_RFootIK.GetState() & BST_CHECKED; }

	bool GetGroundAlign() { return m_GroundAlign.GetState() & BST_CHECKED; }

	bool GetFixedCamera(){	return m_FixedCamera.GetState() & BST_CHECKED;	}
	bool GetAttachedCamera(){	return m_AttachedCamera.GetState() & BST_CHECKED;	}


	bool GetGamepad() { 	return m_Gamepad.GetState() & BST_CHECKED;	}
	bool GetLinearMorphSequence() { 	return m_LinearMorphSequence.GetState() & BST_CHECKED;	}
	bool GetUseMorphTargets() { 	return m_UseMorphTargets.GetState() & BST_CHECKED;	}
	f32 GetLinearMorphSlider() {	return m_LinearMorphSlider.GetValue();	}
	f32 GetLinearMorphSliderFlex(f32 sec, f32 aft); 

	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);

	void EnablePreset(bool bIsEnable);
	void SetPreset(CString name);
	int GetSelectedAnimations() const;

	CComboBox	m_layer;
	CColoredListBox	m_animations;

	CImageList m_imageListFiles;
	CCharAnimMultiTree m_tree_animations;
	HTREEITEM m_hPrevItem;

protected:

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK() {};
	virtual void OnCancel() {};
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedBrowseFile();
	afx_msg void OnBnClickedReloadFile();
	afx_msg void OnBnClickedMaterial();
	afx_msg void OnAnimateLights();

	afx_msg void OnSelect_Animations();
	afx_msg void OnTvnSelchangedTree(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnClickTree(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnTvnGetInfoTip(NMHDR *pNMHDR, LRESULT *pResult);

	afx_msg void OnButton_ChooseBaseCharacter();
	afx_msg void OnButton_StartSelectedAnimations();
	afx_msg void OnButton_SetAimPoses();

	afx_msg void OnButton_StopAnimationInLayer();
	afx_msg void OnButton_StopAnimationsAll();
	afx_msg void OnButton_ResetModel();

	afx_msg void OnButton_UseTransitionTime()	{	m_TransitionTime.EnableWindow (UseTransitionTime());	}

	afx_msg void OnButton_TransRot2000();
	afx_msg void OnButton_VerticalMovement();
	afx_msg void OnButton_VTimeWarping();
	afx_msg void OnButton_FootAnchoring();

	afx_msg void OnSlider_BlendSpaceX();
	afx_msg void OnSlider_BlendSpaceY();
	afx_msg void OnSlider_BlendSpaceZ();
	afx_msg void OnSlider_MotionInterpolation();
	afx_msg void OnSlider_ManualUpdate();

	afx_msg void OnSlider_AnimationSpeed();
	afx_msg void OnNumber_AnimationSpeed();

	afx_msg void OnLoop();
	afx_msg void OnButton_PlayerControl();
	afx_msg void OnButton_PathFollowing();
	afx_msg void OnButton_Idle2Move();
	afx_msg void OnButton_IdleStep();


	afx_msg void OnButton_LFootIK();
	afx_msg void OnButton_RFootIK();
	afx_msg void OnButton_LArmIK();
	afx_msg void OnButton_RArmIK();
	afx_msg void OnButton_GroundAlign();


	afx_msg void OnButton_LinearMorphSequence();
	afx_msg void OnSlider_LinearMorph();

	afx_msg void OnBnClickedRecalculate();
	afx_msg void OnPresetSelect();

	DECLARE_MESSAGE_MAP()

	CEdit m_fileEdit;
	CModelViewport *m_ModelViewPort;

	//CCustomButton m_materialBtn;
	CCustomButton m_reloadFileBtn;
	CCustomButton m_browseFileBtn;
	CCustomButton m_materialBtn;

	CCustomButton m_ChooseBaseCharacter;


	uint32 m_SelectedAnimations;
	CCustomButton m_StartSelectedAnimations;
	CCustomButton m_SetAimPoses;
	CCustomButton m_StopAnimationLayer;

	std::vector<f32> m_arrBlendSpacesFadeX;
	CFillSliderCtrl m_BlendSpaceSliderX;
	CNumberCtrl m_BlendSpaceNumberX;

	std::vector<f32> m_arrBlendSpacesFadeY;
	CFillSliderCtrl m_BlendSpaceSliderY;
	CNumberCtrl m_BlendSpaceNumberY;

	std::vector<f32> m_arrBlendSpacesFadeZ;
	CFillSliderCtrl m_BlendSpaceSliderZ;
	CNumberCtrl m_BlendSpaceNumberZ;

	std::vector<f32> m_arrMotionInterpolationFade;
	CFillSliderCtrl m_MotionInterpolationSlider;
	CNumberCtrl m_MotionInterpolationNumber;

	CButton m_DesiredLocomotionSpeed;
	CButton m_DesiredTurnSpeed;
	CButton m_LockMoveBody;


	// flags per instance
	CButton m_AnimationDrivenMotion;
	CButton m_VerticalMovement;

	// per animation flags
	CButton	m_MirrorAnimation;


	CButton	m_LoopAnimation;
	CButton	m_RepeatLastKey;
	CButton m_VTimeWarping;
	CButton m_PartialBodyUpdate;
	CButton m_DisableMultilayerAnimation;
	CButton m_UseTransitionTime; CNumberCtrl m_TransitionTime;
	CButton	m_AllowAnimationRestart;


	// flags for post-processing
	CButton	m_LookIK; 				CButton	m_AimIK;
	CButton	m_LArmIK;					CButton	m_RArmIK;
	CButton	m_LFootIK;				CButton	m_RFootIK;
	CButton m_FootAnchoring;	CButton	m_GroundAlign;



	CFillSliderCtrl m_AnimationSpeed;
	CNumberCtrl m_AnimationSpeedNumber;

	CButton m_Gamepad;
	CButton m_LinearMorphSequence;
	CButton m_UseMorphTargets;

	std::vector<f32> m_arrLinearMorphFade;
	CFillSliderCtrl m_LinearMorphSlider;
	CNumberCtrl m_LayerMotionInterpolationNumber;

	CComboBox	m_preset;

public:
	void SetBaseCharacter();
	CButton m_PlayControl;
	CButton m_FixedCamera;

	CButton m_PathFollowing;
	CButton m_AttachedCamera;

	CButton m_Idle2Move;
	CButton m_IdleStep;

};


#endif // !defined(AFX_MODELVIEWPANEL_H__3C441EB6_B02A_4D2F_A5E9_587E334E9F48__INCLUDED_)
