#ifndef _DECAL_RENDERNODE_
#define _DECAL_RENDERNODE_

#pragma once


#include "DecalManager.h"


class CDecalRenderNode : public IDecalRenderNode, public Cry3DEngineBase
{
public:
	// implements IDecalRenderNode
	virtual void SetDecalProperties( const SDecalProperties& properties );

	// implements IRenderNode
	virtual void SetMatrix( const Matrix34& mat );

	virtual EERType GetRenderNodeType();
	virtual const char* GetEntityClassName() const;
	virtual const char* GetName() const;
	virtual Vec3 GetPos( bool bWorldOnly = true ) const;
	virtual bool Render( const SRendParams &rParam );
	virtual IPhysicalEntity* GetPhysics() const;
	virtual void SetPhysics( IPhysicalEntity* );
	virtual void SetMaterial( IMaterial* pMat );
	virtual IMaterial* GetMaterial( Vec3* pHitPos = 0 );
	virtual float GetMaxViewDist();
	virtual void Precache();
	virtual void GetMemoryUsage( ICrySizer* pSizer );

  virtual const AABB GetBBox() const { return m_WSBBox; }
  virtual void SetBBox( const AABB& WSBBox ) { m_WSBBox = WSBBox; }

public:
	CDecalRenderNode();
	const SDecalProperties* GetDecalProperties() const;

private:
	~CDecalRenderNode();
	void DeleteDecals();
	void CreateDecals();
	void ProcessUpdateRequest();
  void UpdateAABBFromRenderMeshes();

	void CreatePlanarDecal( IMaterial* pMaterial );
	void CreateDecalOnStaticObjects( IMaterial* pMaterial );
	void CreateDecalOnTerrain( IMaterial* pMaterial );

private:
	Vec3 m_pos;
	AABB m_localBounds;
	_smart_ptr<IMaterial> m_pMaterial;
	bool m_updateRequested;
	SDecalProperties m_decalProperties;
	std::vector< CDecal* > m_decals;
  AABB m_WSBBox;
};


#endif // #ifndef _DECAL_RENDERNODE_