////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   animcameranode.cpp
//  Version:     v1.00
//  Created:     16/8/2002 by Lennert.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "AnimCameraNode.h"
#include "SelectTrack.h"
#include "PNoise3.h"

#include <ISystem.h>
#include <I3DEngine.h>
#include <IEntitySystem.h>
#include <ITimer.h>

enum {
  APARAM_FOCUS_ENTITY = APARAM_USER,
};

#define s_nodeParamsInitialized s_nodeParamsInitializedCam
#define s_nodeParams s_nodeParamsCam
#define AddSupportedParam AddSupportedParamCam

//////////////////////////////////////////////////////////////////////////
namespace
{
	bool s_nodeParamsInitialized = false;
	std::vector<IAnimNode::SParamInfo> s_nodeParams;

	void AddSupportedParam( const char *sName,int paramId,EAnimValue valueType )
	{
		IAnimNode::SParamInfo param;
		param.name = sName;
		param.paramId = paramId;
		param.valueType = valueType;
		s_nodeParams.push_back( param );
	}
};

CAnimCameraNode::CAnimCameraNode( IMovieSystem *sys )
: CAnimEntityNode(sys)
{
	m_dwSupportedTracks = PARAM_BIT(APARAM_POS)|PARAM_BIT(APARAM_ROT)|
												PARAM_BIT(APARAM_EVENT)|PARAM_BIT(APARAM_FOV);
	m_pMovie=sys;
	m_fFOV = 60.0f;
	m_shakeAEnabled = false;
	m_amplitudeA = Vec3(1.0f,1.0f,1.0f);
	m_amplitudeAMult = 0.0f;
	m_frequencyA = Vec3(1.0f,1.0f,1.0f);
	m_frequencyAMult = 0.0f;
	m_noiseAAmpMult = 0.0f;
	m_noiseAFreqMult = 0.0f;
	m_timeOffsetA = 0.0f;
	m_shakeBEnabled = false;
	m_amplitudeB = Vec3(1.0f,1.0f,1.0f);
	m_amplitudeBMult = 0.0f;
	m_frequencyB = Vec3(1.0f,1.0f,1.0f);
	m_frequencyBMult = 0.0f;
	m_noiseBAmpMult = 0.0f;
	m_noiseBFreqMult = 0.0f;
	m_timeOffsetB = 0.0f;
  m_lastFocusKey = -1;

	m_resetPhaseA = true;
	m_resetPhaseB = true;

	if (!s_nodeParamsInitialized)
	{
		s_nodeParamsInitialized = true;
		AddSupportedParam( "Position",APARAM_POS,AVALUE_VECTOR );
		AddSupportedParam( "Rotation",APARAM_ROT,AVALUE_QUAT );
		AddSupportedParam( "Fov",APARAM_FOV,AVALUE_FLOAT );
		AddSupportedParam( "Event",APARAM_EVENT,AVALUE_EVENT );
    AddSupportedParam( "FocusEntity",APARAM_FOCUS_ENTITY,AVALUE_SELECT );
		AddSupportedParam( "Shake",APARAM_SHAKEMULT,AVALUE_VECTOR4 );
	}
}

//////////////////////////////////////////////////////////////////////////
CAnimCameraNode::~CAnimCameraNode()
{
}

//////////////////////////////////////////////////////////////////////////
int CAnimCameraNode::GetParamCount() const
{
	return s_nodeParams.size();
}

//////////////////////////////////////////////////////////////////////////
bool CAnimCameraNode::GetParamInfo( int nIndex, SParamInfo &info ) const
{
	if (nIndex >= 0 && nIndex < (int)s_nodeParams.size())
	{
		info = s_nodeParams[nIndex];
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CAnimCameraNode::GetParamInfoFromId( int paramId, SParamInfo &info ) const
{
	for (unsigned int i = 0; i < s_nodeParams.size(); i++)
	{
		if (s_nodeParams[i].paramId == paramId)
		{
			info = s_nodeParams[i];
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CAnimCameraNode::CreateDefaultTracks()
{
	CreateTrack(APARAM_POS);
	CreateTrack(APARAM_ROT);
	CreateTrack(APARAM_FOV);
};

//////////////////////////////////////////////////////////////////////////
void CAnimCameraNode::Animate( SAnimContext &ec )
{
	CAnimEntityNode::Animate(ec);
	IAnimBlock *anim = GetAnimBlock();
	if (!anim)
		return;
	IAnimTrack *pFOVTrack = anim->GetTrack(APARAM_FOV);
	
	float fov = m_fFOV;
	
	if (pFOVTrack)
		pFOVTrack->GetValue(ec.time, fov);

	// is this camera active ? if so, set the current fov
	if (m_pMovie->GetCameraParams().cameraEntityId == m_EntityId)
	{
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId);
		if (pEntity)
		{
			pEntity->SetFlags(pEntity->GetFlags() | ENTITY_FLAG_TRIGGER_AREAS);
			//pEntity->AddFlags(ENTITY_FLAG_TRIGGER_AREAS);
		}

		SCameraParams CamParams = m_pMovie->GetCameraParams();
		CamParams.fFOV = DEG2RAD(fov);
		m_pMovie->SetCameraParams(CamParams);
	}
	else
	{
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId);
		if (pEntity)
		{
			pEntity->ClearFlags(ENTITY_FLAG_TRIGGER_AREAS);
		}

	}

  CSelectTrack *pFocusTrack = (CSelectTrack*)anim->GetTrack(APARAM_FOCUS_ENTITY);
  if (pFocusTrack)
  {
    ISelectKey key;
    int nkey = pFocusTrack->GetActiveKey(ec.time,&key);
    if (nkey != m_lastFocusKey)
    {
      if (!ec.bSingleFrame || key.time == ec.time) // If Single frame update key time must match current time.
        ApplyFocusKey( key,ec );
    }
    m_lastFocusKey = nkey;
  }

  if (m_focusEntityId)
  {
    IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_focusEntityId);
    if (pEntity)
    {
      Vec3 pos = pEntity->GetPos();
			assert("!GetI3DEngine()->SetCameraFocus() is not supported");
//      GetMovieSystem()->GetSystem()->GetI3DEngine()->SetCameraFocus( pos );
    }
  }

	Vec4 shakeMult = Vec4(m_amplitudeAMult,m_amplitudeBMult,m_frequencyAMult,m_frequencyBMult);

	IAnimTrack *pShakeMultTrack = anim->GetTrack(APARAM_SHAKEMULT);
	if (pShakeMultTrack)
		pShakeMultTrack->GetValue(ec.time,shakeMult);

	if (fov != m_fFOV || shakeMult != Vec4(m_amplitudeAMult,m_amplitudeBMult,m_frequencyAMult,m_frequencyBMult))
	{
		m_fFOV = fov;

		m_amplitudeAMult = shakeMult.x;
		m_amplitudeBMult = shakeMult.y;
		m_frequencyAMult = shakeMult.z;
		m_frequencyBMult = shakeMult.w;

		if (m_pOwner)
		{
			m_pOwner->OnNodeAnimated(this);
		}
	}

	IEntity *pEntity = GetEntity();

	if (pEntity != NULL && !ec.bSingleFrame && (m_shakeAEnabled || m_shakeBEnabled))
	{
		float time = gEnv->pTimer->GetFrameStartTime().GetSeconds();

		Quat rotation;
		rotation = pEntity->GetRotation();

		Ang3 angles = Ang3::GetAnglesXYZ(Matrix33(rotation))*180.0f/gf_PI;

		if (m_shakeAEnabled)
		{
			Ang3 rotationA;
			Ang3 rotationANoise;

			float noiseAAmpMult = m_amplitudeAMult*m_noiseAAmpMult;
			float noiseAFreqMult = m_frequencyAMult*m_noiseAFreqMult;

			if (m_resetPhaseA == true)
			{
				float timeA = time+m_timeOffsetA;

				m_phaseA = Vec3((timeA+15)*m_frequencyA.x*m_frequencyAMult,(timeA+55.1)*m_frequencyA.y*m_frequencyAMult,(timeA+101.2)*m_frequencyA.z*m_frequencyAMult);
				m_phaseANoise = Vec3((timeA+70)*m_frequencyA.x*noiseAFreqMult,(timeA+10)*m_frequencyA.y*noiseAFreqMult,(timeA+30.5)*m_frequencyA.z*noiseAFreqMult);

				m_resetPhaseA = false;
			}
			else
			{
				float timeA = time-m_prevTime;

				m_phaseA += timeA*m_frequencyA*m_frequencyAMult;
				m_phaseANoise += timeA*m_frequencyA*noiseAFreqMult;
			}

			rotationA.x = gEnv->pSystem->GetNoiseGen()->Noise1D(m_phaseA.x)*m_amplitudeA.x*m_amplitudeAMult;
			rotationANoise.x = gEnv->pSystem->GetNoiseGen()->Noise1D(m_phaseANoise.x)*m_amplitudeA.x*noiseAAmpMult;

			rotationA.y = gEnv->pSystem->GetNoiseGen()->Noise1D(m_phaseA.y)*m_amplitudeA.y*m_amplitudeAMult;
			rotationANoise.y = gEnv->pSystem->GetNoiseGen()->Noise1D(m_phaseANoise.y)*m_amplitudeA.y*noiseAAmpMult;

			rotationA.z = gEnv->pSystem->GetNoiseGen()->Noise1D(m_phaseA.z)*m_amplitudeA.z*m_amplitudeAMult;
			rotationANoise.z = gEnv->pSystem->GetNoiseGen()->Noise1D(m_phaseANoise.z)*m_amplitudeA.z*noiseAAmpMult;

			angles += rotationA+rotationANoise;
		}

		if (m_shakeBEnabled)
		{
			Ang3 rotationB;
			Ang3 rotationBNoise;

			float noiseBAmpMult = m_amplitudeBMult*m_noiseBAmpMult;
			float noiseBFreqMult = m_frequencyBMult*m_noiseBFreqMult;

			if (m_resetPhaseB == true)
			{
				float timeB = time+m_timeOffsetB;

				m_phaseB = Vec3((timeB+15)*m_frequencyB.x*m_frequencyBMult,(timeB+155.2)*m_frequencyB.y*m_frequencyBMult,(timeB+22.2)*m_frequencyB.z*m_frequencyBMult);
				m_phaseBNoise = Vec3((timeB+20)*m_frequencyB.x*noiseBFreqMult,(timeB+120)*m_frequencyB.y*noiseBFreqMult,(timeB+130.5)*m_frequencyB.z*noiseBFreqMult);

				m_resetPhaseB = false;
			}
			else
			{
				float timeB = time-m_prevTime;

				m_phaseB += timeB*m_frequencyB*m_frequencyBMult;
				m_phaseBNoise += timeB*m_frequencyB*noiseBFreqMult;
			}

			rotationB.x = gEnv->pSystem->GetNoiseGen()->Noise1D(m_phaseB.x)*m_amplitudeB.x*m_amplitudeBMult;
			rotationBNoise.x = gEnv->pSystem->GetNoiseGen()->Noise1D(m_phaseBNoise.x)*m_amplitudeB.x*noiseBAmpMult;

			rotationB.y = gEnv->pSystem->GetNoiseGen()->Noise1D(m_phaseB.y)*m_amplitudeB.y*m_amplitudeBMult;
			rotationBNoise.y = gEnv->pSystem->GetNoiseGen()->Noise1D(m_phaseBNoise.y)*m_amplitudeB.y*noiseBAmpMult;

			rotationB.z = gEnv->pSystem->GetNoiseGen()->Noise1D(m_phaseB.z)*m_amplitudeB.z*m_amplitudeBMult;
			rotationBNoise.z = gEnv->pSystem->GetNoiseGen()->Noise1D(m_phaseBNoise.z)*m_amplitudeB.z*noiseBAmpMult;

			angles += rotationB+rotationBNoise;
		}

		rotation.SetRotationXYZ(angles*gf_PI/180.0f);

		pEntity->SetRotation(rotation,ENTITY_XFORM_TRACKVIEW);

		m_prevTime = time;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimCameraNode::Reset()
{
	CAnimEntityNode::Reset();
  m_lastFocusKey = -1;
}

//////////////////////////////////////////////////////////////////////////
bool CAnimCameraNode::SetParamValue( float time,AnimParamType param,float value )
{
	if (param == APARAM_FOV)
	{
		// Set default value.
		m_fFOV = value;
	}
	return CAnimEntityNode::SetParamValue( time,param,value );
}

//////////////////////////////////////////////////////////////////////////
bool CAnimCameraNode::SetParamValue( float time,AnimParamType param,const Vec3 &value )
{
	if (param == APARAM_SHAKEAMPA)
	{
		m_amplitudeA = value;
	}
	else if (param == APARAM_SHAKEAMPB)
	{
		m_amplitudeB = value;
	}
	else if (param == APARAM_SHAKEFREQA)
	{
		m_frequencyA = value;
		m_resetPhaseA = true;
	}
	else if (param == APARAM_SHAKEFREQB)
	{
		m_frequencyB = value;
		m_resetPhaseB = true;
	}

	return CAnimEntityNode::SetParamValue( time,param,value );
}

//////////////////////////////////////////////////////////////////////////
bool CAnimCameraNode::SetParamValue( float time,AnimParamType param,const Vec4 &value )
{
	if (param == APARAM_SHAKEMULT)
	{
		m_amplitudeAMult = value.x;
		m_amplitudeBMult = value.y;
		m_frequencyAMult = value.z;
		m_frequencyBMult = value.w;
	}
	else if (param == APARAM_SHAKENOISE)
	{
		m_noiseAAmpMult = value.x;
		m_noiseBAmpMult = value.y;
		m_noiseAFreqMult = value.z;
		m_noiseBFreqMult = value.w;
	}
	else if (param == APARAM_SHAKEWORKING)
	{
		m_timeOffsetA = value.x;
		m_timeOffsetB = value.y;
		m_shakeAEnabled	= value.z == 1.0f ? true : false;
		m_shakeBEnabled	= value.w == 1.0f ? true : false;
		m_resetPhaseA = true;
		m_resetPhaseB = true;
	}

	return CAnimEntityNode::SetParamValue( time,param,value );
}

//////////////////////////////////////////////////////////////////////////
bool CAnimCameraNode::GetParamValue( float time,AnimParamType param,float &value )
{
	if (CAnimEntityNode::GetParamValue(time,param,value))
	{
		return true;
	}

	if (param == APARAM_FOV)
	{
	value = m_fFOV;
	return true;
}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CAnimCameraNode::GetParamValue( float time,AnimParamType param,Vec3 &value )
{
	if (CAnimEntityNode::GetParamValue(time,param,value))
	{
		return true;
	}

	if (param == APARAM_SHAKEAMPA)
	{
		value = m_amplitudeA;
		return true;
	}
	else if (param == APARAM_SHAKEAMPB)
	{
		value = m_amplitudeB;
		return true;
	}
	else if (param == APARAM_SHAKEFREQA)
	{
		value = m_frequencyA;
		return true;
	}
	else if (param == APARAM_SHAKEFREQB)
	{
		value = m_frequencyB;
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CAnimCameraNode::GetParamValue( float time,AnimParamType param,Vec4 &value )
{
	if (CAnimEntityNode::GetParamValue(time,param,value))
	{
		return true;
	}

	if (param == APARAM_SHAKEMULT)
	{
		value = Vec4(m_amplitudeAMult,m_amplitudeBMult,m_frequencyAMult,m_frequencyBMult);
		return true;
	}
	else if (param == APARAM_SHAKENOISE)
	{
		value = Vec4(m_noiseAAmpMult,m_noiseBAmpMult,m_noiseAFreqMult,m_noiseBFreqMult);
		return true;
	}
	else if (param == APARAM_SHAKEWORKING)
	{
		value = Vec4(m_timeOffsetA,m_timeOffsetB,m_shakeAEnabled ? 1.0f : 0.0f,m_shakeBEnabled ? 1.0f : 0.0f);
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
IAnimTrack* CAnimCameraNode::CreateTrack(int nParamType)
{
	IAnimTrack *pTrack = CAnimEntityNode::CreateTrack(nParamType);
	if (nParamType == APARAM_FOV)
	{
		if (pTrack)
			pTrack->SetValue(0,m_fFOV,true);
	}
	else if (nParamType == APARAM_SHAKEMULT)
	{
		if (pTrack)
			pTrack->SetValue(0,Vec4(m_amplitudeAMult,m_amplitudeBMult,m_frequencyAMult,m_frequencyBMult),true);
	}
	return pTrack;
}

//////////////////////////////////////////////////////////////////////////
void CAnimCameraNode::Serialize( XmlNodeRef &xmlNode,bool bLoading )
{
	CAnimEntityNode::Serialize( xmlNode,bLoading );
	if (bLoading)
	{
		xmlNode->getAttr( "FOV",m_fFOV );
		xmlNode->getAttr( "ShakeAEnabled",m_shakeAEnabled );
		xmlNode->getAttr( "AmplitudeA",m_amplitudeA );
		xmlNode->getAttr( "AmplitudeAMult",m_amplitudeAMult );
		xmlNode->getAttr( "FrequencyA",m_frequencyA );
		xmlNode->getAttr( "FrequencyAMult",m_frequencyAMult );
		xmlNode->getAttr( "NoiseAAmpMult",m_noiseAAmpMult );
		xmlNode->getAttr( "NoiseAFreqMult",m_noiseAFreqMult );
		xmlNode->getAttr( "TimeOffsetA",m_timeOffsetA );
		xmlNode->getAttr( "ShakeBEnabled",m_shakeBEnabled );
		xmlNode->getAttr( "AmplitudeB",m_amplitudeB );
		xmlNode->getAttr( "AmplitudeBMult",m_amplitudeBMult );
		xmlNode->getAttr( "FrequencyB",m_frequencyB );
		xmlNode->getAttr( "FrequencyBMult",m_frequencyBMult );
		xmlNode->getAttr( "NoiseBAmpMult",m_noiseBAmpMult );
		xmlNode->getAttr( "NoiseBFreqMult",m_noiseBFreqMult );
		xmlNode->getAttr( "TimeOffsetB",m_timeOffsetB );

		m_resetPhaseA = true;
		m_resetPhaseB = true;
	}
	else
	{
		xmlNode->setAttr( "FOV",m_fFOV );
		xmlNode->setAttr( "ShakeAEnabled",m_shakeAEnabled );
		xmlNode->setAttr( "AmplitudeA",m_amplitudeA );
		xmlNode->setAttr( "AmplitudeAMult",m_amplitudeAMult );
		xmlNode->setAttr( "FrequencyA",m_frequencyA );
		xmlNode->setAttr( "FrequencyAMult",m_frequencyAMult );
		xmlNode->setAttr( "NoiseAAmpMult",m_noiseAAmpMult );
		xmlNode->setAttr( "NoiseAFreqMult",m_noiseAFreqMult );
		xmlNode->setAttr( "TimeOffsetA",m_timeOffsetA );
		xmlNode->setAttr( "ShakeBEnabled",m_shakeBEnabled );
		xmlNode->setAttr( "AmplitudeB",m_amplitudeB );
		xmlNode->setAttr( "AmplitudeBMult",m_amplitudeBMult );
		xmlNode->setAttr( "FrequencyB",m_frequencyB );
		xmlNode->setAttr( "FrequencyBMult",m_frequencyBMult );
		xmlNode->setAttr( "NoiseBAmpMult",m_noiseBAmpMult );
		xmlNode->setAttr( "NoiseBFreqMult",m_noiseBFreqMult );
		xmlNode->setAttr( "TimeOffsetB",m_timeOffsetB );
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimCameraNode::ApplyFocusKey( ISelectKey &key,SAnimContext &ec )
{
  m_focusEntityId = 0;
  if (key.szSelection[0])
  {
    IEntity *pEntity = GetMovieSystem()->GetSystem()->GetIEntitySystem()->FindEntityByName( key.szSelection );
    if (pEntity)
    {
      m_focusEntityId = pEntity->GetId();
    }
  }
}

#undef s_nodeParamsInitialized
#undef s_nodeParams
#undef AddSupportedParam

