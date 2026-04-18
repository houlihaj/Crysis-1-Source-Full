//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File:DecalManager.cpp
//  Implementation of DecalManager class
//
//	History:
//	August 16, 2004: Created by Ivo Herzeg <ivo@crytek.de>
//
//////////////////////////////////////////////////////////////////////

#ifndef _CRY_CHAR_DECAL_MANAGER_HDR_
#define _CRY_CHAR_DECAL_MANAGER_HDR_

#include "ModelMesh.h"


// this is the simple decal vertex - the index of the original vertex and UV coordinates
struct CDecalVertex
{
	uint32 nVertex; //index for the Internal Skin Vertices
	f32 u,v;				//the UV coordinates of the decal
};

//////////////////////////////////////////////////////////////////////////
// the decal structure describing the realized decal with the geometry
// it consists of primary faces/vertices (with the full set of getter methods declared with the special macro)
// and helper faces/vertices. Primary faces index into the array of primary vertices. Primary vertices
// refer to the indices of the character vertices and contain the new UVs
// Helper vertices are explicitly set in the object CS, and helper faces index within those helper vertex array.
class CAnimDecal
{
public:

	// initializes it; expects the decal to come with vPos in LCS rather than WCS
	CAnimDecal() {}

	// sets the current game time for decals (for fading in / out)
	static void setGlobalTime (f32 fTime) {g_fCurrTime = fTime;}

	// starts fading out the decal from this moment on
	void startFadeOut(f32 fTimeout);

	// returns true if this is a dead/empty decal and should be discarded
	bool isDead();

	// returns the decal multiplier: 0 - no decal, 1 - full decal size
	f32 getIntensity()const;

	uint32 GetSize()
	{
		return sizeof(CAnimDecal) + m_arrFaces.capacity() * sizeof(TFace) + m_arrVertices.capacity() * sizeof(CDecalVertex);
	}
	// the texture of the decal
	//int m_nTextureId;
	// the point, in LCS , where the shot was made
	Vec3 m_ptSource;
	// this is the time when the decal was built, in seconds
	f32 m_fBuildTime;
	// this is the fade in time, in seconds
	f32 m_fFadeInTime;
	// this is the fade out time, in seconds; <0 at the start (means no fade out)
	f32 m_fFadeOutTime;
	// this is the time by which the decal should be faded out; 1e+30 at the start (very long life)
	f32 m_fDeathTime;

	// the faces comprising the decal
	// the face that's the center of the decal is 0th
	std::vector<TFace> m_arrFaces;
	// the mapping of the new vertices to the old ones
	std::vector<CDecalVertex> m_arrVertices;

	_smart_ptr<IMaterial> m_pMaterial;
};











// This class manages decals on characters.
// It memorizes requests for decals, then realizes them using the character skin
// geometry (constructs the decal face strip, UV-maps it)
//
// The workflow is as follows:
//   Request(Add) the decal. At this stage, its info will just be memorized, it won't be present in the decal list
//   Realize the decal. At this stage, the deformable geometry will be created for the decal.
//   Render the decal. After realization, the decals have their own geometry that bases on the deformed vertices
//   and normals of the character skin.
class CAnimDecalManager
{
public:

	CAnimDecalManager ()	{	}

	~CAnimDecalManager ()	{ }

	size_t SizeOfThis ();

	void CreateDecalsOnInstance(QuatTS* parrNewSkinQuat, CCharacterModel* pModel, CMorphing* pMorphing, f32* pShapeDeform, CryEngineDecalInfo& DecalLCS, const QuatT& rPhysEntityLocation );

	// discards the decal request queue (not yet realized decals added through Add())
	void RemoveObsoleteDecals();


	// starts fading out all decals that are close enough to the given point
	// NOTE: the radius is m^2 - it's the square of the radius of the sphere
	void fadeOutCloseDecals (const Vec3& ptCenter, f32 fRadius2);
	// cleans up all decals, destroys the vertex buffer
	void clear();

	// the array of requested (unrealized) decals
	std::vector<CryEngineDecalInfo> m_arrDecalRequests;
	// the array of realized decals
	std::vector<CAnimDecal> m_arrDecals;


};

#endif