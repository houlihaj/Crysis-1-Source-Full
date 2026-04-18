// CharPanel_Animation.cpp : implementation file
//

#include "stdafx.h"
#include "Resource.h"
#include "AnimationInfoLoader.h"
#include <I3DEngine.h>
#include <ICryAnimation.h>
#include "ModelViewport.h"
#include "CryCharMorphParams.h"
#include "CharacterEditor.h"
#include "Material/MaterialManager.h"
#include "CharPanel_Preset.h"
#include "CharPanel_Animation.h"
#include "Clipboard.h"

#define RC_IDM_COPY_NAME	911

CCharAnimMultiTree::CCharAnimMultiTree()
{
	m_pCharAnimPanel = 0;
}

BEGIN_MESSAGE_MAP(CCharAnimMultiTree, CMultiTree)
	//{{AFX_MSG_MAP(CMultiTree)
	ON_WM_RBUTTONDOWN()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


void CCharAnimMultiTree::SetParent(CharPanel_Animation * pCharAnimPanel)
{
	m_pCharAnimPanel = pCharAnimPanel;
}

void CCharAnimMultiTree::OnRButtonDown(UINT nFlags, CPoint point) 
{
	if(m_pCharAnimPanel)
	{
		CString strAnimName = m_pCharAnimPanel->GetCurrAnimName();
		if(strlen(strAnimName))
		{
			CMenu menu;
			if (menu.CreatePopupMenu())
			{
				RECT rc;
				GetWindowRect(&rc);
				menu.AppendMenu(MF_STRING, RC_IDM_COPY_NAME, "Copy animation name" );
				menu.TrackPopupMenu(TPM_LEFTALIGN|TPM_LEFTBUTTON, rc.left+point.x, rc.top+point.y, m_pCharAnimPanel);
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
// CharPanel_Animation dialog
/////////////////////////////////////////////////////////////////////////////

enum animations_tree_icons
{
	AIMPOSE_ICON,
	FOLDER_ICON,
	LOOPING_ICON,
	LOOPINGIDLE_ICON,
	NONLOOPING_ICON,
	MISSING_ICON,
	ONDEMAND_ICON,
	LMG_ICON,
};

CharPanel_Animation::CharPanel_Animation( CModelViewport *vp,CWnd* pParent ) : CDialog(CharPanel_Animation::IDD, pParent)
{
	m_ModelViewPort = vp;
	Create(MAKEINTRESOURCE(IDD),pParent);

	m_DesiredLocomotionSpeed.SetCheck(0);
	m_DesiredLocomotionSpeed.EnableWindow(TRUE);
	m_DesiredTurnSpeed.SetCheck(0);
	m_DesiredTurnSpeed.EnableWindow(TRUE);
	m_LockMoveBody.SetCheck(1);
	m_LockMoveBody.EnableWindow(TRUE);

	m_AnimationDrivenMotion.SetCheck(0);
	m_VerticalMovement.SetCheck(0);

	//animation flags
	m_MirrorAnimation.SetCheck(0);
	m_MirrorAnimation.EnableWindow(TRUE);
	m_LoopAnimation.SetCheck(1);
	m_LoopAnimation.EnableWindow(TRUE);
	m_RepeatLastKey.SetCheck(0);
	m_RepeatLastKey.EnableWindow(TRUE);
	
	m_VTimeWarping.SetCheck(1);
	m_VTimeWarping.EnableWindow(TRUE);
	m_PartialBodyUpdate.SetCheck(0);
	m_PartialBodyUpdate.EnableWindow(TRUE);
	m_DisableMultilayerAnimation.SetCheck(0);
	m_DisableMultilayerAnimation.EnableWindow(TRUE);
	m_UseTransitionTime.SetCheck(0);
	m_UseTransitionTime.EnableWindow(TRUE);

	m_TransitionTime.Create( this, IDC_TRANSITION_TIME );
	m_TransitionTime.SetValue( 0.2f );
	m_TransitionTime.EnableWindow(FALSE);

	m_AllowAnimationRestart.SetCheck(0);
	m_AllowAnimationRestart.EnableWindow(TRUE);





	//post processing
	m_FootAnchoring.SetCheck(1);
	m_FootAnchoring.EnableWindow(TRUE);


	m_LArmIK.SetCheck(0);
	m_LArmIK.EnableWindow(TRUE);
	m_RArmIK.SetCheck(0);
	m_RArmIK.EnableWindow(TRUE);
	m_LFootIK.SetCheck(0);
	m_LFootIK.EnableWindow(TRUE);
	m_RFootIK.SetCheck(0);
	m_RFootIK.EnableWindow(TRUE);
	m_GroundAlign.SetCheck(0);
	m_GroundAlign.EnableWindow(TRUE);


	m_PlayControl.SetCheck(0);
	m_PlayControl.EnableWindow(TRUE);
	m_FixedCamera.SetCheck(0);
	m_FixedCamera.EnableWindow(TRUE);

	m_PathFollowing.SetCheck(0);
	m_PathFollowing.EnableWindow(TRUE);
	m_AttachedCamera.SetCheck(0);
	m_AttachedCamera.EnableWindow(TRUE);
	m_Idle2Move.SetCheck(0);
	m_Idle2Move.EnableWindow(TRUE);
	m_IdleStep.SetCheck(0);
	m_IdleStep.EnableWindow(TRUE);


	m_SelectedAnimations=0;
	//m_pCurBone = 0;
	uint32 numSample=0x100;

	m_arrBlendSpacesFadeX.resize(numSample);
	m_arrBlendSpacesFadeY.resize(numSample);
	m_arrBlendSpacesFadeZ.resize(numSample);
	m_arrMotionInterpolationFade.resize(numSample);
	m_arrLinearMorphFade.resize(numSample);
	for (uint32 i=0; i<numSample; i++)
	{
		m_arrBlendSpacesFadeX[i]=0.0f;
		m_arrBlendSpacesFadeY[i]=0.0f;
		m_arrBlendSpacesFadeZ[i]=0.0f;
		m_arrMotionInterpolationFade[i]=0.5f;
		m_arrLinearMorphFade[i]=0.0f;
	}

	m_Gamepad.SetCheck(0);
	m_LinearMorphSequence.SetCheck(0);
	m_UseMorphTargets.SetCheck(1);

	m_tree_animations.SetMultiSelect(true);
	m_tree_animations.SetCanCapture(false);
	m_tree_animations.SetParent(this);

	m_hPrevItem = 0;

	//m_animLoader = 0;
	//m_pBonePreset = 0;
}

CharPanel_Animation::~CharPanel_Animation()
{
	//if(m_animLoader)
		//delete m_animLoader;
	//m_animLoader = 0;
}



void CharPanel_Animation::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_FILE, m_fileEdit);
	DDX_Control(pDX, IDC_BROWSE_FILE, m_browseFileBtn);
	DDX_Control(pDX, IDC_RELOAD_FILE, m_reloadFileBtn);
	DDX_Control(pDX, IDC_MATERIAL, m_materialBtn);

	DDX_Control(pDX, IDC_TREEANIMATION, m_tree_animations);

	DDX_Control(pDX, IDC_CHOOSE_BASE_CHARACTER, m_ChooseBaseCharacter);

	DDX_Control(pDX, IDC_START_SELECTED_ANIMATIONS, m_StartSelectedAnimations);
	DDX_Control(pDX, IDC_SET_AIM_POSES, m_SetAimPoses );
	DDX_Control(pDX, IDC_STOP_ANIMATION_LAYER, m_StopAnimationLayer);

	DDX_Control(pDX, IDC_LAYER, m_layer);
	DDX_Control(pDX, IDC_PRESET, m_preset);
	DDX_Control(pDX, IDC_DESIRED_LOCOMOTION_SPEED, m_DesiredLocomotionSpeed);
	DDX_Control(pDX, IDC_DESIRED_TURN_SPEED, m_DesiredTurnSpeed);
	DDX_Control(pDX, IDC_LOCK_BODYMOVE, m_LockMoveBody);

	//instance flags
	DDX_Control(pDX, IDC_ANIMATION_DRIVEN_MOTION, m_AnimationDrivenMotion);
	DDX_Control(pDX, IDC_VERTICAL_MOVEMENT, m_VerticalMovement);

	//per animation flags
	DDX_Control(pDX, IDC_MIRROR_ANIMATION, m_MirrorAnimation);
	DDX_Control(pDX, IDC_LOOP_ANIMATION, m_LoopAnimation);
	DDX_Control(pDX, IDC_REPEAT_LAST_KEY, m_RepeatLastKey);
	DDX_Control(pDX, IDC_VTIME_WARPING, m_VTimeWarping);
	DDX_Control(pDX, IDC_PARTIAL_BODY_ANIMATION, m_PartialBodyUpdate);
	DDX_Control(pDX, IDC_DISABLE_MULTILAYER_ANIMATION, m_DisableMultilayerAnimation);
	DDX_Control(pDX, IDC_USE_TRANSITION_TIME, m_UseTransitionTime);
	DDX_Control(pDX, IDC_ALLOW_ANIMATION_RESTART, m_AllowAnimationRestart);

	//post processing
	DDX_Control(pDX, IDC_LOOKIK, m_LookIK);
	DDX_Control(pDX, IDC_AIMIK, m_AimIK);

	DDX_Control(pDX, IDC_LFOOTIK, m_LFootIK);
	DDX_Control(pDX, IDC_RFOOTIK, m_RFootIK);
	DDX_Control(pDX, IDC_LARMIK, m_LArmIK);
	DDX_Control(pDX, IDC_RARMIK, m_RArmIK);

	DDX_Control(pDX, IDC_FOOT_ANCHORING, m_FootAnchoring);
	DDX_Control(pDX, IDC_GROUNDALIGN, m_GroundAlign);



	DDX_Control(pDX, IDC_PLAYER_CONTROL, m_PlayControl);
	DDX_Control(pDX, IDC_FIXED_CAMERA, m_FixedCamera);

	DDX_Control(pDX, IDC_PATH_FOLLOWING, m_PathFollowing);
	DDX_Control(pDX, IDC_ATTACHED_CAM, m_AttachedCamera);

	DDX_Control(pDX, IDC_IDLE2MOVE, m_Idle2Move);
	DDX_Control(pDX, IDC_IDLESTEP, m_IdleStep);

	DDX_Control(pDX, IDC_USE_GAMEPAD, m_Gamepad);
	DDX_Control(pDX, IDC_LINEAR_MORPH_SEQUENCE, m_LinearMorphSequence);
	DDX_Control(pDX, IDC_USE_MORPHTARGETS, m_UseMorphTargets);

}


BEGIN_MESSAGE_MAP(CharPanel_Animation, CDialog)
	ON_WM_CREATE()
	ON_BN_CLICKED(IDC_BROWSE_FILE, OnBnClickedBrowseFile)
	ON_BN_CLICKED(IDC_RELOAD_FILE, OnBnClickedReloadFile)
	ON_BN_CLICKED(IDC_ANIMATE_LIGHTS, OnAnimateLights)
	ON_BN_CLICKED(IDC_MATERIAL, OnBnClickedMaterial)

	ON_BN_CLICKED(IDC_CHOOSE_BASE_CHARACTER, OnButton_ChooseBaseCharacter)

	ON_NOTIFY(NM_DBLCLK, IDC_TREEANIMATION, OnClickTree)
	ON_NOTIFY(TVN_SELCHANGED, IDC_TREEANIMATION, OnTvnSelchangedTree)
	ON_NOTIFY(TVN_GETINFOTIP, IDC_TREEANIMATION, OnTvnGetInfoTip)

	ON_CONTROL(WMU_FS_CHANGED, IDC_BLENDSPACESLIDER_X, OnSlider_BlendSpaceX)
	ON_CONTROL(WMU_FS_CHANGED, IDC_BLENDSPACESLIDER_Y, OnSlider_BlendSpaceY)
	ON_CONTROL(WMU_FS_CHANGED, IDC_BLENDSPACESLIDER_Z, OnSlider_BlendSpaceZ)
	ON_CONTROL(WMU_FS_CHANGED, IDC_INTERPOLATION_SLIDER, OnSlider_MotionInterpolation)


	ON_BN_CLICKED(IDC_ANIMATION_DRIVEN_MOTION, OnButton_TransRot2000)
	ON_BN_CLICKED(IDC_VERTICAL_MOVEMENT, OnButton_VerticalMovement)

	ON_BN_CLICKED(IDC_LOOP, OnLoop)
	ON_BN_CLICKED(IDC_VTIME_WARPING, OnButton_VTimeWarping)
	ON_BN_CLICKED(IDC_USE_TRANSITION_TIME, OnButton_UseTransitionTime)

	ON_BN_CLICKED(IDC_FOOT_ANCHORING, OnButton_FootAnchoring)

	ON_BN_CLICKED(IDC_LFOOTIK, OnButton_LFootIK)
	ON_BN_CLICKED(IDC_RFOOTIK, OnButton_RFootIK)
	ON_BN_CLICKED(IDC_LARMIK, OnButton_LArmIK)
	ON_BN_CLICKED(IDC_RARMIK, OnButton_RArmIK)
	ON_BN_CLICKED(IDC_GROUNDALIGN, OnButton_GroundAlign)


	ON_BN_CLICKED(IDC_START_SELECTED_ANIMATIONS, OnButton_StartSelectedAnimations)
	ON_BN_CLICKED(IDC_SET_AIM_POSES, OnButton_SetAimPoses)

	ON_BN_CLICKED(IDC_STOP_ANIMATION_LAYER, OnButton_StopAnimationInLayer)
	ON_BN_CLICKED(IDC_RESETANIMATIONS, OnButton_StopAnimationsAll)
	ON_BN_CLICKED(IDC_RESET_MODEL, OnButton_ResetModel)


	ON_BN_CLICKED(IDC_PLAYER_CONTROL, OnButton_PlayerControl)
	ON_BN_CLICKED(IDC_PATH_FOLLOWING, OnButton_PathFollowing)
	ON_BN_CLICKED(IDC_IDLE2MOVE, OnButton_Idle2Move)
	ON_BN_CLICKED(IDC_IDLESTEP, OnButton_IdleStep)


	ON_CONTROL(WMU_FS_CHANGED, IDC_ANIMATION_SPEED_SLIDER, OnSlider_AnimationSpeed)
	ON_EN_CHANGE(IDC_ANIMATION_SPEED_NUMBER, OnNumber_AnimationSpeed)

	ON_BN_CLICKED(IDC_RECALCULATE, OnBnClickedRecalculate)
	ON_BN_CLICKED(IDC_LINEAR_MORPH_SEQUENCE, OnButton_LinearMorphSequence)

	ON_CBN_SELCHANGE(IDC_PRESET, OnPresetSelect)

END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CharPanel_Animation message handlers

CString CharPanel_Animation::GetCurrFileName()
{
	CString filename;
	m_fileEdit.GetWindowText( filename );
	return filename;
}


CString CharPanel_Animation::GetCurrAnimName(int nNum)
{
	if(!nNum)
	{
		HTREEITEM hItem = m_tree_animations.GetFirstSelectedItem();
		if ((hItem != NULL) && !m_tree_animations.ItemHasChildren(hItem))
			return m_tree_animations.GetItemText(hItem);
	}
	if(nNum==1)
	{
		HTREEITEM hSelItem = m_tree_animations.GetSelectedItem();
		HTREEITEM hItem = m_tree_animations.GetFirstSelectedItem();
		while(hItem)
		{
			if(hItem != hSelItem)
				return m_tree_animations.GetItemText(hItem);
			hItem = m_tree_animations.GetNextSelectedItem(hItem);
		}
	}
	if(nNum==2)
	{
		HTREEITEM hSelItem = m_tree_animations.GetSelectedItem();
		HTREEITEM hItem = m_tree_animations.GetFirstSelectedItem();
		while(hItem)
		{
			if(hItem == hSelItem)
				return m_tree_animations.GetItemText(hItem);
			hItem = m_tree_animations.GetNextSelectedItem(hItem);
		}
	}
	return "";
}

void CharPanel_Animation::OnButton_StartSelectedAnimations()
{
	m_ModelViewPort->SendMessage( WM_COMMAND,MAKEWPARAM(ID_ANIM_PLAY,0),(LPARAM)GetSafeHwnd() );
}

//string zsAimName;
void CharPanel_Animation::OnButton_SetAimPoses()
{
/*	uint32 nCount = g_SortedItems.size();
	m_SelectedAnimations=nCount;
	if (nCount)
	{
		uint32 IDs = GetAnimIDs();
		int16 id0=(int16)(IDs & 0xffff);
		if (id0>=0)
		{
			zsAimName = GetAnimNameByID(id0);
			const char* zsName = (const char*)zsAimName;
			m_ModelViewPort->SetAimPose(zsName);
		}
	}*/
}

void CharPanel_Animation::OnSlider_BlendSpaceX()
{
	f32 val = m_BlendSpaceSliderX.GetValue();

	uint32 ForceSpeed=GetDesiredLocomotionSpeed();
	if (ForceSpeed)
		val=(val+1)*5;

	m_BlendSpaceNumberX.SetValue( val );
}
f32 CharPanel_Animation::GetBlendSpaceFlexX(f32 sec, f32 aft) 
{ 
	uint32 numValues=m_arrBlendSpacesFadeX.size();
	for (int32 i=(numValues-2); i>-1; i--)
		m_arrBlendSpacesFadeX[i+1] = m_arrBlendSpacesFadeX[i];
	m_arrBlendSpacesFadeX[0] = m_BlendSpaceSliderX.GetValue();

	f32 val=0;
	if (aft==0.0f)
		aft=0.000001f;
	uint32 avrg_frames = sec / aft;  //get amount frames of the last x.xx seconds
	if (avrg_frames>numValues)	avrg_frames=numValues;
	if (avrg_frames<1)	avrg_frames=1;

	for (uint32 i=0; i<avrg_frames; i++)
		val += m_arrBlendSpacesFadeX[i];
	val /= avrg_frames;

	return val;	
}




void CharPanel_Animation::OnSlider_BlendSpaceY()
{
	f32 val = m_BlendSpaceSliderY.GetValue();

	uint32 ForceSpeed=GetDesiredTurnSpeed();
	if (ForceSpeed)
		val=-(val*gf_PI);

	m_BlendSpaceNumberY.SetValue( val );
}
f32 CharPanel_Animation::GetBlendSpaceFlexY(f32 sec, f32 aft) 
{ 
	uint32 numValues=m_arrBlendSpacesFadeY.size();
	for (int32 i=(numValues-2); i>-1; i--)
		m_arrBlendSpacesFadeY[i+1] = m_arrBlendSpacesFadeY[i];
	m_arrBlendSpacesFadeY[0] = m_BlendSpaceSliderY.GetValue();

	f32 val=0;
	if (aft==0.0f)
		aft=0.000001f;
	uint32 avrg_frames = sec / aft; //average the radiuses of the 0.40 seconds
	if (avrg_frames>numValues)	avrg_frames=numValues;
	if (avrg_frames<1)	avrg_frames=1;

	for (uint32 i=0; i<avrg_frames; i++)
		val += m_arrBlendSpacesFadeY[i];

	val /= avrg_frames;
	return val;	
}



void CharPanel_Animation::OnSlider_BlendSpaceZ()
{
	f32 val = m_BlendSpaceSliderZ.GetValue();
	m_BlendSpaceNumberZ.SetValue( val );
}

f32 CharPanel_Animation::GetBlendSpaceFlexZ(f32 sec, f32 aft) 
{ 
	uint32 numValues=m_arrBlendSpacesFadeZ.size();
	for (int32 i=(numValues-2); i>-1; i--)
		m_arrBlendSpacesFadeZ[i+1] = m_arrBlendSpacesFadeZ[i];
	m_arrBlendSpacesFadeZ[0] = m_BlendSpaceSliderZ.GetValue();

	f32 val=0;
	if (aft==0.0f)
		aft=0.000001f;
	uint32 avrg_frames = sec / aft; //average the radiuses of the 0.40 seconds
	if (avrg_frames>numValues)	avrg_frames=numValues;
	if (avrg_frames<1)	avrg_frames=1;

	for (uint32 i=0; i<avrg_frames; i++)
		val += m_arrBlendSpacesFadeZ[i];

	val /= avrg_frames;
	return val;	
}






void CharPanel_Animation::OnSlider_MotionInterpolation()
{
	f32 val = m_MotionInterpolationSlider.GetValue();
	m_MotionInterpolationNumber.SetValue( val );
}

f32 CharPanel_Animation::GetMotionInterpolationFlex(f32 sec, f32 aft) 
{ 
	uint32 numValues=m_arrMotionInterpolationFade.size();
	for (int32 i=(numValues-2); i>-1; i--)
		m_arrMotionInterpolationFade[i+1] = m_arrMotionInterpolationFade[i];
	m_arrMotionInterpolationFade[0] = m_MotionInterpolationSlider.GetValue();

	f32 val=0;
	if (aft==0.0f)
		aft=0.000001f;
	uint32 avrg_frames = sec / aft; //average the radiuses of the 0.40 seconds
	if (avrg_frames>numValues)	avrg_frames=numValues;
	if (avrg_frames<1)	avrg_frames=1;

	for (uint32 i=0; i<avrg_frames; i++)
		val += m_arrMotionInterpolationFade[i];
	val /= avrg_frames;
	return val;	
}



void CharPanel_Animation::OnSlider_LinearMorph()
{
	f32 val = m_LinearMorphSlider.GetValue();
	//m_LayerMotionInterpolationNumber.SetValue( val );
}

f32 CharPanel_Animation::GetLinearMorphSliderFlex(f32 sec, f32 aft) 
{ 
	uint32 numValues=m_arrLinearMorphFade.size();
	for (int32 i=(numValues-2); i>-1; i--)
		m_arrLinearMorphFade[i+1] = m_arrLinearMorphFade[i];
	m_arrLinearMorphFade[0] = m_LinearMorphSlider.GetValue();

	f32 val=0;
	
	if (aft==0.0f)
		aft=0.000001f;

	uint32 avrg_frames = sec / aft;  //get amount frames of the last x.xx seconds
	if (avrg_frames>numValues)	avrg_frames=numValues;
	if (avrg_frames<1)	avrg_frames=1;

	for (uint32 i=0; i<avrg_frames; i++)
		val += m_arrLinearMorphFade[i];
	val /= avrg_frames;

	return val;	
}














BOOL CharPanel_Animation::OnInitDialog() 
{
	CDialog::OnInitDialog();

	CMFCUtils::LoadTrueColorImageList( m_imageListFiles, IDB_ANIMATIONS_TREE, 16, RGB(255,0,255) );
	//m_imageListFiles.SetOverlayImage( 1,1 );
	m_tree_animations.SetImageList( &m_imageListFiles, TVSIL_NORMAL );

	m_layer.SetCurSel(0);


	m_AnimationDrivenMotion.SetCheck(BST_CHECKED);
	m_VerticalMovement.SetCheck(BST_CHECKED);

	//--------------------------------------------------------------------
	m_BlendSpaceSliderX.SubclassDlgItem( IDC_BLENDSPACESLIDER_X,this );
	m_BlendSpaceSliderX.SetRangeFloat( -1.0f,+1.0f  );
	m_BlendSpaceSliderX.SetValue(0.0f);
	m_BlendSpaceSliderX.EnableWindow(TRUE);

	m_BlendSpaceNumberX.Create( this,IDC_BLENDSPACENUMBER_X );
	m_BlendSpaceNumberX.SetRange( -1.0f,+10.0f );
	m_BlendSpaceNumberX.SetInternalPrecision( 3 );
	m_BlendSpaceNumberX.SetValue(0.0f);
	m_BlendSpaceNumberX.EnableWindow(TRUE);

	//----------------------------------------------------------------------------
	m_BlendSpaceSliderY.SubclassDlgItem( IDC_BLENDSPACESLIDER_Y,this );
	m_BlendSpaceSliderY.SetRangeFloat( -1.0f,+1.0f  );
	m_BlendSpaceSliderY.SetValue(0.0f);
	m_BlendSpaceSliderY.EnableWindow(TRUE);

	m_BlendSpaceNumberY.Create( this,IDC_BLENDSPACENUMBER_Y );
	m_BlendSpaceNumberY.SetRange( -10.0f,+10.0f );
	m_BlendSpaceNumberY.SetInternalPrecision( 3 );
	m_BlendSpaceNumberY.SetValue(0.0f);
	m_BlendSpaceNumberY.EnableWindow(TRUE);

	//----------------------------------------------------------------------------
	m_BlendSpaceSliderZ.SubclassDlgItem( IDC_BLENDSPACESLIDER_Z,this );
	m_BlendSpaceSliderZ.SetRangeFloat( -1.0f,+1.0f  );
	m_BlendSpaceSliderZ.SetValue(0.0f);
	m_BlendSpaceSliderZ.EnableWindow(TRUE);

	m_BlendSpaceNumberZ.Create( this,IDC_BLENDSPACENUMBER_Z );
	m_BlendSpaceNumberZ.SetRange( -1.0f,+1.0f );
	m_BlendSpaceNumberZ.SetInternalPrecision( 3 );
	m_BlendSpaceNumberZ.SetValue(0.0f);
	m_BlendSpaceNumberZ.EnableWindow(TRUE);

	//----------------------------------------------------------------------------
	m_MotionInterpolationSlider.SubclassDlgItem( IDC_INTERPOLATION_SLIDER,this );
	m_MotionInterpolationSlider.SetRangeFloat( -0.0f,+1.0f  );
	m_MotionInterpolationSlider.SetValue(0.5f);
	m_MotionInterpolationSlider.EnableWindow(TRUE);

	m_MotionInterpolationNumber.Create( this,IDC_INTERPOLATION_NUMBER );
	m_MotionInterpolationNumber.SetRange( 0.0f,+1.0f );
	m_MotionInterpolationNumber.SetInternalPrecision( 3 );
	m_MotionInterpolationNumber.SetValue(0.5f);
	m_MotionInterpolationNumber.EnableWindow(TRUE);

	//--------------------------------------------------------------------

	m_Gamepad.SetCheck(0);
	m_LinearMorphSequence.SetCheck(0);

	m_LinearMorphSlider.SubclassDlgItem( IDC_LINEAR_MORPH_SLIDER,this );
	m_LinearMorphSlider.SetRangeFloat( -0.0f,+1.0f  );
	m_LinearMorphSlider.SetValue(0.5f);
	m_LinearMorphSlider.EnableWindow(FALSE);

	//m_LayerMotionInterpolationNumber.Create( this,IDC_LINEAR_MORPH_NUMBER );
	//m_LayerMotionInterpolationNumber.SetRange( 0.0f,+0.999f );
	//m_LayerMotionInterpolationNumber.SetInternalPrecision( 3 );
	//m_LayerMotionInterpolationNumber.SetValue(0.5f);
	//m_LayerMotionInterpolationNumber.EnableWindow(TRUE);

	//--------------------------------------------------------------------

	m_AnimationSpeed.SubclassDlgItem( IDC_ANIMATION_SPEED_SLIDER,this );
	m_AnimationSpeed.SetRangeFloat( -1,2 );
	m_AnimationSpeed.SetValue(1);
	m_AnimationSpeed.EnableWindow(TRUE);

	m_AnimationSpeedNumber.Create( this,IDC_ANIMATION_SPEED );
	m_AnimationSpeedNumber.SetRange( -1,2 );
	m_AnimationSpeedNumber.SetInternalPrecision( 3 );
	m_AnimationSpeedNumber.SetValue(1);
	m_AnimationSpeedNumber.EnableWindow(TRUE);

	//--------------------------------------------------------------------

	m_preset.ResetContent();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CharPanel_Animation::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) 
{
	// Route the commands to the MainFrame
	if (AfxGetMainWnd())
		AfxGetMainWnd()->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);

	return CDialog::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}


bool CharPanel_Animation::GetAnimLightState()
{
	return IsDlgButtonChecked( IDC_ANIMATE_LIGHTS ) == TRUE;
}

void CharPanel_Animation::OnAnimateLights() 
{
	// TODO: Add your control notification handler code here
	AfxGetMainWnd()->SendMessage( WM_COMMAND,MAKEWPARAM(IDC_ANIMATE_LIGHTS,0),(LPARAM)GetSafeHwnd() );
}






void CharPanel_Animation::OnSlider_AnimationSpeed()
{
	float val = m_AnimationSpeed.GetValue();
	m_AnimationSpeedNumber.SetValue( val );
	ICharacterInstance* pCharacter = m_ModelViewPort->GetCharacterBase();
	if (pCharacter)
		pCharacter->SetAnimationSpeed(val);
}

void CharPanel_Animation::OnNumber_AnimationSpeed()
{
	float val = m_AnimationSpeedNumber.GetValue();
	m_AnimationSpeed.SetValue( val );
	ICharacterInstance* pCharacter = m_ModelViewPort->GetCharacterBase();
	if (pCharacter)
		pCharacter->SetAnimationSpeed(val);
}



void CharPanel_Animation::OnLoop() 
{
	AfxGetMainWnd()->SendMessage( WM_COMMAND,MAKEWPARAM(ID_ANIM_PLAY,0),(LPARAM)GetSafeHwnd() );
}




void CharPanel_Animation::OnButton_StopAnimationInLayer()
{
	int nLayer = GetLayer();
	m_ModelViewPort->StopAnimationInLayer(nLayer);
}

void CharPanel_Animation::OnButton_StopAnimationsAll()
{
	m_ModelViewPort->GetCharacterBase()->GetISkeletonAnim()->StopAnimationsAllLayers();
}
void CharPanel_Animation::OnButton_ResetModel()
{
	m_ModelViewPort->GetCharacterBase()->GetISkeletonPose()->SetDefaultPose();
}

void CharPanel_Animation::OnButton_ChooseBaseCharacter()
{
	m_ModelViewPort->m_pCharacterAnim = m_ModelViewPort->GetCharacterBase();
	m_ModelViewPort->UpdateAnimationList();
	if(m_ModelViewPort->GetCharPanelPreset())
		m_ModelViewPort->GetCharPanelPreset()->InitNodes();
}

void CharPanel_Animation::SetBaseCharacter()
{
	m_ModelViewPort->m_pCharacterAnim = m_ModelViewPort->GetCharacterBase();
	m_ModelViewPort->UpdateAnimationList();
	if(m_ModelViewPort->GetCharPanelPreset())
		m_ModelViewPort->GetCharPanelPreset()->InitNodes();
}

//////////////////////////////////////////////////////////////////////////
void CharPanel_Animation::OnBnClickedBrowseFile()
{
	CString file;
	m_fileEdit.GetWindowText( file );
	if (CFileUtil::SelectSingleFile( EFILE_TYPE_GEOMETRY,file ))
	{
		m_ModelViewPort->LoadObject( file,1 );
		m_fileEdit.SetWindowText( file );
	}
}

//////////////////////////////////////////////////////////////////////////
void CharPanel_Animation::SetFileName( const CString &filename )
{
	m_fileEdit.SetWindowText( filename );
}

//////////////////////////////////////////////////////////////////////////
void CharPanel_Animation::OnBnClickedReloadFile()
{
	CString filename;
	m_fileEdit.GetWindowText( filename );
	m_ModelViewPort->LoadObject( filename,1 );
}

//////////////////////////////////////////////////////////////////////////
void CharPanel_Animation::OnBnClickedMaterial()
{
	// Try to find the material used by the displayed asset.
	IMaterial* pMaterial = 0;
	if (m_ModelViewPort->GetCharacterBase() && m_ModelViewPort->GetCharacterBase()->GetMaterial())
		pMaterial = m_ModelViewPort->GetCharacterBase()->GetMaterial();
	else if (m_ModelViewPort->GetStaticObject() && m_ModelViewPort->GetStaticObject()->GetMaterial())
		pMaterial = m_ModelViewPort->GetStaticObject()->GetMaterial();

	// Open the material editor, showing the material if one was found.
	IDataBaseItem *pItem = 0;
	if (pMaterial)
	{
		CString name = pMaterial->GetName();
		pItem = GetIEditor()->GetMaterialManager()->FindItemByName(name);
	}
	// If we could not find the material, report the problem.
	else
	{
		this->MessageBox("Unable to find material to show - opening material editor with no selection.", "No Material", MB_ICONEXCLAMATION | MB_OK);
	}

	GetIEditor()->OpenDataBaseLibrary( EDB_TYPE_MATERIAL,pItem );
}

//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------
void CharPanel_Animation::OnButton_TransRot2000()
{
	AfxGetMainWnd()->SendMessage( WM_COMMAND,MAKEWPARAM(ID_ANIM_PLAY,0),(LPARAM)GetSafeHwnd() );
	uint32 TransRot2000Status = m_AnimationDrivenMotion.GetState() & BST_CHECKED;
}

void CharPanel_Animation::OnButton_VerticalMovement()
{
	AfxGetMainWnd()->SendMessage( WM_COMMAND,MAKEWPARAM(ID_ANIM_PLAY,0),(LPARAM)GetSafeHwnd() );
	uint32 VerticalMovement = m_VerticalMovement.GetState() & BST_CHECKED;
}

void CharPanel_Animation::OnButton_VTimeWarping()
{
	AfxGetMainWnd()->SendMessage( WM_COMMAND,MAKEWPARAM(ID_ANIM_PLAY,0),(LPARAM)GetSafeHwnd() );
	uint32 TimeWarpingStatus = m_VTimeWarping.GetState() & BST_CHECKED;
}


void CharPanel_Animation::OnButton_FootAnchoring()
{
	AfxGetMainWnd()->SendMessage( WM_COMMAND,MAKEWPARAM(ID_ANIM_PLAY,0),(LPARAM)GetSafeHwnd() );
	//uint32 FVEStatus = m_FootAnchoring.GetState() & BST_CHECKED;
	//m_FootIK.EnableWindow(FVEStatus);
}


void CharPanel_Animation::OnButton_LFootIK()
{
	AfxGetMainWnd()->SendMessage( WM_COMMAND,MAKEWPARAM(ID_ANIM_PLAY,0),(LPARAM)GetSafeHwnd() );
	uint32 buttons = m_LFootIK.GetState() & BST_CHECKED;
	if (buttons)
	{
		//	m_LFootIK.SetCheck(1);
		m_RFootIK.SetCheck(0);
		m_LArmIK.SetCheck(0);
		m_RArmIK.SetCheck(0);
		m_GroundAlign.SetCheck(0);
	}
}

void CharPanel_Animation::OnButton_RFootIK()
{
	AfxGetMainWnd()->SendMessage( WM_COMMAND,MAKEWPARAM(ID_ANIM_PLAY,0),(LPARAM)GetSafeHwnd() );
	uint32 button = m_RFootIK.GetState() & BST_CHECKED;
	if (button)
	{
		m_LFootIK.SetCheck(0);
		//	m_RFootIK.SetCheck(1);
		m_LArmIK.SetCheck(0);
		m_RArmIK.SetCheck(0);
		m_GroundAlign.SetCheck(0);
	}
}

void CharPanel_Animation::OnButton_LArmIK()
{
	AfxGetMainWnd()->SendMessage( WM_COMMAND,MAKEWPARAM(ID_ANIM_PLAY,0),(LPARAM)GetSafeHwnd() );
	uint32 button = m_LArmIK.GetState() & BST_CHECKED;
	if (button)
	{
		m_LFootIK.SetCheck(0);
		m_RFootIK.SetCheck(0);
		//	m_LArmIK.SetCheck(1);
		m_RArmIK.SetCheck(0);
		m_GroundAlign.SetCheck(0);
	}
}

void CharPanel_Animation::OnButton_RArmIK()
{
	AfxGetMainWnd()->SendMessage( WM_COMMAND,MAKEWPARAM(ID_ANIM_PLAY,0),(LPARAM)GetSafeHwnd() );
	uint32 button = m_RArmIK.GetState() & BST_CHECKED;
	if (button)
	{
		m_LFootIK.SetCheck(0);
		m_RFootIK.SetCheck(0);
		m_LArmIK.SetCheck(0);
		//	m_RArmIK.SetCheck(1);
		m_GroundAlign.SetCheck(0);
	}
}

void CharPanel_Animation::OnButton_GroundAlign()
{
	AfxGetMainWnd()->SendMessage( WM_COMMAND,MAKEWPARAM(ID_ANIM_PLAY,0),(LPARAM)GetSafeHwnd() );
	uint32 button = m_GroundAlign.GetState() & BST_CHECKED;
	if (button)
	{
		m_LFootIK.SetCheck(0);
		m_RFootIK.SetCheck(0);
		m_LArmIK.SetCheck(0);
		m_RArmIK.SetCheck(0);
		//m_GroundAlign.SetCheck(1);
	}
}


void CharPanel_Animation::OnButton_PlayerControl()
{
	AfxGetMainWnd()->SendMessage( WM_COMMAND,MAKEWPARAM(ID_ANIM_PLAY,0),(LPARAM)GetSafeHwnd() );

	m_ModelViewPort->SetPlayerPos(Vec3(ZERO));
	uint32 PlayerControlStatus = m_PlayControl.GetState() & BST_CHECKED;

	ICharacterInstance* pCharacter = m_ModelViewPort->GetCharacterBase();
	if (pCharacter)
	{
		if (PlayerControlStatus)
		{
			//enable player-control
			pCharacter->GetISkeletonPose()->SetDefaultPose();
			m_ModelViewPort->SetPlayerControl(PlayerControlStatus);
		}
		else
		{
			//normal character viewer
			m_ModelViewPort->SetPlayerControl(0);
			pCharacter->GetISkeletonPose()->SetDefaultPose();
			Vec3 VPos=Vec3(0,3,2);
			Vec3 VTar=Vec3(0,0,1);
			Matrix33 orient; orient.SetRotationVDir( (VTar-VPos).GetNormalized() );
			m_ModelViewPort->m_Camera.SetMatrix( Matrix34(orient,VPos)  );
		}
	}
}


//------------------------------------------------------------------------
//------------------------------------------------------------------------
//------------------------------------------------------------------------
void CharPanel_Animation::OnButton_PathFollowing()
{
	AfxGetMainWnd()->SendMessage( WM_COMMAND,MAKEWPARAM(ID_ANIM_PLAY,0),(LPARAM)GetSafeHwnd() );

	m_ModelViewPort->SetPlayerPos(Vec3(ZERO));
	uint32 PathFollowingStatus = m_PathFollowing.GetState() & BST_CHECKED;

	
	ICharacterInstance* pCharacter = m_ModelViewPort->GetCharacterBase();
	if (pCharacter)
	{
		if (PathFollowingStatus)
		{
			//enable path-following
			pCharacter->GetISkeletonPose()->SetDefaultPose();
			m_ModelViewPort->SetPlayerControl(PathFollowingStatus<<1);
		}
		else
		{
			//normal character viewer
			m_ModelViewPort->SetPlayerControl(0);
			pCharacter->GetISkeletonPose()->SetDefaultPose();
			Vec3 VPos=Vec3(0,3,2);
			Vec3 VTar=Vec3(0,0,1);
			Matrix33 orient; orient.SetRotationVDir( (VTar-VPos).GetNormalized() );
			m_ModelViewPort->m_Camera.SetMatrix( Matrix34(orient,VPos)  );
		}
	}
}

//------------------------------------------------------------------------
//------------------------------------------------------------------------
//------------------------------------------------------------------------
void CharPanel_Animation::OnButton_Idle2Move()
{
	AfxGetMainWnd()->SendMessage( WM_COMMAND,MAKEWPARAM(ID_ANIM_PLAY,0),(LPARAM)GetSafeHwnd() );

	m_ModelViewPort->SetPlayerPos(Vec3(ZERO));
	uint32 Idle2MoveStatus = m_Idle2Move.GetState() & BST_CHECKED;


	ICharacterInstance* pCharacter = m_ModelViewPort->GetCharacterBase();
	if (pCharacter)
	{
		if (Idle2MoveStatus)
		{
			//enable path-following
			pCharacter->GetISkeletonPose()->SetDefaultPose();
			m_ModelViewPort->SetPlayerControl(Idle2MoveStatus<<2);
		}
		else
		{
			//normal character viewer
			m_ModelViewPort->SetPlayerControl(0);
			pCharacter->GetISkeletonPose()->SetDefaultPose();
			Vec3 VPos=Vec3(0,3,2);
			Vec3 VTar=Vec3(0,0,1);
			Matrix33 orient; orient.SetRotationVDir( (VTar-VPos).GetNormalized() );
			m_ModelViewPort->m_Camera.SetMatrix( Matrix34(orient,VPos)  );
		}
	}
}


//------------------------------------------------------------------------
//------------------------------------------------------------------------
//------------------------------------------------------------------------
void CharPanel_Animation::OnButton_IdleStep()
{
	AfxGetMainWnd()->SendMessage( WM_COMMAND,MAKEWPARAM(ID_ANIM_PLAY,0),(LPARAM)GetSafeHwnd() );

	m_ModelViewPort->SetPlayerPos(Vec3(ZERO));
	uint32 IdleStepStatus = m_IdleStep.GetState() & BST_CHECKED;

	ICharacterInstance* pCharacter = m_ModelViewPort->GetCharacterBase();
	if (pCharacter)
	{
		if (IdleStepStatus)
		{
			//enable path-following
			pCharacter->GetISkeletonPose()->SetDefaultPose();
			m_ModelViewPort->SetPlayerControl(IdleStepStatus<<3);
		}
		else
		{
			//normal character viewer
			m_ModelViewPort->SetPlayerControl(0);
			pCharacter->GetISkeletonPose()->SetDefaultPose();
			Vec3 VPos=Vec3(0,3,2);
			Vec3 VTar=Vec3(0,0,1);
			Matrix33 orient; orient.SetRotationVDir( (VTar-VPos).GetNormalized() );
			m_ModelViewPort->m_Camera.SetMatrix( Matrix34(orient,VPos)  );
		}
	}
}

void CharPanel_Animation::OnButton_LinearMorphSequence()
{
	AfxGetMainWnd()->SendMessage( WM_COMMAND,MAKEWPARAM(ID_ANIM_PLAY,0),(LPARAM)GetSafeHwnd() );
	uint32 status = m_LinearMorphSequence.GetState() & BST_CHECKED;
	m_LinearMorphSlider.EnableWindow( status );

}


////////////////////////////////////////////////////////////
void CharPanel_Animation::ClearAnims()
{
	m_tree_animations.DeleteAllItems();
}


////////////////////////////////////////////////////////////
void CharPanel_Animation::AddAnimName( const CString &name, uint32 id )
{
	IAnimationSet* pAnimations = m_ModelViewPort->GetCharacterAnim()->GetIAnimationSet();
	const char* pname = pAnimations->GetNameByAnimID(id);
	if (pname==0)
		return;

	int icon = -1;
	if (pname[0]!='#')
	{
		float length = pAnimations->GetDuration_sec(id);
		uint32 flags = pAnimations->GetAnimationFlags(id);
		uint32 loaded = flags & CA_ASSET_LOADED;
		uint32 lmg = flags & CA_ASSET_LMG;
		if ( flags & CA_AIMPOSE_UNLOADED )
			icon = AIMPOSE_ICON;
		else if ( flags & CA_ASSET_ONDEMAND )
			icon = ONDEMAND_ICON;
		else if ( (flags & CA_ASSET_LOADED) == 0 )
			icon = MISSING_ICON;
		else if ( flags & CA_ASSET_LMG )
			icon = LMG_ICON;
		else if ( flags & CA_ASSET_CYCLE )
			icon = LOOPING_ICON;
		else
			icon = NONLOOPING_ICON;
	}

	char sFolder[MAX_PATH];
	strcpy(sFolder, name);

	char * ch = sFolder;
	while (*ch!=0 && *ch=='_')
		ch++;
	while(*ch!=0 && *ch!='_')
		ch++;

	if(*ch)
	{
		*ch = 0;

		HTREEITEM hChildItem = m_tree_animations.GetChildItem(TVI_ROOT);
		while (hChildItem != NULL)
		{
			CString sName = m_tree_animations.GetItemText(hChildItem);
			
			if(!stricmp(sFolder, sName))
			{
				HTREEITEM hItem = m_tree_animations.InsertItem( name, icon, icon, hChildItem, TVI_SORT );
				m_tree_animations.SetItemData( hItem, id );
				return;
			}
			hChildItem = m_tree_animations.GetNextItem(hChildItem, TVGN_NEXT);
		}

		hChildItem = m_tree_animations.InsertItem( sFolder, FOLDER_ICON, FOLDER_ICON, TVI_ROOT, TVI_SORT );
		m_tree_animations.SetItemData( hChildItem, -1 );
		HTREEITEM hItem = m_tree_animations.InsertItem( name, icon, icon, hChildItem, TVI_SORT );
		m_tree_animations.SetItemData( hItem, id );
		return;
	}

	HTREEITEM hItem = m_tree_animations.InsertItem( name, TVI_ROOT );
	m_tree_animations.SetItemData( hItem, id );
}

void CharPanel_Animation::OnClickTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	OnTvnSelchangedTree(pNMHDR, pResult);
	/*
	static HTREEITEM hPrevItem = 0;
	HTREEITEM hSelItem = m_tree_animations.GetSelectedItem();
	
	if(hPrevItem == hSelItem)
		OnTvnSelchangedTree(pNMHDR, pResult);

	hPrevItem = hSelItem;
	*/
}


void CharPanel_Animation::OnTvnSelchangedTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	//LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);

	static bool bIsFirstLevel = 1;
	if(!bIsFirstLevel)
		return;

	static HTREEITEM hPrevItem = 0;

	int nCount = 0;
	int nAllCount = 0;
	bool bIsSelEq=false;
	HTREEITEM hSelItem = m_tree_animations.GetSelectedItem();
	HTREEITEM hItem = m_tree_animations.GetFirstSelectedItem();
	while(hItem)
	{
		if(hSelItem==hItem)
			bIsSelEq = true;
		if(!m_tree_animations.ItemHasChildren(hItem))
			nCount++;
		nAllCount++;
		hItem = m_tree_animations.GetNextSelectedItem(hItem);
	}

	if(nAllCount>1)
	{
		HTREEITEM hItem = m_tree_animations.GetFirstSelectedItem();
		while(hItem)
		{
			HTREEITEM hNextItem = m_tree_animations.GetNextSelectedItem(hItem);
			if(m_tree_animations.ItemHasChildren(hItem))
			{
				bIsFirstLevel = 0;
				m_tree_animations.SetItemState(hItem, 0, TVIS_SELECTED);
				bIsFirstLevel = 1;
			}
			hItem = hNextItem;
		}
	}

	if(nCount>2)
	{
		HTREEITEM hItem = m_tree_animations.GetFirstSelectedItem();
		while(hItem)
		{
			HTREEITEM hNextItem = m_tree_animations.GetNextSelectedItem(hItem);
			if(hSelItem!=hItem && hPrevItem!=hItem)
			{
				bIsFirstLevel = 0;
				m_tree_animations.SetItemState(hItem, 0, TVIS_SELECTED);
				bIsFirstLevel = 1;
			}
			hItem = hNextItem;
		}
	}

	m_hPrevItem	= hPrevItem;

	if(bIsSelEq)
		hPrevItem = hSelItem;

	if(!nCount)
		hPrevItem = 0;
	
	if(nCount && GetAsyncKeyState( VK_CONTROL ) == 0)
		m_ModelViewPort->SendMessage( WM_COMMAND,MAKEWPARAM(ID_ANIM_PLAY,0),(LPARAM)GetSafeHwnd() );

	if(m_ModelViewPort->GetCharPanelPreset())
		m_ModelViewPort->GetCharPanelPreset()->UpdateBonePreset();
}

void CharPanel_Animation::OnTvnGetInfoTip(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTVGETINFOTIP pnmtv = (LPNMTVGETINFOTIP) pNMHDR;

	CString text;
	uint32 id = m_tree_animations.GetItemData( pnmtv->hItem );
	if ( id != uint32(-1) )
	{
		IAnimationSet* pAnimations = m_ModelViewPort->GetCharacterAnim()->GetIAnimationSet();
		const char* name = pAnimations->GetNameByAnimID(id);
		if (name)
		{
			if (name[0]=='#')
			{
				text.Format( "%s (morph target)", name );
			}
			else
			{
				float length = pAnimations->GetDuration_sec(id);
				uint32 flags = pAnimations->GetAnimationFlags(id);
				uint32 loaded = flags & CA_ASSET_LOADED;
				uint32 lmg = flags & CA_ASSET_LMG;
				uint32 aimpose = flags & CA_AIMPOSE_UNLOADED;

				if (aimpose)
					text.Format( "%s (aim poses)", name );
				else if (lmg)
					text.Format( "%s (LMG)\n\nLength: %g sec.", name, length );
				else if (loaded)
					text.Format( "%s\n\nLength: %g sec.", name, length );
				else
					text.Format( "%s\n\n(not loaded)", name );
			}
		}
	}

	strcpy_s( pnmtv->pszText, pnmtv->cchTextMax, text );
}

void CharPanel_Animation::EnablePreset(bool bIsEnable)
{
	GetDlgItem(IDC_PRESET)->EnableWindow(bIsEnable);
	GetDlgItem(IDC_RECALCULATE)->EnableWindow(bIsEnable);
}


void CharPanel_Animation::SetPreset(CString name)
{
	if (name != "")
	{
		int ind = m_preset.FindStringExact(0, name);
		if(ind<0)
			ind = m_preset.AddString(name);
		m_preset.SetCurSel(ind);
	}
}


int CharPanel_Animation::GetSelectedAnimations() const
{
	int nCount = 0;
	HTREEITEM hItem = m_tree_animations.GetFirstSelectedItem();
	while(hItem)
	{
		if(!m_tree_animations.ItemHasChildren(hItem))
			nCount++;
		hItem = m_tree_animations.GetNextSelectedItem(hItem);
	}
	return nCount;
}


//////////////////////////////////////////////////////////////////////////
void CharPanel_Animation::OnBnClickedRecalculate()
{
	//rc.exe pathtocbafile /file="outputfilename"
//	m_animLoader->SaveDescription(PathUtil::GetGameFolder() + CBA_FILE);
	m_ModelViewPort->GetCharPanelPreset()->SaveCBAFile();

	CString strAnimName = GetCurrAnimName();
	if(!strlen(strAnimName))
		return;

  PROCESS_INFORMATION   pi;
  STARTUPINFO si = { sizeof(STARTUPINFO) };
  char pRun[MAX_PATH];
  sprintf(pRun,"rc/rc.exe %s%s /file=\"%s\" /wait", PathUtil::GetGameFolder(), CBA_FILE, strAnimName);

  if(CreateProcess(0, pRun, NULL,NULL, FALSE, NORMAL_PRIORITY_CLASS,NULL, NULL, &si, &pi))
  {
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
  }

	// reload character
//	m_ModelViewPort->GetCharacterBase()->GetISkeleton()->ReloadCAF(strAnimName);
	m_ModelViewPort->GetCharacterBase()->GetIAnimationSet()->ReloadCAF(strAnimName, false);
}

void CharPanel_Animation::OnPresetSelect()
{
	int ind = m_preset.GetCurSel();
	if(ind>=0)
	{
		CString str;
		m_preset.GetLBText(ind, str);
		if(m_ModelViewPort->GetCharPanelPreset())
			m_ModelViewPort->GetCharPanelPreset()->ChangeCurPreset(str);
	}
}


BOOL CharPanel_Animation::OnCommand(WPARAM wParam, LPARAM lParam) 
{
	if(LOWORD(wParam)==RC_IDM_COPY_NAME)
	{
		CString strAnimName = GetCurrAnimName();
		if(strlen(strAnimName))
		{
			CClipboard clipboard;
			clipboard.PutString(strAnimName, strAnimName);
		}
	}
	return CWnd::OnCommand(wParam, lParam);
}
