////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   3denginerender.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: rendering
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "3dEngine.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "terrain_water.h"
#include "partman.h"
#include "DecalManager.h"
#include "SkyLightManager.h"
#include "terrain.h"
#include "CullBuffer.h"
#include "WaterVolumes.h"
#include "LightEntity.h"
#include "FogVolumeRenderNode.h"
#include "ObjectsTree.h"
#include "WaterWaveRenderNode.h"
#include "CloudsManager.h"
#include "MatMan.h"
#include "VolumeObjectRenderNode.h"
#include "CryPath.h"

#ifdef GetCharWidth
#undef GetCharWidth
#endif //GetCharWidth

#if defined(PS3)
//#define INFO_FRAME_COUNTER 1
#endif

#undef DrawText

////////////////////////////////////////////////////////////////////////////////////////
// RenderScene
////////////////////////////////////////////////////////////////////////////////////////

#define DLD_SET_CVAR(VarName,FlagName)\
	int VarName = GetCVars()->VarName;\
	if(!(dwDrawFlags & FlagName))\
	GetCVars()->VarName = 0;\

#define DLD_SET_CVAR_FLOAT(VarName,FlagName)\
  float VarName = GetCVars()->VarName;\
  if(!(dwDrawFlags & FlagName))\
  GetCVars()->VarName = 0;\

#define DLD_RESTORE_CVAR(VarName) \
	GetCVars()->VarName = VarName; \

// for panorama screenshots
class CStitchedImage : public Cry3DEngineBase
{
public:
	CStitchedImage(				C3DEngine&	rEngine, 
									const uint32			dwWidth,
									const uint32			dwHeight,
									const uint32			dwVirtualWidth,
									const uint32			dwVirtualHeight,
									const uint32			dwSliceCount,
									const f32					fTransitionSize,
									const bool				bMetaData=false):
	m_rEngine(rEngine),
	m_dwWidth(dwWidth),
	m_dwHeight(dwHeight),
	m_fInvWidth(1.f/static_cast<f32>(dwWidth)),
	m_fInvHeight(1.f/static_cast<f32>(dwHeight)),
	m_dwVirtualWidth(dwVirtualWidth),
	m_dwVirtualHeight(dwVirtualHeight),
	m_fInvVirtualWidth(1.f/static_cast<f32>(dwVirtualWidth)),
	m_fInvVirtualHeight(1.f/static_cast<f32>(dwVirtualHeight)),
	m_nFileId(0),
	m_dwSliceCount(dwSliceCount),
	m_fHorizFOV(2*g_PI/dwSliceCount),
	m_bFlipY(true),
	m_fTransitionSize(fTransitionSize),
	m_bMetaData(bMetaData)
	{
		assert(dwWidth);
		assert(dwHeight);

		const char *szExtension = m_rEngine.GetCVars()->e_screenshot_file_format->GetString();

		m_bFlipY=(stricmp(szExtension,"tga")==0);

		m_RGB.resize(m_dwWidth*3*m_dwHeight);

		// ratio between width and height defines angle 1 (angle from mid to cylinder edges)
		float fVert1Frac = (2*g_PI*m_dwHeight)/m_dwWidth;

		// slice count defines angle 2
		float fHorizFrac = tan(GetHorizFOVWithBorder()*0.5f);
		float fVert2Frac = 2.0f * fHorizFrac / rEngine.GetRenderer()->GetWidth() * rEngine.GetRenderer()->GetHeight();
//		float fVert2Frac = 2.0f * fHorizFrac / rEngine.GetRenderer()->GetWidth() * rEngine.GetRenderer()->GetHeight();

		// the bigger one defines the needed angle
		float fVertFrac = max(fVert1Frac,fVert2Frac);

		// planar image becomes a barrel after projection and we need to zoom in to only utilize the usable part (inner rect)
		// this is not always needed - for quality with low slice count we could be save some quality here
		fVertFrac /= cos(GetHorizFOVWithBorder()*0.5f);

		// compute FOV from Frac
		float fVertFOV = 2*atan(0.5f*fVertFrac);

		m_fPanoramaShotVertFOV = fVertFOV;

		CryLog("RenderFov = %f degrees (%f = max(%f,%f)*fix)",RAD2DEG(m_fPanoramaShotVertFOV), fVertFrac,fVert1Frac,fVert2Frac);
		Clear();
	}

	void Clear()
	{
		memset(&m_RGB[0],0,m_dwWidth*m_dwHeight*3);
	}

	// szDirectory + "/" + file_id + "." + extension
	// logs errors in the case there are problems
	bool SaveImage( const char *szDirectory )
	{
		assert(szDirectory);

		const char *szExtension = m_rEngine.GetCVars()->e_screenshot_file_format->GetString();

		if(	stricmp(szExtension,"dds")!=0	&&
				stricmp(szExtension,"tga")!=0	&&
				stricmp(szExtension,"jpg")!=0)
		{
			gEnv->pLog->LogError("Format e_screenshot_file_format='%s' not supported",szExtension);
			return false;
		}

		char sFileName[512];

		snprintf(sFileName, sizeof(sFileName), "%s/ScreenShots/%s",PathUtil::GetGameFolder(), szDirectory);
		CryCreateDirectory(sFileName,0);

		// find free file id
		for(;;)
		{
			snprintf(sFileName, sizeof(sFileName), "%s/ScreenShots/%s/%.5d.%s",PathUtil::GetGameFolder(),szDirectory,m_nFileId,szExtension);
	
			FILE * fp = gEnv->pCryPak->FOpen(sFileName,"rb");

			if (!fp)
				break; // file doesn't exist

			gEnv->pCryPak->FClose(fp);
			m_nFileId++;
		}

		bool bOk;

		if(stricmp(szExtension,"dds")==0)
			bOk = gEnv->pRenderer->WriteDDS((byte *)&m_RGB[0],m_dwWidth,m_dwHeight,4,sFileName,eTF_DXT5,1);
		else
		if(stricmp(szExtension,"tga")==0)
			bOk = gEnv->pRenderer->WriteTGA((byte *)&m_RGB[0],m_dwWidth,m_dwHeight,sFileName,24,24);
		else
			bOk = gEnv->pRenderer->WriteJPG((byte *)&m_RGB[0],m_dwWidth,m_dwHeight,sFileName,24);

		if(!bOk)
			gEnv->pLog->LogError("Failed to write '%s' (not supported on this platform?)",sFileName);
		else //write meta data
		{
			if(m_bMetaData)
			{
				const f32	nTSize		=	static_cast<f32>(gEnv->p3DEngine->GetTerrainSize());
	//			const f32	fTLX	=	gEnv->pConsole->GetCVar("e_screenshot_map_topleft_x")->GetFVal()*nTSize;
	//			const f32	fTLY	=	(1.f-gEnv->pConsole->GetCVar("e_screenshot_map_topleft_y")->GetFVal())*nTSize;
	//			const f32	fBRX	=	gEnv->pConsole->GetCVar("e_screenshot_map_bottomright_x")->GetFVal()*nTSize;
	//			const f32	fBRY	=	(1.f-gEnv->pConsole->GetCVar("e_screenshot_map_bottomright_y")->GetFVal())*nTSize;
				const f32	fSizeX	=	GetCVars()->e_screenshot_map_size_x;
				const f32	fSizeY	=	GetCVars()->e_screenshot_map_size_y;
				const f32	fTLX	=	GetCVars()->e_screenshot_map_center_x-fSizeX;
				const f32	fTLY	=	GetCVars()->e_screenshot_map_center_y-fSizeY;
				const f32	fBRX	=	GetCVars()->e_screenshot_map_center_x+fSizeX;
				const f32	fBRY	=	GetCVars()->e_screenshot_map_center_y+fSizeY;

				snprintf(sFileName, sizeof(sFileName), "%s/ScreenShots/%s/%.5d.%s",PathUtil::GetGameFolder(),szDirectory,m_nFileId,"xml");

				FILE * metaFile = gEnv->pCryPak->FOpen(sFileName,"wt");
				if(metaFile)
				{
					snprintf(sFileName, sizeof(sFileName), "<MiniMap Filename=\"%.5d.%s\" startX=\"%f\" startY=\"%f\" endX=\"%f\" endY=\"%f\"/>",
						m_nFileId,szExtension,fTLX,fTLY,fBRX,fBRY);
					string data(sFileName);
					gEnv->pCryPak->FWrite(data.c_str(), data.size(), metaFile);
					gEnv->pCryPak->FClose(metaFile);
				}
			}
		}

		return bOk;
	}

	// rasterize rectangle
	// Arguments:
	//   x0 - <x1, including
	//   y0 - <y1, including
	//   x1 - >x0, excluding
	//   y1 - >y0, excluding
	void RasterizeRect(			const uint32*	pRGBAImage,
													const uint32	dwWidth,
													const uint32	dwHeight,
													const uint32	dwSliceX,
													const uint32	dwSliceY,
													const f32			fTransitionSize,
													const bool		bFadeBordersX,
													const bool		bFadeBordersY)
	{
//		if(bFadeBordersX|bFadeBordersY)
		{
			//calculate rect inside the whole image
			const int32 OrgX0	=	static_cast<int32>(static_cast<f32>((dwSliceX*dwWidth)*m_dwWidth)*m_fInvVirtualWidth);
			const int32 OrgY0	=	static_cast<int32>(static_cast<f32>((dwSliceY*dwHeight)*m_dwHeight)*m_fInvVirtualHeight);
			const int32 OrgX1	=	min(static_cast<int32>(static_cast<f32>(((dwSliceX+1)*dwWidth)*m_dwWidth)*m_fInvVirtualWidth),static_cast<int32>(m_dwWidth))-(m_rEngine.GetCVars()->e_screenshot_debug==1?1:0);
			const int32 OrgY1	=	min(static_cast<int32>(static_cast<f32>(((dwSliceY+1)*dwHeight)*m_dwHeight)*m_fInvVirtualHeight),static_cast<int32>(m_dwHeight))-(m_rEngine.GetCVars()->e_screenshot_debug==1?1:0);
			//expand bounds for borderblending
			const int32 CenterX	=	(OrgX0+OrgX1)/2;
			const int32 CenterY	=	(OrgY0+OrgY1)/2;
			const int32 X0	=	static_cast<int32>(static_cast<f32>(OrgX0-CenterX)*(1.f+fTransitionSize))+CenterX;
			const int32 Y0	=	static_cast<int32>(static_cast<f32>(OrgY0-CenterY)*(1.f+fTransitionSize))+CenterY;
			const int32 X1	=	static_cast<int32>(static_cast<f32>(OrgX1-CenterX)*(1.f+fTransitionSize))+CenterX;
			const int32 Y1	=	static_cast<int32>(static_cast<f32>(OrgY1-CenterY)*(1.f+fTransitionSize))+CenterY;
			//clip bounds -- disabled due to some artifacts on edges
			//X0	=	min(max(X0,0),static_cast<int32>(m_dwWidth-1));
			//Y0	=	min(max(Y0,0),static_cast<int32>(m_dwHeight-1));
			//X1	=	min(max(X1,0),static_cast<int32>(m_dwWidth-1));
			//Y1	=	min(max(Y1,0),static_cast<int32>(m_dwHeight-1));
			const f32 InvBlendX	=	1.f/static_cast<f32>(X1-OrgX1);//0.5 'cause in total the boarder is twices as wide as the boarder of an single segment
			const f32 InvBlendY	=	1.f/static_cast<f32>(Y1-OrgY1);
			const int32 DebugScale=	(m_rEngine.GetCVars()->e_screenshot_debug==2)?65536:0;
			for(int32 y=max(Y0,0);y<Y1 && y<m_dwHeight;y++)
			{
				const f32 WeightY	=	bFadeBordersY?min(1.f,static_cast<f32>(min(y-Y0,Y1-y))*InvBlendY):1.f;
				for(int32 x=max(X0,0);x<X1 && x<m_dwWidth;x++)
				{
					uint8 *pDst = &m_RGB[m_bFlipY?3*(x+(m_dwHeight-y-1)*m_dwWidth):3*(x+y*m_dwWidth)];
					const f32 WeightX	=	bFadeBordersX?min(1.f,static_cast<f32>(min(x-X0,X1-x))*InvBlendX):1.f;
					GetBilinearFilteredBlend(	static_cast<int32>(static_cast<f32>(x-X0)/static_cast<f32>(X1-X0)*static_cast<f32>(dwWidth)*16.f),
																		static_cast<int32>(static_cast<f32>(y-Y0)/static_cast<f32>(Y1-Y0)*static_cast<f32>(dwHeight)*16.f),
																		pRGBAImage,dwWidth,dwHeight,
																		max(static_cast<int32>(WeightX*WeightY*65536.f),DebugScale),pDst);
				}
			}
		}
/*		else
		{
			const int32 X0	=	static_cast<int32>(static_cast<f32>((dwSliceX*dwWidth)*m_dwWidth)*m_fInvVirtualWidth);
			const int32 Y0	=	static_cast<int32>(static_cast<f32>((dwSliceY*dwHeight)*m_dwHeight)*m_fInvVirtualHeight);
			const int32 X1	=	min(static_cast<int32>(static_cast<f32>(((dwSliceX+1)*dwWidth)*m_dwWidth)*m_fInvVirtualWidth),static_cast<int32>(m_dwWidth))-(m_rEngine.GetCVars()->e_screenshot_debug==1?1:0);
			const int32 Y1	=	min(static_cast<int32>(static_cast<f32>(((dwSliceY+1)*dwHeight)*m_dwHeight)*m_fInvVirtualHeight),static_cast<int32>(m_dwHeight))-(m_rEngine.GetCVars()->e_screenshot_debug==1?1:0);
			for(int32 y=Y0;y<Y1;y++)
				for(int32 x=X0;x<X1;x++)
				{
					uint8 *pDst = &m_RGB[m_bFlipY?3*(x+(m_dwHeight-y-1)*m_dwWidth):3*(x+y*m_dwWidth)];
					GetBilinearFiltered(static_cast<int32>(static_cast<f32>(x*m_dwVirtualWidth*16)*m_fInvWidth)-((dwSliceX*dwWidth)*16),
																	static_cast<int32>(static_cast<f32>(y*m_dwVirtualHeight*16)*m_fInvHeight)-((dwSliceY*dwHeight)*16),
																	pRGBAImage,dwWidth,dwHeight,pDst);
				}
		}*/
	}

	void RasterizeCylinder( const uint32 *pRGBAImage, 
													const uint32 dwWidth,
													const uint32 dwHeight, 
													const uint32 dwSlice, 
													const bool bFadeBorders)
	{
		float fSrcAngleMin = GetSliceAngle(dwSlice-1);
		float fFractionVert = tan(m_fPanoramaShotVertFOV*0.5f);
		float fFractionHoriz = fFractionVert * gEnv->pRenderer->GetCamera().GetProjRatio();
		float fInvFractionHoriz = 1.0f / fFractionHoriz;
		float fCameraHorizFov = atan(fFractionHoriz)*2;

		// for soft transition
		float fFadeOutFov = GetHorizFOVWithBorder();
		float fFadeInFov = GetHorizFOV();

		int x0=0, y0=0, x1=m_dwWidth, y1=m_dwHeight;

		float fScaleX = 1.0f/m_dwWidth;
		float fScaleY = 0.5f*fInvFractionHoriz/(m_dwWidth/(2*g_PI))/dwHeight*dwWidth;								// this value is not correctly computed yet - but using many slices reduced the problem

		if(m_bFlipY)
			fScaleY=-fScaleY;


		// it's more efficient to process colums than lines
		for(int x=x0;x<x1;++x)
		{
			uint8 *pDst = &m_RGB[3*(x+y0*m_dwWidth)];
			float fSrcX = x*fScaleX-0.5f;		// -0.5 .. 0.5
			float fSrcAngleX = fSrcAngleMin+2*g_PI*fSrcX;

			if(fSrcAngleX>g_PI)
				fSrcAngleX-=2*g_PI;
			if(fSrcAngleX<-g_PI)
				fSrcAngleX+=2*g_PI;

			if(fabs(fSrcAngleX)>fFadeOutFov*0.5f)
				continue;															// clip away curved parts of the barrel

			float fScrPosX = (tan(fSrcAngleX) * 0.5f * fInvFractionHoriz + 0.5f) * dwWidth;
//			float fInvCosSrcX = 1.0f / cos(fSrcAngleX);
			float fInvCosSrcX = 1.0f / cos(fSrcAngleX);

			if(fScrPosX>=0 && fScrPosX<=(float)dwWidth)		// this is an optimization - but it could be done even more efficient
			if(fInvCosSrcX>0)															// don't render the viewer opposing direction
			{
				int iSrcPosX16 = (int)(fScrPosX*16.0f);

				float fYOffset = 16*0.5f*dwHeight - 16*0.5f*m_dwHeight*fScaleY*fInvCosSrcX*dwHeight;
				float fYMul = 16*fScaleY*fInvCosSrcX*dwHeight;

				float fSrcY = y0*fYMul+fYOffset;

				uint32 dwLerp64k = 256*256-1;

				if(!bFadeBorders)
				{
					// first pass - every second image without soft borders
					for(int y=y0;y<y1;++y,fSrcY+=fYMul,pDst+=m_dwWidth*3)
						GetBilinearFiltered(iSrcPosX16,(int)fSrcY,pRGBAImage,dwWidth,dwHeight,pDst);
				}
				else
				{
					// second pass - do all the inbetween with soft borders
					float fOffSlice = fabs(fSrcAngleX/fFadeInFov)-0.5f;					
				
					if(fOffSlice<0)
						fOffSlice=0;			// no transition in this area

					float fBorder = (fFadeOutFov-fFadeInFov)*0.5f;
					
					if(fBorder<0.001f)
						fBorder=0.001f;		// we do not have border
					
					float fFade = 1.0f-fOffSlice*fFadeInFov/fBorder;

					if(fFade<0.0f)
						fFade=0.0f;				// don't use this slice here

					dwLerp64k = (uint32)(fFade*(256.0f*256.0f-1.0f));		// 0..64k

					if(dwLerp64k)															// optimization
						for(int y=y0;y<y1;++y,fSrcY+=fYMul,pDst+=m_dwWidth*3)
							GetBilinearFilteredBlend(iSrcPosX16,(int)fSrcY,pRGBAImage,dwWidth,dwHeight,dwLerp64k,pDst);
				}
			}
		}
	}

	// fast, rgb only
	static inline ColorB lerp( const ColorB x, const ColorB y, const uint32 a, const uint32 dwBase )
	{
		const int32 b = dwBase-a;
		const int32	RC	=	dwBase/2;	//rounding correction


		return ColorB(((int)x.r*b+(int)y.r*a+RC)/dwBase,
									((int)x.g*b+(int)y.g*a+RC)/dwBase,
									((int)x.b*b+(int)y.b*a+RC)/dwBase);
	}

	static inline ColorB Mul( const ColorB x, const int32 a,const int32 dwBase )
	{
		return ColorB(((int)x.r*(int)a)/dwBase,
									((int)x.g*(int)a)/dwBase,
									((int)x.b*(int)a)/dwBase);
	}
	static inline ColorB MadSaturate( const ColorB x, const int32 a,const int32 dwBase , const ColorB y )
	{
		const int32 MAX_COLOR	=	0xff;
		const ColorB PreMuled	=	Mul(x,a,dwBase);
		return ColorB(min((int)PreMuled.r+(int)y.r,MAX_COLOR),
									min((int)PreMuled.g+(int)y.g,MAX_COLOR),
									min((int)PreMuled.b+(int)y.b,MAX_COLOR));
	}

	// bilinear filtering in fixpoint,
	// 4bit fractional part -> multiplier 16 
	// --lookup outside of the image is not defined
	//	lookups outside the image are now clamped, needed due to some float inaccuracy while rasterizing a rect-screenshot
	// Arguments:
	//   iX16 - fX mul 16
	//   iY16 - fY mul 16
	//   result - [0]=red, [1]=green, [2]=blue
	static inline bool GetBilinearFilteredRaw(	const int iX16, const int iY16, 
																							const uint32 *pRGBAImage,
																							const uint32 dwWidth, const uint32 dwHeight,
																							ColorB& result)
	{
		int iLocalX = min(max(iX16/16,0),static_cast<int>(dwWidth-1));
		int iLocalY = min(max(iY16/16,0),static_cast<int>(dwHeight-1));

		int iLerpX = iX16&0xf;			// 0..15
		int iLerpY = iY16&0xf;			// 0..15

		ColorB colS[4];

		const uint32 *pRGBA = &pRGBAImage[iLocalX+iLocalY*dwWidth];

		colS[0] = pRGBA[0];
		colS[1] = pRGBA[1];
		colS[2] = pRGBA[iLocalY+1<dwHeight?dwWidth:0];
		colS[3] = pRGBA[(iLocalX+1<dwWidth?1:0)+(iLocalY+1<dwHeight?dwWidth:0)];

		ColorB colTop,colBottom;
		
		colTop = lerp(colS[0],colS[1],iLerpX,16);
		colBottom = lerp(colS[2],colS[3],iLerpX,16);

		result = lerp(colTop,colBottom,iLerpY,16);
		return true;
	}


	// blend with background
	static inline bool GetBilinearFiltered(	const int iX16, const int iY16, 
																					const uint32 *pRGBAImage,
																					const uint32 dwWidth, const uint32 dwHeight,
																					uint8 result[3] )
	{
		ColorB colFiltered;
		if(GetBilinearFilteredRaw(iX16,iY16,pRGBAImage,dwWidth,dwHeight,colFiltered))
		{
			result[0]=colFiltered.r;
			result[1]=colFiltered.g;
			result[2]=colFiltered.b;
			return true;
		}
		return false;
	}

	static inline bool GetBilinearFilteredBlend(const int iX16, const int iY16, 
																							const uint32 *pRGBAImage,
																							const uint32 dwWidth, const uint32 dwHeight,
																							const uint32 dwLerp64k, 
																							uint8 result[3] )
	{
		ColorB colFiltered;
		if(GetBilinearFilteredRaw(iX16,iY16,pRGBAImage,dwWidth,dwHeight,colFiltered))
		{
			ColorB colRet = lerp(ColorB(result[0],result[1],result[2]),colFiltered,dwLerp64k,256*256);

			result[0]=colRet.r;
			result[1]=colRet.g;
			result[2]=colRet.b;
			return true;
		}
		return false;
	}

	static inline bool GetBilinearFilteredAdd(const int iX16, const int iY16, 
																						const uint32 *pRGBAImage,
																						const uint32 dwWidth, const uint32 dwHeight,
																						const uint32 dwLerp64k, 
																						uint8 result[3] )
	{
		ColorB colFiltered;
		if(GetBilinearFilteredRaw(iX16,iY16,pRGBAImage,dwWidth,dwHeight,colFiltered))
		{
			ColorB colRet = MadSaturate(colFiltered,dwLerp64k,256*256,ColorB(result[0],result[1],result[2]));

			result[0]=colRet.r;
			result[1]=colRet.g;
			result[2]=colRet.b;
			return true;
		}
		return false;
	}


	float GetSliceAngle( const uint32 dwSlice ) const
	{
		uint32 dwAlternatingSlice = (dwSlice*2)%m_dwSliceCount;

		float fAngleStep = m_fHorizFOV;

		float fRet = fAngleStep*dwAlternatingSlice;

		if(dwSlice*2>=m_dwSliceCount)
			fRet += fAngleStep;

		return fRet;
	}

	float GetHorizFOV() const
	{
		return m_fHorizFOV;
	}

	float GetHorizFOVWithBorder() const
	{
		return m_fHorizFOV*(1.0f+m_fTransitionSize);
	}

	//private: // -------------------------------------------------------------------

	uint32									m_dwWidth;									// >0
	uint32									m_dwHeight;									// >0
	f32											m_fInvWidth;								// >0
	f32											m_fInvHeight;								// >0
	uint32									m_dwVirtualWidth;						// >0
	uint32									m_dwVirtualHeight;					// >0
	f32											m_fInvVirtualWidth;					// >0
	f32											m_fInvVirtualHeight;				// >0
	std::vector<uint8>			m_RGB;											// [channel + x*3 + m_dwWidth*3*y], channel=0..2, x<m_dwWidth, y<m_dwHeight, no alpha channel to occupy less memory
	uint32									m_nFileId;									// counts up until it finds free file id
	bool										m_bFlipY;										// might be useful for some image formats
	bool										m_bMetaData;								// output additional metadata

	float										m_fPanoramaShotVertFOV;			// -1 means not set yet - in radians

private:

	uint32									m_dwSliceCount;							//
	C3DEngine &							m_rEngine;									//
	float										m_fHorizFOV;								// - in radians
	float										m_fTransitionSize;					// [0..1], 0=no transition, 1.0=full transition
};

enum EScreenShotType
{
	ESST_NONE			=	0,
	ESST_HIGHRES	=	1,
	ESST_PANORAMA,
	ESST_MAP_DELAYED,
	ESST_MAP,
};

void C3DEngine::ScreenshotDispatcher(const int nRenderFlags, const CCamera &_cam, const int dwDrawFlags, const int nFilterFlags)
{
	CStitchedImage*	pStitchedImage=0;
	const uint32	dwPanWidth			= max(1,GetCVars()->e_screenshot_width);
	const uint32	dwPanHeight			= max(1,GetCVars()->e_screenshot_height);
	const f32			fTransitionSize = min(1.f,abs(GetCVars()->e_screenshot_quality) * 0.01f);
	const uint32	MinSlices				=	max(GetCVars()->e_screenshot_min_slices,
																			max(	static_cast<int>((dwPanWidth+GetRenderer()->GetWidth()-1)/GetRenderer()->GetWidth()),
																						static_cast<int>((dwPanHeight+GetRenderer()->GetHeight()-1)/GetRenderer()->GetHeight())));
	const uint32	dwVirtualWidth	=	GetRenderer()->GetWidth()*MinSlices;
	const uint32	dwVirtualHeight	=	GetRenderer()->GetHeight()*MinSlices;

	switch(abs(GetCVars()->e_screenshot))
	{
		case ESST_HIGHRES:
  			GetConsole()->ShowConsole(false);
				pStitchedImage	=	new CStitchedImage(*this,dwPanWidth,dwPanHeight,dwVirtualWidth,dwVirtualHeight,MinSlices,fTransitionSize);
				ScreenShotHighRes(pStitchedImage,nRenderFlags, _cam, dwDrawFlags, nFilterFlags,MinSlices,fTransitionSize);
				pStitchedImage->SaveImage("HiRes");
				pStitchedImage->Clear();		// good for debugging
				delete pStitchedImage;
				if(GetCVars()->e_screenshot>0)			// <0 is used for multiple frames (videos)
					GetCVars()->e_screenshot=0;	
			break;
		case ESST_PANORAMA:
				GetConsole()->ShowConsole(false);
				pStitchedImage	=	new CStitchedImage(*this,dwPanWidth,dwPanHeight,dwVirtualWidth,dwVirtualHeight,MinSlices,fTransitionSize);
				ScreenShotPanorama(pStitchedImage,nRenderFlags,_cam,dwDrawFlags,nFilterFlags,MinSlices,fTransitionSize);
				pStitchedImage->SaveImage("Panorama");
				pStitchedImage->Clear();		// good for debugging
				delete pStitchedImage;
				if(GetCVars()->e_screenshot>0)			// <0 is used for multiple frames (videos)
					GetCVars()->e_screenshot=0;	
			break;
		case ESST_MAP_DELAYED:
			{
				GetCVars()->e_screenshot	=	sgn(GetCVars()->e_screenshot)*ESST_MAP;		// sgn() to keep sign bit , <0 is used for multiple frames (videos)
        GetTerrain()->ResetTerrainVertBuffers();
			}
			break;
		case ESST_MAP:
			{
				GetRenderer()->ChangeViewport(0,0,512,512);
				uint TmpHeight,TmpWidth,TmpVirtualHeight,TmpVirtualWidth;
				TmpHeight=TmpWidth=TmpVirtualHeight=TmpVirtualWidth=1;

				while((TmpHeight<<1)<=dwPanHeight)
				{
					TmpHeight<<=1;
				}
				while((TmpWidth<<1)<=dwPanWidth)
				{
					TmpWidth<<=1;
				}
				const uint32	TmpMinSlices				=	max(GetCVars()->e_screenshot_min_slices,
																								max(	static_cast<int>((TmpWidth+512-1)/512),
																											static_cast<int>((TmpHeight+512-1)/512)));
				while((TmpVirtualHeight<<1)<=TmpMinSlices*512)
				{
					TmpVirtualHeight<<=1;
				}
				while((TmpVirtualWidth<<1)<=TmpMinSlices*512)
				{
					TmpVirtualWidth<<=1;
				}

				GetConsole()->ShowConsole(false);
				pStitchedImage	=	new CStitchedImage(*this,TmpWidth,TmpHeight,TmpVirtualWidth,TmpVirtualHeight,TmpMinSlices,fTransitionSize,true);
				ScreenShotMap(pStitchedImage,nRenderFlags, _cam, dwDrawFlags, nFilterFlags,TmpMinSlices,fTransitionSize);
				pStitchedImage->SaveImage("Map");
				pStitchedImage->Clear();		// good for debugging
				delete pStitchedImage;
			}
			if(GetCVars()->e_screenshot>0)			// <0 is used for multiple frames (videos)
				GetCVars()->e_screenshot=0;	

      GetTerrain()->ResetTerrainVertBuffers();
			break;
		default:
			GetCVars()->e_screenshot=0;
	}
}



struct SDebugFrustrum
{
	Vec3						m_vPos[8];
	const char *		m_szName;
	CTimeValue			m_TimeStamp;
	ColorB					m_Color;
	float						m_fQuadDist;		// < 0 if not used
};

static std::vector<SDebugFrustrum> g_DebugFrustrums;

void C3DEngine::DebugDraw_Draw()
{
	CTimeValue CurrentTime = GetISystem()->GetITimer()->GetFrameStartTime();

	IRenderAuxGeom *pAux = GetRenderer()->GetIRenderAuxGeom();

	SAuxGeomRenderFlags	oldFlags = pAux->GetRenderFlags();
	SAuxGeomRenderFlags	newFlags;
	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	newFlags.SetCullMode(e_CullModeNone);
	newFlags.SetDepthWriteFlag(e_DepthWriteOff);
	pAux->SetRenderFlags(newFlags);
	std::vector<SDebugFrustrum>::iterator it;
	
	for(it=g_DebugFrustrums.begin();it!=g_DebugFrustrums.end();)
	{
		SDebugFrustrum &ref = *it;

		float fRatio = (CurrentTime-ref.m_TimeStamp).GetSeconds()*2.0f;

		if(fRatio>1.0f)
		{
			it = g_DebugFrustrums.erase(it);
			continue;
		}

		uint16 pnInd[8] = {	0,4,1,5, 2,6,3,7	};

		float fRadius = ((ref.m_vPos[0]+ref.m_vPos[1]+ref.m_vPos[2]+ref.m_vPos[3])-(ref.m_vPos[4]+ref.m_vPos[5]+ref.m_vPos[6]+ref.m_vPos[7])).GetLength()*0.25f;
		float fDistance = min(fRadius,33.0f);		// in meters

		float fRenderRatio = fRatio * fDistance / fRadius;

		if(ref.m_fQuadDist>0)
			fRenderRatio = ref.m_fQuadDist/fRadius;

		Vec3 vPos[4];

		for(uint32 i=0;i<4;++i)
			vPos[i]=ref.m_vPos[i]*fRenderRatio + ref.m_vPos[i+4]*(1.0f-fRenderRatio);

		Vec3 vMid = (vPos[0]+vPos[1]+vPos[2]+vPos[3])*0.25f;

		ColorB col = ref.m_Color;

		if(ref.m_fQuadDist<=0)
		{
			for(uint32 i=0;i<4;++i)
				vPos[i]=vPos[i]*0.95f + vMid*0.05f;

			// quad
			pAux->DrawTriangle(vPos[0],col,vPos[2],col,vPos[1],col);
			pAux->DrawTriangle(vPos[2],col,vPos[0],col,vPos[3],col);
			// projection lines
			pAux->DrawLines(ref.m_vPos, 8, pnInd, 2, RGBA8(0xff,0xff,0x1f,0xff));
			pAux->DrawLines(ref.m_vPos, 8, pnInd+2, 2, RGBA8(0xff,0xff,0x1f,0xff));
			pAux->DrawLines(ref.m_vPos, 8, pnInd+4, 2, RGBA8(0xff,0xff,0x1f,0xff));
			pAux->DrawLines(ref.m_vPos, 8, pnInd+6, 2, RGBA8(0xff,0xff,0x1f,0xff));
		}
		else
		{
			// rectangle
			pAux->DrawPolyline(vPos, 4, true, RGBA8(0xff,0xff,0x1f,0xff));
		}

		++it;
	}

	pAux->SetRenderFlags(oldFlags);
}


void C3DEngine::DebugDraw_PushFrustrum( const char *szName, const CRenderCamera &rCam, const ColorB col, const float fQuadDist )
{
	if(GetCVars()->e_debug_draw!=8 && GetCVars()->e_debug_draw!=9)
		return;

	Vec3 pvFrust[8];

	rCam.CalcVerts(pvFrust);

	SDebugFrustrum one;

	one.m_szName=szName;
	one.m_Color=col;
	one.m_TimeStamp=GetISystem()->GetITimer()->GetFrameStartTime();
	one.m_fQuadDist=fQuadDist;

	one.m_vPos[0]=pvFrust[4];
	one.m_vPos[1]=pvFrust[5];
	one.m_vPos[2]=pvFrust[6];
	one.m_vPos[3]=pvFrust[7];

	one.m_vPos[4]=pvFrust[0];
	one.m_vPos[5]=pvFrust[1];
	one.m_vPos[6]=pvFrust[2];
	one.m_vPos[7]=pvFrust[3];

	g_DebugFrustrums.push_back(one);
}


void C3DEngine::DebugDraw_PushFrustrum( const char *szName, const CCamera &rCam, const ColorB col, const float fQuadDist )
{
	if(GetCVars()->e_debug_draw!=8 && GetCVars()->e_debug_draw!=9)
		return;
	
	SDebugFrustrum one;

	one.m_szName=szName;
	one.m_Color=col;
	one.m_fQuadDist=fQuadDist;
	one.m_TimeStamp=GetISystem()->GetITimer()->GetFrameStartTime();
	rCam.GetFrustumVertices(one.m_vPos);

	g_DebugFrustrums.push_back(one);
}


void C3DEngine::RenderWorld(const int nRenderFlags, const CCamera &_cam, const char *szDebugName, const int dwDrawFlags, const int nFilterFlags)
{
	assert(szDebugName);

	if (!GetCVars()->e_render)
		return;

	FUNCTION_PROFILER_3DENGINE;

	if(m_nRenderStackLevel<0)
	{
		ScreenshotDispatcher(nRenderFlags,_cam,dwDrawFlags,nFilterFlags);

		if(GetCVars()->e_default_material)
		{
			_smart_ptr<IMaterial> pMat = GetMaterialManager()->LoadMaterial("Materials/material_layers_default");
      _smart_ptr<IMaterial> pTerrainMat = GetMaterialManager()->LoadMaterial("Materials/material_terrain_default");
			GetRenderer()->SetDefaultMaterials(pMat,pTerrainMat);
		}
		else
			GetRenderer()->SetDefaultMaterials(NULL,NULL);
	}

	RenderInternal(nRenderFlags,_cam,szDebugName,dwDrawFlags,nFilterFlags); 

	if(GetCVars()->e_debug_draw)
	{
		f32 fColor[4] = {1,1,0,1};
		
		float fYLine=8.0f, fYStep=20.0f;

		GetRenderer()->Draw2dLabel(8.0f,fYLine+=fYStep,2.0f,fColor,false,"e_debug_draw = %d",GetCVars()->e_debug_draw);

		char *szMode = "";

		switch(GetCVars()->e_debug_draw)
		{
			case  1:	szMode="name of the used cgf, polycount, used LOD";break;
			case  2:	szMode="color coded polygon count";break;
			case  3:	szMode="show color coded LODs count, flashing color indicates LOD";break;
			case  4:	szMode="object texture memory usage in KB";break;
			case  5:	szMode="number of render materials (color coded)";break;
			case  6:	szMode="ambient color (R,G,B,A)";break;
			case  7:	szMode="triangle count, number of render materials, texture memory in KB";break;
			case  8:	szMode="RenderWorld statistics (with view cones)";break;
			case  9:	szMode="RenderWorld statistics (with view cones without lights)";break;
			case 10:	szMode="geometry with simple lines and triangles";break;
			case 11:	szMode="occlusion geometry additionally";break;
			case 12:	szMode="occlusion geometry without render geometry";break;
			case 13:	szMode="occlusion amount (used during AO computations)";break;
//			case 14:	szMode="";break;
			case 15:	szMode="display helpers";break;

			default: assert(0);
		}

		GetRenderer()->Draw2dLabel(8.0f,fYLine+=fYStep,2.0f,fColor,false,"   %s",szMode);
	}

  static bool bOutputLoadingTimeStats = true;
  if(bOutputLoadingTimeStats)
  {
    OutputLoadingTimeStats();
    bOutputLoadingTimeStats = false;
  }
}

void C3DEngine::RenderInternal(const int nRenderFlags, const CCamera &_cam, const char *szDebugName, const int dwDrawFlags, const int nFilterFlags)
{
	m_bProfilerEnabled = GetISystem()->GetIProfileSystem()->IsProfiling();

  // test if recursion causes problems
	if(m_nRenderStackLevel>=0 && !GetCVars()->e_recursion)
		return;

	// clamp to prevent cheating (implementation could be improved)
	GetCVars()->e_lod_min = clamp_tpl(GetCVars()->e_lod_min,0,2);
	GetCVars()->e_lod_ratio = clamp_tpl(GetCVars()->e_lod_ratio,3.0f,6.0f);

	// Update particle system as late as possible, only renderer is dependent on it.
	if(m_nRenderStackLevel == -1 && m_pPartManager)
		m_pPartManager->Update();

	if (m_nRenderStackLevel == -1 && GetCVars()->e_clouds)
	{
		if (m_pCloudsManager)
			m_pCloudsManager->MoveClouds();

		CVolumeObjectRenderNode::MoveVolumeObjects();
	}

	m_nRenderStackLevel++;

	if(m_nRenderStackLevel!=0)		// record all except main rendering
		DebugDraw_PushFrustrum(szDebugName,_cam,ColorB(0xff,0xff,0x1f,0x50));

	m_nRenderFrameID = GetRenderer()->GetFrameID();
	m_nRenderMainFrameID = GetRenderer()->GetFrameID(false);
	m_fZoomFactor = 0.001f + 0.999f*(RAD2DEG(_cam.GetFov())/60.f);
	m_pObjManager->m_fMaxViewDistanceScale = 1.f;
	m_pObjManager->m_fGSMMaxDistance = GetCVars()->e_gsm_range * pow(GetCVars()->e_gsm_range_step, 
    GetCVars()->e_gsm_lods_num - (GetCVars()->e_shadows_from_terrain_in_all_lods ?  1 : 0));
	
	assert((UINT_PTR)m_nRenderStackLevel == (UINT_PTR)GetRenderer()->EF_Query(EFQ_RecurseLevel));

	if(m_nRenderStackLevel<0 || m_nRenderStackLevel>=MAX_RECURSION_LEVELS)
  { 
    assert(!"Recursion depther than MAX_RECURSION_LEVELS is not supported"); 	
    m_nRenderStackLevel--;
    return; 
  }

	// set cvars from flags
	DLD_SET_CVAR(e_shadows,DLD_SHADOW_MAPS);
	DLD_SET_CVAR(e_brushes,DLD_STATIC_OBJECTS);
	DLD_SET_CVAR(e_vegetation,DLD_STATIC_OBJECTS);
	DLD_SET_CVAR(e_entities,DLD_ENTITIES);
	DLD_SET_CVAR(e_terrain,DLD_TERRAIN);
	DLD_SET_CVAR(e_water_ocean,DLD_TERRAIN_WATER);
	DLD_SET_CVAR(e_particles,DLD_PARTICLES);	
	DLD_SET_CVAR(e_decals,DLD_DECALS);	
  DLD_SET_CVAR(e_detail_materials,DLD_DETAIL_TEXTURES);	
  float fOldZoomFactor = m_fZoomFactor;

	if(m_nRenderStackLevel)
	{
		GetCVars()->e_shadows = 0;
    m_fZoomFactor /= GetCVars()->e_recursion_view_dist_ratio;
	}

  if(GetCVars()->e_default_material)
  {
    GetCVars()->e_decals = 0;
  }

	CCamera oldCamera = GetCamera();

	SetupCamera(_cam);

  // force preload terrain data if camera was teleported more than 32 meters
  if(m_nRenderStackLevel==0)
  {
    if(GetCVars()->e_level_auto_precache_camera_jump_dist && m_vPrevMainFrameCamPos.GetDistance(GetCamera().GetPosition()) > GetCVars()->e_level_auto_precache_camera_jump_dist)
    {
      PrintMessage("Preparing textures, procedural objects and shaders for position (%.1f,%.1f,%.1f)",
        GetCamera().GetPosition().x, GetCamera().GetPosition().y, GetCamera().GetPosition().z);
      m_bContentPrecacheRequested = true;
    }

    m_vPrevMainFrameCamPos = GetCamera().GetPosition();
  }

	if(!m_nRenderStackLevel)
		UpdateScene();

  CheckPhysicalized(GetCamera().GetPosition()-Vec3(2,2,2),GetCamera().GetPosition()+Vec3(2,2,2));

	if(m_pObjectsTree)
		m_pObjectsTree->UpdateMergingTimeLimit();

	RenderScene(nRenderFlags, GetSystem()->IsDedicated() ? 0 : dwDrawFlags, nFilterFlags);

	// c-buffer debug info
  if(m_pObjManager && m_pCoverageBuffer && GetCVars()->e_cbuffer_debug)
  {
    m_pCoverageBuffer->DrawDebug(GetCVars()->e_cbuffer_debug);
  }

	DLD_RESTORE_CVAR(e_shadows);
	DLD_RESTORE_CVAR(e_brushes);
	DLD_RESTORE_CVAR(e_vegetation);
	DLD_RESTORE_CVAR(e_entities);
	DLD_RESTORE_CVAR(e_terrain);
	DLD_RESTORE_CVAR(e_water_ocean);
	DLD_RESTORE_CVAR(e_particles);	
	DLD_RESTORE_CVAR(e_decals);	
  DLD_RESTORE_CVAR(e_detail_materials);	
  m_fZoomFactor = fOldZoomFactor;
  
	// Unload old stuff
  if(GetCVars()->e_stream_areas)
  {
    if(m_pTerrain)
      m_pTerrain->CheckUnload();
  //  if(m_pVisAreaManager)
//      m_pVisAreaManager->CheckUnload();
  }

	if(m_pObjManager && GetCVars()->e_stream_cgf)
		m_pObjManager->CheckUnload();
/*	
	if(0 && GetCVars()->e_lm_gen_debug)
	{ // light-map map generation debug
//		uchar * pImage = new uchar[GetCVars()->e_lm_gen_debug*GetCVars()->e_lm_gen_debug*3];

		if(CTerrainNode * pSectorInfo = m_pTerrain ? m_pTerrain->GetSecInfo(GetCamera().GetPosition()) : 0)
			if(pSectorInfo = pSectorInfo->GetTexSourceNode(false))
		{
			GetTerrain()->RenderAreaShadowsIntoTexture(pSectorInfo->m_nOriginX, pSectorInfo->m_nOriginY, 
				GetTerrain()->GetSectorSize()<<pSectorInfo->m_nTreeLevel, GetCVars()->e_lm_gen_debug);
			GetCVars()->e_lm_gen_debug = 0;
		}

//		delete [] pImage;
	}
*/

/*
	int nCount=0;
	GetVoxelRenderMeshes(NULL, nCount);
	IRenderMesh ** pNodes = new IRenderMesh * [nCount];
	nCount=0;
	GetVoxelRenderMeshes(pNodes, nCount);
	delete [] pNodes;
*/

	if(m_nRenderStackLevel)
		SetCamera(oldCamera);

  if(!m_nRenderStackLevel)
  {
    m_bContentPrecacheRequested = false;

    COctreeNode::ReleaseEmptyNodes();
  }

	m_nRenderStackLevel--;
}

int __cdecl C3DEngine__Cmp_SRNInfo(const void* v1, const void* v2)
{
	SRNInfo * p1 = (SRNInfo*)v1;
	SRNInfo * p2 = (SRNInfo*)v2;

	float fViewDist1 = p1->fMaxViewDist - p1->aabbBox.GetRadius();
	float fViewDist2 = p2->fMaxViewDist - p2->aabbBox.GetRadius();

	// if same - give closest sectors higher priority
	if(fViewDist1 > fViewDist2)
		return 1;
	else if(fViewDist1 < fViewDist2)
		return -1;

	return 0;
}

bool C3DEngine::RenderVisAreaPotentialOccluders(CVisArea * pThisArea, CCullBuffer & rCB, const CCamera & viewCam, bool bResetAffectedLights)
{
	bool bOccludersFound = false;

	// render objects in current area/portal
	if(pThisArea->m_pObjectsTree)
		bOccludersFound |= pThisArea->m_pObjectsTree->Render(viewCam, rCB, OCTREENODE_RENDER_FLAG_OCCLUDERS);

	for(int i=0; i<pThisArea->m_lstConnections.Count(); i++)
	{
		if(CVisArea * pNeibArea = pThisArea->m_lstConnections[i])
		{
			// render objects in neighbor area/portal
			if(pNeibArea->m_pObjectsTree)
				bOccludersFound |= pNeibArea->m_pObjectsTree->Render(viewCam, rCB, OCTREENODE_RENDER_FLAG_OCCLUDERS);

			// render objects in neighbor of neighbor area/portal
			for(int i=0; i<pNeibArea->m_lstConnections.Count(); i++)
				if(pNeibArea->m_lstConnections[i] != pThisArea && pNeibArea->m_lstConnections[i]->m_pObjectsTree)
					bOccludersFound |= pNeibArea->m_lstConnections[i]->m_pObjectsTree->Render(viewCam, rCB, OCTREENODE_RENDER_FLAG_OCCLUDERS);
		}
	}

	return bOccludersFound;
}

bool C3DEngine::RenderPotentialOccluders(CCullBuffer & rCB, const CCamera & viewCam, bool bResetAffectedLights)
{
	FUNCTION_PROFILER_3DENGINE;

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	assert(&rCB == GetCoverageBuffer() || 1);

	bool bOccludersFound = false;

	if(GetCVars()->e_cbuffer_hw)
	{
		PodArray<SRNInfo> lstStaticsAround;

		// indoors
		PodArray<CVisArea*> * pVisibleAreas = &m_pVisAreaManager->m_lstVisibleAreas;
		for(int i=0; i<pVisibleAreas->Count(); i++)
			if(COctreeNode * pTree = pVisibleAreas->GetAt(i)->m_pObjectsTree)
				pTree->MoveObjectsIntoList(&lstStaticsAround, NULL, false, true, false);

		if(m_pTerrain)
			m_pTerrain->GetObjectsAround(viewCam.GetPosition(), 32.f, &lstStaticsAround);

		Vec3 vCamPos = viewCam.GetPosition();

		int nTrisWriten = 0;

		PodArray<IRenderNode*> lstOccluders;
		for(int i=0; i<lstStaticsAround.Count(); i++)
		{
			IRenderNode * pObj = lstStaticsAround[i].pNode;
			if(pObj->GetRndFlags()&ERF_GOOD_OCCLUDER)
			{
				float fEntDistance = cry_sqrtf(Distance::Point_AABBSq(vCamPos,pObj->GetBBox()))*m_fZoomFactor;
				assert(fEntDistance>=0 && _finite(fEntDistance));
				if(fEntDistance < pObj->m_fWSMaxViewDist*GetCVars()->e_cbuffer_occluders_view_dist_ratio)
					if(viewCam.IsAABBVisible_EM( pObj->GetBBox() ))
					{
						lstOccluders.Add(pObj);
						IRenderMesh * pMesh = pObj->GetRenderMesh(0);
						if(pMesh)
							nTrisWriten += pMesh->GetSysIndicesCount()/3;
					}
			}
		}

		ICVar * p_r_PolygonMode = GetConsole()->GetCVar("r_PolygonMode");
		int nOldPolygonMode = p_r_PolygonMode->GetIVal();

		p_r_PolygonMode->Set(0);

#ifdef OCCLUSIONCULLER_V
		GetRenderer()->RenderOccludersIntoBuffer(viewCam, 
			rCB.m_farrBuffer.GetSize(), lstOccluders, rCB.m_farrBuffer.GetData());
#endif
		p_r_PolygonMode->Set(nOldPolygonMode);

		bOccludersFound = lstOccluders.Count()>0;

		rCB.TrisWritten(nTrisWriten);
		rCB.ObjectsWritten(lstOccluders.Count());
	}
	else
	{
		int old_e_shadows = GetCVars()->e_shadows;
		GetCVars()->e_shadows = 0;

		if(GetCVars()->e_hw_occlusion_culling_objects)
		{
			ColorF GreenYellow(Col_GreenYellow);
			GetRenderer()->ClearBuffer(FRT_CLEAR, &GreenYellow);
			GetRenderer()->EnableFog(0);		
			GetRenderer()->EF_StartEf();  
		}

		if(m_pVisAreaManager)
		{
			bool bCameraInIndoor = m_pVisAreaManager->m_pCurArea || m_pVisAreaManager->m_pCurPortal;

			if(bCameraInIndoor)
			{ // indoors
				PodArray<CVisArea*> * pVisibleAreas = &m_pVisAreaManager->m_lstVisibleAreas;
				for(int i=0; i<pVisibleAreas->Count(); i++)
					if(CVisArea * pArea = pVisibleAreas->GetAt(i))
						if(pArea->m_pObjectsTree)
							for(int c=0; c<pArea->m_lstCurCameras.Count(); c++)
								bOccludersFound |= pArea->m_pObjectsTree->Render(pArea->m_lstCurCameras[c], rCB, OCTREENODE_RENDER_FLAG_OCCLUDERS);
			}

			// outdoor
			if (m_pObjectsTree && IsOutdoorVisible())  	
			{	
				CCamera outdoorViewCam = GetCamera();
				if (m_pVisAreaManager->m_lstOutdoorPortalCameras.Count()	&& 
					(m_pVisAreaManager->m_pCurArea || m_pVisAreaManager->m_pCurPortal))
					outdoorViewCam = m_pVisAreaManager->m_lstOutdoorPortalCameras[0];

				bOccludersFound |= m_pObjectsTree->Render(outdoorViewCam, rCB, OCTREENODE_RENDER_FLAG_OCCLUDERS);
			}

			if(!bCameraInIndoor)
			{ // indoors
				PodArray<CVisArea*> * pVisibleAreas = &m_pVisAreaManager->m_lstVisibleAreas;
				for(int i=0; i<pVisibleAreas->Count(); i++)
					if(CVisArea * pArea = pVisibleAreas->GetAt(i))
						if(pArea->m_pObjectsTree)
							for(int c=0; c<pArea->m_lstCurCameras.Count(); c++)
								bOccludersFound |= pArea->m_pObjectsTree->Render(pArea->m_lstCurCameras[c], rCB, OCTREENODE_RENDER_FLAG_OCCLUDERS);
			}
		}

		if(GetCVars()->e_hw_occlusion_culling_objects)
			GetRenderer()->EF_EndEf3D(SHDF_ZPASS|SHDF_ZPASS_ONLY);

		GetCVars()->e_shadows = old_e_shadows;
	}

	return bOccludersFound;
}

IMaterial* C3DEngine::GetSkyMaterial()
{
	IMaterial* pRes(0);
	if (GetCVars()->e_sky_type == 0)
	{
		if (!m_pSkyLowSpecMat)
		{
			SAFE_RELEASE(m_pSkyMat);
			m_pSkyLowSpecMat = m_skyLowSpecMatName.empty() ? 0 : m_pMatMan->LoadMaterial(m_skyLowSpecMatName.c_str(), false);
			if (m_pSkyLowSpecMat)
				m_pSkyLowSpecMat->AddRef();
		}
		pRes = m_pSkyLowSpecMat;
	}
	else
	{
		if (!m_pSkyMat)
		{
			SAFE_RELEASE(m_pSkyLowSpecMat);
			m_pSkyMat = m_skyMatName.empty() ? 0 : m_pMatMan->LoadMaterial(m_skyMatName.c_str(), false);
			if (m_pSkyMat)
				m_pSkyMat->AddRef();
		}
		pRes = m_pSkyMat;
	}
	return pRes;
}

void C3DEngine::RenderScene(const int nRenderFlags, unsigned int dwDrawFlags, const int nFilterFlags)
{
	FUNCTION_PROFILER_3DENGINE;

////////////////////////////////////////////////////////////////////////////////////////
// Store draw flags
////////////////////////////////////////////////////////////////////////////////////////

	assert(Cry3DEngineBase::m_nRenderStackLevel>=0 && Cry3DEngineBase::m_nRenderStackLevel<MAX_RECURSION_LEVELS);
	m_pObjManager->m_dwRecursionDrawFlags[Cry3DEngineBase::m_nRenderStackLevel] = dwDrawFlags;

////////////////////////////////////////////////////////////////////////////////////////
// Begin scene drawing
////////////////////////////////////////////////////////////////////////////////////////

	// init coverage buffer
	if(!m_nRenderStackLevel)
		if(GetCVars()->e_cbuffer)
			m_pCoverageBuffer->BeginFrame(GetCamera());

////////////////////////////////////////////////////////////////////////////////////////
// Define indoor visibility
////////////////////////////////////////////////////////////////////////////////////////

	if(m_pVisAreaManager)
		m_pVisAreaManager->CheckVis();

////////////////////////////////////////////////////////////////////////////////////////
// Draw potential occluders into z-buffer
////////////////////////////////////////////////////////////////////////////////////////

	int nOld_e_hw_occlusion_culling_objects = GetCVars()->e_hw_occlusion_culling_objects;

	GetCoverageBuffer()->SetFrameTime(GetCurAsyncTimeSec());

	if(!m_nRenderStackLevel)
	{
		if(GetCVars()->e_hw_occlusion_culling_objects	|| GetCVars()->e_cbuffer)
		{
			if(!RenderPotentialOccluders(*GetCoverageBuffer(), GetCamera(), false))
				GetCVars()->e_hw_occlusion_culling_objects = 0; // disable culling for this frame if no occluders found

			GetCoverageBuffer()->UpdateDepthTree();
		}

		float fRenderOccludersTime = GetCurAsyncTimeSec() - GetCoverageBuffer()->GetFrameTime();
		float fTimeRatio = fRenderOccludersTime/(0.001f*max(1,GetCVars()->e_cbuffer_max_add_render_mesh_time));
		GetObjManager()->m_fOcclTimeRatio = GetObjManager()->m_fOcclTimeRatio*0.8f + fTimeRatio*0.2f;
		GetObjManager()->m_fOcclTimeRatio = CLAMP(GetObjManager()->m_fOcclTimeRatio, 1.f, 8.f);
	}
	
	if (GetCVars()->e_cbuffer_draw_occluders) // When drawing occluders disable time managment for occluders.
		GetObjManager()->m_fOcclTimeRatio = 1.0f;

	if(GetCVars()->e_sleep>0)
	{
		CrySleep(GetCVars()->e_sleep);
	}

	// todo: use only visible sectors
	if(m_pVisAreaManager)
		m_pVisAreaManager->DrawOcclusionAreasIntoCBuffer(m_pCoverageBuffer);

////////////////////////////////////////////////////////////////////////////////////////
// From here we add render elements of main scene
////////////////////////////////////////////////////////////////////////////////////////

	GetRenderer()->EF_StartEf();  

////////////////////////////////////////////////////////////////////////////////////////
// Add lsources to the renderer and register into sectors
////////////////////////////////////////////////////////////////////////////////////////

	UpdateLightSources();
	PrepareLightSourcesForRendering_0();    
  PrepareLightSourcesForRendering_1();    
	InitShadowFrustums();

////////////////////////////////////////////////////////////////////////////////////////
// Add render elements for indoor
////////////////////////////////////////////////////////////////////////////////////////

	// draw objects inside visible vis areas
	if(m_pVisAreaManager)
		m_pVisAreaManager->DrawVisibleSectors();

////////////////////////////////////////////////////////////////////////////////////////
// Clear current sprites list
////////////////////////////////////////////////////////////////////////////////////////

	m_pObjManager->m_lstFarObjects[m_nRenderStackLevel].Clear();

////////////////////////////////////////////////////////////////////////////////////////
// Add render elements for outdoor
////////////////////////////////////////////////////////////////////////////////////////

	// Current camera maybe redefined by portal system
  CCamera prevCam = GetCamera();

  // Reset ocean volume
  if( !m_nRenderStackLevel )
    m_nOceanRenderFlags &= ~OCR_OCEANVOLUME_VISIBLE;

  if (IsOutdoorVisible())  	
	{	
		if (m_pVisAreaManager && m_pVisAreaManager->m_lstOutdoorPortalCameras.Count()	&& 
			(m_pVisAreaManager->m_pCurArea || m_pVisAreaManager->m_pCurPortal))
    { // enable multi-camera culling
      m_Camera.m_pMultiCamera = &m_pVisAreaManager->m_lstOutdoorPortalCameras;
    }

		if(IsOutdoorVisible())
		{
			RenderSkyBox(GetSkyMaterial());
		}

    // start processing terrain
		if(IsOutdoorVisible() && GetCVars()->e_terrain && Get3DEngine()->m_bShowTerrainSurface && !gEnv->pSystem->IsDedicated())
			m_pTerrain->CheckVis();

    // process streaming and procedural vegetation distribution
    if(!m_nRenderStackLevel && m_pTerrain)
      m_pTerrain->UpdateNodesIncrementaly();

    // render terrain ground
    m_pTerrain->DrawVisibleSectors(); 

		if(m_pObjectsTree)
			m_pObjectsTree->Render(GetCamera(),*GetCoverageBuffer(),OCTREENODE_RENDER_FLAG_OBJECTS, GetSkyColor());

		// add sprites render item
    if(dwDrawFlags & DLD_FAR_SPRITES)
			m_pObjManager->RenderFarObjects();
  }
  else if(m_pVisAreaManager && m_pVisAreaManager->IsSkyVisible())
	{
		RenderSkyBox(GetSkyMaterial());
	}

  // render outdoor entities very near of camera - fix for 1p vehicle entering into indoor
  if(GetCVars()->e_portals_big_entities_fix)
    if(m_pObjectsTree && !IsOutdoorVisible() && GetVisAreaManager() && GetVisAreaManager()->GetCurVisArea()) 
      if(GetVisAreaManager()->GetCurVisArea()->IsConnectedToOutdoor())
  {
    CCamera cam = GetCamera();
    cam.SetFrustum(cam.GetViewSurfaceX(), cam.GetViewSurfaceZ(), cam.GetFov(), cam.GetNearPlane(), 2.f);
     m_pObjectsTree->Render(cam,*GetCoverageBuffer(),
       OCTREENODE_RENDER_FLAG_OBJECTS | OCTREENODE_RENDER_FLAG_OBJECTS_ONLY_ENTITIES, 
       GetSkyColor());
  }

  if(m_pTerrain)
    m_pTerrain->RenderAOSectors();

  // render special objects like laser beams intersecting entire level
  for(int i=0; i<m_lstAlwaysVisible.Count(); i++)
  {
    IRenderNode * pObj = m_lstAlwaysVisible[i];
    const AABB & objBox = pObj->GetBBox();    
    if(GetCamera().IsAABBVisible_E( objBox ))
    {
      Vec3 vCamPos = GetCamera().GetPosition();
      float fEntDistance = cry_sqrtf(Distance::Point_AABBSq(vCamPos,objBox))*m_fZoomFactor;
      assert(fEntDistance>=0 && _finite(fEntDistance));
      if(fEntDistance < pObj->m_fWSMaxViewDist)
        GetObjManager()->RenderObject( pObj, NULL, GetSkyColor(), objBox, fEntDistance, GetCamera(), NULL, false );
    }
  }

  ProcessOcean(dwDrawFlags);

	// restore camera
  SetCamera(prevCam);
   
	if(dwDrawFlags & DLD_DECALS && GetCVars()->e_decals && m_pDecalManager)
	{
		m_pDecalManager->Render();
	}

	if(dwDrawFlags & DLD_PARTICLES && GetCVars()->e_particles)
	{
    m_pPartManager->Render(m_pObjManager, m_pTerrain);    
	}

////////////////////////////////////////////////////////////////////////////////////////
// Finalize frame
////////////////////////////////////////////////////////////////////////////////////////
  
	SetupDistanceFog();

	if(!m_nRenderStackLevel)
		SetupClearColor();

	IRenderer * ppp = GetRenderer();
	{
		FRAME_PROFILER( "Renderer::EF_EndEf3D", GetSystem(), PROFILE_RENDERER );

    GetRenderer()->EF_EndEf3D(IsShadersSyncLoad() ? (nRenderFlags|SHDF_NOASYNC) : nRenderFlags);
	}

	if(!m_nRenderStackLevel)
		GetRenderer()->EnableFog(false);

  // unload old meshes
  if(!m_nRenderStackLevel && m_pTerrain)
    m_pTerrain->CheckNodesGeomUnload();

	GetCVars()->e_hw_occlusion_culling_objects = nOld_e_hw_occlusion_culling_objects;

  if(!m_nRenderStackLevel)
  {
    UpdateRNTmpDataPool(m_bResetRNTmpDataPool);
    m_bResetRNTmpDataPool = false;
  }
}

void C3DEngine::ProcessOcean(const int dwDrawFlags)
{
  if(m_nOceanRenderFlags & OCR_NO_DRAW || !GetVisAreaManager() || GetCVars()->e_default_material)
    return;

  bool bOceanIsForcedByVisAreaFlags = GetVisAreaManager()->IsOceanVisible();

  if ( !IsOutdoorVisible() && !bOceanIsForcedByVisAreaFlags )
    return;

  float fOceanLevel = GetTerrain()->GetWaterLevel();

  // check for case when no any visible terrain sectors has minz lower than ocean level
  bool bOceanVisible = !Get3DEngine()->m_bShowTerrainSurface;
  bOceanVisible |= m_pTerrain->GetDistanceToSectorWithWater()>=0 && fOceanLevel && m_pTerrain->IsOceanVisible();

  if(	bOceanVisible && (dwDrawFlags & DLD_TERRAIN_WATER) && m_bOcean && GetCVars()->e_water_ocean )
  {
    Vec3 vCamPos = GetCamera().GetPosition();
    float fWaterPlaneSize = GetCamera().GetFarPlane();

    AABB boxOcean( Vec3( vCamPos.x-fWaterPlaneSize, vCamPos.y-fWaterPlaneSize,0),
      Vec3( vCamPos.x+fWaterPlaneSize, vCamPos.y+fWaterPlaneSize,fOceanLevel+0.5f) );

    if((! bOceanIsForcedByVisAreaFlags && GetCamera().IsAABBVisible_EM(boxOcean)) ||
      (   bOceanIsForcedByVisAreaFlags && GetCamera().IsAABBVisible_E (boxOcean)))
    {
      bool bOceanIsVisibleFromIndoor = true;
      if(class PodArray<CCamera> * pMultiCamera = GetCamera().m_pMultiCamera)
      {
        for(int i=0; i<pMultiCamera->Count(); i++)
        {
          CVisArea * pExitPortal = (CVisArea *)(pMultiCamera->Get(i))->m_pPortal;
          float fMinZ = pExitPortal->GetAABBox()->min.z;
          float fMaxZ = pExitPortal->GetAABBox()->max.z;            

          if(!bOceanIsForcedByVisAreaFlags)
          {
            if(fMinZ>fOceanLevel && vCamPos.z<fMinZ)
              bOceanIsVisibleFromIndoor = false;
            
            if(fMaxZ<fOceanLevel && vCamPos.z>fMaxZ)
              bOceanIsVisibleFromIndoor = false;
          }
        }
      }

      if(bOceanIsVisibleFromIndoor && GetCoverageBuffer()->IsObjectVisible(boxOcean,eoot_OCEAN,0))
      {
        m_pTerrain->RenderTerrainWater(); 

        if( GetCVars()->e_water_waves && m_nOceanRenderFlags & OCR_OCEANVOLUME_VISIBLE )
          GetWaterWaveManager()->Update();
      }
    }
  }
}

void C3DEngine::RenderSkyBox(IMaterial * pMat)
{
	FUNCTION_PROFILER_3DENGINE;

	if(!Get3DEngine()->GetCoverageBuffer()->IsOutdooVisible())
		return;

	// hdr sky dome
	// TODO: temporary workaround to force the right sky dome for the selected shader
	if( pMat && m_pREHDRSky && !stricmp( pMat->GetShaderItem().m_pShader->GetName(), "SkyHDR" ) )
	{
		if( GetCVars()->e_sky_box )
		{
			// set sky light quality
			if( GetCVars()->e_sky_quality < 1 )
				GetCVars()->e_sky_quality = 1;
			else if( GetCVars()->e_sky_quality > 2 )
				GetCVars()->e_sky_quality = 2;
			m_pSkyLightManager->SetQuality( GetCVars()->e_sky_quality ); 

			// set sky light incremental update rate and perform update
			if( GetCVars()->e_sky_update_rate <= 0.0f )
				GetCVars()->e_sky_update_rate = 0.01f;
			m_pSkyLightManager->IncrementalUpdate( GetCVars()->e_sky_update_rate ); 

			// prepare render object
			CRenderObject * pObj = GetRenderer()->EF_GetObject( true, -1 );
      if (!pObj)
        return;
			pObj->m_II.m_Matrix.SetTranslationMat( GetCamera().GetPosition() );
			pObj->m_ObjFlags |= FOB_TRANS_TRANSLATE;
			pObj->m_pID = m_pREHDRSky;

			m_pREHDRSky->m_pRenderParams = m_pSkyLightManager->GetRenderParams();
			m_pREHDRSky->m_moonTexId = m_nNightMoonTexId;
			
			// add sky dome to render list
			GetRenderer()->EF_AddEf(m_pREHDRSky, pMat->GetShaderItem(), pObj, EFSLIST_GENERAL, 1);
		}

		GetRenderer()->SetSkyLightRenderParams( m_pSkyLightManager->GetRenderParams() );
	}
	// skybox
	else 
  {
		if (pMat && m_pRESky && GetCVars()->e_sky_box )
		{
			CRenderObject * pObj = GetRenderer()->EF_GetObject(true, -1);
      if (!pObj)
        return;
			pObj->m_II.m_Matrix.SetTranslationMat(GetCamera().GetPosition());
			pObj->m_II.m_Matrix = pObj->m_II.m_Matrix*Matrix33::CreateRotationZ( DEG2RAD(m_fSkyBoxAngle) );
			pObj->m_ObjFlags |= FOB_TRANS_TRANSLATE | FOB_TRANS_ROTATE;

			if(!m_nRenderStackLevel)
			{
				if(m_pVisAreaManager->m_lstIndoorActiveOcclVolumes.Count())
				{
					Matrix34 matInv = pObj->m_II.m_Matrix;
					matInv.Invert();
					Vec3 vPos;

					for(int i=0; i<MAX_SKY_OCCLAREAS_NUM; i++)
					{
						if(i<m_pVisAreaManager->m_lstIndoorActiveOcclVolumes.Count())
						{
							vPos = matInv.TransformPoint(m_pVisAreaManager->m_lstIndoorActiveOcclVolumes[i]->m_arrvActiveVerts[0]);
							m_pRESky->m_arrvPortalVerts[i][0].xyz = vPos;
							m_pRESky->m_arrvPortalVerts[i][0].color.bcolor[3] = 255;

							vPos = matInv.TransformPoint(m_pVisAreaManager->m_lstIndoorActiveOcclVolumes[i]->m_arrvActiveVerts[1]);
							m_pRESky->m_arrvPortalVerts[i][1].xyz = vPos;
							m_pRESky->m_arrvPortalVerts[i][1].color.bcolor[3] = 255;

							vPos = matInv.TransformPoint(m_pVisAreaManager->m_lstIndoorActiveOcclVolumes[i]->m_arrvActiveVerts[3]);
							m_pRESky->m_arrvPortalVerts[i][2].xyz = vPos;
							m_pRESky->m_arrvPortalVerts[i][2].color.bcolor[3] = 255;

							vPos = matInv.TransformPoint(m_pVisAreaManager->m_lstIndoorActiveOcclVolumes[i]->m_arrvActiveVerts[2]);
							m_pRESky->m_arrvPortalVerts[i][3].xyz = vPos;
							m_pRESky->m_arrvPortalVerts[i][3].color.bcolor[3] = 255;
						}
						else
							memset(m_pRESky->m_arrvPortalVerts[i],0,sizeof(m_pRESky->m_arrvPortalVerts[i]));
					}
				}
				else
					memset(m_pRESky->m_arrvPortalVerts,0,sizeof(m_pRESky->m_arrvPortalVerts));
			}
			else
				memset(m_pRESky->m_arrvPortalVerts,0,sizeof(m_pRESky->m_arrvPortalVerts));
			
			m_pRESky->m_fTerrainWaterLevel = max(0.0f,m_pTerrain->GetWaterLevel());
			m_pRESky->m_fSkyBoxStretching = m_fSkyBoxStretching;

			GetRenderer()->SetSkyLightRenderParams( 0 );
			GetRenderer()->EF_AddEf(m_pRESky, pMat->GetShaderItem(), pObj, EFSLIST_GENERAL, 1);		
		}
		
		GetRenderer()->SetSkyLightRenderParams( 0 );
  }

}

void C3DEngine::DrawText(float x, float y, const char * format, ...)
{
	char buffer[512];
	va_list args;
	va_start(args, format);
	vsprintf_s(buffer, sizeof(buffer), format, args);
	va_end(args);

	if (m_pConsolepFont)
	{
		m_pConsolepFont->UseRealPixels(false);
		m_pConsolepFont->SetSameSize(true);
		m_pConsolepFont->SetCharWidthScale(1);
		m_pConsolepFont->SetSize(vector2f(14, 14));
		m_pConsolepFont->SetColor(ColorF(1,1,1,1));
		m_pConsolepFont->SetCharWidthScale(.55f);
		m_pConsolepFont->DrawString( x-m_pConsolepFont->GetCharWidth() * strlen(buffer) * m_pConsolepFont->GetCharWidthScale(), y, buffer );
	}
}


void C3DEngine::DrawTextRA(float x, float y, const char * format, ...)
{
	char buffer[512];
	va_list args;
	va_start(args, format);
	vsprintf_s(buffer, sizeof(buffer), format, args);
	va_end(args);

	if (m_pConsolepFont)
	{
		m_pConsolepFont->UseRealPixels(false);
		m_pConsolepFont->SetSameSize(true);
		m_pConsolepFont->SetCharWidthScale(1);
		m_pConsolepFont->SetSize(vector2f(16, 16));
		m_pConsolepFont->SetColor(ColorF(1,1,1,1));
		m_pConsolepFont->SetCharWidthScale(.65f);
		m_pConsolepFont->DrawString( x, y, buffer );
	}
}

int __cdecl C3DEngine__Cmp_FPS(const void* v1, const void* v2)
{
	float f1 = *(float*)v1;
	float f2 = *(float*)v2;

	if(f1 > f2)
		return 1;
	else if(f1 < f2)
		return -1;

	return 0;
}

inline void Blend( float& Stat, float StatCur, float fBlendCur )
{
	Stat = Stat * (1.f - fBlendCur) + StatCur * fBlendCur;
}

inline void Blend( float& Stat, int& StatCur, float fBlendCur )
{
	Blend( Stat, float(StatCur), fBlendCur );
	StatCur = int_round(Stat);
}


static void AppendString( char * &szEnd, const char *szToAppend )
{
	assert(szToAppend);

	while(*szToAppend)
		*szEnd++ = *szToAppend++;
	
	*szEnd++=' ';
	*szEnd=0;
}



void C3DEngine::DisplayInfo(float & fTextPosX, float & fTextPosY, float & fTextStepY, const bool bEnhanced)
{ 
	if (gEnv->pSystem->IsDedicated())
		return;

#if defined(INFO_FRAME_COUNTER)
  static int frameCounter = 0;
#endif
  GetRenderer()->SetState(GS_NODEPTHTEST);

	// If stat averaging is on, compute blend amount for current stats.

	static float fBlendTime = 0.f;
	float fFrameRate = GetTimer()->GetFrameRate();
	float fFrameTime = GetTimer()->GetRealFrameTime();
	fBlendTime += fFrameTime;

	arrFrameTimeforSaveLevelStats.push_back(fFrameTime);

	int iBlendMode = 0;
	float fBlendCur = GetTimer()->GetProfileFrameBlending(&fBlendTime, &iBlendMode);

	// make level name
	char szLevelName[128];

	*szLevelName=0;
	{
		int i;
		for(i=strlen(m_szLevelFolder)-2; i>0; i--)
			if(m_szLevelFolder[i] == '\\' || m_szLevelFolder[i] == '/')
				break;

		if(i>=0)
		{
			strncpy(szLevelName,&m_szLevelFolder[i+1],sizeof(szLevelName));
			szLevelName[sizeof(szLevelName)-1] = 0;

			for(int i=strlen(szLevelName)-1; i>0; i--)
				if(szLevelName[i] == '\\' || szLevelName[i] == '/')
					szLevelName[i]=0;
		}
	}

	fTextPosX = 800-5;
	fTextPosY = -10;
	fTextStepY = 13;

  Matrix33 m = Matrix33(GetCamera().GetMatrix());
	m.OrthonormalizeFast();
  Ang3 aAng = RAD2DEG(Ang3::GetAnglesXYZ(m));
	Vec3 vPos = GetCamera().GetPosition();

#ifdef SP_DEMO

	ICVar* pDisplayInfo = GetConsole()->GetCVar("r_DisplayInfo");
	if (pDisplayInfo && pDisplayInfo->GetIVal()!=2)
	{
#endif	
  DrawText( fTextPosX, fTextPosY+=fTextStepY, "CamPos=%3d %3d %3d Angl=%3d %2d %3d ZN=%.2f ZF=%d", 
    (int)vPos.x,(int)vPos.y,(int)vPos.z, (int)aAng.x,(int)aAng.y,(int)aAng.z,
		GetCamera().GetNearPlane(), (int)GetCamera().GetFarPlane());
#ifdef SP_DEMO
	}
#endif	

	// get version
	const SFileVersion & ver = GetSystem()->GetFileVersion();
	//char sVersion[128];
	//ver.ToString(sVersion);

	// Get memory usage.
	static IMemoryManager::SProcessMemInfo processMemInfo;
	{
		static int nGetMemInfoCount = 0;
		if ((nGetMemInfoCount&0x1F) == 0)
		{
			// Only get mem stats every 32 frames.
			GetISystem()->GetIMemoryManager()->GetProcessMemInfo(processMemInfo);
		}
		nGetMemInfoCount++;
	}

	bool bHDREnabled = m_pRenderer->EF_Query(EFQ_HDRModeEnabled)!=0;
	bool bMultiGPU = m_pRenderer->EF_Query(EFQ_MultiGPUEnabled)!=0;

	const char* pRenderType(0);
	switch(gEnv->pRenderer->GetRenderType())
	{
		case eRT_DX9: pRenderType = "DX9"; break;
		case eRT_DX10: pRenderType = "DX10"; break;
		case eRT_Xbox360: pRenderType = "X360"; break;
		case eRT_PS3: pRenderType = "PS3"; break;
		case eRT_Null: pRenderType = "Null"; break;
		case eRT_Undefined:
		default: assert(0); pRenderType = "Undefined"; break;
	}

	static int iRarelyUpdatedSpec=-2;				// for performance reasons we don't do this every frame, -2 means it' needs to be updated
	{
		static float fRarelyUpdateTimer=0;

		fRarelyUpdateTimer+=fFrameTime;

		if(fRarelyUpdateTimer>0.800f)					//  update it every once in a while
		{
			fRarelyUpdateTimer=0;
			iRarelyUpdatedSpec=-2;
		}
	}

	if(iRarelyUpdatedSpec==-2)							// update required
	{
		static ICVar *pSysSpecFull = GetConsole()->GetCVar("sys_spec_full");

		if(pSysSpecFull)
			iRarelyUpdatedSpec = pSysSpecFull->GetRealIVal();		// that can be a bit slow
	}

	{
		char szFlags[128], *szFlagsEnd=szFlags;

		switch(iRarelyUpdatedSpec)
		{
			case 1:		AppendString(szFlagsEnd,"LowSpec");break;
			case 2:		AppendString(szFlagsEnd,"MedSpec");break;
			case 3:		AppendString(szFlagsEnd,"HighSpec");break;
			case 4:		AppendString(szFlagsEnd,"VeryHighSpec");break;
			case -1:  AppendString(szFlagsEnd,"Custom");break;
			case -2:	break;																					// cvar doesn't exist yet or spec is -2 which doesn't make sense
			default:	assert(0);
		}

		if(bHDREnabled)
			AppendString(szFlagsEnd,"HDR");

		if(bMultiGPU)
			AppendString(szFlagsEnd,"MGPU");

		if(gEnv->pCryPak->GetLvlResStatus())
			AppendString(szFlagsEnd,"LvlRes");

		if (gEnv->pSystem->IsDevMode())
			AppendString(szFlagsEnd,"DevMode");

		if (m_pRenderer->EF_Query(EFQ_TextureStreamingEnabled))
			AppendString(szFlagsEnd,"Streaming");

		if (m_pRenderer->EF_Query(EFQ_FSAAEnabled))
			AppendString(szFlagsEnd,"FSAA");

		// remove last space
		if(szFlags!=szFlagsEnd)
			*(szFlagsEnd-1)=0;

#ifdef SP_DEMO

		ICVar* pDisplayInfo = GetConsole()->GetCVar("r_DisplayInfo");
		if (pDisplayInfo && pDisplayInfo->GetIVal()==2)
		{
			strcpy(szFlags,"PRE-RELEASE DEMO");
			DrawText( fTextPosX, fTextPosY+=fTextStepY, szFlags);
			return;
		}
		else
			DrawText( fTextPosX, fTextPosY+=fTextStepY, "%s %dbit %s Build=%d Level=%s",
				pRenderType, sizeof(char *) * 8, szFlags,ver.v[0], szLevelName);
#else
		DrawText( fTextPosX, fTextPosY+=fTextStepY, "%s %dbit %s Build=%d Level=%s",
			pRenderType, sizeof(char *) * 8, szFlags,ver.v[0], szLevelName);
#endif
	}

  // Polys in scene
	int nPolygons, nShadowVolPolys;
	GetRenderer()->GetPolyCount(nPolygons,nShadowVolPolys);
	int nDrawCalls = GetRenderer()->GetCurrentNumberOfDrawCalls();
	if (fBlendCur != 1.f)
	{
		// Smooth over time.
		static float fPolygons, fShadowVolPolys, fDrawCalls;
		Blend(fPolygons, nPolygons, fBlendCur);
		Blend(fShadowVolPolys, nShadowVolPolys, fBlendCur);
		Blend(fDrawCalls, nDrawCalls, fBlendCur);
	}

	if (nShadowVolPolys>0)
		DrawText(fTextPosX, fTextPosY+=fTextStepY, "Tris:%2d,%03d (SV %d) DP:%d",
			nPolygons/1000, nPolygons%1000, nShadowVolPolys, nDrawCalls);
	else
		DrawText(fTextPosX, fTextPosY+=fTextStepY, "Tris:%2d,%03d  DP:%d",
			nPolygons/1000, nPolygons%1000, nDrawCalls);

/*  { // Polys per sec
    int per_sec = (int)(nPolygons*fCurrentFPS);
    int m =  per_sec    /1000000;
    int t = (per_sec - m*1000000)/1000;
    int n =  per_sec - m*1000000 - t*1000;      
    DrawText(fTextPosX, fTextPosY+=fTextStepY,"P/Sec %003d,%003d,%003d", m,t,n);
  }*/

  if(GetCVars()->e_stream_cgf && m_pObjManager)
  {
    int nReady=0, nTotalInStreaming=0, nTotal=0;
    m_pObjManager->GetObjectsStreamingStatus(nReady,nTotalInStreaming,nTotal);
    DrawText(fTextPosX, fTextPosY+=fTextStepY,"Objects streaming: %d/%d/%d", nReady, nTotalInStreaming, nTotal);
  }

  if(GetCVars()->e_stream_areas && m_pTerrain)
  {
    int nReady=0, nTotal=0;

    m_pTerrain->GetStreamingStatus(nReady,nTotal);
    DrawText(fTextPosX, fTextPosY+=fTextStepY,"Terrain streaming: %d/%d", nReady, nTotal);

    m_pVisAreaManager->GetStreamingStatus(nReady,nTotal);
    DrawText(fTextPosX, fTextPosY+=fTextStepY,"Indoor streaming: %d/%d", nReady, nTotal);
  }

	// Current fps
	if (iBlendMode)
	{
		// Track FPS frequency, report min/max.
		Blend(m_fAverageFPS, fFrameRate, fBlendCur);

		m_fMinFPS = fFrameRate;
		m_fMaxFPS = fFrameRate;

		// Add current to histogram, anti-aliased.
		if (fBlendCur < 1.f)
		{
			float fBlendHist = fBlendCur / (1.f - fBlendCur);
			int nFPS = int(fFrameRate);
			if (nFPS >= nFPSMaxTrack)
				afFPSCounts[nFPSMaxTrack] += fBlendHist;
			else
			{
				float fFrac = fFrameRate - nFPS;
				afFPSCounts[nFPS] += fBlendHist * (1.f - fFrac);
				afFPSCounts[nFPS+1] += fBlendHist * fFrac;
			}

			// Decay, and sum the history.
			float fTotal = 0.f;
			for (int i = 0; i <= nFPSMaxTrack; i++)
			{
				afFPSCounts[i] *= 1.f - fBlendCur;
				fTotal += afFPSCounts[i];
			}

			// Instead of extreme min/max, average lowest & highest 5%.
			float fSum = 0.f, fCount = 0.f;
			for (int i = 0; i <= nFPSMaxTrack; i++)
			{
				fCount += afFPSCounts[i];
				fSum += afFPSCounts[i] / (i+0.45f);
				if (fCount >= 0.05f*fTotal)
					break;
			}
			fSum > 0.f ? m_fMinFPS = fCount / fSum : 0.f;

			fSum = 0.f, fCount = 0.f;
			for (int i = nFPSMaxTrack; i >= 0; i--)
			{
				fCount += afFPSCounts[i];
				fSum += afFPSCounts[i] / (i+0.45f);
				if (fCount >= 0.05f*fTotal)
					break;
			}
			m_fMaxFPS = fSum > 0.f ? fCount / fSum : 0.f;
		}

		const char* sMode = "";
		switch (iBlendMode)
		{
			case 1: sMode = "avg"; break;
			case 2: sMode = "peak wt"; break;
			case 3: sMode = "peak hold"; break;
		}
		DrawText( fTextPosX, fTextPosY+=fTextStepY, "FPS %.1f [%.0f..%.0f], %s over %.1f s", 
			m_fAverageFPS, m_fMinFPS, m_fMaxFPS, 
			sMode, fBlendTime );
	}
	else
  {
		const int nHistorySize = 16;
		static float arrfFrameRateHistory[nHistorySize]={0};

		static int nFrameId = 0; nFrameId++;
		int nSlotId = nFrameId%nHistorySize;
		assert(nSlotId>=0 && nSlotId<nHistorySize);
		arrfFrameRateHistory[nSlotId] = min(9999.f, GetTimer()->GetFrameRate());

		float fMinFPS = 9999.0f;
		float fMaxFPS = 0;
		for (int i = 0; i < nHistorySize; i++)
		{
			if (arrfFrameRateHistory[i] < fMinFPS)
				fMinFPS = arrfFrameRateHistory[i];
			if (arrfFrameRateHistory[i] > fMaxFPS)
				fMaxFPS = arrfFrameRateHistory[i];
		}

		float fFrameRate = 0;
		float fValidFrames = 0;
		for(int i=0; i<nHistorySize; i++)
		{
			int nSlotId = (nFrameId-i)%nHistorySize;
			fFrameRate += arrfFrameRateHistory[nSlotId];
			fValidFrames++;
		}
		fFrameRate /= fValidFrames;

		m_fAverageFPS = fFrameRate;
		m_fMinFPS = fMinFPS;
		m_fMaxFPS = fMaxFPS;



		if(bEnhanced)
		{
			DrawText( fTextPosX, fTextPosY+=fTextStepY, "%6.2f ~%6.2f ms (%6.2f..%6.2f)",
				GetTimer()->GetFrameTime()*1000.0f, 1000.0f/max(0.0001f,fFrameRate),
				1000.0f/max(0.0001f,fMinFPS),
				1000.0f/max(0.0001f,fMaxFPS));
		}
		else
		{
			DrawText( fTextPosX, fTextPosY+=fTextStepY, "FPS %5.1f (%3d..%3d)",
				min(999.f, fFrameRate), (int)min(999.f, fMinFPS), (int)min(999.f, fMaxFPS) );
		}
  }

  if(GetCVars()->e_particles_debug & 1)
  {
		// Show particle stats.
		static SParticleCounts Counts;
		SParticleCounts CurCounts;
		m_pPartManager->GetCounts(CurCounts);
		Counts.Blend(CurCounts, 1.f-fBlendCur, fBlendCur);

    DrawText( fTextPosX, fTextPosY+=fTextStepY, 
      "Particle %5.0f/%5.0f/%5.0f, Fill %5.2f/%5.2f, Emitter %3.0f/%3.0f/%3.0f",
				Counts.ParticlesRendered, Counts.ParticlesActive, Counts.ParticlesAlloc, 
				Counts.ScrFillRendered, Counts.ScrFillProcessed, 
				Counts.EmittersRendered, Counts.EmittersActive, Counts.EmittersAlloc);
		if (GetCVars()->e_particles_debug & AlphaBit('r'))
			DrawText( fTextPosX, fTextPosY+=fTextStepY, 
				"Particle Reit=%4.0f Ovf=%4.0f Reuse=%4.0f Reject=%4.0f Coll Tr=%4.1f Ob=%4.1f Clip=%4.1f",
					Counts.ParticlesReiterate, Counts.ParticlesOverflow, Counts.ParticlesReuse, Counts.ParticlesReject, 
					Counts.ParticlesCollideTerrain, Counts.ParticlesCollideObjects, Counts.ParticlesClip);
  }
	m_pPartManager->RenderDebugInfo();

	float fDebugListPosY = -10;

	int nVirtMemMB = (int)(processMemInfo.PagefileUsage/(1024*1024));

	if(GetCVars()->e_debug_lights)
  {
    char sLightsList[512]="";
    for(int i=0; i<m_lstDynLights.Count(); i++)
    {
      if(m_lstDynLights[i].m_Id>=0 && m_lstDynLights[i].m_fRadius >= 0.5f )
				if(!(m_lstDynLights[i].m_Flags & DLF_FAKE))
      {
				if( GetCVars()->e_debug_lights == 2 )
				{
					int nShadowCasterNumber = 0;
					for(int l=0; l<MAX_GSM_LODS_NUM; l++)
					{
						ShadowMapFrustum * pFr = m_lstDynLights[i].m_pOwner->GetShadowFrustum(l);
						if(pFr && pFr->pCastersList)
							nShadowCasterNumber += pFr->pCastersList->Count();
					}
					DrawTextRA( 5, fDebugListPosY+=fTextStepY, "%s - SM%d", m_lstDynLights[i].m_sName, nShadowCasterNumber );
				}

				if(i<4)
				{
					char buff[32]="";
					strncat(buff,m_lstDynLights[i].m_sName,8);
					buff[9]=0;

					if(m_lstDynLights[i].m_Flags&DLF_CASTSHADOW_MAPS)
					{
						strcat(buff,"-SM");

						int nCastingObjects = 0;
						for(int l=0; l<MAX_GSM_LODS_NUM; l++)
						{
							ShadowMapFrustum * pFr = m_lstDynLights[i].m_pOwner->GetShadowFrustum(l);
							if(pFr && pFr->pCastersList)
								nCastingObjects += pFr->pCastersList->Count();
						}

						if(nCastingObjects)
						{
							char tmp[32];
							sprintf(tmp,"%d",nCastingObjects);
							strcat(buff, tmp);
						}
					}

					strcat(sLightsList, buff);        
					if(i<m_lstDynLights.Count()-1)
						strcat(sLightsList,",");
				}
      }
    }

#ifdef WIN64
#pragma warning( push )									//AMD Port
#pragma warning( disable : 4267 )
#endif

    DrawText( fTextPosX, fTextPosY+=fTextStepY, "DLights=%s(%d/%d/%d)", sLightsList, m_nRenderLightsNum, m_nRealLightsNum, m_lstDynLights.Count() );
  }
	else
		DrawText( fTextPosX, fTextPosY+=fTextStepY, "Mem=%dMB DLights=(%d/%d/%d)",nVirtMemMB,m_nRenderLightsNum, m_nRealLightsNum, m_lstDynLights.Count() );

#ifdef WIN64
#pragma warning( pop )									//AMD Port
#endif

	if(GetCVars()->e_gsm_stats)
	{
		DrawText( fTextPosX, fTextPosY+=fTextStepY, "--------------- GSM Stats ---------------" );
		
		if(CLightEntity::ShadowMapInfo * pSMI = m_pSun->m_pShadowMapInfo)
		{
			int arrGSMCastersCount[MAX_GSM_LODS_NUM];
			memset(arrGSMCastersCount,0,sizeof(arrGSMCastersCount));
			char szText[256] = "Objects count per shadow map: ";
			for(int nLod=0; nLod<GetCVars()->e_gsm_lods_num && nLod<MAX_GSM_LODS_NUM; nLod++)
			{
				ShadowMapFrustum * & pLsource = pSMI->pGSM[nLod];
				
				if(nLod)
					strcat(&szText[strlen(szText)], ", ");

				sprintf(&szText[strlen(szText)], "%d", pLsource->pCastersList->Count());
			}

			DrawText( fTextPosX, fTextPosY+=fTextStepY, szText );
		}

		for(int nSunInUse=0; nSunInUse<2; nSunInUse++)
		{
			if(nSunInUse)
				DrawText( fTextPosX, fTextPosY+=fTextStepY, "WithSun  ListId   FrNum UserNum" );
			else
				DrawText( fTextPosX, fTextPosY+=fTextStepY, "NoSun    ListId   FrNum UserNum" );

			for(ShadowFrustumListsCache::iterator it = m_FrustumsCache[nSunInUse].begin(); it != m_FrustumsCache[nSunInUse].end(); ++it)
			{
				int nListId = it->first;
				PodArray<ShadowMapFrustum*> * pList = it->second;

				DrawText( fTextPosX, fTextPosY+=fTextStepY, 
					"%8d %8d %8d", 
					nListId,
					pList->Count(), m_FrustumsCacheUsers[nSunInUse][nListId]);
			}
		}
	}

	// objects counter
	if(GetCVars()->e_obj_stats)
	{
		DrawText( fTextPosX, fTextPosY+=fTextStepY, "Vegetations: %d", GetInstCount(eERType_Vegetation));
		DrawText( fTextPosX, fTextPosY+=fTextStepY, "Brushes:     %d", GetInstCount(eERType_Brush));
		DrawText( fTextPosX, fTextPosY+=fTextStepY, "Roads:       %d", GetInstCount(eERType_RoadObject_NEW));
		DrawText( fTextPosX, fTextPosY+=fTextStepY, "VoxObj:      %d", GetInstCount(eERType_VoxelObject));

    if(GetCVars()->e_obj_stats>1 && m_pObjectsTree)
    {
      DrawText( fTextPosX, fTextPosY+=fTextStepY, "By list type:");
      DrawText( fTextPosX, fTextPosY+=fTextStepY, "Main:      %d", m_pObjectsTree->GetObjectsCount(eMain));
      DrawText( fTextPosX, fTextPosY+=fTextStepY, "Cast:      %d", m_pObjectsTree->GetObjectsCount(eCasters));
      DrawText( fTextPosX, fTextPosY+=fTextStepY, "Occl:      %d", m_pObjectsTree->GetObjectsCount(eOccluders));
      DrawText( fTextPosX, fTextPosY+=fTextStepY, "Merg:      %d", m_pObjectsTree->GetObjectsCount(eMergedCache));
    }

		int nFree = m_LTPRootFree.Count();
		int nUsed = m_LTPRootUsed.Count();
		DrawText( fTextPosX, fTextPosY+=fTextStepY, "RNTmpData(Used+Free): %d + %d = %d (%d KB)",	 
			nUsed, nFree, nUsed+nFree, (nUsed+nFree)*sizeof(CRNTmpData)/1024);

    DrawText( fTextPosX, fTextPosY+=fTextStepY, "COctreeNode::m_arrEmptyNodes.Count() = %d", COctreeNode::m_arrEmptyNodes.Count());
	}

	CCullBuffer * pCB = GetCoverageBuffer();
	if(pCB && GetCVars()->e_cbuffer && GetCVars()->e_cbuffer_debug && pCB->TrisWritten())
		DrawText( fTextPosX, fTextPosY+=fTextStepY, 
		"CB: Write:%3d/%2d Test:%4d/%4d/%3d ZFarM:%.2f ZNearM:%.2f Res:%d OI:%s TimeRatio:%.1f", 
			pCB->TrisWritten(), pCB->ObjectsWritten(),
			pCB->TrisTested(), pCB->ObjectsTested(), pCB->ObjectsTestedAndRejected(),
			pCB->GetZFarInMeters(),pCB->GetZNearInMeters(),pCB->SelRes(),
			pCB->IsOutdooVisible() ? "Out" : "In",
			GetObjManager()->m_fOcclTimeRatio);
	
#if defined(INFO_FRAME_COUNTER)
	++frameCounter;
	DrawText(fTextPosX, fTextPosY += fTextStepY, "Frame #%d", frameCounter);
#endif

	if(GetCVars()->e_terrain_texture_streaming_debug && m_pTerrain)
	{
		int nCacheSize[2] = {0,0};
		m_pTerrain->GetTextureCachesStatus(nCacheSize[0],nCacheSize[1]);
		DrawText( fTextPosX, fTextPosY+=fTextStepY, 
			"Terrain texture streaming status: waiting=%d, all=%d, pool=(%d+%d)", 
			m_pTerrain->GetNotReadyTextureNodesCount(), m_pTerrain->GetActiveTextureNodesCount(),
			nCacheSize[0], nCacheSize[1]);	
	}

  if(GetCVars()->e_terrain_bboxes && m_pTerrain)
  {
    DrawText( fTextPosX, fTextPosY+=fTextStepY, 
      "GetDistanceToSectorWithWater() = %.2f", m_pTerrain->GetDistanceToSectorWithWater());	
  }

	if(GetCVars()->e_proc_vegetation == 2)
	{
		CProcVegetPoolMan & pool = *CTerrainNode::GetProcObjPoolMan();
		int nAll; int nUsed = pool.GetUsedInstancesCount(nAll);

		DrawText( fTextPosX, fTextPosY+=fTextStepY, "---------------------------------------" );
		DrawText( fTextPosX, fTextPosY+=fTextStepY, "Procedural root pool status: used=%d, all=%d, active=%d", 
			nUsed, nAll, GetTerrain()->GetActiveProcObjNodesCount());
		DrawText( fTextPosX, fTextPosY+=fTextStepY, "---------------------------------------" );
		for(int i=0; i<pool.m_lstUsed.Count(); i++)
		{
			CProcObjSector * pSubPool = pool.m_lstUsed[i];
			int nAll; int nUsed = pSubPool ->GetUsedInstancesCount(nAll);
			DrawText( fTextPosX, fTextPosY+=fTextStepY, 
				"Used sector: used=%d, all=%dx%d", nUsed, nAll, (int)MAX_PROC_OBJ_IN_CHUNK);
		}
		for(int i=0; i<pool.m_lstFree.Count(); i++)
		{
			CProcObjSector * pSubPool = pool.m_lstFree[i];
			int nAll; int nUsed = pSubPool ->GetUsedInstancesCount(nAll);
			DrawText( fTextPosX, fTextPosY+=fTextStepY, 
				"Free sector: used=%d, all=%dx%d", nUsed, nAll, (int)MAX_PROC_OBJ_IN_CHUNK);
		}
		DrawText( fTextPosX, fTextPosY+=fTextStepY, "---------------------------------------" );
		{
			SProcObjChunkPool & chunks = *CTerrainNode::GetProcObjChunkPool();
			int nAll; int nUsed = chunks.GetUsedInstancesCount(nAll);
			DrawText( fTextPosX, fTextPosY+=fTextStepY, 
				"chunks pool status: used=%d, all=%d, %d MB", nUsed, nAll, 
				nAll*int(MAX_PROC_OBJ_IN_CHUNK)*sizeof(CVegetation)/1024/1024);
		}
	}
}

void C3DEngine::DrawFarTrees()
{
  m_pObjManager->DrawFarObjects(GetMaxViewDistance());
}

void C3DEngine::GenerateFarTrees()
{
  m_pObjManager->GenerateFarObjects(GetMaxViewDistance());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

void C3DEngine::SetupDistanceFog()
{
	FUNCTION_PROFILER_3DENGINE;

  GetRenderer()->SetFog(0, 0, max(GetMaxViewDistance(),0.f), &m_vFogColor[0], R_FOGMODE_LINEAR);
  GetRenderer()->EnableFog(GetCVars()->e_fog>0);
}

void C3DEngine::ScreenShotHighRes(CStitchedImage* pStitchedImage,const int nRenderFlags, const CCamera &cam, const int dwDrawFlags, const int nFilterFlags,uint32 SliceCount,f32 fTransitionSize)
{
#if !defined(LINUX)

	// finish frame started by system
	GetRenderer()->EndFrame();

	GetConsole()->SetScrollMax(0);
	GetTimer()->EnableTimer(false);

	const uint32	ScreenWidth	=	GetRenderer()->GetWidth();
	const	uint32	ScreenHeight	=	GetRenderer()->GetHeight();

	uint32 * pImage = new uint32[ScreenWidth*ScreenHeight];
	for(int yy=0; yy<SliceCount; yy++)
	for(int xx=0; xx<SliceCount; xx++)
	{
		PrintMessage("Rendering tile %d of %d ... ",xx+yy*SliceCount+1,SliceCount*SliceCount);

		const int	BlendX	=	xx*2/SliceCount;
		const int	BlendY	=	yy*2/SliceCount;
		const int x	=	((xx*2)%SliceCount&~1)+BlendX;
		const int y	=	((yy*2)%SliceCount&~1)+BlendY;

		// start new frame and define needed tile
		const f32 ScreenScale	=	1.f/(1.f/static_cast<f32>(SliceCount)*(1.f+fTransitionSize));
		GetRenderer()->BeginFrame();
		GetRenderer()->SetRenderTile( 
			(static_cast<f32>(SliceCount-1-x)-fTransitionSize*0.5f)/static_cast<f32>(SliceCount)*ScreenScale,
			(static_cast<f32>(SliceCount-1-y)-fTransitionSize*0.5f)/static_cast<f32>(SliceCount)*ScreenScale,
			ScreenScale,ScreenScale);

		RenderInternal(nRenderFlags, cam, "ScreenShotHighRes",dwDrawFlags, nFilterFlags);

		GetRenderer()->EndFrame();

		PrintMessagePlus("reading frame buffer ... ");

		GetRenderer()->ReadFrameBufferFast(pImage, ScreenWidth, ScreenHeight);
		pStitchedImage->RasterizeRect(pImage,ScreenWidth,ScreenHeight,x,y,fTransitionSize,
																	fTransitionSize>0.0001f && BlendX,
																	fTransitionSize>0.0001f && BlendY);

		PrintMessagePlus("ok");
	}
	delete[] pImage;

	// re-start frame so system can safely finish it
	GetRenderer()->BeginFrame();

	// restore initial state
	GetRenderer()->SetViewport(0,0,GetRenderer()->GetWidth(),GetRenderer()->GetHeight());
	GetConsole()->SetScrollMax(300);
	GetTimer()->EnableTimer(true);
	GetRenderer()->SetRenderTile();

	// make sure folder existing
/*	CryCreateDirectory("Game\\HiResScreenShots",0);

	// find free file name
	int nFileId = 0;
	char sFileName[256];
	while(1)
	{
		snprintf(sFileName, sizeof(sFileName), "%s/HiResScreenShots/%d_%dx%d.%s", 
		  FIleUtil::GetGameFolder(),
			nFileId, 
			SliceCount, 
			SliceCount,
			stricmp(GetCVars()->e_screenshot_file_format->GetString(),"tga")==0 ? "tga" : "jpg");

		FILE * fp = GetSystem()->GetIPak()->FOpen(sFileName,"rb");
		if (!fp)
			break; // file doesn't exist

		GetSystem()->GetIPak()->FClose(fp);
		nFileId++;
	}

	PrintMessage("Combining tiles into final image ... ");

	int nFinSize = lstSubImages.Count()*
		GetRenderer()->GetWidth()*GetRenderer()->GetHeight()*4;

	unsigned char * pFinalImage = (unsigned char *)malloc(nFinSize);
	if(!pFinalImage)
	{
		PrintMessage("Error making screen shot: memory allocation error (%d MB)", nFinSize/1024/1024);
		return;
	}

	// Save final image
	for(int nImageId=0; nImageId<lstSubImages.Count(); nImageId++)
	{
		int pos_X = (nImageId)%SliceCount*GetRenderer()->GetWidth();
		int pos_Y = (lstSubImages.Count()-nImageId-1)/SliceCount*GetRenderer()->GetHeight();

		for(int x=0; x<GetRenderer()->GetWidth(); x++)
			for(int y=0; y<GetRenderer()->GetHeight(); y++)
			{
				int nFin = ((pos_X+x) + (pos_Y+y)*GetRenderer()->GetWidth()*SliceCount);
				int nSub = ((			 x) + (			 y)*GetRenderer()->GetWidth());

				for(int c=0; c<4; c++)
					pFinalImage[nFin*4+c] = lstSubImages[nImageId][nSub*4+c];
			}

			delete [] lstSubImages[nImageId];
			lstSubImages[nImageId]=0;
	}

	PrintMessagePlus(" ok");

	PrintMessage("Writing %s ... ", sFileName);

	if(strstr(sFileName,".tga"))
	{
		GetRenderer()->WriteTGA(pFinalImage,
			GetRenderer()->GetWidth()*SliceCount, 
			GetRenderer()->GetHeight()*SliceCount, 
			sFileName, 32,32);
	}
	else
	{
		GetRenderer()->WriteJPG(pFinalImage,
			GetRenderer()->GetWidth()*SliceCount, 
			GetRenderer()->GetHeight()*SliceCount, 
			sFileName,32);
	}

	free(pFinalImage);*/

	PrintMessagePlus(" ok");

	GetTimer()->EnableTimer(true);

#endif
}



bool C3DEngine::ScreenShotMap(			CStitchedImage* pStitchedImage,
															const int							nRenderFlags,
															const CCamera&				_cam,
															const int							dwDrawFlags,
															const int							nFilterFlags,
															const uint32					SliceCount,
															const f32							fTransitionSize)
{
#if !defined(LINUX)

	const uint32	nTSize		=	GetTerrain()->GetTerrainSize();//*GetTerrain()->GetSectorSize();
//	const f32			fTLX			=	GetCVars()->e_screenshot_map_topleft_x*static_cast<f32>(nTSize)+fTransitionSize*GetRenderer()->GetWidth();
//	const f32			fTLY			=	(1.f-GetCVars()->e_screenshot_map_topleft_y)*static_cast<f32>(nTSize)+fTransitionSize*GetRenderer()->GetHeight();
//	const f32			fBRX			=	GetCVars()->e_screenshot_map_bottomright_x*static_cast<f32>(nTSize)+fTransitionSize*GetRenderer()->GetWidth();
//	const f32			fBRY			=	(1.f-GetCVars()->e_screenshot_map_bottomright_y)*static_cast<f32>(nTSize)+fTransitionSize*GetRenderer()->GetHeight();
	const f32			fTLX			=	GetCVars()->e_screenshot_map_center_x-GetCVars()->e_screenshot_map_size_x+fTransitionSize*GetRenderer()->GetWidth();
	const f32			fTLY			=	GetCVars()->e_screenshot_map_center_y-GetCVars()->e_screenshot_map_size_y+fTransitionSize*GetRenderer()->GetHeight();
	const f32			fBRX			=	GetCVars()->e_screenshot_map_center_x+GetCVars()->e_screenshot_map_size_x+fTransitionSize*GetRenderer()->GetWidth();
	const f32			fBRY			=	GetCVars()->e_screenshot_map_center_y+GetCVars()->e_screenshot_map_size_y+fTransitionSize*GetRenderer()->GetHeight();
	const f32			Height		=	GetCVars()->e_screenshot_map_camheight;
	const uint32	nSlicesX	=	SliceCount;
	const uint32	nSlicesY	=	SliceCount;

	string SettingsFileName = GetLevelFilePath("ScreenshotMap.Settings");

	FILE * metaFile = gEnv->pCryPak->FOpen(SettingsFileName,"wt");
	if(metaFile)
	{
		char Data[1024*8];
		snprintf(Data,sizeof(Data), "<Map CenterX=\"%f\" CenterY=\"%f\" SizeX=\"%f\" SizeY=\"%f\" Height=\"%f\"  Quality=\"%d\" />",
												GetCVars()->e_screenshot_map_center_x,
												GetCVars()->e_screenshot_map_center_y,
												GetCVars()->e_screenshot_map_size_x,
												GetCVars()->e_screenshot_map_size_y,
												GetCVars()->e_screenshot_map_camheight,
												GetCVars()->e_screenshot_quality);
		string data(Data);
		gEnv->pCryPak->FWrite(data.c_str(),data.size(),metaFile);
		gEnv->pCryPak->FClose(metaFile);
	}


	CCamera cam = _cam;
	Matrix34 tmX,tmY;
	tmX.SetRotationX(-gf_PI*0.5f);
	tmY.SetRotationY(-gf_PI*0.5f);
	Matrix34 tm	=	tmX*tmY;
	tm.SetTranslation(Vec3((fTLX+fBRX)*0.5f,(fTLY+fBRY)*0.5f,Height));
	cam.SetMatrix(tm);

	const f32 AngleX	=	atanf(((fBRX-fTLX)*0.5f)/Height);
	const f32 AngleY	=	atanf(((fTLY-fBRY)*0.5f)/Height);

	ICVar *r_drawnearfov = GetConsole()->GetCVar("r_DrawNearFoV");		assert(r_drawnearfov);
	const f32 drawnearfov_backup = r_drawnearfov->GetFVal();
	const f32 ViewingSize	=	min(cam.GetViewSurfaceX(),cam.GetViewSurfaceZ());
	cam.SetFrustum((int)ViewingSize,(int)ViewingSize,max(AngleX,AngleY)*2.f,Height-8000.f,Height+1000.f);
	r_drawnearfov->Set(-1);
	ScreenShotHighRes(pStitchedImage,nRenderFlags, cam, dwDrawFlags, nFilterFlags,SliceCount,fTransitionSize);
	r_drawnearfov->Set(drawnearfov_backup);

	return true;
#else		// LINUX
	return false;
#endif	// LINUX
}


bool C3DEngine::ScreenShotPanorama(CStitchedImage* pStitchedImage,const int nRenderFlags, const CCamera &_cam, const int dwDrawFlags, const int nFilterFlags,uint32 SliceCount,f32 fTransitionSize)
{
#if !defined(LINUX)

	//GetRenderer()->Update();

	float r_drawnearfov_backup=-1;
	ICVar *r_drawnearfov = GetConsole()->GetCVar("r_DrawNearFoV");		assert(r_drawnearfov);

	r_drawnearfov_backup = r_drawnearfov->GetFVal();
	r_drawnearfov->Set(-1);		// means the fov override should be switched off

	GetTimer()->EnableTimer(false);

	uint32 * pImage = new uint32[GetRenderer()->GetWidth()*GetRenderer()->GetHeight()];

	for(int iSlice=SliceCount-1;iSlice>=0;--iSlice)
	{
		if(iSlice==0)												// the last one should do eye adaption
			GetTimer()->EnableTimer(true);

		GetRenderer()->BeginFrame();

		Matrix33 rot; rot.SetIdentity();

		float fAngle = pStitchedImage->GetSliceAngle(iSlice);

		rot.SetRotationZ(fAngle);

		CCamera cam = _cam;

		Matrix34 tm = cam.GetMatrix();
		tm = tm * rot;
		tm.SetTranslation(_cam.GetPosition());
		cam.SetMatrix(tm);

		cam.SetFrustum(cam.GetViewSurfaceX(),cam.GetViewSurfaceZ(),pStitchedImage->m_fPanoramaShotVertFOV,cam.GetNearPlane(),cam.GetFarPlane());

		// render scene
		RenderInternal(nRenderFlags,cam,"ScreenShotPanorama",dwDrawFlags,nFilterFlags);

		GetRenderer()->ReadFrameBufferFast(pImage, GetRenderer()->GetWidth(), GetRenderer()->GetHeight());

		GetRenderer()->EndFrame();								// show last frame (from direction)

		const bool bFadeBorders = (iSlice+1)*2<=SliceCount;

		PrintMessage("PanoramaScreenShot %d/%d FadeBorders:%c (id: %d/%d)",iSlice+1,SliceCount,bFadeBorders?'t':'f',GetRenderer()->GetFrameID(false),GetRenderer()->GetFrameID(true));

		pStitchedImage->RasterizeCylinder(pImage,GetRenderer()->GetWidth(),GetRenderer()->GetHeight(),iSlice+1,bFadeBorders);

		// debug
//		m_pCurrentStitchedImage->SaveImage("PanoramaScreenShotsTest");

		if(GetCVars()->e_screenshot_quality<0)		// to debug FadeBorders
		if(iSlice*2==SliceCount)
		{
			pStitchedImage->Clear();
			PrintMessage("PanoramaScreenShot clear");
		}

	}
	delete [] pImage;

	r_drawnearfov->Set(r_drawnearfov_backup);

	return true;
#else		// LINUX
	return false;
#endif	// LINUX
}



void C3DEngine::SetupClearColor()
{
	FUNCTION_PROFILER_3DENGINE;

	bool bCameraInOutdoors = m_pVisAreaManager && !m_pVisAreaManager->m_pCurArea && !(m_pVisAreaManager->m_pCurPortal && m_pVisAreaManager->m_pCurPortal->m_lstConnections.Count()>1);
	GetRenderer()->SetClearColor(bCameraInOutdoors ? m_vFogColor : Vec3(0,0,0));
/*
	if(bCameraInOutdoors)
	if(GetCamera().GetPosition().z<GetWaterLevel() && m_pTerrain)
	{
		CTerrainNode * pSectorInfo = m_pTerrain ? m_pTerrain->GetSecInfo(GetCamera().GetPosition()) : 0;
		if(!pSectorInfo || !pSectorInfo->m_pFogVolume || GetCamera().GetPosition().z>pSectorInfo->m_pFogVolume->box.max.z)
		{
			if(GetCamera().GetPosition().z<GetWaterLevel() && m_pTerrain && 
				m_lstFogVolumes.Count() &&
				m_lstFogVolumes[0].bOcean)
				GetRenderer()->SetClearColor( m_lstFogVolumes[0].vColor );
		}
		//else if( pSectorInfo->m_pFogVolume->bOcean ) // makes problems if there is no skybox
			//GetRenderer()->SetClearColor( pSectorInfo->m_pFogVolume->vColor );
	}*/
}

void C3DEngine::FillDebugFPSInfo(SDebugFPSInfo& info) 
{
	float averageFrameTime = 0.0f;
	size_t realFrameNumber = 0;
	size_t frameNumber = 0;
	size_t frameNumberUnder1FPS = 0;
	size_t frameNumberUnder10FPS = 0;
	size_t frameNumberUnder15FPS = 0;
	size_t frameNumberUnder20FPS = 0;

	for (size_t i = 0, end = arrFrameTimeforSaveLevelStats.size(); i < end; ++i)
	{
		frameNumber++;

		if(arrFrameTimeforSaveLevelStats[i] <= 1.0f)
		{
			realFrameNumber++;
			averageFrameTime += arrFrameTimeforSaveLevelStats[i];

		if(arrFrameTimeforSaveLevelStats[i] > 1.0f/10.0f)
		{
			frameNumberUnder10FPS++;
		}
			else if(arrFrameTimeforSaveLevelStats[i] > 1.0f/15.0f)
		{
			frameNumberUnder15FPS++;
		}
			else if(arrFrameTimeforSaveLevelStats[i] > 1.0f/20.0f)
		{
			frameNumberUnder20FPS++;
		}
	}
		else
		{
			frameNumberUnder1FPS++;
		}
	}

	averageFrameTime /= (float)realFrameNumber;

	info.averageFPS = averageFrameTime > 0.0f ? 1.0f/averageFrameTime : 0.0f;
	info.frameNumber = frameNumber; 
	info.frameNumberUnder1FPS = frameNumberUnder1FPS;
	info.frameNumberUnder10FPS = frameNumberUnder10FPS;
	info.frameNumberUnder15FPS = frameNumberUnder15FPS;
	info.frameNumberUnder20FPS = frameNumberUnder20FPS;
}
