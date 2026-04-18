//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File:AttachmentManager.cpp
//  Implementation of AttachmentManager class
//
//	History:
//	August 16, 2004: Created by Ivo Herzeg <ivo@crytek.de>
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include <I3DEngine.h>
#include <IRenderAuxGeom.h>
#include <CryHeaders.h>

#include <Cry_Camera.h>
#include "ModelMesh.h"
#include "AttachmentManager.h"
#include "CharacterInstance.h"
#include "CharacterManager.h"



uint32 CAttachmentManager::LoadAttachmentList(const char* pathname ) {

	XmlNodeRef nodeAttachList	= g_pISystem->LoadXmlFile(pathname);	
	const char* AttachListTag		= nodeAttachList->getTag();
	if (strcmp(AttachListTag,"AttachmentList")==0) 
	{
		int anum2 = GetAttachmentCount();
		RemoveAllAttachments();
		anum2 = GetAttachmentCount();

		uint32 num = nodeAttachList->getChildCount();
		for (uint32 i=0; i<num; i++) {

			XmlNodeRef nodeAttach = nodeAttachList->getChild(i);
			const char* AttachTag = nodeAttach->getTag();
			if (strcmp(AttachTag,"Attachment")==0) 
			{
				Quat WRotation;
				Vec3 WPosition;
				string AName = nodeAttach->getAttr( "AName" );
				string Type  = nodeAttach->getAttr( "Type" );
				nodeAttach->getAttr( "Rotation",WRotation );
				nodeAttach->getAttr( "Position",WPosition );
				string BoneName = nodeAttach->getAttr( "BName" );
				string ObjectFileName = nodeAttach->getAttr( "Binding" );

				IAttachment* pIAttachment=0;

				if (Type=="CA_BONE") 
				{
					pIAttachment = CreateAttachment(AName,CA_BONE,BoneName);
					if (pIAttachment==0) continue; 
					pIAttachment->SetAttAbsoluteDefault( QuatT(WRotation,WPosition) );

					IStatObj* pIStatObj = g_pI3DEngine->LoadStatObj( ObjectFileName );
					if (pIStatObj) 
					{
						CCGFAttachment* pStatAttachment = new CCGFAttachment();
						pStatAttachment->pObj  = pIStatObj;
						IAttachmentObject* pIAttachmentObject = (IAttachmentObject*)pStatAttachment;
						pIAttachment->AddBinding( pIAttachmentObject );
					}
				} 
				if (Type=="CA_FACE") 
				{
					pIAttachment = CreateAttachment(AName,CA_FACE,0);
					pIAttachment->SetAttAbsoluteDefault(QuatT(WRotation,WPosition));

					IStatObj* pIStatObj = g_pI3DEngine->LoadStatObj( ObjectFileName );
					if (pIStatObj) 
					{
						CCGFAttachment* pStatAttachment = new CCGFAttachment();
						pStatAttachment->pObj  = pIStatObj;
						IAttachmentObject* pIAttachmentObject = (IAttachmentObject*)pStatAttachment;
						pIAttachment->AddBinding( pIAttachmentObject );
					}
				}
				if (Type=="CA_SKIN") 
				{
					pIAttachment = CreateAttachment(AName,CA_SKIN,0);
					pIAttachment->SetAttAbsoluteDefault( QuatT(WRotation,WPosition) );
					
					IAttachmentObject* pIAttachmentObject=0;
					ICharacterInstance* pIChildCharacter=0;

					string fileExt = PathUtil::GetExt(ObjectFileName);

					bool IsCDF = (0 == stricmp(fileExt,"cdf"));
					if (IsCDF) 
					{
						if (pIChildCharacter) 
						{
							CCHRAttachment* pCharacterAttachment = new CCHRAttachment();
							pCharacterAttachment->m_pCharInstance  = pIChildCharacter;
							pIAttachmentObject = (IAttachmentObject*)pCharacterAttachment;
						}
						pIChildCharacter = g_pCharacterManager->LoadCharacterDefinition( ObjectFileName );
					}

					bool IsCHR = (0 == stricmp(fileExt,"chr"));
					if (IsCHR)
					{
						pIChildCharacter = g_pCharacterManager->CreateInstance( ObjectFileName );
						if (pIChildCharacter==0)
						{
							g_pILog->LogError ("CryAnimation: no character created: %s", pathname);
						} 
						else 
						{
							CCHRAttachment* pCharacterAttachment = new CCHRAttachment();
							pCharacterAttachment->m_pCharInstance  = pIChildCharacter;
							pIAttachmentObject = (IAttachmentObject*)pCharacterAttachment;
						}
					}

					pIAttachment->AddBinding(pIAttachmentObject);
				}

			}
		}
		uint32 count = GetAttachmentCount();
		assert(count==num);
		ProjectAllAttachment();
	}

	return 1;
};


uint32 CAttachmentManager::SaveAttachmentList(const char* pathname ) {

	uint32 count = GetAttachmentCount();

	if (count) {
		XmlNodeRef nodeAttachements		= g_pISystem->CreateXmlNode( "AttachmentList" );	

		for (uint32 i=0; i<count; i++) 
		{
			IAttachment*  pIAttachment = GetInterfaceByIndex(i);

			XmlNodeRef nodeAttach = nodeAttachements->newChild( "Attachment" );
			const char* AName = pIAttachment->GetName();
			Quat WRotation = pIAttachment->GetAttAbsoluteDefault().q;
			Vec3 WPosition = pIAttachment->GetAttAbsoluteDefault().t;
			uint32 Type = pIAttachment->GetType();

			uint32 BoneID = pIAttachment->GetBoneID();
			const char* BoneName	= m_pSkelInstance->m_SkeletonPose.GetJointNameByID(BoneID);	

			const char* BindingName = "";
			IAttachmentObject* pIAttachmentObject = pIAttachment->GetIAttachmentObject();

			if (Type==CA_BONE) 
			{
				if (pIAttachmentObject)
				{
					IStatObj* m_pIAttachmentObject = pIAttachmentObject->GetIStatObj();
					if (m_pIAttachmentObject)	BindingName=m_pIAttachmentObject->GetFilePath();
				}
			}

			if (Type==CA_FACE) {
				if (pIAttachmentObject){
					IStatObj* m_pIAttachmentObject = pIAttachmentObject->GetIStatObj();
					if (m_pIAttachmentObject)	BindingName=m_pIAttachmentObject->GetFilePath();
				}
			}

			if (Type==CA_SKIN) {
				if (pIAttachmentObject){
					ICharacterInstance* pICharacterChild=pIAttachmentObject->GetICharacterInstance();
					if (pICharacterChild) 
					{
						BindingName = pICharacterChild->GetICharacterModel()->GetModelFilePath();
					}
				}
			}

			nodeAttach->setAttr( "AName",AName );
			if (Type==CA_BONE) nodeAttach->setAttr( "Type", "CA_BONE" );
			if (Type==CA_FACE) nodeAttach->setAttr( "Type", "CA_FACE" );
			if (Type==CA_SKIN) nodeAttach->setAttr( "Type", "CA_SKIN" );
			nodeAttach->setAttr( "Rotation",WRotation );
			nodeAttach->setAttr( "Position",WPosition );
			nodeAttach->setAttr( "BName",BoneName );
			nodeAttach->setAttr( "Binding", BindingName );
		}

		nodeAttachements->saveToFile(pathname);
	}
	return 1;
};




IAttachment* CAttachmentManager::CreateAttachment( const char* szName, uint32 type, const char* szBoneName ) 
{

	IAttachment* pIAttachment = GetInterfaceByName(szName);
	if (pIAttachment)
		return 0;

	uint32 rm = m_pSkelInstance->GetResetMode();
	if (type==CA_BONE || type==CA_FACE)
		m_pSkelInstance->GetISkeletonPose()->SetDefaultPose();

	m_pSkelInstance->SetResetMode(1);

	int32 nBoneID=-1;
	if (type==CA_BONE) 
	{
		if(szBoneName==0)
		{
			m_pSkelInstance->SetResetMode(rm);
			return 0;
		}

		nBoneID = m_pSkelInstance->m_SkeletonPose.GetJointIDByName(szBoneName);
		if(nBoneID<0)
		{
			m_pSkelInstance->SetResetMode(rm);
			return 0;
		}


	}

	CAttachment* pAttachment = new CAttachment();

	pAttachment->m_Type=type;
	pAttachment->m_pAttachmentManager=this;
	pAttachment->m_Name = szName;


	if (type==CA_BONE) { 

		pAttachment->m_BoneID = nBoneID;//m_pSkelInstance->m_SkeletonAnim.GetIDByName(szBoneName);
		assert(pAttachment->m_BoneID<4096);

		pAttachment->m_pIAttachmentObject	=	0;
		pAttachment->m_AttAbsoluteDefault = m_pSkelInstance->m_SkeletonPose.m_AbsolutePose[pAttachment->m_BoneID];
		assert(pAttachment->m_AttAbsoluteDefault.IsValid());
		pAttachment->m_AttRelativeDefault.SetIdentity();
		pAttachment->m_FaceNr	=	0;
		pAttachment->m_AttFlags=	0;
		pAttachment->ProjectAttachment();
		m_arrAttachments.push_back(pAttachment);	
	}

	if (type==CA_FACE) 
	{ 
		pAttachment->m_pIAttachmentObject	=	0;
		pAttachment->ProjectAttachment();
		m_arrAttachments.push_back(pAttachment);	
		m_pSkinInstance->m_UpdateAttachmentMesh = true;
		m_pSkelInstance->m_UpdateAttachmentMesh = true;
	}

	if (type==CA_SKIN) 
	{ 
		m_arrAttachments.push_back(pAttachment);	
	}

	m_pSkelInstance->SetResetMode(rm);
	return pAttachment; 
}; 


void CAttachmentManager::PhysicalizeAttachment(int idx, int nLod, IPhysicalEntity *pent, const Vec3 &offset)
{
	IStatObj *pStatObj;
	IAttachment *pAttachment;
	if (!(pAttachment=GetInterfaceByIndex(idx)) || pAttachment->GetType()!=CA_BONE || !(pAttachment->GetFlags() & FLAGS_ATTACH_PHYSICALIZED) ||
			!pAttachment->GetIAttachmentObject() || !(pStatObj=pAttachment->GetIAttachmentObject()->GetIStatObj()) || !pStatObj->GetPhysGeom())
		return;

	int iBone = pAttachment->GetBoneID();
	pe_articgeomparams gp;
	Matrix34 mtx = Matrix34(m_pSkelInstance->m_SkeletonPose.m_AbsolutePose[iBone] * ((CAttachment*)pAttachment)->m_AttRelativeDefault);
	mtx.AddTranslation(offset);
	gp.pMtx3x4 = &mtx;
	//FIXME:
	gp.idbody = m_pSkelInstance->GetISkeletonPose()->getBonePhysParentOrSelfIndex(iBone,nLod);
	pent->AddGeometry(pStatObj->GetPhysGeom(),&gp,1000+idx);
}

void CAttachmentManager::PhysicalizeAttachment( int idx, IPhysicalEntity *pent, int nLod )
{
	if (!pent)
		if (!(pent = m_pSkelInstance->GetISkeletonPose()->GetCharacterPhysics()))
			return;
	PhysicalizeAttachment(idx,nLod,pent,m_pSkelInstance->m_SkeletonPose.GetOffset());
}

void CAttachmentManager::DephysicalizeAttachment(int idx, IPhysicalEntity *pent)
{
	pent->RemoveGeometry(1000+idx);
}

void CAttachmentManager::UpdatePhysicalizedAttachment(int idx, IPhysicalEntity *pent, const QuatT &offset)
{
	IStatObj *pStatObj;
	IAttachment *pAttachment;
	if (!(pAttachment=GetInterfaceByIndex(idx)) || pAttachment->GetType()!=CA_BONE || !(pAttachment->GetFlags() & FLAGS_ATTACH_PHYSICALIZED) ||
			!pAttachment->GetIAttachmentObject() || !(pStatObj=pAttachment->GetIAttachmentObject()->GetIStatObj()) || !pStatObj->GetPhysGeom())
		return;

	int iBone = pAttachment->GetBoneID();
	pe_params_part pp;
	Matrix34 mtx = Matrix34(offset*pAttachment->GetAttModelRelative());
	pp.partid = 1000+idx;
	pp.pMtx3x4 = &mtx;
	pent->SetParams(&pp);
}


//////////////////////////////////////////////////////////////////////////
void CAttachmentManager::ResetAdditionalRotation()
{
	for (AttachArray::iterator it = m_arrAttachments.begin(); it != m_arrAttachments.end(); ++it)
	{
		CAttachment *pAttachment = *it;
		// Reset addition facial rotation after update.
		pAttachment->m_additionalRotation.SetIdentity();
	}
}


int32 CAttachmentManager::RemoveAttachmentByInterface( const IAttachment* ptr )
{
	CAttachment* p=(CAttachment*)ptr;
	const char* name=p->m_Name;  
	return RemoveAttachmentByName(name);
}

int32 CAttachmentManager::RemoveAttachmentByName( const char* szName ) {

	int32 index = GetIndexByName( szName );
	//	assert(index!=-1);
	if (index==-1) return 0;

	CAttachment* pAttachment = (CAttachment*)GetInterfaceByIndex(index);
	assert(pAttachment);
	if (pAttachment==0) return 0;
	assert(pAttachment->m_Type==CA_BONE || pAttachment->m_Type==CA_FACE || pAttachment->m_Type==CA_SKIN);

		pAttachment->ClearBinding();
	//	delete(pAttachment);
		int32 size = m_arrAttachments.size();
		if (size==0) return 1;
		if (size<=index) return 1;
		delete (m_arrAttachments[index]);
		m_arrAttachments[index]=m_arrAttachments[size-1];	
		m_arrAttachments.pop_back();
		return 1;
};


//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

/*
namespace
{

	class Evil : public CCharInstance
	{
	public:
		Evil() : CCharInstance() { }

		void GetSkeletonPose( int nLOD, const Matrix34& rRenderMat34, QuatTS*& pBoneQuatsL, QuatTS*& pBoneQuatsS, QuatTS*& pMBBoneQuatsL, QuatTS*& pMBBoneQuatsS, Vec4 shapeDeformationData[], uint32 &HWSkinningFlags )
		{
			CSkinInstance::GetSkeletonPose( nLOD, rRenderMat34, pBoneQuatsL, pBoneQuatsS,pMBBoneQuatsL,pMBBoneQuatsS, shapeDeformationData, HWSkinningFlags );
		}
	};
	static Evil evil;
	void Mystify(ICharacterInstance *const p)
	{
		*(void **)p = *(void **)&evil;
	}
}*/

uint32 CAttachment::AddBinding( IAttachmentObject* pModel ) 
{	
	if (m_Type==CA_FACE || m_Type==CA_BONE)
	{
		if (m_pIAttachmentObject)
			m_pIAttachmentObject->Release();
		m_pIAttachmentObject=pModel;	
		return 1;	
	}

	//----------------------------------------------------------------------------------------------
	if (m_Type==CA_SKIN)
	{
		CSkinInstance* pInstanceMaster = m_pAttachmentManager->m_pSkelInstance;

		if (pModel) 
		{
			CSkinInstance* pInstanceSlave=(CSkinInstance*)pModel->GetICharacterInstance();
			if (pInstanceSlave==0)
			{
				//This is an serious error. We cannot attach a non-character
				g_pILog->LogError ("Subskin is no character. Attaching not possible" );
				assert(0);
				return 0;
			}

		//	pInstanceSlave->m_pIMasterAttachment=this;

			CCharacterModel* pModelMaster = ((CSkinInstance*)pInstanceMaster)->m_pModel;
			uint32 numBonesMaster = pModelMaster->m_pModelSkeleton->m_arrModelJoints.size();
			uint32 found=0;
			CCharacterModel* pModelSlave = pInstanceSlave->m_pModel;
			uint32 numBonesSlave = pModelSlave->m_pModelSkeleton->m_arrModelJoints.size();
			uint32 NotMatchingNames=0;
			if (numBonesSlave>numBonesMaster)
			{
				//This is an serious error. We cannot attach a slave to a master with less bones
				g_pILog->LogError ("SLAVE-skeleton needs to have less bone then MASTER-skeleton. SLAVE:%d MASTER:%d ", numBonesSlave, numBonesMaster );
				assert(!"SLAVE-skeleton needs to have less bone then MASTER-skeleton");
				//return 0;
			}

			m_arrRemapTable.resize(numBonesSlave);
			for(uint32 s=0; s<numBonesSlave; s++) 
			{
				found=0;
				const char * sn = pModelSlave->m_pModelSkeleton->m_arrModelJoints[s].GetJointName();
				for(uint32 m=0; m<numBonesMaster; m++) 
				{
					const char * mn = pModelMaster->m_pModelSkeleton->m_arrModelJoints[m].GetJointName();
					if (stricmp(sn,mn) == 0) 
					{
						found=1;
						m_arrRemapTable[s]=m;
						break;
					}
				}

				if (found==0)
				{
					//bones name of slave not found in master
					g_pILog->LogError ("Bone-names of SLAVE-skeleton don't match with bone-names of MASTER-skeleton:  %s", sn );
					NotMatchingNames++;
					assert(!"Missmatching bone names between master and slave skeletons!");
					//return 0;
				}
			} //for loop

			if (NotMatchingNames)
				return 0; //critical! incompatible skeletons. cant create skin-attachment

		}

		m_pIAttachmentObject=pModel;

		m_AttRelativeDefault.SetIdentity();
		m_AttAbsoluteDefault.SetIdentity();

		m_FaceNr = 0;
		m_BoneID = 0xffffffff;

		m_AttModelRelative.SetIdentity();
		m_AttModelRelativePrev.SetIdentity();
		m_AttWorldAbsolute.SetIdentity();
		return 1; 
	}
	return 0; 
}



uint32 CAttachmentManager::ProjectAllAttachment() 
{
	uint32 rm = m_pSkelInstance->GetResetMode();

	m_pSkelInstance->GetISkeletonPose()->SetDefaultPose();
	m_pSkelInstance->SetResetMode(1);

	uint32 numAttachments = m_arrAttachments.size();
	for (AttachArray::iterator it = m_arrAttachments.begin(); it != m_arrAttachments.end(); ++it)
	{
		CAttachment* pAttachment	= (*it);
		uint32 type = pAttachment->m_Type; 

		if (type==CA_FACE) 
		{
			CloseInfo cf=m_pSkinInstance->m_pModel->m_arrModelMeshes[m_pSkinInstance->m_pModel->m_nBaseLOD].FindClosestPointOnMesh( pAttachment->m_AttAbsoluteDefault.t );
			Vec3 x = (cf.tv1-cf.tv0).GetNormalized();
			Vec3 z = ((cf.tv1-cf.tv0)%(cf.tv0-cf.tv2)).GetNormalized();
			Vec3 y = z%x;
			QuatT TriMat=QuatT::CreateFromVectors(x,y,z,cf.middle);
			pAttachment->m_AttRelativeDefault	= TriMat.GetInverted() *  pAttachment->m_AttAbsoluteDefault ;
			pAttachment->m_FaceNr			=	cf.FaceNr;
		}

		if (type==CA_BONE) 
		{
			//ICryBone* pBone		=	m_pSkelInstance->GetBoneByIndex(pAttachment->m_BoneID);
			assert(pAttachment->m_BoneID<4096);
			QuatT BoneMat	= m_pSkelInstance->m_SkeletonPose.m_AbsolutePose[pAttachment->m_BoneID];	
			pAttachment->m_AttRelativeDefault = BoneMat.GetInverted() * pAttachment->m_AttAbsoluteDefault;
		}
	}


	UpdateLocationAttachments(QuatT(IDENTITY),0);
	m_pSkinInstance->m_UpdateAttachmentMesh = true ;
	m_pSkelInstance->m_UpdateAttachmentMesh = true ;
	m_pSkelInstance->SetResetMode(rm);
	return 1;
}

IAttachment* CAttachmentManager::GetInterfaceByName(  const char* szName ) 
{ 
	int32 idx = GetIndexByName( szName ); 
	if (idx==-1) return 0;
	return GetInterfaceByIndex( idx );
};

int32 CAttachmentManager::GetIndexByName( const char* szName ) 
{ 
	int num = GetAttachmentCount();
	for (int i = 0; i < num; i++)	
	{
		IAttachment* pA = GetInterfaceByIndex(i);
		if(stricmp(pA->GetName(),szName)==0) 
			return i;
	}
	return -1;
}

void CAttachmentManager::UpdateLocationAttachments(const QuatT &rPhysLocationNext,IAttachmentObject::EType *pOnlyThisType )
{

	CSkinInstance* pSkin = m_pSkinInstance;
	CCharInstance* pMaster = m_pSkelInstance;
	CAttachment* pAttachment = m_pSkelInstance->m_pSkinAttachment;
	if (pAttachment) 
	{
		uint32 type = pAttachment->GetType();
		if (type==CA_SKIN) 
			pMaster = pAttachment->m_pAttachmentManager->m_pSkelInstance; //this is a skin attachment. we need the skeleton-master
		else 
			assert(0);
	}
	assert(pSkin);
	assert(pMaster);
	CModelMesh* pSkinning = pSkin->m_pModel->GetModelMesh(m_pSkinInstance->m_pModel->m_nBaseLOD);



	if (pOnlyThisType==0)
	{
		//--------------------------------------------
		//--- create attachment mesh               ---
		//--------------------------------------------
		if (pSkin->m_UpdateAttachmentMesh && pSkinning)
		{
			uint32 AttIndex=0;
			m_arrAttSkin.empty();
			m_arrAttSkin.resize(0);
			m_arrAttSkinnedStream.empty();
			m_arrAttSkinnedStream.resize(0);

			uint32 numMergedSkins = pSkinning->m_arrExtVerticesCached.size();
			//move all connected faces into a new buffer
			for (AttachArray::iterator it = m_arrAttachments.begin(); it != m_arrAttachments.end(); ++it)
			{
				CAttachment* pAttachment	= (*it);
				if (pAttachment->m_Type==CA_FACE ) 
				{
					uint16 i0 = pAttachment->m_FaceNr*3+0;
					uint16 i1 = pAttachment->m_FaceNr*3+1;
					uint16 i2 = pAttachment->m_FaceNr*3+2;
					assert(i0<numMergedSkins);
					assert(i1<numMergedSkins);
					assert(i2<numMergedSkins);

					AttSkinVertex iv0(pSkinning->m_arrExtVerticesCached[ i0 ]);
					AttSkinVertex iv1(pSkinning->m_arrExtVerticesCached[ i1 ]);
					AttSkinVertex iv2(pSkinning->m_arrExtVerticesCached[ i2 ]);

				//	float fColor[4] = {0,1,0,1};
				//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"m_FaceNr: %d", pAttachment->m_FaceNr );	g_YLine+=16.0f;
				//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"iv0 (%f %f %f)", iv0.wpos1.x,iv0.wpos1.y,iv0.wpos1.z );	g_YLine+=16.0f;
				//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"iv1 (%f %f %f)", iv1.wpos1.x,iv1.wpos1.y,iv1.wpos1.z );	g_YLine+=16.0f;
				//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"iv2 (%f %f %f)", iv2.wpos1.x,iv2.wpos1.y,iv2.wpos1.z );	g_YLine+=16.0f;

					m_arrAttSkin.push_back(iv0);
					m_arrAttSkin.push_back(iv1);
					m_arrAttSkin.push_back(iv2);
					pAttachment->m_AttFaceNr=AttIndex;
					AttIndex+=3;
				}
			}
			m_arrAttSkinnedStream.resize(AttIndex);
			//m_arrAttMorphStream.resize(AttIndex);
			m_pSkinInstance->m_UpdateAttachmentMesh=0;

			//Timur, Not used for now.
			/*
			//create morph-tragets for all connected faces
			uint32 numMorphStreams = pSkinning->m_morphTargets.size();
			m_morphTargets.resize(numMorphStreams);
			for (uint32 m=0; m<numMorphStreams; m++)
			{
				m_morphTargets[m] = new CMorphTarget;
				uint32 numMorphs = pSkinning->m_morphTargets[m].m_vertices.size();
				for (uint32 i=0; i<numMorphs; i++)
				{
					CMorphTarget::Vertex mvtx = pSkinning->m_morphTargets[m].m_vertices[i];
					uint32 numAttVertices = m_arrAttSkin.size();
					for (uint32 a=0; a<numAttVertices; a++)
					{
						Vec3 morph = pSkinning->m_arrExtVertices[mvtx.nVertexId].wpos1;
						if (morph==m_arrAttSkin[a].wpos1)
						{
							mvtx.nVertexId=a;
							m_arrMorphTargets[m].m_arrAttMorph.push_back( MorphTarget );
						}
					}
				}
			}
			*/
		}
		SkinningAttSW( pSkinning );
	}


	//update attachmnets 
	for (AttachArray::iterator it = m_arrAttachments.begin(); it != m_arrAttachments.end(); ++it)
	{
		CAttachment* pAttachment	= (*it);
		
		uint32 numAttSkinnedStream = m_arrAttSkinnedStream.size();

		if (pOnlyThisType && pAttachment->m_pIAttachmentObject && *pOnlyThisType != pAttachment->m_pIAttachmentObject->GetAttachmentType())
			continue;

		if (pAttachment->m_Type==CA_FACE ) 
		{
			if (pSkinning)
			{
				Vec3 tv0 = m_arrAttSkinnedStream[ pAttachment->m_AttFaceNr+0 ];	
				Vec3 tv1 = m_arrAttSkinnedStream[ pAttachment->m_AttFaceNr+1 ];	
				Vec3 tv2 = m_arrAttSkinnedStream[ pAttachment->m_AttFaceNr+2 ];


				//float fColor[4] = {0,1,0,1};
				//uint32 numAttSkinnedStream = m_arrAttSkinnedStream.size();
				//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"numAttSkinnedStream: %d", numAttSkinnedStream );	g_YLine+=16.0f;
				//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"pAttachment->m_AttFaceNr: %d", pAttachment->m_AttFaceNr );	g_YLine+=16.0f;
				//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"tv0 (%f %f %f)", tv0.x,tv0.y,tv0.z );	g_YLine+=16.0f;
				//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"tv1 (%f %f %f)", tv1.x,tv1.y,tv1.z );	g_YLine+=16.0f;
				//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"tv2 (%f %f %f)", tv2.x,tv2.y,tv2.z );	g_YLine+=16.0f;


				Vec3 middle=(tv0+tv1+tv2)/3.0f;	
				Vec3 x = (tv1-tv0).GetNormalized();
				Vec3 z = ((tv1-tv0)%(tv0-tv2)).GetNormalized();
				Vec3 y = z%x;
				QuatT TriMat=QuatT::CreateFromVectors(x,y,z,middle);        
				pAttachment->m_AttModelRelative = TriMat * pAttachment->m_AttRelativeDefault * pAttachment->m_additionalRotation;         
				pAttachment->m_AttWorldAbsolute = rPhysLocationNext * pAttachment->m_AttModelRelative;
			}
		}

		if (pAttachment->m_Type==CA_BONE ) 
		{
			if (pAttachment->m_BoneID<4096)
			{
				//for normal bone-attachments we use this code
				QuatT AbsBoneMat	= pMaster->m_SkeletonPose.m_AbsolutePose[pAttachment->m_BoneID];        
				pAttachment->m_AttModelRelative = AbsBoneMat*pAttachment->m_AttRelativeDefault * pAttachment->m_additionalRotation;         
				pAttachment->m_AttWorldAbsolute = rPhysLocationNext * pAttachment->m_AttModelRelative;

				//Eyes-attachments are something very special
				uint32 lei=pMaster->m_pModel->m_pModelSkeleton->GetIdx(eIM_LeftEyeIdx);
				uint32 rei=pMaster->m_pModel->m_pModelSkeleton->GetIdx(eIM_RightEyeIdx);
				if (lei==pAttachment->m_BoneID || rei==pAttachment->m_BoneID)
				{
					Quat RelBoneMat	= pMaster->m_SkeletonPose.m_RelativePose[pAttachment->m_BoneID].q;
					AbsBoneMat	= AbsBoneMat * !RelBoneMat;
					pAttachment->m_AttRelativeDefault.q=RelBoneMat;          
					pAttachment->m_AttModelRelative = AbsBoneMat*pAttachment->m_AttRelativeDefault * pAttachment->m_additionalRotation;
					pAttachment->m_AttWorldAbsolute = rPhysLocationNext * pAttachment->m_AttModelRelative;
				}
			}
		}


		if (pAttachment->m_Type==CA_BONE || pAttachment->m_Type==CA_FACE) 
		{
			if (pAttachment->m_pHinge && !m_pSkelInstance->GetPhysicsRelinquished())
			{	// dangly attachments 
				// store center point in world coords, hinge angle speed
				// each step:
				//   calculate prev center point pos in the new coord frame, compute the new angle
				//   add gravity to angular vel, angular vel to angle
				//   if the angle is out of range, snap it and 0 the velocity
				//   (the gravity is queried from the physical entity and projected into local space
				//   re-calculate the matrix basing on the angle
				AABB bbox=AABB(Vec3(ZERO),Vec3(ZERO));
				Vec3 size,sz,ptHinge,ptloc,axisz,axisx,rotax,sg,gravity(0,0,-9.81f);
				Vec3i ic;
				Vec2 ptc,gravity2d;
				int bFlipped;
				float angle,velBody,gravityAng,dt=g_pISystem->GetITimer()->GetFrameTime(); 
				SAttachmentHinge &hinge = *pAttachment->m_pHinge;

				IAttachmentObject* pIAttachmentObject = pAttachment->GetIAttachmentObject();
				if (pIAttachmentObject)
					bbox = pIAttachmentObject->GetAABB();

				ic.z = hinge.idxEdge & 3;
				ic.x = ic.z+1+(hinge.idxEdge>>3&1); ic.x -= 3 & (2-ic.x)>>31;
				ic.y = 3-ic.x-ic.z;
				bFlipped = 1^hinge.idxEdge>>3&1^hinge.idxEdge>>2&1^hinge.idxEdge>>4&1;
				sz=size = bbox.GetSize()*0.5f; sz[ic.y] = 0;
				sz[ic.x] *= (sg[ic.x]=(hinge.idxEdge>>3&2)-1.0f);
				sz[ic.z] *= (sg[ic.z]=(hinge.idxEdge>>1&2)-1.0f);
				ptHinge = bbox.GetCenter()+sz;
				axisz.zero()[ic.z] = -sg[ic.z];
				axisx.zero()[ic.x] = -sg[ic.x];
				rotax = axisx%axisz;
				//	gravity = pAttachment->m_AttAbsolute.GetInvertedFast().TransformVector(gravity);
				gravity = !pAttachment->m_AttWorldAbsolute.q * gravity;

				ptloc = pAttachment->m_AttWorldAbsolute.GetInverted()*hinge.wcenter;
				ptc.set((ptloc[ic.x]-ptHinge[ic.x])*-sg[ic.x], (ptloc[ic.z]-ptHinge[ic.z])*-sg[ic.z]);
				gravity2d.set(gravity[ic.x]*-sg[ic.x], gravity[ic.z]*-sg[ic.z]);

				f32 fLength = ptc.GetLength2();
				if (fLength==0.0f) 
					fLength=0.00001f;

				gravityAng = (ptc^gravity2d)/fLength;
				angle = atan2(ptc.y, ptc.x);
				velBody = iszero(dt) ? 0.0f : (hinge.angle-angle)/dt;
				angle += hinge.velAng*dt; 
				hinge.velAng = (hinge.velAng+gravityAng*dt)*max(0.0f,1-hinge.damping*dt);
				if (angle<0)
					angle=0, hinge.velAng=max(velBody,hinge.velAng);
				else if (angle>hinge.maxAng*(gf_PI/180.0f))
					angle=hinge.maxAng*(gf_PI/180.0f), hinge.velAng=min(velBody,hinge.velAng);
				hinge.angle = angle;

				Quat R = Quat::CreateRotationAA(angle,rotax);
				QuatT Rt(R,ptHinge-R*ptHinge);      
				pAttachment->m_AttModelRelative = pAttachment->m_AttModelRelative*Rt; 
				pAttachment->m_AttWorldAbsolute = pAttachment->m_AttWorldAbsolute*Rt;
				sz.zero()[ic.z] = size[ic.z]*sg[ic.z];
				hinge.wcenter = pAttachment->m_AttWorldAbsolute*(bbox.GetCenter()+sz);
			}
		}
	}
}





void CAttachmentManager::UpdateLocationAttachmentsFast(const QuatT &rPhysLocationNext)
{
	//update attachments 
	for (AttachArray::iterator it = m_arrAttachments.begin(); it != m_arrAttachments.end(); ++it)
	{
		CAttachment* pAttachment	= (*it);
		IAttachmentObject* pIAttachmentObject = pAttachment->m_pIAttachmentObject;

		if (pAttachment->m_Type==CA_FACE) 
		{
			pAttachment->m_AttModelRelative.SetIdentity();         
			pAttachment->m_AttWorldAbsolute = rPhysLocationNext;
		}

		if (pAttachment->m_Type==CA_BONE ) 
		{
			if (pIAttachmentObject && pIAttachmentObject->GetAttachmentType() == IAttachmentObject::eAttachment_Entity)
			{
				//for normal bone-attachments we use this code
				QuatT AbsBoneMat	= m_pSkelInstance->m_SkeletonPose.m_AbsolutePose[pAttachment->m_BoneID];        
				pAttachment->m_AttModelRelative = AbsBoneMat*pAttachment->m_AttRelativeDefault * pAttachment->m_additionalRotation;         
				pAttachment->m_AttWorldAbsolute = rPhysLocationNext * pAttachment->m_AttModelRelative;
			}
			else
			{
				pAttachment->m_AttModelRelative.SetIdentity();         
				pAttachment->m_AttWorldAbsolute = rPhysLocationNext;
			}
		}
	}

}




//////////////////////////////////////////////////////////////////////////
// skinning of internal vertices 
//////////////////////////////////////////////////////////////////////////
void CAttachmentManager::SkinningAttSW( CModelMesh* pSkinning )
{

#ifdef DEFINE_PROFILER_FUNCTION
	DEFINE_PROFILER_FUNCTION();
#endif

	uint32 HowManyVertices = m_arrAttSkin.size();
	if (HowManyVertices==0) return;

	//-----------------------------------------------------------------------
	//----           Create the internal morph-target stream             ----
	//-----------------------------------------------------------------------
	//uint32 numAttVertices = m_arrAttMorphStream.size();	
	//assert(numAttVertices);
	//memset(&m_arrAttMorphStream[0],0,numAttVertices*sizeof(Vec3));

	// Timur, Not used for now.
	/*
	if (m_pSkelInstance->NeedMorph() && Console::GetInst().ca_UseMorph)
	{
		uint32 num=m_pSkelInstance->m_arrMorphEffectors.size();
		for (uint32 nMorphEffector=0; nMorphEffector<num; ++nMorphEffector)
		{
			const CryModEffMorph& rMorphEffector = m_pSkelInstance->m_arrMorphEffectors[nMorphEffector];
			int nMorphTargetId = rMorphEffector.getMorphTargetId ();
			if (nMorphTargetId < 0)
				continue;
			const AttMorphTargets& rMorphSkin = m_arrMorphTargets[nMorphTargetId];
			float fBlending = rMorphEffector.getBlending();
			uint32 num = rMorphSkin.m_arrAttMorph.size();
			for(uint32 i=0; i<num; i++) 
			{
				// add all morph-values to the stream to use subsequently in skinning
				uint32 idx = rMorphSkin.m_arrAttMorph[i].nVertexId;
				m_arrAttMorphStream[idx]+=rMorphSkin.m_arrAttMorph[i].ptVertex*fBlending;
			}
		}
	}
	*/

	//-----------------------------------------------------------------------
	//----    Create 8 Vec4 vectors that contain the blending values     ----
	//---                    to blend between 3 models                    ---
	//-----------------------------------------------------------------------

	CSkinInstance* pSkin = m_pSkinInstance;
	CCharInstance* pMaster = m_pSkelInstance;
	CAttachment* pAttachment = m_pSkinInstance->m_pSkinAttachment;
	if (pAttachment) 
	{
		uint32 type = pAttachment->GetType();
		if (type==CA_SKIN) 
			pMaster = pAttachment->m_pAttachmentManager->m_pSkelInstance; //this is a skin attachment. we need the skeleton-master
		else 
			assert(0);
	}
	assert(pMaster);

	uint32 rm = pMaster->GetResetMode();
	f32 MorphArray[8] = { 0,0,0,0,0,0,0,0};

	if (rm==0) 
	{ 
		f32* pMorphValues = pMaster->GetShapeDeformArray();
		MorphArray[0]=pMorphValues[0];
		MorphArray[1]=pMorphValues[1];
		MorphArray[2]=pMorphValues[2];
		MorphArray[3]=pMorphValues[3];
		MorphArray[4]=pMorphValues[4];
		MorphArray[5]=pMorphValues[5];
		MorphArray[6]=pMorphValues[6];
		MorphArray[7]=pMorphValues[7];
	}

	Vec4 VertexRegs[8];
	for (uint32 i=0; i<8; i++) 
	{
		if (MorphArray[i]<0.0f) 
		{
			VertexRegs[i].x	=	1-(MorphArray[i]+1);
			VertexRegs[i].y	=	MorphArray[i]+1;
			VertexRegs[i].z	=	0;
		} else {
			VertexRegs[i].x	=	0;
			VertexRegs[i].y	=	1-MorphArray[i];
			VertexRegs[i].z	=	MorphArray[i];
		}
	}

	//--------------------------------------------------------
	//----   this part of code belongs into hardware      ----
	//---  this loop is a simulation of the vertex-shader  ---
	//--------------------------------------------------------
	assert(m_arrAttSkinnedStream.size()==HowManyVertices);

	QuatTS arrNewSkinQuat[MAX_JOINT_AMOUNT+1];	//bone quaternions for skinning (entire skeleton)
	uint32 iActiveFrame		=	pMaster->m_iActiveFrame;
	if(pAttachment)
	{
		uint32 numRemapJoints	= pAttachment->m_arrRemapTable.size();
		for (uint32 i=0; i<numRemapJoints; i++)	arrNewSkinQuat[i]=pMaster->m_arrNewSkinQuat[iActiveFrame][pAttachment->m_arrRemapTable[i]];
	}
	else
	{
		uint32 numJoints	= pMaster->m_SkeletonPose.m_AbsolutePose.size();
		cryMemcpy(arrNewSkinQuat, &pMaster->m_arrNewSkinQuat[iActiveFrame][0],	sizeof(QuatTS)*numJoints);
	}

	if (Console::GetInst().ca_SphericalSkinning) 
	{
		for(uint32 i=0; i<HowManyVertices; i++ ) 
		{
			//spherical skinning
			//create the final vertex for skinning (blend between 3 characters)
#if defined(XENON)
			uint8 idx		=	m_arrAttSkin[i].color.r; 
#else
			uint8 idx		=	m_arrAttSkin[i].color.a; 
			assert(idx<8);
#endif
			Vec3 v	= m_arrAttSkin[i].wpos0*VertexRegs[idx].x + m_arrAttSkin[i].wpos1*VertexRegs[idx].y + m_arrAttSkin[i].wpos2*VertexRegs[idx].z;

			//apply morph-targets
			//v+=m_arrAttMorphStream[i];

			//get indices for bones (always 4 indices per vertex)
			uint32 b0 = m_arrAttSkin[i].boneIDs[0];
			uint32 b1 = m_arrAttSkin[i].boneIDs[1];
			uint32 b2 = m_arrAttSkin[i].boneIDs[2];
			uint32 b3 = m_arrAttSkin[i].boneIDs[3];

			//get indices for vertices (always 4 weights per vertex)
			f32 w0 = m_arrAttSkin[i].weights[0];
			f32 w1 = m_arrAttSkin[i].weights[1];
			f32 w2 = m_arrAttSkin[i].weights[2];
			f32 w3 = m_arrAttSkin[i].weights[3];
			//assert(fabsf((w0+w1+w2+w3)-1.0f)<0.0001f);

			const QuatD& q0=(const QuatD&)arrNewSkinQuat[b0];
			const QuatD& q1=(const QuatD&)arrNewSkinQuat[b1];
			const QuatD& q2=(const QuatD&)arrNewSkinQuat[b2];
			const QuatD& q3=(const QuatD&)arrNewSkinQuat[b3];
			QuatD wquat = (q0*w0 +  q1*w1 + q2*w2 +	q3*w3);

			f32 l=1.0f/wquat.nq.GetLength();
			wquat.nq*=l;
			wquat.dq*=l;
			
			Vec3 bv			=	QuatT(wquat)*v;
			m_arrAttSkinnedStream[i] = bv;

		}
	}
	else
	{
		//linar skinning
		for(uint32 i=0; i<HowManyVertices; i++ ) 
		{
			//create the final vertex for skinning (blend between 3 characters)
#if defined(XENON)
			uint8 idx		=	m_arrAttSkin[i].color.r; 
#else
			uint8 idx		=	m_arrAttSkin[i].color.a; 
			assert(idx<8);
#endif
			Vec3 v	= m_arrAttSkin[i].wpos0*VertexRegs[idx].x + m_arrAttSkin[i].wpos1*VertexRegs[idx].y + m_arrAttSkin[i].wpos2*VertexRegs[idx].z;

			//apply morph-targets
			//v+=m_arrAttMorphStream[i];

			//get indices for bones (always 4 indices per vertex)
			uint32 b0 = m_arrAttSkin[i].boneIDs[0];
			uint32 b1 = m_arrAttSkin[i].boneIDs[1];
			uint32 b2 = m_arrAttSkin[i].boneIDs[2];
			uint32 b3 = m_arrAttSkin[i].boneIDs[3];

			//get indices for vertices (always 4 weights per vertex)
			f32 w0 = m_arrAttSkin[i].weights[0];
			f32 w1 = m_arrAttSkin[i].weights[1];
			f32 w2 = m_arrAttSkin[i].weights[2];
			f32 w3 = m_arrAttSkin[i].weights[3];
			//assert(fabsf((w0+w1+w2+w3)-1.0f)<0.0001f);

			//do the skinning transformation 
			const QuatTS& m0 = arrNewSkinQuat[b0];
			const QuatTS& m1 = arrNewSkinQuat[b1];
			const QuatTS& m2 = arrNewSkinQuat[b2];
			const QuatTS& m3 = arrNewSkinQuat[b3];
			Vec3 v0=m0*v*w0;
			Vec3 v1=m1*v*w1;
			Vec3 v2=m2*v*w2;
			Vec3 v3=m3*v*w3;
			m_arrAttSkinnedStream[i] = v0+v1+v2+v3;
		}
	}



}





void CAttachmentManager::DrawAttachments(const SRendParams& rRendParams, Matrix34& WorldMat34)
{
#ifdef DEFINE_PROFILER_FUNCTION
	DEFINE_PROFILER_FUNCTION();
#endif

	if (Console::GetInst().ca_DrawAttachments==0)
		return;

	Matrix34_f64 WMatrixD;
	g_pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );

	uint32 numAttachments = m_arrAttachments.size();
	for (uint32 i=0; i<numAttachments; i++)
	{


		CAttachment& ainst	= *m_arrAttachments[i];

		if (Console::GetInst().ca_DrawFaceAttachments==0 && ainst.m_Type==CA_FACE)
			continue;

		uint32 InRecursion = (uint32)(UINT_PTR)g_pIRenderer->EF_Query(EFQ_RecurseLevel);
		uint32 InShadow = rRendParams.dwFObjFlags & FOB_RENDER_INTO_SHADOWMAP;

		if (InRecursion>1)
		{
			if (ainst.m_HideInRecursion) 
				continue;
		}
		else if (InShadow)
		{
			if (ainst.m_HideInShadowPass) 
				continue;
		}
		else 
		{
			if (ainst.m_HideInMainPass) 
				continue;
		}

		Matrix34 FinalMat34 = WorldMat34 * Matrix34(ainst.m_AttModelRelative);

		if (ainst.m_pIAttachmentObject==0) 
		{
			if ((ainst.m_Type==CA_BONE || ainst.m_Type==CA_FACE) && Console::GetInst().ca_DrawEmptyAttachments) 
			{
				Vec3 pos =  FinalMat34.GetTranslation();
				static Ang3 angle1(0,0,0); 
				static Ang3 angle2(0,0,0); 
				static Ang3 angle3(0,0,0);
				angle1 += Ang3(0.01f,+0.02f,+0.03f);
				angle2 -= Ang3(0.01f,-0.02f,-0.03f);
				angle3 += Ang3(0.03f,-0.02f,+0.01f);

				AABB aabb1 = AABB(Vec3( -0.05f, -0.05f, -0.05f),Vec3( +0.05f, +0.05f, +0.05f));
				AABB aabb2 = AABB(Vec3(-0.005f,-0.005f,-0.005f),Vec3(+0.005f,+0.005f,+0.005f));

				Matrix33 m33;	OBB obb;

				m33=Matrix33::CreateRotationXYZ(angle1);
				obb=OBB::CreateOBBfromAABB( m33,aabb1 );
				g_pAuxGeom->DrawOBB(obb,pos,0,RGBA8(0xff,0x00,0x1f,0xff),eBBD_Extremes_Color_Encoded);
				obb=OBB::CreateOBBfromAABB( m33,aabb1 );
				g_pAuxGeom->DrawOBB(obb,pos,0,RGBA8(0xff,0x00,0x1f,0xff),eBBD_Extremes_Color_Encoded);

				m33=Matrix33::CreateRotationXYZ(angle2);
				obb=OBB::CreateOBBfromAABB( m33,aabb1 );
				g_pAuxGeom->DrawOBB(obb,pos,0,RGBA8(0xff,0x00,0x1f,0xff),eBBD_Extremes_Color_Encoded);
				obb=OBB::CreateOBBfromAABB( m33,aabb2 );
				g_pAuxGeom->DrawOBB(obb,pos,0,RGBA8(0xff,0x00,0x1f,0xff),eBBD_Extremes_Color_Encoded);

				m33=Matrix33::CreateRotationXYZ(angle3);

				obb=OBB::CreateOBBfromAABB( m33,aabb1 );
				g_pAuxGeom->DrawOBB(obb,pos,0,RGBA8(0xff,0x00,0x1f,0xff),eBBD_Extremes_Color_Encoded);
				obb=OBB::CreateOBBfromAABB( m33,aabb2 );
				g_pAuxGeom->DrawOBB(obb,pos,0,RGBA8(0xff,0x00,0x1f,0xff),eBBD_Extremes_Color_Encoded);

				obb=OBB::CreateOBBfromAABB( ainst.m_AttModelRelative.q,aabb1 );
				g_pAuxGeom->DrawOBB(obb,pos,0,RGBA8(0xff,0xff,0xff,0xff),eBBD_Extremes_Color_Encoded);
			}
			continue;
		}






		//---------------------------------------------------------------------
		//---           send attached objects to renderer                 -----
		//---------------------------------------------------------------------
		AABB caabb =	ainst.m_pIAttachmentObject->GetAABB();
		f32 radius	=	(caabb.max-caabb.min).GetLengthFast()*0.5f;

		//if radius is zero, then the object is most probably not visible and we can continue 			
		if (radius==0)
			continue;

		f32 fScaledDist = rRendParams.fDistance/radius;
		if (fScaledDist > Console::GetInst().ca_AttachmentCullingRation) 
			continue;

		if (Console::GetInst().ca_DrawAttachmentOBB) 
		{
			OBB obb2=OBB::CreateOBBfromAABB( Matrix33(FinalMat34),caabb );
			if (ainst.m_Type==CA_BONE) 
				g_pAuxGeom->DrawOBB(obb2,FinalMat34.GetTranslation(),0,RGBA8(0x1f,0x00,0xff,0xff),eBBD_Extremes_Color_Encoded);
			if (ainst.m_Type==CA_FACE) 
				g_pAuxGeom->DrawOBB(obb2,FinalMat34.GetTranslation(),0,RGBA8(0xff,0x00,0x1f,0xff),eBBD_Extremes_Color_Encoded);
			//Vec3 WPos		= Matrix33(FinalMat34)*obb2.c+FinalMat34.GetTranslation();
			//g_pAuxGeom->DrawSphere( WPos,radius, RGBA8(0xff,0x00,0x1f,0xff) );
		}

		SRendParams rParams (rRendParams);

    Matrix34 pAttachPrevMat34 = Matrix34(ainst.m_AttModelRelativePrev);
    ainst.m_AttModelRelativePrev = QuatT(FinalMat34);
    if( rRendParams.pPrevMatrix && ((m_pSkelInstance->m_fFPWeapon!=0) == m_pSkelInstance->m_bPrevFrameFPWeapon) )
      rParams.pPrevMatrix = &pAttachPrevMat34;
    else
      rParams.pPrevMatrix = &FinalMat34;

		rParams.pMatrix = &FinalMat34;
    
    // Get material layer manager
    //rParams.pMaterialLayerManager = (rParams.pMaterialLayerManager)? rParams.pMaterialLayerManager : ainst.GetMaterialLayerManager();

		// this is required to avoid the attachments using the parent character material (this is the material that overrides the default material in the attachment)
		rParams.pMaterial = NULL;

		if (m_pSkelInstance->rp.m_nFlags & CS_FLAG_DRAW_NEAR)
		{
			rParams.dwFObjFlags |= FOB_NEAREST;
		}

	//	if (rParams.dwFObjFlags & FOB_NEAREST)
		{
			rParams.dwFObjFlags |= FOB_HIGHPRECISION;
			QuatT_f64 EntityMatrix34	=	QuatT_f64(WorldMat34);
			QuatT_f64 LMatrix =	QuatT_f64(ainst.m_AttModelRelative * ainst.m_additionalRotation);
			ainst.m_WMatrixD = Matrix34_f64(EntityMatrix34*LMatrix);
			rParams.pCCObjCustomData = &ainst.m_WMatrixD;
//			WMatrixD = Matrix34_f64(EntityMatrix34*LMatrix);
//			rParams.pCCObjCustomData = &WMatrixD;
		}

		ainst.m_pIAttachmentObject->RenderAttachment(rParams, m_arrAttachments[i] );
	}  

}







IAttachment* CAttachmentManager::GetInterfaceByIndex( uint32 c) 
{ 
	size_t size=m_arrAttachments.size();
	if (size==0) return 0;
	if (size<=c) return 0;
	return m_arrAttachments[c];
};

void CAttachmentManager::RemoveAttachmentByIndex(uint32 n) 
{ 
	size_t size = m_arrAttachments.size();
	if (size==0) return;
	if (size<=n) return;
	m_arrAttachments[n]->ClearBinding();
	delete(m_arrAttachments[n]);
	m_arrAttachments[n]=m_arrAttachments[size-1];	
	m_arrAttachments.pop_back();
}

uint32 CAttachmentManager::RemoveAllAttachments() 
{
	uint32 counter = GetAttachmentCount();
	for (uint32 i=counter; i>0; i--) 
		RemoveAttachmentByIndex(i-1);
	return 1;
}



void CAttachmentManager::Serialize(TSerialize ser)
{

	if (ser.GetSerializationTarget() != eST_Network)
	{
		ser.BeginGroup("CAttachmentManager");
		int32 numAttachments=GetAttachmentCount();			
		for (int32 i=0; i<numAttachments; i++)
		{
			CAttachment* pCAttachment = (CAttachment*)GetInterfaceByIndex(i);
			pCAttachment->Serialize(ser);
		}
		ser.EndGroup();
		uint32 fff=0;
	}
}



//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------


uint32 CAttachment::SetType(uint32 type, const char* szBoneName ) 
{ 
	m_Type		=	type;
	m_BoneID	=	0xffffffff;

	if (type==CA_BONE) 
	{
		if(!szBoneName)	{ assert(0); return 0; }
		int BoneId = m_pAttachmentManager->m_pSkelInstance->m_SkeletonPose.GetJointIDByName(szBoneName);
		if(BoneId < 0)
		{
			// this is a severe bug if the bone name is invalid and someone tries to attach something
			g_pILog->LogError ("AttachObjectToBone is called for bone \"%s\", which is not in the model \"%s\". Ignoring, but this may cause a crash because the corresponding object won't be detached after it's destroyed", szBoneName, m_pAttachmentManager->m_pSkelInstance->m_pModel->GetModelFilePath());
			return 0;
		}

		if (m_AlignBoneAttachment)
		{
			QuatT m34 = m_pAttachmentManager->m_pSkelInstance->m_SkeletonPose.m_AbsolutePose[BoneId];
			m_AttAbsoluteDefault=m34;
			m_AttRelativeDefault.SetIdentity();
		}

		m_BoneID			=	BoneId;
		ProjectAttachment();
	}

	if (type==CA_FACE) 
	{
		m_AttAbsoluteDefault.SetIdentity();
		ProjectAttachment();
	}

	if (type==CA_SKIN) 
	{
		m_AttAbsoluteDefault.SetIdentity();
		m_AttRelativeDefault.SetIdentity();
	}

	return 1;
}; 


uint32 CAttachment::ProjectAttachment() 
{ 

	static ColorB color = RGBA8(0xff,0xff,0xff,0x00);
	color.r+=0x13;
	color.g+=0x23;
	color.b+=0x33;

	CSkinInstance*					pSkinInstance = m_pAttachmentManager->m_pSkinInstance;
	CCharInstance*	pSkelInstance = m_pAttachmentManager->m_pSkelInstance;

	uint32 rm = pSkelInstance->GetResetMode();
	pSkelInstance->GetISkeletonPose()->SetDefaultPose();
	pSkelInstance->SetResetMode(1);
	
	pSkinInstance->m_UpdateAttachmentMesh = true;
	pSkelInstance->m_UpdateAttachmentMesh = true;

//#pragma message("TODO: CAttachment::ProjectAttachment() shouldn't render debug helpers by default. Ivo, please fix.")
//#pragma message("Yeahhh, I'll do it later! Good work needs time, ya know?")


	SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
	renderFlags.SetFillMode( e_FillModeWireframe );
	renderFlags.SetDrawInFrontMode( e_DrawInFrontOn );
	g_pAuxGeom->SetRenderFlags( renderFlags );

	if (m_Type==CA_FACE) 
	{

		CCharacterModel* pModel = pSkinInstance->m_pModel;
		if (pModel->m_ObjectType==CHR)
		{
			CloseInfo cf=pModel->m_arrModelMeshes[pModel->m_nBaseLOD].FindClosestPointOnMesh( m_AttAbsoluteDefault.t );
			Vec3 x = (cf.tv1-cf.tv0).GetNormalized();
			Vec3 z = ((cf.tv1-cf.tv0)%(cf.tv0-cf.tv2)).GetNormalized();
			Vec3 y = z%x;
			QuatT TriMat=QuatT::CreateFromVectors(x,y,z,cf.middle);
			m_AttRelativeDefault	= TriMat.GetInverted() * m_AttAbsoluteDefault;
			m_FaceNr			=	cf.FaceNr;
			if (pSkelInstance->m_SkeletonAnim.GetCharEditMode())
			{
				//float fColor[4] = {0,1,0,1};
				//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"m_FaceNr: %d", m_FaceNr );	g_YLine+=16.0f;
				//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"v0 (%f %f %f)", x.x,x.y,x.z );	g_YLine+=16.0f;
				//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"v1 (%f %f %f)", y.x,y.y,y.z );	g_YLine+=16.0f;
				//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"v2 (%f %f %f)", z.x,z.y,z.z );	g_YLine+=16.0f;

				g_pAuxGeom->DrawLine( cf.middle, color, m_AttAbsoluteDefault.t,color );
				g_pAuxGeom->DrawLine( cf.tv0,RGBA8(0xff,0x00,0x00,0x00), cf.tv1,RGBA8(0x00,0xff,0x00,0x00) );
				g_pAuxGeom->DrawLine( cf.tv1,RGBA8(0x00,0xff,0x00,0x00), cf.tv2,RGBA8(0x00,0x00,0xff,0x00) );
				g_pAuxGeom->DrawLine( cf.tv2,RGBA8(0x00,0x00,0xff,0x00), cf.tv0,RGBA8(0xff,0x00,0x00,0x00) );
			}
		}

	}

	if (m_Type==CA_BONE) 
	{
		QuatT BoneMat = pSkelInstance->m_SkeletonPose.m_AbsolutePose[m_BoneID];
		m_AttRelativeDefault = BoneMat.GetInverted() * m_AttAbsoluteDefault;
		m_AttWorldAbsolute = BoneMat;
		if (pSkelInstance->m_SkeletonAnim.GetCharEditMode())
			g_pAuxGeom->DrawLine(BoneMat.t,color,  m_AttAbsoluteDefault.t,color );
	}

	pSkelInstance->SetResetMode(rm);
	return 1;
}

void CAttachment::ClearBinding() 
{ 
	if (m_pIAttachmentObject)
		m_pIAttachmentObject->Release();
	m_pIAttachmentObject=0;
}; 

void CAttachment::GetHingeParams(int &idx,float &limit,float &damping) 
{
	if (!m_pHinge) 
	{ 
		idx=-1; limit=120.0f; damping=2.0f; 
	} else 
	{ 
		idx=m_pHinge->idxEdge; limit=m_pHinge->maxAng; damping=m_pHinge->damping; 
	}
}

void CAttachment::SetHingeParams(int idx,float limit,float damping)
{
	if (idx==-1)
	{
		if (m_pHinge) delete m_pHinge;
		m_pHinge = 0;
	}	else
	{
		if (!m_pHinge) 
		{
			m_pHinge = new SAttachmentHinge;
			m_pHinge->wcenter.zero();
			m_pHinge->angle = 0;
			m_pHinge->velAng = 0;
		}
		m_pHinge->idxEdge = idx;
		m_pHinge->maxAng = limit;
		m_pHinge->damping = damping;
	}
}



void CAttachment::Serialize(TSerialize ser)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		ser.BeginGroup("CAttachment");
		ser.Value("m_HideInMainPass", m_HideInMainPass);
		ser.EndGroup();
	}
}



size_t CAttachmentManager::SizeOfThis()
{
	size_t nSize = 0;
	nSize += m_arrAttSkin.capacity() * sizeof(AttSkinVertex);
	nSize += m_arrAttSkinnedStream.capacity() * sizeof(Vec3);

	uint32 numAttachments= m_arrAttachments.size();
	for(uint32 i=0; i<numAttachments; i++)
		nSize += m_arrAttachments[i]->SizeOfThis();

	return nSize;
}

size_t CAttachment::SizeOfThis()
{
	size_t nSize = sizeof(CAttachment) + m_Name.capacity();
	nSize += m_arrRemapTable.capacity() * sizeof(uint8);
	if (m_pHinge)
		nSize += sizeof(SAttachmentHinge);
	return  nSize;
}

