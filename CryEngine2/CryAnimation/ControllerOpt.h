////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   controllerpq.h
//  Version:     v1.00
//  Created:     14/01/2007 by Alexey.
//  Compilers:   Visual Studio.NET 2005
//  Description: Optimized Quat\pos controller implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef _CRYTEK_CONTROLLER_OPT_PQ_
#define _CRYTEK_CONTROLLER_OPT_PQ_
#pragma once


#include "CGFContent.h"
#include "QuatQuantization.h"
#include "ControllerPQ.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline uint32 ControllerHash(uint32 a, uint32 b, uint32 c, uint32 d)
{
	return (a & 0xFF) + ((b & 0xFF) << 8) + ((c & 0xFF) << 16) + ((d & 0xFF) << 24);
}

class CEmptyKeyTimesData
{
public:

	f32 GetKeyValueFloat(uint32 key) const
	{
		return 0.0f;
	}

	uint32	GetKey(int GAID, f32 normalized_time, f32& difference_time)
	{
		// you should not to be here!
		return 0;
	}

	int GetFormat()
	{
		return eNoFormat;
	}

	int GetNumKeys()
	{
		return 0;
	}

	// set number
	void SetNumKeys(uint32 keys)
	{

	}

	// set pointer to data
	void SetData(char * pData)
	{

	}

	char * GetData()
	{
		return 0;
	}

	ILINE void SetKeyTimes(uint32 num, char * pData)
	{

	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


template<class _Encoder>
struct CKeyTimesData
{

	typedef typename _Encoder::TKeyTypeTraits TData;
	typedef typename _Encoder::TKeyTypeCountTraits TCount;

public:
	CKeyTimesData() : m_iCount(0), m_pData(0), m_lastKey(-1), m_lastTime(-1) {};

	ILINE void SetKeyTimeNumKeys(uint32 i)
	{
		m_iCount = static_cast<TCount>(i);
	}

	ILINE void SetKeyTimeData(char * pData)
	{
		m_pData = static_cast<TData*>(pData);
	}

	ILINE void SetKeyTimes(uint32 num, char * pData)
	{
		m_iCount = static_cast<TCount>(num);
		m_pData = (TData*)(pData);
	}

	char * GetData()
	{
		return (char*)(m_pData);
	}

protected:

	static uint32 GetKeyTimeFormat()
	{
		return _Encoder::GetFormat();
	}

	ILINE TData GetKeyValueFloat(uint32 key)
	{
		return m_pData[key];
	}

	ILINE TData GetValueSafe(uint32 key)
	{
		if (key < m_iCount)
		{
			return m_pData[key];
		}

		return TData(0);
	}

	ILINE uint32 GetKeyTimeNumKeys()
	{
		return m_iCount;
	}

	uint32	GetKey(int GAID, f32 normalized_time, f32& difference_time)
	{
		f32 realtime = ControllerHelper::NTime2KTime(GAID,normalized_time);

		if (realtime == m_lastTime)
		{
			difference_time =  m_LastDifferenceTime;
			return m_lastKey;
		}
		m_lastTime = realtime;

		uint32 numKey = (uint32)m_iCount;//m_arrKeys.size();

		TData keytime_start = m_pData[0];
		TData keytime_end = m_pData[numKey-1];

		f32 test_end = keytime_end;
		if( realtime < keytime_start )
			test_end += realtime;

		if( realtime < keytime_start )
		{
			m_lastKey = 0;
			return 0;
		}

		if( realtime >= keytime_end )
		{
			m_lastKey = numKey;
			return numKey;
		}

		int nPos  = numKey>>1;
		int nStep = numKey>>2;
		// use binary search
		//TODO: Need check efficiency of []operator. Maybe wise use pointer
		while(nStep)
		{
			if(realtime < m_pData[nPos])
				nPos = nPos - nStep;
			else
				if(realtime > m_pData[nPos])
					nPos = nPos + nStep;
				else 
					break;

			nStep = nStep>>1;
		}

		// fine-tuning needed since time is not linear
		while(realtime > m_pData[nPos])
			nPos++;

		while(realtime < m_pData[nPos-1])
			nPos--;

		m_lastKey = nPos;
		// possible error if encoder uses nonlinear methods!!!
		m_LastDifferenceTime = difference_time = (f32)(realtime-(f32)m_pData[nPos-1]) / ((f32)m_pData[nPos] - (f32)m_pData[nPos-1]);

		return nPos;
	}

	ILINE uint32 GetNumKeys()
	{
		return (uint32)m_iCount;
	}

private:
	f32 m_lastTime;
	f32 m_LastDifferenceTime;
	uint32 m_lastKey;

	TCount m_iCount;
	TData  *m_pData;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef CKeyTimesData<ByteEncoder> CKeyTimesByte;
typedef CKeyTimesData<UINT16Encoder> CKeyTimesUINT16;
typedef CKeyTimesData<F32Encoder> CKeyTimesF32;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


template<class _Encoder>
struct CKeyTimesDataBitSet
{
	typedef typename _Encoder::TKeyTypeTraits TData;
	typedef typename _Encoder::TKeyTypeCountTraits TCount;
public:
	CKeyTimesDataBitSet() : m_pData(0), m_lastKey(-1), m_lastTime(-1) {};

	/*
	ILINE void SetKeyTimeNumKeys(uint32 i)
	{
	m_iCount = static_cast<TCount>(i);
	}
	*/
	ILINE void SetKeyTimeData(char * pData)
	{
		m_pData = static_cast<TData*>(pData);
	}

	ILINE void SetKeyTimes(uint32 num, char * pData)
	{
		//		m_iCount = static_cast<TCount>(num);
		m_pData = (TData*)(pData);
	}

	char * GetData()
	{
		return (char*)(m_pData);
	}

protected:

	static uint32 GetKeyTimeFormat()
	{
		return _Encoder::GetFormat();
	}

	ILINE TData GetKeyValueFloat(uint32 key)
	{
		if (key == 0) {
			// first one
			return (float)GetHeader()->m_Start;
		}
		else
			if ( key >= GetNumKeys() - 1) {
				// last one
				return (float)GetHeader()->m_End;
			}
			// worse situation
			int c(0);

			int keys = GetNumKeys();
			int count(0);
			for (int i = 0; i < keys; ++i) {
				uint16 bits = GetKeyData(i);

				for (int j = 0; j < 16; ++j) {
					if ((bits >> j) & 1) {
						++count;
						if ((count - 1) == key)
							return (float)(i * 16 + j);
					}
				}
			}

			return 0;

	}
	/*
	ILINE TData GetValueSafe(uint32 key)
	{
	if (key < m_iCount)
	{
	return m_pData[key];
	}

	return TData(0);
	}


	ILINE uint32 GetKeyTimeNumKeys()
	{
	return m_iCount;
	}
	*/

	uint32	GetKey(int GAID, f32 normalized_time, f32& difference_time)
	{
		f32 realtime = ControllerHelper::NTime2KTime(GAID,normalized_time);

		if (realtime == m_lastTime) {
			difference_time =  m_LastDifferenceTime;
			return m_lastKey;
		}

		m_lastTime = realtime;
		uint32 numKey = (uint32)GetHeader()->m_Size;//m_arrKeys.size();

		f32 keytime_start = (float)GetHeader()->m_Start;
		f32 keytime_end = (float)GetHeader()->m_End;
		f32 test_end = keytime_end;

		if( realtime < keytime_start )
			test_end += realtime;

		if( realtime < keytime_start ) {
			difference_time = 0;
			m_lastKey = 0;
			return 0;
		}

		if( realtime >= keytime_end ) {
			difference_time = 0;
			m_lastKey = numKey;
			return numKey;
		}

		f32 internalTime = realtime - keytime_start;
		uint16 uTime = (uint16)internalTime;
		uint16 piece = (uTime / sizeof(uint16)) >> 3;
		uint16 bit = /*15 - */(uTime % 16);
		uint16 data = GetKeyData(piece);
		uint16 left = data >> bit;

		//left
		left = data & (0xFFFF >> (15 - bit));
		uint16 leftPiece(piece);
		uint16 nearestLeft=0;
		uint16 wBit;
		while ((wBit = GetFirstHighBit(left)) == 16) {
			--leftPiece;
			left = GetKeyData(leftPiece);
		}
		nearestLeft = leftPiece * 16 + wBit;

		//right
		uint16 right = ((data >> (bit + 1)) & 0xFFFF) << (bit + 1);
		uint16 rigthPiece(piece);
		uint16 nearestRight=0;
		while ((wBit = GetFirstLowBit(right)) == 16) {
			++rigthPiece;
			right = GetKeyData(rigthPiece);
		}

		nearestRight = ((rigthPiece  * sizeof(uint16)) << 3) + wBit;
		m_LastDifferenceTime = difference_time = (f32)(internalTime-(f32)nearestLeft) / ((f32)nearestRight - (f32)nearestLeft);

		// count nPos
		uint32 nPos(0);
		for (uint16 i = 0; i < rigthPiece; ++i) {
			uint16 data = GetKeyData(i);
			nPos += ControllerHelper::m_byteTable[data & 255] + ControllerHelper::m_byteTable[data >> 8];
		}

		data = GetKeyData(rigthPiece);
		data = ((data <<  (15 - wBit)) & 0xFFFF) >> (15 - wBit);
		nPos += ControllerHelper::m_byteTable[data & 255] + ControllerHelper::m_byteTable[data >> 8];
		m_lastKey = nPos - 1;

		return m_lastKey;
	}

	ILINE uint32 GetNumKeys()
	{
		return GetHeader()->m_Size; 
	}

private:

	struct Header 
	{
		uint16		m_Start;
		uint16    m_End;
		uint16		m_Size;
	};

	inline Header* GetHeader() const
	{
		return (Header*)(&m_pData[0]);
	};

	inline uint16 GetKeyData(int i) const
	{
		return m_pData[3 + i];
	};

	f32 m_lastTime;
	f32 m_LastDifferenceTime;
	uint32 m_lastKey;

	TData  *m_pData;
	//	TCount m_iCount;

	//private:
	//	static byte m_bTable[256];
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef CKeyTimesDataBitSet<BitsetEncoder> CKeyTimesBitset;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class KeyTimeClass>
class CEmptyRotationData : public KeyTimeClass
{
public:

	uint32 GetRotationNumCount()
	{
		return 0;	
	}

	uint32 GetRotationValue(int GAID, f32 normalized_time, Quat& q)
	{

		return 0;
	}

	void GetRotationValueFromKey(uint32 key, Quat&)
	{

	}

	void SetRotationData(uint32 num, char * m_pData)
	{

	}

	char * GetRotationData()
	{
		return 0;
	}

	char * GetRotKeyTimes()
	{
		return 0;
	}

	void SetRotKeyTimes(uint32 num, char * pData)
	{
	}

	static uint32 GetRotationKeyTimeFormat()
	{
		return eNoFormat;
	}

	static uint32 GetRotationType()
	{
		return eNoFormat;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef CEmptyRotationData<CEmptyKeyTimesData> CEmptyRotationController;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct QuatLerpNoCompression
{
	static inline void Blend(Quat& res, const Quat& val1, const Quat& val2, f32 t)
	{
		res.SetNlerp(val1, val2, t);
	}

	static uint32 GetType()
	{
		return eNoCompressQuat;
	}

	typedef NoCompressQuat TDataTypeTraits;
	typedef uint16 TDataTypeCountTraits;
};

struct QuatLerpSmallTree48BitQuat
{
	static inline void Blend(Quat& res, const Quat& val1, const Quat& val2, f32 t)
	{
		res.SetNlerp(val1, val2, t);
	}

	static uint32 GetType()
	{
		return eSmallTree48BitQuat;
	}

	typedef SmallTree48BitQuat TDataTypeTraits;
	typedef uint16 TDataTypeCountTraits;
};

struct QuatLerpSmallTree64BitQuat
{
	static inline void Blend(Quat& res, const Quat& val1, const Quat& val2, f32 t)
	{
		res.SetNlerp(val1, val2, t);
	}

	static uint32 GetType()
	{
		return eSmallTree64BitQuat;
	}

	typedef SmallTree64BitQuat TDataTypeTraits;
	typedef uint16 TDataTypeCountTraits;
};

struct QuatLerpSmallTree64BitExtQuat
{
	static inline void Blend(Quat& res, const Quat& val1, const Quat& val2, f32 t)
	{
		res.SetNlerp(val1, val2, t);
	}
	static uint32 GetType()
	{
		return eSmallTree64BitExtQuat;
	}

	typedef SmallTree64BitExtQuat TDataTypeTraits;
	typedef uint16 TDataTypeCountTraits;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class KeyTimeClass, class _Interpolator>
class CRotationData : public KeyTimeClass
{
public:

	typedef typename _Interpolator::TDataTypeTraits T;
	typedef typename _Interpolator::TDataTypeCountTraits C;

	ILINE uint32 GetRotationNumCount()
	{
		return this->GetNumKeys();
	}

	ILINE uint32 GetRotationValue(int GAID, f32 normalized_time, Quat& q)
	{
		q = GetValue(GAID, normalized_time);
		return STATUS_O;
	}

	ILINE void GetRotationValueFromKey(uint32 key, Quat& t)
	{
		uint32 num = GetRotationNumCount();
		if (key >= num)
			key = num - 1;

		GetValueFromKey(key, t);
	}

	ILINE void SetRotationData(uint32 num, char * m_pData)
	{
		//		m_iCount = num;
		assert(num == this->GetNumKeys());
		m_arrRots = (T*)m_pData;
	}

	char * GetRotationData()
	{
		return (char *)m_arrRots;
	}

	static uint32 GetRotationType()
	{
		return _Interpolator::GetType();
	}

	static uint32 GetRotationKeyTimeFormat()
	{
		return KeyTimeClass::GetKeyTimeFormat();
	}

	void SetRotKeyTimes(uint32 num, char * pData)
	{
		this->SetKeyTimes(num, pData);
	}

	char * GetRotKeyTimes()
	{
		return this->GetData();
	}

private:

	Quat GetValue (int GAID, f32 normalized_time)
	{
		//DEFINE_PROFILER_SECTION("ControllerPQ::GetValue");
		DEFINE_ALIGNED_DATA (Quat, pos, 16);
		//Quat pos;

		f32 t;
		uint32 key = this->GetKey(GAID, normalized_time, t);

		if (key == 0)
		{
			DEFINE_ALIGNED_DATA (Quat, p1, 16);
			GetValueFromKey(0, p1);
			pos = p1;
		}
		else
			if (key >= GetRotationNumCount())
			{
				DEFINE_ALIGNED_DATA (Quat, p1, 16);
				assert(key - 1 < GetRotationNumCount());
				GetValueFromKey(GetRotationNumCount()-1, p1);
				pos = p1;
			}
			else
			{
				//	Quat p1, p2;
				DEFINE_ALIGNED_DATA (Quat, p1, 16);
				DEFINE_ALIGNED_DATA (Quat, p2, 16);

				GetValueFromKey(key-1, p1);
				GetValueFromKey(key, p2);

				_Interpolator::Blend(pos, p1, p2, t);
			}

			return pos;
	}

	ILINE void GetValueFromKey(uint32 key, Quat& val)
	{
		m_arrRots[key].ToExternalType(val);
	}

private:
	T * m_arrRots;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
template<class T, class C, class KeyTimeClass, class _Interpolator>
typedef CRotationData<SmallTree64BitExtQuat, uint16, CKeyTimesUINT16, > CRotationNoCompressKeysF32;
SmallTree48BitQuat
NoCompressQuat
*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class KeyTimeClass>
class CEmptyPositionData : public KeyTimeClass
{
public:

	uint32 GetPositionNumCount()
	{
		return 0;
	}

	uint32 GetPositionValue(int GAID, f32 normalized_time,Vec3& p)
	{

		return 0;
	}

	void SetPositionData(uint32 num, char * m_pData)
	{

	}

	char * GetPositionData()
	{
		return 0;
	}

	static uint32 GetPositionKeyTimeFormat()
	{
		return eNoFormat;
	}

	static uint32 GetPositionType()
	{
		return eNoFormat;
	}

	void SetPosKeyTimes(uint32 num, char * pData)
	{

	}

	char * GetPosKeyTimes()
	{
		return 0;
	}

};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef CEmptyPositionData<CEmptyKeyTimesData> CEmptyPositionController;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Vec3LerpF32
{
	static inline void Blend(Vec3& res, const Vec3& val1, const Vec3& val2, f32 t)
	{
		res.SetLerp(val1, val2, t);
	}

	static uint32 GetType()
	{
		return eNoCompressVec3;
	}

	typedef NoCompressVec3 TDataTypeTraits;
	typedef uint16 TDataTypeCountTraits;

};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class KeyTimeClass, class _Interpolator>
class CPositionData : public KeyTimeClass
{
public:
	typedef typename _Interpolator::TDataTypeTraits T;
	typedef typename _Interpolator::TDataTypeCountTraits C;

	ILINE uint32 GetPositionNumCount()
	{
		return this->GetNumKeys();
	}

	uint32 GetPositionValue(int GAID, f32 normalized_time,Vec3& p)
	{
		p = GetValue(GAID, normalized_time);
		return STATUS_P;
	}

	ILINE void GetPositionValueFromKey(uint32 key, Vec3& t)
	{
		GetValueFromKey(key, t);
	}

	void SetPositionData(uint32 num, char * m_pData)
	{
		assert(num == this->GetNumKeys());
		m_arrKeys = (T*)m_pData;
	}

	static uint32 GetPositionType()
	{
		return _Interpolator::GetType();
	}

	static uint32 GetPositionKeyTimeFormat()
	{
		return KeyTimeClass::GetKeyTimeFormat();
	}

	void SetPosKeyTimes(uint32 num, char * pData)
	{
		this->SetKeyTimes(num, pData);
	}

	char * GetPositionData()
	{
		return (char *)m_arrKeys;
	}

	char * GetPosKeyTimes()
	{
		return this->GetData();
	}

private:

	Vec3 GetValue (int GAID, f32 normalized_time)
	{
		//DEFINE_PROFILER_SECTION("ControllerPQ::GetValue");
		DEFINE_ALIGNED_DATA (Vec3, pos, 16);

		//		Vec3 pos;

		f32 t;
		uint32 key = this->GetKey(GAID, normalized_time, t);

		if (key == 0)
		{
			GetValueFromKey(0, pos);
		}
		else
			if (key >= GetPositionNumCount())
			{
				GetValueFromKey(key-1, pos);
			}
			else
			{
				DEFINE_ALIGNED_DATA (Vec3, p1, 16);
				DEFINE_ALIGNED_DATA (Vec3, p2, 16);

				GetValueFromKey(key-1, p1);
				GetValueFromKey(key, p2);

				_Interpolator::Blend(pos, p1, p2, t);
			}

			return pos;
	}

	ILINE void GetValueFromKey(uint32 key, Vec3& val)
	{
		m_arrKeys[key].ToExternalType(val);
	}

private:
	T * m_arrKeys;

};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
typedef CPositionData<Vec3, CKeyTimesUINT16> CPositionNoCompressKeysF32;
typedef CPositionData<uint16, CKeyTimesUINT16> CPositionNoCompressKeysUINT16;
*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class IControllerOpt : public IController
{
public:
	virtual void SetRotationKeyTimes(uint32 num, char * pData) = 0;
	virtual void SetPositionKeyTimes(uint32 num, char * pData) = 0;
	virtual void SetRotationKeys(uint32 num, char * pData) = 0;
	virtual void SetPositionKeys(uint32 num, char * pData) = 0;

	virtual uint32 GetRotationFormat() = 0;
	virtual uint32 GetRotationKeyTimesFormat() = 0;
	virtual uint32 GetPositionFormat() = 0;
	virtual uint32 GetPositionKeyTimesFormat() = 0;

	virtual char * GetRotationKeyData() = 0;
	virtual char * GetRotationKeyTimesData() = 0;
	virtual char * GetPositionKeyData() = 0;
	virtual char * GetPositionKeyTimesData() = 0;

	uint32 m_nControllerId;
};

TYPEDEF_AUTOPTR(IControllerOpt);

template <class _PosController, class _RotController>
class CControllerOpt : public IControllerOpt, _PosController, _RotController
{
public:
	//Creation interface

	CControllerOpt(){}

	~CControllerOpt()
	{
		int a = 0;
	}

	uint32 numKeys() const
	{
		// now its hack, because num keys might be different
		return max(this->GetRotationNumCount(), this->GetPositionNumCount());
	}

	uint32 GetID () const {return m_nControllerId;}

	Status4 GetOPS (int GAID, f32 normalized_time, Quat& quat, Vec3& pos, Diag33& scale)
	{
		Status4 res;
		res.o = GetO(GAID, normalized_time, quat) ? 1 : 0;
		res.p = GetP(GAID, normalized_time, pos) ? 1 : 0;
		res.s = GetS(GAID, normalized_time, scale) ? 1: 0;

		return res; 
	}

	Status4 GetOP (int GAID, f32 normalized_time, Quat& quat, Vec3& pos)
	{
		Status4 res;
		res.o = GetO(GAID, normalized_time, quat) ? 1 : 0;
		res.p = GetP(GAID, normalized_time, pos) ? 1 : 0;

		return res; 
	}

	uint32 GetO (int GAID, f32 normalized_time, Quat& quat)
	{
		return this->GetRotationValue(GAID, normalized_time, quat);
	}

	uint32 GetP (int GAID, f32 normalized_time, Vec3& pos)
	{
		return this->GetPositionValue(GAID, normalized_time, pos);
	}

	uint32 GetS (int GAID, f32 normalized_time, Diag33& pos)
	{
		return 0;
	}


	Quat GetOrientationByKey (uint32 key)
	{
		//???
//		assert(key<9);  //don't remove this assert. if the value is not 9, then we use compression for this controller. Thats not good
		//Quat t;
		DEFINE_ALIGNED_DATA_STATIC (Quat, t, 16);
		this->GetRotationValueFromKey(key,t);
		return t;
	}

	// returns the start time
	size_t SizeOfThis () const
	{

		size_t res(sizeof(*this));

		/*
		res += GetRotationSizeOfThis();
		res += GetPositionSizeOfThis();
		*/

		return  res;

	}

	size_t GetRotationKeysNum()
	{
		return this->GetRotationNumCount();
	}

	size_t GetPositionKeysNum()
	{
		return this->GetPositionNumCount();
	}	


	CInfo GetControllerInfo() const
	{
		CInfo info;
		/*
		//		info.numKeys = numKeys();
		info.numKeys = GetRotationNumKeys();
		info.quat = GetRotationValueFromKey(info.numKeys - 1);

		info.etime	 = _RotController::GetKeyValueFloat(info.numKeys - 1);//(int)m_pRotationController->GetTimeFromKey(info.numKeys - 1);
		info.stime	 = _RotController::GetKeyValueFloat(0);


		if (m_pPositionInformation)
		{
		info.numKeys = m_pPositionInformation->GetNumKeys();
		m_pPositionInformation->GetValueFromKey(info.numKeys - 1, info.pos);
		info.etime	 = (int)m_pPositionInformation->GetTimeFromKey(info.numKeys - 1);
		info.stime	 = (int)m_pPositionInformation->GetTimeFromKey(0);
		}


		info.realkeys = (info.etime-info.stime)+1;
		*/
		return info;
	}

	virtual EControllerInfo GetControllerType() 
	{
		return eControllerOpt;
	}

	void SetRotationKeyTimes(uint32 num, char * pData)
	{
		this->SetRotKeyTimes/*_RotController::SetKeyTimes*/(num, pData);
	}

	void SetPositionKeyTimes(uint32 num, char * pData)
	{
		this->SetPosKeyTimes/*_PosController::SetKeyTimes*/(num, pData);
	}

	void SetRotationKeys(uint32 num, char * pData)
	{
		this->SetRotationData(num, pData);
	}

	void SetPositionKeys(uint32 num, char * pData)
	{
		this->SetPositionData(num, pData);
	}


	uint32 GetRotationFormat()
	{
		return this->GetRotationType();
	}

	uint32 GetRotationKeyTimesFormat()
	{
		return this->GetRotationKeyTimeFormat();
	}

	uint32 GetPositionFormat()
	{
		return this->GetPositionType();
	}

	uint32 GetPositionKeyTimesFormat()
	{
		return this->GetPositionKeyTimeFormat();
	}

	char * GetRotationKeyData()
	{
		return this->GetRotationData();
	}

	char * GetRotationKeyTimesData()
	{
		return this->GetRotKeyTimes();
	}

	char * GetPositionKeyData()
	{
		return this->GetPositionData();
	}

	char * GetPositionKeyTimesData()
	{
		return this->GetPosKeyTimes();
	}

	IControllerOpt * CreateController()
	{
		return (IControllerOpt *)new CControllerOpt<_PosController, _RotController>();
	}


	//static uint32 CalculateHash()
	//{
	//	uint32 hash =  ControllerHash(GetRotationType(), GetRotationKeyTimeFormat(), GetPositionType(), GetPositionKeyTimeFormat());
	//	return hash;
	//};


};

//template <class _PosController, class _RotController>
//uint32  CControllerOpt<_PosController, _RotController>::CalculateHash()
//{
//	uint32 hash =  ControllerHash(GetRotationType(), _RotController::GetKeyTimeFormat(), GetPositionType(), _PosController::GetKeyTimeFormat());
//	return hash;
//}


typedef class CControllerOpt<CEmptyPositionController, CEmptyRotationController> CEmptyController;


template<class T>
class CControllerFactoryTpl
{
public:
	typedef T * (*CreateCallback)(int);
private:
	typedef std::map<uint32, CreateCallback> CallbackMap;
public:
	//	CControllerFactoryTpl() {};
	bool Register(uint32 Name, CreateCallback CreateFn)
	{
		return callbacks_.insert(std::make_pair(Name, CreateFn)).second;
	}

	bool Unregister(uint32 Name)
	{
		return callbacks_.erase(Name) == 1;
	}

	T * Create(uint32 Name, int count = 1)
	{
		typename CallbackMap::const_iterator i = callbacks_.find(Name);
		if (i == callbacks_.end())
		{
			return NULL;
		}
		return (i->second)(count);
	}

private:
	CallbackMap callbacks_;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef class CControllerFactoryTpl<IControllerOpt> CControllerFactory;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T>
class CSimpleSingleton 
{
public:
	static T& GetInstance()
	{
		static T inst;
		return inst;
	}

};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef CSimpleSingleton<CControllerFactory> CControllerFactoryInst;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define REGISTER_CLASSINFO(ClassName, Id) \
	namespace \
{ \
	IControllerOpt * Create##ClassName(int count) \
{ \
	if (count < 2) \
	return new ClassName; \
	else \
	return new ClassName[count]; \
} \
struct Reg##ClassName \
{ \
	bool bRegistered; \
	Reg##ClassName() \
{ \
	bRegistered = CControllerFactoryInst::GetInstance().Register(Id, Create##ClassName); \
}; \
}; \
	static Reg##ClassName m_reg##ClassName; \
} \

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//ControllerHash(GetRotationType(), GetRotationKeyTimeFormat(), GetPositionType(), GetPositionKeyTimeFormat());
#define REGISTER_COMBINATION(ROTKT,ROTINT,POSKT, POSINT) \
	namespace \
{ \
	typedef class CControllerOpt< CRotationData<ROTKT,ROTINT> , CPositionData<POSKT, POSINT> > CController##ROTKT##ROTINT##POSKT##POSINT;  \
	REGISTER_CLASSINFO(CController##ROTKT##ROTINT##POSKT##POSINT, ControllerHash(CRotationData<ROTKT,ROTINT>::GetRotationType(), CRotationData<ROTKT,ROTINT>::GetRotationKeyTimeFormat(), \
	CPositionData<POSKT, POSINT>::GetPositionType(), CPositionData<POSKT, POSINT>::GetPositionKeyTimeFormat())); \
}; 


//CController##ROTKT##ROTINT##POSKT##POSINT::CalculateHash()); \
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define REGISTER_COMBINATION_NO_POS(ROTKT,ROTINT) \
	namespace \
{ \
	typedef class CControllerOpt< CRotationData<ROTKT,ROTINT> , CEmptyPositionController > CController##ROTKT##ROTINT;  \
	REGISTER_CLASSINFO(CController##ROTKT##ROTINT, ControllerHash(CRotationData<ROTKT,ROTINT>::GetRotationType(), CRotationData<ROTKT,ROTINT>::GetRotationKeyTimeFormat(), \
	CEmptyPositionController::GetPositionType(), CEmptyPositionController::GetPositionKeyTimeFormat())); \
}; 


//CController##ROTKT##ROTINT::CalculateHash()); \
//}; \

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define REGISTER_COMBINATION_NO_ROT(POSKT, POSINT) \
	namespace \
{ \
	typedef class CControllerOpt< CEmptyRotationController , CPositionData<POSKT, POSINT> > CController##POSKT##POSINT;  \
	REGISTER_CLASSINFO(CController##POSKT##POSINT, ControllerHash(CEmptyRotationController::GetRotationType(), CEmptyRotationController::GetRotationKeyTimeFormat(), \
	CPositionData<POSKT, POSINT>::GetPositionType(), CPositionData<POSKT, POSINT>::GetPositionKeyTimeFormat())); \
}; 


//CController##POSKT##POSINT::CalculateHash()); \
//}; \

//CKeyTimesDataBitSet
// Special case! :-)
typedef class CControllerOpt< CEmptyRotationController , CEmptyPositionController > CControllerEmpty;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
typedef CKeyTimesData<ByteEncoder> CKeyTimesByte;
typedef CKeyTimesData<UINT16Encoder> CKeyTimesUINT16;
typedef CKeyTimesData<F32Encoder> CKeyTimesF32;

struct QuatLerpNoCompression
struct QuatLerpSmallTree48BitQuat
struct QuatLerpSmallTree64BitQuat
struct QuatLerpSmallTree64BitExtQuat

Vec3LerpF32
*/


// copy-paste rules!!!

REGISTER_COMBINATION_NO_ROT(CKeyTimesByte,Vec3LerpF32);
REGISTER_COMBINATION_NO_ROT(CKeyTimesUINT16,Vec3LerpF32);
REGISTER_COMBINATION_NO_ROT(CKeyTimesF32,Vec3LerpF32);
REGISTER_COMBINATION_NO_ROT(CKeyTimesBitset,Vec3LerpF32);

REGISTER_COMBINATION_NO_POS(CKeyTimesByte,QuatLerpNoCompression);
REGISTER_COMBINATION_NO_POS(CKeyTimesByte,QuatLerpSmallTree48BitQuat);
REGISTER_COMBINATION_NO_POS(CKeyTimesByte,QuatLerpSmallTree64BitQuat);
REGISTER_COMBINATION_NO_POS(CKeyTimesByte,QuatLerpSmallTree64BitExtQuat);

REGISTER_COMBINATION_NO_POS(CKeyTimesUINT16,QuatLerpNoCompression);
REGISTER_COMBINATION_NO_POS(CKeyTimesUINT16,QuatLerpSmallTree48BitQuat);
REGISTER_COMBINATION_NO_POS(CKeyTimesUINT16,QuatLerpSmallTree64BitQuat);
REGISTER_COMBINATION_NO_POS(CKeyTimesUINT16,QuatLerpSmallTree64BitExtQuat);

REGISTER_COMBINATION_NO_POS(CKeyTimesF32,QuatLerpNoCompression);
REGISTER_COMBINATION_NO_POS(CKeyTimesF32,QuatLerpSmallTree48BitQuat);
REGISTER_COMBINATION_NO_POS(CKeyTimesF32,QuatLerpSmallTree64BitQuat);
REGISTER_COMBINATION_NO_POS(CKeyTimesF32,QuatLerpSmallTree64BitExtQuat);

REGISTER_COMBINATION_NO_POS(CKeyTimesBitset,QuatLerpNoCompression);
REGISTER_COMBINATION_NO_POS(CKeyTimesBitset,QuatLerpSmallTree48BitQuat);
REGISTER_COMBINATION_NO_POS(CKeyTimesBitset,QuatLerpSmallTree64BitQuat);
REGISTER_COMBINATION_NO_POS(CKeyTimesBitset,QuatLerpSmallTree64BitExtQuat);



REGISTER_COMBINATION(CKeyTimesByte,QuatLerpNoCompression,CKeyTimesByte,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesByte,QuatLerpNoCompression,CKeyTimesUINT16,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesByte,QuatLerpNoCompression,CKeyTimesF32,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesByte,QuatLerpNoCompression,CKeyTimesBitset,Vec3LerpF32);

REGISTER_COMBINATION(CKeyTimesByte,QuatLerpSmallTree48BitQuat,CKeyTimesByte,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesByte,QuatLerpSmallTree48BitQuat,CKeyTimesUINT16,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesByte,QuatLerpSmallTree48BitQuat,CKeyTimesF32,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesByte,QuatLerpSmallTree48BitQuat,CKeyTimesBitset,Vec3LerpF32);

REGISTER_COMBINATION(CKeyTimesByte,QuatLerpSmallTree64BitQuat,CKeyTimesByte,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesByte,QuatLerpSmallTree64BitQuat,CKeyTimesUINT16,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesByte,QuatLerpSmallTree64BitQuat,CKeyTimesF32,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesByte,QuatLerpSmallTree64BitQuat,CKeyTimesBitset,Vec3LerpF32);

REGISTER_COMBINATION(CKeyTimesByte,QuatLerpSmallTree64BitExtQuat,CKeyTimesByte,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesByte,QuatLerpSmallTree64BitExtQuat,CKeyTimesUINT16,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesByte,QuatLerpSmallTree64BitExtQuat,CKeyTimesF32,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesByte,QuatLerpSmallTree64BitExtQuat,CKeyTimesBitset,Vec3LerpF32);



REGISTER_COMBINATION(CKeyTimesUINT16,QuatLerpNoCompression,CKeyTimesByte,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesUINT16,QuatLerpNoCompression,CKeyTimesUINT16,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesUINT16,QuatLerpNoCompression,CKeyTimesF32,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesUINT16,QuatLerpNoCompression,CKeyTimesBitset,Vec3LerpF32);

REGISTER_COMBINATION(CKeyTimesUINT16,QuatLerpSmallTree48BitQuat,CKeyTimesByte,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesUINT16,QuatLerpSmallTree48BitQuat,CKeyTimesUINT16,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesUINT16,QuatLerpSmallTree48BitQuat,CKeyTimesF32,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesUINT16,QuatLerpSmallTree48BitQuat,CKeyTimesBitset,Vec3LerpF32);

REGISTER_COMBINATION(CKeyTimesUINT16,QuatLerpSmallTree64BitQuat,CKeyTimesByte,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesUINT16,QuatLerpSmallTree64BitQuat,CKeyTimesUINT16,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesUINT16,QuatLerpSmallTree64BitQuat,CKeyTimesF32,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesUINT16,QuatLerpSmallTree64BitQuat,CKeyTimesBitset,Vec3LerpF32);

REGISTER_COMBINATION(CKeyTimesUINT16,QuatLerpSmallTree64BitExtQuat,CKeyTimesByte,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesUINT16,QuatLerpSmallTree64BitExtQuat,CKeyTimesUINT16,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesUINT16,QuatLerpSmallTree64BitExtQuat,CKeyTimesF32,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesUINT16,QuatLerpSmallTree64BitExtQuat,CKeyTimesBitset,Vec3LerpF32);



REGISTER_COMBINATION(CKeyTimesF32,QuatLerpNoCompression,CKeyTimesByte,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesF32,QuatLerpNoCompression,CKeyTimesUINT16,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesF32,QuatLerpNoCompression,CKeyTimesF32,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesF32,QuatLerpNoCompression,CKeyTimesBitset,Vec3LerpF32);

REGISTER_COMBINATION(CKeyTimesF32,QuatLerpSmallTree48BitQuat,CKeyTimesByte,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesF32,QuatLerpSmallTree48BitQuat,CKeyTimesUINT16,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesF32,QuatLerpSmallTree48BitQuat,CKeyTimesF32,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesF32,QuatLerpSmallTree48BitQuat,CKeyTimesBitset,Vec3LerpF32);

REGISTER_COMBINATION(CKeyTimesF32,QuatLerpSmallTree64BitQuat,CKeyTimesByte,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesF32,QuatLerpSmallTree64BitQuat,CKeyTimesUINT16,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesF32,QuatLerpSmallTree64BitQuat,CKeyTimesF32,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesF32,QuatLerpSmallTree64BitQuat,CKeyTimesBitset,Vec3LerpF32);

REGISTER_COMBINATION(CKeyTimesF32,QuatLerpSmallTree64BitExtQuat,CKeyTimesByte,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesF32,QuatLerpSmallTree64BitExtQuat,CKeyTimesUINT16,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesF32,QuatLerpSmallTree64BitExtQuat,CKeyTimesF32,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesF32,QuatLerpSmallTree64BitExtQuat,CKeyTimesBitset,Vec3LerpF32);



REGISTER_COMBINATION(CKeyTimesBitset,QuatLerpNoCompression,CKeyTimesByte,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesBitset,QuatLerpNoCompression,CKeyTimesUINT16,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesBitset,QuatLerpNoCompression,CKeyTimesF32,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesBitset,QuatLerpNoCompression,CKeyTimesBitset,Vec3LerpF32);

REGISTER_COMBINATION(CKeyTimesBitset,QuatLerpSmallTree48BitQuat,CKeyTimesByte,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesBitset,QuatLerpSmallTree48BitQuat,CKeyTimesUINT16,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesBitset,QuatLerpSmallTree48BitQuat,CKeyTimesF32,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesBitset,QuatLerpSmallTree48BitQuat,CKeyTimesBitset,Vec3LerpF32);

REGISTER_COMBINATION(CKeyTimesBitset,QuatLerpSmallTree64BitQuat,CKeyTimesByte,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesBitset,QuatLerpSmallTree64BitQuat,CKeyTimesUINT16,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesBitset,QuatLerpSmallTree64BitQuat,CKeyTimesF32,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesBitset,QuatLerpSmallTree64BitQuat,CKeyTimesBitset,Vec3LerpF32);

REGISTER_COMBINATION(CKeyTimesBitset,QuatLerpSmallTree64BitExtQuat,CKeyTimesByte,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesBitset,QuatLerpSmallTree64BitExtQuat,CKeyTimesUINT16,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesBitset,QuatLerpSmallTree64BitExtQuat,CKeyTimesF32,Vec3LerpF32);
REGISTER_COMBINATION(CKeyTimesBitset,QuatLerpSmallTree64BitExtQuat,CKeyTimesBitset,Vec3LerpF32);





#endif//_CRYTEK_CONTROLLER_OPT_PQ_

