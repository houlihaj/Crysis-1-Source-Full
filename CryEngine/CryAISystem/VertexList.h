#ifndef _VERTEX_LIST_
#define _VERTEX_LIST_

#if _MSC_VER > 1000
#pragma once
#endif

#include "IAgent.h"
#include "GraphStructures.h"

class CCryFile;

class CVertexList
{
public:
	CVertexList(void);
	~CVertexList(void);
	int AddVertex(const ObstacleData & od);

	const ObstacleData & GetVertex(int index) const;
	ObstacleData &ModifyVertex(int index);
	int FindVertex(const ObstacleData & od) const;

  bool IsIndexValid(int index) const {return index >= 0 && index < m_obstacles.size();}

	void WriteToFile( const char* fileName );
	bool ReadFromFile( const char* fileName );

	void Clear() {m_obstacles.clear();}
  void Reset();
	int GetSize() {return m_obstacles.size();}
	int GetCapacity() {return m_obstacles.capacity();}
private:
	Obstacles m_obstacles;
};

#endif // #ifndef _VERTEX_LIST_