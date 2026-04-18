////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   EntityLoader.h
//  Version:     v1.00
//  Created:     22/6/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "EntityLoader.h"
#include "Entity.h"
#include "EntitySystem.h"
#include <CryPath.h>
#include <CryFile.h>
#include <INetwork.h>

//////////////////////////////////////////////////////////////////////////
CEntityLoader::CEntityLoader( CEntitySystem *pEntitySystem )
{
	m_pEntitySystem = pEntitySystem;
}

//////////////////////////////////////////////////////////////////////////
CEntityLoader::~CEntityLoader()
{
	AttachChilds();

	// now serialize all FlowGraph proxies. they rely on all EntityGUIDs being already assigned
	LoadFlowGraphProxies();
}

//////////////////////////////////////////////////////////////////////////
bool CEntityLoader::LoadEntities( XmlNodeRef &objectsNode )
{
	// the first pass reserves entity IDs, so dynamically spawned entities don't collide with
	// the IDs
	for (int i = 0; i < objectsNode->getChildCount(); i++)
	{
		XmlNodeRef objectNode = objectsNode->getChild(i);
		if (objectNode->isTag("Entity"))
		{
			// reserve the id
			EntityId id;
			if (objectNode->getAttr( "EntityId", id ))
			{
				m_pEntitySystem->ReserveEntityId(id);
			}
		}
	}

	// this pass actually loads the entities without any FlowGraph proxy
	// the FlowGraph proxies are loaded after all entities have been loaded
	// see CEntityLoader's dtor
	for (int i = 0; i < objectsNode->getChildCount(); i++)
	{
		XmlNodeRef objectNode = objectsNode->getChild(i);
		if (objectNode->isTag("Entity"))
		{
			LoadEntity(objectNode);
		}
		if (0 == (i&7))
		{
			gEnv->pNetwork->SyncWithGame(eNGS_FrameStart);
			gEnv->pNetwork->SyncWithGame(eNGS_FrameEnd);
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityLoader::LoadEntity( XmlNodeRef &node )
{
	int nMinSpec = -1;
	if (node->getAttr( "MinSpec",nMinSpec ))
	{
		if (nMinSpec > 0)
		{
			static ICVar* e_obj_quality(gEnv->pConsole->GetCVar("e_obj_quality"));
			int obj_quality = 0;
			if (e_obj_quality)
				obj_quality = e_obj_quality->GetIVal();

			if (nMinSpec > obj_quality && obj_quality != 0)
			{
				// If the entity minimal spec is higher then the current server object quality this entity will not be loaded.
				return true;
			}
		}
	}

	const char *sEntityClass = node->getAttr( "EntityClass" );
	
	IEntityClass *pClass = m_pEntitySystem->GetClassRegistry()->FindClass(sEntityClass);
	if (!pClass)
		return false;

	SEntitySpawnParams params;

	// Load spawn parameters from xml node.
	params.pClass = pClass;
	params.sName = node->getAttr( "Name" );
	params.sLayerName = node->getAttr( "Layer" );

	if (!gEnv->bMultiplayer)
		params.nFlags |= ENTITY_FLAG_UNREMOVABLE; // Entities loaded from the xml cannot be fully deleted in single player.

	Vec3 pos(0.0f,0.0f,0.0f);
	Quat rot(Quat::CreateIdentity());
	Vec3 scale(1.0f,1.0f,1.0f);

	node->getAttr( "Pos",pos );
	node->getAttr( "Rotate",rot );
	node->getAttr( "Scale",scale );

	/*
	Ang3 vAngles;
	if (node->getAttr( "Angles",vAngles ))
	{
		params.qRotation.SetRotationXYZ(vAngles);
	}
	*/
	if (!node->getAttr( "EntityId",params.id ))
	{
		params.id = 0;
	}
	node->getAttr( "EntityGuid",params.guid );

	// Get flags.
	bool bCastShadow = true; // true by default (do not change, it must be coordinated with editor export)
	//bool bRecvShadow = true; // true by default (do not change, it must be coordinated with editor export)

	bool bGoodOccluder = false; // false by default (do not change, it must be coordinated with editor export)
	bool bOutdoorOnly	=	false;
  
	node->getAttr( "CastShadow",bCastShadow );
	//node->getAttr( "RecvShadow",bRecvShadow );
	node->getAttr( "GoodOccluder",bGoodOccluder );
	node->getAttr( "OutdoorOnly",bOutdoorOnly);
	
	if (bCastShadow)
		params.nFlags |= ENTITY_FLAG_CASTSHADOW;
	//if (bRecvShadow)
		//params.nFlags |= ENTITY_FLAG_RECVSHADOW;
	if (bGoodOccluder)
		params.nFlags |= ENTITY_FLAG_GOOD_OCCLUDER;
	if(bOutdoorOnly)
		params.nFlags	|=	ENTITY_FLAG_OUTDOORONLY;

	const char *sArchetypeName = node->getAttr( "Archetype" );
	if (sArchetypeName[0])
	{
		params.pArchetype = m_pEntitySystem->LoadEntityArchetype( sArchetypeName );
	}

	params.vPosition = pos;
	params.qRotation = rot;
	params.vScale = scale;

	if (!m_pEntitySystem->OnBeforeSpawn(params))
		return false;

	// Spawn entity.
	IEntity* pEntity = m_pEntitySystem->SpawnEntity( params,false );
	if (!pEntity)
	{
		EntityWarning( "[CEntityLoader::LoadEntity] Entity Load Failed: %s (%s)",params.sName,sEntityClass );
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	// Make an Area proxy if needed.
	//////////////////////////////////////////////////////////////////////////
	if (node->findChild("Area"))
	{
		pEntity->CreateProxy(ENTITY_PROXY_AREA);
	}
	if (node->findChild("Rope"))
	{
		pEntity->CreateProxy(ENTITY_PROXY_ROPE);
	}

	// Check if XML node have Material attribute.
	if (node->haveAttr("Material"))
	{
		// Assign material to entity.
		const char *sMtlName = node->getAttr("Material");
		IMaterial *pMtl = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial( sMtlName );
		if (pMtl)
		{
			pEntity->SetMaterial( pMtl );
		}
	}

	// We know that all entities are derived from CEnttiy
	CEntity *pCEntity = (CEntity*)pEntity;
	
	// Serialize script proxy.
	CScriptProxy *pScriptProxy = pCEntity->GetScriptProxy();
	if (pScriptProxy)
		pScriptProxy->SerializeXML( node,true );

	m_pEntitySystem->InitEntity( pEntity,params );

	//////////////////////////////////////////////////////////////////////////
	// Load geom entity (Must be before serializing proxies.
	//////////////////////////////////////////////////////////////////////////
	if (pClass->GetFlags() & ECLF_DEFAULT)
	{
		// Check if it have geometry.
		const char *sGeom = node->getAttr("Geometry");
		if (sGeom[0] != 0)
		{
			// check if character.
			const char *ext = PathUtil::GetExt(sGeom);
			if (stricmp(ext,CRY_CHARACTER_FILE_EXT) == 0 ||
					stricmp(ext,CRY_CHARACTER_DEFINITION_FILE_EXT) == 0 ||
					stricmp(ext,CRY_ANIM_GEOMETRY_FILE_EXT) == 0)
			{
				pEntity->LoadCharacter( 0,sGeom,IEntity::EF_AUTO_PHYSICALIZE );
			}
			else
				pEntity->LoadGeometry( 0,sGeom,0,IEntity::EF_AUTO_PHYSICALIZE );
		}
	}
	//////////////////////////////////////////////////////////////////////////

	// Serialize all entity proxies except Script proxy after initialization.
	for (int i = 0,numProxy = pCEntity->ProxyCount(); i < numProxy; i++)
	{
		IEntityProxy *pProxy = pCEntity->GetProxyAt(i);
		if (pProxy && pProxy != pScriptProxy)
			pProxy->SerializeXML( node,true );
	}

	// Add attachment to parent.
	EntityId nParentId;
	if (node->getAttr( "ParentId",nParentId ))
	{
		AddAttachment( nParentId,params.id,pos,rot,scale );
	}

	// check for a flow graph
	// only store them for later serialization as the FG proxy relies
	// on all EntityGUIDs already loaded
	if (node->findChild("FlowGraph"))
	{
		SFlowGraph sFG;
		sFG.pEntity = pEntity;
		sFG.pNode = node;
		m_flowgraphs.push_back(sFG);
	}

	// Load entity links.
	{
		XmlNodeRef linksNode = node->findChild( "EntityLinks" );
		if (linksNode)
		{
			for (int i = 0,num = linksNode->getChildCount(); i < num; i++)
			{
				XmlNodeRef linkNode = linksNode->getChild(i);
				EntityId targetId = 0;
				linkNode->getAttr( "TargetId",targetId );
				const char *sLinkName = linkNode->getAttr( "Name" );
				pEntity->AddEntityLink(sLinkName,targetId);
			}
		}
	}

	// Hide entity in game. Done after potential RenderProxy is created, so it catches the Hide
	bool bHiddenInGame = false;
	node->getAttr( "HiddenInGame",bHiddenInGame );
	if (bHiddenInGame)
		pCEntity->Hide( true );

	bool bUseOccluders = false;
	node->getAttr( "UseOccluders",bUseOccluders );
	if (bUseOccluders)
		pCEntity->UpdateUseOccluders( true );

  if(nMinSpec>=0)
    if(IEntityRenderProxy * pProxy = (IEntityRenderProxy *)pCEntity->GetProxy(ENTITY_PROXY_RENDER))
      pProxy->GetRenderNode()->SetMinSpec(nMinSpec);

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEntityLoader::AddAttachment( EntityId nParent,EntityId nChild,const Vec3 &pos,const Quat &rot, const Vec3 &scale )
{
	SAttachment a;
	a.child = nChild;
	a.parent = nParent;
	a.pos=pos;
	a.rot=rot;
	a.scale=scale;
	m_attachments.push_back(a);
}

//////////////////////////////////////////////////////////////////////////
void CEntityLoader::AttachChilds()
{
	for (uint32 i = 0; i < m_attachments.size(); i++)
	{
		IEntity *pChild = m_pEntitySystem->GetEntity(m_attachments[i].child);
		IEntity *pParent = m_pEntitySystem->GetEntity(m_attachments[i].parent);
		if (pChild && pParent)
		{
			pParent->AttachChild(pChild);
			pChild->SetLocalTM(Matrix34::Create(m_attachments[i].scale, m_attachments[i].rot, m_attachments[i].pos));
		}
	}
	m_attachments.clear();
}

//////////////////////////////////////////////////////////////////////////
void CEntityLoader::LoadFlowGraphProxies()
{
	// serialize all entities' FlowGraph proxies.
	// they rely on all EntityGUIDs being already loaded/assigned
	std::vector<SFlowGraph>::iterator iter = m_flowgraphs.begin();
	while (iter != m_flowgraphs.end())
	{
		SFlowGraph& sFG= *iter;
		sFG.pEntity->CreateProxy(ENTITY_PROXY_FLOWGRAPH)->SerializeXML(sFG.pNode, true);
		++iter;
	}
	m_flowgraphs.clear();
}