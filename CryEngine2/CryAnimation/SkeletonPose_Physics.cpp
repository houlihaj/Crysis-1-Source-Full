#include "stdafx.h"
#include <CryHeaders.h>
#include <CryCompiledFile.h>

#include "Model.h"
#include "CharacterInstance.h"
#include "CharacterManager.h"

///////////////////////////////////////////// physics stuff /////////////////////////////////////////////////

// given the bone index, (INDEX, NOT ID), returns this bone's parent index
uint32 CSkeletonPose::getBoneParentIndex (uint32 nBoneIndex)
{
	assert (nBoneIndex<m_AbsolutePose.size());

	CModelJoint* pSkel = &m_pInstance->m_pModel->m_pModelSkeleton->m_arrModelJoints[nBoneIndex];
	int32 pidx = nBoneIndex + pSkel->getParentIndexOffset();

	int32 pidx2=m_parrModelJoints[nBoneIndex].m_idxParent;
	if (pidx2<0)	
		pidx2=0;
	assert(pidx2==pidx);

	return pidx;
}

void CSkeletonPose::SetRagdollDefaultPose()
{

	if (m_arrPhysicsJoints.empty())
		InitPhysicsSkeleton();

	uint32 numJoints	=	m_AbsolutePose.size();
	for (uint32 i=0; i<numJoints; ++i)
	{
		int32 pidx=m_parrModelJoints[i].m_idxParent;
		if ( pidx>=0 )
		{
			int32 ParentIdx = m_parrModelJoints[i].m_idxParent;
			m_arrPhysicsJoints[i].m_DefaultRelativeQuat = m_AbsolutePose[i];
			if (ParentIdx>=0)
				m_arrPhysicsJoints[i].m_DefaultRelativeQuat = m_AbsolutePose[ParentIdx].GetInverted() * m_AbsolutePose[i];

			CModelJoint *pParent,*pPhysParent;
			for(int nLod=0; nLod<2; nLod++)
			{
				for(pParent=pPhysParent=m_parrModelJoints[i].getParent(); pPhysParent && !pPhysParent->m_PhysInfo[nLod].pPhysGeom; pPhysParent=pPhysParent->getParent());
				if (pPhysParent) 
				{
					m_arrPhysicsJoints[i].m_qRelPhysParent[nLod] = !m_AbsolutePose[pParent-&m_parrModelJoints[0]].q * m_AbsolutePose[pPhysParent-&m_parrModelJoints[0]].q;
				}
				else 
				{
					m_arrPhysicsJoints[i].m_qRelPhysParent[nLod].SetIdentity();
				}

			}
		}
		else
			m_parrModelJoints[i].m_DefaultRelPose = m_parrModelJoints[i].m_DefaultAbsPose;
	}
}







//////////////////////////////////////////////////////////////////////////
// finds the first physicalized parent of the given bone (bone given by index)
int CSkeletonPose::getBonePhysParentIndex(int iBoneIndex, int nLod)
{
	int iPrevBoneIndex;
	do 
	{
		iBoneIndex = getBoneParentIndex(iPrevBoneIndex = iBoneIndex);
	} 
	while (iBoneIndex!=iPrevBoneIndex && !GetModelJointPointer(iBoneIndex)->m_PhysInfo[nLod].pPhysGeom);

	return iBoneIndex==iPrevBoneIndex ? -1 : iBoneIndex;
}
int CSkeletonPose::getBonePhysParentOrSelfIndex(int iBoneIndex, int nLod)
{
	int iNextBoneIndex;
	for( ;!GetModelJointPointer(iBoneIndex)->m_PhysInfo[nLod].pPhysGeom; iBoneIndex=iNextBoneIndex)
		if ((iNextBoneIndex=getBoneParentIndex(iBoneIndex))==iBoneIndex)
			return -1;
	return iBoneIndex;
}




//////////////////////////////////////////////////////////////////////////
// finds the first physicalized child (or itself) of the given bone (bone given by index)
// returns -1 if it's not physicalized
int CSkeletonPose::getBonePhysChildIndex (int iBoneIndex, int nLod)
{
	CModelJoint* pBoneInfo = GetModelJointPointer(iBoneIndex);
	if (pBoneInfo->m_PhysInfo[nLod].pPhysGeom)
		return iBoneIndex;
	unsigned numChildren = pBoneInfo->numChildren();
	unsigned nFirstChild = pBoneInfo->getFirstChildIndexOffset() + iBoneIndex;
	if (nFirstChild==iBoneIndex)
		return -1;
	for (unsigned nChild = 0; nChild < numChildren; ++nChild)
	{
		int nResult = getBonePhysChildIndex(nFirstChild + nChild, nLod);
		if (nResult >= 0)
			return nResult;
	}
	return -1;
}

int CSkeletonPose::TranslatePartIdToDeadBody(int partid)
{
	if ((unsigned int)partid>=(unsigned int)m_AbsolutePose.size())
		return -1;

	int numLODs = m_pInstance->m_pModel->m_arrModelMeshes.size();
	int nLod = min(numLODs-1,1);
	if (GetModelJointPointer(partid)->m_PhysInfo[nLod].pPhysGeom)
		return partid;

	return getBonePhysParentIndex(partid,nLod);
}


float CSkeletonPose::AssessPoseSimilarity(const CSkeletonPose &skelAnim, int iLod)
{
	primitives::box bbox;
	phys_geometry *pgeom;
	QuatT qDelta;
	int i,j,iax;
	Vec3 pt;
	float res=0;

	FindSpineBones();
	if (m_nSpineBones)
	{
		if (!m_b3DOFStandup)
			qDelta.SetRotationXYZ(Ang3(0,0,
			(Ang3::GetAnglesXYZ(m_AbsolutePose[m_iSpineBone[0]].q)).z - 
			(Ang3::GetAnglesXYZ(skelAnim.m_AbsolutePose[m_iSpineBone[0]].q)).z));
		else
			qDelta.q = m_AbsolutePose[m_iSpineBone[0]].q * !skelAnim.m_AbsolutePose[m_iSpineBone[0]].q;

		qDelta.SetTranslation(m_AbsolutePose[m_iSpineBone[0]].t -	qDelta.q*skelAnim.m_AbsolutePose[m_iSpineBone[0]].t*m_fScale);

		for(i=m_iSpineBone[0]; i<(int)m_AbsolutePose.size(); i++) if (pgeom=GetModelJointPointer(i)->m_PhysInfo[iLod].pPhysGeom)
		{
			pgeom->pGeom->GetBBox(&bbox);
			iax = idxmax3((float*)&bbox.size);
			for(j=-1;j<=1;j+=2)
			{
				pt = bbox.center+bbox.Basis.GetRow(iax)*(bbox.size[iax]*j);
				res += (m_AbsolutePose[i]*pt-qDelta*(skelAnim.m_AbsolutePose[i]*pt*m_fScale)).len2() * pgeom->V;
			}
		}
	}

	return res;	
}

void CSkeletonPose::ProcessPhysics(const QuatT& rPhysEntityLocation, float fDeltaTimePhys, int nNeff, SCharUpdateFeedback *pCharFeedback)
{
	if( Console::GetInst().ca_UsePhysics==0 )
		return;

	//this is the relative orientation & translation of the animated character for this frame
	QuatT KinematicMovement = IDENTITY;

	//if we concatenate it with the physical entity location, then we have the new render location for this frame.
	//	QuatT NewRenderLocation = rPhysEntityLocation*KinematicMovement;


	Matrix34 mtx = Matrix34(rPhysEntityLocation);

	//m_uFlags |= nFlagsNeedReskinAllLODs;

	pe_params_joint pj;
	pe_status_joint sj;
	pe_status_pos sp;
	pe_params_flags pf;
	pj.bNoUpdate = 1;
	sp.flags = status_local;
	if (fDeltaTimePhys>0)
		pj.ranimationTimeStep = 1.0f/(pj.animationTimeStep = fDeltaTimePhys);
	int i,j,iParent,iRoot,bRopesAwake=0;
	m_bPhysicsAwake = 0;

	m_vOffset = Vec3(0,0,0); //offset;

	if (m_pCharPhysics)
		if (m_pCharPhysics->GetType()==PE_ARTICULATED)
		{

			pe_status_awake statusTmp;
			if (m_bPhysicsAwake = m_pCharPhysics->GetStatus(&statusTmp))
			{
				pf.flagsOR = pef_disabled;
				m_pCharPhysics->SetParams(&pf);
				Matrix33 mtxid;	mtxid.SetIdentity();
				int32 numJoints = m_AbsolutePose.size();
				// read angles deltas from physics and set current angles from animation as qext to physics
				for(i=0; i<numJoints; i++)
				{
					if (GetModelJointPointer(i)->getPhysInfo(0).pPhysGeom && (iParent=getBonePhysParentIndex(i)) >= 0)
					{
						//for (j = 0; j < 4 && !(m_pIKEffectors[j] && m_pIKEffectors[j]->AffectsBone(i)); ++j);
						//if (j<4)
						//	continue;

						sj.idChildBody = i;
						if (!m_pCharPhysics->GetStatus(&sj))
							continue;

						Vec3 trans34 = m_RelativePose[i].t;
						Matrix33 matparent; matparent.SetIdentity();

						for(int nCurParent=getBoneParentIndex(i); nCurParent != iParent; nCurParent=getBoneParentIndex(nCurParent)) 
							matparent = Matrix33(m_RelativePose[nCurParent].q)*matparent;
						const Matrix33& mtx0(GetModelJointPointer(i)->getPhysInfo(0).flags!=-1 && GetModelJointPointer(i)->getPhysInfo(0).framemtx[0][0]<10 ? 
							(Matrix33&)*(&GetModelJointPointer(i)->getPhysInfo(0).framemtx[0][0]) : mtxid);

						Matrix33 mtx33=mtx0.T()*matparent*Matrix33(m_RelativePose[i].q);
						if (nNeff)
							sj.qext = Ang3::GetAnglesXYZ(mtx33);

						pj.op[0] = iParent;
						pj.op[1] = i;
						if (!m_bPhysicsRelinquished) 
						{
							pj.qext = sj.qext;
							Matrix33 m33 = Matrix33::CreateRotationXYZ(sj.q+sj.qext);
							m_RelativePose[i].q = Quat( ((mtx0*m33).T()*matparent).T() );
							m_RelativePose[i].t = trans34;

							/*sp.ipart = -1;
							sp.partid = iParent;

							int status = m_pCharPhysics->GetStatus(&sp);
							if (status==0)
							{
								g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pInstance->m_pModel->GetModelFilePath(), "GetStatus() returned 0" );
								assert(0);
#if (EXTREME_TEST==1)
								CryError("CryAnimation: invalid status of GetStatus: %s",m_pInstance->GetModelFilePath());
#endif
							}

							pj.pivot = sp.q*(m_AbsolutePose[iParent].GetInverted() * m_AbsolutePose[i].t)+sp.pos;*/
							pj.pivot = m_AbsolutePose[iParent].GetInverted() * m_AbsolutePose[i].t;
							pj.flagsPivot = 8|1;
						}	else
							pj.qtarget = sj.qext;
						if (nNeff)
							m_pCharPhysics->SetParams(&pj);
					}
				}
				if (!m_bPhysicsRelinquished)
					UnconvertBoneGlobalFromRelativeForm(false);
			}

			if (m_bPhysicsRelinquished)	
			{
				if (nNeff)
					m_bPhysicsWasAwake = 1;
				SynchronizeWithPhysicalEntity(m_pCharPhysics, Vec3(0,0,0),Quat(1,0,0,0), QuatT(IDENTITY));
				m_fPhysBlendTime += fDeltaTimePhys;
			}	else
			{
				MARK_UNUSED pj.pivot;
				iRoot = getBonePhysChildIndex(0);
				if (m_bPhysicsAwake) 
				{	
					pj.q=Ang3(ZERO);
					pj.qext = Ang3::GetAnglesXYZ( Matrix33(m_AbsolutePose[iRoot].q) );
					pj.op[0] = -1;
					pj.op[1] = iRoot;
					m_pCharPhysics->SetParams(&pj);
				}	else 
					SynchronizeWithPhysicalEntity(m_pCharPhysics, Vec3(ZERO),Quat(1,0,0,0), KinematicMovement, 0);

				pe_params_articulated_body pab;
				pab.pivot.zero();
				pab.posHostPivot = KinematicMovement.t + m_AbsolutePose[iRoot].t;//*m_fScale;
				pab.qHostPivot = KinematicMovement.q;
				pab.bRecalcJoints = m_bPhysicsAwake;
				m_pCharPhysics->SetParams(&pab);
			}
		}	else
			SynchronizeWithPhysicalEntity(m_pCharPhysics, Vec3(ZERO),Quat(1,0,0,0),QuatT(IDENTITY), 0);

		pe_status_rope sr;
		pe_params_rope pr;
		pe_action_target_vtx atv;
		pe_action_notify an;
		Vec3 dir,v0,v1,ptAnim;
		Quat q0;
		float len,scale;
		int hasController,bUpdateChar;
		an.iCode = pe_action_notify::ParentChange;

		for(j=0;j<m_nAuxPhys;j++)
		{
			if (nNeff>0) for(i=0,pr.bTargetPoseActive=1;i<m_auxPhys[j].nBones;i++) 
			{
				hasController = 0;
				hasController |= m_arrControllerInfo[ m_auxPhys[j].pauxBoneInfo[i].iBone ].ops;

				if (!hasController)
					pr.bTargetPoseActive = 0;
			}	else
				pr.bTargetPoseActive = 0;
			if (m_pCharPhysics)	
			{
				if (!m_auxPhys[j].bPhysical)
					m_auxPhys[j].pPhysEnt->SetParams(&pr);
				if (!m_bPhysicsRelinquished)
					m_auxPhys[j].pPhysEnt->Action(&an);
			}

			sr.pPoints=0; sr.pVtx=0;
			m_auxPhys[j].pPhysEnt->GetStatus(&sr);
			if (sr.nVtx)
			{
				int iter,ivtx;
				float ka,kb,kc,kd,rnSegs=1.0f/m_auxPhys[j].nBones;
				if (sr.nVtx>m_auxPhys[j].nSubVtxAlloc) {
					if (m_auxPhys[j].pSubVtx) delete[] m_auxPhys[j].pSubVtx;
					m_auxPhys[j].pSubVtx = new Vec3[m_auxPhys[j].nSubVtxAlloc = (sr.nVtx-1&~15)+16];
				}
				sr.pVtx = m_auxPhys[j].pSubVtx;
				m_auxPhys[j].pPhysEnt->GetStatus(&sr);
				sr.pPoints = m_auxPhys[j].pVtx;
				for(i=0,len=0;i<sr.nVtx-1;i++) 
					len += (sr.pVtx[i+1]-sr.pVtx[i]).GetLength();
				iter=3; do {
					sr.pPoints[0] = sr.pVtx[0];
					for(i=ivtx=0; i<m_auxPhys[j].nBones && ivtx<sr.nVtx-1; )
					{
						dir = sr.pVtx[ivtx+1]-sr.pVtx[ivtx]; v0 = sr.pVtx[ivtx]-sr.pPoints[i];
						ka=dir.len2(); kb=v0*dir; kc=v0.len2()-sqr(len*rnSegs); kd=sqrt_tpl(max(0.0f,kb*kb-ka*kc));
						if (kd-kb<ka)
							sr.pPoints[++i] = sr.pVtx[ivtx]+dir*((kd-kb)/ka);
						else
							++ivtx;
					}
					len *= (1.0f-(m_auxPhys[j].nBones-i)*rnSegs);
					len += (sr.pVtx[sr.nVtx-1]-sr.pPoints[i]).GetLength();
				} while(--iter);
				sr.pPoints[m_auxPhys[j].nBones] = sr.pVtx[sr.nVtx-1];
				dir = sr.pPoints[m_auxPhys[j].nBones]-sr.pPoints[i]; 
				if (i+1<m_auxPhys[j].nBones) for(ivtx=i+1,rnSegs=1.0f/(m_auxPhys[j].nBones-i); ivtx<m_auxPhys[j].nBones; ivtx++)
					sr.pPoints[ivtx] = sr.pPoints[i]+dir*((ivtx-i)*rnSegs);
			}	else
			{
				sr.pPoints = m_auxPhys[j].pVtx;
				m_auxPhys[j].pPhysEnt->GetStatus(&sr);
			}
			bUpdateChar = m_pInstance->GetWasVisible() && !(sr.bTargetPoseActive==1 && sr.stiffnessAnim==0);

			for(i=0;i<m_auxPhys[j].nBones;i++) 
			{
				dir = (sr.pPoints[i+1]-sr.pPoints[i])*Matrix33(mtx);
				len = dir.GetLength();
				//SJoint &bone = m_arrJoints[ m_auxPhys[j].pauxBoneInfo[i].iBone ];
				QuatT& matBoneGlobal34 = m_AbsolutePose[m_auxPhys[j].pauxBoneInfo[i].iBone];
				ptAnim = mtx*(matBoneGlobal34.t);//+m_vOffset);
				if (bUpdateChar) 
				{
					v0 = m_auxPhys[j].pauxBoneInfo[i].dir0;
					v1 = len>1E-5f ? dir/len : v0;
					q0 = m_auxPhys[j].pauxBoneInfo[i].quat0;
					scale	=	len*m_auxPhys[j].pauxBoneInfo[i].rlen0;
					matBoneGlobal34.q = (Quat::CreateRotationV0V1(v0,v1)*q0).GetNormalized();
					matBoneGlobal34.t = (mtx.GetInvertedFast()*sr.pPoints[i]);//-m_vOffset);				
				}
				sr.pPoints[i] = ptAnim;
			}
			if (GetModelJointPointer(iParent = m_auxPhys[j].pauxBoneInfo[m_auxPhys[j].nBones-1].iBone)->numChildren()>0)
			{
				ptAnim = mtx*( m_AbsolutePose[ i=GetModelJointChildIndex(iParent,0) ].t );//+m_vOffset);
				if (bUpdateChar) 
				{
					m_AbsolutePose[i].t = mtx.GetInverted()*sr.pPoints[m_auxPhys[j].nBones];
					m_RelativePose[i].t = m_AbsolutePose[iParent].GetInverted()*m_AbsolutePose[i].t;
				}
				sr.pPoints[m_auxPhys[j].nBones] = ptAnim;
			}	

			if (m_pCharPhysics && !m_auxPhys[j].bPhysical)	
			{
				atv.nPoints = m_auxPhys[j].nBones+1;
				atv.points = sr.pPoints;
				m_auxPhys[j].pPhysEnt->Action(&atv);
			}
			pe_status_awake statusTmp;
			bRopesAwake |= m_auxPhys[j].pPhysEnt->GetStatus(&statusTmp);
		}

		if (bRopesAwake)
			UnconvertBoneGlobalFromRelativeForm(true,0,true);

		//if (m_bPhysicsAwake)
		//m_uFlags |= nFlagsNeedReskinAllLODs;
		m_bPhysicsWasAwake = m_bPhysicsAwake;

		if (m_bPhysicsRelinquished && m_bAliveRagdoll && m_iFnPSet<(int)m_pInstance->m_pModel->m_AnimationSet.m_arrStandupAnims.size())
		{ 

			//float fColor[4] = {0,1,0,1};
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.4f, fColor, false,"m_bPhysicsRelinquished" );	
			//g_YLine+=16.0f;

			// if char relinquished and stiff
			Quat drotRoot;
			pe_status_dynamics sd;
			m_pCharPhysics->GetStatus(&sd);
			m_timeRagdolled += fDeltaTimePhys;

			if (sd.nContacts==0 && m_timeLying<=0.0f && m_timeNoColl<=0.0f)
			{	
				//   if he's flying - check the 'body' normal (pelvis+spines), play new fall anim if sextant changes
				m_pSkeletonAnim->StartAnimation("falling_head_nw_up_01",0,0,0,CryCharAnimationParams(0,CA_LOOP_ANIMATION));
			}

			if (sd.nContacts>=1) 
			{	// if collisions present - pause teh anim; if not for some time - resume the anim
				m_pInstance->SetAnimationSpeed(0);
				m_timeNoColl = 0;
			}	
			else
			{ 
				if ((m_timeNoColl+=fDeltaTimePhys)>0.1f)
					m_pInstance->SetAnimationSpeed(1.0f);
			}

			//float Eprone=1.5f,tEprone=0.4f,tEproneCap=1.0f;
			if (sd.nContacts>=2)// && sd.energy<sd.mass*(Eprone+min(tEproneCap,m_timeLying*tEprone))) 
			{

			//	float fColor[4] = {1,0,0,1};
			//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.4f, fColor, false,"Initilize StandUp" );	
			//	g_YLine+=16.0f;

				if (m_timeLying<0.0f)
					m_timeLying=0.0f;

				// find the closest pose among the registered stand-up anims (ignoring the pivot z-rotation difference), play its first frame
				const char *lieAnimName = "prone_toCombat_nw_01";
				const char *curAnimName = 0;
				float curdiff;

				if (m_pInstance->m_pModel->m_pKinCharInstance==0) 
					m_pInstance->m_pModel->m_pKinCharInstance = new CCharInstance(m_pInstance->GetFilePath(), m_pInstance->m_pModel);

				CCharInstance* pKinCharInstance = m_pInstance->m_pModel->m_pKinCharInstance;
				pKinCharInstance->m_SkeletonPose.m_bFullSkeletonUpdate = 1;
				pKinCharInstance->GetISkeletonPose()->SetForceSkeletonUpdate(33);


				f32 mindiff=1E10f;
				int numStandupAnims = m_pInstance->m_pModel->m_AnimationSet.m_arrStandupAnims[m_iFnPSet].size();
				for(i=0; i<numStandupAnims; i++)
				{
					pKinCharInstance->m_SkeletonAnim.StopAnimationsAllLayers();
					curAnimName=m_pInstance->m_pModel->m_AnimationSet.GetNameByAnimID(m_pInstance->m_pModel->m_AnimationSet.m_arrStandupAnims[m_iFnPSet][i]);
					pKinCharInstance->m_SkeletonAnim.StartAnimation(	curAnimName,	0,0,0,CryCharAnimationParams(0,CA_LOOP_ANIMATION));
					pKinCharInstance->m_LastUpdateFrameID_Pre =	pKinCharInstance->m_LastUpdateFrameID_Post = g_pCharacterManager->m_nUpdateCounter-1;
					pKinCharInstance->m_SkeletonAnim.ProcessAnimations(IDENTITY,IDENTITY,0x22);
					pKinCharInstance->m_SkeletonPose.SkeletonPostProcess(IDENTITY,IDENTITY,0,0x22);

					curdiff = AssessPoseSimilarity(pKinCharInstance->m_SkeletonPose, min(1,(int)m_pInstance->m_pModel->m_arrModelMeshes.size()-1));
					if(curdiff<mindiff)
					{
						mindiff=curdiff; 
						lieAnimName=curAnimName;
					}
				}

				CryCharAnimationParams aparamns;
				aparamns.m_fPlaybackSpeed=0.8f;
		//		aparamns.m_nFlags|=CA_LOOP_ANIMATION;
				aparamns.m_nFlags|=CA_REPEAT_LAST_KEY;

				m_pSkeletonAnim->StopAnimationsAllLayers();
				m_pSkeletonAnim->StartAnimation(lieAnimName,0,0,0,aparamns);
				m_pInstance->SetAnimationSpeed(0);

				m_timeLying+=fDeltaTimePhys;
			}
		}

//------------------------------------------------------------

		if (m_timeStandingUp>=0)
		{
		//	float fColor[4] = {0,1,0,1};
		//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.4f, fColor, false,"m_timeStandingUp: %f",m_timeStandingUp );	
		//	g_YLine+=16.0f;

			uint32 numJoints=m_AbsolutePose.size();
			f32 t=min(1.0f,m_timeStandingUp);
			m_RelativePose[0].q.SetNlerp(m_arrPhysicsJoints[0].m_qRelFallPlay,m_RelativePose[0].q,t);
			m_AbsolutePose[0].q = m_RelativePose[0].q;

			for(uint32 j=1; j<numJoints; j++)
			{
				m_RelativePose[j].q.SetNlerp( m_arrPhysicsJoints[j].m_qRelFallPlay,m_RelativePose[j].q, t);
				m_AbsolutePose[j] = m_AbsolutePose[m_parrModelJoints[j].m_idxParent]*m_RelativePose[j];
			}

			SynchronizeWithPhysicalEntity(m_pCharPhysics, Vec3(ZERO),Quat(1,0,0,0), QuatT(IDENTITY), 0);

			if ((m_timeStandingUp+=fDeltaTimePhys)>2.0f)
			{
				pe_params_articulated_body pab;
				pe_player_dynamics pd; pd.bActive = 1;
				pe_params_flags pf; pf.flagsAND = ~pef_ignore_areas;
				if (!m_b3DOFStandup && m_pCharPhysics && m_pCharPhysics->GetParams(&pab) && pab.pHost)
					pab.pHost->SetParams(&pd),pab.pHost->SetParams(&pf);
				m_timeStandingUp = -1.0f;
			}
		}

	if (!is_unused(pf.flagsOR) && pf.flagsOR)
	{
		pf.flagsOR = 0;
		pf.flagsAND = ~pef_disabled;
		m_pCharPhysics->SetParams(&pf);
	}
}

void CSkeletonPose::Fall()
{
	m_bAliveRagdoll=true;
}

void CSkeletonPose::GoLimp()
{
	if (!m_bLimpRagdoll)
	{
		pe_params_joint pj;
		int i,iParent;
		for(i=0; i<(int)m_AbsolutePose.size(); i++)
			if (GetModelJointPointer(i)->getPhysInfo(0).pPhysGeom && (iParent=getBonePhysParentIndex(i)) >= 0)
			{
				pj.op[0] = iParent;
				pj.op[1] = i;
				pj.ks.zero();
				m_pCharPhysics->SetParams(&pj);
			}

			m_bLimpRagdoll=true;
	}
}

void CSkeletonPose::StandUp(const Matrix34 &_mtx, bool b3DOF, IPhysicalEntity *&pNewPhysicalEntity, Matrix34 &mtxDelta)
{
	if (!m_bPhysicsRelinquished || m_iFnPSet>=(int)m_pInstance->m_pModel->m_AnimationSet.m_arrStandupAnims.size())
		return;


//	float fColor[4] = {0,1,0,1};
//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.4f, fColor, false,"CSkeletonPose::StandUp" );	
//	g_YLine+=16.0f;

	if (m_arrPhysicsJoints.empty())
		InitPhysicsSkeleton();

	const char *lieAnimName = "prone_toCombat_nw_01",*curAnimName;
	float curdiff;
	FindSpineBones();
	m_bPhysicsWasAwake = 1;
	//SynchronizeWithPhysicalEntity(m_pCharPhysics, Vec3(0,0,0),Quat(1,0,0,0), QuatT(IDENTITY));
	m_b3DOFStandup = b3DOF;

	if (m_pInstance->m_pModel->m_pKinCharInstance==0) 
		m_pInstance->m_pModel->m_pKinCharInstance = new CCharInstance(m_pInstance->GetFilePath(), m_pInstance->m_pModel);

	CCharInstance* pKinCharInstance = m_pInstance->m_pModel->m_pKinCharInstance;
	pKinCharInstance->m_SkeletonPose.m_bFullSkeletonUpdate = 1;
	pKinCharInstance->GetISkeletonPose()->SetForceSkeletonUpdate(33);

	int numStandupAnims =(int)m_pInstance->m_pModel->m_AnimationSet.m_arrStandupAnims[m_iFnPSet].size();
	f32 mindiff=1E10f;
	int i=0;
	Matrix34 mtx = _mtx; mtx.Scale(Vec3(1.0f/_mtx.GetColumn(0).len()));

	for(i=0; i<numStandupAnims; i++)
	{
		pKinCharInstance->m_SkeletonAnim.StopAnimationsAllLayers();

		int strStandUpAnimID = m_pInstance->m_pModel->m_AnimationSet.m_arrStandupAnims[m_iFnPSet][i];
		curAnimName=m_pInstance->m_pModel->m_AnimationSet.GetNameByAnimID(strStandUpAnimID);
		pKinCharInstance->m_SkeletonAnim.StartAnimation( curAnimName,0,0,0, CryCharAnimationParams(0,CA_LOOP_ANIMATION));

		pKinCharInstance->m_LastUpdateFrameID_Pre =	pKinCharInstance->m_LastUpdateFrameID_Post = g_pCharacterManager->m_nUpdateCounter-1;
		pKinCharInstance->m_SkeletonAnim.ProcessAnimations(IDENTITY,IDENTITY,0xdead);
		pKinCharInstance->m_SkeletonPose.SkeletonPostProcess(IDENTITY,IDENTITY,0,0xdead);

		curdiff = AssessPoseSimilarity(pKinCharInstance->m_SkeletonPose, min(1,(int)m_pInstance->m_pModel->m_arrModelMeshes.size()-1));
		if(curdiff<mindiff)
		{
			mindiff=curdiff; 
			lieAnimName=curAnimName;
		}
	}

	CryCharAnimationParams aparamns;
	aparamns.m_fPlaybackSpeed=0.8f;
	aparamns.m_nFlags|=CA_LOOP_ANIMATION;
	aparamns.m_nFlags|=CA_REPEAT_LAST_KEY;

	pKinCharInstance->m_SkeletonAnim.StopAnimationsAllLayers();
	pKinCharInstance->m_SkeletonAnim.StartAnimation(lieAnimName,0,0,0,aparamns);
	pKinCharInstance->m_LastUpdateFrameID_Pre =	pKinCharInstance->m_LastUpdateFrameID_Post = g_pCharacterManager->m_nUpdateCounter-1;
	pKinCharInstance->m_SkeletonAnim.ProcessAnimations(IDENTITY,IDENTITY,0xbeef);
	pKinCharInstance->m_SkeletonPose.SkeletonPostProcess(IDENTITY,IDENTITY,0,0xbeef);

	if (m_pPrevCharHost)
		g_pIPhysicalWorld->DestroyPhysicalEntity(m_pPrevCharHost,2);
	//DestroyCharacterPhysics();m_bAliveRagdoll=0;
	//g_pIPhysicalWorld->GetPhysVars()->bSingleStepMode=1;
	Matrix34 mtxChar = Matrix34(Vec3(m_fScale),Quat(IDENTITY),m_vOffset);
	CreateCharacterPhysics(m_pPrevCharHost,m_fMass,m_iSurfaceIdx,m_stiffnessScale, 0, mtxChar);
	CreateAuxilaryPhysics(m_pCharPhysics, mtxChar);
	// rotate the entity around z by current (physical) z angle - animation z angle
	Quat drotRoot;
	if (!b3DOF)
		drotRoot.SetRotationXYZ(Ang3(0,0,
		(Ang3::GetAnglesXYZ(m_AbsolutePose[m_iSpineBone[0]].q)).z - 
		(Ang3::GetAnglesXYZ(m_pInstance->m_pModel->m_pKinCharInstance->m_SkeletonPose.m_AbsolutePose[m_iSpineBone[0]].q)).z));
	else
		drotRoot = m_AbsolutePose[m_iSpineBone[0]].q *	!m_pInstance->m_pModel->m_pKinCharInstance->m_SkeletonPose.m_AbsolutePose[m_iSpineBone[0]].q;

	mtxDelta = Matrix33(drotRoot);
	mtxDelta.SetTranslation(mtx*m_AbsolutePose[m_iSpineBone[0]].t - 
		mtxDelta*m_pInstance->m_pModel->m_pKinCharInstance->m_SkeletonPose.m_AbsolutePose[m_iSpineBone[0]].t*m_fScale);
	// find the closest orientation in each animated bone to the current pos 
	// start playing the stand-up anim (store the original q diff, 1/blend-away-time per bone)
	aparamns.m_nFlags&=~CA_LOOP_ANIMATION;
	aparamns.m_nFlags|=CA_FORCE_SKELETON_UPDATE;
	m_pSkeletonAnim->StopAnimationsAllLayers();
	m_pSkeletonAnim->StartAnimation(lieAnimName,0,0,0,aparamns);
	m_pInstance->SetAnimationSpeed(1.0f);

	CAnimationSet *pAnimSet = (CAnimationSet*)m_pInstance->GetIAnimationSet();
	QuatT mtxDeltaInv=QuatT(mtxDelta).GetInverted(), mtxQ=QuatT(mtx);

	uint32 numJoints = m_AbsolutePose.size();
	m_arrPhysicsJoints[0].m_qRelFallPlay = m_AbsolutePose[0].q;
	for(uint32 j=1; j<numJoints; j++)
		m_arrPhysicsJoints[j].m_qRelFallPlay = !m_AbsolutePose[m_parrModelJoints[j].m_idxParent].q * m_AbsolutePose[j].q;

	int32 sidx=m_iSpineBone[0];
	m_arrPhysicsJoints[sidx].m_qRelFallPlay = !drotRoot * m_AbsolutePose[sidx].q;


	for(i=0;i<(int)m_AbsolutePose.size();i++)
		m_AbsolutePose[i] = mtxDeltaInv*(mtxQ*m_AbsolutePose[i]);

	if (m_pCharPhysics) 
	{
		pe_params_articulated_body pab;
		pab.bAwake = 0;
		pab.pivot.zero();
		pab.posHostPivot = m_vOffset+Vec3( m_pInstance->m_pModel->m_pKinCharInstance->m_SkeletonPose.m_AbsolutePose[m_iSpineBone[0]].t );
		pab.qHostPivot.SetIdentity();
		pab.bRecalcJoints = 0;
		m_pCharPhysics->SetParams(&pab);
	}

	if (!b3DOF)
	{
		pe_player_dynamics pd; //pd.bActive = 0;
		pe_params_flags pf;	pf.flagsOR = pef_ignore_areas;
		pd.gravity.Set(0,0,-1.8f);
		if (m_pPrevCharHost)
			m_pPrevCharHost->SetParams(&pd),m_pPrevCharHost->SetParams(&pf);
	}
	m_timeStandingUp = 0;
	mtxDelta.Scale(Vec3(m_fScale));

	pNewPhysicalEntity=m_pPrevCharHost;
}

bool CSkeletonPose::SetFnPAnimGroup(const char *name) 
{ 
	int i;
	for(i=m_pInstance->m_pModel->m_AnimationSet.m_arrStandupAnimTypes.size()-1; 
			i>=0 && strcmp(m_pInstance->m_pModel->m_AnimationSet.m_arrStandupAnimTypes[i],name); i--);
	if (i<0)
		return false; 
	m_iFnPSet=i; return true;
}
bool CSkeletonPose::SetFnPAnimGroup(int idx) 
{ 
	if ((unsigned int)idx>=m_pInstance->m_pModel->m_AnimationSet.m_arrStandupAnimTypes.size())
		return false;
	m_iFnPSet=idx; return true; 
}

float CSkeletonPose::Falling() const
{
	return m_timeRagdolled;
}

float CSkeletonPose::Lying() const
{
	return m_timeLying;
}

float CSkeletonPose::StandingUp() const
{
	return m_timeStandingUp;
}

int CSkeletonPose::GetFallingDir()
{
	if (!m_pCharPhysics)
		return -1;

	pe_status_dynamics sd;
	int status = m_pCharPhysics->GetStatus(&sd);

	FindSpineBones();

	Vec3 n(ZERO);
	int i;
	for(i=0;i<m_nSpineBones;i++)
		n += sd.v*m_AbsolutePose[m_iSpineBone[i]].q;
	Vec3 nAbs = n.abs();
	i = idxmax3((float *)&nAbs);
	i = i*2+isneg(n[i]);

	return i;
}

void CSkeletonPose::FindSpineBones()
{
	int i;
	if (!m_nSpineBones)	
	{
		for(i=m_nSpineBones=0; i<(int)m_AbsolutePose.size() && m_nSpineBones<3; i++)
			if (strstr(GetModelJointPointer(i)->GetJointName(),"Pelvis") || strstr(GetModelJointPointer(i)->GetJointName(),"Spine"))
				m_iSpineBone[m_nSpineBones++] = i;
		if (!m_nSpineBones)
			m_nSpineBones = 1+((m_iSpineBone[0]=getBonePhysChildIndex(0))>>31);
	}
}

void CSkeletonPose::BuildPhysicalEntity(IPhysicalEntity *pent,float mass,int surface_idx,float stiffness_scale, 
																		int nLod,int partid0, const Matrix34 &mtxloc)
{
	float scale=mtxloc.GetColumn(0).GetLength();
	Vec3 offset=mtxloc.GetTranslation(); 
	if (fabs_tpl(scale-1.0f)<0.01f)
		scale = 1.0f;

	if (m_pInstance->m_pModel->m_ObjectType==CGA)
	{
		assert( pent );

		m_pCharPhysics = pent;

		uint32 i;
		float totalVolume = 0;

		uint32 numJoints = m_arrCGAJoints.size();
		assert(numJoints);
		for (i=0; i <numJoints; i++)
		{
			CCGAJoint *joint = &m_arrCGAJoints[i];
			if (!joint->m_CGAObjectInstance)
				continue;
			phys_geometry *geom = joint->m_CGAObjectInstance->GetPhysGeom(0);
			if (geom)
			{
				totalVolume += geom->V;
			}
		}
		float density = totalVolume>0 ? mass/totalVolume:0.0f;

		pe_articgeomparams params;
		//	uint32 numJoints = m_arrJoints.size();
		//	assert(numJoints);
		for (i = 0; i <numJoints ; i++)
		{
			CCGAJoint *joint = &m_arrCGAJoints[i];
			//SJoint *sjoint = &m_arrJoints[i];
			m_parrModelJoints[i].m_NodeID = ~0;
			if (!joint->m_CGAObjectInstance)
				continue;

			IStatObj::SSubObject *pSubObj;
			const char *pstrMass;
			if (m_pInstance->m_pModel->pCGA_Object && (pSubObj=m_pInstance->m_pModel->pCGA_Object->GetSubObject(i)) && 
				(pstrMass=strstr(pSubObj->properties,"mass")))
			{
				params.density = 0;
				for(; *pstrMass && !isdigit(*pstrMass); pstrMass++);
				params.mass = (float)atof(pstrMass);
			}	else
			{
				params.mass = 0;
				params.density = density;
			}

			// Add collision geometry.
			params.flags = geom_collides|geom_floats;
			phys_geometry *geom = joint->m_CGAObjectInstance->GetPhysGeom(PHYS_GEOM_TYPE_NO_COLLIDE),
				*geomProxy = joint->m_CGAObjectInstance->GetPhysGeom(0);
			if (geom && !geomProxy)
				params.flags = geom_colltype_ray;
			if (!geom)
				geom=geomProxy, geomProxy=0;
			params.pos = m_AbsolutePose[i].t*scale;
			params.q = m_AbsolutePose[i].q;
			params.scale = scale;
			params.idbody = i;
			if (geom)
			{
				m_parrModelJoints[i].m_NodeID = m_pCharPhysics->AddGeometry( geom, &params, partid0+i );
				//joint->m_Physics = true;
				joint->m_qqqhasPhysics = partid0+i;
			}
			if (geomProxy)
			{
				params.flags |= geom_proxy;
				m_pCharPhysics->AddGeometry( geomProxy, &params, partid0+i );
			}


			// Add obstruct geometry.
			/*params.flags = geom_proxy;
			geom = joint->m_CGAObject->GetPhysGeom(1);
			if (geom)
			{
			joint->m_id = m_pCharPhysics->AddGeometry( geom, &params, partid0+i );
			joint->m_Physics = true;
			}*/
		}

		//m_pInstance->m_pModel->pCGA_Object->PhysicalizeSubobjects(m_pCharPhysics=pent,0, mass,0, partid0);

		pe_params_joint pj;
		pj.op[0] = -1;
		pj.flagsPivot = 7;
		if (pent->GetType()==PE_ARTICULATED) for(i=0; i<numJoints; i++) 
			if (m_arrCGAJoints[i].m_CGAObjectInstance && m_arrCGAJoints[i].m_CGAObjectInstance->GetPhysGeom(0))
			{
				pj.op[1] = i;
				pj.pivot = m_AbsolutePose[i].t;
				pent->SetParams(&pj);
			}

	}
	else
	{

		pe_type pentype = pent->GetType();
		int i,j;
		pe_geomparams gp;
		pe_articgeomparams agp;
		pe_geomparams *pgp = pentype==PE_ARTICULATED ? &agp:&gp;
		pgp->flags = pentype==PE_LIVING ? 0:geom_collides|geom_floats;
		pgp->bRecalcBBox = 0;
		float volume;


		for (i=0, volume=0; i<(int)m_AbsolutePose.size(); ++i)
			if (GetModelJointPointer(i)->m_PhysInfo[nLod].pPhysGeom)
				volume += GetModelJointPointer(i)->m_PhysInfo[nLod].pPhysGeom->V*cube(scale);

		pgp->density=0.0f;
		if (volume)
			pgp->density = mass/volume;

		pgp->scale = scale;

		if (surface_idx>=0)
			pgp->surface_idx = surface_idx;

		pe_action_remove_all_parts tmp;
		pent->Action(&tmp);

		for(i=0;i<(int)m_AbsolutePose.size();i++) 
			if (GetModelJointPointer(i)->m_PhysInfo[nLod].pPhysGeom) 
			{
				pgp->pos = m_AbsolutePose[i].t+offset;
				pgp->q = m_AbsolutePose[i].q;
				pgp->flags = /*strstr(GetModelJointIdx(i)->m_strJointName,"Hand") ? geom_no_raytrace :*/ geom_collides|geom_floats;
				agp.idbody = i;
				m_parrModelJoints[i].m_NodeID = pent->AddGeometry(GetModelJointPointer(i)->m_PhysInfo[nLod].pPhysGeom,pgp,i);
				GetModelJointPointer(i)->m_fMass = pgp->density * GetModelJointPointer(i)->m_PhysInfo[nLod].pPhysGeom->V;
			}

			if (pentype==PE_ARTICULATED) 
			{
				//SJoint *pBone;
				CModelJoint* pBoneInfo;
				Matrix33 mtx0;
				for(i=0;i<(int)m_AbsolutePose.size();i++) 
					if (GetModelJointPointer(i)->m_PhysInfo[nLod].pPhysGeom)	
					{
						pe_params_joint pj;
						int iParts[8]; const char *ptr,*ptr1;
						//pBone = &m_arrJoints[i];
						pBoneInfo = GetModelJointPointer(i);

						pj.pSelfCollidingParts = iParts;
						if ((pj.flags = pBoneInfo->m_PhysInfo[nLod].flags)!=-1)
							pj.flags |= angle0_auto_kd*7;
						else pj.flags = angle0_locked;
						if (nLod)
							pj.flags &= ~(angle0_auto_kd*7);
						pj.op[0] = getBonePhysParentIndex(i,nLod);
						pj.op[1] = i;

						pj.pivot = m_AbsolutePose[i].t;

						pj.pivot = (pj.pivot+offset);//*scale;

						pj.nSelfCollidingParts = 0;
						if ((ptr=strstr(pBoneInfo->GetJointName(),"Forearm"))) 
						{
							for(j=0;j<(int)m_AbsolutePose.size();j++) 
								if (GetModelJointPointer(j)->m_PhysInfo[nLod].pPhysGeom && 
									(strstr(GetModelJointPointer(j)->GetJointName(),"Pelvis") || strstr(GetModelJointPointer(j)->GetJointName(),"Head") || 
									strstr(GetModelJointPointer(j)->GetJointName(),"Spine") ||	
									(ptr1=strstr(GetModelJointPointer(j)->GetJointName(),"Thigh")) ||//&& ptr[-2]==ptr1[-2] || 
									strstr(GetModelJointPointer(j)->GetJointName(),"Forearm") && i>j))
									pj.pSelfCollidingParts[pj.nSelfCollidingParts++] = j;
						}	
						else 
							if ((ptr=strstr(pBoneInfo->GetJointName(),"Calf"))) 
							{
								for(j=0;j<(int)m_AbsolutePose.size();j++) if (GetModelJointPointer(j)->m_PhysInfo[nLod].pPhysGeom && 
									strstr(GetModelJointPointer(j)->GetJointName(),"Calf") && i>j)
									pj.pSelfCollidingParts[pj.nSelfCollidingParts++] = j;
							}

							if (pBoneInfo->m_PhysInfo[nLod].flags!=-1) 
								for(j=0;j<3;j++) 
								{
									pj.limits[0][j] = pBoneInfo->m_PhysInfo[nLod].min[j];
									pj.limits[1][j] = pBoneInfo->m_PhysInfo[nLod].max[j];
									pj.bounciness[j] = 0;
									pj.ks[j] = pBoneInfo->m_PhysInfo[nLod].spring_tension[j]*stiffness_scale;
									pj.kd[j] = pBoneInfo->m_PhysInfo[nLod].damping[j];
									if (fabsf(pj.limits[0][j])<3) 
									{
										pj.qdashpot[j] = 0.3f; pj.kdashpot[j] = 40.0f-nLod*36.0f;
									} 
									else pj.qdashpot[j] = pj.kdashpot[j] = 0;
								} 
							else 
								for(j=0;j<3;j++) 
								{
									pj.limits[0][j]=-1E10f; pj.limits[1][j]=1E10f;
									pj.bounciness[j]=0; pj.ks[j]=0; pj.kd[j]=stiffness_scale;
								}
								if (strstr(GetModelJointPointer(i)->GetJointName(),"Thigh") || strstr(GetModelJointPointer(i)->GetJointName(),"Calf"))
									pj.flags |= joint_ignore_impulses;

								pj.pMtx0 = 0;
								if (pBoneInfo->m_PhysInfo[nLod].framemtx[0][0]<10 && pBoneInfo->m_PhysInfo[nLod].flags!=-1)
									pj.pMtx0 = (Matrix33*)pBoneInfo->m_PhysInfo[nLod].framemtx[0];
								pent->SetParams(&pj);
					}
			}

			//m_vOffset = offset;
			//m_fScale = scale;
			m_fMass = mass;
			m_iSurfaceIdx = surface_idx;
			m_bPhysicsRelinquished = nLod;
	}

	for(int i=m_pInstance->m_AttachmentManager.GetAttachmentCount()-1; i>=0; i--) 
		m_pInstance->m_AttachmentManager.PhysicalizeAttachment(i,nLod,pent,offset);
}








IPhysicalEntity *CSkeletonPose::GetCharacterPhysics(const char *pRootBoneName) 
{	
	if (!pRootBoneName)
		return m_pCharPhysics; 

	for(int i=0;i<m_nAuxPhys;i++) 
	{
		if (!strnicmp(m_auxPhys[i].strName,pRootBoneName,m_auxPhys[i].nChars))
			return m_auxPhys[i].pPhysEnt;
	}

	return m_pCharPhysics;
}
IPhysicalEntity *CSkeletonPose::GetCharacterPhysics(int iAuxPhys) 
{ 
	if (iAuxPhys<0)
		return m_pCharPhysics;
	if (iAuxPhys>=m_nAuxPhys)
		return 0;

	return m_auxPhys[iAuxPhys].pPhysEnt;
}

void CSkeletonPose::DestroyCharacterPhysics(int iMode) 
{ 
	int i;
	for(i=0;i<m_nAuxPhys;i++) {
		g_pIPhysicalWorld->DestroyPhysicalEntity(m_auxPhys[i].pPhysEnt,iMode);
		if (iMode==0)
			delete[] m_auxPhys[i].pauxBoneInfo;
	}
	if (iMode==0)
		m_nAuxPhys = 0;

	if (m_pCharPhysics)
		g_pIPhysicalWorld->DestroyPhysicalEntity(m_pCharPhysics,iMode);
	if (iMode==0) {
		m_pCharPhysics = 0;
		m_bPhysicsRelinquished=m_bAliveRagdoll = 0; m_timeStandingUp = -1;
		if (m_ppBonePhysics)
		{
			for(i=0;i<(int)m_AbsolutePose.size();i++) if (m_ppBonePhysics[i])
				m_ppBonePhysics[i]->Release(), g_pIPhysicalWorld->DestroyPhysicalEntity(m_ppBonePhysics[i]);
			delete[] m_ppBonePhysics;
			m_ppBonePhysics = 0;
		}
	}

	if (iMode==2) for(i=0;i<m_nAuxPhys;i++) {
		pe_params_rope pr;
		pr.bTargetPoseActive = 0;
		if (m_auxPhys[i].bTied0)
			pr.pEntTiedTo[0] = m_pCharPhysics;
		if (m_auxPhys[i].bTied1)
			pr.pEntTiedTo[1] = m_pCharPhysics;
		m_auxPhys[i].pPhysEnt->SetParams(&pr);
	}
}





IPhysicalEntity *CSkeletonPose::CreateCharacterPhysics(IPhysicalEntity *pHost, float mass,int surface_idx,float stiffness_scale, int nLod, 
																									 const Matrix34 &mtxloc)
{
	float scale = mtxloc.GetColumn(0).GetLength();
	Vec3 offset = mtxloc.GetTranslation();
	if (fabs_tpl(scale-1.0f)<0.01f)
		scale = 1.0f;

	if (m_pCharPhysics) 
	{
		int i;
		g_pIPhysicalWorld->DestroyPhysicalEntity(m_pCharPhysics);
		m_pCharPhysics = 0;
		m_bPhysicsRelinquished=m_bAliveRagdoll = 0;	m_timeStandingUp = -1;
		for(i=0;i<m_nAuxPhys;i++)
		{
			g_pIPhysicalWorld->DestroyPhysicalEntity(m_auxPhys[i].pPhysEnt);
			delete[] m_auxPhys[i].pauxBoneInfo;
			delete[] m_auxPhys[i].pVtx;
		}
		m_nAuxPhys=0;
		if (m_ppBonePhysics)
		{
			for(i=0;i<(int)m_AbsolutePose.size();i++) if (m_ppBonePhysics[i])
				m_ppBonePhysics[i]->Release(), g_pIPhysicalWorld->DestroyPhysicalEntity(m_ppBonePhysics[i]);
			delete[] m_ppBonePhysics;
			m_ppBonePhysics = 0;
		}
	}

	if (m_bHasPhysics) 
	{
		pe_params_pos pp;
		pp.iSimClass = 6;
		m_pCharPhysics = g_pIPhysicalWorld->CreatePhysicalEntity(PE_ARTICULATED,5.0f,&pp,0);

		pe_params_articulated_body pab;
		pab.bGrounded = 1;
		pab.scaleBounceResponse = 0.6f;
		pab.bAwake = 0;

		//IVO_x
		pab.pivot = Vec3( m_AbsolutePose[ getBonePhysChildIndex(0) ].t );

		m_pCharPhysics->SetParams(&pab);

		BuildPhysicalEntity(m_pCharPhysics, mass,surface_idx,stiffness_scale, 0,0, mtxloc);

		if (pHost) 
		{
			pe_params_foreign_data pfd;
			int status = pHost->GetParams(&pfd);
			if (status==0)
			{
				g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pInstance->m_pModel->GetModelFilePath(), "GetParams() returned 0" );
				assert(0);
#if (EXTREME_TEST==1)
				CryError("CryAnimation: invalid status of GetParams: %s",m_pInstance->GetFilePath());
#endif
			}

			MARK_UNUSED pfd.iForeignFlags;
			m_pCharPhysics->SetParams(&pfd);

			pe_params_articulated_body pab1;
			pab1.pivot.zero();
			pab1.pHost = pHost;
			pab1.posHostPivot = Vec3( m_AbsolutePose[getBonePhysChildIndex(0)].t)+offset;
			pab1.qHostPivot.SetIdentity();
			pab1.bAwake = 0;

			m_pCharPhysics->SetParams(&pab1);
		}

		pe_params_joint pj;
		pj.op[0] = -1;
		pj.op[1] = getBonePhysChildIndex(0);
		pj.flags = all_angles_locked | joint_no_gravity | joint_isolated_accelerations;
		m_pCharPhysics->SetParams(&pj);

		pp.iSimClass = 4;
		m_pCharPhysics->SetParams(&pp);
	}

	m_vOffset = offset;
	m_fScale = scale;
	m_fMass = mass;
	m_iSurfaceIdx = surface_idx;
	m_stiffnessScale = stiffness_scale;
	m_pPrevCharHost = pHost;
	m_bPhysicsRelinquished = 0;
	m_timeStandingUp = -1.0f;

	/*IAttachmentObject* pAttachment;
	for(int i=m_AttachmentManager.GetAttachmentCount()-1; i>=0; i--)
	if ((pAttachment = m_AttachmentManager.GetInterfaceByIndex(i)->GetIAttachmentObject()) && pAttachment->GetICharacterInstance())
	pAttachment->GetICharacterInstance()->CreateCharacterPhysics(m_pCharPhysics, mass,surface_idx, stiffness_scale, nLod);*/

	return m_pCharPhysics;
}



int CSkeletonPose::CreateAuxilaryPhysics(IPhysicalEntity *pHost, const Matrix34 &mtx, int nLod)
{
	return CreateAuxilaryPhysics(pHost,mtx,mtx.GetColumn(0).len(),m_vOffset,nLod);
}


int CSkeletonPose::CreateAuxilaryPhysics(IPhysicalEntity *pHost, const Matrix34 &mtx, float scale,Vec3 offset, int nLod) 
//float *pForcedRopeLen,int *piForcedRopeIdx,int nForcedRopes)
{
	int i,j,k,nchars;
	const char *pspace;

	// Delete aux physics

	if (m_nAuxPhys)
	{
		for(i=0;i<m_nAuxPhys;i++)
		{
			pe_params_rope pr;
			if (m_auxPhys[i].bTied0)
				pr.pEntTiedTo[0] = pHost;
			if (m_auxPhys[i].bTied1)
				pr.pEntTiedTo[1] = pHost;
			pr.bTargetPoseActive = 0;
			m_auxPhys[i].pPhysEnt->SetParams(&pr);
			//g_pIPhysicalWorld->DestroyPhysicalEntity(m_auxPhys[i].pPhysEnt);
			//delete[] m_auxPhys[i].pauxBoneInfo;
			//delete[] m_auxPhys[i].pVtx;
		}
		return m_nAuxPhys;
	}

	m_nAuxPhys=0;

	for (i = 0; i<(int)m_AbsolutePose.size(); i++)
	{
		CModelJoint* pBoneInfo = GetModelJointPointer(i);
		const char* szBoneName = pBoneInfo->GetJointName();

		if (!strnicmp(szBoneName,"rope",4) && pBoneInfo->numChildren()>0)
		{
			if (pspace = strchr(szBoneName,' '))
				nchars = (int)(pspace-szBoneName);
			else 
				nchars = strlen(szBoneName);
			for(j = 0; j < m_nAuxPhys && strncmp(m_auxPhys[j].strName,szBoneName,nchars); ++j);
			if (j == m_nAuxPhys)
			{
				if (m_nAuxPhys==SIZEOF_ARRAY(m_auxPhys))
					break;
				m_auxPhys[m_nAuxPhys].pPhysEnt = g_pIPhysicalWorld->CreatePhysicalEntity(PE_ROPE);
				m_auxPhys[m_nAuxPhys].strName = szBoneName;
				m_auxPhys[m_nAuxPhys++].nChars = nchars;
			}
			pBoneInfo->m_nLimbId = 100+j;
		} else if (!pBoneInfo->getPhysInfo(0).pPhysGeom)
			pBoneInfo->getPhysInfo(0).flags = -1;
	}

	pe_status_pos sp;
	sp.pos = mtx.GetTranslation(); 
	sp.q = Quat(Matrix33(mtx)/mtx.GetColumn(0).GetLength());

	pe_params_flags pf;
	pf.flagsOR = pef_disabled;
	pe_action_target_vtx atv;
	pe_simulation_params simp;
	simp.gravity.zero();

	for (j = 0; j < m_nAuxPhys; ++j)
	{
		pe_params_rope pr;
		int iLastBone=0,iFirstBone=0;
		pr.nSegments = 0;
		for(i=0;i<(int)m_AbsolutePose.size();i++) 
			if (!strncmp(m_auxPhys[j].strName,GetModelJointPointer(i)->GetJointName(),m_auxPhys[j].nChars) && GetModelJointPointer(i)->numChildren()>0)
				pr.nSegments++;
		pr.pPoints = new Vec3[pr.nSegments+1];
		m_auxPhys[j].pauxBoneInfo = new aux_bone_info[m_auxPhys[j].nBones = pr.nSegments];
		m_auxPhys[j].pVtx = pr.pPoints.data;
		pr.length = 0;
		m_auxPhys[j].bTied0 = m_auxPhys[j].bTied1 = false;
		m_auxPhys[j].iBoneTiedTo[0] = m_auxPhys[j].iBoneTiedTo[1] = -1;
		m_auxPhys[j].pSubVtx=0; m_auxPhys[j].nSubVtxAlloc=0;

		for(i=k=0;i<(int)m_AbsolutePose.size();i++) 
			if (!strncmp(m_auxPhys[j].strName,GetModelJointPointer(i)->GetJointName(),m_auxPhys[j].nChars) && GetModelJointPointer(i)->numChildren()>0) 
			{
				if (k==0 && getBonePhysParentIndex(iFirstBone=i,nLod)>=0)
					pr.idPartTiedTo[0] = getBonePhysParentIndex(m_auxPhys[j].iBoneTiedTo[0]=i,nLod);

				pr.pPoints[k] = m_AbsolutePose[i].t*scale + offset;
				pr.pPoints[k+1] = m_AbsolutePose[GetModelJointChildIndex(i,0)].t * scale + offset;
				((GetModelJointPointer(i)->getPhysInfo(0).flags) &= 0xFFFF) |= 0x30000;

				m_auxPhys[j].pauxBoneInfo[k].iBone = i;
				m_auxPhys[j].pauxBoneInfo[k].dir0 = pr.pPoints[k+1]-pr.pPoints[k];
				if (m_auxPhys[j].pauxBoneInfo[k].dir0.len2()>0)
					m_auxPhys[j].pauxBoneInfo[k].dir0 *= 
					(m_auxPhys[j].pauxBoneInfo[k].rlen0 = 1.0f/m_auxPhys[j].pauxBoneInfo[k].dir0.GetLength());
				else {
					m_auxPhys[j].pauxBoneInfo[k].dir0(0,0,1);
					m_auxPhys[j].pauxBoneInfo[k].rlen0 = 1.0f;
				}

				m_auxPhys[j].pauxBoneInfo[k].quat0 = m_AbsolutePose[i].q;

				//Matrix34 q34 = Matrix34(m_arrJoints[i].m_AbsolutePose);
				//m_auxPhys[j].pauxBoneInfo[k].quat0 = Quat(q34);

				pr.pPoints[k] = sp.q*pr.pPoints[k]+sp.pos;
				pr.pPoints[k+1] = sp.q*pr.pPoints[k+1]+sp.pos;
				pr.length += (pr.pPoints[k+1]-pr.pPoints[k]).GetLength();
				iLastBone = GetModelJointChildIndex(i,0); k++;
			}

			if (!is_unused(pr.idPartTiedTo[0])) 
			{
				pr.pEntTiedTo[0] = pHost;
				pr.ptTiedTo[0] = pr.pPoints[0];
				m_auxPhys[j].bTied0 = true;
			}

			if ((i = getBonePhysChildIndex(iLastBone))>=0) 
			{
				pr.pEntTiedTo[1] = pHost;
				m_auxPhys[j].iBoneTiedTo[1] = iLastBone;
				pr.idPartTiedTo[1] = i;
				pr.ptTiedTo[1] = pr.pPoints[pr.nSegments];
				m_auxPhys[j].bTied1 = true;
			}
			//if (pForcedRopeLen && *piForcedRopeIdx+i<nForcedRopes)
			//	pr.length = pForcedRopeLen[*piForcedRopeIdx+j];

			CModelJoint* pBoneInfo = GetModelJointPointer(iFirstBone);
			if (!(pBoneInfo->m_PhysInfo[nLod].flags & joint_isolated_accelerations))
			{
				pr.jointLimit = DEG2RAD(pBoneInfo->m_PhysInfo[nLod].spring_tension[0]);
				pr.stiffnessAnim = pBoneInfo->m_PhysInfo[nLod].max[0];
				pr.stiffnessDecayAnim = pBoneInfo->m_PhysInfo[nLod].max[1];
				pr.dampingAnim = pBoneInfo->m_PhysInfo[nLod].max[2];
				pr.bTargetPoseActive = 2*isneg(-pr.stiffnessAnim);
				pf.flagsOR = rope_target_vtx_rel0;
				if (!(pBoneInfo->m_PhysInfo[nLod].flags & 1))
					pf.flagsOR |= rope_collides;
				m_auxPhys[j].bPhysical = true;
				if (pBoneInfo->m_PhysInfo[nLod].flags & joint_no_gravity)
					pf.flagsOR |= pef_ignore_areas;
			}	else
			{
				MARK_UNUSED pr.bTargetPoseActive,pr.jointLimit,pr.stiffnessAnim,pr.dampingAnim,pr.stiffnessDecayAnim;
				m_auxPhys[j].bPhysical = false;
			}

			m_auxPhys[j].pPhysEnt->SetParams(&pr);
			if (!is_unused(pr.jointLimit))
			{
				QuatT qBase = m_AbsolutePose[pr.idPartTiedTo[is_unused(pr.idPartTiedTo[0])]].GetInverted();
				atv.points = m_auxPhys[j].pVtx;
				atv.nPoints = m_auxPhys[j].nBones+1;
				for(i=0;i<m_auxPhys[j].nBones;i++) 
				{
					k = m_auxPhys[j].pauxBoneInfo[i].iBone;
					atv.points[i] = qBase*m_AbsolutePose[k].t;
					atv.points[i+1] = qBase*m_AbsolutePose[GetModelJointChildIndex(k,0)].t;
				}
				m_auxPhys[j].pPhysEnt->Action(&atv);
			}
			if (pf.flagsOR & pef_ignore_areas)
				m_auxPhys[j].pPhysEnt->SetParams(&simp);
			if (m_nAuxPhys>1 || m_auxPhys[j].bPhysical)
			{
				m_auxPhys[j].pPhysEnt->SetParams(&pf);
				pe_action_awake aa; aa.bAwake=0;
				m_auxPhys[j].pPhysEnt->Action(&aa);
			}
	}

	m_vOffset = offset;
	m_fScale = scale;

	//if (pForcedRopeLen)
	//	*piForcedRopeIdx += j;
	IAttachmentObject* pAttachment;
	ICharacterInstance *pChar;
	IPhysicalEntity *pRope;
	for(int i=m_pInstance->m_AttachmentManager.GetAttachmentCount()-1; i>=0; i--)
		if (m_pInstance->m_AttachmentManager.GetInterfaceByIndex(i)->GetType()==CA_BONE && 
			(pAttachment = m_pInstance->m_AttachmentManager.GetInterfaceByIndex(i)->GetIAttachmentObject()) && (pChar=pAttachment->GetICharacterInstance()))
		{
			((CCharInstance*)pChar)->m_SkeletonPose.CreateAuxilaryPhysics(pHost, mtx, scale,offset, nLod);//, pForcedRopeLen,piForcedRopeIdx,nForcedRopes);
			for(int j=0; pRope=pChar->GetISkeletonPose()->GetCharacterPhysics(j); j++)
			{
				pe_params_rope pr;
				pr.pEntTiedTo[0] = pHost;
				pr.idPartTiedTo[0] = getBonePhysParentOrSelfIndex(m_pInstance->m_AttachmentManager.GetInterfaceByIndex(i)->GetBoneID());
				pRope->SetParams(&pr);
			}
		}

		return m_nAuxPhys;
}


int CSkeletonPose::FillRopeLenArray(float *len,int i0,int sz)
{
	int i,i0org=i0;
	pe_params_rope pr;
	for(i=0;i<m_nAuxPhys && i+i0<sz;i++)
	{
		int32 status = m_auxPhys[i].pPhysEnt->GetParams(&pr);
		if (status==0)
		{
			g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pInstance->m_pModel->GetModelFilePath(), "GetParams() returned 0" );
			assert(0);
#if (EXTREME_TEST==1)
			CryError("CryAnimation: invalid status of GetParams: %s",m_pInstance->GetFilePath());
#endif
		}
		len[i0+i] = pr.length;
	}
	i0+=i;

	IAttachmentObject* pAttachment;
	ICharacterInstance *pChar;
	for(i=m_pInstance->m_AttachmentManager.GetAttachmentCount()-1; i>=0; i--)
		if (m_pInstance->m_AttachmentManager.GetInterfaceByIndex(i)->GetType()==CA_BONE && 
			(pAttachment = m_pInstance->m_AttachmentManager.GetInterfaceByIndex(i)->GetIAttachmentObject()) && (pChar=pAttachment->GetICharacterInstance()))
			i0 += ((CCharInstance*)pChar)->m_SkeletonPose.FillRopeLenArray(len,i0,sz);
	return i0-i0org;
}


void CSkeletonPose::SynchronizeWithPhysicalEntity(IPhysicalEntity *pent, const Vec3& posMaster,const Quat& qMaster)
{
	SynchronizeWithPhysicalEntity(pent, posMaster,qMaster, QuatT(IDENTITY) );
	UpdateBBox(1);
}


void CSkeletonPose::SynchronizeWithPhysicalEntity(IPhysicalEntity *pent, const Vec3& posMaster,const Quat& qMaster, QuatT offset, int iDir )
{

	if (pent && (iDir==0 || iDir<0 && pent->GetType()!=PE_ARTICULATED && pent->GetType()!=PE_RIGID))
	{	
		// copy state _to_ physical entity
		pe_params_part partpos;
		int i,j;
		for(i=j=0; i<(int)m_AbsolutePose.size(); i++) 
			if (GetModelJointPointer(i)->m_PhysInfo[0].pPhysGeom)
				j=i;

		if (pent->GetType()==PE_ARTICULATED) 
			offset.t = -m_AbsolutePose[getBonePhysChildIndex(0)].t;

		for(i=0; i<(int)m_AbsolutePose.size();i++) 
			if (GetModelJointPointer(i)->m_PhysInfo[0].pPhysGeom)
			{
				partpos.partid = i;
				partpos.bRecalcBBox = i==j;
				QuatT q = offset*m_AbsolutePose[i];
				partpos.pos = q.t;
				partpos.q = q.q;
				pent->SetParams(&partpos);
			}

			for(i=m_pInstance->m_AttachmentManager.GetAttachmentCount()-1; i>=0; i--) 
				m_pInstance->m_AttachmentManager.UpdatePhysicalizedAttachment(i,pent,offset);
	}	
	else
	{	// copy state _from_ physical entity
		if (!pent && !m_nAuxPhys) 
			return;

		int numLODs = m_pInstance->m_pModel->m_arrModelMeshes.size();
		int i,j,nLod = min(m_pCharPhysics!=pent || m_bPhysicsRelinquished?1:0,numLODs-1);
		QuatT roffs(!offset.q,-offset.t*offset.q);
		pe_status_pos sp;
		pe_status_joint sj;
		m_bPhysicsAwake = 0;


		if (pent)
		{
			pe_status_awake statusTmp2; statusTmp2.lag=2;
			m_bPhysicsAwake = pent->GetStatus(&statusTmp2);
		}
		for(j=0;j<m_nAuxPhys;j++)
		{
			pe_status_awake statusTmp; 
			m_bPhysicsAwake |= m_auxPhys[j].pPhysEnt->GetStatus(&statusTmp);
		}

		if (!m_bPhysicsAwake && !m_bPhysicsWasAwake)
			return;
		if (m_arrPhysicsJoints.empty())
			InitPhysicsSkeleton();

		if (!m_bPhysicsAwake)
			m_fPhysBlendTime = m_fPhysBlendMaxTime+0.1f;

		ResetNonphysicalBoneRotations(nLod, m_fPhysBlendTime*m_frPhysBlendMaxTime);	

		if (pent)
		{
			pe_status_pos partpos;
			partpos.flags = status_local;
			for(i=0; i<(int)m_AbsolutePose.size(); i++) 
				if (GetModelJointPointer(i)->m_PhysInfo[nLod].pPhysGeom) 
				{
					partpos.partid = i;
					//	partpos.pMtx3x4 = &m_arrJoints[i].m_AbsolutePose;
					int32 status = pent->GetStatus(&partpos);
					if (status==0)
						continue;
				/*	{
						g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pInstance->m_pModel->GetModelFilePath(), "GetStatus() returned 0" );
						assert(0);
#if (EXTREME_TEST==1)
						CryError("CryAnimation: invalid status of GetStatus: %s",m_pInstance->GetModelFilePath());
#endif
					}*/

					m_AbsolutePose[i] = roffs*QuatT(partpos.q,partpos.pos);
					if (m_fPhysBlendTime < m_fPhysBlendMaxTime)
						break; // if in blending stage, do it only for the root
				}

				//Matrix33 mtxRel;
				for(; i<(int)m_AbsolutePose.size(); i++) 
					if (GetModelJointPointer(i)->m_PhysInfo[nLod].pPhysGeom) 
					{
						sj.idChildBody = i;
						int status=pent->GetStatus(&sj);
						if (status==0)
						{
							g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pInstance->m_pModel->GetModelFilePath(), "GetStatus() returned 0" );	assert(0);
#if (EXTREME_TEST==1)
							CryError("CryAnimation: invalid status of GetStatus: %s",m_pInstance->GetFilePath());
#endif
						}

						m_RelativePose[i].q =	m_arrPhysicsJoints[i].m_qRelPhysParent[nLod] * sj.quat0 * Quat::CreateRotationXYZ(sj.q+sj.qext);
						m_RelativePose[i].t = m_arrPhysicsJoints[i].m_DefaultRelativeQuat.t*m_fScale; 
					}

					int status2 = pent->GetStatus(&sp);
					if (status2==0)
					{
						g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pInstance->m_pModel->GetModelFilePath(), "GetStatus() returned 0" );		assert(0);
#if (EXTREME_TEST==1)
						CryError("CryAnimation: invalid status of GetStatus: %s",m_pInstance->GetFilePath());
#endif
					}

		}
		else
		{
			sp.pos = posMaster;
			sp.q = qMaster;
		}

		// additionally copy state from all ropes present in this character
		pe_status_rope sr;
		for(j=0;j<m_nAuxPhys;j++)
		{
			sr.pPoints = m_auxPhys[j].pVtx;
			m_auxPhys[j].pPhysEnt->GetStatus(&sr);
			for(i=0;i<m_auxPhys[j].nBones;i++) 
			{
				Vec3 dir = ((sr.pPoints[i+1]-sr.pPoints[i])*sp.q)*offset.q;
				float len = dir.GetLength();
				int nBone=m_auxPhys[j].pauxBoneInfo[i].iBone;
				//SJoint &bone = m_arrJoints[nBone];

				QuatT& matBoneGlobal34 = m_AbsolutePose[nBone];

				Vec3 v0		=	m_auxPhys[j].pauxBoneInfo[i].dir0;
				Vec3 v1   = len>1E-5f ? dir/len : v0;
				Quat q0		=	m_auxPhys[j].pauxBoneInfo[i].quat0, qparent;
				//		float scale	=	len*m_auxPhys[j].pauxBoneInfo[i].rlen0;
				matBoneGlobal34.q=(Quat::CreateRotationV0V1(v0,v1)*q0);//*scale;
				matBoneGlobal34.t = roffs*((sr.pPoints[i]-sp.pos)*sp.q);
			}
			if (GetModelJointPointer(i=m_auxPhys[j].pauxBoneInfo[m_auxPhys[j].nBones-1].iBone)->numChildren()>0)
				m_AbsolutePose[GetModelJointChildIndex(i,0)].t = roffs*((sr.pPoints[m_auxPhys[j].nBones]-sp.pos)*sp.q);
		}

		UnconvertBoneGlobalFromRelativeForm(m_fPhysBlendTime >= m_fPhysBlendMaxTime, nLod);
		ConvertBoneGlobalToRelativeMatrices();

		if(m_bPhysicsAwake)
			ForceReskin();
		m_bPhysicsWasAwake = m_bPhysicsAwake;
	}
}




IPhysicalEntity *CSkeletonPose::RelinquishCharacterPhysics(const Matrix34 &mtx, float stiffness) 
{
	if (m_bHasPhysics) 
	{
		int numLODs = m_pInstance->m_pModel->m_arrModelMeshes.size();
		int i,j,nAuxPhys, nLod=min(numLODs-1,1), iRoot=getBonePhysChildIndex(0,0);
		//float ropeLen[64];
		//FillRopeLenArray(ropeLen,0,64);
		//DestroyCharacterPhysics();

		if (m_arrPhysicsJoints.empty())
			InitPhysicsSkeleton();
		ConvertBoneGlobalToRelativeMatrices();	// takes into accout all post-animation layers
		// store death pose (current) matRelative orientation in bone's m_pqTransform
		for(i=0;i<(int)m_AbsolutePose.size();i++) 
			m_arrPhysicsJoints[i].m_qRelFallPlay = m_RelativePose[i].q;

		ResetNonphysicalBoneRotations(nLod,1.0f); // reset nonphysical bones matRel to default pose matRel
		UnconvertBoneGlobalFromRelativeForm(false,nLod); // build matGlobals from matRelativeToParents 

		pe_params_articulated_body pab;
		pab.bGrounded = 0;
		pab.scaleBounceResponse = 1;
		pab.bCheckCollisions = 1;
		pab.bCollisionResp = 1;

		IPhysicalEntity *res = g_pIPhysicalWorld->CreatePhysicalEntity(PE_ARTICULATED,&pab);//,&pp,0);
		pe_simulation_params sp; 
		sp.iSimClass = 6;	// to make sure the entity is not processed until it's ready (multithreaded)
		res->SetParams(&sp);
		m_vOffset.zero();
		BuildPhysicalEntity(res, m_fMass, m_iSurfaceIdx,stiffness, nLod,0, Matrix34(Vec3(m_fScale),Quat(IDENTITY), Vec3(ZERO) ));

		pe_params_joint pj;
		pj.bNoUpdate = 1;
		iRoot = getBonePhysChildIndex(0,nLod);
		for(i=0;i<3;i++) { pj.kd[i]=0.1f; }//pj.ks[i]=0; }
		for(i=m_AbsolutePose.size()-1;i>=0;i--)	
			if (GetModelJointPointer(i)->m_PhysInfo[nLod].pPhysGeom) 
			{
				pj.op[0] = getBonePhysParentIndex(i,nLod);
				pj.op[1] = i;
				pj.flags = GetModelJointPointer(i)->m_PhysInfo[nLod].flags & (all_angles_locked | angle0_limit_reached*7);
				if (i==iRoot)
					pj.flags = 0;
				res->SetParams(&pj);
			}

		m_fPhysBlendTime = 0.0f;
		m_fPhysBlendMaxTime = Console::GetInst().ca_DeathBlendTime;
		if (m_fPhysBlendMaxTime>0.001f)
			m_frPhysBlendMaxTime = 1.0f/m_fPhysBlendMaxTime;
		else
		{
			m_fPhysBlendMaxTime = 0.0f;
			m_frPhysBlendMaxTime = 1.0f;
		}

		ResetNonphysicalBoneRotations(nLod,0.0f); // restore death pose matRel from m_pqTransform
		UnconvertBoneGlobalFromRelativeForm(false,nLod); // build matGlobals from matRelativeToParents

		m_vOffset.zero();
		m_bPhysicsAwake=m_bPhysicsWasAwake = 1; 
		//m_uFlags |= nFlagsNeedReskinAllLODs; // in order to force skinning update during the first death sequence frame
		pe_params_pos pp;
		pp.pMtx3x4 = (Matrix34*)&mtx;
		int bSkelQueued = res->SetParams(&pp)-1;
		SetForceSkeletonUpdate(33);

		sp.gravity.Set(0,0,-7.5f);
		sp.maxTimeStep = 0.025f;
		sp.damping = 0.1f;
		res->SetParams(&sp);

		//CreateAuxilaryPhysics(res, mtx, 1.0f,m_vOffset, 1, ropeLen,&(i=0),sizeof(ropeLen)/sizeof(ropeLen[0]));
		pe_params_rope pr;
		pr.bLocalPtTied = 1;
		for(i=nAuxPhys=0;i<m_nAuxPhys;i++)
		{
			for(j=0;j<m_auxPhys[i].nBones && !GetModelJointPointer(m_auxPhys[i].pauxBoneInfo[j].iBone)->m_PhysInfo[nLod].pPhysGeom;j++);
			if (j<m_auxPhys[i].nBones)
			{
				g_pIPhysicalWorld->DestroyPhysicalEntity(m_auxPhys[i].pPhysEnt);
				delete[] m_auxPhys[i].pauxBoneInfo;
				delete[] m_auxPhys[i].pVtx;
			} else
			{
				if (m_auxPhys[i].bTied0)
				{
					pr.pEntTiedTo[0] = res;
					pr.idPartTiedTo[0] = getBonePhysParentIndex(j = m_auxPhys[i].iBoneTiedTo[0],nLod);
					pr.ptTiedTo[0] = m_AbsolutePose[getBoneParentIndex(j)]*m_arrPhysicsJoints[j].m_DefaultRelativeQuat.t*m_fScale;
				}
				if (m_auxPhys[i].bTied1)
				{
					pr.pEntTiedTo[1] = res; 
					pr.idPartTiedTo[1] = getBonePhysChildIndex(m_auxPhys[i].iBoneTiedTo[1],nLod);
					pr.ptTiedTo[1] = m_AbsolutePose[pr.idPartTiedTo[1]].t;
				}
				pr.ptTiedTo[0] = m_AbsolutePose[pr.idPartTiedTo[0]].GetInverted()*pr.ptTiedTo[0];
				m_auxPhys[i].pPhysEnt->SetParams(&pr,-bSkelQueued>>31);
				m_auxPhys[nAuxPhys++] = m_auxPhys[i];
			}
		}
		m_nAuxPhys = nAuxPhys;

		MARK_UNUSED pr.pEntTiedTo[1],pr.idPartTiedTo[1];
		IAttachmentObject* pAttachment;
		ICharacterInstance *pChar;
		IPhysicalEntity *pRope;
		for(i=m_pInstance->m_AttachmentManager.GetAttachmentCount()-1; i>=0; i--)
			if (m_pInstance->m_AttachmentManager.GetInterfaceByIndex(i)->GetType()==CA_BONE && 
				(pAttachment = m_pInstance->m_AttachmentManager.GetInterfaceByIndex(i)->GetIAttachmentObject()) && (pChar=pAttachment->GetICharacterInstance()))
				for(j=0; pRope=pChar->GetISkeletonPose()->GetCharacterPhysics(j); j++)
				{
					pr.pEntTiedTo[0] = res;
					pr.idPartTiedTo[0] = getBonePhysParentOrSelfIndex(m_pInstance->m_AttachmentManager.GetInterfaceByIndex(i)->GetBoneID());
					pRope->SetParams(&pr);
				}
				if (m_pCharPhysics)
					g_pIPhysicalWorld->DestroyPhysicalEntity(m_pCharPhysics);
				if (m_ppBonePhysics)
				{
					for(i=0;i<(int)m_AbsolutePose.size();i++) if (m_ppBonePhysics[i])
						m_ppBonePhysics[i]->Release(), g_pIPhysicalWorld->DestroyPhysicalEntity(m_ppBonePhysics[i]);
					delete[] m_ppBonePhysics;
					m_ppBonePhysics = 0;
				}

				m_pCharPhysics = res;
				m_bPhysicsRelinquished = 1;
				m_nSpineBones = 0;
				m_iFlyDir = -1;
				m_bAliveRagdoll = stiffness>0;
				m_bLimpRagdoll = 0;
				m_timeRagdolled = 0;
				m_timeLying=m_timeStandingUp = -1; m_timeNoColl=0;
				m_b3DOFStandup = 0;

		sp.iSimClass = 2;
		res->SetParams(&sp);

		return res;
	}
	return 0;
}




bool CSkeletonPose::AddImpact(const int partid,Vec3 point,Vec3 impact)
{
	float scale=1.0f;

	if (m_pCharPhysics)
	{
		if (m_pInstance->m_pModel->m_ObjectType==CGA || (unsigned int)partid>=(unsigned int)m_AbsolutePose.size())
		{
			// sometimes out of range value (like -1) is passed to indicate that no impulse should be added to character
			return false;
		}
		//int i;

		//SJoint* pImpactBone = &m_arrJoints[partid];
		const char* szImpactBoneName = GetModelJointPointer(partid)->GetJointName();

		//for(i=0; i<4 && !(m_pIKEffectors[i] && m_pIKEffectors[i]->AffectsBone(partid)); i++);
		const char *ptr = strstr(szImpactBoneName,"Spine");
		if (strstr(szImpactBoneName,"Pelvis") || ptr && !ptr[5])
			return false;

		if (strstr(szImpactBoneName,"UpperArm") || strstr(szImpactBoneName,"Forearm"))
			impact *= 0.35f;
		else if (strstr(szImpactBoneName,"Hand"))
			impact *= 0.2f;
		else if (strstr(szImpactBoneName,"Spine1"))
			impact *= 0.2f;

		pe_action_impulse impulse;
		impulse.partid = partid;
		impulse.impulse = impact;
		impulse.point = point;
		m_pCharPhysics->Action(&impulse);

		return true;
	}
	else 
	{
		pe_action_impulse impulse;
		impulse.impulse = impact;
		impulse.point = point;
		for(int i=0;i<m_nAuxPhys;i++)
			m_auxPhys[i].pPhysEnt->Action(&impulse);
	}

	return false;
}





void CSkeletonPose::ResetNonphysicalBoneRotations(int nLod, float fBlend)
{
	// set non-physical bones to their default position wrt parent for LODs>0
	// do it for every bone except the root, parents first
	uint32 numBones = m_arrPhysicsJoints.size();
	//	assert(numBones); having 0 physical bones is a legal case
	for (uint32 nBone=1; nBone<numBones; ++nBone)
	{
		CModelJoint* pInfo = GetModelJointPointer(nBone);
		if (!pInfo->getPhysInfo(nLod).pPhysGeom)
		{
			CPhysicsJoint* pBone = &m_arrPhysicsJoints[nBone];
			QuatT qDef = QuatT(pBone->m_DefaultRelativeQuat.q, pBone->m_DefaultRelativeQuat.t*m_fScale);
			if (fBlend>=1.0f)
				m_RelativePose[nBone] = qDef;
			else 
			{
				QuatT g = QuatT(pBone->m_qRelFallPlay, pBone->m_DefaultRelativeQuat.t*m_fScale);
				m_RelativePose[nBone].SetNLerp(g,qDef, fBlend );
			}

		}
	}
}





//////////////////////////////////////////////////////////////////////////
// Multiplies each bone global matrix with the parent global matrix,
// and calculates the relative-to-default-pos matrix. This is essentially
// the process opposite to conversion of global matrices to relative form
// performed by ConvertBoneGlobalToRelativeMatrices()
// NOTE:
//   The root matrix is relative to the world, which is its parent, so it's
//   obviously both the global and relative (they coincide), so it doesn't change
//
// PARAMETERS:
//   bNonphysicalOnly - if set to true, only those bones that have no physical geometry are affected
//
// ASSUMES: in each m_matRelativeToParent matrix, upon entry, there's a matrix relative to the parent
// RETURNS: in each bone global matrix, there's the actual global matrix
void CSkeletonPose::UnconvertBoneGlobalFromRelativeForm(bool bNonphysicalOnlyArg, int nLod, bool bRopeTipsOnly)
{
	unsigned numBones = m_AbsolutePose.size(), nRopeLevel=0;	
	bool bNonphysicalOnly = true;
	// start from 1, since we don't affect the root, which is #0
	for (unsigned i = 1; i < numBones; ++i)
	{
		CModelJoint* pBoneInfo = GetModelJointPointer(i);
		bool bPhysical = pBoneInfo->getPhysInfo(nLod).pPhysGeom || pBoneInfo->getLimbId()>=100;
		if ((pBoneInfo->getPhysInfo(0).flags & 0xFFFF0000)==0x30000)
			nRopeLevel = pBoneInfo->m_numLevels;
		else
		{
			if (pBoneInfo->m_numLevels<nRopeLevel)
				nRopeLevel = 0;
			if ((!bNonphysicalOnly || !bPhysical) && (!bRopeTipsOnly || nRopeLevel))	// -2 is for rope bones, never force their positions
			{
				//	assert (pBoneInfo->hasParent());
				// CRAIG - gross hack - must be fixed properly
				if (!pBoneInfo->qhasParent())
					continue;
				// END GROSS HACK

				m_AbsolutePose[i] = m_AbsolutePose[getBoneParentIndex(i)]*m_RelativePose[i];
			}
		}

		if (bPhysical) // don't update the 1st physical bone (pelvis) even if bNonphysicalOnlyArg is false
			bNonphysicalOnly = bNonphysicalOnlyArg;
	}
}



//////////////////////////////////////////////////////////////////////////
// Replaces each bone global matrix with the relative matrix.
// The root matrix is relative to the world, which is its parent, so it's
// obviously both the global and relative (they coincide), so it doesn't change
//
// ASSUMES: the matrices are unitary and orthogonal, so that Transponse(M) == Inverse(M)
// ASSUMES: in each bone global matrix, upon entry, there's the actual global matrix
// RETURNS: in each m_matRelativeToParent matrix, there's a matrix relative to the parent
void CSkeletonPose::ConvertBoneGlobalToRelativeMatrices()
{
	uint32 numJoints = m_AbsolutePose.size();
	m_RelativePose[0] = m_AbsolutePose[0];
	for (uint32 i=1; i<numJoints; i++)
	{
		int32 p=m_parrModelJoints[i].m_idxParent;
		m_RelativePose[i] = m_AbsolutePose[p].GetInverted() * m_AbsolutePose[i];
	}

}






// Forces skinning on the next frame
void CSkeletonPose::ForceReskin()
{
	//m_uFlags |= nFlagsNeedReskinAllLODs;
	m_bPhysicsWasAwake = true;
}


