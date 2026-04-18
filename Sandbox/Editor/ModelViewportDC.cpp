//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File: PathFollowing.cpp
//  Implementation of the DrawCharacter function
//
//	History:
//	October 16, 2006: Created by Ivo Herzeg <ivo@crytek.de>
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ModelViewport.h"
#include "CharacterEditor\CharPanel_Animation.h"
#include "ICryAnimation.h"
#include <I3DEngine.h>
#include <IPhysics.h>
#include <ITimer.h>
#include "IRenderAuxGeom.h"

#define NUM_FUTURE_POINTS (40)


extern uint32 g_ypos;




//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CModelViewport::DrawCharacter( ICharacterInstance* pInstance, SRendParams rp )
{
	g_ypos = 162;
	float color1[4] = {1,1,1,1};

	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
	pAuxGeom->SetRenderFlags( renderFlags );

	f32 FrameTime = GetIEditor()->GetSystem()->GetITimer()->GetFrameTime();
	m_AverageFrameTime = pInstance->GetAverageFrameTime(); 



	ISkeletonAnim* pISkeletonAnim = pInstance->GetISkeletonAnim();
	bool bTransRot2000	= (m_pCharPanel_Animation ? m_pCharPanel_Animation->GetAnimationDrivenMotion() : false);
	pISkeletonAnim->SetAnimationDrivenMotion(bTransRot2000);

	IAnimationSet* pIAnimationSet = pInstance->GetIAnimationSet();



	uint32 rm = pInstance->GetResetMode();
	if (rm) 
	{
		I3DEngine* pI3DEngine = GetIEditor()->Get3DEngine();
		char text[512];
		sprintf(text, "ResetMode");
		pI3DEngine->DrawTextRA(00,10, text );
	}


	f32 fZoomFactor = 0.01f+0.99f*(RAD2DEG(GetCamera().GetFov())/90.f);  
	f32 fDistance = GetViewTM().GetTranslation().GetLength()*fZoomFactor;
	if (mv_disableLod)
		fDistance = 0;

	m_EntityMat=Matrix34(m_PhysEntityLocation);
	m_PrevEntityMat=m_EntityMat;


	//------------------------------------------------------------------------------
	//---                           unit-tests                                   ---
	//------------------------------------------------------------------------------

	GetISystem()->GetIAnimationSystem()->SetScalingLimits( Vec2(0.7f, 3.5f) );

	if (m_PlayerControl==0)
		AnimPreview_UnitTest( pInstance, pIAnimationSet, rp );

	uint32 IsHuman=1;
	if (m_PlayerControl==1 && IsHuman)
		PlayerControl_UnitTest( pInstance, pIAnimationSet, rp );

	if (m_PlayerControl==2 && IsHuman)
		PathFollowing_UnitTest( pInstance, pIAnimationSet, rp );

	if (m_PlayerControl==4 && IsHuman)
		Idle2Move_UnitTest( pInstance, pIAnimationSet, rp );

	if (m_PlayerControl==8 && IsHuman)
		IdleStep_UnitTest( pInstance, pIAnimationSet, rp );
}











//////////////////////////////////////////////////////////////////////////
void CModelViewport::DrawGrid( const Quat& m33, const Vec3& MotionTranslation,const Vec3& FootSlide, const Matrix33& rGridRot)
{
	if (!m_renderer)
		return;


	if (m_pCharPanel_Animation) 
	{
		bool bTransRot = m_pCharPanel_Animation->GetAnimationDrivenMotion();
		if (bTransRot)
			m_GridOrigin += m33*MotionTranslation + FootSlide;

	}


	if (m_GridOrigin.x>+1.0f) m_GridOrigin.x-=1.0f; 
	if (m_GridOrigin.y>+1.0f) m_GridOrigin.y-=1.0f; 
	//	if (m_GridOrigin.z>+1.0f) m_GridOrigin.z-=1.0f; 

	if (m_GridOrigin.x<-1.0f) m_GridOrigin.x+=1.0f; 
	if (m_GridOrigin.y<-1.0f) m_GridOrigin.y+=1.0f; 
	//	if (m_GridOrigin.z<-1.0f) m_GridOrigin.z+=1.0f; 

	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );

	float XR = 45;
	float YR = 45;
	Vec3 axis=m33.GetColumn0();

	Matrix33 SlopeMat33=rGridRot;
	// [MichaelS 13/2/2007] Stop breaking the facial editor! Don't assume m_pCharPanel_Animation is valid! You (and I) know who you are!
	uint32 GroundAlign = (m_pCharPanel_Animation ? m_pCharPanel_Animation->GetGroundAlign() : 1);
	if (GroundAlign==0)
		SlopeMat33= Matrix33::CreateRotationAA( -m_absCurrentSlope, axis);

	// Draw grid.
	float step = 0.25f;
	for (float x = -XR; x < XR; x+=step)
	{
		Vec3 p0=Vec3(x,-YR,0)+m_GridOrigin;		Vec3 p1=Vec3(x,YR,0)+m_GridOrigin;
		pAuxGeom->DrawLine( SlopeMat33*p0,RGBA8(0x7f,0x7f,0x7f,0x00), SlopeMat33*p1,RGBA8(0x7f,0x7f,0x7f,0x00) );
	}
	for (float y = -YR; y < YR; y+=step)
	{
		Vec3 p0=Vec3(-XR,y,0)+m_GridOrigin;		Vec3 p1=Vec3(XR,y,0)+m_GridOrigin;
		pAuxGeom->DrawLine( SlopeMat33*p0,RGBA8(0x7f,0x7f,0x7f,0x00), SlopeMat33*p1,RGBA8(0x7f,0x7f,0x7f,0x00) );
	}

	// Draw axis.
	if (mv_showBase)
		DrawCoordSystem(QuatT(IDENTITY),10.0f);

}



//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------

void CModelViewport::DrawHeightField()
{
	if (!m_renderer)
		return;

	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );

	// Draw axis.
	if (mv_showBase)
	{
		const float BASE=10.0f;
		pAuxGeom->DrawLine( Vec3(-BASE,0.00f,0.01f),RGBA8(0x7f,0x00,0x00,0x00), Vec3( BASE,0.00f,0.01f),RGBA8(0xff,0x7f,0x7f,0x00) );
		pAuxGeom->DrawLine( Vec3(0.00f,-BASE,0.01f),RGBA8(0x00,0x7f,0x00,0x00), Vec3(0.00f, BASE,0.01f),RGBA8(0x7f,0xff,0x7f,0x00) );
		pAuxGeom->DrawLine( Vec3(0.00f,0.00f,-BASE),RGBA8(0x00,0x00,0x7f,0x00), Vec3(0.00f,0.00f, BASE),RGBA8(0x7f,0x7f,0xff,0x00) );
	}

#define ROW (40*2)
#define COL (40*2)
#define QUADS ((ROW-1)*(COL-1))

	float step = 0.50f;
	const uint32 NumVertices=ROW*COL;
	const uint32 NumIndices=QUADS*2*3;
	uint32 NumVertices2=0;

	m_arrVerticesHF.resize(NumVertices);
	m_arrIndicesHF.resize(NumIndices);

	int32 y0=-COL/2;
	for (uint32 y=0; y<COL; y++)
	{
		int32 x0=-ROW/2;
		for (uint32 x=0; x<ROW; x++)
		{
			Vec3 p=Vec3(step*x0, step*y0,0)+m_GridOrigin;
			m_arrVerticesHF[x+y*ROW]=p;
			NumVertices2++;
			x0++;
		}
		y0++;
	}


	uint32 IndexCounter=0;
	for (uint32 y=0; y<(COL-1); y++)
	{
		for (uint32 x=0; x<(ROW-1); x++)
		{
			uint32 aval = (y+x)&1;
			if (aval)
			{
				m_arrIndicesHF[0+IndexCounter]=x+0+ROW*(y+0); //0
				m_arrIndicesHF[1+IndexCounter]=x+1+ROW*(y+0); //1
				m_arrIndicesHF[2+IndexCounter]=x+0+ROW*(y+1); //3
				IndexCounter+=3;

				m_arrIndicesHF[0+IndexCounter]=x+1+ROW*(y+0); //1
				m_arrIndicesHF[1+IndexCounter]=x+1+ROW*(y+1); //4
				m_arrIndicesHF[2+IndexCounter]=x+0+ROW*(y+1); //3
				IndexCounter+=3;
			}
		}
	}

	//float color1[4] = {1,1,1,1};
	//m_renderer->Draw2dLabel(12,g_ypos,1.2f,color1,false,"heightfield: vb:%d   vb2:%d  ib:%d tri:%d NumIndices:%d",NumVertices,NumVertices2,IndexCounter,IndexCounter/3, NumIndices );
	//g_ypos+=10;

	{
		pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
		pAuxGeom->DrawTriangles(&m_arrVerticesHF[0],NumVertices,  &m_arrIndicesHF[0],IndexCounter, RGBA8(0x2f,0x2f,0x3f,0x00) );		
	}

}



//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

int PostProcessCallback(ICharacterInstance* pInstance,void* pPlayer)
{
	//process bones specific stuff (IK, torso rotation, etc)
	((CModelViewport*)pPlayer)->ExternalPostProcessing(pInstance);
	return 1;
}
void CModelViewport::ExternalPostProcessing(ICharacterInstance* pInstance)
{
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	ISkeletonAnim* pISkeletonAnim = pInstance->GetISkeletonAnim();

	//	float color1[4] = {1,1,1,1};
	//	m_renderer->Draw2dLabel(12,g_ypos,1.2f,color1,false,"callback Radiant: %f %f %f",offset.t.x,offset.t.y,offset.t.z );
	//	g_ypos+=10;

	if (m_pCharPanel_Animation)
	{
		uint32 GroundAlign = m_pCharPanel_Animation->GetGroundAlign();

		uint32 LFootIK = m_pCharPanel_Animation->GetLFootIK();
		uint32 RFootIK = m_pCharPanel_Animation->GetRFootIK();
		uint32 LArmIK = m_pCharPanel_Animation->GetLArmIK();
		uint32 RArmIK = m_pCharPanel_Animation->GetRArmIK();

		if (GroundAlign)
			UseGroundAlignTest(pInstance);  //just for model-view port


		if (LFootIK)
		{
			UseCCDIK(pInstance);
			UseHumanLimbIK(pInstance,CA_LEG_LEFT);
		}
		if (RFootIK)
		{
			UseCCDIK(pInstance);
			UseHumanLimbIK(pInstance,CA_LEG_RIGHT);
		}

		if (LArmIK)
		{
			UseCCDIK(pInstance);
			UseHumanLimbIK(pInstance,CA_ARM_LEFT);
		}
		if (RArmIK)
		{
			UseCCDIK(pInstance);
			UseHumanLimbIK(pInstance,CA_ARM_RIGHT);
		}


	}


}













//----------------------------------------------------------------------------------
//  Lineseg_OBB
//
//	just ONE intersection point is calculated, and thats the entry point           - 
//  Lineseg and OBB are assumed to be in the same space
//
//--- 0x00 = no intersection (output undefined)           --------------------------
//--- 0x01 = intersection (intersection point in output)              --------------
//--- 0x02 = start of Lineseg is inside the OBB (ls.start is output) 
//----------------------------------------------------------------------------------
inline uint8	Lineseg_OBB( const Lineseg& lseg,const Vec3& pos,const OBB& obb, Vec3& output, Vec3& normal ) 
{
	AABB aabb(obb.c-obb.h,obb.c+obb.h);
	Lineseg ls((lseg.start-pos)*obb.m33,(lseg.end-pos)*obb.m33);

	uint8 cflags;
	float cosine;
	Vec3 cut;
	Vec3 lnormal=(ls.start-ls.end).GetNormalized();
	//--------------------------------------------------------------------------------------
	//----         check if "ls.start" is inside of AABB    ---------------------------
	//--------------------------------------------------------------------------------------
	cflags =(ls.start.x>aabb.min.x)<<0;
	cflags|=(ls.start.x<aabb.max.x)<<1;
	cflags|=(ls.start.y>aabb.min.y)<<2;
	cflags|=(ls.start.y<aabb.max.y)<<3;
	cflags|=(ls.start.z>aabb.min.z)<<4;
	cflags|=(ls.start.z<aabb.max.z)<<5;
	if (cflags==0x3f)	{ 
		//ls.start is inside of aabb
		output=obb.m33*ls.start+pos;	return 0x02; 
	}

	//--------------------------------------------------------------------------------------
	//----         check intersection with x-planes           ------------------------------
	//--------------------------------------------------------------------------------------
	if (lnormal.x) {
		if ( (ls.start.x<aabb.min.x) && (ls.end.x>aabb.min.x) ) {
			normal=-obb.m33.GetColumn0();
			cosine = (-ls.start.x+(+aabb.min.x))/lnormal.x;
			cut(aabb.min.x,ls.start.y+(lnormal.y*cosine),ls.start.z+(lnormal.z*cosine));
			//check if cut-point is inside YZ-plane border
			if ((cut.y>aabb.min.y) && (cut.y<aabb.max.y) && (cut.z>aabb.min.z) && (cut.z<aabb.max.z)) {
				output=obb.m33*cut+pos;	return 0x01;
			}
		}
		if ( (ls.start.x>aabb.max.x) && (ls.end.x<aabb.max.x) ) {
			normal=obb.m33.GetColumn0();
			cosine = (+ls.start.x+(-aabb.max.x))/lnormal.x;
			cut(aabb.max.x,ls.start.y-(lnormal.y*cosine),ls.start.z-(lnormal.z*cosine));
			//check if cut-point is inside YZ-plane border
			if ((cut.y>aabb.min.y) && (cut.y<aabb.max.y) && (cut.z>aabb.min.z) && (cut.z<aabb.max.z)) {
				output=obb.m33*cut+pos;	return 0x01;
			}
		}
	}
	//--------------------------------------------------------------------------------------
	//----         check intersection with z-planes           ------------------------------
	//--------------------------------------------------------------------------------------
	if (lnormal.z) {
		if ( (ls.start.z<aabb.min.z) && (ls.end.z>aabb.min.z) ) {
			normal=-obb.m33.GetColumn2();
			cosine = (-ls.start.z+(+aabb.min.z))/lnormal.z;
			cut(ls.start.x+(lnormal.x*cosine),ls.start.y+(lnormal.y*cosine),aabb.min.z);
			//check if cut-point is inside XY-plane border
			if ((cut.x>aabb.min.x) && (cut.x<aabb.max.x) && (cut.y>aabb.min.y) && (cut.y<aabb.max.y)) {
				output=obb.m33*cut+pos;	return 0x01;
			}
		}
		if ( (ls.start.z>aabb.max.z) && (ls.end.z<aabb.max.z) ) {
			normal=obb.m33.GetColumn2();
			cosine = (+ls.start.z+(-aabb.max.z))/lnormal.z;
			cut(ls.start.x-(lnormal.x*cosine),ls.start.y-(lnormal.y*cosine),aabb.max.z);
			//check if cut-point is inside XY-plane border
			if ((cut.x>aabb.min.x) && (cut.x<aabb.max.x) && (cut.y>aabb.min.y) && (cut.y<aabb.max.y)) {
				output=obb.m33*cut+pos;	return 0x01;
			}
		}
	}
	//--------------------------------------------------------------------------------------
	//----         check intersection with y-planes           ------------------------------
	//--------------------------------------------------------------------------------------
	if (lnormal.y) {
		if ( (ls.start.y<aabb.min.y) && (ls.end.y>aabb.min.y) ) {
			normal=-obb.m33.GetColumn1();
			cosine = (-ls.start.y+(+aabb.min.y))/lnormal.y;
			cut(ls.start.x+(lnormal.x*cosine), aabb.min.y, ls.start.z+(lnormal.z*cosine));
			//check if cut-point is inside XZ-plane border
			if ((cut.x>aabb.min.x) && (cut.x<aabb.max.x) && (cut.z>aabb.min.z) && (cut.z<aabb.max.z)) {
				output=obb.m33*cut+pos;	return 0x01;
			}
		}
		if ( (ls.start.y>aabb.max.y) && (ls.end.y<aabb.max.y) ) {
			normal=obb.m33.GetColumn1();
			cosine = (+ls.start.y+(-aabb.max.y))/lnormal.y;
			cut(ls.start.x-(lnormal.x*cosine), aabb.max.y, ls.start.z-(lnormal.z*cosine));
			//check if cut-point is inside XZ-plane border
			if ((cut.x>aabb.min.x) && (cut.x<aabb.max.x) && (cut.z>aabb.min.z) && (cut.z<aabb.max.z)) {
				output=obb.m33*cut+pos;	return 0x01;
			}
		}
	}
	//no intersection
	return 0x00;
}



//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
Plane CModelViewport::RayCast(const QuatT& rPhysLocation, f32 fAnimHeight, const Vec3& WorldMiddlePos, const Vec3& obbpos, const OBB& obb, Vec3& Intersection  ) 
{
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();

	Vec3 WPos = WorldMiddlePos;
	Lineseg ls_middle;
	ls_middle.start	=WPos+Vec3(0.0f,0.0f, 0.6f);
	ls_middle.end		=WPos+Vec3(0.0f,0.0f,-3.0f);
	//pAuxGeom->DrawLine( ls_middle.start,RGBA8(0x1f,0xff,0x1f,0x00),ls_middle.end,RGBA8(0x1f,0xff,0x1f,0x00) );

	Vec3 GroundIntersection(0,0,-9999.0f);
	Vec3 GroundNormal(0,0,1);
	int8 ground = Lineseg_OBB( ls_middle,obbpos,obb, GroundIntersection,GroundNormal );
	GroundNormal				=	GroundNormal*rPhysLocation.q;
	GroundIntersection	=	rPhysLocation.GetInverted()*GroundIntersection;

	if (GroundNormal.z<0.7f)
		GroundNormal.z = 0.7f;
	GroundNormal.Normalize();

	Plane GroundPlane(Vec3(0,0,1),0);
	if (ground==1)
	{
		if ( (GroundIntersection.z) > (fAnimHeight-0.05f) )
			GroundPlane=Plane::CreatePlane(GroundNormal,GroundIntersection);
		else
			GroundPlane=Plane::CreatePlane(Vec3(0,0,1),Vec3(0,0,fAnimHeight));
	}
	//	pIRenderer->D8Print("GroundPlane: %f %f %f  d:%f", GroundPlane.n.x,GroundPlane.n.y,GroundPlane.n.z,GroundPlane.d );

	Intersection = GroundIntersection;
	return GroundPlane;
}




//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------

uint32 CModelViewport::UseGroundAlignTest(ICharacterInstance* pInstance)
{
	float color1[4] = {1,1,1,1};

	f32 FrameTime = GetIEditor()->GetSystem()->GetITimer()->GetFrameTime();
	//m_AverageFrameTime = pInstance->GetAverageFrameTime(); 
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	ISkeletonAnim* pISkeletonAnim = pInstance->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = pInstance->GetISkeletonPose();


	uint32 strg=0;
	if (CheckVirtualKey(VK_LCONTROL))  strg=1;
	if (CheckVirtualKey(VK_RCONTROL))  strg=1;

	/*
	f32 fMoveUpDown=0;
	if (strg==0)
	{
	if (CheckVirtualKey(VK_NUMPAD8)) fMoveUpDown = -1.0f;
	if (CheckVirtualKey(VK_NUMPAD2)) fMoveUpDown = +1.0f;
	}
	m_GroundOBBPos.z += fMoveUpDown*FrameTime;
	*/

	const f32 MaxSlope=0.4f;
	if (strg)
	{
		f32 fRotUpDown=0;
		if ( CheckVirtualKey(VK_NUMPAD8) ) fRotUpDown = +1.0f;
		if ( CheckVirtualKey(VK_NUMPAD2) ) fRotUpDown = -1.0f;
		m_udGround+=fRotUpDown*FrameTime;
		if (m_udGround>+MaxSlope) m_udGround=+MaxSlope;
		if (m_udGround<-MaxSlope) m_udGround=-MaxSlope;

		f32 fRotLeftRight=0;
		if ( CheckVirtualKey(VK_NUMPAD4) ) fRotLeftRight = +1.0f;
		if ( CheckVirtualKey(VK_NUMPAD6) ) fRotLeftRight = -1.0f;
		m_lrGround+=fRotLeftRight*FrameTime;
		if (m_lrGround>+MaxSlope) m_lrGround=+MaxSlope;
		if (m_lrGround<-MaxSlope) m_lrGround=-MaxSlope;

		if ( CheckVirtualKey(VK_NUMPAD5) )
		{
			if (m_udGround<0)
			{
				m_udGround+=FrameTime;
				if (m_udGround>0)	m_udGround=0;
			}
			if (m_udGround>0)
			{
				m_udGround-=FrameTime;
				if (m_udGround<0)	m_udGround=0;
			}

			if (m_lrGround<0)
			{
				m_lrGround+=FrameTime;
				if (m_lrGround>0)	m_lrGround=0;
			}
			if (m_lrGround>0)
			{
				m_lrGround-=FrameTime;
				if (m_lrGround<0)	m_lrGround=0;
			}

		}

	}



	m_GroundOBB.m33	=	Matrix33::CreateRotationXYZ(Ang3(m_udGround,m_lrGround,0));
	pAuxGeom->DrawOBB(m_GroundOBB,m_GroundOBBPos,1,RGBA8(0x1f,0x1f,0x3f,0xff),eBBD_Extremes_Color_Encoded);


	//m_renderer->Draw2dLabel(12,g_ypos,1.5f,color1,false,"GroundAngle: %f", acos_tpl(m_GroundOBB.m33.m22) );
	//g_ypos+=14;

	//------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------



	{
		int32 LHeelIdx	=	pISkeletonPose->GetJointIDByName("Bip01 L Heel");
		int32 LToeNIdx	=	pISkeletonPose->GetJointIDByName("Bip01 L Toe0");
		if (LHeelIdx<0) 
			return 0;
		if (LToeNIdx<0) 
			return 0;
		Vec3 Final_LHeel	=	m_PhysEntityLocation*pISkeletonPose->GetAbsJointByID(LHeelIdx).t;
		Vec3 Final_LToeN	=	m_PhysEntityLocation*pISkeletonPose->GetAbsJointByID(LToeNIdx).t;

		Vec3 LGroundIntersectionHeel(0,0,-9999.0f);
		Plane LGroundPlaneHeel = RayCast(m_PhysEntityLocation,m_AnimatedCharacter.t.z, Final_LHeel, m_GroundOBBPos, m_GroundOBB, LGroundIntersectionHeel ); 
	//	pAuxGeom->DrawSphere(m_PhysEntityLocation.q*LGroundIntersectionHeel,0.03f,RGBA8(0xff,0xff,0xff,0xff));

		Vec3 LGroundIntersectionToe0(0,0,-9999.0f);
		Plane LGroundPlaneToe0 = RayCast(m_PhysEntityLocation,m_AnimatedCharacter.t.z, Final_LToeN, m_GroundOBBPos, m_GroundOBB, LGroundIntersectionToe0 ); 
	//	pAuxGeom->DrawSphere(m_PhysEntityLocation.q*LGroundIntersectionToe0,0.03f,RGBA8(0xff,0xff,0xff,0xff));


		Vec3 LGroundIntersection=LGroundIntersectionHeel;
		Plane LGroundPlane=LGroundPlaneHeel;
		f32 Ldist = fabsf(LGroundIntersectionHeel.z-LGroundIntersectionToe0.z);
		if (Ldist<0.1f)
		{
			LGroundIntersection	=(LGroundIntersectionHeel+LGroundIntersectionToe0)*0.5f;
			LGroundPlane				=(LGroundPlaneHeel+LGroundPlaneToe0)*0.5f;
		}

	//	m_renderer->Draw2dLabel(12,g_ypos,1.5f,color1,false,"LGroundIntersection: %f %f %f",LGroundIntersection.x,LGroundIntersection.y,LGroundIntersection.z );
	//	g_ypos+=14;

		static Plane LSmoothGroundPlane(Vec3(0,0,1),0);
		static Plane LSmoothGroundPlaneRate(Vec3(0,0,1),0);
		SmoothCD( LSmoothGroundPlane, LSmoothGroundPlaneRate, m_AverageFrameTime, LGroundPlane, 0.05f);
	//	m_renderer->Draw2dLabel(12,g_ypos,1.5f,color1,false,"LSmoothGroundPlane: %f %f %f  d:%f", LSmoothGroundPlane.n.x,LSmoothGroundPlane.n.y,LSmoothGroundPlane.n.z,LSmoothGroundPlane.d );
	//	g_ypos+=14;

		uint32 l0 = ( fabsf(LSmoothGroundPlane.n.x)<0.001f );
		uint32 l1 = ( fabsf(LSmoothGroundPlane.n.y)<0.001f );
		uint32 l2 = ( fabsf(1.0f-LSmoothGroundPlane.n.z)<0.001f );
		uint32 l3 = ( fabsf(LSmoothGroundPlane.d)<0.001f );
		uint32 lik	=	!(l0&l1&l2&l3);
	//	if (lik)
		{
			pISkeletonPose->SetFootGroundAlignmentCCD( CA_LEG_LEFT, LSmoothGroundPlane);
		//	m_renderer->Draw2dLabel(12,g_ypos,1.5f,color1,false,"LIK-ENABLED" );
		//	g_ypos+=14;
		}
	}



	{
		int32 RHeelIdx	=	pISkeletonPose->GetJointIDByName("Bip01 R Heel");
		int32 RToeNIdx	=	pISkeletonPose->GetJointIDByName("Bip01 R Toe0");
		if (RHeelIdx<0) 
			return 0;
		if (RToeNIdx<0) 
			return 0;
		Vec3 Final_RHeel	=	m_PhysEntityLocation*pISkeletonPose->GetAbsJointByID(RHeelIdx).t;
		Vec3 Final_RToeN	=	m_PhysEntityLocation*pISkeletonPose->GetAbsJointByID(RToeNIdx).t;

		Vec3 RGroundIntersectionHeel(0,0,-9999.0f);
		Plane RGroundPlaneHeel = RayCast(m_PhysEntityLocation,m_AnimatedCharacter.t.z, Final_RHeel, m_GroundOBBPos, m_GroundOBB, RGroundIntersectionHeel ); 
	//	pAuxGeom->DrawSphere(m_PhysEntityLocation.q*RGroundIntersectionHeel,0.03f,RGBA8(0xff,0xff,0xff,0xff));

		Vec3 RGroundIntersectionToe0(0,0,-9999.0f);
		Plane RGroundPlaneToe0 = RayCast(m_PhysEntityLocation,m_AnimatedCharacter.t.z, Final_RToeN, m_GroundOBBPos, m_GroundOBB, RGroundIntersectionToe0 ); 
	//	pAuxGeom->DrawSphere(m_PhysEntityLocation.q*RGroundIntersectionToe0,0.03f,RGBA8(0xff,0xff,0xff,0xff));

		Vec3 RGroundIntersection=RGroundIntersectionHeel;
		Plane RGroundPlane=RGroundPlaneHeel;
		f32 Rdist = fabsf(RGroundIntersectionHeel.z-RGroundIntersectionToe0.z);
		if (Rdist<0.1f)
		{
			RGroundIntersection	=(RGroundIntersectionHeel+RGroundIntersectionToe0)*0.5f;
			RGroundPlane				=(RGroundPlaneHeel+RGroundPlaneToe0)*0.5f;
		}

		static Plane RSmoothGroundPlane(Vec3(0,0,1),0);
		static Plane RSmoothGroundPlaneRate(Vec3(0,0,1),0);
		SmoothCD( RSmoothGroundPlane, RSmoothGroundPlaneRate, m_AverageFrameTime, RGroundPlane, 0.10f);
		//pRenderer->D8Print("SmoothGroundPlane: %f %f %f  d:%f", RSmoothGroundPlane.n.x,RSmoothGroundPlane.n.y,RSmoothGroundPlane.n.z,RSmoothGroundPlane.d );

		uint32 r0 = ( fabsf(RSmoothGroundPlane.n.x)<0.001f );
		uint32 r1 = ( fabsf(RSmoothGroundPlane.n.y)<0.001f );
		uint32 r2 = ( fabsf(1.0f-RSmoothGroundPlane.n.z)<0.001f );
		uint32 r3 = ( fabsf(RSmoothGroundPlane.d)<0.001f );
		uint32 rik	=	!(r0&r1&r2&r3);
	//	if (rik)
			pISkeletonPose->SetFootGroundAlignmentCCD(CA_LEG_RIGHT,RSmoothGroundPlane);
	}


	//  m_AnimatedCharacter.t.z=MinHigh; //stay on ground
	//	m_AnimatedCharacter.t.z=0.5f; //stay on ground

	return 1;
}



