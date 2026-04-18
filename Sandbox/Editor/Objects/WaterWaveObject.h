////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   WaverWaveObject.h
//  Version:     v1.00
//  Created:     04/08/2006 by Tiago Sousa.
//  Compilers:   Visual C++ 6.0
//  Description: WaterWave object definition.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __WaterWaveObject_h__
#define __WaterWaveObject_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "RoadObject.h"

/*!
 *	CWaterWaveObject used to create shore waves
 *
 */

class CWaterWaveObject : public CRoadObject
{
public:
	DECLARE_DYNCREATE(CWaterWaveObject)

	CWaterWaveObject();

	virtual void Serialize( CObjectArchive& ar );
	virtual XmlNodeRef Export( const CString& levelPath, XmlNodeRef& xmlNode );
  
  //! CRoadObject override: Needed for snapping correctly to water plane - If there's better solution feel free to change
  virtual void SetPoint( int index,const Vec3 &pos );    
  //! CRoadObject override: Needed for snapping correctly to water plane - If there's better solution feel free to change
  virtual int MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );

protected:
  //! CRoadObject override: Needed for snapping correctly to water plane - If there's better solution feel free to change
  Vec3 MapViewToCP( CViewport *view, CPoint point );

	virtual bool Init( IEditor* ie, CBaseObject* prev, const CString& file );
	virtual void InitVariables();
	virtual void UpdateSectors();
		
	virtual void OnWaterWaveParamChange( IVariable* var );
	virtual void OnWaterWavePhysicsParamChange( IVariable* var );

	void Physicalize();
  bool IsValidSector( const CRoadSector &pSector);

  //////////////////////////////////////////////////////////////////////////
  // Overrides from CBaseObject.
  //////////////////////////////////////////////////////////////////////////

  virtual void Done();

protected:

  uint64 m_nID;

  CVariable<float> mv_surfaceUScale;
	CVariable<float> mv_surfaceVScale;
  
  CVariable<float> mv_speed;
  CVariable<float> mv_speedVar;

  CVariable<float> mv_lifetime;
  CVariable<float> mv_lifetimeVar;

  CVariable<float> mv_height;
  CVariable<float> mv_heightVar;
  
  CVariable<float> mv_posVar;   

  IRenderNode *m_pRenderNode;
};


/*!
 * Class Description of WaterWaveObject.	
 */
class CWaterWaveObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{   
    static const GUID guid = { 0x41dbf7b0, 0x4ef, 0x4e7c, { 0x9a, 0x55, 0x41, 0x4b, 0xa7, 0x7b, 0xd2, 0xa7 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_ROAD; };
	const char* ClassName() { return "Water wave"; };
	const char* Category() { return "Misc"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CWaterWaveObject); };
	int GameCreationOrder() { return 50; };
};

#endif