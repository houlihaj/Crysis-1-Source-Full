////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   MatMan.h
//  Version:     v1.00
//  Created:     23/8/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __MatMan_h__
#define __MatMan_h__
#pragma once

#include "Cry3DEngineBase.h"
#include "SurfaceTypeManager.h"

// forward declarations.
struct IMaterial;
struct ISurfaceType;
struct ISurfaceTypeManager;
class CMatInfo;


//////////////////////////////////////////////////////////////////////////
//
// CMatMan is a material manager class.
//
//////////////////////////////////////////////////////////////////////////
class CMatMan : public IMaterialManager,public ISystemEventListener, public Cry3DEngineBase
{
public:
	CMatMan();
	virtual ~CMatMan();

	// interface IMaterialManager --------------------------------------------------------

	virtual IMaterial* CreateMaterial( const char *sMtlName,int nMtlFlags=0 );
	virtual IMaterial* FindMaterial( const char *sMtlName ) const;
	virtual IMaterial* LoadMaterial( const char *sMtlName,bool bMakeIfNotFound=true, bool bNonremovable = false );
	virtual void SetListener( IMaterialManagerListener *pListener ) { m_pListener = pListener; };
	virtual IMaterial* GetDefaultMaterial();
	virtual IMaterial* GetDefaultTerrainLayerMaterial();
  virtual IMaterial* GetDefaultLayersMaterial();
	virtual IMaterial* GetDefaultHelperMaterial();
	virtual ISurfaceType* GetSurfaceTypeByName( const char *sSurfaceTypeName,const char *sWhy=NULL );
	virtual int GetSurfaceTypeIdByName( const char *sSurfaceTypeName,const char *sWhy=NULL );
	virtual ISurfaceType* GetSurfaceType( int nSurfaceTypeId,const char *sWhy=NULL );
	virtual ISurfaceTypeManager* GetSurfaceTypeManager() { return m_pSurfaceTypeManager; }
	virtual IMaterial* LoadCGFMaterial( CMaterialCGF *pMaterialCGF,const char *sCgfFilename );
	virtual IMaterial* CloneMaterial( IMaterial* pMtl,int nSubMtl=-1 );
	virtual IMaterial* CloneMultiMaterial( IMaterial* pMtl,const char *sSubMtlName=0 );
	virtual void GetLoadedMaterials( IMaterial **pData, uint32 &nObjCount ) const;

	// ------------------------------------------------------------------------------------

	// interface ISystemEventListener -------------------------------------------------------
	virtual void OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam );
	// ------------------------------------------------------------------------------------


	void InitDefaults();

	void PrecacheDecalMaterials();
	void ClearDecalMaterials();

	void LoadMaterialsLibrary( const char *sMtlFile,XmlNodeRef &levelDataRoot );
	void SetSketchMode( int mode );
	int  GetSketchMode() { return e_sketch_mode; }
	
	//////////////////////////////////////////////////////////////////////////
	ISurfaceType* GetSurfaceTypeFast( int nSurfaceTypeId,const char *sWhy=NULL ) { return m_pSurfaceTypeManager->GetSurfaceTypeFast(nSurfaceTypeId,sWhy); }

private: // -----------------------------------------------------------------------------

	friend class CMatInfo;
	void Unregister( CMatInfo *pMat );
	void RenameMaterial( IMaterial *pMtl,const char *sNewName );
	IMaterial* LoadMaterialFromXml( XmlNodeRef mtlNode,const char *sLibraryName,bool bPureChild=false );

  bool LoadMaterialShader( IMaterial *pMtl, const char *sShader, uint nShaderGenMask, SInputShaderResources &sr, XmlNodeRef &publicsNode );
  bool LoadMaterialLayerSlot( uint32 nSlot, IMaterial *pMtl, const char *szShaderName, SInputShaderResources &pBaseResources, XmlNodeRef &pPublicsNode, uint8 nLayerFlags);

	void ParsePublicParams( SInputShaderResources &sr,XmlNodeRef paramsNode );
	const char* UnifyName( const char *sMtlName ) const;
	// Can be called after material creation and initialization, to inform editor that new material in engine exist.
	// Only used internally.
	void NotifyCreateMaterial( IMaterial *pMtl );
	// Make a valid material from the XML node.
	IMaterial* MakeMaterialFromXml( const char *sMtlName,XmlNodeRef node,bool bForcePureChild );

	// ---------------------------------------------------------------------

//	typedef std::set<IMaterial*> MtlSet;
	typedef std::map<string,IMaterial*> MtlNameMap;

//	MtlSet															m_mtlSet;											//
	MtlNameMap													m_mtlNameMap;									//

	IMaterialManagerListener *					m_pListener;									//
	_smart_ptr<CMatInfo>								m_pDefaultMtl;								//
  _smart_ptr<IMaterial>								m_pDefaultLayersMtl;					//
	_smart_ptr<IMaterial>								m_pDefaultTerrainLayersMtl;		//
	_smart_ptr<CMatInfo>								m_pNoDrawMtl;									//
	_smart_ptr<CMatInfo>                m_pDefaultHelperMtl;

	std::vector<_smart_ptr<CMatInfo> >	m_nonRemovables;							//
	std::vector<_smart_ptr<CMatInfo> >	m_precachedDecalMaterial;		  //

	CSurfaceTypeManager *								m_pSurfaceTypeManager;				//

	//////////////////////////////////////////////////////////////////////////
	// Cached XML parser.
	_smart_ptr<IXmlParser> m_pXmlParser;

	static int e_sketch_mode;
  static int e_lowspec_mode;
	static int e_pre_sketch_spec;
};

#endif // __MatMan_h__

