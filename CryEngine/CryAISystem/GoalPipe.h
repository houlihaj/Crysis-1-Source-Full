// GoalPipe.h: interface for the CGoalPipe class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_GOALPIPE_H__12BD0344_3B3F_4B55_8500_25581ECF7ACC__INCLUDED_)
#define AFX_GOALPIPE_H__12BD0344_3B3F_4B55_8500_25581ECF7ACC__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "IAgent.h"
#include <vector>
#include "pointer_container.h"
#include "GoalOp.h"


class CAISystem;

typedef pointer_container<CGoalOp> GoalPointer;

struct QGoal
{
	GoalPointer pGoalOp;	// if pGoalOp is null, this is goal is actually a subpipe
	EGoalOperations op;
	string pipeName;			// The name of the possible pipe.
	GoalParameters params;
	bool bBlocking;
	IGoalPipe::EGroupType eGrouping;

	QGoal()
	{
		pGoalOp = 0;
		bBlocking = false;
		eGrouping = IGoalPipe::GT_NOGROUP;
	}
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
	inline bool operator==(const QGoal &other) const
	{
		return pGoalOp == other.pGoalOp;
	}
};

typedef std::vector<QGoal> VectorOGoals;

typedef std::map<string,int> LabelsMap;

enum EPopGoalResult
{
	ePGR_AtEnd,
	ePGR_Succeed,
	ePGR_BreakLoop,
};

/*! This class defines a logical set of actions that an agent performs in succession.
*/
class CGoalPipe : public IGoalPipe
{
	VectorOGoals	m_qGoalPipe;
	LabelsMap		m_Labels;
	
	unsigned int	m_nPosition;	// position in pipe
	CGoalPipe		*m_pSubPipe;
	bool			m_bLoop;
	bool			m_bDynamic;		// the pipe was created after AISystem initialization
	int				m_nCurrentBlockCounter;	//to be used for WAIT goalOp initialization
	EGoalOpResult m_lastResult;

public:
	int				m_nEventId;
	bool			m_bHighPriority;
	Vec3			m_vAttTargetPosAtStart;	// position of owner's attention target when pipe is selected OR new target is set
	string		m_sName;		// name of this pipe
	string		m_sDebugName;	// debug name of this pipe
	CAIObject	*m_pArgument;

	void SetLastResult(EGoalOpResult res)
	{
		if (m_pSubPipe)
			m_pSubPipe->SetLastResult(res);
		m_lastResult = res;
	}

	EGoalOpResult GetLastResult() const
	{
		if (m_pSubPipe)
			return m_pSubPipe->GetLastResult();
		return m_lastResult;
	}

	void Reset();
	CGoalPipe * Clone();
	CGoalPipe(const string &name, CAISystem *pAISystem, bool bDynamic=false);
	virtual ~CGoalPipe();

	const bool IsDynamic() const {return m_bDynamic;}
	// IGoalPipe
	virtual void PushGoal(EGoalOperations name, bool bBlocking, EGroupType eGrouping, GoalParameters &params);
	virtual void PushPipe(const char* szName, bool bBlocking, EGroupType eGrouping, GoalParameters &params);
	virtual void PushLabel(const char* label);
	virtual void SetDebugName(const char* name);
	virtual void HighPriority() { m_bHighPriority = true; }
	virtual const char* GetName() const { return m_sName; }
	virtual EGoalOperations GetGoalOpEnum(const char* szName);
	virtual const char* GetGoalOpName(EGoalOperations op);

//	GoalPointer PopGoal(bool &blocking, string &name, GoalParameters &params,CPipeUser *pOperand);
	EPopGoalResult PopGoal(QGoal& theGoal,CPipeUser *pOperand);

	// Makes the IP of this pipe jump to the desired position
	bool Jump(int position);
// TODO: cut the string version of Jump in a few weeks
	bool Jump(const char* label);
	void ReExecuteGroup(); // Does Jump(-1) or more to start from the beginning of current group
  bool IsInSubpipe() const {return m_pSubPipe != 0;}
  CGoalPipe* GetSubpipe() {return m_pSubPipe;}
	const CGoalPipe* GetSubpipe() const { return m_pSubPipe; }
  CGoalPipe* GetLastSubpipe() {return GetSubpipe() ? GetSubpipe()->GetLastSubpipe() : this;}
  const CGoalPipe* GetLastSubpipe() const {return GetSubpipe() ? GetSubpipe()->GetLastSubpipe() : this;}
  int CountSubpipes() const {	return GetSubpipe() ? GetSubpipe()->CountSubpipes()+1 : 0;}
	void SetSubpipe(CGoalPipe * pPipe);
	size_t MemStats();
	int GetPosition() { return m_nPosition;}
	void SetPosition(int iNewPos) { if ((iNewPos>0) && ((unsigned int) iNewPos<(m_qGoalPipe.size()))) m_nPosition=iNewPos;}
	inline void SetLoop(bool bLoop) {m_bLoop=bLoop;};
	inline bool IsLoop() {return m_bLoop;};
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker, VectorOGoals &activeGoals);
	void SerializeDynamic(TSerialize ser);
	bool RemoveSubPipe( int& goalPipeId, bool keepInserted, bool keepHigherPriority );
};

#endif // !defined(AFX_GOALPIPE_H__12BD0344_3B3F_4B55_8500_25581ECF7ACC__INCLUDED_)
