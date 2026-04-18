#include "StdAfx.h"
#include "VertexList.h"
#include "AILog.h"
#include <ISystem.h>
#include <CryFile.h>

#define BAI_VERTEX_FILE_VERSION 1


CVertexList::CVertexList(void)
{
	m_obstacles.clear();
}

CVertexList::~CVertexList(void)
{
}

int CVertexList::FindVertex(const ObstacleData & od) const
{
  const Obstacles::const_iterator oiend = m_obstacles.end();
	int index=0;

	for (Obstacles::const_iterator oi=m_obstacles.begin() ; oi!=oiend ; ++oi, ++index)
	{
		if ( (*oi) == od )
			return index;
	}

	return -1;
}

int CVertexList::AddVertex(const ObstacleData & od)
{
	int index=FindVertex(od);
	if (index<0)
	{
		m_obstacles.push_back(od);
		index = (int)m_obstacles.size()-1;
	}

	return index;
}


const ObstacleData & CVertexList::GetVertex(int index) const
{
	if (!IsIndexValid(index))
  {
    AIError("CVertexList::GetVertex Tried to retrieve a non existing vertex (%d) from vertex list (size %d).Please regenerate the triangulation [Design bug]",
      index, m_obstacles.size());
    return m_obstacles[0];
  }
	return m_obstacles[index];
}

ObstacleData &CVertexList::ModifyVertex(int index)
{
	if (!IsIndexValid(index))
  {
    AIError("CVertexList::ModifyVertex Tried to retrieve a non existing vertex (%d) from vertex list (size %d).Please regenerate the triangulation [Design bug]",
    index, m_obstacles.size());
    static ObstacleData od;
    return od;
  }
	return m_obstacles[index];
}


void CVertexList::WriteToFile( const char* fileName )
{
	CCryFile file;
	if( false != file.Open( fileName, "wb" ) )
	{
		int nFileVersion = BAI_VERTEX_FILE_VERSION;
		file.Write( &nFileVersion, sizeof( int ) );

		int iNumber = (int)m_obstacles.size();
		std::vector<ObstacleDataDesc> obDescs;
		obDescs.resize(iNumber);
		for (int i = 0 ; i < iNumber ; ++i)
		{
			obDescs[i].vPos = m_obstacles[i].vPos;
			obDescs[i].vDir = m_obstacles[i].vDir;
			obDescs[i].fApproxRadius = m_obstacles[i].fApproxRadius;
			obDescs[i].bCollidable = m_obstacles[i].bCollidable;
			obDescs[i].approxHeight = m_obstacles[i].approxHeight;
		}
		file.Write( &iNumber, sizeof( int ) );
		if (!iNumber)
			return;
		file.Write( &obDescs[ 0 ], iNumber * sizeof( ObstacleDataDesc ) );
		file.Close();
	}
}

bool CVertexList::ReadFromFile( const char* fileName )
{
  m_obstacles.clear();

	CCryFile file;
	if (!file.Open(fileName, "rb"))
	{
		AIError("CVertexList::ReadFromFile could not open vertex file: Regenerate triangulation in the editor [Design bug]");
		return false;
	}

	int iNumber;

	AILogLoading("Verifying BAI file version");
	file.ReadType(&iNumber);
	if (iNumber != BAI_VERTEX_FILE_VERSION)
	{
		AIError("CVertexList::ReadFromFile Wrong vertex list BAI file version - found %d expected %d: Regenerate triangulation in the editor [Design bug]", iNumber, BAI_VERTEX_FILE_VERSION);
		file.Close();
		return false;
	}

	// Read number of descriptors.
	file.ReadType( &iNumber );

	if (iNumber>0) 
	{
    std::vector<ObstacleDataDesc> obDescs(iNumber);
		file.ReadType( &obDescs[0], iNumber );
    m_obstacles.resize(iNumber);
    for (int i = 0 ; i < iNumber ; ++i)
    {
      m_obstacles[i].vPos = obDescs[i].vPos;
      m_obstacles[i].vDir = obDescs[i].vDir;
      m_obstacles[i].fApproxRadius = obDescs[i].fApproxRadius;
      m_obstacles[i].bCollidable = obDescs[i].bCollidable;
			m_obstacles[i].approxHeight = obDescs[i].approxHeight;
    }
  }

	file.Close();

	return true;
}

//===================================================================
// Reset
//===================================================================
void CVertexList::Reset()
{
  const Obstacles::iterator oiend = m_obstacles.end();
	for (Obstacles::iterator oi=m_obstacles.begin() ; oi!=oiend ; ++oi)
	{
    ObstacleData &od = *oi;
    od.ClearNavNodes();
	}
}