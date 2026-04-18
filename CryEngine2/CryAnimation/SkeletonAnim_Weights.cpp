//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File:Anim_Weights.cpp
//  Implementation of Animation class for parameterisation
//
//	History:
//	January 12, 2005: Created by Ivo Herzeg <ivo@crytek.de>
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <I3DEngine.h>
#include <IRenderAuxGeom.h>
#include "CharacterInstance.h"
#include "Model.h"
#include "ModelSkeleton.h"
#include "CharacterManager.h"
#include <float.h>



Vec3 CSkeletonAnim::WeightComputation3( const Vec2& P, const Vec2 &v0, const Vec2 &v1, const Vec2 &v2 ) 
{
	Vec2 p=P;
	Plane plane = Plane::CreatePlane( v0,v1,Vec3(v0.x,v0.y,1) ); 
	f32 distance = plane|P;
	if (distance>0)
	{
		p.x = P.x-(distance*plane.n.x);	
		p.y = P.y-(distance*plane.n.y);	
	}
	Vec2 e0 = v0-v2;
	Vec2 e1 = v1-v2;
	Vec2 p2	=	p-v2;
	f32 w0 =	p2.x*e1.y	- e1.x*p2.y;
	f32 w1 =	e0.x*p2.y	-	p2.x*e0.y;
	f32 w2 =	e0.x*e1.y -	e1.x*e0.y - w0-w1;
	f32 sum = w0+w1+w2;	
	return Vec3(w0/sum,w1/sum,w2/sum);
}

Vec4 CSkeletonAnim::WeightComputation4( const Vec2& P, const Vec2 &p0, const Vec2 &p1, const Vec2 &p2, const Vec2 &p3 ) 
{
	Vec4 w,Weight4;
	struct TW3
	{
		static ILINE void Weight3( const Vec2& p, const Vec2 &v0, const Vec2 &v1, const Vec2 &v2, f32&w0,f32&w1,f32&w2,f32&w3 )
		{
			w0=0;w1=0;w2=0;w3=0;
			Plane plane = Plane::CreatePlane( v0,v1,Vec3(v0.x,v0.y,1) ); 
			if ((plane|p)<=0)
			{
				Vec2 e0 = v0-v2;
				Vec2 e1 = v1-v2;
				Vec2 p2	=	p-v2;
				w0 =	p2.x*e1.y	- e1.x*p2.y;
				w1 =	e0.x*p2.y	-	p2.x*e0.y;
				w2 =	e0.x*e1.y -	e1.x*e0.y - w0-w1;
			}
		}
	};

	TW3::Weight3( P, p1,p3,p0,  w.y,w.w,w.x, w.z );
	Weight4=w;
	TW3::Weight3( P, p3,p1,p2,  w.w,w.y,w.z, w.x );
	Weight4+=w;
	TW3::Weight3( P, p2,p0,p1,  w.z,w.x,w.y, w.w );
	Weight4+=w;
	TW3::Weight3( P, p0,p2,p3,  w.x,w.z,w.w, w.y );
	Weight4+=w;

	Weight4 /= (Weight4.x+Weight4.y+Weight4.z+Weight4.w);

	if (Weight4.x<0)	Weight4.x=0;
	if (Weight4.y<0)	Weight4.y=0;
	if (Weight4.z<0)	Weight4.z=0;
	if (Weight4.w<0)	Weight4.w=0;
	if (Weight4.x>1)	Weight4.x=1;
	if (Weight4.y>1)	Weight4.y=1;
	if (Weight4.z>1)	Weight4.z=1;
	if (Weight4.w>1)	Weight4.w=1;

	return Weight4;
}



//------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------

ILINE StrafeWeights6 CSkeletonAnim::WeightCalculation6( const Vec2& P, const Vec2 &F, const Vec2 &FL, const Vec2 &L, const Vec2 &B, const Vec2 &BR, const Vec2 &R ) 
{
	StrafeWeights6 w,Weight6(0,0,0,0,0,0);
	struct TW3
	{
		static ILINE void Weight3a( const Vec2& p, const Vec2 &v0, const Vec2 &v1, const Vec2 &v2,  f32&w0,f32&w1,f32&w2,  f32&w3,f32&w4,f32&w5 )
		{
			w0=0; w1=0; w2=0; w3=0; w4=0; w5=0;
			Plane plane = Plane::CreatePlane( v0,v1,Vec3(v0.x,v0.y,1) ); 
			if ((plane|p)<=0)
			{
				Vec2 e0 = v0-v2;
				Vec2 e1 = v1-v2;
				Vec2 p2	=	p-v2;
				w0 =	p2.x*e1.y	- e1.x*p2.y;
				w1 =	e0.x*p2.y	-	p2.x*e0.y;
				w2 =	e0.x*e1.y -	e1.x*e0.y - w0-w1;
			}
		}
		static ILINE void Weight3b( const Vec2& p, const Vec2 &v0, const Vec2 &v1, const Vec2 &v2,  f32&w0,f32&w1,f32&w2,  f32&w3,f32&w4,f32&w5 )
		{
			w0=0; w1=0; w2=0; w3=0; w4=0; w5=0;
			Plane plane1 = Plane::CreatePlane( v0,v1,Vec3(v0.x,v0.y,1) ); 
			Plane plane2 = Plane::CreatePlane( v2,v0,Vec3(v2.x,v2.y,1) ); 
			if ((plane1|p)<=0 && (plane2|p)<=0)
			{
				Vec2 e0 = v0-v2;
				Vec2 e1 = v1-v2;
				Vec2 p2	=	p-v2;
				w0 =	p2.x*e1.y	- e1.x*p2.y;
				w1 =	e0.x*p2.y	-	p2.x*e0.y;
				w2 =	e0.x*e1.y -	e1.x*e0.y - w0-w1;
			}
		}
	};


	TW3::Weight3a( P, B,FL,L,  w.b,w.fl,w.l, w.f,w.r,w.br );
	Weight6+=w;
	TW3::Weight3b( P, FL,B,BR,  w.fl,w.b,w.br, w.f,w.r,w.l );
	Weight6+=w;
	TW3::Weight3b( P, BR,F,FL,  w.br,w.f,w.fl, w.l,w.b,w.r );
	Weight6+=w;
	TW3::Weight3a( P, F,BR,R,  w.f,w.br,w.r, w.l,w.b,w.fl );
	Weight6+=w;

	/*
	TW3::Weight3a( P, BR,L,B,  w.br,w.l,w.b, w.fl,w.r,w.f );
	Weight6+=w;
	TW3::Weight3b( P, BR,FL,L,  w.br,w.fl,w.l,  w.b,w.r,w.f );
	Weight6+=w;
	TW3::Weight3b( P, FL,BR,R,  w.fl,w.br,w.r,  w.b,w.l,w.f );
	Weight6+=w;
	TW3::Weight3a( P, FL,R,F,  w.fl,w.r,w.f,  w.b,w.br,w.l );
	Weight6+=w;
	*/


	TW3::Weight3a( P, B,FL,L,  w.b,w.fl,w.l, w.f,w.r,w.br );
	Weight6+=w;
	TW3::Weight3b( P, B,F,FL,  w.b,w.f,w.fl, w.l,w.r,w.br );
	Weight6+=w;
	TW3::Weight3b( P, F,B,BR,  w.f,w.b,w.br, w.l,w.r,w.fl );
	Weight6+=w;
	TW3::Weight3a( P, F,BR,R,  w.f,w.br,w.r, w.l,w.b,w.fl );
	Weight6+=w;

	TW3::Weight3a( P, BR,L,B,  w.br,w.l,w.b, w.fl,w.r,w.f );
	Weight6+=w;
	TW3::Weight3b( P, L,BR,R,  w.l,w.br,w.r,  w.fl,w.f,w.b );
	Weight6+=w;
	TW3::Weight3b( P, R,FL,L,  w.r,w.fl,w.l,  w.br,w.f,w.b );
	Weight6+=w;
	TW3::Weight3a( P, FL,R,F,  w.fl,w.r,w.f,  w.br,w.l,w.b );
	Weight6+=w;

	Weight6 /= (Weight6.f+Weight6.fl+Weight6.l + Weight6.b+Weight6.br+Weight6.r);

	return Weight6;
}



//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------
LocomotionWeights6 CSkeletonAnim::WeightCalculationLoco6( Vec2& P,Vec2 &FL,Vec2 &FF,Vec2 &FR,  Vec2 &SL,Vec2 &SF,Vec2 &SR) 
{
	LocomotionWeights6 w,Weight6(0,0,0, 0,0,0);

	if (P.x<0)
	{
		Vec4 w4 = WeightComputation4( P, FL,SL,SF,FF ); 
		Weight6.fl+=w4.x;
		Weight6.sl+=w4.y;
		Weight6.sf+=w4.z;
		Weight6.ff+=w4.w;
	}
	else
	{
		Vec4 w4 = WeightComputation4( P, FF,SF,SR,FR ); 
		Weight6.ff+=w4.x;
		Weight6.sf+=w4.y;
		Weight6.sr+=w4.z;
		Weight6.fr+=w4.w;
	}


	f32 sum = (Weight6.fl+Weight6.ff+Weight6.fr + Weight6.sl+Weight6.sf+Weight6.sr );
	if (sum)
	{
		Weight6 /= sum;
	}
	else
	{
		Weight6.fl=-1; Weight6.ff=-1; Weight6.fr=-1;
		Weight6.sl=-1; Weight6.sf=-1; Weight6.sr=-1;
	}

	return Weight6;
}


//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------
LocomotionWeights7 CSkeletonAnim::WeightCalculationLoco7( Vec2& P,Vec2 &FL,Vec2 &FF,Vec2 &FR,Vec2 &MF,Vec2 &SL,Vec2 &SF,Vec2 &SR) 
{

	LocomotionWeights7 w,Weight7(0,0,0, 0, 0,0,0);
	struct TW3
	{
		static ILINE void Weight3a( Vec2 p,  Vec2 v0,Vec2 v1,Vec2 v2,   f32&w0,f32&w1,f32&w2,  f32&w3, f32&w4,f32&w5,f32&w6 )
		{
			w0=0; w1=0; w2=0; w3=0; w4=0; w5=0; w6=0;
			Plane plane = Plane::CreatePlane( v0,v1,Vec3(v0.x,v0.y,1) ); 
			if ((plane|p)<=0)
			{
				Vec2 e0 = v0-v2;
				Vec2 e1 = v1-v2;
				Vec2 p2	=	p-v2;
				w0 =	p2.x*e1.y	- e1.x*p2.y;
				w1 =	e0.x*p2.y	-	p2.x*e0.y;
				w2 =	e0.x*e1.y -	e1.x*e0.y - w0-w1;
			}
		}
		static ILINE void Weight3b( Vec2 p,  Vec2 v0,Vec2 v1,Vec2 v2,   f32&w0,f32&w1,f32&w2,  f32&w3, f32&w4,f32&w5,f32&w6 )
		{
			w0=0; w1=0; w2=0; w3=0; w4=0; w5=0; w6=0;
			Plane plane1 = Plane::CreatePlane( v0,v1,Vec3(v0.x,v0.y,1) ); 
			Plane plane2 = Plane::CreatePlane( v2,v0,Vec3(v2.x,v2.y,1) ); 
			if ((plane1|p)<=0 && (plane2|p)<=0)
			{
				Vec2 e0 = v0-v2;
				Vec2 e1 = v1-v2;
				Vec2 p2	=	p-v2;
				w0 =	p2.x*e1.y	- e1.x*p2.y;
				w1 =	e0.x*p2.y	-	p2.x*e0.y;
				w2 =	e0.x*e1.y -	e1.x*e0.y - w0-w1;
			}
		}
	};

	if (P.x<0)
	{
		TW3::Weight3a( P, MF,SL,SF,  w.mf,w.sl,w.sf, w.fl,w.ff,w.fr,w.sr );
		Weight7+=w;
		TW3::Weight3b( P, MF,FL,SL,  w.mf,w.fl,w.sl, w.ff,w.sf,w.fr,w.sr );
		Weight7+=w;
		TW3::Weight3a( P, FL,MF,FF,  w.fl,w.mf,w.ff, w.sl,w.sf,w.fr,w.sr );
		Weight7+=w;
	}
	else
	{
		TW3::Weight3a( P, SR,MF,SF,  w.sr,w.mf,w.sf, w.fr,w.ff,w.fl,w.sl );
		Weight7+=w;
		TW3::Weight3b( P, MF,SR,FR,  w.mf,w.sr,w.fr, w.ff,w.sf,w.fl,w.sl );
		Weight7+=w;
		TW3::Weight3a( P, MF,FR,FF,  w.mf,w.fr,w.ff, w.sr,w.sf,w.fl,w.sl );
		Weight7+=w;
	}

	f32 sum = (Weight7.fl+Weight7.ff+Weight7.fr + Weight7.mf + Weight7.sl+Weight7.sf+Weight7.sr );
	if (sum)
	{
		Weight7 /= sum;
	}
	else
	{
		assert(0);
		Weight7.fl=-1; Weight7.ff=-1; Weight7.fr=-1;
		Weight7.mf=-1;
		Weight7.sl=-1; Weight7.sf=-1; Weight7.sr=-1;
	}

	return Weight7;

}



struct BlendWeights_STF2
{
	f32    ff; 
	f32 fl,   fr; 
	f32	   fb;

	f32    sf; 
	f32 sl,   sr; 
	f32	   sb;
};

void CSkeletonAnim::WeightComputation_STF2( const GlobalAnimationHeader& rGlobalAnimHeader, const SLocoGroup& lmg, uint32 offset, BlendWeights_STF2& stf2 )
{
	CAnimationSet* pAnimationSet = &m_pInstance->m_pModel->m_AnimationSet;
	GlobalAnimationHeader* parrGlobalAnimations = &g_AnimationManager.m_arrGlobalAnimations[0];

	GlobalAnimationHeader& rsubGAH00 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x00] ]; 
	GlobalAnimationHeader& rsubGAH01 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x01] ]; 
	GlobalAnimationHeader& rsubGAH02 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x02] ]; 
	GlobalAnimationHeader& rsubGAH03 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x03] ]; 

	GlobalAnimationHeader& rsubGAH04 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x04] ]; 
	GlobalAnimationHeader& rsubGAH05 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x05] ]; 
	GlobalAnimationHeader& rsubGAH06 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x06] ]; 
	GlobalAnimationHeader& rsubGAH07 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x07] ]; 

	f32 x0	=	(lmg.m_BlendSpace.m_speed+1)*0.5f;
	f32 x1	=	1.0f-(lmg.m_BlendSpace.m_speed+1)*0.5f;
	Vec2 bF = Vec2(rsubGAH00.m_vVelocity*x0 + rsubGAH04.m_vVelocity*x1);
	Vec2 bL = Vec2(rsubGAH01.m_vVelocity*x0 + rsubGAH05.m_vVelocity*x1);
	Vec2 bB = Vec2(rsubGAH02.m_vVelocity*x0 + rsubGAH06.m_vVelocity*x1);
	Vec2 bR = Vec2(rsubGAH03.m_vVelocity*x0 + rsubGAH07.m_vVelocity*x1);
	StrafeWeights4 blend = pAnimationSet->GetStrafingWeights4(bF,bL,bB,bR, lmg.m_BlendSpace.m_strafe );

	f32 t = lmg.m_BlendSpace.m_strafe.GetLength();
	blend.f	=	blend.f*t + 0.25f*(1.0f-t);
	blend.l	=	blend.l*t + 0.25f*(1.0f-t);	
	blend.b	=	blend.b*t + 0.25f*(1.0f-t);
	blend.r	=	blend.r*t + 0.25f*(1.0f-t);	

	stf2.ff=blend.f*x0;	
	stf2.fl=blend.l*x0;	
	stf2.fb=blend.b*x0;
	stf2.fr=blend.r*x0;	

	stf2.sf=blend.f*x1;	
	stf2.sl=blend.l*x1;	
	stf2.sb=blend.b*x1;
	stf2.sr=blend.r*x1;	

	/*
	Vec2 F	=	Vec2( 0,+1); //0
	Vec2 R	=	Vec2(+1, 0); //1
	Vec2 L	=	Vec2(-1, 0); //2
	Vec2 B	=	Vec2( 0,-1); //3
	Vec2 movedir = F*blend.f + L*blend.l + B*blend.b + R*blend.r;
	f32 length = lmg.m_BlendSpace.m_strafe.GetLength();
	f32 lmd = movedir.GetLength();
	if (lmd>length)	{	movedir.NormalizeSafe(); movedir*=length; 	}

	Vec4 bw=WeightComputation4(movedir, F,L,B,R);

	stf2.ff=bw.x*x0;	
	stf2.fl=bw.y*x0;	
	stf2.fb=bw.z*x0;
	stf2.fr=bw.w*x0;	

	stf2.sf=bw.x*x1;	
	stf2.sl=bw.y*x1;	
	stf2.sb=bw.z*x1;
	stf2.sr=bw.w*x1;	
	*/

	/*
	f32 fColor[4] = {1,1,0,1};
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"      ff %f",stf2.ff); g_YLine+=0x10;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"fl %f   fr %f",stf2.fl,stf2.fr); g_YLine+=0x10;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"      fb %f",stf2.fb); g_YLine+=0x20;

	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"      sf %f",stf2.sf); g_YLine+=0x10;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"sl %f   sr %f",stf2.sl,stf2.sr); g_YLine+=0x10;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"      sb %f",stf2.sb); g_YLine+=0x20;
	*/
	return;
}

struct BlendWeights_S2
{
	f32      ff; 
	f32   ffl; 
	f32 fl,       fr; 
	f32	      fbr;
	f32	     fb;

	f32      sf; 
	f32   sfl; 
	f32 sl,       sr; 
	f32	      sbr;
	f32	     sb;
};

void CSkeletonAnim::WeightComputation_S2( const GlobalAnimationHeader& rGlobalAnimHeader, const SLocoGroup& lmg, uint32 offset, BlendWeights_S2& s2 )
{
	CAnimationSet* pAnimationSet = &m_pInstance->m_pModel->m_AnimationSet;
	GlobalAnimationHeader* parrGlobalAnimations = &g_AnimationManager.m_arrGlobalAnimations[0];

	GlobalAnimationHeader& rsubGAH00 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x00] ]; 
	GlobalAnimationHeader& rsubGAH01 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x01] ]; 
	GlobalAnimationHeader& rsubGAH02 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x02] ]; 
	GlobalAnimationHeader& rsubGAH03 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x03] ]; 
	GlobalAnimationHeader& rsubGAH04 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x04] ]; 
	GlobalAnimationHeader& rsubGAH05 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x05] ]; 

	GlobalAnimationHeader& rsubGAH06 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x06] ]; 
	GlobalAnimationHeader& rsubGAH07 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x07] ]; 
	GlobalAnimationHeader& rsubGAH08 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x08] ]; 
	GlobalAnimationHeader& rsubGAH09 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x09] ]; 
	GlobalAnimationHeader& rsubGAH0a = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x0a] ]; 
	GlobalAnimationHeader& rsubGAH0b = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x0b] ]; 

	f32 fColor[4] = {1,1,0,1};

	f32 x0=(lmg.m_BlendSpace.m_speed+1)*0.5f;
	f32 x1=1.0f-(lmg.m_BlendSpace.m_speed+1)*0.5f;

	Vec2 sF		= Vec2(rsubGAH00.m_vVelocity*x0 + rsubGAH06.m_vVelocity*x1);
	Vec2 sFL	= Vec2(rsubGAH01.m_vVelocity*x0 + rsubGAH07.m_vVelocity*x1);
	Vec2 sL		= Vec2(rsubGAH02.m_vVelocity*x0 + rsubGAH08.m_vVelocity*x1);
	Vec2 sB		= Vec2(rsubGAH03.m_vVelocity*x0 + rsubGAH09.m_vVelocity*x1);
	Vec2 sBR	= Vec2(rsubGAH04.m_vVelocity*x0 + rsubGAH0a.m_vVelocity*x1);
	Vec2 sR		= Vec2(rsubGAH05.m_vVelocity*x0 + rsubGAH0b.m_vVelocity*x1);
	StrafeWeights6 blend = pAnimationSet->GetStrafingWeights6(sF,sFL,sL, sB,sBR,sR, lmg.m_BlendSpace.m_strafe );

	f32 t = lmg.m_BlendSpace.m_strafe.GetLength();
	f32 p =	(1.0f/6.0f);
	blend.f		=	blend.f*t		+ p*(1.0f-t);
	blend.fl	=	blend.fl*t	+ p*(1.0f-t);
	blend.l		=	blend.l*t		+ p*(1.0f-t);	
	blend.b		=	blend.b*t		+ p*(1.0f-t);
	blend.br	=	blend.br*t	+ p*(1.0f-t);
	blend.r		=	blend.r*t		+ p*(1.0f-t);	

	s2.ff		=x0*blend.f;	
	s2.ffl	=x0*blend.fl;	
	s2.fl		=x0*blend.l;	
	s2.fb		=x0*blend.b;	
	s2.fbr	=x0*blend.br;	
	s2.fr		=x0*blend.r;	

	s2.sf		=x1*blend.f;	
	s2.sfl	=x1*blend.fl;	
	s2.sl		=x1*blend.l;	
	s2.sb		=x1*blend.b;	
	s2.sbr	=x1*blend.br;	
	s2.sr		=x1*blend.r;	


	/*
	Vec2 F	=	Vec2( 0,+1); //0
	Vec2 L	=	Vec2(-1, 0); //2
	Vec2 B	=	Vec2( 0,-1); //3
	Vec2 R	=	Vec2(+1, 0); //1
	Vec2 FL	=	(F+L)*0.5f; //0
	Vec2 BR	=	(B+R)*0.5f; //0

	Vec2 movedir= F*blend.f + FL*blend.fl + L*blend.l + B*blend.b + BR*blend.br + R*blend.r;
	f32 length = lmg.m_BlendSpace.m_strafe.GetLength();
	f32 lmd = movedir.GetLength();
	if (lmd>length)	{	movedir.NormalizeSafe(Vec2(0,1)); movedir*=length;	}

	StrafeWeights6 Weight6 = WeightCalculation6( movedir,  F,FL,L,  B,BR,R);

	s2.ff		=x0*Weight6.f;	
	s2.ffl	=x0*Weight6.fl;	
	s2.fl		=x0*Weight6.l;	
	s2.fb		=x0*Weight6.b;	
	s2.fbr	=x0*Weight6.br;	
	s2.fr		=x0*Weight6.r;	

	s2.sf		=x1*Weight6.f;	
	s2.sfl	=x1*Weight6.fl;	
	s2.sl		=x1*Weight6.l;	
	s2.sb		=x1*Weight6.b;	
	s2.sbr	=x1*Weight6.br;	
	s2.sr		=x1*Weight6.r;	*/
	return;
}



struct BlendWeights_ST2
{
	f32      ff; 
	f32   ffl; 
	f32 fl,       fr; 
	f32	      fbr;
	f32	     fb;

	f32      mf; 

	f32      sf; 
	f32   sfl; 
	f32 sl,       sr; 
	f32	      sbr;
	f32	     sb;

	f32	fftl,fftr; 
	f32	sftl,sftr; 
};

void CSkeletonAnim::WeightComputation_ST2( const GlobalAnimationHeader& rGlobalAnimHeader, const SLocoGroup& lmg, uint32 offset, BlendWeights_ST2& st2 )
{
	CAnimationSet* pAnimationSet = &m_pInstance->m_pModel->m_AnimationSet;
	GlobalAnimationHeader* parrGlobalAnimations = &g_AnimationManager.m_arrGlobalAnimations[0];

	GlobalAnimationHeader& rsubGAH00 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x00] ]; 
	GlobalAnimationHeader& rsubGAH01 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x01] ]; 
	GlobalAnimationHeader& rsubGAH02 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x02] ]; 
	GlobalAnimationHeader& rsubGAH03 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x03] ]; 
	GlobalAnimationHeader& rsubGAH04 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x04] ]; 
	GlobalAnimationHeader& rsubGAH05 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x05] ]; 

	GlobalAnimationHeader& rsubGAH06 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x06] ]; 

	GlobalAnimationHeader& rsubGAH07 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x07] ]; 
	GlobalAnimationHeader& rsubGAH08 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x08] ]; 
	GlobalAnimationHeader& rsubGAH09 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x09] ]; 
	GlobalAnimationHeader& rsubGAH0a = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x0a] ]; 
	GlobalAnimationHeader& rsubGAH0b = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x0b] ]; 
	GlobalAnimationHeader& rsubGAH0c = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x0c] ]; 

	//turns
	GlobalAnimationHeader& rsubGAH0d = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x0d] ]; //fast left 
	GlobalAnimationHeader& rsubGAH0e = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x0e] ]; //fast right 
	GlobalAnimationHeader& rsubGAH0f = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x0f] ]; //slow left
	GlobalAnimationHeader& rsubGAH10 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x10] ]; //slow right

	//calculate the strafe-weights
		
	f32	speed = lmg.m_BlendSpace.m_speed;
	f32 ffast=0, fmid=1-fabsf(speed), fslow=0;
	if (speed>0) ffast=speed; else fslow=1-(speed+1);
	f32 x0	=	(lmg.m_BlendSpace.m_speed+1)*0.5f; // fast
	f32 x1	= 1.0f-(lmg.m_BlendSpace.m_speed+1)*0.5f;	//slow





	Vec2 sF		= Vec2(rsubGAH00.m_vVelocity*x0 + rsubGAH07.m_vVelocity*x1);
	Vec2 sFL	= Vec2(rsubGAH01.m_vVelocity*x0 + rsubGAH08.m_vVelocity*x1);
	Vec2 sL		= Vec2(rsubGAH02.m_vVelocity*x0 + rsubGAH09.m_vVelocity*x1);
	Vec2 sB		= Vec2(rsubGAH03.m_vVelocity*x0 + rsubGAH0a.m_vVelocity*x1);
	Vec2 sBR	= Vec2(rsubGAH04.m_vVelocity*x0 + rsubGAH0b.m_vVelocity*x1);
	Vec2 sR		= Vec2(rsubGAH05.m_vVelocity*x0 + rsubGAH0c.m_vVelocity*x1);
	StrafeWeights6 blend = pAnimationSet->GetStrafingWeights6(sF,sFL,sL, sB,sBR,sR, lmg.m_BlendSpace.m_strafe );
	f32 ts			= lmg.m_BlendSpace.m_strafe.GetLength();
	f32 ps			=	(1.0f/6.0f);
	blend.f		=	blend.f*ts		+ ps*(1.0f-ts);
	blend.fl	=	blend.fl*ts	  + ps*(1.0f-ts);
	blend.l		=	blend.l*ts		+ ps*(1.0f-ts);	
	blend.b		=	blend.b*ts		+ ps*(1.0f-ts);
	blend.br	=	blend.br*ts	  + ps*(1.0f-ts);
	blend.r		=	blend.r*ts		+ ps*(1.0f-ts);	
	StrafeWeights6 StrafeWeights=blend;





	f32 L_fast=rsubGAH0d.m_fSpeed;	f32 F_fast=rsubGAH00.m_fSpeed;	f32 R_fast=rsubGAH0e.m_fSpeed;
	f32 F_mid	=rsubGAH06.m_fSpeed;
	f32 L_slow=rsubGAH0f.m_fSpeed;	f32 F_slow=rsubGAH07.m_fSpeed;	f32 R_slow=rsubGAH10.m_fSpeed;

	f32 TL_fast=rsubGAH0d.m_fTurnSpeed;	f32 TF_fast=rsubGAH00.m_fTurnSpeed;	f32 TR_fast=rsubGAH0e.m_fTurnSpeed;
	f32 TF_mid =rsubGAH06.m_fTurnSpeed;
	f32 TL_slow=rsubGAH0f.m_fTurnSpeed;	f32 TF_slow=rsubGAH07.m_fTurnSpeed;	f32 TR_slow=rsubGAH10.m_fTurnSpeed;


	f32 f0 = F_mid	-F_slow;
	f32 f1 = F_fast -F_slow;
	f32 mid = (f0/f1)*2-1;

	//calculate the turn-weights
	Vec2 FL=Vec2(-1, 1);	//fast left	
	Vec2 FF=Vec2( 0, 1);	//fast speed forward	
	Vec2 FR=Vec2( 1, 1);	//fast right	

	Vec2 MF=Vec2( 0, mid);	//middle speed forward	

	Vec2 SL=Vec2(-1,-1);	//slow left
	Vec2 SF=Vec2( 0,-1);	//slow speed forward	
	Vec2 SR=Vec2( 1,-1);	//slow right
	Vec2 p = Vec2(lmg.m_BlendSpace.m_turn,lmg.m_BlendSpace.m_speed);
	LocomotionWeights7 TurnWeights = WeightCalculationLoco7( p, FL,FF,FR, MF, SL,SF,SR );




	Vec2 strafe = lmg.m_BlendSpace.m_strafe.GetNormalizedSafe( Vec2(0,0) );
	f32 turnweight = fabsf(lmg.m_BlendSpace.m_turn*2);
	if (turnweight>1.0f)
		turnweight=1.0f;


	f32 fTurnPriority=0;

	if (m_CharEditMode)
	{
		f32 rad = atan2(strafe.x,strafe.y);
		fTurnPriority = fabsf( rad*2 ); //*turnweight;
		if (fTurnPriority>1.0f) fTurnPriority=1.0f;
		fTurnPriority=1.0f-fTurnPriority;
	}
	else
	{
		fTurnPriority = lmg.m_params[eMotionParamID_Curving].value;
	}

	fTurnPriority*=ts;
//	fTurnPriority*=0.5f;
	fTurnPriority*=clamp( (lmg.m_BlendSpace.m_fAllowLeaningSmooth*1.4f)-0.2f, 0.0f,1.0f);

	f32 staf = 1.0f-fTurnPriority;
	f32 turn = fTurnPriority;

	st2.ff	=	staf*x0*StrafeWeights.f+turn*TurnWeights.ff;	//fast forward speed
	st2.ffl	=	staf*x0*StrafeWeights.fl;	
	st2.fl	=	staf*x0*StrafeWeights.l;	
	st2.fb	=	staf*x0*StrafeWeights.b;	
	st2.fbr	=	staf*x0*StrafeWeights.br;	
	st2.fr	=	staf*x0*StrafeWeights.r;	

	st2.mf	=	turn*TurnWeights.mf;	//middle forward speed	

	st2.sf	=	staf*x1*StrafeWeights.f+turn*TurnWeights.sf;	//slow forward speed
	st2.sfl	=	staf*x1*StrafeWeights.fl;	
	st2.sl	=	staf*x1*StrafeWeights.l;	
	st2.sb	=	staf*x1*StrafeWeights.b;	
	st2.sbr	=	staf*x1*StrafeWeights.br;	
	st2.sr	=	staf*x1*StrafeWeights.r;	

	st2.fftl=turn*TurnWeights.fl;	//left
	st2.fftr=turn*TurnWeights.fr;	//right
	st2.sftl=turn*TurnWeights.sl;	//left
	st2.sftr=turn*TurnWeights.sr;	//right

	f32 sum = (st2.ff+st2.ffl+st2.fl+st2.fb+st2.fbr+st2.fr + st2.mf + st2.sf+st2.sfl+st2.sl+st2.sb+st2.sbr+st2.sr +(st2.fftl+st2.fftr+st2.sftl+st2.sftr));	
	assert( fabsf(sum-1.0f)<0.03f );

	st2.fftl=turn*TurnWeights.fl;	//left
	st2.fftr=turn*TurnWeights.fr;	//right
	st2.sftl=turn*TurnWeights.sl;	//left
	st2.sftr=turn*TurnWeights.sr;	//right

	f32 fColor[4] = {1,1,0,1};
	//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"m_curvingFraction:%f",m_curvingFraction );	g_YLine+=0x20;
	//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"x0:%f  x1:%f   fTurnPriority:%f  sum:%f",x0,x1, fTurnPriority,sum );	g_YLine+=0x20;


	//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t:%f   ffast:%f  fmid:%f  fslow:%f",t ,ffast,fmid,fslow );	g_YLine+=0x20;
	/*
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"                 F: %f",st2.ff);	g_YLine+=0x10;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"         FL: %f",st2.ffl );	g_YLine+=0x10;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"L: %f                              R: %f",st2.fl,st2.fr );	g_YLine+=0x10;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"                         FR: %f",st2.fbr );	g_YLine+=0x10;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"                 B: %f",st2.fb );	g_YLine+=0x20;

	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"                 F: %f",st2.mf);	g_YLine+=0x20;

	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"                 F: %f",st2.sf);	g_YLine+=0x10;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"         BL: %f",st2.sfl );	g_YLine+=0x10;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"L: %f                              R: %f",st2.sl,st2.sr );	g_YLine+=0x10;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"                         BR: %f",st2.fbr );	g_YLine+=0x10;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"                 B: %f",st2.fb );	g_YLine+=0x20;

	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"FL: %f  FR: %f",st2.fftl,st2.fftr );  g_YLine+=0x10;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"SL: %f  SR: %f",st2.sftl,st2.sftr );  g_YLine+=0x10;
	/**/

	

//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"FL: %f    FF: %f      FR: %f",st2.fftl,st2.ff,st2.fftr );  g_YLine+=0x10;
//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"          MF: %f",st2.mf);	g_YLine+=0x10;
//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"SL: %f    SF: %f      SR: %f",st2.sftl,st2.sf,st2.sftr );  g_YLine+=0x10;


}



struct BlendWeights_STX
{
	f32      ff; 
	f32 fl,       fr; 
	f32	     fb;

	f32      mf; 

	f32      sf; 
	f32 sl,       sr; 
	f32	     sb;

	f32	fftl,fftr; //9-a
	f32	sftl,sftr; //b-c
};

void CSkeletonAnim::WeightComputation_STX( const GlobalAnimationHeader& rGlobalAnimHeader, const SLocoGroup& lmg, uint32 offset, BlendWeights_STX& stx )
{
	CAnimationSet* pAnimationSet = &m_pInstance->m_pModel->m_AnimationSet;
	GlobalAnimationHeader* parrGlobalAnimations = &g_AnimationManager.m_arrGlobalAnimations[0];

	GlobalAnimationHeader& rsubGAH00 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x00] ]; 
	GlobalAnimationHeader& rsubGAH01 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x01] ]; 
	GlobalAnimationHeader& rsubGAH02 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x02] ]; 
	GlobalAnimationHeader& rsubGAH03 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x03] ]; 

	GlobalAnimationHeader& rsubGAH05 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x05] ]; 
	GlobalAnimationHeader& rsubGAH06 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x06] ]; 
	GlobalAnimationHeader& rsubGAH07 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x07] ]; 
	GlobalAnimationHeader& rsubGAH08 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x08] ]; 

	//calculate the strafe-weights
	f32	t = lmg.m_BlendSpace.m_speed;
	f32 ffast=0,fmid=1-fabsf(t),fslow=0;
	if (t>0) ffast=t; else fslow=1-(t+1);
	f32 x0	=	(lmg.m_BlendSpace.m_speed+1)*0.5f; // fast
	f32 x1 = 1.0f-(lmg.m_BlendSpace.m_speed+1)*0.5f;	//slow


	//	f32 ffast=0,fmid=1-fabsf(t),fslow=0;
	//	if (t>0) ffast=t; else fslow=1-(t+1);
	//	f32 x0 = (lmg.m_BlendSpace.m_speed+1)*0.5f;				//fast
	//	f32 x1 = 1.0f-(lmg.m_BlendSpace.m_speed+1)*0.5f;	//slow



	Vec2 sF		= Vec2(rsubGAH00.m_vVelocity*x0 + rsubGAH05.m_vVelocity*x1);
	Vec2 sL		= Vec2(rsubGAH01.m_vVelocity*x0 + rsubGAH06.m_vVelocity*x1);
	Vec2 sB		= Vec2(rsubGAH02.m_vVelocity*x0 + rsubGAH07.m_vVelocity*x1);
	Vec2 sR		= Vec2(rsubGAH03.m_vVelocity*x0 + rsubGAH08.m_vVelocity*x1);
	StrafeWeights4 blend = pAnimationSet->GetStrafingWeights4(sF,sL,sB,sR, lmg.m_BlendSpace.m_strafe );



	Vec2 dirF	=	Vec2( 0,+1); //0
	Vec2 dirL	=	Vec2(-1, 0); //2
	Vec2 dirB	=	Vec2( 0,-1); //3
	Vec2 dirR	=	Vec2(+1, 0); //5
	Vec2 movedir= dirF*blend.f +  dirL*blend.l + dirB*blend.b + dirR*blend.r;
	f32 length = lmg.m_BlendSpace.m_strafe.GetLength();
	f32 lmd = movedir.GetLength();
	if (lmd>length)	{	movedir.NormalizeSafe(Vec2(0,1)); movedir*=length;	}
	Vec4 StrafeWeights	=	WeightComputation4(movedir, dirF,dirL,dirB,dirR);





	//calculate the turn-weights
	Vec2 FL=Vec2(-1, 1);	//fast left	
	Vec2 FF=Vec2( 0, 1);	//fast speed forward	
	Vec2 FR=Vec2( 1, 1);	//fast right	

	Vec2 MF=Vec2( 0, 0);	//middle speed forward	

	Vec2 SL=Vec2(-1,-1);	//slow left
	Vec2 SF=Vec2( 0,-1);	//slow speed forward	
	Vec2 SR=Vec2( 1,-1);	//slow right
	Vec2 p = Vec2(lmg.m_BlendSpace.m_turn,lmg.m_BlendSpace.m_speed);
	LocomotionWeights7 TurnWeights = WeightCalculationLoco7( p, FL,FF,FR, MF, SL,SF,SR );




	Vec2 strafe = lmg.m_BlendSpace.m_strafe.GetNormalizedSafe( Vec2(0,0) );


	f32 turnweight = fabsf(lmg.m_BlendSpace.m_turn);

	f32 rad = atan2(strafe.x,strafe.y);
	f32 fTurnPriority = fabsf( rad*2 ); //*turnweight;
	if (fTurnPriority>1.0f) fTurnPriority=1.0f;

	fTurnPriority = lmg.m_params[eMotionParamID_Curving].value;

	f32 staf = fTurnPriority;
	f32 turn = 1.0f-fTurnPriority;

	f32 tw = turnweight*4;
	if (tw>1.0f)
		tw=1.0f;

	//turn*=tw; 
	staf=1.0f-turn; 






	stx.ff	=	staf*ffast*StrafeWeights.x+turn*TurnWeights.ff;	//fast forward speed
	stx.fl	=	staf*x0*   StrafeWeights.y;	
	stx.fb	=	staf*x0*   StrafeWeights.z;	
	stx.fr	=	staf*x0*   StrafeWeights.w;	

	stx.mf	=	staf*fmid* StrafeWeights.x+turn*TurnWeights.mf;		//middle forward speed	

	stx.sf	=	staf*fslow*StrafeWeights.x+turn*TurnWeights.sf;	//slow forward speed
	stx.sl	=	staf*x1*   StrafeWeights.y;	
	stx.sb	=	staf*x1*   StrafeWeights.z;	
	stx.sr	=	staf*x1*   StrafeWeights.w;	

	stx.fftl=turn*TurnWeights.fl;	//left
	stx.fftr=turn*TurnWeights.fr;	//right
	stx.sftl=turn*TurnWeights.sl;	//left
	stx.sftr=turn*TurnWeights.sr;	//right

	/*
	f32 fColor[4] = {1,1,0,1};
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"x0:%f  x1:%f  rad:%f  fTurnPriority:%f",x0,x1,rad,fTurnPriority );	g_YLine+=0x20;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t:%f   ffast:%f  fmid:%f  fslow:%f",t ,ffast,fmid,fslow );	g_YLine+=0x20;

	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"                 F: %f",stx.ff);	g_YLine+=0x10;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"L: %f                              R: %f",stx.fl,stx.fr );	g_YLine+=0x10;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"                 B: %f",stx.fb );	g_YLine+=0x20;

	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"                 F: %f",stx.mf);	g_YLine+=0x20;

	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"                 F: %f",stx.sf);	g_YLine+=0x10;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"L: %f                              R: %f",stx.sl,stx.sr );	g_YLine+=0x10;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"                 B: %f",stx.sb );	g_YLine+=0x10;

	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"FL: %f  FR: %f",TurnWeights.fl,TurnWeights.fr );  g_YLine+=0x10;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"SL: %f  SR: %f",TurnWeights.sl,TurnWeights.sr );  g_YLine+=0x10;
	*/

}





//---------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------
void CSkeletonAnim::WeightComputation(SLocoGroup& lmg)
{
	CAnimationSet* pAnimationSet = &m_pInstance->m_pModel->m_AnimationSet;

	//----------------------------------------------------------------------------------------------
	//------------               start of the real function               --------------------------
	//----------------------------------------------------------------------------------------------

	for (uint32 i=0; i<MAX_LMG_ANIMS; i++)
		lmg.m_fBlendWeight[i]=0;

	if (lmg.m_nLMGID>=0)
	{
		uint32 global_lmg_id=lmg.m_nGlobalLMGID;
		GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[global_lmg_id];
		assert(rGlobalAnimHeader.m_nBlendCodeLMG);

		GlobalAnimationHeader* parrGlobalAnimations = &g_AnimationManager.m_arrGlobalAnimations[0];

		GlobalAnimationHeader& rsubGAH00 = parrGlobalAnimations[ lmg.m_nGlobalID[0x00] ]; 
		GlobalAnimationHeader& rsubGAH01 = parrGlobalAnimations[ lmg.m_nGlobalID[0x01] ]; 
		GlobalAnimationHeader& rsubGAH02 = parrGlobalAnimations[ lmg.m_nGlobalID[0x02] ]; 
		GlobalAnimationHeader& rsubGAH03 = parrGlobalAnimations[ lmg.m_nGlobalID[0x03] ]; 
		GlobalAnimationHeader& rsubGAH04 = parrGlobalAnimations[ lmg.m_nGlobalID[0x04] ]; 
		GlobalAnimationHeader& rsubGAH05 = parrGlobalAnimations[ lmg.m_nGlobalID[0x05] ]; 
		GlobalAnimationHeader& rsubGAH06 = parrGlobalAnimations[ lmg.m_nGlobalID[0x06] ]; 
		GlobalAnimationHeader& rsubGAH07 = parrGlobalAnimations[ lmg.m_nGlobalID[0x07] ]; 

		GlobalAnimationHeader& rsubGAH08 = parrGlobalAnimations[ lmg.m_nGlobalID[0x08] ]; 
		GlobalAnimationHeader& rsubGAH09 = parrGlobalAnimations[ lmg.m_nGlobalID[0x09] ]; 
		GlobalAnimationHeader& rsubGAH0a = parrGlobalAnimations[ lmg.m_nGlobalID[0x0a] ]; 
		GlobalAnimationHeader& rsubGAH0b = parrGlobalAnimations[ lmg.m_nGlobalID[0x0b] ]; 
		GlobalAnimationHeader& rsubGAH0c = parrGlobalAnimations[ lmg.m_nGlobalID[0x0c] ]; 
		GlobalAnimationHeader& rsubGAH0d = parrGlobalAnimations[ lmg.m_nGlobalID[0x0d] ]; 
		GlobalAnimationHeader& rsubGAH0e = parrGlobalAnimations[ lmg.m_nGlobalID[0x0e] ]; 
		GlobalAnimationHeader& rsubGAH0f = parrGlobalAnimations[ lmg.m_nGlobalID[0x0f] ]; 

		GlobalAnimationHeader& rsubGAH10 = parrGlobalAnimations[ lmg.m_nGlobalID[0x10] ]; 
		GlobalAnimationHeader& rsubGAH11 = parrGlobalAnimations[ lmg.m_nGlobalID[0x11] ]; 
		GlobalAnimationHeader& rsubGAH12 = parrGlobalAnimations[ lmg.m_nGlobalID[0x12] ]; 
		GlobalAnimationHeader& rsubGAH13 = parrGlobalAnimations[ lmg.m_nGlobalID[0x13] ]; 
		GlobalAnimationHeader& rsubGAH14 = parrGlobalAnimations[ lmg.m_nGlobalID[0x14] ]; 
		GlobalAnimationHeader& rsubGAH15 = parrGlobalAnimations[ lmg.m_nGlobalID[0x15] ]; 
		GlobalAnimationHeader& rsubGAH16 = parrGlobalAnimations[ lmg.m_nGlobalID[0x16] ]; 
		GlobalAnimationHeader& rsubGAH17 = parrGlobalAnimations[ lmg.m_nGlobalID[0x17] ]; 

		//-----------------------------------------------------------------------------------------
		//-------                 idle rotation                                             -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"IROT" )
		{
			f32	t = lmg.m_BlendSpace.m_turn;
			if (t<0)
			{
				lmg.m_fBlendWeight[1]=1+t;	//middle
				lmg.m_fBlendWeight[2]=-t;		//left
			}
			else
			{
				lmg.m_fBlendWeight[0]=t;		//right
				lmg.m_fBlendWeight[1]=1-t;	//middle
			}			
			return;
		}


		//-----------------------------------------------------------------------------------------
		//-------                 idle rotation                                             -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"TSTP" )
		{
			f32	t = lmg.m_BlendSpace.m_turn;
			if (t<0)
			{
				lmg.m_fBlendWeight[1]=1+t;	//middle
				lmg.m_fBlendWeight[2]=-t;		//left
			}
			else
			{
				lmg.m_fBlendWeight[0]=t;    //right
				lmg.m_fBlendWeight[1]=1-t;  //middle
			}			

			//f32 fColor[4] = {1,1,0,1};
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t00: %f", lmg.m_fBlendWeight[0] ); g_YLine+=0x0d;
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t01: %f", lmg.m_fBlendWeight[1] ); g_YLine+=0x0d;
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t02: %f", lmg.m_fBlendWeight[2] ); g_YLine+=0x0d;

			return;
		}

		//-----------------------------------------------------------------------------------------
		//-------                 idle rotation                                             -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"M2I1" )
		{
			lmg.m_fBlendWeight[0]=1.0f;
			return;
		}

		//-----------------------------------------------------------------------------------------
		//-------                            Idle2Move with 1 speed                         -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"I2M1" )
		{
			f32 length = lmg.m_BlendSpace.m_strafe.GetLength();
			/*
			if (length<0.5f)
			{
			f32 fColor[4] = {1,1,0,1};
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 4.3f, fColor, false,"Serious Error: Strafe vector has zero-length" ); g_YLine+=0x30;
			}
			else
			{
			f32 fColor[4] = {1,1,0,1};
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"FixedStrafe: %f %f   Length: %f ",x,y,length); g_YLine+=0x10;
			}
			*/
			Vec2 mdir = lmg.m_BlendSpace.m_strafe.GetNormalizedSafe(); //forward-right
			IdleStepWeights6 I2M = pAnimationSet->GetIdle2MoveWeights6(mdir); 
			lmg.m_fBlendWeight[0]=I2M.fr;
			lmg.m_fBlendWeight[1]=I2M.rr;
			lmg.m_fBlendWeight[2]=I2M.br;
			lmg.m_fBlendWeight[3]=I2M.fl;
			lmg.m_fBlendWeight[4]=I2M.ll;
			lmg.m_fBlendWeight[5]=I2M.bl;
			return;
		}




		//-----------------------------------------------------------------------------------------
		//-------                            Idle2Move with 2 speed                         -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"I2M2" )
		{

			if (Console::GetInst().ca_DrawIdle2MoveDir)
			{
				f32 length = lmg.m_BlendSpace.m_strafe.GetLength();
				if (length<0.5f)
				{
					f32 fColor[4] = {1,0,0,1};
					g_pIRenderer->Draw2dLabel( 1,g_YLine, 3.3f, fColor, false,"Serious Error: Strafe vector has zero-length" );	g_YLine+=0x30;
				}
				else
				{
					f32 fColor[4] = {1,1,0,1};
					g_pIRenderer->Draw2dLabel( 1,g_YLine, 3.3f, fColor, false,"FixedStrafe: %f %f   Length: %f ",lmg.m_BlendSpace.m_strafe.x,lmg.m_BlendSpace.m_strafe.y,length);	g_YLine+=0x10;
				}

				if (lmg.m_BlendSpace.m_updates==0)
					lmg.m_BlendSpace.m_AnimChar = QuatT(m_pInstance->m_RenderCharLocation);
				lmg.m_BlendSpace.m_updates=1;

				f32 radiant	= -atan2f(lmg.m_BlendSpace.m_strafe.x,lmg.m_BlendSpace.m_strafe.y);
				QuatT BodyLocation = lmg.m_BlendSpace.m_AnimChar*Quat::CreateRotationZ(radiant); 	
				BodyLocation.t += Vec3(0,0,0.41f);
				m_pSkeletonPose->DrawArrow(BodyLocation,19.0f,RGBA8(0xff,0x00,0x00,0x00) );
			}

			Vec2 mdir = lmg.m_BlendSpace.m_strafe.GetNormalizedSafe(Vec2(0,1)); //forward-right
			IdleStepWeights6 I2M = pAnimationSet->GetIdle2MoveWeights6(mdir); 

			f32 t0=1.0f-(lmg.m_BlendSpace.m_speed+1)*0.5f;
			f32 t1=(lmg.m_BlendSpace.m_speed+1)*0.5f;

			// Force fastest speed
			t1=1; t0=0;

			lmg.m_fBlendWeight[ 0]=I2M.fr*t0;
			lmg.m_fBlendWeight[ 1]=I2M.rr*t0;
			lmg.m_fBlendWeight[ 2]=I2M.br*t0;
			lmg.m_fBlendWeight[ 3]=I2M.fl*t0;
			lmg.m_fBlendWeight[ 4]=I2M.ll*t0;
			lmg.m_fBlendWeight[ 5]=I2M.bl*t0;

			lmg.m_fBlendWeight[ 6]=I2M.fr*t1;
			lmg.m_fBlendWeight[ 7]=I2M.rr*t1;
			lmg.m_fBlendWeight[ 8]=I2M.br*t1;
			lmg.m_fBlendWeight[ 9]=I2M.fl*t1;
			lmg.m_fBlendWeight[10]=I2M.ll*t1;
			lmg.m_fBlendWeight[11]=I2M.bl*t1;

			//f32 fColor[4] = {1,0,0,1};
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"        FL: %f    FR: %f",I2M.fl, I2M.fr ); g_YLine+=0x0d;
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"    LL: %f               RR: %f",I2M.ll, I2M.rr ); g_YLine+=0x0d;
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"        BL: %f    BR: %f",I2M.bl, I2M.br ); g_YLine+=0x0d;
			return;
		}


		//-----------------------------------------------------------------------------------------
		//-------                             Idle Step speed                               -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"ISTP" )
		{
			f32 length = lmg.m_BlendSpace.m_strafe.GetLength();
			if (length>1.0f) length=1.0f;

			f32 fColor[4] = {1,1,0,1};

			Vec2 sFR = Vec2(rsubGAH00.m_vVelocity);
			Vec2 sRR = Vec2(rsubGAH01.m_vVelocity);
			Vec2 sBR = Vec2(rsubGAH02.m_vVelocity);
			Vec2 sFL = Vec2(rsubGAH03.m_vVelocity);
			Vec2 sRL = Vec2(rsubGAH04.m_vVelocity);
			Vec2 sBL = Vec2(rsubGAH05.m_vVelocity);
			IdleStepWeights6 blend = pAnimationSet->GetIdleStepWeights6(sFR,sRR,sBR, sFL,sRL,sBL, lmg.m_BlendSpace.m_strafe.GetNormalizedSafe() );

			lmg.m_fBlendWeight[0]=blend.fr*length;	
			lmg.m_fBlendWeight[1]=blend.rr*length;	
			lmg.m_fBlendWeight[2]=blend.br*length;	
			lmg.m_fBlendWeight[3]=blend.fl*length;	
			lmg.m_fBlendWeight[4]=blend.ll*length;	
			lmg.m_fBlendWeight[5]=blend.bl*length;	
			lmg.m_fBlendWeight[6]=1.0f-length;;	
			/*
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"        FL: %f    FR: %f",blend.fl, blend.fr ); g_YLine+=0x0d;
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"    LL: %f               RR: %f",blend.ll, blend.rr ); g_YLine+=0x0d;
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"        BL: %f    BR: %f",blend.bl, blend.br ); g_YLine+=0x0d;
			*/
			return;
		}


		//-----------------------------------------------------------------------------------------
		//-------                            Strafe with 4 assets                          -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"TRN1" )
		{
			f32	turn = lmg.m_BlendSpace.m_turn;
			f32	t = lmg.m_BlendSpace.m_turn*1;
			if (t<0)
			{
				if (t<-0.5f)
				{
					t=t+0.5f;
					lmg.m_fBlendWeight[4]=-t*2;		//left 90
					lmg.m_fBlendWeight[3]=1.0f-lmg.m_fBlendWeight[4];	//middle
				}
				else
				{
					lmg.m_fBlendWeight[3]=-t*2;		//left 90
					lmg.m_fBlendWeight[2]=1.0f-lmg.m_fBlendWeight[3];	//middle
				}
			}
			else
			{
				if (t<0.5f)
				{
					lmg.m_fBlendWeight[1]=t*2;    //right 90
					lmg.m_fBlendWeight[2]=1-t*2;  //middle
				}
				else
				{
					t=t-0.5f;
					lmg.m_fBlendWeight[0]=t*2;    //right 180
					lmg.m_fBlendWeight[1]=1-t*2;  //left 90
				}
			}			

			f32 fColor[4] = {1,1,0,1};
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"r180: %f", lmg.m_fBlendWeight[0] ); g_YLine+=0x0d;
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"r090: %f", lmg.m_fBlendWeight[1] ); g_YLine+=0x0d;
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"f00:  %f", lmg.m_fBlendWeight[2] ); g_YLine+=0x0d;
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"l090: %f", lmg.m_fBlendWeight[3] ); g_YLine+=0x0d;
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"l180: %f", lmg.m_fBlendWeight[4] ); g_YLine+=0x0d;
			return;
		}







		//-----------------------------------------------------------------------------------------
		//-------              Strafe with 4-direction and  speed assets                    -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"STF1" )
		{
			Vec2 sF = Vec2(rsubGAH00.m_vVelocity);
			Vec2 sL = Vec2(rsubGAH01.m_vVelocity);
			Vec2 sB = Vec2(rsubGAH02.m_vVelocity);
			Vec2 sR = Vec2(rsubGAH03.m_vVelocity);
			StrafeWeights4 blend = pAnimationSet->GetStrafingWeights4(sF,sL,sB,sR, lmg.m_BlendSpace.m_strafe );

			f32 t = lmg.m_BlendSpace.m_strafe.GetLength();

			//	t -= 0.5f;
			//	t = 0.5f+t/(0.5f+2.0f*t*t);


			lmg.m_fBlendWeight[0]=blend.f*t + 0.25f*(1.0f-t);
			lmg.m_fBlendWeight[1]=blend.l*t + 0.25f*(1.0f-t);	
			lmg.m_fBlendWeight[2]=blend.b*t + 0.25f*(1.0f-t);
			lmg.m_fBlendWeight[3]=blend.r*t + 0.25f*(1.0f-t);	


			/*
			Vec2 F	=	Vec2( 0,+1); //0
			Vec2 R	=	Vec2(+1, 0); //1
			Vec2 L	=	Vec2(-1, 0); //2
			Vec2 B	=	Vec2( 0,-1); //3
			Vec2 movedir = F*blend.f + R*blend.r + L*blend.l + B*blend.b;

			f32 length = lmg.m_BlendSpace.m_strafe.GetLength();
			f32 lmd = movedir.GetLength();
			if (lmd>length)	{	movedir.NormalizeSafe(); movedir*=length;	}
			Vec4 bw	=	WeightComputation4(movedir, F,L,B,R);

			lmg.m_fBlendWeight[0]=bw.x;
			lmg.m_fBlendWeight[1]=bw.y;	
			lmg.m_fBlendWeight[2]=bw.z;
			lmg.m_fBlendWeight[3]=bw.w;	
			*/

			//	f32 fColor[4] = {1,1,0,1};
			//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"f0 %f",lmg.m_fBlendWeight[0]); g_YLine+=0x10;
			//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"r1 %f",lmg.m_fBlendWeight[1]); g_YLine+=0x10;
			//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"l2 %f",lmg.m_fBlendWeight[2]); g_YLine+=0x10;
			//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"b3 %f",lmg.m_fBlendWeight[3]); g_YLine+=0x10;
			return;
		}


		//-----------------------------------------------------------------------------------------
		//-------                            Strafe with 8 assets                           -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"STF2" )
		{
			uint32 poffset=0;
			BlendWeights_STF2 stf2;	WeightComputation_STF2( rGlobalAnimHeader, lmg,poffset, stf2  );

			lmg.m_fBlendWeight[0+0]=stf2.ff;	
			lmg.m_fBlendWeight[1+0]=stf2.fl;	
			lmg.m_fBlendWeight[2+0]=stf2.fb;
			lmg.m_fBlendWeight[3+0]=stf2.fr;	

			lmg.m_fBlendWeight[0+4]=stf2.sf;	
			lmg.m_fBlendWeight[1+4]=stf2.sl;	
			lmg.m_fBlendWeight[2+4]=stf2.sb;
			lmg.m_fBlendWeight[3+4]=stf2.sr;	
			return;
		}




		//-----------------------------------------------------------------------------------------
		//-------                             Strafe with 24 assets                        -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"SUD2" )
		{
			uint32 uoffset=0;
			BlendWeights_STF2 ustf2;	WeightComputation_STF2( rGlobalAnimHeader, lmg,uoffset, ustf2  );
			uint32 poffset=8;
			BlendWeights_STF2 pstf2;	WeightComputation_STF2( rGlobalAnimHeader, lmg,poffset, pstf2  );
			uint32 doffset=16;
			BlendWeights_STF2 dstf2;	WeightComputation_STF2( rGlobalAnimHeader, lmg,doffset, dstf2  );

			f32 uh=0,fw=0,dh=0;
			f32	q = lmg.m_BlendSpace.m_slope;
			if (q>0) { uh=q;	fw=1-q;	}
			else { fw=1+q;	dh=1-(q+1);	}

			//	f32 fColor[4] = {1,1,0,1};
			//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"strafe uphill/downhill: uh:%f fw:%f dh:%f  = %f ",uh,fw,dh, uh+fw+dh ); g_YLine+=0x10;

			lmg.m_fBlendWeight[0+0x00]=ustf2.ff*uh;	
			lmg.m_fBlendWeight[1+0x00]=ustf2.fl*uh;	
			lmg.m_fBlendWeight[2+0x00]=ustf2.fb*uh;
			lmg.m_fBlendWeight[3+0x00]=ustf2.fr*uh;	
			lmg.m_fBlendWeight[4+0x00]=ustf2.sf*uh;	
			lmg.m_fBlendWeight[5+0x00]=ustf2.sl*uh;	
			lmg.m_fBlendWeight[6+0x00]=ustf2.sb*uh;
			lmg.m_fBlendWeight[7+0x00]=ustf2.sr*uh;	

			lmg.m_fBlendWeight[0+0x08]=pstf2.ff*fw;	
			lmg.m_fBlendWeight[1+0x08]=pstf2.fl*fw;	
			lmg.m_fBlendWeight[2+0x08]=pstf2.fb*fw;
			lmg.m_fBlendWeight[3+0x08]=pstf2.fr*fw;	
			lmg.m_fBlendWeight[4+0x08]=pstf2.sf*fw;	
			lmg.m_fBlendWeight[5+0x08]=pstf2.sl*fw;	
			lmg.m_fBlendWeight[6+0x08]=pstf2.sb*fw;
			lmg.m_fBlendWeight[7+0x08]=pstf2.sr*fw;	

			lmg.m_fBlendWeight[0+0x10]=dstf2.ff*dh;	
			lmg.m_fBlendWeight[1+0x10]=dstf2.fl*dh;	
			lmg.m_fBlendWeight[2+0x10]=dstf2.fb*dh;
			lmg.m_fBlendWeight[3+0x10]=dstf2.fr*dh;	
			lmg.m_fBlendWeight[4+0x10]=dstf2.sf*dh;	
			lmg.m_fBlendWeight[5+0x10]=dstf2.sl*dh;	
			lmg.m_fBlendWeight[6+0x10]=dstf2.sb*dh;
			lmg.m_fBlendWeight[7+0x10]=dstf2.sr*dh;	

			return;
		}



		//-----------------------------------------------------------------------------------------
		//-------                     Strafe with 6-direction assets                        -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"S__1" )
		{
			f32 fColor[4] = {1,1,0,1};
			Vec2 sF		= Vec2(rsubGAH00.m_vVelocity);
			Vec2 sFL	= Vec2(rsubGAH01.m_vVelocity);
			Vec2 sL		= Vec2(rsubGAH02.m_vVelocity);
			Vec2 sB		= Vec2(rsubGAH03.m_vVelocity);
			Vec2 sBR	= Vec2(rsubGAH04.m_vVelocity);
			Vec2 sR		= Vec2(rsubGAH05.m_vVelocity);
			StrafeWeights6 blend = pAnimationSet->GetStrafingWeights6(sF,sFL,sL, sB,sBR,sR, lmg.m_BlendSpace.m_strafe );

			f32 t = lmg.m_BlendSpace.m_strafe.GetLength();
			f32 p =	(1.0f/6.0f);
			blend.f		=	blend.f*t		+ p*(1.0f-t);
			blend.fl	=	blend.fl*t	+ p*(1.0f-t);
			blend.l		=	blend.l*t		+ p*(1.0f-t);	
			blend.b		=	blend.b*t		+ p*(1.0f-t);
			blend.br	=	blend.br*t	+ p*(1.0f-t);
			blend.r		=	blend.r*t		+ p*(1.0f-t);	

			lmg.m_fBlendWeight[0] = blend.f;	
			lmg.m_fBlendWeight[1] = blend.fl;	
			lmg.m_fBlendWeight[2] = blend.l;	
			lmg.m_fBlendWeight[3] = blend.b;	
			lmg.m_fBlendWeight[4] = blend.br;	
			lmg.m_fBlendWeight[5] = blend.r;	

			//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"                 F: %f",blend.f );	g_YLine+=0x10;
			//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"         FL: %f",blend.fl );	g_YLine+=0x10;
			//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"L: %f                              R: %f",blend.l,blend.r );	g_YLine+=0x10;
			//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"                         BR: %f",blend.br );	g_YLine+=0x10;
			//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"                 B: %f",blend.b );	g_YLine+=0x10;

			/*
			Vec2 F	=	Vec2( 0,+1); //0
			Vec2 L	=	Vec2(-1, 0); //2
			Vec2 B	=	Vec2( 0,-1); //3
			Vec2 R	=	Vec2(+1, 0); //1
			Vec2 FL	=	(F+L)*0.5f; //0
			Vec2 BR	=	(B+R)*0.5f; //0
			Vec2 movedir = F*blend.f + FL*blend.fl + L*blend.l + B*blend.b + BR*blend.br + R*blend.r;
			f32 length = Vec2(lmg.m_BlendSpace.m_strafe.x,lmg.m_BlendSpace.m_strafe.y).GetLength();
			f32 lmd = movedir.GetLength();
			if (lmd>length)	{	movedir.NormalizeSafe(Vec2(0,1));	movedir*=length;	}

			StrafeWeights6 Weight6 = WeightCalculation6( movedir,  F,FL,L,  B,BR,R);
			lmg.m_fBlendWeight[0]=Weight6.f;	
			lmg.m_fBlendWeight[1]=Weight6.fl;	
			lmg.m_fBlendWeight[2]=Weight6.l;	
			lmg.m_fBlendWeight[3]=Weight6.b;	
			lmg.m_fBlendWeight[4]=Weight6.br;	
			lmg.m_fBlendWeight[5]=Weight6.r;	
			*/
			//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"Strafe Vector: %f %f",movedir.x,movedir.y );	g_YLine+=0x10;
			//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"                 F: %f",Weight6.f );	g_YLine+=0x10;
			//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"         FL: %f",Weight6.fl );	g_YLine+=0x10;
			//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"L: %f                              R: %f",Weight6.l,Weight6.r );	g_YLine+=0x10;
			//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"                         BR: %f",Weight6.br );	g_YLine+=0x10;
			//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"                 B: %f",Weight6.b );	g_YLine+=0x10;

			return;
		}


		//-----------------------------------------------------------------------------------------
		//-------               Strafe with 6-direction and 2 speeds assets                 -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"S__2" )
		{
			uint32 poffset=0;
			BlendWeights_S2 s2;	WeightComputation_S2( rGlobalAnimHeader, lmg,poffset, s2  );

			lmg.m_fBlendWeight[0+0]=s2.ff;	
			lmg.m_fBlendWeight[1+0]=s2.ffl;	
			lmg.m_fBlendWeight[2+0]=s2.fl;	
			lmg.m_fBlendWeight[3+0]=s2.fb;
			lmg.m_fBlendWeight[4+0]=s2.fbr;
			lmg.m_fBlendWeight[5+0]=s2.fr;	

			lmg.m_fBlendWeight[0+6]=s2.sf;	
			lmg.m_fBlendWeight[1+6]=s2.sfl;	
			lmg.m_fBlendWeight[2+6]=s2.sl;	
			lmg.m_fBlendWeight[3+6]=s2.sb;
			lmg.m_fBlendWeight[4+6]=s2.sbr;
			lmg.m_fBlendWeight[5+6]=s2.sr;	
			return;
		}


		//-----------------------------------------------------------------------------------------------------
		//---   Strafe with 6-direction / 3 forward speeds assets / 2 direction speeds / left-right turns   ---
		//-----------------------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"ST_2" )
		{
			uint32 numAssets = rGlobalAnimHeader.m_arrBSAnimations.size();

			uint32 poffset=0;
			BlendWeights_ST2 st2;	WeightComputation_ST2( rGlobalAnimHeader, lmg,poffset, st2  );

			lmg.m_fBlendWeight[poffset+0x00]=st2.ff;	//fast forward speed
			lmg.m_fBlendWeight[poffset+0x01]=st2.ffl;	
			lmg.m_fBlendWeight[poffset+0x02]=st2.fl;	
			lmg.m_fBlendWeight[poffset+0x03]=st2.fb;	
			lmg.m_fBlendWeight[poffset+0x04]=st2.fbr;	
			lmg.m_fBlendWeight[poffset+0x05]=st2.fr;	

			lmg.m_fBlendWeight[poffset+0x06]=st2.mf;		//middle forward speed	

			lmg.m_fBlendWeight[poffset+0x07]=st2.sf;	//slow forward speed
			lmg.m_fBlendWeight[poffset+0x08]=st2.sfl;	
			lmg.m_fBlendWeight[poffset+0x09]=st2.sl;	
			lmg.m_fBlendWeight[poffset+0x0a]=st2.sb;	
			lmg.m_fBlendWeight[poffset+0x0b]=st2.sbr;	
			lmg.m_fBlendWeight[poffset+0x0c]=st2.sr;	

			lmg.m_fBlendWeight[poffset+0x0d]=st2.fftl;	//left
			lmg.m_fBlendWeight[poffset+0x0e]=st2.fftr;	//right
			lmg.m_fBlendWeight[poffset+0x0f]=st2.sftl;	//left
			lmg.m_fBlendWeight[poffset+0x10]=st2.sftr;	//right
			return;
		}



		//--------------------------------------------------------------------------
		//---   Strafe with 6-direction / 2 direction speeds / uphill-downhill   ---
		//--------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"S_H2" )
		{
			uint32 numAssets = rGlobalAnimHeader.m_arrBSAnimations.size();
			f32 uh=0,fw=0,dh=0;
			f32	q = lmg.m_BlendSpace.m_slope;
			if (q>0) { uh=q;	fw=1-q;	}
			else { fw=1+q;	dh=1-(q+1);	}

			uint32 uoffset=0;
			BlendWeights_STF2 stu2;	WeightComputation_STF2( rGlobalAnimHeader, lmg,uoffset, stu2  );
			lmg.m_fBlendWeight[uoffset+0x00]=stu2.ff*uh;	
			lmg.m_fBlendWeight[uoffset+0x01]=stu2.fl*uh;	
			lmg.m_fBlendWeight[uoffset+0x02]=stu2.fb*uh;	
			lmg.m_fBlendWeight[uoffset+0x03]=stu2.fr*uh;	
			lmg.m_fBlendWeight[uoffset+0x04]=stu2.sf*uh;	
			lmg.m_fBlendWeight[uoffset+0x05]=stu2.sl*uh;	
			lmg.m_fBlendWeight[uoffset+0x06]=stu2.sb*uh;	
			lmg.m_fBlendWeight[uoffset+0x07]=stu2.sr*uh;	


			uint32 poffset=8;
			BlendWeights_S2 s2;	WeightComputation_S2( rGlobalAnimHeader, lmg,poffset, s2 );
			lmg.m_fBlendWeight[poffset+0x00]=s2.ff*fw;	//fast forward speed
			lmg.m_fBlendWeight[poffset+0x01]=s2.ffl*fw;	
			lmg.m_fBlendWeight[poffset+0x02]=s2.fl*fw;	
			lmg.m_fBlendWeight[poffset+0x03]=s2.fb*fw;	
			lmg.m_fBlendWeight[poffset+0x04]=s2.fbr*fw;	
			lmg.m_fBlendWeight[poffset+0x05]=s2.fr*fw;	

			lmg.m_fBlendWeight[poffset+0x06]=s2.sf*fw;	//slow forward speed
			lmg.m_fBlendWeight[poffset+0x07]=s2.sfl*fw;	
			lmg.m_fBlendWeight[poffset+0x08]=s2.sl*fw;	
			lmg.m_fBlendWeight[poffset+0x09]=s2.sb*fw;	
			lmg.m_fBlendWeight[poffset+0x0a]=s2.sbr*fw;	
			lmg.m_fBlendWeight[poffset+0x0b]=s2.sr*fw;	



			uint32 doffset=0x08+0x0c;
			BlendWeights_STF2 std2;	WeightComputation_STF2( rGlobalAnimHeader, lmg,doffset, std2  );
			lmg.m_fBlendWeight[doffset+0x00]=std2.ff*dh;	
			lmg.m_fBlendWeight[doffset+0x01]=std2.fl*dh;	
			lmg.m_fBlendWeight[doffset+0x02]=std2.fb*dh;	
			lmg.m_fBlendWeight[doffset+0x03]=std2.fr*dh;	
			lmg.m_fBlendWeight[doffset+0x04]=std2.sf*dh;	
			lmg.m_fBlendWeight[doffset+0x05]=std2.sl*dh;	
			lmg.m_fBlendWeight[doffset+0x06]=std2.sb*dh;	
			lmg.m_fBlendWeight[doffset+0x07]=std2.sr*dh;	
			return;
		}



		//-----------------------------------------------------------------------------------------------------------------------
		//---   Strafe with 6-direction / 3 forward speeds assets / 2 direction speeds / left-right turns / uphill-downhill   ---
		//-----------------------------------------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"STH2" )
		{
			uint32 numAssets = rGlobalAnimHeader.m_arrBSAnimations.size();
			f32 uh=0,fw=0,dh=0;
			f32	q = lmg.m_BlendSpace.m_slope;
			if (q>0) { uh=q;	fw=1-q;	}
			else { fw=1+q;	dh=1-(q+1);	}

			uint32 uoffset=0;
			BlendWeights_STF2 stu2;	WeightComputation_STF2( rGlobalAnimHeader, lmg,uoffset, stu2  );
			lmg.m_fBlendWeight[uoffset+0x00]=stu2.ff*uh;	
			lmg.m_fBlendWeight[uoffset+0x01]=stu2.fl*uh;	
			lmg.m_fBlendWeight[uoffset+0x02]=stu2.fb*uh;	
			lmg.m_fBlendWeight[uoffset+0x03]=stu2.fr*uh;	

			lmg.m_fBlendWeight[uoffset+0x04]=stu2.sf*uh;	
			lmg.m_fBlendWeight[uoffset+0x05]=stu2.sl*uh;	
			lmg.m_fBlendWeight[uoffset+0x06]=stu2.sb*uh;	
			lmg.m_fBlendWeight[uoffset+0x07]=stu2.sr*uh;	



			uint32 poffset=8;
			BlendWeights_ST2 st2;	WeightComputation_ST2( rGlobalAnimHeader, lmg,poffset, st2  );

			lmg.m_fBlendWeight[poffset+0x00]=st2.ff*fw;	//fast forward speed
			lmg.m_fBlendWeight[poffset+0x01]=st2.ffl*fw;	
			lmg.m_fBlendWeight[poffset+0x02]=st2.fl*fw;	
			lmg.m_fBlendWeight[poffset+0x03]=st2.fb*fw;	
			lmg.m_fBlendWeight[poffset+0x04]=st2.fbr*fw;	
			lmg.m_fBlendWeight[poffset+0x05]=st2.fr*fw;	

			lmg.m_fBlendWeight[poffset+0x06]=st2.mf*fw;		//middle forward speed	

			lmg.m_fBlendWeight[poffset+0x07]=st2.sf*fw;	//slow forward speed
			lmg.m_fBlendWeight[poffset+0x08]=st2.sfl*fw;	
			lmg.m_fBlendWeight[poffset+0x09]=st2.sl*fw;	
			lmg.m_fBlendWeight[poffset+0x0a]=st2.sb*fw;	
			lmg.m_fBlendWeight[poffset+0x0b]=st2.sbr*fw;	
			lmg.m_fBlendWeight[poffset+0x0c]=st2.sr*fw;	

			lmg.m_fBlendWeight[poffset+0x0d]=st2.fftl*fw;	//left
			lmg.m_fBlendWeight[poffset+0x0e]=st2.fftr*fw;	//right
			lmg.m_fBlendWeight[poffset+0x0f]=st2.sftl*fw;	//left
			lmg.m_fBlendWeight[poffset+0x10]=st2.sftr*fw;	//right





			uint32 doffset=0x08+0x11;
			BlendWeights_STF2 std2;	WeightComputation_STF2( rGlobalAnimHeader, lmg,doffset, std2  );
			lmg.m_fBlendWeight[doffset+0x00]=std2.ff*dh;	
			lmg.m_fBlendWeight[doffset+0x01]=std2.fl*dh;	
			lmg.m_fBlendWeight[doffset+0x02]=std2.fb*dh;	
			lmg.m_fBlendWeight[doffset+0x03]=std2.fr*dh;	

			lmg.m_fBlendWeight[doffset+0x04]=std2.sf*dh;	
			lmg.m_fBlendWeight[doffset+0x05]=std2.sl*dh;	
			lmg.m_fBlendWeight[doffset+0x06]=std2.sb*dh;	
			lmg.m_fBlendWeight[doffset+0x07]=std2.sr*dh;	
			return;
		}



		//-----------------------------------------------------------------------------------------------------------------------
		//---   Strafe with 4-direction / 3 forward speeds assets / 2 direction speeds / left-right turns / uphill-downhill   ---
		//-----------------------------------------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"STHX" )
		{
			uint32 numAssets = rGlobalAnimHeader.m_arrBSAnimations.size();
			f32 uh=0,fw=0,dh=0;
			f32	q = lmg.m_BlendSpace.m_slope;
			if (q>0) { uh=q;	fw=1-q;	}
			else { fw=1+q;	dh=1-(q+1);	}

			uint32 uoffset=0;
			BlendWeights_STF2 stu2;	WeightComputation_STF2( rGlobalAnimHeader, lmg,uoffset, stu2  );
			lmg.m_fBlendWeight[uoffset+0x00]=stu2.ff*uh;	
			lmg.m_fBlendWeight[uoffset+0x01]=stu2.fl*uh;	
			lmg.m_fBlendWeight[uoffset+0x02]=stu2.fb*uh;	
			lmg.m_fBlendWeight[uoffset+0x03]=stu2.fr*uh;	

			lmg.m_fBlendWeight[uoffset+0x04]=stu2.sf*uh;	
			lmg.m_fBlendWeight[uoffset+0x05]=stu2.sl*uh;	
			lmg.m_fBlendWeight[uoffset+0x06]=stu2.sb*uh;	
			lmg.m_fBlendWeight[uoffset+0x07]=stu2.sr*uh;	



			uint32 poffset=8;
			BlendWeights_STX stx;	WeightComputation_STX( rGlobalAnimHeader, lmg,poffset, stx  );

			lmg.m_fBlendWeight[poffset+0x00]=stx.ff*fw;	//fast forward speed
			lmg.m_fBlendWeight[poffset+0x01]=stx.fl*fw;	
			lmg.m_fBlendWeight[poffset+0x02]=stx.fb*fw;	
			lmg.m_fBlendWeight[poffset+0x03]=stx.fr*fw;	

			lmg.m_fBlendWeight[poffset+0x04]=stx.mf*fw;		//middle forward speed	

			lmg.m_fBlendWeight[poffset+0x05]=stx.sf*fw;	//slow forward speed
			lmg.m_fBlendWeight[poffset+0x06]=stx.sl*fw;	
			lmg.m_fBlendWeight[poffset+0x07]=stx.sb*fw;	
			lmg.m_fBlendWeight[poffset+0x08]=stx.sr*fw;	

			lmg.m_fBlendWeight[poffset+0x09]=stx.fftl*fw;	//left
			lmg.m_fBlendWeight[poffset+0x0a]=stx.fftr*fw;	//right
			lmg.m_fBlendWeight[poffset+0x0b]=stx.sftl*fw;	//left
			lmg.m_fBlendWeight[poffset+0x0c]=stx.sftr*fw;	//right



			uint32 doffset=0x08+0x0d;
			BlendWeights_STF2 std2;	WeightComputation_STF2( rGlobalAnimHeader, lmg,doffset, std2  );
			lmg.m_fBlendWeight[doffset+0x00]=std2.ff*dh;	
			lmg.m_fBlendWeight[doffset+0x01]=std2.fl*dh;	
			lmg.m_fBlendWeight[doffset+0x02]=std2.fb*dh;	
			lmg.m_fBlendWeight[doffset+0x03]=std2.fr*dh;	

			lmg.m_fBlendWeight[doffset+0x04]=std2.sf*dh;	
			lmg.m_fBlendWeight[doffset+0x05]=std2.sl*dh;	
			lmg.m_fBlendWeight[doffset+0x06]=std2.sb*dh;	
			lmg.m_fBlendWeight[doffset+0x07]=std2.sr*dh;	
			return;
		}







		//-----------------------------------------------------------------------------------------
		//-------                 most simple LMG with two speed                            -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG== *(uint32*)"WFW2" )
		{
			f32	SpeedWeight = (lmg.m_BlendSpace.m_speed+1)*0.5f;
			lmg.m_fBlendWeight[0]=1-SpeedWeight;
			lmg.m_fBlendWeight[1]=SpeedWeight;
			return;
		}

		//-----------------------------------------------------------------------------------------
		//-------                 simple LMG with three speed                              -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG== *(uint32*)"RFW3" )
		{
			f32	t = lmg.m_BlendSpace.m_speed;
			if (t<0)
			{
				lmg.m_fBlendWeight[0]=1-(t+1);
				lmg.m_fBlendWeight[1]=t+1;
			}
			else
			{
				lmg.m_fBlendWeight[0]=0;
				lmg.m_fBlendWeight[1]=(1-t);
			}
			return;
		}


		//-----------------------------------------------------------------------------------------
		//-------               a run Forward-Left-Right LMG with one speeds                -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"FLR1" )
		{
			f32	t = lmg.m_BlendSpace.m_turn;
			if (t<0)
			{
				lmg.m_fBlendWeight[0]=-t;   //left
				lmg.m_fBlendWeight[1]=1+t;  //middle
			}
			else
			{
				lmg.m_fBlendWeight[1]=1-t;	//middle
				lmg.m_fBlendWeight[2]=t;		//right
			}			

			//f32 fColor[4] = {1,1,0,1};
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t0: %f   t1: %f  t2: %f", lmg.m_fBlendWeight[0],	lmg.m_fBlendWeight[1], lmg.m_fBlendWeight[2] ); g_YLine+=0x10;
			return;
		}


		//-----------------------------------------------------------------------------------------
		//-------               a run Forward-Left-Right LMG with two speeds                -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"FLR2" )
		{
			Vec2 FL=Vec2(rGlobalAnimHeader.m_arrBSAnimations[0].m_Position);	//fast	
			Vec2 FF=Vec2(rGlobalAnimHeader.m_arrBSAnimations[1].m_Position);	//fast	
			Vec2 FR=Vec2(rGlobalAnimHeader.m_arrBSAnimations[2].m_Position);	//fast	
			Vec2 SL=Vec2(rGlobalAnimHeader.m_arrBSAnimations[3].m_Position);	//slow	
			Vec2 SF=Vec2(rGlobalAnimHeader.m_arrBSAnimations[4].m_Position);	//slow	
			Vec2 SR=Vec2(rGlobalAnimHeader.m_arrBSAnimations[5].m_Position);	//slow	

			Vec2 p = Vec2(lmg.m_BlendSpace.m_turn,lmg.m_BlendSpace.m_speed);
			LocomotionWeights6 w = WeightCalculationLoco6( p, FL,FF,FR,  SL,SF,SR );
			lmg.m_fBlendWeight[0]=w.fl;	lmg.m_fBlendWeight[1]=w.ff;	lmg.m_fBlendWeight[2]=w.fr;	
			lmg.m_fBlendWeight[3]=w.sl;	lmg.m_fBlendWeight[4]=w.sf;	lmg.m_fBlendWeight[5]=w.sr;	

			//f32 fColor[4] = {1,1,0,1};
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"FL: %f   FF: %f   FR: %f",w.fl,w.ff,w.fr );  g_YLine+=0x10;
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"SL: %f   SF: %f   SR: %f",w.sl,w.sf,w.sr );  g_YLine+=0x10;
			return;
		}




		//-----------------------------------------------------------------------------------------
		//-------              a run Forward-Left-Right LMG with three speeds               -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"FLR3" )
		{
			Vec2 FL=Vec2(rGlobalAnimHeader.m_arrBSAnimations[ 0].m_Position);	//sprint	
			Vec2 FF=Vec2(rGlobalAnimHeader.m_arrBSAnimations[ 1].m_Position);	//sprint	
			Vec2 FR=Vec2(rGlobalAnimHeader.m_arrBSAnimations[ 2].m_Position);	//sprint	
			Vec2 MF=Vec2(rGlobalAnimHeader.m_arrBSAnimations[ 3].m_Position);	//fast forward	
			Vec2 SL=Vec2(rGlobalAnimHeader.m_arrBSAnimations[ 4].m_Position);	//slow	
			Vec2 SF=Vec2(rGlobalAnimHeader.m_arrBSAnimations[ 5].m_Position);	//slow	
			Vec2 SR=Vec2(rGlobalAnimHeader.m_arrBSAnimations[ 6].m_Position);	//slow	

			Vec2 p = Vec2(lmg.m_BlendSpace.m_turn,lmg.m_BlendSpace.m_speed);
			LocomotionWeights7 w = WeightCalculationLoco7( p, FL,FF,FR, MF, SL,SF,SR );

			lmg.m_fBlendWeight[0]=w.fl;	lmg.m_fBlendWeight[1]=w.ff;	lmg.m_fBlendWeight[2]=w.fr;	
			lmg.m_fBlendWeight[3]=w.mf;	
			lmg.m_fBlendWeight[4]=w.sl;	lmg.m_fBlendWeight[5]=w.sf;	lmg.m_fBlendWeight[6]=w.sr;	

			//f32 fColor[4] = {1,1,0,1};
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"FL: %f   FF: %f  FR: %f",w.fl,w.ff,w.fr );  g_YLine+=0x10;
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"         MF: %f",w.mf );   g_YLine+=0x10;
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"SL: %f   SF: %f  SR: %f",w.sl,w.sf,w.sr );  g_YLine+=0x10;
			return;
		}


		//-----------------------------------------------------------------------------------------
		//-------        a standard left/right and uphill/downhill LMG with one speed       -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"UDH1" )
		{
			uint32 numAssets = rGlobalAnimHeader.m_arrBSAnimations.size();
			f32 uh=0,fw=0,dh=0;
			f32	t = lmg.m_BlendSpace.m_slope;
			if (t>0)
			{
				uh=t;	fw=(1-t);
			}
			else
			{
				fw=t+1;	dh=1-(t+1);
			}

			f32 R=0,F=0,L=0;	
			t = lmg.m_BlendSpace.m_turn;
			if (t<0)
			{
				R=1-(t+1); F=t+1;
			}
			else
			{
				F=1-t; L=t;
			}			
			lmg.m_fBlendWeight[0]=R*uh;
			lmg.m_fBlendWeight[1]=F*uh;
			lmg.m_fBlendWeight[2]=L*uh;

			lmg.m_fBlendWeight[3]=R*fw;
			lmg.m_fBlendWeight[4]=F*fw;
			lmg.m_fBlendWeight[5]=L*fw;

			lmg.m_fBlendWeight[6]=R*dh;
			lmg.m_fBlendWeight[7]=F*dh;
			lmg.m_fBlendWeight[8]=L*dh;

			return;
		}


		//-----------------------------------------------------------------------------------------
		//-------      a standard left/right and uphill/downhill LMG with two speeds        -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"UDH2" )
		{
			uint32 numAssets = rGlobalAnimHeader.m_arrBSAnimations.size();

			f32 uh=0;
			f32 fw=0;
			f32 dh=0;
			f32	t = lmg.m_BlendSpace.m_slope;
			if (t>0)
			{
				uh=t;
				fw=(1-t);
			}
			else
			{
				fw=t+1;
				dh=1-(t+1);
			}


			Vec2 p = Vec2(lmg.m_BlendSpace.m_turn,lmg.m_BlendSpace.m_speed);

			Vec3 bs06=rGlobalAnimHeader.m_arrBSAnimations[0x06].m_Position;		
			Vec3 bs07=rGlobalAnimHeader.m_arrBSAnimations[0x07].m_Position;		
			Vec3 bs08=rGlobalAnimHeader.m_arrBSAnimations[0x08].m_Position;		

			Vec3 bs09=rGlobalAnimHeader.m_arrBSAnimations[0x09].m_Position;		
			Vec3 bs0a=rGlobalAnimHeader.m_arrBSAnimations[0x0a].m_Position;		
			Vec3 bs0b=rGlobalAnimHeader.m_arrBSAnimations[0x0b].m_Position;		


			Vec2 p06=Vec2(bs06.x,bs06.y);		Vec2 p07=Vec2(bs07.x,bs07.y);		Vec2 p08=Vec2(bs08.x,bs08.y);
			Vec2 p09=Vec2(bs09.x,bs09.y);		Vec2 p0a=Vec2(bs0a.x,bs0a.y);		Vec2 p0b=Vec2(bs0b.x,bs0b.y);

			if (lmg.m_BlendSpace.m_turn<0)
			{
				//left side
				//f32 fColor[4] = {1,1,0,1};
				//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"left side"); g_YLine+=0x10;

				Vec4 w=WeightComputation4(p, p09,p0a,p07,p06);
				lmg.m_fBlendWeight[0+ 0]=w.w*uh;	lmg.m_fBlendWeight[1+ 0]=w.z*uh;	
				lmg.m_fBlendWeight[3+ 0]=w.x*uh;	lmg.m_fBlendWeight[4+ 0]=w.y*uh;

				lmg.m_fBlendWeight[0+ 6]=w.w*fw;	lmg.m_fBlendWeight[1+ 6]=w.z*fw;	
				lmg.m_fBlendWeight[3+ 6]=w.x*fw;	lmg.m_fBlendWeight[4+ 6]=w.y*fw;

				lmg.m_fBlendWeight[0+12]=w.w*dh;	lmg.m_fBlendWeight[1+12]=w.z*dh;	
				lmg.m_fBlendWeight[3+12]=w.x*dh;	lmg.m_fBlendWeight[4+12]=w.y*dh;
			}
			else
			{
				//right side
				Vec4 w=WeightComputation4(p, p0a,p0b,p08,p07);
				lmg.m_fBlendWeight[1+ 0]=w.w*uh;	lmg.m_fBlendWeight[2+ 0]=w.z*uh;	
				lmg.m_fBlendWeight[4+ 0]=w.x*uh;	lmg.m_fBlendWeight[5+ 0]=w.y*uh;

				lmg.m_fBlendWeight[1+ 6]=w.w*fw;	lmg.m_fBlendWeight[2+ 6]=w.z*fw;	
				lmg.m_fBlendWeight[4+ 6]=w.x*fw;	lmg.m_fBlendWeight[5+ 6]=w.y*fw;

				lmg.m_fBlendWeight[1+12]=w.w*dh;	lmg.m_fBlendWeight[2+12]=w.z*dh;	
				lmg.m_fBlendWeight[4+12]=w.x*dh;	lmg.m_fBlendWeight[5+12]=w.y*dh;
			}

			/*	
			f32 fColor[4] = {1,1,0,1};
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t00: %f   t01: %f   t02: %f", lmg.m_fBlendWeight[0+0],	lmg.m_fBlendWeight[1+0], lmg.m_fBlendWeight[2+0] ); g_YLine+=0x10;
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t03: %f   t04: %f   t05: %f", lmg.m_fBlendWeight[3+0],	lmg.m_fBlendWeight[4+0], lmg.m_fBlendWeight[5+0] ); g_YLine+=0x10;

			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t06: %f   t07: %f   t08: %f", lmg.m_fBlendWeight[0+6],	lmg.m_fBlendWeight[1+6], lmg.m_fBlendWeight[2+6] ); g_YLine+=0x10;
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t09: %f   t0a: %f   t0b: %f", lmg.m_fBlendWeight[3+6],	lmg.m_fBlendWeight[4+6], lmg.m_fBlendWeight[5+6] ); g_YLine+=0x10;

			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t0c: %f   t0d: %f   t0d: %f", lmg.m_fBlendWeight[0+12],	lmg.m_fBlendWeight[1+12], lmg.m_fBlendWeight[2+12] ); g_YLine+=0x10;
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t0e: %f   t0f: %f   t10: %f", lmg.m_fBlendWeight[3+12],	lmg.m_fBlendWeight[4+12], lmg.m_fBlendWeight[5+12] ); g_YLine+=0x10;
			*/

			return;
		}



		//-----------------------------------------------------------------------------------------
		//-------    a standard left/right and uphill/downhill LMG with three speeds        -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader.m_nBlendCodeLMG==*(uint32*)"UDH3" )
		{
			uint32 numAssets = rGlobalAnimHeader.m_arrBSAnimations.size();

			f32 uh=0,fw=0,dh=0;
			f32	t = lmg.m_BlendSpace.m_slope;
			if (t>0) { uh=t;	fw=1-t;	}
			else {	fw=1+t;	dh=1-(t+1);	}

			Vec2 p = Vec2(lmg.m_BlendSpace.m_turn,lmg.m_BlendSpace.m_speed);

			Vec3 bs09=rGlobalAnimHeader.m_arrBSAnimations[0x09].m_Position;		
			Vec3 bs0a=rGlobalAnimHeader.m_arrBSAnimations[0x0a].m_Position;		
			Vec3 bs0b=rGlobalAnimHeader.m_arrBSAnimations[0x0b].m_Position;		

			Vec3 bs0c=rGlobalAnimHeader.m_arrBSAnimations[0x0c].m_Position;		
			Vec3 bs0d=rGlobalAnimHeader.m_arrBSAnimations[0x0d].m_Position;		
			Vec3 bs0e=rGlobalAnimHeader.m_arrBSAnimations[0x0e].m_Position;		

			Vec3 bs0f=rGlobalAnimHeader.m_arrBSAnimations[0x0f].m_Position;		
			Vec3 bs10=rGlobalAnimHeader.m_arrBSAnimations[0x10].m_Position;		
			Vec3 bs11=rGlobalAnimHeader.m_arrBSAnimations[0x11].m_Position;		


			Vec2 p09=Vec2(bs09.x,bs09.y);		Vec2 p0a=Vec2(bs0a.x,bs0a.y);		Vec2 p0b=Vec2(bs0b.x,bs0b.y);
			Vec2 p0c=Vec2(bs0c.x,bs0c.y);		Vec2 p0d=Vec2(bs0d.x,bs0d.y);		Vec2 p0e=Vec2(bs0e.x,bs0e.y);
			Vec2 p0f=Vec2(bs0f.x,bs0f.y);		Vec2 p10=Vec2(bs10.x,bs10.y);		Vec2 p11=Vec2(bs11.x,bs11.y);

			if (lmg.m_BlendSpace.m_speed > 0)
			{
				//fast version
				if (lmg.m_BlendSpace.m_turn<0)
				{
					//left side
					//	f32 fColor[4] = {1,1,0,1};
					//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"left side fast"); g_YLine+=0x10;
					Vec4 w=WeightComputation4(p, p0c,p0d,p0a,p09);
					lmg.m_fBlendWeight[0+ 0]=w.w*uh;	lmg.m_fBlendWeight[1+ 0]=w.z*uh;	
					lmg.m_fBlendWeight[3+ 0]=w.x*uh;	lmg.m_fBlendWeight[4+ 0]=w.y*uh;

					lmg.m_fBlendWeight[0+ 9]=w.w*fw;	lmg.m_fBlendWeight[1+ 9]=w.z*fw;	
					lmg.m_fBlendWeight[3+ 9]=w.x*fw;	lmg.m_fBlendWeight[4+ 9]=w.y*fw;

					lmg.m_fBlendWeight[0+18]=w.w*dh;	lmg.m_fBlendWeight[1+18]=w.z*dh;	
					lmg.m_fBlendWeight[3+18]=w.x*dh;	lmg.m_fBlendWeight[4+18]=w.y*dh;
				}
				else
				{
					//right side
					Vec4 w=WeightComputation4(p, p0d,p0e,p0b,p0a);
					lmg.m_fBlendWeight[1+ 0]=w.w*uh;	lmg.m_fBlendWeight[2+ 0]=w.z*uh;	
					lmg.m_fBlendWeight[4+ 0]=w.x*uh;	lmg.m_fBlendWeight[5+ 0]=w.y*uh;

					lmg.m_fBlendWeight[1+ 9]=w.w*fw;	lmg.m_fBlendWeight[2+ 9]=w.z*fw;	
					lmg.m_fBlendWeight[4+ 9]=w.x*fw;	lmg.m_fBlendWeight[5+ 9]=w.y*fw;

					lmg.m_fBlendWeight[1+18]=w.w*dh;	lmg.m_fBlendWeight[2+18]=w.z*dh;	
					lmg.m_fBlendWeight[4+18]=w.x*dh;	lmg.m_fBlendWeight[5+18]=w.y*dh;
				}
			}
			else
			{
				//slow version
				if (lmg.m_BlendSpace.m_turn<0)
				{
					//left side
					//	f32 fColor[4] = {1,1,0,1};
					//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"left side slow"); g_YLine+=0x10;
					Vec4 w=WeightComputation4(p, p0f,p10,p0d,p0c);
					lmg.m_fBlendWeight[3+ 0]=w.w*uh;	lmg.m_fBlendWeight[4+ 0]=w.z*uh;	
					lmg.m_fBlendWeight[6+ 0]=w.x*uh;	lmg.m_fBlendWeight[7+ 0]=w.y*uh;

					lmg.m_fBlendWeight[3+ 9]=w.w*fw;	lmg.m_fBlendWeight[4+ 9]=w.z*fw;	
					lmg.m_fBlendWeight[6+ 9]=w.x*fw;	lmg.m_fBlendWeight[7+ 9]=w.y*fw;

					lmg.m_fBlendWeight[3+18]=w.w*dh;	lmg.m_fBlendWeight[4+18]=w.z*dh;	
					lmg.m_fBlendWeight[6+18]=w.x*dh;	lmg.m_fBlendWeight[7+18]=w.y*dh;
				}
				else
				{
					//right side
					Vec4 w=WeightComputation4(p, p10,p11,p0e,p0d);
					lmg.m_fBlendWeight[4+ 0]=w.w*uh;	lmg.m_fBlendWeight[5+ 0]=w.z*uh;	
					lmg.m_fBlendWeight[7+ 0]=w.x*uh;	lmg.m_fBlendWeight[8+ 0]=w.y*uh;

					lmg.m_fBlendWeight[4+ 9]=w.w*fw;	lmg.m_fBlendWeight[5+ 9]=w.z*fw;	
					lmg.m_fBlendWeight[7+ 9]=w.x*fw;	lmg.m_fBlendWeight[8+ 9]=w.y*fw;

					lmg.m_fBlendWeight[4+18]=w.w*dh;	lmg.m_fBlendWeight[5+18]=w.z*dh;	
					lmg.m_fBlendWeight[7+18]=w.x*dh;	lmg.m_fBlendWeight[8+18]=w.y*dh;
				}
			}

			//f32 fColor[4] = {1,1,0,1};
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t00: %f   t01: %f   t02: %f", lmg.m_fBlendWeight[0+0],	lmg.m_fBlendWeight[1+0], lmg.m_fBlendWeight[2+0] ); g_YLine+=0x10;
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t03: %f   t04: %f   t05: %f", lmg.m_fBlendWeight[3+0],	lmg.m_fBlendWeight[4+0], lmg.m_fBlendWeight[5+0] ); g_YLine+=0x10;

			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t06: %f   t07: %f   t08: %f", lmg.m_fBlendWeight[0+6],	lmg.m_fBlendWeight[1+6], lmg.m_fBlendWeight[2+6] ); g_YLine+=0x10;
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t09: %f   t0a: %f   t0b: %f", lmg.m_fBlendWeight[3+6],	lmg.m_fBlendWeight[4+6], lmg.m_fBlendWeight[5+6] ); g_YLine+=0x10;

			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t0c: %f   t0d: %f   t0d: %f", lmg.m_fBlendWeight[0+12],	lmg.m_fBlendWeight[1+12], lmg.m_fBlendWeight[2+12] ); g_YLine+=0x10;
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t0e: %f   t0f: %f   t10: %f", lmg.m_fBlendWeight[3+12],	lmg.m_fBlendWeight[4+12], lmg.m_fBlendWeight[5+12] ); g_YLine+=0x10;

			return;
		}




		//LMG not identified. This is a very bad thing
		char errtx[5];	*(uint32*)errtx = rGlobalAnimHeader.m_nBlendCodeLMG; 	errtx[4] = 0;
		CryError("locomotion-group '%s' not identified. This a fatal-error. Please get latest LMG files",errtx);
		assert(0);
	}
	else
	{

		lmg.m_fBlendWeight[0]=1;

	}


}


f32 CSkeletonAnim::GetTimewarpedDuration(SLocoGroup& lmg)
{
	if (lmg.m_nAnimID[0]<0)
		return 0;

	//f32 t0=lmg.m_fBlendWeight[0];
	//f32 t1=lmg.m_fBlendWeight[1];
	//f32 t2=lmg.m_fBlendWeight[2];
	//f32 t3=lmg.m_fBlendWeight[3];
	//f32 t4=lmg.m_fBlendWeight[4];


	f32 fColor[4] = {1,1,0,1};
	for (uint32 i=0; i<MAX_LMG_ANIMS; i++)
	{
		if (lmg.m_fBlendWeight[i]<0.001f)
			lmg.m_fBlendWeight[i]=0;
		if (lmg.m_fBlendWeight[i]>0.999f)
			lmg.m_fBlendWeight[i]=1;

		/*	if (lmg.m_fBlendWeight[i])
		{
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"i:%d  weight:%f",i,lmg.m_fBlendWeight[i] ); g_YLine+=10;
		}
		*/
	}


	f32 fDuration=0;
	for (int32 i=0; i<lmg.m_numAnims; i++)
	{
		int32 GlobalID = lmg.m_nGlobalID[i];
		int32 segcount = lmg.m_nSegmentCounter[i];
		GlobalAnimationHeader& rGAH = g_AnimationManager.m_arrGlobalAnimations[ GlobalID ];
		f32 d0 = rGAH.GetSegmentDuration(segcount);
		//		f32 d1 = lmg.m_fDuration[i];
		//		assert(d0==d1);
		fDuration	+= d0*lmg.m_fBlendWeight[i];
	}
	assert(fDuration);
	return fDuration;
}

