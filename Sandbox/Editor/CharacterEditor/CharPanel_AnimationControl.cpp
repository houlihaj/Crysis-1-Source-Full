// CharPanel_AnimationControl.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "CharPanel_AnimationControl.h"
#include "ICryAnimation.h"
#include "ModelViewportCE.h"
#include ".\charpanel_animationcontrol.h"

#include "AnimEventEditor/AnimEventEditor.h"
#include "AnimEventEditor/AnimEventListView.h"
#include "AnimEventEditor/AnimEventControlView.h"
#include "AnimEventEditor/AnimEventSelectParameterButtonView.h"
#include "AnimEventEditor/AnimEventEngineView.h"
#include <cderr.h>

enum {NUM_LAYERS = 4};

// CAnimationControlDlg dialog

IMPLEMENT_DYNAMIC(CAnimationControlDlg, CDialog)
CAnimationControlDlg::CAnimationControlDlg(CWnd* pParent /*=NULL*/)
: CDialog(CAnimationControlDlg::IDD, pParent),
	m_pModelViewportCE(0),
	m_bAnimationControlEnabled(true),
	m_eViewportPlayStatus(PLAY_NONE),
	m_pAnimEventEditor(0),
	m_pListView(0),
	m_pControlView(0),
	m_pSelectSoundButtonView(0),
	m_pSelectEffectButtonView(0),
	m_pEngineView(0)
{
	std::fill_n(m_hPlayPauseIcons, size_t(PLAY_Count), HICON(0));
}

CAnimationControlDlg::~CAnimationControlDlg()
{
	DestroyAnimEventEditor();
}

void CAnimationControlDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_PROGRESSSLIDER, m_ProgressSlider);
	DDX_Control(pDX, IDC_PROGRESSEDIT, m_ProgressEdit);
	DDX_Control(pDX, IDC_ANIMATION_NAME, m_AnimationNameEdit);
	DDX_Control(pDX, IDC_PROGRESSGROUPBOX, m_ProgressGroupBox);
	DDX_Control(pDX, IDC_PLAYPAUSE, m_PlayPauseButton);
	DDX_Control(pDX, IDC_EVENT_LIST, m_AnimEventListCtrl);
	DDX_Control(pDX, IDC_ADD_EVENT, m_NewEventButton);
	DDX_Control(pDX, IDC_SELECT_SOUND, m_SelectSoundButton);
	DDX_Control(pDX, IDC_USE_SELECTED_EFFECT, m_UseSelectedEffectButton);
	DDX_Control(pDX, IDC_DELETE_EVENTS, m_DeleteEventsButton);
}

BOOL CAnimationControlDlg::OnInitDialog()
{
	// Call the base class implementation.
	CDialog::OnInitDialog();

	// Set up the animation progress slider.
	m_ProgressSlider.SetRange(0, CProgressSlider::NUM_INCREMENTS);

	// Start listening to events from the slider.
	m_ProgressSlider.AddListener(this);

	// Remember the base caption of the progress group box.
	CString sWindowText;
	m_ProgressGroupBox.GetWindowText(sWindowText);
	m_sAnimationProgressGroupBoxBaseText = sWindowText.GetString();

	// Load the play/pause icons - note these are deliberately reversed, since when we are playing, we want the button
	// to pause, and vice versa.
	m_hPlayPauseIcons[PLAY_PLAY] = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_PAUSE), IMAGE_ICON, 16, 16, 0);
	m_hPlayPauseIcons[PLAY_PAUSE] = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_PLAY), IMAGE_ICON, 16, 16, 0);

	// Add columns to the list view.
	m_AnimEventListCtrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP | LVS_EX_HEADERDRAGDROP);
	m_AnimEventListCtrl.InsertColumn(0, "Name", LVCFMT_LEFT, 100);
	m_AnimEventListCtrl.InsertColumn(1, "Time", LVCFMT_LEFT, 60);
	m_AnimEventListCtrl.InsertColumn(2, "Parameter", LVCFMT_LEFT, 120);
	m_AnimEventListCtrl.InsertColumn(3, "Bone", LVCFMT_LEFT, 120);
	m_AnimEventListCtrl.InsertColumn(4, "Offset", LVCFMT_LEFT, 50);
	m_AnimEventListCtrl.InsertColumn(5, "Dir", LVCFMT_LEFT, 50);
	//m_AnimEventListCtrl.SetCallbackMask(LVIS_SELECTED);

	// Update the various controls.
	Update();

	return TRUE;
}

void CAnimationControlDlg::SetAnimationControlEnabled(bool bEnabled, const CString& sReason)
{
	if (m_bAnimationControlEnabled != bEnabled || m_sAnimationProgressStatusReason != sReason)
	{
		// Remember the message describing why we are disabled.
		m_sAnimationProgressStatusReason = sReason;

		// Display the reason for the status.
		CString sText = m_sAnimationProgressGroupBoxBaseText;
		if (!bEnabled)
		{
			sText += " - Disabled";
			if (!m_sAnimationProgressStatusReason.IsEmpty())
				sText += " (" + m_sAnimationProgressStatusReason + ")";
		}
		m_ProgressGroupBox.SetWindowText(sText);
	}

	if (m_bAnimationControlEnabled != bEnabled)
	{
		// Enable or disable the controls.
		m_ProgressSlider.EnableWindow(bEnabled ? TRUE : FALSE);
		m_ProgressEdit.EnableWindow(bEnabled ? TRUE : FALSE);
		m_NewEventButton.EnableWindow(bEnabled ? TRUE : FALSE);
		m_DeleteEventsButton.EnableWindow(bEnabled ? TRUE : FALSE);
		m_AnimEventListCtrl.EnableWindow(bEnabled ? TRUE : FALSE);
		//m_PlayPauseButton.EnableWindow(bEnabled ? TRUE : FALSE);

		m_bAnimationControlEnabled = bEnabled;
	}
}

void CAnimationControlDlg::Update()
{
	// Display the correct icon on the play/pause button.
	UpdatePlayStatus();

	// Update the display of the name of the animation.
	UpdateAnimationName();

	// Update the enabled-ness of the animation slider.
	UpdateAnimationControlEnabled();

	// Update the progress controls (uses m_bAnimationControlEnabled which is
	// set by CAnimationControlDlg::UpdateAnimationControlEnabled).
	UpdateAnimationProgress();
}

void CAnimationControlDlg::UpdateAnimationControlEnabled()
{
	// Check whether there is exactly one active animation - note that
	// CAnimationControlDlg::GetNumberOfActiveAnimations() returns -1
	// on failure.
	bool bEnabled = false;
	CString sReason = "";
	switch (GetNumberOfActiveAnimations())
	{
	case -1:
	case 0:
		bEnabled = false;
		sReason = "Unable to find active animation";
		break;
	case 1:
		bEnabled = true;
		break;
	default:
		bEnabled = false;
		sReason = "Multiple animations playing";
		break;
	}

	// Apply the enabled flag.
	SetAnimationControlEnabled(bEnabled, sReason);
}

int CAnimationControlDlg::GetNumberOfActiveAnimations()
{
	// Get the skeleton.
	ISkeletonAnim* pSkeletonAnim = GetSkeleton();

	// Return error if we can't find a skeleton.
	if (pSkeletonAnim == 0)
		return -1;

	// Loop over all the layers.
	int nNumLayers = 0;
	for (int nLayer = 0; nLayer < NUM_LAYERS; ++nLayer)
	{
		nNumLayers += pSkeletonAnim->GetNumAnimsInFIFO(nLayer);
	}

	// Return the number of animations.
	return nNumLayers;
}

void CAnimationControlDlg::UpdateAnimationProgress()
{
	if (!m_bAnimationControlEnabled)
		return;
	if (m_pModelViewportCE == 0)
		return;

	// Find the active animation.
	CAnimation* pAnimation = GetAnimation();
	if (pAnimation == 0)
		return;

	// Update the animation slider.
	DisplayProgress(pAnimation->m_fAnimTime);
}

ISkeletonAnim* CAnimationControlDlg::GetSkeleton()
{
	ISkeletonAnim* pSkeletonAnim = 0;
	if (m_pModelViewportCE != 0)
	{
		if (m_pModelViewportCE->GetCharacterBase() != 0)
			pSkeletonAnim = m_pModelViewportCE->GetCharacterBase()->GetISkeletonAnim();
	}
	return pSkeletonAnim;
}

CAnimation* CAnimationControlDlg::GetAnimation(int* pnLayerNumber)
{
	// Get the skeleton.
	ISkeletonAnim* pSkeletonAnim = GetSkeleton();

	// Return if we can't find a skeleton.
	if (pSkeletonAnim == 0)
		return 0;

	// Loop through all the layers - make sure we only find
	// one animation, otherwise return 0.
	CAnimation* pAnimation = 0;
	for (int nLayer = 0; nLayer < NUM_LAYERS; ++nLayer)
	{
		if (pSkeletonAnim->GetNumAnimsInFIFO(nLayer) > 1)
			return 0;
		if (pSkeletonAnim->GetNumAnimsInFIFO(nLayer) > 0)
		{
			if (pAnimation == 0)
			{
				pAnimation = &pSkeletonAnim->GetAnimFromFIFO(nLayer, 0);
				if (pnLayerNumber != 0)
					*pnLayerNumber = nLayer;
			}
			else
			{
				return 0;
			}
		}
	}

	return pAnimation;
}

void CAnimationControlDlg::DisplayProgress(float fProgress)
{
	// Update the slider control.
	if (!m_pModelViewportCE->GetPaused())
		m_ProgressSlider.SetPos(CProgressSlider::NUM_INCREMENTS * fProgress);

	// Update the edit control.
	if (CWnd::GetFocus() != &m_ProgressEdit)
	{
		CString text;
		text.Format( "%g",fProgress );
		m_ProgressEdit.SetWindowText(text);
	}
}

void CAnimationControlDlg::UpdatePlayStatus()
{
	// Get the pause status of the view port.
	PlayStatus ePlayStatus = PLAY_PLAY;
	if (m_pModelViewportCE != 0)
		ePlayStatus = m_pModelViewportCE->GetPaused() ? PLAY_PAUSE : PLAY_PLAY;

	if (m_eViewportPlayStatus != ePlayStatus)
	{
		// Update the icon of the play/pause button.
		m_PlayPauseButton.SetIcon(m_hPlayPauseIcons[ePlayStatus]);

		// Update the read-only flag of the progress edit box.
		m_ProgressEdit.SetReadOnly(ePlayStatus == PLAY_PAUSE ? FALSE : TRUE);

		m_eViewportPlayStatus = ePlayStatus;
	}
}

BEGIN_MESSAGE_MAP(CAnimationControlDlg, CDialog)
	ON_COMMAND(IDC_PLAYPAUSE, OnCommand_PlayPause)
	ON_COMMAND(IDC_ADD_EVENT, OnCommand_NewEvent)
	ON_COMMAND(IDC_SELECT_SOUND, OnCommand_SelectSound)
	ON_COMMAND(IDC_USE_SELECTED_EFFECT, OnCommand_SelectEffect)
	ON_COMMAND(IDC_DELETE_EVENTS, OnCommand_DeleteEvents)
	ON_COMMAND(IDC_SAVE_ANIM_EVENTS, OnCommand_SaveEvents)
	ON_COMMAND(IDC_LOAD_ANIM_EVENTS, OnCommand_LoadEvents)
	ON_WM_HSCROLL()
	ON_EN_KILLFOCUS(IDC_PROGRESSEDIT, OnEnKillfocusProgressedit)
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// CAnimationControlDlg message handlers

void CAnimationControlDlg::OnCommand_PlayPause()
{
	// Toggle the pause state of the view port.
	m_pModelViewportCE->SetPaused(!m_pModelViewportCE->GetPaused());

	if (m_pControlView != 0)
		m_pControlView->SetTime(float(m_ProgressSlider.GetPos()) / CProgressSlider::NUM_INCREMENTS);
}

void CAnimationControlDlg::OnCommand_NewEvent()
{
	// Get the current animation.
	CAnimation* pAnimation = GetAnimation();
	if (pAnimation == 0)
		return;
	ICharacterInstance* pCharacter = m_pModelViewportCE->GetCharacterBase();
	IAnimationSet* pIAnimationSet = (pCharacter ? pCharacter->GetIAnimationSet() : 0);
	if (pIAnimationSet == 0)
		return;

	if (m_pAnimEventEditor != 0)
	{
		// Get the name of the "animation" - this can either be an animation asset, or a locomotion group.
		string sAnimationIDName;
		if (pAnimation->m_LMG0.m_nLMGID >= 0)
			sAnimationIDName = pIAnimationSet->GetNameByAnimID(pAnimation->m_LMG0.m_nLMGID);
		else
			sAnimationIDName = pIAnimationSet->GetNameByAnimID(pAnimation->m_LMG0.m_nAnimID[0]);

		// Find the path from the animation name - there must be a view port with a character for us to have got here.
		sAnimationIDName = pIAnimationSet->GetFilePathByName(sAnimationIDName.c_str());

		m_pAnimEventEditor->AddEvent(sAnimationIDName, "New Event");
	}
}

void CAnimationControlDlg::OnCommand_SelectSound()
{
	if (m_pSelectSoundButtonView != 0)
		m_pSelectSoundButtonView->OnPressed();
}

void CAnimationControlDlg::OnCommand_SelectEffect()
{
	if (m_pSelectEffectButtonView != 0)
		m_pSelectEffectButtonView->OnPressed();
}

void CAnimationControlDlg::OnCommand_DeleteEvents()
{
	if (m_pAnimEventEditor != 0)
		m_pAnimEventEditor->DeleteEvents();
}

void CAnimationControlDlg::OnCommand_SaveEvents()
{
	SaveEvents();
}

void CAnimationControlDlg::OnCommand_LoadEvents()
{
	if (m_pAnimEventEditor == 0)
		return;

	CAutoRestoreMasterCDRoot adr;

	// First we should save the current events.
	CheckSaveEvents();

	bool bShouldLoad = true;

	// Get the default filename.
	char szFile[1024] = "";

	// Ask the user for the new filename.
	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = this->GetSafeHwnd();
	ofn.lpstrFilter = "Animation Event Files (*.animevents)\0*.animevents\0";
	ofn.lpstrFile = szFile;

	ofn.nMaxFile = sizeof(szFile);
	ofn.Flags = OFN_SHOWHELP|OFN_NOCHANGEDIR;
	ofn.lpstrTitle = "Choose file to load";

	int nDialogResult = GetOpenFileName(&ofn);
	if (nDialogResult == 0)
	{
		bShouldLoad = false;

		// Find out what really went wrong.
		switch (CommDlgExtendedError())
		{
		case 0:
			// The user cancelled the operation.
			break;

#define COMDLGERR(E) case E: CryMessageBox("Load dialog failed: " #E, "Dialog Failure", MB_OKCANCEL | MB_ICONERROR); break
			COMDLGERR(CDERR_DIALOGFAILURE);
			COMDLGERR(CDERR_FINDRESFAILURE);
			COMDLGERR(CDERR_INITIALIZATION);
			COMDLGERR(CDERR_LOADRESFAILURE);
			COMDLGERR(CDERR_LOADSTRFAILURE);
			COMDLGERR(CDERR_LOCKRESFAILURE);
			COMDLGERR(CDERR_MEMALLOCFAILURE);
			COMDLGERR(CDERR_MEMLOCKFAILURE);
			COMDLGERR(CDERR_NOHINSTANCE);
			COMDLGERR(CDERR_NOHOOK);
			COMDLGERR(CDERR_NOTEMPLATE);
			COMDLGERR(CDERR_STRUCTSIZE);
			COMDLGERR(FNERR_BUFFERTOOSMALL);
			COMDLGERR(FNERR_INVALIDFILENAME);
			COMDLGERR(FNERR_SUBCLASSFAILURE);
#undef COMDLGERR
		default:
			CryMessageBox("Save dialog failed: UNKNOWN", "Dialog Failure", MB_OKCANCEL | MB_ICONERROR);
			break;
		}
	}
	else
	{
		// Update the editor with the filename.
		CString sFileName = szFile;
		Path::ReplaceExtension(sFileName, "animevents");
		this->m_pAnimEventEditor->SetFileName(Path::GamePathToFullPath(sFileName).GetString());
	}

	// Load the file.
	if (bShouldLoad)
		LoadEvents();
}

void CAnimationControlDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if ((CProgressSlider*)pScrollBar == &m_ProgressSlider)
	{
		// Pause the animation, so that the animation isn't fighting the slider.
		m_pModelViewportCE->SetPaused(true);

		// Get the percentage of the animation from the slider.
		float fProgress = float(m_ProgressSlider.GetPos()) / CProgressSlider::NUM_INCREMENTS;

		// Update the animation progress to this value.
		SetAnimationProgress(fProgress);
	}
}

void CAnimationControlDlg::SetAnimationProgress(float fProgress)
{
	// Find the active animation.
	int nLayer;
	CAnimation* pAnimation = GetAnimation(&nLayer);
	ISkeletonAnim* pSkeletonAnim = GetSkeleton();
	if (pAnimation == 0 || pSkeletonAnim == 0)
		return;

	// Update the animation time - we have to convert from normalised time to
	// a time in seconds.
	float fProgressInSeconds = fProgress * pAnimation->m_LMG0.m_fDurationQQQ[0];
	pSkeletonAnim->SetLayerTime(nLayer, fProgressInSeconds);
}

void CAnimationControlDlg::OnEnKillfocusProgressedit()
{
	// Check whether this event indicates that the user has entered a new progress
	// time in the edit box - if so, the animation time should be updated.
	if (m_ProgressEdit.GetStyle() & ES_READONLY)
		return;
	if (!m_pModelViewportCE->GetPaused())
		return;

	// Try and parse the text of the edit box as a float - it must be in the range [0,1].
	CString sText;
	m_ProgressEdit.GetWindowText(sText);
	char* szEnd;
	double dValue = strtod(sText.GetString(), &szEnd);
	if (szEnd != sText.GetString() + strlen(sText.GetString()))
		return;

	// Clip the value to the valid range.
	if (dValue < 0.0)
		dValue = 0.0;
	if (dValue > 1.0)
		dValue = 1.0;

	// Set the progress to this value.
	SetAnimationProgress(float(dValue));

	m_ProgressSlider.SetPos(CProgressSlider::NUM_INCREMENTS * dValue);

	if (m_pControlView != 0)
		m_pControlView->SetTime(float(dValue));
}

void CAnimationControlDlg::UpdateAnimationName()
{
	if (m_pModelViewportCE==0)
		return;

	ICharacterInstance* pInstance = m_pModelViewportCE->GetCharacterBase();
	IAnimationSet* pIAnimationSet = (pInstance ? pInstance->GetIAnimationSet() : 0);
	if (pIAnimationSet == 0)
		return;

	// Get the animation, if there is one.
	CAnimation* pAnimation = GetAnimation();


	// Either display the name of the animation, or display a message explaining why we can't
	// show the name.
	CString sDialogOutput;
	CString sAssetFilePath;
	if (pAnimation != 0)
	{
		CString sAnimName;
		// Get the name of the "animation" - this can either be an animation asset, or a locomotion group.
		if (pAnimation->m_LMG0.m_nLMGID >= 0)
			sAnimName = pIAnimationSet->GetNameByAnimID(pAnimation->m_LMG0.m_nLMGID) ;//pAnimation->m_LMG0.m_strName[0];
		else
			sAnimName = pIAnimationSet->GetNameByAnimID(pAnimation->m_LMG0.m_nAnimID[0]);

		// Find the path from the animation name - there must be a view port with a character for us to have got here.
		ICharacterManager* pICharacterManager = m_pModelViewportCE->GetAnimationSystem();
		IAnimEvents* pIAnimEvents = pICharacterManager->GetIAnimEvents();
		ICharacterInstance* pICharacterInstance = m_pModelViewportCE->GetCharacterBase();
		IAnimationSet* pIAnimationSet = (pICharacterInstance ? pICharacterInstance->GetIAnimationSet() : 0);

		sAssetFilePath	= pIAnimationSet->GetFilePathByName(sAnimName.GetString());
		int32 GlobalID = pIAnimationSet->GetGlobalIDByName(sAnimName.GetString());

		uint32 numEvents0	= pIAnimEvents->GetAnimEventsCount(sAssetFilePath);
		uint32 numEvents1	= pIAnimEvents->GetAnimEventsCount(GlobalID);
		if (numEvents0)
		{
			uint32 i=0;
		}

		//text << " (" << std::setprecision(2) << std::setiosflags(std::ios_base::fixed) << pAnimation->m_LMG0.m_fDuration*pAnimation->m_fAnimTime << "/" << pAnimation->m_LMG0.m_fDuration << " secs)";
		sDialogOutput.Format( "%s (%.2f secs)", sAssetFilePath, pAnimation->m_LMG0.m_fDurationQQQ[0] * pAnimation->m_fAnimTime );
	}
	else
	{
		// Check how many animations there are.
		switch (GetNumberOfActiveAnimations())
		{
		case -1:
		case 0:
			sDialogOutput = "{no animation}";
			break;

		case 1:
			assert(0);
			break;

		default:
			sDialogOutput = "{multiple animations}";
			break;
		}
	}

	// Check whether the message needs to be updated.
	if (m_sAnimationName != sDialogOutput)
	{
		m_AnimationNameEdit.SetWindowText(sDialogOutput);
		m_sAnimationName = sDialogOutput;
	}

	if (m_sAnimationIDName != sAssetFilePath)
	{
		m_sAnimationIDName = sAssetFilePath;
		if (m_pAnimEventEditor != 0)
			m_pAnimEventEditor->SetAnim(m_sAnimationIDName);
	}
}

void CAnimationControlDlg::OnDestroy()
{
	CDialog::OnDestroy();
}

void CAnimationControlDlg::OnOK()
{
	// Don't call the base class implementation - somehow ok messages are being generated
	// in response to command messages - this would close the window unless we stop it here.
}

void CAnimationControlDlg::SliderDraggingFinished()
{
	if (m_pControlView != 0)
		m_pControlView->SetTime(float(m_ProgressSlider.GetPos()) / CProgressSlider::NUM_INCREMENTS);
}

void CAnimationControlDlg::OnAnimEventControlViewTimeChanged(float fTime)
{
	// Set the progress to this value.
	SetAnimationProgress(fTime);

	m_ProgressSlider.SetPos(CProgressSlider::NUM_INCREMENTS * fTime);
}

void CAnimationControlDlg::ModelChanged()
{
	// Save events if necessary.
	CheckSaveEvents();

	// Recreate the event editor.
	CreateAnimEventEditor();
}

void CAnimationControlDlg::CreateAnimEventEditor()
{
	DestroyAnimEventEditor();

	m_pAnimEventEditor = IAnimEventEditor::Create();
	m_pControlView = AnimEventControlView::Create();
	m_pSelectSoundButtonView = AnimEventSelectParameterButtonView::Create(m_SelectSoundButton, AnimEventSelectParameterButtonView::TYPE_SOUND);
	m_pSelectEffectButtonView = AnimEventSelectParameterButtonView::Create(m_UseSelectedEffectButton, AnimEventSelectParameterButtonView::TYPE_EFFECT);
	m_pControlView->AddListener(this);

	// Create a list of bones.
	std::vector<string> bones;
	ICharacterInstance* pICharacterInstance = m_pModelViewportCE->GetCharacterBase();
	if (pICharacterInstance)
	{
		ISkeletonPose* pSkeletonPose = pICharacterInstance->GetISkeletonPose();
		for (int boneIndex = 0; boneIndex < pSkeletonPose->GetJointCount(); ++boneIndex)
			bones.push_back(pSkeletonPose->GetJointNameByID(boneIndex));
	}

	m_pListView = AnimEventListView_Create(&m_AnimEventListCtrl, bones);
	m_pEngineView = new AnimEventEngineView(this->m_pModelViewportCE);
	m_pAnimEventEditor->AddView(m_pControlView);
	m_pAnimEventEditor->AddView(m_pSelectSoundButtonView);
	m_pAnimEventEditor->AddView(m_pSelectEffectButtonView);
	m_pAnimEventEditor->AddView(m_pListView);
	m_pAnimEventEditor->AddView(m_pEngineView);

	// Get the model, and find out where it stores anim events.
	CString sAnimEventFileName;
	if (m_pModelViewportCE->GetCharacterAnim() != 0)
		sAnimEventFileName = m_pModelViewportCE->GetCharacterAnim()->GetModelAnimEventDatabase();

	// Set the filename in the event editor.
	if (!sAnimEventFileName.IsEmpty())
	{
		m_pAnimEventEditor->SetFileName(Path::GamePathToFullPath(sAnimEventFileName).GetString());
		LoadEvents();
	}

}

void CAnimationControlDlg::DestroyAnimEventEditor()
{
	delete m_pControlView;
	m_pControlView = 0;
	delete m_pSelectSoundButtonView;
	m_pSelectSoundButtonView = 0;
	delete m_pSelectEffectButtonView;
	m_pSelectEffectButtonView = 0;
	delete m_pListView;
	m_pListView = 0;
	delete m_pAnimEventEditor;
	m_pAnimEventEditor = 0;
	delete m_pEngineView;
	m_pEngineView = 0;
}

void CAnimationControlDlg::LoadEvents()
{
	if (m_pAnimEventEditor != 0)
	{
		if (!this->m_pAnimEventEditor->Load())
			CryLog("Unable to open file: %s", this->m_pAnimEventEditor->GetErrorString());
		else
			m_pListView->Refresh(0);
	}
}

void CAnimationControlDlg::SaveEvents()
{
	if (m_pAnimEventEditor == 0)
		return;

	CAutoRestoreMasterCDRoot adr;

	bool bShouldSave = true;

	// Check whether we should prompt the user for a file name.
	if (m_pAnimEventEditor->GetFileName().empty())
	{
		// Get the default filename.
		char szFile[1024] = "";
		
		// Ask the user for the new filename.
		OPENFILENAME ofn;
		memset(&ofn, 0, sizeof(ofn));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = this->GetSafeHwnd();
		ofn.lpstrFilter = "Animation Event Files (*.animevents)\0*.animevents\0";
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.Flags = OFN_SHOWHELP;
		ofn.lpstrTitle = "Choose save file";

		int nDialogResult = GetSaveFileName(&ofn);
		if (nDialogResult == 0)
		{
			bShouldSave = false;

			// Find out what really went wrong.
			switch (CommDlgExtendedError())
			{
			case 0:
				// The user cancelled the operation.
				break;

#define COMDLGERR(E) case E: CryMessageBox("Save dialog failed: " #E, "Dialog Failure", MB_OKCANCEL | MB_ICONERROR); break
				COMDLGERR(CDERR_DIALOGFAILURE);
				COMDLGERR(CDERR_FINDRESFAILURE);
				COMDLGERR(CDERR_INITIALIZATION);
				COMDLGERR(CDERR_LOADRESFAILURE);
				COMDLGERR(CDERR_LOADSTRFAILURE);
				COMDLGERR(CDERR_LOCKRESFAILURE);
				COMDLGERR(CDERR_MEMALLOCFAILURE);
				COMDLGERR(CDERR_MEMLOCKFAILURE);
				COMDLGERR(CDERR_NOHINSTANCE);
				COMDLGERR(CDERR_NOHOOK);
				COMDLGERR(CDERR_NOTEMPLATE);
				COMDLGERR(CDERR_STRUCTSIZE);
				COMDLGERR(FNERR_BUFFERTOOSMALL);
				COMDLGERR(FNERR_INVALIDFILENAME);
				COMDLGERR(FNERR_SUBCLASSFAILURE);
#undef COMDLGERR
			default:
				CryMessageBox("Save dialog failed: UNKNOWN", "Dialog Failure", MB_OKCANCEL | MB_ICONERROR);
				break;
			}
		}
		else
		{
			// Update the editor with the filename.
			CString sFileName = szFile;
			sFileName = Path::ReplaceExtension(sFileName, "animevents");
			this->m_pAnimEventEditor->SetFileName(Path::GamePathToFullPath(sFileName).GetString());
		}
	}

	// Save the file.
	if (bShouldSave)
	{
		if (!this->m_pAnimEventEditor->Save())
			CryMessageBox("Unable to save file: " + this->m_pAnimEventEditor->GetErrorString(), "Error saving file", MB_ICONERROR | MB_OK);
	}
}

bool CAnimationControlDlg::CheckSaveEvents()
{
	// Check whether the document is dirty.
	if (m_pAnimEventEditor != 0 && m_pAnimEventEditor->IsDirty())
	{
		// MichaelS we can't cancel, since there is no way to stop the closing down at this stage - either the user saves or loses his work.
		int nResult = CryMessageBox("The animation events have changed - do you want to save them?", "Animation Events Changed", MB_ICONWARNING | MB_YESNO);
		switch (nResult)
		{
		case IDYES:
			SaveEvents();
			break;
		case IDNO:
			return false;
		default:
			break;
		}
	}
	return true;
}

bool CAnimationControlDlg::Close()
{
	// Save the data.
	return CheckSaveEvents();
}
