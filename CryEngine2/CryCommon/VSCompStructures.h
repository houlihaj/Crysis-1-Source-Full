#ifndef __VS_COMP_STRUCTURES_H__
#define __VS_COMP_STRUCTURES_H__

#pragma once 

#define LEVELVS_PAK_NAME "LevelVS.pak"
#define VS_EXPORT_FILE_NAME "vertexscatter.dat"
#define MAX_VERTEX_SCATTER_LODS_NUM 6

struct SVertexScatterLODLevel
{
	int m_nNumVertices;
	Vec3* m_WorldSpacePositions;
	float* m_VertexScatterValues;

	SVertexScatterLODLevel();
	~SVertexScatterLODLevel();
};

struct SVertexScatterSubObject
{
	SVertexScatterLODLevel* m_VertexScatterLODLevels[MAX_VERTEX_SCATTER_LODS_NUM];

	SVertexScatterSubObject();
	~SVertexScatterSubObject();
};

struct SScatteringObjectData
{
	Vec4 m_VertexScatterTransformZ;
	int m_nNumSubObjects;
	SVertexScatterSubObject* m_VertexScatterSubObjects;

	SScatteringObjectData();
	~SScatteringObjectData();
};

#endif // __VS_COMP_STRUCTURES_H__
