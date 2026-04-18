////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   CRAMSolution.cpp
//  Version:     v1.00
//  Created:     19/10/2006 by Michael Kopietz
//  Compilers:   Visual Studio.NET
//  Description: Header for radiosity lighting solution
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"

#include "CRAMSolution.h"
#include "LightEntity.h"
#include "VisAreas.h"


//////////////////////////////////////////////////////////////////////////
// CRAMSolution
//////////////////////////////////////////////////////////////////////////
void CRAMSolution::Init(CVisArea* pParent,const PodArray<Vec3>& rShape,const PodArray<CVisArea*>& rPortals)
{
	m_pShape	=	&rShape;
	const uint32 Count	=	rShape.Size()+rPortals.Size();
	if(Count==0)
	{
		m_pShape	=	0;
		return;
	}

	m_Segments.PreAllocate(Count,Count);
	for(uint32	a=0;a<Count;a++)
		m_Segments[a].ClearColors();

	GenNormals(pParent,rShape,rPortals);
	GenPositions(pParent,rShape,rPortals);
	GenAreaCorrection();
	OrderCorrection();
}

void CRAMSolution::Reset()
{
	const uint32 SCount	=	m_Segments.Size();
	for(uint32 a=0;a<SCount;a++)
		m_Segments[a].m_AmbientColor	=	m_Segments[a].m_DirectColor.zero();
}


void CRAMSolution::GenNormals(CVisArea* pParent,const PodArray<Vec3>& rShape,const PodArray<CVisArea*>& rPortals)
{
	const uint32 Count	=	rShape.Size();
	for(uint32 a=0,b=Count-1;a<Count;b=a++)
	{
		m_Segments[a].m_Normal	=	((rShape[a]-rShape[b])%Vec3(0.f,0.f,pParent->m_fHeight));
//		assert(m_Segments[a].m_Normal.len()>FLT_EPSILON);
		m_Segments[a].m_InvNormalLen	=	m_Segments[a].m_Normal.len();
		m_Segments[a].m_InvNormalLen	=	m_Segments[a].m_InvNormalLen>FLT_EPSILON?1.f/m_Segments[a].m_InvNormalLen:0.f;
		m_Segments[a].m_pPortal	=	NULL;
	}


	const uint PCount	=	rPortals.Size();
	for(uint32 a=0;a<PCount;a++)
	{
		m_Segments[a+Count].m_Normal	=	rPortals[a]->GetConnectionNormal(pParent);
		if(fabsf(m_Segments[a+Count].m_Normal.y)<FLT_EPSILON)
			m_Segments[a+Count].m_Normal.z*=	rPortals[a]->m_fHeight;
		else
		{
			const uint32 PSCount	=	rPortals[a]->m_lstShapePoints.Size();
			if(PSCount>0)
			{
				f32 MinX,MaxX,MinY,MaxY;
				MinX=MaxX=rPortals[a]->m_lstShapePoints[0].x;
				MinY=MaxY=rPortals[a]->m_lstShapePoints[0].y;
				for(uint b=1;b<PSCount;b++)
				{
					if(MinX>rPortals[a]->m_lstShapePoints[b].x)
						MinX=rPortals[a]->m_lstShapePoints[b].x;
					if(MaxX<rPortals[a]->m_lstShapePoints[b].x)
						MaxX=rPortals[a]->m_lstShapePoints[b].x;
					if(MinY>rPortals[a]->m_lstShapePoints[b].y)
						MinY=rPortals[a]->m_lstShapePoints[b].y;
					if(MaxY<rPortals[a]->m_lstShapePoints[b].y)
						MaxY=rPortals[a]->m_lstShapePoints[b].y;
				}
				m_Segments[a+Count].m_Normal.y*=	max(MaxX-MinX,MaxY-MinY);
			}

		}
//		assert(m_Segments[a+Count].m_Normal.len()>FLT_EPSILON);
		m_Segments[a+Count].m_InvNormalLen	=	m_Segments[a+Count].m_Normal.len();
		m_Segments[a+Count].m_InvNormalLen	=	m_Segments[a+Count].m_InvNormalLen>FLT_EPSILON?1.f/m_Segments[a+Count].m_InvNormalLen:0.f;
		m_Segments[a+Count].m_pPortal	=	rPortals[a];
	}
}

void CRAMSolution::GenPositions(CVisArea* pParent,const PodArray<Vec3>& rShape,const PodArray<CVisArea*>& rPortals)
{
	const uint32 Count	=	rShape.Size();
	for(uint32 a=0,b=Count-1;a<Count;b=a++)
	{
			m_Segments[a].m_Position	=	(rShape[a]+rShape[b]+Vec3(0.f,0.f,pParent->m_fHeight))*0.5f;
			m_Segments[a].m_DistToOrigin	=	m_Segments[a].m_Position*m_Segments[a].m_Normal*m_Segments[a].m_InvNormalLen;
	}

	const uint PCount	=	rPortals.Size();
	for(uint32 a=0;a<PCount;a++)
	{
		m_Segments[a+Count].m_Position	=	(rPortals[a]->m_boxArea.min+rPortals[a]->m_boxArea.max+Vec3(0.f,0.f,rPortals[a]->m_fHeight))*0.5f;
		m_Segments[a+Count].m_DistToOrigin	=	m_Segments[a+Count].m_Position*m_Segments[a+Count].m_Normal*m_Segments[a+Count].m_InvNormalLen;
	}
}

void CRAMSolution::GenAreaCorrection()
{
	f32 WallArea	=	0.f;
	f32 PortalArea=	0.f;
	const uint32 Count	=	m_pShape->Size();
	for(uint32 a=0;a<Count;a++)
		WallArea+=m_Segments[a].m_Normal.len();

	const uint PCount	=	m_Segments.Size()-Count;
	for(uint32 a=0;a<PCount;a++)
		PortalArea+=m_Segments[a+Count].m_Normal.len();

	m_AreaCorrection	=	WallArea/(WallArea+PortalArea);
}

void CRAMSolution::OrderCorrection()
{
	const uint32 SCount	=	m_Segments.Size();
	int Facing=0;
	for(uint32 a=0;a<SCount;a++)
		for(uint32 b=0;b<SCount;b++)
			Facing+=m_Segments[a].m_Normal*m_Segments[b].m_Position*m_Segments[a].m_InvNormalLen-m_Segments[a].m_DistToOrigin>=0.f?1:-1;

	if(Facing<0)
		for(uint32 a=0;a<SCount;a++)
		{
			m_Segments[a].m_Normal	*=	-1.f;
			m_Segments[a].m_DistToOrigin	=	m_Segments[a].m_Position*m_Segments[a].m_Normal*m_Segments[a].m_InvNormalLen;
		}
}

f32	CRamSolutionSegment::DistPointToLineSegment(Vec3& rDir,const Vec3& rPos,const Vec3& rEdge0,const Vec3& rEdge1)	const
{
	const Vec3 Lineseg	=	rEdge1-rEdge0;
	const f32 DistToPos	=	rPos*Lineseg;
	const	f32	DistToE0	=	rEdge0*Lineseg;
	const	f32	DistToE1	=	rEdge1*Lineseg;

	if(DistToE0<=DistToE1)
	{
		if(DistToPos<=DistToE0)
		{
			rDir	=	rEdge0-rPos;
			const f32 Len	=	rDir.len();
			rDir	*=	1.f/Len;
			return Len;
		}
		if(DistToPos>=DistToE1)
		{
			rDir	=	rEdge1-rPos;
			const f32 Len	=	rDir.len();
			rDir	*=	1.f/Len;
			return Len;
		}
	}
	else
	{
		if(DistToPos>=DistToE0)
		{
			rDir	=	rEdge0-rPos;
			const f32 Len	=	rDir.len();
			rDir	*=	1.f/Len;
			return Len;
		}
		if(DistToPos<=DistToE1)
		{
			rDir	=	rEdge1-rPos;
			const f32 Len	=	rDir.len();
			rDir	*=	1.f/Len;
			return Len;
		}
	}

	//point nearer to line segment than to one of it's vertices
	rDir	=	m_Normal*m_InvNormalLen;
	return rPos*rDir-m_DistToOrigin;
}

bool CRAMSolution::Intersection(const Vec3& rL0P0,const Vec3& rL0P1,const Vec3& rL1P0,const Vec3& rL1P1)	const
{
	const	f32	x21				=	rL0P1.x-rL0P0.x;
	const	f32	y21				=	rL0P1.y-rL0P0.y;
	const	f32	x43				=	rL1P1.x-rL1P0.x;
	const f32	y43				=	rL1P1.y-rL1P0.y;
	const f32 fCross		=	y43*x21-x43*y21;
	const f32 fCrossAbs	=	fabsf(fCross);

	if(fCross<FLT_EPSILON)
		return false;

	const	f32	x13	=	rL0P0.x-rL1P0.x;
	const f32	y13	=	rL0P0.y-rL1P0.y;
	const f32	ua = x43*y13-y43*x13;
	const	f32	ub = x21*y13-y21*x13;

	return ua*fCross>0 && ua<fCrossAbs && ub*fCross>0 && ub<fCrossAbs;
}

bool CRAMSolution::Intersects(const Vec3& rPos0,const Vec3& rPos1)const
{
	const uint32 Count	=	m_pShape->Size();
	for(uint a=0,b=Count-1;a<Count;b=a++)
		if(Intersection((*m_pShape)[a],(*m_pShape)[b],rPos0,rPos1))
			return true;
/*	for(uint32 a=0;a<CurrentSegment;a++)
		if(m_Segments[a].Intersection(rPos0,rPos1))
			return true;

	const uint32 SCount	=	m_Segments.Size();
	for(uint32 a=CurrentSegment+1;a<SCount;a++)
		if(m_Segments[a].Intersection(rPos0,rPos1))
			return true;*/
	return false;
}

f32 CRAMSolution::Diameter(CVisArea& rVA)const
{
	const uint32	Count			=	rVA.m_lstShapePoints.Size();
	if(Count<=0)
		return 0.f;

	float MinX,MaxX,MinY,MaxY;

	MinX	=	MaxX	=	rVA.m_lstShapePoints[0].x;
	MinY	=	MaxY	=	rVA.m_lstShapePoints[0].y;
	for(uint32 a=1;a<Count;a++)
	{
		const Vec3& rV	=	rVA.m_lstShapePoints[a];
		if(rV.x<MinX)	MinX=rV.x;
		if(rV.y<MinY)	MinY=rV.y;
		if(rV.x>MaxX)	MaxX=rV.x;
		if(rV.y>MaxY)	MaxY=rV.y;
	}
	MaxX-=MinX;
	MaxY-=MinY;
	return sqrtf(MaxX*MaxX+MaxY*MaxY);
}

void CRAMSolution::CalcDirectLighting(CVisArea* pParent,const PodArray<CVisArea*>& rPortals,const PodArray<IRenderNode*>& rLights)
{
	const uint32	ShapeCount	=	m_pShape->Size();
	const uint32	SCount			=	m_Segments.Size();
	for(uint32 a=0;a<SCount;a++)
		m_Segments[a].m_DirectColor.zero();

	//lightsources
	const uint32 LCount	=	rLights.Size();
	for(uint32 a=0;a<LCount;a++)
	{
		assert(rLights[a]->GetRenderNodeType()==eERType_Light);
		const	CDLight&	rLight				=	reinterpret_cast<CLightEntity*>(rLights[a])->GetLightProperties();
		const Vec3&			rLightPos			=	rLight.m_BaseOrigin;
		const Vec3&			rLightAmbient	=	*reinterpret_cast<const Vec3*>(&rLight.m_RAEAmbientColor);
		for(uint32 b=0;b<ShapeCount;b++)
		{
			CRamSolutionSegment&	rSeg	=	m_Segments[b];
			const Vec3 Delta		=	rLightPos-rSeg.m_Position;
			const f32	Intensity	=	Delta*rSeg.m_Normal;
			if(Intensity>FLT_EPSILON && !Intersects(rSeg.m_Position,rLightPos))	//if light affects plane and no shadowcasting
				rSeg.m_DirectColor	+=	rLightAmbient*(Intensity/Delta.len())*rSeg.m_InvNormalLen;	//division for len as normalization, this lazy way prevents checks for zero vektors and useless divisions and len calcs
		}
	}

	//portal(light)sources
	const uint32 PCount			=	rPortals.Size();
	if(PCount+ShapeCount!=SCount)	//if not portals included into lighting solution, reinit
		return Init(pParent,*m_pShape,rPortals);

	for(uint32 a=0;a<PCount;a++)
	{
		CRamSolutionSegment&	rSeg	=	m_Segments[a+ShapeCount];
		if(rSeg.m_Normal.len2()<FLT_EPSILON)
		{
			rSeg.m_InvNormalLen	=	rPortals[a]->m_fHeight*Diameter(*rPortals[a]);
			rSeg.m_Normal	=	pParent->GetConnectionNormal(rPortals[a])*rSeg.m_InvNormalLen;
			rSeg.m_pPortal	=	rPortals[a];
			rSeg.m_InvNormalLen	=	rSeg.m_InvNormalLen>FLT_EPSILON?1.f/rSeg.m_InvNormalLen:0.f;
		}
		CVisArea& rPortal	=	*rSeg.m_pPortal;

		//if affected by sun, add ambient lighting by sun
		if(rPortal.m_bAffectedByOutLights && !rPortal.m_bIgnoreSky)
		{
			const Vec3& rSunDir		=	pParent->Get3DEngine()->GetSunDirNormalized();
			const Vec3 PortalNrm	=	rSeg.m_Normal;
			f32 SunIntensity	=	((-(rSunDir*PortalNrm)*rSeg.m_InvNormalLen)*0.5f+0.5f)/rSeg.m_InvNormalLen;
			if(SunIntensity>FLT_EPSILON)
			{
				rSeg.m_AmbientColor	=	pParent->Get3DEngine()->GetSkyColor();
				rSeg.m_DirectColor	=	pParent->Get3DEngine()->GetSunColor()*SunIntensity;
				rSeg.m_AmbientColor	+=	rSeg.m_DirectColor;
			}
		}
		else	//if affected by neighbor-portal
		{
			//assert(rPortal.m_lstConnections.Size()==2);
			if(rPortal.m_lstConnections.Size()==2)
			{
				CVisArea& rNeighbour	=	*(rPortal.m_lstConnections[0]==pParent?rPortal.m_lstConnections[1]:rPortal.m_lstConnections[0]);
				Vec3 NeighbourColor		=	rNeighbour.m_RAMSolution[(pParent->m_nRndFrameId&1)^1].CalcAmbientDirectColor(rSeg.Position(),-rSeg.m_Normal);
				rSeg.m_AmbientColor		=
				rSeg.m_DirectColor		=	NeighbourColor;//*rSeg.m_InvNormalLen;
			}
		}
	}
}

void CRAMSolution::CalcAmbientDistribution(const CRAMSolution& rLastSolution)
{
	const uint32	SCount1	=	m_pShape->Size();		//calculation just for walls,
	const uint32	SCount2	=	m_Segments.Size();	//but use portals as sources aswell
	if(rLastSolution.m_Segments.Size()!=SCount2)
		return;
	for(uint32 a=0;a<SCount1;a++)
	{
		CRamSolutionSegment& rSeg	=	m_Segments[a];
//		rSeg.m_AmbientColor.zero();
		Vec3 Col;
		Col.zero();
		for(uint32 b=0;b<SCount2;b++)
			if(b!=a)
			{
				const CRamSolutionSegment& rSeg2	=	rLastSolution.m_Segments[b];
				Vec3	Dir				=	(rSeg.Position()-rSeg2.Position());
				const f32 InvDistance	=	1.f/sqrt_tpl(Dir.len2()+FLT_EPSILON);
				Dir	*=	InvDistance;
				const f32 Intensity0	=	-(rSeg.m_Normal*Dir);
				const f32	Intensity1	=	Dir*rSeg2.m_Normal;
				if(Intensity0>FLT_EPSILON && Intensity1>FLT_EPSILON)// && !Intersects(rSeg.m_Position,rSeg2.m_Position))
					Col	+=	rSeg2.m_AmbientColor*(Intensity0*Intensity1*InvDistance*InvDistance*rSeg2.m_InvNormalLen/gf_PI)/*(*0.5f)*rSeg.m_InvNormalLen*/;
			}
		rSeg.m_AmbientColor	*=	0.2f;
		rSeg.m_AmbientColor	+=	(Col*rSeg.m_InvNormalLen*m_AreaCorrection+rSeg.m_DirectColor)*0.8f;
//		rSeg.m_AmbientColor	+=	rSeg.m_DirectColor;
	}
}

void CRAMSolution::CalcLightingCell(CVisArea* pParent,const CRAMSolution& rLastSolution,const PodArray<CVisArea*>& rPortals,const PodArray<IRenderNode*>& rLights)
{
	if(!m_pShape)
		return;
	FUNCTION_PROFILER_FAST( pParent->GetSystem(), PROFILE_3DENGINE, pParent->m_bProfilerEnabled );
	//clear ambient, but just the walls, not the portallights
	const uint32	SCount	=	m_Segments.Size();
//	for(uint32 a=0;a<SCount;a++)
//		m_Segments[a].m_AmbientColor	*=	0.1f;
//		m_Segments[a].m_AmbientColor.zero();

	CalcDirectLighting(pParent,rPortals,rLights);
	CalcAmbientDistribution(rLastSolution);
}

void CRAMSolution::CalcLightingPortal(CVisArea* pParent,const CRAMSolution& rLastSolution,const PodArray<CVisArea*>& rCells,const PodArray<IRenderNode*>& rLights)
{
}

Vec3 CRAMSolution::CalcAmbientDirectColor(const Vec3& rPos,const Vec3& rNrm)	const
{
	Vec3	Color;
	Color.zero();
	const uint32	SCount	=	m_Segments.Size();
	for(uint32 a=0;a<SCount;a++)
	{
		CRamSolutionSegment&	rSeg	=	m_Segments[a];
		const Vec3&	SegCenter	=	rSeg.Position();
		Vec3	Delta			=	rPos-SegCenter;
		const f32		DeltaLen	=	Delta.len2();
		if(DeltaLen>FLT_EPSILON)
		{
			Delta*=1.f/sqrt_tpl(DeltaLen);
			const f32	Intensity0	=	Delta*rSeg.m_Normal;
			const f32	Intensity1	=	-(Delta*rNrm);
			if(Intensity0>FLT_EPSILON	&&	Intensity1>FLT_EPSILON)// &&	!Intersects(rPos,SegCenter))
				Color	+=	rSeg.m_AmbientColor*(Intensity0*Intensity1)/(DeltaLen);
		}
	}
	return Color;//*6.f/2.f;	//we just gathered the light for one face of an six sided cube for the portals
												//but we need one half of the whole cube intensity for one portal side
}

Vec3 CRAMSolution::CalcAmbientColor(const Vec3& rPos,const Vec3& rNrm)	const
{
/*	Vec3	Color;
	Color.zero();
	const uint32	ShapeCount	=	m_pShape->Size();
	f32	UsedEdgeCount=0.f;
	Vec3 Delta;
	for(uint32 a=0,b=ShapeCount-1;a<ShapeCount;b=a++)
	{
		CRamSolutionSegment&	rSeg	=	m_Segments[a];
		const f32		DeltaLen	=	rSeg.DistPointToLineSegment(Delta,rPos,(*m_pShape)[a],(*m_pShape)[b]);
		if(DeltaLen>FLT_EPSILON)
		{
			const f32	Intensity0	=	Delta*rSeg.m_Normal;
			const f32	Intensity1	=	-(Delta*rNrm);
			if(Intensity0>FLT_EPSILON	&&	Intensity1>FLT_EPSILON)
			{
			UsedEdgeCount++;
			Color	+=	(rSeg.m_AmbientColor-rSeg.m_DirectColor)*(Intensity0*Intensity1)/(DeltaLen*DeltaLen);
			}
		}
	}
	return UsedEdgeCount>FLT_EPSILON?Color*(1.f/UsedEdgeCount):Vec3(0.f,0.f,0.f);
/*/	Vec3	Color;
	Color.zero();
	const uint32	SCount	=	m_Segments.Size();
	f32	UsedEdgeCount=0.f;
	for(uint32 a=0;a<SCount;a++)
	{
		CRamSolutionSegment&	rSeg	=	m_Segments[a];
		const Vec3&	SegCenter	=	rSeg.Position();
		Vec3	Delta			=	rPos-SegCenter;
		const f32		InvDeltaLen	=	1.f/sqrt_tpl(Delta.len2()+0.00001f);//0.00001 to prevent div by 0
		Delta*=InvDeltaLen;
		const f32	Intensity0	=	(Delta*rSeg.m_Normal)*0.5f+0.5f;
		const f32	Intensity1	=	-(Delta*rNrm)*0.5f+0.5f;
//		if(Intensity0>FLT_EPSILON	&&	Intensity1>FLT_EPSILON)
		{
			UsedEdgeCount++;
			if(a<m_pShape->Size())
				Color	+=	(rSeg.m_AmbientColor-rSeg.m_DirectColor)*(Intensity0*Intensity1)*InvDeltaLen;
			else
				Color	+=	rSeg.m_AmbientColor*(Intensity0*Intensity1)*InvDeltaLen;
		}
	}
	return UsedEdgeCount>FLT_EPSILON?Color*(1.f/UsedEdgeCount):Vec3(0.f,0.f,0.f);/**/
}

void CRAMSolution::CalcAmbientCube(f32* pAmbientCube,const Vec3& rAABBMin,const Vec3& rAABBMax)	const
{
	const Vec3 Center	=	(rAABBMin+rAABBMax)*0.5f;
	*((Vec3*)&pAmbientCube[0])	=	CalcAmbientColor(Center,Vec3( 1, 0, 0));
	*((Vec3*)&pAmbientCube[4])	=	CalcAmbientColor(Center,Vec3(-1, 0, 0));
	*((Vec3*)&pAmbientCube[8])	=	CalcAmbientColor(Center,Vec3( 0, 1, 0));
	*((Vec3*)&pAmbientCube[12])	=	CalcAmbientColor(Center,Vec3( 0,-1, 0));
	*((Vec3*)&pAmbientCube[16])	=	CalcAmbientColor(Center,Vec3( 0, 0, 1));
	*((Vec3*)&pAmbientCube[20])	=	CalcAmbientColor(Center,Vec3( 0, 0,-1));
}

void CRAMSolution::CalcAmbientCubeArea(f32* pAmbientCube,const Vec3& rAABBMin,const Vec3& rAABBMax)	const
{
	CalcAmbientCube(pAmbientCube,rAABBMin,rAABBMax);
}

void CRAMSolution::CalcAmbientCubePortal(CVisArea* pParent,f32* pAmbientCube,const Vec3& rAABBMin,const Vec3& rAABBMax)	const
{
	for(uint32 a=0;a<24;a++)
		pAmbientCube[a]=0.f;
	const uint32 CCount	=	pParent->m_lstConnections.Size();
	for(uint32 a=0;a<CCount;a++)
	{
		f32	Colors[96];
		pParent->m_lstConnections[a]->m_RAMSolution[pParent->m_nRndFrameId&1].CalcAmbientCube(Colors,rAABBMin,rAABBMax);
		for(uint32 a=0;a<24;a+=4)
		{
			pAmbientCube[a]+=Colors[a];
			pAmbientCube[a+1]+=Colors[a+1];
			pAmbientCube[a+2]+=Colors[a+2];
//			pAmbientCube[a+3]+=Colors[a+3];
		}
	}
}

void CRAMSolution::CompressAmbient(f32* pAmbientCube)	const
{
	float TmpBuffer[6*4];
	//calc luminance per sample
	for(uint a=0;a<6;a++)
		for(uint b=0;b<4;b++)
			TmpBuffer[a*4+b]	=	pAmbientCube[a*16+b*4]*0.5f+pAmbientCube[a*16+b*4+1]*0.4f+pAmbientCube[a*16+b*4+2]*0.1f+FLT_EPSILON;//to void DivByZero

	//average color per side
	for(uint a=0;a<6;a++)
		for(uint b=0;b<3;b++)
			pAmbientCube[a*4+b]	=	(pAmbientCube[a*16+b]/TmpBuffer[a*4]+
														pAmbientCube[a*16+4+b]/TmpBuffer[a*4+1]+
														pAmbientCube[a*16+8+b]/TmpBuffer[a*4+2]+
														pAmbientCube[a*16+12+b]/TmpBuffer[a*4+3])*0.25f;

	for(uint a=0;a<24;a++)
		pAmbientCube[24+a]	=	TmpBuffer[a];
}

void CRAMSolution::CalcHQAmbientCube(f32* pAmbientCube,const Vec3& rAABBMin,const Vec3& rAABBMax)	const
{
	const Vec3	Center	=	(rAABBMin+rAABBMax)*0.5f;
	const Vec3	Extends	=	(rAABBMax-Center)*0.95f;
	const Vec3	AABBMin	=	Center	-	Extends;
	const Vec3	AABBMax	=	Center	+	Extends;
	*((Vec3*)&pAmbientCube[0])	=	CalcAmbientColor(Vec3(AABBMax.x,AABBMin.y,AABBMin.z),Vec3( 1, 0, 0));
	*((Vec3*)&pAmbientCube[4])	=	CalcAmbientColor(Vec3(AABBMax.x,AABBMax.y,AABBMin.z),Vec3( 1, 0, 0));
	*((Vec3*)&pAmbientCube[8])	=	CalcAmbientColor(Vec3(AABBMax.x,AABBMin.y,AABBMax.z),Vec3( 1, 0, 0));
	*((Vec3*)&pAmbientCube[12])	=	CalcAmbientColor(Vec3(AABBMax.x,AABBMax.y,AABBMax.z),Vec3( 1, 0, 0));

	*((Vec3*)&pAmbientCube[16])	=	CalcAmbientColor(Vec3(AABBMin.x,AABBMin.y,AABBMin.z),Vec3(-1, 0, 0));
	*((Vec3*)&pAmbientCube[20])	=	CalcAmbientColor(Vec3(AABBMin.x,AABBMax.y,AABBMin.z),Vec3(-1, 0, 0));
	*((Vec3*)&pAmbientCube[24])	=	CalcAmbientColor(Vec3(AABBMin.x,AABBMin.y,AABBMax.z),Vec3(-1, 0, 0));
	*((Vec3*)&pAmbientCube[28])	=	CalcAmbientColor(Vec3(AABBMin.x,AABBMax.y,AABBMax.z),Vec3(-1, 0, 0));

	*((Vec3*)&pAmbientCube[32])	=	CalcAmbientColor(Vec3(AABBMin.x,AABBMax.y,AABBMin.z),Vec3( 0, 1, 0));
	*((Vec3*)&pAmbientCube[36])	=	CalcAmbientColor(Vec3(AABBMax.x,AABBMax.y,AABBMin.z),Vec3( 0, 1, 0));
	*((Vec3*)&pAmbientCube[40])	=	CalcAmbientColor(Vec3(AABBMin.x,AABBMax.y,AABBMax.z),Vec3( 0, 1, 0));
	*((Vec3*)&pAmbientCube[44])	=	CalcAmbientColor(Vec3(AABBMax.x,AABBMax.y,AABBMax.z),Vec3( 0, 1, 0));

	*((Vec3*)&pAmbientCube[48])	=	CalcAmbientColor(Vec3(AABBMin.x,AABBMin.y,AABBMin.z),Vec3( 0,-1, 0));
	*((Vec3*)&pAmbientCube[52])	=	CalcAmbientColor(Vec3(AABBMax.x,AABBMin.y,AABBMin.z),Vec3( 0,-1, 0));
	*((Vec3*)&pAmbientCube[56])	=	CalcAmbientColor(Vec3(AABBMin.x,AABBMin.y,AABBMax.z),Vec3( 0,-1, 0));
	*((Vec3*)&pAmbientCube[60])	=	CalcAmbientColor(Vec3(AABBMax.x,AABBMin.y,AABBMax.z),Vec3( 0,-1, 0));

	*((Vec3*)&pAmbientCube[64])	=	CalcAmbientColor(Vec3(AABBMin.x,AABBMin.y,AABBMax.z),Vec3( 0, 0, 1));
	*((Vec3*)&pAmbientCube[68])	=	CalcAmbientColor(Vec3(AABBMax.x,AABBMin.y,AABBMax.z),Vec3( 0, 0, 1));
	*((Vec3*)&pAmbientCube[72])	=	CalcAmbientColor(Vec3(AABBMin.x,AABBMax.y,AABBMax.z),Vec3( 0, 0, 1));
	*((Vec3*)&pAmbientCube[76])	=	CalcAmbientColor(Vec3(AABBMax.x,AABBMax.y,AABBMax.z),Vec3( 0, 0, 1));

	*((Vec3*)&pAmbientCube[80])	=	CalcAmbientColor(Vec3(AABBMin.x,AABBMin.y,AABBMin.z),Vec3( 0, 0,-1));
	*((Vec3*)&pAmbientCube[84])	=	CalcAmbientColor(Vec3(AABBMax.x,AABBMin.y,AABBMin.z),Vec3( 0, 0,-1));
	*((Vec3*)&pAmbientCube[88])	=	CalcAmbientColor(Vec3(AABBMin.x,AABBMax.y,AABBMin.z),Vec3( 0, 0,-1));
	*((Vec3*)&pAmbientCube[92])	=	CalcAmbientColor(Vec3(AABBMax.x,AABBMax.y,AABBMin.z),Vec3( 0, 0,-1));
}

void CRAMSolution::CalcHQAmbientCubeArea(f32* pAmbientCube,const Vec3& rAABBMin,const Vec3& rAABBMax)	const
{
	if(!m_pShape)
	{
		for(uint32 a=0;a<96;a++)
			pAmbientCube[a]=0.f;
		return;
	}
	CalcHQAmbientCube(pAmbientCube,rAABBMin,rAABBMax);
	CompressAmbient(pAmbientCube);
}

void CRAMSolution::CalcHQAmbientCubePortal(CVisArea* pParent,f32* pAmbientCube,const Vec3& rAABBMin,const Vec3& rAABBMax)	const
{
	for(uint32 a=0;a<96;a++)
		pAmbientCube[a]=0.f;

	if(!m_pShape)
		return;
	const Vec3 Dirs[]	=	{	Vec3( 1, 0, 0),
												Vec3(-1, 0, 0),
												Vec3( 0, 1, 0),
												Vec3( 0,-1, 0),
												Vec3( 0, 0, 1),
												Vec3( 0, 0,-1)};

	const uint32 CCount	=	pParent->m_lstConnections.Size();
	for(uint32 a=0;a<CCount;a++)
	{
		f32	Colors[96];
		pParent->m_lstConnections[a]->m_RAMSolution[pParent->m_nRndFrameId&1].CalcHQAmbientCube(Colors,rAABBMin,rAABBMax);
		const Vec3	Nrm	=	pParent->m_lstConnections[a]->GetConnectionNormal(pParent);
		for(uint32 a=0;a<96/4;a++)
		{
			const float Intensity	=	Nrm	*	Dirs[a/4];
			if(Intensity>FLT_EPSILON)
			{
				pAmbientCube[a*4]		+=	Colors[a*4]*Intensity;
				pAmbientCube[a*4+1]	+=	Colors[a*4+1]*Intensity;
				pAmbientCube[a*4+2]	+=	Colors[a*4+2]*Intensity;
//			pAmbientCube[a*4+3]	+=	Colors[a*4+3]*Intensity;
			}
		}
	}
	CompressAmbient(pAmbientCube);
}
