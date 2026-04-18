// OBJExporter1.h: interface for the COBJExporter class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_OBJEXPORTER1_H__3864B611_F9DA_49FA_8B08_5A5EDFCA4712__INCLUDED_)
#define AFX_OBJEXPORTER1_H__3864B611_F9DA_49FA_8B08_5A5EDFCA4712__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000  


#define LIST_MAX_SEARCH 1024

#include <vector>
 
struct CVector3D
{    
	float fX;
	float fY; 
	float fZ;
	bool operator== (const CVector3D& cCompare) 
		{ return (memcmp(&cCompare, this, sizeof(CVector3D)) == 0); };
};

typedef std::vector<CVector3D> VectorVector;
typedef VectorVector::iterator VectorIt;

struct CTexCoord2D
{
	float fU;
	float fV;
	bool operator== (const CTexCoord2D& cCompare) 
		{ return (memcmp(&cCompare, this, sizeof(CTexCoord2D)) == 0); };
};
  
typedef std::vector<CTexCoord2D> TexCoordVector;
typedef TexCoordVector::iterator TexCoordIt;

struct CFace
{
	uint32 iVertexIndices[3];
	uint32 iTexCoordIndices[3];
	bool operator== (const CFace& cCompare) 
		{ return (memcmp(&cCompare, this, sizeof(CFace)) == 0); };
};

typedef std::vector<CFace> FaceVector;
typedef FaceVector::iterator FaceIt;


struct CExpObject
{
	VectorVector m_vVertices;
	TexCoordVector m_vTexCoords;
	FaceVector m_vFaces;
	char m_name[256];
};

typedef std::vector<CExpObject> ExpObjectVector;


class COBJExporter  
{
public:
	COBJExporter();
	virtual ~COBJExporter();

	long GetVertexIdx(CVector3D *pVector)
	{
		if(m_curObject==-1)
			StartNewObject("Temp");
		return GetArrayIdx(& m_objects[m_curObject].m_vVertices, pVector); 
	};
	
	long GetTexCoordIdx(CTexCoord2D *pTexCoord)
	{
		if(m_curObject==-1)
			StartNewObject("Temp");
		return GetArrayIdx(& m_objects[m_curObject].m_vTexCoords, pTexCoord);
	};

	void AddVertex(CVector3D & Vector)
	{
		m_objects[m_curObject].m_vVertices.push_back(Vector);
	}

	void AddTexCoord(CTexCoord2D & TexCoord)
	{
		m_objects[m_curObject].m_vTexCoords.push_back(TexCoord);
	}

	void AddFace(CFace sNewFace)
	{
		if(m_curObject==-1)
			StartNewObject("Temp");
		m_objects[m_curObject].m_vFaces.push_back(sNewFace);
	};

	bool StartNewObject(const char * pName);

	bool WriteOBJ(const char * pszFileName);

	//void Clear();
 
protected:

	ExpObjectVector m_objects;
	int m_curObject;

	char * StripZero(char pszOutputBuffer[32], float fNumber);

	template <class T> unsigned long GetArrayIdx(std::vector<T> *pVector, T *pNewElement)
	{
		//////////////////////////////////////////////////////////////////////
		// Search the passed vector pVector for the occurence of the element
		// pNewElement. Return the index of it, add it if it is not present
		// in the container
		//////////////////////////////////////////////////////////////////////

		unsigned long iCnt = 0;
		long i;

		// Search through the vector
		for (i=pVector->size() - 1; i>=0; i--)
		{
			// We found the element, return the index
			if ((* pVector)[i] == (* pNewElement))
				return i;

			iCnt++;

			// To make the searc time always constant, it will grow to fast with big
			// lists
			if (iCnt > LIST_MAX_SEARCH)
				break;
		}

		// We were unable to find the element, add it and return the new index
		pVector->push_back((* pNewElement));

		return pVector->size() - 1;
	}
};

#endif // !defined(AFX_OBJEXPORTER1_H__3864B611_F9DA_49FA_8B08_5A5EDFCA4712__INCLUDED_)
