// StaticObjParam.cpp: implementation of the CVegetationObject class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "VegetationObject.h"
#include "Material\Material.h"
#include "Material\MaterialManager.h"
#include "ErrorReport.h"

#include "Heightmap.h"
#include "VegetationMap.h"

#include "I3DEngine.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CVegetationObject::CVegetationObject( int id,CVegetationMap *pVegMap )
{
	m_vegetationMap = pVegMap;
	m_id = id;

	m_statObj = 0;
	m_objectSize = 1;
	m_numInstances = 0;
	m_bSelected = false;
	m_bHidden = false;
	m_index = 0;
	m_bInUse = true;

	m_bVarIgnoreChange = false;
	
	m_category = _T("Default");

	// Int vars.
	mv_size = 1;
	mv_hmin = GetIEditor()->GetHeightmap()->GetWaterLevel();
	mv_hmax = 4096;
	mv_slope_min = 0;
	mv_slope_max = 255;

  
	mv_density = 1;
  mv_bending = 1.0f;
	mv_sizevar = 0;
	mv_castShadows = false;
	mv_recvShadows = false;
	mv_precalcShadows = false;
	mv_alphaBlend = false;
	mv_hideable = 0;
	//mv_hideableSecondary = false;
  mv_pickable = false;
  mv_aiRadius = -1.0f;
	mv_SpriteDistRatio = 1;
	mv_MaxViewDistRatio = 1;
	mv_ShadowDistRatio = 1;
	mv_brightness = 1;
  mv_UseSprites = true;
  mv_layerFrozen = false;
	mv_minSpec = 0;
  
	CoCreateGuid(&m_guid);

	mv_hideable.AddEnumItem("None", 0);
	mv_hideable.AddEnumItem("Hideable", 1);
	mv_hideable.AddEnumItem("Secondary", 2);

	mv_minSpec.AddEnumItem( "All",0 );
	mv_minSpec.AddEnumItem( "Low",CONFIG_LOW_SPEC );
	mv_minSpec.AddEnumItem( "Medium",CONFIG_MEDIUM_SPEC );
	mv_minSpec.AddEnumItem( "High",CONFIG_HIGH_SPEC );
	mv_minSpec.AddEnumItem( "VeryHigh",CONFIG_VERYHIGH_SPEC );
	mv_minSpec.AddEnumItem( "Detail",CONFIG_DETAIL_SPEC );

	AddVariable( mv_size,"Size",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_sizevar,"SizeVar",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_rotation,"RandomRotation",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_alignToTerrain,"AlignToTerrain",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_useTerrainColor,"UseTerrainColor",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_bending,"Bending",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_hideable,"Hideable",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_affectedByVoxels,"GrowOnVoxles",functor(*this,&CVegetationObject::OnVarChange) );

  AddVariable( mv_pickable,"Pickable",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_aiRadius,"AIRadius",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_brightness,"Brightness",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_density,"Density",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_hmin,"ElevationMin",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_hmax,"ElevationMax",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_slope_min,"SlopeMin",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_slope_max,"SlopeMax",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_castShadows,"CastShadow",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_recvShadows,"RecvShadow",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_alphaBlend,"AlphaBlend",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_SpriteDistRatio,"SpriteDistRatio",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_MaxViewDistRatio,"MaxViewDistRatio",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_material, "Material", functor(*this,&CVegetationObject::OnMaterialChange),IVariable::DT_MATERIAL );
  AddVariable( mv_UseSprites, "UseSprites", functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_minSpec,"MinSpec",functor(*this,&CVegetationObject::OnVarChange) );

  AddVariable( mv_layerFrozen,"Frozen","Layer_Frozen",functor(*this,&CVegetationObject::OnVarChange) );
	AddVariable( mv_layerWet,"Layer_Wet","Layer_Wet",functor(*this,&CVegetationObject::OnVarChange) );
	//AddVariable( mv_layerCloak,"Layer_Cloak","Layer_Cloak",functor(*this,&CVegetationObject::OnVarChange) );
	//AddVariable( mv_layerBurned,"Layer_Burned","Layer_Burned",functor(*this,&CVegetationObject::OnVarChange) );
}

CVegetationObject::~CVegetationObject()
{
	if (m_statObj)
	{
		m_statObj->Release();
	}
	if (m_id >= 0)
	{
		IStatInstGroup grp;
		GetIEditor()->GetSystem()->GetI3DEngine()->SetStatInstGroup( m_id,grp );
	}
	if (m_pMaterial)
	{
		m_pMaterial = NULL;
	}

}

void CVegetationObject::SetFileName( const CString &strFileName )
{
	if (m_strFileName != strFileName)
	{
		m_strFileName = strFileName;
		UnloadObject();
		LoadObject();
	}
	SetEngineParams();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::SetCategory( const CString &category )
{
	m_category = category;
};

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::UnloadObject()
{
	if (m_statObj)
	{
		m_statObj->Release();
	}
	m_statObj = 0;
	m_objectSize = 1;

	SetEngineParams();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::LoadObject()
{
	m_objectSize = 1;
	if (m_statObj == 0 && !m_strFileName.IsEmpty())
	{
		GetIEditor()->GetErrorReport()->SetCurrentFile( m_strFileName );
		if (m_statObj)
		{
			m_statObj->Release();
			m_statObj = 0;
		}
		m_statObj = GetIEditor()->GetSystem()->GetI3DEngine()->LoadStatObj( m_strFileName, 0, false );
		if (m_statObj)
		{
			m_statObj->AddRef();
			Vec3 min = m_statObj->GetBoxMin();
			Vec3 max = m_statObj->GetBoxMax();
			m_objectSize = __max( max.x-min.x,max.y-min.y );

			Validate( *GetIEditor()->GetErrorReport() );
		}
		GetIEditor()->GetErrorReport()->SetCurrentFile( "" );
	}
	SetEngineParams();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::SetHidden( bool bHidden )
{
	m_bHidden = bHidden;
	SetInUse( !bHidden );

	GetIEditor()->SetModifiedFlag();
	/*
	for (int i = 0; i < GetObjectCount(); i++)
	{
		CVegetationObject *obj = GetObject(i);
		obj->SetInUse( !bHidden );
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::CopyFrom( const CVegetationObject &o )
{
	CopyVariableValues( const_cast<CVegetationObject*>(&o) );

	m_strFileName = o.m_strFileName;
	m_bInUse = o.m_bInUse;
	m_bHidden = o.m_bHidden;
	m_category = o.m_category;

	LoadObject();

	GetIEditor()->SetModifiedFlag();
	SetEngineParams();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::OnVarChange( IVariable *var )
{
	if (m_bVarIgnoreChange)
		return;
	
	SetEngineParams();
	GetIEditor()->SetModifiedFlag();

	if (var == mv_hideable.GetVar() )//|| var == mv_hideableSecondary.GetVar())
	{
		// Reposition this object on vegetation map.
		m_vegetationMap->RepositionObject( this );
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::UpdateMaterial()
{
	CMaterial *pMaterial = NULL;
	CString mtlName = mv_material;
	if (!mtlName.IsEmpty())
		pMaterial = (CMaterial*)GetIEditor()->GetMaterialManager()->LoadMaterial( mtlName );

	if (pMaterial == m_pMaterial)
		return;

	m_pMaterial = pMaterial;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::OnMaterialChange( IVariable *var )
{
	if (m_bVarIgnoreChange)
		return;

	UpdateMaterial();
	SetEngineParams();
	GetIEditor()->SetModifiedFlag();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::SetEngineParams()
{
	I3DEngine *engine = GetIEditor()->GetSystem()->GetI3DEngine();
	if (!engine)
		return;

	IStatInstGroup grp;
	if (m_numInstances > 0 || !m_terrainLayers.empty())
	{
		// Only set object when vegetation actually used in level.
		grp.pStatObj = m_statObj;
	}
	grp.bHideability = mv_hideable==1;
	grp.bHideabilitySecondary = mv_hideable==2;
  grp.bPickable = mv_pickable;
	grp.fBending = mv_bending;
	grp.bCastShadow = mv_castShadows;
	grp.bRecvShadow = mv_recvShadows;
	grp.bPrecShadow = mv_precalcShadows;
	grp.bUseAlphaBlending = mv_alphaBlend;
	grp.fSpriteDistRatio = mv_SpriteDistRatio;
	grp.fShadowDistRatio = mv_ShadowDistRatio;
	grp.fMaxViewDistRatio =mv_MaxViewDistRatio;
	grp.fBrightness = mv_brightness;

	int nMinSpec = mv_minSpec;
	grp.minConfigSpec = (ESystemConfigSpec)nMinSpec;
//	grp.bUpdateShadowEveryFrame = mv_realtimeShadow;
	grp.pMaterial = 0;
  grp.nMaterialLayers = 0;

	if (m_pMaterial)
  {
		grp.pMaterial = m_pMaterial->GetMatInfo();
  }
    
  // Set material layer flags
  uint8 nMaterialLayersFlags = 0;

  // Activate frozen layer
  if( mv_layerFrozen)
  {
    nMaterialLayersFlags |= MTL_LAYER_FROZEN;
    // temporary fix: disable bending atm for material layers
    grp.fBending = 0.0f;
  }
	if (mv_layerWet)
	{
		nMaterialLayersFlags |= MTL_LAYER_WET;
	}
	//if (mv_layerCloak)
	//{
	//	nMaterialLayersFlags |= MTL_LAYER_CLOAK;
	//}
	//if (mv_layerBurned)
	//{
	//	nMaterialLayersFlags |= MTL_LAYER_BURNED;
	//}

  grp.nMaterialLayers = nMaterialLayersFlags;
  grp.bUseSprites = mv_UseSprites;
	grp.bRandomRotation = mv_rotation;
	grp.bUseTerrainColor = mv_useTerrainColor;
	grp.bAlignToTerrain = mv_alignToTerrain;
  grp.bAffectedByVoxels = mv_affectedByVoxels;

	// pass parameters for procedural objects placement
	grp.fDensity										= mv_density;
	grp.fElevationMax								= mv_hmax;
	grp.fElevationMin								= mv_hmin;
	grp.fSize												= mv_size;
	grp.fSizeVar										= mv_sizevar;
	grp.fSlopeMax										= mv_slope_max;
	grp.fSlopeMin										= mv_slope_min;


	if(mv_hideable == 2)
	{
		grp.bHideability = false;
		grp.bHideabilitySecondary = true;
	}
	else	if(mv_hideable == 1)
	{
		grp.bHideability = true;
		grp.bHideabilitySecondary = false;
	}
	else
	{
		grp.bHideability = false;
		grp.bHideabilitySecondary = false;
	}

  engine->SetStatInstGroup( m_id,grp );

  float r = mv_aiRadius;
  if (m_statObj)
    m_vegetationMap->SetAIRadius(m_statObj, r);
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::Serialize( XmlNodeRef &node,bool bLoading )
{
	m_bVarIgnoreChange = true;
	CVarObject::Serialize( node,bLoading );
	m_bVarIgnoreChange = false;
	if (bLoading)
	{
		// Loading
		CString fileName;
		node->getAttr( "FileName",fileName );
		fileName = PathUtil::ToUnixPath( (const char*)fileName ).c_str();
		node->getAttr( "GUID",m_guid );
		node->getAttr( "Hidden",m_bHidden );
		node->getAttr( "Category",m_category );

		m_terrainLayers.clear();
		XmlNodeRef layers = node->findChild( "TerrainLayers" );
		if (layers)
		{
			for (int i = 0; i < layers->getChildCount(); i++)
			{
				CString name;
				XmlNodeRef layer = layers->getChild(i);
				if (layer->getAttr( "Name",name ) && !name.IsEmpty())
					m_terrainLayers.push_back(name);
			}
		}

		SetFileName( fileName );

		UpdateMaterial();
		SetEngineParams();
	}
	else
	{
		// Save.
		node->setAttr( "Id",m_id );
		node->setAttr( "FileName",m_strFileName );
		node->setAttr( "GUID",m_guid );
		node->setAttr( "Hidden",m_bHidden );
		node->setAttr( "Index",m_index );
		node->setAttr( "Category",m_category );

		if (!m_terrainLayers.empty())
		{
			XmlNodeRef layers = node->newChild( "TerrainLayers" );
			for (int i = 0; i < m_terrainLayers.size(); i++)
			{
				XmlNodeRef layer = layers->newChild( "Layer" );
				layer->setAttr( "Name",m_terrainLayers[i] );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::Validate( CErrorReport &report )
{
	if (m_statObj && m_statObj->IsDefaultObject())
	{
		// File Not found.
		CErrorRecord err;
		err.error.Format( "Geometry file %s for Vegetation Object not found",(const char*)m_strFileName );
		err.file = m_strFileName;
		err.severity = CErrorRecord::ESEVERITY_WARNING;
		err.flags = CErrorRecord::FLAG_NOFILE;
		report.ReportError(err);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CVegetationObject::IsUsedOnTerrainLayer( const CString &layer )
{
	return stl::find( m_terrainLayers,layer );
}

//////////////////////////////////////////////////////////////////////////
int CVegetationObject::GetTextureMemUsage( ICrySizer *pSizer )
{
	int nSize = 0;
	if (m_statObj != NULL && m_statObj->GetRenderMesh() != NULL)
	{
		IMaterial *pMtl = (m_pMaterial) ? m_pMaterial->GetMatInfo() : NULL;
		if (!pMtl)
			pMtl = m_statObj->GetMaterial();
		if (pMtl)
			nSize = m_statObj->GetRenderMesh()->GetTextureMemoryUsage( pMtl,pSizer );

	}
	return nSize;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::SetSelected( bool bSelected )
{
	bool bSendEvent = bSelected != m_bSelected;
	m_bSelected = bSelected;
	if (bSendEvent)
	{
		GetIEditor()->Notify( eNotify_OnVegetationObjectSelection );
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::OnConfigSpecChange()
{
	bool bHiddenBySpec = false;
	int nMinSpec = mv_minSpec;

	if (gSettings.bApplyConfigSpecInEditor)
	{
		if (nMinSpec == CONFIG_DETAIL_SPEC && gSettings.editorConfigSpec == CONFIG_LOW_SPEC)
			bHiddenBySpec = true;
		if (nMinSpec != 0 && gSettings.editorConfigSpec != 0 && nMinSpec > gSettings.editorConfigSpec)
			bHiddenBySpec = true;
	}

	// Hide/unhide object depending if it`s needed for this spec.
	if (bHiddenBySpec && !IsHidden())
		m_vegetationMap->HideObject(this,true);
	else if (!bHiddenBySpec && IsHidden())
		m_vegetationMap->HideObject(this,false);
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::SetNumInstances( int numInstances)
{
	if (m_numInstances == 0 && numInstances > 0)
	{
		m_numInstances = numInstances;
		// Object is really used.
		SetEngineParams();
	}
	m_numInstances = numInstances;
}
