#include "stdafx.h"
#include "ControllerPQ.h"
#include "CharacterManager.h"

#include "ControllerOpt.h"

#include <float.h>

namespace ControllerHelper
{

	uint32 GetPositionsFormatSizeOf(uint32 format)
	{
		switch(format)
		{
		case eNoCompress:
		case eNoCompressVec3:
			{
				return sizeof (NoCompressVec3);
			}
		}
		return 0;
	}


	uint32 GetRotationFormatSizeOf(uint32 format)
	{
		switch(format)
		{
		case	eNoCompress:
		case eNoCompressQuat:
			return sizeof(NoCompressQuat);

		//case eSmallTreeDWORDQuat:
		//	return sizeof(Sma;

		case eSmallTree48BitQuat:
			return sizeof(SmallTree48BitQuat);

		case eSmallTree64BitQuat:
			return sizeof(SmallTree64BitQuat);

		case eSmallTree64BitExtQuat:
			return sizeof(SmallTree64BitExtQuat);

		}
		return 0;

	}

	uint32 GetKeyTimesFormatSizeOf(uint32 format)
	{
		switch(format)
		{

		case eF32:
			return sizeof(float);

		case eUINT16:
			return sizeof(uint16);

		case eByte:
			return sizeof(byte);

		case eF32StartStop:
			return sizeof(float);

		case eUINT16StartStop:
			return sizeof(uint16);

		case eByteStartStop:
			return sizeof(byte);

		case eBitset:
			return sizeof(uint16);
		}

		return 0;

	}

	ITrackPositionStorage * GetPositionControllerPtr(uint32 format)
	{

//		Quat t;
//		int GG = test.GetO(0,0,t);

		switch(format)
		{
			case eNoCompress:
			case eNoCompressVec3:
				{
//					return new NoCompressVec3PositionTrackInformation;
//					IPositionInformation * pTrack = new PositionTrackInformation;
//				ITrackPositionStorage * pStorage = new NoCompressPosition;
//					pTrack->SetPositionStorage(TrackPositionStoragePtr(pStorage));
//					return pTrack;
					return new NoCompressPosition;
				}
				

		}
		return 0;
		//return new 
	}

	ITrackPositionStorage * GetPositionControllerPtrArray(uint32 format, uint32 size)
	{

		switch(format)
		{
		case eNoCompress:
		case eNoCompressVec3:
			{
				//					return new NoCompressVec3PositionTrackInformation;
				//					IPositionInformation * pTrack = new PositionTrackInformation;
				//				ITrackPositionStorage * pStorage = new NoCompressPosition;
				//					pTrack->SetPositionStorage(TrackPositionStoragePtr(pStorage));
				//					return pTrack;
				return new NoCompressPositionInplace[size];
			}


		}
		return 0;
		//return new 
	}

	ITrackPositionStorage * GetPositionPtrFromArray(void * ptr, uint32 format, uint32 num)
	{
		switch(format)
		{
		case eNoCompress:
		case eNoCompressVec3:
				return &((NoCompressPositionInplace*)(ptr))[num];
		}
		return 0;
	}

	ITrackRotationStorage * GetRotationControllerPtr(uint32 format)
	{
		switch(format)
		{
		case	eNoCompress:
		case eNoCompressQuat:
			return new NoCompressRotation;
			//case eSmallTreeDWORDQuat:
			//	//return new SmallTreeDWORDQuatRotationTrackInformation;

		case eSmallTree48BitQuat:
			//return new SmallTree48BitQuatRotationTrackInformation;
			return new SmallTree48BitQuatRotation;

		case eSmallTree64BitQuat:
			//return new SmallTree64BitQuatRotationTrackInformation;
			return new SmallTree64BitQuatRotation;

			//case ePolarQuat:
			//	//return new PolarQuatRotationTrackInformation;

		case eSmallTree64BitExtQuat:
			//return new SmallTree64BitExtQuatRotationTrackInformation;
			return new SmallTree64BitExtQuatRotation;

		}
		return 0;
	}

	ITrackRotationStorage * GetRotationControllerPtrArray(uint32 format, uint32 size)
	{
		switch(format)
		{

			case	eNoCompress:
			case eNoCompressQuat:
				return new NoCompressRotationInplace[size];

			//case eShotInt3Quat:

			//	//{
			//	//	//					return new ShotInt3QuatRotationTrackInformation;
			//	//	IPositionInformation * pTrack = new PositionTrackInformation;
			//	//	ITrackPositionStorage * pStorage = new NoCompressPosition;
			//	//	pTrack->SetPositionStorage(TrackPositionStoragePtr(pStorage));

			//	//	return pTrack;
			//	//}



			//case eSmallTreeDWORDQuat:
			//	//return new SmallTreeDWORDQuatRotationTrackInformation;

			case eSmallTree48BitQuat:
				//return new SmallTree48BitQuatRotationTrackInformation;
				return new SmallTree48BitQuatRotationInplace[size];

			case eSmallTree64BitQuat:
				//return new SmallTree64BitQuatRotationTrackInformation;
				return new SmallTree64BitQuatRotationInplace[size];

			//case ePolarQuat:
			//	//return new PolarQuatRotationTrackInformation;

			case eSmallTree64BitExtQuat:
				//return new SmallTree64BitExtQuatRotationTrackInformation;
				return new SmallTree64BitExtQuatRotationInplace[size];

		}
		return 0;
	}

	ITrackRotationStorage * GetRotationPtrFromArray(void * ptr,uint32 format, uint32 num)
	{
		switch(format)
		{

		case	eNoCompress:
		case eNoCompressQuat:
			return &((NoCompressRotationInplace*)(ptr))[num]; 


		case eSmallTree48BitQuat:
			return &((SmallTree48BitQuatRotationInplace*)(ptr))[num]; 


		case eSmallTree64BitQuat:
			return &((SmallTree64BitQuatRotationInplace*)(ptr))[num]; 

		case eSmallTree64BitExtQuat:

			return &((SmallTree64BitExtQuatRotationInplace*)(ptr))[num]; 

		}
		return 0;

	}


	IKeyTimesInformation * GetKeyTimesControllerPtr(uint32 format)
	{
		switch(format)
		{
		case eF32:
			return new F32KeyTimesInformation;

		case eUINT16:
			return new UINT16KeyTimesInformation;

		case eByte:
			return new ByteKeyTimesInformation;

		case eF32StartStop:
			return new F32SSKeyTimesInformation;

		case eUINT16StartStop:
			return new UINT16SSKeyTimesInformation;

		case eByteStartStop:
			return new ByteSSKeyTimesInformation;

		case eBitset:
			return new CKeyTimesInformationBitSet;

		}
		return 0;
	}

	IKeyTimesInformation * GetKeyTimesControllerPtrArray(uint32 format, uint32 size)
	{
		switch(format)
		{
		case eF32:
			return new F32KeyTimesInformationInplace[size];

		case eUINT16:
			return new UINT16KeyTimesInformationInplace[size];

		case eByte:
			return new ByteKeyTimesInformationInplace[size];

		case eF32StartStop:
			return new F32SSKeyTimesInformation[size];

		case eUINT16StartStop:
			return new UINT16SSKeyTimesInformation[size];

		case eByteStartStop:
			return new ByteSSKeyTimesInformation[size];

		case eBitset:
			return new CKeyTimesInformationBitSet[size];


		}
		return 0;
	}

	IKeyTimesInformation * GetKeyTimesPtrFromArray(void * ptr, uint32 format, uint32 num)
	{
		switch(format)
		{
		case eF32:
			{
			//	F32KeyTimesInformationInplace* p = (F32KeyTimesInformationInplace *)(ptr);
			//	return &p[num];
				return &((F32KeyTimesInformationInplace*)(ptr))[num];
			}
			

		case eUINT16:
			return &((UINT16KeyTimesInformationInplace*)(ptr))[num];

		case eByte:
			return &((ByteKeyTimesInformationInplace*)(ptr))[num];

		case eF32StartStop:
			return &((F32SSKeyTimesInformation*)(ptr))[num];

		case eUINT16StartStop:
			return &((UINT16SSKeyTimesInformation*)(ptr))[num];

		case eByteStartStop:
			return &((ByteSSKeyTimesInformation*)(ptr))[num];

		case eBitset:
			return &((CKeyTimesInformationBitSet*)(ptr))[num];


		}
		return 0;

	}

	void DeletePositionControllerPtrArray(ITrackPositionStorage * ptr)
	{
		delete[] ptr;
	}

	void DeleteRotationControllerPtrArray(ITrackRotationStorage * ptr)
	{
		//free(ptr);
		delete[] ptr;
	}

	void DeleteTimesControllerPtrArray(IKeyTimesInformation * ptr)
	{
		//free(ptr);
		delete[] ptr;
	}


	//FIXME:
	f32 NTime2KTime(int32 GAID,f32 ntime)
	{
		if (ntime>1)
			ntime=1;

		assert(ntime>=0 && ntime<=1);
		int32 numAnims=int32(g_AnimationManager.m_arrGlobalAnimations.size());
		assert(GAID>=0);
		assert(GAID<numAnims);
		GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[GAID];
		f32 duration	=	rGlobalAnimHeader.m_fEndSec-rGlobalAnimHeader.m_fStartSec;		
		f32 start			=	rGlobalAnimHeader.m_fStartSec;		
		f32 key				= (ntime*TICKS_PER_SECOND*duration  + start*TICKS_PER_SECOND);///40.0f;
		return key;
	}


	IControllerOpt * CreateController(int iRotType, int iRotKeyType, int iPosType, int iPosKeyType )
	{
		return CControllerFactoryInst::GetInstance().Create(ControllerHash(iRotType, iRotKeyType, iPosType, iPosKeyType));
		return 0;
	}
}; 


void CCompressedController::UnCompress()
{

	CWaveletData Rotations[3];
	CWaveletData Positions[3];


	//	for (uint32 i = 0; i < 3; ++i)
	if (m_Rotations0.size() > 0)
	{
		Wavelet::UnCompress(m_Rotations0, Rotations[0]);
		Wavelet::UnCompress(m_Rotations1, Rotations[1]);
		Wavelet::UnCompress(m_Rotations2, Rotations[2]);

		uint32 size = m_pRotationController->GetKeyTimesInformation()->GetNumKeys();//Rotations[0].m_Data.size();

//		m_pRotationController->Reserve(size);

		for (uint32 i = 0; i < size; ++i)
		{
			Ang3 ang(Rotations[0].m_Data[i], Rotations[1].m_Data[i], Rotations[2].m_Data[i]);
			Quat quat;
			quat.SetRotationXYZ(ang);
			quat.Normalize();
			m_pRotationController->GetRotationStorage()->AddValue(quat);
		}

	}


	if (m_Positions0.size() > 0)
	{
		Wavelet::UnCompress(m_Positions0, Positions[0]);
		Wavelet::UnCompress(m_Positions2, Positions[2]);
		Wavelet::UnCompress(m_Positions1, Positions[1]);



		uint32 size = m_pPositionInformation->GetKeyTimesInformation()->GetNumKeys(); //Positions[0].m_Data.size();
//		m_pPositionInformation->Reserve(size);

		for (uint32 i = 0; i < size; ++i)
		{
			Vec3 pos(Positions[0].m_Data[i], Positions[1].m_Data[i], Positions[2].m_Data[i]);
			m_pPositionInformation->GetPositionStorage()->AddValue(pos);
		}

	}

	m_Rotations0.clear();
	m_Rotations1.clear();
	m_Rotations2.clear();
	m_Positions0.clear();
	m_Positions1.clear();
	m_Positions2.clear();
	
}


// We create compressed data in the RC
void CCompressedController::Compress(IController * uncompressedController)//TrackInformationPtr& uncompRotation, PositionInformationPtr& uncompPosition)
{

	//CWaveletData Rotations[3];
	//CWaveletData Positions[3];

	//Rotations[0].m_Data.resize(m_pRotationController->GetNumKeys());
	//Rotations[1].m_Data.resize(m_pRotationController->GetNumKeys());
	//Rotations[2].m_Data.resize(m_pRotationController->GetNumKeys());


	//CControllerPQLog * pController = dynamic_cast<CControllerPQLog *>(uncompressedController);
	//if (pController)
	//{
	//	for (uint32 i = 0; i < m_pRotationController->GetNumKeys(); ++i)
	//	{
	//		Quat quat;
	//		//m_pRotationController->GetValueFromKey(i, quat);
	//		uint32 key = (uint32)m_pRotationController->GetTimeFromKey(i);
	//		// Get uncompressed value to increase precision
	//		quat = !exp(pController->m_arrKeys[i].vRotLog);

	//		// Convert to euler angles
	//		Ang3 angles = Ang3::GetAnglesXYZ(quat);

	//		Rotations[0].m_Data[i] = angles.x;
	//		Rotations[1].m_Data[i] = angles.y;
	//		Rotations[2].m_Data[i] = angles.z;
	//	}

	//}
	//else
	//{
	//	CController * pCController  = dynamic_cast<CController*>(uncompressedController);

	//	if (!pCController)
	//	{
	//		assert(false);
	//		return;
	//	}

	//	for (uint32 i = 0; i < m_pRotationController->GetNumKeys(); ++i)
	//	{

	//		Quat quat;
	//		//m_pRotationController->GetValueFromKey(i, quat);
	//		uint32 key = (uint32)m_pRotationController->GetTimeFromKey(i);
	//		// Get uncompressed value to increase precision
	//		pCController->GetRotationController()->GetValueFromKey(key, quat);

	//		// Convert to euler angles
	//		Ang3 angles = Ang3::GetAnglesXYZ(quat);

	//		Rotations[0].m_Data[i] = angles.x;
	//		Rotations[1].m_Data[i] = angles.y;
	//		Rotations[2].m_Data[i] = angles.z;
	//	}

	//}

	//if (m_pRotationController->GetNumKeys() & 1)
	//{
	//	// Add last value for even number of samples
	//	Rotations[0].m_Data.push_back(Rotations[0].m_Data[Rotations[0].m_Data.size()-1]);
	//	Rotations[1].m_Data.push_back(Rotations[1].m_Data[Rotations[1].m_Data.size()-1]);
	//	Rotations[2].m_Data.push_back(Rotations[2].m_Data[Rotations[2].m_Data.size()-1]);
	//}

	//int rotSizes = 0;

	//m_iRotationFormat = m_pRotationController->GetFormat();

	//float rotError = 0.0001f;//0.000001f

	////	for (uint32 i = 0; i < 3; ++i)
	//{
	//	Wavelet::Compress(Rotations[0], m_Rotations0, 0, 1, rotError, rotError, 30, 30);//0.000001f
	//	rotSizes += m_Rotations0.size() * sizeof(int);
	//	Wavelet::Compress(Rotations[1], m_Rotations1, 0, 1, rotError, rotError, 30, 30);//0.000001f
	//	rotSizes += m_Rotations1.size() * sizeof(int);
	//	Wavelet::Compress(Rotations[2], m_Rotations2, 0, 1, rotError, rotError, 30, 30);//0.000001f
	//	rotSizes += m_Rotations2.size() * sizeof(int);

	//}


	//if (rotSizes > m_pRotationController->GetDataRawSize())
	//{
	//	// Quantization format takes less memory. Don't use wavelets
	//	rotSizes = m_pRotationController->GetDataRawSize();
	//	m_Rotations0.clear();
	//	m_Rotations1.clear();
	//	m_Rotations2.clear();
	//}
	////else
	////{
	////	int a = 0;
	////}

	//int posSizes = 0;



	//if (m_pPositionInformation)
	//{

	//	m_iPositionFormat = m_pPositionInformation->GetFormat();

	//	Positions[0].m_Data.resize(m_pPositionInformation->GetNumKeys());
	//	Positions[1].m_Data.resize(m_pPositionInformation->GetNumKeys());
	//	Positions[2].m_Data.resize(m_pPositionInformation->GetNumKeys());

	//	for (uint32 i = 0; i < m_pPositionInformation->GetNumKeys(); ++i)
	//	{
	//		Vec3 pos;
	//		m_pPositionInformation->GetValueFromKey(i, pos);

	//		Positions[0].m_Data[i] = pos.x;
	//		Positions[1].m_Data[i] = pos.y;
	//		Positions[2].m_Data[i] = pos.z;
	//	}

	//	//		if (m_pPositionInformation->GetNumKeys() > 30)

	//	if (m_pPositionInformation->GetNumKeys() & 1)
	//	{
	//		// Add last value for even number of samples
	//		Positions[0].m_Data.push_back(Positions[0].m_Data[Positions[0].m_Data.size()-1]);
	//		Positions[1].m_Data.push_back(Positions[1].m_Data[Positions[1].m_Data.size()-1]);
	//		Positions[2].m_Data.push_back(Positions[2].m_Data[Positions[2].m_Data.size()-1]);
	//	}


	//	float posError = 0.0001f; //0.000001f
	//	//		for (uint32 i = 0; i < 3; ++i)
	//	{
	//		Wavelet::Compress(Positions[0], m_Positions0, 0, 1, posError, posError, 30, 30); //0.000001f
	//		posSizes += m_Positions0.size() * sizeof(int);
	//		Wavelet::Compress(Positions[1], m_Positions1, 0, 1, posError, posError, 30, 30); //0.000001f
	//		posSizes += m_Positions1.size() * sizeof(int);
	//		Wavelet::Compress(Positions[2], m_Positions2, 0, 1, posError, posError, 30, 30); //0.000001f
	//		posSizes += m_Positions2.size() * sizeof(int);

	//	}

	//	if (posSizes > m_pPositionInformation->GetDataRawSize())
	//	{
	//		posSizes = m_pPositionInformation->GetDataRawSize();
	//		m_Positions0.clear();
	//		m_Positions1.clear();
	//		m_Positions2.clear();
	//	}
	//	//else
	//	//{
	//	//	int a = 0;
	//	//}


	//}

	//m_CompressedSize = rotSizes + posSizes;
}


namespace ControllerHelper
{
	byte /*CKeyTimesInformationBitSet::*/m_byteTable[256] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,
		1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,
		4,4,5,4,5,5,6,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,2,3,
		3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,1,2,2,3,2,3,3,4,2,3,3,
		4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,
		4,5,5,6,4,5,5,6,5,6,6,7,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,
		6,6,7,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8};
}

#if defined(_CPU_SSE) && !defined(_DEBUG)
__m128 SmallTree48BitQuat::div;
__m128 SmallTree48BitQuat::ran;

int Init48b =  SmallTree48BitQuat::Init();
#endif