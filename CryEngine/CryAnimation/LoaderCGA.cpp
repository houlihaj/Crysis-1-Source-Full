////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Crytek Character Animation source code
//	
//	History:
//	20/3/2005 - Created by Ivo Herzeg <ivo@crytek.de>
//
//  Contains:
//  loads and initialises CGA objects 
/////////////////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <CryHeaders.h>

#include "Model.h"
#include "LoaderCGA.h"
#include "LoaderCAF.h"
#include "crc32.h"
#include "AnimEventLoader.h"


#define ANIMATION_EXT "anm"
#define CONT_EXTENSION (0x10)

//uint32 g_nControllerJID=0;

//////////////////////////////////////////////////////////////////////////
// Loads animation object.
//////////////////////////////////////////////////////////////////////////
CCharacterModel* CryCGALoader::LoadNewCGA( const char* OriginalGeomName, CharacterManager* pManager )
{
	//return 0;

	char geomName[256];
	uint32 i=0;
	for (i=0; i<256; i++)
	{
		geomName[i] = OriginalGeomName[i];
		if (geomName[i]==0)
			break;
	}
	if (geomName[i-8]=='_' && geomName[i-7]=='l' && geomName[i-6]=='o' && geomName[i-5]=='w')
	{
		geomName[i-8]='.'; 
		geomName[i-7]='c'; 
		geomName[i-6]='g'; 
		geomName[i-5]='a';
		geomName[i-4]=0;
	}

	CLoaderCAF loader;

	loader.SetLoadOldChunks(Console::GetInst().ca_LoadUncompressedChunks > 0);
	CInternalSkinningInfo * pSkinningInfo = loader.LoadCAF(OriginalGeomName, 0) ;

	if (pSkinningInfo==0)
	{
		g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,	VALIDATOR_FLAG_FILE,OriginalGeomName,	"Failed to load CGA-Object" );
		return 0;
	}

	if (Console::GetInst().ca_AnimWarningLevel > 1 && loader.GetHasOldControllers())
	{
		g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,	VALIDATOR_FLAG_FILE,geomName,	"File has uncompressed data" );
	}

	if (Console::GetInst().ca_AnimWarningLevel > 0 && !loader.GetHasNewControllers())
	{
		g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,	VALIDATOR_FLAG_FILE,geomName,	"File has no compressed data" );
	}

	//CSkinningInfo* pSkinningInfo = pCGF->GetSkinningInfo();
	//if (pSkinningInfo==0) 
	//	return 0;

	CCharacterModel* pCGAModel = new CCharacterModel(OriginalGeomName, pManager, CGA);

	//we use the file-path to calculate a unique codeID for every controller
	uint32 nControllerJID=g_pCrc32Gen->GetCRC32(geomName);
	pCGAModel->m_pModelSkeleton = new CModelSkeleton;

	//-----------------------------------------------------------------------------
	//-----------------------------------------------------------------------------
	//-----------------------------------------------------------------------------

	m_DefaultNodeCount=0;	
	InitNodes(pSkinningInfo,pCGAModel,OriginalGeomName,"Default",1, nControllerJID );
	m_DefaultNodeCount = m_NodeAnims.size();
//	g_pI3DEngine->ReleaseChunkFileContent( pCGF );

	LoadAnimations( geomName,pCGAModel,nControllerJID );

	pCGAModel->SetModelAnimEventDatabase(PathUtil::ReplaceExtension(geomName, "animevents"));
	if (gEnv->pCryPak->IsFileExist(pCGAModel->GetModelAnimEventDatabaseCStr()))
		AnimEventLoader::loadAnimationEventDatabase(pCGAModel, pCGAModel->GetModelAnimEventDatabaseCStr());

	// the first step is for the root bone
	uint32 numJoints = pCGAModel->m_pModelSkeleton->m_arrModelJoints.size();
	for (uint32 i=0; i<numJoints; i++)
	{
		pCGAModel->m_pModelSkeleton->m_arrModelJoints[i].m_DefaultAbsPose = pCGAModel->m_pModelSkeleton->m_arrModelJoints[i].m_DefaultRelPose;
		int32 p=pCGAModel->m_pModelSkeleton->m_arrModelJoints[i].m_idxParent;
		if (p >= 0)
			pCGAModel->m_pModelSkeleton->m_arrModelJoints[i].m_DefaultAbsPose	= pCGAModel->m_pModelSkeleton->m_arrModelJoints[p].m_DefaultAbsPose * pCGAModel->m_pModelSkeleton->m_arrModelJoints[i].m_DefaultAbsPose;

		pCGAModel->m_pModelSkeleton->m_arrModelJoints[i].m_DefaultAbsPoseInverse = pCGAModel->m_pModelSkeleton->m_arrModelJoints[i].m_DefaultAbsPose.GetInverted();

#ifdef _DEBUG
		Vec3 vRoot0=pCGAModel->m_pModelSkeleton->m_arrModelJoints[i].m_DefaultAbsPose.t;
		assert(vRoot0.GetLength()<100.0f);	//usually all animated objects are smaller then 100m
		Vec3 vRoot1=pCGAModel->m_pModelSkeleton->m_arrModelJoints[i].m_DefaultAbsPoseInverse.t;
		assert(vRoot1.GetLength()<100.0f);	//usually all animated objects are smaller then 100m
#endif

	}

	m_CtrlVec3.clear();
	m_CtrlQuat.clear();

	return pCGAModel;
}











//////////////////////////////////////////////////////////////////////////
void CryCGALoader::LoadAnimations( const char *cgaFile, CCharacterModel* pCGAModel, uint32 unique_model_id  )
{
	// Load all filename_***.anm files.
	char filter[_MAX_PATH];
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];

	portable_splitpath( cgaFile,drive,dir,fname,ext );
	strcat( fname,"_*");
	portable_makepath( filter, drive,dir,fname,"anm" );

	char fullpath[_MAX_PATH];
	char filename[_MAX_PATH];
	portable_makepath( fullpath, drive,dir,NULL,NULL );

	ICryPak *pack = g_pISystem->GetIPak();

	// Search files that match filter specification.
	_finddata_t fd;
	int res;
	intptr_t handle;
	if ((handle = pack->FindFirst( filter,&fd )) != -1)
	if (handle != -1)
	{
		do
		{
			// ModelAnimationHeader file found, load it.
			strcpy( filename,fullpath );
			strcat( filename,fd.name );
			LoadAnimationANM( filename, pCGAModel,unique_model_id );
			res = pack->FindNext( handle,&fd );
		} while (res >= 0);
		pack->FindClose(handle);
	}
}


//////////////////////////////////////////////////////////////////////////
bool CryCGALoader::LoadAnimationANM( const char* animFile, CCharacterModel* pCGAModel, uint32 unique_model_id )
{
	// Get file name, this is a name of application.
	char fname[_MAX_PATH];
	strcpy( fname,animFile );
	CryStringUtils::StripFileExtension(fname);
	const char *sAnimName = CryStringUtils::FindFileNameInPath(fname);

	const char *sName = strchr(sAnimName,'_');
	if (sName)
		sName += 1;
	else
		sName = sAnimName;

//------------------------------------------------------------------------------

	CLoaderCAF loader;

	CInternalSkinningInfo * pSkinningInfo		= loader.LoadCAF(animFile, 0) ;;//g_pI3DEngine->LoadChunkFileContent( animFile );
	if (pSkinningInfo==0)
	{
		g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,	VALIDATOR_FLAG_FILE,animFile,	"Failed to load ANM-file: %s", animFile );
		return 0;
	}

	InitNodes(pSkinningInfo,pCGAModel,animFile,sName, 0, unique_model_id );

	return true;
}








//////////////////////////////////////////////////////////////////////////
void CryCGALoader::InitNodes( CInternalSkinningInfo* pSkinningInfo, CCharacterModel* pCGAModel, const char* animFile, const string& strAnimationName, bool bMakeNodes, uint32 unique_model_id )
{

	//-------------------------------------------------------------------------
	//----------        copy animation timing-values            ---------------
	//-------------------------------------------------------------------------
	m_NodeAnims.clear();
	m_ticksPerFrame			=	TICKS_PER_FRAME;	//pSkinningInfo->m_nTicksPerFrame;
	m_secsPerTick				= SECONDS_PER_TICK;	//pSkinningInfo->m_secsPerTick;
	m_start							= pSkinningInfo->m_nStart;
	m_end								= pSkinningInfo->m_nEnd;
	assert(m_ticksPerFrame==TICKS_PER_FRAME);

	m_LoadCurrAnimation.SetAnimName(strAnimationName);//m_strAnimName = strAnimationName;

	//-------------------------------------------------------------------------
	//----------             copy animation tracks              ---------------
	//-------------------------------------------------------------------------
	m_CtrlVec3.clear();
	uint32 numTracksVec3 = pSkinningInfo->m_TrackVec3.size();
	m_CtrlVec3.reserve(numTracksVec3);
	for (uint32 t=0; t<numTracksVec3; t++)
	{
		CControllerTCBVec3 Track;
		uint32 nkeys = pSkinningInfo->m_TrackVec3[t]->size();
		Track.resize(nkeys);
		for (uint32 i=0; i<nkeys; i++)
		{
			Track.key(i).flags			= 0;

			f32 Qtime= (f32)pSkinningInfo->m_TrackVec3[t]->operator[](i).time;

			Track.key(i).time			= (f32)pSkinningInfo->m_TrackVec3[t]->operator[](i).time / TICKS_CONVERT;
			Track.key(i).value			= pSkinningInfo->m_TrackVec3[t]->operator[](i).val;
			Track.key(i).tens			= pSkinningInfo->m_TrackVec3[t]->operator[](i).t;
			Track.key(i).cont			= pSkinningInfo->m_TrackVec3[t]->operator[](i).c;
			Track.key(i).bias			= pSkinningInfo->m_TrackVec3[t]->operator[](i).b;
			Track.key(i).easefrom	= pSkinningInfo->m_TrackVec3[t]->operator[](i).eout;
			Track.key(i).easeto		= pSkinningInfo->m_TrackVec3[t]->operator[](i).ein;
		}

		if (pSkinningInfo->m_TrackVec3Flags[t].f0)
			Track.ORT( spline::TCBSpline<Vec3>::ORT_CYCLE );
		else if (pSkinningInfo->m_TrackVec3Flags[t].f1)
			Track.ORT( spline::TCBSpline<Vec3>::ORT_LOOP );
		Track.comp_deriv();// Precompute spline tangents.
		m_CtrlVec3.push_back(Track);
	}

	m_CtrlQuat.clear();
	uint32 numTracksQuat = pSkinningInfo->m_TrackQuat.size();

	m_CtrlQuat.reserve(numTracksQuat);
	for (uint32 t=0; t<numTracksQuat; t++)
	{
		spline::TCBAngleAxisSpline Track;
		uint32 nkeys = pSkinningInfo->m_TrackQuat[t]->size();
		Track.resize(nkeys);
		for (uint32 i=0; i<nkeys; i++)
		{
			Track.key(i).flags			= 0;
			Track.key(i).time			= (float)pSkinningInfo->m_TrackQuat[t]->operator[](i).time / TICKS_CONVERT;// * secsPerTick;
			Track.key(i).angle			= pSkinningInfo->m_TrackQuat[t]->operator[](i).val.w;	//TCBAngAxisSpline stores relative rotation angle-axis.
			Track.key(i).axis			= pSkinningInfo->m_TrackQuat[t]->operator[](i).val.v;	//@FIXME rotation direction somehow differ from Max.
			Track.key(i).tens			= pSkinningInfo->m_TrackQuat[t]->operator[](i).t;
			Track.key(i).cont			= pSkinningInfo->m_TrackQuat[t]->operator[](i).c;
			Track.key(i).bias			= pSkinningInfo->m_TrackQuat[t]->operator[](i).b;
			Track.key(i).easefrom	= pSkinningInfo->m_TrackQuat[t]->operator[](i).eout;
			Track.key(i).easeto		= pSkinningInfo->m_TrackQuat[t]->operator[](i).ein;
		}

		if (pSkinningInfo->m_TrackQuatFlags[t].f0)
			Track.ORT( spline::TCBAngleAxisSpline::ORT_CYCLE );
		else if (pSkinningInfo->m_TrackQuatFlags[t].f1)
			Track.ORT( spline::TCBAngleAxisSpline::ORT_LOOP );
		Track.comp_deriv();// Precompute spline tangents.
		m_CtrlQuat.push_back(Track);
	}

	m_arrControllers=pSkinningInfo->m_arrControllers;



	//-------------------------------------------------------------------------
	//-------------------------------------------------------------------------
	//-------------------------------------------------------------------------


	CContentCGF* pCGF = g_pI3DEngine->LoadChunkFileContent( animFile );

	if (pCGF == 0)
	{
		//error
		return;
	}



	uint32 numChunks = pCGF->GetSkinningInfo()->m_numChunks;// m;// m_arrControllers.size();
	m_arrChunkNodes.resize(numChunks*CONT_EXTENSION);
	for (uint32 i=0; i<numChunks*CONT_EXTENSION; i++) 
		m_arrChunkNodes[i].active=0;

	IStatObj* pRootStaticObj = g_pI3DEngine->LoadStatObj( pCGAModel->m_strFilePath.c_str() );

	pCGAModel->pCGA_Object = pRootStaticObj;

	uint32 numNodes2 = pCGF->GetNodeCount();
	assert(numNodes2);

	uint32 MeshNodeCounter=0;
	for(uint32 n=0; n<numNodes2; n++) 
	{
		if (pCGF->GetNode(n)->type==CNodeCGF::NODE_MESH)	
			MeshNodeCounter+=(pCGF->GetNode(n)!=0);
	}

	CNodeCGF* pGFXNode=0;
	//uint32 nodecounter=0;
	for(uint32 n=0; n<numNodes2; n++) 
	{
		uint32 MeshNode   = pCGF->GetNode(n)->type==CNodeCGF::NODE_MESH;	
		uint32 HelperNode = pCGF->GetNode(n)->type==CNodeCGF::NODE_HELPER;	
		if (MeshNode || HelperNode)	
		{	
			pGFXNode = pCGF->GetNode(n); 
			assert(pGFXNode);

			// Try to create object.
			IStatObj::SSubObject *pSubObject = NULL;

			NodeDesc nd;
			nd.active				= 1;
			nd.parentID			= pGFXNode->nParentChunkId;
			nd.pos_cont_id	= pGFXNode->pos_cont_id;
			nd.rot_cont_id	= pGFXNode->rot_cont_id;
			nd.scl_cont_id	= pGFXNode->scl_cont_id;

			int numChunks = (int)m_arrChunkNodes.size();

			if (nd.pos_cont_id!=0xffff)
				assert( nd.pos_cont_id < numChunks );
			if (nd.rot_cont_id!=0xffff)
				assert( nd.rot_cont_id < numChunks );
			if (nd.scl_cont_id!=0xffff)
				assert( nd.scl_cont_id < numChunks );

			assert(pGFXNode->nChunkId<(int)numChunks);

			pCGAModel->m_ModelAABB=AABB( Vec3(-2,-2,-2),Vec3(+2,+2,+2) );

			if (bMakeNodes)
			{
				IStatObj::SSubObject *pSubObj = pRootStaticObj->FindSubObject( pGFXNode->name );

				if (pSubObj==0 && MeshNodeCounter!=1)
					continue;

				IStatObj* pStaticObj = 0;
				if (MeshNodeCounter==1 && MeshNode) 
					pStaticObj=pRootStaticObj;
				else
					pStaticObj = pSubObj->pStatObj;

				nd.node_idx				= pCGAModel->m_pModelSkeleton->m_arrModelJoints.size();

				uint16 ParentID = 0xffff;  
				if (pGFXNode->nParentChunkId != 0xffffffff)
				{
					assert(pGFXNode->nParentChunkId<(int)numChunks);
					uint32 numJoints = pCGAModel->m_pModelSkeleton->m_arrModelJoints.size();
					for (uint32 i=0; i<numJoints; i++)
						if (pGFXNode->nParentChunkId==pCGAModel->m_pModelSkeleton->m_arrModelJoints[i].m_numLevels)	ParentID=i;
				}

				CModelJoint mj;
				mj.m_nJointCRC32		= unique_model_id+n;
				mj.m_numLevels			= pGFXNode->nChunkId;  //NOTE:: this is a place-holder to store the chunk-id
				mj.m_idxParent			=	ParentID;
				mj.m_CGAObject			= pStaticObj;
				mj.SetJointName(pGFXNode->name);
				mj.m_NodeID					=	nd.node_idx;
				mj.m_DefaultRelPose	= QuatT( !pGFXNode->rot, pGFXNode->pos );
				pCGAModel->m_pModelSkeleton->m_arrModelJoints.push_back(mj);

				m_arrChunkNodes[pGFXNode->nChunkId] = nd;
		//		assert(nodecounter==n);
		//		nodecounter++;
			//	g_nControllerJID++;
			}
			else
			{
				uint32 numJoints = pCGAModel->m_pModelSkeleton->m_arrModelJoints.size();
				for (uint32 i=0; i<numJoints; i++)
				{
					if (stricmp(pCGAModel->m_pModelSkeleton->m_arrModelJoints[i].GetJointName(), pGFXNode->name) == 0)
					{
						nd.node_idx = i;
						break;
					}
				}
				m_arrChunkNodes[pGFXNode->nChunkId] = nd;
			}
		}
	}

	//------------------------------------------------------------------------
	//---    init nodes                                                    ---
	//------------------------------------------------------------------------
	uint32 numControllers0 = m_CtrlVec3.size();
	uint32 numControllers1 = m_CtrlQuat.size();

	uint32 numAktiveNodes = 0;
	uint32 numNodes = m_arrChunkNodes.size();
	for (uint32 i=0; i<numNodes; i++)
		numAktiveNodes+=m_arrChunkNodes[i].active;

	m_NodeAnims.clear();
	m_NodeAnims.resize(numAktiveNodes);

	if (numAktiveNodes<m_DefaultNodeCount)
		numAktiveNodes=m_DefaultNodeCount;

	m_NodeAnims.resize(numAktiveNodes);


	for (uint32 i=0; i<numNodes; i++)
	{
		NodeDesc nd = m_arrChunkNodes[i];

		if (nd.active==0) 
			continue;

		if (nd.node_idx==0xffff)
			continue;

		uint32 numAnims = m_NodeAnims.size();

		uint32 id = pCGAModel->m_pModelSkeleton->m_arrModelJoints[nd.node_idx].m_NodeID;
		if ( id<0 || id>=numAnims)
			continue;


		// find controllers.
		if (nd.pos_cont_id!=0xffff)
		{
			m_NodeAnims[id].m_active.p=0;
			CControllerType pTCB = m_arrControllers[nd.pos_cont_id];
			if (pTCB.type==0x55)
			{
				m_NodeAnims[id].m_active.p=1;
				m_NodeAnims[id].m_posTrack = m_CtrlVec3[pTCB.index];
			}
		}

		if (nd.rot_cont_id!=0xffff)
		{
			m_NodeAnims[id].m_active.o=0;
			CControllerType pTCB = m_arrControllers[nd.rot_cont_id];
			if (pTCB.type==0xaa)
			{
				m_NodeAnims[id].m_active.o=1;
				m_NodeAnims[id].m_rotTrack = m_CtrlQuat[pTCB.index];
			}
		}

		if (nd.scl_cont_id!=0xffff)
		{
			m_NodeAnims[id].m_active.s=0;
			CControllerType pTCB = m_arrControllers[nd.scl_cont_id];
			if (pTCB.type==0x55)
			{
				m_NodeAnims[id].m_active.s=1;
				m_NodeAnims[id].m_sclTrack = m_CtrlVec3[pTCB.index];
			}
		}
	}

//-------------------------------------------------------------------------

	if (!m_CtrlVec3.empty() || !m_CtrlQuat.empty() )
		LoadANM( pCGAModel, animFile, strAnimationName, m_NodeAnims, unique_model_id );

	uint32 numJoints = pCGAModel->m_pModelSkeleton->m_arrModelJoints.size();
	for (uint32 a=0; a<numJoints; a++)
		pCGAModel->m_pModelSkeleton->m_arrModelJoints[a].m_numChildren=0;
	for (uint32 a=0; a<numJoints; a++)
	{
		for (uint32 b=0; b<numJoints; b++)
		{
			if (a==pCGAModel->m_pModelSkeleton->m_arrModelJoints[b].m_idxParent)
				pCGAModel->m_pModelSkeleton->m_arrModelJoints[a].m_numChildren++;
		}	
	}


	g_pI3DEngine->ReleaseChunkFileContent(pCGF);

}






// loads the animations from the array: pre-allocates the necessary controller arrays
// the 0th animation is the default animation
uint32 CryCGALoader::LoadANM ( CCharacterModel* pModel,const char* pFilePath, const char* pAnimName, std::vector<CControllerTCB>& m_LoadCurrAnimation, uint32 unique_model_id  )
{
	uint32 nAnimID = 0;

	{
		AUTO_PROFILE_SECTION(g_dTimeAnimLoadBindPreallocate);
		pModel->m_AnimationSet.prepareLoadCAFs ( 1 );
	}

	int32 rel = pModel->m_AnimationSet.LoadANM( pFilePath, 0.01f, nAnimID, pAnimName, 0, m_LoadCurrAnimation, this, unique_model_id  );
	if (rel >= 0)
		nAnimID++;
	else
		AnimFileWarning(pModel->GetModelFilePath(),"Animation could not be read" );

	return nAnimID;
}
