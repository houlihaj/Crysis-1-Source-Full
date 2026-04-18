////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   animcameranode.h
//  Version:     v1.00
//  Created:     16/8/2002 by Lennert.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __animcameranode_h__
#define __animcameranode_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "EntityNode.h"

/** Camera node controls camera entity.
*/
class CAnimCameraNode : public CAnimEntityNode
{
private:
	IMovieSystem *m_pMovie;
	// Field of view in DEGREES! To Display it nicely for user.
	float m_fFOV;

	bool m_shakeAEnabled;
	Vec3 m_amplitudeA;
	float m_amplitudeAMult;
	Vec3 m_frequencyA;
	float m_frequencyAMult;
	float m_noiseAAmpMult;
	float m_noiseAFreqMult;
	float m_timeOffsetA;
	bool m_shakeBEnabled;
	Vec3 m_amplitudeB;
	float m_amplitudeBMult;
	Vec3 m_frequencyB;
	float m_frequencyBMult;
	float m_noiseBAmpMult;
	float m_noiseBFreqMult;
	float m_timeOffsetB;
	
	bool m_resetPhaseA;
	bool m_resetPhaseB;
	Vec3 m_phaseA;
	Vec3 m_phaseANoise;
	Vec3 m_phaseB;
	Vec3 m_phaseBNoise;
	float m_prevTime;

  int m_lastFocusKey;
  EntityId m_focusEntityId;
public:
	CAnimCameraNode(IMovieSystem *sys );
	virtual ~CAnimCameraNode();
	virtual EAnimNodeType GetType() const { return ANODE_CAMERA; }
	virtual void Animate( SAnimContext &ec );
	virtual void CreateDefaultTracks();
	virtual void Reset();
	float GetFOV() { return m_fFOV; }
  void ApplyFocusKey( ISelectKey &key,SAnimContext &ec );

	//////////////////////////////////////////////////////////////////////////
	int GetParamCount() const;
	bool GetParamInfo( int nIndex, SParamInfo &info ) const;
	bool GetParamInfoFromId( int paramId, SParamInfo &info ) const;

	//////////////////////////////////////////////////////////////////////////
	bool SetParamValue( float time,AnimParamType param,float value );
	bool SetParamValue( float time,AnimParamType param,const Vec3 &value );
	bool SetParamValue( float time,AnimParamType param,const Vec4 &value );
	bool GetParamValue( float time,AnimParamType param,float &value );
	bool GetParamValue( float time,AnimParamType param,Vec3 &value );
	bool GetParamValue( float time,AnimParamType param,Vec4 &value );

	IAnimTrack* CreateTrack(int nParamType);
	void Serialize( XmlNodeRef &xmlNode,bool bLoading );
};

#endif // __animcameranode_h__