////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   BreakableManager.cpp
//  Version:     v1.00
//  Created:     7/6/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <ICryAnimation.h>
#include "BreakableManager.h"
#include "Entity.h"
#include "EntitySystem.h"
#include "EntityObject.h"
#include "GeomQuery.h"

#include <I3DEngine.h>
#include <ParticleParams.h>
#include <IMaterialEffects.h>

//////////////////////////////////////////////////////////////////////////
CBreakableManager::CBreakableManager( CEntitySystem *pEntitySystem )
{
	m_pEntitySystem = pEntitySystem;
}

//////////////////////////////////////////////////////////////////////////
float ExtractFloatKeyFromString( const char* key, const char* props )
{
	if (const char* ptr = strstr(props,key)) 
	{
		ptr += strlen(key);
		if (*ptr++ == '=')
			return (float)atof(ptr);
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
string ExtractStringKeyFromString( const  char *key, const string &props )
{
	const char *ptr=strstr(props, key);
	if (ptr)
	{
		ptr+=strlen(key);
		while(*ptr && *ptr!='\n' && !isalnum(*ptr)) ++ptr;
		const char *start=ptr;
		while(*ptr && *ptr!='\n') ++ptr;
		const char *end=ptr;
		return string(start, end);
	}
	return "";
}

//////////////////////////////////////////////////////////////////////////
// Get the physical surface type used on the render mesh.
// Stat object can use multiple surface types, so this one will select 1st that matches.
ISurfaceType* GetSurfaceType( IRenderMesh* pMesh )
{
	if (!pMesh)
		return NULL;
	
	IMaterial *pMtl = pMesh->GetMaterial();
	if (!pMtl)
		return 0;

	PodArray<CRenderChunk> &meshChunks = *pMesh->GetChunks();

	int numChunks = meshChunks.size();
	for (int nChunk = 0; nChunk < numChunks; nChunk++) 
	{
		if ((meshChunks[nChunk].m_nMatFlags & MTL_FLAG_NODRAW))
			continue;

		IMaterial *pSubMtl = pMtl->GetSafeSubMtl(meshChunks[nChunk].m_nMatID);
		if (pSubMtl)
		{
			ISurfaceType *pSrfType = pSubMtl->GetSurfaceType();
			if (pSrfType && pSrfType->GetId() != 0) // If not default surface type
				return pSubMtl->GetSurfaceType();
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// Get the physical surface type used on the static object.
// Stat object can use multiple surface types, so this one will select 1st that matches.
ISurfaceType* GetSurfaceType( IStatObj* pStatObj )
{
	if (!pStatObj)
		return NULL;

	if (!pStatObj->GetRenderMesh())
		return 0;

	ISurfaceType *pSurface = GetSurfaceType( pStatObj->GetRenderMesh() );
	
	if (!pSurface)
	{
		phys_geometry* pGeom = pStatObj->GetPhysGeom();
		if (pGeom)
		{
			if (pGeom && pGeom->surface_idx < pGeom->nMats)
			{
				ISurfaceTypeManager *pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
				int nMat = pGeom->pMatMapping[pGeom->surface_idx];
				ISurfaceType *pSrfType = pSurfaceTypeManager->GetSurfaceType(nMat);
				if (pSrfType && pSrfType->GetId() != 0) // If not default surface type
				{
					pSurface = pSrfType;
				}
			}
		}
	}
	if (!pSurface)
		pSurface = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetSurfaceType(0);

	return pSurface;


			/*

	ISurfaceType* pSurface = NULL;

	phys_geometry* pGeom = pStatObj->GetPhysGeom();
	if (pGeom && pGeom->surface_idx < pGeom->nMats)
	{
		int nMat = pGeom->pMatMapping[pGeom->surface_idx];
		ISurfaceTypeManager *pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
		pSurface = pSurfaceTypeManager->GetSurfaceType(nMat);
	}

	if (!pSurface && pStatObj->GetMaterial())
		pSurface = pStatObj->GetMaterial()->GetSurfaceType();

	return pSurface;
	*/
}


//////////////////////////////////////////////////////////////////////////
// Get the physical surface specified on a character.

ISurfaceType* GetSurfaceType( ICharacterInstance* pChar)
{
	if (!pChar)
		return NULL;

	ISurfaceType* pSurface = NULL;
	phys_geometry*pGeom=0;

	ISurfaceTypeManager *pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
	IPhysicalEntity *pPhysics=pChar->GetISkeletonPose()->GetCharacterPhysics();
	if (pPhysics)
	{
		pe_params_part part;
		part.ipart=0;
		if (!pPhysics->GetParams(&part))
			return NULL;
		pGeom=part.pPhysGeom;

		if (pGeom && pGeom->surface_idx < part.nMats)
		{
			int nMat = part.pMatMapping[pGeom->surface_idx];
			pSurface = pSurfaceTypeManager->GetSurfaceType(nMat);
		}
	}

	if (!pSurface)
	{
		ICharacterModel *pModel = pChar->GetICharacterModel();
		if (pModel)
		{
			int numLods = pModel->GetNumLods();
			for (int nLod = 0; nLod < numLods; nLod++)
			{
				IRenderMesh *pRenderMesh = pModel->GetRenderMesh(nLod);
				if (pRenderMesh)
				{
					pSurface = GetSurfaceType(pRenderMesh);
					break;
				}
			}
		}
	}
	if (!pSurface)
		pSurface = pSurfaceTypeManager->GetSurfaceType(0); // Assign default surface type.

	return pSurface;
}

ISurfaceType* GetSurfaceType( GeomRef const &geo )
{
	return geo.m_pStatObj ? GetSurfaceType(geo.m_pStatObj)
			 : geo.m_pChar ? GetSurfaceType(geo.m_pChar)
			 : 0;
}

IParticleEffect* GetSurfaceEffect( ISurfaceType* pSurface, const char* sType, SpawnParams& params )
{
	// Extract property group from surface's script.
	if (!pSurface)
		return NULL;

	ISurfaceType::SBreakageParticles *pBreakParticles = pSurface->GetBreakageParticles( sType );
	if (!pBreakParticles)
	{
		return NULL;
	}

	const char *sEffect = pBreakParticles->particle_effect.c_str();

	if (sEffect && sEffect[0])
	{
		if (IParticleEffect* pEffect = gEnv->p3DEngine->FindParticleEffect(sEffect, "Surface Effect", pSurface->GetName() ))
		{
			// Get effect spawn params, override from script.
			params.fSizeScale = pBreakParticles->scale;
			params.fCountScale = pBreakParticles->count_scale;
			params.bCountPerUnit = pBreakParticles->count_per_unit != 0;

			return pEffect;
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
ISurfaceType* CBreakableManager::GetFirstSurfaceType( IStatObj *pStatObj )
{
	return GetSurfaceType(pStatObj);
}

//////////////////////////////////////////////////////////////////////////
ISurfaceType* CBreakableManager::GetFirstSurfaceType( ICharacterInstance *pCharacter )
{
	return GetSurfaceType(pCharacter);
}

//////////////////////////////////////////////////////////////////////////
bool CBreakableManager::CanShatterRenderMesh( IRenderMesh *pRenderMesh,IMaterial *pMtl )
{
	// Check If all non-default surface types can shatter.
	if (!pMtl)
		pMtl = pRenderMesh->GetMaterial();

	if (!pMtl)
		return false;

	PodArray<CRenderChunk> &meshChunks = *pRenderMesh->GetChunks();

	bool bCanShatter = true;
	int numChunks = meshChunks.size();
	for (int nChunk = 0; nChunk < numChunks; nChunk++) 
	{
		if ((meshChunks[nChunk].m_nMatFlags & MTL_FLAG_NODRAW))
			continue;

		IMaterial *pSubMtl = pMtl->GetSafeSubMtl(meshChunks[nChunk].m_nMatID);
		if (pSubMtl)
		{
			ISurfaceType *pSrfType = pSubMtl->GetSurfaceType();
			if (pSrfType && pSrfType->GetId() != 0) // If not default surface type
			{
				if (pSrfType->GetFlags() & SURFACE_TYPE_CAN_SHATTER)
				{
					bCanShatter = true;
					continue;
				}
				return false; // If any material doesn`t support shatter, we cannot shatter it.
			}
		}
	}
	return bCanShatter;
}

//////////////////////////////////////////////////////////////////////////
bool CBreakableManager::CanShatter( IStatObj *pStatObj )
{
	if (!pStatObj)
		return false;
	IRenderMesh *pRenderMesh = pStatObj->GetRenderMesh();
	if (!pRenderMesh)
		return false;

	// Check If all non-default surface types can shatter.

	IMaterial *pMtl = pStatObj->GetMaterial();
	if (!pMtl)
		return false;

	bool bCanShatter = CanShatterRenderMesh(pRenderMesh,pMtl);
	
	if (!bCanShatter)
	{
		phys_geometry* pGeom = pStatObj->GetPhysGeom();
		if (pGeom)
		{
			if (pGeom && pGeom->surface_idx < pGeom->nMats)
			{
				ISurfaceTypeManager *pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
				int nMat = pGeom->pMatMapping[pGeom->surface_idx];
				ISurfaceType *pSrfType = pSurfaceTypeManager->GetSurfaceType(nMat);
				if (pSrfType && pSrfType->GetId() != 0) // If not default surface type
				{
					if (pSrfType->GetFlags() & SURFACE_TYPE_CAN_SHATTER)
					{
						bCanShatter = true;
					}
				}
			}
		}
	}
	return bCanShatter; // Can shatter.
}

//////////////////////////////////////////////////////////////////////////
bool CBreakableManager::CanShatterEntity( IEntity *pEntity,int nSlot )
{
	if (nSlot < 0)
		nSlot = 0;
	IStatObj *pStatObj = pEntity->GetStatObj(0|ENTITY_SLOT_ACTUAL);
	if (pStatObj)
		return CanShatter(pStatObj);
	else
	{
		ICharacterInstance *pCharacter = pEntity->GetCharacter(0|ENTITY_SLOT_ACTUAL);
		if (pCharacter)
		{
			ICharacterModel *pModel = pCharacter->GetICharacterModel();
			if (pModel)
			{
				int numLods = pModel->GetNumLods();
				for (int nLod = 0; nLod < numLods; nLod++)
				{
					IRenderMesh *pRenderMesh = pModel->GetRenderMesh(nLod);
					if (pRenderMesh)
					{
						// Only test first found mesh.
						if (CanShatterRenderMesh(pRenderMesh,pCharacter->GetMaterial()))
							return true;
						return false;
					}
				}
			}
		}
		return false;
	}
}

namespace
{
	void GetPhysicsStatus(IPhysicalEntity* pPhysEnt, float& fMass, float& fVolume, float& fDensity, Vec3& vCM, Vec3& vLinVel, Vec3& vAngVel)
	{
		fMass = 0.f;
		fVolume = 0.f;
		fDensity = 0.0f;
		vCM = Vec3(ZERO);
		vLinVel = Vec3(ZERO);
		vAngVel = Vec3(ZERO);

		pe_status_nparts statusTmp;
		int nparts = pPhysEnt->GetStatus(&statusTmp);
		for (int p=0; p<nparts; p++)
		{
			pe_params_part pp;
			pp.ipart = p;

			if (pPhysEnt->GetParams(&pp) && pp.density>0.0f)
			{
				fMass += pp.mass;
				fVolume += pp.mass / pp.density;
			}
		}

		if (fVolume > 0.f)
			fDensity = fMass / fVolume;

		pe_status_dynamics pd;
		if (pPhysEnt->GetStatus(&pd))
		{
			vCM = pd.centerOfMass;
			vLinVel = pd.v;
			vAngVel = pd.w;
		}
	}
};


//////////////////////////////////////////////////////////////////////////
void CBreakableManager::BreakIntoPieces( GeomRef &geoOrig, const Matrix34 &mxSrcTM,
																				IStatObj *pPiecesObj, const Matrix34 &mxPiecesTM, 
																				BreakageParams const& Breakage, int nMatLayers )
{
	ENTITY_PROFILER;

	struct MFXExec
	{
		MFXExec(GeomRef &geoOrig, const Matrix34 &mxSrcTM, const BreakageParams& Breakage, const int& nMatLayers) :
			geoOrig(geoOrig),
			mxSrcTM(mxSrcTM),
			Breakage(Breakage),
			nMatLayers(nMatLayers),
			vLinVel(ZERO),
			fTotalMass(0.0f)
		{
		}
		const GeomRef& geoOrig;
		const Matrix34& mxSrcTM;
		const BreakageParams& Breakage;
		const int& nMatLayers;
		Vec3 vLinVel;
		float fTotalMass;

		~MFXExec()
		{
			// Spawn any destroy effect.
			if (Breakage.bMaterialEffects)
			{
				SpawnParams paramsDestroy;
				paramsDestroy.eAttachForm = GeomForm_Volume;
				paramsDestroy.eAttachType = GeomType_Render;

				const char *sType = SURFACE_BREAKAGE_TYPE("destroy");
				if (Breakage.type == BREAKAGE_TYPE_FREEZE_SHATTER)
				{
					paramsDestroy.eAttachForm = GeomForm_Surface;
					sType = SURFACE_BREAKAGE_TYPE("freeze_shatter");
				}
				ISurfaceType* pSurfaceType = GetSurfaceType(geoOrig);
				IParticleEffect* pEffect = GetSurfaceEffect( pSurfaceType, sType, paramsDestroy );
				if (pEffect)
				{
					IParticleEmitter* pEmitter = pEffect->Spawn(true, mxSrcTM);
					if (pEmitter)
					{
						pEmitter->SetMaterialLayers(nMatLayers);
						pEmitter->SetSpawnParams(paramsDestroy, geoOrig);
					}
				}

				if (gEnv->pMaterialEffects && pSurfaceType && fTotalMass > 0.0f) // fTotalMass check to avoid calling
				{
					SMFXBreakageParams mfxBreakageParams;
					mfxBreakageParams.SetMatrix(mxSrcTM);
					mfxBreakageParams.SetHitPos(Breakage.vHitPoint);
					mfxBreakageParams.SetHitImpulse(Breakage.vHitImpulse);
					mfxBreakageParams.SetExplosionImpulse(Breakage.fExplodeImpulse);
					mfxBreakageParams.SetVelocity(vLinVel);
					mfxBreakageParams.SetMass(fTotalMass);
					gEnv->pMaterialEffects->PlayBreakageEffect(pSurfaceType, sType, mfxBreakageParams);
				}
			}
		}
	};

	MFXExec exec(geoOrig, mxSrcTM, Breakage, nMatLayers);

	if (!pPiecesObj)
		return;

	int nSubObjs = pPiecesObj->GetSubObjectCount();
	if (pPiecesObj == geoOrig.m_pStatObj && nSubObjs == 0)
	{
		// Do not spawn itself as pieces.
		return;
	}

	IEntityClass* pClass = m_pEntitySystem->GetClassRegistry()->FindClass("Default");
	I3DEngine* p3DEngine = gEnv->p3DEngine;
	IPhysicalWorld* pPhysicalWorld = gEnv->pPhysicalWorld;
	PhysicsVars *pVars = pPhysicalWorld->GetPhysVars();

	//////////////////////////////////////////////////////////////////////////
	Vec3 vCM = mxPiecesTM.GetTranslation();
	Vec3 vLinVel(0), vAngVel(0);
	float fTotalMass = 0.0f;

	float fDefaultDensity = 0.f;
	if (geoOrig.m_pPhysEnt)
	{
		float fMass = 0.f, fVolume = 0.f;

		pe_status_nparts statusTmp;
		int nparts = geoOrig.m_pPhysEnt->GetStatus(&statusTmp);
		for (int p=0; p<nparts; p++)
		{
			pe_params_part pp;
			pp.ipart = p;

			if (geoOrig.m_pPhysEnt->GetParams(&pp) && pp.density>0.0f)
			{
				fMass += pp.mass;
				fVolume += pp.mass / pp.density;
			}
		}

		fTotalMass = fMass;

		if (fVolume > 0.f)
			fDefaultDensity = fMass / fVolume;

		pe_status_dynamics pd;
		if (geoOrig.m_pPhysEnt->GetStatus(&pd))
		{
			vCM = pd.centerOfMass;
			vLinVel = pd.v;
			vAngVel = pd.w;
		}
	}

	if (fDefaultDensity <= 0.f && geoOrig.m_pStatObj)
	{
		// Compute default density from main piece.
		float fMass,fDensity;
		geoOrig.m_pStatObj->GetPhysicalProperties( fMass,fDensity );
		if (fDensity > 0)
			fDefaultDensity = fDensity*1000.f;
		else
		{
			if (fMass > 0 && geoOrig.m_pStatObj->GetPhysGeom())
			{
				float V = geoOrig.m_pStatObj->GetPhysGeom()->V;
				if (V != 0.f)
					fDefaultDensity = fMass / V;
			}
		}
		fTotalMass = fMass;
	}

	// set parameters to delayed Material Effect execution
	exec.fTotalMass = fTotalMass;
	exec.vLinVel = vLinVel;

	if (fDefaultDensity <= 0.f)
		fDefaultDensity = 1000.f;

	IParticleEmitter* pEmitter = 0;
	QuatTS entityQuatTS(mxPiecesTM);

	const char* sSourceGeometryProperties = "";
	if (geoOrig.m_pStatObj)
		sSourceGeometryProperties = geoOrig.m_pStatObj->GetProperties();

	//////////////////////////////////////////////////////////////////////////
	for (int i = 0; i < max(nSubObjs,1); i++)
	{
		IStatObj* pSubObj;
		Matrix34 mxPiece = mxPiecesTM;
		const char* sProperties = "";

		if (nSubObjs > 0)
		{
			IStatObj::SSubObject *pSubObjInfo = pPiecesObj->GetSubObject(i);
			pSubObj = pSubObjInfo->pStatObj;
			if (!pSubObj || pSubObj == geoOrig.m_pStatObj)
				continue;
			if (pSubObjInfo->nType != STATIC_SUB_OBJECT_MESH && pSubObjInfo->nType != STATIC_SUB_OBJECT_HELPER_MESH)
				continue;

			if (Breakage.bOnlyHelperPieces)
			{
				// If we only spawn pieces.
				if (strstr(sSourceGeometryProperties,pSubObjInfo->name) == 0)
					continue;
			}

			if (!strcmpi(pSubObjInfo->name,"Main") || !strcmpi(pSubObjInfo->name,"Remain"))
				continue;
			mxPiece = mxPiecesTM * pSubObjInfo->tm;
			sProperties = pSubObjInfo->properties.c_str();
		}
		else
		{
			pSubObj = pPiecesObj;
			sProperties = pSubObj->GetProperties();
		}

		phys_geometry *pPhysGeom = pSubObj->GetPhysGeom(0);
		if (pPhysGeom)
		{
			// Extract properties.
			float fMass,fDensity = fDefaultDensity;;
			pSubObj->GetPhysicalProperties(fMass,fDensity);
			if (fMass > 0)
				fDensity = 0;
			else if (fDensity < 0)
				fDensity = fDefaultDensity;

			bool bEntity = Breakage.bForceEntity || strstr( sProperties, "entity" ) != 0;

			// Extract bone affinity.
			if (geoOrig.m_pChar)
			{
				string bone = ExtractStringKeyFromString( "bone", sProperties );
				int boneId = geoOrig.m_pChar->GetISkeletonPose()->GetJointIDByName(bone.c_str());
				if (boneId >= 0)
					mxPiece = mxPiecesTM * Matrix34(geoOrig.m_pChar->GetISkeletonPose()->GetAbsJointByID(boneId));
			}

			// Extract generic count.
			int nCount = (int)ExtractFloatKeyFromString( "count", sProperties );
			if (nCount <= 0)
				nCount = (int)ExtractFloatKeyFromString( "generic", sProperties );
			float fSizeVar = ExtractFloatKeyFromString( "sizevar", sProperties );
			string sRotAxes = ExtractStringKeyFromString( "rotaxes", sProperties );
			nCount = max(nCount, Breakage.nGenericCount);
			for (int n = 0; n < max(nCount,1); n++)
			{
				// Compute initial position.
				if (nCount)
				{
					// Position randomly in parent.
					GeomQuery geo;
					RandomPos ran;
					if (geoOrig.m_pStatObj)
						geoOrig.m_pStatObj->GetRandomPos(ran, geo, GeomForm_Volume);
					else if (geoOrig.m_pChar)
						geoOrig.m_pChar->GetRandomPos(ran, geo, GeomForm_Volume);
					ran.vPos = entityQuatTS * ran.vPos;
					ran.vNorm = entityQuatTS.q * ran.vNorm;
					mxPiece.SetTranslation(ran.vPos);

					// Random rotation and size.
					if (!sRotAxes.empty())
					{
						Ang3 angRot;
						angRot.x = strchr(sRotAxes,'x') ? BiRandom(DEG2RAD(180)) : 0.f;
						angRot.y = strchr(sRotAxes,'y') ? BiRandom(DEG2RAD(180)) : 0.f;
						angRot.z = strchr(sRotAxes,'z') ? BiRandom(DEG2RAD(180)) : 0.f;
						mxPiece = mxPiece * Matrix33::CreateRotationXYZ(angRot);
					}

					if (fSizeVar != 0.f)
					{
						float fScale = 1.f + BiRandom(fSizeVar);
						if (fScale <= 0.01f)
							fScale = 0.01f;
						mxPiece = mxPiece * Matrix33::CreateScale( Vec3(fScale,fScale,fScale) );
					}
				}

				IPhysicalEntity *pPhysEnt = 0;

				if (!bEntity)
				{
					if (!pEmitter)
					{
						ParticleParams params;
						params.fCount = 0;
						params.ePhysicsType = ParticlePhysics_RigidBody;
						params.fParticleLifeTime.SetBase( Breakage.fParticleLifeTime * CVar::pDebrisLifetimeScale->GetFVal() );
						params.fParticleLifeTime.SetRandomVar(0.25f);
						params.bRemainWhileVisible = true;
						pEmitter = p3DEngine->CreateParticleEmitter(true, mxSrcTM, params);
						if (!pEmitter)
							continue;
						pEmitter->SetMaterialLayers(nMatLayers);
					}

					// Spawn as particle.
					pe_params_pos ppos;
					QuatTS qts(mxPiece);
					ppos.pos = qts.t;
					ppos.q = qts.q;
					ppos.scale = qts.s;
					pPhysEnt = pPhysicalWorld->CreatePhysicalEntity( PE_RIGID,&ppos );
					if (pPhysEnt)
					{
						pe_status_pos spos;
						pPhysEnt->GetStatus(&spos);

						pe_geomparams pgeom;
						pgeom.mass = fMass;
						pgeom.density = fDensity;
						pgeom.flags = geom_collides|geom_floats;
						if (spos.scale > 0.f)
							pgeom.scale = ppos.scale / spos.scale;
						pPhysEnt->AddGeometry( pPhysGeom, &pgeom );

						pEmitter->EmitParticle( pSubObj, pPhysEnt );
					}
				}
				else if (pClass)
				{
					// Spawn as entity.
					SEntitySpawnParams params;
					params.nFlags = ENTITY_FLAG_CLIENT_ONLY|ENTITY_FLAG_CASTSHADOW|ENTITY_FLAG_SPAWNED|ENTITY_FLAG_MODIFIED_BY_PHYSICS;
					params.pClass = pClass;
					CEntity *pNewEntity = (CEntity*)m_pEntitySystem->SpawnEntity( params );
					if (!pNewEntity)
						continue;

					IEntityRenderProxy *pRenderProxy = (IEntityRenderProxy*)pNewEntity->CreateProxy(ENTITY_PROXY_RENDER);
					if (nMatLayers)
						pRenderProxy->SetMaterialLayersMask( nMatLayers );
					pNewEntity->SetStatObj( pSubObj,0,false );
					pNewEntity->SetWorldTM( mxPiece );

					SEntityPhysicalizeParams physparams;
					physparams.type = PE_RIGID;
					physparams.mass = fMass;
					physparams.density = fDensity;
					physparams.nSlot = 0;
					pNewEntity->Physicalize( physparams );

					pPhysEnt = pNewEntity->GetPhysics();
					if (pPhysEnt)
					{
						pe_action_awake aa;
						aa.bAwake = 1;
						pPhysEnt->Action(&aa);
					}

					SpawnParams paramsBreak;
					paramsBreak.bCountPerUnit = true;
					paramsBreak.eAttachForm = GeomForm_Edges;
					paramsBreak.eAttachType = GeomType_Physics;

					if (Breakage.type != BREAKAGE_TYPE_FREEZE_SHATTER)
					{
						// Find breakage effect.
						if (Breakage.bMaterialEffects)
						{
							ISurfaceType* pSurfaceType = GetSurfaceType(pSubObj);
							if (pSurfaceType)
							{
								IParticleEffect* pBreakEffect = GetSurfaceEffect( pSurfaceType, SURFACE_BREAKAGE_TYPE("breakage"), paramsBreak );
								if (pBreakEffect)
									pNewEntity->LoadParticleEmitter( -1, pBreakEffect, &paramsBreak );

								if (gEnv->pMaterialEffects)
								{
									SMFXBreakageParams mfxBreakageParams;
									mfxBreakageParams.SetMatrix(mxPiece);
									mfxBreakageParams.SetMass(fTotalMass); // total mass of object
									mfxBreakageParams.SetHitImpulse(Breakage.vHitImpulse);
									mfxBreakageParams.SetExplosionImpulse(Breakage.fExplodeImpulse);
									mfxBreakageParams.SetVelocity(vLinVel);
									gEnv->pMaterialEffects->PlayBreakageEffect(pSurfaceType, SURFACE_BREAKAGE_TYPE("breakage"), mfxBreakageParams);
								}
							}
						}
					}
				}

				if (pPhysEnt)
				{
					pe_params_flags pf;
					pf.flagsOR = pef_log_collisions;
					pPhysEnt->SetParams(&pf);
					pe_simulation_params sp;
					sp.maxLoggedCollisions = 1;
					pPhysEnt->SetParams(&sp);
					if (fMass<=pVars->massLimitDebris)
					{
						pe_params_part pp;
						if (!is_unused(pVars->flagsColliderDebris))
							pp.flagsColliderAND=pp.flagsColliderOR = pVars->flagsColliderDebris;
						pp.flagsAND = pVars->flagsANDDebris;
						pPhysEnt->SetParams(&pp);
					}
					pe_params_foreign_data pfd;
					pfd.iForeignFlagsOR = PFF_UNIMPORTANT;
					pPhysEnt->SetParams(&pfd);
				}

				// Propagate parent velocity.
				Vec3 vPartCM = mxPiece.GetTranslation();
				pe_status_dynamics pd;
				if (pPhysEnt->GetStatus(&pd))
				{
					vPartCM = pd.centerOfMass;
				}

				pe_action_set_velocity sv;
				sv.v = vLinVel;
				sv.w = vAngVel;
				sv.v += vAngVel.Cross(vPartCM-vCM);
				pPhysEnt->Action(&sv);

				// Apply impulses, randomising point on each piece to provide variation & rotation.
				Vec3 vObj = vPartCM + BiRandom(Vec3(pSubObj->GetRadius() * 0.5f));
				float fHitRadius = pPiecesObj->GetRadius();

				pe_action_impulse imp;

				if (Breakage.fExplodeImpulse != 0.f)
				{
					// Radial explosion impulse, largest at perimeter.
					imp.point = vCM;
					imp.impulse = vObj - imp.point;
					imp.impulse *= Breakage.fExplodeImpulse / fHitRadius;
					pPhysEnt->Action(&imp);
				}
				if (!Breakage.vHitImpulse.IsZero())
				{
					// Impulse scales down by distance from hit point.
					imp.point = Breakage.vHitPoint;
					float fDist = (vObj - imp.point).GetLength();
					if (fDist <= fHitRadius)
					{
						imp.point = vObj;
						imp.impulse = Breakage.vHitImpulse * (1.f - fDist / fHitRadius);
						pPhysEnt->Action(&imp);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::BreakIntoPieces( IEntity* pEntity, int nOrigSlot, int nPiecesSlot, BreakageParams const& Breakage )
{
	IEntityRenderProxy *pRenderProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER);
	if (pRenderProxy)
	{
		IRenderNode *pRenderNode = pRenderProxy->GetRenderNode();
		if (pRenderNode)
		{
			gEnv->p3DEngine->DeleteEntityDecals(pRenderNode);

			Matrix34 mx1 = pEntity->GetSlotWorldTM(nOrigSlot);
			Matrix34 mx2 = pEntity->GetSlotWorldTM(nPiecesSlot);
			
			IStatObj* pPiecesObj = pEntity->GetStatObj(nPiecesSlot|ENTITY_SLOT_ACTUAL);
			
			if (pPiecesObj && pPiecesObj->GetParentObject())
				pPiecesObj = pPiecesObj->GetParentObject();

			GeomRef geoOrig;	
			geoOrig.m_pStatObj = pEntity->GetStatObj(nOrigSlot|ENTITY_SLOT_ACTUAL);
			geoOrig.m_pChar = pEntity->GetCharacter(nOrigSlot);
			geoOrig.m_pPhysEnt = pEntity->GetPhysics();

			BreakIntoPieces(geoOrig,mx1, pPiecesObj,mx2, Breakage,pRenderNode->GetMaterialLayers());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::CreateSurfaceEffect( IStatObj *pStatObj,const Matrix34 &tm,const char* sType )
{
	GeomRef geo;
	geo.m_pStatObj = pStatObj;

	SpawnParams params;

	ISurfaceType* pSurfaceType = GetSurfaceType(geo);
	if (pSurfaceType)
	{
		IParticleEffect* pEffect = GetSurfaceEffect( pSurfaceType, sType, params );
		if (pEffect)
		{
			pEffect->Spawn(true,tm);
		}
		if (gEnv->pMaterialEffects)
		{
			float fMass = 0.0f;
			float fDensity = 0.0f;
			if (pStatObj->GetPhysicalProperties(fMass, fDensity))
			{
				if (fMass <= 0.0f)
				{
					if (phys_geometry * pPhysGeom = pStatObj->GetPhysGeom())
					{
						fMass = fDensity * pPhysGeom->V;
					}
				}
			}
			if (fMass < 0.0f)
				fMass = 0.0f;

			SMFXBreakageParams mfxBreakageParams;
			mfxBreakageParams.SetMatrix(tm);
			mfxBreakageParams.SetMass(fMass);
			gEnv->pMaterialEffects->PlayBreakageEffect(pSurfaceType, sType, mfxBreakageParams);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::AttachSurfaceEffect( IEntity* pEntity, int nSlot, const char* sType, SpawnParams const& paramsIn )
{
	SpawnParams params = paramsIn;
	params.nAttachSlot = nSlot;

	GeomRef geo;
	geo.m_pStatObj = pEntity->GetStatObj(nSlot);
	geo.m_pChar = pEntity->GetCharacter(nSlot);

	IParticleEffect* pEffect = GetSurfaceEffect( GetSurfaceType(geo), sType, params );
	if (pEffect)
	{
		if (!params.bIndependent)
			pEntity->LoadParticleEmitter( -1, pEffect, &params );
		else
		{
			IParticleEmitter* pEmitter = pEffect->Spawn(true, pEntity->GetWorldTM() );
			if (pEmitter)
			{
				pEmitter->SetSpawnParams(params, geo);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::CreateObjectAsParticles( IStatObj *pStatObj,IPhysicalEntity *pPhysEnt,SCreateParams &createParams )
{
	ParticleParams params;
	params.fCount = 0;
	params.fParticleLifeTime = 15.f;
	params.fParticleLifeTime.SetRandomVar(0.25f);
	params.bRemainWhileVisible = true;
	params.fSize = createParams.fScale;
	params.ePhysicsType = ParticlePhysics_RigidBody;
	params.nMaxCollisionEvents = 1;

	Matrix34 tm = createParams.worldTM * createParams.slotTM;
	tm.OrthonormalizeFast();
	IParticleEmitter* pEmitter = gEnv->p3DEngine->CreateParticleEmitter( true, tm, params );
	if (!pEmitter)
		return;

	pEmitter->SetMaterialLayers(createParams.nMatLayers);

	pe_action_reset_part_mtx arpm;
	pPhysEnt->Action(&arpm);
	pStatObj->AddRef();
	pEmitter->EmitParticle( pStatObj, pPhysEnt );

	pe_params_part pp;
	pp.idmatBreakable = -1;
	pPhysEnt->SetParams(&pp);

	// enable sounds generation for the new parts
	pe_params_flags pf;
	pf.flagsOR = pef_log_collisions;
	pPhysEnt->SetParams(&pf);
	pe_simulation_params sp;
	sp.maxLoggedCollisions = 1;
	pPhysEnt->SetParams(&sp);
	// By default such entities are marked as unimportant.
	pe_params_foreign_data pfd;
	pfd.iForeignFlagsOR = PFF_UNIMPORTANT;
	pPhysEnt->SetParams(&pfd);
}

//////////////////////////////////////////////////////////////////////////
CEntity* CBreakableManager::CreateObjectAsEntity( IStatObj *pStatObj,IPhysicalEntity *pPhysEnt,SCreateParams &createParams, bool bCreateSubstProxy )
{
	// Create new Rigid body entity.
	IEntityClass *pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
	if (!pClass)
		return 0;

	CreateObjectCommon( pStatObj,pPhysEnt,createParams );

	CPhysicalProxy *pPhysProxy =0;
	CRenderProxy *pRenderProxy = 0;

	CEntity *pEntity = (CEntity*)pPhysEnt->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
	if (!pEntity || !(pEntity->GetFlags() & ENTITY_FLAG_SPAWNED))
	{
		SEntitySpawnParams params;
		params.nFlags = ENTITY_FLAG_CLIENT_ONLY|ENTITY_FLAG_CASTSHADOW|
			ENTITY_FLAG_SPAWNED|createParams.nEntityFlagsAdd;
		params.pClass = pClass;
		params.vScale = Vec3(createParams.fScale,createParams.fScale,createParams.fScale);
		params.sName = createParams.pName;
		pEntity = (CEntity*)gEnv->pEntitySystem->SpawnEntity( params );
		if (!pEntity)
			return 0;
		pPhysProxy = (CPhysicalProxy*)pEntity->CreateProxy(ENTITY_PROXY_PHYSICS);
		pRenderProxy = (CRenderProxy*)pEntity->CreateProxy(ENTITY_PROXY_RENDER);

		pPhysProxy->AttachToPhysicalEntity( pPhysEnt );

		// Copy custom material from src render proxy to this render proxy.
		//if (createParams.pCustomMtl)
		//	pEntity->SetMaterial( createParams.pCustomMtl );
		if (createParams.nMatLayers)
			pRenderProxy->SetMaterialLayers( createParams.nMatLayers );
		pRenderProxy->SetRndFlags( createParams.nRenderNodeFlags );
	}
	else
	{
		pPhysProxy = (CPhysicalProxy*)pEntity->CreateProxy(ENTITY_PROXY_PHYSICS);
		pRenderProxy = (CRenderProxy*)pEntity->CreateProxy(ENTITY_PROXY_RENDER);
	}

	pe_params_foreign_data pfd;
	pfd.iForeignFlagsOR = PFF_UNIMPORTANT;
	pPhysEnt->SetParams(&pfd);

	//pCreateEvent->partidNew
	pRenderProxy->SetSlotGeometry(createParams.nSlotIndex,pStatObj);
	if (createParams.pCustomMtl && createParams.pCustomMtl!=pStatObj->GetMaterial())
		pEntity->SetSlotMaterial(createParams.nSlotIndex, createParams.pCustomMtl);

	if (!((Matrix33)createParams.slotTM).IsIdentity() || !createParams.slotTM.GetTranslation().IsZero())
	{
		pEntity->SetSlotLocalTM(createParams.nSlotIndex,createParams.slotTM);
	}

	if (createParams.nEntitySlotFlagsAdd)
		pEntity->SetSlotFlags(createParams.nSlotIndex, pEntity->GetSlotFlags(createParams.nSlotIndex) | createParams.nEntitySlotFlagsAdd);
	
	//pRenderProxy->InvalidateBounds(true,true);

	/*
	// Attach breakage effect.
	SpawnParams paramsBreak;
	paramsBreak.eAttachType = GeomType_Physics;
	paramsBreak.eAttachForm = GeomForm_Edges;
	IParticleEffect* pBreakEffect = GetSurfaceEffect( GetSurfaceType(pStatObj), SURFACE_BREAKAGE_TYPE("breakage"), paramsBreak );
	if (pBreakEffect)
	{
		paramsBreak.nAttachSlot = createParams.nSlotIndex;
		pEntity->LoadParticleEmitter( -1, pBreakEffect, &paramsBreak );
	}
	*/

	if (createParams.pSrcStaticRenderNode && bCreateSubstProxy)/* && gEnv->pSystem->IsEditor())
	{
		//////////////////////////////////////////////////////////////////////////
		// In editor allow restoring of the broken objects.
		//////////////////////////////////////////////////////////////////////////
		*/
	{
		CSubstitutionProxy *pSubstProxy = (CSubstitutionProxy*)pEntity->CreateProxy(ENTITY_PROXY_SUBSTITUTION);
		pSubstProxy->SetSubstitute(createParams.pSrcStaticRenderNode);
	}

	return pEntity;
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::CreateObjectCommon( IStatObj *pStatObj,IPhysicalEntity *pPhysEnt,SCreateParams &createParams )
{
	if (createParams.pSrcStaticRenderNode)
	{
		IRenderNode *pRenderNode = createParams.pSrcStaticRenderNode;
		// Make sure the original render node is not rendered anymore.
		gEnv->p3DEngine->DeleteEntityDecals(pRenderNode);
		gEnv->p3DEngine->UnRegisterEntity(pRenderNode);
		pRenderNode->SetPhysics(0); // since the new CEntity hijacked the entity already
	}

	// enable sounds generation for the new parts
	pe_params_flags pf;
	pf.flagsOR = pef_log_collisions;
	pPhysEnt->SetParams(&pf);
	pe_simulation_params sp;
	sp.maxLoggedCollisions = 1;
	pPhysEnt->SetParams(&sp);
}


//////////////////////////////////////////////////////////////////////////
bool CBreakableManager::CheckForPieces(IStatObj *pSrcStatObj, IStatObj::SSubObject *pSubObj, const Matrix34 &worldTM, int nMatLayers, 
																			 IPhysicalEntity *pPhysEnt)
{
	const char *sProperties = pSubObj->pStatObj->GetProperties();

	bool bMustShatter = (nMatLayers & MTL_LAYER_FROZEN);
	if (bMustShatter)
	{
		// Spawn frozen shatter effect.
		Matrix34 objWorldTM = worldTM * pSubObj->tm;
		GeomRef geoOrig;
		geoOrig.m_pStatObj = pSubObj->pStatObj;
		geoOrig.m_pPhysEnt = pPhysEnt;
		BreakageParams Breakage;
		Breakage.bMaterialEffects = true;
		Breakage.type = BREAKAGE_TYPE_FREEZE_SHATTER;
		// Only spawn freeze shatter material effect!
		BreakIntoPieces(geoOrig,objWorldTM,NULL,Matrix34(),Breakage,nMatLayers);
	}

	bool bHavePieces = (strstr(sProperties,"pieces=") != 0);
	// if new part is created for the frozen layer or if we have predefined pieces, break object into them.
	if (bHavePieces)
	{
		//////////////////////////////////////////////////////////////////////////
		// Spawn pieces of the object instead.
		GeomRef geoOrig;
		geoOrig.m_pStatObj = pSubObj->pStatObj;
		geoOrig.m_pChar = 0;
		geoOrig.m_pPhysEnt = pPhysEnt;

		IStatObj *pPiecesObj = pSrcStatObj;
		if (pPiecesObj->GetCloneSourceObject())
			pPiecesObj = pPiecesObj->GetCloneSourceObject();

		BreakageParams Breakage;
		Breakage.bOnlyHelperPieces = true;
		Breakage.bMaterialEffects = true;
		Breakage.fExplodeImpulse = 0;
		Breakage.fParticleLifeTime = 30.f;
		Breakage.nGenericCount = 0;
		Breakage.vHitImpulse = Vec3(ZERO); // pCreateEvent->breakImpulse; maybe use real impulse here?
		Breakage.vHitPoint = worldTM.GetTranslation();
		
		if (nMatLayers & MTL_LAYER_FROZEN)
		{
			// We trying to shatter frozen object.
			Breakage.bMaterialEffects = false; // Shatter effect was already spawned.
		}

		Matrix34 objWorldTM = worldTM * pSubObj->tm;

		BreakIntoPieces(geoOrig,objWorldTM,pPiecesObj,worldTM,Breakage,nMatLayers);

		// New physical entity must be deleted.
		gEnv->pPhysicalWorld->DestroyPhysicalEntity(pPhysEnt);

		return true;
		//////////////////////////////////////////////////////////////////////////
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::HandlePhysicsCreateEntityPartEvent( const EventPhysCreateEntityPart *pCreateEvent )
{
	int iForeignData = pCreateEvent->pEntity->GetiForeignData();
	void *pForeignData = pCreateEvent->pEntity->GetForeignData(iForeignData);

	CEntity *pSrcEntity = 0;
	CEntity *pNewEntity = 0;
	bool bNewEntity = false;
	bool bAlreadyDeformed = false;
	IStatObj *pSrcStatObj = 0;
	IStatObj *pNewStatObj = 0;
	IRenderNode *pSrcRenderNode = 0;
	IStatObj::SSubObject *pSubObj;
	IStatObj *pCutShape = 0;
	float cutShapeScale;
	CBreakableManager::SCreateParams createParams;
	int partidSrc = pCreateEvent->partidSrc;

	if (iForeignData == PHYS_FOREIGN_ID_ENTITY)
	{
		//CryLogAlways( "* CreateEvent Entity" );

		pSrcEntity = (CEntity*)pForeignData;
		pNewEntity = (CEntity*)pCreateEvent->pEntNew->GetForeignData(PHYS_FOREIGN_ID_ENTITY);

		if (pCreateEvent->partidSrc>=PARTID_CGA || pSrcEntity->GetCharacter(0) && pSrcEntity->GetCharacter(0)->GetOjectType()!=CGA)
		{
			pSrcEntity->GetCharacter(max(0,pCreateEvent->partidSrc/PARTID_CGA-1))->GetISkeletonPose()->SetPhysEntOnJoint(
				pCreateEvent->partidSrc%PARTID_CGA, pCreateEvent->pEntNew);
			return;
		}

		if (pCreateEvent->partidSrc>=PARTID_LINKED)
			pSrcEntity = (CEntity*)pSrcEntity->UnmapAttachedChild(partidSrc);

		//////////////////////////////////////////////////////////////////////////
		// Handle Breakage of Entity.
		//////////////////////////////////////////////////////////////////////////
		if (pCreateEvent->iReason!=EventPhysCreateEntityPart::ReasonMeshSplit)
		{
			createParams.nEntityFlagsAdd = ENTITY_FLAG_MODIFIED_BY_PHYSICS;
			pSrcEntity->AddFlags(createParams.nEntityFlagsAdd);
		}
		else 
		{
			createParams.nEntityFlagsAdd = pSrcEntity->GetFlags() & ENTITY_FLAG_MODIFIED_BY_PHYSICS;
		}
		//createParams.nSlotIndex = pCreateEvent->partidNew;
		createParams.worldTM = pSrcEntity->GetWorldTM();
		int i = partidSrc; 
		if (pSrcEntity->GetRenderProxy()->GetCompoundObj())
			i = ENTITY_SLOT_ACTUAL;
		pSrcStatObj = pSrcEntity->GetRenderProxy()->GetStatObj(i);
		createParams.slotTM = pSrcEntity->GetSlotLocalTM(i,false);
		/*if (pCreateEvent->partidSrc>=PARTID_CGA)
		{
			if (pSrcEntity->GetRenderProxy())
				pSrcEntity->GetRenderProxy()->SetSlotGeometry(pCreateEvent->partidSrc,0);
		}*/

		CRenderProxy *pSrcRenderProxy = (CRenderProxy*)pSrcEntity->GetRenderProxy();
		if (pSrcRenderProxy)
		{
			pSrcRenderNode = pSrcRenderProxy->GetRenderNode();
			createParams.nMatLayers = pSrcRenderProxy->GetMaterialLayersMask();
			createParams.nRenderNodeFlags = pSrcRenderProxy->GetRndFlags();
			createParams.pCustomMtl = pSrcEntity->GetMaterial();
		}
		createParams.fScale = pSrcEntity->GetScale().x;
		createParams.pName = pSrcEntity->GetName();

		if (pNewEntity)
			pNewStatObj = pNewEntity->GetStatObj(ENTITY_SLOT_ACTUAL);
	}
	else if (iForeignData == PHYS_FOREIGN_ID_STATIC && !pCreateEvent->bInvalid)
	{
		//CryLogAlways( "* CreateEvent Static" );

		IStatObj *pChildStatObj = 0;

		pNewEntity = (CEntity*)pCreateEvent->pEntNew->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
		if (pNewEntity)
		{
			pNewStatObj = pNewEntity->GetStatObj(ENTITY_SLOT_ACTUAL);
		}
		
		//////////////////////////////////////////////////////////////////////////
		// Handle Breakage of Brush or Vegetation.
		//////////////////////////////////////////////////////////////////////////
		IRenderNode *pRenderNode = (IRenderNode*)pForeignData;
		if (pRenderNode)
		{
			createParams.pSrcStaticRenderNode = pRenderNode;

			Matrix34 nodeTM;
			pSrcStatObj = pRenderNode->GetEntityStatObj(0, 0, &nodeTM);
			createParams.fScale = nodeTM.GetColumn(0).len();

			createParams.nSlotIndex = pCreateEvent->partidNew;
			createParams.worldTM = nodeTM;

			createParams.nMatLayers = pRenderNode->GetMaterialLayers();
			createParams.nRenderNodeFlags = pRenderNode->GetRndFlags();
			createParams.pCustomMtl = pRenderNode->GetMaterial();
			createParams.pName = pRenderNode->GetName();
		}
	}

	// Handle compound object.
	if (pSrcStatObj)
	{
		if (pSrcStatObj->GetFlags()&STATIC_OBJECT_COMPOUND)
		{
			if (pCreateEvent->nTotParts > 1 || pNewEntity!=0 && pNewEntity==pSrcEntity)
			{
				// Multi-parts object.
				if (!pNewStatObj)
				{
					pNewStatObj = pSrcStatObj->Clone(false,false,true);
					pNewStatObj->SetFlags( pNewStatObj->GetFlags()|STATIC_OBJECT_GENERATED );
				}
				pNewStatObj->SetSubObjectCount((pNewEntity==pSrcEntity ? pNewStatObj->GetSubObjectCount():0)+pCreateEvent->nTotParts);
				pNewStatObj->CopySubObject( pCreateEvent->partidNew,pSrcStatObj,partidSrc );
				assert(pCreateEvent->nTotParts>1 || pCreateEvent->partidNew==pNewStatObj->GetSubObjectCount()-1);
				if (pNewStatObj->GetSubObject(pCreateEvent->partidNew))
					pNewStatObj->GetSubObject(pCreateEvent->partidNew)->bHidden = false;

				//////////////////////////////////////////////////////////////////////////
				if (pCreateEvent->pMeshNew)
				{
					bAlreadyDeformed = true;
					IStatObj *pDeformedStatObj = gEnv->p3DEngine->UpdateDeformableStatObj(pCreateEvent->pMeshNew, pCreateEvent->pLastUpdate);
					createParams.nEntitySlotFlagsAdd = ENTITY_SLOT_BREAK_AS_ENTITY;
					if (pCreateEvent->bInvalid && pDeformedStatObj)
					{
						pDeformedStatObj->Release();
						pDeformedStatObj=0;
					}
					pSubObj = pNewStatObj->GetSubObject(pCreateEvent->partidNew);
					if (pSubObj && (pSubObj->pStatObj = pDeformedStatObj))
						pDeformedStatObj->AddRef();
				}

				//////////////////////////////////////////////////////////////////////////
				if (pNewEntity)
					pNewStatObj = 0; // Do not apply it again on entity.
			}
			else
			{
				pSubObj = pSrcStatObj->GetSubObject(partidSrc);
				if (pSubObj && pSubObj->pStatObj != NULL && pSubObj->nType == STATIC_SUB_OBJECT_MESH)
				{
					// marcok: make sure decals get removed (might need a better place)
					if (pSrcRenderNode)
						gEnv->p3DEngine->DeleteEntityDecals(pSrcRenderNode);

					if (CheckForPieces(pSrcStatObj,pSubObj, createParams.worldTM,createParams.nMatLayers, pCreateEvent->pEntNew))
						return;

					pNewStatObj = pSubObj->pStatObj;
					if (!pSubObj->bIdentityMatrix)
						createParams.slotTM = pSubObj->tm;
				}
			}
		}
		else
		{
			// Not sub object.
			pNewStatObj = pSrcStatObj;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	if (pCreateEvent->pMeshNew && !bAlreadyDeformed)
	{
		if (pCreateEvent->pLastUpdate && pCreateEvent->pLastUpdate->pMesh[1] && 
				(pCutShape = (IStatObj*)pCreateEvent->pLastUpdate->pMesh[1]->GetForeignData()))
			pCutShape = pCutShape->GetLastBooleanOp(cutShapeScale);

		pNewStatObj = gEnv->p3DEngine->UpdateDeformableStatObj(pCreateEvent->pMeshNew, pCreateEvent->pLastUpdate);
		if (pNewStatObj)
		{
			pNewStatObj->SetFlags( pNewStatObj->GetFlags()|STATIC_OBJECT_GENERATED );
		}
		createParams.nEntitySlotFlagsAdd = ENTITY_SLOT_BREAK_AS_ENTITY;
		if (pCreateEvent->bInvalid && pNewStatObj)
		{
			pNewStatObj->Release();
			pNewStatObj=0;
		}
	}
	//////////////////////////////////////////////////////////////////////////

	if (pNewStatObj)
	{
		if (strstr(pNewStatObj->GetProperties(),"entity"))
			createParams.nEntitySlotFlagsAdd |= ENTITY_SLOT_BREAK_AS_ENTITY;

		if (!(createParams.nEntitySlotFlagsAdd & ENTITY_SLOT_BREAK_AS_ENTITY) && pCreateEvent->nTotParts==1 && 
				pCreateEvent->pEntity!=pCreateEvent->pEntNew && !(pNewStatObj->GetFlags() & STATIC_OBJECT_GENERATED)) 
		{
			CreateObjectAsParticles( pNewStatObj,pCreateEvent->pEntNew,createParams );

			if (pCreateEvent->nTotParts)
			{

			}
		}	
		else
		{
			IEntity *pPrevNewEntity = pNewEntity;
			if (pCreateEvent->pEntity->GetType()==PE_ARTICULATED)
			{			
				Matrix34 mtx = createParams.worldTM;
				mtx.ScaleTranslation(0);
				createParams.slotTM = mtx*createParams.slotTM;
			}
			pNewEntity = CreateObjectAsEntity( pNewStatObj,pCreateEvent->pEntNew,createParams );
			bNewEntity = pNewEntity != pPrevNewEntity;
		}
	}

	if (bNewEntity && pNewEntity && pSrcEntity && (iForeignData == PHYS_FOREIGN_ID_ENTITY) && pNewEntity->GetPhysicalProxy()->PhysicalizeFoliage(0))
		pSrcEntity->GetPhysicalProxy()->DephysicalizeFoliage(0);

	//////////////////////////////////////////////////////////////////////////
	// Object have been cut, procedural breaking...
	//////////////////////////////////////////////////////////////////////////
	if (pCutShape && pCreateEvent->cutRadius > 0 && pNewStatObj)
	{
		Matrix34 tm = Matrix34::CreateTranslationMat(createParams.worldTM.TransformPoint(pCreateEvent->cutPtLoc[1]));

		// Spawn break destroy effect.
		SpawnParams paramsDestroy;
		paramsDestroy.eAttachForm = GeomForm_Surface;
		paramsDestroy.eAttachType = GeomType_None;
		paramsDestroy.fCountScale = 1.0f;
		paramsDestroy.bIndependent = true;
		paramsDestroy.bCountPerUnit = false;
		const char *sType = SURFACE_BREAKAGE_TYPE("breakage");
		if (createParams.nMatLayers & MTL_LAYER_FROZEN)
		{
			sType = SURFACE_BREAKAGE_TYPE("freeze_shatter");
		}

		ISurfaceType* pSurfaceType = GetSurfaceType(pNewStatObj);
		if (pSurfaceType)
		{
			IParticleEffect* pEffect = GetSurfaceEffect( pSurfaceType, sType, paramsDestroy );
			if (pEffect)
			{
				IParticleEmitter* pEmitter = pEffect->Spawn(true, tm);
				if (pEmitter)
				{
					pEmitter->SetMaterialLayers(createParams.nMatLayers);
					//pEmitter->SetSpawnParams(paramsDestroy);
				}
			}
			if (gEnv->pMaterialEffects)
			{
				if (createParams.pSrcStaticRenderNode && 
					  createParams.pSrcStaticRenderNode->GetRenderNodeType() == eERType_Vegetation)
				{
					int a = 0;
				}

				float fMass=0.0f;
				float fDensity=0.0f;
				float fVolume=0.0f;
				Vec3  vCM(ZERO);
				Vec3  vLinVel(ZERO);
				Vec3  vAngVel(ZERO);
				if (pCreateEvent->pEntNew)
					GetPhysicsStatus(pCreateEvent->pEntNew, fMass, fVolume, fDensity, vCM, vLinVel, vAngVel);
				if (fDensity <= 0.0f)
					pNewStatObj->GetPhysicalProperties( fMass,fDensity );
				SMFXBreakageParams mfxBreakageParams;
				mfxBreakageParams.SetMatrix(tm);
				mfxBreakageParams.SetHitImpulse(pCreateEvent->breakImpulse);
				mfxBreakageParams.SetMass(fMass);

				gEnv->pMaterialEffects->PlayBreakageEffect(pSurfaceType, sType, mfxBreakageParams);
			}
		}
	}

	if (pCreateEvent->cutRadius>0 && pCreateEvent->cutDirLoc[1].len2()>0 && pCutShape && (pSubObj = pCutShape->FindSubObject("splinters")))
	{
		Matrix34 tm;
		float rscale;
		if (pNewEntity)
		{
			rscale = 1.0f/pNewEntity->GetScale().x;
			tm.SetRotation33(Matrix33::CreateRotationV0V1(Vec3(0,0,1),pCreateEvent->cutDirLoc[1]));
			tm.Scale(Vec3(pCreateEvent->cutRadius*rscale*pSubObj->helperSize.x)); 
			tm.SetTranslation(pCreateEvent->cutPtLoc[1]*rscale);	
			pNewEntity->SetSlotLocalTM(pNewEntity->SetStatObj(pSubObj->pStatObj,-1,false), tm);
		}
		if (pSrcEntity)
		{
			rscale = 1.0f/pSrcEntity->GetScale().x;
			tm.SetRotation33(Matrix33::CreateRotationV0V1(Vec3(0,0,1),pCreateEvent->cutDirLoc[0]));
			tm.Scale(Vec3(pCreateEvent->cutRadius*rscale*pSubObj->helperSize.x)); 
			tm.SetTranslation(pCreateEvent->cutPtLoc[0]*rscale);
			pSrcEntity->SetSlotLocalTM(pSrcEntity->SetStatObj(pSubObj->pStatObj,-1,false), tm);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::HandlePhysicsRemoveSubPartsEvent( const EventPhysRemoveEntityParts *pRemoveEvent )
{
	int iForeignData = pRemoveEvent->pEntity->GetiForeignData();
	void *pForeignData = pRemoveEvent->pEntity->GetForeignData(iForeignData);

	bool bNewObject = false;
	CEntity *pEntity = NULL;
	IStatObj *pStatObj = NULL;
	Matrix34 nodeTM;
	IRenderNode *pRenderNode = NULL;

	int i,j,idOffs=pRemoveEvent->idOffs;
	for(i=j=0; i<sizeof(pRemoveEvent->partIds)/sizeof(pRemoveEvent->partIds[0]); i++)
		j |= pRemoveEvent->partIds[i];
	if (!j)
		return;

	if (iForeignData == PHYS_FOREIGN_ID_ENTITY)
	{
		//CryLogAlways( "* RemoveEvent Entity" );
		pEntity = (CEntity*)pForeignData;
		if (pEntity && pRemoveEvent->idOffs>=PARTID_LINKED)
			pEntity = (CEntity*)pEntity->UnmapAttachedChild(idOffs);
		if (pEntity)
			pStatObj = pEntity->GetStatObj(ENTITY_SLOT_ACTUAL);
		if (pStatObj && !(pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND))
		{
			// If entity only hosts a single geometry and nothing remains, delete entity itself.
			pStatObj = 0;
			if (pEntity->GetFlags() & ENTITY_FLAG_SPAWNED)
			{
				pEntity->SetStatObj(0,0,false);
				pEntity->UnphysicalizeSlot(0);
			}
		}
		nodeTM = pEntity->GetWorldTM();
		pRenderNode = pEntity->GetRenderProxy();
	}
	else if (iForeignData == PHYS_FOREIGN_ID_STATIC)
	{
		//CryLogAlways( "* RemoveEvent Entity Static" );
		pRenderNode = (IRenderNode*)pForeignData;
		pStatObj = pRenderNode->GetEntityStatObj(0, 0, &nodeTM);
	}

	if (pStatObj && pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND)
	{
		if (!(pStatObj->GetFlags() & STATIC_OBJECT_GENERATED))
		{
			// Make Unique compound static object.
			pStatObj = pStatObj->Clone(false,false,true);
			pStatObj->SetFlags( pStatObj->GetFlags()|STATIC_OBJECT_GENERATED );
			bNewObject = true;
		}

		IStatObj::SSubObject *pSubObj;
		// Delete specified sub-objects.
		for(i=sizeof(pRemoveEvent->partIds)*8-1;i>=0;i--)
		{
			if (pRemoveEvent->partIds[i>>5] & 1<<(i&31))
			{
				// This sub-object must be removed.
				pSubObj = pStatObj->GetSubObject(i+idOffs);
				if (pSubObj)
					pSubObj->bHidden = true;
			}
		}
		for(i=pStatObj->GetSubObjectCount()-1; i>=0 && 
				(pStatObj->GetSubObject(i)->bHidden || !pStatObj->GetSubObject(i)->pStatObj); i--);
		pStatObj->SetSubObjectCount(i+1);
		pStatObj->Invalidate(false);

		int nMeshes,iMesh;
		for(i=0,nMeshes=0;i<pStatObj->GetSubObjectCount();i++) 
		{
			pSubObj = pStatObj->GetSubObject(i);
			if (pSubObj && pSubObj->nType == STATIC_SUB_OBJECT_MESH)
				nMeshes++,iMesh=i;
		}
		if (nMeshes==1 && pRenderNode && CheckForPieces(pStatObj,pSubObj, nodeTM,pRenderNode->GetMaterialLayers(), pRemoveEvent->pEntity))
			return;
	}

	// Make an entity for the remaining piece.
	if (bNewObject)
	{
		if (pEntity)
		{
			pEntity->SetStatObj(pStatObj,ENTITY_SLOT_ACTUAL,false);
		}
		else
		{
			SCreateParams createParams;

			createParams.pSrcStaticRenderNode = pRenderNode;
			createParams.fScale = nodeTM.GetColumn(0).len();

			createParams.nSlotIndex = 0;
			createParams.worldTM = nodeTM;

			createParams.nMatLayers = pRenderNode->GetMaterialLayers();
			createParams.nRenderNodeFlags = pRenderNode->GetRndFlags();
			createParams.pCustomMtl = pRenderNode->GetMaterial();
			if (iForeignData==PHYS_FOREIGN_ID_STATIC)
				createParams.nEntityFlagsAdd = ENTITY_FLAG_MODIFIED_BY_PHYSICS;
			createParams.pName = pRenderNode->GetName();

			CreateObjectAsEntity( pStatObj,pRemoveEvent->pEntity,createParams,true );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
int CBreakableManager::HandlePhysics_UpdateMeshEvent( const EventPhysUpdateMesh *pUpdateEvent )
{

	CEntity *pCEntity;
	Matrix34 mtx;
	int iForeignData = pUpdateEvent->pEntity->GetiForeignData();
	void *pForeignData = pUpdateEvent->pEntity->GetForeignData(iForeignData);
	IFoliage *pSrcFoliage=0;

	bool bNewEntity = false;

	if (iForeignData==PHYS_FOREIGN_ID_ENTITY)
	{
		pCEntity = (CEntity*)pForeignData;
		if (!pCEntity || !pCEntity->GetPhysicalProxy())
			return 1;
	} else if (iForeignData==PHYS_FOREIGN_ID_STATIC)
	{
		CBreakableManager::SCreateParams createParams;

		CBreakableManager* pBreakMgr = (CBreakableManager*)GetIEntitySystem()->GetBreakableManager();

		IRenderNode *pRenderNode = (IRenderNode*)pUpdateEvent->pForeignData;
		IStatObj *pStatObj = pRenderNode->GetEntityStatObj(0, 0, &mtx);

		createParams.fScale = mtx.GetColumn(0).len();
		createParams.pSrcStaticRenderNode = pRenderNode;
		createParams.worldTM = mtx;
		createParams.nMatLayers = pRenderNode->GetMaterialLayers();
		createParams.pCustomMtl = pRenderNode->GetMaterial();
		createParams.nRenderNodeFlags = pRenderNode->GetRndFlags();
		createParams.pName = pRenderNode->GetName();
		if (pSrcFoliage = pRenderNode->GetFoliage())
			pSrcFoliage->AddRef();

		if (pStatObj)
		{
			pCEntity = pBreakMgr->CreateObjectAsEntity( pStatObj,pUpdateEvent->pEntity,createParams,true );
			bNewEntity = true;
		}

		/*
		// todo Danny - decide if this (or something similar) is useful
		// This passes just the smallest 2D outline into AI so that AI
		// can update its navigation graph
		IPhysicalEntity* pe = pUpdateEvent->pEntity;
		pe_status_pos status;
		pe->GetStatus(&status);
		{
		// outline (not self-intersecting please!) of the region to disable in AI
		std::list<Vec3> outline;
		static float extra = 3.0f; // expand the outline a bit - e.g. for trees
		outline.push_back(status.pos + Vec3( status.BBox[0].x - extra,  status.BBox[0].y - extra, 0.0f));
		outline.push_back(status.pos + Vec3( status.BBox[1].x + extra,  status.BBox[0].y - extra, 0.0f));
		outline.push_back(status.pos + Vec3( status.BBox[1].x + extra,  status.BBox[1].y + extra, 0.0f));
		outline.push_back(status.pos + Vec3( status.BBox[0].x - extra,  status.BBox[1].y + extra, 0.0f));
		gEnv->pAISystem->DisableNavigationInBrokenRegion(outline);
		} */
	}	else
		return 1;

	//if (pUpdateEvent->iReason==EventPhysUpdateMesh::ReasonExplosion)
	//	pCEntity->AddFlags(ENTITY_FLAG_MODIFIED_BY_PHYSICS);

	if (pCEntity)
	{
		pCEntity->GetPhysicalProxy()->DephysicalizeFoliage(0);
		IStatObj *pDeformedStatObj = gEnv->p3DEngine->UpdateDeformableStatObj(pUpdateEvent->pMesh,pUpdateEvent->pLastUpdate,pSrcFoliage);
		pCEntity->GetRenderProxy()->SetSlotGeometry( pUpdateEvent->partid,pDeformedStatObj );
		pCEntity->GetPhysicalProxy()->PhysicalizeFoliage(0);

		if (bNewEntity)
		{
			SEntityEvent entityEvent(ENTITY_EVENT_PHYS_BREAK);
			pCEntity->SendEvent( entityEvent );
		}
	}

	if (pSrcFoliage)
		pSrcFoliage->Release();

	return 1;
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::FakePhysicsEvent( EventPhys * pEvent )
{
	switch (pEvent->idval)
	{
	case EventPhysCreateEntityPart::id:
		HandlePhysicsCreateEntityPartEvent( static_cast<EventPhysCreateEntityPart*>(pEvent) );
		break;
	case EventPhysRemoveEntityParts::id:
		HandlePhysicsRemoveSubPartsEvent( static_cast<EventPhysRemoveEntityParts*>(pEvent) );
		break;
	case EventPhysUpdateMesh::id:
		HandlePhysics_UpdateMeshEvent( static_cast<EventPhysUpdateMesh*>(pEvent) );
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::FreezeRenderNode( IRenderNode *pRenderNode,bool bEnable )
{
	
}
