////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   material.cpp
//  Version:     v1.00
//  Created:     3/2/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Material.h"
#include "MaterialManager.h"
#include "BaseLibrary.h"
#include "ErrorReport.h"

#include <I3DEngine.h>
#include "IEntityRenderState.h"
#include <CryArray.h>
#include <CryHeaders.h>
#include <ICryAnimation.h>
#include <ISourceControl.h>

static bool defaultTexMod_Initialized = false;
static SEfTexModificator defaultTexMod;
static SInputShaderResources defaultShaderResource;

//////////////////////////////////////////////////////////////////////////
CMaterial::CMaterial( CMaterialManager *pManager,const CString &name,int nFlags )
{
	m_pManager = pManager;
	m_scFileAttributes = SCC_FILE_ATTRIBUTE_NORMAL;

	m_pParent = 0;

	if (!defaultTexMod_Initialized)
	{
		ZeroStruct(defaultTexMod);
		defaultTexMod.m_Tiling[0] = 1;
		defaultTexMod.m_Tiling[1] = 1;
		defaultTexMod_Initialized = true;
	
	}
	
	m_shaderResources = defaultShaderResource;
	m_shaderResources.m_Opacity = 1;  
	m_shaderResources.m_LMaterial.m_Diffuse.Set(1.0f,1.0f,1.0f,1.0f);
	m_shaderResources.m_LMaterial.m_SpecShininess = 10.0f;
	m_shaderResources.m_Textures[EFTT_BUMP].m_Amount = 50;
	m_shaderResources.m_Textures[EFTT_NORMALMAP].m_Amount = 50;

	m_mtlFlags = nFlags;
	ZeroStruct(m_shaderItem);

	// Default shader.
	m_shaderName = "Illum";
	m_nShaderGenMask = 0;

	m_name = name;
	m_bRegetPublicParams = true;
	m_bKeepPublicParamsValues = false;
	m_bIgnoreNotifyChange = false;
	m_bDummyMaterial = false;
	m_bFromEngine = false;

	m_pMatInfo = NULL;
}

//////////////////////////////////////////////////////////////////////////
CMaterial::~CMaterial()
{
	if (IsModified())
		Save();

	// Release used shader.
	SAFE_RELEASE( m_shaderItem.m_pShader );
	SAFE_RELEASE( m_shaderItem.m_pShaderResources );

	if (m_pMatInfo)
	{
		m_pMatInfo->SetUserData(0);
		m_pMatInfo = 0;
	}
	if (!m_subMaterials.empty())
	{
		for (int i = 0; i < m_subMaterials.size(); i++)
			if (m_subMaterials[i])
				m_subMaterials[i]->m_pParent = NULL;
		m_subMaterials.clear();
	}

	if (!IsPureChild() && !(GetFlags() & MTL_FLAG_UIMATERIAL))
	{
		// Unregister this material from manager.
		m_pManager->DeleteItem(this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::SetName( const CString &name )
{
	if (name != m_name)
	{
		CString oldName = m_name;
		m_name = name;
		if (!IsPureChild())
		{
			m_pManager->RenameItem( this,name,oldName );
			if (m_pMatInfo)
			{
				GetIEditor()->Get3DEngine()->GetMaterialManager()->RenameMaterial( m_pMatInfo,GetName() );
			}
		}
		else
		{
			if (m_pMatInfo)
				m_pMatInfo->SetName(m_name);
		}
		NotifyChanged();
	}
	if (m_shaderItem.m_pShaderResources)
	{
		// Only for correct warning message purposes.
		m_shaderItem.m_pShaderResources->SetMaterialName(m_name);
	}
}

//////////////////////////////////////////////////////////////////////////
CString CMaterial::GetFilename()
{
	CString filename = GetMatManager()->MaterialToFilename(m_name);
	return filename;
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::UpdateFileAttributes()
{
	if (GetFilename().IsEmpty())
		return;

	m_scFileAttributes = GetIEditor()->GetSourceControl()->GetFileAttributes(GetFilename());
}

//////////////////////////////////////////////////////////////////////////
uint32 CMaterial::GetFileAttributes( bool bUpdateFromFile )
{
	if (IsDummy())
		return m_scFileAttributes;

	if (IsPureChild() && m_pParent)
		return m_pParent->GetFileAttributes();

	if (bUpdateFromFile)
		UpdateFileAttributes();
	return m_scFileAttributes;
};

//////////////////////////////////////////////////////////////////////////
void CMaterial::SetShaderName( const CString &shaderName )
{
	if (m_shaderName != shaderName)
	{
    // update params !

		m_bRegetPublicParams = true;
		m_bKeepPublicParamsValues = false;
		RecordUndo("Change Shader");
	}
	m_shaderName = shaderName;
	if (stricmp(m_shaderName,"nodraw") == 0)
		m_mtlFlags |= MTL_FLAG_NODRAW;
	else
		m_mtlFlags &= ~MTL_FLAG_NODRAW;
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::CheckSpecialConditions()
{
	if (stricmp(m_shaderName,"nodraw") == 0)
		m_mtlFlags |= MTL_FLAG_NODRAW;
	else
		m_mtlFlags &= ~MTL_FLAG_NODRAW;

	// If environment texture name have auto_cubemap_ in it, force material to use auto cube-map for it.
	if (!m_shaderResources.m_Textures[EFTT_ENV].m_Name.empty())
	{
		const char *sAtPos = strstr(m_shaderResources.m_Textures[EFTT_ENV].m_Name.c_str(),"auto_cubemap");
		if (sAtPos)
		{
			// Force Auto-Cubemap
			m_shaderResources.m_Textures[EFTT_ENV].m_Sampler.m_eTexType = eTT_AutoCube;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CMaterial::LoadShader()
{
	if (m_bDummyMaterial)
		return true;

	CheckSpecialConditions();

//	if (shaderName.IsEmpty())
//		return false;

	GetIEditor()->GetErrorReport()->SetCurrentValidatorItem( this );

	/*
	if (m_mtlFlags & MTL_FLAG_LIGHTING)
		m_shaderResources.m_LMaterial = &m_lightMaterial;
	else
		m_shaderResources.m_LMaterial = 0;
	*/

	m_shaderResources.m_ResFlags = m_mtlFlags;

	CString sShader = m_shaderName;
	if (sShader.IsEmpty())
	{
		sShader = "<Default>";
	}

  m_shaderResources.m_szMaterialName = m_name;
	SShaderItem newShaderItem = GetIEditor()->GetRenderer()->EF_LoadShaderItem( sShader,false,0,&m_shaderResources,m_nShaderGenMask );

  // Shader not found
  if( newShaderItem.m_pShader && newShaderItem.m_pShader->GetFlags() & EF_NOTFOUND )
    CryWarning(VALIDATOR_MODULE_EDITOR,VALIDATOR_WARNING,"Shader %s from material %s not found",newShaderItem.m_pShader->GetName(), m_name);

  //if (newShaderItem.m_pShader)
  //{
  //  newShaderItem.m_pShader->AddRef();
  //}

  //if (newShaderItem.m_pShaderResources)
  //{
  //  newShaderItem.m_pShaderResources->AddRef();
  //}

  // Release previously used shader (Must be After new shader is loaded, for speed).
  SAFE_RELEASE( m_shaderItem.m_pShader );
  SAFE_RELEASE( m_shaderItem.m_pShaderResources );

	m_shaderItem = newShaderItem;

	if (!m_shaderItem.m_pShader)
	{
		CErrorRecord err;
		err.error.Format( _T("Failed to Load Shader %s"),(const char*)m_shaderName );
		err.pItem = this;
		GetIEditor()->GetErrorReport()->ReportError( err );
		GetIEditor()->GetErrorReport()->SetCurrentValidatorItem( NULL );
		return false;
	}

	IShader *pShader = m_shaderItem.m_pShader;
	m_nShaderGenMask = pShader->GetGenerationMask();
	if (pShader->GetFlags() & EF_NOPREVIEW)
		m_mtlFlags |= MTL_FLAG_NOPREVIEW;
	else
		m_mtlFlags &= ~MTL_FLAG_NOPREVIEW;

	//////////////////////////////////////////////////////////////////////////
	// Reget shader params.
	//////////////////////////////////////////////////////////////////////////
	if (m_bRegetPublicParams)
	{
		if (m_bKeepPublicParamsValues)
		{
			m_bKeepPublicParamsValues = false;
			m_publicVarsCache = CreateXmlNode( "PublicParams" );
			SetXmlFromShaderParams(m_shaderResources, m_publicVarsCache );
		}
		m_shaderResources.m_ShaderParams = pShader->GetPublicParams();
	}
	m_bRegetPublicParams = false;

	//////////////////////////////////////////////////////////////////////////
	// If we have XML node with public parameters loaded, apply it on shader params.
	//////////////////////////////////////////////////////////////////////////
	if (m_publicVarsCache)
	{
    SetShaderParamsFromXml(m_shaderResources, m_publicVarsCache);
		m_publicVarsCache = 0;
	}

	//////////////////////////////////////////////////////////////////////////
	// Set shader params.
	if (m_shaderItem.m_pShaderResources)
		m_shaderItem.m_pShaderResources->SetShaderParams( &m_shaderResources, m_shaderItem.m_pShader );
	//////////////////////////////////////////////////////////////////////////



  //////////////////////////////////////////////////////////////////////////
  // Set Shader Params for material layers
  //////////////////////////////////////////////////////////////////////////

	if (m_pMatInfo)
	{    
		UpdateMatInfo();
	}  

	GetIEditor()->GetErrorReport()->SetCurrentValidatorItem( NULL );
	return true;
}

bool CMaterial::LoadMaterialLayers()
{
  if (!m_pMatInfo)
  {
    return false;
  }

  if( m_shaderItem.m_pShader && m_shaderItem.m_pShaderResources )
  {
    // mask generation for base material shader
    uint32 nMaskGenBase = m_shaderItem.m_pShader->GetGenerationMask();  
    SShaderGen *pShaderGenBase = m_shaderItem.m_pShader->GetGenerationParams();

    for(uint32 l(0); l < MTL_LAYER_MAX_SLOTS; ++l)
    {
      SMaterialLayerResources *pCurrLayer = &m_pMtlLayerResources[l];        
      pCurrLayer->m_nFlags |= MTL_FLAG_NODRAW;
      if(!pCurrLayer->m_shaderName.IsEmpty())
      {
        if (strcmpi(pCurrLayer->m_shaderName,"nodraw") == 0)
        {          
          // no shader = skip layer
          pCurrLayer->m_shaderName.Empty();          
          continue;
        }

        IShader *pNewShader = GetIEditor()->GetRenderer()->EF_LoadShader( pCurrLayer->m_shaderName, 0);
        
        // Check if shader loaded
        if( !pNewShader )
        {
          Warning( "Failed to load material layer shader %s in Material %s", pCurrLayer->m_shaderName, m_pMatInfo->GetName() );          
          continue;
        }
        
        if( !pCurrLayer->m_pMatLayer )
        {
          pCurrLayer->m_pMatLayer = m_pMatInfo->CreateLayer();
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
        SShaderItem newShaderItem = GetIEditor()->GetRenderer()->EF_LoadShaderItem( pCurrLayer->m_shaderName, false, 0, &pCurrLayer->m_shaderResources, nMaskGenLayer);        
        if (!newShaderItem.m_pShader )
        {
          Warning( "Failed to load material layer shader %s in Material %s", pCurrLayer->m_shaderName, m_pMatInfo->GetName() );          
          continue;
        }

        SShaderItem &pCurrShaderItem = pCurrLayer->m_pMatLayer->GetShaderItem();

        if (newShaderItem.m_pShader)
        {
          newShaderItem.m_pShader->AddRef();
        }

        // Release previously used shader (Must be After new shader is loaded, for speed).
        SAFE_RELEASE( pCurrShaderItem.m_pShader ); 
        SAFE_RELEASE( pCurrShaderItem.m_pShaderResources );
        SAFE_RELEASE( newShaderItem.m_pShaderResources );

        pCurrShaderItem.m_pShader = newShaderItem.m_pShader;
        // Copy resources from base material
        pCurrShaderItem.m_pShaderResources = m_shaderItem.m_pShaderResources->Clone();
        pCurrShaderItem.m_nTechnique = newShaderItem.m_nTechnique;
        pCurrShaderItem.m_nPreprocessFlags = newShaderItem.m_nPreprocessFlags;

        // set default params
        if( pCurrLayer->m_bRegetPublicParams)
        {
          pCurrLayer->m_shaderResources.m_ShaderParams = pCurrShaderItem.m_pShader->GetPublicParams();        
        }

        pCurrLayer->m_bRegetPublicParams = false;

        if( pCurrLayer->m_publicVarsCache )
        {
          CMaterial::SetShaderParamsFromXml( pCurrLayer->m_shaderResources, pCurrLayer->m_publicVarsCache );
          pCurrLayer->m_publicVarsCache = 0;  
        }

        if (pCurrShaderItem.m_pShaderResources )
        {
          pCurrShaderItem.m_pShaderResources->SetShaderParams( &pCurrLayer->m_shaderResources, pCurrShaderItem.m_pShader );
        }

        // Activate layer
        pCurrLayer->m_nFlags &= ~MTL_FLAG_NODRAW;
      }

    }

    return true;
  }

  return false;
}

//////////////////////////////////////////////////////////////////////////

void CMaterial::UpdateMaterialLayers()
{
  if (m_pMatInfo && m_shaderItem.m_pShaderResources)
  {
    m_pMatInfo->SetLayerCount( MTL_LAYER_MAX_SLOTS );

    uint8 nMaterialLayerFlags = 0;

    for(int l(0); l< MTL_LAYER_MAX_SLOTS; ++l)
    {      
      SMaterialLayerResources *pCurrLayer = &m_pMtlLayerResources[l]; 
      if( pCurrLayer && !pCurrLayer->m_shaderName.IsEmpty() && pCurrLayer->m_pMatLayer)
      {                                
        pCurrLayer->m_pMatLayer->SetFlags( pCurrLayer->m_nFlags );
        m_pMatInfo->SetLayer(l, pCurrLayer->m_pMatLayer);        

        if( (pCurrLayer->m_nFlags & MTL_LAYER_USAGE_NODRAW) )
        {
          if( !strcmpi(pCurrLayer->m_shaderName, "frozenlayerwip")) 
            nMaterialLayerFlags |= MTL_LAYER_FROZEN ;
          else
          if( !strcmpi(pCurrLayer->m_shaderName, "cloaklayer")) 
            nMaterialLayerFlags |= MTL_LAYER_CLOAK;
        }
      }
    }

    if( m_shaderItem.m_pShaderResources )
      m_shaderItem.m_pShaderResources->SetMtlLayerNoDrawFlags( nMaterialLayerFlags );
  }
}

void CMaterial::UpdateMatInfo()
{  
	if (m_pMatInfo)
	{
		// Mark material invalid.
		m_pMatInfo->SetFlags( m_mtlFlags );
		m_pMatInfo->SetShaderItem( m_shaderItem );
		m_pMatInfo->SetSurfaceType( m_surfaceType );

    LoadMaterialLayers();
    UpdateMaterialLayers();
    
		if (IsMultiSubMaterial())
		{
			m_pMatInfo->SetSubMtlCount(m_subMaterials.size());
			for (unsigned int i = 0; i < m_subMaterials.size(); i++)
			{
				if (m_subMaterials[i])
					m_pMatInfo->SetSubMtl( i,m_subMaterials[i]->GetMatInfo() );
				else
					m_pMatInfo->SetSubMtl( i,NULL );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CVarBlock* CMaterial::GetPublicVars( SInputShaderResources &pShaderResources )
{
	if (pShaderResources.m_ShaderParams.empty())
		return 0;

	CVarBlock *pPublicVars = new CVarBlock;
	for (int i = 0; i < pShaderResources.m_ShaderParams.size(); i++)
	{
		IVariable *pIVar = NULL;
		SShaderParam *pParam = &pShaderResources.m_ShaderParams[i];
		switch (pParam->m_Type)
		{
		case eType_BYTE:
			{
			CVariable<int> *pVar = new CVariable<int>;
			pVar->SetName( pParam->m_Name );
			*pVar = pParam->m_Value.m_Byte;
			pPublicVars->AddVariable( pVar );
			pIVar = pVar;
			}
			break;
		case eType_SHORT:
			{
			CVariable<int> *pVar = new CVariable<int>;
			*pVar = pParam->m_Value.m_Short;
			pVar->SetName( pParam->m_Name );
			pPublicVars->AddVariable( pVar );
			pIVar = pVar;
			}
			break;
		case eType_INT:
			{
			CVariable<int> *pVar = new CVariable<int>;
			*pVar = pParam->m_Value.m_Int;
			pVar->SetName( pParam->m_Name );
			pPublicVars->AddVariable( pVar );
			pIVar = pVar;
			}
			break;
		case eType_FLOAT:
			{
			CVariable<float> *pVar = new CVariable<float>;
			*pVar = pParam->m_Value.m_Float;      
			pVar->SetName( pParam->m_Name );
			pPublicVars->AddVariable( pVar );

			pIVar = pVar;
			}
			break;
	/*
		case eType_STRING:
			pVar = new CVariable<string>;
			*pVar = pParam->m_Value.m_String;
			pVar->SetName( pParam->m_Name.c_str() );
			pPublicVars->AddVariable( pVar );
			break;
			*/
		case eType_FCOLOR:
			{
			CVariable<Vec3> *pVar = new CVariable<Vec3>;
			*pVar = Vec3(pParam->m_Value.m_Color[0],pParam->m_Value.m_Color[1],pParam->m_Value.m_Color[2]);
			pVar->SetName( pParam->m_Name );
			pVar->SetDataType( IVariable::DT_COLOR );
			pPublicVars->AddVariable( pVar );
			pIVar = pVar;
			}
			break;
		case eType_VECTOR:
			{
			CVariable<Vec3> *pVar = new CVariable<Vec3>;
			*pVar = Vec3(pParam->m_Value.m_Vector[0],pParam->m_Value.m_Vector[1],pParam->m_Value.m_Vector[2]);
			pVar->SetName( pParam->m_Name );
			pPublicVars->AddVariable( pVar );      
			pIVar = pVar;
			}
			break;
		}
		if (pIVar && pParam->m_Script.size())
		{
			ParsePublicParamsScript( pParam->m_Script.c_str(),pIVar );
		}
	}

	return pPublicVars;
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::SetPublicVars( CVarBlock *pPublicVars, SInputShaderResources &pInputShaderResources, IRenderShaderResources *pRenderShaderResources, IShader *pShader)
{
	assert( pPublicVars );

	if (pInputShaderResources.m_ShaderParams.empty())
		return;

	RecordUndo("Set Public Vars");

	int numVars = pPublicVars->GetVarsCount();

	for (int i = 0; i < numVars; i++)
	{
		if (i >= numVars)
			break;

		IVariable *pVar = pPublicVars->GetVariable(i);
		SShaderParam *pParam = NULL;
		for (int j = 0; j < pInputShaderResources.m_ShaderParams.size(); j++)
		{	
			if (strcmp(pVar->GetName(),pInputShaderResources.m_ShaderParams[j].m_Name) == 0)
			{
				pParam = &pInputShaderResources.m_ShaderParams[j];
				break;
			}
		}
		if (!pParam)
			continue;

		switch (pParam->m_Type)
		{
		case eType_BYTE:
			if (pVar->GetType() == IVariable::INT)
			{
				int val = 0;
				pVar->Get(val);
				pParam->m_Value.m_Byte = val;
			}
			break;
		case eType_SHORT:
			if (pVar->GetType() == IVariable::INT)
			{
				int val = 0;
				pVar->Get(val);
				pParam->m_Value.m_Short = val;
			}
			break;
		case eType_INT:
			if (pVar->GetType() == IVariable::INT)
			{
				int val = 0;
				pVar->Get(val);
				pParam->m_Value.m_Int = val;
			}
			break;
		case eType_FLOAT:
			if (pVar->GetType() == IVariable::FLOAT)
			{
				float val = 0;
				pVar->Get(val);
				pParam->m_Value.m_Float = val;
			}
			break;
			/*
		case eType_STRING:
			if (pVar->GetType() == IVariable::STRING)
			{
				CString str;
				int val = 0;
				pVar->Get(val);
				pParam->m_Value.m_Byte = val;
			}
			break;
			*/
		case eType_FCOLOR:
			if (pVar->GetType() == IVariable::VECTOR)
			{
				Vec3 val(0,0,0);
				pVar->Get(val);
				pParam->m_Value.m_Color[0] = val.x;
				pParam->m_Value.m_Color[1] = val.y;
				pParam->m_Value.m_Color[2] = val.z;
			}
			break;
		case eType_VECTOR:
			if (pVar->GetType() == IVariable::VECTOR)
			{
				Vec3 val(0,0,0);
				pVar->Get(val);
				pParam->m_Value.m_Vector[0] = val.x;
				pParam->m_Value.m_Vector[1] = val.y;
				pParam->m_Value.m_Vector[2] = val.z;
			}
			break;
		}
	}
  //////////////////////////////////////////////////////////////////////////
  // Set shader params.
  if (pRenderShaderResources)
    pRenderShaderResources->SetShaderParams( &pInputShaderResources, pShader );
  //////////////////////////////////////////////////////////////////////////
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::ParsePublicParamsScript( const char *sUIScript,IVariable *pVar )
{
	string uiscript = sUIScript;
	string element[3];
	int p1 = 0;
	string itemToken = uiscript.Tokenize( ";",p1 );
	while (!itemToken.empty())
	{
		int nElements = 0;
		int p2 = 0;
		string token = itemToken.Tokenize( " \t\r\n=",p2 );
		while (!token.empty())
		{
			element[nElements++] = token;
			if (nElements == 2)
			{
				element[nElements] = itemToken.substr( p2 );
				element[nElements].Trim( " =\t\"" );
				break;
			}
			token = itemToken.Tokenize( " \t\r\n=",p2 );
		};

		float minLimit,maxLimit;
		pVar->GetLimits( minLimit,maxLimit );

		if (stricmp(element[1],"UIWidget") == 0)
		{
			if (stricmp(element[2],"Color") == 0)
				pVar->SetDataType( IVariable::DT_COLOR );
		}
		else if (stricmp(element[1],"UIHelp") == 0)
		{
			string help = element[2];
			help.replace( "\\n","\n" );
			pVar->SetDescription( help );
		}
		else if (stricmp(element[1],"UIName") == 0)
		{
			pVar->SetHumanName( element[2].c_str() );
		}
		else if (stricmp(element[1],"UIMin") == 0)
		{
			pVar->SetLimits( atof(element[2]),maxLimit );

		}
		else if (stricmp(element[1],"UIMax") == 0)
		{
			pVar->SetLimits( minLimit,atof(element[2]) );
		}
		else if (stricmp(element[1],"UIStep") == 0)
		{
			
		}
		
		itemToken = uiscript.Tokenize(";",p1);

	};

}

//////////////////////////////////////////////////////////////////////////
void CMaterial::SetShaderParamsFromXml( SInputShaderResources &pShaderResources, XmlNodeRef &node )
{
	if (pShaderResources.m_ShaderParams.empty())
		return;
	
	int nValue;
	float fValue;
	Vec3 vValue;

	for (int i = 0; i < pShaderResources.m_ShaderParams.size(); i++)
	{
		SShaderParam *pParam = &pShaderResources.m_ShaderParams[i];
		switch (pParam->m_Type)
		{
		case eType_BYTE:
			if (node->getAttr(pParam->m_Name,nValue))
				pParam->m_Value.m_Byte = nValue;
			break;
		case eType_SHORT:
			if (node->getAttr(pParam->m_Name,nValue))
				pParam->m_Value.m_Short = nValue;
			break;
		case eType_INT:
			if (node->getAttr(pParam->m_Name,nValue))
				pParam->m_Value.m_Int = nValue;
			break;
		case eType_FLOAT:
			if (node->getAttr(pParam->m_Name,fValue))
				pParam->m_Value.m_Float = fValue;
			break;
		case eType_FCOLOR:
			if (node->getAttr(pParam->m_Name,vValue))
			{
				pParam->m_Value.m_Color[0] = vValue.x;
				pParam->m_Value.m_Color[1] = vValue.y;
				pParam->m_Value.m_Color[2] = vValue.z;
			}
			break;
		case eType_VECTOR:
			if (node->getAttr(pParam->m_Name,vValue))
			{
				pParam->m_Value.m_Vector[0] = vValue.x;
				pParam->m_Value.m_Vector[1] = vValue.y;
				pParam->m_Value.m_Vector[2] = vValue.z;
			}
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::SetXmlFromShaderParams( SInputShaderResources &pShaderResources, XmlNodeRef &node )
{
	for (int i = 0; i < pShaderResources.m_ShaderParams.size(); i++)
	{
		SShaderParam *pParam = &pShaderResources.m_ShaderParams[i];
		switch (pParam->m_Type)
		{
		case eType_BYTE:
			node->setAttr(pParam->m_Name,(int)pParam->m_Value.m_Byte );
			break;
		case eType_SHORT:
			node->setAttr(pParam->m_Name,(int)pParam->m_Value.m_Short );
			break;
		case eType_INT:
			node->setAttr(pParam->m_Name,(int)pParam->m_Value.m_Int );
			break;
		case eType_FLOAT:
			node->setAttr(pParam->m_Name,(float)pParam->m_Value.m_Float );
			break;
		case eType_FCOLOR:
			node->setAttr(pParam->m_Name,Vec3(pParam->m_Value.m_Color[0],pParam->m_Value.m_Color[1],pParam->m_Value.m_Color[2]) );
			break;
		case eType_VECTOR:
			node->setAttr(pParam->m_Name,Vec3(pParam->m_Value.m_Vector[0],pParam->m_Value.m_Vector[1],pParam->m_Value.m_Vector[2]) );
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CVarBlock* CMaterial::GetShaderGenParamsVars()
{	
	if (!m_shaderItem.m_pShader)
		return 0;
	IShader *pTemplShader = m_shaderItem.m_pShader;
	if (!pTemplShader)
		return 0;

	SShaderGen *pShaderGen = pTemplShader->GetGenerationParams();
	if (!pShaderGen)
		return 0;

	CVarBlock *pBlock = new CVarBlock;
	for (int i = 0; i < pShaderGen->m_BitMask.size(); i++)
	{
		SShaderGenBit *pGenBit = pShaderGen->m_BitMask[i];
		if (pGenBit->m_Flags & SHGF_HIDDEN)
			continue;
		if (!pGenBit->m_ParamProp.empty())
		{
			CVariable<bool> *pVar = new CVariable<bool>;
			pBlock->AddVariable( pVar );
			pVar->SetName( pGenBit->m_ParamProp.c_str() );
			*pVar = (pGenBit->m_Mask & m_nShaderGenMask) != 0;
			pVar->SetDescription( pGenBit->m_ParamDesc.c_str() );
		}
	}
/*
  // make sure if no valid generation parameters to not create new tab
  if(!pBlock->GetVarsCount())
  {
    SAFE_DELETE(pBlock);    
    return 0;
  }
*/
	return pBlock;
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::SetShaderGenParamsVars( CVarBlock *pBlock )
{
	RecordUndo("Change Shader GenMask");

	if (!m_shaderItem.m_pShader)
		return;
	IShader *pTemplShader = m_shaderItem.m_pShader;
	if (!pTemplShader)
		return;

	SShaderGen *pShaderGen = pTemplShader->GetGenerationParams();
	if (!pShaderGen)
		return;

	int nGenMask = 0;

	for (int i = 0; i < pShaderGen->m_BitMask.size(); i++)
	{
		SShaderGenBit *pGenBit = pShaderGen->m_BitMask[i];
		if (pGenBit->m_Flags & SHGF_HIDDEN)
			continue;

		if (!pGenBit->m_ParamProp.empty())
		{
			IVariable *pVar = pBlock->FindVariable(pGenBit->m_ParamProp.c_str());
			if (!pVar)
				continue;
			bool bFlagOn = false;
			pVar->Get(bFlagOn); 
			if (bFlagOn)
				nGenMask |= pGenBit->m_Mask;
		}
	}
	if (m_nShaderGenMask != nGenMask)
	{
		m_bRegetPublicParams = true;
		m_bKeepPublicParamsValues = true;
		m_nShaderGenMask = nGenMask;
	}
}

//////////////////////////////////////////////////////////////////////////
unsigned int CMaterial::GetTexmapUsageMask() const
{
	int mask = 0;
	if (m_shaderItem.m_pShader)
	{
		IShader *pTempl = m_shaderItem.m_pShader;
		if (pTempl)
			mask = pTempl->GetUsedTextureTypes();
	}
	return mask;
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::Update()
{
	// Reload shader item with new resources and shader.
	LoadShader();

	// Mark library as modified.
	SetModified();

	// When modifying pure child, mark his parent as modified.
	if (IsPureChild() && m_pParent)
		m_pParent->SetModified();

	GetIEditor()->SetModifiedFlag(); 
}

namespace
{
	inline Vec3 ToVec3( const ColorF &col ) { return Vec3(col.r,col.g,col.b); }
	inline ColorF ToCFColor( const Vec3 &col ) { return ColorF(col); }
};

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
	{ EFTT_ENV,		"Environment" },
	{ EFTT_DETAIL_OVERLAY,"Detail" },
	{ EFTT_OPACITY,		"Opacity" },
	{ EFTT_DECAL_OVERLAY,	"Decal" },
  { EFTT_SUBSURFACE,	"SubSurface" },
  { EFTT_CUSTOM,	"Custom" },
  { EFTT_CUSTOM_SECONDARY,	"[1] Custom" },
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CMaterial::Serialize( SerializeContext &ctx )
{
	//CBaseLibraryItem::Serialize( ctx );

	XmlNodeRef node = ctx.node;
	if (ctx.bLoading)
	{
		m_bIgnoreNotifyChange = true;
		m_bRegetPublicParams = true;

		SInputShaderResources &sr = m_shaderResources;

		m_shaderResources = defaultShaderResource;

		// Loading
		int flags = m_mtlFlags;
		if (node->getAttr( "MtlFlags",flags ))
		{
			m_mtlFlags &= ~(MTL_FLAGS_SAVE_MASK);
			m_mtlFlags |= (flags & (MTL_FLAGS_SAVE_MASK));
		}


		if (!IsMultiSubMaterial())
		{
			node->getAttr( "Shader",m_shaderName );
			node->getAttr( "GenMask",m_nShaderGenMask );
			node->getAttr( "SurfaceType",m_surfaceType );

			// Load lighting data.
			Vec3 vColor;
			if (node->getAttr( "Diffuse",vColor ))
				m_shaderResources.m_LMaterial.m_Diffuse = ToCFColor(vColor);
			if (node->getAttr( "Specular",vColor ))
				m_shaderResources.m_LMaterial.m_Specular = ToCFColor(vColor);
			if (node->getAttr( "Emissive",vColor ))
				m_shaderResources.m_LMaterial.m_Emission = ToCFColor(vColor);

			node->getAttr( "Shininess",m_shaderResources.m_LMaterial.m_SpecShininess );
			node->getAttr( "Opacity",sr.m_Opacity );
			node->getAttr( "AlphaTest",sr.m_AlphaRef );
      node->getAttr( "GlowAmount",sr.m_GlowAmount );
      node->getAttr( "PostEffectType", sr.m_PostEffects );
      
      //node->getAttr( "MotionBlurAmount",sr.m_MotionBlurAmount);
			//node->getAttr( "UseScatter",sr.m_bUseScatterPass);
			//node->getAttr( "TranslucenseLayer",sr.m_bTranslucenseLayer);


      int vertModif = sr.m_DeformInfo.m_eType;
      if (node->getAttr( "vertModifType", vertModif ))
        sr.m_DeformInfo.m_eType = (EDeformType)vertModif;

			CString texmap,file;
			// 
			XmlNodeRef texturesNode = node->findChild( "Textures" );
			if (texturesNode)
			{
				for (int i = 0; i < texturesNode->getChildCount(); i++)
				{
					texmap = "";
					XmlNodeRef texNode = texturesNode->getChild(i);
					texNode->getAttr( "Map",texmap );

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
					texNode->getAttr( "File",file );

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
			int type = eDT_Unknown;
			deformNode->getAttr( "Type",type );
			m_shaderResources.m_DeformInfo.m_eType = (EDeformType)type;
			deformNode->getAttr( "DividerX",m_shaderResources.m_DeformInfo.m_fDividerX );
			deformNode->getAttr( "DividerY",m_shaderResources.m_DeformInfo.m_fDividerY );
			deformNode->getAttr( "NoiseScale",m_shaderResources.m_DeformInfo.m_vNoiseScale );

			XmlNodeRef waveX = deformNode->findChild("WaveX");
			if (waveX)
			{
				int type = eWF_None;
				waveX->getAttr( "Type",type );
				m_shaderResources.m_DeformInfo.m_WaveX.m_eWFType = (EWaveForm)type;
				waveX->getAttr( "Amp",m_shaderResources.m_DeformInfo.m_WaveX.m_Amp );
				waveX->getAttr( "Level",m_shaderResources.m_DeformInfo.m_WaveX.m_Level );
				waveX->getAttr( "Phase",m_shaderResources.m_DeformInfo.m_WaveX.m_Phase );
				waveX->getAttr( "Freq",m_shaderResources.m_DeformInfo.m_WaveX.m_Freq );
			}
			XmlNodeRef waveY = deformNode->findChild("WaveY");
			if (waveY)
			{
				int type = eWF_None;
				waveY->getAttr( "Type",type );
				m_shaderResources.m_DeformInfo.m_WaveY.m_eWFType = (EWaveForm)type;
				waveY->getAttr( "Amp",m_shaderResources.m_DeformInfo.m_WaveY.m_Amp );
				waveY->getAttr( "Level",m_shaderResources.m_DeformInfo.m_WaveY.m_Level );
				waveY->getAttr( "Phase",m_shaderResources.m_DeformInfo.m_WaveY.m_Phase );
				waveY->getAttr( "Freq",m_shaderResources.m_DeformInfo.m_WaveY.m_Freq );
			}
		}

		ClearAllSubMaterials();
		// Serialize sub materials.
		XmlNodeRef childsNode = node->findChild( "SubMaterials" );
		if (childsNode)
		{
			if (!ctx.bIgnoreChilds)
			{
				CString name;
				int nSubMtls = childsNode->getChildCount();
				SetSubMaterialCount(nSubMtls);
				for (int i = 0; i < nSubMtls; i++)
				{
					XmlNodeRef mtlNode = childsNode->getChild(i);
					if (mtlNode->isTag("Material"))
					{
						mtlNode->getAttr("Name",name);
						CMaterial *pSubMtl = new CMaterial(m_pManager,name,MTL_FLAG_PURE_CHILD);
						SetSubMaterial( i,pSubMtl );

						SerializeContext childCtx(ctx);
						childCtx.node = mtlNode;
						pSubMtl->Serialize( childCtx );
					}
					else
					{
						if (mtlNode->getAttr("Name",name))
						{
							CMaterial *pMtl = m_pManager->LoadMaterial(name);
							if (pMtl)
								SetSubMaterial(i,pMtl);
						}
					}
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Load public parameters.
		//////////////////////////////////////////////////////////////////////////
		m_publicVarsCache = node->findChild( "PublicParams" );

    //////////////////////////////////////////////////////////////////////////
    // Load material layers data
    //////////////////////////////////////////////////////////////////////////

    XmlNodeRef mtlLayersNode = node->findChild( "MaterialLayers" ); 
    if(mtlLayersNode)
    {
      int nChildCount = min( (int) MTL_LAYER_MAX_SLOTS, (int)  mtlLayersNode->getChildCount() );
      for (int l = 0; l < nChildCount; ++l)
      {
        XmlNodeRef layerNode = mtlLayersNode->getChild(l);
        if(layerNode)
        {
          if( layerNode->getAttr( "Name", m_pMtlLayerResources[l].m_shaderName) )
          {
            m_pMtlLayerResources[l].m_bRegetPublicParams = true;

            bool bNoDraw = false;
            layerNode->getAttr( "NoDraw", bNoDraw);          

            m_pMtlLayerResources[l].m_publicVarsCache = layerNode->findChild( "PublicParams" );

            if ( bNoDraw )
              m_pMtlLayerResources[l].m_nFlags |= MTL_LAYER_USAGE_NODRAW;
            else
              m_pMtlLayerResources[l].m_nFlags &= ~MTL_LAYER_USAGE_NODRAW;
          }
        }
      }
    }

		if (ctx.bUndo)
		{
			LoadShader();
			UpdateMatInfo();
		}
		
		m_bIgnoreNotifyChange = false;
		SetModified(false);

		// If copy pasting or undo send update event.
		if (ctx.bCopyPaste || ctx.bUndo)
			NotifyChanged();
	}
	else
	{
		// Saving.
		node->setAttr( "MtlFlags",m_mtlFlags );

		if (!IsMultiSubMaterial())
		{
			node->setAttr( "Shader",m_shaderName );
			node->setAttr( "GenMask",m_nShaderGenMask );
			node->setAttr( "SurfaceType",m_surfaceType );

			SInputShaderResources &sr = m_shaderResources;
			//if (!m_shaderName.IsEmpty() && (stricmp(m_shaderName,"nodraw") != 0))
			{
				// Save ligthing data.
				if (defaultShaderResource.m_LMaterial.m_Diffuse != m_shaderResources.m_LMaterial.m_Diffuse)
					node->setAttr( "Diffuse",ToVec3(m_shaderResources.m_LMaterial.m_Diffuse) );
				if (defaultShaderResource.m_LMaterial.m_Specular != m_shaderResources.m_LMaterial.m_Specular)
					node->setAttr( "Specular",ToVec3(m_shaderResources.m_LMaterial.m_Specular) );
				if (defaultShaderResource.m_LMaterial.m_Emission != m_shaderResources.m_LMaterial.m_Emission)
					node->setAttr( "Emissive",ToVec3(m_shaderResources.m_LMaterial.m_Emission) );
				if (defaultShaderResource.m_LMaterial.m_SpecShininess != m_shaderResources.m_LMaterial.m_SpecShininess)
					node->setAttr( "Shininess",m_shaderResources.m_LMaterial.m_SpecShininess );

				if (defaultShaderResource.m_Opacity != sr.m_Opacity)
					node->setAttr( "Opacity",sr.m_Opacity );
				if (defaultShaderResource.m_AlphaRef != sr.m_AlphaRef)
					node->setAttr( "AlphaTest",sr.m_AlphaRef );
        if (defaultShaderResource.m_GlowAmount != sr.m_GlowAmount)
          node->setAttr( "GlowAmount",sr.m_GlowAmount);
        if (defaultShaderResource.m_PostEffects != sr.m_PostEffects)
          node->setAttr( "PostEffectType",sr.m_PostEffects);

        if (defaultShaderResource.m_DeformInfo.m_eType != sr.m_DeformInfo.m_eType)
          node->setAttr( "vertModifType",sr.m_DeformInfo.m_eType );

				// Save texturing data.
				XmlNodeRef texturesNode = node->newChild( "Textures" );
				for (int i = 0; i < sizeof(sUsedTextures)/sizeof(sUsedTextures[0]); i++)
				{
					int texId = sUsedTextures[i].texId;
					if (!sr.m_Textures[texId].m_Name.empty())
					{
						XmlNodeRef texNode = texturesNode->newChild( "Texture" );
						//			texNode->setAttr( "TexID",texId );
						texNode->setAttr( "Map",sUsedTextures[i].name );
						texNode->setAttr( "File",sr.m_Textures[texId].m_Name.c_str() );

						if (sr.m_Textures[texId].m_Filter != defaultShaderResource.m_Textures[texId].m_Filter)
							texNode->setAttr( "Filter",sr.m_Textures[texId].m_Filter );
						if (sr.m_Textures[texId].m_Amount != defaultShaderResource.m_Textures[texId].m_Amount)
							texNode->setAttr( "Amount",sr.m_Textures[texId].m_Amount );
						if (sr.m_Textures[texId].m_bUTile != defaultShaderResource.m_Textures[texId].m_bUTile)
							texNode->setAttr( "IsTileU",sr.m_Textures[texId].m_bUTile );
						if (sr.m_Textures[texId].m_bVTile != defaultShaderResource.m_Textures[texId].m_bVTile)
							texNode->setAttr( "IsTileV",sr.m_Textures[texId].m_bVTile );
						if (sr.m_Textures[texId].m_Sampler.m_eTexType != defaultShaderResource.m_Textures[texId].m_Sampler.m_eTexType)
							texNode->setAttr( "TexType",sr.m_Textures[texId].m_Sampler.m_eTexType );

						SEfTexModificator &texm = sr.m_Textures[texId].m_TexModificator;
						if (memcmp(&texm,&defaultTexMod,sizeof(texm)) == 0)
							continue;

						XmlNodeRef modNode = texNode->newChild( "TexMod" );
						//////////////////////////////////////////////////////////////////////////
						// Save texture modificators Modificators
						//////////////////////////////////////////////////////////////////////////
						if (texm.m_Tiling[0] != defaultTexMod.m_Tiling[0])
							modNode->setAttr( "TileU",texm.m_Tiling[0] );
						if (texm.m_Tiling[1] != defaultTexMod.m_Tiling[1])
							modNode->setAttr( "TileV",texm.m_Tiling[1] );
						if (texm.m_Offs[0] != defaultTexMod.m_Offs[0])
							modNode->setAttr( "OffsetU",texm.m_Offs[0] );
						if (texm.m_Offs[1] != defaultTexMod.m_Offs[1])
							modNode->setAttr( "OffsetV",texm.m_Offs[1] );
						if (texm.m_Rot[0] != defaultTexMod.m_Rot[0])
							modNode->setAttr( "RotateU",Word2Degr(texm.m_Rot[0]) );
						if (texm.m_Rot[1] != defaultTexMod.m_Rot[1])
							modNode->setAttr( "RotateV",Word2Degr(texm.m_Rot[1]) );
						if (texm.m_Rot[2] != defaultTexMod.m_Rot[2])
							modNode->setAttr( "RotateW",Word2Degr(texm.m_Rot[2]) );
						if (texm.m_eUMoveType != defaultTexMod.m_eUMoveType)
							modNode->setAttr( "TexMod_UOscillatorType",texm.m_eUMoveType );
						if (texm.m_eVMoveType != defaultTexMod.m_eVMoveType)
							modNode->setAttr( "TexMod_VOscillatorType",texm.m_eVMoveType );
						if (texm.m_eRotType != defaultTexMod.m_eRotType)
							modNode->setAttr( "TexMod_RotateType",texm.m_eRotType );
						if (texm.m_eTGType != defaultTexMod.m_eTGType)
							modNode->setAttr( "TexMod_TexGenType",texm.m_eTGType );

						if (texm.m_RotOscRate[0] != defaultTexMod.m_RotOscRate[0])
							modNode->setAttr( "TexMod_URotateRate",Word2Degr(texm.m_RotOscRate[0]) );
						if (texm.m_RotOscPhase[0] != defaultTexMod.m_RotOscPhase[0])
							modNode->setAttr( "TexMod_URotatePhase",Word2Degr(texm.m_RotOscPhase[0]) );
						if (texm.m_RotOscAmplitude[0] != defaultTexMod.m_RotOscAmplitude[0])
							modNode->setAttr( "TexMod_URotateAmplitude",Word2Degr(texm.m_RotOscAmplitude[0]) );
						if (texm.m_RotOscRate[1] != defaultTexMod.m_RotOscRate[1])
							modNode->setAttr( "TexMod_VRotateRate",Word2Degr(texm.m_RotOscRate[1]) );
						if (texm.m_RotOscPhase[1] != defaultTexMod.m_RotOscPhase[1])
							modNode->setAttr( "TexMod_VRotatePhase",Word2Degr(texm.m_RotOscPhase[1]) );
						if (texm.m_RotOscAmplitude[1] != defaultTexMod.m_RotOscAmplitude[1])
							modNode->setAttr( "TexMod_VRotateAmplitude",Word2Degr(texm.m_RotOscAmplitude[1]) );
						if (texm.m_RotOscRate[2] != defaultTexMod.m_RotOscRate[2])
							modNode->setAttr( "TexMod_WRotateRate",Word2Degr(texm.m_RotOscRate[2]) );
						if (texm.m_RotOscPhase[2] != defaultTexMod.m_RotOscPhase[2])
							modNode->setAttr( "TexMod_WRotatePhase",Word2Degr(texm.m_RotOscPhase[2]) );
						if (texm.m_RotOscAmplitude[2] != defaultTexMod.m_RotOscAmplitude[2])
							modNode->setAttr( "TexMod_WRotateAmplitude",Word2Degr(texm.m_RotOscAmplitude[2]) );
						if (texm.m_RotOscCenter[0] != defaultTexMod.m_RotOscCenter[0])
							modNode->setAttr( "TexMod_URotateCenter",texm.m_RotOscCenter[0] );
						if (texm.m_RotOscCenter[1] != defaultTexMod.m_RotOscCenter[1])
							modNode->setAttr( "TexMod_VRotateCenter",texm.m_RotOscCenter[1] );
						if (texm.m_RotOscCenter[2] != defaultTexMod.m_RotOscCenter[2])
							modNode->setAttr( "TexMod_WRotateCenter",texm.m_RotOscCenter[2] );

						if (texm.m_UOscRate != defaultTexMod.m_UOscRate)
							modNode->setAttr( "TexMod_UOscillatorRate",texm.m_UOscRate );
						if (texm.m_VOscRate != defaultTexMod.m_VOscRate)
							modNode->setAttr( "TexMod_VOscillatorRate",texm.m_VOscRate );
						if (texm.m_UOscPhase != defaultTexMod.m_UOscPhase)
							modNode->setAttr( "TexMod_UOscillatorPhase",texm.m_UOscPhase );
						if (texm.m_VOscPhase != defaultTexMod.m_VOscPhase)
							modNode->setAttr( "TexMod_VOscillatorPhase",texm.m_VOscPhase );
						if (texm.m_UOscAmplitude != defaultTexMod.m_UOscAmplitude)
							modNode->setAttr( "TexMod_UOscillatorAmplitude",texm.m_UOscAmplitude );
						if (texm.m_VOscAmplitude != defaultTexMod.m_VOscAmplitude)
							modNode->setAttr( "TexMod_VOscillatorAmplitude",texm.m_VOscAmplitude );
					}
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Check if we have vertex deform.
		//////////////////////////////////////////////////////////////////////////
		if (m_shaderResources.m_DeformInfo.m_eType != eDT_Unknown)
		{
			XmlNodeRef deformNode = node->newChild("VertexDeform");
			deformNode->setAttr( "Type",m_shaderResources.m_DeformInfo.m_eType );
			deformNode->setAttr( "DividerX",m_shaderResources.m_DeformInfo.m_fDividerX );
			deformNode->setAttr( "DividerY",m_shaderResources.m_DeformInfo.m_fDividerY );
			deformNode->setAttr( "NoiseScale",m_shaderResources.m_DeformInfo.m_vNoiseScale );

			if (m_shaderResources.m_DeformInfo.m_WaveX.m_eWFType != eWF_None)
			{
				XmlNodeRef waveX = deformNode->newChild("WaveX");
				waveX->setAttr( "Type",m_shaderResources.m_DeformInfo.m_WaveX.m_eWFType );
				waveX->setAttr( "Amp",m_shaderResources.m_DeformInfo.m_WaveX.m_Amp );
				waveX->setAttr( "Level",m_shaderResources.m_DeformInfo.m_WaveX.m_Level );
				waveX->setAttr( "Phase",m_shaderResources.m_DeformInfo.m_WaveX.m_Phase );
				waveX->setAttr( "Freq",m_shaderResources.m_DeformInfo.m_WaveX.m_Freq );
			}
			if (m_shaderResources.m_DeformInfo.m_WaveY.m_eWFType != eWF_None)
			{
				XmlNodeRef waveY = deformNode->newChild("WaveY");
				waveY->setAttr( "Type",m_shaderResources.m_DeformInfo.m_WaveY.m_eWFType );
				waveY->setAttr( "Amp",m_shaderResources.m_DeformInfo.m_WaveY.m_Amp );
				waveY->setAttr( "Level",m_shaderResources.m_DeformInfo.m_WaveY.m_Level );
				waveY->setAttr( "Phase",m_shaderResources.m_DeformInfo.m_WaveY.m_Phase );
				waveY->setAttr( "Freq",m_shaderResources.m_DeformInfo.m_WaveY.m_Freq );
			}
		}

		if (GetSubMaterialCount() > 0)
		{
			// Serialize sub materials.
			XmlNodeRef childsNode = node->newChild( "SubMaterials" );
			for (int i = 0; i < GetSubMaterialCount(); i++)
			{
				CMaterial *pSubMtl = GetSubMaterial(i);
				if (pSubMtl && pSubMtl->IsPureChild())
				{
					XmlNodeRef mtlNode = childsNode->newChild( "Material" );
					mtlNode->setAttr( "Name",pSubMtl->GetName() );
					SerializeContext childCtx(ctx);
					childCtx.node = mtlNode;
					GetSubMaterial(i)->Serialize( childCtx );
				}
				else
				{
					XmlNodeRef mtlNode = childsNode->newChild( "MaterialRef" );
					if (pSubMtl)
						mtlNode->setAttr( "Name",pSubMtl->GetName() );
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Save public parameters.
		//////////////////////////////////////////////////////////////////////////
		if (m_publicVarsCache)
		{
			node->addChild( m_publicVarsCache );
		}
		else if (!m_shaderResources.m_ShaderParams.empty())
		{
			XmlNodeRef publicsNode = node->newChild( "PublicParams" );
      SetXmlFromShaderParams( m_shaderResources, publicsNode );
		}

    //////////////////////////////////////////////////////////////////////////
    // Save material layers data
    //////////////////////////////////////////////////////////////////////////
 
    bool bMaterialLayers = false;
    for(int l(0); l < MTL_LAYER_MAX_SLOTS; ++l)
    {
      if(!m_pMtlLayerResources[l].m_shaderName.IsEmpty())
      {
        bMaterialLayers = true;
        break;
      }
    }

    if( bMaterialLayers )
    {
      XmlNodeRef mtlLayersNode = node->newChild( "MaterialLayers" );    
      for(int l(0); l < MTL_LAYER_MAX_SLOTS; ++l)
      {
        XmlNodeRef layerNode = mtlLayersNode->newChild( "Layer" );
        if(!m_pMtlLayerResources[l].m_shaderName.IsEmpty())
        {
          layerNode->setAttr( "Name", m_pMtlLayerResources[l].m_shaderName);                        
          layerNode->setAttr( "NoDraw", m_pMtlLayerResources[l].m_nFlags & MTL_LAYER_USAGE_NODRAW );     

          if(m_pMtlLayerResources[l].m_publicVarsCache)
          {
            layerNode->addChild( m_pMtlLayerResources[l].m_publicVarsCache );
          }
          else if( !m_pMtlLayerResources[l].m_shaderResources.m_ShaderParams.empty() ) 
          {
            XmlNodeRef publicsNode = layerNode->newChild( "PublicParams" );
            SetXmlFromShaderParams( m_pMtlLayerResources[l].m_shaderResources, publicsNode ); 
          }
        }
      }
    }

	}
}

/*
//////////////////////////////////////////////////////////////////////////
void CMaterial::SerializePublics( XmlNodeRef &node,bool bLoading )
{
	if (bLoading)
	{
	}
	else
	{
		if (m_shaderParams.empty())
			return;
		XmlNodeRef publicsNode = node->newChild( "PublicParams" );

		for (int i = 0; i < m_shaderParams.size(); i++)
		{
			XmlNodeRef paramNode = node->newChild( "Param" );
			SShaderParam *pParam = &m_shaderParams[i];
			paramNode->setAttr( "Name",pParam->m_Name );
			switch (pParam->m_Type)
			{
			case eType_BYTE:
				paramNode->setAttr( "Value",(int)pParam->m_Value.m_Byte );
				paramNode->setAttr( "Type",(int)pParam->m_Value.m_Byte );
				break;
			case eType_SHORT:
				paramNode->setAttr( "Value",(int)pParam->m_Value.m_Short );
				break;
			case eType_INT:
				paramNode->setAttr( "Value",(int)pParam->m_Value.m_Int );
				break;
			case eType_FLOAT:
				paramNode->setAttr( "Value",pParam->m_Value.m_Float );
				break;
			case eType_STRING:
				paramNode->setAttr( "Value",pParam->m_Value.m_String );
			break;
			case eType_FCOLOR:
				paramNode->setAttr( "Value",Vec3(pParam->m_Value.m_Color[0],pParam->m_Value.m_Color[1],pParam->m_Value.m_Color[2]) );
				break;
			case eType_VECTOR:
				paramNode->setAttr( "Value",Vec3(pParam->m_Value.m_Vector[0],pParam->m_Value.m_Vector[1],pParam->m_Value.m_Vector[2]) );
				break;
			}
		}
	}
}
*/

//////////////////////////////////////////////////////////////////////////
void CMaterial::AssignToEntity( IRenderNode *pEntity )
{
	assert( pEntity );
	pEntity->SetMaterial( GetMatInfo() );
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::SetFromMatInfo( IMaterial *pMatInfo )
{
	assert( pMatInfo );

	m_bFromEngine = true;
	m_shaderName = "";
	
	ClearMatInfo();
	SetModified(true);

	m_mtlFlags = pMatInfo->GetFlags();
	if (m_mtlFlags & MTL_FLAG_MULTI_SUBMTL)
	{
		// Create sub materials.
		SetSubMaterialCount( pMatInfo->GetSubMtlCount() );
		for (int i = 0; i < GetSubMaterialCount(); i++)
		{
			IMaterial *pChildMatInfo = pMatInfo->GetSubMtl(i);
			if (!pChildMatInfo)
				continue;

			if (pChildMatInfo->GetFlags() & MTL_FLAG_PURE_CHILD)
			{
				CMaterial *pChild = new CMaterial( GetMatManager(),pChildMatInfo->GetName(),pChildMatInfo->GetFlags() );
				pChild->SetFromMatInfo( pChildMatInfo );
				SetSubMaterial(i,pChild);
			}
			else
			{
				CMaterial *pChild = GetMatManager()->LoadMaterial( pChildMatInfo->GetName() );
				pChild->SetFromMatInfo( pChildMatInfo );
				SetSubMaterial(i,pChild);
			}
		}
	}
	else
	{
		SAFE_RELEASE( m_shaderItem.m_pShader );
		SAFE_RELEASE( m_shaderItem.m_pShaderResources );
		m_shaderItem = pMatInfo->GetShaderItem();
		if (m_shaderItem.m_pShader)
			m_shaderItem.m_pShader->AddRef();
		if (m_shaderItem.m_pShaderResources)
			m_shaderItem.m_pShaderResources->AddRef();

		if (m_shaderItem.m_pShaderResources)
		{
			m_shaderResources = SInputShaderResources(m_shaderItem.m_pShaderResources);
		}
		if (m_shaderItem.m_pShader)
		{
			// Get name of template.
			IShader *pTemplShader = m_shaderItem.m_pShader;
			if (pTemplShader)
			{
				m_shaderName = pTemplShader->GetName();
				m_nShaderGenMask = pTemplShader->GetGenerationMask();
			}
		}
		ISurfaceType *pSurfaceType = pMatInfo->GetSurfaceType();
		if (pSurfaceType)
			m_surfaceType = pSurfaceType->GetName();
		else
			m_surfaceType = "";
	}

	// Mark as not modified.
	SetModified(false);

	//////////////////////////////////////////////////////////////////////////
	// Assign mat info.
	m_pMatInfo = pMatInfo;
	m_pMatInfo->SetUserData( this );
	AddRef(); // Let IMaterial keep a reference to us.
}

//////////////////////////////////////////////////////////////////////////
int CMaterial::GetSubMaterialCount() const
{
	return m_subMaterials.size();
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::SetSubMaterialCount( int nSubMtlsCount )
{
	RecordUndo("Multi Material Change");
	m_subMaterials.resize(nSubMtlsCount);
	UpdateMatInfo();
	NotifyChanged();
}
	
//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterial::GetSubMaterial( int index ) const
{
	assert( index >= 0 && index < m_subMaterials.size() );
	return m_subMaterials[index];
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::SetSubMaterial( int nSlot,CMaterial *mtl )
{
	RecordUndo("Multi Material Change");
	assert( nSlot >= 0 && nSlot < m_subMaterials.size() );
	if (mtl)
	{
		if (mtl->m_bFromEngine && !m_bFromEngine)
		{
			// Do not allow to assign engine materials to sub-slots of real materials.
			return;
		}
		if (mtl->IsMultiSubMaterial())
			return;
		if (mtl->IsPureChild())
			mtl->m_pParent = this;
	}

	if (m_subMaterials[nSlot])
		m_subMaterials[nSlot]->m_pParent = NULL;
	m_subMaterials[nSlot] = mtl;
	
	UpdateMatInfo();
	NotifyChanged();
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::ClearAllSubMaterials()
{
	RecordUndo("Multi Material Change");
	for (int i = 0; i < m_subMaterials.size(); i++)
	{
		if (m_subMaterials[i])
		{
			m_subMaterials[i]->m_pParent = NULL;
			m_subMaterials[i] = NULL;
		}
	}
	UpdateMatInfo();
	NotifyChanged();
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::Validate()
{
	if (IsDummy())
	{
		CErrorRecord err;
		err.error.Format( _T("Material %s file not found"),(const char*)GetName() );
		err.pItem = this;
		GetIEditor()->GetErrorReport()->ReportError( err );
	}
	// Reload shader.
	LoadShader();

	// Validate sub materials.
	for (int i = 0; i < m_subMaterials.size(); i++)
	{
		if (m_subMaterials[i])
			m_subMaterials[i]->Validate();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::GatherUsedResources( CUsedResources &resources )
{
	if (!IsUsed())
		return;

	SInputShaderResources &sr = GetShaderResources();
	for (int texid = 0; texid < EFTT_MAX; texid++)
	{
		if (!sr.m_Textures[texid].m_Name.empty())
		{
			resources.Add( sr.m_Textures[texid].m_Name.c_str() );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CMaterial::CanModify()
{
	if (m_bDummyMaterial)
		return false;

	if (m_bFromEngine)
		return false;

	if (IsPureChild() && GetParent())
		return GetParent()->CanModify();

	// If read only or in pack, do not save.
	if (m_scFileAttributes&(SCC_FILE_ATTRIBUTE_READONLY|SCC_FILE_ATTRIBUTE_INPAK))
		return false;

	// Managed file must be checked out.
	if ((m_scFileAttributes&SCC_FILE_ATTRIBUTE_MANAGED) && !(m_scFileAttributes&SCC_FILE_ATTRIBUTE_CHECKEDOUT))
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CMaterial::Save()
{
	if (!CanModify())
		return false;

	// Save our parent
	if (IsPureChild())
	{
		if (m_pParent)
			m_pParent->Save();
		return false;
	}

	// If filename is empty do not not save.
	if (GetFilename().IsEmpty())
		return false;

	// Save material XML to a file that corresponds to the material name with extension .mtl.
	XmlNodeRef mtlNode = CreateXmlNode( "Material" );
	CBaseLibraryItem::SerializeContext ctx( mtlNode,false );
	Serialize( ctx );

	//CMaterialManager *pMatMan = (CMaterialManager*)GetLibrary()->GetManager();
	// get file name from material name.
	//CString filename = pMatMan->MaterialToFilename( GetName() );

	//char path[ICryPak::g_nMaxPath];
	//filename = gEnv->pCryPak->AdjustFileName( filename,path,0 );

	if (SaveXmlNode( mtlNode,GetFilename()))
	{
		// If material successfully saved, clear modified flag.
		SetModified(false);
		return true;
	}
	else
		return false;
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::ClearMatInfo()
{
	IMaterial *pMatInfo = m_pMatInfo;
	m_pMatInfo = 0;
	// This call can release CMaterial.
	if (pMatInfo)
		Release();
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMaterial::GetMatInfo()
{
	if (!m_pMatInfo)
	{
    
		if (m_bDummyMaterial)
		{
			m_pMatInfo = GetIEditor()->Get3DEngine()->GetMaterialManager()->GetDefaultMaterial();
			AddRef(); // Always keep dummy materials.
			return m_pMatInfo;
		}

		if (!IsMultiSubMaterial() && !m_shaderItem.m_pShader)
		{
			LoadShader();
		}

		if (!IsPureChild())
			m_pMatInfo = GetIEditor()->Get3DEngine()->GetMaterialManager()->CreateMaterial( GetName(),m_mtlFlags );
		else
		{
			// Pure child should not be registered with the name.
			m_pMatInfo = GetIEditor()->Get3DEngine()->GetMaterialManager()->CreateMaterial( "",m_mtlFlags );
			m_pMatInfo->SetName( GetName() );
		}
		m_mtlFlags = m_pMatInfo->GetFlags();
		m_pMatInfo->SetUserData( this );
		UpdateMatInfo();

		AddRef(); // Let IMaterial keep a reference to us.
	}
	return m_pMatInfo;
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::NotifyChanged()
{
	if (!CanModify() && !IsModified() && CUndo::IsRecording())
	{
		// Display Warning message.
		Warning( "Modifying read only material %s\r\nChanges will not be saved!",(const char*)GetName() );
	}

	SetModified();
	if (!m_bIgnoreNotifyChange)
		m_pManager->OnItemChanged(this);
} 

//////////////////////////////////////////////////////////////////////////
class CUndoMaterial : public IUndoObject
{
public:
	CUndoMaterial( CMaterial *pMaterial,const char *undoDescription )
	{
		// Stores the current state of this object.
		m_undoDescription = undoDescription;
		m_mtlName = pMaterial->GetName();
		// Save material XML to a file that corresponds to the material name with extension .mtl.
		m_undo = CreateXmlNode( "Material" );
		CBaseLibraryItem::SerializeContext ctx( m_undo,false );
		pMaterial->Serialize( ctx );
	}
protected:
	virtual void Release() { delete this; };
	virtual int GetSize()
	{
		return sizeof(*this) + m_undoDescription.GetLength();
	}
	virtual const char* GetDescription() { return m_undoDescription; };

	virtual void Undo( bool bUndo )
	{
		CMaterial* pMaterial = (CMaterial*)GetIEditor()->GetMaterialManager()->FindItemByName(m_mtlName);
		if (!pMaterial)
			return;

		if (bUndo)
		{
			// Save current object state.
			m_redo = CreateXmlNode( "Material" );
			CBaseLibraryItem::SerializeContext ctx( m_redo,false );
			pMaterial->Serialize( ctx );
		}
		CBaseLibraryItem::SerializeContext ctx( m_undo,true );
		ctx.bUndo = true;
		pMaterial->Serialize( ctx );
	}
	virtual void Redo()
	{
		CMaterial* pMaterial = (CMaterial*)GetIEditor()->GetMaterialManager()->FindItemByName(m_mtlName);
		if (!pMaterial)
			return;

		CBaseLibraryItem::SerializeContext ctx( m_redo,true );
		ctx.bUndo = true;
		pMaterial->Serialize( ctx );
	}

private:
	CString m_undoDescription;
	CString m_mtlName;
	XmlNodeRef m_undo;
	XmlNodeRef m_redo;
};

//////////////////////////////////////////////////////////////////////////
void CMaterial::RecordUndo( const char *sText )
{
	if (CUndo::IsRecording())
	{
		CUndo::Record( new CUndoMaterial(this,sText) );
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::OnMakeCurrent()
{
	UpdateFileAttributes();
	
	// If Shader not yet loaded, load it now.
	if (!m_shaderItem.m_pShader)
		LoadShader();
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::SetSurfaceTypeName( const CString &surfaceType )
{
	m_surfaceType = surfaceType;
	UpdateMatInfo();
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::Reload()
{
	if (IsPureChild())
	{
		if (m_pParent)
			m_pParent->Reload();
		return;
	}
	if (IsDummy())
		return;

	XmlNodeRef mtlNode = GetISystem()->LoadXmlFile( GetFilename() );
	if (!mtlNode)
	{
		return;
	}
	CBaseLibraryItem::SerializeContext serCtx( mtlNode,true );
	serCtx.bUndo = true; // Simulate undo.
	Serialize( serCtx );
	UpdateMatInfo();
	NotifyChanged();
}

//////////////////////////////////////////////////////////////////////////
