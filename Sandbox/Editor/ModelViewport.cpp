// RenderViewport.cpp : implementation file
//

#include "stdafx.h"
#include "Controls\AnimationToolBar.h"

#include "ModelViewport.h"
#include "CharacterEditor\CharPanel_BAttach.h"
//#include "CharacterEditor\CharPanel_FAttach.h"
#include "CharacterEditor\CharPanel_Animation.h"

#include "PropertiesPanel.h"
#include "ThumbnailGenerator.h"
#include "GameEngine.h"

#include "ICryAnimation.h"
#include "CryCharMorphParams.h"
#include "CryCharAnimationParams.h"
#include "IFacialAnimation.h"

#include "FileTypeUtils.h"
#include "Objects\DisplayContext.h"
#include "Material\MaterialManager.h"
#include "ErrorReport.h"

#include <I3DEngine.h>
#include <IPhysics.h>
#include <ITimer.h>
#include "IRenderAuxGeom.h"

IMPLEMENT_DYNCREATE(CModelViewport,CRenderViewport)
uint32 g_ypos = 0;

#define SKYBOX_NAME "InfoRedGal"

namespace
{
	int s_varsPanelId = 0;
	CPropertiesPanel* s_varsPanel = 0;
}

#define MULTIPLY (2)

/////////////////////////////////////////////////////////////////////////////
// CModelViewport

CModelViewport::CModelViewport()
{
	m_bPaused = false;

	m_Camera.SetFrustum( 800,600, 3.14f/4.0f, 0.02f,10000 );
	m_CamPos = GetCamera().GetPosition();

	m_pAnimationSystem = GetIEditor()->GetSystem()->GetIAnimationSystem();

	m_bInRotateMode = false;
	m_bInMoveMode = false;

	m_rollupIndex = 0;
	m_rollupIndex2 = 0;

	m_object = 0;
	m_pCharacterBase = 0;
	m_pCharacterAnim = 0;
	m_weaponModel = 0;
	m_attachedCharacter = 0;

	m_pCharPanel_Animation = 0;
	m_pCharPanel_Preset = 0;
	m_pCharPanel_BAttach = 0;

	m_camRadius = 10;

	m_moveSpeed = 0.1f;
	m_LightRotationRadiant = 0.0f;

	m_pRESky = 0;
	m_pSkyboxName = 0;
	m_pSkyBoxShader = NULL;
	m_pPhysicalEntity = NULL;

	m_attachBone = "weapon_bone";

	// Init variable.
	mv_objectAmbientColor = Vec3(0.40f,0.40f,0.40f);
	mv_backgroundColor		= Vec3(0.20f,0.30f,0.40f);

	m_VPLights.resize(3);

	Vec3 d0 = Vec3(0.70f,0.70f,0.70f);
	mv_lightDiffuseColor0  = d0;
	Vec3 d1 = Vec3(0.40f,0.60f,0.90f);
	mv_lightDiffuseColor1  = d1;
	Vec3 d2 = Vec3(0.40f,0.14f,0.00f);
	mv_lightDiffuseColor2  = d2;


	mv_fov = 60;
	mv_showPhysics = false;
	mv_showOcclusion = false;




	m_GridOrigin=Vec3(ZERO);
	m_arrCamRadYaw.resize(0x180);
	m_arrCamRadPitch.resize(0x180);
	m_arrAnimatedCharacterPath.resize(0x200);
	m_arrSmoothEntityPath.resize(0x200);

	m_arrRunStrafeSmoothing.resize(0x100);
	SetPlayerPos(Vec3(ZERO));

	//--------------------------------------------------
	// Register variables.
	//--------------------------------------------------
	m_vars.AddVariable( mv_showPhysics,_T("Display Physics"),functor(*this,&CModelViewport::OnShowPhysics) );
	m_vars.AddVariable( mv_wireframe,_T("Wireframe") );
	m_vars.AddVariable( mv_showGrid,_T("Grid") );
	m_vars.AddVariable( mv_showBase,_T("Base") );
	mv_showGrid = true;
	mv_showBase = false;

	m_vars.AddVariable( mv_lighting,_T("Lighting") );
	mv_lighting = true;

	m_vars.AddVariable( mv_animateLights,_T("AnimLights") );


	m_vars.AddVariable( mv_backgroundColor,_T("BackgroundColor"),functor(*this,&CModelViewport::OnLightColor),IVariable::DT_COLOR );
	m_vars.AddVariable( mv_objectAmbientColor,_T("ObjectAmbient"),functor(*this,&CModelViewport::OnLightColor),IVariable::DT_COLOR );

	m_vars.AddVariable( mv_lightDiffuseColor0,_T("LightDiffuse1"),functor(*this,&CModelViewport::OnLightColor),IVariable::DT_COLOR );
	m_vars.AddVariable( mv_lightDiffuseColor1,_T("LightDiffuse2"),functor(*this,&CModelViewport::OnLightColor),IVariable::DT_COLOR );
	m_vars.AddVariable( mv_lightDiffuseColor2,_T("LightDiffuse3"),functor(*this,&CModelViewport::OnLightColor),IVariable::DT_COLOR );

	m_vars.AddVariable( mv_showTangents,_T("ShowTangents") );
	m_vars.AddVariable( mv_showBinormals,_T("ShowBinormals") );
	m_vars.AddVariable( mv_showNormals,_T("ShowNormals") );

	m_vars.AddVariable( mv_showSkeleton,_T("ShowSkeleton") );
	m_vars.AddVariable( mv_showJointNames,_T("ShowJointNames") );
	m_vars.AddVariable( mv_showJointsValues,_T("ShowJointsValues") );
	m_vars.AddVariable( mv_showSuperimposed,_T("ShowSuperimposed") );
	m_vars.AddVariable( mv_showRootUpdate,_T("ShowRootUpdate") );
	m_vars.AddVariable( mv_showMotionCaps,_T("ShowMotionCaps") );
	m_vars.AddVariable( mv_showStartLocation,_T("ShowStartLocation") );
	m_vars.AddVariable( mv_showFuturePath,_T("ShowFuturePath") );
	m_vars.AddVariable( mv_showCoordinates,_T("ShowCoordinates") ); mv_showCoordinates=1;
	m_vars.AddVariable( mv_showBodyMoveDir,_T("ShowBodyMoveDir") ); mv_showBodyMoveDir=1;
	m_vars.AddVariable( mv_useKeyStrafe,_T("UseKeyStrafe") );
	m_vars.AddVariable( mv_useNaturalSpeed,_T("UseNaturalSpeed") );

	m_vars.AddVariable( mv_printDebugText,_T("PrintDebugText") );

	m_vars.AddVariable( mv_showShaders,_T("ShowShaders"),functor(*this,&CModelViewport::OnShowShaders) );
	m_vars.AddVariable( mv_showSkyBox,_T("ShowSkyBox") );

	m_vars.AddVariable( mv_showTextureUsage,_T("ShowTextureUsage"),functor(*this,&CModelViewport::OnShowTextureUsage) );
	m_vars.AddVariable( mv_showAllTextures,_T("ShowAllTextures"),functor(*this,&CModelViewport::OnShowAllTextures) );

	m_vars.AddVariable( mv_disableLod,"NoLod",_T("No LOD") );
	m_vars.AddVariable( mv_showOcclusion,_T("ShowOcclusion"),functor(*this,&CModelViewport::OnShowOcclusion) );
	m_vars.AddVariable( mv_disableVisibility,_T("DisableVisibilty"),functor(*this,&CModelViewport::OnDisableVisibility) );
	m_vars.AddVariable( mv_fov,_T("FOV") );

	//mv_advancedTable.AddChildVar( mv_ )

	CString xml = AfxGetApp()->GetProfileString( "PreviewSettings","Vars" );
	if (!xml.IsEmpty())
	{
		XmlParser parser;
		XmlNodeRef node = parser.parseBuffer( xml );
		if (node)
		{
			m_vars.Serialize( node,true );
		}
	}

	m_camRadius = 10;

	//YPR_Angle	=	Ang3(0,-1.0f,0);
	//SetViewTM( Matrix34(CCamera::CreateOrientationYPR(YPR_Angle), Vec3(0,-m_camRadius,0))  );
	Vec3 camPos = Vec3(10,10,10);
	Matrix34 tm = Matrix33::CreateRotationVDir( (Vec3(0,0,0) - camPos).GetNormalized() );
	tm.SetTranslation(camPos);
	SetViewTM( tm  );

	if (GetIEditor()->IsInPreviewMode())
	{
		// In preview mode create a simple physical grid, so we can register physical entities.
		int nCellSize = 4;
		IPhysicalWorld *pPhysWorld = gEnv->pPhysicalWorld;
		if (pPhysWorld)
		{
			pPhysWorld->SetupEntityGrid( 2,Vec3(0,0,0),10,10,1,1 );
		}
	}

	m_LTHUMB	= Vec2(0,0);
	m_RTHUMB	= Vec2(0,0);
	IInput* pIInput = GetISystem()->GetIInput(); // Cache IInput pointer.
	pIInput->AddEventListener( this );
	uint32 test = pIInput->HasInputDeviceOfType(eIDT_Gamepad);

	// Sound
	m_ListenerID = LISTENERID_INVALID;
}

CModelViewport::~CModelViewport()
{
	ReleaseObject();

	GetIEditor()->FlushUndo();

	// Save values to registry.
	XmlNodeRef node = CreateXmlNode("Vars");
	m_vars.Serialize( node,false );
	AfxGetApp()->WriteProfileString( "PreviewSettings","Vars",node->getXML() );

	// Sound
	if (gEnv->pSoundSystem)
	{
		// cleaning up all sounds, that might still play
		for (int i=0; i<m_SoundIDs.size(); ++i)
		{
			_smart_ptr<ISound> pSound = gEnv->pSoundSystem->GetSound(m_SoundIDs[i]);

			if (pSound)
				pSound->Stop();
		}

		if (m_ListenerID != LISTENERID_INVALID)
			gEnv->pSoundSystem->RemoveListener(m_ListenerID);
	}

	IInput* pIInput = GetISystem()->GetIInput();
	pIInput->RemoveEventListener( this );

}


BEGIN_MESSAGE_MAP(CModelViewport, CRenderViewport)
	ON_WM_CREATE()
	ON_COMMAND(ID_ANIM_BACK, OnAnimBack)
	ON_COMMAND(ID_ANIM_FAST_BACK, OnAnimFastBack)
	ON_COMMAND(ID_ANIM_FAST_FORWARD, OnAnimFastForward)
	ON_COMMAND(ID_ANIM_FRONT, OnAnimFront)
	ON_COMMAND(ID_ANIM_PLAY, OnAnimPlay)
	ON_WM_LBUTTONDBLCLK()
	ON_WM_KEYDOWN()
	ON_WM_DESTROY()
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CModelViewport message handlers
/////////////////////////////////////////////////////////////////////////////
int CModelViewport::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{

	if (CRenderViewport::OnCreate(lpCreateStruct) == -1)
		return -1;

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::ReleaseObject()
{
	if (m_object)
	{
		m_object->Release();
		m_object = NULL;
	}

	if (m_pCharacterBase)
	{
		if (m_pCharPanel_Animation)
		{
			m_pCharPanel_Animation->ClearAnims();
			m_pCharPanel_Animation->SetFileName( "" );
		}
		m_pCharacterBase=0;
	}

	if (m_pCharacterAnim)
	{
		m_pCharacterAnim=0;
	}

	if (m_weaponModel)
	{
		m_weaponModel->Release();
		m_weaponModel = NULL;
	}
}



//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
bool CModelViewport::OnInputEvent(const SInputEvent &rInputEvent)
{
	float color1[4] = {1,1,1,1};
	//event.deviceId = eDI_XI;
	// "xi_thumbl_up");
	// "xi_thumbl_down");
	// "xi_thumbl_left");
	// "xi_thumbl_right");
	// "xi_thumblx";
	// "xi_thumbly";

	uint32 deviceId = rInputEvent.deviceId;
	if (deviceId == eDI_XI)
	{
		uint32 nKeyID = rInputEvent.keyId;
		uint32 nState = rInputEvent.state;
		f32 nValue		= rInputEvent.value;

		//uint32 lx = strcmp(rInputEvent.keyName,"xi_thumblx");
		if (nKeyID==528)
			m_LTHUMB.x = rInputEvent.value;
		//uint32 ly = strcmp(rInputEvent.keyName,"xi_thumbly");
		if (nKeyID==529)
			m_LTHUMB.y = rInputEvent.value;

		//	uint32 rx = strcmp(rInputEvent.keyName,"xi_thumbrx");
		if (nKeyID==534)
			m_RTHUMB.x = rInputEvent.value;
		//uint32 ry = strcmp(rInputEvent.keyName,"xi_thumbry");
		if (nKeyID==535)
			m_RTHUMB.y = rInputEvent.value;

		/*
		m_renderer->Draw2dLabel(12,g_ypos,1.2f,color1,false,"KeyName: %s",rInputEvent.keyName);
		g_ypos+=15;
		m_renderer->Draw2dLabel(12,g_ypos,1.2f,color1,false,"deviceId: %d",deviceId);
		g_ypos+=15;
		m_renderer->Draw2dLabel(12,g_ypos,1.2f,color1,false,"nKeyID: %d        %d          %f",nKeyID,nState,nValue); 
		g_ypos+=15;
		*/
	}

	return 0;
}



//////////////////////////////////////////////////////////////////////////
void CModelViewport::LoadObject( const CString &fileName,float scale )
{
	m_bPaused = false;

	// Load object.
	CString file = Path::MakeGamePath( fileName );

	bool reload = false;
	if (m_loadedFile == file)
		reload = true;
	m_loadedFile = file;

	SetName( CString("Model View - ") + file );

	ReleaseObject();

	// Do not reload textures.
	//if (m_renderer)
	//m_renderer->FreeResources(FRR_SHADERS|FRR_TEXTURES|FRR_RESTORE);


	// Enables display of warning after model have been loaded.
	CErrorsRecorder errRecorder;


	if (!m_pCharPanel_Animation)	{
		m_pCharPanel_Animation = new CharPanel_Animation(this,this);
		m_rollupIndex = GetIEditor()->AddRollUpPage( ROLLUP_OBJECTS,"Character Animation",m_pCharPanel_Animation );
	}
	if (m_pCharPanel_Animation) {
		m_pCharPanel_Animation->SetFileName( file );
	}

	if (!m_pCharPanel_BAttach)	{
		m_pCharPanel_BAttach = new CharPanel_BAttach(this,this);
		m_rollupIndex = GetIEditor()->AddRollUpPage( ROLLUP_OBJECTS,"Bone Attachment",m_pCharPanel_BAttach );
		GetIEditor()->ExpandRollUpPage( ROLLUP_OBJECTS,m_rollupIndex,false );
	}

	if (IsPreviewableFileType(file))
	{
		// Try Load character.
		if (stricmp( Path::GetExt(file),CRY_CHARACTER_FILE_EXT ) == 0 ||
			stricmp( Path::GetExt(file),CRY_ANIM_GEOMETRY_FILE_EXT ) == 0 ||
			stricmp( Path::GetExt(file),CRY_CHARACTER_DEFINITION_FILE_EXT ) == 0)
		{
			m_pCharacterBase	= m_pAnimationSystem->CreateInstance( file );
			m_pCharacterAnim	=	m_pCharacterBase;
			if (m_pCharacterBase)
			{
				//m_pCharacterBase->AddRef();
				if (m_pCharPanel_Animation && m_pCharacterBase != NULL)
				{
					SetCharacterUIInfo();
				}

				f32  radius = m_pCharacterBase->GetAABB().GetRadius(); //m_pCharacterBase->GetRadius();
				Vec3 center = m_pCharacterBase->GetAABB().GetCenter(); //m_pCharacterBase->GetCenter();

				m_AABB.min = center-Vec3(radius,radius,radius);
				m_AABB.max = center+Vec3(radius,radius,radius);

				if (!reload)
					m_camRadius = center.z + radius;
			}
		}
		else
		{
			LoadStaticObject( file );
		}
	}
	else
	{
		MessageBox( "Preview of this file type not supported","Preview Error",MB_OK|MB_ICONEXCLAMATION );
		return;
	}

	//--------------------------------------------------------------------------------

	if (!s_varsPanel)
	{
		// Regidet variable pannel.
		s_varsPanel = new CPropertiesPanel(this);
		s_varsPanel->AddVars( m_vars.GetVarBlock() );
		s_varsPanelId = GetIEditor()->AddRollUpPage( ROLLUP_OBJECTS,"Debug Options",s_varsPanel );
	}

	//--------------------------------------------------------------------------------


	if (!reload)
	{

		Vec3 v = m_AABB.max - m_AABB.min;
		float radius = v.GetLength()/2.0f;
		m_camRadius = radius*2;
	}

	if (GetIEditor()->IsInPreviewMode())
	{
		Physicalize();
	}

}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::LoadStaticObject(const CString &file )
{
	if (m_object)
		m_object->Release();

	// Load Static object.
	m_object = m_engine->LoadStatObj( file );

	if(!m_object)
	{
		CLogFile::WriteLine( "Loading of object failed." );
		return;
	}
	m_object->AddRef();

	// Generate thumbnail for this cgf.
	CThumbnailGenerator thumbGen;
	thumbGen.GenerateForFile( file );

	m_AABB.min = m_object->GetBoxMin();
	m_AABB.max = m_object->GetBoxMax();
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::SetCharacterUIInfo()
{
	int i;
	// Fill the combo box with the name of the animation sequences
	m_pCharPanel_Animation->ClearAnims();

	// [Sergiy] Add all animations to the animation list box/view
	IAnimationSet* pAnimations = m_pCharacterBase->GetIAnimationSet();
	if (!pAnimations)
		return;

	// mapping name->length of animation
	uint32 numAnims		= pAnimations->numAnimations();
	uint32 numMorphs	= pAnimations->numMorphTargets();
	std::vector< const char* > animNames;
	animNames.reserve(numAnims+numMorphs);
	for (i = 0; i <(numAnims+numMorphs); ++i)
	{
		// the length of the animation is passed in seconds
		animNames.push_back( pAnimations->GetNameByAnimID(i) );
	}

	for (uint32 i = 0; i != animNames.size(); ++i)
		m_pCharPanel_Animation->AddAnimName( animNames[i], i );


	// Fill the bone list
	m_pCharPanel_BAttach->ClearBones();

	int numBones = m_pCharacterBase->GetISkeletonPose()->GetJointCount();
	for (i = 0; i < numBones; i++)
	{
		const char *str = m_pCharacterBase->GetISkeletonPose()->GetJointNameByID(i);
		if (str == NULL)
			break;
		if (strlen(str) > 0)
			m_pCharPanel_BAttach->AddBone(str);
	}
	m_pCharPanel_BAttach->SelectBone( m_attachBone );
}





//////////////////////////////////////////////////////////////////////////
void CModelViewport::UpdateAnimationList()
{
	if (m_pCharacterBase==0)
		return;

	// Fill the combo box with the name of the animation sequences
	m_pCharPanel_Animation->ClearAnims();

	// [Sergiy] Add all animations to the animation list box/view
	IAnimationSet* pAnimations = m_pCharacterBase->GetIAnimationSet();
	if (!pAnimations)
		return;

	// mapping name->length of animation
	uint32 numAnims		= pAnimations->numAnimations();
	uint32 numMorphs	= pAnimations->numMorphTargets();
	std::vector< const char* > animNames;
	animNames.reserve(numAnims+numMorphs);
	for (uint32 i=0; i<(numAnims+numMorphs); ++i)
	{
		const char* aname=pAnimations->GetNameByAnimID(i);

		// the length of the animation is passed in seconds
		f32 length=0;
		if (aname[0]!='#')
			length = pAnimations->GetDuration_sec(i);

		animNames.push_back(aname);
	}

	for (uint32 i = 0; i != animNames.size(); ++i)
		m_pCharPanel_Animation->AddAnimName( animNames[i], i );
}



static IRenderer *s_pRenderer = 0;
static void g_DrawLine(float *v1,float *v2)
{
	Vec3 V1(v1[0],v1[1],v1[2]);
	Vec3 V2(v2[0],v2[1],v2[2]);
	s_pRenderer->GetIRenderAuxGeom()->DrawLine(V1,ColorB(255,255,255,255),V2,ColorB(255,255,255,255));
}

void CModelViewport::OnRender()
{
	ProcessKeys();
	if (m_renderer)
	{
		m_Camera.SetFrustum( m_Camera.GetViewSurfaceX(),m_Camera.GetViewSurfaceZ(), m_Camera.GetFov(), 0.02f,10000 );
		int w = m_rcClient.right - m_rcClient.left;
		int h = m_rcClient.bottom - m_rcClient.top;	
		m_Camera.SetFrustum( w,h, DEG2RAD(mv_fov), 0.0101f,10000.0f );

		if (GetIEditor()->IsInPreviewMode())
		{
			GetISystem()->SetViewCamera( m_Camera );
		}

		Vec3 clearColor = mv_backgroundColor;
		m_renderer->SetClearColor( clearColor );
		m_renderer->SetCamera( m_Camera );

		m_renderer->ClearBuffer(FRT_CLEAR, &ColorF(clearColor, 1.0f));
		m_renderer->ResetToDefault();

		IPhysicalWorld *physWorld = GetIEditor()->GetSystem()->GetIPhysicalWorld();
		if (physWorld)
		{
			s_pRenderer = m_renderer;
			//	physWorld->DrawPhysicsHelperInformation(g_DrawLine);
			//	physWorld->DrawPhysicsHelperInformation(m_renderer);
		}

		if (mv_wireframe)
		{
			m_renderer->SetPolygonMode(R_WIREFRAME_MODE);
			DrawModel();
		} else {
			m_renderer->SetPolygonMode(R_SOLID_MODE);
			DrawModel();
		}
	}
	if (!m_object && !m_pCharacterBase)
	{
		//LoadObject( "Objects\\box.cgf",1 );
	}
}






//////////////////////////////////////////////////////////////////////////
void CModelViewport::DrawSkyBox()
{
	CRenderObject * pObj = m_renderer->EF_GetObject(true, -1);
	pObj->m_II.m_Matrix.SetTranslationMat(GetViewTM().GetTranslation());

	if (m_pSkyboxName)
	{
		m_renderer->EF_AddEf(m_pRESky, SShaderItem(m_pSkyBoxShader), pObj, EFSLIST_GENERAL, 1);
	}
}


void CModelViewport::OnAnimBack() 
{
	// TODO: Add your command handler code here
}

void CModelViewport::OnAnimFastBack() 
{
	// TODO: Add your command handler code here

}

void CModelViewport::OnAnimFastForward() 
{
	// TODO: Add your command handler code here

}

void CModelViewport::OnAnimFront() 
{
	// TODO: Add your command handler code here

}

void CModelViewport::OnAnimPlay() 
{
	// TODO: Add your command handler code here
	if (m_pCharPanel_Animation && m_pCharacterBase != NULL)
	{
		// the name of the currently selected animation
		CString strAnimName = m_pCharPanel_Animation->GetCurrAnimName();
		uint32 looped			= m_pCharPanel_Animation->GetLoopAnimation();
		qqStartAnimation (strAnimName);
	}
}

void CModelViewport::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	// TODO: Add your message handler code here and/or call default

	CRenderViewport::OnLButtonDblClk(nFlags, point);
	Matrix34 tm;
	tm.SetIdentity();
	SetViewTM(tm);
}

void CModelViewport::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
}

void CModelViewport::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	if (m_pCharacterBase)
	{
		int index = -1;
		if (nChar >= 0x30 && nChar <= 0x39)
		{
			if (nChar == 0x30)
				index = 10;
			else
				index = nChar - 0x31;
		}
		if (nChar >= VK_NUMPAD0 && nChar <= VK_NUMPAD9)
		{
			index = nChar - VK_NUMPAD0 + 10;
			if (nChar == VK_NUMPAD0)
				index = 20;
		}
		//if (nFlags& MK_CONTROL

		IAnimationSet* pAnimations = m_pCharacterBase->GetIAnimationSet();
		if (pAnimations)
		{
			uint32 numAnims		= pAnimations->numAnimations();
			uint32 numMorphs	= pAnimations->numMorphTargets();
			if (index >= 0 && index<(numAnims+numMorphs) )
			{
				const char *animName = pAnimations->GetNameByAnimID(index);
				qqStartAnimation (animName);
			}
		}
	}
	CRenderViewport::OnKeyDown(nChar, nRepCnt, nFlags);
}





void CModelViewport::qqStartAnimation (const char* szName)
{
	if (m_pCharPanel_Animation)
	{
		int layer = m_pCharPanel_Animation->GetLayer();

		bool bTransRot2000				= m_pCharPanel_Animation->GetAnimationDrivenMotion();

		bool bMirrorAnimation			= m_pCharPanel_Animation->GetMirrorAnimation();
		bool bLoopAnimation				= m_pCharPanel_Animation->GetLoopAnimation();
		bool bRepeatLastKey				= m_pCharPanel_Animation->GetRepeatLastKey();
		bool bVTimeWarping				= m_pCharPanel_Animation->GetVTimeWarping();
		bool bPartialBodyUpdate		= m_pCharPanel_Animation->GetPartialBodyUpdate();
		bool bDisableMultiplayer	= m_pCharPanel_Animation->GetDisableMultilayer();

		bool bAllowARestart				= m_pCharPanel_Animation->GetAllowAnimationRestart();

		bool bFVE									= m_pCharPanel_Animation->GetFootAnchoring();

		if (szName[0] == '#')
		{
			f32 trans=0.25;
			if (m_pCharPanel_Animation->UseTransitionTime())
				trans = m_pCharPanel_Animation->GetTransitionTime();

			IFacialInstance* pFacialInstance = (m_pCharacterAnim ? m_pCharacterAnim->GetFacialInstance() : 0);
			IFacialModel* pFacialModel = (pFacialInstance ? pFacialInstance->GetFacialModel() : 0);
			IFacialEffectorsLibrary* pEffectorsLibrary = (pFacialModel ? pFacialModel->GetLibrary() : 0);
			IFacialEffector* pFacialEffector = (pEffectorsLibrary ? pEffectorsLibrary->Find(szName + 1) : 0);
			if (pFacialInstance && pFacialEffector)
				pFacialInstance->StartEffectorChannel(pFacialEffector, 1.0, trans, 2.0f * trans);
		}
		else
		{
			CryCharAnimationParams Params (layer);

			ISkeletonAnim* pISkeletonAnim = m_pCharacterAnim->GetISkeletonAnim();
			ISkeletonPose* pISkeletonPose = m_pCharacterAnim->GetISkeletonPose();

			if (pISkeletonAnim)
			{
				pISkeletonAnim->SetAnimationDrivenMotion(bTransRot2000);
				pISkeletonPose->SetFootAnchoring( bFVE );

			//	if (bManualUpdate)
			//		Params.m_nFlags |= CA_MANUAL_UPDATE;
				if (bLoopAnimation)
					Params.m_nFlags |= CA_LOOP_ANIMATION;
				if (bRepeatLastKey)
					Params.m_nFlags |= CA_REPEAT_LAST_KEY;

				if (bVTimeWarping)
					Params.m_nFlags |= CA_TRANSITION_TIMEWARPING;

				if (bPartialBodyUpdate)
					Params.m_nFlags |= CA_PARTIAL_SKELETON_UPDATE;
				if (bDisableMultiplayer)
					Params.m_nFlags |= CA_DISABLE_MULTILAYER;

				if (bAllowARestart)
					Params.m_nFlags |= CA_ALLOW_ANIM_RESTART;

				//Params.m_nFlags |= CA_FORCE_SKELETON_UPDATE;
				//Params.m_nFlags |= CA_START_AFTER;

				Params.m_fLayerBlendIn=0.50f;
				Params.m_fTransTime=m_pCharPanel_Animation->GetTransitionTime();

				if (mv_showSuperimposed)
					pISkeletonPose->SetForceSkeletonUpdate(3);

				int numAnims = m_pCharPanel_Animation->GetSelectedAnimations();
				if (numAnims>1)
				{
					CString strAnimName0,strAnimName1;
					strAnimName0 = m_pCharPanel_Animation->GetCurrAnimName(1);
					strAnimName1 = m_pCharPanel_Animation->GetCurrAnimName(2);

					f32 InterpVal=m_pCharPanel_Animation->GetMotionInterpolationSlider();
					Params.m_fInterpolation = InterpVal;

					pISkeletonPose->SetSuperimposed(mv_showSuperimposed);
					AimPoses aim_poses = AttachAimPoses(szName);
					Params.m_fUserData[0]=2;
					pISkeletonAnim->StartAnimation( strAnimName0,strAnimName1, aim_poses.n0,aim_poses.n1, Params);
				}
				else
				{
					pISkeletonPose->SetSuperimposed(mv_showSuperimposed);
					Params.m_fUserData[0]=1;
					AimPoses aim_poses = AttachAimPoses(szName);
					uint32 IsNULL = (szName[0]=='n' && szName[1]=='u' && szName[2]=='l' && szName[3]=='l');
					if (IsNULL==0)
 					  pISkeletonAnim->StartAnimation( szName,0, aim_poses.n0,aim_poses.n1, Params);
				}
			}

			f32 lval=m_pCharPanel_Animation->GetLinearMorphSlider();

		}
	}
}


//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
AimPoses CModelViewport::AttachAimPoses( const char* szName )
{
	//	if ( strcmp(szName,"_#combat_idle_rifle_01")==0 )
	//		return AimPoses("combat_idleAimPoses_rifle_01",0);


	if ( strcmp(szName,"_#crouch_idle_rifle_01")==0 )
		return AimPoses("_#crouch_peekIdleAimPoses_rifle_left_01",0);
	if ( strcmp(szName,"_#crouch_idle_rifle_02")==0 )
		return AimPoses("_#crouch_peekIdleAimPoses_rifle_right_01",0);
	if ( strcmp(szName,"_#crouch_idle_rifle_03")==0 )
		return AimPoses("_#crouch_peekIdleAimPoses_rifle_leftReverse_01",0);                                                    
	if ( strcmp(szName,"_#crouch_idle_rifle_04")==0 )
		return AimPoses("_#crouch_peekIdleAimPoses_rifle_rightReverse_01",0);                                                    

	if ( strcmp(szName,"_#crouch_idle_rifle_05")==0 )
		return AimPoses("_#crouch_peekIdleAimPoses_pistol_left_01",0);
	if ( strcmp(szName,"_#crouch_idle_rifle_06")==0 )
		return AimPoses("_#crouch_peekIdleAimPoses_pistol_right_01",0);
	if ( strcmp(szName,"_#crouch_idle_rifle_07")==0 )
		return AimPoses("_#crouch_peekIdleAimPoses_pistol_leftReverse_01",0);
	if ( strcmp(szName,"_#crouch_idle_rifle_08")==0 )
		return AimPoses("_#crouch_peekIdleAimPoses_pistol_rightReverse_01",0);



	if ( strcmp(szName,"_#combat_idle_rifle_01")==0 )
		return AimPoses("_#combat_peekIdleAimPoses_rifle_left_01",0);
	if ( strcmp(szName,"_#combat_idle_rifle_02")==0 )
		return AimPoses("_#combat_peekIdleAimPoses_rifle_right_01",0);
	if ( strcmp(szName,"_#combat_idle_rifle_03")==0 )
		return AimPoses("_#combat_peekIdleAimPoses_rifle_leftReverse_01",0);
	if ( strcmp(szName,"_#combat_idle_rifle_04")==0 )
		return AimPoses("_#combat_peekIdleAimPoses_rifle_rightReverse_01",0);

	if ( strcmp(szName,"_#combat_idle_rifle_05")==0 )
		return AimPoses("_#combat_peekIdleAimPoses_pistol_left_01",0);
	if ( strcmp(szName,"_#combat_idle_rifle_06")==0 )
		return AimPoses("_#combat_peekIdleAimPoses_pistol_right_01",0);
	if ( strcmp(szName,"_#combat_idle_rifle_07")==0 )
		return AimPoses("_#combat_peekIdleAimPoses_pistol_leftReverse_01",0);
	if ( strcmp(szName,"_#combat_idle_rifle_08")==0 )
		return AimPoses("_#combat_peekIdleAimPoses_pistol_rightReverse_01",0);



	if ( strcmp(szName,"combat_idleSTEPROTATE_rifle_01")==0 )
		return AimPoses("combat_idleAimPoses_rifle_01",0);
	if ( strcmp(szName,"crouch_idleSTEPROTATE_rifle_01")==0 )
		return AimPoses("crouch_idleAimPoses_rifle_01",0);



	//idles
	if ( strcmp(szName,"_#AIMcombat_idle_rifle_01")==0 )
		return AimPoses("combat_idleAimPoses_rifle_01",0);
	if ( strcmp(szName,"_#AIMcombat_toCrouch_rifle_01")==0 )
		return AimPoses("combat_idleAimPoses_rifle_01","crouch_idleAimPoses_rifle_01");
	if ( strcmp(szName,"_#AIMcrouch_idle_rifle_01")==0 )
		return AimPoses("crouch_idleAimPoses_rifle_01",0);
	if ( strcmp(szName,"_#AIMcrouch_toCombat_rifle_01")==0 )
		return AimPoses("crouch_idleAimPoses_rifle_01","combat_idleAimPoses_rifle_01");

	//idles
	if ( strcmp(szName,"combat_idleSTEPROTATE_rifle_01")==0 )
		return AimPoses("combat_idleAimPoses_rifle_01",0);
	if ( strcmp(szName,"combat_toCrouch_rifle_01")==0 )
		return AimPoses("combat_idleAimPoses_rifle_01","crouch_idleAimPoses_rifle_01");
	if ( strcmp(szName,"crouch_idle_rifle_01")==0 )
		return AimPoses("crouch_idleAimPoses_rifle_01",0);
	if ( strcmp(szName,"crouch_toCombat_rifle_01")==0 )
		return AimPoses("crouch_idleAimPoses_rifle_01","combat_idleAimPoses_rifle_01");

	//	if ( strcmp(szName,"__crouch_idle_rifle_01")==0 )
	//		return AimPoses("crouch_idleAimPoses_rifle_01",0);
	//	if ( strcmp(szName,"__stealth_idle_rifle_01")==0 )
	//		return AimPoses("stealth_idleAimPoses_rifle_01",0);
	//	if ( strcmp(szName,"__prone_idle_rifle_01")==0 )
	//		return AimPoses("prone_idleAimPoses_rifle_01",0);



	if ( strcmp(szName,"crouch_toStealth_rifle_01")==0 )
		return AimPoses("crouch_idleAimPoses_rifle_01","stealth_idleAimPoses_rifle_01");
	if ( strcmp(szName,"combat_toStealth_rifle_01")==0 )
		return AimPoses("combat_idleAimPoses_rifle_01","stealth_idleAimPoses_rifle_01");
	if ( strcmp(szName,"stealth_toCombat_rifle_01")==0 )
		return AimPoses("stealth_idleAimPoses_rifle_01","combat_idleAimPoses_rifle_01");
	if ( strcmp(szName,"stealth_toCrouch_rifle_01")==0 )
		return AimPoses("stealth_idleAimPoses_rifle_01","crouch_idleAimPoses_rifle_01");

	if ( strcmp(szName,"null")==0 )
		return AimPoses("combat_idleAimPoses_pistol_01",0);


	//---------------------------------------------------------
	//----    all rifles
	//---------------------------------------------------------
	if ( strcmp(szName,"_COMBAT_RUN2IDLE_RIFLE_01")==0 )
		return AimPoses("combat_idleAimPoses_rifle_01",0);
	if ( strcmp(szName,"_COMBAT_IDLE2RUN_RIFLE_01")==0 )
		return AimPoses("combat_idleAimPoses_rifle_01",0);


	if ( strcmp(szName,"_COMBAT_IDLESTEP_RIFLE_01")==0 )  //this one will replace "_COMBAT_ROTATE_RIFLE_01" one day
		return AimPoses("combat_idleAimPoses_rifle_01",0);

	if ( strcmp(szName,"_COMBAT_IDLESTEPROTATE_RIFLE_01")==0 )
		return AimPoses("combat_idleAimPoses_rifle_01",0);
	if ( strcmp(szName,"_COMBAT_RUNSTRAFE_RIFLE_01")==0 )
		return AimPoses("combat_runAimPoses_rifle_01",0);
	if ( strcmp(szName,"_COMBAT_WALKSTRAFE_RIFLE_01")==0 )
		return AimPoses("combat_walkAimPoses_rifle_01",0);

	if ( strcmp(szName,"_STEALTH_IDLESTEPROTATE_RIFLE_01")==0 )
		return AimPoses("stealth_idleAimPoses_rifle_01",0);
	if ( strcmp(szName,"_STEALTH_RUNSTRAFE_RIFLE_01")==0 )
		return AimPoses("stealth_runAimPoses_rifle_01",0);
	if ( strcmp(szName,"_STEALTH_WALKSTRAFE_RIFLE_01")==0 )
		return AimPoses("stealth_walkAimPoses_rifle_01",0);

	if ( strcmp(szName,"_CROUCH_IDLESTEPROTATE_RIFLE_01")==0 )
		return AimPoses("crouch_idleAimPoses_rifle_01",0);
	if ( strcmp(szName,"_CROUCH_RUNSTRAFE_RIFLE_01")==0 )
		return AimPoses("stealth_runAimPoses_rifle_01",0);
	if ( strcmp(szName,"_CROUCH_WALKSTRAFE_RIFLE_01")==0 )
		return AimPoses("crouch_walkAimPoses_rifle_01",0);

	if ( strcmp(szName,"_PRONE_IDLESTEPROTATE_RIFLE_01")==0 )
		return AimPoses("prone_idleAimPoses_rifle_01",0);
	if ( strcmp(szName,"_PRONE_WALKSTRAFE_RIFLE_01")==0 )
		return AimPoses("prone_idleAimPoses_rifle_01",0);

	//---------------------------------------------------------
	//----    all pistols
	//---------------------------------------------------------
	if ( strcmp(szName,"_COMBAT_IDLESTEPROTATE_PISTOL_01")==0 )
		return AimPoses("combat_idleAimPoses_pistol_01",0);
	if ( strcmp(szName,"_COMBAT_RUNSTRAFE_PISTOL_01")==0 )
		return AimPoses("combat_runAimPoses_pistol_01",0);
	if ( strcmp(szName,"_COMBAT_WALKSTRAFE_PISTOL_01")==0 )
		return AimPoses("combat_walkAimPoses_pistol_01",0);

	if ( strcmp(szName,"_STEALTH_IDLESTEPROTATE_PISTOL_01")==0 )
		return AimPoses("stealth_idleAimPoses_pistol_01",0);
	if ( strcmp(szName,"_STEALTH_RUNSTRAFE_PISTOL_01")==0 )
		return AimPoses("stealth_runAimPoses_pistol_01",0);
	if ( strcmp(szName,"_STEALTH_WALKSTRAFE_PISTOL_01")==0 )
		return AimPoses("stealth_walkAimPoses_pistol_01",0);

	if ( strcmp(szName,"_CROUCH_IDLESTEPROTATE_PISTOL_01")==0 )
		return AimPoses("crouch_idleAimPoses_pistol_01",0);
	if ( strcmp(szName,"_CROUCH_RUNSTRAFE_PISTOL_01")==0 )
		return AimPoses("stealth_runAimPoses_pistol_01",0);
	if ( strcmp(szName,"_CROUCH_WALKSTRAFE_PISTOL_01")==0 )
		return AimPoses("crouch_walkAimPoses_pistol_01",0);

	if ( strcmp(szName,"_PRONE_IDLESTEPROTATE_PISTOL_01")==0 )
		return AimPoses("prone_idleAimPoses_pistol_01",0);
	if ( strcmp(szName,"_PRONE_WALKSTRAFE_PISTOL_01")==0 )
		return AimPoses("prone_idleAimPoses_pistol_01",0);


	//---------------------------------------------------------
	//----    all mgs
	//---------------------------------------------------------
	if ( strcmp(szName,"_COMBAT_IDLESTEPROTATE_MG_01")==0 )
		return AimPoses("combat_idleAimPoses_mg_01",0);
	if ( strcmp(szName,"_COMBAT_RUNSTRAFE_MG_01")==0 )
		return AimPoses("combat_runAimPoses_mg_01",0);
	if ( strcmp(szName,"_COMBAT_WALKSTRAFE_MG_01")==0 )
		return AimPoses("combat_walkAimPoses_mg_01",0);

	if ( strcmp(szName,"_STEALTH_IDLESTEPROTATE_MG_01")==0 )
		return AimPoses("stealth_idleAimPoses_mg_01",0);
	if ( strcmp(szName,"_STEALTH_RUNSTRAFE_MG_01")==0 )
		return AimPoses("stealth_runAimPoses_mg_01",0);
	if ( strcmp(szName,"_STEALTH_WALKSTRAFE_MG_01")==0 )
		return AimPoses("stealth_walkAimPoses_mg_01",0);

	if ( strcmp(szName,"_CROUCH_IDLESTEPROTATE_MG_01")==0 )
		return AimPoses("crouch_idleAimPoses_mg_01",0);
	if ( strcmp(szName,"_CROUCH_RUNSTRAFE_MG_01")==0 )
		return AimPoses("stealth_runAimPoses_mg_01",0);
	if ( strcmp(szName,"_CROUCH_WALKSTRAFE_MG_01")==0 )
		return AimPoses("crouch_walkAimPoses_mg_01",0);

	if ( strcmp(szName,"_PRONE_IDLESTEPROTATE_MG_01")==0 )
		return AimPoses("prone_idleAimPoses_mg_01",0);
	if ( strcmp(szName,"_PRONE_WALKSTRAFE_MG_01")==0 )
		return AimPoses("prone_idleAimPoses_mg_01",0);


	//---------------------------------------------------------
	//----    all rocket
	//---------------------------------------------------------
	if ( strcmp(szName,"_COMBAT_IDLESTEPROTATE_ROCKET_01")==0 )
		return AimPoses("combat_idleAimPoses_rocket_01",0);
	if ( strcmp(szName,"_COMBAT_RUNSTRAFE_ROCKET_01")==0 )
		return AimPoses("combat_runAimPoses_rocket_01",0);
	if ( strcmp(szName,"_COMBAT_WALKSTRAFE_ROCKET_01")==0 )
		return AimPoses("combat_walkAimPoses_rocket_01",0);

	if ( strcmp(szName,"_STEALTH_IDLESTEPROTATE_ROCKET_01")==0 )
		return AimPoses("stealth_idleAimPoses_rocket_01",0);
	if ( strcmp(szName,"_STEALTH_RUNSTRAFE_ROCKET_01")==0 )
		return AimPoses("stealth_runAimPoses_rocket_01",0);
	if ( strcmp(szName,"_STEALTH_WALKSTRAFE_ROCKET_01")==0 )
		return AimPoses("stealth_walkAimPoses_rocket_01",0);

	if ( strcmp(szName,"_CROUCH_IDLESTEPROTATE_ROCKET_01")==0 )
		return AimPoses("crouch_idleAimPoses_rocket_01",0);
	if ( strcmp(szName,"_CROUCH_RUNSTRAFE_ROCKET_01")==0 )
		return AimPoses("stealth_runAimPoses_rocket_01",0);
	if ( strcmp(szName,"_CROUCH_WALKSTRAFE_ROCKET_01")==0 )
		return AimPoses("crouch_walkAimPoses_rocket_01",0);

	if ( strcmp(szName,"_PRONE_IDLESTEPROTATE_ROCKET_01")==0 )
		return AimPoses("prone_idleAimPoses_rocket_01",0);
	if ( strcmp(szName,"_PRONE_WALKSTRAFE_ROCKET_01")==0 )
		return AimPoses("prone_idleAimPoses_rocket_01",0);



	//---------------------------------------------------------
	//----    all shotgun
	//---------------------------------------------------------
	if ( strcmp(szName,"_COMBAT_ROTATE_SHOTGUN_01")==0 )
		return AimPoses("combat_idleAimPoses_shotgun_01",0);
	if ( strcmp(szName,"_COMBAT_RUNSTRAFE_SHOTGUN_01")==0 )
		return AimPoses("combat_runAimPoses_shotgun_01",0);
	if ( strcmp(szName,"_COMBAT_WALKSTRAFE_SHOTGUN_01")==0 )
		return AimPoses("combat_walkAimPoses_shotgun_01",0);

	if ( strcmp(szName,"_STEALTH_ROTATE_SHOTGUN_01")==0 )
		return AimPoses("stealth_idleAimPoses_shotgun_01",0);
	if ( strcmp(szName,"_STEALTH_RUNSTRAFE_SHOTGUN_01")==0 )
		return AimPoses("stealth_runAimPoses_shotgun_01",0);
	if ( strcmp(szName,"_STEALTH_WALKSTRAFE_SHOTGUN_01")==0 )
		return AimPoses("stealth_walkAimPoses_shotgun_01",0);

	if ( strcmp(szName,"_CROUCH_ROTATE_SHOTGUN_01")==0 )
		return AimPoses("crouch_idleAimPoses_shotgun_01",0);
	if ( strcmp(szName,"_CROUCH_RUNSTRAFE_SHOTGUN_01")==0 )
		return AimPoses("stealth_runAimPoses_shotgun_01",0);
	if ( strcmp(szName,"_CROUCH_WALKSTRAFE_SHOTGUN_01")==0 )
		return AimPoses("crouch_walkAimPoses_shotgun_01",0);

	if ( strcmp(szName,"_PRONE_ROTATE_SHOTGUN_01")==0 )
		return AimPoses("prone_idleAimPoses_shotgun_01",0);
	if ( strcmp(szName,"_PRONE_WALKSTRAFE_SHOTGUN_01")==0 )
		return AimPoses("prone_idleAimPoses_shotgun_01",0);



	//---------------------------------------------------------
	//----    all DualPistol
	//---------------------------------------------------------
	if ( strcmp(szName,"_COMBAT_ROTATE_DUALPISTOL_01")==0 )
		return AimPoses("combat_idleAimPoses_dualPistol_01",0);
	if ( strcmp(szName,"_COMBAT_RUNSTRAFE_DUALPISTOL_01")==0 )
		return AimPoses("combat_runAimPoses_dualPistol_01",0);
	if ( strcmp(szName,"_COMBAT_WALKSTRAFE_DUALPISTOL_01")==0 )
		return AimPoses("combat_walkAimPoses_dualPistol_01",0);

	if ( strcmp(szName,"_STEALTH_ROTATE_DUALPISTOL_01")==0 )
		return AimPoses("stealth_idleAimPoses_dualPistol_01",0);
	if ( strcmp(szName,"_STEALTH_RUNSTRAFE_DUALPISTOL_01")==0 )
		return AimPoses("stealth_runAimPoses_dualPistol_01",0);
	if ( strcmp(szName,"_STEALTH_WALKSTRAFE_DUALPISTOL_01")==0 )
		return AimPoses("stealth_walkAimPoses_dualPistol_01",0);

	if ( strcmp(szName,"_CROUCH_ROTATE_DUALPISTOL_01")==0 )
		return AimPoses("crouch_idleAimPoses_dualPistol_01",0);
	if ( strcmp(szName,"_CROUCH_RUNSTRAFE_DUALPISTOL_01")==0 )
		return AimPoses("stealth_runAimPoses_dualPistol_01",0);
	if ( strcmp(szName,"_CROUCH_WALKSTRAFE_DUALPISTOL_01")==0 )
		return AimPoses("crouch_walkAimPoses_dualPistol_01",0);

	if ( strcmp(szName,"_PRONE_ROTATE_DUALPISTOL_01")==0 )
		return AimPoses("prone_idleAimPoses_dualPistol_01",0);
	if ( strcmp(szName,"_PRONE_WALKSTRAFE_DUALPISTOL_01")==0 )
		return AimPoses("prone_idleAimPoses_dualPistol_01",0);




	if ( strcmp(szName,"_crouch_2peekIdleAim_rifle_left_01")==0 )
		return AimPoses("crouch_peekIdleAimPoses_rifle_left_01",0);

	//	_crouch_0idle_rifle_01			=	crouch\crouch_idle_rifle_01.caf
	//		_crouch_1toPeekIdle_rifle_left_01	=	crouch\crouch_toPeekIdle_rifle_left_01.caf
	//		_crouch_2peekIdleAim_rifle_left_01	=	crouch\crouch_peekIdleAim_rifle_left_01.caf
	//		_crouch_3fromPeekIdle_rifle_left_01 	= 	crouch\crouch_fromPeekIdle_rifle_left_01.caf

	return AimPoses(0,0);
}



void CModelViewport::OnLightColor( IVariable *var )
{
}



//////////////////////////////////////////////////////////////////////////
void CModelViewport::OnShowNormals( IVariable *var )
{
	bool enable = mv_showNormals;
	GetIEditor()->SetConsoleVar( "r_ShowNormals",(enable)?1:0 );
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::OnShowTangents( IVariable *var )
{
	bool enable = mv_showTangents;
	GetIEditor()->SetConsoleVar( "r_ShowTangents",(enable)?1:0 );
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::OnShowTextureUsage( IVariable *var )
{
	bool enable = mv_showTextureUsage;
	GetIEditor()->SetConsoleVar( "r_TexLog",(enable)?2:0 );
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::OnShowAllTextures( IVariable *var )
{
	bool enable = mv_showAllTextures;
	GetIEditor()->SetConsoleVar( "r_TexLog",(enable)?4:0 );
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::OnShowPhysics( IVariable *var )
{
	bool enable = mv_showPhysics;
	GetIEditor()->SetConsoleVar( "p_draw_helpers",(enable)?1:0 ); // set all bits.
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::OnShowOcclusion( IVariable *var )
{
	bool enable = mv_showOcclusion;
	GetIEditor()->SetConsoleVar( "e_debug_draw",(enable)?12:0 ); // set all bits.
}


//////////////////////////////////////////////////////////////////////////
void CModelViewport::AttachObjectToBone( const CString &model,const CString &bone )
{
	if (!m_pCharacterBase)
		return;

	IAttachmentObject* pBindable = NULL;
	m_attachedCharacter = m_pAnimationSystem->CreateInstance(model);
	if (m_attachedCharacter)
		m_attachedCharacter->AddRef();

	//if (m_attachedCharacter)
	//pBindable = m_attachedCharacter;

	if (m_weaponModel)
	{
		SAFE_RELEASE(m_weaponModel);
	}

	if (!m_attachedCharacter)
	{
		m_weaponModel = m_engine->LoadStatObj (model);
		m_weaponModel->AddRef();
		//if (m_weaponModel)
		//pBindable = m_weaponModel;
	}

	m_attachBone = bone;
	if(!pBindable)
	{
		CString str;
		str.Format( "Loading of weapon model %s failed.",(const char*)model );
		AfxMessageBox( str );
		CLogFile::WriteLine( str );
		return;
	}

	IAttachmentManager* pIAttachments = m_pCharacterBase->GetIAttachmentManager();
	if (m_attachedCharacter) {
		// pIAttachments->AttachToBone ( pBindable,m_pCharacterBase->GetModel()->GetBoneByName(bone));
	} else {
		//pIAttachments->CreateAttachmentOld( bone, pBindable, false);
		IAttachment* pAttachment = pIAttachments->CreateAttachment( "BoneAttachment", CA_BONE,bone );
		assert(pAttachment);
		pAttachment->AddBinding(pBindable);
	}
}


//////////////////////////////////////////////////////////////////////////
void CModelViewport::AttachObjectToFace( const CString &model )
{
	if (!m_pCharacterBase)
		return;

	if (m_weaponModel)
		m_weaponModel->Release();

	//	m_pCharacterBase->ResetAnimations();
	//	m_pCharacterBase->InitBones(0);
	//	m_pCharacterBase->SetResetMode(1);

	IAttachmentObject* pBindable = NULL;
	m_weaponModel = m_engine->LoadStatObj (model);
	m_weaponModel->AddRef();
	//if (m_weaponModel)
	//pBindable = m_weaponModel;

	if(!pBindable)
	{
		CString str;
		str.Format( "Loading of weapon model %s failed.",(const char*)model );
		AfxMessageBox( str );
		CLogFile::WriteLine( str );
		return;
	}
	IAttachmentManager* pAttachments = m_pCharacterBase->GetIAttachmentManager();
	IAttachment* pIAttachment = pAttachments->CreateAttachment("FaceAttachment",CA_FACE);
	pIAttachment->AddBinding(pBindable);

}


void CModelViewport::StopAnimationInLayer(int nLayer)
{
	if (!m_pCharacterBase)
		return;
	m_pCharacterBase->GetISkeletonAnim()->StopAnimationInLayer(nLayer,0.50f);
}


//////////////////////////////////////////////////////////////////////////
void CModelViewport::OnShowShaders( IVariable *var )
{
	bool bEnable = mv_showShaders;
	GetIEditor()->SetConsoleVar("r_ProfileShaders",bEnable);
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::OnDisableVisibility( IVariable *var )
{
	bool bDisableVisibility = mv_disableVisibility;
	//	GetIEditor()->Get3DEngine()->GetBuildingManager()->EnableVisibility( !bDisableVisibility );
}
void CModelViewport::OnDestroy()
{
	m_pCharPanel_Animation = 0;

	ReleaseObject();
	if (m_pRESky)
		m_pRESky->Release();

	if (m_rollupIndex)
	{
		GetIEditor()->RemoveRollUpPage( ROLLUP_OBJECTS,m_rollupIndex );
		m_rollupIndex = 0;
	}
	if (m_rollupIndex2)
	{
		GetIEditor()->RemoveRollUpPage( ROLLUP_OBJECTS,m_rollupIndex2 );
		m_rollupIndex2 = 0;
	}
	if (s_varsPanelId)
	{
		GetIEditor()->RemoveRollUpPage( ROLLUP_OBJECTS,s_varsPanelId );
		s_varsPanelId = 0;
	}
	m_pCharPanel_Animation = 0;
	m_pCharPanel_Preset = 0;
	s_varsPanel = 0;

	CRenderViewport::OnDestroy();
}

void CModelViewport::OnSubmeshSetChanged()
{
	SetCharacterUIInfo();
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::OnActivate()
{

}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::OnDeactivate()
{
}

void CModelViewport::Update()
{

	CRenderViewport::Update();

	if (gEnv->pSoundSystem)
	{
		// Process Sounds
		Vec3 vPos = m_AnimatedCharacter.t;

		for (int i=0; i<m_SoundIDs.size(); )
		{
			_smart_ptr<ISound> pSound = gEnv->pSoundSystem->GetSound(m_SoundIDs[i]);

			if (pSound)
			{
				pSound->SetPosition(vPos);
				++i;
			}
			else
			{
				// remove invalid soundID
				if (i < m_SoundIDs.size()-1)
					m_SoundIDs[i] = m_SoundIDs[m_SoundIDs.size()-1];

				m_SoundIDs.pop_back();
			}
		}

		// Process Listener
		IListener *pListener = NULL;

		if (m_ListenerID == LISTENERID_INVALID)
		{
			// set up a new one if we dont have it yet
			m_ListenerID = gEnv->pSoundSystem->CreateListener();
			pListener = gEnv->pSoundSystem->GetListener(m_ListenerID);
			pListener->SetRecordLevel(1.0f);
			pListener->SetActive(true);
			gEnv->pSoundSystem->Update(eSoundUpdateMode_All);
		}

		// update
		pListener = gEnv->pSoundSystem->GetListener(m_ListenerID);

		if (pListener)
			pListener->SetMatrix(m_Camera.GetMatrix());
		//			pListener->SetPosition(m_Camera.GetPosition());

	}

	// Update particle effects.
	m_effectManager.Update();

}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::SetCustomMaterial( CMaterial *pMaterial )
{
	m_pCurrentMaterial = pMaterial;
	//if (m_pCharPanel_Animation)
	//	m_pCharPanel_Animation->SetMaterial(pMaterial);
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CModelViewport::GetMaterial()
{
	if (m_pCurrentMaterial)
		return m_pCurrentMaterial;
	else
	{
		IMaterial *pMtl = 0;
		if (m_object)
			pMtl = m_object->GetMaterial();
		else if (m_pCharacterBase)
			pMtl = m_pCharacterBase->GetMaterial();

		CMaterial *pCMaterial = GetIEditor()->GetMaterialManager()->FromIMaterial(pMtl);
		return pCMaterial;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CModelViewport::CanDrop( CPoint point,IDataBaseItem *pItem )
{
	if (!pItem)
		return false;

	if (pItem->GetType() == EDB_TYPE_MATERIAL)
	{
		SetCustomMaterial( (CMaterial*)pItem );
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::Drop( CPoint point,IDataBaseItem *pItem )
{
	if (!pItem)
	{
		SetCustomMaterial(NULL);
		return;
	}

	if (pItem->GetType() == EDB_TYPE_MATERIAL)
	{
		SetCustomMaterial( (CMaterial*)pItem );
	}
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::Physicalize()
{
	IPhysicalWorld *pPhysWorld = gEnv->pPhysicalWorld;
	if (!pPhysWorld)
		return;

	if (!m_object && !m_pCharacterBase)
		return;

	if (m_pPhysicalEntity)
		pPhysWorld->DestroyPhysicalEntity(m_pPhysicalEntity);

	m_pPhysicalEntity = pPhysWorld->CreatePhysicalEntity(PE_STATIC,NULL,NULL,0);
	if(!m_pPhysicalEntity)
		return;

	pe_action_remove_all_parts remove_all;
	m_pPhysicalEntity->Action(&remove_all);

	// Add geometries.
	if (m_object)
	{
		pe_geomparams params;
		params.flags = geom_colltype_ray;
		for (int i = 0; i < 4; i++)
		{
			if (m_object->GetPhysGeom(i))
				m_pPhysicalEntity->AddGeometry( m_object->GetPhysGeom(i),&params );
		}
		// Add all sub mesh geometries.
		for (int nobj = 0; nobj < m_object->GetSubObjectCount(); nobj++)
		{
			IStatObj *pStatObj = m_object->GetSubObject(nobj)->pStatObj;
			if (pStatObj)
			{
				params.pMtx3x4 = &m_object->GetSubObject(nobj)->tm;
				for (int i = 0; i < 4; i++)
				{
					if (pStatObj->GetPhysGeom(i))
						m_pPhysicalEntity->AddGeometry( pStatObj->GetPhysGeom(i),&params );
				}
			}
		}
	}
	else if (m_pCharacterBase)
	{
		m_pCharacterBase->GetISkeletonPose()->BuildPhysicalEntity(m_pPhysicalEntity,0,-1,1);
	}

	Matrix34 tm;
	tm.SetIdentity();
	pe_params_pos par_pos;
	par_pos.pMtx3x4 = &tm;
	m_pPhysicalEntity->SetParams(&par_pos);

	// Set materials.
	CMaterial *pMaterial = GetMaterial();
	if (pMaterial)
	{
		// Assign custom material to physics.
		int surfaceTypesId[MAX_SUB_MATERIALS];
		memset( surfaceTypesId,0,sizeof(surfaceTypesId) );
		int numIds = pMaterial->GetMatInfo()->FillSurfaceTypeIds(surfaceTypesId);

		pe_params_part ppart;
		ppart.nMats = numIds;
		ppart.pMatMapping = surfaceTypesId;
		m_pPhysicalEntity->SetParams( &ppart );
	}
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::SetPaused(bool bPaused)
{
	if (m_bPaused != bPaused)
	{
		if (bPaused)
		{
			if (GetCharacterBase())
				GetCharacterBase()->SetAnimationSpeed(0.0f);
			if (GetCharacterAnim())
				GetCharacterAnim()->SetAnimationSpeed(0.0f);
		}
		else
		{
			if (GetCharacterBase())
				GetCharacterBase()->SetAnimationSpeed(1.0f);
			if (GetCharacterAnim())
				GetCharacterAnim()->SetAnimationSpeed(1.0f);
		}

		m_bPaused = bPaused;
	}
}





//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------
void CModelViewport::DrawModel()
{

	m_CamPos=GetCamera().GetPosition();	
	//GetISystem()->SetViewCamera( m_Camera );
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	m_renderer->EF_StartEf();

	//////////////////////////////////////////////////////////////////////////
	// Draw lights.
	//////////////////////////////////////////////////////////////////////////
	if (mv_lighting == true)
	{
		uint32 numLights = m_VPLights.size();
		for (int i=0; i<numLights; i++)
		{
			pAuxGeom->DrawSphere( m_VPLights[i].m_Origin,0.2f,ColorB(255,255,0,255) );
		}
	}


	if (mv_showSkyBox)
		DrawSkyBox();

	GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ca_DrawTangents")->Set( mv_showTangents );
	GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ca_DrawBinormals")->Set( mv_showBinormals );
	GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ca_DrawNormals")->Set( mv_showNormals );
	if(GetIEditor()->Get3DEngine())
	{
		float nTextPosX=101-20, nTextPosY=-2, nTextStepY=3;
		GetIEditor()->Get3DEngine()->DisplayInfo(nTextPosX, nTextPosY, nTextStepY,false);
	}





	if (mv_animateLights)
		m_LightRotationRadiant += m_AverageFrameTime;


	Matrix33 LightRot33	=	Matrix33::CreateRotationZ(m_LightRotationRadiant);

	uint32 numLights = m_VPLights.size();

	float orbit = 15;

	Vec3 LPos0 = Vec3(-orbit,orbit,orbit); 

	m_VPLights[0].m_Origin = LightRot33*LPos0;

	Vec3 d0 = mv_lightDiffuseColor0;
	m_VPLights[0].SetLightColor(ColorF(d0.x*MULTIPLY,d0.y*MULTIPLY,d0.z*MULTIPLY,0));

	m_VPLights[0].m_SpecMult = 1.0f;

	m_VPLights[0].m_fRadius = 400;
	m_VPLights[0].m_fStartRadius = 5;
	m_VPLights[0].m_fEndRadius = 400;
	m_VPLights[0].m_Flags |= DLF_POINT;

	//-----------------------------------------------

	Vec3 LPos1 = Vec3(-orbit,-orbit,-orbit/2); 
	m_VPLights[1].m_Origin = LightRot33*LPos1;

	Vec3 d1 = mv_lightDiffuseColor1;
	m_VPLights[1].SetLightColor(ColorF(d1.x*MULTIPLY,d1.y*MULTIPLY,d1.z*MULTIPLY,0));

	m_VPLights[1].m_SpecMult = 1.0f;

	m_VPLights[1].m_fRadius = 400;
	m_VPLights[1].m_fStartRadius = 5;
	m_VPLights[1].m_fEndRadius = 400;
	m_VPLights[1].m_Flags |= DLF_POINT;

	//---------------------------------------------

	Vec3 LPos2 = Vec3(orbit,-orbit,0); 
	m_VPLights[2].m_Origin = LightRot33*LPos2;

	Vec3 d2 = mv_lightDiffuseColor2;
	m_VPLights[2].SetLightColor(ColorF(d2.x*MULTIPLY,d2.y*MULTIPLY,d2.z*MULTIPLY,0));

	m_VPLights[2].m_SpecMult = 1.0f;

	m_VPLights[2].m_fRadius = 400;
	m_VPLights[2].m_fStartRadius = 5;
	m_VPLights[2].m_fEndRadius = 400;
	m_VPLights[2].m_Flags |= DLF_POINT;


	if (mv_lighting == true)
	{
		// Add lights.
		uint32 numLights = m_VPLights.size();
		if (numLights==1)
		{
			m_renderer->EF_ADDDlight( &m_VPLights[0] );
		}

		if (numLights==2)
		{
			m_renderer->EF_ADDDlight( &m_VPLights[0] );
			m_renderer->EF_ADDDlight( &m_VPLights[1] );
		}

		if (numLights==3)
		{
			m_renderer->EF_ADDDlight( &m_VPLights[0] );
			m_renderer->EF_ADDDlight( &m_VPLights[1] );
			m_renderer->EF_ADDDlight( &m_VPLights[2] );
		}

	}

	//-------------------------------------------------------------
	//------           Render physical Proxy                 ------
	//-------------------------------------------------------------
	m_renderer->Set2DMode( true,m_rcClient.right,m_rcClient.bottom );
	if (m_pPhysicalEntity)
	{
		CPoint mousePoint;
		GetCursorPos(&mousePoint);
		ScreenToClient(&mousePoint);
		Vec3 raySrc,rayDir;
		ViewToWorldRay( mousePoint,raySrc,rayDir );

		ray_hit hit;
		hit.pCollider = 0;
		int flags = rwi_any_hit|rwi_stop_at_pierceable;
		int col = gEnv->pPhysicalWorld->RayWorldIntersection( raySrc,rayDir*1000.0f,ent_static,flags,&hit,1 );
		if (col > 0)
		{
			int nMatId = hit.idmatOrg;

			string sMaterial;
			if (GetMaterial())
			{
				sMaterial = GetMaterial()->GetMatInfo()->GetSafeSubMtl(nMatId)->GetName();
			}

			ISurfaceType *pSurfaceType = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceType( hit.surface_idx,m_loadedFile );
			if (pSurfaceType)
			{
				float color[4] = {1,1,1,1};
				//m_renderer->Draw2dLabel( mousePoint.x+12,mousePoint.y+8,1.2f,color,false,"%s\n%s",sMaterial.c_str(),pSurfaceType->GetName() );
			}
		}
	}
	m_renderer->Set2DMode( false,m_rcClient.right,m_rcClient.bottom );



	//-----------------------------------------------------------------------------
	//-----            Render Static Object (handled by 3DEngine)              ----
	//-----------------------------------------------------------------------------
	// calculate LOD
	IConsole* pIConsole = GetIEditor()->GetSystem()->GetIConsole();
	ICVar* pICVar = pIConsole->GetCVar("e_lod_ratio");
	if (pICVar)
		pICVar->Set( 6.0f );

	f32 fDistance = GetViewTM().GetTranslation().GetLength();
	if (mv_disableLod)
	{
		fDistance = 0;
		if (pICVar)
			pICVar->Set( 999999.0f );
	}
	SRendParams rp;
	rp.fDistance	= fDistance;

	Matrix34 tm; tm.SetIdentity();
	rp.pMatrix = &tm;
	rp.pPrevMatrix = &tm;

	Vec3 vAmbient;
	mv_objectAmbientColor.Get(vAmbient);

//	float color1[4] = {1,1,1,1};
//	g_ypos+=100;
//	m_renderer->Draw2dLabel(12,g_ypos,1.2f,color1,false,"vObjectAmbient: %f %f %f",vAmbient.x,vAmbient.y,vAmbient.z );	
//	g_ypos+=10;

	rp.AmbientColor.r=vAmbient.x*MULTIPLY;
	rp.AmbientColor.g=vAmbient.y*MULTIPLY;
	rp.AmbientColor.b=vAmbient.z*MULTIPLY;
	rp.AmbientColor.a = 1;

	rp.nDLightMask = 7;
	if (mv_lighting == true)rp.nDLightMask = 0xffff;
	else rp.nDLightMask = 0;
	rp.nDLightMask;

	rp.dwFObjFlags  = 0;
	rp.dwFObjFlags |= FOB_TRANS_MASK;
	if (m_pCurrentMaterial)
		rp.pMaterial = m_pCurrentMaterial->GetMatInfo();

	// Render particle effects.
	m_effectManager.Render(rp);

	//-----------------------------------------------------------------------------
	//-----            Render Static Object (handled by 3DEngine)              ----
	//-----------------------------------------------------------------------------
	if (m_object)
	{
		m_object->Render( rp );
		if (mv_showGrid)
			DrawGrid( Quat(IDENTITY), Vec3(ZERO), Vec3(ZERO), Matrix33(IDENTITY));
	}


	//-----------------------------------------------------------------------------
	//-----             Render Character (handled by CryAnimation)            ----
	//-----------------------------------------------------------------------------
	if (m_pCharacterBase)
	{
		DrawCharacter( m_pCharacterBase, rp );

		if (m_pCharacterBase)
			m_pCharacterBase->GetISkeletonPose()->SynchronizeWithPhysicalEntity(m_pPhysicalEntity);

		m_EntityMat = Matrix34(m_PhysEntityLocation);

		//	float color1[4] = {1,1,1,1};
		//	g_ypos+=100;
		//	m_renderer->Draw2dLabel(12,g_ypos,1.2f,color1,false,"m_PhysEntityLocation: %f %f %f",m_PhysEntityLocation.t.x,m_PhysEntityLocation.t.y,m_PhysEntityLocation.t.z );	
		//	g_ypos+=10;


		pe_params_pos par_pos;
		par_pos.pMtx3x4 = &m_EntityMat;
		m_pPhysicalEntity->SetParams(&par_pos);
	}

	m_renderer->EF_EndEf3D(SHDF_SORT/* | SHDF_ZPASS*/);
}

CAnimatedCharacterEffectManager::CAnimatedCharacterEffectManager()
:	m_pSkeleton2(0),m_pSkeletonPose(0)
{
}

CAnimatedCharacterEffectManager::~CAnimatedCharacterEffectManager()
{
	KillAllEffects();
}

void CAnimatedCharacterEffectManager::SetSkeleton(ISkeletonAnim* pSkeletonAnim,ISkeletonPose* pSkeletonPose)
{
	m_pSkeleton2 = pSkeletonAnim;
	m_pSkeletonPose = pSkeletonPose;
}

void CAnimatedCharacterEffectManager::Update()
{
	for (int i = 0; i < m_effects.size();)
	{
		EffectEntry& entry = m_effects[i];

		// If the animation has stopped, kill the effect.
		bool effectStillPlaying = (entry.pEmitter ? entry.pEmitter->IsAlive() : false);
		bool animStillPlaying = IsPlayingAnimation(entry.animID);
		if (animStillPlaying && effectStillPlaying)
		{
			// Update the effect position.
			Matrix34 tm;
			GetEffectTM(tm, entry.boneID, entry.offset, entry.dir);
			if (entry.pEmitter)
				entry.pEmitter->SetMatrix(tm);

			++i;
		}
		else
		{
#if defined(USER_michaels)
			CryLogAlways("CAnimatedCharacterEffectManager::Update(): Killing effect \"%s\" because %s.",
				(m_effects[i].pEffect ? m_effects[i].pEffect->GetName() : "<EFFECT NULL>"), (effectStillPlaying ? "animation has ended" : "effect has ended"));
#endif //defined(USER_michaels)
			if (m_effects[i].pEmitter)
				m_effects[i].pEmitter->Activate(false);
			m_effects.erase(m_effects.begin() + i);
		}
	}
}

void CAnimatedCharacterEffectManager::KillAllEffects()
{
	for (int i = 0, count = m_effects.size(); i < count; ++i)
	{
		if (m_effects[i].pEmitter)
			m_effects[i].pEmitter->Activate(false);
	}
	m_effects.clear();
}

void CAnimatedCharacterEffectManager::SpawnEffect(int animID, const char* animName, const char* effectName, const char* boneName, const Vec3& offset, const Vec3& dir)
{
	// Check whether we are already playing this effect, and if so dont restart it.
	bool alreadyPlayingEffect = IsPlayingEffect(effectName);

	if (alreadyPlayingEffect)
	{
#if defined(USER_michaels)
		CryLogAlways("CAnimatedCharacterEffectManager::SpawnEffect(): Refusing to start effect \"%s\" because effect is already running.", (effectName ? effectName : "<MISSING EFFECT NAME>"));
#endif //defined(USER_michaels)
	}
	else
	{
		IParticleEffect* pEffect = gEnv->p3DEngine->FindParticleEffect(effectName);
		if (!pEffect)
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Anim events cannot find effect \"%s\", requested by animation \"%s\".", (effectName ? effectName : "<MISSING EFFECT NAME>"), (animName ? animName : "<MISSING ANIM NAME>"));
		int boneID = (boneName && boneName[0] && m_pSkeletonPose ? m_pSkeletonPose->GetJointIDByName(boneName) : -1);
		boneID = (boneID == -1 ? 0 : boneID);
		Matrix34 tm;
		GetEffectTM(tm, boneID, offset, dir);
		IParticleEmitter* pEmitter = (pEffect ? pEffect->Spawn(false, tm) : 0);

		// Make sure the emitter is not rendered in the game.
		SpawnParams sp;
		sp.bIgnoreLocation = true;
		pEmitter->SetSpawnParams(sp);

		if (pEffect && pEmitter)
		{
#if defined(USER_michaels)
			CryLogAlways("CAnimatedCharacterEffectManager::SpawnEffect(): starting effect \"%s\", requested by animation \"%s\".", (effectName ? effectName : "<MISSING EFFECT NAME>"), (animName ? animName : "<MISSING ANIM NAME>"));
#endif defined(USER_michaels)
			m_effects.push_back(EffectEntry(pEffect, pEmitter, boneID, offset, dir, animID));
		}
	}
}

CAnimatedCharacterEffectManager::EffectEntry::EffectEntry(_smart_ptr<IParticleEffect> pEffect, _smart_ptr<IParticleEmitter> pEmitter, int boneID, const Vec3& offset, const Vec3& dir, int animID)
:	pEffect(pEffect), pEmitter(pEmitter), boneID(boneID), offset(offset), dir(dir), animID(animID)
{
}

CAnimatedCharacterEffectManager::EffectEntry::~EffectEntry()
{
}

void CAnimatedCharacterEffectManager::GetEffectTM(Matrix34& tm, int boneID, const Vec3& offset, const Vec3& dir)
{
	if (dir.len2()>0)
		tm = Matrix33::CreateRotationXYZ(Ang3(dir * 3.14159f / 180.0f));
	else
		tm.SetIdentity();
	tm.AddTranslation(offset);

	if (m_pSkeletonPose)
		tm = Matrix34(m_pSkeletonPose->GetAbsJointByID(boneID)) * tm;
}

bool CAnimatedCharacterEffectManager::IsPlayingAnimation(int animID)
{
	enum {NUM_LAYERS = 4};

	// Check whether the animation has stopped.
	bool animPlaying = false;
	for (int layer = 0; layer < NUM_LAYERS; ++layer)
	{
		for (int animIndex = 0, animCount = (m_pSkeleton2 ? m_pSkeleton2->GetNumAnimsInFIFO(layer) : 0); animIndex < animCount; ++animIndex)
		{
			CAnimation& anim = m_pSkeleton2->GetAnimFromFIFO(animIndex, animIndex);
			int32 id = (anim.m_LMG0.m_nLMGID >= 0) ? anim.m_LMG0.m_nLMGID : anim.m_LMG0.m_nAnimID[0];
			animPlaying = animPlaying || (id == animID);
		}
	}

	return animPlaying;
}

bool CAnimatedCharacterEffectManager::IsPlayingEffect(const char* effectName)
{
	bool isPlaying = false;
	for (int effectIndex = 0, effectCount = m_effects.size(); effectIndex < effectCount; ++effectIndex)
	{
		IParticleEffect* pEffect = m_effects[effectIndex].pEffect;
		if (pEffect && stricmp(pEffect->GetName(), effectName) == 0)
			isPlaying = true;
	}
	return isPlaying;
}

void CAnimatedCharacterEffectManager::Render(SRendParams& params)
{
	for (int effectIndex = 0, effectCount = m_effects.size(); effectIndex < effectCount; ++effectIndex)
	{
		IParticleEmitter* pEmitter = m_effects[effectIndex].pEmitter;
		pEmitter->Update();
		pEmitter->Render(params);
	}
}
