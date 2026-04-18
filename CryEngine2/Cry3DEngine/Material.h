////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   Material.h
//  Version:     v1.00
//  Created:     3/9/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __Material_h__
#define __Material_h__
#pragma once

class CMaterialLayer : public IMaterialLayer
{
public:
  CMaterialLayer(): m_nRefCount( 0 ), m_nFlags( 0 )
  {
  }

  ~CMaterialLayer()
  {
    SAFE_RELEASE(m_pShaderItem.m_pShader);
    SAFE_RELEASE(m_pShaderItem.m_pShaderResources);
  }

  virtual void AddRef() 
  { 
    m_nRefCount++; 
  };

  virtual void Release()
  {
    if (--m_nRefCount <= 0)
    {
      delete this;
    }
  }

  virtual void Enable( bool bEnable = true)
  {
    m_nFlags |= (bEnable == false)? MTL_LAYER_USAGE_NODRAW : 0;
  }

  virtual bool IsEnabled() const
  {
    return ( m_nFlags & MTL_LAYER_USAGE_NODRAW )? false: true;
  }

  virtual void SetShaderItem( const IMaterial *pParentMtl, const SShaderItem & pShaderItem );

  virtual const SShaderItem &GetShaderItem() const
  {
    return m_pShaderItem;
  }

  virtual SShaderItem &GetShaderItem()
  {
    return m_pShaderItem;
  }

  virtual void SetFlags( uint8 nFlags )
  {
    m_nFlags = nFlags;
  }

  virtual uint8 GetFlags( ) const
  {
    return m_nFlags;
  }

private:
  uint8 m_nFlags;  
  int m_nRefCount;
  SShaderItem m_pShaderItem;
};

//////////////////////////////////////////////////////////////////////
class CMatInfo : public IMaterial, public Cry3DEngineBase
{
public: 
	CMatInfo();
	~CMatInfo();

	virtual void AddRef() { m_nRefCount++; };
	virtual void Release()
	{
		if (--m_nRefCount <= 0)
			delete this;
	}
	virtual int GetNumRefs() { return m_nRefCount; };

	//////////////////////////////////////////////////////////////////////////
	// IMaterial implementation
	//////////////////////////////////////////////////////////////////////////
	// material name
	int Size();

	virtual IMaterialManager *GetMaterialManager();

	virtual void SetName( const char *pName );
	virtual const char* GetName() { return m_sMaterialName; };

	virtual void SetFlags( int flags ) { m_Flags = flags; };
	virtual int GetFlags() const { return m_Flags; };

	// Returns true if this is the default material.
	virtual bool IsDefault();

	virtual int GetSurfaceTypeId() { return m_nSurfaceTypeId; };

	virtual void SetSurfaceType( const char *sSurfaceTypeName );
	virtual ISurfaceType* GetSurfaceType();

	// shader item
	
	virtual void SetShaderItem( const SShaderItem & _ShaderItem);
// [Alexey] EF_LoadShaderItem return value with RefCount = 1, so if you'll use SetShaderItem after EF_LoadShaderItem use Assign function
	virtual void AssignShaderItem( const SShaderItem & _ShaderItem);
	virtual SShaderItem & GetShaderItem() { return m_shaderItem; };

	virtual SShaderItem& GetShaderItem( int nSubMtlSlot );

	// shader params
	virtual void SetShaderParams(TArray<SShaderParam> * _pShaderParams) { pShaderParams = _pShaderParams; }
	virtual const TArray<SShaderParam> * GetShaderParams() { return pShaderParams; }

	//////////////////////////////////////////////////////////////////////////
	virtual bool SetGetMaterialParamFloat( const char *sParamName,float &v,bool bGet );
	virtual bool SetGetMaterialParamVec3( const char *sParamName,Vec3 &v,bool bGet );

	virtual void SetCamera( CCamera &cam );

	//////////////////////////////////////////////////////////////////////////
	// Sub materials.
	//////////////////////////////////////////////////////////////////////////
	virtual void SetSubMtlCount( int numSubMtl );
	virtual int GetSubMtlCount();
	virtual IMaterial* GetSubMtl( int nSlot );
	virtual void SetSubMtl( int nSlot,IMaterial *pMtl );
	virtual void SetUserData( void *pUserData );
	virtual void* GetUserData() const;

	virtual IMaterial* GetSafeSubMtl( int nSlot );
  virtual IMaterial* Clone();

  //////////////////////////////////////////////////////////////////////////
  // Layers
  //////////////////////////////////////////////////////////////////////////
  virtual void SetLayerCount( uint32 nCount );
  virtual uint32 GetLayerCount() const;
  virtual void SetLayer( uint32 nSlot, IMaterialLayer *pLayer);
  virtual const IMaterialLayer* GetLayer( uint8 nLayersMask, uint8 nLayersUsageMask ) const;
  virtual const IMaterialLayer* GetLayer( uint32 nSlot ) const;
  virtual IMaterialLayer *CreateLayer();
	
	// For backward compatibility with OLD cgf files.
	void LoadFromMatEntity( MAT_ENTITY *me,const char *szFolderName );

	// Fill int table with surface ids of sub materials.
	// Return number of filled items.
	int FillSurfaceTypeIds( int pSurfaceIdsTable[] );
	virtual bool IsSubSurfScatterCaster() const;
	virtual float GetPSRefractionAmount() const;

	virtual void GetMemoryUsage( ICrySizer *pSizer );

	//////////////////////////////////////////////////////////////////////////
	void SetSketchMode( int mode );

	bool IsPerObjectShadowPassNeeded();

private:
	friend class CMatMan;
	friend class CMaterialLayer;

	//////////////////////////////////////////////////////////////////////////
	string m_sMaterialName;
	string m_sUniqueMaterialName;
	
	// Id of surface type assigned to this material.
	int m_nSurfaceTypeId;

	//! Number of references to this material.
	int m_nRefCount;
	//! Material flags.
	//! @see EMatInfoFlags
	int m_Flags;

	SShaderItem m_shaderItem;
	_smart_ptr<IShader> m_pPreSketchShader;

	float fAlpha;

	TArray<SShaderParam> *pShaderParams;

	//! Array of Sub materials.
	typedef DynArray<_smart_ptr<CMatInfo> > SubMtls;
	SubMtls m_subMtls;

	// User data used by Editor.
	void *m_pUserData;

  //! Material layers
  typedef std::vector< _smart_ptr< CMaterialLayer > > MatLayers;
  MatLayers *m_pMaterialLayers;
  
  //! Used for material layers
  mutable uint8 m_nActiveLayerMask;
  mutable uint8 m_nActiveLayerUsageMask;
  mutable CMaterialLayer *m_pActiveLayer;

	//////////////////////////////////////////////////////////////////////////
};

#endif // __Material_h__

