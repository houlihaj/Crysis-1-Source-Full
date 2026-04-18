////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   material.h
//  Version:     v1.00
//  Created:     3/2/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __material_h__
#define __material_h__
#pragma once

#include "BaseLibraryItem.h"

typedef DynArray<SShaderParam> ShaderPublicParams;

// forward declarations,
class CMaterialManager;

/*
struct SMaterial
{
	bool m_bLighting;
	SLightMaterial m_lightMaterial;
	SShaderResources m_shaderResources;
	ShaderPublicParams m_shaderParams;
};
*/

/** CMaterial class
		Every Material is a member of material library.
		Materials can have child sub materials,
		Sub materials are applied to the same geometry of the parent material in the other material slots.
*/

struct SMaterialLayerResources
{
  SMaterialLayerResources(): m_nFlags( MTL_LAYER_USAGE_REPLACEBASE ), m_bRegetPublicParams(true), m_pMatLayer(0)
  {
  }
    
  uint8 m_nFlags;
  bool m_bRegetPublicParams;
  CString m_shaderName;
  
  _smart_ptr< IMaterialLayer > m_pMatLayer;  
  SInputShaderResources m_shaderResources;
  XmlNodeRef m_publicVarsCache;
};

class CRYEDIT_API CMaterial : public CBaseLibraryItem
{
public:
	//////////////////////////////////////////////////////////////////////////
	CMaterial( CMaterialManager *pManager,const CString &name,int nFlags=0 );
	~CMaterial();

	virtual EDataBaseItemType GetType() const { return EDB_TYPE_MATERIAL; };

	void SetName( const CString &name );

	//////////////////////////////////////////////////////////////////////////
	CString GetFullName() const { return m_name; };
	
	//////////////////////////////////////////////////////////////////////////
	// File properties of the material.
	//////////////////////////////////////////////////////////////////////////
	CString GetFilename();

	void UpdateFileAttributes();
	uint32 GetFileAttributes( bool bUpdateFromFile=true );
	//////////////////////////////////////////////////////////////////////////

	//! Sets one or more material flags from EMaterialFlags enum.
	void SetFlags( int flags ) { m_mtlFlags = flags; };
	//! Query this material flags.
	int GetFlags() const { return m_mtlFlags; }
	bool IsMultiSubMaterial() const { return m_mtlFlags&MTL_FLAG_MULTI_SUBMTL; };
	bool IsPureChild() const { return m_mtlFlags&MTL_FLAG_PURE_CHILD; }

	// Check if material is used.
	bool IsUsed() const { /*return m_nUseCount > 0 || (m_mtlFlags & MTL_FLAG_ALWAYS_USED);*/ return true; };

	virtual void GatherUsedResources( CUsedResources &resources );

	//! Set name of shader used by this material.
	void SetShaderName( const CString &shaderName );
	//! Get name of shader used by this material.
	CString GetShaderName() const { return m_shaderName; };

	SInputShaderResources& GetShaderResources() { return m_shaderResources; };

	//! Get public parameters of material in variable block.
	CVarBlock* GetPublicVars( SInputShaderResources &pShaderResources );

	//////////////////////////////////////////////////////////////////////////
	CVarBlock* GetShaderGenParamsVars();
	void SetShaderGenParamsVars( CVarBlock *pBlock );
	uint GetShaderGenMask() { return m_nShaderGenMask; }

	//! Sets variable block of public shader parameters.
	//! VarBlock must be in same format as returned by GetPublicVars().
	void SetPublicVars( CVarBlock *pPublicVars, SInputShaderResources &pInputShaderResources, IRenderShaderResources *pRenderShaderResources, IShader *pShader);

	//! Return variable block of shader params.
	SShaderItem& GetShaderItem() { return m_shaderItem; };

  //! Return material layers resources
  SMaterialLayerResources *GetMtlLayerResources() { return m_pMtlLayerResources; };
      
	//! Get texture map usage mask for shader in this material.
	unsigned int GetTexmapUsageMask() const;

	//! Load new shader.
	bool LoadShader();

	//! Reload shader, update all shader parameters.
	void Update();

	// Reload material settings from file.
	void Reload();

	//! Serialize material settings to xml.
	virtual void Serialize( SerializeContext &ctx );

	//! Assign this material to static geometry.
	void AssignToEntity( IRenderNode *pEntity );

	//////////////////////////////////////////////////////////////////////////
	// Surface types.
	//////////////////////////////////////////////////////////////////////////
	void SetSurfaceTypeName( const CString &surfaceType );
	const CString& GetSurfaceTypeName() { return m_surfaceType; };

	//////////////////////////////////////////////////////////////////////////
	// Child Sub materials.
	//////////////////////////////////////////////////////////////////////////
	//! Get number of sub materials childs.
	int GetSubMaterialCount() const;
	//! Set number of sub materials childs.
	void SetSubMaterialCount( int nSubMtlsCount );
	//! Get sub material child by index.
	CMaterial* GetSubMaterial( int index ) const;
	// Set a material to the sub materials slot.
	// Use NULL material pointer to clear slot.
	void SetSubMaterial( int nSlot,CMaterial *mtl );
	//! Remove all sub materials, does not change number of sub material slots.
	void ClearAllSubMaterials();

	//! Return pointer to engine material.
	IMaterial* GetMatInfo();
	// Clear stored pointer to engine material.
	void ClearMatInfo();

	//! Validate materials for errors.
	void Validate();

	// Check if material file can be modified.
	// Will check file attributes if it is not read only.
	bool CanModify();

	// Save material to file.
	bool Save();

	// Dummy material is just a placeholder item for materials that have not been found on disk.
	void SetDummy( bool bDummy ) { m_bDummyMaterial = bDummy; }
	bool IsDummy() const { return m_bDummyMaterial; }

	bool IsFromEngine() const { return m_bFromEngine; }

	// Called by material manager when material selected as a current material.
	void OnMakeCurrent();

	void SetFromMatInfo( IMaterial *pMatInfo );

	// Return parent material for submaterial
	CMaterial * GetParent(){return m_pParent;}
  
  //! Loads material layers
  bool LoadMaterialLayers();
  //! Updates material layers
  void UpdateMaterialLayers();  

private:
	void UpdateMatInfo();
	void RecordUndo( const char *sText );
	void SetShaderParamsFromXml( SInputShaderResources &pShaderResources, XmlNodeRef &node);
	void SetXmlFromShaderParams( SInputShaderResources &pShaderResources, XmlNodeRef &node );
	void ParsePublicParamsScript( const char *sScript,IVariable *pVar );
	void CheckSpecialConditions();

	void NotifyChanged();

	CMaterialManager* GetMatManager() { return m_pManager; };

private:
	// Material manager in which we registered.
	CMaterialManager *m_pManager;
	//////////////////////////////////////////////////////////////////////////
	// Variables.
	//////////////////////////////////////////////////////////////////////////
	CString m_shaderName;
	CString m_surfaceType;

	//! Material flags.
	int m_mtlFlags;

	// Parent material, Only valid for Pure Childs.
	CMaterial *m_pParent;

	//////////////////////////////////////////////////////////////////////////
	// Shader resources.
	//////////////////////////////////////////////////////////////////////////
	SShaderItem m_shaderItem;
	SInputShaderResources m_shaderResources;
	//CVarBlockPtr m_shaderParamsVar;
	//! Common shader flags.
	uint m_nShaderGenMask;

  SMaterialLayerResources m_pMtlLayerResources[MTL_LAYER_MAX_SLOTS];

	IMaterial *m_pMatInfo;
	
	XmlNodeRef m_publicVarsCache;

	//! Array of sub materials.
	std::vector<TSmartPtr<CMaterial> > m_subMaterials;

	int m_nUseCount;
	uint32 m_scFileAttributes;

	//! Material Used in level.
	int m_bDummyMaterial       : 1; // Dummy material, name specified but material file not found.
	int m_bFromEngine          : 1; // Material from engine.
	int m_bIgnoreNotifyChange  : 1; // Do not send notifications about changes.
	int m_bRegetPublicParams   : 1;
	int m_bKeepPublicParamsValues : 1;
};

#endif // __material_h__
