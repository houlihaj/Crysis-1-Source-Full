#ifndef _ROPE_RENDERNODE_
#define _ROPE_RENDERNODE_

#pragma once

#include <ISplines.h>

struct IRenderMesh;


class CRopeRenderNode : public IRopeRenderNode, public Cry3DEngineBase
{
public:
	//////////////////////////////////////////////////////////////////////////
	// implements IRenderNode
	virtual void GetLocalBounds( AABB& bbox );
	virtual void SetMatrix( const Matrix34& mat );

	virtual EERType GetRenderNodeType();
	virtual const char* GetEntityClassName() const;
	virtual const char* GetName() const;
	virtual Vec3 GetPos( bool bWorldOnly = true ) const;
	virtual bool Render( const SRendParams &rParam );
	virtual IPhysicalEntity* GetPhysics() const;
	virtual void SetPhysics( IPhysicalEntity* );
	virtual void Physicalize(bool bInstant=false);
	virtual void Dephysicalize(bool bKeepIfReferenced=false);
	virtual void SetMaterial( IMaterial* pMat );
	virtual IMaterial* GetMaterial( Vec3* pHitPos = 0 );
	virtual float GetMaxViewDist();
	virtual void Precache();
	virtual void GetMemoryUsage( ICrySizer* pSizer );
	virtual void LinkEndPoints();
  virtual const AABB GetBBox() const { return m_WSBBox; }
  virtual void SetBBox( const AABB& WSBBox ) { m_WSBBox = WSBBox; }

	virtual void   SetEntityOwner( uint32 nEntityId ) { m_nEntityOwnerId = nEntityId; }
	virtual uint32 GetEntityOwner() const { return m_nEntityOwnerId; }
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IRopeRenderNode implementation
	//////////////////////////////////////////////////////////////////////////
	virtual void SetName( const char *sNodeName );
	virtual void SetParams( const SRopeParams& params );
	virtual const SRopeParams& GetParams() const;

	virtual void SetPoints( const Vec3 *pPoints,int nCount );
	virtual int GetPointsCount() const;
	virtual const Vec3* GetPoints() const;

	virtual uint32 GetLinkedEndsMask() { return m_nLinkedEndsMask; };
	virtual void OnPhysicsPostStep();
	virtual void ForceInvalidate();

	virtual void ResetPoints();

	virtual void LinkEndEntities(IPhysicalEntity* pStartEntity, IPhysicalEntity* pEndEntity);
	virtual void GetEndPointLinks( SEndPointLink *links );
	//////////////////////////////////////////////////////////////////////////

public:
	CRopeRenderNode();

	void CreateRenderMesh();
	void UpdateRenderMesh();
	void AnchorEndPoints( pe_params_rope &pr );
	void SyncWithPhysicalRope( bool bForce );
	bool RenderDebugInfo( const SRendParams &rParams );

private:
	~CRopeRenderNode();

private:
	string m_sName;
	uint32 m_nEntityOwnerId;
	Vec3 m_pos;
	AABB m_localBounds;
	Matrix34 m_worldTM;
	Matrix34 m_InvWorldTM;
	_smart_ptr<IMaterial> m_pMaterial;
	IRenderMesh* m_pRenderMesh;
	IPhysicalEntity *m_pPhysicalEntity;
	
	uint32 m_nLinkedEndsMask;
	
	// Flags
	uint32 m_bModified : 1;
	uint32 m_bRopeCreatedInsideVisArea : 1;
	uint32 m_bNeedToReRegister : 1;
	uint32 m_bStaticPhysics : 1;

	std::vector<Vec3> m_points;
	std::vector<Vec3> m_physicsPoints;

	SRopeParams m_params;

	typedef spline::CatmullRomSpline<Vec3> SplineType;
	SplineType m_spline;

  AABB m_WSBBox;
};


#endif // _ROPE_RENDERNODE_