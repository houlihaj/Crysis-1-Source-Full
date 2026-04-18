////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   decalmanager.h
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef DECAL_MANAGER
#define DECAL_MANAGER

#define DECAL_COUNT (512) // must be pow2
#define ENTITY_DECAL_DIST_FACTOR (200)
#define DIST_FADING_FACTOR (6)

class C3DEngine;

enum EDecal_Type
{
	eDecalType_Undefined,
	eDecalType_OS_OwnersVerticesUsed,
	eDecalType_WS_Merged,
	eDecalType_WS_OnTheGround,
	eDecalType_WS_SimpleQuad,
	eDecalType_OS_SimpleQuad
};

class CDecal : public Cry3DEngineBase
{
public:

  // cur state
  Vec3					m_vPos;
  Vec3					m_vRight, m_vUp, m_vFront;
  float					m_fSize;
	Vec3					m_vWSPos;													// Decal position (world coordinates) from DecalInfo.vPos
	float					m_fWSSize;												// Decal size (world coordinates) from DecalInfo.fSize

	// life style
  float					m_fLifeTime;											// relative time left till decal should die
  Vec3					m_vAmbient;												// ambient color
	SDecalOwnerInfo	m_ownerInfo;
	EDecal_Type		m_eDecalType;
	float					m_fGrowTime;											// e.g. growing blood pools
	float					m_fLifeBeginTime;									//
	uint8					m_sortPrio;

	// render data
	IRenderMesh * m_pRenderMesh;							// only needed for terrain decals, 4 of them because they might cross borders
	float					m_arrBigDecalRMCustomData[16];		// only needed if one of m_arrBigDecalRMs[]!=0, most likely we can reduce to [12]

	bool					m_isTempMat;
	_smart_ptr< IMaterial > m_pMaterial;
  uint          m_nGroupId; // used for multi-component decals

#ifdef _DEBUG
	char m_decalOwnerEntityClassName[256];
	char m_decalOwnerName[256];
	EERType m_decalOwnerType;
#endif

	CDecal()
	: m_vPos( 0, 0, 0 ), m_vRight( 0, 0, 0 ), m_vUp( 0, 0, 0 ), m_vFront( 0, 0, 0 )
	, m_fSize( 0 ), m_vWSPos( 0, 0, 0 ), m_fWSSize( 0 )
	, m_fLifeTime( 0 )
	, m_vAmbient( 0, 0, 0 )
	, m_fGrowTime( 0 )
	, m_fLifeBeginTime( 0 )
	, m_sortPrio( 0 )
	, m_isTempMat( false )
	, m_pMaterial( 0 )
  , m_nGroupId( 0 )
	{
		m_eDecalType = eDecalType_Undefined;
		m_pRenderMesh = NULL;
		memset( &m_arrBigDecalRMCustomData[ 0 ], 0, sizeof( m_arrBigDecalRMCustomData ) );

#ifdef _DEBUG
		m_decalOwnerEntityClassName[0] = '\0';
		m_decalOwnerName[0] = '\0';
		m_decalOwnerType = eERType_Unknown;
#endif
	}

	~CDecal()
	{
		FreeRenderData();
	}

  void Render(const float fFrameTime, int nAfterWater, uint nDLMask, float fDistanceFading);
  int Update( bool & active, const float fFrameTime );
	void RenderBigDecalOnTerrain(float fAlpha, float fScale, uint nDLMask);
	void FreeRenderData();
	bool IsBigDecalUsed() const { return m_pRenderMesh!=0; }
  Vec3 GetWorldPosition();
};

class CDecalManager : public Cry3DEngineBase
{
  CDecal						m_arrDecals[DECAL_COUNT];
  bool							m_arrbActiveDecals[DECAL_COUNT];
  int								m_nCurDecal;

public: // ---------------------------------------------------------------
  
  CDecalManager();
	~CDecalManager();
  bool Spawn(CryEngineDecalInfo Decal, float fMaxViewDistance, CTerrain* pTerrain, CDecal* pCallerManagedDecal = 0 );
	// once per frame
  void Update( const float fFrameTime );
	// maybe multiple times per frame
  void Render();
	void OnEntityDeleted(IRenderNode * pEnt);
	void OnRenderMeshDeleted(IRenderMesh * pRenderMesh);
	
	// complex decals
	void FillBigDecalIndices(IRenderMesh * pRenderMesh, Vec3 vPos, float fRadius, Vec3 vProjDir, PodArray<ushort> * plstIndices, IMaterial * pMat, AABB & meshBBox);
	IRenderMesh * MakeBigDecalRenderMesh(IRenderMesh * pSourceRenderMesh, Vec3 vPos, float fRadius, Vec3 vProjDir, IMaterial* pDecalMat, IMaterial* pSrcMat);
	void GetMemoryUsage(ICrySizer*pSizer);
	void Reset() { memset(m_arrbActiveDecals,0,sizeof(m_arrbActiveDecals)); m_nCurDecal=0; }
	void DeleteDecalsInRange( AABB * pAreaBox, IRenderNode * pEntity );
	bool AdjustDecalPosition( CryEngineDecalInfo & DecalInfo, bool bMakeFatTest );
	static bool RayRenderMeshIntersection(IRenderMesh * pRenderMesh, const Vec3 & vInPos, const Vec3 & vInDir, Vec3 & vOutPos, Vec3 & vOutNormal,bool bFastTest,float fMaxHitDistance,IMaterial * pMat);
	void Serialize( TSerialize ser );
  bool SpawnHierarhical( const CryEngineDecalInfo & rootDecalInfo, float fMaxViewDistance, CTerrain* pTerrain, CDecal* pCallerManagedDecal );

private:
	IMaterial* GetMaterialForDecalTexture( const char* pTextureName );
};

#endif // DECAL_MANAGER
