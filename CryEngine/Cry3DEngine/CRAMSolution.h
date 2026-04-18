////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   CRAMSolution.h
//  Version:     v1.00
//  Created:     19/10/2006 by Michael Kopietz
//  Compilers:   Visual Studio.NET
//  Description: Header for radiosity lighting solution
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRAMSOLUTION_H__
#define __CRAMSOLUTION_H__

struct CRamSolutionSegment
{
	Vec3							m_Position;
	f32								m_DistToOrigin;
	Vec3							m_Normal;
	f32								m_InvNormalLen;
	Vec3							m_AmbientColor;
	Vec3							m_DirectColor;
	CVisArea*					m_pPortal;
	void							ClearColors(){m_AmbientColor	=	m_DirectColor.zero();}
	const Vec3&				Position()const{return m_Position;}
	f32								DistPointToLineSegment(Vec3& rDir,const Vec3& rPos,const Vec3& rEdge0,const Vec3& rEdge1)	const;
};

class CRAMSolution
{
	const PodArray<Vec3>*					m_pShape;
	PodArray<CRamSolutionSegment>	m_Segments;
	f32													m_Orientation;
	f32													m_AreaCorrection;

	Vec3												CalcAmbientDirectColor(const Vec3& rPos,const Vec3& rNrm)	const;
	Vec3												CalcAmbientColor(const Vec3& rPos,const Vec3& rNrm)	const;
	void												CalcAmbientDistribution(const CRAMSolution& rLastSolution);
	bool												Intersects(const Vec3& rPos0,const Vec3& rPos1)const;
	f32													Diameter(CVisArea& rVA)const;
	void												CalcDirectLighting(CVisArea* pParent,const PodArray<CVisArea*>& rPortals,const PodArray<IRenderNode*>& rLights);
	bool												Intersection(const Vec3& rL0P0,const Vec3& rL0P1,const Vec3& rL1P0,const Vec3& rL1P1)	const;
	void												GenNormals(CVisArea* pParent,const PodArray<Vec3>& rShape,const PodArray<CVisArea*>& rPortals);
	void												GenPositions(CVisArea* pParent,const PodArray<Vec3>& rShape,const PodArray<CVisArea*>& rPortals);
	void												GenAreaCorrection();
	void												OrderCorrection();
	void												CalcAmbientCube(f32* pAmbientCube,const Vec3& rAABBMin,const Vec3& rAABBMax)	const;
	void												CalcHQAmbientCube(f32* pAmbientCube,const Vec3& rAABBMin,const Vec3& rAABBMax)	const;
	void												CompressAmbient(f32* pAmbientCube)	const;
public:

															CRAMSolution():
															m_pShape(0)
															{
															}
	void												Init(CVisArea* pParent,const PodArray<Vec3>& rShape,const PodArray<CVisArea*>& rPortals);

	void												CalcLightingCell(CVisArea* pParent,const CRAMSolution& rLastSolution,const PodArray<CVisArea*>& rPortals,const PodArray<IRenderNode*>& rLights);
	void												CalcLightingPortal(CVisArea* pParent,const CRAMSolution& rLastSolution,const PodArray<CVisArea*>& rCells,const PodArray<IRenderNode*>& rLights);

	void												CalcAmbientCubeArea(f32* pAmbientCube,const Vec3& rAABBMin,const Vec3& rAABBMax)	const;
	void												CalcAmbientCubePortal(CVisArea* pParent,f32* pAmbientCube,const Vec3& rAABBMin,const Vec3& rAABBMax)	const;
	void												CalcHQAmbientCubeArea(f32* pAmbientCube,const Vec3& rAABBMin,const Vec3& rAABBMax)	const;
	void												CalcHQAmbientCubePortal(CVisArea* pParent,f32* pAmbientCube,const Vec3& rAABBMin,const Vec3& rAABBMax)	const;

	void												Reset();
};


#endif // __CRAMSOLUTION_H__
