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

#ifndef __EntityLoader_h__
#define __EntityLoader_h__
#pragma once

struct IMaterialManager;
class CEntitySystem;

class CEntityLoader
{
public:
	CEntityLoader( CEntitySystem *pEntitySystem );
	~CEntityLoader();

	bool LoadEntities( XmlNodeRef &objectsNode );
	bool LoadEntity( XmlNodeRef &entityNode );

private:
	void AddAttachment( EntityId nParent,EntityId nChild,const Vec3 &pos, const Quat &rot,const Vec3 &scale );
	void AttachChilds();
	void LoadFlowGraphProxies();

private:
	CEntitySystem *m_pEntitySystem;
	struct SAttachment
	{
		EntityId child;
		EntityId parent;
		Vec3 pos;
		Quat rot;
		Vec3 scale;
	};
	std::vector<SAttachment> m_attachments;
	IMaterialManager *m_pMatMan;
	struct SFlowGraph
	{
		IEntity *pEntity;
		XmlNodeRef pNode;
	};
	std::vector<SFlowGraph> m_flowgraphs;
};

#endif // __EntityLoader_h__