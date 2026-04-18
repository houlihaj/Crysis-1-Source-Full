#ifndef _AUTO_CUBEMAP_RENDERNODE_
#define _AUTO_CUBEMAP_RENDERNODE_

#pragma once


class CAutoCubeMapRenderNode : public IAutoCubeMapRenderNode, public Cry3DEngineBase
{
public:
	// implements IAutoCubeMapRenderNode
	virtual void SetProperties(const SAutoCubeMapProperties& properties);
	virtual const SAutoCubeMapProperties& GetProperties() const;
	virtual uint32 GetID() const;
	virtual void SetPrivateData(const void* pData);
	virtual const void* GetPrivateData() const;

	// implements IRenderNode
	virtual void SetMatrix(const Matrix34& mat);

	virtual EERType GetRenderNodeType();
	virtual const char* GetEntityClassName() const;
	virtual const char* GetName() const;
	virtual Vec3 GetPos(bool bWorldOnly = true) const;
	virtual bool Render(const SRendParams& rParam);
	virtual IPhysicalEntity* GetPhysics() const;
	virtual void SetPhysics(IPhysicalEntity*);
	virtual void SetMaterial(IMaterial* pMat);
	virtual IMaterial* GetMaterial(Vec3* pHitPos = 0);
	virtual float GetMaxViewDist();
	virtual void Precache();
	virtual void GetMemoryUsage(ICrySizer* pSizer);
  virtual const AABB GetBBox() const { return m_WSBBox; }
  virtual void SetBBox( const AABB& WSBBox ) { m_WSBBox = WSBBox; }

public:
	CAutoCubeMapRenderNode();

private:
	~CAutoCubeMapRenderNode();
	void UpdateBoundingInfo();

private:
	static uint32 ms_acmGenID;

private:
	uint32 m_id;
	SAutoCubeMapProperties m_properties;
	const void* m_pPrivateData;
  AABB m_WSBBox;
};


#endif // __AUTO_CUBEMAP_RENDERNODE__