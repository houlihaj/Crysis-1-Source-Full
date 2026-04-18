/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 09:05:2005   14:32 : Created by Carsten Wenzel

*************************************************************************/

#ifndef _SKY_LIGHT_MANAGER_H_
#define _SKY_LIGHT_MANAGER_H_

#pragma once


#include <memory>
#include <vector>


class CSkyLightNishita;
struct ITimer;


class CSkyLightManager : public Cry3DEngineBase
{
public:
	struct SSkyDomeCondition
	{
		SSkyDomeCondition()
		: m_sunIntensity( 20.0f, 20.0f, 20.0f )
		, m_Km( 0.001f )
		, m_Kr( 0.00025f )
		, m_g( -0.99f )
		, m_rgbWaveLengths( 650.0f, 570.0f, 475.0f )
		, m_sunDirection( 0.0f, 0.707106f, 0.707106f )
		{
		}

		Vec3 m_sunIntensity;
		float m_Km;
		float m_Kr;
		float m_g;
		Vec3 m_rgbWaveLengths;
		Vec3 m_sunDirection;
	};

public:
	CSkyLightManager();
	~CSkyLightManager();

	// sky dome condition
	void SetSkyDomeCondition( const SSkyDomeCondition& skyDomeCondition );
	void GetCurSkyDomeCondition( SSkyDomeCondition& skyCond ) const;

	// controls updates
	void FullUpdate();
	void IncrementalUpdate( f32 updateRatioPerFrame );
	void SetQuality( int32 quality );
	
	// rendering params
	const SSkyLightRenderParams* GetRenderParams() const;

private:
	struct SFloatABGR32
	{
		SFloatABGR32() {}

		SFloatABGR32( float _r, float _g, float _b, float _a )
		: r( _r ), g( _g ), b( _b ), a( _a ) {}

		float r, g, b, a;
	} _ALIGN(16);

	typedef std::vector< SFloatABGR32 > SkyDomeTextureData;

private:
	void UpdateInternal( int32 numUpdates, bool callerIsFullUpdate = false );

	bool IsSkyDomeUpdateFinished() const;
	void InitSkyDomeMesh();

	int GetFrontBuffer() const;
	int GetBackBuffer() const;
	void ToggleBuffer();
	void UpdateRenderParams();

private:
	std::auto_ptr< CSkyLightNishita > m_pSkyLightNishita;
		
	SkyDomeTextureData m_skyDomeTextureDataMie[ 2 ];
	SkyDomeTextureData m_skyDomeTextureDataRayleigh[ 2 ];
	int32 m_skyDomeTextureTimeStamp[ 2 ];

	IRenderMesh* m_pSkyDomeMesh;

	SSkyDomeCondition m_curSkyDomeCondition;
	SSkyDomeCondition m_reqSkyDomeCondition;
	bool m_updateRequested;

	int32 m_numSkyDomeColorsComputed;
	int32 m_curBackBuffer;

	int32 m_lastFrameID;

	Vec3 m_curSkyHemiColor[5];
	Vec3 m_curHazeColor;
	Vec3 m_curHazeColorMieNoPremul;
	Vec3 m_curHazeColorRayleighNoPremul;
	
	Vec3 m_skyHemiColorAccum[5];
	Vec3 m_hazeColorAccum;
	Vec3 m_hazeColorMieNoPremulAccum;
	Vec3 m_hazeColorRayleighNoPremulAccum;

	SSkyLightRenderParams m_renderParams;
};


#endif // #ifndef _SKY_LIGHT_MANAGER_H_