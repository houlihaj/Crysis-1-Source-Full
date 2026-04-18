////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Crytek Character Animation source code
//	
//	History:
//	10/9/2004 - Created by Ivo Herzeg <ivo@crytek.de>
//
//  Contains:
//  interface class to all motions  
/////////////////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CharacterManager.h"
#include "Model.h"
#include "ModelAnimationSet.h"
#include "ModelMesh.h"
#include "crc32.h"
#include "ControllerPQ.h"

CAnimationSet::CAnimationSet()
{
	m_pModel=0;
	m_CharEditMode=0;
}

CAnimationSet::~CAnimationSet()
{
	// now there are no controllers referred to by this object, we can release the animations
	for (std::vector<ModelAnimationHeader>::iterator it = m_arrAnimations.begin(); it != m_arrAnimations.end(); ++it)
		g_AnimationManager.AnimationRelease(it->m_nGlobalAnimId, this);

	g_AnimationManager.Unregister(this);
}

void CAnimationSet::Init()
{
	g_AnimationManager.Register(this);
}

size_t CAnimationSet::SizeOfThis() const
{
	size_t nSize = m_arrAnimations.size();
	for (uint32 i =0; i < m_arrAnimations.size(); ++i)
		nSize += m_arrAnimations[i].SizeOfThis();

	// somebody know how to get size of multimap?
	nSize += m_FullPathAnimationsMap.size() * 4* sizeof(uint32) + m_AnimationHashMap.GetSize();

//	nSize += m_arrAnimByGlobalId.size() * sizeof(LocalAnimId);
	nSize += m_arrStandupAnims.size() * sizeof(int);

	for (FacialAnimationSet::const_iterator it = m_facialAnimations.begin(), end = m_facialAnimations.end(); 
		it != end; ++it) {
			nSize += it->name.capacity() + it->path.capacity();
	}

	//for (size_t i = 0; i < m_facialAnimations.size(); ++i) {
	//	nSize += m_facialAnimations[i].name.capacity() + m_facialAnimations[i].path.capacity();
	//}

	return nSize;
}


int CAnimationSet::ReloadCAF(const char * szFileName, bool bFullPath)
{
	int nModelAnimId;// = GetIDByName(szFileName);

	if (bFullPath)
	{

		uint32 crc = g_pCrc32Gen->GetCRC32Lowercase(szFileName);

		std::pair<TCRCmap::iterator, TCRCmap::iterator> p = m_FullPathAnimationsMap.equal_range(crc);
		TCRCmap::iterator it = p.first;//*m_FullPathAnimationsMap.find(crc);*/
		TCRCmap::iterator end = p.second; //m_FullPathAnimationsMap.end();

		int res = -1;
		for (; it != end; ++it)
		{
			res = ReloadCAF(it->second);
		}

		return res;
	}

	nModelAnimId = GetAnimIDByName(szFileName);

	if (nModelAnimId == -1)
		return -1;

	return ReloadCAF(nModelAnimId);
}

//TODO:
void CAnimationSet::PreloadCAF(int nGlobalAnimID)
{

}


int CAnimationSet::ReloadCAF(int nGlobalAnimID)
{
	g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].ClearControllers();//m_arrController.clear();

	g_AnimationManager.LoadAnimation(nGlobalAnimID, eLoadFullData);

	//uint32 numClients = g_AnimationManager.m_arrClients.size();
	//for (uint32 i=0; i<numClients; i++)
	OnAnimationGlobalLoad(nGlobalAnimID);


	return nGlobalAnimID;
}

int CAnimationSet::UnloadCAF(int nGlobalAnimID)
{
	OnAnimationGlobalUnLoad(nGlobalAnimID);
	g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].ClearControllers();//m_arrController.clear();
	g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].ClearAssetLoaded();

	// TODO. Error checking
	return 1;
}


int CAnimationSet::RenewCAF(const char * szFileName, bool bFullPath)
{
	int nModelAnimId;// = GetIDByName(szFileName);

	if (bFullPath)
	{
		uint32 crc = g_pCrc32Gen->GetCRC32Lowercase(szFileName);
		std::pair<TCRCmap::iterator, TCRCmap::iterator> p = m_FullPathAnimationsMap.equal_range(crc);
		TCRCmap::iterator it = p.first;//*m_FullPathAnimationsMap.find(crc);*/
		TCRCmap::iterator end = p.second; //m_FullPathAnimationsMap.end();

		int res = -1;
		for (; it != end; ++it)
		{
			res = RenewCAF(it->second);
		}

		return res;
	}

	nModelAnimId = GetAnimIDByName(szFileName);

	if (nModelAnimId == -1)
		return -1;

	return RenewCAF(nModelAnimId);

}


int CAnimationSet::RenewCAF(int nGlobalAnimID)
{
	//uint32 numClients = g_AnimationManager.m_arrClients.size();
	//for (uint32 i=0; i<numClients; i++)
	OnAnimationGlobalLoad(nGlobalAnimID);

	return nGlobalAnimID;
}

const char* CAnimationSet::GetFacialAnimationPathByName(const char* szName)
{
	FacialAnimationSet::iterator itFacialAnim = std::lower_bound(m_facialAnimations.begin(), m_facialAnimations.end(), szName, stl::less_stricmp<const char*>());
	if (itFacialAnim != m_facialAnimations.end() && stl::less_stricmp<const char*>()(szName, *itFacialAnim))
		itFacialAnim = m_facialAnimations.end();
	const char* szPath = (itFacialAnim != m_facialAnimations.end() ? (*itFacialAnim).path.c_str() : 0);
	return szPath;
}

int CAnimationSet::GetNumFacialAnimations()
{
	return m_facialAnimations.size();
}

const char* CAnimationSet::GetFacialAnimationName(int index)
{
	if (index < 0 || index >= (int)m_facialAnimations.size())
		return 0;
	return m_facialAnimations[index].name.c_str();
}

const char* CAnimationSet::GetFacialAnimationPath(int index)
{
	if (index < 0 || index >= (int)m_facialAnimations.size())
		return 0;
	return m_facialAnimations[index].path.c_str();
}

//////////////////////////////////////////////////////////////////////////
// Loads animation file. Returns the global anim id of the file, or -1 if error
// SIDE EFFECT NOTES:
//  THis function does not put up a warning in the case the animation couldn't be loaded.
//  It returns an error (false) and the caller must process it.
int CAnimationSet::LoadCAF(const char * szFilePath, float fScale, int nAnimID, const char * szAnimName, unsigned nGlobalAnimFlags)
{
	int nModelAnimId = GetAnimIDByName (szAnimName);
	if (nModelAnimId == -1) 
	{
		int nGlobalAnimID = g_AnimationManager.CreateGlobalMotionHeader (szFilePath);

		int nLocalAnimId = m_arrAnimations.size();
		ModelAnimationHeader localAnim; 
		localAnim.m_nGlobalAnimId = nGlobalAnimID;
		localAnim.SetAnimName(szAnimName);

		m_arrAnimations.push_back(localAnim);
//		m_arrAnimByGlobalId.insert (std::lower_bound(m_arrAnimByGlobalId.begin(), m_arrAnimByGlobalId.end(), nGlobalAnimID, AnimationGlobIdPred(m_arrAnimations)), LocalAnimId(nLocalAnimId));
		m_AnimationHashMap.InsertValue(&localAnim, nLocalAnimId);

		string path = szFilePath;

		path.replace('\\','/' );

		int p = path.find("//");
		while (p != string::npos)
		{
			path.erase(p,1 );
			p = path.find("//");
		}

		//path.replace('\\',"\\\\");
		path.MakeLower();

		uint32 crc = g_pCrc32Gen->GetCRC32Lowercase(path.c_str());
		m_FullPathAnimationsMap.insert(std::make_pair<uint32, int32>(crc,nGlobalAnimID));

		selfValidate();

		g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].AddRef();

		uint32 loaded = g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].IsAssetLoaded();
		if (loaded)
		{
			assert(g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].m_arrController.size());
			OnAnimationGlobalLoad(nGlobalAnimID);
		}
		else
		{
			assert(g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].m_arrController.size()==0);

			// if nGlobalAnimFlags == LOAD_ON_DEMAND  we just 
			if (!nGlobalAnimFlags)
			{
				g_AnimationManager.LoadAnimation(nGlobalAnimID, eLoadFullData);

				// If the animation was null, then we must add dummy controllers here.
				bool IsNull = (strncmp(szFilePath, NULL_ANIM_FILE, strlen(NULL_ANIM_FILE)) == 0);
				if (IsNull)
					AddNullControllers(nGlobalAnimID);

				//uint32 numClients = g_AnimationManager.m_arrClients.size();
				//for (uint32 i=0; i<numClients; i++)
				OnAnimationGlobalLoad(nGlobalAnimID);
			}
			else
			{
				// Set on-demand status
				//g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].OnA
				g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].OnAssetOnDemand();
				g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].ClearAssetLoaded();
				//if (g_pISystem->IsEditor())
				{
					g_AnimationManager.LoadAnimation(nGlobalAnimID, eLoadOnlyInfo);
					UnloadCAF(nGlobalAnimID);
				}

			}
		}
		if (!_strnicmp(szAnimName,"standup",7))
		{
			const char *ptr;
			char buf[256];
			int i;
			for(ptr=szAnimName+8; *ptr && _strnicmp(ptr,"back",4) && _strnicmp(ptr,"stomach",7) && _strnicmp(ptr,"side",4); ptr++);
			strncpy(buf, szAnimName+8, ptr-szAnimName-8); buf[ptr-szAnimName-8]=0;
			for(i=0; i<(int)m_arrStandupAnimTypes.size() && _stricmp(m_arrStandupAnimTypes[i],buf)<0; i++);
			if (i>=(int)m_arrStandupAnimTypes.size() || _stricmp(m_arrStandupAnimTypes[i],buf)) 
			{
				m_arrStandupAnimTypes.insert(m_arrStandupAnimTypes.begin()+i, buf);
				m_arrStandupAnims.insert(m_arrStandupAnims.begin()+i, std::vector<int>());
			}
			m_arrStandupAnims[i].push_back(nLocalAnimId);
		}

		return nGlobalAnimID;
	}
	else
	{
		int nGlobalAnimID = m_arrAnimations[nModelAnimId].m_nGlobalAnimId;
		g_pILog->LogError("CryAnimation:: Trying to load animation with alias \"%s\" from file \"%s\" into the animation container. Such animation alias already exists and uses file \"%s\". Please use another animation alias.", szAnimName, szFilePath, g_AnimationManager.GetGlobalAnimationHeader(nGlobalAnimID).GetPathName());
		return nGlobalAnimID;
	}
}




//////////////////////////////////////////////////////////////////////////
// Loads animation file. Returns the global anim id of the file, or -1 if error
// SIDE EFFECT NOTES:
//  THis function does not put up a warning in the case the animation couldn't be loaded.
//  It returns an error (false) and the caller must process it.
int CAnimationSet::LoadANM(const char * szFileName, float fScale, int nAnimID, const char * szAnimName, unsigned qqqqqnGlobalAnimFlags, std::vector<CControllerTCB>& m_LoadCurrAnimation, CryCGALoader* pCGA, uint32 unique_model_id )
{
	int nModelAnimId = GetAnimIDByName (szAnimName);
	if (nModelAnimId == -1) 
	{
		int nGlobalAnimID = g_AnimationManager.CreateGlobalMotionHeader (szFileName);

		int nLocalAnimId = m_arrAnimations.size();
		ModelAnimationHeader LocalAnim; 
		LocalAnim.m_nGlobalAnimId = nGlobalAnimID;
		LocalAnim.SetAnimName(szAnimName);
		m_arrAnimations.push_back(LocalAnim);

		//m_arrAnimByGlobalId.insert (std::lower_bound(m_arrAnimByGlobalId.begin(), m_arrAnimByGlobalId.end(), nGlobalAnimID, AnimationGlobIdPred(m_arrAnimations)), LocalAnimId(nLocalAnimId));
		m_AnimationHashMap.InsertValue(&LocalAnim, nLocalAnimId);
		//		m_arrAnimByLocalName.insert (std::lower_bound(m_arrAnimByLocalName.begin(), m_arrAnimByLocalName.end(), szAnimName, AnimationNamePred(m_arrAnimations)), nLocalAnimId);
		selfValidate();

		g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].AddRef();

		uint32 loaded = g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].IsAssetLoaded();
		if (loaded)
		{
			assert(g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].m_arrController.size());
			OnAnimationGlobalLoad(nGlobalAnimID);
		}
		else
		{
			assert(g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].m_arrController.size()==0);
			g_AnimationManager.LoadAnimationTCB(nGlobalAnimID, m_LoadCurrAnimation, pCGA, unique_model_id );
			//uint32 numClients = g_AnimationManager.m_arrClients.size();
			//for (uint32 i=0; i<numClients; i++)
			OnAnimationGlobalLoad(nGlobalAnimID);
		}

		return nGlobalAnimID;
	}
	else
	{
		int nGlobalAnimID = m_arrAnimations[nModelAnimId].m_nGlobalAnimId;
		g_pILog->LogError("CryAnimation:: Trying to load animation with alias \"%s\" from file \"%s\" into the animation container. Such animation alias already exists and uses file \"%s\". Please use another animation alias.",szAnimName,	szFileName,	g_AnimationManager.GetGlobalAnimationHeader(nGlobalAnimID).GetPathName());
		return nGlobalAnimID;
	}
}



void CAnimationSet::OnAnimationGlobalLoad (int nGlobalAnimId)
{
	//std::vector<LocalAnimId>::iterator it = std::lower_bound (m_arrAnimByGlobalId.begin(), m_arrAnimByGlobalId.end(), nGlobalAnimId, AnimationGlobIdPred(m_arrAnimations));
	//assert (it == m_arrAnimByGlobalId.begin() || m_arrAnimations[(it-1)->nAnimId].m_nGlobalAnimId < nGlobalAnimId);

	//// scan through the sequence of animations with the given Global AnimId and bind each of them to the bones
	//if (it != m_arrAnimByGlobalId.end() && m_arrAnimations[it->nAnimId].m_nGlobalAnimId == nGlobalAnimId)
	//{
	//	int32 nAnimID = it->nAnimId;
	//	GlobalAnimationHeader& GlobalAnim = g_AnimationManager.GetGlobalAnimationHeader (nGlobalAnimId);
	//	do 
	//	{
	//		uint32 numConts=GlobalAnim.m_arrController.size();
	//		//if ( numConts )
	//		//{
	//		uint32 numBones = m_pModel->m_pModelSkeleton->m_arrModelJoints.size();
	//		for (uint32 b=0; b<numBones; b++)
	//		{
	//			// this is an autopointer array, so there's no need to re-initialize the autopointers after construction
	//			int32 numControllers = (int)m_pModel->m_pModelSkeleton->m_arrModelJoints[b].m_arrControllersMJoint.size();
	//			if (numControllers <= nAnimID)
	//				m_pModel->m_pModelSkeleton->m_arrModelJoints[b].m_arrControllersMJoint.resize(nAnimID+1); 
	//			uint32 CRC32 = m_pModel->m_pModelSkeleton->m_arrModelJoints[b].m_nJointCRC32;
	//			IController* pIController=GlobalAnim.GetControllerByJointID(CRC32);
	//			m_pModel->m_pModelSkeleton->m_arrModelJoints[b].m_arrControllersMJoint[nAnimID] = pIController;
	//		}
	//		//}
	//	} while(++it != m_arrAnimByGlobalId.end() && m_arrAnimations[it->nAnimId].m_nGlobalAnimId == nGlobalAnimId);
	//}
}


void CAnimationSet::OnAnimationGlobalUnLoad (int nGlobalAnimId)
{
	//std::vector<LocalAnimId>::iterator it = std::lower_bound (m_arrAnimByGlobalId.begin(), m_arrAnimByGlobalId.end(), nGlobalAnimId, AnimationGlobIdPred(m_arrAnimations));
	//assert (it == m_arrAnimByGlobalId.begin() || m_arrAnimations[(it-1)->nAnimId].m_nGlobalAnimId < nGlobalAnimId);
	//// scan through the sequence of animations with the given Global AnimId and bind each of them to the bones
	//if (it != m_arrAnimByGlobalId.end() && m_arrAnimations[it->nAnimId].m_nGlobalAnimId == nGlobalAnimId)
	//{
	//	int32 nAnimID = it->nAnimId;
	//	GlobalAnimationHeader& GlobalAnim = g_AnimationManager.GetGlobalAnimationHeader (nGlobalAnimId);
	//	do 
	//	{
	//		uint32 numConts=GlobalAnim.m_arrController.size();
	//		//if ( numConts )
	//		//{
	//		uint32 numBones = m_pModel->m_pModelSkeleton->m_arrModelJoints.size();
	//		for (uint32 b=0; b<numBones; b++)
	//		{
	//			// this is an autopointer array, so there's no need to re-initialize the autopointers after construction
	//			int32 numControllers = (int)m_pModel->m_pModelSkeleton->m_arrModelJoints[b].m_arrControllersMJoint.size();
	//			if (numControllers <= nAnimID)
	//				m_pModel->m_pModelSkeleton->m_arrModelJoints[b].m_arrControllersMJoint.resize(nAnimID+1); 
	//			uint32 CRC32 = m_pModel->m_pModelSkeleton->m_arrModelJoints[b].m_nJointCRC32;
	//			IController* pIController=GlobalAnim.GetControllerByJointID(CRC32);
	//			m_pModel->m_pModelSkeleton->m_arrModelJoints[b].m_arrControllersMJoint[nAnimID] = 0;
	//		}
	//		//}
	//	} while(++it != m_arrAnimByGlobalId.end() && m_arrAnimations[it->nAnimId].m_nGlobalAnimId == nGlobalAnimId);
	//}
}










int CAnimationSet::LoadLMG(const char * szFilePath, float fScale, int nAnimID, const char * szAnimName, unsigned nGlobalAnimFlags)
{
	int nModelAnimId = GetAnimIDByName (szAnimName);
	if (nModelAnimId == -1) 
	{
		int nGlobalAnimID = g_AnimationManager.CreateGlobalMotionHeader(szFilePath);
		int nLocalAnimId = m_arrAnimations.size();

		ModelAnimationHeader LocalAnim; 
		LocalAnim.m_nGlobalAnimId = nGlobalAnimID;
		LocalAnim.SetAnimName(szAnimName);
		m_arrAnimations.push_back(LocalAnim);

//		m_arrAnimByGlobalId.insert (std::lower_bound(m_arrAnimByGlobalId.begin(), m_arrAnimByGlobalId.end(), nGlobalAnimID, AnimationGlobIdPred(m_arrAnimations)), LocalAnimId(nLocalAnimId));
		m_AnimationHashMap.InsertValue(&LocalAnim, nLocalAnimId);
		selfValidate();

		g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].AddRef();

		uint32 loaded=g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].IsAssetLoaded();
		if (loaded==0)
		{
			assert(g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].IsAssetLMG()==0);
			assert(g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].m_arrBSAnimations.size()==0);
			assert(g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].m_arrController.size()==0);
			GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID];
			rGlobalAnimHeader.m_nTicksPerFrame		= 0;
			rGlobalAnimHeader.m_fSecsPerTick			= 0;
			bool r=LoadLocoGroupXML( rGlobalAnimHeader ); 
			rGlobalAnimHeader.OnAssetLoaded();
			rGlobalAnimHeader.OnAssetLMG();
		}
		return nGlobalAnimID;
	}
	else
	{
		int nGlobalAnimID = m_arrAnimations[nModelAnimId].m_nGlobalAnimId;
		g_pILog->LogError("CryAnimation:: Trying to load animation with alias \"%s\" from file \"%s\" into the animation container. Such animation alias already exists and uses file \"%s\". Please use another animation alias.",szAnimName, szFilePath,	g_AnimationManager.GetGlobalAnimationHeader(nGlobalAnimID).GetPathName());
		return nGlobalAnimID;
	}

}



const SAnimationSelectionProperties* CAnimationSet::GetAnimationSelectionProperties(const char* szAnimationName)
{
	int32 localAnimID = GetAnimIDByName(szAnimationName);
	if (localAnimID < 0)
		return NULL;

	// Just make sure we have the prop's computed...
	if (!ComputeSelectionProperties(localAnimID))
		return NULL;

	int32 globalAnimID = m_arrAnimations[localAnimID].m_nGlobalAnimId;
	return g_AnimationManager.m_arrGlobalAnimations[globalAnimID].m_pSelectionProperties;
}

bool CAnimationSet::ComputeSelectionProperties(int32 localAnimID)
{
	int32 globalAnimID = m_arrAnimations[localAnimID].m_nGlobalAnimId;

	GlobalAnimationHeader& globalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[globalAnimID];
	if (globalAnimHeader.m_pSelectionProperties != NULL)
		return true;

	if (!globalAnimHeader.IsAssetLoaded())
		return false;

	#define BLENDCODE(code)		(*(uint32*)code)

	globalAnimHeader.AllocateAnimSelectProps();

	if (globalAnimHeader.m_nBlendCodeLMG != 0) // Is LMG
	{
		// Recurse sub animations first
		uint32 numSubAnims = globalAnimHeader.m_arrBSAnimations.size();
		for (uint32 i = 0; i < numSubAnims; ++i)
		{
			const char* szModelAnimNameSub = globalAnimHeader.m_arrBSAnimations[i].m_strAnimName; //0
			int32 localAnimIDSub = GetAnimIDByName(szModelAnimNameSub);
			assert(localAnimIDSub >= 0);
			if (localAnimIDSub < 0)
				return false;
			if (!ComputeSelectionProperties(localAnimIDSub))
				return false;
		}

		{
			globalAnimHeader.m_pSelectionProperties->m_fStartTravelSpeedMin = 100.0f;
			globalAnimHeader.m_pSelectionProperties->m_fEndTravelSpeedMin = 100.0f;
		}

		// Then use their limits to expand LMG limits.
		for (uint32 i = 0; i < numSubAnims; ++i)
		{
			const char* szModelAnimNameSub = globalAnimHeader.m_arrBSAnimations[i].m_strAnimName; //0
			int32 localAnimIDSub = GetAnimIDByName(szModelAnimNameSub);
			assert(localAnimIDSub >= 0);
			int32 globalAnimIDSub = m_arrAnimations[localAnimIDSub].m_nGlobalAnimId;
			assert(globalAnimIDSub >= 0);
			GlobalAnimationHeader& globalAnimHeaderSub = g_AnimationManager.m_arrGlobalAnimations[globalAnimIDSub];
			globalAnimHeader.m_pSelectionProperties->expand(globalAnimHeaderSub.m_pSelectionProperties);
		}

		SAnimationSelectionProperties& proceduralProperties = *globalAnimHeader.m_pSelectionProperties;

//#ifdef _DEBUG
		proceduralProperties.DebugCapsCode = globalAnimHeader.m_nSelectionCapsCode;
//#endif

		if (globalAnimHeader.m_nSelectionCapsCode == BLENDCODE("IROT"))
		{
			proceduralProperties.m_fStartTravelAngleMin = -180.0f;
			proceduralProperties.m_fStartTravelAngleMax = +180.0f;
			proceduralProperties.m_fEndTravelAngleMin = -180.0f;
			proceduralProperties.m_fEndTravelAngleMax = +180.0f;
			proceduralProperties.m_fEndBodyAngleMin = -180.0f;
			proceduralProperties.m_fEndBodyAngleMax = +180.0f;
			proceduralProperties.m_fTravelAngleChangeMin = -180.0f;
			proceduralProperties.m_fTravelAngleChangeMax = +180.0f;
			proceduralProperties.m_fTravelDistanceMin = 0.0f;
			proceduralProperties.m_fTravelDistanceMax = 0.0f;
			proceduralProperties.m_fStartTravelSpeedMin = 0.0f;
			proceduralProperties.m_fStartTravelSpeedMax = 0.0f;
			proceduralProperties.m_fEndTravelSpeedMin = 0.0f;
			proceduralProperties.m_fEndTravelSpeedMax = 0.0f;
			proceduralProperties.m_fEndTravelToBodyAngleMin = -180.0f;
			proceduralProperties.m_fEndTravelToBodyAngleMax = +180.0f;
			proceduralProperties.m_fUrgencyMin = 0.0f;
			proceduralProperties.m_fUrgencyMax = 0.0f;
			proceduralProperties.m_bLocomotion = true;
			proceduralProperties.m_fEndBodyAngleThreshold = 70.0f; // NOTE: This should always be less than the clamping angle, or the turn will never get a chance.
		}

		if (globalAnimHeader.m_nSelectionCapsCode == BLENDCODE("ISTP"))
		{
			proceduralProperties.m_fStartTravelAngleMin = -180.0f;
			proceduralProperties.m_fStartTravelAngleMax = +180.0f;
			proceduralProperties.m_fEndTravelAngleMin = -180.0f;
			proceduralProperties.m_fEndTravelAngleMax = +180.0f;
			proceduralProperties.m_fTravelDistanceMin = 0.3f;
			proceduralProperties.m_fTravelDistanceMax = 1.0f;
			proceduralProperties.m_fEndBodyAngleMin = 0.0f;
			proceduralProperties.m_fEndBodyAngleMax = 0.0f;
			proceduralProperties.m_fTravelAngleChangeMin = -180.0f;
			proceduralProperties.m_fTravelAngleChangeMax = +180.0f;
			proceduralProperties.m_fStartTravelSpeedMin = 0.0f;
			proceduralProperties.m_fStartTravelSpeedMax = 0.5f;
			proceduralProperties.m_fEndTravelSpeedMin = 0.0f;
			proceduralProperties.m_fEndTravelSpeedMax = 0.5f;
			proceduralProperties.m_fEndTravelToBodyAngleMin = -180.0f;
			proceduralProperties.m_fEndTravelToBodyAngleMax = +180.0f;
			proceduralProperties.m_fUrgencyMin = 0.0f;
			proceduralProperties.m_fUrgencyMax = 0.0f;
			proceduralProperties.m_bLocomotion = true;
			proceduralProperties.m_fTravelDistanceThreshold = 0.1f;
		}

		if (globalAnimHeader.m_nSelectionCapsCode == BLENDCODE("I2M_") ||
			globalAnimHeader.m_nSelectionCapsCode == BLENDCODE("I2W_"))
		{
			proceduralProperties.m_fStartTravelAngleMin = -180.0f;
			proceduralProperties.m_fStartTravelAngleMax = +180.0f;
			proceduralProperties.m_fEndTravelAngleMin = -180.0f;
			proceduralProperties.m_fEndTravelAngleMax = +180.0f;
			proceduralProperties.m_fEndBodyAngleMin = -180.0f;
			proceduralProperties.m_fEndBodyAngleMax = +180.0f;
			proceduralProperties.m_fTravelAngleChangeMin = -30.0f;
			proceduralProperties.m_fTravelAngleChangeMax = +30.0f;
			proceduralProperties.m_fDurationMin = 1.0f;
			proceduralProperties.m_fDurationMax = 10.0f;
			proceduralProperties.m_fTravelDistanceMin = 1.6f;
			proceduralProperties.m_fTravelDistanceMax = 10.0f;
			proceduralProperties.m_fStartTravelSpeedMin = 0.0f;
			proceduralProperties.m_fStartTravelSpeedMax = 0.8f;
			if (globalAnimHeader.m_nSelectionCapsCode == BLENDCODE("I2W_"))
			{
				proceduralProperties.m_fStartTravelSpeedMax = 0.1f;
				proceduralProperties.m_fUrgencyMin = 0.2f;
				proceduralProperties.m_fUrgencyMax = 0.9f;
				proceduralProperties.m_fTravelDistanceMin = 0.4f;
				proceduralProperties.m_fTravelDistanceMax = 5.0f;
			}
			else
			{
				proceduralProperties.m_fUrgencyMin = 1.0f;
				proceduralProperties.m_fUrgencyMax = 3.0f;
			}
			//proceduralProperties.m_fEndTravelSpeedMin *= Console::GetInst().ca_travelSpeedScaleMin;
			//proceduralProperties.m_fEndTravelSpeedMax *= Console::GetInst().ca_travelSpeedScaleMax;
			proceduralProperties.m_fEndTravelToBodyAngleMin = -30.0f;
			proceduralProperties.m_fEndTravelToBodyAngleMax = +30.0f;
			proceduralProperties.m_bPredicted = true;
			proceduralProperties.m_bGuarded = true;
			proceduralProperties.m_bComplexBodyTurning = false; // These two are not supported anymore and are instead specified in the AG template (optimization).
			proceduralProperties.m_bComplexTravelPath = false;
			proceduralProperties.m_bLocomotion = true;
			//proceduralProperties.m_fTravelDistanceThreshold = 1.0f;
		}

		if (globalAnimHeader.m_nSelectionCapsCode == BLENDCODE("M2I_") ||
			globalAnimHeader.m_nSelectionCapsCode == BLENDCODE("W2I_"))
		{
/*
			proceduralProperties.m_fStartTravelAngleMin = -180.0f;
			proceduralProperties.m_fStartTravelAngleMax = +180.0f;
			proceduralProperties.m_fEndTravelAngleMin = -180.0f;
			proceduralProperties.m_fEndTravelAngleMax = +180.0f;
			proceduralProperties.m_fEndBodyAngleMin = -180.0f;
			proceduralProperties.m_fEndBodyAngleMax = +180.0f;
*/
			proceduralProperties.m_fStartTravelAngleMin = -5.0f;
			proceduralProperties.m_fStartTravelAngleMax = +5.0f;
			proceduralProperties.m_fEndTravelAngleMin = -5.0f;
			proceduralProperties.m_fEndTravelAngleMax = +5.0f;
			proceduralProperties.m_fEndBodyAngleMin = -5.0f;
			proceduralProperties.m_fEndBodyAngleMax = +5.0f;
			proceduralProperties.m_fTravelAngleChangeMin = -5.0f;
			proceduralProperties.m_fTravelAngleChangeMax = 5.0f;
			proceduralProperties.m_fDurationMin = 1.0f;
			proceduralProperties.m_fDurationMax = 1.0f;
			proceduralProperties.m_fTravelDistanceMin = proceduralProperties.m_fTravelDistanceMax * 1.1f;
			proceduralProperties.m_fTravelDistanceMax = proceduralProperties.m_fTravelDistanceMax * 1.2f;
			//proceduralProperties.m_fStartTravelSpeedMin *= Console::GetInst().ca_travelSpeedScaleMin;
			proceduralProperties.m_fStartTravelSpeedMax *= Console::GetInst().ca_travelSpeedScaleMax * 4.0f;
			proceduralProperties.m_fEndTravelSpeedMin = 0.0f;
			proceduralProperties.m_fEndTravelSpeedMax = 0.5f;
			if (globalAnimHeader.m_nSelectionCapsCode == BLENDCODE("W2I_"))
			{
				proceduralProperties.m_fUrgencyMin = 0.2f;
				proceduralProperties.m_fUrgencyMax = 0.9f;
			}
			else
			{
				proceduralProperties.m_fUrgencyMin = 1.0f;
				proceduralProperties.m_fUrgencyMax = 3.0f;
			}
			proceduralProperties.m_fEndTravelToBodyAngleMin = -5.0f;
			proceduralProperties.m_fEndTravelToBodyAngleMax = +5.0f;
			proceduralProperties.m_bPredicted = true;
			proceduralProperties.m_bGuarded = true;
			proceduralProperties.m_bLocomotion = true;
			//proceduralProperties.m_fTravelDistanceThreshold = 1.0f;
		}

		// WALKS
		if (globalAnimHeader.m_nSelectionCapsCode == BLENDCODE("WALK"))
		{
			proceduralProperties.m_fDurationMin = 0.0f;
			proceduralProperties.m_fDurationMax = 10.0f;
			proceduralProperties.m_fTravelDistanceMin = 0.0f;
			proceduralProperties.m_fTravelDistanceMax = 10.0f;
			//proceduralProperties.m_fStartTravelSpeedMin = 1.0f;
			proceduralProperties.m_fStartTravelSpeedMin *= Console::GetInst().ca_travelSpeedScaleMin * 0.0f;
			proceduralProperties.m_fStartTravelSpeedMax *= Console::GetInst().ca_travelSpeedScaleMax;
			proceduralProperties.m_fEndTravelSpeedMin *= Console::GetInst().ca_travelSpeedScaleMin * 0.0f;
			proceduralProperties.m_fEndTravelSpeedMax *= Console::GetInst().ca_travelSpeedScaleMax;
			proceduralProperties.m_fStartTravelAngleMin = -180.0f;
			proceduralProperties.m_fStartTravelAngleMax = +180.0f;
			proceduralProperties.m_fEndTravelAngleMin = -180.0f;
			proceduralProperties.m_fEndTravelAngleMax = +180.0f;
			proceduralProperties.m_fEndBodyAngleMin = -180.0f;
			proceduralProperties.m_fEndBodyAngleMax = +180.0f;
			proceduralProperties.m_fTravelAngleChangeMin = -180.0f;
			proceduralProperties.m_fTravelAngleChangeMax = +180.0f;
			proceduralProperties.m_fEndTravelToBodyAngleMin = -180.0f;
			proceduralProperties.m_fEndTravelToBodyAngleMax = +180.0f;
			proceduralProperties.m_fUrgencyMin = 0.2f;
			proceduralProperties.m_fUrgencyMax = 0.5f;
			proceduralProperties.m_bLocomotion = true;
		}

		// RUN+SPRINT
		if (globalAnimHeader.m_nSelectionCapsCode == BLENDCODE("RUN_"))
		{
			f32 fScale = 1.0f; 
			proceduralProperties.m_fDurationMin = 0.0f;
			proceduralProperties.m_fDurationMax = 10.0f;
			proceduralProperties.m_fTravelDistanceMin = 0.0f;
			proceduralProperties.m_fTravelDistanceMax = 10.0f;
			//proceduralProperties.m_fStartTravelSpeedMin = 1.0f;
			proceduralProperties.m_fStartTravelSpeedMin *= Console::GetInst().ca_travelSpeedScaleMin * 0.0f;
			proceduralProperties.m_fStartTravelSpeedMax *= Console::GetInst().ca_travelSpeedScaleMax;
			proceduralProperties.m_fEndTravelSpeedMin *= Console::GetInst().ca_travelSpeedScaleMin * 0.0f;
			//proceduralProperties.m_fEndTravelSpeedMin = max(proceduralProperties.m_fStartTravelSpeedMin, proceduralProperties.m_fEndTravelSpeedMin);
			proceduralProperties.m_fEndTravelSpeedMax *= Console::GetInst().ca_travelSpeedScaleMax;
			proceduralProperties.m_fStartTravelAngleMin = -180.0f*fScale;
			proceduralProperties.m_fStartTravelAngleMax = +180.0f*fScale;
			proceduralProperties.m_fEndTravelAngleMin = -180.0f*fScale;
			proceduralProperties.m_fEndTravelAngleMax = +180.0f*fScale;
			proceduralProperties.m_fEndBodyAngleMin = -180.0f*fScale;
			proceduralProperties.m_fEndBodyAngleMax = +180.0f*fScale;
			proceduralProperties.m_fTravelAngleChangeMin = -180.0f*fScale;
			proceduralProperties.m_fTravelAngleChangeMax = +180.0f*fScale;
			proceduralProperties.m_fEndTravelToBodyAngleMin = -180.0f*fScale;
			proceduralProperties.m_fEndTravelToBodyAngleMax = +180.0f*fScale;
			proceduralProperties.m_fUrgencyMin = 0.9f;
			proceduralProperties.m_fUrgencyMax = 2.0f;
			proceduralProperties.m_bLocomotion = true;
		}

		//globalAnimHeader.m_selectionProperties = proceduralProperties;
	}
	else
	{
		// Measure the trajectory of the anim to figure out all the properties.
		SAnimationSelectionProperties& props = *globalAnimHeader.m_pSelectionProperties;

		props.m_fDurationMin = globalAnimHeader.m_fEndSec - globalAnimHeader.m_fStartSec;

		int jointIndex = 0;
		const CModelJoint& modelJoint = m_pModel->m_pModelSkeleton->m_arrModelJoints[jointIndex];
		IController* pController = g_AnimationManager.m_arrGlobalAnimations[globalAnimID].GetControllerByJointID(modelJoint.m_nJointCRC32);//modelJoint.m_arrControllersMJoint[localAnimID];
		if (pController == NULL)
			return false;

		static float samplesPerSecond = 20.0f;
		int sampleCount = max(2, (int)(samplesPerSecond * props.m_fDurationMin));
		float sampleDuration = props.m_fDurationMin / (float)sampleCount;

		// These allocating should maybe be moved outside the loop and reused for all assets, allocated at the maximum size, or implemented as chunks.
		Vec3* locomotionLocatorSamplesPos = new Vec3[sampleCount];
		Quat* locomotionLocatorSamplesOri = new Quat[sampleCount];
		Vec3* locomotionLocatorSamplesMovement = new Vec3[sampleCount];
		Vec3* locomotionLocatorSamplesMovementSmoothed = new Vec3[sampleCount];
		Vec3* locomotionLocatorSamplesBodyDirSmoothed = new Vec3[sampleCount];

		// Evaluate translation & orientation of joint 0 of the animation asset.
		for (int i = 0; i < sampleCount; ++i)
		{
			float t = (float)i / (float)(sampleCount - 1);
			pController->GetP(globalAnimID, t, locomotionLocatorSamplesPos[i]);
			pController->GetO(globalAnimID, t, locomotionLocatorSamplesOri[i]);
		}

		for (int i = 0; i < sampleCount; ++i)
		{
			int i0 = max(i - 1, 0);
			int i1 = min(i + 1, sampleCount - 1);
			locomotionLocatorSamplesMovement[i] = locomotionLocatorSamplesPos[i1] - locomotionLocatorSamplesPos[i0];
			locomotionLocatorSamplesMovement[i] /= (float)(i1 - i0);
		}

		static int windowSampleCount = 10;
		for (int i = 0; i < sampleCount; ++i)
		{
			locomotionLocatorSamplesBodyDirSmoothed[i] = ZERO;
			locomotionLocatorSamplesMovementSmoothed[i] = ZERO;
			float weightSum = 0.0f;
			for (int j = 0; j < windowSampleCount; ++j)
			{
				int k = CLAMP(i - windowSampleCount/2 + j, 0, sampleCount-1);
				float weight = 1.0f - CLAMP((float)(k - i) / (float)windowSampleCount, 0.0f, 1.0f);
				weightSum += weight;
				locomotionLocatorSamplesMovementSmoothed[i] += locomotionLocatorSamplesMovement[k] * weight;
				
				Vec3 locomotionLocatorBodyDir = locomotionLocatorSamplesOri[i] * FORWARD_DIRECTION;
				locomotionLocatorSamplesBodyDirSmoothed[i] += locomotionLocatorBodyDir * weight;
			}
			locomotionLocatorSamplesMovementSmoothed[i] /= weightSum;
			locomotionLocatorSamplesBodyDirSmoothed[i] /= weightSum;
		}

		props.m_fTravelDistanceMin = (locomotionLocatorSamplesPos[sampleCount-1] - locomotionLocatorSamplesPos[0]).GetLength();
		props.m_fStartTravelSpeedMin = locomotionLocatorSamplesMovementSmoothed[0].GetLength() / sampleDuration;
		props.m_fEndTravelSpeedMin = locomotionLocatorSamplesMovementSmoothed[sampleCount-1].GetLength() / sampleDuration;
		props.m_fStartTravelAngleMin = DEG2RAD(cry_atan2f(-locomotionLocatorSamplesMovementSmoothed[0].x, locomotionLocatorSamplesMovementSmoothed[0].y));
		props.m_fEndTravelAngleMin = DEG2RAD(cry_atan2f(-locomotionLocatorSamplesMovementSmoothed[sampleCount-1].x, locomotionLocatorSamplesMovementSmoothed[sampleCount-1].y));
		float fStartBodyAngle = DEG2RAD(cry_atan2f(-locomotionLocatorSamplesBodyDirSmoothed[0].x, locomotionLocatorSamplesBodyDirSmoothed[0].y));
		float fEndBodyAngle = DEG2RAD(cry_atan2f(-locomotionLocatorSamplesBodyDirSmoothed[sampleCount-1].x, locomotionLocatorSamplesBodyDirSmoothed[sampleCount-1].y));
		props.m_fEndBodyAngleMin = (fEndBodyAngle - fStartBodyAngle);
		if (props.m_fEndBodyAngleMin < -180.0f)
			props.m_fEndBodyAngleMin += 360.0f;
		if (props.m_fEndBodyAngleMin > 180.0f)
			props.m_fEndBodyAngleMin -= 360.0f;

		if (abs(props.m_fStartTravelAngleMin) < 0.0001f) props.m_fStartTravelAngleMin = 0.0f;
		if (abs(props.m_fEndTravelAngleMin) < 0.0001f) props.m_fEndTravelAngleMin = 0.0f;
		if (abs(props.m_fEndBodyAngleMin) < 0.0001f) props.m_fEndBodyAngleMin = 0.0f;

		props.m_fTravelAngleChangeMin = props.m_fEndTravelAngleMin - props.m_fStartTravelAngleMin;

		props.m_fDurationMax					= props.m_fDurationMin;
		props.m_fStartTravelSpeedMax	= props.m_fStartTravelSpeedMin;
		props.m_fEndTravelSpeedMax		=	props.m_fEndTravelSpeedMin;
		props.m_fStartTravelAngleMax	= props.m_fStartTravelAngleMin;
		props.m_fEndTravelAngleMax		=	props.m_fEndTravelAngleMin;
		props.m_fEndBodyAngleMin			=	props.m_fEndBodyAngleMax;
		props.m_fTravelDistanceMax		=	props.m_fTravelDistanceMin;
		props.m_fTravelAngleChangeMax = props.m_fTravelAngleChangeMin;

		delete[] locomotionLocatorSamplesPos;
		delete[] locomotionLocatorSamplesOri;
		delete[] locomotionLocatorSamplesMovement;
		delete[] locomotionLocatorSamplesMovementSmoothed;
		delete[] locomotionLocatorSamplesBodyDirSmoothed;
	}

	return true;
}

void CAnimationSet::ComputeSelectionProperties()
{
/*
	int32 animationCount = m_arrAnimations.size();
	for (int32 localAnimID = 0; localAnimID < animationCount; localAnimID++)
	{
		bool success = ComputeSelectionProperties(localAnimID);
		//assert(success);
	}
*/
}


//----------------------------------------------------------------------------------
//----      check if all animation-assets in a locomotion group are valid       ----
//----------------------------------------------------------------------------------
void CAnimationSet::VerifyLocomotionGroups()
{
	//search for animation-names 
	uint32 numAnimation=m_arrAnimations.size();
	for (uint32 i=0; i<numAnimation; i++)
	{

		uint32 GlobalAnimationID			= m_arrAnimations[i].m_nGlobalAnimId;
		GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[GlobalAnimationID];
		const char* LMGName = m_arrAnimations[i].GetAnimName();

		if (rGlobalAnimHeader.IsAssetLoaded())
		{
			if (rGlobalAnimHeader.IsAssetLMG())
			{
				//its a locomotion group
				uint32 numLMG = rGlobalAnimHeader.m_arrBSAnimations.size();
				uint32 LMG_OK=numLMG;
				for (uint32 g=0; g<numLMG; g++)
				{
					const char* aname = rGlobalAnimHeader.m_arrBSAnimations[g].m_strAnimName;
					int32 id=GetAnimIDByName(aname);
					if (id<0)
					{
						AnimFileWarning(m_pModel->GetModelFilePath(),"locomotion group '%s' is invalid! The animation '%s' is not in the CAL-file.", m_arrAnimations[i].GetAnimName(),aname);
						LMG_OK=0;
					}
					else
					{
						int32 gaid=m_arrAnimations[id].m_nGlobalAnimId;
						GlobalAnimationHeader& rGAH = g_AnimationManager.m_arrGlobalAnimations[gaid];
						if (rGAH.m_fStartSec<0 || rGAH.m_fEndSec<0)
						{
							AnimFileWarning(m_pModel->GetModelFilePath(),"locomotion group '%s' is invalid! The the asset for animation '%s' does not exist.", m_arrAnimations[i].GetAnimName(),aname);
							LMG_OK=0;
						} 

					}


					//---------------------------------------------------------------------------
					const char* OldStyle = rGlobalAnimHeader.m_strOldStyle;
					uint32 hasOldStyle = OldStyle[0];
					if (hasOldStyle)
					{
						int32 id=GetAnimIDByName(OldStyle);
						if (id<0)
						{
							g_pILog->LogWarning("CryAnimation: locomotion group '%s' is invalid! The style-animation '%s' is not in the CAL-file.", m_arrAnimations[i].GetAnimName(),OldStyle);
							LMG_OK=0;
						}
						else
						{
							int32 gaid=m_arrAnimations[id].m_nGlobalAnimId;
							GlobalAnimationHeader& rGAH = g_AnimationManager.m_arrGlobalAnimations[gaid];
							if (rGAH.m_fStartSec<0 || rGAH.m_fEndSec<0)
							{
								g_pILog->LogWarning("CryAnimation: locomotion group '%s' is invalid! The the style-asset for animation '%s' does not exist.", m_arrAnimations[i].GetAnimName(),OldStyle);
								LMG_OK=0;
							} 
						}
					}

					const char* NewStyle = rGlobalAnimHeader.m_strNewStyle;
					uint32 hasNewStyle = NewStyle[0];
					if (hasNewStyle)
					{
						int32 id=GetAnimIDByName(NewStyle);
						if (id<0)
						{
							g_pILog->LogWarning("CryAnimation: locomotion group '%s' is invalid! The style-animation '%s' is not in the CAL-file.", m_arrAnimations[i].GetAnimName(),NewStyle);
							LMG_OK=0;
						}
						else
						{
							int32 gaid=m_arrAnimations[id].m_nGlobalAnimId;
							GlobalAnimationHeader& rGAH = g_AnimationManager.m_arrGlobalAnimations[gaid];
							if (rGAH.m_fStartSec<0 || rGAH.m_fEndSec<0)
							{
								g_pILog->LogWarning("CryAnimation: locomotion group '%s' is invalid! The the style-asset for animation '%s' does not exist.", m_arrAnimations[i].GetAnimName(),NewStyle);
								LMG_OK=0;
							} 
						}
					}

					if (hasOldStyle || hasNewStyle)
					{
						if (hasOldStyle==0)
							LMG_OK=0;
						if (hasNewStyle==0)
							LMG_OK=0;
					}



				}

				if (LMG_OK)
				{
					rGlobalAnimHeader.OnAssetLMGValid();
					ParameterExtraction( rGlobalAnimHeader );
				}

			}
		}

	}
}




void CAnimationSet::ParameterExtraction( GlobalAnimationHeader& rGlobalAnimHeader )
{

	const CModelJoint* pModelJoint	= &m_pModel->m_pModelSkeleton->m_arrModelJoints[0];
	IController* pController=0;
	Vec3 pos;
	uint32 nBC = rGlobalAnimHeader.m_nBlendCodeLMG;

	//-----------------------------------------------------------------------------------------
	//----------                     idle parameterizations                     ---------------
	//-----------------------------------------------------------------------------------------
	uint32 nIROT = *(uint32*)"IROT";
	uint32 nTSTP = *(uint32*)"TSTP";
	if (nBC==nIROT || nBC==nTSTP)
	{
		uint32 numAssets = rGlobalAnimHeader.m_arrBSAnimations.size();
		for (uint32 i=0; i<numAssets; i++)
		{
			const char* name = rGlobalAnimHeader.m_arrBSAnimations[i].m_strAnimName; //0
			int32 globalID = GetGlobalIDByName(name);
			assert(globalID>=0);
			int32 animID = GetAnimIDByName(name);
			assert(animID>=0);

			f32 fStart	= g_AnimationManager.m_arrGlobalAnimations[globalID].m_fStartSec;
			f32 fEnd		= g_AnimationManager.m_arrGlobalAnimations[globalID].m_fEndSec;
			f32 duration = fEnd-fStart;

			f32 speed = g_AnimationManager.m_arrGlobalAnimations[globalID].m_fSpeed;
			g_AnimationManager.m_arrGlobalAnimations[globalID].m_vVelocity	=	Vec3(0,speed,0);

			pController	= g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[0].m_nJointCRC32);//pModelJoint[0].m_arrControllersMJoint[animID];
      assert(pController);
      if(!pController)
        continue;

			Quat rot0; pController->GetO(globalID,0, rot0);
			Quat rot1; pController->GetO(globalID,1, rot1);
			Vec3 v0=rot0.GetColumn1();
			Vec3 v1=rot1.GetColumn1();
			f32 radiant = Ang3::CreateRadZ(v0,v1);
			g_AnimationManager.m_arrGlobalAnimations[globalID].m_fAssetTurn =	radiant;
			g_AnimationManager.m_arrGlobalAnimations[globalID].m_fTurnSpeed =	radiant/duration;
		}
	}


	//-----------------------------------------------------------------------------------------
	//----------             these are all the MoveStrafe-LMGs we have          ---------------
	//-----------------------------------------------------------------------------------------
	uint32 nISTP = *(uint32*)"ISTP";

	uint32 nS__1 = *(uint32*)"S__1"; // strafe, one speed
	uint32 nS__2 = *(uint32*)"S__2"; // strafe, two speeds
	uint32 nS_H2 = *(uint32*)"S_H2"; // strafe, slope, two speeds
	uint32 nST_2 = *(uint32*)"ST_2"; // strafe+curving, two speeds
	uint32 nSTH2 = *(uint32*)"STH2"; // strafe+curve+slope, two speeds
	uint32 nSTF1 = *(uint32*)"STF1"; // strafe, one speed
	uint32 nSTF2 = *(uint32*)"STF2"; // strafe, two speeds

	// OBSOLETE
	uint32 nSUD2 = *(uint32*)"SUD2";
	uint32 nSTHX = *(uint32*)"STHX";

	if ( nBC==nS__1 || nBC==nS__2 || nBC==nS_H2 || nBC==nST_2 || nBC==nSTH2 || nBC==nSTHX     ||    nBC==nSTF1 ||	nBC==nSTF2 || nBC==nSUD2)
	{
		uint32 numAssets = rGlobalAnimHeader.m_arrBSAnimations.size();
		for (uint32 i=0; i<numAssets; i++)
		{
			const char* name = rGlobalAnimHeader.m_arrBSAnimations[i].m_strAnimName; //0
			int32 globalID = GetGlobalIDByName(name);
			assert(globalID>=0);
			int32 animID = GetAnimIDByName(name);
			assert(animID>=0);

			f32 fStart	= g_AnimationManager.m_arrGlobalAnimations[globalID].m_fStartSec;
			f32 fEnd		= g_AnimationManager.m_arrGlobalAnimations[globalID].m_fEndSec;
			f32 duration = fEnd-fStart;

			pController	= g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[0].m_nJointCRC32);//;//pModelJoint[0].m_arrControllersMJoint[animID];
      assert(pController);
      if (!pController)
        continue;

			Vec3 TravelDir0; pController->GetP(globalID,0,TravelDir0);
			Vec3 TravelDir1; pController->GetP(globalID,1,TravelDir1);
			assert( (TravelDir1-TravelDir0).GetLength()>0.01f );
			Vec3 TravelDir = (TravelDir1-TravelDir0).GetNormalizedSafe(Vec3(0,1,0));

			f32 speed = g_AnimationManager.m_arrGlobalAnimations[globalID].m_fSpeed;
			assert(speed>0.1f);
			g_AnimationManager.m_arrGlobalAnimations[globalID].m_vVelocity	=	Vec3(TravelDir)*speed;

			Quat rot0,rot1;
//			pController	= pModelJoint[0].m_arrControllersMJoint[animID];
			pController->GetO(globalID,0, rot0);
			pController->GetO(globalID,1, rot1);
			Vec3 v0=rot0.GetColumn1();
			Vec3 v1=rot1.GetColumn1();
			f32 radiant = Ang3::CreateRadZ(v0,v1);
			f32 fTurnSec = radiant/duration;
			g_AnimationManager.m_arrGlobalAnimations[globalID].m_fAssetTurn =	radiant;
			g_AnimationManager.m_arrGlobalAnimations[globalID].m_fTurnSpeed = fTurnSec;
			if (fabsf(fTurnSec)>0.3f)
				g_AnimationManager.m_arrGlobalAnimations[globalID].m_vVelocity	=	Vec3(0,speed,0);
		}
	}


	//--------------------------------------------------------------------------------------------
	//--------------------------------------------------------------------------------------------
	//--------------------------------------------------------------------------------------------
  if (nBC==nSTH2)
	{
		uint32 numAssets = rGlobalAnimHeader.m_arrBSAnimations.size();
		//not a perfect solution, but it will do for now
		{
			const char* name0 = rGlobalAnimHeader.m_arrBSAnimations[21].m_strAnimName; //0
			int32 globalID0 = GetGlobalIDByName(name0);
			assert(globalID0>=0);
			f32 radiant0 = g_AnimationManager.m_arrGlobalAnimations[globalID0].m_fAssetTurn;
			f32 turnsec0 = g_AnimationManager.m_arrGlobalAnimations[globalID0].m_fTurnSpeed;
			assert(turnsec0>0.5f);

			const char* name1 = rGlobalAnimHeader.m_arrBSAnimations[23].m_strAnimName; //0
			int32 globalID1 = GetGlobalIDByName(name1);
			assert(globalID1>=0);
			f32 radiant1 = g_AnimationManager.m_arrGlobalAnimations[globalID1].m_fAssetTurn;
			f32 turnsec1 = g_AnimationManager.m_arrGlobalAnimations[globalID1].m_fTurnSpeed;
			assert(turnsec1>0.5f);

			g_AnimationManager.m_arrGlobalAnimations[globalID0].m_fAssetTurn=(radiant0+radiant1)*0.5f;
			g_AnimationManager.m_arrGlobalAnimations[globalID0].m_fTurnSpeed=(turnsec0+turnsec1)*0.5f;

			g_AnimationManager.m_arrGlobalAnimations[globalID1].m_fAssetTurn=(radiant0+radiant1)*0.5f;
			g_AnimationManager.m_arrGlobalAnimations[globalID1].m_fTurnSpeed=(turnsec0+turnsec1)*0.5f;
		}

		{
			const char* name0 = rGlobalAnimHeader.m_arrBSAnimations[22].m_strAnimName; //0
			int32 globalID0 = GetGlobalIDByName(name0);
			assert(globalID0>=0);
			f32 radiant0 = g_AnimationManager.m_arrGlobalAnimations[globalID0].m_fAssetTurn;
			f32 turnsec0 = g_AnimationManager.m_arrGlobalAnimations[globalID0].m_fTurnSpeed;
			assert(turnsec0<-0.5f);

			const char* name1 = rGlobalAnimHeader.m_arrBSAnimations[24].m_strAnimName; //0
			int32 globalID1 = GetGlobalIDByName(name1);
			assert(globalID1>=0);
			f32 radiant1 = g_AnimationManager.m_arrGlobalAnimations[globalID1].m_fAssetTurn;
			f32 turnsec1 = g_AnimationManager.m_arrGlobalAnimations[globalID1].m_fTurnSpeed;
			assert(turnsec0<-0.5f);

			g_AnimationManager.m_arrGlobalAnimations[globalID0].m_fAssetTurn=(radiant0+radiant1)*0.5f;
			g_AnimationManager.m_arrGlobalAnimations[globalID0].m_fTurnSpeed=(turnsec0+turnsec1)*0.5f;

			g_AnimationManager.m_arrGlobalAnimations[globalID1].m_fAssetTurn=(radiant0+radiant1)*0.5f;
			g_AnimationManager.m_arrGlobalAnimations[globalID1].m_fTurnSpeed=(turnsec0+turnsec1)*0.5f;
		}

		uint32 ddd=0;

	}

	//--------------------------------------------------------------------------------------------
	//--------------------------------------------------------------------------------------------
	//--------------------------------------------------------------------------------------------


	if ( nBC==nISTP )
	{
		uint32 numAssets = rGlobalAnimHeader.m_arrBSAnimations.size();
		for (uint32 i=0; i<numAssets; i++)
		{
			const char* name = rGlobalAnimHeader.m_arrBSAnimations[i].m_strAnimName; //0
			int32 globalID = GetGlobalIDByName(name);
			assert(globalID>=0);
			int32 animID = GetAnimIDByName(name);
			assert(animID>=0);

			f32 fStart	= g_AnimationManager.m_arrGlobalAnimations[globalID].m_fStartSec;
			f32 fEnd		= g_AnimationManager.m_arrGlobalAnimations[globalID].m_fEndSec;
			f32 duration = fEnd-fStart;

			pController	= g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[0].m_nJointCRC32);//pModelJoint[0].m_arrControllersMJoint[animID];
			Vec3 TravelDir0; pController->GetP(globalID,0,TravelDir0);
			Vec3 TravelDir1; pController->GetP(globalID,1,TravelDir1);
			//		assert( (TravelDir1-TravelDir0).GetLength()>0.01f );
			Vec3 TravelDir = (TravelDir1-TravelDir0).GetNormalizedSafe(Vec3(0,1,0));

			f32 speed = g_AnimationManager.m_arrGlobalAnimations[globalID].m_fSpeed;
			//		assert(speed>0.1f);
			g_AnimationManager.m_arrGlobalAnimations[globalID].m_vVelocity	=	Vec3(TravelDir)*speed;

			g_AnimationManager.m_arrGlobalAnimations[globalID].m_fAssetTurn =	0;
			g_AnimationManager.m_arrGlobalAnimations[globalID].m_fTurnSpeed = 0;
		}
	}



	//-----------------------------------------------------------------------------------------
	//----------            these are all the MoveTurn-LMGs we have               -------------
	//-----------------------------------------------------------------------------------------

	uint32 nFLR1 = *(uint32*)"FLR1"; 
	uint32 nFLR2 = *(uint32*)"FLR2"; 
	uint32 nFLR3 = *(uint32*)"FLR3"; 

	uint32 nUDH1 = *(uint32*)"UDH1"; 
	uint32 nUDH2 = *(uint32*)"UDH2"; 
	uint32 nUDH3 = *(uint32*)"UDH3";

	if (nBC==nFLR1 ||	nBC==nFLR2 ||	nBC==nFLR3 || nBC==nUDH1 || nBC==nUDH2 ||	nBC==nUDH3 )
	{
		uint32 numAssets = rGlobalAnimHeader.m_arrBSAnimations.size();
		for (uint32 i=0; i<numAssets; i++)
		{
			const char* name = rGlobalAnimHeader.m_arrBSAnimations[i].m_strAnimName; //0
			int32 globalID = GetGlobalIDByName(name);
			assert(globalID>=0);
			int32 animID = GetAnimIDByName(name);
			assert(animID>=0);

			f32 fStart	= g_AnimationManager.m_arrGlobalAnimations[globalID].m_fStartSec;
			f32 fEnd		= g_AnimationManager.m_arrGlobalAnimations[globalID].m_fEndSec;
			f32 duration = fEnd-fStart;

			f32 speed = g_AnimationManager.m_arrGlobalAnimations[globalID].m_fSpeed;
			g_AnimationManager.m_arrGlobalAnimations[globalID].m_vVelocity	=	Vec3(0,speed,0);

			Quat rot0,rot1;
			pController	= g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[0].m_nJointCRC32);//g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[0].m_nJointCRC32);//pModelJoint[0].m_arrControllersMJoint[animID];
			pController->GetO(globalID,0, rot0);
			pController->GetO(globalID,1, rot1);
			Vec3 v0=rot0.GetColumn1();
			Vec3 v1=rot1.GetColumn1();
			f32 radiant = Ang3::CreateRadZ(v0,v1);
			g_AnimationManager.m_arrGlobalAnimations[globalID].m_fAssetTurn =	radiant;
			g_AnimationManager.m_arrGlobalAnimations[globalID].m_fTurnSpeed =	radiant/duration;
		}
	}
	//-------------------------------------------------------------------------------------------------

	uint32 nI2M1 = *(uint32*)"I2M1"; 
	uint32 nI2M2 = *(uint32*)"I2M2"; 
	if (nBC==nI2M1 || nBC==nI2M2)
	{
		uint32 numAssets = rGlobalAnimHeader.m_arrBSAnimations.size();
		for (uint32 i=0; i<numAssets; i++)
		{
			const char* name = rGlobalAnimHeader.m_arrBSAnimations[i].m_strAnimName; //0
			int32 globalID = GetGlobalIDByName(name);
			assert(globalID>=0);
			int32 animID = GetAnimIDByName(name);
			assert(animID>=0);

			f32 fStart	= g_AnimationManager.m_arrGlobalAnimations[globalID].m_fStartSec;
			f32 fEnd		= g_AnimationManager.m_arrGlobalAnimations[globalID].m_fEndSec;
			f32 duration = fEnd-fStart;

			pController	= g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[0].m_nJointCRC32);//pModelJoint[0].m_arrControllersMJoint[animID];
			Vec3 TravelDir0; pController->GetP(globalID,0,TravelDir0);
			Vec3 TravelDir1; pController->GetP(globalID,1,TravelDir1);
			Vec3 TravelDir = (TravelDir1-TravelDir0).GetNormalizedSafe(Vec3(0,1,0));

//			pController	= pModelJoint[0].m_arrControllersMJoint[animID];
			Quat rot0; pController->GetO(globalID,0, rot0);
			Vec3 v0=rot0.GetColumn1();
			Quat rot1; pController->GetO(globalID,1, rot1);
			Vec3 v1=rot1.GetColumn1();
			f32 radiant = Ang3::CreateRadZ(v0,v1);
			g_AnimationManager.m_arrGlobalAnimations[globalID].m_fAssetTurn =	radiant;
			g_AnimationManager.m_arrGlobalAnimations[globalID].m_fTurnSpeed =	radiant/duration;

			uint32 numKeys = uint32((duration / (1.0f/30.0f))+1);
			f32 timestep = 1.0f/f32(numKeys);
			f32 t=0.0f;
			std::vector<Vec3> root_keys;
			root_keys.resize(numKeys);

			for (uint32 k=0; k<numKeys; k++)
			{
				pController->GetP(globalID,t, root_keys[k]);
				t += timestep;
			}

			f32 speed = 0.0f; 
			uint32 poses=5;
			for (uint32 k=(numKeys-poses-1); k<(numKeys-1); k++)
				speed += (root_keys[k+0]-root_keys[k+1]).GetLength()*30.0f; 

			speed/=poses;

			g_AnimationManager.m_arrGlobalAnimations[globalID].m_fSpeed =	speed;
			g_AnimationManager.m_arrGlobalAnimations[globalID].m_vVelocity = Vec3(0,speed,0);
		}
	}

}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool CAnimationSet::LoadLocoGroupXML(  GlobalAnimationHeader& rGlobalAnim ) 
{
	uint32 numAnims = rGlobalAnim.m_arrBSAnimations.size();
	if (numAnims)
		return 1; //already loaded

	const char* pathname = rGlobalAnim.GetPathName(); 
	g_pILog->LogToFile("Loading LMG %s", pathname);				// to file only so console is cleaner
	g_pILog->UpdateLoadingScreen(0);
	XmlNodeRef root		= g_pISystem->LoadXmlFile(pathname);	
	if (root==0)
	{
		g_pILog->LogError ("locomotion-group not found: %s", pathname);
		return 0;
	}

	const char* XMLTAG = root->getTag();

	if (strcmp(XMLTAG,"LocomotionGroup")==0) 
	{
		rGlobalAnim.m_arrBSAnimations.reserve( MAX_LMG_ANIMS );

		uint32 numChilds = root->getChildCount();
		for (uint32 c=0; c<numChilds; c++)
		{
			XmlNodeRef nodeList = root->getChild(c);
			const char* ListTag = nodeList->getTag();


			//-----------------------------------------------------------
			//load example-list 
			//-----------------------------------------------------------
			if ( strcmp(ListTag,"ExampleList")==0 ) 
			{
				uint32 num = nodeList->getChildCount();
				for (uint32 i=0; i<num; i++) 
				{
					XmlNodeRef nodeExample = nodeList->getChild(i);
					const char* ExampleTag = nodeExample->getTag();
					if (strcmp(ExampleTag,"Example")==0) 
					{
						BSAnimation bsa;
						bsa.m_strAnimName = nodeExample->getAttr( "AName" );
						bsa.m_Position;		nodeExample->getAttr( "Position",bsa.m_Position );
						rGlobalAnim.m_arrBSAnimations.push_back(bsa);
					}
					else 
						return 0;
				}
				continue;
			}

			//-----------------------------------------------------------
			//---                load blending-type                   ---
			//-----------------------------------------------------------
			if (strcmp(ListTag,"BLENDTYPE")==0) 
			{
				uint32 sum = nodeList->haveAttr("type");
				assert(sum);
				if (sum)
				{
					const char *type = nodeList->getAttr("type");
					rGlobalAnim.m_nBlendCodeLMG = *(uint32*)type;
				}
				continue;
			}

			if (strcmp(ListTag,"CAPS")==0) 
			{
				uint32 sum = nodeList->haveAttr("code");
				assert(sum);
				if (sum)
				{
					const char *code = nodeList->getAttr("code");
					rGlobalAnim.m_nSelectionCapsCode = *(uint32*)code;
				}
				continue;
			}

			//-----------------------------------------------------------
			//-- check of Style-Transfer examples 
			//-----------------------------------------------------------
			if ( strcmp(ListTag,"StyleTransfer")==0 ) 
			{
				uint32 num = nodeList->getChildCount();
				for (uint32 i=0; i<num; i++) 
				{
					XmlNodeRef nodeExample = nodeList->getChild(i);
					const char* ExampleTag = nodeExample->getTag();

					if (strcmp(ExampleTag,"OldStyle")==0) 
					{
						rGlobalAnim.m_strOldStyle=nodeExample->getAttr( "Style" );
						continue;
					}

					if (strcmp(ExampleTag,"NewStyle")==0) 
					{
						//const char* test1 = rGlobalAnim.m_strNewStyle;
						rGlobalAnim.m_strNewStyle=nodeExample->getAttr( "Style" );

						//const char* test2 = rGlobalAnim.m_strNewStyle;
						//uint32 test3 = test1[0];
						continue;
					}

					assert(0); //illegal Tag found
					return 0;
				}
				continue;
			}


			//-----------------------------------------------------------
			//-- check of Motion-Combination examples 
			//-----------------------------------------------------------
			if ( strcmp(ListTag,"MotionCombination")==0 ) 
			{
				uint32 num = nodeList->getChildCount();
				rGlobalAnim.m_strSpliceAnim.resize(num);
				for (uint32 i=0; i<num; i++) 
				{
					XmlNodeRef nodeExample = nodeList->getChild(i);
					const char* ExampleTag = nodeExample->getTag();

					if (strcmp(ExampleTag,"NewStyle")==0) 
					{
						const char* test1 = rGlobalAnim.m_strNewStyle;
						rGlobalAnim.m_strLayerAnim=nodeExample->getAttr( "Style" );

					  const char* test2 = rGlobalAnim.m_strLayerAnim;
					  uint32 test3 = test1[0];
						continue;
					}

					assert(0); //illegal Tag found
					return 0;
				}
				continue;
			}



		}

		assert(rGlobalAnim.m_nBlendCodeLMG);
	}
	return 1;
}




uint32 CAnimationSet::numMorphTargets() const  
{
	CModelMesh* pModelMesh = m_pModel->GetModelMesh(0);
	if (pModelMesh==0)
		return 0;

	return pModelMesh->m_morphTargets.size();
};

const char* CAnimationSet::GetNameMorphTarget (int nMorphTargetId) 
{
	if (nMorphTargetId< 0)
		return "!NEGATIVE MORPH TARGET ID!";

	if ((uint32)nMorphTargetId >= m_pModel->GetModelMesh(0)->m_morphTargets.size())
		return "!MORPH TARGET ID OUT OF RANGE!";

	return m_pModel->GetModelMesh(0)->m_morphTargets[nMorphTargetId]->m_name.c_str();
};



// prepares to load the specified number of CAFs by reserving the space for the controller pointers
void CAnimationSet::prepareLoadCAFs (uint32 nReserveAnimations)
{
	nReserveAnimations += m_arrAnimations.size();

	//uint32 numJoints = m_pModel->m_pModelSkeleton->m_arrModelJoints.size();
	//for (uint32 i=0; i<numJoints; i++)
	//	m_pModel->m_pModelSkeleton->m_arrModelJoints[i].m_arrControllersMJoint.reserve (nReserveAnimations);

	m_arrAnimations.reserve (nReserveAnimations);
//	m_arrAnimByGlobalId.reserve (nReserveAnimations);
	//	m_arrAnimByLocalName.reserve (nReserveAnimations);
}


void CAnimationSet::selfValidate()
{
//#ifdef _DEBUG
//	assert (m_arrAnimByGlobalId.size() == m_arrAnimations.size());
//	//	assert (m_arrAnimByLocalName.size() == m_arrAnimations.size());
//	for (unsigned i = 0; i < m_arrAnimByGlobalId.size()-1; ++i)
//	{
//		assert (m_arrAnimations[m_arrAnimByGlobalId[i].nAnimId].m_nGlobalAnimId <= m_arrAnimations[m_arrAnimByGlobalId[i+1].nAnimId].m_nGlobalAnimId);
//		//		assert (stricmp(m_arrAnimations[m_arrAnimByLocalName[i]].m_strAnimName.c_str(), m_arrAnimations[m_arrAnimByLocalName[i+1]].m_strAnimName.c_str())<0);
//	}
//#endif
}



const ModelAnimationHeader* CAnimationSet::GetModelAnimationHeader(int32 i)
{
	DEFINE_PROFILER_FUNCTION();

	int32 numAnimation = (int32)m_arrAnimations.size();
	if (i<0 || i>=numAnimation)
		return 0;

	int32 GlobalID = m_arrAnimations[i].m_nGlobalAnimId;

	//check if the animation-asset is really in memory
	uint32 AnimLoaded = g_AnimationManager.m_arrGlobalAnimations[GlobalID].IsAssetLoaded();
	AnimLoaded |= g_AnimationManager.m_arrGlobalAnimations[GlobalID].IsAimposeUnloaded();
	

	if(AnimLoaded)
		return &m_arrAnimations[i];  //the animation-asset exists
	else
	{

		uint32 AnimOnDemand = g_AnimationManager.m_arrGlobalAnimations[GlobalID].IsAssetOnDemand();
		// todo!
		if (AnimOnDemand)
			return &m_arrAnimations[i];
		//there is an animation name in the CAL-file, but the animation-asset does not exist
#if !defined(PS3)
		// FIXME: no animations for PS3 - avoid console flooding
		AnimFileWarning(m_pModel->GetModelFilePath(),"asset for animation-name '%s' not available", m_arrAnimations[i].GetAnimName());
#endif
		return 0;  
	}
}




//----------------------------------------------------------------------------------
// Returns the index of the animation in the set, -1 if there's no such animation
//----------------------------------------------------------------------------------
int CAnimationSet::GetAnimIDByName( const char* szAnimationName)
{
	if (szAnimationName==0)
		return -1;

	//this is probably the slowest function in the system.
	//TODO: needs some heavy optimisation in the future 
	if (szAnimationName[0] == '#')
	{	
		//search for morph-names
		return int(m_arrAnimations.size() + m_pModel->m_arrModelMeshes[0].FindMorphTarget(szAnimationName));
	}
	else
	{
		//search for animation-names 
		/*
		uint32 numAnims=m_arrAnimations.size();
		for (uint32 i=0; i<numAnims; i++)
		{
		if (stricmp(m_arrAnimations[i].m_strAnimName.c_str(),szAnimationName)==0)
		return i;
		}
		*/

		return m_AnimationHashMap.GetValue(szAnimationName);
	}
}


// Returns the given animation name
const char* CAnimationSet::GetNameByAnimID ( int nAnimationId)
{
	if (m_pModel->m_ObjectType==CGA)
	{
		if (nAnimationId < 0)
			return "";
		return m_arrAnimations[nAnimationId].GetAnimName();
	} 
	else
	{
		if (nAnimationId >=0)
		{
			if (nAnimationId < (int)m_arrAnimations.size())
				return m_arrAnimations[nAnimationId].GetAnimName();
			nAnimationId -= (int)m_arrAnimations.size();
			if (nAnimationId < (int)m_pModel->m_arrModelMeshes[0].m_morphTargets.size())
				return m_pModel->m_arrModelMeshes[0].m_morphTargets[nAnimationId]->m_name.c_str();
			return "!ANIMATION ID OUT OF RANGE!";
		}
		else
			return "!NEGATIVE ANIMATION ID!";
	}

}

//------------------------------------------------------------------------------

f32 CAnimationSet::GetSpeed(int nAnimationId)
{
	int32 numAnimation = (int32)m_arrAnimations.size();
	if (nAnimationId<0 || nAnimationId>=numAnimation)
	{
		AnimFileWarning(m_pModel->GetModelFilePath(),"illegal animation index used in function GetSpeed '%d'", nAnimationId);
		return -1;
	}

	const ModelAnimationHeader* anim = GetModelAnimationHeader(nAnimationId);
	if (anim==0)
		return -1;

	uint32 GlobalAnimationID			= anim->m_nGlobalAnimId;
	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[GlobalAnimationID];
	f32	fDistance = rGlobalAnimHeader.m_fDistance;
	f32	fDuration = rGlobalAnimHeader.m_fEndSec - rGlobalAnimHeader.m_fStartSec;
	f32	msec      = fDistance/fDuration;
	return msec;
}

f32 CAnimationSet::GetSlope(int nAnimationId)
{
	int32 numAnimation = (int32)m_arrAnimations.size();
	if (nAnimationId<0 || nAnimationId>=numAnimation)
	{
		AnimFileWarning(m_pModel->GetModelFilePath(),"illegal animation index used in function GetSpeed '%d'", nAnimationId);
		return 0;
	}

	const ModelAnimationHeader* anim = GetModelAnimationHeader(nAnimationId);
	if (anim==0)
		return 0;

	uint32 GlobalAnimationID			= anim->m_nGlobalAnimId;
	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[GlobalAnimationID];
	return rGlobalAnimHeader.m_fSlope;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

GlobalAnimationHeader* CAnimationSet::GetGlobalAnimationHeaderFromLocalName(const char* AnimationName)
{ 
	int32 subAnimID = GetAnimIDByName(AnimationName);
	if (subAnimID<0)
		return 0; //error -> name not found
	return GetGlobalAnimationHeaderFromLocalID(subAnimID);
}

GlobalAnimationHeader* CAnimationSet::GetGlobalAnimationHeaderFromLocalID(int nAnimationId)
{ 

	int32 numAnimation = (int32)m_arrAnimations.size();
	if ((nAnimationId < 0) || (nAnimationId >= numAnimation))
	{
		AnimFileWarning(m_pModel->GetModelFilePath(),"illegal animation index '%d'", nAnimationId);
		return NULL;
	}

	const ModelAnimationHeader* anim = GetModelAnimationHeader(nAnimationId);
	if (anim == NULL)
		return NULL;

	uint32 GlobalAnimationID			= anim->m_nGlobalAnimId;
	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[GlobalAnimationID];

	return &rGlobalAnimHeader;
}




//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------
IdleStepWeights6 CAnimationSet::GetIdleStepWeights6(   Vec2 FR,Vec2 RR,Vec2 BR,   Vec2 FL,Vec2 LL,Vec2 BL,  Vec2 DesiredDirection  ) 
{
	f32 fr=0,rr=0,br=0;
	f32 fl=0,ll=0,bl=0;
	f32 t	=	0.5f;
	f32 step=0.55f;

	f32 length_FR = FR.GetLength();
	f32 length_FL = FL.GetLength();
	Vec2 F = (FR+FL).GetNormalizedSafe();
	FR	=	F*length_FR;
	FL	=	F*length_FL;

	f32 length_BR = BR.GetLength();
	f32 length_BL = BL.GetLength();
	Vec2 B = (BR+BL).GetNormalizedSafe();
	BR	=	B*length_BR;
	BL	=	B*length_BL;

	uint32 limit=0x100;

	f32 dotFR = Ang3::CreateRadZ(FR,DesiredDirection);
	f32 dotRR = Ang3::CreateRadZ(RR,DesiredDirection);
	f32 dotBR = Ang3::CreateRadZ(BR,DesiredDirection);
	f32 dotFL = Ang3::CreateRadZ(FL,DesiredDirection);
	f32 dotLL = Ang3::CreateRadZ(LL,DesiredDirection);
	f32 dotBL = Ang3::CreateRadZ(BL,DesiredDirection);

	int32 QuadrantFRRR=0;
	if (dotFR<=0 && dotRR>=0)
		QuadrantFRRR=1;
	int32 QuadrantBRRR=0;
	if (dotBR>=0 && dotRR<=0)
		QuadrantBRRR=1;

	int32 QuadrantFLLL=0;
	if (dotFL>=0 && dotLL<=0)
		QuadrantFLLL=1;
	int32 QuadrantBLLL=0;
	if (dotBL<=0 && dotLL>=0)
		QuadrantBLLL=1;

	assert( QuadrantFRRR + QuadrantBRRR + QuadrantFLLL + QuadrantBLLL );

	f32 dot;

	if (QuadrantFRRR)
	{
		for (uint32 i=0; i<limit; i++)
		{
			fr=1.0f-t; 	rr=t;
			dot = Ang3::CreateRadZ(DesiredDirection,FR*fr+RR*rr);
			if ( fabs(dot)<0.00001 ) break;
			t+=step*sgn(dot);	step*=0.6f;
		}
		return IdleStepWeights6( f32(fr),f32(rr),f32(br), f32(fl),f32(ll),f32(bl) );
	}



	if (QuadrantFLLL)
	{
		for (uint32 i=0; i<limit; i++)
		{
			fl=1.0f-t;	ll=t;
			dot = Ang3::CreateRadZ(FL*fl+LL*ll,DesiredDirection);
			if ( fabs(dot)<0.00001 ) break;
			t+=step*sgn(dot); step*=0.6f;
		}
		return IdleStepWeights6( f32(fr),f32(rr),f32(br), f32(fl),f32(ll),f32(bl) );
	}


	if (QuadrantBRRR)
	{
		for (uint32 i=0; i<limit; i++)
		{
			br=1.0f-t;	rr=t;
			dot = Ang3::CreateRadZ(BR*br+RR*rr,DesiredDirection);
			if ( fabs(dot)<0.00001 )	break;
			t+=step*sgn(dot); step*=0.6f;
		}
		return IdleStepWeights6( f32(fr),f32(rr),f32(br), f32(fl),f32(ll),f32(bl) );
	}


	if (QuadrantBLLL)
	{
		for (uint32 i=0; i<limit; i++)
		{
			bl=1.0f-t;	ll=t;
			dot = Ang3::CreateRadZ(DesiredDirection,BL*bl+LL*ll);
			if ( fabs(dot)<0.00001 ) break;
			t+=step*sgn(dot); step*=0.6f;
		}
		return IdleStepWeights6( f32(fr),f32(rr),f32(br), f32(fl),f32(ll),f32(bl) );
	}

	return IdleStepWeights6(0,0,0,0,0,0);
}



//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

IdleStepWeights6 CAnimationSet::GetIdle2MoveWeights6(  Vec2 DesiredDirection  ) 
{
	IdleStepWeights6 I2M;
	I2M.fr=0.0f; I2M.rr=0.0f; I2M.br=0.0f;
	I2M.fl=0.0f; I2M.ll=0.0f; I2M.bl=0.0f;

	f32 rad = Ang3::CreateRadZ(Vec2(0,1),DesiredDirection);
	if (rad<0)
	{
		if (rad>=(-gf_PI*0.5f))
		{
			//forward-right quadrant
			f32 t = -(rad/(gf_PI*0.5f));
			I2M.fr=1-t;	I2M.rr=t;	return I2M;
		}
		else
		{
			//right-backward quadrant
			rad = rad+(gf_PI*0.5f);
			f32 t = -(rad/(gf_PI*0.5f));
			I2M.rr=1-t;	I2M.br=t;	return I2M;
		}
	}
	else
	{
		if (rad<=(gf_PI*0.5f))
		{
			//forward-left quadrant
			f32 t = rad/(gf_PI*0.5f);
			I2M.fl=1-t;	I2M.ll=t;	return I2M;
		}
		else
		{
			//left-backward quadrant
			rad = rad-(gf_PI*0.5f);
			f32 t = rad/(gf_PI*0.5f);
			I2M.ll=1-t;	I2M.bl=t;	return I2M;
		}
	}

	assert(0);
	return I2M;
}


void CAnimationSet::GetStrafeWeights(const Vec2& F, const Vec2& R, const Vec2& W, float& f, float& r)
{
	float det = F.x * R.y - F.y * R.x;
	float det1 = W.x * R.y - W.y * R.x;
	float det2 = F.x * W.y - F.y * W.x;

	f = det1/det;
	r = det2/det;
	f = f > 0.0f ? f : 0.5f;
	r = r > 0.0f ? r : 0.5f;

	float sum = 1.0f/(f + r);
	f *= sum;
	r *= sum;
}

//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------
StrafeWeights4 CAnimationSet::GetStrafingWeights4( Vec2 F, Vec2 L, Vec2 B, Vec2 R, Vec2 DesiredDirection ) 
{
	
	f32 length=(DesiredDirection|DesiredDirection);
	if (length==0.0f)
		return StrafeWeights4(0.25f,0.25f,0.25f,0.25f);
	
	f32 f=0,r=0,l=0,b=0;
	f32 dotF	= Ang3::CreateRadZ(F,DesiredDirection);
	f32 dotL	= Ang3::CreateRadZ(L,DesiredDirection);
	f32 dotB	= Ang3::CreateRadZ(B,DesiredDirection);
	f32 dotR	= Ang3::CreateRadZ(R,DesiredDirection);

	if (dotF>=0 && dotL<=0)
	{
		GetStrafeWeights(F, L, DesiredDirection, f, l);
		return StrafeWeights4(f,l,b,r);
	}

	if (dotB>=0 && dotR<=0)
	{
		GetStrafeWeights(B, R, DesiredDirection, b, r);
		return StrafeWeights4(f,l,b,r);
	}

	if (dotB<=0 && dotL>=0)
	{
		GetStrafeWeights(B, L, DesiredDirection, b, l);
		return StrafeWeights4(f,l,b,r);
	}

	if (dotF<=0 && dotR>=0)
	{
		GetStrafeWeights(F, R, DesiredDirection, f, r);
		return StrafeWeights4(f,l,b,r);
	}

	return StrafeWeights4(0.25f,0.25f,0.25f,0.25f);
}



//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------
StrafeWeights6 CAnimationSet::GetStrafingWeights6( Vec2 F,Vec2 FL,Vec2 L,  Vec2 B,Vec2 BR,Vec2 R, const Vec2& DesiredDirection   ) 
{

	f32 d6=1.0f/6.0f;
	f32 length=(DesiredDirection|DesiredDirection);
	if (length==0.0f)
		return StrafeWeights6(d6,d6,d6,d6,d6,d6);

	f32 f=0,fl=0,l=0, b=0,br=0,r=0;
	f32 dotF	= Ang3::CreateRadZ(F, DesiredDirection);
	f32 dotFL	= Ang3::CreateRadZ(FL,DesiredDirection);
	f32 dotL	= Ang3::CreateRadZ(L, DesiredDirection);
	f32 dotB	= Ang3::CreateRadZ(B, DesiredDirection);
	f32 dotBR	= Ang3::CreateRadZ(BR,DesiredDirection);
	f32 dotR	= Ang3::CreateRadZ(R, DesiredDirection);

	if (dotF>=0 && dotFL<=0)
	{
		GetStrafeWeights(F, FL, DesiredDirection, f, fl);
		return StrafeWeights6(f,fl,l,b,br,r);
	}
	if (dotFL>=0 && dotL<=0 )
	{
		GetStrafeWeights(FL, L, DesiredDirection, fl, l);
		return StrafeWeights6(f,fl,l,b,br,r);
	}
	if (dotL>=0 && dotB<=0 )
	{
		GetStrafeWeights(L, B,  DesiredDirection, l,  b);
		return StrafeWeights6(f,fl,l,b,br,r);
	}

	if (dotB>=0  && dotBR<=0)
	{
		GetStrafeWeights(B, BR, DesiredDirection, b, br);
		return StrafeWeights6(f,fl,l,b,br,r);
	}
	if (dotBR>=0 && dotR<=0)
	{
		GetStrafeWeights(BR, R, DesiredDirection, br, r);
		return StrafeWeights6(f,fl,l,b,br,r);
	}
	if (dotR>=0  && dotF<=0)
	{
		GetStrafeWeights(R, F,  DesiredDirection, r,  f);
		return StrafeWeights6(f,fl,l,b,br,r);
	}

	return StrafeWeights6(d6,d6,d6,d6,d6,d6);
}






//------------------------------------------------------------------------------

Vec2 CAnimationSet::GetMinMaxSpeedAsset_msec(int32 animID )
{ 
	Vec2 MinMax(0,0);
	if (animID<0)
		return MinMax;

	GlobalAnimationHeader* rGlobalAnimHeader = GetGlobalAnimationHeaderFromLocalID(animID);
	if (rGlobalAnimHeader==0)
	{
		AnimFileWarning(m_pModel->GetModelFilePath(),"CryAnimation\\CAnimationSet::GetMinMaxSpeed_msec(): Animation %d is not valid", animID);
		return MinMax;
	}

	if (!rGlobalAnimHeader->IsAssetLoaded())
	{
		AnimFileWarning(m_pModel->GetModelFilePath(),"CryAnimation\\CAnimationSet::GetMinMaxSpeed_msec(): Animation %d is not loaded", animID);
		return MinMax;
	}

	int type = 0; // 0==unloaded, 1==regular, 2==lmg
	if (rGlobalAnimHeader->IsAssetLMG())
		type = 2;
	else
		type = 1;

	if (type == 1)
		return Vec2(rGlobalAnimHeader->m_fSpeed,rGlobalAnimHeader->m_fSpeed); 

	if (type == 2)
	{
		f32 minspeed=+9999.0f;
		f32 maxspeed=-9999.0f;
		uint32 subCount = rGlobalAnimHeader->m_arrBSAnimations.size();
		assert(subCount < 40);
		for (uint32 i = 0; i < subCount; i++)
		{
			const char* aname = rGlobalAnimHeader->m_arrBSAnimations[i].m_strAnimName;
			if (aname==0)
				return Vec2(0,0); 

			int32 subAnimID = GetAnimIDByName(aname);
			if (subAnimID<0)
				return Vec2(0,0); 

			GlobalAnimationHeader* rsubGlobalAnimHeader = GetGlobalAnimationHeaderFromLocalID(subAnimID);
			if ((rsubGlobalAnimHeader == 0) || rsubGlobalAnimHeader->IsAssetLMGValid()==0)
				return Vec2(0,0); 

			if (minspeed>rsubGlobalAnimHeader->m_fSpeed) 
				minspeed=rsubGlobalAnimHeader->m_fSpeed;
			if (maxspeed<rsubGlobalAnimHeader->m_fSpeed) 
				maxspeed=rsubGlobalAnimHeader->m_fSpeed;
		}
		return Vec2(minspeed,maxspeed); 
	}

	return MinMax; 
}

ILINE f32 Turn2BlendWeight(f32 fDesiredTurnSpeed)
{
	f32 sign = f32(sgn(-fDesiredTurnSpeed));
	f32 tb=2.37f/gf_PI;
	f32 turn_bw = fabsf( (fDesiredTurnSpeed/gf_PI)*tb ) - 0.01f;;
	//	f32 turn_bw = fabsf( fDesiredTurnSpeed/(gf_PI*0.70f) ) - 0.0f;;
	if (turn_bw> 1.0f)	turn_bw=1.0f;
	if (turn_bw< 0.0f)	turn_bw=0.00001f;
	return turn_bw*sign;
}

LMGCapabilities CAnimationSet::GetLMGPropertiesByName( const char* szAnimName, Vec2& vStrafeDirection, f32 fDesiredTurn, f32 fSlope  )
{
	LMGCapabilities lmg_caps;

	int32 nAnimID = GetAnimIDByName( szAnimName );
	if (nAnimID<0)	
	{
		AnimFileWarning(m_pModel->GetModelFilePath(),"animation-name '%s' not in CAL-file", szAnimName );
		return lmg_caps;
	}
	//check if the asset exists
	const ModelAnimationHeader* pAnim = GetModelAnimationHeader(nAnimID);
	if (pAnim==0)	
	{
		AnimFileWarning(m_pModel->GetModelFilePath(),"animation-asset '%s' not loaded", szAnimName);
		return lmg_caps;
	}



	SLocoGroup lmg = BuildRealTimeLMG( pAnim,nAnimID);

/*
	f32 m_turn=0;
	if (lmg.m_params[eMotionParamID_TurnSpeed].initialized)
		m_turn = 1.0f - 2.0f * lmg.m_params[eMotionParamID_TurnSpeed].blendspace.m_fAssetBlend;
	else if (lmg.m_params[eMotionParamID_TurnAngle].initialized)
		m_turn = 1.0f - 2.0f * lmg.m_params[eMotionParamID_TurnAngle].blendspace.m_fAssetBlend;
*/

	lmg.m_BlendSpace.m_strafe	= vStrafeDirection;
	lmg.m_BlendSpace.m_turn		=	Turn2BlendWeight(fDesiredTurn);
	lmg.m_BlendSpace.m_slope	= fSlope;
	lmg.m_BlendSpace.m_speed	= 0.0f; // Not sure this is needed/used by GetLMGCaps, but it should be cheap enough to do.

	lmg.m_params[eMotionParamID_TurnSpeed].value = fDesiredTurn;

//	float fColor[4] = {1,1,0,1};
//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"lmg.m_BlendSpace.m_turn: %f     m_turn: %f",lmg.m_BlendSpace.m_turn,m_turn); 
//	g_YLine+=0x20;

	lmg_caps = GetLMGCapabilities( lmg );
	return lmg_caps;
}

SLocoGroup CAnimationSet::BuildRealTimeLMG( const ModelAnimationHeader* pAnim0,int nID)
{
	SLocoGroup lmg;

	int32 nAnimID[MAX_LMG_ANIMS]; 
	const ModelAnimationHeader* pAnimHeaders[MAX_LMG_ANIMS];

	uint32 nGlobalAnimId = pAnim0->m_nGlobalAnimId;
	GlobalAnimationHeader& rGlobalAnimHeader0 = g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimId];
	if (rGlobalAnimHeader0.IsAssetLMG())
	{
		lmg.m_nLMGID				= nID;
		lmg.m_nGlobalLMGID	= nGlobalAnimId;
		//CRC32
	//	lmg.m_nCRC32GlobalLMGID = rGlobalAnimHeader0.GetCRCPathName();
		lmg.m_numAnims			=	SetLoconames(rGlobalAnimHeader0, &nAnimID[0],&pAnimHeaders[0]);
		for (int32 i=0; i<lmg.m_numAnims; i++)
		{
			if (pAnimHeaders[i])
			{
				lmg.m_nAnimID[i]		=	nAnimID[i];
				lmg.m_nGlobalID[i]	=	pAnimHeaders[i]->m_nGlobalAnimId;

				lmg.m_nSegmentCounter[i]	=0;

				GlobalAnimationHeader& rGAH = g_AnimationManager.m_arrGlobalAnimations[pAnimHeaders[i]->m_nGlobalAnimId];
				//CRC32
	//			lmg.m_nCRC32GlobalID[i] = rGAH.GetCRCPathName();
				// Ivo I've added also GlobalAnimationHeader& CAnimationManager::GlobalAnimationHeaderFromCRC(uint32 crc) 
				lmg.m_fDurationQQQ[i]	=	rGAH.m_fTotalDuration;  assert(rGAH.m_fTotalDuration>0);

				lmg.m_fFootPlants[i]	=	0.0f;
				uint32 numPoses = rGAH.m_FootPlantBits.size();
				for (uint32 f=0; f<numPoses; f++)
				{
					if (rGAH.m_FootPlantBits[f])
						lmg.m_fFootPlants[i]	=	1.0f;
				}

			}	else assert(0);
		}
	}
	else
	{
		lmg.m_nAnimID[0]		=	nID;
		lmg.m_nGlobalID[0]	=	pAnim0->m_nGlobalAnimId;
		lmg.m_numAnims			=	1;
		lmg.m_nSegmentCounter[0]	=0;
		GlobalAnimationHeader& rGAH = g_AnimationManager.m_arrGlobalAnimations[pAnim0->m_nGlobalAnimId];
		lmg.m_fDurationQQQ[0]	=	rGAH.m_fTotalDuration;  assert(rGAH.m_fTotalDuration>0);

		lmg.m_fFootPlants[0]	=	0.0f;
		uint32 numPoses = rGAH.m_FootPlantBits.size();
		for (uint32 f=0; f<numPoses; f++)
		{
			uint8 fp=rGAH.m_FootPlantBits[f];
			if (fp)
				lmg.m_fFootPlants[0]	=	1.0f;
		}

	}

	return lmg;
}

//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------

uint32 CAnimationSet::SetLoconames(const GlobalAnimationHeader& rGlobalAnimHeader, int32* nAnimID, const ModelAnimationHeader** pAnim)
{
	uint32 numAssets=0;
	nAnimID[0]=-1; 
	nAnimID[1]=-1;
	nAnimID[2]=-1; 

	nAnimID[3]=-1;
	nAnimID[4]=-1; 
	nAnimID[5]=-1;

	nAnimID[6]=-1;
	nAnimID[7]=-1;
	nAnimID[8]=-1;

	uint32 num=0;
	if (rGlobalAnimHeader.IsAssetLMG())
	{
		num = rGlobalAnimHeader.m_arrBSAnimations.size();
		for (uint32 i=0; i<num; i++)
		{
			nAnimID[i] = GetAnimIDByName( rGlobalAnimHeader.m_arrBSAnimations[i].m_strAnimName );
			pAnim[i] = GetModelAnimationHeader(nAnimID[i]);
		}
	}

	return num;
}



//------------------------------------------------------------------------------
LMGCapabilities CAnimationSet::GetLMGCapabilities( const SLocoGroup& lmg )
{ 
	static f32 proceduralTurnSpeedChangeRate = DEG2RAD(360.0f) * 4.0f;
	static f32 curvingTurnSpeedChangeRate = proceduralTurnSpeedChangeRate;

	curvingTurnSpeedChangeRate = proceduralTurnSpeedChangeRate;
	if (Console::GetInst().ca_GameControlledStrafing == 0)
		curvingTurnSpeedChangeRate *= 0.25f;

//	float fColor[4] = {1,1,0,1};
//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"GetLMGCapabilities"); g_YLine+=0x10;

	struct Property
	{

		static void GetProperties_STF1( CAnimationSet* pAnimationSet, const SLocoGroup& lmg, uint32 offset, LMGCapabilities& lmg_caps )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.2f;

			GlobalAnimationHeader* parrGlobalAnimations = &g_AnimationManager.m_arrGlobalAnimations[0];
			GlobalAnimationHeader& rsubGAH00 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x00] ]; 
			GlobalAnimationHeader& rsubGAH01 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x01] ]; 
			GlobalAnimationHeader& rsubGAH02 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x02] ]; 
			GlobalAnimationHeader& rsubGAH03 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x03] ]; 

			Vec2 sF = Vec2(rsubGAH00.m_vVelocity);
			Vec2 sL = Vec2(rsubGAH01.m_vVelocity);
			Vec2 sB = Vec2(rsubGAH02.m_vVelocity);
			Vec2 sR = Vec2(rsubGAH03.m_vVelocity);
			StrafeWeights4 blend = pAnimationSet->GetStrafingWeights4(sF,sL,sB,sR, lmg.m_BlendSpace.m_strafe);
			Vec2 velocity = sF*blend.f + sL*blend.l + sB*blend.b + sR*blend.r;
			lmg_caps.m_bHasVelocity = 1;
			lmg_caps.m_vMinVelocity	= velocity;
			lmg_caps.m_vMaxVelocity	= velocity;

			f32 duration = rsubGAH00.m_fTotalDuration*blend.f + rsubGAH01.m_fTotalDuration*blend.l  + rsubGAH02.m_fTotalDuration*blend.b  + rsubGAH03.m_fTotalDuration*blend.r;
			lmg_caps.m_fSlowDuration	= duration;
			lmg_caps.m_fFastDuration	= duration;
		}

		static void GetProperties_STF2( CAnimationSet* pAnimationSet, const SLocoGroup& lmg, uint32 offset, LMGCapabilities& lmg_caps )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.2f;

			LMGCapabilities lmg_fast;
			GetProperties_STF1(pAnimationSet,lmg,offset+0,lmg_fast);
			lmg_caps.m_fFastDuration=lmg_fast.m_fFastDuration;
			lmg_caps.m_vMaxVelocity=lmg_fast.m_vMaxVelocity;

			LMGCapabilities lmg_slow;
			GetProperties_STF1(pAnimationSet,lmg,offset+4,lmg_slow);
			lmg_caps.m_fSlowDuration=lmg_slow.m_fSlowDuration;
			lmg_caps.m_bHasVelocity = 1;
			lmg_caps.m_vMinVelocity=lmg_slow.m_vMinVelocity;
		}



		static void GetProperties_S1( CAnimationSet* pAnimationSet, const SLocoGroup& lmg, uint32 offset, LMGCapabilities& lmg_caps )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.2f;

			GlobalAnimationHeader* parrGlobalAnimations = &g_AnimationManager.m_arrGlobalAnimations[0];
			GlobalAnimationHeader& rsubGAH00 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x00] ]; 
			GlobalAnimationHeader& rsubGAH01 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x01] ]; 
			GlobalAnimationHeader& rsubGAH02 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x02] ]; 
			GlobalAnimationHeader& rsubGAH03 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x03] ]; 
			GlobalAnimationHeader& rsubGAH04 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x04] ]; 
			GlobalAnimationHeader& rsubGAH05 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x05] ]; 

			Vec2 sF		= Vec2(rsubGAH00.m_vVelocity);
			Vec2 sFL	= Vec2(rsubGAH01.m_vVelocity);
			Vec2 sL		= Vec2(rsubGAH02.m_vVelocity);
			Vec2 sB		= Vec2(rsubGAH03.m_vVelocity);
			Vec2 sBR	= Vec2(rsubGAH04.m_vVelocity);
			Vec2 sR		= Vec2(rsubGAH05.m_vVelocity);
			assert( lmg.m_BlendSpace.m_strafe.IsValid() );

			f32 length = lmg.m_BlendSpace.m_strafe.GetLength();
		//	if (length==0)
		//		lmg.m_BlendSpace.m_strafe=Vec2(0,0);

			StrafeWeights6 blend = pAnimationSet->GetStrafingWeights6(sF,sFL,sL, sB,sBR,sR, lmg.m_BlendSpace.m_strafe );
			Vec2 velocity = (sF*blend.f)+(sFL*blend.fl)+(sL*blend.l)+(sB*blend.b)+(sBR*blend.br)+(sR*blend.r);

			f32 dF		= rsubGAH00.m_fTotalDuration;
			f32 dFL		= rsubGAH01.m_fTotalDuration;
			f32 dL		= rsubGAH02.m_fTotalDuration;
			f32 dB		= rsubGAH03.m_fTotalDuration;
			f32 dBR		= rsubGAH04.m_fTotalDuration;
			f32 dR		= rsubGAH05.m_fTotalDuration;
			f32 duration = (dF*blend.f)+(dFL*blend.fl)+(dL*blend.l)+(dB*blend.b)+(dBR*blend.br)+(dR*blend.r);

			lmg_caps.m_fSlowDuration	= duration;
			lmg_caps.m_fFastDuration	= duration;
			lmg_caps.m_bHasVelocity = 1;
			lmg_caps.m_vMinVelocity	= velocity;
			lmg_caps.m_vMaxVelocity	= velocity;
		}

		static void GetProperties_S2( CAnimationSet* pAnimationSet, const SLocoGroup& lmg, uint32 offset, LMGCapabilities& lmg_caps )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.2f;

			LMGCapabilities lmg_fast;
			GetProperties_S1(pAnimationSet,lmg,offset+0,lmg_fast);
			lmg_caps.m_fFastDuration=lmg_fast.m_fFastDuration;
			lmg_caps.m_vMaxVelocity=lmg_fast.m_vMaxVelocity;

			LMGCapabilities lmg_slow;
			GetProperties_S1(pAnimationSet,lmg,offset+6,lmg_slow);
			lmg_caps.m_fSlowDuration=lmg_slow.m_fSlowDuration;
			lmg_caps.m_vMinVelocity=lmg_slow.m_vMinVelocity;
		}


		static void GetProperties_ST2( CAnimationSet* pAnimationSet, const SLocoGroup& lmg, uint32 offset, LMGCapabilities& lmg_caps )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.2f;

			lmg_caps.m_bHasCurving = 1;
			lmg_caps.m_curvingChangeRate = 2.0f;
			lmg_caps.m_turnSpeedChangeRate = LERP(proceduralTurnSpeedChangeRate, curvingTurnSpeedChangeRate, 
																			lmg.m_params[eMotionParamID_Curving].value);

			GlobalAnimationHeader* parrGlobalAnimations = &g_AnimationManager.m_arrGlobalAnimations[0];

			GlobalAnimationHeader& rsubGAH00 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x00] ]; 
			GlobalAnimationHeader& rsubGAH01 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x01] ]; 
			GlobalAnimationHeader& rsubGAH02 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x02] ]; 
			GlobalAnimationHeader& rsubGAH03 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x03] ]; 
			GlobalAnimationHeader& rsubGAH04 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x04] ]; 
			GlobalAnimationHeader& rsubGAH05 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x05] ]; 

			GlobalAnimationHeader& rsubGAH07 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x07] ]; 
			GlobalAnimationHeader& rsubGAH08 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x08] ]; 
			GlobalAnimationHeader& rsubGAH09 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x09] ]; 
			GlobalAnimationHeader& rsubGAH0a = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x0a] ]; 
			GlobalAnimationHeader& rsubGAH0b = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x0b] ]; 
			GlobalAnimationHeader& rsubGAH0c = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x0c] ]; 

			GlobalAnimationHeader& rsubGAH0d = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x0d] ]; 
			GlobalAnimationHeader& rsubGAH0e = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x0e] ]; 
			GlobalAnimationHeader& rsubGAH0f = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x0f] ]; 
			GlobalAnimationHeader& rsubGAH10 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x10] ]; 


			Vec2 ssF	= Vec2(rsubGAH00.m_vVelocity);
			Vec2 ssFL	= Vec2(rsubGAH01.m_vVelocity);
			Vec2 ssL	= Vec2(rsubGAH02.m_vVelocity);
			Vec2 ssB	= Vec2(rsubGAH03.m_vVelocity);
			Vec2 ssBR	= Vec2(rsubGAH04.m_vVelocity);
			Vec2 ssR	= Vec2(rsubGAH05.m_vVelocity);
			StrafeWeights6 sblend = pAnimationSet->GetStrafingWeights6(ssF,ssFL,ssL, ssB,ssBR,ssR, lmg.m_BlendSpace.m_strafe );
			Vec3 strafe_velocity_fast = (ssF*sblend.f)+(ssFL*sblend.fl)+(ssL*sblend.l)+(ssB*sblend.b)+(ssBR*sblend.br)+(ssR*sblend.r);
			f32 dsF		= rsubGAH00.m_fTotalDuration;
			f32 dsFL	= rsubGAH01.m_fTotalDuration;
			f32 dsL		= rsubGAH02.m_fTotalDuration;
			f32 dsB		= rsubGAH03.m_fTotalDuration;
			f32 dsBR	= rsubGAH04.m_fTotalDuration;
			f32 dsR		= rsubGAH05.m_fTotalDuration;
			f32 strafe_duration_fast = (dsF*sblend.f)+(dsFL*sblend.fl)+(dsL*sblend.l)+(dsB*sblend.b)+(dsBR*sblend.br)+(dsR*sblend.r);

			Vec2 sfF	= Vec2(rsubGAH07.m_vVelocity);
			Vec2 sfFL	= Vec2(rsubGAH08.m_vVelocity);
			Vec2 sfL	= Vec2(rsubGAH09.m_vVelocity);
			Vec2 sfB	= Vec2(rsubGAH0a.m_vVelocity);
			Vec2 sfBR	= Vec2(rsubGAH0b.m_vVelocity);
			Vec2 sfR	= Vec2(rsubGAH0c.m_vVelocity);
			StrafeWeights6 fblend = pAnimationSet->GetStrafingWeights6(sfF,sfFL,sfL, sfB,sfBR,sfR, lmg.m_BlendSpace.m_strafe );
			Vec3 strafe_velocity_slow = (sfF*fblend.f)+(sfFL*fblend.fl)+(sfL*fblend.l)+(sfB*fblend.b)+(sfBR*fblend.br)+(sfR*fblend.r);
			f32 dfF		= rsubGAH07.m_fTotalDuration;
			f32 dfFL	= rsubGAH08.m_fTotalDuration;
			f32 dfL		= rsubGAH09.m_fTotalDuration;
			f32 dfB		= rsubGAH0a.m_fTotalDuration;
			f32 dfBR	= rsubGAH0b.m_fTotalDuration;
			f32 dfR		= rsubGAH0c.m_fTotalDuration;
			f32 strafe_duration_slow = (dfF*fblend.f)+(dfFL*fblend.fl)+(dfL*fblend.l)+(dfB*fblend.b)+(dfBR*fblend.br)+(dfR*fblend.r);


			//store the asset features in t 2d-vector
			Vec2 fast_FWL(rsubGAH0d.m_fTurnSpeed,rsubGAH0d.m_fSpeed);	Vec2 fast_FW(0,rsubGAH00.m_fSpeed);	Vec2 fast_FWR(rsubGAH0e.m_fTurnSpeed,rsubGAH0e.m_fSpeed);	
			Vec2 slow_FWL(rsubGAH0f.m_fTurnSpeed,rsubGAH0f.m_fSpeed);	Vec2 slow_FW(0,rsubGAH07.m_fSpeed);	Vec2 slow_FWR(rsubGAH10.m_fTurnSpeed,rsubGAH10.m_fSpeed);

			f32 tu=lmg.m_BlendSpace.m_turn;
			f32 L=0,F=0,R=0;
			if (tu<0) { L=-tu; F=1.0f+tu; }	else { F=1.0f-tu; R=tu; }

			f32 fTurnParam = lmg.m_params[eMotionParamID_TurnSpeed].value;
			f32 fMaxL=max(rsubGAH0d.m_fTurnSpeed,rsubGAH0f.m_fTurnSpeed); //left is positive
			f32 fMaxR=min(rsubGAH0e.m_fTurnSpeed,rsubGAH10.m_fTurnSpeed); //right is negative

			if (fTurnParam>fMaxL)
				fTurnParam=fMaxL;
			if (fTurnParam<fMaxR)
				fTurnParam=fMaxR;

		//	float fColor[4] = {1,1,0,1};
		//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"++++++++++++++++++++++++++         L %f   F %f    R %f",L,F,R); g_YLine+=0x10;
		//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"fTurnParam: %f",fTurnParam); g_YLine+=0x10;

			//calculate the speed based on the turning-parameter
			f32 fw_speed_fast = 0; //fast_FWL.y*L + fast_FW.y*F + fast_FWR.y*R;
			f32 fw_speed_slow	= 0; //slow_FWL.y*L + slow_FW.y*F + slow_FWR.y*R;


			f32 a,t;
			Lineseg vertical;
			vertical.start(fTurnParam,-100,0);
			vertical.end(fTurnParam,100,0);
			if (fTurnParam<0)
			{
				//right check
				Lineseg rslow(slow_FW,slow_FWR);
				Intersect::Lineseg_Lineseg2D(vertical,rslow,a,t);
		//		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"tA: %f   tB: %f",a,t); g_YLine+=0x10;
				fw_speed_slow = slow_FW.y*(1-t)+slow_FWR.y*t;

				Lineseg rfast1(fast_FW,fast_FWR);
				Intersect::Lineseg_Lineseg2D(vertical,rfast1,a,t);
		//		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"tA: %f   tB: %f",a,t); g_YLine+=0x10;
				if (t>=0.0f)
					fw_speed_fast = fast_FW.y*(1-t)+fast_FWR.y*t;

			//	Lineseg rfast2(fast_FWR,slow_FWR);
			//	Intersect::Lineseg_Lineseg2D(vertical,rfast2,a,t);
			//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"tA: %f   tB: %f",a,t); g_YLine+=0x10;
			//	if (t>=0.0f)
			//		fw_speed_fast = fast_FWR.y*(1-t)+slow_FWR.y*t;
			}
			else
			{
				//left check
				Lineseg lslow(slow_FW,slow_FWL);
				Intersect::Lineseg_Lineseg2D(vertical,lslow,a,t);
		//		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"tA: %f   tB: %f",a,t); g_YLine+=0x10;
				fw_speed_slow = slow_FW.y*(1-t)+slow_FWL.y*t;

				Lineseg lfast1(fast_FW,fast_FWL);
				Intersect::Lineseg_Lineseg2D(vertical,lfast1,a,t);
		//		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"tA: %f   tB: %f",a,t); g_YLine+=0x10;
				if (t>=0.0f)
					fw_speed_fast = fast_FW.y*(1-t)+fast_FWL.y*t;

		//		Lineseg lfast2(fast_FWL,slow_FWL);
		//		Intersect::Lineseg_Lineseg2D(vertical,lfast2,a,t);
		//		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"tA: %f   tB: %f",a,t); g_YLine+=0x10;
		//		if (t>=0.0f)
		//			fw_speed_fast = fast_FWL.y*(1-t)+slow_FWL.y*t;
			}


			Vec3 turn_velocity_fast = Vec3(0,fw_speed_fast,0);
			Vec3 turn_velocity_slow	= Vec3(0,fw_speed_slow,0);



			f32 fTurnPriority=0;
			if (pAnimationSet->m_CharEditMode)
			{
				Vec2 strafe = lmg.m_BlendSpace.m_strafe.GetNormalizedSafe( Vec2(0,0) );
				f32 rad = atan2(strafe.x,strafe.y);
				fTurnPriority = fabsf( rad*2 );
				if (fTurnPriority>1.0f) fTurnPriority=1.0f;
				fTurnPriority=1.0f-fTurnPriority;
			}
			else
			{
				fTurnPriority = lmg.m_params[eMotionParamID_Curving].value;
			}

			f32 staf = 1.0f-fTurnPriority;
			f32 turn = fTurnPriority;
			lmg_caps.m_vMinVelocity = turn*turn_velocity_slow + staf*strafe_velocity_slow;
			lmg_caps.m_vMaxVelocity = turn*turn_velocity_fast + staf*strafe_velocity_fast;

			{
				lmg_caps.m_fSlowTurnLeft	= turn*rsubGAH0f.m_fTurnSpeed;
				lmg_caps.m_fSlowTurnRight	= turn*rsubGAH10.m_fTurnSpeed;

				lmg_caps.m_fFastTurnLeft	= turn*rsubGAH0d.m_fTurnSpeed;
				lmg_caps.m_fFastTurnRight	= turn*rsubGAH0e.m_fTurnSpeed;

				/*
				f32 lslow=turn*rsubGAH0f.m_fTurnSpeed;
				f32 rslow=turn*rsubGAH10.m_fTurnSpeed;
				f32 lfast=turn*rsubGAH0d.m_fTurnSpeed;
				f32 rfast=turn*rsubGAH0e.m_fTurnSpeed;
				f32 t0=1.0f-speed_blend;
				f32 t1=speed_blend;
				lmg_caps.m_fFastTurnLeft		= lfast*t1 + lslow*t0;
				lmg_caps.m_fFastTurnRight	= rfast*t1 + rslow*t0;   
				*/
			}


			//duration
			f32 fL	= rsubGAH0d.m_fTotalDuration;
			f32 fF	= rsubGAH00.m_fTotalDuration;
			f32 fR	= rsubGAH0e.m_fTotalDuration;
			f32 sL	= rsubGAH0f.m_fTotalDuration;
			f32 sF	= rsubGAH07.m_fTotalDuration;
			f32 sR	= rsubGAH10.m_fTotalDuration;
			f32 turn_duration_fast = fL*L + fF*F + fR*R;
			f32 turn_duration_slow = sL*L + sF*F + sR*R;
			lmg_caps.m_fSlowDuration = turn*turn_duration_slow + staf*strafe_duration_slow;
			lmg_caps.m_fFastDuration = turn*turn_duration_fast + staf*strafe_duration_fast;
		}


		static void GetProperties_ST2X( CAnimationSet* pAnimationSet, const SLocoGroup& lmg, uint32 offset, LMGCapabilities& lmg_caps )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.2f;

			GlobalAnimationHeader* parrGlobalAnimations = &g_AnimationManager.m_arrGlobalAnimations[0];

			GlobalAnimationHeader& rsubGAH00 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x00] ]; 
			GlobalAnimationHeader& rsubGAH01 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x01] ]; 
			GlobalAnimationHeader& rsubGAH02 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x02] ]; 
			GlobalAnimationHeader& rsubGAH03 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x03] ]; 

			GlobalAnimationHeader& rsubGAH05 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x05] ]; 
			GlobalAnimationHeader& rsubGAH06 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x06] ]; 
			GlobalAnimationHeader& rsubGAH07 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x07] ]; 
			GlobalAnimationHeader& rsubGAH08 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x08] ]; 

			GlobalAnimationHeader& rsubGAH09 = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x09] ]; 
			GlobalAnimationHeader& rsubGAH0a = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x0a] ]; 
			GlobalAnimationHeader& rsubGAH0b = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x0b] ]; 
			GlobalAnimationHeader& rsubGAH0c = parrGlobalAnimations[ lmg.m_nGlobalID[offset+0x0c] ]; 


			Vec2 ssF	= Vec2(rsubGAH00.m_vVelocity);
			Vec2 ssL	= Vec2(rsubGAH01.m_vVelocity);
			Vec2 ssB	= Vec2(rsubGAH02.m_vVelocity);
			Vec2 ssR	= Vec2(rsubGAH03.m_vVelocity);
			StrafeWeights4 sblend = pAnimationSet->GetStrafingWeights4(ssF,ssL, ssB,ssR, lmg.m_BlendSpace.m_strafe );
			Vec3 strafe_velocity_fast = (ssF*sblend.f)+(ssL*sblend.l)+(ssB*sblend.b)+(ssR*sblend.r);
			f32 dsF		= rsubGAH00.m_fTotalDuration;
			f32 dsL		= rsubGAH01.m_fTotalDuration;
			f32 dsB		= rsubGAH02.m_fTotalDuration;
			f32 dsR		= rsubGAH03.m_fTotalDuration;
			f32 strafe_duration_fast = (dsF*sblend.f)+(dsL*sblend.l)+(dsB*sblend.b)+(dsR*sblend.r);

			Vec2 sfF	= Vec2(rsubGAH05.m_vVelocity);
			Vec2 sfL	= Vec2(rsubGAH06.m_vVelocity);
			Vec2 sfB	= Vec2(rsubGAH07.m_vVelocity);
			Vec2 sfR	= Vec2(rsubGAH08.m_vVelocity);
			StrafeWeights4 fblend = pAnimationSet->GetStrafingWeights4(sfF,sfL, sfB,sfR, lmg.m_BlendSpace.m_strafe );
			Vec3 strafe_velocity_slow = (sfF*fblend.f)+(sfL*fblend.l)+(sfB*fblend.b)+(sfR*fblend.r);
			f32 dfF		= rsubGAH05.m_fTotalDuration;
			f32 dfL		= rsubGAH06.m_fTotalDuration;
			f32 dfB		= rsubGAH07.m_fTotalDuration;
			f32 dfR		= rsubGAH08.m_fTotalDuration;
			f32 strafe_duration_slow = (dfF*fblend.f)+(dfL*fblend.l)+(dfB*fblend.b)+(dfR*fblend.r);



			f32 fast_FWL = rsubGAH09.m_fSpeed;	//fast left
			f32 fast_FW  = rsubGAH00.m_fSpeed;	//fast
			f32 fast_FWR = rsubGAH0a.m_fSpeed;	//fast right
			f32 slow_FWL = rsubGAH0b.m_fSpeed;	//slow left
			f32 slow_FW	 = rsubGAH05.m_fSpeed;	//slow
			f32 slow_FWR = rsubGAH0c.m_fSpeed;	//slow right
			f32 t=lmg.m_BlendSpace.m_turn;
			f32 L=0,F=0,R=0;
			if (t<0) { L=-t; F=1.0f+t; }	else { F=1.0f-t; R=t; }
			//float fColor[4] = {1,1,0,1};
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"++++++++++++++++++++++++++         L %f   F %f    R %f",L,F,R); g_YLine+=0x10;

			f32 fw_speed_fast = fast_FWL*L + fast_FW*F + fast_FWR*R;
			f32 fw_speed_slow	= slow_FWL*L + slow_FW*F + slow_FWR*R;
			Vec3 turn_velocity_fast = Vec3(0,fw_speed_fast,0);
			Vec3 turn_velocity_slow	= Vec3(0,fw_speed_slow,0);



			Vec2 strafe = lmg.m_BlendSpace.m_strafe.GetNormalizedSafe( Vec2(0,0) );
			f32 rad = atan2(strafe.x,strafe.y);
			f32 fTurnPriority = fabsf( rad*2 );
			if (fTurnPriority>1.0f) fTurnPriority=1.0f;
			fTurnPriority=1.0f-fTurnPriority;

			fTurnPriority = lmg.m_params[eMotionParamID_Curving].value;

			f32 staf = 1.0f-fTurnPriority;
			f32 turn = fTurnPriority;
			lmg_caps.m_bHasVelocity = 1;
			lmg_caps.m_vMinVelocity = turn*turn_velocity_slow + staf*strafe_velocity_slow;
			lmg_caps.m_vMaxVelocity = turn*turn_velocity_fast + staf*strafe_velocity_fast;


			//duration
			f32 fL	= rsubGAH09.m_fTotalDuration;
			f32 fF	= rsubGAH00.m_fTotalDuration;
			f32 fR	= rsubGAH0a.m_fTotalDuration;
			f32 sL	= rsubGAH0b.m_fTotalDuration;
			f32 sF	= rsubGAH05.m_fTotalDuration;
			f32 sR	= rsubGAH0c.m_fTotalDuration;
			f32 turn_duration_fast = fL*L + fF*F + fR*R;
			f32 turn_duration_slow = sL*L + sF*F + sR*R;
			lmg_caps.m_fSlowDuration = turn*turn_duration_slow + staf*strafe_duration_slow;
			lmg_caps.m_fFastDuration = turn*turn_duration_fast + staf*strafe_duration_fast;
		}

		static Vec2 GetLRTurn( GlobalAnimationHeader* rGlobalAnimHeader, CAnimationSet* pAnimationSet, const SLocoGroup& lmg )
		{
			uint32 numAssets = rGlobalAnimHeader->m_arrBSAnimations.size();
			GlobalAnimationHeader* parrGlobalAnimations = &g_AnimationManager.m_arrGlobalAnimations[0];
			f32 fTurnLeft	=-99999.0f;
			f32 fTurnRight=+99999.0f;
			for (uint32 i=0; i<numAssets; i++)
			{
				int32 gid = lmg.m_nGlobalID[i];
				f32 turn = parrGlobalAnimations[ gid ].m_fTurnSpeed;
				if (fTurnLeft<turn)
					fTurnLeft=turn;
				if (fTurnRight>turn)
					fTurnRight=turn;
			}
			return Vec2(fTurnLeft,fTurnRight);
		}
	};






	LMGCapabilities lmg_caps;
	lmg_caps.m_travelAngleChangeRate = -1;
	lmg_caps.m_travelSpeedChangeRate = 8.0f;
	lmg_caps.m_turnSpeedChangeRate = proceduralTurnSpeedChangeRate;
	lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.0f;

	int32 animID=lmg.m_nLMGID;
	if (animID<0)
		animID=lmg.m_nAnimID[0]; //its no LMG, maybe it is a simple asset
	if (animID<0)
		return lmg_caps; //its not even a simple asset! quit function

	GlobalAnimationHeader* rGlobalAnimHeader = GetGlobalAnimationHeaderFromLocalID(animID);
	if (rGlobalAnimHeader==0)
	{
		AnimFileWarning(m_pModel->GetModelFilePath(),"animation-id '%d' is not valid in function GetLMGCapabilities()", animID);
		return lmg_caps;
	}


	int type = 0; // 0==unloaded, 1==regular, 2==lmg
	if (rGlobalAnimHeader->IsAssetLMG())
		type = 2;
	else
		type = 1;

	if (type == 1)
	{
		lmg_caps.m_bIsValid=1;

		lmg_caps.m_fSlowDuration		=	rGlobalAnimHeader->m_fTotalDuration;
		lmg_caps.m_fFastDuration		=	rGlobalAnimHeader->m_fTotalDuration;

		lmg_caps.m_fFastTurnLeft		=	rGlobalAnimHeader->m_fTurnSpeed;
		lmg_caps.m_fFastTurnRight	=	rGlobalAnimHeader->m_fTurnSpeed;
		lmg_caps.m_fAllowDesiredTurning	=	0;

		// Don't set the speed here, since we don't have a flag for if we allow parameterized speed or not.
		// If we need the speed to be set here we also need to add a m_bHasVelocity flag to the caps.
		// NOTE: m_bHasVelocity has been added and is 0 by default.
		lmg_caps.m_vMinVelocity	=	rGlobalAnimHeader->m_vVelocity;
		lmg_caps.m_vMaxVelocity	=	rGlobalAnimHeader->m_vVelocity;
		return lmg_caps;
	}

	if (!rGlobalAnimHeader->IsAssetLoaded())
	{
		AnimFileWarning(m_pModel->GetModelFilePath(),"animation-asset '%s' is not loaded in function GetLMGCapabilities()", rGlobalAnimHeader->GetPathName());
		return lmg_caps;
	}

	//-------------------------------------------------------------------------------------------
	//----------                evaluate Locomotion Group                             -----------
	//-------------------------------------------------------------------------------------------

	GlobalAnimationHeader* parrGlobalAnimations = &g_AnimationManager.m_arrGlobalAnimations[0];
	if (type == 2)
	{
		GlobalAnimationHeader& rsubGAH00 = parrGlobalAnimations[ lmg.m_nGlobalID[0x00] ];
		GlobalAnimationHeader& rsubGAH01 = parrGlobalAnimations[ lmg.m_nGlobalID[0x01] ];
		GlobalAnimationHeader& rsubGAH02 = parrGlobalAnimations[ lmg.m_nGlobalID[0x02] ];
		GlobalAnimationHeader& rsubGAH03 = parrGlobalAnimations[ lmg.m_nGlobalID[0x03] ];
		GlobalAnimationHeader& rsubGAH04 = parrGlobalAnimations[ lmg.m_nGlobalID[0x04] ];
		GlobalAnimationHeader& rsubGAH05 = parrGlobalAnimations[ lmg.m_nGlobalID[0x05] ];

		GlobalAnimationHeader& rsubGAH06 = parrGlobalAnimations[ lmg.m_nGlobalID[0x06] ];
		GlobalAnimationHeader& rsubGAH07 = parrGlobalAnimations[ lmg.m_nGlobalID[0x07] ];
		GlobalAnimationHeader& rsubGAH08 = parrGlobalAnimations[ lmg.m_nGlobalID[0x08] ];
		GlobalAnimationHeader& rsubGAH09 = parrGlobalAnimations[ lmg.m_nGlobalID[0x09] ];
		GlobalAnimationHeader& rsubGAH0a = parrGlobalAnimations[ lmg.m_nGlobalID[0x0a] ];
		GlobalAnimationHeader& rsubGAH0b = parrGlobalAnimations[ lmg.m_nGlobalID[0x0b] ];

		GlobalAnimationHeader& rsubGAH0c = parrGlobalAnimations[ lmg.m_nGlobalID[0x0c] ];
		GlobalAnimationHeader& rsubGAH0d = parrGlobalAnimations[ lmg.m_nGlobalID[0x0d] ];
		GlobalAnimationHeader& rsubGAH0e = parrGlobalAnimations[ lmg.m_nGlobalID[0x0e] ];
		GlobalAnimationHeader& rsubGAH0f = parrGlobalAnimations[ lmg.m_nGlobalID[0x0f] ];
		GlobalAnimationHeader& rsubGAH10 = parrGlobalAnimations[ lmg.m_nGlobalID[0x10] ];
		GlobalAnimationHeader& rsubGAH11 = parrGlobalAnimations[ lmg.m_nGlobalID[0x11] ];

		GlobalAnimationHeader& rsubGAH12 = parrGlobalAnimations[ lmg.m_nGlobalID[0x12] ];
		GlobalAnimationHeader& rsubGAH13 = parrGlobalAnimations[ lmg.m_nGlobalID[0x13] ];
		GlobalAnimationHeader& rsubGAH14 = parrGlobalAnimations[ lmg.m_nGlobalID[0x14] ];
		GlobalAnimationHeader& rsubGAH15 = parrGlobalAnimations[ lmg.m_nGlobalID[0x15] ];
		GlobalAnimationHeader& rsubGAH16 = parrGlobalAnimations[ lmg.m_nGlobalID[0x16] ];
		GlobalAnimationHeader& rsubGAH17 = parrGlobalAnimations[ lmg.m_nGlobalID[0x17] ];

		GlobalAnimationHeader& rsubGAH18 = parrGlobalAnimations[ lmg.m_nGlobalID[0x18] ];

		GlobalAnimationHeader& rsubGAH19 = parrGlobalAnimations[ lmg.m_nGlobalID[0x19] ];
		GlobalAnimationHeader& rsubGAH1a = parrGlobalAnimations[ lmg.m_nGlobalID[0x1a] ];
		GlobalAnimationHeader& rsubGAH1b = parrGlobalAnimations[ lmg.m_nGlobalID[0x1b] ];
		GlobalAnimationHeader& rsubGAH1c = parrGlobalAnimations[ lmg.m_nGlobalID[0x1c] ];
		GlobalAnimationHeader& rsubGAH1d = parrGlobalAnimations[ lmg.m_nGlobalID[0x1d] ];
		GlobalAnimationHeader& rsubGAH1e = parrGlobalAnimations[ lmg.m_nGlobalID[0x1e] ];
		GlobalAnimationHeader& rsubGAH1f = parrGlobalAnimations[ lmg.m_nGlobalID[0x1f] ];
		GlobalAnimationHeader& rsubGAH20 = parrGlobalAnimations[ lmg.m_nGlobalID[0x20] ];

		lmg_caps.m_BlendType=rGlobalAnimHeader->m_nBlendCodeLMG;

		//-----------------------------------------------------------------------------------------
		//-------         LMG for idle-rotations. Usually they have no speed at all        -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader->m_nBlendCodeLMG==*(uint32*)"IROT" )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.0f;

			uint32 numAssets = rGlobalAnimHeader->m_arrBSAnimations.size();
			f32 t0=0; f32 t1=0; f32 t2=0;
			f32	t = lmg.m_BlendSpace.m_turn;
			if (t<0)
			{
				t1=1+t;   //middle
				t2=-t;     //left
			}
			else
			{
				t0=t;    //right
				t1=1-t;  //middle
			}			

			f32 fMinDuration=+99999.0f;
			f32 fMaxDuration=-99999.0f;
			f32 fTurnLeft	=-99999.0f;
			f32 fTurnRight=+99999.0f;
			for (uint32 i=0; i<numAssets; i++)
			{
				int32 gid = lmg.m_nGlobalID[i];

				f32 turn = parrGlobalAnimations[ gid ].m_fTurnSpeed;
				if (fTurnLeft<turn)
					fTurnLeft=turn;
				if (fTurnRight>turn)
					fTurnRight=turn;

				f32 duration = parrGlobalAnimations[ gid ].m_fTotalDuration;
				if (fMinDuration>duration)
					fMinDuration=duration;
				if (fMaxDuration<duration)
					fMaxDuration=duration;
			}

			lmg_caps.m_bIsValid=1;
			lmg_caps.m_bIsLMG=1;
			//lmg_caps.m_bHasStrafingAsset=-1;
			//lmg_caps.m_bHasDistance=-1;
			lmg_caps.m_bHasTurningAsset=1;
			lmg_caps.m_fAllowDesiredTurning=0;

			lmg_caps.m_fSlowDuration	=	fMinDuration;
			lmg_caps.m_fFastDuration	=	fMaxDuration;

			lmg_caps.m_vMinVelocity	= Vec3(0,0,0);
			lmg_caps.m_vMaxVelocity	= Vec3(0,0,0);

			lmg_caps.m_fFastTurnLeft		= fTurnLeft;
			lmg_caps.m_fFastTurnRight	= fTurnRight;
	
			return lmg_caps;
		}

		if (rGlobalAnimHeader->m_nBlendCodeLMG==*(uint32*)"M2I1" )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 1.0f;
			return lmg_caps;
		}




		//-----------------------------------------------------------------------------------------
		//-------         LMG for idle-rotations. Usually they have no speed at all        -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader->m_nBlendCodeLMG==*(uint32*)"TSTP" )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.0f;

			lmg_caps.m_bIsValid=1;
			lmg_caps.m_bIsLMG=1;
			lmg_caps.m_bHasStrafingAsset=0;
			lmg_caps.m_bHasTurningDistAsset=1;
			lmg_caps.m_bHasVelocity = 0;
			lmg_caps.m_travelAngleChangeRate = 0.0f;
			lmg_caps.m_travelSpeedChangeRate = 0.0f;

			uint32 numAssets = rGlobalAnimHeader->m_arrBSAnimations.size();
			f32 t0=0; f32 t1=0; f32 t2=0;
			f32	t = lmg.m_BlendSpace.m_turn;
			if (t<0)
			{
				t1=1+t;   //middle
				t2=-t;     //left
			}
			else
			{
				t0=t;    //right
				t1=1-t;  //middle
			}			

			f32 fMinDuration=+99999.0f;
			f32 fMaxDuration=-99999.0f;
			f32 fTurnLeft	=-99999.0f;
			f32 fTurnRight=+99999.0f;
			for (uint32 i=0; i<numAssets; i++)
			{
				int32 gid = lmg.m_nGlobalID[i];

				f32 turn = parrGlobalAnimations[ gid ].m_fAssetTurn;
				if (fTurnLeft<turn) fTurnLeft=turn;
				if (fTurnRight>turn) fTurnRight=turn;

				f32 duration = parrGlobalAnimations[ gid ].m_fTotalDuration;
				if (fMinDuration>duration) fMinDuration=duration;
				if (fMaxDuration<duration) fMaxDuration=duration;
			}

			lmg_caps.m_fSlowDuration	=	fMinDuration;
			lmg_caps.m_fFastDuration	=	fMaxDuration;
			lmg_caps.m_vMinVelocity	= Vec3(0,0,0);
			lmg_caps.m_vMaxVelocity	= Vec3(0,0,0);
			lmg_caps.m_fFastTurnLeft		= fTurnLeft;
			lmg_caps.m_fFastTurnRight	= fTurnRight;
			lmg_caps.m_fAllowDesiredTurning=0;
			return lmg_caps;
		}



		if ( rGlobalAnimHeader->m_nBlendCodeLMG==*(uint32*)"I2M1" )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 1.0f;

			Vec2 mdir = lmg.m_BlendSpace.m_strafe; 
			mdir.NormalizeSafe();
			IdleStepWeights6 I2M = GetIdle2MoveWeights6(mdir); 
			f32 speed = rsubGAH00.m_fSpeed*I2M.fr + rsubGAH01.m_fSpeed*I2M.rr + rsubGAH02.m_fSpeed*I2M.br   +   rsubGAH03.m_fSpeed*I2M.fl + rsubGAH04.m_fSpeed*I2M.ll + rsubGAH05.m_fSpeed*I2M.bl;
			f32 turn = rsubGAH00.m_fTurnSpeed*I2M.fr + rsubGAH01.m_fTurnSpeed*I2M.rr + rsubGAH02.m_fTurnSpeed*I2M.br   +   rsubGAH03.m_fTurnSpeed*I2M.fl + rsubGAH04.m_fTurnSpeed*I2M.ll + rsubGAH05.m_fTurnSpeed*I2M.bl;
			f32 duration = rsubGAH00.m_fTotalDuration*I2M.fr + rsubGAH01.m_fTotalDuration*I2M.rr + rsubGAH02.m_fTotalDuration*I2M.br   +   rsubGAH03.m_fTotalDuration*I2M.fl + rsubGAH04.m_fTotalDuration*I2M.ll + rsubGAH05.m_fTotalDuration*I2M.bl;

			lmg_caps.m_bIsValid=1;
			lmg_caps.m_bIsLMG=1;
			lmg_caps.m_bHasStrafingAsset=1;
			//lmg_caps.m_bHasTurningAsset=-1;
			lmg_caps.m_bHasTurningAsset=0;
			lmg_caps.m_travelAngleChangeRate = 0.0f;
			lmg_caps.m_travelSpeedChangeRate = 0.0f;
			lmg_caps.m_fAllowDesiredTurning=0;

			lmg_caps.m_fSlowDuration = duration;
			lmg_caps.m_fFastDuration = duration;

			lmg_caps.m_bHasVelocity = 0;
			lmg_caps.m_vMinVelocity	=	Vec3(0,speed,0);
			lmg_caps.m_vMaxVelocity	=	Vec3(0,speed,0);

			lmg_caps.m_fFastTurnLeft =turn;
			lmg_caps.m_fFastTurnRight=turn;
			lmg_caps.m_fAllowDesiredTurning=0;
			return lmg_caps;
		}


		if ( rGlobalAnimHeader->m_nBlendCodeLMG==*(uint32*)"I2M2" )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 1.0f;

			lmg_caps.m_bIsValid=1;
			lmg_caps.m_bIsLMG=1;
			lmg_caps.m_bHasStrafingAsset=1;
			//lmg_caps.m_bHasTurningAsset=-1;
			lmg_caps.m_bHasTurningAsset=0;
			lmg_caps.m_bHasVelocity = 0;
			lmg_caps.m_bHasDistance = 0;
			lmg_caps.m_travelAngleChangeRate = 0.0f;
			lmg_caps.m_travelSpeedChangeRate = 0.0f;
			lmg_caps.m_fAllowDesiredTurning=0;

			Vec2 mdir = lmg.m_BlendSpace.m_strafe; 
			mdir.NormalizeSafe();
			IdleStepWeights6 I2M = GetIdle2MoveWeights6(mdir); 

			f32 minspeed            = rsubGAH00.m_fSpeed*I2M.fr         + rsubGAH01.m_fSpeed*I2M.rr         + rsubGAH02.m_fSpeed*I2M.br           +   rsubGAH03.m_fSpeed*I2M.fl         + rsubGAH04.m_fSpeed*I2M.ll         + rsubGAH05.m_fSpeed*I2M.bl;
			lmg_caps.m_vMinVelocity	=	Vec3(0,minspeed-0.001f,0);
			//lmg_caps.m_fFastTurnLeft = rsubGAH00.m_fTurn*I2M.fr          + rsubGAH01.m_fTurn*I2M.rr          + rsubGAH02.m_fTurn*I2M.br            +   rsubGAH03.m_fTurn*I2M.fl          + rsubGAH04.m_fTurn*I2M.ll          + rsubGAH05.m_fTurn*I2M.bl;
			lmg_caps.m_fFastTurnLeft = 0;
			lmg_caps.m_fSlowDuration = rsubGAH00.m_fTotalDuration*I2M.fr + rsubGAH01.m_fTotalDuration*I2M.rr + rsubGAH02.m_fTotalDuration*I2M.br   +   rsubGAH03.m_fTotalDuration*I2M.fl + rsubGAH04.m_fTotalDuration*I2M.ll + rsubGAH05.m_fTotalDuration*I2M.bl;

			f32 maxspeed            = rsubGAH06.m_fSpeed*I2M.fr         + rsubGAH07.m_fSpeed*I2M.rr         + rsubGAH08.m_fSpeed*I2M.br           +   rsubGAH09.m_fSpeed*I2M.fl         + rsubGAH0a.m_fSpeed*I2M.ll         + rsubGAH0b.m_fSpeed*I2M.bl;
			lmg_caps.m_vMaxVelocity	=	Vec3(0,maxspeed-0.001f,0);
			//lmg_caps.m_fFastTurnRight = rsubGAH06.m_fTurn*I2M.fr          + rsubGAH07.m_fTurn*I2M.rr          + rsubGAH08.m_fTurn*I2M.br            +   rsubGAH09.m_fTurn*I2M.fl          + rsubGAH0a.m_fTurn*I2M.ll          + rsubGAH0b.m_fTurn*I2M.bl;
			lmg_caps.m_fFastTurnRight = 0;
			lmg_caps.m_fFastDuration = rsubGAH06.m_fTotalDuration*I2M.fr + rsubGAH07.m_fTotalDuration*I2M.rr + rsubGAH08.m_fTotalDuration*I2M.br   +   rsubGAH09.m_fTotalDuration*I2M.fl + rsubGAH0a.m_fTotalDuration*I2M.ll + rsubGAH0b.m_fTotalDuration*I2M.bl;

			lmg_caps.m_fAllowDesiredTurning=0;
			return lmg_caps;
		}


		if (rGlobalAnimHeader->m_nBlendCodeLMG==*(uint32*)"ISTP" )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.0f;

			Vec2 sFR = Vec2(0,rsubGAH00.m_vVelocity.y);
			Vec2 sRR = Vec2(rsubGAH01.m_vVelocity);
			Vec2 sBR = Vec2(0,rsubGAH02.m_vVelocity.y);

			Vec2 sFL = Vec2(0,rsubGAH03.m_vVelocity.y);
			Vec2 sRL = Vec2(rsubGAH04.m_vVelocity);
			Vec2 sBL = Vec2(0,rsubGAH05.m_vVelocity.y);
			IdleStepWeights6 blend = GetIdleStepWeights6(sFR,sRR,sBR, sFL,sRL,sBL, lmg.m_BlendSpace.m_strafe );
			f32 turn = rsubGAH00.m_fTurnSpeed*blend.fr + rsubGAH01.m_fTurnSpeed*blend.rr + rsubGAH02.m_fTurnSpeed*blend.br   +   rsubGAH03.m_fTurnSpeed*blend.fl + rsubGAH04.m_fTurnSpeed*blend.ll + rsubGAH05.m_fTurnSpeed*blend.bl;
			Vec2 velocity = sFR*blend.fr + sRR*blend.rr + sBR*blend.br + sFL*blend.fl + sRL*blend.ll + sBL*blend.bl;
			f32 duration = rsubGAH00.m_fTotalDuration*blend.fr + rsubGAH01.m_fTotalDuration*blend.rr + rsubGAH02.m_fTotalDuration*blend.br + rsubGAH03.m_fTotalDuration*blend.fl + rsubGAH04.m_fTotalDuration*blend.ll + rsubGAH05.m_fTotalDuration*blend.bl;

			lmg_caps.m_bIsValid=1;
			lmg_caps.m_bIsLMG=1;
			lmg_caps.m_bHasStrafingAsset=1;
			lmg_caps.m_bHasTurningAsset=0;
			lmg_caps.m_bHasDistance = 1;
			lmg_caps.m_bHasVelocity = 0;
			lmg_caps.m_travelAngleChangeRate = 0.0f;
			lmg_caps.m_travelSpeedChangeRate = 0.0f;

			lmg_caps.m_fAllowDesiredTurning=0;
			//lmg_caps.m_fFastTurnLeft		= DEG2RAD(-0);
			//lmg_caps.m_fFastTurnRight	= DEG2RAD(+0);

			lmg_caps.m_travelAngleChangeRate = 0.0f;
			lmg_caps.m_travelSpeedChangeRate = 0.0f;

			lmg_caps.m_fSlowDuration	=	rsubGAH06.m_fTotalDuration;
			lmg_caps.m_fFastDuration	=	duration;

		//	lmg_caps.m_bHasVelocity = 1;
			lmg_caps.m_vMinVelocity	= Vec3(0,0,0);
			lmg_caps.m_vMaxVelocity	= velocity;
			return lmg_caps;
		}

		if ( rGlobalAnimHeader->m_nBlendCodeLMG == *(uint32*)"TRN1" )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.0f;

			lmg_caps.m_fAllowDesiredTurning=0;
			lmg_caps.m_vMinVelocity	=	Vec3(0,0,0);
			lmg_caps.m_vMaxVelocity	=	Vec3(0,0.001f,0);
			return lmg_caps;
		}



		//-----------------------------------------------------------------------------------------
		//-------                a run Left-Forward-Right LMG with one speed                -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader->m_nBlendCodeLMG==*(uint32*)"FLR1" )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.2f;

			//set LMG caps
			lmg_caps.m_bIsValid			=	1;
			lmg_caps.m_bIsLMG				=	1;
			lmg_caps.m_bHasStrafingAsset	=	0;
			lmg_caps.m_bHasTurningAsset	=	1;
			lmg_caps.m_fAllowDesiredTurning=1;

			f32 fast_FWL = rsubGAH00.m_fSpeed; //sprint left
			f32 fast_FW  = rsubGAH01.m_fSpeed; //sprint
			f32 fast_FWR = rsubGAH02.m_fSpeed; //sprint right

			f32 t=lmg.m_BlendSpace.m_turn;
			f32 L=0,F=0,R=0;
			if (t<0) { L=-t; F=1.0f+t; }	else { F=1.0f-t; R=t; }

			f32 fw_speed  = fast_FWL*L + fast_FW*F + fast_FWR*R;

		//	float fColor[4] = {1,1,0,1};
		//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"L %f   F %f    R %f",L,F,R); g_YLine+=0x10;

			//turning
			f32 fast_TL = rsubGAH00.m_fTurnSpeed; //sprint left
			f32 fast_TR = rsubGAH02.m_fTurnSpeed; //sprint right

			lmg_caps.m_bHasVelocity = 1;
			lmg_caps.m_vMinVelocity	= Vec3(0,fw_speed,0);
			lmg_caps.m_vMaxVelocity	= Vec3(0,fw_speed,0);

			lmg_caps.m_fFastTurnLeft		=	fast_TL;
			lmg_caps.m_fFastTurnRight	=	fast_TR;
			lmg_caps.m_fAllowDesiredTurning=1;


			f32 dfL	= rsubGAH00.m_fTotalDuration;
			f32 dfF	= rsubGAH01.m_fTotalDuration;
			f32 dfR	= rsubGAH02.m_fTotalDuration;
			lmg_caps.m_fFastDuration = dfL*L + dfF*F + dfR*R;
			lmg_caps.m_fSlowDuration = dfL*L + dfF*F + dfR*R;
			return lmg_caps;
		}


		//-----------------------------------------------------------------------------------------
		//-------                a run Left-Forward-Right LMG with two speeds               -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader->m_nBlendCodeLMG==*(uint32*)"FLR2" )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.2f;

			//set LMG caps
			lmg_caps.m_bIsValid=1;
			lmg_caps.m_bIsLMG=1;
			lmg_caps.m_bHasStrafingAsset=0;
			lmg_caps.m_bHasTurningAsset=1;
			lmg_caps.m_fAllowDesiredTurning=1;


			f32 fast_FWL = rsubGAH00.m_fSpeed; //fast left
			f32 fast_FW  = rsubGAH01.m_fSpeed; //fast
			f32 fast_FWR = rsubGAH02.m_fSpeed; //fast right

			f32 slow_FWL = rsubGAH03.m_fSpeed; //slow left
			f32 slow_FW  = rsubGAH04.m_fSpeed; //slow
			f32 slow_FWR = rsubGAH05.m_fSpeed; //slow right

			f32 t=lmg.m_BlendSpace.m_turn;
			f32 L=0,F=0,R=0;
			if (t<0) { L=-t; F=1.0f+t; }	else { F=1.0f-t; R=t; }
			//f32 white[] = {1.0f, 1.0f, 1.0f, 1.0f};
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, white, false,"--------------------------------------L: %f   F: %f   R: %f   turn: %f",L,F,R,t); g_YLine+=0x10;

			f32 fw_speed_fast = fast_FWL*L + fast_FW*F + fast_FWR*R;
			f32 fw_speed_slow = slow_FWL*L + slow_FW*F + slow_FWR*R;


			f32 dfL	= rsubGAH00.m_fTotalDuration;
			f32 dfF	= rsubGAH01.m_fTotalDuration;
			f32 dfR	= rsubGAH02.m_fTotalDuration;
			f32 dsL	= rsubGAH03.m_fTotalDuration;
			f32 dsF	= rsubGAH04.m_fTotalDuration;
			f32 dsR	= rsubGAH05.m_fTotalDuration;
			lmg_caps.m_fFastDuration = dfL*L + dfF*F + dfR*R;
			lmg_caps.m_fSlowDuration = dsL*L + dsF*F + dsR*R;

			lmg_caps.m_fFastTurnLeft		=	rsubGAH00.m_fTurnSpeed;
			lmg_caps.m_fFastTurnRight	=	rsubGAH02.m_fTurnSpeed;
			lmg_caps.m_fAllowDesiredTurning=1;

			lmg_caps.m_bHasVelocity = 1;
			lmg_caps.m_vMinVelocity= Vec3(0,fw_speed_slow,0);
			lmg_caps.m_vMaxVelocity= Vec3(0,fw_speed_fast,0);
			return lmg_caps;
		}

		//-------------------------------------------------------------------------------------
		//-------------------------------------------------------------------------------------
		//-------------------------------------------------------------------------------------
		if (rGlobalAnimHeader->m_nBlendCodeLMG==*(uint32*)"FLR3" )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.2f;

			//set LMG caps
			lmg_caps.m_bIsValid=1;
			lmg_caps.m_bIsLMG=1;
			lmg_caps.m_bHasStrafingAsset=0;
			lmg_caps.m_bHasTurningAsset=1;
			lmg_caps.m_fAllowDesiredTurning=1;

			f32 fast_FWL = rsubGAH00.m_fSpeed;	//fast left
			f32 fast_FW  = rsubGAH01.m_fSpeed;	//fast
			f32 fast_FWR = rsubGAH02.m_fSpeed;	//fast right
			f32 slow_FWL = rsubGAH04.m_fSpeed;	//slow left
			f32 slow_FW	 = rsubGAH05.m_fSpeed;	//slow
			f32 slow_FWR = rsubGAH06.m_fSpeed;	//slow right

			f32 t=lmg.m_BlendSpace.m_turn;
			f32 L=0,F=0,R=0;
			if (t<0) { L=-t; F=1.0f+t; }	else { F=1.0f-t; R=t; }
		//	float fColor[4] = {1,1,0,1};
		//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"++++++++++++++++++++++++++         L %f   F %f    R %f",L,F,R); g_YLine+=0x10;

			f32 fw_speed_fast = fast_FWL*L + fast_FW*F + fast_FWR*R;
			f32 fw_speed_slow	= slow_FWL*L + slow_FW*F + slow_FWR*R;

			lmg_caps.m_fFastTurnLeft		= rsubGAH00.m_fTurnSpeed; //sprint left;
			lmg_caps.m_fFastTurnRight	= rsubGAH02.m_fTurnSpeed; //sprint right;
			lmg_caps.m_fAllowDesiredTurning=1;
			lmg_caps.m_bHasVelocity = 1;
			lmg_caps.m_vMinVelocity=Vec3(0,fw_speed_slow,0);
			lmg_caps.m_vMaxVelocity=Vec3(0,fw_speed_fast,0);



			f32 dfL	= rsubGAH00.m_fTotalDuration;
			f32 dfF	= rsubGAH01.m_fTotalDuration;
			f32 dfR	= rsubGAH02.m_fTotalDuration;
			f32 dsL	= rsubGAH04.m_fTotalDuration;
			f32 dsF	= rsubGAH05.m_fTotalDuration;
			f32 dsR	= rsubGAH06.m_fTotalDuration;
			lmg_caps.m_fFastDuration = dfL*L + dfF*F + dfR*R;
			lmg_caps.m_fSlowDuration = dsL*L + dsF*F + dsR*R;

			lmg_caps.m_fFastTurnLeft		=	rsubGAH00.m_fTurnSpeed;
			lmg_caps.m_fFastTurnRight	=	rsubGAH02.m_fTurnSpeed;

			return lmg_caps;
		}




		//-----------------------------------------------------------------------------------------
		//-------                          just a test lmg                                  -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader->m_nBlendCodeLMG==*(uint32*)"UDH1" )
		{
			lmg_caps.m_vMinVelocity=Vec3(0,0,0);
			lmg_caps.m_vMaxVelocity=Vec3(0,0.001f,0);
			return lmg_caps;
		}

		//-----------------------------------------------------------------------------------------
		//-------                          just a test lmg                                  -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader->m_nBlendCodeLMG==*(uint32*)"UDH2" )
		{
			lmg_caps.m_vMinVelocity=Vec3(0,0,0);
			lmg_caps.m_vMaxVelocity=Vec3(0,0.001f,0);
			return lmg_caps;
		}

		//-----------------------------------------------------------------------------------------
		//-------            a standard left/right/hill LMG with three speeds               -------
		//-----------------------------------------------------------------------------------------
		if (rGlobalAnimHeader->m_nBlendCodeLMG==*(uint32*)"UDH3" )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.2f;

			uint32 numAssets = rGlobalAnimHeader->m_arrBSAnimations.size();
			//set LMG caps
			lmg_caps.m_bIsValid=1;
			lmg_caps.m_bIsLMG=1;
			lmg_caps.m_bHasStrafingAsset=0;
			lmg_caps.m_bHasTurningAsset=1;
			lmg_caps.m_bHasSlopeAsset=1;
			lmg_caps.m_fAllowDesiredTurning=1;


			f32 uh=0,fw=0,dh=0;
			f32	t = lmg.m_BlendSpace.m_slope;
			if (t>0)
			{
				uh=t;	fw=(1-t);
			}
			else
			{
				fw=t+1;	dh=1-(t+1);
			}

			GlobalAnimationHeader& rsubGAH0_0 = parrGlobalAnimations[ lmg.m_nGlobalID[0+0] ];
			GlobalAnimationHeader& rsubGAH1_0 = parrGlobalAnimations[ lmg.m_nGlobalID[1+0] ];
			GlobalAnimationHeader& rsubGAH2_0 = parrGlobalAnimations[ lmg.m_nGlobalID[2+0] ];
			GlobalAnimationHeader& rsubGAH6_0 = parrGlobalAnimations[ lmg.m_nGlobalID[6+0] ];
			GlobalAnimationHeader& rsubGAH7_0 = parrGlobalAnimations[ lmg.m_nGlobalID[7+0] ];
			GlobalAnimationHeader& rsubGAH8_0 = parrGlobalAnimations[ lmg.m_nGlobalID[8+0] ];
			f32 fast_UHL = rsubGAH0_0.m_fSpeed; //sprint left
			f32 fast_UH  = rsubGAH1_0.m_fSpeed; //sprint
			f32 fast_UHR = rsubGAH2_0.m_fSpeed; //sprint right
			f32 slow_UHL = rsubGAH6_0.m_fSpeed; //slow jog left
			f32 slow_UH  = rsubGAH7_0.m_fSpeed; //slow jog
			f32 slow_UHR = rsubGAH8_0.m_fSpeed; //slow jog right
			f32 dfast_UHL = rsubGAH0_0.m_fTotalDuration; //sprint left
			f32 dfast_UH  = rsubGAH1_0.m_fTotalDuration; //sprint
			f32 dfast_UHR = rsubGAH2_0.m_fTotalDuration; //sprint right
			f32 dslow_UHL = rsubGAH6_0.m_fTotalDuration; //slow jog left
			f32 dslow_UH  = rsubGAH7_0.m_fTotalDuration; //slow jog
			f32 dslow_UHR = rsubGAH8_0.m_fTotalDuration; //slow jog right

			GlobalAnimationHeader& rsubGAH0_9 = parrGlobalAnimations[ lmg.m_nGlobalID[0+9] ];
			GlobalAnimationHeader& rsubGAH1_9 = parrGlobalAnimations[ lmg.m_nGlobalID[1+9] ];
			GlobalAnimationHeader& rsubGAH2_9 = parrGlobalAnimations[ lmg.m_nGlobalID[2+9] ];
			GlobalAnimationHeader& rsubGAH6_9 = parrGlobalAnimations[ lmg.m_nGlobalID[6+9] ];
			GlobalAnimationHeader& rsubGAH7_9 = parrGlobalAnimations[ lmg.m_nGlobalID[7+9] ];
			GlobalAnimationHeader& rsubGAH8_9 = parrGlobalAnimations[ lmg.m_nGlobalID[8+9] ];
			f32 fast_FWL = rsubGAH0_9.m_fSpeed; //sprint left
			f32 fast_FW  = rsubGAH1_9.m_fSpeed; //sprint
			f32 fast_FWR = rsubGAH2_9.m_fSpeed; //sprint right
			f32 slow_FWL = rsubGAH6_9.m_fSpeed; //slow jog left
			f32 slow_FW  = rsubGAH7_9.m_fSpeed; //slow jog
			f32 slow_FWR = rsubGAH8_9.m_fSpeed; //slow jog right

			f32 dfast_FWL = rsubGAH0_9.m_fTotalDuration; //sprint left
			f32 dfast_FW  = rsubGAH1_9.m_fTotalDuration; //sprint
			f32 dfast_FWR = rsubGAH2_9.m_fTotalDuration; //sprint right
			f32 dslow_FWL = rsubGAH6_9.m_fTotalDuration; //slow jog left
			f32 dslow_FW  = rsubGAH7_9.m_fTotalDuration; //slow jog
			f32 dslow_FWR = rsubGAH8_9.m_fTotalDuration; //slow jog right

			GlobalAnimationHeader& rsubGAH0_18 = parrGlobalAnimations[ lmg.m_nGlobalID[0+18] ];
			GlobalAnimationHeader& rsubGAH1_18 = parrGlobalAnimations[ lmg.m_nGlobalID[1+18] ];
			GlobalAnimationHeader& rsubGAH2_18 = parrGlobalAnimations[ lmg.m_nGlobalID[2+18] ];
			GlobalAnimationHeader& rsubGAH6_18 = parrGlobalAnimations[ lmg.m_nGlobalID[6+18] ];
			GlobalAnimationHeader& rsubGAH7_18 = parrGlobalAnimations[ lmg.m_nGlobalID[7+18] ];
			GlobalAnimationHeader& rsubGAH8_18 = parrGlobalAnimations[ lmg.m_nGlobalID[8+18] ];
			f32 fast_DHL = rsubGAH0_18.m_fSpeed; //sprint left
			f32 fast_DH  = rsubGAH1_18.m_fSpeed; //sprint
			f32 fast_DHR = rsubGAH2_18.m_fSpeed; //sprint right
			f32 slow_DHL = rsubGAH6_18.m_fSpeed; //slow jog left
			f32 slow_DH  = rsubGAH7_18.m_fSpeed; //slow jog
			f32 slow_DHR = rsubGAH8_18.m_fSpeed; //slow jog right

			f32 dfast_DHL = rsubGAH0_18.m_fTotalDuration; //sprint left
			f32 dfast_DH  = rsubGAH1_18.m_fTotalDuration; //sprint
			f32 dfast_DHR = rsubGAH2_18.m_fTotalDuration; //sprint right
			f32 dslow_DHL = rsubGAH6_18.m_fTotalDuration; //slow jog left
			f32 dslow_DH  = rsubGAH7_18.m_fTotalDuration; //slow jog
			f32 dslow_DHR = rsubGAH8_18.m_fTotalDuration; //slow jog right

			f32 L=0,F=0,R=0;
			if (lmg.m_BlendSpace.m_turn<0)
			{
				//left side
				L=-lmg.m_BlendSpace.m_turn;
				F=1.0f+lmg.m_BlendSpace.m_turn;
			}
			else
			{
				//right side
				F=1.0f-lmg.m_BlendSpace.m_turn;
				R=lmg.m_BlendSpace.m_turn;
			}

			f32 uh_maxspeed= fast_UHL*L + fast_UH*F + fast_UHR*R;
			f32 uh_minspeed= slow_UHL*L + slow_UH*F + slow_UHR*R;
			f32 fw_maxspeed= fast_FWL*L + fast_FW*F + fast_FWR*R;
			f32 fw_minspeed= slow_FWL*L + slow_FW*F + slow_FWR*R;
			f32 dh_maxspeed= fast_DHL*L + fast_DH*F + fast_DHR*R;
			f32 dh_minspeed= slow_DHL*L + slow_DH*F + slow_DHR*R;
			f32 maxspeed	= uh_maxspeed*uh + fw_maxspeed*fw + dh_maxspeed*dh;
			f32 minspeed	= uh_minspeed*uh + fw_minspeed*fw + dh_minspeed*dh;

			f32 duh_maxspeed= dfast_UHL*L + dfast_UH*F + dfast_UHR*R;
			f32 duh_minspeed= dslow_UHL*L + dslow_UH*F + dslow_UHR*R;
			f32 dfw_maxspeed= dfast_FWL*L + dfast_FW*F + dfast_FWR*R;
			f32 dfw_minspeed= dslow_FWL*L + dslow_FW*F + dslow_FWR*R;
			f32 ddh_maxspeed= dfast_DHL*L + dfast_DH*F + dfast_DHR*R;
			f32 ddh_minspeed= dslow_DHL*L + dslow_DH*F + dslow_DHR*R;
			f32 maxduration	= duh_maxspeed*uh + dfw_maxspeed*fw + ddh_maxspeed*dh;
			f32 minduration	= duh_minspeed*uh + dfw_minspeed*fw + ddh_minspeed*dh;

			/*
			float fColor[4] = {1,1,0,1};
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"left side fast: L %f  F %f  R %f ",L,F,R); g_YLine+=0x10;

			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"uh_minspeed: %f",uh_minspeed); g_YLine+=0x10;
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"uh_maxspeed: %f",uh_maxspeed); g_YLine+=0x10;

			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"fw_minspeed: %f",fw_minspeed); g_YLine+=0x10;
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"fw_maxspeed: %f",fw_maxspeed); g_YLine+=0x10;

			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"dh_minspeed: %f",dh_minspeed); g_YLine+=0x10;
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"dh_maxspeed: %f",dh_maxspeed); g_YLine+=0x10;

			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"minspeed: %f",minspeed); g_YLine+=0x10;
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"maxspeed: %f",maxspeed); g_YLine+=0x10;
			*/

			lmg_caps.m_fSlowDuration	= minduration;
			lmg_caps.m_fFastDuration	= maxduration;

			lmg_caps.m_bHasVelocity = 1;
			lmg_caps.m_vMinVelocity	= Vec3(0,minspeed,0);
			lmg_caps.m_vMaxVelocity	= Vec3(0,maxspeed,0);
			return lmg_caps;
		}









		//------------------------------------------------------------------------------------
		//------------------------------------------------------------------------------------
		//------------------------------------------------------------------------------------

		if ( rGlobalAnimHeader->m_nBlendCodeLMG == *(uint32*)"STF1" )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.2f;

			//set LMG caps
			lmg_caps.m_bIsValid=1;
			lmg_caps.m_bIsLMG=1;
			lmg_caps.m_bHasStrafingAsset=1;
			lmg_caps.m_bHasTurningAsset=1;
			lmg_caps.m_fAllowDesiredTurning=1;
			Property::GetProperties_STF1( this,lmg,0,lmg_caps );
			return lmg_caps;
		}



		//---------------------------------------------------------------------------------------------------------
		//---------------------------------------------------------------------------------------------------------
		//---------------------------------------------------------------------------------------------------------
		if ( rGlobalAnimHeader->m_nBlendCodeLMG == *(uint32*)"STF2" )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.2f;

			//set LMG caps
			lmg_caps.m_bIsValid=1;
			lmg_caps.m_bIsLMG=1;
			lmg_caps.m_bHasStrafingAsset=1;
			lmg_caps.m_bHasTurningAsset=1;
			lmg_caps.m_fAllowDesiredTurning=1;
			Property::GetProperties_STF2( this,lmg,0,lmg_caps );
			return lmg_caps;
		}


		//---------------------------------------------------------------------------------------------------------
		//---------------------------------------------------------------------------------------------------------
		//---------------------------------------------------------------------------------------------------------
		if ( rGlobalAnimHeader->m_nBlendCodeLMG == *(uint32*)"SUD2" )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.2f;

			//set LMG caps
			lmg_caps.m_bIsValid=1;
			lmg_caps.m_bIsLMG=1;
			lmg_caps.m_bHasStrafingAsset=1;
			lmg_caps.m_bHasTurningAsset=1;
			lmg_caps.m_bHasSlopeAsset=1;
			lmg_caps.m_fAllowDesiredTurning=1;

			f32 uh=0,fw=0,dh=0;
			f32	t = lmg.m_BlendSpace.m_slope;
			if (t>0) { uh=t;	fw=1-t;	}
			else	{	fw=1+t;	dh=1-(t+1);	}
			LMGCapabilities ulmg;		Property::GetProperties_STF2( this,lmg, 0,ulmg );
			LMGCapabilities plmg;		Property::GetProperties_STF2( this,lmg, 8,plmg );
			LMGCapabilities dlmg;		Property::GetProperties_STF2( this,lmg,16,dlmg );
			lmg_caps.m_fSlowDuration	= ulmg.m_fSlowDuration*uh+plmg.m_fSlowDuration*fw+dlmg.m_fSlowDuration*dh;
			lmg_caps.m_fFastDuration	= ulmg.m_fFastDuration*uh+plmg.m_fFastDuration*fw+dlmg.m_fFastDuration*dh;
			lmg_caps.m_bHasVelocity = 1;
			lmg_caps.m_vMinVelocity	= ulmg.m_vMinVelocity*uh+plmg.m_vMinVelocity*fw+dlmg.m_vMinVelocity*dh;
			lmg_caps.m_vMaxVelocity	= ulmg.m_vMaxVelocity*uh+plmg.m_vMaxVelocity*fw+dlmg.m_vMaxVelocity*dh;
			return lmg_caps;
		}



		//-------------------------------------------------------------------------------------------------
		//--------------  these are the new strafes with 6 directions and one speed     -------------------
		//-------------------------------------------------------------------------------------------------
		if ( rGlobalAnimHeader->m_nBlendCodeLMG == *(uint32*)"S__1" )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.2f;

			//set LMG caps
			lmg_caps.m_bIsValid=1;
			lmg_caps.m_bIsLMG=1;
			lmg_caps.m_bHasStrafingAsset=1;
			lmg_caps.m_bHasTurningAsset=0;
			lmg_caps.m_fAllowDesiredTurning=1;
			Property::GetProperties_S1( this,lmg,0,lmg_caps );
			return lmg_caps;
		}

		//-------------------------------------------------------------------------------------------------
		//--------------  these are the new strafes with 6 directions and two speeds    -------------------
		//-------------------------------------------------------------------------------------------------
		if ( rGlobalAnimHeader->m_nBlendCodeLMG == *(uint32*)"S__2" )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.2f;

			//set LMG caps
			lmg_caps.m_bIsValid=1;
			lmg_caps.m_bIsLMG=1;
			lmg_caps.m_bHasStrafingAsset=1;
			lmg_caps.m_bHasTurningAsset=0;
			lmg_caps.m_fAllowDesiredTurning=1;
			Property::GetProperties_S2( this,lmg,0,lmg_caps );
			return lmg_caps;
		}


		//-------------------------------------------------------------------------------------------------
		//---------   these are the new strafes with 6 directions / asset turning / two speeds  -----------
		//-------------------------------------------------------------------------------------------------
		if ( rGlobalAnimHeader->m_nBlendCodeLMG == *(uint32*)"ST_2" )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.2f;

			//set LMG caps
			lmg_caps.m_bIsValid=1;
			lmg_caps.m_bIsLMG=1;
			lmg_caps.m_bHasStrafingAsset=1;
			lmg_caps.m_bHasTurningAsset=1;
			lmg_caps.m_fAllowDesiredTurning=1;
			Property::GetProperties_ST2( this,lmg, 0, lmg_caps );
			return lmg_caps;
		}



		//-------------------------------------------------------------------------------------------------
		//--------------  these are the new strafes with 6 directions and two speeds    -------------------
		//-------------------------------------------------------------------------------------------------
		if ( rGlobalAnimHeader->m_nBlendCodeLMG == *(uint32*)"S_H2" )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.2f;

			//set LMG caps
			lmg_caps.m_bIsValid=1;
			lmg_caps.m_bIsLMG=1;
			lmg_caps.m_bHasStrafingAsset=1;
			lmg_caps.m_bHasTurningAsset=1;
			lmg_caps.m_bHasSlopeAsset=0;
			lmg_caps.m_fAllowDesiredTurning=1;

			f32 uh=0,fw=0,dh=0;
			f32	t = lmg.m_BlendSpace.m_slope;
			if (t>0) { uh=t;	fw=1-t;	}
			else	{	fw=1+t;	dh=1-(t+1);	}
			LMGCapabilities ulmg;		Property::GetProperties_STF2( this,lmg, 0,ulmg );
			LMGCapabilities plmg;		Property::GetProperties_S2( this,lmg, 8,plmg );
			LMGCapabilities dlmg;		Property::GetProperties_STF2( this,lmg,20,dlmg );
			lmg_caps.m_fSlowDuration	= ulmg.m_fSlowDuration*uh+plmg.m_fSlowDuration*fw+dlmg.m_fSlowDuration*dh;
			lmg_caps.m_fFastDuration	= ulmg.m_fFastDuration*uh+plmg.m_fFastDuration*fw+dlmg.m_fFastDuration*dh;
			lmg_caps.m_bHasVelocity = 1;
			lmg_caps.m_vMinVelocity	= ulmg.m_vMinVelocity*uh+plmg.m_vMinVelocity*fw+dlmg.m_vMinVelocity*dh;
			lmg_caps.m_vMaxVelocity	= ulmg.m_vMaxVelocity*uh+plmg.m_vMaxVelocity*fw+dlmg.m_vMaxVelocity*dh;
			return lmg_caps;
		}



		//-------------------------------------------------------------------------------------------------
		//--------------  these are the new strafes with 6 directions and two speeds    -------------------
		//-------------------------------------------------------------------------------------------------
		if ( rGlobalAnimHeader->m_nBlendCodeLMG == *(uint32*)"STH2" )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.2f;

			//set LMG caps
			lmg_caps.m_bIsValid=1;
			lmg_caps.m_bIsLMG=1;
			lmg_caps.m_bHasStrafingAsset=1;
			lmg_caps.m_bHasTurningAsset=1;
			lmg_caps.m_bHasSlopeAsset=1;
			lmg_caps.m_fAllowDesiredTurning=1.0f;

			f32 uh=0,fw=0,dh=0;
			f32	t = lmg.m_BlendSpace.m_slope;
			if (t>0) { uh=t;	fw=1-t;	}
			else	{	fw=1+t;	dh=1-(t+1);	}
			LMGCapabilities ulmg;		Property::GetProperties_STF2( this,lmg, 0,ulmg );
			LMGCapabilities plmg;		Property::GetProperties_ST2(  this,lmg, 8,plmg );
			LMGCapabilities dlmg;		Property::GetProperties_STF2( this,lmg,25,dlmg );
			lmg_caps.m_fSlowTurnLeft		= ulmg.m_fSlowTurnLeft*uh +plmg.m_fSlowTurnLeft*fw +dlmg.m_fSlowTurnLeft*dh;
			lmg_caps.m_fSlowTurnRight		= ulmg.m_fSlowTurnRight*uh+plmg.m_fSlowTurnRight*fw+dlmg.m_fSlowTurnRight*dh;
			lmg_caps.m_fFastTurnLeft		= ulmg.m_fFastTurnLeft*uh+plmg.m_fFastTurnLeft*fw+dlmg.m_fFastTurnLeft*dh;
			lmg_caps.m_fFastTurnRight		= ulmg.m_fFastTurnRight*uh+plmg.m_fFastTurnRight*fw+dlmg.m_fFastTurnRight*dh;
			lmg_caps.m_fSlowDuration		= ulmg.m_fSlowDuration*uh+plmg.m_fSlowDuration*fw+dlmg.m_fSlowDuration*dh;
			lmg_caps.m_fFastDuration		= ulmg.m_fFastDuration*uh+plmg.m_fFastDuration*fw+dlmg.m_fFastDuration*dh;
			lmg_caps.m_bHasVelocity = 1;
			lmg_caps.m_vMinVelocity		= ulmg.m_vMinVelocity*uh+plmg.m_vMinVelocity*fw+dlmg.m_vMinVelocity*dh;
			lmg_caps.m_vMaxVelocity		= ulmg.m_vMaxVelocity*uh+plmg.m_vMaxVelocity*fw+dlmg.m_vMaxVelocity*dh;

			lmg_caps.m_bHasCurving = plmg.m_bHasCurving;
			lmg_caps.m_curvingChangeRate = plmg.m_curvingChangeRate;
			lmg_caps.m_turnSpeedChangeRate = plmg.m_turnSpeedChangeRate;
			return lmg_caps;
		}



		//-------------------------------------------------------------------------------------------------
		//--------------  these are the new strafes with 6 directions and two speeds    -------------------
		//-------------------------------------------------------------------------------------------------
		if ( rGlobalAnimHeader->m_nBlendCodeLMG == *(uint32*)"STHX" )
		{
			lmg_caps.m_fDesiredLocalLocationLookaheadTime = 0.2f;

			//set LMG caps
			lmg_caps.m_bIsValid=1;
			lmg_caps.m_bIsLMG=1;
			lmg_caps.m_bHasStrafingAsset=1;
			lmg_caps.m_bHasTurningAsset=1;
			lmg_caps.m_bHasSlopeAsset=1;
			lmg_caps.m_fAllowDesiredTurning=1;

			f32 uh=0,fw=0,dh=0;
			f32	t = lmg.m_BlendSpace.m_slope;
			if (t>0) { uh=t;	fw=1-t;	}
			else	{	fw=1+t;	dh=1-(t+1);	}

			LMGCapabilities ulmg;		Property::GetProperties_STF2( this,lmg, 0,ulmg );
			LMGCapabilities plmg;		Property::GetProperties_ST2X( this,lmg, 8,plmg );
			LMGCapabilities dlmg;		Property::GetProperties_STF2( this,lmg,21,dlmg );
			lmg_caps.m_fSlowDuration	= ulmg.m_fSlowDuration*uh+plmg.m_fSlowDuration*fw+dlmg.m_fSlowDuration*dh;
			lmg_caps.m_fFastDuration	= ulmg.m_fFastDuration*uh+plmg.m_fFastDuration*fw+dlmg.m_fFastDuration*dh;
			lmg_caps.m_bHasVelocity = 1;
			lmg_caps.m_vMinVelocity	= ulmg.m_vMinVelocity*uh+plmg.m_vMinVelocity*fw+dlmg.m_vMinVelocity*dh;
			lmg_caps.m_vMaxVelocity	= ulmg.m_vMaxVelocity*uh+plmg.m_vMaxVelocity*fw+dlmg.m_vMaxVelocity*dh;

			return lmg_caps;
		}


		//----------------------------------------------------------------------------------
		//----------------------------------------------------------------------------------
		//----------------------------------------------------------------------------------

		//LMG not identified. This is a very bad thing. 
		char errtx[5];	*(uint32*)errtx = rGlobalAnimHeader->m_nBlendCodeLMG; 	errtx[4] = 0;
		CryError("locomotion-group '%s' not identified. This a fatal-error.",errtx);
		assert(0);

	}
	return lmg_caps; 
};








//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
f32 CAnimationSet::GetIWeightForSpeed(int nAnimationId, f32 Speed)
{
	Vec2 MinMax = GetMinMaxSpeedAsset_msec(nAnimationId);
	if (Speed<MinMax.x)
		return -1;
	if (Speed>MinMax.y)
		return 1;

	f32 speed=(Speed-MinMax.x);
	f32 smax=(MinMax.y-MinMax.x);
	return (speed/smax)*2-1;	
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
f32 CAnimationSet::GetDuration_sec(int nAnimationId)
{
	int32 numAnimation = (int32)m_arrAnimations.size();
	if (nAnimationId<0 || nAnimationId>=numAnimation)
	{
		AnimFileWarning(m_pModel->GetModelFilePath(),"illegal animation index used in function GetDuration_sec'%d'", nAnimationId);
		return -1;
	}

	const ModelAnimationHeader* anim = GetModelAnimationHeader(nAnimationId);
	if (anim==0)
		return -1;

	uint32 GlobalAnimationID                 = anim->m_nGlobalAnimId;
	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[GlobalAnimationID];
	uint32 lmg = rGlobalAnimHeader.IsAssetLMG();
	if ( lmg )
	{
		if (rGlobalAnimHeader.IsAssetLMGValid()==0)
			return 0; 

		f32 fDuration=0;
		uint32 numBS = rGlobalAnimHeader.m_arrBSAnimations.size();
		for (uint32 i=0; i<numBS; i++)
		{
			const char* aname = rGlobalAnimHeader.m_arrBSAnimations[i].m_strAnimName;
			int32 aid=GetAnimIDByName(aname);      
			assert(aid>=0);
			fDuration += GetDuration_sec(aid);
		}
		return      fDuration/numBS;
	} 
	else
	{
		f32   fDuration = rGlobalAnimHeader.m_fEndSec - rGlobalAnimHeader.m_fStartSec;
		return      fDuration;
	}
}

uint32 CAnimationSet::GetAnimationFlags(int nAnimationId)
{

	int32 numAnimation = (int32)m_arrAnimations.size();
	if (nAnimationId<0 || nAnimationId>=numAnimation)
	{
		AnimFileWarning(m_pModel->GetModelFilePath(),"illegal animation index used in function GetAnimationFlags '%d'", nAnimationId);
		return 0;
	}

	const ModelAnimationHeader* anim = GetModelAnimationHeader(nAnimationId);
	if (anim==0)
		return 0;

	uint32 GlobalAnimationID			= anim->m_nGlobalAnimId;
	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[GlobalAnimationID];
	return rGlobalAnimHeader.m_nFlags;
}


uint32 CAnimationSet::GetBlendSpaceCode(int nAnimationId)
{
	int32 numAnimation = (int32)m_arrAnimations.size();
	if (nAnimationId<0 || nAnimationId>=numAnimation)
	{
		AnimFileWarning(m_pModel->GetModelFilePath(),"illegal animation index used in function GetAnimationFlags '%d'", nAnimationId);
		return 0;
	}

	const ModelAnimationHeader* anim = GetModelAnimationHeader(nAnimationId);
	if (anim==0)
		return 0;

	uint32 GlobalAnimationID			= anim->m_nGlobalAnimId;
	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[GlobalAnimationID];
	return rGlobalAnimHeader.m_nBlendCodeLMG;
}


//-------------------------------------------------------------------
//! Returns the given animation's start, in seconds; 0 if the id is invalid
//-------------------------------------------------------------------
f32 CAnimationSet::GetStart (int nAnimationId)
{
	int32 numAnimation = (int32)m_arrAnimations.size();
	if (nAnimationId<0 || nAnimationId>=numAnimation)
	{
		AnimFileWarning(m_pModel->GetModelFilePath(),"illegal animation index used in function GetStart: '%d'", nAnimationId);
		return 0;
	}

	const ModelAnimationHeader* pAnim = &m_arrAnimations[nAnimationId];
	GlobalAnimationHeader& rGAH = g_AnimationManager.m_arrGlobalAnimations[pAnim->m_nGlobalAnimId];
	return rGAH.m_fStartSec;
}



CryAnimationPath CAnimationSet::GetAnimationPath(const char* szAnimationName) 
{
	CryAnimationPath p;
	p.m_key0.SetIdentity();
	p.m_key1.SetIdentity();
	int AnimID = GetAnimIDByName(szAnimationName);
	if (AnimID<0)
		return p;

	const ModelAnimationHeader* anim = GetModelAnimationHeader(AnimID);
	if (anim)
	{
		uint32 GlobalAnimationID = anim->m_nGlobalAnimId;
		GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[GlobalAnimationID];
		if ( rGlobalAnimHeader.IsAssetLMG() )
			return p;

		if ( rGlobalAnimHeader.IsAssetOnDemand() && !rGlobalAnimHeader.IsAssetLoaded() )
			return p;

		const CModelJoint* pModelJoint = &m_pModel->m_pModelSkeleton->m_arrModelJoints[0];
		IController* pController = g_AnimationManager.m_arrGlobalAnimations[GlobalAnimationID].GetControllerByJointID(pModelJoint[0].m_nJointCRC32);//pController = pModelJoint[0].m_arrControllersMJoint[AnimID];
		Diag33 scale; 
		// TODO: for locoman groups, pController is NULL (what to do here!!)
		if (pController)
		{
			Diag33 scale; 
			pController->GetOPS(GlobalAnimationID, 0, p.m_key0.q, p.m_key0.t,scale);	
			pController->GetOPS(GlobalAnimationID, 1, p.m_key1.q, p.m_key1.t,scale);	
		}
	}
	return p;
};

const QuatT& CAnimationSet::GetAnimationStartLocation(const char* szAnimationName) 
{
	static QuatT DefaultStartLocation; 
	DefaultStartLocation.SetIdentity();

	int32 AnimID = GetAnimIDByName(szAnimationName);
	if (AnimID<0)
	{
		AnimFileWarning(m_pModel->GetModelFilePath(),"animation-name not in cal-file: %s",szAnimationName);
		return DefaultStartLocation;
	}

	const ModelAnimationHeader* anim = GetModelAnimationHeader(AnimID);
	if (anim==0)
		return DefaultStartLocation;

	uint32 GlobalAnimationID = anim->m_nGlobalAnimId;
	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[GlobalAnimationID];
	return  rGlobalAnimHeader.m_StartLocation;
};

const char* CAnimationSet::GetFilePathByName (const char* szAnimationName)
{
	int32 AnimID = GetAnimIDByName(szAnimationName);
	if (AnimID<0)
	{
		AnimFileWarning(m_pModel->GetModelFilePath(),"animation-name not in cal-file: %s",szAnimationName);
		return 0;
	}
	const ModelAnimationHeader* anim = GetModelAnimationHeader(AnimID);
	if (anim==0)
		return 0;

	uint32 GlobalAnimationID			= anim->m_nGlobalAnimId;
	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[GlobalAnimationID];
	return  rGlobalAnimHeader.GetPathName();
};

const char* CAnimationSet::GetFilePathByID(int nAnimationId)
{
	const ModelAnimationHeader* anim = GetModelAnimationHeader(nAnimationId);
	if (anim==0)
		return 0;

	uint32 GlobalAnimationID			= anim->m_nGlobalAnimId;
	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[GlobalAnimationID];
	return  rGlobalAnimHeader.GetPathName();
};


int32 CAnimationSet::GetGlobalIDByName(const char* szAnimationName)
{
	int32 AnimID = GetAnimIDByName(szAnimationName);
	if (AnimID<0)
	{
		AnimFileWarning(m_pModel->GetModelFilePath(),"animation-name not in cal-file: %s",szAnimationName);
		return -1;
	}
	const ModelAnimationHeader* anim = GetModelAnimationHeader(AnimID);
	if (anim==0)
		return -1;

	return (int32)anim->m_nGlobalAnimId;
}
int32 CAnimationSet::GetGlobalIDByID(int nAnimationId)
{
	const ModelAnimationHeader* anim = GetModelAnimationHeader(nAnimationId);
	if (anim==0)
		return -1;
	return (int32)anim->m_nGlobalAnimId;
}


//if the return-value is positive then the closest-quaternion is at key[0...1]
f32 CAnimationSet::GetClosestQuatInChannel(const char* szAnimationName,int32 JointID, const Quat& reference)
{
	int32 numJoints = m_pModel->m_pModelSkeleton->m_arrModelJoints.size();
	if (JointID<0 || JointID>=numJoints)
	{
		AnimFileWarning(m_pModel->GetModelFilePath(),"illegal joint index: %d",JointID);
		return -1; // wrong animation name
	}

	int32 AnimID = GetAnimIDByName(szAnimationName);
	if (AnimID<0)
	{
		AnimFileWarning(m_pModel->GetModelFilePath(),"animation-name not in cal-file: %s",szAnimationName);
		return -1; // wrong animation name
	}
	const ModelAnimationHeader* anim = GetModelAnimationHeader(AnimID);
	if (anim==0)
		return -1; //asset does not exist

	f32 smalest_dot=0.0f;
	f32 closest_key=-1;

	uint32 GlobalAnimationID = anim->m_nGlobalAnimId;
	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[GlobalAnimationID];
	f32	fDuration = rGlobalAnimHeader.m_fEndSec - rGlobalAnimHeader.m_fStartSec;
	f32 timestep = (1.0f/30.0f)/fDuration;


	IController* pController = g_AnimationManager.m_arrGlobalAnimations[GlobalAnimationID].GetControllerByJointID(m_pModel->m_pModelSkeleton->m_arrModelJoints[JointID].m_nJointCRC32);//m_pModel->m_pModelSkeleton->m_arrModelJoints[JointID].m_arrControllersMJoint[AnimID];
	if (pController)
	{
		for (f32 t=0; t<1.0f; t=t+timestep)
		{
			//NOTE: all quternions in the channel are relative to the parent, which means they are usually close to identity

			Quat keyquat;	pController->GetO(GlobalAnimationID, t, keyquat);	


			f32 cosine=fabsf(keyquat|reference);
			if (smalest_dot<cosine)
			{
				smalest_dot=cosine;
				closest_key=t;
			}
		}
	}

	return closest_key*fDuration;
}

void CAnimationSet::AddNullControllers(int nGlobalAnimID)
{
	// The animation we have loaded has no controllers. We will have to add controllers to it here, one for each bone.
	GlobalAnimationHeader& rGlobalAnim = g_AnimationManager.GetGlobalAnimationHeader(nGlobalAnimID);

	// Create a controller for each joint.
	uint32 numController = m_pModel->m_pModelSkeleton->m_arrModelJoints.size();
	rGlobalAnim.m_arrController.resize(numController);
	for(uint32 i=0; i<numController; i++ )
	{
		// Create a dummy controller for this joint.
		CControllerPQLog_AutoPtr pController = new CControllerPQLog ();

		const CModelJoint& joint = m_pModel->m_pModelSkeleton->m_arrModelJoints[i];

		pController->m_arrKeys.clear();
		QuatT inverse = joint.m_DefaultRelPose;
		inverse.Invert();
		Vec3 vecQuatLog = log(inverse.q);
		PQLog pqlog = {joint.m_DefaultRelPose.t, vecQuatLog};
		pController->m_arrKeys.push_back(pqlog);

		pController->m_arrTimes.clear();
		pController->m_arrTimes.push_back(0);

		pController->m_nControllerId = m_pModel->m_pModelSkeleton->m_arrModelJoints[i].m_nJointCRC32;

		assert (i < rGlobalAnim.m_arrController.size());
		rGlobalAnim.m_arrController[i] = (IController*)pController;
	}

	std::sort(rGlobalAnim.m_arrController.begin(),	rGlobalAnim.m_arrController.end(), AnimCtrlSortPred()	);

	rGlobalAnim.m_fDistance=0;
	rGlobalAnim.m_fSpeed=0;
	rGlobalAnim.OnAssetProcessed();
}
