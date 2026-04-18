////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   TimeOfDay.h
//  Version:     v1.00
//  Created:     25/10/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __TimeOfDay_h__
#define __TimeOfDay_h__

#define MAX_VAR_PARAM_COUNT 3

//////////////////////////////////////////////////////////////////////////
// ITimeOfDay interface implementation.
//////////////////////////////////////////////////////////////////////////
class CTimeOfDay : public ITimeOfDay
{
public:
	CTimeOfDay();
	~CTimeOfDay();

	//////////////////////////////////////////////////////////////////////////
	// ITimeOfDay 
	//////////////////////////////////////////////////////////////////////////
	virtual int GetVariableCount() { return m_vars.size(); };
	virtual bool GetVariableInfo( int nIndex,SVariableInfo &varInfo );
	virtual void SetVariableValue( int nIndex,float fValue[3] );

	virtual void ResetVariables();

	// Time of day is specified in hours.
	virtual void SetTime( float fHour,bool bForceUpdate=false );
	virtual float GetTime() { return m_fTime; };

	virtual void SetPaused(bool paused) { m_bPaused = paused; }

	virtual void SetAdvancedInfo( const SAdvancedInfo &advInfo );
	virtual void GetAdvancedInfo( SAdvancedInfo &advInfo );

	float GetHDRDynamicMultiplier() { return m_fHDRDynamicMultiplier; }

	virtual void Update( bool bInterpolate=true,bool bForceUpdate=false );

	virtual void Serialize( XmlNodeRef &node,bool bLoading );
	virtual void Serialize( TSerialize ser );

	virtual void SetTimer( ITimer * pTimer );

	virtual void NetSerialize( TSerialize ser, float lag, uint32 flags );

	virtual void Tick();
	//////////////////////////////////////////////////////////////////////////

	void BeginEditMode() { m_bEditMode = true; };
	void EndEditMode() { m_bEditMode = false; };

private:
	enum ETimeOfDayParamID
	{
		PARAM_HDR_DYNAMIC_POWER_FACTOR,

		PARAM_TERRAIN_OCCL_MULTIPLIER,
    PARAM_SSAO_MULTIPLIER,

		PARAM_SUN_COLOR,
		PARAM_SUN_COLOR_MULTIPLIER,
    PARAM_SUN_SPECULAR_MULTIPLIER,

		PARAM_SKY_COLOR,
		PARAM_SKY_COLOR_MULTIPLIER,

		PARAM_FOG_COLOR,
		PARAM_FOG_COLOR_MULTIPLIER,

		PARAM_VOLFOG_GLOBAL_DENSITY,
		PARAM_VOLFOG_ATMOSPHERE_HEIGHT,
		PARAM_VOLFOG_ARTIST_TWEAK_DENSITY_OFFSET,

		PARAM_SKYLIGHT_SUN_INTENSITY,
		PARAM_SKYLIGHT_SUN_INTENSITY_MULTIPLIER,

		PARAM_SKYLIGHT_KM,
		PARAM_SKYLIGHT_KR,
		PARAM_SKYLIGHT_G,

		PARAM_SKYLIGHT_WAVELENGTH_R,
		PARAM_SKYLIGHT_WAVELENGTH_G,
		PARAM_SKYLIGHT_WAVELENGTH_B,

		PARAM_NIGHSKY_HORIZON_COLOR,
		PARAM_NIGHSKY_HORIZON_COLOR_MULTIPLIER,
		PARAM_NIGHSKY_ZENITH_COLOR,
		PARAM_NIGHSKY_ZENITH_COLOR_MULTIPLIER,
		PARAM_NIGHSKY_ZENITH_SHIFT,

		PARAM_NIGHSKY_START_INTENSITY,

		PARAM_NIGHSKY_MOON_COLOR,
		PARAM_NIGHSKY_MOON_COLOR_MULTIPLIER,
		PARAM_NIGHSKY_MOON_INNERCORONA_COLOR,
		PARAM_NIGHSKY_MOON_INNERCORONA_COLOR_MULTIPLIER,
		PARAM_NIGHSKY_MOON_INNERCORONA_SCALE,
		PARAM_NIGHSKY_MOON_OUTERCORONA_COLOR,
		PARAM_NIGHSKY_MOON_OUTERCORONA_COLOR_MULTIPLIER,
		PARAM_NIGHSKY_MOON_OUTERCORONA_SCALE,

		PARAM_CLOUDSHADING_SUNLIGHT_MULTIPLIER,
		PARAM_CLOUDSHADING_SKYLIGHT_MULTIPLIER,

    PARAM_SUN_SHAFTS_VISIBILITY,
    PARAM_SUN_RAYS_VISIBILITY,
    PARAM_SUN_RAYS_ATTENUATION,

		PARAM_OCEANFOG_COLOR_MULTIPLIER,

		PARAM_SKYBOX_MULTIPLIER,

    PARAM_COLORGRADING_COLOR_SATURATION,
    PARAM_COLORGRADING_COLOR_CONTRAST,
    PARAM_COLORGRADING_COLOR_BRIGHTNESS,

    PARAM_COLORGRADING_LEVELS_MININPUT,
    PARAM_COLORGRADING_LEVELS_GAMMA,
    PARAM_COLORGRADING_LEVELS_MAXINPUT,
    PARAM_COLORGRADING_LEVELS_MINOUTPUT,
    PARAM_COLORGRADING_LEVELS_MAXOUTPUT,

    PARAM_COLORGRADING_SELCOLOR_COLOR,
    PARAM_COLORGRADING_SELCOLOR_CYANS,
    PARAM_COLORGRADING_SELCOLOR_MAGENTAS,
    PARAM_COLORGRADING_SELCOLOR_YELLOWS,
    PARAM_COLORGRADING_SELCOLOR_BLACKS,

    PARAM_COLORGRADING_FILTERS_GRAIN,
    PARAM_COLORGRADING_FILTERS_SHARPENING,
    PARAM_COLORGRADING_FILTERS_PHOTOFILTER_COLOR,
    PARAM_COLORGRADING_FILTERS_PHOTOFILTER_DENSITY,

    PARAM_EYEADAPTION_CLAMP,

    PARAM_COLORGRADING_DOF_FOCUSRANGE,
    PARAM_COLORGRADING_DOF_BLURAMOUNT,

		PARAM_TOTAL
	};

private:
	int AddVar( const char *group,const char *displayName,const char *name,int nParamId,EVariableType type,float defVal0=0,float defVal1=0,float defVal2=0 );
	SVariableInfo& GetVar( ETimeOfDayParamID id );
	void UpdateEnvLighting( bool forceUpdate );

private:
	std::vector<SVariableInfo> m_vars;
	std::map<const char*,int,stl::less_strcmp<const char*> > m_varsMap;
	float m_fTime;
	bool m_bEditMode;
	bool m_bPaused;

	SAdvancedInfo m_advancedInfo;
	ITimer * m_pTimer;
	float m_fHDRDynamicMultiplier;
	ICVar * m_pTimeOfDaySpeedCVar;
};

#endif //__TimeOfDay_h__
