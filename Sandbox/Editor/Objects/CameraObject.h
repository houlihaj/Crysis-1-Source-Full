////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   cameraobject.h
//  Version:     v1.00
//  Created:     29/7/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __cameraobject_h__
#define __cameraobject_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "Entity.h"

/*!
 *	CCameraObject is an object that represent Source or Target of camera.
 *
 */
class CCameraObject : public CEntity
{
public:
	DECLARE_DYNCREATE(CCameraObject)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
	void InitVariables();
	void Done();
	CString GetTypeDescription() const { return GetTypeName(); };
	void Display( DisplayContext &disp );

	//////////////////////////////////////////////////////////////////////////
	virtual void SetName( const CString &name );
	virtual void SetScale( const Vec3 &scale );

	void BeginEditParams( IEditor *ie,int flags );
	void EndEditParams( IEditor *ie );

	//! Called when object is being created.
	int MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );
	bool HitTest( HitContext &hc );
	bool HitTestRect( HitContext &hc );
	void Serialize( CObjectArchive &ar );

	void GetBoundBox( BBox &box );
	void GetLocalBounds( BBox &box );
	//////////////////////////////////////////////////////////////////////////

	//! Get Camera Field Of View angle and Camera Shake parameters.
	const float GetFOV() const {return mv_fov;}
	const bool GetShakeAEnabled() const {return mv_shakeAEnabled;}
	const Vec3& GetAmplitudeA() const {return mv_amplitudeA;}
	const float GetAmplitudeAMult() const {return mv_amplitudeAMult;}
	const Vec3& GetFrequencyA() const {return mv_frequencyA;}
	const float GetFrequencyAMult() const {return mv_frequencyAMult;}
	const float GetNoiseAAmpMult() const {return mv_noiseAAmpMult;}
	const float GetNoiseAFreqMult() const {return mv_noiseAFreqMult;}
	const float GetTimeOffsetA() const {return mv_timeOffsetA;}
	const bool GetShakeBEnabled() const {return mv_shakeBEnabled;}
	const Vec3& GetAmplitudeB() const {return mv_amplitudeB;}
	const float GetAmplitudeBMult() const {return mv_amplitudeBMult;}
	const Vec3& GetFrequencyB() const {return mv_frequencyB;}
	const float GetFrequencyBMult() const {return mv_frequencyBMult;}
	const float GetNoiseBAmpMult() const {return mv_noiseBAmpMult;}
	const float GetNoiseBFreqMult() const {return mv_noiseBFreqMult;}
	const float GetTimeOffsetB() const {return mv_timeOffsetB;}

protected:
	//! Dtor must be protected.
	CCameraObject();
	
	virtual void EnableAnimNode( bool bEnable );
	// overrided from IAnimNodeCallback
	void OnNodeAnimated( IAnimNode *pNode );
	virtual void DrawHighlight( DisplayContext &dc ) {};

	// return world position for the entity targeted by look at.
	Vec3 GetLookAtEntityPos() const;

	void OnFovChange( IVariable *var );
	void SetEntityCameraFov();

	void OnShakeAmpAChange( IVariable *var );
	void OnShakeAmpBChange( IVariable *var );
	void OnShakeFreqAChange( IVariable *var );
	void OnShakeFreqBChange( IVariable *var );
	void OnShakeMultChange( IVariable *var );
	void OnShakeNoiseChange( IVariable *var );
	void OnShakeWorkingChange( IVariable *var );

	// Arguments
	//   fAspectRatio - e.g. 4.0/3.0
	void GetConePoints( Vec3 q[4],float dist, const float fAspectRatio );
	void DrawCone( DisplayContext &dc,float dist,float fScale=1 );
	void CreateTarget();

	//////////////////////////////////////////////////////////////////////////
	//! Field of view and camera shake.
	CVariable<float> mv_fov;

	CVariableArray mv_shakeParamsArray;
	CVariable<bool> mv_shakeAEnabled;
	CVariable<Vec3> mv_amplitudeA;
	CVariable<float> mv_amplitudeAMult;
	CVariable<Vec3> mv_frequencyA;
	CVariable<float> mv_frequencyAMult;
	CVariable<float> mv_noiseAAmpMult;
	CVariable<float> mv_noiseAFreqMult;
	CVariable<float> mv_timeOffsetA;
	CVariable<bool> mv_shakeBEnabled;
	CVariable<Vec3> mv_amplitudeB;
	CVariable<float> mv_amplitudeBMult;
	CVariable<Vec3> mv_frequencyB;
	CVariable<float> mv_frequencyBMult;
	CVariable<float> mv_noiseBAmpMult;
	CVariable<float> mv_noiseBFreqMult;
	CVariable<float> mv_timeOffsetB;

	//////////////////////////////////////////////////////////////////////////
	// Mouse callback.
	int m_creationStep;
};

/*!
 *	CCameraObjectTarget is a target object for Camera.
 *
 */
class CCameraObjectTarget : public CEntity
{
public:
	DECLARE_DYNCREATE(CCameraObjectTarget)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
	void InitVariables();
	CString GetTypeDescription() const { return GetTypeName(); };
	void Display( DisplayContext &disp );
	bool HitTest( HitContext &hc );
	void GetBoundBox( BBox &box );
	void SetScale( const Vec3 &scale ) {};
	void SetAngles( const Ang3& scale ) {};
	void Serialize( CObjectArchive &ar );
	//////////////////////////////////////////////////////////////////////////

protected:
	//! Dtor must be protected.
	CCameraObjectTarget();

	virtual void DrawHighlight( DisplayContext &dc ) {};
};

/*!
 * Class Description of CameraObject.	
 */
class CCameraObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {23612EE3-B568-465d-9B31-0CA32FDE2340}
		static const GUID guid = { 0x23612ee3, 0xb568, 0x465d, { 0x9b, 0x31, 0xc, 0xa3, 0x2f, 0xde, 0x23, 0x40 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_ENTITY; };
	const char* ClassName() { return "Camera"; };
	const char* Category() { return "Camera"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CCameraObject); };
	int GameCreationOrder() { return 202; };
};

/*!
 * Class Description of CameraObjectTarget.
 */
class CCameraObjectTargetClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {1AC4CF4E-9614-4de8-B791-C0028D0010D2}
		static const GUID guid = { 0x1ac4cf4e, 0x9614, 0x4de8, { 0xb7, 0x91, 0xc0, 0x2, 0x8d, 0x0, 0x10, 0xd2 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_ENTITY; };
	const char* ClassName() { return "CameraTarget"; };
	const char* Category() { return ""; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CCameraObjectTarget); };
	int GameCreationOrder() { return 202; };
};

#endif // __cameraobject_h__