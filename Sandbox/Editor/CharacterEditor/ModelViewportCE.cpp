// CharacterEditor.cpp : implementation file
//

#include "stdafx.h"

#include <IRenderer.h>
#include <I3DEngine.h>
#include <ICryAnimation.h>
#include <ITimer.h>
#include <IRenderAuxGeom.h>

#include "CryEdit.h"
#include "PropertiesPanel.h"
#include "ThumbnailGenerator.h"
#include "FileTypeUtils.h"
#include "ToolbarDialog.h"

#include "CharPanel_Animation.h"
#include "CharPanel_Attachments.h"
#include "CharPanel_Morphing.h"
#include "ModelViewport.h"
#include "ModelViewportCE.h"
#include "CryCharMorphParams.h"
#include "CharPanel_AnimationControl.h"
#include "CharPanel_Character.h"
#include "CharacterEditor.h"


IMPLEMENT_DYNCREATE(CModelViewportCE,CModelViewport)

BEGIN_MESSAGE_MAP(CModelViewportCE, CModelViewport)
	ON_COMMAND(ID_ANIM_PLAY, OnAnimPlay)

	ON_WM_MOUSEMOVE()

	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()

	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDBLCLK()

END_MESSAGE_MAP()


//////////////////////////////////////////////////////////////////////////
void CModelViewportCE::SetCharacter( ICharacterInstance *pInstance )
{
	if (!pInstance)
		return;

	m_pCharacterBase = pInstance;
	m_pCharacterAnim = m_pCharacterBase;
	m_AABB = pInstance->GetAABB();

	m_camRadius = (m_AABB.max-m_AABB.min).GetLength();
	m_absCameraPosVP = Vec3( 0, -m_camRadius, (m_AABB.max.z+m_AABB.min.z)/2 );
	SetViewTM( Matrix34::CreateTranslationMat( m_absCameraPosVP ) );
	

	if (m_pCharacterPropertiesDlg)
		m_pCharacterPropertiesDlg->SetMaterial( GetMaterial() );

	Physicalize();

	SendOnCharacterChanged();
}


//ICharacterInstance* arrCharacterBase[0x1000];

//////////////////////////////////////////////////////////////////////////
void CModelViewportCE::LoadObject( const CString &fileName, float )
{
	CErrorsRecorder errorRecorder;

	m_bPaused = false;

	// Load object.
	CString file = fileName;

	bool reload = false;
	if (m_loadedFile == file)
		reload = true;
	m_loadedFile = file;

	SetName( CString("Model View - ") + file );
	ReleaseObject();

	//assert(m_pCharPanel_Animation);
	if (m_pCharPanel_Animation) 
	{
		m_pCharPanel_Animation->SetFileName( fileName );
	}

	if (IsPreviewableFileType(file))
	{
		m_pCharacterBase=0;
		m_pCharacterAnim=0;
		string fileExt = Path::GetExt(file);

		bool IsCGA = (0 == stricmp(fileExt,"cga"));
		if (IsCGA)
		{ 
			CLogFile::WriteLine("Loading CGA Model...");
			m_pCharacterBase = m_pAnimationSystem->CreateInstance( file );
			m_pCharacterAnim = m_pCharacterBase;
		}

		bool IsCDF = (0 == stricmp(fileExt,"cdf"));
		if (IsCDF) 
		{
			CLogFile::WriteLine("Importing Character Definitions...");
			m_pCharacterBase = m_pAnimationSystem->CreateInstance( file );
			m_pCharacterAnim = m_pCharacterBase;

		/*	
			uint32 numInstances = sizeof(arrCharacterBase)/4;
			for (uint32 i=0; i<numInstances; i++)
				arrCharacterBase[i] = m_pAnimationSystem->CreateInstance( file );
		*/


/*
			IAttachmentManager* pIAttachmentManager = m_pCharacterBase->GetIAttachmentManager();
			//STEP 1: create the socket for the face-attachment
			IAttachment* pIAttachment = pIAttachmentManager->CreateAttachment("C4_KaWummmmm",CA_FACE,0);
			//STEP 2: place the socket at the right position on the character
			if (pIAttachment) 
			{
				uint32 type = pIAttachment->GetType();
				if (type==CA_BONE || type==CA_FACE) 
				{
					//				<Attachment AName="back01" Type="CA_FACE" Rotation="-0.075126655,-0.99558783,0.03969615,   0.03981521" Position="0.041536111,-0.15446994,1.4762511" BoneName="" Binding="objects/characters/attachment/nanosuit/nanosuit_pouch.cgf" Flags="0" Material="objects/characters/attachment/nanosuit/Nanosuit_pockets"/>
					Quat WRot=Quat(-0.075126655f,-0.99558783f,0.03969615f,0.03981521f);
					Vec3 WPos=Vec3(0.041536111f,-0.15446994f,1.4762511f);
					pIAttachment->SetRMWRotation( WRot );
					pIAttachment->SetRMWPosition( WPos ); 
					//pIAttachment->ProjectAttachment();
				}
			}
			pIAttachmentManager->ProjectAllAttachment();
*/

		}

		bool IsCHR = (0 == stricmp(fileExt,"chr"));
		if (IsCHR)
		{ 
			CLogFile::WriteLine("Loading Character Model...");
			m_pCharacterBase = m_pAnimationSystem->CreateInstance( file );
			m_pCharacterAnim = m_pCharacterBase;

/*
			IAttachmentManager* pIAttachmentManager = m_pCharacterBase->GetIAttachmentManager();
			//STEP 1: create the socket for the face-attachment
			IAttachment* pIAttachment = pIAttachmentManager->CreateAttachment("C4_KaWummmmm",CA_FACE,0);
			//STEP 2: place the socket at the right position on the character
			if (pIAttachment) 
			{
				uint32 type = pIAttachment->GetType();
				if (type==CA_BONE || type==CA_FACE) 
				{
					//				<Attachment AName="back01" Type="CA_FACE" Rotation="-0.075126655,-0.99558783,0.03969615,   0.03981521" Position="0.041536111,-0.15446994,1.4762511" BoneName="" Binding="objects/characters/attachment/nanosuit/nanosuit_pouch.cgf" Flags="0" Material="objects/characters/attachment/nanosuit/Nanosuit_pockets"/>
					Quat WRot=Quat(-0.075126655f,-0.99558783f,0.03969615f,0.03981521f);
					Vec3 WPos=Vec3(0.041536111f,-0.15446994f,1.4762511f);
					pIAttachment->SetRMWRotation( WRot );
					pIAttachment->SetRMWPosition( WPos ); 
			//		pIAttachment->ProjectAttachment();
				}
			}
			pIAttachmentManager->ProjectAllAttachment();
*/

		}

		if (m_pCharacterBase)
		{
			//m_pCharacterBase->AddRef();

			CThumbnailGenerator thumbGen;
			thumbGen.GenerateForFile( file );

			if (m_pCharPanel_Animation && m_pCharacterBase != NULL && m_pCharacterAnim != NULL)
			{	
				UpdateAnimationList();
				UpdateBoneList();
			//SetCharacterUIInfo();		
			}

			f32	 radius	= m_pCharacterBase->GetAABB().GetRadius();		//m_pCharacterBase->GetRadius();
			Vec3 center	= m_pCharacterBase->GetAABB().GetCenter();		//m_pCharacterBase->GetCenter();

			m_AABB.min = center-Vec3(radius,radius,radius);
			m_AABB.max = center+Vec3(radius,radius,radius);
			if (!reload)
				m_camRadius = center.z + radius;

			m_Button_MOVE=0;
			m_Button_ROTATE=0;

			if (m_pMorphingDlg)
			{
				m_pMorphingDlg->m_DeformationSlider.SetValue(0);
				m_pMorphingDlg->m_DeformationNumber.SetValue(0);
				m_pMorphingDlg->m_DeformationSlider.EnableWindow(FALSE);
				m_pMorphingDlg->m_DeformationNumber.EnableWindow(FALSE);

				m_pMorphingDlg->m_button[0].EnableWindow(TRUE);
				m_pMorphingDlg->m_button[1].EnableWindow(TRUE);
				m_pMorphingDlg->m_button[2].EnableWindow(TRUE);
				m_pMorphingDlg->m_button[3].EnableWindow(TRUE);
				m_pMorphingDlg->m_button[4].EnableWindow(TRUE);
				m_pMorphingDlg->m_button[5].EnableWindow(TRUE);
				m_pMorphingDlg->m_button[6].EnableWindow(TRUE);
				m_pMorphingDlg->m_button[7].EnableWindow(TRUE);

				m_pMorphingDlg->m_button[0].SetCheck(FALSE);
				m_pMorphingDlg->m_button[1].SetCheck(FALSE);
				m_pMorphingDlg->m_button[2].SetCheck(FALSE);
				m_pMorphingDlg->m_button[3].SetCheck(FALSE);
				m_pMorphingDlg->m_button[5].SetCheck(FALSE);
				m_pMorphingDlg->m_button[6].SetCheck(FALSE);
				m_pMorphingDlg->m_button[7].SetCheck(FALSE);

				m_pMorphingDlg->m_button[0].SetBkColor(RGB(0x3f,0x3f,0x3f));
				m_pMorphingDlg->m_button[0].SetPushedBkColor(RGB(0x000,0x00,0x00));
				m_pMorphingDlg->m_button[1].SetBkColor(RGB(0x7f,0x3f,0x3f));
				m_pMorphingDlg->m_button[1].SetPushedBkColor(RGB(0x0ff,0x00,0x00));
				m_pMorphingDlg->m_button[2].SetBkColor(RGB(0x3f,0x7f,0x3f));
				m_pMorphingDlg->m_button[2].SetPushedBkColor(RGB(0x00,0xff,0x00));
				m_pMorphingDlg->m_button[3].SetBkColor(RGB(0x7f,0x7f,0x3f));
				m_pMorphingDlg->m_button[3].SetPushedBkColor(RGB(0xff,0xff,0x00));
				m_pMorphingDlg->m_button[4].SetBkColor(RGB(0x3f,0x3f,0x7f));
				m_pMorphingDlg->m_button[4].SetPushedBkColor(RGB(0x000,0x00,0xff));
				m_pMorphingDlg->m_button[5].SetBkColor(RGB(0x7f,0x3f,0x7f));
				m_pMorphingDlg->m_button[5].SetPushedBkColor(RGB(0xff,0x00,0xff));
				m_pMorphingDlg->m_button[6].SetBkColor(RGB(0x3f,0x7f,0x7f));
				m_pMorphingDlg->m_button[6].SetPushedBkColor(RGB(0x00,0xff,0xff));
				m_pMorphingDlg->m_button[7].SetBkColor(RGB(0x7f,0x7f,0x7f));
				m_pMorphingDlg->m_button[7].SetPushedBkColor(RGB(0xff,0xff,0xff));
				m_pMorphingDlg->m_nColor=9;
			}

			if (m_pAttachmentsDlg)
			{
				m_pAttachmentsDlg->CharacterChanged=0;
				m_pAttachmentsDlg->ReloadAttachment();
	//			m_pAttachmentsDlg->ClearBones();

				//initialize selection
				m_SelectedAttachment=0;
				IAttachmentManager* pAttachmentManager = m_pCharacterBase->GetIAttachmentManager();
				uint32 numAttachment = pAttachmentManager->GetAttachmentCount();
				if (numAttachment) 
				{
					IAttachment* pIAttachment = pAttachmentManager->GetInterfaceByIndex(m_SelectedAttachment);  
					uint32 type = pIAttachment->GetType();
					if (type==CA_BONE || type==CA_FACE) 
					{
						m_ArcBall.DragRotation.SetIdentity();
						m_ArcBall.ObjectRotation	=	pIAttachment->GetAttAbsoluteDefault().q;
						m_ArcBall.sphere.center		= pIAttachment->GetAttAbsoluteDefault().t; 
					}

					m_pAttachmentsDlg->UpdateList();

					string name = pIAttachment->GetName();
					uint32 n = m_pAttachmentsDlg->m_attachmentsList.FindString(-1,name);
					m_pAttachmentsDlg->m_attachmentsList.SetCurSel(n);
					m_pAttachmentsDlg->OnAttachmentSelect();
					m_pAttachmentsDlg->CharacterChanged=1;
				}
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

//	m_objectAngles(0,0,0);
	m_camRadius = (m_AABB.max-m_AABB.min).GetLength();

	Matrix34 VMat;
	VMat.SetRotationZ(gf_PI);
	VMat.SetTranslation( Vec3(0, m_camRadius,(m_AABB.max.z+m_AABB.min.z)/2) );
	SetViewTM( VMat );

	if (m_pCharacterPropertiesDlg)
		m_pCharacterPropertiesDlg->SetMaterial(GetMaterial());

	Physicalize();

	if (m_pCharPanel_Animation)
		m_pCharPanel_Animation->SetBaseCharacter();

	SendOnCharacterChanged();
}






//////////////////////////////////////////////////////////////////////////
void CModelViewportCE::UpdateAnimationList()
{
	// Fill the combo box with the name of the animation sequences
	m_pCharPanel_Animation->ClearAnims();
	m_pCharPanel_Animation->DisableFileBroser();

	if ( !m_pCharacterAnim )
		return;

	//Add all animations to the animation list box/view
	IAnimationSet* pAnimations = m_pCharacterAnim->GetIAnimationSet();
	if (!pAnimations)
		return;

	// names of animations
	uint32 numAnims = pAnimations->numAnimations();
	uint32 numMorphs = pAnimations->numMorphTargets();
	std::vector< const char* > animNames;
	animNames.reserve( numAnims+numMorphs );
	for (uint32 i=0; i<(numAnims+numMorphs); ++i)
	{
		// the length of the animation is passed in seconds
		const char* pName = pAnimations->GetNameByAnimID(i);
		f32 length=0;
		if (pName[0]!='#')
		{
			uint32 flags	= pAnimations->GetAnimationFlags(i);
			uint32 cycle	=	flags&CA_ASSET_CYCLE;
			uint32 loaded	=	flags&CA_ASSET_LOADED;
			uint32 lmg	  =	flags&CA_ASSET_LMG;
			length = pAnimations->GetDuration_sec(i);
			if (lmg==0 && loaded)
				assert(length>=0);
		}
		animNames.push_back( pName );
	}

	m_pCharPanel_Animation->m_tree_animations.LockWindowUpdate();
	for ( uint32 i = 0; i < animNames.size(); ++i )
		m_pCharPanel_Animation->AddAnimName( animNames[i], i );
	m_pCharPanel_Animation->m_tree_animations.UnlockWindowUpdate();
}


void CModelViewportCE::UpdateBoneList()
{
	if ( !m_pAttachmentsDlg )
		return;

	//------------------------------------------------------------------------------
	// Fill the bone list in the TabPanel
	//------------------------------------------------------------------------------
	//ICryCharModel *pCharModel = m_pCharacterBase->GetModel();
	uint32 numBones = m_pCharacterBase->GetISkeletonPose()->GetJointCount();
	m_pAttachmentsDlg->ClearBones();
	for (uint32 i=0; i < numBones; i++)
	{
		const char *str = m_pCharacterBase->GetISkeletonPose()->GetJointNameByID(i);
		if (str == NULL)
			break;
		if (strlen(str) > 0)
			m_pAttachmentsDlg->AddBone(str);
	}
	m_pAttachmentsDlg->SelectBone( m_attachBone );

}




//-----------------------------------------------------
void CModelViewportCE::OnAnimPlay()
{
	m_Button_MOVE		= 0;
	m_Button_ROTATE	= 0;
	m_pCharacterBase->SetResetMode(0);
	CModelViewport::OnAnimPlay();
}


//-------------------------------------------------------------------
void CModelViewportCE::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
}
//-------------------------------------------------------------------
void CModelViewportCE::OnRButtonDown( UINT nFlags, CPoint point)
{
	m_MouseButtonR = true;
	CRenderViewport::OnRButtonDown(nFlags,point);
}
//-------------------------------------------------------------------
void CModelViewportCE::OnRButtonUp( UINT nFlags, CPoint point)
{
	m_MouseButtonR = false;
	CRenderViewport::OnRButtonUp(nFlags,point);
}

//-------------------------------------------------------------------

void CModelViewportCE::OnLButtonDown( UINT nFlags, CPoint point)
{
	ICharacterInstance* pCharacter=GetCharacterBase();
	if (pCharacter)
	{
		if (pCharacter->GetResetMode()) 
		{
			IAttachmentManager* pIAttachmentManager = pCharacter->GetIAttachmentManager();

			m_opMode = SelectMode;

			m_MouseButtonL = true;
			m_pAttachmentsDlg->CharacterChanged=1;

			m_cMouseDownPos = point;

			if (m_Button_MOVE)
			{
				Matrix34 m34=Matrix34(Matrix33(m_ArcBall.ObjectRotation),m_ArcBall.sphere.center );
				m_HitContext.view = this;
				m_HitContext.b2DViewport = false;
				m_HitContext.point2d = point;
				ViewToWorldRay( point, m_HitContext.raySrc,m_HitContext.rayDir );
				m_HitContext.distanceTollerance = 0;
				if (m_AxisHelper.HitTest(m34,m_HitContext))
				{
					SetAxisConstrain( m_HitContext.axis );
					GetIEditor()->SetAxisConstrains( (AxisConstrains)m_HitContext.axis );
					if (m_Button_MOVE)
						m_opMode = MoveMode;
					SetConstructionMatrix( COORDS_LOCAL,m34 );
					return;
				}
			}


			if (m_MouseOnAttachment >= 0)
			{
				uint32 numAttachment = pIAttachmentManager->GetAttachmentCount();
				assert(m_MouseOnAttachment<numAttachment);

				m_SelectedAttachment=m_MouseOnAttachment;

				if (numAttachment) 
				{
					IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByIndex(m_SelectedAttachment);  
					uint32  type = pIAttachment->GetType(); 
					if (type==CA_BONE || type==CA_FACE ) 
					{
						m_ArcBall.DragRotation.SetIdentity();
						m_ArcBall.ObjectRotation	=	pIAttachment->GetAttAbsoluteDefault().q;
						m_ArcBall.sphere.center		= pIAttachment->GetAttAbsoluteDefault().t; 
					}
					string name = pIAttachment->GetName();
					uint32 n = m_pAttachmentsDlg->m_attachmentsList.FindString(-1,name);
					m_pAttachmentsDlg->m_attachmentsList.SetCurSel(n);
					m_pAttachmentsDlg->OnAttachmentSelect();
					m_pAttachmentsDlg->CharacterChanged=1;
				}
			}

			Matrix34 m34=Matrix34(Matrix33(m_ArcBall.ObjectRotation),m_ArcBall.sphere.center );
			SetConstructionMatrix( COORDS_LOCAL,m34 );
		}
	}

}

void CModelViewportCE::OnLButtonUp( UINT nFlags, CPoint point) {
	m_MouseButtonL = false;
	m_opMode = SelectMode;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

f32 CModelViewportCE::CharacterPicking( const Ray& mray ) 
{
	if (!m_pFaces)
		return 0;
	
	//IRenderer* renderer = GetIEditor()->GetRenderer();
	//IRenderAuxGeom* pAuxGeom = renderer->GetIRenderAuxGeom();
	//SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
	//renderFlags.SetFillMode( e_FillModeWireframe );
	//renderFlags.SetDrawInFrontMode( e_DrawInFrontOn );
	//pAuxGeom->SetRenderFlags( renderFlags );

	f32 distance=3.3E38f;

	/*
	uint32 numFaces = m_numFaces;
	for(uint32 i=0; i<numFaces; i++) 
	{
		uint32 i0=(m_pFaces)[i].i0;
		uint32 i1=(m_pFaces)[i].i1;
		uint32 i2=(m_pFaces)[i].i2;
		Vec3 output;

		Vec3 v0=(m_pSkinVertices)[i1].wpos1;
		Vec3 v1=(m_pSkinVertices)[i0].wpos1;
		Vec3 v2=(m_pSkinVertices)[i2].wpos1;
		bool t = Intersect::Ray_Triangle( mray, v0,v1,v2, output );
		if (t) 
		{
			//pAuxGeom->DrawLine( v0,RGBA8(0xff,0x00,0x00,0x00), v1,RGBA8(0x00,0xff,0x00,0x00) );
			//pAuxGeom->DrawLine( v1,RGBA8(0x00,0xff,0x00,0x00), v2,RGBA8(0x00,0x00,0xff,0x00) );
			//pAuxGeom->DrawLine( v2,RGBA8(0x00,0x00,0xff,0x00), v0,RGBA8(0xff,0x00,0x00,0x00) );
			f32 d = (output-m_Camera.GetPosition()).GetLength();
			if (distance>d) distance=d;
		}
	}
	*/

	ICharacterInstance* pICharacterInstance = m_pCharacterBase;

	if (!pICharacterInstance)
		return distance;


	IRenderMesh* pRenderMesh = pICharacterInstance->GetICharacterModel()->GetRenderMesh(-1);//pIStaticObject->GetRenderMesh();
	if (pRenderMesh==0)
		return 0;

	uint32 NumVertices = pRenderMesh->GetVertCount();

	int pIndicesCount=0;
	uint16* pIndices = pRenderMesh->GetIndices(&pIndicesCount);
	int PosStride=0;
	byte* pVertices_strided = pRenderMesh->GetStridedPosPtr(PosStride);

	static Vec3 LineVertices[0x5000];
	for(uint32 i=0; i<NumVertices; i++) 
	{
		LineVertices[i] = *(Vec3*)pVertices_strided;
		LineVertices[i] = /*m34**/LineVertices[i];
		pVertices_strided += PosStride;
	}

	for(uint32 i=0; i<pIndicesCount; i=i+3) 
	{
		uint32 i0=pIndices[i+0];
		uint32 i1=pIndices[i+1];
		uint32 i2=pIndices[i+2];
		Vec3 output;
		bool t = Intersect::Ray_Triangle( mray, LineVertices[i1],LineVertices[i0],LineVertices[i2], output );
		if (t) 
		{	
			f32 d = (output-m_Camera.GetPosition()).GetLength();	
			if (distance>d) 
				distance=d;	
		}
	}

	return distance;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

f32 CModelViewportCE::AttachmentPicking( const Ray& mray, IAttachment* pIAttachment, const Matrix34& m34 ) 
{
	f32 distance = 3.3E38f;
	Vec3 output(ZERO);
	IAttachmentObject* pBindable = pIAttachment->GetIAttachmentObject();
	if (pBindable) 
	{

		AABB caabb=pBindable->GetAABB();
		OBB obb = OBB::CreateOBBfromAABB( Matrix33(m34), caabb );

		uint32 intersection = Intersect::Ray_OBB(mray,m34.GetTranslation(),obb,output);

		if (intersection) 
		{
			IAttachmentObject* pIAttachmentObject = pIAttachment->GetIAttachmentObject();

			IStatObj* pIStaticObject = pIAttachmentObject->GetIStatObj();
			if (pIStaticObject) 
			{
				uint32 count = pIStaticObject->GetSubObjectCount();
				if (count==0)
				{
					IRenderMesh* pRenderMesh = pIStaticObject->GetRenderMesh();
					uint32 NumVertices = pRenderMesh->GetVertCount();

					int pIndicesCount=0;
					uint16* pIndices = pRenderMesh->GetIndices(&pIndicesCount);
					int PosStride=0;
					byte* pVertices_strided = pRenderMesh->GetStridedPosPtr(PosStride);

					static Vec3 LineVertices[0x5000];
					for(uint32 i=0; i<NumVertices; i++) 
					{
						LineVertices[i] = *(Vec3*)pVertices_strided;
						LineVertices[i] = m34*LineVertices[i];
						pVertices_strided += PosStride;
					}

					for(uint32 i=0; i<pIndicesCount; i=i+3) 
					{
						uint32 i0=pIndices[i+0];
						uint32 i1=pIndices[i+1];
						uint32 i2=pIndices[i+2];
						Vec3 output;
						bool t = Intersect::Ray_Triangle( mray, LineVertices[i1],LineVertices[i0],LineVertices[i2], output );
						if (t) {	f32 d = (output-m_Camera.GetPosition()).GetLength();	if (distance>d) distance=d;	}
					}
				}
			}
			else 
			{
				//maybe object is an attached character 
				/*
			
				if (pICharacterInstance) 
				{
					ExtSkinVertex* pSkinVertices=0;
					uint32 numVertices=0;
					TFace* pFaces=0;
					uint32 numFaces=0;

					uint32 res = 0;//pICharacterInstance->GetMesh( pSkinVertices,numVertices, pFaces,numFaces);
					if (res)
					{
						//nt32 numFaces = numFaces;
						for(uint32 i=0; i<numFaces; i++) 
						{
							uint32 i0=pFaces[i].i0;
							uint32 i1=pFaces[i].i1;
							uint32 i2=pFaces[i].i2;
							Vec3 v0=m34*pSkinVertices[i1].wpos1;
							Vec3 v1=m34*pSkinVertices[i0].wpos1;
							Vec3 v2=m34*pSkinVertices[i2].wpos1;
							Vec3 output;
							bool t = Intersect::Ray_Triangle( mray, v0,v1,v2, output );
							if (t) 
							{
								f32 d = (output-m_Camera.GetPosition()).GetLength();	
								if (distance>d) distance=d;	
							}
						}
					}
					*/
				ICharacterInstance* pICharacterInstance = pIAttachmentObject->GetICharacterInstance();

				if (!pICharacterInstance)
					return distance;


				IRenderMesh* pRenderMesh = pICharacterInstance->GetICharacterModel()->GetRenderMesh(-1);//pIStaticObject->GetRenderMesh();
				uint32 NumVertices = pRenderMesh->GetVertCount();

				int pIndicesCount=0;
				uint16* pIndices = pRenderMesh->GetIndices(&pIndicesCount);
				int PosStride=0;
				byte* pVertices_strided = pRenderMesh->GetStridedPosPtr(PosStride);

				static Vec3 LineVertices[0x5000];
				for(uint32 i=0; i<NumVertices; i++) 
				{
					LineVertices[i] = *(Vec3*)pVertices_strided;
					LineVertices[i] = m34*LineVertices[i];
					pVertices_strided += PosStride;
				}

				for(uint32 i=0; i<pIndicesCount; i=i+3) 
				{
					uint32 i0=pIndices[i+0];
					uint32 i1=pIndices[i+1];
					uint32 i2=pIndices[i+2];
					Vec3 output;
					bool t = Intersect::Ray_Triangle( mray, LineVertices[i1],LineVertices[i0],LineVertices[i2], output );
					if (t) 
					{	
						f32 d = (output-m_Camera.GetPosition()).GetLength();	
						if (distance>d) 
							distance=d;	
					}
				}
			}
		}	 //if mouse on obb
	} 
	else 
	{
		AABB caabb = AABB(Vec3( -0.05f, -0.05f, -0.05f),Vec3( +0.05f, +0.05f, +0.05f));
		OBB obb = OBB::CreateOBBfromAABB( Matrix33(Quat(1,0,0,0)), caabb );
		uint32 intersection = Intersect::Ray_OBB(mray,m34.GetTranslation(),obb,output);
		if (intersection) {	distance = (output-m_Camera.GetPosition()).GetLength();	}
	}

	return distance;
}



//-------------------------------------------------------------------

void CModelViewportCE::OnMouseMove(UINT nFlags, CPoint point)
{
	CRenderViewport::OnMouseMove(nFlags,point);

	//calculate the Ray for camera-pos to mouse-cursor
	GetWindowRect(m_WinRect);
	f32 RresX=(f32)(m_WinRect.right-m_WinRect.left); 
	f32 RresY=(f32)(m_WinRect.bottom-m_WinRect.top);
	f32 MiddleX=RresX/2.0f;
	f32 MiddleY=RresY/2.0f;
	Vec3 MoursePos3D = Vec3( point.x-MiddleX, m_Camera.GetEdgeP().y, -point.y+MiddleY ) * Matrix33(m_Camera.GetViewMatrix()) + m_Camera.GetPosition();
	Ray mray(m_Camera.GetPosition(),(MoursePos3D-m_Camera.GetPosition()).GetNormalized() );

	if (m_MouseButtonR==false && m_pCharacterBase  != NULL && m_SelectionUpdate && m_pCharacterBase->GetResetMode()) 
	{
		m_SelectionUpdate=0;
		ClosestPoint ct;

		//-----------------------------------------------------------------------
		//---            find closest point on character-mesh                 ---
		//-----------------------------------------------------------------------
		IAttachmentManager* pIAttachmentManager = m_pCharacterBase->GetIAttachmentManager();
		uint32 numAttachment = pIAttachmentManager->GetAttachmentCount();

		uint32 res = 0;//m_pCharacterBase->GetMesh( m_pSkinVertices,m_numSkinVertices, m_pFaces,m_numFaces);

		ct.basemodel=0;
		//if (res)
			ct.distance = CharacterPicking( mray ); 
		if (ct.distance<10000.0f)
			ct.basemodel=1;
		//-----------------------------------------------------------------------
		//---            find closest point on arcball                        ---
		//-----------------------------------------------------------------------
		if (m_Button_ROTATE && numAttachment>0) 
		{
			Vec3 output;
			uint32 i=Intersect::Ray_SphereFirst(mray,m_ArcBall.sphere,output);
			if (i) 
			{ 
				f32 d=(output-m_Camera.GetPosition()).GetLength();
				if (ct.distance>d) 
				{
					ct.basemodel=0;
					ct.distance=d;
				}
				//ct.distance=0;
				for (uint32 s=0; s<numAttachment; s++)  
				{
					IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByIndex(s);
					uint32 type = pIAttachment->GetType();
					if (type==CA_BONE || type==CA_FACE) 
						if (pIAttachment->GetAttAbsoluteDefault().t==m_ArcBall.sphere.center)	ct.selection =s;
				} //for loop
			}
		}



		//----------------------------------------------------------------------
		//----------------------------------------------------------------------
		//----------------------------------------------------------------------

		SetCurrentCursor( STD_CURSOR_DEFAULT,"" );

		if (numAttachment) 
		{
			if (m_Button_ROTATE) 
			{
				m_ArcBall.ArcControl( mray, m_MouseButtonL );
			}

			if (m_Button_MOVE) 
			{
				Matrix34 m34=Matrix34(Matrix33(m_ArcBall.ObjectRotation),m_ArcBall.sphere.center );
				m_HitContext.view = this;
				m_HitContext.b2DViewport = false;
				m_HitContext.point2d = point;
				m_HitContext.rayDir = mray.direction;
				m_HitContext.raySrc = mray.origin;
				ViewToWorldRay( point, m_HitContext.raySrc,m_HitContext.rayDir );
				m_HitContext.distanceTollerance = 0;
				if (m_AxisHelper.HitTest(m34,m_HitContext))
				{
					SetCurrentCursor( STD_CURSOR_MOVE,"Attachment" );
					ct.distance=0;						
					for (uint32 s=0; s<numAttachment; s++)  
					{
						IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByIndex(s);
						uint32 type = pIAttachment->GetType();
						if (type==CA_BONE || type==CA_FACE) 
							if (pIAttachment->GetAttAbsoluteDefault().t==m_ArcBall.sphere.center)	ct.selection =s;
					} //for loop
				}
			}
		}



		//-----------------------------------------------------------------------
		//---       loop over all attachments and do raycasting               ---
		//-----------------------------------------------------------------------
		m_MouseOnAttachment=-1;
		for (uint32 s=0; s<numAttachment; s++)  
		{
			IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByIndex(s);
			uint32 type = pIAttachment->GetType();
			//if (type==CA_BONE || type==CA_FACE) 
			{
				Matrix34 m34=Matrix34( pIAttachment->GetAttAbsoluteDefault() );
				if (type==CA_SKIN) 
					m34.SetIdentity(); 
				f32 distance = AttachmentPicking(mray, pIAttachment,m34); 
				if (ct.distance>distance) 
				{
					ct.basemodel	=	0;
					ct.distance		= distance;
					ct.selection	=s;
				}
			} 

		} //for loop
		cp=ct;
		m_MouseOnAttachment=cp.selection;

		if (m_opMode == MoveMode)
		{
			Vec3 p1 = MapViewToCP(m_cMouseDownPos);
			Vec3 p2 = MapViewToCP(point);
			if (p1.IsZero() || p2.IsZero())
				return;
			Vec3 v = GetCPVector(p1,p2);

			m_cMouseDownPos = point;
			m_ArcBall.sphere.center += v;
		}



	}
}













void CModelViewportCE::OnRender()
{
	IRenderer* renderer = GetIEditor()->GetRenderer();
	IRenderAuxGeom* pAuxGeom = renderer->GetIRenderAuxGeom();

	if (m_AxisHelper.GetHighlightAxis() == 0)
		m_AxisHelper.SetHighlightAxis( GetAxisConstrain() );

	if (m_pCharacterBase) 
	{	

		if (mv_showJointsValues)
		{
			CString bonename;
			if (m_pAttachmentsDlg)
				bonename = m_pAttachmentsDlg->GetBonenameFromWindow();
			int32 idx = m_pCharacterBase->GetISkeletonPose()->GetJointIDByName (bonename);

			if (idx>=0)
			{		
				uint32 ypos = 300;
				float color1[4] = {1,1,1,1};
				renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"bonename: %s %d",bonename,idx);
				ypos+=10;

				Matrix34 abs34=Matrix34( m_pCharacterBase->GetISkeletonPose()->GetAbsJointByID(idx) );
				Matrix34 rel34=Matrix34( m_pCharacterBase->GetISkeletonPose()->GetRelJointByID(idx) );

				Vec3 WPos			=	abs34.GetTranslation();
				Vec3 AxisX		=	abs34.GetColumn0();
				Vec3 AxisY		=	abs34.GetColumn1();
				Vec3 AxisZ		=	abs34.GetColumn2();

				SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
				pAuxGeom->SetRenderFlags( renderFlags );
				pAuxGeom->DrawLine( WPos,RGBA8(0xff,0x00,0x00,0x00), WPos+AxisX*29.0f,RGBA8(0x00,0x00,0x00,0x00) );
				pAuxGeom->DrawLine( WPos,RGBA8(0x00,0xff,0x00,0x00), WPos+AxisY*29.0f,RGBA8(0x00,0x00,0x00,0x00) );
				pAuxGeom->DrawLine( WPos,RGBA8(0x00,0x00,0xff,0x00), WPos+AxisZ*29.0f,RGBA8(0x00,0x00,0x00,0x00) );

				QuatT absQuat=QuatT(abs34);
				QuatT relQuat=QuatT(rel34);
				renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"absQuat: %15.10f %15.10f %15.10f %15.10f  --- %15.10f %15.10f %15.10f",absQuat.q.w,absQuat.q.v.x,absQuat.q.v.y,absQuat.q.v.z,  absQuat.t.x,absQuat.t.y,absQuat.t.z);
				ypos+=10;
				renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"relQuat: %15.10f %15.10f %15.10f %15.10f  --- %15.10f %15.10f %15.10f",relQuat.q.w,relQuat.q.v.x,relQuat.q.v.y,relQuat.q.v.z,  relQuat.t.x,relQuat.t.y,relQuat.t.z );
				ypos+=10;
			}
		}

		if (mv_showJointNames)
		{
			ISkeletonPose* pSkeletonPose = (m_pCharacterBase ? m_pCharacterBase->GetISkeletonPose() : 0);
			for (int jointIndex = 0, jointCount = (pSkeletonPose ? pSkeletonPose->GetJointCount() : 0); jointIndex < jointCount; ++jointIndex)
			{
				const char* jointName = (pSkeletonPose ? pSkeletonPose->GetJointNameByID(jointIndex) : 0);
				QuatT jointTM = (pSkeletonPose ? pSkeletonPose->GetAbsJointByID(jointIndex) : QuatT(IDENTITY));
				if (renderer)
					renderer->DrawLabel(jointTM.t, 1, "%s", (jointName ? jointName : "<UNKOWN JOINT>"));
			}
		}

		//-------------------------------------------------------------------------------------
		//-------------------------------------------------------------------------------------
		//-------------------------------------------------------------------------------------
		if (m_pCharPanel_Animation)
		{
			if (m_Button_ROTATE || m_Button_MOVE) 
			{
				m_pCharPanel_Animation->m_PlayControl.SetCheck(0);
				m_pCharPanel_Animation->m_PlayControl.EnableWindow(FALSE);
				m_pCharPanel_Animation->m_FixedCamera.SetCheck(0);
				m_pCharPanel_Animation->m_FixedCamera.EnableWindow(FALSE);

				m_pCharPanel_Animation->m_PathFollowing.SetCheck(0);
				m_pCharPanel_Animation->m_PathFollowing.EnableWindow(FALSE);
				m_pCharPanel_Animation->m_AttachedCamera.SetCheck(0);
				m_pCharPanel_Animation->m_AttachedCamera.EnableWindow(FALSE);

				m_pCharPanel_Animation->m_Idle2Move.SetCheck(0);
				m_pCharPanel_Animation->m_Idle2Move.EnableWindow(FALSE);

				m_pCharPanel_Animation->m_IdleStep.SetCheck(0);
				m_pCharPanel_Animation->m_IdleStep.EnableWindow(FALSE);
			}
			else
			{
				m_pCharPanel_Animation->m_PlayControl.EnableWindow(TRUE);
				m_pCharPanel_Animation->m_FixedCamera.EnableWindow(TRUE);
				m_pCharPanel_Animation->m_PathFollowing.EnableWindow(TRUE);
				m_pCharPanel_Animation->m_AttachedCamera.EnableWindow(TRUE);

				m_pCharPanel_Animation->m_Idle2Move.EnableWindow(TRUE);
				m_pCharPanel_Animation->m_IdleStep.EnableWindow(TRUE);
			}
		}


/*		static f32 m_fLastAnimUpdateTime=0.0f;
		f32 m_fFrameTime=0;
		f32 fAnimUpdateTime = GetIEditor()->GetSystem()->GetITimer()->GetCurrTime();

		if (m_fLastAnimUpdateTime > 0)
			m_fFrameTime = (fAnimUpdateTime - m_fLastAnimUpdateTime)*32;

		if (m_fFrameTime >= 0) 
		{
			m_fLastAnimUpdateTime = fAnimUpdateTime;
			if (m_fFrameTime>1.0f) 
				m_fFrameTime=1.0f;

			extern f32 OldVal;
			extern f32 NewVal;
			f32 dif = (NewVal-OldVal);
			f32 d=dif*dif;
			if (dif<0)
				d*=-1;

			OldVal += d*m_fFrameTime;
			if (OldVal>1)
				OldVal=1;

			CryCharMorphParams Params;
			Params.m_fBlendIn		=	1;
			Params.m_fLength		=	9.000000f;//(fBlendInTime+fBlendOutTime)/2;
			Params.m_fBlendOut	=	1;
			Params.m_fAmplitude = m_pCharPanel_Animation->GetMTAmplitude();
			Params.m_fStartTime = OldVal*1.5f;
			if (Params.m_fStartTime<0) 
				Params.m_fStartTime=0;
			if (Params.m_fStartTime>1) 
				Params.m_fStartTime=1;
			Params.m_fSpeed = 1;
			m_pCharacterAnim->StopMorph ("#test_1",1);
			m_pCharacterAnim->StartMorph("#test_1", Params);

			Params.m_fStartTime = (OldVal-0.5f)*1.7f;
			if (Params.m_fStartTime<0) 
				Params.m_fStartTime=0;
			if (Params.m_fStartTime>1) 
				Params.m_fStartTime=1;
		//	m_pCharacterAnim->StopMorph ("#Full_Hurt",1);
		//	m_pCharacterAnim->StartMorph("#Full_Hurt", Params);
		//	m_pCharacterAnim->StopMorph ("#Asian_Head_Not_Happy",1);
		//	m_pCharacterAnim->StartMorph("#Asian_Head_Not_Happy", Params);
			Params.m_fAmplitude = 0.40f; //m_pCharPanel_Animation->GetMTAmplitude();
			m_pCharacterAnim->StopMorph ("#test_2",1);
			m_pCharacterAnim->StartMorph("#test_2", Params);


			Params.m_fStartTime = (1-OldVal*2);
			if (Params.m_fStartTime<0) 
				Params.m_fStartTime=0;
			if (Params.m_fStartTime>1) 
				Params.m_fStartTime=1;
			Params.m_fAmplitude = 0.750f; //m_pCharPanel_Animation->GetMTAmplitude();
			m_pCharacterAnim->StopMorph ("#Asian_Head_Not_Happy",1);
			m_pCharacterAnim->StartMorph("#Asian_Head_Not_Happy", Params);

		}*/


		//m_pCharacterBase->SetScale(Vec3(-1,2,1));
		if (m_pCharacterBase->GetResetMode()) 
		{
			//-----------------------------------------------------------------------
			//---                    resolve selection                            ---
			//-----------------------------------------------------------------------
			IAttachmentManager* pIAttachmentManager = m_pCharacterBase->GetIAttachmentManager();
			uint32 numAttachment=pIAttachmentManager->GetAttachmentCount();
			if (numAttachment==0) 
			{
				m_SelectedAttachment=-1;
			}
			if (numAttachment==1) 
			{
				m_SelectedAttachment=0;
			}

			//-----------------------------------------------------------------------
			// draw wireframe over closest selection
			//-----------------------------------------------------------------------
			if (cp.selection != -1) 
			{
				IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByIndex(cp.selection);
				IAttachmentObject* pIAttachmentObject = pIAttachment->GetIAttachmentObject();

				if (pIAttachmentObject) 
				{
					IStatObj* pIStaticObject = pIAttachmentObject->GetIStatObj();
					if (pIStaticObject) 
					{
						uint32 count = pIStaticObject->GetSubObjectCount();
						if (count==0)
						{
							IRenderMesh* pRenderMesh = pIStaticObject->GetRenderMesh();
							uint32 NumVertices = pRenderMesh->GetVertCount();

							int numIndices=0;
							uint16* pIndices = pRenderMesh->GetIndices(&numIndices);
							int PosStride=0;
							byte* pVertices_strided = pRenderMesh->GetStridedPosPtr(PosStride);

							Matrix34 m34 = Matrix34( pIAttachment->GetAttAbsoluteDefault() );
							static Vec3 LineVertices[0x5000];
							for(uint32 i=0; i<NumVertices; i++) 
							{
								LineVertices[i]		= *(Vec3*)pVertices_strided;
								LineVertices[i]		= m34*LineVertices[i];
								pVertices_strided += PosStride;
							}
							SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
							renderFlags.SetFillMode( e_FillModeWireframe );
							renderFlags.SetDrawInFrontMode( e_DrawInFrontOn );
							renderFlags.SetAlphaBlendMode(e_AlphaAdditive);
							pAuxGeom->SetRenderFlags( renderFlags );
							pAuxGeom->DrawTriangles(LineVertices,NumVertices,pIndices,numIndices,RGBA8(0x00,0x1f,0x00,0x00));		
						}
					} 
					else 
					{
						//maybe object is an attached character 
						ICharacterInstance* pICharacterInstance = pIAttachmentObject->GetICharacterInstance();
						if (pICharacterInstance) 
						{
							Matrix34 m34 = Matrix34( pIAttachment->GetAttAbsoluteDefault() );
							pICharacterInstance->DrawWireframeStatic(m34,0,RGBA8(0x00,0x1f,0x00,0x00));
						}
					}
				}
			} 
			else 
			{
				if (cp.basemodel) 
				{
					m_pCharacterBase->DrawWireframeStatic( Matrix34(IDENTITY),0,RGBA8(0x1f,0x00,0x00,0x00));
				}
			}


			//-----------------------------------------------------------------------
			//---            use arcball to re-orient attachment                  ---
			//-----------------------------------------------------------------------
			if (numAttachment) 
			{
				if (m_SelectedAttachment != -1) 
				{
					IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByIndex(m_SelectedAttachment);
					if (pIAttachment) 
					{
						uint32 type = pIAttachment->GetType();
						if (type==CA_BONE || type==CA_FACE) 
						{
							//<Attachment AName="back01" Type="CA_FACE" Rotation="-0.075126655,-0.99558783,0.03969615,   0.03981521" Position="0.041536111,-0.15446994,1.4762511" BoneName="" Binding="objects/characters/attachment/nanosuit/nanosuit_pouch.cgf" Flags="0" Material="objects/characters/attachment/nanosuit/Nanosuit_pockets"/>
							Quat WRot=m_ArcBall.DragRotation*m_ArcBall.ObjectRotation;
							Vec3 WPos=m_ArcBall.sphere.center;
							pIAttachment->SetAttAbsoluteDefault( QuatT(WRot,WPos) );

						//	uint32 ypos = 300;
						//	float color1[4] = {1,1,1,1};
						//	renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"Attachment rotation: %15.10f (%15.10f %15.10f %15.10f)", WRot.w, WRot.v.x,WRot.v.y,WRot.v.z );
						//	ypos+=10;
						//	renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"Attachment position: %15.10f %15.10f %15.10f", WPos.x, WPos.y, WPos.z);
						//	ypos+=10;

							pIAttachment->ProjectAttachment();

							if (m_Button_ROTATE) 
							{
								m_ArcBall.Draw_Sphere( GetCamera(),pAuxGeom );
							}
							if (m_Button_MOVE) 
							{
								SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
								renderFlags.SetFillMode( e_FillModeSolid );
								pAuxGeom->SetRenderFlags( renderFlags );
								Matrix34 m34=Matrix34(Matrix33(m_ArcBall.DragRotation*m_ArcBall.ObjectRotation),m_ArcBall.sphere.center );
								m_AxisHelper.DrawAxis(m34,m_displayContext);
							}
						}
					}
				}
			}


		} //if resetmode
	} //if character

	m_SelectionUpdate=1;
	CModelViewport::OnRender();

	if (m_pAnimationControlDlg != 0)
		m_pAnimationControlDlg->Update();

}

void CModelViewportCE::SetCharacterChangeListener(ICharacterChangeListener* pListener)
{
	m_pCharacterChangeListener = pListener;
}

void CModelViewportCE::SendOnCharacterChanged()
{
	if ( m_pCharacterChangeListener )
		m_pCharacterChangeListener->OnCharacterChanged();
}
