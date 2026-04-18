#pragma once
#include "resource.h"
#include "ProgressSlider.h"
#include "Controls/ObjectListCtrl.h"
#include "AnimEventEditor/AnimEventControlView.h"


// CAnimationControlDlg dialog

class CModelViewportCE;
struct ISkeletonAnim;
struct CAnimation;
class IAnimEventEditor;
class IAnimEventView;
class AnimEventSelectParameterButtonView;
class AnimEventEngineView;

class CAnimationControlDlg : public CDialog, public ISliderListener, public IAnimEventControlViewListener
{
	DECLARE_DYNAMIC(CAnimationControlDlg)

public:
	CAnimationControlDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CAnimationControlDlg();

// Dialog Data
	enum { IDD = IDD_CHARACTER_EDITOR_ANIMATIONCONTROL };

	CModelViewportCE *m_pModelViewportCE;

	CProgressSlider m_ProgressSlider;
	CEdit m_ProgressEdit;
	CEdit m_AnimationNameEdit;
	CButton m_ProgressGroupBox;
	CButton m_PlayPauseButton;
	CObjectListCtrl m_AnimEventListCtrl;
	CButton m_NewEventButton;
	CButton m_SelectSoundButton;
	CButton m_UseSelectedEffectButton;
	CButton m_DeleteEventsButton;

	void Update();

	void ModelChanged();
	bool Close();

protected:
	enum PlayStatus
	{
		PLAY_NONE,
		PLAY_PAUSE,
		PLAY_PLAY,
		PLAY_Count
	};

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	void SetAnimationControlEnabled(bool bEnabled, const CString& sReason = "");
	void UpdateAnimationControlEnabled();
	int GetNumberOfActiveAnimations();
	void UpdateAnimationProgress();
	ISkeletonAnim* GetSkeleton();
	CAnimation* GetAnimation(int* pnLayerNumber=0);
	void DisplayProgress(float fProgress);
	void UpdatePlayStatus();
	void SetAnimationProgress(float fProgress);
	void UpdateAnimationName();
	void CreateAnimEventEditor();
	void DestroyAnimEventEditor();
	void LoadEvents();
	void SaveEvents();
	bool CheckSaveEvents();

	//virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	virtual void OnOK();

	virtual void SliderDraggingFinished();

	virtual void OnAnimEventControlViewTimeChanged(float fTime);

	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnCommand_PlayPause();
	afx_msg void OnCommand_NewEvent();
	afx_msg void OnCommand_SelectSound();
	afx_msg void OnCommand_SelectEffect();
	afx_msg void OnCommand_DeleteEvents();
	afx_msg void OnCommand_SaveEvents();
	afx_msg void OnCommand_LoadEvents();
	afx_msg void OnEnKillfocusProgressedit();
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()

private:
	bool m_bAnimationControlEnabled;
	CString m_sAnimationProgressGroupBoxBaseText;
	CString m_sAnimationProgressStatusReason;
	CString m_sAnimationName;
	CString m_sAnimationIDName;
	PlayStatus m_eViewportPlayStatus;
	HICON m_hPlayPauseIcons[PLAY_Count];
	IAnimEventEditor* m_pAnimEventEditor;
	AnimEventControlView* m_pControlView;
	AnimEventSelectParameterButtonView* m_pSelectSoundButtonView;
	AnimEventSelectParameterButtonView* m_pSelectEffectButtonView;
	AnimEventEngineView* m_pEngineView;
	IAnimEventView* m_pListView;
};
