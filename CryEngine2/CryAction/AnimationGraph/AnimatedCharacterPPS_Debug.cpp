//--------------------------------------------------------------------------------

#include "StdAfx.h"
#include "AnimatedCharacter.h"
#include "AnimationGraphCVars.h"
#include "PersistantDebug.h"
#include "DebugHistory.h"

//--------------------------------------------------------------------------------

void CAnimatedCharacter::SAnimErrorClampStats::Update(float frameTime, float distance, float angle, float distanceMax, float angleMax)
{
	m_elapsedTime += frameTime;
	m_distanceMin = min(distance, m_distanceMin);
	m_distanceMax = max(distance, m_distanceMax);
	m_distanceAvg = (distance * frameTime + m_distanceAvg * m_elapsedTime) / (frameTime + m_elapsedTime);
	m_distanceClampDuration += (distance > distanceMax) ? frameTime : 0.0f;
	m_angleMin = min(angle, m_angleMin);
	m_angleMax = max(angle, m_angleMax);
	m_angleAvg = (distance * frameTime + m_angleAvg * m_elapsedTime) / (frameTime + m_elapsedTime);
	m_angleClampDuration += (angle > angleMax) ? frameTime : 0.0f;
}

//--------------------------------------------------------------------------------

struct SDebugNodeSizeDesc
{
	Vec2 m_prefered;
	Vec2 m_importance;
};

struct IDebugNode
{
	virtual const SDebugNodeSizeDesc& GetSizeDesc() const = 0;
	virtual void SetActualSize(const Vec2& size) = 0;
};

struct IDebugContainer : public IDebugNode
{
	virtual void AddChild(IDebugNode* child);

	virtual const SDebugNodeSizeDesc& GetSizeDesc() const;
	virtual void SetAvailableSize(const Vec2& size);
};

class LayoutManager
{
public:
	void setRootNode(IDebugNode* root);
	void setTotalSize(const Vec2& size);
private:
	void layout();
};

/*
addRow("params");
add("param1");
add("param2");
add("param3");
add("param4");
addRow()
add(param4);
*/

//--------------------------------------------------------------------------------

#ifdef DEBUGHISTORY
void CAnimatedCharacter::LayoutHelper(const char* id, const char* name, bool visible, float minout, float maxout, float minin, float maxin, float x, float y, float w/*=1.0f*/, float h/*=1.0f*/)
{
	if (!visible)
	{
		m_debugHistoryManager->RemoveHistory(id);
		return;
	}

	IDebugHistory* pDH = m_debugHistoryManager->GetHistory(id);
	if (pDH != NULL)
		return;

	pDH = m_debugHistoryManager->CreateHistory(id, name);
	pDH->SetupScopeExtent(minout, maxout, minin, maxin);
	pDH->SetVisibility(true);

	static float x0 = 0.01f;
	static float y0 = 0.05f;
	static float x1 = 0.99f;
	static float y1 = 1.00f;
	static float dx = x1 - x0;
	static float dy = y1 - y0;
	static float xtiles = 6.0f;
	static float ytiles = 4.0f;
	pDH->SetupLayoutRel(x0 + dx * x / xtiles,
											y0 + dy * y / ytiles, 
											dx * w * 0.95f / xtiles, 
											dy * h * 0.93f / ytiles, 
											dy * 0.02f / ytiles);
}
#endif

//--------------------------------------------------------------------------------

#ifdef DEBUGHISTORY // Remove function from code memory when not used.
void CAnimatedCharacter::SetupDebugHistories()
{
	static bool showAll = false;
	bool any = false;
	bool showMotionParams = (CAnimationGraphCVars::Get().m_debugMotionParams != 0) || showAll;
	any |= showMotionParams;
	bool showPredError = (CAnimationGraphCVars::Get().m_debugMotionParams != 0) || showAll;
	any |= showPredError;
	bool showPredVelo = (CAnimationGraphCVars::Get().m_debugMotionParams != 0) || showAll;
	any |= showPredVelo;
	bool showSelectionParams = (CAnimationGraphCVars::Get().m_debugSelectionParams != 0) || showAll;
	any |= showSelectionParams;
	bool showAnimMovement = (CAnimationGraphCVars::Get().m_debugLocationsGraphs != 0) || showAll;
	any |= showAnimMovement;
	bool showEntMovement = (CAnimationGraphCVars::Get().m_debugLocationsGraphs != 0) || showAll;
	any |= showEntMovement;
	bool showAsset = (CAnimationGraphCVars::Get().m_debugLocationsGraphs != 0) || showAll;
	any |= showAsset;
	bool showTemp1 = ((CAnimationGraphCVars::Get().m_debugTempValues & 1) != 0) || showAll;
	any |= showTemp1;
	bool showTemp2 = ((CAnimationGraphCVars::Get().m_debugTempValues & 2) != 0) || showAll;
	any |= showTemp2;
	bool showTemp3 = ((CAnimationGraphCVars::Get().m_debugTempValues & 4) != 0) || showAll;
	any |= showTemp3;
	bool showTemp4 = ((CAnimationGraphCVars::Get().m_debugTempValues & 8) != 0) || showAll;
	any |= showTemp4;
	bool showMCM = (CAnimationGraphCVars::Get().m_debugMovementControlMethods != 0) || showAll;
	any |= showMCM;
	bool showFrameTime = (CAnimationGraphCVars::Get().m_debugFrameTime != 0) || showAll;
	any |= showFrameTime;

	bool showAnimError = (CAnimationGraphCVars::Get().m_debugAnimError != 0) || showAll;
	any |= showAnimError;
	bool showAnimTargetMovement = (CAnimationGraphCVars::Get().m_debugAnimTarget != 0) || showAll;
	any |= showAnimTargetMovement;
	bool showAnimEntOffset = (CAnimationGraphCVars::Get().m_debugAnimError != 0) || showAll;
	any |= showAnimEntOffset;


	if (any)
	{
		LayoutHelper("eDH_FrameTime", "FrameTime", showFrameTime, 0.0f, 1.0f, 0.0f, 0.0f, 5, 0.3f, 1, 0.7f);

		LayoutHelper("eDH_MovementControlMethodH", "MCM H", showMCM, 0, eMCM_COUNT-1, 0, eMCM_COUNT-1, 4, 0.2f, 1, 0.4f);
		LayoutHelper("eDH_MovementControlMethodV", "MCM V", showMCM, 0, eMCM_COUNT-1, 0, eMCM_COUNT-1, 4, 0.6f, 1, 0.4f);

		LayoutHelper("eDH_TurnSpeed", "TurnSpeed", showMotionParams, -360, +360, -1, +1, 0, 0, 1.0f, 0.5f);
		LayoutHelper("eDH_TravelSlope", "TravelSlope", showMotionParams, -25, 25, -25, 25, 0, 0.5f, 1.0f, 0.5f);
		LayoutHelper("eDH_TravelDirX", "TravelDirX", showMotionParams, -2, +2, -1, +1, 1, 0);
		LayoutHelper("eDH_TravelDirY", "TravelDirY", showMotionParams, -2, +2, -1, +1, 2, 0);
		LayoutHelper("eDH_TravelDistScale", "TravelDistScale", showMotionParams, 0, 1, 0, 1, 3, 0.0f, 1.0f, 0.2f);
		LayoutHelper("eDH_TravelDist", "TravelDist", showMotionParams, 0, 10, 0, 1, 3, 0.2f, 1.0f, 0.3f);
		LayoutHelper("eDH_TravelSpeed", "TravelSpeed", showMotionParams, 0, 10, 0, 1, 3, 0.5f, 1.0f, 0.5f);

		LayoutHelper("eDH_DesiredLocalLocationRZ", "LocalLocaRZ", showPredError, -180, +180, -1, +1, 0, 1, 1, 0.5f);
		LayoutHelper("eDH_DesiredLocalLocationTX", "LocalLocaTX", showPredError, -5, +5, -0.01f, +0.01f, 1, 1, 1, 0.5f);
		LayoutHelper("eDH_DesiredLocalLocationTY", "LocalLocaTY", showPredError, -5, +5, -0.01f, +0.01f, 2, 1, 1, 0.5f);

		LayoutHelper("eDH_DesiredLocalVelocityRZ", "LocalVeloRZ", showPredVelo, -180, +180, -1, +1, 0, 1.5f, 1, 0.5f);
		LayoutHelper("eDH_DesiredLocalVelocityTX", "LocalVeloTX", showPredVelo, -5, +5, -0.01f, +0.01f, 1, 1.5f, 1, 0.5f);
		LayoutHelper("eDH_DesiredLocalVelocityTY", "LocalVeloTY", showPredVelo, -5, +5, -0.01f, +0.01f, 2, 1.5f, 1, 0.5f);

		LayoutHelper("eDH_PredictionTime", "PredictionTime", showMotionParams, 0, 2, 0, 0, 3, 1, 1, 0.5f);
		LayoutHelper("eDH_Immediateness", "Immediateness", showMotionParams, 0, 2, 0, 0, 4, 1, 1, 0.5f);
		LayoutHelper("eDH_CurvingFraction", "Curving", showMotionParams, 0, 1, 0, 1, 3, 1.5f, 1.0f, 0.5f);


		LayoutHelper("eDH_ReqEntMovementRotZ", "ReqEntMoveRotZ", showEntMovement, -180, +180, -1, +1, 0, 2, 1, 0.5f);
		LayoutHelper("eDH_ReqEntMovementTransX", "ReqEntMoveX", showEntMovement, -10, +10, -0.0f, +0.0f, 1, 2, 1, 0.5f);
		LayoutHelper("eDH_ReqEntMovementTransY", "ReqEntMoveY", showEntMovement, -10, +10, -0.0f, +0.0f, 2, 2, 1, 0.5f);

		LayoutHelper("eDH_EntLocationOriZ", "EntOriZ", showAnimEntOffset || showEntMovement, -360, +360, +360, -360, 0, 2.5f, 1, 0.5f);
		LayoutHelper("eDH_EntLocationPosX", "EntPosX", showAnimEntOffset || showEntMovement, -10000, +10000, +10000, -10000, 1, 2.5f, 1, 0.5f);
		LayoutHelper("eDH_EntLocationPosY", "EntPosY", showAnimEntOffset || showEntMovement, -10000, +10000, +10000, -10000, 2, 2.5f, 1, 0.5f);

		LayoutHelper("eDH_AnimLocationOriZ", "AnimOriZ", showAnimEntOffset || showAnimMovement, -360, +360, +360, -360, 0, 3, 1, 0.5f);
		LayoutHelper("eDH_AnimLocationPosX", "AnimPosX", showAnimEntOffset || showAnimMovement, -10000, +10000, +10000, -10000, 1, 3, 1, 0.5f);
		LayoutHelper("eDH_AnimLocationPosY", "AnimPosY", showAnimEntOffset || showAnimMovement, -10000, +10000, +10000, -10000, 2, 3, 1, 0.5f);

		LayoutHelper("eDH_AnimAssetRotZ", "AssetRotZ", showAnimMovement, -180, +180, -1, +1, 0, 3.5f, 1, 0.5f);
		LayoutHelper("eDH_AnimAssetTransX", "AssetX", showAnimMovement, -5, +5, -0.0f, +0.0f, 1, 3.5f, 1, 0.5f);
		LayoutHelper("eDH_AnimAssetTransY", "AssetY", showAnimMovement, -5, +5, -0.0f, +0.0f, 2, 3.5f, 1, 0.5f);

		LayoutHelper("eDH_AnimEntityOffsetRotZ", "OffsetRotZ", showAnimEntOffset, -180, +180, -1, +1, 3.00f, 3, 0.3f, 0.5f);
		LayoutHelper("eDH_AnimEntityOffsetTransX", "OffsetX", showAnimEntOffset, -5, +5, -0.5f, +0.5f, 3.35f, 3, 0.3f, 0.5f);
		LayoutHelper("eDH_AnimEntityOffsetTransY", "OffsetY", showAnimEntOffset, -5, +5, -0.5f, +0.5f, 3.70f, 3, 0.3f, 0.5f);

		LayoutHelper("eDH_AnimErrorDistance", "AnimErrorDist", showAnimEntOffset, 0, 2, 0, 2, 3, 3.5f, 1, 0.25f);
		LayoutHelper("eDH_AnimErrorAngle", "AnimErrorAngle", showAnimEntOffset, -180, +180, -45, +45, 3, 3.75f, 1, 0.25f);

		LayoutHelper("eDH_AnimTargetCorrectionRotZ",  "AnimTargetRotZ", showAnimTargetMovement, -180, +180, -1, +1, 3, 1.5f, 1, 0.5f);
		LayoutHelper("eDH_AnimTargetCorrectionTransX", "AnimTargetX", showAnimTargetMovement, -5, +5, -0.0f, +0.0f, 4, 1.5f, 1, 0.5f);
		LayoutHelper("eDH_AnimTargetCorrectionTransY", "AnimTargetY", showAnimTargetMovement, -5, +5, -0.0f, +0.0f, 5, 1.5f, 1, 0.5f);

		LayoutHelper("eDH_EntMovementErrorRotZ", "EntMoveErrorRotZ", showEntMovement, -180, +180, -1, +1, 3, 2.0f, 1, 0.5f);
		LayoutHelper("eDH_EntMovementErrorTransX", "EntMoveErrorX", showEntMovement, -5, +5, -0.0f, +0.0f, 4, 2.0f, 1, 0.5f);
		LayoutHelper("eDH_EntMovementErrorTransY", "EntMoveErrorY", showEntMovement, -5, +5, -0.0f, +0.0f, 5, 2.0f, 1, 0.5f);

		LayoutHelper("eDH_EntTeleportMovementRotZ", "EntTeleportRotZ", showEntMovement, -180, +180, -1, +1, 3, 2.5f, 1, 0.5f);
		LayoutHelper("eDH_EntTeleportMovementTransX", "EntTeleportX", showEntMovement, -5, +5, -0.0f, +0.0f, 4, 2.5f, 1, 0.5f);
		LayoutHelper("eDH_EntTeleportMovementTransY", "EntTeleportY", showEntMovement, -5, +5, -0.0f, +0.0f, 5, 2.5f, 1, 0.5f);


		//LayoutHelper("eDH_StateSelection_State", "State", showSelectionParams, 0, LocomotionStates_Count-1, 0, LocomotionStates_Count-1, 0, 2);
		LayoutHelper("eDH_StateSelection_StartTravelSpeed", "StartSpeed", showSelectionParams, 0, 10, 0, 1, 1, 2);
		LayoutHelper("eDH_StateSelection_EndTravelSpeed", "EndSpeed", showSelectionParams, 0, 10, 0, 1, 2, 2);
		LayoutHelper("eDH_StateSelection_TravelDistance", "Distance", showSelectionParams, 0, 10, 0, 1, 3, 2);
		LayoutHelper("eDH_StateSelection_StartTravelAngle", "StartTravelAngle", showSelectionParams, -180, +180, -1, +1, 4, 2);
		LayoutHelper("eDH_StateSelection_EndTravelAngle", "EndTravelAngle", showSelectionParams, -180, +180, -1, +1, 5, 2, 0.5f, 1);
		LayoutHelper("eDH_StateSelection_EndBodyAngle", "EndBodyAngle", showSelectionParams, -180, +180, -1, +1, 5.5f, 2, 0.5f, 1);

		//LayoutHelper("eDH_CarryCorrectionAngle", showFrameTime, "CarryTurnSpeed", 0.0f, 360.0f, 0.0f, 0.0f, 0, 2, 1, 0.5f);
		//LayoutHelper("eDH_CarryCorrectionDistance", showFrameTime, "CarryMoveSpeed", 0.0f, 10.0f, 0.0f, 0.0f, 0, 2.5f, 1, 0.5f);

		LayoutHelper("eDH_TEMP00", "TEMP00", showTemp1, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 5, 0.4f, 1, 0.4f);
		LayoutHelper("eDH_TEMP01", "TEMP01", showTemp1, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 5, 0.8f, 1, 0.4f);
		LayoutHelper("eDH_TEMP02", "TEMP02", showTemp1, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 5, 1.2f, 1, 0.4f);
		LayoutHelper("eDH_TEMP03", "TEMP03", showTemp1, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 5, 1.6f, 1, 0.4f);
		LayoutHelper("eDH_TEMP04", "TEMP04", showTemp2, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 5, 2.0f, 1, 0.4f);
		LayoutHelper("eDH_TEMP05", "TEMP05", showTemp2, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 5, 2.4f, 1, 0.4f);
		LayoutHelper("eDH_TEMP06", "TEMP06", showTemp2, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 5, 2.8f, 1, 0.4f);
		LayoutHelper("eDH_TEMP07", "TEMP07", showTemp2, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 5, 3.2f, 1, 0.4f);

		LayoutHelper("eDH_TEMP10", "TEMP10", showTemp3, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 4, 0.4f, 1, 0.4f);
		LayoutHelper("eDH_TEMP11", "TEMP11", showTemp3, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 4, 0.8f, 1, 0.4f);
		LayoutHelper("eDH_TEMP12", "TEMP12", showTemp3, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 4, 1.2f, 1, 0.4f);
		LayoutHelper("eDH_TEMP13", "TEMP13", showTemp3, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 4, 1.6f, 1, 0.4f);
		LayoutHelper("eDH_TEMP14", "TEMP14", showTemp4, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 4, 2.0f, 1, 0.4f);
		LayoutHelper("eDH_TEMP15", "TEMP15", showTemp4, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 4, 2.4f, 1, 0.4f);
		LayoutHelper("eDH_TEMP16", "TEMP16", showTemp4, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 4, 2.8f, 1, 0.4f);
		LayoutHelper("eDH_TEMP17", "TEMP17", showTemp4, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 4, 3.2f, 1, 0.4f);
	}
	else
	{
		m_debugHistoryManager->Clear();
	}


	/*
	bool showEntityValues = (CAnimationGraphCVars::Get().m_debugEntityParams != 0);
	if(m_pEntityMoveSpeed == NULL)
	{
	m_pEntityMoveSpeed = m_debugHistoryManager->CreateHistory("EntityMoveSpeed");
	m_pEntityMoveSpeed->SetupLayoutAbs(10, 10, 200, 200, 5);
	m_pEntityMoveSpeed->SetupScopeExtent(-360, +360, -1, +1);
	}

	if(m_pEntityPhysSpeed == NULL)
	{
	m_pEntityPhysSpeed = m_debugHistoryManager->CreateHistory("EntityPhysSpeed");
	m_pEntityPhysSpeed->SetupLayoutAbs(220, 10, 200, 200, 5);
	m_pEntityPhysSpeed->SetupScopeExtent(-360, +360, -1, +1);
	}

	if(m_pACRequestSpeed == NULL)
	{
	m_pACRequestSpeed = m_debugHistoryManager->CreateHistory("ACRequestSpeed");
	m_pACRequestSpeed->SetupLayoutAbs(430, 10, 200, 200, 5);
	m_pACRequestSpeed->SetupScopeExtent(-360, +360, -1, +1);
	}
	*/

/*
	LayoutManager.getNode("root").addHorizontal("");
	LayoutManager.getNode("last").add(DH);
	LayoutManager.getNode("last").add(DH);
	LayoutManager.getNode("last").add(DH);
	LayoutManager.getNode("last").add(DH);
	LayoutManager.getNode("root").addHorizontal("middle").addVertical("");
	LayoutManager.getNode("last").add(DH);
	LayoutManager.getNode("last").add(DH);
	LayoutManager.getNode("last").add(DH);
	LayoutManager.getNode("root").addHorizontal("");
	LayoutManager.getNode("last").add(DH);
	LayoutManager.getNode("last").add(DH);
	LayoutManager.getNode("last").add(DH);
	LayoutManager.getNode("last").add(DH);
	LayoutManager.getNode("last").add(DH);
	LayoutManager.getNode("last").add(DH);
	LayoutManager.getNode("middle").addRows(2);
	LayoutManager.getNode("middle.row1").addCols(2);
	LayoutManager.getNode("middle.row1.col0").add(DH);
	DH.setWidth(preferred, unit); // actual pixels, ref pixels, percent of parent
	DH.setHeight(preferred, unit);
	LayoutManager.relax();
*/	
}
#endif

//--------------------------------------------------------------------------------

void CAnimatedCharacter::DebugGraphQT(const QuatT& m, const char* tx, const char* ty, const char* rz, const char* tz /* = UNDEFINED */)
{
#ifndef DEBUGHISTORY
	return;
#else

	if (!DebugFilter())
		return;

	DebugHistory_AddValue(tx, m.t.x);
	DebugHistory_AddValue(ty, m.t.y);
	DebugHistory_AddValue(tz, m.t.z);
	DebugHistory_AddValue(rz, RAD2DEG(m.q.GetRotZ()));
#endif
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::DebugHistory_AddValue(const char* id, float x)
{
#ifndef DEBUGHISTORY
	return;
#else

	if (id == NULL)
		return;

	IDebugHistory* pDH = m_debugHistoryManager->GetHistory(id);
	if (pDH != NULL)
		pDH->AddValue(x);

#endif
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

bool CAnimatedCharacter::DebugFilter() const
{
/*
#ifndef DEBUGHISTORY
	return false;
#else
*/

	const char* entityfilter = CAnimationGraphCVars::Get().m_pDebugFilter->GetString();
	const char* entityname = GetEntity()->GetName();
	if (strcmp(entityfilter, "0") == 0)
		return true;

	return (strcmp(entityfilter, entityname) == 0);

//#endif
}

//--------------------------------------------------------------------------------

bool CAnimatedCharacter::DebugTextEnabled() const
{
#ifdef _DEBUG
	if (CAnimationGraphCVars::Get().m_debugText == 0)
		return false;

	return DebugFilter();
#else
	return false;
#endif
}

//--------------------------------------------------------------------------------

#ifdef _DEBUG
void CAnimatedCharacter::DebugRenderFutureAnimPath(ISkeletonAnim* pSkeletonAnim, float duration)
{
	if ((CAnimationGraphCVars::Get().m_debugFutureAnimPath == 0) && (CAnimationGraphCVars::Get().m_debugMotionParams == 0))
		return;

	CPersistantDebug* pPD = CCryAction::GetCryAction()->GetPersistantDebug();
	if (pPD == NULL)
		return;

	pPD->Begin(UNIQUE("AnimatedCharacter.PrePhysicsStep2.FuturePath"), true);

	Vec3 bump(0,0,0.1f);
	Vec3 prevPos;

	bool allowLoop = (m_pAnimTarget == NULL) || (!m_pAnimTarget->activated);

	pSkeletonAnim->SetFuturePathAnalyser(true);
	for (float dt = 0.0f ; dt < duration; dt += 0.05f)
	{
		AnimTransRotParams amp = pSkeletonAnim->GetBlendedAnimTransRot(dt, !allowLoop);
		if (amp.m_NormalizedTimeAbs != -1)
		{
			Vec3 curPos = m_animLocation.t + m_animLocation.q * amp.m_TransRot.t;
			Quat curRot = m_animLocation.q * amp.m_TransRot.q;
			pPD->AddSphere(curPos + bump, 0.02f, ColorF(1,0,1,1), 0.5f);
			pPD->AddDirection(curPos + bump, 0.05f, curRot * FORWARD_DIRECTION, ColorF(1,0,1,1), 0.5f);

			if (dt > 0.0f)
				pPD->AddLine(curPos + bump, prevPos + bump, ColorF(1,0,1,1), 0.5f);

			prevPos = curPos;
		}
		else
		{
			return;
		}
	}
	pPD->AddSphere(prevPos + bump, 0.05f, ColorF(1,0.5f,1,1), 0.5f);
}
#endif

//--------------------------------------------------------------------------------

// TODO: This is too important and useful to disable even in profile, yet...
void CAnimatedCharacter::DebugRenderCurLocations() const
{
	if (CAnimationGraphCVars::Get().m_debugLocations == 0)
		return;

	CPersistantDebug* pPD = CCryAction::GetCryAction()->GetPersistantDebug();
	if (pPD == NULL)
		return;

	static Vec3 abump(0,0,0.11f);
	static Vec3 ebump(0,0.005f,0.1f);
	static float entDuration = 0.5f;
	static float animDuration = 0.5f;
	static float entTrailDuration = 10.0f;
	static float animTrailDuration = 10.0f;
	static ColorF entColor0(0,0,1,1);
	static ColorF entColor1(0.2f,0.2f,1,1);
	static ColorF animColor0(1,0,0,1);
	static ColorF animColor1(1,0.2f,0.2f,1);
	static ColorF connectColor(1,0,1,0.2f);

	float sideLength = 0.5f;
	float fwdLength = 1.0f;
	float upLength = 4.0f;

	IEntity* pEntity = GetEntity();	assert(pEntity != NULL);
	IPhysicalEntity *pPhysEnt = pEntity->GetPhysics();
	if (pPhysEnt)
	{
		pe_player_dimensions dim;
		if (pPhysEnt->GetParams(&dim) != 0)
		{
			float scaleLength = dim.sizeCollider.GetLength2D();
			sideLength *= scaleLength;
			fwdLength *= scaleLength;
			upLength *= scaleLength;
		}
	}

	pPD->Begin(UNIQUE("AnimatedCharacter.Locations"), true);

	pPD->AddSphere(m_animLocation.t+abump, 0.05f, animColor0, animDuration);
	pPD->AddLine(m_animLocation.t+abump, m_animLocation.t+abump+(m_animLocation.q*Vec3(0,0,upLength)), animColor0, animDuration);
	pPD->AddLine(m_animLocation.t+abump, m_animLocation.t+abump+(m_animLocation.q*Vec3(0,fwdLength,0)), animColor1, animDuration);
	pPD->AddLine(m_animLocation.t+abump+(m_animLocation.q*Vec3(-sideLength,0,0)), m_animLocation.t+abump+(m_animLocation.q*Vec3(+sideLength,0,0)), animColor0, animDuration);

	pPD->AddSphere(m_entLocation.t+ebump, 0.05f, entColor0, entDuration);
	pPD->AddLine(m_entLocation.t+ebump, m_entLocation.t+ebump+(m_animLocation.q*Vec3(0,0,upLength)), entColor0, entDuration);
	pPD->AddLine(m_entLocation.t+ebump, m_entLocation.t+ebump+(m_entLocation.q*Vec3(0,fwdLength,0)), entColor1, entDuration);
	pPD->AddLine(m_entLocation.t+ebump+(m_entLocation.q*Vec3(-sideLength,0,0)), m_entLocation.t+ebump+(m_entLocation.q*Vec3(+sideLength,0,0)), entColor0, entDuration);

	pPD->AddLine(m_animLocation.t+abump, m_entLocation.t+ebump, connectColor, max(entDuration, animDuration));

	pPD->Begin(UNIQUE("AnimatedCharacter.Locations.Trail"), false);

	pPD->AddSphere(m_animLocation.t+abump, 0.01f, animColor0, animTrailDuration);
	pPD->AddSphere(m_entLocation.t+ebump, 0.01f, entColor0, entTrailDuration);

#ifdef _DEBUG
	if (DebugTextEnabled())
	{
		Ang3 EntOri(m_entLocation.q);
		const ColorF cWhite = ColorF(1,1,1,1);
		gEnv->pRenderer->Draw2dLabel(10, 1, 2.0f, (float*)&cWhite, false, 
			"  Ent Location[%.2f, %.2f, %.2f | %.2f, %.2f, %.2f]",
			m_entLocation.t.x, m_entLocation.t.y, m_entLocation.t.z, RAD2DEG(EntOri.x), RAD2DEG(EntOri.y), RAD2DEG(EntOri.z));

		Ang3 AnimOri(m_animLocation.q);
		gEnv->pRenderer->Draw2dLabel(10, 25, 2.0f, (float*)&cWhite, false, 
			"  Anim Location[%.2f, %.2f, %.2f | %.2f, %.2f, %.2f]",
			m_animLocation.t.x, m_animLocation.t.y, m_animLocation.t.z, RAD2DEG(AnimOri.x), RAD2DEG(AnimOri.y), RAD2DEG(AnimOri.z));
	}
#endif
}


//--------------------------------------------------------------------------------

void CAnimatedCharacter::DebugDisplayNewLocationsAndMovements(const QuatT& EntLocation, const QuatT& EntMovement, 
													   const QuatT& AnimLocation, const QuatT& AnimMovement, 
													   float frameTime) const
{
	CPersistantDebug* pPD = CCryAction::GetCryAction()->GetPersistantDebug();
	static float trailTime = 6000.0f;

	if ((pPD != NULL) && (CAnimationGraphCVars::Get().m_debugLocations != 0))
	{
		Vec3 bump(0,0,0.1f);
    bool anim = true;

		pPD->Begin(UNIQUE("AnimatedCharacter.PrePhysicsStep2.new"), true);
    if(anim)
      pPD->AddLine(Vec3(0,0,AnimLocation.t.z)+bump, AnimLocation.t+bump, ColorF(1,0,0,0.5f), 0.5f);
		pPD->AddLine(Vec3(0,0,EntLocation.t.z)+bump, EntLocation.t+bump, ColorF(0,0,1,0.5f), 0.5f);

    if(anim)
    {
      pPD->AddSphere(AnimLocation.t+bump, 0.05f, ColorF(1,0,0,1), 0.5f);
		  pPD->AddDirection(AnimLocation.t+bump, 0.15f, (AnimLocation.q * FORWARD_DIRECTION), ColorF(1,0,0,1), 0.5f);
    }

		pPD->AddSphere(EntLocation.t+bump, 0.05f, ColorF(0,0,1,1), 0.5f);
		pPD->AddDirection(EntLocation.t+bump, 0.15f, (EntLocation.q * FORWARD_DIRECTION), ColorF(0,0,1,1), 0.5f);

		pPD->AddLine(EntLocation.t+bump, EntLocation.t+EntMovement.t*10.0f+bump, ColorF(0,0,1,0.5f), 0.5f);
		pPD->AddLine(EntLocation.t+bump, EntLocation.t+EntMovement.t+bump, ColorF(0.5f,0.5f,1,1.0f), 0.5f);
    if(anim)
    {
      pPD->AddLine(AnimLocation.t+bump, AnimLocation.t+AnimMovement.t*10.0f+bump, ColorF(1,0,0,0.5f), 0.5f);
		  pPD->AddLine(AnimLocation.t+bump, AnimLocation.t+AnimMovement.t+bump, ColorF(1,0.5f,0.5f,1.0f), 0.5f);
    }

		pPD->AddLine(AnimLocation.t+bump, EntLocation.t+bump, ColorF(1,0,1,0.1f), 0.1f);

		pPD->Begin(UNIQUE("AnimatedCharacter.PrePhysicsStep2.trail"), false);

    if(anim)
      pPD->AddSphere(AnimLocation.t+bump, 0.01f, ColorF(1,0,0,0.5f), 5.0f);
		pPD->AddSphere(EntLocation.t+bump, 0.1f, ColorF(0,0,1,0.5f), trailTime);
	}

#ifdef _DEBUG
	if (DebugTextEnabled())
	{
		Ang3 EntOri(EntLocation.q);
		Ang3 EntRot(EntMovement.q);
		const ColorF cWhite = ColorF(1,1,1,1);
		gEnv->pRenderer->Draw2dLabel(10, 100, 2.0f, (float*)&cWhite, false, 
			"N Ent Location[%.2f, %.2f, %.2f | %.2f, %.2f, %.2f]  Movement[%.2f, %.2f, %.2f | %.2f, %.2f, %.2f]",
			EntLocation.t.x, EntLocation.t.y, EntLocation.t.z, RAD2DEG(EntOri.x), RAD2DEG(EntOri.y), RAD2DEG(EntOri.z), 
			EntMovement.t.x / frameTime, EntMovement.t.y / frameTime, EntMovement.t.z / frameTime,
			RAD2DEG(EntRot.x), RAD2DEG(EntRot.y), RAD2DEG(EntRot.z));

		Ang3 AnimOri(AnimLocation.q);
		Ang3 AnimRot(AnimMovement.q);
		gEnv->pRenderer->Draw2dLabel(10, 125, 2.0f, (float*)&cWhite, false, 
			"N Anim Location[%.2f, %.2f, %.2f | %.2f, %.2f, %.2f]  Movement[%.2f, %.2f, %.2f | %.2f, %.2f, %.2f]",
			AnimLocation.t.x, AnimLocation.t.y, AnimLocation.t.z, RAD2DEG(AnimOri.x), RAD2DEG(AnimOri.y), RAD2DEG(AnimOri.z), 
			AnimMovement.t.x / frameTime, AnimMovement.t.y / frameTime, AnimMovement.t.z / frameTime,
			RAD2DEG(AnimRot.x), RAD2DEG(AnimRot.y), RAD2DEG(AnimRot.z));
	}
#endif
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::DebugBlendWeights(const ISkeletonAnim* pSkeletonAnim)
{
/*
	if (pSkeleton == NULL)
		return;

	float BlendWeight0 = 1.0f;
	float BlendWeight1 = 0.0f;
	uint32 numAnimsLayer0 = pSkeleton->GetNumAnimsInFIFO(0);
	if (numAnimsLayer0 == 1)
	{
		CAnimation animationSteady;
		animationSteady = pSkeleton->GetAnimFromFIFO(0,0);
		BlendWeight0 = 1.0f;
	}
	else if (numAnimsLayer0 == 2)
	{
		CAnimation animationBlendIn;
		CAnimation animationBlendOut;
		animationBlendOut = pSkeleton->GetAnimFromFIFO(0,0);
		BlendWeight0 = animationBlendOut.m_fTransitionBlend;
		animationBlendIn = pSkeleton->GetAnimFromFIFO(0,1);
		BlendWeight1 = animationBlendIn.m_fTransitionBlend;
	}
*/
}

//--------------------------------------------------------------------------------

#ifdef _DEBUG
void CAnimatedCharacter::DebugAnimEntDeviation(const QuatT& offset, float distance, float angle, float distanceMax, float angleMax)
{
	if (CAnimationGraphCVars::Get().m_debugAnimError == 0)
	{
		// release it
		if (m_pAnimErrorClampStats != NULL)
		{
			delete m_pAnimErrorClampStats;
			m_pAnimErrorClampStats = NULL;
		}

		return;
	}

	DebugHistory_AddValue("eDH_AnimErrorDistance", distance);
	DebugHistory_AddValue("eDH_AnimErrorAngle", angle);

	// let's create one if we don't have it already
	if (m_pAnimErrorClampStats == 0)
	{
		m_pAnimErrorClampStats = new SAnimErrorClampStats();
	}

	m_pAnimErrorClampStats->Update(m_curFrameTime, distance, angle, distanceMax, angleMax);

	float screenw = (float)gEnv->pRenderer->GetWidth();
	float screenh = (float)gEnv->pRenderer->GetHeight();
	float refscreenw = 1024.0f;
	float refscreenh = 768.0f;
	float scalex = screenw / refscreenw;
	float scaley = screenh / refscreenh;

	{
		static float textx = 690.0f;
		static float texty = 720.0f;
		static float texth = 12.0f;
		static float textscale = 1.0f;
		const ColorF cWhite = ColorF(1,1,1,1);
		gEnv->pRenderer->Draw2dLabel(textx*scalex, texty*scaley, textscale, (float*)&cWhite, false, 
			"AnimErrorClamp Distance(max %3.2f) [%3.1f%% | cur %3.2f, worse %3.2f, avg %3.2f ]",
			distanceMax, 
			m_pAnimErrorClampStats->m_distanceClampDuration / m_pAnimErrorClampStats->m_elapsedTime * 100.0f,
			distance,
			m_pAnimErrorClampStats->m_distanceMax, m_pAnimErrorClampStats->m_distanceAvg);
		gEnv->pRenderer->Draw2dLabel(textx*scalex, (texty+texth)*scaley, textscale, (float*)&cWhite, false, 
			"AnimErrorClamp Angle(max %3.2f)   [%3.1f%% | cur %3.2f, worse %3.2f, avg %3.2f ]",
			angleMax, 
			m_pAnimErrorClampStats->m_angleClampDuration / m_pAnimErrorClampStats->m_elapsedTime * 100.0f,
			angle,
			m_pAnimErrorClampStats->m_angleMax, m_pAnimErrorClampStats->m_angleAvg);
	}

	//{
	gEnv->pRenderer->SetMaterialColor(1, 1, 1, 1);

	IRenderAuxGeom* pAux = gEnv->pRenderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags flags = pAux->GetRenderFlags();
	flags.SetMode2D3DFlag(e_Mode2D);
	pAux->SetRenderFlags(flags);

	static float c[4] = { 1,1,0,1 };
	static float aechx = 700.0f;
	static float aechy = 700.0f;

	for (int i = 0; i < SAnimErrorClampStats::ms_histogramSize; i++)
	{
		if ((distance >= m_pAnimErrorClampStats->m_distanceHistogramMin[i]) &&
			((i == (SAnimErrorClampStats::ms_histogramSize-1)) || 
			(distance < m_pAnimErrorClampStats->m_distanceHistogramMin[i+1])))
		{
			m_pAnimErrorClampStats->m_distanceHistogramTime[i] += m_curFrameTime;
		}

		static float w = 200.0f;
		static float h = 100.0f;
		float x = w * (float)(i) / (float)(SAnimErrorClampStats::ms_histogramSize-1);
		float y = h * (m_pAnimErrorClampStats->m_distanceHistogramTime[i] / m_pAnimErrorClampStats->m_elapsedTime);
		gEnv->pRenderer->Draw2dLabel(aechx*scalex + x, aechy*scaley, 1.0f, c, true, "%2.2f" , m_pAnimErrorClampStats->m_distanceHistogramMin[i]);
		for (float f = 0.0f; f < 1.0f; f+=0.1f)
		{
			Draw2DLine(aechx*scalex + x+5 + f*10, aechy*scaley-10, aechx*scalex + x+5 + f*10, aechy*scaley-10 - y, ColorF(1, 1, 0, 1));
		}
	}
	//}

	CPersistantDebug* pPD = CCryAction::GetCryAction()->GetPersistantDebug();
	if (pPD != NULL)
	{
		pPD->Begin(UNIQUE("AnimatedCharacter.PrePhysicsStep2.AnimClamp"), false);

		Vec3 bump(0,0,0.1f);

		if ((distance > distanceMax) && (distanceMax > 0.0f))
			DebugRenderDistanceMeasure(pPD, m_entLocation.t+bump, offset.t, distance / distanceMax);

		if ((angle > angleMax) && (angleMax > 0.0f))
			DebugRenderAngleMeasure(pPD, m_entLocation.t+bump, m_entLocation.q, offset.q, angle / angleMax);
	}
}
#endif

//--------------------------------------------------------------------------------

#ifdef _DEBUG
void DebugRenderDistanceMeasure(CPersistantDebug* pPD, const Vec3& origin, const Vec3& offset, float fraction)
{
	float len = offset.GetLength();
	if (len == 0.0f)
		return;

	Vec3 dir = offset / len;
	float vertical = abs(dir | Vec3(0, 0, 1));

	Vec3 rgt = LERP((dir % Vec3(0,0,1)), Vec3(1,0,0), vertical);
	Vec3 up = dir % rgt;
	rgt = up % dir;

	static ColorF colorInside(0, 1, 1, 0.5f);
	static ColorF colorOutside(1, 1, 0, 1);
	ColorF color = (fraction < 1.0f) ? colorInside : colorOutside;

	static float crossSize = 0.05f;

	pPD->AddLine(origin, origin+offset, color, 1.0f);
	pPD->AddLine(origin-rgt*crossSize, origin+rgt*crossSize, color, 1.0f);
	pPD->AddLine(origin-up*crossSize, origin+up*crossSize, color, 1.0f);
	pPD->AddLine(origin+offset-rgt*crossSize, origin+offset+rgt*crossSize, color, 1.0f);
	pPD->AddLine(origin+offset-up*crossSize, origin+offset+up*crossSize, color, 1.0f);
}
#endif

//--------------------------------------------------------------------------------

#ifdef _DEBUG
void DebugRenderAngleMeasure(CPersistantDebug* pPD, const Vec3& origin, const Quat& orientation, const Quat& offset, float fraction)
{
	Vec3 baseDir = orientation * FORWARD_DIRECTION;
	Vec3 clampedDir = orientation*offset * FORWARD_DIRECTION;

	static ColorF colorInside(0, 1, 1, 0.5f);
	static ColorF colorOutside(1, 1, 0, 1);
	ColorF color = (fraction < 1.0f) ? colorInside : colorOutside;

	static float armSize = 1.0f;
	pPD->AddLine(origin, origin+baseDir*(armSize*0.5f), color, 1.0f);
	pPD->AddLine(origin, origin+clampedDir*armSize, color, 1.0f);
	pPD->AddLine(origin+baseDir*(armSize*0.5f), origin+clampedDir*(armSize*0.5f), color, 1.0f);
}
#endif

//--------------------------------------------------------------------------------

QuatT GetDebugEntityLocation(const char* name, const QuatT& _default)
{
#ifdef _DEBUG
	IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(name);
	if (pEntity != NULL)
		return QuatT(pEntity->GetWorldPos(), pEntity->GetWorldRotation());
#endif

	return _default;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

#ifdef _DEBUG
void RunExtractCombineTest(bool flat)
{
	int errors = 0;

	for (int i = 0; i < 1000; i++)
	{
		QuatT qt1;
		qt1.t = Vec3(0.0f); //Random(Vec3(-100, -100, -10), Vec3(100, 100, 10));
		Vec3 rgt0(1, 0, 0), fwd0(0, 1, 0), up0(0, 0, 1);
		fwd0 = Random(Vec3(-100, -100, 0), Vec3(100, 100, 0)).GetNormalizedSafe(fwd0);

		if (!flat)
			up0 = Random(Vec3(-10, -10, 100), Vec3(10, 10, 100)).GetNormalizedSafe(up0);

		up0 = Vec3(0, 0, 1);
		rgt0 = (fwd0 % up0).GetNormalizedSafe(rgt0);
		up0 = (rgt0 % fwd0).GetNormalizedSafe(up0);
		qt1.q = Quat(Matrix33::CreateFromVectors(rgt0, fwd0, up0));

		Vec3 rgt1 = qt1.q * Vec3(1, 0, 0);
		Vec3 fwd1 = qt1.q * Vec3(0, 1, 0);
		Vec3 up1 = qt1.q * Vec3(0, 0, 1);

		QuatT qth = ExtractHComponent(qt1);
		QuatT qtv = ExtractVComponent(qt1);
		QuatT qt2;
		
		if (flat)
			qt2 = CombineHVComponents2D(qth, qtv);
		else
			qt2 = CombineHVComponents2D(qth, qtv);

		QuatT hybrid; hybrid.SetNLerp(qt1, qt2, 0.5f);
		bool c1 = !qt1.IsEquivalent(qt2);
		bool c2 = !hybrid.IsEquivalent(qt1);
		bool c3 = !hybrid.IsEquivalent(qt2);

		if (c1 || c2 || c3)
		{
/*
#if defined(DEBUG_BREAK) && defined(USER_david)
			DEBUG_BREAK;
#endif
*/
			errors++;

			Vec3 rgt2 = qt2.q * Vec3(1, 0, 0);
			Vec3 fwd2 = qt2.q * Vec3(0, 1, 0);
			Vec3 up2 = qt2.q * Vec3(0, 0, 1);

			Vec3 rgtH = qth.q * Vec3(1, 0, 0);
			Vec3 fwdH = qth.q * Vec3(0, 1, 0);
			Vec3 upH = qth.q * Vec3(0, 0, 1);

			Vec3 rgtV = qtv.q * Vec3(1, 0, 0);
			Vec3 fwdV = qtv.q * Vec3(0, 1, 0);
			Vec3 upV = qtv.q * Vec3(0, 0, 1);
		}
	}

	assert(errors == 0);
}
#endif

//--------------------------------------------------------------------------------

#ifdef _DEBUG
void RunQuatConcatTest()
{
	int validConcatCountWorst = 10000;
	int validConcatCountBest = 0;
	bool allValid = true;

	for (int i = 0; i < 100; i++)
	{
		Quat o(IDENTITY);

		for (int j = 0; j < 1000; j++)
		{
			if (!o.IsValid())
			{
#if defined(DEBUG_BREAK) && defined(USER_david)
				DEBUG_BREAK;
#endif

				validConcatCountWorst = min(j, validConcatCountWorst);
				validConcatCountBest = max(j, validConcatCountBest);
				allValid = false;
				break;
			}

			Quat q;
/*
			Vec3 rgt(1, 0, 0), fwd(0, 1, 0), up(0, 0, 1);
			fwd = Random(Vec3(-100, -100, -100), Vec3(100, 100, 100)).GetNormalizedSafe(fwd);
			up = Random(Vec3(-100, -100, 100), Vec3(100, 100, 100)).GetNormalizedSafe(up);
			rgt = (fwd % up).GetNormalizedSafe(rgt);
			up = (rgt % fwd).GetNormalizedSafe(up);
			q = Quat(Matrix33::CreateFromVectors(rgt, fwd, up));
*/
			q.v = Random(Vec3(-1, -1, -1), Vec3(1, 1, 1));
			q.w = Random(-1, 1);
			q.Normalize();
			assert(q.IsValid());

			Quat qInv = q.GetInverted();
			assert(qInv.IsValid());

			o = o * qInv;
		}
	}

	if (!allValid)
		CryLog("RunQuatConcatTest: valid consecutive concatenations - worst case %d, best case %d.", validConcatCountWorst, validConcatCountBest);
}
#endif

//--------------------------------------------------------------------------------

#ifdef _DEBUG
void RunQuatPredicateTest()
{
	int errors = 0;

	for (int i = 0; i < 1000; i++)
	{
		Quat q(Random(), 0.0f, 0.0f, Random()); q.Normalize();

		Vec3 fwd = q.GetColumn1();
		float rz1 = atan2(fwd.y, fwd.x);
		float rz2 = atan2(-fwd.x, fwd.y);
		float rz3 = q.GetRotZ();

		if (fabs(rz2 - rz3) > 0.0001f)
		{
#if defined(DEBUG_BREAK) && defined(USER_david)
			DEBUG_BREAK;
#endif

			errors++;
		}
	}

	assert(errors == 0);
}
#endif

//--------------------------------------------------------------------------------

void CAnimatedCharacter::RunTests()
{
#ifndef _DEBUG
	return;
#else

	static bool m_bDebugRunTests = true;

	if (!m_bDebugRunTests)
		return;

	m_bDebugRunTests = false;

	RunExtractCombineTest(true);
	RunExtractCombineTest(false);
	RunQuatConcatTest();
	RunQuatPredicateTest();

#endif
}

//--------------------------------------------------------------------------------

