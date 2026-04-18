========================================================================
       DYNAMIC LINK LIBRARY : CryAISystem
========================================================================


AppWizard has created this CryAISystem DLL for you.  

This file contains a summary of what you will find in each of the files that
make up your CryAISystem application.

CryAISystem.dsp
    This file (the project file) contains information at the project level and
    is used to build a single project or subproject. Other users can share the
    project (.dsp) file, but they should export the makefiles locally.

CryAISystem.cpp
    This is the main DLL source file.

	When created, this DLL does not export any symbols. As a result, it 
	will not produce a .lib file when it is built. If you wish this project
	to be a project dependency of some other project, you will either need to 
	add code to export some symbols from the DLL so that an export library 
	will be produced, or you can check the "doesn't produce lib" checkbox in 
	the Linker settings page for this project. 

/////////////////////////////////////////////////////////////////////////////
Other standard files:

StdAfx.h, StdAfx.cpp
    These files are used to build a precompiled header (PCH) file
    named CryAISystem.pch and a precompiled types file named StdAfx.obj.


/////////////////////////////////////////////////////////////////////////////
Other notes:

AppWizard uses "TODO:" to indicate parts of the source code you
should add to or customize.


/////////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------------
bool COPFollow::Execute(CPipeUser *pOperand)
{
	CAIObject *pTarget = pOperand->m_pAttentionTarget;
	CAISystem *pSystem = GetAISystem();
	Vec3 mypos,vFollowPos;
 
	if (!pTarget || m_bUseLastOpResult) 
	{
		if (pOperand->m_pLastOpResult)
			pTarget = pOperand->m_pLastOpResult;
		else
		{
			// no target, nothing to approach to
			Reset(pOperand);
			return true;
		}
	}
	int iType = pOperand->GetType();

	if(iType == AIOBJECT_VEHICLE)
		pOperand->m_State.bodystate =1; // force "chase" condition for vehicle

	mypos = pOperand->GetPos();
	Vec3 vTargetPos = pTarget->GetPos();

	if(!m_pTargetPathMarker)
	{
		//created here since the target in COPFollow constructor is not known
		float fStep;
		switch(iType)
		{
		case AIOBJECT_VEHICLE:
			fStep = 0.5f;
			break;
		case AIOBJECT_PUPPET:
			fStep = 0.3f;
			break;
		default:
			fStep = 0.2f;
		}
		m_pTargetPathMarker = new CPathMarker(m_fDistance + m_fDistance+1,fStep);
		
		Vec3 vTargetDir(0,-1,0);
		//Convert target angles to normalized sight vector
		// TO DO: replace with new math function when ready
		Matrix44 tm;
		tm.SetIdentity();
		tm=GetRotationZYX44(pTarget->GetAngles()*(-gf_DEGTORAD)); //NOTE: angles in radians and negated 
		vTargetDir = GetTransposed44(tm)*(vTargetDir);

		if(iType == AIOBJECT_VEHICLE)
			vTargetDir *= m_fDistance;
		else
			vTargetDir *= -m_fDistance;
		m_pTargetPathMarker->Init(pTarget->GetPos()+ vTargetDir, pTarget->GetPos() );

		m_pFollowTarget = GetAISystem()->CreateDummyObject();
		//m_vLastUpdatedPos = ..;
		m_vLastPos = mypos;
		m_vLastTargetPos = vTargetPos;
	
		pOperand->Following(pTarget);

	}

	m_pTargetPathMarker->Update(vTargetPos);
	float fActualFollowDistance = m_pTargetPathMarker->GetPointAtDistance(vTargetPos,m_fDistance,&vFollowPos);
	//tricky: can't init m_fLastDistance before updating the PathMarker
	if(m_fLastDistance <=0)
		m_fLastDistance = m_pTargetPathMarker->GetDistanceToPoint(vTargetPos,mypos);

	m_pFollowTarget->SetPos(vFollowPos);
	//Debug only
	/*
	IPuppet *pPuppet=0;
	if (pOperand->CanBeConvertedTo(AIOBJECT_PUPPET,(void**)&pPuppet))
		pPuppet->SetRefPointPos(vFollowPos);
	*/
	////////////

	Vec3	projectedDist = mypos-vFollowPos;
	projectedDist.z = 0;
	float dist = projectedDist.GetLength();

	bool	bIndoor=false;
	int building;
	IVisArea *pArea;
	if (GetAISystem()->CheckInside(mypos,building,pArea))
		bIndoor=true;
		
	//GraphNode *pThisNode = pSystem->GetGraph()->GetEnclosing(pOperand->GetPos(),pOperand->m_pLastNode);
	
	
	if(pOperand->m_bForceTargetPos)
		pOperand->m_State.vTargetPos = vFollowPos;
/*
	if( ((!pOperand->m_bNeedsPathIndoor)&&bIndoor) || ((!pOperand->m_bNeedsPathOutdoor)&&(!bIndoor)) || 
		pOperand->m_State.pathUsage == SOBJECTSTATE::PU_NoPathfind )
	{
		mypos-=vFollowPos;
		if (!m_bPercent)
		{
			if (dist < m_fDistance)
			{
					m_fInitialDistance = 0;
					return true;
			}
		}
		else
		{ 
			if (dist < m_fInitialDistance)
			{
					m_fInitialDistance = 0;
					return true;
			}
		}

		// no pathfinding - just approach
		pOperand->m_State.vTargetPos = vFollowPos;
		pOperand->m_State.vMoveDir = vFollowPos - pOperand->GetPos();
		pOperand->m_State.vMoveDir.Normalize();

		// try to steer now - only if it's a CAR
		CAIVehicle	*pVelicle;
		if(pOperand->CanBeConvertedTo( AIOBJECT_CVEHICLE, (void**)&pVelicle ))
			if( pVelicle->GetVehicleType() == AIVEHICLE_CAR)
		{
			GraphNode *pThisNode = pSystem->GetGraph()->GetEnclosing(pOperand->GetPos(),pOperand->m_pLastNode);
			pOperand->Steer(vFollowPos,pThisNode);
			pOperand->m_State.vMoveDir.Normalize();
		}

		m_fLastDistance = dist;
		return false;
	}
*/
	if (!m_pPathfindDirective)
	{
		// generate path to target
		m_pPathfindDirective = new COPPathFind("",pTarget);
		pOperand->m_nPathDecision = PATHFINDER_STILLTRACING;
		if (m_pPathfindDirective->Execute(pOperand))
		{
			if (pOperand->m_nPathDecision == PATHFINDER_NOPATH)
			{
					pOperand->m_State.vMoveDir.Set(0,0,0);
					Reset(pOperand);
					return true;
			}
		}

		if (m_pTempTarget)
		{
			pOperand->m_State.vMoveDir = GetNormalized(m_pTempTarget->GetPos()-pOperand->GetPos());
			pOperand->m_State.vTargetPos = m_pTempTarget->GetPos();
		}


		m_fTime = 0;
		return false;
	}
	else
	{
		// if we have a path, trace it
		if (pOperand->m_nPathDecision==PATHFINDER_PATHFOUND)
		{
			bool bDone=false;
			if (m_pTraceDirective)
			{
				// keep tracing - previous code will stop us when close enough
				bDone = m_pTraceDirective->Execute(pOperand);


//if(0)
				if (!bDone)
				{

					GraphNode *pThisNode = pSystem->GetGraph()->GetEnclosing(pOperand->GetPos(),pOperand->m_pLastNode);
					pOperand->m_pLastNode = pThisNode;
					if(m_pTraceDirective->m_pNavTarget)
						pOperand->Steer( m_pTraceDirective->m_pNavTarget->GetPos(),pThisNode);

				// for vehicles - regenirate path to target once in a while 
					if(pOperand->GetType() == AIOBJECT_VEHICLE)
					{

						pOperand->m_State.vMoveDir.Normalize();

						m_fTime+=1;
//					if(0)
						if(pOperand->m_State.pathUsage == SOBJECTSTATE::PU_NewPathWanted)
						if(m_fTime > 15)
						{
//							m_fTime = 0;
							if( m_pTraceDirective->m_pNavTarget )
							{
								m_pTempTarget = GetAISystem()->CreateDummyObject();
								m_pTempTarget->SetPos(m_pTraceDirective->m_pNavTarget->GetPos());
							}
							delete m_pTraceDirective;
							m_pTraceDirective = 0;
							delete m_pPathfindDirective;
							m_pPathfindDirective = 0;	

							if (m_pTempTarget)
							{
								pOperand->m_State.vMoveDir = GetNormalized(m_pTempTarget->GetPos()-pOperand->GetPos());
								pOperand->m_State.vTargetPos = m_pTempTarget->GetPos();
							}

							return false;
						}
					}

					
				}
			}
			else
			{
				if (m_pTempTarget)
				{
					GetAISystem()->RemoveDummyObject(m_pTempTarget);
					m_pTempTarget = 0;
				}

				bool bExact = false; //(pTarget->GetType()!=AIOBJECT_PUPPET) && (pTarget->GetType()!=AIOBJECT_PLAYER);
				m_pTraceDirective = new COPTrace(bExact,m_fDistance);
				bDone = m_pTraceDirective->Execute(pOperand);
			}

			if (bDone)
			{
				/*if(pOperand->GetType() == AIOBJECT_VEHICLE)
				{
					Reset(pOperand);
					return true;
				}*/
				float fDesiredDistance = m_fDistance;
				if (m_pFollowTarget)//Luc pOperand->m_pAttentionTarget )
				{
						//2D-ize the check - TO DO: remove it for 3D pathfinding
						Vec3 op_pos = pOperand->GetPos();
						//Luc Vec3 tg_pos = vFollowPos;//Luc pOperand->m_pAttentionTarget->GetPos();
						op_pos.z = vFollowPos.z;
						if (GetLength(op_pos-vFollowPos)>fDesiredDistance)
						{
							m_pTempTarget = GetAISystem()->CreateDummyObject();
							m_pTempTarget->SetPos(vFollowPos);//Luc pOperand->m_pAttentionTarget->GetPos());
							delete m_pTraceDirective;
							m_pTraceDirective = 0;
							delete m_pPathfindDirective;
							m_pPathfindDirective = 0;	

							m_pPathfindDirective = new COPPathFind("",m_pFollowTarget);
							pOperand->m_nPathDecision = PATHFINDER_STILLTRACING;
							if (m_pPathfindDirective->Execute(pOperand))
							{
								if (pOperand->m_nPathDecision == PATHFINDER_NOPATH)
								{
									pOperand->m_State.vMoveDir.Set(0,0,0);
									Reset(pOperand);
									return true;
								}
							}
						}
/*						else
						{
							Reset(pOperand);
							return true;
						}*/
				}
				else
				{
					Reset(pOperand);
					return true;
				}
			}
		}
		else
		{
			if (pOperand->m_nPathDecision==PATHFINDER_NOPATH)
			{
				Reset(pOperand);
				return true;
			}
			else
				m_pPathfindDirective->Execute(pOperand);

		}
	}

	//m_fLastDistance = dist;

//if(0)
	if (m_pTempTarget)
	{
		pOperand->m_State.vMoveDir = GetNormalized(m_pTempTarget->GetPos()-pOperand->GetPos());
		pOperand->m_State.vTargetPos = m_pTempTarget->GetPos();
	}
//	else
//	{
//		pOperand->m_State.vMoveDir = GetNormalized(-mypos);
//	}

	// Check the speed and distance with the followed entity, try to keep up with it
			
	float fCurrentTime = m_pTimer->GetCurrTime();
	if(fCurrentTime - m_fLastUpdatedTime > m_fUpdatePeriod)
	{
		mypos = pOperand->GetPos();

		float fDeltaTime = fCurrentTime - m_fLastUpdatedTime;
		// average speeds
		float fMySpeed = (mypos - m_vLastPos).GetLength() / fDeltaTime;
		float fDesiredSpeed = pOperand->m_State.fDesiredSpeed;
		float fTargetSpeed = (vTargetPos - m_vLastTargetPos).GetLength() / fDeltaTime;
		// distance along path and its delta 
		float fCurrentDistance = m_pTargetPathMarker->GetDistanceToPoint(vTargetPos,mypos);
		float fDeltaDistance = fCurrentDistance - m_fLastDistance;
		
		//rules to modify speed and send signals to the leading entity
		// TO DO: different values for vehicles and puppets
		// TO DO: fuzzy logic?
		if(iType==AIOBJECT_VEHICLE)
		{	
			bool bTooDistant = fCurrentDistance > m_fDistance*1.5;
			bool bAccelerating = fMySpeed > m_fLastSpeed;
			// Tell the leading vehicle to slow down if:
			// distance is greater and deltaDistance is not decreasing
			if( bTooDistant && fDeltaDistance > -0.1f
			// distance is increasing too fast
			||	fDeltaDistance > 1
			// distance is too high, /*desired speed is already max*/ and I'm not accelerating
			|| bTooDistant && /*fDesiredSpeed==1 &&*/ !bAccelerating)
			{
				IVehicleProxy* pLeadingProxy=NULL;
				IAIObject* pLeadingAIDriver = NULL;
				IAIObject* pLeadingVehicle = pOperand->GetFollowedLeader();
				if(!pLeadingVehicle)
					pLeadingVehicle = pTarget;
				if(pLeadingVehicle->GetProxy()->QueryProxy(AIPROXY_VEHICLE, (void**)&pLeadingProxy))
					pLeadingAIDriver = pLeadingProxy->GetDriverAI();
				//if there's a driver, send the signal to him directly
				if(pLeadingAIDriver)
					pLeadingAIDriver->SetSignal(1,"OnSlowDown");
				else //otherwise, send it to the vehicle
					pLeadingVehicle->SetSignal(1,"OnSlowDown");
			}
			bool bTooClose = fCurrentDistance < m_fDistance*1.5;
			// Tell the leading vehicle to speed up if:

		}
		// update values to be used next time
		m_fLastUpdatedTime = fCurrentTime;
		m_vLastUpdatedPos = vTargetPos;
		m_vLastPos = mypos;
		m_fLastDistance = fCurrentDistance;
		m_fLastSpeed = fMySpeed;
		/*
		if(iType == AIOBJECT_VEHICLE)
		{
			IVehicleProxy* proxy=NULL;
			if(pOperand->GetProxy()->QueryProxy(AIPROXY_VEHICLE, (void**)&proxy))
				fCurrentSpeed = proxy->GetCurrentSpeed();
			else
				return false;
		}
			
		if(pTarget->GetType() == AIOBJECT_VEHICLE)
		{
			IVehicleProxy* proxy=NULL;
			if(pTarget->GetProxy()->QueryProxy(AIPROXY_VEHICLE, (void**)&proxy))
				fTargetSpeed = proxy->GetCurrentSpeed();
			else
				return false;
		}
		*/
	}

	return false;
}

//
//-------------------------------------------------------------------------------------------------------------------







void CPuppet::Steer(const Vec3 & vTargetPos, GraphNode * pNode)
void CPuppet::Navigate(CAIObject *pTarget)
void CPuppet::CrowdControl(void)
void CPuppet::FireCommand(void)
void CPuppet::CheckPlayerTargeting(void)
Vec3 CPuppet::GetOutdoorHidePoint(int nMethod, float fSearchDistance, bool bSameOk)
bool CPuppet::PointVisible(const Vec3 &pos)

void CFormation::Update(CAIObject *pOwner)
void CFormation::Create(FormationDescriptor & desc)

bool COPLookAt::Execute(CPipeUser *pOperand)
bool COPFollow::Execute(CPipeUser *pOperand)
bool COPStrafe::Execute(CPipeUser *pOperand)
bool COPLookAround::Execute(CPipeUser *pOperand)
bool COPHide::Execute(CPipeUser *pOperand)
bool COPTrace::Execute(CPipeUser *pOperand)
bool COPJumpCmd::Execute(CPipeUser *pOperand)
bool COPHide::IsBadHiding(CPipeUser *pOperand)

void CAIVehicle::Steer(const Vec3 & vTargetPos, GraphNode * pNode)
void CAIVehicle::Navigate(CAIObject *pTarget)

IAIObject * CAISystem::GetNearestToObject(IAIObject * pRef, unsigned short nType, float fRadius)
if (m_cvViewField->GetIVal())
void CAISystem::DebugDrawDirections(IRenderer *pRenderer)
void CAISystem::DebugDrawAlter(IRenderer *pRenderer)

void CPuppet::Load(CStream &stm)
void CPuppet::Save(CStream &stm)

void CAIVehicle::Load(CStream &stm)
void CAIVehicle::Save(CStream &stm)





	SHARED_RELOAD = function (self,entity, sender)
		if (entity.cnt) then
			if (entity.cnt.ammo_in_clip) then
				if (entity.cnt.ammo_in_clip > 5) then
					do return end
				end
			end
		end


		AI:CreateGoalPipe("reload_timeout");
		AI:PushGoal("reload_timeout","timeout",1,entity:GetAnimationLength("reload"));
		entity:InsertSubpipe(0,"reload_timeout");

		entity:StartAnimation(0,"reload",4);
--		if (AI:GetGroupCount(entity.id) > 1) then
--			AI:Signal(SIGNALID_READIBILITY, 1, "RELOADING",entity.id);
--		end
		BasicPlayer.Reload(entity);	
	end,


