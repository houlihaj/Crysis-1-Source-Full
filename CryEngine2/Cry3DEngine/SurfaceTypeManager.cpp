////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   SurfaceManager.cpp
//  Version:     v1.00
//  Created:     29/9/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "SurfaceTypeManager.h"

#include <IScriptSystem.h>
#include <ICryPak.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#define BASE_DYNAMIC_SURFACE_ID 100
#define DEFAULT_MATERIAL_NAME "mat_default"

//////////////////////////////////////////////////////////////////////////
template <class TMap>
class CSurfaceTypeEnumerator : public ISurfaceTypeEnumerator
{
	std::vector<ISurfaceType*> m_items;
	typename std::vector<ISurfaceType*>::iterator m_iterator;

public:
	CSurfaceTypeEnumerator( TMap *pMap )
	{
		assert(pMap);
		m_items.reserve(pMap->size());
		for (typename TMap::iterator it = pMap->begin(); it != pMap->end(); ++it)
		{
			m_items.push_back( it->second->pSurfaceType );
		}
		m_iterator = m_items.begin();
	}
	virtual void Release() { delete this; };
	virtual ISurfaceType* GetFirst()
	{
		m_iterator = m_items.begin();
		if (m_iterator == m_items.end())
			return 0;
		return *m_iterator;
	}
	virtual ISurfaceType* GetNext()
	{
		if (m_iterator != m_items.end())
			m_iterator++;
		if (m_iterator == m_items.end())
			return 0;
		return *m_iterator;
	}
};

//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////
class CMaterialSurfaceType : public ISurfaceType
{
public:
	string m_name;
	string m_typename;
	int m_nId;
	int m_nFlags;
	SSurfaceTypeAIParams *m_aiParams;
	SPhysicalParams m_physParams;
	SBreakable2DParams *m_pBreakable2DParams;
	std::vector<SBreakageParticles> m_breakageParticles;

	CMaterialSurfaceType( const char *name )
	{
		m_name = name;
		m_nId = -1;
		m_nFlags = 0;
		m_aiParams = 0;
		m_pBreakable2DParams = 0;
		memset(&m_physParams,0,sizeof(m_physParams));
	}
	~CMaterialSurfaceType()
	{
	}

	//////////////////////////////////////////////////////////////////////////
	// ISurfaceType interface
	//////////////////////////////////////////////////////////////////////////
	virtual void Release() { delete this; }
	virtual uint16 GetId() { return m_nId; }
	virtual const char *GetName() { return m_name; }
	virtual const char *GetType() { return m_typename; };
	virtual int GetFlags() { return m_nFlags; };
	virtual void Execute( SSurfaceTypeExecuteParams &params ) {};
	virtual bool Load( int nId ) { m_nId = nId; return true; };
	virtual int GetBreakability() { return m_physParams.iBreakability; }
	virtual int GetHitpoints() { return (int)m_physParams.hit_points; }
	virtual float GetBreakEnergy() { return m_physParams.break_energy; }
	virtual const SSurfaceTypeAIParams* GetAIParams() { return m_aiParams; };
	virtual const SPhysicalParams& GetPhyscalParams() { return m_physParams; };
	virtual SBreakable2DParams* GetBreakable2DParams() { return m_pBreakable2DParams; };
	virtual SBreakageParticles* GetBreakageParticles( const char *sType,bool bLookInDefault=true );
	//////////////////////////////////////////////////////////////////////////
};

CMaterialSurfaceType *g_pDefaultSurfaceType = NULL;

//////////////////////////////////////////////////////////////////////////
CMaterialSurfaceType::SBreakageParticles* CMaterialSurfaceType::GetBreakageParticles( const char *sType,bool bLookInDefault )
{
	for (int i = 0,num = m_breakageParticles.size(); i < num; i++)
	{
		if (strcmp(sType,m_breakageParticles[i].type) == 0)
		{
			return &m_breakageParticles[i];
		}
	}
	if (bLookInDefault && g_pDefaultSurfaceType)
	{
		for (int i = 0,num = g_pDefaultSurfaceType->m_breakageParticles.size(); i < num; i++)
		{
			if (strcmp(sType,g_pDefaultSurfaceType->m_breakageParticles[i].type) == 0)
			{
				return &g_pDefaultSurfaceType->m_breakageParticles[i];
			}
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
static void ReloadSurfaceTypes( IConsoleCmdArgs *pArgs )
{
	CSurfaceTypeManager *pMgr = (CSurfaceTypeManager*)(gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager());
	pMgr->LoadSurfaceTypes();

}

//////////////////////////////////////////////////////////////////////////
// SurfaceManager implementation.
//////////////////////////////////////////////////////////////////////////
CSurfaceTypeManager::CSurfaceTypeManager( ISystem *pSystem )
{
	m_pSystem = pSystem;
	m_lastSurfaceId = BASE_DYNAMIC_SURFACE_ID;
	memset( m_idToSurface,0,sizeof(m_idToSurface) );
	
	m_pDefaultSurfaceType = new CMaterialSurfaceType(DEFAULT_MATERIAL_NAME);
	m_pDefaultSurfaceType->m_nId = 0;
	RegisterSurfaceType(m_pDefaultSurfaceType,true);
	g_pDefaultSurfaceType = m_pDefaultSurfaceType;

	LoadSurfaceTypes();

	gEnv->pConsole->AddCommand( "e_reloadSurfaces",&ReloadSurfaceTypes );
}

//////////////////////////////////////////////////////////////////////////
CSurfaceTypeManager::~CSurfaceTypeManager()
{
	g_pDefaultSurfaceType = NULL;
	RemoveAll();
	m_pDefaultSurfaceType->Release();
}

//////////////////////////////////////////////////////////////////////////
void CSurfaceTypeManager::RemoveAll()
{
	// Release all materials.
	for (NameToSurfaceMap::iterator it = m_nameToSurface.begin(); it != m_nameToSurface.end(); ++it)
	{
		SSurfaceRecord *pRec = it->second;
		if (pRec->pSurfaceType && pRec->pSurfaceType != m_pDefaultSurfaceType)
			pRec->pSurfaceType->Release();
		delete pRec;
	}
	m_nameToSurface.clear();
	memset( m_idToSurface,0,sizeof(m_idToSurface) );

	m_lastSurfaceId = BASE_DYNAMIC_SURFACE_ID;
	RegisterSurfaceType(m_pDefaultSurfaceType,true);
}

//////////////////////////////////////////////////////////////////////////
ISurfaceType* CSurfaceTypeManager::GetSurfaceTypeByName( const char *sName,const char *sWhy,bool warn )
{
	if (!sName || *sName == 0)
	{
		// Empty surface type.
		return m_pDefaultSurfaceType;
	}
	NameToSurfaceMap::iterator it = m_nameToSurface.find(CONST_TEMP_STRING(sName));
	
	SSurfaceRecord *pRec = 0;

	if(it!=m_nameToSurface.end())
		pRec = it->second;

	if (!pRec || !pRec->pSurfaceType)
	{
		if (!sWhy)
			sWhy = "";
    if (warn)
		  Warning( "'%s' undefined surface type, using mat_default (%s)",sName,sWhy );		
    return m_pDefaultSurfaceType;
	}

	if (!pRec->bLoaded)
	{
		pRec->pSurfaceType->Load( pRec->pSurfaceType->GetId() );
		pRec->bLoaded = true;
	}
	return pRec->pSurfaceType;
}

//////////////////////////////////////////////////////////////////////////
ISurfaceType* CSurfaceTypeManager::GetSurfaceTypeFast( int nSurfaceId,const char *sWhy )
{
	if (nSurfaceId <= 0 || nSurfaceId >= MAX_SURFACE_ID)
		return m_pDefaultSurfaceType;
	SSurfaceRecord *pRec = m_idToSurface[nSurfaceId];
	if (!pRec || !pRec->pSurfaceType)
	{
		//Warning( "'%d' undefined surface type id, using mat_default (%s)",nSurfaceId,sWhy );
		return m_pDefaultSurfaceType;
	}

	if (!pRec->bLoaded)
	{
		pRec->pSurfaceType->Load( pRec->pSurfaceType->GetId() );
		pRec->bLoaded = true;
	}
	return pRec->pSurfaceType;
}

//////////////////////////////////////////////////////////////////////////
ISurfaceType* CSurfaceTypeManager::GetSurfaceType( int nSurfaceId,const char *sWhy )
{
	return GetSurfaceTypeFast(nSurfaceId,sWhy);
}

//////////////////////////////////////////////////////////////////////////
ISurfaceTypeEnumerator* CSurfaceTypeManager::GetEnumerator()
{
	return new CSurfaceTypeEnumerator<NameToSurfaceMap>(&m_nameToSurface);
}

//////////////////////////////////////////////////////////////////////////
bool CSurfaceTypeManager::RegisterSurfaceType( ISurfaceType *pSurfaceType,bool bDefault )
{
	assert(pSurfaceType);

	int nSurfTypeId = pSurfaceType->GetId();

	SSurfaceRecord *pRecord = NULL;

	if (nSurfTypeId >=0 && nSurfTypeId <= MAX_SURFACE_ID)
	{
		pRecord = m_idToSurface[nSurfTypeId];
		if (pRecord)
			return true; // Already registered.
	}

	int nId = m_lastSurfaceId;
	if (nId > MAX_SURFACE_ID)
		return false;

	if (bDefault)
	{
		nId = 0;
		//m_pDefaultSurfaceType = pSurfaceType;
	}
	if (!pSurfaceType->Load( nId ))
		return false;

	if (!bDefault)
	{
		m_lastSurfaceId++;
	}
	
	pRecord = new SSurfaceRecord;
	pRecord->bLoaded = true;
	pRecord->pSurfaceType = pSurfaceType;

	m_idToSurface[nId] = pRecord;
	m_nameToSurface[pSurfaceType->GetName()] = pRecord;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CSurfaceTypeManager::UnregisterSurfaceType( ISurfaceType *pSurfaceType )
{
	assert(pSurfaceType);
	
	assert( pSurfaceType->GetId() <= MAX_SURFACE_ID);

	SSurfaceRecord *pRecord = m_idToSurface[pSurfaceType->GetId()];
	if (pRecord)
	{
		m_idToSurface[pSurfaceType->GetId()] = 0;
		m_nameToSurface.erase( pSurfaceType->GetName() );
		delete pRecord;
	}
}

//////////////////////////////////////////////////////////////////////////
void CSurfaceTypeManager::LoadSurfaceTypes()
{
	XmlNodeRef root = GetISystem()->LoadXmlFile( "Libs/MaterialEffects/SurfaceTypes.xml" );
	if (!root)
		return;

	m_lastSurfaceId = BASE_DYNAMIC_SURFACE_ID;
	RemoveAll();

	for (int i = 0,nChilds = root->getChildCount(); i < nChilds; i++)
	{
		XmlNodeRef matNode = root->getChild(i);
		if (!matNode->isTag("SurfaceType"))
			continue;

		const char *sName = matNode->getAttr("name");
		NameToSurfaceMap::iterator it = m_nameToSurface.find(CONST_TEMP_STRING(sName));
		CMaterialSurfaceType *pSurfaceType = 0;
		if (it != m_nameToSurface.end())
		{
			pSurfaceType = (CMaterialSurfaceType*)it->second->pSurfaceType;
		}
		if (strcmp(sName,DEFAULT_MATERIAL_NAME) == 0)
		{
			pSurfaceType = m_pDefaultSurfaceType;
		}

		if (!pSurfaceType)
		{
			pSurfaceType = new CMaterialSurfaceType(sName);
			bool bDefault = false;
			if (!RegisterSurfaceType( pSurfaceType,bDefault ))
				continue;
		}

		int   bManuallyBreakable=0;
		bool  bNoCollide = false;
		bool  vehicle_only_collisions = false;
		bool  nBreakable2d = 0;
		bool  can_shatter = false;
		
		ISurfaceType::SPhysicalParams &physParams = pSurfaceType->m_physParams;

		pSurfaceType->m_typename = matNode->getAttr("type");
		if (pSurfaceType->m_typename.empty())
		{
			// typename will be name with mat_ prefix.
			pSurfaceType->m_typename = pSurfaceType->m_name;
			if (pSurfaceType->m_typename.find("mat_") == 0)
			{
				pSurfaceType->m_typename = pSurfaceType->m_typename.substr(4);
			}
		}

		XmlNodeRef physNode = matNode->findChild("Physics");
		if (physNode)
		{
			physNode->getAttr( "friction",physParams.friction=1.0f );
			physNode->getAttr( "elasticity",physParams.bouncyness );
			physNode->getAttr( "breakable_id",physParams.breakable_id=-1 );
			physNode->getAttr( "pierceability",physParams.pierceability=0 );
			physNode->getAttr( "no_collide",bNoCollide=0 );
			physNode->getAttr( "break_energy",physParams.break_energy=0 );
			physNode->getAttr( "hit_points",physParams.hit_points=0 );
			physNode->getAttr( "hit_radius",physParams.hit_radius=10000.0f );
			physNode->getAttr( "hit_maxdmg",physParams.hit_maxdmg=1000 );
			physNode->getAttr( "hit_lifetime",physParams.hit_lifetime=10.0f );
			physNode->getAttr( "hole_size",physParams.hole_size );
			physNode->getAttr( "breakable_2d",nBreakable2d );
			physNode->getAttr( "vehicle_only_collisions",vehicle_only_collisions );
			physNode->getAttr( "can_shatter",can_shatter );

			pSurfaceType->m_physParams = physParams;

			if (physParams.break_energy != 0)
			{
				physParams.iBreakability = nBreakable2d ? 1:2;
				bManuallyBreakable = sf_manually_breakable;
			}

			if (bNoCollide)
				pSurfaceType->m_nFlags |= SURFACE_TYPE_NO_COLLIDE;
			if (vehicle_only_collisions)
				pSurfaceType->m_nFlags |= SURFACE_TYPE_VEHICLE_ONLY_COLLISION;
			if (can_shatter)
				pSurfaceType->m_nFlags |= SURFACE_TYPE_CAN_SHATTER;

			if (gEnv->pPhysicalWorld)
				gEnv->pPhysicalWorld->SetSurfaceParameters( pSurfaceType->GetId(),physParams.bouncyness,physParams.friction,
					sf_pierceability(physParams.pierceability) | sf_matbreakable(physParams.breakable_id) | bManuallyBreakable);
		}

		XmlNodeRef break2dNode = matNode->findChild("breakable_2d");
		if (break2dNode)
		{
			ISurfaceType::SBreakable2DParams break2dParams;
			break2dNode->getAttr("blast_radius",break2dParams.blast_radius=0.2f);
			break2dNode->getAttr("rigid_body",break2dParams.rigid_body);
			break2dNode->getAttr("lifetime",break2dParams.life_time=4.0f);
			break2dParams.particle_effect = break2dNode->getAttr("particle_effect");

			break2dNode->getAttr("cell_size",break2dParams.cell_size=0.1f);
			break2dNode->getAttr("max_patch_tris",break2dParams.max_patch_tris=6);
			break2dNode->getAttr("shard_density",break2dParams.shard_density=1200.0f);

			SAFE_DELETE(pSurfaceType->m_pBreakable2DParams);
			pSurfaceType->m_pBreakable2DParams = new ISurfaceType::SBreakable2DParams;
			*pSurfaceType->m_pBreakable2DParams = break2dParams;
		}

		{
			// Load Breakage particles.
			for (int i = 0; i < matNode->getChildCount(); i++)
			{
				XmlNodeRef breakageNode = matNode->getChild(i);
				if (breakageNode->isTag("BreakageParticles"))
				{
					ISurfaceType::SBreakageParticles params;
					breakageNode->getAttr("scale",params.scale);
					breakageNode->getAttr("count_scale",params.count_scale);
					breakageNode->getAttr("count_per_unit",params.count_per_unit);
					params.type = breakageNode->getAttr("type");
					params.particle_effect = breakageNode->getAttr("effect");

					pSurfaceType->m_breakageParticles.push_back(params);
				}
			}
		}

		XmlNodeRef aiNode = matNode->findChild("AI");
		if (aiNode)
		{
			ISurfaceType::SSurfaceTypeAIParams aiParams;
			aiNode->getAttr( "fImpactRadius",aiParams.fImpactRadius );
			aiNode->getAttr( "fImpactSoundRadius",aiParams.fImpactSoundRadius );
			aiNode->getAttr( "fFootStepRadius",aiParams.fFootStepRadius );
			aiNode->getAttr( "proneMult",aiParams.proneMult );
			aiNode->getAttr( "crouchMult",aiParams.crouchMult );
			aiNode->getAttr( "movingMult",aiParams.movingMult );

			SAFE_DELETE(pSurfaceType->m_aiParams);
			pSurfaceType->m_aiParams = new ISurfaceType::SSurfaceTypeAIParams;
			*pSurfaceType->m_aiParams = aiParams;
		}
	}
}
