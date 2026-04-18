////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   MatMan.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Material Manager Implementation
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "MatMan.h"
#include "Material.h"
#include "3dEngine.h"
#include "ObjMan.h"
#include "IRenderer.h"
#include "SurfaceTypeManager.h"
#include "CGFContent.h"

#define MATERIAL_EXT ".mtl"
#define MATERIAL_NODRAW "nodraw"

#define MATERIAL_DECALS_FOLDER "Materials/Decals"

int CMatMan::e_sketch_mode = 0;
int CMatMan::e_pre_sketch_spec = 0;
int CMatMan::e_lowspec_mode = 0;

static void OnSketchModeChange( ICVar *pVar )
{
	int mode = pVar->GetIVal();
	((CMatMan*)gEnv->p3DEngine->GetMaterialManager())->SetSketchMode( mode );
}

//////////////////////////////////////////////////////////////////////////
CMatMan::CMatMan()
{
	m_pListener = NULL;
	m_pDefaultMtl = NULL;
	m_pDefaultTerrainLayersMtl = NULL;
  m_pDefaultLayersMtl = NULL;
	m_pDefaultHelperMtl = NULL;
	m_pNoDrawMtl = NULL;
	Cry3DEngineBase::m_pMaterialManager = this;

	m_pSurfaceTypeManager = new CSurfaceTypeManager(GetSystem());

	ICVar *pCVar = GetConsole()->Register( "e_sketch_mode",&e_sketch_mode,0,VF_CHEAT,"Enables Sketch mode drawing" );  
	pCVar->SetOnChangeCallback( &OnSketchModeChange );

  ICVar *pCVarLowSpec = GetConsole()->Register( "e_lowspec_mode",&e_lowspec_mode,0,VF_CHEAT,"Enables lowspec mode" );

	m_pXmlParser = GetISystem()->GetXmlUtils()->CreateXmlParser();

	//InitDefaults();
}

//////////////////////////////////////////////////////////////////////////
CMatMan::~CMatMan()
{
	delete m_pSurfaceTypeManager;
	int nNotUsed=0, nNotUsedParents=0;
	m_pDefaultMtl = NULL;
	m_pDefaultTerrainLayersMtl = NULL;
  m_pDefaultLayersMtl = NULL;
	m_pDefaultHelperMtl = NULL;

	/*
  for (MtlSet::iterator it = m_mtlSet.begin(); it != m_mtlSet.end(); ++it)
  {
    IMaterial *pMtl = *it;
    SShaderItem Sh = pMtl->GetShaderItem();
		if(Sh.m_pShader)
		{
			Sh.m_pShader->Release();
			Sh.m_pShader = 0;
		}
    if(Sh.m_pShaderResources)
		{
			Sh.m_pShaderResources->Release();
			Sh.m_pShaderResources = 0;
		}
		pMtl->SetShaderItem( Sh );

		if(!(pMtl->GetFlags()&MIF_CHILD))
		if(!(pMtl->GetFlags()&MIF_WASUSED))
		{
			PrintMessage("Warning: CMatMan::~CMatMan: Material was loaded but never used: %s", pMtl->GetName());
			nNotUsed += (pMtl->GetSubMtlCount()+1);
			nNotUsedParents++;
		}
		if (pMtl->GetNumRefs() > 1)
		{
			//
			PrintMessage("Warning: CMatMan::~CMatMan: Material %s is being referenced", pMtl->GetName());
		}
  }
	*/

	if(nNotUsed)
		PrintMessage("Warning: CMatMan::~CMatMan: %d(%d) of %d materials was not used in level", 
		nNotUsedParents, nNotUsed, m_mtlNameMap.size());
}

//////////////////////////////////////////////////////////////////////////
const char* CMatMan::UnifyName( const char *sMtlName ) const
{
	static char name[260];
	int n = strlen(sMtlName);

	//TODO: change this code to not use a statically allocated buffer and return it ...
	//TODO: provide a general name unification function, which can be used in other places as well and remove this thing
	if (n>=260)
		Error("Static buffer size exceeded by material name!");

	for (int i = 0; i < n && i < sizeof(name); i++)
	{
		if (sMtlName[i] != '\\')
			name[i] = __ascii_tolower(sMtlName[i]);
		else
			name[i] = '/';
	}
	name[n] = '\0';
	return name;
}

//////////////////////////////////////////////////////////////////////////
IMaterial * CMatMan::CreateMaterial( const char *sMtlName,int nMtlFlags )
{
	CMatInfo *pMat = new CMatInfo;

	//m_mtlSet.insert( pMat );
	pMat->SetName( sMtlName );
	pMat->SetFlags( nMtlFlags | pMat->GetFlags() );
	if (!(nMtlFlags & MTL_FLAG_PURE_CHILD))
	{
		m_mtlNameMap[ UnifyName(sMtlName) ] = pMat;
	}

	if (nMtlFlags & MTL_FLAG_NON_REMOVABLE)
	{
		// Add reference to this material to prevent its deletion.
		m_nonRemovables.push_back( pMat );
	}

	return pMat;
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::NotifyCreateMaterial( IMaterial *pMtl )
{
	if (m_pListener)
		m_pListener->OnCreateMaterial( pMtl );
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::Unregister( CMatInfo * pMat )
{
	assert( pMat );

	if (!(pMat->m_Flags & MTL_FLAG_PURE_CHILD))
		m_mtlNameMap.erase( CONST_TEMP_STRING(UnifyName(pMat->GetName())) );

	if (m_pListener)
		m_pListener->OnDeleteMaterial(pMat);
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::RenameMaterial( IMaterial *pMtl,const char *sNewName )
{
	assert( pMtl );
	const char *sName = pMtl->GetName();
	if (*sName != '\0')
	{
		m_mtlNameMap.erase( CONST_TEMP_STRING(UnifyName(pMtl->GetName())) );
	}
	pMtl->SetName( sNewName );
	m_mtlNameMap[ UnifyName(sNewName) ] = pMtl;
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::FindMaterial( const char *sMtlName ) const
{
	const char *name = UnifyName(sMtlName);

	MtlNameMap::const_iterator it = m_mtlNameMap.find(CONST_TEMP_STRING(name));

	if(it==m_mtlNameMap.end())
		return 0;

	return it->second;
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::LoadMaterial( const char *sMtlName,bool bMakeIfNotFound, bool bNonremovable )
{
	LOADING_TIME_PROFILE_SECTION(GetSystem());

	const char *name = UnifyName(sMtlName);

	MtlNameMap::const_iterator it = m_mtlNameMap.find(CONST_TEMP_STRING(name));

	IMaterial *pMtl = 0;

	if(it!=m_mtlNameMap.end()) {
		pMtl = it->second;
		return pMtl;
	}

	if (!pMtl)
	{
		if (m_pListener)
		{
			pMtl = m_pListener->OnLoadMaterial( sMtlName,bMakeIfNotFound );
			if (pMtl)
			{
				if (bNonremovable)
					m_nonRemovables.push_back(static_cast<CMatInfo*>(pMtl));

				if (pMtl && e_sketch_mode != 0)
				{
					((CMatInfo*)pMtl)->SetSketchMode(e_sketch_mode);
				}
				return pMtl;
			}
		}
		// Try to load material from file.
		string filename = name;
		if (filename.find('.') == string::npos)
			filename += MATERIAL_EXT;

		static int nRecursionCounter = 0;

		bool bCanCleanPools = nRecursionCounter == 0; // Only clean pool, when we are called not recursively.
		nRecursionCounter++;

		XmlNodeRef mtlNode;
		if (m_pXmlParser)
		{
			// WARNING!!!
			// Shared XML parser does not support recursion!!!
			// On parsing a new XML file all previous XmlNodes are invalidated.
			mtlNode = m_pXmlParser->ParseFile( filename.c_str(),bCanCleanPools );
		}
		else
			mtlNode = GetSystem()->LoadXmlFile( filename.c_str() );
		if (mtlNode)
		{
			pMtl = MakeMaterialFromXml( name,mtlNode,false );
			
			if (pMtl && e_sketch_mode != 0)
			{
				((CMatInfo*)pMtl)->SetSketchMode(e_sketch_mode);
			}
		}

		nRecursionCounter--;
	}
	if (!pMtl && bMakeIfNotFound)
	{
		pMtl = m_pDefaultMtl;
	}

	if (bNonremovable && pMtl)
		m_nonRemovables.push_back(static_cast<CMatInfo*>(pMtl));

	return pMtl;
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::LoadMaterialsLibrary( const char *sMtlFile,XmlNodeRef &levelDataRoot )
{
	PrintMessage("Loading materials ...");

	// load environment settings
	if (!levelDataRoot)
		return;
	
	XmlNodeRef mtlLibs = levelDataRoot->findChild( "MaterialsLibrary" );
	if (mtlLibs)
	{
		// Enumerate material libraries.
		for (int i = 0; i < mtlLibs->getChildCount(); i++)
		{
			XmlNodeRef mtlLib = mtlLibs->getChild(i);
			XmlString libraryName = mtlLib->getAttr( "Name" );
			for (int j =0; j < mtlLib->getChildCount(); j++)
			{
				XmlNodeRef mtlNode = mtlLib->getChild(j);
				LoadMaterialFromXml( mtlNode,libraryName.c_str() );
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Loading from external materials.xml file.
	//////////////////////////////////////////////////////////////////////////
	mtlLibs = GetSystem()->LoadXmlFile( sMtlFile );
	if (mtlLibs)
	{
		// Enumerate material libraries.
		for (int i = 0; i < mtlLibs->getChildCount(); i++)
		{
			XmlNodeRef mtlLib = mtlLibs->getChild(i);
			if (!mtlLib->isTag("Library"))
				continue;
			XmlString libraryName = mtlLib->getAttr( "Name" );
			for (int j =0; j < mtlLib->getChildCount(); j++)
			{
				XmlNodeRef mtlNode = mtlLib->getChild(j);
				LoadMaterialFromXml( mtlNode,libraryName.c_str() );
			}
		}
	}

  PrintMessage(" %d mats loaded", m_mtlNameMap.size());
}

static struct
{
	int texId;
	const char *name;
} sUsedTextures[] =
{
	{ EFTT_DIFFUSE,		"Diffuse" },
	{ EFTT_GLOSS,			"Specular" },
	{ EFTT_BUMP,			"Bumpmap" },
	{ EFTT_NORMALMAP,	"Normalmap" },
	{ EFTT_ENV,		    "Environment" },
	{ EFTT_DETAIL_OVERLAY,"Detail" },
	{ EFTT_OPACITY,		"Opacity" },
	{ EFTT_DECAL_OVERLAY,	"Decal" },
  { EFTT_SUBSURFACE,	"SubSurface" },
  { EFTT_CUSTOM,	"Custom" },
  { EFTT_CUSTOM_SECONDARY,	"[1] Custom" },
};
inline ColorF ToCFColor( const Vec3 &col ) { return ColorF(col); }

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::LoadMaterialFromXml( XmlNodeRef node,const char *sLibraryName,bool bPureChild )
{
	XmlString name,shaderName,texmap,file,surfaceType;
	int mtlFlags = 0;
	unsigned int nShaderGenMask = 0;
	SInputShaderResources sr;

	// Make new mat info.
	name = node->getAttr( "Name" );

	if (bPureChild)
	{
		name += " <SubMtl>";
	}

	IMaterial* pMtl = NULL;
	if (!bPureChild)
		pMtl = CreateMaterial( name.c_str() );
	else
	{
		pMtl = new CMatInfo();
		pMtl->SetName(name);
	}

	// Loading from Material XML node.
	shaderName = node->getAttr( "Shader" );
	if (shaderName.empty())
	{
		// Replace empty shader with NoDraw material.
		shaderName = "NoDraw";
	}

	node->getAttr( "MtlFlags",mtlFlags );
	mtlFlags &= (MTL_FLAGS_SAVE_MASK); // Clear all flags that need not to be saved.
	mtlFlags |= pMtl->GetFlags();

	node->getAttr( "GenMask",nShaderGenMask );
	node->getAttr( "SurfaceType",surfaceType );
	pMtl->SetSurfaceType( surfaceType );

	if (stricmp(shaderName,"nodraw") == 0)
		mtlFlags |= MTL_FLAG_NODRAW;

	pMtl->SetFlags(mtlFlags);

	// Load lighting data.
	Vec3 diffuse,specular,emissive;
	node->getAttr( "Diffuse",diffuse );
	node->getAttr( "Specular",specular );
	node->getAttr( "Emissive",emissive );
	node->getAttr( "Shininess",sr.m_LMaterial.m_SpecShininess );
	sr.m_LMaterial.m_Diffuse = ToCFColor(diffuse);
	sr.m_LMaterial.m_Specular = ToCFColor(specular);
	sr.m_LMaterial.m_Emission = ToCFColor(emissive);

	node->getAttr( "Opacity", sr.m_Opacity );
	node->getAttr( "AlphaTest", sr.m_AlphaRef );
  node->getAttr( "GlowAmount", sr.m_GlowAmount );
  node->getAttr( "PostEffectType", sr.m_PostEffects );

	// Load material textures.
	XmlNodeRef texturesNode = node->findChild( "Textures" );
	if (texturesNode)
	{
		for (int i = 0; i < texturesNode->getChildCount(); i++)
		{
			texmap = "";
			XmlNodeRef texNode = texturesNode->getChild(i);
			texmap = texNode->getAttr( "Map" );

			int texId = -1;
			for (int j = 0; j < sizeof(sUsedTextures)/sizeof(sUsedTextures[0]); j++)
			{
				if (stricmp(sUsedTextures[j].name,texmap) == 0)
				{
					texId = sUsedTextures[j].texId;
					break;
				}
			}
			if (texId < 0)
				continue;

			file = "";
			file = texNode->getAttr( "File" );

			// Correct texid found.
			sr.m_Textures[texId].m_Name = file;
			texNode->getAttr( "Amount",sr.m_Textures[texId].m_Amount );
			texNode->getAttr( "IsTileU",sr.m_Textures[texId].m_bUTile );
			texNode->getAttr( "IsTileV",sr.m_Textures[texId].m_bVTile );
      texNode->getAttr( "TexType",sr.m_Textures[texId].m_Sampler.m_eTexType );

			XmlNodeRef modNode = texNode->findChild( "TexMod" );
			if (modNode)
			{
				SEfTexModificator &texm = sr.m_Textures[texId].m_TexModificator;

				// Modificators
				modNode->getAttr( "TileU",texm.m_Tiling[0] );
				modNode->getAttr( "TileV",texm.m_Tiling[1] );
				modNode->getAttr( "OffsetU",texm.m_Offs[0] );
				modNode->getAttr( "OffsetV",texm.m_Offs[1] );
				modNode->getAttr( "TexType",sr.m_Textures[texId].m_Sampler.m_eTexType );

				float f;
				modNode->getAttr( "TexMod_bTexGenProjected",texm.m_bTexGenProjected );
				modNode->getAttr( "TexMod_UOscillatorType",texm.m_eUMoveType );
				modNode->getAttr( "TexMod_VOscillatorType",texm.m_eVMoveType );
				modNode->getAttr( "TexMod_RotateType",texm.m_eRotType );
				modNode->getAttr( "TexMod_TexGenType",texm.m_eTGType );

				if (modNode->getAttr( "RotateU",f ))
				  texm.m_Rot[0] = Degr2Word(f);
				if (modNode->getAttr( "RotateV",f ))
				  texm.m_Rot[1] = Degr2Word(f);
				if (modNode->getAttr( "RotateW",f ))
				  texm.m_Rot[2] = Degr2Word(f);
				if (modNode->getAttr( "TexMod_URotateRate",f ))
				  texm.m_RotOscRate[0] = Degr2Word(f);
				if (modNode->getAttr( "TexMod_VRotateRate",f ))
				  texm.m_RotOscRate[1] = Degr2Word(f);
				if (modNode->getAttr( "TexMod_WRotateRate",f ))
				  texm.m_RotOscRate[2] = Degr2Word(f);
				if (modNode->getAttr( "TexMod_URotatePhase",f ))
				  texm.m_RotOscPhase[0] = Degr2Word(f);
				if (modNode->getAttr( "TexMod_VRotatePhase",f ))
				  texm.m_RotOscPhase[1] = Degr2Word(f);
				if (modNode->getAttr( "TexMod_WRotatePhase",f ))
			  	texm.m_RotOscPhase[2] = Degr2Word(f);
				if (modNode->getAttr( "TexMod_URotateAmplitude",f ))
		  		texm.m_RotOscAmplitude[0] = Degr2Word(f);
				if (modNode->getAttr( "TexMod_VRotateAmplitude",f ))
	  			texm.m_RotOscAmplitude[1] = Degr2Word(f);
				if (modNode->getAttr( "TexMod_WRotateAmplitude",f ))
  				texm.m_RotOscAmplitude[2] = Degr2Word(f);
				modNode->getAttr( "TexMod_URotateCenter",texm.m_RotOscCenter[0] );
        modNode->getAttr( "TexMod_VRotateCenter",texm.m_RotOscCenter[1] );
        modNode->getAttr( "TexMod_WRotateCenter",texm.m_RotOscCenter[2] );

				modNode->getAttr( "TexMod_UOscillatorRate",texm.m_UOscRate );
				modNode->getAttr( "TexMod_VOscillatorRate",texm.m_VOscRate );
				modNode->getAttr( "TexMod_UOscillatorPhase",texm.m_UOscPhase );
				modNode->getAttr( "TexMod_VOscillatorPhase",texm.m_VOscPhase );
				modNode->getAttr( "TexMod_UOscillatorAmplitude",texm.m_UOscAmplitude );
				modNode->getAttr( "TexMod_VOscillatorAmplitude",texm.m_VOscAmplitude );
			}
    }
	}

	// Load sub materials.
	XmlNodeRef childsNode = node->findChild( "SubMaterials" );
	if (childsNode)
	{
		mtlFlags |= MTL_FLAG_MULTI_SUBMTL;
		pMtl->SetFlags(mtlFlags);
		
		int nSubSlots = childsNode->getChildCount();
		pMtl->SetSubMtlCount(nSubSlots);
		for (int i = 0; i < nSubSlots; i++)
		{
			XmlNodeRef mtlNode = childsNode->getChild(i);
			if (mtlNode->isTag("MaterialRef"))
			{
				XmlString subname;
				if (mtlNode->getAttr("Name",subname))
				{
					IMaterial *pChildMtl = LoadMaterial( subname );
					pMtl->SetSubMtl(i,pChildMtl);
				}
			}
			else
			{
				IMaterial *pChildMtl = LoadMaterialFromXml( mtlNode,sLibraryName );
				pMtl->SetSubMtl(i,pChildMtl);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Load public parameters.
	//////////////////////////////////////////////////////////////////////////
	XmlNodeRef publicsNode = node->findChild( "PublicParams" );

	//////////////////////////////////////////////////////////////////////////
	// Reload shader item with new resources and shader.
	//////////////////////////////////////////////////////////////////////////
	LoadMaterialShader( pMtl,shaderName.c_str(),nShaderGenMask,sr,publicsNode );

  //////////////////////////////////////////////////////////////////////////
  // Load material layers data
  //////////////////////////////////////////////////////////////////////////
  
  XmlNodeRef pMtlLayersNode = node->findChild( "MaterialLayers" ); 
  if( pMtlLayersNode )
  {
    int nLayerCount = min( (int) MTL_LAYER_MAX_SLOTS, (int) pMtlLayersNode->getChildCount());
    if( nLayerCount)
    {
      uint8 nMaterialLayerFlags = 0;

      pMtl->SetLayerCount( nLayerCount );
      for (int l(0); l < nLayerCount; ++l)
      {
        XmlNodeRef pLayerNode = pMtlLayersNode->getChild(l);
        if(pLayerNode)
        {
          XmlString pszShaderName;
          if( pLayerNode->getAttr( "Name", pszShaderName) )
          {
            bool bNoDraw = false;
            pLayerNode->getAttr( "NoDraw", bNoDraw);

            uint8 nLayerFlags = 0;
            if ( bNoDraw )
            {
              nLayerFlags |= MTL_LAYER_USAGE_NODRAW;

              if( !strcmpi(pszShaderName.c_str(), "frozenlayerwip")) 
                nMaterialLayerFlags |= MTL_LAYER_FROZEN ;
              else
              if( !strcmpi(pszShaderName.c_str(), "cloaklayer")) 
                nMaterialLayerFlags |= MTL_LAYER_CLOAK;
            }
            else
              nLayerFlags &= ~MTL_LAYER_USAGE_NODRAW;

            XmlNodeRef pPublicsParamsNode = pLayerNode->findChild( "PublicParams" );                    
            LoadMaterialLayerSlot( l, pMtl, pszShaderName.c_str(), sr, pPublicsParamsNode, nLayerFlags); 
          }
        }
      }

      SShaderItem pShaderItemBase = pMtl->GetShaderItem();  
      if( pShaderItemBase.m_pShaderResources )
        pShaderItemBase.m_pShaderResources->SetMtlLayerNoDrawFlags( nMaterialLayerFlags );
    }
  }

	return pMtl;
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::MakeMaterialFromXml( const char *sMtlName,XmlNodeRef node,bool bForcePureChild )
{
	int mtlFlags = 0;
	CryFixedStringT<128> shaderName;
	unsigned int nShaderGenMask = 0;
	SInputShaderResources sr;

	// Loading
	node->getAttr( "MtlFlags",mtlFlags );
	if (bForcePureChild)
		mtlFlags |= MTL_FLAG_PURE_CHILD;

	IMaterial *pMtl = CreateMaterial( sMtlName,mtlFlags );

	if (!(mtlFlags & MTL_FLAG_MULTI_SUBMTL))
	{
		shaderName = node->getAttr( "Shader" );
		node->getAttr( "GenMask",nShaderGenMask );
		const char *surfaceType = node->getAttr( "SurfaceType" );
		pMtl->SetSurfaceType( surfaceType );

		if (stricmp(shaderName,"nodraw") == 0)
			mtlFlags |= MTL_FLAG_NODRAW;

		// Load lighting data.
		Vec3 vColor;
		if (node->getAttr( "Diffuse",vColor ))
			sr.m_LMaterial.m_Diffuse = ToCFColor(vColor);
		if (node->getAttr( "Specular",vColor ))
			sr.m_LMaterial.m_Specular = ToCFColor(vColor);
		if (node->getAttr( "Emissive",vColor ))
			sr.m_LMaterial.m_Emission = ToCFColor(vColor);
		node->getAttr( "Shininess",sr.m_LMaterial.m_SpecShininess );
		node->getAttr( "Opacity",sr.m_Opacity );
		node->getAttr( "AlphaTest",sr.m_AlphaRef );
    node->getAttr( "GlowAmount",sr.m_GlowAmount );
    node->getAttr( "PostEffectType", sr.m_PostEffects );

		const char* texmap = "";
		const char* file = "";
		// 
		XmlNodeRef texturesNode = node->findChild( "Textures" );
		if (texturesNode)
		{
			for (int i = 0; i < texturesNode->getChildCount(); i++)
			{
				XmlNodeRef texNode = texturesNode->getChild(i);
				texmap = texNode->getAttr( "Map" );

				int texId = -1;
				for (int j = 0; j < sizeof(sUsedTextures)/sizeof(sUsedTextures[0]); j++)
				{
					if (stricmp(sUsedTextures[j].name,texmap) == 0)
					{
						texId = sUsedTextures[j].texId;
						break;
					}
				}
				if (texId < 0)
					continue;

				file = texNode->getAttr( "File" );

				// Correct texid found.
				sr.m_Textures[texId].m_Name = file;
				texNode->getAttr( "Amount",sr.m_Textures[texId].m_Amount );
				texNode->getAttr( "IsTileU",sr.m_Textures[texId].m_bUTile );
				texNode->getAttr( "IsTileV",sr.m_Textures[texId].m_bVTile );
				texNode->getAttr( "TexType",sr.m_Textures[texId].m_Sampler.m_eTexType );

				int filter = sr.m_Textures[texId].m_Filter;
				if (texNode->getAttr( "Filter",filter ))
					sr.m_Textures[texId].m_Filter = filter;

				XmlNodeRef modNode = texNode->findChild( "TexMod" );
				if (modNode)
				{
					SEfTexModificator &texm = sr.m_Textures[texId].m_TexModificator;

					// Modificators
					modNode->getAttr( "TileU",texm.m_Tiling[0] );
					modNode->getAttr( "TileV",texm.m_Tiling[1] );
					modNode->getAttr( "OffsetU",texm.m_Offs[0] );
					modNode->getAttr( "OffsetV",texm.m_Offs[1] );

					float f;
					modNode->getAttr( "TexMod_bTexGenProjected",texm.m_bTexGenProjected );
					modNode->getAttr( "TexMod_UOscillatorType",texm.m_eUMoveType );
					modNode->getAttr( "TexMod_VOscillatorType",texm.m_eVMoveType );
					modNode->getAttr( "TexMod_RotateType",texm.m_eRotType );
					modNode->getAttr( "TexMod_TexGenType",texm.m_eTGType );

					if (modNode->getAttr( "RotateU",f ))
						texm.m_Rot[0] = Degr2Word(f);
					if (modNode->getAttr( "RotateV",f ))
						texm.m_Rot[1] = Degr2Word(f);
					if (modNode->getAttr( "RotateW",f ))
						texm.m_Rot[2] = Degr2Word(f);
					if (modNode->getAttr( "TexMod_URotateRate",f ))
						texm.m_RotOscRate[0] = Degr2Word(f);
					if (modNode->getAttr( "TexMod_VRotateRate",f ))
						texm.m_RotOscRate[1] = Degr2Word(f);
					if (modNode->getAttr( "TexMod_WRotateRate",f ))
						texm.m_RotOscRate[2] = Degr2Word(f);
					if (modNode->getAttr( "TexMod_URotatePhase",f ))
						texm.m_RotOscPhase[0] = Degr2Word(f);
					if (modNode->getAttr( "TexMod_VRotatePhase",f ))
						texm.m_RotOscPhase[1] = Degr2Word(f);
					if (modNode->getAttr( "TexMod_WRotatePhase",f ))
						texm.m_RotOscPhase[2] = Degr2Word(f);
					if (modNode->getAttr( "TexMod_URotateAmplitude",f ))
						texm.m_RotOscAmplitude[0] = Degr2Word(f);
					if (modNode->getAttr( "TexMod_VRotateAmplitude",f ))
						texm.m_RotOscAmplitude[1] = Degr2Word(f);
					if (modNode->getAttr( "TexMod_WRotateAmplitude",f ))
						texm.m_RotOscAmplitude[2] = Degr2Word(f);
					modNode->getAttr( "TexMod_URotateCenter",texm.m_RotOscCenter[0] );
					modNode->getAttr( "TexMod_VRotateCenter",texm.m_RotOscCenter[1] );
					modNode->getAttr( "TexMod_WRotateCenter",texm.m_RotOscCenter[2] );

					modNode->getAttr( "TexMod_UOscillatorRate",texm.m_UOscRate );
					modNode->getAttr( "TexMod_VOscillatorRate",texm.m_VOscRate );
					modNode->getAttr( "TexMod_UOscillatorPhase",texm.m_UOscPhase );
					modNode->getAttr( "TexMod_VOscillatorPhase",texm.m_VOscPhase );
					modNode->getAttr( "TexMod_UOscillatorAmplitude",texm.m_UOscAmplitude );
					modNode->getAttr( "TexMod_VOscillatorAmplitude",texm.m_VOscAmplitude );
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Check if we have vertex deform.
	//////////////////////////////////////////////////////////////////////////
	XmlNodeRef deformNode = node->findChild("VertexDeform");
	if (deformNode)
	{
		int deform_type = eDT_Unknown;
		deformNode->getAttr( "Type",deform_type );
		sr.m_DeformInfo.m_eType = (EDeformType)deform_type;
		deformNode->getAttr( "DividerX",sr.m_DeformInfo.m_fDividerX );
		deformNode->getAttr( "DividerY",sr.m_DeformInfo.m_fDividerY );
		deformNode->getAttr( "NoiseScale",sr.m_DeformInfo.m_vNoiseScale );

		XmlNodeRef waveX = deformNode->findChild("WaveX");
		if (waveX)
		{
			int type = eWF_None;
			waveX->getAttr( "Type",type );
			sr.m_DeformInfo.m_WaveX.m_eWFType = (EWaveForm)type;
			waveX->getAttr( "Amp",sr.m_DeformInfo.m_WaveX.m_Amp );
			waveX->getAttr( "Level",sr.m_DeformInfo.m_WaveX.m_Level );
			waveX->getAttr( "Phase",sr.m_DeformInfo.m_WaveX.m_Phase );
			waveX->getAttr( "Freq",sr.m_DeformInfo.m_WaveX.m_Freq );
		}
		XmlNodeRef waveY = deformNode->findChild("WaveY");
		if (waveY)
		{
			int type = eWF_None;
			waveY->getAttr( "Type",type );
			sr.m_DeformInfo.m_WaveY.m_eWFType = (EWaveForm)type;
			waveY->getAttr( "Amp",sr.m_DeformInfo.m_WaveY.m_Amp );
			waveY->getAttr( "Level",sr.m_DeformInfo.m_WaveY.m_Level );
			waveY->getAttr( "Phase",sr.m_DeformInfo.m_WaveY.m_Phase );
			waveY->getAttr( "Freq",sr.m_DeformInfo.m_WaveY.m_Freq );
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Load public parameters.
	XmlNodeRef publicVarsNode = node->findChild( "PublicParams" );

	//////////////////////////////////////////////////////////////////////////
	// Reload shader item with new resources and shader.
	if (!(mtlFlags & MTL_FLAG_MULTI_SUBMTL))
	{
    sr.m_szMaterialName = sMtlName;
		LoadMaterialShader( pMtl,shaderName.c_str(),nShaderGenMask,sr,publicVarsNode );
	}

  //////////////////////////////////////////////////////////////////////////
  // Load material layers data
  //////////////////////////////////////////////////////////////////////////

  if( pMtl && pMtl->GetShaderItem().m_pShader && pMtl->GetShaderItem().m_pShaderResources )
  {
    XmlNodeRef pMtlLayersNode = node->findChild( "MaterialLayers" ); 
    if( pMtlLayersNode )
    {
      int nLayerCount = min( (int) MTL_LAYER_MAX_SLOTS, (int) pMtlLayersNode->getChildCount());
      if( nLayerCount)
      {
        uint8 nMaterialLayerFlags = 0;

        pMtl->SetLayerCount( nLayerCount );
        for (int l(0); l < nLayerCount; ++l)
        {
          XmlNodeRef pLayerNode = pMtlLayersNode->getChild(l);
          if(pLayerNode)
          {
            XmlString pszShaderName;
            if( pLayerNode->getAttr( "Name", pszShaderName ) )
            {
              bool bNoDraw = false;
              pLayerNode->getAttr( "NoDraw", bNoDraw);                    

              uint8 nLayerFlags = 0;
              if ( bNoDraw )
              {
                nLayerFlags |= MTL_LAYER_USAGE_NODRAW;

                if( !strcmpi(pszShaderName.c_str(), "frozenlayerwip")) 
                  nMaterialLayerFlags |= MTL_LAYER_FROZEN ;
                else
                if( !strcmpi(pszShaderName.c_str(), "cloaklayer")) 
                  nMaterialLayerFlags |= MTL_LAYER_CLOAK;
              }
              else
                nLayerFlags &= ~MTL_LAYER_USAGE_NODRAW;
              
              XmlNodeRef pPublicsParamsNode = pLayerNode->findChild( "PublicParams" );
              sr.m_szMaterialName = sMtlName;
              LoadMaterialLayerSlot( l, pMtl, pszShaderName.c_str(), sr, pPublicsParamsNode, nLayerFlags); 
            }
          }
        }

        SShaderItem pShaderItemBase = pMtl->GetShaderItem();  
        if( pShaderItemBase.m_pShaderResources )
          pShaderItemBase.m_pShaderResources->SetMtlLayerNoDrawFlags( nMaterialLayerFlags );
      }
    }
  }
	
	// Serialize sub materials.
	XmlNodeRef childsNode = node->findChild( "SubMaterials" );
	if (childsNode)
	{
		int nSubMtls = childsNode->getChildCount();
		pMtl->SetSubMtlCount(nSubMtls);
		for (int i = 0; i < nSubMtls; i++)
		{
			XmlNodeRef mtlNode = childsNode->getChild(i);
			if (mtlNode->isTag("Material"))
			{
				const char *name = mtlNode->getAttr("Name");
				IMaterial *pChildMtl = MakeMaterialFromXml( name,mtlNode,true );
				if (pChildMtl)
					pMtl->SetSubMtl(i,pChildMtl);
				else
					pMtl->SetSubMtl(i,m_pDefaultMtl);
			}
			else
			{
				const char *name = mtlNode->getAttr("Name");
				if (name[0])
				{
					IMaterial *pChildMtl = LoadMaterial(name);
					if (pChildMtl)
						pMtl->SetSubMtl(i,pChildMtl);
				}
			}
		}
	}

	return pMtl;
}

//////////////////////////////////////////////////////////////////////////
bool CMatMan::LoadMaterialShader( IMaterial *pMtl,const char *sShader,unsigned int nShaderGenMask,SInputShaderResources &sr,XmlNodeRef &publicsNode)
{
	// Mark material invalid by default.
	int mtlFlags = pMtl->GetFlags();

	sr.m_ResFlags = pMtl->GetFlags();

	SShaderItem shaderItem = GetSystem()->GetIRenderer()->EF_LoadShaderItem(sShader,false,0,&sr,nShaderGenMask);
	if (!shaderItem.m_pShader)
	{
		Warning( "Failed to load shader %s in Material %s",sShader,pMtl->GetName() );
		return false;
	}
	pMtl->AssignShaderItem( shaderItem );

	// Set public params.
	if (publicsNode)
	{
		// Copy public params from the shader.
		sr.m_ShaderParams = shaderItem.m_pShader->GetPublicParams();
		// Parse public parameters, and assign them to source shader resources.
		ParsePublicParams( sr, publicsNode );
		shaderItem.m_pShaderResources->SetShaderParams(&sr, shaderItem.m_pShader);
	}
	
	return true;
}

bool CMatMan::LoadMaterialLayerSlot( uint32 nSlot, IMaterial *pMtl, const char *szShaderName, SInputShaderResources &pBaseResources, XmlNodeRef &pPublicsNode, uint8 nLayerFlags)
{
  if( !pMtl || pMtl->GetLayer( nSlot ) || !pPublicsNode)
  {
    return false;
  }

  // need to handle no draw case
  if (stricmp(szShaderName,"nodraw") == 0)
  {          
    // no shader = skip layer
    return false;
  }

  // Get base material/shaderItem info
  SInputShaderResources pInputResources;
  SShaderItem pShaderItemBase = pMtl->GetShaderItem();  

  uint32 nMaskGenBase = pShaderItemBase.m_pShader->GetGenerationMask();  
  SShaderGen *pShaderGenBase = pShaderItemBase.m_pShader->GetGenerationParams();

  // copy diffuse and bump textures names  

  pInputResources.m_szMaterialName = pBaseResources.m_szMaterialName;
  pInputResources.m_Textures[EFTT_DIFFUSE].m_Name = pBaseResources.m_Textures[EFTT_DIFFUSE].m_Name;
  pInputResources.m_Textures[EFTT_BUMP].m_Name = pBaseResources.m_Textures[EFTT_BUMP].m_Name;
  
  // Check if names are valid - else replace with default textures

  if( pInputResources.m_Textures[EFTT_DIFFUSE].m_Name.empty() )
  {
    pInputResources.m_Textures[EFTT_DIFFUSE].m_Name = "Textures/defaults/replaceme.dds";
//		pInputResources.m_Textures[EFTT_DIFFUSE].m_Name = "<default>";
  }

  if( pInputResources.m_Textures[EFTT_BUMP].m_Name.empty() )
  {
    pInputResources.m_Textures[EFTT_BUMP].m_Name = "Textures/defaults/white_ddn.dds";
  }

  // Load layer shader item
  IShader *pNewShader = GetSystem()->GetIRenderer()->EF_LoadShader( szShaderName, 0);
  if (!pNewShader ) 
  {
    Warning( "Failed to load material layer shader %s in Material %s", szShaderName, pMtl->GetName() );
    return false;
  }
    
  // mask generation for base material shader
  uint32 nMaskGenLayer = 0;  
  SShaderGen *pShaderGenLayer = pNewShader->GetGenerationParams();    
  if (pShaderGenBase && pShaderGenLayer)
  {                        
    for (int nLayerBit(0); nLayerBit < pShaderGenLayer->m_BitMask.size(); ++nLayerBit)
    {
      SShaderGenBit *pLayerBit = pShaderGenLayer->m_BitMask[nLayerBit];

      for (int nBaseBit(0); nBaseBit < pShaderGenBase->m_BitMask.size(); ++nBaseBit)
      {
        SShaderGenBit *pBaseBit = pShaderGenBase->m_BitMask[nBaseBit];

        // Need to check if flag name is common to both shaders (since flags values can be different), if so activate it on this layer
        if( nMaskGenBase&pBaseBit->m_Mask )
        {
          if (!pLayerBit->m_ParamName.empty() && !pBaseBit->m_ParamName.empty())
          {
            if( pLayerBit->m_ParamName ==  pBaseBit->m_ParamName )
            {
              nMaskGenLayer |= pLayerBit->m_Mask; 
              break;
            }
          }
        }
      }
    }
  }

  // Reload with proper flags
  SShaderItem pShaderItem = GetSystem()->GetIRenderer()->EF_LoadShaderItem( szShaderName, false, 0, &pInputResources, nMaskGenLayer);
  if (!pShaderItem.m_pShader )
  {
    Warning( "Failed to load material layer shader %s in Material %s", szShaderName, pMtl->GetName() );
    return false;
  }
 
  // Copy public params from the shader.
  pInputResources.m_ShaderParams = pShaderItem.m_pShader->GetPublicParams();

  SAFE_RELEASE( pShaderItem.m_pShaderResources );

  // Copy resources from base material
  pShaderItem.m_pShaderResources = pShaderItemBase.m_pShaderResources->Clone();

  // Parse public parameters, and assign them to source shader resources.
  ParsePublicParams( pInputResources, pPublicsNode );
  pShaderItem.m_pShaderResources->SetShaderParams(&pInputResources, pShaderItem.m_pShader);  

  IMaterialLayer *pCurrMtlLayer = pMtl->CreateLayer();
    
  pCurrMtlLayer->SetFlags( nLayerFlags );
  pCurrMtlLayer->SetShaderItem(pMtl, pShaderItem);
  
  pMtl->SetLayer(nSlot, pCurrMtlLayer);

  return true;
}

//////////////////////////////////////////////////////////////////////////

void CMatMan::ParsePublicParams( SInputShaderResources &sr,XmlNodeRef paramsNode )
{
	if (sr.m_ShaderParams.empty())
		return;

	for (unsigned int i = 0; i < sr.m_ShaderParams.size(); i++)
	{
		SShaderParam *pParam = &sr.m_ShaderParams[i];
		switch (pParam->m_Type)
		{
		case eType_BYTE:
			paramsNode->getAttr( pParam->m_Name,pParam->m_Value.m_Byte );
			break;
		case eType_SHORT:
			paramsNode->getAttr( pParam->m_Name,pParam->m_Value.m_Short );
			break;
		case eType_INT:
			paramsNode->getAttr( pParam->m_Name,pParam->m_Value.m_Int );
			break;
		case eType_FLOAT:
			paramsNode->getAttr( pParam->m_Name,pParam->m_Value.m_Float );
			break;
		//case eType_STRING:
			//paramsNode->getAttr( pParam->m_Name,pParam->m_Value.m_String );
			//break;
		case eType_FCOLOR:
			{
				Vec3 v(pParam->m_Value.m_Color[0],pParam->m_Value.m_Color[1],pParam->m_Value.m_Color[2]);
				paramsNode->getAttr( pParam->m_Name,v );
				pParam->m_Value.m_Color[0] = v.x;
				pParam->m_Value.m_Color[1] = v.y;
				pParam->m_Value.m_Color[2] = v.z;
			}
			break;
		case eType_VECTOR:
			{
				Vec3 v(pParam->m_Value.m_Vector[0],pParam->m_Value.m_Vector[1],pParam->m_Value.m_Vector[2]);
				paramsNode->getAttr( pParam->m_Name,v );
				pParam->m_Value.m_Vector[0] = v.x;
				pParam->m_Value.m_Vector[1] = v.y;
				pParam->m_Value.m_Vector[2] = v.z;
			}
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
ISurfaceType* CMatMan::GetSurfaceTypeByName( const char *sSurfaceTypeName,const char *sWhy )
{
	return m_pSurfaceTypeManager->GetSurfaceTypeByName(sSurfaceTypeName,sWhy);
};

//////////////////////////////////////////////////////////////////////////
int CMatMan::GetSurfaceTypeIdByName( const char *sSurfaceTypeName,const char *sWhy )
{
	ISurfaceType* pSurfaceType = m_pSurfaceTypeManager->GetSurfaceTypeByName(sSurfaceTypeName,sWhy);
	if (pSurfaceType)
		return pSurfaceType->GetId();
	return 0;
};

//////////////////////////////////////////////////////////////////////////
ISurfaceType* CMatMan::GetSurfaceType( int nSurfaceTypeId,const char *sWhy )
{
	return m_pSurfaceTypeManager->GetSurfaceTypeFast(nSurfaceTypeId,sWhy);
};

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::GetDefaultMaterial()
{
	return m_pDefaultMtl;
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::GetDefaultTerrainLayerMaterial()
{
	return m_pDefaultTerrainLayersMtl;
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::GetDefaultLayersMaterial()
{
  return m_pDefaultLayersMtl;
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::GetDefaultHelperMaterial()
{
	return m_pDefaultHelperMtl;
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::GetLoadedMaterials( IMaterial **pData, uint32 &nObjCount ) const
{
	nObjCount = m_mtlNameMap.size();

	if(!pData)
		return;

	MtlNameMap::const_iterator it, end=m_mtlNameMap.end();

	for(it=m_mtlNameMap.begin();it!=end;++it)
	{
		IMaterial *pMat = it->second;

		*pData++ = pMat;
	}
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::CloneMaterial( IMaterial* pSrcMtl,int nSubMtl )
{
	if (pSrcMtl->GetFlags() & MTL_FLAG_MULTI_SUBMTL)
	{
		IMaterial *pMultiMat = new CMatInfo;

		//m_mtlSet.insert( pMat );
		pMultiMat->SetName( pSrcMtl->GetName() );
		pMultiMat->SetFlags( pMultiMat->GetFlags()|MTL_FLAG_MULTI_SUBMTL );

		bool bCloneAllSubMtls = nSubMtl < 0;

		int nSubMtls = pSrcMtl->GetSubMtlCount();
		pMultiMat->SetSubMtlCount(nSubMtls);
		for (int i = 0; i < nSubMtls; i++)
		{
			CMatInfo *pChildSrcMtl = (CMatInfo*)pSrcMtl->GetSubMtl(i);
			if (!pChildSrcMtl)
				continue;
			if (bCloneAllSubMtls)
			{
				pMultiMat->SetSubMtl( i,pChildSrcMtl->Clone() );
			}
			else
			{
				pMultiMat->SetSubMtl( i,pChildSrcMtl );
				if (i == nSubMtls)
				{
					// Clone this slot.
					pMultiMat->SetSubMtl( i,pChildSrcMtl->Clone() );
				}
			}
		}
		return pMultiMat;
	}
	else
	{
		return ((CMatInfo*)pSrcMtl)->Clone();
	}
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::CloneMultiMaterial( IMaterial* pSrcMtl,const char *sSubMtlName )
{
	if (pSrcMtl->GetFlags() & MTL_FLAG_MULTI_SUBMTL)
	{
		IMaterial *pMultiMat = new CMatInfo;

		//m_mtlSet.insert( pMat );
		pMultiMat->SetName( pSrcMtl->GetName() );
		pMultiMat->SetFlags( pMultiMat->GetFlags()|MTL_FLAG_MULTI_SUBMTL );

		bool bCloneAllSubMtls = sSubMtlName == 0;

		int nSubMtls = pSrcMtl->GetSubMtlCount();
		pMultiMat->SetSubMtlCount(nSubMtls);
		for (int i = 0; i < nSubMtls; i++)
		{
			CMatInfo *pChildSrcMtl = (CMatInfo*)pSrcMtl->GetSubMtl(i);
			if (!pChildSrcMtl)
				continue;
			if (bCloneAllSubMtls)
			{
				pMultiMat->SetSubMtl( i,pChildSrcMtl->Clone() );
			}
			else
			{
				pMultiMat->SetSubMtl( i,pChildSrcMtl );
				if (stricmp(pChildSrcMtl->GetName(),sSubMtlName) == 0)
				{
					// Clone this slot.
					pMultiMat->SetSubMtl( i,pChildSrcMtl->Clone() );
				}
			}
		}
		return pMultiMat;
	}
	else
	{
		return ((CMatInfo*)pSrcMtl)->Clone();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::InitDefaults()
{
	if (!m_pDefaultMtl)
	{
		m_pDefaultMtl = new CMatInfo;
		m_pDefaultMtl->SetName( "Default" );
		SInputShaderResources sr;
		sr.m_Opacity = 1;
		sr.m_LMaterial.m_Diffuse.set(1,1,1,1);
		sr.m_Textures[EFTT_DIFFUSE].m_Name = "Textures/defaults/replaceme.dds";
		//sr.m_Textures[EFTT_DIFFUSE].m_Name = "<default>";
		SShaderItem si = GetRenderer()->EF_LoadShaderItem("Illum", true, 0, &sr);
		if (si.m_pShaderResources)
			si.m_pShaderResources->SetMaterialName("Default");
		m_pDefaultMtl->AssignShaderItem(si);
	}

	if(!m_pDefaultTerrainLayersMtl)
	{
		m_pDefaultTerrainLayersMtl = new CMatInfo;
		m_pDefaultTerrainLayersMtl->SetName( "DefaultTerrainLayer" );
		SInputShaderResources sr;
		sr.m_Opacity = 1;
		sr.m_LMaterial.m_Diffuse.set(1,1,1,1);
		sr.m_Textures[EFTT_DIFFUSE].m_Name = "Textures/defaults/replaceme.dds";
		SShaderItem si = GetRenderer()->EF_LoadShaderItem("Terrain.Layer", true, 0, &sr);
		if (si.m_pShaderResources)
			si.m_pShaderResources->SetMaterialName("DefaultTerrainLayer");
		m_pDefaultTerrainLayersMtl->AssignShaderItem(si);
	}

  if( !m_pDefaultLayersMtl )
  {
    m_pDefaultLayersMtl = LoadMaterial( "Materials/material_layers_default", false );
  }

	if (!m_pNoDrawMtl)
	{
		m_pNoDrawMtl = new CMatInfo;
		m_pNoDrawMtl->SetFlags(MTL_FLAG_NODRAW);
		m_pNoDrawMtl->SetName(MATERIAL_NODRAW);
		SShaderItem si;
		si.m_pShader = GetRenderer()->EF_LoadShader(MATERIAL_NODRAW, EF_SYSTEM);
		m_pNoDrawMtl->AssignShaderItem(si);
		m_mtlNameMap[ UnifyName(m_pNoDrawMtl->GetName()) ] = m_pNoDrawMtl;
	}

	if (!m_pDefaultHelperMtl)
	{
		m_pDefaultHelperMtl = new CMatInfo;
		m_pDefaultHelperMtl->SetName( "DefaultHelper" );
		SInputShaderResources sr;
		sr.m_Opacity = 1;
		sr.m_LMaterial.m_Diffuse.set(1,1,1,1);
		sr.m_Textures[EFTT_DIFFUSE].m_Name = "Textures/defaults/replaceme.dds";
		SShaderItem si = GetRenderer()->EF_LoadShaderItem("Helper", true, 0, &sr);
		if (si.m_pShaderResources)
			si.m_pShaderResources->SetMaterialName("DefaultHelper");
		m_pDefaultHelperMtl->AssignShaderItem(si);
	}
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::LoadCGFMaterial( CMaterialCGF *pMaterialCGF,const char *sCgfFilename )
{
	if (!pMaterialCGF->pMatEntity && !pMaterialCGF->bOldMaterial)
	{
		string sMtlName = pMaterialCGF->name;
		if (sMtlName.find('/') == string::npos)
		{
			// If no slashes in the name assume it is in same folder as a cgf.
			sMtlName = PathUtil::AddSlash(PathUtil::GetPath(sCgfFilename) ) + sMtlName;
		}
		return LoadMaterial( sMtlName );
	}

	//////////////////////////////////////////////////////////////////////////
	// Create materials from Old CGF materials.
	//////////////////////////////////////////////////////////////////////////
	string sMtlName;
	sMtlName = sCgfFilename;
	sMtlName.replace( '\\','/' );
	PathUtil::RemoveExtension(sMtlName);

	IMaterial *pMaterial = LoadMaterial( sMtlName,false );
	if (!pMaterial)
	{
		string sMtlPath = PathUtil::GetPath(sMtlName);
		if (pMaterialCGF->pMatEntity)
		{
			pMaterial = CreateMaterial( sMtlName );
			((CMatInfo*)pMaterial)->LoadFromMatEntity( pMaterialCGF->pMatEntity,sMtlPath );
			pMaterial->SetName( sMtlName );
		}
		else
		{
			pMaterial = CreateMaterial( sMtlName,MTL_FLAG_MULTI_SUBMTL );
			pMaterial->SetSubMtlCount( pMaterialCGF->subMaterials.size() );
			for (int i = 0; i < (int)pMaterialCGF->subMaterials.size(); i++)
			{
				CMaterialCGF *pChildMtlCGF = pMaterialCGF->subMaterials[i];
				if (pChildMtlCGF && pChildMtlCGF->pMatEntity)
				{
					IMaterial *pChildMtl = CreateMaterial( pChildMtlCGF->name,MTL_FLAG_PURE_CHILD );
					((CMatInfo*)pChildMtl)->LoadFromMatEntity( pChildMtlCGF->pMatEntity,sMtlPath );
					pMaterial->SetSubMtl( i,pChildMtl );
				}
			}
		}
		if (pMaterial)
		{
			NotifyCreateMaterial(pMaterial);
		}
	}
	if (!pMaterial)
	{
		pMaterial = GetDefaultMaterial();
	}
	return pMaterial;
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::PrecacheDecalMaterials()
{
	LOADING_TIME_PROFILE_SECTION(GetSystem());

	// Wildcards load.
	string sPath = PathUtil::Make( MATERIAL_DECALS_FOLDER,"*.mtl" );
	ICryPak *pack = gEnv->pCryPak;
	_finddata_t fd;
	intptr_t handle = pack->FindFirst( sPath, &fd );
	int nCount = 0;
	if (handle >= 0)
	{
		do {
			string sMtlName = PathUtil::Make( MATERIAL_DECALS_FOLDER,fd.name );
			PathUtil::RemoveExtension(sMtlName);
			IMaterial *pMtl = LoadMaterial( sMtlName,true );
			if (pMtl)
			{
				m_precachedDecalMaterial.push_back( (CMatInfo*)pMtl );
			}
		} while (pack->FindNext( handle, &fd ) >= 0);
		pack->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::ClearDecalMaterials()
{
	m_precachedDecalMaterial.clear();
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::SetSketchMode( int mode )
{
  if( !e_lowspec_mode )
  {
    if (mode != 0)
    {
      gEnv->pConsole->ExecuteString( "exec sketch_on" );
    }
    else
    {
      gEnv->pConsole->ExecuteString( "exec sketch_off" );
      gEnv->pConsole->ExecuteString( "sys_RestoreSpec apply" ); // Restore the spec as it was before sketch.
    }
  }

	for (MtlNameMap::iterator it = m_mtlNameMap.begin(); it != m_mtlNameMap.end(); ++it)
	{
		CMatInfo *pMtl = (CMatInfo*)it->second;
		pMtl->SetSketchMode( mode );
	}	
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam )
{
	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_LOAD_START:
		if (!m_pXmlParser)
		{
			// Create a shared XML parser for level loading.
			m_pXmlParser = m_pXmlParser = GetISystem()->GetXmlUtils()->CreateXmlParser();
		}
		break;
	case ESYSTEM_EVENT_LEVEL_LOAD_END:
		// Free xml parser after the level have been loaded.
		m_pXmlParser = 0;
		break;
	}
}
