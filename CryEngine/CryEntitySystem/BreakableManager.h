////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   BreakableManager.h
//  Version:     v1.00
//  Created:     7/6/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __BreakableManager_h__
#define __BreakableManager_h__
#pragma once

class CEntitySystem;
class CEntity;

//////////////////////////////////////////////////////////////////////////
//
// BreakableManager manager handles all the code for breaking/destroying entity geometry.
//
//////////////////////////////////////////////////////////////////////////
class CBreakableManager : public IBreakableManager
{
public:
	CBreakableManager( CEntitySystem *pEntitySystem );

	// actual breaking function.
	void BreakIntoPieces( GeomRef &geoOrig, const Matrix34 &srcObjTM, 
												IStatObj *pPiecesObj, const Matrix34 &piecesObjTM, 
												BreakageParams const& Breakage,int nMatLayers );

	//////////////////////////////////////////////////////////////////////////
	// IBreakableManager implementation
	//////////////////////////////////////////////////////////////////////////
	virtual void BreakIntoPieces( IEntity *pEntity, int nOrigSlot, int nPiecesSlot, BreakageParams const& Breakage );

	// Check if this stat object can shatter.
	// Check if its materials support shattering.
	virtual bool CanShatter( IStatObj *pStatObj );
	virtual bool CanShatterEntity( IEntity *pEntity,int nSlot=-1 );

	// Attach the effect & params specified by material of object in slot.
	virtual void AttachSurfaceEffect( IEntity* pEntity, int nSlot, const char* sType, SpawnParams const& paramsIn );
	virtual void CreateSurfaceEffect( IStatObj *pStatObj,const Matrix34 &tm,const char* sType );
	//////////////////////////////////////////////////////////////////////////

	bool CanShatterRenderMesh( IRenderMesh *pMesh,IMaterial *pMtl );

	ISurfaceType* GetFirstSurfaceType( IStatObj *pStatObj );
	ISurfaceType* GetFirstSurfaceType( ICharacterInstance *pCharacter );

	void HandlePhysicsCreateEntityPartEvent( const EventPhysCreateEntityPart *pCreateEvent );
	void HandlePhysicsRemoveSubPartsEvent( const EventPhysRemoveEntityParts *pRemoveEvent );
	int  HandlePhysics_UpdateMeshEvent( const EventPhysUpdateMesh *pEvent );

	void FakePhysicsEvent( EventPhys * pEvent );
	
	// Freeze/unfreeze render node.
	void FreezeRenderNode( IRenderNode *pRenderNode,bool bEnable );

	struct SCreateParams
	{
		int nSlotIndex;
		Matrix34 slotTM;
		Matrix34 worldTM;
		float fScale;
		IMaterial *pCustomMtl;
		int nMatLayers;
		int nEntityFlagsAdd;
		int nEntitySlotFlagsAdd;
		int nRenderNodeFlags;
		IRenderNode *pSrcStaticRenderNode;
		const char *pName;
		SCreateParams() : fScale(1.0f),pCustomMtl(0),nSlotIndex(0),nRenderNodeFlags(0),pName(0),
			nMatLayers(0),nEntityFlagsAdd(0),nEntitySlotFlagsAdd(0),pSrcStaticRenderNode(0) { slotTM.SetIdentity(); worldTM.SetIdentity(); };
	};

	void CreateObjectAsParticles( IStatObj *pStatObj,IPhysicalEntity *pPhysEnt,SCreateParams &createParams );
	CEntity* CreateObjectAsEntity( IStatObj *pStatObj,IPhysicalEntity *pPhysEnt,SCreateParams &createParams, bool bCreateSubstProxy=false );
	bool CheckForPieces(IStatObj *pStatObjSrc, IStatObj::SSubObject *pSubObj, const Matrix34 &worldTM, int nMatLayers, IPhysicalEntity *pPhysEnt);

private:
	void CreateObjectCommon( IStatObj *pStatObj,IPhysicalEntity *pPhysEnt,SCreateParams &createParams );

	// Remove Parts
	bool RemoveStatObjParts( IStatObj* &pStatObj );

private:
	CEntitySystem *m_pEntitySystem;
};

#endif //__BreakableManager_h__
