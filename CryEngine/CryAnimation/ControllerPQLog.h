/////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Crytek Character Animation source code
//	
//	History:
//	Created by Ivo Herzeg
//	
//  Notes:
//    CControllerPQLog class declaration
//    CControllerPQLog is implementation of IController which is compatible with
//    the old (before 6/27/02) caf file format that contained only CryBoneKey keys.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _CRYTEK_CONTROLLER_PQLOG_
#define _CRYTEK_CONTROLLER_PQLOG_

#include "CGFContent.h"




// old motion format cry bone controller
class CControllerPQLog: public IController
{
public:

	CControllerPQLog();
	~CControllerPQLog();

	uint32 numKeys() const
	{
		return m_arrKeys.size();
	}

	virtual EControllerInfo GetControllerType() 
	{
		return ePQLog;
	}


	uint32 GetID () const {return m_nControllerId;}

	QuatT DecodeKey(int GAID, f32 normalized_time);

	f32 NTime2KTime(int32 GAID,f32 ntime);
	Status4 GetOPS (int GAID, f32 normalized_time, Quat& quat, Vec3& pos, Diag33& scale);
	Status4 GetOP (int GAID, f32 normalized_time, Quat& quat, Vec3& pos);
	uint32 GetO (int GAID, f32 normalized_time, Quat& quat);
	uint32 GetP (int GAID, f32 normalized_time, Vec3& pos);
	uint32 GetS (int GAID, f32 normalized_time, Diag33& pos);

	QuatT GetValueByKey(uint32 key);

	QuatT GetKey0()
	{
		uint32 numKey = numKeys();
		assert(numKey);
		PQLog pq = m_arrKeys[0];
		return QuatT(!exp(pq.vRotLog),pq.vPos);
	}


	Quat GetOrientationByKey (uint32 key)
	{
		assert(key<9);
		QuatT pq = GetValueByKey(key);
		return pq.q;
	}










	// returns the start time
	f32 GetTimeStart ()
	{
		return f32(m_arrTimes[0]);
	}

	// returns the end time
	f32 GetTimeEnd()
	{
		assert (numKeys() > 0);
		return f32(m_arrTimes[numKeys()-1]);
	}


	size_t SizeOfThis ()const;


	CInfo GetControllerInfo() const
	{
		CInfo info;
		info.numKeys = m_arrKeys.size();
		info.quat	   = !exp(m_arrKeys[info.numKeys-1].vRotLog);
		info.pos		 = m_arrKeys[info.numKeys-1].vPos;
		info.stime		= m_arrTimes[0];
		info.etime		= m_arrTimes[info.numKeys-1];
		info.realkeys =	(info.etime-info.stime)/TICKS_PER_FRAME+1;
		return info;
	}


	void SetControllerData( const DynArray<PQLog>& arrKeys, const DynArray<int>& arrTimes )
	{
		m_arrKeys	=arrKeys;
		m_arrTimes=arrTimes;
	}

	size_t GetRotationKeysNum() 
	{
		return 0;
	}

	size_t GetPositionKeysNum()
	{
		return 0;
	}

//--------------------------------------------------------------------------------------------------

//protected:
	DynArray<PQLog> m_arrKeys;
	DynArray<int> m_arrTimes;

	float m_lastTime;
	QuatT m_lastValue;

	float m_lastTimeLM;
	QuatT m_lastValueLM;

	unsigned m_nControllerId;
};


TYPEDEF_AUTOPTR(CControllerPQLog);

#endif

