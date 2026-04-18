//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File:AttachmentManager.h
//  Declaration of AttachmentManager class
//
//	History:
//	August 16, 2004: Created by Ivo Herzeg <ivo@crytek.de>
//
//////////////////////////////////////////////////////////////////////
#ifndef _CRY_ATTACHMENT_MANAGER
#define _CRY_ATTACHMENT_MANAGER


class CAttachment;
class CAttachmentManager;
class CSkinInstance;
class CModelMesh;

struct AttMorphTargets
{
	std::vector<SMeshMorphTargetVertex> m_arrAttMorph; 
};

#define nHideAttachment (0x001)

class CAttachmentManager : public IAttachmentManager
{
	friend class CAttachment;

public:
	CAttachmentManager() 
	{	
		m_pSkinInstance=0;	
		m_pSkelInstance=0;	
	};

	uint32 LoadAttachmentList(const char* pathname );
	uint32 SaveAttachmentList(const char* pathname );

	IAttachment* CreateAttachment( const char* szName, uint32, const char* szBoneName );

	int32 GetAttachmentCount() { return m_arrAttachments.size(); };

	IAttachment* GetInterfaceByIndex( uint32 c);
	IAttachment* GetInterfaceByName(  const char* szName ); 

	int32 GetIndexByName( const char* szName ); 

	void UpdateLocationAttachments(const QuatT &mtx,IAttachmentObject::EType *pOnlyThisType );
	void UpdateLocationAttachmentsFast(const QuatT &rPhysLocationNext );
	void DrawAttachments(const SRendParams& rRendParams, Matrix34 &m);

	void RemoveAttachmentByIndex( uint32 n );
	int32 RemoveAttachmentByInterface( const IAttachment* ptr  );
	int32 RemoveAttachmentByName( const char* szName );
	uint32 RemoveAllAttachments(); 
	uint32 ProjectAllAttachment();
	void SkinningAttSW( CModelMesh* pSkinning );

	void PhysicalizeAttachment( int idx, int nLod, IPhysicalEntity *pent, const Vec3 &offset );
	void UpdatePhysicalizedAttachment( int idx, IPhysicalEntity *pent, const QuatT &offset );

	virtual void PhysicalizeAttachment( int idx, IPhysicalEntity *pent=0, int nLod=0 );
	virtual void DephysicalizeAttachment( int idx, IPhysicalEntity *pent=0 );

	void ResetAdditionalRotation();

	void Serialize(TSerialize ser);

	//-----------------------------------------------
	std::vector<CAttachment*> m_arrAttachments;

	std::vector<AttSkinVertex> m_arrAttSkin;		//internal vertices (just position and links)
	//std::vector<AttMorphTargets> m_arrMorphTargets;

	//std::vector<Vec3> m_arrAttMorphStream; 
	std::vector<Vec3> m_arrAttSkinnedStream;

	class CSkinInstance* m_pSkinInstance; 
	class CCharInstance* m_pSkelInstance; 

	size_t SizeOfThis();

private:

};


struct SAttachmentHinge 
{
	int idxEdge;
	Vec3 wcenter;
	float angle;
	float velAng;
	float maxAng;
	float damping;
}; 


class CAttachment : public IAttachment 
{
public:

	uint32 GetType() { return m_Type; } 
	uint32 SetType(uint32 type, const char* szBoneName=0 );

	uint32 GetFlags() { return m_AttFlags; }
	void SetFlags(uint32 flags) { m_AttFlags=flags; }

	const QuatT& GetAttAbsoluteDefault() { return m_AttAbsoluteDefault;  }; 
	void SetAttAbsoluteDefault(const QuatT& rot) 
	{ 
		m_AttAbsoluteDefault=rot; 
		assert(m_AttAbsoluteDefault.q.IsValid()); 
	}; 

	void SetAttRelativeDefault(const QuatT& qt) { m_AttRelativeDefault=qt; }; 
	const QuatT& GetAttRelativeDefault() { return m_AttRelativeDefault; }; 

	const QuatT& GetAttModelRelative() { return m_AttModelRelative;  }; 
	const QuatT& GetAttWorldAbsolute() { return m_AttWorldAbsolute;  }; 


	const char* GetName() { return m_Name; };   
	uint32 ReName( const char* szBoneName ) {	m_Name.clear();	m_Name=szBoneName;	return 1;	};   


	uint32 GetBoneID() const { return m_BoneID; }; 

	uint32 AddBinding( IAttachmentObject* pModel ); 
	void ClearBinding();

	void HideAttachment( uint32 x ) {	m_HideInMainPass=x;	HideInRecursion(x); HideInShadow(x); }
	uint32 IsAttachmentHidden() { return m_HideInMainPass; }
	void HideInRecursion( uint32 x ) {	m_HideInRecursion=x;	}
	uint32 IsAttachmentHiddenInRecursion() { return m_HideInRecursion; }
	void HideInShadow( uint32 x ) {	m_HideInShadowPass=x;	}
	uint32 IsAttachmentHiddenInShadow() { return m_HideInShadowPass; }

	void AlignBoneAttachment( uint32 x ) { m_AlignBoneAttachment=x; }



	uint32 ProjectAttachment();

	IAttachmentObject* GetIAttachmentObject() {	return m_pIAttachmentObject;	}
	void Serialize(TSerialize ser);

	//---------------------------------------------------------

	QuatT m_AttAbsoluteDefault;		//orientation of attachment in character-space when character is in default-pose
	QuatT m_AttRelativeDefault;		//this attachment matrix is relative to the facematrix/bonematrix in default pose;

	Quat	m_additionalRotation;		//additional rotation applied by the facial animation.

	QuatT	m_AttModelRelative;			//the position and orientation of the attachment in animated relative bone/face space 
	QuatT	m_AttModelRelativePrev; //the previous position and orientation of the attachment in animated relative bone/face space 
	QuatT	m_AttWorldAbsolute;			//the position and orientation of the attachment in animated character-space 
	Matrix34_f64 m_WMatrixD;			//the position and orientation of the attachment in animated character-space 

	string m_Name;
	IAttachmentObject* m_pIAttachmentObject;
	CAttachmentManager* m_pAttachmentManager;
	SAttachmentHinge *m_pHinge;
	DynArray<uint8> m_arrRemapTable;
	uint32 m_Type;

	uint32 m_FaceNr;				//only used for face-attachmnets (points to the internal-mesh)
	uint32 m_AttFaceNr;			//only used for face-attachmnets (points to the attachment-mesh)
	uint32 m_BoneID;				//bone-number. only used for bone-attachmnets
	uint32 m_AttFlags; 

	uint8 m_HideInMainPass;
	uint8 m_HideInShadowPass;
	uint8 m_HideInRecursion;
	uint8 m_AlignBoneAttachment;

	CAttachment() 
	{
		m_Type		=	CA_FACE;
		m_pIAttachmentObject	=	0;

		m_HideInMainPass=0;
		m_HideInShadowPass=0;
		m_HideInRecursion=0;

		m_AlignBoneAttachment=0;
		m_AttAbsoluteDefault.SetIdentity();
		m_AttRelativeDefault.SetIdentity();
		m_FaceNr	=	0;
		m_AttFaceNr	=	0;
		m_BoneID		= 0xfffffffe;

		m_AttModelRelative.SetIdentity();
		m_AttModelRelativePrev.SetIdentity();
		m_AttWorldAbsolute.SetIdentity();
		m_additionalRotation.SetIdentity();

		m_AttFlags  = 0;

    //m_pMaterialLayerManager = 0;
		m_pHinge = 0;
	};

	~CAttachment() { if (m_pHinge) delete m_pHinge; }

	size_t SizeOfThis();

	virtual void GetHingeParams(int &idx,float &limit,float &damping);
	virtual void SetHingeParams(int idx,float limit,float damping);
  
};

typedef std::vector<CAttachment*> AttachArray;




#endif
