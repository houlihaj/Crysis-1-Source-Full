////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Crytek Character Animation source code
//	
//	History:
//	28/09/2004 - Created by Ivo Herzeg <ivo@crytek.de>
//
//  Contains:
//  loading of model and animations
/////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _CRY_MODEL_LOADER_HDR_
#define _CRY_MODEL_LOADER_HDR_

#include "ModelAnimationSet.h"

class CCharacterModel;
class CMesh;

#define LOAD_ON_DEMAND 0x1


// information about an animation to load
struct SAnimFile
{
	string strFileName;
	string strAnimName;
	uint32 nAnimFlags; // combination of GlobalAnimation internal flags

	SAnimFile( const string& fileName, const char* szAnimName, uint32 animflags )
	{
		strFileName	=	fileName;
		strAnimName	=	szAnimName;
		nAnimFlags	=	animflags; 
		CryStringUtils::UnifyFilePath(strFileName);
	}

	SAnimFile() { nAnimFlags=0; }
};

// the animation file array
typedef std::vector<SAnimFile*> TAnimFilesVec;

class CryCHRLoader
{
public:

	CryCHRLoader()
	{
		m_arrAnimFiles.reserve(2500);
		m_arrFacialAnimations.reserve(2500);
	}
	~CryCHRLoader() 
	{	
		clear(); 
	}

	CCharacterModel* LoadNewCHR (const string& strGeomFileName, CharacterManager* pManager, uint32 SurpressWarning,uint32 loadanimation);

	//void VertexCleanup(CCharacterModel* pModel,CModelMesh* pModelMesh, uint32 i, uint32 numExtVertices,DynArray<uint16>& m_arrExtToIntMap, std::vector<ExtSkinVertex>& arrExtVertices);
	//void ChildParentRelation(CCharacterModel* pModel,CModelMesh* pModelMesh, uint32 i, uint32 numExtVertices,DynArray<uint16>& m_arrExtToIntMap, std::vector<ExtSkinVertex>& arrExtVertices);
	void VertexCleanup(CCharacterModel* pModel,CModelMesh* pModelMesh, uint32 i, uint32 numExtVertices,DynArray<uint16>& m_arrExtToIntMap, ExtSkinVertex * arrExtVertices);
	void ChildParentRelation(CCharacterModel* pModel,CModelMesh* pModelMesh, uint32 i, uint32 numExtVertices,DynArray<uint16>& m_arrExtToIntMap, ExtSkinVertex * arrExtVertices);

	// search for animation-path inside CAL-file
	const char* GetFilePath( FILE* fCalFile ); 
	// loads animations for already loaded model
	bool loadAnimations( string m_strCalFileName, CCharacterModel* pModel );
	int32 loadNullAnimation(const char* pFilePath, int nAnimID, const char* pAnimName, uint32 nAnimFlags, CCharacterModel* pModel);
	
	// loads animations for this cgf from the given cal file
	// does NOT close the file (the file belongs to the calling party)
	void ParseCALFile ( string m_strCalFileName, CCharacterModel* pModel );
	void CollectAnimationsInCAL ( string m_strCalFileName, CCharacterModel* pModel );

	// loads the animations from the array
	uint32 loadAnimationArray( CCharacterModel* pModel, TAnimFilesVec& arrAnimFiles, const TAnimFilesVec& arrWildcardAnimFiles);

	// loads the data from the animation event database file (.animevent) - this is usually
	// specified in the CAL file.
	bool loadAnimationEventDatabase( CCharacterModel* pModel, const char* pFileName );

	// cleans up the resources allocated during load
	void clear();


	// the controller manager for the new model; this remains the same during the whole lifecycle
	// the file without extension
	string m_strGeomFileNameNoExt;
	// the name of the cal file

	std::vector<string> m_arrCalFiles;

	TAnimFilesVec m_arrAnimFiles;
	TAnimFilesVec m_arrWildcardAnimFiles;
	CAnimationSet::FacialAnimationSet::container_type m_arrFacialAnimations;

	struct _finddata_t m_fileinfo;


};

#endif
