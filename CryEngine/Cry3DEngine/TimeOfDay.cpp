////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   TimeOfDay.cpp
//  Version:     v1.00
//  Created:     25/10/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "TimeOfDay.h"
#include "terrain_water.h"
#include <ISplines.h>

class CCatmullRomSplineFloat : public spline::CBaseSplineInterpolator<float,spline::CatmullRomSpline<float> >
{
public:
	float m_fMinValue;
	float m_fMaxValue;

	virtual int GetNumDimensions() { return 1; };
	virtual ESplineType GetSplineType() { return ESPLINE_CATMULLROM; }

	virtual void Interpolate( float time,ValueType &value )
	{
		value_type v;
		interpolate( time,v );
		ToValueType(v,value);
		// Clamp values
		//value[0] = clamp_tpl(value[0],m_fMinValue,m_fMaxValue);
	}

	//////////////////////////////////////////////////////////////////////////
	void SerializeSpline( XmlNodeRef &node,bool bLoading )
	{
		if (bLoading)
		{
			string keystr = node->getAttr( "Keys" );

			resize(0);

			int curPos= 0;
			string key = keystr.Tokenize(",",curPos);
			while (!key.empty())
			{
				float time,v;
				sscanf( key,"%g:%g",&time,&v );
				ValueType val;
				val[0] = v;
				InsertKey(time,val);
				key = keystr.Tokenize(",",curPos);
			};

		}
		else
		{
			string keystr;
			string skey;
			for (int i = 0; i < num_keys(); i++)
			{
				skey.Format("%g:%g,",key(i).time,key(i).value );
				keystr += skey;
			}
			node->setAttr( "Keys",keystr );
		}
	}
};

//////////////////////////////////////////////////////////////////////////
class CCatmullRomSplineVec3 : public spline::CBaseSplineInterpolator<Vec3,spline::CatmullRomSpline<Vec3> >
{
public:
	virtual int GetNumDimensions() { return 3; };
	virtual ESplineType GetSplineType() { return ESPLINE_CATMULLROM; }

	virtual void Interpolate( float time,ValueType &value )
	{
		value_type v;
		interpolate( time,v );
		ToValueType(v,value);
		// Clamp for colors.
		//value[0] = clamp_tpl(value[0],0.0f,1.0f);
		//value[1] = clamp_tpl(value[1],0.0f,1.0f);
		//value[2] = clamp_tpl(value[2],0.0f,1.0f);
	}

	//////////////////////////////////////////////////////////////////////////
	void SerializeSpline( XmlNodeRef &node,bool bLoading )
	{
		if (bLoading)
		{
			string keystr = node->getAttr( "Keys" );

			resize(0);

			int curPos= 0;
			string key = keystr.Tokenize(",",curPos);
			while (!key.empty())
			{
				float time,val0,val1,val2;
				sscanf( key,"%g:(%g:%g:%g),",&time,&val0,&val1,&val2 );
				ValueType val;
				val[0] = val0; val[1] = val1; val[2] = val2;
				InsertKey(time,val);
				key = keystr.Tokenize(",",curPos);
			};

		}
		else
		{
			string keystr;
			string skey;
			for (int i = 0; i < num_keys(); i++)
			{
				skey.Format("%g:(%g:%g:%g),",key(i).time,key(i).value.x,key(i).value.y,key(i).value.z );
				keystr += skey;
			}
			node->setAttr( "Keys",keystr );
		}
	}

	void ClampValues( float fMinValue,float fMaxValue )
	{
		for (int i = 0, nkeys = num_keys(); i < nkeys; i++ )
		{
			ValueType val;
			if (GetKeyValue(i,val))
			{
				//val[0] = clamp_tpl(val[0],fMinValue,fMaxValue);
				//val[1] = clamp_tpl(val[1],fMinValue,fMaxValue);
				//val[2] = clamp_tpl(val[2],fMinValue,fMaxValue);
				SetKeyValue(i,val);
			}
		}

	}
};

//////////////////////////////////////////////////////////////////////////
CTimeOfDay::CTimeOfDay()
{
	m_pTimer = 0;
	SetTimer( gEnv->pTimer );
	m_fTime = 12;
	m_bEditMode = false;
	m_advancedInfo.fAnimSpeed = 0;
	m_advancedInfo.fStartTime = 0;
	m_advancedInfo.fEndTime = 24;
	m_fHDRDynamicMultiplier = 1.f;
	m_pTimeOfDaySpeedCVar = gEnv->pConsole->GetCVar("e_time_of_day_speed");

	m_bPaused = false;

	ResetVariables();
}

//////////////////////////////////////////////////////////////////////////
CTimeOfDay::~CTimeOfDay()
{
	for (int i = 0; i < m_vars.size(); i++)
	{
		switch (m_vars[i].type)
		{
		case TYPE_FLOAT:
			delete (CCatmullRomSplineFloat*)(m_vars[i].pInterpolator);
			break;
		case TYPE_COLOR:
			delete (CCatmullRomSplineVec3*)(m_vars[i].pInterpolator);
			break;
		}
	}
}

void CTimeOfDay::SetTimer( ITimer * pTimer )
{
	assert(pTimer);
	m_pTimer = pTimer;

  // Update timer for ocean also - Craig
  COcean::SetTimer( pTimer );
}

ITimeOfDay::SVariableInfo& CTimeOfDay::GetVar( ETimeOfDayParamID id )
{
	assert( id == m_vars[ id ].nParamId );
	return( m_vars[ id ] );
}

//////////////////////////////////////////////////////////////////////////
int CTimeOfDay::AddVar( const char *group,const char *displayName,const char *name,int nParamId,EVariableType type,float defVal0,float defVal1,float defVal2 )
{
	//assert( nParamId == m_vars.size() );

	SVariableInfo var;
	var.name = name;
	if (*displayName)
		var.displayName = displayName;
	else
		var.displayName = name;
	if (*group == 0)
		var.group = "Default";
	else
		var.group = group;
	var.type = type;
	var.nParamId = nParamId;
	var.fValue[0] = defVal0;
	var.fValue[1] = defVal1;
	var.fValue[2] = defVal2;
	var.pInterpolator = 0;
	var.bSelected = 0;
	switch(type) {
	case TYPE_FLOAT:
		{
			CCatmullRomSplineFloat *pSpline = new CCatmullRomSplineFloat;
			pSpline->m_fMinValue = defVal1;
			pSpline->m_fMaxValue = defVal2;
			pSpline->InsertKeyFloat( 0,defVal0 );
			pSpline->InsertKeyFloat( 1,defVal0 );
			var.pInterpolator = pSpline;
		}
		break;
	case TYPE_COLOR:
		{
			CCatmullRomSplineVec3 *pSpline = new CCatmullRomSplineVec3;
			pSpline->InsertKeyFloat3( 0,var.fValue );
			pSpline->InsertKeyFloat3( 1,var.fValue );
			var.pInterpolator = pSpline;
		}
		break;
	}
	//m_vars.push_back(var);
	m_vars[nParamId] = var;
	m_varsMap[var.name] = nParamId;
	return m_vars.size()-1;
}

//////////////////////////////////////////////////////////////////////////
bool CTimeOfDay::GetVariableInfo( int nIndex,SVariableInfo &varInfo )
{
	if (nIndex < 0 || nIndex >= m_vars.size())
		return false;
	
	varInfo = m_vars[nIndex];
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::SetVariableValue( int nIndex,float fValue[3] )
{
	if (nIndex < 0 || nIndex >= m_vars.size())
		return;

	m_vars[nIndex].fValue[0] = fValue[0];
	m_vars[nIndex].fValue[1] = fValue[1];
	m_vars[nIndex].fValue[2] = fValue[2];
}
//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::ResetVariables()
{
	m_vars.resize( PARAM_TOTAL );

	// NOTE: Add variables in the same order as their IDs were enumerated (avoids 
	// translation via a map)! AddVar and GetVar will check this in debug...

	AddVar( "Sky","","HDR dynamic power factor", PARAM_HDR_DYNAMIC_POWER_FACTOR, TYPE_FLOAT, 0.0f, -4.0f, 4.0f );

	AddVar( "Sky","","Sky brightening (terrain occlusion)", PARAM_TERRAIN_OCCL_MULTIPLIER, TYPE_FLOAT, 0.3f, 0.f, 1.f );
	AddVar( "Sky","","SSAO amount multiplier", PARAM_SSAO_MULTIPLIER, TYPE_FLOAT, 1.f, 0.f, 2.5f );

	AddVar( "Sky","","Sun color", PARAM_SUN_COLOR, TYPE_COLOR, 215.0f / 255.0f, 200.0f / 255.0f, 170.0f / 255.0f );
	AddVar( "Sky","","Sun color multiplier", PARAM_SUN_COLOR_MULTIPLIER, TYPE_FLOAT, 2.4f, 0.0f, 16.0f );
	AddVar( "Sky","","Sun specular multiplier", PARAM_SUN_SPECULAR_MULTIPLIER, TYPE_FLOAT, 1.0f, 0.0f, 4.0f );

	AddVar( "Sky","","Sky color", PARAM_SKY_COLOR, TYPE_COLOR, 160.0f / 255.0f, 200.0f / 255.0f, 240.0f / 255.0f );
	AddVar( "Sky","","Sky color multiplier", PARAM_SKY_COLOR_MULTIPLIER, TYPE_FLOAT, 1.1f, 0.0f, 16.0f );

	AddVar( "Fog","color","Fog color", PARAM_FOG_COLOR, TYPE_COLOR, 0.0f, 0.0f, 0.0f );
	AddVar( "Fog","color multiplier","Fog color multiplier", PARAM_FOG_COLOR_MULTIPLIER, TYPE_FLOAT, 0.0f, 0.0f, 16.0f );

	AddVar( "Fog","Global density","Volumetric fog: Global density", PARAM_VOLFOG_GLOBAL_DENSITY, TYPE_FLOAT, 0.02f, 0.0f, 100.0f );
	AddVar( "Fog","Atmosphere height","Volumetric fog: Atmosphere height", PARAM_VOLFOG_ATMOSPHERE_HEIGHT, TYPE_FLOAT, 4000.0f, 100.0f, 30000.0f );
	AddVar( "Fog","Density offset (in reality 0)","Volumetric fog: Density offset (in reality 0)", PARAM_VOLFOG_ARTIST_TWEAK_DENSITY_OFFSET, TYPE_FLOAT, 0.0f, 0.0f, 100.0f );	

	AddVar( "Sky Light","Sun intensity","Sky light: Sun intensity", PARAM_SKYLIGHT_SUN_INTENSITY, TYPE_COLOR, 5.0f / 6.0f, 5.0f / 6.0f, 1.0f );
	AddVar( "Sky Light","Sun intensity multiplier","Sky light: Sun intensity multiplier", PARAM_SKYLIGHT_SUN_INTENSITY_MULTIPLIER, TYPE_FLOAT, 30.0f, 0.0f, 1000.0f );
	AddVar( "Sky Light","Mie scattering","Sky light: Mie scattering", PARAM_SKYLIGHT_KM, TYPE_FLOAT, 4.8f, 0.0f, 1e6f );
	AddVar( "Sky Light","Rayleigh scattering","Sky light: Rayleigh scattering", PARAM_SKYLIGHT_KR, TYPE_FLOAT, 2.0f, 0.0f, 1e6f );
	AddVar( "Sky Light","Sun anisotropy factor","Sky light: Sun anisotropy factor", PARAM_SKYLIGHT_G, TYPE_FLOAT, -0.995f, -0.9999f, 0.9999f );
	AddVar( "Sky Light","Wavelength (R)","Sky light: Wavelength (R)", PARAM_SKYLIGHT_WAVELENGTH_R, TYPE_FLOAT, 750.0f, 380.0f, 780.0f );
	AddVar( "Sky Light","Wavelength (G)","Sky light: Wavelength (G)", PARAM_SKYLIGHT_WAVELENGTH_G, TYPE_FLOAT, 601.0f, 380.0f, 780.0f );
	AddVar( "Sky Light","Wavelength (B)","Sky light: Wavelength (B)", PARAM_SKYLIGHT_WAVELENGTH_B, TYPE_FLOAT, 555.0f, 380.0f, 780.0f );

	AddVar( "Night Sky","Horizon color","Night sky: Horizon color", PARAM_NIGHSKY_HORIZON_COLOR, TYPE_COLOR, 222.0f / 255.0f, 148.0f / 255.0f, 47.0f / 255.0f );
	AddVar( "Night Sky","Zenith color","Night sky: Zenith color", PARAM_NIGHSKY_ZENITH_COLOR, TYPE_COLOR, 17.0f / 255.0f, 38.0f / 255.0f, 78.0f / 255.0f );
	AddVar( "Night Sky","Zenith shift","Night sky: Zenith shift", PARAM_NIGHSKY_ZENITH_SHIFT, TYPE_FLOAT, 0.25f, 0.0f, 1.0f );
	AddVar( "Night Sky","Star intensity","Night sky: Star intensity", PARAM_NIGHSKY_START_INTENSITY, TYPE_FLOAT, 0.0f, 0.0f, 3.0f );
	AddVar( "Night Sky","Moon color","Night sky: Moon color", PARAM_NIGHSKY_MOON_COLOR, TYPE_COLOR, 255.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f );
	AddVar( "Night Sky","Moon inner corona color","Night sky: Moon inner corona color", PARAM_NIGHSKY_MOON_INNERCORONA_COLOR, TYPE_COLOR, 230.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f );	
	AddVar( "Night Sky","Moon inner corona scale","Night sky: Moon inner corona scale", PARAM_NIGHSKY_MOON_INNERCORONA_SCALE, TYPE_FLOAT, 0.499f, 0.0f, 2.0f );
	AddVar( "Night Sky","Moon outer corona color","Night sky: Moon outer corona color", PARAM_NIGHSKY_MOON_OUTERCORONA_COLOR, TYPE_COLOR, 128.0f / 255.0f, 200.0f / 255.0f, 255.0f / 255.0f );	
	AddVar( "Night Sky","Moon outer corona scale","Night sky: Moon outer corona scale", PARAM_NIGHSKY_MOON_OUTERCORONA_SCALE, TYPE_FLOAT, 0.006f, 0.0f, 2.0f );

	AddVar( "Night Sky Multiplier","Horizon color","Night sky: Horizon color multiplier", PARAM_NIGHSKY_HORIZON_COLOR_MULTIPLIER, TYPE_FLOAT, 1.0f, 0.0f, 16.0f );
	AddVar( "Night Sky Multiplier","Zenith color","Night sky: Zenith color multiplier", PARAM_NIGHSKY_ZENITH_COLOR_MULTIPLIER, TYPE_FLOAT, 0.25f, 0.0f, 16.0f );
	AddVar( "Night Sky Multiplier","Moon color","Night sky: Moon color multiplier", PARAM_NIGHSKY_MOON_COLOR_MULTIPLIER, TYPE_FLOAT, 0.0f, 0.0f, 16.0f );
	AddVar( "Night Sky Multiplier","Moon inner corona color","Night sky: Moon inner corona color multiplier", PARAM_NIGHSKY_MOON_INNERCORONA_COLOR_MULTIPLIER, TYPE_FLOAT, 0.0f, 0.0f, 16.0f );
	AddVar( "Night Sky Multiplier","Moon outer corona color","Night sky: Moon outer corona color multiplier", PARAM_NIGHSKY_MOON_OUTERCORONA_COLOR_MULTIPLIER, TYPE_FLOAT, 0.0f, 0.0f, 16.0f );
	///

	AddVar( "Cloud Shading","Sun contribution","Cloud shading: Sun light multiplier", PARAM_CLOUDSHADING_SUNLIGHT_MULTIPLIER, TYPE_FLOAT, 1.96f, 0.0f, 16.0f );
	AddVar( "Cloud Shading","Sky contribution","Cloud shading: Sky light multiplier", PARAM_CLOUDSHADING_SKYLIGHT_MULTIPLIER, TYPE_FLOAT, 0.8f,  0.0f, 16.0f );

	AddVar( "Sun Rays Effect","","Sun shafts visibility", PARAM_SUN_SHAFTS_VISIBILITY, TYPE_FLOAT, 0.25f,  0.0f, 1.0f );
	AddVar( "Sun Rays Effect","","Sun rays visibility", PARAM_SUN_RAYS_VISIBILITY, TYPE_FLOAT, 2.5f,  0.0f, 10.0f );
	AddVar( "Sun Rays Effect","","Sun rays attenuation", PARAM_SUN_RAYS_ATTENUATION, TYPE_FLOAT, 5.0f,  0.0f, 10.0f );

	AddVar( "Color Filter","Saturation","Color: saturation", PARAM_COLORGRADING_COLOR_SATURATION, TYPE_FLOAT, 1.0f, 0.0f, 10.0f );
	AddVar( "Color Filter","Contrast","Color: contrast", PARAM_COLORGRADING_COLOR_CONTRAST, TYPE_FLOAT, 1.0f, 0.0f, 10.0f );
	AddVar( "Color Filter","Brightness","Color: brightness", PARAM_COLORGRADING_COLOR_BRIGHTNESS, TYPE_FLOAT, 1.0f, 0.0f, 10.0f );

	AddVar( "Color Levels","Min input","Levels: min input", PARAM_COLORGRADING_LEVELS_MININPUT, TYPE_FLOAT, 0.0f, 0.0f, 255.0f );
	AddVar( "Color Levels","Gamma","Levels: gamma", PARAM_COLORGRADING_LEVELS_GAMMA, TYPE_FLOAT, 1.0f, 0.0f, 10.0f );
	AddVar( "Color Levels","Max input","Levels: max input", PARAM_COLORGRADING_LEVELS_MAXINPUT, TYPE_FLOAT, 255.0f, 0.0f, 255.0f );
	AddVar( "Color Levels","Min output","Levels: min output", PARAM_COLORGRADING_LEVELS_MINOUTPUT, TYPE_FLOAT, 0.0f, 0.0f, 0.0f );
	AddVar( "Color Levels","Max output","Levels: max output", PARAM_COLORGRADING_LEVELS_MAXOUTPUT, TYPE_FLOAT, 255.0f, 0.0f, 255.0f );

	AddVar( "Selective Color","Color","Selective Color: color", PARAM_COLORGRADING_SELCOLOR_COLOR, TYPE_COLOR, 0.0f, 1.0f, 1.0f);
	AddVar( "Selective Color","Cyans","Selective Color: cyans", PARAM_COLORGRADING_SELCOLOR_CYANS, TYPE_FLOAT, 0.0f, -100.0f, 100.0f );
	AddVar( "Selective Color","Magentas","Selective Color: magentas", PARAM_COLORGRADING_SELCOLOR_MAGENTAS, TYPE_FLOAT, 0.0f, -100.0f, 100.0f );
	AddVar( "Selective Color","Yellows","Selective Color: yellows", PARAM_COLORGRADING_SELCOLOR_YELLOWS, TYPE_FLOAT, 0.0f, -100.0f, 100.0f );
	AddVar( "Selective Color","Blacks","Selective Color: blacks", PARAM_COLORGRADING_SELCOLOR_BLACKS, TYPE_FLOAT, 0.0f, -100.0f, 100.0f );

	AddVar( "Filters","Grain","Filters: grain", PARAM_COLORGRADING_FILTERS_GRAIN, TYPE_FLOAT, 0.0f, 0.0f, 1.0f );
	AddVar( "Filters","Sharpening","Filters: sharpening", PARAM_COLORGRADING_FILTERS_SHARPENING, TYPE_FLOAT, 0.0f, 0.0f, 1.0f );
	AddVar( "Filters","Photofilter color","Filters: photofilter color", PARAM_COLORGRADING_FILTERS_PHOTOFILTER_COLOR, TYPE_COLOR, 0.952f, 0.517f, 0.09f);
	AddVar( "Filters","Photofilter density","Filters: photofilter density", PARAM_COLORGRADING_FILTERS_PHOTOFILTER_DENSITY, TYPE_FLOAT, 0.0f, 0.0f, 1.0f );

	AddVar( "Depth Of Field","Focus range","Dof: focus range", PARAM_COLORGRADING_DOF_FOCUSRANGE, TYPE_FLOAT, 1000.0f, 0.0f, 10000.0f );
	AddVar( "Depth Of Field","Blur amount","Dof: blur amount", PARAM_COLORGRADING_DOF_BLURAMOUNT, TYPE_FLOAT, 0.0f, 0.0f, 1.0f );

	AddVar( "Advanced","","EyeAdaption: Clamp", PARAM_EYEADAPTION_CLAMP, TYPE_FLOAT, 4.0f, 0.0f, 10.0f );
	AddVar( "Advanced","","Ocean fog color multiplier", PARAM_OCEANFOG_COLOR_MULTIPLIER, TYPE_FLOAT, 1.0f, 0.0f, 1.0f );
	AddVar( "Advanced","","Skybox multiplier", PARAM_SKYBOX_MULTIPLIER, TYPE_FLOAT, 1.0f, 0.0f, 1.0f );
}
//////////////////////////////////////////////////////////////////////////
// Time of day is specified in hours.
void CTimeOfDay::SetTime( float fHour,bool bForceUpdate )
{
	// set new time
	m_fTime = fHour;
	
	// Change time variable.
	Cry3DEngineBase::GetCVars()->e_time_of_day = m_fTime;

	Update( true,bForceUpdate );
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::Update( bool bInterpolate,bool bForceUpdate )
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_3DENGINE);

	if (bInterpolate)
	{
		// normalized time for interpolation
		float t = m_fTime / 24.0f;

		// interpolate all values
		for (int i = 0; i < m_vars.size(); i++)
		{
			SVariableInfo &var = m_vars[i];
			if (var.pInterpolator)
			{
				if (var.pInterpolator->GetNumDimensions() == 1)
					var.pInterpolator->InterpolateFloat(t,var.fValue[0]);
				else if (var.pInterpolator->GetNumDimensions() == 3)
					var.pInterpolator->InterpolateFloat3(t,var.fValue);

				switch(var.type)
				{
				case TYPE_FLOAT:
					{
						var.fValue[0] = clamp_tpl( var.fValue[0], var.fValue[1], var.fValue[2] );
						if (fabs(var.fValue[0]) < 1e-10f)
							var.fValue[0] = 0.0f;
						break;
					}
				case TYPE_COLOR:
					{
						var.fValue[0] = clamp_tpl( var.fValue[0], 0.0f, 1.0f );
						var.fValue[1] = clamp_tpl( var.fValue[1], 0.0f, 1.0f );
						var.fValue[2] = clamp_tpl( var.fValue[2], 0.0f, 1.0f );
						break;
					}
				default:
					{
						assert( 0 );
					}
				}
			}
		}
	}

	// update environment lighting according to new interpolated values
	UpdateEnvLighting( bForceUpdate );
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::UpdateEnvLighting( bool forceUpdate )
{
	C3DEngine* p3DEngine( (C3DEngine*)gEnv->p3DEngine );
	IRenderer* pRenderer( gEnv->pRenderer );

	if( pRenderer->EF_Query( EFQ_HDRModeEnabled ) )
		m_fHDRDynamicMultiplier = powf( p3DEngine->GetHDRDynamicMultiplier(), GetVar( PARAM_HDR_DYNAMIC_POWER_FACTOR ).fValue[ 0 ] );
	else
		m_fHDRDynamicMultiplier = 1.f;

	float skyBrightMultiplier( GetVar( PARAM_TERRAIN_OCCL_MULTIPLIER ).fValue[ 0 ]);
  float SSAOMultiplier( GetVar( PARAM_SSAO_MULTIPLIER ).fValue[ 0 ]);
	float sunMultiplier( GetVar( PARAM_SUN_COLOR_MULTIPLIER ).fValue[ 0 ] * m_fHDRDynamicMultiplier );
  float sunSpecMultiplier( GetVar( PARAM_SUN_SPECULAR_MULTIPLIER ).fValue[ 0 ] );
	float skyMultiplier( GetVar( PARAM_SKY_COLOR_MULTIPLIER ).fValue[ 0 ] * m_fHDRDynamicMultiplier );
	float fogMultiplier( GetVar( PARAM_FOG_COLOR_MULTIPLIER ).fValue[ 0 ] * m_fHDRDynamicMultiplier );
	float nightSkyHorizonMultiplier( GetVar( PARAM_NIGHSKY_HORIZON_COLOR_MULTIPLIER ).fValue[ 0 ] * m_fHDRDynamicMultiplier );
	float nightSkyZenithMultiplier( GetVar( PARAM_NIGHSKY_ZENITH_COLOR_MULTIPLIER ).fValue[ 0 ] * m_fHDRDynamicMultiplier );	
	float nightSkyMoonMultiplier( GetVar( PARAM_NIGHSKY_MOON_COLOR_MULTIPLIER ).fValue[ 0 ] * m_fHDRDynamicMultiplier );
	float nightSkyMoonInnerCoronaMultiplier( GetVar( PARAM_NIGHSKY_MOON_INNERCORONA_COLOR_MULTIPLIER ).fValue[ 0 ] * m_fHDRDynamicMultiplier );
	float nightSkyMoonOuterCoronaMultiplier( GetVar( PARAM_NIGHSKY_MOON_OUTERCORONA_COLOR_MULTIPLIER ).fValue[ 0 ] * m_fHDRDynamicMultiplier );

	Vec3 sunRotParams;
	p3DEngine->GetGlobalParameter( E3DPARAM_SKY_SUNROTATION, sunRotParams );

	// set sun position
	float timeAng( ( ( m_fTime + 12.0f ) / 24.0f ) * gf_PI * 2.0f );

	Vec3 sunPos;
	{
		float sunRot = gf_PI * ( -sunRotParams.x ) / 180.0f;
		float longitude = 0.5f * gf_PI - gf_PI * sunRotParams.y / 180.0f;

		Matrix33 a, b, c, m;

		a.SetRotationZ( timeAng );
		b.SetRotationX( longitude );
		c.SetRotationY( sunRot );

		m = a * b * c;
		sunPos = Vec3( 0, 1, 0 ) * m;

		float h = sunPos.z; 
		sunPos.z = sunPos.y;
		sunPos.y = -h;
	}
	
	// transition phase for sun/moon lighting
	assert(p3DEngine->m_dawnStart <= p3DEngine->m_dawnEnd);
	assert(p3DEngine->m_duskStart <= p3DEngine->m_duskEnd);
	assert(p3DEngine->m_dawnEnd <= p3DEngine->m_duskStart);
	float sunIntensityMultiplier(m_fHDRDynamicMultiplier);
	float dayNightIndicator(1.0);
	if (m_fTime < p3DEngine->m_dawnStart || m_fTime >= p3DEngine->m_duskEnd)
	{
		// night
		sunIntensityMultiplier = 0.0;
		p3DEngine->GetGlobalParameter( E3DPARAM_NIGHSKY_MOON_DIRECTION, sunPos );
		dayNightIndicator = 0.0;
	}
	else if(m_fTime < p3DEngine->m_dawnEnd)
	{
		// dawn
		assert(p3DEngine->m_dawnStart < p3DEngine->m_dawnEnd);
		float b(0.5f * (p3DEngine->m_dawnStart + p3DEngine->m_dawnEnd));
		if (m_fTime < b)
		{
			// fade out moon
			sunMultiplier *= (b - m_fTime) / (b - p3DEngine->m_dawnStart);
			sunIntensityMultiplier = 0.0;
			p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_DIRECTION, sunPos);
		}
		else
		{
			// fade in sun
			float t((m_fTime - b) / (p3DEngine->m_dawnEnd - b));
			sunMultiplier *= t;
			sunIntensityMultiplier = t;
		}

		dayNightIndicator = (m_fTime - p3DEngine->m_dawnStart) / (p3DEngine->m_dawnEnd - p3DEngine->m_dawnStart);
	}
	else if(m_fTime < p3DEngine->m_duskStart)
	{
		// day		
		dayNightIndicator = 1.0;
	}
	else if(m_fTime < p3DEngine->m_duskEnd)
	{
		// dusk
		assert(p3DEngine->m_duskStart < p3DEngine->m_duskEnd);
		float b(0.5f * (p3DEngine->m_duskStart + p3DEngine->m_duskEnd));
		if (m_fTime < b)
		{
			// fade out sun
			float t((b - m_fTime) / (b - p3DEngine->m_duskStart));
			sunMultiplier *= t;
			sunIntensityMultiplier = t;
		}
		else
		{
			// fade in moon
			sunMultiplier *= (m_fTime - b) / (p3DEngine->m_duskEnd - b);
			sunIntensityMultiplier = 0.0;
			p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_DIRECTION, sunPos);
		}
		
		dayNightIndicator = (p3DEngine->m_duskEnd - m_fTime) / (p3DEngine->m_duskEnd - p3DEngine->m_duskStart);
	}
	sunIntensityMultiplier *= max(GetVar(PARAM_SKYLIGHT_SUN_INTENSITY_MULTIPLIER).fValue[0], 0.0f);	
	p3DEngine->SetGlobalParameter(E3DPARAM_DAY_NIGHT_INDICATOR, Vec3(dayNightIndicator, 0, 0));

	p3DEngine->SetSunDir(sunPos);

	// set sun, sky, and fog color
	Vec3 sunColor( sunMultiplier * Vec3( GetVar( PARAM_SUN_COLOR ).fValue[ 0 ], 
		GetVar( PARAM_SUN_COLOR ).fValue[ 1 ], GetVar( PARAM_SUN_COLOR ).fValue[ 2 ] ) );	
	p3DEngine->SetSunColor( sunColor );
  p3DEngine->SetSunSpecMultiplier( sunSpecMultiplier );

  p3DEngine->SetSkyBrightness( skyBrightMultiplier );
  p3DEngine->SetSSAOAmount( SSAOMultiplier );

	Vec3 skyColor( skyMultiplier * Vec3( GetVar( PARAM_SKY_COLOR ).fValue[ 0 ], 
		GetVar( PARAM_SKY_COLOR ).fValue[ 1 ], GetVar( PARAM_SKY_COLOR ).fValue[ 2 ] ) );	
	p3DEngine->SetSkyColor( skyColor );

	Vec3 fogColor( fogMultiplier * Vec3( GetVar( PARAM_FOG_COLOR ).fValue[ 0 ], 
		GetVar( PARAM_FOG_COLOR ).fValue[ 1 ], GetVar( PARAM_FOG_COLOR ).fValue[ 2 ] ) );	
	p3DEngine->SetFogColor( fogColor );

	// set volumetric fog properties
	p3DEngine->SetVolumetricFogSettings( GetVar( PARAM_VOLFOG_GLOBAL_DENSITY ).fValue[ 0 ], 
		GetVar( PARAM_VOLFOG_ATMOSPHERE_HEIGHT ).fValue[ 0 ], GetVar( PARAM_VOLFOG_ARTIST_TWEAK_DENSITY_OFFSET ).fValue[ 0 ] );

	// set HDR sky lighting properties
	Vec3 sunIntensity( sunIntensityMultiplier * Vec3( GetVar( PARAM_SKYLIGHT_SUN_INTENSITY ).fValue[ 0 ], 
		GetVar( PARAM_SKYLIGHT_SUN_INTENSITY ).fValue[ 1 ], GetVar( PARAM_SKYLIGHT_SUN_INTENSITY ).fValue[ 2 ] ) );

	Vec3 rgbWaveLengths( GetVar( PARAM_SKYLIGHT_WAVELENGTH_R ).fValue[ 0 ], 
		GetVar( PARAM_SKYLIGHT_WAVELENGTH_G ).fValue[ 0 ], GetVar( PARAM_SKYLIGHT_WAVELENGTH_B ).fValue[ 0 ] );

	p3DEngine->SetSkyLightParameters( sunIntensity, GetVar( PARAM_SKYLIGHT_KM ).fValue[ 0 ], 
		GetVar( PARAM_SKYLIGHT_KR ).fValue[ 0 ], GetVar( PARAM_SKYLIGHT_G ).fValue[ 0 ], rgbWaveLengths, forceUpdate );				

	// set night sky color properties
	Vec3 nightSkyHorizonColor( nightSkyHorizonMultiplier * Vec3( GetVar( PARAM_NIGHSKY_HORIZON_COLOR ).fValue[ 0 ], 
		GetVar( PARAM_NIGHSKY_HORIZON_COLOR ).fValue[ 1 ], GetVar( PARAM_NIGHSKY_HORIZON_COLOR ).fValue[ 2 ] ) );
	p3DEngine->SetGlobalParameter( E3DPARAM_NIGHSKY_HORIZON_COLOR, nightSkyHorizonColor );

	Vec3 nightSkyZenithColor( nightSkyZenithMultiplier * Vec3( GetVar( PARAM_NIGHSKY_ZENITH_COLOR ).fValue[ 0 ], 
		GetVar( PARAM_NIGHSKY_ZENITH_COLOR ).fValue[ 1 ], GetVar( PARAM_NIGHSKY_ZENITH_COLOR ).fValue[ 2 ] ) );
	p3DEngine->SetGlobalParameter( E3DPARAM_NIGHSKY_ZENITH_COLOR, nightSkyZenithColor );

	float nightSkyZenithColorShift( GetVar( PARAM_NIGHSKY_ZENITH_SHIFT ).fValue[ 0 ] );
	p3DEngine->SetGlobalParameter( E3DPARAM_NIGHSKY_ZENITH_SHIFT, Vec3( nightSkyZenithColorShift, 0, 0 ) );

	float nightSkyStarIntensity( GetVar( PARAM_NIGHSKY_START_INTENSITY ).fValue[ 0 ] );
	p3DEngine->SetGlobalParameter( E3DPARAM_NIGHSKY_STAR_INTENSITY, Vec3( nightSkyStarIntensity, 0, 0 ) );

	Vec3 nightSkyMoonColor( nightSkyMoonMultiplier * Vec3( GetVar( PARAM_NIGHSKY_MOON_COLOR ).fValue[ 0 ], 
		GetVar( PARAM_NIGHSKY_MOON_COLOR ).fValue[ 1 ], GetVar( PARAM_NIGHSKY_MOON_COLOR ).fValue[ 2 ] ) );
	p3DEngine->SetGlobalParameter( E3DPARAM_NIGHSKY_MOON_COLOR, nightSkyMoonColor );

	Vec3 nightSkyMoonInnerCoronaColor( nightSkyMoonInnerCoronaMultiplier * Vec3( GetVar( PARAM_NIGHSKY_MOON_INNERCORONA_COLOR ).fValue[ 0 ], 
		GetVar( PARAM_NIGHSKY_MOON_INNERCORONA_COLOR ).fValue[ 1 ], GetVar( PARAM_NIGHSKY_MOON_INNERCORONA_COLOR ).fValue[ 2 ] ) );
	p3DEngine->SetGlobalParameter( E3DPARAM_NIGHSKY_MOON_INNERCORONA_COLOR, nightSkyMoonInnerCoronaColor );
 
	float nightSkyMoonInnerCoronaScale( GetVar( PARAM_NIGHSKY_MOON_INNERCORONA_SCALE ).fValue[ 0 ] );
	p3DEngine->SetGlobalParameter( E3DPARAM_NIGHSKY_MOON_INNERCORONA_SCALE, Vec3( nightSkyMoonInnerCoronaScale, 0, 0 ) );

	Vec3 nightSkyMoonOuterCoronaColor( nightSkyMoonOuterCoronaMultiplier * Vec3( GetVar( PARAM_NIGHSKY_MOON_OUTERCORONA_COLOR ).fValue[ 0 ], 
		GetVar( PARAM_NIGHSKY_MOON_OUTERCORONA_COLOR ).fValue[ 1 ], GetVar( PARAM_NIGHSKY_MOON_OUTERCORONA_COLOR ).fValue[ 2 ] ) );
	p3DEngine->SetGlobalParameter( E3DPARAM_NIGHSKY_MOON_OUTERCORONA_COLOR, nightSkyMoonOuterCoronaColor );

	float nightSkyMoonOuterCoronaScale( GetVar( PARAM_NIGHSKY_MOON_OUTERCORONA_SCALE ).fValue[ 0 ] );
	p3DEngine->SetGlobalParameter( E3DPARAM_NIGHSKY_MOON_OUTERCORONA_SCALE, Vec3( nightSkyMoonOuterCoronaScale, 0, 0 ) );

  // set sun shafts visibility and activate if required
  float fSunShaftsVis = GetVar( PARAM_SUN_SHAFTS_VISIBILITY ).fValue[ 0 ];
  fSunShaftsVis = clamp_tpl<float>(fSunShaftsVis, 0.0f, 0.3f);
  float fSunRaysVis = GetVar( PARAM_SUN_RAYS_VISIBILITY ).fValue[ 0 ];
  float fSunRaysAtten = GetVar( PARAM_SUN_RAYS_ATTENUATION ).fValue[ 0 ];

  p3DEngine->SetPostEffectParam( "SunShafts_Active", (fSunShaftsVis > 0.05 || fSunRaysVis > 0.05)? 1: 0);
  p3DEngine->SetPostEffectParam( "SunShafts_Amount", fSunShaftsVis );  
  p3DEngine->SetPostEffectParam( "SunShafts_RaysAmount", fSunRaysVis );  
  p3DEngine->SetPostEffectParam( "SunShafts_RaysAttenuation", fSunRaysAtten );  
    
	// set cloud shading multiplier
	p3DEngine->SetCloudShadingMultiplier( GetVar( PARAM_CLOUDSHADING_SUNLIGHT_MULTIPLIER ).fValue[ 0 ],
		GetVar( PARAM_CLOUDSHADING_SKYLIGHT_MULTIPLIER ).fValue[ 0 ] );

	// set ocean fog color multiplier
	float oceanFogColorMultiplier( GetVar( PARAM_OCEANFOG_COLOR_MULTIPLIER ).fValue[ 0 ] );
	p3DEngine->SetGlobalParameter( E3DPARAM_OCEANFOG_COLOR_MULTIPLIER, Vec3( oceanFogColorMultiplier, 0, 0 ) );

	// set skybox multiplier
	float skyBoxMulitplier( GetVar( PARAM_SKYBOX_MULTIPLIER ).fValue[ 0 ] * m_fHDRDynamicMultiplier );
	p3DEngine->SetGlobalParameter( E3DPARAM_SKYBOX_MULTIPLIER, Vec3( skyBoxMulitplier, 0, 0 ) );

	{
		float fVal = GetVar( PARAM_EYEADAPTION_CLAMP ).fValue[0];
		p3DEngine->SetGlobalParameter(E3DPARAM_EYEADAPTIONCLAMP,Vec3(fVal,0,0));
	}

  // Set color grading stuff
  float fValue = GetVar( PARAM_COLORGRADING_COLOR_SATURATION ).fValue[ 0 ];
  //p3DEngine->SetPostEffectParam("ColorGrading_Saturation", fValue);
  p3DEngine->SetGlobalParameter(E3DPARAM_COLORGRADING_COLOR_SATURATION,Vec3(fValue,0,0));

  fValue = GetVar( PARAM_COLORGRADING_COLOR_CONTRAST ).fValue[ 0 ];
  p3DEngine->SetPostEffectParam("ColorGrading_Contrast", fValue);
  fValue = GetVar( PARAM_COLORGRADING_COLOR_BRIGHTNESS ).fValue[ 0 ];
  p3DEngine->SetPostEffectParam("ColorGrading_Brightness", fValue);

  fValue = GetVar( PARAM_COLORGRADING_LEVELS_MININPUT ).fValue[ 0 ];
  p3DEngine->SetPostEffectParam("ColorGrading_minInput", fValue);
  fValue = GetVar( PARAM_COLORGRADING_LEVELS_GAMMA ).fValue[ 0 ];
  p3DEngine->SetPostEffectParam("ColorGrading_gammaInput", fValue);
  fValue = GetVar( PARAM_COLORGRADING_LEVELS_MAXINPUT ).fValue[ 0 ];
  p3DEngine->SetPostEffectParam("ColorGrading_maxInput", fValue);
  fValue = GetVar( PARAM_COLORGRADING_LEVELS_MINOUTPUT ).fValue[ 0 ];
  p3DEngine->SetPostEffectParam("ColorGrading_minOutput", fValue);
  fValue = GetVar( PARAM_COLORGRADING_LEVELS_MAXOUTPUT ).fValue[ 0 ];
  p3DEngine->SetPostEffectParam("ColorGrading_maxOutput", fValue);

  
  Vec4 pColor = Vec4( GetVar( PARAM_COLORGRADING_SELCOLOR_COLOR ).fValue[ 0 ], 
                      GetVar( PARAM_COLORGRADING_SELCOLOR_COLOR ).fValue[ 1 ],
                      GetVar( PARAM_COLORGRADING_SELCOLOR_COLOR ).fValue[ 2 ], 1.0f);
  p3DEngine->SetPostEffectParamVec4("clr_ColorGrading_SelectiveColor", pColor);
  fValue = GetVar( PARAM_COLORGRADING_SELCOLOR_CYANS ).fValue[ 0 ];
  p3DEngine->SetPostEffectParam("ColorGrading_SelectiveColorCyans", fValue);
  fValue = GetVar( PARAM_COLORGRADING_SELCOLOR_MAGENTAS ).fValue[ 0 ];
  p3DEngine->SetPostEffectParam("ColorGrading_SelectiveColorMagentas", fValue);
  fValue = GetVar( PARAM_COLORGRADING_SELCOLOR_YELLOWS ).fValue[ 0 ];
  p3DEngine->SetPostEffectParam("ColorGrading_SelectiveColorYellows", fValue);
  fValue = GetVar( PARAM_COLORGRADING_SELCOLOR_BLACKS ).fValue[ 0 ];
  p3DEngine->SetPostEffectParam("ColorGrading_SelectiveColorBlacks", fValue);

  fValue = GetVar( PARAM_COLORGRADING_FILTERS_GRAIN ).fValue[ 0 ];
//  p3DEngine->SetPostEffectParam("ColorGrading_GrainAmount", fValue);
  //p3DEngine->SetPostEffectParam("ColorGrading_Saturation", fValue);
  p3DEngine->SetGlobalParameter(E3DPARAM_COLORGRADING_FILTERS_GRAIN,Vec3(fValue,0,0));

  fValue = GetVar( PARAM_COLORGRADING_FILTERS_SHARPENING ).fValue[ 0 ];
  p3DEngine->SetPostEffectParam("ColorGrading_SharpenAmount", fValue);
  pColor = Vec4( GetVar( PARAM_COLORGRADING_FILTERS_PHOTOFILTER_COLOR ).fValue[ 0 ], 
                 GetVar( PARAM_COLORGRADING_FILTERS_PHOTOFILTER_COLOR ).fValue[ 1 ],
                 GetVar( PARAM_COLORGRADING_FILTERS_PHOTOFILTER_COLOR ).fValue[ 2 ], 1.0f);
  //p3DEngine->SetPostEffectParamVec4("clr_ColorGrading_PhotoFilterColor", pColor);
  p3DEngine->SetGlobalParameter(E3DPARAM_COLORGRADING_FILTERS_PHOTOFILTER_COLOR,Vec3(pColor.x,pColor.y,pColor.z));
  fValue = GetVar( PARAM_COLORGRADING_FILTERS_PHOTOFILTER_DENSITY ).fValue[ 0 ];
  p3DEngine->SetGlobalParameter(E3DPARAM_COLORGRADING_FILTERS_PHOTOFILTER_DENSITY,Vec3(fValue,0,0));
  //p3DEngine->SetPostEffectParam("ColorGrading_PhotoFilterColorDensity", fValue);

  fValue = GetVar( PARAM_COLORGRADING_DOF_FOCUSRANGE ).fValue[ 0 ];
  p3DEngine->SetPostEffectParam("Dof_Tod_FocusRange", fValue);

  fValue = GetVar( PARAM_COLORGRADING_DOF_BLURAMOUNT ).fValue[ 0 ];
  p3DEngine->SetPostEffectParam("Dof_Tod_BlurAmount", fValue);
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::SetAdvancedInfo( const SAdvancedInfo &advInfo )
{
	m_advancedInfo = advInfo;
	if (m_pTimeOfDaySpeedCVar->GetFVal() != m_advancedInfo.fAnimSpeed)
		m_pTimeOfDaySpeedCVar->Set(m_advancedInfo.fAnimSpeed);
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::GetAdvancedInfo( SAdvancedInfo &advInfo )
{
	advInfo = m_advancedInfo;
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::Serialize( XmlNodeRef &node,bool bLoading )
{
	if (bLoading)
	{
		node->getAttr( "Time",m_fTime );

		node->getAttr( "TimeStart",m_advancedInfo.fStartTime );
		node->getAttr( "TimeEnd",m_advancedInfo.fEndTime );
		node->getAttr( "TimeAnimSpeed",m_advancedInfo.fAnimSpeed );

		if (!m_bEditMode)
			m_fTime = m_advancedInfo.fStartTime;
/*
		if (m_fTime > m_advancedInfo.fEndTime)
			m_fTime = m_advancedInfo.fStartTime;
		if (m_fTime < m_advancedInfo.fStartTime)
			m_fTime = m_advancedInfo.fEndTime;
*/
		if (m_pTimeOfDaySpeedCVar->GetFVal() != m_advancedInfo.fAnimSpeed)
			m_pTimeOfDaySpeedCVar->Set(m_advancedInfo.fAnimSpeed);

		// Load.
		for (int i = 0; i < node->getChildCount(); i++)
		{
			XmlNodeRef varNode = node->getChild(i);
			int nParamId = stl::find_in_map( m_varsMap,varNode->getAttr("Name"),-1 );
			if (nParamId < 0 || nParamId >= PARAM_TOTAL)
				continue;
			
			XmlNodeRef splineNode = varNode->findChild( "Spline" );
			SVariableInfo &var = GetVar((ETimeOfDayParamID)nParamId);
			switch (var.type) {
			case TYPE_FLOAT:
				varNode->getAttr( "Value",var.fValue[0] );
				if (var.pInterpolator && splineNode != 0)
				{
					((CCatmullRomSplineFloat*)var.pInterpolator)->SerializeSpline( splineNode,bLoading );
				}
				break;
			case TYPE_COLOR:
				{
					Vec3 v(var.fValue[0],var.fValue[1],var.fValue[2]);
					varNode->getAttr( "Color",v );
					var.fValue[0] = v.x;
					var.fValue[1] = v.y;
					var.fValue[2] = v.z;

					if (var.pInterpolator && splineNode != 0)
					{
						CCatmullRomSplineVec3 *pCatmullRomSpline = ((CCatmullRomSplineVec3*)var.pInterpolator);
						pCatmullRomSpline->SerializeSpline( splineNode,bLoading );
						// Clamp colors in case too big colors are provided.
						pCatmullRomSpline->ClampValues( -100,100 );
					}
				}
				break;
			}
		}
		SetTime(m_fTime,true);
	}
	else
	{
		node->setAttr( "Time",m_fTime );
		node->setAttr( "TimeStart",m_advancedInfo.fStartTime );
		node->setAttr( "TimeEnd",m_advancedInfo.fEndTime );
		node->setAttr( "TimeAnimSpeed",m_advancedInfo.fAnimSpeed );
		// Save.
		for (int i = 0; i < m_vars.size(); i++)
		{
			SVariableInfo &var = m_vars[i];
			XmlNodeRef varNode = node->newChild("Variable");
			varNode->setAttr( "Name",var.name );
			switch (var.type) {
			case TYPE_FLOAT:
				varNode->setAttr( "Value",var.fValue[0] );
				if (var.pInterpolator)
				{
					XmlNodeRef splineNode = varNode->newChild("Spline");
					((CCatmullRomSplineFloat*)var.pInterpolator)->SerializeSpline( splineNode,bLoading );
				}
				break;
			case TYPE_COLOR:
				varNode->setAttr( "Color",Vec3(var.fValue[0],var.fValue[1],var.fValue[2]) );
				if (var.pInterpolator)
				{
					XmlNodeRef splineNode = varNode->newChild("Spline");
					((CCatmullRomSplineVec3*)var.pInterpolator)->SerializeSpline( splineNode,bLoading );
				}
				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::Serialize( TSerialize ser )
{
	assert(ser.GetSerializationTarget() != eST_Network);

	string tempName;

	ser.Value("time", m_fTime);
	ser.Value("mode", m_bEditMode);
	int size = m_vars.size();
	ser.BeginGroup("VariableValues");
	for(int v = 0; v < size; v++)
	{
		tempName = m_vars[v].name;
		tempName.replace( ' ','_' );
		tempName.replace( '(','_' );
		tempName.replace( ')','_' );
		tempName.replace( ':','_' );
		ser.BeginGroup(tempName);
		ser.Value("Val0", m_vars[v].fValue[0]);
		ser.Value("Val1", m_vars[v].fValue[1]);
		ser.Value("Val2", m_vars[v].fValue[2]);
		ser.EndGroup();
	}
	ser.EndGroup();

	ser.Value("AdvInfoSpeed", m_advancedInfo.fAnimSpeed);
	ser.Value("AdvInfoStart", m_advancedInfo.fStartTime);
	ser.Value("AdvInfoEnd", m_advancedInfo.fEndTime);

	if (ser.IsReading())
	{
		SetTime(m_fTime, true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::NetSerialize( TSerialize ser, float lag, uint32 flags )
{
	if (0 == (flags & NETSER_STATICPROPS))
	{
		if (ser.IsWriting())
		{
			ser.Value("time", m_fTime, 'tod');
		}
		else
		{
			float serializedTime;
			ser.Value("time", serializedTime, 'tod');
			float remoteTime = serializedTime + ((flags & NETSER_COMPENSATELAG) != 0) * m_advancedInfo.fAnimSpeed * lag;
			float setTime = remoteTime;
			if (0 == (flags & NETSER_FORCESET))
			{
				static const float adjustmentFactor = 0.05f;
				static const float wraparoundGuardHours = 2.0f;

				float localTime = m_fTime;
				// handle wraparound
				if (localTime < wraparoundGuardHours && remoteTime > (24.0f - wraparoundGuardHours))
					localTime += 24.0f;
				else if (remoteTime < wraparoundGuardHours && localTime > (24.0f - wraparoundGuardHours))
					remoteTime += 24.0f;
				// don't blend times if they're very different
				if (fabsf(remoteTime - localTime) < 1.0f)
				{
					setTime = adjustmentFactor * remoteTime + (1.0f - adjustmentFactor) * m_fTime;
					if (setTime > 24.0f)
						setTime -= 24.0f;
				}
			}
			SetTime( setTime, (flags & NETSER_FORCESET) != 0 );
		}
	}
	else
	{
		// no static serialization (yet)
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::Tick()
{
//	if(!gEnv->bServer)
//		return;
	if (!m_bEditMode && !m_bPaused)
	{
		if (fabs(m_advancedInfo.fAnimSpeed) > 0.0001f)
		{
			// advance (forward or backward)
			float fTime = m_fTime + m_advancedInfo.fAnimSpeed*m_pTimer->GetFrameTime();

			// full cycle mode
			if(m_advancedInfo.fStartTime<=0.05f && m_advancedInfo.fEndTime>=23.5f)
			{
				if (fTime > m_advancedInfo.fEndTime)
					fTime = m_advancedInfo.fStartTime;
				if (fTime < m_advancedInfo.fStartTime)
					fTime = m_advancedInfo.fEndTime;
			}
      else if(fabs(m_advancedInfo.fStartTime-m_advancedInfo.fEndTime)<=0.05f)//full cycle mode
      {
        if(fTime>24.0f)        
          fTime -= 24.0f;
        else if(fTime<0.0f)
          fTime += 24.0f;
      }
			else
			{
				// clamp advancing time
				if (fTime > m_advancedInfo.fEndTime)
					fTime = m_advancedInfo.fEndTime;
				if (fTime < m_advancedInfo.fStartTime)
					fTime = m_advancedInfo.fStartTime;
			}

			SetTime( fTime );
		}
	}
}
