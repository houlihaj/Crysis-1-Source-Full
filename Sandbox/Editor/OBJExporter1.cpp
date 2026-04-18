// OBJExporter1.cpp: implementation of the COBJExporter class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "OBJExporter1.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

COBJExporter::COBJExporter()
{
	m_curObject = -1;
}

COBJExporter::~COBJExporter()
{

}


bool COBJExporter::StartNewObject(const char * pName)
{
	CExpObject newObject;
	strcpy(newObject.m_name, pName);
	m_objects.push_back(newObject);
	m_curObject++;
	return true;
}



bool COBJExporter::WriteOBJ(const char * pszFileName)
{
	//////////////////////////////////////////////////////////////////////
	// Write the OBJ file to disk
	//////////////////////////////////////////////////////////////////////

	FILE *hFile = NULL;
	VectorIt itVec;
	FaceIt itFace;
	TexCoordIt itTexCoord;
	CString szMTL;
	char ppszBuffer[3][32];

	ASSERT(pszFileName);

	CLogFile::FormatLine("Exporting OBJ file to '%s'", pszFileName);

	// Open the file
	hFile = fopen(pszFileName, "w");

	if (!hFile)
	{
		CLogFile::FormatLine("Error while opening file '%s' !", pszFileName);
		ASSERT(hFile);
		return false;
	}

	// Write header
	fprintf(hFile, "# Object file exported by CryEdit\n");
	fprintf(hFile, "# Attention: while import to 3DS Max Unify checkbox for normals must be unchecked.\n");
	fprintf(hFile, "#\n");

	// Create MTL library filename
	szMTL = pszFileName;
	szMTL = Path::ReplaceExtension( szMTL,"mtl" );
	szMTL = Path::GetFile(szMTL);

	// Write material library import statement
	fprintf(hFile, "mtllib %s\n", (const char*)szMTL);
	fprintf(hFile, "#\n");

	int totalVertexNumber = 0;
	int totalTexCoordNumber = 0;

	for(int i=0; i<=m_curObject; i++)
	{
		fprintf(hFile, "g\n");
		fprintf(hFile, "# object %s cumming...\n", m_objects[i].m_name);
		fprintf(hFile, "#\n");

		// Write all vertices
		for (itVec=m_objects[i].m_vVertices.begin(); itVec!=m_objects[i].m_vVertices.end(); itVec++)
		{
			fprintf(hFile, "v  %s %s %s\n", 
				StripZero(ppszBuffer[0], (* itVec).fX),
				StripZero(ppszBuffer[1], (* itVec).fY), 
				StripZero(ppszBuffer[2], (* itVec).fZ));
		}
		fprintf(hFile, "# %i vertices\n\n", m_objects[i].m_vVertices.size());

		// Write all texture coordinates
		for (itTexCoord=m_objects[i].m_vTexCoords.begin(); itTexCoord!=m_objects[i].m_vTexCoords.end(); itTexCoord++)
		{
			fprintf(hFile, "vt  %s %s 0\n",
				StripZero(ppszBuffer[0], (* itTexCoord).fU),
				StripZero(ppszBuffer[1], (* itTexCoord).fV));
		}
		fprintf(hFile, "# %i texture vertices\n\n", m_objects[i].m_vTexCoords.size());

		// Write material usage statement
		fprintf(hFile, "g %s\n", m_objects[i].m_name);
		fprintf(hFile, "usemtl Terrain\n");
		fprintf(hFile, "s 0\n");

		// Write all faces, convert the indices to one based indices
		for (itFace=m_objects[i].m_vFaces.begin(); itFace!=m_objects[i].m_vFaces.end(); itFace++)
		{
			fprintf(hFile, "f %i/%i %i/%i %i/%i\n",
				(* itFace).iVertexIndices[0] + totalVertexNumber + 1, (* itFace).iTexCoordIndices[0] + totalTexCoordNumber + 1,
				(* itFace).iVertexIndices[1] + totalVertexNumber + 1, (* itFace).iTexCoordIndices[1] + totalTexCoordNumber + 1,
				(* itFace).iVertexIndices[2] + totalVertexNumber + 1, (* itFace).iTexCoordIndices[2] + totalTexCoordNumber + 1);
			/*
			fprintf(hFile, "f %i/%i %i/%i %i/%i\n",
				(* itFace).iVertexIndices[0] -m_objects[i].m_vVertices.size(), (* itFace).iTexCoordIndices[0] -m_objects[i].m_vTexCoords.size(),
				(* itFace).iVertexIndices[1] -m_objects[i].m_vVertices.size(), (* itFace).iTexCoordIndices[1] -m_objects[i].m_vTexCoords.size(),
				(* itFace).iVertexIndices[2] -m_objects[i].m_vVertices.size(), (* itFace).iTexCoordIndices[2] -m_objects[i].m_vTexCoords.size());
				*/
		}
		fprintf(hFile, "# %i faces\n\n", m_objects[i].m_vFaces.size());

		totalVertexNumber += m_objects[i].m_vVertices.size();
		totalTexCoordNumber += m_objects[i].m_vTexCoords.size();
	}

	fprintf(hFile, "g\n");
	fclose(hFile);

	// Create MTL library filename
	szMTL = Path::ReplaceExtension( pszFileName,"mtl" );


	// Open the material file
	hFile = fopen(szMTL, "w");

	if (!hFile)
	{
		CLogFile::FormatLine("Error while opening file '%s' !", (const char*)szMTL);
		ASSERT(hFile);
		return false;
	}

	// Write header
	fprintf(hFile, "# Material file exported by CryEdit\n\n");

	// Write terrain material
	fprintf(hFile, "newmtl Terrain\n");
	fprintf(hFile, "Ka 1.000000 1.000000 1.000000\n");
	fprintf(hFile, "Kd 1.000000 1.000000 1.000000\n");
	fprintf(hFile, "Ks 1.000000 1.000000 1.000000\n");
	fprintf(hFile, "Ns 0.000000\n");
	fprintf(hFile, "map_Kd Terrain.bmp");

	fclose(hFile);
	
	return true;
}

char * COBJExporter::StripZero(char pszOutputBuffer[32], float fNumber)
{
	////////////////////////////////////////////////////////////////////////
	// Convert a float into a string representation and remove all 
	// uneccessary zeroes and the decimal dot if it is not needed
	////////////////////////////////////////////////////////////////////////

	long i;

	sprintf(pszOutputBuffer, "%f", fNumber);

	for (i=strlen(pszOutputBuffer) - 1; i>=0; i--)
	{
		if (pszOutputBuffer[i] == '0')
			pszOutputBuffer[i] = '\0';
		else if (pszOutputBuffer[i] == '.')
		{
			pszOutputBuffer[i] = '\0';
			break;
		}
		else
			break;
	}

	return pszOutputBuffer;
}