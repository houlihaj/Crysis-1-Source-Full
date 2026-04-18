////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FacialEffector.h
//  Version:     v1.00
//  Created:     7/10/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __FacialEffector_h__
#define __FacialEffector_h__
#pragma once

#include <IFacialAnimation.h>
#include <ISplines.h>
#include "../TCBSpline.h"

class CFacialEffCtrl;
class CFacialEffectorsLibrary;

#define EFFECTOR_MIN_WEIGHT_EPSILON 0.0001f

//////////////////////////////////////////////////////////////////////////
class CFacialEffector : public IFacialEffector, public _i_reference_target_t
{
public:
	CFacialEffector()
	{
		m_nIndexInState = -1;
		m_nFlags = 0;
		m_pLibrary = 0;
	}

	//////////////////////////////////////////////////////////////////////////
	// IFacialEffector.
	//////////////////////////////////////////////////////////////////////////
	virtual void SetName( const char *sNewName );
	virtual const char* GetName() { return m_name.c_str(); };

	virtual EFacialEffectorType GetType() { return EFE_TYPE_GROUP; };

	virtual void   SetFlags( uint32 nFlags ) { m_nFlags = nFlags; };
	virtual uint32 GetFlags() { return m_nFlags; };

	virtual int GetIndexInState() { return m_nIndexInState; };

	virtual void SetParamString( EFacialEffectorParam param,const char *str ) {};
	virtual const char* GetParamString( EFacialEffectorParam param ) { return ""; };
	virtual void  SetParamVec3( EFacialEffectorParam param,Vec3 vValue ) {};
	virtual Vec3  GetParamVec3( EFacialEffectorParam param ) { return Vec3(0,0,0); };
	virtual void  SetParamInt( EFacialEffectorParam param,int nValue ) {};
	virtual int   GetParamInt( EFacialEffectorParam param ) { return 0; };

	virtual int GetSubEffectorCount() { return (int)m_subEffectors.size(); };
	virtual IFacialEffector* GetSubEffector( int nIndex );
	virtual IFacialEffCtrl* GetSubEffCtrl( int nIndex );
	virtual IFacialEffCtrl* AddSubEffector( IFacialEffector *pEffector );
	virtual void RemoveSubEffector( IFacialEffector *pEffector );
	virtual void RemoveAllSubEffectors();
	//////////////////////////////////////////////////////////////////////////

	// Serialize extension of facial effector.
	virtual void Serialize( XmlNodeRef &node,bool bLoading );
	
	void SetIndexInState( int nIndex ) { m_nIndexInState = nIndex; };
	void SetNameString( const string &str ) { m_name = str; }
	const string& GetNameString() { return m_name; }

	void SetLibrary( CFacialEffectorsLibrary *pLib ) { m_pLibrary = pLib;	}

	// Fast access to sub controllers.
	CFacialEffCtrl* GetSubCtrl( int nIndex ) { return m_subEffectors[nIndex]; }

protected:
	friend class CFaceState; // For faster access.

	string m_name;
	uint32 m_nFlags;
	int m_nIndexInState;
	std::vector<_smart_ptr<CFacialEffCtrl> > m_subEffectors;
	CFacialEffectorsLibrary* m_pLibrary;
};

//////////////////////////////////////////////////////////////////////////
class CFacialEffectorExpression : public CFacialEffector
{
public:
	virtual EFacialEffectorType GetType() { return EFE_TYPE_EXPRESSION; };
};

//////////////////////////////////////////////////////////////////////////
class CFacialMorphTarget : public CFacialEffector
{
public:
	CFacialMorphTarget( uint32 nMorphTargetId ) { m_nMorphTargetId = nMorphTargetId; }
	virtual EFacialEffectorType GetType() { return EFE_TYPE_MORPH_TARGET; };

	void SetMorphTargetId(uint32 nTarget) { m_nMorphTargetId = nTarget; }
	uint32 GetMorphTargetId() const { return m_nMorphTargetId; }

private:
	uint32 m_nMorphTargetId;
};

//////////////////////////////////////////////////////////////////////////
class CFacialEffectorAttachment : public CFacialEffector
{
public:
	CFacialEffectorAttachment()
	{
		m_vAngles.Set(0,0,0);
		m_vOffset.Set(0,0,0);
	}

	//////////////////////////////////////////////////////////////////////////
	// IFacialEffector.
	//////////////////////////////////////////////////////////////////////////
	virtual EFacialEffectorType GetType() { return EFE_TYPE_ATTACHMENT; };
	virtual void SetParamString( EFacialEffectorParam param,const char *str );
	virtual const char* GetParamString( EFacialEffectorParam param );
	virtual void  SetParamVec3( EFacialEffectorParam param,Vec3 vValue );
	virtual Vec3  GetParamVec3( EFacialEffectorParam param );

	const Vec3& GetOffset() const
	{
		return m_vOffset;
	}

	void SetOffset(const Vec3& offset)
	{
		m_vOffset = offset;
	}

	const Vec3& GetAngles() const
	{
		return m_vAngles;
	}

	void SetAngles(const Vec3& angles)
	{
		m_vAngles = angles;
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void Serialize( XmlNodeRef &node,bool bLoading );

private:
	string m_sAttacment; // Name of the attachment or bone in character.
	Vec3 m_vOffset;
	Vec3 m_vAngles;
};

//////////////////////////////////////////////////////////////////////////
class CFacialEffectorBone : public CFacialEffectorAttachment
{
public:
	//////////////////////////////////////////////////////////////////////////
	// IFacialEffector.
	//////////////////////////////////////////////////////////////////////////
	virtual EFacialEffectorType GetType() { return EFE_TYPE_BONE; };
};


//////////////////////////////////////////////////////////////////////////
// Controller of the face effector.
//////////////////////////////////////////////////////////////////////////
class CFacialEffCtrl : public IFacialEffCtrl, public _i_reference_target_t, public spline::CBaseSplineInterpolator<float,spline::CatmullRomSpline<float> >
{
public:
	CFacialEffCtrl()
	{
		m_fWeight = 1.0f;
		m_fBalance = 0.0f;
		m_type = CTRL_LINEAR;
		m_nFlags = 0;
	}

	//////////////////////////////////////////////////////////////////////////
	// IFacialEffCtrl interface
	//////////////////////////////////////////////////////////////////////////
	virtual ControlType GetType() { return m_type; };
	virtual void SetType( ControlType t );
	virtual IFacialEffector* GetEffector() { return m_pEffector; };
	virtual void SetConstantWeight( float fWeight );
	virtual float GetConstantWeight() { return m_fWeight; }
	virtual float GetConstantBalance() { return m_fBalance; }
	virtual void SetConstantBalance( float fBalance );

	virtual int GetFlags() { return m_nFlags; }
	virtual void SetFlags( int nFlags ) { m_nFlags = nFlags; };
	virtual ISplineInterpolator* GetSpline() { return this; };
	virtual float Evaluate( float fInput );
	//////////////////////////////////////////////////////////////////////////

	
	//////////////////////////////////////////////////////////////////////////
	void GetMinMax( float &min,float &max )
	{
		switch (m_type)
		{
		case CTRL_LINEAR:
			min = MAX(-1.0f,m_fWeight);
			max = MIN(1.0f,m_fWeight);
			return;
		case CTRL_SPLINE:
			// get min/max of spline points.
			break;
		}
	}

	// Evaluate input.
	CFacialEffector* GetCEffector() const { return m_pEffector; };
	void SetCEffector( CFacialEffector* pEff ) { m_pEffector = pEff; };

	void Serialize( CFacialEffectorsLibrary *pLibrary,XmlNodeRef &node,bool bLoading );
	
	bool CheckFlag( int nFlag ) const { return (m_nFlags&nFlag) == nFlag; }


	//////////////////////////////////////////////////////////////////////////
	// ISplineInterpolator.
	//////////////////////////////////////////////////////////////////////////
	// Dimension of the spline from 0 to 3, number of parameters used in ValueType.
	virtual int GetNumDimensions() { return 1; };
	virtual ESplineType GetSplineType() { return ESPLINE_CATMULLROM; };
	virtual void SerializeSpline( XmlNodeRef &node,bool bLoading );

	virtual void Interpolate( float time,ValueType &val )
	{
		val[0] = Evaluate( time );
	}
	//////////////////////////////////////////////////////////////////////////

public:
	ControlType m_type;
	int m_nFlags;
	float m_fWeight; // Constant weight.
	float m_fBalance;
	_smart_ptr<CFacialEffector> m_pEffector;
};

#endif // __FacialEffector_h__
