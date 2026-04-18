/*=============================================================================
  RLMesh.h : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#ifndef __RLMESH_H__
#define __RLMESH_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "Cry_Color.h"
#include "IEntityRenderState.h"
#include "IlluminanceIntegrator.h"
#include "SceneContext.h"
#include "RLUnwrapGroup.h"
#include "RLWaitProgress.h"

//RenderLight mesh
class CRLMesh
{
	public:
		// Con/de-struction
		CRLMesh(IRenderMesh* pRenderMesh, Matrix34* pNodeMatrix, CRenderChunk* pRenderChunk, IRenderShaderResources* shaderResources,const bool bNeedToAutoGenerateTextureCoordinates = false,const bool bOpaque = true, const f32 fLightmapQuality = 1 );

		virtual ~CRLMesh (void)
		{
			if (m_pUnwrapGroups!=NULL)
				delete[] m_pUnwrapGroups;
		};

		bool	RasterizeGroup_ValidPixel( CRLUnwrapGroup* pGroup,  FILE *pCacheFile, int32 &nCacheFilePosition, const int32 nVisAreaID );
		bool	RasterizeGroup( CRLUnwrapGroup* pGroup, CIlluminanceIntegrator* pDirectIntegrator, CIlluminanceIntegrator* pIndirectIntegrator,
													CIlluminanceIntegrator* pSunIntegrator, CIlluminanceIntegrator* pOcclusionIntegrator,  CIlluminanceIntegrator* pAmbientOcclusionIntegrator,
													CRLWaitProgress* pWaitProgress, FILE *pCacheFile, int32 &nCacheFilePosition, t_pairEntityId *LightmapLightIDs, t_pairEntityId *OcclusionLightIDs, int32 nOcclLightNum, const int32 nVisAreaID, bool bGenerateSpanBuffer );

		bool	CheckValidSize( const f32 fQualityModifier = 1.f );
		bool	RasterizeValidPixels( const f32 fQualityModifier, FILE *pCacheFile, int32 &nCacheFilePosition );
		bool	RasterizeDebugColor( CRLWaitProgress* pWaitProgress, FILE *pCacheFile, int32 &nCacheFilePosition, const bool bGenerateRandomColors, t_pairEntityId &OcclusionLightIDs, int32 nOcclLightNum );
		bool	ReadBack_Debug( CRLWaitProgress* pWaitProgress, FILE *pCacheFile, int32 &nCacheFilePosition );
		bool	RasterizeMesh(CIlluminanceIntegrator* pIntegrator, CIlluminanceIntegrator* pIndirectIntegrator, CIlluminanceIntegrator* pSunIntegrator, CIlluminanceIntegrator* pOcclusionIntegrator, CIlluminanceIntegrator* pAmbientOcclusionIntegrator,
												 CRLWaitProgress* pWaitProgress, FILE *pCacheFile, int32 &nCacheFilePosition, t_pairEntityId *LightmapLightIDs, t_pairEntityId *OcclusionLightIDs, int32 nOcclLightNum, const int32 nVisAreaID );
		CRLUnwrapGroup* GetUnwrapGroups();
		int16 GetUnwrapGroupsNum();
		void	AttachOcclusionLightToList( t_pairEntityId &LightIDs, IVisArea *pVisArea ) const;						///generate the occlusion light list
		void	AttachLightmapLightToList( t_pairEntityId &LightIDs, IVisArea *pVisArea ) const;							///generate the lightmap light list
		void	ReleaseAllGroupSpanBuffer();
		void	ReleaseAllGroup();
		float GetLightmapQuality() const { return m_fLightmapQuality; }																		/// Give back the lightmapqualtiy
		void  SetLightmapQuality( f32 fLightmapQuality ) { m_fLightmapQuality = fLightmapQuality; }				/// Set the lightmapqualtiy

	private:
		// Disable copying
		CRLMesh& operator = (CRLMesh& o) { return *this; };

		void	ExtendCoordinates( float x[3], float y[3] );
		void	CalcOffsetSamples();
		bool	CreateMesh(IRenderMesh* pRenderMesh, Matrix34* pNodeMatrix, CRenderChunk* pRenderChunk, const bool bOpaque, const bool bNeedToAutoGenerateTextureCoordinates);
		bool	NormalizeTexCoords(CRLUnwrapGroup& group);
		void	RasterizeUnwrapGroup(CRLUnwrapGroup& group, CIlluminanceIntegrator* pDirectIntegrator,
																										 CIlluminanceIntegrator* pIndirectIntegrator,
																										 CIlluminanceIntegrator* pSunIntegrator, 
																										 CIlluminanceIntegrator* pOcclusionIntegrator,
																										 CIlluminanceIntegrator* pAmbientOcclusionIntegrator,
																										 const t_pairEntityId &LightmapLightIDs, const t_pairEntityId &OcclusionLightIDs);
	public:
		AABB						 m_BBox;													/// Bounding box for the CRLMesh.

		/* Later maybe usefull variables
		//fix: add runtime references
		//defaul material; 
		ColorF					 m_Ambient;
		ColorF					 m_Diffuse;
		ColorF					 m_Specular;
		ColorF					 m_Emission;
		float						 m_SpecShininess;
		*/
		static bool			 m_bSamplesCache;
		static int			 m_numOffsetSamples;
		static Vec2*		 m_pvOffsetSamples;

	private:
		struct IRenderShaderResources* m_pMaterial;
		CGeomCore*			m_pGeomCore;
		bool						m_bIsFilled;
		struct IRenderShaderResources* m_pShaderResources; //unified material for this RLMesh
		CRLUnwrapGroup* m_pUnwrapGroups;
		int16						m_numUnwrapGroups;
		f32							m_fLightmapQuality;								///scale value per mesh
};

#endif